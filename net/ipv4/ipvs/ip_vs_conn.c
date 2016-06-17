/*
 * IPVS         An implementation of the IP virtual server support for the
 *              LINUX operating system.  IPVS is now implemented as a module
 *              over the Netfilter framework. IPVS can be used to build a
 *              high-performance and highly available server based on a
 *              cluster of servers.
 *
 * Version:     $Id: ip_vs_conn.c,v 1.28.2.5 2003/08/09 13:27:08 wensong Exp $
 *
 * Authors:     Wensong Zhang <wensong@linuxvirtualserver.org>
 *              Peter Kese <peter.kese@ijs.si>
 *              Julian Anastasov <ja@ssi.bg>
 *
 *              This program is free software; you can redistribute it and/or
 *              modify it under the terms of the GNU General Public License
 *              as published by the Free Software Foundation; either version
 *              2 of the License, or (at your option) any later version.
 *
 * The IPVS code for kernel 2.2 was done by Wensong Zhang and Peter Kese,
 * with changes/fixes from Julian Anastasov, Lars Marowsky-Bree, Horms
 * and others. Many code here is taken from IP MASQ code of kernel 2.2.
 *
 * Changes:
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/vmalloc.h>
#include <linux/ip.h>
#include <linux/tcp.h>                  /* for tcphdr */
#include <linux/in.h>
#include <linux/proc_fs.h>              /* for proc_net_* */
#include <asm/softirq.h>                /* for local_bh_* */
#include <net/ip.h>
#include <net/tcp.h>                    /* for csum_tcpudp_magic */
#include <net/udp.h>
#include <net/icmp.h>                   /* for icmp_send */
#include <net/route.h>                  /* for ip_route_output */
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/jhash.h>
#include <linux/random.h>

#include <net/ip_vs.h>


/*
 *  Connection hash table: for input and output packets lookups of IPVS
 */
static struct list_head *ip_vs_conn_tab;

/* SLAB cache for IPVS connections */
static kmem_cache_t *ip_vs_conn_cachep;

/* counter for current IPVS connections */
static atomic_t ip_vs_conn_count = ATOMIC_INIT(0);

/* counter for no-client-port connections */
static atomic_t ip_vs_conn_no_cport_cnt = ATOMIC_INIT(0);

/* random value for IPVS connection hash */
static unsigned int ip_vs_conn_rnd;

/*
 *  Fine locking granularity for big connection hash table
 */
#define CT_LOCKARRAY_BITS  4
#define CT_LOCKARRAY_SIZE  (1<<CT_LOCKARRAY_BITS)
#define CT_LOCKARRAY_MASK  (CT_LOCKARRAY_SIZE-1)

struct ip_vs_aligned_lock
{
	rwlock_t	l;
} __attribute__((__aligned__(SMP_CACHE_BYTES)));

/* lock array for conn table */
struct ip_vs_aligned_lock
__ip_vs_conntbl_lock_array[CT_LOCKARRAY_SIZE] __cacheline_aligned;

static inline void ct_read_lock(unsigned key)
{
	read_lock(&__ip_vs_conntbl_lock_array[key&CT_LOCKARRAY_MASK].l);
}

static inline void ct_read_unlock(unsigned key)
{
	read_unlock(&__ip_vs_conntbl_lock_array[key&CT_LOCKARRAY_MASK].l);
}

static inline void ct_write_lock(unsigned key)
{
	write_lock(&__ip_vs_conntbl_lock_array[key&CT_LOCKARRAY_MASK].l);
}

static inline void ct_write_unlock(unsigned key)
{
	write_unlock(&__ip_vs_conntbl_lock_array[key&CT_LOCKARRAY_MASK].l);
}

static inline void ct_read_lock_bh(unsigned key)
{
	read_lock_bh(&__ip_vs_conntbl_lock_array[key&CT_LOCKARRAY_MASK].l);
}

static inline void ct_read_unlock_bh(unsigned key)
{
	read_unlock_bh(&__ip_vs_conntbl_lock_array[key&CT_LOCKARRAY_MASK].l);
}

static inline void ct_write_lock_bh(unsigned key)
{
	write_lock_bh(&__ip_vs_conntbl_lock_array[key&CT_LOCKARRAY_MASK].l);
}

static inline void ct_write_unlock_bh(unsigned key)
{
	write_unlock_bh(&__ip_vs_conntbl_lock_array[key&CT_LOCKARRAY_MASK].l);
}


/*
 *	Returns hash value for IPVS connection entry
 */
static unsigned
ip_vs_conn_hashkey(unsigned proto, __u32 addr, __u16 port)
{
	return jhash_3words(addr, port, proto, ip_vs_conn_rnd)
		& IP_VS_CONN_TAB_MASK;
}


/*
 *	Hashes ip_vs_conn in ip_vs_conn_tab by proto,addr,port.
 *	returns bool success.
 */
static int ip_vs_conn_hash(struct ip_vs_conn *cp)
{
	unsigned hash;
	int ret;

	/* Hash by protocol, client address and port */
	hash = ip_vs_conn_hashkey(cp->protocol, cp->caddr, cp->cport);

	ct_write_lock(hash);

	if (!(cp->flags & IP_VS_CONN_F_HASHED)) {
		list_add(&cp->c_list, &ip_vs_conn_tab[hash]);
		cp->flags |= IP_VS_CONN_F_HASHED;
		atomic_inc(&cp->refcnt);
		ret = 1;
	} else {
		IP_VS_ERR("ip_vs_conn_hash(): request for already hashed, "
			  "called from %p\n", __builtin_return_address(0));
		ret = 0;
	}

	ct_write_unlock(hash);

	return ret;
}


/*
 *	UNhashes ip_vs_conn from ip_vs_conn_tab.
 *	returns bool success.
 */
static int ip_vs_conn_unhash(struct ip_vs_conn *cp)
{
	unsigned hash;
	int ret;

	/* unhash it and decrease its reference counter */
	hash = ip_vs_conn_hashkey(cp->protocol, cp->caddr, cp->cport);
	ct_write_lock(hash);

	if (cp->flags & IP_VS_CONN_F_HASHED) {
		list_del(&cp->c_list);
		cp->flags &= ~IP_VS_CONN_F_HASHED;
		atomic_dec(&cp->refcnt);
		ret = 1;
	} else
		ret = 0;

	ct_write_unlock(hash);

	return ret;
}


/*
 *  Gets ip_vs_conn associated with supplied parameters in the ip_vs_conn_tab.
 *  Called for pkts coming from OUTside-to-INside.
 *	s_addr, s_port: pkt source address (foreign host)
 *	d_addr, d_port: pkt dest address (load balancer)
 */
static inline struct ip_vs_conn *__ip_vs_conn_in_get
(int protocol, __u32 s_addr, __u16 s_port, __u32 d_addr, __u16 d_port)
{
	unsigned hash;
	struct ip_vs_conn *cp;
	struct list_head *l,*e;

	hash = ip_vs_conn_hashkey(protocol, s_addr, s_port);
	l = &ip_vs_conn_tab[hash];

	ct_read_lock(hash);

	for (e=l->next; e!=l; e=e->next) {
		cp = list_entry(e, struct ip_vs_conn, c_list);
		if (s_addr==cp->caddr && s_port==cp->cport &&
		    d_port==cp->vport && d_addr==cp->vaddr &&
		    protocol==cp->protocol) {
			/* HIT */
			atomic_inc(&cp->refcnt);
			ct_read_unlock(hash);
			return cp;
		}
	}

	ct_read_unlock(hash);

	return NULL;
}

