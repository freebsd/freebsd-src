/*
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  INET is implemented using the  BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		Generic socket support routines. Memory allocators, socket lock/release
 *		handler for protocols to use and generic option handler.
 *
 *
 * Version:	$Id: sock.c,v 1.116 2001/11/08 04:20:06 davem Exp $
 *
 * Authors:	Ross Biro, <bir7@leland.Stanford.Edu>
 *		Fred N. van Kempen, <waltje@uWalt.NL.Mugnet.ORG>
 *		Florian La Roche, <flla@stud.uni-sb.de>
 *		Alan Cox, <A.Cox@swansea.ac.uk>
 *
 * Fixes:
 *		Alan Cox	: 	Numerous verify_area() problems
 *		Alan Cox	:	Connecting on a connecting socket
 *					now returns an error for tcp.
 *		Alan Cox	:	sock->protocol is set correctly.
 *					and is not sometimes left as 0.
 *		Alan Cox	:	connect handles icmp errors on a
 *					connect properly. Unfortunately there
 *					is a restart syscall nasty there. I
 *					can't match BSD without hacking the C
 *					library. Ideas urgently sought!
 *		Alan Cox	:	Disallow bind() to addresses that are
 *					not ours - especially broadcast ones!!
 *		Alan Cox	:	Socket 1024 _IS_ ok for users. (fencepost)
 *		Alan Cox	:	sock_wfree/sock_rfree don't destroy sockets,
 *					instead they leave that for the DESTROY timer.
 *		Alan Cox	:	Clean up error flag in accept
 *		Alan Cox	:	TCP ack handling is buggy, the DESTROY timer
 *					was buggy. Put a remove_sock() in the handler
 *					for memory when we hit 0. Also altered the timer
 *					code. The ACK stuff can wait and needs major 
 *					TCP layer surgery.
 *		Alan Cox	:	Fixed TCP ack bug, removed remove sock
 *					and fixed timer/inet_bh race.
 *		Alan Cox	:	Added zapped flag for TCP
 *		Alan Cox	:	Move kfree_skb into skbuff.c and tidied up surplus code
 *		Alan Cox	:	for new sk_buff allocations wmalloc/rmalloc now call alloc_skb
 *		Alan Cox	:	kfree_s calls now are kfree_skbmem so we can track skb resources
 *		Alan Cox	:	Supports socket option broadcast now as does udp. Packet and raw need fixing.
 *		Alan Cox	:	Added RCVBUF,SNDBUF size setting. It suddenly occurred to me how easy it was so...
 *		Rick Sladkey	:	Relaxed UDP rules for matching packets.
 *		C.E.Hawkins	:	IFF_PROMISC/SIOCGHWADDR support
 *	Pauline Middelink	:	identd support
 *		Alan Cox	:	Fixed connect() taking signals I think.
 *		Alan Cox	:	SO_LINGER supported
 *		Alan Cox	:	Error reporting fixes
 *		Anonymous	:	inet_create tidied up (sk->reuse setting)
 *		Alan Cox	:	inet sockets don't set sk->type!
 *		Alan Cox	:	Split socket option code
 *		Alan Cox	:	Callbacks
 *		Alan Cox	:	Nagle flag for Charles & Johannes stuff
 *		Alex		:	Removed restriction on inet fioctl
 *		Alan Cox	:	Splitting INET from NET core
 *		Alan Cox	:	Fixed bogus SO_TYPE handling in getsockopt()
 *		Adam Caldwell	:	Missing return in SO_DONTROUTE/SO_DEBUG code
 *		Alan Cox	:	Split IP from generic code
 *		Alan Cox	:	New kfree_skbmem()
 *		Alan Cox	:	Make SO_DEBUG superuser only.
 *		Alan Cox	:	Allow anyone to clear SO_DEBUG
 *					(compatibility fix)
 *		Alan Cox	:	Added optimistic memory grabbing for AF_UNIX throughput.
 *		Alan Cox	:	Allocator for a socket is settable.
 *		Alan Cox	:	SO_ERROR includes soft errors.
 *		Alan Cox	:	Allow NULL arguments on some SO_ opts
 *		Alan Cox	: 	Generic socket allocation to make hooks
 *					easier (suggested by Craig Metz).
 *		Michael Pall	:	SO_ERROR returns positive errno again
 *              Steve Whitehouse:       Added default destructor to free
 *                                      protocol private data.
 *              Steve Whitehouse:       Added various other default routines
 *                                      common to several socket families.
 *              Chris Evans     :       Call suser() check last on F_SETOWN
 *		Jay Schulist	:	Added SO_ATTACH_FILTER and SO_DETACH_FILTER.
 *		Andi Kleen	:	Add sock_kmalloc()/sock_kfree_s()
 *		Andi Kleen	:	Fix write_space callback
 *		Chris Evans	:	Security fixes - signedness again
 *		Arnaldo C. Melo :       cleanups, use skb_queue_purge
 *
 * To Fix:
 *
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 */

