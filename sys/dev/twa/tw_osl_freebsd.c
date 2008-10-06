/*
 * Copyright (c) 2004-07 Applied Micro Circuits Corporation.
 * Copyright (c) 2004-05 Vinod Kashyap.
 * Copyright (c) 2000 Michael Smith
 * Copyright (c) 2000 BSDi
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
 *	$FreeBSD$
 */

/*
 * AMCC'S 3ware driver for 9000 series storage controllers.
 *
 * Author: Vinod Kashyap
 * Modifications by: Adam Radford
 * Modifications by: Manjunath Ranganathaiah
 */


/*
 * FreeBSD specific functions not related to CAM, and other
 * miscellaneous functions.
 */


#include <dev/twa/tw_osl_includes.h>
#include <dev/twa/tw_cl_fwif.h>
#include <dev/twa/tw_cl_ioctl.h>
#include <dev/twa/tw_osl_ioctl.h>

#ifdef TW_OSL_DEBUG
TW_INT32	TW_DEBUG_LEVEL_FOR_OSL = TW_OSL_DEBUG;
TW_INT32	TW_OSL_DEBUG_LEVEL_FOR_CL = TW_OSL_DEBUG;
#endif /* TW_OSL_DEBUG */

MALLOC_DEFINE(TW_OSLI_MALLOC_CLASS, "twa_commands", "twa commands");


static	d_open_t		twa_open;
static	d_close_t		twa_close;
static	d_ioctl_t		twa_ioctl;

static struct cdevsw twa_cdevsw = {
	.d_version =	D_VERSION,
	.d_open =	twa_open,
	.d_close =	twa_close,
	.d_ioctl =	twa_ioctl,
	.d_name =	"twa",
};

static devclass_t	twa_devclass;


/*
 * Function name:	twa_open
 * Description:		Called when the controller is opened.
 *			Simply marks the controller as open.
 *
 * Input:		dev	-- control device corresponding to the ctlr
 *			flags	-- mode of open
 *			fmt	-- device type (character/block etc.)
 *			proc	-- current process
 * Output:		None
 * Return value:	0	-- success
 *			non-zero-- failure
 */
static TW_INT32
twa_open(struct cdev *dev, TW_INT32 flags, TW_INT32 fmt, d_thread_t *proc)
{
	TW_INT32		unit = dev2unit(dev);
	struct twa_softc	*sc = devclass_get_softc(twa_devclass, unit);

	tw_osli_dbg_dprintf(5, sc, "entered");
	sc->state |= TW_OSLI_CTLR_STATE_OPEN;
	return(0);
}



/*
 * Function name:	twa_close
 * Description:		Called when the controller is closed.
 *			Simply marks the controller as not open.
 *
 * Input:		dev	-- control device corresponding to the ctlr
 *			flags	-- mode of corresponding open
 *			fmt	-- device type (character/block etc.)
 *			proc	-- current process
 * Output:		None
 * Return value:	0	-- success
 *			non-zero-- failure
 */
static TW_INT32
twa_close(struct cdev *dev, TW_INT32 flags, TW_INT32 fmt, d_thread_t *proc)
{
	TW_INT32		unit = dev2unit(dev);
	struct twa_softc	*sc = devclass_get_softc(twa_devclass, unit);

	tw_osli_dbg_dprintf(5, sc, "entered");
	sc->state &= ~TW_OSLI_CTLR_STATE_OPEN;
	return(0);
}



/*
 * Function name:	twa_ioctl
 * Description:		Called when an ioctl is posted to the controller.
 *			Handles any OS Layer specific cmds, passes the rest
 *			on to the Common Layer.
 *
 * Input:		dev	-- control device corresponding to the ctlr
 *			cmd	-- ioctl cmd
 *			buf	-- ptr to buffer in kernel memory, which is
 *				   a copy of the input buffer in user-space
 *			flags	-- mode of corresponding open
 *			proc	-- current process
 * Output:		buf	-- ptr to buffer in kernel memory, which will
 *				   be copied to the output buffer in user-space
 * Return value:	0	-- success
 *			non-zero-- failure
 */
static TW_INT32
twa_ioctl(struct cdev *dev, u_long cmd, caddr_t buf, TW_INT32 flags, d_thread_t *proc)
{
	struct twa_softc	*sc = (struct twa_softc *)(dev->si_drv1);
	TW_INT32		error;

	tw_osli_dbg_dprintf(5, sc, "entered");

	switch (cmd) {
	case TW_OSL_IOCTL_FIRMWARE_PASS_THROUGH:
		tw_osli_dbg_dprintf(6, sc, "ioctl: fw_passthru");
		error = tw_osli_fw_passthru(sc, (TW_INT8 *)buf);
		break;

	case TW_OSL_IOCTL_SCAN_BUS:
		/* Request CAM for a bus scan. */
		tw_osli_dbg_dprintf(6, sc, "ioctl: scan bus");
		error = tw_osli_request_bus_scan(sc);
		break;

	default:
		tw_osli_dbg_dprintf(6, sc, "ioctl: 0x%lx", cmd);
		error = tw_cl_ioctl(&sc->ctlr_handle, cmd, buf);
		break;
	}
	return(error);
}



static TW_INT32	twa_probe(device_t dev);
static TW_INT32	twa_attach(device_t dev);
static TW_INT32	twa_detach(device_t dev);
static TW_INT32	twa_shutdown(device_t dev);
static TW_VOID	twa_busdma_lock(TW_VOID *lock_arg, bus_dma_lock_op_t op);
#ifdef TW_OSLI_DEFERRED_INTR_USED
static int	twa_pci_intr_fast(TW_VOID *arg);
static TW_VOID	twa_deferred_intr(TW_VOID *context, TW_INT32 pending);
#else
static TW_VOID	twa_pci_intr(TW_VOID *arg);
#endif /* TW_OSLI_DEFERRED_INTR_USED */

static TW_INT32	tw_osli_alloc_mem(struct twa_softc *sc);
static TW_VOID	tw_osli_free_resources(struct twa_softc *sc);

static TW_VOID	twa_map_load_data_callback(TW_VOID *arg,
	bus_dma_segment_t *segs, TW_INT32 nsegments, TW_INT32 error);
static TW_VOID	twa_map_load_callback(TW_VOID *arg,
	bus_dma_segment_t *segs, TW_INT32 nsegments, TW_INT32 error);


static device_method_t	twa_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		twa_probe),
	DEVMETHOD(device_attach,	twa_attach),
	DEVMETHOD(device_detach,	twa_detach),
	DEVMETHOD(device_shutdown,	twa_shutdown),

	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),
	{0, 0}
};

static driver_t	twa_pci_driver = {
	"twa",
	twa_methods,
	sizeof(struct twa_softc)
};

DRIVER_MODULE(twa, pci, twa_pci_driver, twa_devclass, 0, 0);
MODULE_DEPEND(twa, cam, 1, 1, 1);
MODULE_DEPEND(twa, pci, 1, 1, 1);


/*
 * Function name:	twa_probe
 * Description:		Called at driver load time.  Claims 9000 ctlrs.
 *
 * Input:		dev	-- bus device corresponding to the ctlr
 * Output:		None
 * Return value:	<= 0	-- success
 *			> 0	-- failure
 */
static TW_INT32
twa_probe(device_t dev)
{
	static TW_UINT8	first_ctlr = 1;

	tw_osli_dbg_printf(3, "entered");

	if (tw_cl_ctlr_supported(pci_get_vendor(dev), pci_get_device(dev))) {
		device_set_desc(dev, TW_OSLI_DEVICE_NAME);
		/* Print the driver version only once. */
		if (first_ctlr) {
			printf("3ware device driver for 9000 series storage "
				"controllers, version: %s\n",
				TW_OSL_DRIVER_VERSION_STRING);
			first_ctlr = 0;
		}
		return(0);
	}
	return(ENXIO);
}



