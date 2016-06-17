/*
 *	X.25 Packet Layer release 002
 *
 *	This is ALPHA test software. This code may break your machine, randomly fail to work with new 
 *	releases, misbehave and/or generally screw up. It might even work. 
 *
 *	This code REQUIRES 2.1.15 or higher
 *
 *	This module:
 *		This module is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 *	History
 *	X.25 001	Jonathan Naylor	Started coding.
 *	X.25 002	Jonathan Naylor	Centralised disconnect handling.
 *					New timer architecture.
 *	2000-03-11	Henner Eisen	MSG_EOR handling more POSIX compliant.
 *	2000-03-22	Daniela Squassoni Allowed disabling/enabling of 
 *					  facilities negotiation and increased 
 *					  the throughput upper limit.
 *	2000-08-27	Arnaldo C. Melo s/suser/capable/ + micro cleanups
 *	2000-09-04	Henner Eisen	Set sock->state in x25_accept(). 
 *					Fixed x25_output() related skb leakage.
 *	2000-10-02	Henner Eisen	Made x25_kick() single threaded per socket.
 *	2000-10-27	Henner Eisen    MSG_DONTWAIT for fragment allocation.
 *	2000-11-14	Henner Eisen    Closing datalink from NETDEV_GOING_DOWN
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/socket.h>
#include <linux/in.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/string.h>
#include <linux/sockios.h>
#include <linux/net.h>
#include <linux/stat.h>
#include <linux/inet.h>
#include <linux/netdevice.h>
#include <linux/if_arp.h>
#include <linux/skbuff.h>
#include <net/sock.h>
#include <asm/segment.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <linux/fcntl.h>
#include <linux/termios.h>	/* For TIOCINQ/OUTQ */
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/notifier.h>
#include <linux/proc_fs.h>
#include <linux/init.h>
#include <net/x25.h>

int sysctl_x25_restart_request_timeout = X25_DEFAULT_T20;
int sysctl_x25_call_request_timeout    = X25_DEFAULT_T21;
int sysctl_x25_reset_request_timeout   = X25_DEFAULT_T22;
int sysctl_x25_clear_request_timeout   = X25_DEFAULT_T23;
int sysctl_x25_ack_holdback_timeout    = X25_DEFAULT_T2;

static struct sock *volatile x25_list /* = NULL initially */;

static struct proto_ops x25_proto_ops;

static x25_address null_x25_address = {"               "};

int x25_addr_ntoa(unsigned char *p, x25_address *called_addr, x25_address *calling_addr)
{
	int called_len, calling_len;
	char *called, *calling;
	int i;

	called_len  = (*p >> 0) & 0x0F;
	calling_len = (*p >> 4) & 0x0F;

	called  = called_addr->x25_addr;
	calling = calling_addr->x25_addr;
	p++;

	for (i = 0; i < (called_len + calling_len); i++) {
		if (i < called_len) {
			if (i % 2 != 0) {
				*called++ = ((*p >> 0) & 0x0F) + '0';
				p++;
			} else {
				*called++ = ((*p >> 4) & 0x0F) + '0';
			}
		} else {
			if (i % 2 != 0) {
				*calling++ = ((*p >> 0) & 0x0F) + '0';
				p++;
			} else {
				*calling++ = ((*p >> 4) & 0x0F) + '0';
			}
		}
	}

	*called  = '\0';
	*calling = '\0';

	return 1 + (called_len + calling_len + 1) / 2;
}

int x25_addr_aton(unsigned char *p, x25_address *called_addr, x25_address *calling_addr)
{
	unsigned int called_len, calling_len;
	char *called, *calling;
	int i;

	called  = called_addr->x25_addr;
	calling = calling_addr->x25_addr;

	called_len  = strlen(called);
	calling_len = strlen(calling);

	*p++ = (calling_len << 4) | (called_len << 0);

	for (i = 0; i < (called_len + calling_len); i++) {
		if (i < called_len) {
			if (i % 2 != 0) {
				*p |= (*called++ - '0') << 0;
				p++;
			} else {
				*p = 0x00;
				*p |= (*called++ - '0') << 4;
			}
		} else {
			if (i % 2 != 0) {
				*p |= (*calling++ - '0') << 0;
				p++;
			} else {
				*p = 0x00;
				*p |= (*calling++ - '0') << 4;
			}
		}
	}

	return 1 + (called_len + calling_len + 1) / 2;
}

/*
 *	Socket removal during an interrupt is now safe.
 */
static void x25_remove_socket(struct sock *sk)
{
	struct sock *s;
	unsigned long flags;

	save_flags(flags);
	cli();

	if ((s = x25_list) == sk) {
		x25_list = s->next;
		restore_flags(flags);
		return;
	}

	while (s != NULL && s->next != NULL) {
		if (s->next == sk) {
			s->next = sk->next;
			restore_flags(flags);
			return;
		}

		s = s->next;
	}

	restore_flags(flags);
}

/*
 *	Kill all bound sockets on a dropped device.
 */
static void x25_kill_by_device(struct net_device *dev)
{
	struct sock *s;

	for (s = x25_list; s != NULL; s = s->next)
		if (s->protinfo.x25->neighbour &&
		    s->protinfo.x25->neighbour->dev == dev)
			x25_disconnect(s, ENETUNREACH, 0, 0);
}

/*
 *	Handle device status changes.
 */