#include <linux/config.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/socket.h>
#include <linux/in.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/string.h>
#include <linux/sockios.h>
#include <linux/net.h>
#include <linux/fcntl.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/poll.h>
#include <linux/tcp.h>
#include <linux/init.h>

#include <asm/uaccess.h>
#include <asm/system.h>

#include <linux/netdevice.h>
#include <net/protocol.h>
#include <linux/skbuff.h>
#include <net/sock.h>
#include <linux/ipsec.h>

#ifdef CONFIG_FILTER
#include <linux/filter.h>
#endif

#ifdef CONFIG_INET
#include <net/tcp.h>
#endif

/* Take into consideration the size of the struct sk_buff overhead in the
 * determination of these values, since that is non-constant across
 * platforms.  This makes socket queueing behavior and performance
 * not depend upon such differences.
 */
#define _SK_MEM_PACKETS		256
#define _SK_MEM_OVERHEAD	(sizeof(struct sk_buff) + 256)
#define SK_WMEM_MAX		(_SK_MEM_OVERHEAD * _SK_MEM_PACKETS)
#define SK_RMEM_MAX		(_SK_MEM_OVERHEAD * _SK_MEM_PACKETS)

/* Run time adjustable parameters. */
__u32 sysctl_wmem_max = SK_WMEM_MAX;
__u32 sysctl_rmem_max = SK_RMEM_MAX;
__u32 sysctl_wmem_default = SK_WMEM_MAX;
__u32 sysctl_rmem_default = SK_RMEM_MAX;

/* Maximal space eaten by iovec or ancilliary data plus some space */
int sysctl_optmem_max = sizeof(unsigned long)*(2*UIO_MAXIOV + 512);

static int sock_set_timeout(long *timeo_p, char *optval, int optlen)
{
	struct timeval tv;

	if (optlen < sizeof(tv))
		return -EINVAL;
	if (copy_from_user(&tv, optval, sizeof(tv)))
		return -EFAULT;

	*timeo_p = MAX_SCHEDULE_TIMEOUT;
	if (tv.tv_sec == 0 && tv.tv_usec == 0)
		return 0;
	if (tv.tv_sec < (MAX_SCHEDULE_TIMEOUT/HZ - 1))
		*timeo_p = tv.tv_sec*HZ + (tv.tv_usec+(1000000/HZ-1))/(1000000/HZ);
	return 0;
}

/*
 *	This is meant for all protocols to use and covers goings on
 *	at the socket level. Everything here is generic.
 */

