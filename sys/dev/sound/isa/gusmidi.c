/*
 * GUS midi interface driver.
 * Based on the newmidi MPU401 driver.
 *
 * Copyright (c) 1999 Ville-Pertti Keinonen
 * Copyright by Hannu Savolainen 1993
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer. 2.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * Modified: Riccardo Facchetti  24 Mar 1995 - Added the Audio Excel DSP 16
 * initialization routine.
 *
 * Ported to the new Audio Driver by Luigi Rizzo:
 * (C) 1999 Seigo Tanimura <tanimura@r.dl.itc.u-tokyo.ac.jp>
 *
 * $FreeBSD$
 *
 */

#include <dev/sound/midi/midi.h>

#include <dev/sound/chip.h>
#include <machine/cpufunc.h>

static devclass_t midi_devclass;

extern synthdev_info midisynth_op_desc;

/* These are the synthesizer and the midi interface information. */
static struct synth_info gusmidi_synthinfo = {
	"GUS MIDI",
	0,
	SYNTH_TYPE_MIDI,
	0,
	0,
	128,
	128,
	128,
	SYNTH_CAP_INPUT,
};

static struct midi_info gusmidi_midiinfo = {
	"GUS MIDI",
	0,
	0,
	0,
};

#define MIDICTL_MASTER_RESET	0x03
#define MIDICTL_TX_IRQ_EN	0x20
#define MIDICTL_RX_IRQ_EN	0x80

#define MIDIST_RXFULL		0x01
#define MIDIST_TXDONE		0x02
#define MIDIST_ERR_FR		0x10
#define MIDIST_ERR_OVR		0x20
#define MIDIST_INTR_PEND	0x80

#define PORT_CTL		0
#define PORT_ST			0
#define PORT_TX			1
#define PORT_RX			1

/*
 * These functions goes into gusmidi_op_desc to get called
 * from sound.c.
 */

static int gusmidi_probe(device_t dev);
static int gusmidi_attach(device_t dev);

static d_open_t gusmidi_open;
static d_ioctl_t gusmidi_ioctl;
driver_intr_t gusmidi_intr;
static midi_callback_t gusmidi_callback;

/* Here is the parameter structure per a device. */
struct gusmidi_softc {
	device_t dev; /* device information */
	mididev_info *devinfo; /* midi device information */

	struct mtx mtx; /* Mutex to protect the device. */

	struct resource *io; /* Base of io port */
	int io_rid; /* Io resource ID */
	struct resource *irq; /* Irq */
	int irq_rid; /* Irq resource ID */
	void *ih; /* Interrupt cookie */

	int ctl;	/* Control bits.  */
};

typedef struct gusmidi_softc *sc_p;

/* These functions are local. */
static int gusmidi_init(device_t dev);
static int gusmidi_allocres(sc_p scp, device_t dev);
static void gusmidi_releaseres(sc_p scp, device_t dev);
static void gusmidi_startplay(sc_p scp);
static void gusmidi_xmit(sc_p scp);
static u_int gusmidi_readport(sc_p scp, int off);
static void gusmidi_writeport(sc_p scp, int off, u_int8_t value);

/*
 * This is the device descriptor for the midi device.
 */
static mididev_info gusmidi_op_desc = {
	"GUS midi",

	SNDCARD_GUS,

	gusmidi_open,
	NULL,
	gusmidi_ioctl,

	gusmidi_callback,

	MIDI_BUFFSIZE, /* Queue Length */

	0, /* XXX This is not an *audio* device! */
};

static int
gusmidi_probe(device_t dev)
{
	char *s;
	sc_p scp;
	struct sndcard_func *func;

	/* The parent device has already been probed. */

	func = device_get_ivars(dev);
	if (func == NULL || func->func != SCF_MIDI)
		return (ENXIO);

	s = "GUS Midi Interface";

	scp = device_get_softc(dev);
	bzero(scp, sizeof(*scp));
	scp->io_rid = 1;
	scp->irq_rid = 0;
#if notdef
	ret = mpu_probe2(dev);
	if (ret != 0)
		return (ret);
#endif /* notdef */
	device_set_desc(dev, s);
	return (0);
}