struct ip_vs_conn *ip_vs_conn_in_get
(int protocol, __u32 s_addr, __u16 s_port, __u32 d_addr, __u16 d_port)
{
	struct ip_vs_conn *cp;

	cp = __ip_vs_conn_in_get(protocol, s_addr, s_port, d_addr, d_port);
	if (!cp && atomic_read(&ip_vs_conn_no_cport_cnt))
		cp = __ip_vs_conn_in_get(protocol, s_addr, 0, d_addr, d_port);

	IP_VS_DBG(7, "lookup/in %s %u.%u.%u.%u:%d->%u.%u.%u.%u:%d %s\n",
		  ip_vs_proto_name(protocol),
		  NIPQUAD(s_addr), ntohs(s_port),
		  NIPQUAD(d_addr), ntohs(d_port),
		  cp?"hit":"not hit");

	return cp;
}


/*
 *  Gets ip_vs_conn associated with supplied parameters in the ip_vs_conn_tab.
 *  Called for pkts coming from inside-to-OUTside.
 *	s_addr, s_port: pkt source address (inside host)
 *	d_addr, d_port: pkt dest address (foreign host)
 */
struct ip_vs_conn *ip_vs_conn_out_get
(int protocol, __u32 s_addr, __u16 s_port, __u32 d_addr, __u16 d_port)
{
	unsigned hash;
	struct ip_vs_conn *cp, *ret=NULL;
	struct list_head *l,*e;

	/*
	 *	Check for "full" addressed entries
	 */
	hash = ip_vs_conn_hashkey(protocol, d_addr, d_port);
	l = &ip_vs_conn_tab[hash];

	ct_read_lock(hash);

	for (e=l->next; e!=l; e=e->next) {
		cp = list_entry(e, struct ip_vs_conn, c_list);
		if (d_addr == cp->caddr && d_port == cp->cport &&
		    s_port == cp->dport && s_addr == cp->daddr &&
		    protocol == cp->protocol) {
			/* HIT */
			atomic_inc(&cp->refcnt);
			ret = cp;
			break;
		}
	}

	ct_read_unlock(hash);

	IP_VS_DBG(7, "lookup/out %s %u.%u.%u.%u:%d->%u.%u.%u.%u:%d %s\n",
		  ip_vs_proto_name(protocol),
		  NIPQUAD(s_addr), ntohs(s_port),
		  NIPQUAD(d_addr), ntohs(d_port),
		  ret?"hit":"not hit");

	return ret;
}


/*
 *      Put back the conn and restart its timer with its timeout
 */
void ip_vs_conn_put(struct ip_vs_conn *cp)
{
	/* reset it expire in its timeout */
	mod_timer(&cp->timer, jiffies+cp->timeout);

	__ip_vs_conn_put(cp);
}


/*
 *	Timeout table[state]
 */
struct ip_vs_timeout_table vs_timeout_table = {
	ATOMIC_INIT(0),	/* refcnt */
	0,		/* scale  */
	{
		[IP_VS_S_NONE]          =	30*60*HZ,
		[IP_VS_S_ESTABLISHED]	=	15*60*HZ,
		[IP_VS_S_SYN_SENT]	=	2*60*HZ,
		[IP_VS_S_SYN_RECV]	=	1*60*HZ,
		[IP_VS_S_FIN_WAIT]	=	2*60*HZ,
		[IP_VS_S_TIME_WAIT]	=	2*60*HZ,
		[IP_VS_S_CLOSE]         =	10*HZ,
		[IP_VS_S_CLOSE_WAIT]	=	60*HZ,
		[IP_VS_S_LAST_ACK]	=	30*HZ,
		[IP_VS_S_LISTEN]	=	2*60*HZ,
		[IP_VS_S_SYNACK]	=	120*HZ,
		[IP_VS_S_UDP]		=	5*60*HZ,
		[IP_VS_S_ICMP]          =	1*60*HZ,
		[IP_VS_S_LAST]          =	2*HZ,
	},	/* timeout */
};


struct ip_vs_timeout_table vs_timeout_table_dos = {
	ATOMIC_INIT(0),	/* refcnt */
	0,		/* scale  */
	{
		[IP_VS_S_NONE]          =	15*60*HZ,
		[IP_VS_S_ESTABLISHED]	=	8*60*HZ,
		[IP_VS_S_SYN_SENT]	=	60*HZ,
		[IP_VS_S_SYN_RECV]	=	10*HZ,
		[IP_VS_S_FIN_WAIT]	=	60*HZ,
		[IP_VS_S_TIME_WAIT]	=	60*HZ,
		[IP_VS_S_CLOSE]         =	10*HZ,
		[IP_VS_S_CLOSE_WAIT]	=	60*HZ,
		[IP_VS_S_LAST_ACK]	=	30*HZ,
		[IP_VS_S_LISTEN]	=	2*60*HZ,
		[IP_VS_S_SYNACK]	=	100*HZ,
		[IP_VS_S_UDP]		=	3*60*HZ,
		[IP_VS_S_ICMP]          =	1*60*HZ,
		[IP_VS_S_LAST]          =	2*HZ,
	},	/* timeout */
};


/*
 *	Timeout table to use for the VS entries
 *	If NULL we use the default table (vs_timeout_table).
 *	Under flood attack we switch to vs_timeout_table_dos
 */

static struct ip_vs_timeout_table *ip_vs_timeout_table = &vs_timeout_table;

static const char * state_name_table[IP_VS_S_LAST+1] = {
	[IP_VS_S_NONE]          =	"NONE",
	[IP_VS_S_ESTABLISHED]	=	"ESTABLISHED",
	[IP_VS_S_SYN_SENT]	=	"SYN_SENT",
	[IP_VS_S_SYN_RECV]	=	"SYN_RECV",
	[IP_VS_S_FIN_WAIT]	=	"FIN_WAIT",
	[IP_VS_S_TIME_WAIT]	=	"TIME_WAIT",
	[IP_VS_S_CLOSE]         =	"CLOSE",
	[IP_VS_S_CLOSE_WAIT]	=	"CLOSE_WAIT",
	[IP_VS_S_LAST_ACK]	=	"LAST_ACK",
	[IP_VS_S_LISTEN]	=	"LISTEN",
	[IP_VS_S_SYNACK]	=	"SYNACK",
	[IP_VS_S_UDP]		=	"UDP",
	[IP_VS_S_ICMP]          =	"ICMP",
	[IP_VS_S_LAST]          =	"BUG!",
};

#define sNO IP_VS_S_NONE
#define sES IP_VS_S_ESTABLISHED
#define sSS IP_VS_S_SYN_SENT
#define sSR IP_VS_S_SYN_RECV
#define sFW IP_VS_S_FIN_WAIT
#define sTW IP_VS_S_TIME_WAIT
#define sCL IP_VS_S_CLOSE
#define sCW IP_VS_S_CLOSE_WAIT
#define sLA IP_VS_S_LAST_ACK
#define sLI IP_VS_S_LISTEN
#define sSA IP_VS_S_SYNACK

