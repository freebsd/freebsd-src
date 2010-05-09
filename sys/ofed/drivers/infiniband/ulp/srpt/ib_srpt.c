/*
 * Copyright (c) 2006 - 2009 Mellanox Technology Inc.  All rights reserved.
 * Copyright (C) 2008 Vladislav Bolkhovitin <vst@vlnb.net>
 * Copyright (C) 2008 - 2010 Bart Van Assche <bart.vanassche@gmail.com>
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/ctype.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <asm/atomic.h>
#if defined(CONFIG_SCST_DEBUG) || defined(CONFIG_SCST_TRACING)
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#endif
#include "ib_srpt.h"
#define LOG_PREFIX "ib_srpt" /* Prefix for SCST tracing macros. */
#include "scst_debug.h"

#define CONFIG_SCST_PROC

/* Name of this kernel module. */
#define DRV_NAME		"ib_srpt"
#define DRV_VERSION		"1.0.1"
#define DRV_RELDATE		"July 10, 2008"
#if defined(CONFIG_SCST_DEBUG) || defined(CONFIG_SCST_TRACING)
/* Flags to be used in SCST debug tracing statements. */
#define DEFAULT_SRPT_TRACE_FLAGS (TRACE_OUT_OF_MEM | TRACE_MINOR \
				  | TRACE_MGMT | TRACE_SPECIAL)
/* Name of the entry that will be created under /proc/scsi_tgt/ib_srpt. */
#define SRPT_PROC_TRACE_LEVEL_NAME	"trace_level"
#endif

#define MELLANOX_SRPT_ID_STRING	"SCST SRP target"

MODULE_AUTHOR("Vu Pham");
MODULE_DESCRIPTION("InfiniBand SCSI RDMA Protocol target "
		   "v" DRV_VERSION " (" DRV_RELDATE ")");
MODULE_LICENSE("Dual BSD/GPL");


/*
 * Global Variables
 */

static u64 srpt_service_guid;
/* List of srpt_device structures. */
static atomic_t srpt_device_count;
static DECLARE_WAIT_QUEUE_HEAD(ioctx_list_waitQ);
#if defined(CONFIG_SCST_DEBUG) || defined(CONFIG_SCST_TRACING)
static unsigned long trace_flag = DEFAULT_SRPT_TRACE_FLAGS;
module_param(trace_flag, long, 0644);
MODULE_PARM_DESC(trace_flag,
		 "Trace flags for the ib_srpt kernel module.");
#endif
#if defined(CONFIG_SCST_DEBUG)
static unsigned long interrupt_processing_delay_in_us;
module_param(interrupt_processing_delay_in_us, long, 0744);
MODULE_PARM_DESC(interrupt_processing_delay_in_us,
		 "CQ completion handler interrupt delay in microseconds.");
#endif

static int thread;
module_param(thread, int, 0444);
MODULE_PARM_DESC(thread,
		 "Execute SCSI commands in thread context. Defaults to zero,"
		 " i.e. soft IRQ whenever possible.");

static unsigned srp_max_rdma_size = DEFAULT_MAX_RDMA_SIZE;
module_param(srp_max_rdma_size, int, 0744);
MODULE_PARM_DESC(srp_max_rdma_size,
		 "Maximum size of SRP RDMA transfers for new connections.");

static unsigned srp_max_message_size = DEFAULT_MAX_MESSAGE_SIZE;
module_param(srp_max_message_size, int, 0444);
MODULE_PARM_DESC(srp_max_message_size,
		 "Maximum size of SRP control messages in bytes.");

static int use_port_guid_in_session_name;
module_param(use_port_guid_in_session_name, bool, 0444);
MODULE_PARM_DESC(use_port_guid_in_session_name,
		 "Use target port ID in the SCST session name such that"
		 " redundant paths between multiport systems can be masked.");

module_param(srpt_service_guid, long, 0444);
MODULE_PARM_DESC(srpt_service_guid,
		 "Using this value for ioc_guid, id_ext, and cm_listen_id"
		 " instead of using the node_guid of the first HCA.");

static void srpt_add_one(struct ib_device *device);
static void srpt_remove_one(struct ib_device *device);
static void srpt_unregister_mad_agent(struct srpt_device *sdev);
#ifdef CONFIG_SCST_PROC
static void srpt_unregister_procfs_entry(struct scst_tgt_template *tgt);
#endif /*CONFIG_SCST_PROC*/
static void srpt_unmap_sg_to_ib_sge(struct srpt_rdma_ch *ch,
				    struct srpt_ioctx *ioctx);
static void srpt_release_channel(struct scst_session *scst_sess);

static struct ib_client srpt_client = {
	.name = DRV_NAME,
	.add = srpt_add_one,
	.remove = srpt_remove_one
};

/**
 * Atomically test and set the channel state.
 * @ch: RDMA channel.
 * @old: channel state to compare with.
 * @new: state to change the channel state to if the current state matches the
 *       argument 'old'.
 *
 * Returns the previous channel state.
 */
static enum rdma_ch_state
srpt_test_and_set_channel_state(struct srpt_rdma_ch *ch,
				enum rdma_ch_state old,
				enum rdma_ch_state new)
{
	return atomic_cmpxchg(&ch->state, old, new);
}

/*
 * Callback function called by the InfiniBand core when an asynchronous IB
 * event occurs. This callback may occur in interrupt context. See also
 * section 11.5.2, Set Asynchronous Event Handler in the InfiniBand
 * Architecture Specification.
 */
static void srpt_event_handler(struct ib_event_handler *handler,
			       struct ib_event *event)
{
	struct srpt_device *sdev;
	struct srpt_port *sport;

	TRACE_ENTRY();

	sdev = ib_get_client_data(event->device, &srpt_client);
	if (!sdev || sdev->device != event->device)
		return;

	TRACE_DBG("ASYNC event= %d on device= %s",
		  event->event, sdev->device->name);

	switch (event->event) {
	case IB_EVENT_PORT_ERR:
		if (event->element.port_num <= sdev->device->phys_port_cnt) {
			sport = &sdev->port[event->element.port_num - 1];
			sport->lid = 0;
			sport->sm_lid = 0;
		}
		break;
	case IB_EVENT_PORT_ACTIVE:
	case IB_EVENT_LID_CHANGE:
	case IB_EVENT_PKEY_CHANGE:
	case IB_EVENT_SM_CHANGE:
	case IB_EVENT_CLIENT_REREGISTER:
		/*
		 * Refresh port data asynchronously. Note: it is safe to call
		 * schedule_work() even if &sport->work is already on the
		 * global workqueue because schedule_work() tests for the
		 * work_pending() condition before adding &sport->work to the
		 * global work queue.
		 */
		if (event->element.port_num <= sdev->device->phys_port_cnt) {
			sport = &sdev->port[event->element.port_num - 1];
			if (!sport->lid && !sport->sm_lid)
				schedule_work(&sport->work);
		}
		break;
	default:
		PRINT_ERROR("received unrecognized IB event %d", event->event);
		break;
	}

	TRACE_EXIT();
}

/*
 * Callback function called by the InfiniBand core for SRQ (shared receive
 * queue) events.
 */
static void srpt_srq_event(struct ib_event *event, void *ctx)
{
	PRINT_INFO("SRQ event %d", event->event);
}

/*
 * Callback function called by the InfiniBand core for QP (queue pair) events.
 */
static void srpt_qp_event(struct ib_event *event, struct srpt_rdma_ch *ch)
{
	TRACE_DBG("QP event %d on cm_id=%p sess_name=%s state=%d",
		  event->event, ch->cm_id, ch->sess_name,
		  atomic_read(&ch->state));

	switch (event->event) {
	case IB_EVENT_COMM_EST:
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 20) || defined(BACKPORT_LINUX_WORKQUEUE_TO_2_6_19)
		ib_cm_notify(ch->cm_id, event->event);
#else
		/* Vanilla 2.6.19 kernel (or before) without OFED. */
		PRINT_ERROR("%s", "how to perform ib_cm_notify() on a"
			    " vanilla 2.6.18 kernel ???");
#endif
		break;
	case IB_EVENT_QP_LAST_WQE_REACHED:
		if (srpt_test_and_set_channel_state(ch, RDMA_CHANNEL_LIVE,
			RDMA_CHANNEL_DISCONNECTING) == RDMA_CHANNEL_LIVE) {
			PRINT_INFO("disconnected session %s.", ch->sess_name);
			ib_send_cm_dreq(ch->cm_id, NULL, 0);
		}
		break;
	default:
		PRINT_ERROR("received unrecognized IB QP event %d",
			    event->event);
		break;
	}
}

/*
 * Helper function for filling in an InfiniBand IOUnitInfo structure. Copies
 * the lowest four bits of value in element slot of the array of four bit
 * elements called c_list (controller list). The index slot is one-based.
 *
 * @pre 1 <= slot && 0 <= value && value < 16
 */
static void srpt_set_ioc(u8 *c_list, u32 slot, u8 value)
{
	u16 id;
	u8 tmp;

	id = (slot - 1) / 2;
	if (slot & 0x1) {
		tmp = c_list[id] & 0xf;
		c_list[id] = (value << 4) | tmp;
	} else {
		tmp = c_list[id] & 0xf0;
		c_list[id] = (value & 0xf) | tmp;
	}
}

/*
 * Write InfiniBand ClassPortInfo to mad. See also section 16.3.3.1
 * ClassPortInfo in the InfiniBand Architecture Specification.
 */
static void srpt_get_class_port_info(struct ib_dm_mad *mad)
{
	struct ib_class_port_info *cif;

	cif = (struct ib_class_port_info *)mad->data;
	memset(cif, 0, sizeof *cif);
	cif->base_version = 1;
	cif->class_version = 1;
	cif->resp_time_value = 20;

	mad->mad_hdr.status = 0;
}

/*
 * Write IOUnitInfo to mad. See also section 16.3.3.3 IOUnitInfo in the
 * InfiniBand Architecture Specification. See also section B.7,
 * table B.6 in the T10 SRP r16a document.
 */
static void srpt_get_iou(struct ib_dm_mad *mad)
{
	struct ib_dm_iou_info *ioui;
	u8 slot;
	int i;

	ioui = (struct ib_dm_iou_info *)mad->data;
	ioui->change_id = 1;
	ioui->max_controllers = 16;

	/* set present for slot 1 and empty for the rest */
	srpt_set_ioc(ioui->controller_list, 1, 1);
	for (i = 1, slot = 2; i < 16; i++, slot++)
		srpt_set_ioc(ioui->controller_list, slot, 0);

	mad->mad_hdr.status = 0;
}

/*
 * Write IOControllerprofile to mad for I/O controller (sdev, slot). See also
 * section 16.3.3.4 IOControllerProfile in the InfiniBand Architecture
 * Specification. See also section B.7, table B.7 in the T10 SRP r16a
 * document.
 */
static void srpt_get_ioc(struct srpt_device *sdev, u32 slot,
			 struct ib_dm_mad *mad)
{
	struct ib_dm_ioc_profile *iocp;

	iocp = (struct ib_dm_ioc_profile *)mad->data;

	if (!slot || slot > 16) {
		mad->mad_hdr.status = cpu_to_be16(DM_MAD_STATUS_INVALID_FIELD);
		return;
	}

	if (slot > 2) {
		mad->mad_hdr.status = cpu_to_be16(DM_MAD_STATUS_NO_IOC);
		return;
	}

	memset(iocp, 0, sizeof *iocp);
	strcpy(iocp->id_string, MELLANOX_SRPT_ID_STRING);
	iocp->guid = cpu_to_be64(srpt_service_guid);
	iocp->vendor_id = cpu_to_be32(sdev->dev_attr.vendor_id);
	iocp->device_id = cpu_to_be32(sdev->dev_attr.vendor_part_id);
	iocp->device_version = cpu_to_be16(sdev->dev_attr.hw_ver);
	iocp->subsys_vendor_id = cpu_to_be32(sdev->dev_attr.vendor_id);
	iocp->subsys_device_id = 0x0;
	iocp->io_class = cpu_to_be16(SRP_REV16A_IB_IO_CLASS);
	iocp->io_subclass = cpu_to_be16(SRP_IO_SUBCLASS);
	iocp->protocol = cpu_to_be16(SRP_PROTOCOL);
	iocp->protocol_version = cpu_to_be16(SRP_PROTOCOL_VERSION);
	iocp->send_queue_depth = cpu_to_be16(SRPT_SRQ_SIZE);
	iocp->rdma_read_depth = 4;
	iocp->send_size = cpu_to_be32(srp_max_message_size);
	iocp->rdma_size = cpu_to_be32(min(max(srp_max_rdma_size, 256U),
					  1U << 24));
	iocp->num_svc_entries = 1;
	iocp->op_cap_mask = SRP_SEND_TO_IOC | SRP_SEND_FROM_IOC |
		SRP_RDMA_READ_FROM_IOC | SRP_RDMA_WRITE_FROM_IOC;

	mad->mad_hdr.status = 0;
}

/*
 * Device management: write ServiceEntries to mad for the given slot. See also
 * section 16.3.3.5 ServiceEntries in the InfiniBand Architecture
 * Specification. See also section B.7, table B.8 in the T10 SRP r16a document.
 */
static void srpt_get_svc_entries(u64 ioc_guid,
				 u16 slot, u8 hi, u8 lo, struct ib_dm_mad *mad)
{
	struct ib_dm_svc_entries *svc_entries;

	WARN_ON(!ioc_guid);

	if (!slot || slot > 16) {
		mad->mad_hdr.status = cpu_to_be16(DM_MAD_STATUS_INVALID_FIELD);
		return;
	}

	if (slot > 2 || lo > hi || hi > 1) {
		mad->mad_hdr.status = cpu_to_be16(DM_MAD_STATUS_NO_IOC);
		return;
	}

	svc_entries = (struct ib_dm_svc_entries *)mad->data;
	memset(svc_entries, 0, sizeof *svc_entries);
	svc_entries->service_entries[0].id = cpu_to_be64(ioc_guid);
	snprintf(svc_entries->service_entries[0].name,
		 sizeof(svc_entries->service_entries[0].name),
		 "%s%016llx",
		 SRP_SERVICE_NAME_PREFIX,
		 (unsigned long long)ioc_guid);

	mad->mad_hdr.status = 0;
}

/*
 * Actual processing of a received MAD *rq_mad received through source port *sp
 * (MAD = InfiniBand management datagram). The response to be sent back is
 * written to *rsp_mad.
 */
static void srpt_mgmt_method_get(struct srpt_port *sp, struct ib_mad *rq_mad,
				 struct ib_dm_mad *rsp_mad)
{
	u16 attr_id;
	u32 slot;
	u8 hi, lo;

