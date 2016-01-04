/*-
 * Copyright (c) 2010 Aleksandr Rybalko.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/interrupt.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/malloc.h>

#include <machine/bus.h>

#include <mips/rt305x/rt305xreg.h>
#include <mips/rt305x/rt305x_sysctlvar.h>


static int	rt305x_sysctl_probe(device_t);
static int	rt305x_sysctl_attach(device_t);
static int	rt305x_sysctl_detach(device_t);


static struct rt305x_sysctl_softc *rt305x_sysctl_softc = NULL;

static void
rt305x_sysctl_dump_config(device_t dev)
{
	uint32_t val;
#define DUMPREG(r) 							\
	val = rt305x_sysctl_get(r); printf("    " #r "=%#08x\n", val)

	val = rt305x_sysctl_get(SYSCTL_CHIPID0_3);
	printf("\tChip ID: \"%c%c%c%c", 
	    (val >> 0 ) & 0xff, 
	    (val >> 8 ) & 0xff, 
	    (val >> 16) & 0xff, 
	    (val >> 24) & 0xff);
	val = rt305x_sysctl_get(SYSCTL_CHIPID4_7);
	printf("%c%c%c%c\"\n", 
	    (val >> 0 ) & 0xff, 
	    (val >> 8 ) & 0xff, 
	    (val >> 16) & 0xff, 
	    (val >> 24) & 0xff);

	DUMPREG(SYSCTL_SYSCFG);
#if !defined(RT5350) && !defined(MT7620)
	if ( val & SYSCTL_SYSCFG_INIC_EE_SDRAM)
		printf("\tGet SDRAM config from EEPROM\n");
	if ( val & SYSCTL_SYSCFG_INIC_8MB_SDRAM)
		printf("\tBootstrap flag is set\n");
	printf("\tGE0 mode %u\n",
	    ((val & SYSCTL_SYSCFG_GE0_MODE_MASK) >> 
		SYSCTL_SYSCFG_GE0_MODE_SHIFT));
	if ( val & SYSCTL_SYSCFG_BOOT_ADDR_1F00)
		printf("\tBoot from 0x1f000000\n");
	if ( val & SYSCTL_SYSCFG_BYPASS_PLL)
		printf("\tBypass PLL\n");
	if ( val & SYSCTL_SYSCFG_BIG_ENDIAN)
		printf("\tBig Endian\n");
	if ( val & SYSCTL_SYSCFG_CPU_CLK_SEL_384MHZ)
		printf("\tClock is 384MHz\n");
	printf("\tBoot from %u\n",
	    ((val & SYSCTL_SYSCFG_BOOT_FROM_MASK) >> 
		SYSCTL_SYSCFG_BOOT_FROM_SHIFT));
	printf("\tBootstrap test code %u\n",
	    ((val & SYSCTL_SYSCFG_TEST_CODE_MASK) >> 
		SYSCTL_SYSCFG_TEST_CODE_SHIFT));
	printf("\tSRAM_CS mode %u\n",
	    ((val & SYSCTL_SYSCFG_SRAM_CS_MODE_MASK) >> 
		SYSCTL_SYSCFG_SRAM_CS_MODE_SHIFT));
	printf("\t%umA SDRAM_CLK driving\n",
	    (val & SYSCTL_SYSCFG_SDRAM_CLK_DRV)?12:8);

	DUMPREG(SYSCTL_CLKCFG0);
	printf("\tSDRAM_CLK_SKEW %uns\n", (val >> 30) & 0x03);

	DUMPREG(SYSCTL_CLKCFG1);
	if ( val & SYSCTL_CLKCFG1_PBUS_DIV_CLK_BY2)
		printf("\tPbus clock is 1/2 of System clock\n");
	if ( val & SYSCTL_CLKCFG1_OTG_CLK_EN)
		printf("\tUSB OTG clock is enabled\n");
	if ( val & SYSCTL_CLKCFG1_I2S_CLK_EN)
		printf("\tI2S clock is enabled\n");
	printf("\tI2S clock is %s\n", 
	    (val & SYSCTL_CLKCFG1_I2S_CLK_SEL_EXT)?
		"external":"internal 15.625MHz");
	printf("\tI2S clock divider %u\n",
	    ((val & SYSCTL_CLKCFG1_I2S_CLK_DIV_MASK) >> 
		SYSCTL_CLKCFG1_I2S_CLK_DIV_SHIFT));
	if ( val & SYSCTL_CLKCFG1_PCM_CLK_EN)
		printf("\tPCM clock is enabled\n");

	printf("\tPCM clock is %s\n", 
	    (val & SYSCTL_CLKCFG1_PCM_CLK_SEL_EXT)?
		"external":"internal 15.625MHz");
	printf("\tPCM clock divider %u\n",
	    ((val & SYSCTL_CLKCFG1_PCM_CLK_DIV_MASK) >> 
		SYSCTL_CLKCFG1_PCM_CLK_DIV_SHIFT));
	DUMPREG(SYSCTL_GPIOMODE);
#endif
#undef DUMPREG

	return;
}

static int
rt305x_sysctl_probe(device_t dev)
{
	device_set_desc(dev, "RT305X System Control driver");
	return (0);
}

static int
rt305x_sysctl_attach(device_t dev)
{
	struct rt305x_sysctl_softc *sc = device_get_softc(dev);
	int error = 0;

	KASSERT((device_get_unit(dev) == 0),
	    ("rt305x_sysctl: Only one sysctl module supported"));

	if (rt305x_sysctl_softc != NULL)
		return (ENXIO);
	rt305x_sysctl_softc = sc;


	/* Map control/status registers. */
	sc->mem_rid = 0;
	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &sc->mem_rid, RF_ACTIVE);

	if (sc->mem_res == NULL) {
		device_printf(dev, "couldn't map memory\n");
		error = ENXIO;
		rt305x_sysctl_detach(dev);
		return(error);
	}
