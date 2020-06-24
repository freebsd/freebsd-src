/*
 * timexsup.c - 'struct timex' support functions
 *
 * Written by Juergen Perlinger (perlinger@ntp.org) for the NTP project.
 * The contents of 'html/copyright.html' apply.
 */

#include "config.h"
#include "timexsup.h"
#include <limits.h>
#include <math.h>

#ifdef HAVE_SYS_TIMEX_H
# include <sys/timex.h>
#endif

#if defined(MOD_NANO) != defined(STA_NANO)
# warning inconsistent definitions of MOD_NANO vs STA_NANO
#endif

static long
clamp_rounded(
	double dval
	)
{
	/* round */
	dval = floor(dval + 0.5);

	/* clamp / saturate */
	if (dval >= (double)LONG_MAX)
		return LONG_MAX;
	if (dval <= (double)LONG_MIN)
		return LONG_MIN;
	return (long)dval;
}

double
dbl_from_var_long(
	long	lval,
	int	status
	)
{
#ifdef STA_NANO
	if (status & STA_NANO)
		return (double)lval * 1e-9;
#else
	(void)status;
#endif
	return (double)lval * 1e-6;
}

double
dbl_from_usec_long(
	long	lval
	)
{
	return (double)lval * 1e-6;
}

long
var_long_from_dbl(
	double		dval,
	unsigned int *	modes
	)
{
#ifdef MOD_NANO
	*modes |= MOD_NANO;
	dval *= 1e+9;
#else
	(void)modes;
	dval *= 1e+6;
#endif
	return clamp_rounded(dval);
}

long
usec_long_from_dbl(
	double	dval
	)
{
	return clamp_rounded(dval * 1e+6);
}
