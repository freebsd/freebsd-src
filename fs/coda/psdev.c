/*
 *      	An implementation of a loadable kernel mode driver providing
 *		multiple kernel/user space bidirectional communications links.
 *
 * 		Author: 	Alan Cox <alan@redhat.com>
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 * 
 *              Adapted to become the Linux 2.0 Coda pseudo device
 *              Peter  Braam  <braam@maths.ox.ac.uk> 
 *              Michael Callahan <mjc@emmy.smith.edu>           
 *
 *              Changes for Linux 2.1
 *              Copyright (c) 1997 Carnegie-Mellon University
 */

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/sched.h>
#include <linux/lp.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#include <linux/fcntl.h>
#include <linux/delay.h>
#include <linux/skbuff.h>
#include <linux/proc_fs.h>
#include <linux/devfs_fs_kernel.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/poll.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/smp_lock.h>
#include <asm/io.h>
#include <asm/system.h>
#include <asm/poll.h>
#include <asm/uaccess.h>

#include <linux/coda.h>
#include <linux/coda_linux.h>
#include <linux/coda_fs_i.h>
#include <linux/coda_psdev.h>
#include <linux/coda_proc.h>

#define upc_free(r) kfree(r)

/* 
 * Coda stuff
 */
extern struct file_system_type coda_fs_type;

/* statistics */
int           coda_hard;         /* allows signals during upcalls */
unsigned long coda_timeout = 30; /* .. secs, then signals will dequeue */


struct venus_comm coda_comms[MAX_CODADEVS];
kmem_cache_t *cii_cache, *cred_cache, *upc_cache;

/*
 * Device operations
 */

static unsigned int coda_psdev_poll(struct file *file, poll_table * wait)
{
        struct venus_comm *vcp = (struct venus_comm *) file->private_data;
	unsigned int mask = POLLOUT | POLLWRNORM;

	poll_wait(file, &vcp->vc_waitq, wait);
	if (!list_empty(&vcp->vc_pending))
                mask |= POLLIN | POLLRDNORM;

	return mask;
}

static int coda_psdev_ioctl(struct inode * inode, struct file * filp, 
			    unsigned int cmd, unsigned long arg)
{
	unsigned int data;

	switch(cmd) {
	case CIOC_KERNEL_VERSION:
		data = CODA_KERNEL_VERSION;
		return put_user(data, (int *) arg);
	default:
		return -ENOTTY;
	}

	return 0;
}

/*
 *	Receive a message written by Venus to the psdev
 */
 
