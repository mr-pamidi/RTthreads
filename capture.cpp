
//
//  Author: Nagarjuna Pamidi
//
//  File name: utilities.h
//
//  Description: Used for querying and storing the frames from the USB camera
//

#include "capture.hpp"
#include "include.h"
#include "posix_timer.h"
#include "utilities.h"

//global variable //updated once, and used across the application for sync
extern pthread_cond_t cond_query_frames_thread;
extern pthread_cond_t cond_store_frames_thread;
extern pthread_mutex_t app_timer_counter_mutex_lock;
extern unsigned long long app_timer_counter;
extern bool timer_started;
extern unsigned int store_frames_frequency;
extern bool live_camera_view;
extern unsigned int compress_ratio; //default:0 no compression

//cpp namespaces
using namespace cv;
using namespace std;

//capture window title
const char capture_window_title[] = "Project-Trails";

//global capture variables
static CvCapture* grab_frame;
static IplImage* retrieve_frame;
//protect globally shared frame data
static pthread_mutex_t frame_mutex_lock;
static pthread_mutexattr_t frame_mutex_lock_attr;

//synchronization purposes
static int exit_application = FALSE;

//------------------------------------------------------------------------------------------------------------------------------
//  Function Name:  initialize_device_use_openCV
//
//  Parameters:     None
//
//  Return:         None
//
//  Description:    Initializes the ca,era device using openCV
//
//------------------------------------------------------------------------------------------------------------------------------
void initialize_device_use_openCV(void)
{
    //start capturing frames from /dev/video0
    grab_frame = cvCreateCameraCapture(0);
    //set capture properties
    cvSetCaptureProperty(grab_frame, CV_CAP_PROP_FRAME_WIDTH, FRAME_HRES);
    cvSetCaptureProperty(grab_frame, CV_CAP_PROP_FRAME_HEIGHT, FRAME_VRES);
    cvNamedWindow(capture_window_title, CV_WINDOW_AUTOSIZE);

    //grab and retrieve a frame
    retrieve_frame = cvQueryFrame(grab_frame);
    if(!retrieve_frame) EXIT_FAIL("Problem initializing the device");

    //show the recently grabbed frame
    cvShowImage(capture_window_title, retrieve_frame);
    //wait for user key input
    char c = cvWaitKey(33);
    if(c == 'q' || c == 27)
    {
        exit(SUCCESS);
    }

    //convert IplImage type to Mat type
    Mat openCV_store_frames_mat = cvarrToMat(retrieve_frame);

    //paramaters to save .ppm file
    vector<int> ppm_params;
    ppm_params.push_back(CV_IMWRITE_PXM_BINARY);
    ppm_params.push_back(1);

    //try writing a dummy file, and see if the write was successful or not
    try
    {
        imwrite("dump.ppm", openCV_store_frames_mat, ppm_params);
    }
    catch (runtime_error& ex)
    {
        //exit applicaiton if having troubles to save the file
        printf("Exception converting image to PPM format!\n");
        exit(ERROR);
    }

}


