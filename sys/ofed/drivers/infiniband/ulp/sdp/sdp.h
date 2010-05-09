#ifndef _SDP_H_
#define _SDP_H_

#include <linux/workqueue.h>
#include <linux/wait.h>
#include <net/inet_sock.h>
#include <net/tcp.h> /* For urgent data flags */
#include <rdma/ib_verbs.h>
#include <linux/sched.h>
#include <rdma/rdma_cm.h>
#include <rdma/ib_cm.h>
#include "sdp_dbg.h"

/* Interval between sucessive polls in the Tx routine when polling is used
   instead of interrupts (in per-core Tx rings) - should be power of 2 */
#define SDP_TX_POLL_MODER	16
#define SDP_TX_POLL_TIMEOUT	(HZ / 20)
#define SDP_NAGLE_TIMEOUT (HZ / 10)

#define SDP_SRCAVAIL_CANCEL_TIMEOUT (HZ * 5)
#define SDP_SRCAVAIL_ADV_TIMEOUT (1 * HZ)
#define SDP_SRCAVAIL_PAYLOAD_LEN 1

#define SDP_RESOLVE_TIMEOUT 1000
#define SDP_ROUTE_TIMEOUT 1000
#define SDP_RETRY_COUNT 5
#define SDP_KEEPALIVE_TIME (120 * 60 * HZ)
#define SDP_FIN_WAIT_TIMEOUT (60 * HZ) /* like TCP_FIN_TIMEOUT */

#define SDP_TX_SIZE 0x40
#define SDP_RX_SIZE 0x40

#define SDP_FMR_SIZE (MIN(0x1000, PAGE_SIZE) / sizeof(u64))
#define SDP_FMR_POOL_SIZE	1024
#define SDP_FMR_DIRTY_SIZE	( SDP_FMR_POOL_SIZE / 4 )

#define SDP_MAX_RDMA_READ_LEN (PAGE_SIZE * (SDP_FMR_SIZE - 2))

#define SDP_MAX_RECV_SGES 9 /* 1 for sdp header + 8 for payload */
#define SDP_MAX_SEND_SGES 9 /* same as above */

/* skb inlined data len - rest will be rx'ed into frags */
#define SDP_SKB_HEAD_SIZE (0x500 + sizeof(struct sdp_bsdh))

/* limit tx payload len, if the sink supports bigger buffers than the source
 * can handle.
 * or rx fragment size (limited by sge->length size) */
#define SDP_MAX_PAYLOAD ((1 << 16) - SDP_SKB_HEAD_SIZE)

#define SDP_NUM_WC 4

#define SDP_DEF_ZCOPY_THRESH 64*1024
#define SDP_MIN_ZCOPY_THRESH PAGE_SIZE
#define SDP_MAX_ZCOPY_THRESH 1048576

#define SDP_OP_RECV 0x800000000LL
#define SDP_OP_SEND 0x400000000LL
#define SDP_OP_RDMA 0x200000000LL
#define SDP_OP_NOP  0x100000000LL

/* how long (in jiffies) to block sender till tx completion*/
#define SDP_BZCOPY_POLL_TIMEOUT (HZ / 10)

#define SDP_AUTO_CONF	0xffff
#define AUTO_MOD_DELAY (HZ / 4)

struct sdp_skb_cb {
	__u32		seq;		/* Starting sequence number	*/
	__u32		end_seq;	/* SEQ + FIN + SYN + datalen	*/
	__u8		flags;		/* TCP header flags.		*/
	struct bzcopy_state      *bz;
	struct rx_srcavail_state *rx_sa;
	struct tx_srcavail_state *tx_sa;
};

#define SDP_SKB_CB(__skb)      ((struct sdp_skb_cb *)&((__skb)->cb[0]))
#define BZCOPY_STATE(skb)      (SDP_SKB_CB(skb)->bz)
#define RX_SRCAVAIL_STATE(skb) (SDP_SKB_CB(skb)->rx_sa)
#define TX_SRCAVAIL_STATE(skb) (SDP_SKB_CB(skb)->tx_sa)

#ifndef MIN
#define MIN(a, b) (a < b ? a : b)
#endif

#define ring_head(ring)   (atomic_read(&(ring).head))
#define ring_tail(ring)   (atomic_read(&(ring).tail))
#define ring_posted(ring) (ring_head(ring) - ring_tail(ring))

