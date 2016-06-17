/*
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  INET is implemented using the  BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		Definitions for the TCP module.
 *
 * Version:	@(#)tcp.h	1.0.5	05/23/93
 *
 * Authors:	Ross Biro, <bir7@leland.Stanford.Edu>
 *		Fred N. van Kempen, <waltje@uWalt.NL.Mugnet.ORG>
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 */
#ifndef _TCP_H
#define _TCP_H

#define TCP_DEBUG 1
#define FASTRETRANS_DEBUG 1

/* Cancel timers, when they are not required. */
#undef TCP_CLEAR_TIMERS

#include <linux/config.h>
#include <linux/tcp.h>
#include <linux/slab.h>
#include <linux/cache.h>
#include <net/checksum.h>
#include <net/sock.h>
#include <net/snmp.h>

/* This is for all connections with a full identity, no wildcards.
 * New scheme, half the table is for TIME_WAIT, the other half is
 * for the rest.  I'll experiment with dynamic table growth later.
 */
struct tcp_ehash_bucket {
	rwlock_t	lock;
	struct sock	*chain;
} __attribute__((__aligned__(8)));

/* This is for listening sockets, thus all sockets which possess wildcards. */
#define TCP_LHTABLE_SIZE	32	/* Yes, really, this is all you need. */

/* There are a few simple rules, which allow for local port reuse by
 * an application.  In essence:
 *
 *	1) Sockets bound to different interfaces may share a local port.
 *	   Failing that, goto test 2.
 *	2) If all sockets have sk->reuse set, and none of them are in
 *	   TCP_LISTEN state, the port may be shared.
 *	   Failing that, goto test 3.
 *	3) If all sockets are bound to a specific sk->rcv_saddr local
 *	   address, and none of them are the same, the port may be
 *	   shared.
 *	   Failing this, the port cannot be shared.
 *
 * The interesting point, is test #2.  This is what an FTP server does
 * all day.  To optimize this case we use a specific flag bit defined
 * below.  As we add sockets to a bind bucket list, we perform a
 * check of: (newsk->reuse && (newsk->state != TCP_LISTEN))
 * As long as all sockets added to a bind bucket pass this test,
 * the flag bit will be set.
 * The resulting situation is that tcp_v[46]_verify_bind() can just check
 * for this flag bit, if it is set and the socket trying to bind has
 * sk->reuse set, we don't even have to walk the owners list at all,
 * we return that it is ok to bind this socket to the requested local port.
 *
 * Sounds like a lot of work, but it is worth it.  In a more naive
 * implementation (ie. current FreeBSD etc.) the entire list of ports
 * must be walked for each data port opened by an ftp server.  Needless
 * to say, this does not scale at all.  With a couple thousand FTP
 * users logged onto your box, isn't it nice to know that new data
 * ports are created in O(1) time?  I thought so. ;-)	-DaveM
 */
struct tcp_bind_bucket {
	unsigned short		port;
	signed short		fastreuse;
	struct tcp_bind_bucket	*next;
	struct sock		*owners;
	struct tcp_bind_bucket	**pprev;
};

struct tcp_bind_hashbucket {
	spinlock_t		lock;
	struct tcp_bind_bucket	*chain;
};

extern struct tcp_hashinfo {
	/* This is for sockets with full identity only.  Sockets here will
	 * always be without wildcards and will have the following invariant:
	 *
	 *          TCP_ESTABLISHED <= sk->state < TCP_CLOSE
	 *
	 * First half of the table is for sockets not in TIME_WAIT, second half
	 * is for TIME_WAIT sockets only.
	 */
	struct tcp_ehash_bucket *__tcp_ehash;

	/* Ok, let's try this, I give up, we do need a local binding
	 * TCP hash as well as the others for fast bind/connect.
	 */
	struct tcp_bind_hashbucket *__tcp_bhash;

	int __tcp_bhash_size;
	int __tcp_ehash_size;

	/* All sockets in TCP_LISTEN state will be in here.  This is the only
	 * table where wildcard'd TCP sockets can exist.  Hash function here
	 * is just local port number.
	 */
	struct sock *__tcp_listening_hash[TCP_LHTABLE_SIZE];

	/* All the above members are written once at bootup and
	 * never written again _or_ are predominantly read-access.
	 *
	 * Now align to a new cache line as all the following members
	 * are often dirty.
	 */
	rwlock_t __tcp_lhash_lock ____cacheline_aligned;
	atomic_t __tcp_lhash_users;
	wait_queue_head_t __tcp_lhash_wait;
	spinlock_t __tcp_portalloc_lock;
} tcp_hashinfo;

#define tcp_ehash	(tcp_hashinfo.__tcp_ehash)
#define tcp_bhash	(tcp_hashinfo.__tcp_bhash)
#define tcp_ehash_size	(tcp_hashinfo.__tcp_ehash_size)
#define tcp_bhash_size	(tcp_hashinfo.__tcp_bhash_size)
#define tcp_listening_hash (tcp_hashinfo.__tcp_listening_hash)
#define tcp_lhash_lock	(tcp_hashinfo.__tcp_lhash_lock)
#define tcp_lhash_users	(tcp_hashinfo.__tcp_lhash_users)
#define tcp_lhash_wait	(tcp_hashinfo.__tcp_lhash_wait)
#define tcp_portalloc_lock (tcp_hashinfo.__tcp_portalloc_lock)

extern kmem_cache_t *tcp_bucket_cachep;
extern struct tcp_bind_bucket *tcp_bucket_create(struct tcp_bind_hashbucket *head,
						 unsigned short snum);
extern void tcp_bucket_unlock(struct sock *sk);
extern int tcp_port_rover;
extern struct sock *tcp_v4_lookup_listener(u32 addr, unsigned short hnum, int dif);

/* These are AF independent. */
static __inline__ int tcp_bhashfn(__u16 lport)
{
	return (lport & (tcp_bhash_size - 1));
}

/* This is a TIME_WAIT bucket.  It works around the memory consumption
 * problems of sockets in such a state on heavily loaded servers, but
 * without violating the protocol specification.
 */
struct tcp_tw_bucket {
	/* These _must_ match the beginning of struct sock precisely.
	 * XXX Yes I know this is gross, but I'd have to edit every single
	 * XXX networking file if I created a "struct sock_header". -DaveM
	 */
	__u32			daddr;
	__u32			rcv_saddr;
	__u16			dport;
	unsigned short		num;
	int			bound_dev_if;
	struct sock		*next;
	struct sock		**pprev;
	struct sock		*bind_next;
	struct sock		**bind_pprev;
	unsigned char		state,
				substate; /* "zapped" is replaced with "substate" */
	__u16			sport;
	unsigned short		family;
	unsigned char		reuse,
				rcv_wscale; /* It is also TW bucket specific */
	atomic_t		refcnt;

	/* And these are ours. */
	int			hashent;
	int			timeout;
	__u32			rcv_nxt;
	__u32			snd_nxt;
	__u32			rcv_wnd;
        __u32			ts_recent;
        long			ts_recent_stamp;
	unsigned long		ttd;
	struct tcp_bind_bucket	*tb;
	struct tcp_tw_bucket	*next_death;
	struct tcp_tw_bucket	**pprev_death;

#if defined(CONFIG_IPV6) || defined(CONFIG_IPV6_MODULE)
	struct in6_addr		v6_daddr;
	struct in6_addr		v6_rcv_saddr;
#endif
};

extern kmem_cache_t *tcp_timewait_cachep;

static inline void tcp_tw_put(struct tcp_tw_bucket *tw)
{
	if (atomic_dec_and_test(&tw->refcnt)) {
#ifdef INET_REFCNT_DEBUG
		printk(KERN_DEBUG "tw_bucket %p released\n", tw);
#endif
		kmem_cache_free(tcp_timewait_cachep, tw);
	}
}

extern atomic_t tcp_orphan_count;
extern int tcp_tw_count;
extern void tcp_time_wait(struct sock *sk, int state, int timeo);
extern void tcp_timewait_kill(struct tcp_tw_bucket *tw);
extern void tcp_tw_schedule(struct tcp_tw_bucket *tw, int timeo);
extern void tcp_tw_deschedule(struct tcp_tw_bucket *tw);


/* Socket demux engine toys. */
#ifdef __BIG_ENDIAN
#define TCP_COMBINED_PORTS(__sport, __dport) \
	(((__u32)(__sport)<<16) | (__u32)(__dport))
#else /* __LITTLE_ENDIAN */
#define TCP_COMBINED_PORTS(__sport, __dport) \
	(((__u32)(__dport)<<16) | (__u32)(__sport))
#endif

#if (BITS_PER_LONG == 64)
#ifdef __BIG_ENDIAN
#define TCP_V4_ADDR_COOKIE(__name, __saddr, __daddr) \
	__u64 __name = (((__u64)(__saddr))<<32)|((__u64)(__daddr));
#else /* __LITTLE_ENDIAN */
#define TCP_V4_ADDR_COOKIE(__name, __saddr, __daddr) \
	__u64 __name = (((__u64)(__daddr))<<32)|((__u64)(__saddr));
#endif /* __BIG_ENDIAN */
#define TCP_IPV4_MATCH(__sk, __cookie, __saddr, __daddr, __ports, __dif)\
	(((*((__u64 *)&((__sk)->daddr)))== (__cookie))	&&		\
	 ((*((__u32 *)&((__sk)->dport)))== (__ports))   &&		\
	 (!((__sk)->bound_dev_if) || ((__sk)->bound_dev_if == (__dif))))
