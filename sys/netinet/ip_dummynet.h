/*
 * Copyright (c) 1998-2000 Luigi Rizzo, Universita` di Pisa
 * Portions Copyright (c) 2000 Akamba Corp.
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/netinet/ip_dummynet.h,v 1.10 2000/02/10 14:17:40 luigi Exp $
 */

#ifndef _IP_DUMMYNET_H
#define _IP_DUMMYNET_H

/*
 * Definition of dummynet data structures.
 * We first start with the heap which is used by the scheduler.
 *
 * Each list contains a set of parameters identifying the pipe, and
 * a set of packets queued on the pipe itself.
 *
 * I could have used queue macros, but the management i have
 * is pretty simple and this makes the code more portable.
 */

typedef u_int32_t dn_key ;      /* sorting key */
#define DN_KEY_LT(a,b)     ((int)((a)-(b)) < 0)
#define DN_KEY_LEQ(a,b)    ((int)((a)-(b)) <= 0)
#define DN_KEY_GT(a,b)     ((int)((a)-(b)) > 0)
#define DN_KEY_GEQ(a,b)    ((int)((a)-(b)) >= 0)

struct dn_heap_entry {
    dn_key key ;	/* sorting key. Topmost element is smallest one */
    void *object ;	/* object pointer */
} ;

struct dn_heap {
    int size ;
    int elements ;
    struct dn_heap_entry *p ;	/* really an array of "size" entries */
} ;

/*
 * MT_DUMMYNET is a new (fake) mbuf type that is prepended to the
 * packet when it comes out of a pipe. The definition
 * ought to go in /sys/sys/mbuf.h but here it is less intrusive.
 */

#define MT_DUMMYNET MT_CONTROL

/*
 * struct dn_pkt identifies a packet in the dummynet queue. The
 * first part is really an m_hdr for implementation purposes, and some
 * fields are saved there. When passing the packet back to the ip_input/
 * ip_output(), the struct is prepended to the mbuf chain with type
 * MT_DUMMYNET, and contains the pointer to the matching rule.
 */
struct dn_pkt {
	struct m_hdr hdr ;
#define dn_next	hdr.mh_nextpkt	/* next element in queue */
#define DN_NEXT(x)	(struct dn_pkt *)(x)->dn_next
#define dn_m	hdr.mh_next	/* packet to be forwarded */
/* #define dn_dst	hdr.mh_len -* dst, for ip_output		*/
#define dn_dir	hdr.mh_flags	/* action when pkt extracted from a queue */
#define DN_TO_IP_OUT	1
#define DN_TO_IP_IN	2
#define DN_TO_BDG_FWD	3

	dn_key  output_time;    /* when the pkt is due for delivery */
        struct ifnet *ifp;	/* interface, for ip_output		*/
	struct sockaddr_in *dn_dst ;
        struct route ro;	/* route, for ip_output. MUST COPY	*/
	int flags ;		/* flags, for ip_output (IPv6 ?) */
};

struct dn_queue {
	struct dn_pkt *head, *tail;
} ;

/*
 * We use per flow queues. Hashing is used to select the right slot,
 * then we scan the list to match the flow-id.
 * The pipe is shared as it is only a delay line and thus one is enough.
 */
struct dn_flow_queue {
    struct dn_flow_queue *next ;
    struct ipfw_flow_id id ;
    struct dn_pipe *p ;	/* parent pipe */
    struct dn_queue r;
    long numbytes ;
    u_int len ;
    u_int len_bytes ;

    u_int64_t tot_pkts ;	/* statistics counters	*/
    u_int64_t tot_bytes ;
    u_int32_t drops ;
    int hash_slot ;	/* debugging/diagnostic */
} ;

/*
 * Pipe descriptor. Contains global parameters, delay-line queue,
 * and the hash array of the per-flow queues.
 */
struct dn_pipe {			/* a pipe */
	struct dn_pipe *next ;

	u_short	pipe_nr ;		/* number	*/
	u_short	flags ;			/* to speed up things	*/
#define DN_HAVE_FLOW_MASK	8
	int	bandwidth;		/* really, bytes/tick.	*/
	int	queue_size ;
	int	queue_size_bytes ;
	int	delay ;			/* really, ticks	*/
	int	plr ;		/* pkt loss rate (2^31-1 means 100%) */

        struct	dn_queue p ;
	struct ipfw_flow_id flow_mask ;
	int rq_size ;
	int rq_elements ;
	struct dn_flow_queue **rq ;	/* array of rq_size entries */
};

#ifdef _KERNEL

MALLOC_DECLARE(M_IPFW);

typedef int ip_dn_ctl_t __P((struct sockopt *)) ;
extern ip_dn_ctl_t *ip_dn_ctl_ptr;

void dn_rule_delete(void *r);		/* used in ip_fw.c */
int dummynet_io(int pipe, int dir,
	struct mbuf *m, struct ifnet *ifp, struct route *ro,
	struct sockaddr_in * dst,
	struct ip_fw_chain *rule, int flags);
#endif

#endif /* _IP_DUMMYNET_H */
