
/***************************************************************************
 *
 *  drivers/s390/char/tapechar.c
 *    character device frontend for tape device driver
 *
 *  S390 and zSeries version
 *    Copyright (C) 2001 IBM Corporation
 *    Author(s): Carsten Otte <cotte@de.ibm.com>
 *               Tuan Ngo-Anh <ngoanh@de.ibm.com>
 *
 *
 ****************************************************************************
 */

#include "tapedefs.h"
#include <linux/config.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <asm/ccwcache.h>	/* CCW allocations      */
#include <asm/s390dyn.h>
#include <asm/debug.h>
#include <linux/mtio.h>
#include <asm/uaccess.h>
#include <linux/compatmac.h>
#ifdef MODULE
#define __NO_VERSION__
#include <linux/module.h>
#endif
#include "tape.h"
#include "tapechar.h"

#define PRINTK_HEADER "TCHAR:"

/*
 * file operation structure for tape devices
 */
static struct file_operations tape_fops =
{
    //    owner   : THIS_MODULE,
	llseek:NULL,		/* lseek - default */
	read:tape_read,		/* read  */
	write:tape_write,	/* write */
	readdir:NULL,		/* readdir - bad */
	poll:NULL,		/* poll */
	ioctl:tape_ioctl,	/* ioctl */
	mmap:NULL,		/* mmap */
	open:tape_open,		/* open */
	flush:NULL,		/* flush */
	release:tape_release,	/* release */
	fsync:NULL,		/* fsync */
	fasync:NULL,		/* fasync */
	lock:NULL,
};

int tape_major = TAPE_MAJOR;

#ifdef CONFIG_DEVFS_FS
void
tapechar_mkdevfstree (tape_info_t* ti) {
    ti->devfs_char_dir=devfs_mk_dir (ti->devfs_dir, "char", ti);
    ti->devfs_nonrewinding=devfs_register(ti->devfs_char_dir, "nonrewinding",
					    DEVFS_FL_DEFAULT,tape_major, 
					    ti->nor_minor, TAPECHAR_DEFAULTMODE, 
					    &tape_fops, ti);
    ti->devfs_rewinding=devfs_register(ti->devfs_char_dir, "rewinding",
					 DEVFS_FL_DEFAULT, tape_major, ti->rew_minor,
					 TAPECHAR_DEFAULTMODE, &tape_fops, ti);
}

void
tapechar_rmdevfstree (tape_info_t* ti) {
    devfs_unregister(ti->devfs_nonrewinding);
    devfs_unregister(ti->devfs_rewinding);
    devfs_unregister(ti->devfs_char_dir);
}
#endif

void
tapechar_setup (tape_info_t * ti)
{
#ifdef CONFIG_DEVFS_FS
    tapechar_mkdevfstree(ti);
#endif
}

void
tapechar_init (void)
{
	int result;
	tape_frontend_t *charfront,*temp;
	tape_info_t* ti;

	tape_init();

	/* Register the tape major number to the kernel */
#ifdef CONFIG_DEVFS_FS
	result = devfs_register_chrdev (tape_major, "tape", &tape_fops);
#else
	result = register_chrdev (tape_major, "tape", &tape_fops);
#endif

	if (result < 0) {
		PRINT_WARN (KERN_ERR "tape: can't get major %d\n", tape_major);
#ifdef TAPE_DEBUG
		debug_text_event (tape_debug_area,3,"c:initfail");
		debug_text_event (tape_debug_area,3,"regchrfail");
#endif /* TAPE_DEBUG */
		panic ("no major number available for tape char device");
	}
	if (tape_major == 0)
		tape_major = result;	/* accept dynamic major number */
	PRINT_WARN (KERN_ERR " tape gets major %d for character device\n", result);
	charfront = kmalloc (sizeof (tape_frontend_t), GFP_KERNEL);
	if (charfront == NULL) {
#ifdef TAPE_DEBUG
                debug_text_event (tape_debug_area,3,"c:initfail");
		debug_text_event (tape_debug_area,3,"no mem");
#endif /* TAPE_DEBUG */
		panic ("no major number available for tape char device");		
	}
	charfront->device_setup = tapechar_setup;
#ifdef CONFIG_DEVFS_FS
	charfront->mkdevfstree = tapechar_mkdevfstree;
	charfront->rmdevfstree = tapechar_rmdevfstree;
#endif
#ifdef TAPE_DEBUG
        debug_text_event (tape_debug_area,3,"c:init ok");
#endif /* TAPE_DEBUG */
	charfront->next=NULL;
	if (first_frontend==NULL) {
	    first_frontend=charfront;
	} else {
	    temp=first_frontend;
	    while (temp->next!=NULL)
		temp=temp->next;
	    temp->next=charfront;
	}
	ti=first_tape_info;
	while (ti!=NULL) {
	    tapechar_setup(ti);
	    ti=ti->next;
	}
}

