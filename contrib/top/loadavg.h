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
 */

/*
 * We assume that if FSCALE is defined, then avenrun and ccpu are type long.
 * If your machine is an exception (mips, perhaps?) then make adjustments
 * here.
 *
 * Defined types:  load_avg for load averages, pctcpu for cpu percentages.
 */
#if defined(mips) && !(defined(NetBSD) || defined(FreeBSD))
# include <sys/fixpoint.h>
# if defined(FBITS) && !defined(FSCALE)
#  define FSCALE (1 << FBITS)	/* RISC/os on mips */
# endif
#endif

#ifdef FSCALE
# define FIXED_LOADAVG FSCALE
# define FIXED_PCTCPU FSCALE
#endif

#ifdef ibm032
# undef FIXED_LOADAVG
# undef FIXED_PCTCPU
# define FIXED_PCTCPU PCT_SCALE
#endif


#ifdef FIXED_PCTCPU
  typedef long pctcpu;
# define pctdouble(p) ((double)(p) / FIXED_PCTCPU)
#else
typedef double pctcpu;
# define pctdouble(p) (p)
#endif

#ifdef FIXED_LOADAVG
  typedef fixpt_t load_avg;
# define loaddouble(la) ((double)(la) / FIXED_LOADAVG)
# define intload(i) ((int)((i) * FIXED_LOADAVG))
#else
  typedef double load_avg;
# define loaddouble(la) (la)
# define intload(i) ((double)(i))
#endif
