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
 * RFCOMM sockets.
 *
 * $Id: sock.c,v 1.30 2002/10/18 20:12:12 maxk Exp $
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
#include <linux/socket.h>
#include <linux/skbuff.h>
#include <linux/list.h>
#include <net/sock.h>

#include <asm/system.h>
#include <asm/uaccess.h>

#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/rfcomm.h>

#ifndef CONFIG_BLUEZ_RFCOMM_DEBUG
#undef  BT_DBG
#define BT_DBG(D...)
#endif

static struct proto_ops rfcomm_sock_ops;

static struct bluez_sock_list rfcomm_sk_list = {
	lock: RW_LOCK_UNLOCKED
};

static void rfcomm_sock_close(struct sock *sk);
static void rfcomm_sock_kill(struct sock *sk);

/* ---- DLC callbacks ----
 *
 * called under rfcomm_dlc_lock()
 */
static void rfcomm_sk_data_ready(struct rfcomm_dlc *d, struct sk_buff *skb)
{
	struct sock *sk = d->owner;
	if (!sk)
		return;

	atomic_add(skb->len, &sk->rmem_alloc);
	skb_queue_tail(&sk->receive_queue, skb);
	sk->data_ready(sk, skb->len);

	if (atomic_read(&sk->rmem_alloc) >= sk->rcvbuf)
		rfcomm_dlc_throttle(d);
}

static void rfcomm_sk_state_change(struct rfcomm_dlc *d, int err)
{
	struct sock *sk = d->owner, *parent;
	if (!sk)
		return;

	BT_DBG("dlc %p state %ld err %d", d, d->state, err);

	bh_lock_sock(sk);

	if (err)
		sk->err = err;
	sk->state = d->state;

	parent = bluez_pi(sk)->parent;
	if (!parent) {
		if (d->state == BT_CONNECTED)
			rfcomm_session_getaddr(d->session, &bluez_pi(sk)->src, NULL);
		sk->state_change(sk);
	} else
		parent->data_ready(parent, 0);

	bh_unlock_sock(sk);
}

/* ---- Socket functions ---- */
static struct sock *__rfcomm_get_sock_by_addr(u8 channel, bdaddr_t *src)
{
	struct sock *sk;

	for (sk = rfcomm_sk_list.head; sk; sk = sk->next) {
		if (rfcomm_pi(sk)->channel == channel && 
				!bacmp(&bluez_pi(sk)->src, src))
			break;
	}

	return sk;
}

/* Find socket with channel and source bdaddr.
 * Returns closest match.
 */
static struct sock *__rfcomm_get_sock_by_channel(int state, u8 channel, bdaddr_t *src)
{
	struct sock *sk, *sk1 = NULL;

	for (sk = rfcomm_sk_list.head; sk; sk = sk->next) {
		if (state && sk->state != state)
			continue;

		if (rfcomm_pi(sk)->channel == channel) {
			/* Exact match. */
			if (!bacmp(&bluez_pi(sk)->src, src))
				break;

			/* Closest match */
			if (!bacmp(&bluez_pi(sk)->src, BDADDR_ANY))
				sk1 = sk;
		}
	}
	return sk ? sk : sk1;
}

/* Find socket with given address (channel, src).
 * Returns locked socket */
static inline struct sock *rfcomm_get_sock_by_channel(int state, u8 channel, bdaddr_t *src)
{
	struct sock *s;
	read_lock(&rfcomm_sk_list.lock);
	s = __rfcomm_get_sock_by_channel(state, channel, src);
	if (s) bh_lock_sock(s);
	read_unlock(&rfcomm_sk_list.lock);
	return s;
}

