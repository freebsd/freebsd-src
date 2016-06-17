/* 
   BlueZ - Bluetooth protocol stack for Linux
   Copyright (C) 2000-2001 Qualcomm Incorporated

   Written 2000,2001 by Maxim Krasnyansky <maxk@qualcomm.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2 as
   published by the Free Software Foundation;

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD PARTY RIGHTS.
   IN NO EVENT SHALL THE COPYRIGHT HOLDER(S) AND AUTHOR(S) BE LIABLE FOR ANY
   CLAIM, OR ANY SPECIAL INDIRECT OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES 
   WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN 
   ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF 
   OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

   ALL LIABILITY, INCLUDING LIABILITY FOR INFRINGEMENT OF ANY PATENTS, 
   COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS, RELATING TO USE OF THIS 
   SOFTWARE IS DISCLAIMED.
*/

/*
 * HCI Events.
 *
 * $Id: hci_event.c,v 1.4 2002/07/27 18:14:38 maxk Exp $
 */

#include <linux/config.h>
#include <linux/module.h>

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/fcntl.h>
#include <linux/init.h>
#include <linux/skbuff.h>
#include <linux/interrupt.h>
#include <linux/notifier.h>
#include <net/sock.h>

#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/unaligned.h>

#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h>

#ifndef HCI_CORE_DEBUG
#undef  BT_DBG
#define BT_DBG( A... )
#endif

/* Handle HCI Event packets */

/* Command Complete OGF LINK_CTL  */
static void hci_cc_link_ctl(struct hci_dev *hdev, __u16 ocf, struct sk_buff *skb)
{
	__u8 status;

	BT_DBG("%s ocf 0x%x", hdev->name, ocf);

	switch (ocf) {
	case OCF_INQUIRY_CANCEL:
		status = *((__u8 *) skb->data);

		if (status) {
			BT_DBG("%s Inquiry cancel error: status 0x%x", hdev->name, status);
		} else {
			clear_bit(HCI_INQUIRY, &hdev->flags);
			hci_req_complete(hdev, status);
		}
		break;

	default:
		BT_DBG("%s Command complete: ogf LINK_CTL ocf %x", hdev->name, ocf);
		break;
	};
}

/* Command Complete OGF LINK_POLICY  */
static void hci_cc_link_policy(struct hci_dev *hdev, __u16 ocf, struct sk_buff *skb)
{
	struct hci_conn *conn;
	role_discovery_rp *rd;

	BT_DBG("%s ocf 0x%x", hdev->name, ocf);

	switch (ocf) {
	case OCF_ROLE_DISCOVERY: 
		rd = (void *) skb->data;

		if (rd->status)
			break;
		
		hci_dev_lock(hdev);
	
		conn = conn_hash_lookup_handle(hdev, __le16_to_cpu(rd->handle));
		if (conn) {
			if (rd->role)
				conn->link_mode &= ~HCI_LM_MASTER;
			else
				conn->link_mode |= HCI_LM_MASTER;
		}
			
		hci_dev_unlock(hdev);
		break;

	default:
		BT_DBG("%s: Command complete: ogf LINK_POLICY ocf %x", 
				hdev->name, ocf);
		break;
	};
}

