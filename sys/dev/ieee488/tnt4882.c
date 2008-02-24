/*-
 * Copyright (c) 2005 Poul-Henning Kamp
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
 * $FreeBSD: src/sys/dev/ieee488/tnt4882.c,v 1.3 2007/02/23 19:30:55 imp Exp $
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/stdarg.h>
#include <sys/rman.h>

/* vtophys */
#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/pmap.h>

#define UPD7210_HW_DRIVER 1
#include <dev/ieee488/upd7210.h>

struct tnt_softc {
	int foo;
	struct upd7210		upd7210;

	struct resource		*res[3];
	void			*intr_handler;
};

static struct resource_spec tnt_res_spec[] = {
	{ SYS_RES_MEMORY,	PCIR_BAR(0),	RF_ACTIVE},
	{ SYS_RES_MEMORY,	PCIR_BAR(1),	RF_ACTIVE},
	{ SYS_RES_IRQ,		0,		RF_ACTIVE | RF_SHAREABLE},
	{ -1, 0 }
};

enum tnt4882reg {
	dir = 0x00,
	cdor = 0x00,
	isr1 = 0x02,
	imr1 = 0x02,
	isr2 = 0x04,
	imr2 = 0x04,
	accwr = 0x05,
	spsr = 0x06,
	spmr = 0x06,
	intr = 0x07,
	adsr = 0x08,
	admr = 0x08,
	cnt2 = 0x09,
	cptr = 0x0a,
	auxmr = 0x0a,
	tauxcr = 0x0a,	/* 9914 mode register */
	cnt3 = 0x0b,
	adr0 = 0x0c,
	adr = 0x0c,
	hssel = 0x0d,
	adr1 = 0x0e,
	eosr = 0x0e,
	sts1 = 0x10,
	cfg = 0x10,
	dsr = 0x11,
	sh_cnt = 0x11,
	imr3 = 0x12,
	hier = 0x13,
	cnt0 = 0x14,
	misc = 0x15,
	cnt1 = 0x16,
	csr = 0x17,
	keyreg = 0x17,
	fifob = 0x18,
	fifoa = 0x19,
	isr3 = 0x1a,
	ccr = 0x1a,
	sasr = 0x1b,
	dcr = 0x1b,
	sts2 = 0x1c,
	cmdr = 0x1c,
	isr0 = 0x1d,
	imr0 = 0x1d,
	timer = 0x1e,
	bsr = 0x1f,
	bcr = 0x1f
};

struct tst {
	enum {RD, WT, xDELAY, END}
				action;
	enum tnt4882reg 	reg;
	uint8_t			val;
};

/*
 * From NI Application note 095:
 *   Writing Functional Self-Tests for the TNT4882 GPIB Interface Chip
 * XXX: fill in the rest ?
 */
static struct tst tst_reset[] = {
	{WT, tauxcr, 0x80},	/* chip reset if in 9914 mode */
	{WT, auxmr, 0x80},	/* swrst if swapped */ 
	{WT, tauxcr, 0x99},	/* switch to 7210 mode */
	{WT, auxmr, 0x99},	/* switch to 7210 mode if swapped */ 
	{WT, auxmr, 0x02},	/* execute chip reset */
	{WT, keyreg, 0x00},	/* important! clear the swap bit */
	{WT, eosr, 0x00},	/* clear EOS register */
	{WT, cdor, 0x00},	/* clear data lines */
	{WT, imr1, 0x00},	/* disable all interrupts */
	{WT, imr2, 0x00},
	{WT, imr0, 0x80},
	{WT, adr, 0x80},
	{WT, adr, 0x00},
	{WT, admr, 0x00},	/* clear addressing modes */
	{WT, auxmr, 0x00},	/* release from idle state with pon */
	{WT, auxmr, 0x60},	/* reset ppr */
	{WT, bcr, 0x00},	/* reset bcr */
	{WT, misc, 0x04},	/* set wrap plug bit */
	{WT, cmdr, 0xB2},	/* issue soft reset */
	{WT, hssel, 0x00},	/* select two-chip mode */
	{END, 0, 0}
};