#define rx_ring_posted(ssk) ring_posted(ssk->rx_ring)
#define tx_ring_posted(ssk) (ring_posted(ssk->tx_ring) + \
	(ssk->tx_ring.rdma_inflight ? ssk->tx_ring.rdma_inflight->busy : 0))

#define posts_handler(ssk) atomic_read(&ssk->somebody_is_doing_posts)
#define posts_handler_get(ssk) atomic_inc(&ssk->somebody_is_doing_posts)
#define posts_handler_put(ssk) do {\
	atomic_dec(&ssk->somebody_is_doing_posts); \
	sdp_do_posts(ssk); \
} while (0)

extern int sdp_zcopy_thresh;
extern struct workqueue_struct *sdp_wq;
extern struct list_head sock_list;
extern spinlock_t sock_list_lock;
extern int rcvbuf_initial_size;
extern struct proto sdp_proto;
extern struct workqueue_struct *rx_comp_wq;
extern atomic_t sdp_current_mem_usage;
extern spinlock_t sdp_large_sockets_lock;
extern struct ib_client sdp_client;
#ifdef SDPSTATS_ON
DECLARE_PER_CPU(struct sdpstats, sdpstats);
#endif

enum sdp_mid {
	SDP_MID_HELLO = 0x0,
	SDP_MID_HELLO_ACK = 0x1,
	SDP_MID_DISCONN = 0x2,
	SDP_MID_ABORT = 0x3,
	SDP_MID_SENDSM = 0x4,
	SDP_MID_RDMARDCOMPL = 0x6,
	SDP_MID_SRCAVAIL_CANCEL = 0x8,
	SDP_MID_CHRCVBUF = 0xB,
	SDP_MID_CHRCVBUF_ACK = 0xC,
	SDP_MID_SINKAVAIL = 0xFD,
	SDP_MID_SRCAVAIL = 0xFE,
	SDP_MID_DATA = 0xFF,
};

enum sdp_flags {
        SDP_OOB_PRES = 1 << 0,
        SDP_OOB_PEND = 1 << 1,
};

enum {
	SDP_MIN_TX_CREDITS = 2
};

enum {
	SDP_ERR_ERROR   = -4,
	SDP_ERR_FAULT   = -3,
	SDP_NEW_SEG     = -2,
	SDP_DO_WAIT_MEM = -1
};

struct sdp_bsdh {
	u8 mid;
	u8 flags;
	__u16 bufs;
	__u32 len;
	__u32 mseq;
	__u32 mseq_ack;
} __attribute__((__packed__));

union cma_ip_addr {
	struct in6_addr ip6;
	struct {
		__u32 pad[3];
		__u32 addr;
	} ip4;
} __attribute__((__packed__));

/* TODO: too much? Can I avoid having the src/dst and port here? */
struct sdp_hh {
	struct sdp_bsdh bsdh;
	u8 majv_minv;
	u8 ipv_cap;
	u8 rsvd1;
	u8 max_adverts;
	__u32 desremrcvsz;
	__u32 localrcvsz;
	__u16 port;
	__u16 rsvd2;
	union cma_ip_addr src_addr;
	union cma_ip_addr dst_addr;
	u8 rsvd3[IB_CM_REQ_PRIVATE_DATA_SIZE - sizeof(struct sdp_bsdh) - 48];
} __attribute__((__packed__));

struct sdp_hah {
	struct sdp_bsdh bsdh;
	u8 majv_minv;
	u8 ipv_cap;
	u8 rsvd1;
	u8 ext_max_adverts;
	__u32 actrcvsz;
	u8 rsvd2[IB_CM_REP_PRIVATE_DATA_SIZE - sizeof(struct sdp_bsdh) - 8];
} __attribute__((__packed__));

struct sdp_rrch {
	__u32 len;
} __attribute__((__packed__));

struct sdp_srcah {
	__u32 len;
	__u32 rkey;
	__u64 vaddr;
} __attribute__((__packed__));

struct sdp_buf {
        struct sk_buff *skb;
        u64             mapping[SDP_MAX_SEND_SGES];
} __attribute__((__packed__));

struct sdp_chrecvbuf {
	u32 size;
} __attribute__((__packed__));

/* Context used for synchronous zero copy bcopy (BZCOPY) */
struct bzcopy_state {
	unsigned char __user  *u_base;
	int                    u_len;
	int                    left;
	int                    page_cnt;
	int                    cur_page;
	int                    cur_offset;
	int                    busy;
	struct sdp_sock      *ssk;
	struct page         **pages;
};