static ssize_t coda_psdev_write(struct file *file, const char *buf, 
				size_t nbytes, loff_t *off)
{
        struct venus_comm *vcp = (struct venus_comm *) file->private_data;
        struct upc_req *req = NULL;
        struct upc_req *tmp;
	struct list_head *lh;
	struct coda_in_hdr hdr;
	ssize_t retval = 0, count = 0;
	int error;

        /* Peek at the opcode, uniquefier */
	if (copy_from_user(&hdr, buf, 2 * sizeof(u_long)))
	        return -EFAULT;

	CDEBUG(D_PSDEV, "(process,opc,uniq)=(%d,%ld,%ld), nbytes %ld\n", 
	       current->pid, hdr.opcode, hdr.unique, (long)nbytes);

        if (DOWNCALL(hdr.opcode)) {
		struct super_block *sb = NULL;
                union outputArgs *dcbuf;
		int size = sizeof(*dcbuf);

		sb = vcp->vc_sb;
		if ( !sb ) {
			CDEBUG(D_PSDEV, "coda_psdev_write: downcall, no SB!\n");
                        count = nbytes;
                        goto out;
		}
		CDEBUG(D_PSDEV, "handling downcall\n");

		if  ( nbytes < sizeof(struct coda_out_hdr) ) {
		        printk("coda_downcall opc %ld uniq %ld, not enough!\n",
			       hdr.opcode, hdr.unique);
			count = nbytes;
			goto out;
		}
		if ( nbytes > size ) {
		        printk("Coda: downcall opc %ld, uniq %ld, too much!",
			       hdr.opcode, hdr.unique);
		        nbytes = size;
		}
		CODA_ALLOC(dcbuf, union outputArgs *, nbytes);
		if (copy_from_user(dcbuf, buf, nbytes)) {
			CODA_FREE(dcbuf, nbytes);
			retval = -EFAULT;
			goto out;
		}

		/* what downcall errors does Venus handle ? */
		lock_kernel();
		error = coda_downcall(hdr.opcode, dcbuf, sb);
		unlock_kernel();

		CODA_FREE(dcbuf, nbytes);
		if (error) {
		        printk("psdev_write: coda_downcall error: %d\n", error);
			retval = error;
			goto out;
		}
		count = nbytes;
		goto out;
	}
        
	/* Look for the message on the processing queue. */
	lock_kernel();
	list_for_each(lh, &vcp->vc_processing) {
		tmp = list_entry(lh, struct upc_req , uc_chain);
		if (tmp->uc_unique == hdr.unique) {
			req = tmp;
			list_del(&req->uc_chain);
			break;
		}
	}
	unlock_kernel();

	if (!req) {
		printk("psdev_write: msg (%ld, %ld) not found\n", 
			hdr.opcode, hdr.unique);
		retval = -ESRCH;
		goto out;
	}

	CDEBUG(D_PSDEV,"Eureka: uniq %ld on queue!\n", hdr.unique);

        /* move data into response buffer. */
	if (req->uc_outSize < nbytes) {
                printk("psdev_write: too much cnt: %d, cnt: %ld, opc: %ld, uniq: %ld.\n",
		       req->uc_outSize, (long)nbytes, hdr.opcode, hdr.unique);
		nbytes = req->uc_outSize; /* don't have more space! */
	}
        if (copy_from_user(req->uc_data, buf, nbytes)) {
		req->uc_flags |= REQ_ABORT;
		wake_up(&req->uc_sleep);
		retval = -EFAULT;
		goto out;
	}

	/* adjust outsize. is this useful ?? */
        req->uc_outSize = nbytes;	
        req->uc_flags |= REQ_WRITE;
	count = nbytes;

	/* Convert filedescriptor into a file handle */
	if (req->uc_opcode == CODA_OPEN_BY_FD) {
		struct coda_open_by_fd_out *outp =
			(struct coda_open_by_fd_out *)req->uc_data;
		outp->fh = fget(outp->fd);
	}

	CDEBUG(D_PSDEV, 
	       "Found! Count %ld for (opc,uniq)=(%ld,%ld), upc_req at %p\n", 
	        (long)count, hdr.opcode, hdr.unique, &req);

        wake_up(&req->uc_sleep);
out:
        return(count ? count : retval);  
}

/*
 *	Read a message from the kernel to Venus
 */

static ssize_t coda_psdev_read(struct file * file, char * buf, 
			       size_t nbytes, loff_t *off)
{
	DECLARE_WAITQUEUE(wait, current);
        struct venus_comm *vcp = (struct venus_comm *) file->private_data;
        struct upc_req *req;
	ssize_t retval = 0, count = 0;

	if (nbytes == 0)
		return 0;

	lock_kernel();

	add_wait_queue(&vcp->vc_waitq, &wait);
	set_current_state(TASK_INTERRUPTIBLE);

	while (list_empty(&vcp->vc_pending)) {
		if (file->f_flags & O_NONBLOCK) {
			retval = -EAGAIN;
			break;
		}
		if (signal_pending(current)) {
			retval = -ERESTARTSYS;
			break;
		}
		schedule();
	}

	set_current_state(TASK_RUNNING);
	remove_wait_queue(&vcp->vc_waitq, &wait);

	if (retval)
		goto out;

	req = list_entry(vcp->vc_pending.next, struct upc_req,uc_chain);
	list_del(&req->uc_chain);

	/* Move the input args into userspace */
	count = req->uc_inSize;
	if (nbytes < req->uc_inSize) {
                printk ("psdev_read: Venus read %ld bytes of %d in message\n",
			(long)nbytes, req->uc_inSize);
		count = nbytes;
        }

	if (copy_to_user(buf, req->uc_data, count))
	        retval = -EFAULT;
        
	/* If request was not a signal, enqueue and don't free */
	if (!(req->uc_flags & REQ_ASYNC)) {
		req->uc_flags |= REQ_READ;
		list_add(&(req->uc_chain), vcp->vc_processing.prev);
		goto out;
	}

	CDEBUG(D_PSDEV, "vcread: signal msg (%d, %d)\n", 
			req->uc_opcode, req->uc_unique);

	CODA_FREE(req->uc_data, sizeof(struct coda_in_hdr));
	upc_free(req);
out:
	unlock_kernel();
	return (count ? count : retval);
}

