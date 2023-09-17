/*-
 * SPDX-License-Identifier: GPL-2.0 or Linux-OpenIB
 *
 * Copyright (c) 2021 - 2023 Intel Corporation
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenFabrics.org BSD license below:
 *
 *   Redistribution and use in source and binary forms, with or
 *   without modification, are permitted provided that the following
 *   conditions are met:
 *
 *    - Redistributions of source code must retain the above
 *	copyright notice, this list of conditions and the following
 *	disclaimer.
 *
 *    - Redistributions in binary form must reproduce the above
 *	copyright notice, this list of conditions and the following
 *	disclaimer in the documentation and/or other materials
 *	provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/sysctl.h>
#include <machine/bus.h>
#include <linux/device.h>
#include <sys/rman.h>

#include "ice_rdma.h"
#include "irdma_main.h"
#include "icrdma_hw.h"

#include "irdma_if.h"
#include "irdma_di_if.h"

/**
 *  Driver version
 */
char irdma_driver_version[] = "1.2.17-k";

/**
 * irdma_init_tunable - prepare tunables
 * @rf: RDMA PCI function
 * @pf_id: id of the pf
 */
static void
irdma_init_tunable(struct irdma_pci_f *rf, uint8_t pf_id)
{
	struct sysctl_oid_list *irdma_oid_list;
	struct irdma_tunable_info *t_info = &rf->tun_info;
	char pf_name[16];

	snprintf(pf_name, 15, "irdma%d", pf_id);
	sysctl_ctx_init(&t_info->irdma_sysctl_ctx);

	t_info->irdma_sysctl_tree = SYSCTL_ADD_NODE(&t_info->irdma_sysctl_ctx,
						    SYSCTL_STATIC_CHILDREN(_dev),
						    OID_AUTO, pf_name,
						    CTLFLAG_RD, NULL, "");

	irdma_oid_list = SYSCTL_CHILDREN(t_info->irdma_sysctl_tree);

	t_info->sws_sysctl_tree = SYSCTL_ADD_NODE(&t_info->irdma_sysctl_ctx,
						  irdma_oid_list, OID_AUTO,
						  "sw_stats", CTLFLAG_RD,
						  NULL, "");
	/*
	 * debug mask setting
	 */
	SYSCTL_ADD_S32(&t_info->irdma_sysctl_ctx, irdma_oid_list,
		       OID_AUTO, "debug", CTLFLAG_RWTUN, &rf->sc_dev.debug_mask,
		       0, "irdma debug");

	/*
	 * RoCEv2/iWARP setting RoCEv2 the default mode
	 */
	t_info->roce_ena = 1;
	SYSCTL_ADD_U8(&t_info->irdma_sysctl_ctx, irdma_oid_list, OID_AUTO,
		      "roce_enable", CTLFLAG_RDTUN, &t_info->roce_ena, 0,
		      "RoCEv2 mode enable");

	rf->protocol_used = IRDMA_IWARP_PROTOCOL_ONLY;
	if (t_info->roce_ena == 1)
		rf->protocol_used = IRDMA_ROCE_PROTOCOL_ONLY;
	else if (t_info->roce_ena != 0)
		printf("%s:%d wrong roce_enable value (%d), using iWARP\n",
		       __func__, __LINE__, t_info->roce_ena);
	printf("%s:%d protocol: %s, roce_enable value: %d\n", __func__, __LINE__,
	       (rf->protocol_used == IRDMA_IWARP_PROTOCOL_ONLY) ? "iWARP" : "RoCEv2",
	       t_info->roce_ena);

	snprintf(t_info->drv_ver, IRDMA_VER_LEN, "%s", irdma_driver_version);
	SYSCTL_ADD_STRING(&t_info->irdma_sysctl_ctx, irdma_oid_list,
			  OID_AUTO, "drv_ver", CTLFLAG_RDTUN, t_info->drv_ver,
			  IRDMA_VER_LEN, "driver version");

	irdma_dcqcn_tunables_init(rf);
	irdma_sysctl_settings(rf);
}

/**
 * irdma_find_handler - obtain hdl object to identify pf
 * @p_dev: the peer interface structure
 */
