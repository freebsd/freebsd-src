/*-
 * Copyright (c) 2001 Cubical Solutions Ltd. All rights reserved.
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

/* capi/iavc/iavc_card.c
 *		The AVM ISDN controllers' card specific support routines.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/i4b/capi/iavc/iavc_card.c,v 1.7 2007/07/06 07:17:17 bz Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <net/if.h>


#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <i4b/include/i4b_debug.h>
#include <i4b/include/i4b_ioctl.h>
#include <i4b/include/i4b_trace.h>

#include <i4b/include/i4b_global.h>
#include <i4b/include/i4b_l3l4.h>
#include <i4b/include/i4b_mbuf.h>

#include <i4b/capi/capi.h>

#include <i4b/capi/iavc/iavc.h>

/*
//  AVM B1 (active BRI, PIO mode)
*/

int b1_detect(iavc_softc_t *sc)
{
    if ((iavc_read_port(sc, B1_INSTAT) & 0xfc) ||
	(iavc_read_port(sc, B1_OUTSTAT) & 0xfc))
	return (1);

    b1io_outp(sc, B1_INSTAT, 0x02);
    b1io_outp(sc, B1_OUTSTAT, 0x02);
    if ((iavc_read_port(sc, B1_INSTAT) & 0xfe) != 2 ||
	(iavc_read_port(sc, B1_OUTSTAT) & 0xfe) != 2)
	return (2);

    b1io_outp(sc, B1_INSTAT, 0x00);
    b1io_outp(sc, B1_OUTSTAT, 0x00);
    if ((iavc_read_port(sc, B1_INSTAT) & 0xfe) ||
	(iavc_read_port(sc, B1_OUTSTAT) & 0xfe))
	return (3);

    return (0); /* found */
}

void b1_disable_irq(iavc_softc_t *sc)
{
    b1io_outp(sc, B1_INSTAT, 0x00);
}

void b1_reset(iavc_softc_t *sc)
{
    b1io_outp(sc, B1_RESET, 0);
    DELAY(55*2*1000);

    b1io_outp(sc, B1_RESET, 1);
    DELAY(55*2*1000);

    b1io_outp(sc, B1_RESET, 0);
    DELAY(55*2*1000);
}

/*
//  Newer PCI-based B1's, and T1's, supports DMA
*/

int b1dma_detect(iavc_softc_t *sc)
{
    AMCC_WRITE(sc, AMCC_MCSR, 0);
    DELAY(10*1000);
    AMCC_WRITE(sc, AMCC_MCSR, 0x0f000000);
    DELAY(10*1000);
    AMCC_WRITE(sc, AMCC_MCSR, 0);
    DELAY(42*1000);

    AMCC_WRITE(sc, AMCC_RXLEN, 0);
    AMCC_WRITE(sc, AMCC_TXLEN, 0);
    sc->sc_csr = 0;
    AMCC_WRITE(sc, AMCC_INTCSR, sc->sc_csr);

    if (AMCC_READ(sc, AMCC_INTCSR) != 0)
	return 1;

    AMCC_WRITE(sc, AMCC_RXPTR, 0xffffffff);
    AMCC_WRITE(sc, AMCC_TXPTR, 0xffffffff);
    if ((AMCC_READ(sc, AMCC_RXPTR) != 0xfffffffc) ||
	(AMCC_READ(sc, AMCC_TXPTR) != 0xfffffffc))
	return 2;

    AMCC_WRITE(sc, AMCC_RXPTR, 0);
    AMCC_WRITE(sc, AMCC_TXPTR, 0);
    if ((AMCC_READ(sc, AMCC_RXPTR) != 0) ||
	(AMCC_READ(sc, AMCC_TXPTR) != 0))
	return 3;

    iavc_write_port(sc, 0x10, 0x00);
    iavc_write_port(sc, 0x07, 0x00);

    iavc_write_port(sc, 0x02, 0x02);
    iavc_write_port(sc, 0x03, 0x02);

    if (((iavc_read_port(sc, 0x02) & 0xfe) != 0x02) ||
	(iavc_read_port(sc, 0x03) != 0x03))
	return 4;

    iavc_write_port(sc, 0x02, 0x00);
    iavc_write_port(sc, 0x03, 0x00);

    if (((iavc_read_port(sc, 0x02) & 0xfe) != 0x00) ||
	(iavc_read_port(sc, 0x03) != 0x01))
	return 5;

    return (0); /* found */
}

