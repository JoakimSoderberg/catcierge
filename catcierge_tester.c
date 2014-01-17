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

int main(int argc, char **argv)
{
	catcierge_t ctx;
	char *snout_path = NULL;
	char *img_paths[256];
	size_t img_count = 0;
	IplImage *img = NULL;
	CvRect match_rect;
	CvScalar match_color;
	int match_success = 0;
	double match_res = 0;
	int i;
	int show = 0;
	int save = 0;
	char *output_path = "output";
	double match_threshold = 0.8;
	int match_flipped = 1;
	int was_flipped = 0;

	if (argc < 4)
	{
		fprintf(stderr, "Catcierge Image match Tester (C) Joakim Soderberg 2013-2014\n");
		fprintf(stderr, "Usage: %s --snout <snout image>\n"
						"         [--output [path]] [--show] [--match_flipped <0|1>] [--threshold]\n"
						"         <input images>\n", argv[0]);
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

			if (argc >= (i + 1))
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
			if (argc >= (i + 1))
			{
				i++;
				snout_path = argv[i];
				continue;
			}
		}
		else if (!strcmp(argv[i], "--threshold"))
		{
			if (argc >= (i + 1))
			{
				i++;
				match_threshold = atof(argv[i]);
				continue;
			}
		}
		else if (!strcmp(argv[i], "--match_flipped"))
		{
			if (argc >= (i + 1))
			{
				i++;
				match_flipped = atoi(argv[i]);
				continue;
			}
		}

		img_paths[img_count++] = argv[i];
	}

	if (!snout_path)
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

	if (catcierge_init(&ctx, snout_path, match_flipped, match_threshold))
	{
		fprintf(stderr, "Failed to init catcierge lib!\n");
		return -1;
	}

	for (i = 0; i < img_count; i++)
	{
		match_success = 0;

		if (!(img = cvLoadImage(img_paths[i], 1)))
		{
			fprintf(stderr, "Failed to load match image: %s\n", img_paths[i]);
			goto fail;
		}

		if ((match_res = catcierge_match(&ctx, img, &match_rect, &was_flipped)) < 0)
		{
			fprintf(stderr, "Something went wrong when matching image: %s\n", img_paths[i]);
			catcierge_destroy(&ctx);
		}

		match_success = (match_res >= match_threshold);

		if (match_success)
		{
			printf("Match%s! %f\n", was_flipped ? " (Flipped)": "", match_res);
			match_color = CV_RGB(0, 255, 0);
		}
		else
		{
			printf("No match! %f\n", match_res);
			match_color = CV_RGB(255, 0, 0);
		}

		cvRectangleR(img, match_rect, match_color, 1, 8, 0);
	
		if (show)
		{
			cvShowImage(img_paths[i], img);
			cvWaitKey(0);
			cvDestroyWindow(img_paths[i]);
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

		cvReleaseImage(&img);
	}
fail:
	catcierge_destroy(&ctx);

	return 0;
}