struct vs_tcp_states_t {
	int next_state[IP_VS_S_LAST];	/* should be _LAST_TCP */
};

const char * ip_vs_state_name(int state)
{
	if (state >= IP_VS_S_LAST)
		return "ERR!";
	return state_name_table[state] ? state_name_table[state] : "?";
}

static struct vs_tcp_states_t vs_tcp_states [] = {
/*	INPUT */
/*        sNO, sES, sSS, sSR, sFW, sTW, sCL, sCW, sLA, sLI, sSA	*/
/*syn*/ {{sSR, sES, sES, sSR, sSR, sSR, sSR, sSR, sSR, sSR, sSR }},
/*fin*/ {{sCL, sCW, sSS, sTW, sTW, sTW, sCL, sCW, sLA, sLI, sTW }},
/*ack*/ {{sCL, sES, sSS, sES, sFW, sTW, sCL, sCW, sCL, sLI, sES }},
/*rst*/ {{sCL, sCL, sCL, sSR, sCL, sCL, sCL, sCL, sLA, sLI, sSR }},

/*	OUTPUT */
/*        sNO, sES, sSS, sSR, sFW, sTW, sCL, sCW, sLA, sLI, sSA	*/
/*syn*/ {{sSS, sES, sSS, sSR, sSS, sSS, sSS, sSS, sSS, sLI, sSR }},
/*fin*/ {{sTW, sFW, sSS, sTW, sFW, sTW, sCL, sTW, sLA, sLI, sTW }},
/*ack*/ {{sES, sES, sSS, sES, sFW, sTW, sCL, sCW, sLA, sES, sES }},
/*rst*/ {{sCL, sCL, sSS, sCL, sCL, sTW, sCL, sCL, sCL, sCL, sCL }},

/*	INPUT-ONLY */
/*        sNO, sES, sSS, sSR, sFW, sTW, sCL, sCW, sLA, sLI, sSA	*/
/*syn*/ {{sSR, sES, sES, sSR, sSR, sSR, sSR, sSR, sSR, sSR, sSR }},
/*fin*/ {{sCL, sFW, sSS, sTW, sFW, sTW, sCL, sCW, sLA, sLI, sTW }},
/*ack*/ {{sCL, sES, sSS, sES, sFW, sTW, sCL, sCW, sCL, sLI, sES }},
/*rst*/ {{sCL, sCL, sCL, sSR, sCL, sCL, sCL, sCL, sLA, sLI, sCL }},
};

static struct vs_tcp_states_t vs_tcp_states_dos [] = {
/*	INPUT */
/*        sNO, sES, sSS, sSR, sFW, sTW, sCL, sCW, sLA, sLI, sSA	*/
/*syn*/ {{sSR, sES, sES, sSR, sSR, sSR, sSR, sSR, sSR, sSR, sSA }},
/*fin*/ {{sCL, sCW, sSS, sTW, sTW, sTW, sCL, sCW, sLA, sLI, sSA }},
/*ack*/ {{sCL, sES, sSS, sSR, sFW, sTW, sCL, sCW, sCL, sLI, sSA }},
/*rst*/ {{sCL, sCL, sCL, sSR, sCL, sCL, sCL, sCL, sLA, sLI, sCL }},

/*	OUTPUT */
/*        sNO, sES, sSS, sSR, sFW, sTW, sCL, sCW, sLA, sLI, sSA	*/
/*syn*/ {{sSS, sES, sSS, sSA, sSS, sSS, sSS, sSS, sSS, sLI, sSA }},
/*fin*/ {{sTW, sFW, sSS, sTW, sFW, sTW, sCL, sTW, sLA, sLI, sTW }},
/*ack*/ {{sES, sES, sSS, sES, sFW, sTW, sCL, sCW, sLA, sES, sES }},
/*rst*/ {{sCL, sCL, sSS, sCL, sCL, sTW, sCL, sCL, sCL, sCL, sCL }},

/*	INPUT-ONLY */
/*        sNO, sES, sSS, sSR, sFW, sTW, sCL, sCW, sLA, sLI, sSA	*/
/*syn*/ {{sSA, sES, sES, sSR, sSA, sSA, sSA, sSA, sSA, sSA, sSA }},
/*fin*/ {{sCL, sFW, sSS, sTW, sFW, sTW, sCL, sCW, sLA, sLI, sTW }},
/*ack*/ {{sCL, sES, sSS, sES, sFW, sTW, sCL, sCW, sCL, sLI, sES }},
/*rst*/ {{sCL, sCL, sCL, sSR, sCL, sCL, sCL, sCL, sLA, sLI, sCL }},
};

static struct vs_tcp_states_t *ip_vs_state_table = vs_tcp_states;

void ip_vs_secure_tcp_set(int on)
{
	if (on) {
		ip_vs_state_table = vs_tcp_states_dos;
		ip_vs_timeout_table = &vs_timeout_table_dos;
	} else {
		ip_vs_state_table = vs_tcp_states;
		ip_vs_timeout_table = &vs_timeout_table;
	}
}


static inline int vs_tcp_state_idx(struct tcphdr *th, int state_off)
{
	/*
	 *	[0-3]: input states, [4-7]: output, [8-11] input only states.
	 */
	if (th->rst)
		return state_off+3;
	if (th->syn)
		return state_off+0;
	if (th->fin)
		return state_off+1;
	if (th->ack)
		return state_off+2;
	return -1;
}


static inline int vs_set_state_timeout(struct ip_vs_conn *cp, int state)
{
	struct ip_vs_timeout_table *vstim = cp->timeout_table;

	/*
	 *	Use default timeout table if no specific for this entry
	 */
	if (!vstim)
		vstim = &vs_timeout_table;

	cp->timeout = vstim->timeout[cp->state=state];

	if (vstim->scale) {
		int scale = vstim->scale;

		if (scale<0)
			cp->timeout >>= -scale;
		else if (scale > 0)
			cp->timeout <<= scale;
	}

	return state;
}