void
tapechar_uninit (void)
{
	unregister_chrdev (tape_major, "tape");
}

/*
 * Tape device read function
 */
ssize_t
tape_read (struct file *filp, char *data, size_t count, loff_t * ppos)
{
	long lockflags;
	tape_info_t *ti;
	size_t block_size;
	ccw_req_t *cqr;
	int rc;
#ifdef TAPE_DEBUG
        debug_text_event (tape_debug_area,6,"c:read");
#endif /* TAPE_DEBUG */
	ti = first_tape_info;
	while ((ti != NULL) && (ti->rew_filp != filp) && (ti->nor_filp != filp))
		ti = (tape_info_t *) ti->next;
	if (ti == NULL) {
#ifdef TAPE_DEBUG
	        debug_text_event (tape_debug_area,6,"c:nodev");
#endif /* TAPE_DEBUG */
		return -ENODEV;
	}
	if (ppos != &filp->f_pos) {
		/* "A request was outside the capabilities of the device." */
#ifdef TAPE_DEBUG
	        debug_text_event (tape_debug_area,6,"c:ppos wrong");
#endif /* TAPE_DEBUG */
		return -EOVERFLOW;	/* errno=75 Value too large for def. data type */
	}
	if (ti->block_size == 0) {
		block_size = count;
	} else {
		block_size = ti->block_size;
	}
#ifdef TAPE_DEBUG
	debug_text_event (tape_debug_area,6,"c:nbytes:");
	debug_int_event (tape_debug_area,6,block_size);
#endif
	cqr = ti->discipline->read_block (data, block_size, ti);
	if (!cqr) {
		return -ENOBUFS;
	}
	s390irq_spin_lock_irqsave (ti->devinfo.irq, lockflags);
	ti->cqr = cqr;
	ti->wanna_wakeup=0;
	rc = do_IO (ti->devinfo.irq, cqr->cpaddr, (unsigned long) cqr, 0x00, cqr->options);
	if (rc) {
	    tapestate_set(ti,TS_IDLE);
	    kfree (cqr);
	    s390irq_spin_unlock_irqrestore (ti->devinfo.irq, lockflags);
	    return rc;
	}
	s390irq_spin_unlock_irqrestore (ti->devinfo.irq, lockflags);
	wait_event (ti->wq,ti->wanna_wakeup);
	ti->cqr = NULL;
	ti->discipline->free_read_block (cqr, ti);
	s390irq_spin_lock_irqsave (ti->devinfo.irq, lockflags);
	if (tapestate_get (ti) == TS_FAILED) {
		tapestate_set (ti, TS_IDLE);
		s390irq_spin_unlock_irqrestore (ti->devinfo.irq, lockflags);
		return ti->rc;
	}
	if (tapestate_get (ti) == TS_NOT_OPER) {
	    ti->blk_minor=ti->rew_minor=ti->nor_minor=-1;
	    ti->devinfo.irq=-1;
	    s390irq_spin_unlock_irqrestore (ti->devinfo.irq,lockflags);
	    return -ENODEV;
	}
	if (tapestate_get (ti) != TS_DONE) {
		tapestate_set (ti, TS_IDLE);
		s390irq_spin_unlock_irqrestore (ti->devinfo.irq, lockflags);
		return -EIO;
	}
	tapestate_set (ti, TS_IDLE);
	s390irq_spin_unlock_irqrestore (ti->devinfo.irq, lockflags);
#ifdef TAPE_DEBUG
	debug_text_event (tape_debug_area,6,"c:rbytes:");
	debug_int_event (tape_debug_area,6,block_size - ti->devstat.rescnt);
#endif	/* TAPE_DEBUG */
	filp->f_pos += block_size - ti->devstat.rescnt;
	return block_size - ti->devstat.rescnt;
}

