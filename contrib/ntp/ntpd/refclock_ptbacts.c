/*
 * crude hack to avoid hard links in distribution
 * and keep only one ACTS type source for different
 * ACTS refclocks
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#if defined(REFCLOCK) && defined(CLOCK_PTBACTS)
# define KEEPPTBACTS
# include "refclock_acts.c"
#else /* not (REFCLOCK && CLOCK_PTBACTS) */
int refclock_ptbacts_bs;
#endif /* not (REFCLOCK && CLOCK_PTBACTS) */