	attr_id = be16_to_cpu(rq_mad->mad_hdr.attr_id);
	switch (attr_id) {
	case DM_ATTR_CLASS_PORT_INFO:
		srpt_get_class_port_info(rsp_mad);
		break;
	case DM_ATTR_IOU_INFO:
		srpt_get_iou(rsp_mad);
		break;
	case DM_ATTR_IOC_PROFILE:
		slot = be32_to_cpu(rq_mad->mad_hdr.attr_mod);
		srpt_get_ioc(sp->sdev, slot, rsp_mad);
		break;
	case DM_ATTR_SVC_ENTRIES:
		slot = be32_to_cpu(rq_mad->mad_hdr.attr_mod);
		hi = (u8) ((slot >> 8) & 0xff);
		lo = (u8) (slot & 0xff);
		slot = (u16) ((slot >> 16) & 0xffff);
		srpt_get_svc_entries(srpt_service_guid,
				     slot, hi, lo, rsp_mad);
		break;
	default:
		rsp_mad->mad_hdr.status =
		    cpu_to_be16(DM_MAD_STATUS_UNSUP_METHOD_ATTR);
		break;
	}
}

/*
 * Callback function that is called by the InfiniBand core after transmission of
 * a MAD. (MAD = management datagram; AH = address handle.)
 */
static void srpt_mad_send_handler(struct ib_mad_agent *mad_agent,
				  struct ib_mad_send_wc *mad_wc)
{
	ib_destroy_ah(mad_wc->send_buf->ah);
	ib_free_send_mad(mad_wc->send_buf);
}

/*
 * Callback function that is called by the InfiniBand core after reception of
 * a MAD (management datagram).
 */
static void srpt_mad_recv_handler(struct ib_mad_agent *mad_agent,
				  struct ib_mad_recv_wc *mad_wc)
{
	struct srpt_port *sport = (struct srpt_port *)mad_agent->context;
	struct ib_ah *ah;
	struct ib_mad_send_buf *rsp;
	struct ib_dm_mad *dm_mad;

	if (!mad_wc || !mad_wc->recv_buf.mad)
		return;

	ah = ib_create_ah_from_wc(mad_agent->qp->pd, mad_wc->wc,
				  mad_wc->recv_buf.grh, mad_agent->port_num);
	if (IS_ERR(ah))
		goto err;

	BUILD_BUG_ON(offsetof(struct ib_dm_mad, data) != IB_MGMT_DEVICE_HDR);

	rsp = ib_create_send_mad(mad_agent, mad_wc->wc->src_qp,
				 mad_wc->wc->pkey_index, 0,
				 IB_MGMT_DEVICE_HDR, IB_MGMT_DEVICE_DATA,
				 GFP_KERNEL);
	if (IS_ERR(rsp))
		goto err_rsp;

	rsp->ah = ah;

	dm_mad = rsp->mad;
	memcpy(dm_mad, mad_wc->recv_buf.mad, sizeof *dm_mad);
	dm_mad->mad_hdr.method = IB_MGMT_METHOD_GET_RESP;
	dm_mad->mad_hdr.status = 0;

	switch (mad_wc->recv_buf.mad->mad_hdr.method) {
	case IB_MGMT_METHOD_GET:
		srpt_mgmt_method_get(sport, mad_wc->recv_buf.mad, dm_mad);
		break;
	case IB_MGMT_METHOD_SET:
		dm_mad->mad_hdr.status =
		    cpu_to_be16(DM_MAD_STATUS_UNSUP_METHOD_ATTR);
		break;
	default:
		dm_mad->mad_hdr.status =
		    cpu_to_be16(DM_MAD_STATUS_UNSUP_METHOD);
		break;
	}

	if (!ib_post_send_mad(rsp, NULL)) {
		ib_free_recv_mad(mad_wc);
		/* will destroy_ah & free_send_mad in send completion */
		return;
	}

	ib_free_send_mad(rsp);

err_rsp:
	ib_destroy_ah(ah);
err:
	ib_free_recv_mad(mad_wc);
}

/*
 * Enable InfiniBand management datagram processing, update the cached sm_lid,
 * lid and gid values, and register a callback function for processing MADs
 * on the specified port. It is safe to call this function more than once for
 * the same port.
 */
static int srpt_refresh_port(struct srpt_port *sport)
{
	struct ib_mad_reg_req reg_req;
	struct ib_port_modify port_modify;
	struct ib_port_attr port_attr;
	int ret;

	TRACE_ENTRY();

	memset(&port_modify, 0, sizeof port_modify);
	port_modify.set_port_cap_mask = IB_PORT_DEVICE_MGMT_SUP;
	port_modify.clr_port_cap_mask = 0;

	ret = ib_modify_port(sport->sdev->device, sport->port, 0, &port_modify);
	if (ret)
		goto err_mod_port;

	ret = ib_query_port(sport->sdev->device, sport->port, &port_attr);
	if (ret)
		goto err_query_port;

	sport->sm_lid = port_attr.sm_lid;
	sport->lid = port_attr.lid;

	ret = ib_query_gid(sport->sdev->device, sport->port, 0, &sport->gid);
	if (ret)
		goto err_query_port;

	if (!sport->mad_agent) {
		memset(&reg_req, 0, sizeof reg_req);
		reg_req.mgmt_class = IB_MGMT_CLASS_DEVICE_MGMT;
		reg_req.mgmt_class_version = IB_MGMT_BASE_VERSION;
		set_bit(IB_MGMT_METHOD_GET, reg_req.method_mask);
		set_bit(IB_MGMT_METHOD_SET, reg_req.method_mask);

		sport->mad_agent = ib_register_mad_agent(sport->sdev->device,
							 sport->port,
							 IB_QPT_GSI,
							 &reg_req, 0,
							 srpt_mad_send_handler,
							 srpt_mad_recv_handler,
							 sport);
		if (IS_ERR(sport->mad_agent)) {
			ret = PTR_ERR(sport->mad_agent);
			sport->mad_agent = NULL;
			goto err_query_port;
		}
	}

	TRACE_EXIT_RES(0);

	return 0;

err_query_port:

	port_modify.set_port_cap_mask = 0;
	port_modify.clr_port_cap_mask = IB_PORT_DEVICE_MGMT_SUP;
	ib_modify_port(sport->sdev->device, sport->port, 0, &port_modify);

err_mod_port:

	TRACE_EXIT_RES(ret);

	return ret;
}

/*
 * Unregister the callback function for processing MADs and disable MAD
 * processing for all ports of the specified device. It is safe to call this
 * function more than once for the same device.
 */
static void srpt_unregister_mad_agent(struct srpt_device *sdev)
{
	struct ib_port_modify port_modify = {
		.clr_port_cap_mask = IB_PORT_DEVICE_MGMT_SUP,
	};
	struct srpt_port *sport;
	int i;

	for (i = 1; i <= sdev->device->phys_port_cnt; i++) {
		sport = &sdev->port[i - 1];
		WARN_ON(sport->port != i);
		if (ib_modify_port(sdev->device, i, 0, &port_modify) < 0)
			PRINT_ERROR("%s", "disabling MAD processing failed.");
		if (sport->mad_agent) {
			ib_unregister_mad_agent(sport->mad_agent);
			sport->mad_agent = NULL;
		}
	}
}

/**
 * Allocate and initialize an SRPT I/O context structure.
 */
static struct srpt_ioctx *srpt_alloc_ioctx(struct srpt_device *sdev)
{
	struct srpt_ioctx *ioctx;

	ioctx = kmalloc(sizeof *ioctx, GFP_KERNEL);
	if (!ioctx)
		goto out;

	ioctx->buf = kzalloc(srp_max_message_size, GFP_KERNEL);
	if (!ioctx->buf)
		goto out_free_ioctx;

	ioctx->dma = ib_dma_map_single(sdev->device, ioctx->buf,
				       srp_max_message_size, DMA_BIDIRECTIONAL);
	if (ib_dma_mapping_error(sdev->device, ioctx->dma))
		goto out_free_buf;

	return ioctx;

out_free_buf:
	kfree(ioctx->buf);
out_free_ioctx:
	kfree(ioctx);
out:
	return NULL;
}

/*
 * Deallocate an SRPT I/O context structure.
 */
static void srpt_free_ioctx(struct srpt_device *sdev, struct srpt_ioctx *ioctx)
{
	if (!ioctx)
		return;

	ib_dma_unmap_single(sdev->device, ioctx->dma,
			    srp_max_message_size, DMA_BIDIRECTIONAL);
	kfree(ioctx->buf);
	kfree(ioctx);
}

/**
 * srpt_alloc_ioctx_ring() -- allocate a ring of SRPT I/O context structures.
 * @sdev: device to allocate the I/O context ring for.
 * @ioctx_ring: pointer to an array of I/O contexts.
 * @ring_size: number of elements in the I/O context ring.
 * @flags: flags to be set in the ring index.
 */
static int srpt_alloc_ioctx_ring(struct srpt_device *sdev,
				 struct srpt_ioctx **ioctx_ring,
				 int ring_size,
				 u32 flags)
{
	int res;
	int i;

	TRACE_ENTRY();

	res = -ENOMEM;
	for (i = 0; i < ring_size; ++i) {
		ioctx_ring[i] = srpt_alloc_ioctx(sdev);

		if (!ioctx_ring[i])
			goto err;

		WARN_ON(i & flags);
		ioctx_ring[i]->index = i | flags;
	}
	res = 0;
	goto out;

err:
	while (--i >= 0) {
		srpt_free_ioctx(sdev, ioctx_ring[i]);
		ioctx_ring[i] = NULL;
	}
out:
	TRACE_EXIT_RES(res);
	return res;
}

/* Free the ring of SRPT I/O context structures. */
static void srpt_free_ioctx_ring(struct srpt_device *sdev,
				 struct srpt_ioctx **ioctx_ring,
				 int ring_size)
{
	int i;

	for (i = 0; i < ring_size; ++i) {
		srpt_free_ioctx(sdev, ioctx_ring[i]);
		ioctx_ring[i] = NULL;
	}
}

/**
 * srpt_get_cmd_state() - Get the state of a SCSI command.
 */
static enum srpt_command_state srpt_get_cmd_state(struct srpt_ioctx *ioctx)
{
	BUG_ON(!ioctx);

	return atomic_read(&ioctx->state);
}

/**
 * srpt_set_cmd_state() - Set the state of a SCSI command.
 * @new: New state to be set.
 *
 * Does not modify the state of aborted commands. Returns the previous command
 * state.
 */
static enum srpt_command_state srpt_set_cmd_state(struct srpt_ioctx *ioctx,
						  enum srpt_command_state new)
{
	enum srpt_command_state previous;

	BUG_ON(!ioctx);
	WARN_ON(new == SRPT_STATE_NEW);

	do {
		previous = atomic_read(&ioctx->state);
	} while (previous != SRPT_STATE_DONE
	       && atomic_cmpxchg(&ioctx->state, previous, new) != previous);

	return previous;
}

/**
 * Test and set the state of a command.
 * @old: State to compare against.
 * @new: New state to be set if the current state matches 'old'.
 *
 * Returns the previous command state.
 */
static enum srpt_command_state
srpt_test_and_set_cmd_state(struct srpt_ioctx *ioctx,
			    enum srpt_command_state old,
			    enum srpt_command_state new)
{
	WARN_ON(!ioctx);
	WARN_ON(old == SRPT_STATE_DONE);
	WARN_ON(new == SRPT_STATE_NEW);

	return atomic_cmpxchg(&ioctx->state, old, new);
}

/**
 * Post a receive request on the work queue of InfiniBand device 'sdev'.
 */
static int srpt_post_recv(struct srpt_device *sdev, struct srpt_ioctx *ioctx)
{
	struct ib_sge list;
	struct ib_recv_wr wr, *bad_wr;

	wr.wr_id = ioctx->index | SRPT_OP_RECV;

	list.addr = ioctx->dma;
	list.length = srp_max_message_size;
	list.lkey = sdev->mr->lkey;

	wr.next = NULL;
	wr.sg_list = &list;
	wr.num_sge = 1;

	return ib_post_srq_recv(sdev->srq, &wr, &bad_wr);
}

/**
 * Post an IB send request.
 * @ch: RDMA channel to post the send request on.
 * @ioctx: I/O context of the send request.
 * @len: length of the request to be sent in bytes.
 *
 * Returns zero upon success and a non-zero value upon failure.
 */
static int srpt_post_send(struct srpt_rdma_ch *ch, struct srpt_ioctx *ioctx,
			  int len)
{
	struct ib_sge list;
	struct ib_send_wr wr, *bad_wr;
	struct srpt_device *sdev = ch->sport->sdev;
	int ret;

	ret = -ENOMEM;
	if (atomic_dec_return(&ch->qp_wr_avail) < 0) {
		PRINT_ERROR("%s[%d]: SRQ full", __func__, __LINE__);
		goto out;
	}

	ib_dma_sync_single_for_device(sdev->device, ioctx->dma,
				      len, DMA_TO_DEVICE);

	list.addr = ioctx->dma;
	list.length = len;
	list.lkey = sdev->mr->lkey;

	wr.next = NULL;
	wr.wr_id = ioctx->index;
	wr.sg_list = &list;
	wr.num_sge = 1;
	wr.opcode = IB_WR_SEND;
	wr.send_flags = IB_SEND_SIGNALED;

	ret = ib_post_send(ch->qp, &wr, &bad_wr);

out:
	if (ret < 0)
		atomic_inc(&ch->qp_wr_avail);
	return ret;
}

/**
 * srpt_get_desc_tbl() - Parse the data descriptors of an SRP_CMD request.
 * @ioctx: Pointer to the I/O context associated with the request.
 * @srp_cmd: Pointer to the SRP_CMD request data.
 * @dir: Pointer to the variable to which the transfer direction will be
 *   written.
 * @data_len: Pointer to the variable to which the total data length of all
 *   descriptors in the SRP_CMD request will be written.
 *
 * This function initializes ioctx->nrbuf and ioctx->r_bufs.
 *
 * Returns -EINVAL when the SRP_CMD request contains inconsistent descriptors;
 * -ENOMEM when memory allocation fails and zero upon success.
 */