static inline int
vs_tcp_state(struct ip_vs_conn *cp, int state_off, struct tcphdr *th)
{
	int state_idx;
	int new_state = IP_VS_S_CLOSE;

	/*
	 *    Update state offset to INPUT_ONLY if necessary
	 *    or delete NO_OUTPUT flag if output packet detected
	 */
	if (cp->flags & IP_VS_CONN_F_NOOUTPUT) {
		if (state_off == VS_STATE_OUTPUT)
			cp->flags &= ~IP_VS_CONN_F_NOOUTPUT;
		else
			state_off = VS_STATE_INPUT_ONLY;
	}

	if ((state_idx = vs_tcp_state_idx(th, state_off)) < 0) {
		IP_VS_DBG(8, "vs_tcp_state_idx(%d)=%d!!!\n",
			  state_off, state_idx);
		goto tcp_state_out;
	}

	new_state = ip_vs_state_table[state_idx].next_state[cp->state];

  tcp_state_out:
	if (new_state != cp->state) {
		struct ip_vs_dest *dest = cp->dest;

		IP_VS_DBG(8, "%s %s [%c%c%c%c] %u.%u.%u.%u:%d->"
			  "%u.%u.%u.%u:%d state: %s->%s cnt:%d\n",
			  ip_vs_proto_name(cp->protocol),
			  (state_off==VS_STATE_OUTPUT)?"output ":"input ",
			  th->syn? 'S' : '.',
			  th->fin? 'F' : '.',
			  th->ack? 'A' : '.',
			  th->rst? 'R' : '.',
			  NIPQUAD(cp->daddr), ntohs(cp->dport),
			  NIPQUAD(cp->caddr), ntohs(cp->cport),
			  ip_vs_state_name(cp->state),
			  ip_vs_state_name(new_state),
			  atomic_read(&cp->refcnt));
		if (dest) {
			if (!(cp->flags & IP_VS_CONN_F_INACTIVE) &&
			    (new_state != IP_VS_S_ESTABLISHED)) {
				atomic_dec(&dest->activeconns);
				atomic_inc(&dest->inactconns);
				cp->flags |= IP_VS_CONN_F_INACTIVE;
			} else if ((cp->flags & IP_VS_CONN_F_INACTIVE) &&
				   (new_state == IP_VS_S_ESTABLISHED)) {
				atomic_inc(&dest->activeconns);
				atomic_dec(&dest->inactconns);
				cp->flags &= ~IP_VS_CONN_F_INACTIVE;
			}
		}
	}

	return vs_set_state_timeout(cp, new_state);
}


/*
 *	Handle state transitions
 */
int ip_vs_set_state(struct ip_vs_conn *cp,
		    int state_off, struct iphdr *iph, void *tp)
{
	int ret;

	spin_lock(&cp->lock);
	switch (iph->protocol) {
	case IPPROTO_TCP:
		ret = vs_tcp_state(cp, state_off, tp);
		break;
	case IPPROTO_UDP:
		ret = vs_set_state_timeout(cp, IP_VS_S_UDP);
		break;
	case IPPROTO_ICMP:
		ret = vs_set_state_timeout(cp, IP_VS_S_ICMP);
		break;
	default:
		ret = -1;
	}
	spin_unlock(&cp->lock);

	return ret;
}


/*
 *	Set LISTEN timeout. (ip_vs_conn_put will setup timer)
 */
int ip_vs_conn_listen(struct ip_vs_conn *cp)
{
	vs_set_state_timeout(cp, IP_VS_S_LISTEN);
	return cp->timeout;
}


/*
 *      Bypass transmitter
 *      Let packets bypass the destination when the destination is not
 *      available, it may be only used in transparent cache cluster.
 */
static int ip_vs_bypass_xmit(struct sk_buff *skb, struct ip_vs_conn *cp)
{
	struct rtable *rt;			/* Route to the other host */
	struct iphdr  *iph = skb->nh.iph;
	u8     tos = iph->tos;
	int    mtu;

	EnterFunction(10);

	if (ip_route_output(&rt, iph->daddr, 0, RT_TOS(tos), 0)) {
		IP_VS_DBG_RL("ip_vs_bypass_xmit(): ip_route_output error, "
			     "dest: %u.%u.%u.%u\n", NIPQUAD(iph->daddr));
		goto tx_error_icmp;
	}

	/* MTU checking */
	mtu = rt->u.dst.pmtu;
	if ((skb->len > mtu) && (iph->frag_off&__constant_htons(IP_DF))) {
		ip_rt_put(rt);
		icmp_send(skb, ICMP_DEST_UNREACH,ICMP_FRAG_NEEDED, htonl(mtu));
		IP_VS_DBG_RL("ip_vs_bypass_xmit(): frag needed\n");
		goto tx_error;
	}

	/* update checksum because skb might be defragmented */
	ip_send_check(iph);

	if (unlikely(skb_headroom(skb) < rt->u.dst.dev->hard_header_len)) {
		if (skb_cow(skb, rt->u.dst.dev->hard_header_len)) {
			ip_rt_put(rt);
			IP_VS_ERR_RL("ip_vs_bypass_xmit(): no memory\n");
			goto tx_error;
		}
	}

	/* drop old route */
	dst_release(skb->dst);
	skb->dst = &rt->u.dst;

#ifdef CONFIG_NETFILTER_DEBUG
	skb->nf_debug = 1 << NF_IP_LOCAL_OUT;
#endif /* CONFIG_NETFILTER_DEBUG */
	skb->nfcache |= NFC_IPVS_PROPERTY;
	ip_send(skb);

	LeaveFunction(10);
	return NF_STOLEN;

  tx_error_icmp:
	dst_link_failure(skb);
  tx_error:
	kfree_skb(skb);
	return NF_STOLEN;
}


/*
 *      NULL transmitter (do nothing except return NF_ACCEPT)
 */
static int ip_vs_null_xmit(struct sk_buff *skb, struct ip_vs_conn *cp)
{
	return NF_ACCEPT;
}


/*
 *      NAT transmitter (only for outside-to-inside nat forwarding)
 */
