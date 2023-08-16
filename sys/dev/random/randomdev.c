/*-
 * Copyright (c) 2017 Oliver Pinter
 * Copyright (c) 2000-2015 Mark R V Murray
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/filio.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/poll.h>
#include <sys/proc.h>
#include <sys/random.h>
#include <sys/sbuf.h>
#include <sys/selinfo.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/unistd.h>

#include <crypto/rijndael/rijndael-api-fst.h>
#include <crypto/sha2/sha256.h>

#include <dev/random/hash.h>
#include <dev/random/randomdev.h>
#include <dev/random/random_harvestq.h>

#define	RANDOM_UNIT	0

/*
 * In loadable random, the core randomdev.c / random(9) routines have static
 * visibility and an alternative name to avoid conflicting with the function
 * pointers of the real names in the core kernel.  random_alg_context_init
 * installs pointers to the loadable static names into the core kernel's
 * function pointers at SI_SUB_RANDOM:SI_ORDER_SECOND.
 */
#if defined(RANDOM_LOADABLE)
static int (read_random_uio)(struct uio *, bool);
static void (read_random)(void *, u_int);
static bool (is_random_seeded)(void);
#endif

static d_read_t randomdev_read;
static d_write_t randomdev_write;
static d_poll_t randomdev_poll;
static d_ioctl_t randomdev_ioctl;

static struct cdevsw random_cdevsw = {
	.d_name = "random",
	.d_version = D_VERSION,
	.d_read = randomdev_read,
	.d_write = randomdev_write,
	.d_poll = randomdev_poll,
	.d_ioctl = randomdev_ioctl,
};

/* For use with make_dev(9)/destroy_dev(9). */
static struct cdev *random_dev;

#if defined(RANDOM_LOADABLE)
static void
random_alg_context_init(void *dummy __unused)
{
	_read_random_uio = (read_random_uio);
	_read_random = (read_random);
	_is_random_seeded = (is_random_seeded);
}
SYSINIT(random_device, SI_SUB_RANDOM, SI_ORDER_SECOND, random_alg_context_init,
    NULL);
#endif

static struct selinfo rsel;

/*
 * This is the read uio(9) interface for random(4).
 */
/* ARGSUSED */
static int
randomdev_read(struct cdev *dev __unused, struct uio *uio, int flags)
{

	return ((read_random_uio)(uio, (flags & O_NONBLOCK) != 0));
}

/*
 * If the random device is not seeded, blocks until it is seeded.
 *
 * Returns zero when the random device is seeded.
 *
 * If the 'interruptible' parameter is true, and the device is unseeded, this
 * routine may be interrupted.  If interrupted, it will return either ERESTART
 * or EINTR.
 */
#define SEEDWAIT_INTERRUPTIBLE		true
#define SEEDWAIT_UNINTERRUPTIBLE	false
static int
randomdev_wait_until_seeded(bool interruptible)
{
	int error, spamcount, slpflags;

	slpflags = interruptible ? PCATCH : 0;

	error = 0;
	spamcount = 0;
	while (!p_random_alg_context->ra_seeded()) {
		/* keep tapping away at the pre-read until we seed/unblock. */
		p_random_alg_context->ra_pre_read();
		/* Only bother the console every 10 seconds or so */
		if (spamcount == 0)
			printf("random: %s unblock wait\n", __func__);
		spamcount = (spamcount + 1) % 100;
		error = tsleep(p_random_alg_context, slpflags, "randseed",
		    hz / 10);
		if (error == ERESTART || error == EINTR) {
			KASSERT(interruptible,
			    ("unexpected wake of non-interruptible sleep"));
			break;
		}
		/* Squash tsleep timeout condition */
		if (error == EWOULDBLOCK)
			error = 0;
		KASSERT(error == 0, ("unexpected tsleep error %d", error));
	}
	return (error);
}

