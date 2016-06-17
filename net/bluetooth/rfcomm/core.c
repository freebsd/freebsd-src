/* 
   RFCOMM implementation for Linux Bluetooth stack (BlueZ).
   Copyright (C) 2002 Maxim Krasnyansky <maxk@qualcomm.com>
   Copyright (C) 2002 Marcel Holtmann <marcel@holtmann.org>

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
   RPN support    -    Dirk Husemann <hud@zurich.ibm.com>
*/

/*
 * RFCOMM core.
 *
 * $Id: core.c,v 1.46 2002/10/18 20:12:12 maxk Exp $
 */

#define __KERNEL_SYSCALLS__

#include <linux/config.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/init.h>
#include <linux/wait.h>
#include <linux/net.h>
#include <linux/proc_fs.h>
#include <net/sock.h>
#include <asm/uaccess.h>
#include <asm/unaligned.h>

#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/l2cap.h>
#include <net/bluetooth/rfcomm.h>

#define VERSION "1.1"

#ifndef CONFIG_BLUEZ_RFCOMM_DEBUG
#undef  BT_DBG
#define BT_DBG(D...)
#endif

struct task_struct *rfcomm_thread;
DECLARE_MUTEX(rfcomm_sem);
unsigned long rfcomm_event;

static LIST_HEAD(session_list);
static atomic_t terminate, running;

static int rfcomm_send_frame(struct rfcomm_session *s, u8 *data, int len);
static int rfcomm_send_sabm(struct rfcomm_session *s, u8 dlci);
static int rfcomm_send_disc(struct rfcomm_session *s, u8 dlci);
static int rfcomm_queue_disc(struct rfcomm_dlc *d);
static int rfcomm_send_nsc(struct rfcomm_session *s, int cr, u8 type);
static int rfcomm_send_pn(struct rfcomm_session *s, int cr, struct rfcomm_dlc *d);
static int rfcomm_send_msc(struct rfcomm_session *s, int cr, u8 dlci, u8 v24_sig);
static int rfcomm_send_test(struct rfcomm_session *s, int cr, u8 *pattern, int len);
static int rfcomm_send_credits(struct rfcomm_session *s, u8 addr, u8 credits);
static void rfcomm_make_uih(struct sk_buff *skb, u8 addr);

static void rfcomm_process_connect(struct rfcomm_session *s);

/* ---- RFCOMM frame parsing macros ---- */
#define __get_dlci(b)     ((b & 0xfc) >> 2)
#define __get_channel(b)  ((b & 0xf8) >> 3)
#define __get_dir(b)      ((b & 0x04) >> 2)
#define __get_type(b)     ((b & 0xef))

#define __test_ea(b)      ((b & 0x01))
#define __test_cr(b)      ((b & 0x02))
#define __test_pf(b)      ((b & 0x10))

#define __addr(cr, dlci)       (((dlci & 0x3f) << 2) | (cr << 1) | 0x01)
#define __ctrl(type, pf)       (((type & 0xef) | (pf << 4)))
#define __dlci(dir, chn)       (((chn & 0x1f) << 1) | dir)
#define __srv_channel(dlci)    (dlci >> 1)
#define __dir(dlci)            (dlci & 0x01)

#define __len8(len)       (((len) << 1) | 1)
#define __len16(len)      ((len) << 1)

/* MCC macros */
#define __mcc_type(cr, type)   (((type << 2) | (cr << 1) | 0x01))
#define __get_mcc_type(b) ((b & 0xfc) >> 2)
#define __get_mcc_len(b)  ((b & 0xfe) >> 1)

/* RPN macros */
#define __rpn_line_settings(data, stop, parity)  ((data & 0x3) | ((stop & 0x1) << 2) | ((parity & 0x3) << 3))
#define __get_rpn_data_bits(line) ((line) & 0x3)
#define __get_rpn_stop_bits(line) (((line) >> 2) & 0x1)
#define __get_rpn_parity(line)    (((line) >> 3) & 0x3)

/* ---- RFCOMM FCS computation ---- */

/* CRC on 2 bytes */
#define __crc(data) (rfcomm_crc_table[rfcomm_crc_table[0xff ^ data[0]] ^ data[1]])

/* FCS on 2 bytes */ 
static inline u8 __fcs(u8 *data)
{
	return (0xff - __crc(data));
}

/* FCS on 3 bytes */ 
static inline u8 __fcs2(u8 *data)
{
	return (0xff - rfcomm_crc_table[__crc(data) ^ data[2]]);
}

/* Check FCS */
static inline int __check_fcs(u8 *data, int type, u8 fcs)
{
	u8 f = __crc(data);

	if (type != RFCOMM_UIH)
		f = rfcomm_crc_table[f ^ data[2]];

	return rfcomm_crc_table[f ^ fcs] != 0xcf;
}

/* ---- L2CAP callbacks ---- */
static void rfcomm_l2state_change(struct sock *sk)
{
	BT_DBG("%p state %d", sk, sk->state);
	rfcomm_schedule(RFCOMM_SCHED_STATE);
}

static void rfcomm_l2data_ready(struct sock *sk, int bytes)
{
	BT_DBG("%p bytes %d", sk, bytes);
	rfcomm_schedule(RFCOMM_SCHED_RX);
}

static int rfcomm_l2sock_create(struct socket **sock)
{
	int err;

	BT_DBG("");

	err = sock_create(PF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP, sock);
	if (!err) {
		struct sock *sk = (*sock)->sk;
		sk->data_ready   = rfcomm_l2data_ready;
		sk->state_change = rfcomm_l2state_change;
	}
	return err;
}

/* ---- RFCOMM DLCs ---- */
static void rfcomm_dlc_timeout(unsigned long arg)
{
	struct rfcomm_dlc *d = (void *) arg;

	BT_DBG("dlc %p state %ld", d, d->state);

	set_bit(RFCOMM_TIMED_OUT, &d->flags);
	rfcomm_dlc_put(d);
	rfcomm_schedule(RFCOMM_SCHED_TIMEO);
}

static void rfcomm_dlc_set_timer(struct rfcomm_dlc *d, long timeout)
{
	BT_DBG("dlc %p state %ld timeout %ld", d, d->state, timeout);

	if (!mod_timer(&d->timer, jiffies + timeout))
		rfcomm_dlc_hold(d);
}

static void rfcomm_dlc_clear_timer(struct rfcomm_dlc *d)
{
	BT_DBG("dlc %p state %ld", d, d->state);

	if (timer_pending(&d->timer) && del_timer(&d->timer))
		rfcomm_dlc_put(d);
}

static void rfcomm_dlc_clear_state(struct rfcomm_dlc *d)
{
	BT_DBG("%p", d);

	d->state      = BT_OPEN;
	d->flags      = 0;
	d->mscex      = 0;
	d->mtu        = RFCOMM_DEFAULT_MTU;
	d->v24_sig    = RFCOMM_V24_RTC | RFCOMM_V24_RTR | RFCOMM_V24_DV;

	d->cfc        = RFCOMM_CFC_DISABLED;
	d->rx_credits = RFCOMM_DEFAULT_CREDITS;
}

struct rfcomm_dlc *rfcomm_dlc_alloc(int prio)
{
	struct rfcomm_dlc *d = kmalloc(sizeof(*d), prio);
	if (!d)
		return NULL;
	memset(d, 0, sizeof(*d));

	init_timer(&d->timer);
	d->timer.function = rfcomm_dlc_timeout;
	d->timer.data = (unsigned long) d;

	skb_queue_head_init(&d->tx_queue);
	spin_lock_init(&d->lock);
	atomic_set(&d->refcnt, 1);

	rfcomm_dlc_clear_state(d);
	
	BT_DBG("%p", d);
	return d;
}

void rfcomm_dlc_free(struct rfcomm_dlc *d)
{
	BT_DBG("%p", d);

	skb_queue_purge(&d->tx_queue);
	kfree(d);
}

