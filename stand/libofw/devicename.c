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

#include "bootstrap.h"
#include "libofw.h"

/*
 * Point (dev) at an allocated device specifier for the device matching the
 * path in (devspec). If it contains an explicit device specification,
 * use that.  If not, use the default device.
 */
int
ofw_getdev(void **vdev, const char *devspec, const char **path)
{
	struct devdesc **dev = (struct devdesc **)vdev;
	int rv;

	/*
	 * If it looks like this is just a path and no device, go with the
	 * current device.
	 */
	if (devspec == NULL || strpbrk(devspec, ":@") == NULL) {
		rv = devparse(dev, getenv("currdev"), NULL);
		if (rv == 0  && path != NULL)
			*path = devspec;
		return (rv);
	}

	/*
	 * Try to parse the device name off the beginning of the devspec
	 */
	return (devparse(dev, devspec, path));
}

/*
 * Search the OFW (path) for a node that's of (want_type).
 */
phandle_t
ofw_path_to_handle(const char *ofwpath, const char *want_type, const char **path)
{
	const char *p, *s;
	char name[256];
	char type[64];
	phandle_t handle;
	int len;

	for (p = s = ofwpath; *s != '\0'; p = s) {
		if ((s = strchr(p + 1, '/')) == NULL)
			s = strchr(p, '\0');
		len = s - ofwpath;
		if (len >= sizeof(name))
			return ((phandle_t)-1);
		bcopy(ofwpath, name, len);
		name[len] = '\0';
		if ((handle = OF_finddevice(name)) == -1)
			continue;
		if (OF_getprop(handle, "device_type", type, sizeof(type)) == -1)
			continue;
		if (strcmp(want_type, type) == 0) {
			*path = s;
			return (handle);
		}
	}
	return ((phandle_t)-1);
}

int
ofw_common_parsedev(struct devdesc **dev, const char *devspec, const char **path,
    const char *ofwtype)
{
	const char *rem_path;
	struct ofw_devdesc *idev;

	if (ofw_path_to_handle(devspec, ofwtype, &rem_path) == -1)
		return (ENOENT);
	idev = malloc(sizeof(struct ofw_devdesc));
	if (idev == NULL) {
		printf("ofw_parsedev: malloc failed\n");
		return ENOMEM;
	};
	strlcpy(idev->d_path, devspec, min(rem_path - devspec + 1,
		sizeof(idev->d_path)));
	*dev = &idev->dd;
	if (path != NULL)
		*path = rem_path;
	return 0;
}
