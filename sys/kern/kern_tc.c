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

#include "opt_ntp.h"

#include <sys/param.h>
#include <sys/timetc.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/timex.h>
#include <sys/timepps.h>

/*
 * Number of timecounters used to implement stable storage
 */
#ifndef NTIMECOUNTER
#define NTIMECOUNTER	hz
#endif

static MALLOC_DEFINE(M_TIMECOUNTER, "timecounter", 
	"Timecounter stable storage");

static void tco_setscales __P((struct timecounter *tc));
static __inline unsigned tco_delta __P((struct timecounter *tc));

time_t time_second;

struct	timeval boottime;
SYSCTL_STRUCT(_kern, KERN_BOOTTIME, boottime, CTLFLAG_RD,
    &boottime, timeval, "System boottime");

SYSCTL_NODE(_kern, OID_AUTO, timecounter, CTLFLAG_RW, 0, "");

static unsigned nmicrotime;
static unsigned nnanotime;
static unsigned ngetmicrotime;
static unsigned ngetnanotime;
static unsigned nmicrouptime;
static unsigned nnanouptime;
static unsigned ngetmicrouptime;
static unsigned ngetnanouptime;
SYSCTL_INT(_kern_timecounter, OID_AUTO, nmicrotime, CTLFLAG_RD, &nmicrotime, 0, "");
SYSCTL_INT(_kern_timecounter, OID_AUTO, nnanotime, CTLFLAG_RD, &nnanotime, 0, "");
SYSCTL_INT(_kern_timecounter, OID_AUTO, nmicrouptime, CTLFLAG_RD, &nmicrouptime, 0, "");
SYSCTL_INT(_kern_timecounter, OID_AUTO, nnanouptime, CTLFLAG_RD, &nnanouptime, 0, "");
SYSCTL_INT(_kern_timecounter, OID_AUTO, ngetmicrotime, CTLFLAG_RD, &ngetmicrotime, 0, "");
SYSCTL_INT(_kern_timecounter, OID_AUTO, ngetnanotime, CTLFLAG_RD, &ngetnanotime, 0, "");
SYSCTL_INT(_kern_timecounter, OID_AUTO, ngetmicrouptime, CTLFLAG_RD, &ngetmicrouptime, 0, "");
SYSCTL_INT(_kern_timecounter, OID_AUTO, ngetnanouptime, CTLFLAG_RD, &ngetnanouptime, 0, "");

/*
 * Implement a dummy timecounter which we can use until we get a real one
 * in the air.  This allows the console and other early stuff to use
 * timeservices.
 */

static unsigned 
dummy_get_timecount(struct timecounter *tc)
{
	static unsigned now;

	return (++now);
}

static struct timecounter dummy_timecounter = {
	dummy_get_timecount,
	0,
	~0u,
	1000000,
	"dummy"
};

struct timecounter *volatile timecounter = &dummy_timecounter;

static __inline unsigned
tco_delta(struct timecounter *tc)
{

	return ((tc->tc_get_timecount(tc) - tc->tc_offset_count) & 
	    tc->tc_counter_mask);
}

/*
 * We have eight functions for looking at the clock, four for
 * microseconds and four for nanoseconds.  For each there is fast
 * but less precise version "get{nano|micro}[up]time" which will
 * return a time which is up to 1/HZ previous to the call, whereas
 * the raw version "{nano|micro}[up]time" will return a timestamp
 * which is as precise as possible.  The "up" variants return the
 * time relative to system boot, these are well suited for time
 * interval measurements.
 */

void
getmicrotime(struct timeval *tvp)
{
	struct timecounter *tc;

	ngetmicrotime++;
	tc = timecounter;
	*tvp = tc->tc_microtime;
}

void
getnanotime(struct timespec *tsp)
{
	struct timecounter *tc;

	ngetnanotime++;
	tc = timecounter;
	*tsp = tc->tc_nanotime;
}

