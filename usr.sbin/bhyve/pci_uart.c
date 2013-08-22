/*-
 * Copyright (c) 2012 NetApp, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/select.h>
#include <dev/ic/ns16550.h>

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <termios.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>

#include "bhyverun.h"
#include "pci_emul.h"
#include "mevent.h"

#define	COM1_BASE      	0x3F8
#define COM1_IRQ	4
#define	COM2_BASE      	0x2F8
#define COM2_IRQ	3

#define	DEFAULT_RCLK	1843200
#define	DEFAULT_BAUD	9600

#define	FCR_RX_MASK	0xC0

#define	MCR_OUT1	0x04
#define	MCR_OUT2	0x08

#define	MSR_DELTA_MASK	0x0f

#ifndef REG_SCR
#define REG_SCR		com_scr
#endif

#define	FIFOSZ	16

/*
 * Pick a PCI vid/did of a chip with a single uart at
 * BAR0, that most versions of FreeBSD can understand:
 * Siig CyberSerial 1-port.
 */
#define COM_VENDOR	0x131f
#define COM_DEV		0x2000

static int pci_uart_stdio;	/* stdio in use for i/o */

static int pci_uart_nldevs;	/* number of legacy devices - 2 max */

static struct {
	uint64_t	baddr;
	int		vector;
} pci_uart_lres[] = {
	{ COM1_BASE, COM1_IRQ},
	{ COM2_BASE, COM2_IRQ},
	{ 0, 0 }
};

struct fifo {
	uint8_t	buf[FIFOSZ];
	int	rindex;		/* index to read from */
	int	windex;		/* index to write to */
	int	num;		/* number of characters in the fifo */
	int	size;		/* size of the fifo */
};

struct pci_uart_softc {
	struct pci_devinst *pi;
	pthread_mutex_t mtx;	/* protects all softc elements */
	uint8_t data;		/* Data register (R/W) */
	uint8_t ier;		/* Interrupt enable register (R/W) */
	uint8_t lcr;		/* Line control register (R/W) */
	uint8_t mcr;		/* Modem control register (R/W) */
	uint8_t lsr;		/* Line status register (R/W) */
	uint8_t msr;		/* Modem status register (R/W) */
	uint8_t fcr;		/* FIFO control register (W) */
	uint8_t scr;		/* Scratch register (R/W) */

	uint8_t dll;		/* Baudrate divisor latch LSB */
	uint8_t dlh;		/* Baudrate divisor latch MSB */

	struct fifo rxfifo;

	int	opened;
	int	stdio;
	bool	thre_int_pending;	/* THRE interrupt pending */
};

static void pci_uart_drain(int fd, enum ev_type ev, void *arg);

static struct termios tio_orig, tio_new;	/* I/O Terminals */

static void
ttyclose(void)
{
	tcsetattr(STDIN_FILENO, TCSANOW, &tio_orig);
}

static void
ttyopen(void)
{
	tcgetattr(STDIN_FILENO, &tio_orig);

	cfmakeraw(&tio_new);
	tcsetattr(STDIN_FILENO, TCSANOW, &tio_new);

	atexit(ttyclose);
}

static bool
tty_char_available(void)
{
	fd_set rfds;
	struct timeval tv;

	FD_ZERO(&rfds);
	FD_SET(STDIN_FILENO, &rfds);
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	if (select(STDIN_FILENO + 1, &rfds, NULL, NULL, &tv) > 0 ) {
		return (true);
	} else {
		return (false);
	}
}

static int
ttyread(void)
{
	char rb;

	if (tty_char_available()) {
		read(STDIN_FILENO, &rb, 1);
		return (rb & 0xff);
	} else {
		return (-1);
	}
}

static void
ttywrite(unsigned char wb)
{
	(void) write(STDIN_FILENO, &wb, 1);
}

static void
fifo_reset(struct fifo *fifo, int size)
{
	bzero(fifo, sizeof(struct fifo));
	fifo->size = size;
}

static int
fifo_putchar(struct fifo *fifo, uint8_t ch)
{

	if (fifo->num < fifo->size) {
		fifo->buf[fifo->windex] = ch;
		fifo->windex = (fifo->windex + 1) % fifo->size;
		fifo->num++;
		return (0);
	} else
		return (-1);
}