/*
 * Function name:	twa_attach
 * Description:		Allocates pci resources; updates sc; adds a node to the
 *			sysctl tree to expose the driver version; makes calls
 *			(to the Common Layer) to initialize ctlr, and to
 *			attach to CAM.
 *
 * Input:		dev	-- bus device corresponding to the ctlr
 * Output:		None
 * Return value:	0	-- success
 *			non-zero-- failure
 */
static TW_INT32
twa_attach(device_t dev)
{
	struct twa_softc	*sc = device_get_softc(dev);
	TW_UINT32		command;
	TW_INT32		bar_num;
	TW_INT32		bar0_offset;
	TW_INT32		bar_size;
	TW_INT32		error;

	tw_osli_dbg_dprintf(3, sc, "entered");

	sc->ctlr_handle.osl_ctlr_ctxt = sc;

	/* Initialize the softc structure. */
	sc->bus_dev = dev;
	sc->device_id = pci_get_device(dev);

	/* Initialize the mutexes right here. */
	sc->io_lock = &(sc->io_lock_handle);
	mtx_init(sc->io_lock, "tw_osl_io_lock", NULL, MTX_SPIN);
	sc->q_lock = &(sc->q_lock_handle);
	mtx_init(sc->q_lock, "tw_osl_q_lock", NULL, MTX_SPIN);
	sc->sim_lock = &(sc->sim_lock_handle);
	mtx_init(sc->sim_lock, "tw_osl_sim_lock", NULL, MTX_DEF | MTX_RECURSE);

	sysctl_ctx_init(&sc->sysctl_ctxt);
	sc->sysctl_tree = SYSCTL_ADD_NODE(&sc->sysctl_ctxt,
		SYSCTL_STATIC_CHILDREN(_hw), OID_AUTO,
		device_get_nameunit(dev), CTLFLAG_RD, 0, "");
	if (sc->sysctl_tree == NULL) {
		tw_osli_printf(sc, "error = %d",
			TW_CL_SEVERITY_ERROR_STRING,
			TW_CL_MESSAGE_SOURCE_FREEBSD_DRIVER,
			0x2000,
			"Cannot add sysctl tree node",
			ENXIO);
		return(ENXIO);
	}
	SYSCTL_ADD_STRING(&sc->sysctl_ctxt, SYSCTL_CHILDREN(sc->sysctl_tree),
		OID_AUTO, "driver_version", CTLFLAG_RD,
		TW_OSL_DRIVER_VERSION_STRING, 0, "TWA driver version");

	/* Make sure we are going to be able to talk to this board. */
	command = pci_read_config(dev, PCIR_COMMAND, 2);
	if ((command & PCIM_CMD_PORTEN) == 0) {
		tw_osli_printf(sc, "error = %d",
			TW_CL_SEVERITY_ERROR_STRING,
			TW_CL_MESSAGE_SOURCE_FREEBSD_DRIVER,
			0x2001,
			"Register window not available",
			ENXIO);
		tw_osli_free_resources(sc);
		return(ENXIO);
	}

	/* Force the busmaster enable bit on, in case the BIOS forgot. */
	command |= PCIM_CMD_BUSMASTEREN;
	pci_write_config(dev, PCIR_COMMAND, command, 2);

	/* Allocate the PCI register window. */
	if ((error = tw_cl_get_pci_bar_info(sc->device_id, TW_CL_BAR_TYPE_MEM,
		&bar_num, &bar0_offset, &bar_size))) {
		tw_osli_printf(sc, "error = %d",
			TW_CL_SEVERITY_ERROR_STRING,
			TW_CL_MESSAGE_SOURCE_FREEBSD_DRIVER,
			0x201F,
			"Can't get PCI BAR info",
			error);
		tw_osli_free_resources(sc);
		return(error);
	}
	sc->reg_res_id = PCIR_BARS + bar0_offset;
	if ((sc->reg_res = bus_alloc_resource(dev, SYS_RES_MEMORY,
				&(sc->reg_res_id), 0, ~0, 1, RF_ACTIVE))
				== NULL) {
		tw_osli_printf(sc, "error = %d",
			TW_CL_SEVERITY_ERROR_STRING,
			TW_CL_MESSAGE_SOURCE_FREEBSD_DRIVER,
			0x2002,
			"Can't allocate register window",
			ENXIO);
		tw_osli_free_resources(sc);
		return(ENXIO);
	}
	sc->bus_tag = rman_get_bustag(sc->reg_res);
	sc->bus_handle = rman_get_bushandle(sc->reg_res);

	/* Allocate and register our interrupt. */
	sc->irq_res_id = 0;
	if ((sc->irq_res = bus_alloc_resource(sc->bus_dev, SYS_RES_IRQ,
				&(sc->irq_res_id), 0, ~0, 1,
				RF_SHAREABLE | RF_ACTIVE)) == NULL) {
		tw_osli_printf(sc, "error = %d",
			TW_CL_SEVERITY_ERROR_STRING,
			TW_CL_MESSAGE_SOURCE_FREEBSD_DRIVER,
			0x2003,
			"Can't allocate interrupt",
			ENXIO);
		tw_osli_free_resources(sc);
		return(ENXIO);
	}
	if ((error = bus_setup_intr(sc->bus_dev, sc->irq_res,
#ifdef TW_OSLI_DEFERRED_INTR_USED
			INTR_TYPE_CAM | INTR_FAST,
			twa_pci_intr_fast, NULL,
#else
			INTR_TYPE_CAM | INTR_MPSAFE,
			NULL, twa_pci_intr,	    
#endif 
			sc, &sc->intr_handle))) {
		tw_osli_printf(sc, "error = %d",
			TW_CL_SEVERITY_ERROR_STRING,
			TW_CL_MESSAGE_SOURCE_FREEBSD_DRIVER,
			0x2004,
			"Can't set up interrupt",
			error);
		tw_osli_free_resources(sc);
		return(error);
	}

#ifdef TW_OSLI_DEFERRED_INTR_USED
	TASK_INIT(&sc->deferred_intr_callback, 0, twa_deferred_intr, sc);
#endif /* TW_OSLI_DEFERRED_INTR_USED */

	if ((error = tw_osli_alloc_mem(sc))) {
		tw_osli_printf(sc, "error = %d",
			TW_CL_SEVERITY_ERROR_STRING,
			TW_CL_MESSAGE_SOURCE_FREEBSD_DRIVER,
			0x2005,
			"Memory allocation failure",
			error);
		tw_osli_free_resources(sc);
		return(error);
	}

	/* Initialize the Common Layer for this controller. */
	if ((error = tw_cl_init_ctlr(&sc->ctlr_handle, sc->flags, sc->device_id,
			TW_OSLI_MAX_NUM_IOS, TW_OSLI_MAX_NUM_AENS,
			sc->non_dma_mem, sc->dma_mem,
			sc->dma_mem_phys
			))) {
		tw_osli_printf(sc, "error = %d",
			TW_CL_SEVERITY_ERROR_STRING,
			TW_CL_MESSAGE_SOURCE_FREEBSD_DRIVER,
			0x2006,
			"Failed to initialize Common Layer/controller",
			error);
		tw_osli_free_resources(sc);
		return(error);
	}

	/* Create the control device. */
	sc->ctrl_dev = make_dev(&twa_cdevsw, device_get_unit(sc->bus_dev),
			UID_ROOT, GID_OPERATOR, S_IRUSR | S_IWUSR,
			"twa%d", device_get_unit(sc->bus_dev));
	sc->ctrl_dev->si_drv1 = sc;

	if ((error = tw_osli_cam_attach(sc))) {
		tw_osli_free_resources(sc);
		tw_osli_printf(sc, "error = %d",
			TW_CL_SEVERITY_ERROR_STRING,
			TW_CL_MESSAGE_SOURCE_FREEBSD_DRIVER,
			0x2007,
			"Failed to initialize CAM",
			error);
		return(error);
	}

	return(0);
}