void
microtime(struct timeval *tv)
{
	struct timecounter *tc;

	nmicrotime++;
	tc = timecounter;
	tv->tv_sec = tc->tc_offset_sec;
	tv->tv_usec = tc->tc_offset_micro;
	tv->tv_usec += ((u_int64_t)tco_delta(tc) * tc->tc_scale_micro) >> 32;
	tv->tv_usec += boottime.tv_usec;
	tv->tv_sec += boottime.tv_sec;
	while (tv->tv_usec >= 1000000) {
		tv->tv_usec -= 1000000;
		tv->tv_sec++;
	}
}

void
nanotime(struct timespec *ts)
{
	unsigned count;
	u_int64_t delta;
	struct timecounter *tc;

	nnanotime++;
	tc = timecounter;
	ts->tv_sec = tc->tc_offset_sec;
	count = tco_delta(tc);
	delta = tc->tc_offset_nano;
	delta += ((u_int64_t)count * tc->tc_scale_nano_f);
	delta >>= 32;
	delta += ((u_int64_t)count * tc->tc_scale_nano_i);
	delta += boottime.tv_usec * 1000;
	ts->tv_sec += boottime.tv_sec;
	while (delta >= 1000000000) {
		delta -= 1000000000;
		ts->tv_sec++;
	}
	ts->tv_nsec = delta;
}

void
getmicrouptime(struct timeval *tvp)
{
	struct timecounter *tc;

	ngetmicrouptime++;
	tc = timecounter;
	tvp->tv_sec = tc->tc_offset_sec;
	tvp->tv_usec = tc->tc_offset_micro;
}

void
getnanouptime(struct timespec *tsp)
{
	struct timecounter *tc;

	ngetnanouptime++;
	tc = timecounter;
	tsp->tv_sec = tc->tc_offset_sec;
	tsp->tv_nsec = tc->tc_offset_nano >> 32;
}

void
microuptime(struct timeval *tv)
{
	struct timecounter *tc;

	nmicrouptime++;
	tc = timecounter;
	tv->tv_sec = tc->tc_offset_sec;
	tv->tv_usec = tc->tc_offset_micro;
	tv->tv_usec += ((u_int64_t)tco_delta(tc) * tc->tc_scale_micro) >> 32;
	while (tv->tv_usec >= 1000000) {
		tv->tv_usec -= 1000000;
		tv->tv_sec++;
	}
}

void
nanouptime(struct timespec *ts)
{
	unsigned count;
	u_int64_t delta;
	struct timecounter *tc;

	nnanouptime++;
	tc = timecounter;
	ts->tv_sec = tc->tc_offset_sec;
	count = tco_delta(tc);
	delta = tc->tc_offset_nano;
	delta += ((u_int64_t)count * tc->tc_scale_nano_f);
	delta >>= 32;
	delta += ((u_int64_t)count * tc->tc_scale_nano_i);
	while (delta >= 1000000000) {
		delta -= 1000000000;
		ts->tv_sec++;
	}
	ts->tv_nsec = delta;
}

static void
tco_setscales(struct timecounter *tc)
{
	u_int64_t scale;

	scale = 1000000000LL << 32;
	scale += tc->tc_adjustment;
	scale /= tc->tc_tweak->tc_frequency;
	tc->tc_scale_micro = scale / 1000;
	tc->tc_scale_nano_f = scale & 0xffffffff;
	tc->tc_scale_nano_i = scale >> 32;
}

void
tc_update(struct timecounter *tc)
{
	tco_setscales(tc);
}

