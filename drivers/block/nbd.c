/*
 * Network block device - make block devices work over TCP
 *
 * Note that you can not swap over this thing, yet. Seems to work but
 * deadlocks sometimes - you can not swap over TCP in general.
 * 
 * Copyright 1997-2000 Pavel Machek <pavel@ucw.cz>
 * Parts copyright 2001 Steven Whitehouse <steve@chygwyn.com>
 *
 * (part of code stolen from loop.c)
 *
 * 97-3-25 compiled 0-th version, not yet tested it 
 *   (it did not work, BTW) (later that day) HEY! it works!
 *   (bit later) hmm, not that much... 2:00am next day:
 *   yes, it works, but it gives something like 50kB/sec
 * 97-4-01 complete rewrite to make it possible for many requests at 
 *   once to be processed
 * 97-4-11 Making protocol independent of endianity etc.
 * 97-9-13 Cosmetic changes
 * 98-5-13 Attempt to make 64-bit-clean on 64-bit machines
 * 99-1-11 Attempt to make 64-bit-clean on 32-bit machines <ankry@mif.pg.gda.pl>
 * 01-2-27 Fix to store proper blockcount for kernel (calculated using
 *   BLOCK_SIZE_BITS, not device blocksize) <aga@permonline.ru>
 * 01-3-11 Make nbd work with new Linux block layer code. It now supports
 *   plugging like all the other block devices. Also added in MSG_MORE to
 *   reduce number of partial TCP segments sent. <steve@chygwyn.com>
 * 01-12-6 Fix deadlock condition by making queue locks independant of
 *   the transmit lock. <steve@chygwyn.com>
 * 02-10-11 Allow hung xmit to be aborted via SIGKILL & various fixes.
 *   <Paul.Clements@SteelEye.com> <James.Bottomley@SteelEye.com>
 *
 * possible FIXME: make set_sock / set_blksize / set_size / do_it one syscall
 * why not: would need verify_area and friends, would share yet another 
 *          structure with userland
 */

#define PARANOIA
#include <linux/major.h>

#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/stat.h>
#include <linux/errno.h>
#include <linux/file.h>
#include <linux/ioctl.h>
#include <net/sock.h>

#include <linux/devfs_fs_kernel.h>

#include <asm/uaccess.h>
#include <asm/types.h>

#define MAJOR_NR NBD_MAJOR
#include <linux/nbd.h>

#define LO_MAGIC 0x68797548

static int nbd_blksizes[MAX_NBD];
static int nbd_blksize_bits[MAX_NBD];
static int nbd_sizes[MAX_NBD];
static u64 nbd_bytesizes[MAX_NBD];

static struct nbd_device nbd_dev[MAX_NBD];
static devfs_handle_t devfs_handle;

#define DEBUG( s )
/* #define DEBUG( s ) printk( s ) 
 */

#ifdef PARANOIA
static int requests_in;
static int requests_out;
#endif

static int nbd_open(struct inode *inode, struct file *file)
{
	int dev;

	if (!inode)
		return -EINVAL;
	dev = MINOR(inode->i_rdev);
	if (dev >= MAX_NBD)
		return -ENODEV;

	nbd_dev[dev].refcnt++;
	return 0;
}

/*
 *  Send or receive packet.
 */
