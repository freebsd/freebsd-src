/*
 * The low level driver for Roland MPU-401 compatible Midi interfaces.
 * 
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
 * (C) 1999 Seigo Tanimura
 *
 * This is the MPU401 midi interface driver for FreeBSD, based on the Luigi Sound Driver.
 * This handles io against /dev/midi, the midi {in, out}put event queues
 * and the event/message transmittion to/from an MPU401 interface.
 *
 * $FreeBSD$
 *
 */

#include <dev/sound/midi/midi.h>
#include <dev/sound/chip.h>
#include <machine/cpufunc.h>

#include <isa/isavar.h>
#include <dev/ic/ns16550.h>

static devclass_t midi_devclass;

#ifndef DDB
#undef DDB
#define DDB(x)
#endif /* DDB */

#define MPU_DATAPORT   0
#define MPU_CMDPORT    1
#define MPU_STATPORT   1

#define MPU_RESET      0xff
#define MPU_UART       0x3f
#define MPU_ACK        0xfe

#define MPU_STATMASK   0xc0
#define MPU_OUTPUTBUSY 0x40
#define MPU_INPUTBUSY  0x80

#define MPU_TRYDATA 50
#define MPU_DELAY   25000

/* Device flag.  */
#define MPU_DF_NO_IRQ	1

extern synthdev_info midisynth_op_desc;

/* PnP IDs */
static struct isa_pnp_id mpu_ids[] = {
	{0x01200001, "@H@2001 Midi Interface"},	/* @H@2001 */
	{0x01100001, "@H@1001 Midi Interface"},	/* @H@1001 */
#if notdef
	/* TODO: write bridge driver for these devices */
	{0x0000630e, "CSC0000 Midi Interface"},	/* CSC0000 */
	{0x2100a865, "YMH0021 Midi Interface"},	/* YMH0021 */
	{0x80719304, "ADS7180 Midi Interface"},	/* ADS7180 */
	{0x0300561e, "GRV0003 Midi Interface"},	/* GRV0003 */
#endif
};

/* These are the synthesizer and the midi interface information. */
static struct synth_info mpu_synthinfo = {
	"MPU401 MIDI",
	0,
	SYNTH_TYPE_MIDI,
	0,
	0,
	128,
	128,
	128,
	SYNTH_CAP_INPUT,
};

static struct midi_info mpu_midiinfo = {
	"MPU401 MIDI",
	0,
	0,
	0,
};

/*
 * These functions goes into mpu_op_desc to get called
 * from sound.c.
 */

static int mpu_probe(device_t dev);
static int mpu_probe1(device_t dev);
static int mpu_probe2(device_t dev);
static int mpu_attach(device_t dev);
static int mpusbc_probe(device_t dev);
static int mpusbc_attach(device_t dev);

static d_ioctl_t mpu_ioctl;
static driver_intr_t mpu_intr;
static midi_callback_t mpu_callback;

/* Here is the parameter structure per a device. */
struct mpu_softc {
	device_t dev; /* device information */
	mididev_info *devinfo; /* midi device information */

	struct mtx mtx; /* Mutex to protect the device. */

	struct resource *io; /* Base of io port */
	int io_rid; /* Io resource ID */
	u_long irq_val; /* Irq value */
	struct resource *irq; /* Irq */
	int irq_rid; /* Irq resource ID */
	void *ih; /* Interrupt cookie */

	int fflags; /* File flags */
};

typedef struct mpu_softc *sc_p;

/* These functions are local. */
static void mpu_startplay(sc_p scp);
static void mpu_xmit(sc_p scp);
static int mpu_resetmode(sc_p scp);
static int mpu_uartmode(sc_p scp);
static int mpu_waitack(sc_p scp);
static int mpu_status(sc_p scp);
static int mpu_command(sc_p scp, u_int8_t value);
static int mpu_readdata(sc_p scp, u_int8_t *value);
static int mpu_writedata(sc_p scp, u_int8_t value);
static u_int mpu_readport(sc_p scp, int off);
static void mpu_writeport(sc_p scp, int off, u_int8_t value);
static int mpu_allocres(sc_p scp, device_t dev);
static void mpu_releaseres(sc_p scp, device_t dev);