static void rfcomm_dlc_link(struct rfcomm_session *s, struct rfcomm_dlc *d)
{
	BT_DBG("dlc %p session %p", d, s);

	rfcomm_session_hold(s);

	rfcomm_dlc_hold(d);
	list_add(&d->list, &s->dlcs);
	d->session = s;
}

static void rfcomm_dlc_unlink(struct rfcomm_dlc *d)
{
	struct rfcomm_session *s = d->session;

	BT_DBG("dlc %p refcnt %d session %p", d, atomic_read(&d->refcnt), s);

	list_del(&d->list);
	d->session = NULL;
	rfcomm_dlc_put(d);

	rfcomm_session_put(s);
}

static struct rfcomm_dlc *rfcomm_dlc_get(struct rfcomm_session *s, u8 dlci)
{
	struct rfcomm_dlc *d;
	struct list_head *p;

	list_for_each(p, &s->dlcs) {
		d = list_entry(p, struct rfcomm_dlc, list);
		if (d->dlci == dlci)
			return d;
	}
	return NULL;
}

static int __rfcomm_dlc_open(struct rfcomm_dlc *d, bdaddr_t *src, bdaddr_t *dst, u8 channel)
{
	struct rfcomm_session *s;
	int err = 0;
	u8 dlci;

	BT_DBG("dlc %p state %ld %s %s channel %d", 
			d, d->state, batostr(src), batostr(dst), channel);

	if (channel < 1 || channel > 30)
		return -EINVAL;

	if (d->state != BT_OPEN && d->state != BT_CLOSED)
		return 0;

	s = rfcomm_session_get(src, dst);
	if (!s) {
		s = rfcomm_session_create(src, dst, &err);
		if (!s)
			return err;
	}

	dlci = __dlci(!s->initiator, channel);

	/* Check if DLCI already exists */
	if (rfcomm_dlc_get(s, dlci))
		return -EBUSY;

	rfcomm_dlc_clear_state(d);

	d->dlci     = dlci;
	d->addr     = __addr(s->initiator, dlci);
	d->priority = 7;

	d->state    = BT_CONFIG;
	rfcomm_dlc_link(s, d);

	d->mtu = s->mtu;
	d->cfc = (s->cfc == RFCOMM_CFC_UNKNOWN) ? 0 : s->cfc;

	if (s->state == BT_CONNECTED)
		rfcomm_send_pn(s, 1, d);
	rfcomm_dlc_set_timer(d, RFCOMM_CONN_TIMEOUT);
	return 0;
}

int rfcomm_dlc_open(struct rfcomm_dlc *d, bdaddr_t *src, bdaddr_t *dst, u8 channel)
{
	mm_segment_t fs;
	int r;

	rfcomm_lock();

	fs = get_fs(); set_fs(KERNEL_DS);
	r = __rfcomm_dlc_open(d, src, dst, channel);
	set_fs(fs);

	rfcomm_unlock();
	return r;
}

static int __rfcomm_dlc_close(struct rfcomm_dlc *d, int err)
{
	struct rfcomm_session *s = d->session;
	if (!s)
		return 0;

	BT_DBG("dlc %p state %ld dlci %d err %d session %p",
			d, d->state, d->dlci, err, s);

	switch (d->state) {
	case BT_CONNECTED:
	case BT_CONFIG:
	case BT_CONNECT:
		d->state = BT_DISCONN;
		if (skb_queue_empty(&d->tx_queue)) {
			rfcomm_send_disc(s, d->dlci);
			rfcomm_dlc_set_timer(d, RFCOMM_DISC_TIMEOUT);
		} else {
			rfcomm_queue_disc(d);
			rfcomm_dlc_set_timer(d, RFCOMM_DISC_TIMEOUT * 2);
		}
		break;

	default:
		rfcomm_dlc_clear_timer(d);

		rfcomm_dlc_lock(d);
		d->state = BT_CLOSED;
		d->state_change(d, err);
		rfcomm_dlc_unlock(d);

		skb_queue_purge(&d->tx_queue);
		rfcomm_dlc_unlink(d);
	}

	return 0;
}

int rfcomm_dlc_close(struct rfcomm_dlc *d, int err)
{
	mm_segment_t fs;
	int r;

	rfcomm_lock();

	fs = get_fs(); set_fs(KERNEL_DS);
	r = __rfcomm_dlc_close(d, err);
	set_fs(fs);

	rfcomm_unlock();
	return r;
}

int rfcomm_dlc_send(struct rfcomm_dlc *d, struct sk_buff *skb)
{
	int len = skb->len;

	if (d->state != BT_CONNECTED)
		return -ENOTCONN;

	BT_DBG("dlc %p mtu %d len %d", d, d->mtu, len);

	if (len > d->mtu)
		return -EINVAL;

	rfcomm_make_uih(skb, d->addr);
	skb_queue_tail(&d->tx_queue, skb);

	if (!test_bit(RFCOMM_TX_THROTTLED, &d->flags))
		rfcomm_schedule(RFCOMM_SCHED_TX);
	return len;
}

void __rfcomm_dlc_throttle(struct rfcomm_dlc *d)
{
	BT_DBG("dlc %p state %ld", d, d->state);

	if (!d->cfc) {
		d->v24_sig |= RFCOMM_V24_FC;
		set_bit(RFCOMM_MSC_PENDING, &d->flags);
	}
	rfcomm_schedule(RFCOMM_SCHED_TX);
}

void __rfcomm_dlc_unthrottle(struct rfcomm_dlc *d)
{
	BT_DBG("dlc %p state %ld", d, d->state);

	if (!d->cfc) {
		d->v24_sig &= ~RFCOMM_V24_FC;
		set_bit(RFCOMM_MSC_PENDING, &d->flags);
	}
	rfcomm_schedule(RFCOMM_SCHED_TX);
}

/* 
   Set/get modem status functions use _local_ status i.e. what we report
   to the other side.
   Remote status is provided by dlc->modem_status() callback.
 */
int rfcomm_dlc_set_modem_status(struct rfcomm_dlc *d, u8 v24_sig)
{
	BT_DBG("dlc %p state %ld v24_sig 0x%x", 
			d, d->state, v24_sig);

	if (test_bit(RFCOMM_RX_THROTTLED, &d->flags))
		v24_sig |= RFCOMM_V24_FC;
	else
		v24_sig &= ~RFCOMM_V24_FC;
	
	d->v24_sig = v24_sig;

	if (!test_and_set_bit(RFCOMM_MSC_PENDING, &d->flags))
		rfcomm_schedule(RFCOMM_SCHED_TX);

	return 0;
}

int rfcomm_dlc_get_modem_status(struct rfcomm_dlc *d, u8 *v24_sig)
{
	BT_DBG("dlc %p state %ld v24_sig 0x%x", 
			d, d->state, d->v24_sig);

	*v24_sig = d->v24_sig;
	return 0;
}

/* ---- RFCOMM sessions ---- */
struct rfcomm_session *rfcomm_session_add(struct socket *sock, int state)
{
	struct rfcomm_session *s = kmalloc(sizeof(*s), GFP_KERNEL);
	if (!s)
		return NULL;
	memset(s, 0, sizeof(*s));
	
	BT_DBG("session %p sock %p", s, sock);

	INIT_LIST_HEAD(&s->dlcs);
	s->state = state;
	s->sock  = sock;

	s->mtu   = RFCOMM_DEFAULT_MTU;
	s->cfc   = RFCOMM_CFC_UNKNOWN;
	
	list_add(&s->list, &session_list);

	/* Do not increment module usage count for listeting sessions.
	 * Otherwise we won't be able to unload the module. */
	if (state != BT_LISTEN)
		MOD_INC_USE_COUNT;
	return s;
}

void rfcomm_session_del(struct rfcomm_session *s)
{
	int state = s->state;
	
	BT_DBG("session %p state %ld", s, s->state);

	list_del(&s->list);

	if (state == BT_CONNECTED)
		rfcomm_send_disc(s, 0);

	sock_release(s->sock);
	kfree(s);

	if (state != BT_LISTEN)
		MOD_DEC_USE_COUNT;
}