static struct irdma_handler *
irdma_find_handler(struct ice_rdma_peer *p_dev)
{
	struct irdma_handler *hdl;
	unsigned long flags;

	spin_lock_irqsave(&irdma_handler_lock, flags);
	list_for_each_entry(hdl, &irdma_handlers, list) {
		if (!hdl->iwdev->rf->peer_info)
			continue;
		if (hdl->iwdev->rf->peer_info->dev == p_dev->dev) {
			spin_unlock_irqrestore(&irdma_handler_lock, flags);
			return hdl;
		}
	}
	spin_unlock_irqrestore(&irdma_handler_lock, flags);

	return NULL;
}

/**
 * peer_to_iwdev - return iwdev based on peer
 * @peer: the peer interface structure
 */
static struct irdma_device *
peer_to_iwdev(struct ice_rdma_peer *peer)
{
	struct irdma_handler *hdl;

	hdl = irdma_find_handler(peer);
	if (!hdl) {
		printf("%s:%d rdma handler not found\n", __func__, __LINE__);
		return NULL;
	}

	return hdl->iwdev;
}

/**
 * irdma_get_qos_info - save qos info from parameters to internal struct
 * @l2params: destination, qos, tc, mtu info structure
 * @qos_info: source, DCB settings structure
 */
static void
irdma_get_qos_info(struct irdma_pci_f *rf, struct irdma_l2params *l2params,
		   struct ice_qos_params *qos_info)
{
	int i;
	char txt[7][128] = {"", "", "", "", "", "", ""};
	u8 len;

	l2params->num_tc = qos_info->num_tc;
	l2params->num_apps = qos_info->num_apps;
	l2params->vsi_prio_type = qos_info->vsi_priority_type;
	l2params->vsi_rel_bw = qos_info->vsi_relative_bw;
	for (i = 0; i < l2params->num_tc; i++) {
		l2params->tc_info[i].egress_virt_up =
		    qos_info->tc_info[i].egress_virt_up;
		l2params->tc_info[i].ingress_virt_up =
		    qos_info->tc_info[i].ingress_virt_up;
		l2params->tc_info[i].prio_type = qos_info->tc_info[i].prio_type;
		l2params->tc_info[i].rel_bw = qos_info->tc_info[i].rel_bw;
		l2params->tc_info[i].tc_ctx = qos_info->tc_info[i].tc_ctx;
	}
	for (i = 0; i < IRDMA_MAX_USER_PRIORITY; i++)
		l2params->up2tc[i] = qos_info->up2tc[i];

	if (qos_info->pfc_mode == IRDMA_QOS_MODE_DSCP) {
		l2params->dscp_mode = true;
		memcpy(l2params->dscp_map, qos_info->dscp_map, sizeof(l2params->dscp_map));
	}
	if (!(rf->sc_dev.debug_mask & IRDMA_DEBUG_DCB))
		return;
	for (i = 0; i < l2params->num_tc; i++) {
		len = strlen(txt[0]);
		snprintf(txt[0] + len, sizeof(txt[0]) - 5, " %d",
			 l2params->tc_info[i].egress_virt_up);
		len = strlen(txt[1]);
		snprintf(txt[1] + len, sizeof(txt[1]) - 5, " %d",
			 l2params->tc_info[i].ingress_virt_up);
		len = strlen(txt[2]);
		snprintf(txt[2] + len, sizeof(txt[2]) - 5, " %d",
			 l2params->tc_info[i].prio_type);
		len = strlen(txt[3]);
		snprintf(txt[3] + len, sizeof(txt[3]) - 5, " %d",
			 l2params->tc_info[i].rel_bw);
		len = strlen(txt[4]);
		snprintf(txt[4] + len, sizeof(txt[4]) - 5, " %lu",
			 l2params->tc_info[i].tc_ctx);
	}
	len = strlen(txt[5]);
	for (i = 0; i < IRDMA_MAX_USER_PRIORITY; i++)
		len += snprintf(txt[5] + len, sizeof(txt[5]) - 5, " %d",
				l2params->up2tc[i]);
	len = strlen(txt[6]);
	for (i = 0; i < IRDMA_DSCP_NUM_VAL; i++)
		len += snprintf(txt[6] + len, sizeof(txt[6]) - 5, " %d",
				l2params->dscp_map[i]);
	irdma_debug(&rf->sc_dev, IRDMA_DEBUG_DCB, "num_tc:          %d\n", l2params->num_tc);
	irdma_debug(&rf->sc_dev, IRDMA_DEBUG_DCB, "num_apps:        %d\n", l2params->num_apps);
	irdma_debug(&rf->sc_dev, IRDMA_DEBUG_DCB, "vsi_prio_type:   %d\n", l2params->vsi_prio_type);
	irdma_debug(&rf->sc_dev, IRDMA_DEBUG_DCB, "vsi_rel_bw:      %d\n", l2params->vsi_rel_bw);
	irdma_debug(&rf->sc_dev, IRDMA_DEBUG_DCB, "egress_virt_up: %s\n", txt[0]);
	irdma_debug(&rf->sc_dev, IRDMA_DEBUG_DCB, "ingress_virt_up:%s\n", txt[1]);
	irdma_debug(&rf->sc_dev, IRDMA_DEBUG_DCB, "prio_type: %s\n", txt[2]);
	irdma_debug(&rf->sc_dev, IRDMA_DEBUG_DCB, "rel_bw:    %s\n", txt[3]);
	irdma_debug(&rf->sc_dev, IRDMA_DEBUG_DCB, "tc_ctx:    %s\n", txt[4]);
	irdma_debug(&rf->sc_dev, IRDMA_DEBUG_DCB, "up2tc:     %s\n", txt[5]);
	irdma_debug(&rf->sc_dev, IRDMA_DEBUG_DCB, "dscp_mode: %s\n", txt[6]);