#else /* 32-bit arch */
#define TCP_V4_ADDR_COOKIE(__name, __saddr, __daddr)
#define TCP_IPV4_MATCH(__sk, __cookie, __saddr, __daddr, __ports, __dif)\
	(((__sk)->daddr			== (__saddr))	&&		\
	 ((__sk)->rcv_saddr		== (__daddr))	&&		\
	 ((*((__u32 *)&((__sk)->dport)))== (__ports))   &&		\
	 (!((__sk)->bound_dev_if) || ((__sk)->bound_dev_if == (__dif))))
#endif /* 64-bit arch */

#define TCP_IPV6_MATCH(__sk, __saddr, __daddr, __ports, __dif)			   \
	(((*((__u32 *)&((__sk)->dport)))== (__ports))   			&& \
	 ((__sk)->family		== AF_INET6)				&& \
	 !ipv6_addr_cmp(&(__sk)->net_pinfo.af_inet6.daddr, (__saddr))		&& \
	 !ipv6_addr_cmp(&(__sk)->net_pinfo.af_inet6.rcv_saddr, (__daddr))	&& \
	 (!((__sk)->bound_dev_if) || ((__sk)->bound_dev_if == (__dif))))

/* These can have wildcards, don't try too hard. */
static __inline__ int tcp_lhashfn(unsigned short num)
{
	return num & (TCP_LHTABLE_SIZE - 1);
}

static __inline__ int tcp_sk_listen_hashfn(struct sock *sk)
{
	return tcp_lhashfn(sk->num);
}

#define MAX_TCP_HEADER	(128 + MAX_HEADER)

/* 
 * Never offer a window over 32767 without using window scaling. Some
 * poor stacks do signed 16bit maths! 
 */
#define MAX_TCP_WINDOW		32767U

/* Minimal accepted MSS. It is (60+60+8) - (20+20). */
#define TCP_MIN_MSS		88U

/* Minimal RCV_MSS. */
#define TCP_MIN_RCVMSS		536U

/* After receiving this amount of duplicate ACKs fast retransmit starts. */
#define TCP_FASTRETRANS_THRESH 3

/* Maximal reordering. */
#define TCP_MAX_REORDERING	127

/* Maximal number of ACKs sent quickly to accelerate slow-start. */
#define TCP_MAX_QUICKACKS	16U

/* urg_data states */
#define TCP_URG_VALID	0x0100
#define TCP_URG_NOTYET	0x0200
#define TCP_URG_READ	0x0400

#define TCP_RETR1	3	/*
				 * This is how many retries it does before it
				 * tries to figure out if the gateway is
				 * down. Minimal RFC value is 3; it corresponds
				 * to ~3sec-8min depending on RTO.
				 */

#define TCP_RETR2	15	/*
				 * This should take at least
				 * 90 minutes to time out.
				 * RFC1122 says that the limit is 100 sec.
				 * 15 is ~13-30min depending on RTO.
				 */

#define TCP_SYN_RETRIES	 5	/* number of times to retry active opening a
				 * connection: ~180sec is RFC minumum	*/

#define TCP_SYNACK_RETRIES 5	/* number of times to retry passive opening a
				 * connection: ~180sec is RFC minumum	*/


#define TCP_ORPHAN_RETRIES 7	/* number of times to retry on an orphaned
				 * socket. 7 is ~50sec-16min.
				 */


#define TCP_TIMEWAIT_LEN (60*HZ) /* how long to wait to destroy TIME-WAIT
				  * state, about 60 seconds	*/
#define TCP_FIN_TIMEOUT	TCP_TIMEWAIT_LEN
                                 /* BSD style FIN_WAIT2 deadlock breaker.
				  * It used to be 3min, new value is 60sec,
				  * to combine FIN-WAIT-2 timeout with
				  * TIME-WAIT timer.
				  */

#define TCP_DELACK_MAX	((unsigned)(HZ/5))	/* maximal time to delay before sending an ACK */
#if HZ >= 100
#define TCP_DELACK_MIN	((unsigned)(HZ/25))	/* minimal time to delay before sending an ACK */
#define TCP_ATO_MIN	((unsigned)(HZ/25))
#else
#define TCP_DELACK_MIN	4U
#define TCP_ATO_MIN	4U
#endif
#define TCP_RTO_MAX	((unsigned)(120*HZ))
#define TCP_RTO_MIN	((unsigned)(HZ/5))
#define TCP_TIMEOUT_INIT ((unsigned)(3*HZ))	/* RFC 1122 initial RTO value	*/

#define TCP_RESOURCE_PROBE_INTERVAL ((unsigned)(HZ/2U)) /* Maximal interval between probes
					                 * for local resources.
					                 */

#define TCP_KEEPALIVE_TIME	(120*60*HZ)	/* two hours */
#define TCP_KEEPALIVE_PROBES	9		/* Max of 9 keepalive probes	*/
#define TCP_KEEPALIVE_INTVL	(75*HZ)

#define MAX_TCP_KEEPIDLE	32767
#define MAX_TCP_KEEPINTVL	32767
#define MAX_TCP_KEEPCNT		127
#define MAX_TCP_SYNCNT		127

/* TIME_WAIT reaping mechanism. */
#define TCP_TWKILL_SLOTS	8	/* Please keep this a power of 2. */
#define TCP_TWKILL_PERIOD	(TCP_TIMEWAIT_LEN/TCP_TWKILL_SLOTS)

#define TCP_SYNQ_INTERVAL	(HZ/5)	/* Period of SYNACK timer */
#define TCP_SYNQ_HSIZE		512	/* Size of SYNACK hash table */

#define TCP_PAWS_24DAYS	(60 * 60 * 24 * 24)
#define TCP_PAWS_MSL	60		/* Per-host timestamps are invalidated
					 * after this time. It should be equal
					 * (or greater than) TCP_TIMEWAIT_LEN
					 * to provide reliability equal to one
					 * provided by timewait state.
					 */
#define TCP_PAWS_WINDOW	1		/* Replay window for per-host
					 * timestamps. It must be less than
					 * minimal timewait lifetime.
					 */

#define TCP_TW_RECYCLE_SLOTS_LOG	5
#define TCP_TW_RECYCLE_SLOTS		(1<<TCP_TW_RECYCLE_SLOTS_LOG)

/* If time > 4sec, it is "slow" path, no recycling is required,
   so that we select tick to get range about 4 seconds.
 */

#if HZ <= 16 || HZ > 4096
# error Unsupported: HZ <= 16 or HZ > 4096
#elif HZ <= 32
# define TCP_TW_RECYCLE_TICK (5+2-TCP_TW_RECYCLE_SLOTS_LOG)
#elif HZ <= 64
# define TCP_TW_RECYCLE_TICK (6+2-TCP_TW_RECYCLE_SLOTS_LOG)
#elif HZ <= 128
# define TCP_TW_RECYCLE_TICK (7+2-TCP_TW_RECYCLE_SLOTS_LOG)
#elif HZ <= 256
# define TCP_TW_RECYCLE_TICK (8+2-TCP_TW_RECYCLE_SLOTS_LOG)
#elif HZ <= 512
# define TCP_TW_RECYCLE_TICK (9+2-TCP_TW_RECYCLE_SLOTS_LOG)
#elif HZ <= 1024
# define TCP_TW_RECYCLE_TICK (10+2-TCP_TW_RECYCLE_SLOTS_LOG)
#elif HZ <= 2048
# define TCP_TW_RECYCLE_TICK (11+2-TCP_TW_RECYCLE_SLOTS_LOG)
#else
# define TCP_TW_RECYCLE_TICK (12+2-TCP_TW_RECYCLE_SLOTS_LOG)
#endif

/*
 *	TCP option
 */
 
#define TCPOPT_NOP		1	/* Padding */
#define TCPOPT_EOL		0	/* End of options */
#define TCPOPT_MSS		2	/* Segment size negotiating */
#define TCPOPT_WINDOW		3	/* Window scaling */
#define TCPOPT_SACK_PERM        4       /* SACK Permitted */
#define TCPOPT_SACK             5       /* SACK Block */
#define TCPOPT_TIMESTAMP	8	/* Better RTT estimations/PAWS */

/*
 *     TCP option lengths
 */

#define TCPOLEN_MSS            4
#define TCPOLEN_WINDOW         3
#define TCPOLEN_SACK_PERM      2
#define TCPOLEN_TIMESTAMP      10

/* But this is what stacks really send out. */
#define TCPOLEN_TSTAMP_ALIGNED		12
#define TCPOLEN_WSCALE_ALIGNED		4
#define TCPOLEN_SACKPERM_ALIGNED	4
#define TCPOLEN_SACK_BASE		2
#define TCPOLEN_SACK_BASE_ALIGNED	4
#define TCPOLEN_SACK_PERBLOCK		8

#define TCP_TIME_RETRANS	1	/* Retransmit timer */
#define TCP_TIME_DACK		2	/* Delayed ack timer */
#define TCP_TIME_PROBE0		3	/* Zero window probe timer */
#define TCP_TIME_KEEPOPEN	4	/* Keepalive timer */

