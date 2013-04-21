/*-
 * Copyright (c) 2005 M. Warner Losh
 * Copyright (c) 2005 Olivier Houchard
 * Copyright (c) 2012 Ian Lepore
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/cons.h>
#include <sys/tty.h>
#include <machine/bus.h>

#include <dev/uart/uart.h>
#include <dev/uart/uart_cpu.h>
#include <dev/uart/uart_bus.h>
#include <arm/at91/at91_usartreg.h>
#include <arm/at91/at91_pdcreg.h>
#include <arm/at91/at91_piovar.h>
#include <arm/at91/at91_pioreg.h>
#include <arm/at91/at91rm92reg.h>
#include <arm/at91/at91var.h>

#include "uart_if.h"

#define	DEFAULT_RCLK			at91_master_clock
#define	USART_DEFAULT_FIFO_BYTES	128

#define	USART_DCE_CHANGE_BITS	(USART_CSR_CTSIC | USART_CSR_DCDIC | \
				 USART_CSR_DSRIC | USART_CSR_RIIC)

/*
 * High-level UART interface.
 */
struct at91_usart_rx {
	bus_addr_t	pa;
	uint8_t		*buffer;
	bus_dmamap_t	map;
};

struct at91_usart_softc {
	struct uart_softc base;
	bus_dma_tag_t tx_tag;
	bus_dmamap_t tx_map;
	uint32_t flags;
#define	HAS_TIMEOUT		0x1
#define	USE_RTS0_WORKAROUND	0x2
	bus_dma_tag_t rx_tag;
	struct at91_usart_rx ping_pong[2];
	struct at91_usart_rx *ping;
	struct at91_usart_rx *pong;
};

#define	RD4(bas, reg)		\
	bus_space_read_4((bas)->bst, (bas)->bsh, uart_regofs(bas, reg))
#define	WR4(bas, reg, value)	\
	bus_space_write_4((bas)->bst, (bas)->bsh, uart_regofs(bas, reg), value)

#define	SIGCHG(c, i, s, d)				\
	do {						\
		if (c) {				\
			i |= (i & s) ? s : s | d;	\
		} else {				\
			i = (i & s) ? (i & ~s) | d : i;	\
		}					\
	} while (0);

#define BAUD2DIVISOR(b) \
	((((DEFAULT_RCLK * 10) / ((b) * 16)) + 5) / 10)

/*
 * Low-level UART interface.
 */
static int at91_usart_probe(struct uart_bas *bas);
static void at91_usart_init(struct uart_bas *bas, int, int, int, int);
static void at91_usart_term(struct uart_bas *bas);
static void at91_usart_putc(struct uart_bas *bas, int);
static int at91_usart_rxready(struct uart_bas *bas);
static int at91_usart_getc(struct uart_bas *bas, struct mtx *hwmtx);

extern SLIST_HEAD(uart_devinfo_list, uart_devinfo) uart_sysdevs;