struct rfcomm_session *rfcomm_session_get(bdaddr_t *src, bdaddr_t *dst)
{
	struct rfcomm_session *s;
	struct list_head *p, *n;
	struct bluez_pinfo *pi;
	list_for_each_safe(p, n, &session_list) {
		s = list_entry(p, struct rfcomm_session, list);
		pi = bluez_pi(s->sock->sk); 

		if ((!bacmp(src, BDADDR_ANY) || !bacmp(&pi->src, src)) &&
				!bacmp(&pi->dst, dst))
			return s;
	}
	return NULL;
}

void rfcomm_session_close(struct rfcomm_session *s, int err)
{
	struct rfcomm_dlc *d;
	struct list_head *p, *n;

	BT_DBG("session %p state %ld err %d", s, s->state, err);

	rfcomm_session_hold(s);

	s->state = BT_CLOSED;

	/* Close all dlcs */
	list_for_each_safe(p, n, &s->dlcs) {
		d = list_entry(p, struct rfcomm_dlc, list);
		d->state = BT_CLOSED;
		__rfcomm_dlc_close(d, err);
	}

	rfcomm_session_put(s);
}

struct rfcomm_session *rfcomm_session_create(bdaddr_t *src, bdaddr_t *dst, int *err)
{
	struct rfcomm_session *s = NULL;
	struct sockaddr_l2 addr;
	struct l2cap_options opts;
	struct socket *sock;
	int    size;

	BT_DBG("%s %s", batostr(src), batostr(dst));

	*err = rfcomm_l2sock_create(&sock);
	if (*err < 0)
		return NULL;

	bacpy(&addr.l2_bdaddr, src);
	addr.l2_family = AF_BLUETOOTH;
	addr.l2_psm    = 0;
	*err = sock->ops->bind(sock, (struct sockaddr *) &addr, sizeof(addr));
	if (*err < 0)
		goto failed;

	/* Set L2CAP options */
	size = sizeof(opts);
	sock->ops->getsockopt(sock, SOL_L2CAP, L2CAP_OPTIONS, (void *)&opts, &size);
	
	opts.imtu = RFCOMM_MAX_L2CAP_MTU;
	sock->ops->setsockopt(sock, SOL_L2CAP, L2CAP_OPTIONS, (void *)&opts, size);

	s = rfcomm_session_add(sock, BT_BOUND);
	if (!s) {
		*err = -ENOMEM;
		goto failed;
	}

	s->initiator = 1;

	bacpy(&addr.l2_bdaddr, dst);
	addr.l2_family = AF_BLUETOOTH;
	addr.l2_psm    = htobs(RFCOMM_PSM);
	*err = sock->ops->connect(sock, (struct sockaddr *) &addr, sizeof(addr), O_NONBLOCK);
	if (*err == 0 || *err == -EAGAIN)
		return s;

	rfcomm_session_del(s);
	return NULL;

failed:
	sock_release(sock);
	return NULL;
}

void rfcomm_session_getaddr(struct rfcomm_session *s, bdaddr_t *src, bdaddr_t *dst)
{
	struct sock *sk = s->sock->sk;
	if (src)
		bacpy(src, &bluez_pi(sk)->src);
	if (dst)
		bacpy(dst, &bluez_pi(sk)->dst);
}

/* ---- RFCOMM frame sending ---- */
static int rfcomm_send_frame(struct rfcomm_session *s, u8 *data, int len)
{
	struct socket *sock = s->sock;
	struct iovec iv = { data, len };
	struct msghdr msg;
	int err;

	BT_DBG("session %p len %d", s, len);

	memset(&msg, 0, sizeof(msg));
	msg.msg_iovlen = 1;
	msg.msg_iov = &iv;

	err = sock->ops->sendmsg(sock, &msg, len, 0);
	return err;
}

static int rfcomm_send_sabm(struct rfcomm_session *s, u8 dlci)
{
	struct rfcomm_cmd cmd;

	BT_DBG("%p dlci %d", s, dlci);

	cmd.addr = __addr(s->initiator, dlci);
	cmd.ctrl = __ctrl(RFCOMM_SABM, 1);
	cmd.len  = __len8(0);
	cmd.fcs  = __fcs2((u8 *) &cmd);

	return rfcomm_send_frame(s, (void *) &cmd, sizeof(cmd));
}

static int rfcomm_send_ua(struct rfcomm_session *s, u8 dlci)
{
	struct rfcomm_cmd cmd;

	BT_DBG("%p dlci %d", s, dlci);

	cmd.addr = __addr(!s->initiator, dlci);
	cmd.ctrl = __ctrl(RFCOMM_UA, 1);
	cmd.len  = __len8(0);
	cmd.fcs  = __fcs2((u8 *) &cmd);

	return rfcomm_send_frame(s, (void *) &cmd, sizeof(cmd));
}

static int rfcomm_send_disc(struct rfcomm_session *s, u8 dlci)
{
	struct rfcomm_cmd cmd;

	BT_DBG("%p dlci %d", s, dlci);

	cmd.addr = __addr(s->initiator, dlci);
	cmd.ctrl = __ctrl(RFCOMM_DISC, 1);
	cmd.len  = __len8(0);
	cmd.fcs  = __fcs2((u8 *) &cmd);

	return rfcomm_send_frame(s, (void *) &cmd, sizeof(cmd));
}

static int rfcomm_queue_disc(struct rfcomm_dlc *d)
{
	struct rfcomm_cmd *cmd;
	struct sk_buff *skb;

	BT_DBG("dlc %p dlci %d", d, d->dlci);

	skb = alloc_skb(sizeof(*cmd), GFP_KERNEL);
	if (!skb)
		return -ENOMEM;

	cmd = (void *) __skb_put(skb, sizeof(*cmd));
	cmd->addr = d->addr;
	cmd->ctrl = __ctrl(RFCOMM_DISC, 1);
	cmd->len  = __len8(0);
	cmd->fcs  = __fcs2((u8 *) cmd);

	skb_queue_tail(&d->tx_queue, skb);
	rfcomm_schedule(RFCOMM_SCHED_TX);
	return 0;
}

static int rfcomm_send_dm(struct rfcomm_session *s, u8 dlci)
{
	struct rfcomm_cmd cmd;

	BT_DBG("%p dlci %d", s, dlci);

	cmd.addr = __addr(!s->initiator, dlci);
	cmd.ctrl = __ctrl(RFCOMM_DM, 1);
	cmd.len  = __len8(0);
	cmd.fcs  = __fcs2((u8 *) &cmd);

	return rfcomm_send_frame(s, (void *) &cmd, sizeof(cmd));
}

static int rfcomm_send_nsc(struct rfcomm_session *s, int cr, u8 type)
{
	struct rfcomm_hdr *hdr;
	struct rfcomm_mcc *mcc;
	u8 buf[16], *ptr = buf;

	BT_DBG("%p cr %d type %d", s, cr, type);

	hdr = (void *) ptr; ptr += sizeof(*hdr);
	hdr->addr = __addr(s->initiator, 0);
	hdr->ctrl = __ctrl(RFCOMM_UIH, 0);
	hdr->len  = __len8(sizeof(*mcc) + 1);

	mcc = (void *) ptr; ptr += sizeof(*mcc);
	mcc->type = __mcc_type(cr, RFCOMM_NSC);
	mcc->len  = __len8(1);

	/* Type that we didn't like */
	*ptr = __mcc_type(cr, type); ptr++;

	*ptr = __fcs(buf); ptr++;

	return rfcomm_send_frame(s, buf, ptr - buf);
}

