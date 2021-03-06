//
//  Author: Nagarjuna Pamidi
//
//  File name: main.c
//
//  Description: main() function, manages RT Threads
//

#include "capture.hpp"
#include "include.h"
#include "posix_timer.h"
#include "utilities.h"
#include "v4l2_capture.h"

//function prototyping
void *rt_thread_dispatcher_handler(void *args);
static void usage(FILE *fp, int argc, char **argv);

// /dev/videoX name
char *device_name="/dev/video0";

//global time variable mutex to restrict access to it
pthread_mutex_t app_timer_counter_mutex_lock;
pthread_mutexattr_t app_timer_counter_mutex_lock_attr;

//global variable //updated once, and used across the application for sync
bool query_frames_thread_dispatched = false;
bool store_frames_thread_dispatched = false;
bool timer_started = false;
unsigned int store_frames_frequency = 1; //default value 1
bool live_camera_view = false;
unsigned int compress_ratio = 0; //default: no compression
unsigned int max_no_of_frames_allowed = 100;


//------------------------------------------------------------------------------
//  Function Name:  main
//
//  Parameters:     Command-line args. Enter "./main -h" for options
//
//  Return:         Fail/Success
//
//  Description:    Parse command line arguments.
//                  Initialize syslogs
//                  Create rt_thread_dispatcher thread, and wait for it to exit!
//
//------------------------------------------------------------------------------
int main( int argc, char** argv )
{

    //parse user options
    while(1)
    {
        int idx;
        int user_input_option;

        user_input_option = getopt(argc, argv, "c:d:f:hl:n:");

        if (user_input_option == -1) break; //exit forever loop

        switch (user_input_option)
        {
            case 'c':
            compress_ratio = atoi(optarg);
            //validate user input
            if(compress_ratio<0)
            {
                compress_ratio = 0; //not supporting less than 0
                fprintf(stdout, "Resetting compression ratio to 0 (Min allowed)! \n");
            }
            else if(compress_ratio > 9)
            {
                compress_ratio = 9; //not supporting more than 9
                fprintf(stdout, "Resetting compression ratio to 9 (Max allowed)! \n");
            }
            //ignoring device name changes for now
            break;

            case 'd':
            //ignoring device name changes for now
            break;

            case 'f':
            store_frames_frequency = atoi(optarg);
            //validate the frequency parameter
            if(store_frames_frequency < 1)
            {
                store_frames_frequency = 1; //reset to one
                fprintf(stdout, "Resetting frequency to save the frames to 1 Hz (Min allowed)! \n");
            }
            else if(store_frames_frequency > 10)
            {
                store_frames_frequency = 10; //not suppporting more than 10Hz
                fprintf(stdout, "Resetting frequency to save the frames to 10 Hz (Max allowed)! \n");
            }
            break;

            case 'h':
            usage(stdout, argc, argv);
            return(SUCCESS);

            case 'l':
            live_camera_view = (bool)atoi(optarg);
            break;

            case 'n':
            max_no_of_frames_allowed = atoi(optarg);
            //boundary checks
            if(max_no_of_frames_allowed < 0)
            {
                max_no_of_frames_allowed = 1;
                fprintf(stdout, "Resetting no.of frames collecting to 1 (Min allowed)!\n");
            }
            else if(max_no_of_frames_allowed > 6000)
            {
                max_no_of_frames_allowed = 6000;
                fprintf(stdout, "Resetting no.of frames collecting to 6000 (Max allowed)!\n");
            }
            break;

            default:
            usage(stderr, argc, argv);
            exit(EXIT_FAILURE);
            break;
        }//end of switch(user_input_option)
    }//end of while(1)

    int rc = 0;

    //syslogs
    initialize_syslogs();

    pthread_t rt_thread_dispatcher;
    pthread_attr_t rt_thread_dispatcher_sched_attr;
    struct sched_param rt_thread_dispatcher_sched_param;

    assign_RT_schedular_attr(&rt_thread_dispatcher_sched_attr, &rt_thread_dispatcher_sched_param, SCHED_FIFO, SCHED_FIFO_MAX_PRIORITY, JETSON_TX2_ARM_CORE2);

    syslog(LOG_WARNING, "RT dispatcher thread dispatching with priority ==> %d <==", rt_thread_dispatcher_sched_param.sched_priority);
    rc = pthread_create(&rt_thread_dispatcher, &rt_thread_dispatcher_sched_attr, rt_thread_dispatcher_handler, (void *)0 );
    if(rc)
    {
        EXIT_FAIL("pthread_create");
    }

    //wait for main thread to finish execution
    pthread_join(rt_thread_dispatcher, NULL);

    //syslog(LOG_WARNING, "End of user log!");
    closelog();

    //print to terminal
    fprintf(stdout, "\n\nexiting Time-Lapse Image Acquisition application..!\nSee you soon :)\n\n");
    return 0;
} //end of main()