static int ip_vs_nat_xmit(struct sk_buff *skb, struct ip_vs_conn *cp)
{
	struct rtable *rt;		/* Route to the other host */
	struct iphdr  *iph;
	union ip_vs_tphdr h;
	int ihl;
	unsigned short size;
	int mtu;

	EnterFunction(10);

	/*
	 * If it has ip_vs_app helper, the helper may change the payload,
	 * so it needs full checksum checking and checksum calculation.
	 * If not, only the header (such as IP address and port number)
	 * will be changed, so it is fast to do incremental checksum update,
	 * and let the destination host  do final checksum checking.
	 */

	if (cp->app && skb_is_nonlinear(skb)
	    && skb_linearize(skb, GFP_ATOMIC) != 0)
		return NF_DROP;

	iph = skb->nh.iph;
	ihl = iph->ihl << 2;
	h.raw = (char*) iph + ihl;
	size = ntohs(iph->tot_len) - ihl;

	/* do TCP/UDP checksum checking if it has application helper */
	if (cp->app && (iph->protocol != IPPROTO_UDP || h.uh->check != 0)) {
		switch (skb->ip_summed) {
		case CHECKSUM_NONE:
			skb->csum = csum_partial(h.raw, size, 0);

		case CHECKSUM_HW:
			if (csum_tcpudp_magic(iph->saddr, iph->daddr, size,
					      iph->protocol, skb->csum)) {
				IP_VS_DBG_RL("Incoming failed %s checksum "
					     "from %d.%d.%d.%d (size=%d)!\n",
					     ip_vs_proto_name(iph->protocol),
					     NIPQUAD(iph->saddr),
					     size);
				goto tx_error;
			}
			break;
		default:
			/* CHECKSUM_UNNECESSARY */
			break;
		}
	}

	/*
	 *  Check if it is no_cport connection ...
	 */
	if (unlikely(cp->flags & IP_VS_CONN_F_NO_CPORT)) {
		if (ip_vs_conn_unhash(cp)) {
			spin_lock(&cp->lock);
			if (cp->flags & IP_VS_CONN_F_NO_CPORT) {
				atomic_dec(&ip_vs_conn_no_cport_cnt);
				cp->flags &= ~IP_VS_CONN_F_NO_CPORT;
				cp->cport = h.portp[0];
				IP_VS_DBG(10, "filled cport=%d\n", ntohs(cp->dport));
			}
			spin_unlock(&cp->lock);

			/* hash on new dport */
			ip_vs_conn_hash(cp);
		}
	}

	if (!(rt = __ip_vs_get_out_rt(cp, RT_TOS(iph->tos))))
		goto tx_error_icmp;

	/* MTU checking */
	mtu = rt->u.dst.pmtu;
	if ((skb->len > mtu) && (iph->frag_off&__constant_htons(IP_DF))) {
		ip_rt_put(rt);
		icmp_send(skb, ICMP_DEST_UNREACH,ICMP_FRAG_NEEDED, htonl(mtu));
		IP_VS_DBG_RL("ip_vs_nat_xmit(): frag needed\n");
		goto tx_error;
	}

	/* drop old route */
	dst_release(skb->dst);
	skb->dst = &rt->u.dst;

	/* copy-on-write the packet before mangling it */
	if (ip_vs_skb_cow(skb, rt->u.dst.dev->hard_header_len, &iph, &h.raw))
		return NF_DROP;

	/* mangle the packet */
	iph->daddr = cp->daddr;
	h.portp[1] = cp->dport;

	/*
	 *	Attempt ip_vs_app call.
	 *	will fix ip_vs_conn and iph ack_seq stuff
	 */
	if (ip_vs_app_pkt_in(cp, skb) != 0) {
		/* skb data has probably changed, update pointers */
		iph = skb->nh.iph;
		h.raw = (char*) iph + ihl;
		size = skb->len - ihl;
	}

	/*
	 *	Adjust TCP/UDP checksums
	 */
	if (!cp->app && (iph->protocol != IPPROTO_UDP || h.uh->check != 0)) {
		/* Only port and addr are changed, do fast csum update */
		ip_vs_fast_check_update(&h, cp->vaddr, cp->daddr,
					cp->vport, cp->dport, iph->protocol);
		if (skb->ip_summed == CHECKSUM_HW)
			skb->ip_summed = CHECKSUM_NONE;
	} else {
		/* full checksum calculation */
		switch (iph->protocol) {
		case IPPROTO_TCP:
			h.th->check = 0;
			h.th->check = csum_tcpudp_magic(iph->saddr, iph->daddr,
							size, iph->protocol,
							csum_partial(h.raw, size, 0));
			break;
		case IPPROTO_UDP:
			h.uh->check = 0;
			h.uh->check = csum_tcpudp_magic(iph->saddr, iph->daddr,
							size, iph->protocol,
							csum_partial(h.raw, size, 0));
			if (h.uh->check == 0)
				h.uh->check = 0xFFFF;
			break;
		}
		skb->ip_summed = CHECKSUM_UNNECESSARY;
	}
	ip_send_check(iph);

	IP_VS_DBG(10, "NAT to %u.%u.%u.%u:%d\n",
		  NIPQUAD(iph->daddr), ntohs(h.portp[1]));

	/* FIXME: when application helper enlarges the packet and the length
	   is larger than the MTU of outgoing device, there will be still
	   MTU problem. */

#ifdef CONFIG_NETFILTER_DEBUG
	skb->nf_debug = 1 << NF_IP_LOCAL_OUT;
#endif /* CONFIG_NETFILTER_DEBUG */
	skb->nfcache |= NFC_IPVS_PROPERTY;
	ip_send(skb);

	LeaveFunction(10);
	return NF_STOLEN;

  tx_error_icmp:
	dst_link_failure(skb);
  tx_error:
	kfree_skb(skb);
	return NF_STOLEN;
}


/*
 *   IP Tunneling transmitter
 *
 *   This function encapsulates the packet in a new IP packet, its
 *   destination will be set to cp->daddr. Most code of this function
 *   is taken from ipip.c.
 *
 *   It is used in VS/TUN cluster. The load balancer selects a real
 *   server from a cluster based on a scheduling algorithm,
 *   encapsulates the request packet and forwards it to the selected
 *   server. For example, all real servers are configured with
 *   "ifconfig tunl0 <Virtual IP Address> up". When the server receives
 *   the encapsulated packet, it will decapsulate the packet, processe
 *   the request and return the response packets directly to the client
 *   without passing the load balancer. This can greatly increase the
 *   scalability of virtual server.
 */
static int ip_vs_tunnel_xmit(struct sk_buff *skb, struct ip_vs_conn *cp)
{
	struct rtable *rt;			/* Route to the other host */
	struct net_device *tdev;		/* Device to other host */
	struct iphdr  *old_iph = skb->nh.iph;
	u8     tos = old_iph->tos;
	u16    df = old_iph->frag_off;
	struct iphdr  *iph;			/* Our new IP header */
	int    max_headroom;			/* The extra header space needed */
	int    mtu;

	EnterFunction(10);

	if (skb->protocol != __constant_htons(ETH_P_IP)) {
		IP_VS_DBG_RL("ip_vs_tunnel_xmit(): protocol error, "
			     "ETH_P_IP: %d, skb protocol: %d\n",
			     __constant_htons(ETH_P_IP), skb->protocol);
		goto tx_error;
	}

	if (!(rt = __ip_vs_get_out_rt(cp, RT_TOS(tos))))
		goto tx_error_icmp;

	tdev = rt->u.dst.dev;

	mtu = rt->u.dst.pmtu - sizeof(struct iphdr);
	if (mtu < 68) {
		ip_rt_put(rt);
		IP_VS_DBG_RL("ip_vs_tunnel_xmit(): mtu less than 68\n");
		goto tx_error;
	}
	if (skb->dst && mtu < skb->dst->pmtu)
		skb->dst->pmtu = mtu;

	df |= (old_iph->frag_off&__constant_htons(IP_DF));

	if ((old_iph->frag_off&__constant_htons(IP_DF))
	    && mtu < ntohs(old_iph->tot_len)) {
		icmp_send(skb, ICMP_DEST_UNREACH,ICMP_FRAG_NEEDED, htonl(mtu));
		ip_rt_put(rt);
		IP_VS_DBG_RL("ip_vs_tunnel_xmit(): frag needed\n");
		goto tx_error;
	}

	/* update checksum because skb might be defragmented */
	ip_send_check(old_iph);

	/*
	 * Okay, now see if we can stuff it in the buffer as-is.
	 */
	max_headroom = (((tdev->hard_header_len+15)&~15)+sizeof(struct iphdr));

	if (skb_headroom(skb) < max_headroom
	    || skb_cloned(skb) || skb_shared(skb)) {
		struct sk_buff *new_skb =
			skb_realloc_headroom(skb, max_headroom);
		if (!new_skb) {
			ip_rt_put(rt);
			IP_VS_ERR_RL("ip_vs_tunnel_xmit(): no memory\n");
			return NF_DROP;
		}
		kfree_skb(skb);
		skb = new_skb;
		old_iph = skb->nh.iph;
	}

	skb->h.raw = skb->nh.raw;
	skb->nh.raw = skb_push(skb, sizeof(struct iphdr));
	memset(&(IPCB(skb)->opt), 0, sizeof(IPCB(skb)->opt));

	/* drop old route */
	dst_release(skb->dst);
	skb->dst = &rt->u.dst;

	/*
	 *	Push down and install the IPIP header.
	 */
	iph			=	skb->nh.iph;
	iph->version		=	4;
	iph->ihl		=	sizeof(struct iphdr)>>2;
	iph->frag_off		=	df;
	iph->protocol		=	IPPROTO_IPIP;
	iph->tos		=	tos;
	iph->daddr		=	rt->rt_dst;
	iph->saddr		=	rt->rt_src;
	iph->ttl		=	old_iph->ttl;
	iph->tot_len		=	htons(skb->len);
	ip_select_ident(iph, &rt->u.dst, NULL);
	ip_send_check(iph);

	skb->ip_summed = CHECKSUM_NONE;
#ifdef CONFIG_NETFILTER_DEBUG
	skb->nf_debug = 1 << NF_IP_LOCAL_OUT;
#endif /* CONFIG_NETFILTER_DEBUG */
	skb->nfcache |= NFC_IPVS_PROPERTY;
	ip_send(skb);

	LeaveFunction(10);

	return NF_STOLEN;

  tx_error_icmp:
	dst_link_failure(skb);
  tx_error:
	kfree_skb(skb);
	return NF_STOLEN;
}