int sock_setsockopt(struct socket *sock, int level, int optname,
		    char *optval, int optlen)
{
	struct sock *sk=sock->sk;
#ifdef CONFIG_FILTER
	struct sk_filter *filter;
#endif
	int val;
	int valbool;
	struct linger ling;
	int ret = 0;
	
	/*
	 *	Options without arguments
	 */

#ifdef SO_DONTLINGER		/* Compatibility item... */
	switch(optname)
	{
		case SO_DONTLINGER:
			sk->linger=0;
			return 0;
	}
#endif	
		
  	if(optlen<sizeof(int))
  		return(-EINVAL);
  	
	if (get_user(val, (int *)optval))
		return -EFAULT;
	
  	valbool = val?1:0;

	lock_sock(sk);

  	switch(optname) 
  	{
		case SO_DEBUG:	
			if(val && !capable(CAP_NET_ADMIN))
			{
				ret = -EACCES;
			}
			else
				sk->debug=valbool;
			break;
		case SO_REUSEADDR:
			sk->reuse = valbool;
			break;
		case SO_TYPE:
		case SO_ERROR:
			ret = -ENOPROTOOPT;
		  	break;
		case SO_DONTROUTE:
			sk->localroute=valbool;
			break;
		case SO_BROADCAST:
			sk->broadcast=valbool;
			break;
		case SO_SNDBUF:
			/* Don't error on this BSD doesn't and if you think
			   about it this is right. Otherwise apps have to
			   play 'guess the biggest size' games. RCVBUF/SNDBUF
			   are treated in BSD as hints */
			   
			if (val > sysctl_wmem_max)
				val = sysctl_wmem_max;

			sk->userlocks |= SOCK_SNDBUF_LOCK;
			if ((val * 2) < SOCK_MIN_SNDBUF)
				sk->sndbuf = SOCK_MIN_SNDBUF;
			else
				sk->sndbuf = (val * 2);

			/*
			 *	Wake up sending tasks if we
			 *	upped the value.
			 */
			sk->write_space(sk);
			break;

		case SO_RCVBUF:
			/* Don't error on this BSD doesn't and if you think
			   about it this is right. Otherwise apps have to
			   play 'guess the biggest size' games. RCVBUF/SNDBUF
			   are treated in BSD as hints */
			  
			if (val > sysctl_rmem_max)
				val = sysctl_rmem_max;

			sk->userlocks |= SOCK_RCVBUF_LOCK;
			/* FIXME: is this lower bound the right one? */
			if ((val * 2) < SOCK_MIN_RCVBUF)
				sk->rcvbuf = SOCK_MIN_RCVBUF;
			else
				sk->rcvbuf = (val * 2);
			break;

		case SO_KEEPALIVE:
#ifdef CONFIG_INET
			if (sk->protocol == IPPROTO_TCP)
			{
				tcp_set_keepalive(sk, valbool);
			}
#endif
			sk->keepopen = valbool;
			break;

	 	case SO_OOBINLINE:
			sk->urginline = valbool;
			break;

	 	case SO_NO_CHECK:
			sk->no_check = valbool;
			break;

		case SO_PRIORITY:
			if ((val >= 0 && val <= 6) || capable(CAP_NET_ADMIN)) 
				sk->priority = val;
			else
				ret = -EPERM;
			break;

		case SO_LINGER:
			if(optlen<sizeof(ling)) {
				ret = -EINVAL;	/* 1003.1g */
				break;
			}
			if (copy_from_user(&ling,optval,sizeof(ling))) {
				ret = -EFAULT;
				break;
			}
			if(ling.l_onoff==0) {
				sk->linger=0;
			} else {
#if (BITS_PER_LONG == 32)
				if (ling.l_linger >= MAX_SCHEDULE_TIMEOUT/HZ)
					sk->lingertime=MAX_SCHEDULE_TIMEOUT;
				else
#endif
					sk->lingertime=ling.l_linger*HZ;
				sk->linger=1;
			}
			break;

		case SO_BSDCOMPAT:
			sk->bsdism = valbool;
			break;

		case SO_PASSCRED:
			sock->passcred = valbool;
			break;

		case SO_TIMESTAMP:
			sk->rcvtstamp = valbool;
			break;

		case SO_RCVLOWAT:
			if (val < 0)
				val = INT_MAX;
			sk->rcvlowat = val ? : 1;
			break;

		case SO_RCVTIMEO:
			ret = sock_set_timeout(&sk->rcvtimeo, optval, optlen);
			break;

		case SO_SNDTIMEO:
			ret = sock_set_timeout(&sk->sndtimeo, optval, optlen);
			break;

#ifdef CONFIG_NETDEVICES
		case SO_BINDTODEVICE:
		{
			char devname[IFNAMSIZ]; 

			/* Sorry... */ 
			if (!capable(CAP_NET_RAW)) {
				ret = -EPERM;
				break;
			}

			/* Bind this socket to a particular device like "eth0",
			 * as specified in the passed interface name. If the
			 * name is "" or the option length is zero the socket 
			 * is not bound. 
			 */ 

			if (!valbool) {
				sk->bound_dev_if = 0;
			} else {
				if (optlen > IFNAMSIZ) 
					optlen = IFNAMSIZ; 
				if (copy_from_user(devname, optval, optlen)) {
					ret = -EFAULT;
					break;
				}

				/* Remove any cached route for this socket. */
				sk_dst_reset(sk);

				if (devname[0] == '\0') {
					sk->bound_dev_if = 0;
				} else {
					struct net_device *dev = dev_get_by_name(devname);
					if (!dev) {
						ret = -ENODEV;
						break;
					}
					sk->bound_dev_if = dev->ifindex;
					dev_put(dev);
				}
			}
			break;
		}
#endif


#ifdef CONFIG_FILTER
		case SO_ATTACH_FILTER:
			ret = -EINVAL;
			if (optlen == sizeof(struct sock_fprog)) {
				struct sock_fprog fprog;

				ret = -EFAULT;
				if (copy_from_user(&fprog, optval, sizeof(fprog)))
					break;

				ret = sk_attach_filter(&fprog, sk);
			}
			break;

		case SO_DETACH_FILTER:
			spin_lock_bh(&sk->lock.slock);
			filter = sk->filter;
                        if (filter) {
				sk->filter = NULL;
				spin_unlock_bh(&sk->lock.slock);
				sk_filter_release(sk, filter);
				break;
			}
			spin_unlock_bh(&sk->lock.slock);
			ret = -ENONET;
			break;
#endif
		/* We implement the SO_SNDLOWAT etc to
		   not be settable (1003.1g 5.3) */
		default:
		  	ret = -ENOPROTOOPT;
			break;
  	}
	release_sock(sk);
	return ret;
}


