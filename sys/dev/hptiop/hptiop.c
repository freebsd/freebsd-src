/*
 * HighPoint RR3xxx RAID Driver for FreeBSD
 * Copyright (C) 2005-2007 HighPoint Technologies, Inc. All Rights Reserved.
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
#include <sys/types.h>
#include <sys/cons.h>
#if (__FreeBSD_version >= 500000)
#include <sys/time.h>
#include <sys/systm.h>
#else
#include <machine/clock.h>
#endif

#include <sys/stat.h>
#include <sys/malloc.h>
#include <sys/conf.h>
#include <sys/libkern.h>
#include <sys/kernel.h>

#if (__FreeBSD_version >= 500000)
#include <sys/kthread.h>
#include <sys/mutex.h>
#include <sys/module.h>
#endif

#include <sys/eventhandler.h>
#include <sys/bus.h>
#include <sys/taskqueue.h>
#include <sys/ioccom.h>

#include <machine/resource.h>
#include <machine/bus.h>
#include <machine/stdarg.h>
#include <sys/rman.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#if (__FreeBSD_version >= 500000)
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#else
#include <pci/pcivar.h>
#include <pci/pcireg.h>
#endif

#if (__FreeBSD_version <= 500043)
#include <sys/devicestat.h>
#endif

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt_sim.h>
#include <cam/cam_debug.h>
#include <cam/cam_xpt_periph.h>
#include <cam/cam_periph.h>
#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_message.h>

#if (__FreeBSD_version < 500043)
#include <sys/bus_private.h>
#endif

#include <dev/hptiop/hptiop.h>

static struct hpt_iop_hba * g_hba[4];
static int  iop_count = 0;
static char driver_name[] = "hptiop";
static char driver_version[] = "v1.2 (041307)";
static int  osm_max_targets = 32;

static devclass_t hptiop_devclass;

static void os_request_callback(struct hpt_iop_hba * hba, u_int32_t req);
static void os_message_callback(struct hpt_iop_hba * hba, u_int32_t msg);
static int  hptiop_do_ioctl(struct hpt_iop_hba * hba, struct hpt_iop_ioctl_param * pParams);
static void hptiop_bus_scan_cb(struct cam_periph *periph, union ccb *ccb);
static int  hptiop_rescan_bus(struct hpt_iop_hba * hba);
static int  hptiop_post_ioctl_command(struct hpt_iop_hba * hba,
		struct hpt_iop_request_ioctl_command * req, struct hpt_iop_ioctl_param * pParams);
static int  os_query_remove_device(struct hpt_iop_hba * hba, int target_id);
static int  hptiop_probe(device_t dev);
static int  hptiop_attach(device_t dev);
static int  hptiop_detach(device_t dev);
static int  hptiop_shutdown(device_t dev);
static void hptiop_action(struct cam_sim *sim, union ccb *ccb);
static void hptiop_poll(struct cam_sim *sim);
static void hptiop_async(void * callback_arg, u_int32_t code,
					struct cam_path * path, void * arg);
static void hptiop_pci_intr(void *arg);
static void hptiop_release_resource(struct hpt_iop_hba * hba);
static int  hptiop_reset_adapter(struct hpt_iop_hba * hba);
static void hptiop_enable_interrupts(struct hpt_iop_hba * hba);
static void hptiop_disable_interrupts(struct hpt_iop_hba * hba);

static d_open_t hptiop_open;
static d_close_t hptiop_close;
static d_ioctl_t hptiop_ioctl;

static struct cdevsw hptiop_cdevsw = {
	.d_open = hptiop_open,
	.d_close = hptiop_close,
	.d_ioctl = hptiop_ioctl,
	.d_name = driver_name,
#if __FreeBSD_version>=503000
	.d_version = D_VERSION,
#endif
#if (__FreeBSD_version>=503000 && __FreeBSD_version<600034)
	.d_flags = D_NEEDGIANT,
#endif
#if __FreeBSD_version<600034
#if __FreeBSD_version>=501000
	.d_maj = MAJOR_AUTO,
#else
	.d_maj = HPT_DEV_MAJOR,
#endif
#endif
};

#if __FreeBSD_version < 503000
#define hba_from_dev(dev) ((struct hpt_iop_hba *)(dev)->si_drv1)
#else
#define hba_from_dev(dev) \
		((struct hpt_iop_hba *)devclass_get_softc(hptiop_devclass, minor(dev)))
#endif

static int hptiop_open(ioctl_dev_t dev, int flags,
					int devtype, ioctl_thread_t proc)
{
	struct hpt_iop_hba * hba = hba_from_dev(dev);

	if (hba==NULL)
		return ENXIO;
	if (hba->flag & HPT_IOCTL_FLAG_OPEN)
		return EBUSY;
	hba->flag |= HPT_IOCTL_FLAG_OPEN;
	return 0;
}

static int hptiop_close(ioctl_dev_t dev, int flags,
					int devtype, ioctl_thread_t proc)
{
	struct hpt_iop_hba * hba = hba_from_dev(dev);
	hba->flag &= ~(u_int32_t)HPT_IOCTL_FLAG_OPEN;
	return 0;
}

static int hptiop_ioctl(ioctl_dev_t dev, u_long cmd, caddr_t data,
					int flags, ioctl_thread_t proc)
{
	int ret = EFAULT;
	struct hpt_iop_hba * hba = hba_from_dev(dev);

#if (__FreeBSD_version >= 500000)
	mtx_lock(&Giant);
#endif

	switch (cmd) {
	case HPT_DO_IOCONTROL:
		ret = hptiop_do_ioctl(hba, (struct hpt_iop_ioctl_param *)data);
		break;
	case HPT_SCAN_BUS:
		ret = hptiop_rescan_bus(hba);
		break;
	}

#if (__FreeBSD_version >= 500000)
	mtx_unlock(&Giant);
#endif

	return ret;
}

static __inline void * iop_get_inbound_request(struct hpt_iopmu * iop)
{
	u_int32_t m = readl(&iop->inbound_queue);
	return (m == 0xFFFFFFFF)? 0 : ((char *)iop + m);
}

static __inline void iop_post_inbound_request(struct hpt_iopmu * iop, void *req)
{
	writel(&iop->inbound_queue, (char *)req - (char *)iop);
}

static __inline void iop_post_outbound_request(struct hpt_iopmu * iop, void *req)
{
	writel(&iop->outbound_queue, (char *)req - (char *)iop);
}

static __inline void hptiop_pci_posting_flush(struct hpt_iopmu * iop)
{
	readl(&iop->outbound_intstatus);
}

static int iop_wait_ready(struct hpt_iopmu * iop, u_int32_t millisec)
{
	u_int32_t req=0;
	int i;

	for (i = 0; i < millisec; i++) {
		req = readl(&iop->inbound_queue);
		if (req != IOPMU_QUEUE_EMPTY)
			break;
		DELAY(1000);
	}

	if (req!=IOPMU_QUEUE_EMPTY) {
		writel(&iop->outbound_queue, req);
		hptiop_pci_posting_flush(iop);
		return 0;
	}

	return -1;
}

static int iop_intr(struct hpt_iop_hba * hba)
{
	struct hpt_iopmu * iop = hba->iop;
	u_int32_t status;
	int ret = 0;

	status = readl(&iop->outbound_intstatus);

	if (status & IOPMU_OUTBOUND_INT_MSG0) {
		u_int32_t msg = readl(&iop->outbound_msgaddr0);
		KdPrint(("received outbound msg %x", msg));
		writel(&iop->outbound_intstatus, IOPMU_OUTBOUND_INT_MSG0);
		os_message_callback(hba, msg);
		ret = 1;
	}

	if (status & IOPMU_OUTBOUND_INT_POSTQUEUE) {
		u_int32_t req;
		while ((req = readl(&iop->outbound_queue))
							!=IOPMU_QUEUE_EMPTY) {
			if (req & IOPMU_QUEUE_MASK_HOST_BITS)
				os_request_callback(hba, req);
			else {
				struct hpt_iop_request_header * p;
				p = (struct hpt_iop_request_header *)((char *)hba->iop + req);
				if (p->flags & IOP_REQUEST_FLAG_SYNC_REQUEST) {
					if (p->context)
						os_request_callback(hba, req);
					else
						p->context= 1;
				}
				else
					os_request_callback(hba, req);
			}
		}
		ret = 1;
	}
	return ret;
}

static int iop_send_sync_request(struct hpt_iop_hba * hba, void *req, u_int32_t millisec)
{
	u_int32_t i;

	((struct hpt_iop_request_header *)req)->flags |= IOP_REQUEST_FLAG_SYNC_REQUEST;
	((struct hpt_iop_request_header *)req)->context = 0;

	writel(&hba->iop->inbound_queue,
			(u_int32_t)((char *)req - (char *)hba->iop));

	hptiop_pci_posting_flush(hba->iop);

	for (i = 0; i < millisec; i++) {
		iop_intr(hba);
		if (((struct hpt_iop_request_header *)req)->context)
			return 0;
		DELAY(1000);
	}

	return -1;
}

static int iop_send_sync_msg(struct hpt_iop_hba * hba, u_int32_t msg, int *done, u_int32_t millisec)
{
	u_int32_t i;

	*done = 0;

	writel(&hba->iop->inbound_msgaddr0, msg);

	hptiop_pci_posting_flush(hba->iop);

	for (i = 0; i < millisec; i++) {
		iop_intr(hba);
		if (*done)
			break;
		DELAY(1000);
	}

	return *done? 0 : -1;
}

static int iop_get_config(struct hpt_iop_hba * hba, struct hpt_iop_request_get_config * config)
{
	u_int32_t req32=0;
	struct hpt_iop_request_get_config * req;

	if ((req32 = readl(&hba->iop->inbound_queue)) == IOPMU_QUEUE_EMPTY)
		return -1;

	req = (struct hpt_iop_request_get_config *)((char *)hba->iop + req32);
	req->header.flags = 0;
	req->header.type = IOP_REQUEST_TYPE_GET_CONFIG;
	req->header.size = sizeof(struct hpt_iop_request_get_config);
	req->header.result = IOP_RESULT_PENDING;

	if (iop_send_sync_request(hba, req, 20000)) {
		KdPrint(("Get config send cmd failed"));
		return -1;
	}

	*config = *req;
	writel(&hba->iop->outbound_queue, req32);
	return 0;
}

static int iop_set_config(struct hpt_iop_hba * hba, struct hpt_iop_request_set_config *config)
{
	u_int32_t req32;
	struct hpt_iop_request_set_config *req;

	req32 = readl(&hba->iop->inbound_queue);
	if (req32 == IOPMU_QUEUE_EMPTY)
		return -1;

	req = (struct hpt_iop_request_set_config *)((char *)hba->iop + req32);
	memcpy((u_int8_t *)req + sizeof(struct hpt_iop_request_header),
		(u_int8_t *)config + sizeof(struct hpt_iop_request_header),
		sizeof(struct hpt_iop_request_set_config) - sizeof(struct hpt_iop_request_header));
	req->header.flags = 0;
	req->header.type = IOP_REQUEST_TYPE_SET_CONFIG;
	req->header.size = sizeof(struct hpt_iop_request_set_config);
	req->header.result = IOP_RESULT_PENDING;

	if (iop_send_sync_request(hba, req, 20000)) {
		KdPrint(("Set config send cmd failed"));
		return -1;
	}

	writel(&hba->iop->outbound_queue, req32);
	return 0;
}

static int hptiop_do_ioctl(struct hpt_iop_hba * hba, struct hpt_iop_ioctl_param * pParams)
{
	struct hpt_iop_request_ioctl_command * req;

	if ((pParams->Magic != HPT_IOCTL_MAGIC) &&
		(pParams->Magic != HPT_IOCTL_MAGIC32))
		return EFAULT;

	req = (struct hpt_iop_request_ioctl_command *)iop_get_inbound_request(hba->iop);
	if (!req) {
		printf("hptiop: ioctl command failed");
		return EFAULT;
	}

	if (pParams->nInBufferSize)
		if (copyin((void *)pParams->lpInBuffer,
				req->buf, pParams->nInBufferSize))
			goto invalid;

	if (hptiop_post_ioctl_command(hba, req, pParams))
		goto invalid;

	if (req->header.result == IOP_RESULT_SUCCESS) {
		if (pParams->nOutBufferSize)
			if (copyout(req->buf +
					((pParams->nInBufferSize + 3) & ~3),
					(void*)pParams->lpOutBuffer,
					pParams->nOutBufferSize))
				goto invalid;

		if (pParams->lpBytesReturned)
			if (copyout(&req->bytes_returned,
					(void*)pParams->lpBytesReturned,
					sizeof(u_int32_t)))
				goto invalid;
		iop_post_outbound_request(hba->iop, req);
		return 0;
	} else{
invalid:
		iop_post_outbound_request(hba->iop, req);
		return EFAULT;
	}
}

static int hptiop_post_ioctl_command(struct hpt_iop_hba *hba,
		struct hpt_iop_request_ioctl_command *req, struct hpt_iop_ioctl_param *pParams)
{
	if (((pParams->nInBufferSize + 3) & ~3) + pParams->nOutBufferSize >
			hba->max_request_size - sizeof(struct hpt_iop_request_header) -
					4 * sizeof(u_int32_t)) {
		printf("hptiop: request size beyond max value");
		return -1;
	}

	req->ioctl_code = HPT_CTL_CODE_BSD_TO_IOP(pParams->dwIoControlCode);
	req->inbuf_size = pParams->nInBufferSize;
	req->outbuf_size = pParams->nOutBufferSize;

	req->header.size = sizeof(struct hpt_iop_request_ioctl_command)
					- 4 + pParams->nInBufferSize;
	req->header.context = (u_int64_t)(unsigned long)req;
	req->header.type = IOP_REQUEST_TYPE_IOCTL_COMMAND;
	req->header.result = IOP_RESULT_PENDING;
	req->header.flags |= IOP_REQUEST_FLAG_SYNC_REQUEST;

	hptiop_lock_adapter(hba);
	iop_post_inbound_request(hba->iop, req);
	hptiop_pci_posting_flush(hba->iop);

	while (req->header.context) {
		if (hptiop_sleep(hba, req,
				PPAUSE, "hptctl", HPT_OSM_TIMEOUT)==0)
			break;
		iop_send_sync_msg(hba, IOPMU_INBOUND_MSG0_RESET,
						&hba->msg_done, 60000);
	}

	hptiop_unlock_adapter(hba);
	return 0;
}

static int  hptiop_rescan_bus(struct hpt_iop_hba * hba)
{
	struct cam_path     *path;
	union ccb           *ccb;
	if (xpt_create_path(&path, xpt_periph, cam_sim_path(hba->sim),
		CAM_TARGET_WILDCARD, CAM_LUN_WILDCARD) != CAM_REQ_CMP)
		return(EIO);
	if ((ccb = malloc(sizeof(union ccb), M_TEMP, M_WAITOK)) == NULL)
		return(ENOMEM);
	bzero(ccb, sizeof(union ccb));
	xpt_setup_ccb(&ccb->ccb_h, path, 5);
	ccb->ccb_h.func_code = XPT_SCAN_BUS;
	ccb->ccb_h.cbfcnp = hptiop_bus_scan_cb;
	ccb->crcn.flags = CAM_FLAG_NONE;
	xpt_action(ccb);
	return(0);
}

static void hptiop_bus_scan_cb(struct cam_periph *periph, union ccb *ccb)
{
	xpt_free_path(ccb->ccb_h.path);
	free(ccb, M_TEMP);
	return;
}

static  bus_dmamap_callback_t   hptiop_map_srb;
static  bus_dmamap_callback_t   hptiop_post_scsi_command;

/*
 * CAM driver interface
 */
