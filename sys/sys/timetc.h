/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.ORG> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $FreeBSD$
 */

#ifndef _SYS_TIMETC_H_
#define _SYS_TIMETC_H_

/*
 * Struct timecounter is the interface between the hardware which implements
 * a timecounter and the MI code which uses this to keep track of time.
 *
 * A timecounter is a binary counter which has two properties:
 *    * it runs at a fixed, known frequency.
 *    * it has sufficient bits to not roll over in faster than approx
 *	2 msec or 2/hz, whichever is faster.  (The value of 2 here is
 *	really 1 + delta, for some indeterminate value of delta).
 *
 */

struct timecounter;
typedef u_int timecounter_get_t(struct timecounter *);
typedef void timecounter_pps_t(struct timecounter *);

struct timecounter {
	timecounter_get_t	*tc_get_timecount;
		/*
		 * This function reads the counter.  It is not required to
		 * mask any unimplemented bits out, as long as they are
		 * constant.
		 */
	timecounter_pps_t	*tc_poll_pps;
		/*
		 * This function is optional, it will be called whenever the
		 * timecounter is rewound, and is intended to check for PPS
		 * events.  Most hardware do not need it.
		 */
	u_int 		tc_counter_mask;
		/* This mask should mask off any unimplemnted bits. */
	u_int32_t		tc_frequency;
		/* Frequency of the counter in Hz. */
	char			*tc_name;
		/* Name of the counter. */
	void			*tc_priv;
		/* Pointer to the counters private parts. */
	struct timecounter	*tc_next;
		/* Initialize this to NUL */
};

#ifdef _KERNEL
extern struct timecounter *timecounter;

u_int32_t tc_getfrequency(void);
void	tc_init(struct timecounter *tc);
void	tc_setclock(struct timespec *ts);
#endif /* !_KERNEL */

#endif /* !_SYS_TIMETC_H_ */
