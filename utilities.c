//
//  Author: Nagarjuna Pamidi
//
//  File name: utilities.c
//
//  Description: Frequently used function APIs
//
#include "include.h"

//initialize
//jetson_tx2_cores jetson_tx2_cpu_cores;

//------------------------------------------------------------------------------------------------------------------------------
//  Function Name:  assign_RT_schedular_attr
//
//  Parameters:     thread_attr - pthread attribute structure, used while creating a pthread
//                  sched_param - parameter to assign to the scheduler
//                  rt_sched_policy - Type of real time scheduling policy (SCHED_FIFO)
//                  thread_priority - Assign priority based on this priority level (Assigned as (RT_MAX - threadpriority))
//                  core - One of available cores on Jetson TX2 board
//
//  Return:         None
//
//  Description:    Used for assigning the pthread attributes with the provided real-time scheduling scheme and priority.
//
//------------------------------------------------------------------------------------------------------------------------------
void assign_RT_schedular_attr(pthread_attr_t *thread_attr, struct sched_param *sched_param, const int rt_sched_policy, const int thread_priority, const jetson_tx2_cores core)
{
    int rc = 0;

    //initialize the thread attributes to default values
    //and, check if the assignment is successful or not
    rc = pthread_attr_init(thread_attr);
    if(rc)
	{
		EXIT_FAIL("pthread_attr_init");
	}

    //Set scheduling policy to inherited
    rc = pthread_attr_setinheritsched(thread_attr, PTHREAD_EXPLICIT_SCHED);
	if(rc)
	{
		EXIT_FAIL("pthread_attr_setinheritsched");
	}

    //Assign real-time scheduling scheme attribute
    rc = pthread_attr_setschedpolicy(thread_attr, rt_sched_policy);
	if(rc)
	{
		EXIT_FAIL("pthread_attr_setschedpolicy");
	}

    //assign priorty
    //***Note: The priorities are assigned as (RT_MAX - priority)
    sched_param->sched_priority = (sched_get_priority_max(rt_sched_policy) - thread_priority);

    //validate that the thread_priority value is feasible or not
    assert((sched_param->sched_priority >= sched_get_priority_min(rt_sched_policy)) &&
           (sched_param->sched_priority <= sched_get_priority_max(rt_sched_policy)));

    //set schedular with SCHED_FIFO scheme
    rc = sched_setscheduler(THIS_THREAD, rt_sched_policy, sched_param);
	if(rc)
	{
		EXIT_FAIL("sched_setscheduler");
	}


    set_thread_cpu_affinity(THIS_THREAD, core);


    //set scheduling paramater to the thread
    rc = pthread_attr_setschedparam(thread_attr, sched_param);
	if(rc)
	{
		EXIT_FAIL("pthread_attr_setschedparam");
	}
}

//------------------------------------------------------------------------------------------------------------------------------
//  Function Name:  delta_time_in_msec
//
//  Parameters:     end_time - end time
//                  start_time - start time
//
//  Return:         time difference between "end_time" and "start_time" in milliseconds
//
//  Description:    Calculates and returns time difference between the provided 'end' and 'start' times
//
//------------------------------------------------------------------------------------------------------------------------------
double delta_time_in_msec(const struct timespec *end_time, const struct timespec *start_time)
{
    double delta_time;
    double dt_sec = (end_time->tv_sec - start_time->tv_sec); //time difference in seconds
    double dt_nsec = (end_time->tv_nsec - start_time->tv_nsec); //time difference in nano seconds

    //convert to milliseconds
    dt_sec *= MSEC_PER_SEC;
    dt_nsec /= NSEC_PER_MSEC;

    //delta time in milli seconds
    delta_time = (double)(dt_sec + dt_nsec);

    //make sure the time difference is valid
    if(delta_time >= 0)
        return delta_time;

    else
        assert(delta_time >= 0);

    //the funciton call should not reach here.
    //if reaches, exit!
    printf("delta_time_in_msec error in File:\"%s\", Line:%d\n", __FILE__, __LINE__);
    exit(ERROR);
}


//------------------------------------------------------------------------------------------------------------------------------
//  Function Name:  elapsed_time_in_msec
//
//  Parameters:     past_time - end time
//
//  Return:         time difference between "past_time" and current time in milliseconds
//
//  Description:    Calculates and returns time difference between the provided time and current time
//
//------------------------------------------------------------------------------------------------------------------------------
double elapsed_time_in_msec(const struct timespec *past_time)
{
    double dt_sec, dt_nsec, elapsed_time;
    struct timespec current_time;

    //collect current time
    clock_gettime(CLOCK_REALTIME, &current_time);

    dt_sec= (current_time.tv_sec - past_time->tv_sec); //time difference in seconds
    dt_nsec = (current_time.tv_nsec - past_time->tv_nsec); //time difference in nano seconds

    //convert to milliseconds
    dt_sec *= MSEC_PER_SEC;
    dt_nsec /= NSEC_PER_MSEC;

    //elapsed time in milli seconds
    elapsed_time = (double)(dt_sec + dt_nsec);

    //make sure the provided time is not a future time
    if(elapsed_time >= 0)
        return elapsed_time;

    else
        assert(elapsed_time >= 0);

    //the funciton call should not reach here.
    //if reaches, exit!
	EXIT_FAIL("elapsed_time_in_msec");
}


