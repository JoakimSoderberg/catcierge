
#ifndef __CATCIERGE_TIMER_H__
#define __CATCIERGE_TIMER_H__

#include "catcierge_util.h"
#include <time.h>

#ifdef _WIN32
#include "win32/gettimeofday.h"
#else
#include <unistd.h>
#include <sys/time.h>
#endif

typedef struct catcierge_timer_s
{
	struct timeval start;
	struct timeval end;
	double timeout;
} catcierge_timer_t;

void catcierge_timer_reset(catcierge_timer_t *t);

int catcierge_timer_isactive(catcierge_timer_t *t);

void catcierge_timer_start(catcierge_timer_t *t);

double catcierge_timer_get(catcierge_timer_t *t);

void catcierge_timer_set(catcierge_timer_t *t, double timeout);

int catcierge_timer_has_timed_out(catcierge_timer_t *t);


#endif // __CATCIERGE_TIMER_H__
