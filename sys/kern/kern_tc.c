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
#include <sys/syslog.h>
#include <sys/systm.h>
#include <sys/timepps.h>
#include <sys/timetc.h>
#include <sys/timex.h>

/*
 * A large step happens on boot.  This constant detects such steps.
 * It is relatively small so that ntp_update_second gets called enough
 * in the typical 'missed a couple of seconds' case, but doesn't loop
 * forever when the time step is large.
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
	dummy_get_timecount, 0, ~0u, 1000000, "dummy", -1000000
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

static struct timehands th0;
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
time_t time_uptime = 1;

static struct bintime boottimebin;
struct timeval boottime;
static int sysctl_kern_boottime(SYSCTL_HANDLER_ARGS);
SYSCTL_PROC(_kern, KERN_BOOTTIME, boottime, CTLTYPE_STRUCT|CTLFLAG_RD,
    NULL, 0, sysctl_kern_boottime, "S,timeval", "System boottime");

SYSCTL_NODE(_kern, OID_AUTO, timecounter, CTLFLAG_RW, 0, "");
SYSCTL_NODE(_kern_timecounter, OID_AUTO, tc, CTLFLAG_RW, 0, "");

static int timestepwarnings;
SYSCTL_INT(_kern_timecounter, OID_AUTO, stepwarnings, CTLFLAG_RW,
    &timestepwarnings, 0, "");

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
static void cpu_tick_calibrate(int);

static int
sysctl_kern_boottime(SYSCTL_HANDLER_ARGS)
{
#ifdef SCTL_MASK32
	int tv[2];

	if (req->flags & SCTL_MASK32) {
		tv[0] = boottime.tv_sec;
		tv[1] = boottime.tv_usec;
		return SYSCTL_OUT(req, tv, sizeof(tv));
	} else
#endif
		return SYSCTL_OUT(req, &boottime, sizeof(boottime));
}

static int
sysctl_kern_timecounter_get(SYSCTL_HANDLER_ARGS)
{
	u_int ncount;
	struct timecounter *tc = arg1;

	ncount = tc->tc_get_timecount(tc);
	return sysctl_handle_int(oidp, &ncount, 0, req);
}

static int
sysctl_kern_timecounter_freq(SYSCTL_HANDLER_ARGS)
{
	u_int64_t freq;
	struct timecounter *tc = arg1;

	freq = tc->tc_frequency;
	return sysctl_handle_quad(oidp, &freq, 0, req);
}

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
 * Initialize a new timecounter and possibly use it.
 */