/* Command Complete OGF HOST_CTL  */
static void hci_cc_host_ctl(struct hci_dev *hdev, __u16 ocf, struct sk_buff *skb)
{
	__u8 status, param;
	void *sent;

	BT_DBG("%s ocf 0x%x", hdev->name, ocf);

	switch (ocf) {
	case OCF_RESET:
		status = *((__u8 *) skb->data);
		hci_req_complete(hdev, status);
		break;

	case OCF_SET_EVENT_FLT:
		status = *((__u8 *) skb->data);
		if (status) {
			BT_DBG("%s SET_EVENT_FLT failed %d", hdev->name, status);
		} else {
			BT_DBG("%s SET_EVENT_FLT succeseful", hdev->name);
		}
		break;

	case OCF_WRITE_AUTH_ENABLE:
		sent = hci_sent_cmd_data(hdev, OGF_HOST_CTL, OCF_WRITE_AUTH_ENABLE);
		if (!sent)
			break;

		status = *((__u8 *) skb->data);
		param  = *((__u8 *) sent);

		if (!status) {
			if (param == AUTH_ENABLED)
				set_bit(HCI_AUTH, &hdev->flags);
			else
				clear_bit(HCI_AUTH, &hdev->flags);
		}
		hci_req_complete(hdev, status);
		break;

	case OCF_WRITE_ENCRYPT_MODE:
		sent = hci_sent_cmd_data(hdev, OGF_HOST_CTL, OCF_WRITE_ENCRYPT_MODE);
		if (!sent)
			break;

		status = *((__u8 *) skb->data);
		param  = *((__u8 *) sent);

		if (!status) {
			if (param)
				set_bit(HCI_ENCRYPT, &hdev->flags);
			else
				clear_bit(HCI_ENCRYPT, &hdev->flags);
		}
		hci_req_complete(hdev, status);
		break;

	case OCF_WRITE_CA_TIMEOUT:
		status = *((__u8 *) skb->data);
		if (status) {
			BT_DBG("%s OCF_WRITE_CA_TIMEOUT failed %d", hdev->name, status);
		} else {
			BT_DBG("%s OCF_WRITE_CA_TIMEOUT succeseful", hdev->name);
		}
		break;

	case OCF_WRITE_PG_TIMEOUT:
		status = *((__u8 *) skb->data);
		if (status) {
			BT_DBG("%s OCF_WRITE_PG_TIMEOUT failed %d", hdev->name, status);
		} else {
			BT_DBG("%s: OCF_WRITE_PG_TIMEOUT succeseful", hdev->name);
		}
		break;

	case OCF_WRITE_SCAN_ENABLE:
		sent = hci_sent_cmd_data(hdev, OGF_HOST_CTL, OCF_WRITE_SCAN_ENABLE);
		if (!sent)
			break;
		status = *((__u8 *) skb->data);
		param  = *((__u8 *) sent);

		BT_DBG("param 0x%x", param);

		if (!status) {
			clear_bit(HCI_PSCAN, &hdev->flags);
			clear_bit(HCI_ISCAN, &hdev->flags);
			if (param & SCAN_INQUIRY) 
				set_bit(HCI_ISCAN, &hdev->flags);

			if (param & SCAN_PAGE) 
				set_bit(HCI_PSCAN, &hdev->flags);
		}
		hci_req_complete(hdev, status);
		break;

	case OCF_HOST_BUFFER_SIZE:
		status = *((__u8 *) skb->data);
		if (status) {
			BT_DBG("%s OCF_BUFFER_SIZE failed %d", hdev->name, status);
			hci_req_complete(hdev, status);
		}
		break;

	default:
		BT_DBG("%s Command complete: ogf HOST_CTL ocf %x", hdev->name, ocf);
		break;
	};
}

/* Command Complete OGF INFO_PARAM  */
static void hci_cc_info_param(struct hci_dev *hdev, __u16 ocf, struct sk_buff *skb)
{
	read_local_features_rp *lf;
	read_buffer_size_rp *bs;
	read_bd_addr_rp *ba;

	BT_DBG("%s ocf 0x%x", hdev->name, ocf);

	switch (ocf) {
	case OCF_READ_LOCAL_FEATURES:
		lf = (read_local_features_rp *) skb->data;

		if (lf->status) {
			BT_DBG("%s READ_LOCAL_FEATURES failed %d", hdev->name, lf->status);
			break;
		}

		memcpy(hdev->features, lf->features, sizeof(hdev->features));

		/* Adjust default settings according to features 
		 * supported by device. */
		if (hdev->features[0] & LMP_3SLOT)
			hdev->pkt_type |= (HCI_DM3 | HCI_DH3);

		if (hdev->features[0] & LMP_5SLOT)
			hdev->pkt_type |= (HCI_DM5 | HCI_DH5);

		if (hdev->features[1] & LMP_HV2)
			hdev->pkt_type |= (HCI_HV2);

		if (hdev->features[1] & LMP_HV3)
			hdev->pkt_type |= (HCI_HV3);

		BT_DBG("%s: features 0x%x 0x%x 0x%x", hdev->name, lf->features[0], lf->features[1], lf->features[2]);

		break;

	case OCF_READ_BUFFER_SIZE:
		bs = (read_buffer_size_rp *) skb->data;

		if (bs->status) {
			BT_DBG("%s READ_BUFFER_SIZE failed %d", hdev->name, bs->status);
			hci_req_complete(hdev, bs->status);
			break;
		}

		hdev->acl_mtu  = __le16_to_cpu(bs->acl_mtu);
		hdev->sco_mtu  = bs->sco_mtu ? bs->sco_mtu : 64;
		hdev->acl_pkts = hdev->acl_cnt = __le16_to_cpu(bs->acl_max_pkt);
		hdev->sco_pkts = hdev->sco_cnt = __le16_to_cpu(bs->sco_max_pkt);

		BT_DBG("%s mtu: acl %d, sco %d max_pkt: acl %d, sco %d", hdev->name,
		    hdev->acl_mtu, hdev->sco_mtu, hdev->acl_pkts, hdev->sco_pkts);
		break;

	case OCF_READ_BD_ADDR:
		ba = (read_bd_addr_rp *) skb->data;

		if (!ba->status) {
			bacpy(&hdev->bdaddr, &ba->bdaddr);
		} else {
			BT_DBG("%s: READ_BD_ADDR failed %d", hdev->name, ba->status);
		}

		hci_req_complete(hdev, ba->status);
		break;

	default:
		BT_DBG("%s Command complete: ogf INFO_PARAM ocf %x", hdev->name, ocf);
		break;
	};
}