static int x25_device_event(struct notifier_block *this, unsigned long event, void *ptr)
{
	struct net_device *dev = (struct net_device *)ptr;
	struct x25_neigh *neigh;

	if (dev->type == ARPHRD_X25
#if defined(CONFIG_LLC) || defined(CONFIG_LLC_MODULE)
	 || dev->type == ARPHRD_ETHER
#endif
	 ) {
		switch (event) {
			case NETDEV_UP:
				x25_link_device_up(dev);
				break;
			case NETDEV_GOING_DOWN:
				if ((neigh = x25_get_neigh(dev)))
					x25_terminate_link(neigh);
				break;
			case NETDEV_DOWN:
				x25_kill_by_device(dev);
				x25_route_device_down(dev);
				x25_link_device_down(dev);
				break;
		}
	}

	return NOTIFY_DONE;
}

/*
 *	Add a socket to the bound sockets list.
 */
static void x25_insert_socket(struct sock *sk)
{
	unsigned long flags;

	save_flags(flags);
	cli();

	sk->next = x25_list;
	x25_list = sk;

	restore_flags(flags);
}

/*
 *	Find a socket that wants to accept the Call Request we just
 *	received.
 */
static struct sock *x25_find_listener(x25_address *addr)
{
	unsigned long flags;
	struct sock *s;

	save_flags(flags);
	cli();

	for (s = x25_list; s != NULL; s = s->next) {
		if ((strcmp(addr->x25_addr, s->protinfo.x25->source_addr.x25_addr) == 0 ||
		     strcmp(addr->x25_addr, null_x25_address.x25_addr) == 0) &&
		     s->state == TCP_LISTEN) {
			restore_flags(flags);
			return s;
		}
	}

	restore_flags(flags);
	return NULL;
}

/*
 *	Find a connected X.25 socket given my LCI and neighbour.
 */
struct sock *x25_find_socket(unsigned int lci, struct x25_neigh *neigh)
{
	struct sock *s;
	unsigned long flags;

	save_flags(flags);
	cli();

	for (s = x25_list; s != NULL; s = s->next) {
		if (s->protinfo.x25->lci == lci && s->protinfo.x25->neighbour == neigh) {
			restore_flags(flags);
			return s;
		}
	}

	restore_flags(flags);
	return NULL;
}

/*
 *	Find a unique LCI for a given device.
 */
unsigned int x25_new_lci(struct x25_neigh *neigh)
{
	unsigned int lci = 1;

	while (x25_find_socket(lci, neigh) != NULL) {
		lci++;
		if (lci == 4096) return 0;
	}

	return lci;
}

/*
 *	Deferred destroy.
 */
void x25_destroy_socket(struct sock *);

/*
 *	handler for deferred kills.
 */
static void x25_destroy_timer(unsigned long data)
{
	x25_destroy_socket((struct sock *)data);
}

/*
 *	This is called from user mode and the timers. Thus it protects itself against
 *	interrupt users but doesn't worry about being called during work.
 *	Once it is removed from the queue no interrupt or bottom half will
 *	touch it and we are (fairly 8-) ) safe.
 */
void x25_destroy_socket(struct sock *sk)	/* Not static as it's used by the timer */
{
	struct sk_buff *skb;
	unsigned long flags;

	save_flags(flags);
	cli();

	x25_stop_heartbeat(sk);
	x25_stop_timer(sk);

	x25_remove_socket(sk);
	x25_clear_queues(sk);		/* Flush the queues */

	while ((skb = skb_dequeue(&sk->receive_queue)) != NULL) {
		if (skb->sk != sk) {		/* A pending connection */
			skb->sk->dead = 1;	/* Queue the unaccepted socket for death */
			x25_start_heartbeat(skb->sk);
			skb->sk->protinfo.x25->state = X25_STATE_0;
		}

		kfree_skb(skb);
	}

	if (atomic_read(&sk->wmem_alloc) != 0 || atomic_read(&sk->rmem_alloc) != 0) {
		/* Defer: outstanding buffers */
		init_timer(&sk->timer);
		sk->timer.expires  = jiffies + 10 * HZ;
		sk->timer.function = x25_destroy_timer;
		sk->timer.data     = (unsigned long)sk;
		add_timer(&sk->timer);
	} else {
		sk_free(sk);
		MOD_DEC_USE_COUNT;
	}

	restore_flags(flags);
}

/*
 *	Handling for system calls applied via the various interfaces to a
 *	X.25 socket object.
 */

static int x25_setsockopt(struct socket *sock, int level, int optname,
	char *optval, int optlen)
{
	struct sock *sk = sock->sk;
	int opt;

	if (level != SOL_X25)
		return -ENOPROTOOPT;

	if (optlen < sizeof(int))
		return-EINVAL;

	if (get_user(opt, (int *)optval))
		return -EFAULT;

	switch (optname) {
		case X25_QBITINCL:
			sk->protinfo.x25->qbitincl = opt ? 1 : 0;
			return 0;

		default:
			return -ENOPROTOOPT;
	}
}