//------------------------------------------------------------------------------------------------------------------------------
//  Function Name:  query_frames
//
//  Parameters:     cameraIdx: Not used at the moment
//
//  Return:         None
//
//  Description:    query_frames_thread handler function. Which executes at 30Hz to query frames from the device
//
//------------------------------------------------------------------------------------------------------------------------------
void *query_frames(void *cameraIdx)
{
    int rc;
    int *dev = (int *)cameraIdx;
    static unsigned int frame_counter = 0;

    #ifdef TIME_ANALYSIS
    //RT time analysis
    static struct timespec query_frames_start_time;
    static double query_frames_elapsed_time, query_frames_average_load_time, query_frames_wcet=0;
    static unsigned int missed_deadlines = 0;
    #endif //TIME_ANALYSIS

    //initilize mutex to protect timer count variable
    if(pthread_mutexattr_init(&frame_mutex_lock_attr)) EXIT_FAIL("pthread_mutexattr_init");
    if(pthread_mutexattr_settype(&frame_mutex_lock_attr, PTHREAD_MUTEX_ERRORCHECK)) EXIT_FAIL("pthread_mutexattr_settype");
    if(pthread_mutex_init(&frame_mutex_lock, &frame_mutex_lock_attr)) EXIT_FAIL("pthread_mutex_init");

    while(1)
    {
        //wait for signal from timer
        if(timer_started)
        {
            if(pthread_mutex_lock(&app_timer_counter_mutex_lock)) EXIT_FAIL("pthread_mutex_lock");
            if(pthread_cond_wait(&cond_query_frames_thread, &app_timer_counter_mutex_lock)) EXIT_FAIL("pthread_cond_wait");
            if(pthread_mutex_unlock(&app_timer_counter_mutex_lock)) EXIT_FAIL("pthread_mutex_unlock");
        }
        else
        {
            EXIT_FAIL("Timer not available");
        }

        if(exit_application) break;

        //RT time analysis purposes
        #ifdef TIME_ANALYSIS
        if(clock_gettime(CLOCK_REALTIME, &query_frames_start_time)) EXIT_FAIL("clock_gettime");
        #endif //TIME_ANALYSIS

        //debug purposes
        #ifdef DEBUG_MODE_ON
        syslog(LOG_WARNING," cvQueryframe start at :%lld", app_timer_counter);
        #endif //DEBUG_MODE_ON

        //thread safe //lock frame before updating
        if(pthread_mutex_lock(&frame_mutex_lock)) EXIT_FAIL("pthread_mutex_lock");
        //grab a new frame. Returns a valid int on Success
        if(!(cvGrabFrame(grab_frame))) EXIT_FAIL("cvGrabFrame"); //grab new frame
        //retrieve frame data only if live view is selected. Saving some Milli sec time!!
        if(live_camera_view)
        {
            retrieve_frame = cvRetrieveFrame(grab_frame);
            //if there is not valid data, exit application
            if(!retrieve_frame) break;
        }

        if(pthread_mutex_unlock(&frame_mutex_lock)) EXIT_FAIL("pthread_mutex_unlock");

        #ifdef DEBUG_MODE_ON
        syslog(LOG_WARNING," cvQueryframe done at :%lld", app_timer_counter);
        #endif //DEBUG_MODE_ON

        //show frames in real time
        if(live_camera_view)
        {
            //show recently retrieved frame and wait for user key input
            cvShowImage(capture_window_title, retrieve_frame);
            char c = cvWaitKey(1);
            if( c == 'q' || c == 27) break;
        }

        #ifdef DEBUG_MODE_ON
        syslog(LOG_WARNING," cvShowImage done at :%lld", app_timer_counter);
        #endif //DEBUG_MODE_ON

        ++frame_counter;

        #ifdef TIME_ANALYSIS
        //measure elapsed time
        query_frames_elapsed_time = elapsed_time_in_msec(&query_frames_start_time);

        //measure WCET
        if(query_frames_elapsed_time > query_frames_wcet)
        {
            query_frames_wcet = query_frames_elapsed_time;
        }

        //measure avrage load time
        query_frames_average_load_time += query_frames_elapsed_time;

        //keep track of missed deadlines
        if(query_frames_elapsed_time > QUERY_FRAMES_INTERVAL_IN_MSEC)
        {
            ++missed_deadlines; //tbd: add syslog with time when missed deadline
        }
        #endif //TIME_ANALYSIS

    }

    //stop capturing and destroy the frame view window
    cvReleaseCapture(&grab_frame);
    cvDestroyWindow(capture_window_title);

    //destroy mutex lock!
    pthread_mutex_destroy(&frame_mutex_lock);

    #ifdef TIME_ANALYSIS
    //validate for division by Zero
    if(frame_counter)
    {
        query_frames_average_load_time /= frame_counter;
    }
    syslog(LOG_WARNING, " query_frames_thread execuiton results, frames:%d, WCET:%lf, Average:%lf, Missed Deadlines:%d", frame_counter, query_frames_wcet, query_frames_average_load_time, missed_deadlines);
    #endif //TIME_ANALYSIS

    #ifdef DEBUG_MODE_ON
    syslog(LOG_WARNING," query_frames_thread exiting...");
    #endif //DEBUG_MODE_ON

    //set this bit to let other threads know!
    exit_application = TRUE;
}

