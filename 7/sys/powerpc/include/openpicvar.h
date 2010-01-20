/*-
 * Copyright (C) 2002 Benno Rice.
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
 * THIS SOFTWARE IS PROVIDED BY Benno Rice ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef	_POWERPC_OPENPICVAR_H_
#define	_POWERPC_OPENPICVAR_H_

#define OPENPIC_DEVSTR	"OpenPIC Interrupt Controller"

#define OPENPIC_IRQMAX	256	/* h/w allows more */

struct openpic_softc {
	device_t	sc_dev;
	struct resource	*sc_memr;
	bus_space_tag_t sc_bt;
	bus_space_handle_t sc_bh;
	char		*sc_version;
	int		sc_rid;
	u_int		sc_ncpu;
	u_int		sc_nirq;
	int		sc_psim;
};

extern devclass_t openpic_devclass;

/*
 * Bus-independent attach i/f
 */
int	openpic_attach(device_t);

/*
 * PIC interface.
 */
void	openpic_dispatch(device_t, struct trapframe *);
void	openpic_enable(device_t, u_int, u_int);
void	openpic_eoi(device_t, u_int);
void	openpic_mask(device_t, u_int);
void	openpic_unmask(device_t, u_int);

#endif /* _POWERPC_OPENPICVAR_H_ */