static int
fifo_getchar(struct fifo *fifo)
{
	int c;

	if (fifo->num > 0) {
		c = fifo->buf[fifo->rindex];
		fifo->rindex = (fifo->rindex + 1) % fifo->size;
		fifo->num--;
		return (c);
	} else
		return (-1);
}

static int
fifo_numchars(struct fifo *fifo)
{

	return (fifo->num);
}

static int
fifo_available(struct fifo *fifo)
{

	return (fifo->num < fifo->size);
}

static void
pci_uart_opentty(struct pci_uart_softc *sc)
{
	struct mevent *mev;

	assert(sc->opened == 0);
	assert(sc->stdio);

	ttyopen();
	mev = mevent_add(STDIN_FILENO, EVF_READ, pci_uart_drain, sc);
	assert(mev);
}

static void
pci_uart_legacy_res(uint64_t *bar, int *ivec)
{
	if (pci_uart_lres[pci_uart_nldevs].baddr != 0) {
		*bar = pci_uart_lres[pci_uart_nldevs].baddr;
		*ivec = pci_uart_lres[pci_uart_nldevs].vector;
		pci_uart_nldevs++;
	} else {
		/* TODO: print warning ? */
		*bar = 0;
		*ivec= -1;
	}
}

/*
 * The IIR returns a prioritized interrupt reason:
 * - receive data available
 * - transmit holding register empty
 * - modem status change
 *
 * Return an interrupt reason if one is available.
 */
static int
pci_uart_intr_reason(struct pci_uart_softc *sc)
{

	if ((sc->lsr & LSR_OE) != 0 && (sc->ier & IER_ERLS) != 0)
		return (IIR_RLS);
	else if (fifo_numchars(&sc->rxfifo) > 0 && (sc->ier & IER_ERXRDY) != 0)
		return (IIR_RXTOUT);
	else if (sc->thre_int_pending && (sc->ier & IER_ETXRDY) != 0)
		return (IIR_TXRDY);
	else if ((sc->msr & MSR_DELTA_MASK) != 0 && (sc->ier & IER_EMSC) != 0)
		return (IIR_MLSC);
	else
		return (IIR_NOPEND);
}

static void
pci_uart_reset(struct pci_uart_softc *sc)
{
	uint16_t divisor;

	divisor = DEFAULT_RCLK / DEFAULT_BAUD / 16;
	sc->dll = divisor;
	sc->dlh = divisor >> 16;

	fifo_reset(&sc->rxfifo, 1);	/* no fifo until enabled by software */
}

/*
 * Toggle the COM port's intr pin depending on whether or not we have an
 * interrupt condition to report to the processor.
 */
static void
pci_uart_toggle_intr(struct pci_uart_softc *sc)
{
	uint8_t intr_reason;

	intr_reason = pci_uart_intr_reason(sc);

	if (intr_reason == IIR_NOPEND)
		pci_lintr_deassert(sc->pi);
	else
		pci_lintr_assert(sc->pi);
}

static void
pci_uart_drain(int fd, enum ev_type ev, void *arg)
{
	struct pci_uart_softc *sc;
	int ch;

	sc = arg;	

	assert(fd == STDIN_FILENO);
	assert(ev == EVF_READ);
	
	/*
	 * This routine is called in the context of the mevent thread
	 * to take out the softc lock to protect against concurrent
	 * access from a vCPU i/o exit
	 */
	pthread_mutex_lock(&sc->mtx);

	if ((sc->mcr & MCR_LOOPBACK) != 0) {
		(void) ttyread();
	} else {
		while (fifo_available(&sc->rxfifo) &&
		       ((ch = ttyread()) != -1)) {
			fifo_putchar(&sc->rxfifo, ch);
		}
		pci_uart_toggle_intr(sc);
	}

	pthread_mutex_unlock(&sc->mtx);
}

