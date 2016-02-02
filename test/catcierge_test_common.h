
#ifndef __CATCIERGE_TEST_COMMON_H__
#define __CATCIERGE_TEST_COMMON_H__

#include "catcierge_args.h"

IplImage *create_clear_image();
IplImage *create_black_image();
IplImage *open_test_image(int series, int i);
void set_default_test_snouts(catcierge_args_t *args);
void free_test_image(catcierge_grb_t *grb);
int load_test_image_and_run(catcierge_grb_t *grb, int series, int i);

int load_black_test_image_and_run(catcierge_grb_t *grb);
int get_argc(char **argv);

#endif // __CATCIERGE_TEST_COMMON_H__