/* sysctl variables for tcp */
extern int sysctl_max_syn_backlog;
extern int sysctl_tcp_timestamps;
extern int sysctl_tcp_window_scaling;
extern int sysctl_tcp_sack;
extern int sysctl_tcp_fin_timeout;
extern int sysctl_tcp_tw_recycle;
extern int sysctl_tcp_keepalive_time;
extern int sysctl_tcp_keepalive_probes;
extern int sysctl_tcp_keepalive_intvl;
extern int sysctl_tcp_syn_retries;
extern int sysctl_tcp_synack_retries;
extern int sysctl_tcp_retries1;
extern int sysctl_tcp_retries2;
extern int sysctl_tcp_orphan_retries;
extern int sysctl_tcp_syncookies;
extern int sysctl_tcp_retrans_collapse;
extern int sysctl_tcp_stdurg;
extern int sysctl_tcp_rfc1337;
extern int sysctl_tcp_abort_on_overflow;
extern int sysctl_tcp_max_orphans;
extern int sysctl_tcp_max_tw_buckets;
extern int sysctl_tcp_fack;
extern int sysctl_tcp_reordering;
extern int sysctl_tcp_ecn;
extern int sysctl_tcp_dsack;
extern int sysctl_tcp_mem[3];
extern int sysctl_tcp_wmem[3];
extern int sysctl_tcp_rmem[3];
extern int sysctl_tcp_app_win;
extern int sysctl_tcp_adv_win_scale;
extern int sysctl_tcp_tw_reuse;
extern int sysctl_tcp_frto;
extern int sysctl_tcp_low_latency;
extern int sysctl_tcp_westwood;

extern atomic_t tcp_memory_allocated;
extern atomic_t tcp_sockets_allocated;
extern int tcp_memory_pressure;

struct open_request;

struct or_calltable {
	int  family;
	int  (*rtx_syn_ack)	(struct sock *sk, struct open_request *req, struct dst_entry*);
	void (*send_ack)	(struct sk_buff *skb, struct open_request *req);
	void (*destructor)	(struct open_request *req);
	void (*send_reset)	(struct sk_buff *skb);
};

struct tcp_v4_open_req {
	__u32			loc_addr;
	__u32			rmt_addr;
	struct ip_options	*opt;
};

#if defined(CONFIG_IPV6) || defined (CONFIG_IPV6_MODULE)
struct tcp_v6_open_req {
	struct in6_addr		loc_addr;
	struct in6_addr		rmt_addr;
	struct sk_buff		*pktopts;
	int			iif;
};
#endif

/* this structure is too big */
struct open_request {
	struct open_request	*dl_next; /* Must be first member! */
	__u32			rcv_isn;
	__u32			snt_isn;
	__u16			rmt_port;
	__u16			mss;
	__u8			retrans;
	__u8			__pad;
	__u16	snd_wscale : 4, 
		rcv_wscale : 4, 
		tstamp_ok : 1,
		sack_ok : 1,
		wscale_ok : 1,
		ecn_ok : 1,
		acked : 1;
	/* The following two fields can be easily recomputed I think -AK */
	__u32			window_clamp;	/* window clamp at creation time */
	__u32			rcv_wnd;	/* rcv_wnd offered first time */
	__u32			ts_recent;
	unsigned long		expires;
	struct or_calltable	*class;
	struct sock		*sk;
	union {
		struct tcp_v4_open_req v4_req;
#if defined(CONFIG_IPV6) || defined (CONFIG_IPV6_MODULE)
		struct tcp_v6_open_req v6_req;
#endif
	} af;
};

/* SLAB cache for open requests. */
extern kmem_cache_t *tcp_openreq_cachep;

#define tcp_openreq_alloc()		kmem_cache_alloc(tcp_openreq_cachep, SLAB_ATOMIC)
#define tcp_openreq_fastfree(req)	kmem_cache_free(tcp_openreq_cachep, req)

static inline void tcp_openreq_free(struct open_request *req)
{
	req->class->destructor(req);
	tcp_openreq_fastfree(req);
}

#if defined(CONFIG_IPV6) || defined(CONFIG_IPV6_MODULE)
#define TCP_INET_FAMILY(fam) ((fam) == AF_INET)
#else
#define TCP_INET_FAMILY(fam) 1
#endif

/*
 *	Pointers to address related TCP functions
 *	(i.e. things that depend on the address family)
 *
 * 	BUGGG_FUTURE: all the idea behind this struct is wrong.
 *	It mixes socket frontend with transport function.
 *	With port sharing between IPv6/v4 it gives the only advantage,
 *	only poor IPv6 needs to permanently recheck, that it
 *	is still IPv6 8)8) It must be cleaned up as soon as possible.
 *						--ANK (980802)
 */

struct tcp_func {
	int			(*queue_xmit)		(struct sk_buff *skb,
							 int ipfragok);

	void			(*send_check)		(struct sock *sk,
							 struct tcphdr *th,
							 int len,
							 struct sk_buff *skb);

	int			(*rebuild_header)	(struct sock *sk);

	int			(*conn_request)		(struct sock *sk,
							 struct sk_buff *skb);

	struct sock *		(*syn_recv_sock)	(struct sock *sk,
							 struct sk_buff *skb,
							 struct open_request *req,
							 struct dst_entry *dst);
    
	int			(*remember_stamp)	(struct sock *sk);

	__u16			net_header_len;

	int			(*setsockopt)		(struct sock *sk, 
							 int level, 
							 int optname, 
							 char *optval, 
							 int optlen);

	int			(*getsockopt)		(struct sock *sk, 
							 int level, 
							 int optname, 
							 char *optval, 
							 int *optlen);


	void			(*addr2sockaddr)	(struct sock *sk,
							 struct sockaddr *);

	int sockaddr_len;
};

/*
 * The next routines deal with comparing 32 bit unsigned ints
 * and worry about wraparound (automatic with unsigned arithmetic).
 */

static inline int before(__u32 seq1, __u32 seq2)
{
        return (__s32)(seq1-seq2) < 0;
}

static inline int after(__u32 seq1, __u32 seq2)
{
	return (__s32)(seq2-seq1) < 0;
}


/* is s2<=s1<=s3 ? */
static inline int between(__u32 seq1, __u32 seq2, __u32 seq3)
{
	return seq3 - seq2 >= seq1 - seq2;
}


extern struct proto tcp_prot;

extern struct tcp_mib tcp_statistics[NR_CPUS*2];
#define TCP_INC_STATS(field)		SNMP_INC_STATS(tcp_statistics, field)
#define TCP_INC_STATS_BH(field)		SNMP_INC_STATS_BH(tcp_statistics, field)
#define TCP_INC_STATS_USER(field) 	SNMP_INC_STATS_USER(tcp_statistics, field)
#define TCP_ADD_STATS_BH(field, val)	SNMP_ADD_STATS_BH(tcp_statistics, field, val)
#define TCP_ADD_STATS_USER(field, val)	SNMP_ADD_STATS_USER(tcp_statistics, field, val)

extern void			tcp_put_port(struct sock *sk);
extern void			__tcp_put_port(struct sock *sk);
extern void			tcp_inherit_port(struct sock *sk, struct sock *child);

extern void			tcp_v4_err(struct sk_buff *skb, u32);

extern void			tcp_shutdown (struct sock *sk, int how);

extern int			tcp_v4_rcv(struct sk_buff *skb);

extern int			tcp_v4_remember_stamp(struct sock *sk);

extern int		    	tcp_v4_tw_remember_stamp(struct tcp_tw_bucket *tw);

extern int			tcp_sendmsg(struct sock *sk, struct msghdr *msg, int size);
extern ssize_t			tcp_sendpage(struct socket *sock, struct page *page, int offset, size_t size, int flags);

extern int			tcp_ioctl(struct sock *sk, 
					  int cmd, 
					  unsigned long arg);

extern int			tcp_rcv_state_process(struct sock *sk, 
						      struct sk_buff *skb,
						      struct tcphdr *th,
						      unsigned len);

extern int			tcp_rcv_established(struct sock *sk, 
						    struct sk_buff *skb,
						    struct tcphdr *th, 
						    unsigned len);

enum tcp_ack_state_t
{
	TCP_ACK_SCHED = 1,
	TCP_ACK_TIMER = 2,
	TCP_ACK_PUSHED= 4
};

static inline void tcp_schedule_ack(struct tcp_opt *tp)
{
	tp->ack.pending |= TCP_ACK_SCHED;
}

static inline int tcp_ack_scheduled(struct tcp_opt *tp)
{
	return tp->ack.pending&TCP_ACK_SCHED;
}

static __inline__ void tcp_dec_quickack_mode(struct tcp_opt *tp)
{
	if (tp->ack.quick && --tp->ack.quick == 0) {
		/* Leaving quickack mode we deflate ATO. */
		tp->ack.ato = TCP_ATO_MIN;
	}
}

extern void tcp_enter_quickack_mode(struct tcp_opt *tp);

static __inline__ void tcp_delack_init(struct tcp_opt *tp)
{
	memset(&tp->ack, 0, sizeof(tp->ack));
}

static inline void tcp_clear_options(struct tcp_opt *tp)
{
 	tp->tstamp_ok = tp->sack_ok = tp->wscale_ok = tp->snd_wscale = 0;
}