	irdma_debug_buf(&rf->sc_dev, IRDMA_DEBUG_DCB, "l2params", l2params, sizeof(*l2params));
}

/**
 * irdma_log_invalid_mtu - check mtu setting validity
 * @mtu: mtu value
 * @dev: hardware control device structure
 */
static void
irdma_log_invalid_mtu(u16 mtu, struct irdma_sc_dev *dev)
{
	if (mtu < IRDMA_MIN_MTU_IPV4)
		irdma_dev_warn(to_ibdev(dev),
			       "MTU setting [%d] too low for RDMA traffic. Minimum MTU is 576 for IPv4\n",
			       mtu);
	else if (mtu < IRDMA_MIN_MTU_IPV6)
		irdma_dev_warn(to_ibdev(dev),
			       "MTU setting [%d] too low for RDMA traffic. Minimum MTU is 1280 for IPv6\\n",
			       mtu);
}

/**
 * irdma_get_event_name - convert type enum to string
 * @type: event type enum
 */
static const char *
irdma_get_event_name(enum ice_rdma_event_type type)
{
	switch (type) {
	case ICE_RDMA_EVENT_LINK_CHANGE:
		return "LINK CHANGE";
	case ICE_RDMA_EVENT_MTU_CHANGE:
		return "MTU CHANGE";
	case ICE_RDMA_EVENT_TC_CHANGE:
		return "TC CHANGE";
	case ICE_RDMA_EVENT_API_CHANGE:
		return "API CHANGE";
	case ICE_RDMA_EVENT_CRIT_ERR:
		return "CRITICAL ERROR";
	case ICE_RDMA_EVENT_RESET:
		return "RESET";
	case ICE_RDMA_EVENT_QSET_REGISTER:
		return "QSET REGISTER";
	case ICE_RDMA_EVENT_VSI_FILTER_UPDATE:
		return "VSI FILTER UPDATE";
	default:
		return "UNKNOWN";
	}
}

/**
 * irdma_event_handler - handling events from lan driver
 * @peer: the peer interface structure
 * @event: event info structure
 */
