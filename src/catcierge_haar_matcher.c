//
// This file is part of the Catcierge project.
//
// Copyright (c) Joakim Soderberg 2013-2015
//
//    Catcierge is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 2 of the License, or
//    (at your option) any later version.
//
//    Catcierge is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with Catcierge.  If not, see <http://www.gnu.org/licenses/>.
//
#include <assert.h>
#include "catcierge_haar_matcher.h"
#include "catcierge_haar_wrapper.h"
#include "catcierge_types.h"
#include "catcierge_util.h"
#include "catcierge_log.h"
#include <opencv2/core/core_c.h>
#include "cargo.h"

int catcierge_haar_matcher_init(catcierge_matcher_t **octx,
		catcierge_matcher_args_t *oargs)
{
	catcierge_haar_matcher_t *ctx = NULL;
	catcierge_haar_matcher_args_t *args = (catcierge_haar_matcher_args_t *)oargs;
	assert(args);
	assert(octx);

	if (!(*octx = calloc(1, sizeof(catcierge_haar_matcher_t))))
	{
		CATERR("Out of memory!\n");
		return -1;
	}

	ctx = (catcierge_haar_matcher_t *)*octx;

	ctx->super.type = MATCHER_HAAR;
	ctx->super.name = "Haar Cascade";
	ctx->super.short_name = "haar";

	if (!args->cascade)
	{
		CATERR("Haar matcher: No cascade xml specified. Use --cascade\n");
		return -1;
	}

	if (!(ctx->cascade = cv2CascadeClassifier_create()))
	{
		CATERR("Failed to create cascade classifier.\n");
		goto opencv_error;
	}

	if (cv2CascadeClassifier_load(ctx->cascade, args->cascade))
	{
		CATERR("Failed to load cascade xml: %s\n", args->cascade);
		return -1;
	}

	if (!(ctx->storage = cvCreateMemStorage(0)))
	{
		goto opencv_error;
	}

	if (!(ctx->kernel2x2 = cvCreateStructuringElementEx(2, 2, 0, 0, CV_SHAPE_RECT, NULL)))
	{
		goto opencv_error;
	}

	if (!(ctx->kernel3x3 = cvCreateStructuringElementEx(3, 3, 0, 0, CV_SHAPE_RECT, NULL)))
	{
		goto opencv_error;
	}

	if (!(ctx->kernel5x1 = cvCreateStructuringElementEx(5, 1, 0, 0, CV_SHAPE_RECT, NULL)))
	{
		goto opencv_error;
	}

	ctx->args = args;
	ctx->super.debug = args->debug;
	ctx->super.match = catcierge_haar_matcher_match;
	ctx->super.decide = catcierge_haar_matcher_decide;
	ctx->super.translate = catcierge_haar_matcher_translate;

	return 0;
opencv_error:
	CATERR("OpenCV error\n");
	return -1;
}

void catcierge_haar_matcher_destroy(catcierge_matcher_t **octx)
{
	catcierge_haar_matcher_t *ctx;

	if (!octx || !(*octx))
		return;

	ctx = (catcierge_haar_matcher_t *)*octx;

	if (ctx->cascade)
	{
		cv2CascadeClassifier_destroy(ctx->cascade);
		ctx->cascade = NULL;
	}

	if (ctx->kernel2x2)
	{
		cvReleaseStructuringElement(&ctx->kernel2x2);
		ctx->kernel2x2 = NULL;
	}

	if (ctx->kernel3x3)
	{
		cvReleaseStructuringElement(&ctx->kernel3x3);
		ctx->kernel3x3 = NULL;
	}

	if (ctx->kernel5x1)
	{
		cvReleaseStructuringElement(&ctx->kernel5x1);
		ctx->kernel5x1 = NULL;
	}

	if (ctx->storage)
	{
		cvReleaseMemStorage(&ctx->storage);
		ctx->storage = NULL;
	}

	free(ctx);
	*octx = NULL;
}

