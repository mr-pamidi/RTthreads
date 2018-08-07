//
//  Author: Nagarjuna Pamidi
//
//  File name: include.h
//
//  Description: Contains the globally used macros and variables
//

#ifndef _INCLUDE_H
#define _INCLUDE_H

#define _GNU_SOURCE // see "feature_test_macros"
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h> //Reference: https://linux.die.net/man/3/syslog
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>

//Important Note: Priorities in this application are used as..
//0 refers to (RT_MAX) priority,
//1 refers to (RT_MAX - 1) priority,
//2 refers to (RT_MAX - 2) priority, and so on..
#define SCHED_FIFO_MAX_PRIORITY     	(0)   //used as (sched_get_priority_max(SCHED_FIFO) - (SCHED_FIFO_MAX_PRIORITY))
#define TIMER_THREAD_PRIORITY			(SCHED_FIFO_MAX_PRIORITY) //used as (sched_get_priority_max(SCHED_FIFO) - (SCHED_FIFO_MAX_PRIORITY))
#define STORE_FRAMES_THREAD_PRIORITY	(SCHED_FIFO_MAX_PRIORITY + 1) //used as (sched_get_priority_max(SCHED_FIFO) - (SCHED_FIFO_MAX_PRIORITY + 1))
#define QUERY_FRAMES_THREAD_PRIORITY	(SCHED_FIFO_MAX_PRIORITY + 2) //used as (sched_get_priority_max(SCHED_FIFO) - (SCHED_FIFO_MAX_PRIORITY + 2))
#define RT_THREAD_DISPATCHER_PRIORITY	(SCHED_FIFO_MAX_PRIORITY)// + 5) //used as (sched_get_priority_max(SCHED_FIFO) - (SCHED_FIFO_MAX_PRIORITY + 5))

//Thread indexes
#define QUERY_FRAMES_THREAD_IDX  	(1)
#define STORE_FRAMES_THREAD_IDX		(2)
//macros for time functions
#define MSEC_PER_SEC    (1000)              //milli seconds per second
#define USEC_PER_SEC    (1000*1000)        	//micro seconds per second
#define USEC_PER_MSEC   (1000)            	//micro seconds per milli seconds
#define NSEC_PER_SEC    (1000*1000*1000)	//nano seconds per second
#define NSEC_PER_MSEC   (1000*1000)        	//nano seconds per milli seconds
#define NSEC_PER_USEC   (1000)              //nano seconds per micro seconds

//1000 Hz
#define APP_TIMER_INTERVAL_IN_MSEC	1 //timer period
//30 Hz
#define QUERY_FRAMES_INTERVAL_IN_MSEC	33 //frame query
//1 Hz
#define STORE_FRAMES_INTERVAL_IN_MSEC	(MSEC_PER_SEC) //store frames

//other utilities
#define TRUE    	(1)
#define FALSE   	(0)
#define ERROR   	(-1)
#define SUCCESS 	(0)
#define THIS_THREAD	(0)

//user defined thread indexes
typedef struct
{
    unsigned int threadIdx;
} threadParams_t;

//as root do: "~./tegrastats" to find these CPU core numbers
#define JETSON_TX2_ARM_CORE0    (0)
#define JETSON_TX2_DENVER_CORE0 (1)
#define JETSON_TX2_DENVER_CORE1 (2)
#define JETSON_TX2_ARM_CORE1    (3)
#define JETSON_TX2_ARM_CORE2    (4)
#define JETSON_TX2_ARM_CORE3    (5)
//as root do: "~./tegrastats" to find these CPU core numbers
typedef enum jetson_tx2_cores{
    arm_core0,      //core 0
    //denver_core0,   //core 1
    //denver_core1,   //core 2
    arm_core1=3,      //core 3
    arm_core2,      //core 4
    arm_core3,      //core 5
}jetson_tx2_cores;

#define EXIT_FAIL(fun_name){\
	printf("\n********** Run time ERROR **************\
	     	\nFile: \"%s\"						\
		 	\nLine: %d							\
		 	\nsymbol: %s						\
		 	\nError: %s							\
		 	\n\nExiting Application...\n\n", __FILE__, __LINE__, fun_name, strerror(errno));\
	exit(ERROR);\
}

#endif //_INCLUDE_H

//==============================================================================
//	End of file!
//==============================================================================
