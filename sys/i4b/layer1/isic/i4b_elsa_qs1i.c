/*
 * Copyright (c) 1997, 2000 Hellmuth Michaelis. All rights reserved.
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
 *
 *---------------------------------------------------------------------------
 *
 *	isic - I4B Siemens ISDN Chipset Driver for ELSA Quickstep 1000pro ISA
 *	=====================================================================
 *
 *	$Id: i4b_elsa_qs1i.c,v 1.3 2000/05/29 15:41:41 hm Exp $
 *
 * $FreeBSD$
 *
 *      last edit-date: [Fri Oct 13 16:00:15 2000]
 *
 *---------------------------------------------------------------------------*/

#include "isic.h"
#include "opt_i4b.h"

#if (NISIC > 0) && defined(ELSA_QS1ISA)

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <net/if.h>


#include <machine/i4b_ioctl.h>

#include <i4b/layer1/isic/i4b_isic.h>
#include <i4b/layer1/isic/i4b_hscx.h>

static void i4b_eq1i_clrirq(struct l1_softc *sc);

/* masks for register encoded in base addr */

#define ELSA_BASE_MASK		0x0ffff
#define ELSA_OFF_MASK		0xf0000

/* register id's to be encoded in base addr */

#define ELSA_IDISAC		0x00000
#define ELSA_IDHSCXA		0x10000
#define ELSA_IDHSCXB		0x20000

/* offsets from base address */

#define ELSA_OFF_ISAC		0x00
#define ELSA_OFF_HSCX		0x02
#define ELSA_OFF_OFF		0x03
#define ELSA_OFF_CTRL		0x04
#define ELSA_OFF_CFG		0x05
#define ELSA_OFF_TIMR		0x06
#define ELSA_OFF_IRQ		0x07

/* control register (write access) */

#define ELSA_CTRL_LED_YELLOW	0x02
#define ELSA_CTRL_LED_GREEN	0x08
#define ELSA_CTRL_RESET		0x20
#define ELSA_CTRL_TIMEREN	0x80
#define ELSA_CTRL_SECRET	0x50

/*---------------------------------------------------------------------------*
 *      ELSA QuickStep 1000pro/ISA clear IRQ routine
 *---------------------------------------------------------------------------*/
static void
i4b_eq1i_clrirq(struct l1_softc *sc)
{
	bus_space_tag_t    t = rman_get_bustag(sc->sc_resources.io_base[0]);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.io_base[0]);
	bus_space_write_1(t, h, ELSA_OFF_IRQ, 0);
}

/*---------------------------------------------------------------------------*
 *      ELSA QuickStep 1000pro/ISA ISAC get fifo routine
 *---------------------------------------------------------------------------*/
static void
eqs1pi_read_fifo(struct l1_softc *sc, int what, void *buf, size_t size)
{
	bus_space_tag_t    t = rman_get_bustag(sc->sc_resources.io_base[0]);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.io_base[0]);

	switch (what) {
		case ISIC_WHAT_ISAC:
			bus_space_write_1(t, h, ELSA_OFF_OFF, 0);
			bus_space_read_multi_1(t, h, ELSA_OFF_ISAC, buf, size);
			break;
		case ISIC_WHAT_HSCXA:
			bus_space_write_1(t, h, ELSA_OFF_OFF, 0);
			bus_space_read_multi_1(t, h, ELSA_OFF_HSCX, buf, size);
			break;
		case ISIC_WHAT_HSCXB:
			bus_space_write_1(t, h, ELSA_OFF_OFF, 0x40);
			bus_space_read_multi_1(t, h, ELSA_OFF_HSCX, buf, size);
			break;
	}
}

/*---------------------------------------------------------------------------*
 *      ELSA QuickStep 1000pro/ISA ISAC put fifo routine
 *---------------------------------------------------------------------------*/
static void
eqs1pi_write_fifo(struct l1_softc *sc, int what, void *buf, size_t size)
{
	bus_space_tag_t    t = rman_get_bustag(sc->sc_resources.io_base[0]);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.io_base[0]);

	switch (what) {
		case ISIC_WHAT_ISAC:
			bus_space_write_1(t, h, ELSA_OFF_OFF, 0);
			bus_space_write_multi_1(t, h, ELSA_OFF_ISAC, buf, size);
			break;
		case ISIC_WHAT_HSCXA:
			bus_space_write_1(t, h, ELSA_OFF_OFF, 0);
			bus_space_write_multi_1(t, h, ELSA_OFF_HSCX, buf, size);
			break;
		case ISIC_WHAT_HSCXB:
			bus_space_write_1(t, h, ELSA_OFF_OFF, 0x40);
			bus_space_write_multi_1(t, h, ELSA_OFF_HSCX, buf, size);
			break;
	}
}