static void rfcomm_sock_destruct(struct sock *sk)
{
	struct rfcomm_dlc *d = rfcomm_pi(sk)->dlc;

	BT_DBG("sk %p dlc %p", sk, d);

	skb_queue_purge(&sk->receive_queue);
	skb_queue_purge(&sk->write_queue);

	rfcomm_dlc_lock(d);
	rfcomm_pi(sk)->dlc = NULL;
	
	/* Detach DLC if it's owned by this socket */
	if (d->owner == sk)
		d->owner = NULL;
	rfcomm_dlc_unlock(d);

	rfcomm_dlc_put(d);

	MOD_DEC_USE_COUNT;
}

static void rfcomm_sock_cleanup_listen(struct sock *parent)
{
	struct sock *sk;

	BT_DBG("parent %p", parent);

	/* Close not yet accepted dlcs */
	while ((sk = bluez_accept_dequeue(parent, NULL))) {
		rfcomm_sock_close(sk);
		rfcomm_sock_kill(sk);
	}

	parent->state  = BT_CLOSED;
	parent->zapped = 1;
}

/* Kill socket (only if zapped and orphan)
 * Must be called on unlocked socket.
 */
static void rfcomm_sock_kill(struct sock *sk)
{
	if (!sk->zapped || sk->socket)
		return;

	BT_DBG("sk %p state %d refcnt %d", sk, sk->state, atomic_read(&sk->refcnt));

	/* Kill poor orphan */
	bluez_sock_unlink(&rfcomm_sk_list, sk);
	sk->dead = 1;
	sock_put(sk);
}

static void __rfcomm_sock_close(struct sock *sk)
{
	struct rfcomm_dlc *d = rfcomm_pi(sk)->dlc;

	BT_DBG("sk %p state %d socket %p", sk, sk->state, sk->socket);

	switch (sk->state) {
	case BT_LISTEN:
		rfcomm_sock_cleanup_listen(sk);
		break;

	case BT_CONNECT:
	case BT_CONNECT2:
	case BT_CONFIG:
	case BT_CONNECTED:
		rfcomm_dlc_close(d, 0);

	default:
		sk->zapped = 1;
		break;
	}
}

/* Close socket.
 * Must be called on unlocked socket.
 */
static void rfcomm_sock_close(struct sock *sk)
{
	lock_sock(sk);
	__rfcomm_sock_close(sk);
	release_sock(sk);
}

static void rfcomm_sock_init(struct sock *sk, struct sock *parent)
{
	BT_DBG("sk %p", sk);

	if (parent) 
		sk->type = parent->type;
}

static struct sock *rfcomm_sock_alloc(struct socket *sock, int proto, int prio)
{
	struct rfcomm_dlc *d;
	struct sock *sk;

	sk = sk_alloc(PF_BLUETOOTH, prio, 1);
	if (!sk)
		return NULL;

	d = rfcomm_dlc_alloc(prio);
	if (!d) {
		sk_free(sk);
		return NULL;
	}
	d->data_ready   = rfcomm_sk_data_ready;
	d->state_change = rfcomm_sk_state_change;

	rfcomm_pi(sk)->dlc = d;
	d->owner = sk;

	bluez_sock_init(sock, sk);

	sk->zapped   = 0;

	sk->destruct = rfcomm_sock_destruct;
	sk->sndtimeo = RFCOMM_CONN_TIMEOUT;

	sk->sndbuf   = RFCOMM_MAX_CREDITS * RFCOMM_DEFAULT_MTU * 10;
	sk->rcvbuf   = RFCOMM_MAX_CREDITS * RFCOMM_DEFAULT_MTU * 10;

	sk->protocol = proto;
	sk->state    = BT_OPEN;

	bluez_sock_link(&rfcomm_sk_list, sk);

	BT_DBG("sk %p", sk);

	MOD_INC_USE_COUNT;
	return sk;
}

static int rfcomm_sock_create(struct socket *sock, int protocol)
{
	struct sock *sk;

	BT_DBG("sock %p", sock);

	sock->state = SS_UNCONNECTED;

	if (sock->type != SOCK_STREAM && sock->type != SOCK_RAW)
		return -ESOCKTNOSUPPORT;

	sock->ops = &rfcomm_sock_ops;

	if (!(sk = rfcomm_sock_alloc(sock, protocol, GFP_KERNEL)))
		return -ENOMEM;

	rfcomm_sock_init(sk, NULL);
	return 0;
}