static int nbd_xmit(int send, struct socket *sock, char *buf, int size, int msg_flags)
{
	mm_segment_t oldfs;
	int result;
	struct msghdr msg;
	struct iovec iov;
	unsigned long flags;
	sigset_t oldset;

	oldfs = get_fs();
	set_fs(get_ds());

	/* Allow interception of SIGKILL only
	 * Don't allow other signals to interrupt the transmission */
	spin_lock_irqsave(&current->sigmask_lock, flags);
	oldset = current->blocked;
	sigfillset(&current->blocked);
	sigdelsetmask(&current->blocked, sigmask(SIGKILL));
	recalc_sigpending(current);
	spin_unlock_irqrestore(&current->sigmask_lock, flags);


	do {
		sock->sk->allocation = GFP_NOIO;
		iov.iov_base = buf;
		iov.iov_len = size;
		msg.msg_name = NULL;
		msg.msg_namelen = 0;
		msg.msg_iov = &iov;
		msg.msg_iovlen = 1;
		msg.msg_control = NULL;
		msg.msg_controllen = 0;
		msg.msg_namelen = 0;
		msg.msg_flags = msg_flags | MSG_NOSIGNAL;

		if (send)
			result = sock_sendmsg(sock, &msg, size);
		else
			result = sock_recvmsg(sock, &msg, size, 0);

		if (signal_pending(current)) {
			siginfo_t info;
			spin_lock_irqsave(&current->sigmask_lock, flags);
			printk(KERN_WARNING "NBD (pid %d: %s) got signal %d\n",
				current->pid, current->comm, 
				dequeue_signal(&current->blocked, &info));
			spin_unlock_irqrestore(&current->sigmask_lock, flags);
			result = -EINTR;
			break;
		}

		if (result <= 0) {
#ifdef PARANOIA
			printk(KERN_ERR "NBD: %s - sock=%ld at buf=%ld, size=%d returned %d.\n",
			       send ? "send" : "receive", (long) sock, (long) buf, size, result);
#endif
			break;
		}
		size -= result;
		buf += result;
	} while (size > 0);

	spin_lock_irqsave(&current->sigmask_lock, flags);
	current->blocked = oldset;
	recalc_sigpending(current);
	spin_unlock_irqrestore(&current->sigmask_lock, flags);

	set_fs(oldfs);
	return result;
}

#define FAIL( s ) { printk( KERN_ERR "NBD: " s "(result %d)\n", result ); goto error_out; }

void nbd_send_req(struct nbd_device *lo, struct request *req)
{
	int result = -1;
	struct nbd_request request;
	unsigned long size = req->nr_sectors << 9;
	struct socket *sock = lo->sock;

	DEBUG("NBD: sending control, ");
	request.magic = htonl(NBD_REQUEST_MAGIC);
	request.type = htonl(req->cmd);
	request.from = cpu_to_be64( (u64) req->sector << 9);
	request.len = htonl(size);
	memcpy(request.handle, &req, sizeof(req));

	down(&lo->tx_lock);

	if (!sock || !lo->sock) {
		FAIL("Attempted sendmsg to closed socket\n");
	}

	result = nbd_xmit(1, sock, (char *) &request, sizeof(request), req->cmd == WRITE ? MSG_MORE : 0);
	if (result <= 0)
		FAIL("Sendmsg failed for control.");

	if (req->cmd == WRITE) {
		struct buffer_head *bh = req->bh;
		DEBUG("data, ");
		do {
			result = nbd_xmit(1, sock, bh->b_data, bh->b_size, bh->b_reqnext == NULL ? 0 : MSG_MORE);
			if (result <= 0)
				FAIL("Send data failed.");
			bh = bh->b_reqnext;
		} while(bh);
	}
	up(&lo->tx_lock);
	return;

error_out:
	up(&lo->tx_lock);
	req->errors++;
}

static struct request *nbd_find_request(struct nbd_device *lo, char *handle)
{
	struct request *req;
	struct list_head *tmp;
	struct request *xreq;

	memcpy(&xreq, handle, sizeof(xreq));

	spin_lock(&lo->queue_lock);
	list_for_each(tmp, &lo->queue_head) {
		req = list_entry(tmp, struct request, queue);
		if (req != xreq)
			continue;
		list_del(&req->queue);
		spin_unlock(&lo->queue_lock);
		return req;
	}
	spin_unlock(&lo->queue_lock);
	return NULL;
}

#define HARDFAIL( s ) { printk( KERN_ERR "NBD: " s "(result %d)\n", result ); lo->harderror = result; return NULL; }
struct request *nbd_read_stat(struct nbd_device *lo)
		/* NULL returned = something went wrong, inform userspace       */ 
{
	int result;
	struct nbd_reply reply;
	struct request *req;

	DEBUG("reading control, ");
	reply.magic = 0;
	result = nbd_xmit(0, lo->sock, (char *) &reply, sizeof(reply), MSG_WAITALL);
	if (result <= 0)
		HARDFAIL("Recv control failed.");
	req = nbd_find_request(lo, reply.handle);
	if (req == NULL)
		HARDFAIL("Unexpected reply");

	DEBUG("ok, ");
	if (ntohl(reply.magic) != NBD_REPLY_MAGIC)
		HARDFAIL("Not enough magic.");
	if (ntohl(reply.error))
		FAIL("Other side returned error.");
	if (req->cmd == READ) {
		struct buffer_head *bh = req->bh;
		DEBUG("data, ");
		do {
			result = nbd_xmit(0, lo->sock, bh->b_data, bh->b_size, MSG_WAITALL);
			if (result <= 0)
				HARDFAIL("Recv data failed.");
			bh = bh->b_reqnext;
		} while(bh);
	}
	DEBUG("done.\n");
	return req;

/* Can we get here? Yes, if other side returns error */
      error_out:
	req->errors++;
	return req;
}

