
//
//  Author: Nagarjuna Pamidi
//
//  File name: utilities.h
//
//  Description: Used for querying and storing the frames from the USB camera
//
#include "capture.hpp"

//global variables
extern pthread_cond_t cond_query_frames_thread;
extern pthread_cond_t cond_store_frames_thread;
extern pthread_mutex_t app_timer_counter_mutex_lock;
extern unsigned long long app_timer_counter;

using namespace cv;
using namespace std;

//capture window title
const char capture_window_title[] = "Project-Trails";

//global capture variables
CvCapture* capture;
IplImage* frame;
pthread_mutex_t frame_mutex_lock;
pthread_mutexattr_t frame_mutex_lock_attr;

//query_frames_thread must start before store_frames_thread
void *query_frames(void *cameraIdx)
{
	int rc;
	int *dev = (int *)cameraIdx;

	//initilize mutex to protect timer count variable
	rc = pthread_mutexattr_init(&frame_mutex_lock_attr);
	assert(rc == SUCCESS);
	rc = pthread_mutexattr_settype(&frame_mutex_lock_attr, PTHREAD_MUTEX_ERRORCHECK);
	assert(rc == SUCCESS);
    rc = pthread_mutex_init(&frame_mutex_lock, &frame_mutex_lock_attr);
    assert(rc == SUCCESS);

    capture = cvCreateCameraCapture(0);
    cvSetCaptureProperty(capture, CV_CAP_PROP_FRAME_WIDTH, FRAME_HRES);
    cvSetCaptureProperty(capture, CV_CAP_PROP_FRAME_HEIGHT, FRAME_VRES);
    cvNamedWindow(capture_window_title, CV_WINDOW_AUTOSIZE);

    while(1)
    {
        //wait for signal
        rc = pthread_mutex_lock(&app_timer_counter_mutex_lock);
		if(rc)
		{
			validate_pthread_mutex_lock_status("app_timer_counter_mutex_lock", rc);
		}
        pthread_cond_wait(&cond_query_frames_thread, &app_timer_counter_mutex_lock);
        rc = pthread_mutex_unlock(&app_timer_counter_mutex_lock);
		if(rc)
		{
			validate_pthread_mutex_unlock_status("app_timer_counter_mutex_lock", rc);
		}

        #ifdef DEBUG_MODE_ON
            //syslog(LOG_WARNING," cvQueryframe start at :%lld", app_timer_counter);
        #endif

        //thread safe
        rc = pthread_mutex_lock(&frame_mutex_lock);
		if(rc)
		{
			validate_pthread_mutex_lock_status("frame_mutex_lock", rc);
		}
		frame = cvQueryFrame(capture);
		rc = pthread_mutex_unlock(&frame_mutex_lock);
		if(rc)
		{
			validate_pthread_mutex_unlock_status("frame_mutex_lock", rc);
		}

        if(!frame) break;

        #ifdef DEBUG_MODE_ON
            //syslog(LOG_WARNING," cvQueryframe done at :%lld", app_timer_counter);
        #endif

        cvShowImage(capture_window_title, frame);

        char c = cvWaitKey(1);
        if( c == 'q')
        {
            break;
        }

        #ifdef DEBUG_MODE_ON
            //syslog(LOG_WARNING," cvShowImage done at :%lld", app_timer_counter);
        #endif
    }

    cvReleaseCapture(&capture);
    cvDestroyWindow(capture_window_title);

	//destroy mutex lock!
	pthread_mutex_destroy(&frame_mutex_lock);

    #ifdef DEBUG_MODE_ON
        syslog(LOG_WARNING," QUERY_FRAMES_THREAD exiting...");
    #endif

};


void *store_frames(void *params)
{

	int rc;
	vector<int> compression_params;
    compression_params.push_back(CV_IMWRITE_PXM_BINARY);
    compression_params.push_back(1);
	unsigned int frame_counter=0;

	char ppm_file_name[20] ={};

    Mat mat;

	while(1)
	{
		//wait for signal
		rc = pthread_mutex_lock(&app_timer_counter_mutex_lock);
		if(rc)
		{
			validate_pthread_mutex_lock_status("app_timer_counter_mutex_lock", rc);
		}

		rc = pthread_cond_wait(&cond_store_frames_thread, &app_timer_counter_mutex_lock);
		assert(rc == SUCCESS);

		rc = pthread_mutex_unlock(&app_timer_counter_mutex_lock);
		if(rc)
		{
			validate_pthread_mutex_unlock_status("app_timer_counter_mutex_lock", rc);
		}


		#ifdef DEBUG_MODE_ON
			syslog(LOG_WARNING, " store_frames start write at:%lld", app_timer_counter);
		#endif

		//make sure other threads are not updating frames at this moment
		rc = pthread_mutex_lock(&frame_mutex_lock);
		if(rc)
		{
			validate_pthread_mutex_lock_status("frame_mutex_lock", rc);
		}

		mat = cvarrToMat(frame);

	    rc = pthread_mutex_unlock(&frame_mutex_lock);
		if(rc)
		{
			validate_pthread_mutex_unlock_status("frame_mutex_lock", rc);
		}

		sprintf(ppm_file_name, "alpha%d.ppm", frame_counter);

	   	try
	   	{
			imwrite(ppm_file_name, mat, compression_params);
	   	}
	   	catch (runtime_error& ex)
	   	{
			syslog(LOG_ERR, " ***Exception converting image to PNG format:");
			exit(ERROR);
	   	}

	   	++frame_counter;

	   	#ifdef DEBUG_MODE_ON
			syslog(LOG_WARNING, " store_frames end of write at:%lld", app_timer_counter);
	   	#endif

	}

}

//==============================================================================
//	End of file!
//==============================================================================
