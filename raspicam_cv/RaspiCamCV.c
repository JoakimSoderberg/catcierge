/////////////////////////////////////////////////////////////
//
// Many source code lines are copied from RaspiVid.c
// Copyright (c) 2012, Broadcom Europe Ltd
// 
// Lines have been added by Pierre Raufast - mai 2013
// pierre.raufast@gmail.com
// to work with OpenCV 2.3
// visit thinkrpi.wordpress.com
// Enjoy !
// This file display camera in a OpenCv window
//
// For a better world, read Giono's Books
// 
/////////////////////////////////////////////////////////////
//
// Emil Valkov - robidouille@valkov.com
//
// Converted to a library, which exposes an interface
// similar to what OpenCV provides, but uses the RaspiCam
// underneath - September 22 2013.
//
// cvCreateCameraCapture -> raspiCamCvCreateCameraCapture
// cvReleaseCapture -> raspiCamCvReleaseCapture
// cvSetCaptureProperty -> raspiCamCvSetCaptureProperty
// cvQueryFrame -> raspiCamCvQueryFrame
//
/////////////////////////////////////////////////////////////

#include "RaspiCamCV.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>

//new
#include <cv.h>
#include <highgui.h>
#include "time.h"

#include "bcm_host.h"
#include "interface/vcos/vcos.h"

#include "interface/mmal/mmal.h"
#include "interface/mmal/mmal_logging.h"
#include "interface/mmal/mmal_buffer.h"
#include "interface/mmal/util/mmal_util.h"
#include "interface/mmal/util/mmal_util_params.h"
#include "interface/mmal/util/mmal_default_components.h"
#include "interface/mmal/util/mmal_connection.h"

#include "RaspiCamControl.h"

#include <semaphore.h>

/// Camera number to use - we only have one camera, indexed from 0.
#define CAMERA_NUMBER 0

// Standard port setting for the camera component
#define MMAL_CAMERA_PREVIEW_PORT 0
#define MMAL_CAMERA_VIDEO_PORT 1
#define MMAL_CAMERA_CAPTURE_PORT 2

// Video format information
#define VIDEO_FRAME_RATE_NUM 30
#define VIDEO_FRAME_RATE_DEN 1

/// Video render needs at least 2 buffers.
#define VIDEO_OUTPUT_BUFFERS_NUM 3

// Max bitrate we allow for recording
const int MAX_BITRATE = 30000000; // 30Mbits/s

int mmal_status_to_int(MMAL_STATUS_T status);

/** Structure containing all state information for the current run
 */
typedef struct _RASPIVID_STATE
{
	int finished;
	int width;                          /// Requested width of image
	int height;                         /// requested height of image
	int bitrate;                        /// Requested bitrate
	int framerate;                      /// Requested frame rate (fps)
	int graymode;			/// capture in gray only (2x faster)
	int immutableInput;     /// Flag to specify whether encoder works in place or creates a new buffer. Result is preview can display either
                            /// the camera output or the encoder output (with compression artifacts)
	RASPICAM_CAMERA_PARAMETERS camera_parameters; /// Camera setup parameters

	MMAL_COMPONENT_T *camera_component;    /// Pointer to the camera component
	MMAL_COMPONENT_T *encoder_component;   /// Pointer to the encoder component
	MMAL_CONNECTION_T *preview_connection; /// Pointer to the connection from camera to preview
	MMAL_CONNECTION_T *encoder_connection; /// Pointer to the connection from camera to encoder

	MMAL_POOL_T *video_pool; /// Pointer to the pool of buffers used by encoder output port

	IplImage *py, *pu, *pv;
	IplImage *pu_big, *pv_big, *yuvImage,* dstImage;

	VCOS_SEMAPHORE_T capture_sem;
	VCOS_SEMAPHORE_T capture_done_sem;
   
} RASPIVID_STATE;

// default status
static void default_status(RASPIVID_STATE *state)
{
   if (!state)
   {
      vcos_assert(0);
      return;
   }

   // Default everything to zero
   memset(state, 0, sizeof(RASPIVID_STATE));

   // Now set anything non-zero
   state->finished          = 0;
   state->width 			= 640;      // use a multiple of 320 (640, 1280)
   state->height 			= 480;		// use a multiple of 240 (480, 960)
   state->bitrate 			= 17000000; // This is a decent default bitrate for 1080p
   state->framerate 		= VIDEO_FRAME_RATE_NUM;
   state->immutableInput 	= 1;
   state->graymode 			= 0;		// Gray (1) much faster than color (0)
   
   // Set up the camera_parameters to default
   raspicamcontrol_set_defaults(&state->camera_parameters);
}

