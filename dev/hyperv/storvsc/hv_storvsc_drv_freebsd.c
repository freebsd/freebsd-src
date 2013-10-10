/*-
 * Copyright (c) 2009-2012 Microsoft Corp.
 * Copyright (c) 2012 NetApp Inc.
 * Copyright (c) 2012 Citrix Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * StorVSC driver for Hyper-V.  This driver presents a SCSI HBA interface
 * to the Comman Access Method (CAM) layer.  CAM control blocks (CCBs) are
 * converted into VSCSI protocol messages which are delivered to the parent
 * partition StorVSP driver over the Hyper-V VMBUS.
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/condvar.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/queue.h>
#include <sys/lock.h>
#include <sys/sx.h>
#include <sys/taskqueue.h>
#include <sys/bus.h>
#include <sys/mutex.h>
#include <sys/callout.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <sys/lock.h>
#include <sys/sema.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_periph.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt_sim.h>
#include <cam/cam_xpt_internal.h>
#include <cam/cam_debug.h>
#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_message.h>


#include <dev/hyperv/include/hyperv.h>
#include "hv_vstorage.h"

#define STORVSC_RINGBUFFER_SIZE		(20*PAGE_SIZE)
#define STORVSC_MAX_LUNS_PER_TARGET	(64)
#define STORVSC_MAX_IO_REQUESTS		(STORVSC_MAX_LUNS_PER_TARGET * 2)
#define BLKVSC_MAX_IDE_DISKS_PER_TARGET	(1)
#define BLKVSC_MAX_IO_REQUESTS		STORVSC_MAX_IO_REQUESTS
#define STORVSC_MAX_TARGETS		(1)

struct storvsc_softc;

enum storvsc_request_type {
	WRITE_TYPE,
	READ_TYPE,
	UNKNOWN_TYPE
};

struct hv_storvsc_request {
	LIST_ENTRY(hv_storvsc_request) link;
	struct vstor_packet	vstor_packet;
	hv_vmbus_multipage_buffer data_buf;
	void *sense_data;
	uint8_t sense_info_len;
	uint8_t retries;
	union ccb *ccb;
	struct storvsc_softc *softc;
	struct callout callout;
	struct sema synch_sema; /*Synchronize the request/response if needed */
};

struct storvsc_softc {
	struct hv_device		*hs_dev;
        LIST_HEAD(, hv_storvsc_request) hs_free_list;
        struct mtx      		hs_lock;
        struct storvsc_driver_props     *hs_drv_props;
        int 				hs_unit;
        uint32_t         		hs_frozen;
        struct cam_sim  		*hs_sim;
        struct cam_path 		*hs_path;
	uint32_t			hs_num_out_reqs;
	boolean_t			hs_destroy;
	boolean_t			hs_drain_notify;
	struct sema 			hs_drain_sema;	
	struct hv_storvsc_request	hs_init_req;
	struct hv_storvsc_request	hs_reset_req;
};


/**
 * HyperV storvsc timeout testing cases:
 * a. IO returned after first timeout;
 * b. IO returned after second timeout and queue freeze;
 * c. IO returned while timer handler is running
 * The first can be tested by "sg_senddiag -vv /dev/daX",
 * and the second and third can be done by
 * "sg_wr_mode -v -p 08 -c 0,1a -m 0,ff /dev/daX".
 */ 
#define HVS_TIMEOUT_TEST 0

/*
 * Bus/adapter reset functionality on the Hyper-V host is
 * buggy and it will be disabled until
 * it can be further tested.
 */
#define HVS_HOST_RESET 0

struct storvsc_driver_props {
	char		*drv_name;
	char		*drv_desc;
	uint8_t		drv_max_luns_per_target;
	uint8_t		drv_max_ios_per_target; 
	uint32_t	drv_ringbuffer_size;
};

enum hv_storage_type {
	DRIVER_BLKVSC,
	DRIVER_STORVSC,
	DRIVER_UNKNOWN
};

#define HS_MAX_ADAPTERS 10

/* {ba6163d9-04a1-4d29-b605-72e2ffb1dc7f} */
static const hv_guid gStorVscDeviceType={
	.data = {0xd9, 0x63, 0x61, 0xba, 0xa1, 0x04, 0x29, 0x4d,
		 0xb6, 0x05, 0x72, 0xe2, 0xff, 0xb1, 0xdc, 0x7f}
};

/* {32412632-86cb-44a2-9b5c-50d1417354f5} */
static const hv_guid gBlkVscDeviceType={
	.data = {0x32, 0x26, 0x41, 0x32, 0xcb, 0x86, 0xa2, 0x44,
		 0x9b, 0x5c, 0x50, 0xd1, 0x41, 0x73, 0x54, 0xf5}
};

static struct storvsc_driver_props g_drv_props_table[] = {
	{"blkvsc", "Hyper-V IDE Storage Interface",
	 BLKVSC_MAX_IDE_DISKS_PER_TARGET, BLKVSC_MAX_IO_REQUESTS,
	 STORVSC_RINGBUFFER_SIZE},
	{"storvsc", "Hyper-V SCSI Storage Interface",
	 STORVSC_MAX_LUNS_PER_TARGET, STORVSC_MAX_IO_REQUESTS,
	 STORVSC_RINGBUFFER_SIZE}
};

static struct storvsc_softc *hs_softc[HS_MAX_ADAPTERS];

/* static functions */
static int storvsc_probe(device_t dev);
static int storvsc_attach(device_t dev);
static int storvsc_detach(device_t dev);
static void storvsc_poll(struct cam_sim * sim);
static void storvsc_action(struct cam_sim * sim, union ccb * ccb);
static void scan_for_luns(struct storvsc_softc * sc);
static void create_storvsc_request(union ccb *ccb, struct hv_storvsc_request *reqp);
static void storvsc_free_request(struct storvsc_softc *sc, struct hv_storvsc_request *reqp);
static enum hv_storage_type storvsc_get_storage_type(device_t dev);
static void hv_storvsc_on_channel_callback(void *context);
static void hv_storvsc_on_iocompletion( struct storvsc_softc *sc,
					struct vstor_packet *vstor_packet,
					struct hv_storvsc_request *request);
static int hv_storvsc_connect_vsp(struct hv_device *device);
static void storvsc_io_done(struct hv_storvsc_request *reqp);

