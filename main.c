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

#define THREAD1         (1)
#define THREAD2         (2)
#define THREAD3         (3)
#define QUERY_FRAMES_THREAD  	(4)
#define STORE_FRAMES_THREAD		(5)

//Note: In this application, 0 is the RT MAX priority..
//..the assign_RT_scheduler_attr() function internally converts 0 to RT_MAX, and similar
#define THREAD1_PRIORITY        (SCHED_FIFO_MAX_PRIORITY + 10) //used as (sched_get_priority_max(SCHED_FIFO) - (SCHED_FIFO_MAX_PRIORITY + 10))
#define THREAD2_PRIORITY        (SCHED_FIFO_MAX_PRIORITY + 10) //used as (sched_get_priority_max(SCHED_FIFO) - (SCHED_FIFO_MAX_PRIORITY + 10))
#define THREAD3_PRIORITY        (SCHED_FIFO_MAX_PRIORITY + 10) //used as (sched_get_priority_max(SCHED_FIFO) - (SCHED_FIFO_MAX_PRIORITY + 10))
#define QUERY_FRAMES_THREAD_PRIORITY	(SCHED_FIFO_MAX_PRIORITY + 1)  //used as (sched_get_priority_max(SCHED_FIFO) - (SCHED_FIFO_MAX_PRIORITY + 1))
#define STORE_FRAMES_THREAD_PRIORITY	(SCHED_FIFO_MAX_PRIORITY + 2)  //used as (sched_get_priority_max(SCHED_FIFO) - (SCHED_FIFO_MAX_PRIORITY + 2))

//global time variable, and a mutex to restrict access to it
unsigned long long system_time = 0;
pthread_mutex_t system_time_mutex_lock;

pthread_cond_t cond_thread1 = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_thread2 = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_thread3 = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_query_frames_thread = PTHREAD_COND_INITIALIZER;

//function prototyping
void *print_test(void *threadIdx);
void *thread_dispatcher(void *something);
void timer_handler(union sigval arg);

//**********************************
//  Funcation Name:     main
//**********************************
int main( int argc, char** argv )
{
    pthread_t main_thread;
    pthread_attr_t main_thread_sched_attr;
    struct sched_param main_thread_sched_param;

    //syslogs
    initialize_syslogs();

    assign_RT_schedular_attr(&main_thread_sched_attr, &main_thread_sched_param, SCHED_FIFO, SCHED_FIFO_MAX_PRIORITY+5, (jetson_tx2_cores)JETSON_TX2_ARM_CORE2);

    syslog(LOG_WARNING, "RT dispatcher thread dispatching with priority ==> %d <==", main_thread_sched_param.sched_priority);
    pthread_create(&main_thread, &main_thread_sched_attr, thread_dispatcher, (void *)0 );

    //wait for main thread to finish execution
    pthread_join(main_thread, NULL);


    //syslog(LOG_WARNING, "End of user log!");
    closelog();

    printf("Exiting..!\n");
    return 0;
}