int sock_getsockopt(struct socket *sock, int level, int optname,
		    char *optval, int *optlen)
{
	struct sock *sk = sock->sk;
	
	union
	{
  		int val;
  		struct linger ling;
		struct timeval tm;
	} v;
	
	unsigned int lv=sizeof(int),len;
  	
  	if(get_user(len,optlen))
  		return -EFAULT;
	if(len < 0)
		return -EINVAL;
		
  	switch(optname) 
  	{
		case SO_DEBUG:		
			v.val = sk->debug;
			break;
		
		case SO_DONTROUTE:
			v.val = sk->localroute;
			break;
		
		case SO_BROADCAST:
			v.val= sk->broadcast;
			break;

		case SO_SNDBUF:
			v.val=sk->sndbuf;
			break;
		
		case SO_RCVBUF:
			v.val =sk->rcvbuf;
			break;

		case SO_REUSEADDR:
			v.val = sk->reuse;
			break;

		case SO_KEEPALIVE:
			v.val = sk->keepopen;
			break;

		case SO_TYPE:
			v.val = sk->type;		  		
			break;

		case SO_ERROR:
			v.val = -sock_error(sk);
			if(v.val==0)
				v.val=xchg(&sk->err_soft,0);
			break;

		case SO_OOBINLINE:
			v.val = sk->urginline;
			break;
	
		case SO_NO_CHECK:
			v.val = sk->no_check;
			break;

		case SO_PRIORITY:
			v.val = sk->priority;
			break;
		
		case SO_LINGER:	
			lv=sizeof(v.ling);
			v.ling.l_onoff=sk->linger;
 			v.ling.l_linger=sk->lingertime/HZ;
			break;
					
		case SO_BSDCOMPAT:
			v.val = sk->bsdism;
			break;

		case SO_TIMESTAMP:
			v.val = sk->rcvtstamp;
			break;

		case SO_RCVTIMEO:
			lv=sizeof(struct timeval);
			if (sk->rcvtimeo == MAX_SCHEDULE_TIMEOUT) {
				v.tm.tv_sec = 0;
				v.tm.tv_usec = 0;
			} else {
				v.tm.tv_sec = sk->rcvtimeo/HZ;
				v.tm.tv_usec = ((sk->rcvtimeo%HZ)*1000)/HZ;
			}
			break;

		case SO_SNDTIMEO:
			lv=sizeof(struct timeval);
			if (sk->sndtimeo == MAX_SCHEDULE_TIMEOUT) {
				v.tm.tv_sec = 0;
				v.tm.tv_usec = 0;
			} else {
				v.tm.tv_sec = sk->sndtimeo/HZ;
				v.tm.tv_usec = ((sk->sndtimeo%HZ)*1000)/HZ;
			}
			break;

		case SO_RCVLOWAT:
			v.val = sk->rcvlowat;
			break;

		case SO_SNDLOWAT:
			v.val=1;
			break; 

		case SO_PASSCRED:
			v.val = sock->passcred;
			break;

		case SO_PEERCRED:
			if (len > sizeof(sk->peercred))
				len = sizeof(sk->peercred);
			if (copy_to_user(optval, &sk->peercred, len))
				return -EFAULT;
			goto lenout;

		case SO_PEERNAME:
		{
			char address[128];

			if (sock->ops->getname(sock, (struct sockaddr *)address, &lv, 2))
				return -ENOTCONN;
			if (lv < len)
				return -EINVAL;
			if(copy_to_user((void*)optval, address, len))
				return -EFAULT;
			goto lenout;
		}

		/* Dubious BSD thing... Probably nobody even uses it, but
		 * the UNIX standard wants it for whatever reason... -DaveM
		 */
		case SO_ACCEPTCONN:
			v.val = (sk->state == TCP_LISTEN);
			break;

		default:
			return(-ENOPROTOOPT);
	}
	if (len > lv)
		len = lv;
	if (copy_to_user(optval, &v, len))
		return -EFAULT;
lenout:
  	if (put_user(len, optlen))
  		return -EFAULT;
  	return 0;
}