int
(read_random_uio)(struct uio *uio, bool nonblock)
{
	/* 16 MiB takes about 0.08 s CPU time on my 2017 AMD Zen CPU */
#define SIGCHK_PERIOD (16 * 1024 * 1024)
	const size_t sigchk_period = SIGCHK_PERIOD;
	CTASSERT(SIGCHK_PERIOD % PAGE_SIZE == 0);
#undef SIGCHK_PERIOD

	uint8_t *random_buf;
	size_t total_read, read_len;
	ssize_t bufsize;
	int error;


	KASSERT(uio->uio_rw == UIO_READ, ("%s: bogus write", __func__));
	KASSERT(uio->uio_resid >= 0, ("%s: bogus negative resid", __func__));

	p_random_alg_context->ra_pre_read();
	error = 0;
	/* (Un)Blocking logic */
	if (!p_random_alg_context->ra_seeded()) {
		if (nonblock)
			error = EWOULDBLOCK;
		else
			error = randomdev_wait_until_seeded(
			    SEEDWAIT_INTERRUPTIBLE);
	}
	if (error != 0)
		return (error);

	total_read = 0;

	/* Easy to deal with the trivial 0 byte case. */
	if (__predict_false(uio->uio_resid == 0))
		return (0);

	/*
	 * If memory is plentiful, use maximally sized requests to avoid
	 * per-call algorithm overhead.  But fall back to a single page
	 * allocation if the full request isn't immediately available.
	 */
	bufsize = MIN(sigchk_period, (size_t)uio->uio_resid);
	random_buf = malloc(bufsize, M_ENTROPY, M_NOWAIT);
	if (random_buf == NULL) {
		bufsize = PAGE_SIZE;
		random_buf = malloc(bufsize, M_ENTROPY, M_WAITOK);
	}

	error = 0;
	while (uio->uio_resid > 0 && error == 0) {
		read_len = MIN((size_t)uio->uio_resid, bufsize);

		p_random_alg_context->ra_read(random_buf, read_len);

		/*
		 * uiomove() may yield the CPU before each 'read_len' bytes (up
		 * to bufsize) are copied out.
		 */
		error = uiomove(random_buf, read_len, uio);
		total_read += read_len;

		/*
		 * Poll for signals every few MBs to avoid very long
		 * uninterruptible syscalls.
		 */
		if (error == 0 && uio->uio_resid != 0 &&
		    total_read % sigchk_period == 0) {
			error = tsleep_sbt(p_random_alg_context, PCATCH,
			    "randrd", SBT_1NS, 0, C_HARDCLOCK);
			/* Squash tsleep timeout condition */
			if (error == EWOULDBLOCK)
				error = 0;
		}
	}

	/*
	 * Short reads due to signal interrupt should not indicate error.
	 * Instead, the uio will reflect that the read was shorter than
	 * requested.
	 */
	if (error == ERESTART || error == EINTR)
		error = 0;

	zfree(random_buf, M_ENTROPY);
	return (error);
}

/*-
 * Kernel API version of read_random().  This is similar to read_random_uio(),
 * except it doesn't interface with uio(9).  It cannot assumed that random_buf
 * is a multiple of RANDOM_BLOCKSIZE bytes.
 *
 * If the tunable 'kern.random.initial_seeding.bypass_before_seeding' is set
 * non-zero, silently fail to emit random data (matching the pre-r346250
 * behavior).  If read_random is called prior to seeding and bypassed because
 * of this tunable, the condition is reported in the read-only sysctl
 * 'kern.random.initial_seeding.read_random_bypassed_before_seeding'.
 */
void
(read_random)(void *random_buf, u_int len)
{

	KASSERT(random_buf != NULL, ("No suitable random buffer in %s", __func__));
	p_random_alg_context->ra_pre_read();

	if (len == 0)
		return;

	/* (Un)Blocking logic */
	if (__predict_false(!p_random_alg_context->ra_seeded())) {
		if (random_bypass_before_seeding) {
			if (!read_random_bypassed_before_seeding) {
				if (!random_bypass_disable_warnings)
					printf("read_random: WARNING: bypassing"
					    " request for random data because "
					    "the random device is not yet "
					    "seeded and the knob "
					    "'bypass_before_seeding' was "
					    "enabled.\n");
				read_random_bypassed_before_seeding = true;
			}
			/* Avoid potentially leaking stack garbage */
			memset(random_buf, 0, len);
			return;
		}

		(void)randomdev_wait_until_seeded(SEEDWAIT_UNINTERRUPTIBLE);
	}
	p_random_alg_context->ra_read(random_buf, len);
}

bool
(is_random_seeded)(void)
{
	return (p_random_alg_context->ra_seeded());
}