enum tcp_tw_status
{
	TCP_TW_SUCCESS = 0,
	TCP_TW_RST = 1,
	TCP_TW_ACK = 2,
	TCP_TW_SYN = 3
};


extern enum tcp_tw_status	tcp_timewait_state_process(struct tcp_tw_bucket *tw,
							   struct sk_buff *skb,
							   struct tcphdr *th,
							   unsigned len);

extern struct sock *		tcp_check_req(struct sock *sk,struct sk_buff *skb,
					      struct open_request *req,
					      struct open_request **prev);
extern int			tcp_child_process(struct sock *parent,
						  struct sock *child,
						  struct sk_buff *skb);
extern void			tcp_enter_frto(struct sock *sk);
extern void			tcp_enter_loss(struct sock *sk, int how);
extern void			tcp_clear_retrans(struct tcp_opt *tp);
extern void			tcp_update_metrics(struct sock *sk);

extern void			tcp_close(struct sock *sk, 
					  long timeout);
extern struct sock *		tcp_accept(struct sock *sk, int flags, int *err);
extern unsigned int		tcp_poll(struct file * file, struct socket *sock, struct poll_table_struct *wait);
extern void			tcp_write_space(struct sock *sk); 

extern int			tcp_getsockopt(struct sock *sk, int level, 
					       int optname, char *optval, 
					       int *optlen);
extern int			tcp_setsockopt(struct sock *sk, int level, 
					       int optname, char *optval, 
					       int optlen);
extern void			tcp_set_keepalive(struct sock *sk, int val);
extern int			tcp_recvmsg(struct sock *sk, 
					    struct msghdr *msg,
					    int len, int nonblock, 
					    int flags, int *addr_len);

extern int			tcp_listen_start(struct sock *sk);

extern void			tcp_parse_options(struct sk_buff *skb,
						  struct tcp_opt *tp,
						  int estab);

/*
 *	TCP v4 functions exported for the inet6 API
 */

extern int		       	tcp_v4_rebuild_header(struct sock *sk);

extern int		       	tcp_v4_build_header(struct sock *sk, 
						    struct sk_buff *skb);

extern void		       	tcp_v4_send_check(struct sock *sk, 
						  struct tcphdr *th, int len, 
						  struct sk_buff *skb);

extern int			tcp_v4_conn_request(struct sock *sk,
						    struct sk_buff *skb);

extern struct sock *		tcp_create_openreq_child(struct sock *sk,
							 struct open_request *req,
							 struct sk_buff *skb);

extern struct sock *		tcp_v4_syn_recv_sock(struct sock *sk,
						     struct sk_buff *skb,
						     struct open_request *req,
							struct dst_entry *dst);

extern int			tcp_v4_do_rcv(struct sock *sk,
					      struct sk_buff *skb);

extern int			tcp_v4_connect(struct sock *sk,
					       struct sockaddr *uaddr,
					       int addr_len);

extern int			tcp_connect(struct sock *sk);

extern struct sk_buff *		tcp_make_synack(struct sock *sk,
						struct dst_entry *dst,
						struct open_request *req);

extern int			tcp_disconnect(struct sock *sk, int flags);

extern void			tcp_unhash(struct sock *sk);

extern int			tcp_v4_hash_connecting(struct sock *sk);


/* From syncookies.c */
extern struct sock *cookie_v4_check(struct sock *sk, struct sk_buff *skb, 
				    struct ip_options *opt);
extern __u32 cookie_v4_init_sequence(struct sock *sk, struct sk_buff *skb, 
				     __u16 *mss);

/* tcp_output.c */

extern int tcp_write_xmit(struct sock *, int nonagle);
extern int tcp_retransmit_skb(struct sock *, struct sk_buff *);
extern void tcp_xmit_retransmit_queue(struct sock *);
extern void tcp_simple_retransmit(struct sock *);

extern void tcp_send_probe0(struct sock *);
extern void tcp_send_partial(struct sock *);
extern int  tcp_write_wakeup(struct sock *);
extern void tcp_send_fin(struct sock *sk);
extern void tcp_send_active_reset(struct sock *sk, int priority);
extern int  tcp_send_synack(struct sock *);
extern int  tcp_transmit_skb(struct sock *, struct sk_buff *);
extern void tcp_send_skb(struct sock *, struct sk_buff *, int force_queue, unsigned mss_now);
extern void tcp_push_one(struct sock *, unsigned mss_now);
extern void tcp_send_ack(struct sock *sk);
extern void tcp_send_delayed_ack(struct sock *sk);

/* tcp_timer.c */
extern void tcp_init_xmit_timers(struct sock *);
extern void tcp_clear_xmit_timers(struct sock *);

extern void tcp_delete_keepalive_timer (struct sock *);
extern void tcp_reset_keepalive_timer (struct sock *, unsigned long);
extern int tcp_sync_mss(struct sock *sk, u32 pmtu);

extern const char timer_bug_msg[];

/* Read 'sendfile()'-style from a TCP socket */
typedef int (*sk_read_actor_t)(read_descriptor_t *, struct sk_buff *,
				unsigned int, size_t);
extern int tcp_read_sock(struct sock *sk, read_descriptor_t *desc,
			 sk_read_actor_t recv_actor);

static inline void tcp_clear_xmit_timer(struct sock *sk, int what)
{
	struct tcp_opt *tp = &sk->tp_pinfo.af_tcp;
	
	switch (what) {
	case TCP_TIME_RETRANS:
	case TCP_TIME_PROBE0:
		tp->pending = 0;

#ifdef TCP_CLEAR_TIMERS
		if (timer_pending(&tp->retransmit_timer) &&
		    del_timer(&tp->retransmit_timer))
			__sock_put(sk);
#endif
		break;
	case TCP_TIME_DACK:
		tp->ack.blocked = 0;
		tp->ack.pending = 0;

#ifdef TCP_CLEAR_TIMERS
		if (timer_pending(&tp->delack_timer) &&
		    del_timer(&tp->delack_timer))
			__sock_put(sk);
#endif
		break;
	default:
		printk(timer_bug_msg);
		return;
	};

}

/*
 *	Reset the retransmission timer
 */
static inline void tcp_reset_xmit_timer(struct sock *sk, int what, unsigned long when)
{
	struct tcp_opt *tp = &sk->tp_pinfo.af_tcp;

	if (when > TCP_RTO_MAX) {
#ifdef TCP_DEBUG
		printk(KERN_DEBUG "reset_xmit_timer sk=%p %d when=0x%lx, caller=%p\n", sk, what, when, current_text_addr());
#endif
		when = TCP_RTO_MAX;
	}

	switch (what) {
	case TCP_TIME_RETRANS:
	case TCP_TIME_PROBE0:
		tp->pending = what;
		tp->timeout = jiffies+when;
		if (!mod_timer(&tp->retransmit_timer, tp->timeout))
			sock_hold(sk);
		break;

	case TCP_TIME_DACK:
		tp->ack.pending |= TCP_ACK_TIMER;
		tp->ack.timeout = jiffies+when;
		if (!mod_timer(&tp->delack_timer, tp->ack.timeout))
			sock_hold(sk);
		break;

	default:
		printk(KERN_DEBUG "bug: unknown timer value\n");
	};
}

/* Compute the current effective MSS, taking SACKs and IP options,
 * and even PMTU discovery events into account.
 */

static __inline__ unsigned int tcp_current_mss(struct sock *sk)
{
	struct tcp_opt *tp = &sk->tp_pinfo.af_tcp;
	struct dst_entry *dst = __sk_dst_get(sk);
	int mss_now = tp->mss_cache; 

	if (dst && dst->pmtu != tp->pmtu_cookie)
		mss_now = tcp_sync_mss(sk, dst->pmtu);

	if (tp->eff_sacks)
		mss_now -= (TCPOLEN_SACK_BASE_ALIGNED +
			    (tp->eff_sacks * TCPOLEN_SACK_PERBLOCK));
	return mss_now;
}

/* Initialize RCV_MSS value.
 * RCV_MSS is an our guess about MSS used by the peer.
 * We haven't any direct information about the MSS.
 * It's better to underestimate the RCV_MSS rather than overestimate.
 * Overestimations make us ACKing less frequently than needed.
 * Underestimations are more easy to detect and fix by tcp_measure_rcv_mss().
 */

static inline void tcp_initialize_rcv_mss(struct sock *sk)
{
	struct tcp_opt *tp = &sk->tp_pinfo.af_tcp;
	unsigned int hint = min(tp->advmss, tp->mss_cache);

	hint = min(hint, tp->rcv_wnd/2);
	hint = min(hint, TCP_MIN_RCVMSS);
	hint = max(hint, TCP_MIN_MSS);

	tp->ack.rcv_mss = hint;
}

static __inline__ void __tcp_fast_path_on(struct tcp_opt *tp, u32 snd_wnd)
{
	tp->pred_flags = htonl((tp->tcp_header_len << 26) |
			       ntohl(TCP_FLAG_ACK) |
			       snd_wnd);
}

static __inline__ void tcp_fast_path_on(struct tcp_opt *tp)
{
	__tcp_fast_path_on(tp, tp->snd_wnd>>tp->snd_wscale);
}

static inline void tcp_fast_path_check(struct sock *sk, struct tcp_opt *tp)
{
	if (skb_queue_len(&tp->out_of_order_queue) == 0 &&
	    tp->rcv_wnd &&
	    atomic_read(&sk->rmem_alloc) < sk->rcvbuf &&
	    !tp->urg_data)
		tcp_fast_path_on(tp);
}