void nbd_do_it(struct nbd_device *lo)
{
	struct request *req;

	while (1) {
		req = nbd_read_stat(lo);

		if (!req) {
			printk(KERN_ALERT "req should never be null\n" );
			goto out;
		}
#ifdef PARANOIA
		if (lo != &nbd_dev[MINOR(req->rq_dev)]) {
			printk(KERN_ALERT "NBD: request corrupted!\n");
			continue;
		}
		if (lo->magic != LO_MAGIC) {
			printk(KERN_ALERT "NBD: nbd_dev[] corrupted: Not enough magic\n");
			goto out;
		}
#endif
		
		nbd_end_request(req);

	}
 out:
	return;
}

void nbd_clear_que(struct nbd_device *lo)
{
	struct request *req;

#ifdef PARANOIA
	if (lo->magic != LO_MAGIC) {
		printk(KERN_ERR "NBD: nbd_dev[] corrupted: Not enough magic when clearing!\n");
		return;
	}
#endif
	do {
		req = NULL;
		spin_lock(&lo->queue_lock);
		if (!list_empty(&lo->queue_head)) {
			req = list_entry(lo->queue_head.next, struct request, queue);
			list_del(&req->queue);
		}
		spin_unlock(&lo->queue_lock);
		if (req) {
			req->errors++;
			nbd_end_request(req);
		}
	} while(req);

}

/*
 * We always wait for result of write, for now. It would be nice to make it optional
 * in future
 * if ((req->cmd == WRITE) && (lo->flags & NBD_WRITE_NOCHK)) 
 *   { printk( "Warning: Ignoring result!\n"); nbd_end_request( req ); }
 */

#undef FAIL
#define FAIL( s ) { printk( KERN_ERR "NBD, minor %d: " s "\n", dev ); goto error_out; }

static void do_nbd_request(request_queue_t * q)
{
	struct request *req;
	int dev = 0;
	struct nbd_device *lo;

	while (!QUEUE_EMPTY) {
		req = CURRENT;
#ifdef PARANOIA
		if (!req)
			FAIL("que not empty but no request?");
#endif
		dev = MINOR(req->rq_dev);
#ifdef PARANOIA
		if (dev >= MAX_NBD)
			FAIL("Minor too big.");		/* Probably can not happen */
#endif
		lo = &nbd_dev[dev];
		if (!lo->file)
			FAIL("Request when not-ready.");
		if ((req->cmd == WRITE) && (lo->flags & NBD_READ_ONLY))
			FAIL("Write on read-only");
#ifdef PARANOIA
		if (lo->magic != LO_MAGIC)
			FAIL("nbd[] is not magical!");
		requests_in++;
#endif
		req->errors = 0;
		blkdev_dequeue_request(req);
		spin_unlock_irq(&io_request_lock);

		spin_lock(&lo->queue_lock);
		if (!lo->file) {
			spin_unlock(&lo->queue_lock);
			printk(KERN_ERR "nbd: failed between accept and semaphore, file lost\n");
			req->errors++;
			nbd_end_request(req);
			spin_lock_irq(&io_request_lock);
			continue;
		}

		list_add_tail(&req->queue, &lo->queue_head);
		spin_unlock(&lo->queue_lock);

		nbd_send_req(lo, req);
		if (req->errors) {
			printk(KERN_ERR "nbd: nbd_send_req failed\n");
			spin_lock(&lo->queue_lock);
			list_del(&req->queue);
			spin_unlock(&lo->queue_lock);
			nbd_end_request(req);
			spin_lock_irq(&io_request_lock);
			continue;
		}

		spin_lock_irq(&io_request_lock);
		continue;

	      error_out:
		req->errors++;
		blkdev_dequeue_request(req);
		spin_unlock(&io_request_lock);
		nbd_end_request(req);
		spin_lock(&io_request_lock);
	}
	return;
}

