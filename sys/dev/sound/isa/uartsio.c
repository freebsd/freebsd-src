/*
 * Copyright by George Hansper 1996
 *
 * Tue Jan 23 22:32:10 EST 1996 ghansper@daemon.apana.org.au
 *      added 16450/16550 support for standard serial-port UARTs
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS
 * IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * Wed Apl  1 02:25:30 JST 1998 zinnia@jan.ne.jp
 *      ported to FreeBSD 2.2.5R-RELEASE
 *
 * Fri Apl  1 21:16:20 JST 1999 zinnia@jan.ne.jp
 *      ported to FreeBSD 3.1-STABLE
 *
 *
 * Ported to the new Audio Driver by Luigi Rizzo:
 * (C) 1999 Seigo Tanimura
 *
 * This is the 16550 midi uart driver for FreeBSD, based on the Luigi Sound Driver.
 * This handles io against /dev/midi, the midi {in, out}put event queues
 * and the event/message transmittion to/from a serial port interface.
 *
 * $FreeBSD$
 *
 */

#include <isa/sioreg.h>
#include <dev/ic/ns16550.h>
#include <dev/sound/midi/midi.h>

/* XXX What about a PCI uart? */
#include <isa/isavar.h>

static devclass_t midi_devclass;

#ifndef DDB
#undef DDB
#define DDB(x)
#endif /* DDB */

#define TX_FIFO_SIZE 16

extern synthdev_info midisynth_op_desc;

/* These are the synthesizer and the midi interface information. */
static struct synth_info uartsio_synthinfo = {
	"uart16550A MIDI",
	0,
	SYNTH_TYPE_MIDI,
	0,
	0,
	128,
	128,
	128,
	SYNTH_CAP_INPUT,
};

static struct midi_info uartsio_midiinfo = {
	"uart16550A MIDI",
	0,
	0,
	0,
};

/*
 * These functions goes into uartsio_op_desc to get called
 * from sound.c.
 */

static int uartsio_probe(device_t dev);
static int uartsio_attach(device_t dev);

static d_ioctl_t uartsio_ioctl;
static driver_intr_t uartsio_intr;
static midi_callback_t uartsio_callback;

/* Here is the parameter structure per a device. */
struct uartsio_softc {
	device_t dev; /* device information */
	mididev_info *devinfo; /* midi device information */

	struct mtx mtx; /* Mutex to protect the device. */

	struct resource *io; /* Base of io port */
	int io_rid; /* Io resource ID */
	struct resource *irq; /* Irq */
	int irq_rid; /* Irq resource ID */
	void *ih; /* Interrupt cookie */

	int fflags; /* File flags */

	int has_fifo; /* TX/RX fifo in the uart */
	int tx_size; /* Size of TX on a transmission */

};

typedef struct uartsio_softc *sc_p;

/* These functions are local. */
static void uartsio_startplay(sc_p scp);
static int uartsio_xmit(sc_p scp);
static int uartsio_readport(sc_p scp, int off);
static void uartsio_writeport(sc_p scp, int off, u_int8_t value);
static int uartsio_allocres(sc_p scp, device_t dev);
static void uartsio_releaseres(sc_p scp, device_t dev);

/*
 * This is the device descriptor for the midi device.
 */
static mididev_info uartsio_op_desc = {
	"16550 uart midi",

	SNDCARD_UART16550,

	NULL,
	NULL,
	uartsio_ioctl,

	uartsio_callback,

	MIDI_BUFFSIZE, /* Queue Length */

	0, /* XXX This is not an *audio* device! */
};

/*
 * Here are the main functions to interact to the user process.
 * These are called from snd* functions in sys/i386/isa/snd/sound.c.
 */