static int x25_getsockopt(struct socket *sock, int level, int optname,
	char *optval, int *optlen)
{
	struct sock *sk = sock->sk;
	int val = 0;
	int len; 
	
	if (level != SOL_X25)
		return -ENOPROTOOPT;

	if (get_user(len, optlen))
		return -EFAULT;

	switch (optname) {
		case X25_QBITINCL:
			val = sk->protinfo.x25->qbitincl;
			break;

		default:
			return -ENOPROTOOPT;
	}

	len = min_t(unsigned int, len, sizeof(int));

	if (len < 0)
		return -EINVAL;
		
	if (put_user(len, optlen))
		return -EFAULT;

	return copy_to_user(optval, &val, len) ? -EFAULT : 0;
}

static int x25_listen(struct socket *sock, int backlog)
{
	struct sock *sk = sock->sk;

	if (sk->state != TCP_LISTEN) {
		memset(&sk->protinfo.x25->dest_addr, '\0', X25_ADDR_LEN);
		sk->max_ack_backlog = backlog;
		sk->state           = TCP_LISTEN;
		return 0;
	}

	return -EOPNOTSUPP;
}

static struct sock *x25_alloc_socket(void)
{
	struct sock *sk;
	x25_cb *x25;

	if ((sk = sk_alloc(AF_X25, GFP_ATOMIC, 1)) == NULL)
		return NULL;

	if ((x25 = kmalloc(sizeof(*x25), GFP_ATOMIC)) == NULL) {
		sk_free(sk);
		return NULL;
	}

	memset(x25, 0x00, sizeof(*x25));

	x25->sk          = sk;
	sk->protinfo.x25 = x25;

	MOD_INC_USE_COUNT;

	sock_init_data(NULL, sk);

	skb_queue_head_init(&x25->ack_queue);
	skb_queue_head_init(&x25->fragment_queue);
	skb_queue_head_init(&x25->interrupt_in_queue);
	skb_queue_head_init(&x25->interrupt_out_queue);

	return sk;
}

static int x25_create(struct socket *sock, int protocol)
{
	struct sock *sk;
	x25_cb *x25;

	if (sock->type != SOCK_SEQPACKET || protocol != 0)
		return -ESOCKTNOSUPPORT;

	if ((sk = x25_alloc_socket()) == NULL)
		return -ENOMEM;

	x25 = sk->protinfo.x25;

	sock_init_data(sock, sk);

	init_timer(&x25->timer);

	sock->ops    = &x25_proto_ops;
	sk->protocol = protocol;
	sk->backlog_rcv = x25_backlog_rcv;

	x25->t21   = sysctl_x25_call_request_timeout;
	x25->t22   = sysctl_x25_reset_request_timeout;
	x25->t23   = sysctl_x25_clear_request_timeout;
	x25->t2    = sysctl_x25_ack_holdback_timeout;
	x25->state = X25_STATE_0;

	x25->facilities.winsize_in  = X25_DEFAULT_WINDOW_SIZE;
	x25->facilities.winsize_out = X25_DEFAULT_WINDOW_SIZE;
	x25->facilities.pacsize_in  = X25_DEFAULT_PACKET_SIZE;
	x25->facilities.pacsize_out = X25_DEFAULT_PACKET_SIZE;
	x25->facilities.throughput  = X25_DEFAULT_THROUGHPUT;
	x25->facilities.reverse     = X25_DEFAULT_REVERSE;

	return 0;
}

static struct sock *x25_make_new(struct sock *osk)
{
	struct sock *sk;
	x25_cb *x25;

	if (osk->type != SOCK_SEQPACKET)
		return NULL;

	if ((sk = x25_alloc_socket()) == NULL)
		return NULL;

	x25 = sk->protinfo.x25;

	sk->type        = osk->type;
	sk->socket      = osk->socket;
	sk->priority    = osk->priority;
	sk->protocol    = osk->protocol;
	sk->rcvbuf      = osk->rcvbuf;
	sk->sndbuf      = osk->sndbuf;
	sk->debug       = osk->debug;
	sk->state       = TCP_ESTABLISHED;
	sk->sleep       = osk->sleep;
	sk->zapped      = osk->zapped;
	sk->backlog_rcv = osk->backlog_rcv;

	x25->t21        = osk->protinfo.x25->t21;
	x25->t22        = osk->protinfo.x25->t22;
	x25->t23        = osk->protinfo.x25->t23;
	x25->t2         = osk->protinfo.x25->t2;

	x25->facilities = osk->protinfo.x25->facilities;

	x25->qbitincl   = osk->protinfo.x25->qbitincl;

	init_timer(&x25->timer);

	return sk;
}

static int x25_release(struct socket *sock)
{
	struct sock *sk = sock->sk;

	if (sk == NULL) return 0;

	switch (sk->protinfo.x25->state) {

		case X25_STATE_0:
		case X25_STATE_2:
			x25_disconnect(sk, 0, 0, 0);
			x25_destroy_socket(sk);
			break;

		case X25_STATE_1:
		case X25_STATE_3:
		case X25_STATE_4:
			x25_clear_queues(sk);
			x25_write_internal(sk, X25_CLEAR_REQUEST);
			x25_start_t23timer(sk);
			sk->protinfo.x25->state = X25_STATE_2;
			sk->state               = TCP_CLOSE;
			sk->shutdown           |= SEND_SHUTDOWN;
			sk->state_change(sk);
			sk->dead                = 1;
			sk->destroy             = 1;
			break;

		default:
			break;
	}

	sock->sk   = NULL;	
	sk->socket = NULL;	/* Not used, but we should do this */

	return 0;
}

