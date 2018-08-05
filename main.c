//
//  Author: Nagarjuna Pamidi
//
//  File name: main.c
//
//  Description: Manages RT Threads and timer
//

#include "include.h"
#include "utilities.h"
#include "capture.hpp"

//global time variable, and a mutex to restrict access to it
unsigned long long app_timer_counter = 0;
pthread_mutex_t app_timer_counter_mutex_lock;
pthread_mutexattr_t app_timer_counter_mutex_lock_attr;

pthread_cond_t cond_query_frames_thread = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_store_frames_thread = PTHREAD_COND_INITIALIZER;

//function prototyping
void *rt_thread_dispatcher_handler(void *something);
void timer_handler(union sigval arg);

//**********************************
//  Funcation Name:     main
//**********************************
int main( int argc, char** argv )
{
	int rc = 0;
	pthread_t rt_thread_dispatcher;
    pthread_attr_t rt_thread_dispatcher_sched_attr;
    struct sched_param rt_thread_dispatcher_sched_param;

    //syslogs
    initialize_syslogs();

    assign_RT_schedular_attr(&rt_thread_dispatcher_sched_attr, &rt_thread_dispatcher_sched_param, SCHED_FIFO, RT_THREAD_DISPATCHER_PRIORITY, (jetson_tx2_cores)JETSON_TX2_ARM_CORE2);

    syslog(LOG_WARNING, "RT dispatcher thread dispatching with priority ==> %d <==", rt_thread_dispatcher_sched_param.sched_priority);
    rc = pthread_create(&rt_thread_dispatcher, &rt_thread_dispatcher_sched_attr, rt_thread_dispatcher_handler, (void *)0 );
	assert (rc == SUCCESS);

    //wait for main thread to finish execution
    pthread_join(rt_thread_dispatcher, NULL);

    //syslog(LOG_WARNING, "End of user log!");
    closelog();

	//print to terminal
    printf("Exiting..!\n");
    return 0;
}


//*****************************************
//  Funcation Name:     rt_thread_dispatcher_handler
//*****************************************
void *rt_thread_dispatcher_handler(void *something)
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
	assign_RT_schedular_attr(&query_frames_thread_attr, &query_frames_thread_sched_param, SCHED_FIFO, QUERY_FRAMES_THREAD_PRIORITY, (jetson_tx2_cores)JETSON_TX2_ARM_CORE2);
	assign_RT_schedular_attr(&store_frames_thread_attr, &store_frames_thread_sched_param, SCHED_FIFO, STORE_FRAMES_THREAD_PRIORITY, (jetson_tx2_cores)JETSON_TX2_ARM_CORE2);

	#ifdef DEBUG_MODE_ON
    syslog_scheduler();
	#endif //DEBUG_MODE_ON

	//initialize timer thread attributes
    assign_RT_schedular_attr(&timer_thread_attr, &timer_thread_sched_param, SCHED_FIFO, SCHED_FIFO_MAX_PRIORITY, (jetson_tx2_cores)JETSON_TX2_ARM_CORE2);

	//assign sigevent paramaters for the timer
    sigevent_param.sigev_notify = SIGEV_THREAD;
    sigevent_param.sigev_value.sival_ptr = &timer_id;
    sigevent_param.sigev_notify_function = &timer_handler;
    sigevent_param.sigev_notify_attributes = &timer_thread_attr;

	//initialize timer period values
    timer_period.it_interval.tv_sec = (APP_TIMER_INTERVAL_IN_MSEC / MSEC_PER_SEC);
    timer_period.it_interval.tv_nsec = (APP_TIMER_INTERVAL_IN_MSEC * NSEC_PER_MSEC);
    timer_period.it_value.tv_sec = timer_period.it_interval.tv_sec + 2; //2 seconds delay before start
    timer_period.it_value.tv_nsec = timer_period.it_interval.tv_nsec;

	//create timer
    rc = timer_create(CLOCK_REALTIME, &sigevent_param, &timer_id);
    assert (rc == SUCCESS);


	//initialize app_timer_counter variable mutex attributes
	rc = pthread_mutexattr_init(&app_timer_counter_mutex_lock_attr);
	assert(rc == SUCCESS);
	//set mutex type to PTHREAD_MUTEX_ERRORCHECK
	rc = pthread_mutexattr_settype(&app_timer_counter_mutex_lock_attr, PTHREAD_MUTEX_ERRORCHECK);
	assert(rc == SUCCESS);
	//initilize mutex to protect app_timer_counter variable
    rc = pthread_mutex_init(&app_timer_counter_mutex_lock, &app_timer_counter_mutex_lock_attr);
    assert(rc == SUCCESS);

	//start timer
	syslog(LOG_WARNING,"\n Timer starting with timer_thread_attr priority ==> %d <==", timer_thread_sched_param.sched_priority);
    rc = timer_settime(timer_id, 0, &timer_period, 0);
    assert (rc == SUCCESS);

	//create query_frames_thread
    syslog(LOG_WARNING,"\n query_frames_thread dispatching with priority ==> %d <==", query_frames_thread_sched_param.sched_priority);
    rc = pthread_create(&query_frames_thread, &query_frames_thread_attr, query_frames, (void *)&query_frames_threadIdx);
	assert (rc == SUCCESS);

	//create store_frames_thread
	syslog(LOG_WARNING,"\n store_frames_thread dispatching with priority ==> %d <==", store_frames_thread_sched_param.sched_priority);
	rc = pthread_create(&store_frames_thread, &store_frames_thread_attr, store_frames, (void *)&store_frames_threadIdx);
	assert (rc == SUCCESS);

	//wait fot query_frames_thread to exit
    pthread_join(query_frames_thread, NULL);

	//stop timer
    timer_period.it_interval.tv_sec = 0;
    timer_period.it_interval.tv_nsec = 0;
    rc = timer_settime(timer_id, 0, &timer_period, 0);
    assert (rc == SUCCESS);

	//destroy mutex lock
    pthread_mutex_destroy(&app_timer_counter_mutex_lock);
	//add a log
    syslog(LOG_WARNING," rt_thread_dispatcher exiting...");
	//exit thread
    pthread_exit(NULL);
}

//*****************************************
//  Funcation Name:     timer_handler
//*****************************************
void timer_handler(union sigval arg)
{
	int rc;

	//accquire mutex lock on timer counter variable
    rc = pthread_mutex_lock(&app_timer_counter_mutex_lock);
	//verify mutex lock status
	if(rc)
	{
		validate_pthread_mutex_lock_status("app_timer_counter_mutex_lock", rc);
	}
	//update timer counter
    app_timer_counter += APP_TIMER_INTERVAL_IN_MSEC;

    //run at 30Hz
    if((app_timer_counter % QUERY_FRAMES_INTERVAL_IN_MSEC) == 0)
    {
        //signal  query_frames_thread function handler..
        pthread_cond_signal(&cond_query_frames_thread);
    }
	//run at 1 Hz
	if((app_timer_counter % STORE_FRAMES_INTERVAL_IN_MSEC) == 0)
	{
		//signal store_frames_thread function handler..
        pthread_cond_signal(&cond_store_frames_thread);
	}
	//relinquish mutex lock on timer counter variable
    rc = pthread_mutex_unlock(&app_timer_counter_mutex_lock);
	//verify mutex unlock status
	if(rc)
	{
		validate_pthread_mutex_unlock_status("app_timer_counter_mutex_lock", rc);
	}
}

//==============================================================================
//	End of file!
//==============================================================================