static int
uartsio_probe(device_t dev)
{
	sc_p scp;
	int unit;
	u_char c;

	if (isa_get_logicalid(dev) != 0)
		/* This is NOT a PnP device! */
		return (ENXIO);

	scp = device_get_softc(dev);
	unit = device_get_unit(dev);

	device_set_desc(dev, uartsio_op_desc.name);
	bzero(scp, sizeof(*scp));

	scp->io_rid = 0;
	scp->io = bus_alloc_resource(dev, SYS_RES_IOPORT, &scp->io_rid, 0, ~0, 8, RF_ACTIVE);
	if (scp->io == NULL)
		return (ENXIO);

	DEB(printf("uartsio%d: probing.\n", unit));

/* Read the IER. The upper four bits should all be zero. */
	c = uartsio_readport(scp, com_ier);
	if ((c & 0xf0) != 0) {
		uartsio_releaseres(scp, dev);
		return (ENXIO);
	}

/* Read the MSR. The upper three bits should all be zero. */
	c = uartsio_readport(scp, com_mcr);
	if ((c & 0xe0) != 0) {
		uartsio_releaseres(scp, dev);
		return (ENXIO);
	}

	/* XXX Do we need a loopback test? */

	DEB(printf("uartsio%d: probed.\n", unit));

	return (0);
}

static int
uartsio_attach(device_t dev)
{
	sc_p scp;
	mididev_info *devinfo;

	scp = device_get_softc(dev);

	DEB(printf("uartsio: attaching.\n"));

	/* Allocate resources. */
	if (uartsio_allocres(scp, dev)) {
		uartsio_releaseres(scp, dev);
		return (ENXIO);
	}

	/* See the size of the tx fifo. */
	uartsio_writeport(scp, com_fifo, FIFO_ENABLE | FIFO_RCV_RST | FIFO_XMT_RST | FIFO_RX_HIGH);
	if ((uartsio_readport(scp, com_iir) & IIR_FIFO_MASK) == FIFO_RX_HIGH) {
		scp->has_fifo = 1;
		scp->tx_size = TX_FIFO_SIZE;
		DEB(printf("uartsio: uart is 16550A, tx size is %d bytes.\n", scp->tx_size));
	} else {
		scp->has_fifo = 0;
		scp->tx_size = 1;
		DEB(printf("uartsio: uart is not 16550A.\n"));
	}

	/* Configure the uart. */
	uartsio_writeport(scp, com_cfcr, CFCR_DLAB); /* Latch the divisor. */
	uartsio_writeport(scp, com_dlbl, 0x03);
	uartsio_writeport(scp, com_dlbh, 0x00); /* We want a bitrate of 38.4kbps. */
	uartsio_writeport(scp, com_cfcr, CFCR_8BITS); /* We want 8bits, 1 stop bit, no parity. */
	uartsio_writeport(scp, com_mcr, MCR_IENABLE | MCR_RTS | MCR_DTR); /* Enable interrupt, set RTS and DTR. */
	uartsio_writeport(scp, com_ier, IER_ERXRDY | IER_ETXRDY | IER_EMSC | IER_ERLS); /* Give us an interrupt on RXRDY, TXRDY, MSC and RLS. */
	if (scp->has_fifo)
		uartsio_writeport(scp, com_fifo, FIFO_ENABLE | FIFO_RCV_RST | FIFO_XMT_RST | FIFO_RX_LOW); /* We use the fifo. */
	else
		uartsio_writeport(scp, com_fifo, FIFO_RCV_RST | FIFO_XMT_RST | FIFO_RX_LOW); /* We do not use the fifo. */

	/* Clear the gabage. */
	uartsio_readport(scp, com_lsr);
	uartsio_readport(scp, com_lsr);
	uartsio_readport(scp, com_iir);
	uartsio_readport(scp, com_data);

	/* Fill the softc. */
	scp->dev = dev;
	mtx_init(&scp->mtx, "siomid", MTX_DEF);
	scp->devinfo = devinfo = create_mididev_info_unit(MDT_MIDI, &uartsio_op_desc, &midisynth_op_desc);

	/* Fill the midi info. */
	snprintf(devinfo->midistat, sizeof(devinfo->midistat), "at 0x%x irq %d",
		 (u_int)rman_get_start(scp->io), (int)rman_get_start(scp->irq));

	midiinit(devinfo, dev);

	/* Now we can handle the interrupts. */
	bus_setup_intr(dev, scp->irq, INTR_TYPE_TTY, uartsio_intr, scp, &scp->ih);

	DEB(printf("uartsio: attached.\n"));

	return (0);
}