static void
irdma_event_handler(struct ice_rdma_peer *peer, struct ice_rdma_event *event)
{
	struct irdma_device *iwdev;
	struct irdma_l2params l2params = {};

	printf("%s:%d event_handler %s (%x) on pf %d (%d)\n", __func__, __LINE__,
	       irdma_get_event_name(event->type),
	       event->type, peer->pf_id, if_getdunit(peer->ifp));
	iwdev = peer_to_iwdev(peer);
	if (!iwdev) {
		printf("%s:%d rdma device not found\n", __func__, __LINE__);
		return;
	}

	switch (event->type) {
	case ICE_RDMA_EVENT_LINK_CHANGE:
		printf("%s:%d PF: %x (%x), state: %d, speed: %lu\n", __func__, __LINE__,
		       peer->pf_id, if_getdunit(peer->ifp), event->linkstate,
		       event->baudrate);
		break;
	case ICE_RDMA_EVENT_MTU_CHANGE:
		if (iwdev->vsi.mtu != event->mtu) {
			l2params.mtu = event->mtu;
			l2params.mtu_changed = true;
			irdma_log_invalid_mtu(l2params.mtu, &iwdev->rf->sc_dev);
			irdma_change_l2params(&iwdev->vsi, &l2params);
		}
		break;
	case ICE_RDMA_EVENT_TC_CHANGE:
		/*
		 * 1. check if it is pre or post 2. check if it is currently being done
		 */
		if (event->prep == iwdev->vsi.tc_change_pending) {
			printf("%s:%d can't process %s TC change if TC change is %spending\n",
			       __func__, __LINE__,
			       event->prep ? "pre" : "post",
			       event->prep ? " " : "not ");
			goto done;
		}
		if (!atomic_inc_not_zero(&iwdev->rf->dev_ctx.event_rfcnt)) {
			printf("%s:%d (%d) EVENT_TC_CHANGE received, but not processed %d\n",
			       __func__, __LINE__, if_getdunit(peer->ifp),
			       atomic_read(&iwdev->rf->dev_ctx.event_rfcnt));
			break;
		}
		if (event->prep) {
			iwdev->vsi.tc_change_pending = true;
			irdma_sc_suspend_resume_qps(&iwdev->vsi, IRDMA_OP_SUSPEND);
			wait_event_timeout(iwdev->suspend_wq,
					   !atomic_read(&iwdev->vsi.qp_suspend_reqs),
					   IRDMA_EVENT_TIMEOUT_MS * 10);
			irdma_ws_reset(&iwdev->vsi);
			printf("%s:%d TC change preparation done\n", __func__, __LINE__);
		} else {
			l2params.tc_changed = true;
			irdma_get_qos_info(iwdev->rf, &l2params, &event->port_qos);
			if (iwdev->rf->protocol_used != IRDMA_IWARP_PROTOCOL_ONLY)
				iwdev->dcb_vlan_mode = l2params.num_tc > 1 && !l2params.dscp_mode;

			irdma_check_fc_for_tc_update(&iwdev->vsi, &l2params);
			irdma_change_l2params(&iwdev->vsi, &l2params);
			printf("%s:%d TC change done\n", __func__, __LINE__);
		}
		atomic_dec(&iwdev->rf->dev_ctx.event_rfcnt);
		break;
	case ICE_RDMA_EVENT_CRIT_ERR:
		if (event->oicr_reg & IRDMAPFINT_OICR_PE_CRITERR_M) {
			u32 pe_criterr;

#define IRDMA_Q1_RESOURCE_ERR  0x0001024d
			pe_criterr = readl(iwdev->rf->sc_dev.hw_regs[IRDMA_GLPE_CRITERR]);
			if (pe_criterr != IRDMA_Q1_RESOURCE_ERR) {
				irdma_pr_err("critical PE Error, GLPE_CRITERR=0x%08x\n",
					     pe_criterr);
				iwdev->rf->reset = true;
			} else {
				irdma_dev_warn(to_ibdev(&iwdev->rf->sc_dev),
					       "Q1 Resource Check\n");
			}
		}
		if (event->oicr_reg & IRDMAPFINT_OICR_HMC_ERR_M) {
			irdma_pr_err("HMC Error\n");
			iwdev->rf->reset = true;
		}
		if (iwdev->rf->reset)
			iwdev->rf->gen_ops.request_reset(iwdev->rf);
		break;
	case ICE_RDMA_EVENT_RESET:
		iwdev->rf->reset = true;
		break;
	default:
		printf("%s:%d event type unsupported: %d\n", __func__, __LINE__, event->type);
	}
done:
	return;
}

/**
 * irdma_link_change - Callback for link state change
 * @peer: the peer interface structure
 * @linkstate: state of the link
 * @baudrate: speed of the link
 */