static int srpt_get_desc_tbl(struct srpt_ioctx *ioctx, struct srp_cmd *srp_cmd,
			     scst_data_direction *dir, u64 *data_len)
{
	struct srp_indirect_buf *idb;
	struct srp_direct_buf *db;
	unsigned add_cdb_offset;
	int ret;

	/*
	 * The pointer computations below will only be compiled correctly
	 * if srp_cmd::add_data is declared as s8*, u8*, s8[] or u8[], so check
	 * whether srp_cmd::add_data has been declared as a byte pointer.
	 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 31)
	BUILD_BUG_ON(!__same_type(srp_cmd->add_data[0], (s8)0)
		     && !__same_type(srp_cmd->add_data[0], (u8)0));
#else
	/* Note: the __same_type() macro has been introduced in kernel 2.6.31.*/
#endif

	BUG_ON(!dir);
	BUG_ON(!data_len);

	ret = 0;
	*data_len = 0;

	/*
	 * The lower four bits of the buffer format field contain the DATA-IN
	 * buffer descriptor format, and the highest four bits contain the
	 * DATA-OUT buffer descriptor format.
	 */
	*dir = SCST_DATA_NONE;
	if (srp_cmd->buf_fmt & 0xf)
		/* DATA-IN: transfer data from target to initiator. */
		*dir = SCST_DATA_READ;
	else if (srp_cmd->buf_fmt >> 4)
		/* DATA-OUT: transfer data from initiator to target. */
		*dir = SCST_DATA_WRITE;

	/*
	 * According to the SRP spec, the lower two bits of the 'ADDITIONAL
	 * CDB LENGTH' field are reserved and the size in bytes of this field
	 * is four times the value specified in bits 3..7. Hence the "& ~3".
	 */
	add_cdb_offset = srp_cmd->add_cdb_len & ~3;
	if (((srp_cmd->buf_fmt & 0xf) == SRP_DATA_DESC_DIRECT) ||
	    ((srp_cmd->buf_fmt >> 4) == SRP_DATA_DESC_DIRECT)) {
		ioctx->n_rbuf = 1;
		ioctx->rbufs = &ioctx->single_rbuf;

		db = (struct srp_direct_buf *)(srp_cmd->add_data
					       + add_cdb_offset);
		memcpy(ioctx->rbufs, db, sizeof *db);
		*data_len = be32_to_cpu(db->len);
	} else if (((srp_cmd->buf_fmt & 0xf) == SRP_DATA_DESC_INDIRECT) ||
		   ((srp_cmd->buf_fmt >> 4) == SRP_DATA_DESC_INDIRECT)) {
		idb = (struct srp_indirect_buf *)(srp_cmd->add_data
						  + add_cdb_offset);

		ioctx->n_rbuf = be32_to_cpu(idb->table_desc.len) / sizeof *db;

		if (ioctx->n_rbuf >
		    (srp_cmd->data_out_desc_cnt + srp_cmd->data_in_desc_cnt)) {
			PRINT_ERROR("received corrupt SRP_CMD request"
				    " (%u out + %u in != %u / %zu)",
				    srp_cmd->data_out_desc_cnt,
				    srp_cmd->data_in_desc_cnt,
				    be32_to_cpu(idb->table_desc.len),
				    sizeof(*db));
			ioctx->n_rbuf = 0;
			ret = -EINVAL;
			goto out;
		}

		if (ioctx->n_rbuf == 1)
			ioctx->rbufs = &ioctx->single_rbuf;
		else {
			ioctx->rbufs =
				kmalloc(ioctx->n_rbuf * sizeof *db, GFP_ATOMIC);
			if (!ioctx->rbufs) {
				ioctx->n_rbuf = 0;
				ret = -ENOMEM;
				goto out;
			}
		}

		db = idb->desc_list;
		memcpy(ioctx->rbufs, db, ioctx->n_rbuf * sizeof *db);
		*data_len = be32_to_cpu(idb->len);
	}
out:
	return ret;
}

/*
 * Modify the attributes of queue pair 'qp': allow local write, remote read,
 * and remote write. Also transition 'qp' to state IB_QPS_INIT.
 */
static int srpt_init_ch_qp(struct srpt_rdma_ch *ch, struct ib_qp *qp)
{
	struct ib_qp_attr *attr;
	int ret;

	attr = kzalloc(sizeof *attr, GFP_KERNEL);
	if (!attr)
		return -ENOMEM;

	attr->qp_state = IB_QPS_INIT;
	attr->qp_access_flags = IB_ACCESS_LOCAL_WRITE | IB_ACCESS_REMOTE_READ |
	    IB_ACCESS_REMOTE_WRITE;
	attr->port_num = ch->sport->port;
	attr->pkey_index = 0;

	ret = ib_modify_qp(qp, attr,
			   IB_QP_STATE | IB_QP_ACCESS_FLAGS | IB_QP_PORT |
			   IB_QP_PKEY_INDEX);

	kfree(attr);
	return ret;
}

/**
 * Change the state of a channel to 'ready to receive' (RTR).
 * @ch: channel of the queue pair.
 * @qp: queue pair to change the state of.
 *
 * Returns zero upon success and a negative value upon failure.
 *
 * Note: currently a struct ib_qp_attr takes 136 bytes on a 64-bit system.
 * If this structure ever becomes larger, it might be necessary to allocate
 * it dynamically instead of on the stack.
 */
static int srpt_ch_qp_rtr(struct srpt_rdma_ch *ch, struct ib_qp *qp)
{
	struct ib_qp_attr qp_attr;
	int attr_mask;
	int ret;

	qp_attr.qp_state = IB_QPS_RTR;
	ret = ib_cm_init_qp_attr(ch->cm_id, &qp_attr, &attr_mask);
	if (ret)
		goto out;

	qp_attr.max_dest_rd_atomic = 4;

	ret = ib_modify_qp(qp, &qp_attr, attr_mask);

out:
	return ret;
}

/**
 * Change the state of a channel to 'ready to send' (RTS).
 * @ch: channel of the queue pair.
 * @qp: queue pair to change the state of.
 *
 * Returns zero upon success and a negative value upon failure.
 *
 * Note: currently a struct ib_qp_attr takes 136 bytes on a 64-bit system.
 * If this structure ever becomes larger, it might be necessary to allocate
 * it dynamically instead of on the stack.
 */
static int srpt_ch_qp_rts(struct srpt_rdma_ch *ch, struct ib_qp *qp)
{
	struct ib_qp_attr qp_attr;
	int attr_mask;
	int ret;

	qp_attr.qp_state = IB_QPS_RTS;
	ret = ib_cm_init_qp_attr(ch->cm_id, &qp_attr, &attr_mask);
	if (ret)
		goto out;

	qp_attr.max_rd_atomic = 4;

	ret = ib_modify_qp(qp, &qp_attr, attr_mask);

out:
	return ret;
}

/**
 * srpt_req_lim_delta() - Compute by how much req_lim changed since the
 * last time this function has been called. This value is necessary for
 * filling in the REQUEST LIMIT DELTA field of an SRP_RSP response.
 *
 * Side Effect: Modifies ch->last_response_req_lim.
 */
static int srpt_req_lim_delta(struct srpt_rdma_ch *ch)
{
	int req_lim;
	int last_rsp_req_lim;

	req_lim = atomic_read(&ch->req_lim);
	last_rsp_req_lim = atomic_xchg(&ch->last_response_req_lim, req_lim);
	return req_lim - last_rsp_req_lim;
}

static void srpt_reset_ioctx(struct srpt_rdma_ch *ch, struct srpt_ioctx *ioctx)
{
	BUG_ON(!ch);
	BUG_ON(!ioctx);

	/*
	 * If the WARN_ON() below gets triggered this means that
	 * srpt_unmap_sg_to_ib_sge() has not been called before
	 * scst_tgt_cmd_done().
	 */
	WARN_ON(ioctx->mapped_sg_count);

	if (ioctx->n_rbuf > 1) {
		kfree(ioctx->rbufs);
		ioctx->rbufs = NULL;
		ioctx->n_rbuf = 0;
	}

	if (srpt_post_recv(ch->sport->sdev, ioctx))
		PRINT_ERROR("%s", "SRQ post_recv failed - this is serious.");
		/* we should queue it back to free_ioctx queue */
	else {
		int req_lim;

		req_lim = atomic_inc_return(&ch->req_lim);
		if (req_lim < 0 || req_lim > SRPT_RQ_SIZE)
			PRINT_ERROR("req_lim = %d out of range %d .. %d",
				    req_lim, 0, SRPT_RQ_SIZE);
	}
}

/**
 * srpt_abort_scst_cmd() - Abort a SCSI command.
 */
static void srpt_abort_scst_cmd(struct srpt_ioctx *ioctx,
				enum scst_exec_context context)
{
	struct scst_cmd *scmnd;
	enum srpt_command_state state;

	TRACE_ENTRY();

	BUG_ON(!ioctx);

	state = srpt_set_cmd_state(ioctx, SRPT_STATE_DONE);
	if (state == SRPT_STATE_DONE)
		goto out;

	scmnd = ioctx->scmnd;
	WARN_ON(!scmnd);
	if (!scmnd)
		goto out;

	WARN_ON(ioctx != scst_cmd_get_tgt_priv(scmnd));

	TRACE_DBG("Aborting cmd with state %d and tag %lld",
		  state, scst_cmd_get_tag(scmnd));

	srpt_unmap_sg_to_ib_sge(ioctx->ch, ioctx);

	switch (state) {
	case SRPT_STATE_NEW:
		scst_set_delivery_status(scmnd, SCST_CMD_DELIVERY_ABORTED);
		break;
	case SRPT_STATE_NEED_DATA:
		scst_rx_data(scmnd, SCST_RX_STATUS_ERROR, context);
		break;
	case SRPT_STATE_DATA_IN:
	case SRPT_STATE_CMD_RSP_SENT:
		scst_set_delivery_status(scmnd, SCST_CMD_DELIVERY_ABORTED);
		scst_tgt_cmd_done(scmnd, context);
		break;
	case SRPT_STATE_MGMT_RSP_SENT:
		WARN_ON("ERROR: srpt_abort_scst_cmd() has been called for"
			" a management command.");
		scst_tgt_cmd_done(scmnd, context);
		break;
	default:
		WARN_ON("ERROR: unexpected command state");
		break;
	}

out:
	;

	TRACE_EXIT();
}

static void srpt_handle_err_comp(struct srpt_rdma_ch *ch, struct ib_wc *wc,
				 enum scst_exec_context context)
{
	struct srpt_ioctx *ioctx;
	struct srpt_device *sdev = ch->sport->sdev;

	TRACE_DBG("%s:%d wr_id = %#llx.", __func__, __LINE__, wc->wr_id);

	if (wc->wr_id & SRPT_OP_RECV) {
		ioctx = sdev->ioctx_ring[wc->wr_id & ~SRPT_OP_RECV];
		PRINT_ERROR("%s", "This is serious - SRQ is in bad state.");
	} else {
		ioctx = sdev->ioctx_ring[wc->wr_id];

		if (ioctx->scmnd)
			srpt_abort_scst_cmd(ioctx, context);
		else {
			PRINT_ERROR("%s: %s state=%d", __func__, "reset_ioctx",
				    srpt_get_cmd_state(ioctx));
			if (srpt_get_cmd_state(ioctx) != SRPT_STATE_DONE)
				srpt_reset_ioctx(ch, ioctx);
		}
	}
}

/** Process an IB send completion notification. */
static void srpt_handle_send_comp(struct srpt_rdma_ch *ch,
				  struct srpt_ioctx *ioctx,
				  enum scst_exec_context context)
{
	enum srpt_command_state state;
	struct scst_cmd *scmnd;

	state = srpt_get_cmd_state(ioctx);
	scmnd = ioctx->scmnd;

	WARN_ON(state != SRPT_STATE_CMD_RSP_SENT
		&& state != SRPT_STATE_MGMT_RSP_SENT);
	WARN_ON(state == SRPT_STATE_MGMT_RSP_SENT && scmnd);

	if (scmnd) {
		srpt_unmap_sg_to_ib_sge(ch, ioctx);
		srpt_set_cmd_state(ioctx, SRPT_STATE_DONE);
		scst_tgt_cmd_done(scmnd, context);
	} else {
		PRINT_ERROR("%s: %s state=%d",
			    __func__, "reset_ioctx", srpt_get_cmd_state(ioctx));
		srpt_reset_ioctx(ch, ioctx);
	}
}

/** Process an IB RDMA completion notification. */
static void srpt_handle_rdma_comp(struct srpt_rdma_ch *ch,
				  struct srpt_ioctx *ioctx,
				  enum scst_exec_context context)
{
	enum srpt_command_state state;
	struct scst_cmd *scmnd;

	scmnd = ioctx->scmnd;
	WARN_ON(!scmnd);
	if (!scmnd)
		return;

	state = srpt_test_and_set_cmd_state(ioctx, SRPT_STATE_NEED_DATA,
					    SRPT_STATE_DATA_IN);

	WARN_ON(state != SRPT_STATE_NEED_DATA);

	if (unlikely(scst_cmd_aborted(scmnd)))
		srpt_abort_scst_cmd(ioctx, context);
	else {
		srpt_unmap_sg_to_ib_sge(ch, ioctx);
		scst_rx_data(ioctx->scmnd, SCST_RX_STATUS_SUCCESS, context);
	}
}

/**
 * srpt_build_cmd_rsp() - Build an SRP_RSP response.
 * @ch: RDMA channel through which the request has been received.
 * @ioctx: I/O context associated with the SRP_CMD request. The response will
 *   be built in the buffer ioctx->buf points at and hence this function will
 *   overwrite the request data.
 * @tag: tag of the request for which this response is being generated.
 * @status: value for the STATUS field of the SRP_RSP information unit.
 * @sense_data: pointer to sense data to be included in the response.
 * @sense_data_len: length in bytes of the sense data.
 *
 * Returns the size in bytes of the SRP_RSP response.
 *
 * An SRP_RSP response contains a SCSI status or service response. See also
 * section 6.9 in the T10 SRP r16a document for the format of an SRP_RSP
 * response. See also SPC-2 for more information about sense data.
 */
static int srpt_build_cmd_rsp(struct srpt_rdma_ch *ch,
			      struct srpt_ioctx *ioctx, s32 req_lim_delta,
			      u64 tag, int status,
			      const u8 *sense_data, int sense_data_len)
{
	struct srp_rsp *srp_rsp;
	int max_sense_len;

	/*
	 * The lowest bit of all SAM-3 status codes is zero (see also
	 * paragraph 5.3 in SAM-3).
	 */
	WARN_ON(status & 1);

	srp_rsp = ioctx->buf;
	BUG_ON(!srp_rsp);
	memset(srp_rsp, 0, sizeof *srp_rsp);

	srp_rsp->opcode = SRP_RSP;
	srp_rsp->req_lim_delta = cpu_to_be32(req_lim_delta);
	srp_rsp->tag = tag;

	if (SCST_SENSE_VALID(sense_data)) {
		BUILD_BUG_ON(MIN_MAX_MESSAGE_SIZE <= sizeof(*srp_rsp));
		max_sense_len = ch->max_ti_iu_len - sizeof(*srp_rsp);
		if (sense_data_len > max_sense_len) {
			PRINT_WARNING("truncated sense data from %d to %d"
				" bytes", sense_data_len,
				max_sense_len);
			sense_data_len = max_sense_len;
		}

		srp_rsp->flags |= SRP_RSP_FLAG_SNSVALID;
		srp_rsp->status = status;
		srp_rsp->sense_data_len = cpu_to_be32(sense_data_len);
		memcpy(srp_rsp + 1, sense_data, sense_data_len);
	} else
		sense_data_len = 0;

	return sizeof(*srp_rsp) + sense_data_len;
}