/*
 * Function name:	tw_osli_alloc_mem
 * Description:		Allocates memory needed both by CL and OSL.
 *
 * Input:		sc	-- OSL internal controller context
 * Output:		None
 * Return value:	0	-- success
 *			non-zero-- failure
 */
static TW_INT32
tw_osli_alloc_mem(struct twa_softc *sc)
{
	struct tw_osli_req_context	*req;
	TW_UINT32			max_sg_elements;
	TW_UINT32			non_dma_mem_size;
	TW_UINT32			dma_mem_size;
	TW_INT32			error;
	TW_INT32			i;

	tw_osli_dbg_dprintf(3, sc, "entered");

	sc->flags |= (sizeof(bus_addr_t) == 8) ? TW_CL_64BIT_ADDRESSES : 0;
	sc->flags |= (sizeof(bus_size_t) == 8) ? TW_CL_64BIT_SG_LENGTH : 0;
#ifdef TW_OSLI_DEFERRED_INTR_USED
	sc->flags |= TW_CL_DEFERRED_INTR_USED; 
#endif /* TW_OSLI_DEFERRED_INTR_USED */

	max_sg_elements = (sizeof(bus_addr_t) == 8) ?
		TW_CL_MAX_64BIT_SG_ELEMENTS : TW_CL_MAX_32BIT_SG_ELEMENTS;

	if ((error = tw_cl_get_mem_requirements(&sc->ctlr_handle, sc->flags,
			sc->device_id, TW_OSLI_MAX_NUM_IOS,  TW_OSLI_MAX_NUM_AENS,
			&(sc->alignment), &(sc->sg_size_factor),
			&non_dma_mem_size, &dma_mem_size
			))) {
		tw_osli_printf(sc, "error = %d",
			TW_CL_SEVERITY_ERROR_STRING,
			TW_CL_MESSAGE_SOURCE_FREEBSD_DRIVER,
			0x2008,
			"Can't get Common Layer's memory requirements",
			error);
		return(error);
	}

	if ((sc->non_dma_mem = malloc(non_dma_mem_size, TW_OSLI_MALLOC_CLASS,
				M_WAITOK)) == NULL) {
		tw_osli_printf(sc, "error = %d",
			TW_CL_SEVERITY_ERROR_STRING,
			TW_CL_MESSAGE_SOURCE_FREEBSD_DRIVER,
			0x2009,
			"Can't allocate non-dma memory",
			ENOMEM);
		return(ENOMEM);
	}

	/* Create the parent dma tag. */
	if (bus_dma_tag_create(NULL,			/* parent */
				sc->alignment,		/* alignment */
				0,			/* boundary */
				BUS_SPACE_MAXADDR_32BIT,/* lowaddr */
				BUS_SPACE_MAXADDR, 	/* highaddr */
				NULL, NULL, 		/* filter, filterarg */
				TW_CL_MAX_IO_SIZE,	/* maxsize */
				max_sg_elements,	/* nsegments */
				TW_CL_MAX_IO_SIZE,	/* maxsegsize */
				0,			/* flags */
				NULL,			/* lockfunc */
				NULL,			/* lockfuncarg */
				&sc->parent_tag		/* tag */)) {
		tw_osli_printf(sc, "error = %d",
			TW_CL_SEVERITY_ERROR_STRING,
			TW_CL_MESSAGE_SOURCE_FREEBSD_DRIVER,
			0x200A,
			"Can't allocate parent DMA tag",
			ENOMEM);
		return(ENOMEM);
	}

	/* Create a dma tag for Common Layer's DMA'able memory (dma_mem). */
	if (bus_dma_tag_create(sc->parent_tag,		/* parent */
				sc->alignment,		/* alignment */
				0,			/* boundary */
				BUS_SPACE_MAXADDR_32BIT,/* lowaddr */
				BUS_SPACE_MAXADDR, 	/* highaddr */
				NULL, NULL, 		/* filter, filterarg */
				dma_mem_size,		/* maxsize */
				1,			/* nsegments */
				BUS_SPACE_MAXSIZE,	/* maxsegsize */
				0,			/* flags */
				NULL,			/* lockfunc */
				NULL,			/* lockfuncarg */
				&sc->cmd_tag		/* tag */)) {
		tw_osli_printf(sc, "error = %d",
			TW_CL_SEVERITY_ERROR_STRING,
			TW_CL_MESSAGE_SOURCE_FREEBSD_DRIVER,
			0x200B,
			"Can't allocate DMA tag for Common Layer's "
			"DMA'able memory",
			ENOMEM);
		return(ENOMEM);
	}

	if (bus_dmamem_alloc(sc->cmd_tag, &sc->dma_mem,
		BUS_DMA_NOWAIT, &sc->cmd_map)) {
		/* Try a second time. */
		if (bus_dmamem_alloc(sc->cmd_tag, &sc->dma_mem,
			BUS_DMA_NOWAIT, &sc->cmd_map)) {
			tw_osli_printf(sc, "error = %d",
				TW_CL_SEVERITY_ERROR_STRING,
				TW_CL_MESSAGE_SOURCE_FREEBSD_DRIVER,
				0x200C,
				"Can't allocate DMA'able memory for the"
				"Common Layer",
				ENOMEM);
			return(ENOMEM);
		}
	}

	bus_dmamap_load(sc->cmd_tag, sc->cmd_map, sc->dma_mem,
		dma_mem_size, twa_map_load_callback,
		&sc->dma_mem_phys, 0);

	/*
	 * Create a dma tag for data buffers; size will be the maximum
	 * possible I/O size (128kB).
	 */
	if (bus_dma_tag_create(sc->parent_tag,		/* parent */
				sc->alignment,		/* alignment */
				0,			/* boundary */
				BUS_SPACE_MAXADDR_32BIT,/* lowaddr */
				BUS_SPACE_MAXADDR, 	/* highaddr */
				NULL, NULL, 		/* filter, filterarg */
				TW_CL_MAX_IO_SIZE,	/* maxsize */
				max_sg_elements,	/* nsegments */
				TW_CL_MAX_IO_SIZE,	/* maxsegsize */
				BUS_DMA_ALLOCNOW,	/* flags */
				twa_busdma_lock,	/* lockfunc */
				sc->io_lock,		/* lockfuncarg */
				&sc->dma_tag		/* tag */)) {
		tw_osli_printf(sc, "error = %d",
			TW_CL_SEVERITY_ERROR_STRING,
			TW_CL_MESSAGE_SOURCE_FREEBSD_DRIVER,
			0x200F,
			"Can't allocate DMA tag for data buffers",
			ENOMEM);
		return(ENOMEM);
	}

	/*
	 * Create a dma tag for ioctl data buffers; size will be the maximum
	 * possible I/O size (128kB).
	 */
	if (bus_dma_tag_create(sc->parent_tag,		/* parent */
				sc->alignment,		/* alignment */
				0,			/* boundary */
				BUS_SPACE_MAXADDR_32BIT,/* lowaddr */
				BUS_SPACE_MAXADDR, 	/* highaddr */
				NULL, NULL, 		/* filter, filterarg */
				TW_CL_MAX_IO_SIZE,	/* maxsize */
				max_sg_elements,	/* nsegments */
				TW_CL_MAX_IO_SIZE,	/* maxsegsize */
				BUS_DMA_ALLOCNOW,	/* flags */
				twa_busdma_lock,	/* lockfunc */
				sc->io_lock,		/* lockfuncarg */
				&sc->ioctl_tag		/* tag */)) {
		tw_osli_printf(sc, "error = %d",
			TW_CL_SEVERITY_ERROR_STRING,
			TW_CL_MESSAGE_SOURCE_FREEBSD_DRIVER,
			0x2010,
			"Can't allocate DMA tag for ioctl data buffers",
			ENOMEM);
		return(ENOMEM);
	}

	/* Create just one map for all ioctl request data buffers. */
	if (bus_dmamap_create(sc->ioctl_tag, 0, &sc->ioctl_map)) {
		tw_osli_printf(sc, "error = %d",
			TW_CL_SEVERITY_ERROR_STRING,
			TW_CL_MESSAGE_SOURCE_FREEBSD_DRIVER,
			0x2011,
			"Can't create ioctl map",
			ENOMEM);
		return(ENOMEM);
	}


	/* Initialize request queues. */
	tw_osli_req_q_init(sc, TW_OSLI_FREE_Q);
	tw_osli_req_q_init(sc, TW_OSLI_BUSY_Q);

	if ((sc->req_ctxt_buf = (struct tw_osli_req_context *)
			malloc((sizeof(struct tw_osli_req_context) *
				TW_OSLI_MAX_NUM_IOS),
				TW_OSLI_MALLOC_CLASS, M_WAITOK)) == NULL) {
		tw_osli_printf(sc, "error = %d",
			TW_CL_SEVERITY_ERROR_STRING,
			TW_CL_MESSAGE_SOURCE_FREEBSD_DRIVER,
			0x2012,
			"Failed to allocate request packets",
			ENOMEM);
		return(ENOMEM);
	}
	bzero(sc->req_ctxt_buf,
		sizeof(struct tw_osli_req_context) * TW_OSLI_MAX_NUM_IOS);

	for (i = 0; i < TW_OSLI_MAX_NUM_IOS; i++) {
		req = &(sc->req_ctxt_buf[i]);
		req->ctlr = sc;
		if (bus_dmamap_create(sc->dma_tag, 0, &req->dma_map)) {
			tw_osli_printf(sc, "request # = %d, error = %d",
				TW_CL_SEVERITY_ERROR_STRING,
				TW_CL_MESSAGE_SOURCE_FREEBSD_DRIVER,
				0x2013,
				"Can't create dma map",
				i, ENOMEM);
			return(ENOMEM);
		}

		/* Insert request into the free queue. */
		tw_osli_req_q_insert_tail(req, TW_OSLI_FREE_Q);
	}

	return(0);
}



