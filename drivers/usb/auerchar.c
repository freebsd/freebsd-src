/*****************************************************************************/
/*
 *      auerchar.c  --  Auerswald PBX/System Telephone character interface.
 *
 *      Copyright (C) 2002  Wolfgang Mües (wolfgang@iksw-muees.de)
 *
 *      Very much code of this driver is borrowed from dabusb.c (Deti Fliegl)
 *      and from the USB Skeleton driver (Greg Kroah-Hartman). Thank you.
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
 /*****************************************************************************/

#undef DEBUG			/* include debug macros until it's done */
#include <linux/usb.h>
#include "auerchar.h"
#include "auermain.h"
#include <linux/slab.h>
#include <asm/uaccess.h>	/* user area access functions */

/*-------------------------------------------------------------------*/

/* wake up waiting readers */
static void auerchar_disconnect(struct auerscon *scp)
{
	struct auerchar *ccp =((struct auerchar *) ((char *) (scp) - (unsigned long) (&((struct auerchar *) 0)->scontext)));
	dbg("auerchar_disconnect called");
	ccp->removed = 1;
	wake_up(&ccp->readwait);
}


/* dispatch a read paket to a waiting character device */
static void auerchar_ctrlread_dispatch(struct auerscon *scp,
				       struct auerbuf *bp)
{
	unsigned long flags;
	struct auerchar *ccp;
	struct auerbuf *newbp = NULL;
	char *charp;
	dbg("auerchar_ctrlread_dispatch called");
	ccp =((struct auerchar *) ((char *) (scp) - (unsigned long)(&((struct auerchar *) 0)->scontext)));

	/* get a read buffer from character device context */
	newbp = auerbuf_getbuf(&ccp->bufctl);
	if (!newbp) {
		dbg("No read buffer available, discard paket!");
		return;		/* no buffer, no dispatch */
	}

	/* copy information to new buffer element
	   (all buffers have the same length) */
	charp = newbp->bufp;
	newbp->bufp = bp->bufp;
	bp->bufp = charp;
	newbp->len = bp->len;

	/* insert new buffer in read list */
	spin_lock_irqsave(&ccp->bufctl.lock, flags);
	list_add_tail(&newbp->buff_list, &ccp->bufctl.rec_buff_list);
	spin_unlock_irqrestore(&ccp->bufctl.lock, flags);
	dbg("read buffer appended to rec_list");

	/* wake up pending synchronous reads */
	wake_up(&ccp->readwait);
}


/* Delete an auerswald character context */
void auerchar_delete(struct auerchar *ccp)
{
	dbg("auerchar_delete");
	if (ccp == NULL)
		return;

	/* wake up pending synchronous reads */
	ccp->removed = 1;
	wake_up(&ccp->readwait);

	/* remove the read buffer */
	if (ccp->readbuf) {
		auerbuf_releasebuf(ccp->readbuf);
		ccp->readbuf = NULL;
	}

	/* remove the character buffers */
	auerbuf_free_buffers(&ccp->bufctl);

	/* release the memory */
	kfree(ccp);
}


/* --------------------------------------------------------------------- */
/* Char device functions                                                 */

/* Open a new character device */
int auerchar_open(struct inode *inode, struct file *file)
{
	int dtindex = MINOR(inode->i_rdev) - AUER_MINOR_BASE;
	struct auerswald *cp = NULL;
	struct auerchar *ccp = NULL;
	int ret;

	/* minor number in range? */
	if ((dtindex < 0) || (dtindex >= AUER_MAX_DEVICES)) {
		return -ENODEV;
	}
	/* usb device available? */
	if (down_interruptible(&auerdev_table_mutex)) {
		return -ERESTARTSYS;
	}
	cp = auerdev_table[dtindex];
	if (cp == NULL) {
		up(&auerdev_table_mutex);
		return -ENODEV;
	}
	if (down_interruptible(&cp->mutex)) {
		up(&auerdev_table_mutex);
		return -ERESTARTSYS;
	}
	up(&auerdev_table_mutex);

	/* we have access to the device. Now lets allocate memory */
	ccp = (struct auerchar *) kmalloc(sizeof(struct auerchar), GFP_KERNEL);
	if (ccp == NULL) {
		err("out of memory");
		ret = -ENOMEM;
		goto ofail;
	}

	/* Initialize device descriptor */
	memset(ccp, 0, sizeof(struct auerchar));
	init_MUTEX(&ccp->mutex);
	init_MUTEX(&ccp->readmutex);
	auerbuf_init(&ccp->bufctl);
	ccp->scontext.id = AUH_UNASSIGNED;
	ccp->scontext.dispatch = auerchar_ctrlread_dispatch;
	ccp->scontext.disconnect = auerchar_disconnect;
	init_waitqueue_head(&ccp->readwait);

	ret =
	    auerbuf_setup(&ccp->bufctl, AU_RBUFFERS,
			  cp->maxControlLength + AUH_SIZE);
	if (ret) {
		goto ofail;
	}

	cp->open_count++;
	ccp->auerdev = cp;
	dbg("open %s as /dev/usb/%s", cp->dev_desc, cp->name);
	up(&cp->mutex);

	/* file IO stuff */
	file->f_pos = 0;
	file->private_data = ccp;
	return 0;

	/* Error exit */
      ofail:up(&cp->mutex);
	auerchar_delete(ccp);
	return ret;
}