/**
 * Build a task management response, which is a specific SRP_RSP response.
 * @ch: RDMA channel through which the request has been received.
 * @ioctx: I/O context in which the SRP_RSP response will be built.
 * @rsp_code: RSP_CODE that will be stored in the response.
 * @tag: tag of the request for which this response is being generated.
 *
 * Returns the size in bytes of the SRP_RSP response.
 *
 * An SRP_RSP response contains a SCSI status or service response. See also
 * section 6.9 in the T10 SRP r16a document for the format of an SRP_RSP
 * response.
 */
static int srpt_build_tskmgmt_rsp(struct srpt_rdma_ch *ch,
				  struct srpt_ioctx *ioctx, s32 req_lim_delta,
				  u8 rsp_code, u64 tag)
{
	struct srp_rsp *srp_rsp;
	int resp_data_len;
	int resp_len;

	resp_data_len = (rsp_code == SRP_TSK_MGMT_SUCCESS) ? 0 : 4;
	resp_len = sizeof(*srp_rsp) + resp_data_len;

	srp_rsp = ioctx->buf;
	memset(srp_rsp, 0, sizeof *srp_rsp);

	srp_rsp->opcode = SRP_RSP;
	srp_rsp->req_lim_delta = cpu_to_be32(req_lim_delta);
	srp_rsp->tag = tag;

	if (rsp_code != SRP_TSK_MGMT_SUCCESS) {
		srp_rsp->flags |= SRP_RSP_FLAG_RSPVALID;
		srp_rsp->resp_data_len = cpu_to_be32(resp_data_len);
		srp_rsp->data[3] = rsp_code;
	}

	return resp_len;
}

/*
 * Process SRP_CMD.
 */
static int srpt_handle_cmd(struct srpt_rdma_ch *ch, struct srpt_ioctx *ioctx,
			   enum scst_exec_context context)
{
	struct scst_cmd *scmnd;
	struct srp_cmd *srp_cmd;
	scst_data_direction dir;
	u64 data_len;
	int ret;

	srp_cmd = ioctx->buf;

	scmnd = scst_rx_cmd(ch->scst_sess, (u8 *) &srp_cmd->lun,
			    sizeof srp_cmd->lun, srp_cmd->cdb, 16, context);
	if (!scmnd)
		goto err;

	ioctx->scmnd = scmnd;

	ret = srpt_get_desc_tbl(ioctx, srp_cmd, &dir, &data_len);
	if (ret) {
		scst_set_cmd_error(scmnd,
			SCST_LOAD_SENSE(scst_sense_invalid_field_in_cdb));
		goto err;
	}

	switch (srp_cmd->task_attr) {
	case SRP_CMD_HEAD_OF_Q:
		scmnd->queue_type = SCST_CMD_QUEUE_HEAD_OF_QUEUE;
		break;
	case SRP_CMD_ORDERED_Q:
		scmnd->queue_type = SCST_CMD_QUEUE_ORDERED;
		break;
	case SRP_CMD_SIMPLE_Q:
		scmnd->queue_type = SCST_CMD_QUEUE_SIMPLE;
		break;
	case SRP_CMD_ACA:
		scmnd->queue_type = SCST_CMD_QUEUE_ACA;
		break;
	default:
		scmnd->queue_type = SCST_CMD_QUEUE_ORDERED;
		break;
	}

	scst_cmd_set_tag(scmnd, srp_cmd->tag);
	scst_cmd_set_tgt_priv(scmnd, ioctx);
	scst_cmd_set_expected(scmnd, dir, data_len);
	scst_cmd_init_done(scmnd, context);

	return 0;

err:
	return -1;
}

/*
 * srpt_handle_tsk_mgmt() - Process an SRP_TSK_MGMT information unit.
 *
 * Returns SRP_TSK_MGMT_SUCCESS upon success.
 *
 * Each task management function is performed by calling one of the
 * scst_rx_mgmt_fn*() functions. These functions will either report failure
 * or process the task management function asynchronously. The function
 * srpt_tsk_mgmt_done() will be called by the SCST core upon completion of the
 * task management function. When srpt_handle_tsk_mgmt() reports failure
 * (i.e. returns -1) a response will have been built in ioctx->buf. This
 * information unit has to be sent back by the caller.
 *
 * For more information about SRP_TSK_MGMT information units, see also section
 * 6.7 in the T10 SRP r16a document.
 */
static u8 srpt_handle_tsk_mgmt(struct srpt_rdma_ch *ch,
			       struct srpt_ioctx *ioctx)
{
	struct srp_tsk_mgmt *srp_tsk;
	struct srpt_mgmt_ioctx *mgmt_ioctx;
	int ret;
	u8 srp_tsk_mgmt_status;

	srp_tsk = ioctx->buf;

	TRACE_DBG("recv_tsk_mgmt= %d for task_tag= %lld"
		  " using tag= %lld cm_id= %p sess= %p",
		  srp_tsk->tsk_mgmt_func,
		  (unsigned long long) srp_tsk->task_tag,
		  (unsigned long long) srp_tsk->tag,
		  ch->cm_id, ch->scst_sess);

	srp_tsk_mgmt_status = SRP_TSK_MGMT_FAILED;
	mgmt_ioctx = kmalloc(sizeof *mgmt_ioctx, GFP_ATOMIC);
	if (!mgmt_ioctx)
		goto err;

	mgmt_ioctx->ioctx = ioctx;
	mgmt_ioctx->ch = ch;
	mgmt_ioctx->tag = srp_tsk->tag;

	switch (srp_tsk->tsk_mgmt_func) {
	case SRP_TSK_ABORT_TASK:
		TRACE_DBG("%s", "Processing SRP_TSK_ABORT_TASK");
		ret = scst_rx_mgmt_fn_tag(ch->scst_sess,
					  SCST_ABORT_TASK,
					  srp_tsk->task_tag,
					  SCST_ATOMIC, mgmt_ioctx);
		break;
	case SRP_TSK_ABORT_TASK_SET:
		TRACE_DBG("%s", "Processing SRP_TSK_ABORT_TASK_SET");
		ret = scst_rx_mgmt_fn_lun(ch->scst_sess,
					  SCST_ABORT_TASK_SET,
					  (u8 *) &srp_tsk->lun,
					  sizeof srp_tsk->lun,
					  SCST_ATOMIC, mgmt_ioctx);
		break;
	case SRP_TSK_CLEAR_TASK_SET:
		TRACE_DBG("%s", "Processing SRP_TSK_CLEAR_TASK_SET");
		ret = scst_rx_mgmt_fn_lun(ch->scst_sess,
					  SCST_CLEAR_TASK_SET,
					  (u8 *) &srp_tsk->lun,
					  sizeof srp_tsk->lun,
					  SCST_ATOMIC, mgmt_ioctx);
		break;
	case SRP_TSK_LUN_RESET:
		TRACE_DBG("%s", "Processing SRP_TSK_LUN_RESET");
		ret = scst_rx_mgmt_fn_lun(ch->scst_sess,
					  SCST_LUN_RESET,
					  (u8 *) &srp_tsk->lun,
					  sizeof srp_tsk->lun,
					  SCST_ATOMIC, mgmt_ioctx);
		break;
	case SRP_TSK_CLEAR_ACA:
		TRACE_DBG("%s", "Processing SRP_TSK_CLEAR_ACA");
		ret = scst_rx_mgmt_fn_lun(ch->scst_sess,
					  SCST_CLEAR_ACA,
					  (u8 *) &srp_tsk->lun,
					  sizeof srp_tsk->lun,
					  SCST_ATOMIC, mgmt_ioctx);
		break;
	default:
		TRACE_DBG("%s", "Unsupported task management function.");
		srp_tsk_mgmt_status = SRP_TSK_MGMT_FUNC_NOT_SUPP;
		goto err;
	}

	if (ret) {
		TRACE_DBG("Processing task management function failed"
			  " (ret = %d).", ret);
		goto err;
	}
	return SRP_TSK_MGMT_SUCCESS;

err:
	kfree(mgmt_ioctx);
	return srp_tsk_mgmt_status;
}

/**
 * Process a newly received information unit.
 * @ch: RDMA channel through which the information unit has been received.
 * @ioctx: SRPT I/O context associated with the information unit.
 */
static void srpt_handle_new_iu(struct srpt_rdma_ch *ch,
			       struct srpt_ioctx *ioctx)
{
	struct srp_cmd *srp_cmd;
	struct scst_cmd *scmnd;
	enum rdma_ch_state ch_state;
	u8 srp_response_status;
	u8 srp_tsk_mgmt_status;
	int len;
	int send_rsp_res;
	enum scst_exec_context context;

	ch_state = atomic_read(&ch->state);
	if (ch_state == RDMA_CHANNEL_CONNECTING) {
		list_add_tail(&ioctx->wait_list, &ch->cmd_wait_list);
		return;
	}

	ioctx->n_rbuf = 0;
	ioctx->rbufs = NULL;
	ioctx->n_rdma = 0;
	ioctx->n_rdma_ius = 0;
	ioctx->rdma_ius = NULL;
	ioctx->mapped_sg_count = 0;
	ioctx->scmnd = NULL;
	ioctx->ch = ch;
	atomic_set(&ioctx->state, SRPT_STATE_NEW);

	if (ch_state == RDMA_CHANNEL_DISCONNECTING) {
		PRINT_ERROR("%s: %s state=%d",
			    __func__, "reset_ioctx", srpt_get_cmd_state(ioctx));
		srpt_reset_ioctx(ch, ioctx);
		return;
	}

	WARN_ON(ch_state != RDMA_CHANNEL_LIVE);

	context = thread ? SCST_CONTEXT_THREAD : SCST_CONTEXT_TASKLET;

	scmnd = NULL;

	srp_response_status = SAM_STAT_BUSY;
	/* To keep the compiler happy. */
	srp_tsk_mgmt_status = -1;

	ib_dma_sync_single_for_cpu(ch->sport->sdev->device,
				   ioctx->dma, srp_max_message_size,
				   DMA_FROM_DEVICE);

	srp_cmd = ioctx->buf;

	switch (srp_cmd->opcode) {
	case SRP_CMD:
		if (srpt_handle_cmd(ch, ioctx, context) < 0) {
			scmnd = ioctx->scmnd;
			if (scmnd)
				srp_response_status =
					scst_cmd_get_status(scmnd);
			goto err;
		}
		break;

	case SRP_TSK_MGMT:
		srp_tsk_mgmt_status = srpt_handle_tsk_mgmt(ch, ioctx);
		if (srp_tsk_mgmt_status != SRP_TSK_MGMT_SUCCESS)
			goto err;
		break;

	case SRP_CRED_RSP:
		TRACE_DBG("%s", "received SRP_CRED_RSP");
		srpt_reset_ioctx(ch, ioctx);
		break;

	case SRP_AER_RSP:
		TRACE_DBG("%s", "received SRP_AER_RSP");
		srpt_reset_ioctx(ch, ioctx);
		break;

	case SRP_I_LOGOUT:
	default:
		goto err;
	}

	return;

err:
	send_rsp_res = -ENOTCONN;

	if (atomic_read(&ch->state) != RDMA_CHANNEL_LIVE) {
		/* Give up if another thread modified the channel state. */
		PRINT_ERROR("%s", "channel is no longer in connected state.");
	} else {
		s32 req_lim_delta;

		req_lim_delta = srpt_req_lim_delta(ch) + 1;
		if (srp_cmd->opcode == SRP_TSK_MGMT)
			len = srpt_build_tskmgmt_rsp(ch, ioctx, req_lim_delta,
				     srp_tsk_mgmt_status,
				     ((struct srp_tsk_mgmt *)srp_cmd)->tag);
		else if (scmnd)
			len = srpt_build_cmd_rsp(ch, ioctx, req_lim_delta,
				srp_cmd->tag, srp_response_status,
				scst_cmd_get_sense_buffer(scmnd),
				scst_cmd_get_sense_buffer_len(scmnd));
		else
			len = srpt_build_cmd_rsp(ch, ioctx, srp_cmd->tag,
						 req_lim_delta,
						 srp_response_status,
						 NULL, 0);
		srpt_set_cmd_state(ioctx,
				   srp_cmd->opcode == SRP_TSK_MGMT
				   ? SRPT_STATE_MGMT_RSP_SENT
				   : SRPT_STATE_CMD_RSP_SENT);
		send_rsp_res = srpt_post_send(ch, ioctx, len);
		if (send_rsp_res) {
			PRINT_ERROR("%s", "Sending SRP_RSP response failed.");
			atomic_sub(req_lim_delta, &ch->last_response_req_lim);
		}
	}
	if (send_rsp_res) {
		if (scmnd)
			srpt_abort_scst_cmd(ioctx, context);
		else {
			PRINT_ERROR("%s: %s state=%d", __func__, "reset_ioctx",
				    srpt_get_cmd_state(ioctx));
			srpt_reset_ioctx(ch, ioctx);
		}
	}
}

/**
 * InfiniBand completion queue callback function.
 * @cq: completion queue.
 * @ctx: completion queue context, which was passed as the fourth argument of
 *       the function ib_create_cq().
 */
static void srpt_completion(struct ib_cq *cq, void *ctx)
{
	struct srpt_rdma_ch *ch = ctx;
	struct srpt_device *sdev = ch->sport->sdev;
	struct ib_wc wc;
	struct srpt_ioctx *ioctx;
	enum scst_exec_context context;

	context = thread ? SCST_CONTEXT_THREAD : SCST_CONTEXT_TASKLET;

	ib_req_notify_cq(ch->cq, IB_CQ_NEXT_COMP);
	while (ib_poll_cq(ch->cq, 1, &wc) > 0) {
		if (wc.status) {
			PRINT_INFO("%s failed with status %d",
				   wc.wr_id & SRPT_OP_RECV
				   ? "receiving"
				   : "sending response",
				   wc.status);
			srpt_handle_err_comp(ch, &wc, context);
			continue;
		}

		if (wc.wr_id & SRPT_OP_RECV) {
			int req_lim;

			req_lim = atomic_dec_return(&ch->req_lim);
			if (req_lim < 0)
				PRINT_ERROR("req_lim = %d < 0", req_lim);
			ioctx = sdev->ioctx_ring[wc.wr_id & ~SRPT_OP_RECV];
			srpt_handle_new_iu(ch, ioctx);
		} else {
			ioctx = sdev->ioctx_ring[wc.wr_id];
			if (wc.opcode == IB_WC_SEND)
				atomic_inc(&ch->qp_wr_avail);
			else {
				WARN_ON(wc.opcode != IB_WC_RDMA_READ);
				WARN_ON(ioctx->n_rdma <= 0);
				atomic_add(ioctx->n_rdma,
					   &ch->qp_wr_avail);
			}
			switch (wc.opcode) {
			case IB_WC_SEND:
				srpt_handle_send_comp(ch, ioctx, context);
				break;
			case IB_WC_RDMA_WRITE:
			case IB_WC_RDMA_READ:
				srpt_handle_rdma_comp(ch, ioctx, context);
				break;
			default:
				PRINT_ERROR("received unrecognized"
					    " IB WC opcode %d",
					    wc.opcode);
				break;
			}
		}

#if defined(CONFIG_SCST_DEBUG)
		if (interrupt_processing_delay_in_us <= MAX_UDELAY_MS * 1000)
			udelay(interrupt_processing_delay_in_us);
#endif
	}
}

