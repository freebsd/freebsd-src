/*
 * 1999 Copyright (C) Pavel Machek, pavel@ucw.cz. This code is GPL.
 * 1999/11/04 Copyright (C) 1999 VMware, Inc. (Regis "HPReg" Duchesne)
 *            Made nbd_end_request() use the io_request_lock
 * 2001 Copyright (C) Steven Whitehouse
 *            New nbd_end_request() for compatibility with new linux block
 *            layer code.
 */

#ifndef LINUX_NBD_H
#define LINUX_NBD_H

#define NBD_SET_SOCK	_IO( 0xab, 0 )
#define NBD_SET_BLKSIZE	_IO( 0xab, 1 )
#define NBD_SET_SIZE	_IO( 0xab, 2 )
#define NBD_DO_IT	_IO( 0xab, 3 )
#define NBD_CLEAR_SOCK	_IO( 0xab, 4 )
#define NBD_CLEAR_QUE	_IO( 0xab, 5 )
#define NBD_PRINT_DEBUG	_IO( 0xab, 6 )
#define NBD_SET_SIZE_BLOCKS	_IO( 0xab, 7 )
#define NBD_DISCONNECT  _IO( 0xab, 8 )

#ifdef MAJOR_NR

#include <linux/locks.h>
#include <asm/semaphore.h>

#define LOCAL_END_REQUEST

#include <linux/blk.h>

#ifdef PARANOIA
extern int requests_in;
extern int requests_out;
#endif

static void
nbd_end_request(struct request *req)
{
	struct buffer_head *bh;
	unsigned nsect;
	unsigned long flags;
	int uptodate = (req->errors == 0) ? 1 : 0;

#ifdef PARANOIA
	requests_out++;
#endif
	spin_lock_irqsave(&io_request_lock, flags);
	while((bh = req->bh) != NULL) {
		nsect = bh->b_size >> 9;
		blk_finished_io(nsect);
		req->bh = bh->b_reqnext;
		bh->b_reqnext = NULL;
		bh->b_end_io(bh, uptodate);
	}
	blkdev_release_request(req);
	spin_unlock_irqrestore(&io_request_lock, flags);
}

#define MAX_NBD 128

struct nbd_device {
	int refcnt;	
	int flags;
	int harderror;		/* Code of hard error			*/
#define NBD_READ_ONLY 0x0001
#define NBD_WRITE_NOCHK 0x0002
	struct socket * sock;
	struct file * file; 		/* If == NULL, device is not ready, yet	*/
	int magic;			/* FIXME: not if debugging is off	*/
	spinlock_t queue_lock;
	struct list_head queue_head;	/* Requests are added here...			*/
	struct semaphore tx_lock;
};
#endif

/* This now IS in some kind of include file...	*/

/* These are send over network in request/reply magic field */

#define NBD_REQUEST_MAGIC 0x25609513
#define NBD_REPLY_MAGIC 0x67446698
/* Do *not* use magics: 0x12560953 0x96744668. */

/*
 * This is packet used for communication between client and
 * server. All data are in network byte order.
 */
struct nbd_request {
	u32 magic;
	u32 type;	/* == READ || == WRITE 	*/
	char handle[8];
	u64 from;
	u32 len;
}
#ifdef __GNUC__
	__attribute__ ((packed))
#endif
;

struct nbd_reply {
	u32 magic;
	u32 error;		/* 0 = ok, else error	*/
	char handle[8];		/* handle you got from request	*/
};
#endif