enum rx_sa_flag {
	RX_SA_ABORTED    = 2,
};

enum tx_sa_flag {
	TX_SA_SENDSM     = 0x01,
	TX_SA_CROSS_SEND = 0x02,
	TX_SA_INTRRUPTED = 0x04,
	TX_SA_TIMEDOUT   = 0x08,
	TX_SA_ERROR      = 0x10,
};

struct rx_srcavail_state {
	/* Advertised buffer stuff */
	u32 mseq;
	u32 used;
	u32 reported;
	u32 len;
	u32 rkey;
	u64 vaddr;

	/* Dest buff info */
	struct ib_umem *umem;
	struct ib_pool_fmr *fmr;

	/* Utility */
	u8  busy;
	enum rx_sa_flag  flags;
};

struct tx_srcavail_state {
	/* Data below 'busy' will be reset */
	u8		busy;

	struct ib_umem *umem;
	struct ib_pool_fmr *fmr;

	u32		bytes_sent;
	u32		bytes_acked;

	enum tx_sa_flag	abort_flags;
	u8		posted;

	u32		mseq;
};

struct sdp_tx_ring {
	struct rx_srcavail_state *rdma_inflight;
	struct sdp_buf   	*buffer;
	atomic_t          	head;
	atomic_t          	tail;
	struct ib_cq 	 	*cq;

	int 		  	una_seq;
	atomic_t 	  	credits;
#define tx_credits(ssk) (atomic_read(&ssk->tx_ring.credits))

	struct timer_list 	timer;
	struct tasklet_struct 	tasklet;
	u16 		  	poll_cnt;
};

struct sdp_rx_ring {
	struct sdp_buf   *buffer;
	atomic_t          head;
	atomic_t          tail;
	struct ib_cq 	 *cq;

	int		 destroyed;
	rwlock_t 	 destroyed_lock;

	struct tasklet_struct 	tasklet;
};

struct sdp_device {
	struct ib_pd 		*pd;
	struct ib_mr 		*mr;
	struct ib_fmr_pool 	*fmr_pool;
};

struct sdp_moderation {
	unsigned long last_moder_packets;
	unsigned long last_moder_tx_packets;
	unsigned long last_moder_bytes;
	unsigned long last_moder_jiffies;
	int last_moder_time;
	u16 rx_usecs;
	u16 rx_frames;
	u16 tx_usecs;
	u32 pkt_rate_low;
	u16 rx_usecs_low;
	u32 pkt_rate_high;
	u16 rx_usecs_high;
	u16 sample_interval;
	u16 adaptive_rx_coal;
	u32 msg_enable;

	int moder_cnt;
	int moder_time;
};

struct sdp_sock {
	/* sk has to be the first member of inet_sock */
	struct inet_sock isk;
	struct list_head sock_list;
	struct list_head accept_queue;
	struct list_head backlog_queue;
	struct sk_buff_head rx_ctl_q;
	struct sock *parent;
	struct sdp_device *sdp_dev;

	int qp_active;
	struct tx_srcavail_state *tx_sa;
	struct rx_srcavail_state *rx_sa;
	spinlock_t tx_sa_lock;
	struct delayed_work srcavail_cancel_work;
	int srcavail_cancel_mseq;

	struct ib_ucontext context;

	int max_sge;

	struct work_struct rx_comp_work;
	wait_queue_head_t wq;

	struct delayed_work dreq_wait_work;
	struct work_struct destroy_work;

	int tx_compl_pending;
	atomic_t somebody_is_doing_posts;

	/* Like tcp_sock */
	u16 urg_data;
	u32 urg_seq;
	u32 copied_seq;
#define rcv_nxt(ssk) atomic_read(&(ssk->rcv_nxt))
	atomic_t rcv_nxt;

	int write_seq;
	int pushed_seq;
	int xmit_size_goal;
	int nonagle;

	int dreq_wait_timeout;

	unsigned keepalive_time;

	spinlock_t lock;

	/* tx_head/rx_head when keepalive timer started */
	unsigned keepalive_tx_head;
	unsigned keepalive_rx_head;

	int destructed_already;
	int sdp_disconnect;
	int destruct_in_process;

	struct sdp_rx_ring rx_ring;
	struct sdp_tx_ring tx_ring;

	/* Data below will be reset on error */
	struct rdma_cm_id *id;
	struct ib_device *ib_device;