/*
 * Create a completion queue on the specified device.
 */
static int srpt_create_ch_ib(struct srpt_rdma_ch *ch)
{
	struct ib_qp_init_attr *qp_init;
	struct srpt_device *sdev = ch->sport->sdev;
	int cqe;
	int ret;

	qp_init = kzalloc(sizeof *qp_init, GFP_KERNEL);
	if (!qp_init)
		return -ENOMEM;

	/* Create a completion queue (CQ). */

	cqe = SRPT_RQ_SIZE + SRPT_SQ_SIZE - 1;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 20) && ! defined(RHEL_RELEASE_CODE)
	ch->cq = ib_create_cq(sdev->device, srpt_completion, NULL, ch, cqe);
#else
	ch->cq = ib_create_cq(sdev->device, srpt_completion, NULL, ch, cqe, 0);
#endif
	if (IS_ERR(ch->cq)) {
		ret = PTR_ERR(ch->cq);
		PRINT_ERROR("failed to create_cq cqe= %d ret= %d", cqe, ret);
		goto out;
	}

	/* Request completion notification. */

	ib_req_notify_cq(ch->cq, IB_CQ_NEXT_COMP);

	/* Create a queue pair (QP). */

	qp_init->qp_context = (void *)ch;
	qp_init->event_handler
		= (void(*)(struct ib_event *, void*))srpt_qp_event;
	qp_init->send_cq = ch->cq;
	qp_init->recv_cq = ch->cq;
	qp_init->srq = sdev->srq;
	qp_init->sq_sig_type = IB_SIGNAL_REQ_WR;
	qp_init->qp_type = IB_QPT_RC;
	qp_init->cap.max_send_wr = SRPT_SQ_SIZE;
	qp_init->cap.max_send_sge = SRPT_DEF_SG_PER_WQE;

	ch->qp = ib_create_qp(sdev->pd, qp_init);
	if (IS_ERR(ch->qp)) {
		ret = PTR_ERR(ch->qp);
		ib_destroy_cq(ch->cq);
		PRINT_ERROR("failed to create_qp ret= %d", ret);
		goto out;
	}

	atomic_set(&ch->qp_wr_avail, qp_init->cap.max_send_wr);

	TRACE_DBG("%s: max_cqe= %d max_sge= %d cm_id= %p",
	       __func__, ch->cq->cqe, qp_init->cap.max_send_sge,
	       ch->cm_id);

	/* Modify the attributes and the state of queue pair ch->qp. */

	ret = srpt_init_ch_qp(ch, ch->qp);
	if (ret) {
		ib_destroy_qp(ch->qp);
		ib_destroy_cq(ch->cq);
		goto out;
	}

out:
	kfree(qp_init);
	return ret;
}

/* Caller must hold ch->sdev->spinlock. */
static void srpt_unregister_channel(struct srpt_rdma_ch *ch)
	__acquires(&ch->sport->sdev->spinlock)
	__releases(&ch->sport->sdev->spinlock)
{
	struct srpt_device *sdev;
	sdev = ch->sport->sdev;

	list_del(&ch->list);
	atomic_set(&ch->state, RDMA_CHANNEL_DISCONNECTING);
	spin_unlock_irq(&sdev->spinlock);
	scst_unregister_session(ch->scst_sess, 0, srpt_release_channel);
	spin_lock_irq(&sdev->spinlock);
}

/**
 * Release the channel corresponding to the specified cm_id.
 *
 * Note: must be called from inside srpt_cm_handler to avoid a race between
 * accessing sdev->spinlock and the call to kfree(sdev) in srpt_remove_one()
 * (the caller of srpt_cm_handler holds the cm_id spinlock;
 * srpt_remove_one() waits until all SCST sessions for the associated
 * IB device have been unregistered and SCST session registration involves
 * a call to ib_destroy_cm_id(), which locks the cm_id spinlock and hence
 * waits until this function has finished).
 */
static void srpt_release_channel_by_cmid(struct ib_cm_id *cm_id)
{
	struct srpt_device *sdev;
	struct srpt_rdma_ch *ch;

	TRACE_ENTRY();

	sdev = cm_id->context;
	BUG_ON(!sdev);
	spin_lock_irq(&sdev->spinlock);
	list_for_each_entry(ch, &sdev->rch_list, list) {
		if (ch->cm_id == cm_id) {
			srpt_unregister_channel(ch);
			break;
		}
	}
	spin_unlock_irq(&sdev->spinlock);

	TRACE_EXIT();
}

/**
 * Look up the RDMA channel that corresponds to the specified cm_id.
 *
 * Return NULL if no matching RDMA channel has been found.
 */
static struct srpt_rdma_ch *srpt_find_channel(struct srpt_device *sdev,
					      struct ib_cm_id *cm_id)
{
	struct srpt_rdma_ch *ch;
	bool found;

	BUG_ON(!sdev);
	found = false;
	spin_lock_irq(&sdev->spinlock);
	list_for_each_entry(ch, &sdev->rch_list, list) {
		if (ch->cm_id == cm_id) {
			found = true;
			break;
		}
	}
	spin_unlock_irq(&sdev->spinlock);

	return found ? ch : NULL;
}

/**
 * Release all resources associated with an RDMA channel.
 *
 * Notes:
 * - The caller must have removed the channel from the channel list before
 *   calling this function.
 * - Must be called as a callback function via scst_unregister_session(). Never
 *   call this function directly because doing so would trigger several race
 *   conditions.
 * - Do not access ch->sport or ch->sport->sdev in this function because the
 *   memory that was allocated for the sport and/or sdev data structures may
 *   already have been freed at the time this function is called.
 */
static void srpt_release_channel(struct scst_session *scst_sess)
{
	struct srpt_rdma_ch *ch;

	TRACE_ENTRY();

	ch = scst_sess_get_tgt_priv(scst_sess);
	BUG_ON(!ch);
	WARN_ON(atomic_read(&ch->state) != RDMA_CHANNEL_DISCONNECTING);

	TRACE_DBG("destroying cm_id %p", ch->cm_id);
	BUG_ON(!ch->cm_id);
	ib_destroy_cm_id(ch->cm_id);

	ib_destroy_qp(ch->qp);
	ib_destroy_cq(ch->cq);
	kfree(ch);

	TRACE_EXIT();
}

/**
 * Process the event IB_CM_REQ_RECEIVED.
 *
 * Ownership of the cm_id is transferred to the SCST session if this functions
 * returns zero. Otherwise the caller remains the owner of cm_id.
 */
static int srpt_cm_req_recv(struct ib_cm_id *cm_id,
			    struct ib_cm_req_event_param *param,
			    void *private_data)
{
	struct srpt_device *sdev = cm_id->context;
	struct srp_login_req *req;
	struct srp_login_rsp *rsp;
	struct srp_login_rej *rej;
	struct ib_cm_rep_param *rep_param;
	struct srpt_rdma_ch *ch, *tmp_ch;
	u32 it_iu_len;
	int ret = 0;

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 18)
	WARN_ON(!sdev || !private_data);
	if (!sdev || !private_data)
		return -EINVAL;
#else
	if (WARN_ON(!sdev || !private_data))
		return -EINVAL;
#endif

	req = (struct srp_login_req *)private_data;

	it_iu_len = be32_to_cpu(req->req_it_iu_len);

	PRINT_INFO("Received SRP_LOGIN_REQ with"
	    " i_port_id 0x%llx:0x%llx, t_port_id 0x%llx:0x%llx and it_iu_len %d"
	    " on port %d (guid=0x%llx:0x%llx)",
	    (unsigned long long)be64_to_cpu(*(u64 *)&req->initiator_port_id[0]),
	    (unsigned long long)be64_to_cpu(*(u64 *)&req->initiator_port_id[8]),
	    (unsigned long long)be64_to_cpu(*(u64 *)&req->target_port_id[0]),
	    (unsigned long long)be64_to_cpu(*(u64 *)&req->target_port_id[8]),
	    it_iu_len,
	    param->port,
	    (unsigned long long)be64_to_cpu(*(u64 *)
				&sdev->port[param->port - 1].gid.raw[0]),
	    (unsigned long long)be64_to_cpu(*(u64 *)
				&sdev->port[param->port - 1].gid.raw[8]));

	rsp = kzalloc(sizeof *rsp, GFP_KERNEL);
	rej = kzalloc(sizeof *rej, GFP_KERNEL);
	rep_param = kzalloc(sizeof *rep_param, GFP_KERNEL);

	if (!rsp || !rej || !rep_param) {
		ret = -ENOMEM;
		goto out;
	}

	if (it_iu_len > srp_max_message_size || it_iu_len < 64) {
		rej->reason =
		    cpu_to_be32(SRP_LOGIN_REJ_REQ_IT_IU_LENGTH_TOO_LARGE);
		ret = -EINVAL;
		PRINT_ERROR("rejected SRP_LOGIN_REQ because its"
			    " length (%d bytes) is out of range (%d .. %d)",
			    it_iu_len, 64, srp_max_message_size);
		goto reject;
	}

	if ((req->req_flags & SRP_MTCH_ACTION) == SRP_MULTICHAN_SINGLE) {
		rsp->rsp_flags = SRP_LOGIN_RSP_MULTICHAN_NO_CHAN;

		spin_lock_irq(&sdev->spinlock);

		list_for_each_entry_safe(ch, tmp_ch, &sdev->rch_list, list) {
			if (!memcmp(ch->i_port_id, req->initiator_port_id, 16)
			    && !memcmp(ch->t_port_id, req->target_port_id, 16)
			    && param->port == ch->sport->port
			    && param->listen_id == ch->sport->sdev->cm_id
			    && ch->cm_id) {
				enum rdma_ch_state prev_state;

				/* found an existing channel */
				TRACE_DBG("Found existing channel name= %s"
					  " cm_id= %p state= %d",
					  ch->sess_name, ch->cm_id,
					  atomic_read(&ch->state));

				prev_state = atomic_xchg(&ch->state,
						RDMA_CHANNEL_DISCONNECTING);
				if (prev_state == RDMA_CHANNEL_CONNECTING)
					srpt_unregister_channel(ch);

				spin_unlock_irq(&sdev->spinlock);

				rsp->rsp_flags =
					SRP_LOGIN_RSP_MULTICHAN_TERMINATED;

				if (prev_state == RDMA_CHANNEL_LIVE) {
					ib_send_cm_dreq(ch->cm_id, NULL, 0);
					PRINT_INFO("disconnected"
					  " session %s because a new"
					  " SRP_LOGIN_REQ has been received.",
					  ch->sess_name);
				} else if (prev_state ==
					 RDMA_CHANNEL_CONNECTING) {
					PRINT_ERROR("%s", "rejected"
					  " SRP_LOGIN_REQ because another login"
					  " request is being processed.");
					ib_send_cm_rej(ch->cm_id,
						       IB_CM_REJ_NO_RESOURCES,
						       NULL, 0, NULL, 0);
				}

				spin_lock_irq(&sdev->spinlock);
			}
		}

		spin_unlock_irq(&sdev->spinlock);

	} else
		rsp->rsp_flags = SRP_LOGIN_RSP_MULTICHAN_MAINTAINED;

	if (((u64) (*(u64 *) req->target_port_id) !=
	     cpu_to_be64(srpt_service_guid)) ||
	    ((u64) (*(u64 *) (req->target_port_id + 8)) !=
	     cpu_to_be64(srpt_service_guid))) {
		rej->reason =
		    cpu_to_be32(SRP_LOGIN_REJ_UNABLE_ASSOCIATE_CHANNEL);
		ret = -ENOMEM;
		PRINT_ERROR("%s", "rejected SRP_LOGIN_REQ because it"
		       " has an invalid target port identifier.");
		goto reject;
	}

	ch = kzalloc(sizeof *ch, GFP_KERNEL);
	if (!ch) {
		rej->reason = cpu_to_be32(SRP_LOGIN_REJ_INSUFFICIENT_RESOURCES);
		PRINT_ERROR("%s",
			    "rejected SRP_LOGIN_REQ because out of memory.");
		ret = -ENOMEM;
		goto reject;
	}

	memcpy(ch->i_port_id, req->initiator_port_id, 16);
	memcpy(ch->t_port_id, req->target_port_id, 16);
	ch->sport = &sdev->port[param->port - 1];
	ch->cm_id = cm_id;
	atomic_set(&ch->state, RDMA_CHANNEL_CONNECTING);
	INIT_LIST_HEAD(&ch->cmd_wait_list);

	ret = srpt_create_ch_ib(ch);
	if (ret) {
		rej->reason = cpu_to_be32(SRP_LOGIN_REJ_INSUFFICIENT_RESOURCES);
		PRINT_ERROR("%s", "rejected SRP_LOGIN_REQ because creating"
			    " a new RDMA channel failed.");
		goto free_ch;
	}

	ret = srpt_ch_qp_rtr(ch, ch->qp);
	if (ret) {
		rej->reason = cpu_to_be32(SRP_LOGIN_REJ_INSUFFICIENT_RESOURCES);
		PRINT_ERROR("rejected SRP_LOGIN_REQ because enabling"
		       " RTR failed (error code = %d)", ret);
		goto destroy_ib;
	}

	if (use_port_guid_in_session_name) {
		/*
		 * If the kernel module parameter use_port_guid_in_session_name
		 * has been specified, use a combination of the target port
		 * GUID and the initiator port ID as the session name. This
		 * was the original behavior of the SRP target implementation
		 * (i.e. before the SRPT was included in OFED 1.3).
		 */
		snprintf(ch->sess_name, sizeof(ch->sess_name),
			 "0x%016llx%016llx",
			 (unsigned long long)be64_to_cpu(*(u64 *)
				&sdev->port[param->port - 1].gid.raw[8]),
			 (unsigned long long)be64_to_cpu(*(u64 *)
				(ch->i_port_id + 8)));
	} else {
		/*
		 * Default behavior: use the initator port identifier as the
		 * session name.
		 */
		snprintf(ch->sess_name, sizeof(ch->sess_name),
			 "0x%016llx%016llx",
			 (unsigned long long)be64_to_cpu(*(u64 *)ch->i_port_id),
			 (unsigned long long)be64_to_cpu(*(u64 *)
				 (ch->i_port_id + 8)));
	}

	TRACE_DBG("registering session %s", ch->sess_name);

	BUG_ON(!sdev->scst_tgt);
	ch->scst_sess = scst_register_session(sdev->scst_tgt, 0, ch->sess_name,
					      NULL, NULL);
	if (!ch->scst_sess) {
		rej->reason = cpu_to_be32(SRP_LOGIN_REJ_INSUFFICIENT_RESOURCES);
		TRACE_DBG("%s", "Failed to create scst sess");
		goto destroy_ib;
	}

	TRACE_DBG("Establish connection sess=%p name=%s cm_id=%p",
		  ch->scst_sess, ch->sess_name, ch->cm_id);

	scst_sess_set_tgt_priv(ch->scst_sess, ch);

	/* create srp_login_response */
	rsp->opcode = SRP_LOGIN_RSP;
	rsp->tag = req->tag;
	rsp->max_it_iu_len = req->req_it_iu_len;
	rsp->max_ti_iu_len = req->req_it_iu_len;
	ch->max_ti_iu_len = req->req_it_iu_len;
	rsp->buf_fmt =
	    cpu_to_be16(SRP_BUF_FORMAT_DIRECT | SRP_BUF_FORMAT_INDIRECT);
	rsp->req_lim_delta = cpu_to_be32(SRPT_RQ_SIZE);
	atomic_set(&ch->req_lim, SRPT_RQ_SIZE);
	atomic_set(&ch->last_response_req_lim, SRPT_RQ_SIZE);

	/* create cm reply */
	rep_param->qp_num = ch->qp->qp_num;
	rep_param->private_data = (void *)rsp;
	rep_param->private_data_len = sizeof *rsp;
	rep_param->rnr_retry_count = 7;
	rep_param->flow_control = 1;
	rep_param->failover_accepted = 0;
	rep_param->srq = 1;
	rep_param->responder_resources = 4;
	rep_param->initiator_depth = 4;

	ret = ib_send_cm_rep(cm_id, rep_param);
	if (ret) {
		PRINT_ERROR("sending SRP_LOGIN_REQ response failed"
			    " (error code = %d)", ret);
		goto release_channel;
	}

	spin_lock_irq(&sdev->spinlock);
	list_add_tail(&ch->list, &sdev->rch_list);
	spin_unlock_irq(&sdev->spinlock);

	goto out;