/* Compute the actual receive window we are currently advertising.
 * Rcv_nxt can be after the window if our peer push more data
 * than the offered window.
 */
static __inline__ u32 tcp_receive_window(struct tcp_opt *tp)
{
	s32 win = tp->rcv_wup + tp->rcv_wnd - tp->rcv_nxt;

	if (win < 0)
		win = 0;
	return (u32) win;
}

/* Choose a new window, without checks for shrinking, and without
 * scaling applied to the result.  The caller does these things
 * if necessary.  This is a "raw" window selection.
 */
extern u32	__tcp_select_window(struct sock *sk);

/* TCP timestamps are only 32-bits, this causes a slight
 * complication on 64-bit systems since we store a snapshot
 * of jiffies in the buffer control blocks below.  We decidely
 * only use of the low 32-bits of jiffies and hide the ugly
 * casts with the following macro.
 */
#define tcp_time_stamp		((__u32)(jiffies))

/* This is what the send packet queueing engine uses to pass
 * TCP per-packet control information to the transmission
 * code.  We also store the host-order sequence numbers in
 * here too.  This is 36 bytes on 32-bit architectures,
 * 40 bytes on 64-bit machines, if this grows please adjust
 * skbuff.h:skbuff->cb[xxx] size appropriately.
 */
struct tcp_skb_cb {
	union {
		struct inet_skb_parm	h4;
#if defined(CONFIG_IPV6) || defined (CONFIG_IPV6_MODULE)
		struct inet6_skb_parm	h6;
#endif
	} header;	/* For incoming frames		*/
	__u32		seq;		/* Starting sequence number	*/
	__u32		end_seq;	/* SEQ + FIN + SYN + datalen	*/
	__u32		when;		/* used to compute rtt's	*/
	__u8		flags;		/* TCP header flags.		*/

	/* NOTE: These must match up to the flags byte in a
	 *       real TCP header.
	 */
#define TCPCB_FLAG_FIN		0x01
#define TCPCB_FLAG_SYN		0x02
#define TCPCB_FLAG_RST		0x04
#define TCPCB_FLAG_PSH		0x08
#define TCPCB_FLAG_ACK		0x10
#define TCPCB_FLAG_URG		0x20
#define TCPCB_FLAG_ECE		0x40
#define TCPCB_FLAG_CWR		0x80

	__u8		sacked;		/* State flags for SACK/FACK.	*/
#define TCPCB_SACKED_ACKED	0x01	/* SKB ACK'd by a SACK block	*/
#define TCPCB_SACKED_RETRANS	0x02	/* SKB retransmitted		*/
#define TCPCB_LOST		0x04	/* SKB is lost			*/
#define TCPCB_TAGBITS		0x07	/* All tag bits			*/

#define TCPCB_EVER_RETRANS	0x80	/* Ever retransmitted frame	*/
#define TCPCB_RETRANS		(TCPCB_SACKED_RETRANS|TCPCB_EVER_RETRANS)

#define TCPCB_URG		0x20	/* Urgent pointer advenced here	*/

#define TCPCB_AT_TAIL		(TCPCB_URG)

	__u16		urg_ptr;	/* Valid w/URG flags is set.	*/
	__u32		ack_seq;	/* Sequence number ACK'd	*/
};

#define TCP_SKB_CB(__skb)	((struct tcp_skb_cb *)&((__skb)->cb[0]))

#define for_retrans_queue(skb, sk, tp) \
		for (skb = (sk)->write_queue.next;			\
		     (skb != (tp)->send_head) &&			\
		     (skb != (struct sk_buff *)&(sk)->write_queue);	\
		     skb=skb->next)


#include <net/tcp_ecn.h>


/*
 *	Compute minimal free write space needed to queue new packets. 
 */
static inline int tcp_min_write_space(struct sock *sk)
{
	return sk->wmem_queued/2;
}
 
static inline int tcp_wspace(struct sock *sk)
{
	return sk->sndbuf - sk->wmem_queued;
}


/* This determines how many packets are "in the network" to the best
 * of our knowledge.  In many cases it is conservative, but where
 * detailed information is available from the receiver (via SACK
 * blocks etc.) we can make more aggressive calculations.
 *
 * Use this for decisions involving congestion control, use just
 * tp->packets_out to determine if the send queue is empty or not.
 *
 * Read this equation as:
 *
 *	"Packets sent once on transmission queue" MINUS
 *	"Packets left network, but not honestly ACKed yet" PLUS
 *	"Packets fast retransmitted"
 */
static __inline__ unsigned int tcp_packets_in_flight(struct tcp_opt *tp)
{
	return tp->packets_out - tp->left_out + tp->retrans_out;
}

/* Recalculate snd_ssthresh, we want to set it to:
 *
 * 	one half the current congestion window, but no
 *	less than two segments
 */
static inline __u32 tcp_recalc_ssthresh(struct tcp_opt *tp)
{
	return max(tp->snd_cwnd >> 1U, 2U);
}

/* If cwnd > ssthresh, we may raise ssthresh to be half-way to cwnd.
 * The exception is rate halving phase, when cwnd is decreasing towards
 * ssthresh.
 */
static inline __u32 tcp_current_ssthresh(struct tcp_opt *tp)
{
	if ((1<<tp->ca_state)&(TCPF_CA_CWR|TCPF_CA_Recovery))
		return tp->snd_ssthresh;
	else
		return max(tp->snd_ssthresh,
			   ((tp->snd_cwnd >> 1) +
			    (tp->snd_cwnd >> 2)));
}

static inline void tcp_sync_left_out(struct tcp_opt *tp)
{
	if (tp->sack_ok && tp->sacked_out >= tp->packets_out - tp->lost_out)
		tp->sacked_out = tp->packets_out - tp->lost_out;
	tp->left_out = tp->sacked_out + tp->lost_out;
}

extern void tcp_cwnd_application_limited(struct sock *sk);

/* Congestion window validation. (RFC2861) */

static inline void tcp_cwnd_validate(struct sock *sk, struct tcp_opt *tp)
{
	if (tp->packets_out >= tp->snd_cwnd) {
		/* Network is feed fully. */
		tp->snd_cwnd_used = 0;
		tp->snd_cwnd_stamp = tcp_time_stamp;
	} else {
		/* Network starves. */
		if (tp->packets_out > tp->snd_cwnd_used)
			tp->snd_cwnd_used = tp->packets_out;

		if ((s32)(tcp_time_stamp - tp->snd_cwnd_stamp) >= tp->rto)
			tcp_cwnd_application_limited(sk);
	}
}

/* Set slow start threshould and cwnd not falling to slow start */
static inline void __tcp_enter_cwr(struct tcp_opt *tp)
{
	tp->undo_marker = 0;
	tp->snd_ssthresh = tcp_recalc_ssthresh(tp);
	tp->snd_cwnd = min(tp->snd_cwnd,
			   tcp_packets_in_flight(tp) + 1U);
	tp->snd_cwnd_cnt = 0;
	tp->high_seq = tp->snd_nxt;
	tp->snd_cwnd_stamp = tcp_time_stamp;
	TCP_ECN_queue_cwr(tp);
}

static inline void tcp_enter_cwr(struct tcp_opt *tp)
{
	tp->prior_ssthresh = 0;
	if (tp->ca_state < TCP_CA_CWR) {
		__tcp_enter_cwr(tp);
		tp->ca_state = TCP_CA_CWR;
	}
}

extern __u32 tcp_init_cwnd(struct tcp_opt *tp);

/* Slow start with delack produces 3 packets of burst, so that
 * it is safe "de facto".
 */
static __inline__ __u32 tcp_max_burst(struct tcp_opt *tp)
{
	return 3;
}

static __inline__ int tcp_minshall_check(struct tcp_opt *tp)
{
	return after(tp->snd_sml,tp->snd_una) &&
		!after(tp->snd_sml, tp->snd_nxt);
}

static __inline__ void tcp_minshall_update(struct tcp_opt *tp, int mss, struct sk_buff *skb)
{
	if (skb->len < mss)
		tp->snd_sml = TCP_SKB_CB(skb)->end_seq;
}

/* Return 0, if packet can be sent now without violation Nagle's rules:
   1. It is full sized.
   2. Or it contains FIN.
   3. Or TCP_NODELAY was set.
   4. Or TCP_CORK is not set, and all sent packets are ACKed.
      With Minshall's modification: all sent small packets are ACKed.
 */

static __inline__ int
tcp_nagle_check(struct tcp_opt *tp, struct sk_buff *skb, unsigned mss_now, int nonagle)
{
	return (skb->len < mss_now &&
		!(TCP_SKB_CB(skb)->flags & TCPCB_FLAG_FIN) &&
		(nonagle == 2 ||
		 (!nonagle &&
		  tp->packets_out &&
		  tcp_minshall_check(tp))));
}

/* This checks if the data bearing packet SKB (usually tp->send_head)
 * should be put on the wire right now.
 */
