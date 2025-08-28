/*-
 * Copyright 2016-2025 Microchip Technology, Inc. and/or its subsidiaries.
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


/*
 * Driver for the Microsemi Smart storage controllers
 */

#include "smartpqi_includes.h"
#include "smartpqi_controllers.h"

CTASSERT(BSD_SUCCESS == PQI_STATUS_SUCCESS);

/*
 * Logging levels global
*/
unsigned long logging_level  = PQISRC_LOG_LEVEL;

/*
 * Function to identify the installed adapter.
 */
static struct pqi_ident *
pqi_find_ident(device_t dev)
{
	struct pqi_ident *m;
	u_int16_t vendid, devid, sub_vendid, sub_devid;
	static long AllowWildcards = 0xffffffff;
	int result;

#ifdef DEVICE_HINT
	if (AllowWildcards == 0xffffffff)
	{
		result = resource_long_value("smartpqi", 0, "allow_wildcards", &AllowWildcards);

		/* the default case if the hint is not found is to allow wildcards */
		if (result != DEVICE_HINT_SUCCESS) {
			AllowWildcards = 1;
		}
	}

#endif

	vendid = pci_get_vendor(dev);
	devid = pci_get_device(dev);
	sub_vendid = pci_get_subvendor(dev);
	sub_devid = pci_get_subdevice(dev);

	for (m = pqi_identifiers; m->vendor != 0; m++) {
		if ((m->vendor == vendid) && (m->device == devid) &&
			(m->subvendor == sub_vendid) &&
			(m->subdevice == sub_devid)) {
			return (m);
		}
	}

	for (m = pqi_family_identifiers; m->vendor != 0; m++) {
		if ((m->vendor == vendid) && (m->device == devid)) {
			if (AllowWildcards != 0)
			{
				DBG_NOTE("Controller device ID matched using wildcards\n");
				return (m);
			}
			else
			{
				DBG_NOTE("Controller not probed because device ID wildcards are disabled\n")
				return (NULL);
			}
		}
	}

	return (NULL);
}

/*
 * Determine whether this is one of our supported adapters.
 */
static int
smartpqi_probe(device_t dev)
{
	struct pqi_ident *id;

	if ((id = pqi_find_ident(dev)) != NULL) {
		device_set_desc(dev, id->desc);
		return(BUS_PROBE_VENDOR);
	}

	return(ENXIO);
}

/*
 * Store Bus/Device/Function in softs
 */
void
pqisrc_save_controller_info(struct pqisrc_softstate *softs)
{
	device_t dev = softs->os_specific.pqi_dev;

	softs->bus_id = (uint32_t)pci_get_bus(dev);
	softs->device_id = (uint32_t)pci_get_device(dev);
	softs->func_id = (uint32_t)pci_get_function(dev);
}


static void read_device_hint_resource(struct pqisrc_softstate *softs,
		char *keyword,  uint32_t *value)
{
	DBG_FUNC("IN\n");

	long result = 0;

	device_t dev = softs->os_specific.pqi_dev;

	if (resource_long_value("smartpqi", device_get_unit(dev), keyword, &result) == DEVICE_HINT_SUCCESS) {
		if (result) {
			/* set resource to 1 for disabling the
			 * firmware feature in device hint file. */
			*value = 0;

		}
		else {
			/* set resource to 0 for enabling the
			 * firmware feature in device hint file. */
			*value = 1;
		}
	}
	else {
		/* Enabled by default */
		*value = 1;
	}

	DBG_NOTE("SmartPQI Device Hint: %s, Is it enabled = %u\n", keyword, *value);

	DBG_FUNC("OUT\n");
}

static void read_device_hint_decimal_value(struct pqisrc_softstate *softs,
		char *keyword, uint32_t *value)
{
	DBG_FUNC("IN\n");

	long result = 0;

	device_t dev = softs->os_specific.pqi_dev;

	if (resource_long_value("smartpqi", device_get_unit(dev), keyword, &result) == DEVICE_HINT_SUCCESS) {
		/* Nothing to do here. Value reads
		 * directly from Device.Hint file */
		*value = result;
	}
	else {
		/* Set to max to determine the value */
		*value = 0XFFFF;
	}

	DBG_FUNC("OUT\n");
}