/*
 * Tape device write function
 */
ssize_t
tape_write (struct file *filp, const char *data, size_t count, loff_t * ppos)
{
	long lockflags;
	tape_info_t *ti;
	size_t block_size;
	ccw_req_t *cqr;
	int nblocks, i, rc;
	size_t written = 0;
#ifdef TAPE_DEBUG
	debug_text_event (tape_debug_area,6,"c:write");
#endif
	ti = first_tape_info;
	while ((ti != NULL) && (ti->nor_filp != filp) && (ti->rew_filp != filp))
		ti = (tape_info_t *) ti->next;
	if (ti == NULL)
		return -ENODEV;
	if (ppos != &filp->f_pos) {
		/* "A request was outside the capabilities of the device." */
#ifdef TAPE_DEBUG
	        debug_text_event (tape_debug_area,6,"c:ppos wrong");
#endif
		return -EOVERFLOW;	/* errno=75 Value too large for def. data type */
	}
	if ((ti->block_size != 0) && (count % ti->block_size != 0))
		return -EIO;
	if (ti->block_size == 0) {
		block_size = count;
		nblocks = 1;
	} else {
		block_size = ti->block_size;
		nblocks = count / (ti->block_size);
	}
#ifdef TAPE_DEBUG
	        debug_text_event (tape_debug_area,6,"c:nbytes:");
		debug_int_event (tape_debug_area,6,block_size);
	        debug_text_event (tape_debug_area,6,"c:nblocks:");
	        debug_int_event (tape_debug_area,6,nblocks);
#endif
	for (i = 0; i < nblocks; i++) {
		cqr = ti->discipline->write_block (data + i * block_size, block_size, ti);
		if (!cqr) {
			return -ENOBUFS;
		}
		s390irq_spin_lock_irqsave (ti->devinfo.irq, lockflags);
		ti->cqr = cqr;
		ti->wanna_wakeup=0;
		rc = do_IO (ti->devinfo.irq, cqr->cpaddr, (unsigned long) cqr, 0x00, cqr->options);
		s390irq_spin_unlock_irqrestore (ti->devinfo.irq, lockflags);
		wait_event_interruptible (ti->wq,ti->wanna_wakeup);
		ti->cqr = NULL;
		ti->discipline->free_write_block (cqr, ti);
		if (signal_pending (current)) {
			tapestate_set (ti, TS_IDLE);
			return -ERESTARTSYS;
		}
		s390irq_spin_lock_irqsave (ti->devinfo.irq, lockflags);
		if (tapestate_get (ti) == TS_FAILED) {
			tapestate_set (ti, TS_IDLE);
			s390irq_spin_unlock_irqrestore (ti->devinfo.irq, lockflags);
                        if ((ti->rc==-ENOSPC) && (i!=0))
			  return i*block_size;
			return ti->rc;
		}
		if (tapestate_get (ti) == TS_NOT_OPER) {
		    ti->blk_minor=ti->rew_minor=ti->nor_minor=-1;
		    ti->devinfo.irq=-1;
		    s390irq_spin_unlock_irqrestore (ti->devinfo.irq,lockflags);
		    return -ENODEV;
		}
		if (tapestate_get (ti) != TS_DONE) {
			tapestate_set (ti, TS_IDLE);
			s390irq_spin_unlock_irqrestore (ti->devinfo.irq, lockflags);
			return -EIO;
		}
		tapestate_set (ti, TS_IDLE);
		s390irq_spin_unlock_irqrestore (ti->devinfo.irq, lockflags);
#ifdef TAPE_DEBUG
	        debug_text_event (tape_debug_area,6,"c:wbytes:"); 
		debug_int_event (tape_debug_area,6,block_size - ti->devstat.rescnt);
#endif
		filp->f_pos += block_size - ti->devstat.rescnt;
		written += block_size - ti->devstat.rescnt;
		if (ti->devstat.rescnt > 0)
			return written;
	}
#ifdef TAPE_DEBUG
	debug_text_event (tape_debug_area,6,"c:wtotal:");
	debug_int_event (tape_debug_area,6,written);
#endif
	return written;
}

