/*
 *	AX.25 release 038
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
 *	AX.25 006	Alan(GW4PTS)		Nearly died of shock - it's working 8-)
 *	AX.25 007	Alan(GW4PTS)		Removed the silliest bugs
 *	AX.25 008	Alan(GW4PTS)		Cleaned up, fixed a few state machine problems, added callbacks
 *	AX.25 009	Alan(GW4PTS)		Emergency patch kit to fix memory corruption
 * 	AX.25 010	Alan(GW4PTS)		Added RAW sockets/Digipeat.
 *	AX.25 011	Alan(GW4PTS)		RAW socket and datagram fixes (thanks) - Raw sendto now gets PID right
 *						datagram sendto uses correct target address.
 *	AX.25 012	Alan(GW4PTS)		Correct incoming connection handling, send DM to failed connects.
 *						Use skb->data not skb+1. Support sk->priority correctly.
 *						Correct receive on SOCK_DGRAM.
 *	AX.25 013	Alan(GW4PTS)		Send DM to all unknown frames, missing initialiser fixed
 *						Leave spare SSID bits set (DAMA etc) - thanks for bug report,
 *						removed device registration (it's not used or needed). Clean up for
 *						gcc 2.5.8. PID to AX25_P_
 *	AX.25 014	Alan(GW4PTS)		Cleanup and NET3 merge
 *	AX.25 015	Alan(GW4PTS)		Internal test version.
 *	AX.25 016	Alan(GW4PTS)		Semi Internal version for PI card
 *						work.
 *	AX.25 017	Alan(GW4PTS)		Fixed some small bugs reported by
 *						G4KLX
 *	AX.25 018	Alan(GW4PTS)		Fixed a small error in SOCK_DGRAM
 *	AX.25 019	Alan(GW4PTS)		Clean ups for the non INET kernel and device ioctls in AX.25
 *	AX.25 020	Jonathan(G4KLX)		/proc support and other changes.
 *	AX.25 021	Alan(GW4PTS)		Added AX25_T1, AX25_N2, AX25_T3 as requested.
 *	AX.25 022	Jonathan(G4KLX)		More work on the ax25 auto router and /proc improved (again)!
 *			Alan(GW4PTS)		Added TIOCINQ/OUTQ
 *	AX.25 023	Alan(GW4PTS)		Fixed shutdown bug
 *	AX.25 023	Alan(GW4PTS)		Linus changed timers
 *	AX.25 024	Alan(GW4PTS)		Small bug fixes
 *	AX.25 025	Alan(GW4PTS)		More fixes, Linux 1.1.51 compatibility stuff, timers again!
 *	AX.25 026	Alan(GW4PTS)		Small state fix.
 *	AX.25 027	Alan(GW4PTS)		Socket close crash fixes.
 *	AX.25 028	Alan(GW4PTS)		Callsign control including settings per uid.
 *						Small bug fixes.
 *						Protocol set by sockets only.
 *						Small changes to allow for start of NET/ROM layer.
 *	AX.25 028a	Jonathan(G4KLX)		Changes to state machine.
 *	AX.25 028b	Jonathan(G4KLX)		Extracted ax25 control block
 *						from sock structure.
 *	AX.25 029	Alan(GW4PTS)		Combined 028b and some KA9Q code
 *			Jonathan(G4KLX)		and removed all the old Berkeley, added IP mode registration.
 *			Darryl(G7LED)		stuff. Cross-port digipeating. Minor fixes and enhancements.
 *			Alan(GW4PTS)		Missed suser() on axassociate checks
 *	AX.25 030	Alan(GW4PTS)		Added variable length headers.
 *			Jonathan(G4KLX)		Added BPQ Ethernet interface.
 *			Steven(GW7RRM)		Added digi-peating control ioctl.
 *						Added extended AX.25 support.
 *						Added AX.25 frame segmentation.
 *			Darryl(G7LED)		Changed connect(), recvfrom(), sendto() sockaddr/addrlen to
 *						fall inline with bind() and new policy.
 *						Moved digipeating ctl to new ax25_dev structs.
 *						Fixed ax25_release(), set TCP_CLOSE, wakeup app
 *						context, THEN make the sock dead.
 *			Alan(GW4PTS)		Cleaned up for single recvmsg methods.
 *			Alan(GW4PTS)		Fixed not clearing error on connect failure.
 *	AX.25 031	Jonathan(G4KLX)		Added binding to any device.
 *			Joerg(DL1BKE)		Added DAMA support, fixed (?) digipeating, fixed buffer locking
 *						for "virtual connect" mode... Result: Probably the
 *						"Most Buggiest Code You've Ever Seen" (TM)
 *			HaJo(DD8NE)		Implementation of a T5 (idle) timer
 *			Joerg(DL1BKE)		Renamed T5 to IDLE and changed behaviour:
 *						the timer gets reloaded on every received or transmitted
 *						I frame for IP or NETROM. The idle timer is not active
 *						on "vanilla AX.25" connections. Furthermore added PACLEN
 *						to provide AX.25-layer based fragmentation (like WAMPES)
 *      AX.25 032	Joerg(DL1BKE)		Fixed DAMA timeout error.
 *						ax25_send_frame() limits the number of enqueued
 *						datagrams per socket.
 *	AX.25 033	Jonathan(G4KLX)		Removed auto-router.
 *			Hans(PE1AYX)		Converted to Module.
 *			Joerg(DL1BKE)		Moved BPQ Ethernet to separate driver.
 *	AX.25 034	Jonathan(G4KLX)		2.1 changes
 *			Alan(GW4PTS)		Small POSIXisations
 *	AX.25 035	Alan(GW4PTS)		Started fixing to the new
 *						format.
 *			Hans(PE1AYX)		Fixed interface to IP layer.
 *			Alan(GW4PTS)		Added asynchronous support.
 *			Frederic(F1OAT)		Support for pseudo-digipeating.
 *			Jonathan(G4KLX)		Support for packet forwarding.
 *	AX.25 036	Jonathan(G4KLX)		Major restructuring.
 *			Joerg(DL1BKE)		Fixed DAMA Slave.
 *			Jonathan(G4KLX)		Fix wildcard listen parameter setting.
 *	AX.25 037	Jonathan(G4KLX)		New timer architecture.
 *      AX.25 038       Matthias(DG2FEF)        Small fixes to the syscall interface to make kernel
 *                                              independent of AX25_MAX_DIGIS used by applications.
 *                      Tomi(OH2BNS)            Fixed ax25_getname().
 *			Joerg(DL1BKE)		Starting to phase out the support for full_sockaddr_ax25
 *						with only 6 digipeaters and sockaddr_ax25 in ax25_bind(),
 *						ax25_connect() and ax25_sendmsg()
 *			Joerg(DL1BKE)		Added support for SO_BINDTODEVICE
 *			Arnaldo C. Melo		s/suser/capable(CAP_NET_ADMIN)/, some more cleanups
 *			Michal Ostrowski	Module initialization cleanup.
 *			Jeroen(PE1RXQ)		Use sock_orphan() on release.
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
#include <linux/proc_fs.h>
#include <linux/stat.h>
#include <linux/netfilter.h>
#include <linux/sysctl.h>
#include <linux/init.h>
#include <net/ip.h>
#include <net/arp.h>



ax25_cb *volatile ax25_list;

static struct proto_ops ax25_proto_ops;

/*
 *	Free an allocated ax25 control block. This is done to centralise
 *	the MOD count code.
 */
void ax25_free_cb(ax25_cb *ax25)
{
	if (ax25->digipeat != NULL) {
		kfree(ax25->digipeat);
		ax25->digipeat = NULL;
	}

	kfree(ax25);

	MOD_DEC_USE_COUNT;
}

static void ax25_free_sock(struct sock *sk)
{
	ax25_free_cb(sk->protinfo.ax25);
}

/*
 *	Socket removal during an interrupt is now safe.
 */