static device_method_t driver_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,     hptiop_probe),
	DEVMETHOD(device_attach,    hptiop_attach),
	DEVMETHOD(device_detach,    hptiop_detach),
	DEVMETHOD(device_shutdown,  hptiop_shutdown),
	{ 0, 0 }
};

static driver_t hptiop_pci_driver = {
	driver_name,
	driver_methods,
	sizeof(struct hpt_iop_hba)
};

DRIVER_MODULE(hptiop, pci, hptiop_pci_driver, hptiop_devclass, 0, 0);

static int hptiop_probe(device_t dev)
{
	struct hpt_iop_hba *hba;

	if ((pci_get_vendor(dev) == 0x1103 && pci_get_device(dev) == 0x3220) ||
		(pci_get_vendor(dev) == 0x1103 && pci_get_device(dev) == 0x3320) ||
		(pci_get_vendor(dev) == 0x1103 && pci_get_device(dev) == 0x3520)) {
		printf("hptiop: adapter at PCI %d:%d:%d, IRQ %d",
			pci_get_bus(dev), pci_get_slot(dev),
			pci_get_function(dev), pci_get_irq(dev));
		device_set_desc(dev, driver_name);
		hba = (struct hpt_iop_hba *)device_get_softc(dev);
		memset(hba, 0, sizeof(struct hpt_iop_hba));
		return 0;
	}
	return ENXIO;
}