static void smartpqi_read_all_device_hint_file_entries(struct pqisrc_softstate *softs)
{
	uint32_t value = 0;

	DBG_FUNC("IN\n");

	/* hint.smartpqi.0.stream_disable =  "0" */
	read_device_hint_resource(softs, STREAM_DETECTION, &value);
	softs->hint.stream_status = value;

	/* hint.smartpqi.0.sata_unique_wwn_disable =  "0" */
	read_device_hint_resource(softs, SATA_UNIQUE_WWN, &value);
	softs->hint.sata_unique_wwn_status = value;

	/* hint.smartpqi.0.aio_raid1_write_disable =  "0" */
	read_device_hint_resource(softs, AIO_RAID1_WRITE_BYPASS, &value);
	softs->hint.aio_raid1_write_status = value;

	/* hint.smartpqi.0.aio_raid5_write_disable =  "0" */
	read_device_hint_resource(softs, AIO_RAID5_WRITE_BYPASS, &value);
	softs->hint.aio_raid5_write_status = value;

	/* hint.smartpqi.0.aio_raid6_write_disable =  "0" */
	read_device_hint_resource(softs, AIO_RAID6_WRITE_BYPASS, &value);
	softs->hint.aio_raid6_write_status = value;

	/* hint.smartpqi.0.queue_depth =  "0" */
	read_device_hint_decimal_value(softs, ADAPTER_QUEUE_DEPTH, &value);
	softs->hint.queue_depth = value;

	/* hint.smartpqi.0.sg_count =  "0" */
	read_device_hint_decimal_value(softs, SCATTER_GATHER_COUNT, &value);
	softs->hint.sg_segments = value;

	DBG_FUNC("IN\n");
}

/* Get the driver parameter tunables. */
static void
smartpqi_get_tunables(void)
{
   /*
    * Temp variable used to get the value from loader.conf.
    * Initializing it with the current logging level value.
    */
	unsigned long logging_level_temp = PQISRC_LOG_LEVEL;

	TUNABLE_ULONG_FETCH("hw.smartpqi.debug_level", &logging_level_temp);

   DBG_SET_LOGGING_LEVEL(logging_level_temp);
}

/*
 * Allocate resources for our device, set up the bus interface.
 * Initialize the PQI related functionality, scan devices, register sim to
 * upper layer, create management interface device node etc.
 */