/* Command Status OGF LINK_CTL  */
static inline void hci_cs_create_conn(struct hci_dev *hdev, __u8 status)
{
	struct hci_conn *conn;
	create_conn_cp *cc = hci_sent_cmd_data(hdev, OGF_LINK_CTL, OCF_CREATE_CONN);

	if (!cc)
		return;

	hci_dev_lock(hdev);
	
	conn = conn_hash_lookup_ba(hdev, ACL_LINK, &cc->bdaddr);

	BT_DBG("%s status 0x%x bdaddr %s conn %p", hdev->name, 
			status, batostr(&cc->bdaddr), conn);

	if (status) {
		if (conn) {
			conn->state = BT_CLOSED;
			hci_proto_connect_cfm(conn, status);
			hci_conn_del(conn);
		}
	} else {
		if (!conn) {
			conn = hci_conn_add(hdev, ACL_LINK, &cc->bdaddr);
			if (conn) {
				conn->out = 1;
				conn->link_mode |= HCI_LM_MASTER;
			} else
				BT_ERR("No memmory for new connection");
		}
	}

	hci_dev_unlock(hdev);
}

static void hci_cs_link_ctl(struct hci_dev *hdev, __u16 ocf, __u8 status)
{
	BT_DBG("%s ocf 0x%x", hdev->name, ocf);

	switch (ocf) {
	case OCF_CREATE_CONN:
		hci_cs_create_conn(hdev, status);
		break;

	case OCF_ADD_SCO:
		if (status) {
			struct hci_conn *acl, *sco;
			add_sco_cp *cp = hci_sent_cmd_data(hdev, 
						OGF_LINK_CTL, OCF_ADD_SCO);
			__u16 handle;

			if (!cp)
				break;

			handle = __le16_to_cpu(cp->handle);

			BT_DBG("%s Add SCO error: handle %d status 0x%x", hdev->name, handle, status);

			hci_dev_lock(hdev);
	
			acl = conn_hash_lookup_handle(hdev, handle);
			if (acl && (sco = acl->link)) {
				sco->state = BT_CLOSED;
				hci_proto_connect_cfm(sco, status);
				hci_conn_del(sco);
			}

			hci_dev_unlock(hdev);
		}
		break;

	case OCF_INQUIRY:
		if (status) {
			BT_DBG("%s Inquiry error: status 0x%x", hdev->name, status);
			hci_req_complete(hdev, status);
		} else {
			set_bit(HCI_INQUIRY, &hdev->flags);
		}
		break;

	default:
		BT_DBG("%s Command status: ogf LINK_CTL ocf %x status %d", 
			hdev->name, ocf, status);
		break;
	};
}

/* Command Status OGF LINK_POLICY */
static void hci_cs_link_policy(struct hci_dev *hdev, __u16 ocf, __u8 status)
{
	BT_DBG("%s ocf 0x%x", hdev->name, ocf);

	switch (ocf) {
	default:
		BT_DBG("%s Command status: ogf HOST_POLICY ocf %x", hdev->name, ocf);
		break;
	};
}

