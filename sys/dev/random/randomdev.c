/*-
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
__FBSDID("$FreeBSD$");

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
#include <crypto/sha2/sha2.h>

#include <dev/random/hash.h>
#include <dev/random/randomdev.h>
#include <dev/random/random_harvestq.h>

#include "opt_random.h"

#if defined(RANDOM_DUMMY) && defined(RANDOM_YARROW)
#error "Cannot define both RANDOM_DUMMY and RANDOM_YARROW"
#endif

#define	RANDOM_UNIT	0

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

/* Set up the sysctl root node for the entropy device */
SYSCTL_NODE(_kern, OID_AUTO, random, CTLFLAG_RW, 0, "Cryptographically Secure Random Number Generator");

MALLOC_DEFINE(M_ENTROPY, "entropy", "Entropy harvesting buffers and data structures");

#if defined(RANDOM_DUMMY)

/*-
 * Dummy "always block" pseudo algorithm, used when there is no real
 * random(4) driver to provide a CSPRNG.
 */

static u_int
dummy_random_zero(void)
{

	return (0);
}

static void
dummy_random(void)
{
}

struct random_algorithm random_alg_context = {
	.ra_ident = "Dummy",
	.ra_init_alg = NULL,
	.ra_deinit_alg = NULL,
	.ra_pre_read = dummy_random,
	.ra_read = (random_alg_read_t *)dummy_random_zero,
	.ra_write = (random_alg_write_t *)dummy_random_zero,
	.ra_reseed = dummy_random,
	.ra_seeded = (random_alg_seeded_t *)dummy_random_zero,
	.ra_event_processor = NULL,
	.ra_poolcount = 0,
};

#else /* !defined(RANDOM_DUMMY) */

LIST_HEAD(sources_head, random_sources);
static struct sources_head source_list = LIST_HEAD_INITIALIZER(source_list);
static u_int read_rate;

static void
random_alg_context_ra_init_alg(void *data)
{

	random_alg_context.ra_init_alg(data);
}

static void
random_alg_context_ra_deinit_alg(void *data)
{

	random_alg_context.ra_deinit_alg(data);
}

SYSINIT(random_device, SI_SUB_RANDOM, SI_ORDER_THIRD, random_alg_context_ra_init_alg, NULL);
SYSUNINIT(random_device, SI_SUB_RANDOM, SI_ORDER_THIRD, random_alg_context_ra_deinit_alg, NULL);

#endif /* defined(RANDOM_DUMMY) */

static struct selinfo rsel;

/*
 * This is the read uio(9) interface for random(4).
 */
/* ARGSUSED */
static int
randomdev_read(struct cdev *dev __unused, struct uio *uio, int flags)
{

	return (read_random_uio(uio, (flags & O_NONBLOCK) != 0));
}

int
read_random_uio(struct uio *uio, bool nonblock)
{
	uint8_t *random_buf;
	int error;
	ssize_t read_len, total_read, c;

	random_buf = malloc(PAGE_SIZE, M_ENTROPY, M_WAITOK);
	random_alg_context.ra_pre_read();
	/* (Un)Blocking logic */
	error = 0;
	while (!random_alg_context.ra_seeded()) {
		if (flags & O_NONBLOCK)	{
			error = EWOULDBLOCK;
			break;
		}
		tsleep(&random_alg_context, 0, "randseed", hz/10);
		/* keep tapping away at the pre-read until we seed/unblock. */
		random_alg_context.ra_pre_read();
		printf("random: %s unblock wait\n", __func__);
	}
	if (error == 0) {
#if !defined(RANDOM_DUMMY)
		/* XXX: FIX!! Next line as an atomic operation? */
		read_rate += (uio->uio_resid + sizeof(uint32_t))/sizeof(uint32_t);
#endif
		total_read = 0;
		while (uio->uio_resid && !error) {
			read_len = uio->uio_resid;
			/*
			 * Belt-and-braces.
			 * Round up the read length to a crypto block size multiple,
			 * which is what the underlying generator is expecting.
			 * See the random_buf size requirements in the Yarrow/Fortuna code.
			 */
			read_len += RANDOM_BLOCKSIZE;
			read_len -= read_len % RANDOM_BLOCKSIZE;
			read_len = MIN(read_len, PAGE_SIZE);
			random_alg_context.ra_read(random_buf, read_len);
			c = MIN(uio->uio_resid, read_len);
			error = uiomove(random_buf, c, uio);
			total_read += c;
		}
		if (total_read != uio->uio_resid && (error == ERESTART || error == EINTR) )
			/* Return partial read, not error. */
			error = 0;
	}
	free(random_buf, M_ENTROPY);
	return (error);
}

/*-
 * Kernel API version of read_random().
 * This is similar to random_alg_read(),
 * except it doesn't interface with uio(9).
 * It cannot assumed that random_buf is a multiple of
 * RANDOM_BLOCKSIZE bytes.
 */
