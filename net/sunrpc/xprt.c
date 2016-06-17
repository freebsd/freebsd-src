/*
 *  linux/net/sunrpc/xprt.c
 *
 *  This is a generic RPC call interface supporting congestion avoidance,
 *  and asynchronous calls.
 *
 *  The interface works like this:
 *
 *  -	When a process places a call, it allocates a request slot if
 *	one is available. Otherwise, it sleeps on the backlog queue
 *	(xprt_reserve).
 *  -	Next, the caller puts together the RPC message, stuffs it into
 *	the request struct, and calls xprt_call().
 *  -	xprt_call transmits the message and installs the caller on the
 *	socket's wait list. At the same time, it installs a timer that
 *	is run after the packet's timeout has expired.
 *  -	When a packet arrives, the data_ready handler walks the list of
 *	pending requests for that socket. If a matching XID is found, the
 *	caller is woken up, and the timer removed.
 *  -	When no reply arrives within the timeout interval, the timer is
 *	fired by the kernel and runs xprt_timer(). It either adjusts the
 *	timeout values (minor timeout) or wakes up the caller with a status
 *	of -ETIMEDOUT.
 *  -	When the caller receives a notification from RPC that a reply arrived,
 *	it should release the RPC slot, and process the reply.
 *	If the call timed out, it may choose to retry the operation by
 *	adjusting the initial timeout value, and simply calling rpc_call
 *	again.
 *
 *  Support for async RPC is done through a set of RPC-specific scheduling
 *  primitives that `transparently' work for processes as well as async
 *  tasks that rely on callbacks.
 *
 *  Copyright (C) 1995-1997, Olaf Kirch <okir@monad.swb.de>
 *
 *  TCP callback races fixes (C) 1998 Red Hat Software <alan@redhat.com>
 *  TCP send fixes (C) 1998 Red Hat Software <alan@redhat.com>
 *  TCP NFS related read + write fixes
 *   (C) 1999 Dave Airlie, University of Limerick, Ireland <airlied@linux.ie>
 *
 *  Rewrite of larges part of the code in order to stabilize TCP stuff.
 *  Fix behaviour when socket buffer is full.
 *   (C) 1999 Trond Myklebust <trond.myklebust@fys.uio.no>
 */

#define __KERNEL_SYSCALLS__

#include <linux/version.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/capability.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/socket.h>
#include <linux/in.h>
#include <linux/net.h>
#include <linux/mm.h>
#include <linux/udp.h>
#include <linux/unistd.h>
#include <linux/sunrpc/clnt.h>
#include <linux/file.h>

#include <net/sock.h>
#include <net/checksum.h>
#include <net/udp.h>
#include <net/tcp.h>

#include <asm/uaccess.h>

/*
 * Local variables
 */

#ifdef RPC_DEBUG
# undef  RPC_DEBUG_DATA
# define RPCDBG_FACILITY	RPCDBG_XPRT
#endif

#define XPRT_MAX_BACKOFF	(8)

/*
 * Local functions
 */
static void	xprt_request_init(struct rpc_task *, struct rpc_xprt *);
static void	do_xprt_transmit(struct rpc_task *);
static inline void	do_xprt_reserve(struct rpc_task *);
static void	xprt_disconnect(struct rpc_xprt *);
static void	xprt_connect_status(struct rpc_task *task);
static struct socket *xprt_create_socket(int, struct rpc_timeout *, int);
static int	xprt_bind_socket(struct rpc_xprt *, struct socket *);
static int      __xprt_get_cong(struct rpc_xprt *, struct rpc_task *);

#ifdef RPC_DEBUG_DATA
/*
 * Print the buffer contents (first 128 bytes only--just enough for
 * diropres return).
 */
static void
xprt_pktdump(char *msg, u32 *packet, unsigned int count)
{
	u8	*buf = (u8 *) packet;
	int	j;

	dprintk("RPC:      %s\n", msg);
	for (j = 0; j < count && j < 128; j += 4) {
		if (!(j & 31)) {
			if (j)
				dprintk("\n");
			dprintk("0x%04x ", j);
		}
		dprintk("%02x%02x%02x%02x ",
			buf[j], buf[j+1], buf[j+2], buf[j+3]);
	}
	dprintk("\n");
}
#else
static inline void
xprt_pktdump(char *msg, u32 *packet, unsigned int count)
{
	/* NOP */
}
#endif

/*
 * Look up RPC transport given an INET socket
 */
static inline struct rpc_xprt *
xprt_from_sock(struct sock *sk)
{
	return (struct rpc_xprt *) sk->user_data;
}

/*
 * Serialize write access to sockets, in order to prevent different
 * requests from interfering with each other.
 * Also prevents TCP socket connections from colliding with writes.
 */
static int
__xprt_lock_write(struct rpc_xprt *xprt, struct rpc_task *task)
{
	struct rpc_rqst *req = task->tk_rqstp;
	if (!xprt->snd_task) {
		if (xprt->nocong || __xprt_get_cong(xprt, task)) {
			xprt->snd_task = task;
			if (req) {
				req->rq_bytes_sent = 0;
				req->rq_ntrans++;
			}
		}
	}
	if (xprt->snd_task != task) {
		dprintk("RPC: %4d TCP write queue full\n", task->tk_pid);
		task->tk_timeout = 0;
		task->tk_status = -EAGAIN;
		if (req && req->rq_ntrans)
			rpc_sleep_on(&xprt->resend, task, NULL, NULL);
		else
			rpc_sleep_on(&xprt->sending, task, NULL, NULL);
	}
	return xprt->snd_task == task;
}

static inline int
xprt_lock_write(struct rpc_xprt *xprt, struct rpc_task *task)
{
	int retval;
	spin_lock_bh(&xprt->sock_lock);
	retval = __xprt_lock_write(xprt, task);
	spin_unlock_bh(&xprt->sock_lock);
	return retval;
}

static void
__xprt_lock_write_next(struct rpc_xprt *xprt)
{
	struct rpc_task *task;

	if (xprt->snd_task)
		return;
	task = rpc_wake_up_next(&xprt->resend);
	if (!task) {
		if (!xprt->nocong && RPCXPRT_CONGESTED(xprt))
			return;
		task = rpc_wake_up_next(&xprt->sending);
		if (!task)
			return;
	}
	if (xprt->nocong || __xprt_get_cong(xprt, task)) {
		struct rpc_rqst *req = task->tk_rqstp;
		xprt->snd_task = task;
		if (req) {
			req->rq_bytes_sent = 0;
			req->rq_ntrans++;
		}
	}
}