static void
irdma_link_change(struct ice_rdma_peer *peer, int linkstate, uint64_t baudrate)
{
	printf("%s:%d PF: %x (%x), state: %d, speed: %lu\n", __func__, __LINE__,
	       peer->pf_id, if_getdunit(peer->ifp), linkstate, baudrate);
}

/**
 * irdma_finalize_task - Finish open or close phase in a separate thread
 * @context: instance holding peer and iwdev information
 *
 * Triggered from irdma_open or irdma_close to perform rt_init_hw or
 * rt_deinit_hw respectively. Does registration and unregistration of
 * the device.
 */
static void
irdma_finalize_task(void *context, int pending)
{
	struct irdma_task_arg *task_arg = (struct irdma_task_arg *)context;
	struct irdma_device *iwdev = task_arg->iwdev;
	struct irdma_pci_f *rf = iwdev->rf;
	struct ice_rdma_peer *peer = task_arg->peer;
	struct irdma_l2params l2params = {{{0}}};
	struct ice_rdma_request req = {0};
	int status = 0;

	if (iwdev->iw_status) {
		irdma_debug(&rf->sc_dev, IRDMA_DEBUG_INIT,
			    "Starting deferred closing %d (%d)\n",
			    rf->peer_info->pf_id, if_getdunit(peer->ifp));
		atomic_dec(&rf->dev_ctx.event_rfcnt);
		wait_event_timeout(iwdev->suspend_wq,
				   !atomic_read(&rf->dev_ctx.event_rfcnt),
				   IRDMA_MAX_TIMEOUT);
		if (atomic_read(&rf->dev_ctx.event_rfcnt) != 0) {
			printf("%s:%d (%d) waiting for event_rfcnt (%d) timeout, proceed with unload\n",
			       __func__, __LINE__, if_getdunit(peer->ifp),
			       atomic_read(&rf->dev_ctx.event_rfcnt));
		}
		irdma_dereg_ipaddr_event_cb(rf);
		irdma_ib_unregister_device(iwdev);
		req.type = ICE_RDMA_EVENT_VSI_FILTER_UPDATE;
		req.enable_filter = false;
		IRDMA_DI_REQ_HANDLER(peer, &req);
		irdma_cleanup_dead_qps(&iwdev->vsi);
		irdma_rt_deinit_hw(iwdev);
	} else {
		irdma_debug(&rf->sc_dev, IRDMA_DEBUG_INIT,
			    "Starting deferred opening %d (%d)\n",
			    rf->peer_info->pf_id, if_getdunit(peer->ifp));
		irdma_get_qos_info(iwdev->rf, &l2params, &peer->initial_qos_info);
		if (iwdev->rf->protocol_used != IRDMA_IWARP_PROTOCOL_ONLY)
			iwdev->dcb_vlan_mode = l2params.num_tc > 1 && !l2params.dscp_mode;

		l2params.mtu = peer->mtu;
		status = irdma_rt_init_hw(iwdev, &l2params);
		if (status) {
			irdma_pr_err("RT init failed %d\n", status);
			ib_dealloc_device(&iwdev->ibdev);
			return;
		}
		status = irdma_ib_register_device(iwdev);
		if (status) {
			irdma_pr_err("Registration failed %d\n", status);
			irdma_rt_deinit_hw(iwdev);
			ib_dealloc_device(&iwdev->ibdev);
		}
		irdma_sw_stats_tunables_init(rf);
		req.type = ICE_RDMA_EVENT_VSI_FILTER_UPDATE;
		req.enable_filter = true;
		IRDMA_DI_REQ_HANDLER(peer, &req);
		irdma_reg_ipaddr_event_cb(rf);
		atomic_inc(&rf->dev_ctx.event_rfcnt);
		irdma_debug(&rf->sc_dev, IRDMA_DEBUG_INIT,
			    "Deferred opening finished %d (%d)\n",
			    rf->peer_info->pf_id, if_getdunit(peer->ifp));
	}
}

/**
 * irdma_alloc_pcidev - allocate memory for pcidev and populate data
 * @peer: the new peer interface structure
 * @rf: RDMA PCI function
 */