/*
 * This is the device descriptor for the midi device.
 */
static mididev_info mpu_op_desc = {
	"MPU401 midi",

	SNDCARD_MPU401,

	NULL,
	NULL,
	mpu_ioctl,

	mpu_callback,

	MIDI_BUFFSIZE, /* Queue Length */

	0, /* XXX This is not an *audio* device! */
};

/*
 * Here are the main functions to interact to the user process.
 */

static int
mpu_probe(device_t dev)
{
	sc_p scp;
	int ret;

	/* Check isapnp ids */
	if (isa_get_logicalid(dev) != 0)
		return (ISA_PNP_PROBE(device_get_parent(dev), dev, mpu_ids));

	scp = device_get_softc(dev);

	device_set_desc(dev, mpu_op_desc.name);
	bzero(scp, sizeof(*scp));

	scp->io_rid = 0;
	ret = mpu_probe1(dev);
	if (ret != 0)
		return (ret);
	ret = mpu_probe2(dev);
	if (ret != 0)
		return (ret);

	return (0);
}

/*
 * Make sure this is an MPU401, not an 16550 uart.
 * Called only for non-pnp devices.
 */
static int
mpu_probe1(device_t dev)
{
	sc_p scp;
	int iir;
	struct resource *io;

	scp = device_get_softc(dev);

	/*
	 * If an MPU401 is ready to both input and output,
	 * the status register value is zero, which may
	 * confuse an 16550 uart to probe as an MPU401.
	 * We read the IIR (base + 2), which is not used
	 * by an MPU401.
	 */
	io = bus_alloc_resource(dev, SYS_RES_IOPORT, &scp->io_rid, 0, ~0, 3, RF_ACTIVE);
	iir = bus_space_read_1(rman_get_bustag(io), rman_get_bushandle(io), com_iir) & 0xff;
	bus_release_resource(dev, SYS_RES_IOPORT, scp->io_rid, io);
	if ((iir & ~(IIR_IMASK | IIR_FIFO_MASK)) == 0)
		/* Likely to be an 16550. */
		return (ENXIO);

	return (0);
}

/* Look up the irq. */
static int
mpu_probe2(device_t dev)
{
	sc_p scp;
	int unit, i;
	intrmask_t irqp0, irqp1;

	scp = device_get_softc(dev);
	unit = device_get_unit(dev);

	scp->io = bus_alloc_resource(dev, SYS_RES_IOPORT, &scp->io_rid, 0, ~0, 2, RF_ACTIVE);
	if (scp->io == NULL)
		return (ENXIO);

	MIDI_DEBUG(printf("mpu%d: probing.\n", unit));

	/* Reset the interface. */
	if (mpu_resetmode(scp) != 0 || mpu_waitack(scp) != 0) {
		printf("mpu%d: reset failed.\n", unit);
		mpu_releaseres(scp, dev);
		return (ENXIO);
	}

	/*
	 * At this point, we are likely to have an interface.
	 *
	 * Switching the interface to uart mode gives us an interrupt.
	 * We can make use of it to determine the irq.
	 * Idea-stolen-from: sys/isa/sio.c:sioprobe()
	 */

	critical_enter();

	/*
	 * See the initial irq. We have to do this now,
	 * otherwise a midi module/instrument might send
	 * an active sensing, to mess up the irq.
	 */
	irqp0 = isa_irq_pending();
	irqp1 = 0;

	/* Switch to uart mode. */
	if (mpu_uartmode(scp) != 0) {
		critical_exit();
		printf("mpu%d: mode switching failed.\n", unit);
		mpu_releaseres(scp, dev);
		return (ENXIO);
	}

	if (device_get_flags(dev) & MPU_DF_NO_IRQ) {
		irqp0 = irqp1 = 0;
		goto no_irq;
	}

	/* See which irq we have now. */
	for (i = 0 ; i < MPU_TRYDATA ; i++) {
		DELAY(MPU_DELAY);
		irqp1 = isa_irq_pending();
		if (irqp1 != irqp0)
			break;
	}
	if (irqp1 == irqp0) {
		critical_exit();
		printf("mpu%d: switching the mode gave no interrupt.\n", unit);
		mpu_releaseres(scp, dev);
		return (ENXIO);
	}

no_irq:
	/* Wait to see an ACK. */
	if (mpu_waitack(scp) != 0) {
		critical_exit();
		printf("mpu%d: not acked.\n", unit);
		mpu_releaseres(scp, dev);
		return (ENXIO);
	}

	critical_exit();

	if (device_get_flags(dev) & MPU_DF_NO_IRQ)
		scp->irq_val = 0;
	else
		/* We have found the irq. */
		scp->irq_val = ffs(~irqp0 & irqp1) - 1;

	MIDI_DEBUG(printf("mpu%d: probed.\n", unit));

	return (0);
}

