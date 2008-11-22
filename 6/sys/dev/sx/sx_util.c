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

#include "opt_debug_sx.h"

/* Utility and support routines for the Specialix I/O8+ driver. */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/tty.h>
#include <machine/resource.h>   
#include <machine/bus.h>
#include <machine/clock.h>
#include <sys/rman.h>

#include <dev/sx/cd1865.h>
#include <dev/sx/sxvar.h>
#include <dev/sx/sx.h>
#include <dev/sx/sx_util.h>

/*
 * sx_probe_io8()
 *	Probe the board to verify that it is a Specialix I/O8+.
 *
 * Description:
 *	This is called by sx_pci_attach() (and possibly in the future by
 *	sx_isa_attach()) to verify that the card we're attaching to is
 *	indeed a Specialix I/O8+.  To do this, we check for the Prescaler
 *	Period Register of the CD1865 chip and for the Specialix signature
 *	on the DSR input line of each channel.  These lines, along with the
 *	RTS output lines, are wired down in hardware.
 */
int
sx_probe_io8(
	device_t dev)
{
	struct sx_softc *sc;
	unsigned char val1, val2;
	int i;

	sc = device_get_softc(dev);
	/*
	 * Try to write the Prescaler Period Register, then read it back,
	 * twice.  If this fails, it's not an I/O8+.
	 */
	sx_cd1865_out(sc, CD1865_PPRL, 0x5a);
	DELAY(1);
	val1 = sx_cd1865_in(sc, CD1865_PPRL);

	sx_cd1865_out(sc, CD1865_PPRL, 0xa5);
	DELAY(1);
	val2 = sx_cd1865_in(sc, CD1865_PPRL);

	if ((val1 != 0x5a) || (val2 != 0xa5))
		return(1);

	/*
	 * Check the lines that Specialix uses as board identification.
	 * These are the DSR input and the RTS output, which are wired
	 * down.
	 */
	val1 = 0;
	for (i = 0; i < 8; i++) {
		sx_cd1865_out(sc, CD1865_CAR, i);	/* Select channel.    */
		if (sx_cd1865_in(sc, CD1865_MSVR) & CD1865_MSVR_DSR) /* Set?  */
			val1 |= 1 << i;			/* OR it in.          */
	}
#ifdef notdef
	val2 = 0;
	for (i = 0; i < 8; i++) {
		sx_cd1865_out(sc, CD1865_CAR, i);	/* Select channel.    */
		if (sx_cd1865_in(sc, CD1865_MSVR) & CD1865_MSVR_RTS) /* Set?  */
			val2 |= 1 << i;			/* OR it in.          */
	}
	/*
	 * They managed to switch the bit order between the docs and
	 * the IO8+ card. The new PCI card now conforms to old docs.
	 * They changed the PCI docs to reflect the situation on the
	 * old card.
	 */
	val2 = (bp->flags & SX_BOARD_IS_PCI) ? 0x4d : 0xb2;
#endif /* notdef */
	if (val1 != 0x4d) {
		if (bootverbose)
			device_printf(dev,
				      "Specialix I/O8+ ID 0x4d not found (0x%02x).\n",
				      val1);
		return(1);
	}
	return(0);		/* Probed successfully.                       */
}

/*
 * sx_init_CD1865()
 *	Hard-reset and initialize the I/O8+ CD1865 processor.
 *
 * Description:
 *	This routine does a hard reset of the CD1865 chip and waits for it
 *	to complete.  (The reset should complete after 500us; we wait 1ms
 *	and fail if we time out.)  We then initialize the CD1865 processor.
 */