static int rfcomm_send_pn(struct rfcomm_session *s, int cr, struct rfcomm_dlc *d)
{
	struct rfcomm_hdr *hdr;
	struct rfcomm_mcc *mcc;
	struct rfcomm_pn  *pn;
	u8 buf[16], *ptr = buf;

	BT_DBG("%p cr %d dlci %d mtu %d", s, cr, d->dlci, d->mtu);

	hdr = (void *) ptr; ptr += sizeof(*hdr);
	hdr->addr = __addr(s->initiator, 0);
	hdr->ctrl = __ctrl(RFCOMM_UIH, 0);
	hdr->len  = __len8(sizeof(*mcc) + sizeof(*pn));

	mcc = (void *) ptr; ptr += sizeof(*mcc);
	mcc->type = __mcc_type(cr, RFCOMM_PN);
	mcc->len  = __len8(sizeof(*pn));

	pn = (void *) ptr; ptr += sizeof(*pn);
	pn->dlci        = d->dlci;
	pn->priority    = d->priority;
	pn->ack_timer   = 0;
	pn->max_retrans = 0;

	if (s->cfc) {
		pn->flow_ctrl = cr ? 0xf0 : 0xe0;
		pn->credits = RFCOMM_DEFAULT_CREDITS;
	} else {
		pn->flow_ctrl = 0;
		pn->credits   = 0;
	}

	pn->mtu = htobs(d->mtu);

	*ptr = __fcs(buf); ptr++;

	return rfcomm_send_frame(s, buf, ptr - buf);
}

static int rfcomm_send_rpn(struct rfcomm_session *s, int cr, u8 dlci,
			   u8 bit_rate, u8 data_bits, u8 stop_bits,
			   u8 parity, u8 flow_ctrl_settings, 
			   u8 xon_char, u8 xoff_char, u16 param_mask)
{
	struct rfcomm_hdr *hdr;
	struct rfcomm_mcc *mcc;
	struct rfcomm_rpn *rpn;
	u8 buf[16], *ptr = buf;

	BT_DBG("%p cr %d dlci %d bit_r 0x%x data_b 0x%x stop_b 0x%x parity 0x%x"
	       "flwc_s 0x%x xon_c 0x%x xoff_c 0x%x p_mask 0x%x", 
			s, cr, dlci, bit_rate, data_bits, stop_bits, parity, 
			flow_ctrl_settings, xon_char, xoff_char, param_mask);

	hdr = (void *) ptr; ptr += sizeof(*hdr);
	hdr->addr = __addr(s->initiator, 0);
	hdr->ctrl = __ctrl(RFCOMM_UIH, 0);
	hdr->len  = __len8(sizeof(*mcc) + sizeof(*rpn));

	mcc = (void *) ptr; ptr += sizeof(*mcc);
	mcc->type = __mcc_type(cr, RFCOMM_RPN);
	mcc->len  = __len8(sizeof(*rpn));

	rpn = (void *) ptr; ptr += sizeof(*rpn);
	rpn->dlci          = __addr(1, dlci);
	rpn->bit_rate      = bit_rate;
	rpn->line_settings = __rpn_line_settings(data_bits, stop_bits, parity);
	rpn->flow_ctrl     = flow_ctrl_settings;
	rpn->xon_char      = xon_char;
	rpn->xoff_char     = xoff_char;
	rpn->param_mask    = param_mask;

	*ptr = __fcs(buf); ptr++;

	return rfcomm_send_frame(s, buf, ptr - buf);
}

static int rfcomm_send_rls(struct rfcomm_session *s, int cr, u8 dlci, u8 status)
{
	struct rfcomm_hdr *hdr;
	struct rfcomm_mcc *mcc;
	struct rfcomm_rls *rls;
	u8 buf[16], *ptr = buf;

	BT_DBG("%p cr %d status 0x%x", s, cr, status);

	hdr = (void *) ptr; ptr += sizeof(*hdr);
	hdr->addr = __addr(s->initiator, 0);
	hdr->ctrl = __ctrl(RFCOMM_UIH, 0);
	hdr->len  = __len8(sizeof(*mcc) + sizeof(*rls));

	mcc = (void *) ptr; ptr += sizeof(*mcc);
	mcc->type = __mcc_type(cr, RFCOMM_RLS);
	mcc->len  = __len8(sizeof(*rls));

	rls = (void *) ptr; ptr += sizeof(*rls);
	rls->dlci   = __addr(1, dlci);
	rls->status = status;

	*ptr = __fcs(buf); ptr++;

	return rfcomm_send_frame(s, buf, ptr - buf);
}

static int rfcomm_send_msc(struct rfcomm_session *s, int cr, u8 dlci, u8 v24_sig)
{
	struct rfcomm_hdr *hdr;
	struct rfcomm_mcc *mcc;
	struct rfcomm_msc *msc;
	u8 buf[16], *ptr = buf;

	BT_DBG("%p cr %d v24 0x%x", s, cr, v24_sig);

	hdr = (void *) ptr; ptr += sizeof(*hdr);
	hdr->addr = __addr(s->initiator, 0);
	hdr->ctrl = __ctrl(RFCOMM_UIH, 0);
	hdr->len  = __len8(sizeof(*mcc) + sizeof(*msc));

	mcc = (void *) ptr; ptr += sizeof(*mcc);
	mcc->type = __mcc_type(cr, RFCOMM_MSC);
	mcc->len  = __len8(sizeof(*msc));

	msc = (void *) ptr; ptr += sizeof(*msc);
	msc->dlci    = __addr(1, dlci);
	msc->v24_sig = v24_sig | 0x01;

	*ptr = __fcs(buf); ptr++;

	return rfcomm_send_frame(s, buf, ptr - buf);
}

static int rfcomm_send_fcoff(struct rfcomm_session *s, int cr)
{
	struct rfcomm_hdr *hdr;
	struct rfcomm_mcc *mcc;
	u8 buf[16], *ptr = buf;

	BT_DBG("%p cr %d", s, cr);

	hdr = (void *) ptr; ptr += sizeof(*hdr);
	hdr->addr = __addr(s->initiator, 0);
	hdr->ctrl = __ctrl(RFCOMM_UIH, 0);
	hdr->len  = __len8(sizeof(*mcc));

	mcc = (void *) ptr; ptr += sizeof(*mcc);
	mcc->type = __mcc_type(cr, RFCOMM_FCOFF);
	mcc->len  = __len8(0);

	*ptr = __fcs(buf); ptr++;

	return rfcomm_send_frame(s, buf, ptr - buf);
}

static int rfcomm_send_fcon(struct rfcomm_session *s, int cr)
{
	struct rfcomm_hdr *hdr;
	struct rfcomm_mcc *mcc;
	u8 buf[16], *ptr = buf;

	BT_DBG("%p cr %d", s, cr);

	hdr = (void *) ptr; ptr += sizeof(*hdr);
	hdr->addr = __addr(s->initiator, 0);
	hdr->ctrl = __ctrl(RFCOMM_UIH, 0);
	hdr->len  = __len8(sizeof(*mcc));

	mcc = (void *) ptr; ptr += sizeof(*mcc);
	mcc->type = __mcc_type(cr, RFCOMM_FCON);
	mcc->len  = __len8(0);

	*ptr = __fcs(buf); ptr++;

	return rfcomm_send_frame(s, buf, ptr - buf);
}

static int rfcomm_send_test(struct rfcomm_session *s, int cr, u8 *pattern, int len)
{
	struct socket *sock = s->sock;
	struct iovec iv[3];
	struct msghdr msg;
	unsigned char hdr[5], crc[1];

	if (len > 125)
		return -EINVAL;

	BT_DBG("%p cr %d", s, cr);

	hdr[0] = __addr(s->initiator, 0);
	hdr[1] = __ctrl(RFCOMM_UIH, 0);
	hdr[2] = 0x01 | ((len + 2) << 1);
	hdr[3] = 0x01 | ((cr & 0x01) << 1) | (RFCOMM_TEST << 2);
	hdr[4] = 0x01 | (len << 1);

	crc[0] = __fcs(hdr);

	iv[0].iov_base = hdr;
	iv[0].iov_len  = 5;
	iv[1].iov_base = pattern;
	iv[1].iov_len  = len;
	iv[2].iov_base = crc;
	iv[2].iov_len  = 1;

	memset(&msg, 0, sizeof(msg));
	msg.msg_iovlen = 3;
	msg.msg_iov = iv;
	return sock->ops->sendmsg(sock, &msg, 6 + len, 0);
}

