/*-
 * Copyright (c) 2003-04 3ware, Inc.
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
 * 3ware driver for 9000 series storage controllers.
 *
 * Author: Vinod Kashyap
 */


#include <dev/twa/twa_includes.h>

static void	twa_setup_data_dmamap(void *arg, bus_dma_segment_t *segs,
						int nsegments, int error);
static void	twa_setup_request_dmamap(void *arg, bus_dma_segment_t *segs,
						int nsegments, int error);

MALLOC_DEFINE(TWA_MALLOC_CLASS, "twa commands", "twa commands");


static	d_open_t		twa_open;
static	d_close_t		twa_close;
static	d_ioctl_t		twa_ioctl_wrapper;

static struct cdevsw twa_cdevsw = {
	twa_open,
	twa_close,
	noread,
	nowrite,
	twa_ioctl_wrapper,
	nopoll,
	nommap,
	nostrategy,
	"twa",
	TWA_CDEV_MAJOR,
	nodump,
	nopsize,
	0
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
static int
twa_open(dev_t dev, int flags, int fmt, d_thread_t *proc)
{
	int			unit = minor(dev);
	struct twa_softc	*sc = devclass_get_softc(twa_devclass, unit);

	sc->twa_state |= TWA_STATE_OPEN;
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
static int
twa_close(dev_t dev, int flags, int fmt, d_thread_t *proc)
{
	int			unit = minor(dev);
	struct twa_softc	*sc = devclass_get_softc(twa_devclass, unit);

	sc->twa_state &= ~TWA_STATE_OPEN;
	return(0);
}



/*
 * Function name:	twa_ioctl_wrapper
 * Description:		Called when an ioctl is posted to the controller.
 *			Simply calls the ioctl handler.
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
static int
twa_ioctl_wrapper(dev_t dev, u_long cmd, caddr_t buf,
					int flags, d_thread_t *proc)
{
	struct twa_softc	*sc = (struct twa_softc *)(dev->si_drv1);

	return(twa_ioctl(sc, cmd, buf));
}



static int	twa_probe (device_t dev);
static int	twa_attach (device_t dev);
static void	twa_free (struct twa_softc *sc);
static int	twa_detach (device_t dev);
static int	twa_shutdown (device_t dev);
static int	twa_suspend (device_t dev);
static int	twa_resume (device_t dev);
static void	twa_pci_intr(void *arg);

static device_method_t	twa_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		twa_probe),
	DEVMETHOD(device_attach,	twa_attach),
	DEVMETHOD(device_detach,	twa_detach),
	DEVMETHOD(device_shutdown,	twa_shutdown),
	DEVMETHOD(device_suspend,	twa_suspend),
	DEVMETHOD(device_resume,	twa_resume),

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



/*
 * Function name:	twa_probe
 * Description:		Called at driver load time.  Claims 9000 ctlrs.
 *
 * Input:		dev	-- bus device corresponding to the ctlr
 * Output:		None
 * Return value:	<= 0	-- success
 *			> 0	-- failure
 */
static int
twa_probe(device_t dev)
{
	static u_int8_t	first_ctlr = 1;

	twa_dbg_print(3, "entered");

	if ((pci_get_vendor(dev) == TWA_VENDOR_ID) &&
			(pci_get_device(dev) == TWA_DEVICE_ID_9K)) {
		device_set_desc(dev, TWA_DEVICE_NAME);
		/* Print the driver version only once. */
		if (first_ctlr) {
			printf("3ware device driver for 9000 series storage controllers, version: %s\n",
					TWA_DRIVER_VERSION_STRING);
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
 *			to initialize ctlr, and to attach to CAM.
 *
 * Input:		dev	-- bus device corresponding to the ctlr
 * Output:		None
 * Return value:	0	-- success
 *			non-zero-- failure
 */
static int
twa_attach(device_t dev)
{
	struct twa_softc	*sc = device_get_softc(dev);
	u_int32_t		command;
	int			res_id;
	int			error;

	twa_dbg_dprint_enter(3, sc);

	/* Initialize the softc structure. */
	sc->twa_bus_dev = dev;

	sysctl_ctx_init(&sc->twa_sysctl_ctx);
	sc->twa_sysctl_tree = SYSCTL_ADD_NODE(&sc->twa_sysctl_ctx,
				SYSCTL_STATIC_CHILDREN(_hw), OID_AUTO,
				device_get_nameunit(dev), CTLFLAG_RD, 0, "");
	if (sc->twa_sysctl_tree == NULL) {
		twa_printf(sc, "Cannot add sysctl tree node.\n");
		return(ENXIO);
	}
	SYSCTL_ADD_STRING(&sc->twa_sysctl_ctx, SYSCTL_CHILDREN(sc->twa_sysctl_tree),
				OID_AUTO, "driver_version", CTLFLAG_RD,
				TWA_DRIVER_VERSION_STRING, 0, "TWA driver version");

	/* Make sure we are going to be able to talk to this board. */
	command = pci_read_config(dev, PCIR_COMMAND, 2);
	if ((command & PCIM_CMD_PORTEN) == 0) {
		twa_printf(sc, "Register window not available.\n");
		return(ENXIO);
	}
	
	/* Force the busmaster enable bit on, in case the BIOS forgot. */
	command |= PCIM_CMD_BUSMASTEREN;
	pci_write_config(dev, PCIR_COMMAND, command, 2);

	/* Allocate the PCI register window. */
	res_id = TWA_IO_CONFIG_REG;
	if ((sc->twa_io_res = bus_alloc_resource(dev, SYS_RES_IOPORT, &res_id,
					0, ~0, 1, RF_ACTIVE)) == NULL) {
		twa_printf(sc, "can't allocate register window.\n");
		twa_free(sc);
		return(ENXIO);
	}
	sc->twa_bus_tag = rman_get_bustag(sc->twa_io_res);
	sc->twa_bus_handle = rman_get_bushandle(sc->twa_io_res);

	/* Allocate and connect our interrupt. */
	res_id = 0;
	if ((sc->twa_irq_res = bus_alloc_resource(sc->twa_bus_dev, SYS_RES_IRQ,
					&res_id, 0, ~0, 1,
					RF_SHAREABLE | RF_ACTIVE)) == NULL) {
		twa_printf(sc, "Can't allocate interrupt.\n");
		twa_free(sc);
		return(ENXIO);
	}
	if (bus_setup_intr(sc->twa_bus_dev, sc->twa_irq_res, INTR_TYPE_CAM,
				twa_pci_intr, sc, &sc->twa_intr_handle)) {
		twa_printf(sc, "Can't set up interrupt.\n");
		twa_free(sc);
		return(ENXIO);
	}

	/* Initialize the driver for this controller. */
	if ((error = twa_setup(sc))) {
		twa_free(sc);
		return(error);
	}

	/* Print some information about the controller and configuration. */
	twa_describe_controller(sc);

	/* Create the control device. */
	sc->twa_ctrl_dev = make_dev(&twa_cdevsw, device_get_unit(sc->twa_bus_dev),
					UID_ROOT, GID_OPERATOR, S_IRUSR | S_IWUSR,
					"twa%d", device_get_unit(sc->twa_bus_dev));
	sc->twa_ctrl_dev->si_drv1 = sc;
	twa_enable_interrupts(sc);

	if ((error = twa_cam_setup(sc))) {
		twa_free(sc);
		return(error);
	}
	return(0);
}



/*
 * Function name:	twa_free
 * Description:		Performs clean-up at the time of going down.
 *
 * Input:		sc	-- ptr to per ctlr structure
 * Output:		None
 * Return value:	None
 */
static void
twa_free(struct twa_softc *sc)
{
	struct twa_request	*tr;

	twa_dbg_dprint_enter(3, sc);

	/* Detach from CAM */
	twa_cam_detach(sc);

	/* Destroy dma handles. */

	bus_dmamap_unload(sc->twa_dma_tag, sc->twa_cmd_map); 
	while ((tr = twa_dequeue_free(sc)) != NULL)
		bus_dmamap_destroy(sc->twa_dma_tag, tr->tr_dma_map);

	/* Free all memory allocated so far. */
	if (sc->twa_req_buf)
		free(sc->twa_req_buf, TWA_MALLOC_CLASS);
	if (sc->twa_cmd_pkt_buf)
		bus_dmamem_free(sc->twa_dma_tag, sc->twa_cmd_pkt_buf,
					sc->twa_cmd_map);
	if (sc->twa_aen_queue[0])
		free (sc->twa_aen_queue[0], M_DEVBUF);

	/* Destroy the data-transfer DMA tag. */
	if (sc->twa_dma_tag)
		bus_dma_tag_destroy(sc->twa_dma_tag);

	/* Disconnect the interrupt handler. */
	if (sc->twa_intr_handle)
		bus_teardown_intr(sc->twa_bus_dev, sc->twa_irq_res,
					sc->twa_intr_handle);
	if (sc->twa_irq_res != NULL)
		bus_release_resource(sc->twa_bus_dev, SYS_RES_IRQ,
					0, sc->twa_irq_res);

	/* Release the register window mapping. */
	if (sc->twa_io_res != NULL)
		bus_release_resource(sc->twa_bus_dev, SYS_RES_IOPORT,
					TWA_IO_CONFIG_REG, sc->twa_io_res);

	/* Destroy the control device. */
	if (sc->twa_ctrl_dev != (dev_t)NULL)
		destroy_dev(sc->twa_ctrl_dev);

	sysctl_ctx_free(&sc->twa_sysctl_ctx);
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
static int
twa_detach(device_t dev)
{
	struct twa_softc	*sc = device_get_softc(dev);
	int			s;
	int			error;

	twa_dbg_dprint_enter(3, sc);

	error = EBUSY;
	s = splcam();
	if (sc->twa_state & TWA_STATE_OPEN)
		goto out;

	/* Shut the controller down. */
	if ((error = twa_shutdown(dev)))
		goto out;

	/* Free all resources associated with this controller. */
	twa_free(sc);
	error = 0;

out:
	splx(s);
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
static int
twa_shutdown(device_t dev)
{
	struct twa_softc	*sc = device_get_softc(dev);
	int			s;
	int			error = 0;

	twa_dbg_dprint_enter(3, sc);

	s = splcam();

	/* Disconnect from the controller. */
	error = twa_deinit_ctlr(sc);

	splx(s);
	return(error);
}



/*
 * Function name:	twa_suspend
 * Description:		Called to suspend I/O before hot-swapping PCI ctlrs.
 *			Doesn't do much as of now.
 *
 * Input:		dev	-- bus device corresponding to the ctlr
 * Output:		None
 * Return value:	0	-- success
 *			non-zero-- failure
 */
static int
twa_suspend(device_t dev)
{
	struct twa_softc	*sc = device_get_softc(dev);
	int			s;

	twa_dbg_dprint_enter(3, sc);

	s = splcam();
	sc->twa_state |= TWA_STATE_SUSPEND;
    
	twa_disable_interrupts(sc);
	splx(s);

	return(1);
}



/*
 * Function name:	twa_resume
 * Description:		Called to resume I/O after hot-swapping PCI ctlrs.
 *			Doesn't do much as of now.
 *
 * Input:		dev	-- bus device corresponding to the ctlr
 * Output:		None
 * Return value:	0	-- success
 *			non-zero-- failure
 */
static int
twa_resume(device_t dev)
{
	struct twa_softc	*sc = device_get_softc(dev);

	twa_dbg_dprint_enter(3, sc);

	sc->twa_state &= ~TWA_STATE_SUSPEND;
	twa_enable_interrupts(sc);

	return(1);
}



/*
 * Function name:	twa_pci_intr
 * Description:		Interrupt handler.  Wrapper for twa_interrupt.
 *
 * Input:		arg	-- ptr to per ctlr structure
 * Output:		None
 * Return value:	None
 */
static void
twa_pci_intr(void *arg)
{
	struct twa_softc	*sc = (struct twa_softc *)arg;

	twa_interrupt(sc);
}



/*
 * Function name:	twa_write_pci_config
 * Description:		Writes to the PCI config space.
 *
 * Input:		sc	-- ptr to per ctlr structure
 *			value	-- value to be written
 *			size	-- # of bytes to be written
 * Output:		None
 * Return value:	None
 */
void
twa_write_pci_config(struct twa_softc *sc, u_int32_t value, int size)
{
	pci_write_config(sc->twa_bus_dev, PCIR_STATUS, value, size);
}



/*
 * Function name:	twa_alloc_req_pkts
 * Description:		Allocates memory for, and initializes request pkts,
 *			and queues them in the free queue.
 *
 * Input:		sc	-- ptr to per ctlr structure
 *			num_reqs-- # of request pkts to allocate and initialize.
 * Output:		None
 * Return value:	0	-- success
 *			non-zero-- failure
 */
int
twa_alloc_req_pkts(struct twa_softc *sc, int num_reqs)
{
	struct twa_request	*tr;
	int			i;

	if ((sc->twa_req_buf = malloc(num_reqs * sizeof(struct twa_request),
					TWA_MALLOC_CLASS, M_NOWAIT)) == NULL)
		return(ENOMEM);

	/* Allocate the bus DMA tag appropriate for PCI. */
	if (bus_dma_tag_create(NULL,			/* parent */
				TWA_ALIGNMENT,		/* alignment */
				0,			/* boundary */
				BUS_SPACE_MAXADDR,	/* lowaddr */
				BUS_SPACE_MAXADDR + 1, 	/* highaddr */
				NULL, NULL, 		/* filter, filterarg */
				TWA_Q_LENGTH *
				(sizeof(struct twa_command_packet)),/* maxsize */
				TWA_MAX_SG_ELEMENTS,	/* nsegments */
				BUS_SPACE_MAXSIZE_32BIT,/* maxsegsize */
				BUS_DMA_ALLOCNOW,	/* flags */
				&sc->twa_dma_tag	/* tag */)) {
		twa_printf(sc, "Can't allocate DMA tag.\n");
		return(ENOMEM);
	}

	/* Allocate memory for cmd pkts. */
	if (bus_dmamem_alloc(sc->twa_dma_tag,
				(void *)(&(sc->twa_cmd_pkt_buf)),
				BUS_DMA_WAITOK, &(sc->twa_cmd_map)))
		return(ENOMEM);

	bus_dmamap_load(sc->twa_dma_tag, sc->twa_cmd_map,
				sc->twa_cmd_pkt_buf,
				num_reqs * sizeof(struct twa_command_packet),
				twa_setup_request_dmamap, sc, 0);
	bzero(sc->twa_req_buf, num_reqs * sizeof(struct twa_request));
	bzero(sc->twa_cmd_pkt_buf,
			num_reqs * sizeof(struct twa_command_packet));

	for (i = 0; i < num_reqs; i++) {
		tr = &(sc->twa_req_buf[i]);
		tr->tr_command = &(sc->twa_cmd_pkt_buf[i]);
		tr->tr_cmd_phys = sc->twa_cmd_pkt_phys +
					(i * sizeof(struct twa_command_packet));
		tr->tr_request_id = i;
		tr->tr_sc = sc;
		sc->twa_lookup[i] = tr;

		/*
		 * Create a map for data buffers.  maxsize (256 * 1024) used in
		 * bus_dma_tag_create above should suffice the bounce page needs
		 * for data buffers, since the max I/O size we support is 128KB.
		 * If we supported I/O's bigger than 256KB, we would have to
		 * create a second dma_tag, with the appropriate maxsize.
		 */
		if (bus_dmamap_create(sc->twa_dma_tag, 0,
						&tr->tr_dma_map))
			return(ENOMEM);

		/* Insert request into the free queue. */
		twa_release_request(tr);
	}
	return(0);
}



/*
 * Function name:	twa_fillin_sgl
 * Description:		Fills in the scatter/gather list.
 *
 * Input:		sgl	-- ptr to sg list
 *			segs	-- ptr to fill the sg list from
 *			nsegments--# of segments
 * Output:		None
 * Return value:	None
 */
static void
twa_fillin_sgl(struct twa_sg *sgl, bus_dma_segment_t *segs, int nsegments)
{
	int	i;

	for (i = 0; i < nsegments; i++) {
		sgl[i].address = segs[i].ds_addr;
		sgl[i].length = segs[i].ds_len;
	}
}



/*
 * Function name:	twa_setup_data_dmamap
 * Description:		Callback of bus_dmamap_load for the buffer associated
 *			with data.  Updates the cmd pkt (size/sgl_entries
 *			fields, as applicable) to reflect the number of sg
 *			elements.
 *
 * Input:		arg	-- ptr to request pkt
 *			segs	-- ptr to a list of segment descriptors
 *			nsegments--# of segments
 *			error	-- 0 if no errors encountered before callback,
 *				   non-zero if errors were encountered
 * Output:		None
 * Return value:	None
 */
static void
twa_setup_data_dmamap(void *arg, bus_dma_segment_t *segs,
					int nsegments, int error)
{
	struct twa_request		*tr = (struct twa_request *)arg;
	struct twa_command_packet	*cmdpkt = tr->tr_command;
	struct twa_command_9k		*cmd9k;
	union twa_command_7k		*cmd7k;
	u_int8_t			sgl_offset;

	twa_dbg_dprint_enter(10, tr->tr_sc);

	if ((tr->tr_flags & TWA_CMD_IN_PROGRESS) &&
			(tr->tr_cmd_pkt_type & TWA_CMD_PKT_TYPE_EXTERNAL))
		twa_allow_new_requests(tr->tr_sc, (void *)(tr->tr_private));

	if (error == EFBIG) {
		tr->tr_error = error;
		goto out;
	}

	if (tr->tr_cmd_pkt_type & TWA_CMD_PKT_TYPE_9K) {
		cmd9k = &(cmdpkt->command.cmd_pkt_9k);
		twa_fillin_sgl(&(cmd9k->sg_list[0]), segs, nsegments);
		cmd9k->sgl_entries += nsegments - 1;
	} else {
		/* It's a 7000 command packet. */
		cmd7k = &(cmdpkt->command.cmd_pkt_7k);
		if ((sgl_offset = cmdpkt->command.cmd_pkt_7k.generic.sgl_offset))
			twa_fillin_sgl((struct twa_sg *)
					(((u_int32_t *)cmd7k) + sgl_offset),
					segs, nsegments);
		/* Modify the size field, based on sg address size. */
		cmd7k->generic.size += 
				((TWA_64BIT_ADDRESSES ? 3 : 2) * nsegments);
	}

	if (tr->tr_flags & TWA_CMD_DATA_IN)
		bus_dmamap_sync(tr->tr_sc->twa_dma_tag, tr->tr_dma_map,
							BUS_DMASYNC_PREREAD);
	if (tr->tr_flags & TWA_CMD_DATA_OUT) {
		/* 
		 * If we're using an alignment buffer, and we're
		 * writing data, copy the real data out.
		 */
		if (tr->tr_flags & TWA_CMD_DATA_COPY_NEEDED)
			bcopy(tr->tr_real_data, tr->tr_data, tr->tr_real_length);
		bus_dmamap_sync(tr->tr_sc->twa_dma_tag, tr->tr_dma_map,
						BUS_DMASYNC_PREWRITE);
	}
	error = twa_submit_io(tr);

out:
	if (error) {
		twa_unmap_request(tr);
		/*
		 * If the caller had been returned EINPROGRESS, and he has
		 * registered a callback for handling completion, the callback
		 * will never get called because we were unable to submit the
		 * request.  So, free up the request right here.
		 */
		if ((tr->tr_flags & TWA_CMD_IN_PROGRESS) && (tr->tr_callback))
			twa_release_request(tr);
	}
}



/*
 * Function name:	twa_setup_request_dmamap
 * Description:		Callback of bus_dmamap_load for the buffer associated
 *			with a cmd pkt.
 *
 * Input:		arg	-- ptr to request pkt
 *			segs	-- ptr to a list of segment descriptors
 *			nsegments--# of segments
 *			error	-- 0 if no errors encountered before callback,
 *				   non-zero if errors were encountered
 * Output:		None
 * Return value:	None
 */
static void
twa_setup_request_dmamap(void *arg, bus_dma_segment_t *segs,
						int nsegments, int error)
{
	struct twa_softc	*sc = (struct twa_softc *)arg;

	twa_dbg_dprint_enter(10, sc);

	sc->twa_cmd_pkt_phys = segs[0].ds_addr;
}



/*
 * Function name:	twa_map_request
 * Description:		Maps a cmd pkt and data associated with it, into
 *			DMA'able memory.
 *
 * Input:		tr	-- ptr to request pkt
 * Output:		None
 * Return value:	0	-- success
 *			non-zero-- failure
 */
int
twa_map_request(struct twa_request *tr)
{
	struct twa_softc	*sc = tr->tr_sc;
	int			error = 0;

	twa_dbg_dprint_enter(10, sc);

	/* If the command involves data, map that too. */
	if (tr->tr_data != NULL) {
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
		if (((vm_offset_t)tr->tr_data % 512) || (tr->tr_length % 512)) {
			tr->tr_flags |= TWA_CMD_DATA_COPY_NEEDED;
			tr->tr_real_data = tr->tr_data; /* save original data pointer */
			tr->tr_real_length = tr->tr_length; /* save original data length */
			tr->tr_length = (tr->tr_length + 511) & ~511;
			tr->tr_data = malloc(tr->tr_length, TWA_MALLOC_CLASS, M_NOWAIT);
			if (tr->tr_data == NULL) {
				twa_printf(sc, "%s: malloc failed\n", __func__);
				tr->tr_data = tr->tr_real_data; /* restore original data pointer */
				tr->tr_length = tr->tr_real_length; /* restore original data length */
				return(ENOMEM);
			}
		}
	
		/*
		 * Map the data buffer into bus space and build the s/g list.
		 */
		if ((error = bus_dmamap_load(sc->twa_dma_tag, tr->tr_dma_map,
					tr->tr_data, tr->tr_length, 
					twa_setup_data_dmamap, tr,
					BUS_DMA_WAITOK))) {
			if (error == EINPROGRESS) {
				tr->tr_flags |= TWA_CMD_IN_PROGRESS;
				if (tr->tr_cmd_pkt_type & TWA_CMD_PKT_TYPE_EXTERNAL)
					twa_disallow_new_requests(sc);
				error = 0;
			} else {
				/* Free alignment buffer if it was used. */
				if (tr->tr_flags & TWA_CMD_DATA_COPY_NEEDED) {
					free(tr->tr_data, TWA_MALLOC_CLASS);
					tr->tr_data = tr->tr_real_data;	/* restore 'real' data pointer */
					tr->tr_length = tr->tr_real_length;/* restore 'real' data length */
				}
			}
		} else
			error = tr->tr_error;

	} else
		if ((error = twa_submit_io(tr)))
			twa_unmap_request(tr);

	return(error);
}



/*
 * Function name:	twa_unmap_request
 * Description:		Undoes the mapping done by twa_map_request.
 *
 * Input:		tr	-- ptr to request pkt
 * Output:		None
 * Return value:	None
 */
void
twa_unmap_request(struct twa_request *tr)
{
	struct twa_softc	*sc = tr->tr_sc;
	u_int8_t		cmd_status;

	twa_dbg_dprint_enter(10, sc);

	/* If the command involved data, unmap that too. */
	if (tr->tr_data != NULL) {
		if (tr->tr_cmd_pkt_type & TWA_CMD_PKT_TYPE_9K)
			cmd_status = tr->tr_command->command.cmd_pkt_9k.status;
		else
			cmd_status = tr->tr_command->command.cmd_pkt_7k.generic.status;

		if (tr->tr_flags & TWA_CMD_DATA_IN) {
			bus_dmamap_sync(sc->twa_dma_tag,
					tr->tr_dma_map, BUS_DMASYNC_POSTREAD);

			/* 
			 * If we are using a bounce buffer, and we are reading
			 * data, copy the real data in.
			 */
			if (tr->tr_flags & TWA_CMD_DATA_COPY_NEEDED)
				if (cmd_status == 0)
					bcopy(tr->tr_data, tr->tr_real_data,
							tr->tr_real_length);
		}
		if (tr->tr_flags & TWA_CMD_DATA_OUT)
			bus_dmamap_sync(sc->twa_dma_tag, tr->tr_dma_map,
							BUS_DMASYNC_POSTWRITE);

		bus_dmamap_unload(sc->twa_dma_tag, tr->tr_dma_map); 
	}

	/* Free alignment buffer if it was used. */
	if (tr->tr_flags & TWA_CMD_DATA_COPY_NEEDED) {
		free(tr->tr_data, TWA_MALLOC_CLASS);
		tr->tr_data = tr->tr_real_data;	/* restore 'real' data pointer */
		tr->tr_length = tr->tr_real_length;/* restore 'real' data length */
	}
}



#ifdef TWA_DEBUG
void	twa_report(void);
void	twa_reset_stats(void);
void	twa_print_request(struct twa_request *tr, int req_type);



/*
 * Function name:	twa_report
 * Description:		For being called from ddb.  Prints controller stats,
 *			and requests, if any, that are in the wrong queue.
 *
 * Input:		None
 * Output:		None
 * Return value:	None
 */
void
twa_report(void)
{
	struct twa_softc	*sc;
	struct twa_request	*tr;
	int			s;
	int			i;

	s = splcam();
	for (i = 0; (sc = devclass_get_softc(twa_devclass, i)) != NULL; i++) {
		twa_print_controller(sc);
		TAILQ_FOREACH(tr, &sc->twa_busy, tr_link)
			twa_print_request(tr, TWA_CMD_BUSY);
		TAILQ_FOREACH(tr, &sc->twa_complete, tr_link)
			twa_print_request(tr, TWA_CMD_COMPLETE);
	}
	splx(s);
}



/*
 * Function name:	twa_reset_stats
 * Description:		For being called from ddb.
 *			Resets some controller stats.
 *
 * Input:		None
 * Output:		None
 * Return value:	None
 */
void
twa_reset_stats(void)
{
	struct twa_softc	*sc;
	int			s;
	int			i;

	s = splcam();
	for (i = 0; (sc = devclass_get_softc(twa_devclass, i)) != NULL; i++) {
		sc->twa_qstats[TWAQ_FREE].q_max = 0;
		sc->twa_qstats[TWAQ_BUSY].q_max = 0;
		sc->twa_qstats[TWAQ_PENDING].q_max = 0;
		sc->twa_qstats[TWAQ_COMPLETE].q_max = 0;
	}
	splx(s);
}



/*
 * Function name:	twa_print_request
 * Description:		Prints a given request if it's in the wrong queue.
 *
 * Input:		tr	-- ptr to request pkt
 *			req_type-- expected status of the given request
 * Output:		None
 * Return value:	None
 */
void
twa_print_request(struct twa_request *tr, int req_type)
{
	struct twa_softc		*sc = tr->tr_sc;
	struct twa_command_packet	*cmdpkt = tr->tr_command;
	struct twa_command_9k		*cmd9k;
	union twa_command_7k		*cmd7k;
	u_int8_t			*cdb;
	int				cmd_phys_addr;

	if (tr->tr_status != req_type) {
		twa_printf(sc, "Invalid %s request %p in queue! req_type = %x, queue_type = %x\n",
			(tr->tr_cmd_pkt_type & TWA_CMD_PKT_TYPE_INTERNAL) ? "INTERNAL" : "EXTERNAL",
			tr, tr->tr_status, req_type);

		if (tr->tr_cmd_pkt_type & TWA_CMD_PKT_TYPE_9K) {
			cmd9k = &(cmdpkt->command.cmd_pkt_9k);
			cmd_phys_addr = cmd9k->sg_list[0].address;
			twa_printf(sc, "9K cmd = %x %x %x %x %x %x %x %x %x\n",
					cmd9k->command.opcode,
					cmd9k->command.reserved,
					cmd9k->unit,
					cmd9k->request_id,
					cmd9k->status,
					cmd9k->sgl_offset,
					cmd9k->sgl_entries,
					cmd_phys_addr,
					cmd9k->sg_list[0].length);
			cdb = (u_int8_t *)(cmdpkt->command.cmd_pkt_9k.cdb);
			twa_printf(sc, "cdb = %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x\n",
				cdb[0], cdb[1], cdb[2], cdb[3], cdb[4], cdb[5], cdb[6], cdb[7],
				cdb[8], cdb[9], cdb[10], cdb[11], cdb[12], cdb[13], cdb[14], cdb[15]);
		} else {
			cmd7k = &(cmdpkt->command.cmd_pkt_7k);
			twa_printf(sc, "7K cmd = %x %x %x %x %x %x %x %x %x\n",
					cmd7k->generic.opcode,
					cmd7k->generic.sgl_offset,
					cmd7k->generic.size,
					cmd7k->generic.request_id,
					cmd7k->generic.unit,
					cmd7k->generic.host_id,
					cmd7k->generic.status,
					cmd7k->generic.flags,
					cmd7k->generic.count);
		}

		cmd_phys_addr = (int)(tr->tr_cmd_phys);
		twa_printf(sc, "cmdphys=0x%x data=%p length=0x%x\n",
				cmd_phys_addr, tr->tr_data, tr->tr_length);
		twa_printf(sc, "req_id=0x%x flags=0x%x callback=%p private=%p\n",
					tr->tr_request_id, tr->tr_flags,
					tr->tr_callback, tr->tr_private);
	}
}
#endif
