/*-
 * Copyright (c) 1998 Michael Smith <msmith@freebsd.org>
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

#include <stand.h>
#include <string.h>

#include "bootstrap.h"
#include "disk.h"
#include "libuboot.h"

static int uboot_parsedev(struct uboot_devdesc **dev, const char *devspec,
    const char **path);

/*
 * Point (dev) at an allocated device specifier for the device matching the
 * path in (devspec). If it contains an explicit device specification,
 * use that.  If not, use the default device.
 */
int
uboot_getdev(void **vdev, const char *devspec, const char **path)
{
	struct uboot_devdesc **dev = (struct uboot_devdesc **)vdev;
	int rv;

	/*
	 * If it looks like this is just a path and no
	 * device, go with the current device.
	 */
	if ((devspec == NULL) || (devspec[0] == '/') ||
	    (strchr(devspec, ':') == NULL)) {

		if (((rv = uboot_parsedev(dev, getenv("currdev"), NULL)) == 0)
		    && (path != NULL))
		*path = devspec;
		return(rv);
	}

	/*
	 * Try to parse the device name off the beginning of the devspec.
	 */
	return (uboot_parsedev(dev, devspec, path));
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
 * disk<unit>[<partition>]:
 *
 */
static int
uboot_parsedev(struct uboot_devdesc **dev, const char *devspec,
    const char **path)
{
	struct uboot_devdesc *idev;
	struct devsw *dv;
	char *cp;
	const char *np;
	int i, unit, err;

	/* minimum length check */
	if (strlen(devspec) < 2)
		return(EINVAL);

	/* look for a device that matches */
	for (i = 0, dv = NULL; devsw[i] != NULL; i++) {
		if (!strncmp(devspec, devsw[i]->dv_name,
		    strlen(devsw[i]->dv_name))) {
			dv = devsw[i];
			break;
		}
	}
	if (dv == NULL)
		return(ENOENT);
	idev = malloc(sizeof(struct uboot_devdesc));
	err = 0;
	np = (devspec + strlen(dv->dv_name));

	switch(dv->dv_type) {
	case DEVT_NONE:
		break;

#ifdef LOADER_DISK_SUPPORT
	case DEVT_DISK:
		free(idev);
		err = disk_parsedev((struct devdesc **)&idev, devspec, path);
		if (err != 0)
			goto fail;
		break;
#endif

	case DEVT_NET:
		unit = 0;

		if (*np && (*np != ':')) {
			/* get unit number if present */
			unit = strtol(np, &cp, 0);
			if (cp == np) {
				err = EUNIT;
				goto fail;
			}
		}
		if (*cp && (*cp != ':')) {
			err = EINVAL;
			goto fail;
		}
		idev->dd.d_unit = unit;

		if (path != NULL)
			*path = (*cp == 0) ? cp : cp + 1;
		break;

	default:
		err = EINVAL;
		goto fail;
	}
	idev->dd.d_dev = dv;
	/*
	 * dev can be NULL, since uboot_getdev calls us directly, rather than via
	 * dv_parsedev in devparse() which otherwise ensures that it can't be NULL.
	 */
	if (dev == NULL) {
		free(idev);
	} else {
		*dev = idev;
	}
	return (0);

fail:
	free(idev);
	return (err);
}


/*
 * Set currdev to suit the value being supplied in (value).
 */
int
uboot_setcurrdev(struct env_var *ev, int flags, const void *value)
{
	struct uboot_devdesc *ncurr;
	int rv;

	if ((rv = uboot_parsedev(&ncurr, value, NULL)) != 0)
		return (rv);
	free(ncurr);

	return (mount_currdev(ev, flags, value));
}
