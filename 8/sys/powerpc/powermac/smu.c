/*-
 * Copyright (c) 2009 Nathan Whitehorn
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/conf.h>
#include <sys/cpu.h>
#include <sys/kernel.h>
#include <sys/rman.h>
#include <sys/sysctl.h>

#include <machine/bus.h>
#include <machine/md_var.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <powerpc/powermac/macgpiovar.h>

struct smu_cmd {
	uint8_t		cmd;
	uint8_t		len;
	uint8_t		data[254];
};

struct smu_softc {
	device_t	sc_dev;
	struct mtx	sc_mtx;

	struct resource	*sc_memr;
	int		sc_memrid;

	bus_dma_tag_t	sc_dmatag;
	bus_space_tag_t	sc_bt;
	bus_space_handle_t sc_mailbox;

	struct smu_cmd	*sc_cmd;
	bus_addr_t	sc_cmd_phys;
	bus_dmamap_t	sc_cmd_dmamap;
};

/* regular bus attachment functions */

static int	smu_probe(device_t);
static int	smu_attach(device_t);

/* cpufreq notification hooks */

static void	smu_cpufreq_pre_change(device_t, const struct cf_level *level);
static void	smu_cpufreq_post_change(device_t, const struct cf_level *level);

/* where to find the doorbell GPIO */

static device_t	smu_doorbell = NULL;

static device_method_t  smu_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		smu_probe),
	DEVMETHOD(device_attach,	smu_attach),
	{ 0, 0 },
};

static driver_t smu_driver = {
	"smu",
	smu_methods,
	sizeof(struct smu_softc)
};

static devclass_t smu_devclass;

DRIVER_MODULE(smu, nexus, smu_driver, smu_devclass, 0, 0);

#define SMU_MAILBOX	0x860c

/* Command types */
#define SMU_POWER	0xaa

static int
smu_probe(device_t dev)
{
	const char *name = ofw_bus_get_name(dev);

	if (strcmp(name, "smu") != 0)
		return (ENXIO);

	device_set_desc(dev, "Apple System Management Unit");
	return (0);
}

static void
smu_phys_callback(void *xsc, bus_dma_segment_t *segs, int nsegs, int error)
{
	struct smu_softc *sc = xsc;

	sc->sc_cmd_phys = segs[0].ds_addr;
}

static int
smu_attach(device_t dev)
{
	struct smu_softc *sc;

	sc = device_get_softc(dev);

	mtx_init(&sc->sc_mtx, "smu", NULL, MTX_DEF);

	/*
	 * Map the mailbox area. This should be determined from firmware,
	 * but I have not found a simple way to do that.
	 */
	bus_dma_tag_create(NULL, 16, 0, BUS_SPACE_MAXADDR_32BIT,
	    BUS_SPACE_MAXADDR, NULL, NULL, PAGE_SIZE, 1, PAGE_SIZE, 0, NULL,
	    NULL, &(sc->sc_dmatag));
	sc->sc_bt = &bs_be_tag;
	bus_space_map(sc->sc_bt, SMU_MAILBOX, 4, 0, &sc->sc_mailbox);

	/*
	 * Allocate the command buffer. This can be anywhere in the low 4 GB
	 * of memory.
	 */
	bus_dmamem_alloc(sc->sc_dmatag, (void **)&sc->sc_cmd, BUS_DMA_WAITOK | 
	    BUS_DMA_ZERO, &sc->sc_cmd_dmamap);
	bus_dmamap_load(sc->sc_dmatag, sc->sc_cmd_dmamap,
	    sc->sc_cmd, PAGE_SIZE, smu_phys_callback, sc, 0);

	/*
	 * Set up handlers to change CPU voltage when CPU frequency is changed.
	 */
	EVENTHANDLER_REGISTER(cpufreq_pre_change, smu_cpufreq_pre_change, dev,
	    EVENTHANDLER_PRI_ANY);
	EVENTHANDLER_REGISTER(cpufreq_post_change, smu_cpufreq_post_change, dev,
	    EVENTHANDLER_PRI_ANY);

	return (0);
}