static void
pci_uart_write(struct vmctx *ctx, int vcpu, struct pci_devinst *pi,
	       int baridx, uint64_t offset, int size, uint64_t value)
{
        struct pci_uart_softc *sc;
	int fifosz;
	uint8_t msr;

	sc = pi->pi_arg;

	assert(baridx == 0);
	assert(size == 1);

	/* Open terminal */
	if (!sc->opened && sc->stdio) {
		pci_uart_opentty(sc);
		sc->opened = 1;
	}

	pthread_mutex_lock(&sc->mtx);
	
	/*
	 * Take care of the special case DLAB accesses first
	 */
	if ((sc->lcr & LCR_DLAB) != 0) {
		if (offset == REG_DLL) {
			sc->dll = value;
			goto done;
		}
		
		if (offset == REG_DLH) {
			sc->dlh = value;
			goto done;
		}
	}

        switch (offset) {
	case REG_DATA:
		if (sc->mcr & MCR_LOOPBACK) {
			if (fifo_putchar(&sc->rxfifo, value) != 0)
				sc->lsr |= LSR_OE;
		} else if (sc->stdio) {
			ttywrite(value);
		} /* else drop on floor */
		sc->thre_int_pending = true;
		break;
	case REG_IER:
		/*
		 * Apply mask so that bits 4-7 are 0
		 * Also enables bits 0-3 only if they're 1
		 */
		sc->ier = value & 0x0F;
		break;
		case REG_FCR:
			/*
			 * When moving from FIFO and 16450 mode and vice versa,
			 * the FIFO contents are reset.
			 */
			if ((sc->fcr & FCR_ENABLE) ^ (value & FCR_ENABLE)) {
				fifosz = (value & FCR_ENABLE) ? FIFOSZ : 1;
				fifo_reset(&sc->rxfifo, fifosz);
			}

			/*
			 * The FCR_ENABLE bit must be '1' for the programming
			 * of other FCR bits to be effective.
			 */
			if ((value & FCR_ENABLE) == 0) {
				sc->fcr = 0;
			} else {
				if ((value & FCR_RCV_RST) != 0)
					fifo_reset(&sc->rxfifo, FIFOSZ);

				sc->fcr = value &
					 (FCR_ENABLE | FCR_DMA | FCR_RX_MASK);
			}
			break;
		case REG_LCR:
			sc->lcr = value;
			break;
		case REG_MCR:
			/* Apply mask so that bits 5-7 are 0 */
			sc->mcr = value & 0x1F;

			msr = 0;
			if (sc->mcr & MCR_LOOPBACK) {
				/*
				 * In the loopback mode certain bits from the
				 * MCR are reflected back into MSR
				 */
				if (sc->mcr & MCR_RTS)
					msr |= MSR_CTS;
				if (sc->mcr & MCR_DTR)
					msr |= MSR_DSR;
				if (sc->mcr & MCR_OUT1)
					msr |= MSR_RI;
				if (sc->mcr & MCR_OUT2)
					msr |= MSR_DCD;
			}

			/*
			 * Detect if there has been any change between the
			 * previous and the new value of MSR. If there is
			 * then assert the appropriate MSR delta bit.
			 */
			if ((msr & MSR_CTS) ^ (sc->msr & MSR_CTS))
				sc->msr |= MSR_DCTS;
			if ((msr & MSR_DSR) ^ (sc->msr & MSR_DSR))
				sc->msr |= MSR_DDSR;
			if ((msr & MSR_DCD) ^ (sc->msr & MSR_DCD))
				sc->msr |= MSR_DDCD;
			if ((sc->msr & MSR_RI) != 0 && (msr & MSR_RI) == 0)
				sc->msr |= MSR_TERI;

			/*
			 * Update the value of MSR while retaining the delta
			 * bits.
			 */
			sc->msr &= MSR_DELTA_MASK;
			sc->msr |= msr;
			break;
		case REG_LSR:
			/*
			 * Line status register is not meant to be written to
			 * during normal operation.
			 */
			break;
		case REG_MSR:
			/*
			 * As far as I can tell MSR is a read-only register.
			 */
			break;
		case REG_SCR:
			sc->scr = value;
			break;
		default:
			break;
	}

done:
	pci_uart_toggle_intr(sc);
	pthread_mutex_unlock(&sc->mtx);
}

