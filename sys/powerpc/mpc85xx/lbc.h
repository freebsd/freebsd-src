/*-
 * Copyright (c) 2006-2008, Juniper Networks, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
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
 * $FreeBSD$
 */

#ifndef _MACHINE_LBC_H_
#define	_MACHINE_LBC_H_

/* Maximum number of devices on Local Bus */
#define	LBC_DEV_MAX	8

/* Local access registers */
#define	LBC85XX_BR(n)	(8 * n)
#define	LBC85XX_OR(n)	(4 + (8 * n))
#define	LBC85XX_LBCR	(0xd0)
#define	LBC85XX_LCRR	(0xd4)

/* LBC machine select */
#define	LBCRES_MSEL_GPCM	0
#define	LBCRES_MSEL_FCM		1
#define	LBCRES_MSEL_UPMA	8
#define	LBCRES_MSEL_UPMB	9
#define	LBCRES_MSEL_UPMC	10

/* LBC data error checking modes */
#define	LBCRES_DECC_DISABLED	0
#define	LBCRES_DECC_NORMAL	1
#define	LBCRES_DECC_RMW		2

/* LBC atomic operation modes */
#define	LBCRES_ATOM_DISABLED	0
#define	LBCRES_ATOM_RAWA	1
#define	LBCRES_ATOM_WARA	2

struct lbc_bank {
	u_long		pa;		/* physical addr of the bank */
	u_long		size;		/* bank size */
	vm_offset_t	va;		/* VA of the bank */

	/*
	 * XXX the following bank attributes do not have properties specified
	 * in the LBC DTS bindings yet (11.2009), so they are mainly a
	 * placeholder for future extensions.
	 */
	int		width;		/* data bus width */
	uint8_t		msel;		/* machine select */
	uint8_t		atom;		/* atomic op mode */
	uint8_t		wp;		/* write protect */
	uint8_t		decc;		/* data error checking */
};

struct lbc_softc {
	device_t		sc_dev;
	struct resource		*sc_res;
	bus_space_handle_t	sc_bsh;
	bus_space_tag_t		sc_bst;
	int			sc_rid;

	struct rman		sc_rman;

	int			sc_addr_cells;
	int			sc_size_cells;

	struct lbc_bank		sc_banks[LBC_DEV_MAX];
};

struct lbc_devinfo {
	struct ofw_bus_devinfo	di_ofw;
	struct resource_list	di_res;
	int			di_bank;
};

#endif /* _MACHINE_LBC_H_ */