static int
irdma_alloc_pcidev(struct ice_rdma_peer *peer, struct irdma_pci_f *rf)
{
	rf->pcidev = kzalloc(sizeof(struct pci_dev), GFP_KERNEL);
	if (!rf->pcidev) {
		return -ENOMEM;
	}
	if (linux_pci_attach_device(rf->dev_ctx.dev, NULL, NULL, rf->pcidev))
		return -ENOMEM;

	return 0;
}

/**
 * irdma_dealloc_pcidev - deallocate memory for pcidev
 * @rf: RDMA PCI function
 */
static void
irdma_dealloc_pcidev(struct irdma_pci_f *rf)
{
	linux_pci_detach_device(rf->pcidev);
	kfree(rf->pcidev);
}

/**
 * irdma_fill_device_info - assign initial values to rf variables
 * @iwdev: irdma device
 * @peer: the peer interface structure
 */
static void
irdma_fill_device_info(struct irdma_device *iwdev,
		       struct ice_rdma_peer *peer)
{
	struct irdma_pci_f *rf = iwdev->rf;

	rf->peer_info = peer;
	rf->gen_ops.register_qset = irdma_register_qset;
	rf->gen_ops.unregister_qset = irdma_unregister_qset;

	rf->rdma_ver = IRDMA_GEN_2;
	rf->sc_dev.hw_attrs.uk_attrs.hw_rev = IRDMA_GEN_2;
	rf->rsrc_profile = IRDMA_HMC_PROFILE_DEFAULT;
	rf->rst_to = IRDMA_RST_TIMEOUT_HZ;
	rf->check_fc = irdma_check_fc_for_qp;
	rf->gen_ops.request_reset = irdma_request_reset;
	irdma_set_rf_user_cfg_params(rf);

	rf->default_vsi.vsi_idx = peer->pf_vsi_num;
	rf->dev_ctx.dev = peer->dev;
	rf->dev_ctx.mem_bus_space_tag = rman_get_bustag(peer->pci_mem);
	rf->dev_ctx.mem_bus_space_handle = rman_get_bushandle(peer->pci_mem);
	rf->dev_ctx.mem_bus_space_size = rman_get_size(peer->pci_mem);

	rf->hw.dev_context = &rf->dev_ctx;
	rf->hw.hw_addr = (u8 *)rman_get_virtual(peer->pci_mem);
	rf->msix_count = peer->msix.count;
	rf->msix_info.entry = peer->msix.base;
	rf->msix_info.vector = peer->msix.count;
	printf("%s:%d msix_info: %d %d %d\n", __func__, __LINE__,
	       rf->msix_count, rf->msix_info.entry, rf->msix_info.vector);

	rf->iwdev = iwdev;
	iwdev->netdev = peer->ifp;
	iwdev->init_state = INITIAL_STATE;
	iwdev->vsi_num = peer->pf_vsi_num;
	iwdev->rcv_wnd = IRDMA_CM_DEFAULT_RCV_WND_SCALED;
	iwdev->rcv_wscale = IRDMA_CM_DEFAULT_RCV_WND_SCALE;
	iwdev->roce_cwnd = IRDMA_ROCE_CWND_DEFAULT;
	iwdev->roce_ackcreds = IRDMA_ROCE_ACKCREDS_DEFAULT;
	iwdev->roce_rtomin = 5;

	if (rf->protocol_used == IRDMA_ROCE_PROTOCOL_ONLY) {
		iwdev->roce_mode = true;
	}
}

/**
 * irdma_probe - Callback to probe a new RDMA peer device
 * @peer: the new peer interface structure
 *
 * Callback implementing the RDMA_PROBE function. Called by the ice driver to
 * notify the RDMA client driver that a new device has been created
 */