void
tc_init(struct timecounter *tc)
{
	struct timespec ts1;
	struct timecounter *t1, *t2, *t3;
	int i;

	tc->tc_adjustment = 0;
	tc->tc_tweak = tc;
	tco_setscales(tc);
	tc->tc_offset_count = tc->tc_get_timecount(tc);
	if (timecounter == &dummy_timecounter)
		tc->tc_avail = tc;
	else {
		tc->tc_avail = timecounter->tc_tweak->tc_avail;
		timecounter->tc_tweak->tc_avail = tc;
	}
	MALLOC(t1, struct timecounter *, sizeof *t1, M_TIMECOUNTER, M_WAITOK);
	tc->tc_other = t1;
	*t1 = *tc;
	t2 = t1;
	t3 = NULL;
	for (i = 1; i < NTIMECOUNTER; i++) {
		MALLOC(t3, struct timecounter *, sizeof *t3,
		    M_TIMECOUNTER, M_WAITOK);
		*t3 = *tc;
		t3->tc_other = t2;
		t2 = t3;
	}
	t1->tc_other = t3;
	tc = t1;

	printf("Timecounter \"%s\"  frequency %lu Hz\n", 
	    tc->tc_name, (u_long)tc->tc_frequency);

	/* XXX: For now always start using the counter. */
	tc->tc_offset_count = tc->tc_get_timecount(tc);
	nanouptime(&ts1);
	tc->tc_offset_nano = (u_int64_t)ts1.tv_nsec << 32;
	tc->tc_offset_micro = ts1.tv_nsec / 1000;
	tc->tc_offset_sec = ts1.tv_sec;
	timecounter = tc;
}

void
tc_setclock(struct timespec *ts)
{
	struct timespec ts2;

	nanouptime(&ts2);
	boottime.tv_sec = ts->tv_sec - ts2.tv_sec;
	boottime.tv_usec = (ts->tv_nsec - ts2.tv_nsec) / 1000;
	if (boottime.tv_usec < 0) {
		boottime.tv_usec += 1000000;
		boottime.tv_sec--;
	}
	/* fiddle all the little crinkly bits around the fiords... */
	tc_windup();
}

static void
switch_timecounter(struct timecounter *newtc)
{
	int s;
	struct timecounter *tc;
	struct timespec ts;

	s = splclock();
	tc = timecounter;
	if (newtc->tc_tweak == tc->tc_tweak) {
		splx(s);
		return;
	}
	newtc = newtc->tc_tweak->tc_other;
	nanouptime(&ts);
	newtc->tc_offset_sec = ts.tv_sec;
	newtc->tc_offset_nano = (u_int64_t)ts.tv_nsec << 32;
	newtc->tc_offset_micro = ts.tv_nsec / 1000;
	newtc->tc_offset_count = newtc->tc_get_timecount(newtc);
	tco_setscales(newtc);
	timecounter = newtc;
	splx(s);
}

static struct timecounter *
sync_other_counter(void)
{
	struct timecounter *tc, *tcn, *tco;
	unsigned delta;

	tco = timecounter;
	tc = tco->tc_other;
	tcn = tc->tc_other;
	*tc = *tco;
	tc->tc_other = tcn;
	delta = tco_delta(tc);
	tc->tc_offset_count += delta;
	tc->tc_offset_count &= tc->tc_counter_mask;
	tc->tc_offset_nano += (u_int64_t)delta * tc->tc_scale_nano_f;
	tc->tc_offset_nano += (u_int64_t)delta * tc->tc_scale_nano_i << 32;
	return (tc);
}

