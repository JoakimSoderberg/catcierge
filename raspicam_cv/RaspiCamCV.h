#ifndef __RaspiCamCV__
#define __RaspiCamCV__

#ifdef __cplusplus
extern "C" {
#endif

#include "interface/mmal/mmal.h"
#include "interface/mmal/mmal_logging.h"
#include "interface/mmal/mmal_buffer.h"
#include "interface/mmal/util/mmal_util.h"
#include "interface/mmal/util/mmal_util_params.h"
#include "interface/mmal/util/mmal_default_components.h"
#include "interface/mmal/util/mmal_connection.h"
#include "RaspiCamControl.h"

typedef struct _RASPIVID_STATE RASPIVID_STATE;

typedef struct 
{
	int width;                          /// Requested width of image
	int height;                         /// requested height of image
	int bitrate;                        /// Requested bitrate
	int framerate;                      /// Requested frame rate (fps)
	int graymode;			            /// capture in gray only (2x faster)
	int immutableInput;
	RASPICAM_CAMERA_PARAMETERS camera_parameters;
} RASPIVID_SETTINGS;

typedef struct 
{
	RASPIVID_STATE * pState;
} RaspiCamCvCapture;

typedef struct _IplImage IplImage;

void raspiCamCvSetDefaultCameraParameters(RASPICAM_CAMERA_PARAMETERS *params);

RaspiCamCvCapture * raspiCamCvCreateCameraCapture(int index);
RaspiCamCvCapture * raspiCamCvCreateCameraCaptureEx(int index, RASPIVID_SETTINGS *settings);
void raspiCamCvReleaseCapture(RaspiCamCvCapture ** capture);
void raspiCamCvSetCaptureProperty(RaspiCamCvCapture * capture, int property_id, double value);
IplImage * raspiCamCvQueryFrame(RaspiCamCvCapture * capture);

#ifdef __cplusplus
}
#endif

#endif