/*
 * Function name:	tw_osli_free_resources
 * Description:		Performs clean-up at the time of going down.
 *
 * Input:		sc	-- ptr to OSL internal ctlr context
 * Output:		None
 * Return value:	None
 */
static TW_VOID
tw_osli_free_resources(struct twa_softc *sc)
{
	struct tw_osli_req_context	*req;
	TW_INT32			error = 0;

	tw_osli_dbg_dprintf(3, sc, "entered");

	/* Detach from CAM */
	tw_osli_cam_detach(sc);

	if (sc->req_ctxt_buf)
		while ((req = tw_osli_req_q_remove_head(sc, TW_OSLI_FREE_Q)) !=
			NULL)
			if ((error = bus_dmamap_destroy(sc->dma_tag,
					req->dma_map)))
				tw_osli_dbg_dprintf(1, sc,
					"dmamap_destroy(dma) returned %d",
					error);

	if ((sc->ioctl_tag) && (sc->ioctl_map))
		if ((error = bus_dmamap_destroy(sc->ioctl_tag, sc->ioctl_map)))
			tw_osli_dbg_dprintf(1, sc,
				"dmamap_destroy(ioctl) returned %d", error);

	/* Free all memory allocated so far. */
	if (sc->req_ctxt_buf)
		free(sc->req_ctxt_buf, TW_OSLI_MALLOC_CLASS);

	if (sc->non_dma_mem)
		free(sc->non_dma_mem, TW_OSLI_MALLOC_CLASS);

	if (sc->dma_mem) {
		bus_dmamap_unload(sc->cmd_tag, sc->cmd_map);
		bus_dmamem_free(sc->cmd_tag, sc->dma_mem,
			sc->cmd_map);
	}
	if (sc->cmd_tag)
		if ((error = bus_dma_tag_destroy(sc->cmd_tag)))
			tw_osli_dbg_dprintf(1, sc,
				"dma_tag_destroy(cmd) returned %d", error);

	if (sc->dma_tag)
		if ((error = bus_dma_tag_destroy(sc->dma_tag)))
			tw_osli_dbg_dprintf(1, sc,
				"dma_tag_destroy(dma) returned %d", error);

	if (sc->ioctl_tag)
		if ((error = bus_dma_tag_destroy(sc->ioctl_tag)))
			tw_osli_dbg_dprintf(1, sc,
				"dma_tag_destroy(ioctl) returned %d", error);

	if (sc->parent_tag)
		if ((error = bus_dma_tag_destroy(sc->parent_tag)))
			tw_osli_dbg_dprintf(1, sc,
				"dma_tag_destroy(parent) returned %d", error);


	/* Disconnect the interrupt handler. */
	if (sc->intr_handle)
		if ((error = bus_teardown_intr(sc->bus_dev,
				sc->irq_res, sc->intr_handle)))
			tw_osli_dbg_dprintf(1, sc,
				"teardown_intr returned %d", error);

	if (sc->irq_res != NULL)
		if ((error = bus_release_resource(sc->bus_dev,
				SYS_RES_IRQ, sc->irq_res_id, sc->irq_res)))
			tw_osli_dbg_dprintf(1, sc,
				"release_resource(irq) returned %d", error);


	/* Release the register window mapping. */
	if (sc->reg_res != NULL)
		if ((error = bus_release_resource(sc->bus_dev,
				SYS_RES_MEMORY, sc->reg_res_id, sc->reg_res)))
			tw_osli_dbg_dprintf(1, sc,
				"release_resource(io) returned %d", error);


	/* Destroy the control device. */
	if (sc->ctrl_dev != (struct cdev *)NULL)
		destroy_dev(sc->ctrl_dev);

	if ((error = sysctl_ctx_free(&sc->sysctl_ctxt)))
		tw_osli_dbg_dprintf(1, sc,
			"sysctl_ctx_free returned %d", error);

}



/*
 * Function name:	twa_detach
 * Description:		Called when the controller is being detached from
 *			the pci bus.
 *
 * Input:		dev	-- bus device corresponding to the ctlr
 * Output:		None
 * Return value:	0	-- success
 *			non-zero-- failure
 */
static TW_INT32
twa_detach(device_t dev)
{
	struct twa_softc	*sc = device_get_softc(dev);
	TW_INT32		error;

	tw_osli_dbg_dprintf(3, sc, "entered");

	error = EBUSY;
	if (sc->state & TW_OSLI_CTLR_STATE_OPEN) {
		tw_osli_printf(sc, "error = %d",
			TW_CL_SEVERITY_ERROR_STRING,
			TW_CL_MESSAGE_SOURCE_FREEBSD_DRIVER,
			0x2014,
			"Device open",
			error);
		goto out;
	}

	/* Shut the controller down. */
	if ((error = twa_shutdown(dev)))
		goto out;

	/* Free all resources associated with this controller. */
	tw_osli_free_resources(sc);
	error = 0;

out:
	return(error);
}



