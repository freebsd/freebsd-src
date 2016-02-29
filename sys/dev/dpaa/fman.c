/*-
 * Copyright (c) 2011-2012 Semihalf.
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
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/malloc.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "opt_platform.h"

#include <contrib/ncsw/inc/Peripherals/fm_ext.h>
#include <contrib/ncsw/inc/Peripherals/fm_muram_ext.h>
#include <contrib/ncsw/inc/ncsw_ext.h>
#include <contrib/ncsw/integrations/fman_ucode.h>

#include "fman.h"


/**
 * @group FMan private defines.
 * @{
 */
enum fman_irq_enum {
	FMAN_IRQ_NUM		= 0,
	FMAN_ERR_IRQ_NUM	= 1
};

enum fman_mu_ram_map {
	FMAN_MURAM_OFF		= 0x0,
	FMAN_MURAM_SIZE		= 0x28000
};

struct fman_config {
	device_t fman_device;
	uintptr_t mem_base_addr;
	int irq_num;
	int err_irq_num;
	uint8_t fm_id;
	t_FmExceptionsCallback *exception_callback;
	t_FmBusErrorCallback *bus_error_callback;
};

/**
 * @group FMan private methods/members.
 * @{
 */
/**
 * Frame Manager firmware.
 * We use the same firmware for both P3041 and P2041 devices.
 */
const uint32_t fman_firmware[] = FMAN_UC_IMG;
const uint32_t fman_firmware_size = sizeof(fman_firmware);
static struct fman_softc *fm_sc = NULL;

static t_Handle
fman_init(struct fman_softc *sc, struct fman_config *cfg)
{
	t_FmParams fm_params;
	t_Handle muram_handle, fm_handle;
	t_Error error;
	t_FmRevisionInfo revision_info;
	uint16_t clock;
	uint32_t tmp, mod;

	/* MURAM configuration */
	muram_handle = FM_MURAM_ConfigAndInit(cfg->mem_base_addr +
	    FMAN_MURAM_OFF, FMAN_MURAM_SIZE);
	if (muram_handle == NULL) {
		device_printf(cfg->fman_device, "couldn't init FM MURAM module"
		    "\n");
		return (NULL);
	}
	sc->muram_handle = muram_handle;

	/* Fill in FM configuration */
	fm_params.fmId = cfg->fm_id;
	/* XXX we support only one partition thus each fman has master id */
	fm_params.guestId = NCSW_MASTER_ID;

	fm_params.baseAddr = cfg->mem_base_addr;
	fm_params.h_FmMuram = muram_handle;

	/* Get FMan clock in Hz */
	if ((tmp = fman_get_clock(sc)) == 0)
		return (NULL);

	/* Convert FMan clock to MHz */
	clock = (uint16_t)(tmp / 1000000);
	mod = tmp % 1000000;

	if (mod >= 500000)
		++clock;

	fm_params.fmClkFreq = clock;
	fm_params.f_Exception = cfg->exception_callback;
	fm_params.f_BusError = cfg->bus_error_callback;
	fm_params.h_App = cfg->fman_device;
	fm_params.irq = cfg->irq_num;
	fm_params.errIrq = cfg->err_irq_num;

	fm_params.firmware.size = fman_firmware_size;
	fm_params.firmware.p_Code = (uint32_t*)fman_firmware;

	fm_handle = FM_Config(&fm_params);
	if (fm_handle == NULL) {
		device_printf(cfg->fman_device, "couldn't configure FM "
		    "module\n");
		goto err;
	}

	FM_ConfigResetOnInit(fm_handle, TRUE);

	error = FM_Init(fm_handle);
	if (error != E_OK) {
		device_printf(cfg->fman_device, "couldn't init FM module\n");
		goto err2;
	}

	error = FM_GetRevision(fm_handle, &revision_info);
	if (error != E_OK) {
		device_printf(cfg->fman_device, "couldn't get FM revision\n");
		goto err2;
	}

	device_printf(cfg->fman_device, "Hardware version: %d.%d.\n",
	    revision_info.majorRev, revision_info.minorRev);

	return (fm_handle);

err2:
	FM_Free(fm_handle);
err:
	FM_MURAM_Free(muram_handle);
	return (NULL);
}

