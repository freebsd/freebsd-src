/*
 * Copyright (c) 1999 Seigo Tanimura
 * All rights reserved.
 *
 * Portions of this source are based on cwcealdr.cpp and dhwiface.cpp in
 * cwcealdr1.zip, the sample sources by Crystal Semiconductor.
 * Copyright (c) 1996-1998 Crystal Semiconductor Corp.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <machine/resource.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <sys/soundcard.h>
#include <dev/sound/pcm/sound.h>
#include <dev/sound/chip.h>
#include <dev/sound/pci/csareg.h>
#include <dev/sound/pci/csavar.h>

#include <pci/pcireg.h>
#include <pci/pcivar.h>

#include <dev/sound/pci/csaimg.h>

/* Here is the parameter structure per a device. */
struct csa_softc {
	device_t dev; /* device */
	csa_res res; /* resources */

	device_t pcm; /* pcm device */
	driver_intr_t* pcmintr; /* pcm intr */
	void *pcmintr_arg; /* pcm intr arg */
	device_t midi; /* midi device */
	driver_intr_t* midiintr; /* midi intr */
	void *midiintr_arg; /* midi intr arg */
	void *ih; /* cookie */

	struct csa_bridgeinfo binfo; /* The state of this bridge. */
};

typedef struct csa_softc *sc_p;

static int csa_probe(device_t dev);
static int csa_attach(device_t dev);
static struct resource *csa_alloc_resource(device_t bus, device_t child, int type, int *rid,
					      u_long start, u_long end, u_long count, u_int flags);
static int csa_release_resource(device_t bus, device_t child, int type, int rid,
				   struct resource *r);
static int csa_setup_intr(device_t bus, device_t child,
			  struct resource *irq, int flags,
			  driver_intr_t *intr, void *arg, void **cookiep);
static int csa_teardown_intr(device_t bus, device_t child,
			     struct resource *irq, void *cookie);
static driver_intr_t csa_intr;
static int csa_initialize(sc_p scp);
static void csa_resetdsp(csa_res *resp);
static int csa_downloadimage(csa_res *resp);
static int csa_transferimage(csa_res *resp, u_long *src, u_long dest, u_long len);

static devclass_t csa_devclass;

static int
csa_probe(device_t dev)
{
	char *s;

	s = NULL;
	switch (pci_get_devid(dev)) {
	case CS4610_PCI_ID:
		s = "Crystal Semiconductor CS4610/4611 Audio accelerator";
		break;
	case CS4614_PCI_ID:
		s = "Crystal Semiconductor CS4614/4622/4624 Audio accelerator/4280 Audio controller";
		break;
	case CS4615_PCI_ID:
		s = "Crystal Semiconductor CS4615 Audio accelerator";
		break;
	}

	if (s != NULL) {
		device_set_desc(dev, s);
		return (0);
	}

	return (ENXIO);
}