/*
 * Releases the socket for use by other requests.
 */
static void
__xprt_release_write(struct rpc_xprt *xprt, struct rpc_task *task)
{
	if (xprt->snd_task == task)
		xprt->snd_task = NULL;
	__xprt_lock_write_next(xprt);
}

static inline void
xprt_release_write(struct rpc_xprt *xprt, struct rpc_task *task)
{
	spin_lock_bh(&xprt->sock_lock);
	__xprt_release_write(xprt, task);
	spin_unlock_bh(&xprt->sock_lock);
}

/*
 * Write data to socket.
 */
static inline int
xprt_sendmsg(struct rpc_xprt *xprt, struct rpc_rqst *req)
{
	struct socket	*sock = xprt->sock;
	struct msghdr	msg;
	struct xdr_buf	*xdr = &req->rq_snd_buf;
	struct iovec	niv[MAX_IOVEC];
	unsigned int	niov, slen, skip;
	mm_segment_t	oldfs;
	int		result;

	if (!sock)
		return -ENOTCONN;

	xprt_pktdump("packet data:",
				req->rq_svec->iov_base,
				req->rq_svec->iov_len);

	/* Dont repeat bytes */
	skip = req->rq_bytes_sent;
	slen = xdr->len - skip;
	oldfs = get_fs(); set_fs(get_ds());
	do {
		unsigned int slen_part, n;

		niov = xdr_kmap(niv, xdr, skip);
		if (!niov) {
			result = -EAGAIN;
			break;
		}

		msg.msg_flags   = MSG_DONTWAIT|MSG_NOSIGNAL;
		msg.msg_iov	= niv;
		msg.msg_iovlen	= niov;
		msg.msg_name	= (struct sockaddr *) &xprt->addr;
		msg.msg_namelen = sizeof(xprt->addr);
		msg.msg_control = NULL;
		msg.msg_controllen = 0;

		slen_part = 0;
		for (n = 0; n < niov; n++)
			slen_part += niv[n].iov_len;

		clear_bit(SOCK_ASYNC_NOSPACE, &sock->flags);
		result = sock_sendmsg(sock, &msg, slen_part);

		xdr_kunmap(xdr, skip, niov);

		skip += slen_part;
		slen -= slen_part;
	} while (result >= 0 && slen);
	set_fs(oldfs);

	dprintk("RPC:      xprt_sendmsg(%d) = %d\n", slen, result);

	if (result >= 0)
		return result;

	switch (result) {
	case -ECONNREFUSED:
		/* When the server has died, an ICMP port unreachable message
		 * prompts ECONNREFUSED.
		 */
	case -EAGAIN:
		break;
	case -ECONNRESET:
	case -ENOTCONN:
	case -EPIPE:
		/* connection broken */
		if (xprt->stream)
			result = -ENOTCONN;
		break;
	default:
		printk(KERN_NOTICE "RPC: sendmsg returned error %d\n", -result);
	}
	return result;
}

/*
 * Van Jacobson congestion avoidance. Check if the congestion window
 * overflowed. Put the task to sleep if this is the case.
 */
static int
__xprt_get_cong(struct rpc_xprt *xprt, struct rpc_task *task)
{
	struct rpc_rqst *req = task->tk_rqstp;

	if (req->rq_cong)
		return 1;
	dprintk("RPC: %4d xprt_cwnd_limited cong = %ld cwnd = %ld\n",
			task->tk_pid, xprt->cong, xprt->cwnd);
	if (RPCXPRT_CONGESTED(xprt))
		return 0;
	req->rq_cong = 1;
	xprt->cong += RPC_CWNDSCALE;
	return 1;
}

/*
 * Adjust the congestion window, and wake up the next task
 * that has been sleeping due to congestion
 */
static void
__xprt_put_cong(struct rpc_xprt *xprt, struct rpc_rqst *req)
{
	if (!req->rq_cong)
		return;
	req->rq_cong = 0;
	xprt->cong -= RPC_CWNDSCALE;
	__xprt_lock_write_next(xprt);
}

/*
 * Adjust RPC congestion window
 * We use a time-smoothed congestion estimator to avoid heavy oscillation.
 */
static void
xprt_adjust_cwnd(struct rpc_xprt *xprt, int result)
{
	unsigned long	cwnd;

	cwnd = xprt->cwnd;
	if (result >= 0 && cwnd <= xprt->cong) {
		/* The (cwnd >> 1) term makes sure
		 * the result gets rounded properly. */
		cwnd += (RPC_CWNDSCALE * RPC_CWNDSCALE + (cwnd >> 1)) / cwnd;
		if (cwnd > RPC_MAXCWND)
			cwnd = RPC_MAXCWND;
		__xprt_lock_write_next(xprt);
	} else if (result == -ETIMEDOUT) {
		cwnd >>= 1;
		if (cwnd < RPC_CWNDSCALE)
			cwnd = RPC_CWNDSCALE;
	}
	dprintk("RPC:      cong %ld, cwnd was %ld, now %ld\n",
			xprt->cong, xprt->cwnd, cwnd);
	xprt->cwnd = cwnd;
}

/*
 * Adjust timeout values etc for next retransmit
 */
int
xprt_adjust_timeout(struct rpc_timeout *to)
{
	if (to->to_retries > 0) {
		if (to->to_exponential)
			to->to_current <<= 1;
		else
			to->to_current += to->to_increment;
		if (to->to_maxval && to->to_current >= to->to_maxval)
			to->to_current = to->to_maxval;
	} else {
		if (to->to_exponential)
			to->to_initval <<= 1;
		else
			to->to_initval += to->to_increment;
		if (to->to_maxval && to->to_initval >= to->to_maxval)
			to->to_initval = to->to_maxval;
		to->to_current = to->to_initval;
	}

	if (!to->to_current) {
		printk(KERN_WARNING "xprt_adjust_timeout: to_current = 0!\n");
		to->to_current = 5 * HZ;
	}
	pprintk("RPC: %lu %s\n", jiffies,
			to->to_retries? "retrans" : "timeout");
	return to->to_retries-- > 0;
}

/*
 * Close down a transport socket
 */
static void
xprt_close(struct rpc_xprt *xprt)
{
	struct socket	*sock = xprt->sock;
	struct sock	*sk = xprt->inet;

	if (!sk)
		return;

	write_lock_bh(&sk->callback_lock);
	xprt->inet = NULL;
	xprt->sock = NULL;

	sk->user_data    = NULL;
	sk->data_ready   = xprt->old_data_ready;
	sk->state_change = xprt->old_state_change;
	sk->write_space  = xprt->old_write_space;
	write_unlock_bh(&sk->callback_lock);

	xprt_disconnect(xprt);
	sk->no_check	 = 0;

	sock_release(sock);
}

