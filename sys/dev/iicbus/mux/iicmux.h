/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Ian Lepore <ian@freebsd.org>
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

#ifndef __IICMUX_H
#define __IICMUX_H

#define	IICMUX_MAX_BUSES	16	/* More than any available mux chip. */

/*
 * IICMUX_SELECT_IDLE instructs the mux hardware driver to do whatever it is
 * configured to do when the downstream buses are idle.  Hardware has varying
 * capabilities; it may disconnect all downstream buses, or connect a specific
 * bus, or just leave whatever bus was last used connected.  Hardware which is
 * capable of various choices will have some mechanism to configure the choice
 * which is handled outside of the iicmux framework.
 */
#define	IICMUX_SELECT_IDLE	(-1)

/*
 * The iicmux softc; chip drivers should embed one of these as the first member
 * variable of their own softc struct, and must call iicmux_attach() to
 * initialize it before calling any other iicmux functions.
 */
struct iicmux_softc {
	device_t	busdev;   /* Upstream i2c bus (may not be our parent). */
	device_t	dev;      /* Ourself. */
	int		maxbus;   /* Index of highest populated busdevs slot. */
	int		numbuses; /* Number of buses supported by the chip. */
	int		debugmux; /* Write debug messages when > 0. */
	device_t	childdevs[IICMUX_MAX_BUSES]; /* Child bus instances. */
#ifdef FDT
	phandle_t	childnodes[IICMUX_MAX_BUSES]; /* Child bus fdt nodes. */
#endif
};

DECLARE_CLASS(iicmux_driver);

/*
 * Helpers to call from attach/detach functions of chip-specific drivers.
 *
 * The iicmux_attach() function initializes the core driver's portion of the
 * softc, and creates child iicbus instances for any children it can identify
 * using hints and FDT data.  If a chip driver does its own device_add_child()
 * calls to add other downstream buses that participate in the mux switching, it
 * must call iicmux_add_child() to inform the core driver of the downstream
 * busidx<->device_t relationship.
 */
int  iicmux_add_child(device_t dev, device_t child, int busidx);
int  iicmux_attach(device_t dev, device_t busdev, int numbuses);
int  iicmux_detach(device_t dev);

#endif