/* Command Status OGF HOST_CTL */
static void hci_cs_host_ctl(struct hci_dev *hdev, __u16 ocf, __u8 status)
{
	BT_DBG("%s ocf 0x%x", hdev->name, ocf);

	switch (ocf) {
	default:
		BT_DBG("%s Command status: ogf HOST_CTL ocf %x", hdev->name, ocf);
		break;
	};
}

/* Command Status OGF INFO_PARAM  */
static void hci_cs_info_param(struct hci_dev *hdev, __u16 ocf, __u8 status)
{
	BT_DBG("%s: hci_cs_info_param: ocf 0x%x", hdev->name, ocf);

	switch (ocf) {
	default:
		BT_DBG("%s Command status: ogf INFO_PARAM ocf %x", hdev->name, ocf);
		break;
	};
}

/* Inquiry Complete */
static inline void hci_inquiry_complete_evt(struct hci_dev *hdev, struct sk_buff *skb)
{
	__u8 status = *((__u8 *) skb->data);

	BT_DBG("%s status %d", hdev->name, status);

	clear_bit(HCI_INQUIRY, &hdev->flags);
	hci_req_complete(hdev, status);
}

/* Inquiry Result */
static inline void hci_inquiry_result_evt(struct hci_dev *hdev, struct sk_buff *skb)
{
	inquiry_info *info = (inquiry_info *) (skb->data + 1);
	int num_rsp = *((__u8 *) skb->data);

	BT_DBG("%s num_rsp %d", hdev->name, num_rsp);

	hci_dev_lock(hdev);
	for (; num_rsp; num_rsp--)
		inquiry_cache_update(hdev, info++);
	hci_dev_unlock(hdev);
}

/* Inquiry Result With RSSI */
static inline void hci_inquiry_result_with_rssi_evt(struct hci_dev *hdev, struct sk_buff *skb)
{
	inquiry_info_with_rssi *info = (inquiry_info_with_rssi *) (skb->data + 1);
	int num_rsp = *((__u8 *) skb->data);

	BT_DBG("%s num_rsp %d", hdev->name, num_rsp);

	hci_dev_lock(hdev);
	for (; num_rsp; num_rsp--) {
		inquiry_info tmp;
		bacpy(&tmp.bdaddr, &info->bdaddr);
		tmp.pscan_rep_mode    = info->pscan_rep_mode;
		tmp.pscan_period_mode = info->pscan_period_mode;
		tmp.pscan_mode        = 0x00;
		memcpy(tmp.dev_class, &info->dev_class, 3);
		tmp.clock_offset      = info->clock_offset;
		info++;
		inquiry_cache_update(hdev, &tmp);
	}
	hci_dev_unlock(hdev);
}

/* Connect Request */
static inline void hci_conn_request_evt(struct hci_dev *hdev, struct sk_buff *skb)
{
	evt_conn_request *cr = (evt_conn_request *) skb->data;
	int mask = hdev->link_mode;

	BT_DBG("%s Connection request: %s type 0x%x", hdev->name,
			batostr(&cr->bdaddr), cr->link_type);

	mask |= hci_proto_connect_ind(hdev, &cr->bdaddr, cr->link_type);

	if (mask & HCI_LM_ACCEPT) {
		/* Connection accepted */
		struct hci_conn *conn;
		accept_conn_req_cp ac;

		hci_dev_lock(hdev);
		conn = conn_hash_lookup_ba(hdev, cr->link_type, &cr->bdaddr);
		if (!conn) {
			if (!(conn = hci_conn_add(hdev, cr->link_type, &cr->bdaddr))) {
				BT_ERR("No memmory for new connection");
				hci_dev_unlock(hdev);
				return;
			}
		}
		conn->state = BT_CONNECT;
		hci_dev_unlock(hdev);

		bacpy(&ac.bdaddr, &cr->bdaddr);
	
		if (lmp_rswitch_capable(hdev) && (mask & HCI_LM_MASTER))
			ac.role = 0x00; /* Become master */
		else
			ac.role = 0x01; /* Remain slave */

		hci_send_cmd(hdev, OGF_LINK_CTL, OCF_ACCEPT_CONN_REQ, 
				ACCEPT_CONN_REQ_CP_SIZE, &ac);
	} else {
		/* Connection rejected */
		reject_conn_req_cp rc;

		bacpy(&rc.bdaddr, &cr->bdaddr);
		rc.reason = 0x0f;
		hci_send_cmd(hdev, OGF_LINK_CTL, OCF_REJECT_CONN_REQ,
				REJECT_CONN_REQ_CP_SIZE, &rc);
	}
}