static int nbd_ioctl(struct inode *inode, struct file *file,
		     unsigned int cmd, unsigned long arg)
{
	struct nbd_device *lo;
	int dev, error, temp;
	struct request sreq ;

	/* Anyone capable of this syscall can do *real bad* things */

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	if (!inode)
		return -EINVAL;
	dev = MINOR(inode->i_rdev);
	if (dev >= MAX_NBD)
		return -ENODEV;

	lo = &nbd_dev[dev];
	switch (cmd) {
	case NBD_DISCONNECT:
	        printk("NBD_DISCONNECT\n");
                sreq.cmd=2 ; /* shutdown command */
                if (!lo->sock) return -EINVAL;
                nbd_send_req(lo, &sreq);
                return 0 ;
 
	case NBD_CLEAR_SOCK:
		error = 0;
		down(&lo->tx_lock);
		lo->sock = NULL;
		up(&lo->tx_lock);
		spin_lock(&lo->queue_lock);
		file = lo->file;
		lo->file = NULL;
		spin_unlock(&lo->queue_lock);
		nbd_clear_que(lo);
		spin_lock(&lo->queue_lock);
		if (!list_empty(&lo->queue_head)) {
			printk(KERN_ERR "nbd: disconnect: some requests are in progress -> please try again.\n");
			error = -EBUSY;
		}
		spin_unlock(&lo->queue_lock);
		if (file)
			fput(file);
		return error;
	case NBD_SET_SOCK:
		if (lo->file)
			return -EBUSY;
		error = -EINVAL;
		file = fget(arg);
		if (file) {
			inode = file->f_dentry->d_inode;
			/* N.B. Should verify that it's a socket */
			lo->file = file;
			lo->sock = &inode->u.socket_i;
			error = 0;
		}
		return error;
	case NBD_SET_BLKSIZE:
		if ((arg & (arg-1)) || (arg < 512) || (arg > PAGE_SIZE))
			return -EINVAL;
		nbd_blksizes[dev] = arg;
		temp = arg >> 9;
		nbd_blksize_bits[dev] = 9;
		while (temp > 1) {
			nbd_blksize_bits[dev]++;
			temp >>= 1;
		}
		nbd_bytesizes[dev] &= ~(nbd_blksizes[dev]-1); 
		nbd_sizes[dev] = nbd_bytesizes[dev] >> BLOCK_SIZE_BITS;
		return 0;
	case NBD_SET_SIZE:
		nbd_bytesizes[dev] = arg & ~(nbd_blksizes[dev]-1); 
		nbd_sizes[dev] = nbd_bytesizes[dev] >> BLOCK_SIZE_BITS;
		return 0;
	case NBD_SET_SIZE_BLOCKS:
		nbd_bytesizes[dev] = ((u64) arg) << nbd_blksize_bits[dev]; 
		nbd_sizes[dev] = nbd_bytesizes[dev] >> BLOCK_SIZE_BITS;
		return 0;
	case NBD_DO_IT:
		if (!lo->file)
			return -EINVAL;
		nbd_do_it(lo);
		/* on return tidy up in case we have a signal */
		/* Forcibly shutdown the socket causing all listeners
		 * to error
		 *
		 * FIXME: This code is duplicated from sys_shutdown, but
		 * there should be a more generic interface rather than
		 * calling socket ops directly here */
		down(&lo->tx_lock);
		if (lo->sock) {
			printk(KERN_WARNING "nbd: shutting down socket\n");
			lo->sock->ops->shutdown(lo->sock,
				SEND_SHUTDOWN|RCV_SHUTDOWN);
			lo->sock = NULL;
		}
		up(&lo->tx_lock);
		spin_lock(&lo->queue_lock);
		file = lo->file;
		lo->file = NULL;
		spin_unlock(&lo->queue_lock);
		nbd_clear_que(lo);
		printk(KERN_WARNING "nbd: queue cleared\n");
		if (file)
			fput(file);
		return lo->harderror;
	case NBD_CLEAR_QUE:
		down(&lo->tx_lock);
		if (lo->sock) {
			up(&lo->tx_lock);
			return 0; /* probably should be error, but that would
				   * break "nbd-client -d", so just return 0 */
		}
		up(&lo->tx_lock);
		nbd_clear_que(lo);
		return 0;
#ifdef PARANOIA
	case NBD_PRINT_DEBUG:
		printk(KERN_INFO "NBD device %d: next = %p, prev = %p. Global: in %d, out %d\n",
		       dev, lo->queue_head.next, lo->queue_head.prev, requests_in, requests_out);
		return 0;
#endif
	case BLKGETSIZE:
		return put_user(nbd_bytesizes[dev] >> 9, (unsigned long *) arg);
	case BLKGETSIZE64:
		return put_user((u64)nbd_bytesizes[dev], (u64 *) arg);
	}
	return -EINVAL;
}