static int rfcomm_send_credits(struct rfcomm_session *s, u8 addr, u8 credits)
{
	struct rfcomm_hdr *hdr;
	u8 buf[16], *ptr = buf;

	BT_DBG("%p addr %d credits %d", s, addr, credits);

	hdr = (void *) ptr; ptr += sizeof(*hdr);
	hdr->addr = addr;
	hdr->ctrl = __ctrl(RFCOMM_UIH, 1);
	hdr->len  = __len8(0);

	*ptr = credits; ptr++;

	*ptr = __fcs(buf); ptr++;

	return rfcomm_send_frame(s, buf, ptr - buf);
}

static void rfcomm_make_uih(struct sk_buff *skb, u8 addr)
{
	struct rfcomm_hdr *hdr;
	int len = skb->len;
	u8 *crc;

	if (len > 127) {
		hdr = (void *) skb_push(skb, 4);
		put_unaligned(htobs(__len16(len)), (u16 *) &hdr->len);
	} else {
		hdr = (void *) skb_push(skb, 3);
		hdr->len = __len8(len);
	}
	hdr->addr = addr;
	hdr->ctrl = __ctrl(RFCOMM_UIH, 0);

	crc = skb_put(skb, 1);
	*crc = __fcs((void *) hdr);
}

/* ---- RFCOMM frame reception ---- */
static int rfcomm_recv_ua(struct rfcomm_session *s, u8 dlci)
{
	BT_DBG("session %p state %ld dlci %d", s, s->state, dlci);

	if (dlci) {
		/* Data channel */
		struct rfcomm_dlc *d = rfcomm_dlc_get(s, dlci);
		if (!d) {
			rfcomm_send_dm(s, dlci);
			return 0;
		}

		switch (d->state) {
		case BT_CONNECT:
			rfcomm_dlc_clear_timer(d);

			rfcomm_dlc_lock(d);
			d->state = BT_CONNECTED;
			d->state_change(d, 0);
			rfcomm_dlc_unlock(d);

			rfcomm_send_msc(s, 1, dlci, d->v24_sig);
			break;

		case BT_DISCONN:
			d->state = BT_CLOSED;
			__rfcomm_dlc_close(d, 0);
			break;
		}
	} else {
		/* Control channel */
		switch (s->state) {
		case BT_CONNECT:
			s->state = BT_CONNECTED;
			rfcomm_process_connect(s);
			break;
		}
	}
	return 0;
}

static int rfcomm_recv_dm(struct rfcomm_session *s, u8 dlci)
{
	int err = 0;

	BT_DBG("session %p state %ld dlci %d", s, s->state, dlci);

	if (dlci) {
		/* Data DLC */
		struct rfcomm_dlc *d = rfcomm_dlc_get(s, dlci);
		if (d) {
			if (d->state == BT_CONNECT || d->state == BT_CONFIG)
				err = ECONNREFUSED;
			else
				err = ECONNRESET;

			d->state = BT_CLOSED;
			__rfcomm_dlc_close(d, err);
		}
	} else {
		if (s->state == BT_CONNECT)
			err = ECONNREFUSED;
		else
			err = ECONNRESET;

		s->state = BT_CLOSED;
		rfcomm_session_close(s, err);
	}
	return 0;
}

static int rfcomm_recv_disc(struct rfcomm_session *s, u8 dlci)
{
	int err = 0;

	BT_DBG("session %p state %ld dlci %d", s, s->state, dlci);

	if (dlci) {
		struct rfcomm_dlc *d = rfcomm_dlc_get(s, dlci);
		if (d) {
			rfcomm_send_ua(s, dlci);

			if (d->state == BT_CONNECT || d->state == BT_CONFIG)
				err = ECONNREFUSED;
			else
				err = ECONNRESET;

			d->state = BT_CLOSED;
			__rfcomm_dlc_close(d, err);
		} else 
			rfcomm_send_dm(s, dlci);
			
	} else {
		rfcomm_send_ua(s, 0);

		if (s->state == BT_CONNECT)
			err = ECONNREFUSED;
		else
			err = ECONNRESET;

		s->state = BT_CLOSED;
		rfcomm_session_close(s, err);
	}

	return 0;
}

static int rfcomm_recv_sabm(struct rfcomm_session *s, u8 dlci)
{
	struct rfcomm_dlc *d;
	u8 channel;

	BT_DBG("session %p state %ld dlci %d", s, s->state, dlci);

	if (!dlci) {
		rfcomm_send_ua(s, 0);

		if (s->state == BT_OPEN) {
			s->state = BT_CONNECTED;
			rfcomm_process_connect(s);
		}
		return 0;
	}

	/* Check if DLC exists */
	d = rfcomm_dlc_get(s, dlci);
	if (d) {
		if (d->state == BT_OPEN) {
			/* DLC was previously opened by PN request */
			rfcomm_send_ua(s, dlci);

			rfcomm_dlc_lock(d);
			d->state = BT_CONNECTED;
			d->state_change(d, 0);
			rfcomm_dlc_unlock(d);

			rfcomm_send_msc(s, 1, dlci, d->v24_sig);
		}
		return 0;
	}

	/* Notify socket layer about incomming connection */
	channel = __srv_channel(dlci);
	if (rfcomm_connect_ind(s, channel, &d)) {
		d->dlci = dlci;
		d->addr = __addr(s->initiator, dlci);
		rfcomm_dlc_link(s, d);

		rfcomm_send_ua(s, dlci);

		rfcomm_dlc_lock(d);
		d->state = BT_CONNECTED;
		d->state_change(d, 0);
		rfcomm_dlc_unlock(d);

		rfcomm_send_msc(s, 1, dlci, d->v24_sig);
	} else {
		rfcomm_send_dm(s, dlci);
	}

	return 0;
}

static int rfcomm_apply_pn(struct rfcomm_dlc *d, int cr, struct rfcomm_pn *pn)
{
	struct rfcomm_session *s = d->session;

	BT_DBG("dlc %p state %ld dlci %d mtu %d fc 0x%x credits %d", 
			d, d->state, d->dlci, pn->mtu, pn->flow_ctrl, pn->credits);

	if (pn->flow_ctrl == 0xf0 || pn->flow_ctrl == 0xe0) {
		d->cfc = s->cfc = RFCOMM_CFC_ENABLED;
		d->tx_credits = pn->credits;
	} else {
		d->cfc = s->cfc = RFCOMM_CFC_DISABLED;
		set_bit(RFCOMM_TX_THROTTLED, &d->flags);
	}

	d->priority = pn->priority;

	d->mtu = s->mtu = btohs(pn->mtu);

	return 0;
}

static int rfcomm_recv_pn(struct rfcomm_session *s, int cr, struct sk_buff *skb)
{
	struct rfcomm_pn *pn = (void *) skb->data;
	struct rfcomm_dlc *d;
	u8 dlci = pn->dlci;

	BT_DBG("session %p state %ld dlci %d", s, s->state, dlci);

	if (!dlci)
		return 0;

	d = rfcomm_dlc_get(s, dlci);
	if (d) {
		if (cr) {
			/* PN request */
			rfcomm_apply_pn(d, cr, pn);
			rfcomm_send_pn(s, 0, d);
		} else {
			/* PN response */
			switch (d->state) {
			case BT_CONFIG:
				rfcomm_apply_pn(d, cr, pn);

				d->state = BT_CONNECT;
				rfcomm_send_sabm(s, d->dlci);
				break;
			}
		}
	} else {
		u8 channel = __srv_channel(dlci);

		if (!cr)
			return 0;

		/* PN request for non existing DLC.
		 * Assume incomming connection. */
		if (rfcomm_connect_ind(s, channel, &d)) {
			d->dlci = dlci;
			d->addr = __addr(s->initiator, dlci);
			rfcomm_dlc_link(s, d);

			rfcomm_apply_pn(d, cr, pn);

			d->state = BT_OPEN;
			rfcomm_send_pn(s, 0, d);
		} else {
			rfcomm_send_dm(s, dlci);
		}
	}
	return 0;
}