int
sx_init_cd1865(
	struct sx_softc *sc,
	int unit)
{
	int s;
	unsigned int to;

	s = spltty();
	disable_intr();
	sx_cd1865_out(sc, CD1865_GSVR, 0x00); /* Clear the GSVR.              */
	sx_cd1865_wait_CCR(sc, 0);	/* Wait for the CCR to clear.         */
	sx_cd1865_out(sc, CD1865_CCR, CD1865_CCR_HARDRESET); /* Reset CD1865. */
	enable_intr();
	to = SX_GSVR_TIMEOUT/5;
	while (to-- > 0) {
		if (sx_cd1865_in(sc, CD1865_GSVR) == 0xff)
			break;
		DELAY(5);
	}
	if (to == 0) {
		splx(s);
		printf("sx%d:  Timeout waiting for reset.\n", unit);
		return(EIO);
	}
	/*
	 * The high five bits of the Global Interrupt Vector Register is
	 * used to identify daisy-chained CD1865 chips.  The I/O8+ isn't
	 * daisy chained, but we have to initialize the field anyway.
	 */
	sx_cd1865_out(sc, CD1865_GIVR, SX_CD1865_ID);
	/* Clear the Global Interrupting Channel register. */
	sx_cd1865_out(sc, CD1865_GICR, 0);
	/*
	 * Set the Service Match Registers to the appropriate values.  See
	 * the cd1865.h include file for more information.
	 */
	sx_cd1865_out(sc, CD1865_MSMR, CD1865_ACK_MINT); /* Modem.            */
	sx_cd1865_out(sc, CD1865_TSMR, CD1865_ACK_TINT); /* Transmit.         */
	sx_cd1865_out(sc, CD1865_RSMR, CD1865_ACK_RINT); /* Receive.          */
	/*
	 * Set RegAckEn in the Service Request Configuration Register;
	 * we'll be acknowledging service requests in software, not
	 * hardware.
	 */
	sx_cd1865_bis(sc, CD1865_SRCR, CD1865_SRCR_REGACKEN);
	/*
	 * Set the CD1865 timer tick rate.  The value here is the processor
	 * clock rate (in MHz) divided by the rate in ticks per second.  See
	 * commentary in sx.h.
	 */
	sx_cd1865_out(sc, CD1865_PPRH, SX_CD1865_PRESCALE >> 8);
	sx_cd1865_out(sc, CD1865_PPRL, SX_CD1865_PRESCALE & 0xff);

	splx(s);
	return(0);
}

#ifdef notyet
/*
 * Set the IRQ using the RTS lines that run to the PAL on the board....
 *
 * This is a placeholder for ISA support, if that's ever implemented.  This
 * should _only_ be called from sx_isa_attach().
 */
int
sx_set_irq(
	struct sx_softc *sc,
	int unit,
	int irq)
{
	register int virq;
	register int i, j;

	switch (irq) {
	/* In the same order as in the docs... */
		case 15:
			virq = 0;
			break;
		case 12:
			virq = 1;
			break;
		case 11:
			virq = 2;
			break;
		case 9:
			virq = 3;
			break;
		default:
			printf("sx%d:  Illegal irq %d.\n", unit, irq);
			return(0);
	}
	for (i = 0; i < 2; i++) {
		sx_cd1865_out(sc, CD1865_CAR, i); /* Select channel.          */
		j =  ((virq >> i) & 0x1) ? MSVR_RTS : 0;
		sx_cd1865_out(sc, CD1865_MSVRTS, j);
	}
	return(1);
}

#endif /* notyet */

/*
 * sx_int_port()
 *	Determine the port that interrupted us.
 *
 * Description:
 *	This routine checks the Global Interrupting Channel Register (GICR)
 *	to find the port that caused an interrupt.  It returns a pointer to
 *	the sx_port structure of the interrupting port, or NULL if there was
 *	none.
 *
 * XXX - check type/validity of interrupt?
 */
struct sx_port *
sx_int_port(
	struct sx_softc *sc,
	int unit)
{
	unsigned char chan;
	struct sx_port *pp;
	
	chan = (sx_cd1865_in(sc, CD1865_GSCR2|SX_EI) & CD1865_GICR_CHAN_MASK)
						      >> CD1865_GICR_CHAN_SHIFT;
	DPRINT((NULL, DBG_INTR, "Intr chan %d\n", chan));
	if (chan < CD1865_NUMCHAN) {
		pp = sc->sc_ports + (int)chan;
		return(pp);
	}
	printf("sx%d: False interrupt on port %d.\n", unit, chan);
	return(NULL);
}