uint64_t
pci_uart_read(struct vmctx *ctx, int vcpu, struct pci_devinst *pi,
	      int baridx, uint64_t offset, int size)
{
	struct pci_uart_softc *sc;
	uint8_t iir, intr_reason;
	uint64_t reg;

	sc = pi->pi_arg;

	assert(baridx == 0);
	assert(size == 1);

	/* Open terminal */
	if (!sc->opened && sc->stdio) {
		pci_uart_opentty(sc);
		sc->opened = 1;
	}

	pthread_mutex_lock(&sc->mtx);

	/*
	 * Take care of the special case DLAB accesses first
	 */
	if ((sc->lcr & LCR_DLAB) != 0) {
		if (offset == REG_DLL) {
			reg = sc->dll;
			goto done;
		}
		
		if (offset == REG_DLH) {
			reg = sc->dlh;
			goto done;
		}
	}

	switch (offset) {
	case REG_DATA:
		reg = fifo_getchar(&sc->rxfifo);
		break;
	case REG_IER:
		reg = sc->ier;
		break;
	case REG_IIR:
		iir = (sc->fcr & FCR_ENABLE) ? IIR_FIFO_MASK : 0;

		intr_reason = pci_uart_intr_reason(sc);
			
		/*
		 * Deal with side effects of reading the IIR register
		 */
		if (intr_reason == IIR_TXRDY)
			sc->thre_int_pending = false;

		iir |= intr_reason;

		reg = iir;
		break;
	case REG_LCR:
		reg = sc->lcr;
		break;
	case REG_MCR:
		reg = sc->mcr;
		break;
	case REG_LSR:
		/* Transmitter is always ready for more data */
		sc->lsr |= LSR_TEMT | LSR_THRE;

		/* Check for new receive data */
		if (fifo_numchars(&sc->rxfifo) > 0)
			sc->lsr |= LSR_RXRDY;
		else
			sc->lsr &= ~LSR_RXRDY;

		reg = sc->lsr;

		/* The LSR_OE bit is cleared on LSR read */
		sc->lsr &= ~LSR_OE;
		break;
	case REG_MSR:
		/*
		 * MSR delta bits are cleared on read
		 */
		reg = sc->msr;
		sc->msr &= ~MSR_DELTA_MASK;
		break;
	case REG_SCR:
		reg = sc->scr;
		break;
	default:
		reg = 0xFF;
		break;
	}

done:
	pci_uart_toggle_intr(sc);
	pthread_mutex_unlock(&sc->mtx);

	return (reg);
}

static int
pci_uart_init(struct vmctx *ctx, struct pci_devinst *pi, char *opts)
{
	struct pci_uart_softc *sc;
	uint64_t bar;
	int ivec;

	sc = malloc(sizeof(struct pci_uart_softc));
	memset(sc, 0, sizeof(struct pci_uart_softc));

	pi->pi_arg = sc;
	sc->pi = pi;

	pthread_mutex_init(&sc->mtx, NULL);

	/* initialize config space */
	pci_set_cfgdata16(pi, PCIR_DEVICE, COM_DEV);
	pci_set_cfgdata16(pi, PCIR_VENDOR, COM_VENDOR);
	pci_set_cfgdata8(pi, PCIR_CLASS, PCIC_SIMPLECOMM);
	if (pci_is_legacy(pi)) {
		pci_uart_legacy_res(&bar, &ivec);
		pci_emul_alloc_pbar(pi, 0, bar, PCIBAR_IO, 8);
	} else {
		ivec = -1;
		pci_emul_alloc_bar(pi, 0, PCIBAR_IO, 8);
	}
	pci_lintr_request(pi, ivec);

	if (opts != NULL && !strcmp("stdio", opts) && !pci_uart_stdio) {
		pci_uart_stdio = 1;
		sc->stdio = 1;
	}

	pci_uart_reset(sc);

	return (0);
}

struct pci_devemu pci_de_com = {
	.pe_emu =	"uart",
	.pe_init =	pci_uart_init,
	.pe_barwrite =	pci_uart_write,
	.pe_barread =	pci_uart_read
};
PCI_EMUL_SET(pci_de_com);