/*
 * Mark a transport as disconnected
 */
static void
xprt_disconnect(struct rpc_xprt *xprt)
{
	dprintk("RPC:      disconnected transport %p\n", xprt);
	spin_lock_bh(&xprt->sock_lock);
	xprt_clear_connected(xprt);
	rpc_wake_up_status(&xprt->pending, -ENOTCONN);
	spin_unlock_bh(&xprt->sock_lock);
}

/*
 * Reconnect a broken TCP connection.
 *
 */
void
xprt_connect(struct rpc_task *task)
{
	struct rpc_xprt	*xprt = task->tk_xprt;
	struct socket	*sock = xprt->sock;
	struct sock	*inet;
	int		status;

	dprintk("RPC: %4d xprt_connect %p connected %d\n",
				task->tk_pid, xprt, xprt_connected(xprt));
	if (xprt->shutdown)
		return;

	if (!xprt->addr.sin_port) {
		task->tk_status = -EIO;
		return;
	}

	if (!xprt_lock_write(xprt, task))
		return;
	if (xprt_connected(xprt))
		goto out_write;

	if (task->tk_rqstp)
		task->tk_rqstp->rq_bytes_sent = 0;

	xprt_close(xprt);
	/* Create an unconnected socket */
	sock = xprt_create_socket(xprt->prot, &xprt->timeout, xprt->resvport);
	if (!sock) {
		/* couldn't create socket or bind to reserved port;
		 * this is likely a permanent error, so cause an abort */
		task->tk_status = -EIO;
		goto out_write;
	}
	xprt_bind_socket(xprt, sock);

	if (!xprt->stream)
		goto out_write;

	inet = sock->sk;

	/* Now connect it asynchronously. */
	dprintk("RPC: %4d connecting new socket\n", task->tk_pid);
	status = sock->ops->connect(sock, (struct sockaddr *) &xprt->addr,
				sizeof(xprt->addr), O_NONBLOCK);
	dprintk("RPC: %4d connect status %d connected %d\n",
		task->tk_pid, status, xprt_connected(xprt));

	if (status >= 0)
		return;

	switch (status) {
	case -EALREADY:
	case -EINPROGRESS:
		/* Protect against TCP socket state changes */
		lock_sock(inet);
		if (inet->state != TCP_ESTABLISHED) {
			dprintk("RPC: %4d  waiting for connection\n",
					task->tk_pid);
			task->tk_timeout = RPC_CONNECT_TIMEOUT;
			/* if the socket is already closing, delay briefly */
			if ((1<<inet->state) & ~(TCPF_SYN_SENT|TCPF_SYN_RECV))
				task->tk_timeout = RPC_REESTABLISH_TIMEOUT;
			rpc_sleep_on(&xprt->pending, task, xprt_connect_status,
					NULL);
		}
		release_sock(inet);
		break;
	case -ECONNREFUSED:
	case -ECONNRESET:
	case -ENOTCONN:
		if (!task->tk_client->cl_softrtry) {
			rpc_delay(task, RPC_REESTABLISH_TIMEOUT);
			task->tk_status = -ENOTCONN;
			break;
		}
	default:
		/* Report myriad other possible returns.  If this file
		 * system is soft mounted, just error out, like Solaris.  */
		if (task->tk_client->cl_softrtry) {
			printk(KERN_WARNING
					"RPC: error %d connecting to server %s, exiting\n",
					-status, task->tk_client->cl_server);
			task->tk_status = -EIO;
			goto out_write;
		}
		printk(KERN_WARNING "RPC: error %d connecting to server %s\n",
				-status, task->tk_client->cl_server);
		/* This will prevent anybody else from connecting */
		rpc_delay(task, RPC_REESTABLISH_TIMEOUT);
		task->tk_status = status;
		break;
	}
	return;
 out_write:
	xprt_release_write(xprt, task);
}

/*
 * We arrive here when awoken from waiting on connection establishment.
 */
static void
xprt_connect_status(struct rpc_task *task)
{
	struct rpc_xprt	*xprt = task->tk_xprt;

	if (task->tk_status >= 0) {
		dprintk("RPC: %4d xprt_connect_status: connection established\n",
				task->tk_pid);
		return;
	}

	/* if soft mounted, cause this RPC to fail */
	if (task->tk_client->cl_softrtry)
		task->tk_status = -EIO;

	switch (task->tk_status) {
	case -ENOTCONN:
		rpc_delay(task, RPC_REESTABLISH_TIMEOUT);
		return;
	case -ETIMEDOUT:
		dprintk("RPC: %4d xprt_connect_status: timed out\n",
				task->tk_pid);
		break;
	default:
		printk(KERN_ERR "RPC: error %d connecting to server %s\n",
				-task->tk_status, task->tk_client->cl_server);
	}
	xprt_release_write(xprt, task);
}

/*
 * Look up the RPC request corresponding to a reply, and then lock it.
 */
static inline struct rpc_rqst *
xprt_lookup_rqst(struct rpc_xprt *xprt, u32 xid)
{
	struct list_head *pos;
	struct rpc_rqst	*req = NULL;

	list_for_each(pos, &xprt->recv) {
		struct rpc_rqst *entry = list_entry(pos, struct rpc_rqst, rq_list);
		if (entry->rq_xid == xid) {
			req = entry;
			break;
		}
	}
	return req;
}

/*
 * Complete reply received.
 * The TCP code relies on us to remove the request from xprt->pending.
 */
