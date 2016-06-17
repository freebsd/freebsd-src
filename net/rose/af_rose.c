/*
 *	ROSE release 003
 *
 *	This code REQUIRES 2.1.15 or higher/ NET3.038
 *
 *	This module:
 *		This module is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 *	History
 *	ROSE 001	Jonathan(G4KLX)	Cloned from af_netrom.c.
 *			Alan(GW4PTS)	Hacked up for newer API stuff
 *			Terry (VK2KTJ)	Added support for variable length
 * 					address masks.
 *	ROSE 002	Jonathan(G4KLX)	Changed hdrincl to qbitincl.
 *					Added random number facilities entry.
 *					Variable number of ROSE devices.
 *	ROSE 003	Jonathan(G4KLX)	New timer architecture.
 *					Implemented idle timer.
 *					Added use count to neighbour.
 *                      Tomi(OH2BNS)    Fixed rose_getname().
 *                      Arnaldo C. Melo s/suser/capable/ + micro cleanups
 *                      Joroen (PE1RXQ) Use sock_orphan() on release.
 *
 *  ROSE 0.63	Jean-Paul(F6FBB) Fixed wrong length of L3 packets
 *					Added CLEAR_REQUEST facilities
 *  ROSE 0.64	Jean-Paul(F6FBB) Fixed null pointer in rose_kill_by_device
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/init.h>
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
#include <net/ax25.h>
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
#include <net/rose.h>
#include <linux/proc_fs.h>
#include <net/ip.h>
#include <net/arp.h>

int rose_ndevs = 10;

int sysctl_rose_restart_request_timeout = ROSE_DEFAULT_T0;
int sysctl_rose_call_request_timeout    = ROSE_DEFAULT_T1;
int sysctl_rose_reset_request_timeout   = ROSE_DEFAULT_T2;
int sysctl_rose_clear_request_timeout   = ROSE_DEFAULT_T3;
int sysctl_rose_no_activity_timeout     = ROSE_DEFAULT_IDLE;
int sysctl_rose_ack_hold_back_timeout   = ROSE_DEFAULT_HB;
int sysctl_rose_routing_control         = ROSE_DEFAULT_ROUTING;
int sysctl_rose_link_fail_timeout       = ROSE_DEFAULT_FAIL_TIMEOUT;
int sysctl_rose_maximum_vcs             = ROSE_DEFAULT_MAXVC;
int sysctl_rose_window_size             = ROSE_DEFAULT_WINDOW_SIZE;

static struct sock *rose_list;

static struct proto_ops rose_proto_ops;

ax25_address rose_callsign;

/*
 *	Convert a ROSE address into text.
 */
char *rose2asc(rose_address *addr)
{
	static char buffer[11];

	if (addr->rose_addr[0] == 0x00 && addr->rose_addr[1] == 0x00 &&
	    addr->rose_addr[2] == 0x00 && addr->rose_addr[3] == 0x00 &&
	    addr->rose_addr[4] == 0x00) {
		strcpy(buffer, "*");
	} else {
		sprintf(buffer, "%02X%02X%02X%02X%02X", addr->rose_addr[0] & 0xFF,
						addr->rose_addr[1] & 0xFF,
						addr->rose_addr[2] & 0xFF,
						addr->rose_addr[3] & 0xFF,
						addr->rose_addr[4] & 0xFF);
	}

	return buffer;
}

/*
 *	Compare two ROSE addresses, 0 == equal.
 */
int rosecmp(rose_address *addr1, rose_address *addr2)
{
	int i;

	for (i = 0; i < 5; i++)
		if (addr1->rose_addr[i] != addr2->rose_addr[i])
			return 1;

	return 0;
}

/*
 *	Compare two ROSE addresses for only mask digits, 0 == equal.
 */
int rosecmpm(rose_address *addr1, rose_address *addr2, unsigned short mask)
{
	int i, j;

	if (mask > 10)
		return 1;

	for (i = 0; i < mask; i++) {
		j = i / 2;

		if ((i % 2) != 0) {
			if ((addr1->rose_addr[j] & 0x0F) != (addr2->rose_addr[j] & 0x0F))
				return 1;
		} else {
			if ((addr1->rose_addr[j] & 0xF0) != (addr2->rose_addr[j] & 0xF0))
				return 1;
		}
	}

	return 0;
}

static void rose_free_sock(struct sock *sk)
{
	sk_free(sk);

	MOD_DEC_USE_COUNT;
}

static struct sock *rose_alloc_sock(void)
{
	struct sock *sk;
	rose_cb *rose;

	if ((sk = sk_alloc(PF_ROSE, GFP_ATOMIC, 1)) == NULL)
		return NULL;

	if ((rose = kmalloc(sizeof(*rose), GFP_ATOMIC)) == NULL) {
		sk_free(sk);
		return NULL;
	}

	MOD_INC_USE_COUNT;

	memset(rose, 0x00, sizeof(*rose));

	sk->protinfo.rose = rose;
	rose->sk          = sk;

	return sk;
}

/*
 *	Socket removal during an interrupt is now safe.
 */
