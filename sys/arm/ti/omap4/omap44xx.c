/*-
 * Copyright (c) 2011
 *	Ben Gray <ben.r.gray@gmail.com>.
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
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/resource.h>
#include <machine/resource.h>

#define	_ARM32_BUS_DMA_PRIVATE
#include <machine/bus.h>

#include <arm/ti/omapvar.h>
#include <arm/ti/omap_scm.h>
#include <arm/ti/omap4/omap4var.h>
#include <arm/ti/omap4/omap44xx_reg.h>

#include "omap_if.h"
#include "omap4_if.h"

/*
 * Standard priority levels for the system -  0 has the highest priority and 63
 * is the lowest.  
 *
 * Currently these are all set to the same standard value.
 */
static const struct omap4_intr_conf omap44xx_irq_prio[] =
{
	{ OMAP44XX_IRQ_L2CACHE,         0,   0x1},      /* L2 cache controller interrupt */
	{ OMAP44XX_IRQ_CTI_0,           0,   0x1},      /* Cross-trigger module 0 (CTI0) interrupt */
	{ OMAP44XX_IRQ_CTI_1,           0,   0x1},      /* Cross-trigger module 1 (CTI1) interrupt */
	{ OMAP44XX_IRQ_ELM,             0,   0x1},      /* Error location process completion */
	{ OMAP44XX_IRQ_SYS_NIRQ,        0,   0x1},      /* External source (active low) */
	{ OMAP44XX_IRQ_L3_DBG,          0,   0x1},      /* L3 interconnect debug error */
	{ OMAP44XX_IRQ_L3_APP,          0,   0x1},      /* L3 interconnect application error */
	{ OMAP44XX_IRQ_PRCM_MPU,        0,   0x1},      /* PRCM module IRQ */
	{ OMAP44XX_IRQ_SDMA0,           0,   0x1},      /* System DMA request 0(3) */
	{ OMAP44XX_IRQ_SDMA1,           0,   0x1},      /* System DMA request 1(3) */
	{ OMAP44XX_IRQ_SDMA2,           0,   0x1},      /* System DMA request 2 */
	{ OMAP44XX_IRQ_SDMA3,           0,   0x1},      /* System DMA request 3 */
	{ OMAP44XX_IRQ_MCBSP4,          0,   0x1},      /* McBSP module 4 IRQ */
	{ OMAP44XX_IRQ_MCBSP1,          0,   0x1},      /* McBSP module 1 IRQ */
	{ OMAP44XX_IRQ_SR1,             0,   0x1},      /* SmartReflex™ 1 */
	{ OMAP44XX_IRQ_SR2,             0,   0x1},      /* SmartReflex™ 2 */
	{ OMAP44XX_IRQ_GPMC,            0,   0x1},      /* General-purpose memory controller module */
	{ OMAP44XX_IRQ_SGX,             0,   0x1},      /* 2D/3D graphics module */
	{ OMAP44XX_IRQ_MCBSP2,          0,   0x1},      /* McBSP module 2 */
	{ OMAP44XX_IRQ_MCBSP3,          0,   0x1},      /* McBSP module 3 */
	{ OMAP44XX_IRQ_ISS5,            0,   0x1},      /* Imaging subsystem interrupt 5 */
	{ OMAP44XX_IRQ_DSS,             0,   0x1},      /* Display subsystem module(3) */
	{ OMAP44XX_IRQ_MAIL_U0,         0,   0x1},      /* Mailbox user 0 request */
	{ OMAP44XX_IRQ_C2C_SSCM,        0,   0x1},      /* C2C status interrupt */
	{ OMAP44XX_IRQ_DSP_MMU,         0,   0x1},      /* DSP MMU */
	{ OMAP44XX_IRQ_GPIO1_MPU,       0,   0x1},      /* GPIO module 1(3) */
	{ OMAP44XX_IRQ_GPIO2_MPU,       0,   0x1},      /* GPIO module 2(3) */
	{ OMAP44XX_IRQ_GPIO3_MPU,       0,   0x1},      /* GPIO module 3(3) */
	{ OMAP44XX_IRQ_GPIO4_MPU,       0,   0x1},      /* GPIO module 4(3) */
	{ OMAP44XX_IRQ_GPIO5_MPU,       0,   0x1},      /* GPIO module 5(3) */
	{ OMAP44XX_IRQ_GPIO6_MPU,       0,   0x1},      /* GPIO module 6(3) */
	{ OMAP44XX_IRQ_WDT3,            0,   0x1},      /* Watchdog timer module 3 overflow */
	{ OMAP44XX_IRQ_GPT1,            0,   0x1},      /* General-purpose timer module 1 */
	{ OMAP44XX_IRQ_GPT2,            0,   0x1},      /* General-purpose timer module 2 */
	{ OMAP44XX_IRQ_GPT3,            0,   0x1},      /* General-purpose timer module 3 */
	{ OMAP44XX_IRQ_GPT4,            0,   0x1},      /* General-purpose timer module 4 */
	{ OMAP44XX_IRQ_GPT5,            0,   0x1},      /* General-purpose timer module 5(3) */
	{ OMAP44XX_IRQ_GPT6,            0,   0x1},      /* General-purpose timer module 6(3) */
	{ OMAP44XX_IRQ_GPT7,            0,   0x1},      /* General-purpose timer module 7(3) */
	{ OMAP44XX_IRQ_GPT8,            0,   0x1},      /* General-purpose timer module 8(3) */
	{ OMAP44XX_IRQ_GPT9,            0,   0x1},      /* General-purpose timer module 9 */
	{ OMAP44XX_IRQ_GPT10,           0,   0x1},      /* General-purpose timer module 10 */
	{ OMAP44XX_IRQ_GPT11,           0,   0x1},      /* General-purpose timer module 11 */
	{ OMAP44XX_IRQ_MCSPI4,          0,   0x1},      /* McSPI module 4 */
	{ OMAP44XX_IRQ_DSS_DSI1,        0,   0x1},      /* Display Subsystem DSI1 interrupt */
	{ OMAP44XX_IRQ_I2C1,            0,   0x1},      /* I2C module 1 */
	{ OMAP44XX_IRQ_I2C2,            0,   0x1},      /* I2C module 2 */
	{ OMAP44XX_IRQ_HDQ,             0,   0x1},      /* HDQ / One-wire */
	{ OMAP44XX_IRQ_MMC5,            0,   0x1},      /* MMC5 interrupt */
	{ OMAP44XX_IRQ_I2C3,            0,   0x1},      /* I2C module 3 */
	{ OMAP44XX_IRQ_I2C4,            0,   0x1},      /* I2C module 4 */
	{ OMAP44XX_IRQ_MCSPI1,          0,   0x1},      /* McSPI module 1 */
	{ OMAP44XX_IRQ_MCSPI2,          0,   0x1},      /* McSPI module 2 */
	{ OMAP44XX_IRQ_HSI_P1,          0,   0x1},      /* HSI Port 1 interrupt */
	{ OMAP44XX_IRQ_HSI_P2,          0,   0x1},      /* HSI Port 2 interrupt */
	{ OMAP44XX_IRQ_FDIF_3,          0,   0x1},      /* Face detect interrupt 3 */
	{ OMAP44XX_IRQ_UART4,           0,   0x1},      /* UART module 4 interrupt */
	{ OMAP44XX_IRQ_HSI_DMA,         0,   0x1},      /* HSI DMA engine MPU request */
	{ OMAP44XX_IRQ_UART1,           0,   0x1},      /* UART module 1 */
	{ OMAP44XX_IRQ_UART2,           0,   0x1},      /* UART module 2 */
	{ OMAP44XX_IRQ_UART3,           0,   0x1},      /* UART module 3 (also infrared)(3) */
	{ OMAP44XX_IRQ_PBIAS,           0,   0x1},      /* Merged interrupt for PBIASlite1 and 2 */
	{ OMAP44XX_IRQ_OHCI,            0,   0x1},      /* OHCI controller HSUSB MP Host Interrupt */
	{ OMAP44XX_IRQ_EHCI,            0,   0x1},      /* EHCI controller HSUSB MP Host Interrupt */
	{ OMAP44XX_IRQ_TLL,             0,   0x1},      /* HSUSB MP TLL Interrupt */
	{ OMAP44XX_IRQ_WDT2,            0,   0x1},      /* WDTIMER2 interrupt */
	{ OMAP44XX_IRQ_MMC1,            0,   0x1},      /* MMC/SD module 1 */
	{ OMAP44XX_IRQ_DSS_DSI2,        0,   0x1},      /* Display subsystem DSI2 interrupt */
	{ OMAP44XX_IRQ_MMC2,            0,   0x1},      /* MMC/SD module 2 */
	{ OMAP44XX_IRQ_MPU_ICR,         0,   0x1},      /* MPU ICR */
	{ OMAP44XX_IRQ_C2C_GPI,         0,   0x1},      /* C2C GPI interrupt */
	{ OMAP44XX_IRQ_FSUSB,           0,   0x1},      /* FS-USB - host controller Interrupt */
	{ OMAP44XX_IRQ_FSUSB_SMI,       0,   0x1},      /* FS-USB - host controller SMI Interrupt */
	{ OMAP44XX_IRQ_MCSPI3,          0,   0x1},      /* McSPI module 3 */
	{ OMAP44XX_IRQ_HSUSB_OTG,       0,   0x1},      /* High-Speed USB OTG controller */
	{ OMAP44XX_IRQ_HSUSB_OTG_DMA,   0,   0x1},      /* High-Speed USB OTG DMA controller */
	{ OMAP44XX_IRQ_MMC3,            0,   0x1},      /* MMC/SD module 3 */
	{ OMAP44XX_IRQ_MMC4,            0,   0x1},      /* MMC4 interrupt */
	{ OMAP44XX_IRQ_SLIMBUS1,        0,   0x1},      /* SLIMBUS1 interrupt */
	{ OMAP44XX_IRQ_SLIMBUS2,        0,   0x1},      /* SLIMBUS2 interrupt */
	{ OMAP44XX_IRQ_ABE,             0,   0x1},      /* Audio back-end interrupt */
	{ OMAP44XX_IRQ_CORTEXM3_MMU,    0,   0x1},      /* Cortex-M3 MMU interrupt */
	{ OMAP44XX_IRQ_DSS_HDMI,        0,   0x1},      /* Display subsystem HDMI interrupt */
	{ OMAP44XX_IRQ_SR_IVA,          0,   0x1},      /* SmartReflex IVA interrupt */
	{ OMAP44XX_IRQ_IVAHD1,          0,   0x1},      /* Sync interrupt from iCONT2 (vDMA) */
	{ OMAP44XX_IRQ_IVAHD2,          0,   0x1},      /* Sync interrupt from iCONT1 */
	{ OMAP44XX_IRQ_IVAHD_MAILBOX0,  0,   0x1},      /* IVAHD mailbox interrupt */
	{ OMAP44XX_IRQ_MCASP1,          0,   0x1},      /* McASP1 transmit interrupt */
	{ OMAP44XX_IRQ_EMIF1,           0,   0x1},      /* EMIF1 interrupt */
	{ OMAP44XX_IRQ_EMIF2,           0,   0x1},      /* EMIF2 interrupt */
	{ OMAP44XX_IRQ_MCPDM,           0,   0x1},      /* MCPDM interrupt */
	{ OMAP44XX_IRQ_DMM,             0,   0x1},      /* DMM interrupt */
	{ OMAP44XX_IRQ_DMIC,            0,   0x1},      /* DMIC interrupt */
	{ OMAP44XX_IRQ_SYS_NIRQ2,       0,   0x1},      /* External source 2 (active low) */
	{ OMAP44XX_IRQ_KBD,             0,   0x1},      /* Keyboard controller interrupt */
	{ -1, 0, 0 },
};