static int x25_bind(struct socket *sock, struct sockaddr *uaddr, int addr_len)
{
	struct sock *sk = sock->sk;
	struct sockaddr_x25 *addr = (struct sockaddr_x25 *)uaddr;

	if (sk->zapped == 0)
		return -EINVAL;

	if (addr_len != sizeof(struct sockaddr_x25))
		return -EINVAL;

	if (addr->sx25_family != AF_X25)
		return -EINVAL;

	sk->protinfo.x25->source_addr = addr->sx25_addr;

	x25_insert_socket(sk);

	sk->zapped = 0;

	SOCK_DEBUG(sk, "x25_bind: socket is bound\n");

	return 0;
}

static int x25_connect(struct socket *sock, struct sockaddr *uaddr, int addr_len, int flags)
{
	struct sock *sk = sock->sk;
	struct sockaddr_x25 *addr = (struct sockaddr_x25 *)uaddr;
	struct net_device *dev;

	if (sk->state == TCP_ESTABLISHED && sock->state == SS_CONNECTING) {
		sock->state = SS_CONNECTED;
		return 0;	/* Connect completed during a ERESTARTSYS event */
	}

	if (sk->state == TCP_CLOSE && sock->state == SS_CONNECTING) {
		sock->state = SS_UNCONNECTED;
		return -ECONNREFUSED;
	}

	if (sk->state == TCP_ESTABLISHED)
		return -EISCONN;	/* No reconnect on a seqpacket socket */

	sk->state   = TCP_CLOSE;	
	sock->state = SS_UNCONNECTED;

	if (addr_len != sizeof(struct sockaddr_x25))
		return -EINVAL;

	if (addr->sx25_family != AF_X25)
		return -EINVAL;

	if ((dev = x25_get_route(&addr->sx25_addr)) == NULL)
		return -ENETUNREACH;

	if ((sk->protinfo.x25->neighbour = x25_get_neigh(dev)) == NULL)
		return -ENETUNREACH;

	x25_limit_facilities(&sk->protinfo.x25->facilities,
			     sk->protinfo.x25->neighbour);

	if ((sk->protinfo.x25->lci = x25_new_lci(sk->protinfo.x25->neighbour)) == 0)
		return -ENETUNREACH;

	if (sk->zapped)		/* Must bind first - autobinding does not work */
		return -EINVAL;

	if (strcmp(sk->protinfo.x25->source_addr.x25_addr, null_x25_address.x25_addr) == 0)
		memset(&sk->protinfo.x25->source_addr, '\0', X25_ADDR_LEN);

	sk->protinfo.x25->dest_addr = addr->sx25_addr;

	/* Move to connecting socket, start sending Connect Requests */
	sock->state   = SS_CONNECTING;
	sk->state     = TCP_SYN_SENT;

	sk->protinfo.x25->state = X25_STATE_1;

	x25_write_internal(sk, X25_CALL_REQUEST);

	x25_start_heartbeat(sk);
	x25_start_t21timer(sk);

	/* Now the loop */
	if (sk->state != TCP_ESTABLISHED && (flags & O_NONBLOCK))
		return -EINPROGRESS;

	cli();	/* To avoid races on the sleep */

	/*
	 * A Clear Request or timeout or failed routing will go to closed.
	 */
	while (sk->state == TCP_SYN_SENT) {
		interruptible_sleep_on(sk->sleep);
		if (signal_pending(current)) {
			sti();
			return -ERESTARTSYS;
		}
	}

	if (sk->state != TCP_ESTABLISHED) {
		sti();
		sock->state = SS_UNCONNECTED;
		return sock_error(sk);	/* Always set at this point */
	}

	sock->state = SS_CONNECTED;

	sti();

	return 0;
}
	
static int x25_accept(struct socket *sock, struct socket *newsock, int flags)
{
	struct sock *sk;
	struct sock *newsk;
	struct sk_buff *skb;

	if ((sk = sock->sk) == NULL)
		return -EINVAL;

	if (sk->type != SOCK_SEQPACKET)
		return -EOPNOTSUPP;

	if (sk->state != TCP_LISTEN)
		return -EINVAL;

	/*
	 *	The write queue this time is holding sockets ready to use
	 *	hooked into the CALL INDICATION we saved
	 */
	do {
		cli();
		if ((skb = skb_dequeue(&sk->receive_queue)) == NULL) {
			if (flags & O_NONBLOCK) {
				sti();
				return -EWOULDBLOCK;
			}
			interruptible_sleep_on(sk->sleep);
			if (signal_pending(current)) {
				sti();
				return -ERESTARTSYS;
			}
		}
	} while (skb == NULL);

	newsk = skb->sk;
	newsk->pair = NULL;
	newsk->socket = newsock;
	newsk->sleep = &newsock->wait;
	sti();

	/* Now attach up the new socket */
	skb->sk = NULL;
	kfree_skb(skb);
	sk->ack_backlog--;
	newsock->sk = newsk;
	newsock->state = SS_CONNECTED;

	return 0;
}

static int x25_getname(struct socket *sock, struct sockaddr *uaddr, int *uaddr_len, int peer)
{
	struct sockaddr_x25 *sx25 = (struct sockaddr_x25 *)uaddr;
	struct sock *sk = sock->sk;

	if (peer != 0) {
		if (sk->state != TCP_ESTABLISHED)
			return -ENOTCONN;
		sx25->sx25_addr   = sk->protinfo.x25->dest_addr;
	} else {
		sx25->sx25_addr   = sk->protinfo.x25->source_addr;
	}

	sx25->sx25_family = AF_X25;
	*uaddr_len = sizeof(struct sockaddr_x25);

	return 0;
}
 
