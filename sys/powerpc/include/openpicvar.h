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
	char		*sc_version;
	u_int		sc_ncpu;
	u_int		sc_nirq;
	int             sc_psim;
	struct		rman sc_rman;
	bus_space_tag_t sc_bt;
	bus_space_handle_t sc_bh;
	u_int		sc_hwprobed;
	u_int		sc_early_done;
	device_t	sc_altdev;
	u_char		sc_irqrsv[OPENPIC_IRQMAX]; /* pre-h/w reservation */
};

/*
 * Bus-independent attach i/f
 */
int		openpic_early_attach(device_t);
int		openpic_attach(device_t);

/*
 * PIC interface.
 */
struct resource	*openpic_allocate_intr(device_t, device_t, int *,
			    u_long, u_int);
int		openpic_setup_intr(device_t, device_t,
			    struct resource *, int, driver_intr_t, void *,
			    void **);
int		openpic_teardown_intr(device_t, device_t,
			    struct resource *, void *);
int		openpic_release_intr(device_t dev, device_t, int,
			    struct resource *res);

#endif /* _POWERPC_OPENPICVAR_H_ */