static __inline__ int tcp_snd_test(struct tcp_opt *tp, struct sk_buff *skb,
				   unsigned cur_mss, int nonagle)
{
	/*	RFC 1122 - section 4.2.3.4
	 *
	 *	We must queue if
	 *
	 *	a) The right edge of this frame exceeds the window
	 *	b) There are packets in flight and we have a small segment
	 *	   [SWS avoidance and Nagle algorithm]
	 *	   (part of SWS is done on packetization)
	 *	   Minshall version sounds: there are no _small_
	 *	   segments in flight. (tcp_nagle_check)
	 *	c) We have too many packets 'in flight'
	 *
	 * 	Don't use the nagle rule for urgent data (or
	 *	for the final FIN -DaveM).
	 *
	 *	Also, Nagle rule does not apply to frames, which
	 *	sit in the middle of queue (they have no chances
	 *	to get new data) and if room at tail of skb is
	 *	not enough to save something seriously (<32 for now).
	 */

	/* Don't be strict about the congestion window for the
	 * final FIN frame.  -DaveM
	 */
	return ((nonagle==1 || tp->urg_mode
		 || !tcp_nagle_check(tp, skb, cur_mss, nonagle)) &&
		((tcp_packets_in_flight(tp) < tp->snd_cwnd) ||
		 (TCP_SKB_CB(skb)->flags & TCPCB_FLAG_FIN)) &&
		!after(TCP_SKB_CB(skb)->end_seq, tp->snd_una + tp->snd_wnd));
}

static __inline__ void tcp_check_probe_timer(struct sock *sk, struct tcp_opt *tp)
{
	if (!tp->packets_out && !tp->pending)
		tcp_reset_xmit_timer(sk, TCP_TIME_PROBE0, tp->rto);
}

static __inline__ int tcp_skb_is_last(struct sock *sk, struct sk_buff *skb)
{
	return (skb->next == (struct sk_buff*)&sk->write_queue);
}

/* Push out any pending frames which were held back due to
 * TCP_CORK or attempt at coalescing tiny packets.
 * The socket must be locked by the caller.
 */
static __inline__ void __tcp_push_pending_frames(struct sock *sk,
						 struct tcp_opt *tp,
						 unsigned cur_mss,
						 int nonagle)
{
	struct sk_buff *skb = tp->send_head;

	if (skb) {
		if (!tcp_skb_is_last(sk, skb))
			nonagle = 1;
		if (!tcp_snd_test(tp, skb, cur_mss, nonagle) ||
		    tcp_write_xmit(sk, nonagle))
			tcp_check_probe_timer(sk, tp);
	}
	tcp_cwnd_validate(sk, tp);
}

static __inline__ void tcp_push_pending_frames(struct sock *sk,
					       struct tcp_opt *tp)
{
	__tcp_push_pending_frames(sk, tp, tcp_current_mss(sk), tp->nonagle);
}

static __inline__ int tcp_may_send_now(struct sock *sk, struct tcp_opt *tp)
{
	struct sk_buff *skb = tp->send_head;

	return (skb &&
		tcp_snd_test(tp, skb, tcp_current_mss(sk),
			     tcp_skb_is_last(sk, skb) ? 1 : tp->nonagle));
}

static __inline__ void tcp_init_wl(struct tcp_opt *tp, u32 ack, u32 seq)
{
	tp->snd_wl1 = seq;
}

static __inline__ void tcp_update_wl(struct tcp_opt *tp, u32 ack, u32 seq)
{
	tp->snd_wl1 = seq;
}

extern void			tcp_destroy_sock(struct sock *sk);


/*
 * Calculate(/check) TCP checksum
 */
static __inline__ u16 tcp_v4_check(struct tcphdr *th, int len,
				   unsigned long saddr, unsigned long daddr, 
				   unsigned long base)
{
	return csum_tcpudp_magic(saddr,daddr,len,IPPROTO_TCP,base);
}

static __inline__ int __tcp_checksum_complete(struct sk_buff *skb)
{
	return (unsigned short)csum_fold(skb_checksum(skb, 0, skb->len, skb->csum));
}

static __inline__ int tcp_checksum_complete(struct sk_buff *skb)
{
	return skb->ip_summed != CHECKSUM_UNNECESSARY &&
		__tcp_checksum_complete(skb);
}

/* Prequeue for VJ style copy to user, combined with checksumming. */

static __inline__ void tcp_prequeue_init(struct tcp_opt *tp)
{
	tp->ucopy.task = NULL;
	tp->ucopy.len = 0;
	tp->ucopy.memory = 0;
	skb_queue_head_init(&tp->ucopy.prequeue);
}

/* Packet is added to VJ-style prequeue for processing in process
 * context, if a reader task is waiting. Apparently, this exciting
 * idea (VJ's mail "Re: query about TCP header on tcp-ip" of 07 Sep 93)
 * failed somewhere. Latency? Burstiness? Well, at least now we will
 * see, why it failed. 8)8)				  --ANK
 *
 * NOTE: is this not too big to inline?
 */
static __inline__ int tcp_prequeue(struct sock *sk, struct sk_buff *skb)
{
	struct tcp_opt *tp = &sk->tp_pinfo.af_tcp;

	if (!sysctl_tcp_low_latency && tp->ucopy.task) {
		__skb_queue_tail(&tp->ucopy.prequeue, skb);
		tp->ucopy.memory += skb->truesize;
		if (tp->ucopy.memory > sk->rcvbuf) {
			struct sk_buff *skb1;

			if (sk->lock.users)
				out_of_line_bug();

			while ((skb1 = __skb_dequeue(&tp->ucopy.prequeue)) != NULL) {
				sk->backlog_rcv(sk, skb1);
				NET_INC_STATS_BH(TCPPrequeueDropped);
			}

			tp->ucopy.memory = 0;
		} else if (skb_queue_len(&tp->ucopy.prequeue) == 1) {
			wake_up_interruptible(sk->sleep);
			if (!tcp_ack_scheduled(tp))
				tcp_reset_xmit_timer(sk, TCP_TIME_DACK, (3*TCP_RTO_MIN)/4);
		}
		return 1;
	}
	return 0;
}


#undef STATE_TRACE

#ifdef STATE_TRACE
static char *statename[]={
	"Unused","Established","Syn Sent","Syn Recv",
	"Fin Wait 1","Fin Wait 2","Time Wait", "Close",
	"Close Wait","Last ACK","Listen","Closing"
};
#endif

static __inline__ void tcp_set_state(struct sock *sk, int state)
{
	int oldstate = sk->state;

	switch (state) {
	case TCP_ESTABLISHED:
		if (oldstate != TCP_ESTABLISHED)
			TCP_INC_STATS(TcpCurrEstab);
		break;

	case TCP_CLOSE:
		if (oldstate == TCP_CLOSE_WAIT || oldstate == TCP_ESTABLISHED)
			TCP_INC_STATS(TcpEstabResets);

		sk->prot->unhash(sk);
		if (sk->prev && !(sk->userlocks&SOCK_BINDPORT_LOCK))
			tcp_put_port(sk);
		/* fall through */
	default:
		if (oldstate==TCP_ESTABLISHED)
			tcp_statistics[smp_processor_id()*2+!in_softirq()].TcpCurrEstab--;
	}

	/* Change state AFTER socket is unhashed to avoid closed
	 * socket sitting in hash tables.
	 */
	sk->state = state;

#ifdef STATE_TRACE
	SOCK_DEBUG(sk, "TCP sk=%p, State %s -> %s\n",sk, statename[oldstate],statename[state]);
#endif	
}

static __inline__ void tcp_done(struct sock *sk)
{
	tcp_set_state(sk, TCP_CLOSE);
	tcp_clear_xmit_timers(sk);

	sk->shutdown = SHUTDOWN_MASK;

	if (!sk->dead)
		sk->state_change(sk);
	else
		tcp_destroy_sock(sk);
}

static __inline__ void tcp_sack_reset(struct tcp_opt *tp)
{
	tp->dsack = 0;
	tp->eff_sacks = 0;
	tp->num_sacks = 0;
}

static __inline__ void tcp_build_and_update_options(__u32 *ptr, struct tcp_opt *tp, __u32 tstamp)
{
	if (tp->tstamp_ok) {
		*ptr++ = __constant_htonl((TCPOPT_NOP << 24) |
					  (TCPOPT_NOP << 16) |
					  (TCPOPT_TIMESTAMP << 8) |
					  TCPOLEN_TIMESTAMP);
		*ptr++ = htonl(tstamp);
		*ptr++ = htonl(tp->ts_recent);
	}
	if (tp->eff_sacks) {
		struct tcp_sack_block *sp = tp->dsack ? tp->duplicate_sack : tp->selective_acks;
		int this_sack;

		*ptr++ = __constant_htonl((TCPOPT_NOP << 24) |
					  (TCPOPT_NOP << 16) |
					  (TCPOPT_SACK << 8) |
					  (TCPOLEN_SACK_BASE +
					   (tp->eff_sacks * TCPOLEN_SACK_PERBLOCK)));
		for(this_sack = 0; this_sack < tp->eff_sacks; this_sack++) {
			*ptr++ = htonl(sp[this_sack].start_seq);
			*ptr++ = htonl(sp[this_sack].end_seq);
		}
		if (tp->dsack) {
			tp->dsack = 0;
			tp->eff_sacks--;
		}
	}
}

/* Construct a tcp options header for a SYN or SYN_ACK packet.
 * If this is every changed make sure to change the definition of
 * MAX_SYN_SIZE to match the new maximum number of options that you
 * can generate.
 */
