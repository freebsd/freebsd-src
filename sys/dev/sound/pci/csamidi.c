/*-
 * Copyright (c) 1999 Seigo Tanimura
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
 * $FreeBSD$
 */

#include <dev/sound/midi/midi.h>
#include <dev/sound/chip.h>
#include <dev/sound/pci/csareg.h>
#include <machine/cpufunc.h>

#include <pci/pcireg.h>
#include <pci/pcivar.h>

static devclass_t midi_devclass;

#ifndef DDB
#undef DDB
#define DDB(x)
#endif /* DDB */

#define CSAMIDI_RESET      0xff
#define CSAMIDI_UART       0x3f
#define CSAMIDI_ACK        0xfe

#define CSAMIDI_STATMASK   0xc0
#define CSAMIDI_OUTPUTBUSY 0x40
#define CSAMIDI_INPUTBUSY  0x80

#define CSAMIDI_TRYDATA 50
#define CSAMIDI_DELAY   25000

extern synthdev_info midisynth_op_desc;

/* These are the synthesizer and the midi interface information. */
static struct synth_info csamidi_synthinfo = {
	"CS461x MIDI",
	0,
	SYNTH_TYPE_MIDI,
	0,
	0,
	128,
	128,
	128,
	SYNTH_CAP_INPUT,
};

static struct midi_info csamidi_midiinfo = {
	"CS461x MIDI",
	0,
	0,
	0,
};

/*
 * These functions goes into csamidi_op_desc to get called
 * from sound.c.
 */

static int csamidi_probe(device_t dev);
static int csamidi_attach(device_t dev);

static d_ioctl_t csamidi_ioctl;
static driver_intr_t csamidi_intr;
static midi_callback_t csamidi_callback;

/* Here is the parameter structure per a device. */
struct csamidi_softc {
	device_t dev; /* device information */
	mididev_info *devinfo; /* midi device information */
	struct csa_bridgeinfo *binfo; /* The state of the parent. */

	struct mtx mtx; /* Mutex to protect the device. */

	struct resource *io; /* Base of io map */
	int io_rid; /* Io map resource ID */
	struct resource *mem; /* Base of memory map */
	int mem_rid; /* Memory map resource ID */
	struct resource *irq; /* Irq */
	int irq_rid; /* Irq resource ID */
	void *ih; /* Interrupt cookie */

	int fflags; /* File flags */
};

typedef struct csamidi_softc *sc_p;

/* These functions are local. */
static void csamidi_startplay(sc_p scp);
static void csamidi_xmit(sc_p scp);
static int csamidi_reset(sc_p scp);
static int csamidi_status(sc_p scp);
static int csamidi_command(sc_p scp, u_int32_t value);
static int csamidi_readdata(sc_p scp);
static int csamidi_writedata(sc_p scp, u_int32_t value);
static u_int32_t csamidi_readio(sc_p scp, u_long offset);
static void csamidi_writeio(sc_p scp, u_long offset, u_int32_t data);
/* Not used in this file. */
#if notdef
static u_int32_t csamidi_readmem(sc_p scp, u_long offset);
static void csamidi_writemem(sc_p scp, u_long offset, u_int32_t data);
#endif /* notdef */
static int csamidi_allocres(sc_p scp, device_t dev);
static void csamidi_releaseres(sc_p scp, device_t dev);

/*
 * This is the device descriptor for the midi device.
 */
static mididev_info csamidi_op_desc = {
	"CS461x midi",

	SNDCARD_MPU401,

	NULL,
	NULL,
	csamidi_ioctl,

	csamidi_callback,

	MIDI_BUFFSIZE, /* Queue Length */

	0, /* XXX This is not an *audio* device! */
};

/*
 * Here are the main functions to interact to the user process.
 */

static int
csamidi_probe(device_t dev)
{
	char *s;
	sc_p scp;
	struct sndcard_func *func;

	/* The parent device has already been probed. */

	func = device_get_ivars(dev);
	if (func == NULL || func->func != SCF_MIDI)
		return (ENXIO);

	s = "CS461x Midi Interface";

	scp = device_get_softc(dev);
	bzero(scp, sizeof(*scp));
	scp->io_rid = PCIR_MAPS;
	scp->mem_rid = PCIR_MAPS + 4;
	scp->irq_rid = 0;

	device_set_desc(dev, s);
	return (0);
}

static int
csamidi_attach(device_t dev)
{
	sc_p scp;
	mididev_info *devinfo;
	struct sndcard_func *func;

	scp = device_get_softc(dev);
	func = device_get_ivars(dev);
	scp->binfo = func->varinfo;

	/* Allocate the resources. */
	if (csamidi_allocres(scp, dev)) {
		csamidi_releaseres(scp, dev);
		return (ENXIO);
	}

	/* Fill the softc. */
	scp->dev = dev;
	mtx_init(&scp->mtx, "csamid", MTX_DEF);
	scp->devinfo = devinfo = create_mididev_info_unit(MDT_MIDI, &csamidi_op_desc, &midisynth_op_desc);

	/* Fill the midi info. */
	snprintf(devinfo->midistat, sizeof(devinfo->midistat), "at irq %d",
		 (int)rman_get_start(scp->irq));

	midiinit(devinfo, dev);

	/* Enable interrupt. */
	if (bus_setup_intr(dev, scp->irq, INTR_TYPE_TTY, csamidi_intr, scp, &scp->ih)) {
		csamidi_releaseres(scp, dev);
		return (ENXIO);
	}

	/* Reset the interface. */
	csamidi_reset(scp);

	return (0);
}

