//
//  Author: Nagarjuna Pamidi
//
//  File name: utilities.h
//
//  Description: Header file for utilities.c
//

#ifndef _UTILITIES_H
#define _UTILITIES_H

#include "include.h"

//APIs
void assign_RT_schedular_attr(pthread_attr_t *thread_attr, struct sched_param *sched_param, const int rt_sched_policy, const int thread_priority, const jetson_tx2_cores core);
void set_thread_cpu_affinity(pthread_t thread, const jetson_tx2_cores core);
void initialize_syslogs();
void syslog_scheduler();
void syslog_time(unsigned int thread_id, const struct timespec *time);
double delta_time_in_msec(const struct timespec *end_time, const struct timespec *start_time);
double elapsed_time_in_msec(const struct timespec *past_time);
struct timespec max_time(const struct timespec *time1, const struct timespec *time2);
struct timespec min_time(const struct timespec *time1, const struct timespec *time2);

#endif