static void
xprt_complete_rqst(struct rpc_xprt *xprt, struct rpc_rqst *req, int copied)
{
	struct rpc_task	*task = req->rq_task;
	struct rpc_clnt *clnt = task->tk_client;

	/* Adjust congestion window */
	if (!xprt->nocong) {
		int timer = rpcproc_timer(clnt, task->tk_msg.rpc_proc);
		xprt_adjust_cwnd(xprt, copied);
		__xprt_put_cong(xprt, req);
	       	if (req->rq_ntrans == 1) {
			if (timer)
				rpc_update_rtt(&clnt->cl_rtt, timer, (long)jiffies - req->rq_xtime);
		}
		rpc_set_timeo(&clnt->cl_rtt, timer, req->rq_ntrans - 1);
	}

#ifdef RPC_PROFILE
	/* Profile only reads for now */
	if (copied > 1024) {
		static unsigned long	nextstat = 0;
		static unsigned long	pkt_rtt = 0, pkt_len = 0, pkt_cnt = 0;

		pkt_cnt++;
		pkt_len += req->rq_slen + copied;
		pkt_rtt += jiffies - req->rq_xtime;
		if (time_before(nextstat, jiffies)) {
			printk("RPC: %lu %ld cwnd\n", jiffies, xprt->cwnd);
			printk("RPC: %ld %ld %ld %ld stat\n",
					jiffies, pkt_cnt, pkt_len, pkt_rtt);
			pkt_rtt = pkt_len = pkt_cnt = 0;
			nextstat = jiffies + 5 * HZ;
		}
	}
#endif

	dprintk("RPC: %4d has input (%d bytes)\n", task->tk_pid, copied);
	req->rq_received = copied;
	list_del_init(&req->rq_list);

	/* ... and wake up the process. */
	rpc_wake_up_task(task);
	return;
}

static size_t
skb_read_bits(skb_reader_t *desc, void *to, size_t len)
{
	if (len > desc->count)
		len = desc->count;
	skb_copy_bits(desc->skb, desc->offset, to, len);
	desc->count -= len;
	desc->offset += len;
	return len;
}

static size_t
skb_read_and_csum_bits(skb_reader_t *desc, void *to, size_t len)
{
	unsigned int csum2, pos;

	if (len > desc->count)
		len = desc->count;
	pos = desc->offset;
	csum2 = skb_copy_and_csum_bits(desc->skb, pos, to, len, 0);
	desc->csum = csum_block_add(desc->csum, csum2, pos);
	desc->count -= len;
	desc->offset += len;
	return len;
}

/*
 * We have set things up such that we perform the checksum of the UDP
 * packet in parallel with the copies into the RPC client iovec.  -DaveM
 */
static int
csum_partial_copy_to_xdr(struct xdr_buf *xdr, struct sk_buff *skb)
{
	skb_reader_t desc;

	desc.skb = skb;
	desc.offset = sizeof(struct udphdr);
	desc.count = skb->len - desc.offset;

	if (skb->ip_summed == CHECKSUM_UNNECESSARY)
		goto no_checksum;

	desc.csum = csum_partial(skb->data, desc.offset, skb->csum);
	xdr_partial_copy_from_skb(xdr, 0, &desc, skb_read_and_csum_bits);
	if (desc.offset != skb->len) {
		unsigned int csum2;
		csum2 = skb_checksum(skb, desc.offset, skb->len - desc.offset, 0);
		desc.csum = csum_block_add(desc.csum, csum2, desc.offset);
	}
	if ((unsigned short)csum_fold(desc.csum))
		return -1;
	return 0;
no_checksum:
	xdr_partial_copy_from_skb(xdr, 0, &desc, skb_read_bits);
	return 0;
}

/*
 * Input handler for RPC replies. Called from a bottom half and hence
 * atomic.
 */
static void
udp_data_ready(struct sock *sk, int len)
{
	struct rpc_task	*task;
	struct rpc_xprt	*xprt;
	struct rpc_rqst *rovr;
	struct sk_buff	*skb;
	int		err, repsize, copied;

	read_lock(&sk->callback_lock);
	dprintk("RPC:      udp_data_ready...\n");
	if (sk->dead || !(xprt = xprt_from_sock(sk))) {
		printk("RPC:      udp_data_ready request not found!\n");
		goto out;
	}

	dprintk("RPC:      udp_data_ready client %p\n", xprt);

	if ((skb = skb_recv_datagram(sk, 0, 1, &err)) == NULL)
		goto out;

	if (xprt->shutdown)
		goto dropit;

	repsize = skb->len - sizeof(struct udphdr);
	if (repsize < 4) {
		printk("RPC: impossible RPC reply size %d!\n", repsize);
		goto dropit;
	}

	/* Look up and lock the request corresponding to the given XID */
	spin_lock(&xprt->sock_lock);
	rovr = xprt_lookup_rqst(xprt, *(u32 *) (skb->h.raw + sizeof(struct udphdr)));
	if (!rovr)
		goto out_unlock;
	task = rovr->rq_task;

	dprintk("RPC: %4d received reply\n", task->tk_pid);
	xprt_pktdump("packet data:",
		     (u32 *) (skb->h.raw+sizeof(struct udphdr)), repsize);

	if ((copied = rovr->rq_private_buf.len) > repsize)
		copied = repsize;

	/* Suck it into the iovec, verify checksum if not done by hw. */
	if (csum_partial_copy_to_xdr(&rovr->rq_private_buf, skb))
		goto out_unlock;

	/* Something worked... */
	dst_confirm(skb->dst);

	xprt_complete_rqst(xprt, rovr, copied);

 out_unlock:
	spin_unlock(&xprt->sock_lock);
 dropit:
	skb_free_datagram(sk, skb);
 out:
	if (sk->sleep && waitqueue_active(sk->sleep))
		wake_up_interruptible(sk->sleep);
	read_unlock(&sk->callback_lock);
}

/*
 * Copy from an skb into memory and shrink the skb.
 */
static inline size_t
tcp_copy_data(skb_reader_t *desc, void *p, size_t len)
{
	if (len > desc->count)
		len = desc->count;
	skb_copy_bits(desc->skb, desc->offset, p, len);
	desc->offset += len;
	desc->count -= len;
	return len;
}

/*
 * TCP read fragment marker
 */
static inline void
tcp_read_fraghdr(struct rpc_xprt *xprt, skb_reader_t *desc)
{
	size_t len, used;
	char *p;

	p = ((char *) &xprt->tcp_recm) + xprt->tcp_offset;
	len = sizeof(xprt->tcp_recm) - xprt->tcp_offset;
	used = tcp_copy_data(desc, p, len);
	xprt->tcp_offset += used;
	if (used != len)
		return;
	xprt->tcp_reclen = ntohl(xprt->tcp_recm);
	if (xprt->tcp_reclen & 0x80000000)
		xprt->tcp_flags |= XPRT_LAST_FRAG;
	else
		xprt->tcp_flags &= ~XPRT_LAST_FRAG;
	xprt->tcp_reclen &= 0x7fffffff;
	xprt->tcp_flags &= ~XPRT_COPY_RECM;
	xprt->tcp_offset = 0;
	/* Sanity check of the record length */
	if (xprt->tcp_reclen < 4) {
		printk(KERN_ERR "RPC: Invalid TCP record fragment length\n");
		xprt_disconnect(xprt);
	}
	dprintk("RPC:      reading TCP record fragment of length %d\n",
			xprt->tcp_reclen);
}