static int
tape_mtioctop (struct file *filp, short mt_op, int mt_count)
{
	tape_info_t *ti;
	ccw_req_t *cqr = NULL;
	int rc;
	long lockflags;
#ifdef TAPE_DEBUG
	debug_text_event (tape_debug_area,6,"c:mtio");
	debug_text_event (tape_debug_area,6,"c:ioop:");
	debug_int_event (tape_debug_area,6,mt_op); 
	debug_text_event (tape_debug_area,6,"c:arg:");
	debug_int_event (tape_debug_area,6,mt_count);
#endif
	ti = first_tape_info;
	while ((ti != NULL) && (ti->rew_filp != filp) && (ti->nor_filp != filp))
		ti = (tape_info_t *) ti->next;
	if (ti == NULL)
		return -ENODEV;
	switch (mt_op) {
	case MTREW:		// rewind

		cqr = ti->discipline->mtrew (ti, mt_count);
		break;
	case MTOFFL:		// put drive offline

		cqr = ti->discipline->mtoffl (ti, mt_count);
		break;
	case MTUNLOAD:		// unload the tape

		cqr = ti->discipline->mtunload (ti, mt_count);
		break;
	case MTWEOF:		// write tapemark

		cqr = ti->discipline->mtweof (ti, mt_count);
		break;
	case MTFSF:		// forward space file

		cqr = ti->discipline->mtfsf (ti, mt_count);
		break;
	case MTBSF:		// backward space file

		cqr = ti->discipline->mtbsf (ti, mt_count);
		break;
	case MTFSFM:		// forward space file, stop at BOT side

		cqr = ti->discipline->mtfsfm (ti, mt_count);
		break;
	case MTBSFM:		// backward space file, stop at BOT side

		cqr = ti->discipline->mtbsfm (ti, mt_count);
		break;
	case MTFSR:		// forward space file

		cqr = ti->discipline->mtfsr (ti, mt_count);
		break;
	case MTBSR:		// backward space file

		cqr = ti->discipline->mtbsr (ti, mt_count);
		break;
	case MTNOP:
		cqr = ti->discipline->mtnop (ti, mt_count);
		break;
	case MTEOM:		// postion at the end of portion

	case MTRETEN:		// retension the tape

		cqr = ti->discipline->mteom (ti, mt_count);
		break;
	case MTERASE:
		cqr = ti->discipline->mterase (ti, mt_count);
		break;
	case MTSETDENSITY:
		cqr = ti->discipline->mtsetdensity (ti, mt_count);
		break;
	case MTSEEK:
		cqr = ti->discipline->mtseek (ti, mt_count);
		break;
	case MTSETDRVBUFFER:
		cqr = ti->discipline->mtsetdrvbuffer (ti, mt_count);
		break;
	case MTLOCK:
		cqr = ti->discipline->mtsetdrvbuffer (ti, mt_count);
		break;
	case MTUNLOCK:
		cqr = ti->discipline->mtsetdrvbuffer (ti, mt_count);
		break;
	case MTLOAD:
		cqr = ti->discipline->mtload (ti, mt_count);
		if (cqr!=NULL) break; // if backend driver has an load function ->use it
		// if no medium is in, wait until it gets inserted
		if (ti->medium_is_unloaded) {
		    wait_event_interruptible (ti->wq,ti->medium_is_unloaded==0);
		}
		return 0;
	case MTCOMPRESSION:
		cqr = ti->discipline->mtcompression (ti, mt_count);
		break;
	case MTSETPART:
		cqr = ti->discipline->mtsetpart (ti, mt_count);
		break;
	case MTMKPART:
		cqr = ti->discipline->mtmkpart (ti, mt_count);
		break;
	case MTTELL:		// return number of block relative to current file

		cqr = ti->discipline->mttell (ti, mt_count);
		break;
	case MTSETBLK:
		s390irq_spin_lock_irqsave (ti->devinfo.irq, lockflags);
		ti->block_size = mt_count;
		s390irq_spin_unlock_irqrestore (ti->devinfo.irq, lockflags);
#ifdef TAPE_DEBUG
		debug_text_event (tape_debug_area,6,"c:setblk:");
	        debug_int_event (tape_debug_area,6,mt_count);
#endif
		return 0;
	case MTRESET:
		s390irq_spin_lock_irqsave (ti->devinfo.irq, lockflags);
		ti->kernbuf = ti->userbuf = NULL;
		tapestate_set (ti, TS_IDLE);
		ti->block_size = 0;
		s390irq_spin_unlock_irqrestore (ti->devinfo.irq, lockflags);
#ifdef TAPE_DEBUG
		debug_text_event (tape_debug_area,6,"c:devreset:");
		debug_int_event (tape_debug_area,6,ti->blk_minor);
#endif
		return 0;
	default:
#ifdef TAPE_DEBUG
	    debug_text_event (tape_debug_area,6,"c:inv.mtio");
#endif
		return -EINVAL;
	}
	if (cqr == NULL) {
#ifdef TAPE_DEBUG
	        debug_text_event (tape_debug_area,6,"c:ccwg fail");
#endif
		return -ENOSPC;
	}
	s390irq_spin_lock_irqsave (ti->devinfo.irq, lockflags);
	ti->cqr = cqr;
	ti->wanna_wakeup=0;
	rc = do_IO (ti->devinfo.irq, cqr->cpaddr, (unsigned long) cqr, 0x00, cqr->options);
	s390irq_spin_unlock_irqrestore (ti->devinfo.irq, lockflags);
	wait_event_interruptible (ti->wq,ti->wanna_wakeup);
	ti->cqr = NULL;
	if (ti->kernbuf != NULL) {
		kfree (ti->kernbuf);
		ti->kernbuf = NULL;
	}
	tape_free_request (cqr);
	// if medium was unloaded, update the corresponding variable.
	switch (mt_op) {
	case MTOFFL:
	case MTUNLOAD:
	    ti->medium_is_unloaded=1;
	}
	if (signal_pending (current)) {
		tapestate_set (ti, TS_IDLE);
		return -ERESTARTSYS;
	}
	s390irq_spin_lock_irqsave (ti->devinfo.irq, lockflags);
	if (((mt_op == MTEOM) || (mt_op == MTRETEN)) && (tapestate_get (ti) == TS_FAILED))
		tapestate_set (ti, TS_DONE);
	if (tapestate_get (ti) == TS_FAILED) {
		tapestate_set (ti, TS_IDLE);
		s390irq_spin_unlock_irqrestore (ti->devinfo.irq, lockflags);
		return ti->rc;
	}
	if (tapestate_get (ti) == TS_NOT_OPER) {
	    ti->blk_minor=ti->rew_minor=ti->nor_minor=-1;
	    ti->devinfo.irq=-1;
	    s390irq_spin_unlock_irqrestore (ti->devinfo.irq,lockflags);
	    return -ENODEV;
	}
	if (tapestate_get (ti) != TS_DONE) {
		tapestate_set (ti, TS_IDLE);
		s390irq_spin_unlock_irqrestore (ti->devinfo.irq, lockflags);
		return -EIO;
	}
	tapestate_set (ti, TS_IDLE);
	s390irq_spin_unlock_irqrestore (ti->devinfo.irq, lockflags);
	switch (mt_op) {
	case MTRETEN:		//need to rewind the tape after moving to eom

		return tape_mtioctop (filp, MTREW, 1);
	case MTFSFM:		//need to skip back over the filemark

		return tape_mtioctop (filp, MTBSFM, 1);
	case MTBSF:		//need to skip forward over the filemark

		return tape_mtioctop (filp, MTFSF, 1);
	}
#ifdef TAPE_DEBUG
	debug_text_event (tape_debug_area,6,"c:mtio done");
#endif
	return 0;
}