/*
 * Function name:	twa_shutdown
 * Description:		Called at unload/shutdown time.  Lets the controller
 *			know that we are going down.
 *
 * Input:		dev	-- bus device corresponding to the ctlr
 * Output:		None
 * Return value:	0	-- success
 *			non-zero-- failure
 */
static TW_INT32
twa_shutdown(device_t dev)
{
	struct twa_softc	*sc = device_get_softc(dev);
	TW_INT32		error = 0;

	tw_osli_dbg_dprintf(3, sc, "entered");

	/* Disconnect from the controller. */
	if ((error = tw_cl_shutdown_ctlr(&(sc->ctlr_handle), 0))) {
		tw_osli_printf(sc, "error = %d",
			TW_CL_SEVERITY_ERROR_STRING,
			TW_CL_MESSAGE_SOURCE_FREEBSD_DRIVER,
			0x2015,
			"Failed to shutdown Common Layer/controller",
			error);
	}
	return(error);
}



/*
 * Function name:	twa_busdma_lock
 * Description:		Function to provide synchronization during busdma_swi.
 *
 * Input:		lock_arg -- lock mutex sent as argument
 *			op -- operation (lock/unlock) expected of the function
 * Output:		None
 * Return value:	None
 */
TW_VOID
twa_busdma_lock(TW_VOID *lock_arg, bus_dma_lock_op_t op)
{
	struct mtx	*lock;

	lock = (struct mtx *)lock_arg;
	switch (op) {
	case BUS_DMA_LOCK:
		mtx_lock_spin(lock);
		break;

	case BUS_DMA_UNLOCK:
		mtx_unlock_spin(lock);
		break;

	default:
		panic("Unknown operation 0x%x for twa_busdma_lock!", op);
	}
}


#ifdef TW_OSLI_DEFERRED_INTR_USED
/*
 * Function name:	twa_pci_intr_fast
 * Description:		Interrupt handler.  Wrapper for twa_interrupt.
 *
 * Input:		arg	-- ptr to OSL internal ctlr context
 * Output:		FILTER_HANDLED or FILTER_STRAY
 * Return value:	None
 */
static int
twa_pci_intr_fast(TW_VOID *arg)
{
	struct twa_softc	*sc = (struct twa_softc *)arg;

	tw_osli_dbg_dprintf(10, sc, "entered");
	if (tw_cl_interrupt(&(sc->ctlr_handle))) {
		taskqueue_enqueue_fast(taskqueue_fast,
			&(sc->deferred_intr_callback));
		return(FILTER_HANDLED);
	}
	return(FILTER_STRAY);
}
#else
/*
 * Function name:	twa_pci_intr
 * Description:		Interrupt handler.  Wrapper for twa_interrupt.
 *
 * Input:		arg	-- ptr to OSL internal ctlr context
 * Output:		None
 * Return value:	None
 */
static TW_VOID
twa_pci_intr(TW_VOID *arg)
{
	struct twa_softc	*sc = (struct twa_softc *)arg;

	tw_osli_dbg_dprintf(10, sc, "entered");
	if (tw_cl_interrupt(&(sc->ctlr_handle)))
		tw_cl_deferred_interrupt(&(sc->ctlr_handle));
}
#endif

#ifdef TW_OSLI_DEFERRED_INTR_USED

/*
 * Function name:	twa_deferred_intr
 * Description:		Deferred interrupt handler.
 *
 * Input:		context	-- ptr to OSL internal ctlr context
 *			pending	-- not used
 * Output:		None
 * Return value:	None
 */
static TW_VOID
twa_deferred_intr(TW_VOID *context, TW_INT32 pending)
{
	struct twa_softc	*sc = (struct twa_softc *)context;

	tw_osli_dbg_dprintf(10, sc, "entered");

	tw_cl_deferred_interrupt(&(sc->ctlr_handle));
}

#endif /* TW_OSLI_DEFERRED_INTR_USED */



/*
 * Function name:	tw_osli_fw_passthru
 * Description:		Builds a fw passthru cmd pkt, and submits it to CL.
 *
 * Input:		sc	-- ptr to OSL internal ctlr context
 *			buf	-- ptr to ioctl pkt understood by CL
 * Output:		None
 * Return value:	0	-- success
 *			non-zero-- failure
 */
TW_INT32
tw_osli_fw_passthru(struct twa_softc *sc, TW_INT8 *buf)
{
	struct tw_osli_req_context		*req;
	struct tw_osli_ioctl_no_data_buf	*user_buf =
		(struct tw_osli_ioctl_no_data_buf *)buf;
	TW_TIME					end_time;
	TW_UINT32				timeout = 60;
	TW_UINT32				data_buf_size_adjusted;
	struct tw_cl_req_packet			*req_pkt;
	struct tw_cl_passthru_req_packet	*pt_req;
	TW_INT32				error;

	tw_osli_dbg_dprintf(5, sc, "ioctl: passthru");
		
	if ((req = tw_osli_get_request(sc)) == NULL)
		return(EBUSY);

	req->req_handle.osl_req_ctxt = req;
	req->orig_req = buf;
	req->flags |= TW_OSLI_REQ_FLAGS_PASSTHRU;

	req_pkt = &(req->req_pkt);
	req_pkt->status = 0;
	req_pkt->tw_osl_callback = tw_osl_complete_passthru;
	/* Let the Common Layer retry the request on cmd queue full. */
	req_pkt->flags |= TW_CL_REQ_RETRY_ON_BUSY;

	pt_req = &(req_pkt->gen_req_pkt.pt_req);
	/*
	 * Make sure that the data buffer sent to firmware is a 
	 * 512 byte multiple in size.
	 */
	data_buf_size_adjusted =
		(user_buf->driver_pkt.buffer_length +
		(sc->sg_size_factor - 1)) & ~(sc->sg_size_factor - 1);
	if ((req->length = data_buf_size_adjusted)) {
		if ((req->data = malloc(data_buf_size_adjusted,
			TW_OSLI_MALLOC_CLASS, M_WAITOK)) == NULL) {
			error = ENOMEM;
			tw_osli_printf(sc, "error = %d",
				TW_CL_SEVERITY_ERROR_STRING,
				TW_CL_MESSAGE_SOURCE_FREEBSD_DRIVER,
				0x2016,
				"Could not alloc mem for "
				"fw_passthru data_buf",
				error);
			goto fw_passthru_err;
		}
		/* Copy the payload. */
		if ((error = copyin((TW_VOID *)(user_buf->pdata), 
			req->data,
			user_buf->driver_pkt.buffer_length)) != 0) {
			tw_osli_printf(sc, "error = %d",
				TW_CL_SEVERITY_ERROR_STRING,
				TW_CL_MESSAGE_SOURCE_FREEBSD_DRIVER,
				0x2017,
				"Could not copyin fw_passthru data_buf",
				error);
			goto fw_passthru_err;
		}
		pt_req->sgl_entries = 1; /* will be updated during mapping */
		req->flags |= (TW_OSLI_REQ_FLAGS_DATA_IN |
			TW_OSLI_REQ_FLAGS_DATA_OUT);
	} else
		pt_req->sgl_entries = 0; /* no payload */

	pt_req->cmd_pkt = (TW_VOID *)(&(user_buf->cmd_pkt));
	pt_req->cmd_pkt_length = sizeof(struct tw_cl_command_packet);

	if ((error = tw_osli_map_request(req)))
		goto fw_passthru_err;

	end_time = tw_osl_get_local_time() + timeout;
	while (req->state != TW_OSLI_REQ_STATE_COMPLETE) {
		req->flags |= TW_OSLI_REQ_FLAGS_SLEEPING;
		
		error = tsleep(req, PRIBIO, "twa_passthru", timeout * hz);

		if (!(req->flags & TW_OSLI_REQ_FLAGS_SLEEPING))
			error = 0;
		req->flags &= ~TW_OSLI_REQ_FLAGS_SLEEPING;

		if (! error) {
			if (((error = req->error_code)) ||
				((error = (req->state !=
				TW_OSLI_REQ_STATE_COMPLETE))) ||
				((error = req_pkt->status)))
				goto fw_passthru_err;
			break;
		}

		if (req_pkt->status) {
			error = req_pkt->status;
			goto fw_passthru_err;
		}

		if (error == EWOULDBLOCK) {
			/* Time out! */
			tw_osli_printf(sc, "request = %p",
				TW_CL_SEVERITY_ERROR_STRING,
				TW_CL_MESSAGE_SOURCE_FREEBSD_DRIVER,
				0x2018,
				"Passthru request timed out!",
				req);
			/*
			 * Should I check here if the timeout happened
			 * because of yet another reset, and not do a
			 * second reset?
			 */
			tw_cl_reset_ctlr(&sc->ctlr_handle);
			/*
			 * Don't touch req after a reset.  It (and any
			 * associated data) will already have been
			 * unmapped by the callback.
			 */
			user_buf->driver_pkt.os_status = error;
			error = ETIMEDOUT;
			goto fw_passthru_err;
		}
		/* 
		 * Either the request got completed, or we were woken up by a
		 * signal.  Calculate the new timeout, in case it was the latter.
		 */
		timeout = (end_time - tw_osl_get_local_time());
	}

	/* If there was a payload, copy it back. */
	if ((!error) && (req->length))
		if ((error = copyout(req->data, user_buf->pdata,
			user_buf->driver_pkt.buffer_length)))
			tw_osli_printf(sc, "error = %d",
				TW_CL_SEVERITY_ERROR_STRING,
				TW_CL_MESSAGE_SOURCE_FREEBSD_DRIVER,
				0x2019,
				"Could not copyout fw_passthru data_buf",
				error);
	
fw_passthru_err:
	/*
	 * Print the failure message.  For some reason, on certain OS versions,
	 * printing this error message during reset hangs the display (although
	 * the rest of the system is running fine.  So, don't print it if the
	 * failure was due to a reset.
	 */
	if ((error) && (error != TW_CL_ERR_REQ_BUS_RESET))
		tw_osli_printf(sc, "error = %d",		
			TW_CL_SEVERITY_ERROR_STRING,
			TW_CL_MESSAGE_SOURCE_FREEBSD_DRIVER,
			0x201A,
			"Firmware passthru failed!",
			error);

	user_buf->driver_pkt.os_status = error;
	/* Free resources. */
	if (req->data)
		free(req->data, TW_OSLI_MALLOC_CLASS);
	tw_osli_req_q_insert_tail(req, TW_OSLI_FREE_Q);
	return(error);
}