//*****************************************
//  Funcation Name:     thread_dispatcher
//*****************************************
void *thread_dispatcher(void *something)
{
    int rc;
    pthread_t thread1, thread2, thread3, query_frames_thread, store_frames_thread;
    pthread_attr_t thread1_attr, thread2_attr, thread3_attr, query_frames_thread_attr, store_frames_thread_attr;
    threadParams_t thread1Idx, thread2Idx, thread3Idx, query_frames_threadIdx, store_frames_threadIdx;
    struct sched_param thread1_sched_param, thread2_sched_param, thread3_sched_param, query_frames_thread_sched_param, store_frames_thread_sched_param;

    //posix timer parameters
    struct sigevent sigevent_param;
    timer_t timer_id;
    struct itimerspec timer_period;
    pthread_attr_t timer_thread_attr;
    struct sched_param timer_thread_sched_param;


    thread1Idx.threadIdx = 1;
    thread2Idx.threadIdx = 2;
    thread3Idx.threadIdx = 3;
    query_frames_threadIdx.threadIdx = 4;
	store_frames_threadIdx.threadIdx = 5;

    //assign RT scheduler attributes
    assign_RT_schedular_attr(&thread1_attr, &thread1_sched_param, SCHED_FIFO, THREAD1_PRIORITY, (jetson_tx2_cores)JETSON_TX2_ARM_CORE2);
    assign_RT_schedular_attr(&thread2_attr, &thread2_sched_param, SCHED_FIFO, THREAD2_PRIORITY, (jetson_tx2_cores)JETSON_TX2_ARM_CORE2);
    assign_RT_schedular_attr(&thread3_attr, &thread3_sched_param, SCHED_FIFO, THREAD3_PRIORITY, (jetson_tx2_cores)JETSON_TX2_ARM_CORE2);
    assign_RT_schedular_attr(&query_frames_thread_attr, &query_frames_thread_sched_param, SCHED_FIFO, QUERY_FRAMES_THREAD_PRIORITY, (jetson_tx2_cores)JETSON_TX2_ARM_CORE2);
	assign_RT_schedular_attr(&store_frames_thread_attr, &store_frames_thread_sched_param, SCHED_FIFO, STORE_FRAMES_THREAD_PRIORITY, (jetson_tx2_cores)JETSON_TX2_ARM_CORE2);

#ifdef DEBUG_MODE_ON
    syslog_scheduler();
#endif

//initialize timer thread attributes
    assign_RT_schedular_attr(&timer_thread_attr, &timer_thread_sched_param, SCHED_FIFO, SCHED_FIFO_MAX_PRIORITY, (jetson_tx2_cores)JETSON_TX2_ARM_CORE2);

//assign sigevent paramaters for the timer
    sigevent_param.sigev_notify = SIGEV_THREAD;
    sigevent_param.sigev_value.sival_ptr = &timer_id;
    sigevent_param.sigev_notify_function = &timer_handler;
    sigevent_param.sigev_notify_attributes = &timer_thread_attr;

//initialize timer period values
    timer_period.it_value.tv_sec = (APP_TIMER_INTERVAL_IN_MSEC / MSEC_PER_SEC);
    timer_period.it_value.tv_nsec = (APP_TIMER_INTERVAL_IN_MSEC * NSEC_PER_MSEC);
    timer_period.it_interval.tv_sec = timer_period.it_value.tv_sec;
    timer_period.it_interval.tv_nsec = timer_period.it_value.tv_nsec;

//create timer
    rc = timer_create(CLOCK_REALTIME, &sigevent_param, &timer_id);
    assert (rc == 0);

//initilize mutex to protect timer count variable
    rc = pthread_mutex_init(&system_time_mutex_lock, NULL);
    assert(rc == 0);

//start timer
	syslog(LOG_WARNING,"\n Timer starting with thread priority ==> %d <==", timer_thread_sched_param.sched_priority);
    rc = timer_settime(timer_id, 0, &timer_period, 0);
    assert (rc == 0);
/*
//create threads to test timer
    syslog(LOG_WARNING,"\n Thread:1 dispatching with priority ==> %d <==", thread1_sched_param.sched_priority);
    pthread_create(&thread1, &thread1_attr, print_test, (void *)&thread1Idx );

    syslog(LOG_WARNING,"\n Thread:2 dispatching with priority ==> %d <==", thread2_sched_param.sched_priority);
    pthread_create(&thread2, &thread2_attr, print_test, (void *)&thread2Idx );

    syslog(LOG_WARNING,"\n Thread:3 dispatching with priority ==> %d <==", thread3_sched_param.sched_priority);
    pthread_create(&thread3, &thread3_attr, print_test, (void *)&thread3Idx );
*/
    syslog(LOG_WARNING,"\n QUERY_FRAMES_THREAD dispatching with priority ==> %d <==", query_frames_thread_sched_param.sched_priority);
    pthread_create(&query_frames_thread, &query_frames_thread_attr, query_frames, (void *)&query_frames_threadIdx);

	syslog(LOG_WARNING,"\n STORE_FRAMES_THREAD dispatching with priority ==> %d <==", store_frames_thread_sched_param.sched_priority);
	pthread_create(&store_frames_thread, &store_frames_thread_attr, store_frames, (void *)&store_frames_threadIdx);

    pthread_join(query_frames_thread, NULL);

    //wait for threads to join
    //pthread_join( thread1, NULL);
    //pthread_join( thread2, NULL);
    //pthread_join( thread3, NULL);

//stop timer
    timer_period.it_interval.tv_sec = 0;
    timer_period.it_interval.tv_nsec = 0;
    rc = timer_settime(timer_id, 0, &timer_period, 0);
    assert (rc == 0);

//destroy mutex lock
    pthread_mutex_destroy(&system_time_mutex_lock);

    syslog(LOG_WARNING," RT Dispatcher thread exiting...");

    pthread_exit(NULL);
}