/*
 * Tape device io controls.
 */
int
tape_ioctl (struct inode *inode, struct file *filp,
	    unsigned int cmd, unsigned long arg)
{
	long lockflags;
	tape_info_t *ti;
	ccw_req_t *cqr;
	struct mtop op;		/* structure for MTIOCTOP */
	struct mtpos pos;	/* structure for MTIOCPOS */
	struct mtget get;

	int rc;
#ifdef TAPE_DEBUG
	debug_text_event (tape_debug_area,6,"c:ioct");
#endif
	ti = first_tape_info;
	while ((ti != NULL) &&
	       (ti->rew_minor != MINOR (inode->i_rdev)) &&
	       (ti->nor_minor != MINOR (inode->i_rdev)))
		ti = (tape_info_t *) ti->next;
	if (ti == NULL) {
#ifdef TAPE_DEBUG
	        debug_text_event (tape_debug_area,6,"c:nodev");
#endif
		return -ENODEV;
	}
	// check for discipline ioctl overloading
	if ((rc = ti->discipline->discipline_ioctl_overload (inode, filp, cmd, arg))
	    != -EINVAL) {
#ifdef TAPE_DEBUG
	    debug_text_event (tape_debug_area,6,"c:ioverloa");
#endif
	    return rc;
	}

	switch (cmd) {
	case MTIOCTOP:		/* tape op command */
		if (copy_from_user (&op, (char *) arg, sizeof (struct mtop))) {
			 return -EFAULT;
		}
		return (tape_mtioctop (filp, op.mt_op, op.mt_count));
	case MTIOCPOS:		/* query tape position */
		cqr = ti->discipline->mttell (ti, 0);
		s390irq_spin_lock_irqsave (ti->devinfo.irq, lockflags);
		ti->cqr = cqr;
		ti->wanna_wakeup=0;
		do_IO (ti->devinfo.irq, cqr->cpaddr, (unsigned long) cqr, 0x00, cqr->options);
		s390irq_spin_unlock_irqrestore (ti->devinfo.irq, lockflags);
		wait_event_interruptible (ti->wq,ti->wanna_wakeup);
		pos.mt_blkno = ti->rc;
		ti->cqr = NULL;
		if (ti->kernbuf != NULL) {
			kfree (ti->kernbuf);
			ti->kernbuf = NULL;
		}
		tape_free_request (cqr);
		if (signal_pending (current)) {
			tapestate_set (ti, TS_IDLE);
			return -ERESTARTSYS;
		}
		s390irq_spin_lock_irqsave (ti->devinfo.irq, lockflags);
		tapestate_set (ti, TS_IDLE);
		s390irq_spin_unlock_irqrestore (ti->devinfo.irq, lockflags);
		if (copy_to_user ((char *) arg, &pos, sizeof (struct mtpos)))
			 return -EFAULT;
		return 0;
	case MTIOCGET:
		get.mt_erreg = ti->rc;
		cqr = ti->discipline->mttell (ti, 0);
		s390irq_spin_lock_irqsave (ti->devinfo.irq, lockflags);
		ti->cqr = cqr;
		ti->wanna_wakeup=0;
		do_IO (ti->devinfo.irq, cqr->cpaddr, (unsigned long) cqr, 0x00, cqr->options);
		s390irq_spin_unlock_irqrestore (ti->devinfo.irq, lockflags);
		wait_event_interruptible (ti->wq,ti->wanna_wakeup);
		get.mt_blkno = ti->rc;
		get.mt_fileno = 0;
		get.mt_type = MT_ISUNKNOWN;
		get.mt_resid = ti->devstat.rescnt;
		get.mt_dsreg = ti->devstat.ii.sense.data[3];
		get.mt_gstat = 0;
		if (ti->devstat.ii.sense.data[1] & 0x08)
			get.mt_gstat &= GMT_BOT (1);	// BOT

		if (ti->devstat.ii.sense.data[1] & 0x02)
			get.mt_gstat &= GMT_WR_PROT (1);	// write protected

		if (ti->devstat.ii.sense.data[1] & 0x40)
			get.mt_gstat &= GMT_ONLINE (1);		//drive online

		ti->cqr = NULL;
		if (ti->kernbuf != NULL) {
			kfree (ti->kernbuf);
			ti->kernbuf = NULL;
		}
		tape_free_request (cqr);
		if (signal_pending (current)) {
			tapestate_set (ti, TS_IDLE);
			return -ERESTARTSYS;
		}
		s390irq_spin_lock_irqsave (ti->devinfo.irq, lockflags);
		tapestate_set (ti, TS_IDLE);
		s390irq_spin_unlock_irqrestore (ti->devinfo.irq, lockflags);
		if (copy_to_user ((char *) arg, &get, sizeof (struct mtget)))
			 return -EFAULT;
		return 0;
	default:
#ifdef TAPE_DEBUG
	        debug_text_event (tape_debug_area,3,"c:ioct inv");
#endif	    
		return -EINVAL;
	}
}

