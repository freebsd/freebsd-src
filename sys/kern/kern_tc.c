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

time_t time_second;

struct	bintime boottimebin;
struct	timeval boottime;
SYSCTL_STRUCT(_kern, KERN_BOOTTIME, boottime, CTLFLAG_RD,
    &boottime, timeval, "System boottime");

SYSCTL_NODE(_kern, OID_AUTO, timecounter, CTLFLAG_RW, 0, "");

#define TC_STATS(foo) \
	static unsigned foo; \
	SYSCTL_INT(_kern_timecounter, OID_AUTO, foo, CTLFLAG_RD, & foo, 0, "")

TC_STATS(nbinuptime);    TC_STATS(nnanouptime);    TC_STATS(nmicrouptime);
TC_STATS(nbintime);      TC_STATS(nnanotime);      TC_STATS(nmicrotime);
TC_STATS(ngetbinuptime); TC_STATS(ngetnanouptime); TC_STATS(ngetmicrouptime);
TC_STATS(ngetbintime);   TC_STATS(ngetnanotime);   TC_STATS(ngetmicrotime);

#undef TC_STATS

static void tc_windup(void);

/*
 * Implement a dummy timecounter which we can use until we get a real one
 * in the air.  This allows the console and other early stuff to use
 * timeservices.
 */