/* IOCTL functions */
int auerchar_ioctl(struct inode *inode, struct file *file,
		   unsigned int cmd, unsigned long arg)
{
	struct auerchar *ccp = (struct auerchar *) file->private_data;
	int ret = 0;
	struct audevinfo devinfo;
	struct auerswald *cp = NULL;
	unsigned int u;
	dbg("ioctl");

	/* get the mutexes */
	if (down_interruptible(&ccp->mutex)) {
		return -ERESTARTSYS;
	}
	cp = ccp->auerdev;
	if (!cp) {
		up(&ccp->mutex);
		return -ENODEV;
	}
	if (down_interruptible(&cp->mutex)) {
		up(&ccp->mutex);
		return -ERESTARTSYS;
	}

	/* Check for removal */
	if (!cp->usbdev) {
		up(&cp->mutex);
		up(&ccp->mutex);
		return -ENODEV;
	}

	switch (cmd) {

		/* return != 0 if Transmitt channel ready to send */
	case IOCTL_AU_TXREADY:
		dbg("IOCTL_AU_TXREADY");
		u = ccp->auerdev && (ccp->scontext.id != AUH_UNASSIGNED)
		    && !list_empty(&cp->bufctl.free_buff_list);
		ret = put_user(u, (unsigned int *) arg);
		break;

		/* return != 0 if connected to a service channel */
	case IOCTL_AU_CONNECT:
		dbg("IOCTL_AU_CONNECT");
		u = (ccp->scontext.id != AUH_UNASSIGNED);
		ret = put_user(u, (unsigned int *) arg);
		break;

		/* return != 0 if Receive Data available */
	case IOCTL_AU_RXAVAIL:
		dbg("IOCTL_AU_RXAVAIL");
		if (ccp->scontext.id == AUH_UNASSIGNED) {
			ret = -EIO;
			break;
		}
		u = 0;		/* no data */
		if (ccp->readbuf) {
			int restlen = ccp->readbuf->len - ccp->readoffset;
			if (restlen > 0)
				u = 1;
		}
		if (!u) {
			if (!list_empty(&ccp->bufctl.rec_buff_list)) {
				u = 1;
			}
		}
		ret = put_user(u, (unsigned int *) arg);
		break;

		/* return the max. buffer length for the device */
	case IOCTL_AU_BUFLEN:
		dbg("IOCTL_AU_BUFLEN");
		u = cp->maxControlLength;
		ret = put_user(u, (unsigned int *) arg);
		break;

		/* requesting a service channel */
	case IOCTL_AU_SERVREQ:
		dbg("IOCTL_AU_SERVREQ");
		/* requesting a service means: release the previous one first */
		auerswald_removeservice(cp, &ccp->scontext);
		/* get the channel number */
		ret = get_user(u, (unsigned int *) arg);
		if (ret) {
			break;
		}
		if ((u < AUH_FIRSTUSERCH) || (u >= AUH_TYPESIZE)) {
			ret = -EIO;
			break;
		}
		dbg("auerchar service request parameters are ok");
		ccp->scontext.id = u;

		/* request the service now */
		ret = auerswald_addservice(cp, &ccp->scontext);
		if (ret) {
			/* no: revert service entry */
			ccp->scontext.id = AUH_UNASSIGNED;
		}
		break;

		/* get a string descriptor for the device */
	case IOCTL_AU_DEVINFO:
		dbg("IOCTL_AU_DEVINFO");
		if (copy_from_user
		    (&devinfo, (void *) arg, sizeof(struct audevinfo))) {
			ret = -EFAULT;
			break;
		}
		u = strlen(cp->dev_desc) + 1;
		if (u > devinfo.bsize) {
			u = devinfo.bsize;
		}
		ret = copy_to_user(devinfo.buf, cp->dev_desc, u);
		break;

		/* get the max. string descriptor length */
	case IOCTL_AU_SLEN:
		dbg("IOCTL_AU_SLEN");
		u = AUSI_DLEN;
		ret = put_user(u, (unsigned int *) arg);
		break;

	default:
		dbg("IOCTL_AU_UNKNOWN");
		ret = -ENOIOCTLCMD;
		break;
	}
	/* release the mutexes */
	up(&cp->mutex);
	up(&ccp->mutex);
	return ret;
}


