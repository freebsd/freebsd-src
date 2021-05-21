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
 *
 *
 * $FreeBSD$
 */

#ifndef _SDIO_SUBR_H_
#define _SDIO_SUBR_H_

/*
 * This file contains structures and functions to work with SDIO cards.
 */

struct sdio_func {
	device_t	dev;		/* The device to talk to CAM. */
	uintptr_t	drvdata;	/* Driver specific data. */

	uint8_t		fn;		/* Function number. */

	uint8_t		class;		/* Class of function. */
	uint16_t	vendor;		/* Manufacturer ID. */
	uint16_t	device;		/* Card ID. */

	uint16_t	max_blksize;	/* Maximum block size of function. */
	uint16_t	cur_blksize;	/* Current block size of function. */

	uint16_t	retries;	/* Retires for CAM operations. */
	uint32_t	timeout;	/* Timeout. */
};

struct card_info {
	struct sdio_func f[8];

	/* Compared to R4 Number of I/O Functions we DO count F0 here. */
	uint8_t		num_funcs;

	bool		support_multiblk; /* Support Multiple Block Transfer */
};

#ifdef _KERNEL
int sdio_enable_func(struct sdio_func *);
int sdio_disable_func(struct sdio_func *);
int sdio_set_block_size(struct sdio_func *, uint16_t);

uint8_t sdio_read_1(struct sdio_func *, uint32_t, int *);
void sdio_write_1(struct sdio_func *, uint32_t, uint8_t, int *);
uint32_t sdio_read_4(struct sdio_func *, uint32_t, int *);
void sdio_write_4(struct sdio_func *, uint32_t, uint32_t, int *);

uint8_t sdio_f0_read_1(struct sdio_func *, uint32_t, int *);
void sdio_f0_write_1(struct sdio_func *, uint32_t, uint8_t, int *);
#endif /* _KERNEL */

#endif /* _SDIO_SUBR_H_ */
