//
//  Author: Nagarjuna Pamidi
//
//  File name: v4l2_capture.c
//
//  Description: Queries and stores frames periodically
//
// Note:Parts of this file implementation is referenced from..
// ..source: http://ecee.colorado.edu/~ecen5623/ecen/ex/Linux/computer-vision/simple-capture/capture.c

#include "include.h"

// /dev/videoX name
extern static char *device_name; //extern from main.c

static struct v4l2_format fmt;

static void init_device(void)
{
	int rc;
	//
    struct v4l2_capability device_v4l2_capability;
    struct v4l2_cropcap device_v4l2_cropcap;
    struct v4l2_crop device_v4l2_crop;
    unsigned int min;

	//https://www.linuxtv.org/downloads/v4l-dvb-apis-old/vidioc-querycap.html
	//query device capabilities
    rc == xioctl(device_file_descriptor, VIDIOC_QUERYCAP, &device_v4l2_capability))
	if(rc)
	{
        if (EINVAL == errno)
		{
            fprintf(stderr, "%s is no V4L2 device\n", device_name);
        }
        EXIT_FAIL("VIDIOC_QUERYCAP");
    }

	//check if the '/dev/videoX' is video capable or not
    if (!(device_v4l2_capability.capabilities & V4L2_CAP_VIDEO_CAPTURE))
    {
        fprintf(stderr, "%s is no video capture device\n", device_name);
        EXIT_FAIL("V4L2_CAP_VIDEO_CAPTURE");
    }

	//check if '/dev/videoX' support I/O read/write capability or not
    if (!(device_v4l2_capability.capabilities & V4L2_CAP_READWRITE))
    {
        fprintf(stderr, "%s does not support read i/o\n", device_name);
        EXIT_FAIL("V4L2_CAP_READWRITE");
    }

	//check if '/dev/videoX' support streaming capability or not
    if (!(device_v4l2_capability.capabilities & V4L2_CAP_STREAMING))
    {
        fprintf(stderr, "%s does not support streaming i/o\n", device_name);
        EXIT_FAIL("V4L2_CAP_STREAMING");
    }

//Tune video standards
    CLEAR_MEMORY(device_v4l2_cropcap); //clear structure

	//set v4l2 buffer type to video capture type
    device_v4l2_cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	//https://www.linuxtv.org/downloads/legacy/video4linux/API/V4L2_API/spec/rn01re22.html
	//set v4l2 buffer type to capture type
    if (0 == xioctl(device_file_descriptor, VIDIOC_CROPCAP, &device_v4l2_cropcap))
    {
        device_v4l2_crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        device_v4l2_crop.c = device_v4l2_cropcap.defrect; //reset to default

		//use default crop scaling
        rc == xioctl(device_file_descriptor, VIDIOC_S_CROP, &device_v4l2_crop))
        if(rc)
		{
            switch (errno)
            {
                case EINVAL:
                    // Cropping not supported
					fprintf(stdout, "%s cropping not supported\n", device_name);
                    break;

				default:
                    //Errors Ignored
                    break;
            }
        }

    }
    else
    {
        // Errors ignored
    }


    CLEAR_MEMORY(fmt);

    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	/*
	//set capture fram height and width
	//fmt.fmt.pix.width       = 640;
    //fmt.fmt.pix.height      = 480;

	//specify pixel format
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    //fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_UYVY;
    //fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_VYUY;
    // Would be nice if camera supported
    //fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_GREY;
    //fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB24;

    //fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;
    //fmt.fmt.pix.field       = V4L2_FIELD_NONE;

    /*if (-1 == xioctl(device_file_descriptor, VIDIOC_S_FMT, &fmt))
	{
        EXIT_FAIL("VIDIOC_S_FMT");
	} //*/

	/* Preserve original settings as set by v4l2-ctl for example */
	if (-1 == xioctl(device_file_descriptor, VIDIOC_G_FMT, &fmt))
	{
		EXIT_FAIL("VIDIOC_G_FMT");
	}

	//leaving the reference code as is...
    /* Buggy driver paranoia. */
    min = fmt.fmt.pix.width * 2;
    if (fmt.fmt.pix.bytesperline < min)
            fmt.fmt.pix.bytesperline = min; //Distance in bytes between the leftmost pixels in two adjacent lines.
    min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
    if (fmt.fmt.pix.sizeimage < min)
            fmt.fmt.pix.sizeimage = min;

/*
    switch (io)
    {
        case IO_METHOD_READ: */
            init_read(fmt.fmt.pix.sizeimage); /*
            break;

        case IO_METHOD_MMAP:
            init_mmap();
            break;

        case IO_METHOD_USERPTR:
            init_userp(fmt.fmt.pix.sizeimage);
            break;
    } */
}

static void open_device(void)
{
    struct stat device_stats;

	//retrive informaiton about the /dev/videoX file
    rc = stat(device_name, &device_stats));
	if(rc)
	{
        fprintf(stderr, "Cannot identify '%s'\n", device_name);
        EXIT_FAIL("stat");
    }

	//test for device_stats.st_mode directory
    if (!S_ISCHR(device_stats.st_mode))
	{
        fprintf(stderr, "%s is no device\n", device_name);
        EXIT_FAIL("S_ISCHR");
    }

	//open /dev/videoX file with read/write capabiliteis, and with non-blocking option.
    device_file_descriptor = open(device_name, O_RDWR | O_NONBLOCK, 0);
    if (device_file_descriptor == -1)
	{
        fprintf(stderr, "Cannot open '%s'\n",device_name);
        EXIT_FAIL("open");
    }
}


static int xioctl(int file_descriptor, int request, void *arg)
{
    int rc;

    do
    {
		//http://man7.org/linux/man-pages/man2/ioctl.2.html
		//system call manipulates the underlying device parameters..
        // ..of '/dev/videoX'
        rc = ioctl(file_descriptor, request, arg);
    } while (-1 == r && EINTR == errno);

    return rc;
}