static int
uartsio_ioctl(dev_t i_dev, u_long cmd, caddr_t arg, int mode, struct proc *p)
{
	sc_p scp;
	mididev_info *devinfo;
	int unit;
	struct synth_info *synthinfo;
	struct midi_info *midiinfo;

	unit = MIDIUNIT(i_dev);

	devinfo = get_mididev_info(i_dev, &unit);
	if (devinfo == NULL) {
		DEB(printf("uartsio_ioctl: unit %d is not configured.\n", unit));
		return (ENXIO);
	}
	scp = devinfo->softc;

	switch (cmd) {
	case SNDCTL_SYNTH_INFO:
		synthinfo = (struct synth_info *)arg;
		if (synthinfo->device != unit)
			return (ENXIO);
		bcopy(&uartsio_synthinfo, synthinfo, sizeof(uartsio_synthinfo));
		synthinfo->device = unit;
		return (0);
		break;
	case SNDCTL_MIDI_INFO:
		midiinfo = (struct midi_info *)arg;
		if (midiinfo->device != unit)
			return (ENXIO);
		bcopy(&uartsio_midiinfo, midiinfo, sizeof(uartsio_midiinfo));
		midiinfo->device = unit;
		return (0);
		break;
	default:
		return (ENOSYS);
	}
	/* NOTREACHED */
	return (EINVAL);
}

static void
uartsio_intr(void *arg)
{
	sc_p scp;
	mididev_info *devinfo;

	scp = (sc_p)arg;
	devinfo = scp->devinfo;

	mtx_lock(&devinfo->flagqueue_mtx);
	uartsio_xmit(scp);
	mtx_unlock(&devinfo->flagqueue_mtx);

	/* Invoke the upper layer. */
	midi_intr(devinfo);
}

static int
uartsio_callback(mididev_info *d, int reason)
{
	int unit;
	sc_p scp;

	mtx_assert(&d->flagqueue_mtx, MA_OWNED);

	if (d == NULL) {
		DEB(printf("uartsio_callback: device not configured.\n"));
		return (ENXIO);
	}

	unit = d->unit;
	scp = d->softc;

	switch (reason & MIDI_CB_REASON_MASK) {
	case MIDI_CB_START:
		if ((reason & MIDI_CB_RD) != 0 && (d->flags & MIDI_F_READING) == 0)
			/* Begin recording. */
			d->flags |= MIDI_F_READING;
		if ((reason & MIDI_CB_WR) != 0 && (d->flags & MIDI_F_WRITING) == 0)
			uartsio_startplay(scp);
		break;
	case MIDI_CB_STOP:
	case MIDI_CB_ABORT:
		if ((reason & MIDI_CB_RD) != 0 && (d->flags & MIDI_F_READING) != 0)
			/* Stop recording. */
			d->flags &= ~MIDI_F_READING;
		if ((reason & MIDI_CB_WR) != 0 && (d->flags & MIDI_F_WRITING) != 0)
			/* Stop Playing. */
			d->flags &= ~MIDI_F_WRITING;
		break;
	}

	return (0);
}

/*
 * The functions below here are the libraries for the above ones.
 */

/*
 * Starts to play the data in the output queue.
 */
static void
uartsio_startplay(sc_p scp)
{
	mididev_info *devinfo;

	devinfo = scp->devinfo;

	mtx_assert(&devinfo->flagqueue_mtx, MA_OWNED);

	/* Can we play now? */
	if (devinfo->midi_dbuf_out.rl == 0)
		return;

	devinfo->flags |= MIDI_F_WRITING;
	uartsio_xmit(scp);
}