/*
 *      Direct Routing transmitter
 */
static int ip_vs_dr_xmit(struct sk_buff *skb, struct ip_vs_conn *cp)
{
	struct rtable *rt;			/* Route to the other host */
	struct iphdr  *iph = skb->nh.iph;
	int    mtu;

	EnterFunction(10);

	if (!(rt = __ip_vs_get_out_rt(cp, RT_TOS(iph->tos))))
		goto tx_error_icmp;

	/* MTU checking */
	mtu = rt->u.dst.pmtu;
	if ((iph->frag_off&__constant_htons(IP_DF)) && skb->len > mtu) {
		icmp_send(skb, ICMP_DEST_UNREACH,ICMP_FRAG_NEEDED, htonl(mtu));
		ip_rt_put(rt);
		IP_VS_DBG_RL("ip_vs_dr_xmit(): frag needed\n");
		goto tx_error;
	}

	/* update checksum because skb might be defragmented */
	ip_send_check(iph);

	if (unlikely(skb_headroom(skb) < rt->u.dst.dev->hard_header_len)) {
		if (skb_cow(skb, rt->u.dst.dev->hard_header_len)) {
			ip_rt_put(rt);
			IP_VS_ERR_RL("ip_vs_dr_xmit(): no memory\n");
			goto tx_error;
		}
	}

	/* drop old route */
	dst_release(skb->dst);
	skb->dst = &rt->u.dst;

#ifdef CONFIG_NETFILTER_DEBUG
	skb->nf_debug = 1 << NF_IP_LOCAL_OUT;
#endif /* CONFIG_NETFILTER_DEBUG */
	skb->nfcache |= NFC_IPVS_PROPERTY;
	ip_send(skb);

#if 0000
	NF_HOOK(PF_INET, NF_IP_LOCAL_OUT, skb, NULL, rt->u.dst.dev,
		do_ip_send);
#endif
	LeaveFunction(10);
	return NF_STOLEN;

  tx_error_icmp:
	dst_link_failure(skb);
  tx_error:
	kfree_skb(skb);
	return NF_STOLEN;
}


/*
 *  Bind a connection entry with the corresponding packet_xmit.
 *  Called by ip_vs_conn_new.
 */
static inline void ip_vs_bind_xmit(struct ip_vs_conn *cp)
{
	switch (IP_VS_FWD_METHOD(cp)) {
	case IP_VS_CONN_F_MASQ:
		cp->packet_xmit = ip_vs_nat_xmit;
		break;

	case IP_VS_CONN_F_TUNNEL:
		cp->packet_xmit = ip_vs_tunnel_xmit;
		break;

	case IP_VS_CONN_F_DROUTE:
		cp->packet_xmit = ip_vs_dr_xmit;
		break;

	case IP_VS_CONN_F_LOCALNODE:
		cp->packet_xmit = ip_vs_null_xmit;
		break;

	case IP_VS_CONN_F_BYPASS:
		cp->packet_xmit = ip_vs_bypass_xmit;
		break;
	}
}


/*
 *  Bind a connection entry with a virtual service destination
 *  Called just after a new connection entry is created.
 */
static inline void
ip_vs_bind_dest(struct ip_vs_conn *cp, struct ip_vs_dest *dest)
{
	/* if dest is NULL, then return directly */
	if (!dest)
		return;

	/* Increase the refcnt counter of the dest */
	atomic_inc(&dest->refcnt);

	/* Bind with the destination and its corresponding transmitter */
	cp->flags |= atomic_read(&dest->conn_flags);
	cp->dest = dest;

	IP_VS_DBG(9, "Bind-dest %s c:%u.%u.%u.%u:%d v:%u.%u.%u.%u:%d "
		  "d:%u.%u.%u.%u:%d fwd:%c s:%s flg:%X cnt:%d destcnt:%d\n",
		  ip_vs_proto_name(cp->protocol),
		  NIPQUAD(cp->caddr), ntohs(cp->cport),
		  NIPQUAD(cp->vaddr), ntohs(cp->vport),
		  NIPQUAD(cp->daddr), ntohs(cp->dport),
		  ip_vs_fwd_tag(cp), ip_vs_state_name(cp->state),
		  cp->flags, atomic_read(&cp->refcnt),
		  atomic_read(&dest->refcnt));
}


/*
 *  Unbind a connection entry with its VS destination
 *  Called by the ip_vs_conn_expire function.
 */
static inline void ip_vs_unbind_dest(struct ip_vs_conn *cp)
{
	struct ip_vs_dest *dest = cp->dest;

	/* if dest is NULL, then return directly */
	if (!dest)
		return;

	IP_VS_DBG(9, "Unbind-dest %s c:%u.%u.%u.%u:%d "
		  "v:%u.%u.%u.%u:%d d:%u.%u.%u.%u:%d fwd:%c "
		  "s:%s flg:%X cnt:%d destcnt:%d",
		  ip_vs_proto_name(cp->protocol),
		  NIPQUAD(cp->caddr), ntohs(cp->cport),
		  NIPQUAD(cp->vaddr), ntohs(cp->vport),
		  NIPQUAD(cp->daddr), ntohs(cp->dport),
		  ip_vs_fwd_tag(cp), ip_vs_state_name(cp->state),
		  cp->flags, atomic_read(&cp->refcnt),
		  atomic_read(&dest->refcnt));

	/*
	 * Decrease the inactconns or activeconns counter
	 * if it is not a connection template ((cp->cport!=0)
	 *   || (cp->flags & IP_VS_CONN_F_NO_CPORT)).
	 */
	if (cp->cport || (cp->flags & IP_VS_CONN_F_NO_CPORT)) {
		if (cp->flags & IP_VS_CONN_F_INACTIVE) {
			atomic_dec(&dest->inactconns);
		} else {
			atomic_dec(&dest->activeconns);
		}
	}

	/*
	 * Simply decrease the refcnt of the dest, because the
	 * dest will be either in service's destination list
	 * or in the trash.
	 */
	atomic_dec(&dest->refcnt);
}