static int rfcomm_sock_bind(struct socket *sock, struct sockaddr *addr, int addr_len)
{
	struct sockaddr_rc *sa = (struct sockaddr_rc *) addr;
	struct sock *sk = sock->sk;
	int err = 0;

	BT_DBG("sk %p %s", sk, batostr(&sa->rc_bdaddr));

	if (!addr || addr->sa_family != AF_BLUETOOTH)
		return -EINVAL;

	lock_sock(sk);

	if (sk->state != BT_OPEN) {
		err = -EBADFD;
		goto done;
	}

	write_lock_bh(&rfcomm_sk_list.lock);

	if (sa->rc_channel && __rfcomm_get_sock_by_addr(sa->rc_channel, &sa->rc_bdaddr)) {
		err = -EADDRINUSE;
	} else {
		/* Save source address */
		bacpy(&bluez_pi(sk)->src, &sa->rc_bdaddr);
		rfcomm_pi(sk)->channel = sa->rc_channel;
		sk->state = BT_BOUND;
	}

	write_unlock_bh(&rfcomm_sk_list.lock);

done:
	release_sock(sk);
	return err;
}

static int rfcomm_sock_connect(struct socket *sock, struct sockaddr *addr, int alen, int flags)
{
	struct sockaddr_rc *sa = (struct sockaddr_rc *) addr;
	struct sock *sk = sock->sk;
	struct rfcomm_dlc *d = rfcomm_pi(sk)->dlc;
	int err = 0;

	BT_DBG("sk %p", sk);

	if (addr->sa_family != AF_BLUETOOTH || alen < sizeof(struct sockaddr_rc))
		return -EINVAL;

	if (sk->state != BT_OPEN && sk->state != BT_BOUND)
		return -EBADFD;

	if (sk->type != SOCK_STREAM)
		return -EINVAL;

	lock_sock(sk);

	sk->state = BT_CONNECT;
	bacpy(&bluez_pi(sk)->dst, &sa->rc_bdaddr);
	rfcomm_pi(sk)->channel = sa->rc_channel;
	
	err = rfcomm_dlc_open(d, &bluez_pi(sk)->src, &sa->rc_bdaddr, sa->rc_channel);
	if (!err)
		err = bluez_sock_wait_state(sk, BT_CONNECTED,
				sock_sndtimeo(sk, flags & O_NONBLOCK));

	release_sock(sk);
	return err;
}

int rfcomm_sock_listen(struct socket *sock, int backlog)
{
	struct sock *sk = sock->sk;
	int err = 0;

	BT_DBG("sk %p backlog %d", sk, backlog);

	lock_sock(sk);

	if (sk->state != BT_BOUND) {
		err = -EBADFD;
		goto done;
	}

	sk->max_ack_backlog = backlog;
	sk->ack_backlog = 0;
	sk->state = BT_LISTEN;

done:
	release_sock(sk);
	return err;
}

int rfcomm_sock_accept(struct socket *sock, struct socket *newsock, int flags)
{
	DECLARE_WAITQUEUE(wait, current);
	struct sock *sk = sock->sk, *nsk;
	long timeo;
	int err = 0;

	lock_sock(sk);

	if (sk->state != BT_LISTEN) {
		err = -EBADFD;
		goto done;
	}

	timeo = sock_rcvtimeo(sk, flags & O_NONBLOCK);

	BT_DBG("sk %p timeo %ld", sk, timeo);

	/* Wait for an incoming connection. (wake-one). */
	add_wait_queue_exclusive(sk->sleep, &wait);
	while (!(nsk = bluez_accept_dequeue(sk, newsock))) {
		set_current_state(TASK_INTERRUPTIBLE);
		if (!timeo) {
			err = -EAGAIN;
			break;
		}

		release_sock(sk);
		timeo = schedule_timeout(timeo);
		lock_sock(sk);

		if (sk->state != BT_LISTEN) {
			err = -EBADFD;
			break;
		}

		if (signal_pending(current)) {
			err = sock_intr_errno(timeo);
			break;
		}
	}
	set_current_state(TASK_RUNNING);
	remove_wait_queue(sk->sleep, &wait);

	if (err)
		goto done;

	newsock->state = SS_CONNECTED;

	BT_DBG("new socket %p", nsk);

done:
	release_sock(sk);
	return err;
}