/**
 *	omap_sdram_size - called from machdep to get the total memory size
 * 
 *	Since this function is called very early in the boot, there is none of the
 *	bus handling code setup. However the boot device map will be setup, so
 *	we can directly access registers already mapped.
 *
 *	This is a bit ugly, but since we need this information early on and the
 *	only way to get it (appart from hardcoding it or via kernel args) is to read
 *	from the EMIF_SRAM registers.
 *
 *	RETURNS:
 *	The size of memory in bytes.
 */
unsigned int
omap_sdram_size(void)
{
	uint32_t sdram_cfg;
	uint32_t ibank, ebank, pagesize;

	/* Read the two SDRAM config register */
	sdram_cfg = *(volatile uint32_t*)(OMAP44XX_L3_EMIF1_VBASE + 0x0008);

	/* Read the REG_EBANK, REG_IBANK and REG_PAGESIZE - together these tell
	 * us the maximum size of memory we can access.
	 */
	ibank = (sdram_cfg >> 4) & 0x7;
	ebank = (sdram_cfg >> 3) & 0x1;
	pagesize = (sdram_cfg >> 0) & 0x7;
	
	if (bootverbose)
		printf("omap_sdram_size: ibank=%u, ebank=%u, pagesize=%u\n",
		    ibank, ebank, pagesize);

	/* Using the above read values we determine the memory size, this was
	 * gleened from tables 15-106 and 15-107 in the omap44xx tech ref manual.
	 */
	return (1UL << (25 + ibank + ebank + pagesize));
}