static struct tst tst_read_reg[] = {
	{RD, isr1, 0x00},	/* Verify mask registers are clear */
	{RD, isr2, 0x00},
	{RD, adsr, 0x40},	/* Verify ATN is not asserted */
	{RD, adr0, 0x00},	/* Verify Primary address not set */
	{RD, adr1, 0x00},	/* Verify Secondary address not set */
	{RD, sts1, 0x8B},	/* Verify DONE, STOP, HALT, and GSYNC set */
	{RD, isr3, 0x19},	/* Verify STOP, Not Full FIFO, & DONE set */
	{RD, sts2, 0x9A},	/* Verify FIFO A/B is empty */
	{RD, sasr, 0x00},	/* Verify clear */
	{RD, isr0, 0x01},	/* Verify SYNC bit is set */
	{END, 0, 0}
};

static struct tst tst_bsr_dcr[] = {
	{WT, bcr, 0x55},	/* Set DAV, NRFD, SRQ, and REN */
	{WT, dcr, 0xAA},	/* Write pattern to GPIB data lines */
	{RD, bsr, 0x55},	/* Verify DAV, NRFD, SRQ, and REN are set */
	{RD, dsr, 0xAA},	/* Verify data pattern written previously */
	{WT, bcr, 0xAA},	/* Set ATN, NDAC, EOI, & IFC */
	{WT, dcr, 0x55},	/* Write pattern to GPIB data lines */
	{RD, bsr, 0xAA},	/* Verify ATN, NDAC, EOI, & IFC are set */
	{RD, dsr, 0x55},	/* Verify data pattern written previously */
	{WT, bcr, 0x00},	/* Clear control lines */
	{WT, dcr, 0x00},	/* Clear data lines */
	{RD, bsr, 0x00},	/* Verify control lines are clear */
	{RD, dsr, 0x00},	/* Verify data lines are clear */
	{END, 0, 0}
};

static struct tst tst_adr0_1[] = {
	{WT, adr, 0x55},	/* Set Primary talk address */
	{WT, adr, 0xAA},	/* Set Secondary listen address */
	{RD, adr0, 0x55},	/* Read Primary address */
	{RD, adr1, 0x2A},	/* Read Secondary address */
	{WT, adr, 0x2A},	/* Set Primay listen address */
	{WT, adr, 0xD5},	/* Set Secondary talk address */
	{RD, adr0, 0x2A},	/* Read Primary address */
	{RD, adr1, 0x55},	/* Read Secondary address */
	{END, 0, 0}
};

static struct tst tst_cdor_dir[] = {
	{WT, admr, 0xF0},	/* program AT-GPIB as talker only and
				 * listener only */
	{RD, isr1, 0x02},	/* check DO bit set */
	{RD, adsr, 0x46},	/* check AT-GPIB is both talker active
				 * and listener active */
	{WT, cdor, 0xAA},	/* write out data byte */
	{xDELAY, 0, 1},		/* One ISA I/O Cycle (500-ns) */
	{RD, isr1, 0x03},	/* check DO and DI bits set */
	{RD, dir, 0xAA},	/* verify data received */
	{WT, cdor, 0x55},	/* write out data byte */
	{xDELAY, 0, 1},		/* One ISA I/O Cycle (500-ns) */
	{RD, dir, 0x55},	/* verify data received */
	{END, 0, 0}
};

static struct tst tst_spmr_spsr[] = {
	{WT, spsr, 0x00},	/* Write pattern to SPSR register */
	{RD, spmr, 0x00},	/* Read back previously written pattern */
	{WT, spsr, 0xBF},	/* Write pattern to SPSR register */
	{RD, spmr, 0xBF},	/* Read back previously written pattern */
	{END, 0, 0}
};

