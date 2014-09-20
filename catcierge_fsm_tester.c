#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "catcierge_fsm.h"
#include "catcierge_args.h"
#include "catcierge_types.h"
#include "catcierge_config.h"
#include "catcierge_args.h"
#include "catcierge_output.h"
#include <opencv2/imgproc/imgproc_c.h>
#include <opencv2/highgui/highgui_c.h>

typedef struct fsm_tester_ctx_s
{
	char *img_paths[4];
	size_t img_count;
} fsm_tester_ctx_t;

static IplImage *load_image(const char *path)
{
	IplImage *img = NULL;

	if (!(img = cvLoadImage(path, 0)))
	{
		fprintf(stderr, "Failed to load image: %s\n", path);
		return NULL;
	}

	return img;
}

int parse_args_callback(catcierge_args_t *args,
		char *key, char **values, size_t value_count, void *user)
{
	size_t i;
	fsm_tester_ctx_t *ctx = (fsm_tester_ctx_t *)user;
	printf("Callback!\n");

	if (!strcmp(key, "images"))
	{
		if (value_count != 4)
		{
			fprintf(stderr, "Need %d images but got %d\n", 4, (int)value_count);
			return -1;
		}

		for (i = 0; i < value_count; i++)
		{
			ctx->img_paths[i] = values[i];
			ctx->img_count++;
		}

		return 0;
	}

	return -1;
}

int main(int argc, char **argv)
{
	size_t i;
	int ret = 0;
	fsm_tester_ctx_t ctx;
	catcierge_grb_t grb;
	catcierge_args_t *args = &grb.args;

	catcierge_grabber_init(&grb);

	if (catcierge_parse_config(args, argc, argv))
	{
		catcierge_show_usage(args, argv[0]);
		return -1;
	}

	if (catcierge_parse_cmdargs(args, argc, argv, parse_args_callback, &ctx))
	{
		catcierge_show_usage(args, argv[0]);
		fprintf(stderr, "--images <4 input images>\n");
		fprintf(stderr, "Not enough parameters\n");
		ret = -1; goto fail;
	}

	// TODO: Support the template matcher here as well...
	if (args->matcher_type != MATCHER_HAAR)
	{
		fprintf(stderr, "Sorry the \"%s\" matcher is not supported by this program.\n", args->matcher);
		ret = -1; goto fail;
	}

	if (catcierge_haar_matcher_init(&grb.haar, &grb.common_matcher, &args->haar))
	{
		ret = -1; goto fail;
	}

	if (catcierge_output_init(&grb.output))
	{
		fprintf(stderr, "Failed to init output template system\n");
		exit(-1);
	}

	if (catcierge_output_load_templates(&grb.output,
			args->inputs, args->input_count))
	{
		fprintf(stderr, "Failed to load output templates\n");
		exit(-1);
	}

	catcierge_set_state(&grb, catcierge_state_waiting);

	// Load the first image and obstruct the frame.
	if (!(grb.img = load_image(ctx.img_paths[0])))
	{
		ret = -1; goto fail;
	}

	catcierge_run_state(&grb);
	cvReleaseImage(&grb.img);
	grb.img = NULL;

	// Load the match images.
	for (i = 0; i < ctx.img_count; i++)
	{
		if (!(grb.img = load_image(ctx.img_paths[i])))
		{
			ret = -1; goto fail;
		}

		catcierge_run_state(&grb);

		cvReleaseImage(&grb.img);
		grb.img = NULL;
	}

fail:
	catcierge_haar_matcher_destroy(&grb.haar);
	catcierge_output_destroy(&grb.output);
	catcierge_grabber_destroy(&grb);

	return ret;
}