static int
smartpqi_attach(device_t dev)
{
	struct pqisrc_softstate *softs;
	struct pqi_ident *id = NULL;
	int error = BSD_SUCCESS;
	u_int32_t command = 0, i = 0;
	int card_index = device_get_unit(dev);
	rcb_t *rcbp = NULL;

	/*
	 * Initialize softc.
	 */
	softs = device_get_softc(dev);

	if (!softs) {
		printf("Could not get softc\n");
		error = EINVAL;
		goto out;
	}
	memset(softs, 0, sizeof(*softs));
	softs->os_specific.pqi_dev = dev;

    smartpqi_get_tunables();

	DBG_FUNC("IN\n");

	/* assume failure is 'not configured' */
	error = ENXIO;

	/*
	 * Verify that the adapter is correctly set up in PCI space.
	 */
	pci_enable_busmaster(softs->os_specific.pqi_dev);
	command = pci_read_config(softs->os_specific.pqi_dev, PCIR_COMMAND, 2);
	if ((command & PCIM_CMD_MEMEN) == 0) {
		DBG_ERR("memory window not available command = %d\n", command);
		error = ENXIO;
		goto out;
	}

	/*
	 * Detect the hardware interface version, set up the bus interface
	 * indirection.
	 */
	id = pqi_find_ident(dev);
	if (!id) {
		DBG_ERR("NULL return value from pqi_find_ident\n");
		goto out;
	}

	softs->os_specific.pqi_hwif = id->hwif;

	switch(softs->os_specific.pqi_hwif) {
		case PQI_HWIF_SRCV:
			DBG_INFO("set hardware up for PMC SRCv for %p\n", softs);
			break;
		default:
			softs->os_specific.pqi_hwif = PQI_HWIF_UNKNOWN;
			DBG_ERR("unknown hardware type\n");
			error = ENXIO;
			goto out;
	}

	pqisrc_save_controller_info(softs);

	/*
	 * Allocate the PCI register window.
	 */
	softs->os_specific.pqi_regs_rid0 = PCIR_BAR(0);
	if ((softs->os_specific.pqi_regs_res0 =
		bus_alloc_resource_any(softs->os_specific.pqi_dev, SYS_RES_MEMORY,
		&softs->os_specific.pqi_regs_rid0, RF_ACTIVE)) == NULL) {
		DBG_ERR("couldn't allocate register window 0\n");
		/* assume failure is 'out of memory' */
		error = ENOMEM;
		goto out;
	}

	bus_get_resource_start(softs->os_specific.pqi_dev, SYS_RES_MEMORY,
		softs->os_specific.pqi_regs_rid0);

	softs->pci_mem_handle.pqi_btag = rman_get_bustag(softs->os_specific.pqi_regs_res0);
	softs->pci_mem_handle.pqi_bhandle = rman_get_bushandle(softs->os_specific.pqi_regs_res0);
	/* softs->pci_mem_base_vaddr = (uintptr_t)rman_get_virtual(softs->os_specific.pqi_regs_res0); */
	softs->pci_mem_base_vaddr = (char *)rman_get_virtual(softs->os_specific.pqi_regs_res0);

	/*
	 * Allocate the parent bus DMA tag appropriate for our PCI interface.
	 *
	 * Note that some of these controllers are 64-bit capable.
	 */
	if (bus_dma_tag_create(bus_get_dma_tag(dev), 	/* parent */
				PAGE_SIZE, 0,		/* algnmnt, boundary */
				BUS_SPACE_MAXADDR,/* lowaddr */
				BUS_SPACE_MAXADDR, 	/* highaddr */
				NULL, NULL, 		/* filter, filterarg */
				BUS_SPACE_MAXSIZE,	/* maxsize */
				BUS_SPACE_UNRESTRICTED,	/* nsegments */
				BUS_SPACE_MAXSIZE,	/* maxsegsize */
				0,			/* flags */
				NULL, NULL,		/* No locking needed */
				&softs->os_specific.pqi_parent_dmat)) {
		DBG_ERR("can't allocate parent DMA tag\n");
		/* assume failure is 'out of memory' */
		error = ENOMEM;
		goto dma_out;
	}

	softs->os_specific.sim_registered = FALSE;
	softs->os_name = "FreeBSD ";

	smartpqi_read_all_device_hint_file_entries(softs);

	/* Initialize the PQI library */
	error = pqisrc_init(softs);
	if (error != PQI_STATUS_SUCCESS) {
		DBG_ERR("Failed to initialize pqi lib error = %d\n", error);
		error = ENXIO;
		goto out;
	}
	else {
		error = BSD_SUCCESS;
	}

        mtx_init(&softs->os_specific.cam_lock, "cam_lock", NULL, MTX_DEF);
        softs->os_specific.mtx_init = TRUE;
        mtx_init(&softs->os_specific.map_lock, "map_lock", NULL, MTX_DEF);

	callout_init(&softs->os_specific.wellness_periodic, 1);
	callout_init(&softs->os_specific.heartbeat_timeout_id, 1);

        /*
         * Create DMA tag for mapping buffers into controller-addressable space.
         */
        if (bus_dma_tag_create(softs->os_specific.pqi_parent_dmat,/* parent */
				1, 0,			/* algnmnt, boundary */
				BUS_SPACE_MAXADDR,	/* lowaddr */
				BUS_SPACE_MAXADDR,	/* highaddr */
				NULL, NULL,		/* filter, filterarg */
				(bus_size_t)softs->pqi_cap.max_sg_elem*PAGE_SIZE,/* maxsize */
				softs->pqi_cap.max_sg_elem,	/* nsegments */
				BUS_SPACE_MAXSIZE,	/* maxsegsize */
				BUS_DMA_ALLOCNOW,		/* flags */
				busdma_lock_mutex,		/* lockfunc */
				&softs->os_specific.map_lock,	/* lockfuncarg*/
				&softs->os_specific.pqi_buffer_dmat)) {
		DBG_ERR("can't allocate buffer DMA tag for pqi_buffer_dmat\n");
		return (ENOMEM);
        }

	rcbp = &softs->rcb[1];
	for( i = 1;  i <= softs->pqi_cap.max_outstanding_io; i++, rcbp++ ) {
		if ((error = bus_dmamap_create(softs->os_specific.pqi_buffer_dmat, 0, &rcbp->cm_datamap)) != 0) {
			DBG_ERR("Cant create datamap for buf @"
			"rcbp = %p maxio = %u error = %d\n",
			rcbp, softs->pqi_cap.max_outstanding_io, error);
			goto dma_out;
		}
	}

	os_start_heartbeat_timer((void *)softs); /* Start the heart-beat timer */
	callout_reset(&softs->os_specific.wellness_periodic, 120 * hz,
			os_wellness_periodic, softs);

	error = pqisrc_scan_devices(softs);
	if (error != PQI_STATUS_SUCCESS) {
		DBG_ERR("Failed to scan lib error = %d\n", error);
		error = ENXIO;
		goto out;
	}
	else {
		error = BSD_SUCCESS;
	}

	error = register_sim(softs, card_index);
	if (error) {
		DBG_ERR("Failed to register sim index = %d error = %d\n",
			card_index, error);
		goto out;
	}

	smartpqi_target_rescan(softs);

	TASK_INIT(&softs->os_specific.event_task, 0, pqisrc_event_worker,softs);

	error = create_char_dev(softs, card_index);
	if (error) {
		DBG_ERR("Failed to register character device index=%d r=%d\n",
			card_index, error);
		goto out;
	}

	goto out;

dma_out:
	if (softs->os_specific.pqi_regs_res0 != NULL)
		bus_release_resource(softs->os_specific.pqi_dev, SYS_RES_MEMORY,
			softs->os_specific.pqi_regs_rid0,
			softs->os_specific.pqi_regs_res0);
out:
	DBG_FUNC("OUT error = %d\n", error);

	return(error);
}

