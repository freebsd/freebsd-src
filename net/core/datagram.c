/*
 *	SUCS NET3:
 *
 *	Generic datagram handling routines. These are generic for all protocols. Possibly a generic IP version on top
 *	of these would make sense. Not tonight however 8-).
 *	This is used because UDP, RAW, PACKET, DDP, IPX, AX.25 and NetROM layer all have identical poll code and mostly
 *	identical recvmsg() code. So we share it here. The poll was shared before but buried in udp.c so I moved it.
 *
 *	Authors:	Alan Cox <alan@redhat.com>. (datagram_poll() from old udp.c code)
 *
 *	Fixes:
 *		Alan Cox	:	NULL return from skb_peek_copy() understood
 *		Alan Cox	:	Rewrote skb_read_datagram to avoid the skb_peek_copy stuff.
 *		Alan Cox	:	Added support for SOCK_SEQPACKET. IPX can no longer use the SO_TYPE hack but
 *					AX.25 now works right, and SPX is feasible.
 *		Alan Cox	:	Fixed write poll of non IP protocol crash.
 *		Florian  La Roche:	Changed for my new skbuff handling.
 *		Darryl Miles	:	Fixed non-blocking SOCK_SEQPACKET.
 *		Linus Torvalds	:	BSD semantic fixes.
 *		Alan Cox	:	Datagram iovec handling
 *		Darryl Miles	:	Fixed non-blocking SOCK_STREAM.
 *		Alan Cox	:	POSIXisms
 *		Pete Wyckoff    :       Unconnected accept() fix.
 *
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/inet.h>
#include <linux/netdevice.h>
#include <linux/rtnetlink.h>
#include <linux/poll.h>
#include <linux/highmem.h>

#include <net/protocol.h>
#include <linux/skbuff.h>
#include <net/sock.h>
#include <net/checksum.h>


/*
 *	Is a socket 'connection oriented' ?
 */
 
static inline int connection_based(struct sock *sk)
{
	return (sk->type==SOCK_SEQPACKET || sk->type==SOCK_STREAM);
}


/*
 * Wait for a packet..
 */

static int wait_for_packet(struct sock * sk, int *err, long *timeo_p)
{
	int error;

	DECLARE_WAITQUEUE(wait, current);

	__set_current_state(TASK_INTERRUPTIBLE);
	add_wait_queue_exclusive(sk->sleep, &wait);

	/* Socket errors? */
	error = sock_error(sk);
	if (error)
		goto out_err;

	if (!skb_queue_empty(&sk->receive_queue))
		goto ready;

	/* Socket shut down? */
	if (sk->shutdown & RCV_SHUTDOWN)
		goto out_noerr;

	/* Sequenced packets can come disconnected. If so we report the problem */
	error = -ENOTCONN;
	if(connection_based(sk) && !(sk->state==TCP_ESTABLISHED || sk->state==TCP_LISTEN))
		goto out_err;

	/* handle signals */
	if (signal_pending(current))
		goto interrupted;

	*timeo_p = schedule_timeout(*timeo_p);

ready:
	current->state = TASK_RUNNING;
	remove_wait_queue(sk->sleep, &wait);
	return 0;

interrupted:
	error = sock_intr_errno(*timeo_p);
out_err:
	*err = error;
out:
	current->state = TASK_RUNNING;
	remove_wait_queue(sk->sleep, &wait);
	return error;
out_noerr:
	*err = 0;
	error = 1;
	goto out;
}

/*
 *	Get a datagram skbuff, understands the peeking, nonblocking wakeups and possible
 *	races. This replaces identical code in packet,raw and udp, as well as the IPX
 *	AX.25 and Appletalk. It also finally fixes the long standing peek and read
 *	race for datagram sockets. If you alter this routine remember it must be
 *	re-entrant.
 *
 *	This function will lock the socket if a skb is returned, so the caller
 *	needs to unlock the socket in that case (usually by calling skb_free_datagram)
 *
 *	* It does not lock socket since today. This function is
 *	* free of race conditions. This measure should/can improve
 *	* significantly datagram socket latencies at high loads,
 *	* when data copying to user space takes lots of time.
 *	* (BTW I've just killed the last cli() in IP/IPv6/core/netlink/packet
 *	*  8) Great win.)
 *	*			                    --ANK (980729)
 *
 *	The order of the tests when we find no data waiting are specified
 *	quite explicitly by POSIX 1003.1g, don't change them without having
 *	the standard around please.
 */

