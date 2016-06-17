/*
 *  linux/fs/ncpfs/sock.c
 *
 *  Copyright (C) 1992, 1993  Rick Sladkey
 *
 *  Modified 1995, 1996 by Volker Lendecke to be usable for ncp
 *  Modified 1997 Peter Waltenberg, Bill Hawes, David Woodhouse for 2.1 dcache
 *
 */

#include <linux/config.h>

#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/socket.h>
#include <linux/fcntl.h>
#include <linux/stat.h>
#include <asm/uaccess.h>
#include <linux/in.h>
#include <linux/net.h>
#include <linux/mm.h>
#include <linux/netdevice.h>
#include <linux/signal.h>
#include <net/scm.h>
#include <net/sock.h>
#include <linux/ipx.h>
#include <linux/poll.h>
#include <linux/file.h>

#include <linux/ncp_fs.h>

#ifdef CONFIG_NCPFS_PACKET_SIGNING
#include "ncpsign_kernel.h"
#endif

static int _recv(struct socket *sock, unsigned char *ubuf, int size,
		 unsigned flags)
{
	struct iovec iov;
	struct msghdr msg;
	struct scm_cookie scm;

	memset(&scm, 0, sizeof(scm));

	iov.iov_base = ubuf;
	iov.iov_len = size;

	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_control = NULL;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	return sock->ops->recvmsg(sock, &msg, size, flags, &scm);
}

static int _send(struct socket *sock, const void *buff, int len)
{
	struct iovec iov;
	struct msghdr msg;
	struct scm_cookie scm;
	int err;

	iov.iov_base = (void *) buff;
	iov.iov_len = len;

	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_control = NULL;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_flags = 0;

	err = scm_send(sock, &msg, &scm);
	if (err < 0) {
		return err;
	}
	err = sock->ops->sendmsg(sock, &msg, len, &scm);
	scm_destroy(&scm);
	return err;
}

static int do_ncp_rpc_call(struct ncp_server *server, int size,
		struct ncp_reply_header* reply_buf, int max_reply_size)
{
	struct file *file;
	struct socket *sock;
	int result;
	char *start = server->packet;
	poll_table wait_table;
	int init_timeout, max_timeout;
	int timeout;
	int retrans;
	int major_timeout_seen;
	int acknowledge_seen;
	int n;

	/* We have to check the result, so store the complete header */
	struct ncp_request_header request =
	*((struct ncp_request_header *) (server->packet));

	struct ncp_reply_header reply;

	file = server->ncp_filp;
	sock = &file->f_dentry->d_inode->u.socket_i;

	init_timeout = server->m.time_out;
	max_timeout = NCP_MAX_RPC_TIMEOUT;
	retrans = server->m.retry_count;
	major_timeout_seen = 0;
	acknowledge_seen = 0;

