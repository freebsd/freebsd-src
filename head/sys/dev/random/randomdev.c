/*-
 * Copyright (c) 2000-2013 Mark R V Murray
 * Copyright (c) 2013 Arthur Mesh <arthurmesh@gmail.com>
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
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/poll.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/random.h>
#include <sys/selinfo.h>
#include <sys/uio.h>
#include <sys/unistd.h>

#include <machine/bus.h>
#include <machine/cpu.h>

#include <dev/random/randomdev.h>
#include <dev/random/randomdev_soft.h>
#include <dev/random/random_adaptors.h>
#include <dev/random/random_harvestq.h>
#include <dev/random/live_entropy_sources.h>

#define RANDOM_MINOR	0

static d_read_t random_read;
static d_write_t random_write;
static d_ioctl_t random_ioctl;
static d_poll_t random_poll;

static struct cdevsw random_cdevsw = {
	.d_version = D_VERSION,
	.d_read = random_read,
	.d_write = random_write,
	.d_ioctl = random_ioctl,
	.d_poll = random_poll,
	.d_name = "random",
};

/* For use with make_dev(9)/destroy_dev(9). */
static struct cdev *random_dev;

/* ARGSUSED */
static int
random_read(struct cdev *dev __unused, struct uio *uio, int flag)
{
	int c, error = 0;
	void *random_buf;

	/* Blocking logic */
	if (!random_adaptor->seeded)
		error = (*random_adaptor->block)(flag);

	/* The actual read */
	if (!error) {

		random_buf = (void *)malloc(PAGE_SIZE, M_ENTROPY, M_WAITOK);

		while (uio->uio_resid > 0 && !error) {
			c = MIN(uio->uio_resid, PAGE_SIZE);
			c = (*random_adaptor->read)(random_buf, c);
			error = uiomove(random_buf, c, uio);
		}
		/* Finished reading; let the source know so it can do some
		 * optional housekeeping */
		(*random_adaptor->read)(NULL, 0);

		free(random_buf, M_ENTROPY);

	}

	return (error);
}

/* ARGSUSED */
static int
random_write(struct cdev *dev __unused, struct uio *uio, int flag __unused)
{

	/* We used to allow this to insert userland entropy.
	 * We don't any more because (1) this so-called entropy
	 * is usually lousy and (b) its vaguely possible to
	 * mess with entropy harvesting by overdoing a write.
	 * Now we just ignore input like /dev/null does.
	 */
	uio->uio_resid = 0;

	return (0);
}

/* ARGSUSED */
static int
random_ioctl(struct cdev *dev __unused, u_long cmd, caddr_t addr __unused,
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
random_poll(struct cdev *dev __unused, int events, struct thread *td)
{
	int revents = 0;

	if (events & (POLLIN | POLLRDNORM)) {
		if (random_adaptor->seeded)
			revents = events & (POLLIN | POLLRDNORM);
		else
			revents = (*random_adaptor->poll)(events, td);
	}
	return (revents);
}

static void
random_initialize(void *p, struct random_adaptor *s)
{
	static int random_inited = 0;

	if (random_inited) {
		printf("random: <%s> already initialized\n",
		    random_adaptor->ident);
		return;
	}

	random_adaptor = s;

	(s->init)();

	printf("random: <%s> initialized\n", s->ident);

	/* Use an appropriately evil mode for those who are concerned
	 * with daemons */
	random_dev = make_dev_credf(MAKEDEV_ETERNAL_KLD, &random_cdevsw,
	    RANDOM_MINOR, NULL, UID_ROOT, GID_WHEEL, 0666, "random");
	make_dev_alias(random_dev, "urandom"); /* compatibility */

	/* mark random(4) as initialized, to avoid being called again */
	random_inited = 1;
}

/* ARGSUSED */
static int
random_modevent(module_t mod __unused, int type, void *data __unused)
{
	static eventhandler_tag attach_tag = NULL;
	int error = 0;

	switch (type) {
	case MOD_LOAD:
		random_adaptor_choose(&random_adaptor);

		if (random_adaptor == NULL) {
			printf("random: No random adaptor attached, "
			    "postponing initialization\n");
			attach_tag = EVENTHANDLER_REGISTER(random_adaptor_attach,
			    random_initialize, NULL, EVENTHANDLER_PRI_ANY);
		} else
			random_initialize(NULL, random_adaptor);

		break;

	case MOD_UNLOAD:
		if (random_adaptor != NULL) {
			(*random_adaptor->deinit)();
			destroy_dev(random_dev);
		}
		/* Unregister the event handler */
		if (attach_tag != NULL)
			EVENTHANDLER_DEREGISTER(random_adaptor_attach,
			    attach_tag);

		break;

	case MOD_SHUTDOWN:
		break;

	default:
		error = EOPNOTSUPP;
		break;

	}
	return (error);
}

DEV_MODULE(random, random_modevent, NULL);
MODULE_VERSION(random, 1);
