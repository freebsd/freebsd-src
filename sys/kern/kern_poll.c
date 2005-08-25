/*-
 * Copyright (c) 2001-2002 Luigi Rizzo
 *
 * Supported by: the Xorp Project (www.xorp.org)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/socket.h>			/* needed by net/if.h		*/
#include <sys/sysctl.h>

#include <net/if.h>			/* for IFF_* flags		*/
#include <net/netisr.h>			/* for NETISR_POLL		*/

#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/kthread.h>

static void netisr_poll(void);		/* the two netisr handlers      */
static void netisr_pollmore(void);

void hardclock_device_poll(void);	/* hook from hardclock		*/
void ether_poll(int);			/* polling while in trap	*/

/*
 * Polling support for [network] device drivers.
 *
 * Drivers which support this feature try to register with the
 * polling code.
 *
 * If registration is successful, the driver must disable interrupts,
 * and further I/O is performed through the handler, which is invoked
 * (at least once per clock tick) with 3 arguments: the "arg" passed at
 * register time (a struct ifnet pointer), a command, and a "count" limit.
 *
 * The command can be one of the following:
 *  POLL_ONLY: quick move of "count" packets from input/output queues.
 *  POLL_AND_CHECK_STATUS: as above, plus check status registers or do
 *	other more expensive operations. This command is issued periodically
 *	but less frequently than POLL_ONLY.
 *  POLL_DEREGISTER: deregister and return to interrupt mode.
 *
 * The first two commands are only issued if the interface is marked as
 * 'IFF_UP and IFF_DRV_RUNNING', the last one only if IFF_DRV_RUNNING is set.
 *
 * The count limit specifies how much work the handler can do during the
 * call -- typically this is the number of packets to be received, or
 * transmitted, etc. (drivers are free to interpret this number, as long
 * as the max time spent in the function grows roughly linearly with the
 * count).
 *
 * Deregistration can be requested by the driver itself (typically in the
 * *_stop() routine), or by the polling code, by invoking the handler.
 *
 * Polling can be globally enabled or disabled with the sysctl variable
 * kern.polling.enable (default is 0, disabled)
 *
 * A second variable controls the sharing of CPU between polling/kernel
 * network processing, and other activities (typically userlevel tasks):
 * kern.polling.user_frac (between 0 and 100, default 50) sets the share
 * of CPU allocated to user tasks. CPU is allocated proportionally to the
 * shares, by dynamically adjusting the "count" (poll_burst).
 *
 * Other parameters can should be left to their default values.
 * The following constraints hold
 *
 *	1 <= poll_each_burst <= poll_burst <= poll_burst_max
 *	0 <= poll_in_trap <= poll_each_burst
 *	MIN_POLL_BURST_MAX <= poll_burst_max <= MAX_POLL_BURST_MAX
 */

#define MIN_POLL_BURST_MAX	10
#define MAX_POLL_BURST_MAX	1000

SYSCTL_NODE(_kern, OID_AUTO, polling, CTLFLAG_RW, 0,
	"Device polling parameters");

static u_int32_t poll_burst = 5;
SYSCTL_UINT(_kern_polling, OID_AUTO, burst, CTLFLAG_RW,
	&poll_burst, 0, "Current polling burst size");

static u_int32_t poll_each_burst = 5;
SYSCTL_UINT(_kern_polling, OID_AUTO, each_burst, CTLFLAG_RW,
	&poll_each_burst, 0, "Max size of each burst");

static u_int32_t poll_burst_max = 150;	/* good for 100Mbit net and HZ=1000 */
SYSCTL_UINT(_kern_polling, OID_AUTO, burst_max, CTLFLAG_RW,
	&poll_burst_max, 0, "Max Polling burst size");

static u_int32_t poll_in_idle_loop=0;	/* do we poll in idle loop ? */
SYSCTL_UINT(_kern_polling, OID_AUTO, idle_poll, CTLFLAG_RW,
	&poll_in_idle_loop, 0, "Enable device polling in idle loop");

u_int32_t poll_in_trap;			/* used in trap.c */
SYSCTL_UINT(_kern_polling, OID_AUTO, poll_in_trap, CTLFLAG_RW,
	&poll_in_trap, 0, "Poll burst size during a trap");

static u_int32_t user_frac = 50;
SYSCTL_UINT(_kern_polling, OID_AUTO, user_frac, CTLFLAG_RW,
	&user_frac, 0, "Desired user fraction of cpu time");

static u_int32_t reg_frac = 20 ;
SYSCTL_UINT(_kern_polling, OID_AUTO, reg_frac, CTLFLAG_RW,
	&reg_frac, 0, "Every this many cycles poll register");