static int rfcomm_recv_rpn(struct rfcomm_session *s, int cr, int len, struct sk_buff *skb)
{
	struct rfcomm_rpn *rpn = (void *) skb->data;
	u8 dlci = __get_dlci(rpn->dlci);

	u8 bit_rate  = 0;
	u8 data_bits = 0;
	u8 stop_bits = 0;
	u8 parity    = 0;
	u8 flow_ctrl = 0;
	u8 xon_char  = 0;
	u8 xoff_char = 0;
	u16 rpn_mask = RFCOMM_RPN_PM_ALL;
	
	BT_DBG("dlci %d cr %d len 0x%x bitr 0x%x line 0x%x flow 0x%x xonc 0x%x xoffc 0x%x pm 0x%x", 
	       dlci, cr, len, rpn->bit_rate, rpn->line_settings, rpn->flow_ctrl,
	       rpn->xon_char, rpn->xoff_char, rpn->param_mask);
	
	if (!cr) 
		return 0;
	
	if (len == 1) {
		/* request: return default setting */
		bit_rate  = RFCOMM_RPN_BR_115200;
		data_bits = RFCOMM_RPN_DATA_8;
		stop_bits = RFCOMM_RPN_STOP_1;
		parity    = RFCOMM_RPN_PARITY_NONE;
		flow_ctrl = RFCOMM_RPN_FLOW_NONE;
		xon_char  = RFCOMM_RPN_XON_CHAR;
		xoff_char = RFCOMM_RPN_XOFF_CHAR;

		goto rpn_out;
	}
	/* check for sane values: ignore/accept bit_rate, 8 bits, 1 stop bit, no parity,
	                          no flow control lines, normal XON/XOFF chars */
	if (rpn->param_mask & RFCOMM_RPN_PM_BITRATE) {
		bit_rate = rpn->bit_rate;
		if (bit_rate != RFCOMM_RPN_BR_115200) {
			BT_DBG("RPN bit rate mismatch 0x%x", bit_rate);
			bit_rate = RFCOMM_RPN_BR_115200;
			rpn_mask ^= RFCOMM_RPN_PM_BITRATE;
		}
	}
	if (rpn->param_mask & RFCOMM_RPN_PM_DATA) {
		data_bits = __get_rpn_data_bits(rpn->line_settings);
		if (data_bits != RFCOMM_RPN_DATA_8) {
			BT_DBG("RPN data bits mismatch 0x%x", data_bits);
			data_bits = RFCOMM_RPN_DATA_8;
			rpn_mask ^= RFCOMM_RPN_PM_DATA;
		}
	}
	if (rpn->param_mask & RFCOMM_RPN_PM_STOP) {
		stop_bits = __get_rpn_stop_bits(rpn->line_settings);
		if (stop_bits != RFCOMM_RPN_STOP_1) {
			BT_DBG("RPN stop bits mismatch 0x%x", stop_bits);
			stop_bits = RFCOMM_RPN_STOP_1;
			rpn_mask ^= RFCOMM_RPN_PM_STOP;
		}
	}
	if (rpn->param_mask & RFCOMM_RPN_PM_PARITY) {
		parity = __get_rpn_parity(rpn->line_settings);
		if (parity != RFCOMM_RPN_PARITY_NONE) {
			BT_DBG("RPN parity mismatch 0x%x", parity);
			parity = RFCOMM_RPN_PARITY_NONE;
			rpn_mask ^= RFCOMM_RPN_PM_PARITY;
		}
	}
	if (rpn->param_mask & RFCOMM_RPN_PM_FLOW) {
		flow_ctrl = rpn->flow_ctrl;
		if (flow_ctrl != RFCOMM_RPN_FLOW_NONE) {
			BT_DBG("RPN flow ctrl mismatch 0x%x", flow_ctrl);
			flow_ctrl = RFCOMM_RPN_FLOW_NONE;
			rpn_mask ^= RFCOMM_RPN_PM_FLOW;
		}
	}
	if (rpn->param_mask & RFCOMM_RPN_PM_XON) {
		xon_char = rpn->xon_char;
		if (xon_char != RFCOMM_RPN_XON_CHAR) {
			BT_DBG("RPN XON char mismatch 0x%x", xon_char);
			xon_char = RFCOMM_RPN_XON_CHAR;
			rpn_mask ^= RFCOMM_RPN_PM_XON;
		}
	}
	if (rpn->param_mask & RFCOMM_RPN_PM_XOFF) {
		xoff_char = rpn->xoff_char;
		if (xoff_char != RFCOMM_RPN_XOFF_CHAR) {
			BT_DBG("RPN XOFF char mismatch 0x%x", xoff_char);
			xoff_char = RFCOMM_RPN_XOFF_CHAR;
			rpn_mask ^= RFCOMM_RPN_PM_XOFF;
		}
	}

rpn_out:
	rfcomm_send_rpn(s, 0, dlci, 
			bit_rate, data_bits, stop_bits, parity, flow_ctrl,
			xon_char, xoff_char, rpn_mask);

	return 0;
}

static int rfcomm_recv_rls(struct rfcomm_session *s, int cr, struct sk_buff *skb)
{
	struct rfcomm_rls *rls = (void *) skb->data;
	u8 dlci = __get_dlci(rls->dlci);

	BT_DBG("dlci %d cr %d status 0x%x", dlci, cr, rls->status);
	
	if (!cr)
		return 0;

	/* FIXME: We should probably do something with this
	   information here. But for now it's sufficient just
	   to reply -- Bluetooth 1.1 says it's mandatory to 
	   recognise and respond to RLS */

	rfcomm_send_rls(s, 0, dlci, rls->status);

	return 0;
}

static int rfcomm_recv_msc(struct rfcomm_session *s, int cr, struct sk_buff *skb)
{
	struct rfcomm_msc *msc = (void *) skb->data;
	struct rfcomm_dlc *d;
	u8 dlci = __get_dlci(msc->dlci);

	BT_DBG("dlci %d cr %d v24 0x%x", dlci, cr, msc->v24_sig);

	d = rfcomm_dlc_get(s, dlci);
	if (!d)
		return 0;

	if (cr) {
		if (msc->v24_sig & RFCOMM_V24_FC && !d->cfc)
			set_bit(RFCOMM_TX_THROTTLED, &d->flags);
		else
			clear_bit(RFCOMM_TX_THROTTLED, &d->flags);
		
		rfcomm_dlc_lock(d);
		if (d->modem_status)
			d->modem_status(d, msc->v24_sig);
		rfcomm_dlc_unlock(d);
		
		rfcomm_send_msc(s, 0, dlci, msc->v24_sig);

		d->mscex |= RFCOMM_MSCEX_RX;
	} else 
		d->mscex |= RFCOMM_MSCEX_TX;

	return 0;
}