release_channel:
	atomic_set(&ch->state, RDMA_CHANNEL_DISCONNECTING);
	scst_unregister_session(ch->scst_sess, 0, NULL);
	ch->scst_sess = NULL;

destroy_ib:
	ib_destroy_qp(ch->qp);
	ib_destroy_cq(ch->cq);

free_ch:
	kfree(ch);

reject:
	rej->opcode = SRP_LOGIN_REJ;
	rej->tag = req->tag;
	rej->buf_fmt =
	    cpu_to_be16(SRP_BUF_FORMAT_DIRECT | SRP_BUF_FORMAT_INDIRECT);

	ib_send_cm_rej(cm_id, IB_CM_REJ_CONSUMER_DEFINED, NULL, 0,
			     (void *)rej, sizeof *rej);

out:
	kfree(rep_param);
	kfree(rsp);
	kfree(rej);

	return ret;
}

static void srpt_cm_rej_recv(struct ib_cm_id *cm_id)
{
	PRINT_INFO("Received InfiniBand REJ packet for cm_id %p.", cm_id);
	srpt_release_channel_by_cmid(cm_id);
}

/**
 * Process an IB_CM_RTU_RECEIVED or IB_CM_USER_ESTABLISHED event.
 *
 * An IB_CM_RTU_RECEIVED message indicates that the connection is established
 * and that the recipient may begin transmitting (RTU = ready to use).
 */
static void srpt_cm_rtu_recv(struct ib_cm_id *cm_id)
{
	struct srpt_rdma_ch *ch;
	int ret;

	ch = srpt_find_channel(cm_id->context, cm_id);
	WARN_ON(!ch);
	if (!ch)
		goto out;

	if (srpt_test_and_set_channel_state(ch, RDMA_CHANNEL_CONNECTING,
			RDMA_CHANNEL_LIVE) == RDMA_CHANNEL_CONNECTING) {
		struct srpt_ioctx *ioctx, *ioctx_tmp;

		ret = srpt_ch_qp_rts(ch, ch->qp);

		list_for_each_entry_safe(ioctx, ioctx_tmp, &ch->cmd_wait_list,
					 wait_list) {
			list_del(&ioctx->wait_list);
			srpt_handle_new_iu(ch, ioctx);
		}
		if (ret && srpt_test_and_set_channel_state(ch,
			RDMA_CHANNEL_LIVE,
			RDMA_CHANNEL_DISCONNECTING) == RDMA_CHANNEL_LIVE) {
			TRACE_DBG("cm_id=%p sess_name=%s state=%d",
				  cm_id, ch->sess_name,
				  atomic_read(&ch->state));
			ib_send_cm_dreq(ch->cm_id, NULL, 0);
		}
	}

out:
	;
}

static void srpt_cm_timewait_exit(struct ib_cm_id *cm_id)
{
	PRINT_INFO("Received InfiniBand TimeWait exit for cm_id %p.", cm_id);
	srpt_release_channel_by_cmid(cm_id);
}

static void srpt_cm_rep_error(struct ib_cm_id *cm_id)
{
	PRINT_INFO("Received InfiniBand REP error for cm_id %p.", cm_id);
	srpt_release_channel_by_cmid(cm_id);
}

static void srpt_cm_dreq_recv(struct ib_cm_id *cm_id)
{
	struct srpt_rdma_ch *ch;

	ch = srpt_find_channel(cm_id->context, cm_id);
	if (!ch) {
		TRACE_DBG("Received DREQ for channel %p which is already"
			  " being unregistered.", cm_id);
		goto out;
	}

	TRACE_DBG("cm_id= %p ch->state= %d", cm_id, atomic_read(&ch->state));

	switch (atomic_read(&ch->state)) {
	case RDMA_CHANNEL_LIVE:
	case RDMA_CHANNEL_CONNECTING:
		ib_send_cm_drep(ch->cm_id, NULL, 0);
		PRINT_INFO("Received DREQ and sent DREP for session %s.",
			   ch->sess_name);
		break;
	case RDMA_CHANNEL_DISCONNECTING:
	default:
		break;
	}

out:
	;
}

static void srpt_cm_drep_recv(struct ib_cm_id *cm_id)
{
	PRINT_INFO("Received InfiniBand DREP message for cm_id %p.", cm_id);
	srpt_release_channel_by_cmid(cm_id);
}

/**
 * IB connection manager callback function.
 *
 * A non-zero return value will cause the caller destroy the CM ID.
 *
 * Note: srpt_cm_handler() must only return a non-zero value when transferring
 * ownership of the cm_id to a channel by srpt_cm_req_recv() failed. Returning
 * a non-zero value in any other case will trigger a race with the
 * ib_destroy_cm_id() call in srpt_release_channel().
 */
static int srpt_cm_handler(struct ib_cm_id *cm_id, struct ib_cm_event *event)
{
	int ret;

	ret = 0;
	switch (event->event) {
	case IB_CM_REQ_RECEIVED:
		ret = srpt_cm_req_recv(cm_id, &event->param.req_rcvd,
				       event->private_data);
		break;
	case IB_CM_REJ_RECEIVED:
		srpt_cm_rej_recv(cm_id);
		break;
	case IB_CM_RTU_RECEIVED:
	case IB_CM_USER_ESTABLISHED:
		srpt_cm_rtu_recv(cm_id);
		break;
	case IB_CM_DREQ_RECEIVED:
		srpt_cm_dreq_recv(cm_id);
		break;
	case IB_CM_DREP_RECEIVED:
		srpt_cm_drep_recv(cm_id);
		break;
	case IB_CM_TIMEWAIT_EXIT:
		srpt_cm_timewait_exit(cm_id);
		break;
	case IB_CM_REP_ERROR:
		srpt_cm_rep_error(cm_id);
		break;
	default:
		PRINT_ERROR("received unrecognized IB CM event %d",
			    event->event);
		break;
	}

	return ret;
}

static int srpt_map_sg_to_ib_sge(struct srpt_rdma_ch *ch,
				 struct srpt_ioctx *ioctx,
				 struct scst_cmd *scmnd)
{
	struct scatterlist *scat;
	scst_data_direction dir;
	struct rdma_iu *riu;
	struct srp_direct_buf *db;
	dma_addr_t dma_addr;
	struct ib_sge *sge;
	u64 raddr;
	u32 rsize;
	u32 tsize;
	u32 dma_len;
	int count, nrdma;
	int i, j, k;

	BUG_ON(!ch);
	BUG_ON(!ioctx);
	BUG_ON(!scmnd);
	scat = scst_cmd_get_sg(scmnd);
	WARN_ON(!scat);
	dir = scst_cmd_get_data_direction(scmnd);
	count = ib_dma_map_sg(ch->sport->sdev->device, scat,
			      scst_cmd_get_sg_cnt(scmnd),
			      scst_to_tgt_dma_dir(dir));
	if (unlikely(!count))
		return -EBUSY;

	ioctx->mapped_sg_count = count;

	if (ioctx->rdma_ius && ioctx->n_rdma_ius)
		nrdma = ioctx->n_rdma_ius;
	else {
		nrdma = count / SRPT_DEF_SG_PER_WQE + ioctx->n_rbuf;

		ioctx->rdma_ius = kzalloc(nrdma * sizeof *riu,
					  scst_cmd_atomic(scmnd)
					  ? GFP_ATOMIC : GFP_KERNEL);
		if (!ioctx->rdma_ius)
			goto free_mem;

		ioctx->n_rdma_ius = nrdma;
	}

	db = ioctx->rbufs;
	tsize = (dir == SCST_DATA_READ) ?
		scst_cmd_get_resp_data_len(scmnd) : scst_cmd_get_bufflen(scmnd);
	dma_len = sg_dma_len(&scat[0]);
	riu = ioctx->rdma_ius;

	/*
	 * For each remote desc - calculate the #ib_sge.
	 * If #ib_sge < SRPT_DEF_SG_PER_WQE per rdma operation then
	 *      each remote desc rdma_iu is required a rdma wr;
	 * else
	 *      we need to allocate extra rdma_iu to carry extra #ib_sge in
	 *      another rdma wr
	 */
	for (i = 0, j = 0;
	     j < count && i < ioctx->n_rbuf && tsize > 0; ++i, ++riu, ++db) {
		rsize = be32_to_cpu(db->len);
		raddr = be64_to_cpu(db->va);
		riu->raddr = raddr;
		riu->rkey = be32_to_cpu(db->key);
		riu->sge_cnt = 0;

		/* calculate how many sge required for this remote_buf */
		while (rsize > 0 && tsize > 0) {

			if (rsize >= dma_len) {
				tsize -= dma_len;
				rsize -= dma_len;
				raddr += dma_len;

				if (tsize > 0) {
					++j;
					if (j < count)
						dma_len = sg_dma_len(&scat[j]);
				}
			} else {
				tsize -= rsize;
				dma_len -= rsize;
				rsize = 0;
			}

			++riu->sge_cnt;

			if (rsize > 0 && riu->sge_cnt == SRPT_DEF_SG_PER_WQE) {
				++ioctx->n_rdma;
				riu->sge =
				    kmalloc(riu->sge_cnt * sizeof *riu->sge,
					    scst_cmd_atomic(scmnd)
					    ? GFP_ATOMIC : GFP_KERNEL);
				if (!riu->sge)
					goto free_mem;

				++riu;
				riu->sge_cnt = 0;
				riu->raddr = raddr;
				riu->rkey = be32_to_cpu(db->key);
			}
		}

		++ioctx->n_rdma;
		riu->sge = kmalloc(riu->sge_cnt * sizeof *riu->sge,
				   scst_cmd_atomic(scmnd)
				   ? GFP_ATOMIC : GFP_KERNEL);
		if (!riu->sge)
			goto free_mem;
	}

	db = ioctx->rbufs;
	scat = scst_cmd_get_sg(scmnd);
	tsize = (dir == SCST_DATA_READ) ?
		scst_cmd_get_resp_data_len(scmnd) : scst_cmd_get_bufflen(scmnd);
	riu = ioctx->rdma_ius;
	dma_len = sg_dma_len(&scat[0]);
	dma_addr = sg_dma_address(&scat[0]);

	/* this second loop is really mapped sg_addres to rdma_iu->ib_sge */
	for (i = 0, j = 0;
	     j < count && i < ioctx->n_rbuf && tsize > 0; ++i, ++riu, ++db) {
		rsize = be32_to_cpu(db->len);
		sge = riu->sge;
		k = 0;

		while (rsize > 0 && tsize > 0) {
			sge->addr = dma_addr;
			sge->lkey = ch->sport->sdev->mr->lkey;

			if (rsize >= dma_len) {
				sge->length =
					(tsize < dma_len) ? tsize : dma_len;
				tsize -= dma_len;
				rsize -= dma_len;

				if (tsize > 0) {
					++j;
					if (j < count) {
						dma_len = sg_dma_len(&scat[j]);
						dma_addr =
						    sg_dma_address(&scat[j]);
					}
				}
			} else {
				sge->length = (tsize < rsize) ? tsize : rsize;
				tsize -= rsize;
				dma_len -= rsize;
				dma_addr += rsize;
				rsize = 0;
			}

			++k;
			if (k == riu->sge_cnt && rsize > 0) {
				++riu;
				sge = riu->sge;
				k = 0;
			} else if (rsize > 0)
				++sge;
		}
	}

	return 0;

free_mem:
	srpt_unmap_sg_to_ib_sge(ch, ioctx);

	return -ENOMEM;
}

static void srpt_unmap_sg_to_ib_sge(struct srpt_rdma_ch *ch,
				    struct srpt_ioctx *ioctx)
{
	struct scst_cmd *scmnd;
	struct scatterlist *scat;
	scst_data_direction dir;

	BUG_ON(!ch);
	BUG_ON(!ioctx);
	BUG_ON(ioctx->n_rdma && !ioctx->rdma_ius);

	while (ioctx->n_rdma)
		kfree(ioctx->rdma_ius[--ioctx->n_rdma].sge);

	kfree(ioctx->rdma_ius);
	ioctx->rdma_ius = NULL;

	if (ioctx->mapped_sg_count) {
		scmnd = ioctx->scmnd;
		BUG_ON(!scmnd);
		WARN_ON(ioctx->scmnd != scmnd);
		WARN_ON(ioctx != scst_cmd_get_tgt_priv(scmnd));
		scat = scst_cmd_get_sg(scmnd);
		WARN_ON(!scat);
		dir = scst_cmd_get_data_direction(scmnd);
		WARN_ON(dir == SCST_DATA_NONE);
		ib_dma_unmap_sg(ch->sport->sdev->device, scat,
				scst_cmd_get_sg_cnt(scmnd),
				scst_to_tgt_dma_dir(dir));
		ioctx->mapped_sg_count = 0;
	}
}