static void rose_remove_socket(struct sock *sk)
{
	struct sock *s;
	unsigned long flags;

	save_flags(flags); cli();

	if ((s = rose_list) == sk) {
		rose_list = s->next;
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
 *	Kill all bound sockets on a broken link layer connection to a
 *	particular neighbour.
 */
void rose_kill_by_neigh(struct rose_neigh *neigh)
{
	struct sock *s;

	for (s = rose_list; s != NULL; s = s->next) {
		if (s->protinfo.rose->neighbour == neigh) {
			rose_disconnect(s, ENETUNREACH, ROSE_OUT_OF_ORDER, 0);
			s->protinfo.rose->neighbour->use--;
			s->protinfo.rose->neighbour = NULL;
		}
	}
}

/*
 *	Kill all bound sockets on a dropped device.
 */
static void rose_kill_by_device(struct net_device *dev)
{
	struct sock *s;
	
	for (s = rose_list; s != NULL; s = s->next) {
		if (s->protinfo.rose->device == dev) {
			rose_disconnect(s, ENETUNREACH, ROSE_OUT_OF_ORDER, 0);
			if (s->protinfo.rose->neighbour)
				s->protinfo.rose->neighbour->use--;
			s->protinfo.rose->device = NULL;
		}
	}
}

/*
 *	Handle device status changes.
 */
static int rose_device_event(struct notifier_block *this, unsigned long event, void *ptr)
{
	struct net_device *dev = (struct net_device *)ptr;

	if (event != NETDEV_DOWN)
		return NOTIFY_DONE;

	switch (dev->type) {
		case ARPHRD_ROSE:
			rose_kill_by_device(dev);
			break;
		case ARPHRD_AX25:
			rose_link_device_down(dev);
			rose_rt_device_down(dev);
			break;
	}

	return NOTIFY_DONE;
}

/*
 *	Add a socket to the bound sockets list.
 */
static void rose_insert_socket(struct sock *sk)
{
	unsigned long flags;

	save_flags(flags); cli();

	sk->next  = rose_list;
	rose_list = sk;

	restore_flags(flags);
}

/*
 *	Find a socket that wants to accept the Call Request we just
 *	received.
 */
static struct sock *rose_find_listener(rose_address *addr, ax25_address *call)
{
	unsigned long flags;
	struct sock *s;

	save_flags(flags); cli();

	for (s = rose_list; s != NULL; s = s->next) {
		if (rosecmp(&s->protinfo.rose->source_addr, addr) == 0 && ax25cmp(&s->protinfo.rose->source_call, call) == 0 && s->protinfo.rose->source_ndigis == 0 && s->state == TCP_LISTEN) {
			restore_flags(flags);
			return s;
		}
	}

	for (s = rose_list; s != NULL; s = s->next) {
		if (rosecmp(&s->protinfo.rose->source_addr, addr) == 0 && ax25cmp(&s->protinfo.rose->source_call, &null_ax25_address) == 0 && s->state == TCP_LISTEN) {
			restore_flags(flags);
			return s;
		}
	}

	restore_flags(flags);
	return NULL;
}

/*
 *	Find a connected ROSE socket given my LCI and device.
 */
struct sock *rose_find_socket(unsigned int lci, struct rose_neigh *neigh)
{
	struct sock *s;
	unsigned long flags;

	save_flags(flags); cli();

	for (s = rose_list; s != NULL; s = s->next) {
		if (s->protinfo.rose->lci == lci && s->protinfo.rose->neighbour == neigh) {
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
unsigned int rose_new_lci(struct rose_neigh *neigh)
{
	int lci;

	if (neigh->dce_mode) {
		for (lci = 1; lci <= sysctl_rose_maximum_vcs; lci++)
			if (rose_find_socket(lci, neigh) == NULL && rose_route_free_lci(lci, neigh) == NULL)
				return lci;
	} else {
		for (lci = sysctl_rose_maximum_vcs; lci > 0; lci--)
			if (rose_find_socket(lci, neigh) == NULL && rose_route_free_lci(lci, neigh) == NULL)
				return lci;
	}

	return 0;
}

/*
 *	Deferred destroy.
 */
void rose_destroy_socket(struct sock *);

/*
 *	Handler for deferred kills.
 */
static void rose_destroy_timer(unsigned long data)
{
	rose_destroy_socket((struct sock *)data);
}

/*
 *	This is called from user mode and the timers. Thus it protects itself against
 *	interrupt users but doesn't worry about being called during work.
 *	Once it is removed from the queue no interrupt or bottom half will
 *	touch it and we are (fairly 8-) ) safe.
 */
void rose_destroy_socket(struct sock *sk)	/* Not static as it's used by the timer */
{
	struct sk_buff *skb;
	unsigned long flags;

	save_flags(flags); cli();

	rose_stop_heartbeat(sk);
	rose_stop_idletimer(sk);
	rose_stop_timer(sk);

	rose_remove_socket(sk);
	rose_clear_queues(sk);		/* Flush the queues */

	while ((skb = skb_dequeue(&sk->receive_queue)) != NULL) {
		if (skb->sk != sk) {			/* A pending connection */
			skb->sk->dead = 1;	/* Queue the unaccepted socket for death */
			rose_start_heartbeat(skb->sk);
			skb->sk->protinfo.rose->state = ROSE_STATE_0;
		}

		kfree_skb(skb);
	}

	if (atomic_read(&sk->wmem_alloc) != 0 || atomic_read(&sk->rmem_alloc) != 0) {
		/* Defer: outstanding buffers */
		init_timer(&sk->timer);
		sk->timer.expires  = jiffies + 10 * HZ;
		sk->timer.function = rose_destroy_timer;
		sk->timer.data     = (unsigned long)sk;
		add_timer(&sk->timer);
	} else {
		rose_free_sock(sk);
	}

	restore_flags(flags);
}

/*
 *	Handling for system calls applied via the various interfaces to a
 *	ROSE socket object.
 */

static int rose_setsockopt(struct socket *sock, int level, int optname,
	char *optval, int optlen)
{
	struct sock *sk = sock->sk;
	int opt;

	if (level != SOL_ROSE)
		return -ENOPROTOOPT;

	if (optlen < sizeof(int))
		return -EINVAL;

	if (get_user(opt, (int *)optval))
		return -EFAULT;

	switch (optname) {
		case ROSE_DEFER:
			sk->protinfo.rose->defer = opt ? 1 : 0;
			return 0;

		case ROSE_T1:
			if (opt < 1)
				return -EINVAL;
			sk->protinfo.rose->t1 = opt * HZ;
			return 0;

		case ROSE_T2:
			if (opt < 1)
				return -EINVAL;
			sk->protinfo.rose->t2 = opt * HZ;
			return 0;

		case ROSE_T3:
			if (opt < 1)
				return -EINVAL;
			sk->protinfo.rose->t3 = opt * HZ;
			return 0;

		case ROSE_HOLDBACK:
			if (opt < 1)
				return -EINVAL;
			sk->protinfo.rose->hb = opt * HZ;
			return 0;

		case ROSE_IDLE:
			if (opt < 0)
				return -EINVAL;
			sk->protinfo.rose->idle = opt * 60 * HZ;
			return 0;

		case ROSE_QBITINCL:
			sk->protinfo.rose->qbitincl = opt ? 1 : 0;
			return 0;

		default:
			return -ENOPROTOOPT;
	}
}

static int rose_getsockopt(struct socket *sock, int level, int optname,
	char *optval, int *optlen)
{
	struct sock *sk = sock->sk;
	int val = 0;
	int len;

	if (level != SOL_ROSE)
		return -ENOPROTOOPT;
		
	if (get_user(len, optlen))
		return -EFAULT;

	if (len < 0)
		return -EINVAL;
			
	switch (optname) {
		case ROSE_DEFER:
			val = sk->protinfo.rose->defer;
			break;

		case ROSE_T1:
			val = sk->protinfo.rose->t1 / HZ;
			break;

		case ROSE_T2:
			val = sk->protinfo.rose->t2 / HZ;
			break;

		case ROSE_T3:
			val = sk->protinfo.rose->t3 / HZ;
			break;

		case ROSE_HOLDBACK:
			val = sk->protinfo.rose->hb / HZ;
			break;

		case ROSE_IDLE:
			val = sk->protinfo.rose->idle / (60 * HZ);
			break;

		case ROSE_QBITINCL:
			val = sk->protinfo.rose->qbitincl;
			break;

		default:
			return -ENOPROTOOPT;
	}

	len = min_t(unsigned int, len, sizeof(int));

	if (put_user(len, optlen))
		return -EFAULT;

	return copy_to_user(optval, &val, len) ? -EFAULT : 0;
}

static int rose_listen(struct socket *sock, int backlog)
{
	struct sock *sk = sock->sk;

	if (sk->state != TCP_LISTEN) {
		sk->protinfo.rose->dest_ndigis = 0;
		memset(&sk->protinfo.rose->dest_addr, '\0', ROSE_ADDR_LEN);
		memset(&sk->protinfo.rose->dest_call, '\0', AX25_ADDR_LEN);
		memset(sk->protinfo.rose->dest_digis, '\0', AX25_ADDR_LEN*ROSE_MAX_DIGIS);
		sk->max_ack_backlog = backlog;
		sk->state           = TCP_LISTEN;
		return 0;
	}

	return -EOPNOTSUPP;
}

static int rose_create(struct socket *sock, int protocol)
{
	struct sock *sk;
	rose_cb *rose;

	if (sock->type != SOCK_SEQPACKET || protocol != 0)
		return -ESOCKTNOSUPPORT;

	if ((sk = rose_alloc_sock()) == NULL)
		return -ENOMEM;

	rose = sk->protinfo.rose;

	sock_init_data(sock, sk);
	
	skb_queue_head_init(&rose->ack_queue);
#ifdef M_BIT
	skb_queue_head_init(&rose->frag_queue);
	rose->fraglen    = 0;
#endif

	sock->ops    = &rose_proto_ops;
	sk->protocol = protocol;

	init_timer(&rose->timer);
	init_timer(&rose->idletimer);

	rose->t1   = sysctl_rose_call_request_timeout;
	rose->t2   = sysctl_rose_reset_request_timeout;
	rose->t3   = sysctl_rose_clear_request_timeout;
	rose->hb   = sysctl_rose_ack_hold_back_timeout;
	rose->idle = sysctl_rose_no_activity_timeout;

	rose->state = ROSE_STATE_0;

	return 0;
}

static struct sock *rose_make_new(struct sock *osk)
{
	struct sock *sk;
	rose_cb *rose;

	if (osk->type != SOCK_SEQPACKET)
		return NULL;

	if ((sk = rose_alloc_sock()) == NULL)
		return NULL;

	rose = sk->protinfo.rose;

	sock_init_data(NULL, sk);

	skb_queue_head_init(&rose->ack_queue);
#ifdef M_BIT
	skb_queue_head_init(&rose->frag_queue);
	rose->fraglen  = 0;
#endif

	sk->type     = osk->type;
	sk->socket   = osk->socket;
	sk->priority = osk->priority;
	sk->protocol = osk->protocol;
	sk->rcvbuf   = osk->rcvbuf;
	sk->sndbuf   = osk->sndbuf;
	sk->debug    = osk->debug;
	sk->state    = TCP_ESTABLISHED;
	sk->sleep    = osk->sleep;
	sk->zapped   = osk->zapped;

	init_timer(&rose->timer);
	init_timer(&rose->idletimer);

	rose->t1      = osk->protinfo.rose->t1;
	rose->t2      = osk->protinfo.rose->t2;
	rose->t3      = osk->protinfo.rose->t3;
	rose->hb      = osk->protinfo.rose->hb;
	rose->idle    = osk->protinfo.rose->idle;

	rose->defer    = osk->protinfo.rose->defer;
	rose->device   = osk->protinfo.rose->device;
	rose->qbitincl = osk->protinfo.rose->qbitincl;

	return sk;
}

static int rose_release(struct socket *sock)
{
	struct sock *sk = sock->sk;

	if (sk == NULL) return 0;

	switch (sk->protinfo.rose->state) {

		case ROSE_STATE_0:
			rose_disconnect(sk, 0, -1, -1);
			rose_destroy_socket(sk);
			break;

		case ROSE_STATE_2:
			sk->protinfo.rose->neighbour->use--;
			rose_disconnect(sk, 0, -1, -1);
			rose_destroy_socket(sk);
			break;

		case ROSE_STATE_1:
		case ROSE_STATE_3:
		case ROSE_STATE_4:
		case ROSE_STATE_5:
			rose_clear_queues(sk);
			rose_stop_idletimer(sk);
			rose_write_internal(sk, ROSE_CLEAR_REQUEST);
			rose_start_t3timer(sk);
			sk->protinfo.rose->state = ROSE_STATE_2;
			sk->state                = TCP_CLOSE;
			sk->shutdown            |= SEND_SHUTDOWN;
			sk->state_change(sk);
			sock_orphan(sk);
			sk->destroy              = 1;
			break;

		default:
			sk->socket = NULL;
			break;
	}

	sock->sk = NULL;	

	return 0;
}

static int rose_bind(struct socket *sock, struct sockaddr *uaddr, int addr_len)
{
	struct sock *sk = sock->sk;
	struct sockaddr_rose *addr = (struct sockaddr_rose *)uaddr;
	struct net_device *dev;
	ax25_address *user, *source;
	int n;

	if (sk->zapped == 0)
		return -EINVAL;

	if (addr_len != sizeof(struct sockaddr_rose) && addr_len != sizeof(struct full_sockaddr_rose))
		return -EINVAL;

	if (addr->srose_family != AF_ROSE)
		return -EINVAL;

	if (addr_len == sizeof(struct sockaddr_rose) && addr->srose_ndigis > 1)
		return -EINVAL;

	if (addr->srose_ndigis > ROSE_MAX_DIGIS)
		return -EINVAL;

	if ((dev = rose_dev_get(&addr->srose_addr)) == NULL) {
		SOCK_DEBUG(sk, "ROSE: bind failed: invalid address\n");
		return -EADDRNOTAVAIL;
	}

	source = &addr->srose_call;

	if ((user = ax25_findbyuid(current->euid)) == NULL) {
		if (ax25_uid_policy && !capable(CAP_NET_BIND_SERVICE))
			return -EACCES;
		user = source;
	}

	sk->protinfo.rose->source_addr   = addr->srose_addr;
	sk->protinfo.rose->source_call   = *user;
	sk->protinfo.rose->device        = dev;
	sk->protinfo.rose->source_ndigis = addr->srose_ndigis;

	if (addr_len == sizeof(struct full_sockaddr_rose)) {
		struct full_sockaddr_rose *full_addr = (struct full_sockaddr_rose *)uaddr;
		for (n = 0 ; n < addr->srose_ndigis ; n++)
			sk->protinfo.rose->source_digis[n] = full_addr->srose_digis[n];
	} else {
		if (sk->protinfo.rose->source_ndigis == 1) {
			sk->protinfo.rose->source_digis[0] = addr->srose_digi;
		}
	}

	rose_insert_socket(sk);

	sk->zapped = 0;
	SOCK_DEBUG(sk, "ROSE: socket is bound\n");
	return 0;
}

static int rose_connect(struct socket *sock, struct sockaddr *uaddr, int addr_len, int flags)
{
	struct sock *sk = sock->sk;
	struct sockaddr_rose *addr = (struct sockaddr_rose *)uaddr;
	unsigned char cause, diagnostic;
	ax25_address *user;
	struct net_device *dev;
	int n;

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

	if (addr_len != sizeof(struct sockaddr_rose) && addr_len != sizeof(struct full_sockaddr_rose))
		return -EINVAL;

	if (addr->srose_family != AF_ROSE)
		return -EINVAL;

	if (addr_len == sizeof(struct sockaddr_rose) && addr->srose_ndigis > 1)
		return -EINVAL;

	if (addr->srose_ndigis > ROSE_MAX_DIGIS)
		return -EINVAL;

	/* Source + Destination digis should not exceed ROSE_MAX_DIGIS */
	if ((sk->protinfo.rose->source_ndigis + addr->srose_ndigis) > ROSE_MAX_DIGIS)
		return -EINVAL;

	if ((sk->protinfo.rose->neighbour = rose_get_neigh(&addr->srose_addr, &cause, &diagnostic)) == NULL)
		return -ENETUNREACH;

	if ((sk->protinfo.rose->lci = rose_new_lci(sk->protinfo.rose->neighbour)) == 0)
		return -ENETUNREACH;

	if (sk->zapped) {	/* Must bind first - autobinding in this may or may not work */
		sk->zapped = 0;

		if ((dev = rose_dev_first()) == NULL)
			return -ENETUNREACH;

		if ((user = ax25_findbyuid(current->euid)) == NULL)
			return -EINVAL;

		memcpy(&sk->protinfo.rose->source_addr, dev->dev_addr, ROSE_ADDR_LEN);
		sk->protinfo.rose->source_call = *user;
		sk->protinfo.rose->device      = dev;

		rose_insert_socket(sk);		/* Finish the bind */
	}

	sk->protinfo.rose->dest_addr   = addr->srose_addr;
	sk->protinfo.rose->dest_call   = addr->srose_call;
	sk->protinfo.rose->rand        = ((int)sk->protinfo.rose & 0xFFFF) + sk->protinfo.rose->lci;
	sk->protinfo.rose->dest_ndigis = addr->srose_ndigis;

	if (addr_len == sizeof(struct full_sockaddr_rose)) {
		struct full_sockaddr_rose *full_addr = (struct full_sockaddr_rose *)uaddr;
		for (n = 0 ; n < addr->srose_ndigis ; n++)
			sk->protinfo.rose->dest_digis[n] = full_addr->srose_digis[n];
	} else {
		if (sk->protinfo.rose->dest_ndigis == 1) {
			sk->protinfo.rose->dest_digis[0] = addr->srose_digi;
		}
	}

	/* Move to connecting socket, start sending Connect Requests */
	sock->state   = SS_CONNECTING;
	sk->state     = TCP_SYN_SENT;

	sk->protinfo.rose->state = ROSE_STATE_1;

	sk->protinfo.rose->neighbour->use++;

	rose_write_internal(sk, ROSE_CALL_REQUEST);
	rose_start_heartbeat(sk);
	rose_start_t1timer(sk);

	/* Now the loop */
	if (sk->state != TCP_ESTABLISHED && (flags & O_NONBLOCK))
		return -EINPROGRESS;

	cli();	/* To avoid races on the sleep */

	/*
	 * A Connect Ack with Choke or timeout or failed routing will go to closed.
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

static int rose_accept(struct socket *sock, struct socket *newsock, int flags)
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
	 *	hooked into the SABM we saved
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

	return 0;
}

static int rose_getname(struct socket *sock, struct sockaddr *uaddr,
	int *uaddr_len, int peer)
{
	struct full_sockaddr_rose *srose = (struct full_sockaddr_rose *)uaddr;
	struct sock *sk = sock->sk;
	int n;

	if (peer != 0) {
		if (sk->state != TCP_ESTABLISHED)
			return -ENOTCONN;
		srose->srose_family = AF_ROSE;
		srose->srose_addr   = sk->protinfo.rose->dest_addr;
		srose->srose_call   = sk->protinfo.rose->dest_call;
		srose->srose_ndigis = sk->protinfo.rose->dest_ndigis;
		for (n = 0 ; n < sk->protinfo.rose->dest_ndigis ; n++)
			srose->srose_digis[n] = sk->protinfo.rose->dest_digis[n];
	} else {
		srose->srose_family = AF_ROSE;
		srose->srose_addr   = sk->protinfo.rose->source_addr;
		srose->srose_call   = sk->protinfo.rose->source_call;
		srose->srose_ndigis = sk->protinfo.rose->source_ndigis;
		for (n = 0 ; n < sk->protinfo.rose->source_ndigis ; n++)
			srose->srose_digis[n] = sk->protinfo.rose->source_digis[n];
	}

	*uaddr_len = sizeof(struct full_sockaddr_rose);
	return 0;
}

int rose_rx_call_request(struct sk_buff *skb, struct net_device *dev, struct rose_neigh *neigh, unsigned int lci)
{
	struct sock *sk;
	struct sock *make;
	struct rose_facilities_struct facilities;
	int n, len;

	skb->sk = NULL;		/* Initially we don't know who it's for */

	/*
	 *	skb->data points to the rose frame start
	 */
	memset(&facilities, 0x00, sizeof(struct rose_facilities_struct));
	
	len  = (((skb->data[3] >> 4) & 0x0F) + 1) / 2;
	len += (((skb->data[3] >> 0) & 0x0F) + 1) / 2;
	if (!rose_parse_facilities(skb->data + len + 4, &facilities)) {
		rose_transmit_clear_request(neigh, lci, ROSE_INVALID_FACILITY, 76);
		return 0;
	}

	sk = rose_find_listener(&facilities.source_addr, &facilities.source_call);

	/*
	 * We can't accept the Call Request.
	 */
	if (sk == NULL || sk->ack_backlog == sk->max_ack_backlog || (make = rose_make_new(sk)) == NULL) {
		rose_transmit_clear_request(neigh, lci, ROSE_NETWORK_CONGESTION, 120);
		return 0;
	}

	skb->sk     = make;
	make->state = TCP_ESTABLISHED;

	make->protinfo.rose->lci           = lci;
	make->protinfo.rose->dest_addr     = facilities.dest_addr;
	make->protinfo.rose->dest_call     = facilities.dest_call;
	make->protinfo.rose->dest_ndigis   = facilities.dest_ndigis;
	for (n = 0 ; n < facilities.dest_ndigis ; n++)
		make->protinfo.rose->dest_digis[n] = facilities.dest_digis[n];
	make->protinfo.rose->source_addr   = facilities.source_addr;
	make->protinfo.rose->source_call   = facilities.source_call;
	make->protinfo.rose->source_ndigis = facilities.source_ndigis;
	for (n = 0 ; n < facilities.source_ndigis ; n++)
		make->protinfo.rose->source_digis[n]= facilities.source_digis[n];
	make->protinfo.rose->neighbour     = neigh;
	make->protinfo.rose->device        = dev;
	make->protinfo.rose->facilities    = facilities;

	make->protinfo.rose->neighbour->use++;

	if (sk->protinfo.rose->defer) {
		make->protinfo.rose->state = ROSE_STATE_5;
	} else {
		rose_write_internal(make, ROSE_CALL_ACCEPTED);
		make->protinfo.rose->state = ROSE_STATE_3;
		rose_start_idletimer(make);
	}

	make->protinfo.rose->condition = 0x00;
	make->protinfo.rose->vs        = 0;
	make->protinfo.rose->va        = 0;
	make->protinfo.rose->vr        = 0;
	make->protinfo.rose->vl        = 0;
	sk->ack_backlog++;
	make->pair = sk;

	rose_insert_socket(make);

	skb_queue_head(&sk->receive_queue, skb);

	rose_start_heartbeat(make);

	if (!sk->dead)
		sk->data_ready(sk, skb->len);

	return 1;
}

static int rose_sendmsg(struct socket *sock, struct msghdr *msg, int len, 
				struct scm_cookie *scm)
{
	struct sock *sk = sock->sk;
	struct sockaddr_rose *usrose = (struct sockaddr_rose *)msg->msg_name;
	int err;
	struct full_sockaddr_rose srose;
	struct sk_buff *skb;
	unsigned char *asmptr;
	int n, size, qbit = 0;

	if (msg->msg_flags & ~(MSG_DONTWAIT|MSG_EOR))
		return -EINVAL;

	if (sk->zapped)
		return -EADDRNOTAVAIL;

	if (sk->shutdown & SEND_SHUTDOWN) {
		send_sig(SIGPIPE, current, 0);
		return -EPIPE;
	}

	if (sk->protinfo.rose->neighbour == NULL || sk->protinfo.rose->device == NULL)
		return -ENETUNREACH;

	if (usrose != NULL) {
		if (msg->msg_namelen != sizeof(struct sockaddr_rose) && msg->msg_namelen != sizeof(struct full_sockaddr_rose))
			return -EINVAL;
		memset(&srose, 0, sizeof(struct full_sockaddr_rose));
		memcpy(&srose, usrose, msg->msg_namelen);
		if (rosecmp(&sk->protinfo.rose->dest_addr, &srose.srose_addr) != 0 ||
		    ax25cmp(&sk->protinfo.rose->dest_call, &srose.srose_call) != 0)
			return -EISCONN;
		if (srose.srose_ndigis != sk->protinfo.rose->dest_ndigis)
			return -EISCONN;
		if (srose.srose_ndigis == sk->protinfo.rose->dest_ndigis) {
			for (n = 0 ; n < srose.srose_ndigis ; n++)
				if (ax25cmp(&sk->protinfo.rose->dest_digis[n], &srose.srose_digis[n]) != 0)
					return -EISCONN;
		}
		if (srose.srose_family != AF_ROSE)
			return -EINVAL;
	} else {
		if (sk->state != TCP_ESTABLISHED)
			return -ENOTCONN;

		srose.srose_family = AF_ROSE;
		srose.srose_addr   = sk->protinfo.rose->dest_addr;
		srose.srose_call   = sk->protinfo.rose->dest_call;
		srose.srose_ndigis = sk->protinfo.rose->dest_ndigis;
		for (n = 0 ; n < sk->protinfo.rose->dest_ndigis ; n++)
			srose.srose_digis[n] = sk->protinfo.rose->dest_digis[n];
	}

	SOCK_DEBUG(sk, "ROSE: sendto: Addresses built.\n");

	/* Build a packet */
	SOCK_DEBUG(sk, "ROSE: sendto: building packet.\n");
	size = len + AX25_BPQ_HEADER_LEN + AX25_MAX_HEADER_LEN + ROSE_MIN_LEN;

	if ((skb = sock_alloc_send_skb(sk, size, msg->msg_flags & MSG_DONTWAIT, &err)) == NULL)
		return err;

	skb_reserve(skb, AX25_BPQ_HEADER_LEN + AX25_MAX_HEADER_LEN + ROSE_MIN_LEN);

	/*
	 *	Put the data on the end
	 */
	SOCK_DEBUG(sk, "ROSE: Appending user data\n");

	asmptr = skb->h.raw = skb_put(skb, len);

	memcpy_fromiovec(asmptr, msg->msg_iov, len);

	/*
	 *	If the Q BIT Include socket option is in force, the first
	 *	byte of the user data is the logical value of the Q Bit.
	 */
	if (sk->protinfo.rose->qbitincl) {
		qbit = skb->data[0];
		skb_pull(skb, 1);
	}

	/*
	 *	Push down the ROSE header
	 */
	asmptr = skb_push(skb, ROSE_MIN_LEN);

	SOCK_DEBUG(sk, "ROSE: Building Network Header.\n");

	/* Build a ROSE Network header */
	asmptr[0] = ((sk->protinfo.rose->lci >> 8) & 0x0F) | ROSE_GFI;
	asmptr[1] = (sk->protinfo.rose->lci >> 0) & 0xFF;
	asmptr[2] = ROSE_DATA;

	if (qbit)
		asmptr[0] |= ROSE_Q_BIT;

	SOCK_DEBUG(sk, "ROSE: Built header.\n");

	SOCK_DEBUG(sk, "ROSE: Transmitting buffer\n");
	
	if (sk->state != TCP_ESTABLISHED) {
		kfree_skb(skb);
		return -ENOTCONN;
	}

#ifdef M_BIT
#define ROSE_PACLEN (256-ROSE_MIN_LEN)
	if (skb->len - ROSE_MIN_LEN > ROSE_PACLEN) {
		unsigned char header[ROSE_MIN_LEN];
		struct sk_buff *skbn;
		int frontlen;
		int lg;
		
		/* Save a copy of the Header */
		memcpy(header, skb->data, ROSE_MIN_LEN);
		skb_pull(skb, ROSE_MIN_LEN);

		frontlen = skb_headroom(skb);

		while (skb->len > 0) {
			if ((skbn = sock_alloc_send_skb(sk, frontlen + ROSE_PACLEN, 0, &err)) == NULL)
				return err;

			skbn->sk   = sk;
			skbn->free = 1;
			skbn->arp  = 1;

			skb_reserve(skbn, frontlen);

			lg = (ROSE_PACLEN > skb->len) ? skb->len : ROSE_PACLEN;

			/* Copy the user data */
			memcpy(skb_put(skbn, lg), skb->data, lg);
			skb_pull(skb, lg);

			/* Duplicate the Header */
			skb_push(skbn, ROSE_MIN_LEN);
			memcpy(skbn->data, header, ROSE_MIN_LEN);

			if (skb->len > 0)
				skbn->data[2] |= M_BIT;
		
			skb_queue_tail(&sk->write_queue, skbn); /* Throw it on the queue */
		}
		
		skb->free = 1;
		kfree_skb(skb, FREE_WRITE);
	} else {
		skb_queue_tail(&sk->write_queue, skb);		/* Throw it on the queue */
	}
#else
	skb_queue_tail(&sk->write_queue, skb);	/* Shove it onto the queue */
#endif

	rose_kick(sk);

	return len;
}


static int rose_recvmsg(struct socket *sock, struct msghdr *msg, int size, 
		   int flags, struct scm_cookie *scm)
{
	struct sock *sk = sock->sk;
	struct sockaddr_rose *srose = (struct sockaddr_rose *)msg->msg_name;
	int copied, qbit;
	unsigned char *asmptr;
	struct sk_buff *skb;
	int n, er;

	/*
	 * This works for seqpacket too. The receiver has ordered the queue for
	 * us! We do one quick check first though
	 */
	if (sk->state != TCP_ESTABLISHED)
		return -ENOTCONN;

	/* Now we can treat all alike */
	if ((skb = skb_recv_datagram(sk, flags & ~MSG_DONTWAIT, flags & MSG_DONTWAIT, &er)) == NULL)
		return er;

	qbit = (skb->data[0] & ROSE_Q_BIT) == ROSE_Q_BIT;

	skb_pull(skb, ROSE_MIN_LEN);

	if (sk->protinfo.rose->qbitincl) {
		asmptr  = skb_push(skb, 1);
		*asmptr = qbit;
	}

	skb->h.raw = skb->data;
	copied     = skb->len;

	if (copied > size) {
		copied = size;
		msg->msg_flags |= MSG_TRUNC;
	}

	skb_copy_datagram_iovec(skb, 0, msg->msg_iov, copied);

	if (srose != NULL) {
		srose->srose_family = AF_ROSE;
		srose->srose_addr   = sk->protinfo.rose->dest_addr;
		srose->srose_call   = sk->protinfo.rose->dest_call;
		srose->srose_ndigis = sk->protinfo.rose->dest_ndigis;
		if (msg->msg_namelen >= sizeof(struct full_sockaddr_rose)) {
			struct full_sockaddr_rose *full_srose = (struct full_sockaddr_rose *)msg->msg_name;
			for (n = 0 ; n < sk->protinfo.rose->dest_ndigis ; n++)
				full_srose->srose_digis[n] = sk->protinfo.rose->dest_digis[n];
			msg->msg_namelen = sizeof(struct full_sockaddr_rose);
		} else {
			if (sk->protinfo.rose->dest_ndigis >= 1) {
				srose->srose_ndigis = 1;
				srose->srose_digi = sk->protinfo.rose->dest_digis[0];
			}
			msg->msg_namelen = sizeof(struct sockaddr_rose);
		}
	}

	skb_free_datagram(sk, skb);

	return copied;
}


static int rose_ioctl(struct socket *sock, unsigned int cmd, unsigned long arg)
{
	struct sock *sk = sock->sk;

	switch (cmd) {
		case TIOCOUTQ: {
			long amount;
			amount = sk->sndbuf - atomic_read(&sk->wmem_alloc);
			if (amount < 0)
				amount = 0;
			return put_user(amount, (unsigned int *)arg);
		}

		case TIOCINQ: {
			struct sk_buff *skb;
			long amount = 0L;
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
		case SIOCRSCLRRT:
			if (!capable(CAP_NET_ADMIN)) return -EPERM;
			return rose_rt_ioctl(cmd, (void *)arg);

		case SIOCRSGCAUSE: {
			struct rose_cause_struct rose_cause;
			rose_cause.cause      = sk->protinfo.rose->cause;
			rose_cause.diagnostic = sk->protinfo.rose->diagnostic;
			return copy_to_user((void *)arg, &rose_cause, sizeof(struct rose_cause_struct)) ? -EFAULT : 0;
		}

		case SIOCRSSCAUSE: {
			struct rose_cause_struct rose_cause;
			if (copy_from_user(&rose_cause, (void *)arg, sizeof(struct rose_cause_struct)))
				return -EFAULT;
			sk->protinfo.rose->cause      = rose_cause.cause;
			sk->protinfo.rose->diagnostic = rose_cause.diagnostic;
			return 0;
		}

		case SIOCRSSL2CALL:
			if (!capable(CAP_NET_ADMIN)) return -EPERM;
			if (ax25cmp(&rose_callsign, &null_ax25_address) != 0)
				ax25_listen_release(&rose_callsign, NULL);
			if (copy_from_user(&rose_callsign, (void *)arg, sizeof(ax25_address)))
				return -EFAULT;
			if (ax25cmp(&rose_callsign, &null_ax25_address) != 0)
				ax25_listen_register(&rose_callsign, NULL);
			return 0;

		case SIOCRSGL2CALL:
			return copy_to_user((void *)arg, &rose_callsign, sizeof(ax25_address)) ? -EFAULT : 0;

		case SIOCRSACCEPT:
			if (sk->protinfo.rose->state == ROSE_STATE_5) {
				rose_write_internal(sk, ROSE_CALL_ACCEPTED);
				rose_start_idletimer(sk);
				sk->protinfo.rose->condition = 0x00;
				sk->protinfo.rose->vs        = 0;
				sk->protinfo.rose->va        = 0;
				sk->protinfo.rose->vr        = 0;
				sk->protinfo.rose->vl        = 0;
				sk->protinfo.rose->state     = ROSE_STATE_3;
			}
			return 0;

		default:
			return dev_ioctl(cmd, (void *)arg);
	}

	/*NOTREACHED*/
	return 0;
}

static int rose_get_info(char *buffer, char **start, off_t offset, int length)
{
	struct sock *s;
	struct net_device *dev;
	const char *devname, *callsign;
	int len = 0;
	off_t pos = 0;
	off_t begin = 0;

	cli();

	len += sprintf(buffer, "dest_addr  dest_call src_addr   src_call  dev   lci neigh st vs vr va   t  t1  t2  t3  hb    idle Snd-Q Rcv-Q inode\n");

	for (s = rose_list; s != NULL; s = s->next) {
		if ((dev = s->protinfo.rose->device) == NULL)
			devname = "???";
		else
			devname = dev->name;

		len += sprintf(buffer + len, "%-10s %-9s ",
			rose2asc(&s->protinfo.rose->dest_addr),
			ax2asc(&s->protinfo.rose->dest_call));

		if (ax25cmp(&s->protinfo.rose->source_call, &null_ax25_address) == 0)
			callsign = "??????-?";
		else
			callsign = ax2asc(&s->protinfo.rose->source_call);

		len += sprintf(buffer + len, "%-10s %-9s %-5s %3.3X %05d  %d  %d  %d  %d %3lu %3lu %3lu %3lu %3lu %3lu/%03lu %5d %5d %ld\n",
			rose2asc(&s->protinfo.rose->source_addr),
			callsign,
			devname, 
			s->protinfo.rose->lci & 0x0FFF,
			(s->protinfo.rose->neighbour) ? s->protinfo.rose->neighbour->number : 0,
			s->protinfo.rose->state,
			s->protinfo.rose->vs,
			s->protinfo.rose->vr,
			s->protinfo.rose->va,
			ax25_display_timer(&s->protinfo.rose->timer) / HZ,
			s->protinfo.rose->t1 / HZ,
			s->protinfo.rose->t2 / HZ,
			s->protinfo.rose->t3 / HZ,
			s->protinfo.rose->hb / HZ,
			ax25_display_timer(&s->protinfo.rose->idletimer) / (60 * HZ),
			s->protinfo.rose->idle / (60 * HZ),
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

static struct net_proto_family rose_family_ops = {
	family:		PF_ROSE,
	create:		rose_create,
};

static struct proto_ops SOCKOPS_WRAPPED(rose_proto_ops) = {
	family:		PF_ROSE,

	release:	rose_release,
	bind:		rose_bind,
	connect:	rose_connect,
	socketpair:	sock_no_socketpair,
	accept:		rose_accept,
	getname:	rose_getname,
	poll:		datagram_poll,
	ioctl:		rose_ioctl,
	listen:		rose_listen,
	shutdown:	sock_no_shutdown,
	setsockopt:	rose_setsockopt,
	getsockopt:	rose_getsockopt,
	sendmsg:	rose_sendmsg,
	recvmsg:	rose_recvmsg,
	mmap:		sock_no_mmap,
	sendpage:	sock_no_sendpage,
};

#include <linux/smp_lock.h>
SOCKOPS_WRAP(rose_proto, PF_ROSE);

static struct notifier_block rose_dev_notifier = {
	notifier_call:	rose_device_event,
};

static struct net_device *dev_rose;

static const char banner[] = KERN_INFO "F6FBB/G4KLX ROSE for Linux. Version 0.64 for AX25.037 Linux 2.4\n";

static int __init rose_proto_init(void)
{
	int i;

	rose_callsign = null_ax25_address;

	if (rose_ndevs > 0x7FFFFFFF/sizeof(struct net_device)) {
		printk(KERN_ERR "ROSE: rose_proto_init - rose_ndevs parameter to large\n");
		return -1;
	}

	if ((dev_rose = kmalloc(rose_ndevs * sizeof(struct net_device), GFP_KERNEL)) == NULL) {
		printk(KERN_ERR "ROSE: rose_proto_init - unable to allocate device structure\n");
		return -1;
	}

	memset(dev_rose, 0x00, rose_ndevs * sizeof(struct net_device));

	for (i = 0; i < rose_ndevs; i++) {
		sprintf(dev_rose[i].name, "rose%d", i);
		dev_rose[i].init = rose_init;
		register_netdev(&dev_rose[i]);
	}

	sock_register(&rose_family_ops);
	register_netdevice_notifier(&rose_dev_notifier);
	printk(banner);

	ax25_protocol_register(AX25_P_ROSE, rose_route_frame);
	ax25_linkfail_register(rose_link_failed);

#ifdef CONFIG_SYSCTL
	rose_register_sysctl();
#endif
	rose_loopback_init();

	rose_add_loopback_neigh();

	proc_net_create("rose", 0, rose_get_info);
	proc_net_create("rose_neigh", 0, rose_neigh_get_info);
	proc_net_create("rose_nodes", 0, rose_nodes_get_info);
	proc_net_create("rose_routes", 0, rose_routes_get_info);
	return 0;
}
module_init(rose_proto_init);

EXPORT_NO_SYMBOLS;

MODULE_PARM(rose_ndevs, "i");
MODULE_PARM_DESC(rose_ndevs, "number of ROSE devices");

MODULE_AUTHOR("Jonathan Naylor G4KLX <g4klx@g4klx.demon.co.uk>");
MODULE_DESCRIPTION("The amateur radio ROSE network layer protocol");
MODULE_LICENSE("GPL");

static void __exit rose_exit(void)
{
	int i;

	proc_net_remove("rose");
	proc_net_remove("rose_neigh");
	proc_net_remove("rose_nodes");
	proc_net_remove("rose_routes");
	rose_loopback_clear();

	rose_rt_free();

	ax25_protocol_release(AX25_P_ROSE);
	ax25_linkfail_release(rose_link_failed);

	if (ax25cmp(&rose_callsign, &null_ax25_address) != 0)
		ax25_listen_release(&rose_callsign, NULL);

#ifdef CONFIG_SYSCTL
	rose_unregister_sysctl();
#endif
	unregister_netdevice_notifier(&rose_dev_notifier);

	sock_unregister(PF_ROSE);

	for (i = 0; i < rose_ndevs; i++) {
		if (dev_rose[i].priv != NULL) {
			kfree(dev_rose[i].priv);
			dev_rose[i].priv = NULL;
			unregister_netdev(&dev_rose[i]);
		}
	}

	kfree(dev_rose);
}
module_exit(rose_exit);