/* Connect Complete */
static inline void hci_conn_complete_evt(struct hci_dev *hdev, struct sk_buff *skb)
{
	evt_conn_complete *cc = (evt_conn_complete *) skb->data;
	struct hci_conn *conn = NULL;

	BT_DBG("%s", hdev->name);

	hci_dev_lock(hdev);
	
	conn = conn_hash_lookup_ba(hdev, cc->link_type, &cc->bdaddr);
	if (!conn) {
		hci_dev_unlock(hdev);
		return;
	}

	if (!cc->status) {
		conn->handle = __le16_to_cpu(cc->handle);
		conn->state  = BT_CONNECTED;

		if (test_bit(HCI_AUTH, &hdev->flags))
			conn->link_mode |= HCI_LM_AUTH;
		
		if (test_bit(HCI_ENCRYPT, &hdev->flags))
			conn->link_mode |= HCI_LM_ENCRYPT;


		/* Set link policy */
		if (conn->type == ACL_LINK && hdev->link_policy) {
			write_link_policy_cp lp;
			lp.handle = cc->handle;
			lp.policy = __cpu_to_le16(hdev->link_policy);
			hci_send_cmd(hdev, OGF_LINK_POLICY, OCF_WRITE_LINK_POLICY,
				WRITE_LINK_POLICY_CP_SIZE, &lp);
		}

		/* Set packet type for incomming connection */
		if (!conn->out) {
			change_conn_ptype_cp cp;
			cp.handle = cc->handle;
			cp.pkt_type = (conn->type == ACL_LINK) ? 
				__cpu_to_le16(hdev->pkt_type & ACL_PTYPE_MASK):
				__cpu_to_le16(hdev->pkt_type & SCO_PTYPE_MASK);

			hci_send_cmd(hdev, OGF_LINK_CTL, OCF_CHANGE_CONN_PTYPE,
				CHANGE_CONN_PTYPE_CP_SIZE, &cp);
		}
	} else
		conn->state = BT_CLOSED;

	if (conn->type == ACL_LINK) {
		struct hci_conn *sco = conn->link;
		if (sco) {
			if (!cc->status)
				hci_add_sco(sco, conn->handle);
			else {
				hci_proto_connect_cfm(sco, cc->status);
				hci_conn_del(sco);
			}
		}
	}

	hci_proto_connect_cfm(conn, cc->status);
	if (cc->status)
		hci_conn_del(conn);

	hci_dev_unlock(hdev);
}

/* Disconnect Complete */
static inline void hci_disconn_complete_evt(struct hci_dev *hdev, struct sk_buff *skb)
{
	evt_disconn_complete *dc = (evt_disconn_complete *) skb->data;
	struct hci_conn *conn = NULL;
	__u16 handle = __le16_to_cpu(dc->handle);

	BT_DBG("%s status %d", hdev->name, dc->status);

	if (dc->status)
		return;

	hci_dev_lock(hdev);
	
	conn = conn_hash_lookup_handle(hdev, handle);
	if (conn) {
		conn->state = BT_CLOSED;
		hci_proto_disconn_ind(conn, dc->reason);
		hci_conn_del(conn);
	}

	hci_dev_unlock(hdev);
}

/* Number of completed packets */
static inline void hci_num_comp_pkts_evt(struct hci_dev *hdev, struct sk_buff *skb)
{
	evt_num_comp_pkts *nc = (evt_num_comp_pkts *) skb->data;
	__u16 *ptr;
	int i;

	skb_pull(skb, EVT_NUM_COMP_PKTS_SIZE);

	BT_DBG("%s num_hndl %d", hdev->name, nc->num_hndl);

	if (skb->len < nc->num_hndl * 4) {
		BT_DBG("%s bad parameters", hdev->name);
		return;
	}

	tasklet_disable(&hdev->tx_task);

	for (i = 0, ptr = (__u16 *) skb->data; i < nc->num_hndl; i++) {
		struct hci_conn *conn;
		__u16  handle, count;

		handle = __le16_to_cpu(get_unaligned(ptr++));
		count  = __le16_to_cpu(get_unaligned(ptr++));

		conn = conn_hash_lookup_handle(hdev, handle);
		if (conn) {
			conn->sent -= count;

			if (conn->type == SCO_LINK) {
				if ((hdev->sco_cnt += count) > hdev->sco_pkts)
					hdev->sco_cnt = hdev->sco_pkts;
			} else {
				if ((hdev->acl_cnt += count) > hdev->acl_pkts)
					hdev->acl_cnt = hdev->acl_pkts;
			}
		}
	}
	hci_sched_tx(hdev);

	tasklet_enable(&hdev->tx_task);
}