struct sk_buff *skb_recv_datagram(struct sock *sk, unsigned flags, int noblock, int *err)
{
	int error;
	struct sk_buff *skb;
	long timeo;

	/* Caller is allowed not to check sk->err before skb_recv_datagram() */
	error = sock_error(sk);
	if (error)
		goto no_packet;

	timeo = sock_rcvtimeo(sk, noblock);

	do {
		/* Again only user level code calls this function, so nothing interrupt level
		   will suddenly eat the receive_queue.

		   Look at current nfs client by the way...
		   However, this function was corrent in any case. 8)
		 */
		if (flags & MSG_PEEK)
		{
			unsigned long cpu_flags;

			spin_lock_irqsave(&sk->receive_queue.lock, cpu_flags);
			skb = skb_peek(&sk->receive_queue);
			if(skb!=NULL)
				atomic_inc(&skb->users);
			spin_unlock_irqrestore(&sk->receive_queue.lock, cpu_flags);
		} else
			skb = skb_dequeue(&sk->receive_queue);

		if (skb)
			return skb;

		/* User doesn't want to wait */
		error = -EAGAIN;
		if (!timeo)
			goto no_packet;

	} while (wait_for_packet(sk, err, &timeo) == 0);

	return NULL;

no_packet:
	*err = error;
	return NULL;
}

void skb_free_datagram(struct sock * sk, struct sk_buff *skb)
{
	kfree_skb(skb);
}

/*
 *	Copy a datagram to a linear buffer.
 */

int skb_copy_datagram(const struct sk_buff *skb, int offset, char *to, int size)
{
	struct iovec iov = { to, size };

	return skb_copy_datagram_iovec(skb, offset, &iov, size);
}

/*
 *	Copy a datagram to an iovec.
 *	Note: the iovec is modified during the copy.
 */
int skb_copy_datagram_iovec(const struct sk_buff *skb, int offset, struct iovec *to,
			    int len)
{
	int i, copy;
	int start = skb->len - skb->data_len;

	/* Copy header. */
	if ((copy = start-offset) > 0) {
		if (copy > len)
			copy = len;
		if (memcpy_toiovec(to, skb->data + offset, copy))
			goto fault;
		if ((len -= copy) == 0)
			return 0;
		offset += copy;
	}

	/* Copy paged appendix. Hmm... why does this look so complicated? */
	for (i=0; i<skb_shinfo(skb)->nr_frags; i++) {
		int end;

		BUG_TRAP(start <= offset+len);

		end = start + skb_shinfo(skb)->frags[i].size;
		if ((copy = end-offset) > 0) {
			int err;
			u8  *vaddr;
			skb_frag_t *frag = &skb_shinfo(skb)->frags[i];
			struct page *page = frag->page;

			if (copy > len)
				copy = len;
			vaddr = kmap(page);
			err = memcpy_toiovec(to, vaddr + frag->page_offset +
					     offset-start, copy);
			kunmap(page);
			if (err)
				goto fault;
			if (!(len -= copy))
				return 0;
			offset += copy;
		}
		start = end;
	}

	if (skb_shinfo(skb)->frag_list) {
		struct sk_buff *list;

		for (list = skb_shinfo(skb)->frag_list; list; list=list->next) {
			int end;

			BUG_TRAP(start <= offset+len);

			end = start + list->len;
			if ((copy = end-offset) > 0) {
				if (copy > len)
					copy = len;
				if (skb_copy_datagram_iovec(list, offset-start, to, copy))
					goto fault;
				if ((len -= copy) == 0)
					return 0;
				offset += copy;
			}
			start = end;
		}
	}
	if (len == 0)
		return 0;

fault:
	return -EFAULT;
}

