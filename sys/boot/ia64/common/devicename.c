/*-
 * Copyright (c) 1998 Michael Smith <msmith@freebsd.org>
 * Copyright (c) 2006 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/boot/ia64/common/devicename.c,v 1.6 2006/11/05 22:03:03 marcel Exp $");

#include <stand.h>
#include <string.h>
#include <sys/disklabel.h>
#include "bootstrap.h"

#include <efi.h>
#include <efilib.h>

static int ia64_parsedev(struct devdesc **, const char *, const char **);

/* 
 * Point (dev) at an allocated device specifier for the device matching the
 * path in (devspec). If it contains an explicit device specification,
 * use that.  If not, use the default device.
 */
int
ia64_getdev(void **vdev, const char *devspec, const char **path)
{
	struct devdesc **dev = (struct devdesc **)vdev;
	int rv;

	/*
	 * If it looks like this is just a path and no device, then
	 * use the current device instead.
	 */
	if (devspec == NULL || *devspec == '/' || !strchr(devspec, ':')) {
		rv = ia64_parsedev(dev, getenv("currdev"), NULL);
		if (rv == 0 && path != NULL)
			*path = devspec;
		return (rv);
	}

	/* Parse the device name off the beginning of the devspec. */
	return (ia64_parsedev(dev, devspec, path));
}

/*
 * Point (dev) at an allocated device specifier matching the string version
 * at the beginning of (devspec).  Return a pointer to the remaining
 * text in (path).
 *
 * In all cases, the beginning of (devspec) is compared to the names
 * of known devices in the device switch, and then any following text
 * is parsed according to the rules applied to the device type.
 *
 * For disk-type devices, the syntax is:
 *
 * fs<unit>:
 */
static int
ia64_parsedev(struct devdesc **dev, const char *devspec, const char **path)
{
	struct devdesc *idev;
	struct devsw *dv;
	char *cp;
	const char *np;
	int i, err;

	/* minimum length check */
	if (strlen(devspec) < 2)
		return (EINVAL);

	/* look for a device that matches */
	for (i = 0; devsw[i] != NULL; i++) {
		dv = devsw[i];
		if (!strncmp(devspec, dv->dv_name, strlen(dv->dv_name)))
			break;
	}
	if (devsw[i] == NULL)
		return (ENOENT);

	idev = malloc(sizeof(struct devdesc));
	if (idev == NULL)
		return (ENOMEM);

	idev->d_dev = dv;
	idev->d_type = dv->dv_type;
	idev->d_unit = -1;

	err = 0;
	np = devspec + strlen(dv->dv_name);
	if (*np != '\0' && *np != ':') {
		idev->d_unit = strtol(np, &cp, 0);
		if (cp == np) {
			idev->d_unit = -1;
			free(idev);
			return (EUNIT);
		}
	}
	if (*cp != '\0' && *cp != ':') {
		free(idev);
		return (EINVAL);
	}

	if (path != NULL)
		*path = (*cp == 0) ? cp : cp + 1;
	if (dev != NULL)
		*dev = idev;
	else
		free(idev);
	return (0);
}

char *
ia64_fmtdev(void *vdev)
{
	struct devdesc *dev = (struct devdesc *)vdev;
	static char buf[32];	/* XXX device length constant? */

	switch(dev->d_type) {
	case DEVT_NONE:
		strcpy(buf, "(no device)");
		break;

	default:
		sprintf(buf, "%s%d:", dev->d_dev->dv_name, dev->d_unit);
		break;
	}

	return(buf);
}

/*
 * Set currdev to suit the value being supplied in (value)
 */
int
ia64_setcurrdev(struct env_var *ev, int flags, const void *value)
{
	struct devdesc *ncurr;
	int rv;

	rv = ia64_parsedev(&ncurr, value, NULL);
	if (rv != 0)
		return(rv);

	free(ncurr);
	env_setenv(ev->ev_name, flags | EV_NOHOOK, value, NULL, NULL);
	return (0);
}