static int
at91_usart_param(struct uart_bas *bas, int baudrate, int databits,
    int stopbits, int parity)
{
	uint32_t mr;

	/*
	 * Assume 3-wire RS-232 configuration.
	 * XXX Not sure how uart will present the other modes to us, so
	 * XXX they are unimplemented.  maybe ioctl?
	 */
	mr = USART_MR_MODE_NORMAL;
	mr |= USART_MR_USCLKS_MCK;	/* Assume MCK */

	/*
	 * Or in the databits requested
	 */
	if (databits < 9)
		mr &= ~USART_MR_MODE9;
	switch (databits) {
	case 5:
		mr |= USART_MR_CHRL_5BITS;
		break;
	case 6:
		mr |= USART_MR_CHRL_6BITS;
		break;
	case 7:
		mr |= USART_MR_CHRL_7BITS;
		break;
	case 8:
		mr |= USART_MR_CHRL_8BITS;
		break;
	case 9:
		mr |= USART_MR_CHRL_8BITS | USART_MR_MODE9;
		break;
	default:
		return (EINVAL);
	}

	/*
	 * Or in the parity
	 */
	switch (parity) {
	case UART_PARITY_NONE:
		mr |= USART_MR_PAR_NONE;
		break;
	case UART_PARITY_ODD:
		mr |= USART_MR_PAR_ODD;
		break;
	case UART_PARITY_EVEN:
		mr |= USART_MR_PAR_EVEN;
		break;
	case UART_PARITY_MARK:
		mr |= USART_MR_PAR_MARK;
		break;
	case UART_PARITY_SPACE:
		mr |= USART_MR_PAR_SPACE;
		break;
	default:
		return (EINVAL);
	}

	/*
	 * Or in the stop bits.  Note: The hardware supports 1.5 stop
	 * bits in async mode, but there's no way to specify that
	 * AFAICT.  Instead, rely on the convention documented at
	 * http://www.lammertbies.nl/comm/info/RS-232_specs.html which
	 * states that 1.5 stop bits are used for 5 bit bytes and
	 * 2 stop bits only for longer bytes.
	 */
	if (stopbits == 1)
		mr |= USART_MR_NBSTOP_1;
	else if (databits > 5)
		mr |= USART_MR_NBSTOP_2;
	else
		mr |= USART_MR_NBSTOP_1_5;

	/*
	 * We want normal plumbing mode too, none of this fancy
	 * loopback or echo mode.
	 */
	mr |= USART_MR_CHMODE_NORMAL;

	mr &= ~USART_MR_MSBF;	/* lsb first */
	mr &= ~USART_MR_CKLO_SCK;	/* Don't drive SCK */

	WR4(bas, USART_MR, mr);

	/*
	 * Set the baud rate (only if we know our master clock rate)
	 */
	if (DEFAULT_RCLK != 0)
		WR4(bas, USART_BRGR, BAUD2DIVISOR(baudrate));

	/*
	 * Set the receive timeout based on the baud rate.  The idea is to
	 * compromise between being responsive on an interactive connection and
	 * giving a bulk data sender a bit of time to queue up a new buffer
	 * without mistaking it for a stopping point in the transmission.  For
	 * 19.2kbps and below, use 20 * bit time (2 characters).  For faster
	 * connections use 500 microseconds worth of bits.
	 */
	if (baudrate <= 19200)
		WR4(bas, USART_RTOR, 20);
	else 
		WR4(bas, USART_RTOR, baudrate / 2000);
	WR4(bas, USART_CR, USART_CR_STTTO);

	/* XXX Need to take possible synchronous mode into account */
	return (0);
}

static struct uart_ops at91_usart_ops = {
	.probe = at91_usart_probe,
	.init = at91_usart_init,
	.term = at91_usart_term,
	.putc = at91_usart_putc,
	.rxready = at91_usart_rxready,
	.getc = at91_usart_getc,
};

static int
at91_usart_probe(struct uart_bas *bas)
{

	/* We know that this is always here */
	return (0);
}

/*
 * Initialize this device for use as a console.
 */
static void
at91_usart_init(struct uart_bas *bas, int baudrate, int databits, int stopbits,
    int parity)
{

	at91_usart_param(bas, baudrate, databits, stopbits, parity);

	/* Reset the rx and tx buffers and turn on rx and tx */
	WR4(bas, USART_CR, USART_CR_RSTSTA | USART_CR_RSTRX | USART_CR_RSTTX);
	WR4(bas, USART_CR, USART_CR_RXEN | USART_CR_TXEN);
	WR4(bas, USART_IDR, 0xffffffff);
}

/*
 * Free resources now that we're no longer the console.  This appears to
 * be never called, and I'm unsure quite what to do if I am called.
 */
static void
at91_usart_term(struct uart_bas *bas)
{

	/* XXX */
}

/*
 * Put a character of console output (so we do it here polling rather than
 * interrupt driven).
 */
static void
at91_usart_putc(struct uart_bas *bas, int c)
{

	while (!(RD4(bas, USART_CSR) & USART_CSR_TXRDY))
		continue;
	WR4(bas, USART_THR, c);
}

/*
 * Check for a character available.
 */
static int
at91_usart_rxready(struct uart_bas *bas)
{

	return ((RD4(bas, USART_CSR) & USART_CSR_RXRDY) != 0 ? 1 : 0);
}

/*
 * Block waiting for a character.
 */
static int
at91_usart_getc(struct uart_bas *bas, struct mtx *hwmtx)
{
	int c;

	uart_lock(hwmtx);
	while (!(RD4(bas, USART_CSR) & USART_CSR_RXRDY)) {
		uart_unlock(hwmtx);
		DELAY(4);
		uart_lock(hwmtx);
	}
	c = RD4(bas, USART_RHR) & 0xff;
	uart_unlock(hwmtx);
	return (c);
}