static int
gusmidi_attach(device_t dev)
{
	sc_p scp;

	scp = device_get_softc(dev);

	/* Allocate the resources, switch to uart mode. */
	if (gusmidi_allocres(scp, dev)) {
		gusmidi_releaseres(scp, dev);
		return (ENXIO);
	}

	gusmidi_init(dev);

	return (0);
}

static int
gusmidi_init(device_t dev)
{
	sc_p scp;
	mididev_info *devinfo;

	scp = device_get_softc(dev);

	/* Fill the softc. */
	scp->dev = dev;
	mtx_init(&scp->mtx, "gusmid", MTX_DEF);
	scp->devinfo = devinfo = create_mididev_info_unit(MDT_MIDI, &gusmidi_op_desc, &midisynth_op_desc);

	/* Fill the midi info. */
	if (scp->irq != NULL)
		snprintf(devinfo->midistat, sizeof(devinfo->midistat), "at 0x%x irq %d",
			 (u_int)rman_get_start(scp->io), (int)rman_get_start(scp->irq));
	else
		snprintf(devinfo->midistat, sizeof(devinfo->midistat), "at 0x%x",
			 (u_int)rman_get_start(scp->io));

	midiinit(devinfo, dev);

	bus_setup_intr(dev, scp->irq, INTR_TYPE_AV, gusmidi_intr, scp,
	    &scp->ih);

	return (0);
}

static int
gusmidi_open(dev_t i_dev, int flags, int mode, struct thread *td)
{
	sc_p scp;
	mididev_info *devinfo;
	int unit;

	unit = MIDIUNIT(i_dev);

	devinfo = get_mididev_info(i_dev, &unit);
	if (devinfo == NULL) {
		DEB(printf("gusmidi_open: unit %d is not configured.\n", unit));
		return (ENXIO);
	}
	scp = devinfo->softc;

	mtx_lock(&scp->mtx);

	gusmidi_writeport(scp, PORT_CTL, MIDICTL_MASTER_RESET);
	DELAY(100);

	gusmidi_writeport(scp, PORT_CTL, scp->ctl);

	mtx_unlock(&scp->mtx);

	return (0);
}

static int
gusmidi_ioctl(dev_t i_dev, u_long cmd, caddr_t arg, int mode, struct thread *td)
{
	sc_p scp;
	mididev_info *devinfo;
	int unit;
	struct synth_info *synthinfo;
	struct midi_info *midiinfo;

	unit = MIDIUNIT(i_dev);

	devinfo = get_mididev_info(i_dev, &unit);
	if (devinfo == NULL) {
		DEB(printf("gusmidi_ioctl: unit %d is not configured.\n", unit));
		return (ENXIO);
	}
	scp = devinfo->softc;

	switch (cmd) {
	case SNDCTL_SYNTH_INFO:
		synthinfo = (struct synth_info *)arg;
		if (synthinfo->device != unit)
			return (ENXIO);
		bcopy(&gusmidi_synthinfo, synthinfo, sizeof(gusmidi_synthinfo));
		synthinfo->device = unit;
		return (0);
		break;
	case SNDCTL_MIDI_INFO:
		midiinfo = (struct midi_info *)arg;
		if (midiinfo->device != unit)
			return (ENXIO);
		bcopy(&gusmidi_midiinfo, midiinfo, sizeof(gusmidi_midiinfo));
		midiinfo->device = unit;
		return (0);
		break;
	default:
		return (ENOSYS);
	}
	/* NOTREACHED */
	return (EINVAL);
}

