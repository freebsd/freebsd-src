/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020-2021 The FreeBSD Foundation
 *
 * This software was developed by Björn Zeeb under sponsorship from
 * the FreeBSD Foundation.
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
 *
 * $FreeBSD$
 */

#include <sys/types.h>
#include <sys/malloc.h>
#include <sys/firmware.h>

#include <linux/types.h>
#include <linux/device.h>

#include <linux/firmware.h>
#undef firmware

MALLOC_DEFINE(M_LKPI_FW, "lkpifw", "LinuxKPI firmware");

static int
_linuxkpi_request_firmware(const char *fw_name, const struct linuxkpi_firmware **fw,
    struct device *dev, gfp_t gfp __unused, bool enoentok, bool warn)
{
	const struct firmware *fbdfw;
	struct linuxkpi_firmware *lfw;
	const char *fwimg;
	char *p;
	uint32_t flags;

	if (fw_name == NULL || fw == NULL || dev == NULL)
		return (-EINVAL);

	/* Set independent on "warn". To debug, bootverbose is avail. */
	flags = FIRMWARE_GET_NOWARN;

	KASSERT(gfp == GFP_KERNEL, ("%s: gfp %#x\n", __func__, gfp));
	lfw = malloc(sizeof(*lfw), M_LKPI_FW, M_WAITOK | M_ZERO);

	/*
	 * Linux can have a path in the firmware which is hard to replicate
	 * for auto-firmware-module-loading.
	 * On FreeBSD, depending on what people do, the firmware will either
	 * be called "fw", or "dir_fw", or "modname_dir_fw".  The latter the
	 * driver author has to deal with herself (requesting the special name).
	 * We also optionally flatten '/'s and '.'s as some firmware modules do.
	 * We probe in the least-of-work order avoiding memory operations.
	 * It will be preferred to build the firmware .ko in a well matching
	 * way rather than adding more name-mangling-hacks here in the future
	 * (though we could if needed).
	 */
	/* (1) Try any name removed of path. */
	fwimg = strrchr(fw_name, '/');
	if (fwimg != NULL)
		fwimg++;
	if (fwimg == NULL || *fwimg == '\0')
		fwimg = fw_name;
	fbdfw = firmware_get_flags(fwimg, flags);
	/* (2) Try the original name if we have not yet. */
	if (fbdfw == NULL && fwimg != fw_name) {
		fwimg = fw_name;
		fbdfw = firmware_get_flags(fwimg, flags);
	}
	/* (3) Flatten '/' and then '.' to '_' and try with adjusted name. */
	if (fbdfw == NULL &&
	    (strchr(fw_name, '/') != NULL || strchr(fw_name, '.') != NULL)) {
		fwimg = strdup(fw_name, M_LKPI_FW);
		if (fwimg != NULL) {
			while ((p = strchr(fwimg, '/')) != NULL)
				*p = '_';
			fbdfw = firmware_get_flags(fwimg, flags);
			if (fbdfw == NULL) {
				while ((p = strchr(fwimg, '.')) != NULL)
					*p = '_';
				fbdfw = firmware_get_flags(fwimg, flags);
			}
			free(__DECONST(void *, fwimg), M_LKPI_FW);
		}
	}
	if (fbdfw == NULL) {
		if (enoentok)
			*fw = lfw;
		else {
			free(lfw, M_LKPI_FW);
			*fw = NULL;
		}
		if (warn)
			device_printf(dev->bsddev, "could not load firmware "
			    "image '%s'\n", fw_name);
		return (-ENOENT);
	}

	device_printf(dev->bsddev,"successfully loaded firmware image '%s'\n",
	    fw_name);
	lfw->fbdfw = fbdfw;
	lfw->data = (const uint8_t *)fbdfw->data;
	lfw->size = fbdfw->datasize;
	*fw = lfw;
	return (0);
}

int
linuxkpi_request_firmware_nowait(struct module *mod __unused, bool _t __unused,
    const char *fw_name, struct device *dev, gfp_t gfp, void *drv,
    void(*cont)(const struct linuxkpi_firmware *, void *))
{
	const struct linuxkpi_firmware *lfw;
	int error;

	/*
	 * Linux seems to run the callback if it cannot find the firmware.
	 * The fact that this is "_nowait()" and has a callback seems to
	 * imply that this is run in a deferred conext which we currently
	 * do not do.  Should it become necessary (a driver actually requiring
	 * it) we would need to implement it here.
	 */
	error = _linuxkpi_request_firmware(fw_name, &lfw, dev, gfp, true, true);
	if (error == -ENOENT)
		error = 0;
	if (error == 0)
		cont(lfw, drv);

	return (error);
}

int
linuxkpi_request_firmware(const struct linuxkpi_firmware **fw,
    const char *fw_name, struct device *dev)
{

	return (_linuxkpi_request_firmware(fw_name, fw, dev, GFP_KERNEL, false,
	    true));
}

int
linuxkpi_firmware_request_nowarn(const struct linuxkpi_firmware **fw,
    const char *fw_name, struct device *dev)
{

	return (_linuxkpi_request_firmware(fw_name, fw, dev, GFP_KERNEL, false,
	    false));
}

void
linuxkpi_release_firmware(const struct linuxkpi_firmware *fw)
{

	if (fw == NULL)
		return;

	if (fw->fbdfw)
		firmware_put(fw->fbdfw, FIRMWARE_UNLOAD);
	free(__DECONST(void *, fw), M_LKPI_FW);
}
