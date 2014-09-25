#ifndef _TIMEKEEPING_INTERNAL_H
#define _TIMEKEEPING_INTERNAL_H
/*
 * timekeeping debug functions
 */
#include <linux/time.h>
#include <linux/clocksource.h>

static inline cycle_t clocksource_delta(cycle_t now, cycle_t last, cycle_t mask)
{
	return (now - last) & mask;
}

#endif /* _TIMEKEEPING_INTERNAL_H */