	for (n = 0, timeout = init_timeout;; n++, timeout <<= 1) {
		/*
		DDPRINTK("ncpfs: %08lX:%02X%02X%02X%02X%02X%02X:%04X\n",
			 htonl(server->m.serv_addr.sipx_network),
			 server->m.serv_addr.sipx_node[0],
			 server->m.serv_addr.sipx_node[1],
			 server->m.serv_addr.sipx_node[2],
			 server->m.serv_addr.sipx_node[3],
			 server->m.serv_addr.sipx_node[4],
			 server->m.serv_addr.sipx_node[5],
			 ntohs(server->m.serv_addr.sipx_port));
		*/
		DDPRINTK("ncpfs: req.typ: %04X, con: %d, "
			 "seq: %d",
			 request.type,
			 (request.conn_high << 8) + request.conn_low,
			 request.sequence);
		DDPRINTK(" func: %d\n",
			 request.function);

		result = _send(sock, (void *) start, size);
		if (result < 0) {
			printk(KERN_ERR "ncp_rpc_call: send error = %d\n", result);
			break;
		}
	      re_select:
		poll_initwait(&wait_table);
		/* mb() is not necessary because ->poll() will serialize
		   instructions adding the wait_table waitqueues in the
		   waitqueue-head before going to calculate the mask-retval. */
		__set_current_state(TASK_INTERRUPTIBLE);
		if (!(sock->ops->poll(file, sock, &wait_table) & POLLIN)) {
			int timed_out;
			if (timeout > max_timeout) {
				/* JEJB/JSP 2/7/94
				 * This is useful to see if the system is
				 * hanging */
				if (acknowledge_seen == 0) {
					printk(KERN_WARNING "NCP max timeout\n");
				}
				timeout = max_timeout;
			}
			timed_out = !schedule_timeout(timeout);
			poll_freewait(&wait_table);
			current->state = TASK_RUNNING;
			if (signal_pending(current)) {
				result = -ERESTARTSYS;
				break;
			}
			if(wait_table.error) {
				result = wait_table.error;
				break;
			}
			if (timed_out) {
				if (n < retrans)
					continue;
				if (server->m.flags & NCP_MOUNT_SOFT) {
					printk(KERN_WARNING "NCP server not responding\n");
					result = -EIO;
					break;
				}
				n = 0;
				timeout = init_timeout;
				if (init_timeout < max_timeout)
					init_timeout <<= 1;
				if (!major_timeout_seen) {
					printk(KERN_WARNING "NCP server not responding\n");
				}
				major_timeout_seen = 1;
				continue;
			}
		} else {
			poll_freewait(&wait_table);
		}
		current->state = TASK_RUNNING;

		/* Get the header from the next packet using a peek, so keep it
		 * on the recv queue.  If it is wrong, it will be some reply
		 * we don't now need, so discard it */
		result = _recv(sock, (void *) &reply, sizeof(reply),
			       MSG_PEEK | MSG_DONTWAIT);
		if (result < 0) {
			if (result == -EAGAIN) {
				DDPRINTK("ncp_rpc_call: bad select ready\n");
				goto re_select;
			}
			if (result == -ECONNREFUSED) {
				DPRINTK("ncp_rpc_call: server playing coy\n");
				goto re_select;
			}
			if (result != -ERESTARTSYS) {
				printk(KERN_ERR "ncp_rpc_call: recv error = %d\n",
				       -result);
			}
			break;
		}
		if ((result == sizeof(reply))
		    && (reply.type == NCP_POSITIVE_ACK)) {
			/* Throw away the packet */
			DPRINTK("ncp_rpc_call: got positive acknowledge\n");
			_recv(sock, (void *) &reply, sizeof(reply),
			      MSG_DONTWAIT);
			n = 0;
			timeout = max_timeout;
			acknowledge_seen = 1;
			goto re_select;
		}
		DDPRINTK("ncpfs: rep.typ: %04X, con: %d, tsk: %d,"
			 "seq: %d\n",
			 reply.type,
			 (reply.conn_high << 8) + reply.conn_low,
			 reply.task,
			 reply.sequence);

		if ((result >= sizeof(reply))
		    && (reply.type == NCP_REPLY)
		    && ((request.type == NCP_ALLOC_SLOT_REQUEST)
			|| ((reply.sequence == request.sequence)
			    && (reply.conn_low == request.conn_low)
/* seem to get wrong task from NW311 && (reply.task      == request.task) */
			    && (reply.conn_high == request.conn_high)))) {
			if (major_timeout_seen)
				printk(KERN_NOTICE "NCP server OK\n");
			break;
		}
		/* JEJB/JSP 2/7/94
		 * we have xid mismatch, so discard the packet and start
		 * again.  What a hack! but I can't call recvfrom with
		 * a null buffer yet. */
		_recv(sock, (void *) &reply, sizeof(reply), MSG_DONTWAIT);

		DPRINTK("ncp_rpc_call: reply mismatch\n");
		goto re_select;
	}
	/* 
	 * we have the correct reply, so read into the correct place and
	 * return it
	 */
	result = _recv(sock, (void *)reply_buf, max_reply_size, MSG_DONTWAIT);
	if (result < 0) {
		printk(KERN_WARNING "NCP: notice message: result=%d\n", result);
	} else if (result < sizeof(struct ncp_reply_header)) {
		printk(KERN_ERR "NCP: just caught a too small read memory size..., "
		       "email to NET channel\n");
		printk(KERN_ERR "NCP: result=%d\n", result);
		result = -EIO;
	}