int x25_rx_call_request(struct sk_buff *skb, struct x25_neigh *neigh, unsigned int lci)
{
	struct sock *sk;
	struct sock *make;
	x25_address source_addr, dest_addr;
	struct x25_facilities facilities;
	int len;

	/*
	 *	Remove the LCI and frame type.
	 */
	skb_pull(skb, X25_STD_MIN_LEN);

	/*
	 *	Extract the X.25 addresses and convert them to ASCII strings,
	 *	and remove them.
	 */
	skb_pull(skb, x25_addr_ntoa(skb->data, &source_addr, &dest_addr));

	/*
	 *	Find a listener for the particular address.
	 */
	sk = x25_find_listener(&source_addr);

	/*
	 *	We can't accept the Call Request.
	 */
	if (sk == NULL || sk->ack_backlog == sk->max_ack_backlog) {
		x25_transmit_clear_request(neigh, lci, 0x01);
		return 0;
	}

	/*
	 *	Try to reach a compromise on the requested facilities.
	 */
	if ((len = x25_negotiate_facilities(skb, sk, &facilities)) == -1) {
		x25_transmit_clear_request(neigh, lci, 0x01);
		return 0;
	}

	/*
	 * current neighbour/link might impose additional limits
	 * on certain facilties
	 */

	x25_limit_facilities(&facilities,neigh);

	/*
	 *	Try to create a new socket.
	 */
	if ((make = x25_make_new(sk)) == NULL) {
		x25_transmit_clear_request(neigh, lci, 0x01);
		return 0;
	}

	/*
	 *	Remove the facilities, leaving any Call User Data.
	 */
	skb_pull(skb, len);

	skb->sk     = make;
	make->state = TCP_ESTABLISHED;

	make->protinfo.x25->lci           = lci;
	make->protinfo.x25->dest_addr     = dest_addr;
	make->protinfo.x25->source_addr   = source_addr;
	make->protinfo.x25->neighbour     = neigh;
	make->protinfo.x25->facilities    = facilities;
	make->protinfo.x25->vc_facil_mask = sk->protinfo.x25->vc_facil_mask;

	x25_write_internal(make, X25_CALL_ACCEPTED);

	/*
	 *	Incoming Call User Data.
	 */
	if (skb->len >= 0) {
		memcpy(make->protinfo.x25->calluserdata.cuddata, skb->data, skb->len);
		make->protinfo.x25->calluserdata.cudlength = skb->len;
	}

	make->protinfo.x25->state = X25_STATE_3;

	sk->ack_backlog++;
	make->pair = sk;

	x25_insert_socket(make);

	skb_queue_head(&sk->receive_queue, skb);

	x25_start_heartbeat(make);

	if (!sk->dead)
		sk->data_ready(sk, skb->len);

	return 1;
}