/*
 * Deallocate resources for our device.
 */
static int
smartpqi_detach(device_t dev)
{
	struct pqisrc_softstate *softs = device_get_softc(dev);
	int rval = BSD_SUCCESS;

	DBG_FUNC("IN\n");

	if (softs == NULL)
		return ENXIO;

	/* kill the periodic event */
	callout_drain(&softs->os_specific.wellness_periodic);
	/* Kill the heart beat event */
	callout_drain(&softs->os_specific.heartbeat_timeout_id);

	if (!pqisrc_ctrl_offline(softs)) {
		rval = pqisrc_flush_cache(softs, PQISRC_NONE_CACHE_FLUSH_ONLY);
		if (rval != PQI_STATUS_SUCCESS) {
			DBG_ERR("Unable to flush adapter cache! rval = %d\n", rval);
			rval = EIO;
		} else {
			rval = BSD_SUCCESS;
		}
	}

	destroy_char_dev(softs);
	pqisrc_uninit(softs);
	deregister_sim(softs);
	pci_release_msi(dev);

	DBG_FUNC("OUT\n");

	return rval;
}

/*
 * Bring the controller to a quiescent state, ready for system suspend.
 */
static int
smartpqi_suspend(device_t dev)
{
	struct pqisrc_softstate *softs = device_get_softc(dev);

	DBG_FUNC("IN\n");

	if (softs == NULL)
		return ENXIO;

	DBG_INFO("Suspending the device %p\n", softs);
	softs->os_specific.pqi_state |= SMART_STATE_SUSPEND;

	DBG_FUNC("OUT\n");

	return BSD_SUCCESS;
}

/*
 * Bring the controller back to a state ready for operation.
 */
static int
smartpqi_resume(device_t dev)
{
	struct pqisrc_softstate *softs = device_get_softc(dev);

	DBG_FUNC("IN\n");

	if (softs == NULL)
		return ENXIO;

	softs->os_specific.pqi_state &= ~SMART_STATE_SUSPEND;

	DBG_FUNC("OUT\n");

	return BSD_SUCCESS;
}

/*
 * Do whatever is needed during a system shutdown.
 */
static int
smartpqi_shutdown(device_t dev)
{
	struct pqisrc_softstate *softs = device_get_softc(dev);
	int bsd_status = BSD_SUCCESS;
	int pqi_status;

	DBG_FUNC("IN\n");

	if (softs == NULL)
		return ENXIO;

	if (pqisrc_ctrl_offline(softs))
		return BSD_SUCCESS;

	pqi_status = pqisrc_flush_cache(softs, PQISRC_SHUTDOWN);
	if (pqi_status != PQI_STATUS_SUCCESS) {
		DBG_ERR("Unable to flush adapter cache! rval = %d\n", pqi_status);
		bsd_status = EIO;
	}

	DBG_FUNC("OUT\n");

	return bsd_status;
}


/*
 * PCI bus interface.
 */
static device_method_t pqi_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		smartpqi_probe),
	DEVMETHOD(device_attach,	smartpqi_attach),
	DEVMETHOD(device_detach,	smartpqi_detach),
	DEVMETHOD(device_suspend,	smartpqi_suspend),
	DEVMETHOD(device_resume,	smartpqi_resume),
	DEVMETHOD(device_shutdown,	smartpqi_shutdown),
	{ 0, 0 }
};

static driver_t smartpqi_pci_driver = {
	"smartpqi",
	pqi_methods,
	sizeof(struct pqisrc_softstate)
};

DRIVER_MODULE(smartpqi, pci, smartpqi_pci_driver, 0, 0);

MODULE_DEPEND(smartpqi, pci, 1, 1, 1);