static u_int32_t short_ticks;
SYSCTL_UINT(_kern_polling, OID_AUTO, short_ticks, CTLFLAG_RW,
	&short_ticks, 0, "Hardclock ticks shorter than they should be");

static u_int32_t lost_polls;
SYSCTL_UINT(_kern_polling, OID_AUTO, lost_polls, CTLFLAG_RW,
	&lost_polls, 0, "How many times we would have lost a poll tick");

static u_int32_t pending_polls;
SYSCTL_UINT(_kern_polling, OID_AUTO, pending_polls, CTLFLAG_RW,
	&pending_polls, 0, "Do we need to poll again");

static int residual_burst = 0;
SYSCTL_INT(_kern_polling, OID_AUTO, residual_burst, CTLFLAG_RW,
	&residual_burst, 0, "# of residual cycles in burst");

static u_int32_t poll_handlers; /* next free entry in pr[]. */
SYSCTL_UINT(_kern_polling, OID_AUTO, handlers, CTLFLAG_RD,
	&poll_handlers, 0, "Number of registered poll handlers");

static int polling = 0;		/* global polling enable */
SYSCTL_UINT(_kern_polling, OID_AUTO, enable, CTLFLAG_RW,
	&polling, 0, "Polling enabled");

static u_int32_t phase;
SYSCTL_UINT(_kern_polling, OID_AUTO, phase, CTLFLAG_RW,
	&phase, 0, "Polling phase");

static u_int32_t suspect;
SYSCTL_UINT(_kern_polling, OID_AUTO, suspect, CTLFLAG_RW,
	&suspect, 0, "suspect event");

static u_int32_t stalled;
SYSCTL_UINT(_kern_polling, OID_AUTO, stalled, CTLFLAG_RW,
	&stalled, 0, "potential stalls");

static u_int32_t idlepoll_sleeping; /* idlepoll is sleeping */
SYSCTL_UINT(_kern_polling, OID_AUTO, idlepoll_sleeping, CTLFLAG_RD,
	&idlepoll_sleeping, 0, "idlepoll is sleeping");


#define POLL_LIST_LEN  128
struct pollrec {
	poll_handler_t	*handler;
	struct ifnet	*ifp;
};

static struct pollrec pr[POLL_LIST_LEN];

static void
init_device_poll(void)
{

	netisr_register(NETISR_POLL, (netisr_t *)netisr_poll, NULL, 0);
	netisr_register(NETISR_POLLMORE, (netisr_t *)netisr_pollmore, NULL, 0);
}
SYSINIT(device_poll, SI_SUB_CLOCKS, SI_ORDER_MIDDLE, init_device_poll, NULL)


/*
 * Hook from hardclock. Tries to schedule a netisr, but keeps track
 * of lost ticks due to the previous handler taking too long.
 * Normally, this should not happen, because polling handler should
 * run for a short time. However, in some cases (e.g. when there are
 * changes in link status etc.) the drivers take a very long time
 * (even in the order of milliseconds) to reset and reconfigure the
 * device, causing apparent lost polls.
 *
 * The first part of the code is just for debugging purposes, and tries
 * to count how often hardclock ticks are shorter than they should,
 * meaning either stray interrupts or delayed events.
 */
void
hardclock_device_poll(void)
{
	static struct timeval prev_t, t;
	int delta;

	if (poll_handlers == 0)
		return;

	microuptime(&t);
	delta = (t.tv_usec - prev_t.tv_usec) +
		(t.tv_sec - prev_t.tv_sec)*1000000;
	if (delta * hz < 500000)
		short_ticks++;
	else
		prev_t = t;

	if (pending_polls > 100) {
		/*
		 * Too much, assume it has stalled (not always true
		 * see comment above).
		 */
		stalled++;
		pending_polls = 0;
		phase = 0;
	}

	if (phase <= 2) {
		if (phase != 0)
			suspect++;
		phase = 1;
		schednetisrbits(1 << NETISR_POLL | 1 << NETISR_POLLMORE);
		phase = 2;
	}
	if (pending_polls++ > 0)
		lost_polls++;
}

/*
 * ether_poll is called from the idle loop or from the trap handler.
 */
void
ether_poll(int count)
{
	int i;

	mtx_lock(&Giant);

	if (count > poll_each_burst)
		count = poll_each_burst;
	for (i = 0 ; i < poll_handlers ; i++)
		if (pr[i].handler &&
		    (pr[i].ifp->if_flags & IFF_UP) &&
		    (pr[i].ifp->if_drv_flags & IFF_DRV_RUNNING))
			pr[i].handler(pr[i].ifp, 0, count); /* quick check */
	mtx_unlock(&Giant);
}

