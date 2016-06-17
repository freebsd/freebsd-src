#ifndef _ASM_PARAM_H
#define _ASM_PARAM_H

#ifndef HZ

#ifdef __KERNEL__

/* Safeguard against user stupidity  */
#ifdef _SYS_PARAM_H
#error Do not include <asm/param.h> with __KERNEL__ defined!
#endif

#include <linux/config.h>

#ifdef CONFIG_DECSTATION
   /*
    * log2(HZ), change this here if you want another HZ value. This is also
    * used in dec_time_init.  Minimum is 1, Maximum is 15.
    */
#  define LOG_2_HZ 7
#  define HZ (1 << LOG_2_HZ)
   /*
    * Ye olde division-by-multiplication trick.
    * This works only if 100 / HZ <= 1
    */
#  define QUOTIENT ((1UL << (32 - LOG_2_HZ)) * 100)
#  define hz_to_std(a)				\
   ({ unsigned long __res;			\
      unsigned long __lo;			\
	__asm__("multu\t%2,%3\n\t"		\
		:"=h" (__res), "=l" (__lo)	\
		:"r" (a), "r" (QUOTIENT));	\
	(__typeof__(a)) __res;})

#else /* Not a DECstation  */

/* This is the internal value of HZ, that is the rate at which the jiffies
   counter is increasing.  This value is independent from the external value
   and can be changed in order to suit the hardware and application
   requirements.  */
#  define HZ 100
#  define hz_to_std(a) (a)

#endif /* Not a DECstation  */

#else /* defined(__KERNEL__)  */

/* This is the external value of HZ as seen by user programs.  Don't change
   unless you know what you're doing - changing breaks binary compatibility.  */
#define HZ 100

#endif /* defined(__KERNEL__)  */
#endif /* defined(HZ)  */

#define EXEC_PAGESIZE	65536

#ifndef NGROUPS
#define NGROUPS		32
#endif

#ifndef NOGROUP
#define NOGROUP		(-1)
#endif

#define MAXHOSTNAMELEN	64	/* max length of hostname */

#ifdef __KERNEL__
# define CLOCKS_PER_SEC	100	/* frequency at which times() counts */
#endif

#endif /* _ASM_PARAM_H */