static void
tcp_check_recm(struct rpc_xprt *xprt)
{
	if (xprt->tcp_offset == xprt->tcp_reclen) {
		xprt->tcp_flags |= XPRT_COPY_RECM;
		xprt->tcp_offset = 0;
		if (xprt->tcp_flags & XPRT_LAST_FRAG) {
			xprt->tcp_flags &= ~XPRT_COPY_DATA;
			xprt->tcp_flags |= XPRT_COPY_XID;
			xprt->tcp_copied = 0;
		}
	}
}

/*
 * TCP read xid
 */
static inline void
tcp_read_xid(struct rpc_xprt *xprt, skb_reader_t *desc)
{
	size_t len, used;
	char *p;

	len = sizeof(xprt->tcp_xid) - xprt->tcp_offset;
	dprintk("RPC:      reading XID (%Zu bytes)\n", len);
	p = ((char *) &xprt->tcp_xid) + xprt->tcp_offset;
	used = tcp_copy_data(desc, p, len);
	xprt->tcp_offset += used;
	if (used != len)
		return;
	xprt->tcp_flags &= ~XPRT_COPY_XID;
	xprt->tcp_flags |= XPRT_COPY_DATA;
	xprt->tcp_copied = 4;
	dprintk("RPC:      reading reply for XID %08x\n", xprt->tcp_xid);
	tcp_check_recm(xprt);
}

/*
 * TCP read and complete request
 */
static inline void
tcp_read_request(struct rpc_xprt *xprt, skb_reader_t *desc)
{
	struct rpc_rqst *req;
	struct xdr_buf *rcvbuf;
	size_t len;

	/* Find and lock the request corresponding to this xid */
	spin_lock(&xprt->sock_lock);
	req = xprt_lookup_rqst(xprt, xprt->tcp_xid);
	if (!req) {
		xprt->tcp_flags &= ~XPRT_COPY_DATA;
		dprintk("RPC:      XID %08x request not found!\n",
				xprt->tcp_xid);
		spin_unlock(&xprt->sock_lock);
		return;
	}

	rcvbuf = &req->rq_private_buf;
	len = desc->count;
	if (len > xprt->tcp_reclen - xprt->tcp_offset) {
		skb_reader_t my_desc;

		len = xprt->tcp_reclen - xprt->tcp_offset;
		memcpy(&my_desc, desc, sizeof(my_desc));
		my_desc.count = len;
		xdr_partial_copy_from_skb(rcvbuf, xprt->tcp_copied,
					  &my_desc, tcp_copy_data);
		desc->count -= len;
		desc->offset += len;
	} else
		xdr_partial_copy_from_skb(rcvbuf, xprt->tcp_copied,
					  desc, tcp_copy_data);
	xprt->tcp_copied += len;
	xprt->tcp_offset += len;

	if (xprt->tcp_copied == req->rq_private_buf.len)
		xprt->tcp_flags &= ~XPRT_COPY_DATA;
	else if (xprt->tcp_offset == xprt->tcp_reclen) {
		if (xprt->tcp_flags & XPRT_LAST_FRAG)
			xprt->tcp_flags &= ~XPRT_COPY_DATA;
	}

	if (!(xprt->tcp_flags & XPRT_COPY_DATA)) {
		dprintk("RPC: %4d received reply complete\n",
				req->rq_task->tk_pid);
		xprt_complete_rqst(xprt, req, xprt->tcp_copied);
	}
	spin_unlock(&xprt->sock_lock);
	tcp_check_recm(xprt);
}

/*
 * TCP discard extra bytes from a short read
 */
static inline void
tcp_read_discard(struct rpc_xprt *xprt, skb_reader_t *desc)
{
	size_t len;

	len = xprt->tcp_reclen - xprt->tcp_offset;
	if (len > desc->count)
		len = desc->count;
	desc->count -= len;
	desc->offset += len;
	xprt->tcp_offset += len;
	tcp_check_recm(xprt);
}

/*
 * TCP record receive routine
 * We first have to grab the record marker, then the XID, then the data.
 */
static int
tcp_data_recv(read_descriptor_t *rd_desc, struct sk_buff *skb,
		unsigned int offset, size_t len)
{
	struct rpc_xprt *xprt = (struct rpc_xprt *)rd_desc->buf;
	skb_reader_t desc = { skb, offset, len };

	dprintk("RPC:      tcp_data_recv\n");
	do {
		/* Read in a new fragment marker if necessary */
		/* Can we ever really expect to get completely empty fragments? */
		if (xprt->tcp_flags & XPRT_COPY_RECM) {
			tcp_read_fraghdr(xprt, &desc);
			continue;
		}
		/* Read in the xid if necessary */
		if (xprt->tcp_flags & XPRT_COPY_XID) {
			tcp_read_xid(xprt, &desc);
			continue;
		}
		/* Read in the request data */
		if (xprt->tcp_flags & XPRT_COPY_DATA) {
			tcp_read_request(xprt, &desc);
			continue;
		}
		/* Skip over any trailing bytes on short reads */
		tcp_read_discard(xprt, &desc);
	} while (desc.count);
	dprintk("RPC:      tcp_data_recv done\n");
	return len - desc.count;
}

static void tcp_data_ready(struct sock *sk, int bytes)
{
	struct rpc_xprt *xprt;
	read_descriptor_t rd_desc;

	read_lock(&sk->callback_lock);
	dprintk("RPC:      tcp_data_ready...\n");
	if (!(xprt = xprt_from_sock(sk))) {
		printk("RPC:      tcp_data_ready socket info not found!\n");
		goto out;
	}
	if (xprt->shutdown)
		goto out;

	/* We use rd_desc to pass struct xprt to tcp_data_recv */
	rd_desc.buf = (char *)xprt;
	rd_desc.count = 65536;
	tcp_read_sock(sk, &rd_desc, tcp_data_recv);
out:
	read_unlock(&sk->callback_lock);
}