void
tc_init(struct timecounter *tc)
{
	u_int u;
	struct sysctl_oid *tc_root;

	u = tc->tc_frequency / tc->tc_counter_mask;
	/* XXX: We need some margin here, 10% is a guess */
	u *= 11;
	u /= 10;
	if (u > hz && tc->tc_quality >= 0) {
		tc->tc_quality = -2000;
		if (bootverbose) {
			printf("Timecounter \"%s\" frequency %ju Hz",
			    tc->tc_name, (uintmax_t)tc->tc_frequency);
			printf(" -- Insufficient hz, needs at least %u\n", u);
		}
	} else if (tc->tc_quality >= 0 || bootverbose) {
		printf("Timecounter \"%s\" frequency %ju Hz quality %d\n",
		    tc->tc_name, (uintmax_t)tc->tc_frequency,
		    tc->tc_quality);
	}

	tc->tc_next = timecounters;
	timecounters = tc;
	/*
	 * Set up sysctl tree for this counter.
	 */
	tc_root = SYSCTL_ADD_NODE(NULL,
	    SYSCTL_STATIC_CHILDREN(_kern_timecounter_tc), OID_AUTO, tc->tc_name,
	    CTLFLAG_RW, 0, "timecounter description");
	SYSCTL_ADD_UINT(NULL, SYSCTL_CHILDREN(tc_root), OID_AUTO,
	    "mask", CTLFLAG_RD, &(tc->tc_counter_mask), 0,
	    "mask for implemented bits");
	SYSCTL_ADD_PROC(NULL, SYSCTL_CHILDREN(tc_root), OID_AUTO,
	    "counter", CTLTYPE_UINT | CTLFLAG_RD, tc, sizeof(*tc),
	    sysctl_kern_timecounter_get, "IU", "current timecounter value");
	SYSCTL_ADD_PROC(NULL, SYSCTL_CHILDREN(tc_root), OID_AUTO,
	    "frequency", CTLTYPE_QUAD | CTLFLAG_RD, tc, sizeof(*tc),
	     sysctl_kern_timecounter_freq, "QU", "timecounter frequency");
	SYSCTL_ADD_INT(NULL, SYSCTL_CHILDREN(tc_root), OID_AUTO,
	    "quality", CTLFLAG_RD, &(tc->tc_quality), 0,
	    "goodness of time counter");
	/*
	 * Never automatically use a timecounter with negative quality.
	 * Even though we run on the dummy counter, switching here may be
	 * worse since this timecounter may not be monotonous.
	 */
	if (tc->tc_quality < 0)
		return;
	if (tc->tc_quality < timecounter->tc_quality)
		return;
	if (tc->tc_quality == timecounter->tc_quality &&
	    tc->tc_frequency < timecounter->tc_frequency)
		return;
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
 * when we booted.
 * XXX: not locked.
 */
void
tc_setclock(struct timespec *ts)
{
	struct timespec tbef, taft;
	struct bintime bt, bt2;

	cpu_tick_calibrate(1);
	nsetclock++;
	nanotime(&tbef);
	timespec2bintime(ts, &bt);
	binuptime(&bt2);
	bintime_sub(&bt, &bt2);
	bintime_add(&bt2, &boottimebin);
	boottimebin = bt;
	bintime2timeval(&bt, &boottime);

	/* XXX fiddle all the little crinkly bits around the fiords... */
	tc_windup();
	nanotime(&taft);
	if (timestepwarnings) {
		log(LOG_INFO,
		    "Time stepped from %jd.%09ld to %jd.%09ld (%jd.%09ld)\n",
		    (intmax_t)tbef.tv_sec, tbef.tv_nsec,
		    (intmax_t)taft.tv_sec, taft.tv_nsec,
		    (intmax_t)ts->tv_sec, ts->tv_nsec);
	}
	cpu_tick_calibrate(1);
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
	 * Deal with NTP second processing.  The for loop normally
	 * iterates at most once, but in extreme situations it might
	 * keep NTP sane if timeouts are not run for several seconds.
	 * At boot, the time step can be large when the TOD hardware
	 * has been read, so on really large steps, we call
	 * ntp_update_second only twice.  We need to call it twice in
	 * case we missed a leap second.
	 */
	bt = th->th_offset;
	bintime_add(&bt, &boottimebin);
	i = bt.sec - tho->th_microtime.tv_sec;
	if (i > LARGE_STEP)
		i = 2;
	for (; i > 0; i--) {
		t = bt.sec;
		ntp_update_second(&th->th_adjustment, &bt.sec);
		if (bt.sec != t)
			boottimebin.sec += bt.sec - t;
	}
	/* Update the UTC timestamps used by the get*() functions. */
	/* XXX shouldn't do this here.  Should force non-`get' versions. */
	bintime2timeval(&bt, &th->th_microtime);
	bintime2timespec(&bt, &th->th_nanotime);

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
	 * we can only multiply by about 850 without overflowing, that
	 * leaves no suitably precise fractions for multiply before divide.
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


/* Report or change the active timecounter hardware. */
static int
sysctl_kern_timecounter_choice(SYSCTL_HANDLER_ARGS)
{
	char buf[32], *spc;
	struct timecounter *tc;
	int error;

	spc = "";
	error = 0;
	for (tc = timecounters; error == 0 && tc != NULL; tc = tc->tc_next) {
		sprintf(buf, "%s%s(%d)",
		    spc, tc->tc_name, tc->tc_quality);
		error = SYSCTL_OUT(req, buf, strlen(buf));
		spc = " ";
	}
	return (error);
}

SYSCTL_PROC(_kern_timecounter, OID_AUTO, choice, CTLTYPE_STRING | CTLFLAG_RD,
    0, 0, sysctl_kern_timecounter_choice, "A", "");

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

	KASSERT(pps != NULL, ("NULL pps pointer in pps_ioctl"));
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
		return (ENOIOCTL);
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

	KASSERT(pps != NULL, ("NULL pps pointer in pps_capture"));
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

	KASSERT(pps != NULL, ("NULL pps pointer in pps_event"));
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
	static time_t last_calib;

	if (++count < tc_tick)
		return;
	count = 0;
	tc_windup();
	if (time_uptime != last_calib && !(time_uptime & 0xf)) {
		cpu_tick_calibrate(0);
		last_calib = time_uptime;
	}
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

/* Cpu tick handling -------------------------------------------------*/

static int cpu_tick_variable;
static uint64_t	cpu_tick_frequency;

static uint64_t
tc_cpu_ticks(void)
{
	static uint64_t base;
	static unsigned last;
	unsigned u;
	struct timecounter *tc;

	tc = timehands->th_counter;
	u = tc->tc_get_timecount(tc) & tc->tc_counter_mask;
	if (u < last)
		base += (uint64_t)tc->tc_counter_mask + 1;
	last = u;
	return (u + base);
}

/*
 * This function gets called ever 16 seconds on only one designated
 * CPU in the system from hardclock() via tc_ticktock().
 *
 * Whenever the real time clock is stepped we get called with reset=1
 * to make sure we handle suspend/resume and similar events correctly.
 */

static void
cpu_tick_calibrate(int reset)
{
	static uint64_t c_last;
	uint64_t c_this, c_delta;
	static struct bintime  t_last;
	struct bintime t_this, t_delta;
	uint32_t divi;

	if (reset) {
		/* The clock was stepped, abort & reset */
		t_last.sec = 0;
		return;
	}

	/* we don't calibrate fixed rate cputicks */
	if (!cpu_tick_variable)
		return;

	getbinuptime(&t_this);
	c_this = cpu_ticks();
	if (t_last.sec != 0) {
		c_delta = c_this - c_last;
		t_delta = t_this;
		bintime_sub(&t_delta, &t_last);
		/*
		 * Validate that 16 +/- 1/256 seconds passed. 
		 * After division by 16 this gives us a precision of
		 * roughly 250PPM which is sufficient
		 */
		if (t_delta.sec > 16 || (
		    t_delta.sec == 16 && t_delta.frac >= (0x01LL << 56))) {
			/* too long */
			if (bootverbose)
				printf("%ju.%016jx too long\n",
				    (uintmax_t)t_delta.sec,
				    (uintmax_t)t_delta.frac);
		} else if (t_delta.sec < 15 ||
		    (t_delta.sec == 15 && t_delta.frac <= (0xffLL << 56))) {
			/* too short */
			if (bootverbose)
				printf("%ju.%016jx too short\n",
				    (uintmax_t)t_delta.sec,
				    (uintmax_t)t_delta.frac);
		} else {
			/* just right */
			/*
			 * Headroom:
			 * 	2^(64-20) / 16[s] =
			 * 	2^(44) / 16[s] =
			 * 	17.592.186.044.416 / 16 =
			 * 	1.099.511.627.776 [Hz]
			 */
			divi = t_delta.sec << 20;
			divi |= t_delta.frac >> (64 - 20);
			c_delta <<= 20;
			c_delta /= divi;
			if (c_delta  > cpu_tick_frequency) {
				if (0 && bootverbose)
					printf("cpu_tick increased to %ju Hz\n",
					    c_delta);
				cpu_tick_frequency = c_delta;
			}
		}
	}
	c_last = c_this;
	t_last = t_this;
}

void
set_cputicker(cpu_tick_f *func, uint64_t freq, unsigned var)
{

	if (func == NULL) {
		cpu_ticks = tc_cpu_ticks;
	} else {
		cpu_tick_frequency = freq;
		cpu_tick_variable = var;
		cpu_ticks = func;
	}
}

uint64_t
cpu_tickrate(void)
{

	if (cpu_ticks == tc_cpu_ticks) 
		return (tc_getfrequency());
	return (cpu_tick_frequency);
}

/*
 * We need to be slightly careful converting cputicks to microseconds.
 * There is plenty of margin in 64 bits of microseconds (half a million
 * years) and in 64 bits at 4 GHz (146 years), but if we do a multiply
 * before divide conversion (to retain precision) we find that the
 * margin shrinks to 1.5 hours (one millionth of 146y).
 * With a three prong approach we never lose significant bits, no
 * matter what the cputick rate and length of timeinterval is.
 */

uint64_t
cputick2usec(uint64_t tick)
{

	if (tick > 18446744073709551LL)		/* floor(2^64 / 1000) */
		return (tick / (cpu_tickrate() / 1000000LL));
	else if (tick > 18446744073709LL)	/* floor(2^64 / 1000000) */
		return ((tick * 1000LL) / (cpu_tickrate() / 1000LL));
	else
		return ((tick * 1000000LL) / cpu_tickrate());
}

cpu_tick_f	*cpu_ticks = tc_cpu_ticks;
