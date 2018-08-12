//
//  Author: Nagarjuna Pamidi
//
//  File name: posix_timer.c
//
//  Description: Timer functionalities
//

#include "include.h"
#include "posix_timer.h"

//global timer variable
unsigned long long app_timer_counter = 1;

//global time variable mutex to restrict access to it
extern pthread_mutex_t app_timer_counter_mutex_lock;
extern pthread_mutexattr_t app_timer_counter_mutex_lock_attr;

//cond wait/signal for synchronization
pthread_cond_t cond_query_frames_thread = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_store_frames_thread = PTHREAD_COND_INITIALIZER;

//global variable //updated by only once, and used across the application for sync
extern bool use_v4l2_libs;
extern bool query_frames_thread_dispatched;
extern bool store_frames_thread_dispatched;
extern unsigned int store_frames_frequency; //default value 1

//------------------------------------------------------------------------------------------------------------------------------
//  Function Name:  timer_handler.c
//
//  Parameters:     arg - not used
//
//  Return:         None
//
//  Description:    Initialize and start POSIX timer
//                  Dispatch RT thtreads to query and store frames
//
//---------------------------------------------------------------------------------------------------------------------------
void timer_handler(union sigval arg)
{
    #ifdef TIMER_TIME_ANALYSIS
    static struct timespec timer_start_time;
    static double timer_wcet=0;
    #endif

    //accquire mutex lock on timer counter variable
    if(pthread_mutex_lock(&app_timer_counter_mutex_lock)) EXIT_FAIL("pthread_mutex_lock");

    #ifdef TIMER_TIME_ANALYSIS
    clock_gettime(CLOCK_REALTIME, &timer_start_time);
    #endif
    //update timer counter
    app_timer_counter += APP_TIMER_INTERVAL_IN_MSEC;

    //use v4l2 library APIs
    if(use_v4l2_libs)
    {

    }

    //use openCV APIs
    else
    {
        //run at 20Hz
        if((query_frames_thread_dispatched) && ((app_timer_counter % QUERY_FRAMES_INTERVAL_IN_MSEC) == 0))
        {
            //signal  query_frames_thread
            if(pthread_cond_signal(&cond_query_frames_thread)) EXIT_FAIL("pthread_cond_signal");
        }

        //run at variable frequency from 1 Hz to 10 Hz
        if((store_frames_thread_dispatched) && ((app_timer_counter % (DEFAULT_STORE_FRAMES_INTERVAL_IN_MSEC/store_frames_frequency)) == 0))
        {
            //signal store_frames_thread
            if(pthread_cond_signal(&cond_store_frames_thread)) EXIT_FAIL("pthread_cond_signal");
        }

    }

    //relinquish mutex lock on timer counter variable
    if(pthread_mutex_unlock(&app_timer_counter_mutex_lock)) EXIT_FAIL("pthread_mutex_unlock");

    #ifdef TIMER_TIME_ANALYSIS
    if(timer_wcet < elapsed_time_in_msec(&timer_start_time))
    {
        timer_wcet = elapsed_time_in_msec(&timer_start_time);
        syslog(LOG_WARNING, " =====> Timer WCET:%lf", timer_wcet);
    }
    #endif
} //end of "timer_handler()"

//==============================================================================
//    End of file!
//==============================================================================
