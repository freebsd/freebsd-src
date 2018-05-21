/*
 *  Top - a top users display for Berkeley Unix
 *
 *  Defines required to access load average figures.
 *
 *  This include file sets up everything we need to access the load average
 *  values in the kernel in a machine independent way.  First, it sets the
 *  typedef "load_avg" to be either double or long (depending on what is
 *  needed), then it defines these macros appropriately:
 *
 *	loaddouble(la) - convert load_avg to double.
 *	intload(i)     - convert integer to load_avg.
 *
 *	$FreeBSD$
 */

/*
 * We assume that if FSCALE is defined, then avenrun and ccpu are type long.
 * If your machine is an exception (mips, perhaps?) then make adjustments
 * here.
 *
 * Defined types:  load_avg for load averages, pctcpu for cpu percentages.
 */
#if defined(__mips__) && defined(__FreeBSD__)
# include <sys/fixpoint.h>
# if defined(FBITS) && !defined(FSCALE)
#  define FSCALE (1 << FBITS)	/* RISC/os on mips */
# endif
#endif

#define FIXED_LOADAVG FSCALE
#define FIXED_PCTCPU FSCALE

typedef long pctcpu;
#define pctdouble(p) ((double)(p) / FIXED_PCTCPU)

typedef fixpt_t load_avg;
#define loaddouble(la) ((double)(la) / FIXED_LOADAVG)
#define intload(i) ((int)((i) * FIXED_LOADAVG))