//*****************************************
//  Funcation Name:     timer_handler
//*****************************************
void timer_handler(union sigval arg)
{
    pthread_mutex_lock(&system_time_mutex_lock);

    system_time += APP_TIMER_INTERVAL_IN_MSEC; //update time periodically

    //run at 30Hz
    if((system_time % QUERY_FRAMES_INTERVAL_IN_MSEC) == 0)
    {
        //signal query frames thread..
        pthread_cond_signal(&cond_query_frames_thread);
    }

	//1 Hz
	if((system_time % MSEC_PER_SEC) == 0)
	{
		//signal store frames..
        pthread_cond_signal(&cond_store_frames);
	}

    pthread_mutex_unlock(&system_time_mutex_lock);

    //syslog(LOG_WARNING, " timer_handler called at %lld", system_time);
}


//*****************************************
//  Funcation Name:     print_test
//*****************************************
void *print_test(void *threadIdx)
{
    int rc;
    volatile long loopCounter=0;
    struct timespec log_time, sleep_delta_time;
    threadParams_t *currentIdx = (threadParams_t *)threadIdx;

    sleep_delta_time.tv_sec = 0;
    sleep_delta_time.tv_nsec = (APP_TIMER_INTERVAL_IN_MSEC * NSEC_PER_MSEC);


    while(1)
    {
        //yield before proceeding further
        //so that other thread will get a chance to run as well..
        //pthread_yield();

        pthread_mutex_lock(&system_time_mutex_lock);


        if(currentIdx->threadIdx == THREAD1) //high priority task
        {
            pthread_cond_wait(&cond_thread1, &system_time_mutex_lock);

            //if((system_time % (30)) == 0)
            {
                #ifdef DEBUG_MODE_ON
//                syslog(LOG_WARNING," Thread:%d, timer:%lld", (int)currentIdx->threadIdx, system_time);
                #endif

                //if((loopCounter++) >= 10) break;
                //rc = nanosleep(&sleep_delta_time, NULL);
                //assert(rc == 0);
            }

        }

        else if(currentIdx->threadIdx == THREAD2) //mid priority thread
        {
            pthread_cond_wait(&cond_thread2, &system_time_mutex_lock);

            //if((system_time % (20)) == 0)
            {
                #ifdef DEBUG_MODE_ON
//                    syslog(LOG_WARNING," Thread:%d, timer:%lld", (int)currentIdx->threadIdx, system_time);
                #endif
                //if((loopCounter++) >= 10) break;
                //rc = nanosleep(&sleep_delta_time, NULL);
                //assert(rc == 0);
            }
        }

        else if(currentIdx->threadIdx == THREAD3) //low priority thread
        {
            pthread_cond_wait(&cond_thread3, &system_time_mutex_lock);

            //if((system_time % (10)) == 0)
            {
                #ifdef DEBUG_MODE_ON
//                    syslog(LOG_WARNING," Thread:%d, timer:%lld", (int)currentIdx->threadIdx, system_time);
                #endif
                //if((loopCounter++) >= 10) break;
                //rc = nanosleep(&sleep_delta_time, NULL);
                //assert(rc == 0);
            }
        }

        else
        {
            printf("\nUn-known thread!");
            exit(-1);
        }

        pthread_mutex_unlock(&system_time_mutex_lock);
    }


#ifdef DEBUG_MODE_ON
    //collect current time
    //clock_gettime(CLOCK_REALTIME, &log_time);
    //syslog_time((int)currentIdx->threadIdx, &log_time);
#endif

    //after exiting the loop, log the following message!
    syslog(LOG_WARNING,"\n  ===> Thread:%d <=== done! Loop count:%ld", (int)currentIdx->threadIdx, loopCounter);

}

//End of file!