static int rfcomm_sock_getname(struct socket *sock, struct sockaddr *addr, int *len, int peer)
{
	struct sockaddr_rc *sa = (struct sockaddr_rc *) addr;
	struct sock *sk = sock->sk;

	BT_DBG("sock %p, sk %p", sock, sk);

	sa->rc_family  = AF_BLUETOOTH;
	sa->rc_channel = rfcomm_pi(sk)->channel;
	if (peer)
		bacpy(&sa->rc_bdaddr, &bluez_pi(sk)->dst);
	else
		bacpy(&sa->rc_bdaddr, &bluez_pi(sk)->src);

	*len = sizeof(struct sockaddr_rc);
	return 0;
}

static int rfcomm_sock_sendmsg(struct socket *sock, struct msghdr *msg, int len,
			       struct scm_cookie *scm)
{
	struct sock *sk = sock->sk;
	struct rfcomm_dlc *d = rfcomm_pi(sk)->dlc;
	struct sk_buff *skb;
	int err, size;
	int sent = 0;

	if (msg->msg_flags & MSG_OOB)
		return -EOPNOTSUPP;

	if (sk->shutdown & SEND_SHUTDOWN)
		return -EPIPE;

	BT_DBG("sock %p, sk %p", sock, sk);

	lock_sock(sk);

	while (len) {
		size = min_t(uint, len, d->mtu);
		
		skb = sock_alloc_send_skb(sk, size + RFCOMM_SKB_RESERVE,
				msg->msg_flags & MSG_DONTWAIT, &err);
		if (!skb)
			break;
		skb_reserve(skb, RFCOMM_SKB_HEAD_RESERVE);

		err = memcpy_fromiovec(skb_put(skb, size), msg->msg_iov, size);
		if (err) {
			kfree_skb(skb);
			sent = err;
			break;
		}

		err = rfcomm_dlc_send(d, skb);
		if (err < 0) {
			kfree_skb(skb);
			break;
		}

		sent += size;
		len  -= size;
	}

	release_sock(sk);

	return sent ? sent : err;
}

static long rfcomm_sock_data_wait(struct sock *sk, long timeo)
{
	DECLARE_WAITQUEUE(wait, current);

	add_wait_queue(sk->sleep, &wait);
	for (;;) {
		set_current_state(TASK_INTERRUPTIBLE);

		if (skb_queue_len(&sk->receive_queue) || sk->err || (sk->shutdown & RCV_SHUTDOWN) ||
				signal_pending(current) || !timeo)
			break;

		set_bit(SOCK_ASYNC_WAITDATA, &sk->socket->flags);
		release_sock(sk);
		timeo = schedule_timeout(timeo);
		lock_sock(sk);
		clear_bit(SOCK_ASYNC_WAITDATA, &sk->socket->flags);
	}

	__set_current_state(TASK_RUNNING);
	remove_wait_queue(sk->sleep, &wait);
	return timeo;
}