//------------------------------------------------------------------------------------------------------------------------------
//  Function Name:  store_frames
//
//  Parameters:     params: Not used at the moment
//
//  Return:         None
//
//  Description:    store_frames_thread handler function. Which stores frames at user defined frequency rate (1 Hz to 10 Hz)
//
//------------------------------------------------------------------------------------------------------------------------------
void *store_frames(void *params)
{

    int rc;

    #ifdef TIME_ANALYSIS
    //time analysis
    static struct timespec store_frames_start_time;
    static double store_frames_elapsed_time, store_frames_average_load_time, store_frames_wcet=0;
    static unsigned int missed_deadlines = 0;
    #endif //TIME_ANALYSIS

    static unsigned int frame_counter=0;

    //.ppm file name variable
    static struct timeval frame_timestamp;
    static char ppm_file_name[20] = {};
    static char ppm_header1[64] = "";
    static char ppm_header2[] = "\n# TARGET: Linux tegra-ubuntu 4.4.38-tegra #1 SMP PREEMPT Thu May 17 00:15:19 PDT 2018 aarch64 aarch64 aarch64 GNU/Linux";
    static int ppm_fd, ppm_file_size, dump_fd;
    static const unsigned int frame_data_size = 0xff;
    static char buffer[frame_data_size] = {};

    //openCV supported Mat class data structure
    Mat openCV_store_frames_mat;

    //parameters to save the frame as compressed .png file
    vector<int> compress_params;
    compress_params.push_back(CV_IMWRITE_PXM_BINARY);
    compress_params.push_back(compress_ratio); //user selectable compression ration

    //parameters to save the frame as .ppm file
    vector<int> ppm_params;
    ppm_params.push_back(CV_IMWRITE_PXM_BINARY);
    ppm_params.push_back(1);

    //loop forever, until user enters 'q' or 'Esc'
    while(1)
    {
        if(timer_started)
        {
            //wait for signal from timer...
            if(pthread_mutex_lock(&app_timer_counter_mutex_lock)) EXIT_FAIL("pthread_mutex_lock");
            if(pthread_cond_wait(&cond_store_frames_thread, &app_timer_counter_mutex_lock)) EXIT_FAIL("pthread_cond_wait");
            if(pthread_mutex_unlock(&app_timer_counter_mutex_lock)) EXIT_FAIL("pthread_mutex_unlock");
        }
        else
        {
            EXIT_FAIL("Timer not available!");
        }

        if(exit_application) break;

        //log for RT time analysis
        #ifdef TIME_ANALYSIS
        if(clock_gettime(CLOCK_REALTIME, &store_frames_start_time)) EXIT_FAIL("clock_gettime");
        #endif //TIME_ANALYSIS

        //log for debugging purposes
        #ifdef DEBUG_MODE_ON
        syslog(LOG_WARNING, " store_frames start write at:%lld", app_timer_counter);
        #endif //DEBUG_MODE_ON

        //make sure other threads are not updating frames at this moment
        if(pthread_mutex_lock(&frame_mutex_lock)) EXIT_FAIL("pthread_mutex_lock");
        //get timestamp
        gettimeofday(&frame_timestamp, NULL);
        //if this bit is set, most recent frame is already retrieved by the query_frames_thread
        if(!live_camera_view)
        {
            retrieve_frame = cvRetrieveFrame(grab_frame);
        }
        //convert IplImage type to Mat type
        openCV_store_frames_mat = cvarrToMat(retrieve_frame);
        if(pthread_mutex_unlock(&frame_mutex_lock)) EXIT_FAIL("pthread_mutex_unlock");

        #ifdef DEBUG_MODE_ON
        syslog(LOG_WARNING, " store_frames unlocked frame_mutex at %lld", app_timer_counter);
        #endif

        if(compress_ratio)
        {
            //dump frames as png
            try
            {
                imwrite("dump.png", openCV_store_frames_mat, compress_params);
            }
            //catch any exceptions, and exit the application if there are any issue while storing the .ppm file
            catch(runtime_error& ex)
            {
                printf("Exception converting image to PPM format!\n");
                exit(ERROR);
            }

            //convert .png to .ppm
            openCV_store_frames_mat = imread("dump.png", CV_LOAD_IMAGE_COLOR);
        }

        //dump frames as ppm
        try
        {
            imwrite("dump.ppm", openCV_store_frames_mat, ppm_params);
        }
        //catch any exceptions, and exit the application if there are any issue while storing the .ppm file
        catch(runtime_error& ex)
        {
            printf("Exception converting image to PPM format!\n");
            exit(ERROR);
        }

        //.ppm file name
        sprintf(ppm_file_name, "alpha%d.ppm", frame_counter);
        //apend ppm header
        ppm_fd = open(ppm_file_name, O_RDWR | O_NONBLOCK | O_CREAT, 00666);
        dump_fd = open("dump.ppm", O_RDONLY | O_NONBLOCK | O_CREAT, 00666);

        //read first line of the file which specifies the format P6
        if(read(dump_fd, buffer, 2))
        {
            write(ppm_fd, buffer, 2);
        }
        else
        {
            EXIT_FAIL("Error opening dump.ppm file!");
        }

        //append headers to the .ppm file
        CLEAR_MEMORY(ppm_header1); //remove previous header data
        //write time-stamp to header string
        sprintf(ppm_header1, "\n#Frame %d captured at %ld:%ld", frame_counter, frame_timestamp.tv_sec, frame_timestamp.tv_usec);
        write(ppm_fd, ppm_header1, strlen(ppm_header1));
        write(ppm_fd, ppm_header2, strlen(ppm_header2));

        //read dump.ppm file contents
        while(read(dump_fd, buffer, frame_data_size))
        {
            //write data
            write(ppm_fd, buffer, frame_data_size);
            CLEAR_MEMORY(buffer);
        }
        //write last few bytes before the EOF
        write(ppm_fd, buffer, frame_data_size);
        //close files
        close(ppm_fd);
        close(dump_fd);

        //if this bit is set, most recent frames are already being displayed by query_frames_thread
        if(!live_camera_view)
        {
            //show image and wait for 1ms to receive user input
            cvShowImage(capture_window_title, retrieve_frame);
            char c = cvWaitKey(1);
            if( c == 'q' || c == 27) break;
        }

        ++frame_counter;

        #ifdef DEBUG_MODE_ON
        syslog(LOG_WARNING, " store_frames end of write at:%lld", app_timer_counter);
        #endif //DEBUG_MODE_ON

        #ifdef TIME_ANALYSIS
        //measure elapsed time
        store_frames_elapsed_time = elapsed_time_in_msec(&store_frames_start_time);

        //measure WCET
        if(store_frames_elapsed_time > store_frames_wcet)
        {
            store_frames_wcet = store_frames_elapsed_time;
        }

        //measure average run time
        store_frames_average_load_time += store_frames_elapsed_time;

        //keep track of number of missed deadlines
        if(store_frames_elapsed_time > (DEFAULT_STORE_FRAMES_INTERVAL_IN_MSEC/store_frames_frequency))
        {
            ++missed_deadlines;
        }
        #endif //TIME_ANALYSIS

    }

    #ifdef TIME_ANALYSIS
    //do not divide by Zero
    if(frame_counter)
    {
        store_frames_average_load_time /= frame_counter;
    }
    syslog(LOG_WARNING, " store_frames_thread execuiton results, frames:%d, WCET:%lf, Average:%lf, Missed Deadlines:%d", frame_counter, store_frames_wcet, store_frames_average_load_time, missed_deadlines);
    #endif //TIME_ANALYSIS

    #ifdef DEBUG_MODE_ON
    syslog(LOG_WARNING," store_frames_thread exiting...");
    #endif //DEBUG_MODE_ON

    exit_application = TRUE;

}

//==============================================================================
//    End of file!
//==============================================================================