static device_method_t storvsc_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		storvsc_probe),
	DEVMETHOD(device_attach,	storvsc_attach),
	DEVMETHOD(device_detach,	storvsc_detach),
	DEVMETHOD(device_shutdown,      bus_generic_shutdown),
	DEVMETHOD_END
};

static driver_t storvsc_driver = {
	"storvsc", storvsc_methods, sizeof(struct storvsc_softc),
};

static devclass_t storvsc_devclass;
DRIVER_MODULE(storvsc, vmbus, storvsc_driver, storvsc_devclass, 0, 0);
MODULE_VERSION(storvsc, 1);
MODULE_DEPEND(storvsc, vmbus, 1, 1, 1);


/**
 * The host is capable of sending messages to us that are 
 * completely unsolicited. So, we need to address the race
 * condition where we may be in the process of unloading the
 * driver when the host may send us an unsolicited message.
 * We address this issue by implementing a sequentially
 * consistent protocol:
 *
 * 1. Channel callback is invoked while holding the the channel lock
 *    and an unloading driver will reset the channel callback under
 *    the protection of this channel lock.
 *
 * 2. To ensure bounded wait time for unloading a driver, we don't
 *    permit outgoing traffic once the device is marked as being
 *    destroyed.
 *
 * 3. Once the device is marked as being destroyed, we only
 *    permit incoming traffic to properly account for 
 *    packets already sent out.
 */
static inline struct storvsc_softc *
get_stor_device(struct hv_device *device,
				boolean_t outbound)
{
	struct storvsc_softc *sc;

	sc = device_get_softc(device->device);
	if (sc == NULL) {
		return NULL;
	}

	if (outbound) {
		/*
		 * Here we permit outgoing I/O only
		 * if the device is not being destroyed.
		 */

		if (sc->hs_destroy) {
			sc = NULL;
		}
	} else {
		/*
		 * inbound case; if being destroyed
		 * only permit to account for
		 * messages already sent out.
		 */
		if (sc->hs_destroy && (sc->hs_num_out_reqs == 0)) {
			sc = NULL;
		}
	}
	return sc;
}

/**
 * @brief initialize channel connection to parent partition
 *
 * @param dev  a Hyper-V device pointer
 * @returns  0 on success, non-zero error on failure
 */
static int
hv_storvsc_channel_init(struct hv_device *dev)
{
	int ret = 0;
	struct hv_storvsc_request *request;
	struct vstor_packet *vstor_packet;
	struct storvsc_softc *sc;

	sc = get_stor_device(dev, TRUE);
	if (sc == NULL) {
		return ENODEV;
	}

	request = &sc->hs_init_req;
	memset(request, 0, sizeof(struct hv_storvsc_request));
	vstor_packet = &request->vstor_packet;
	request->softc = sc;

	/**
	 * Initiate the vsc/vsp initialization protocol on the open channel
	 */
	sema_init(&request->synch_sema, 0, ("stor_synch_sema"));

	vstor_packet->operation = VSTOR_OPERATION_BEGININITIALIZATION;
	vstor_packet->flags = REQUEST_COMPLETION_FLAG;


	ret = hv_vmbus_channel_send_packet(
			dev->channel,
			vstor_packet,
			sizeof(struct vstor_packet),
			(uint64_t)request,
			HV_VMBUS_PACKET_TYPE_DATA_IN_BAND,
			HV_VMBUS_DATA_PACKET_FLAG_COMPLETION_REQUESTED);

	if (ret != 0) {
		goto cleanup;
	}

	ret = sema_timedwait(&request->synch_sema, 500); /* KYS 5 seconds */

	if (ret != 0) {
		goto cleanup;
	}

	if (vstor_packet->operation != VSTOR_OPERATION_COMPLETEIO ||
		vstor_packet->status != 0) {
		goto cleanup;
	}

	/* reuse the packet for version range supported */

	memset(vstor_packet, 0, sizeof(struct vstor_packet));
	vstor_packet->operation = VSTOR_OPERATION_QUERYPROTOCOLVERSION;
	vstor_packet->flags = REQUEST_COMPLETION_FLAG;

	vstor_packet->u.version.major_minor = VMSTOR_PROTOCOL_VERSION_CURRENT;

	/* revision is only significant for Windows guests */
	vstor_packet->u.version.revision = 0;

	ret = hv_vmbus_channel_send_packet(
			dev->channel,
			vstor_packet,
			sizeof(struct vstor_packet),
			(uint64_t)request,
			HV_VMBUS_PACKET_TYPE_DATA_IN_BAND,
			HV_VMBUS_DATA_PACKET_FLAG_COMPLETION_REQUESTED);

	if (ret != 0) {
		goto cleanup;
	}

	ret = sema_timedwait(&request->synch_sema, 500); /* KYS 5 seconds */

	if (ret) {
		goto cleanup;
	}

	/* TODO: Check returned version */
	if (vstor_packet->operation != VSTOR_OPERATION_COMPLETEIO ||
		vstor_packet->status != 0) {
		goto cleanup;
	}

	/**
	 * Query channel properties
	 */
	memset(vstor_packet, 0, sizeof(struct vstor_packet));
	vstor_packet->operation = VSTOR_OPERATION_QUERYPROPERTIES;
	vstor_packet->flags = REQUEST_COMPLETION_FLAG;

	ret = hv_vmbus_channel_send_packet(
				dev->channel,
				vstor_packet,
				sizeof(struct vstor_packet),
				(uint64_t)request,
				HV_VMBUS_PACKET_TYPE_DATA_IN_BAND,
				HV_VMBUS_DATA_PACKET_FLAG_COMPLETION_REQUESTED);

	if ( ret != 0) {
		goto cleanup;
	}

	ret = sema_timedwait(&request->synch_sema, 500); /* KYS 5 seconds */

	if (ret != 0) {
		goto cleanup;
	}

	/* TODO: Check returned version */
	if (vstor_packet->operation != VSTOR_OPERATION_COMPLETEIO ||
		vstor_packet->status != 0) {
		goto cleanup;
	}

	memset(vstor_packet, 0, sizeof(struct vstor_packet));
	vstor_packet->operation = VSTOR_OPERATION_ENDINITIALIZATION;
	vstor_packet->flags = REQUEST_COMPLETION_FLAG;

	ret = hv_vmbus_channel_send_packet(
			dev->channel,
			vstor_packet,
			sizeof(struct vstor_packet),
			(uint64_t)request,
			HV_VMBUS_PACKET_TYPE_DATA_IN_BAND,
			HV_VMBUS_DATA_PACKET_FLAG_COMPLETION_REQUESTED);

	if (ret != 0) {
		goto cleanup;
	}

	ret = sema_timedwait(&request->synch_sema, 500); /* KYS 5 seconds */

	if (ret != 0) {
		goto cleanup;
	}

	if (vstor_packet->operation != VSTOR_OPERATION_COMPLETEIO ||
		vstor_packet->status != 0) {
		goto cleanup;
	}

cleanup:
	sema_destroy(&request->synch_sema);
	return (ret);
}