	return result;
}

static int do_tcp_rcv(struct ncp_server *server, void *buffer, size_t len) {
	poll_table wait_table;
	struct file *file;
	struct socket *sock;
	int init_timeout;
	size_t dataread;
	int result = 0;
	
	file = server->ncp_filp;
	sock = &file->f_dentry->d_inode->u.socket_i;
	
	dataread = 0;

	init_timeout = server->m.time_out * 20;
	
	/* hard-mounted volumes have no timeout, except connection close... */
	if (!(server->m.flags & NCP_MOUNT_SOFT))
		init_timeout = 0x7FFF0000;

	while (len) {
		poll_initwait(&wait_table);
		/* mb() is not necessary because ->poll() will serialize
		   instructions adding the wait_table waitqueues in the
		   waitqueue-head before going to calculate the mask-retval. */
		__set_current_state(TASK_INTERRUPTIBLE);
		if (!(sock->ops->poll(file, sock, &wait_table) & POLLIN)) {
			init_timeout = schedule_timeout(init_timeout);
			poll_freewait(&wait_table);
			current->state = TASK_RUNNING;
			if (signal_pending(current)) {
				return -ERESTARTSYS;
			}
			if (!init_timeout) {
				return -EIO;
			}
			if(wait_table.error) {
				return wait_table.error;
			}
		} else {
			poll_freewait(&wait_table);
		}
		current->state = TASK_RUNNING;

		result = _recv(sock, buffer, len, MSG_DONTWAIT);
		if (result < 0) {
			if (result == -EAGAIN) {
				DDPRINTK("ncpfs: tcp: bad select ready\n");
				continue;
			}
			return result;
		}
		if (result == 0) {
			printk(KERN_ERR "ncpfs: tcp: EOF on socket\n");
			return -EIO;
		}
		if (result > len) {
			printk(KERN_ERR "ncpfs: tcp: bug in recvmsg\n");
			return -EIO;			
		}
		dataread += result;
		buffer += result;
		len -= result;
	}
	return 0;
}	

#define NCP_TCP_XMIT_MAGIC	(0x446D6454)
#define NCP_TCP_XMIT_VERSION	(1)
#define NCP_TCP_RCVD_MAGIC	(0x744E6350)

static int do_ncp_tcp_rpc_call(struct ncp_server *server, int size,
		struct ncp_reply_header* reply_buf, int max_reply_size)
{
	struct file *file;
	struct socket *sock;
	int result;
	struct iovec iov[2];
	struct msghdr msg;
	struct scm_cookie scm;
	__u32 ncptcp_rcvd_hdr[2];
	__u32 ncptcp_xmit_hdr[4];
	int   datalen;

	/* We have to check the result, so store the complete header */
	struct ncp_request_header request =
	*((struct ncp_request_header *) (server->packet));

	file = server->ncp_filp;
	sock = &file->f_dentry->d_inode->u.socket_i;
	
	ncptcp_xmit_hdr[0] = htonl(NCP_TCP_XMIT_MAGIC);
	ncptcp_xmit_hdr[1] = htonl(size + 16);
	ncptcp_xmit_hdr[2] = htonl(NCP_TCP_XMIT_VERSION);
	ncptcp_xmit_hdr[3] = htonl(max_reply_size + 8);

	DDPRINTK("ncpfs: req.typ: %04X, con: %d, "
		 "seq: %d",
		 request.type,
		 (request.conn_high << 8) + request.conn_low,
		 request.sequence);
	DDPRINTK(" func: %d\n",
		 request.function);

	iov[1].iov_base = (void *) server->packet;
	iov[1].iov_len = size;
	iov[0].iov_base = ncptcp_xmit_hdr;
	iov[0].iov_len = 16;
	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_control = NULL;
	msg.msg_iov = iov;
	msg.msg_iovlen = 2;
	msg.msg_flags = MSG_NOSIGNAL;

