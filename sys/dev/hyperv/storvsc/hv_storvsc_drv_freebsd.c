/*-
 * Copyright (c) 2009-2012,2016 Microsoft Corp.
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
#include <sys/time.h>
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
#include <vm/uma.h>
#include <sys/lock.h>
#include <sys/sema.h>
#include <sys/sglist.h>
#include <machine/bus.h>
#include <sys/bus_dma.h>

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
#include "vmbus_if.h"

#define STORVSC_RINGBUFFER_SIZE		(20*PAGE_SIZE)
#define STORVSC_MAX_LUNS_PER_TARGET	(64)
#define STORVSC_MAX_IO_REQUESTS		(STORVSC_MAX_LUNS_PER_TARGET * 2)
#define BLKVSC_MAX_IDE_DISKS_PER_TARGET	(1)
#define BLKVSC_MAX_IO_REQUESTS		STORVSC_MAX_IO_REQUESTS
#define STORVSC_MAX_TARGETS		(2)

#define VSTOR_PKT_SIZE	(sizeof(struct vstor_packet) - vmscsi_size_delta)

#define HV_ALIGN(x, a) roundup2(x, a)

struct storvsc_softc;

struct hv_sgl_node {
	LIST_ENTRY(hv_sgl_node) link;
	struct sglist *sgl_data;
};

struct hv_sgl_page_pool{
	LIST_HEAD(, hv_sgl_node) in_use_sgl_list;
	LIST_HEAD(, hv_sgl_node) free_sgl_list;
	boolean_t                is_init;
} g_hv_sgl_page_pool;

#define STORVSC_MAX_SG_PAGE_CNT STORVSC_MAX_IO_REQUESTS * HV_MAX_MULTIPAGE_BUFFER_COUNT

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
	struct sglist *bounce_sgl;
	unsigned int bounce_sgl_count;
	uint64_t not_aligned_seg_bits;
};

struct storvsc_softc {
	struct hv_vmbus_channel		*hs_chan;
	LIST_HEAD(, hv_storvsc_request)	hs_free_list;
	struct mtx			hs_lock;
	struct storvsc_driver_props	*hs_drv_props;
	int 				hs_unit;
	uint32_t			hs_frozen;
	struct cam_sim			*hs_sim;
	struct cam_path 		*hs_path;
	uint32_t			hs_num_out_reqs;
	boolean_t			hs_destroy;
	boolean_t			hs_drain_notify;
	struct sema 			hs_drain_sema;	
	struct hv_storvsc_request	hs_init_req;
	struct hv_storvsc_request	hs_reset_req;
	device_t			hs_dev;
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

#define HV_STORAGE_SUPPORTS_MULTI_CHANNEL 0x1

/* {ba6163d9-04a1-4d29-b605-72e2ffb1dc7f} */
static const struct hyperv_guid gStorVscDeviceType={
	.hv_guid = {0xd9, 0x63, 0x61, 0xba, 0xa1, 0x04, 0x29, 0x4d,
		 0xb6, 0x05, 0x72, 0xe2, 0xff, 0xb1, 0xdc, 0x7f}
};

