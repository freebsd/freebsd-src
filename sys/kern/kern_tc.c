/*-
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.ORG> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ntp.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/timepps.h>
#include <sys/timetc.h>
#include <sys/timex.h>

/*
 * a large step happens on boot.  This constant detects such 
 * a steps.  It is relatively small so that ntp_update_second gets called
 * enough in the typical 'missed a couple of seconds' case, but doesn't
 * loop forever when the time step is large.
 */
#define LARGE_STEP	200

/*
 * Implement a dummy timecounter which we can use until we get a real one
 * in the air.  This allows the console and other early stuff to use
 * time services.
 */

static u_int
dummy_get_timecount(struct timecounter *tc)
{
	static u_int now;

	return (++now);
}

static struct timecounter dummy_timecounter = {
	dummy_get_timecount, 0, ~0u, 1000000, "dummy",
};

struct timehands {
	/* These fields must be initialized by the driver. */
	struct timecounter	*th_counter;
	int64_t			th_adjustment;
	u_int64_t		th_scale;
	u_int	 		th_offset_count;
	struct bintime		th_offset;
	struct timeval		th_microtime;
	struct timespec		th_nanotime;
	/* Fields not to be copied in tc_windup start with th_generation. */
	volatile u_int		th_generation;
	struct timehands	*th_next;
};

extern struct timehands th0;
static struct timehands th9 = { NULL, 0, 0, 0, {0, 0}, {0, 0}, {0, 0}, 0, &th0};
static struct timehands th8 = { NULL, 0, 0, 0, {0, 0}, {0, 0}, {0, 0}, 0, &th9};
static struct timehands th7 = { NULL, 0, 0, 0, {0, 0}, {0, 0}, {0, 0}, 0, &th8};
static struct timehands th6 = { NULL, 0, 0, 0, {0, 0}, {0, 0}, {0, 0}, 0, &th7};
static struct timehands th5 = { NULL, 0, 0, 0, {0, 0}, {0, 0}, {0, 0}, 0, &th6};
static struct timehands th4 = { NULL, 0, 0, 0, {0, 0}, {0, 0}, {0, 0}, 0, &th5};
static struct timehands th3 = { NULL, 0, 0, 0, {0, 0}, {0, 0}, {0, 0}, 0, &th4};
static struct timehands th2 = { NULL, 0, 0, 0, {0, 0}, {0, 0}, {0, 0}, 0, &th3};
static struct timehands th1 = { NULL, 0, 0, 0, {0, 0}, {0, 0}, {0, 0}, 0, &th2};
static struct timehands th0 = {
	&dummy_timecounter,
	0,
	(uint64_t)-1 / 1000000,
	0,
	{1, 0},
	{0, 0},
	{0, 0},
	1,
	&th1
};

static struct timehands *volatile timehands = &th0;
struct timecounter *timecounter = &dummy_timecounter;
static struct timecounter *timecounters = &dummy_timecounter;

time_t time_second = 1;
time_t time_uptime = 0;

static struct bintime boottimebin;
struct timeval boottime;
SYSCTL_STRUCT(_kern, KERN_BOOTTIME, boottime, CTLFLAG_RD,
    &boottime, timeval, "System boottime");

SYSCTL_NODE(_kern, OID_AUTO, timecounter, CTLFLAG_RW, 0, "");

#define TC_STATS(foo) \
	static u_int foo; \
	SYSCTL_UINT(_kern_timecounter, OID_AUTO, foo, CTLFLAG_RD, &foo, 0, "");\
	struct __hack

TC_STATS(nbinuptime);    TC_STATS(nnanouptime);    TC_STATS(nmicrouptime);
TC_STATS(nbintime);      TC_STATS(nnanotime);      TC_STATS(nmicrotime);
TC_STATS(ngetbinuptime); TC_STATS(ngetnanouptime); TC_STATS(ngetmicrouptime);
TC_STATS(ngetbintime);   TC_STATS(ngetnanotime);   TC_STATS(ngetmicrotime);
TC_STATS(nsetclock);

#undef TC_STATS

static void tc_windup(void);

/*
 * Return the difference between the timehands' counter value now and what
 * was when we copied it to the timehands' offset_count.
 */
static __inline u_int
tc_delta(struct timehands *th)
{
	struct timecounter *tc;

	tc = th->th_counter;
	return ((tc->tc_get_timecount(tc) - th->th_offset_count) &
	    tc->tc_counter_mask);
}

/*
 * Functions for reading the time.  We have to loop until we are sure that
 * the timehands that we operated on was not updated under our feet.  See
 * the comment in <sys/time.h> for a description of these 12 functions.
 */