static kmem_cache_t *sk_cachep;

/*
 *	All socket objects are allocated here. This is for future
 *	usage.
 */
 
struct sock *sk_alloc(int family, int priority, int zero_it)
{
	struct sock *sk = kmem_cache_alloc(sk_cachep, priority);

	if(sk && zero_it) {
		memset(sk, 0, sizeof(struct sock));
		sk->family = family;
		sock_lock_init(sk);
	}

	return sk;
}

void sk_free(struct sock *sk)
{
#ifdef CONFIG_FILTER
	struct sk_filter *filter;
#endif

	if (sk->destruct)
		sk->destruct(sk);

#ifdef CONFIG_FILTER
	filter = sk->filter;
	if (filter) {
		sk_filter_release(sk, filter);
		sk->filter = NULL;
	}
#endif

	if (atomic_read(&sk->omem_alloc))
		printk(KERN_DEBUG "sk_free: optmem leakage (%d bytes) detected.\n", atomic_read(&sk->omem_alloc));

	kmem_cache_free(sk_cachep, sk);
}

void __init sk_init(void)
{
	sk_cachep = kmem_cache_create("sock", sizeof(struct sock), 0,
				      SLAB_HWCACHE_ALIGN, 0, 0);
	if (!sk_cachep)
		printk(KERN_CRIT "sk_init: Cannot create sock SLAB cache!");

	if (num_physpages <= 4096) {
		sysctl_wmem_max = 32767;
		sysctl_rmem_max = 32767;
		sysctl_wmem_default = 32767;
		sysctl_rmem_default = 32767;
	} else if (num_physpages >= 131072) {
		sysctl_wmem_max = 131071;
		sysctl_rmem_max = 131071;
	}
}

/*
 *	Simple resource managers for sockets.
 */


/* 
 * Write buffer destructor automatically called from kfree_skb. 
 */
void sock_wfree(struct sk_buff *skb)
{
	struct sock *sk = skb->sk;

	/* In case it might be waiting for more memory. */
	atomic_sub(skb->truesize, &sk->wmem_alloc);
	if (!sk->use_write_queue)
		sk->write_space(sk);
	sock_put(sk);
}

/* 
 * Read buffer destructor automatically called from kfree_skb. 
 */
void sock_rfree(struct sk_buff *skb)
{
	struct sock *sk = skb->sk;

	atomic_sub(skb->truesize, &sk->rmem_alloc);
}

/*
 * Allocate a skb from the socket's send buffer.
 */
struct sk_buff *sock_wmalloc(struct sock *sk, unsigned long size, int force, int priority)
{
	if (force || atomic_read(&sk->wmem_alloc) < sk->sndbuf) {
		struct sk_buff * skb = alloc_skb(size, priority);
		if (skb) {
			skb_set_owner_w(skb, sk);
			return skb;
		}
	}
	return NULL;
}

/*
 * Allocate a skb from the socket's receive buffer.
 */ 
struct sk_buff *sock_rmalloc(struct sock *sk, unsigned long size, int force, int priority)
{
	if (force || atomic_read(&sk->rmem_alloc) < sk->rcvbuf) {
		struct sk_buff *skb = alloc_skb(size, priority);
		if (skb) {
			skb_set_owner_r(skb, sk);
			return skb;
		}
	}
	return NULL;
}

/* 
 * Allocate a memory block from the socket's option memory buffer.
 */ 
void *sock_kmalloc(struct sock *sk, int size, int priority)
{
	if ((unsigned)size <= sysctl_optmem_max &&
	    atomic_read(&sk->omem_alloc)+size < sysctl_optmem_max) {
		void *mem;
		/* First do the add, to avoid the race if kmalloc
 		 * might sleep.
		 */
		atomic_add(size, &sk->omem_alloc);
		mem = kmalloc(size, priority);
		if (mem)
			return mem;
		atomic_sub(size, &sk->omem_alloc);
	}
	return NULL;
}

/*
 * Free an option memory block.
 */
void sock_kfree_s(struct sock *sk, void *mem, int size)
{
	kfree(mem);
	atomic_sub(size, &sk->omem_alloc);
}

/* It is almost wait_for_tcp_memory minus release_sock/lock_sock.
   I think, these locks should be removed for datagram sockets.
 */
