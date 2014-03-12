#include <catcierge_config.h>
#include <assert.h>
#include "catcierge_timer.h"
#include <string.h>

void catcierge_timer_reset(catcierge_timer_t *t)
{
	assert(t);
	memset(&t->start, 0, sizeof(struct timeval));
	memset(&t->end, 0, sizeof(struct timeval));
}

int catcierge_timer_isactive(catcierge_timer_t *t)
{
	assert(t);
	return (t->start.tv_sec != 0);
}

void catcierge_timer_start(catcierge_timer_t *t)
{
	assert(t);
	gettimeofday(&t->start, NULL);
	gettimeofday(&t->end, NULL);
}

double catcierge_timer_get(catcierge_timer_t *t)
{
	assert(t);
	if (t->start.tv_sec == 0)
		return 0.0;

	gettimeofday(&t->end, NULL);
	return (t->end.tv_sec - t->start.tv_sec) +
			((t->end.tv_usec - t->start.tv_usec) / 1000000.0);
}

void catcierge_timer_set(catcierge_timer_t *t, double timeout)
{
	assert(t);
	t->timeout = timeout;
}

int catcierge_timer_has_timed_out(catcierge_timer_t *t)
{
	assert(t);
	return (catcierge_timer_get(t) >= t->timeout);
}
