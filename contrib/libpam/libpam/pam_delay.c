/*
 * pam_delay.c
 *
 * Copyright (c) Andrew G. Morgan <morgan@linux.kernel.org> 1996-8
 * All rights reserved.
 *
 * $Id: pam_delay.c,v 1.5 1997/04/05 06:54:19 morgan Exp $
 * $FreeBSD$
 *
 * $Log: pam_delay.c,v $
 */

/*
 * This is a simple implementation of a delay on failure mechanism; an
 * attempt to overcome authentication-time attacks in a simple manner.
 */

#include <unistd.h>
#include "pam_private.h"

/* **********************************************************************
 * initialize the time as unset, this is set on the return from the
 * authenticating pair of of the libpam pam_XXX calls.
 */

void _pam_reset_timer(pam_handle_t *pamh)
{
     D(("setting pamh->fail_delay.set to FALSE"));
     pamh->fail_delay.set = PAM_FALSE;
}

/* **********************************************************************
 * this function sets the start time for possible delayed failing.
 *
 * Eventually, it may set the timer so libpam knows how long the program
 * has already been executing. Currently, this value is used to seed
 * a pseudo-random number generator...
 */

void _pam_start_timer(pam_handle_t *pamh)
{
     pamh->fail_delay.begin = time(NULL);
     D(("starting timer..."));
}

/* *******************************************************************
 * Compute a pseudo random time. The value is base*(1 +/- 1/5) where
 * the distribution is pseudo gausian (the sum of three evenly
 * distributed random numbers -- central limit theorem and all ;^) The
 * linear random numbers are based on a formulae given in Knuth's
 * Seminumerical recipies that was reproduced in `Numerical Recipies
 * in C'. It is *not* a cryptographically strong generator, but it is
 * probably "good enough" for our purposes here.
 *
 * /dev/random might be a better place to look for some numbers...
 */

static unsigned int _pam_rand(unsigned int seed)
{
#define N1 1664525
#define N2 1013904223
     return N1*seed + N2;
}

static unsigned int _pam_compute_delay(unsigned int seed, unsigned int base)
{
     int i;
     double sum;
     unsigned int ans;

     for (sum=i=0; i<3; ++i) {
	  seed = _pam_rand(seed);
	  sum += (double) ((seed / 10) % 1000000);
     }
     sum = (sum/3.)/1e6 - .5;                      /* rescale */
     ans = (unsigned int) ( base*(1.+sum) );
     D(("random number: base=%u -> ans=%u\n", base, ans));

     return ans;
}

/* **********************************************************************
 * the following function sleeps for a random time. The actual time
 * slept is computed above.. It is based on the requested time but will
 * differ by up to +/- 25%.
 */

void _pam_await_timer(pam_handle_t *pamh, int status)
{
    unsigned int delay;
    D(("waiting?..."));

    delay = _pam_compute_delay(pamh->fail_delay.begin,
			       pamh->fail_delay.delay);
    if (pamh->fail_delay.delay_fn_ptr) {
	union {
	    const void *value;
	    void (*fn)(int, unsigned);
	} hack_fn_u;

	/* always call the applications delay function, even if
	   the delay is zero - indicate status */
	hack_fn_u.value = pamh->fail_delay.delay_fn_ptr;
	hack_fn_u.fn(status, delay);

    } else if (status != PAM_SUCCESS && pamh->fail_delay.set) {

	D(("will wait %u usec", delay));

	if (delay > 0) {
	    struct timeval tval;

	    tval.tv_sec  = delay / 1000000;
	    tval.tv_usec = delay % 1000000;
	    select(0, NULL, NULL, NULL, &tval);
	}
    }

    _pam_reset_timer(pamh);
    D(("waiting done"));
}

/* **********************************************************************
 * this function is known to both the module and the application, it
 * keeps a running score of the largest-requested delay so far, as
 * specified by either modules or an application.
 */

int pam_fail_delay(pam_handle_t *pamh, unsigned int usec)
{
     int largest;

     IF_NO_PAMH("pam_fail_delay", pamh, PAM_SYSTEM_ERR);

     D(("setting delay to %u",usec));

     if (pamh->fail_delay.set) {
          largest = pamh->fail_delay.delay;
     } else {
	  pamh->fail_delay.set = PAM_TRUE;
          largest = 0;
     }

     D(("largest = %u",largest));

     if (largest < usec) {
          D(("resetting largest delay"));
	  pamh->fail_delay.delay = usec;
     }

     return PAM_SUCCESS;
}