static int srpt_perform_rdmas(struct srpt_rdma_ch *ch, struct srpt_ioctx *ioctx,
			      scst_data_direction dir)
{
	struct ib_send_wr wr;
	struct ib_send_wr *bad_wr;
	struct rdma_iu *riu;
	int i;
	int ret;
	int srq_wr_avail;

	if (dir == SCST_DATA_WRITE) {
		ret = -ENOMEM;
		srq_wr_avail = atomic_sub_return(ioctx->n_rdma,
						 &ch->qp_wr_avail);
		if (srq_wr_avail < 0) {
			atomic_add(ioctx->n_rdma, &ch->qp_wr_avail);
			PRINT_INFO("%s[%d]: SRQ full", __func__, __LINE__);
			goto out;
		}
	}

	ret = 0;
	riu = ioctx->rdma_ius;
	memset(&wr, 0, sizeof wr);

	for (i = 0; i < ioctx->n_rdma; ++i, ++riu) {
		wr.opcode = (dir == SCST_DATA_READ) ?
		    IB_WR_RDMA_WRITE : IB_WR_RDMA_READ;
		wr.next = NULL;
		wr.wr_id = ioctx->index;
		wr.wr.rdma.remote_addr = riu->raddr;
		wr.wr.rdma.rkey = riu->rkey;
		wr.num_sge = riu->sge_cnt;
		wr.sg_list = riu->sge;

		/* only get completion event for the last rdma wr */
		if (i == (ioctx->n_rdma - 1) && dir == SCST_DATA_WRITE)
			wr.send_flags = IB_SEND_SIGNALED;

		ret = ib_post_send(ch->qp, &wr, &bad_wr);
		if (ret)
			goto out;
	}

out:
	return ret;
}

/*
 * Start data transfer between initiator and target. Must not block.
 */
static int srpt_xfer_data(struct srpt_rdma_ch *ch, struct srpt_ioctx *ioctx,
			  struct scst_cmd *scmnd)
{
	int ret;

	ret = srpt_map_sg_to_ib_sge(ch, ioctx, scmnd);
	if (ret) {
		PRINT_ERROR("%s[%d] ret=%d", __func__, __LINE__, ret);
		ret = SCST_TGT_RES_QUEUE_FULL;
		goto out;
	}

	ret = srpt_perform_rdmas(ch, ioctx, scst_cmd_get_data_direction(scmnd));
	if (ret) {
		if (ret == -EAGAIN || ret == -ENOMEM) {
			PRINT_INFO("%s[%d] queue full -- ret=%d",
				   __func__, __LINE__, ret);
			ret = SCST_TGT_RES_QUEUE_FULL;
		} else {
			PRINT_ERROR("%s[%d] fatal error -- ret=%d",
				    __func__, __LINE__, ret);
			ret = SCST_TGT_RES_FATAL_ERROR;
		}
		goto out_unmap;
	}

	ret = SCST_TGT_RES_SUCCESS;

out:
	return ret;
out_unmap:
	srpt_unmap_sg_to_ib_sge(ch, ioctx);
	goto out;
}

/*
 * Called by the SCST core to inform ib_srpt that data reception from the
 * initiator should start (SCST_DATA_WRITE). Must not block.
 */
static int srpt_rdy_to_xfer(struct scst_cmd *scmnd)
{
	struct srpt_rdma_ch *ch;
	struct srpt_ioctx *ioctx;
	enum rdma_ch_state ch_state;
	int ret;

	ioctx = scst_cmd_get_tgt_priv(scmnd);
	BUG_ON(!ioctx);

	WARN_ON(srpt_set_cmd_state(ioctx, SRPT_STATE_NEED_DATA)
		== SRPT_STATE_DONE);

	ch = ioctx->ch;
	WARN_ON(ch != scst_sess_get_tgt_priv(scst_cmd_get_session(scmnd)));
	BUG_ON(!ch);

	ch_state = atomic_read(&ch->state);
	if (ch_state == RDMA_CHANNEL_DISCONNECTING) {
		TRACE_DBG("cmd with tag %lld: channel disconnecting",
			  scst_cmd_get_tag(scmnd));
		ret = SCST_TGT_RES_FATAL_ERROR;
		goto out;
	} else if (ch_state == RDMA_CHANNEL_CONNECTING) {
		ret = SCST_TGT_RES_QUEUE_FULL;
		goto out;
	}
	ret = srpt_xfer_data(ch, ioctx, scmnd);

out:
	return ret;
}

/**
 * srpt_must_wait_for_cred() - Whether or not the target must wait with
 * sending a response towards the initiator in order to avoid that the
 * initiator locks up. The Linux SRP initiator locks up when:
 * initiator.req_lim < req_lim_min (2 for SRP_CMD; 1 for SRP_TSK_MGMT).
 * no new SRP_RSP is received, or new SRP_RSP do not increase initiator.req_lim.
 * In order to avoid an initiator lock up, the target must not send an SRP_RSP
 * that keeps initiator.req_lim < req_lim_min when initiator.req_lim
 * < req_lim_min. when target.req_lim == req_lim_min - 1, initiator.req_lim must
 * also equal req_lim_min - 1 because of the credit mechanism defined in the
 * SRP standard. Hence wait with sending a response if that response would not
 * increase initiator.req_lim.
 */
static bool srpt_must_wait_for_cred(struct srpt_rdma_ch *ch, int req_lim_min)
{
	int req_lim;
	req_lim = atomic_read(&ch->req_lim);

	return req_lim < req_lim_min
		&& req_lim - atomic_read(&ch->last_response_req_lim) + 1 <= 0;
}

static void srpt_wait_for_cred(struct srpt_rdma_ch *ch, int req_lim_min)
{
	while (unlikely(srpt_must_wait_for_cred(ch, req_lim_min)))
		schedule();
}

/**
 * srpt_xmit_response() - SCST callback function that transmits the response
 * to a SCSI command.
 *
 * Must not block.
 */
static int srpt_xmit_response(struct scst_cmd *scmnd)
{
	struct srpt_rdma_ch *ch;
	struct srpt_ioctx *ioctx;
	s32 req_lim_delta;
	int ret = SCST_TGT_RES_SUCCESS;
	int dir;
	int resp_len;

	ioctx = scst_cmd_get_tgt_priv(scmnd);
	BUG_ON(!ioctx);

	ch = scst_sess_get_tgt_priv(scst_cmd_get_session(scmnd));
	BUG_ON(!ch);

	if (unlikely(scst_cmd_aborted(scmnd))) {
		TRACE_DBG("cmd with tag %lld has been aborted",
			  scst_cmd_get_tag(scmnd));
		srpt_abort_scst_cmd(ioctx, SCST_CONTEXT_SAME);
		ret = SCST_TGT_RES_SUCCESS;
		goto out;
	}

	if (unlikely(scst_cmd_atomic(scmnd))) {
		TRACE_DBG("%s", "Switching to thread context.");
		ret = SCST_TGT_RES_NEED_THREAD_CTX;
		goto out;
	}

	srpt_wait_for_cred(ch, 2);

	WARN_ON(srpt_set_cmd_state(ioctx, SRPT_STATE_CMD_RSP_SENT)
		== SRPT_STATE_DONE);

	dir = scst_cmd_get_data_direction(scmnd);

	/* For read commands, transfer the data to the initiator. */
	if (dir == SCST_DATA_READ && scst_cmd_get_resp_data_len(scmnd)) {
		ret = srpt_xfer_data(ch, ioctx, scmnd);
		if (ret != SCST_TGT_RES_SUCCESS) {
			PRINT_ERROR("%s: tag= %lld xfer_data failed",
				    __func__,
				    (unsigned long long)
				    scst_cmd_get_tag(scmnd));
			goto out;
		}
	}

	scst_check_convert_sense(scmnd);

	req_lim_delta = srpt_req_lim_delta(ch) + 1;
	resp_len = srpt_build_cmd_rsp(ch, ioctx, req_lim_delta,
				      scst_cmd_get_tag(scmnd),
				      scst_cmd_get_status(scmnd),
				      scst_cmd_get_sense_buffer(scmnd),
				      scst_cmd_get_sense_buffer_len(scmnd));

	if (srpt_post_send(ch, ioctx, resp_len)) {
		srpt_unmap_sg_to_ib_sge(ch, ioctx);
		srpt_set_cmd_state(ioctx, SRPT_STATE_DONE);
		scst_set_delivery_status(scmnd, SCST_CMD_DELIVERY_FAILED);
		scst_tgt_cmd_done(scmnd, SCST_CONTEXT_SAME);
		PRINT_ERROR("%s[%d]: ch->state= %d tag= %lld",
			    __func__, __LINE__, atomic_read(&ch->state),
			    (unsigned long long)scst_cmd_get_tag(scmnd));
		atomic_sub(req_lim_delta, &ch->last_response_req_lim);
		ret = SCST_TGT_RES_FATAL_ERROR;
	}

out:
	return ret;
}

/**
 * srpt_tsk_mgmt_done() - SCST callback function that sends back the response
 * for a task management request.
 *
 * Must not block.
 */
static void srpt_tsk_mgmt_done(struct scst_mgmt_cmd *mcmnd)
{
	struct srpt_rdma_ch *ch;
	struct srpt_mgmt_ioctx *mgmt_ioctx;
	struct srpt_ioctx *ioctx;
	s32 req_lim_delta;
	int rsp_len;

	mgmt_ioctx = scst_mgmt_cmd_get_tgt_priv(mcmnd);
	BUG_ON(!mgmt_ioctx);

	ch = mgmt_ioctx->ch;
	BUG_ON(!ch);

	ioctx = mgmt_ioctx->ioctx;
	BUG_ON(!ioctx);

	TRACE_DBG("%s: tsk_mgmt_done for tag= %lld status=%d",
		  __func__, (unsigned long long)mgmt_ioctx->tag,
		  scst_mgmt_cmd_get_status(mcmnd));

	WARN_ON(in_irq());

	srpt_wait_for_cred(ch, 1);

	WARN_ON(srpt_set_cmd_state(ioctx, SRPT_STATE_MGMT_RSP_SENT)
		== SRPT_STATE_DONE);

	req_lim_delta = srpt_req_lim_delta(ch) + 1;
	rsp_len = srpt_build_tskmgmt_rsp(ch, ioctx, req_lim_delta,
					 (scst_mgmt_cmd_get_status(mcmnd) ==
					  SCST_MGMT_STATUS_SUCCESS) ?
					 SRP_TSK_MGMT_SUCCESS :
					 SRP_TSK_MGMT_FAILED,
					 mgmt_ioctx->tag);
	/*
	 * Note: the srpt_post_send() call below sends the task management
	 * response asynchronously. It is possible that the SCST core has
	 * already freed the struct scst_mgmt_cmd structure before the
	 * response is sent. This is fine.
	 */
	if (srpt_post_send(ch, ioctx, rsp_len)) {
		PRINT_ERROR("%s", "Sending SRP_RSP response failed.");
		atomic_sub(req_lim_delta, &ch->last_response_req_lim);
	}

	scst_mgmt_cmd_set_tgt_priv(mcmnd, NULL);

	kfree(mgmt_ioctx);
}

/*
 * Called by the SCST core to inform ib_srpt that the command 'scmnd' is about
 * to be freed. May be called in IRQ context.
 */
static void srpt_on_free_cmd(struct scst_cmd *scmnd)
{
	struct srpt_rdma_ch *ch;
	struct srpt_ioctx *ioctx;

	ioctx = scst_cmd_get_tgt_priv(scmnd);
	BUG_ON(!ioctx);

	WARN_ON(srpt_get_cmd_state(ioctx) != SRPT_STATE_DONE);

	ch = ioctx->ch;
	BUG_ON(!ch);

	srpt_reset_ioctx(ch, ioctx);

	scst_cmd_set_tgt_priv(scmnd, NULL);
	ioctx->scmnd = NULL;
	ioctx->ch = NULL;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 20) && ! defined(BACKPORT_LINUX_WORKQUEUE_TO_2_6_19)
/* A vanilla 2.6.19 or older kernel without backported OFED kernel headers. */
static void srpt_refresh_port_work(void *ctx)
#else
static void srpt_refresh_port_work(struct work_struct *work)
#endif
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 20) && ! defined(BACKPORT_LINUX_WORKQUEUE_TO_2_6_19)
	struct srpt_port *sport = (struct srpt_port *)ctx;
#else
	struct srpt_port *sport = container_of(work, struct srpt_port, work);
#endif

	srpt_refresh_port(sport);
}

/*
 * Called by the SCST core to detect target adapters. Returns the number of
 * detected target adapters.
 */
static int srpt_detect(struct scst_tgt_template *tp)
{
	int device_count;

	TRACE_ENTRY();

	device_count = atomic_read(&srpt_device_count);

	TRACE_EXIT_RES(device_count);

	return device_count;
}

/*
 * Callback function called by the SCST core from scst_unregister() to free up
 * the resources associated with device scst_tgt.
 */
static int srpt_release(struct scst_tgt *scst_tgt)
{
	struct srpt_device *sdev = scst_tgt_get_tgt_priv(scst_tgt);
	struct srpt_rdma_ch *ch;

	TRACE_ENTRY();

	BUG_ON(!scst_tgt);
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 18)
	WARN_ON(!sdev);
	if (!sdev)
		return -ENODEV;
#else
	if (WARN_ON(!sdev))
		return -ENODEV;
#endif

#ifdef CONFIG_SCST_PROC
	srpt_unregister_procfs_entry(scst_tgt->tgtt);
#endif /*CONFIG_SCST_PROC*/

	spin_lock_irq(&sdev->spinlock);
	while (!list_empty(&sdev->rch_list)) {
		ch = list_first_entry(&sdev->rch_list, typeof(*ch), list);
		srpt_unregister_channel(ch);
	}
	spin_unlock_irq(&sdev->spinlock);

	scst_tgt_set_tgt_priv(scst_tgt, NULL);

	TRACE_EXIT();

	return 0;
}

/* SCST target template for the SRP target implementation. */
static struct scst_tgt_template srpt_template = {
	.name = DRV_NAME,
	.sg_tablesize = SRPT_DEF_SG_TABLESIZE,
	.detect = srpt_detect,
	.release = srpt_release,
	.xmit_response = srpt_xmit_response,
	.rdy_to_xfer = srpt_rdy_to_xfer,
	.on_free_cmd = srpt_on_free_cmd,
	.task_mgmt_fn_done = srpt_tsk_mgmt_done
};