/**
 * @brief Open channel connection to paraent partition StorVSP driver
 *
 * Open and initialize channel connection to parent partition StorVSP driver.
 *
 * @param pointer to a Hyper-V device
 * @returns 0 on success, non-zero error on failure
 */
static int
hv_storvsc_connect_vsp(struct hv_device *dev)
{	
	int ret = 0;
	struct vmstor_chan_props props;
	struct storvsc_softc *sc;

	sc = device_get_softc(dev->device);
		
	memset(&props, 0, sizeof(struct vmstor_chan_props));

	/*
	 * Open the channel
	 */

	ret = hv_vmbus_channel_open(
		dev->channel,
		sc->hs_drv_props->drv_ringbuffer_size,
		sc->hs_drv_props->drv_ringbuffer_size,
		(void *)&props,
		sizeof(struct vmstor_chan_props),
		hv_storvsc_on_channel_callback,
		dev);


	if (ret != 0) {
		return ret;
	}

	ret = hv_storvsc_channel_init(dev);

	return (ret);
}

#if HVS_HOST_RESET
static int
hv_storvsc_host_reset(struct hv_device *dev)
{
	int ret = 0;
	struct storvsc_softc *sc;

	struct hv_storvsc_request *request;
	struct vstor_packet *vstor_packet;

	sc = get_stor_device(dev, TRUE);
	if (sc == NULL) {
		return ENODEV;
	}

	request = &sc->hs_reset_req;
	request->softc = sc;
	vstor_packet = &request->vstor_packet;

	sema_init(&request->synch_sema, 0, "stor synch sema");

	vstor_packet->operation = VSTOR_OPERATION_RESETBUS;
	vstor_packet->flags = REQUEST_COMPLETION_FLAG;

	ret = hv_vmbus_channel_send_packet(dev->channel,
			vstor_packet,
			sizeof(struct vstor_packet),
			(uint64_t)&sc->hs_reset_req,
			HV_VMBUS_PACKET_TYPE_DATA_IN_BAND,
			HV_VMBUS_DATA_PACKET_FLAG_COMPLETION_REQUESTED);

	if (ret != 0) {
		goto cleanup;
	}

	ret = sema_timedwait(&request->synch_sema, 500); /* KYS 5 seconds */

	if (ret) {
		goto cleanup;
	}


	/*
	 * At this point, all outstanding requests in the adapter 
	 * should have been flushed out and return to us
	 */

cleanup:
	sema_destroy(&request->synch_sema);
	return (ret);
}
#endif /* HVS_HOST_RESET */

/**
 * @brief Function to initiate an I/O request
 *
 * @param device Hyper-V device pointer
 * @param request pointer to a request structure
 * @returns 0 on success, non-zero error on failure
 */
static int
hv_storvsc_io_request(struct hv_device *device,
					  struct hv_storvsc_request *request)
{
	struct storvsc_softc *sc;
	struct vstor_packet *vstor_packet = &request->vstor_packet;
	int ret = 0;

	sc = get_stor_device(device, TRUE);

	if (sc == NULL) {
		return ENODEV;
	}

	vstor_packet->flags |= REQUEST_COMPLETION_FLAG;

	vstor_packet->u.vm_srb.length = sizeof(struct vmscsi_req);
	
	vstor_packet->u.vm_srb.sense_info_len = SENSE_BUFFER_SIZE;

	vstor_packet->u.vm_srb.transfer_len = request->data_buf.length;

	vstor_packet->operation = VSTOR_OPERATION_EXECUTESRB;


	mtx_unlock(&request->softc->hs_lock);
	if (request->data_buf.length) {
		ret = hv_vmbus_channel_send_packet_multipagebuffer(
				device->channel,
				&request->data_buf,
				vstor_packet, 
				sizeof(struct vstor_packet), 
				(uint64_t)request);

	} else {
		ret = hv_vmbus_channel_send_packet(
			device->channel,
			vstor_packet,
			sizeof(struct vstor_packet),
			(uint64_t)request,
			HV_VMBUS_PACKET_TYPE_DATA_IN_BAND,
			HV_VMBUS_DATA_PACKET_FLAG_COMPLETION_REQUESTED);
	}
	mtx_lock(&request->softc->hs_lock);

	if (ret != 0) {
		printf("Unable to send packet %p ret %d", vstor_packet, ret);
	} else {
		atomic_add_int(&sc->hs_num_out_reqs, 1);
	}

	return (ret);
}


/**
 * Process IO_COMPLETION_OPERATION and ready
 * the result to be completed for upper layer
 * processing by the CAM layer.
 */
static void
hv_storvsc_on_iocompletion(struct storvsc_softc *sc,
			   struct vstor_packet *vstor_packet,
			   struct hv_storvsc_request *request)
{
	struct vmscsi_req *vm_srb;

	vm_srb = &vstor_packet->u.vm_srb;

	request->sense_info_len = 0;
	if (((vm_srb->scsi_status & 0xFF) == SCSI_STATUS_CHECK_COND) &&
			(vm_srb->srb_status & SRB_STATUS_AUTOSENSE_VALID)) {
		/* Autosense data available */

		KASSERT(vm_srb->sense_info_len <= request->sense_info_len,
				("vm_srb->sense_info_len <= "
				 "request->sense_info_len"));

		memcpy(request->sense_data, vm_srb->u.sense_data,
			vm_srb->sense_info_len);

		request->sense_info_len = vm_srb->sense_info_len;
	}