static inline void tcp_syn_build_options(__u32 *ptr, int mss, int ts, int sack,
					     int offer_wscale, int wscale, __u32 tstamp, __u32 ts_recent)
{
	/* We always get an MSS option.
	 * The option bytes which will be seen in normal data
	 * packets should timestamps be used, must be in the MSS
	 * advertised.  But we subtract them from tp->mss_cache so
	 * that calculations in tcp_sendmsg are simpler etc.
	 * So account for this fact here if necessary.  If we
	 * don't do this correctly, as a receiver we won't
	 * recognize data packets as being full sized when we
	 * should, and thus we won't abide by the delayed ACK
	 * rules correctly.
	 * SACKs don't matter, we never delay an ACK when we
	 * have any of those going out.
	 */
	*ptr++ = htonl((TCPOPT_MSS << 24) | (TCPOLEN_MSS << 16) | mss);
	if (ts) {
		if(sack)
			*ptr++ = __constant_htonl((TCPOPT_SACK_PERM << 24) | (TCPOLEN_SACK_PERM << 16) |
						  (TCPOPT_TIMESTAMP << 8) | TCPOLEN_TIMESTAMP);
		else
			*ptr++ = __constant_htonl((TCPOPT_NOP << 24) | (TCPOPT_NOP << 16) |
						  (TCPOPT_TIMESTAMP << 8) | TCPOLEN_TIMESTAMP);
		*ptr++ = htonl(tstamp);		/* TSVAL */
		*ptr++ = htonl(ts_recent);	/* TSECR */
	} else if(sack)
		*ptr++ = __constant_htonl((TCPOPT_NOP << 24) | (TCPOPT_NOP << 16) |
					  (TCPOPT_SACK_PERM << 8) | TCPOLEN_SACK_PERM);
	if (offer_wscale)
		*ptr++ = htonl((TCPOPT_NOP << 24) | (TCPOPT_WINDOW << 16) | (TCPOLEN_WINDOW << 8) | (wscale));
}

/* Determine a window scaling and initial window to offer.
 * Based on the assumption that the given amount of space
 * will be offered. Store the results in the tp structure.
 * NOTE: for smooth operation initial space offering should
 * be a multiple of mss if possible. We assume here that mss >= 1.
 * This MUST be enforced by all callers.
 */
static inline void tcp_select_initial_window(int __space, __u32 mss,
	__u32 *rcv_wnd,
	__u32 *window_clamp,
	int wscale_ok,
	__u8 *rcv_wscale)
{
	unsigned int space = (__space < 0 ? 0 : __space);

	/* If no clamp set the clamp to the max possible scaled window */
	if (*window_clamp == 0)
		(*window_clamp) = (65535 << 14);
	space = min(*window_clamp, space);

	/* Quantize space offering to a multiple of mss if possible. */
	if (space > mss)
		space = (space / mss) * mss;

	/* NOTE: offering an initial window larger than 32767
	 * will break some buggy TCP stacks. We try to be nice.
	 * If we are not window scaling, then this truncates
	 * our initial window offering to 32k. There should also
	 * be a sysctl option to stop being nice.
	 */
	(*rcv_wnd) = min(space, MAX_TCP_WINDOW);
	(*rcv_wscale) = 0;
	if (wscale_ok) {
		/* See RFC1323 for an explanation of the limit to 14 */
		while (space > 65535 && (*rcv_wscale) < 14) {
			space >>= 1;
			(*rcv_wscale)++;
		}
		if (*rcv_wscale && sysctl_tcp_app_win && space>=mss &&
		    space - max((space>>sysctl_tcp_app_win), mss>>*rcv_wscale) < 65536/2)
			(*rcv_wscale)--;
	}

	/* Set initial window to value enough for senders,
	 * following RFC1414. Senders, not following this RFC,
	 * will be satisfied with 2.
	 */
	if (mss > (1<<*rcv_wscale)) {
		int init_cwnd = 4;
		if (mss > 1460*3)
			init_cwnd = 2;
		else if (mss > 1460)
			init_cwnd = 3;
		if (*rcv_wnd > init_cwnd*mss)
			*rcv_wnd = init_cwnd*mss;
	}
	/* Set the clamp no higher than max representable value */
	(*window_clamp) = min(65535U << (*rcv_wscale), *window_clamp);
}

static inline int tcp_win_from_space(int space)
{
	return sysctl_tcp_adv_win_scale<=0 ?
		(space>>(-sysctl_tcp_adv_win_scale)) :
		space - (space>>sysctl_tcp_adv_win_scale);
}

/* Note: caller must be prepared to deal with negative returns */ 
static inline int tcp_space(struct sock *sk)
{
	return tcp_win_from_space(sk->rcvbuf - atomic_read(&sk->rmem_alloc));
} 

static inline int tcp_full_space( struct sock *sk)
{
	return tcp_win_from_space(sk->rcvbuf); 
}

static inline void tcp_acceptq_removed(struct sock *sk)
{
	sk->ack_backlog--;
}

static inline void tcp_acceptq_added(struct sock *sk)
{
	sk->ack_backlog++;
}

static inline int tcp_acceptq_is_full(struct sock *sk)
{
	return sk->ack_backlog > sk->max_ack_backlog;
}

static inline void tcp_acceptq_queue(struct sock *sk, struct open_request *req,
					 struct sock *child)
{
	struct tcp_opt *tp = &sk->tp_pinfo.af_tcp;

	req->sk = child;
	tcp_acceptq_added(sk);

	if (!tp->accept_queue_tail) {
		tp->accept_queue = req;
	} else {
		tp->accept_queue_tail->dl_next = req;
	}
	tp->accept_queue_tail = req;
	req->dl_next = NULL;
}

struct tcp_listen_opt
{
	u8			max_qlen_log;	/* log_2 of maximal queued SYNs */
	int			qlen;
	int			qlen_young;
	int			clock_hand;
	u32			hash_rnd;
	struct open_request	*syn_table[TCP_SYNQ_HSIZE];
};

static inline void
tcp_synq_removed(struct sock *sk, struct open_request *req)
{
	struct tcp_listen_opt *lopt = sk->tp_pinfo.af_tcp.listen_opt;

	if (--lopt->qlen == 0)
		tcp_delete_keepalive_timer(sk);
	if (req->retrans == 0)
		lopt->qlen_young--;
}

static inline void tcp_synq_added(struct sock *sk)
{
	struct tcp_listen_opt *lopt = sk->tp_pinfo.af_tcp.listen_opt;

	if (lopt->qlen++ == 0)
		tcp_reset_keepalive_timer(sk, TCP_TIMEOUT_INIT);
	lopt->qlen_young++;
}

static inline int tcp_synq_len(struct sock *sk)
{
	return sk->tp_pinfo.af_tcp.listen_opt->qlen;
}

static inline int tcp_synq_young(struct sock *sk)
{
	return sk->tp_pinfo.af_tcp.listen_opt->qlen_young;
}

static inline int tcp_synq_is_full(struct sock *sk)
{
	return tcp_synq_len(sk)>>sk->tp_pinfo.af_tcp.listen_opt->max_qlen_log;
}

static inline void tcp_synq_unlink(struct tcp_opt *tp, struct open_request *req,
				       struct open_request **prev)
{
	write_lock(&tp->syn_wait_lock);
	*prev = req->dl_next;
	write_unlock(&tp->syn_wait_lock);
}

static inline void tcp_synq_drop(struct sock *sk, struct open_request *req,
				     struct open_request **prev)
{
	tcp_synq_unlink(&sk->tp_pinfo.af_tcp, req, prev);
	tcp_synq_removed(sk, req);
	tcp_openreq_free(req);
}

static __inline__ void tcp_openreq_init(struct open_request *req,
					struct tcp_opt *tp,
					struct sk_buff *skb)
{
	req->rcv_wnd = 0;		/* So that tcp_send_synack() knows! */
	req->rcv_isn = TCP_SKB_CB(skb)->seq;
	req->mss = tp->mss_clamp;
	req->ts_recent = tp->saw_tstamp ? tp->rcv_tsval : 0;
	req->tstamp_ok = tp->tstamp_ok;
	req->sack_ok = tp->sack_ok;
	req->snd_wscale = tp->snd_wscale;
	req->wscale_ok = tp->wscale_ok;
	req->acked = 0;
	req->ecn_ok = 0;
	req->rmt_port = skb->h.th->source;
}

#define TCP_MEM_QUANTUM	((int)PAGE_SIZE)

static inline void tcp_free_skb(struct sock *sk, struct sk_buff *skb)
{
	sk->tp_pinfo.af_tcp.queue_shrunk = 1;
	sk->wmem_queued -= skb->truesize;
	sk->forward_alloc += skb->truesize;
	__kfree_skb(skb);
}

static inline void tcp_charge_skb(struct sock *sk, struct sk_buff *skb)
{
	sk->wmem_queued += skb->truesize;
	sk->forward_alloc -= skb->truesize;
}

extern void __tcp_mem_reclaim(struct sock *sk);
extern int tcp_mem_schedule(struct sock *sk, int size, int kind);

static inline void tcp_mem_reclaim(struct sock *sk)
{
	if (sk->forward_alloc >= TCP_MEM_QUANTUM)
		__tcp_mem_reclaim(sk);
}