#ifdef notyet
	sc->irq_rid = 0;
	if ((sc->irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, 
	    &sc->irq_rid, RF_SHAREABLE | RF_ACTIVE)) == NULL) {
		device_printf(dev, "unable to allocate IRQ resource\n");
		return (ENXIO);
	}

	if ((bus_setup_intr(dev, sc->irq_res, INTR_TYPE_MISC, 
	    rt305x_sysctl_intr, NULL, sc, &sc->sysctl_ih))) {
		device_printf(dev,
		    "WARNING: unable to register interrupt handler\n");
		return (ENXIO);
	}
#endif
	rt305x_sysctl_dump_config(dev);

	return (bus_generic_attach(dev));
}

static int
rt305x_sysctl_detach(device_t dev)
{
	struct rt305x_sysctl_softc *sc = device_get_softc(dev);

	bus_generic_detach(dev);

	if (sc->mem_res)
		bus_release_resource(dev, SYS_RES_MEMORY, sc->mem_rid,
		    sc->mem_res);
#ifdef notyet
	if (sc->irq_res)
		bus_release_resource(dev, SYS_RES_IRQ, sc->irq_rid,
		    sc->irq_res);
#endif
	return(0);
}

#ifdef notyet
static int
rt305x_sysctl_intr(void *arg)
{
	return (FILTER_HANDLED);
}
#endif

uint32_t
rt305x_sysctl_get(uint32_t reg)
{
	struct rt305x_sysctl_softc *sc = rt305x_sysctl_softc;
	return (bus_read_4(sc->mem_res, reg));
}

void
rt305x_sysctl_set(uint32_t reg, uint32_t val)
{
	struct rt305x_sysctl_softc *sc = rt305x_sysctl_softc;
	bus_write_4(sc->mem_res, reg, val);
	return;
}


static device_method_t rt305x_sysctl_methods[] = {
	DEVMETHOD(device_probe,			rt305x_sysctl_probe),
	DEVMETHOD(device_attach,		rt305x_sysctl_attach),
	DEVMETHOD(device_detach,		rt305x_sysctl_detach),

	{0, 0},
};

static driver_t rt305x_sysctl_driver = {
	"rt305x_sysctl",
	rt305x_sysctl_methods,
	sizeof(struct rt305x_sysctl_softc),
};
static devclass_t rt305x_sysctl_devclass;

DRIVER_MODULE(rt305x_sysctl, obio, rt305x_sysctl_driver, rt305x_sysctl_devclass, 0, 0);