/* Role Change */
static inline void hci_role_change_evt(struct hci_dev *hdev, struct sk_buff *skb)
{
	evt_role_change *rc = (evt_role_change *) skb->data;
	struct hci_conn *conn = NULL;

	BT_DBG("%s status %d", hdev->name, rc->status);

	if (rc->status)
		return;

	hci_dev_lock(hdev);
	
	conn = conn_hash_lookup_ba(hdev, ACL_LINK, &rc->bdaddr);
	if (conn) {
		if (rc->role)
			conn->link_mode &= ~HCI_LM_MASTER;
		else 
			conn->link_mode |= HCI_LM_MASTER;
	}

	hci_dev_unlock(hdev);
}

/* Authentication Complete */
static inline void hci_auth_complete_evt(struct hci_dev *hdev, struct sk_buff *skb)
{
	evt_auth_complete *ac = (evt_auth_complete *) skb->data;
	struct hci_conn *conn = NULL;
	__u16 handle = __le16_to_cpu(ac->handle);

	BT_DBG("%s status %d", hdev->name, ac->status);

	hci_dev_lock(hdev);
	
	conn = conn_hash_lookup_handle(hdev, handle);
	if (conn) {
		if (!ac->status)
			conn->link_mode |= HCI_LM_AUTH;
		clear_bit(HCI_CONN_AUTH_PEND, &conn->pend);

		hci_proto_auth_cfm(conn, ac->status);
		
		if (test_bit(HCI_CONN_ENCRYPT_PEND, &conn->pend)) {
			if (!ac->status) {
				set_conn_encrypt_cp ce;
				ce.handle  = __cpu_to_le16(conn->handle);
				ce.encrypt = 1;
				hci_send_cmd(conn->hdev, OGF_LINK_CTL,
						OCF_SET_CONN_ENCRYPT,
						SET_CONN_ENCRYPT_CP_SIZE, &ce);
			} else {
				clear_bit(HCI_CONN_ENCRYPT_PEND, &conn->pend);
				hci_proto_encrypt_cfm(conn, ac->status);
			}
		}
	}

	hci_dev_unlock(hdev);
}

/* Encryption Change */
static inline void hci_encrypt_change_evt(struct hci_dev *hdev, struct sk_buff *skb)
{
	evt_encrypt_change *ec = (evt_encrypt_change *) skb->data;
	struct hci_conn *conn = NULL;
	__u16 handle = __le16_to_cpu(ec->handle);

	BT_DBG("%s status %d", hdev->name, ec->status);

	hci_dev_lock(hdev);
	
	conn = conn_hash_lookup_handle(hdev, handle);
	if (conn) {
		if (!ec->status) {
		       	if (ec->encrypt)
				conn->link_mode |= HCI_LM_ENCRYPT;
			else
				conn->link_mode &= ~HCI_LM_ENCRYPT;
		}
		clear_bit(HCI_CONN_ENCRYPT_PEND, &conn->pend);
		
		hci_proto_encrypt_cfm(conn, ec->status);
	}

	hci_dev_unlock(hdev);
}