static void
tcp_state_change(struct sock *sk)
{
	struct rpc_xprt	*xprt;

	read_lock(&sk->callback_lock);
	if (!(xprt = xprt_from_sock(sk)))
		goto out;
	dprintk("RPC:      tcp_state_change client %p...\n", xprt);
	dprintk("RPC:      state %x conn %d dead %d zapped %d\n",
				sk->state, xprt_connected(xprt),
				sk->dead, sk->zapped);

	switch (sk->state) {
	case TCP_ESTABLISHED:
		if (xprt_test_and_set_connected(xprt))
			break;

		/* Reset TCP record info */
		xprt->tcp_offset = 0;
		xprt->tcp_reclen = 0;
		xprt->tcp_copied = 0;
		xprt->tcp_flags = XPRT_COPY_RECM | XPRT_COPY_XID;

		spin_lock_bh(&xprt->sock_lock);
		if (xprt->snd_task && xprt->snd_task->tk_rpcwait == &xprt->pending)
			rpc_wake_up_task(xprt->snd_task);
		spin_unlock_bh(&xprt->sock_lock);
		break;
	case TCP_SYN_SENT:
	case TCP_SYN_RECV:
		break;
	default:
		xprt_disconnect(xprt);
		break;
	}
 out:
	if (sk->sleep && waitqueue_active(sk->sleep))
		wake_up_interruptible_all(sk->sleep);
	read_unlock(&sk->callback_lock);
}

/*
 * Called when more output buffer space is available for this socket.
 * We try not to wake our writers until they can make "significant"
 * progress, otherwise we'll waste resources thrashing sock_sendmsg
 * with a bunch of small requests.
 */
static void
xprt_write_space(struct sock *sk)
{
	struct rpc_xprt	*xprt;
	struct socket	*sock;

	read_lock(&sk->callback_lock);
	if (!(xprt = xprt_from_sock(sk)) || !(sock = sk->socket))
		goto out;
	if (xprt->shutdown)
		goto out;

	/* Wait until we have enough socket memory */
	if (xprt->stream) {
		/* from net/ipv4/tcp.c:tcp_write_space */
		if (tcp_wspace(sk) < tcp_min_write_space(sk))
			goto out;
	} else {
		/* from net/core/sock.c:sock_def_write_space */
		if (!sock_writeable(sk))
			goto out;
	}

	if (!test_and_clear_bit(SOCK_NOSPACE, &sock->flags))
		goto out;

	spin_lock_bh(&xprt->sock_lock);
	if (xprt->snd_task && xprt->snd_task->tk_rpcwait == &xprt->pending)
		rpc_wake_up_task(xprt->snd_task);
	spin_unlock_bh(&xprt->sock_lock);
	if (sk->sleep && waitqueue_active(sk->sleep))
		wake_up_interruptible(sk->sleep);
out:
	read_unlock(&sk->callback_lock);
}

/*
 * RPC receive timeout handler.
 */
static void
xprt_timer(struct rpc_task *task)
{
	struct rpc_rqst	*req = task->tk_rqstp;
	struct rpc_xprt *xprt = req->rq_xprt;

	spin_lock(&xprt->sock_lock);
	if (req->rq_received)
		goto out;

	xprt_adjust_cwnd(req->rq_xprt, -ETIMEDOUT);
	__xprt_put_cong(xprt, req);

	dprintk("RPC: %4d xprt_timer (%s request)\n",
		task->tk_pid, req ? "pending" : "backlogged");

	task->tk_status  = -ETIMEDOUT;
out:
	task->tk_timeout = 0;
	rpc_wake_up_task(task);
	spin_unlock(&xprt->sock_lock);
}

/*
 * Place the actual RPC call.
 * We have to copy the iovec because sendmsg fiddles with its contents.
 */
void
xprt_transmit(struct rpc_task *task)
{
	struct rpc_rqst	*req = task->tk_rqstp;
	struct rpc_xprt	*xprt = req->rq_xprt;

	dprintk("RPC: %4d xprt_transmit(%x)\n", task->tk_pid, 
				*(u32 *)(req->rq_svec[0].iov_base));

	if (xprt->shutdown)
		task->tk_status = -EIO;

	if (task->tk_status < 0)
		return;

	if (task->tk_rpcwait)
		rpc_remove_wait_queue(task);

	/* set up everything as needed. */
	/* Write the record marker */
	if (xprt->stream) {
		u32	*marker = req->rq_svec[0].iov_base;

		*marker = htonl(0x80000000|(req->rq_slen-sizeof(*marker)));
	}

	spin_lock_bh(&xprt->sock_lock);
	if (req->rq_received != 0 && !req->rq_bytes_sent)
		goto out_notrans;

	if (!__xprt_lock_write(xprt, task))
		goto out_notrans;

	if (!xprt_connected(xprt)) {
		task->tk_status = -ENOTCONN;
		goto out_notrans;
	}

	if (list_empty(&req->rq_list)) {
		/* Update the softirq receive buffer */
		memcpy(&req->rq_private_buf, &req->rq_rcv_buf,
				sizeof(req->rq_private_buf));
		list_add_tail(&req->rq_list, &xprt->recv);
	}
	spin_unlock_bh(&xprt->sock_lock);

	do_xprt_transmit(task);
	return;
out_notrans:
	spin_unlock_bh(&xprt->sock_lock);
}

