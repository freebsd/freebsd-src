/*-
 * Copyright (c) 2004 Scott Long
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
 */

/*	$NetBSD: esp_sbus.c,v 1.27 2002/12/10 13:44:47 pk Exp $	*/

/*-
 * Copyright (c) 1997, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum; Jason R. Thorpe of the Numerical Aerospace
 * Simulation Facility, NASA Ames Research Center; Paul Kranenburg.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/resource.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/openfirm.h>
#include <machine/bus.h>
#include <machine/ofw_machdep.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/scsi/scsi_all.h>

#include <sparc64/sbus/lsi64854reg.h>
#include <sparc64/sbus/lsi64854var.h>
#include <sparc64/sbus/sbusvar.h>

#include <dev/esp/ncr53c9xreg.h>
#include <dev/esp/ncr53c9xvar.h>

/* #define ESP_SBUS_DEBUG */

struct esp_softc {
	struct ncr53c9x_softc	sc_ncr53c9x;	/* glue to MI code */
	struct device		*sc_dev;

	int			sc_rid;
	struct resource		*sc_res;
	bus_space_handle_t	sc_regh;
	bus_space_tag_t		sc_regt;

	int			sc_irqrid;
	struct resource		*sc_irqres;
	void			*sc_irq;

	struct lsi64854_softc	*sc_dma;	/* pointer to my DMA */

	int	sc_pri;				/* SBUS priority */
};

static int	esp_sbus_probe(device_t);
static int	esp_sbus_attach(device_t);
static int	esp_sbus_detach(device_t);
static int	esp_sbus_suspend(device_t);
static int	esp_sbus_resume(device_t);

static device_method_t esp_sbus_methods[] = {
	DEVMETHOD(device_probe,		esp_sbus_probe),
	DEVMETHOD(device_attach,	esp_sbus_attach),
	DEVMETHOD(device_detach,	esp_sbus_detach),
	DEVMETHOD(device_suspend,	esp_sbus_suspend),
	DEVMETHOD(device_resume,	esp_sbus_resume),
	{0, 0}
};

static driver_t esp_sbus_driver = {
	"esp",
	esp_sbus_methods,
	sizeof(struct esp_softc)
};

static devclass_t	esp_devclass;
DRIVER_MODULE(esp, sbus, esp_sbus_driver, esp_devclass, 0, 0);

/*
 * Functions and the switch for the MI code.
 */
static u_char	esp_read_reg(struct ncr53c9x_softc *, int);
static void	esp_write_reg(struct ncr53c9x_softc *, int, u_char);
static int	esp_dma_isintr(struct ncr53c9x_softc *);
static void	esp_dma_reset(struct ncr53c9x_softc *);
static int	esp_dma_intr(struct ncr53c9x_softc *);
static int	esp_dma_setup(struct ncr53c9x_softc *, caddr_t *, size_t *,
			      int, size_t *);
static void	esp_dma_go(struct ncr53c9x_softc *);
static void	esp_dma_stop(struct ncr53c9x_softc *);
static int	esp_dma_isactive(struct ncr53c9x_softc *);
static void	espattach(struct esp_softc *, struct ncr53c9x_glue *);

static struct ncr53c9x_glue esp_sbus_glue = {
	esp_read_reg,
	esp_write_reg,
	esp_dma_isintr,
	esp_dma_reset,
	esp_dma_intr,
	esp_dma_setup,
	esp_dma_go,
	esp_dma_stop,
	esp_dma_isactive,
	NULL,			/* gl_clear_latched_intr */
};

static int
esp_sbus_probe(device_t dev)
{
	const char *name;

	name = ofw_bus_get_name(dev);
	if (strcmp("SUNW,fas", name) == 0) {
		device_set_desc(dev, "Sun FAS366 Fast-Wide SCSI");
	        return (-10);
	}

	return (ENXIO);
}

