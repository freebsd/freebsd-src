/*-
 * Device driver for Specialix I/O8+ multiport serial card.
 *
 * Copyright 2003 Frank Mayhar <frank@exit.com>
 *
 * Derived from the "si" driver by Peter Wemm <peter@netplex.com.au>, using
 * lots of information from the Linux "specialix" driver by Roger Wolff
 * <R.E.Wolff@BitWizard.nl> and from the Intel CD1865 "Intelligent Eight-
 * Channel Communications Controller" datasheet.  Roger was also nice
 * enough to answer numerous questions about stuff specific to the I/O8+
 * not covered by the CD1865 datasheet.
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


int	sxattach(device_t dev);
void	sx_intr(void *);

extern devclass_t sx_devclass;

struct sx_softc {
	struct sx_port	*sc_ports;	/* port structures for this card */
	int		sc_irq;		/* copy of attach irq */
	struct resource *sc_io_res;
	struct resource *sc_irq_res;
	bus_space_tag_t	sc_st;
	bus_space_handle_t sc_sh;
	int		sc_io_rid;
	int		sc_irq_rid;
	int		sc_unit;
};

#ifdef SX_DEBUG
/*
 * debugging stuff
 */
void	sx_dprintf(struct sx_port *pp, int flags, const char *fmt, ...);

#define DPRINT(x)	sx_dprintf x

#define	DBG_ENTRY	0x00000001
#define	DBG_DRAIN	0x00000002
#define	DBG_OPEN	0x00000004
#define	DBG_CLOSE	0x00000008
#define	DBG_READ	0x00000010
#define	DBG_WRITE	0x00000020
#define	DBG_PARAM	0x00000040
#define	DBG_INTR	0x00000080
#define	DBG_IOCTL	0x00000100
/*			0x00000200 */
#define	DBG_SELECT	0x00000400
#define	DBG_OPTIM	0x00000800
#define	DBG_START	0x00001000
#define	DBG_EXIT	0x00002000
#define	DBG_FAIL	0x00004000
#define	DBG_STOP	0x00008000
#define	DBG_AUTOBOOT	0x00010000
#define	DBG_MODEM	0x00020000
#define	DBG_DOWNLOAD	0x00040000
/*			0x00080000*/
#define	DBG_POLL	0x00100000
#define	DBG_ALL		0xffffffff

#else

#define DPRINT(x)	/* void */

#endif
