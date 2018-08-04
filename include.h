//
//  Author: Nagarjuna Pamidi
//
//  File name: include.h
//
//
//  Description: contains the globally used macros
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
#include <syslog.h> //Reference: https://linux.die.net/man/3/syslog
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>

#define SCHED_FIFO_MAX_PRIORITY     0   //which converts to (sched_get_priority_max(SCHED_FIFO) - SCHED_FIFO_MAX_PRIORITY)

#define MSEC_PER_SEC    (1000)              //milli seconds per second
#define USEC_PER_SEC    (1000*1000)        //micro seconds per second
#define USEC_PER_MSEC   (1000)UL            //micro seconds per milli seconds
#define NSEC_PER_SEC    (1000*1000*1000)   //nano seconds per second
#define NSEC_PER_MSEC   (1000*1000)        //nano seconds per milli seconds
#define NSEC_PER_USEC   (1000)              //nano seconds per micro seconds

//1000 Hz
#define APP_TIMER_INTERVAL_IN_MSEC   1 //timer period      

//30 Hz
#define QUERY_FRAMES_INTERVAL_IN_MSEC    33 //frame query

//as root do: "~./tegrastats" to find these CPU core numbers
#define JETSON_TX2_ARM_CORE0    (0)
#define JETSON_TX2_DENVER_CORE0 (1)
#define JETSON_TX2_DENVER_CORE1 (2)
#define JETSON_TX2_ARM_CORE1    (3)
#define JETSON_TX2_ARM_CORE2    (4)
#define JETSON_TX2_ARM_CORE3    (5)

#define TRUE    (1)
#define FALSE   (0)
#define ERROR   (-1)
#define SUCCESS (0)
#define THIS_THREAD  (0)

//use this macro while developing i sin progress
//and to logthe  required debug messages.
//#define DEBUG_MODE_ON   TRUE

//user defined thread indexes
typedef struct
{
    unsigned int threadIdx;
} threadParams_t;

//as root do: "~./tegrastats" to find these CPU core numbers
typedef enum jetson_tx2_cores{
    arm_core0,      //core 0
    denver_core0,   //core 1
    denver_core1,   //core 2
    arm_core1,      //core 3
    arm_core2,      //core 4
    arm_core3,      //core 5
}jetson_tx2_cores;

#endif