void
binuptime(struct bintime *bt)
{
	struct timehands *th;
	u_int gen;

	nbinuptime++;
	do {
		th = timehands;
		gen = th->th_generation;
		*bt = th->th_offset;
		bintime_addx(bt, th->th_scale * tc_delta(th));
	} while (gen == 0 || gen != th->th_generation);
}

void
nanouptime(struct timespec *tsp)
{
	struct bintime bt;

	nnanouptime++;
	binuptime(&bt);
	bintime2timespec(&bt, tsp);
}

void
microuptime(struct timeval *tvp)
{
	struct bintime bt;

	nmicrouptime++;
	binuptime(&bt);
	bintime2timeval(&bt, tvp);
}

void
bintime(struct bintime *bt)
{

	nbintime++;
	binuptime(bt);
	bintime_add(bt, &boottimebin);
}

void
nanotime(struct timespec *tsp)
{
	struct bintime bt;

	nnanotime++;
	bintime(&bt);
	bintime2timespec(&bt, tsp);
}

void
microtime(struct timeval *tvp)
{
	struct bintime bt;

	nmicrotime++;
	bintime(&bt);
	bintime2timeval(&bt, tvp);
}

void
getbinuptime(struct bintime *bt)
{
	struct timehands *th;
	u_int gen;

	ngetbinuptime++;
	do {
		th = timehands;
		gen = th->th_generation;
		*bt = th->th_offset;
	} while (gen == 0 || gen != th->th_generation);
}

void
getnanouptime(struct timespec *tsp)
{
	struct timehands *th;
	u_int gen;

	ngetnanouptime++;
	do {
		th = timehands;
		gen = th->th_generation;
		bintime2timespec(&th->th_offset, tsp);
	} while (gen == 0 || gen != th->th_generation);
}

void
getmicrouptime(struct timeval *tvp)
{
	struct timehands *th;
	u_int gen;

	ngetmicrouptime++;
	do {
		th = timehands;
		gen = th->th_generation;
		bintime2timeval(&th->th_offset, tvp);
	} while (gen == 0 || gen != th->th_generation);
}

void
getbintime(struct bintime *bt)
{
	struct timehands *th;
	u_int gen;

	ngetbintime++;
	do {
		th = timehands;
		gen = th->th_generation;
		*bt = th->th_offset;
	} while (gen == 0 || gen != th->th_generation);
	bintime_add(bt, &boottimebin);
}

void
getnanotime(struct timespec *tsp)
{
	struct timehands *th;
	u_int gen;

	ngetnanotime++;
	do {
		th = timehands;
		gen = th->th_generation;
		*tsp = th->th_nanotime;
	} while (gen == 0 || gen != th->th_generation);
}

void
getmicrotime(struct timeval *tvp)
{
	struct timehands *th;
	u_int gen;

	ngetmicrotime++;
	do {
		th = timehands;
		gen = th->th_generation;
		*tvp = th->th_microtime;
	} while (gen == 0 || gen != th->th_generation);
}

/*
 * Initialize a new timecounter.
 * We should really try to rank the timecounters and intelligently determine
 * if the new timecounter is better than the current one.  This is subject
 * to further study.  For now always use the new timecounter.
 */
void
tc_init(struct timecounter *tc)
{
	unsigned u;

	printf("Timecounter \"%s\"  frequency %ju Hz",
	    tc->tc_name, (intmax_t)tc->tc_frequency);

	u = tc->tc_frequency / tc->tc_counter_mask;
	if (u > hz) {
		printf(" -- Insufficient hz, needs at least %u\n", u);
		return;
	}
	tc->tc_next = timecounters;
	timecounters = tc;
	printf("\n");
	(void)tc->tc_get_timecount(tc);
	(void)tc->tc_get_timecount(tc);
	timecounter = tc;
}

/* Report the frequency of the current timecounter. */
u_int64_t
tc_getfrequency(void)
{

	return (timehands->th_counter->tc_frequency);
}

/*
 * Step our concept of UTC.  This is done by modifying our estimate of
 * when we booted.  XXX: needs futher work.
 */
void
tc_setclock(struct timespec *ts)
{
	struct timespec ts2;

	nsetclock++;
	nanouptime(&ts2);
	boottime.tv_sec = ts->tv_sec - ts2.tv_sec;
	/* XXX boottime should probably be a timespec. */
	boottime.tv_usec = (ts->tv_nsec - ts2.tv_nsec) / 1000;
	if (boottime.tv_usec < 0) {
		boottime.tv_usec += 1000000;
		boottime.tv_sec--;
	}
	timeval2bintime(&boottime, &boottimebin);

	/* XXX fiddle all the little crinkly bits around the fiords... */
	tc_windup();
}