/*
 * The callback function srpt_release_class_dev() is called whenever a
 * device is removed from the /sys/class/infiniband_srpt device class.
 * Although this function has been left empty, a release function has been
 * defined such that upon module removal no complaint is logged about a
 * missing release function.
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 26)
static void srpt_release_class_dev(struct class_device *class_dev)
#else
static void srpt_release_class_dev(struct device *dev)
#endif
{
}

#ifdef CONFIG_SCST_PROC

#if defined(CONFIG_SCST_DEBUG) || defined(CONFIG_SCST_TRACING)
static int srpt_trace_level_show(struct seq_file *seq, void *v)
{
	return scst_proc_log_entry_read(seq, trace_flag, NULL);
}

static ssize_t srpt_proc_trace_level_write(struct file *file,
	const char __user *buf, size_t length, loff_t *off)
{
	return scst_proc_log_entry_write(file, buf, length, &trace_flag,
		DEFAULT_SRPT_TRACE_FLAGS, NULL);
}

static struct scst_proc_data srpt_log_proc_data = {
	SCST_DEF_RW_SEQ_OP(srpt_proc_trace_level_write)
	.show = srpt_trace_level_show,
};
#endif

#endif /* CONFIG_SCST_PROC */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 26)
static ssize_t show_login_info(struct class_device *class_dev, char *buf)
#else
static ssize_t show_login_info(struct device *dev,
			       struct device_attribute *attr, char *buf)
#endif
{
	struct srpt_device *sdev =
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 26)
		container_of(class_dev, struct srpt_device, class_dev);
#else
		container_of(dev, struct srpt_device, dev);
#endif
	struct srpt_port *sport;
	int i;
	int len = 0;

	for (i = 0; i < sdev->device->phys_port_cnt; i++) {
		sport = &sdev->port[i];

		len += sprintf(buf + len,
			       "tid_ext=%016llx,ioc_guid=%016llx,pkey=ffff,"
			       "dgid=%04x%04x%04x%04x%04x%04x%04x%04x,"
			       "service_id=%016llx\n",
			       (unsigned long long) srpt_service_guid,
			       (unsigned long long) srpt_service_guid,
			       be16_to_cpu(((__be16 *) sport->gid.raw)[0]),
			       be16_to_cpu(((__be16 *) sport->gid.raw)[1]),
			       be16_to_cpu(((__be16 *) sport->gid.raw)[2]),
			       be16_to_cpu(((__be16 *) sport->gid.raw)[3]),
			       be16_to_cpu(((__be16 *) sport->gid.raw)[4]),
			       be16_to_cpu(((__be16 *) sport->gid.raw)[5]),
			       be16_to_cpu(((__be16 *) sport->gid.raw)[6]),
			       be16_to_cpu(((__be16 *) sport->gid.raw)[7]),
			       (unsigned long long) srpt_service_guid);
	}

	return len;
}

static struct class_attribute srpt_class_attrs[] = {
	__ATTR_NULL,
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 26)
static struct class_device_attribute srpt_dev_attrs[] = {
#else
static struct device_attribute srpt_dev_attrs[] = {
#endif
	__ATTR(login_info, S_IRUGO, show_login_info, NULL),
	__ATTR_NULL,
};

static struct class srpt_class = {
	.name        = "infiniband_srpt",
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 26)
	.release = srpt_release_class_dev,
#else
	.dev_release = srpt_release_class_dev,
#endif
	.class_attrs = srpt_class_attrs,
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 26)
	.class_dev_attrs = srpt_dev_attrs,
#else
	.dev_attrs   = srpt_dev_attrs,
#endif
};

/**
 * srpt_add_one() - Callback function that is called once by the InfiniBand
 * core for each registered InfiniBand device.
 */
static void srpt_add_one(struct ib_device *device)
{
	struct srpt_device *sdev;
	struct srpt_port *sport;
	struct ib_srq_init_attr srq_attr;
	int i;

	TRACE_ENTRY();

	TRACE_DBG("device = %p, device->dma_ops = %p", device, device->dma_ops);

	sdev = kzalloc(sizeof *sdev, GFP_KERNEL);
	if (!sdev)
		return;

	sdev->scst_tgt = scst_register(&srpt_template, NULL);
	if (!sdev->scst_tgt) {
		PRINT_ERROR("SCST registration failed for %s.",
			    sdev->device->name);
		goto free_dev;
	}

	scst_tgt_set_tgt_priv(sdev->scst_tgt, sdev);

	sdev->device = device;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 26)
	sdev->class_dev.class = &srpt_class;
	sdev->class_dev.dev = device->dma_device;
	snprintf(sdev->class_dev.class_id, BUS_ID_SIZE,
		 "srpt-%s", device->name);
#else
	sdev->dev.class = &srpt_class;
	sdev->dev.parent = device->dma_device;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 30)
	snprintf(sdev->dev.bus_id, BUS_ID_SIZE, "srpt-%s", device->name);
#else
	dev_set_name(&sdev->dev, "srpt-%s", device->name);
#endif
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 26)
	if (class_device_register(&sdev->class_dev))
		goto unregister_tgt;
#else
	if (device_register(&sdev->dev))
		goto unregister_tgt;
#endif

	if (ib_query_device(device, &sdev->dev_attr))
		goto err_dev;

	sdev->pd = ib_alloc_pd(device);
	if (IS_ERR(sdev->pd))
		goto err_dev;

	sdev->mr = ib_get_dma_mr(sdev->pd, IB_ACCESS_LOCAL_WRITE);
	if (IS_ERR(sdev->mr))
		goto err_pd;

	srq_attr.event_handler = srpt_srq_event;
	srq_attr.srq_context = (void *)sdev;
	srq_attr.attr.max_wr = min(SRPT_SRQ_SIZE, sdev->dev_attr.max_srq_wr);
	srq_attr.attr.max_sge = 1;
	srq_attr.attr.srq_limit = 0;

	sdev->srq = ib_create_srq(sdev->pd, &srq_attr);
	if (IS_ERR(sdev->srq))
		goto err_mr;

	TRACE_DBG("%s: create SRQ #wr= %d max_allow=%d dev= %s",
	       __func__, srq_attr.attr.max_wr,
	      sdev->dev_attr.max_srq_wr, device->name);

	if (!srpt_service_guid)
		srpt_service_guid = be64_to_cpu(device->node_guid);

	sdev->cm_id = ib_create_cm_id(device, srpt_cm_handler, sdev);
	if (IS_ERR(sdev->cm_id))
		goto err_srq;

	/* print out target login information */
	TRACE_DBG("Target login info: id_ext=%016llx,"
		  "ioc_guid=%016llx,pkey=ffff,service_id=%016llx",
		  (unsigned long long) srpt_service_guid,
		  (unsigned long long) srpt_service_guid,
		  (unsigned long long) srpt_service_guid);

	/*
	 * We do not have a consistent service_id (ie. also id_ext of target_id)
	 * to identify this target. We currently use the guid of the first HCA
	 * in the system as service_id; therefore, the target_id will change
	 * if this HCA is gone bad and replaced by different HCA
	 */
	if (ib_cm_listen(sdev->cm_id, cpu_to_be64(srpt_service_guid), 0, NULL))
		goto err_cm;

	INIT_IB_EVENT_HANDLER(&sdev->event_handler, sdev->device,
			      srpt_event_handler);
	if (ib_register_event_handler(&sdev->event_handler))
		goto err_cm;

	if (srpt_alloc_ioctx_ring(sdev, sdev->ioctx_ring,
				  ARRAY_SIZE(sdev->ioctx_ring), 0))
		goto err_event;

	INIT_LIST_HEAD(&sdev->rch_list);
	spin_lock_init(&sdev->spinlock);

	for (i = 0; i < SRPT_SRQ_SIZE; ++i)
		srpt_post_recv(sdev, sdev->ioctx_ring[i]);

	ib_set_client_data(device, &srpt_client, sdev);

	WARN_ON(sdev->device->phys_port_cnt
		> sizeof(sdev->port)/sizeof(sdev->port[0]));

	for (i = 1; i <= sdev->device->phys_port_cnt; i++) {
		sport = &sdev->port[i - 1];
		sport->sdev = sdev;
		sport->port = i;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 20) && ! defined(BACKPORT_LINUX_WORKQUEUE_TO_2_6_19)
		/*
		 * A vanilla 2.6.19 or older kernel without backported OFED
		 * kernel headers.
		 */
		INIT_WORK(&sport->work, srpt_refresh_port_work, sport);
#else
		INIT_WORK(&sport->work, srpt_refresh_port_work);
#endif
		if (srpt_refresh_port(sport)) {
			PRINT_ERROR("MAD registration failed for %s-%d.",
				    sdev->device->name, i);
			goto err_ring;
		}
	}

	atomic_inc(&srpt_device_count);

	TRACE_EXIT();

	return;

err_ring:
	ib_set_client_data(device, &srpt_client, NULL);
	srpt_free_ioctx_ring(sdev, sdev->ioctx_ring,
			     ARRAY_SIZE(sdev->ioctx_ring));
err_event:
	ib_unregister_event_handler(&sdev->event_handler);
err_cm:
	ib_destroy_cm_id(sdev->cm_id);
err_srq:
	ib_destroy_srq(sdev->srq);
err_mr:
	ib_dereg_mr(sdev->mr);
err_pd:
	ib_dealloc_pd(sdev->pd);
err_dev:
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 26)
	class_device_unregister(&sdev->class_dev);
#else
	device_unregister(&sdev->dev);
#endif
unregister_tgt:
	scst_unregister(sdev->scst_tgt);
free_dev:
	kfree(sdev);

	TRACE_EXIT();
}

/*
 * Callback function called by the InfiniBand core when either an InfiniBand
 * device has been removed or during the ib_unregister_client() call for each
 * registered InfiniBand device.
 */
static void srpt_remove_one(struct ib_device *device)
{
	int i;
	struct srpt_device *sdev;

	TRACE_ENTRY();

	sdev = ib_get_client_data(device, &srpt_client);
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 18)
	WARN_ON(!sdev);
	if (!sdev)
		return;
#else
	if (WARN_ON(!sdev))
		return;
#endif

	srpt_unregister_mad_agent(sdev);

	/*
	 * Cancel the work if it is queued. Wait until srpt_refresh_port_work()
	 * finished if it is running.
	 */
	for (i = 0; i < sdev->device->phys_port_cnt; i++)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 22)
		cancel_work_sync(&sdev->port[i].work);
#else
		/*
		 * cancel_work_sync() was introduced in kernel 2.6.22. Older
		 * kernels do not have a facility to cancel scheduled work.
		 */
		PRINT_ERROR("%s",
		       "your kernel does not provide cancel_work_sync().");
#endif

	scst_unregister(sdev->scst_tgt);
	sdev->scst_tgt = NULL;

	ib_unregister_event_handler(&sdev->event_handler);
	ib_destroy_cm_id(sdev->cm_id);
	ib_destroy_srq(sdev->srq);
	ib_dereg_mr(sdev->mr);
	ib_dealloc_pd(sdev->pd);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 26)
	class_device_unregister(&sdev->class_dev);
#else
	device_unregister(&sdev->dev);
#endif

	srpt_free_ioctx_ring(sdev, sdev->ioctx_ring,
			     ARRAY_SIZE(sdev->ioctx_ring));
	kfree(sdev);

	TRACE_EXIT();
}

#ifdef CONFIG_SCST_PROC

/**
 * Create procfs entries for srpt. Currently the only procfs entry created
 * by this function is the "trace_level" entry.
 */
static int srpt_register_procfs_entry(struct scst_tgt_template *tgt)
{
	int res = 0;
#if defined(CONFIG_SCST_DEBUG) || defined(CONFIG_SCST_TRACING)
	struct proc_dir_entry *p, *root;

	root = scst_proc_get_tgt_root(tgt);
	WARN_ON(!root);
	if (root) {
		/*
		 * Fill in the scst_proc_data::data pointer, which is used in
		 * a printk(KERN_INFO ...) statement in
		 * scst_proc_log_entry_write() in scst_proc.c.
		 */
		srpt_log_proc_data.data = (char *)tgt->name;
		p = scst_create_proc_entry(root, SRPT_PROC_TRACE_LEVEL_NAME,
					   &srpt_log_proc_data);
		if (!p)
			res = -ENOMEM;
	} else
		res = -ENOMEM;

#endif
	return res;
}

static void srpt_unregister_procfs_entry(struct scst_tgt_template *tgt)
{
#if defined(CONFIG_SCST_DEBUG) || defined(CONFIG_SCST_TRACING)
	struct proc_dir_entry *root;

	root = scst_proc_get_tgt_root(tgt);
	WARN_ON(!root);
	if (root)
		remove_proc_entry(SRPT_PROC_TRACE_LEVEL_NAME, root);
#endif
}

#endif /*CONFIG_SCST_PROC*/

/*
 * Module initialization.
 *
 * Note: since ib_register_client() registers callback functions, and since at
 * least one of these callback functions (srpt_add_one()) calls SCST functions,
 * the SCST target template must be registered before ib_register_client() is
 * called.
 */
static int __init srpt_init_module(void)
{
	int ret;

	ret = -EINVAL;
	if (srp_max_message_size < MIN_MAX_MESSAGE_SIZE) {
		PRINT_ERROR("invalid value %d for kernel module parameter"
			    " srp_max_message_size -- must be at least %d.",
			    srp_max_message_size,
			    MIN_MAX_MESSAGE_SIZE);
		goto out;
	}

	ret = class_register(&srpt_class);
	if (ret) {
		PRINT_ERROR("%s", "couldn't register class ib_srpt");
		goto out;
	}

	srpt_template.xmit_response_atomic = !thread;
	srpt_template.rdy_to_xfer_atomic = !thread;
	ret = scst_register_target_template(&srpt_template);
	if (ret < 0) {
		PRINT_ERROR("%s", "couldn't register with scst");
		ret = -ENODEV;
		goto out_unregister_class;
	}

#ifdef CONFIG_SCST_PROC
	ret = srpt_register_procfs_entry(&srpt_template);
	if (ret) {
		PRINT_ERROR("%s", "couldn't register procfs entry");
		goto out_unregister_target;
	}
#endif /*CONFIG_SCST_PROC*/

	ret = ib_register_client(&srpt_client);
	if (ret) {
		PRINT_ERROR("%s", "couldn't register IB client");
		goto out_unregister_target;
	}

	return 0;

out_unregister_target:
#ifdef CONFIG_SCST_PROC
	/*
	 * Note: the procfs entry is unregistered in srpt_release(), which is
	 * called by scst_unregister_target_template().
	 */
#endif /*CONFIG_SCST_PROC*/
	scst_unregister_target_template(&srpt_template);
out_unregister_class:
	class_unregister(&srpt_class);
out:
	return ret;
}

static void __exit srpt_cleanup_module(void)
{
	TRACE_ENTRY();

	ib_unregister_client(&srpt_client);
	scst_unregister_target_template(&srpt_template);
	class_unregister(&srpt_class);

	TRACE_EXIT();
}

module_init(srpt_init_module);
module_exit(srpt_cleanup_module);