/**
 *  buffer header callback function for video
 *
 * @param port Pointer to port from which callback originated
 * @param buffer mmal buffer header pointer
 */
static void video_buffer_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
	MMAL_BUFFER_HEADER_T *new_buffer;
	RASPIVID_STATE * state = (RASPIVID_STATE *)port->userdata;		

	if (state)
	{
		if (state->finished) {
			vcos_semaphore_post(&state->capture_done_sem);
			return;
		}
		if (buffer->length)
		{
			mmal_buffer_header_mem_lock(buffer);
 
			//
			// *** PR : OPEN CV Stuff here !
			//
			int w=state->width;	// get image size
			int h=state->height;
			int h4=h/4;

			memcpy(state->py->imageData,buffer->data,w*h);	// read Y
		
			if (state->graymode==0)
			{
				memcpy(state->pu->imageData,buffer->data+w*h,w*h4); // read U
				memcpy(state->pv->imageData,buffer->data+w*h+w*h4,w*h4); // read v
			}

			vcos_semaphore_post(&state->capture_done_sem);
			vcos_semaphore_wait(&state->capture_sem);

			mmal_buffer_header_mem_unlock(buffer);
		}
		else
		{
			vcos_log_error("buffer null");
		}
	}
	else
	{
		vcos_log_error("Received a encoder buffer callback with no state");
	}

	// release buffer back to the pool
	mmal_buffer_header_release(buffer);

	// and send one back to the port (if still open)
	if (port->is_enabled)
	{
		MMAL_STATUS_T status;

		new_buffer = mmal_queue_get(state->video_pool->queue);

		if (new_buffer)
			status = mmal_port_send_buffer(port, new_buffer);

		if (!new_buffer || status != MMAL_SUCCESS)
			vcos_log_error("Unable to return a buffer to the encoder port");
	}
}


/**
 * Create the camera component, set up its ports
 *
 * @param state Pointer to state control struct
 *
 * @return 0 if failed, pointer to component if successful
 *
 */
static MMAL_COMPONENT_T *create_camera_component(RASPIVID_STATE *state)
{
	MMAL_COMPONENT_T *camera = 0;
	MMAL_ES_FORMAT_T *format;
	MMAL_PORT_T *preview_port = NULL, *video_port = NULL, *still_port = NULL;
	MMAL_STATUS_T status;
	
	/* Create the component */
	status = mmal_component_create(MMAL_COMPONENT_DEFAULT_CAMERA, &camera);
	
	if (status != MMAL_SUCCESS)
	{
	   vcos_log_error("Failed to create camera component");
	   goto error;
	}
	
	if (!camera->output_num)
	{
	   vcos_log_error("Camera doesn't have output ports");
	   goto error;
	}
	
	video_port = camera->output[MMAL_CAMERA_VIDEO_PORT];
	still_port = camera->output[MMAL_CAMERA_CAPTURE_PORT];
	
	//  set up the camera configuration
	{
	   MMAL_PARAMETER_CAMERA_CONFIG_T cam_config =
	   {
	      { MMAL_PARAMETER_CAMERA_CONFIG, sizeof(cam_config) },
	      .max_stills_w = state->width,
	      .max_stills_h = state->height,
	      .stills_yuv422 = 0,
	      .one_shot_stills = 0,
	      .max_preview_video_w = state->width,
	      .max_preview_video_h = state->height,
	      .num_preview_video_frames = 3,
	      .stills_capture_circular_buffer_height = 0,
	      .fast_preview_resume = 0,
	      .use_stc_timestamp = MMAL_PARAM_TIMESTAMP_MODE_RESET_STC
	   };
	   mmal_port_parameter_set(camera->control, &cam_config.hdr);
	}
	// Set the encode format on the video  port
	
	format = video_port->format;
	format->encoding_variant = MMAL_ENCODING_I420;
	format->encoding = MMAL_ENCODING_I420;
	format->es->video.width = state->width;
	format->es->video.height = state->height;
	format->es->video.crop.x = 0;
	format->es->video.crop.y = 0;
	format->es->video.crop.width = state->width;
	format->es->video.crop.height = state->height;
	format->es->video.frame_rate.num = state->framerate;
	format->es->video.frame_rate.den = VIDEO_FRAME_RATE_DEN;
	
	status = mmal_port_format_commit(video_port);
	if (status)
	{
	   vcos_log_error("camera video format couldn't be set");
	   goto error;
	}
	
	// PR : plug the callback to the video port 
	status = mmal_port_enable(video_port, video_buffer_callback);
	if (status)
	{
	   vcos_log_error("camera video callback2 error");
	   goto error;
	}

   // Ensure there are enough buffers to avoid dropping frames
   if (video_port->buffer_num < VIDEO_OUTPUT_BUFFERS_NUM)
      video_port->buffer_num = VIDEO_OUTPUT_BUFFERS_NUM;


   // Set the encode format on the still  port
   format = still_port->format;
   format->encoding = MMAL_ENCODING_OPAQUE;
   format->encoding_variant = MMAL_ENCODING_I420;
   format->es->video.width = state->width;
   format->es->video.height = state->height;
   format->es->video.crop.x = 0;
   format->es->video.crop.y = 0;
   format->es->video.crop.width = state->width;
   format->es->video.crop.height = state->height;
   format->es->video.frame_rate.num = 1;
   format->es->video.frame_rate.den = 1;

   status = mmal_port_format_commit(still_port);
   if (status)
   {
      vcos_log_error("camera still format couldn't be set");
      goto error;
   }

	
	//PR : create pool of message on video port
	MMAL_POOL_T *pool;
	video_port->buffer_size = video_port->buffer_size_recommended;
	video_port->buffer_num = video_port->buffer_num_recommended;
	pool = mmal_port_pool_create(video_port, video_port->buffer_num, video_port->buffer_size);
	if (!pool)
	{
	   vcos_log_error("Failed to create buffer header pool for video output port");
	}
	state->video_pool = pool;

	/* Ensure there are enough buffers to avoid dropping frames */
	if (still_port->buffer_num < VIDEO_OUTPUT_BUFFERS_NUM)
	   still_port->buffer_num = VIDEO_OUTPUT_BUFFERS_NUM;
	
	/* Enable component */
	status = mmal_component_enable(camera);
	
	if (status)
	{
	   vcos_log_error("camera component couldn't be enabled");
	   goto error;
	}
	
	raspicamcontrol_set_all_parameters(camera, &state->camera_parameters);
	
	state->camera_component = camera;
	
	return camera;

error:

   if (camera)
      mmal_component_destroy(camera);

   return 0;
}