static int
esp_sbus_attach(device_t dev)
{
	struct esp_softc *esc = device_get_softc(dev);
	struct ncr53c9x_softc *sc = &esc->sc_ncr53c9x;
	struct lsi64854_softc *lsc;
	phandle_t node;
	int burst;

	esc->sc_dev = dev;
	node = ofw_bus_get_node(dev);
	if (OF_getprop(node, "initiator-id", &sc->sc_id,
		       sizeof(sc->sc_id)) == -1)
		sc->sc_id = 7;
	if (OF_getprop(node, "clock-frequency", &sc->sc_freq,
	    sizeof(sc->sc_freq)) == -1) {
		printf("failed to query OFW for clock-frequency\n");
		sc->sc_freq = sbus_get_clockfreq(dev);
	}

#ifdef ESP_SBUS_DEBUG
	device_printf(dev, "espattach_sbus: sc_id %d, freq %d\n",
	    sc->sc_id, sc->sc_freq);
#endif

	/*
	 * allocate space for dma, in SUNW,fas there are no separate
	 * dma devices
	 */
	lsc = malloc(sizeof (struct lsi64854_softc), M_DEVBUF, M_NOWAIT);

	if (lsc == NULL) {
		device_printf(dev, "out of memory (lsi64854_softc)\n");
		return (ENOMEM);
	}
	esc->sc_dma = lsc;

	/*
	 * fas has 2 register spaces: dma(lsi64854) and SCSI core (ncr53c9x)
	 */

	/* Map dma registers */
	lsc->sc_rid = 0;
	if ((lsc->sc_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &lsc->sc_rid, RF_ACTIVE)) == NULL) {
		device_printf(dev, "cannot map dma registers\n");
		free(lsc, M_DEVBUF);
		return (ENXIO);
	}
	lsc->sc_regt = rman_get_bustag(lsc->sc_res);
	lsc->sc_regh = rman_get_bushandle(lsc->sc_res);

	/* Create a parent DMA tag based on this bus */
	if (bus_dma_tag_create(NULL,			/* parent */
				PAGE_SIZE, 0,		/* algnmnt, boundary */
				BUS_SPACE_MAXADDR,	/* lowaddr */
				BUS_SPACE_MAXADDR,	/* highaddr */
				NULL, NULL,		/* filter, filterarg */
				BUS_SPACE_MAXSIZE_32BIT,/* maxsize */
				0,			/* nsegments */
				BUS_SPACE_MAXSIZE_32BIT,/* maxsegsize */
				0,			/* flags */
				NULL, NULL,		/* No locking */
				&lsc->sc_parent_dmat)) {
		device_printf(dev, "cannot allocate parent DMA tag\n");
		free(lsc, M_DEVBUF);
		return (ENOMEM);
	}
	burst = sbus_get_burstsz(dev);

#ifdef ESP_SBUS_DEBUG
	printf("espattach_sbus: burst 0x%x\n", burst);
#endif

	lsc->sc_burst = (burst & SBUS_BURST_32) ? 32 :
	    (burst & SBUS_BURST_16) ? 16 : 0;

	lsc->sc_channel = L64854_CHANNEL_SCSI;
	lsc->sc_client = sc;
	lsc->sc_dev = dev;

	lsi64854_attach(lsc);

	/*
	 * map SCSI core registers
	 */
	esc->sc_rid = 1;
	if ((esc->sc_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &esc->sc_rid, RF_ACTIVE)) == NULL) {
		device_printf(dev, "cannot map scsi core registers\n");
		free(lsc, M_DEVBUF);
		return (ENXIO);
	}
	esc->sc_regt = rman_get_bustag(esc->sc_res);
	esc->sc_regh = rman_get_bushandle(esc->sc_res);

#if 0
	esc->sc_pri = sa->sa_pri;

	/* add me to the sbus structures */
	esc->sc_sd.sd_reset = (void *) ncr53c9x_reset;
	sbus_establish(&esc->sc_sd, &sc->sc_dev);
#endif

	espattach(esc, &esp_sbus_glue);

	return (0);
}

static int
esp_sbus_detach(device_t dev)
{
	struct ncr53c9x_softc *sc;
	struct esp_softc *esc;

	esc = device_get_softc(dev);
	sc = &esc->sc_ncr53c9x;
	return (ncr53c9x_detach(sc, 0));
}

static int
esp_sbus_suspend(device_t dev)
{
	return (ENXIO);
}

static int
esp_sbus_resume(device_t dev)
{
	return (ENXIO);
}

/*
 * Attach this instance, and then all the sub-devices
 */
