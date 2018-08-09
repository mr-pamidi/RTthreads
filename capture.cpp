
//
//  Author: Nagarjuna Pamidi
//
//  File name: utilities.h
//
//  Description: Used for querying and storing the frames from the USB camera
//

#include "capture.hpp"
#include "include.h"
#include "utilities.h"

//global variables
extern pthread_cond_t cond_query_frames_thread;
extern pthread_cond_t cond_store_frames_thread;
extern pthread_mutex_t app_timer_counter_mutex_lock;
extern unsigned long long app_timer_counter;
extern bool timer_started;
//global variables to keep track of missed deadlines
extern bool query_frames_thread_checked_in;
extern bool store_frames_thread_checked_in;
//mutexes to protect
extern pthread_mutex_t qft_checked_in_flag_mutex; //qft = query_frames_thread
extern pthread_mutex_t sft_checked_in_flag_mutex; //stf = store_frames_thread

using namespace cv;
using namespace std;

//capture window title
const char capture_window_title[] = "Project-Trails";

//global capture variables
CvCapture* capture;
IplImage* frame;
pthread_mutex_t frame_mutex_lock;
pthread_mutexattr_t frame_mutex_lock_attr;

int exit_application = FALSE;

void initialize_device_use_openCV(void)
{
    capture = cvCreateCameraCapture(0);
    cvSetCaptureProperty(capture, CV_CAP_PROP_FRAME_WIDTH, FRAME_HRES);
    cvSetCaptureProperty(capture, CV_CAP_PROP_FRAME_HEIGHT, FRAME_VRES);
    cvNamedWindow(capture_window_title, CV_WINDOW_AUTOSIZE);

    frame = cvQueryFrame(capture);
    if(!frame) EXIT_FAIL("Problem initializing the device");

    cvShowImage(capture_window_title, frame);

    char c = cvWaitKey(33);

    Mat mat = cvarrToMat(frame);

    vector<int> compression_params;
    compression_params.push_back(CV_IMWRITE_PXM_BINARY);
    compression_params.push_back(1);

    try
    {
        imwrite("dummy.ppm", mat, compression_params);
    }
    catch (runtime_error& ex)
    {
        printf("Exception converting image to PPM format!\n");
        exit(ERROR);
    }

}


//query_frames_thread must start before store_frames_thread
void *query_frames(void *cameraIdx)
{
    int rc;
    int *dev = (int *)cameraIdx;
    unsigned int frame_counter;

    #ifdef TIME_ANALYSIS
    //time analysis
    struct timespec query_frames_start_time;
    double query_frames_elapsed_time, query_frames_average_load_time, query_frames_wcet=0;
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

        #ifdef TIME_ANALYSIS
        if(clock_gettime(CLOCK_REALTIME, &query_frames_start_time)) EXIT_FAIL("clock_gettime");
        #endif //TIME_ANALYSIS

        #ifdef DEBUG_MODE_ON
        syslog(LOG_WARNING," cvQueryframe start at :%lld", app_timer_counter);
        #endif //DEBUG_MODE_ON

        //thread safe //lock frame before updating
        if(pthread_mutex_lock(&frame_mutex_lock)) EXIT_FAIL("pthread_mutex_lock");
        frame = cvQueryFrame(capture); //capture new frame
        if(pthread_mutex_unlock(&frame_mutex_lock)) EXIT_FAIL("pthread_mutex_unlock");
        if(!frame) break;

        #ifdef DEBUG_MODE_ON
        syslog(LOG_WARNING," cvQueryframe done at :%lld", app_timer_counter);
        #endif //DEBUG_MODE_ON

        cvShowImage(capture_window_title, frame);
        char c = cvWaitKey(1);
        if( c == 'q') break;

        #ifdef DEBUG_MODE_ON
        syslog(LOG_WARNING," cvShowImage done at :%lld", app_timer_counter);
        #endif //DEBUG_MODE_ON

        ++frame_counter;

        #ifdef TIME_ANALYSIS
        query_frames_elapsed_time = elapsed_time_in_msec(&query_frames_start_time);

        if(query_frames_elapsed_time > query_frames_wcet)
        {
            query_frames_wcet = query_frames_elapsed_time;
        }

        query_frames_average_load_time += query_frames_elapsed_time;
        #endif //TIME_ANALYSIS

        if(query_frames_elapsed_time > QUERY_FRAMES_INTERVAL_IN_MSEC)
        {
            static unsigned int missed_deadlines++;
        }
        //set checkin flag
        //if(pthread_mutex_lock(&sft_checked_in_flag_mutex)) EXIT_FAIL("pthread_mutex_lock");
        //store_frames_thread_checked_in = true;
        //if(pthread_mutex_unlock(&sft_checked_in_flag_mutex)) EXIT_FAIL("pthread_mutex_lock");
    }

    cvReleaseCapture(&capture);
    cvDestroyWindow(capture_window_title);

    //destroy mutex lock!
    pthread_mutex_destroy(&frame_mutex_lock);

    #ifdef TIME_ANALYSIS
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


