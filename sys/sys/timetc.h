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
 * Structure used to interface to the machine dependent hardware support
 * for timekeeping.
 *
 * A timecounter is a (hard or soft) binary counter which has two properties:
 *    * it runs at a fixed, known frequency.
 *    * it must not roll over in less than (1 + delta)/HZ seconds.  "delta"
 *	is expected to be less than 20 msec, but no hard data has been 
 *      collected on this.  16 bit at 5 MHz (31 msec) is known to work.
 *
 * get_timecount() reads the counter.
 *
 * counter_mask removes unimplemented bits from the count value.
 *
 * frequency is the counter frequency in hz.
 *
 * name is a short mnemonic name for this counter.
 *
 * cost is a measure of how long time it takes to read the counter.
 *
 * adjustment [PPM << 16] which means that the smallest unit of correction
 *     you can apply amounts to 481.5 usec/year.
 *
 * scale_micro [2^32 * usec/tick].
 * scale_nano_i [ns/tick].
 * scale_nano_f [(ns/2^32)/tick].
 *
 * offset_count is the contents of the counter which corresponds to the
 *     rest of the offset_* values.
 *
 * offset_sec [s].
 * offset_micro [usec].
 * offset_nano [ns/2^32] is misnamed, the real unit is .23283064365...
 *     attoseconds (10E-18) and before you ask: yes, they are in fact 
 *     called attoseconds, it comes from "atten" for 18 in Danish/Swedish.
 *
 * Each timecounter must supply an array of three timecounters, this is needed
 * to guarantee atomicity in the code.  Index zero is used to transport 
 * modifications, for instance done with sysctl, into the timecounter being 
 * used in a safe way.  Such changes may be adopted with a delay of up to 1/HZ,
 * index one & two are used alternately for the actual timekeeping.
 *
 * 'tc_avail' points to the next available (external) timecounter in a
 *      circular queue.  This is only valid for index 0.
 *
 * `tc_other' points to the next "work" timecounter in a circular queue,
 *      i.e., for index i > 0 it points to index 1 + (i - 1) % NTIMECOUNTER.
 *      We also use it to point from index 0 to index 1.
 *
 * `tc_tweak' points to index 0.
 */

struct timecounter;
typedef unsigned timecounter_get_t __P((struct timecounter *));
typedef void timecounter_pps_t __P((struct timecounter *));

struct timecounter {
	/* These fields must be initialized by the driver. */
	timecounter_get_t	*tc_get_timecount;
	timecounter_pps_t	*tc_poll_pps;
	unsigned 		tc_counter_mask;
	u_int32_t		tc_frequency;
	char			*tc_name;
	void			*tc_priv;
	/* These fields will be managed by the generic code. */
	int64_t			tc_adjustment;
	u_int64_t		tc_scale;
	unsigned 		tc_offset_count;
	struct bintime		tc_offset;
	struct timeval		tc_microtime;
	struct timespec		tc_nanotime;
	struct timecounter	*tc_avail;
	struct timecounter	*tc_tweak;
	/* Fields not to be copied in tc_windup start with tc_generation */
	volatile unsigned	tc_generation;
	struct timecounter	*tc_next;
};

#ifdef _KERNEL
extern struct timecounter *volatile timecounter;

void	tc_init __P((struct timecounter *tc));
void	tc_setclock __P((struct timespec *ts));
void	tc_windup __P((void));
void	tc_update __P((struct timecounter *tc));
#endif /* !_KERNEL */

#endif /* !_SYS_TIMETC_H_ */