static __inline void
randomdev_accumulate(uint8_t *buf, u_int count)
{
	static u_int destination = 0;
	static struct harvest_event event;
	static struct randomdev_hash hash;
	static uint32_t entropy_data[RANDOM_KEYSIZE_WORDS];
	uint32_t timestamp;
	int i;

	/* Extra timing here is helpful to scrape scheduler jitter entropy */
	randomdev_hash_init(&hash);
	timestamp = (uint32_t)get_cyclecount();
	randomdev_hash_iterate(&hash, &timestamp, sizeof(timestamp));
	randomdev_hash_iterate(&hash, buf, count);
	timestamp = (uint32_t)get_cyclecount();
	randomdev_hash_iterate(&hash, &timestamp, sizeof(timestamp));
	randomdev_hash_finish(&hash, entropy_data);
	for (i = 0; i < RANDOM_KEYSIZE_WORDS; i += sizeof(event.he_entropy)/sizeof(event.he_entropy[0])) {
		event.he_somecounter = (uint32_t)get_cyclecount();
		event.he_size = sizeof(event.he_entropy);
		event.he_source = RANDOM_CACHED;
		event.he_destination = destination++; /* Harmless cheating */
		memcpy(event.he_entropy, entropy_data + i, sizeof(event.he_entropy));
		p_random_alg_context->ra_event_processor(&event);
	}
	explicit_bzero(&event, sizeof(event));
	explicit_bzero(entropy_data, sizeof(entropy_data));
}

/* ARGSUSED */
static int
randomdev_write(struct cdev *dev __unused, struct uio *uio, int flags __unused)
{
	uint8_t *random_buf;
	int c, error = 0;
	ssize_t nbytes;

	random_buf = malloc(PAGE_SIZE, M_ENTROPY, M_WAITOK);
	nbytes = uio->uio_resid;
	while (uio->uio_resid > 0 && error == 0) {
		c = MIN(uio->uio_resid, PAGE_SIZE);
		error = uiomove(random_buf, c, uio);
		if (error)
			break;
		randomdev_accumulate(random_buf, c);
	}
	if (nbytes != uio->uio_resid && (error == ERESTART || error == EINTR))
		/* Partial write, not error. */
		error = 0;
	free(random_buf, M_ENTROPY);
	return (error);
}

/* ARGSUSED */
static int
randomdev_poll(struct cdev *dev __unused, int events, struct thread *td __unused)
{

	if (events & (POLLIN | POLLRDNORM)) {
		if (p_random_alg_context->ra_seeded())
			events &= (POLLIN | POLLRDNORM);
		else
			selrecord(td, &rsel);
	}
	return (events);
}

/* This will be called by the entropy processor when it seeds itself and becomes secure */
void
randomdev_unblock(void)
{

	selwakeuppri(&rsel, PUSER);
	wakeup(p_random_alg_context);
	printf("random: unblocking device.\n");
#ifndef RANDOM_FENESTRASX
	/* Do random(9) a favour while we are about it. */
	(void)atomic_cmpset_int(&arc4rand_iniseed_state, ARC4_ENTR_NONE, ARC4_ENTR_HAVE);
#endif
}

/* ARGSUSED */
static int
randomdev_ioctl(struct cdev *dev __unused, u_long cmd, caddr_t addr __unused,
    int flags __unused, struct thread *td __unused)
{
	int error = 0;

	switch (cmd) {
		/* Really handled in upper layer */
	case FIOASYNC:
	case FIONBIO:
		break;
	default:
		error = ENOTTY;
	}

	return (error);
}

/* ARGSUSED */
static int
randomdev_modevent(module_t mod __unused, int type, void *data __unused)
{
	int error = 0;

	switch (type) {
	case MOD_LOAD:
		printf("random: entropy device external interface\n");
		random_dev = make_dev_credf(MAKEDEV_ETERNAL_KLD, &random_cdevsw,
		    RANDOM_UNIT, NULL, UID_ROOT, GID_WHEEL, 0644, "random");
		make_dev_alias(random_dev, "urandom"); /* compatibility */
		break;
	case MOD_UNLOAD:
		error = EBUSY;
		break;
	case MOD_SHUTDOWN:
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}
	return (error);
}

static moduledata_t randomdev_mod = {
	"random_device",
	randomdev_modevent,
	0
};

DECLARE_MODULE(random_device, randomdev_mod, SI_SUB_DRIVERS, SI_ORDER_FIRST);
MODULE_VERSION(random_device, 1);
MODULE_DEPEND(random_device, crypto, 1, 1, 1);
MODULE_DEPEND(random_device, random_harvestq, 1, 1, 1);
