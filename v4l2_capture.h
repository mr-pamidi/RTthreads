//
//  Author: Nagarjuna Pamidi
//
//  File name: v4l2_capture.h
//
//  Description: Header file for v4l2_capture.c file
//

#ifndef _V4L2_CAPTURE_HPP_
#define _V4L2_CAPTURE_HPP_

#include "include.h"

static void init_device(void);
static void open_device(void);
static int xioctl(int file_descriptor, int request, void *arg);

#endif //_V4L2_CAPTURE_HPP_
