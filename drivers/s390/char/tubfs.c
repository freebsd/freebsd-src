/*
 *  IBM/3270 Driver -- Copyright (C) UTS Global LLC
 *
 *  tubfs.c -- Fullscreen driver
 *
 *
 *
 *
 *
 *  Author:  Richard Hitt
 */
#include "tubio.h"

int fs3270_major = -1;			/* init to impossible -1 */

static int fs3270_open(struct inode *, struct file *);
static int fs3270_close(struct inode *, struct file *);
static int fs3270_ioctl(struct inode *, struct file *, unsigned int, unsigned long);
static ssize_t fs3270_read(struct file *, char *, size_t, loff_t *);
static ssize_t fs3270_write(struct file *, const char *, size_t, loff_t *);
static int fs3270_wait(tub_t *, long *);
static void fs3270_int(tub_t *tubp, devstat_t *dsp);
extern void tty3270_refresh(tub_t *);

static struct file_operations fs3270_fops = {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,3,0))
	owner: THIS_MODULE,		/* owner */
#endif
	read: 	fs3270_read,	/* read */
	write:	fs3270_write,	/* write */
	ioctl:	fs3270_ioctl,	/* ioctl */
	open: 	fs3270_open,	/* open */
	release:fs3270_close,	/* release */
};

#ifdef CONFIG_DEVFS_FS
devfs_handle_t fs3270_devfs_dir;
devfs_handle_t fs3270_devfs_tub;
extern struct file_operations tty_fops;

void fs3270_devfs_register(tub_t *tubp)
{
	char name[16];

	sprintf(name, "tub%.4x", tubp->devno);
	devfs_register(fs3270_devfs_dir, name, DEVFS_FL_DEFAULT,
		       IBM_FS3270_MAJOR, tubp->minor,
		       S_IFCHR | S_IRUSR | S_IWUSR, &fs3270_fops, NULL);
	sprintf(name, "tty%.4x", tubp->devno);
	tty_register_devfs_name(&tty3270_driver, 0, tubp->minor,
				fs3270_devfs_dir, name);
}

void fs3270_devfs_unregister(tub_t *tubp)
{
	char name[16];
	devfs_handle_t handle;

	sprintf(name, "tub%.4x", tubp->devno);
	handle = devfs_find_handle (fs3270_devfs_dir, name,
				    IBM_FS3270_MAJOR, tubp->minor,
				    DEVFS_SPECIAL_CHR, 0);
	devfs_unregister (handle);
	sprintf(name, "tty%.4x", tubp->devno);
	handle = devfs_find_handle (fs3270_devfs_dir, name,
				    IBM_TTY3270_MAJOR, tubp->minor,
				    DEVFS_SPECIAL_CHR, 0);
	devfs_unregister(handle);
}
#endif

/*
 * fs3270_init() -- Initialize fullscreen tubes
 */
int
fs3270_init(void)
{
	int rc;

#ifdef CONFIG_DEVFS_FS
	rc = devfs_register_chrdev (IBM_FS3270_MAJOR, "fs3270", &fs3270_fops);
	if (rc) {
		printk(KERN_ERR "tubmod can't get major nbr %d: error %d\n",
			IBM_FS3270_MAJOR, rc);
		return -1;
	}
	fs3270_devfs_dir = devfs_mk_dir(NULL, "3270", NULL);
	fs3270_devfs_tub = 
		devfs_register(fs3270_devfs_dir, "tub", DEVFS_FL_DEFAULT,
			       IBM_FS3270_MAJOR, 0,
			       S_IFCHR | S_IRUGO | S_IWUGO, 
			       &fs3270_fops, NULL);
#else
	rc = register_chrdev(IBM_FS3270_MAJOR, "fs3270", &fs3270_fops);
	if (rc) {
		printk(KERN_ERR "tubmod can't get major nbr %d: error %d\n",
			IBM_FS3270_MAJOR, rc);
		return -1;
	}
#endif
	fs3270_major = IBM_FS3270_MAJOR;
	return 0;
}