/*---------------------------------------------------------------------------*
 *      ELSA QuickStep 1000pro/ISA ISAC put register routine
 *---------------------------------------------------------------------------*/
static void
eqs1pi_write_reg(struct l1_softc *sc, int what, bus_size_t offs, u_int8_t data)
{
	bus_space_tag_t    t = rman_get_bustag(sc->sc_resources.io_base[0]);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.io_base[0]);

	switch (what) {
		case ISIC_WHAT_ISAC:
			bus_space_write_1(t, h, ELSA_OFF_OFF, offs);
			bus_space_write_1(t, h, ELSA_OFF_ISAC, data);
			break;
		case ISIC_WHAT_HSCXA:
			bus_space_write_1(t, h, ELSA_OFF_OFF, offs);
			bus_space_write_1(t, h, ELSA_OFF_HSCX, data);
			break;
		case ISIC_WHAT_HSCXB:
			bus_space_write_1(t, h, ELSA_OFF_OFF, 0x40+offs);
			bus_space_write_1(t, h, ELSA_OFF_HSCX, data);
			break;
	}
}

/*---------------------------------------------------------------------------*
 *	ELSA QuickStep 1000pro/ISA ISAC get register routine
 *---------------------------------------------------------------------------*/
static u_int8_t
eqs1pi_read_reg(struct l1_softc *sc, int what, bus_size_t offs)
{
	bus_space_tag_t    t = rman_get_bustag(sc->sc_resources.io_base[0]);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.io_base[0]);

	switch (what) {
		case ISIC_WHAT_ISAC:
			bus_space_write_1(t, h, ELSA_OFF_OFF, offs);
			return bus_space_read_1(t, h, ELSA_OFF_ISAC);
		case ISIC_WHAT_HSCXA:
			bus_space_write_1(t, h, ELSA_OFF_OFF, offs);
			return bus_space_read_1(t, h, ELSA_OFF_HSCX);
		case ISIC_WHAT_HSCXB:
			bus_space_write_1(t, h, ELSA_OFF_OFF, 0x40+offs);
			return bus_space_read_1(t, h, ELSA_OFF_HSCX);
	}
	return 0;
}

/*---------------------------------------------------------------------------*
 * isic_attach_Eqs1pi - attach for ELSA QuickStep 1000pro/ISA
 *---------------------------------------------------------------------------*/
int
isic_attach_Eqs1pi(device_t dev)
{
	int unit = device_get_unit(dev);
	struct l1_softc *sc = &l1_sc[unit];	
	bus_space_tag_t    t = rman_get_bustag(sc->sc_resources.io_base[0]);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.io_base[0]);

	u_char byte = ELSA_CTRL_SECRET;

	/* setup access routines */

	sc->clearirq = i4b_eq1i_clrirq;
	sc->readreg = eqs1pi_read_reg;
	sc->writereg = eqs1pi_write_reg;

	sc->readfifo = eqs1pi_read_fifo;
	sc->writefifo = eqs1pi_write_fifo;

	/* setup card type */
	
	sc->sc_cardtyp = CARD_TYPEP_ELSAQS1ISA;

	/* setup IOM bus type */
	
	sc->sc_bustyp = BUS_TYPE_IOM2;

	sc->sc_ipac = 0;
	sc->sc_bfifolen = HSCX_FIFO_LEN;	

	/* enable the card */
	
	byte &= ~ELSA_CTRL_RESET;
        bus_space_write_1(t, h, ELSA_OFF_CTRL, byte);
        DELAY(20);
	byte |= ELSA_CTRL_RESET;
        bus_space_write_1(t, h, ELSA_OFF_CTRL, byte);

        DELAY(20);
        bus_space_write_1(t, h, ELSA_OFF_IRQ, 0xff);

	return 0;
}
#endif /* (NISIC > 0) && defined(ELSA_QS1ISA) */