void hci_event_packet(struct hci_dev *hdev, struct sk_buff *skb)
{
	hci_event_hdr *he = (hci_event_hdr *) skb->data;
	evt_cmd_status *cs;
	evt_cmd_complete *ec;
	__u16 opcode, ocf, ogf;

	skb_pull(skb, HCI_EVENT_HDR_SIZE);

	BT_DBG("%s evt 0x%x", hdev->name, he->evt);

	switch (he->evt) {
	case EVT_NUM_COMP_PKTS:
		hci_num_comp_pkts_evt(hdev, skb);
		break;

	case EVT_INQUIRY_COMPLETE:
		hci_inquiry_complete_evt(hdev, skb);
		break;

	case EVT_INQUIRY_RESULT:
		hci_inquiry_result_evt(hdev, skb);
		break;

	case EVT_INQUIRY_RESULT_WITH_RSSI:
		hci_inquiry_result_with_rssi_evt(hdev, skb);
		break;

	case EVT_CONN_REQUEST:
		hci_conn_request_evt(hdev, skb);
		break;

	case EVT_CONN_COMPLETE:
		hci_conn_complete_evt(hdev, skb);
		break;

	case EVT_DISCONN_COMPLETE:
		hci_disconn_complete_evt(hdev, skb);
		break;

	case EVT_ROLE_CHANGE:
		hci_role_change_evt(hdev, skb);
		break;

	case EVT_AUTH_COMPLETE:
		hci_auth_complete_evt(hdev, skb);
		break;

	case EVT_ENCRYPT_CHANGE:
		hci_encrypt_change_evt(hdev, skb);
		break;

	case EVT_CMD_STATUS:
		cs = (evt_cmd_status *) skb->data;
		skb_pull(skb, EVT_CMD_STATUS_SIZE);
				
		opcode = __le16_to_cpu(cs->opcode);
		ogf = cmd_opcode_ogf(opcode);
		ocf = cmd_opcode_ocf(opcode);

		switch (ogf) {
		case OGF_INFO_PARAM:
			hci_cs_info_param(hdev, ocf, cs->status);
			break;

		case OGF_HOST_CTL:
			hci_cs_host_ctl(hdev, ocf, cs->status);
			break;

		case OGF_LINK_CTL:
			hci_cs_link_ctl(hdev, ocf, cs->status);
			break;

		case OGF_LINK_POLICY:
			hci_cs_link_policy(hdev, ocf, cs->status);
			break;

		default:
			BT_DBG("%s Command Status OGF %x", hdev->name, ogf);
			break;
		};

		if (cs->ncmd) {
			atomic_set(&hdev->cmd_cnt, 1);
			if (!skb_queue_empty(&hdev->cmd_q))
				hci_sched_cmd(hdev);
		}
		break;

	case EVT_CMD_COMPLETE:
		ec = (evt_cmd_complete *) skb->data;
		skb_pull(skb, EVT_CMD_COMPLETE_SIZE);

		opcode = __le16_to_cpu(ec->opcode);
		ogf = cmd_opcode_ogf(opcode);
		ocf = cmd_opcode_ocf(opcode);

		switch (ogf) {
		case OGF_INFO_PARAM:
			hci_cc_info_param(hdev, ocf, skb);
			break;

		case OGF_HOST_CTL:
			hci_cc_host_ctl(hdev, ocf, skb);
			break;

		case OGF_LINK_CTL:
			hci_cc_link_ctl(hdev, ocf, skb);
			break;

		case OGF_LINK_POLICY:
			hci_cc_link_policy(hdev, ocf, skb);
			break;

		default:
			BT_DBG("%s Command Completed OGF %x", hdev->name, ogf);
			break;
		};

		if (ec->ncmd) {
			atomic_set(&hdev->cmd_cnt, 1);
			if (!skb_queue_empty(&hdev->cmd_q))
				hci_sched_cmd(hdev);
		}
		break;
	};

	kfree_skb(skb);
	hdev->stat.evt_rx++;
}

/* General internal stack event */
void hci_si_event(struct hci_dev *hdev, int type, int dlen, void *data)
{
	hci_event_hdr *eh;
	evt_stack_internal *si;
	struct sk_buff *skb;
	int size;
	void *ptr;

	size = HCI_EVENT_HDR_SIZE + EVT_STACK_INTERNAL_SIZE + dlen;
	skb  = bluez_skb_alloc(size, GFP_ATOMIC);
	if (!skb)
		return;

	ptr = skb_put(skb, size);

	eh = ptr;
       	eh->evt  = EVT_STACK_INTERNAL;
	eh->plen = EVT_STACK_INTERNAL_SIZE + dlen;
	ptr += HCI_EVENT_HDR_SIZE;

	si = ptr;
	si->type = type;
	memcpy(si->data, data, dlen);
	
	skb->pkt_type = HCI_EVENT_PKT;
	skb->dev = (void *) hdev;
	hci_send_to_sock(hdev, skb);
	kfree_skb(skb);
}