match_direction_t catcierge_haar_guess_direction(catcierge_haar_matcher_t *ctx, IplImage *thr_img, int inverted)
{
	int left_sum;
	int right_sum;
	catcierge_haar_matcher_args_t *args = ctx->args;
	match_direction_t dir = MATCH_DIR_UNKNOWN;
	CvRect roi = cvGetImageROI(thr_img);
	assert(ctx);
	assert(ctx->args);

	// Left.
	cvSetImageROI(thr_img, cvRect(0, 0, 1, roi.height));
	left_sum = (int)cvSum(thr_img).val[0];

	// Right.
	cvSetImageROI(thr_img, cvRect(roi.width - 1, 0, 1, roi.height));
	right_sum = (int)cvSum(thr_img).val[0];

	if (abs(left_sum - right_sum) > 25)
	{
		if (ctx->super.debug) printf("Left: %d, Right: %d\n", left_sum, right_sum);

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

	cvSetImageROI(thr_img, roi);

	if (inverted && (dir != MATCH_DIR_UNKNOWN))
	{
		if (dir == MATCH_DIR_IN) return MATCH_DIR_OUT;
		return MATCH_DIR_IN;
	}
	else
	{
		return dir;
	}
}

size_t catcierge_haar_matcher_count_contours(catcierge_haar_matcher_t *ctx, CvSeq *contours)
{
	size_t contour_count = 0;
	double area;
	int big_enough = 0;
	CvSeq *it = NULL;
	assert(ctx);
	assert(ctx->args);

	if (!contours)
		return 0;

	it = contours;
	while (it)
	{
		area = cvContourArea(it, CV_WHOLE_SEQ, 0);
		big_enough = (area > 10.0);
		if (ctx->super.debug) printf("Area: %f %s\n", area, big_enough ? "" : "(too small)");

		if (big_enough)
		{
			contour_count++;
		}

		it = it->h_next;
	}

	return contour_count;
}

void catcierge_haar_matcher_save_step_image(catcierge_haar_matcher_t *ctx,
											IplImage *img, match_result_t *result,
											const char *name, const char *description,
											int save)
{
	match_step_t *step = NULL;
	CvSize img_size;
	CvRect roi;
	assert(result->step_img_count < MAX_STEPS);

	if (ctx->super.debug)
		cvShowImage(description, img);

	step = &result->steps[result->step_img_count];

	if (step->img)
	{
		cvReleaseImage(&step->img);
		step->img = NULL;
	}

	// If saving steps is turned off, simply release
	// any previous step image.
	if (!save)
		return;

	// We only want to copy the Region Of Interest (ROI).
	roi = cvGetImageROI(img);
	img_size.width = roi.width;
	img_size.height = roi.height;

	step->img = cvCreateImage(img_size, 8, img->nChannels);
	cvCopy(img, step->img, NULL);

	step->name = name;
	step->description = description;

	result->step_img_count++;
}

int catcierge_haar_matcher_find_prey_adaptive(catcierge_haar_matcher_t *ctx,
											IplImage *img, IplImage *inv_thr_img,
											match_result_t *result, int save_steps)
{
	IplImage *inv_adpthr_img = NULL;
	IplImage *inv_combined = NULL;
	IplImage *open_combined = NULL;
	IplImage *dilate_combined = NULL;
	CvSeq *contours = NULL;
	size_t contour_count = 0;
	CvSize img_size;
	assert(ctx);
	assert(img);
	assert(ctx->args);

	img_size = cvGetSize(img);

	// We expect to be given an inverted global thresholded image (inv_thr_img)
	// that contains the rough cat profile.

	// Do an inverted adaptive threshold of the original image as well.
	// This brings out small details such as a mouse tail that fades
	// into the background during a global threshold.
	inv_adpthr_img = cvCreateImage(img_size, 8, 1);
	cvAdaptiveThreshold(img, inv_adpthr_img, 255,
		CV_ADAPTIVE_THRESH_GAUSSIAN_C, CV_THRESH_BINARY_INV, 11, 5);
	catcierge_haar_matcher_save_step_image(ctx,
		inv_adpthr_img, result, "adp_thresh", "Inverted adaptive threshold", save_steps);

	// Now we can combine the two thresholded images into one.
	inv_combined = cvCreateImage(img_size, 8, 1);
	cvAdd(inv_thr_img, inv_adpthr_img, inv_combined, NULL);
	catcierge_haar_matcher_save_step_image(ctx,
		inv_combined, result, "inv_combined", "Combined global and adaptive threshold", save_steps);

	// Get rid of noise from the adaptive threshold.
	open_combined = cvCreateImage(img_size, 8, 1);
	cvMorphologyEx(inv_combined, open_combined, NULL, ctx->kernel2x2, CV_MOP_OPEN, 2);
	catcierge_haar_matcher_save_step_image(ctx,
		open_combined, result, "opened", "Opened image", save_steps);

	dilate_combined = cvCreateImage(img_size, 8, 1);
	cvDilate(open_combined, dilate_combined, ctx->kernel3x3, 3);
	catcierge_haar_matcher_save_step_image(ctx,
		dilate_combined, result, "dilated", "Dilated image", save_steps);

	// Invert back the result so the background is white again.
	cvNot(dilate_combined, dilate_combined);
	catcierge_haar_matcher_save_step_image(ctx,
		dilate_combined, result, "combined", "Combined binary image", save_steps);

	cvFindContours(dilate_combined, ctx->storage, &contours,
		sizeof(CvContour), CV_RETR_LIST, CV_CHAIN_APPROX_NONE, cvPoint(0, 0));

	// If we get more than 1 contour we count it as a prey.
	contour_count = catcierge_haar_matcher_count_contours(ctx, contours);

	if (save_steps)
	{
		IplImage *img_contour = cvCloneImage(img);
		IplImage *img_final_color = NULL;
		CvScalar color;

		cvDrawContours(img_contour, contours, cvScalarAll(255), cvScalarAll(0), 1, 1, 8, cvPoint(0, 0));
		catcierge_haar_matcher_save_step_image(ctx,
			img_contour, result, "contours", "Background contours", save_steps);

		// Draw a final color combined image with the Haar detection + contour.
		cvResetImageROI(img_contour);

		img_final_color =  cvCreateImage(cvGetSize(img_contour), 8, 3);

		cvCvtColor(img_contour, img_final_color, CV_GRAY2BGR);
		color = (contour_count > 1) ? CV_RGB(255, 0, 0) : CV_RGB(0, 255, 0);
		cvRectangleR(img_final_color, result->match_rects[0], color, 2, 8, 0);

		catcierge_haar_matcher_save_step_image(ctx,
			img_final_color, result, "final", "Final image", save_steps);

		cvReleaseImage(&img_contour);
		cvReleaseImage(&img_final_color);
	}

	cvReleaseImage(&inv_adpthr_img);
	cvReleaseImage(&inv_combined);
	cvReleaseImage(&open_combined);
	cvReleaseImage(&dilate_combined);

	return (contour_count > 1);
}

int catcierge_haar_matcher_find_prey(catcierge_haar_matcher_t *ctx,
									IplImage *img, IplImage *thr_img,
									match_result_t *result, int save_steps)
{
	catcierge_haar_matcher_args_t *args = ctx->args;
	IplImage *thr_img2 = NULL;
	CvSeq *contours = NULL;
	size_t contour_count = 0;
	assert(ctx);
	assert(img);
	assert(ctx->args);

	// thr_img is modified by FindContours so we clone it first.
	thr_img2 = cvCloneImage(thr_img);

	cvFindContours(thr_img, ctx->storage, &contours,
		sizeof(CvContour), CV_RETR_LIST, CV_CHAIN_APPROX_NONE, cvPoint(0, 0));

	// If we get more than 1 contour we count it as a prey. At least something
	// is intersecting the white are to split up the image.
	contour_count = catcierge_haar_matcher_count_contours(ctx, contours);

	// If we don't find any prey 
	if ((args->prey_steps >= 2) && (contour_count == 1))
	{
		IplImage *erod_img = NULL;
		IplImage *open_img = NULL;
		CvSeq *contours2 = NULL;

		erod_img = cvCreateImage(cvGetSize(thr_img2), 8, 1);
		cvErode(thr_img2, erod_img, ctx->kernel3x3, 3);
		if (ctx->super.debug) cvShowImage("haar eroded img", erod_img);

		open_img = cvCreateImage(cvGetSize(thr_img2), 8, 1);
		cvMorphologyEx(erod_img, open_img, NULL, ctx->kernel5x1, CV_MOP_OPEN, 1);
		if (ctx->super.debug) cvShowImage("haar opened img", erod_img);

		cvFindContours(erod_img, ctx->storage, &contours2,
			sizeof(CvContour), CV_RETR_LIST, CV_CHAIN_APPROX_NONE, cvPoint(0, 0));
		cvReleaseImage(&erod_img);
		cvReleaseImage(&open_img);

		contour_count = catcierge_haar_matcher_count_contours(ctx, contours2);
	}

	if (ctx->super.debug)
	{
		cvDrawContours(img, contours, cvScalarAll(0), cvScalarAll(0), 1, 1, 8, cvPoint(0, 0));
		cvShowImage("Haar Contours", img);
	}

	cvReleaseImage(&thr_img2);

	return (contour_count > 1);
}

void catcierge_haar_matcher_calculate_roi(catcierge_haar_matcher_t *ctx, CvRect *roi)
{
	// Limit the roi to the lower part where the prey might be.
	// (This gets rid of some false positives)
	roi->height /= 2;
	roi->y += roi->height;

	// Extend the rect a bit towards the outside.
	// This way for big mice and such we still get some white on each side of it.
	// TODO: Make this a commandline argument.
	roi->width += 30;
	roi->x = roi->x + ((ctx->args->in_direction == DIR_RIGHT) ? -30 : 30);
	if (roi->x < 0) roi->x = 0;
}

double catcierge_haar_matcher_match(void *octx,
		IplImage *img, match_result_t *result, int save_steps)
{
	catcierge_haar_matcher_t *ctx = (catcierge_haar_matcher_t *)octx;
	catcierge_haar_matcher_args_t *args = ctx->args;
	double ret = HAAR_SUCCESS_NO_HEAD;
	IplImage *img_eq = NULL;
	IplImage *img_gray = NULL;
	IplImage *tmp = NULL;
	IplImage *thr_img = NULL;
	CvSize max_size;
	CvSize min_size;
	int cat_head_found = 0;
	assert(ctx);
	assert(ctx->args);
	assert(result);

	min_size.width = args->min_width;
	min_size.height = args->min_height;
	max_size.width = 0;
	max_size.height = 0;
	result->step_img_count = 0;
	result->description[0] = '\0';

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

	if (result->direction)
	{
		result->direction = MATCH_DIR_UNKNOWN;
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

	catcierge_haar_matcher_save_step_image(ctx,
		img_eq, result, "gray", "Grayscale original", save_steps);

	result->rect_count = MAX_MATCH_RECTS;

	if (cv2CascadeClassifier_detectMultiScale(ctx->cascade,
			img_eq, result->match_rects, &result->rect_count,
			1.1, 3, CV_HAAR_SCALE_IMAGE, &min_size, &max_size))
	{
		ret = -1.0;
		goto fail;
	}

	if (ctx->super.debug) printf("Rect count: %d\n", (int)result->rect_count);

	cat_head_found = (result->rect_count > 0);

	// Even if we don't find a face we count it as a success.
	// Only when a prey is found we consider it a fail.
	// Unless args->no_match_is_fail is set.
	if (args->no_match_is_fail)
	{
		// Any return value above 0.0 is considered
		// a success. Just so we can distinguish the types of successes.
		ret = cat_head_found ?
			HAAR_SUCCESS_NO_HEAD_IS_FAIL : HAAR_FAIL;
	}

	if (cat_head_found)
	{
		int inverted; 
		int flags;
		CvRect roi;
		find_prey_f find_prey = NULL;

		// Only use the lower part of the region of interest
		// and extend it some towards the "outside" for better result.
		// (We only use the first haar cascade match).
		roi = result->match_rects[0];

		// If we're saving steps, include the original haar cascade
		// match rectangle image.
		if (save_steps)
		{
			cvSetImageROI(img_eq, roi);

			catcierge_haar_matcher_save_step_image(ctx,
				img_eq, result, "haar_roi", "Haar match", save_steps);
		}

		catcierge_haar_matcher_calculate_roi(ctx, &roi);
		cvSetImageROI(img_eq, roi);

		catcierge_haar_matcher_save_step_image(ctx,
				img_eq, result, "roi", "Cropped region of interest", save_steps);

		if (args->prey_method == PREY_METHOD_ADAPTIVE)
		{
			inverted = 1;
			flags = CV_THRESH_BINARY_INV | CV_THRESH_OTSU;
			find_prey = catcierge_haar_matcher_find_prey_adaptive;
		}
		else
		{
			inverted = 0;
			flags = CV_THRESH_BINARY | CV_THRESH_OTSU;
			find_prey = catcierge_haar_matcher_find_prey;
		}

		// Both "find prey" and "guess direction" needs
		// a thresholded image, so perform it before calling those.
		thr_img = cvCreateImage(cvGetSize(img_eq), 8, 1);
		cvThreshold(img_eq, thr_img, 0, 255, flags);
		if (ctx->super.debug) cvShowImage("Haar image binary", thr_img);

		catcierge_haar_matcher_save_step_image(ctx,
			thr_img, result, "thresh", "Global thresholded binary image", save_steps);

		result->direction = catcierge_haar_guess_direction(ctx, thr_img, inverted);
		if (ctx->super.debug) printf("Direction: %s\n", catcierge_get_direction_str(result->direction));

		// Don't bother looking for prey when the cat is going outside.
		if ((result->direction) == MATCH_DIR_OUT)
		{
			if (ctx->super.debug) printf("Skipping prey detection!\n");
			snprintf(result->description, sizeof(result->description) - 1,
				"Skipped prey detection when going out");
			goto done;
		}

		// Note that thr_img will be modified.
		if (find_prey(ctx, img_eq, thr_img, result, save_steps))
		{
			if (ctx->super.debug) printf("Found prey!\n");
			ret = HAAR_FAIL;

			snprintf(result->description, sizeof(result->description) - 1,
				"Prey detected");
		}
		else
		{

			ret = HAAR_SUCCESS;
			snprintf(result->description, sizeof(result->description) - 1,
				"No prey detected");
		}
	}
	else
	{
		snprintf(result->description, sizeof(result->description) - 1,
			"%sNo cat head detected",
			(ret == HAAR_SUCCESS_NO_HEAD_IS_FAIL) ? "Fail ": "");
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

	if (thr_img)
	{
		cvReleaseImage(&thr_img);
	}

	result->result = ret;
	result->success = (result->result > 0.0);

	return ret;
}

int catcierge_haar_matcher_decide(void *ctx, match_group_t *mg)
{
	size_t i;
	size_t no_head_count = 0;
	assert(mg);

	// If no cat is found at all, consider that a FAIL.
	for (i = 0; i < mg->match_count; i++)
	{
		if (mg->matches[i].result.result == HAAR_SUCCESS_NO_HEAD)
		{
			no_head_count++;
		}
	}

	if (no_head_count == mg->match_count)
	{
		snprintf(mg->description, sizeof(mg->description),
			"%s", "No head found in any image");

		mg->final_decision = 1;
		return 0;
	}

	return mg->success;
}

static int parse_in_direction(cargo_t ctx, void *user, const char *optname,
                                 	int argc, char **argv)
{
	direction_t *in_dir = (direction_t *)user;
	char *d = NULL;

	if (argc < 1)
	{
		cargo_set_error(ctx, 0,
			"%s requires 1 argument", optname);
		return -1;
	}

	d = argv[0];

	if (!strcasecmp(d, "left"))
	{
		*in_dir = DIR_LEFT;
	}
	else if (!strcasecmp(d, "right"))
	{
		*in_dir = DIR_RIGHT;
	}
	else
	{
		cargo_set_error(ctx, 0,
			"Invalid direction \"%s\", must be \"left\" or \"right\" "
			"for option %s.", d, optname);
		return -1;
	}

	return 1;
}

static int parse_width_height(cargo_t ctx, void *user, const char *optname,
                              int argc, char **argv)
{
	catcierge_haar_matcher_args_t *args = (catcierge_haar_matcher_args_t *)user;
	int sret = 0;

	if (argc < 1)
	{
		cargo_set_error(ctx, 0,
			"%s requires 1 argument", optname);
		return -1;
	}

	sret = sscanf(argv[0], "%dx%d", &args->min_width, &args->min_height);

	if ((sret == EOF) || (sret != 2))
	{
		cargo_set_error(ctx, 0,
			"Cannot parse %s value \"%s\" expected format: WxH\n", optname, argv[0]);
		return -1;
	}

	return 1;
}

static int parse_prey_method(cargo_t ctx, void *user, const char *optname,
                             int argc, char **argv)
{
	catcierge_haar_prey_method_t *pm = (catcierge_haar_prey_method_t *)user;
	char *d = NULL;

	if (argc < 1)
	{
		cargo_set_error(ctx, 0,
			"Missing either \"adaptive\" or \"normal\" for %s", optname);
		return -1;
	}

	d = argv[0];

	if (!strcasecmp(d, "adaptive"))
	{
		*pm = PREY_METHOD_ADAPTIVE;
	}
	else if (!strcasecmp(d, "normal"))
	{
		*pm = PREY_METHOD_NORMAL;
	}
	else
	{
		cargo_set_error(ctx, 0,
			"Invalid prey method \"%s\", must be \"adaptive\" "
			"or \"normal\".", d);
		return -1;
	}

	return 1;
}

int catcierge_haar_matcher_args_destroy(catcierge_haar_matcher_args_t *args)
{
	catcierge_xfree(&args->cascade);
	return 0;
}

int catcierge_haar_matcher_add_options(cargo_t cargo,
										catcierge_haar_matcher_args_t *args)
{
	int ret = 0;
	assert(cargo);
	assert(args);

	ret |= cargo_add_group(cargo, 0,
			"haar", "Haar cascade matcher settings",
			"Settings for when --haar_matcher is used.\n"
			"This is the recommended matcher type.");

	ret |= cargo_add_option(cargo, 0,
			"<haar> --cascade",
			"Path to the haar cascade xml generated by opencv_traincascade.",
			"s", &args->cascade);

	ret |= cargo_add_option(cargo, 0,
			"<haar> --in_direction",
			"The direction which is considered going inside.",
			"c", parse_in_direction, &args->in_direction);
	ret |= cargo_set_metavar(cargo,
			"--in_direction",
			"LEFT|RIGHT");

	ret |= cargo_add_option(cargo, 0,
			"<haar> --min_size",
			"The size of the minimum sized box that fits the matched cat head.",
			"c", parse_width_height, args);

	ret |= cargo_add_option(cargo, 0,
			"<haar> --no_match_is_fail",
			"If no cat head is found in the picture, consider this a failure. "
			"The default is to only consider found prey a failure.",
			"b", &args->no_match_is_fail);

	ret |= cargo_add_option(cargo, 0,
			"<haar> --equalize_histogram --eqhist",
			"Equalize the histogram of the image before doing. "
			"the haar cascade detection step.",
			"b", &args->eq_histogram);

	ret |= cargo_add_option(cargo, 0,
			"<haar> --prey_steps",
			"Only applicable for normal prey mode. 2 means a secondary "
			"search should be made if no prey is found initially.",
			"i", &args->prey_steps);
	ret |= cargo_set_metavar(cargo,
			"--prey_steps",
			"1|2");
	ret |= cargo_add_validation(cargo, 0, "--prey_steps",
								cargo_validate_int_range(1, 2));

	ret |= cargo_add_option(cargo, 0,
			"<haar> --prey_method",
			"Sets the prey matching method. Adaptive combines the result "
			"of both a global and adaptive thresholding to be better able to "
			"find prey parts otherwise blended into the background. "
			"Normal is simpler and doesn't catch such corner cases as well.",
			"c", parse_prey_method, &args->prey_method);
	ret |= cargo_set_metavar(cargo,
			"--prey_method",
			"ADAPTIVE|NORMAL");

	return ret;
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
	fprintf(stderr, " --prey_method <adaptive|normal>  (Adaptive is default)\n");
	fprintf(stderr, "                        Sets the prey matching method. Adaptive combines the result\n");
	fprintf(stderr, "                        of both a global and adaptive thresholding to be better able to\n");
	fprintf(stderr, "                        find prey parts otherwise blended into the background.\n");
	fprintf(stderr, "                        Normal is simpler and doesn't catch such corner cases as well.\n");
	fprintf(stderr, " --prey_steps <1-2>     Only applicable for normal prey mode. 2 means a secondary\n");
	fprintf(stderr, "                        search should be made if no prey is found initially.\n");
	fprintf(stderr, "\n");
}

catcierge_output_var_t haar_vars[] =
{
	{ "cascade", "Cascade XML given via --cascade." },
	{ "in_direction", "The in direction left or right, same as --in_direction." },
	{ "min_size", "Minimum size of a match in the format WxH. Given by --min_size." },
	{ "min_size_width", "Minimum width of a match. Given --min_size." },
	{ "min_size_height", "Minimum height of a match. Given by --min_size." },
	{ "no_match_is_fail", "Value of --no_match_is_fail." },
	{ "eq_histogram", "Value of --eq_histogram." },
	{ "prey_method", "Value of --prey_method." },
	{ "prey_steps", "Value of --prey_steps." },
};

void catcierge_haar_output_print_usage()
{
	size_t i;

	fprintf(stderr, "Haar matcher output variables:\n");
	fprintf(stderr, "------------------------------\n");

	for (i = 0; i < sizeof(haar_vars) / sizeof(haar_vars[0]); i++)
	{
		fprintf(stderr, "%30s   %s\n", haar_vars[i].name, haar_vars[i].description);
	}
}

const char *catcierge_haar_matcher_prey_method_str(catcierge_haar_prey_method_t method)
{
	switch (method)
	{
		case PREY_METHOD_NORMAL: return "normal";
		case PREY_METHOD_ADAPTIVE: return "adaptive";
		default: return "unknown";
	}
}

const char *catcierge_haar_matcher_translate(catcierge_matcher_t *octx, const char *var,
	char *buf, size_t bufsize)
{
	catcierge_haar_matcher_t *ctx = (catcierge_haar_matcher_t *)octx;
	assert(ctx);

	if (!strcmp(var, "cascade"))
	{
		return ctx->args->cascade;
	}

	if (!strcmp(var, "in_direction"))
	{
		return catcierge_get_left_right_str(ctx->args->in_direction);
	}

	if (!strcmp(var, "min_size"))
	{
		snprintf(buf, bufsize - 1, "%dx%d",
			ctx->args->min_width,
			ctx->args->min_height);
		return buf;
	}

	if (!strcmp(var, "min_size_width"))
	{
		snprintf(buf, bufsize - 1, "%d", ctx->args->min_width);
		return buf;
	}

	if (!strcmp(var, "min_size_height"))
	{
		snprintf(buf, bufsize - 1, "%d", ctx->args->min_height);
		return buf;
	}

	if (!strcmp(var, "no_match_is_fail"))
	{
		snprintf(buf, bufsize - 1, "%d", ctx->args->no_match_is_fail);
		return buf;
	}

	if (!strcmp(var, "eq_histogram"))
	{
		snprintf(buf, bufsize - 1, "%d", ctx->args->eq_histogram);
		return buf;
	}

	if (!strcmp(var, "prey_method"))
	{
		return catcierge_haar_matcher_prey_method_str(ctx->args->prey_method);
	}

	if (!strcmp(var, "prey_steps"))
	{
		snprintf(buf, bufsize - 1, "%d", ctx->args->prey_steps);
		return buf;
	}

	return NULL;
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
	printf("       Prey method: %s\n", args->prey_method == PREY_METHOD_ADAPTIVE ? "Adaptive" : "Normal");
	printf("        Prey steps: %d\n", args->prey_steps);
	printf("\n");
}

void catcierge_haar_matcher_args_init(catcierge_haar_matcher_args_t *args)
{
	assert(args);
	memset(args, 0, sizeof(catcierge_haar_matcher_args_t));
	args->super.type = MATCHER_HAAR;
	args->min_width = 80;
	args->min_height = 80;
	args->in_direction = DIR_RIGHT;
	args->eq_histogram = 0;
	args->debug = 0;
	args->no_match_is_fail = 0;
	args->prey_steps = 2;
	args->prey_method = PREY_METHOD_ADAPTIVE;
}

void catcierge_haar_matcher_set_debug(catcierge_haar_matcher_t *ctx, int debug)
{
	assert(ctx);
	ctx->super.debug = debug;
}
