/*-
 * Copyright (c) 2012-2013 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/priv.h>
#include <sys/disk.h>
#include <sys/bus.h>
#include <sys/filio.h>

#include <machine/bus.h>
#include <machine/vmparam.h>

#define	BUSDMA_VERSION	1

static const char busdma_name[] = "busdma";

/* For use with destroy_dev(9). */
static struct cdev *busdma_dev;

static d_read_t busdma_read;
static d_write_t busdma_write;
static d_ioctl_t busdma_ioctl;

static struct cdevsw busdma_cdevsw = {
	.d_version =	D_VERSION,
	.d_read =	busdma_read,
	.d_write =	busdma_write,
	.d_ioctl =	busdma_ioctl,
	.d_name =	busdma_name,
};

static int
busdma_read(struct cdev *dev __unused, struct uio *uio, int flags __unused)
{

	return (ENXIO);
}

static int
busdma_write(struct cdev *dev __unused, struct uio *uio, int flags __unused)
{

	return (ENXIO);
}

static int
busdma_ioctl(struct cdev *dev __unused, u_long cmd, caddr_t data __unused,
    int flags __unused, struct thread *td)
{

	return (ENOIOCTL);
}

static int
busdma_modevent(module_t mod __unused, int type, void *data __unused)
{

	switch(type) {
	case MOD_LOAD:
		printf("%s: <BUSDMA unit test driver, version %u>\n",
		    busdma_name, BUSDMA_VERSION);
		busdma_dev = make_dev_credf(MAKEDEV_ETERNAL_KLD,
		    &busdma_cdevsw, 0, NULL, UID_ROOT, GID_WHEEL, 0666,
		    busdma_name);
		break;

	case MOD_UNLOAD:
		destroy_dev(busdma_dev);
		break;

	case MOD_SHUTDOWN:
		break;

	default:
		return (EOPNOTSUPP);
	}

	return (0);
}

DEV_MODULE(busdma, busdma_modevent, NULL);
MODULE_VERSION(busdma, BUSDMA_VERSION);