static int
smu_run_cmd(device_t dev, struct smu_cmd *cmd)
{
	struct smu_softc *sc;
	int doorbell_ack, result;

	sc = device_get_softc(dev);

	mtx_lock(&sc->sc_mtx);

	/* Copy the command to the mailbox */
	memcpy(sc->sc_cmd, cmd, sizeof(*cmd));
	bus_dmamap_sync(sc->sc_dmatag, sc->sc_cmd_dmamap, BUS_DMASYNC_PREWRITE);
	bus_space_write_4(sc->sc_bt, sc->sc_mailbox, 0, sc->sc_cmd_phys);

	/* Invalidate the cacheline it is in -- SMU bypasses the cache */
	__asm __volatile("dcbst 0,%0; sync" :: "r"(sc->sc_cmd): "memory");

	/* Ring SMU doorbell */
	macgpio_write(smu_doorbell, GPIO_DDR_OUTPUT);

	/* Wait for the doorbell GPIO to go high, signaling completion */
	do {
		/* XXX: timeout */
		DELAY(50);
		doorbell_ack = macgpio_read(smu_doorbell);
	} while (!doorbell_ack);

	/* Check result. First invalidate the cache again... */
	__asm __volatile("dcbf 0,%0; sync" :: "r"(sc->sc_cmd) : "memory");
	
	bus_dmamap_sync(sc->sc_dmatag, sc->sc_cmd_dmamap, BUS_DMASYNC_POSTREAD);

	/* SMU acks the command by inverting the command bits */
	if (sc->sc_cmd->cmd == ~cmd->cmd)
		result = 0;
	else
		result = EIO;

	mtx_unlock(&sc->sc_mtx);

	return (result);
}

static void
smu_slew_cpu_voltage(device_t dev, int to)
{
	struct smu_cmd cmd;

	cmd.cmd = SMU_POWER;
	cmd.len = 8;
	cmd.data[0] = 'V';
	cmd.data[1] = 'S'; 
	cmd.data[2] = 'L'; 
	cmd.data[3] = 'E'; 
	cmd.data[4] = 'W'; 
	cmd.data[5] = 0xff;
	cmd.data[6] = 1;
	cmd.data[7] = to;

	smu_run_cmd(dev, &cmd);
}

static void
smu_cpufreq_pre_change(device_t dev, const struct cf_level *level)
{
	/*
	 * Make sure the CPU voltage is raised before we raise
	 * the clock.
	 */
		
	if (level->rel_set[0].freq == 10000 /* max */)
		smu_slew_cpu_voltage(dev, 0);
}

static void
smu_cpufreq_post_change(device_t dev, const struct cf_level *level)
{
	/* We are safe to reduce CPU voltage after a downward transition */

	if (level->rel_set[0].freq < 10000 /* max */)
		smu_slew_cpu_voltage(dev, 1); /* XXX: 1/4 voltage for 970MP? */
}

/* Routines for probing the SMU doorbell GPIO */
static int doorbell_probe(device_t dev);
static int doorbell_attach(device_t dev);

static device_method_t  doorbell_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		doorbell_probe),
	DEVMETHOD(device_attach,	doorbell_attach),
	{ 0, 0 },
};

static driver_t doorbell_driver = {
	"smudoorbell",
	doorbell_methods,
	0
};

static devclass_t doorbell_devclass;

DRIVER_MODULE(smudoorbell, macgpio, doorbell_driver, doorbell_devclass, 0, 0);

static int
doorbell_probe(device_t dev)
{
	const char *name = ofw_bus_get_name(dev);

	if (strcmp(name, "smu-doorbell") != 0)
		return (ENXIO);

	device_set_desc(dev, "SMU Doorbell GPIO");
	device_quiet(dev);
	return (0);
}

static int
doorbell_attach(device_t dev)
{
	smu_doorbell = dev;
	return (0);
}