/**
 * Destroy the camera component
 *
 * @param state Pointer to state control struct
 *
 */
static void destroy_camera_component(RASPIVID_STATE *state)
{
   if (state->camera_component)
   {
      mmal_component_destroy(state->camera_component);
      state->camera_component = NULL;
   }
}


/**
 * Destroy the encoder component
 *
 * @param state Pointer to state control struct
 *
 */
static void destroy_encoder_component(RASPIVID_STATE *state)
{
   // Get rid of any port buffers first
   if (state->video_pool)
   {
      mmal_port_pool_destroy(state->encoder_component->output[0], state->video_pool);
   }

   if (state->encoder_component)
   {
      mmal_component_destroy(state->encoder_component);
      state->encoder_component = NULL;
   }
}

/**
 * Connect two specific ports together
 *
 * @param output_port Pointer the output port
 * @param input_port Pointer the input port
 * @param Pointer to a mmal connection pointer, reassigned if function successful
 * @return Returns a MMAL_STATUS_T giving result of operation
 *
 */
static MMAL_STATUS_T connect_ports(MMAL_PORT_T *output_port, MMAL_PORT_T *input_port, MMAL_CONNECTION_T **connection)
{
   MMAL_STATUS_T status;

   status =  mmal_connection_create(connection, output_port, input_port, MMAL_CONNECTION_FLAG_TUNNELLING | MMAL_CONNECTION_FLAG_ALLOCATION_ON_INPUT);

   if (status == MMAL_SUCCESS)
   {
      status =  mmal_connection_enable(*connection);
      if (status != MMAL_SUCCESS)
         mmal_connection_destroy(*connection);
   }

   return status;
}

/**
 * Checks if specified port is valid and enabled, then disables it
 *
 * @param port  Pointer the port
 *
 */
static void check_disable_port(MMAL_PORT_T *port)
{
   if (port && port->is_enabled)
      mmal_port_disable(port);
}