/*
 * Function name:	tw_osl_complete_passthru
 * Description:		Called to complete passthru requests.
 *
 * Input:		req_handle	-- ptr to request handle
 * Output:		None
 * Return value:	None
 */
TW_VOID
tw_osl_complete_passthru(struct tw_cl_req_handle *req_handle)
{
	struct tw_osli_req_context	*req = req_handle->osl_req_ctxt;
	struct twa_softc		*sc = req->ctlr;

	tw_osli_dbg_dprintf(5, sc, "entered");

	if (req->state != TW_OSLI_REQ_STATE_BUSY) {
		tw_osli_printf(sc, "request = %p, status = %d",
			TW_CL_SEVERITY_ERROR_STRING,
			TW_CL_MESSAGE_SOURCE_FREEBSD_DRIVER,
			0x201B,
			"Unposted command completed!!",
			req, req->state);
	}

	/*
	 * Remove request from the busy queue.  Just mark it complete.
	 * There's no need to move it into the complete queue as we are
	 * going to be done with it right now.
	 */
	req->state = TW_OSLI_REQ_STATE_COMPLETE;
	tw_osli_req_q_remove_item(req, TW_OSLI_BUSY_Q);

	tw_osli_unmap_request(req);

	/*
	 * Don't do a wake up if there was an error even before the request
	 * was sent down to the Common Layer, and we hadn't gotten an
	 * EINPROGRESS.  The request originator will then be returned an
	 * error, and he can do the clean-up.
	 */
	if ((req->error_code) &&
		(!(req->state & TW_OSLI_REQ_FLAGS_IN_PROGRESS)))
		return;

	if (req->flags & TW_OSLI_REQ_FLAGS_PASSTHRU) {
		if (req->flags & TW_OSLI_REQ_FLAGS_SLEEPING) {
			/* Wake up the sleeping command originator. */
			tw_osli_dbg_dprintf(5, sc,
				"Waking up originator of request %p", req);
			req->flags &= ~TW_OSLI_REQ_FLAGS_SLEEPING;
			wakeup_one(req);
		} else {
			/*
			 * If the request completed even before tsleep
			 * was called, simply return.
			 */
			if (req->flags & TW_OSLI_REQ_FLAGS_MAPPED)
				return;

			tw_osli_printf(sc, "request = %p",
				TW_CL_SEVERITY_ERROR_STRING,
				TW_CL_MESSAGE_SOURCE_FREEBSD_DRIVER,
				0x201C,
				"Passthru callback called, "
				"and caller not sleeping",
				req);
		}
	} else {
		tw_osli_printf(sc, "request = %p",
			TW_CL_SEVERITY_ERROR_STRING,
			TW_CL_MESSAGE_SOURCE_FREEBSD_DRIVER,
			0x201D,
			"Passthru callback called for non-passthru request",
			req);
	}
}



/*
 * Function name:	tw_osli_get_request
 * Description:		Gets a request pkt from the free queue.
 *
 * Input:		sc	-- ptr to OSL internal ctlr context
 * Output:		None
 * Return value:	ptr to request pkt	-- success
 *			NULL			-- failure
 */
struct tw_osli_req_context *
tw_osli_get_request(struct twa_softc *sc)
{
	struct tw_osli_req_context	*req;

	tw_osli_dbg_dprintf(4, sc, "entered");

	/* Get a free request packet. */
	req = tw_osli_req_q_remove_head(sc, TW_OSLI_FREE_Q);

	/* Initialize some fields to their defaults. */
	if (req) {
		req->req_handle.osl_req_ctxt = NULL;
		req->req_handle.cl_req_ctxt = NULL;
		req->data = NULL;
		req->length = 0;
		req->real_data = NULL;
		req->real_length = 0;
		req->state = TW_OSLI_REQ_STATE_INIT;/* req being initialized */
		req->flags = 0;
		req->error_code = 0;
		req->orig_req = NULL;

		bzero(&(req->req_pkt), sizeof(struct tw_cl_req_packet));

	}
	return(req);
}



/*
 * Function name:	twa_map_load_data_callback
 * Description:		Callback of bus_dmamap_load for the buffer associated
 *			with data.  Updates the cmd pkt (size/sgl_entries
 *			fields, as applicable) to reflect the number of sg
 *			elements.
 *
 * Input:		arg	-- ptr to OSL internal request context
 *			segs	-- ptr to a list of segment descriptors
 *			nsegments--# of segments
 *			error	-- 0 if no errors encountered before callback,
 *				   non-zero if errors were encountered
 * Output:		None
 * Return value:	None
 */