static int rfcomm_sock_recvmsg(struct socket *sock, struct msghdr *msg, int size,
			       int flags, struct scm_cookie *scm)
{
	struct sock *sk = sock->sk;
	int target, err = 0, copied = 0;
	long timeo;

	if (flags & MSG_OOB)
		return -EOPNOTSUPP;

	msg->msg_namelen = 0;

	BT_DBG("sk %p size %d", sk, size);

	lock_sock(sk);

	target = sock_rcvlowat(sk, flags & MSG_WAITALL, size);
	timeo  = sock_rcvtimeo(sk, flags & MSG_DONTWAIT);

	do {
		struct sk_buff *skb;
		int chunk;

		skb = skb_dequeue(&sk->receive_queue);
		if (!skb) {
			if (copied >= target)
				break;

			if ((err = sock_error(sk)) != 0)
				break;
			if (sk->shutdown & RCV_SHUTDOWN)
				break;

			err = -EAGAIN;
			if (!timeo)
				break;

			timeo = rfcomm_sock_data_wait(sk, timeo);

			if (signal_pending(current)) {
				err = sock_intr_errno(timeo);
				goto out;
			}
			continue;
		}

		chunk = min_t(unsigned int, skb->len, size);
		if (memcpy_toiovec(msg->msg_iov, skb->data, chunk)) {
			skb_queue_head(&sk->receive_queue, skb);
			if (!copied)
				copied = -EFAULT;
			break;
		}
		copied += chunk;
		size   -= chunk;

		if (!(flags & MSG_PEEK)) {
			atomic_sub(chunk, &sk->rmem_alloc);

			skb_pull(skb, chunk);
			if (skb->len) {
				skb_queue_head(&sk->receive_queue, skb);
				break;
			}
			kfree_skb(skb);

		} else {
			/* put message back and return */
			skb_queue_head(&sk->receive_queue, skb);
			break;
		}
	} while (size);

out:
	if (atomic_read(&sk->rmem_alloc) <= (sk->rcvbuf >> 2))
		rfcomm_dlc_unthrottle(rfcomm_pi(sk)->dlc);

	release_sock(sk);
	return copied ? : err;
}

static int rfcomm_sock_setsockopt(struct socket *sock, int level, int optname, char *optval, int optlen)
{
	struct sock *sk = sock->sk;
	int err = 0;

	BT_DBG("sk %p", sk);

	lock_sock(sk);

	switch (optname) {
	default:
		err = -ENOPROTOOPT;
		break;
	};

	release_sock(sk);
	return err;
}

static int rfcomm_sock_getsockopt(struct socket *sock, int level, int optname, char *optval, int *optlen)
{
	struct sock *sk = sock->sk;
	int len, err = 0; 

	BT_DBG("sk %p", sk);

	if (get_user(len, optlen))
		return -EFAULT;

	lock_sock(sk);

	switch (optname) {
	default:
		err = -ENOPROTOOPT;
		break;
	};

	release_sock(sk);
	return err;
}

static int rfcomm_sock_ioctl(struct socket *sock, unsigned int cmd, unsigned long arg)
{
	struct sock *sk = sock->sk;
	int err;

	lock_sock(sk);

#ifdef CONFIG_BLUEZ_RFCOMM_TTY
	err = rfcomm_dev_ioctl(sk, cmd, arg);
#else
	err = -EOPNOTSUPP;
#endif

	release_sock(sk);

	return err;
}

static int rfcomm_sock_shutdown(struct socket *sock, int how)
{
	struct sock *sk = sock->sk;
	int err = 0;

	BT_DBG("sock %p, sk %p", sock, sk);

	if (!sk) return 0;

	lock_sock(sk);
	if (!sk->shutdown) {
		sk->shutdown = SHUTDOWN_MASK;
		__rfcomm_sock_close(sk);

		if (sk->linger)
			err = bluez_sock_wait_state(sk, BT_CLOSED, sk->lingertime);
	}
	release_sock(sk);
	return err;
}

static int rfcomm_sock_release(struct socket *sock)
{
	struct sock *sk = sock->sk;
	int err = 0;

	BT_DBG("sock %p, sk %p", sock, sk);

	if (!sk)
		return 0;

	err = rfcomm_sock_shutdown(sock, 2);

	sock_orphan(sk);
	rfcomm_sock_kill(sk);
	return err;
}