static void ax25_remove_socket(ax25_cb *ax25)
{
	ax25_cb *s;
	unsigned long flags;

	save_flags(flags); cli();

	if ((s = ax25_list) == ax25) {
		ax25_list = s->next;
		restore_flags(flags);
		return;
	}

	while (s != NULL && s->next != NULL) {
		if (s->next == ax25) {
			s->next = ax25->next;
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
static void ax25_kill_by_device(struct net_device *dev)
{
	ax25_dev *ax25_dev;
	ax25_cb *s;

	if ((ax25_dev = ax25_dev_ax25dev(dev)) == NULL)
		return;

	for (s = ax25_list; s != NULL; s = s->next) {
		if (s->ax25_dev == ax25_dev) {
			s->ax25_dev = NULL;
			ax25_disconnect(s, ENETUNREACH);
		}
	}
}

/*
 *	Handle device status changes.
 */
static int ax25_device_event(struct notifier_block *this,unsigned long event, void *ptr)
{
	struct net_device *dev = (struct net_device *)ptr;

	/* Reject non AX.25 devices */
	if (dev->type != ARPHRD_AX25)
		return NOTIFY_DONE;

	switch (event) {
		case NETDEV_UP:
			ax25_dev_device_up(dev);
			break;
		case NETDEV_DOWN:
			ax25_kill_by_device(dev);
			ax25_rt_device_down(dev);
			ax25_dev_device_down(dev);
			break;
		default:
			break;
	}

	return NOTIFY_DONE;
}

/*
 *	Add a socket to the bound sockets list.
 */
void ax25_insert_socket(ax25_cb *ax25)
{
	unsigned long flags;

	save_flags(flags);
	cli();

	ax25->next = ax25_list;
	ax25_list  = ax25;

	restore_flags(flags);
}

/*
 *	Find a socket that wants to accept the SABM we have just
 *	received.
 */
struct sock *ax25_find_listener(ax25_address *addr, int digi, struct net_device *dev, int type)
{
	unsigned long flags;
	ax25_cb *s;

	save_flags(flags);
	cli();

	for (s = ax25_list; s != NULL; s = s->next) {
		if ((s->iamdigi && !digi) || (!s->iamdigi && digi))
			continue;
		if (s->sk != NULL && ax25cmp(&s->source_addr, addr) == 0 && s->sk->type == type && s->sk->state == TCP_LISTEN) {
			/* If device is null we match any device */
			if (s->ax25_dev == NULL || s->ax25_dev->dev == dev) {
				restore_flags(flags);
				return s->sk;
			}
		}
	}

	restore_flags(flags);
	return NULL;
}

/*
 *	Find an AX.25 socket given both ends.
 */
struct sock *ax25_find_socket(ax25_address *my_addr, ax25_address *dest_addr, int type)
{
	ax25_cb *s;
	unsigned long flags;

	save_flags(flags);
	cli();

	for (s = ax25_list; s != NULL; s = s->next) {
		if (s->sk != NULL && ax25cmp(&s->source_addr, my_addr) == 0 && ax25cmp(&s->dest_addr, dest_addr) == 0 && s->sk->type == type) {
			restore_flags(flags);
			return s->sk;
		}
	}

	restore_flags(flags);

	return NULL;
}

/*
 *	Find an AX.25 control block given both ends. It will only pick up
 *	floating AX.25 control blocks or non Raw socket bound control blocks.
 */
ax25_cb *ax25_find_cb(ax25_address *src_addr, ax25_address *dest_addr, ax25_digi *digi, struct net_device *dev)
{
	ax25_cb *s;
	unsigned long flags;

	save_flags(flags);
	cli();

	for (s = ax25_list; s != NULL; s = s->next) {
		if (s->sk != NULL && s->sk->type != SOCK_SEQPACKET)
			continue;
		if (s->ax25_dev == NULL)
			continue;
		if (ax25cmp(&s->source_addr, src_addr) == 0 && ax25cmp(&s->dest_addr, dest_addr) == 0 && s->ax25_dev->dev == dev) {
			if (digi != NULL && digi->ndigi != 0) {
				if (s->digipeat == NULL)
					continue;
				if (ax25digicmp(s->digipeat, digi) != 0)
					continue;
			} else {
				if (s->digipeat != NULL && s->digipeat->ndigi != 0)
					continue;
			}
			restore_flags(flags);
			return s;
		}
	}

	restore_flags(flags);

	return NULL;
}

/*
 *	Look for any matching address - RAW sockets can bind to arbitrary names
 */
struct sock *ax25_addr_match(ax25_address *addr)
{
	unsigned long flags;
	ax25_cb *s;

	save_flags(flags);
	cli();

	for (s = ax25_list; s != NULL; s = s->next) {
		if (s->sk != NULL && ax25cmp(&s->source_addr, addr) == 0 && s->sk->type == SOCK_RAW) {
			restore_flags(flags);
			return s->sk;
		}
	}

	restore_flags(flags);

	return NULL;
}

void ax25_send_to_raw(struct sock *sk, struct sk_buff *skb, int proto)
{
	struct sk_buff *copy;

	while (sk != NULL) {
		if (sk->type == SOCK_RAW &&
		    sk->protocol == proto &&
		    atomic_read(&sk->rmem_alloc) <= sk->rcvbuf) {
			if ((copy = skb_clone(skb, GFP_ATOMIC)) == NULL)
				return;

			if (sock_queue_rcv_skb(sk, copy) != 0)
				kfree_skb(copy);
		}

		sk = sk->next;
	}
}

/*
 *	Deferred destroy.
 */
void ax25_destroy_socket(ax25_cb *);

/*
 *	Handler for deferred kills.
 */
static void ax25_destroy_timer(unsigned long data)
{
	ax25_destroy_socket((ax25_cb *)data);
}

/*
 *	This is called from user mode and the timers. Thus it protects itself against
 *	interrupt users but doesn't worry about being called during work.
 *	Once it is removed from the queue no interrupt or bottom half will
 *	touch it and we are (fairly 8-) ) safe.
 */
void ax25_destroy_socket(ax25_cb *ax25)	/* Not static as it's used by the timer */
{
	struct sk_buff *skb;
	unsigned long flags;

	save_flags(flags); cli();

	ax25_stop_heartbeat(ax25);
	ax25_stop_t1timer(ax25);
	ax25_stop_t2timer(ax25);
	ax25_stop_t3timer(ax25);
	ax25_stop_idletimer(ax25);

	ax25_remove_socket(ax25);
	ax25_clear_queues(ax25);	/* Flush the queues */

	if (ax25->sk != NULL) {
		while ((skb = skb_dequeue(&ax25->sk->receive_queue)) != NULL) {
			if (skb->sk != ax25->sk) {			/* A pending connection */
				skb->sk->dead = 1;	/* Queue the unaccepted socket for death */
				ax25_start_heartbeat(skb->sk->protinfo.ax25);
				skb->sk->protinfo.ax25->state = AX25_STATE_0;
			}

			kfree_skb(skb);
		}
	}

	if (ax25->sk != NULL) {
		if (atomic_read(&ax25->sk->wmem_alloc) != 0 ||
		    atomic_read(&ax25->sk->rmem_alloc) != 0) {
			/* Defer: outstanding buffers */
			init_timer(&ax25->timer);
			ax25->timer.expires  = jiffies + 10 * HZ;
			ax25->timer.function = ax25_destroy_timer;
			ax25->timer.data     = (unsigned long)ax25;
			add_timer(&ax25->timer);
		} else {
			sk_free(ax25->sk);
		}
	} else {
		ax25_free_cb(ax25);
	}

	restore_flags(flags);
}

/*
 * dl1bke 960311: set parameters for existing AX.25 connections,
 *		  includes a KILL command to abort any connection.
 *		  VERY useful for debugging ;-)
 */
static int ax25_ctl_ioctl(const unsigned int cmd, void *arg)
{
	struct ax25_ctl_struct ax25_ctl;
	ax25_digi digi;
	ax25_dev *ax25_dev;
	ax25_cb *ax25;
	unsigned int k;

	if (copy_from_user(&ax25_ctl, arg, sizeof(ax25_ctl)))
		return -EFAULT;

	if ((ax25_dev = ax25_addr_ax25dev(&ax25_ctl.port_addr)) == NULL)
		return -ENODEV;

	if (ax25_ctl.digi_count > AX25_MAX_DIGIS)
		return -EINVAL;

	digi.ndigi = ax25_ctl.digi_count;
	for (k = 0; k < digi.ndigi; k++)
		digi.calls[k] = ax25_ctl.digi_addr[k];

	if ((ax25 = ax25_find_cb(&ax25_ctl.source_addr, &ax25_ctl.dest_addr, &digi, ax25_dev->dev)) == NULL)
		return -ENOTCONN;

	switch (ax25_ctl.cmd) {
		case AX25_KILL:
			ax25_send_control(ax25, AX25_DISC, AX25_POLLON, AX25_COMMAND);
#ifdef CONFIG_AX25_DAMA_SLAVE
			if (ax25_dev->dama.slave && ax25->ax25_dev->values[AX25_VALUES_PROTOCOL] == AX25_PROTO_DAMA_SLAVE)
				ax25_dama_off(ax25);
#endif
			ax25_disconnect(ax25, ENETRESET);
			break;

	  	case AX25_WINDOW:
	  		if (ax25->modulus == AX25_MODULUS) {
	  			if (ax25_ctl.arg < 1 || ax25_ctl.arg > 7)
	  				return -EINVAL;
	  		} else {
	  			if (ax25_ctl.arg < 1 || ax25_ctl.arg > 63)
	  				return -EINVAL;
	  		}
	  		ax25->window = ax25_ctl.arg;
	  		break;

	  	case AX25_T1:
  			if (ax25_ctl.arg < 1)
  				return -EINVAL;
  			ax25->rtt = (ax25_ctl.arg * HZ) / 2;
  			ax25->t1  = ax25_ctl.arg * HZ;
  			break;

	  	case AX25_T2:
	  		if (ax25_ctl.arg < 1)
	  			return -EINVAL;
	  		ax25->t2 = ax25_ctl.arg * HZ;
	  		break;

	  	case AX25_N2:
	  		if (ax25_ctl.arg < 1 || ax25_ctl.arg > 31)
	  			return -EINVAL;
	  		ax25->n2count = 0;
	  		ax25->n2 = ax25_ctl.arg;
	  		break;

	  	case AX25_T3:
	  		if (ax25_ctl.arg < 0)
	  			return -EINVAL;
	  		ax25->t3 = ax25_ctl.arg * HZ;
	  		break;

	  	case AX25_IDLE:
	  		if (ax25_ctl.arg < 0)
	  			return -EINVAL;
	  		ax25->idle = ax25_ctl.arg * 60 * HZ;
	  		break;

	  	case AX25_PACLEN:
	  		if (ax25_ctl.arg < 16 || ax25_ctl.arg > 65535)
	  			return -EINVAL;
	  		ax25->paclen = ax25_ctl.arg;
	  		break;

	  	default:
	  		return -EINVAL;
	  }

	  return 0;
}

/*
 *	Fill in a created AX.25 created control block with the default
 *	values for a particular device.
 */
void ax25_fillin_cb(ax25_cb *ax25, ax25_dev *ax25_dev)
{
	ax25->ax25_dev = ax25_dev;

	if (ax25->ax25_dev != NULL) {
		ax25->rtt     = ax25_dev->values[AX25_VALUES_T1] / 2;
		ax25->t1      = ax25_dev->values[AX25_VALUES_T1];
		ax25->t2      = ax25_dev->values[AX25_VALUES_T2];
		ax25->t3      = ax25_dev->values[AX25_VALUES_T3];
		ax25->n2      = ax25_dev->values[AX25_VALUES_N2];
		ax25->paclen  = ax25_dev->values[AX25_VALUES_PACLEN];
		ax25->idle    = ax25_dev->values[AX25_VALUES_IDLE];
		ax25->backoff = ax25_dev->values[AX25_VALUES_BACKOFF];

		if (ax25_dev->values[AX25_VALUES_AXDEFMODE]) {
			ax25->modulus = AX25_EMODULUS;
			ax25->window  = ax25_dev->values[AX25_VALUES_EWINDOW];
		} else {
			ax25->modulus = AX25_MODULUS;
			ax25->window  = ax25_dev->values[AX25_VALUES_WINDOW];
		}
	} else {
		ax25->rtt     = AX25_DEF_T1 / 2;
		ax25->t1      = AX25_DEF_T1;
		ax25->t2      = AX25_DEF_T2;
		ax25->t3      = AX25_DEF_T3;
		ax25->n2      = AX25_DEF_N2;
		ax25->paclen  = AX25_DEF_PACLEN;
		ax25->idle    = AX25_DEF_IDLE;
		ax25->backoff = AX25_DEF_BACKOFF;

		if (AX25_DEF_AXDEFMODE) {
			ax25->modulus = AX25_EMODULUS;
			ax25->window  = AX25_DEF_EWINDOW;
		} else {
			ax25->modulus = AX25_MODULUS;
			ax25->window  = AX25_DEF_WINDOW;
		}
	}
}

/*
 * Create an empty AX.25 control block.
 */
ax25_cb *ax25_create_cb(void)
{
	ax25_cb *ax25;

	if ((ax25 = kmalloc(sizeof(*ax25), GFP_ATOMIC)) == NULL)
		return NULL;

	MOD_INC_USE_COUNT;

	memset(ax25, 0x00, sizeof(*ax25));

	skb_queue_head_init(&ax25->write_queue);
	skb_queue_head_init(&ax25->frag_queue);
	skb_queue_head_init(&ax25->ack_queue);
	skb_queue_head_init(&ax25->reseq_queue);

	init_timer(&ax25->timer);
	init_timer(&ax25->t1timer);
	init_timer(&ax25->t2timer);
	init_timer(&ax25->t3timer);
	init_timer(&ax25->idletimer);

	ax25_fillin_cb(ax25, NULL);

	ax25->state = AX25_STATE_0;

	return ax25;
}

/*
 *	Handling for system calls applied via the various interfaces to an
 *	AX25 socket object
 */

static int ax25_setsockopt(struct socket *sock, int level, int optname, char *optval, int optlen)
{
	struct sock *sk = sock->sk;
	struct net_device *dev;
	char devname[IFNAMSIZ];
	int opt;

	if (level != SOL_AX25)
		return -ENOPROTOOPT;

	if (optlen < sizeof(int))
		return -EINVAL;

	if (get_user(opt, (int *)optval))
		return -EFAULT;

	switch (optname) {
		case AX25_WINDOW:
			if (sk->protinfo.ax25->modulus == AX25_MODULUS) {
				if (opt < 1 || opt > 7)
					return -EINVAL;
			} else {
				if (opt < 1 || opt > 63)
					return -EINVAL;
			}
			sk->protinfo.ax25->window = opt;
			return 0;

		case AX25_T1:
			if (opt < 1)
				return -EINVAL;
			sk->protinfo.ax25->rtt = (opt * HZ) / 2;
			sk->protinfo.ax25->t1  = opt * HZ;
			return 0;

		case AX25_T2:
			if (opt < 1)
				return -EINVAL;
			sk->protinfo.ax25->t2 = opt * HZ;
			return 0;

		case AX25_N2:
			if (opt < 1 || opt > 31)
				return -EINVAL;
			sk->protinfo.ax25->n2 = opt;
			return 0;

		case AX25_T3:
			if (opt < 1)
				return -EINVAL;
			sk->protinfo.ax25->t3 = opt * HZ;
			return 0;

		case AX25_IDLE:
			if (opt < 0)
				return -EINVAL;
			sk->protinfo.ax25->idle = opt * 60 * HZ;
			return 0;

		case AX25_BACKOFF:
			if (opt < 0 || opt > 2)
				return -EINVAL;
			sk->protinfo.ax25->backoff = opt;
			return 0;

		case AX25_EXTSEQ:
			sk->protinfo.ax25->modulus = opt ? AX25_EMODULUS : AX25_MODULUS;
			return 0;

		case AX25_PIDINCL:
			sk->protinfo.ax25->pidincl = opt ? 1 : 0;
			return 0;

		case AX25_IAMDIGI:
			sk->protinfo.ax25->iamdigi = opt ? 1 : 0;
			return 0;

		case AX25_PACLEN:
			if (opt < 16 || opt > 65535)
				return -EINVAL;
			sk->protinfo.ax25->paclen = opt;
			return 0;

		case SO_BINDTODEVICE:
			if (optlen > IFNAMSIZ) optlen=IFNAMSIZ;
			if (copy_from_user(devname, optval, optlen))
				return -EFAULT;

			dev = dev_get_by_name(devname);
			if (dev == NULL) return -ENODEV;

			if (sk->type == SOCK_SEQPACKET && 
			   (sock->state != SS_UNCONNECTED || sk->state == TCP_LISTEN))
				return -EADDRNOTAVAIL;
		
			sk->protinfo.ax25->ax25_dev = ax25_dev_ax25dev(dev);
			ax25_fillin_cb(sk->protinfo.ax25, sk->protinfo.ax25->ax25_dev);
			return 0;

		default:
			return -ENOPROTOOPT;
	}
}

static int ax25_getsockopt(struct socket *sock, int level, int optname, char *optval, int *optlen)
{
	struct sock *sk = sock->sk;
	struct ax25_dev *ax25_dev;
	char devname[IFNAMSIZ];
	void *valptr;
	int val = 0;
	int maxlen, length;

	if (level != SOL_AX25)
		return -ENOPROTOOPT;

	if (get_user(maxlen, optlen))
		return -EFAULT;
		
	if (maxlen < 1)
		return -EFAULT;

	valptr = (void *) &val;
	length = min_t(unsigned int, maxlen, sizeof(int));

	switch (optname) {
		case AX25_WINDOW:
			val = sk->protinfo.ax25->window;
			break;

		case AX25_T1:
			val = sk->protinfo.ax25->t1 / HZ;
			break;

		case AX25_T2:
			val = sk->protinfo.ax25->t2 / HZ;
			break;

		case AX25_N2:
			val = sk->protinfo.ax25->n2;
			break;

		case AX25_T3:
			val = sk->protinfo.ax25->t3 / HZ;
			break;

		case AX25_IDLE:
			val = sk->protinfo.ax25->idle / (60 * HZ);
			break;

		case AX25_BACKOFF:
			val = sk->protinfo.ax25->backoff;
			break;

		case AX25_EXTSEQ:
			val = (sk->protinfo.ax25->modulus == AX25_EMODULUS);
			break;

		case AX25_PIDINCL:
			val = sk->protinfo.ax25->pidincl;
			break;

		case AX25_IAMDIGI:
			val = sk->protinfo.ax25->iamdigi;
			break;

		case AX25_PACLEN:
			val = sk->protinfo.ax25->paclen;
			break;
			
		case SO_BINDTODEVICE:
			ax25_dev = sk->protinfo.ax25->ax25_dev;

			if (ax25_dev != NULL && ax25_dev->dev != NULL) {
				strncpy(devname, ax25_dev->dev->name, IFNAMSIZ);
				length = min_t(unsigned int, strlen(ax25_dev->dev->name)+1, maxlen);
				devname[length-1] = '\0';
			} else {
				*devname = '\0';
				length = 1;
			}

			valptr = (void *) devname;
			break;

		default:
			return -ENOPROTOOPT;
	}

	if (put_user(length, optlen))
		return -EFAULT;

	return copy_to_user(optval, valptr, length) ? -EFAULT : 0;
}

static int ax25_listen(struct socket *sock, int backlog)
{
	struct sock *sk = sock->sk;

	if (sk->type == SOCK_SEQPACKET && sk->state != TCP_LISTEN) {
		sk->max_ack_backlog = backlog;
		sk->state           = TCP_LISTEN;
		return 0;
	}

	return -EOPNOTSUPP;
}

int ax25_create(struct socket *sock, int protocol)
{
	struct sock *sk;
	ax25_cb *ax25;

	switch (sock->type) {
		case SOCK_DGRAM:
			if (protocol == 0 || protocol == PF_AX25)
				protocol = AX25_P_TEXT;
			break;
		case SOCK_SEQPACKET:
			switch (protocol) {
				case 0:
				case PF_AX25:	/* For CLX */
					protocol = AX25_P_TEXT;
					break;
				case AX25_P_SEGMENT:
#ifdef CONFIG_INET
				case AX25_P_ARP:
				case AX25_P_IP:
#endif
#ifdef CONFIG_NETROM
				case AX25_P_NETROM:
#endif
#ifdef CONFIG_ROSE
				case AX25_P_ROSE:
#endif
					return -ESOCKTNOSUPPORT;
#ifdef CONFIG_NETROM_MODULE
				case AX25_P_NETROM:
					if (ax25_protocol_is_registered(AX25_P_NETROM))
						return -ESOCKTNOSUPPORT;
#endif
#ifdef CONFIG_ROSE_MODULE
				case AX25_P_ROSE:
					if (ax25_protocol_is_registered(AX25_P_ROSE))
						return -ESOCKTNOSUPPORT;
#endif
				default:
					break;
			}
			break;
		case SOCK_RAW:
			break;
		default:
			return -ESOCKTNOSUPPORT;
	}

	if ((sk = sk_alloc(PF_AX25, GFP_ATOMIC, 1)) == NULL)
		return -ENOMEM;

	if ((ax25 = ax25_create_cb()) == NULL) {
		sk_free(sk);
		return -ENOMEM;
	}

	sock_init_data(sock, sk);

	sk->destruct = ax25_free_sock;
	sock->ops    = &ax25_proto_ops;
	sk->protocol = protocol;

	ax25->sk          = sk;
	sk->protinfo.ax25 = ax25;

	return 0;
}

struct sock *ax25_make_new(struct sock *osk, struct ax25_dev *ax25_dev)
{
	struct sock *sk;
	ax25_cb *ax25;

	if ((sk = sk_alloc(PF_AX25, GFP_ATOMIC, 1)) == NULL)
		return NULL;

	if ((ax25 = ax25_create_cb()) == NULL) {
		sk_free(sk);
		return NULL;
	}

	switch (osk->type) {
		case SOCK_DGRAM:
			break;
		case SOCK_SEQPACKET:
			break;
		default:
			sk_free(sk);
			ax25_free_cb(ax25);
			return NULL;
	}

	sock_init_data(NULL, sk);

	sk->destruct = ax25_free_sock;
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

	ax25->modulus = osk->protinfo.ax25->modulus;
	ax25->backoff = osk->protinfo.ax25->backoff;
	ax25->pidincl = osk->protinfo.ax25->pidincl;
	ax25->iamdigi = osk->protinfo.ax25->iamdigi;
	ax25->rtt     = osk->protinfo.ax25->rtt;
	ax25->t1      = osk->protinfo.ax25->t1;
	ax25->t2      = osk->protinfo.ax25->t2;
	ax25->t3      = osk->protinfo.ax25->t3;
	ax25->n2      = osk->protinfo.ax25->n2;
	ax25->idle    = osk->protinfo.ax25->idle;
	ax25->paclen  = osk->protinfo.ax25->paclen;
	ax25->window  = osk->protinfo.ax25->window;

	ax25->ax25_dev    = ax25_dev;
	ax25->source_addr = osk->protinfo.ax25->source_addr;

	if (osk->protinfo.ax25->digipeat != NULL) {
		if ((ax25->digipeat = kmalloc(sizeof(ax25_digi), GFP_ATOMIC)) == NULL) {
			sk_free(sk);
			return NULL;
		}

		memcpy(ax25->digipeat, osk->protinfo.ax25->digipeat, sizeof(ax25_digi));
	}

	sk->protinfo.ax25 = ax25;
	ax25->sk          = sk;

	return sk;
}

static int ax25_release(struct socket *sock)
{
	struct sock *sk = sock->sk;

	if (sk == NULL) return 0;

	if (sk->type == SOCK_SEQPACKET) {
		switch (sk->protinfo.ax25->state) {
			case AX25_STATE_0:
				ax25_disconnect(sk->protinfo.ax25, 0);
				ax25_destroy_socket(sk->protinfo.ax25);
				break;

			case AX25_STATE_1:
			case AX25_STATE_2:
				ax25_send_control(sk->protinfo.ax25, AX25_DISC, AX25_POLLON, AX25_COMMAND);
				ax25_disconnect(sk->protinfo.ax25, 0);
				ax25_destroy_socket(sk->protinfo.ax25);
				break;

			case AX25_STATE_3:
			case AX25_STATE_4:
				ax25_clear_queues(sk->protinfo.ax25);
				sk->protinfo.ax25->n2count = 0;
				switch (sk->protinfo.ax25->ax25_dev->values[AX25_VALUES_PROTOCOL]) {
					case AX25_PROTO_STD_SIMPLEX:
					case AX25_PROTO_STD_DUPLEX:
						ax25_send_control(sk->protinfo.ax25, AX25_DISC, AX25_POLLON, AX25_COMMAND);
						ax25_stop_t2timer(sk->protinfo.ax25);
						ax25_stop_t3timer(sk->protinfo.ax25);
						ax25_stop_idletimer(sk->protinfo.ax25);
						break;
#ifdef CONFIG_AX25_DAMA_SLAVE
					case AX25_PROTO_DAMA_SLAVE:
						ax25_stop_t3timer(sk->protinfo.ax25);
						ax25_stop_idletimer(sk->protinfo.ax25);
						break;
#endif
				}
				ax25_calculate_t1(sk->protinfo.ax25);
				ax25_start_t1timer(sk->protinfo.ax25);
				sk->protinfo.ax25->state = AX25_STATE_2;
				sk->state                = TCP_CLOSE;
				sk->shutdown            |= SEND_SHUTDOWN;
				sk->state_change(sk);
				sock_orphan(sk);
				sk->destroy              = 1;
				break;

			default:
				break;
		}
	} else {
		sk->state     = TCP_CLOSE;
		sk->shutdown |= SEND_SHUTDOWN;
		sk->state_change(sk);
		sock_orphan(sk);
		ax25_destroy_socket(sk->protinfo.ax25);
	}

	sock->sk   = NULL;	
	sk->socket = NULL;	/* Not used, but we should do this */

	return 0;
}

/*
 *	We support a funny extension here so you can (as root) give any callsign
 *	digipeated via a local address as source. This hack is obsolete now
 *	that we've implemented support for SO_BINDTODEVICE. It is however small 
 *	and trivially backward compatible.
 */
static int ax25_bind(struct socket *sock, struct sockaddr *uaddr, int addr_len)
{
	struct sock *sk = sock->sk;
	struct full_sockaddr_ax25 *addr = (struct full_sockaddr_ax25 *)uaddr;
	ax25_address *call;
	ax25_dev *ax25_dev = NULL;

	if (sk->zapped == 0)
		return -EINVAL;

	if (addr_len != sizeof(struct sockaddr_ax25) && 
	    addr_len != sizeof(struct full_sockaddr_ax25)) {
		/* support for old structure may go away some time */
		if ((addr_len < sizeof(struct sockaddr_ax25) + sizeof(ax25_address) * 6) ||
		    (addr_len > sizeof(struct full_sockaddr_ax25)))
			return -EINVAL;

		printk(KERN_WARNING "ax25_bind(): %s uses old (6 digipeater) socket structure.\n",
			current->comm);
	}

	if (addr->fsa_ax25.sax25_family != AF_AX25)
		return -EINVAL;

	call = ax25_findbyuid(current->euid);
	if (call == NULL && ax25_uid_policy && !capable(CAP_NET_ADMIN))
		return -EACCES;

	if (call == NULL)
		sk->protinfo.ax25->source_addr = addr->fsa_ax25.sax25_call;
	else
		sk->protinfo.ax25->source_addr = *call;

	/*
	 * User already set interface with SO_BINDTODEVICE
	 */

	if (sk->protinfo.ax25->ax25_dev != NULL)
		goto done;

	if (addr_len > sizeof(struct sockaddr_ax25) && addr->fsa_ax25.sax25_ndigis == 1) {
		if (ax25cmp(&addr->fsa_digipeater[0], &null_ax25_address) != 0 &&
		    (ax25_dev = ax25_addr_ax25dev(&addr->fsa_digipeater[0])) == NULL)
			return -EADDRNOTAVAIL;
	}  else {
		if ((ax25_dev = ax25_addr_ax25dev(&addr->fsa_ax25.sax25_call)) == NULL)
			return -EADDRNOTAVAIL;
	}

	if (ax25_dev != NULL)
		ax25_fillin_cb(sk->protinfo.ax25, ax25_dev);

done:
	ax25_insert_socket(sk->protinfo.ax25);
	sk->zapped = 0;
	return 0;
}

/*
 *	FIXME: nonblock behaviour looks like it may have a bug.
 */
static int ax25_connect(struct socket *sock, struct sockaddr *uaddr, int addr_len, int flags)
{
	struct sock *sk = sock->sk;
	struct full_sockaddr_ax25 *fsa = (struct full_sockaddr_ax25 *)uaddr;
	ax25_digi *digi = NULL;
	int ct = 0, err;

	/* deal with restarts */
	if (sock->state == SS_CONNECTING) {
		switch (sk->state) {
		case TCP_SYN_SENT: /* still trying */
			return -EINPROGRESS;

		case TCP_ESTABLISHED: /* connection established */
			sock->state = SS_CONNECTED;
			return 0;

		case TCP_CLOSE: /* connection refused */
			sock->state = SS_UNCONNECTED;
			return -ECONNREFUSED;
		}
	}

	if (sk->state == TCP_ESTABLISHED && sk->type == SOCK_SEQPACKET)
		return -EISCONN;	/* No reconnect on a seqpacket socket */

	sk->state   = TCP_CLOSE;
	sock->state = SS_UNCONNECTED;

	/*
	 * some sanity checks. code further down depends on this
	 */

	if (addr_len == sizeof(struct sockaddr_ax25)) {
		/* support for this will go away in early 2.5.x */
		printk(KERN_WARNING "ax25_connect(): %s uses obsolete socket structure\n",
			current->comm);
	}
	else if (addr_len != sizeof(struct full_sockaddr_ax25)) {
		/* support for old structure may go away some time */
		if ((addr_len < sizeof(struct sockaddr_ax25) + sizeof(ax25_address) * 6) ||
		    (addr_len > sizeof(struct full_sockaddr_ax25)))
			return -EINVAL;

		printk(KERN_WARNING "ax25_connect(): %s uses old (6 digipeater) socket structure.\n",
			current->comm);
	}

	if (fsa->fsa_ax25.sax25_family != AF_AX25)
		return -EINVAL;

	if (sk->protinfo.ax25->digipeat != NULL) {
		kfree(sk->protinfo.ax25->digipeat);
		sk->protinfo.ax25->digipeat = NULL;
	}
	
	/*
	 *	Handle digi-peaters to be used.
	 */
	if (addr_len > sizeof(struct sockaddr_ax25) && fsa->fsa_ax25.sax25_ndigis != 0) {
		/* Valid number of digipeaters ? */
		if (fsa->fsa_ax25.sax25_ndigis < 1 || fsa->fsa_ax25.sax25_ndigis > AX25_MAX_DIGIS)
			return -EINVAL;

		if ((digi = kmalloc(sizeof(ax25_digi), GFP_KERNEL)) == NULL)
			return -ENOBUFS;

		digi->ndigi      = fsa->fsa_ax25.sax25_ndigis;
		digi->lastrepeat = -1;

		while (ct < fsa->fsa_ax25.sax25_ndigis) {
			if ((fsa->fsa_digipeater[ct].ax25_call[6] & AX25_HBIT) && sk->protinfo.ax25->iamdigi) {
				digi->repeated[ct] = 1;
				digi->lastrepeat   = ct;
			} else {
				digi->repeated[ct] = 0;
			}
			digi->calls[ct] = fsa->fsa_digipeater[ct];
			ct++;
		}
	}

	/*
	 *	Must bind first - autobinding in this may or may not work. If
	 *	the socket is already bound, check to see if the device has
	 *	been filled in, error if it hasn't.
	 */
	if (sk->zapped) {
		/* check if we can remove this feature. It is broken. */
		printk(KERN_WARNING "ax25_connect(): %s uses autobind, please contact jreuter@yaina.de\n",
			current->comm);
		if ((err = ax25_rt_autobind(sk->protinfo.ax25, &fsa->fsa_ax25.sax25_call)) < 0)
			return err;
		ax25_fillin_cb(sk->protinfo.ax25, sk->protinfo.ax25->ax25_dev);
		ax25_insert_socket(sk->protinfo.ax25);
	} else {
		if (sk->protinfo.ax25->ax25_dev == NULL)
			return -EHOSTUNREACH;
	}

	if (sk->type == SOCK_SEQPACKET && ax25_find_cb(&sk->protinfo.ax25->source_addr, &fsa->fsa_ax25.sax25_call, digi, sk->protinfo.ax25->ax25_dev->dev) != NULL) {
		if (digi != NULL) kfree(digi);
		return -EADDRINUSE;			/* Already such a connection */
	}

	sk->protinfo.ax25->dest_addr = fsa->fsa_ax25.sax25_call;
	sk->protinfo.ax25->digipeat  = digi;

	/* First the easy one */
	if (sk->type != SOCK_SEQPACKET) {
		sock->state = SS_CONNECTED;
		sk->state   = TCP_ESTABLISHED;
		return 0;
	}

	/* Move to connecting socket, ax.25 lapb WAIT_UA.. */
	sock->state        = SS_CONNECTING;
	sk->state          = TCP_SYN_SENT;

	switch (sk->protinfo.ax25->ax25_dev->values[AX25_VALUES_PROTOCOL]) {
		case AX25_PROTO_STD_SIMPLEX:
		case AX25_PROTO_STD_DUPLEX:
			ax25_std_establish_data_link(sk->protinfo.ax25);
			break;

#ifdef CONFIG_AX25_DAMA_SLAVE
		case AX25_PROTO_DAMA_SLAVE:
			sk->protinfo.ax25->modulus = AX25_MODULUS;
			sk->protinfo.ax25->window  = sk->protinfo.ax25->ax25_dev->values[AX25_VALUES_WINDOW];
			if (sk->protinfo.ax25->ax25_dev->dama.slave)
				ax25_ds_establish_data_link(sk->protinfo.ax25);
			else
				ax25_std_establish_data_link(sk->protinfo.ax25);
			break;
#endif
	}

	sk->protinfo.ax25->state = AX25_STATE_1;

	ax25_start_heartbeat(sk->protinfo.ax25);

	/* Now the loop */
	if (sk->state != TCP_ESTABLISHED && (flags & O_NONBLOCK))
		return -EINPROGRESS;

	cli();	/* To avoid races on the sleep */

	/* A DM or timeout will go to closed, a UA will go to ABM */
	while (sk->state == TCP_SYN_SENT) {
		interruptible_sleep_on(sk->sleep);
		if (signal_pending(current)) {
			sti();
			return -ERESTARTSYS;
		}
	}

	if (sk->state != TCP_ESTABLISHED) {
		/* Not in ABM, not in WAIT_UA -> failed */
		sti();
		sock->state = SS_UNCONNECTED;
		return sock_error(sk);	/* Always set at this point */
	}

	sock->state = SS_CONNECTED;

	sti();

	return 0;
}


static int ax25_accept(struct socket *sock, struct socket *newsock, int flags)
{
	struct sock *sk;
	struct sock *newsk;
	struct sk_buff *skb;

	if (sock->state != SS_UNCONNECTED)
		return -EINVAL;

	if ((sk = sock->sk) == NULL)
		return -EINVAL;

	if (sk->type != SOCK_SEQPACKET)
		return -EOPNOTSUPP;

	if (sk->state != TCP_LISTEN)
		return -EINVAL;

	/*
	 *	The read queue this time is holding sockets ready to use
	 *	hooked into the SABM we saved
	 */
	do {
		if ((skb = skb_dequeue(&sk->receive_queue)) == NULL) {
			if (flags & O_NONBLOCK)
				return -EWOULDBLOCK;

			interruptible_sleep_on(sk->sleep);
			if (signal_pending(current)) 
				return -ERESTARTSYS;
		}
	} while (skb == NULL);

	newsk = skb->sk;
	newsk->pair = NULL;
	newsk->socket = newsock;
	newsk->sleep = &newsock->wait;

	/* Now attach up the new socket */
	kfree_skb(skb);
	sk->ack_backlog--;
	newsock->sk    = newsk;
	newsock->state = SS_CONNECTED;

	return 0;
}

static int ax25_getname(struct socket *sock, struct sockaddr *uaddr, int *uaddr_len, int peer)
{
	struct sock *sk = sock->sk;
	struct full_sockaddr_ax25 *fsa = (struct full_sockaddr_ax25 *)uaddr;
	unsigned char ndigi, i;

	if (peer != 0) {
		if (sk->state != TCP_ESTABLISHED)
			return -ENOTCONN;

		fsa->fsa_ax25.sax25_family = AF_AX25;
		fsa->fsa_ax25.sax25_call   = sk->protinfo.ax25->dest_addr;
		fsa->fsa_ax25.sax25_ndigis = 0;

		if (sk->protinfo.ax25->digipeat != NULL) {
			ndigi = sk->protinfo.ax25->digipeat->ndigi;
			fsa->fsa_ax25.sax25_ndigis = ndigi;
			for (i = 0; i < ndigi; i++)
				fsa->fsa_digipeater[i] = sk->protinfo.ax25->digipeat->calls[i];
		}
	} else {
		fsa->fsa_ax25.sax25_family = AF_AX25;
		fsa->fsa_ax25.sax25_call   = sk->protinfo.ax25->source_addr;
		fsa->fsa_ax25.sax25_ndigis = 1;
		if (sk->protinfo.ax25->ax25_dev != NULL) {
			memcpy(&fsa->fsa_digipeater[0], sk->protinfo.ax25->ax25_dev->dev->dev_addr, AX25_ADDR_LEN);
		} else {
			fsa->fsa_digipeater[0] = null_ax25_address;
		}
	}
	*uaddr_len = sizeof (struct full_sockaddr_ax25);
	return 0;
}

static int ax25_sendmsg(struct socket *sock, struct msghdr *msg, int len, struct scm_cookie *scm)
{
	struct sock *sk = sock->sk;
	struct sockaddr_ax25 *usax = (struct sockaddr_ax25 *)msg->msg_name;
	int err;
	struct sockaddr_ax25 sax;
	struct sk_buff *skb;
	unsigned char *asmptr;
	int size;
	ax25_digi *dp;
	ax25_digi dtmp;
	int lv;
	int addr_len = msg->msg_namelen;

	if (msg->msg_flags & ~(MSG_DONTWAIT|MSG_EOR))
		return -EINVAL;

	if (sk->zapped)
		return -EADDRNOTAVAIL;

	if (sk->shutdown & SEND_SHUTDOWN) {
		send_sig(SIGPIPE, current, 0);
		return -EPIPE;
	}

	if (sk->protinfo.ax25->ax25_dev == NULL)
		return -ENETUNREACH;

	if (usax != NULL) {
		if (usax->sax25_family != AF_AX25)
			return -EINVAL;

		if (addr_len == sizeof(struct sockaddr_ax25)) {
			printk(KERN_WARNING "ax25_sendmsg(): %s uses obsolete socket structure\n",
				current->comm);
		}
		else if (addr_len != sizeof(struct full_sockaddr_ax25)) {
			/* support for old structure may go away some time */
			if ((addr_len < sizeof(struct sockaddr_ax25) + sizeof(ax25_address) * 6) ||
		    	    (addr_len > sizeof(struct full_sockaddr_ax25)))
		    		return -EINVAL;

			printk(KERN_WARNING "ax25_sendmsg(): %s uses old (6 digipeater) socket structure.\n",
				current->comm);
		}

		if (addr_len > sizeof(struct sockaddr_ax25) && usax->sax25_ndigis != 0) {
			int ct           = 0;
			struct full_sockaddr_ax25 *fsa = (struct full_sockaddr_ax25 *)usax;

			/* Valid number of digipeaters ? */
			if (usax->sax25_ndigis < 1 || usax->sax25_ndigis > AX25_MAX_DIGIS)
				return -EINVAL;

			dtmp.ndigi      = usax->sax25_ndigis;

			while (ct < usax->sax25_ndigis) {
				dtmp.repeated[ct] = 0;
				dtmp.calls[ct]    = fsa->fsa_digipeater[ct];
				ct++;
			}

			dtmp.lastrepeat = 0;
		}

		sax = *usax;
		if (sk->type == SOCK_SEQPACKET && ax25cmp(&sk->protinfo.ax25->dest_addr, &sax.sax25_call) != 0)
			return -EISCONN;
		if (usax->sax25_ndigis == 0)
			dp = NULL;
		else
			dp = &dtmp;
	} else {
		/*
		 *	FIXME: 1003.1g - if the socket is like this because
		 *	it has become closed (not started closed) and is VC
		 *	we ought to SIGPIPE, EPIPE
		 */
		if (sk->state != TCP_ESTABLISHED)
			return -ENOTCONN;
		sax.sax25_family = AF_AX25;
		sax.sax25_call   = sk->protinfo.ax25->dest_addr;
		dp = sk->protinfo.ax25->digipeat;
	}

	SOCK_DEBUG(sk, "AX.25: sendto: Addresses built.\n");

	/* Build a packet */
	SOCK_DEBUG(sk, "AX.25: sendto: building packet.\n");

	/* Assume the worst case */
	size = len + 3 + ax25_addr_size(dp) + AX25_BPQ_HEADER_LEN;

	if ((skb = sock_alloc_send_skb(sk, size, msg->msg_flags & MSG_DONTWAIT, &err)) == NULL)
		return err;

	skb_reserve(skb, size - len);

	SOCK_DEBUG(sk, "AX.25: Appending user data\n");

	/* User data follows immediately after the AX.25 data */
	memcpy_fromiovec(skb_put(skb, len), msg->msg_iov, len);
	skb->nh.raw = skb->data;

	/* Add the PID if one is not supplied by the user in the skb */
	if (!sk->protinfo.ax25->pidincl) {
		asmptr  = skb_push(skb, 1);
		*asmptr = sk->protocol;
	}

	SOCK_DEBUG(sk, "AX.25: Transmitting buffer\n");

	if (sk->type == SOCK_SEQPACKET) {
		/* Connected mode sockets go via the LAPB machine */
		if (sk->state != TCP_ESTABLISHED) {
			kfree_skb(skb);
			return -ENOTCONN;
		}

		ax25_output(sk->protinfo.ax25, sk->protinfo.ax25->paclen, skb);	/* Shove it onto the queue and kick */

		return len;
	} else {
		asmptr = skb_push(skb, 1 + ax25_addr_size(dp));

		SOCK_DEBUG(sk, "Building AX.25 Header (dp=%p).\n", dp);

		if (dp != NULL)
			SOCK_DEBUG(sk, "Num digipeaters=%d\n", dp->ndigi);

		/* Build an AX.25 header */
		asmptr += (lv = ax25_addr_build(asmptr, &sk->protinfo.ax25->source_addr, &sax.sax25_call, dp, AX25_COMMAND, AX25_MODULUS));

		SOCK_DEBUG(sk, "Built header (%d bytes)\n",lv);

		skb->h.raw = asmptr;

		SOCK_DEBUG(sk, "base=%p pos=%p\n", skb->data, asmptr);

		*asmptr = AX25_UI;

		/* Datagram frames go straight out of the door as UI */
		skb->dev      = sk->protinfo.ax25->ax25_dev->dev;

		ax25_queue_xmit(skb);

		return len;
	}
}

static int ax25_recvmsg(struct socket *sock, struct msghdr *msg, int size, int flags, struct scm_cookie *scm)
{
	struct sock *sk = sock->sk;
	int copied;
	struct sk_buff *skb;
	int er;

	/*
	 * 	This works for seqpacket too. The receiver has ordered the
	 *	queue for us! We do one quick check first though
	 */
	if (sk->type == SOCK_SEQPACKET && sk->state != TCP_ESTABLISHED)
		return -ENOTCONN;

	/* Now we can treat all alike */
	if ((skb = skb_recv_datagram(sk, flags & ~MSG_DONTWAIT, flags & MSG_DONTWAIT, &er)) == NULL)
		return er;

	if (!sk->protinfo.ax25->pidincl)
		skb_pull(skb, 1);		/* Remove PID */

	skb->h.raw = skb->data;
	copied     = skb->len;

	if (copied > size) {
		copied = size;
		msg->msg_flags |= MSG_TRUNC;
	}		

	skb_copy_datagram_iovec(skb, 0, msg->msg_iov, copied);

	if (msg->msg_namelen != 0) {
		struct sockaddr_ax25 *sax = (struct sockaddr_ax25 *)msg->msg_name;
		ax25_digi digi;
		ax25_address dest;

		ax25_addr_parse(skb->mac.raw+1, skb->data-skb->mac.raw-1, NULL, &dest, &digi, NULL, NULL);

		sax->sax25_family = AF_AX25;
		/* We set this correctly, even though we may not let the
		   application know the digi calls further down (because it
		   did NOT ask to know them).  This could get political... **/
		sax->sax25_ndigis = digi.ndigi;
		sax->sax25_call   = dest;

		if (sax->sax25_ndigis != 0) {
			int ct;
			struct full_sockaddr_ax25 *fsa = (struct full_sockaddr_ax25 *)sax;

			for (ct = 0; ct < digi.ndigi; ct++)
				fsa->fsa_digipeater[ct] = digi.calls[ct];
		}
		msg->msg_namelen = sizeof(struct full_sockaddr_ax25);
	}

	skb_free_datagram(sk, skb);

	return copied;
}

static int ax25_shutdown(struct socket *sk, int how)
{
	/* FIXME - generate DM and RNR states */
	return -EOPNOTSUPP;
}

static int ax25_ioctl(struct socket *sock, unsigned int cmd, unsigned long arg)
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

		case SIOCAX25ADDUID:	/* Add a uid to the uid/call map table */
		case SIOCAX25DELUID:	/* Delete a uid from the uid/call map table */
		case SIOCAX25GETUID: {
			struct sockaddr_ax25 sax25;
			if (copy_from_user(&sax25, (void *)arg, sizeof(sax25)))
				return -EFAULT;
			return ax25_uid_ioctl(cmd, &sax25);
		}

		case SIOCAX25NOUID: {	/* Set the default policy (default/bar) */
			long amount;
			if (!capable(CAP_NET_ADMIN))
				return -EPERM;
			if (get_user(amount, (long *)arg))
				return -EFAULT;
			if (amount > AX25_NOUID_BLOCK)
				return -EINVAL;
			ax25_uid_policy = amount;
			return 0;
		}

		case SIOCADDRT:
		case SIOCDELRT:
		case SIOCAX25OPTRT:
			if (!capable(CAP_NET_ADMIN))
				return -EPERM;
			return ax25_rt_ioctl(cmd, (void *)arg);

		case SIOCAX25CTLCON:
			if (!capable(CAP_NET_ADMIN))
				return -EPERM;
			return ax25_ctl_ioctl(cmd, (void *)arg);

		case SIOCAX25GETINFO: 
		case SIOCAX25GETINFOOLD: {
			struct ax25_info_struct ax25_info;

			ax25_info.t1        = sk->protinfo.ax25->t1   / HZ;
			ax25_info.t2        = sk->protinfo.ax25->t2   / HZ;
			ax25_info.t3        = sk->protinfo.ax25->t3   / HZ;
			ax25_info.idle      = sk->protinfo.ax25->idle / (60 * HZ);
			ax25_info.n2        = sk->protinfo.ax25->n2;
			ax25_info.t1timer   = ax25_display_timer(&sk->protinfo.ax25->t1timer)   / HZ;
			ax25_info.t2timer   = ax25_display_timer(&sk->protinfo.ax25->t2timer)   / HZ;
			ax25_info.t3timer   = ax25_display_timer(&sk->protinfo.ax25->t3timer)   / HZ;
			ax25_info.idletimer = ax25_display_timer(&sk->protinfo.ax25->idletimer) / (60 * HZ);
			ax25_info.n2count   = sk->protinfo.ax25->n2count;
			ax25_info.state     = sk->protinfo.ax25->state;
			ax25_info.rcv_q     = atomic_read(&sk->rmem_alloc);
			ax25_info.snd_q     = atomic_read(&sk->wmem_alloc);
			ax25_info.vs        = sk->protinfo.ax25->vs;
			ax25_info.vr        = sk->protinfo.ax25->vr;
			ax25_info.va        = sk->protinfo.ax25->va;
			ax25_info.vs_max    = sk->protinfo.ax25->vs; /* reserved */
			ax25_info.paclen    = sk->protinfo.ax25->paclen;
			ax25_info.window    = sk->protinfo.ax25->window;

			/* old structure? */
			if (cmd == SIOCAX25GETINFOOLD) {
				static int warned = 0;
				if (!warned) {
					printk(KERN_INFO "%s uses old SIOCAX25GETINFO\n",
						current->comm);
					warned=1;
				}

				if (copy_to_user((void *)arg, &ax25_info, sizeof(struct ax25_info_struct_depreciated)))
					return -EFAULT;
			} else {
				if (copy_to_user((void *)arg, &ax25_info, sizeof(struct ax25_info_struct)))
					return -EINVAL;
			} 
			return 0;
		}

		case SIOCAX25ADDFWD:
		case SIOCAX25DELFWD: {
			struct ax25_fwd_struct ax25_fwd;
			if (!capable(CAP_NET_ADMIN))
				return -EPERM;
			if (copy_from_user(&ax25_fwd, (void *)arg, sizeof(ax25_fwd)))
				return -EFAULT;
			return ax25_fwd_ioctl(cmd, &ax25_fwd);
		}

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

		default:
			return dev_ioctl(cmd, (void *)arg);
	}

	/*NOTREACHED*/
	return 0;
}

static int ax25_get_info(char *buffer, char **start, off_t offset, int length)
{
	ax25_cb *ax25;
	int k;
	int len = 0;
	off_t pos = 0;
	off_t begin = 0;

	cli();

	/*
	 * New format:
	 * magic dev src_addr dest_addr,digi1,digi2,.. st vs vr va t1 t1 t2 t2 t3 t3 idle idle n2 n2 rtt window paclen Snd-Q Rcv-Q inode 
	 */
	
	for (ax25 = ax25_list; ax25 != NULL; ax25 = ax25->next) {
		len += sprintf(buffer+len, "%8.8lx %s %s%s ", 
				(long) ax25, 
				ax25->ax25_dev == NULL? "???" : ax25->ax25_dev->dev->name,
				ax2asc(&ax25->source_addr),
				ax25->iamdigi? "*":"");

		len += sprintf(buffer+len, "%s", ax2asc(&ax25->dest_addr));
				
		for (k=0; (ax25->digipeat != NULL) && (k < ax25->digipeat->ndigi); k++) {
			len += sprintf(buffer+len, ",%s%s",
					ax2asc(&ax25->digipeat->calls[k]),
					ax25->digipeat->repeated[k]? "*":"");
		}
		
		len += sprintf(buffer+len, " %d %d %d %d %lu %lu %lu %lu %lu %lu %lu %lu %d %d %lu %d %d",
			ax25->state,
			ax25->vs, ax25->vr, ax25->va,
			ax25_display_timer(&ax25->t1timer) / HZ, ax25->t1 / HZ,
			ax25_display_timer(&ax25->t2timer) / HZ, ax25->t2 / HZ,
			ax25_display_timer(&ax25->t3timer) / HZ, ax25->t3 / HZ,
			ax25_display_timer(&ax25->idletimer) / (60 * HZ),
			ax25->idle / (60 * HZ),
			ax25->n2count, ax25->n2,
			ax25->rtt / HZ,
			ax25->window,
			ax25->paclen);

		if (ax25->sk != NULL) {
			len += sprintf(buffer + len, " %d %d %ld\n",
				atomic_read(&ax25->sk->wmem_alloc),
				atomic_read(&ax25->sk->rmem_alloc),
				ax25->sk->socket != NULL ? ax25->sk->socket->inode->i_ino : 0L);
		} else {
			len += sprintf(buffer + len, " * * *\n");
		}

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

static struct net_proto_family ax25_family_ops = {
	family:		PF_AX25,
	create:		ax25_create,
};

static struct proto_ops SOCKOPS_WRAPPED(ax25_proto_ops) = {
	family:		PF_AX25,

	release:	ax25_release,
	bind:		ax25_bind,
	connect:	ax25_connect,
	socketpair:	sock_no_socketpair,
	accept:		ax25_accept,
	getname:	ax25_getname,
	poll:		datagram_poll,
	ioctl:		ax25_ioctl,
	listen:		ax25_listen,
	shutdown:	ax25_shutdown,
	setsockopt:	ax25_setsockopt,
	getsockopt:	ax25_getsockopt,
	sendmsg:	ax25_sendmsg,
	recvmsg:	ax25_recvmsg,
	mmap:		sock_no_mmap,
	sendpage:	sock_no_sendpage,
};

#include <linux/smp_lock.h>
SOCKOPS_WRAP(ax25_proto, PF_AX25);

/*
 *	Called by socket.c on kernel start up
 */
static struct packet_type ax25_packet_type = {
	type:		__constant_htons(ETH_P_AX25),
	func:		ax25_kiss_rcv,
};

static struct notifier_block ax25_dev_notifier = {
	notifier_call:	ax25_device_event,
};

EXPORT_SYMBOL(ax25_encapsulate);
EXPORT_SYMBOL(ax25_rebuild_header);
EXPORT_SYMBOL(ax25_findbyuid);
EXPORT_SYMBOL(ax25_find_cb);
EXPORT_SYMBOL(ax25_linkfail_register);
EXPORT_SYMBOL(ax25_linkfail_release);
EXPORT_SYMBOL(ax25_listen_register);
EXPORT_SYMBOL(ax25_listen_release);
EXPORT_SYMBOL(ax25_protocol_register);
EXPORT_SYMBOL(ax25_protocol_release);
EXPORT_SYMBOL(ax25_send_frame);
EXPORT_SYMBOL(ax25_uid_policy);
EXPORT_SYMBOL(ax25cmp);
EXPORT_SYMBOL(ax2asc);
EXPORT_SYMBOL(asc2ax);
EXPORT_SYMBOL(null_ax25_address);
EXPORT_SYMBOL(ax25_display_timer);

static char banner[] __initdata = KERN_INFO "NET4: G4KLX/GW4PTS AX.25 for Linux. Version 0.37 for Linux NET4.0\n";

static int __init ax25_init(void)
{
	sock_register(&ax25_family_ops);
	dev_add_pack(&ax25_packet_type);
	register_netdevice_notifier(&ax25_dev_notifier);
	ax25_register_sysctl();

	proc_net_create("ax25_route", 0, ax25_rt_get_info);
	proc_net_create("ax25", 0, ax25_get_info);
	proc_net_create("ax25_calls", 0, ax25_uid_get_info);

	printk(banner);
	return 0;
}
module_init(ax25_init);


MODULE_AUTHOR("Jonathan Naylor G4KLX <g4klx@g4klx.demon.co.uk>");
MODULE_DESCRIPTION("The amateur radio AX.25 link layer protocol");
MODULE_LICENSE("GPL");

static void __exit ax25_exit(void)
{
	proc_net_remove("ax25_route");
	proc_net_remove("ax25");
	proc_net_remove("ax25_calls");
	ax25_rt_free();
	ax25_uid_free();
	ax25_dev_free();

	ax25_unregister_sysctl();
	unregister_netdevice_notifier(&ax25_dev_notifier);

	dev_remove_pack(&ax25_packet_type);

	sock_unregister(PF_AX25);
}
module_exit(ax25_exit);