static int
csamidi_ioctl(dev_t i_dev, u_long cmd, caddr_t arg, int mode, struct proc *p)
{
	sc_p scp;
	mididev_info *devinfo;
	int unit;
	struct synth_info *synthinfo;
	struct midi_info *midiinfo;

	unit = MIDIUNIT(i_dev);

	devinfo = get_mididev_info(i_dev, &unit);
	if (devinfo == NULL) {
		DEB(printf("csamidi_ioctl: unit %d is not configured.\n", unit));
		return (ENXIO);
	}
	scp = devinfo->softc;

	switch (cmd) {
	case SNDCTL_SYNTH_INFO:
		synthinfo = (struct synth_info *)arg;
		if (synthinfo->device != unit)
			return (ENXIO);
		bcopy(&csamidi_synthinfo, synthinfo, sizeof(csamidi_synthinfo));
		synthinfo->device = unit;
		return (0);
		break;
	case SNDCTL_MIDI_INFO:
		midiinfo = (struct midi_info *)arg;
		if (midiinfo->device != unit)
			return (ENXIO);
		bcopy(&csamidi_midiinfo, midiinfo, sizeof(csamidi_midiinfo));
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
csamidi_intr(void *arg)
{
	sc_p scp;
	u_char c;
	mididev_info *devinfo;

	scp = (sc_p)arg;
	devinfo = scp->devinfo;

	mtx_lock(&devinfo->flagqueue_mtx);
	mtx_lock(&scp->mtx);

	/* Read the received data. */
	while ((csamidi_status(scp) & MIDSR_RBE) == 0) {
		/* Receive the data. */
		c = (u_char)csamidi_readdata(scp);
		mtx_unlock(&scp->mtx);

		/* Queue into the passthru buffer and start transmitting if we can. */
		if ((devinfo->flags & MIDI_F_PASSTHRU) != 0 && ((devinfo->flags & MIDI_F_BUSY) == 0 || (devinfo->fflags & FWRITE) == 0)) {
			midibuf_input_intr(&devinfo->midi_dbuf_passthru, &c, sizeof(c));
			devinfo->callback(devinfo, MIDI_CB_START | MIDI_CB_WR);
		}
		/* Queue if we are reading. Discard an active sensing. */
		if ((devinfo->flags & MIDI_F_READING) != 0 && c != 0xfe) {
			midibuf_input_intr(&devinfo->midi_dbuf_in, &c, sizeof(c));
		}
		mtx_lock(&scp->mtx);
	}
	mtx_unlock(&scp->mtx);

	/* Transmit out data. */
	if ((devinfo->flags & MIDI_F_WRITING) != 0 && (csamidi_status(scp) & MIDSR_TBF) == 0)
		csamidi_xmit(scp);

	mtx_unlock(&devinfo->flagqueue_mtx);

	/* Invoke the upper layer. */
	midi_intr(devinfo);
}

static int
csamidi_callback(mididev_info *d, int reason)
{
	int unit;
	sc_p scp;

	mtx_assert(&d->flagqueue_mtx, MA_OWNED);

	if (d == NULL) {
		DEB(printf("csamidi_callback: device not configured.\n"));
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
			/* Start playing. */
			csamidi_startplay(scp);
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
csamidi_startplay(sc_p scp)
{
	mididev_info *devinfo;

	devinfo = scp->devinfo;

	mtx_assert(&devinfo->flagqueue_mtx, MA_OWNED);

	/* Can we play now? */
	if (devinfo->midi_dbuf_out.rl == 0)
		return;

	devinfo->flags |= MIDI_F_WRITING;
	csamidi_xmit(scp);
}

static void
csamidi_xmit(sc_p scp)
{
	register mididev_info *devinfo;
	register midi_dbuf *dbuf;
	u_char c;

	devinfo = scp->devinfo;

	mtx_assert(&devinfo->flagqueue_mtx, MA_OWNED);

	/* See which source to use. */
	if ((devinfo->flags & MIDI_F_PASSTHRU) == 0 || ((devinfo->flags & MIDI_F_BUSY) != 0 && (devinfo->fflags & FWRITE) != 0))
		dbuf = &devinfo->midi_dbuf_out;
	else
		dbuf = &devinfo->midi_dbuf_passthru;

	/* Transmit the data in the queue. */
	while ((devinfo->flags & MIDI_F_WRITING) != 0) {
		/* Do we have the data to transmit? */
		if (dbuf->rl == 0) {
			/* Stop playing. */
			devinfo->flags &= ~MIDI_F_WRITING;
			break;
		} else {
			mtx_lock(&scp->mtx);
			if ((csamidi_status(scp) & MIDSR_TBF) != 0) {
				mtx_unlock(&scp->mtx);
				break;
			}
			/* Send the data. */
			midibuf_output_intr(dbuf, &c, sizeof(c));
			csamidi_writedata(scp, c);
			/* We are playing now. */
			devinfo->flags |= MIDI_F_WRITING;
			mtx_unlock(&scp->mtx);
		}
	}
}

/* Reset midi. */
static int
csamidi_reset(sc_p scp)
{
	int i, resp;

	mtx_lock(&scp->mtx);

	/* Reset the midi. */
	resp = 0;
	for (i = 0 ; i < CSAMIDI_TRYDATA ; i++) {
		resp = csamidi_command(scp, MIDCR_MRST);
		if (resp == 0)
			break;
	}
	if (resp != 0) {
		mtx_unlock(&scp->mtx);
		return (1);
	}
	for (i = 0 ; i < CSAMIDI_TRYDATA ; i++) {
		resp = csamidi_command(scp, MIDCR_TXE | MIDCR_RXE | MIDCR_RIE | MIDCR_TIE);
		if (resp == 0)
			break;
	}
	if (resp != 0)
		return (1);

	mtx_unlock(&scp->mtx);

	DELAY(CSAMIDI_DELAY);

	return (0);
}

/* Reads the status. */
static int
csamidi_status(sc_p scp)
{
	return csamidi_readio(scp, BA0_MIDSR);
}

/* Writes a command. */
static int
csamidi_command(sc_p scp, u_int32_t value)
{
	csamidi_writeio(scp, BA0_MIDCR, value);

	return (0);
}

/* Reads a byte of data. */
static int
csamidi_readdata(sc_p scp)
{
	u_int status;

	/* Is the interface ready to read? */
	status = csamidi_status(scp);
	if ((status & MIDSR_RBE) != 0)
		/* The interface is busy. */
		return (-EAGAIN);

	return (int)csamidi_readio(scp, BA0_MIDRP) & 0xff;
}

/* Writes a byte of data. */
static int
csamidi_writedata(sc_p scp, u_int32_t value)
{
	u_int status;

	/* Is the interface ready to write? */
	status = csamidi_status(scp);
	if ((status & MIDSR_TBF) != 0)
		/* The interface is busy. */
		return (EAGAIN);

	csamidi_writeio(scp, BA0_MIDWP, value & 0xff);

	return (0);
}

static u_int32_t
csamidi_readio(sc_p scp, u_long offset)
{
	if (offset < BA0_AC97_RESET)
		return bus_space_read_4(rman_get_bustag(scp->io), rman_get_bushandle(scp->io), offset) & 0xffffffff;
	else
		return (0);
}

static void
csamidi_writeio(sc_p scp, u_long offset, u_int32_t data)
{
	if (offset < BA0_AC97_RESET)
		bus_space_write_4(rman_get_bustag(scp->io), rman_get_bushandle(scp->io), offset, data);
}

/* Not used in this file. */
#if notdef
static u_int32_t
csamidi_readmem(sc_p scp, u_long offset)
{
	return bus_space_read_4(rman_get_bustag(scp->mem), rman_get_bushandle(scp->mem), offset) & 0xffffffff;
}

static void
csamidi_writemem(sc_p scp, u_long offset, u_int32_t data)
{
	bus_space_write_4(rman_get_bustag(scp->mem), rman_get_bushandle(scp->mem), offset, data);
}
#endif /* notdef */

/* Allocates resources. */
static int
csamidi_allocres(sc_p scp, device_t dev)
{
	if (scp->io == NULL) {
		scp->io = bus_alloc_resource(dev, SYS_RES_MEMORY, &scp->io_rid, 0, ~0, 1, RF_ACTIVE);
		if (scp->io == NULL)
			return (1);
	}
	if (scp->mem == NULL) {
		scp->mem = bus_alloc_resource(dev, SYS_RES_MEMORY, &scp->mem_rid, 0, ~0, 1, RF_ACTIVE);
		if (scp->mem == NULL)
			return (1);
	}
	if (scp->irq == NULL) {
		scp->irq = bus_alloc_resource(dev, SYS_RES_IRQ, &scp->irq_rid, 0, ~0, 1, RF_ACTIVE | RF_SHAREABLE);
		if (scp->irq == NULL)
			return (1);
	}

	return (0);
}

/* Releases resources. */
static void
csamidi_releaseres(sc_p scp, device_t dev)
{
	if (scp->irq != NULL) {
		bus_release_resource(dev, SYS_RES_IRQ, scp->irq_rid, scp->irq);
		scp->irq = NULL;
	}
	if (scp->io != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY, scp->io_rid, scp->io);
		scp->io = NULL;
	}
	if (scp->mem != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY, scp->mem_rid, scp->mem);
		scp->mem = NULL;
	}
}

static device_method_t csamidi_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe , csamidi_probe ),
	DEVMETHOD(device_attach, csamidi_attach),

	{ 0, 0 },
};

static driver_t csamidi_driver = {
	"midi",
	csamidi_methods,
	sizeof(struct csamidi_softc),
};

DRIVER_MODULE(csamidi, csa, csamidi_driver, midi_devclass, 0, 0);