static struct tst tst_count0_1[] = {
	{WT, cnt0, 0x55}, 	/* Verify every other bit can be set */
	{WT, cnt1, 0xAA},
	{RD, cnt0, 0x55}, 	/* Read back previously written pattern */
	{RD, cnt1, 0xAA},
	{WT, cnt0, 0xAA}, 	/* Verify every other bit can be set */
	{WT, cnt1, 0x55},
	{RD, cnt0, 0xAA}, 	/* Read back previously written pattern */
	{RD, cnt1, 0x55},
	{END, 0, 0}
};

static int
tst_exec(struct tnt_softc *sc, struct tst *tp, const char *name)
{
	uint8_t u;
	int step;

	for (step = 0; tp->action != END; tp++, step++) {
		switch (tp->action) {
		case WT:
			bus_write_1(sc->res[1], tp->reg, tp->val);
			break;
		case RD:
			u = bus_read_1(sc->res[1], tp->reg);
			if (u != tp->val) {
				printf(
				    "Test %s, step %d: reg(%02x) = %02x",
				    name, step, tp->reg, u);
				printf( "should have been %02x\n", tp->val);
				return (1);
			}
			break;
		case xDELAY:
			DELAY(tp->val);
			break;
		default:
			printf("Unknown action in test %s, step %d: %d\n",
			name, step, tp->action);
			return (1);
		}
	}
	if (bootverbose)
		printf("Test %s passed\n", name);
	return (0);
}

static int
tnt_probe(device_t dev)
{

	if (pci_get_vendor(dev) == 0x1093 && pci_get_device(dev) == 0xc801) {
		device_set_desc(dev, "NI PCI-GPIB");
		return (BUS_PROBE_DEFAULT);
	}
	return (ENXIO);
}

static int
tnt_attach(device_t dev)
{
	struct tnt_softc *sc;
	int error, i;

	sc = device_get_softc(dev);

	error = bus_alloc_resources(dev, tnt_res_spec, sc->res);
	if (error)
		return (error);

	error = bus_setup_intr(dev, sc->res[2], INTR_TYPE_MISC | INTR_MPSAFE,
	    NULL, upd7210intr, &sc->upd7210, &sc->intr_handler);

	/* Necessary magic for MITE */
	bus_write_4(sc->res[0], 0xc0, rman_get_start(sc->res[1]) | 0x80);

	tst_exec(sc, tst_reset, "Reset");
	tst_exec(sc, tst_read_reg, "Read registers");
	tst_exec(sc, tst_bsr_dcr, "BSR & DCR");
	tst_exec(sc, tst_adr0_1, "ADR0,1");
	tst_exec(sc, tst_cdor_dir, "CDOR/DIR");
	tst_exec(sc, tst_spmr_spsr, "CPMR/SPSR");
	tst_exec(sc, tst_count0_1, "COUNT0:1");
	tst_exec(sc, tst_reset, "Reset");

	/* pass 7210 interrupts through */
	bus_write_1(sc->res[1], imr3, 0x02);

	for (i = 0; i < 8; i++) {
		sc->upd7210.reg_res[i] = sc->res[1];
		sc->upd7210.reg_offset[i] = i * 2;
	}

	/* No DMA help */
	sc->upd7210.dmachan = -1;

	upd7210attach(&sc->upd7210);

	return (0);
}

static int
tnt_detach(device_t dev)
{
	struct tnt_softc *sc;

	sc = device_get_softc(dev);
	bus_teardown_intr(dev, sc->res[2], sc->intr_handler);
	upd7210detach(&sc->upd7210);

	bus_release_resources(dev, tnt_res_spec, sc->res);

	return (0);
}

static device_method_t	tnt4882_methods[] = {
	DEVMETHOD(device_probe,		tnt_probe),
	DEVMETHOD(device_attach,	tnt_attach),
	DEVMETHOD(device_detach,	tnt_detach),
	{ 0, 0 }
};

static driver_t pci_gpib_driver = {
	"tnt4882",
	tnt4882_methods,
	sizeof(struct tnt_softc)
};

static devclass_t pci_gpib_devclass;

DRIVER_MODULE(pci_gpib, pci, pci_gpib_driver, pci_gpib_devclass, 0, 0);
