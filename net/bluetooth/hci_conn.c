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
 * HCI Connection handling.
 *
 * $Id: hci_conn.c,v 1.5 2002/07/17 18:46:25 maxk Exp $
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

void hci_acl_connect(struct hci_conn *conn)
{
	struct hci_dev *hdev = conn->hdev;
	struct inquiry_entry *ie;
	create_conn_cp cp;

	BT_DBG("%p", conn);

	conn->state = BT_CONNECT;
	conn->out   = 1;
	conn->link_mode = HCI_LM_MASTER;

	memset(&cp, 0, sizeof(cp));
	bacpy(&cp.bdaddr, &conn->dst);
	cp.pscan_rep_mode = 0x02;

	if ((ie = inquiry_cache_lookup(hdev, &conn->dst)) &&
			inquiry_entry_age(ie) <= INQUIRY_ENTRY_AGE_MAX) {
		cp.pscan_rep_mode = ie->info.pscan_rep_mode;
		cp.pscan_mode     = ie->info.pscan_mode;
		cp.clock_offset   = ie->info.clock_offset | __cpu_to_le16(0x8000);
	}

	cp.pkt_type = __cpu_to_le16(hdev->pkt_type & ACL_PTYPE_MASK);
	if (lmp_rswitch_capable(hdev) && !(hdev->link_mode & HCI_LM_MASTER))
		cp.role_switch	= 0x01;
	else
		cp.role_switch	= 0x00;
		
	hci_send_cmd(hdev, OGF_LINK_CTL, OCF_CREATE_CONN,
				CREATE_CONN_CP_SIZE, &cp);
}

void hci_acl_disconn(struct hci_conn *conn, __u8 reason)
{
	disconnect_cp cp;

	BT_DBG("%p", conn);

	conn->state = BT_DISCONN;

	cp.handle = __cpu_to_le16(conn->handle);
	cp.reason = reason;
	hci_send_cmd(conn->hdev, OGF_LINK_CTL, OCF_DISCONNECT,
				DISCONNECT_CP_SIZE, &cp);
}

void hci_add_sco(struct hci_conn *conn, __u16 handle)
{
	struct hci_dev *hdev = conn->hdev;
	add_sco_cp cp;

	BT_DBG("%p", conn);

	conn->state = BT_CONNECT;
	conn->out = 1;

	cp.pkt_type = __cpu_to_le16(hdev->pkt_type & SCO_PTYPE_MASK);
	cp.handle   = __cpu_to_le16(handle);

	hci_send_cmd(hdev, OGF_LINK_CTL, OCF_ADD_SCO, ADD_SCO_CP_SIZE, &cp);
}

static void hci_conn_timeout(unsigned long arg)
{
	struct hci_conn *conn = (void *)arg;
	struct hci_dev  *hdev = conn->hdev;

	BT_DBG("conn %p state %d", conn, conn->state);

	if (atomic_read(&conn->refcnt))
		return;

	hci_dev_lock(hdev);
 	if (conn->state == BT_CONNECTED)
		hci_acl_disconn(conn, 0x13);
	else
		conn->state = BT_CLOSED;
	hci_dev_unlock(hdev);
	return;
}

static void hci_conn_init_timer(struct hci_conn *conn)
{
	init_timer(&conn->timer);
	conn->timer.function = hci_conn_timeout;
	conn->timer.data = (unsigned long)conn;
}

struct hci_conn *hci_conn_add(struct hci_dev *hdev, int type, bdaddr_t *dst)
{
	struct hci_conn *conn;

	BT_DBG("%s dst %s", hdev->name, batostr(dst));

	if (!(conn = kmalloc(sizeof(struct hci_conn), GFP_ATOMIC)))
		return NULL;
	memset(conn, 0, sizeof(struct hci_conn));

	bacpy(&conn->dst, dst);
	conn->type   = type;
	conn->hdev   = hdev;
	conn->state  = BT_OPEN;

	skb_queue_head_init(&conn->data_q);
	hci_conn_init_timer(conn);

