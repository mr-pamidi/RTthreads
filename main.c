//
//  Author: Nagarjuna Pamidi
//
//  File name: main.c
//
//  Description: Manages RT Threads and timer
//

#include "capture.hpp"
#include "include.h"
#include "utilities.h"
#include "v4l2_capture.h"

// /dev/videoX name
char *device_name=NULL;

//global time variable, and a mutex to restrict access to it
unsigned long long app_timer_counter = 1;
pthread_mutex_t app_timer_counter_mutex_lock;
pthread_mutexattr_t app_timer_counter_mutex_lock_attr;

pthread_cond_t cond_query_frames_thread = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_store_frames_thread = PTHREAD_COND_INITIALIZER;

//function prototyping
void *rt_thread_dispatcher_handler(void *something);
void timer_handler(union sigval arg);


static void usage(FILE *fp, int argc, char **argv)
{
        fprintf(fp,
				 "Usage: %s /dev/videoX [options]\n\n"
                 "Options:\n"
                 "-d | --device name   Video device name\n"
                 "-h | --help          Print this message\n"
                 "-m | --mmap          Use memory mapped buffers [default]\n"
                 "-r | --read          Use read() calls\n"
                 "-u | --userp         Use application allocated buffers\n"
                 "-o | --output        Outputs stream to stdout\n"
                 "-f | --format        Force format to 640x480 GREY\n"
                 "-c | --count         Number of frames to grab\n"
                 "",
                 argv[0]);
}

bool use_v4l2_libs = false; //global variable

//**********************************
//  Funcation Name:     main
//**********************************
int main( int argc, char** argv )
{
	if(argc >1)
	{
		device_name = argv[1];
	}
	else
	{
		device_name = "/dev/video0";
	}

	while(1)
	{
		int idx;
		int user_input_option;

		user_input_option = getopt(argc, argv, "fh");
		
		if (user_input_option == -1)
                break; //exit forever loop

        	switch (user_input_option)
        	{

            		case 'h':
                		usage(stdout, argc, argv);
                		return(SUCCESS);

            		case 'f':
                		use_v4l2_libs = true;
                		break;

            		default:
                		usage(stderr, argc, argv);
                		exit(EXIT_FAILURE);
				break;
        	}
    	}


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
    timer_period.it_value.tv_sec = timer_period.it_interval.tv_sec + 2; //2 seconds delay before start
    timer_period.it_value.tv_nsec = timer_period.it_interval.tv_nsec;

	//create timer
    rc = timer_create(CLOCK_REALTIME, &sigevent_param, &timer_id);
	if(rc)
	{
		EXIT_FAIL("timer_create");
	}


	//initialize app_timer_counter variable mutex attributes
	rc = pthread_mutexattr_init(&app_timer_counter_mutex_lock_attr);
	if(rc)
	{
		EXIT_FAIL("pthread_mutexattr_init");
	}
	//set mutex type to PTHREAD_MUTEX_ERRORCHECK
	rc = pthread_mutexattr_settype(&app_timer_counter_mutex_lock_attr, PTHREAD_MUTEX_ERRORCHECK);
	if(rc)
	{
		EXIT_FAIL("pthread_mutexattr_settype");
	}
	//initilize mutex to protect app_timer_counter variable
    rc = pthread_mutex_init(&app_timer_counter_mutex_lock, &app_timer_counter_mutex_lock_attr);
	if(rc)
	{
		EXIT_FAIL("pthread_mutex_init");
	}

	//start timer
	syslog(LOG_WARNING,"\n Timer starting with timer_thread_attr priority ==> %d <==", timer_thread_sched_param.sched_priority);
    rc = timer_settime(timer_id, 0, &timer_period, 0);
	if(rc)
	{
		EXIT_FAIL("timer_settime");
	}

	if(use_v4l2_libs)
	{

	}

	//use openCV APIs
	else
	{
		//create query_frames_thread
    	syslog(LOG_WARNING,"\n query_frames_thread dispatching with priority ==> %d <==", query_frames_thread_sched_param.sched_priority);
    	rc = pthread_create(&query_frames_thread, &query_frames_thread_attr, query_frames, (void *)&query_frames_threadIdx);
		if(rc)
		{
			EXIT_FAIL("pthread_create");
		}

		//create store_frames_thread
		syslog(LOG_WARNING,"\n store_frames_thread dispatching with priority ==> %d <==", store_frames_thread_sched_param.sched_priority);
		rc = pthread_create(&store_frames_thread, &store_frames_thread_attr, store_frames, (void *)&store_frames_threadIdx);
		if(rc)
		{
			EXIT_FAIL("pthread_create");
		}

		//wait fot query_frames_thread to exit
    	pthread_join(query_frames_thread, NULL);
		//wait for store_frames_thread to exit
		pthread_join(store_frames_thread, NULL);
	}

	//stop timer
    timer_period.it_interval.tv_sec = 0;
    timer_period.it_interval.tv_nsec = 0;
    rc = timer_settime(timer_id, 0, &timer_period, 0);
	if(rc)
	{
		EXIT_FAIL("timer_settime");
	}

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
	if(rc)
	{
		EXIT_FAIL("pthread_mutex_lock");
	}

	//update timer counter
    app_timer_counter += APP_TIMER_INTERVAL_IN_MSEC;

	//use v4l2 library APIs
	if(use_v4l2_libs)
	{

	}

	//use openCV APIs
	else
	{
    	//run at 30Hz
    	if((app_timer_counter % QUERY_FRAMES_INTERVAL_IN_MSEC) == 0)
    	{
        	//signal  query_frames_thread
        	rc = pthread_cond_signal(&cond_query_frames_thread);
			if(rc)
			{
				EXIT_FAIL("pthread_cond_signal");
			}
    	}
		//run at 1 Hz
		if((app_timer_counter % STORE_FRAMES_INTERVAL_IN_MSEC) == 0)
		{
			//signal store_frames_thread
        	pthread_cond_signal(&cond_store_frames_thread);
			if(rc)
			{
				EXIT_FAIL("pthread_cond_signal");
			}
		}
	}

	//relinquish mutex lock on timer counter variable
    rc = pthread_mutex_unlock(&app_timer_counter_mutex_lock);
	//verify mutex unlock status
	if(rc)
	{
		EXIT_FAIL("pthread_mutex_unlock");
	}
}

//==============================================================================
//	End of file!
//==============================================================================
