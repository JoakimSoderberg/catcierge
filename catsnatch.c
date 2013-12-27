
#include "catsnatch.h"
#include <opencv2/imgproc/imgproc_c.h>
#include <opencv2/highgui/highgui_c.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>

static int _catsnatch_prepare_img(catsnatch_t *ctx, const IplImage *src, IplImage *dst)
{
	IplImage *tmp = cvCreateImage(cvGetSize(src), 8, 1);
	IplImage *tmp2 = cvCreateImage(cvGetSize(src), 8, 1);
	assert(ctx);
	assert(ctx->snout);
	assert(ctx->kernel);
	assert(src != dst);

	cvCvtColor(src, tmp, CV_BGR2GRAY);
	cvThreshold(tmp, tmp2, 35, 255, CV_THRESH_BINARY);
	cvErode(tmp2, dst, ctx->kernel, 3);

	cvReleaseImage(&tmp);
	cvReleaseImage(&tmp2);

	return 0;
}

int catsnatch_init(catsnatch_t *ctx, const char *snout_path)
{
	IplImage *snout_prep = NULL;
	assert(ctx);
	memset(ctx, 0, sizeof(catsnatch_t));

	if (access(snout_path, F_OK) == -1)
	{
		fprintf(stderr, "No such file %s\n", snout_path);
		return -1;
	}
	
	if (!(ctx->storage = cvCreateMemStorage(0)))
	{
		return -1;
	}

	if (!(ctx->kernel = cvCreateStructuringElementEx(3, 3, 0, 0, CV_SHAPE_RECT, NULL)))
	{
		return -1;
	}

	if (!(snout_prep = cvLoadImage(snout_path, 1)))
	{
		fprintf(stderr, "Failed to load snout image: %s\n", snout_path);
		return -1;
	}

	if (!(ctx->snout = cvCreateImage(cvGetSize(snout_prep), 8, 1)))
	{
		cvReleaseImage(&snout_prep);
		return -1;
	}

	if (_catsnatch_prepare_img(ctx, snout_prep, ctx->snout))
	{
		fprintf(stderr, "Failed to prepare snout image: %s\n", snout_path);
		return -1;
	}

	cvReleaseImage(&snout_prep);

	return 0;
}

void catsnatch_destroy(catsnatch_t *ctx)
{
	assert(ctx);

	if (ctx->snout)
	{
		cvReleaseImage(&ctx->snout);
		ctx->snout = NULL;
	}

	if (ctx->storage)
	{
		cvReleaseMemStorage(&ctx->storage);
		ctx->storage = NULL;
	}

	if (ctx->kernel)
	{
		cvReleaseStructuringElement(&ctx->kernel);
		ctx->kernel = NULL;
	}
}

int catsnatch_match(catsnatch_t *ctx, const IplImage *img, CvRect *match_rect)
{
	int res = 0;
	IplImage *img_cpy = NULL;
	IplImage *img_prep = NULL;
	IplImage *matchres = NULL;
	CvPoint min_loc;
	CvPoint max_loc;
	CvSize img_size = cvGetSize(img);
	CvSize snout_size = cvGetSize(ctx->snout);
	CvSize matchres_size;
	double min_val;
	double max_val;
	assert(ctx);

	img_cpy = cvCreateImage(img_size, 8, 1);

	if (!(img_prep = cvCloneImage(img)))
	{
		fprintf(stderr, "Failed to clone match image\n");
		return -1;
	}

	if (_catsnatch_prepare_img(ctx, img_prep, img_cpy))
	{
		fprintf(stderr, "Failed to prepare match image\n");
		cvReleaseImage(&img_cpy);
		cvReleaseImage(&img_prep);
		return -1;
	}

	// Try to match the snout with the image.
	// If we find it, the max_val should be close to 1.0
	matchres_size = cvSize(img_size.width - snout_size.width + 1, 
						   img_size.height - snout_size.height + 1);
	matchres = cvCreateImage(matchres_size, IPL_DEPTH_32F, 1);
	cvMatchTemplate(img_cpy, ctx->snout, matchres, CV_TM_CCOEFF_NORMED);
	cvMinMaxLoc(matchres, &min_val, &max_val, &min_loc, &max_loc, NULL);

	// From testing this seems like a good threshold.
	if (max_val >= 0.9)
	{
		// Match. No prey in the cats mouth!
		res = 1;
	}
	else
	{
		// No Match. Either the cat has prey or it's something else.
		res = 0;
	}

	*match_rect = cvRect(max_loc.x, max_loc.y, snout_size.width, snout_size.height);

	cvReleaseImage(&img_cpy);
	cvReleaseImage(&img_prep);

	return res;
}