void b1dma_reset(iavc_softc_t *sc)
{
    int s = SPLI4B();

    sc->sc_csr = 0;
    AMCC_WRITE(sc, AMCC_INTCSR, sc->sc_csr);
    AMCC_WRITE(sc, AMCC_MCSR, 0);
    AMCC_WRITE(sc, AMCC_RXLEN, 0);
    AMCC_WRITE(sc, AMCC_TXLEN, 0);

    iavc_write_port(sc, 0x10, 0x00); /* XXX magic numbers from */
    iavc_write_port(sc, 0x07, 0x00); /* XXX the linux driver */

    splx(s);

    AMCC_WRITE(sc, AMCC_MCSR, 0);
    DELAY(10 * 1000);
    AMCC_WRITE(sc, AMCC_MCSR, 0x0f000000);
    DELAY(10 * 1000);
    AMCC_WRITE(sc, AMCC_MCSR, 0);
    DELAY(42 * 1000);
}

/*
//  AVM T1 (active PRI)
*/

/* XXX how do these differ from b1io_{read,write}_reg()? XXX */

static int b1dma_tx_empty(int iobase)
{ return inb(iobase + 3) & 1; }

static int b1dma_rx_full(int iobase)
{ return inb(iobase + 2) & 1; }

static int b1dma_tolink(iavc_softc_t *sc, void *buf, int len)
{
    volatile int spin;
    char *s = (char*) buf;
    while (len--) {
	spin = 0;
	while (!b1dma_tx_empty(sc->sc_iobase) && spin < 100000)
	    spin++;
	if (!b1dma_tx_empty(sc->sc_iobase))
	    return -1;
	t1io_outp(sc, 1, *s++);
    }
    return 0;
}

static int b1dma_fromlink(iavc_softc_t *sc, void *buf, int len)
{
    volatile int spin;
    char *s = (char*) buf;
    while (len--) {
	spin = 0;
	while (!b1dma_rx_full(sc->sc_iobase) && spin < 100000)
	    spin++;
	if (!b1dma_rx_full(sc->sc_iobase))
	    return -1;
	*s++ = t1io_inp(sc, 0);
    }
    return 0;
}

static int WriteReg(iavc_softc_t *sc, u_int32_t reg, u_int8_t val)
{
    u_int8_t cmd = 0;
    if (b1dma_tolink(sc, &cmd, 1) == 0 &&
	b1dma_tolink(sc, &reg, 4) == 0) {
	u_int32_t tmp = val;
	return b1dma_tolink(sc, &tmp, 4);
    }
    return -1;
}

static u_int8_t ReadReg(iavc_softc_t *sc, u_int32_t reg)
{
    u_int8_t cmd = 1;
    if (b1dma_tolink(sc, &cmd, 1) == 0 &&
	b1dma_tolink(sc, &reg, 4) == 0) {
	u_int32_t tmp;
	if (b1dma_fromlink(sc, &tmp, 4) == 0)
	    return (u_int8_t) tmp;
    }
    return 0xff;
}

int t1_detect(iavc_softc_t *sc)
{
    int ret = b1dma_detect(sc);
    if (ret) return ret;

    if ((WriteReg(sc, 0x80001000, 0x11) != 0) ||
	(WriteReg(sc, 0x80101000, 0x22) != 0) ||
	(WriteReg(sc, 0x80201000, 0x33) != 0) ||
	(WriteReg(sc, 0x80301000, 0x44) != 0))
	return 6;

    if ((ReadReg(sc, 0x80001000) != 0x11) ||
	(ReadReg(sc, 0x80101000) != 0x22) ||
	(ReadReg(sc, 0x80201000) != 0x33) ||
	(ReadReg(sc, 0x80301000) != 0x44))
	return 7;

    if ((WriteReg(sc, 0x80001000, 0x55) != 0) ||
	(WriteReg(sc, 0x80101000, 0x66) != 0) ||
	(WriteReg(sc, 0x80201000, 0x77) != 0) ||
	(WriteReg(sc, 0x80301000, 0x88) != 0))
	return 8;

    if ((ReadReg(sc, 0x80001000) != 0x55) ||
	(ReadReg(sc, 0x80101000) != 0x66) ||
	(ReadReg(sc, 0x80201000) != 0x77) ||
	(ReadReg(sc, 0x80301000) != 0x88))
	return 9;

    return 0; /* found */
}

void t1_disable_irq(iavc_softc_t *sc)
{
    iavc_write_port(sc, T1_IRQMASTER, 0x00);
}

void t1_reset(iavc_softc_t *sc)
{
    b1_reset(sc);
    iavc_write_port(sc, B1_INSTAT, 0x00);
    iavc_write_port(sc, B1_OUTSTAT, 0x00);
    iavc_write_port(sc, T1_IRQMASTER, 0x00);
    iavc_write_port(sc, T1_RESETBOARD, 0x0f);
}