static int at91_usart_bus_probe(struct uart_softc *sc);
static int at91_usart_bus_attach(struct uart_softc *sc);
static int at91_usart_bus_flush(struct uart_softc *, int);
static int at91_usart_bus_getsig(struct uart_softc *);
static int at91_usart_bus_ioctl(struct uart_softc *, int, intptr_t);
static int at91_usart_bus_ipend(struct uart_softc *);
static int at91_usart_bus_param(struct uart_softc *, int, int, int, int);
static int at91_usart_bus_receive(struct uart_softc *);
static int at91_usart_bus_setsig(struct uart_softc *, int);
static int at91_usart_bus_transmit(struct uart_softc *);

static kobj_method_t at91_usart_methods[] = {
	KOBJMETHOD(uart_probe,		at91_usart_bus_probe),
	KOBJMETHOD(uart_attach,		at91_usart_bus_attach),
	KOBJMETHOD(uart_flush,		at91_usart_bus_flush),
	KOBJMETHOD(uart_getsig,		at91_usart_bus_getsig),
	KOBJMETHOD(uart_ioctl,		at91_usart_bus_ioctl),
	KOBJMETHOD(uart_ipend,		at91_usart_bus_ipend),
	KOBJMETHOD(uart_param,		at91_usart_bus_param),
	KOBJMETHOD(uart_receive,	at91_usart_bus_receive),
	KOBJMETHOD(uart_setsig,		at91_usart_bus_setsig),
	KOBJMETHOD(uart_transmit,	at91_usart_bus_transmit),

	KOBJMETHOD_END
};

int
at91_usart_bus_probe(struct uart_softc *sc)
{
	int value;

	value = USART_DEFAULT_FIFO_BYTES;
	resource_int_value(device_get_name(sc->sc_dev), 
	    device_get_unit(sc->sc_dev), "fifo_bytes", &value);
	value = roundup2(value, arm_dcache_align);
	sc->sc_txfifosz = value;
	sc->sc_rxfifosz = value;
	sc->sc_hwiflow = 0;
	return (0);
}

static void
at91_getaddr(void *arg, bus_dma_segment_t *segs, int nsegs, int error)
{

	if (error != 0)
		return;
	*(bus_addr_t *)arg = segs[0].ds_addr;
}

static int
at91_usart_requires_rts0_workaround(struct uart_softc *sc)
{
	int value;
	int unit;

	unit = device_get_unit(sc->sc_dev);

	/*
	 * On the rm9200 chips, the PA21/RTS0 pin is not correctly wired to the
	 * usart device interally (so-called 'erratum 39', but it's 41.14 in rev
	 * I of the manual).  This prevents use of the hardware flow control
	 * feature in the usart itself.  It also means that if we are to
	 * implement RTS/CTS flow via the tty layer logic, we must use pin PA21
	 * as a gpio and manually manipulate it in at91_usart_bus_setsig().  We
	 * can only safely do so if we've been given permission via a hint,
	 * otherwise we might manipulate a pin that's attached to who-knows-what
	 * and Bad Things could happen.
	 */
	if (at91_is_rm92() && unit == 1) {
		value = 0;
		resource_int_value(device_get_name(sc->sc_dev), unit,
		    "use_rts0_workaround", &value);
		if (value != 0) {
			at91_pio_use_gpio(AT91RM92_PIOA_BASE, AT91C_PIO_PA21);
			at91_pio_gpio_output(AT91RM92_PIOA_BASE, 
			    AT91C_PIO_PA21, 1);
			at91_pio_use_periph_a(AT91RM92_PIOA_BASE, 
			    AT91C_PIO_PA20, 0);
			return (1);
		}
	}
	return (0);
}

