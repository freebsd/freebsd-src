/*
 *
 * ===================================
 * HARP  |  Host ATM Research Platform
 * ===================================
 *
 *
 * This Host ATM Research Platform ("HARP") file (the "Software") is
 * made available by Network Computing Services, Inc. ("NetworkCS")
 * "AS IS".  NetworkCS does not provide maintenance, improvements or
 * support of any kind.
 *
 * NETWORKCS MAKES NO WARRANTIES OR REPRESENTATIONS, EXPRESS OR IMPLIED,
 * INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE, AS TO ANY ELEMENT OF THE
 * SOFTWARE OR ANY SUPPORT PROVIDED IN CONNECTION WITH THIS SOFTWARE.
 * In no event shall NetworkCS be responsible for any damages, including
 * but not limited to consequential damages, arising from or relating to
 * any use of the Software or related support.
 *
 * Copyright 1994-1998 Network Computing Services, Inc.
 *
 * Copies of this Software may be made, however, the above copyright
 * notice must be reproduced on all copies.
 *
 *	@(#) $FreeBSD: src/lib/libatm/timer.c,v 1.3 1999/08/27 23:58:05 peter Exp $
 *
 */

/*
 * User Space Library Functions
 * ----------------------------
 *
 * Timer functions
 *
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netatm/port.h>
#include <netatm/queue.h>
#include <netatm/atm.h>
#include <netatm/atm_if.h>
#include <netatm/atm_sap.h>
#include <netatm/atm_sys.h>
#include <netatm/atm_ioctl.h>

#include <errno.h>
#include <signal.h>

#include "libatm.h"

#ifndef lint
__RCSID("@(#) $FreeBSD: src/lib/libatm/timer.c,v 1.3 1999/08/27 23:58:05 peter Exp $");
#endif


Harp_timer	*harp_timer_head;
int		harp_timer_exec;


/*
 * Process a HARP timer tick
 *
 * This function is called via the SIGALRM signal.  It increments
 * harp_timer_exec.  The user should check this flag frequently and
 * call timer_proc when it is set.
 *
 * Arguments:
 *	None
 *
 * Returns:
 *	None
 *
 */
static void
timer_tick()
{
	/*
	 * Bump the timer flag
	 */
	harp_timer_exec++;
}


/*
 * Process HARP timers
 *
 * This function is called after a SIGALRM signal is posted.  It runs
 * down the list of timer entries, calling the specified functions
 * for any timers that have expired.
 *
 * Arguments:
 *	None
 *
 * Returns:
 *	None
 *
 */
void
timer_proc()
{
	Harp_timer	*htp;
	void		(*f)();

	/*
	 * Reset marks in all timers on the queue
	 */
	for (htp = harp_timer_head; htp; htp = htp->ht_next) {
		htp->ht_mark = -1;
	}

	/*
	 * Run through timer chain decrementing each timer.
	 * If an expired timer is found, take the timer block
	 * off the chain and call the specified function.  A
	 * timer's action can result in other timers being
	 * cancelled (taken off the queue), so every time we
	 * call a user function, we start over from the top of
	 * the list.
	 */
timer_cont:
	for (htp = harp_timer_head; htp; htp = htp->ht_next) {
		/*
		 * Make sure we only process each entry once and
		 * don't process entries that are put on the queue
		 * by user functions we call for this tick
		 */
		if (htp->ht_mark == -1) {
			/*
			 * Decrement the timer and mark that we've
			 * processed the entry
			 */
			htp->ht_ticks -= harp_timer_exec;
			htp->ht_mark = 1;

			/*
			 * Check whether the timer is expired
			 */
			if (htp->ht_ticks <= 0) {
				/*
				 * Unlink the timer block and call
				 * the user function
				 */
				f = htp->ht_func;
				UNLINK(htp, Harp_timer, harp_timer_head,
						ht_next);
				f(htp);

				/*
				 * Start over
				 */
				goto timer_cont;
			}
		}
	}

	/*
	 * Reset the timer exec flag
	 */
	harp_timer_exec = 0;
}


/*
 * Start the timer
 *
 * Set up the SIGALRM signal handler and set up the real-time
 * timer to tick once per second.
 *
 * Arguments:
 *	None
 *
 * Returns:
 *	0	success
 *	errno	reason for failure
 *
 */
int
init_timer()
{
	int			rc = 0;
	struct itimerval	timeval;

	/*
	 * Clear the timer flag
	 */
	harp_timer_exec = 0;

	/*
	 * Set up signal handler
	 */
	if ((int)signal(SIGALRM, timer_tick) == -1) {
		return(errno);
	}

	/*
	 * Start timer
	 */
	timeval.it_value.tv_sec = 1;
	timeval.it_value.tv_usec = 0;
	timeval.it_interval.tv_sec = 1;
	timeval.it_interval.tv_usec = 0;

	if (setitimer(ITIMER_REAL, &timeval,
			(struct itimerval *)0) == -1) {
		rc = errno;
		(void)signal(SIGALRM, SIG_DFL);
	}

	return(rc);
}


/*
 * Block timers from firing
 *
 * Block the SIGALRM signal.
 *
 * Arguments:
 *	None
 *
 * Returns:
 *	mask	the previous blocked signal mask
 *
 */
int
block_timer()
{
	/*
	 * Block the SIGALRM signal
	 */
	return(sigblock(sigmask(SIGALRM)));
}


/*
 * Re-enable timers
 *
 * Restore the signal mask (presumably one that was returned by
 * block_timer).
 *
 * Arguments:
 *	mask	the signal mask to be restored
 *
 * Returns:
 *	mask	the previous blocked signal mask
 *
 */
void
enable_timer(mask)
	int	mask;
{
	/*
	 * Set the signal mask
	 */
	sigsetmask(mask);

	return;
}