static int hptiop_attach(device_t dev)
{
	struct hpt_iop_hba * hba = (struct hpt_iop_hba *)device_get_softc(dev);
	struct hpt_iop_request_get_config  iop_config;
	struct hpt_iop_request_set_config  set_config;
	int rid = 0;
	struct cam_devq *devq;
	struct ccb_setasync ccb;
	u_int32_t unit = device_get_unit(dev);
	
	printf("%s%d: RocketRAID 3xxx controller driver %s\n",
		driver_name, unit, driver_version);

	KdPrint(("hptiop_attach(%d, %d/%d/%d)", unit,
		pci_get_bus(dev), pci_get_slot(dev), pci_get_function(dev)));

#if __FreeBSD_version >=440000
	pci_enable_busmaster(dev);
#endif
	hba->pcidev = dev;
	hba->pciunit = unit;

	hba->bar0_rid = 0x10;
	hba->bar0_res = bus_alloc_resource(hba->pcidev,
					SYS_RES_MEMORY,	&hba->bar0_rid,
					0, ~0, 0x100000, RF_ACTIVE);

	if (hba->bar0_res == NULL) {
		printf("hptiop: Failed to get iop base adrress.\n");
		return ENXIO;
	}

	hba->iop = (struct hpt_iopmu *)rman_get_virtual(hba->bar0_res);

	if (!hba->iop) {
		printf("hptiop: alloc mem res failed\n");
		return ENXIO;
	}

	if (iop_wait_ready(hba->iop, 2000)) {
		printf("hptiop: adapter is not ready\n");
		return ENXIO;
	}

	if (iop_get_config(hba, &iop_config)) {
		printf("hptiop: Get iop config failed.\n");
		return ENXIO;
	}

	hba->firmware_version = iop_config.firmware_version;
	hba->interface_version = iop_config.interface_version;	
	hba->max_requests = iop_config.max_requests;
	hba->max_devices = iop_config.max_devices;
	hba->max_request_size = iop_config.request_size;
	hba->max_sg_count = iop_config.max_sg_count;

#if (__FreeBSD_version >= 500000)
	mtx_init(&hba->lock, "hptioplock", NULL, MTX_DEF);
#endif

	if (bus_dma_tag_create(NULL,/* parent */
			1,  /* alignment */
			0, /* boundary */
			BUS_SPACE_MAXADDR,  /* lowaddr */
			BUS_SPACE_MAXADDR,  /* highaddr */
			NULL, NULL,         /* filter, filterarg */
			BUS_SPACE_MAXSIZE_32BIT,    /* maxsize */
			BUS_SPACE_UNRESTRICTED, /* nsegments */
			BUS_SPACE_MAXSIZE_32BIT,    /* maxsegsize */
			0,      /* flags */
#if __FreeBSD_version>502000
			NULL,   /* lockfunc */
			NULL,       /* lockfuncarg */
#endif
			&hba->parent_dmat   /* tag */))
	{
		printf("hptiop: alloc parent_dmat failed\n");
		return ENXIO;
	}

	if (bus_dma_tag_create(hba->parent_dmat,/* parent */
			4,  /* alignment */
			BUS_SPACE_MAXADDR_32BIT+1, /* boundary */
			BUS_SPACE_MAXADDR,  /* lowaddr */
			BUS_SPACE_MAXADDR,  /* highaddr */
			NULL, NULL,         /* filter, filterarg */
			PAGE_SIZE * (hba->max_sg_count-1),  /* maxsize */
			hba->max_sg_count,  /* nsegments */
			0x20000,    /* maxsegsize */
			BUS_DMA_ALLOCNOW,       /* flags */
#if __FreeBSD_version>502000
			busdma_lock_mutex,  /* lockfunc */
			&hba->lock,     /* lockfuncarg */
#endif
			&hba->io_dmat   /* tag */))
	{
		printf("hptiop: alloc io_dmat failed\n");
		bus_dma_tag_destroy(hba->parent_dmat);
		return ENXIO;
	}

	if (bus_dma_tag_create(hba->parent_dmat,/* parent */
			1,  /* alignment */
			0, /* boundary */
			BUS_SPACE_MAXADDR_32BIT,    /* lowaddr */
			BUS_SPACE_MAXADDR,  /* highaddr */
			NULL, NULL,         /* filter, filterarg */
			HPT_SRB_MAX_SIZE * HPT_SRB_MAX_QUEUE_SIZE + 0x20,
			1,  /* nsegments */
			BUS_SPACE_MAXSIZE_32BIT,    /* maxsegsize */
			0,      /* flags */
#if __FreeBSD_version>502000
			NULL,   /* lockfunc */
			NULL,       /* lockfuncarg */
#endif
			&hba->srb_dmat  /* tag */))
	{
		printf("hptiop: alloc srb_dmat failed\n");
		bus_dma_tag_destroy(hba->io_dmat);
		bus_dma_tag_destroy(hba->parent_dmat);
		return ENXIO;
	}

	if (bus_dmamem_alloc(hba->srb_dmat, (void **)&hba->uncached_ptr,
#if __FreeBSD_version>501000
		BUS_DMA_WAITOK | BUS_DMA_COHERENT,
#else
		BUS_DMA_WAITOK,
#endif
		&hba->srb_dmamap) != 0)
	{
			printf("hptiop: bus_dmamem_alloc failed!\n");
release_tag:
			bus_dma_tag_destroy(hba->srb_dmat);
			bus_dma_tag_destroy(hba->io_dmat);
			bus_dma_tag_destroy(hba->parent_dmat);
			return ENXIO;
	}

	if (bus_dmamap_load(hba->srb_dmat,
			hba->srb_dmamap, hba->uncached_ptr,
			(HPT_SRB_MAX_SIZE * HPT_SRB_MAX_QUEUE_SIZE) + 0x20,
			hptiop_map_srb, hba, 0))
	{
		printf("hptiop: bus_dmamap_load failed!\n");
		goto release_tag;
	}

	if ((devq = cam_simq_alloc(hba->max_requests - 1 )) == NULL) {
		printf("hptiop: cam_simq_alloc failed\n");
attach_failed:
		hptiop_release_resource(hba);
		return ENXIO;
	}
	hba->sim = cam_sim_alloc(hptiop_action, hptiop_poll, driver_name,
			hba, unit, &Giant, hba->max_requests - 1, 1, devq);
	if (!hba->sim) {
		printf("hptiop: cam_sim_alloc failed\n");
		cam_simq_free(devq);
		goto attach_failed;
	}

	if (xpt_bus_register(hba->sim, dev, 0) != CAM_SUCCESS) {
		printf("hptiop: xpt_bus_register failed\n");
		cam_sim_free(hba->sim, /*free devq*/ TRUE);
		hba->sim = NULL;
		goto attach_failed;
	}

	if (xpt_create_path(&hba->path, /*periph */ NULL,
			cam_sim_path(hba->sim), CAM_TARGET_WILDCARD,
			CAM_LUN_WILDCARD) != CAM_REQ_CMP)
	{
		printf("hptiop: xpt_create_path failed\n");
		xpt_bus_deregister(cam_sim_path(hba->sim));
		cam_sim_free(hba->sim, /*free_devq*/TRUE);
		hba->sim = NULL;
		goto attach_failed;
	}

	bzero(&set_config, sizeof(set_config));
	set_config.iop_id = iop_count;
	set_config.vbus_id = cam_sim_path(hba->sim);
	set_config.max_host_request_size = HPT_SRB_MAX_REQ_SIZE;

	if (iop_set_config(hba, &set_config)) {
		printf("hptiop: Set iop config failed.\n");
		goto attach_failed;
	}

	xpt_setup_ccb(&ccb.ccb_h, hba->path, /*priority*/5);
	ccb.ccb_h.func_code = XPT_SASYNC_CB;
	ccb.event_enable = (AC_FOUND_DEVICE | AC_LOST_DEVICE);
	ccb.callback = hptiop_async;
	ccb.callback_arg = hba->sim;
	xpt_action((union ccb *)&ccb);

	rid = 0;
	if ((hba->irq_res = bus_alloc_resource(hba->pcidev, SYS_RES_IRQ,
			&rid, 0, ~0ul, 1, RF_SHAREABLE | RF_ACTIVE)) == NULL) {
		printf("hptiop: allocate irq failed!\n");
		goto attach_failed;
	}

	if (bus_setup_intr(hba->pcidev, hba->irq_res, INTR_TYPE_CAM,
				NULL, hptiop_pci_intr, hba, &hba->irq_handle)) {
		printf("hptiop: allocate intr function failed!\n");
		goto attach_failed;
	}

	hptiop_enable_interrupts(hba);

	if (iop_send_sync_msg(hba,
			IOPMU_INBOUND_MSG0_START_BACKGROUND_TASK,
			&hba->msg_done, 5000)) {
		printf("hptiop: Fail to start background task\n");
		goto attach_failed;
	}

	hba->ioctl_dev = make_dev(&hptiop_cdevsw, unit,
				UID_ROOT, GID_WHEEL /*GID_OPERATOR*/,
				S_IRUSR | S_IWUSR, "%s%d", driver_name, unit);

#if __FreeBSD_version < 503000
	hba->ioctl_dev->si_drv1 = hba;
#endif

	hptiop_rescan_bus(hba);

	g_hba[iop_count++] = hba;
	return 0;
}