//------------------------------------------------------------------------------------------------------------------------------
//  Function Name:  initialize_syslogs
//
//  Parameters:     None
//
//  Return:         None
//
//  Description:    Initializes the syslog parameters in USER mode
//
//------------------------------------------------------------------------------------------------------------------------------
void initialize_syslogs()
{
    //set log mask to log upto and including LOG_DEBUG level
    setlogmask (LOG_UPTO (LOG_DEBUG)); //Reference: https://linux.die.net/man/3/setlogmask

    //open log with tht provided string.
    //Check the reference to see what each of the parameters are used
    openlog("Real-time pthread practise", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_USER); //Reference: https://linux.die.net/man/3/openlog
}


//------------------------------------------------------------------------------------------------------------------------------
//  Function Name:  min_time
//
//  Parameters:     time1
//                  time2
//
//  Return:         Min of "time1" and "time2"
//
//  Description:    Calculates and returns min time between "time1" and "time2"
//
//------------------------------------------------------------------------------------------------------------------------------
struct timespec min_time(const struct timespec *time1, const struct timespec *time2)
{
  //check min for both seconds and nanoseconds
  if( (time1->tv_sec < time2->tv_sec) &&  (time1->tv_nsec < time2->tv_nsec) )
  {
      return (*time1);
  }
  //check min seconds
  else if (time1->tv_sec < time2->tv_sec)
  {
      return (*time1);
  }
  //check min nanoseconds
  else if (time1->tv_nsec < time2->tv_nsec)
  {
      return (*time1);
  }
  //if all of the above fails, return time2
  else
  {
      return (*time2);
  }
}


//------------------------------------------------------------------------------------------------------------------------------
//  Function Name:  max_time
//
//  Parameters:     time1
//                  time2
//
//  Return:         Max of "time1" and "time2"
//
//  Description:    Calculates and returns max time between "time1" and "time2"
//
//------------------------------------------------------------------------------------------------------------------------------
struct timespec max_time(const struct timespec *time1, const struct timespec *time2)
{
    //check max for both seconds and nanoseconds
    if( (time1->tv_sec > time2->tv_sec) &&  (time1->tv_nsec > time2->tv_nsec) )
    {
        return (*time1);
    }
    //check max seconds
    else if (time1->tv_sec > time2->tv_sec)
    {
        return (*time1);
    }
    //check max nanoseconds
    else if (time1->tv_nsec > time2->tv_nsec)
    {
        return (*time1);
    }
    //if all of the above fails, return time2
    else
    {
        return (*time2);
    }
}


//------------------------------------------------------------------------------------------------------------------------------
//  Function Name:  set_thread_cpu_affinity
//
//  Parameters:     thread - calling thread
//                  core - available core numebr
//
//  Return:         None
//
//  Description:    Used for assigning the pthread cpu affinity of the calling thread, to run on the given core number
//
//------------------------------------------------------------------------------------------------------------------------------
void set_thread_cpu_affinity(pthread_t thread, const jetson_tx2_cores core)
{

    int rc;
    cpu_set_t jetson_cpu_set; //used for cpu affinity set

    //Note: make sure "#define _GNU_SOURCE" is included in the header

    CPU_ZERO(&jetson_cpu_set); //Initialize jetson_cpu_set to all to 0, i.e. no CPUs selected.
    CPU_SET(core, &jetson_cpu_set); //set the bit that represents core

    rc = sched_setaffinity(thread, sizeof(cpu_set_t), &jetson_cpu_set); //Set affinity of current thread to the defined jetson_cpu_set mask
    if(rc)
	{
		EXIT_FAIL("sched_setaffinity");
	}
}

//------------------------------------------------------------------------------------------------------------------------------
//  Function Name:  syslog_scheduler
//
//  Parameters:     None
//
//  Return:         None
//
//  Description:    Logs the type of schedular being used by the calling thread
//
//------------------------------------------------------------------------------------------------------------------------------
//Note: Make sure syslog is initialized, and not closed before calling this function
void syslog_scheduler()
{
    int thread_sched_type;

    //Get current schedular policy for the calling thread
    thread_sched_type = sched_getscheduler(THIS_THREAD);

    switch(thread_sched_type)
    {
        case SCHED_FIFO:
            syslog(LOG_INFO, " Pthread Policy is SCHED_FIFO");
            break;

        case SCHED_OTHER:
            syslog(LOG_INFO, " Pthread Policy is SCHED_OTHER\n");
            break;

        case SCHED_RR:
            syslog(LOG_INFO, " Pthread Policy is SCHED_RR\n");
            break;

        default:
            syslog(LOG_ERR, " Pthread Policy is UNKNOWN\n");
    }
}


//------------------------------------------------------------------------------------------------------------------------------
//  Function Name:  syslog_time
//
//  Parameters:     thread_id - user provided thread number or id
//                  time - time to log
//
//  Return:         None
//
//  Description:    Logs the type of schedular being used by the calling thread
//
//------------------------------------------------------------------------------------------------------------------------------
//Note: Make sure syslog is initialized, and not closed before calling this function
//TBD: Make chanegs to this function as required.
void syslog_time(unsigned int thread_id, const struct timespec *time)
{
    syslog(LOG_INFO, "Thread: %d, syslog timestamp %ld:%ld",thread_id, time->tv_sec, time->tv_nsec);
}

//==============================================================================
//	End of file!
//==============================================================================