/*
 *  Checking if the destination of a connection template is available.
 *  If available, return 1, otherwise invalidate this connection
 *  template and return 0.
 */
int ip_vs_check_template(struct ip_vs_conn *ct)
{
	struct ip_vs_dest *dest = ct->dest;

	/*
	 * Checking the dest server status.
	 */
	if ((dest == NULL) ||
	    !(dest->flags & IP_VS_DEST_F_AVAILABLE)) {
		IP_VS_DBG(9, "check_template: dest not available for "
			  "protocol %s s:%u.%u.%u.%u:%d v:%u.%u.%u.%u:%d "
			  "-> d:%u.%u.%u.%u:%d\n",
			  ip_vs_proto_name(ct->protocol),
			  NIPQUAD(ct->caddr), ntohs(ct->cport),
			  NIPQUAD(ct->vaddr), ntohs(ct->vport),
			  NIPQUAD(ct->daddr), ntohs(ct->dport));

		/*
		 * Invalidate the connection template
		 */
		if (ct->cport) {
			if (ip_vs_conn_unhash(ct)) {
				ct->dport = 65535;
				ct->vport = 65535;
				ct->cport = 0;
				ip_vs_conn_hash(ct);
			}
		}

		/*
		 * Simply decrease the refcnt of the template,
		 * don't restart its timer.
		 */
		atomic_dec(&ct->refcnt);
		return 0;
	}
	return 1;
}


static inline void
ip_vs_timeout_attach(struct ip_vs_conn *cp, struct ip_vs_timeout_table *vstim)
{
	atomic_inc(&vstim->refcnt);
	cp->timeout_table = vstim;
}

static inline void ip_vs_timeout_detach(struct ip_vs_conn *cp)
{
	struct ip_vs_timeout_table *vstim = cp->timeout_table;

	if (!vstim)
		return;
	cp->timeout_table = NULL;
	atomic_dec(&vstim->refcnt);
}


static void ip_vs_conn_expire(unsigned long data)
{
	struct ip_vs_conn *cp = (struct ip_vs_conn *)data;

	if (cp->timeout_table)
		cp->timeout = cp->timeout_table->timeout[IP_VS_S_TIME_WAIT];
	else
		cp->timeout = vs_timeout_table.timeout[IP_VS_S_TIME_WAIT];

	/*
	 *	hey, I'm using it
	 */
	atomic_inc(&cp->refcnt);

	/*
	 *	do I control anybody?
	 */
	if (atomic_read(&cp->n_control))
		goto expire_later;

	/*
	 *	unhash it if it is hashed in the conn table
	 */
	if (!ip_vs_conn_unhash(cp))
		goto expire_later;

	/*
	 *	refcnt==1 implies I'm the only one referrer
	 */
	if (likely(atomic_read(&cp->refcnt) == 1)) {
		/* make sure that there is no timer on it now */
		if (timer_pending(&cp->timer))
			del_timer(&cp->timer);

		/* does anybody control me? */
		if (cp->control)
			ip_vs_control_del(cp);

		ip_vs_unbind_dest(cp);
		ip_vs_unbind_app(cp);
		ip_vs_timeout_detach(cp);
		if (cp->flags & IP_VS_CONN_F_NO_CPORT)
			atomic_dec(&ip_vs_conn_no_cport_cnt);
		atomic_dec(&ip_vs_conn_count);

		kmem_cache_free(ip_vs_conn_cachep, cp);
		return;
	}

	/* hash it back to the table */
	ip_vs_conn_hash(cp);

  expire_later:
	IP_VS_DBG(7, "delayed: refcnt-1=%d conn.n_control=%d\n",
		  atomic_read(&cp->refcnt)-1,
		  atomic_read(&cp->n_control));

	ip_vs_conn_put(cp);
}


void ip_vs_conn_expire_now(struct ip_vs_conn *cp)
{
	cp->timeout = 0;
	mod_timer(&cp->timer, jiffies);
	__ip_vs_conn_put(cp);
}

/*
 *  Create a new connection entry and hash it into the ip_vs_conn_tab.
 */
struct ip_vs_conn *
ip_vs_conn_new(int proto, __u32 caddr, __u16 cport, __u32 vaddr, __u16 vport,
	       __u32 daddr, __u16 dport, unsigned flags,
	       struct ip_vs_dest *dest)
{
	struct ip_vs_conn *cp;

	cp = kmem_cache_alloc(ip_vs_conn_cachep, GFP_ATOMIC);
	if (cp == NULL) {
		IP_VS_ERR_RL("ip_vs_conn_new: no memory available.\n");
		return NULL;
	}

	memset(cp, 0, sizeof(*cp));
	INIT_LIST_HEAD(&cp->c_list);
	init_timer(&cp->timer);
	cp->timer.data     = (unsigned long)cp;
	cp->timer.function = ip_vs_conn_expire;
	ip_vs_timeout_attach(cp, ip_vs_timeout_table);
	cp->protocol	   = proto;
	cp->caddr	   = caddr;
	cp->cport	   = cport;
	cp->vaddr	   = vaddr;
	cp->vport	   = vport;
	cp->daddr          = daddr;
	cp->dport          = dport;
	cp->flags	   = flags;
	cp->app_data	   = NULL;
	cp->control	   = NULL;
	cp->lock           = SPIN_LOCK_UNLOCKED;

	atomic_set(&cp->n_control, 0);
	atomic_set(&cp->in_pkts, 0);

	atomic_inc(&ip_vs_conn_count);
	if (flags & IP_VS_CONN_F_NO_CPORT)
		atomic_inc(&ip_vs_conn_no_cport_cnt);

	/* Bind its application helper (only for VS/NAT) if any */
	ip_vs_bind_app(cp);

	/* Bind the connection with a destination server */
	ip_vs_bind_dest(cp, dest);

	/* Set its state and timeout */
	vs_set_state_timeout(cp, IP_VS_S_NONE);

	/* Bind its packet transmitter */
	ip_vs_bind_xmit(cp);

	/*
	 * Set the entry is referenced by the current thread before hashing
	 * it in the table, so that other thread run ip_vs_random_dropentry
	 * but cannot drop this entry.
	 */
	atomic_set(&cp->refcnt, 1);

	/* Hash it in the ip_vs_conn_tab finally */
	ip_vs_conn_hash(cp);

	return cp;
}


/*
 *	/proc/net/ip_vs_conn entries
 */