u_int
read_random(void *random_buf, u_int len)
{
	u_int read_len, total_read, c;
	uint8_t local_buf[len + RANDOM_BLOCKSIZE];

	KASSERT(random_buf != NULL, ("No suitable random buffer in %s", __func__));
	random_alg_context.ra_pre_read();
	/* (Un)Blocking logic; if not seeded, return nothing. */
	if (random_alg_context.ra_seeded()) {
#if !defined(RANDOM_DUMMY)
		/* XXX: FIX!! Next line as an atomic operation? */
		read_rate += (len + sizeof(uint32_t))/sizeof(uint32_t);
#endif
		read_len = len;
		/*
		 * Belt-and-braces.
		 * Round up the read length to a crypto block size multiple,
		 * which is what the underlying generator is expecting.
		 */
		read_len += RANDOM_BLOCKSIZE;
		read_len -= read_len % RANDOM_BLOCKSIZE;
		total_read = 0;
		while (read_len) {
			c = MIN(read_len, PAGE_SIZE);
			random_alg_context.ra_read(&local_buf[total_read], c);
			read_len -= c;
			total_read += c;
		}
		memcpy(random_buf, local_buf, len);
	} else
		len = 0;
	return (len);
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
		random_alg_context.ra_write(random_buf, c);
		tsleep(&random_alg_context, 0, "randwr", hz/10);
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
		if (random_alg_context.ra_seeded())
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
	wakeup(&random_alg_context);
	printf("random: unblocking device.\n");
	/* Do random(9) a favour while we are about it. */
	(void)atomic_cmpset_int(&arc4rand_iniseed_state, ARC4_ENTR_NONE, ARC4_ENTR_HAVE);
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

void
random_source_register(struct random_source *rsource)
{
#if defined(RANDOM_DUMMY)
	(void)rsource;
#else /* !defined(RANDOM_DUMMY) */
	struct random_sources *rrs;

	KASSERT(rsource != NULL, ("invalid input to %s", __func__));

	rrs = malloc(sizeof(*rrs), M_ENTROPY, M_WAITOK);
	rrs->rrs_source = rsource;

	printf("random: registering fast source %s\n", rsource->rs_ident);
	LIST_INSERT_HEAD(&source_list, rrs, rrs_entries);
#endif /* defined(RANDOM_DUMMY) */
}

void
random_source_deregister(struct random_source *rsource)
{
#if defined(RANDOM_DUMMY)
	(void)rsource;
#else /* !defined(RANDOM_DUMMY) */
	struct random_sources *rrs = NULL;

	KASSERT(rsource != NULL, ("invalid input to %s", __func__));
	LIST_FOREACH(rrs, &source_list, rrs_entries)
		if (rrs->rrs_source == rsource) {
			LIST_REMOVE(rrs, rrs_entries);
			break;
		}
	if (rrs != NULL)
		free(rrs, M_ENTROPY);
#endif /* defined(RANDOM_DUMMY) */
}

#if !defined(RANDOM_DUMMY)
/*
 * Run through all fast sources reading entropy for the given
 * number of rounds, which should be a multiple of the number
 * of entropy accumulation pools in use; 2 for Yarrow and 32
 * for Fortuna.
 *
 * BEWARE!!!
 * This function runs inside the RNG thread! Don't do anything silly!
 */
void
random_sources_feed(void)
{
	uint32_t entropy[HARVESTSIZE];
	struct random_sources *rrs;
	u_int i, n, local_read_rate;

	/*
	 * Step over all of live entropy sources, and feed their output
	 * to the system-wide RNG.
	 */
	/* XXX: FIX!! Next lines as an atomic operation? */
	local_read_rate = read_rate;
	read_rate = RANDOM_ALG_READ_RATE_MINIMUM;
	LIST_FOREACH(rrs, &source_list, rrs_entries) {
		for (i = 0; i < random_alg_context.ra_poolcount*local_read_rate; i++) {
			n = rrs->rrs_source->rs_read(entropy, sizeof(entropy));
			KASSERT((n > 0 && n <= sizeof(entropy)), ("very bad return from rs_read (= %d) in %s", n, __func__));
			random_harvest_direct(entropy, n, (n*8)/2, rrs->rrs_source->rs_source);
		}
	}
	explicit_bzero(entropy, sizeof(entropy));
}

static int
random_source_handler(SYSCTL_HANDLER_ARGS)
{
	struct random_sources *rrs;
	struct sbuf sbuf;
	int error, count;

	sbuf_new_for_sysctl(&sbuf, NULL, 64, req);
	count = 0;
	LIST_FOREACH(rrs, &source_list, rrs_entries) {
		sbuf_cat(&sbuf, (count++ ? ",'" : "'"));
		sbuf_cat(&sbuf, rrs->rrs_source->rs_ident);
		sbuf_cat(&sbuf, "'");
	}
	error = sbuf_finish(&sbuf);
	sbuf_delete(&sbuf);
	return (error);
}
SYSCTL_PROC(_kern_random, OID_AUTO, random_sources, CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE,
	    NULL, 0, random_source_handler, "A",
	    "List of active fast entropy sources.");
#endif /* !defined(RANDOM_DUMMY) */

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
		destroy_dev(random_dev);
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
