/*-
 * Copyright (c) 2015 Ruslan Bukin <br@bsdpad.com>
 * All rights reserved.
 *
 * Portions of this software were developed by SRI International and the
 * University of Cambridge Computer Laboratory under DARPA/AFRL contract
 * FA8750-10-C-0237 ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * Portions of this software were developed by the University of Cambridge
 * Computer Laboratory as part of the CTSRD Project, with support from the
 * UK Higher Education Innovation Fund (HEIF).
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

#define	HTIF_DEV_ID_SHIFT	(56)
#define	HTIF_DEV_ID_MASK	(0xfful << HTIF_DEV_ID_SHIFT)
#define	HTIF_CMD_SHIFT		(48)
#define	HTIF_CMD_MASK		(0xfful << HTIF_CMD_SHIFT)
#define	HTIF_DATA_SHIFT		(0)
#define	HTIF_DATA_MASK		(0xffffffff << HTIF_DATA_SHIFT)

#define	HTIF_CMD_READ			(0x00ul)
#define	HTIF_CMD_WRITE			(0x01ul)
#define	HTIF_CMD_READ_CONTROL_REG	(0x02ul)
#define	HTIF_CMD_WRITE_CONTROL_REG	(0x03ul)
#define	HTIF_CMD_IDENTIFY		(0xfful)
#define	 IDENTIFY_PADDR_SHIFT		8
#define	 IDENTIFY_IDENT			0xff

#define	HTIF_NDEV		(256)
#define	HTIF_ID_LEN		(64)
#define	HTIF_ALIGN		(64)

#define	HTIF_DEV_CMD(entry)	((entry & HTIF_CMD_MASK) >> HTIF_CMD_SHIFT)
#define	HTIF_DEV_ID(entry)	((entry & HTIF_DEV_ID_MASK) >> HTIF_DEV_ID_SHIFT)
#define	HTIF_DEV_DATA(entry)	((entry & HTIF_DATA_MASK) >> HTIF_DATA_SHIFT)

/* bus softc */
struct htif_softc {
	struct resource		*res[1];
	void			*ihl[1];
	device_t		dev;
	uint64_t		identify_id;
	uint64_t		identify_done;
};

/* device private data */
struct htif_dev_ivars {
	char			*id;
	int			index;
	device_t		dev;
	struct htif_softc	*sc;
};

uint64_t htif_command(uint64_t);
int htif_setup_intr(int id, void *func, void *arg);
int htif_read_ivar(device_t dev, device_t child, int which, uintptr_t *result);

enum htif_device_ivars {
	HTIF_IVAR_INDEX,
	HTIF_IVAR_ID,
};

/*
 * Simplified accessors for HTIF devices
 */
#define HTIF_ACCESSOR(var, ivar, type)	\
	__BUS_ACCESSOR(htif, var, HTIF, ivar, type)

HTIF_ACCESSOR(index, INDEX, int);
HTIF_ACCESSOR(id, ID, char *);