void *store_frames(void *params)
{

    int rc;

    #ifdef TIME_ANALYSIS
    //time analysis
    struct timespec store_frames_start_time;
    double store_frames_elapsed_time, store_frames_average_load_time, store_frames_wcet=0;
    #endif //TIME_ANALYSIS

    vector<int> compression_params;
    compression_params.push_back(CV_IMWRITE_PXM_BINARY);
    compression_params.push_back(1);
    unsigned int frame_counter=0;

    char ppm_file_name[20] ={};

    Mat mat;

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

        #ifdef TIME_ANALYSIS
        if(clock_gettime(CLOCK_REALTIME, &store_frames_start_time)) EXIT_FAIL("clock_gettime");
        #endif //TIME_ANALYSIS

        #ifdef DEBUG_MODE_ON
        syslog(LOG_WARNING, " store_frames start write at:%lld", app_timer_counter);
        #endif //DEBUG_MODE_ON

        //make sure other threads are not updating frames at this moment
        if(pthread_mutex_lock(&frame_mutex_lock)) EXIT_FAIL("pthread_mutex_lock");
        mat = cvarrToMat(frame);
        if(pthread_mutex_unlock(&frame_mutex_lock)) EXIT_FAIL("pthread_mutex_unlock");

        #ifdef DEBUG_MODE_ON
        syslog(LOG_WARNING, " store_frames unlocked frame_mutex at %lld", app_timer_counter);
        #endif

        sprintf(ppm_file_name, "alpha%d.ppm", frame_counter);

           try
           {
            imwrite(ppm_file_name, mat, compression_params);
           }
           catch (runtime_error& ex)
           {
            printf("Exception converting image to PPM format!\n");
            exit(ERROR);
           }

           ++frame_counter;

           #ifdef DEBUG_MODE_ON
        syslog(LOG_WARNING, " store_frames end of write at:%lld", app_timer_counter);
           #endif //DEBUG_MODE_ON

        #ifdef TIME_ANALYSIS
        store_frames_elapsed_time = elapsed_time_in_msec(&store_frames_start_time);

        if(store_frames_elapsed_time > store_frames_wcet)
        {
            store_frames_wcet = store_frames_elapsed_time;
        }

        store_frames_average_load_time += store_frames_elapsed_time;
        #endif //TIME_ANALYSIS

        if(store_frames_elapsed_time > DEFAULT_STORE_FRAMES_INTERVAL_IN_MSEC)
        {
            static unsigned int missed_deadlines++;
        }
        //set checkin flag
        //if(pthread_mutex_lock(&sft_checked_in_flag_mutex)) EXIT_FAIL("pthread_mutex_lock");
        //store_frames_thread_checked_in = true;
        //if(pthread_mutex_unlock(&sft_checked_in_flag_mutex)) EXIT_FAIL("pthread_mutex_lock");
    }

    #ifdef TIME_ANALYSIS
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