	/* SDP specific */
	atomic_t mseq_ack;
#define mseq_ack(ssk) (atomic_read(&ssk->mseq_ack))
	unsigned max_bufs;	/* Initial buffers offered by other side */
	unsigned min_bufs;	/* Low water mark to wake senders */

	unsigned long nagle_last_unacked; /* mseq of lastest unacked packet */
	struct timer_list nagle_timer; /* timeout waiting for ack */

	atomic_t               remote_credits;
#define remote_credits(ssk) (atomic_read(&ssk->remote_credits))
	int 		  poll_cq;

	/* rdma specific */
	struct ib_qp *qp;

	/* SDP slow start */
	int rcvbuf_scale; 	/* local recv buf scale for each socket */
	int sent_request_head; 	/* mark the tx_head of the last send resize
				   request */
	int sent_request; 	/* 0 - not sent yet, 1 - request pending
				   -1 - resize done succesfully */
	int recv_request_head; 	/* mark the rx_head when the resize request
				   was recieved */
	int recv_request; 	/* flag if request to resize was recieved */
	int recv_frags; 	/* max skb frags in recv packets */
	int send_frags; 	/* max skb frags in send packets */

	unsigned long tx_packets;
	unsigned long rx_packets;
	unsigned long tx_bytes;
	unsigned long rx_bytes;
	struct sdp_moderation auto_mod;

	/* ZCOPY data: -1:use global; 0:disable zcopy; >0: zcopy threshold */
	int zcopy_thresh;

	int last_bind_err;
};

static inline void tx_sa_reset(struct tx_srcavail_state *tx_sa)
{
	memset((void *)&tx_sa->busy, 0,
			sizeof(*tx_sa) - offsetof(typeof(*tx_sa), busy));
}

static inline void rx_ring_unlock(struct sdp_rx_ring *rx_ring)
{
	read_unlock_bh(&rx_ring->destroyed_lock);
}

static inline int rx_ring_trylock(struct sdp_rx_ring *rx_ring)
{
	read_lock_bh(&rx_ring->destroyed_lock);
	if (rx_ring->destroyed) {
		rx_ring_unlock(rx_ring);
		return 0;
	}
	return 1;
}

static inline void rx_ring_destroy_lock(struct sdp_rx_ring *rx_ring)
{
	write_lock_bh(&rx_ring->destroyed_lock);
	rx_ring->destroyed = 1;
	write_unlock_bh(&rx_ring->destroyed_lock);
}

static inline struct sdp_sock *sdp_sk(const struct sock *sk)
{
	        return (struct sdp_sock *)sk;
}

static inline int _sdp_exch_state(const char *func, int line, struct sock *sk,
				 int from_states, int state)
{
	unsigned long flags;
	int old;

	spin_lock_irqsave(&sdp_sk(sk)->lock, flags);

	sdp_dbg(sk, "%s:%d - set state: %s -> %s 0x%x\n", func, line,
		sdp_state_str(sk->sk_state),
		sdp_state_str(state), from_states);

	if ((1 << sk->sk_state) & ~from_states) {
		sdp_warn(sk, "trying to exchange state from unexpected state "
			"%s to state %s. expected states: 0x%x\n",
			sdp_state_str(sk->sk_state), sdp_state_str(state),
			from_states);
	}

	old = sk->sk_state;
	sk->sk_state = state;

	spin_unlock_irqrestore(&sdp_sk(sk)->lock, flags);

	return old;
}
#define sdp_exch_state(sk, from_states, state) \
	_sdp_exch_state(__func__, __LINE__, sk, from_states, state)

static inline void sdp_set_error(struct sock *sk, int err)
{
	int ib_teardown_states = TCPF_FIN_WAIT1 | TCPF_CLOSE_WAIT
		| TCPF_LAST_ACK;
	sk->sk_err = -err;
	if (sk->sk_socket)
		sk->sk_socket->state = SS_DISCONNECTING;

	if ((1 << sk->sk_state) & ib_teardown_states)
		sdp_exch_state(sk, ib_teardown_states, TCP_TIME_WAIT);
	else
		sdp_exch_state(sk, ~0, TCP_CLOSE);

	sk->sk_error_report(sk);
}

static inline void sdp_arm_rx_cq(struct sock *sk)
{
	sdp_prf(sk, NULL, "Arming RX cq");
	sdp_dbg_data(sk, "Arming RX cq\n");

	ib_req_notify_cq(sdp_sk(sk)->rx_ring.cq, IB_CQ_NEXT_COMP);
}