static long sock_wait_for_wmem(struct sock * sk, long timeo)
{
	DECLARE_WAITQUEUE(wait, current);

	clear_bit(SOCK_ASYNC_NOSPACE, &sk->socket->flags);
	add_wait_queue(sk->sleep, &wait);
	for (;;) {
		if (!timeo)
			break;
		if (signal_pending(current))
			break;
		set_bit(SOCK_NOSPACE, &sk->socket->flags);
		set_current_state(TASK_INTERRUPTIBLE);
		if (atomic_read(&sk->wmem_alloc) < sk->sndbuf)
			break;
		if (sk->shutdown & SEND_SHUTDOWN)
			break;
		if (sk->err)
			break;
		timeo = schedule_timeout(timeo);
	}
	__set_current_state(TASK_RUNNING);
	remove_wait_queue(sk->sleep, &wait);
	return timeo;
}


/*
 *	Generic send/receive buffer handlers
 */

struct sk_buff *sock_alloc_send_pskb(struct sock *sk, unsigned long header_len,
				     unsigned long data_len, int noblock, int *errcode)
{
	struct sk_buff *skb;
	long timeo;
	int err;

	timeo = sock_sndtimeo(sk, noblock);
	while (1) {
		err = sock_error(sk);
		if (err != 0)
			goto failure;

		err = -EPIPE;
		if (sk->shutdown & SEND_SHUTDOWN)
			goto failure;

		if (atomic_read(&sk->wmem_alloc) < sk->sndbuf) {
			skb = alloc_skb(header_len, sk->allocation);
			if (skb) {
				int npages;
				int i;

				/* No pages, we're done... */
				if (!data_len)
					break;

				npages = (data_len + (PAGE_SIZE - 1)) >> PAGE_SHIFT;
				skb->truesize += data_len;
				skb_shinfo(skb)->nr_frags = npages;
				for (i = 0; i < npages; i++) {
					struct page *page;
					skb_frag_t *frag;

					page = alloc_pages(sk->allocation, 0);
					if (!page) {
						err = -ENOBUFS;
						skb_shinfo(skb)->nr_frags = i;
						kfree_skb(skb);
						goto failure;
					}

					frag = &skb_shinfo(skb)->frags[i];
					frag->page = page;
					frag->page_offset = 0;
					frag->size = (data_len >= PAGE_SIZE ?
						      PAGE_SIZE :
						      data_len);
					data_len -= PAGE_SIZE;
				}

				/* Full success... */
				break;
			}
			err = -ENOBUFS;
			goto failure;
		}
		set_bit(SOCK_ASYNC_NOSPACE, &sk->socket->flags);
		set_bit(SOCK_NOSPACE, &sk->socket->flags);
		err = -EAGAIN;
		if (!timeo)
			goto failure;
		if (signal_pending(current))
			goto interrupted;
		timeo = sock_wait_for_wmem(sk, timeo);
	}

	skb_set_owner_w(skb, sk);
	return skb;

interrupted:
	err = sock_intr_errno(timeo);
failure:
	*errcode = err;
	return NULL;
}

struct sk_buff *sock_alloc_send_skb(struct sock *sk, unsigned long size, 
				    int noblock, int *errcode)
{
	return sock_alloc_send_pskb(sk, size, 0, noblock, errcode);
}

void __lock_sock(struct sock *sk)
{
	DECLARE_WAITQUEUE(wait, current);

	add_wait_queue_exclusive(&sk->lock.wq, &wait);
	for(;;) {
		current->state = TASK_UNINTERRUPTIBLE;
		spin_unlock_bh(&sk->lock.slock);
		schedule();
		spin_lock_bh(&sk->lock.slock);
		if(!sk->lock.users)
			break;
	}
	current->state = TASK_RUNNING;
	remove_wait_queue(&sk->lock.wq, &wait);
}

void __release_sock(struct sock *sk)
{
	struct sk_buff *skb = sk->backlog.head;

	do {
		sk->backlog.head = sk->backlog.tail = NULL;
		bh_unlock_sock(sk);

		do {
			struct sk_buff *next = skb->next;

			skb->next = NULL;
			sk->backlog_rcv(sk, skb);
			skb = next;
		} while (skb != NULL);

		bh_lock_sock(sk);
	} while((skb = sk->backlog.head) != NULL);
}

/*
 *	Generic socket manager library. Most simpler socket families
 *	use this to manage their socket lists. At some point we should
 *	hash these. By making this generic we get the lot hashed for free.
 *
 *	It is broken by design. All the protocols using it must be fixed. --ANK
 */