static inline void tcp_enter_memory_pressure(void)
{
	if (!tcp_memory_pressure) {
		NET_INC_STATS(TCPMemoryPressures);
		tcp_memory_pressure = 1;
	}
}

static inline void tcp_moderate_sndbuf(struct sock *sk)
{
	if (!(sk->userlocks&SOCK_SNDBUF_LOCK)) {
		sk->sndbuf = min(sk->sndbuf, sk->wmem_queued/2);
		sk->sndbuf = max(sk->sndbuf, SOCK_MIN_SNDBUF);
	}
}

static inline struct sk_buff *tcp_alloc_pskb(struct sock *sk, int size, int mem, int gfp)
{
	struct sk_buff *skb = alloc_skb(size+MAX_TCP_HEADER, gfp);

	if (skb) {
		skb->truesize += mem;
		if (sk->forward_alloc >= (int)skb->truesize ||
		    tcp_mem_schedule(sk, skb->truesize, 0)) {
			skb_reserve(skb, MAX_TCP_HEADER);
			return skb;
		}
		__kfree_skb(skb);
	} else {
		tcp_enter_memory_pressure();
		tcp_moderate_sndbuf(sk);
	}
	return NULL;
}

static inline struct sk_buff *tcp_alloc_skb(struct sock *sk, int size, int gfp)
{
	return tcp_alloc_pskb(sk, size, 0, gfp);
}

static inline struct page * tcp_alloc_page(struct sock *sk)
{
	if (sk->forward_alloc >= (int)PAGE_SIZE ||
	    tcp_mem_schedule(sk, PAGE_SIZE, 0)) {
		struct page *page = alloc_pages(sk->allocation, 0);
		if (page)
			return page;
	}
	tcp_enter_memory_pressure();
	tcp_moderate_sndbuf(sk);
	return NULL;
}

static inline void tcp_writequeue_purge(struct sock *sk)
{
	struct sk_buff *skb;

	while ((skb = __skb_dequeue(&sk->write_queue)) != NULL)
		tcp_free_skb(sk, skb);
	tcp_mem_reclaim(sk);
}

extern void tcp_rfree(struct sk_buff *skb);

static inline void tcp_set_owner_r(struct sk_buff *skb, struct sock *sk)
{
	skb->sk = sk;
	skb->destructor = tcp_rfree;
	atomic_add(skb->truesize, &sk->rmem_alloc);
	sk->forward_alloc -= skb->truesize;
}

extern void tcp_listen_wlock(void);

/* - We may sleep inside this lock.
 * - If sleeping is not required (or called from BH),
 *   use plain read_(un)lock(&tcp_lhash_lock).
 */

static inline void tcp_listen_lock(void)
{
	/* read_lock synchronizes to candidates to writers */
	read_lock(&tcp_lhash_lock);
	atomic_inc(&tcp_lhash_users);
	read_unlock(&tcp_lhash_lock);
}

static inline void tcp_listen_unlock(void)
{
	if (atomic_dec_and_test(&tcp_lhash_users))
		wake_up(&tcp_lhash_wait);
}

static inline int keepalive_intvl_when(struct tcp_opt *tp)
{
	return tp->keepalive_intvl ? : sysctl_tcp_keepalive_intvl;
}

static inline int keepalive_time_when(struct tcp_opt *tp)
{
	return tp->keepalive_time ? : sysctl_tcp_keepalive_time;
}

static inline int tcp_fin_time(struct tcp_opt *tp)
{
	int fin_timeout = tp->linger2 ? : sysctl_tcp_fin_timeout;

	if (fin_timeout < (tp->rto<<2) - (tp->rto>>1))
		fin_timeout = (tp->rto<<2) - (tp->rto>>1);

	return fin_timeout;
}

static inline int tcp_paws_check(struct tcp_opt *tp, int rst)
{
	if ((s32)(tp->rcv_tsval - tp->ts_recent) >= 0)
		return 0;
	if (xtime.tv_sec >= tp->ts_recent_stamp + TCP_PAWS_24DAYS)
		return 0;

	/* RST segments are not recommended to carry timestamp,
	   and, if they do, it is recommended to ignore PAWS because
	   "their cleanup function should take precedence over timestamps."
	   Certainly, it is mistake. It is necessary to understand the reasons
	   of this constraint to relax it: if peer reboots, clock may go
	   out-of-sync and half-open connections will not be reset.
	   Actually, the problem would be not existing if all
	   the implementations followed draft about maintaining clock
	   via reboots. Linux-2.2 DOES NOT!

	   However, we can relax time bounds for RST segments to MSL.
	 */
	if (rst && xtime.tv_sec >= tp->ts_recent_stamp + TCP_PAWS_MSL)
		return 0;
	return 1;
}

#define TCP_CHECK_TIMER(sk) do { } while (0)

static inline int tcp_use_frto(const struct sock *sk)
{
	const struct tcp_opt *tp = &sk->tp_pinfo.af_tcp;
	
	/* F-RTO must be activated in sysctl and there must be some
	 * unsent new data, and the advertised window should allow
	 * sending it.
	 */
	return (sysctl_tcp_frto && tp->send_head &&
		!after(TCP_SKB_CB(tp->send_head)->end_seq,
		       tp->snd_una + tp->snd_wnd));
}

static inline void tcp_mib_init(void)
{
	/* See RFC 2012 */
	TCP_ADD_STATS_USER(TcpRtoAlgorithm, 1);
	TCP_ADD_STATS_USER(TcpRtoMin, TCP_RTO_MIN*1000/HZ);
	TCP_ADD_STATS_USER(TcpRtoMax, TCP_RTO_MAX*1000/HZ);
	TCP_ADD_STATS_USER(TcpMaxConn, -1);
}


/* TCP Westwood functions and constants */

#define TCP_WESTWOOD_INIT_RTT               20*HZ           /* maybe too conservative?! */
#define TCP_WESTWOOD_RTT_MIN                HZ/20           /* 50ms */

static inline void tcp_westwood_update_rtt(struct tcp_opt *tp, __u32 rtt_seq)
{
	if (sysctl_tcp_westwood)
		tp->westwood.rtt = rtt_seq;
}

void __tcp_westwood_fast_bw(struct sock *, struct sk_buff *);
void __tcp_westwood_slow_bw(struct sock *, struct sk_buff *);

/*
 * This function initializes fields used in TCP Westwood+. We can't
 * get no information about RTTmin at this time so we simply set it to
 * TCP_WESTWOOD_INIT_RTT. This value was chosen to be too conservative
 * since in this way we're sure it will be updated in a consistent
 * way as soon as possible. It will reasonably happen within the first
 * RTT period of the connection lifetime.
 */

static inline void __tcp_init_westwood(struct sock *sk)
{
	struct tcp_opt *tp = &(sk->tp_pinfo.af_tcp);

	tp->westwood.bw_ns_est = 0;
	tp->westwood.bw_est = 0;
	tp->westwood.accounted = 0;
	tp->westwood.cumul_ack = 0;
	tp->westwood.rtt_win_sx = tcp_time_stamp;
	tp->westwood.rtt = TCP_WESTWOOD_INIT_RTT;
	tp->westwood.rtt_min = TCP_WESTWOOD_INIT_RTT;
	tp->westwood.snd_una = tp->snd_una;
}

static inline void tcp_init_westwood(struct sock *sk)
{
	__tcp_init_westwood(sk);
}

static inline void tcp_westwood_fast_bw(struct sock *sk, struct sk_buff *skb)
{
	if (sysctl_tcp_westwood)
		__tcp_westwood_fast_bw(sk, skb);
}

static inline void tcp_westwood_slow_bw(struct sock *sk, struct sk_buff *skb)
{
	if (sysctl_tcp_westwood)
		__tcp_westwood_slow_bw(sk, skb);
}

static inline __u32 __tcp_westwood_bw_rttmin(struct tcp_opt *tp)
{
	return (__u32) ((tp->westwood.bw_est) * (tp->westwood.rtt_min) /
			(__u32) (tp->mss_cache));
}

static inline __u32 tcp_westwood_bw_rttmin(struct tcp_opt *tp)
{
	__u32 ret = 0;

	if (sysctl_tcp_westwood)
		ret = (__u32) (max(__tcp_westwood_bw_rttmin(tp), 2U));

	return ret;
}

static inline int tcp_westwood_ssthresh(struct tcp_opt *tp)
{
	int ret = 0;
	__u32 ssthresh;

	if (sysctl_tcp_westwood) {
		if (!(ssthresh = tcp_westwood_bw_rttmin(tp)))
			return ret;

		tp->snd_ssthresh = ssthresh;
		ret = 1;
	}

	return ret;
}

static inline int tcp_westwood_cwnd(struct tcp_opt *tp)
{
	int ret = 0;
	__u32 cwnd;

	if (sysctl_tcp_westwood) {
		if (!(cwnd = tcp_westwood_bw_rttmin(tp)))
			return ret;

		tp->snd_cwnd = cwnd;
		ret = 1;
	}

	return ret;
}

static inline int tcp_westwood_complete_cwr(struct tcp_opt *tp) 
{
	int ret = 0;

	if (sysctl_tcp_westwood) {
		if (tcp_westwood_cwnd(tp)) {
			tp->snd_ssthresh = tp->snd_cwnd;
			ret = 1;
		}
	}

	return ret;
}

#endif	/* _TCP_H */