/*
 * fs3270_fini() -- Uninitialize fullscreen tubes
 */
void
fs3270_fini(void)
{
	if (fs3270_major != -1) {
#ifdef CONFIG_DEVFS_FS
		devfs_unregister(fs3270_devfs_tub);
		devfs_unregister(fs3270_devfs_dir);
#endif
		unregister_chrdev(fs3270_major, "fs3270");
		fs3270_major = -1;
	}
}

/*
 * fs3270_open
 */
static int
fs3270_open(struct inode *ip, struct file *fp)
{
	tub_t *tubp;
	long flags;

	/* See INODE2TUB(ip) for handling of "/dev/3270/tub" */
	if ((tubp = INODE2TUB(ip)) == NULL)
		return -ENOENT;

	TUBLOCK(tubp->irq, flags);
	if (tubp->mode == TBM_FS || tubp->mode == TBM_FSLN) {
		TUBUNLOCK(tubp->irq, flags);
		return -EBUSY;
	}

	tub_inc_use_count();
	fp->private_data = ip;
	tubp->mode = TBM_FS;
	tubp->intv = fs3270_int;
	tubp->dstat = 0;
	tubp->fs_pid = current->pid;
	tubp->fsopen = 1;
	TUBUNLOCK(tubp->irq, flags);
	return 0;
}

/*
 * fs3270_close aka release:  free the irq
 */
static int
fs3270_close(struct inode *ip, struct file *fp)
{
	tub_t *tubp;
	long flags;

	if ((tubp = INODE2TUB(ip)) == NULL)
		return -ENODEV;

	fs3270_wait(tubp, &flags);
	tubp->fsopen = 0;
	tubp->fs_pid = 0;
	tub_dec_use_count();
	tubp->intv = NULL;
	tubp->mode = 0;
	tty3270_refresh(tubp);
	TUBUNLOCK(tubp->irq, flags);
	return 0;
}

/*
 * fs3270_release() called from tty3270_hangup()
 */
void
fs3270_release(tub_t *tubp)
{
	long flags;

	if (tubp->mode != TBM_FS)
		return;
	fs3270_wait(tubp, &flags);
	tubp->fsopen = 0;
	tubp->fs_pid = 0;
	tub_dec_use_count();
	tubp->intv = NULL;
	tubp->mode = 0;
	/*tty3270_refresh(tubp);*/
	TUBUNLOCK(tubp->irq, flags);
}

/*
 * fs3270_wait(tub_t *tubp, int *flags) -- Wait to use tube
 * Entered without irq lock
 * On return:
 *      * Lock is held
 *      * Value is 0 or -ERESTARTSYS
 */
static int
fs3270_wait(tub_t *tubp, long *flags)
{
	DECLARE_WAITQUEUE(wait, current);

	TUBLOCK(tubp->irq, *flags);
	add_wait_queue(&tubp->waitq, &wait);
	while (!signal_pending(current) &&
	    ((tubp->mode != TBM_FS) ||
	     (tubp->flags & (TUB_WORKING | TUB_RDPENDING)) != 0)) {
		current->state = TASK_INTERRUPTIBLE;
		TUBUNLOCK(tubp->irq, *flags);
		schedule();
		current->state = TASK_RUNNING;
		TUBLOCK(tubp->irq, *flags);
	}
	remove_wait_queue(&tubp->waitq, &wait);
	return signal_pending(current)? -ERESTARTSYS: 0;
}

/*
 * fs3270_io(tubp, ccw1_t*) -- start I/O on the tube
 * Entered with irq lock held, WORKING off
 */
static int
fs3270_io(tub_t *tubp, ccw1_t *ccwp)
{
	int rc;

	rc = do_IO(tubp->irq, ccwp, tubp->irq, 0, 0);
	tubp->flags |= TUB_WORKING;
	tubp->dstat = 0;
	return rc;
}

