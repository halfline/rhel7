#ifndef _LINUX_TIMEKEEPING_H
#define _LINUX_TIMEKEEPING_H

/* Included from linux/ktime.h */
void timekeeping_init(void);
extern int timekeeping_suspended;

/*
 * Get and set timeofday
 */
extern void do_gettimeofday(struct timeval *tv);
extern int do_settimeofday(const struct timespec *tv);
extern int do_sys_settimeofday(const struct timespec *tv,
			       const struct timezone *tz);

/*
 * Kernel time accessors
 */
unsigned long get_seconds(void);
struct timespec current_kernel_time(void);
/* does not take xtime_lock */
struct timespec __current_kernel_time(void);

/*
 * timespec based interfaces
 */
struct timespec get_monotonic_coarse(void);
extern void getrawmonotonic(struct timespec *ts);
extern void monotonic_to_bootbased(struct timespec *ts);
extern void get_monotonic_boottime(struct timespec *ts);

#if BITS_PER_LONG == 64
static inline void ktime_get_ts(struct timespec *ts)
{
	ktime_get_ts64(ts);
}

static inline void ktime_get_real_ts(struct timespec *ts)
{
	getnstimeofday64(ts);
}

#else
static inline void ktime_get_ts(struct timespec *ts)
{
	struct timespec64 ts64;

	ktime_get_ts64(&ts64);
	*ts = timespec64_to_timespec(ts64);
}

static inline void ktime_get_real_ts(struct timespec *ts)
{
	struct timespec64 ts64;

	getnstimeofday64(&ts64);
	*ts = timespec64_to_timespec(ts64);
}
#endif

#if BITS_PER_LONG == 64
static inline int __getnstimeofday(struct timespec *ts)
{
	return __getnstimeofday64(ts);
}

static inline void getnstimeofday(struct timespec *ts)
{
	getnstimeofday64(ts);
}

#else
static inline int __getnstimeofday(struct timespec *ts)
{
	struct timespec64 ts64;
	int ret = __getnstimeofday64(&ts64);

	*ts = timespec64_to_timespec(ts64);
	return ret;
}

static inline void getnstimeofday(struct timespec *ts)
{
	struct timespec64 ts64;

	getnstimeofday64(&ts64);
	*ts = timespec64_to_timespec(ts64);
}
#endif

extern void getboottime(struct timespec *ts);

#define do_posix_clock_monotonic_gettime(ts) ktime_get_ts(ts)

/*
 * ktime_t based interfaces
 */
extern ktime_t ktime_get(void);
extern ktime_t ktime_get_real(void);
extern ktime_t ktime_get_boottime(void);
extern ktime_t ktime_get_monotonic_offset(void);
extern ktime_t ktime_get_clocktai(void);
extern ktime_t ktime_get_raw(void);

/*
 * RTC specific
 */
void timekeeping_inject_sleeptime(struct timespec *delta);

static inline u64 ktime_get_raw_ns(void)
{
	return ktime_to_ns(ktime_get_raw());
}

/*
 * struct system_time_snapshot - simultaneous raw/real time capture with
 *	counter value
 * @cycles:	Clocksource counter value to produce the system times
 * @real:	Realtime system time
 * @raw:	Monotonic raw system time
 */
struct system_time_snapshot {
	cycle_t		cycles;
	ktime_t		real;
	ktime_t		raw;
};

/*
 * Simultaneously snapshot realtime and monotonic raw clocks
 */
extern void ktime_get_snapshot(struct system_time_snapshot *systime_snapshot);

/*
 * Persistent clock related interfaces
 */
extern bool persistent_clock_exist;
extern int persistent_clock_is_local;

static inline bool has_persistent_clock(void)
{
	return persistent_clock_exist;
}

extern void read_persistent_clock(struct timespec *ts);
extern void read_boot_clock(struct timespec *ts);
extern int update_persistent_clock(struct timespec now);


#endif