/*
 * Tape device open function.
 */
int
tape_open (struct inode *inode, struct file *filp)
{
	tape_info_t *ti;
	kdev_t dev;
	long lockflags;

	inode = filp->f_dentry->d_inode;
	ti = first_tape_info;
	while ((ti != NULL) &&
	       (ti->rew_minor != MINOR (inode->i_rdev)) &&
	       (ti->nor_minor != MINOR (inode->i_rdev)))
		ti = (tape_info_t *) ti->next;
	if (ti == NULL)
		return -ENODEV;
#ifdef TAPE_DEBUG
	debug_text_event (tape_debug_area,6,"c:open:");
	debug_int_event (tape_debug_area,6,ti->blk_minor);
#endif
	s390irq_spin_lock_irqsave (ti->devinfo.irq, lockflags);
	if (tapestate_get (ti) != TS_UNUSED) {
		s390irq_spin_unlock_irqrestore (ti->devinfo.irq, lockflags);
#ifdef TAPE_DEBUG
		debug_text_event (tape_debug_area,6,"c:dbusy");
#endif
		return -EBUSY;
	}
	tapestate_set (ti, TS_IDLE);
	s390irq_spin_unlock_irqrestore (ti->devinfo.irq, lockflags);

	dev = MKDEV (tape_major, MINOR (inode->i_rdev));	/* Get the device */
	s390irq_spin_lock_irqsave (ti->devinfo.irq, lockflags);
	if (ti->rew_minor == MINOR (inode->i_rdev))
		ti->rew_filp = filp;	/* save for later reference     */
	else
		ti->nor_filp = filp;
	filp->private_data = ti;	/* save the dev.info for later reference */
	s390irq_spin_unlock_irqrestore (ti->devinfo.irq, lockflags);

#ifdef MODULE
	MOD_INC_USE_COUNT;
#endif				/* MODULE */
	return 0;
}