static int x25_sendmsg(struct socket *sock, struct msghdr *msg, int len, struct scm_cookie *scm)
{
	struct sock *sk = sock->sk;
	struct sockaddr_x25 *usx25 = (struct sockaddr_x25 *)msg->msg_name;
	int err;
	struct sockaddr_x25 sx25;
	struct sk_buff *skb;
	unsigned char *asmptr;
	int size, qbit = 0;

	if (msg->msg_flags & ~(MSG_DONTWAIT | MSG_OOB | MSG_EOR))
		return -EINVAL;

	/* we currently don't support segmented records at the user interface */
	if (!(msg->msg_flags & (MSG_EOR|MSG_OOB)))
		return -EINVAL;

	if (sk->zapped)
		return -EADDRNOTAVAIL;

	if (sk->shutdown & SEND_SHUTDOWN) {
		send_sig(SIGPIPE, current, 0);
		return -EPIPE;
	}

	if (sk->protinfo.x25->neighbour == NULL)
		return -ENETUNREACH;

	if (usx25 != NULL) {
		if (msg->msg_namelen < sizeof(sx25))
			return -EINVAL;
		sx25 = *usx25;
		if (strcmp(sk->protinfo.x25->dest_addr.x25_addr, sx25.sx25_addr.x25_addr) != 0)
			return -EISCONN;
		if (sx25.sx25_family != AF_X25)
			return -EINVAL;
	} else {
		/*
		 *	FIXME 1003.1g - if the socket is like this because
		 *	it has become closed (not started closed) we ought
		 *	to SIGPIPE, EPIPE;
		 */
		if (sk->state != TCP_ESTABLISHED)
			return -ENOTCONN;

		sx25.sx25_family = AF_X25;
		sx25.sx25_addr   = sk->protinfo.x25->dest_addr;
	}

	SOCK_DEBUG(sk, "x25_sendmsg: sendto: Addresses built.\n");

	/* Build a packet */
	SOCK_DEBUG(sk, "x25_sendmsg: sendto: building packet.\n");

	if ((msg->msg_flags & MSG_OOB) && len > 32)
		len = 32;

	size = len + X25_MAX_L2_LEN + X25_EXT_MIN_LEN;

	if ((skb = sock_alloc_send_skb(sk, size, msg->msg_flags & MSG_DONTWAIT, &err)) == NULL)
		return err;
	X25_SKB_CB(skb)->flags = msg->msg_flags;

	skb_reserve(skb, X25_MAX_L2_LEN + X25_EXT_MIN_LEN);

	/*
	 *	Put the data on the end
	 */
	SOCK_DEBUG(sk, "x25_sendmsg: Copying user data\n");

	asmptr = skb->h.raw = skb_put(skb, len);

	memcpy_fromiovec(asmptr, msg->msg_iov, len);

	/*
	 *	If the Q BIT Include socket option is in force, the first
	 *	byte of the user data is the logical value of the Q Bit.
	 */
	if (sk->protinfo.x25->qbitincl) {
		qbit = skb->data[0];
		skb_pull(skb, 1);
	}

	/*
	 *	Push down the X.25 header
	 */
	SOCK_DEBUG(sk, "x25_sendmsg: Building X.25 Header.\n");

	if (msg->msg_flags & MSG_OOB) {
		if (sk->protinfo.x25->neighbour->extended) {
			asmptr    = skb_push(skb, X25_STD_MIN_LEN);
			*asmptr++ = ((sk->protinfo.x25->lci >> 8) & 0x0F) | X25_GFI_EXTSEQ;
			*asmptr++ = (sk->protinfo.x25->lci >> 0) & 0xFF;
			*asmptr++ = X25_INTERRUPT;
		} else {
			asmptr    = skb_push(skb, X25_STD_MIN_LEN);
			*asmptr++ = ((sk->protinfo.x25->lci >> 8) & 0x0F) | X25_GFI_STDSEQ;
			*asmptr++ = (sk->protinfo.x25->lci >> 0) & 0xFF;
			*asmptr++ = X25_INTERRUPT;
		}
	} else {
		if (sk->protinfo.x25->neighbour->extended) {
			/* Build an Extended X.25 header */
			asmptr    = skb_push(skb, X25_EXT_MIN_LEN);
			*asmptr++ = ((sk->protinfo.x25->lci >> 8) & 0x0F) | X25_GFI_EXTSEQ;
			*asmptr++ = (sk->protinfo.x25->lci >> 0) & 0xFF;
			*asmptr++ = X25_DATA;
			*asmptr++ = X25_DATA;
		} else {
			/* Build an Standard X.25 header */
			asmptr    = skb_push(skb, X25_STD_MIN_LEN);
			*asmptr++ = ((sk->protinfo.x25->lci >> 8) & 0x0F) | X25_GFI_STDSEQ;
			*asmptr++ = (sk->protinfo.x25->lci >> 0) & 0xFF;
			*asmptr++ = X25_DATA;
		}

		if (qbit)
			skb->data[0] |= X25_Q_BIT;
	}

	SOCK_DEBUG(sk, "x25_sendmsg: Built header.\n");
	SOCK_DEBUG(sk, "x25_sendmsg: Transmitting buffer\n");

	if (sk->state != TCP_ESTABLISHED) {
		kfree_skb(skb);
		return -ENOTCONN;
	}

	if (msg->msg_flags & MSG_OOB) {
		skb_queue_tail(&sk->protinfo.x25->interrupt_out_queue, skb);
	} else {
	        len = x25_output(sk, skb);
		if(len<0){
			kfree_skb(skb);
		} else {
			if(sk->protinfo.x25->qbitincl) len++;
		}
	}

	/*
	 * lock_sock() is currently only used to serialize this x25_kick()
	 * against input-driven x25_kick() calls. It currently only blocks
	 * incoming packets for this socket and does not protect against
	 * any other socket state changes and is not called from anywhere
	 * else. As x25_kick() cannot block and as long as all socket
	 * operations are BKL-wrapped, we don't need take to care about
	 * purging the backlog queue in x25_release().
	 *
	 * Using lock_sock() to protect all socket operations entirely
	 * (and making the whole x25 stack SMP aware) unfortunately would
	 * require major changes to {send,recv}msg and skb allocation methods.
	 * -> 2.5 ;)
	 */
	lock_sock(sk);
	x25_kick(sk);
	release_sock(sk);

	return len;
}


static int x25_recvmsg(struct socket *sock, struct msghdr *msg, int size, int flags, struct scm_cookie *scm)
{
	struct sock *sk = sock->sk;
	struct sockaddr_x25 *sx25 = (struct sockaddr_x25 *)msg->msg_name;
	int copied, qbit;
	struct sk_buff *skb;
	unsigned char *asmptr;
	int er;

	/*
	 * This works for seqpacket too. The receiver has ordered the queue for
	 * us! We do one quick check first though
	 */
	if (sk->state != TCP_ESTABLISHED)
		return -ENOTCONN;

	if (flags & MSG_OOB) {
		if (sk->urginline || skb_peek(&sk->protinfo.x25->interrupt_in_queue) == NULL)
			return -EINVAL;

		skb = skb_dequeue(&sk->protinfo.x25->interrupt_in_queue);

		skb_pull(skb, X25_STD_MIN_LEN);

		/*
		 *	No Q bit information on Interrupt data.
		 */
		if (sk->protinfo.x25->qbitincl) {
			asmptr  = skb_push(skb, 1);
			*asmptr = 0x00;
		}

		msg->msg_flags |= MSG_OOB;
	} else {
		/* Now we can treat all alike */
		if ((skb = skb_recv_datagram(sk, flags & ~MSG_DONTWAIT, flags & MSG_DONTWAIT, &er)) == NULL)
			return er;

		qbit = (skb->data[0] & X25_Q_BIT) == X25_Q_BIT;

		skb_pull(skb, (sk->protinfo.x25->neighbour->extended) ? X25_EXT_MIN_LEN : X25_STD_MIN_LEN);

		if (sk->protinfo.x25->qbitincl) {
			asmptr  = skb_push(skb, 1);
			*asmptr = qbit;
		}
	}

	skb->h.raw = skb->data;

	copied = skb->len;

	if (copied > size) {
		copied = size;
		msg->msg_flags |= MSG_TRUNC;
	}

	/* Currently, each datagram always contains a complete record */ 
	msg->msg_flags |= MSG_EOR;

	skb_copy_datagram_iovec(skb, 0, msg->msg_iov, copied);

	if (sx25 != NULL) {
		sx25->sx25_family = AF_X25;
		sx25->sx25_addr   = sk->protinfo.x25->dest_addr;
	}

	msg->msg_namelen = sizeof(struct sockaddr_x25);

	skb_free_datagram(sk, skb);
	lock_sock(sk);
	x25_check_rbuf(sk);
	release_sock(sk);

	return copied;
}