static TW_VOID
twa_map_load_data_callback(TW_VOID *arg, bus_dma_segment_t *segs,
	TW_INT32 nsegments, TW_INT32 error)
{
	struct tw_osli_req_context	*req =
		(struct tw_osli_req_context *)arg;
	struct twa_softc		*sc = req->ctlr;
	struct tw_cl_req_packet		*req_pkt = &(req->req_pkt);

	tw_osli_dbg_dprintf(10, sc, "entered");

	/* Mark the request as currently being processed. */
	req->state = TW_OSLI_REQ_STATE_BUSY;
	/* Move the request into the busy queue. */
	tw_osli_req_q_insert_tail(req, TW_OSLI_BUSY_Q);

	req->flags |= TW_OSLI_REQ_FLAGS_MAPPED;
	if (req->flags & TW_OSLI_REQ_FLAGS_IN_PROGRESS)
		tw_osli_allow_new_requests(sc, (TW_VOID *)(req->orig_req));

	if (error == EFBIG) {
		req->error_code = error;
		goto out;
	}

	if (req->flags & TW_OSLI_REQ_FLAGS_PASSTHRU) {
		struct tw_cl_passthru_req_packet	*pt_req;

		if (req->flags & TW_OSLI_REQ_FLAGS_DATA_IN)
			bus_dmamap_sync(sc->ioctl_tag, sc->ioctl_map,
				BUS_DMASYNC_PREREAD);

		if (req->flags & TW_OSLI_REQ_FLAGS_DATA_OUT) {
			/* 
			 * If we're using an alignment buffer, and we're
			 * writing data, copy the real data out.
			 */
			if (req->flags & TW_OSLI_REQ_FLAGS_DATA_COPY_NEEDED)
				bcopy(req->real_data, req->data, req->real_length);
			bus_dmamap_sync(sc->ioctl_tag, sc->ioctl_map,
				BUS_DMASYNC_PREWRITE);
		}

		pt_req = &(req_pkt->gen_req_pkt.pt_req);
		pt_req->sg_list = (TW_UINT8 *)segs;
		pt_req->sgl_entries += (nsegments - 1);
		error = tw_cl_fw_passthru(&(sc->ctlr_handle), req_pkt,
			&(req->req_handle));
	} else {
		struct tw_cl_scsi_req_packet	*scsi_req;

		if (req->flags & TW_OSLI_REQ_FLAGS_DATA_IN)
			bus_dmamap_sync(sc->dma_tag, req->dma_map,
				BUS_DMASYNC_PREREAD);

		if (req->flags & TW_OSLI_REQ_FLAGS_DATA_OUT) {
			/* 
			 * If we're using an alignment buffer, and we're
			 * writing data, copy the real data out.
			 */
			if (req->flags & TW_OSLI_REQ_FLAGS_DATA_COPY_NEEDED)
				bcopy(req->real_data, req->data, req->real_length);
			bus_dmamap_sync(sc->dma_tag, req->dma_map,
				BUS_DMASYNC_PREWRITE);
		}

		scsi_req = &(req_pkt->gen_req_pkt.scsi_req);
		scsi_req->sg_list = (TW_UINT8 *)segs;
		scsi_req->sgl_entries += (nsegments - 1);
		error = tw_cl_start_io(&(sc->ctlr_handle), req_pkt,
			&(req->req_handle));
	}

out:
	if (error) {
		req->error_code = error;
		req_pkt->tw_osl_callback(&(req->req_handle));
		/*
		 * If the caller had been returned EINPROGRESS, and he has
		 * registered a callback for handling completion, the callback
		 * will never get called because we were unable to submit the
		 * request.  So, free up the request right here.
		 */
		if (req->flags & TW_OSLI_REQ_FLAGS_IN_PROGRESS)
			tw_osli_req_q_insert_tail(req, TW_OSLI_FREE_Q);
	}
}



/*
 * Function name:	twa_map_load_callback
 * Description:		Callback of bus_dmamap_load for the buffer associated
 *			with a cmd pkt.
 *
 * Input:		arg	-- ptr to variable to hold phys addr
 *			segs	-- ptr to a list of segment descriptors
 *			nsegments--# of segments
 *			error	-- 0 if no errors encountered before callback,
 *				   non-zero if errors were encountered
 * Output:		None
 * Return value:	None
 */
static TW_VOID
twa_map_load_callback(TW_VOID *arg, bus_dma_segment_t *segs,
	TW_INT32 nsegments, TW_INT32 error)
{
	*((bus_addr_t *)arg) = segs[0].ds_addr;
}



/*
 * Function name:	tw_osli_map_request
 * Description:		Maps a cmd pkt and data associated with it, into
 *			DMA'able memory.
 *
 * Input:		req	-- ptr to request pkt
 * Output:		None
 * Return value:	0	-- success
 *			non-zero-- failure
 */
TW_INT32
tw_osli_map_request(struct tw_osli_req_context *req)
{
	struct twa_softc	*sc = req->ctlr;
	TW_INT32		error = 0;

	tw_osli_dbg_dprintf(10, sc, "entered");

	/* If the command involves data, map that too. */
	if (req->data != NULL) {
		/*
		 * It's sufficient for the data pointer to be 4-byte aligned
		 * to work with 9000.  However, if 4-byte aligned addresses
		 * are passed to bus_dmamap_load, we can get back sg elements
		 * that are not 512-byte multiples in size.  So, we will let
		 * only those buffers that are 512-byte aligned to pass
		 * through, and bounce the rest, so as to make sure that we
		 * always get back sg elements that are 512-byte multiples
		 * in size.
		 */
		if (((vm_offset_t)req->data % sc->sg_size_factor) ||
			(req->length % sc->sg_size_factor)) {
			req->flags |= TW_OSLI_REQ_FLAGS_DATA_COPY_NEEDED;
			/* Save original data pointer and length. */
			req->real_data = req->data;
			req->real_length = req->length;
			req->length = (req->length +
				(sc->sg_size_factor - 1)) &
				~(sc->sg_size_factor - 1);
			req->data = malloc(req->length, TW_OSLI_MALLOC_CLASS,
					M_NOWAIT);
			if (req->data == NULL) {
				tw_osli_printf(sc, "error = %d",
					TW_CL_SEVERITY_ERROR_STRING,
					TW_CL_MESSAGE_SOURCE_FREEBSD_DRIVER,
					0x201E,
					"Failed to allocate memory "
					"for bounce buffer",
					ENOMEM);
				/* Restore original data pointer and length. */
				req->data = req->real_data;
				req->length = req->real_length;
				return(ENOMEM);
			}
		}
	
		/*
		 * Map the data buffer into bus space and build the SG list.
		 */
		if (req->flags & TW_OSLI_REQ_FLAGS_PASSTHRU) {
			/* Lock against multiple simultaneous ioctl calls. */
			mtx_lock_spin(sc->io_lock);
			error = bus_dmamap_load(sc->ioctl_tag, sc->ioctl_map,
				req->data, req->length,
				twa_map_load_data_callback, req,
				BUS_DMA_WAITOK);
			mtx_unlock_spin(sc->io_lock);
		} else {
			/*
			 * There's only one CAM I/O thread running at a time.
			 * So, there's no need to hold the io_lock.
			 */
			error = bus_dmamap_load(sc->dma_tag, req->dma_map,
				req->data, req->length,
				twa_map_load_data_callback, req,
				BUS_DMA_WAITOK);
		}
		
		if (!error)
			error = req->error_code;
		else {
			if (error == EINPROGRESS) {
				/*
				 * Specifying sc->io_lock as the lockfuncarg
				 * in ...tag_create should protect the access
				 * of ...FLAGS_MAPPED from the callback.
				 */
				mtx_lock_spin(sc->io_lock);
				if (!(req->flags & TW_OSLI_REQ_FLAGS_MAPPED)) {
					req->flags |=
						TW_OSLI_REQ_FLAGS_IN_PROGRESS;
					tw_osli_disallow_new_requests(sc);
				}
				mtx_unlock_spin(sc->io_lock);
				error = 0;
			} else {
				/* Free alignment buffer if it was used. */
				if (req->flags &
					TW_OSLI_REQ_FLAGS_DATA_COPY_NEEDED) {
					free(req->data, TW_OSLI_MALLOC_CLASS);
					/*
					 * Restore original data pointer
					 * and length.
					 */
					req->data = req->real_data;
					req->length = req->real_length;
				}
			}
		}

	} else {
		/* Mark the request as currently being processed. */
		req->state = TW_OSLI_REQ_STATE_BUSY;
		/* Move the request into the busy queue. */
		tw_osli_req_q_insert_tail(req, TW_OSLI_BUSY_Q);
		if (req->flags & TW_OSLI_REQ_FLAGS_PASSTHRU)
			error = tw_cl_fw_passthru(&sc->ctlr_handle,
					&(req->req_pkt), &(req->req_handle));
		else
			error = tw_cl_start_io(&sc->ctlr_handle,
					&(req->req_pkt), &(req->req_handle));
		if (error) {
			req->error_code = error;
			req->req_pkt.tw_osl_callback(&(req->req_handle));
		}
	}
	return(error);
}