void
tc_windup(void)
{
	struct timecounter *tc, *tco;
	struct timeval tvt;

	tco = timecounter;
	tc = sync_other_counter();
	/*
	 * We may be inducing a tiny error here, the tc_poll_pps() may
	 * process a latched count which happens after the tco_delta()
	 * in sync_other_counter(), which would extend the previous
	 * counters parameters into the domain of this new one.
	 * Since the timewindow is very small for this, the error is
	 * going to be only a few weenieseconds (as Dave Mills would
	 * say), so lets just not talk more about it, OK ?
	 */
	if (tco->tc_poll_pps) 
		tco->tc_poll_pps(tco);
	if (timedelta != 0) {
		tvt = boottime;
		tvt.tv_usec += tickdelta;
		if (tvt.tv_usec >= 1000000) {
			tvt.tv_sec++;
			tvt.tv_usec -= 1000000;
		} else if (tvt.tv_usec < 0) {
			tvt.tv_sec--;
			tvt.tv_usec += 1000000;
		}
		boottime = tvt;
		timedelta -= tickdelta;
	}

	while (tc->tc_offset_nano >= 1000000000ULL << 32) {
		tc->tc_offset_nano -= 1000000000ULL << 32;
		tc->tc_offset_sec++;
		ntp_update_second(tc);	/* XXX only needed if xntpd runs */
		tco_setscales(tc);
	}

	tc->tc_offset_micro = (tc->tc_offset_nano / 1000) >> 32;

	/* Figure out the wall-clock time */
	tc->tc_nanotime.tv_sec = tc->tc_offset_sec + boottime.tv_sec;
	tc->tc_nanotime.tv_nsec = 
	    (tc->tc_offset_nano >> 32) + boottime.tv_usec * 1000;
	tc->tc_microtime.tv_usec = tc->tc_offset_micro + boottime.tv_usec;
	if (tc->tc_nanotime.tv_nsec >= 1000000000) {
		tc->tc_nanotime.tv_nsec -= 1000000000;
		tc->tc_microtime.tv_usec -= 1000000;
		tc->tc_nanotime.tv_sec++;
	}
	time_second = tc->tc_microtime.tv_sec = tc->tc_nanotime.tv_sec;

	timecounter = tc;
}

static int
sysctl_kern_timecounter_hardware(SYSCTL_HANDLER_ARGS)
{
	char newname[32];
	struct timecounter *newtc, *tc;
	int error;

	tc = timecounter->tc_tweak;
	strncpy(newname, tc->tc_name, sizeof(newname));
	error = sysctl_handle_string(oidp, &newname[0], sizeof(newname), req);
	if (error == 0 && req->newptr != NULL &&
	    strcmp(newname, tc->tc_name) != 0) {
		for (newtc = tc->tc_avail; newtc != tc;
		    newtc = newtc->tc_avail) {
			if (strcmp(newname, newtc->tc_name) == 0) {
				/* Warm up new timecounter. */
				(void)newtc->tc_get_timecount(newtc);

				switch_timecounter(newtc);
				return (0);
			}
		}
		return (EINVAL);
	}
	return (error);
}

SYSCTL_PROC(_kern_timecounter, OID_AUTO, hardware, CTLTYPE_STRING | CTLFLAG_RW,
    0, 0, sysctl_kern_timecounter_hardware, "A", "");


int
pps_ioctl(u_long cmd, caddr_t data, struct pps_state *pps)
{
	pps_params_t *app;
	struct pps_fetch_args *fapi;
#ifdef PPS_SYNC
	struct pps_kcbind_args *kapi;
#endif

	switch (cmd) {
	case PPS_IOC_CREATE:
		return (0);
	case PPS_IOC_DESTROY:
		return (0);
	case PPS_IOC_SETPARAMS:
		app = (pps_params_t *)data;
		if (app->mode & ~pps->ppscap)
			return (EINVAL);
		pps->ppsparam = *app;         
		return (0);
	case PPS_IOC_GETPARAMS:
		app = (pps_params_t *)data;
		*app = pps->ppsparam;
		app->api_version = PPS_API_VERS_1;
		return (0);
	case PPS_IOC_GETCAP:
		*(int*)data = pps->ppscap;
		return (0);
	case PPS_IOC_FETCH:
		fapi = (struct pps_fetch_args *)data;
		if (fapi->tsformat && fapi->tsformat != PPS_TSFMT_TSPEC)
			return (EINVAL);
		if (fapi->timeout.tv_sec || fapi->timeout.tv_nsec)
			return (EOPNOTSUPP);
		pps->ppsinfo.current_mode = pps->ppsparam.mode;         
		fapi->pps_info_buf = pps->ppsinfo;
		return (0);
	case PPS_IOC_KCBIND:
#ifdef PPS_SYNC
		kapi = (struct pps_kcbind_args *)data;
		/* XXX Only root should be able to do this */
		if (kapi->tsformat && kapi->tsformat != PPS_TSFMT_TSPEC)
			return (EINVAL);
		if (kapi->kernel_consumer != PPS_KC_HARDPPS)
			return (EINVAL);
		if (kapi->edge & ~pps->ppscap)
			return (EINVAL);
		pps->kcmode = kapi->edge;
		return (0);
#else
		return (EOPNOTSUPP);
#endif
	default:
		return (ENOTTY);
	}
}

