/*-
 * Copyright (c) 2017 Ilya Bakulin.  All rights reserved.
 * Copyright (c) 2018-2019 The FreeBSD Foundation
 *
 * Portions of this software were developed by Björn Zeeb
 * under sponsorship from the FreeBSD Foundation.
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
 *
 * Portions of this software may have been developed with reference to
 * the SD Simplified Specification.  The following disclaimer may apply:
 *
 * The following conditions apply to the release of the simplified
 * specification ("Simplified Specification") by the SD Card Association and
 * the SD Group. The Simplified Specification is a subset of the complete SD
 * Specification which is owned by the SD Card Association and the SD
 * Group. This Simplified Specification is provided on a non-confidential
 * basis subject to the disclaimers below. Any implementation of the
 * Simplified Specification may require a license from the SD Card
 * Association, SD Group, SD-3C LLC or other third parties.
 *
 * Disclaimers:
 *
 * The information contained in the Simplified Specification is presented only
 * as a standard specification for SD Cards and SD Host/Ancillary products and
 * is provided "AS-IS" without any representations or warranties of any
 * kind. No responsibility is assumed by the SD Group, SD-3C LLC or the SD
 * Card Association for any damages, any infringements of patents or other
 * right of the SD Group, SD-3C LLC, the SD Card Association or any third
 * parties, which may result from its use. No license is granted by
 * implication, estoppel or otherwise under any patent or other rights of the
 * SD Group, SD-3C LLC, the SD Card Association or any third party. Nothing
 * herein shall be construed as an obligation by the SD Group, the SD-3C LLC
 * or the SD Card Association to disclose or distribute any technical
 * information, know-how or other confidential information to any third party.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/endian.h>

#include <dev/mmc/mmcreg.h>

#include <dev/sdio/sdiob.h>
#include <dev/sdio/sdio_subr.h>

#include "sdio_if.h"

/* Works on F0. */
static int
sdio_set_bool_for_func(device_t dev, uint32_t addr, uint8_t fn, bool enable)
{
	device_t pdev;
	int error;
	uint8_t val;
	bool enabled;

	pdev = device_get_parent(dev);
	error = SDIO_READ_DIRECT(pdev, 0, addr, &val);
	if (error != 0)
		return (error);

	enabled = (val & (1 << fn)) ? true : false;
	if (enabled == enable)
		return (0);

	if (enable)
		val |= (1 << fn);
	else
		val &= ~(1 << fn);
	error = SDIO_WRITE_DIRECT(pdev, 0, addr, val);
	return (error);
}

int
sdio_enable_func(struct sdio_func *f)
{

	return (sdio_set_bool_for_func(f->dev, SD_IO_CCCR_FN_ENABLE,
	    f->fn, true));
}

int
sdio_disable_func(struct sdio_func *f)
{

	return (sdio_set_bool_for_func(f->dev, SD_IO_CCCR_FN_ENABLE,
	    f->fn, false));
}

int
sdio_set_block_size(struct sdio_func *f, uint16_t bs)
{
	device_t pdev;
	int error;
	uint32_t addr;
	uint16_t v;

	if (!sdio_get_support_multiblk(f->dev))
		return (EOPNOTSUPP);

	pdev = device_get_parent(f->dev);
	addr = SD_IO_FBR_START * f->fn + SD_IO_FBR_IOBLKSZ;
	v = htole16(bs);
	/* Always write through F0. */
	error = SDIO_WRITE_DIRECT(pdev, 0, addr, v & 0xff);
	if (error == 0)
		error = SDIO_WRITE_DIRECT(pdev, 0, addr + 1,
		    (v >> 8) & 0xff);
	if (error == 0)
		f->cur_blksize = bs;

	return (error);
}

uint8_t
sdio_read_1(struct sdio_func *f, uint32_t addr, int *err)
{
	int error;
	uint8_t v;

	error = SDIO_READ_DIRECT(device_get_parent(f->dev), f->fn, addr, &v);
	if (error) {
		if (err != NULL)
			*err = error;
		return (0xff);
	} else {
		if (err != NULL)
			*err = 0;
		return (v);
	}
}

void
sdio_write_1(struct sdio_func *f, uint32_t addr, uint8_t val, int *err)
{
	int error;

	error = SDIO_WRITE_DIRECT(device_get_parent(f->dev), f->fn, addr, val);
	if (err != NULL)
		*err = error;
}

uint32_t
sdio_read_4(struct sdio_func *f, uint32_t addr, int *err)
{
	int error;
	uint32_t v;

	error = SDIO_READ_EXTENDED(device_get_parent(f->dev), f->fn, addr,
	    sizeof(v), (uint8_t *)&v, false);
	if (error) {
		if (err != NULL)
			*err = error;
		return (0xffffffff);
	} else {
		if (err != NULL)
			*err = 0;
		return (le32toh(v));
	}
}

void
sdio_write_4(struct sdio_func *f, uint32_t addr, uint32_t val, int *err)
{
	int error;

	error = SDIO_WRITE_EXTENDED(device_get_parent(f->dev), f->fn, addr,
	    sizeof(val), (uint8_t *)&val, false);
	if (err != NULL)
		*err = error;
}

uint8_t
sdio_f0_read_1(struct sdio_func *f, uint32_t addr, int *err)
{
	int error;
	uint8_t v;

	error = SDIO_READ_DIRECT(device_get_parent(f->dev), 0, addr, &v);
	if (error) {
		if (err != NULL)
			*err = error;
		return (0xff);
	} else {
		if (err != NULL)
			*err = 0;
		return (v);
	}
}

void
sdio_f0_write_1(struct sdio_func *f, uint32_t addr, uint8_t val, int *err)
{
	int error;

	error = SDIO_WRITE_DIRECT(device_get_parent(f->dev), 0, addr, val);
	if (err != NULL)
		*err = error;
}

/* end */
