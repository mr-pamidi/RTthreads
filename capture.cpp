/*
 *
 *  Example by Sam Siewert 
 *
 *
 */
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <vector>
#include "capture.hpp"
#include "include.h"

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

//frame resolution
#define FRAME_HRES 640
#define FRAME_VRES 480

//global variables
extern pthread_cond_t cond_capture_thread;
extern pthread_mutex_t system_time_mutex_lock;
extern unsigned long long system_time;

using namespace cv;
using namespace std;

//capture window title
const char capture_window_title[] = "Project-Trails";

//global capture variables
CvCapture* capture;
IplImage* frame;
pthread_mutex_t frame_mutex_lock;

void *capture_frames(void *cameraIdx)
{
    int *dev = (int *)cameraIdx;

    capture = cvCreateCameraCapture(0);
    cvSetCaptureProperty(capture, CV_CAP_PROP_FRAME_WIDTH, FRAME_HRES);
    cvSetCaptureProperty(capture, CV_CAP_PROP_FRAME_HEIGHT, FRAME_VRES);
    cvNamedWindow("Capture Example", CV_WINDOW_AUTOSIZE);
    

    while(1)
    {
                
        pthread_mutex_lock(&system_time_mutex_lock);
        //wait for signal
        pthread_cond_wait(&cond_capture_thread, &system_time_mutex_lock);
        pthread_mutex_unlock(&system_time_mutex_lock);

        #ifdef DEBUG_MODE_ON
            syslog(LOG_WARNING," cvQueryframe start at :%lld", system_time);   
        #endif
        
        //thread safe
        pthread_mutex_lock(&frame_mutex_lock);                
        frame = cvQueryFrame(capture);
        pthread_mutex_unlock(&frame_mutex_lock);
        
        if(!frame) break;
        
        #ifdef DEBUG_MODE_ON
            syslog(LOG_WARNING," cvQueryframe done at :%lld", system_time);
        #endif
        
        cvShowImage("Capture Example", frame);
            
        char c = cvWaitKey(1);
        if( c == 'q')
        {
            break;
        }
        
        #ifdef DEBUG_MODE_ON
            syslog(LOG_WARNING," cvShowImage done at :%lld", system_time);
        #endif        
    }

    cvReleaseCapture(&capture);
    cvDestroyWindow("Capture Example");

    #ifdef DEBUG_MODE_ON
        syslog(LOG_WARNING," Capture Thread exiting...");
    #endif
        								
};


void *store_frames(void *params)
{
        
    vector<int> compression_params;
    compression_params.push_back(CV_IMWRITE_PNG_COMPRESSION);
    compression_params.push_back(9);
    
    Mat mat = cvarrToMat(frame);
    
    try
    {
        imwrite("alpha.png", mat, compression_params);
    }
    catch (runtime_error& ex) 
    {
        syslog(LOG_ERR, " ***Exception converting image to PNG format:");
        exit(ERROR);
    }
}
