/* $FreeBSD$ */
/* $NetBSD: tlsbvar.h,v 1.5 1998/05/13 23:23:23 thorpej Exp $ */

/*-
 * Copyright (c) 1997, 2000 by Matthew Jacob
 * NASA AMES Research Center.
 * All rights reserved.
 *
 * Based in part upon a prototype version by Jason Thorpe
 * Copyright (c) 1996 by Jason Thorpe.
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 * Definitions for the TurboLaser System Bus found on
 * AlphaServer 8200/8400 systems.
 */

enum tlsb_device_instvars {
	TLSB_IVAR_NODE,
	TLSB_IVAR_DTYPE,
	TLSB_IVAR_SWREV,
	TLSB_IVAR_HWREV
};

/*
 * Simplified accessors for turbolaser devices
 */

#define TLSB_ACCESSOR(A, B, T)						 \
									 \
static __inline T tlsb_get_ ## A(device_t dev)				 \
{									 \
	u_long v;							 \
	BUS_READ_IVAR(device_get_parent(dev), dev, TLSB_IVAR_ ## B, &v); \
	return v;							 \
}

TLSB_ACCESSOR(node, NODE, int)
TLSB_ACCESSOR(dtype, DTYPE, u_int16_t)
TLSB_ACCESSOR(hwrev, HWREV, u_int8_t)
TLSB_ACCESSOR(swrev, SWREV, u_int8_t)

/*
 * Bus-dependent structure for CPUs. This is dynamically allocated
 * for each CPU on the TurboLaser, and glued into the cpu_softc
 * as sc_busdep (when there is a cpu_softc to do this to).
 */
struct tlsb_cpu_busdep {
	u_int8_t	tcpu_vid;	/* virtual ID of CPU */
	int		tcpu_node;	/* TurboLaser node */
};

/*
 * The structure used to attach devices to the TurboLaser.
 */
struct tlsb_device {
	int		td_node;	/* node number (TLSB slot) */
	u_int32_t	td_tldev;	/* tl device id */
};
#define DEVTOTLSB(dev)	((struct tlsb_device *) device_get_ivars(dev))
#ifdef	_KERNEL
extern struct tlsb_device *tlsb_primary_cpu;
#endif