static inline void sdp_arm_tx_cq(struct sock *sk)
{
	sdp_prf(sk, NULL, "Arming TX cq");
	sdp_dbg_data(sk, "Arming TX cq. credits: %d, posted: %d\n",
		tx_credits(sdp_sk(sk)), tx_ring_posted(sdp_sk(sk)));

	ib_req_notify_cq(sdp_sk(sk)->tx_ring.cq, IB_CQ_NEXT_COMP);
}

/* return the min of:
 * - tx credits
 * - free slots in tx_ring (not including SDP_MIN_TX_CREDITS
 */
static inline int tx_slots_free(struct sdp_sock *ssk)
{
	int min_free;

	min_free = MIN(tx_credits(ssk),
			SDP_TX_SIZE - tx_ring_posted(ssk));
	if (min_free < SDP_MIN_TX_CREDITS)
		return 0;

	return min_free - SDP_MIN_TX_CREDITS;
};

/* utilities */
static inline char *mid2str(int mid)
{
#define ENUM2STR(e) [e] = #e
	static char *mid2str[] = {
		ENUM2STR(SDP_MID_HELLO),
		ENUM2STR(SDP_MID_HELLO_ACK),
		ENUM2STR(SDP_MID_ABORT),
		ENUM2STR(SDP_MID_DISCONN),
		ENUM2STR(SDP_MID_SENDSM),
		ENUM2STR(SDP_MID_RDMARDCOMPL),
		ENUM2STR(SDP_MID_SRCAVAIL_CANCEL),
		ENUM2STR(SDP_MID_CHRCVBUF),
		ENUM2STR(SDP_MID_CHRCVBUF_ACK),
		ENUM2STR(SDP_MID_DATA),
		ENUM2STR(SDP_MID_SRCAVAIL),
		ENUM2STR(SDP_MID_SINKAVAIL),
	};

	if (mid >= ARRAY_SIZE(mid2str))
		return NULL;

	return mid2str[mid];
}

static inline struct sk_buff *sdp_stream_alloc_skb(struct sock *sk, int size,
		gfp_t gfp)
{
	struct sk_buff *skb;

	/* The TCP header must be at least 32-bit aligned.  */
	size = ALIGN(size, 4);

	skb = alloc_skb_fclone(size + sk->sk_prot->max_header, gfp);
	if (skb) {
		if (sk_wmem_schedule(sk, skb->truesize)) {
			/*
			 * Make sure that we have exactly size bytes
			 * available to the caller, no more, no less.
			 */
			skb_reserve(skb, skb_tailroom(skb) - size);
			return skb;
		}
		__kfree_skb(skb);
	} else {
		sk->sk_prot->enter_memory_pressure(sk);
		sk_stream_moderate_sndbuf(sk);
	}
	return NULL;
}

static inline struct sk_buff *sdp_alloc_skb(struct sock *sk, u8 mid, int size,
		gfp_t gfp)
{
	struct sdp_bsdh *h;
	struct sk_buff *skb;

	if (!gfp) {
		if (unlikely(sk->sk_allocation))
			gfp = sk->sk_allocation;
		else
			gfp = GFP_KERNEL;
	}

	skb = sdp_stream_alloc_skb(sk, sizeof(struct sdp_bsdh) + size, gfp);
	BUG_ON(!skb);

        skb_header_release(skb);

	h = (struct sdp_bsdh *)skb_push(skb, sizeof *h);
	h->mid = mid;

	skb_reset_transport_header(skb);

	return skb;
}
static inline struct sk_buff *sdp_alloc_skb_data(struct sock *sk, gfp_t gfp)
{
	return sdp_alloc_skb(sk, SDP_MID_DATA, 0, gfp);
}

static inline struct sk_buff *sdp_alloc_skb_disconnect(struct sock *sk,
		gfp_t gfp)
{
	return sdp_alloc_skb(sk, SDP_MID_DISCONN, 0, gfp);
}

static inline struct sk_buff *sdp_alloc_skb_chrcvbuf_ack(struct sock *sk,
		int size, gfp_t gfp)
{
	struct sk_buff *skb;
	struct sdp_chrecvbuf *resp_size;

	skb = sdp_alloc_skb(sk, SDP_MID_CHRCVBUF_ACK, sizeof(*resp_size), gfp);

	resp_size = (struct sdp_chrecvbuf *)skb_put(skb, sizeof *resp_size);
	resp_size->size = htonl(size);

	return skb;
}