static int
mpusbc_probe(device_t dev)
{
	char *s;
	sc_p scp;
	struct sndcard_func *func;

	/* The parent device has already been probed. */

	func = device_get_ivars(dev);
	if (func == NULL || func->func != SCF_MIDI)
		return (ENXIO);

	s = "SB Midi Interface";

	scp = device_get_softc(dev);
	bzero(scp, sizeof(*scp));
	scp->io_rid = 1;
	scp->irq_rid = 0;
	device_set_desc(dev, s);
	return (0);
}

static int
mpu_attach(device_t dev)
{
	sc_p scp;
	mididev_info *devinfo;

	scp = device_get_softc(dev);

	MIDI_DEBUG(printf("mpu: attaching.\n"));

	mtx_init(&scp->mtx, "mpumid", NULL, MTX_DEF);

	/* Allocate the resources, switch to uart mode. */
	if (mpu_allocres(scp, dev) || mpu_uartmode(scp)) {
		mpu_releaseres(scp, dev);
		mtx_destroy(&scp->mtx);
		return (ENXIO);
	}

	/* mpu_probe() has put the interface to uart mode. */

	/* Fill the softc. */
	scp->dev = dev;
	scp->devinfo = devinfo = create_mididev_info_unit(MDT_MIDI, &mpu_op_desc, &midisynth_op_desc);

	/* Fill the midi info. */
	if (scp->irq != NULL)
		snprintf(devinfo->midistat, sizeof(devinfo->midistat), "at 0x%x irq %d",
			 (u_int)rman_get_start(scp->io), (int)rman_get_start(scp->irq));
	else
		snprintf(devinfo->midistat, sizeof(devinfo->midistat), "at 0x%x",
			 (u_int)rman_get_start(scp->io));

	midiinit(devinfo, dev);

	/* Now we can handle the interrupts. */
	if (scp->irq != NULL)
		bus_setup_intr(dev, scp->irq, INTR_TYPE_AV, mpu_intr, scp,
			       &scp->ih);

	MIDI_DEBUG(printf("mpu: attached.\n"));

	return (0);
}

static int
mpusbc_attach(device_t dev)
{
	sc_p scp;
	int unit;

	scp = device_get_softc(dev);
	unit = device_get_unit(dev);

	mpu_attach(dev);

	return (0);
}