/* {32412632-86cb-44a2-9b5c-50d1417354f5} */
static const struct hyperv_guid gBlkVscDeviceType={
	.hv_guid = {0x32, 0x26, 0x41, 0x32, 0xcb, 0x86, 0xa2, 0x44,
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

/*
 * Sense buffer size changed in win8; have a run-time
 * variable to track the size we should use.
 */
static int sense_buffer_size = PRE_WIN8_STORVSC_SENSE_BUFFER_SIZE;

/*
 * The size of the vmscsi_request has changed in win8. The
 * additional size is for the newly added elements in the
 * structure. These elements are valid only when we are talking
 * to a win8 host.
 * Track the correct size we need to apply.
 */
static int vmscsi_size_delta;
/*
 * The storage protocol version is determined during the
 * initial exchange with the host.  It will indicate which
 * storage functionality is available in the host.
*/
static int vmstor_proto_version;

struct vmstor_proto {
        int proto_version;
        int sense_buffer_size;
        int vmscsi_size_delta;
};

static const struct vmstor_proto vmstor_proto_list[] = {
        {
                VMSTOR_PROTOCOL_VERSION_WIN10,
                POST_WIN7_STORVSC_SENSE_BUFFER_SIZE,
                0
        },
        {
                VMSTOR_PROTOCOL_VERSION_WIN8_1,
                POST_WIN7_STORVSC_SENSE_BUFFER_SIZE,
                0
        },
        {
                VMSTOR_PROTOCOL_VERSION_WIN8,
                POST_WIN7_STORVSC_SENSE_BUFFER_SIZE,
                0
        },
        {
                VMSTOR_PROTOCOL_VERSION_WIN7,
                PRE_WIN8_STORVSC_SENSE_BUFFER_SIZE,
                sizeof(struct vmscsi_win8_extension),
        },
        {
                VMSTOR_PROTOCOL_VERSION_WIN6,
                PRE_WIN8_STORVSC_SENSE_BUFFER_SIZE,
                sizeof(struct vmscsi_win8_extension),
        }
};

/* static functions */
static int storvsc_probe(device_t dev);
static int storvsc_attach(device_t dev);
static int storvsc_detach(device_t dev);
static void storvsc_poll(struct cam_sim * sim);
static void storvsc_action(struct cam_sim * sim, union ccb * ccb);
static int create_storvsc_request(union ccb *ccb, struct hv_storvsc_request *reqp);
static void storvsc_free_request(struct storvsc_softc *sc, struct hv_storvsc_request *reqp);
static enum hv_storage_type storvsc_get_storage_type(device_t dev);
static void hv_storvsc_rescan_target(struct storvsc_softc *sc);
static void hv_storvsc_on_channel_callback(void *xchan);
static void hv_storvsc_on_iocompletion( struct storvsc_softc *sc,
					struct vstor_packet *vstor_packet,
					struct hv_storvsc_request *request);
static int hv_storvsc_connect_vsp(struct storvsc_softc *);
static void storvsc_io_done(struct hv_storvsc_request *reqp);
static void storvsc_copy_sgl_to_bounce_buf(struct sglist *bounce_sgl,
				bus_dma_segment_t *orig_sgl,
				unsigned int orig_sgl_count,
				uint64_t seg_bits);
void storvsc_copy_from_bounce_buf_to_sgl(bus_dma_segment_t *dest_sgl,
				unsigned int dest_sgl_count,
				struct sglist* src_sgl,
				uint64_t seg_bits);

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

static void
storvsc_subchan_attach(struct storvsc_softc *sc,
    struct hv_vmbus_channel *new_channel)
{
	struct vmstor_chan_props props;
	int ret = 0;

	memset(&props, 0, sizeof(props));

	new_channel->hv_chan_priv1 = sc;
	vmbus_channel_cpu_rr(new_channel);
	ret = hv_vmbus_channel_open(new_channel,
	    sc->hs_drv_props->drv_ringbuffer_size,
  	    sc->hs_drv_props->drv_ringbuffer_size,
	    (void *)&props,
	    sizeof(struct vmstor_chan_props),
	    hv_storvsc_on_channel_callback,
	    new_channel);
}

/**
 * @brief Send multi-channel creation request to host
 *
 * @param device  a Hyper-V device pointer
 * @param max_chans  the max channels supported by vmbus
 */
static void
storvsc_send_multichannel_request(struct storvsc_softc *sc, int max_chans)
{
	struct hv_vmbus_channel **subchan;
	struct hv_storvsc_request *request;
	struct vstor_packet *vstor_packet;	
	int request_channels_cnt = 0;
	int ret, i;

	/* get multichannels count that need to create */
	request_channels_cnt = MIN(max_chans, mp_ncpus);

	request = &sc->hs_init_req;

	/* request the host to create multi-channel */
	memset(request, 0, sizeof(struct hv_storvsc_request));
	
	sema_init(&request->synch_sema, 0, ("stor_synch_sema"));

	vstor_packet = &request->vstor_packet;
	
	vstor_packet->operation = VSTOR_OPERATION_CREATE_MULTI_CHANNELS;
	vstor_packet->flags = REQUEST_COMPLETION_FLAG;
	vstor_packet->u.multi_channels_cnt = request_channels_cnt;

	ret = hv_vmbus_channel_send_packet(
	    sc->hs_chan,
	    vstor_packet,
	    VSTOR_PKT_SIZE,
	    (uint64_t)(uintptr_t)request,
	    HV_VMBUS_PACKET_TYPE_DATA_IN_BAND,
	    HV_VMBUS_DATA_PACKET_FLAG_COMPLETION_REQUESTED);

	/* wait for 5 seconds */
	ret = sema_timedwait(&request->synch_sema, 5 * hz);
	if (ret != 0) {		
		printf("Storvsc_error: create multi-channel timeout, %d\n",
		    ret);
		return;
	}

	if (vstor_packet->operation != VSTOR_OPERATION_COMPLETEIO ||
	    vstor_packet->status != 0) {		
		printf("Storvsc_error: create multi-channel invalid operation "
		    "(%d) or statue (%u)\n",
		    vstor_packet->operation, vstor_packet->status);
		return;
	}

	/* Wait for sub-channels setup to complete. */
	subchan = vmbus_get_subchan(sc->hs_chan, request_channels_cnt);

	/* Attach the sub-channels. */
	for (i = 0; i < request_channels_cnt; ++i)
		storvsc_subchan_attach(sc, subchan[i]);

	/* Release the sub-channels. */
	vmbus_rel_subchan(subchan, request_channels_cnt);

	if (bootverbose)
		printf("Storvsc create multi-channel success!\n");
}

/**
 * @brief initialize channel connection to parent partition
 *
 * @param dev  a Hyper-V device pointer
 * @returns  0 on success, non-zero error on failure
 */
static int
hv_storvsc_channel_init(struct storvsc_softc *sc)
{
	int ret = 0, i;
	struct hv_storvsc_request *request;
	struct vstor_packet *vstor_packet;
	uint16_t max_chans = 0;
	boolean_t support_multichannel = FALSE;
	uint32_t version;

	max_chans = 0;
	support_multichannel = FALSE;

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
			sc->hs_chan,
			vstor_packet,
			VSTOR_PKT_SIZE,
			(uint64_t)(uintptr_t)request,
			HV_VMBUS_PACKET_TYPE_DATA_IN_BAND,
			HV_VMBUS_DATA_PACKET_FLAG_COMPLETION_REQUESTED);

	if (ret != 0)
		goto cleanup;

	/* wait 5 seconds */
	ret = sema_timedwait(&request->synch_sema, 5 * hz);
	if (ret != 0)
		goto cleanup;

	if (vstor_packet->operation != VSTOR_OPERATION_COMPLETEIO ||
		vstor_packet->status != 0) {
		goto cleanup;
	}

	for (i = 0; i < nitems(vmstor_proto_list); i++) {
		/* reuse the packet for version range supported */

		memset(vstor_packet, 0, sizeof(struct vstor_packet));
		vstor_packet->operation = VSTOR_OPERATION_QUERYPROTOCOLVERSION;
		vstor_packet->flags = REQUEST_COMPLETION_FLAG;

		vstor_packet->u.version.major_minor =
			vmstor_proto_list[i].proto_version;

		/* revision is only significant for Windows guests */
		vstor_packet->u.version.revision = 0;

		ret = hv_vmbus_channel_send_packet(
			sc->hs_chan,
			vstor_packet,
			VSTOR_PKT_SIZE,
			(uint64_t)(uintptr_t)request,
			HV_VMBUS_PACKET_TYPE_DATA_IN_BAND,
			HV_VMBUS_DATA_PACKET_FLAG_COMPLETION_REQUESTED);

		if (ret != 0)
			goto cleanup;

		/* wait 5 seconds */
		ret = sema_timedwait(&request->synch_sema, 5 * hz);

		if (ret)
			goto cleanup;

		if (vstor_packet->operation != VSTOR_OPERATION_COMPLETEIO) {
			ret = EINVAL;
			goto cleanup;	
		}
		if (vstor_packet->status == 0) {
			vmstor_proto_version =
				vmstor_proto_list[i].proto_version;
			sense_buffer_size =
				vmstor_proto_list[i].sense_buffer_size;
			vmscsi_size_delta =
				vmstor_proto_list[i].vmscsi_size_delta;
			break;
		}
	}

	if (vstor_packet->status != 0) {
		ret = EINVAL;
		goto cleanup;
	}
	/**
	 * Query channel properties
	 */
	memset(vstor_packet, 0, sizeof(struct vstor_packet));
	vstor_packet->operation = VSTOR_OPERATION_QUERYPROPERTIES;
	vstor_packet->flags = REQUEST_COMPLETION_FLAG;

	ret = hv_vmbus_channel_send_packet(
				sc->hs_chan,
				vstor_packet,
				VSTOR_PKT_SIZE,
				(uint64_t)(uintptr_t)request,
				HV_VMBUS_PACKET_TYPE_DATA_IN_BAND,
				HV_VMBUS_DATA_PACKET_FLAG_COMPLETION_REQUESTED);

	if ( ret != 0)
		goto cleanup;

	/* wait 5 seconds */
	ret = sema_timedwait(&request->synch_sema, 5 * hz);

	if (ret != 0)
		goto cleanup;

	/* TODO: Check returned version */
	if (vstor_packet->operation != VSTOR_OPERATION_COMPLETEIO ||
	    vstor_packet->status != 0) {
		goto cleanup;
	}

	/* multi-channels feature is supported by WIN8 and above version */
	max_chans = vstor_packet->u.chan_props.max_channel_cnt;
	version = VMBUS_GET_VERSION(device_get_parent(sc->hs_dev), sc->hs_dev);
	if (version != VMBUS_VERSION_WIN7 && version != VMBUS_VERSION_WS2008 &&
	    (vstor_packet->u.chan_props.flags &
	     HV_STORAGE_SUPPORTS_MULTI_CHANNEL)) {
		support_multichannel = TRUE;
	}

	memset(vstor_packet, 0, sizeof(struct vstor_packet));
	vstor_packet->operation = VSTOR_OPERATION_ENDINITIALIZATION;
	vstor_packet->flags = REQUEST_COMPLETION_FLAG;

	ret = hv_vmbus_channel_send_packet(
			sc->hs_chan,
			vstor_packet,
			VSTOR_PKT_SIZE,
			(uint64_t)(uintptr_t)request,
			HV_VMBUS_PACKET_TYPE_DATA_IN_BAND,
			HV_VMBUS_DATA_PACKET_FLAG_COMPLETION_REQUESTED);

	if (ret != 0) {
		goto cleanup;
	}

	/* wait 5 seconds */
	ret = sema_timedwait(&request->synch_sema, 5 * hz);

	if (ret != 0)
		goto cleanup;

	if (vstor_packet->operation != VSTOR_OPERATION_COMPLETEIO ||
	    vstor_packet->status != 0)
		goto cleanup;

	/*
	 * If multi-channel is supported, send multichannel create
	 * request to host.
	 */
	if (support_multichannel)
		storvsc_send_multichannel_request(sc, max_chans);

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
hv_storvsc_connect_vsp(struct storvsc_softc *sc)
{	
	int ret = 0;
	struct vmstor_chan_props props;

	memset(&props, 0, sizeof(struct vmstor_chan_props));

	/*
	 * Open the channel
	 */
	KASSERT(sc->hs_chan->hv_chan_priv1 == sc, ("invalid chan priv1"));
	vmbus_channel_cpu_rr(sc->hs_chan);
	ret = hv_vmbus_channel_open(
		sc->hs_chan,
		sc->hs_drv_props->drv_ringbuffer_size,
		sc->hs_drv_props->drv_ringbuffer_size,
		(void *)&props,
		sizeof(struct vmstor_chan_props),
		hv_storvsc_on_channel_callback,
		sc->hs_chan);

	if (ret != 0) {
		return ret;
	}

	ret = hv_storvsc_channel_init(sc);

	return (ret);
}

#if HVS_HOST_RESET
static int
hv_storvsc_host_reset(struct storvsc_softc *sc)
{
	int ret = 0;

	struct hv_storvsc_request *request;
	struct vstor_packet *vstor_packet;

	request = &sc->hs_reset_req;
	request->softc = sc;
	vstor_packet = &request->vstor_packet;

	sema_init(&request->synch_sema, 0, "stor synch sema");

	vstor_packet->operation = VSTOR_OPERATION_RESETBUS;
	vstor_packet->flags = REQUEST_COMPLETION_FLAG;

	ret = hv_vmbus_channel_send_packet(dev->channel,
			vstor_packet,
			VSTOR_PKT_SIZE,
			(uint64_t)(uintptr_t)&sc->hs_reset_req,
			HV_VMBUS_PACKET_TYPE_DATA_IN_BAND,
			HV_VMBUS_DATA_PACKET_FLAG_COMPLETION_REQUESTED);

	if (ret != 0) {
		goto cleanup;
	}

	ret = sema_timedwait(&request->synch_sema, 5 * hz); /* KYS 5 seconds */

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
hv_storvsc_io_request(struct storvsc_softc *sc,
					  struct hv_storvsc_request *request)
{
	struct vstor_packet *vstor_packet = &request->vstor_packet;
	struct hv_vmbus_channel* outgoing_channel = NULL;
	int ret = 0;

	vstor_packet->flags |= REQUEST_COMPLETION_FLAG;

	vstor_packet->u.vm_srb.length = VSTOR_PKT_SIZE;
	
	vstor_packet->u.vm_srb.sense_info_len = sense_buffer_size;

	vstor_packet->u.vm_srb.transfer_len = request->data_buf.length;

	vstor_packet->operation = VSTOR_OPERATION_EXECUTESRB;

	outgoing_channel = vmbus_select_outgoing_channel(sc->hs_chan);

	mtx_unlock(&request->softc->hs_lock);
	if (request->data_buf.length) {
		ret = hv_vmbus_channel_send_packet_multipagebuffer(
				outgoing_channel,
				&request->data_buf,
				vstor_packet,
				VSTOR_PKT_SIZE,
				(uint64_t)(uintptr_t)request);

	} else {
		ret = hv_vmbus_channel_send_packet(
			outgoing_channel,
			vstor_packet,
			VSTOR_PKT_SIZE,
			(uint64_t)(uintptr_t)request,
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

	/*
	 * Copy some fields of the host's response into the request structure,
	 * because the fields will be used later in storvsc_io_done().
	 */
	request->vstor_packet.u.vm_srb.scsi_status = vm_srb->scsi_status;
	request->vstor_packet.u.vm_srb.transfer_len = vm_srb->transfer_len;

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
hv_storvsc_rescan_target(struct storvsc_softc *sc)
{
	path_id_t pathid;
	target_id_t targetid;
	union ccb *ccb;

	pathid = cam_sim_path(sc->hs_sim);
	targetid = CAM_TARGET_WILDCARD;

	/*
	 * Allocate a CCB and schedule a rescan.
	 */
	ccb = xpt_alloc_ccb_nowait();
	if (ccb == NULL) {
		printf("unable to alloc CCB for rescan\n");
		return;
	}

	if (xpt_create_path(&ccb->ccb_h.path, NULL, pathid, targetid,
	    CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
		printf("unable to create path for rescan, pathid: %u,"
		    "targetid: %u\n", pathid, targetid);
		xpt_free_ccb(ccb);
		return;
	}

	if (targetid == CAM_TARGET_WILDCARD)
		ccb->ccb_h.func_code = XPT_SCAN_BUS;
	else
		ccb->ccb_h.func_code = XPT_SCAN_TGT;

	xpt_rescan(ccb);
}

static void
hv_storvsc_on_channel_callback(void *xchan)
{
	int ret = 0;
	hv_vmbus_channel *channel = xchan;
	struct storvsc_softc *sc = channel->hv_chan_priv1;
	uint32_t bytes_recvd;
	uint64_t request_id;
	uint8_t packet[roundup2(sizeof(struct vstor_packet), 8)];
	struct hv_storvsc_request *request;
	struct vstor_packet *vstor_packet;

	ret = hv_vmbus_channel_recv_packet(
			channel,
			packet,
			roundup2(VSTOR_PKT_SIZE, 8),
			&bytes_recvd,
			&request_id);

	while ((ret == 0) && (bytes_recvd > 0)) {
		request = (struct hv_storvsc_request *)(uintptr_t)request_id;

		if ((request == &sc->hs_init_req) ||
			(request == &sc->hs_reset_req)) {
			memcpy(&request->vstor_packet, packet,
				   sizeof(struct vstor_packet));
			sema_post(&request->synch_sema);
		} else {
			vstor_packet = (struct vstor_packet *)packet;
			switch(vstor_packet->operation) {
			case VSTOR_OPERATION_COMPLETEIO:
				if (request == NULL)
					panic("VMBUS: storvsc received a "
					    "packet with NULL request id in "
					    "COMPLETEIO operation.");

				hv_storvsc_on_iocompletion(sc,
							vstor_packet, request);
				break;
			case VSTOR_OPERATION_REMOVEDEVICE:
				printf("VMBUS: storvsc operation %d not "
				    "implemented.\n", vstor_packet->operation);
				/* TODO: implement */
				break;
			case VSTOR_OPERATION_ENUMERATE_BUS:
				hv_storvsc_rescan_target(sc);
				break;
			default:
				break;
			}			
		}
		ret = hv_vmbus_channel_recv_packet(
				channel,
				packet,
				roundup2(VSTOR_PKT_SIZE, 8),
				&bytes_recvd,
				&request_id);
	}
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
			device_set_desc(dev, g_drv_props_table[DRIVER_BLKVSC].drv_desc);
			ret = BUS_PROBE_DEFAULT;
		} else if(bootverbose)
			device_printf(dev, "Emulated ATA/IDE set (hw.ata.disk_enable set)\n");
		break;
	case DRIVER_STORVSC:
		if(bootverbose)
			device_printf(dev, "Enlightened SCSI device detected\n");
		device_set_desc(dev, g_drv_props_table[DRIVER_STORVSC].drv_desc);
		ret = BUS_PROBE_DEFAULT;
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
	enum hv_storage_type stor_type;
	struct storvsc_softc *sc;
	struct cam_devq *devq;
	int ret, i, j;
	struct hv_storvsc_request *reqp;
	struct root_hold_token *root_mount_token = NULL;
	struct hv_sgl_node *sgl_node = NULL;
	void *tmp_buff = NULL;

	/*
	 * We need to serialize storvsc attach calls.
	 */
	root_mount_token = root_mount_hold("storvsc");

	sc = device_get_softc(dev);
	sc->hs_chan = vmbus_get_channel(dev);
	sc->hs_chan->hv_chan_priv1 = sc;

	stor_type = storvsc_get_storage_type(dev);

	if (stor_type == DRIVER_UNKNOWN) {
		ret = ENODEV;
		goto cleanup;
	}

	/* fill in driver specific properties */
	sc->hs_drv_props = &g_drv_props_table[stor_type];

	/* fill in device specific properties */
	sc->hs_unit	= device_get_unit(dev);
	sc->hs_dev	= dev;

	LIST_INIT(&sc->hs_free_list);
	mtx_init(&sc->hs_lock, "hvslck", NULL, MTX_DEF);

	for (i = 0; i < sc->hs_drv_props->drv_max_ios_per_target; ++i) {
		reqp = malloc(sizeof(struct hv_storvsc_request),
				 M_DEVBUF, M_WAITOK|M_ZERO);
		reqp->softc = sc;

		LIST_INSERT_HEAD(&sc->hs_free_list, reqp, link);
	}

	/* create sg-list page pool */
	if (FALSE == g_hv_sgl_page_pool.is_init) {
		g_hv_sgl_page_pool.is_init = TRUE;
		LIST_INIT(&g_hv_sgl_page_pool.in_use_sgl_list);
		LIST_INIT(&g_hv_sgl_page_pool.free_sgl_list);

		/*
		 * Pre-create SG list, each SG list with
		 * HV_MAX_MULTIPAGE_BUFFER_COUNT segments, each
		 * segment has one page buffer
		 */
		for (i = 0; i < STORVSC_MAX_IO_REQUESTS; i++) {
	        	sgl_node = malloc(sizeof(struct hv_sgl_node),
			    M_DEVBUF, M_WAITOK|M_ZERO);

			sgl_node->sgl_data =
			    sglist_alloc(HV_MAX_MULTIPAGE_BUFFER_COUNT,
			    M_WAITOK|M_ZERO);

			for (j = 0; j < HV_MAX_MULTIPAGE_BUFFER_COUNT; j++) {
				tmp_buff = malloc(PAGE_SIZE,
				    M_DEVBUF, M_WAITOK|M_ZERO);

				sgl_node->sgl_data->sg_segs[j].ss_paddr =
				    (vm_paddr_t)tmp_buff;
			}

			LIST_INSERT_HEAD(&g_hv_sgl_page_pool.free_sgl_list,
			    sgl_node, link);
		}
	}

	sc->hs_destroy = FALSE;
	sc->hs_drain_notify = FALSE;
	sema_init(&sc->hs_drain_sema, 0, "Store Drain Sema");

	ret = hv_storvsc_connect_vsp(sc);
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

	root_mount_rel(root_mount_token);
	return (0);


cleanup:
	root_mount_rel(root_mount_token);
	while (!LIST_EMPTY(&sc->hs_free_list)) {
		reqp = LIST_FIRST(&sc->hs_free_list);
		LIST_REMOVE(reqp, link);
		free(reqp, M_DEVBUF);
	}

	while (!LIST_EMPTY(&g_hv_sgl_page_pool.free_sgl_list)) {
		sgl_node = LIST_FIRST(&g_hv_sgl_page_pool.free_sgl_list);
		LIST_REMOVE(sgl_node, link);
		for (j = 0; j < HV_MAX_MULTIPAGE_BUFFER_COUNT; j++) {
			if (NULL !=
			    (void*)sgl_node->sgl_data->sg_segs[j].ss_paddr) {
				free((void*)sgl_node->sgl_data->sg_segs[j].ss_paddr, M_DEVBUF);
			}
		}
		sglist_free(sgl_node->sgl_data);
		free(sgl_node, M_DEVBUF);
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
	struct hv_sgl_node *sgl_node = NULL;
	int j = 0;

	sc->hs_destroy = TRUE;

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

	hv_vmbus_channel_close(sc->hs_chan);

	mtx_lock(&sc->hs_lock);
	while (!LIST_EMPTY(&sc->hs_free_list)) {
		reqp = LIST_FIRST(&sc->hs_free_list);
		LIST_REMOVE(reqp, link);

		free(reqp, M_DEVBUF);
	}
	mtx_unlock(&sc->hs_lock);

	while (!LIST_EMPTY(&g_hv_sgl_page_pool.free_sgl_list)) {
		sgl_node = LIST_FIRST(&g_hv_sgl_page_pool.free_sgl_list);
		LIST_REMOVE(sgl_node, link);
		for (j = 0; j < HV_MAX_MULTIPAGE_BUFFER_COUNT; j++){
			if (NULL !=
			    (void*)sgl_node->sgl_data->sg_segs[j].ss_paddr) {
				free((void*)sgl_node->sgl_data->sg_segs[j].ss_paddr, M_DEVBUF);
			}
		}
		sglist_free(sgl_node->sgl_data);
		free(sgl_node, M_DEVBUF);
	}
	
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
	ret = hv_storvsc_io_request(sc, reqp);
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

#ifdef notyet
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
		callout_reset_sbt(&reqp->callout, SBT_1MS * ccb->ccb_h.timeout,
		    0, storvsc_timeout, reqp, 0);
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
#endif

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
	hv_storvsc_on_channel_callback(sc->hs_chan);
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
		cpi->initiator_id = cpi->max_target;
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
		if ((res = hv_storvsc_host_reset(sc)) != 0) {
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
		if ((res = create_storvsc_request(ccb, reqp)) != 0) {
			ccb->ccb_h.status = CAM_REQ_INVALID;
			xpt_done(ccb);
			return;
		}

#ifdef notyet
		if (ccb->ccb_h.timeout != CAM_TIME_INFINITY) {
			callout_init(&reqp->callout, 1);
			callout_reset_sbt(&reqp->callout,
			    SBT_1MS * ccb->ccb_h.timeout, 0,
			    storvsc_timeout, reqp, 0);
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
#endif

		if ((res = hv_storvsc_io_request(sc, reqp)) != 0) {
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
 * @brief destroy bounce buffer
 *
 * This function is responsible for destroy a Scatter/Gather list
 * that create by storvsc_create_bounce_buffer()
 *
 * @param sgl- the Scatter/Gather need be destroy
 * @param sg_count- page count of the SG list.
 *
 */
static void
storvsc_destroy_bounce_buffer(struct sglist *sgl)
{
	struct hv_sgl_node *sgl_node = NULL;
	if (LIST_EMPTY(&g_hv_sgl_page_pool.in_use_sgl_list)) {
		printf("storvsc error: not enough in use sgl\n");
		return;
	}
	sgl_node = LIST_FIRST(&g_hv_sgl_page_pool.in_use_sgl_list);
	LIST_REMOVE(sgl_node, link);
	sgl_node->sgl_data = sgl;
	LIST_INSERT_HEAD(&g_hv_sgl_page_pool.free_sgl_list, sgl_node, link);
}

/**
 * @brief create bounce buffer
 *
 * This function is responsible for create a Scatter/Gather list,
 * which hold several pages that can be aligned with page size.
 *
 * @param seg_count- SG-list segments count
 * @param write - if WRITE_TYPE, set SG list page used size to 0,
 * otherwise set used size to page size.
 *
 * return NULL if create failed
 */
static struct sglist *
storvsc_create_bounce_buffer(uint16_t seg_count, int write)
{
	int i = 0;
	struct sglist *bounce_sgl = NULL;
	unsigned int buf_len = ((write == WRITE_TYPE) ? 0 : PAGE_SIZE);
	struct hv_sgl_node *sgl_node = NULL;	

	/* get struct sglist from free_sgl_list */
	if (LIST_EMPTY(&g_hv_sgl_page_pool.free_sgl_list)) {
		printf("storvsc error: not enough free sgl\n");
		return NULL;
	}
	sgl_node = LIST_FIRST(&g_hv_sgl_page_pool.free_sgl_list);
	LIST_REMOVE(sgl_node, link);
	bounce_sgl = sgl_node->sgl_data;
	LIST_INSERT_HEAD(&g_hv_sgl_page_pool.in_use_sgl_list, sgl_node, link);

	bounce_sgl->sg_maxseg = seg_count;

	if (write == WRITE_TYPE)
		bounce_sgl->sg_nseg = 0;
	else
		bounce_sgl->sg_nseg = seg_count;

	for (i = 0; i < seg_count; i++)
	        bounce_sgl->sg_segs[i].ss_len = buf_len;

	return bounce_sgl;
}

/**
 * @brief copy data from SG list to bounce buffer
 *
 * This function is responsible for copy data from one SG list's segments
 * to another SG list which used as bounce buffer.
 *
 * @param bounce_sgl - the destination SG list
 * @param orig_sgl - the segment of the source SG list.
 * @param orig_sgl_count - the count of segments.
 * @param orig_sgl_count - indicate which segment need bounce buffer,
 *  set 1 means need.
 *
 */
static void
storvsc_copy_sgl_to_bounce_buf(struct sglist *bounce_sgl,
			       bus_dma_segment_t *orig_sgl,
			       unsigned int orig_sgl_count,
			       uint64_t seg_bits)
{
	int src_sgl_idx = 0;

	for (src_sgl_idx = 0; src_sgl_idx < orig_sgl_count; src_sgl_idx++) {
		if (seg_bits & (1 << src_sgl_idx)) {
			memcpy((void*)bounce_sgl->sg_segs[src_sgl_idx].ss_paddr,
			    (void*)orig_sgl[src_sgl_idx].ds_addr,
			    orig_sgl[src_sgl_idx].ds_len);

			bounce_sgl->sg_segs[src_sgl_idx].ss_len =
			    orig_sgl[src_sgl_idx].ds_len;
		}
	}
}

/**
 * @brief copy data from SG list which used as bounce to another SG list
 *
 * This function is responsible for copy data from one SG list with bounce
 * buffer to another SG list's segments.
 *
 * @param dest_sgl - the destination SG list's segments
 * @param dest_sgl_count - the count of destination SG list's segment.
 * @param src_sgl - the source SG list.
 * @param seg_bits - indicate which segment used bounce buffer of src SG-list.
 *
 */
void
storvsc_copy_from_bounce_buf_to_sgl(bus_dma_segment_t *dest_sgl,
				    unsigned int dest_sgl_count,
				    struct sglist* src_sgl,
				    uint64_t seg_bits)
{
	int sgl_idx = 0;
	
	for (sgl_idx = 0; sgl_idx < dest_sgl_count; sgl_idx++) {
		if (seg_bits & (1 << sgl_idx)) {
			memcpy((void*)(dest_sgl[sgl_idx].ds_addr),
			    (void*)(src_sgl->sg_segs[sgl_idx].ss_paddr),
			    src_sgl->sg_segs[sgl_idx].ss_len);
		}
	}
}

/**
 * @brief check SG list with bounce buffer or not
 *
 * This function is responsible for check if need bounce buffer for SG list.
 *
 * @param sgl - the SG list's segments
 * @param sg_count - the count of SG list's segment.
 * @param bits - segmengs number that need bounce buffer
 *
 * return -1 if SG list needless bounce buffer
 */
static int
storvsc_check_bounce_buffer_sgl(bus_dma_segment_t *sgl,
				unsigned int sg_count,
				uint64_t *bits)
{
	int i = 0;
	int offset = 0;
	uint64_t phys_addr = 0;
	uint64_t tmp_bits = 0;
	boolean_t found_hole = FALSE;
	boolean_t pre_aligned = TRUE;

	if (sg_count < 2){
		return -1;
	}

	*bits = 0;
	
	phys_addr = vtophys(sgl[0].ds_addr);
	offset =  phys_addr - trunc_page(phys_addr);

	if (offset != 0) {
		pre_aligned = FALSE;
		tmp_bits |= 1;
	}

	for (i = 1; i < sg_count; i++) {
		phys_addr = vtophys(sgl[i].ds_addr);
		offset =  phys_addr - trunc_page(phys_addr);

		if (offset == 0) {
			if (FALSE == pre_aligned){
				/*
				 * This segment is aligned, if the previous
				 * one is not aligned, find a hole
				 */
				found_hole = TRUE;
			}
			pre_aligned = TRUE;
		} else {
			tmp_bits |= 1 << i;
			if (!pre_aligned) {
				if (phys_addr != vtophys(sgl[i-1].ds_addr +
				    sgl[i-1].ds_len)) {
					/*
					 * Check whether connect to previous
					 * segment,if not, find the hole
					 */
					found_hole = TRUE;
				}
			} else {
				found_hole = TRUE;
			}
			pre_aligned = FALSE;
		}
	}

	if (!found_hole) {
		return (-1);
	} else {
		*bits = tmp_bits;
		return 0;
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
static int
create_storvsc_request(union ccb *ccb, struct hv_storvsc_request *reqp)
{
	struct ccb_scsiio *csio = &ccb->csio;
	uint64_t phys_addr;
	uint32_t bytes_to_copy = 0;
	uint32_t pfn_num = 0;
	uint32_t pfn;
	uint64_t not_aligned_seg_bits = 0;
	
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

	if (0 == csio->dxfer_len) {
		return (0);
	}

	reqp->data_buf.length = csio->dxfer_len;

	switch (ccb->ccb_h.flags & CAM_DATA_MASK) {
	case CAM_DATA_VADDR:
	{
		bytes_to_copy = csio->dxfer_len;
		phys_addr = vtophys(csio->data_ptr);
		reqp->data_buf.offset = phys_addr & PAGE_MASK;
		
		while (bytes_to_copy != 0) {
			int bytes, page_offset;
			phys_addr =
			    vtophys(&csio->data_ptr[reqp->data_buf.length -
			    bytes_to_copy]);
			pfn = phys_addr >> PAGE_SHIFT;
			reqp->data_buf.pfn_array[pfn_num] = pfn;
			page_offset = phys_addr & PAGE_MASK;

			bytes = min(PAGE_SIZE - page_offset, bytes_to_copy);

			bytes_to_copy -= bytes;
			pfn_num++;
		}
		break;
	}

	case CAM_DATA_SG:
	{
		int i = 0;
		int offset = 0;
		int ret;

		bus_dma_segment_t *storvsc_sglist =
		    (bus_dma_segment_t *)ccb->csio.data_ptr;
		u_int16_t storvsc_sg_count = ccb->csio.sglist_cnt;

		printf("Storvsc: get SG I/O operation, %d\n",
		    reqp->vstor_packet.u.vm_srb.data_in);

		if (storvsc_sg_count > HV_MAX_MULTIPAGE_BUFFER_COUNT){
			printf("Storvsc: %d segments is too much, "
			    "only support %d segments\n",
			    storvsc_sg_count, HV_MAX_MULTIPAGE_BUFFER_COUNT);
			return (EINVAL);
		}

		/*
		 * We create our own bounce buffer function currently. Idealy
		 * we should use BUS_DMA(9) framework. But with current BUS_DMA
		 * code there is no callback API to check the page alignment of
		 * middle segments before busdma can decide if a bounce buffer
		 * is needed for particular segment. There is callback,
		 * "bus_dma_filter_t *filter", but the parrameters are not
		 * sufficient for storvsc driver.
		 * TODO:
		 *	Add page alignment check in BUS_DMA(9) callback. Once
		 *	this is complete, switch the following code to use
		 *	BUS_DMA(9) for storvsc bounce buffer support.
		 */
		/* check if we need to create bounce buffer */
		ret = storvsc_check_bounce_buffer_sgl(storvsc_sglist,
		    storvsc_sg_count, &not_aligned_seg_bits);
		if (ret != -1) {
			reqp->bounce_sgl =
			    storvsc_create_bounce_buffer(storvsc_sg_count,
			    reqp->vstor_packet.u.vm_srb.data_in);
			if (NULL == reqp->bounce_sgl) {
				printf("Storvsc_error: "
				    "create bounce buffer failed.\n");
				return (ENOMEM);
			}

			reqp->bounce_sgl_count = storvsc_sg_count;
			reqp->not_aligned_seg_bits = not_aligned_seg_bits;

			/*
			 * if it is write, we need copy the original data
			 *to bounce buffer
			 */
			if (WRITE_TYPE == reqp->vstor_packet.u.vm_srb.data_in) {
				storvsc_copy_sgl_to_bounce_buf(
				    reqp->bounce_sgl,
				    storvsc_sglist,
				    storvsc_sg_count,
				    reqp->not_aligned_seg_bits);
			}

			/* transfer virtual address to physical frame number */
			if (reqp->not_aligned_seg_bits & 0x1){
 				phys_addr =
				    vtophys(reqp->bounce_sgl->sg_segs[0].ss_paddr);
			}else{
 				phys_addr =
					vtophys(storvsc_sglist[0].ds_addr);
			}
			reqp->data_buf.offset = phys_addr & PAGE_MASK;

			pfn = phys_addr >> PAGE_SHIFT;
			reqp->data_buf.pfn_array[0] = pfn;
			
			for (i = 1; i < storvsc_sg_count; i++) {
				if (reqp->not_aligned_seg_bits & (1 << i)) {
					phys_addr =
					    vtophys(reqp->bounce_sgl->sg_segs[i].ss_paddr);
				} else {
					phys_addr =
					    vtophys(storvsc_sglist[i].ds_addr);
				}

				pfn = phys_addr >> PAGE_SHIFT;
				reqp->data_buf.pfn_array[i] = pfn;
			}
		} else {
			phys_addr = vtophys(storvsc_sglist[0].ds_addr);

			reqp->data_buf.offset = phys_addr & PAGE_MASK;

			for (i = 0; i < storvsc_sg_count; i++) {
				phys_addr = vtophys(storvsc_sglist[i].ds_addr);
				pfn = phys_addr >> PAGE_SHIFT;
				reqp->data_buf.pfn_array[i] = pfn;
			}

			/* check the last segment cross boundary or not */
			offset = phys_addr & PAGE_MASK;
			if (offset) {
				phys_addr =
				    vtophys(storvsc_sglist[i-1].ds_addr +
				    PAGE_SIZE - offset);
				pfn = phys_addr >> PAGE_SHIFT;
				reqp->data_buf.pfn_array[i] = pfn;
			}
			
			reqp->bounce_sgl_count = 0;
		}
		break;
	}
	default:
		printf("Unknow flags: %d\n", ccb->ccb_h.flags);
		return(EINVAL);
	}

	return(0);
}

/*
 * SCSI Inquiry checks qualifier and type.
 * If qualifier is 011b, means the device server is not capable
 * of supporting a peripheral device on this logical unit, and
 * the type should be set to 1Fh.
 * 
 * Return 1 if it is valid, 0 otherwise.
 */
static inline int
is_inquiry_valid(const struct scsi_inquiry_data *inq_data)
{
	uint8_t type;
	if (SID_QUAL(inq_data) != SID_QUAL_LU_CONNECTED) {
		return (0);
	}
	type = SID_TYPE(inq_data);
	if (type == T_NODEVICE) {
		return (0);
	}
	return (1);
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
	bus_dma_segment_t *ori_sglist = NULL;
	int ori_sg_count = 0;

	/* destroy bounce buffer if it is used */
	if (reqp->bounce_sgl_count) {
		ori_sglist = (bus_dma_segment_t *)ccb->csio.data_ptr;
		ori_sg_count = ccb->csio.sglist_cnt;

		/*
		 * If it is READ operation, we should copy back the data
		 * to original SG list.
		 */
		if (READ_TYPE == reqp->vstor_packet.u.vm_srb.data_in) {
			storvsc_copy_from_bounce_buf_to_sgl(ori_sglist,
			    ori_sg_count,
			    reqp->bounce_sgl,
			    reqp->not_aligned_seg_bits);
		}

		storvsc_destroy_bounce_buffer(reqp->bounce_sgl);
		reqp->bounce_sgl_count = 0;
	}
		
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

#ifdef notyet
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
#endif

	ccb->ccb_h.status &= ~CAM_SIM_QUEUED;
	ccb->ccb_h.status &= ~CAM_STATUS_MASK;
	if (vm_srb->scsi_status == SCSI_STATUS_OK) {
		const struct scsi_generic *cmd;
		/*
		 * Check whether the data for INQUIRY cmd is valid or
		 * not.  Windows 10 and Windows 2016 send all zero
		 * inquiry data to VM even for unpopulated slots.
		 */
		cmd = (const struct scsi_generic *)
		    ((ccb->ccb_h.flags & CAM_CDB_POINTER) ?
		     csio->cdb_io.cdb_ptr : csio->cdb_io.cdb_bytes);
		if (cmd->opcode == INQUIRY) {
		    /*
		     * The host of Windows 10 or 2016 server will response
		     * the inquiry request with invalid data for unexisted device:
			[0x7f 0x0 0x5 0x2 0x1f ... ]
		     * But on windows 2012 R2, the response is:
			[0x7f 0x0 0x0 0x0 0x0 ]
		     * That is why here wants to validate the inquiry response.
		     * The validation will skip the INQUIRY whose response is short,
		     * which is less than SHORT_INQUIRY_LENGTH (36).
		     *
		     * For more information about INQUIRY, please refer to:
		     *  ftp://ftp.avc-pioneer.com/Mtfuji_7/Proposal/Jun09/INQUIRY.pdf
		     */
		    const struct scsi_inquiry_data *inq_data =
			(const struct scsi_inquiry_data *)csio->data_ptr;
		    uint8_t* resp_buf = (uint8_t*)csio->data_ptr;
		    /* Get the buffer length reported by host */
		    int resp_xfer_len = vm_srb->transfer_len;
		    /* Get the available buffer length */
		    int resp_buf_len = resp_xfer_len >= 5 ? resp_buf[4] + 5 : 0;
		    int data_len = (resp_buf_len < resp_xfer_len) ? resp_buf_len : resp_xfer_len;
		    if (data_len < SHORT_INQUIRY_LENGTH) {
			ccb->ccb_h.status |= CAM_REQ_CMP;
			if (bootverbose && data_len >= 5) {
				mtx_lock(&sc->hs_lock);
				xpt_print(ccb->ccb_h.path,
				    "storvsc skips the validation for short inquiry (%d)"
				    " [%x %x %x %x %x]\n",
				    data_len,resp_buf[0],resp_buf[1],resp_buf[2],
				    resp_buf[3],resp_buf[4]);
				mtx_unlock(&sc->hs_lock);
			}
		    } else if (is_inquiry_valid(inq_data) == 0) {
			ccb->ccb_h.status |= CAM_DEV_NOT_THERE;
			if (bootverbose && data_len >= 5) {
				mtx_lock(&sc->hs_lock);
				xpt_print(ccb->ccb_h.path,
				    "storvsc uninstalled invalid device"
				    " [%x %x %x %x %x]\n",
				resp_buf[0],resp_buf[1],resp_buf[2],resp_buf[3],resp_buf[4]);
				mtx_unlock(&sc->hs_lock);
			}
		    } else {
			ccb->ccb_h.status |= CAM_REQ_CMP;
			if (bootverbose) {
				mtx_lock(&sc->hs_lock);
				xpt_print(ccb->ccb_h.path,
				    "storvsc has passed inquiry response (%d) validation\n",
				    data_len);
				mtx_unlock(&sc->hs_lock);
			}
		    }
		} else {
			ccb->ccb_h.status |= CAM_REQ_CMP;
		}
	} else {
		mtx_lock(&sc->hs_lock);
		xpt_print(ccb->ccb_h.path,
			"storvsc scsi_status = %d\n",
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
	mtx_unlock(&sc->hs_lock);

	xpt_done_direct(ccb);
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
	device_t parent = device_get_parent(dev);

	if (VMBUS_PROBE_GUID(parent, dev, &gBlkVscDeviceType) == 0)
		return DRIVER_BLKVSC;
	if (VMBUS_PROBE_GUID(parent, dev, &gStorVscDeviceType) == 0)
		return DRIVER_STORVSC;
	return DRIVER_UNKNOWN;
}