static inline struct sk_buff *sdp_alloc_skb_srcavail(struct sock *sk,
	u32 len, u32 rkey, u64 vaddr, gfp_t gfp)
{
	struct sk_buff *skb;
	struct sdp_srcah *srcah;

	skb = sdp_alloc_skb(sk, SDP_MID_SRCAVAIL, sizeof(*srcah), gfp);

	srcah = (struct sdp_srcah *)skb_put(skb, sizeof(*srcah));
	srcah->len = htonl(len);
	srcah->rkey = htonl(rkey);
	srcah->vaddr = cpu_to_be64(vaddr);

	return skb;
}

static inline struct sk_buff *sdp_alloc_skb_srcavail_cancel(struct sock *sk,
		gfp_t gfp)
{
	return sdp_alloc_skb(sk, SDP_MID_SRCAVAIL_CANCEL, 0, gfp);
}

static inline struct sk_buff *sdp_alloc_skb_rdmardcompl(struct sock *sk,
	u32 len, gfp_t gfp)
{
	struct sk_buff *skb;
	struct sdp_rrch *rrch;

	skb = sdp_alloc_skb(sk, SDP_MID_RDMARDCOMPL, sizeof(*rrch), gfp);

	rrch = (struct sdp_rrch *)skb_put(skb, sizeof(*rrch));
	rrch->len = htonl(len);

	return skb;
}

static inline struct sk_buff *sdp_alloc_skb_sendsm(struct sock *sk, gfp_t gfp)
{
	return sdp_alloc_skb(sk, SDP_MID_SENDSM, 0, gfp);
}
static inline int sdp_tx_ring_slots_left(struct sdp_sock *ssk)
{
	return SDP_TX_SIZE - tx_ring_posted(ssk);
}

static inline int credit_update_needed(struct sdp_sock *ssk)
{
	int c;

	c = remote_credits(ssk);
	if (likely(c > SDP_MIN_TX_CREDITS))
		c += c/2;
	return unlikely(c < rx_ring_posted(ssk)) &&
	    likely(tx_credits(ssk) > 0) &&
	    likely(sdp_tx_ring_slots_left(ssk));
}


#ifdef SDPSTATS_ON

#define SDPSTATS_MAX_HIST_SIZE 256
struct sdpstats {
	u32 post_send[256];
	u32 sendmsg_bcopy_segment;
	u32 sendmsg_bzcopy_segment;
	u32 sendmsg_zcopy_segment;
	u32 sendmsg;
	u32 post_send_credits;
	u32 sendmsg_nagle_skip;
	u32 sendmsg_seglen[25];
	u32 send_size[25];
	u32 post_recv;
	u32 rx_int_count;
	u32 tx_int_count;
	u32 bzcopy_poll_miss;
	u32 send_wait_for_mem;
	u32 send_miss_no_credits;
	u32 rx_poll_miss;
	u32 tx_poll_miss;
	u32 tx_poll_hit;
	u32 tx_poll_busy;
	u32 memcpy_count;
	u32 credits_before_update[64];
	u32 zcopy_tx_timeout;
	u32 zcopy_cross_send;
	u32 zcopy_tx_aborted;
	u32 zcopy_tx_error;
};

static inline void sdpstats_hist(u32 *h, u32 val, u32 maxidx, int is_log)
{
	int idx = is_log ? ilog2(val) : val;
	if (idx > maxidx)
		idx = maxidx;

	h[idx]++;
}

#define SDPSTATS_COUNTER_INC(stat) do { __get_cpu_var(sdpstats).stat++; } while (0)
#define SDPSTATS_COUNTER_ADD(stat, val) do { __get_cpu_var(sdpstats).stat += val; } while (0)
#define SDPSTATS_COUNTER_MID_INC(stat, mid) do { __get_cpu_var(sdpstats).stat[mid]++; } \
	while (0)
#define SDPSTATS_HIST(stat, size) \
	sdpstats_hist(__get_cpu_var(sdpstats).stat, size, ARRAY_SIZE(__get_cpu_var(sdpstats).stat) - 1, 1)

#define SDPSTATS_HIST_LINEAR(stat, size) \
	sdpstats_hist(__get_cpu_var(sdpstats).stat, size, ARRAY_SIZE(__get_cpu_var(sdpstats).stat) - 1, 0)