static int nbd_release(struct inode *inode, struct file *file)
{
	struct nbd_device *lo;
	int dev;

	if (!inode)
		return -ENODEV;
	dev = MINOR(inode->i_rdev);
	if (dev >= MAX_NBD)
		return -ENODEV;
	lo = &nbd_dev[dev];
	if (lo->refcnt <= 0)
		printk(KERN_ALERT "nbd_release: refcount(%d) <= 0\n", lo->refcnt);
	lo->refcnt--;
	/* N.B. Doesn't lo->file need an fput?? */
	return 0;
}

static struct block_device_operations nbd_fops =
{
	owner:		THIS_MODULE,
	open:		nbd_open,
	release:	nbd_release,
	ioctl:		nbd_ioctl,
};

/*
 * And here should be modules and kernel interface 
 *  (Just smiley confuses emacs :-)
 */

static int __init nbd_init(void)
{
	int i;

	if (sizeof(struct nbd_request) != 28) {
		printk(KERN_CRIT "Sizeof nbd_request needs to be 28 in order to work!\n" );
		return -EIO;
	}

	if (register_blkdev(MAJOR_NR, "nbd", &nbd_fops)) {
		printk("Unable to get major number %d for NBD\n",
		       MAJOR_NR);
		return -EIO;
	}
#ifdef MODULE
	printk("nbd: registered device at major %d\n", MAJOR_NR);
#endif
	blksize_size[MAJOR_NR] = nbd_blksizes;
	blk_size[MAJOR_NR] = nbd_sizes;
	blk_init_queue(BLK_DEFAULT_QUEUE(MAJOR_NR), do_nbd_request);
	blk_queue_headactive(BLK_DEFAULT_QUEUE(MAJOR_NR), 0);
	for (i = 0; i < MAX_NBD; i++) {
		nbd_dev[i].refcnt = 0;
		nbd_dev[i].file = NULL;
		nbd_dev[i].magic = LO_MAGIC;
		nbd_dev[i].flags = 0;
		spin_lock_init(&nbd_dev[i].queue_lock);
		INIT_LIST_HEAD(&nbd_dev[i].queue_head);
		init_MUTEX(&nbd_dev[i].tx_lock);
		nbd_blksizes[i] = 1024;
		nbd_blksize_bits[i] = 10;
		nbd_bytesizes[i] = ((u64)0x7ffffc00) << 10; /* 2TB */
		nbd_sizes[i] = nbd_bytesizes[i] >> BLOCK_SIZE_BITS;
		register_disk(NULL, MKDEV(MAJOR_NR,i), 1, &nbd_fops,
				nbd_bytesizes[i]>>9);
	}
	devfs_handle = devfs_mk_dir (NULL, "nbd", NULL);
	devfs_register_series (devfs_handle, "%u", MAX_NBD,
			       DEVFS_FL_DEFAULT, MAJOR_NR, 0,
			       S_IFBLK | S_IRUSR | S_IWUSR,
			       &nbd_fops, NULL);

	return 0;
}

static void __exit nbd_cleanup(void)
{
	devfs_unregister (devfs_handle);
	blk_cleanup_queue(BLK_DEFAULT_QUEUE(MAJOR_NR));

	if (unregister_blkdev(MAJOR_NR, "nbd") != 0)
		printk("nbd: cleanup_module failed\n");
	else
		printk("nbd: module cleaned up.\n");
}

module_init(nbd_init);
module_exit(nbd_cleanup);

MODULE_DESCRIPTION("Network Block Device");
MODULE_LICENSE("GPL");