static int hptiop_detach(device_t dev)
{
	struct hpt_iop_hba * hba = (struct hpt_iop_hba *)device_get_softc(dev);
	int i;
	int error = EBUSY;

	hptiop_lock_adapter(hba);
	for (i = 0; i < osm_max_targets; i++)
		if (os_query_remove_device(hba, i)) {
			printf("hptiop%d file system is busy. id=%d",
						hba->pciunit, i);
			goto out;
		}

	if ((error = hptiop_shutdown(dev)) != 0)
		goto out;
	if (iop_send_sync_msg(hba, IOPMU_INBOUND_MSG0_STOP_BACKGROUND_TASK,
						&hba->msg_done, 60000))
		goto out;

	hptiop_release_resource(hba);
	error = 0;
out:
	hptiop_unlock_adapter(hba);
	return error;
}

static int hptiop_shutdown(device_t dev)
{
	struct hpt_iop_hba * hba = (struct hpt_iop_hba *)device_get_softc(dev);

	int error = 0;

	if (hba->flag & HPT_IOCTL_FLAG_OPEN) {
		printf("hptiop: %d server is busy", hba->pciunit);
		return EBUSY;
	}
	hptiop_disable_interrupts(hba);
	if (iop_send_sync_msg(hba, IOPMU_INBOUND_MSG0_SHUTDOWN,
						&hba->msg_done, 60000))
		error = EBUSY;

	return error;
}