rwlock_t net_big_sklist_lock = RW_LOCK_UNLOCKED;
 
void sklist_remove_socket(struct sock **list, struct sock *sk)
{
	struct sock *s;

	write_lock_bh(&net_big_sklist_lock);

	while ((s = *list) != NULL) {
		if (s == sk) {
			*list = s->next;
			break;
		}
		list = &s->next;
	}

	write_unlock_bh(&net_big_sklist_lock);
	if (s)
		sock_put(s);
}

void sklist_insert_socket(struct sock **list, struct sock *sk)
{
	write_lock_bh(&net_big_sklist_lock);
	sk->next= *list;
	*list=sk;
	sock_hold(sk);
	write_unlock_bh(&net_big_sklist_lock);
}

/*
 *	This is only called from user mode. Thus it protects itself against
 *	interrupt users but doesn't worry about being called during work.
 *	Once it is removed from the queue no interrupt or bottom half will
 *	touch it and we are (fairly 8-) ) safe.
 */

void sklist_destroy_socket(struct sock **list, struct sock *sk);

/*
 *	Handler for deferred kills.
 */

static void sklist_destroy_timer(unsigned long data)
{
	struct sock *sk=(struct sock *)data;
	sklist_destroy_socket(NULL,sk);
}

/*
 *	Destroy a socket. We pass NULL for a list if we know the
 *	socket is not on a list.
 */
 
void sklist_destroy_socket(struct sock **list,struct sock *sk)
{
	if(list)
		sklist_remove_socket(list, sk);

	skb_queue_purge(&sk->receive_queue);

	if(atomic_read(&sk->wmem_alloc) == 0 &&
	   atomic_read(&sk->rmem_alloc) == 0 &&
	   sk->dead)
	{
		sock_put(sk);
	}
	else
	{
		/*
		 *	Someone is using our buffers still.. defer
		 */
		init_timer(&sk->timer);
		sk->timer.expires=jiffies+SOCK_DESTROY_TIME;
		sk->timer.function=sklist_destroy_timer;
		sk->timer.data = (unsigned long)sk;
		add_timer(&sk->timer);
	}
}

/*
 * Set of default routines for initialising struct proto_ops when
 * the protocol does not support a particular function. In certain
 * cases where it makes no sense for a protocol to have a "do nothing"
 * function, some default processing is provided.
 */

int sock_no_release(struct socket *sock)
{
	return 0;
}

int sock_no_bind(struct socket *sock, struct sockaddr *saddr, int len)
{
	return -EOPNOTSUPP;
}

int sock_no_connect(struct socket *sock, struct sockaddr *saddr, 
		    int len, int flags)
{
	return -EOPNOTSUPP;
}

int sock_no_socketpair(struct socket *sock1, struct socket *sock2)
{
	return -EOPNOTSUPP;
}

int sock_no_accept(struct socket *sock, struct socket *newsock, int flags)
{
	return -EOPNOTSUPP;
}

int sock_no_getname(struct socket *sock, struct sockaddr *saddr, 
		    int *len, int peer)
{
	return -EOPNOTSUPP;
}

unsigned int sock_no_poll(struct file * file, struct socket *sock, poll_table *pt)
{
	return 0;
}

int sock_no_ioctl(struct socket *sock, unsigned int cmd, unsigned long arg)
{
	return -EOPNOTSUPP;
}

int sock_no_listen(struct socket *sock, int backlog)
{
	return -EOPNOTSUPP;
}

int sock_no_shutdown(struct socket *sock, int how)
{
	return -EOPNOTSUPP;
}

int sock_no_setsockopt(struct socket *sock, int level, int optname,
		    char *optval, int optlen)
{
	return -EOPNOTSUPP;
}

int sock_no_getsockopt(struct socket *sock, int level, int optname,
		    char *optval, int *optlen)
{
	return -EOPNOTSUPP;
}

/* 
 * Note: if you add something that sleeps here then change sock_fcntl()
 *       to do proper fd locking.
 */
int sock_no_fcntl(struct socket *sock, unsigned int cmd, unsigned long arg)
{
	struct sock *sk = sock->sk;

	switch(cmd)
	{
		case F_SETOWN:
			/*
			 * This is a little restrictive, but it's the only
			 * way to make sure that you can't send a sigurg to
			 * another process.
			 */
			if (current->pgrp != -arg &&
				current->pid != arg &&
				!capable(CAP_KILL)) return(-EPERM);
			sk->proc = arg;
			return(0);
		case F_GETOWN:
			return(sk->proc);
		default:
			return(-EINVAL);
	}
}