	result = scm_send(sock, &msg, &scm);
	if (result < 0) {
		return result;
	}
	result = sock->ops->sendmsg(sock, &msg, size + 16, &scm);
	scm_destroy(&scm);
	if (result < 0) {
		printk(KERN_ERR "ncpfs: tcp: Send failed: %d\n", result);
		return result;
	}
rstrcv:
	result = do_tcp_rcv(server, ncptcp_rcvd_hdr, 8);
	if (result)
		return result;
	if (ncptcp_rcvd_hdr[0] != htonl(NCP_TCP_RCVD_MAGIC)) {
		printk(KERN_ERR "ncpfs: tcp: Unexpected reply type %08X\n", ntohl(ncptcp_rcvd_hdr[0]));
		return -EIO;
	}
	datalen = ntohl(ncptcp_rcvd_hdr[1]);
	if (datalen < 8 + sizeof(*reply_buf) || datalen > max_reply_size + 8) {
		printk(KERN_ERR "ncpfs: tcp: Unexpected reply len %d\n", datalen);
		return -EIO;
	}
	datalen -= 8;
	result = do_tcp_rcv(server, reply_buf, datalen);
	if (result)
		return result;
	if (reply_buf->type != NCP_REPLY) {
		DDPRINTK("ncpfs: tcp: Unexpected NCP type %02X\n", reply_buf->type);
		goto rstrcv;
	}
	if (request.type == NCP_ALLOC_SLOT_REQUEST)
		return datalen;
	if (reply_buf->sequence != request.sequence) {
		printk(KERN_ERR "ncpfs: tcp: Bad sequence number\n");
		return -EIO;
	}
	if ((reply_buf->conn_low != request.conn_low) ||
	    (reply_buf->conn_high != request.conn_high)) {
		printk(KERN_ERR "ncpfs: tcp: Connection number mismatch\n");
		return -EIO;
	}
	return datalen;
}

/*
 * We need the server to be locked here, so check!
 */

static int ncp_do_request(struct ncp_server *server, int size,
		void* reply, int max_reply_size)
{
	struct file *file;
	struct socket *sock;
	int result;

	if (server->lock == 0) {
		printk(KERN_ERR "ncpfs: Server not locked!\n");
		return -EIO;
	}
	if (!ncp_conn_valid(server)) {
		return -EIO;
	}
#ifdef CONFIG_NCPFS_PACKET_SIGNING
	if (server->sign_active)
	{
		sign_packet(server, &size);
	}
#endif /* CONFIG_NCPFS_PACKET_SIGNING */
	file = server->ncp_filp;
	sock = &file->f_dentry->d_inode->u.socket_i;
	/* N.B. this isn't needed ... check socket type? */
	if (!sock) {
		printk(KERN_ERR "ncp_rpc_call: socki_lookup failed\n");
		result = -EBADF;
	} else {
		mm_segment_t fs;
		sigset_t old_set;
		unsigned long mask, flags;

		spin_lock_irqsave(&current->sigmask_lock, flags);
		old_set = current->blocked;
		if (current->flags & PF_EXITING)
			mask = 0;
		else
			mask = sigmask(SIGKILL);
		if (server->m.flags & NCP_MOUNT_INTR) {
			/* FIXME: This doesn't seem right at all.  So, like,
			   we can't handle SIGINT and get whatever to stop?
			   What if we've blocked it ourselves?  What about
			   alarms?  Why, in fact, are we mucking with the
			   sigmask at all? -- r~ */
			if (current->sig->action[SIGINT - 1].sa.sa_handler == SIG_DFL)
				mask |= sigmask(SIGINT);
			if (current->sig->action[SIGQUIT - 1].sa.sa_handler == SIG_DFL)
				mask |= sigmask(SIGQUIT);
		}
		siginitsetinv(&current->blocked, mask);
		recalc_sigpending(current);
		spin_unlock_irqrestore(&current->sigmask_lock, flags);
		
		fs = get_fs();
		set_fs(get_ds());

		if (sock->type == SOCK_STREAM)
			result = do_ncp_tcp_rpc_call(server, size, reply, max_reply_size);
		else
			result = do_ncp_rpc_call(server, size, reply, max_reply_size);

		set_fs(fs);

		spin_lock_irqsave(&current->sigmask_lock, flags);
		current->blocked = old_set;
		recalc_sigpending(current);
		spin_unlock_irqrestore(&current->sigmask_lock, flags);
	}

	DDPRINTK("do_ncp_rpc_call returned %d\n", result);

	if (result < 0) {
		/* There was a problem with I/O, so the connections is
		 * no longer usable. */
		ncp_invalidate_conn(server);
	}
	return result;
}