static unsigned 
dummy_get_timecount(struct timecounter *tc)
{
	static unsigned now;

	if (tc->tc_generation == 0)
		tc->tc_generation = 1;
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
tc_delta(struct timecounter *tc)
{

	return ((tc->tc_get_timecount(tc) - tc->tc_offset_count) & 
	    tc->tc_counter_mask);
}

void
binuptime(struct bintime *bt)
{
	struct timecounter *tc;
	unsigned gen;

	nbinuptime++;
	do {
		tc = timecounter;
		gen = tc->tc_generation;
		*bt = tc->tc_offset;
		bintime_addx(bt, tc->tc_scale * tc_delta(tc));
	} while (gen == 0 || gen != tc->tc_generation);
}

void
nanouptime(struct timespec *ts)
{
	struct bintime bt;

	nnanouptime++;
	binuptime(&bt);
	bintime2timespec(&bt, ts);
}

void
microuptime(struct timeval *tv)
{
	struct bintime bt;

	nmicrouptime++;
	binuptime(&bt);
	bintime2timeval(&bt, tv);
}

void
bintime(struct bintime *bt)
{

	nbintime++;
	binuptime(bt);
	bintime_add(bt, &boottimebin);
}

void
nanotime(struct timespec *ts)
{
	struct bintime bt;

	nnanotime++;
	bintime(&bt);
	bintime2timespec(&bt, ts);
}

void
microtime(struct timeval *tv)
{
	struct bintime bt;

	nmicrotime++;
	bintime(&bt);
	bintime2timeval(&bt, tv);
}

void
getbinuptime(struct bintime *bt)
{
	struct timecounter *tc;
	unsigned gen;

	ngetbinuptime++;
	do {
		tc = timecounter;
		gen = tc->tc_generation;
		*bt = tc->tc_offset;
	} while (gen == 0 || gen != tc->tc_generation);
}

void
getnanouptime(struct timespec *tsp)
{
	struct timecounter *tc;
	unsigned gen;

	ngetnanouptime++;
	do {
		tc = timecounter;
		gen = tc->tc_generation;
		bintime2timespec(&tc->tc_offset, tsp);
	} while (gen == 0 || gen != tc->tc_generation);
}

void
getmicrouptime(struct timeval *tvp)
{
	struct timecounter *tc;
	unsigned gen;

	ngetmicrouptime++;
	do {
		tc = timecounter;
		gen = tc->tc_generation;
		bintime2timeval(&tc->tc_offset, tvp);
	} while (gen == 0 || gen != tc->tc_generation);
}

void
getbintime(struct bintime *bt)
{
	struct timecounter *tc;
	unsigned gen;

	ngetbintime++;
	do {
		tc = timecounter;
		gen = tc->tc_generation;
		*bt = tc->tc_offset;
	} while (gen == 0 || gen != tc->tc_generation);
	bintime_add(bt, &boottimebin);
}

void
getnanotime(struct timespec *tsp)
{
	struct timecounter *tc;
	unsigned gen;

	ngetnanotime++;
	do {
		tc = timecounter;
		gen = tc->tc_generation;
		*tsp = tc->tc_nanotime;
	} while (gen == 0 || gen != tc->tc_generation);
}

void
getmicrotime(struct timeval *tvp)
{
	struct timecounter *tc;
	unsigned gen;

	ngetmicrotime++;
	do {
		tc = timecounter;
		gen = tc->tc_generation;
		*tvp = tc->tc_microtime;
	} while (gen == 0 || gen != tc->tc_generation);
}

static void
tc_setscales(struct timecounter *tc)
{
	u_int64_t scale;

	/* Sacrifice the lower bit to the deity for code clarity */
	scale = 1ULL << 63;
	/* 
	 * We get nanoseconds with 32 bit binary fraction and want
	 * 64 bit binary fraction: x = a * 2^32 / 10^9 = a * 4.294967296
	 * The range is +/- 5000PPM so we can only multiply by about 850
	 * without overflowing.  The best suitable fraction is 2199/512.
	 * Divide by 2 times 512 to match the temporary lower precision.
	 */
	scale += (tc->tc_adjustment / 1024) * 2199;
	scale /= tc->tc_tweak->tc_frequency;
	tc->tc_scale = scale * 2;
}

void
tc_init(struct timecounter *tc)
{
	struct timecounter *t1, *t2, *t3;
	int i;

	tc->tc_adjustment = 0;
	tc->tc_tweak = tc;
	tc_setscales(tc);
	tc->tc_offset_count = tc->tc_get_timecount(tc);
	if (timecounter == &dummy_timecounter)
		tc->tc_avail = tc;
	else {
		tc->tc_avail = timecounter->tc_tweak->tc_avail;
		timecounter->tc_tweak->tc_avail = tc;
	}
	MALLOC(t1, struct timecounter *, sizeof *t1, M_TIMECOUNTER, M_WAITOK | M_ZERO);
	tc->tc_next = t1;
	*t1 = *tc;
	t2 = t1;
	t3 = NULL;
	for (i = 1; i < NTIMECOUNTER; i++) {
		MALLOC(t3, struct timecounter *, sizeof *t3,
		    M_TIMECOUNTER, M_WAITOK | M_ZERO);
		*t3 = *tc;
		t3->tc_next = t2;
		t2 = t3;
	}
	t1->tc_next = t3;
	tc = t1;

	printf("Timecounter \"%s\"  frequency %lu Hz\n", 
	    tc->tc_name, (u_long)tc->tc_frequency);

	/* XXX: For now always start using the counter. */
	tc->tc_offset_count = tc->tc_get_timecount(tc);
	binuptime(&tc->tc_offset);
	timecounter = tc;
	tc_windup();
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
	timeval2bintime(&boottime, &boottimebin);
	/* fiddle all the little crinkly bits around the fiords... */
	tc_windup();
}

static void
switch_timecounter(struct timecounter *newtc)
{
	int s;
	struct timecounter *tc;

	s = splclock();
	tc = timecounter;
	if (newtc->tc_tweak == tc->tc_tweak) {
		splx(s);
		return;
	}
	newtc = newtc->tc_tweak->tc_next;
	binuptime(&newtc->tc_offset);
	newtc->tc_offset_count = newtc->tc_get_timecount(newtc);
	tc_setscales(newtc);
	newtc->tc_generation = 0;
	timecounter = newtc;
	tc_windup();
	splx(s);
}

static void
tc_windup(void)
{
	struct timecounter *tc, *tco;
	struct bintime bt;
	unsigned ogen, delta;
	int i;

	tco = timecounter;
	tc = tco->tc_next;
	ogen = tc->tc_generation;
	tc->tc_generation = 0;
	bcopy(tco, tc, __offsetof(struct timecounter, tc_generation));
	delta = tc_delta(tc);
	tc->tc_offset_count += delta;
	tc->tc_offset_count &= tc->tc_counter_mask;
	bintime_addx(&tc->tc_offset, tc->tc_scale * delta);
	/*
	 * We may be inducing a tiny error here, the tc_poll_pps() may
	 * process a latched count which happens after the tc_delta()
	 * in sync_other_counter(), which would extend the previous
	 * counters parameters into the domain of this new one.
	 * Since the timewindow is very small for this, the error is
	 * going to be only a few weenieseconds (as Dave Mills would
	 * say), so lets just not talk more about it, OK ?
	 */
	if (tco->tc_poll_pps) 
		tco->tc_poll_pps(tco);
	for (i = tc->tc_offset.sec - tco->tc_offset.sec; i > 0; i--) {
		ntp_update_second(tc);	/* XXX only needed if xntpd runs */
		tc_setscales(tc);
	}

	bt = tc->tc_offset;
	bintime_add(&bt, &boottimebin);
	bintime2timeval(&bt, &tc->tc_microtime);
	bintime2timespec(&bt, &tc->tc_nanotime);
	ogen++;
	if (ogen == 0)
		ogen++;
	tc->tc_generation = ogen;
	time_second = tc->tc_microtime.tv_sec;
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
pps_capture(struct pps_state *pps)
{
	struct timecounter *tc;

	tc = timecounter;
	pps->captc = tc;
	pps->capgen = tc->tc_generation;
	pps->capcount = tc->tc_get_timecount(tc);
}

void
pps_event(struct pps_state *pps, int event)
{
	struct timespec ts, *tsp, *osp;
	unsigned tcount, *pcount;
	struct bintime bt;
	int foff, fhard;
	pps_seq_t	*pseq;

	/* If the timecounter were wound up, bail. */
	if (pps->capgen != pps->capgen)
		return;

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
	    pps->ppstc->tc_name != pps->captc->tc_name || 
	    pps->captc->tc_name != timecounter->tc_name) {
		pps->ppstc = pps->captc;
		*pcount = pps->capcount;
#ifdef PPS_SYNC
		pps->ppscount[2] = pps->capcount;
#endif
		return;
	}

	/* Nothing really happened */
	if (*pcount == pps->capcount)
		return;

	/* Convert the count to timespec */
	tcount = pps->capcount - pps->captc->tc_offset_count;
	tcount &= pps->captc->tc_counter_mask;
	bt = pps->captc->tc_offset;
	bintime_addx(&bt, pps->captc->tc_scale * tcount);
	bintime2timespec(&bt, &ts);

	/* If the timecounter were wound up, bail. */
	if (pps->capgen != pps->capgen)
		return;

	*pcount = pps->capcount;
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
		tcount = pps->capcount - pps->ppscount[2];
		pps->ppscount[2] = pps->capcount;
		tcount &= pps->captc->tc_counter_mask;
		bt.sec = 0;
		bt.frac = 0;
		bintime_addx(&bt, pps->captc->tc_scale * tcount);
		bintime2timespec(&bt, &ts);
		hardpps(tsp, ts.tv_nsec + 1000000000 * ts.tv_sec);
	}
#endif
}

/*-
 * Timecounters need to be updated every so often to prevent the hardware
 * counter from overflowing.  Updating also recalculates the cached values
 * used by the get*() family of functions, so their precision depends on
 * the update frequency.
 * Don't update faster than approx once per millisecond, if people want
 * better timestamps they should use the non-"get" functions.
 */

static int tc_tick;
SYSCTL_INT(_kern_timecounter, OID_AUTO, tick, CTLFLAG_RD, &tick, 0, "");

static void
tc_ticktock(void *dummy)
{

	tc_windup();
	timeout(tc_ticktock, NULL, tc_tick);
}

static void 
inittimecounter(void *dummy)
{
	u_int p;

	if (hz > 1000)
		tc_tick = (hz + 500) / 1000;
	else
		tc_tick = 1;
	p = (tc_tick * 1000000) / hz;
	printf("Timecounters tick every %d.%03u msec\n", p / 1000, p % 1000);
	tc_ticktock(NULL);
}

SYSINIT(timecounter, SI_SUB_CLOCKS, SI_ORDER_FIRST, inittimecounter, NULL)
