/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020-2021 The FreeBSD Foundation
 * Copyright (c) 2022 Bjoern A. Zeeb
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

#ifndef	_LINUXKPI_LINUX_FIRMWARE_H
#define	_LINUXKPI_LINUX_FIRMWARE_H

#include <sys/types.h>
#include <linux/types.h>
#include <linux/device.h>

struct firmware;

struct linuxkpi_firmware {
	size_t			size;
	const uint8_t		*data;
	/* XXX Does Linux expose anything else? */

	/* This is LinuxKPI implementation private. */
	const struct firmware	*fbdfw;
};

int linuxkpi_request_firmware_nowait(struct module *, bool, const char *,
    struct device *, gfp_t, void *,
    void(*cont)(const struct linuxkpi_firmware *, void *));
int linuxkpi_request_firmware(const struct linuxkpi_firmware **,
    const char *, struct device *);
int linuxkpi_firmware_request_nowarn(const struct linuxkpi_firmware **,
    const char *, struct device *);
void linuxkpi_release_firmware(const struct linuxkpi_firmware *);
int linuxkpi_request_partial_firmware_into_buf(const struct linuxkpi_firmware **,
    const char *, struct device *, uint8_t *, size_t, size_t);


static __inline int
request_firmware_nowait(struct module *mod, bool _t,
    const char *fw_name, struct device *dev, gfp_t gfp, void *drv,
    void(*cont)(const struct linuxkpi_firmware *, void *))
{


	return (linuxkpi_request_firmware_nowait(mod, _t, fw_name, dev, gfp,
	    drv, cont));
}

static __inline int
request_firmware(const struct linuxkpi_firmware **fw,
    const char *fw_name, struct device *dev)
{

	return (linuxkpi_request_firmware(fw, fw_name, dev));
}

static __inline int
request_firmware_direct(const struct linuxkpi_firmware **fw,
    const char *fw_name, struct device *dev)
{

	return (linuxkpi_request_firmware(fw, fw_name, dev));
}

static __inline int
firmware_request_nowarn(const struct linuxkpi_firmware **fw,
    const char *fw_name, struct device *dev)
{

	return (linuxkpi_firmware_request_nowarn(fw, fw_name, dev));
}

static __inline void
release_firmware(const struct linuxkpi_firmware *fw)
{

	linuxkpi_release_firmware(fw);
}

static inline int
request_partial_firmware_into_buf(const struct linuxkpi_firmware **fw,
    const char *fw_name, struct device *dev, void *buf, size_t buflen,
    size_t offset)
{

	return (linuxkpi_request_partial_firmware_into_buf(fw, fw_name,
	    dev, buf, buflen, offset));
}

#define	firmware	linuxkpi_firmware

#endif	/* _LINUXKPI_LINUX_FIRMWARE_H */
