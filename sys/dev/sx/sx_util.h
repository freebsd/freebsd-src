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


/* Utility functions and macros for the Specialix I/O8+ driver. */

/*
 * sx_cd1865_out()
 *	Write a CD1865 register on the card.
 */
static __inline void
sx_cd1865_out(
	struct sx_softc *sc,
	unsigned int reg,
	unsigned char val)
{
	bus_space_write_1(sc->sc_st, sc->sc_sh, SX_ADDR_REG, reg);
	bus_space_write_1(sc->sc_st, sc->sc_sh, SX_DATA_REG, val);
}

/*
 * sx_cd1865_in()
 *	Read a register from the card.
 */
static __inline unsigned char
sx_cd1865_in(
	struct sx_softc *sc,
	unsigned int reg)
{
	bus_space_write_1(sc->sc_st, sc->sc_sh, SX_ADDR_REG, reg);
	return(bus_space_read_1(sc->sc_st, sc->sc_sh, SX_DATA_REG));
}

/*
 * sx_cd1865_bis()
 *	Set bits in a CD1865 register.
 */
static __inline void
sx_cd1865_bis(
	struct sx_softc *sc,
	unsigned int reg,
	unsigned char bits)
{
	register unsigned char rval;

	rval = sx_cd1865_in(sc, reg);
	rval |= bits;
	sx_cd1865_out(sc, reg, rval);
}

/*
 * sx_cd1865_bic()
 *	Clear bits in a CD1865 register.
 */
static __inline void
sx_cd1865_bic(
	struct sx_softc *sc,
	unsigned int reg,
	unsigned char bits)
{
	register unsigned char rval;

	rval = sx_cd1865_in(sc, reg);
	rval &= ~bits;
	sx_cd1865_out(sc, reg, rval);
}

/*
 * sx_cd1865_wait_CCR()
 *	Spin waiting for the board Channel Command Register to clear.
 *
 * Description:
 *	The CD1865 processor clears the Channel Command Register to
 *	indicate that it has completed the last command.  This routine
 *	waits for the CCR to become zero by watching the register,
 *	delaying ten microseconds between each check.  We time out after
 *	ten milliseconds (or SX_CCR_TIMEOUT microseconds).
 */
static __inline void
sx_cd1865_wait_CCR(
	struct sx_softc *sc,
	unsigned int ei_flag)
{
	unsigned int to = SX_CCR_TIMEOUT/10;

	while (to-- > 0) {
		if (sx_cd1865_in(sc, CD1865_CCR|ei_flag) == 0)
			return;
		DELAY(10);
	}
	printf("sx: Timeout waiting for CCR to clear.\n");
}

/*
 * sx_cd1865_etcmode()
 *	Set or clear embedded transmit command mode on a CD1865 port.
 *
 * Description:
 *	We can use commands embedded in the transmit data stream to do
 *	things like start and stop breaks or insert time delays.  We normally
 *	run with embedded commands disabled; this routine selects the channel
 *	we're dealing with and enables or disables embedded commands depending
 *	on the flag passed to it.  The caller must remember this state and
 *	escape any NULs it sends while embedded commands are enabled.
 *	Should be called at spltty().  Disables interrupts for the duration
 *	of the routine.
 */
static __inline void
sx_cd1865_etcmode(
	struct sx_softc *sc,
	unsigned int ei_flag,
	int chan,
	int mode)
{
	sx_cd1865_out(sc, CD1865_CAR|ei_flag, chan); /* Select channel.       */
	if (mode) {			/* Enable embedded commands?          */
		sx_cd1865_bis(sc, CD1865_COR2|ei_flag, CD1865_COR2_ETC);
	}
	else {
		sx_cd1865_bic(sc, CD1865_COR2|ei_flag, CD1865_COR2_ETC);
	}
	/*
	 * Wait for the CCR to clear, ding the card, let it know stuff
	 * changed, then wait for CCR to clear again.
	 */
	sx_cd1865_wait_CCR(sc, ei_flag);
	sx_cd1865_out(sc, CD1865_CCR|ei_flag, CD1865_CCR_CORCHG2);
	sx_cd1865_wait_CCR(sc, ei_flag);
}

int sx_probe_io8(device_t dev);
int sx_init_cd1865(struct sx_softc *sc, int unit);
struct sx_port *sx_int_port(struct sx_softc *sc, int unit);