static int
irdma_probe(struct ice_rdma_peer *peer)
{
	struct irdma_device *iwdev;
	struct irdma_pci_f *rf;
	struct irdma_handler *hdl;
	int err = 0;

	irdma_pr_info("probe: irdma-%s peer=%p, peer->pf_id=%d, peer->ifp=%p, peer->ifp->if_dunit=%d, peer->pci_mem->r_bustag=%p\n",
		      irdma_driver_version, peer, peer->pf_id, peer->ifp,
		      if_getdunit(peer->ifp), (void *)(uintptr_t)peer->pci_mem->r_bustag);

	hdl = irdma_find_handler(peer);
	if (hdl)
		return -EBUSY;

	hdl = kzalloc(sizeof(*hdl), GFP_KERNEL);
	if (!hdl)
		return -ENOMEM;

	iwdev = (struct irdma_device *)ib_alloc_device(sizeof(*iwdev));
	if (!iwdev) {
		kfree(hdl);
		return -ENOMEM;
	}

	iwdev->rf = kzalloc(sizeof(*rf), GFP_KERNEL);
	if (!iwdev->rf) {
		ib_dealloc_device(&iwdev->ibdev);
		kfree(hdl);
		return -ENOMEM;
	}
	hdl->iwdev = iwdev;
	iwdev->hdl = hdl;

	irdma_init_tunable(iwdev->rf, if_getdunit(peer->ifp));
	irdma_fill_device_info(iwdev, peer);
	rf = iwdev->rf;

	if (irdma_alloc_pcidev(peer, rf))
		goto err_pcidev;

	irdma_add_handler(hdl);

	if (irdma_ctrl_init_hw(rf)) {
		err = -EIO;
		goto err_ctrl_init;
	}

	rf->dev_ctx.task_arg.peer = peer;
	rf->dev_ctx.task_arg.iwdev = iwdev;
	rf->dev_ctx.task_arg.peer = peer;

	TASK_INIT(&hdl->deferred_task, 0, irdma_finalize_task, &rf->dev_ctx.task_arg);
	hdl->deferred_tq = taskqueue_create_fast("irdma_defer",
						 M_NOWAIT, taskqueue_thread_enqueue,
						 &hdl->deferred_tq);
	taskqueue_start_threads(&hdl->deferred_tq, 1, PI_NET, "irdma_defer_t");

	taskqueue_enqueue(hdl->deferred_tq, &hdl->deferred_task);

	return 0;

err_ctrl_init:
	irdma_del_handler(hdl);
	irdma_dealloc_pcidev(rf);
err_pcidev:
	kfree(iwdev->rf);
	ib_dealloc_device(&iwdev->ibdev);
	kfree(hdl);

	return err;
}

/**
 * irdma_remove - Callback to remove an RDMA peer device
 * @peer: the new peer interface structure
 *
 * Callback implementing the RDMA_REMOVE function. Called by the ice driver to
 * notify the RDMA client driver that the device wille be delated
 */
static int
irdma_remove(struct ice_rdma_peer *peer)
{
	struct irdma_handler *hdl;
	struct irdma_device *iwdev;

	irdma_debug((struct irdma_sc_dev *)NULL, IRDMA_DEBUG_INIT,
		    "removing %s irdma%d\n", __func__, if_getdunit(peer->ifp));

	hdl = irdma_find_handler(peer);
	if (!hdl)
		return 0;

	iwdev = hdl->iwdev;

	if (iwdev->vsi.tc_change_pending) {
		iwdev->vsi.tc_change_pending = false;
		irdma_sc_suspend_resume_qps(&iwdev->vsi, IRDMA_OP_RESUME);
	}

	taskqueue_enqueue(hdl->deferred_tq, &hdl->deferred_task);

	taskqueue_drain(hdl->deferred_tq, &hdl->deferred_task);
	taskqueue_free(hdl->deferred_tq);
	hdl->iwdev->rf->dev_ctx.task_arg.iwdev = NULL;
	hdl->iwdev->rf->dev_ctx.task_arg.peer = NULL;

	sysctl_ctx_free(&iwdev->rf->tun_info.irdma_sysctl_ctx);
	hdl->iwdev->rf->tun_info.irdma_sysctl_tree = NULL;
	hdl->iwdev->rf->tun_info.sws_sysctl_tree = NULL;

	irdma_ctrl_deinit_hw(iwdev->rf);

	irdma_dealloc_pcidev(iwdev->rf);

	irdma_del_handler(iwdev->hdl);
	kfree(iwdev->hdl);
	kfree(iwdev->rf);
	ib_dealloc_device(&iwdev->ibdev);
	irdma_pr_info("IRDMA hardware deinitialization complete irdma%d\n",
		      if_getdunit(peer->ifp));

	return 0;
}