static int rfcomm_recv_mcc(struct rfcomm_session *s, struct sk_buff *skb)
{
	struct rfcomm_mcc *mcc = (void *) skb->data;
	u8 type, cr, len;

	cr   = __test_cr(mcc->type);
	type = __get_mcc_type(mcc->type);
	len  = __get_mcc_len(mcc->len);

	BT_DBG("%p type 0x%x cr %d", s, type, cr);

	skb_pull(skb, 2);

	switch (type) {
	case RFCOMM_PN:
		rfcomm_recv_pn(s, cr, skb);
		break;

	case RFCOMM_RPN:
		rfcomm_recv_rpn(s, cr, len, skb);
		break;

	case RFCOMM_RLS:
		rfcomm_recv_rls(s, cr, skb);
		break;

	case RFCOMM_MSC:
		rfcomm_recv_msc(s, cr, skb);
		break;

	case RFCOMM_FCOFF:
		if (cr) {
			set_bit(RFCOMM_TX_THROTTLED, &s->flags);
			rfcomm_send_fcoff(s, 0);
		}
		break;

	case RFCOMM_FCON:
		if (cr) {
			clear_bit(RFCOMM_TX_THROTTLED, &s->flags);
			rfcomm_send_fcon(s, 0);
		}
		break;

	case RFCOMM_TEST:
		if (cr)
			rfcomm_send_test(s, 0, skb->data, skb->len);
		break;

	case RFCOMM_NSC:
		break;

	default:
		BT_ERR("Unknown control type 0x%02x", type);
		rfcomm_send_nsc(s, cr, type);
		break;
	}
	return 0;
}

static int rfcomm_recv_data(struct rfcomm_session *s, u8 dlci, int pf, struct sk_buff *skb)
{
	struct rfcomm_dlc *d;

	BT_DBG("session %p state %ld dlci %d pf %d", s, s->state, dlci, pf);

	d = rfcomm_dlc_get(s, dlci);
	if (!d) {
		rfcomm_send_dm(s, dlci);
		goto drop;
	}

	if (pf && d->cfc) {
		u8 credits = *(u8 *) skb->data; skb_pull(skb, 1);

		d->tx_credits += credits;
		if (d->tx_credits)
			clear_bit(RFCOMM_TX_THROTTLED, &d->flags);
	}

	if (skb->len && d->state == BT_CONNECTED) {
		rfcomm_dlc_lock(d);
		d->rx_credits--;
		d->data_ready(d, skb);
		rfcomm_dlc_unlock(d);
		return 0;
	}

drop:
	kfree_skb(skb);
	return 0;
}

static int rfcomm_recv_frame(struct rfcomm_session *s, struct sk_buff *skb)
{
	struct rfcomm_hdr *hdr = (void *) skb->data;
	u8 type, dlci, fcs;

	dlci = __get_dlci(hdr->addr);
	type = __get_type(hdr->ctrl);

	/* Trim FCS */
	skb->len--; skb->tail--;
	fcs = *(u8 *) skb->tail;
	
	if (__check_fcs(skb->data, type, fcs)) {
		BT_ERR("bad checksum in packet");
		kfree_skb(skb);
		return -EILSEQ;
	}

	if (__test_ea(hdr->len))
		skb_pull(skb, 3);
	else
		skb_pull(skb, 4);
	
	switch (type) {
	case RFCOMM_SABM:
		if (__test_pf(hdr->ctrl))
			rfcomm_recv_sabm(s, dlci);
		break;

	case RFCOMM_DISC:
		if (__test_pf(hdr->ctrl))
			rfcomm_recv_disc(s, dlci);
		break;

	case RFCOMM_UA:
		if (__test_pf(hdr->ctrl))
			rfcomm_recv_ua(s, dlci);
		break;

	case RFCOMM_DM:
		rfcomm_recv_dm(s, dlci);
		break;

	case RFCOMM_UIH:
		if (dlci)
			return rfcomm_recv_data(s, dlci, __test_pf(hdr->ctrl), skb);

		rfcomm_recv_mcc(s, skb);
		break;

	default:
		BT_ERR("Unknown packet type 0x%02x\n", type);
		break;
	}
	kfree_skb(skb);
	return 0;
}

/* ---- Connection and data processing ---- */

static void rfcomm_process_connect(struct rfcomm_session *s)
{
	struct rfcomm_dlc *d;
	struct list_head *p, *n;

	BT_DBG("session %p state %ld", s, s->state);

	list_for_each_safe(p, n, &s->dlcs) {
		d = list_entry(p, struct rfcomm_dlc, list);
		if (d->state == BT_CONFIG) {
			d->mtu = s->mtu;
			rfcomm_send_pn(s, 1, d);
		}
	}
}

/* Send data queued for the DLC.
 * Return number of frames left in the queue.
 */
static inline int rfcomm_process_tx(struct rfcomm_dlc *d)
{
	struct sk_buff *skb;
	int err;

	BT_DBG("dlc %p state %ld cfc %d rx_credits %d tx_credits %d", 
			d, d->state, d->cfc, d->rx_credits, d->tx_credits);

	/* Send pending MSC */
	if (test_and_clear_bit(RFCOMM_MSC_PENDING, &d->flags))
		rfcomm_send_msc(d->session, 1, d->dlci, d->v24_sig);
	
	if (d->cfc) {
		/* CFC enabled. 
		 * Give them some credits */
		if (!test_bit(RFCOMM_RX_THROTTLED, &d->flags) &&
			       	d->rx_credits <= (d->cfc >> 2)) {
			rfcomm_send_credits(d->session, d->addr, d->cfc - d->rx_credits);
			d->rx_credits = d->cfc;
		}
	} else {
		/* CFC disabled. 
		 * Give ourselves some credits */
		d->tx_credits = 5;
	}

	if (test_bit(RFCOMM_TX_THROTTLED, &d->flags))
		return skb_queue_len(&d->tx_queue);

	while (d->tx_credits && (skb = skb_dequeue(&d->tx_queue))) {
		err = rfcomm_send_frame(d->session, skb->data, skb->len);
		if (err < 0) {
			skb_queue_head(&d->tx_queue, skb);
			break;
		}
		kfree_skb(skb);
		d->tx_credits--;
	}

	if (d->cfc && !d->tx_credits) {
		/* We're out of TX credits.
		 * Set TX_THROTTLED flag to avoid unnesary wakeups by dlc_send. */
		set_bit(RFCOMM_TX_THROTTLED, &d->flags);
	}

	return skb_queue_len(&d->tx_queue);
}

static inline void rfcomm_process_dlcs(struct rfcomm_session *s)
{
	struct rfcomm_dlc *d;
	struct list_head *p, *n;

	BT_DBG("session %p state %ld", s, s->state);

	list_for_each_safe(p, n, &s->dlcs) {
		d = list_entry(p, struct rfcomm_dlc, list);
		if (test_bit(RFCOMM_TIMED_OUT, &d->flags)) {
			__rfcomm_dlc_close(d, ETIMEDOUT);
			continue;
		}

		if (test_bit(RFCOMM_TX_THROTTLED, &s->flags))
			continue;

		if ((d->state == BT_CONNECTED || d->state == BT_DISCONN) &&
				d->mscex == RFCOMM_MSCEX_OK)
			rfcomm_process_tx(d);
	}
}

static inline void rfcomm_process_rx(struct rfcomm_session *s)
{
	struct socket *sock = s->sock;
	struct sock *sk = sock->sk;
	struct sk_buff *skb;

	BT_DBG("session %p state %ld qlen %d", s, s->state, skb_queue_len(&sk->receive_queue));

	/* Get data directly from socket receive queue without copying it. */
	while ((skb = skb_dequeue(&sk->receive_queue))) {
		skb_orphan(skb);
		rfcomm_recv_frame(s, skb);
	}

	if (sk->state == BT_CLOSED) {
		if (!s->initiator)
			rfcomm_session_put(s);

		rfcomm_session_close(s, sk->err);
	}
}

static inline void rfcomm_accept_connection(struct rfcomm_session *s)
{
	struct socket *sock = s->sock, *nsock;
	int err;

	/* Fast check for a new connection.
	 * Avoids unnesesary socket allocations. */
	if (list_empty(&bluez_pi(sock->sk)->accept_q))
		return;

	BT_DBG("session %p", s);

	nsock = sock_alloc();
	if (!nsock)
		return;

	nsock->type = sock->type;
	nsock->ops  = sock->ops;
	
	err = sock->ops->accept(sock, nsock, O_NONBLOCK);
	if (err < 0) {
		sock_release(nsock);
		return;
	}

	/* Set our callbacks */
	nsock->sk->data_ready   = rfcomm_l2data_ready;
	nsock->sk->state_change = rfcomm_l2state_change;

	s = rfcomm_session_add(nsock, BT_OPEN);
	if (s) {
		rfcomm_session_hold(s);
		rfcomm_schedule(RFCOMM_SCHED_RX);
	} else
		sock_release(nsock);
}

