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
 * $FreeBSD: src/sys/netinet/ip_dummynet.h,v 1.10.2.1 2000/06/11 18:39:44 luigi Exp $
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

/*
 * The key for the heap is used for two different values
   1. timer ticks- max 10K/second, so 32 bits are enough
   2. virtual times. These increase in steps of len/x, where len is the
      packet length, and x is either the weight of the flow, or the
      sum of all weights.
      If we limit to max 1000 flows and a max weight of 100, then
      x needs 17 bits. The packet size is 16 bits, so we can easily
      overflow if we do not allow errors.

 */
typedef u_int64_t dn_key ;      /* sorting key */
#define DN_KEY_LT(a,b)     ((int64_t)((a)-(b)) < 0)
#define DN_KEY_LEQ(a,b)    ((int64_t)((a)-(b)) <= 0)
#define DN_KEY_GT(a,b)     ((int64_t)((a)-(b)) > 0)
#define DN_KEY_GEQ(a,b)    ((int64_t)((a)-(b)) >= 0)
/* XXX check names of next two macros */
#define MAX64(x,y)  (( (int64_t) ( (y)-(x) )) > 0 ) ? (y) : (x)
#define MY_M	16 /* number of left shift to obtain a larger precision */
/*
 * XXX With this scaling, max 1000 flows, max weight 100, 1Gbit/s, the
 * virtual time wraps every 15 days.
 */

#define OFFSET_OF(type, field) ((int)&( ((type *)0)->field) )

struct dn_heap_entry {
    dn_key key ;	/* sorting key. Topmost element is smallest one */
    void *object ;	/* object pointer */
} ;

struct dn_heap {
    int size ;
    int elements ;
    int offset ; /* XXX if > 0 this is the offset of direct ptr to obj */
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

/*
 * Overall structure (with WFQ):

We have 3 data structures definining a pipe and associated queues:
 + dn_pipe, which contains the main configuration parameters related
   to delay and bandwidth
 + dn_flow_set which contains WFQ configuration, flow
   masks, plr and RED configuration
 + dn_flow_queue which is the per-flow queue.
 Multiple dn_flow_set can be linked to the same pipe, and multiple
 dn_flow_queue can be linked to the same dn_flow_set.

 During configuration we set the dn_flow_set and dn_pipe parameters.
 At runtime: packets are sent to the dn_flow_set (either WFQ ones, or
 the one embedded in the dn_pipe for fixed-rate flows) which in turn
 dispatches them to the appropriate dn_flow_queue (created dynamically
 according to the masks).
 The transmit clock for fixed rate flows (ready_event) selects the
 dn_flow_queue to be used to transmit the next packet. For WF2Q,
 wfq_ready_event() extract a pipe which in turn selects the right
 flow using a number of heaps defined into the pipe.

 *
 */

/*
 * We use per flow queues. Hashing is used to select the right slot,
 * then we scan the list to match the flow-id.
 */
struct dn_flow_queue {
    struct dn_flow_queue *next ;
    struct ipfw_flow_id id ;
    struct dn_pkt *head, *tail ;	/* queue of packets */
    u_int len ;
    u_int len_bytes ;
    long numbytes ;		/* credit for transmission (dynamic queues) */

    u_int64_t tot_pkts ;	/* statistics counters	*/
    u_int64_t tot_bytes ;
    u_int32_t drops ;
    int hash_slot ;	/* debugging/diagnostic */

    /* RED parameters */
    int avg ;                   /* average queue length est. (scaled) */
    int count ;                 /* arrivals since last RED drop */
    int random ;                /* random value (scaled) */
    u_int32_t q_time ;          /* start of queue idle time */

    /* WF2Q+ support */
    struct dn_flow_set *fs ; /* parent flow set */
    int blh_pos ;	/* position in backlogged_heap */
    dn_key sched_time ; /* current time when queue enters ready_heap */

    dn_key S,F ; /* start-time, finishing time */
} ;

struct dn_flow_set {
    struct dn_flow_set *next; /* next flow set in all_flow_sets list */