static int
at91_usart_bus_attach(struct uart_softc *sc)
{
	int err;
	int i;
	uint32_t cr;
	struct at91_usart_softc *atsc;

	atsc = (struct at91_usart_softc *)sc;

	if (at91_usart_requires_rts0_workaround(sc))
		atsc->flags |= USE_RTS0_WORKAROUND;

	/*
	 * See if we have a TIMEOUT bit.  We disable all interrupts as
	 * a side effect.  Boot loaders may have enabled them.  Since
	 * a TIMEOUT interrupt can't happen without other setup, the
	 * apparent race here can't actually happen.
	 */
	WR4(&sc->sc_bas, USART_IDR, 0xffffffff);
	WR4(&sc->sc_bas, USART_IER, USART_CSR_TIMEOUT);
	if (RD4(&sc->sc_bas, USART_IMR) & USART_CSR_TIMEOUT)
		atsc->flags |= HAS_TIMEOUT;
	WR4(&sc->sc_bas, USART_IDR, 0xffffffff);

	/*
	 * Allocate transmit DMA tag and map.  We allow a transmit buffer
	 * to be any size, but it must map to a single contiguous physical
	 * extent.
	 */
	err = bus_dma_tag_create(bus_get_dma_tag(sc->sc_dev), 1, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL,
	    BUS_SPACE_MAXSIZE_32BIT, 1, BUS_SPACE_MAXSIZE_32BIT, 0, NULL,
	    NULL, &atsc->tx_tag);
	if (err != 0)
		goto errout;
	err = bus_dmamap_create(atsc->tx_tag, 0, &atsc->tx_map);
	if (err != 0)
		goto errout;

	if (atsc->flags & HAS_TIMEOUT) {
		/*
		 * Allocate receive DMA tags, maps, and buffers.
		 * The receive buffers should be aligned to arm_dcache_align,
		 * otherwise partial cache line flushes on every receive
		 * interrupt are pretty much guaranteed.
		 */
		err = bus_dma_tag_create(bus_get_dma_tag(sc->sc_dev),
		    arm_dcache_align, 0, BUS_SPACE_MAXADDR_32BIT,
		    BUS_SPACE_MAXADDR, NULL, NULL, sc->sc_rxfifosz, 1,
		    sc->sc_rxfifosz, BUS_DMA_ALLOCNOW, NULL, NULL,
		    &atsc->rx_tag);
		if (err != 0)
			goto errout;
		for (i = 0; i < 2; i++) {
			err = bus_dmamem_alloc(atsc->rx_tag,
			    (void **)&atsc->ping_pong[i].buffer,
			    BUS_DMA_NOWAIT, &atsc->ping_pong[i].map);
			if (err != 0)
				goto errout;
			err = bus_dmamap_load(atsc->rx_tag,
			    atsc->ping_pong[i].map,
			    atsc->ping_pong[i].buffer, sc->sc_rxfifosz,
			    at91_getaddr, &atsc->ping_pong[i].pa, 0);
			if (err != 0)
				goto errout;
			bus_dmamap_sync(atsc->rx_tag, atsc->ping_pong[i].map,
			    BUS_DMASYNC_PREREAD);
		}
		atsc->ping = &atsc->ping_pong[0];
		atsc->pong = &atsc->ping_pong[1];
	}

	/* Turn on rx and tx */
	cr = USART_CR_RSTSTA | USART_CR_RSTRX | USART_CR_RSTTX;
	WR4(&sc->sc_bas, USART_CR, cr);
	WR4(&sc->sc_bas, USART_CR, USART_CR_RXEN | USART_CR_TXEN);

	/*
	 * Setup the PDC to receive data.  We use the ping-pong buffers
	 * so that we can more easily bounce between the two and so that
	 * we get an interrupt 1/2 way through the software 'fifo' we have
	 * to avoid overruns.
	 */
	if (atsc->flags & HAS_TIMEOUT) {
		WR4(&sc->sc_bas, PDC_RPR, atsc->ping->pa);
		WR4(&sc->sc_bas, PDC_RCR, sc->sc_rxfifosz);
		WR4(&sc->sc_bas, PDC_RNPR, atsc->pong->pa);
		WR4(&sc->sc_bas, PDC_RNCR, sc->sc_rxfifosz);
		WR4(&sc->sc_bas, PDC_PTCR, PDC_PTCR_RXTEN);

		/*
		 * Set the receive timeout to be 1.5 character times
		 * assuming 8N1.
		 */
		WR4(&sc->sc_bas, USART_RTOR, 15);
		WR4(&sc->sc_bas, USART_CR, USART_CR_STTTO);
		WR4(&sc->sc_bas, USART_IER, USART_CSR_TIMEOUT |
		    USART_CSR_RXBUFF | USART_CSR_ENDRX);
	} else {
		WR4(&sc->sc_bas, USART_IER, USART_CSR_RXRDY);
	}
	WR4(&sc->sc_bas, USART_IER, USART_CSR_RXBRK | USART_DCE_CHANGE_BITS);

	/* Prime sc->hwsig with the initial hw line states. */
	at91_usart_bus_getsig(sc);

errout:
	return (err);
}

