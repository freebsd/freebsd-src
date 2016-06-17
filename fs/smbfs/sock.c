/*
 *  sock.c
 *
 *  Copyright (C) 1995, 1996 by Paal-Kr. Engstad and Volker Lendecke
 *  Copyright (C) 1997 by Volker Lendecke
 *
 *  Please add a note about your changes to smbfs in the ChangeLog file.
 */

#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/socket.h>
#include <linux/fcntl.h>
#include <linux/file.h>
#include <linux/poll.h>
#include <linux/in.h>
#include <linux/net.h>
#include <linux/mm.h>
#include <linux/netdevice.h>
#include <linux/smp_lock.h>
#include <net/scm.h>
#include <net/ip.h>

#include <linux/smb_fs.h>
#include <linux/smb.h>
#include <linux/smbno.h>

#include <asm/uaccess.h>

#include "smb_debug.h"
#include "proto.h"


static int
_recvfrom(struct socket *socket, unsigned char *ubuf, int size,
	  unsigned flags)
{
	struct iovec iov;
	struct msghdr msg;
	struct scm_cookie scm;

	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = NULL;
	iov.iov_base = ubuf;
	iov.iov_len = size;
	
	memset(&scm, 0,sizeof(scm));
	size=socket->ops->recvmsg(socket, &msg, size, flags, &scm);
	if(size>=0)
		scm_recv(socket,&msg,&scm,flags);
	return size;
}

static int
_send(struct socket *socket, const void *buff, int len)
{
	struct iovec iov;
	struct msghdr msg;
	struct scm_cookie scm;
	int err;

	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	
	iov.iov_base = (void *)buff;
	iov.iov_len = len;

	msg.msg_flags = 0;

	err = scm_send(socket, &msg, &scm);
        if (err >= 0)
	{
		err = socket->ops->sendmsg(socket, &msg, len, &scm);
		scm_destroy(&scm);
	}
	return err;
}

struct data_callback {
	struct tq_struct cb;
	struct sock *sk;
};
/*
 * N.B. What happens if we're in here when the socket closes??
 */
static void
found_data(struct sock *sk)
{
	/*
	 * FIXME: copied from sock_def_readable, it should be a call to
	 * server->data_ready()	-- manfreds@colorfullife.com
	 */
	read_lock(&sk->callback_lock);
	if(!sk->dead) {
		wake_up_interruptible(sk->sleep);
		sock_wake_async(sk->socket,1,POLL_IN);
	}
	read_unlock(&sk->callback_lock);
}

static void
smb_data_callback(void* ptr)
{
	struct data_callback* job=ptr;
	struct socket *socket = job->sk->socket;
	unsigned char peek_buf[4];
	int result = 0;
	mm_segment_t fs;
	int count = 100;   /* this is a lot, we should have some data waiting */
	int found = 0;

	fs = get_fs();
	set_fs(get_ds());

	lock_kernel();
	while (count-- > 0) {
		peek_buf[0] = 0;

		result = -EIO;
		if (job->sk->dead) {
			PARANOIA("sock dead!\n");
			break;
		}

		result = _recvfrom(socket, (void *) peek_buf, 1,
				   MSG_PEEK | MSG_DONTWAIT);
		if (result < 0)
			break;
		if (peek_buf[0] != 0x85)
			break;

		/* got SESSION KEEP ALIVE */
		result = _recvfrom(socket, (void *) peek_buf, 4,
				   MSG_DONTWAIT);

		DEBUG1("got SESSION KEEPALIVE\n");

		if (result < 0)
			break;
		found = 1;
	}
	unlock_kernel();
	set_fs(fs);

	DEBUG1("found=%d, count=%d, result=%d\n", found, count, result);
	if (found)
		found_data(job->sk);
	smb_kfree(ptr);
}

static void
smb_data_ready(struct sock *sk, int len)
{
	struct data_callback* job;
	job = smb_kmalloc(sizeof(struct data_callback),GFP_ATOMIC);
	if(job == 0) {
		printk("smb_data_ready: lost SESSION KEEPALIVE due to OOM.\n");
		found_data(sk);
		return;
	}
	INIT_LIST_HEAD(&job->cb.list);
	job->cb.sync = 0;
	job->cb.routine = smb_data_callback;
	job->cb.data = job;
	job->sk = sk;
	schedule_task(&job->cb);
}