/*
 * netisr_pollmore is called after other netisr's, possibly scheduling
 * another NETISR_POLL call, or adapting the burst size for the next cycle.
 *
 * It is very bad to fetch large bursts of packets from a single card at once,
 * because the burst could take a long time to be completely processed, or
 * could saturate the intermediate queue (ipintrq or similar) leading to
 * losses or unfairness. To reduce the problem, and also to account better for
 * time spent in network-related processing, we split the burst in smaller
 * chunks of fixed size, giving control to the other netisr's between chunks.
 * This helps in improving the fairness, reducing livelock (because we
 * emulate more closely the "process to completion" that we have with
 * fastforwarding) and accounting for the work performed in low level
 * handling and forwarding.
 */

static struct timeval poll_start_t;

void
netisr_pollmore()
{
	struct timeval t;
	int kern_load;
	/* XXX run at splhigh() or equivalent */

	phase = 5;
	if (residual_burst > 0) {
		schednetisrbits(1 << NETISR_POLL | 1 << NETISR_POLLMORE);
		/* will run immediately on return, followed by netisrs */
		return;
	}
	/* here we can account time spent in netisr's in this tick */
	microuptime(&t);
	kern_load = (t.tv_usec - poll_start_t.tv_usec) +
		(t.tv_sec - poll_start_t.tv_sec)*1000000;	/* us */
	kern_load = (kern_load * hz) / 10000;			/* 0..100 */
	if (kern_load > (100 - user_frac)) { /* try decrease ticks */
		if (poll_burst > 1)
			poll_burst--;
	} else {
		if (poll_burst < poll_burst_max)
			poll_burst++;
	}

	pending_polls--;
	if (pending_polls == 0) /* we are done */
		phase = 0;
	else {
		/*
		 * Last cycle was long and caused us to miss one or more
		 * hardclock ticks. Restart processing again, but slightly
		 * reduce the burst size to prevent that this happens again.
		 */
		poll_burst -= (poll_burst / 8);
		if (poll_burst < 1)
			poll_burst = 1;
		schednetisrbits(1 << NETISR_POLL | 1 << NETISR_POLLMORE);
		phase = 6;
	}
}

/*
 * netisr_poll is scheduled by schednetisr when appropriate, typically once
 * per tick. It is called at splnet() so first thing to do is to upgrade to
 * splimp(), and call all registered handlers.
 */
static void
netisr_poll(void)
{
	static int reg_frac_count;
	int i, cycles;
	enum poll_cmd arg = POLL_ONLY;
	mtx_lock(&Giant);

	phase = 3;
	if (residual_burst == 0) { /* first call in this tick */
		microuptime(&poll_start_t);
		/*
		 * Check that paremeters are consistent with runtime
		 * variables. Some of these tests could be done at sysctl
		 * time, but the savings would be very limited because we
		 * still have to check against reg_frac_count and
		 * poll_each_burst. So, instead of writing separate sysctl
		 * handlers, we do all here.
		 */

		if (reg_frac > hz)
			reg_frac = hz;
		else if (reg_frac < 1)
			reg_frac = 1;
		if (reg_frac_count > reg_frac)
			reg_frac_count = reg_frac - 1;
		if (reg_frac_count-- == 0) {
			arg = POLL_AND_CHECK_STATUS;
			reg_frac_count = reg_frac - 1;
		}
		if (poll_burst_max < MIN_POLL_BURST_MAX)
			poll_burst_max = MIN_POLL_BURST_MAX;
		else if (poll_burst_max > MAX_POLL_BURST_MAX)
			poll_burst_max = MAX_POLL_BURST_MAX;

		if (poll_each_burst < 1)
			poll_each_burst = 1;
		else if (poll_each_burst > poll_burst_max)
			poll_each_burst = poll_burst_max;

		if (poll_burst > poll_burst_max)
			poll_burst = poll_burst_max;
		residual_burst = poll_burst;
	}
	cycles = (residual_burst < poll_each_burst) ?
		residual_burst : poll_each_burst;
	residual_burst -= cycles;

	if (polling) {
		for (i = 0 ; i < poll_handlers ; i++)
			if (pr[i].handler &&
			    (pr[i].ifp->if_flags & IFF_UP) &&
			    (pr[i].ifp->if_drv_flags & IFF_DRV_RUNNING))
				pr[i].handler(pr[i].ifp, arg, cycles);
	} else {	/* unregister */
		for (i = 0 ; i < poll_handlers ; i++) {
			if (pr[i].handler &&
			    pr[i].ifp->if_drv_flags & IFF_DRV_RUNNING) {
				pr[i].ifp->if_flags &= ~IFF_POLLING;
				pr[i].handler(pr[i].ifp, POLL_DEREGISTER, 1);
			}
			pr[i].handler=NULL;
		}
		residual_burst = 0;
		poll_handlers = 0;
	}
	/* on -stable, schednetisr(NETISR_POLLMORE); */
	phase = 4;
	mtx_unlock(&Giant);
}

