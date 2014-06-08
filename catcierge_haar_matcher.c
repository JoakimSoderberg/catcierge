
#include <assert.h>
#include "catcierge_haar_matcher.h"
#include "catcierge_haar_wrapper.h"
#include "catcierge_types.h"
#include "catcierge_util.h"

int catcierge_haar_matcher_init(catcierge_haar_matcher_t *ctx, catcierge_haar_matcher_args_t *args)
{
	assert(args);
	assert(ctx);

	if (!args->cascade)
	{
		return -1;
	}

	if (!(ctx->cascade = cv2CascadeClassifier_create()))
	{
		return -1;
	}

	if (cv2CascadeClassifier_load(ctx->cascade, args->cascade))
	{
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

	if (!(ctx->kernel_tall = cvCreateStructuringElementEx(5, 1, 0, 0, CV_SHAPE_RECT, NULL)))
	{
		return -1;
	}

	ctx->args = args;
	ctx->debug = args->debug;

	return 0;
}

void catcierge_haar_matcher_destroy(catcierge_haar_matcher_t *ctx)
{
	assert(ctx);
	if (ctx->cascade)
	{
		cv2CascadeClassifier_destroy(ctx->cascade);
		ctx->cascade = NULL;
	}

	// TODO: We might have to release the xml as well..

	if (ctx->storage)
	{
		cvReleaseMemStorage(&ctx->storage);
		ctx->storage = NULL;
	}
}

match_direction_t catcierge_haar_guess_direction(catcierge_haar_matcher_t *ctx, IplImage *img)
{
	assert(ctx);
	assert(ctx->args);
	int left_sum;
	int right_sum;
	catcierge_haar_matcher_args_t *args = ctx->args;
	match_direction_t dir = MATCH_DIR_UNKNOWN;
	CvRect roi = cvGetImageROI(img);

	// Left.
	cvSetImageROI(img, cvRect(0, 0, 1, roi.height));
	left_sum = (int)cvSum(img).val[0];

	// Right.
	cvSetImageROI(img, cvRect(roi.width - 1, 0, 1, roi.height));
	right_sum = (int)cvSum(img).val[0];

	if (abs(left_sum - right_sum) > 25)
	{
		if (ctx->debug) printf("Left: %d, Right: %d\n", left_sum, right_sum);

		if (right_sum > left_sum)
		{
			// Going right.
			dir = (args->in_direction == DIR_RIGHT) ? MATCH_DIR_IN : MATCH_DIR_OUT;
		}
		else
		{
			// Going left.
			dir = (args->in_direction == DIR_LEFT) ? MATCH_DIR_IN : MATCH_DIR_OUT;
		}
	}

	cvSetImageROI(img, roi);

	return dir;
}

size_t catcierge_haar_matcher_count_contours(catcierge_haar_matcher_t *ctx, CvSeq *contours)
{
	assert(ctx);
	assert(ctx->args);
	size_t contour_count = 0;
	double area;
	int big_enough = 0;
	CvSeq *it = NULL;
	catcierge_haar_matcher_args_t *args = ctx->args;

	if (!contours)
		return 0;

	it = contours;
	while (it)
	{
		area = cvContourArea(it, CV_WHOLE_SEQ, 0);
		big_enough = (area > 10.0);
		if (ctx->debug) printf("Area: %f %s\n", area, big_enough ? "" : "(too small)");

		if (big_enough)
		{
			contour_count++;
		}

		it = it->h_next;
	}

	return contour_count;
}

int catcierge_haar_matcher_find_prey(catcierge_haar_matcher_t *ctx, IplImage *img)
{
	assert(ctx);
	assert(img);
	assert(ctx->args);
	catcierge_haar_matcher_args_t *args = ctx->args;
	IplImage *thr_img = NULL;
	IplImage *thr_img2 = NULL;
	CvSeq *contours = NULL;
	size_t contour_count = 0;

	thr_img = cvCreateImage(cvGetSize(img), 8, 1);
	cvThreshold(img, thr_img,
				0, 255,
				CV_THRESH_OTSU);

	thr_img2 = cvCloneImage(thr_img);

	if (ctx->debug) cvShowImage("Haar image th", thr_img);

	cvFindContours(thr_img, ctx->storage, &contours,
		sizeof(CvContour), CV_RETR_LIST, CV_CHAIN_APPROX_NONE, cvPoint(0, 0));

	contour_count = catcierge_haar_matcher_count_contours(ctx, contours);

	if ((args->prey_steps >= 2) && (contour_count == 1))
	{
		IplImage *erod_img = NULL;
		IplImage *open_img = NULL;
		CvSeq *contours2 = NULL;

		erod_img = cvCreateImage(cvGetSize(thr_img2), 8, 1);
		cvErode(thr_img2, erod_img, ctx->kernel, 3);
		if (ctx->debug) cvShowImage("haar eroded img", erod_img);

		open_img = cvCreateImage(cvGetSize(thr_img2), 8, 1);
		cvMorphologyEx(erod_img, open_img, NULL, ctx->kernel_tall, CV_MOP_OPEN, 1);
		if (ctx->debug) cvShowImage("haar opened img", erod_img);

		cvFindContours(erod_img, ctx->storage, &contours2,
			sizeof(CvContour), CV_RETR_LIST, CV_CHAIN_APPROX_NONE, cvPoint(0, 0));
		cvReleaseImage(&erod_img);
		cvReleaseImage(&open_img);

		contour_count = catcierge_haar_matcher_count_contours(ctx, contours2);
	}

	if (ctx->debug)
	{
		cvDrawContours(img, contours, cvScalarAll(0), cvScalarAll(0), 1, 1, 8, cvPoint(0, 0));
		cvShowImage("Haar Contours", img);
	}

	cvReleaseImage(&thr_img);
	return (contour_count > 1);
}

double catcierge_haar_matcher_match(catcierge_haar_matcher_t *ctx, IplImage *img,
		CvRect *match_rects, size_t *rect_count, match_direction_t *direction)
{
	assert(ctx);
	assert(ctx->args);
	catcierge_haar_matcher_args_t *args = ctx->args;
	double ret = 1.0;
	IplImage *img_eq = NULL;
	IplImage *img_gray = NULL;
	IplImage *tmp = NULL;
	CvSize max_size;
	CvSize min_size;
	min_size.width = 80;
	min_size.height = 80;
	max_size.width = 0;
	max_size.height = 0;

	// Make gray scale if needed.
	if (img->nChannels != 1)
	{
		tmp = cvCreateImage(cvGetSize(img), 8, 1);
		cvCvtColor(img, tmp, CV_BGR2GRAY);
		img_gray = tmp;
	}
	else
	{
		img_gray = img;
	}

	if (direction)
	{
		*direction = MATCH_DIR_UNKNOWN;
	}

	// Equalize histogram.
	if (args->eq_histogram)
	{
		img_eq = cvCreateImage(cvGetSize(img), 8, 1);
		cvEqualizeHist(img_gray, img_eq);
	}
	else
	{
		img_eq = img_gray;
	}

	if (cv2CascadeClassifier_detectMultiScale(ctx->cascade,
			img_eq, match_rects, rect_count,
			1.1, 3, CV_HAAR_SCALE_IMAGE, &min_size, &max_size))
	{
		ret = -1.0;
		goto fail;
	}

	if (ctx->debug) printf("Rect count: %zu\n", *rect_count);

	// Even if we don't find a face we count it as a success.
	// Only when a prey is found we consider it a fail.
	// Unless args->no_match_is_fail is set.
	if (args->no_match_is_fail)
	{
		ret = (*rect_count > 0) ? 1.0 : 0.0;
	}

	if ((*rect_count > 0))
	{
		CvRect roi = match_rects[0];

		// TODO: Break out into a function.
		// Limit the roi to the lower part where the prey might be.
		// (This gets rid of some false positives)
		roi.height /= 2;
		roi.y += roi.height;

		// Extend the rect a bit towards the outside.
		// This way for big mice and such we still get some white on each side of it.
		roi.width += 30;
		roi.x = roi.x + ((args->in_direction == DIR_RIGHT) ? -30 : 30);
		if (roi.x < 0) roi.x = 0;

		cvSetImageROI(img_eq, roi);

		// TODO: Is this more effective before or after ROI is set?
		if (direction)
		{
			*direction = catcierge_haar_guess_direction(ctx, img_eq);

			if (ctx->debug) printf("Direction: ");
			switch (*direction)
			{
				case MATCH_DIR_IN:
				{
					if (ctx->debug) printf("IN\n"); 
					break;
				}
				case MATCH_DIR_OUT:
				{
					if (ctx->debug) printf("OUT\n");
					// Don't bother looking for prey when the cat
					// is going outside.
					goto done;
				}
				default: if (ctx->debug) printf("Unknown\n"); break;
			}
		}

		if (catcierge_haar_matcher_find_prey(ctx, img_eq))
		{
			if (ctx->debug) printf("Found prey!\n");
			ret = 0.0; // Fail.
		}
		else
		{
			ret = 1.0; // Success.
		}
	}
done:
fail:
	cvResetImageROI(img);

	if (args->eq_histogram)
	{
		cvReleaseImage(&img_eq);
	}

	if (tmp)
	{
		cvReleaseImage(&tmp);
	}

	return ret;
}

int catcierge_haar_matcher_parse_args(catcierge_haar_matcher_args_t *args, const char *key, char **values, size_t value_count)
{
	printf("Parse: %s %s", key, values[0]);
	if (!strcmp(key, "cascade"))
	{
		if (value_count == 1)
		{
			args->cascade = values[0];
		}
		else
		{
			fprintf(stderr, "Missing value for --cascade\n");
			return -1;
		}

		return 0;
	}

	if (!strcmp(key, "min_size"))
	{
		if (value_count == 1)
		{
			if (sscanf(values[0], "%dx%d", &args->min_width, &args->min_height) == EOF)
			{
				fprintf(stderr, "Invalid format for --min_size \"%s\"\n", values[0]);
				return -1;
			}
		}
		else
		{
			fprintf(stderr, "Missing value for --min_size\n");
			return -1;
		}

		return 0;
	}

	if (!strcmp(key, "no_match_is_fail"))
	{
		args->no_match_is_fail = 1;
		return 0;
	}

	if (!strcmp(key, "equalize_historgram") || !strcmp(key, "eqhist"))
	{
		args->eq_histogram = 1;
		return 0;
	}

	if (!strcmp(key, "prey_steps"))
	{
		if (value_count == 1)
		{
			args->prey_steps = atoi(values[0]);
		}
		else
		{
			fprintf(stderr, "Missing value for --prey_steps\n");
			return -1;
		}

		return 0;
	}

	if (!strcmp(key, "in_direction"))
	{
		if (value_count == 1)
		{
			char *d = values[0];
			if (!strcasecmp(d, "left"))
			{
				args->in_direction = DIR_LEFT;
			}
			else if (!strcasecmp(d, "right"))
			{
				args->in_direction = DIR_RIGHT;
			}
			else
			{
				fprintf(stderr, "Invalid direction \"%s\"\n", d);
			}
		}
		else
		{
			fprintf(stderr, "Missing value for --prey_steps\n");
			return -1;
		}

		return 0;
	}

	return 1;
}

void catcierge_haar_matcher_usage()
{
	fprintf(stderr, " --cascade <path>       Path to the haar cascade xml generated by opencv_traincascade.\n");
	fprintf(stderr, " --in_direction <left|right>\n");
	fprintf(stderr, "                        The direction which is considered going inside.\n");
	fprintf(stderr, " --min_size <WxH>       The size of the minimum.\n");
	fprintf(stderr, " --no_match_is_fail     If no cat head is found in the picture, consider this a failure.\n");
	fprintf(stderr, "                        The default is to only consider found prey a failure.\n");
	fprintf(stderr, " --eq_histogram         Equalize the histogram of the image before doing.\n");
	fprintf(stderr, "                        the haar cascade detection step.\n");
	fprintf(stderr, " --prey_steps           The algorithm for finding the prey \n");
	fprintf(stderr, "\n");
}

void catcierge_haar_matcher_print_settings(catcierge_haar_matcher_args_t *args)
{
	assert(args);
	printf("Haar Cascade Matcher:\n");
	printf("           Cascade: %s\n", args->cascade);
	printf("      In direction: %s\n", (args->in_direction == DIR_LEFT) ? "Left" : "Right");
	printf("          Min size: %dx%d\n", args->min_width, args->min_height);
	printf("Equalize histogram: %d\n", args->eq_histogram);
	printf("  No match is fail: %d\n", args->no_match_is_fail);
	printf("        Prey steps: %d\n", args->prey_steps);
	printf("\n");
}

void catcierge_haar_matcher_args_init(catcierge_haar_matcher_args_t *args)
{
	assert(args);
	memset(args, 0, sizeof(catcierge_haar_matcher_args_t));
	args->min_width = 80;
	args->min_height = 80;
	args->in_direction = DIR_RIGHT;
	args->eq_histogram = 0;
	args->debug = 0;
	args->no_match_is_fail = 0;
	args->prey_steps = 2;
}

void catcierge_haar_matcher_set_debug(catcierge_haar_matcher_t *ctx, int debug)
{
	assert(ctx);
	ctx->debug = debug;
}