	atomic_set(&conn->refcnt, 0);

	hci_dev_hold(hdev);

	tasklet_disable(&hdev->tx_task);
	conn_hash_add(hdev, conn);
	tasklet_enable(&hdev->tx_task);

	return conn;
}

int hci_conn_del(struct hci_conn *conn)
{
	struct hci_dev  *hdev = conn->hdev;

	BT_DBG("%s conn %p handle %d", hdev->name, conn, conn->handle);
	
	hci_conn_del_timer(conn);

	if (conn->type == SCO_LINK) {
		struct hci_conn *acl = conn->link;
		if (acl) {
			acl->link = NULL;
			hci_conn_put(acl);
		}
	} else {
		struct hci_conn *sco = conn->link;
		if (sco)
			sco->link = NULL;

		/* Unacked frames */
		hdev->acl_cnt += conn->sent;
	}

	tasklet_disable(&hdev->tx_task);
	conn_hash_del(hdev, conn);
	tasklet_enable(&hdev->tx_task);

	skb_queue_purge(&conn->data_q);

	hci_dev_put(hdev);

	kfree(conn);
	return 0;
}

struct hci_dev *hci_get_route(bdaddr_t *dst, bdaddr_t *src)
{
	int use_src = bacmp(src, BDADDR_ANY);
	struct hci_dev *hdev = NULL;
	struct list_head *p;

	BT_DBG("%s -> %s", batostr(src), batostr(dst));

	read_lock_bh(&hdev_list_lock);

	list_for_each(p, &hdev_list) {
		struct hci_dev *d;
		d = list_entry(p, struct hci_dev, list);
		
		if (!test_bit(HCI_UP, &d->flags))
			continue;

		/* Simple routing: 
	 	 * 	No source address - find interface with bdaddr != dst 
	 	 *	Source address 	  - find interface with bdaddr == src 
	 	 */

		if (use_src) {
			if (!bacmp(&d->bdaddr, src)) {
				hdev = d; break;
			}
		} else {
			if (bacmp(&d->bdaddr, dst)) {
				hdev = d; break;
			}
		}
	}

	if (hdev)
		hci_dev_hold(hdev);

	read_unlock_bh(&hdev_list_lock);
	return hdev;
}

/* Create SCO or ACL connection.
 * Device _must_ be locked */
struct hci_conn * hci_connect(struct hci_dev *hdev, int type, bdaddr_t *dst)
{
	struct hci_conn *acl;

	BT_DBG("%s dst %s", hdev->name, batostr(dst));

	if (!(acl = conn_hash_lookup_ba(hdev, ACL_LINK, dst))) {
		if (!(acl = hci_conn_add(hdev, ACL_LINK, dst)))
			return NULL;
	}

	hci_conn_hold(acl);

	if (acl->state == BT_OPEN || acl->state == BT_CLOSED)
		hci_acl_connect(acl);

	if (type == SCO_LINK) {
		struct hci_conn *sco;

		if (!(sco = conn_hash_lookup_ba(hdev, SCO_LINK, dst))) {
			if (!(sco = hci_conn_add(hdev, SCO_LINK, dst))) {
				hci_conn_put(acl);
				return NULL;
			}
		}
		acl->link = sco;
		sco->link = acl;

		hci_conn_hold(sco);

		if (acl->state == BT_CONNECTED && 
				(sco->state == BT_OPEN || sco->state == BT_CLOSED))
			hci_add_sco(sco, acl->handle);

		return sco;
	} else {
		return acl;
	}
}

/* Authenticate remote device */
int hci_conn_auth(struct hci_conn *conn)
{
	BT_DBG("conn %p", conn);
	
	if (conn->link_mode & HCI_LM_AUTH)
		return 1;
	
	if (!test_and_set_bit(HCI_CONN_AUTH_PEND, &conn->pend)) {
		auth_requested_cp ar;
		ar.handle = __cpu_to_le16(conn->handle);
		hci_send_cmd(conn->hdev, OGF_LINK_CTL, OCF_AUTH_REQUESTED,
				AUTH_REQUESTED_CP_SIZE, &ar);
	}
	return 0;
}