static int x25_ioctl(struct socket *sock, unsigned int cmd, unsigned long arg)
{
	struct sock *sk = sock->sk;

	switch (cmd) {
		case TIOCOUTQ: {
			int amount;
			amount = sk->sndbuf - atomic_read(&sk->wmem_alloc);
			if (amount < 0)
				amount = 0;
			return put_user(amount, (unsigned int *)arg);
		}

		case TIOCINQ: {
			struct sk_buff *skb;
			int amount = 0;
			/* These two are safe on a single CPU system as only user tasks fiddle here */
			if ((skb = skb_peek(&sk->receive_queue)) != NULL)
				amount = skb->len;
			return put_user(amount, (unsigned int *)arg);
		}

		case SIOCGSTAMP:
			if (sk != NULL) {
				if (sk->stamp.tv_sec == 0)
					return -ENOENT;
				return copy_to_user((void *)arg, &sk->stamp, sizeof(struct timeval)) ? -EFAULT : 0;
			}
			return -EINVAL;

		case SIOCGIFADDR:
		case SIOCSIFADDR:
		case SIOCGIFDSTADDR:
		case SIOCSIFDSTADDR:
		case SIOCGIFBRDADDR:
		case SIOCSIFBRDADDR:
		case SIOCGIFNETMASK:
		case SIOCSIFNETMASK:
		case SIOCGIFMETRIC:
		case SIOCSIFMETRIC:
			return -EINVAL;

		case SIOCADDRT:
		case SIOCDELRT:
			if (!capable(CAP_NET_ADMIN)) return -EPERM;
			return x25_route_ioctl(cmd, (void *)arg);

		case SIOCX25GSUBSCRIP:
			return x25_subscr_ioctl(cmd, (void *)arg);

		case SIOCX25SSUBSCRIP:
			if (!capable(CAP_NET_ADMIN)) return -EPERM;
			return x25_subscr_ioctl(cmd, (void *)arg);

		case SIOCX25GFACILITIES: {
			struct x25_facilities facilities;
			facilities = sk->protinfo.x25->facilities;
			return copy_to_user((void *)arg, &facilities, sizeof(facilities)) ? -EFAULT : 0;
		}

		case SIOCX25SFACILITIES: {
			struct x25_facilities facilities;
			if (copy_from_user(&facilities, (void *)arg, sizeof(facilities)))
				return -EFAULT;
			if (sk->state != TCP_LISTEN && sk->state != TCP_CLOSE)
				return -EINVAL;
			if (facilities.pacsize_in < X25_PS16 || facilities.pacsize_in > X25_PS4096)
				return -EINVAL;
			if (facilities.pacsize_out < X25_PS16 || facilities.pacsize_out > X25_PS4096)
				return -EINVAL;
			if (facilities.winsize_in < 1 || facilities.winsize_in > 127)
				return -EINVAL;
			if (facilities.throughput < 0x03 || facilities.throughput > 0xDD)
				return -EINVAL;
			if (facilities.reverse != 0 && facilities.reverse != 1)
				return -EINVAL;
			sk->protinfo.x25->facilities = facilities;
			return 0;
		}

		case SIOCX25GCALLUSERDATA: {
			struct x25_calluserdata calluserdata;
			calluserdata = sk->protinfo.x25->calluserdata;
			return copy_to_user((void *)arg, &calluserdata, sizeof(calluserdata)) ? -EFAULT : 0;
		}

		case SIOCX25SCALLUSERDATA: {
			struct x25_calluserdata calluserdata;
			if (copy_from_user(&calluserdata, (void *)arg, sizeof(calluserdata)))
				return -EFAULT;
			if (calluserdata.cudlength > X25_MAX_CUD_LEN)
				return -EINVAL;
			sk->protinfo.x25->calluserdata = calluserdata;
			return 0;
		}

		case SIOCX25GCAUSEDIAG: {
			struct x25_causediag causediag;
			causediag = sk->protinfo.x25->causediag;
			return copy_to_user((void *)arg, &causediag, sizeof(causediag)) ? -EFAULT : 0;
		}

 		default:
			return dev_ioctl(cmd, (void *)arg);
	}

	/*NOTREACHED*/
	return 0;
}