/* Seek is not supported */
loff_t auerchar_llseek(struct file * file, loff_t offset, int origin)
{
	dbg("auerchar_seek");
	return -ESPIPE;
}


/* Read data from the device */
ssize_t auerchar_read(struct file * file, char *buf, size_t count,
		      loff_t * ppos)
{
	unsigned long flags;
	struct auerchar *ccp = (struct auerchar *) file->private_data;
	struct auerbuf *bp = NULL;
	wait_queue_t wait;

	dbg("auerchar_read");

	/* Error checking */
	if (!ccp)
		return -EIO;
	if (*ppos)
		return -ESPIPE;
	if (count == 0)
		return 0;

	/* get the mutex */
	if (down_interruptible(&ccp->mutex))
		return -ERESTARTSYS;

	/* Can we expect to read something? */
	if (ccp->scontext.id == AUH_UNASSIGNED) {
		up(&ccp->mutex);
		return -EIO;
	}

	/* only one reader per device allowed */
	if (down_interruptible(&ccp->readmutex)) {
		up(&ccp->mutex);
		return -ERESTARTSYS;
	}

	/* read data from readbuf, if available */
      doreadbuf:
	bp = ccp->readbuf;
	if (bp) {
		/* read the maximum bytes */
		int restlen = bp->len - ccp->readoffset;
		if (restlen < 0)
			restlen = 0;
		if (count > restlen)
			count = restlen;
		if (count) {
			if (copy_to_user
			    (buf, bp->bufp + ccp->readoffset, count)) {
				dbg("auerswald_read: copy_to_user failed");
				up(&ccp->readmutex);
				up(&ccp->mutex);
				return -EFAULT;
			}
		}
		/* advance the read offset */
		ccp->readoffset += count;
		restlen -= count;
		// reuse the read buffer
		if (restlen <= 0) {
			auerbuf_releasebuf(bp);
			ccp->readbuf = NULL;
		}
		/* return with number of bytes read */
		if (count) {
			up(&ccp->readmutex);
			up(&ccp->mutex);
			return count;
		}
	}

	/* a read buffer is not available. Try to get the next data block. */
      doreadlist:
	/* Preparing for sleep */
	init_waitqueue_entry(&wait, current);
	set_current_state(TASK_INTERRUPTIBLE);
	add_wait_queue(&ccp->readwait, &wait);

	bp = NULL;
	spin_lock_irqsave(&ccp->bufctl.lock, flags);
	if (!list_empty(&ccp->bufctl.rec_buff_list)) {
		/* yes: get the entry */
		struct list_head *tmp = ccp->bufctl.rec_buff_list.next;
		list_del(tmp);
		bp = list_entry(tmp, struct auerbuf, buff_list);
	}
	spin_unlock_irqrestore(&ccp->bufctl.lock, flags);

	/* have we got data? */
	if (bp) {
		ccp->readbuf = bp;
		ccp->readoffset = AUH_SIZE;	/* for headerbyte */
		set_current_state(TASK_RUNNING);
		remove_wait_queue(&ccp->readwait, &wait);
		goto doreadbuf;	/* now we can read! */
	}

	/* no data available. Should we wait? */
	if (file->f_flags & O_NONBLOCK) {
		dbg("No read buffer available, returning -EAGAIN");
		set_current_state(TASK_RUNNING);
		remove_wait_queue(&ccp->readwait, &wait);
		up(&ccp->readmutex);
		up(&ccp->mutex);
		return -EAGAIN;	/* nonblocking, no data available */
	}

	/* yes, we should wait! */
	up(&ccp->mutex);	/* allow other operations while we wait */
	schedule();
	remove_wait_queue(&ccp->readwait, &wait);
	if (signal_pending(current)) {
		/* waked up by a signal */
		up(&ccp->readmutex);
		return -ERESTARTSYS;
	}

	/* Anything left to read? */
	if ((ccp->scontext.id == AUH_UNASSIGNED) || ccp->removed) {
		up(&ccp->readmutex);
		return -EIO;
	}

	if (down_interruptible(&ccp->mutex)) {
		up(&ccp->readmutex);
		return -ERESTARTSYS;
	}

	/* try to read the incomming data again */
	goto doreadlist;
}