/*
 * Initialize the next struct timehands in the ring and make
 * it the active timehands.  Along the way we might switch to a different
 * timecounter and/or do seconds processing in NTP.  Slightly magic.
 */
static void
tc_windup(void)
{
	struct bintime bt;
	struct timehands *th, *tho;
	u_int64_t scale;
	u_int delta, ncount, ogen;
	int i;
	time_t t;

	/*
	 * Make the next timehands a copy of the current one, but do not
	 * overwrite the generation or next pointer.  While we update
	 * the contents, the generation must be zero.
	 */
	tho = timehands;
	th = tho->th_next;
	ogen = th->th_generation;
	th->th_generation = 0;
	bcopy(tho, th, offsetof(struct timehands, th_generation));

	/*
	 * Capture a timecounter delta on the current timecounter and if
	 * changing timecounters, a counter value from the new timecounter.
	 * Update the offset fields accordingly.
	 */
	delta = tc_delta(th);
	if (th->th_counter != timecounter)
		ncount = timecounter->tc_get_timecount(timecounter);
	else
		ncount = 0;
	th->th_offset_count += delta;
	th->th_offset_count &= th->th_counter->tc_counter_mask;
	bintime_addx(&th->th_offset, th->th_scale * delta);

	/*
	 * Hardware latching timecounters may not generate interrupts on
	 * PPS events, so instead we poll them.  There is a finite risk that
	 * the hardware might capture a count which is later than the one we
	 * got above, and therefore possibly in the next NTP second which might
	 * have a different rate than the current NTP second.  It doesn't
	 * matter in practice.
	 */
	if (tho->th_counter->tc_poll_pps)
		tho->th_counter->tc_poll_pps(tho->th_counter);

 	/*
	 * Compute the UTC time, before any leapsecond adjustments, are
	 * made.
	 */
	bt = th->th_offset;
	bintime_add(&bt, &boottimebin);

	/*
	 * Deal with NTP second processing.  The for loop normally only
	 * iterates once, but in extreme situations it might keep NTP sane
	 * if timeouts are not run for several seconds.  At boot, the
	 * time step can be large when the TOD hardware has been read, so
	 * on really large steps, we call ntp_update_second only once.
	 */
	for (i = bt.sec - tho->th_microtime.tv_sec; i > 0; i--) {
		t = bt.sec;
		ntp_update_second(&th->th_adjustment, &bt.sec);
		if (bt.sec != t)
			boottimebin.sec += bt.sec - t;
		if (i > LARGE_STEP)
			break;
	}

	/* Now is a good time to change timecounters. */
	if (th->th_counter != timecounter) {
		th->th_counter = timecounter;
		th->th_offset_count = ncount;
	}

	/*-
	 * Recalculate the scaling factor.  We want the number of 1/2^64
	 * fractions of a second per period of the hardware counter, taking
	 * into account the th_adjustment factor which the NTP PLL/adjtime(2)
	 * processing provides us with.
	 *
	 * The th_adjustment is nanoseconds per second with 32 bit binary
	 * fraction and we want 64 bit binary fraction of second:
	 *
	 *	 x = a * 2^32 / 10^9 = a * 4.294967296
	 *
	 * The range of th_adjustment is +/- 5000PPM so inside a 64bit int
	 * we can only multiply by about 850 without overflowing, but that
	 * leaves suitably precise fractions for multiply before divide.
	 *
	 * Divide before multiply with a fraction of 2199/512 results in a
	 * systematic undercompensation of 10PPM of th_adjustment.  On a
	 * 5000PPM adjustment this is a 0.05PPM error.  This is acceptable.
 	 *
	 * We happily sacrifice the lowest of the 64 bits of our result
	 * to the goddess of code clarity.
	 *
	 */
	scale = (u_int64_t)1 << 63;
	scale += (th->th_adjustment / 1024) * 2199;
	scale /= th->th_counter->tc_frequency;
	th->th_scale = scale * 2;

	bintime2timeval(&bt, &th->th_microtime);
	bintime2timespec(&bt, &th->th_nanotime);

	/*
	 * Now that the struct timehands is again consistent, set the new
	 * generation number, making sure to not make it zero.
	 */
	if (++ogen == 0)
		ogen = 1;
	th->th_generation = ogen;

	/* Go live with the new struct timehands. */
	time_second = th->th_microtime.tv_sec;
	time_uptime = th->th_offset.sec;
	timehands = th;
}