/**
 * irdma_open - Callback for operation open for RDMA device
 * @peer: the new peer interface structure
 *
 * Callback implementing the RDMA_OPEN function. Called by the ice driver to
 * notify the RDMA client driver that a new device has been initialized.
 */
static int
irdma_open(struct ice_rdma_peer *peer)
{
	struct irdma_device *iwdev;
	struct ice_rdma_event event = {0};

	iwdev = peer_to_iwdev(peer);
	if (iwdev) {
		event.type = ICE_RDMA_EVENT_MTU_CHANGE;
		event.mtu = peer->mtu;

		irdma_event_handler(peer, &event);
	} else {
		irdma_probe(peer);
	}

	return 0;
}

/**
 * irdma_close - Callback to notify that a peer device is down
 * @peer: the RDMA peer device being stopped
 *
 * Callback implementing the RDMA_CLOSE function. Called by the ice driver to
 * notify the RDMA client driver that a peer device is being stopped.
 */
static int
irdma_close(struct ice_rdma_peer *peer)
{
	/*
	 * This is called when ifconfig down or pf-reset is about to happen.
	 */
	struct irdma_device *iwdev;

	iwdev = peer_to_iwdev(peer);
	if (iwdev && iwdev->rf->reset)
		irdma_remove(peer);

	return 0;
}

/**
 * irdma_prep_for_unregister - ensure the driver is ready to unregister
 */
static void
irdma_prep_for_unregister(void)
{
	struct irdma_handler *hdl;
	unsigned long flags;
	bool hdl_valid;

	do {
		hdl_valid = false;
		spin_lock_irqsave(&irdma_handler_lock, flags);
		list_for_each_entry(hdl, &irdma_handlers, list) {
			if (!hdl->iwdev->rf->peer_info)
				continue;
			hdl_valid = true;
			break;
		}
		spin_unlock_irqrestore(&irdma_handler_lock, flags);
		if (!hdl || !hdl_valid)
			break;
		IRDMA_CLOSE(hdl->iwdev->rf->peer_info);
		IRDMA_REMOVE(hdl->iwdev->rf->peer_info);
	} while (1);
}

static kobj_method_t irdma_methods[] = {
	KOBJMETHOD(irdma_probe, irdma_probe),
	    KOBJMETHOD(irdma_open, irdma_open),
	    KOBJMETHOD(irdma_close, irdma_close),
	    KOBJMETHOD(irdma_remove, irdma_remove),
	    KOBJMETHOD(irdma_link_change, irdma_link_change),
	    KOBJMETHOD(irdma_event_handler, irdma_event_handler),
	    KOBJMETHOD_END
};

/* declare irdma_class which extends the ice_rdma_di class */
DEFINE_CLASS_1(irdma, irdma_class, irdma_methods, sizeof(struct ice_rdma_peer), ice_rdma_di_class);

static struct ice_rdma_info irdma_info = {
	.major_version = ICE_RDMA_MAJOR_VERSION,
	.minor_version = ICE_RDMA_MINOR_VERSION,
	.patch_version = ICE_RDMA_PATCH_VERSION,
	.rdma_class = &irdma_class,
};

/**
 * irdma_module_event_handler - Module event handler callback
 * @mod: unused mod argument
 * @what: the module event to handle
 * @arg: unused module event argument
 *
 * Callback used by the FreeBSD module stack to notify the driver of module
 * events. Used to implement custom handling for certain module events such as
 * load and unload.
 */
static int
irdma_module_event_handler(module_t __unused mod, int what, void __unused * arg)
{
	switch (what) {
	case MOD_LOAD:
		printf("Loading irdma module\n");
		return ice_rdma_register(&irdma_info);
	case MOD_UNLOAD:
		printf("Unloading irdma module\n");
		irdma_prep_for_unregister();
		ice_rdma_unregister();
		return (0);
	default:
		return (EOPNOTSUPP);
	}

	return (0);
}

static moduledata_t irdma_moduledata = {
	"irdma",
	    irdma_module_event_handler,
	    NULL
};

DECLARE_MODULE(irdma, irdma_moduledata, SI_SUB_LAST, SI_ORDER_ANY);
MODULE_VERSION(irdma, 1);
MODULE_DEPEND(irdma, ice, 1, 1, 1);
MODULE_DEPEND(irdma, ibcore, 1, 1, 1);
