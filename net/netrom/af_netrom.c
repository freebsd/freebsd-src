/*
 *	NET/ROM release 007
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
 *	NET/ROM 001	Jonathan(G4KLX)	Cloned from the AX25 code.
 *	NET/ROM 002	Darryl(G7LED)	Fixes and address enhancement.
 *			Jonathan(G4KLX)	Complete bind re-think.
 *			Alan(GW4PTS)	Trivial tweaks into new format.
 *	NET/ROM	003	Jonathan(G4KLX)	Added G8BPQ extensions.
 *					Added NET/ROM routing ioctl.
 *			Darryl(G7LED)	Fix autobinding (on connect).
 *					Fixed nr_release(), set TCP_CLOSE, wakeup app
 *					context, THEN make the sock dead.
 *					Circuit ID check before allocating it on
 *					a connection.
 *			Alan(GW4PTS)	sendmsg/recvmsg only. Fixed connect clear bug
 *					inherited from AX.25
 *	NET/ROM 004	Jonathan(G4KLX)	Converted to module.
 *	NET/ROM 005	Jonathan(G4KLX) Linux 2.1
 *			Alan(GW4PTS)	Started POSIXisms
 *	NET/ROM 006	Alan(GW4PTS)	Brought in line with the ANK changes
 *			Jonathan(G4KLX)	Removed hdrincl.
 *	NET/ROM 007	Jonathan(G4KLX)	New timer architecture.
 *					Impmented Idle timer.
 *			Arnaldo C. Melo s/suser/capable/, micro cleanups
 *			Jeroen(PE1RXQ)	Use sock_orphan() on release.
 *			Tomi(OH2BNS)	Better frame type checking.
 *					Device refcnt fixes.
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
#include <net/ax25.h>
#include <linux/inet.h>
#include <linux/netdevice.h>
#include <linux/if_arp.h>
#include <linux/skbuff.h>
#include <net/sock.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <linux/fcntl.h>
#include <linux/termios.h>	/* For TIOCINQ/OUTQ */
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/notifier.h>
#include <net/netrom.h>
#include <linux/proc_fs.h>
#include <net/ip.h>
#include <net/arp.h>
#include <linux/init.h>

int nr_ndevs = 4;

int sysctl_netrom_default_path_quality            = NR_DEFAULT_QUAL;
int sysctl_netrom_obsolescence_count_initialiser  = NR_DEFAULT_OBS;
int sysctl_netrom_network_ttl_initialiser         = NR_DEFAULT_TTL;
int sysctl_netrom_transport_timeout               = NR_DEFAULT_T1;
int sysctl_netrom_transport_maximum_tries         = NR_DEFAULT_N2;
int sysctl_netrom_transport_acknowledge_delay     = NR_DEFAULT_T2;
int sysctl_netrom_transport_busy_delay            = NR_DEFAULT_T4;
int sysctl_netrom_transport_requested_window_size = NR_DEFAULT_WINDOW;
int sysctl_netrom_transport_no_activity_timeout   = NR_DEFAULT_IDLE;
int sysctl_netrom_routing_control                 = NR_DEFAULT_ROUTING;
int sysctl_netrom_link_fails_count                = NR_DEFAULT_FAILS;

static unsigned short circuit = 0x101;

static struct sock *volatile nr_list;

static struct proto_ops nr_proto_ops;

static void nr_free_sock(struct sock *sk)
{
	sk_free(sk);

	MOD_DEC_USE_COUNT;
}

static struct sock *nr_alloc_sock(void)
{
	struct sock *sk;
	nr_cb *nr;

	if ((sk = sk_alloc(PF_NETROM, GFP_ATOMIC, 1)) == NULL)
		return NULL;

	if ((nr = kmalloc(sizeof(*nr), GFP_ATOMIC)) == NULL) {
		sk_free(sk);
		return NULL;
	}

	MOD_INC_USE_COUNT;

	memset(nr, 0x00, sizeof(*nr));

	sk->protinfo.nr = nr;
	nr->sk = sk;

	return sk;
}

/*
 *	Socket removal during an interrupt is now safe.
 */