static int
at91_usart_bus_transmit(struct uart_softc *sc)
{
	bus_addr_t addr;
	struct at91_usart_softc *atsc;
	int err;

	err = 0;
	atsc = (struct at91_usart_softc *)sc;
	uart_lock(sc->sc_hwmtx);
	if (bus_dmamap_load(atsc->tx_tag, atsc->tx_map, sc->sc_txbuf,
	    sc->sc_txdatasz, at91_getaddr, &addr, 0) != 0) {
		err = EAGAIN;
		goto errout;
	}
	bus_dmamap_sync(atsc->tx_tag, atsc->tx_map, BUS_DMASYNC_PREWRITE);
	sc->sc_txbusy = 1;
	/*
	 * Setup the PDC to transfer the data and interrupt us when it
	 * is done.  We've already requested the interrupt.
	 */
	WR4(&sc->sc_bas, PDC_TPR, addr);
	WR4(&sc->sc_bas, PDC_TCR, sc->sc_txdatasz);
	WR4(&sc->sc_bas, PDC_PTCR, PDC_PTCR_TXTEN);
	WR4(&sc->sc_bas, USART_IER, USART_CSR_ENDTX);
errout:
	uart_unlock(sc->sc_hwmtx);
	return (err);
}

static int
at91_usart_bus_setsig(struct uart_softc *sc, int sig)
{
	uint32_t new, old, cr;
	struct at91_usart_softc *atsc;

	atsc = (struct at91_usart_softc *)sc;

	do {
		old = sc->sc_hwsig;
		new = old;
		if (sig & SER_DDTR)
			SIGCHG(sig & SER_DTR, new, SER_DTR, SER_DDTR);
		if (sig & SER_DRTS)
			SIGCHG(sig & SER_RTS, new, SER_RTS, SER_DRTS);
	} while (!atomic_cmpset_32(&sc->sc_hwsig, old, new));

	cr = 0;
	if (new & SER_DTR)
		cr |= USART_CR_DTREN;
	else
		cr |= USART_CR_DTRDIS;
	if (new & SER_RTS)
		cr |= USART_CR_RTSEN;
	else
		cr |= USART_CR_RTSDIS;

	uart_lock(sc->sc_hwmtx);
	WR4(&sc->sc_bas, USART_CR, cr);
	if (atsc->flags & USE_RTS0_WORKAROUND) {
		/* Signal is active-low. */
		if (new & SER_RTS)
			at91_pio_gpio_clear(AT91RM92_PIOA_BASE, AT91C_PIO_PA21);
		else
			at91_pio_gpio_set(AT91RM92_PIOA_BASE,AT91C_PIO_PA21);
	}
	uart_unlock(sc->sc_hwmtx);

	return (0);
}

static int
at91_usart_bus_receive(struct uart_softc *sc)
{

	return (0);
}

static int
at91_usart_bus_param(struct uart_softc *sc, int baudrate, int databits,
    int stopbits, int parity)
{

	return (at91_usart_param(&sc->sc_bas, baudrate, databits, stopbits,
	    parity));
}

static __inline void
at91_rx_put(struct uart_softc *sc, int key)
{

#if defined(KDB)
	if (sc->sc_sysdev != NULL && sc->sc_sysdev->type == UART_DEV_CONSOLE)
		kdb_alt_break(key, &sc->sc_altbrk);
#endif
	uart_rx_put(sc, key);
}