static void
do_xprt_transmit(struct rpc_task *task)
{
	struct rpc_clnt *clnt = task->tk_client;
	struct rpc_rqst	*req = task->tk_rqstp;
	struct rpc_xprt	*xprt = req->rq_xprt;
	int status, retry = 0;


	/* Continue transmitting the packet/record. We must be careful
	 * to cope with writespace callbacks arriving _after_ we have
	 * called xprt_sendmsg().
	 */
	while (1) {
		req->rq_xtime = jiffies;
		status = xprt_sendmsg(xprt, req);

		if (status < 0)
			break;

		if (xprt->stream) {
			req->rq_bytes_sent += status;

			/* If we've sent the entire packet, immediately
			 * reset the count of bytes sent. */
			if (req->rq_bytes_sent >= req->rq_slen) {
				req->rq_bytes_sent = 0;
				goto out_receive;
			}
		} else {
			if (status >= req->rq_slen)
				goto out_receive;
			status = -EAGAIN;
			break;
		}

		dprintk("RPC: %4d xmit incomplete (%d left of %d)\n",
				task->tk_pid, req->rq_slen - req->rq_bytes_sent,
				req->rq_slen);

		status = -EAGAIN;
		if (retry++ > 50)
			break;
	}

	/* If we're doing a resend and have received a reply already,
	 * then exit early.
	 * Note, though, that we can't do this if we've already started
	 * resending down a TCP stream.
	 */
	task->tk_status = status;

	switch (status) {
	case -EAGAIN:
		if (test_bit(SOCK_ASYNC_NOSPACE, &xprt->sock->flags)) {
			/* Protect against races with xprt_write_space */
			spin_lock_bh(&xprt->sock_lock);
			/* Don't race with disconnect */
			if (!xprt_connected(xprt))
				task->tk_status = -ENOTCONN;
			else if (test_bit(SOCK_NOSPACE, &xprt->sock->flags)) {
				task->tk_timeout = req->rq_timeout.to_current;
				rpc_sleep_on(&xprt->pending, task, NULL, NULL);
			}
			spin_unlock_bh(&xprt->sock_lock);
			return;
		}
		/* Keep holding the socket if it is blocked */
		rpc_delay(task, HZ>>4);
		return;
	case -ECONNREFUSED:
		task->tk_timeout = RPC_REESTABLISH_TIMEOUT;
		rpc_sleep_on(&xprt->sending, task, NULL, NULL);
	case -ENOTCONN:
		return;
	default:
		if (xprt->stream)
			xprt_disconnect(xprt);
	}
	xprt_release_write(xprt, task);
	return;
 out_receive:
	dprintk("RPC: %4d xmit complete\n", task->tk_pid);
	spin_lock_bh(&xprt->sock_lock);
	/* Set the task's receive timeout value */
	if (!xprt->nocong) {
		int timer = rpcproc_timer(clnt, task->tk_msg.rpc_proc);
		task->tk_timeout = rpc_calc_rto(&clnt->cl_rtt, timer);
		task->tk_timeout <<= rpc_ntimeo(&clnt->cl_rtt, timer);
		task->tk_timeout <<= clnt->cl_timeout.to_retries
			- req->rq_timeout.to_retries;
		if (task->tk_timeout > req->rq_timeout.to_maxval)
			task->tk_timeout = req->rq_timeout.to_maxval;
	} else
		task->tk_timeout = req->rq_timeout.to_current;
	/* Don't race with disconnect */
	if (!xprt_connected(xprt))
		task->tk_status = -ENOTCONN;
	else if (!req->rq_received)
		rpc_sleep_on(&xprt->pending, task, NULL, xprt_timer);
	__xprt_release_write(xprt, task);
	spin_unlock_bh(&xprt->sock_lock);
}

/*
 * Reserve an RPC call slot.
 */
void
xprt_reserve(struct rpc_task *task)
{
	struct rpc_xprt	*xprt = task->tk_xprt;

	task->tk_status = -EIO;
	if (!xprt->shutdown) {
		spin_lock(&xprt->xprt_lock);
		do_xprt_reserve(task);
		spin_unlock(&xprt->xprt_lock);
	}
}

static inline void
do_xprt_reserve(struct rpc_task *task)
{
	struct rpc_xprt	*xprt = task->tk_xprt;

	task->tk_status = 0;
	if (task->tk_rqstp)
		return;
	if (xprt->free) {
		struct rpc_rqst	*req = xprt->free;
		xprt->free = req->rq_next;
		req->rq_next = NULL;
		task->tk_rqstp = req;
		xprt_request_init(task, xprt);
		return;
	}
	dprintk("RPC:      waiting for request slot\n");
	task->tk_status = -EAGAIN;
	task->tk_timeout = 0;
	rpc_sleep_on(&xprt->backlog, task, NULL, NULL);
}

/*
 * Allocate a 'unique' XID
 */
static u32
xprt_alloc_xid(void)
{
	static spinlock_t xid_lock = SPIN_LOCK_UNLOCKED;
	static int need_init = 1;
	static u32 xid;
	u32 ret;

	spin_lock(&xid_lock);
	if (unlikely(need_init)) {
		xid = CURRENT_TIME << 12;
		need_init = 0;
	}
	ret = xid++;
	spin_unlock(&xid_lock);
	return ret;
}

/*
 * Initialize RPC request
 */
static void
xprt_request_init(struct rpc_task *task, struct rpc_xprt *xprt)
{
	struct rpc_rqst	*req = task->tk_rqstp;

	req->rq_timeout = xprt->timeout;
	req->rq_task	= task;
	req->rq_xprt    = xprt;
	req->rq_xid     = xprt_alloc_xid();
	INIT_LIST_HEAD(&req->rq_list);
	dprintk("RPC: %4d reserved req %p xid %08x\n", task->tk_pid,
			req, req->rq_xid);
}

/*
 * Release an RPC call slot
 */
void
xprt_release(struct rpc_task *task)
{
	struct rpc_xprt	*xprt = task->tk_xprt;
	struct rpc_rqst	*req;

	if (!(req = task->tk_rqstp))
		return;
	spin_lock_bh(&xprt->sock_lock);
	__xprt_release_write(xprt, task);
	__xprt_put_cong(xprt, req);
	if (!list_empty(&req->rq_list))
		list_del(&req->rq_list);
	spin_unlock_bh(&xprt->sock_lock);
	task->tk_rqstp = NULL;
	memset(req, 0, sizeof(*req));	/* mark unused */

	dprintk("RPC: %4d release request %p\n", task->tk_pid, req);

	spin_lock(&xprt->xprt_lock);
	req->rq_next = xprt->free;
	xprt->free   = req;

	xprt_clear_backlog(xprt);
	spin_unlock(&xprt->xprt_lock);
}

/*
 * Set default timeout parameters
 */
void
xprt_default_timeout(struct rpc_timeout *to, int proto)
{
	if (proto == IPPROTO_UDP)
		xprt_set_timeout(to, 5,  5 * HZ);
	else
		xprt_set_timeout(to, 5, 60 * HZ);
}

/*
 * Set constant timeout
 */
void
xprt_set_timeout(struct rpc_timeout *to, unsigned int retr, unsigned long incr)
{
	to->to_current   = 
	to->to_initval   = 
	to->to_increment = incr;
	to->to_maxval    = incr * retr;
	to->to_retries   = retr;
	to->to_exponential = 0;
}

/*
 * Initialize an RPC client
 */