/*
 * Writes val to SCM register at offset off
 */
static void 
omap4xx_scm_writes(device_t dev, bus_size_t off, uint16_t val)
{
	struct omap4_softc *sc = device_get_softc(dev);

	bus_write_2(sc->sc_scm_mem, off, val);
}

/*
 * Reads SCM register at offset off
 */
static uint16_t 
omap4xx_scm_reads(device_t dev, bus_size_t off)
{
	struct omap4_softc *sc = device_get_softc(dev);

	return bus_read_2(sc->sc_scm_mem, off);
}

static void 
omap4xx_gic_dist_write(device_t dev, bus_size_t off, uint32_t val)
{
	struct omap4_softc *sc = device_get_softc(dev);

	bus_space_write_4(sc->sc_iotag, sc->sc_gic_dist_ioh,
	    off, val);
}

static uint32_t 
omap4xx_gic_dist_read(device_t dev, bus_size_t off)
{
	struct omap4_softc *sc = device_get_softc(dev);

	return bus_space_read_4(sc->sc_iotag, sc->sc_gic_dist_ioh, off);
}

static void 
omap4xx_gic_cpu_write(device_t dev, bus_size_t off, uint32_t val)
{
	struct omap4_softc *sc = device_get_softc(dev);

	bus_space_write_4(sc->sc_iotag, sc->sc_gic_cpu_ioh,
	    off, val);
}