static int
at91_usart_bus_ipend(struct uart_softc *sc)
{
	struct at91_usart_softc *atsc;
	struct at91_usart_rx *p;
	int i, ipend, len;
	uint32_t csr;

	ipend = 0;
	atsc = (struct at91_usart_softc *)sc;
	uart_lock(sc->sc_hwmtx);
	csr = RD4(&sc->sc_bas, USART_CSR);

	if (csr & USART_CSR_OVRE) {
		WR4(&sc->sc_bas, USART_CR, USART_CR_RSTSTA);
		ipend |= SER_INT_OVERRUN;
	}

	if (csr & USART_DCE_CHANGE_BITS)
		ipend |= SER_INT_SIGCHG;

	if (csr & USART_CSR_ENDTX) {
		bus_dmamap_sync(atsc->tx_tag, atsc->tx_map,
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(atsc->tx_tag, atsc->tx_map);
	}
	if (csr & (USART_CSR_TXRDY | USART_CSR_ENDTX)) {
		if (sc->sc_txbusy)
			ipend |= SER_INT_TXIDLE;
		WR4(&sc->sc_bas, USART_IDR, csr & (USART_CSR_TXRDY |
		    USART_CSR_ENDTX));
	}

	/*
	 * Due to the contraints of the DMA engine present in the
	 * atmel chip, I can't just say I have a rx interrupt pending
	 * and do all the work elsewhere.  I need to look at the CSR
	 * bits right now and do things based on them to avoid races.
	 */
	if (atsc->flags & HAS_TIMEOUT) {
		if (csr & USART_CSR_RXBUFF) {
			/*
			 * We have a buffer overflow.  Consume data from ping
			 * and give it back to the hardware before worrying
			 * about pong, to minimze data loss.  Insert an overrun
			 * marker after the contents of the pong buffer.
			 */
			WR4(&sc->sc_bas, PDC_PTCR, PDC_PTCR_RXTDIS);
			bus_dmamap_sync(atsc->rx_tag, atsc->ping->map,
			    BUS_DMASYNC_POSTREAD);
			for (i = 0; i < sc->sc_rxfifosz; i++)
				at91_rx_put(sc, atsc->ping->buffer[i]);
			bus_dmamap_sync(atsc->rx_tag, atsc->ping->map,
			    BUS_DMASYNC_PREREAD);
			WR4(&sc->sc_bas, PDC_RPR, atsc->ping->pa);
			WR4(&sc->sc_bas, PDC_RCR, sc->sc_rxfifosz);
			WR4(&sc->sc_bas, PDC_PTCR, PDC_PTCR_RXTEN);
			bus_dmamap_sync(atsc->rx_tag, atsc->pong->map,
			    BUS_DMASYNC_POSTREAD);
			for (i = 0; i < sc->sc_rxfifosz; i++)
				at91_rx_put(sc, atsc->pong->buffer[i]);
			uart_rx_put(sc, UART_STAT_OVERRUN);
			bus_dmamap_sync(atsc->rx_tag, atsc->pong->map,
			    BUS_DMASYNC_PREREAD);
			WR4(&sc->sc_bas, PDC_RNPR, atsc->pong->pa);
			WR4(&sc->sc_bas, PDC_RNCR, sc->sc_rxfifosz);
			ipend |= SER_INT_RXREADY;
		} else if (csr & USART_CSR_ENDRX) {
			/*
			 * Consume data from ping of ping pong buffer, but leave
			 * current pong in place, as it has become the new ping.
			 * We need to copy data and setup the old ping as the
			 * new pong when we're done.
			 */
			bus_dmamap_sync(atsc->rx_tag, atsc->ping->map,
			    BUS_DMASYNC_POSTREAD);
			for (i = 0; i < sc->sc_rxfifosz; i++)
				at91_rx_put(sc, atsc->ping->buffer[i]);
			p = atsc->ping;
			atsc->ping = atsc->pong;
			atsc->pong = p;
			bus_dmamap_sync(atsc->rx_tag, atsc->pong->map,
			    BUS_DMASYNC_PREREAD);
			WR4(&sc->sc_bas, PDC_RNPR, atsc->pong->pa);
			WR4(&sc->sc_bas, PDC_RNCR, sc->sc_rxfifosz);
			ipend |= SER_INT_RXREADY;
		} else if (csr & USART_CSR_TIMEOUT) {
			/*
			 * On a timeout, one of the following applies:
			 * 1. Two empty buffers.  The last received byte exactly
			 *    filled a buffer, causing an ENDTX that got
			 *    processed earlier; no new bytes have arrived.
			 * 2. Ping buffer contains some data and pong is empty.
			 *    This should be the most common timeout condition.
			 * 3. Ping buffer is full and pong is now being filled.
			 *    This is exceedingly rare; it can happen only if
			 *    the ping buffer is almost full when a timeout is
			 *    signaled, and then dataflow resumes and the ping
			 *    buffer filled up between the time we read the
			 *    status register above and the point where the
			 *    RXTDIS takes effect here.  Yes, it can happen.
			 * Because dataflow can resume at any time following a
			 * timeout (it may have already resumed before we get
			 * here), it's important to minimize the time the PDC is
			 * disabled -- just long enough to take the ping buffer
			 * out of service (so we can consume it) and install the
			 * pong buffer as the active one.  Note that in case 3
			 * the hardware has already done the ping-pong swap.
			 */
			WR4(&sc->sc_bas, PDC_PTCR, PDC_PTCR_RXTDIS);
			if (RD4(&sc->sc_bas, PDC_RNCR) == 0) {
				len = sc->sc_rxfifosz;
			} else {
				len = sc->sc_rxfifosz - RD4(&sc->sc_bas, PDC_RCR);
				WR4(&sc->sc_bas, PDC_RPR, atsc->pong->pa);
				WR4(&sc->sc_bas, PDC_RCR, sc->sc_rxfifosz);
				WR4(&sc->sc_bas, PDC_RNCR, 0);
			}
			WR4(&sc->sc_bas, USART_CR, USART_CR_STTTO);
			WR4(&sc->sc_bas, PDC_PTCR, PDC_PTCR_RXTEN);
			bus_dmamap_sync(atsc->rx_tag, atsc->ping->map,
			    BUS_DMASYNC_POSTREAD);
			for (i = 0; i < len; i++)
				at91_rx_put(sc, atsc->ping->buffer[i]);
			bus_dmamap_sync(atsc->rx_tag, atsc->ping->map,
			    BUS_DMASYNC_PREREAD);
			p = atsc->ping;
			atsc->ping = atsc->pong;
			atsc->pong = p;
			WR4(&sc->sc_bas, PDC_RNPR, atsc->pong->pa);
			WR4(&sc->sc_bas, PDC_RNCR, sc->sc_rxfifosz);
			ipend |= SER_INT_RXREADY;
		}
	} else if (csr & USART_CSR_RXRDY) {
		/*
		 * We have another charater in a device that doesn't support
		 * timeouts, so we do it one character at a time.
		 */
		at91_rx_put(sc, RD4(&sc->sc_bas, USART_RHR) & 0xff);
		ipend |= SER_INT_RXREADY;
	}

	if (csr & USART_CSR_RXBRK) {
		ipend |= SER_INT_BREAK;
		WR4(&sc->sc_bas, USART_CR, USART_CR_RSTSTA);
	}
	uart_unlock(sc->sc_hwmtx);
	return (ipend);
}

static int
at91_usart_bus_flush(struct uart_softc *sc, int what)
{

	return (0);
}

static int
at91_usart_bus_getsig(struct uart_softc *sc)
{
	uint32_t csr, new, old, sig;

	/*
	 * Note that the atmel channel status register DCE status bits reflect
	 * the electrical state of the lines, not the logical state.  Since they
	 * are logically active-low signals, we invert the tests here.
	 */
	do {
		old = sc->sc_hwsig;
		sig = old;
		csr = RD4(&sc->sc_bas, USART_CSR);
		SIGCHG(!(csr & USART_CSR_DSR), sig, SER_DSR, SER_DDSR);
		SIGCHG(!(csr & USART_CSR_CTS), sig, SER_CTS, SER_DCTS);
		SIGCHG(!(csr & USART_CSR_DCD), sig, SER_DCD, SER_DDCD);
		SIGCHG(!(csr & USART_CSR_RI),  sig, SER_RI,  SER_DRI);
		new = sig & ~SER_MASK_DELTA;
	} while (!atomic_cmpset_32(&sc->sc_hwsig, old, new));

	return (sig);
}

static int
at91_usart_bus_ioctl(struct uart_softc *sc, int request, intptr_t data)
{

	switch (request) {
	case UART_IOCTL_BREAK:
	case UART_IOCTL_IFLOW:
	case UART_IOCTL_OFLOW:
		break;
	case UART_IOCTL_BAUD:
		/* only if we know our master clock rate */
		if (DEFAULT_RCLK != 0)
			WR4(&sc->sc_bas, USART_BRGR,
			    BAUD2DIVISOR(*(int *)data));
		return (0);
	}
	return (EINVAL);
}

struct uart_class at91_usart_class = {
	"at91_usart",
	at91_usart_methods,
	sizeof(struct at91_usart_softc),
	.uc_ops = &at91_usart_ops,
	.uc_range = 8
};