static struct rpc_xprt *
xprt_setup(int proto, struct sockaddr_in *ap, struct rpc_timeout *to)
{
	struct rpc_xprt	*xprt;
	struct rpc_rqst	*req;
	int		i;

	dprintk("RPC:      setting up %s transport...\n",
				proto == IPPROTO_UDP? "UDP" : "TCP");

	if ((xprt = kmalloc(sizeof(struct rpc_xprt), GFP_KERNEL)) == NULL)
		return NULL;
	memset(xprt, 0, sizeof(*xprt)); /* Nnnngh! */

	xprt->addr = *ap;
	xprt->prot = proto;
	xprt->stream = (proto == IPPROTO_TCP)? 1 : 0;
	if (xprt->stream) {
		xprt->cwnd = RPC_MAXCWND;
		xprt->nocong = 1;
	} else
		xprt->cwnd = RPC_INITCWND;
	spin_lock_init(&xprt->sock_lock);
	spin_lock_init(&xprt->xprt_lock);
	init_waitqueue_head(&xprt->cong_wait);

	INIT_LIST_HEAD(&xprt->recv);

	/* Set timeout parameters */
	if (to) {
		xprt->timeout = *to;
		xprt->timeout.to_current = to->to_initval;
	} else
		xprt_default_timeout(&xprt->timeout, xprt->prot);

	INIT_RPC_WAITQ(&xprt->pending, "xprt_pending");
	INIT_RPC_WAITQ(&xprt->sending, "xprt_sending");
	INIT_RPC_WAITQ(&xprt->resend, "xprt_resend");
	INIT_RPC_WAITQ(&xprt->backlog, "xprt_backlog");

	/* initialize free list */
	for (i = 0, req = xprt->slot; i < RPC_MAXREQS-1; i++, req++)
		req->rq_next = req + 1;
	req->rq_next = NULL;
	xprt->free = xprt->slot;

	/* Check whether we want to use a reserved port */
	xprt->resvport = capable(CAP_NET_BIND_SERVICE) ? 1 : 0;

	dprintk("RPC:      created transport %p\n", xprt);
	
	return xprt;
}

/*
 * Bind to a reserved port
 */
static inline int
xprt_bindresvport(struct socket *sock)
{
	struct sockaddr_in myaddr;
	int		err, port;
	kernel_cap_t saved_cap = current->cap_effective;

	/* Override capabilities.
	 * They were checked in xprt_create_proto i.e. at mount time
	 */
	cap_raise (current->cap_effective, CAP_NET_BIND_SERVICE);

	memset(&myaddr, 0, sizeof(myaddr));
	myaddr.sin_family = AF_INET;
	port = 800;
	do {
		myaddr.sin_port = htons(port);
		err = sock->ops->bind(sock, (struct sockaddr *) &myaddr,
						sizeof(myaddr));
	} while (err == -EADDRINUSE && --port > 0);
	current->cap_effective = saved_cap;

	if (err < 0)
		printk("RPC: Can't bind to reserved port (%d).\n", -err);

	return err;
}

static int 
xprt_bind_socket(struct rpc_xprt *xprt, struct socket *sock)
{
	struct sock	*sk = sock->sk;

	if (xprt->inet)
		return -EBUSY;

	write_lock_bh(&sk->callback_lock);
	sk->user_data = xprt;
	xprt->old_data_ready = sk->data_ready;
	xprt->old_state_change = sk->state_change;
	xprt->old_write_space = sk->write_space;
	if (xprt->prot == IPPROTO_UDP) {
		sk->data_ready = udp_data_ready;
		sk->no_check = UDP_CSUM_NORCV;
		xprt_set_connected(xprt);
	} else {
		struct tcp_opt *tp = &(sk->tp_pinfo.af_tcp);
		tp->nonagle = 1;	/* disable Nagle's algorithm */
		sk->data_ready = tcp_data_ready;
		sk->state_change = tcp_state_change;
		xprt_clear_connected(xprt);
	}
	sk->write_space = xprt_write_space;

	/* Reset to new socket */
	xprt->sock = sock;
	xprt->inet = sk;
	write_unlock_bh(&sk->callback_lock);

	return 0;
}

/*
 * Set socket buffer length
 */
void
xprt_sock_setbufsize(struct rpc_xprt *xprt)
{
	struct sock *sk = xprt->inet;

	if (xprt->stream)
		return;
	if (xprt->rcvsize) {
		sk->userlocks |= SOCK_RCVBUF_LOCK;
		sk->rcvbuf = xprt->rcvsize * RPC_MAXCONG * 2;
	}
	if (xprt->sndsize) {
		sk->userlocks |= SOCK_SNDBUF_LOCK;
		sk->sndbuf = xprt->sndsize * RPC_MAXCONG * 2;
		sk->write_space(sk);
	}
}

/*
 * Create a client socket given the protocol and peer address.
 */
static struct socket *
xprt_create_socket(int proto, struct rpc_timeout *to, int resvport)
{
	struct socket	*sock;
	int		type, err;

	dprintk("RPC:      xprt_create_socket(%s %d)\n",
			   (proto == IPPROTO_UDP)? "udp" : "tcp", proto);

	type = (proto == IPPROTO_UDP)? SOCK_DGRAM : SOCK_STREAM;

	if ((err = sock_create(PF_INET, type, proto, &sock)) < 0) {
		printk("RPC: can't create socket (%d).\n", -err);
		goto failed;
	}

	/* bind to a reserved port */
	if (resvport && xprt_bindresvport(sock) < 0)
		goto failed;

	return sock;

failed:
	sock_release(sock);
	return NULL;
}

/*
 * Create an RPC client transport given the protocol and peer address.
 */
struct rpc_xprt *
xprt_create_proto(int proto, struct sockaddr_in *sap, struct rpc_timeout *to)
{
	struct rpc_xprt	*xprt;

	xprt = xprt_setup(proto, sap, to);
	if (!xprt)
		goto out_bad;

	dprintk("RPC:      xprt_create_proto created xprt %p\n", xprt);
	return xprt;
out_bad:
	dprintk("RPC:      xprt_create_proto failed\n");
	if (xprt)
		kfree(xprt);
	return NULL;
}

/*
 * Prepare for transport shutdown.
 */
void
xprt_shutdown(struct rpc_xprt *xprt)
{
	xprt->shutdown = 1;
	rpc_wake_up(&xprt->sending);
	rpc_wake_up(&xprt->resend);
	rpc_wake_up(&xprt->pending);
	rpc_wake_up(&xprt->backlog);
	if (waitqueue_active(&xprt->cong_wait))
		wake_up(&xprt->cong_wait);
}

/*
 * Clear the xprt backlog queue
 */
int
xprt_clear_backlog(struct rpc_xprt *xprt) {
	rpc_wake_up_next(&xprt->backlog);
	if (waitqueue_active(&xprt->cong_wait))
		wake_up(&xprt->cong_wait);
	return 1;
}

/*
 * Destroy an RPC transport, killing off all requests.
 */
int
xprt_destroy(struct rpc_xprt *xprt)
{
	dprintk("RPC:      destroying transport %p\n", xprt);
	xprt_shutdown(xprt);
	xprt_close(xprt);
	kfree(xprt);

	return 0;
}