static uint32_t 
omap4xx_gic_cpu_read(device_t dev, bus_size_t off)
{
	struct omap4_softc *sc = device_get_softc(dev);

	return bus_space_read_4(sc->sc_iotag, sc->sc_gic_cpu_ioh, off);
}

static inline uint32_t
omap4xx_gbl_timer_read(device_t dev, bus_size_t off)
{
	struct omap4_softc *sc = device_get_softc(dev);

	return (bus_space_read_4(sc->sc_iotag, sc->sc_gbl_timer_ioh, off));
}

static inline void
omap4xx_gbl_timer_write(device_t dev, bus_size_t off, uint32_t val)
{
	struct omap4_softc *sc = device_get_softc(dev);

	bus_space_write_4(sc->sc_iotag, sc->sc_gbl_timer_ioh, off, val);
}

static inline uint32_t
omap4xx_prv_timer_read(device_t dev, bus_size_t off)
{
	struct omap4_softc *sc = device_get_softc(dev);

	return (bus_space_read_4(sc->sc_iotag, sc->sc_prv_timer_ioh, off));
}

static inline void
omap4xx_prv_timer_write(device_t dev, bus_size_t off, uint32_t val)
{
	struct omap4_softc *sc = device_get_softc(dev);

	bus_space_write_4(sc->sc_iotag, sc->sc_prv_timer_ioh, off, val);
}

/**
 *	omap44xx_identify - adds the SoC child components
 *	@dev: this device, the one we are adding to
 * 
 *	Adds a child to the omap3 base device.
 *
 */
static void
omap44xx_identify(driver_t *drv, device_t parent)
{
	int omap44xx_irqs[] = { 27, 29, -1 };
	struct omap_mem_range omap44xx_mem[] = { { OMAP44XX_MPU_SUBSYS_HWBASE,
	                                           OMAP44XX_MPU_SUBSYS_SIZE    },
	                                         { 0, 0 } }; 
	int i;

	device_t self = BUS_ADD_CHILD(parent, 0, "omap44xx", 0);

	for (i = 0; omap44xx_irqs[i] != -1; i++)
		bus_set_resource(self, SYS_RES_IRQ, i, omap44xx_irqs[i], 1);
	
	/* Assign the memory region to the resource list */
	for (i = 0; omap44xx_mem[i].base != 0; i++) {
		bus_set_resource(self, SYS_RES_MEMORY, i,
		    omap44xx_mem[i].base, omap44xx_mem[i].size);
	}
}

/**
 *	omap44xx_probe - called when the device is probed
 *	@dev: the new device
 * 
 *	All we do in this routine is set the description of this device
 *
 */
static int
omap44xx_probe(device_t dev)
{
	device_set_desc(dev, "TI OMAP44XX");
	return (0);
}

/**
 *	omap44xx_attach - called when the device is attached
 *	@dev: the new device
 * 
 *	All we do in this routine is set the description of this device
 *
 */