RaspiCamCvCapture * raspiCamCvCreateCameraCapture(int index)
{
	RaspiCamCvCapture * capture = (RaspiCamCvCapture*)malloc(sizeof(RaspiCamCvCapture));
	// Our main data storage vessel..
	RASPIVID_STATE * state = (RASPIVID_STATE*)malloc(sizeof(RASPIVID_STATE));
	capture->pState = state;
	
	MMAL_STATUS_T status = -1;
	MMAL_PORT_T *camera_video_port = NULL;
	MMAL_PORT_T *camera_still_port = NULL;

	bcm_host_init();

	// read default status
	default_status(state);

	int w = state->width;
	int h = state->height;
	state->py = cvCreateImage(cvSize(w,h), IPL_DEPTH_8U, 1);		// Y component of YUV I420 frame
	if (state->graymode==0) {
		state->pu = cvCreateImage(cvSize(w/2,h/2), IPL_DEPTH_8U, 1);	// U component of YUV I420 frame
		state->pv = cvCreateImage(cvSize(w/2,h/2), IPL_DEPTH_8U, 1);	// V component of YUV I420 frame
	}
	vcos_semaphore_create(&state->capture_sem, "Capture-Sem", 0);
	vcos_semaphore_create(&state->capture_done_sem, "Capture-Done-Sem", 0);

	if (state->graymode==0) {
		state->pu_big = cvCreateImage(cvSize(w,h), IPL_DEPTH_8U, 1);
		state->pv_big = cvCreateImage(cvSize(w,h), IPL_DEPTH_8U, 1);
		state->yuvImage = cvCreateImage(cvSize(w,h), IPL_DEPTH_8U, 3);
		state->dstImage = cvCreateImage(cvSize(w,h), IPL_DEPTH_8U, 3); // final picture to display
	}

	// create camera
	if (!create_camera_component(state))
	{
	   vcos_log_error("%s: Failed to create camera component", __func__);
	   raspiCamCvReleaseCapture(&capture);
	   return NULL;
	}

	camera_video_port = state->camera_component->output[MMAL_CAMERA_VIDEO_PORT];
	camera_still_port = state->camera_component->output[MMAL_CAMERA_CAPTURE_PORT];

	// assign data to use for callback
	camera_video_port->userdata = (struct MMAL_PORT_USERDATA_T *)state;

	// start capture
	if (mmal_port_parameter_set_boolean(camera_video_port, MMAL_PARAMETER_CAPTURE, 1) != MMAL_SUCCESS)
	{
	   vcos_log_error("%s: Failed to start capture", __func__);
	   raspiCamCvReleaseCapture(&capture);
	   return NULL;
	}

	// Send all the buffers to the video port
		
	int num = mmal_queue_length(state->video_pool->queue);
	int q;
	for (q = 0; q < num; q++)
	{
		MMAL_BUFFER_HEADER_T *buffer = mmal_queue_get(state->video_pool->queue);
		
		if (!buffer)
			vcos_log_error("Unable to get a required buffer %d from pool queue", q);
		
		if (mmal_port_send_buffer(camera_video_port, buffer)!= MMAL_SUCCESS)
			vcos_log_error("Unable to send a buffer to encoder output port (%d)", q);
	}

	//mmal_status_to_int(status);	
		
	// Disable all our ports that are not handled by connections
	//check_disable_port(camera_still_port);

	//if (status != 0)
	//	raspicamcontrol_check_configuration(128);

	vcos_semaphore_wait(&state->capture_done_sem);
	return capture;
}

void raspiCamCvReleaseCapture(RaspiCamCvCapture ** capture)
{
	RASPIVID_STATE * state = (*capture)->pState;

	// Unblock the the callback.
	state->finished = 1;
	vcos_semaphore_post(&state->capture_sem);
	vcos_semaphore_wait(&state->capture_done_sem);

	vcos_semaphore_delete(&state->capture_sem);
	vcos_semaphore_delete(&state->capture_done_sem);

	if (state->camera_component)
		mmal_component_disable(state->camera_component);

	destroy_camera_component(state);

	cvReleaseImage(&state->pu);
	if (state->graymode==0) {
		cvReleaseImage(&state->pv);
		cvReleaseImage(&state->py);
	}

	if (state->graymode==0) {
		cvReleaseImage(&state->pu_big);
		cvReleaseImage(&state->pv_big);
		cvReleaseImage(&state->yuvImage);
		cvReleaseImage(&state->dstImage);
	}

	free(state);
	free(*capture);
	*capture = 0;
}

void raspiCamCvSetCaptureProperty(RaspiCamCvCapture * capture, int property_id, double value)
{
}

IplImage * raspiCamCvQueryFrame(RaspiCamCvCapture * capture)
{
	RASPIVID_STATE * state = capture->pState;
	vcos_semaphore_post(&state->capture_sem);
	vcos_semaphore_wait(&state->capture_done_sem);

	if (state->graymode==0)
	{
		cvResize(state->pu, state->pu_big, CV_INTER_NN);
		cvResize(state->pv, state->pv_big, CV_INTER_NN);  //CV_INTER_LINEAR looks better but it's slower
		cvMerge(state->py, state->pu_big, state->pv_big, NULL, state->yuvImage);
	
		cvCvtColor(state->yuvImage,state->dstImage,CV_YCrCb2RGB);	// convert in RGB color space (slow)
		return state->dstImage;
	}
	return state->py;
}
