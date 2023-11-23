/*	$NetBSD: dev.c,v 1.4 1994/10/30 21:48:23 cgd Exp $	*/

/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/reboot.h>

#include "stand.h"

int
nodev(void)
{
	return (ENXIO);
}

void
nullsys(void)
{
}

/* ARGSUSED */
int
noioctl(struct open_file *f __unused, u_long cmd __unused, void *data __unused)
{
	return (EINVAL);
}

char *
devformat(struct devdesc *d)
{
	static char name[DEV_DEVLEN];

	if (d->d_dev->dv_fmtdev != NULL)
		return (d->d_dev->dv_fmtdev(d));
	snprintf(name, sizeof(name), "%s%d:", d->d_dev->dv_name, d->d_unit);
	return (name);
}

/* NB: devspec points to the remainder of the device name after dv_name */
static int
default_parsedev(struct devdesc **dev, const char *devspec,
    const char **path)
{
	struct devdesc *idev;
	int unit, err;
	char *cp;

	idev = malloc(sizeof(struct devdesc));
	if (idev == NULL)
		return (ENOMEM);

	unit = 0;
	cp = (char *)devspec;	/* strtol interface, alas */

	if (*devspec != '\0' && *devspec != ':') {
		errno = 0;
		unit = strtol(devspec, &cp, 0);
		if (errno != 0 || cp == devspec) {
			err = EUNIT;
			goto fail;
		}
	}
	if (*cp != '\0' && *cp != ':') {
		err = EINVAL;
		goto fail;
	}

	idev->d_unit = unit;
	if (path != NULL)
		*path = (*cp == 0) ? cp : cp + 1;
	*dev = idev;
	return (0);
fail:
	free(idev);
	return (err);
}

/* NB: devspec points to the whole device spec, and possible trailing path */
int
devparse(struct devdesc **dev, const char *devspec, const char **path)
{
	struct devdesc *idev;
	struct devsw *dv;
	int i, err;
	const char *np;

	/* minimum length check */
	if (strlen(devspec) < 2)
		return (EINVAL);

	/* look for a device that matches */
	for (i = 0; devsw[i] != NULL; i++) {
		dv = devsw[i];
		if (dv->dv_match != NULL) {
			if (dv->dv_match(dv, devspec) != 0)
				break;
		} else {
			if (!strncmp(devspec, dv->dv_name, strlen(dv->dv_name)))
				break;
		}
	}
	if (devsw[i] == NULL)
		return (ENOENT);
	idev = NULL;
	err = 0;
	if (dv->dv_parsedev) {
		err = dv->dv_parsedev(&idev, devspec, path);
	} else {
		np = devspec + strlen(dv->dv_name);
		err = default_parsedev(&idev, np, path);
	}
	if (err != 0)
		return (err);

	idev->d_dev = dv;
	if (dev != NULL)
		*dev = idev;
	else
		free(idev);
	return (0);
}

int
devinit(void)
{
	int err = 0;

	/*
	 * March through the device switch probing for things.
	 */
	for (int i = 0; devsw[i] != NULL; i++) {
		if (devsw[i]->dv_init != NULL) {
			if ((devsw[i]->dv_init)() != 0) {
				err++;
			}
		}
	}
	return (err);
}

void
dev_cleanup(void)
{
    int		i;

    /* Call cleanup routines */
    for (i = 0; devsw[i] != NULL; ++i)
	if (devsw[i]->dv_cleanup != NULL)
	    (devsw[i]->dv_cleanup)();
}
