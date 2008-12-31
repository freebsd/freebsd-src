/*-
 * Copyright (c) 2000 Mark R. V. Murray & Jeroen C. van Gelderen
 * Copyright (c) 2001-2004 Mark R. V. Murray
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
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
__FBSDID("$FreeBSD: src/sys/dev/null/null.c,v 1.32.6.1 2008/11/25 02:59:29 kensmith Exp $");

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
#include <machine/bus.h>

/* For use with destroy_dev(9). */
static struct cdev *null_dev;
static struct cdev *zero_dev;

static d_write_t null_write;
static d_ioctl_t null_ioctl;
static d_read_t zero_read;

#define NULL_MINOR	2
#define ZERO_MINOR	12

static struct cdevsw null_cdevsw = {
	.d_version =	D_VERSION,
	.d_read =	(d_read_t *)nullop,
	.d_write =	null_write,
	.d_ioctl =	null_ioctl,
	.d_name =	"null",
};

static struct cdevsw zero_cdevsw = {
	.d_version =	D_VERSION,
	.d_read =	zero_read,
	.d_write =	null_write,
	.d_name =	"zero",
	.d_flags =	D_MMAP_ANON,
};

static void *zbuf;

/* ARGSUSED */
static int
null_write(struct cdev *dev __unused, struct uio *uio, int flags __unused)
{
	uio->uio_resid = 0;

	return (0);
}

/* ARGSUSED */
static int
null_ioctl(struct cdev *dev __unused, u_long cmd, caddr_t data __unused,
    int flags __unused, struct thread *td)
{
	int error;

	if (cmd != DIOCSKERNELDUMP)
		return (ENOIOCTL);
	error = priv_check(td, PRIV_SETDUMPER);
	if (error)
		return (error);
	return (set_dumper(NULL));
}

/* ARGSUSED */
static int
zero_read(struct cdev *dev __unused, struct uio *uio, int flags __unused)
{
	int error = 0;

	while (uio->uio_resid > 0 && error == 0)
		error = uiomove(zbuf, MIN(uio->uio_resid, PAGE_SIZE), uio);

	return (error);
}

/* ARGSUSED */
static int
null_modevent(module_t mod __unused, int type, void *data __unused)
{
	switch(type) {
	case MOD_LOAD:
		if (bootverbose)
			printf("null: <null device, zero device>\n");
		zbuf = (void *)malloc(PAGE_SIZE, M_TEMP, M_WAITOK | M_ZERO);
		null_dev = make_dev(&null_cdevsw, NULL_MINOR, UID_ROOT,
			GID_WHEEL, 0666, "null");
		zero_dev = make_dev(&zero_cdevsw, ZERO_MINOR, UID_ROOT,
			GID_WHEEL, 0666, "zero");
		break;

	case MOD_UNLOAD:
		destroy_dev(null_dev);
		destroy_dev(zero_dev);
		free(zbuf, M_TEMP);
		break;

	case MOD_SHUTDOWN:
		break;

	default:
		return (EOPNOTSUPP);
	}

	return (0);
}

DEV_MODULE(null, null_modevent, NULL);
MODULE_VERSION(null, 1);
