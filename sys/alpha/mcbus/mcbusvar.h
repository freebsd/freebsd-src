/* $FreeBSD$ */
/*-
 * Copyright (c) 2000 by Matthew Jacob
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Definitions for the MCBUS System Bus found on
 * AlphaServer 4100 systems.
 */

enum mcbus_device_instvars {
	MCBUS_IVAR_MID,
	MCBUS_IVAR_GID,
	MCBUS_IVAR_TYPE,
};

#define MCBUS_ACCESSOR(A, B, T)						 \
									 \
static __inline T mcbus_get_ ## A(device_t dev)				 \
{									 \
	u_long v;							 \
	BUS_READ_IVAR(device_get_parent(dev), dev, MCBUS_IVAR_ ## B, &v); \
	return v;							 \
}

MCBUS_ACCESSOR(mid, MID, u_int8_t)
MCBUS_ACCESSOR(gid, GID, u_int8_t)
MCBUS_ACCESSOR(type, TYPE, u_int8_t)

/*
 * The structure used to attach devices to the MCBUS
 */
struct mcbus_device {
	u_int8_t	ma_gid;		/* GID of MCBUS (MCBUS #) */
	u_int8_t	ma_mid;		/* Module ID on MCBUS */
	u_int8_t	ma_type;	/* Module "type" */
	u_int8_t	ma_order;	/* order of attachment */
};
#define	MCBUS_GID_FROM_INSTANCE(unit)	(7 - unit)

/*
 * "types"
 */
#define	MCBUS_TYPE_RES	0
#define	MCBUS_TYPE_UNK	1
#define	MCBUS_TYPE_MEM	2
#define	MCBUS_TYPE_CPU	3
#define	MCBUS_TYPE_PCI	4

#define DEVTOMCBUS(dev)	((struct mcbus_device *) device_get_ivars(dev))

extern void mcbus_init(void);