/*
 * fs3270_bh(tubp) -- Perform back-half processing
 */
static void
fs3270_bh(void *data)
{
	long flags;
	tub_t *tubp;

	tubp = data;
	TUBLOCK(tubp->irq, flags);
	tubp->flags &= ~TUB_BHPENDING;

	if (tubp->wbuf) {       /* if we were writing */
		idal_buffer_free(tubp->wbuf);
		tubp->wbuf = NULL;
	}

	if ((tubp->flags & (TUB_ATTN | TUB_RDPENDING)) ==
	    (TUB_ATTN | TUB_RDPENDING)) {
		fs3270_io(tubp, &tubp->rccw);
		tubp->flags &= ~(TUB_ATTN | TUB_RDPENDING);
	}

	if ((tubp->flags & TUB_WORKING) == 0)
		wake_up_interruptible(&tubp->waitq);

	TUBUNLOCK(tubp->irq, flags);
}

/*
 * fs3270_sched_bh(tubp) -- Schedule the back half
 * Irq lock must be held on entry and remains held on exit.
 */
static void
fs3270_sched_bh(tub_t *tubp)
{
	if (tubp->flags & TUB_BHPENDING)
		return;
	tubp->flags |= TUB_BHPENDING;
	tubp->tqueue.routine = fs3270_bh;
	tubp->tqueue.data = tubp;
	queue_task(&tubp->tqueue, &tq_immediate);
	mark_bh(IMMEDIATE_BH);
}

/*
 * fs3270_int(tubp, prp) -- Process interrupt from tube in FS mode
 * This routine is entered with irq lock held (see do_IRQ in s390io.c)
 */
static void
fs3270_int(tub_t *tubp, devstat_t *dsp)
{
#define	DEV_UE_BUSY \
	(DEV_STAT_CHN_END | DEV_STAT_DEV_END | DEV_STAT_UNIT_EXCEP)

#ifdef RBHNOTYET
	/* XXX needs more work; must save 2d arg to fs370_io() */
	/* Handle CE-DE-UE and subsequent UDE */
	if (dsp->dstat == DEV_UE_BUSY) {
		tubp->flags |= TUB_UE_BUSY;
		return;
	} else if (tubp->flags & TUB_UE_BUSY) {
		tubp->flags &= ~TUB_UE_BUSY;
		if (dsp->dstat == DEV_STAT_DEV_END &&
		    (tubp->flags & TUB_WORKING) != 0) {
			fs3270_io(tubp);
			return;
		}
	}
#endif

	/* Handle ATTN */
	if (dsp->dstat & DEV_STAT_ATTENTION)
		tubp->flags |= TUB_ATTN;

	if (dsp->dstat & DEV_STAT_CHN_END) {
		tubp->cswl = dsp->rescnt;
		if ((dsp->dstat & DEV_STAT_DEV_END) == 0)
			tubp->flags |= TUB_EXPECT_DE;
		else
			tubp->flags &= ~TUB_EXPECT_DE;
	} else if (dsp->dstat & DEV_STAT_DEV_END) {
		if ((tubp->flags & TUB_EXPECT_DE) == 0)
			tubp->flags |= TUB_UNSOL_DE;
		tubp->flags &= ~TUB_EXPECT_DE;
	}
	if (dsp->dstat & DEV_STAT_DEV_END)
		tubp->flags &= ~TUB_WORKING;

	if ((tubp->flags & TUB_WORKING) == 0)
		fs3270_sched_bh(tubp);
}

/*
 * process ioctl commands for the tube driver
 */