static int coda_psdev_open(struct inode * inode, struct file * file)
{
        struct venus_comm *vcp;
	int idx;

	lock_kernel();
	idx = MINOR(inode->i_rdev);
	if(idx >= MAX_CODADEVS) {
		unlock_kernel();
		return -ENODEV;
	}

	vcp = &coda_comms[idx];
	if(vcp->vc_inuse) {
		unlock_kernel();
		return -EBUSY;
	}
	
	if (!vcp->vc_inuse++) {
		INIT_LIST_HEAD(&vcp->vc_pending);
		INIT_LIST_HEAD(&vcp->vc_processing);
		init_waitqueue_head(&vcp->vc_waitq);
		vcp->vc_sb = 0;
		vcp->vc_seq = 0;
	}
	
	file->private_data = vcp;

	CDEBUG(D_PSDEV, "device %i - inuse: %d\n", idx, vcp->vc_inuse);

	unlock_kernel();
        return 0;
}


static int coda_psdev_release(struct inode * inode, struct file * file)
{
        struct venus_comm *vcp = (struct venus_comm *) file->private_data;
        struct upc_req *req;
	struct list_head *lh, *next;

	lock_kernel();
	if ( !vcp->vc_inuse ) {
		unlock_kernel();
		printk("psdev_release: Not open.\n");
		return -1;
	}

	CDEBUG(D_PSDEV, "psdev_release: inuse %d\n", vcp->vc_inuse);
	if (--vcp->vc_inuse) {
		unlock_kernel();
		return 0;
	}
        
        /* Wakeup clients so they can return. */
	CDEBUG(D_PSDEV, "wake up pending clients\n");
	lh = vcp->vc_pending.next;
	next = lh;
	while ( (lh = next) != &vcp->vc_pending) {
		next = lh->next;
		req = list_entry(lh, struct upc_req, uc_chain);
		/* Async requests need to be freed here */
		if (req->uc_flags & REQ_ASYNC) {
			CODA_FREE(req->uc_data, sizeof(struct coda_in_hdr));
			upc_free(req);
			continue;
		}
		req->uc_flags |= REQ_ABORT;
		wake_up(&req->uc_sleep);
        }
        
	lh = &vcp->vc_processing;
	CDEBUG(D_PSDEV, "wake up processing clients\n");
	while ( (lh = lh->next) != &vcp->vc_processing) {
		req = list_entry(lh, struct upc_req, uc_chain);
		req->uc_flags |= REQ_ABORT;
	        wake_up(&req->uc_sleep);
        }
	CDEBUG(D_PSDEV, "Done.\n");

	unlock_kernel();
	return 0;
}


static struct file_operations coda_psdev_fops = {
	owner:		THIS_MODULE,
	read:		coda_psdev_read,
	write:		coda_psdev_write,
	poll:		coda_psdev_poll,
	ioctl:		coda_psdev_ioctl,
	open:		coda_psdev_open,
	release:	coda_psdev_release,
};

static devfs_handle_t devfs_handle;

static int init_coda_psdev(void)
{
	if(devfs_register_chrdev(CODA_PSDEV_MAJOR,"coda_psdev",
				 &coda_psdev_fops)) {
              printk(KERN_ERR "coda_psdev: unable to get major %d\n", 
		     CODA_PSDEV_MAJOR);
              return -EIO;
	}
	devfs_handle = devfs_mk_dir (NULL, "coda", NULL);
	devfs_register_series (devfs_handle, "%u", MAX_CODADEVS, DEVFS_FL_NONE,
			       CODA_PSDEV_MAJOR, 0,
			       S_IFCHR | S_IRUSR | S_IWUSR,
			       &coda_psdev_fops, NULL);

	coda_sysctl_init();

	return 0;
}


MODULE_AUTHOR("Peter J. Braam <braam@cs.cmu.edu>");
MODULE_LICENSE("GPL");

static int __init init_coda(void)
{
	int status;
	printk(KERN_INFO "Coda Kernel/Venus communications, v5.3.18, coda@cs.cmu.edu\n");

	status = init_coda_psdev();
	if ( status ) {
		printk("Problem (%d) in init_coda_psdev\n", status);
		return status;
	}
	
	status = register_filesystem(&coda_fs_type);
	if (status) {
		printk("coda: failed to register filesystem!\n");
		devfs_unregister(devfs_handle);
		devfs_unregister_chrdev(CODA_PSDEV_MAJOR,"coda_psdev");
		coda_sysctl_clean();
	}
	return status;
}

static void __exit exit_coda(void)
{
        int err;

	err = unregister_filesystem(&coda_fs_type);
        if ( err != 0 ) {
                printk("coda: failed to unregister filesystem\n");
        }
	devfs_unregister(devfs_handle);
	devfs_unregister_chrdev(CODA_PSDEV_MAJOR, "coda_psdev");
	coda_sysctl_clean();
}

module_init(init_coda);
module_exit(exit_coda);