static void hptiop_pci_intr(void *arg)
{
	struct hpt_iop_hba * hba = (struct hpt_iop_hba *)arg;
	hptiop_lock_adapter(hba);
	iop_intr(hba);
	hptiop_unlock_adapter(hba);
}

static void hptiop_poll(struct cam_sim *sim)
{
	hptiop_pci_intr(cam_sim_softc(sim));
}

static void hptiop_async(void * callback_arg, u_int32_t code,
					struct cam_path * path, void * arg)
{
}

static void hptiop_enable_interrupts(struct hpt_iop_hba * hba)
{
	writel(&hba->iop->outbound_intmask,
		~(IOPMU_OUTBOUND_INT_POSTQUEUE | IOPMU_OUTBOUND_INT_MSG0));
}

static void hptiop_disable_interrupts(struct hpt_iop_hba * hba)
{
	u_int32_t int_mask;

	int_mask = readl(&hba->iop->outbound_intmask);
	writel(&hba->iop->outbound_intmask, int_mask |
		IOPMU_OUTBOUND_INT_POSTQUEUE | IOPMU_OUTBOUND_INT_MSG0);
	hptiop_pci_posting_flush(hba->iop);
}

static int hptiop_reset_adapter(struct hpt_iop_hba * hba)
{
	return iop_send_sync_msg(hba, IOPMU_INBOUND_MSG0_RESET,
						&hba->msg_done, 60000);
}