static int
csa_attach(device_t dev)
{
	u_int32_t stcmd;
	sc_p scp;
	csa_res *resp;
	struct sndcard_func *func;

	scp = device_get_softc(dev);

	/* Fill in the softc. */
	bzero(scp, sizeof(*scp));
	scp->dev = dev;

	/* Wake up the device. */
	stcmd = pci_read_config(dev, PCIR_COMMAND, 2);
	if ((stcmd & PCIM_CMD_MEMEN) == 0 || (stcmd & PCIM_CMD_BUSMASTEREN) == 0) {
		stcmd |= (PCIM_CMD_MEMEN | PCIM_CMD_BUSMASTEREN);
		pci_write_config(dev, PCIR_COMMAND, stcmd, 2);
	}

	/* Allocate the resources. */
	resp = &scp->res;
	resp->io_rid = CS461x_IO_OFFSET;
	resp->io = bus_alloc_resource(dev, SYS_RES_MEMORY, &resp->io_rid, 0, ~0, CS461x_IO_SIZE, RF_ACTIVE);
	if (resp->io == NULL)
		return (ENXIO);
	resp->mem_rid = CS461x_MEM_OFFSET;
	resp->mem = bus_alloc_resource(dev, SYS_RES_MEMORY, &resp->mem_rid, 0, ~0, CS461x_MEM_SIZE, RF_ACTIVE);
	if (resp->mem == NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY, resp->io_rid, resp->io);
		return (ENXIO);
	}
	resp->irq_rid = 0;
	resp->irq = bus_alloc_resource(dev, SYS_RES_IRQ, &resp->irq_rid, 0, ~0, 1, RF_ACTIVE | RF_SHAREABLE);
	if (resp->irq == NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY, resp->io_rid, resp->io);
		bus_release_resource(dev, SYS_RES_MEMORY, resp->mem_rid, resp->mem);
		return (ENXIO);
	}

	/* Enable interrupt. */
	if (bus_setup_intr(dev, resp->irq, INTR_TYPE_TTY, csa_intr, scp, &scp->ih)) {
		bus_release_resource(dev, SYS_RES_MEMORY, resp->io_rid, resp->io);
		bus_release_resource(dev, SYS_RES_MEMORY, resp->mem_rid, resp->mem);
		bus_release_resource(dev, SYS_RES_IRQ, resp->irq_rid, resp->irq);
		return (ENXIO);
	}
	if ((csa_readio(resp, BA0_HISR) & HISR_INTENA) == 0)
		csa_writeio(resp, BA0_HICR, HICR_IEV | HICR_CHGM);

	/* Initialize the chip. */
	if (csa_initialize(scp)) {
		bus_release_resource(dev, SYS_RES_MEMORY, resp->io_rid, resp->io);
		bus_release_resource(dev, SYS_RES_MEMORY, resp->mem_rid, resp->mem);
		bus_release_resource(dev, SYS_RES_IRQ, resp->irq_rid, resp->irq);
		return (ENXIO);
	}

	/* Reset the Processor. */
	csa_resetdsp(resp);

	/* Download the Processor Image to the processor. */
	if (csa_downloadimage(resp)) {
		bus_release_resource(dev, SYS_RES_MEMORY, resp->io_rid, resp->io);
		bus_release_resource(dev, SYS_RES_MEMORY, resp->mem_rid, resp->mem);
		bus_release_resource(dev, SYS_RES_IRQ, resp->irq_rid, resp->irq);
		return (ENXIO);
	}

	/* Attach the children. */

	/* PCM Audio */
	func = malloc(sizeof(struct sndcard_func), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (func == NULL)
		return (ENOMEM);
	func->varinfo = &scp->binfo;
	func->func = SCF_PCM;
	scp->pcm = device_add_child(dev, "pcm", -1);
	device_set_ivars(scp->pcm, func);

	/* Midi Interface */
	func = malloc(sizeof(struct sndcard_func), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (func == NULL)
		return (ENOMEM);
	func->varinfo = &scp->binfo;
	func->func = SCF_MIDI;
	scp->midi = device_add_child(dev, "midi", -1);
	device_set_ivars(scp->midi, func);

	bus_generic_attach(dev);

	return (0);
}

static struct resource *
csa_alloc_resource(device_t bus, device_t child, int type, int *rid,
		      u_long start, u_long end, u_long count, u_int flags)
{
	sc_p scp;
	csa_res *resp;
	struct resource *res;

	scp = device_get_softc(bus);
	resp = &scp->res;
	switch (type) {
	case SYS_RES_IRQ:
		if (*rid != 0)
			return (NULL);
		res = resp->irq;
		break;
	case SYS_RES_MEMORY:
		switch (*rid) {
		case CS461x_IO_OFFSET:
			res = resp->io;
			break;
		case CS461x_MEM_OFFSET:
			res = resp->mem;
			break;
		default:
			return (NULL);
		}
		break;
	default:
		return (NULL);
	}

	return res;
}

static int
csa_release_resource(device_t bus, device_t child, int type, int rid,
			struct resource *r)
{
	return (0);
}

/*
 * The following three functions deal with interrupt handling.
 * An interrupt is primarily handled by the bridge driver.
 * The bridge driver then determines the child devices to pass
 * the interrupt. Certain information of the device can be read
 * only once(eg the value of HISR). The bridge driver is responsible
 * to pass such the information to the children.
 */

static int
csa_setup_intr(device_t bus, device_t child,
	       struct resource *irq, int flags,
	       driver_intr_t *intr, void *arg, void **cookiep)
{
	sc_p scp;
	csa_res *resp;
	struct sndcard_func *func;

	scp = device_get_softc(bus);
	resp = &scp->res;

	/*
	 * Look at the function code of the child to determine
	 * the appropriate hander for it.
	 */
	func = device_get_ivars(child);
	if (func == NULL || irq != resp->irq)
		return (EINVAL);

	switch (func->func) {
	case SCF_PCM:
		scp->pcmintr = intr;
		scp->pcmintr_arg = arg;
		break;

	case SCF_MIDI:
		scp->midiintr = intr;
		scp->midiintr_arg = arg;
		break;

	default:
		return (EINVAL);
	}
	*cookiep = scp;
	if ((csa_readio(resp, BA0_HISR) & HISR_INTENA) == 0)
		csa_writeio(resp, BA0_HICR, HICR_IEV | HICR_CHGM);

	return (0);
}

static int
csa_teardown_intr(device_t bus, device_t child,
		  struct resource *irq, void *cookie)
{
	sc_p scp;
	csa_res *resp;
	struct sndcard_func *func;

	scp = device_get_softc(bus);
	resp = &scp->res;

	/*
	 * Look at the function code of the child to determine
	 * the appropriate hander for it.
	 */
	func = device_get_ivars(child);
	if (func == NULL || irq != resp->irq || cookie != scp)
		return (EINVAL);

	switch (func->func) {
	case SCF_PCM:
		scp->pcmintr = NULL;
		scp->pcmintr_arg = NULL;
		break;

	case SCF_MIDI:
		scp->midiintr = NULL;
		scp->midiintr_arg = NULL;
		break;

	default:
		return (EINVAL);
	}

	return (0);
}

/* The interrupt handler */
static void
csa_intr(void *arg)
{
	sc_p scp = arg;
	csa_res *resp;
	u_int32_t hisr;

	resp = &scp->res;

	/* Is this interrupt for us? */
	hisr = csa_readio(resp, BA0_HISR);
	if ((hisr & ~HISR_INTENA) == 0) {
		/* Throw an eoi. */
		csa_writeio(resp, BA0_HICR, HICR_IEV | HICR_CHGM);
		return;
	}

	/*
	 * Pass the value of HISR via struct csa_bridgeinfo.
	 * The children get access through their ivars.
	 */
	scp->binfo.hisr = hisr;

	/* Invoke the handlers of the children. */
	if ((hisr & (HISR_VC0 | HISR_VC1)) != 0 && scp->pcmintr != NULL)
		scp->pcmintr(scp->pcmintr_arg);
	if ((hisr & HISR_MIDI) != 0 && scp->midiintr != NULL)
		scp->midiintr(scp->midiintr_arg);

	/* Throw an eoi. */
	csa_writeio(resp, BA0_HICR, HICR_IEV | HICR_CHGM);
}

static int
csa_initialize(sc_p scp)
{
	int i;
	u_int32_t acsts, acisv;
	csa_res *resp;

	resp = &scp->res;

	/*
	 * First, blast the clock control register to zero so that the PLL starts
	 * out in a known state, and blast the master serial port control register
	 * to zero so that the serial ports also start out in a known state.
	 */
	csa_writeio(resp, BA0_CLKCR1, 0);
	csa_writeio(resp, BA0_SERMC1, 0);

	/*
	 * If we are in AC97 mode, then we must set the part to a host controlled
	 * AC-link.  Otherwise, we won't be able to bring up the link.
	 */
#if 1
	csa_writeio(resp, BA0_SERACC, SERACC_HSP | SERACC_CODEC_TYPE_1_03); /* 1.03 codec */
#else
	csa_writeio(resp, BA0_SERACC, SERACC_HSP | SERACC_CODEC_TYPE_2_0); /* 2.0 codec */
#endif /* 1 */

	/*
	 * Drive the ARST# pin low for a minimum of 1uS (as defined in the AC97
	 * spec) and then drive it high.  This is done for non AC97 modes since
	 * there might be logic external to the CS461x that uses the ARST# line
	 * for a reset.
	 */
	csa_writeio(resp, BA0_ACCTL, 0);
	DELAY(100);
	csa_writeio(resp, BA0_ACCTL, ACCTL_RSTN);

	/*
	 * The first thing we do here is to enable sync generation.  As soon
	 * as we start receiving bit clock, we'll start producing the SYNC
	 * signal.
	 */
	csa_writeio(resp, BA0_ACCTL, ACCTL_ESYN | ACCTL_RSTN);

	/*
	 * Now wait for a short while to allow the AC97 part to start
	 * generating bit clock (so we don't try to start the PLL without an
	 * input clock).
	 */
	DELAY(50000);

	/*
	 * Set the serial port timing configuration, so that
	 * the clock control circuit gets its clock from the correct place.
	 */
	csa_writeio(resp, BA0_SERMC1, SERMC1_PTC_AC97);

	/*
	 * Write the selected clock control setup to the hardware.  Do not turn on
	 * SWCE yet (if requested), so that the devices clocked by the output of
	 * PLL are not clocked until the PLL is stable.
	 */
	csa_writeio(resp, BA0_PLLCC, PLLCC_LPF_1050_2780_KHZ | PLLCC_CDR_73_104_MHZ);
	csa_writeio(resp, BA0_PLLM, 0x3a);
	csa_writeio(resp, BA0_CLKCR2, CLKCR2_PDIVS_8);

	/*
	 * Power up the PLL.
	 */
	csa_writeio(resp, BA0_CLKCR1, CLKCR1_PLLP);

	/*
	 * Wait until the PLL has stabilized.
	 */
	DELAY(50000);

	/*
	 * Turn on clocking of the core so that we can setup the serial ports.
	 */
	csa_writeio(resp, BA0_CLKCR1, csa_readio(resp, BA0_CLKCR1) | CLKCR1_SWCE);

	/*
	 * Fill the serial port FIFOs with silence.
	 */
	csa_clearserialfifos(resp);

	/*
	 * Set the serial port FIFO pointer to the first sample in the FIFO.
	 */
#if notdef
	csa_writeio(resp, BA0_SERBSP, 0);
#endif /* notdef */

	/*
	 *  Write the serial port configuration to the part.  The master
	 *  enable bit is not set until all other values have been written.
	 */
	csa_writeio(resp, BA0_SERC1, SERC1_SO1F_AC97 | SERC1_SO1EN);
	csa_writeio(resp, BA0_SERC2, SERC2_SI1F_AC97 | SERC1_SO1EN);
	csa_writeio(resp, BA0_SERMC1, SERMC1_PTC_AC97 | SERMC1_MSPE);

	/*
	 * Wait for the codec ready signal from the AC97 codec.
	 */
	acsts = 0;
	for (i = 0 ; i < 1000 ; i++) {
		/*
		 * First, lets wait a short while to let things settle out a bit,
		 * and to prevent retrying the read too quickly.
		 */
		DELAY(125);

		/*
		 * Read the AC97 status register to see if we've seen a CODEC READY
		 * signal from the AC97 codec.
		 */
		acsts = csa_readio(resp, BA0_ACSTS);
		if ((acsts & ACSTS_CRDY) != 0)
			break;
	}

	/*
	 * Make sure we sampled CODEC READY.
	 */
	if ((acsts & ACSTS_CRDY) == 0)
		return (ENXIO);

	/*
	 * Assert the vaid frame signal so that we can start sending commands
	 * to the AC97 codec.
	 */
	csa_writeio(resp, BA0_ACCTL, ACCTL_VFRM | ACCTL_ESYN | ACCTL_RSTN);

	/*
	 * Wait until we've sampled input slots 3 and 4 as valid, meaning that
	 * the codec is pumping ADC data across the AC-link.
	 */
	acisv = 0;
	for (i = 0 ; i < 1000 ; i++) {
		/*
		 * First, lets wait a short while to let things settle out a bit,
		 * and to prevent retrying the read too quickly.
		 */
#if notdef
		DELAY(10000000L); /* clw */
#else
		DELAY(1000);
#endif /* notdef */
		/*
		 * Read the input slot valid register and see if input slots 3 and
		 * 4 are valid yet.
		 */
		acisv = csa_readio(resp, BA0_ACISV);
		if ((acisv & (ACISV_ISV3 | ACISV_ISV4)) == (ACISV_ISV3 | ACISV_ISV4))
			break;
	}
	/*
	 * Make sure we sampled valid input slots 3 and 4.  If not, then return
	 * an error.
	 */
	if ((acisv & (ACISV_ISV3 | ACISV_ISV4)) != (ACISV_ISV3 | ACISV_ISV4))
		return (ENXIO);

	/*
	 * Now, assert valid frame and the slot 3 and 4 valid bits.  This will
	 * commense the transfer of digital audio data to the AC97 codec.
	 */
	csa_writeio(resp, BA0_ACOSV, ACOSV_SLV3 | ACOSV_SLV4);

	/*
	 * Power down the DAC and ADC.  We will power them up (if) when we need
	 * them.
	 */
#if notdef
	csa_writeio(resp, BA0_AC97_POWERDOWN, 0x300);
#endif /* notdef */

	/*
	 * Turn off the Processor by turning off the software clock enable flag in
	 * the clock control register.
	 */
#if notdef
	clkcr1 = csa_readio(resp, BA0_CLKCR1) & ~CLKCR1_SWCE;
	csa_writeio(resp, BA0_CLKCR1, clkcr1);
#endif /* notdef */

	/*
	 * Enable interrupts on the part.
	 */
#if notdef
	csa_writeio(resp, BA0_HICR, HICR_IEV | HICR_CHGM);
#endif /* notdef */

	return (0);
}

void
csa_clearserialfifos(csa_res *resp)
{
	int i, j, pwr;
	u_int8_t clkcr1, serbst;

	/*
	 * See if the devices are powered down.  If so, we must power them up first
	 * or they will not respond.
	 */
	pwr = 1;
	clkcr1 = csa_readio(resp, BA0_CLKCR1);
	if ((clkcr1 & CLKCR1_SWCE) == 0) {
		csa_writeio(resp, BA0_CLKCR1, clkcr1 | CLKCR1_SWCE);
		pwr = 0;
	}

	/*
	 * We want to clear out the serial port FIFOs so we don't end up playing
	 * whatever random garbage happens to be in them.  We fill the sample FIFOs
	 * with zero (silence).
	 */
	csa_writeio(resp, BA0_SERBWP, 0);

	/* Fill all 256 sample FIFO locations. */
	serbst = 0;
	for (i = 0 ; i < 256 ; i++) {
		/* Make sure the previous FIFO write operation has completed. */
		for (j = 0 ; j < 5 ; j++) {
			DELAY(100);
			serbst = csa_readio(resp, BA0_SERBST);
			if ((serbst & SERBST_WBSY) == 0)
				break;
		}
		if ((serbst & SERBST_WBSY) != 0) {
			if (!pwr)
				csa_writeio(resp, BA0_CLKCR1, clkcr1);
		}
		/* Write the serial port FIFO index. */
		csa_writeio(resp, BA0_SERBAD, i);
		/* Tell the serial port to load the new value into the FIFO location. */
		csa_writeio(resp, BA0_SERBCM, SERBCM_WRC);
	}
	/*
	 *  Now, if we powered up the devices, then power them back down again.
	 *  This is kinda ugly, but should never happen.
	 */
	if (!pwr)
		csa_writeio(resp, BA0_CLKCR1, clkcr1);
}

static void
csa_resetdsp(csa_res *resp)
{
	int i;

	/*
	 * Write the reset bit of the SP control register.
	 */
	csa_writemem(resp, BA1_SPCR, SPCR_RSTSP);

	/*
	 * Write the control register.
	 */
	csa_writemem(resp, BA1_SPCR, SPCR_DRQEN);

	/*
	 * Clear the trap registers.
	 */
	for (i = 0 ; i < 8 ; i++) {
		csa_writemem(resp, BA1_DREG, DREG_REGID_TRAP_SELECT + i);
		csa_writemem(resp, BA1_TWPR, 0xffff);
	}
	csa_writemem(resp, BA1_DREG, 0);

	/*
	 * Set the frame timer to reflect the number of cycles per frame.
	 */
	csa_writemem(resp, BA1_FRMT, 0xadf);
}

static int
csa_downloadimage(csa_res *resp)
{
	int ret;
	u_long ul, offset;

	for (ul = 0, offset = 0 ; ul < INKY_MEMORY_COUNT ; ul++) {
		/*
		 * DMA this block from host memory to the appropriate
		 * memory on the CSDevice.
		 */
		ret = csa_transferimage(
			resp,
			BA1Struct.BA1Array + offset,
			BA1Struct.MemoryStat[ul].ulDestByteOffset,
			BA1Struct.MemoryStat[ul].ulSourceByteSize);
		if (ret)
			return (ret);
		offset += BA1Struct.MemoryStat[ul].ulSourceByteSize >> 2;
	}

	return (0);
}

static int
csa_transferimage(csa_res *resp, u_long *src, u_long dest, u_long len)
{
	u_long ul;

	/*
	 * We do not allow DMAs from host memory to host memory (although the DMA
	 * can do it) and we do not allow DMAs which are not a multiple of 4 bytes
	 * in size (because that DMA can not do that).  Return an error if either
	 * of these conditions exist.
	 */
	if ((len & 0x3) != 0)
		return (EINVAL);

	/* Check the destination address that it is a multiple of 4 */
	if ((dest & 0x3) != 0)
		return (EINVAL);

	/* Write the buffer out. */
	for (ul = 0 ; ul < len ; ul += 4)
		csa_writemem(resp, dest + ul, src[ul >> 2]);

	return (0);
}

int
csa_readcodec(csa_res *resp, u_long offset, u_int32_t *data)
{
	int i;
	u_int32_t acsda, acctl, acsts;

	/*
	 * Make sure that there is not data sitting around from a previous
	 * uncompleted access. ACSDA = Status Data Register = 47Ch
	 */
	acsda = csa_readio(resp, BA0_ACSDA);

	/*
	 * Setup the AC97 control registers on the CS461x to send the
	 * appropriate command to the AC97 to perform the read.
	 * ACCAD = Command Address Register = 46Ch
	 * ACCDA = Command Data Register = 470h
	 * ACCTL = Control Register = 460h
	 * set DCV - will clear when process completed
	 * set CRW - Read command
	 * set VFRM - valid frame enabled
	 * set ESYN - ASYNC generation enabled
	 * set RSTN - ARST# inactive, AC97 codec not reset
	 */

	/*
	 * Get the actual AC97 register from the offset
	 */
	csa_writeio(resp, BA0_ACCAD, offset - BA0_AC97_RESET);
	csa_writeio(resp, BA0_ACCDA, 0);
	csa_writeio(resp, BA0_ACCTL, ACCTL_DCV | ACCTL_CRW | ACCTL_VFRM | ACCTL_ESYN | ACCTL_RSTN);

	/*
	 * Wait for the read to occur.
	 */
	acctl = 0;
	for (i = 0 ; i < 10 ; i++) {
		/*
		 * First, we want to wait for a short time.
		 */
		DELAY(25);

		/*
		 * Now, check to see if the read has completed.
		 * ACCTL = 460h, DCV should be reset by now and 460h = 17h
		 */
		acctl = csa_readio(resp, BA0_ACCTL);
		if ((acctl & ACCTL_DCV) == 0)
			break;
	}

	/*
	 * Make sure the read completed.
	 */
	if ((acctl & ACCTL_DCV) != 0)
		return (EAGAIN);

	/*
	 * Wait for the valid status bit to go active.
	 */
	acsts = 0;
	for (i = 0 ; i < 10 ; i++) {
		/*
		 * Read the AC97 status register.
		 * ACSTS = Status Register = 464h
		 */
		acsts = csa_readio(resp, BA0_ACSTS);
		/*
		 * See if we have valid status.
		 * VSTS - Valid Status
		 */
		if ((acsts & ACSTS_VSTS) != 0)
			break;
		/*
		 * Wait for a short while.
		 */
		 DELAY(25);
	}

	/*
	 * Make sure we got valid status.
	 */
	if ((acsts & ACSTS_VSTS) == 0)
		return (EAGAIN);

	/*
	 * Read the data returned from the AC97 register.
	 * ACSDA = Status Data Register = 474h
	 */
	*data = csa_readio(resp, BA0_ACSDA);

	return (0);
}

int
csa_writecodec(csa_res *resp, u_long offset, u_int32_t data)
{
	int i;
	u_int32_t acctl;

	/*
	 * Setup the AC97 control registers on the CS461x to send the
	 * appropriate command to the AC97 to perform the write.
	 * ACCAD = Command Address Register = 46Ch
	 * ACCDA = Command Data Register = 470h
	 * ACCTL = Control Register = 460h
	 * set DCV - will clear when process completed
	 * set VFRM - valid frame enabled
	 * set ESYN - ASYNC generation enabled
	 * set RSTN - ARST# inactive, AC97 codec not reset
	 */

	/*
	 * Get the actual AC97 register from the offset
	 */
	csa_writeio(resp, BA0_ACCAD, offset - BA0_AC97_RESET);
	csa_writeio(resp, BA0_ACCDA, data);
	csa_writeio(resp, BA0_ACCTL, ACCTL_DCV | ACCTL_VFRM | ACCTL_ESYN | ACCTL_RSTN);

	/*
	 * Wait for the write to occur.
	 */
	acctl = 0;
	for (i = 0 ; i < 10 ; i++) {
		/*
		 * First, we want to wait for a short time.
		 */
		DELAY(25);

		/*
		 * Now, check to see if the read has completed.
		 * ACCTL = 460h, DCV should be reset by now and 460h = 17h
		 */
		acctl = csa_readio(resp, BA0_ACCTL);
		if ((acctl & ACCTL_DCV) == 0)
			break;
	}

	/*
	 * Make sure the write completed.
	 */
	if ((acctl & ACCTL_DCV) != 0)
		return (EAGAIN);

	return (0);
}

u_int32_t
csa_readio(csa_res *resp, u_long offset)
{
	u_int32_t ul;

	if (offset < BA0_AC97_RESET)
		return bus_space_read_4(rman_get_bustag(resp->io), rman_get_bushandle(resp->io), offset) & 0xffffffff;
	else {
		if (csa_readcodec(resp, offset, &ul))
			ul = 0;
		return (ul);
	}
}

void
csa_writeio(csa_res *resp, u_long offset, u_int32_t data)
{
	if (offset < BA0_AC97_RESET)
		bus_space_write_4(rman_get_bustag(resp->io), rman_get_bushandle(resp->io), offset, data);
	else
		csa_writecodec(resp, offset, data);
}

u_int32_t
csa_readmem(csa_res *resp, u_long offset)
{
	return bus_space_read_4(rman_get_bustag(resp->mem), rman_get_bushandle(resp->mem), offset) & 0xffffffff;
}

void
csa_writemem(csa_res *resp, u_long offset, u_int32_t data)
{
	bus_space_write_4(rman_get_bustag(resp->mem), rman_get_bushandle(resp->mem), offset, data);
}

static device_method_t csa_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		csa_probe),
	DEVMETHOD(device_attach,	csa_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_alloc_resource,	csa_alloc_resource),
	DEVMETHOD(bus_release_resource,	csa_release_resource),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_setup_intr,	csa_setup_intr),
	DEVMETHOD(bus_teardown_intr,	csa_teardown_intr),

	{ 0, 0 }
};

static driver_t csa_driver = {
	"csa",
	csa_methods,
	sizeof(struct csa_softc),
};

/*
 * csa can be attached to a pci bus.
 */
DRIVER_MODULE(snd_csa, pci, csa_driver, csa_devclass, 0, 0);
MODULE_DEPEND(snd_csa, snd_pcm, PCM_MINVER, PCM_PREFVER, PCM_MAXVER);
MODULE_VERSION(snd_csa, 1);
