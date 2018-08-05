//
//  Author: Nagarjuna Pamidi
//
//  File name: capture.hpp
//
//  Description: Header file for capture.cpp
//
#ifndef _CAPTURE_HPP_
#define _CAPTURE_HPP_

#include "capture.hpp"
#include "include.h"
#include <iostream>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <vector>

//frame resolution
#define FRAME_HRES 640
#define FRAME_VRES 480

void *query_frames(void *cameraIdx);
void *store_frames(void *params);

#endif