/* Report or change the active timecounter hardware. */
static int
sysctl_kern_timecounter_hardware(SYSCTL_HANDLER_ARGS)
{
	char newname[32];
	struct timecounter *newtc, *tc;
	int error;

	tc = timecounter;
	strlcpy(newname, tc->tc_name, sizeof(newname));

	error = sysctl_handle_string(oidp, &newname[0], sizeof(newname), req);
	if (error != 0 || req->newptr == NULL ||
	    strcmp(newname, tc->tc_name) == 0)
		return (error);
	for (newtc = timecounters; newtc != NULL; newtc = newtc->tc_next) {
		if (strcmp(newname, newtc->tc_name) != 0)
			continue;

		/* Warm up new timecounter. */
		(void)newtc->tc_get_timecount(newtc);
		(void)newtc->tc_get_timecount(newtc);

		timecounter = newtc;
		return (0);
	}
	return (EINVAL);
}

SYSCTL_PROC(_kern_timecounter, OID_AUTO, hardware, CTLTYPE_STRING | CTLFLAG_RW,
    0, 0, sysctl_kern_timecounter_hardware, "A", "");

/*
 * RFC 2783 PPS-API implementation.
 */

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
	struct timehands *th;

	th = timehands;
	pps->capgen = th->th_generation;
	pps->capth = th;
	pps->capcount = th->th_counter->tc_get_timecount(th->th_counter);
	if (pps->capgen != th->th_generation)
		pps->capgen = 0;
}

void
pps_event(struct pps_state *pps, int event)
{
	struct bintime bt;
	struct timespec ts, *tsp, *osp;
	u_int tcount, *pcount;
	int foff, fhard;
	pps_seq_t *pseq;

	/* If the timecounter was wound up underneath us, bail out. */
	if (pps->capgen == 0 || pps->capgen != pps->capth->th_generation)
		return;

	/* Things would be easier with arrays. */
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

	/*
	 * If the timecounter changed, we cannot compare the count values, so
	 * we have to drop the rest of the PPS-stuff until the next event.
	 */
	if (pps->ppstc != pps->capth->th_counter) {
		pps->ppstc = pps->capth->th_counter;
		*pcount = pps->capcount;
		pps->ppscount[2] = pps->capcount;
		return;
	}

	/* Return if nothing really happened. */
	if (*pcount == pps->capcount)
		return;

	/* Convert the count to a timespec. */
	tcount = pps->capcount - pps->capth->th_offset_count;
	tcount &= pps->capth->th_counter->tc_counter_mask;
	bt = pps->capth->th_offset;
	bintime_addx(&bt, pps->capth->th_scale * tcount);
	bintime_add(&bt, &boottimebin);
	bintime2timespec(&bt, &ts);

	/* If the timecounter was wound up underneath us, bail out. */
	if (pps->capgen != pps->capth->th_generation)
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
		u_int64_t scale;

		/*
		 * Feed the NTP PLL/FLL.
		 * The FLL wants to know how many (hardware) nanoseconds
		 * elapsed since the previous event.
		 */
		tcount = pps->capcount - pps->ppscount[2];
		pps->ppscount[2] = pps->capcount;
		tcount &= pps->capth->th_counter->tc_counter_mask;
		scale = (u_int64_t)1 << 63;
		scale /= pps->capth->th_counter->tc_frequency;
		scale *= 2;
		bt.sec = 0;
		bt.frac = 0;
		bintime_addx(&bt, scale * tcount);
		bintime2timespec(&bt, &ts);
		hardpps(tsp, ts.tv_nsec + 1000000000 * ts.tv_sec);
	}
#endif
}

/*
 * Timecounters need to be updated every so often to prevent the hardware
 * counter from overflowing.  Updating also recalculates the cached values
 * used by the get*() family of functions, so their precision depends on
 * the update frequency.
 */

static int tc_tick;
SYSCTL_INT(_kern_timecounter, OID_AUTO, tick, CTLFLAG_RD, &tc_tick, 0, "");

void
tc_ticktock(void)
{
	static int count;

	if (++count < tc_tick)
		return;
	count = 0;
	tc_windup();
}

static void
inittimecounter(void *dummy)
{
	u_int p;

	/*
	 * Set the initial timeout to
	 * max(1, <approx. number of hardclock ticks in a millisecond>).
	 * People should probably not use the sysctl to set the timeout
	 * to smaller than its inital value, since that value is the
	 * smallest reasonable one.  If they want better timestamps they
	 * should use the non-"get"* functions.
	 */
	if (hz > 1000)
		tc_tick = (hz + 500) / 1000;
	else
		tc_tick = 1;
	p = (tc_tick * 1000000) / hz;
	printf("Timecounters tick every %d.%03u msec\n", p / 1000, p % 1000);

	/* warm up new timecounter (again) and get rolling. */
	(void)timecounter->tc_get_timecount(timecounter);
	(void)timecounter->tc_get_timecount(timecounter);
}

SYSINIT(timecounter, SI_SUB_CLOCKS, SI_ORDER_SECOND, inittimecounter, NULL)