/*
 * Function name:	tw_osli_unmap_request
 * Description:		Undoes the mapping done by tw_osli_map_request.
 *
 * Input:		req	-- ptr to request pkt
 * Output:		None
 * Return value:	None
 */
TW_VOID
tw_osli_unmap_request(struct tw_osli_req_context *req)
{
	struct twa_softc	*sc = req->ctlr;

	tw_osli_dbg_dprintf(10, sc, "entered");

	/* If the command involved data, unmap that too. */
	if (req->data != NULL) {
		if (req->flags & TW_OSLI_REQ_FLAGS_PASSTHRU) {
			/* Lock against multiple simultaneous ioctl calls. */
			mtx_lock_spin(sc->io_lock);

			if (req->flags & TW_OSLI_REQ_FLAGS_DATA_IN) {
				bus_dmamap_sync(sc->ioctl_tag,
					sc->ioctl_map, BUS_DMASYNC_POSTREAD);

				/* 
				 * If we are using a bounce buffer, and we are
				 * reading data, copy the real data in.
				 */
				if (req->flags & TW_OSLI_REQ_FLAGS_DATA_COPY_NEEDED)
					bcopy(req->data, req->real_data,
						req->real_length);
			}

			if (req->flags & TW_OSLI_REQ_FLAGS_DATA_OUT)
				bus_dmamap_sync(sc->ioctl_tag, sc->ioctl_map,
					BUS_DMASYNC_POSTWRITE);

			bus_dmamap_unload(sc->ioctl_tag, sc->ioctl_map);

			mtx_unlock_spin(sc->io_lock);
		} else {
			if (req->flags & TW_OSLI_REQ_FLAGS_DATA_IN) {
				bus_dmamap_sync(sc->dma_tag,
					req->dma_map, BUS_DMASYNC_POSTREAD);

				/* 
				 * If we are using a bounce buffer, and we are
				 * reading data, copy the real data in.
				 */
				if (req->flags & TW_OSLI_REQ_FLAGS_DATA_COPY_NEEDED)
					bcopy(req->data, req->real_data,
						req->real_length);
			}
			if (req->flags & TW_OSLI_REQ_FLAGS_DATA_OUT)
				bus_dmamap_sync(sc->dma_tag, req->dma_map,
					BUS_DMASYNC_POSTWRITE);

			bus_dmamap_unload(sc->dma_tag, req->dma_map);
		}
	}

	/* Free alignment buffer if it was used. */
	if (req->flags & TW_OSLI_REQ_FLAGS_DATA_COPY_NEEDED) {
		free(req->data, TW_OSLI_MALLOC_CLASS);
		/* Restore original data pointer and length. */
		req->data = req->real_data;
		req->length = req->real_length;
	}
}



#ifdef TW_OSL_DEBUG

TW_VOID	twa_report_stats(TW_VOID);
TW_VOID	twa_reset_stats(TW_VOID);
TW_VOID	tw_osli_print_ctlr_stats(struct twa_softc *sc);
TW_VOID twa_print_req_info(struct tw_osli_req_context *req);


/*
 * Function name:	twa_report_stats
 * Description:		For being called from ddb.  Calls functions that print
 *			OSL and CL internal stats for the controller.
 *
 * Input:		None
 * Output:		None
 * Return value:	None
 */
TW_VOID
twa_report_stats(TW_VOID)
{
	struct twa_softc	*sc;
	TW_INT32		i;

	for (i = 0; (sc = devclass_get_softc(twa_devclass, i)) != NULL; i++) {
		tw_osli_print_ctlr_stats(sc);
		tw_cl_print_ctlr_stats(&sc->ctlr_handle);
	}
}



/*
 * Function name:	tw_osli_print_ctlr_stats
 * Description:		For being called from ddb.  Prints OSL controller stats
 *
 * Input:		sc	-- ptr to OSL internal controller context
 * Output:		None
 * Return value:	None
 */
TW_VOID
tw_osli_print_ctlr_stats(struct twa_softc *sc)
{
	twa_printf(sc, "osl_ctlr_ctxt = %p\n", sc);
	twa_printf(sc, "OSLq type  current  max\n");
	twa_printf(sc, "free      %04d     %04d\n",
		sc->q_stats[TW_OSLI_FREE_Q].cur_len,
		sc->q_stats[TW_OSLI_FREE_Q].max_len);
	twa_printf(sc, "busy      %04d     %04d\n",
		sc->q_stats[TW_OSLI_BUSY_Q].cur_len,
		sc->q_stats[TW_OSLI_BUSY_Q].max_len);
}	



/*
 * Function name:	twa_print_req_info
 * Description:		For being called from ddb.  Calls functions that print
 *			OSL and CL internal details for the request.
 *
 * Input:		req	-- ptr to OSL internal request context
 * Output:		None
 * Return value:	None
 */
TW_VOID
twa_print_req_info(struct tw_osli_req_context *req)
{
	struct twa_softc	*sc = req->ctlr;

	twa_printf(sc, "OSL details for request:\n");
	twa_printf(sc, "osl_req_ctxt = %p, cl_req_ctxt = %p\n"
		"data = %p, length = 0x%x, real_data = %p, real_length = 0x%x\n"
		"state = 0x%x, flags = 0x%x, error = 0x%x, orig_req = %p\n"
		"next_req = %p, prev_req = %p, dma_map = %p\n",
		req->req_handle.osl_req_ctxt, req->req_handle.cl_req_ctxt,
		req->data, req->length, req->real_data, req->real_length,
		req->state, req->flags, req->error_code, req->orig_req,
		req->link.next, req->link.prev, req->dma_map);
	tw_cl_print_req_info(&(req->req_handle));
}



/*
 * Function name:	twa_reset_stats
 * Description:		For being called from ddb.
 *			Resets some OSL controller stats.
 *
 * Input:		None
 * Output:		None
 * Return value:	None
 */
TW_VOID
twa_reset_stats(TW_VOID)
{
	struct twa_softc	*sc;
	TW_INT32		i;

	for (i = 0; (sc = devclass_get_softc(twa_devclass, i)) != NULL; i++) {
		sc->q_stats[TW_OSLI_FREE_Q].max_len = 0;
		sc->q_stats[TW_OSLI_BUSY_Q].max_len = 0;
		tw_cl_reset_stats(&sc->ctlr_handle);
	}
}

#endif /* TW_OSL_DEBUG */