int skb_copy_and_csum_datagram(const struct sk_buff *skb, int offset, u8 *to, int len, unsigned int *csump)
{
	int i, copy;
	int start = skb->len - skb->data_len;
	int pos = 0;

	/* Copy header. */
	if ((copy = start-offset) > 0) {
		int err = 0;
		if (copy > len)
			copy = len;
		*csump = csum_and_copy_to_user(skb->data+offset, to, copy, *csump, &err);
		if (err)
			goto fault;
		if ((len -= copy) == 0)
			return 0;
		offset += copy;
		to += copy;
		pos = copy;
	}

	for (i=0; i<skb_shinfo(skb)->nr_frags; i++) {
		int end;

		BUG_TRAP(start <= offset+len);

		end = start + skb_shinfo(skb)->frags[i].size;
		if ((copy = end-offset) > 0) {
			unsigned int csum2;
			int err = 0;
			u8  *vaddr;
			skb_frag_t *frag = &skb_shinfo(skb)->frags[i];
			struct page *page = frag->page;

			if (copy > len)
				copy = len;
			vaddr = kmap(page);
			csum2 = csum_and_copy_to_user(vaddr + frag->page_offset +
						      offset-start, to, copy, 0, &err);
			kunmap(page);
			if (err)
				goto fault;
			*csump = csum_block_add(*csump, csum2, pos);
			if (!(len -= copy))
				return 0;
			offset += copy;
			to += copy;
			pos += copy;
		}
		start = end;
	}

	if (skb_shinfo(skb)->frag_list) {
		struct sk_buff *list;

		for (list = skb_shinfo(skb)->frag_list; list; list=list->next) {
			int end;

			BUG_TRAP(start <= offset+len);

			end = start + list->len;
			if ((copy = end-offset) > 0) {
				unsigned int csum2 = 0;
				if (copy > len)
					copy = len;
				if (skb_copy_and_csum_datagram(list, offset-start, to, copy, &csum2))
					goto fault;
				*csump = csum_block_add(*csump, csum2, pos);
				if ((len -= copy) == 0)
					return 0;
				offset += copy;
				to += copy;
				pos += copy;
			}
			start = end;
		}
	}
	if (len == 0)
		return 0;

fault:
	return -EFAULT;
}

/* Copy and checkum skb to user iovec. Caller _must_ check that
   skb will fit to this iovec.

   Returns: 0       - success.
            -EINVAL - checksum failure.
	    -EFAULT - fault during copy. Beware, in this case iovec can be
	              modified!
 */

int skb_copy_and_csum_datagram_iovec(const struct sk_buff *skb, int hlen, struct iovec *iov)
{
	unsigned int csum;
	int chunk = skb->len - hlen;

	/* Skip filled elements. Pretty silly, look at memcpy_toiovec, though 8) */
	while (iov->iov_len == 0)
		iov++;

	if (iov->iov_len < chunk) {
		if ((unsigned short)csum_fold(skb_checksum(skb, 0, chunk+hlen, skb->csum)))
			goto csum_error;
		if (skb_copy_datagram_iovec(skb, hlen, iov, chunk))
			goto fault;
	} else {
		csum = csum_partial(skb->data, hlen, skb->csum);
		if (skb_copy_and_csum_datagram(skb, hlen, iov->iov_base, chunk, &csum))
			goto fault;
		if ((unsigned short)csum_fold(csum))
			goto csum_error;
		iov->iov_len -= chunk;
		iov->iov_base += chunk;
	}
	return 0;

csum_error:
	return -EINVAL;

fault:
	return -EFAULT;
}



/*
 *	Datagram poll: Again totally generic. This also handles
 *	sequenced packet sockets providing the socket receive queue
 *	is only ever holding data ready to receive.
 *
 *	Note: when you _don't_ use this routine for this protocol,
 *	and you use a different write policy from sock_writeable()
 *	then please supply your own write_space callback.
 */

unsigned int datagram_poll(struct file * file, struct socket *sock, poll_table *wait)
{
	struct sock *sk = sock->sk;
	unsigned int mask;

	poll_wait(file, sk->sleep, wait);
	mask = 0;

	/* exceptional events? */
	if (sk->err || !skb_queue_empty(&sk->error_queue))
		mask |= POLLERR;
	if (sk->shutdown == SHUTDOWN_MASK)
		mask |= POLLHUP;

	/* readable? */
	if (!skb_queue_empty(&sk->receive_queue) || (sk->shutdown&RCV_SHUTDOWN))
		mask |= POLLIN | POLLRDNORM;

	/* Connection-based need to check for termination and startup */
	if (connection_based(sk)) {
		if (sk->state==TCP_CLOSE)
			mask |= POLLHUP;
		/* connection hasn't started yet? */
		if (sk->state == TCP_SYN_SENT)
			return mask;
	}

	/* writable? */
	if (sock_writeable(sk))
		mask |= POLLOUT | POLLWRNORM | POLLWRBAND;
	else
		set_bit(SOCK_ASYNC_NOSPACE, &sk->socket->flags);

	return mask;
}