void
gusmidi_intr(void *arg)
{
	sc_p scp;
	u_char c;
	mididev_info *devinfo;
	int stat, did_something, leni;

	scp = (sc_p)arg;
	devinfo = scp->devinfo;

	/* XXX No framing/overrun checks...  */
	mtx_lock(&devinfo->flagqueue_mtx);
	mtx_lock(&scp->mtx);

	do {
		stat = gusmidi_readport(scp, PORT_ST);
		did_something = 0;
		if (stat & MIDIST_RXFULL) {
			c = gusmidi_readport(scp, PORT_RX);
			mtx_unlock(&scp->mtx);
			if ((devinfo->flags & MIDI_F_PASSTHRU) &&
			    (!(devinfo->flags & MIDI_F_BUSY) ||
			     !(devinfo->fflags & FWRITE))) {
				midibuf_input_intr(&devinfo->midi_dbuf_passthru,
				    &c, sizeof c, &leni);
				devinfo->callback(devinfo,
				    MIDI_CB_START | MIDI_CB_WR);
			}
			if ((devinfo->flags & MIDI_F_READING) && c != 0xfe) {
				midibuf_input_intr(&devinfo->midi_dbuf_in,
				    &c, sizeof c, &leni);
			}
			did_something = 1;
		} else
			mtx_unlock(&scp->mtx);
		if (stat & MIDIST_TXDONE) {
			if (devinfo->flags & MIDI_F_WRITING) {
				gusmidi_xmit(scp);
				did_something = 1;
				mtx_lock(&scp->mtx);
			} else if (scp->ctl & MIDICTL_TX_IRQ_EN) {
				/* This shouldn't happen.  */
				mtx_lock(&scp->mtx);
				scp->ctl &= ~MIDICTL_TX_IRQ_EN;	
				gusmidi_writeport(scp, PORT_CTL, scp->ctl);
			}
		} else
			mtx_lock(&scp->mtx);
	} while (did_something != 0);

	mtx_unlock(&scp->mtx);
	mtx_unlock(&devinfo->flagqueue_mtx);

	/* Invoke the upper layer. */
	midi_intr(devinfo);
}

