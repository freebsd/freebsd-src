/*-
 * Copyright (c) 2000 Mark R. V. Murray & Jeroen C. van Gelderen
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
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

/* For use with destroy_dev(9). */
static dev_t null_dev;
static dev_t zero_dev;

static d_write_t null_write;
static d_read_t zero_read;

#define CDEV_MAJOR	2
#define NULL_MINOR	2
#define ZERO_MINOR	12

static struct cdevsw null_cdevsw = {
	/* open */	(d_open_t *)nullop,
	/* close */	(d_close_t *)nullop,
	/* read */	(d_read_t *)nullop,
	/* write */	null_write,
	/* ioctl */	noioctl,
	/* poll */	nopoll,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* name */	"null",
	/* maj */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	0,
};

static struct cdevsw zero_cdevsw = {
	/* open */	(d_open_t *)nullop,
	/* close */	(d_close_t *)nullop,
	/* read */	zero_read,
	/* write */	null_write,
	/* ioctl */	noioctl,
	/* poll */	nopoll,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* name */	"zero",
	/* maj */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	D_MMAP_ANON,
};

static void *zbuf;

static int
null_write(dev_t dev, struct uio *uio, int flag)
{
	uio->uio_resid = 0;
	return 0;
}

static int
zero_read(dev_t dev, struct uio *uio, int flag)
{
	u_int c;
	int error = 0;

	while (uio->uio_resid > 0 && error == 0) {
		c = min(uio->uio_resid, PAGE_SIZE);
		error = uiomove(zbuf, c, uio);
	}
	return error;
}

static int
null_modevent(module_t mod, int type, void *data)
{
	switch(type) {
	case MOD_LOAD:
		if (bootverbose)
			printf("null: <null device, zero device>\n");
		zbuf = (void *)malloc(PAGE_SIZE, M_TEMP, M_WAITOK | M_ZERO);
		zero_dev = make_dev(&zero_cdevsw, ZERO_MINOR, UID_ROOT,
			GID_WHEEL, 0666, "zero");
		null_dev = make_dev(&null_cdevsw, NULL_MINOR, UID_ROOT,
			GID_WHEEL, 0666, "null");
		return 0;

	case MOD_UNLOAD:
		destroy_dev(null_dev);
		destroy_dev(zero_dev);
		free(zbuf, M_TEMP);
		return 0;

	case MOD_SHUTDOWN:
		return 0;

	default:
		return EOPNOTSUPP;
	}
}

DEV_MODULE(null, null_modevent, NULL);