int sock_no_sendmsg(struct socket *sock, struct msghdr *m, int flags,
		    struct scm_cookie *scm)
{
	return -EOPNOTSUPP;
}

int sock_no_recvmsg(struct socket *sock, struct msghdr *m, int len, int flags,
		    struct scm_cookie *scm)
{
	return -EOPNOTSUPP;
}

int sock_no_mmap(struct file *file, struct socket *sock, struct vm_area_struct *vma)
{
	/* Mirror missing mmap method error code */
	return -ENODEV;
}

ssize_t sock_no_sendpage(struct socket *sock, struct page *page, int offset, size_t size, int flags)
{
	ssize_t res;
	struct msghdr msg;
	struct iovec iov;
	mm_segment_t old_fs;
	char *kaddr;

	kaddr = kmap(page);

	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	msg.msg_flags = flags;

	iov.iov_base = kaddr + offset;
	iov.iov_len = size;

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	res = sock_sendmsg(sock, &msg, size);
	set_fs(old_fs);

	kunmap(page);
	return res;
}

/*
 *	Default Socket Callbacks
 */

void sock_def_wakeup(struct sock *sk)
{
	read_lock(&sk->callback_lock);
	if (sk->sleep && waitqueue_active(sk->sleep))
		wake_up_interruptible_all(sk->sleep);
	read_unlock(&sk->callback_lock);
}

void sock_def_error_report(struct sock *sk)
{
	read_lock(&sk->callback_lock);
	if (sk->sleep && waitqueue_active(sk->sleep))
		wake_up_interruptible(sk->sleep);
	sk_wake_async(sk,0,POLL_ERR); 
	read_unlock(&sk->callback_lock);
}

void sock_def_readable(struct sock *sk, int len)
{
	read_lock(&sk->callback_lock);
	if (sk->sleep && waitqueue_active(sk->sleep))
		wake_up_interruptible(sk->sleep);
	sk_wake_async(sk,1,POLL_IN);
	read_unlock(&sk->callback_lock);
}

void sock_def_write_space(struct sock *sk)
{
	read_lock(&sk->callback_lock);

	/* Do not wake up a writer until he can make "significant"
	 * progress.  --DaveM
	 */
	if((atomic_read(&sk->wmem_alloc) << 1) <= sk->sndbuf) {
		if (sk->sleep && waitqueue_active(sk->sleep))
			wake_up_interruptible(sk->sleep);

		/* Should agree with poll, otherwise some programs break */
		if (sock_writeable(sk))
			sk_wake_async(sk, 2, POLL_OUT);
	}

	read_unlock(&sk->callback_lock);
}

void sock_def_destruct(struct sock *sk)
{
	if (sk->protinfo.destruct_hook)
		kfree(sk->protinfo.destruct_hook);
}

void sock_init_data(struct socket *sock, struct sock *sk)
{
	skb_queue_head_init(&sk->receive_queue);
	skb_queue_head_init(&sk->write_queue);
	skb_queue_head_init(&sk->error_queue);

	init_timer(&sk->timer);
	
	sk->allocation	=	GFP_KERNEL;
	sk->rcvbuf	=	sysctl_rmem_default;
	sk->sndbuf	=	sysctl_wmem_default;
	sk->state 	= 	TCP_CLOSE;
	sk->zapped	=	1;
	sk->socket	=	sock;

	if(sock)
	{
		sk->type	=	sock->type;
		sk->sleep	=	&sock->wait;
		sock->sk	=	sk;
	} else
		sk->sleep	=	NULL;

	sk->dst_lock		=	RW_LOCK_UNLOCKED;
	sk->callback_lock	=	RW_LOCK_UNLOCKED;

	sk->state_change	=	sock_def_wakeup;
	sk->data_ready		=	sock_def_readable;
	sk->write_space		=	sock_def_write_space;
	sk->error_report	=	sock_def_error_report;
	sk->destruct            =       sock_def_destruct;

	sk->peercred.pid 	=	0;
	sk->peercred.uid	=	-1;
	sk->peercred.gid	=	-1;
	sk->rcvlowat		=	1;
	sk->rcvtimeo		=	MAX_SCHEDULE_TIMEOUT;
	sk->sndtimeo		=	MAX_SCHEDULE_TIMEOUT;

	atomic_set(&sk->refcnt, 1);
}