static int
gusmidi_callback(void *di, int reason)
{
	int unit;
	sc_p scp;
	mididev_info *d;

	d = (mididev_info *)di;

	mtx_assert(&d->flagqueue_mtx, MA_OWNED);

	if (d == NULL) {
		DEB(printf("gusmidi_callback: device not configured.\n"));
		return (ENXIO);
	}

	unit = d->unit;
	scp = d->softc;

	switch (reason & MIDI_CB_REASON_MASK) {
	case MIDI_CB_START:
		if ((reason & MIDI_CB_RD) != 0 && (d->flags & MIDI_F_READING) == 0) {
			/* Begin recording. */
			d->flags |= MIDI_F_READING;
			mtx_lock(&scp->mtx);
			scp->ctl |= MIDICTL_RX_IRQ_EN;
			gusmidi_writeport(scp, PORT_CTL, scp->ctl);
			mtx_unlock(&scp->mtx);
		}
		if ((reason & MIDI_CB_WR) != 0 && (d->flags & MIDI_F_WRITING) == 0)
			/* Start playing. */
			gusmidi_startplay(scp);
		break;
	case MIDI_CB_STOP:
	case MIDI_CB_ABORT:
		mtx_lock(&scp->mtx);
		if ((reason & MIDI_CB_RD) != 0 && (d->flags & MIDI_F_READING) != 0) {
			/* Stop recording. */
			d->flags &= ~MIDI_F_READING;
			scp->ctl &= ~MIDICTL_RX_IRQ_EN;
		}
		if ((reason & MIDI_CB_WR) != 0 && (d->flags & MIDI_F_WRITING) != 0) {
			/* Stop Playing. */
			d->flags &= ~MIDI_F_WRITING;
			scp->ctl &= ~MIDICTL_TX_IRQ_EN;
		}
		gusmidi_writeport(scp, PORT_CTL, scp->ctl);
		mtx_unlock(&scp->mtx);
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
gusmidi_startplay(sc_p scp)
{
	mididev_info *devinfo;

	devinfo = scp->devinfo;

	mtx_assert(&devinfo->flagqueue_mtx, MA_OWNED);

	/* Can we play now? */
	if (devinfo->midi_dbuf_out.rl == 0)
		return;

	devinfo->flags |= MIDI_F_WRITING;
	mtx_lock(&scp->mtx);
	scp->ctl |= MIDICTL_TX_IRQ_EN;
	mtx_unlock(&scp->mtx);
}

static void
gusmidi_xmit(sc_p scp)
{
	register mididev_info *devinfo;
	register midi_dbuf *dbuf;
	u_char c;
	int leno;

	devinfo = scp->devinfo;

	mtx_assert(&devinfo->flagqueue_mtx, MA_OWNED);

	/* See which source to use. */
	if ((devinfo->flags & MIDI_F_PASSTHRU) == 0 || ((devinfo->flags & MIDI_F_BUSY) != 0 && (devinfo->fflags & FWRITE) != 0))
		dbuf = &devinfo->midi_dbuf_out;
	else
		dbuf = &devinfo->midi_dbuf_passthru;

	/* Transmit the data in the queue. */
	while (devinfo->flags & MIDI_F_WRITING) {
		/* Do we have the data to transmit? */
		if (dbuf->rl == 0) {
			/* Stop playing. */
			devinfo->flags &= ~MIDI_F_WRITING;
			mtx_lock(&scp->mtx);
			scp->ctl &= ~MIDICTL_TX_IRQ_EN;
			gusmidi_writeport(scp, PORT_CTL, scp->ctl);
			mtx_unlock(&scp->mtx);
			break;
		} else {
			mtx_lock(&scp->mtx);
			if (gusmidi_readport(scp, PORT_ST) & MIDIST_TXDONE) {
				/* Send the data. */
				midibuf_output_intr(dbuf, &c, sizeof(c), &leno);
				gusmidi_writeport(scp, PORT_TX, c);
				/* We are playing now. */
			} else {
				mtx_unlock(&scp->mtx);
				break;
			}
			mtx_unlock(&scp->mtx);
		}
	}
}

/* Reads from a port. */
static u_int
gusmidi_readport(sc_p scp, int off)
{
	return bus_space_read_1(rman_get_bustag(scp->io), rman_get_bushandle(scp->io), off) & 0xff;
}

/* Writes to a port. */
static void
gusmidi_writeport(sc_p scp, int off, u_int8_t value)
{
	bus_space_write_1(rman_get_bustag(scp->io), rman_get_bushandle(scp->io), off, value);
}

/* Allocates resources. */
static int
gusmidi_allocres(sc_p scp, device_t dev)
{
	if (scp->io == NULL) {
		scp->io = bus_alloc_resource(dev, SYS_RES_IOPORT, &scp->io_rid, 0, ~0, 2, RF_ACTIVE);
		if (scp->io == NULL)
			return (1);
	}
#if notdef
	if (scp->irq == NULL && !(device_get_flags(dev) & MPU_DF_NO_IRQ)) {
#else
	if (scp->irq == NULL) {
#endif /* notdef */
		scp->irq = bus_alloc_resource(dev, SYS_RES_IRQ, &scp->irq_rid, 0, ~0, 1, RF_ACTIVE | RF_SHAREABLE);
		if (scp->irq == NULL)
			return (1);
	}

	return (0);
}

/* Releases resources. */
static void
gusmidi_releaseres(sc_p scp, device_t dev)
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

static device_method_t gusmidi_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe , gusmidi_probe ),
	DEVMETHOD(device_attach, gusmidi_attach),

	{ 0, 0 },
};

driver_t gusmidi_driver = {
	"midi",
	gusmidi_methods,
	sizeof(struct gusmidi_softc),
};

DRIVER_MODULE(gusmidi, gusc, gusmidi_driver, midi_devclass, 0, 0);