int
smb_valid_socket(struct inode * inode)
{
	return (inode && S_ISSOCK(inode->i_mode) && 
		inode->u.socket_i.type == SOCK_STREAM);
}

static struct socket *
server_sock(struct smb_sb_info *server)
{
	struct file *file;

	if (server && (file = server->sock_file))
	{
#ifdef SMBFS_PARANOIA
		if (!smb_valid_socket(file->f_dentry->d_inode))
			PARANOIA("bad socket!\n");
#endif
		return &file->f_dentry->d_inode->u.socket_i;
	}
	return NULL;
}

int
smb_catch_keepalive(struct smb_sb_info *server)
{
	struct socket *socket;
	struct sock *sk;
	void *data_ready;
	int error;

	error = -EINVAL;
	socket = server_sock(server);
	if (!socket)
	{
		printk(KERN_DEBUG "smb_catch_keepalive: did not get valid server!\n");
		server->data_ready = NULL;
		goto out;
	}

	sk = socket->sk;
	if (sk == NULL)
	{
		DEBUG1("sk == NULL");
		server->data_ready = NULL;
		goto out;
	}
	DEBUG1("sk->d_r = %x, server->d_r = %x\n",
		 (unsigned int) (sk->data_ready),
		 (unsigned int) (server->data_ready));

	/*
	 * Install the callback atomically to avoid races ...
	 */
	data_ready = xchg(&sk->data_ready, smb_data_ready);
	if (data_ready != smb_data_ready) {
		server->data_ready = data_ready;
		error = 0;
	} else
		printk(KERN_ERR "smb_catch_keepalive: already done\n");
out:
	return error;
}

int
smb_dont_catch_keepalive(struct smb_sb_info *server)
{
	struct socket *socket;
	struct sock *sk;
	void * data_ready;
	int error;

	error = -EINVAL;
	socket = server_sock(server);
	if (!socket)
	{
		printk(KERN_DEBUG "smb_dont_catch_keepalive: did not get valid server!\n");
		goto out;
	}

	sk = socket->sk;
	if (sk == NULL)
	{
		DEBUG1("sk == NULL");
		goto out;
	}

	/* Is this really an error?? */
	if (server->data_ready == NULL)
	{
		printk(KERN_DEBUG "smb_dont_catch_keepalive: "
		       "server->data_ready == NULL\n");
		goto out;
	}
	DEBUG1("smb_dont_catch_keepalive: sk->d_r = %x, server->d_r = %x\n",
	       (unsigned int) (sk->data_ready),
	       (unsigned int) (server->data_ready));

	/*
	 * Restore the original callback atomically to avoid races ...
	 */
	data_ready = xchg(&sk->data_ready, server->data_ready);
	server->data_ready = NULL;
	if (data_ready != smb_data_ready)
	{
		printk(KERN_ERR "smb_dont_catch_keepalive: "
		       "sk->data_ready != smb_data_ready\n");
	}
	error = 0;
out:
	return error;
}

/*
 * Called with the server locked.
 */
void
smb_close_socket(struct smb_sb_info *server)
{
	struct file * file = server->sock_file;

	if (file)
	{
		VERBOSE("closing socket %p\n", server_sock(server));
#ifdef SMBFS_PARANOIA
		if (server_sock(server)->sk->data_ready == smb_data_ready)
			PARANOIA("still catching keepalives!\n");
#endif
		server->sock_file = NULL;
		fput(file);
	}
}

/*
 * Poll the server->socket to allow receives to time out.
 * returns 0 when ok to continue, <0 on errors.
 */
static int
smb_receive_poll(struct smb_sb_info *server)
{
	struct file *file = server->sock_file;
	poll_table wait_table;
	int result = 0;
	int timeout = server->mnt->timeo * HZ;
	int mask;

	for (;;) {
		poll_initwait(&wait_table);
                set_current_state(TASK_INTERRUPTIBLE);

		mask = file->f_op->poll(file, &wait_table);
		if (mask & POLLIN) {
			poll_freewait(&wait_table);
			current->state = TASK_RUNNING;
			break;
		}

		timeout = schedule_timeout(timeout);
		poll_freewait(&wait_table);
                set_current_state(TASK_RUNNING);

		if (wait_table.error) {
			result = wait_table.error;
			break;
		}

		if (signal_pending(current)) {
			/* we got a signal (which?) tell the caller to
			   try again (on all signals?). */
			DEBUG1("got signal_pending()\n");
			result = -ERESTARTSYS;
			break;
		}
		if (!timeout) {
			printk(KERN_WARNING "SMB server not responding\n");
			result = -EIO;
			break;
		}
	}
	return result;
}