void
espattach(struct esp_softc *esc, struct ncr53c9x_glue *gluep)
{
	struct ncr53c9x_softc *sc = &esc->sc_ncr53c9x;
	unsigned int uid = 0;

	/*
	 * Set up glue for MI code early; we use some of it here.
	 */
	sc->sc_glue = gluep;

	/* gimme MHz */
	sc->sc_freq /= 1000000;

	/*
	 * XXX More of this should be in ncr53c9x_attach(), but
	 * XXX should we really poke around the chip that much in
	 * XXX the MI code?  Think about this more...
	 */

	/*
	 * It is necessary to try to load the 2nd config register here,
	 * to find out what rev the esp chip is, else the ncr53c9x_reset
	 * will not set up the defaults correctly.
	 */
	sc->sc_cfg1 = sc->sc_id | NCRCFG1_PARENB;
	sc->sc_cfg2 = NCRCFG2_SCSI2 | NCRCFG2_RPE;
	sc->sc_cfg3 = NCRCFG3_CDB;
	NCR_WRITE_REG(sc, NCR_CFG2, sc->sc_cfg2);

	if ((NCR_READ_REG(sc, NCR_CFG2) & ~NCRCFG2_RSVD) !=
	    (NCRCFG2_SCSI2 | NCRCFG2_RPE)) {
		sc->sc_rev = NCR_VARIANT_ESP100;
	} else {
		sc->sc_cfg2 = NCRCFG2_SCSI2;
		NCR_WRITE_REG(sc, NCR_CFG2, sc->sc_cfg2);
		sc->sc_cfg3 = 0;
		NCR_WRITE_REG(sc, NCR_CFG3, sc->sc_cfg3);
		sc->sc_cfg3 = (NCRCFG3_CDB | NCRCFG3_FCLK);
		NCR_WRITE_REG(sc, NCR_CFG3, sc->sc_cfg3);
		if (NCR_READ_REG(sc, NCR_CFG3) !=
		    (NCRCFG3_CDB | NCRCFG3_FCLK)) {
			sc->sc_rev = NCR_VARIANT_ESP100A;
		} else {
			/* NCRCFG2_FE enables > 64K transfers */
			sc->sc_cfg2 |= NCRCFG2_FE;
			sc->sc_cfg3 = 0;
			NCR_WRITE_REG(sc, NCR_CFG3, sc->sc_cfg3);
			sc->sc_rev = NCR_VARIANT_ESP200;

			/* XXX spec says it's valid after power up or chip reset */
			uid = NCR_READ_REG(sc, NCR_UID);
			if (((uid & 0xf8) >> 3) == 0x0a) /* XXX */
				sc->sc_rev = NCR_VARIANT_FAS366;
		}
	}

#ifdef ESP_SBUS_DEBUG
	printf("espattach: revision %d, uid 0x%x\n", sc->sc_rev, uid);
#endif

	/*
	 * XXX minsync and maxxfer _should_ be set up in MI code,
	 * XXX but it appears to have some dependency on what sort
	 * XXX of DMA we're hooked up to, etc.
	 */

	/*
	 * This is the value used to start sync negotiations
	 * Note that the NCR register "SYNCTP" is programmed
	 * in "clocks per byte", and has a minimum value of 4.
	 * The SCSI period used in negotiation is one-fourth
	 * of the time (in nanoseconds) needed to transfer one byte.
	 * Since the chip's clock is given in MHz, we have the following
	 * formula: 4 * period = (1000 / freq) * 4
	 */
	sc->sc_minsync = 1000 / sc->sc_freq;

	/* limit minsync due to unsolved performance issues */
	sc->sc_maxsync = sc->sc_minsync;
	sc->sc_maxoffset = 15;

	sc->sc_extended_geom = 1;

	/*
	 * Alas, we must now modify the value a bit, because it's
	 * only valid when can switch on FASTCLK and FASTSCSI bits  
	 * in config register 3... 
	 */
	switch (sc->sc_rev) {
	case NCR_VARIANT_ESP100:
		sc->sc_maxwidth = 0;
		sc->sc_maxxfer = 64 * 1024;
		sc->sc_minsync = 0;	/* No synch on old chip? */
		break;

	case NCR_VARIANT_ESP100A:
		sc->sc_maxwidth = 1;
		sc->sc_maxxfer = 64 * 1024;
		/* Min clocks/byte is 5 */
		sc->sc_minsync = ncr53c9x_cpb2stp(sc, 5);
		break;

	case NCR_VARIANT_ESP200:
	case NCR_VARIANT_FAS366:
		sc->sc_maxwidth = 1;
		sc->sc_maxxfer = 16 * 1024 * 1024;
		/* XXX - do actually set FAST* bits */
		break;
	}

	/* Establish interrupt channel */
	esc->sc_irqrid = 0;
	if ((esc->sc_irqres = bus_alloc_resource_any(esc->sc_dev, SYS_RES_IRQ,
	    &esc->sc_irqrid, RF_SHAREABLE|RF_ACTIVE)) == NULL) {
		device_printf(esc->sc_dev, "Cannot allocate interrupt\n");
		return;
	}
	if (bus_setup_intr(esc->sc_dev, esc->sc_irqres,
	    INTR_TYPE_BIO|INTR_MPSAFE, ncr53c9x_intr, sc, &esc->sc_irq)) {
		device_printf(esc->sc_dev, "Cannot set up interrupt\n");
		return;
	}

	/* Turn on target selection using the `dma' method */
	if (sc->sc_rev != NCR_VARIANT_FAS366)
		sc->sc_features |= NCR_F_DMASELECT;

	/* Do the common parts of attachment. */
	sc->sc_dev = esc->sc_dev;
	ncr53c9x_attach(sc);
}

/*
 * Glue functions.
 */

#ifdef ESP_SBUS_DEBUG
int esp_sbus_debug = 0;