void
pps_init(struct pps_state *pps)
{
	pps->ppscap |= PPS_TSFMT_TSPEC;
	if (pps->ppscap & PPS_CAPTUREASSERT)
		pps->ppscap |= PPS_OFFSETASSERT;
	if (pps->ppscap & PPS_CAPTURECLEAR)
		pps->ppscap |= PPS_OFFSETCLEAR;
}

void
pps_event(struct pps_state *pps, struct timecounter *tc, unsigned count, int event)
{
	struct timespec ts, *tsp, *osp;
	u_int64_t delta;
	unsigned tcount, *pcount;
	int foff, fhard;
	pps_seq_t	*pseq;

	/* Things would be easier with arrays... */
	if (event == PPS_CAPTUREASSERT) {
		tsp = &pps->ppsinfo.assert_timestamp;
		osp = &pps->ppsparam.assert_offset;
		foff = pps->ppsparam.mode & PPS_OFFSETASSERT;
		fhard = pps->kcmode & PPS_CAPTUREASSERT;
		pcount = &pps->ppscount[0];
		pseq = &pps->ppsinfo.assert_sequence;
	} else {
		tsp = &pps->ppsinfo.clear_timestamp;
		osp = &pps->ppsparam.clear_offset;
		foff = pps->ppsparam.mode & PPS_OFFSETCLEAR;
		fhard = pps->kcmode & PPS_CAPTURECLEAR;
		pcount = &pps->ppscount[1];
		pseq = &pps->ppsinfo.clear_sequence;
	}

	/* The timecounter changed: bail */
	if (!pps->ppstc || 
	    pps->ppstc->tc_name != tc->tc_name || 
	    tc->tc_name != timecounter->tc_name) {
		pps->ppstc = tc;
		*pcount = count;
		return;
	}

	/* Nothing really happened */
	if (*pcount == count)
		return;

	*pcount = count;

	/* Convert the count to timespec */
	ts.tv_sec = tc->tc_offset_sec;
	tcount = count - tc->tc_offset_count;
	tcount &= tc->tc_counter_mask;
	delta = tc->tc_offset_nano;
	delta += ((u_int64_t)tcount * tc->tc_scale_nano_f);
	delta >>= 32;
	delta += ((u_int64_t)tcount * tc->tc_scale_nano_i);
	delta += boottime.tv_usec * 1000;
	ts.tv_sec += boottime.tv_sec;
	while (delta >= 1000000000) {
		delta -= 1000000000;
		ts.tv_sec++;
	}
	ts.tv_nsec = delta;

	(*pseq)++;
	*tsp = ts;

	if (foff) {
		timespecadd(tsp, osp);
		if (tsp->tv_nsec < 0) {
			tsp->tv_nsec += 1000000000;
			tsp->tv_sec -= 1;
		}
	}
#ifdef PPS_SYNC
	if (fhard) {
		/* magic, at its best... */
		tcount = count - pps->ppscount[2];
		pps->ppscount[2] = count;
		tcount &= tc->tc_counter_mask;
		delta = ((u_int64_t)tcount * tc->tc_tweak->tc_scale_nano_f);
		delta >>= 32;
		delta += ((u_int64_t)tcount * tc->tc_tweak->tc_scale_nano_i);
		hardpps(tsp, delta);
	}
#endif
}