static int
smb_send_raw(struct socket *socket, unsigned char *source, int length)
{
	int result;
	int already_sent = 0;

	while (already_sent < length)
	{
		result = _send(socket,
			       (void *) (source + already_sent),
			       length - already_sent);

		if (result == 0)
		{
			return -EIO;
		}
		if (result < 0)
		{
			DEBUG1("smb_send_raw: sendto error = %d\n", -result);
			return result;
		}
		already_sent += result;
	}
	return already_sent;
}

static int
smb_receive_raw(struct smb_sb_info *server, unsigned char *target, int length)
{
	int result;
	int already_read = 0;
	struct socket *socket = server_sock(server);

	while (already_read < length)
	{
		result = smb_receive_poll(server);
		if (result < 0) {
			DEBUG1("poll error = %d\n", -result);
			return result;
		}
		result = _recvfrom(socket,
				   (void *) (target + already_read),
				   length - already_read, 0);

		if (result == 0)
		{
			return -EIO;
		}
		if (result < 0)
		{
			DEBUG1("recvfrom error = %d\n", -result);
			return result;
		}
		already_read += result;
	}
	return already_read;
}

static int
smb_get_length(struct smb_sb_info *server, unsigned char *header)
{
	int result;
	unsigned char peek_buf[4];
	mm_segment_t fs;

      re_recv:
	fs = get_fs();
	set_fs(get_ds());
	result = smb_receive_raw(server, peek_buf, 4);
	set_fs(fs);

	if (result < 0)
	{
		PARANOIA("recv error = %d\n", -result);
		return result;
	}
	switch (peek_buf[0])
	{
	case 0x00:
	case 0x82:
		break;

	case 0x85:
		DEBUG1("Got SESSION KEEP ALIVE\n");
		goto re_recv;

	default:
		PARANOIA("Invalid NBT packet, code=%x\n", peek_buf[0]);
		return -EIO;
	}

	if (header != NULL)
	{
		memcpy(header, peek_buf, 4);
	}
	/* The length in the RFC NB header is the raw data length */
	return smb_len(peek_buf);
}

/*
 * Since we allocate memory in increments of PAGE_SIZE,
 * round up the packet length to the next multiple.
 */