/* ---- RFCOMM core layer callbacks ---- 
 *
 * called under rfcomm_lock()
 */
int rfcomm_connect_ind(struct rfcomm_session *s, u8 channel, struct rfcomm_dlc **d)
{
	struct sock *sk, *parent;
	bdaddr_t src, dst;
	int result = 0;

	BT_DBG("session %p channel %d", s, channel);

	rfcomm_session_getaddr(s, &src, &dst);

	/* Check if we have socket listening on this channel */
	parent = rfcomm_get_sock_by_channel(BT_LISTEN, channel, &src);
	if (!parent)
		return 0;

	/* Check for backlog size */
	if (parent->ack_backlog > parent->max_ack_backlog) {
		BT_DBG("backlog full %d", parent->ack_backlog); 
		goto done;
	}

	sk = rfcomm_sock_alloc(NULL, BTPROTO_RFCOMM, GFP_ATOMIC);
	if (!sk)
		goto done;

	rfcomm_sock_init(sk, parent);
	bacpy(&bluez_pi(sk)->src, &src);
	bacpy(&bluez_pi(sk)->dst, &dst);
	rfcomm_pi(sk)->channel = channel;

	sk->state = BT_CONFIG;
	bluez_accept_enqueue(parent, sk);

	/* Accept connection and return socket DLC */
	*d = rfcomm_pi(sk)->dlc;
	result = 1;

done:
	bh_unlock_sock(parent);
	return result;
}

/* ---- Proc fs support ---- */
int rfcomm_sock_dump(char *buf)
{
	struct bluez_sock_list *list = &rfcomm_sk_list;
	struct rfcomm_pinfo *pi;
	struct sock *sk;
	char *ptr = buf;

	write_lock_bh(&list->lock);

	for (sk = list->head; sk; sk = sk->next) {
		pi = rfcomm_pi(sk);
		ptr += sprintf(ptr, "sk  %s %s %d %d\n",
				batostr(&bluez_pi(sk)->src), batostr(&bluez_pi(sk)->dst),
				sk->state, rfcomm_pi(sk)->channel);
	}

	write_unlock_bh(&list->lock);

	return ptr - buf;
}

static struct proto_ops rfcomm_sock_ops = {
	family:		PF_BLUETOOTH,
	release:	rfcomm_sock_release,
	bind:		rfcomm_sock_bind,
	connect:	rfcomm_sock_connect,
	listen:		rfcomm_sock_listen,
	accept:		rfcomm_sock_accept,
	getname:	rfcomm_sock_getname,
	sendmsg:	rfcomm_sock_sendmsg,
	recvmsg:	rfcomm_sock_recvmsg,
	shutdown:	rfcomm_sock_shutdown,
	setsockopt:	rfcomm_sock_setsockopt,
	getsockopt:	rfcomm_sock_getsockopt,
	ioctl:		rfcomm_sock_ioctl,
	poll:		bluez_sock_poll,
	socketpair:	sock_no_socketpair,
	mmap:		sock_no_mmap
};

static struct net_proto_family rfcomm_sock_family_ops = {
	family:		PF_BLUETOOTH,
	create:		rfcomm_sock_create
};

int rfcomm_init_sockets(void)
{
	int err;

	if ((err = bluez_sock_register(BTPROTO_RFCOMM, &rfcomm_sock_family_ops))) {
		BT_ERR("Can't register RFCOMM socket layer");
		return err;
	}

	return 0;
}

void rfcomm_cleanup_sockets(void)
{
	int err;

	/* Unregister socket, protocol and notifier */
	if ((err = bluez_sock_unregister(BTPROTO_RFCOMM)))
		BT_ERR("Can't unregister RFCOMM socket layer %d", err);
}