static struct {
	char *r_name;
	int   r_flag; 
} esp__read_regnames [] = {
	{ "TCL", 0},			/* 0/00 */
	{ "TCM", 0},			/* 1/04 */
	{ "FIFO", 0},			/* 2/08 */
	{ "CMD", 0},			/* 3/0c */
	{ "STAT", 0},			/* 4/10 */
	{ "INTR", 0},			/* 5/14 */
	{ "STEP", 0},			/* 6/18 */
	{ "FFLAGS", 1},			/* 7/1c */
	{ "CFG1", 1},			/* 8/20 */
	{ "STAT2", 0},			/* 9/24 */
	{ "CFG4", 1},			/* a/28 */
	{ "CFG2", 1},			/* b/2c */
	{ "CFG3", 1},			/* c/30 */
	{ "-none", 1},			/* d/34 */
	{ "TCH", 1},			/* e/38 */
	{ "TCX", 1},			/* f/3c */
};

static struct {
	char *r_name;
	int   r_flag;
} esp__write_regnames[] = {
	{ "TCL", 1},			/* 0/00 */
	{ "TCM", 1},			/* 1/04 */
	{ "FIFO", 0},			/* 2/08 */
	{ "CMD", 0},			/* 3/0c */
	{ "SELID", 1},			/* 4/10 */
	{ "TIMEOUT", 1},		/* 5/14 */
	{ "SYNCTP", 1},			/* 6/18 */
	{ "SYNCOFF", 1},		/* 7/1c */
	{ "CFG1", 1},			/* 8/20 */
	{ "CCF", 1},			/* 9/24 */
	{ "TEST", 1},			/* a/28 */
	{ "CFG2", 1},			/* b/2c */
	{ "CFG3", 1},			/* c/30 */
	{ "-none", 1},			/* d/34 */
	{ "TCH", 1},			/* e/38 */
	{ "TCX", 1},			/* f/3c */
};
#endif

u_char
esp_read_reg(struct ncr53c9x_softc *sc, int reg)
{
	struct esp_softc *esc = (struct esp_softc *)sc;
	u_char v;

	v = bus_space_read_1(esc->sc_regt, esc->sc_regh, reg * 4);
#ifdef ESP_SBUS_DEBUG
	if (esp_sbus_debug && (reg < 0x10) && esp__read_regnames[reg].r_flag)
		printf("RD:%x <%s> %x\n", reg * 4,
		    ((unsigned)reg < 0x10) ? esp__read_regnames[reg].r_name : "<***>", v);
#endif
	return v;
}

void
esp_write_reg(struct ncr53c9x_softc *sc, int reg, u_char v)
{
	struct esp_softc *esc = (struct esp_softc *)sc;

#ifdef ESP_SBUS_DEBUG
	if (esp_sbus_debug && (reg < 0x10) && esp__write_regnames[reg].r_flag)
		printf("WR:%x <%s> %x\n", reg * 4,
		    ((unsigned)reg < 0x10) ? esp__write_regnames[reg].r_name : "<***>", v);
#endif
	bus_space_write_1(esc->sc_regt, esc->sc_regh, reg * 4, v);
}

int
esp_dma_isintr(struct ncr53c9x_softc *sc)
{
	struct esp_softc *esc = (struct esp_softc *)sc;

	return (DMA_ISINTR(esc->sc_dma));
}

void
esp_dma_reset(struct ncr53c9x_softc *sc)
{
	struct esp_softc *esc = (struct esp_softc *)sc;

	DMA_RESET(esc->sc_dma);
}

int
esp_dma_intr(struct ncr53c9x_softc *sc)
{
	struct esp_softc *esc = (struct esp_softc *)sc;

	return (DMA_INTR(esc->sc_dma));
}

int
esp_dma_setup(struct ncr53c9x_softc *sc, caddr_t *addr, size_t *len,
	      int datain, size_t *dmasize)
{
	struct esp_softc *esc = (struct esp_softc *)sc;

	return (DMA_SETUP(esc->sc_dma, addr, len, datain, dmasize));
}

void
esp_dma_go(struct ncr53c9x_softc *sc)
{
	struct esp_softc *esc = (struct esp_softc *)sc;

	DMA_GO(esc->sc_dma);
}

void
esp_dma_stop(struct ncr53c9x_softc *sc)
{
	struct esp_softc *esc = (struct esp_softc *)sc;
	uint32_t csr;

	csr = L64854_GCSR(esc->sc_dma);
	csr &= ~D_EN_DMA;
	L64854_SCSR(esc->sc_dma, csr);
}

int
esp_dma_isactive(struct ncr53c9x_softc *sc)
{
	struct esp_softc *esc = (struct esp_softc *)sc;

	return (DMA_ISACTIVE(esc->sc_dma));
}