/* Enable encryption */
int hci_conn_encrypt(struct hci_conn *conn)
{
	BT_DBG("conn %p", conn);
	
	if (conn->link_mode & HCI_LM_ENCRYPT)
		return 1;
	
	if (test_and_set_bit(HCI_CONN_ENCRYPT_PEND, &conn->pend))
		return 0;

	if (hci_conn_auth(conn)) {
		set_conn_encrypt_cp ce;
		ce.handle  = __cpu_to_le16(conn->handle);
		ce.encrypt = 1; 
		hci_send_cmd(conn->hdev, OGF_LINK_CTL, OCF_SET_CONN_ENCRYPT,
				SET_CONN_ENCRYPT_CP_SIZE, &ce);
	}
	return 0;
}

/* Drop all connection on the device */
void hci_conn_hash_flush(struct hci_dev *hdev)
{
	struct conn_hash *h = &hdev->conn_hash;
        struct list_head *p;

	BT_DBG("hdev %s", hdev->name);

	p = h->list.next;
	while (p != &h->list) {
		struct hci_conn *c;

		c = list_entry(p, struct hci_conn, list);
		p = p->next;

		c->state = BT_CLOSED;

		hci_proto_disconn_ind(c, 0x16);
		hci_conn_del(c);
	}
}

int hci_get_conn_list(unsigned long arg)
{
	struct hci_conn_list_req req, *cl;
	struct hci_conn_info *ci;
	struct hci_dev *hdev;
	struct list_head *p;
	int n = 0, size, err;

	if (copy_from_user(&req, (void *) arg, sizeof(req)))
		return -EFAULT;

	if (!req.conn_num || req.conn_num > (PAGE_SIZE * 2) / sizeof(*ci))
		return -EINVAL;

	size = sizeof(req) + req.conn_num * sizeof(*ci);

	if (!(cl = (void *) kmalloc(size, GFP_KERNEL)))
		return -ENOMEM;

	if (!(hdev = hci_dev_get(req.dev_id))) {
		kfree(cl);
		return -ENODEV;
	}

	ci = cl->conn_info;

	hci_dev_lock_bh(hdev);
	list_for_each(p, &hdev->conn_hash.list) {
		register struct hci_conn *c;
		c = list_entry(p, struct hci_conn, list);

		bacpy(&(ci + n)->bdaddr, &c->dst);
		(ci + n)->handle = c->handle;
		(ci + n)->type  = c->type;
		(ci + n)->out   = c->out;
		(ci + n)->state = c->state;
		(ci + n)->link_mode = c->link_mode;
		if (++n >= req.conn_num)
			break;
	}
	hci_dev_unlock_bh(hdev);

	cl->dev_id = hdev->id;
	cl->conn_num = n;
	size = sizeof(req) + n * sizeof(*ci);

	hci_dev_put(hdev);

	err = copy_to_user((void *) arg, cl, size);
	kfree(cl);

	return err ? -EFAULT : 0;
}

int hci_get_conn_info(struct hci_dev *hdev, unsigned long arg)
{
	struct hci_conn_info_req req;
	struct hci_conn_info ci;
	struct hci_conn *conn;
	char *ptr = (void *) arg + sizeof(req);

	if (copy_from_user(&req, (void *) arg, sizeof(req)))
		return -EFAULT;

	hci_dev_lock_bh(hdev);
	conn = conn_hash_lookup_ba(hdev, req.type, &req.bdaddr);
	if (conn) {
		bacpy(&ci.bdaddr, &conn->dst);
		ci.handle = conn->handle;
		ci.type  = conn->type;
		ci.out   = conn->out;
		ci.state = conn->state;
		ci.link_mode = conn->link_mode;
	}
	hci_dev_unlock_bh(hdev);

	if (!conn)
		return -ENOENT;

	return copy_to_user(ptr, &ci, sizeof(ci)) ? -EFAULT : 0;
}