/*
 * Tape device release function.
 */
int
tape_release (struct inode *inode, struct file *filp)
{
	long lockflags;
	tape_info_t *ti,*lastti;
	ccw_req_t *cqr = NULL;
	int rc = 0;

	ti = first_tape_info;
	while ((ti != NULL) && (ti->rew_minor != MINOR (inode->i_rdev)) && (ti->nor_minor != MINOR (inode->i_rdev)))
		ti = (tape_info_t *) ti->next;
	if ((ti != NULL) && (tapestate_get (ti) == TS_NOT_OPER)) {
	    if (ti==first_tape_info) {
		first_tape_info=ti->next;
	    } else {
		lastti=first_tape_info;
		while (lastti->next!=ti) lastti=lastti->next;
		lastti->next=ti->next;
	    }
	    kfree(ti);    
	    goto out;
	}
	if ((ti == NULL) || (tapestate_get (ti) != TS_IDLE)) {
#ifdef TAPE_DEBUG
	debug_text_event (tape_debug_area,6,"c:notidle!");
#endif
		rc = -ENXIO;	/* error in tape_release */
		goto out;
	}
#ifdef TAPE_DEBUG
	debug_text_event (tape_debug_area,6,"c:release:");
	debug_int_event (tape_debug_area,6,ti->blk_minor);
#endif
	if (ti->rew_minor == MINOR (inode->i_rdev)) {
		cqr = ti->discipline->mtrew (ti, 1);
		if (cqr != NULL) {
#ifdef TAPE_DEBUG
		        debug_text_event (tape_debug_area,6,"c:rewrelea");
#endif
			s390irq_spin_lock_irqsave (ti->devinfo.irq, lockflags);
			tapestate_set (ti, TS_REW_RELEASE_INIT);
			ti->cqr = cqr;
			ti->wanna_wakeup=0;
			rc = do_IO (ti->devinfo.irq, cqr->cpaddr, (unsigned long) cqr, 0x00, cqr->options);
			s390irq_spin_unlock_irqrestore (ti->devinfo.irq, lockflags);
			wait_event (ti->wq,ti->wanna_wakeup);
			ti->cqr = NULL;
			tape_free_request (cqr);
		}
	}
	s390irq_spin_lock_irqsave (ti->devinfo.irq, lockflags);
	tapestate_set (ti, TS_UNUSED);
	s390irq_spin_unlock_irqrestore (ti->devinfo.irq, lockflags);
out:
#ifdef MODULE
	MOD_DEC_USE_COUNT;
#endif				/* MODULE */
	return rc;
}