static int
ip_vs_conn_getinfo(char *buffer, char **start, off_t offset, int length)
{
	off_t pos=0;
	int idx, len=0;
	char temp[70];
	struct ip_vs_conn *cp;
	struct list_head *l, *e;

	pos = 128;
	if (pos > offset) {
		len += sprintf(buffer+len, "%-127s\n",
			       "Pro FromIP   FPrt ToIP     TPrt DestIP   DPrt State       Expires");
	}

	for(idx = 0; idx < IP_VS_CONN_TAB_SIZE; idx++) {
		/*
		 *	Lock is actually only need in next loop
		 *	we are called from uspace: must stop bh.
		 */
		ct_read_lock_bh(idx);

		l = &ip_vs_conn_tab[idx];
		for (e=l->next; e!=l; e=e->next) {
			cp = list_entry(e, struct ip_vs_conn, c_list);
			pos += 128;
			if (pos <= offset)
				continue;
			sprintf(temp,
				"%-3s %08X %04X %08X %04X %08X %04X %-11s %7lu",
				ip_vs_proto_name(cp->protocol),
				ntohl(cp->caddr), ntohs(cp->cport),
				ntohl(cp->vaddr), ntohs(cp->vport),
				ntohl(cp->daddr), ntohs(cp->dport),
				ip_vs_state_name(cp->state),
				(cp->timer.expires-jiffies)/HZ);
			len += sprintf(buffer+len, "%-127s\n", temp);
			if (pos >= offset+length) {
				ct_read_unlock_bh(idx);
				goto done;
			}
		}
		ct_read_unlock_bh(idx);
	}

  done:
	*start = buffer+len-(pos-offset);       /* Start of wanted data */
	len = pos-offset;
	if (len > length)
		len = length;
	if (len < 0)
		len = 0;
	return len;
}


/*
 *      Randomly drop connection entries before running out of memory
 */
static inline int todrop_entry(struct ip_vs_conn *cp)
{
	/*
	 * The drop rate array needs tuning for real environments.
	 * Called from timer bh only => no locking
	 */
	static char todrop_rate[9] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
	static char todrop_counter[9] = {0};
	int i;

	/* if the conn entry hasn't lasted for 60 seconds, don't drop it.
	   This will leave enough time for normal connection to get
	   through. */
	if (cp->timeout+jiffies-cp->timer.expires < 60*HZ)
		return 0;

	/* Don't drop the entry if its number of incoming packets is not
	   located in [0, 8] */
	i = atomic_read(&cp->in_pkts);
	if (i > 8 || i < 0) return 0;

	if (!todrop_rate[i]) return 0;
	if (--todrop_counter[i] > 0) return 0;

	todrop_counter[i] = todrop_rate[i];
	return 1;
}


void ip_vs_random_dropentry(void)
{
	int idx;
	struct ip_vs_conn *cp;
	struct list_head *l,*e;
	struct ip_vs_conn *ct;

	/*
	 * Randomly scan 1/32 of the whole table every second
	 */
	for (idx=0; idx<(IP_VS_CONN_TAB_SIZE>>5); idx++) {
		unsigned hash = net_random()&IP_VS_CONN_TAB_MASK;

		/*
		 *  Lock is actually needed in this loop.
		 */
		ct_write_lock(hash);

		l = &ip_vs_conn_tab[hash];
		for (e=l->next; e!=l; e=e->next) {
			cp = list_entry(e, struct ip_vs_conn, c_list);
			if (!cp->cport && !(cp->flags & IP_VS_CONN_F_NO_CPORT))
				/* connection template */
				continue;
			switch(cp->state) {
			case IP_VS_S_SYN_RECV:
			case IP_VS_S_SYNACK:
				break;

			case IP_VS_S_ESTABLISHED:
			case IP_VS_S_UDP:
				if (todrop_entry(cp))
					break;
				continue;

			default:
				continue;
			}

			/*
			 * Drop the entry, and drop its ct if not referenced
			 */
			atomic_inc(&cp->refcnt);
			ct_write_unlock(hash);

			if ((ct = cp->control))
				atomic_inc(&ct->refcnt);
			IP_VS_DBG(4, "del connection\n");
			ip_vs_conn_expire_now(cp);
			if (ct) {
				IP_VS_DBG(4, "del conn template\n");
				ip_vs_conn_expire_now(ct);
			}
			ct_write_lock(hash);
		}
		ct_write_unlock(hash);
	}
}


/*
 *      Flush all the connection entries in the ip_vs_conn_tab
 */
static void ip_vs_conn_flush(void)
{
	int idx;
	struct ip_vs_conn *cp;
	struct list_head *l,*e;
	struct ip_vs_conn *ct;

  flush_again:
	for (idx=0; idx<IP_VS_CONN_TAB_SIZE; idx++) {
		/*
		 *  Lock is actually needed in this loop.
		 */
		ct_write_lock_bh(idx);

		l = &ip_vs_conn_tab[idx];
		for (e=l->next; e!=l; e=e->next) {
			cp = list_entry(e, struct ip_vs_conn, c_list);
			atomic_inc(&cp->refcnt);
			ct_write_unlock(idx);

			if ((ct = cp->control))
				atomic_inc(&ct->refcnt);
			IP_VS_DBG(4, "del connection\n");
			ip_vs_conn_expire_now(cp);
			if (ct) {
				IP_VS_DBG(4, "del conn template\n");
				ip_vs_conn_expire_now(ct);
			}
			ct_write_lock(idx);
		}
		ct_write_unlock_bh(idx);
	}

	/* the counter may be not NULL, because maybe some conn entries
	   are run by slow timer handler or unhashed but still referred */
	if (atomic_read(&ip_vs_conn_count) != 0) {
		schedule();
		goto flush_again;
	}
}


int ip_vs_conn_init(void)
{
	int idx;

	/*
	 * Allocate the connection hash table and initialize its list heads
	 */
	ip_vs_conn_tab = vmalloc(IP_VS_CONN_TAB_SIZE*sizeof(struct list_head));
	if (!ip_vs_conn_tab)
		return -ENOMEM;

	IP_VS_INFO("Connection hash table configured "
		   "(size=%d, memory=%ldKbytes)\n",
		   IP_VS_CONN_TAB_SIZE,
		   (long)(IP_VS_CONN_TAB_SIZE*sizeof(struct list_head))/1024);
	IP_VS_DBG(0, "Each connection entry needs %d bytes at least\n",
		  sizeof(struct ip_vs_conn));

	for (idx = 0; idx < IP_VS_CONN_TAB_SIZE; idx++) {
		INIT_LIST_HEAD(&ip_vs_conn_tab[idx]);
	}

	for (idx = 0; idx < CT_LOCKARRAY_SIZE; idx++)  {
		__ip_vs_conntbl_lock_array[idx].l = RW_LOCK_UNLOCKED;
	}

	/* Allocate ip_vs_conn slab cache */
	ip_vs_conn_cachep = kmem_cache_create("ip_vs_conn",
					      sizeof(struct ip_vs_conn), 0,
					      SLAB_HWCACHE_ALIGN, NULL, NULL);
	if (!ip_vs_conn_cachep) {
		vfree(ip_vs_conn_tab);
		return -ENOMEM;
	}

	proc_net_create("ip_vs_conn", 0, ip_vs_conn_getinfo);

	/* calculate the random value for connection hash */
	get_random_bytes(&ip_vs_conn_rnd, sizeof(ip_vs_conn_rnd));

	return 0;
}

void ip_vs_conn_cleanup(void)
{
	/* flush all the connection entries first */
	ip_vs_conn_flush();

	/* Release the empty cache */
	kmem_cache_destroy(ip_vs_conn_cachep);
	proc_net_remove("ip_vs_conn");
	vfree(ip_vs_conn_tab);
}