#else
#define SDPSTATS_COUNTER_INC(stat)
#define SDPSTATS_COUNTER_ADD(stat, val)
#define SDPSTATS_COUNTER_MID_INC(stat, mid)
#define SDPSTATS_HIST_LINEAR(stat, size)
#define SDPSTATS_HIST(stat, size)
#endif

static inline void sdp_cleanup_sdp_buf(struct sdp_sock *ssk, struct sdp_buf *sbuf,
		size_t head_size, enum dma_data_direction dir)
{
	int i;
	struct sk_buff *skb;
	struct ib_device *dev = ssk->ib_device;

	skb = sbuf->skb;

	ib_dma_unmap_single(dev, sbuf->mapping[0], head_size, dir);

	for (i = 0; i < skb_shinfo(skb)->nr_frags; i++) {
		ib_dma_unmap_page(dev, sbuf->mapping[i + 1],
				  skb_shinfo(skb)->frags[i].size,
				  dir);
	}
}

/* sdp_main.c */
void sdp_set_default_moderation(struct sdp_sock *ssk);
int sdp_init_sock(struct sock *sk);
void sdp_start_keepalive_timer(struct sock *sk);
void sdp_remove_sock(struct sdp_sock *ssk);
void sdp_add_sock(struct sdp_sock *ssk);
void sdp_urg(struct sdp_sock *ssk, struct sk_buff *skb);
void sdp_cancel_dreq_wait_timeout(struct sdp_sock *ssk);
void sdp_reset_sk(struct sock *sk, int rc);
void sdp_reset(struct sock *sk);
int sdp_tx_wait_memory(struct sdp_sock *ssk, long *timeo_p, int *credits_needed);
void skb_entail(struct sock *sk, struct sdp_sock *ssk, struct sk_buff *skb);

/* sdp_proc.c */
int __init sdp_proc_init(void);
void sdp_proc_unregister(void);

/* sdp_cma.c */
int sdp_cma_handler(struct rdma_cm_id *, struct rdma_cm_event *);

/* sdp_tx.c */
int sdp_tx_ring_create(struct sdp_sock *ssk, struct ib_device *device);
void sdp_tx_ring_destroy(struct sdp_sock *ssk);
int sdp_xmit_poll(struct sdp_sock *ssk, int force);
void sdp_post_send(struct sdp_sock *ssk, struct sk_buff *skb);
void sdp_post_sends(struct sdp_sock *ssk, gfp_t gfp);
void sdp_nagle_timeout(unsigned long data);
void sdp_post_keepalive(struct sdp_sock *ssk);

/* sdp_rx.c */
void sdp_rx_ring_init(struct sdp_sock *ssk);
int sdp_rx_ring_create(struct sdp_sock *ssk, struct ib_device *device);
void sdp_rx_ring_destroy(struct sdp_sock *ssk);
int sdp_resize_buffers(struct sdp_sock *ssk, u32 new_size);
int sdp_init_buffers(struct sdp_sock *ssk, u32 new_size);
void sdp_do_posts(struct sdp_sock *ssk);
void sdp_rx_comp_full(struct sdp_sock *ssk);
void sdp_remove_large_sock(struct sdp_sock *ssk);
void sdp_handle_disconn(struct sock *sk);

/* sdp_zcopy.c */
int sdp_sendmsg_zcopy(struct kiocb *iocb, struct sock *sk, struct iovec *iov);
int sdp_handle_srcavail(struct sdp_sock *ssk, struct sdp_srcah *srcah);
void sdp_handle_sendsm(struct sdp_sock *ssk, u32 mseq_ack);
void sdp_handle_rdma_read_compl(struct sdp_sock *ssk, u32 mseq_ack,
		u32 bytes_completed);
int sdp_handle_rdma_read_cqe(struct sdp_sock *ssk);
int sdp_rdma_to_iovec(struct sock *sk, struct iovec *iov, struct sk_buff *skb,
		unsigned long *used);
int sdp_post_rdma_rd_compl(struct sdp_sock *ssk,
		struct rx_srcavail_state *rx_sa);
int sdp_post_sendsm(struct sock *sk);
void srcavail_cancel_timeout(struct work_struct *work);
void sdp_abort_srcavail(struct sock *sk);
void sdp_abort_rdma_read(struct sock *sk);

#endif