static int
fs3270_ioctl(struct inode *ip, struct file *fp,
	unsigned int cmd, unsigned long arg)
{
	tub_t *tubp;
	int rc = 0;
	long flags;

	if ((tubp = INODE2TUB(ip)) == NULL)
		return -ENODEV;
	if ((rc = fs3270_wait(tubp, &flags))) {
		TUBUNLOCK(tubp->irq, flags);
		return rc;
	}

	switch(cmd) {
	case TUBICMD: tubp->icmd = arg; break;
	case TUBOCMD: tubp->ocmd = arg; break;
	case TUBGETI: put_user(tubp->icmd, (char *)arg); break;
	case TUBGETO: put_user(tubp->ocmd, (char *)arg); break;
	case TUBGETMOD:
		if (copy_to_user((char *)arg, &tubp->tubiocb,
		    sizeof tubp->tubiocb))
			rc = -EFAULT;
		break;
	}
	TUBUNLOCK(tubp->irq, flags);
	return rc;
}

/*
 * process read commands for the tube driver
 */
static ssize_t
fs3270_read(struct file *fp, char *dp, size_t len, loff_t *off)
{
	tub_t *tubp;
	ccw1_t *cp;
	int rc;
	long flags;
	struct idal_buffer *idal_buffer;

	if (len == 0 || len > 65535) {
		return -EINVAL;
	}

	if ((tubp = INODE2TUB((struct inode *)fp->private_data)) == NULL)
		return -ENODEV;

	if((idal_buffer = idal_buffer_alloc(len, 0)) == NULL) {
		len = -ENOMEM;
		goto do_cleanup;
	}

	if ((rc = fs3270_wait(tubp, &flags)) != 0) {
		TUBUNLOCK(tubp->irq, flags);
		len = rc;
		goto do_cleanup;
	}
	cp = &tubp->rccw;
	if (tubp->icmd == 0 && tubp->ocmd != 0)  tubp->icmd = 6;
	cp->cmd_code = tubp->icmd?:2;
	idal_buffer_set_cda(idal_buffer, cp);
	cp->flags |= CCW_FLAG_SLI;
	tubp->flags |= TUB_RDPENDING;
	TUBUNLOCK(tubp->irq, flags);

	if ((rc = fs3270_wait(tubp, &flags)) != 0) {
		tubp->flags &= ~TUB_RDPENDING;
		len = rc;
		TUBUNLOCK(tubp->irq, flags);
		goto do_cleanup;
	}
	TUBUNLOCK(tubp->irq, flags);

	len -= tubp->cswl;
	if (idal_buffer_to_user(idal_buffer, dp, len) != 0) {
		len = -EFAULT;
		goto do_cleanup;
	}

do_cleanup:
	idal_buffer_free(idal_buffer);

	return len;
}

/*
 * process write commands for the tube driver
 */
static ssize_t
fs3270_write(struct file *fp, const char *dp, size_t len, loff_t *off)
{
	tub_t *tubp;
	ccw1_t *cp;
	int rc;
	long flags;
	struct idal_buffer *idal_buffer;

	if (len > 65535 || len == 0)
		return -EINVAL;

	/* Locate the tube */
	if ((tubp = INODE2TUB((struct inode *)fp->private_data)) == NULL)
		return -ENODEV;

	if ((idal_buffer = idal_buffer_alloc(len, 0)) == NULL)
		return -ENOMEM;

	if (idal_buffer_from_user(idal_buffer, dp, len) != 0) {
		len = -EFAULT;
		goto do_cleanup;
	}

	/* Wait till tube's not working or signal is pending */
	if ((rc = fs3270_wait(tubp, &flags))) {
		len = rc;
		TUBUNLOCK(tubp->irq, flags);
		goto do_cleanup;
	}

	/* Make CCW and start I/O.  Back end will free buffers & idal. */
	cp = &tubp->wccw;
	cp->cmd_code = tubp->ocmd? tubp->ocmd == 5? 13: tubp->ocmd: 1;
	tubp->wbuf = idal_buffer;
	idal_buffer_set_cda(idal_buffer, cp);
	cp->flags |= CCW_FLAG_SLI;
	fs3270_io(tubp, cp);
	TUBUNLOCK(tubp->irq, flags);

	return len;

do_cleanup:
	idal_buffer_free(idal_buffer);

	return len;
}