    u_short fs_nr ;             /* flow_set number       */
    u_short flags_fs;
#define DN_HAVE_FLOW_MASK	0x0001
#define DN_IS_PIPE		0x4000
#define DN_IS_QUEUE		0x8000
#define DN_IS_RED		0x0002
#define DN_IS_GENTLE_RED	0x0004
#define DN_QSIZE_IS_BYTES	0x0008	/* queue measured in bytes */

    struct dn_pipe *pipe ;		/* pointer to parent pipe */
    u_short parent_nr ;		/* parent pipe#, 0 if local to a pipe */

    int weight ; /* WFQ queue weight */
    int qsize ;		/* queue size in slots or bytes */
    int plr ;           /* pkt loss rate (2^31-1 means 100%) */

    struct ipfw_flow_id flow_mask ;
    /* hash table of queues onto this flow_set */
    int rq_size ;		/* number of slots */
    int rq_elements ;		/* active elements */
    struct dn_flow_queue **rq;	/* array of rq_size entries */
    u_int32_t last_expired ;	/* do not expire too frequently */
	/* XXX some RED parameters as well ? */
    int backlogged ;		/* #active queues for this flowset */

        /* RED parameters */
#define SCALE_RED               16
#define SCALE(x)                ( (x) << SCALE_RED )
#define SCALE_VAL(x)            ( (x) >> SCALE_RED )
#define SCALE_MUL(x,y)          ( ( (x) * (y) ) >> SCALE_RED )
    int w_q ;               /* queue weight (scaled) */
    int max_th ;            /* maximum threshold for queue (scaled) */
    int min_th ;            /* minimum threshold for queue (scaled) */
    int max_p ;             /* maximum value for p_b (scaled) */
    u_int c_1 ;             /* max_p/(max_th-min_th) (scaled) */
    u_int c_2 ;             /* max_p*min_th/(max_th-min_th) (scaled) */
    u_int c_3 ;             /* for GRED, (1-max_p)/max_th (scaled) */
    u_int c_4 ;             /* for GRED, 1 - 2*max_p (scaled) */
    u_int * w_q_lookup ;    /* lookup table for computing (1-w_q)^t */
    u_int lookup_depth ;    /* depth of lookup table */
    int lookup_step ;       /* granularity inside the lookup table */
    int lookup_weight ;     /* equal to (1-w_q)^t / (1-w_q)^(t+1) */
    int avg_pkt_size ;      /* medium packet size */
    int max_pkt_size ;      /* max packet size */
} ;

/*
 * Pipe descriptor. Contains global parameters, delay-line queue.
 * 
 * For WF2Q support it also has 3 heaps holding dn_flow_queue:
 *   not_eligible_heap, for queues whose start time is higher
 *	than the virtual time. Sorted by start time.
 *   scheduler_heap, for queues eligible for scheduling. Sorted by
 *	finish time.
 *   backlogged_heap, all flows in the two heaps above, sorted by
 *	start time. This is used to compute the virtual time.
 *
 */
struct dn_pipe {			/* a pipe */
	struct dn_pipe *next ;

    int	pipe_nr ;		/* number	*/
	int	bandwidth;		/* really, bytes/tick.	*/
	int	delay ;			/* really, ticks	*/

    struct	dn_pkt *head, *tail ;	/* packets in delay line */

    /* WF2Q+ */
    struct dn_heap scheduler_heap ; /* top extract - key Finish time*/
    struct dn_heap not_eligible_heap; /* top extract- key Start time */
    struct dn_heap backlogged_heap ; /* random extract - key Start time */

    dn_key V ; /* virtual time */
    int sum;	/* sum of weights of all active sessions */
    int numbytes;	/* bit i can transmit (more or less). */

    dn_key sched_time ; /* first time pipe is scheduled in ready_heap */

    /* the tx clock can come from an interface. In this case, the
     * name is below, and the pointer is filled when the rule is
     * configured. We identify this by setting the if_name to a
     * non-empty string.
     */
    char if_name[16];
    struct ifnet *ifp ;
    int ready ; /* set if ifp != NULL and we got a signal from it */

    struct dn_flow_set fs ; /* used with fixed-rate flows */
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