/*
 * Try to register routine for polling. Returns 1 if successful
 * (and polling should be enabled), 0 otherwise.
 * A device is not supposed to register itself multiple times.
 *
 * This is called from within the *_intr() functions, so we do not need
 * further locking.
 */
int
ether_poll_register(poll_handler_t *h, struct ifnet *ifp)
{
	int s;

	if (polling == 0) /* polling disabled, cannot register */
		return 0;
	if (h == NULL || ifp == NULL)		/* bad arguments	*/
		return 0;
	if ( !(ifp->if_flags & IFF_UP) )	/* must be up		*/
		return 0;
	if (ifp->if_flags & IFF_POLLING)	/* already polling	*/
		return 0;

	s = splhigh();
	if (poll_handlers >= POLL_LIST_LEN) {
		/*
		 * List full, cannot register more entries.
		 * This should never happen; if it does, it is probably a
		 * broken driver trying to register multiple times. Checking
		 * this at runtime is expensive, and won't solve the problem
		 * anyways, so just report a few times and then give up.
		 */
		static int verbose = 10 ;
		splx(s);
		if (verbose >0) {
			printf("poll handlers list full, "
				"maybe a broken driver ?\n");
			verbose--;
		}
		return 0; /* no polling for you */
	}

	pr[poll_handlers].handler = h;
	pr[poll_handlers].ifp = ifp;
	poll_handlers++;
	ifp->if_flags |= IFF_POLLING;
	splx(s);
	if (idlepoll_sleeping)
		wakeup(&idlepoll_sleeping);
	return 1; /* polling enabled in next call */
}

/*
 * Remove interface from the polling list. Normally called by *_stop().
 * It is not an error to call it with IFF_POLLING clear, the call is
 * sufficiently rare to be preferable to save the space for the extra
 * test in each driver in exchange of one additional function call.
 */
int
ether_poll_deregister(struct ifnet *ifp)
{
	int i;

	mtx_lock(&Giant);
	if ( !ifp || !(ifp->if_flags & IFF_POLLING) ) {
		mtx_unlock(&Giant);
		return 0;
	}
	for (i = 0 ; i < poll_handlers ; i++)
		if (pr[i].ifp == ifp) /* found it */
			break;
	ifp->if_flags &= ~IFF_POLLING; /* found or not... */
	if (i == poll_handlers) {
		mtx_unlock(&Giant);
		printf("ether_poll_deregister: ifp not found!!!\n");
		return 0;
	}
	poll_handlers--;
	if (i < poll_handlers) { /* Last entry replaces this one. */
		pr[i].handler = pr[poll_handlers].handler;
		pr[i].ifp = pr[poll_handlers].ifp;
	}
	mtx_unlock(&Giant);
	return 1;
}

static void
poll_idle(void)
{
	struct thread *td = curthread;
	struct rtprio rtp;
	int pri;

	rtp.prio = RTP_PRIO_MAX;	/* lowest priority */
	rtp.type = RTP_PRIO_IDLE;
	mtx_lock_spin(&sched_lock);
	rtp_to_pri(&rtp, td->td_ksegrp);
	pri = td->td_priority;
	mtx_unlock_spin(&sched_lock);

	for (;;) {
		if (poll_in_idle_loop && poll_handlers > 0) {
			idlepoll_sleeping = 0;
			mtx_lock(&Giant);
			ether_poll(poll_each_burst);
			mtx_unlock(&Giant);
			mtx_assert(&Giant, MA_NOTOWNED);
			mtx_lock_spin(&sched_lock);
			mi_switch(SW_VOL, NULL);
			mtx_unlock_spin(&sched_lock);
		} else {
			idlepoll_sleeping = 1;
			tsleep(&idlepoll_sleeping, pri, "pollid", hz * 3);
		}
	}
}

static struct proc *idlepoll;
static struct kproc_desc idlepoll_kp = {
	 "idlepoll",
	 poll_idle,
	 &idlepoll
};
SYSINIT(idlepoll, SI_SUB_KTHREAD_VM, SI_ORDER_ANY, kproc_start, &idlepoll_kp)