/* Write a data block into the right service channel of the device */
ssize_t auerchar_write(struct file *file, const char *buf, size_t len,
		       loff_t * ppos)
{
	struct auerchar *ccp = (struct auerchar *) file->private_data;
	struct auerswald *cp = NULL;
	struct auerbuf *bp;
	int ret;
	wait_queue_t wait;

	dbg("auerchar_write %d bytes", len);

	/* Error checking */
	if (!ccp)
		return -EIO;
	if (*ppos)
		return -ESPIPE;
	if (len == 0)
		return 0;

      write_again:
	/* get the mutex */
	if (down_interruptible(&ccp->mutex))
		return -ERESTARTSYS;

	/* Can we expect to write something? */
	if (ccp->scontext.id == AUH_UNASSIGNED) {
		up(&ccp->mutex);
		return -EIO;
	}

	cp = ccp->auerdev;
	if (!cp) {
		up(&ccp->mutex);
		return -ERESTARTSYS;
	}
	if (down_interruptible(&cp->mutex)) {
		up(&ccp->mutex);
		return -ERESTARTSYS;
	}
	if (!cp->usbdev) {
		up(&cp->mutex);
		up(&ccp->mutex);
		return -EIO;
	}
	/* Prepare for sleep */
	init_waitqueue_entry(&wait, current);
	set_current_state(TASK_INTERRUPTIBLE);
	add_wait_queue(&cp->bufferwait, &wait);

	/* Try to get a buffer from the device pool.
	   We can't use a buffer from ccp->bufctl because the write
	   command will last beond a release() */
	bp = auerbuf_getbuf(&cp->bufctl);
	/* are there any buffers left? */
	if (!bp) {
		up(&cp->mutex);
		up(&ccp->mutex);

		/* NONBLOCK: don't wait */
		if (file->f_flags & O_NONBLOCK) {
			set_current_state(TASK_RUNNING);
			remove_wait_queue(&cp->bufferwait, &wait);
			return -EAGAIN;
		}

		/* BLOCKING: wait */
		schedule();
		remove_wait_queue(&cp->bufferwait, &wait);
		if (signal_pending(current)) {
			/* waked up by a signal */
			return -ERESTARTSYS;
		}
		goto write_again;
	} else {
		set_current_state(TASK_RUNNING);
		remove_wait_queue(&cp->bufferwait, &wait);
	}

	/* protect against too big write requests */
	if (len > cp->maxControlLength)
		len = cp->maxControlLength;

	/* Fill the buffer */
	if (copy_from_user(bp->bufp + AUH_SIZE, buf, len)) {
		dbg("copy_from_user failed");
		auerbuf_releasebuf(bp);
		/* Wake up all processes waiting for a buffer */
		wake_up(&cp->bufferwait);
		up(&cp->mutex);
		up(&ccp->mutex);
		return -EIO;
	}

	/* set the header byte */
	*(bp->bufp) = ccp->scontext.id | AUH_DIRECT | AUH_UNSPLIT;

	/* Set the transfer Parameters */
	bp->len = len + AUH_SIZE;
	bp->dr->bRequestType = AUT_WREQ;
	bp->dr->bRequest = AUV_WBLOCK;
	bp->dr->wValue = cpu_to_le16(0);
	bp->dr->wIndex =
	    cpu_to_le16(ccp->scontext.id | AUH_DIRECT | AUH_UNSPLIT);
	bp->dr->wLength = cpu_to_le16(len + AUH_SIZE);
	FILL_CONTROL_URB(bp->urbp, cp->usbdev,
			 usb_sndctrlpipe(cp->usbdev, 0),
			 (unsigned char *) bp->dr, bp->bufp,
			 len + AUH_SIZE, auerchar_ctrlwrite_complete, bp);
	/* up we go */
	ret = auerchain_submit_urb(&cp->controlchain, bp->urbp);
	up(&cp->mutex);
	if (ret) {
		dbg("auerchar_write: nonzero result of auerchain_submit_urb %d", ret);
		auerbuf_releasebuf(bp);
		/* Wake up all processes waiting for a buffer */
		wake_up(&cp->bufferwait);
		up(&ccp->mutex);
		return -EIO;
	} else {
		dbg("auerchar_write: Write OK");
		up(&ccp->mutex);
		return len;
	}
}


/* Close a character device */
int auerchar_release(struct inode *inode, struct file *file)
{
	struct auerchar *ccp = (struct auerchar *) file->private_data;
	struct auerswald *cp;
	dbg("release");

	/* get the mutexes */
	if (down_interruptible(&ccp->mutex)) {
		return -ERESTARTSYS;
	}
	cp = ccp->auerdev;
	if (cp) {
		if (down_interruptible(&cp->mutex)) {
			up(&ccp->mutex);
			return -ERESTARTSYS;
		}
		/* remove an open service */
		auerswald_removeservice(cp, &ccp->scontext);
		/* detach from device */
		if ((--cp->open_count <= 0) && (cp->usbdev == NULL)) {
			/* usb device waits for removal */
			up(&cp->mutex);
			auerswald_delete(cp);
		} else {
			up(&cp->mutex);
		}
		cp = NULL;
		ccp->auerdev = NULL;
	}
	up(&ccp->mutex);
	auerchar_delete(ccp);

	return 0;
}
