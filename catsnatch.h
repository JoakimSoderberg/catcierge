
#ifndef __CATSNATCH_H__
#define __CATSNATCH_H__

#include <opencv2/imgproc/imgproc_c.h>

#ifdef WITH_RFID
#include "catsnatch_rfid.h"
#endif

typedef struct catsnatch_s
{
	CvMemStorage* storage;
	IplImage* snout;
	IplConvKernel *kernel;

	#ifdef WITH_RFID
	
	#endif // WITH_RFID
} catsnatch_t;

int catsnatch_init(catsnatch_t *ctx, const char *snout_path);

void catsnatch_destroy(catsnatch_t *ctx);

double catsnatch_match(catsnatch_t *ctx, const IplImage *img, CvRect *match_rect);

int catsnatch_is_matchable(catsnatch_t *ctx, IplImage *img);

#endif // __CATSNATCH_H__
