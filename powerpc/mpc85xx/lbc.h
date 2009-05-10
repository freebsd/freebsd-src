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

#define	LBC_IVAR_DEVTYPE	1

/* Maximum number of devices on Local Bus */
#define	LBC_DEV_MAX	8

/* Device types. */
#define	LBC_DEVTYPE_CFI		1
#define	LBC_DEVTYPE_RTC		2

/* Local access registers */
#define	LBC85XX_BR(n)	(OCP85XX_LBC_OFF + (8 * n))
#define	LBC85XX_OR(n)	(OCP85XX_LBC_OFF + 4 + (8 * n))
#define	LBC85XX_LBCR	(OCP85XX_LBC_OFF + 0xd0)
#define	LBC85XX_LCRR	(OCP85XX_LBC_OFF + 0xd4)

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

struct lbc_resource {
	int		lbr_devtype;	/* LBC device type */
	int		lbr_unit;	/* Resource table entry number */
	vm_paddr_t	lbr_base_addr;	/* Device mem region base address */
	size_t		lbr_size;	/* Device mem region size */
	int		lbr_port_size;	/* Data bus width */
	uint8_t		lbr_msel;	/* LBC machine select */
	uint8_t		lbr_decc;	/* Data error checking mode */
	uint8_t		lbr_atom;	/* Atomic operation mode */
	uint8_t		lbr_wp;		/* Write protect */
};

extern const struct lbc_resource mpc85xx_lbc_resources[];

#endif /* _MACHINE_LBC_H_ */