int
smb_round_length(int len)
{
	return (len + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
}
 
/*
 * smb_receive
 * fs points to the correct segment
 */
static int
smb_receive(struct smb_sb_info *server)
{
	unsigned char * packet = server->packet;
	int len, result;
	unsigned char peek_buf[4];

	result = smb_get_length(server, peek_buf);
	if (result < 0)
		goto out;
	len = result;
	/*
	 * Some servers do not respect our max_xmit and send
	 * larger packets.  Try to allocate a new packet,
	 * but don't free the old one unless we succeed.
	 */
	if (len + 4 > server->packet_size)
	{
		int new_len = smb_round_length(len + 4);

		result = -ENOMEM;
		packet = smb_vmalloc(new_len);
		if (packet == NULL)
			goto out;
		smb_vfree(server->packet);
		server->packet = packet;
		server->packet_size = new_len;
	}
	memcpy(packet, peek_buf, 4);
	result = smb_receive_raw(server, packet + 4, len);
	if (result < 0)
	{
		VERBOSE("receive error: %d\n", result);
		goto out;
	}
	server->rcls = *(packet + smb_rcls);
	server->err  = WVAL(packet, smb_err);

#ifdef SMBFS_DEBUG_VERBOSE
	if (server->rcls != 0)
		VERBOSE("rcls=%d, err=%d\n", server->rcls, server->err);
#endif
out:
	return result;
}

/*
 * This routine checks first for "fast track" processing, as most
 * packets won't need to be copied. Otherwise, it allocates a new
 * packet to hold the incoming data.
 *
 * Note that the final server packet must be the larger of the two;
 * server packets aren't allowed to shrink.
 */
static int
smb_receive_trans2(struct smb_sb_info *server,
		   int *ldata, unsigned char **data,
		   int *lparm, unsigned char **parm)
{
	unsigned char *inbuf, *base, *rcv_buf = NULL;
	unsigned int parm_disp, parm_offset, parm_count, parm_tot, parm_len = 0;
	unsigned int data_disp, data_offset, data_count, data_tot, data_len = 0;
	unsigned int total_p = 0, total_d = 0, buf_len = 0;
	int result;

	while (1) {
		result = smb_receive(server);
		if (result < 0)
			goto out;
		inbuf = server->packet;
		if (server->rcls != 0) {
			*parm = *data = inbuf;
			*ldata = *lparm = 0;
			goto out;
		}
		/*
		 * Extract the control data from the packet.
		 */
		data_tot    = WVAL(inbuf, smb_tdrcnt);
		parm_tot    = WVAL(inbuf, smb_tprcnt);
		parm_disp   = WVAL(inbuf, smb_prdisp);
		parm_offset = WVAL(inbuf, smb_proff);
		parm_count  = WVAL(inbuf, smb_prcnt);
		data_disp   = WVAL(inbuf, smb_drdisp);
		data_offset = WVAL(inbuf, smb_droff);
		data_count  = WVAL(inbuf, smb_drcnt);
		base = smb_base(inbuf);

		/*
		 * Assume success and increment lengths.
		 */
		parm_len += parm_count;
		data_len += data_count;

		if (!rcv_buf) {
			/*
			 * Check for fast track processing ... just this packet.
			 */
			if (parm_count == parm_tot && data_count == data_tot) {
				VERBOSE("fast track, parm=%u %u %u, data=%u %u %u\n",
					parm_disp, parm_offset, parm_count,
					data_disp, data_offset, data_count);
				*parm  = base + parm_offset;
				*data  = base + data_offset;
				goto success;
			}

			/*
			 * Save the total parameter and data length.
			 */
			total_d = data_tot;
			total_p = parm_tot;

			buf_len = total_d + total_p;
			if (server->packet_size > buf_len)
				buf_len = server->packet_size;
			buf_len = smb_round_length(buf_len);
			if (buf_len > SMB_MAX_PACKET_SIZE)
				goto out_too_long;

			rcv_buf = smb_vmalloc(buf_len);
			if (!rcv_buf)
				goto out_no_mem;
			*parm = rcv_buf;
			*data = rcv_buf + total_p;
		} else if (data_tot > total_d || parm_tot > total_p)
			goto out_data_grew;

		if (parm_disp + parm_count > total_p)
			goto out_bad_parm;
		if (data_disp + data_count > total_d)
			goto out_bad_data;
		memcpy(*parm + parm_disp, base + parm_offset, parm_count);
		memcpy(*data + data_disp, base + data_offset, data_count);

		PARANOIA("copied, parm=%u of %u, data=%u of %u\n",
			 parm_len, parm_tot, data_len, data_tot);

		/*
		 * Check whether we've received all of the data. Note that
		 * we use the packet totals -- total lengths might shrink!
		 */
		if (data_len >= data_tot && parm_len >= parm_tot)
			break;
	}

	/*
	 * Install the new packet.  Note that it's possible, though
	 * unlikely, that the new packet could be smaller than the
	 * old one, in which case we just copy the data.
	 */
	inbuf = server->packet;
	if (buf_len >= server->packet_size) {
		server->packet_size = buf_len;
		server->packet = rcv_buf;
		rcv_buf = inbuf;
	} else {
		PARANOIA("copying data, old size=%d, new size=%u\n",
			 server->packet_size, buf_len);
		memcpy(inbuf, rcv_buf, parm_len + data_len);
	}

success:
	*ldata = data_len;
	*lparm = parm_len;
out:
	if (rcv_buf)
		smb_vfree(rcv_buf);
	return result;

out_no_mem:
	PARANOIA("couldn't allocate data area\n");
	result = -ENOMEM;
	goto out;
out_too_long:
	printk(KERN_ERR "smb_receive_trans2: data/param too long, data=%d, parm=%d\n",
		data_tot, parm_tot);
	goto out_error;
out_data_grew:
	printk(KERN_ERR "smb_receive_trans2: data/params grew!\n");
	goto out_error;
out_bad_parm:
	printk(KERN_ERR "smb_receive_trans2: invalid parms, disp=%d, cnt=%d, tot=%d\n",
		parm_disp, parm_count, parm_tot);
	goto out_error;
out_bad_data:
	printk(KERN_ERR "smb_receive_trans2: invalid data, disp=%d, cnt=%d, tot=%d\n",
		data_disp, data_count, data_tot);
out_error:
	result = -EIO;
	goto out;
}

/*
 * Called with the server locked
 */
int
smb_request(struct smb_sb_info *server)
{
	unsigned long flags, sigpipe;
	mm_segment_t fs;
	sigset_t old_set;
	int len, result;
	unsigned char *buffer;

	result = -EBADF;
	buffer = server->packet;
	if (!buffer)
		goto bad_no_packet;

	result = -EIO;
	if (server->state != CONN_VALID)
		goto bad_no_conn;

	if ((result = smb_dont_catch_keepalive(server)) != 0)
		goto bad_conn;

	len = smb_len(buffer) + 4;
	DEBUG1("len = %d cmd = 0x%X\n", len, buffer[8]);

	spin_lock_irqsave(&current->sigmask_lock, flags);
	sigpipe = sigismember(&current->pending.signal, SIGPIPE);
	old_set = current->blocked;
	siginitsetinv(&current->blocked, sigmask(SIGKILL)|sigmask(SIGSTOP));
	recalc_sigpending(current);
	spin_unlock_irqrestore(&current->sigmask_lock, flags);

	fs = get_fs();
	set_fs(get_ds());

	result = smb_send_raw(server_sock(server), (void *) buffer, len);
	if (result > 0)
	{
		result = smb_receive(server);
	}

	/* read/write errors are handled by errno */
	spin_lock_irqsave(&current->sigmask_lock, flags);
	if (result == -EPIPE && !sigpipe)
		sigdelset(&current->pending.signal, SIGPIPE);
	current->blocked = old_set;
	recalc_sigpending(current);
	spin_unlock_irqrestore(&current->sigmask_lock, flags);

	set_fs(fs);

	if (result >= 0)
	{
		int result2 = smb_catch_keepalive(server);
		if (result2 < 0)
		{
			printk(KERN_ERR "smb_request: catch keepalive failed\n");
			result = result2;
		}
	}
	if (result < 0)
		goto bad_conn;
	/*
	 * Check for fatal server errors ...
	 */
	if (server->rcls) {
		int error = smb_errno(server);
		if (error == -EBADSLT) {
			printk(KERN_ERR "smb_request: tree ID invalid\n");
			result = error;
			goto bad_conn;
		}
	}

out:
	DEBUG1("result = %d\n", result);
	return result;
	
bad_conn:
	PARANOIA("result %d, setting invalid\n", result);
	server->state = CONN_INVALID;
	smb_invalidate_inodes(server);
	goto out;		
bad_no_packet:
	printk(KERN_ERR "smb_request: no packet!\n");
	goto out;
bad_no_conn:
	printk(KERN_ERR "smb_request: connection %d not valid!\n",
	       server->state);
	goto out;
}

#define ROUND_UP(x) (((x)+3) & ~3)
static int
smb_send_trans2(struct smb_sb_info *server, __u16 trans2_command,
		int ldata, unsigned char *data,
		int lparam, unsigned char *param)
{
	struct socket *sock = server_sock(server);
	struct scm_cookie scm;
	int err;
	int mparam, mdata;

	/* I know the following is very ugly, but I want to build the
	   smb packet as efficiently as possible. */

	const int smb_parameters = 15;
	const int oparam =
		ROUND_UP(SMB_HEADER_LEN + 2 * smb_parameters + 2 + 3);
	const int odata =
		ROUND_UP(oparam + lparam);
	const int bcc =
		odata + ldata - (SMB_HEADER_LEN + 2 * smb_parameters + 2);
	const int packet_length =
		SMB_HEADER_LEN + 2 * smb_parameters + bcc + 2;

	unsigned char padding[4] =
	{0,};
	char *p;

	struct iovec iov[4];
	struct msghdr msg;

	/* FIXME! this test needs to include SMB overhead too, I think ... */
	if ((bcc + oparam) > server->opt.max_xmit)
		return -ENOMEM;
	p = smb_setup_header(server, SMBtrans2, smb_parameters, bcc);

	/*
	 * max parameters + max data + max setup == max_xmit to make NT4 happy
	 * and not abort the transfer or split into multiple responses.
	 *
	 * -100 is to make room for headers, which OS/2 seems to include in the
	 * size calculation while NT4 does not?
	 */
	mparam = SMB_TRANS2_MAX_PARAM;
	mdata = server->opt.max_xmit - mparam - 100;
	if (mdata < 1024) {
		mdata = 1024;
		mparam = 20;
	}

	WSET(server->packet, smb_tpscnt, lparam);
	WSET(server->packet, smb_tdscnt, ldata);
	WSET(server->packet, smb_mprcnt, mparam);
	WSET(server->packet, smb_mdrcnt, mdata);
	WSET(server->packet, smb_msrcnt, 0);    /* max setup always 0 ? */
	WSET(server->packet, smb_flags, 0);
	DSET(server->packet, smb_timeout, 0);
	WSET(server->packet, smb_pscnt, lparam);
	WSET(server->packet, smb_psoff, oparam - 4);
	WSET(server->packet, smb_dscnt, ldata);
	WSET(server->packet, smb_dsoff, odata - 4);
	WSET(server->packet, smb_suwcnt, 1);
	WSET(server->packet, smb_setup0, trans2_command);
	*p++ = 0;		/* null smb_name for trans2 */
	*p++ = 'D';		/* this was added because OS/2 does it */
	*p++ = ' ';


	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	msg.msg_iov = iov;
	msg.msg_iovlen = 4;
	msg.msg_flags = 0;
	
	iov[0].iov_base = (void *) server->packet;
	iov[0].iov_len = oparam;
	iov[1].iov_base = (param == NULL) ? padding : param;
	iov[1].iov_len = lparam;
	iov[2].iov_base = padding;
	iov[2].iov_len = odata - oparam - lparam;
	iov[3].iov_base = (data == NULL) ? padding : data;
	iov[3].iov_len = ldata;

	err = scm_send(sock, &msg, &scm);
        if (err >= 0) {
		err = sock->ops->sendmsg(sock, &msg, packet_length, &scm);
		scm_destroy(&scm);
	}
	return err;
}

/*
 * This is not really a trans2 request, we assume that you only have
 * one packet to send.
 */
int
smb_trans2_request(struct smb_sb_info *server, __u16 trans2_command,
		   int ldata, unsigned char *data,
		   int lparam, unsigned char *param,
		   int *lrdata, unsigned char **rdata,
		   int *lrparam, unsigned char **rparam)
{
	sigset_t old_set;
	unsigned long flags, sigpipe;
	mm_segment_t fs;
	int result;

	DEBUG1("com=%d, ld=%d, lp=%d\n", trans2_command, ldata, lparam);

	/*
	 * These are initialized in smb_request_ok, but not here??
	 */
	server->rcls = 0;
	server->err = 0;

	result = -EIO;
	if (server->state != CONN_VALID)
		goto out;

	if ((result = smb_dont_catch_keepalive(server)) != 0)
		goto bad_conn;

	spin_lock_irqsave(&current->sigmask_lock, flags);
	sigpipe = sigismember(&current->pending.signal, SIGPIPE);
	old_set = current->blocked;
	siginitsetinv(&current->blocked, sigmask(SIGKILL)|sigmask(SIGSTOP));
	recalc_sigpending(current);
	spin_unlock_irqrestore(&current->sigmask_lock, flags);

	fs = get_fs();
	set_fs(get_ds());

	result = smb_send_trans2(server, trans2_command,
				 ldata, data, lparam, param);
	if (result >= 0)
	{
		result = smb_receive_trans2(server,
					    lrdata, rdata, lrparam, rparam);
	}

	/* read/write errors are handled by errno */
	spin_lock_irqsave(&current->sigmask_lock, flags);
	if (result == -EPIPE && !sigpipe)
		sigdelset(&current->pending.signal, SIGPIPE);
	current->blocked = old_set;
	recalc_sigpending(current);
	spin_unlock_irqrestore(&current->sigmask_lock, flags);

	set_fs(fs);

	if (result >= 0)
	{
		int result2 = smb_catch_keepalive(server);
		if (result2 < 0)
		{
			result = result2;
		}
	}
	if (result < 0)
		goto bad_conn;
	/*
	 * Check for fatal server errors ...
	 */
	if (server->rcls) {
		int error = smb_errno(server);
		if (error == -EBADSLT) {
			printk(KERN_ERR "smb_request: tree ID invalid\n");
			result = error;
			goto bad_conn;
		}
	}

out:
	return result;

bad_conn:
	PARANOIA("result=%d, setting invalid\n", result);
	server->state = CONN_INVALID;
	smb_invalidate_inodes(server);
	goto out;
}