//------------------------------------------------------------------------------
//  Function Name:  rt_thread_dispatcher_handler
//
//  Parameters:     args - not used
//
//  Return:         None
//
//  Description:    Initialize and start POSIX timer
//                  Dispatch RT thtreads to query and store frames
//
//------------------------------------------------------------------------------
void *rt_thread_dispatcher_handler(void *args)
{
    int rc;
    pthread_t query_frames_thread, store_frames_thread;
    pthread_attr_t query_frames_thread_attr, store_frames_thread_attr;
    threadParams_t query_frames_threadIdx, store_frames_threadIdx;
    struct sched_param query_frames_thread_sched_param, store_frames_thread_sched_param;

    //posix timer parameters
    struct sigevent sigevent_param;
    timer_t timer_id;
    struct itimerspec timer_period;
    pthread_attr_t timer_thread_attr;
    struct sched_param timer_thread_sched_param;

    //assign thread indexes to keep track
    query_frames_threadIdx.threadIdx = QUERY_FRAMES_THREAD_IDX;
    store_frames_threadIdx.threadIdx = STORE_FRAMES_THREAD_IDX;

    //assign RT scheduler attributes
    assign_RT_schedular_attr(&query_frames_thread_attr, &query_frames_thread_sched_param, SCHED_FIFO, QUERY_FRAMES_THREAD_PRIORITY, JETSON_TX2_ARM_CORE2);
    assign_RT_schedular_attr(&store_frames_thread_attr, &store_frames_thread_sched_param, SCHED_FIFO, STORE_FRAMES_THREAD_PRIORITY, JETSON_TX2_ARM_CORE2);

    #ifdef DEBUG_MODE_ON
    syslog_scheduler();
    #endif //DEBUG_MODE_ON

    //initialize timer thread attributes
    assign_RT_schedular_attr(&timer_thread_attr, &timer_thread_sched_param, SCHED_FIFO, SCHED_FIFO_MAX_PRIORITY, JETSON_TX2_ARM_CORE2);

    //assign sigevent paramaters for the timer
    sigevent_param.sigev_notify = SIGEV_THREAD;
    sigevent_param.sigev_value.sival_ptr = &timer_id;
    sigevent_param.sigev_notify_function = &timer_handler;
    sigevent_param.sigev_notify_attributes = &timer_thread_attr;

    //initialize timer period values
    timer_period.it_interval.tv_sec = (APP_TIMER_INTERVAL_IN_MSEC / MSEC_PER_SEC);
    timer_period.it_interval.tv_nsec = (APP_TIMER_INTERVAL_IN_MSEC * NSEC_PER_MSEC);
    timer_period.it_value.tv_sec = timer_period.it_interval.tv_sec; //start time
    timer_period.it_value.tv_nsec = timer_period.it_interval.tv_nsec;

    //create timer
    if(timer_create(CLOCK_REALTIME, &sigevent_param, &timer_id)) EXIT_FAIL("timer_create");

    //initialize app_timer_counter variable mutex attributes
    if(pthread_mutexattr_init(&app_timer_counter_mutex_lock_attr)) EXIT_FAIL("pthread_mutexattr_init");
    //set mutex type to PTHREAD_MUTEX_ERRORCHECK
    if(pthread_mutexattr_settype(&app_timer_counter_mutex_lock_attr, PTHREAD_MUTEX_ERRORCHECK)) EXIT_FAIL("pthread_mutexattr_settype");
    //initilize mutex to protect app_timer_counter variable
    if(pthread_mutex_init(&app_timer_counter_mutex_lock, &app_timer_counter_mutex_lock_attr)) EXIT_FAIL("pthread_mutex_init");

    //start timer
    syslog(LOG_WARNING,"\n Timer starting with timer_thread_attr priority ==> %d <==", timer_thread_sched_param.sched_priority);
    if(timer_settime(timer_id, 0, &timer_period, 0)) EXIT_FAIL("timer_settime");
    timer_started = true;

    //using openCV APIs to qccquire individual frames from the camera
    //initialize, start querying frames, and save a sample frame, to make sure device is working..!
    initialize_device_use_openCV();

    //create query_frames_thread
    syslog(LOG_WARNING,"\n query_frames_thread dispatching with priority ==> %d <==", query_frames_thread_sched_param.sched_priority);
    query_frames_thread_dispatched = true;
    rc = pthread_create(&query_frames_thread, &query_frames_thread_attr, query_frames, (void *)&query_frames_threadIdx);
    if(rc)
    {
        EXIT_FAIL("pthread_create");
    }

    //create store_frames_thread
    syslog(LOG_WARNING,"\n store_frames_thread dispatching with priority ==> %d <==", store_frames_thread_sched_param.sched_priority);
    store_frames_thread_dispatched = true;
    rc = pthread_create(&store_frames_thread, &store_frames_thread_attr, store_frames, (void *)&store_frames_threadIdx);
    if(rc)
    {
        EXIT_FAIL("pthread_create");
    }

    //wait fot query_frames_thread to exit
    pthread_join(query_frames_thread, NULL);
    //wait for store_frames_thread to exit
    pthread_join(store_frames_thread, NULL);

    //stop timer
    timer_period.it_interval.tv_sec = 0;
    timer_period.it_interval.tv_nsec = 0;
    if(timer_settime(timer_id, 0, &timer_period, 0)) EXIT_FAIL("timer_settime");
    timer_started = false;

    //destroy mutex lock
    pthread_mutex_destroy(&app_timer_counter_mutex_lock);
    //add a log
    syslog(LOG_WARNING," rt_thread_dispatcher exiting...");
    //exit thread
    pthread_exit(NULL);
} //end of "rt_thread_dispatcher_handler()""


//------------------------------------------------------------------------------
//  Function Name:  rt_thread_dispatcher_handler
//
//  Parameters:     args - not used
//
//  Return:         None
//
//  Description:    Initialize and start POSIX timer
//                  Dispatch RT thtreads to query and store frames
//
//------------------------------------------------------------------------------
static void usage(FILE *fp, int argc, char **argv)
{
    fprintf(fp,
             "\nUsage: %s [options]\n\n"
             "Options:\n"
             "\t-c    Compression ratio \n\t\t[Min: 0, Max: 9, Default :0]\n\n"
             "\t-d    Video device name \n\t\t[default: '/dev/video0']\n\n"
             "\t-f    Select frequency to save frames \n\t\t[Min: 1 Hz, Max: 10 Hz, Default: 1 Hz]\n\n"
             "\t-h    Print this message\n\n"
			 "\t-l    Live camera view \n\t\t[default: false]\n\n"
             "\t-n    Number of frames to collect \n\t\t[Min: 1, Max: 6000, Default: 100]\n\n",
             argv[0]);
}

//==============================================================================
//    End of file!
//==============================================================================