static void
fman_exception_callback(t_Handle app_handle, e_FmExceptions exception)
{
	struct fman_softc *sc;

	sc = app_handle;
	device_printf(sc->dev, "FMan exception occurred.\n");
}

static void
fman_error_callback(t_Handle app_handle, e_FmPortType port_type,
    uint8_t port_id, uint64_t addr, uint8_t tnum, uint16_t liodn)
{
	struct fman_softc *sc;

	sc = app_handle;
	device_printf(sc->dev, "FMan error occurred.\n");
}
/** @} */


/**
 * @group FMan driver interface.
 * @{
 */

int
fman_get_handle(t_Handle *fmh)
{

	if (fm_sc == NULL)
		return (ENOMEM);

	*fmh = fm_sc->fm_handle;

	return (0);
}

int
fman_get_muram_handle(t_Handle *muramh)
{

	if (fm_sc == NULL)
		return (ENOMEM);

	*muramh = fm_sc->muram_handle;

	return (0);
}

int
fman_get_bushandle(vm_offset_t *fm_base)
{

	if (fm_sc == NULL)
		return (ENOMEM);

	*fm_base = rman_get_bushandle(fm_sc->mem_res);

	return (0);
}

int
fman_attach(device_t dev)
{
	struct fman_softc *sc;
	struct fman_config cfg;

	sc = device_get_softc(dev);
	sc->dev = dev;
	fm_sc = sc;

	/* Check if MallocSmart allocator is ready */
	if (XX_MallocSmartInit() != E_OK) {
		device_printf(dev, "could not initialize smart allocator.\n");
		return (ENXIO);
	}

	XX_TrackInit();

	sc->mem_rid = 0;
	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &sc->mem_rid,
	    RF_ACTIVE);
	if (!sc->mem_res) {
		device_printf(dev, "could not allocate memory.\n");
		return (ENXIO);
	}

	sc->irq_rid = 0;
	sc->irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &sc->irq_rid,
	    RF_ACTIVE);
	if (!sc->irq_res) {
		device_printf(dev, "could not allocate interrupt.\n");
		goto err;
	}

	/*
	 * XXX: Fix FMan interrupt. This is workaround for the issue with
	 * interrupts directed to multiple CPUs by the interrupts subsystem.
	 * Workaround is to bind the interrupt to only one CPU0.
	 */
	XX_FmanFixIntr(rman_get_start(sc->irq_res));

	sc->err_irq_rid = 1;
	sc->err_irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ,
	    &sc->err_irq_rid, RF_ACTIVE | RF_SHAREABLE);
	if (!sc->err_irq_res) {
		device_printf(dev, "could not allocate error interrupt.\n");
		goto err;
	}

	/* Set FMan configuration */
	cfg.fman_device = dev;
	cfg.fm_id = device_get_unit(dev);
	cfg.mem_base_addr = rman_get_bushandle(sc->mem_res);
	cfg.irq_num = (int)sc->irq_res;
	cfg.err_irq_num = (int)sc->err_irq_res;
	cfg.exception_callback = fman_exception_callback;
	cfg.bus_error_callback = fman_error_callback;

	sc->fm_handle = fman_init(sc, &cfg);
	if (sc->fm_handle == NULL) {
		device_printf(dev, "could not be configured\n");
		return (ENXIO);
	}

	return (bus_generic_attach(dev));

err:
	fman_detach(dev);
	return (ENXIO);
}

int
fman_detach(device_t dev)
{
	struct fman_softc *sc;

	sc = device_get_softc(dev);

	if (sc->muram_handle) {
		FM_MURAM_Free(sc->muram_handle);
	}

	if (sc->fm_handle) {
		FM_Free(sc->fm_handle);
	}

	if (sc->mem_res) {
		bus_release_resource(dev, SYS_RES_MEMORY, sc->mem_rid,
		    sc->mem_res);
	}

	if (sc->irq_res) {
		bus_release_resource(dev, SYS_RES_IRQ, sc->irq_rid,
		    sc->irq_res);
	}

	if (sc->irq_res) {
		bus_release_resource(dev, SYS_RES_IRQ, sc->err_irq_rid,
		    sc->err_irq_res);
	}

	return (0);
}

int
fman_suspend(device_t dev)
{

	return (0);
}

int
fman_resume(device_t dev)
{

	return (0);
}

int
fman_shutdown(device_t dev)
{

	return (0);
}

/** @} */