static int
uartsio_xmit(sc_p scp)
{
	mididev_info *devinfo;
	midi_dbuf *dbuf;
	int lsr, msr, iir, i, txsize;
	u_char c[TX_FIFO_SIZE];

	devinfo = scp->devinfo;

	mtx_assert(&devinfo->flagqueue_mtx, MA_OWNED);

	mtx_lock(&scp->mtx);
	for (;;) {
		/* Read the received data. */
		while (((lsr = uartsio_readport(scp, com_lsr)) & LSR_RCV_MASK) != 0) {
			/* Is this a data or an error/break? */
			if ((lsr & LSR_RXRDY) == 0)
				printf("uartsio_xmit: receive error or break in unit %d.\n", devinfo->unit);
			else {
				/* Receive the data. */
				c[0] = uartsio_readport(scp, com_data);
				mtx_unlock(&scp->mtx);
				/* Queue into the passthru buffer and start transmitting if we can. */
				if ((devinfo->flags & MIDI_F_PASSTHRU) != 0 && ((devinfo->flags & MIDI_F_BUSY) == 0 || (devinfo->fflags & FWRITE) == 0)) {
					midibuf_input_intr(&devinfo->midi_dbuf_passthru, &c[0], sizeof(c[0]));
					devinfo->flags |= MIDI_F_WRITING;
				}
				/* Queue if we are reading. Discard an active sensing. */
				if ((devinfo->flags & MIDI_F_READING) != 0 && c[0] != 0xfe)
					midibuf_input_intr(&devinfo->midi_dbuf_in, &c[0], sizeof(c[0]));
				mtx_lock(&scp->mtx);
			}
		}
		mtx_unlock(&scp->mtx);

		/* See which source to use. */
		if ((devinfo->flags & MIDI_F_PASSTHRU) == 0 || ((devinfo->flags & MIDI_F_BUSY) != 0 && (devinfo->fflags & FWRITE) != 0))
			dbuf = &devinfo->midi_dbuf_out;
		else
			dbuf = &devinfo->midi_dbuf_passthru;

		/* Transmit the data in the queue. */
		if ((devinfo->flags & MIDI_F_WRITING) != 0) {
			/* Do we have the data to transmit? */
			if (dbuf->rl == 0) {
				/* Stop playing. */
				devinfo->flags &= ~MIDI_F_WRITING;
			} else {
				mtx_lock(&scp->mtx);
				/* Read LSR and MSR. */
				lsr = uartsio_readport(scp, com_lsr);
				msr = uartsio_readport(scp, com_msr);
				/* Is the device ready?. */
				if ((lsr & LSR_TXRDY) != 0 && (msr & MSR_CTS) != 0) {
					/* send the data. */
					txsize = scp->tx_size;
					if (dbuf->rl < txsize)
						txsize = dbuf->rl;
					midibuf_output_intr(dbuf, c, txsize);
					for (i = 0 ; i < txsize ; i++)
						uartsio_writeport(scp, com_data, c[i]);
					/* We are playing now. */
					devinfo->flags |= MIDI_F_WRITING;
				} else {
					/* Do we have the data to transmit? */
					if (dbuf->rl > 0)
					/* Wait for the next interrupt. */
						devinfo->flags |= MIDI_F_WRITING;
				}
				mtx_unlock(&scp->mtx);
			}
		}
		mtx_lock(&scp->mtx);
		if (((iir = uartsio_readport(scp, com_iir)) & IIR_IMASK) == IIR_NOPEND)
			break;
	}
	mtx_unlock(&scp->mtx);

	return (0);
}

/* Reads from a port. */
static int
uartsio_readport(sc_p scp, int off)
{
	return bus_space_read_1(rman_get_bustag(scp->io), rman_get_bushandle(scp->io), off);
}

/* Writes to a port. */
static void
uartsio_writeport(sc_p scp, int off, u_int8_t value)
{
	return bus_space_write_1(rman_get_bustag(scp->io), rman_get_bushandle(scp->io), off, value);
}

/* Allocates resources other than IO ports. */
static int
uartsio_allocres(sc_p scp, device_t dev)
{
	if (scp->irq == NULL) {
		scp->irq_rid = 0;
		scp->irq = bus_alloc_resource(dev, SYS_RES_IRQ, &scp->irq_rid, 0, ~0, 1, RF_ACTIVE);
	}
	if (scp->irq == NULL)
		return (1);

	return (0);
}

/* Releases resources. */
static void
uartsio_releaseres(sc_p scp, device_t dev)
{
	if (scp->irq != NULL) {
		bus_release_resource(dev, SYS_RES_IRQ, scp->irq_rid, scp->irq);
		scp->irq = NULL;
	}
	if (scp->io != NULL) {
		bus_release_resource(dev, SYS_RES_IOPORT, scp->io_rid, scp->io);
		scp->io = NULL;
	}
}

static device_method_t uartsio_methods[] = {
/* Device interface */
	DEVMETHOD(device_probe , uartsio_probe ),
	DEVMETHOD(device_attach, uartsio_attach),

	{ 0, 0 },
};

static driver_t uartsio_driver = {
	"midi",
	uartsio_methods,
	sizeof(struct uartsio_softc),
};

DRIVER_MODULE(uartsio, isa, uartsio_driver, midi_devclass, 0, 0);
