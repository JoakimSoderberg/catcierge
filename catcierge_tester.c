//
// This file is part of the Catcierge project.
//
// Copyright (c) Joakim Soderberg 2013-2014
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
#include <opencv2/imgproc/imgproc_c.h>
#include <opencv2/highgui/highgui_c.h>
#include <stdio.h>
#include <stdlib.h>
#include "catcierge.h"
#include "catcierge_util.h"
#ifdef _WIN32
#include <process.h>
#else
#include <limits.h>
#endif
#include <time.h>

int main(int argc, char **argv)
{
	catcierge_t ctx;
	#define MAX_SNOUT_COUNT 24
	const char *snout_paths[MAX_SNOUT_COUNT];
	size_t snout_count = 0;
	char *img_paths[4096];
	IplImage *imgs[4096];
	size_t img_count = 0;
	IplImage *img = NULL;
	CvRect match_rects[MAX_SNOUT_COUNT];
	CvSize img_size;
	CvScalar match_color;
	int match_success = 0;
	double match_res = 0;
	int debug = 0;
	int i;
	int j;
	int show = 0;
	int save = 0;
	char *output_path = "output";
	double match_threshold = 0.8;
	int match_flipped = 1;
	int was_flipped = 0;
	int success_count = 0;
	int preload = 1;
	clock_t start;
	clock_t end;

	fprintf(stderr, "Catcierge Image match Tester (C) Joakim Soderberg 2013-2014\n");

	if (argc < 4)
	{
		fprintf(stderr, "Usage: %s [--output [path]] [--debug] [--show]\n"
						"          [--match_flipped <0|1>] [--threshold]\n"
						"          [--preload]\n"
						"           --snout <snout images> --images <input images>\n", argv[0]);
		return -1;
	}

	for (i = 1; i < argc; i++)
	{
		if (!strcmp(argv[i], "--show"))
		{
			show = 1;
			continue;
		}
		else if (!strcmp(argv[i], "--output"))
		{
			save = 1;

			if ((i + 1) < argc)
			{
				if (strncmp(argv[i+1], "--", 2))
				{
					i++;
					output_path = argv[i];
				}
			}
			continue;
		}
		else if (!strcmp(argv[i], "--snout"))
		{
			while (((i + 1) < argc) 
				&& strncmp(argv[i+1], "--", 2))
			{
				i++;
				snout_paths[snout_count] = argv[i];
				snout_count++;
			}
		}
		else if (!strcmp(argv[i], "--threshold"))
		{
			if ((i + 1) < argc)
			{
				i++;
				match_threshold = atof(argv[i]);
				continue;
			}
		}
		else if (!strcmp(argv[i], "--match_flipped"))
		{
			if ((i + 1) < argc)
			{
				i++;
				match_flipped = atoi(argv[i]);
				continue;
			}
		}
		else if (!strcmp(argv[i], "--debug"))
		{
			debug = 1;
		}
		else if (!strcmp(argv[i], "--images"))
		{
			while (((i + 1) < argc) 
				&& strncmp(argv[i+1], "--", 2))
			{
				i++;
				img_paths[img_count] = argv[i];
				img_count++;
			}
		}
		else if (!strcmp(argv[i], "--preload"))
		{
			if ((i + 1) < argc)
			{
				i++;
				preload = 1;
				continue;
			}
		}
	}

	if (snout_count == 0)
	{
		fprintf(stderr, "No snout image specified\n");
		return -1;
	}

	if (img_count == 0)
	{
		fprintf(stderr, "No input image specified\n");
		return -1;
	}

	// Create output directory.
	if (save)
	{
		char cmd[2048];
		#ifdef _WIN32
		snprintf(cmd, sizeof(cmd), "md %s", output_path);
		#else
		snprintf(cmd, sizeof(cmd), "mkdir -p %s", output_path);
		#endif
		system(cmd);
	}

	if (catcierge_init(&ctx, snout_paths, snout_count))
	{
		fprintf(stderr, "Failed to init catcierge lib!\n");
		return -1;
	}

	catcierge_set_match_flipped(&ctx, match_flipped);
	catcierge_set_match_threshold(&ctx, match_threshold);
	catcierge_set_debug(&ctx, debug);
	//catcierge_set_binary_thresholds(&ctx, 90, 200);

	// If we should preload the images or not
	// (Don't let file IO screw with benchmark)
	if (preload)
	{
		for (i = 0; i < (int)img_count; i++)
		{
			if (!(imgs[i] = cvLoadImage(img_paths[i], 1)))
			{
				fprintf(stderr, "Failed to load match image: %s\n", img_paths[i]);
				goto fail;
			}
		}
	}

	start = clock();

	for (i = 0; i < (int)img_count; i++)
	{
		match_success = 0;

		printf("%s:\n", img_paths[i]);

		if (preload)
		{
			img = imgs[i];
		}
		else
		{
			if (!(img = cvLoadImage(img_paths[i], 1)))
			{
				fprintf(stderr, "Failed to load match image: %s\n", img_paths[i]);
				goto fail;
			}
		}

		img_size = cvGetSize(img);

		printf("  Image size: %dx%d\n", img_size.width, img_size.height);

		if ((match_res = catcierge_match(&ctx, img, match_rects, snout_count, &was_flipped)) < 0)
		{
			fprintf(stderr, "Something went wrong when matching image: %s\n", img_paths[i]);
			catcierge_destroy(&ctx);
		}

		match_success = (match_res >= match_threshold);

		if (match_success)
		{
			printf("  Match%s! %f\n", was_flipped ? " (Flipped)": "", match_res);
			match_color = CV_RGB(0, 255, 0);
			success_count++;
		}
		else
		{
			printf("  No match! %f\n", match_res);
			match_color = CV_RGB(255, 0, 0);
		}

		if (show || save)
		{
			for (j = 0; j < (int)snout_count; j++)
			{
				cvRectangleR(img, match_rects[j], match_color, 1, 8, 0);
			}

			if (show)
			{
				cvShowImage("image", img);
				cvWaitKey(0);
			}

			if (save)
			{
				char out_file[PATH_MAX]; 
				char tmp[PATH_MAX];
				char *filename = tmp;
				char *ext;
				char *start;

				// Get the extension.
				strncpy(tmp, img_paths[i], sizeof(tmp));
				ext = strrchr(tmp, '.');
				*ext = '\0';
				ext++;

				// And filename.
				filename = strrchr(tmp, '/');
				start = strrchr(tmp, '\\');
				if (start> filename)
					filename = start;
				filename++;

				snprintf(out_file, sizeof(out_file) - 1, "%s/match_%s__%s.%s", 
						output_path, match_success ? "ok" : "fail", filename, ext);

				printf("Saving image \"%s\"\n", out_file);

				cvSaveImage(out_file, img, 0);
			}
		}

		cvReleaseImage(&img);
	}

	end = clock();

	printf("%d of %zu successful! (%f seconds)\n",
		success_count, img_count, (float)(end - start) / CLOCKS_PER_SEC);

fail:
	catcierge_destroy(&ctx);
	cvDestroyAllWindows();

	return 0;
}