static int
omap44xx_attach(device_t dev)
{
	struct omap_softc *omapsc = device_get_softc(device_get_parent(dev));
	struct omap4_softc *sc = device_get_softc(dev);

	sc->sc_iotag = omapsc->sc_iotag;
	sc->sc_dev = dev;


	/* Map in the Generic Interrupt Controller (GIC) register set */
	if (bus_space_map(sc->sc_iotag, OMAP44XX_GIC_CPU_HWBASE,
	                  OMAP44XX_GIC_CPU_SIZE, 0, &sc->sc_gic_cpu_ioh)) {
		panic("%s: Cannot map registers", device_get_name(dev));
	}

	/* Also map in the CPU GIC register set */
	if (bus_space_map(sc->sc_iotag, OMAP44XX_GIC_DIST_HWBASE,
	                  OMAP44XX_GIC_DIST_SIZE, 0, &sc->sc_gic_dist_ioh)) {
		panic("%s: Cannot map registers", device_get_name(dev));
	}

	/* And the private and global timer register set */
	if (bus_space_map(sc->sc_iotag, OMAP44XX_PRV_TIMER_HWBASE,
	                  OMAP44XX_PRV_TIMER_SIZE, 0, &sc->sc_prv_timer_ioh)) {
		panic("%s: Cannot map registers", device_get_name(dev));
	}
	if (bus_space_map(sc->sc_iotag, OMAP44XX_GBL_TIMER_HWBASE,
	                  OMAP44XX_GBL_TIMER_SIZE, 0, &sc->sc_gbl_timer_ioh)) {
		panic("%s: Cannot map registers", device_get_name(dev));
	}

	/* Map in the PL310 (L2 cache controller) as well */
	if (bus_space_map(sc->sc_iotag, OMAP44XX_PL310_HWBASE,
	                  OMAP44XX_PL310_SIZE, 0, &sc->sc_pl310_ioh)) {
		panic("%s: Cannot map registers", device_get_name(dev));
	}


	/* Init SCM access resource */
	sc->sc_scm_mem = bus_alloc_resource(dev, SYS_RES_MEMORY, 
	    &sc->sc_scm_rid,
            OMAP44XX_SCM_PADCONF_HWBASE, 
	    OMAP44XX_SCM_PADCONF_HWBASE + OMAP44XX_SCM_PADCONF_SIZE,
            OMAP44XX_SCM_PADCONF_SIZE, RF_ACTIVE);

	if (sc->sc_scm_mem == NULL)
		printf("failed to allocate SCM memory resource");

	/* TODO: Revisit - Install an interrupt post filter */
	arm_post_filter = omap4_post_filter_intr;
	
	/* Setup the ARM GIC interrupt controller */
	omap4_setup_intr_controller(dev, omap44xx_irq_prio);
	/* Should be called after omap4_setup_intr_controller */
	omap4_setup_gic_cpu(0xF0);

	/* Setup basic timer stuff before cpu_initclocks is called */
	omap4_init_timer(dev);

	omap_scm_padconf_init_from_hints(dev);

	bus_generic_probe(dev);
	bus_generic_attach(dev);

	return (0);
}



static device_method_t omap44xx_methods[] = {
	DEVMETHOD(device_probe, omap44xx_probe),
	DEVMETHOD(device_attach, omap44xx_attach),
	DEVMETHOD(device_identify, omap44xx_identify),

	/* SCM access methods */
	DEVMETHOD(omap_scm_reads, omap4xx_scm_reads),
	DEVMETHOD(omap_scm_writes, omap4xx_scm_writes),

	/* OMAP4-specific accessors methods */

	/* Interrupt controller */
	DEVMETHOD(omap4_gic_dist_read, omap4xx_gic_dist_read),
	DEVMETHOD(omap4_gic_dist_write, omap4xx_gic_dist_write),
	DEVMETHOD(omap4_gic_cpu_read, omap4xx_gic_cpu_read),
	DEVMETHOD(omap4_gic_cpu_write, omap4xx_gic_cpu_write),

	/* Timers */
	DEVMETHOD(omap4_gbl_timer_read, omap4xx_gbl_timer_read),
	DEVMETHOD(omap4_gbl_timer_write, omap4xx_gbl_timer_write),
	DEVMETHOD(omap4_prv_timer_read, omap4xx_prv_timer_read),
	DEVMETHOD(omap4_prv_timer_write, omap4xx_prv_timer_write),

	{0, 0},
};

static driver_t omap44xx_driver = {
	"omap44xx",
	omap44xx_methods,
	sizeof(struct omap4_softc),
};

static devclass_t omap44xx_devclass;

DRIVER_MODULE(omap44xx, omap, omap44xx_driver, omap44xx_devclass, 0, 0);
