
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
extern pthread_mutex_t system_time_mutex_lock;
extern unsigned long long system_time;

pthread_cond_t cond_store_frames = PTHREAD_COND_INITIALIZER;

using namespace cv;
using namespace std;

//capture window title
const char capture_window_title[] = "Project-Trails";

//global capture variables
CvCapture* capture;
IplImage* frame;
pthread_mutex_t frame_mutex_lock;

void *query_frames(void *cameraIdx)
{
    int *dev = (int *)cameraIdx;

    capture = cvCreateCameraCapture(0);
    cvSetCaptureProperty(capture, CV_CAP_PROP_FRAME_WIDTH, FRAME_HRES);
    cvSetCaptureProperty(capture, CV_CAP_PROP_FRAME_HEIGHT, FRAME_VRES);
    cvNamedWindow(capture_window_title, CV_WINDOW_AUTOSIZE);


    while(1)
    {

        pthread_mutex_lock(&system_time_mutex_lock);
        //wait for signal
        pthread_cond_wait(&cond_query_frames_thread, &system_time_mutex_lock);
        pthread_mutex_unlock(&system_time_mutex_lock);

        #ifdef DEBUG_MODE_ON
            syslog(LOG_WARNING," cvQueryframe start at :%lld", system_time);
        #endif

        //thread safe
        pthread_mutex_lock(&frame_mutex_lock);
        frame = cvQueryFrame(capture);
		pthread_cond_signal(&cond_store_frames); //signal to store current file..
		pthread_mutex_unlock(&frame_mutex_lock);

        if(!frame) break;

        #ifdef DEBUG_MODE_ON
            syslog(LOG_WARNING," cvQueryframe done at :%lld", system_time);
        #endif

        cvShowImage(capture_window_title, frame);

        char c = cvWaitKey(1);
        if( c == 'q')
        {
            break;
        }

        #ifdef DEBUG_MODE_ON
            syslog(LOG_WARNING," cvShowImage done at :%lld", system_time);
        #endif

		break;
    }

    cvReleaseCapture(&capture);
    cvDestroyWindow(capture_window_title);

    #ifdef DEBUG_MODE_ON
        syslog(LOG_WARNING," query_frames() exiting...");
    #endif

};


void *store_frames(void *params)
{

    vector<int> compression_params;
    compression_params.push_back(CV_IMWRITE_PXM_BINARY);
    compression_params.push_back(1);

    Mat mat;
	while(1)
	{
		pthread_mutex_lock(&frame_mutex_lock);
		pthread_cond_wait(&cond_store_frames, &frame_mutex_lock);
        pthread_mutex_unlock(&frame_mutex_lock);

		mat = cvarrToMat(frame);

	   try
	   {
		   imwrite("alpha.ppm", mat, compression_params);
	   }
	   catch (runtime_error& ex)
	   {
		   syslog(LOG_ERR, " ***Exception converting image to PNG format:");
		   exit(ERROR);
	   }
	}

}