	/* Complete request by passing to the CAM layer */
	storvsc_io_done(request);
	atomic_subtract_int(&sc->hs_num_out_reqs, 1);
	if (sc->hs_drain_notify && (sc->hs_num_out_reqs == 0)) {
		sema_post(&sc->hs_drain_sema);
	}
}

static void
hv_storvsc_on_channel_callback(void *context)
{
	int ret = 0;
	struct hv_device *device = (struct hv_device *)context;
	struct storvsc_softc *sc;
	uint32_t bytes_recvd;
	uint64_t request_id;
	uint8_t packet[roundup2(sizeof(struct vstor_packet), 8)];
	struct hv_storvsc_request *request;
	struct vstor_packet *vstor_packet;

	sc = get_stor_device(device, FALSE);
	if (sc == NULL) {
		return;
	}

	KASSERT(device, ("device"));

	ret = hv_vmbus_channel_recv_packet(
			device->channel,
			packet,
			roundup2(sizeof(struct vstor_packet), 8),
			&bytes_recvd,
			&request_id);

	while ((ret == 0) && (bytes_recvd > 0)) {
		request = (struct hv_storvsc_request *)request_id;
		KASSERT(request, ("request"));

		if ((request == &sc->hs_init_req) ||
			(request == &sc->hs_reset_req)) {
			memcpy(&request->vstor_packet, packet,
				   sizeof(struct vstor_packet));
			sema_post(&request->synch_sema); 
		} else {
			vstor_packet = (struct vstor_packet *)packet;
			switch(vstor_packet->operation) {
			case VSTOR_OPERATION_COMPLETEIO:
				hv_storvsc_on_iocompletion(sc,
							vstor_packet, request);
				break;
			case VSTOR_OPERATION_REMOVEDEVICE:
				/* TODO: implement */
				break;
			default:
				break;
			}			
		}
		ret = hv_vmbus_channel_recv_packet(
				device->channel,
				packet,
				roundup2(sizeof(struct vstor_packet), 8),
				&bytes_recvd,
				&request_id);
	}
}

/**
 * @brief callback function for completing a single LUN scan
 *
 * This function is responsible for waking up the executer of
 * the scan LUN CCB action (cam_periph_runccb.)  cam_periph_ccbwait
 * sleeps on the mutex being signaled.
 *
 * @param periph a pointer to a CAM peripheral
 * @param done_ccb pointer to CAM control block
 */
static void
storvsc_xptdone(struct cam_periph *periph, union ccb *done_ccb)
{
	wakeup(&done_ccb->ccb_h.cbfcnp);
}

/**
 * @brief scan for attached logical unit numbers (LUNs)
 *
 * In Hyper-V there is no backend changed device operation which
 * presents FreeBSD with a list of devices to connect.  The result is
 * that we have to scan for a list of luns in the storvsc_attach()
 * routine.  There is only one SCSI target, so scan for the maximum
 * number of luns.
 *
 * @param pointer to softc
 */
static void
scan_for_luns(struct storvsc_softc *sc)
{
	union ccb *request_ccb;
	struct cam_path *path = sc->hs_path;
	struct cam_path *my_path = NULL;
	cam_status status;
	int lun_nb = 0;
	int error;

	request_ccb = malloc(sizeof(union ccb), M_CAMXPT, M_WAITOK);
	my_path = malloc(sizeof(*my_path), M_CAMXPT, M_WAITOK);

	mtx_lock(&sc->hs_lock);
	do {
		/*
		 * Scan the next LUN. Reuse path and ccb structs.
		 */
		bzero(my_path, sizeof(*my_path));
		bzero(request_ccb, sizeof(*request_ccb));
		status = xpt_compile_path(my_path,
				  xpt_periph,
				  path->bus->path_id,
				  0,
				  lun_nb);

		if (status != CAM_REQ_CMP) {
			mtx_unlock(&sc->hs_lock);
	       		xpt_print(path, "scan_for_lunYYY: can't compile"
					 " path, 0x%p can't continue\n",
					 sc->hs_path);
			free(request_ccb, M_CAMXPT);
			free(my_path, M_CAMXPT);
			return;
		}

		xpt_setup_ccb(&request_ccb->ccb_h, my_path, 5);
		request_ccb->ccb_h.func_code = XPT_SCAN_LUN;
		request_ccb->ccb_h.cbfcnp    = storvsc_xptdone;
		request_ccb->crcn.flags	     = CAM_FLAG_NONE;

		error = cam_periph_runccb(request_ccb, NULL, 
						CAM_FLAG_NONE, 0, NULL);
		KASSERT(error == 0, ("cam_periph_runccb failed %d\n", error));
		xpt_release_path(my_path);
	} while ( ++lun_nb < sc->hs_drv_props->drv_max_luns_per_target);
	mtx_unlock(&sc->hs_lock);
	free(request_ccb, M_CAMXPT);
	free(my_path, M_CAMXPT);
}

/**
 * @brief StorVSC probe function
 *
 * Device probe function.  Returns 0 if the input device is a StorVSC
 * device.  Otherwise, a ENXIO is returned.  If the input device is
 * for BlkVSC (paravirtual IDE) device and this support is disabled in
 * favor of the emulated ATA/IDE device, return ENXIO.
 *
 * @param a device
 * @returns 0 on success, ENXIO if not a matcing StorVSC device
 */
static int
storvsc_probe(device_t dev)
{
	int ata_disk_enable = 0;
	int ret	= ENXIO;

	switch (storvsc_get_storage_type(dev)) {
	case DRIVER_BLKVSC:
		if(bootverbose)
			device_printf(dev, "DRIVER_BLKVSC-Emulated ATA/IDE probe\n");
		if (!getenv_int("hw.ata.disk_enable", &ata_disk_enable)) {
			if(bootverbose)
				device_printf(dev,
					"Enlightened ATA/IDE detected\n");
			ret = 0;
		} else if(bootverbose)
			device_printf(dev, "Emulated ATA/IDE set (hw.ata.disk_enable set)\n");
		break;
	case DRIVER_STORVSC:
		if(bootverbose)
			device_printf(dev, "Enlightened SCSI device detected\n");
		ret = 0;
		break;
	default:
		ret = ENXIO;
	}
	return (ret);
}