static inline void rfcomm_check_connection(struct rfcomm_session *s)
{
	struct sock *sk = s->sock->sk;

	BT_DBG("%p state %ld", s, s->state);

	switch(sk->state) {
	case BT_CONNECTED:
		s->state = BT_CONNECT;

		/* We can adjust MTU on outgoing sessions.
		 * L2CAP MTU minus UIH header and FCS. */
		s->mtu = min(l2cap_pi(sk)->omtu, l2cap_pi(sk)->imtu) - 5;

		rfcomm_send_sabm(s, 0);
		break;

	case BT_CLOSED:
		s->state = BT_CLOSED;
		rfcomm_session_close(s, sk->err);
		break;
	}
}

static inline void rfcomm_process_sessions(void)
{
	struct list_head *p, *n;

	rfcomm_lock();

	list_for_each_safe(p, n, &session_list) {
		struct rfcomm_session *s;
		s = list_entry(p, struct rfcomm_session, list);

		if (s->state == BT_LISTEN) {
			rfcomm_accept_connection(s);
			continue;
		}

		rfcomm_session_hold(s);

		switch (s->state) {
		case BT_BOUND:
			rfcomm_check_connection(s);
			break;

		default:
			rfcomm_process_rx(s);
			break;
		}

		rfcomm_process_dlcs(s);

		rfcomm_session_put(s);
	}
	
	rfcomm_unlock();
}

static void rfcomm_worker(void)
{
	BT_DBG("");

	daemonize(); reparent_to_init();
	set_fs(KERNEL_DS);

	while (!atomic_read(&terminate)) {
		BT_DBG("worker loop event 0x%lx", rfcomm_event);

		if (!test_bit(RFCOMM_SCHED_WAKEUP, &rfcomm_event)) {
			/* No pending events. Let's sleep.
			 * Incomming connections and data will wake us up. */
			set_current_state(TASK_INTERRUPTIBLE);
			schedule();
		}

		/* Process stuff */
		clear_bit(RFCOMM_SCHED_WAKEUP, &rfcomm_event);
		rfcomm_process_sessions();
	}
	set_current_state(TASK_RUNNING);
	return;
}

static int rfcomm_add_listener(bdaddr_t *ba)
{
	struct sockaddr_l2 addr;
	struct l2cap_options opts;
	struct socket *sock;
	struct rfcomm_session *s;
	int    size, err = 0;

	/* Create socket */
	err = rfcomm_l2sock_create(&sock);
	if (err < 0) { 
		BT_ERR("Create socket failed %d", err);
		return err;
	}

	/* Bind socket */
	bacpy(&addr.l2_bdaddr, ba);
	addr.l2_family = AF_BLUETOOTH;
	addr.l2_psm    = htobs(RFCOMM_PSM);
	err = sock->ops->bind(sock, (struct sockaddr *) &addr, sizeof(addr));
	if (err < 0) {
		BT_ERR("Bind failed %d", err);
		goto failed;
	}

	/* Set L2CAP options */
	size = sizeof(opts);
	sock->ops->getsockopt(sock, SOL_L2CAP, L2CAP_OPTIONS, (void *)&opts, &size);

	opts.imtu = RFCOMM_MAX_L2CAP_MTU;
	sock->ops->setsockopt(sock, SOL_L2CAP, L2CAP_OPTIONS, (void *)&opts, size);

	/* Start listening on the socket */
	err = sock->ops->listen(sock, 10);
	if (err) {
		BT_ERR("Listen failed %d", err);
		goto failed;
	}

	/* Add listening session */
	s = rfcomm_session_add(sock, BT_LISTEN);
	if (!s)
		goto failed;

	rfcomm_session_hold(s);
	return 0;
failed:
	sock_release(sock);
	return err;
}

static void rfcomm_kill_listener(void)
{
	struct rfcomm_session *s;
	struct list_head *p, *n;

	BT_DBG("");

	list_for_each_safe(p, n, &session_list) {
		s = list_entry(p, struct rfcomm_session, list);
		rfcomm_session_del(s);
	}
}

static int rfcomm_run(void *unused)
{
	rfcomm_thread = current;

	atomic_inc(&running);

	daemonize(); reparent_to_init();

	sigfillset(&current->blocked);
	set_fs(KERNEL_DS);

	sprintf(current->comm, "krfcommd");

	BT_DBG("");

	rfcomm_add_listener(BDADDR_ANY);

	rfcomm_worker();

	rfcomm_kill_listener();

	atomic_dec(&running);
	return 0;
}

/* ---- Proc fs support ---- */
static int rfcomm_dlc_dump(char *buf)
{
	struct rfcomm_session *s;
	struct sock *sk;
	struct list_head *p, *pp;
	char *ptr = buf;

	rfcomm_lock();

	list_for_each(p, &session_list) {
		s = list_entry(p, struct rfcomm_session, list);
		sk = s->sock->sk;

		list_for_each(pp, &s->dlcs) {
		struct rfcomm_dlc *d;
			d = list_entry(pp, struct rfcomm_dlc, list);

			ptr += sprintf(ptr, "dlc %s %s %ld %d %d %d %d\n",
				batostr(&bluez_pi(sk)->src), batostr(&bluez_pi(sk)->dst),
				d->state, d->dlci, d->mtu, d->rx_credits, d->tx_credits);
		}
	}
	
	rfcomm_unlock();

	return ptr - buf;
}

extern int rfcomm_sock_dump(char *buf);

static int rfcomm_read_proc(char *buf, char **start, off_t offset, int count, int *eof, void *priv)
{
	char *ptr = buf;
	int len;

	BT_DBG("count %d, offset %ld", count, offset);

	ptr += rfcomm_dlc_dump(ptr);
	ptr += rfcomm_sock_dump(ptr);
	len  = ptr - buf;

	if (len <= count + offset)
		*eof = 1;

	*start = buf + offset;
	len -= offset;

	if (len > count)
		len = count;
	if (len < 0)
		len = 0;

	return len;
}

/* ---- Initialization ---- */
int __init rfcomm_init(void)
{
	l2cap_load();

	kernel_thread(rfcomm_run, NULL, CLONE_FS | CLONE_FILES | CLONE_SIGHAND);

	rfcomm_init_sockets();

#ifdef CONFIG_BLUEZ_RFCOMM_TTY
	rfcomm_init_ttys();
#endif

	create_proc_read_entry("bluetooth/rfcomm", 0, 0, rfcomm_read_proc, NULL);

	BT_INFO("BlueZ RFCOMM ver %s", VERSION);
	BT_INFO("Copyright (C) 2002 Maxim Krasnyansky <maxk@qualcomm.com>");
	BT_INFO("Copyright (C) 2002 Marcel Holtmann <marcel@holtmann.org>");
	return 0;
}

void rfcomm_cleanup(void)
{
	/* Terminate working thread.
	 * ie. Set terminate flag and wake it up */
	atomic_inc(&terminate);
	rfcomm_schedule(RFCOMM_SCHED_STATE);

	/* Wait until thread is running */
	while (atomic_read(&running))
		schedule();

	remove_proc_entry("bluetooth/rfcomm", NULL);

#ifdef CONFIG_BLUEZ_RFCOMM_TTY
	rfcomm_cleanup_ttys();
#endif

	rfcomm_cleanup_sockets();
	return;
}

module_init(rfcomm_init);
module_exit(rfcomm_cleanup);

MODULE_AUTHOR("Maxim Krasnyansky <maxk@qualcomm.com>, Marcel Holtmann <marcel@holtmann.org>");
MODULE_DESCRIPTION("BlueZ RFCOMM ver " VERSION);
MODULE_LICENSE("GPL");