/* ncp_do_request assures that at least a complete reply header is
 * received. It assumes that server->current_size contains the ncp
 * request size
 */
int ncp_request2(struct ncp_server *server, int function, 
		void* rpl, int size)
{
	struct ncp_request_header *h;
	struct ncp_reply_header* reply = rpl;
	int request_size = server->current_size
			 - sizeof(struct ncp_request_header);
	int result;

	h = (struct ncp_request_header *) (server->packet);
	if (server->has_subfunction != 0) {
		*(__u16 *) & (h->data[0]) = htons(request_size - 2);
	}
	h->type = NCP_REQUEST;

	server->sequence += 1;
	h->sequence = server->sequence;
	h->conn_low = (server->connection) & 0xff;
	h->conn_high = ((server->connection) & 0xff00) >> 8;
	/*
	 * The server shouldn't know or care what task is making a
	 * request, so we always use the same task number.
	 */
	h->task = 2; /* (current->pid) & 0xff; */
	h->function = function;

	result = ncp_do_request(server, request_size + sizeof(*h), reply, size);
	if (result < 0) {
		DPRINTK("ncp_request_error: %d\n", result);
		goto out;
	}
	server->completion = reply->completion_code;
	server->conn_status = reply->connection_state;
	server->reply_size = result;
	server->ncp_reply_size = result - sizeof(struct ncp_reply_header);

	result = reply->completion_code;

	if (result != 0)
		PPRINTK("ncp_request: completion code=%x\n", result);
out:
	return result;
}

int ncp_connect(struct ncp_server *server)
{
	struct ncp_request_header *h;
	int result;

	h = (struct ncp_request_header *) (server->packet);
	h->type = NCP_ALLOC_SLOT_REQUEST;

	server->sequence = 0;
	h->sequence	= server->sequence;
	h->conn_low	= 0xff;
	h->conn_high	= 0xff;
	h->task		= 2; /* see above */
	h->function	= 0;

	result = ncp_do_request(server, sizeof(*h), server->packet, server->packet_size);
	if (result < 0)
		goto out;
	server->sequence = 0;
	server->connection = h->conn_low + (h->conn_high * 256);
	result = 0;
out:
	return result;
}

int ncp_disconnect(struct ncp_server *server)
{
	struct ncp_request_header *h;

	h = (struct ncp_request_header *) (server->packet);
	h->type = NCP_DEALLOC_SLOT_REQUEST;

	server->sequence += 1;
	h->sequence	= server->sequence;
	h->conn_low	= (server->connection) & 0xff;
	h->conn_high	= ((server->connection) & 0xff00) >> 8;
	h->task		= 2; /* see above */
	h->function	= 0;

	return ncp_do_request(server, sizeof(*h), server->packet, server->packet_size);
}

void ncp_lock_server(struct ncp_server *server)
{
	down(&server->sem);
	if (server->lock)
		printk(KERN_WARNING "ncp_lock_server: was locked!\n");
	server->lock = 1;
}

void ncp_unlock_server(struct ncp_server *server)
{
	if (!server->lock) {
		printk(KERN_WARNING "ncp_unlock_server: was not locked!\n");
		return;
	}
	server->lock = 0;
	up(&server->sem);
}
