/*
 * Device driver for Specialix range (SI/XIO) of serial line multiplexors.
 *
 * Copyright (C) 2000, Peter Wemm <peter@netplex.com.au>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notices, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notices, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
 * NO EVENT SHALL THE AUTHORS BE LIABLE.
 *
 * $FreeBSD$
 */

int	siattach(device_t dev);
void	si_intr(void *);

extern devclass_t si_devclass;

/* where the firmware lives; defined in si2_z280.c and si3_t225.c */
/* old: si2_z280.c */
extern unsigned char si2_z280_download[];
extern unsigned short si2_z280_downloadaddr;
extern int si2_z280_dsize;
/* new: si3_t225.c */
extern unsigned char si3_t225_download[];
extern unsigned short si3_t225_downloadaddr;
extern int si3_t225_dsize;
extern unsigned char si3_t225_bootstrap[];
extern unsigned short si3_t225_bootloadaddr;
extern int si3_t225_bsize;

struct si_softc {
	int 		sc_type;	/* adapter type */
	char 		*sc_typename;	/* adapter type string */

	struct si_port	*sc_ports;	/* port structures for this card */

	caddr_t		sc_paddr;	/* physical addr of iomem */
	caddr_t		sc_maddr;	/* kvaddr of iomem */
	int		sc_nport;	/* # ports on this card */
	int		sc_irq;		/* copy of attach irq */
	int		sc_iobase;	/* EISA io port address */
	struct resource *sc_port_res;
	struct resource *sc_irq_res;
	struct resource *sc_mem_res;
	int		sc_port_rid;
	int		sc_irq_rid;
	int		sc_mem_rid;
	int		sc_memsize;
};

#ifdef SI_DEBUG
/*
 * debugging stuff - manipulated using siconfig(8)
 */

void	si_dprintf(struct si_port *pp, int flags, const char *fmt, ...);

#define DPRINT(x)	si_dprintf x

#define	DBG_ENTRY		0x00000001
#define	DBG_DRAIN		0x00000002
#define	DBG_OPEN		0x00000004
#define	DBG_CLOSE		0x00000008
#define	DBG_READ		0x00000010
#define	DBG_WRITE		0x00000020
#define	DBG_PARAM		0x00000040
#define	DBG_INTR		0x00000080
#define	DBG_IOCTL		0x00000100
/*				0x00000200 */
#define	DBG_SELECT		0x00000400
#define	DBG_OPTIM		0x00000800
#define	DBG_START		0x00001000
#define	DBG_EXIT		0x00002000
#define	DBG_FAIL		0x00004000
#define	DBG_STOP		0x00008000
#define	DBG_AUTOBOOT		0x00010000
#define	DBG_MODEM		0x00020000
#define	DBG_DOWNLOAD		0x00040000
#define	DBG_LSTART		0x00080000
#define	DBG_POLL		0x00100000
#define	DBG_ALL			0xffffffff

#else
#define DPRINT(x)	/* void */
#endif

/* Adapter types */
#define SIEMPTY		0
#define SIHOST		1
#define SIMCA		2
#define SIHOST2		3
#define SIEISA		4
#define SIPCI		5
#define SIJETPCI	6
#define SIJETISA	7

#define SI_ISJET(x)	(((x) == SIJETPCI) || ((x) == SIJETISA))