static int x25_get_info(char *buffer, char **start, off_t offset, int length)
{
	struct sock *s;
	struct net_device *dev;
	const char *devname;
	int len = 0;
	off_t pos = 0;
	off_t begin = 0;

	cli();

	len += sprintf(buffer, "dest_addr  src_addr   dev   lci st vs vr va   t  t2 t21 t22 t23 Snd-Q Rcv-Q inode\n");

	for (s = x25_list; s != NULL; s = s->next) {
		if (s->protinfo.x25->neighbour == NULL || (dev = s->protinfo.x25->neighbour->dev) == NULL)
			devname = "???";
		else
			devname = s->protinfo.x25->neighbour->dev->name;

		len += sprintf(buffer + len, "%-10s %-10s %-5s %3.3X  %d  %d  %d  %d %3lu %3lu %3lu %3lu %3lu %5d %5d %ld\n",
			(s->protinfo.x25->dest_addr.x25_addr[0] == '\0')   ? "*" : s->protinfo.x25->dest_addr.x25_addr,
			(s->protinfo.x25->source_addr.x25_addr[0] == '\0') ? "*" : s->protinfo.x25->source_addr.x25_addr,
			devname, 
			s->protinfo.x25->lci & 0x0FFF,
			s->protinfo.x25->state,
			s->protinfo.x25->vs,
			s->protinfo.x25->vr,
			s->protinfo.x25->va,
			x25_display_timer(s) / HZ,
			s->protinfo.x25->t2  / HZ,
			s->protinfo.x25->t21 / HZ,
			s->protinfo.x25->t22 / HZ,
			s->protinfo.x25->t23 / HZ,
			atomic_read(&s->wmem_alloc),
			atomic_read(&s->rmem_alloc),
			s->socket != NULL ? s->socket->inode->i_ino : 0L);

		pos = begin + len;

		if (pos < offset) {
			len   = 0;
			begin = pos;
		}

		if (pos > offset + length)
			break;
	}

	sti();

	*start = buffer + (offset - begin);
	len   -= (offset - begin);

	if (len > length) len = length;

	return(len);
} 

struct net_proto_family x25_family_ops = {
	family:		AF_X25,
	create:		x25_create,
};

static struct proto_ops SOCKOPS_WRAPPED(x25_proto_ops) = {
	family:		AF_X25,

	release:	x25_release,
	bind:		x25_bind,
	connect:	x25_connect,
	socketpair:	sock_no_socketpair,
	accept:		x25_accept,
	getname:	x25_getname,
	poll:		datagram_poll,
	ioctl:		x25_ioctl,
	listen:		x25_listen,
	shutdown:	sock_no_shutdown,
	setsockopt:	x25_setsockopt,
	getsockopt:	x25_getsockopt,
	sendmsg:	x25_sendmsg,
	recvmsg:	x25_recvmsg,
	mmap:		sock_no_mmap,
	sendpage:	sock_no_sendpage,
};

#include <linux/smp_lock.h>
SOCKOPS_WRAP(x25_proto, AF_X25);


static struct packet_type x25_packet_type = {
	type:		__constant_htons(ETH_P_X25),
	func:		x25_lapb_receive_frame,
};

struct notifier_block x25_dev_notifier = {
	notifier_call:	x25_device_event,
};

void x25_kill_by_neigh(struct x25_neigh *neigh)
{
	struct sock *s;

	for( s=x25_list; s != NULL; s=s->next){
		if( s->protinfo.x25->neighbour == neigh )
			x25_disconnect(s, ENETUNREACH, 0, 0);
	} 
}

static int __init x25_init(void)
{
#ifdef MODULE
	struct net_device *dev;
#endif /* MODULE */
	sock_register(&x25_family_ops);

	dev_add_pack(&x25_packet_type);

	register_netdevice_notifier(&x25_dev_notifier);

	printk(KERN_INFO "X.25 for Linux. Version 0.2 for Linux 2.1.15\n");

#ifdef CONFIG_SYSCTL
	x25_register_sysctl();
#endif

	proc_net_create("x25", 0, x25_get_info);
	proc_net_create("x25_routes", 0, x25_routes_get_info);

#ifdef MODULE
	/*
	 *	Register any pre existing devices.
	 */
	read_lock(&dev_base_lock);
	for (dev = dev_base; dev != NULL; dev = dev->next) {
		if ((dev->flags & IFF_UP) && (dev->type == ARPHRD_X25
#if defined(CONFIG_LLC) || defined(CONFIG_LLC_MODULE)
					   || dev->type == ARPHRD_ETHER
#endif
			))
			x25_link_device_up(dev);
	}
	read_unlock(&dev_base_lock);
#endif /* MODULE */
	return 0;
}
module_init(x25_init);



EXPORT_NO_SYMBOLS;

MODULE_AUTHOR("Jonathan Naylor <g4klx@g4klx.demon.co.uk>");
MODULE_DESCRIPTION("The X.25 Packet Layer network layer protocol");
MODULE_LICENSE("GPL");

static void __exit x25_exit(void)
{

	proc_net_remove("x25");
	proc_net_remove("x25_routes");

	x25_link_free();
	x25_route_free();

#ifdef CONFIG_SYSCTL
	x25_unregister_sysctl();
#endif

	unregister_netdevice_notifier(&x25_dev_notifier);

	dev_remove_pack(&x25_packet_type);

	sock_unregister(AF_X25);
}
module_exit(x25_exit);