/**
 * @brief StorVSC attach function
 *
 * Function responsible for allocating per-device structures,
 * setting up CAM interfaces and scanning for available LUNs to
 * be used for SCSI device peripherals.
 *
 * @param a device
 * @returns 0 on success or an error on failure
 */
static int
storvsc_attach(device_t dev)
{
	struct hv_device *hv_dev = vmbus_get_devctx(dev);
	enum hv_storage_type stor_type;
	struct storvsc_softc *sc;
	struct cam_devq *devq;
	int ret, i;
	struct hv_storvsc_request *reqp;
	struct root_hold_token *root_mount_token = NULL;

	/*
	 * We need to serialize storvsc attach calls.
	 */
	root_mount_token = root_mount_hold("storvsc");

	sc = device_get_softc(dev);
	if (sc == NULL) {
		ret = ENOMEM;
		goto cleanup;
	}

	stor_type = storvsc_get_storage_type(dev);

	if (stor_type == DRIVER_UNKNOWN) {
		ret = ENODEV;
		goto cleanup;
	}

	bzero(sc, sizeof(struct storvsc_softc));

	/* fill in driver specific properties */
	sc->hs_drv_props = &g_drv_props_table[stor_type];

	/* fill in device specific properties */
	sc->hs_unit	= device_get_unit(dev);
	sc->hs_dev	= hv_dev;
	device_set_desc(dev, g_drv_props_table[stor_type].drv_desc);

	LIST_INIT(&sc->hs_free_list);
	mtx_init(&sc->hs_lock, "hvslck", NULL, MTX_DEF);

	for (i = 0; i < sc->hs_drv_props->drv_max_ios_per_target; ++i) {
		reqp = malloc(sizeof(struct hv_storvsc_request),
				 M_DEVBUF, M_WAITOK|M_ZERO);
		reqp->softc = sc;

		LIST_INSERT_HEAD(&sc->hs_free_list, reqp, link);
	}

	sc->hs_destroy = FALSE;
	sc->hs_drain_notify = FALSE;
	sema_init(&sc->hs_drain_sema, 0, "Store Drain Sema");

	ret = hv_storvsc_connect_vsp(hv_dev);
	if (ret != 0) {
		goto cleanup;
	}

	/*
	 * Create the device queue.
	 * Hyper-V maps each target to one SCSI HBA
	 */
	devq = cam_simq_alloc(sc->hs_drv_props->drv_max_ios_per_target);
	if (devq == NULL) {
		device_printf(dev, "Failed to alloc device queue\n");
		ret = ENOMEM;
		goto cleanup;
	}

	sc->hs_sim = cam_sim_alloc(storvsc_action,
				storvsc_poll,
				sc->hs_drv_props->drv_name,
				sc,
				sc->hs_unit,
				&sc->hs_lock, 1,
				sc->hs_drv_props->drv_max_ios_per_target,
				devq);

	if (sc->hs_sim == NULL) {
		device_printf(dev, "Failed to alloc sim\n");
		cam_simq_free(devq);
		ret = ENOMEM;
		goto cleanup;
	}

	mtx_lock(&sc->hs_lock);
	/* bus_id is set to 0, need to get it from VMBUS channel query? */
	if (xpt_bus_register(sc->hs_sim, dev, 0) != CAM_SUCCESS) {
		cam_sim_free(sc->hs_sim, /*free_devq*/TRUE);
		mtx_unlock(&sc->hs_lock);
		device_printf(dev, "Unable to register SCSI bus\n");
		ret = ENXIO;
		goto cleanup;
	}

	if (xpt_create_path(&sc->hs_path, /*periph*/NULL,
		 cam_sim_path(sc->hs_sim),
		CAM_TARGET_WILDCARD, CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
		xpt_bus_deregister(cam_sim_path(sc->hs_sim));
		cam_sim_free(sc->hs_sim, /*free_devq*/TRUE);
		mtx_unlock(&sc->hs_lock);
		device_printf(dev, "Unable to create path\n");
		ret = ENXIO;
		goto cleanup;
	}

	mtx_unlock(&sc->hs_lock);
	scan_for_luns(sc);
	for (i = 0; (hs_softc[i] != NULL) && (i < HS_MAX_ADAPTERS); i++);
	KASSERT(i < HS_MAX_ADAPTERS, ("storvsc_attach: hs_softc full\n"));
	hs_softc[i] = sc;

	root_mount_rel(root_mount_token);
	return (0);


cleanup:
	root_mount_rel(root_mount_token);
	while (!LIST_EMPTY(&sc->hs_free_list)) {
		reqp = LIST_FIRST(&sc->hs_free_list);
		LIST_REMOVE(reqp, link);
		free(reqp, M_DEVBUF);
	}
	return (ret);
}

/**
 * @brief StorVSC device detach function
 *
 * This function is responsible for safely detaching a
 * StorVSC device.  This includes waiting for inbound responses
 * to complete and freeing associated per-device structures.
 *
 * @param dev a device
 * returns 0 on success
 */
static int
storvsc_detach(device_t dev)
{
	struct storvsc_softc *sc = device_get_softc(dev);
	struct hv_storvsc_request *reqp = NULL;
	struct hv_device *hv_device = vmbus_get_devctx(dev);

	mtx_lock(&hv_device->channel->inbound_lock);
	sc->hs_destroy = TRUE;
	mtx_unlock(&hv_device->channel->inbound_lock);

	/*
	 * At this point, all outbound traffic should be disabled. We
	 * only allow inbound traffic (responses) to proceed so that
	 * outstanding requests can be completed.
	 */

	sc->hs_drain_notify = TRUE;
	sema_wait(&sc->hs_drain_sema);
	sc->hs_drain_notify = FALSE;

	/*
	 * Since we have already drained, we don't need to busy wait.
	 * The call to close the channel will reset the callback
	 * under the protection of the incoming channel lock.
	 */

	hv_vmbus_channel_close(hv_device->channel);

	mtx_lock(&sc->hs_lock);
	while (!LIST_EMPTY(&sc->hs_free_list)) {
		reqp = LIST_FIRST(&sc->hs_free_list);
		LIST_REMOVE(reqp, link);

		free(reqp, M_DEVBUF);
	}
	mtx_unlock(&sc->hs_lock);
	return (0);
}

#if HVS_TIMEOUT_TEST
/**
 * @brief unit test for timed out operations
 *
 * This function provides unit testing capability to simulate
 * timed out operations.  Recompilation with HV_TIMEOUT_TEST=1
 * is required.
 *
 * @param reqp pointer to a request structure
 * @param opcode SCSI operation being performed
 * @param wait if 1, wait for I/O to complete
 */
static void
storvsc_timeout_test(struct hv_storvsc_request *reqp,
		uint8_t opcode, int wait)
{
	int ret;
	union ccb *ccb = reqp->ccb;
	struct storvsc_softc *sc = reqp->softc;

	if (reqp->vstor_packet.vm_srb.cdb[0] != opcode) {
		return;
	}

	if (wait) {
		mtx_lock(&reqp->event.mtx);
	}
	ret = hv_storvsc_io_request(sc->hs_dev, reqp);
	if (ret != 0) {
		if (wait) {
			mtx_unlock(&reqp->event.mtx);
		}
		printf("%s: io_request failed with %d.\n",
				__func__, ret);
		ccb->ccb_h.status = CAM_PROVIDE_FAIL;
		mtx_lock(&sc->hs_lock);
		storvsc_free_request(sc, reqp);
		xpt_done(ccb);
		mtx_unlock(&sc->hs_lock);
		return;
	}

	if (wait) {
		xpt_print(ccb->ccb_h.path,
				"%u: %s: waiting for IO return.\n",
				ticks, __func__);
		ret = cv_timedwait(&reqp->event.cv, &reqp->event.mtx, 60*hz);
		mtx_unlock(&reqp->event.mtx);
		xpt_print(ccb->ccb_h.path, "%u: %s: %s.\n",
				ticks, __func__, (ret == 0)?
				"IO return detected" :
				"IO return not detected");
		/* 
		 * Now both the timer handler and io done are running
		 * simultaneously. We want to confirm the io done always
		 * finishes after the timer handler exits. So reqp used by
		 * timer handler is not freed or stale. Do busy loop for
		 * another 1/10 second to make sure io done does
		 * wait for the timer handler to complete.
		 */
		DELAY(100*1000);
		mtx_lock(&sc->hs_lock);
		xpt_print(ccb->ccb_h.path,
				"%u: %s: finishing, queue frozen %d, "
				"ccb status 0x%x scsi_status 0x%x.\n",
				ticks, __func__, sc->hs_frozen,
				ccb->ccb_h.status,
				ccb->csio.scsi_status);
		mtx_unlock(&sc->hs_lock);
	}
}
#endif /* HVS_TIMEOUT_TEST */

/**
 * @brief timeout handler for requests
 *
 * This function is called as a result of a callout expiring.
 *
 * @param arg pointer to a request
 */
static void
storvsc_timeout(void *arg)
{
	struct hv_storvsc_request *reqp = arg;
	struct storvsc_softc *sc = reqp->softc;
	union ccb *ccb = reqp->ccb;

	if (reqp->retries == 0) {
		mtx_lock(&sc->hs_lock);
		xpt_print(ccb->ccb_h.path,
		    "%u: IO timed out (req=0x%p), wait for another %u secs.\n",
		    ticks, reqp, ccb->ccb_h.timeout / 1000);
		cam_error_print(ccb, CAM_ESF_ALL, CAM_EPF_ALL);
		mtx_unlock(&sc->hs_lock);

		reqp->retries++;
		callout_reset(&reqp->callout,
				(ccb->ccb_h.timeout * hz) / 1000,
				storvsc_timeout, reqp);
#if HVS_TIMEOUT_TEST
		storvsc_timeout_test(reqp, SEND_DIAGNOSTIC, 0);
#endif
		return;
	}

	mtx_lock(&sc->hs_lock);
	xpt_print(ccb->ccb_h.path,
		"%u: IO (reqp = 0x%p) did not return for %u seconds, %s.\n",
		ticks, reqp, ccb->ccb_h.timeout * (reqp->retries+1) / 1000,
		(sc->hs_frozen == 0)?
		"freezing the queue" : "the queue is already frozen");
	if (sc->hs_frozen == 0) {
		sc->hs_frozen = 1;
		xpt_freeze_simq(xpt_path_sim(ccb->ccb_h.path), 1);
	}
	mtx_unlock(&sc->hs_lock);
	
#if HVS_TIMEOUT_TEST
	storvsc_timeout_test(reqp, MODE_SELECT_10, 1);
#endif
}

/**
 * @brief StorVSC device poll function
 *
 * This function is responsible for servicing requests when
 * interrupts are disabled (i.e when we are dumping core.)
 *
 * @param sim a pointer to a CAM SCSI interface module
 */
static void
storvsc_poll(struct cam_sim *sim)
{
	struct storvsc_softc *sc = cam_sim_softc(sim);

	mtx_assert(&sc->hs_lock, MA_OWNED);
	mtx_unlock(&sc->hs_lock);
	hv_storvsc_on_channel_callback(sc->hs_dev);
	mtx_lock(&sc->hs_lock);
}

/**
 * @brief StorVSC device action function
 *
 * This function is responsible for handling SCSI operations which
 * are passed from the CAM layer.  The requests are in the form of
 * CAM control blocks which indicate the action being performed.
 * Not all actions require converting the request to a VSCSI protocol
 * message - these actions can be responded to by this driver.
 * Requests which are destined for a backend storage device are converted
 * to a VSCSI protocol message and sent on the channel connection associated
 * with this device.
 *
 * @param sim pointer to a CAM SCSI interface module
 * @param ccb pointer to a CAM control block
 */
static void
storvsc_action(struct cam_sim *sim, union ccb *ccb)
{
	struct storvsc_softc *sc = cam_sim_softc(sim);
	int res;

	mtx_assert(&sc->hs_lock, MA_OWNED);
	switch (ccb->ccb_h.func_code) {
	case XPT_PATH_INQ: {
		struct ccb_pathinq *cpi = &ccb->cpi;

		cpi->version_num = 1;
		cpi->hba_inquiry = PI_TAG_ABLE|PI_SDTR_ABLE;
		cpi->target_sprt = 0;
		cpi->hba_misc = PIM_NOBUSRESET;
		cpi->hba_eng_cnt = 0;
		cpi->max_target = STORVSC_MAX_TARGETS;
		cpi->max_lun = sc->hs_drv_props->drv_max_luns_per_target;
		cpi->initiator_id = 0;
		cpi->bus_id = cam_sim_bus(sim);
		cpi->base_transfer_speed = 300000;
		cpi->transport = XPORT_SAS;
		cpi->transport_version = 0;
		cpi->protocol = PROTO_SCSI;
		cpi->protocol_version = SCSI_REV_SPC2;
		strncpy(cpi->sim_vid, "FreeBSD", SIM_IDLEN);
		strncpy(cpi->hba_vid, sc->hs_drv_props->drv_name, HBA_IDLEN);
		strncpy(cpi->dev_name, cam_sim_name(sim), DEV_IDLEN);
		cpi->unit_number = cam_sim_unit(sim);

		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		return;
	}
	case XPT_GET_TRAN_SETTINGS: {
		struct  ccb_trans_settings *cts = &ccb->cts;

		cts->transport = XPORT_SAS;
		cts->transport_version = 0;
		cts->protocol = PROTO_SCSI;
		cts->protocol_version = SCSI_REV_SPC2;

		/* enable tag queuing and disconnected mode */
		cts->proto_specific.valid = CTS_SCSI_VALID_TQ;
		cts->proto_specific.scsi.valid = CTS_SCSI_VALID_TQ;
		cts->proto_specific.scsi.flags = CTS_SCSI_FLAGS_TAG_ENB;
		cts->xport_specific.valid = CTS_SPI_VALID_DISC;
		cts->xport_specific.spi.flags = CTS_SPI_FLAGS_DISC_ENB;
			
		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		return;
	}
	case XPT_SET_TRAN_SETTINGS:	{
		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		return;
	}
	case XPT_CALC_GEOMETRY:{
		cam_calc_geometry(&ccb->ccg, 1);
		xpt_done(ccb);
		return;
	}
	case  XPT_RESET_BUS:
	case  XPT_RESET_DEV:{
#if HVS_HOST_RESET
		if ((res = hv_storvsc_host_reset(sc->hs_dev)) != 0) {
			xpt_print(ccb->ccb_h.path,
				"hv_storvsc_host_reset failed with %d\n", res);
			ccb->ccb_h.status = CAM_PROVIDE_FAIL;
			xpt_done(ccb);
			return;
		}
		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		return;
#else
		xpt_print(ccb->ccb_h.path,
				  "%s reset not supported.\n",
				  (ccb->ccb_h.func_code == XPT_RESET_BUS)?
				  "bus" : "dev");
		ccb->ccb_h.status = CAM_REQ_INVALID;
		xpt_done(ccb);
		return;
#endif	/* HVS_HOST_RESET */
	}
	case XPT_SCSI_IO:
	case XPT_IMMED_NOTIFY: {
		struct hv_storvsc_request *reqp = NULL;

		if (ccb->csio.cdb_len == 0) {
			panic("cdl_len is 0\n");
		}

		if (LIST_EMPTY(&sc->hs_free_list)) {
			ccb->ccb_h.status = CAM_REQUEUE_REQ;
			if (sc->hs_frozen == 0) {
				sc->hs_frozen = 1;
				xpt_freeze_simq(sim, /* count*/1);
			}
			xpt_done(ccb);
			return;
		}

		reqp = LIST_FIRST(&sc->hs_free_list);
		LIST_REMOVE(reqp, link);

		bzero(reqp, sizeof(struct hv_storvsc_request));
		reqp->softc = sc;

		ccb->ccb_h.status |= CAM_SIM_QUEUED;	    
		create_storvsc_request(ccb, reqp);

		if (ccb->ccb_h.timeout != CAM_TIME_INFINITY) {
			callout_init(&reqp->callout, CALLOUT_MPSAFE);
			callout_reset(&reqp->callout,
					(ccb->ccb_h.timeout * hz) / 1000,
					storvsc_timeout, reqp);
#if HVS_TIMEOUT_TEST
			cv_init(&reqp->event.cv, "storvsc timeout cv");
			mtx_init(&reqp->event.mtx, "storvsc timeout mutex",
					NULL, MTX_DEF);
			switch (reqp->vstor_packet.vm_srb.cdb[0]) {
				case MODE_SELECT_10:
				case SEND_DIAGNOSTIC:
					/* To have timer send the request. */
					return;
				default:
					break;
			}
#endif /* HVS_TIMEOUT_TEST */
		}

		if ((res = hv_storvsc_io_request(sc->hs_dev, reqp)) != 0) {
			xpt_print(ccb->ccb_h.path,
				"hv_storvsc_io_request failed with %d\n", res);
			ccb->ccb_h.status = CAM_PROVIDE_FAIL;
			storvsc_free_request(sc, reqp);
			xpt_done(ccb);
			return;
		}
		return;
	}

	default:
		ccb->ccb_h.status = CAM_REQ_INVALID;
		xpt_done(ccb);
		return;
	}
}

/**
 * @brief Fill in a request structure based on a CAM control block
 *
 * Fills in a request structure based on the contents of a CAM control
 * block.  The request structure holds the payload information for
 * VSCSI protocol request.
 *
 * @param ccb pointer to a CAM contorl block
 * @param reqp pointer to a request structure
 */
static void
create_storvsc_request(union ccb *ccb, struct hv_storvsc_request *reqp)
{
	struct ccb_scsiio *csio = &ccb->csio;
	uint64_t phys_addr;
	uint32_t bytes_to_copy = 0;
	uint32_t pfn_num = 0;
	uint32_t pfn;
	
	/* refer to struct vmscsi_req for meanings of these two fields */
	reqp->vstor_packet.u.vm_srb.port =
		cam_sim_unit(xpt_path_sim(ccb->ccb_h.path));
	reqp->vstor_packet.u.vm_srb.path_id =
		cam_sim_bus(xpt_path_sim(ccb->ccb_h.path));

	reqp->vstor_packet.u.vm_srb.target_id = ccb->ccb_h.target_id;
	reqp->vstor_packet.u.vm_srb.lun = ccb->ccb_h.target_lun;

	reqp->vstor_packet.u.vm_srb.cdb_len = csio->cdb_len;
	if(ccb->ccb_h.flags & CAM_CDB_POINTER) {
		memcpy(&reqp->vstor_packet.u.vm_srb.u.cdb, csio->cdb_io.cdb_ptr,
			csio->cdb_len);
	} else {
		memcpy(&reqp->vstor_packet.u.vm_srb.u.cdb, csio->cdb_io.cdb_bytes,
			csio->cdb_len);
	}

	switch (ccb->ccb_h.flags & CAM_DIR_MASK) {
    	case CAM_DIR_OUT: 
    		reqp->vstor_packet.u.vm_srb.data_in = WRITE_TYPE;
    		break;
    	case CAM_DIR_IN:
    		reqp->vstor_packet.u.vm_srb.data_in = READ_TYPE;
    		break;
    	case CAM_DIR_NONE:
    		reqp->vstor_packet.u.vm_srb.data_in = UNKNOWN_TYPE;
    		break;
    	default:
    		reqp->vstor_packet.u.vm_srb.data_in = UNKNOWN_TYPE;
    		break;
	}

	reqp->sense_data     = &csio->sense_data;
	reqp->sense_info_len = csio->sense_len;

	reqp->ccb = ccb;
	/*
	KASSERT((ccb->ccb_h.flags & CAM_SCATTER_VALID) == 0,
			("ccb is scatter gather valid\n"));
	*/
	if (csio->dxfer_len != 0) {
		reqp->data_buf.length = csio->dxfer_len;
		bytes_to_copy = csio->dxfer_len;
		phys_addr = vtophys(csio->data_ptr);
		reqp->data_buf.offset = phys_addr - trunc_page(phys_addr);
	}

	while (bytes_to_copy != 0) {
		int bytes, page_offset;
		phys_addr = vtophys(&csio->data_ptr[reqp->data_buf.length -
		                                    bytes_to_copy]);
		pfn = phys_addr >> PAGE_SHIFT;
		reqp->data_buf.pfn_array[pfn_num] = pfn;
		page_offset = phys_addr - trunc_page(phys_addr);

		bytes = min(PAGE_SIZE - page_offset, bytes_to_copy);

		bytes_to_copy -= bytes;
		pfn_num++;
	}
}

/**
 * @brief completion function before returning to CAM
 *
 * I/O process has been completed and the result needs
 * to be passed to the CAM layer.
 * Free resources related to this request.
 *
 * @param reqp pointer to a request structure
 */
static void
storvsc_io_done(struct hv_storvsc_request *reqp)
{
	union ccb *ccb = reqp->ccb;
	struct ccb_scsiio *csio = &ccb->csio;
	struct storvsc_softc *sc = reqp->softc;
	struct vmscsi_req *vm_srb = &reqp->vstor_packet.u.vm_srb;
	
	if (reqp->retries > 0) {
		mtx_lock(&sc->hs_lock);
#if HVS_TIMEOUT_TEST
		xpt_print(ccb->ccb_h.path,
			"%u: IO returned after timeout, "
			"waking up timer handler if any.\n", ticks);
		mtx_lock(&reqp->event.mtx);
		cv_signal(&reqp->event.cv);
		mtx_unlock(&reqp->event.mtx);
#endif
		reqp->retries = 0;
		xpt_print(ccb->ccb_h.path,
			"%u: IO returned after timeout, "
			"stopping timer if any.\n", ticks);
		mtx_unlock(&sc->hs_lock);
	}

	/* 
	 * callout_drain() will wait for the timer handler to finish
	 * if it is running. So we don't need any lock to synchronize
	 * between this routine and the timer handler.
	 * Note that we need to make sure reqp is not freed when timer
	 * handler is using or will use it.
	 */
	if (ccb->ccb_h.timeout != CAM_TIME_INFINITY) {
		callout_drain(&reqp->callout);
	}

	ccb->ccb_h.status &= ~CAM_SIM_QUEUED;
	ccb->ccb_h.status &= ~CAM_STATUS_MASK;
	if (vm_srb->scsi_status == SCSI_STATUS_OK) {
		ccb->ccb_h.status |= CAM_REQ_CMP;
	 } else {
		mtx_lock(&sc->hs_lock);
		xpt_print(ccb->ccb_h.path,
			"srovsc scsi_status = %d\n",
			vm_srb->scsi_status);
		mtx_unlock(&sc->hs_lock);
		ccb->ccb_h.status |= CAM_SCSI_STATUS_ERROR;
	}

	ccb->csio.scsi_status = (vm_srb->scsi_status & 0xFF);
	ccb->csio.resid = ccb->csio.dxfer_len - vm_srb->transfer_len;

	if (reqp->sense_info_len != 0) {
		csio->sense_resid = csio->sense_len - reqp->sense_info_len;
		ccb->ccb_h.status |= CAM_AUTOSNS_VALID;
	}

	mtx_lock(&sc->hs_lock);
	if (reqp->softc->hs_frozen == 1) {
		xpt_print(ccb->ccb_h.path,
			"%u: storvsc unfreezing softc 0x%p.\n",
			ticks, reqp->softc);
		ccb->ccb_h.status |= CAM_RELEASE_SIMQ;
		reqp->softc->hs_frozen = 0;
	}
	storvsc_free_request(sc, reqp);
	xpt_done(ccb);
	mtx_unlock(&sc->hs_lock);
}

/**
 * @brief Free a request structure
 *
 * Free a request structure by returning it to the free list
 *
 * @param sc pointer to a softc
 * @param reqp pointer to a request structure
 */	
static void
storvsc_free_request(struct storvsc_softc *sc, struct hv_storvsc_request *reqp)
{

	LIST_INSERT_HEAD(&sc->hs_free_list, reqp, link);
}

/**
 * @brief Determine type of storage device from GUID
 *
 * Using the type GUID, determine if this is a StorVSC (paravirtual
 * SCSI or BlkVSC (paravirtual IDE) device.
 *
 * @param dev a device
 * returns an enum
 */
static enum hv_storage_type
storvsc_get_storage_type(device_t dev)
{
	const char *p = vmbus_get_type(dev);

	if (!memcmp(p, &gBlkVscDeviceType, sizeof(hv_guid))) {
		return DRIVER_BLKVSC;
	} else if (!memcmp(p, &gStorVscDeviceType, sizeof(hv_guid))) {
		return DRIVER_STORVSC;
	}
	return (DRIVER_UNKNOWN);
}