static void nr_remove_socket(struct sock *sk)
{
	struct sock *s;
	unsigned long flags;

	save_flags(flags); cli();

	if ((s = nr_list) == sk) {
		nr_list = s->next;
		dev_put(sk->protinfo.nr->device);
		restore_flags(flags);
		return;
	}

	while (s != NULL && s->next != NULL) {
		if (s->next == sk) {
			s->next = sk->next;
			dev_put(sk->protinfo.nr->device);
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
static void nr_kill_by_device(struct net_device *dev)
{
	struct sock *s;

	for (s = nr_list; s != NULL; s = s->next) {
		if (s->protinfo.nr->device == dev)
			nr_disconnect(s, ENETUNREACH);
	}
}

/*
 *	Handle device status changes.
 */
static int nr_device_event(struct notifier_block *this, unsigned long event, void *ptr)
{
	struct net_device *dev = (struct net_device *)ptr;

	if (event != NETDEV_DOWN)
		return NOTIFY_DONE;

	nr_kill_by_device(dev);
	nr_rt_device_down(dev);
	
	return NOTIFY_DONE;
}

/*
 *	Add a socket to the bound sockets list.
 */
static void nr_insert_socket(struct sock *sk)
{
	unsigned long flags;

	save_flags(flags); cli();

	sk->next = nr_list;
	nr_list  = sk;

	restore_flags(flags);
}

/*
 *	Find a socket that wants to accept the Connect Request we just
 *	received.
 */
static struct sock *nr_find_listener(ax25_address *addr)
{
	unsigned long flags;
	struct sock *s;

	save_flags(flags);
	cli();

	for (s = nr_list; s != NULL; s = s->next) {
		if (ax25cmp(&s->protinfo.nr->source_addr, addr) == 0 && s->state == TCP_LISTEN) {
			restore_flags(flags);
			return s;
		}
	}

	restore_flags(flags);
	return NULL;
}

/*
 *	Find a connected NET/ROM socket given my circuit IDs.
 */
static struct sock *nr_find_socket(unsigned char index, unsigned char id)
{
	struct sock *s;
	unsigned long flags;

	save_flags(flags);
	cli();

	for (s = nr_list; s != NULL; s = s->next) {
		if (s->protinfo.nr->my_index == index && s->protinfo.nr->my_id == id) {
			restore_flags(flags);
			return s;
		}
	}

	restore_flags(flags);

	return NULL;
}

/*
 *	Find a connected NET/ROM socket given their circuit IDs.
 */
static struct sock *nr_find_peer(unsigned char index, unsigned char id, ax25_address *dest)
{
	struct sock *s;
	unsigned long flags;

	save_flags(flags);
	cli();

	for (s = nr_list; s != NULL; s = s->next) {
		if (s->protinfo.nr->your_index == index && s->protinfo.nr->your_id == id && ax25cmp(&s->protinfo.nr->dest_addr, dest) == 0) {
			restore_flags(flags);
			return s;
		}
	}

	restore_flags(flags);

	return NULL;
}

/*
 *	Find next free circuit ID.
 */
static unsigned short nr_find_next_circuit(void)
{
	unsigned short id = circuit;
	unsigned char i, j;

	for (;;) {
		i = id / 256;
		j = id % 256;

		if (i != 0 && j != 0)
			if (nr_find_socket(i, j) == NULL)
				break;

		id++;
	}

	return id;
}

/*
 *	Deferred destroy.
 */
void nr_destroy_socket(struct sock *);

/*
 *	Handler for deferred kills.
 */
static void nr_destroy_timer(unsigned long data)
{
	nr_destroy_socket((struct sock *)data);
}

/*
 *	This is called from user mode and the timers. Thus it protects itself against
 *	interrupt users but doesn't worry about being called during work.
 *	Once it is removed from the queue no interrupt or bottom half will
 *	touch it and we are (fairly 8-) ) safe.
 */
void nr_destroy_socket(struct sock *sk)	/* Not static as it's used by the timer */
{
	struct sk_buff *skb;
	unsigned long flags;

	save_flags(flags); cli();

	nr_stop_heartbeat(sk);
	nr_stop_t1timer(sk);
	nr_stop_t2timer(sk);
	nr_stop_t4timer(sk);
	nr_stop_idletimer(sk);

	nr_remove_socket(sk);
	nr_clear_queues(sk);		/* Flush the queues */

	while ((skb = skb_dequeue(&sk->receive_queue)) != NULL) {
		if (skb->sk != sk) {			/* A pending connection */
			skb->sk->dead = 1;	/* Queue the unaccepted socket for death */
			nr_start_heartbeat(skb->sk);
			skb->sk->protinfo.nr->state = NR_STATE_0;
		}

		kfree_skb(skb);
	}

	if (atomic_read(&sk->wmem_alloc) != 0 || atomic_read(&sk->rmem_alloc) != 0) {
		/* Defer: outstanding buffers */
		init_timer(&sk->timer);
		sk->timer.expires  = jiffies + 10 * HZ;
		sk->timer.function = nr_destroy_timer;
		sk->timer.data     = (unsigned long)sk;
		add_timer(&sk->timer);
	} else {
		nr_free_sock(sk);
	}

	restore_flags(flags);
}

/*
 *	Handling for system calls applied via the various interfaces to a
 *	NET/ROM socket object.
 */

static int nr_setsockopt(struct socket *sock, int level, int optname,
	char *optval, int optlen)
{
	struct sock *sk = sock->sk;
	int opt;

	if (level != SOL_NETROM)
		return -ENOPROTOOPT;

	if (optlen < sizeof(int))
		return -EINVAL;

	if (get_user(opt, (int *)optval))
		return -EFAULT;

	switch (optname) {
		case NETROM_T1:
			if (opt < 1)
				return -EINVAL;
			sk->protinfo.nr->t1 = opt * HZ;
			return 0;

		case NETROM_T2:
			if (opt < 1)
				return -EINVAL;
			sk->protinfo.nr->t2 = opt * HZ;
			return 0;

		case NETROM_N2:
			if (opt < 1 || opt > 31)
				return -EINVAL;
			sk->protinfo.nr->n2 = opt;
			return 0;

		case NETROM_T4:
			if (opt < 1)
				return -EINVAL;
			sk->protinfo.nr->t4 = opt * HZ;
			return 0;

		case NETROM_IDLE:
			if (opt < 0)
				return -EINVAL;
			sk->protinfo.nr->idle = opt * 60 * HZ;
			return 0;

		default:
			return -ENOPROTOOPT;
	}
}

static int nr_getsockopt(struct socket *sock, int level, int optname,
	char *optval, int *optlen)
{
	struct sock *sk = sock->sk;
	int val = 0;
	int len; 

	if (level != SOL_NETROM)
		return -ENOPROTOOPT;
	
	if (get_user(len, optlen))
		return -EFAULT;

	if (len < 0)
		return -EINVAL;
		
	switch (optname) {
		case NETROM_T1:
			val = sk->protinfo.nr->t1 / HZ;
			break;

		case NETROM_T2:
			val = sk->protinfo.nr->t2 / HZ;
			break;

		case NETROM_N2:
			val = sk->protinfo.nr->n2;
			break;

		case NETROM_T4:
			val = sk->protinfo.nr->t4 / HZ;
			break;

		case NETROM_IDLE:
			val = sk->protinfo.nr->idle / (60 * HZ);
			break;

		default:
			return -ENOPROTOOPT;
	}

	len = min_t(unsigned int, len, sizeof(int));

	if (put_user(len, optlen))
		return -EFAULT;

	return copy_to_user(optval, &val, len) ? -EFAULT : 0;
}

static int nr_listen(struct socket *sock, int backlog)
{
	struct sock *sk = sock->sk;

	if (sk->state != TCP_LISTEN) {
		memset(&sk->protinfo.nr->user_addr, '\0', AX25_ADDR_LEN);
		sk->max_ack_backlog = backlog;
		sk->state           = TCP_LISTEN;
		return 0;
	}

	return -EOPNOTSUPP;
}

static int nr_create(struct socket *sock, int protocol)
{
	struct sock *sk;
	nr_cb *nr;

	if (sock->type != SOCK_SEQPACKET || protocol != 0)
		return -ESOCKTNOSUPPORT;

	if ((sk = nr_alloc_sock()) == NULL)
		return -ENOMEM;

	nr = sk->protinfo.nr;

	sock_init_data(sock, sk);

	sock->ops    = &nr_proto_ops;
	sk->protocol = protocol;

	skb_queue_head_init(&nr->ack_queue);
	skb_queue_head_init(&nr->reseq_queue);
	skb_queue_head_init(&nr->frag_queue);

	init_timer(&nr->t1timer);
	init_timer(&nr->t2timer);
	init_timer(&nr->t4timer);
	init_timer(&nr->idletimer);

	nr->t1     = sysctl_netrom_transport_timeout;
	nr->t2     = sysctl_netrom_transport_acknowledge_delay;
	nr->n2     = sysctl_netrom_transport_maximum_tries;
	nr->t4     = sysctl_netrom_transport_busy_delay;
	nr->idle   = sysctl_netrom_transport_no_activity_timeout;
	nr->window = sysctl_netrom_transport_requested_window_size;

	nr->bpqext = 1;
	nr->state  = NR_STATE_0;

	return 0;
}

static struct sock *nr_make_new(struct sock *osk)
{
	struct sock *sk;
	nr_cb *nr;

	if (osk->type != SOCK_SEQPACKET)
		return NULL;

	if ((sk = nr_alloc_sock()) == NULL)
		return NULL;

	nr = sk->protinfo.nr;

	sock_init_data(NULL, sk);

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

	skb_queue_head_init(&nr->ack_queue);
	skb_queue_head_init(&nr->reseq_queue);
	skb_queue_head_init(&nr->frag_queue);

	init_timer(&nr->t1timer);
	init_timer(&nr->t2timer);
	init_timer(&nr->t4timer);
	init_timer(&nr->idletimer);

	nr->t1      = osk->protinfo.nr->t1;
	nr->t2      = osk->protinfo.nr->t2;
	nr->n2      = osk->protinfo.nr->n2;
	nr->t4      = osk->protinfo.nr->t4;
	nr->idle    = osk->protinfo.nr->idle;
	nr->window  = osk->protinfo.nr->window;

	nr->device  = osk->protinfo.nr->device;
	nr->bpqext  = osk->protinfo.nr->bpqext;

	return sk;
}

static int nr_release(struct socket *sock)
{
	struct sock *sk = sock->sk;

	if (sk == NULL) return 0;

	switch (sk->protinfo.nr->state) {

		case NR_STATE_0:
		case NR_STATE_1:
		case NR_STATE_2:
			nr_disconnect(sk, 0);
			nr_destroy_socket(sk);
			break;

		case NR_STATE_3:
			nr_clear_queues(sk);
			sk->protinfo.nr->n2count = 0;
			nr_write_internal(sk, NR_DISCREQ);
			nr_start_t1timer(sk);
			nr_stop_t2timer(sk);
			nr_stop_t4timer(sk);
			nr_stop_idletimer(sk);
			sk->protinfo.nr->state   = NR_STATE_2;
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

	sock->sk   = NULL;	

	return 0;
}

static int nr_bind(struct socket *sock, struct sockaddr *uaddr, int addr_len)
{
	struct sock *sk = sock->sk;
	struct full_sockaddr_ax25 *addr = (struct full_sockaddr_ax25 *)uaddr;
	struct net_device *dev;
	ax25_address *user, *source;

	if (sk->zapped == 0)
		return -EINVAL;

	if (addr_len < sizeof(struct sockaddr_ax25) || addr_len > sizeof(struct
full_sockaddr_ax25))
		return -EINVAL;

	if (addr_len < (addr->fsa_ax25.sax25_ndigis * sizeof(ax25_address) + sizeof(struct sockaddr_ax25)))
		return -EINVAL;

	if (addr->fsa_ax25.sax25_family != AF_NETROM)
		return -EINVAL;

	if ((dev = nr_dev_get(&addr->fsa_ax25.sax25_call)) == NULL) {
		SOCK_DEBUG(sk, "NET/ROM: bind failed: invalid node callsign\n");
		return -EADDRNOTAVAIL;
	}

	/*
	 * Only the super user can set an arbitrary user callsign.
	 */
	if (addr->fsa_ax25.sax25_ndigis == 1) {
		if (!capable(CAP_NET_BIND_SERVICE)) {
			dev_put(dev);
			return -EACCES;
		}
		sk->protinfo.nr->user_addr   = addr->fsa_digipeater[0];
		sk->protinfo.nr->source_addr = addr->fsa_ax25.sax25_call;
	} else {
		source = &addr->fsa_ax25.sax25_call;

		if ((user = ax25_findbyuid(current->euid)) == NULL) {
			if (ax25_uid_policy && !capable(CAP_NET_BIND_SERVICE)) {
				dev_put(dev);
				return -EPERM;
			}
			user = source;
		}

		sk->protinfo.nr->user_addr   = *user;
		sk->protinfo.nr->source_addr = *source;
	}

	sk->protinfo.nr->device = dev;
	nr_insert_socket(sk);

	sk->zapped = 0;
	SOCK_DEBUG(sk, "NET/ROM: socket is bound\n");
	return 0;
}

static int nr_connect(struct socket *sock, struct sockaddr *uaddr,
	int addr_len, int flags)
{
	struct sock *sk = sock->sk;
	struct sockaddr_ax25 *addr = (struct sockaddr_ax25 *)uaddr;
	ax25_address *user, *source = NULL;
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

	if (addr_len != sizeof(struct sockaddr_ax25) && addr_len != sizeof(struct full_sockaddr_ax25))
		return -EINVAL;

	if (addr->sax25_family != AF_NETROM)
		return -EINVAL;

	if (sk->zapped) {	/* Must bind first - autobinding in this may or may not work */
		sk->zapped = 0;

		if ((dev = nr_dev_first()) == NULL)
			return -ENETUNREACH;

		source = (ax25_address *)dev->dev_addr;

		if ((user = ax25_findbyuid(current->euid)) == NULL) {
			if (ax25_uid_policy && !capable(CAP_NET_ADMIN)) {
				dev_put(dev);
				return -EPERM;
			}
			user = source;
		}

		sk->protinfo.nr->user_addr   = *user;
		sk->protinfo.nr->source_addr = *source;
		sk->protinfo.nr->device      = dev;

		nr_insert_socket(sk);		/* Finish the bind */
	}

	sk->protinfo.nr->dest_addr = addr->sax25_call;

	circuit = nr_find_next_circuit();

	sk->protinfo.nr->my_index = circuit / 256;
	sk->protinfo.nr->my_id    = circuit % 256;

	circuit++;

	/* Move to connecting socket, start sending Connect Requests */
	sock->state            = SS_CONNECTING;
	sk->state              = TCP_SYN_SENT;

	nr_establish_data_link(sk);

	sk->protinfo.nr->state = NR_STATE_1;

	nr_start_heartbeat(sk);

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

static int nr_accept(struct socket *sock, struct socket *newsock, int flags)
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
	kfree_skb(skb);
	sk->ack_backlog--;
	newsock->sk = newsk;

	return 0;
}

static int nr_getname(struct socket *sock, struct sockaddr *uaddr,
	int *uaddr_len, int peer)
{
	struct full_sockaddr_ax25 *sax = (struct full_sockaddr_ax25 *)uaddr;
	struct sock *sk = sock->sk;

	if (peer != 0) {
		if (sk->state != TCP_ESTABLISHED)
			return -ENOTCONN;
		sax->fsa_ax25.sax25_family = AF_NETROM;
		sax->fsa_ax25.sax25_ndigis = 1;
		sax->fsa_ax25.sax25_call   = sk->protinfo.nr->user_addr;
		sax->fsa_digipeater[0]     = sk->protinfo.nr->dest_addr;
		*uaddr_len = sizeof(struct full_sockaddr_ax25);
	} else {
		sax->fsa_ax25.sax25_family = AF_NETROM;
		sax->fsa_ax25.sax25_ndigis = 0;
		sax->fsa_ax25.sax25_call   = sk->protinfo.nr->source_addr;
		*uaddr_len = sizeof(struct sockaddr_ax25);
	}

	return 0;
}

int nr_rx_frame(struct sk_buff *skb, struct net_device *dev)
{
	struct sock *sk;
	struct sock *make;	
	ax25_address *src, *dest, *user;
	unsigned short circuit_index, circuit_id;
	unsigned short peer_circuit_index, peer_circuit_id;
	unsigned short frametype, flags, window, timeout;

	skb->sk = NULL;		/* Initially we don't know who it's for */

	/*
	 *	skb->data points to the netrom frame start
	 */

	src  = (ax25_address *)(skb->data + 0);
	dest = (ax25_address *)(skb->data + 7);

	circuit_index      = skb->data[15];
	circuit_id         = skb->data[16];
	peer_circuit_index = skb->data[17];
	peer_circuit_id    = skb->data[18];
	frametype          = skb->data[19] & 0x0F;
	flags              = skb->data[19] & 0xF0;

	switch (frametype) {
	case NR_PROTOEXT:
#ifdef CONFIG_INET
		/*
		 * Check for an incoming IP over NET/ROM frame.
		 */
		if (circuit_index == NR_PROTO_IP && circuit_id == NR_PROTO_IP) {
			skb_pull(skb, NR_NETWORK_LEN + NR_TRANSPORT_LEN);
			skb->h.raw = skb->data;

			return nr_rx_ip(skb, dev);
		}
#endif
		return 0;

	case NR_CONNREQ:
	case NR_CONNACK:
	case NR_DISCREQ:
	case NR_DISCACK:
	case NR_INFO:
	case NR_INFOACK:
		/*
		 * These frame types we understand.
		 */
		break;

	default:
		/*
		 * Everything else is ignored.
		 */
		return 0;
	}

	/*
	 * Find an existing socket connection, based on circuit ID, if it's
	 * a Connect Request base it on their circuit ID.
	 *
	 * Circuit ID 0/0 is not valid but it could still be a "reset" for a
	 * circuit that no longer exists at the other end ...
	 */

	sk = NULL;

	if (circuit_index == 0 && circuit_id == 0) {
		if (frametype == NR_CONNACK && flags == NR_CHOKE_FLAG)
			sk = nr_find_peer(peer_circuit_index, peer_circuit_id, src);
	} else {
		if (frametype == NR_CONNREQ)
			sk = nr_find_peer(circuit_index, circuit_id, src);
		else
			sk = nr_find_socket(circuit_index, circuit_id);
	}

	if (sk != NULL) {
		skb->h.raw = skb->data;

		if (frametype == NR_CONNACK && skb->len == 22)
			sk->protinfo.nr->bpqext = 1;
		else
			sk->protinfo.nr->bpqext = 0;

		return nr_process_rx_frame(sk, skb);
	}

	/*
	 * Now it should be a CONNREQ.
	 */
	if (frametype != NR_CONNREQ) {
		/*
		 * Here it would be nice to be able to send a reset but
		 * NET/ROM doesn't have one. The following hack would
		 * have been a way to extend the protocol but apparently
		 * it kills BPQ boxes... :-(
		 */
#if 0
		/*
		 * Never reply to a CONNACK/CHOKE.
		 */
		if (frametype != NR_CONNACK || flags != NR_CHOKE_FLAG)
			nr_transmit_refusal(skb, 1);
#endif
		return 0;
	}

	sk = nr_find_listener(dest);

	user = (ax25_address *)(skb->data + 21);

	if (sk == NULL || sk->ack_backlog == sk->max_ack_backlog || (make = nr_make_new(sk)) == NULL) {
		nr_transmit_refusal(skb, 0);
		return 0;
	}

	window = skb->data[20];

	skb->sk             = make;
	make->state         = TCP_ESTABLISHED;

	/* Fill in his circuit details */
	make->protinfo.nr->source_addr = *dest;
	make->protinfo.nr->dest_addr   = *src;
	make->protinfo.nr->user_addr   = *user;

	make->protinfo.nr->your_index  = circuit_index;
	make->protinfo.nr->your_id     = circuit_id;

	circuit = nr_find_next_circuit();

	make->protinfo.nr->my_index    = circuit / 256;
	make->protinfo.nr->my_id       = circuit % 256;

	circuit++;

	/* Window negotiation */
	if (window < make->protinfo.nr->window)
		make->protinfo.nr->window = window;

	/* L4 timeout negotiation */
	if (skb->len == 37) {
		timeout = skb->data[36] * 256 + skb->data[35];
		if (timeout * HZ < make->protinfo.nr->t1)
			make->protinfo.nr->t1 = timeout * HZ;
		make->protinfo.nr->bpqext = 1;
	} else {
		make->protinfo.nr->bpqext = 0;
	}

	nr_write_internal(make, NR_CONNACK);

	make->protinfo.nr->condition = 0x00;
	make->protinfo.nr->vs        = 0;
	make->protinfo.nr->va        = 0;
	make->protinfo.nr->vr        = 0;
	make->protinfo.nr->vl        = 0;
	make->protinfo.nr->state     = NR_STATE_3;
	sk->ack_backlog++;
	make->pair = sk;

	dev_hold(make->protinfo.nr->device);

	nr_insert_socket(make);

	skb_queue_head(&sk->receive_queue, skb);

	nr_start_heartbeat(make);
	nr_start_idletimer(make);

	if (!sk->dead)
		sk->data_ready(sk, skb->len);

	return 1;
}

static int nr_sendmsg(struct socket *sock, struct msghdr *msg, int len, struct scm_cookie *scm)
{
	struct sock *sk = sock->sk;
	struct sockaddr_ax25 *usax = (struct sockaddr_ax25 *)msg->msg_name;
	int err;
	struct sockaddr_ax25 sax;
	struct sk_buff *skb;
	unsigned char *asmptr;
	int size;

	if (msg->msg_flags & ~(MSG_DONTWAIT|MSG_EOR))
		return -EINVAL;

	if (sk->zapped)
		return -EADDRNOTAVAIL;

	if (sk->shutdown & SEND_SHUTDOWN) {
		send_sig(SIGPIPE, current, 0);
		return -EPIPE;
	}

	if (sk->protinfo.nr->device == NULL)
		return -ENETUNREACH;

	if (usax) {
		if (msg->msg_namelen < sizeof(sax))
			return -EINVAL;
		sax = *usax;
		if (ax25cmp(&sk->protinfo.nr->dest_addr, &sax.sax25_call) != 0)
			return -EISCONN;
		if (sax.sax25_family != AF_NETROM)
			return -EINVAL;
	} else {
		if (sk->state != TCP_ESTABLISHED)
			return -ENOTCONN;
		sax.sax25_family = AF_NETROM;
		sax.sax25_call   = sk->protinfo.nr->dest_addr;
	}

	SOCK_DEBUG(sk, "NET/ROM: sendto: Addresses built.\n");

	/* Build a packet */
	SOCK_DEBUG(sk, "NET/ROM: sendto: building packet.\n");
	size = len + AX25_BPQ_HEADER_LEN + AX25_MAX_HEADER_LEN + NR_NETWORK_LEN + NR_TRANSPORT_LEN;

	if ((skb = sock_alloc_send_skb(sk, size, msg->msg_flags & MSG_DONTWAIT, &err)) == NULL)
		return err;

	skb_reserve(skb, size - len);

	/*
	 *	Push down the NET/ROM header
	 */

	asmptr = skb_push(skb, NR_TRANSPORT_LEN);
	SOCK_DEBUG(sk, "Building NET/ROM Header.\n");

	/* Build a NET/ROM Transport header */

	*asmptr++ = sk->protinfo.nr->your_index;
	*asmptr++ = sk->protinfo.nr->your_id;
	*asmptr++ = 0;		/* To be filled in later */
	*asmptr++ = 0;		/*      Ditto            */
	*asmptr++ = NR_INFO;
	SOCK_DEBUG(sk, "Built header.\n");

	/*
	 *	Put the data on the end
	 */

	skb->h.raw = skb_put(skb, len);

	asmptr = skb->h.raw;
	SOCK_DEBUG(sk, "NET/ROM: Appending user data\n");

	/* User data follows immediately after the NET/ROM transport header */
	memcpy_fromiovec(asmptr, msg->msg_iov, len);
	SOCK_DEBUG(sk, "NET/ROM: Transmitting buffer\n");

	if (sk->state != TCP_ESTABLISHED) {
		kfree_skb(skb);
		return -ENOTCONN;
	}

	nr_output(sk, skb);	/* Shove it onto the queue */

	return len;
}

static int nr_recvmsg(struct socket *sock, struct msghdr *msg, int size, 
	int flags, struct scm_cookie *scm)
{
	struct sock *sk = sock->sk;
	struct sockaddr_ax25 *sax = (struct sockaddr_ax25 *)msg->msg_name;
	int copied;
	struct sk_buff *skb;
	int er;

	/*
	 * This works for seqpacket too. The receiver has ordered the queue for
	 * us! We do one quick check first though
	 */

	if (sk->state != TCP_ESTABLISHED)
		return -ENOTCONN;

	/* Now we can treat all alike */
	if ((skb = skb_recv_datagram(sk, flags & ~MSG_DONTWAIT, flags & MSG_DONTWAIT, &er)) == NULL)
		return er;

	skb->h.raw = skb->data;
	copied     = skb->len;

	if (copied > size) {
		copied = size;
		msg->msg_flags |= MSG_TRUNC;
	}

	skb_copy_datagram_iovec(skb, 0, msg->msg_iov, copied);

	if (sax != NULL) {
		sax->sax25_family = AF_NETROM;
		memcpy(sax->sax25_call.ax25_call, skb->data + 7, AX25_ADDR_LEN);
	}

	msg->msg_namelen = sizeof(*sax);

	skb_free_datagram(sk, skb);

	return copied;
}


static int nr_ioctl(struct socket *sock, unsigned int cmd, unsigned long arg)
{
	struct sock *sk = sock->sk;

	switch (cmd) {
		case TIOCOUTQ: {
			long amount;
			amount = sk->sndbuf - atomic_read(&sk->wmem_alloc);
			if (amount < 0)
				amount = 0;
			return put_user(amount, (int *)arg);
		}

		case TIOCINQ: {
			struct sk_buff *skb;
			long amount = 0L;
			/* These two are safe on a single CPU system as only user tasks fiddle here */
			if ((skb = skb_peek(&sk->receive_queue)) != NULL)
				amount = skb->len;
			return put_user(amount, (int *)arg);
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
		case SIOCNRDECOBS:
			if (!capable(CAP_NET_ADMIN)) return -EPERM;
			return nr_rt_ioctl(cmd, (void *)arg);

 		default:
			return dev_ioctl(cmd, (void *)arg);
	}

	/*NOTREACHED*/
	return 0;
}

static int nr_get_info(char *buffer, char **start, off_t offset, int length)
{
	struct sock *s;
	struct net_device *dev;
	const char *devname;
	int len = 0;
	off_t pos = 0;
	off_t begin = 0;

	cli();

	len += sprintf(buffer, "user_addr dest_node src_node  dev    my  your  st  vs  vr  va    t1     t2     t4      idle   n2  wnd Snd-Q Rcv-Q inode\n");

	for (s = nr_list; s != NULL; s = s->next) {
		if ((dev = s->protinfo.nr->device) == NULL)
			devname = "???";
		else
			devname = dev->name;

		len += sprintf(buffer + len, "%-9s ",
			ax2asc(&s->protinfo.nr->user_addr));
		len += sprintf(buffer + len, "%-9s ",
			ax2asc(&s->protinfo.nr->dest_addr));
		len += sprintf(buffer + len, "%-9s %-3s  %02X/%02X %02X/%02X %2d %3d %3d %3d %3lu/%03lu %2lu/%02lu %3lu/%03lu %3lu/%03lu %2d/%02d %3d %5d %5d %ld\n",
			ax2asc(&s->protinfo.nr->source_addr),
			devname,
			s->protinfo.nr->my_index,
			s->protinfo.nr->my_id,
			s->protinfo.nr->your_index,
			s->protinfo.nr->your_id,
			s->protinfo.nr->state,
			s->protinfo.nr->vs,
			s->protinfo.nr->vr,
			s->protinfo.nr->va,
			ax25_display_timer(&s->protinfo.nr->t1timer) / HZ,
			s->protinfo.nr->t1 / HZ,
			ax25_display_timer(&s->protinfo.nr->t2timer) / HZ,
			s->protinfo.nr->t2 / HZ,
			ax25_display_timer(&s->protinfo.nr->t4timer) / HZ,
			s->protinfo.nr->t4 / HZ,
			ax25_display_timer(&s->protinfo.nr->idletimer) / (60 * HZ),
			s->protinfo.nr->idle / (60 * HZ),
			s->protinfo.nr->n2count,
			s->protinfo.nr->n2,
			s->protinfo.nr->window,
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

static struct net_proto_family nr_family_ops = {
	family:		PF_NETROM,
	create:		nr_create,
};

static struct proto_ops SOCKOPS_WRAPPED(nr_proto_ops) = {
	family:		PF_NETROM,

	release:	nr_release,
	bind:		nr_bind,
	connect:	nr_connect,
	socketpair:	sock_no_socketpair,
	accept:		nr_accept,
	getname:	nr_getname,
	poll:		datagram_poll,
	ioctl:		nr_ioctl,
	listen:		nr_listen,
	shutdown:	sock_no_shutdown,
	setsockopt:	nr_setsockopt,
	getsockopt:	nr_getsockopt,
	sendmsg:	nr_sendmsg,
	recvmsg:	nr_recvmsg,
	mmap:		sock_no_mmap,
	sendpage:	sock_no_sendpage,
};

#include <linux/smp_lock.h>
SOCKOPS_WRAP(nr_proto, PF_NETROM);

static struct notifier_block nr_dev_notifier = {
	notifier_call:	nr_device_event,
};

static struct net_device *dev_nr;

static char banner[] __initdata = KERN_INFO "G4KLX NET/ROM for Linux. Version 0.7 for AX25.037 Linux 2.4\n";

static int __init nr_proto_init(void)
{
	int i;

	if (nr_ndevs > 0x7fffffff/sizeof(struct net_device)) {
		printk(KERN_ERR "NET/ROM: nr_proto_init - nr_ndevs parameter to large\n");
		return -1;
	}

	if ((dev_nr = kmalloc(nr_ndevs * sizeof(struct net_device), GFP_KERNEL)) == NULL) {
		printk(KERN_ERR "NET/ROM: nr_proto_init - unable to allocate device structure\n");
		return -1;
	}

	memset(dev_nr, 0x00, nr_ndevs * sizeof(struct net_device));

	for (i = 0; i < nr_ndevs; i++) {
		sprintf(dev_nr[i].name, "nr%d", i);
		dev_nr[i].init = nr_init;
		register_netdev(&dev_nr[i]);
	}

	sock_register(&nr_family_ops);
	register_netdevice_notifier(&nr_dev_notifier);
	printk(banner);

	ax25_protocol_register(AX25_P_NETROM, nr_route_frame);
	ax25_linkfail_register(nr_link_failed);

#ifdef CONFIG_SYSCTL
	nr_register_sysctl();
#endif

	nr_loopback_init();

	proc_net_create("nr", 0, nr_get_info);
	proc_net_create("nr_neigh", 0, nr_neigh_get_info);
	proc_net_create("nr_nodes", 0, nr_nodes_get_info);
	return 0;
}

module_init(nr_proto_init);


EXPORT_NO_SYMBOLS;

MODULE_PARM(nr_ndevs, "i");
MODULE_PARM_DESC(nr_ndevs, "number of NET/ROM devices");

MODULE_AUTHOR("Jonathan Naylor G4KLX <g4klx@g4klx.demon.co.uk>");
MODULE_DESCRIPTION("The amateur radio NET/ROM network and transport layer protocol");
MODULE_LICENSE("GPL");

static void __exit nr_exit(void)
{
	int i;

	proc_net_remove("nr");
	proc_net_remove("nr_neigh");
	proc_net_remove("nr_nodes");
	nr_loopback_clear();

	nr_rt_free();

	ax25_protocol_release(AX25_P_NETROM);
	ax25_linkfail_release(nr_link_failed);

	unregister_netdevice_notifier(&nr_dev_notifier);

#ifdef CONFIG_SYSCTL
	nr_unregister_sysctl();
#endif
	sock_unregister(PF_NETROM);

	for (i = 0; i < nr_ndevs; i++) {
		if (dev_nr[i].priv != NULL) {
			kfree(dev_nr[i].priv);
			dev_nr[i].priv = NULL;
			unregister_netdev(&dev_nr[i]);
		}
	}

	kfree(dev_nr);
}
module_exit(nr_exit);