static void *hptiop_get_srb(struct hpt_iop_hba * hba)
{
	struct hpt_iop_srb * srb;

	if (hba->srb_list) {
		srb = hba->srb_list;
		hba->srb_list = srb->next;
	}
	else
		srb=NULL;

	return srb;
}

static void hptiop_free_srb(struct hpt_iop_hba * hba, struct hpt_iop_srb * srb)
{
	srb->next = hba->srb_list;
	hba->srb_list = srb;
}

static void hptiop_action(struct cam_sim *sim, union ccb *ccb)
{
	struct hpt_iop_hba * hba = (struct hpt_iop_hba *)cam_sim_softc(sim);
	struct hpt_iop_srb * srb;

	switch (ccb->ccb_h.func_code) {

	case XPT_SCSI_IO:
		hptiop_lock_adapter(hba);
		if (ccb->ccb_h.target_lun != 0 ||
			ccb->ccb_h.target_id >= osm_max_targets ||
			(ccb->ccb_h.flags & CAM_CDB_PHYS))
		{
			ccb->ccb_h.status = CAM_TID_INVALID;
			xpt_done(ccb);
			goto scsi_done;
		}

		if ((srb = hptiop_get_srb(hba)) == NULL) {
			printf("hptiop: srb allocated failed");
			ccb->ccb_h.status = CAM_REQ_CMP_ERR;
			xpt_done(ccb);
			goto scsi_done;
		}

		srb->ccb = ccb;

		if ((ccb->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_NONE)
			hptiop_post_scsi_command(srb, NULL, 0, 0);
		else if ((ccb->ccb_h.flags & CAM_SCATTER_VALID) == 0) {
			if ((ccb->ccb_h.flags & CAM_DATA_PHYS) == 0) {
				int error;

				error = bus_dmamap_load(hba->io_dmat,
					srb->dma_map,
					ccb->csio.data_ptr, ccb->csio.dxfer_len,
					hptiop_post_scsi_command, srb, 0);

				if (error && error != EINPROGRESS) {
					printf("hptiop: %d bus_dmamap_load error %d",
							hba->pciunit, error);
					xpt_freeze_simq(hba->sim, 1);
					ccb->ccb_h.status = CAM_REQ_CMP_ERR;
invalid:
					hptiop_free_srb(hba, srb);
					xpt_done(ccb);
					goto scsi_done;
				}
			}
			else {
				printf("hptiop: CAM_DATA_PHYS not supported");
				ccb->ccb_h.status = CAM_REQ_CMP_ERR;
				goto invalid;
			}
		}
		else {
			struct bus_dma_segment *segs;

			if ((ccb->ccb_h.flags & CAM_SG_LIST_PHYS) == 0 ||
				(ccb->ccb_h.flags & CAM_DATA_PHYS) != 0) {
				printf("hptiop: SCSI cmd failed");
				ccb->ccb_h.status=CAM_PROVIDE_FAIL;
				goto invalid;
			}

			segs = (struct bus_dma_segment *)ccb->csio.data_ptr;
			hptiop_post_scsi_command(srb, segs,
						ccb->csio.sglist_cnt, 0);
		}

scsi_done:
		hptiop_unlock_adapter(hba);
		return;

	case XPT_RESET_BUS:
		printf("hptiop: reset adapter");
		hptiop_lock_adapter(hba);
		hba->msg_done = 0;
		hptiop_reset_adapter(hba);
		hptiop_unlock_adapter(hba);
		break;

	case XPT_GET_TRAN_SETTINGS:
	case XPT_SET_TRAN_SETTINGS:
		ccb->ccb_h.status = CAM_FUNC_NOTAVAIL;
		break;

	case XPT_CALC_GEOMETRY:
		ccb->ccg.heads = 255;
		ccb->ccg.secs_per_track = 63;
		ccb->ccg.cylinders = ccb->ccg.volume_size /
				(ccb->ccg.heads * ccb->ccg.secs_per_track);
		ccb->ccb_h.status = CAM_REQ_CMP;
		break;

	case XPT_PATH_INQ:
	{
		struct ccb_pathinq *cpi = &ccb->cpi;

		cpi->version_num = 1;
		cpi->hba_inquiry = PI_SDTR_ABLE;
		cpi->target_sprt = 0;
		cpi->hba_misc = PIM_NOBUSRESET;
		cpi->hba_eng_cnt = 0;
		cpi->max_target = osm_max_targets;
		cpi->max_lun = 0;
		cpi->unit_number = cam_sim_unit(sim);
		cpi->bus_id = cam_sim_bus(sim);
		cpi->initiator_id = osm_max_targets;
		cpi->base_transfer_speed = 3300;

		strncpy(cpi->sim_vid, "FreeBSD", SIM_IDLEN);
		strncpy(cpi->hba_vid, "HPT   ", HBA_IDLEN);
		strncpy(cpi->dev_name, cam_sim_name(sim), DEV_IDLEN);
		cpi->ccb_h.status = CAM_REQ_CMP;
		break;
	}

	default:
		ccb->ccb_h.status = CAM_REQ_INVALID;
		break;
	}

	xpt_done(ccb);
	return;
}

static void hptiop_post_scsi_command(void *arg, bus_dma_segment_t *segs,
							int nsegs, int error)
{
	int idx;
	struct hpt_iop_srb *srb = (struct hpt_iop_srb *)arg;
	union ccb *ccb = srb->ccb;
	u_int8_t *cdb;
	struct hpt_iop_hba * hba = srb->hba;
	struct hpt_iop_request_scsi_command * req;

	if (error) {
scsi_error:
		printf("hptiop: post scsi command: dma error, err = %d, nsegs = %d",
					error, nsegs);
		ccb->ccb_h.status = CAM_BUSY;
		bus_dmamap_unload(hba->io_dmat, srb->dma_map);
		hptiop_free_srb(hba, srb);
		xpt_done(ccb);
		return;
	}

	if (nsegs > hba->max_sg_count) {
		printf("hptiop: nsegs is too large: nsegs=%d, Allowed count=%d",
					nsegs, hba->max_sg_count);
		goto scsi_error;
	}

	if (!srb) {
		printf("hptiop: invalid srb");
		goto scsi_error;
	}

	if (srb->srb_flag & HPT_SRB_FLAG_HIGH_MEM_ACESS) {
		u_int32_t m = readl(&hba->iop->inbound_queue);

		if (m == 0xFFFFFFFF) {
			printf("hptiop: invaild req offset: %d", m);
			goto scsi_error;
		}
		req = (struct hpt_iop_request_scsi_command *)((char *)hba->iop + m);
	}
	else
		req = (struct hpt_iop_request_scsi_command *)srb;

	if (ccb->csio.dxfer_len && nsegs > 0) {
		struct hpt_iopsg *psg = req->sg_list;
		for (idx = 0; idx < nsegs; idx++, psg++) {
			psg->pci_address = (u_int64_t)segs[idx].ds_addr;
			psg->size = segs[idx].ds_len;
			psg->eot = 0;
		}
		psg[-1].eot = 1;
	}

	if (ccb->ccb_h.flags & CAM_CDB_POINTER)
		cdb = ccb->csio.cdb_io.cdb_ptr;
	else
		cdb = ccb->csio.cdb_io.cdb_bytes;

	bcopy(cdb, req->cdb, ccb->csio.cdb_len);

	req->header.type = IOP_REQUEST_TYPE_SCSI_COMMAND;
	req->header.result = IOP_RESULT_PENDING;
	req->dataxfer_length = ccb->csio.dxfer_len;
	req->channel =  0;
	req->target =  ccb->ccb_h.target_id;
	req->lun =  ccb->ccb_h.target_lun;
	req->header.size = sizeof(struct hpt_iop_request_scsi_command)
					- sizeof(struct hpt_iopsg) + nsegs*sizeof(struct hpt_iopsg);

	if ((ccb->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN) {
		bus_dmamap_sync(hba->io_dmat,
					srb->dma_map, BUS_DMASYNC_PREREAD);
	}
	else if ((ccb->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_OUT)
		bus_dmamap_sync(hba->io_dmat,
					srb->dma_map, BUS_DMASYNC_PREWRITE);

	if (srb->srb_flag & HPT_SRB_FLAG_HIGH_MEM_ACESS) {
		req->header.context = (u_int64_t)(unsigned long)srb;
		req->header.flags = 0;
		writel(&hba->iop->inbound_queue,
					((char *)req - (char *)hba->iop));
	}
	else {
		req->header.context = (u_int64_t)srb->index |
						IOPMU_QUEUE_ADDR_HOST_BIT;
		req->header.flags = IOP_REQUEST_FLAG_OUTPUT_CONTEXT;

		if (hba->firmware_version > 0x01020000 || hba->interface_version > 0x01020000) {
			u_int32_t size_bits;

			if (req->header.size < 256)
				size_bits = IOPMU_QUEUE_REQUEST_SIZE_BIT;
			else if (req->header.size < 512)
				size_bits = IOPMU_QUEUE_ADDR_HOST_BIT;
			else
				size_bits = IOPMU_QUEUE_REQUEST_SIZE_BIT | IOPMU_QUEUE_ADDR_HOST_BIT;
			writel(&hba->iop->inbound_queue, srb->phy_addr | size_bits);
		} else		
			writel(&hba->iop->inbound_queue,
				srb->phy_addr | IOPMU_QUEUE_ADDR_HOST_BIT);
	}
}

static void os_request_callback(struct hpt_iop_hba * hba, u_int32_t index)
{
	struct hpt_iop_srb * srb;
	struct hpt_iop_request_scsi_command * req;
	union ccb *ccb;
	u_int8_t *cdb;

	if (index & IOPMU_QUEUE_MASK_HOST_BITS) {
		if (hba->firmware_version > 0x01020000 || hba->interface_version > 0x01020000) {
			srb = hba->srb[index & ~(u_int32_t)
				(IOPMU_QUEUE_ADDR_HOST_BIT | IOPMU_QUEUE_REQUEST_RESULT_BIT)];
			req = (struct hpt_iop_request_scsi_command *)srb;
			if (index & IOPMU_QUEUE_REQUEST_RESULT_BIT)
				req->header.result = IOP_RESULT_SUCCESS;
		} else {
			srb = hba->srb[index & ~(u_int32_t)IOPMU_QUEUE_ADDR_HOST_BIT];
			req = (struct hpt_iop_request_scsi_command *)srb;
		}
		goto srb_complete;
	}

	req = (struct hpt_iop_request_scsi_command *)((char *)hba->iop + index);

	switch(((struct hpt_iop_request_header *)req)->type) {
	case IOP_REQUEST_TYPE_IOCTL_COMMAND:
	{
		struct hpt_iop_request_ioctl_command * p;
		p = (struct hpt_iop_request_ioctl_command *)(unsigned long)
				(((struct hpt_iop_request_header *)req)->context);
		((struct hpt_iop_request_header *)req)->context = 0;
		wakeup(req);
		break;
	}

	case IOP_REQUEST_TYPE_SCSI_COMMAND:
		srb = (struct hpt_iop_srb *)(unsigned long)req->header.context;
srb_complete:
		ccb = (union ccb *)srb->ccb;
		if (ccb->ccb_h.flags & CAM_CDB_POINTER)
			cdb = ccb->csio.cdb_io.cdb_ptr;
		else
			cdb = ccb->csio.cdb_io.cdb_bytes;

		if (cdb[0] == SYNCHRONIZE_CACHE) { /* ??? */
			ccb->ccb_h.status = CAM_REQ_CMP;
			goto scsi_done;
		}

		switch (((struct hpt_iop_request_header *)req)->result) {
		case IOP_RESULT_SUCCESS:
			switch (ccb->ccb_h.flags & CAM_DIR_MASK) {
			case CAM_DIR_IN:
				bus_dmamap_sync(hba->io_dmat,
					srb->dma_map, BUS_DMASYNC_POSTREAD);
				bus_dmamap_unload(hba->io_dmat, srb->dma_map);
				break;
			case CAM_DIR_OUT:
				bus_dmamap_sync(hba->io_dmat,
					srb->dma_map, BUS_DMASYNC_POSTWRITE);
				bus_dmamap_unload(hba->io_dmat, srb->dma_map);
				break;
			}

			ccb->ccb_h.status = CAM_REQ_CMP;
			break;

		case IOP_RESULT_BAD_TARGET:
			ccb->ccb_h.status = CAM_DEV_NOT_THERE;
			break;
		case IOP_RESULT_BUSY:
			ccb->ccb_h.status = CAM_BUSY;
			break;
		case IOP_RESULT_INVALID_REQUEST:
			ccb->ccb_h.status = CAM_REQ_INVALID;
			break;
		case IOP_RESULT_FAIL:
			ccb->ccb_h.status = CAM_SCSI_STATUS_ERROR;
			break;
		case IOP_RESULT_RESET:
			ccb->ccb_h.status = CAM_BUSY;
			break;
		default:
			ccb->ccb_h.status = CAM_SCSI_STATUS_ERROR;
			break;
		}
scsi_done:
		if (srb->srb_flag & HPT_SRB_FLAG_HIGH_MEM_ACESS)
			iop_post_outbound_request(hba->iop, req);

		hptiop_free_srb(hba, srb);
		xpt_done(ccb);
		break;
	}
}

static void hptiop_map_srb(void *arg, bus_dma_segment_t *segs,
						int nsegs, int error)
{
	struct hpt_iop_hba * hba = (struct hpt_iop_hba *)arg;
	bus_addr_t phy_addr = (segs->ds_addr + 0x1F) & ~(bus_addr_t)0x1F;
	struct hpt_iop_srb *srb, *tmp_srb;
	int i;

	if (error || nsegs == 0) {
		printf("hptiop_map_srb error");
		return;
	}

	/* map srb */
	srb = (struct hpt_iop_srb *)
		(((unsigned long)hba->uncached_ptr + 0x1F) & ~(unsigned long)0x1F);

	for (i = 0; i < HPT_SRB_MAX_QUEUE_SIZE; i++) {
		tmp_srb = (struct hpt_iop_srb *)
					((char *)srb + i * HPT_SRB_MAX_SIZE);
		if (((unsigned long)tmp_srb & 0x1F) == 0) {
			if (bus_dmamap_create(hba->io_dmat,
						0, &tmp_srb->dma_map)) {
				printf("hptiop: dmamap create failed");
				return;
			}

			bzero(tmp_srb, sizeof(struct hpt_iop_srb));
			tmp_srb->hba = hba;
			tmp_srb->index = i;
			if (phy_addr & IOPMU_MAX_MEM_SUPPORT_MASK_32G)
				tmp_srb->srb_flag = HPT_SRB_FLAG_HIGH_MEM_ACESS;
			tmp_srb->phy_addr = (u_int32_t)(phy_addr >> 5);
			hptiop_free_srb(hba, tmp_srb);
			hba->srb[i] = tmp_srb;
			phy_addr += HPT_SRB_MAX_SIZE;
		}
		else {
			printf("hptiop: invalid alignment");
			return;
		}
	}
}

static void os_message_callback(struct hpt_iop_hba * hba, u_int32_t msg)
{
		hba->msg_done = 1;
}

static  int os_query_remove_device(struct hpt_iop_hba * hba, int target_id)
{
	struct cam_periph       *periph = NULL;
	struct cam_path         *path;
	int                     status, retval = 0;

	status = xpt_create_path(&path, NULL, hba->sim->path_id, target_id, 0);

	if (status == CAM_REQ_CMP) {
		if ((periph = cam_periph_find(path, "da")) != NULL) {
			if (periph->refcount >= 1) {
				printf("hptiop: %d ,target_id=0x%x, refcount=%d",
				    hba->pciunit, target_id, periph->refcount);
				retval = -1;
			}
		}
		xpt_free_path(path);
	}
	return retval;
}

static void hptiop_release_resource(struct hpt_iop_hba *hba)
{
	struct ccb_setasync ccb;

	if (hba->path) {
		xpt_setup_ccb(&ccb.ccb_h, hba->path, /*priority*/5);
		ccb.ccb_h.func_code = XPT_SASYNC_CB;
		ccb.event_enable = 0;
		ccb.callback = hptiop_async;
		ccb.callback_arg = hba->sim;
		xpt_action((union ccb *)&ccb);
		xpt_free_path(hba->path);
	}

	if (hba->sim) {
		xpt_bus_deregister(cam_sim_path(hba->sim));
		cam_sim_free(hba->sim, TRUE);
	}

	if (hba->srb_dmat) {
		bus_dmamem_free(hba->srb_dmat,
					hba->uncached_ptr, hba->srb_dmamap);
		bus_dmamap_unload(hba->srb_dmat, hba->srb_dmamap);
		bus_dma_tag_destroy(hba->srb_dmat);
	}

	if (hba->io_dmat)
		bus_dma_tag_destroy(hba->io_dmat);

	if (hba->parent_dmat)
		bus_dma_tag_destroy(hba->parent_dmat);

	if (hba->irq_handle)
		bus_teardown_intr(hba->pcidev, hba->irq_res, hba->irq_handle);

	if (hba->irq_res)
		bus_release_resource(hba->pcidev, SYS_RES_IRQ, 0, hba->irq_res);

	if (hba->bar0_res)
		bus_release_resource(hba->pcidev, SYS_RES_MEMORY,
					hba->bar0_rid, hba->bar0_res);

	if (hba->ioctl_dev)
		destroy_dev(hba->ioctl_dev);
}