static int
mpu_ioctl(dev_t i_dev, u_long cmd, caddr_t arg, int mode, struct thread *td)
{
	sc_p scp;
	mididev_info *devinfo;
	int unit;
	struct synth_info *synthinfo;
	struct midi_info *midiinfo;

	unit = MIDIUNIT(i_dev);

	MIDI_DEBUG(printf("mpu_ioctl: unit %d, cmd %s.\n", unit, midi_cmdname(cmd, cmdtab_midiioctl)));

	devinfo = get_mididev_info(i_dev, &unit);
	if (devinfo == NULL) {
		MIDI_DEBUG(printf("mpu_ioctl: unit %d is not configured.\n", unit));
		return (ENXIO);
	}
	scp = devinfo->softc;

	switch (cmd) {
	case SNDCTL_SYNTH_INFO:
		synthinfo = (struct synth_info *)arg;
		if (synthinfo->device != unit)
			return (ENXIO);
		bcopy(&mpu_synthinfo, synthinfo, sizeof(mpu_synthinfo));
		synthinfo->device = unit;
		return (0);
		break;
	case SNDCTL_MIDI_INFO:
		midiinfo = (struct midi_info *)arg;
		if (midiinfo->device != unit)
			return (ENXIO);
		bcopy(&mpu_midiinfo, midiinfo, sizeof(mpu_midiinfo));
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
mpu_intr(void *arg)
{
	sc_p scp;
	u_char c;
	mididev_info *devinfo;
	int leni;

	scp = (sc_p)arg;
	devinfo = scp->devinfo;

	mtx_lock(&devinfo->flagqueue_mtx);
	mtx_lock(&scp->mtx);

	/* Read the received data. */
	while ((mpu_status(scp) & MPU_INPUTBUSY) == 0) {
		/* Receive the data. */
		mpu_readdata(scp, &c);
		mtx_unlock(&scp->mtx);
		/* Queue into the passthru buffer and start transmitting if we can. */
		if ((devinfo->flags & MIDI_F_PASSTHRU) != 0 && ((devinfo->flags & MIDI_F_BUSY) == 0 || (devinfo->fflags & FWRITE) == 0)) {
			midibuf_input_intr(&devinfo->midi_dbuf_passthru, &c, sizeof(c), &leni);
			devinfo->callback(devinfo, MIDI_CB_START | MIDI_CB_WR);
		}
		/* Queue if we are reading. Discard an active sensing. */
		if ((devinfo->flags & MIDI_F_READING) != 0 && c != 0xfe) {
			midibuf_input_intr(&devinfo->midi_dbuf_in, &c, sizeof(c), &leni);
		}
		mtx_lock(&scp->mtx);
	}
	mtx_unlock(&scp->mtx);
	mtx_unlock(&devinfo->flagqueue_mtx);

	/* Invoke the upper layer. */
	midi_intr(devinfo);
}

static int
mpu_callback(void *di, int reason)
{
	int unit;
	sc_p scp;
	mididev_info *d;

	d = (mididev_info *)di;

	mtx_assert(&d->flagqueue_mtx, MA_OWNED);

	if (d == NULL) {
		MIDI_DEBUG(printf("mpu_callback: device not configured.\n"));
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
			mpu_startplay(scp);
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
mpu_startplay(sc_p scp)
{
	mididev_info *devinfo;

	devinfo = scp->devinfo;

	mtx_assert(&devinfo->flagqueue_mtx, MA_OWNED);

	/* Can we play now? */
	if (devinfo->midi_dbuf_out.rl == 0)
		return;

	devinfo->flags |= MIDI_F_WRITING;
	mpu_xmit(scp);
}

static void
mpu_xmit(sc_p scp)
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
	while ((devinfo->flags & MIDI_F_WRITING) != 0) {
		if (dbuf->rl == 0) 
			break;
		else {
			mtx_lock(&scp->mtx);
			/* XXX Wait until we can write the data. */
			if ((mpu_status(scp) & MPU_OUTPUTBUSY) == 0) {
				/* Send the data. */
				midibuf_output_intr(dbuf, &c, sizeof(c), &leno);
				mpu_writedata(scp, c);
				/* We are playing now. */
				devinfo->flags |= MIDI_F_WRITING;
			}
			mtx_unlock(&scp->mtx);
		}
	}
	/* Stop playing. */
	devinfo->flags &= ~MIDI_F_WRITING;
}


/* 
 * Reset mpu.
 * The caller must lock scp->mtx before calling this function if needed.
 */
static int
mpu_resetmode(sc_p scp)
{
	int i, resp;

	/* Reset the mpu. */
	resp = 0;
	for (i = 0 ; i < MPU_TRYDATA ; i++) {
		resp = mpu_command(scp, MPU_RESET);
		if (resp == 0)
			break;
	}
	if (resp != 0)
		return (1);

	DELAY(MPU_DELAY);
	return (0);
}

/*
 * Switch to uart mode.
 * The caller must lock scp->mtx before calling this function if needed.
 */
static int
mpu_uartmode(sc_p scp)
{
	int i, resp;

	/* Switch to uart mode. */
	resp = 0;
	for (i = 0 ; i < MPU_TRYDATA ; i++) {
		resp = mpu_command(scp, MPU_UART);
		if (resp == 0)
			break;
	}
	if (resp != 0)
		return (1);

	DELAY(MPU_DELAY);
	return (0);
}

/*
 * Wait to see an ACK.
 * The caller must lock scp->mtx before calling this function if needed.
 */
static int
mpu_waitack(sc_p scp)
{
	int i;
	u_int8_t resp;

	resp = 0;
	for (i = 0 ; i < MPU_TRYDATA ; i++) {
		if (mpu_readdata(scp, &resp) == 0)
			break;
	}
	if (resp != MPU_ACK)
		return (1);

	DELAY(MPU_DELAY);
	return (0);
}

/* Reads the status. */
static int
mpu_status(sc_p scp)
{
	return mpu_readport(scp, MPU_STATPORT);
}

/* Writes a command. */
static int
mpu_command(sc_p scp, u_int8_t value)
{
	u_int status;

	/* Is the interface ready to write? */
	status = mpu_status(scp);
	if ((status & MPU_OUTPUTBUSY) != 0)
		/* The interface is busy. */
		return (EAGAIN);

	mpu_writeport(scp, MPU_CMDPORT, value);

	return (0);
}

/* Reads a byte of data. */
static int
mpu_readdata(sc_p scp, u_int8_t *value)
{
	u_int status;

	if (value == NULL)
		return (EINVAL);

	/* Is the interface ready to write? */
	status = mpu_status(scp);
	if ((status & MPU_INPUTBUSY) != 0)
		/* The interface is busy. */
		return (EAGAIN);

	*value = (u_int8_t)(mpu_readport(scp, MPU_DATAPORT) & 0xff);

	return (0);
}

/* Writes a byte of data. */
static int
mpu_writedata(sc_p scp, u_int8_t value)
{
	u_int status;

	/* Is the interface ready to write? */
	status = mpu_status(scp);
	if ((status & MPU_OUTPUTBUSY) != 0)
		/* The interface is busy. */
		return (EAGAIN);

	mpu_writeport(scp, MPU_DATAPORT, value);

	return (0);
}

/* Reads from a port. */
static u_int
mpu_readport(sc_p scp, int off)
{
	return bus_space_read_1(rman_get_bustag(scp->io), rman_get_bushandle(scp->io), off) & 0xff;
}

/* Writes to a port. */
static void
mpu_writeport(sc_p scp, int off, u_int8_t value)
{
	bus_space_write_1(rman_get_bustag(scp->io), rman_get_bushandle(scp->io), off, value);
}

/* Allocates resources. */
static int
mpu_allocres(sc_p scp, device_t dev)
{
	if (scp->io == NULL) {
		scp->io = bus_alloc_resource(dev, SYS_RES_IOPORT, &scp->io_rid, 0, ~0, 2, RF_ACTIVE);
		if (scp->io == NULL)
			return (1);
	}
	if (scp->irq == NULL && !(device_get_flags(dev) & MPU_DF_NO_IRQ)) {
		if (scp->irq_val == 0)
			scp->irq = bus_alloc_resource(dev, SYS_RES_IRQ, &scp->irq_rid, 0, ~0, 1, RF_ACTIVE | RF_SHAREABLE);
		else
			scp->irq = bus_alloc_resource(dev, SYS_RES_IRQ, &scp->irq_rid, scp->irq_val, scp->irq_val, 1, RF_ACTIVE | RF_SHAREABLE);
		if (scp->irq == NULL)
			return (1);
	}

	return (0);
}

/* Releases resources. */
static void
mpu_releaseres(sc_p scp, device_t dev)
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

static device_method_t mpu_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe , mpu_probe ),
	DEVMETHOD(device_attach, mpu_attach),

	{ 0, 0 },
};

static driver_t mpu_driver = {
	"midi",
	mpu_methods,
	sizeof(struct mpu_softc),
};

DRIVER_MODULE(mpu, isa, mpu_driver, midi_devclass, 0, 0);

static device_method_t mpusbc_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe , mpusbc_probe ),
	DEVMETHOD(device_attach, mpusbc_attach),

	{ 0, 0 },
};

static driver_t mpusbc_driver = {
	"midi",
	mpusbc_methods,
	sizeof(struct mpu_softc),
};

DRIVER_MODULE(mpusbc, sbc, mpusbc_driver, midi_devclass, 0, 0);
