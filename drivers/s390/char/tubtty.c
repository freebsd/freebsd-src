/*
 *  IBM/3270 Driver -- Copyright (C) 2000, 2001 UTS Global LLC
 *
 *  tubtty.c -- Linemode tty driver
 *
 *
 *
 *
 *
 *  Author:  Richard Hitt
 */
#include <linux/config.h>
#include "tubio.h"

/* Initialization & uninitialization for tubtty */
int tty3270_init(void);
void tty3270_fini(void);

/* Interface routines from the upper tty layer to the tty driver */
static int tty3270_open(struct tty_struct *, struct file *);
static void tty3270_close(struct tty_struct *, struct file *);
static int tty3270_write(struct tty_struct *, int,
        const unsigned char *, int);
static void tty3270_put_char(struct tty_struct *, unsigned char);
static void tty3270_flush_chars(struct tty_struct *);
static int tty3270_write_room(struct tty_struct *);
static int tty3270_chars_in_buffer(struct tty_struct *);
static int tty3270_ioctl(struct tty_struct *, struct file *,
	unsigned int cmd, unsigned long arg);
static void tty3270_set_termios(struct tty_struct *, struct termios *);
static void tty3270_hangup(struct tty_struct *);
static void tty3270_flush_buffer(struct tty_struct *);
static int tty3270_read_proc(char *, char **, off_t, int, int *, void *);
static int tty3270_write_proc(struct file *, const char *,
	unsigned long, void *);

/* tty3270 utility functions */
static void tty3270_bh(void *);
       void tty3270_sched_bh(tub_t *);
static int tty3270_wait(tub_t *, long *);
       void tty3270_int(tub_t *, devstat_t *);
       int tty3270_try_logging(tub_t *);
static void tty3270_start_input(tub_t *);
static void tty3270_do_input(tub_t *);
static void tty3270_do_enter(tub_t *, char *, int);
static void tty3270_do_showi(tub_t *, char *, int);
       int tty3270_io(tub_t *);
static int tty3270_show_tube(int, char *, int);

int tty3270_major = -1;
struct tty_driver tty3270_driver;
int tty3270_refcount;
struct tty_struct *tty3270_table[TUBMAXMINS];
struct termios *tty3270_termios[TUBMAXMINS];
struct termios *tty3270_termios_locked[TUBMAXMINS];
#ifdef CONFIG_TN3270_CONSOLE
int con3270_major = -1;
struct tty_driver con3270_driver;
int con3270_refcount;
struct tty_struct *con3270_table[1];
struct termios *con3270_termios[1];
struct termios *con3270_termios_locked[1];
#endif /* CONFIG_TN3270_CONSOLE */

int tty3270_proc_index;
int tty3270_proc_data;
int tty3270_proc_misc;
enum tubwhat tty3270_proc_what;

/*
 * tty3270_init() -- Register the tty3270 driver
 */
int
tty3270_init(void)
{
	struct tty_driver *td = &tty3270_driver;
	int rc;

	/* Initialize for tty driver */
	td->magic = TTY_DRIVER_MAGIC;
	td->driver_name = "tty3270";
	td->name = "tty3270";
	td->major = IBM_TTY3270_MAJOR;
	td->minor_start = 0;
	td->num = TUBMAXMINS;
	td->type = TTY_DRIVER_TYPE_SYSTEM;
	td->subtype = SYSTEM_TYPE_TTY;
	td->init_termios = tty_std_termios;
	td->flags = TTY_DRIVER_RESET_TERMIOS;
#ifdef CONFIG_DEVFS_FS
	td->flags |= TTY_DRIVER_NO_DEVFS;
#endif
	td->refcount = &tty3270_refcount;
	td->table = tty3270_table;
	td->termios = tty3270_termios;
	td->termios_locked = tty3270_termios_locked;

	td->open = tty3270_open;
	td->close = tty3270_close;
	td->write = tty3270_write;
	td->put_char = tty3270_put_char;
	td->flush_chars = tty3270_flush_chars;
	td->write_room = tty3270_write_room;
	td->chars_in_buffer = tty3270_chars_in_buffer;
	td->ioctl = tty3270_ioctl;
	td->ioctl = NULL;
	td->set_termios = tty3270_set_termios;
	td->throttle = NULL;
	td->unthrottle = NULL;
	td->stop = NULL;
	td->start = NULL;
	td->hangup = tty3270_hangup;
	td->break_ctl = NULL;
	td->flush_buffer = tty3270_flush_buffer;
	td->set_ldisc = NULL;
	td->wait_until_sent = NULL;
	td->send_xchar = NULL;
	td->read_proc = tty3270_read_proc;
	td->write_proc = tty3270_write_proc;

	rc = tty_register_driver(td);
	if (rc) {
		printk(KERN_ERR "tty3270 registration failed with %d\n", rc);
	} else {
		tty3270_major = IBM_TTY3270_MAJOR;
		if (td->proc_entry != NULL)
			td->proc_entry->mode = S_IRUGO | S_IWUGO;
	}
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,3,0))
#ifdef CONFIG_TN3270_CONSOLE
	if (CONSOLE_IS_3270) {
		tty3270_con_driver = *td;
		td = &tty3270_con_driver;
		td->driver_name = "con3270";
		td->name = "con3270";
		td->major = MAJOR(S390_CONSOLE_DEV);
		td->minor_start = MINOR(S390_CONSOLE_DEV);
		td->num = 1;
		td->refcount = &con3270_refcount;
		td->table = con3270_table;
		td->termios = con3270_termios;
		td->termios_locked = con3270_termios_locked;

		rc = tty_register_driver(td);
		if (rc) {
			printk(KERN_ERR
			       "con3270 registration failed with %d\n", rc);
		} else {
			con3270_major = MAJOR(S390_CONSOLE_DEV);
			if (td->proc_entry != NULL)
				td->proc_entry->mode = S_IRUGO | S_IWUGO;
		}
	}
#endif /* ifdef CONFIG_TN3270_CONSOLE */
#endif /* if LINUX_VERSION_CODE */

	return rc;
}

/*
 * tty3270_fini() -- Uninitialize linemode tubes
 */
void
tty3270_fini(void)
{
	if (tty3270_major != -1) {
		tty_unregister_driver(&tty3270_driver);
		tty3270_major = -1;
	}
#ifdef CONFIG_TN3270_CONSOLE
	if (CONSOLE_IS_3270 && con3270_major != -1) {
		tty_unregister_driver(&con3270_driver);
		con3270_major = -1;
	}
#endif
}

static int 
tty3270_open(struct tty_struct *tty, struct file *filp)
{
	tub_t *tubp;
	long flags;
	int rc;
	int cmd;

	if ((tubp = TTY2TUB(tty)) == NULL) {
		return -ENODEV;
	}

	tub_inc_use_count();
	if ((rc = tty3270_wait(tubp, &flags)) != 0)
		goto do_fail;
	if (tubp->lnopen > 0) {
		tubp->lnopen++;
		TUBUNLOCK(tubp->irq, flags);
		return 0;
	}
	if (tubp->flags & TUB_OPEN_STET) {
		cmd = TBC_UPDLOG;
	} else {
		cmd = TBC_OPEN;
		tubp->flags &= ~TUB_SIZED;
	}
	if ((rc = tty3270_size(tubp, &flags)) != 0)
		goto do_fail;
	if ((rc = tty3270_rcl_init(tubp)) != 0)
		goto do_fail;
	if ((rc = tty3270_aid_init(tubp)) != 0)
		goto do_fail;
	if ((rc = tty3270_scl_init(tubp)) != 0)
		goto do_fail;
	tubp->mode = TBM_LN;
	tubp->intv = tty3270_int;
	tubp->tty = tty;
	tubp->lnopen = 1;
	tty->driver_data = tubp;
	tty->winsize.ws_row = tubp->geom_rows - 2;
	tty->winsize.ws_col = tubp->geom_cols;
	if (tubp->tty_input == NULL)
		tubp->tty_input = kmalloc(GEOM_INPLEN, GFP_KERNEL|GFP_DMA);
	tubp->tty_inattr = TF_INPUT;
	tubp->cmd = cmd;
	tty3270_build(tubp);
	TUBUNLOCK(tubp->irq, flags);
	return 0;

do_fail:
	tty3270_scl_fini(tubp);
	tty3270_aid_fini(tubp);
	tty3270_rcl_fini(tubp);
	TUBUNLOCK(tubp->irq, flags);
	tub_dec_use_count();
	return rc;
}

static void
tty3270_close(struct tty_struct *tty, struct file *filp)
{
	tub_t *tubp;
	long flags;

	if ((tubp = tty->driver_data) == NULL)
		return;

	tty3270_wait(tubp, &flags);
	if (--tubp->lnopen > 0)
		goto do_return;
	tubp->tty = NULL;
	tty->driver_data = NULL;
	tty3270_aid_fini(tubp);
	tty3270_rcl_fini(tubp);
	tty3270_scl_fini(tubp);
do_return:
	tub_dec_use_count();
	TUBUNLOCK(tubp->irq, flags);
}

static int 
tty3270_write(struct tty_struct *tty, int fromuser,
		const unsigned char *buf, int count)
{
	tub_t *tubp;
	long flags;
	bcb_t obcb;
	int rc = 0;

	if ((tubp = tty->driver_data) == NULL)
		return -1;

#ifdef CONFIG_TN3270_CONSOLE
	if (CONSOLE_IS_3270 && tub3270_con_tubp == tubp)
		tub3270_con_copy(tubp);
#endif /* CONFIG_TN3270_CONSOLE */

	obcb.bc_buf = (char *)buf;
	obcb.bc_len = obcb.bc_cnt = obcb.bc_wr = count;
	obcb.bc_rd = 0;

	TUBLOCK(tubp->irq, flags);
	rc = tub3270_movedata(&obcb, &tubp->tty_bcb, fromuser);
	tty3270_try_logging(tubp);
	TUBUNLOCK(tubp->irq, flags);
	return rc;
} 

static void
tty3270_put_char(struct tty_struct *tty, unsigned char ch)
{
	long flags;
	tub_t *tubp;
	bcb_t *ob;

	if ((tubp = tty->driver_data) == NULL)
		return;

	TUBLOCK(tubp->irq, flags);
	ob = &tubp->tty_bcb;
	if (ob->bc_cnt < ob->bc_len) {
		ob->bc_buf[ob->bc_wr++] = ch;
		if (ob->bc_wr == ob->bc_len)
			ob->bc_wr = 0;
		ob->bc_cnt++;
	}
	tty3270_try_logging(tubp);
	TUBUNLOCK(tubp->irq, flags);
}

static void
tty3270_flush_chars(struct tty_struct *tty)
{
	tub_t *tubp;
	long flags;

	if ((tubp = tty->driver_data) == NULL)
		return;

	TUBLOCK(tubp->irq, flags);
	tty3270_try_logging(tubp);
	TUBUNLOCK(tubp->irq, flags);
}

static int 
tty3270_write_room(struct tty_struct *tty)
{
	tub_t *tubp;
	bcb_t *ob;

	if ((tubp = tty->driver_data) == NULL)
		return -1;

	ob = &tubp->tty_bcb;
	return ob->bc_len - ob->bc_cnt;
}

static int
tty3270_chars_in_buffer(struct tty_struct *tty)
{
	tub_t *tubp;
	bcb_t *ob;

	if ((tubp = tty->driver_data) == NULL)
		return -1;

	ob = &tubp->tty_bcb;
	return ob->bc_cnt;
}

static int
tty3270_ioctl(struct tty_struct *tty, struct file *file,
		unsigned int cmd, unsigned long arg)
{
	tub_t *tubp;
	long flags;
	int ret = 0;
	struct termios termios;

	if ((tubp = tty->driver_data) == NULL)
		return -ENODEV;

	TUBLOCK(tubp->irq, flags);
	if (tty->flags * (1 << TTY_IO_ERROR)) {
		ret = -EIO;
		goto do_return;
	}
	switch(cmd) {
	case TCGETS:
		ret = -ENOIOCTLCMD;
		goto do_return;
	case TCFLSH:            /* arg:  2 or 0 */
		ret = -ENOIOCTLCMD;
		goto do_return;
	case TCSETSF:
		if (user_termios_to_kernel_termios(&termios,
		    (struct termios *)arg)) {
			ret = -EFAULT;
			goto do_return;
		}
		ret = -ENOIOCTLCMD;
		goto do_return;
	case TCGETA:
		ret = -ENOIOCTLCMD;
		goto do_return;
	case TCSETA:
		if (user_termio_to_kernel_termios(&termios,
		    (struct termio *)arg)) {
			ret = -EFAULT;
			goto do_return;
		}
		ret = -ENOIOCTLCMD;
		goto do_return;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

do_return:
	TUBUNLOCK(tubp->irq, flags);
	return ret;
}

static void
tty3270_set_termios(struct tty_struct *tty, struct termios *old)
{
	tub_t *tubp;
	long flags;
	int new;

	if ((tubp = tty->driver_data) == NULL)
		return;

	if (tty3270_wait(tubp, &flags) != 0) {
		TUBUNLOCK(tubp->irq, flags);
		return;
	}
	new = L_ICANON(tty)? L_ECHO(tty)? TF_INPUT: TF_INPUTN:
		tubp->tty_inattr;
	if (new != tubp->tty_inattr) {
		tubp->tty_inattr = new;
		tubp->cmd = TBC_CLRINPUT;
		tty3270_build(tubp);
	}

	TUBUNLOCK(tubp->irq, flags);
}

static void
tty3270_flush_buffer(struct tty_struct *tty)
{
	tub_t *tubp;
	bcb_t *ob;
	long flags;

	if ((tubp = tty->driver_data) == NULL)
		return;

	if (tubp->mode == TBM_FS && tubp->fs_pid != 0) {
		kill_proc(tubp->fs_pid, SIGHUP, 1);
	}

	if ((tubp->flags & TUB_OPEN_STET) == 0) {
		ob = &tubp->tty_bcb;
		TUBLOCK(tubp->irq, flags);
		ob->bc_rd = 0;
		ob->bc_wr = 0;
		ob->bc_cnt = 0;
		TUBUNLOCK(tubp->irq, flags);
	}
	wake_up_interruptible(&tty->write_wait);
	if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) &&
	    tty->ldisc.write_wakeup)
		(tty->ldisc.write_wakeup)(tty);
}

static int
tty3270_read_proc(char *buf, char **start, off_t off, int count,
		int *eof, void *data)
{
	tub_t *tubp;
	int begin = 0;
	int i;
	int rc;
	int len = 0;

	if (tty3270_proc_what == TW_CONFIG) {
		/*
		 * Describe the 3270 configuration in ascii lines.
		 * Line 1:		0 <fsmajor> 0
		 * Console line:	<devnum> CONSOLE <minor>
		 * Other lines:		<devnum> <ttymajor> <minor>
		 */
		len += sprintf(buf + len, "0 %d 0\n", fs3270_major);
		for (i = 1; i <= tubnummins; i++) {
			tubp = (*tubminors)[i];
#ifdef CONFIG_TN3270_CONSOLE
			if (CONSOLE_IS_3270 && tubp == tub3270_con_tubp)
				len += sprintf(buf + len, "%.4x CONSOLE %d\n",
					       tubp->devno, i);
			else
#endif
				len += sprintf(buf + len, "%.4x %d %d\n",
					       tubp->devno, tty3270_major, i);
			if (begin + len > off + count)
				break;
			if (begin + len < off) {
				begin += len;
				len = 0;
			}
		}
		if (i > tubnummins)
			*eof = 1;
		if (off >= begin + len) {
			rc = 0;
		} else {
			*start = buf + off - begin;
			rc = MIN(count, begin + len - off);
		}
		if (*eof && rc == 0)
			tty3270_proc_what = TW_BOGUS;
		return rc;
	}

	len += sprintf(buf, "There are %d devices.  fs major is %d, "
		"tty major is %d.\n", tubnummins, fs3270_major,
		tty3270_major);
	len += sprintf(buf+len, "        index=%d data=%d misc=%d\n",
		tty3270_proc_index,
		tty3270_proc_data,
		tty3270_proc_misc);

	/*
	 * Display info for the tube with minor nr in index
	 */
	len += tty3270_show_tube(tty3270_proc_index, buf+len, count-len);

	*eof = 1;
	if (off >= begin + len)
		return 0;
	*start = buf + off - begin;
	return MIN(count, begin + len - off);
}

static int
tty3270_write_proc(struct file *file, const char *buffer,
		unsigned long count, void *data)
{
	char mybuf[GEOM_MAXINPLEN];
	int mycount;
	tub_t *tubp;
	struct tty_struct *tty;
	kdev_t device;
	int rc;

	mycount = MIN(count, sizeof mybuf - 1);
	if (copy_from_user(mybuf, buffer, mycount) != 0)
		return -EFAULT;
	mybuf[mycount] = '\0';

	/*
	 * User-mode settings affect only the current tty ---
	 */
	tubp = NULL;
	tty = current->tty;
	device = tty? tty->device: 0;
	if (device) {
		if (MAJOR(device) == IBM_TTY3270_MAJOR)
			tubp = (*tubminors)[MINOR(device)];
#ifdef CONFIG_TN3270_CONSOLE
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,3,0))
		if (CONSOLE_IS_3270 && device == S390_CONSOLE_DEV)
			tubp = tub3270_con_tubp;
#endif /* LINUX_VERSION_CODE */
#endif /* CONFIG_TN3270_CONSOLE */
	}
	if (tubp) {
		if ((rc = tty3270_aid_set(tubp, mybuf, mycount + 1)))
			return rc > 0? count: rc;
		if ((rc = tty3270_rcl_set(tubp, mybuf, mycount + 1)))
			return rc > 0? count: rc;
		if ((rc = tty3270_scl_set(tubp, mybuf, mycount + 1)))
			return rc > 0? count: rc;
	}

	/*
	 * Superuser-mode settings affect the driver overall ---
	 */
	if (!suser()) {
		return -EPERM;
	} else if (strncmp(mybuf, "index=", 6) == 0) {
		tty3270_proc_index = simple_strtoul(mybuf + 6, 0,0);
		return count;
	} else if (strncmp(mybuf, "data=", 5) == 0) {
		tty3270_proc_data = simple_strtoul(mybuf + 5, 0, 0);
		return count;
	} else if (strncmp(mybuf, "misc=", 5) == 0) {
		tty3270_proc_misc = simple_strtoul(mybuf + 5, 0, 0);
		return count;
	} else if (strncmp(mybuf, "what=", 5) == 0) {
		if (strcmp(mybuf+5, "bogus") == 0)
			tty3270_proc_what = 0;
		else if (strncmp(mybuf+5, "config", 6) == 0)
			tty3270_proc_what = TW_CONFIG;
		return count;
	} else {
		return -EINVAL;
	}
}

static void
tty3270_hangup(struct tty_struct *tty)
{
	tub_t *tubp;
	extern void fs3270_release(tub_t *);

	if ((tubp = tty->driver_data) == NULL)
		return;
	tty3270_rcl_purge(tubp);
	tty3270_aid_reinit(tubp);
	fs3270_release(tubp);
}


/*
 * tty3270_bh(tubp) -- Perform back-half processing
 */
static void
tty3270_bh(void *data)
{
	tub_t *tubp;
	ioinfo_t *ioinfop;
	long flags;
	struct tty_struct *tty;

	ioinfop = ioinfo[(tubp = data)->irq];
	while (TUBTRYLOCK(tubp->irq, flags) == 0) {
		if (ioinfop->ui.flags.unready == 1)
			return;
	}
	if (ioinfop->ui.flags.unready == 1 ||
	    ioinfop->ui.flags.ready == 0)
		goto do_unlock;

	tubp->flags &= ~TUB_BHPENDING;
	tty = tubp->tty;

	if (tubp->flags & TUB_UNSOL_DE) {
		tubp->flags &= ~TUB_UNSOL_DE;
		if (tty != NULL) {
			tty_hangup(tty);
			wake_up_interruptible(&tubp->waitq);
			goto do_unlock;
		}
	}

	if (tubp->flags & TUB_IACTIVE) {        /* If read ended, */
		tty3270_do_input(tubp);
		tubp->flags &= ~TUB_IACTIVE;
	}

	if ((tubp->flags & TUB_WORKING) == 0) {
		if (tubp->flags & TUB_ATTN) {
			tty3270_start_input(tubp);
			tubp->flags &= ~TUB_ATTN;
		} else if (tty3270_try_logging(tubp) == 0) {
			wake_up_interruptible(&tubp->waitq);
		}
	}

	if (tty != NULL) {
		if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) &&
		    tty->ldisc.write_wakeup != NULL)
			(tty->ldisc.write_wakeup)(tty);
		wake_up_interruptible(&tty->write_wait);
	}
do_unlock:
	TUBUNLOCK(tubp->irq, flags);
}

/*
 * tty3270_sched_bh(tubp) -- Schedule the back half
 * Irq lock must be held on entry and remains held on exit.
 */
void
tty3270_sched_bh(tub_t *tubp)
{
	if (tubp->flags & TUB_BHPENDING)
		return;
	tubp->flags |= TUB_BHPENDING;
	tubp->tqueue.routine = tty3270_bh;
	tubp->tqueue.data = tubp;
	queue_task(&tubp->tqueue, &tq_immediate);
	mark_bh(IMMEDIATE_BH);
}

/*
 * tty3270_io() -- Perform line-mode reads and writes here
 */
int 
tty3270_io(tub_t *tubp)
{
	int rc;
	ccw1_t *ccwp;

	tubp->flags |= TUB_WORKING;
	tubp->dstat = 0;
	ccwp = &tubp->ttyccw;

	rc = do_IO(tubp->irq, ccwp, tubp->irq, 0, 0);
	return rc;
}

/*
 * tty3270_wait(tubp) -- Wait until TUB_WORKING is off
 * On entry the lock must not be held; on exit it is held.
 */
static int
tty3270_wait(tub_t *tubp, long *flags)
{
	DECLARE_WAITQUEUE(wait, current);

	TUBLOCK(tubp->irq, *flags);
	add_wait_queue(&tubp->waitq, &wait);
	while (!signal_pending(current) &&
	    (tubp->flags & TUB_WORKING) != 0) {
		current->state = TASK_INTERRUPTIBLE;
		TUBUNLOCK(tubp->irq, *flags);
		schedule();
		current->state = TASK_RUNNING;
		TUBLOCK(tubp->irq, *flags);
	}
	remove_wait_queue(&tubp->waitq, &wait);
	return signal_pending(current)? -ERESTARTSYS: 0;
}

void
tty3270_int(tub_t *tubp, devstat_t *dsp)
{
#define	DEV_UE_BUSY \
	(DEV_STAT_CHN_END | DEV_STAT_DEV_END | DEV_STAT_UNIT_EXCEP)
#define DEV_NOT_WORKING \
	(DEV_STAT_ATTENTION | DEV_STAT_DEV_END | DEV_STAT_UNIT_CHECK)

	tubp->dstat = dsp->dstat;

	/* Handle CE-DE-UE and subsequent UDE */
	if (dsp->dstat == DEV_UE_BUSY) {
		tubp->flags |= TUB_UE_BUSY;
		return;
	} else if (tubp->flags & TUB_UE_BUSY) {
		tubp->flags &= ~TUB_UE_BUSY;
		if (dsp->dstat == DEV_STAT_DEV_END &&
		    (tubp->flags & TUB_WORKING) != 0) {
			tty3270_io(tubp);
			return;
		}
	}

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
	if (dsp->dstat & DEV_NOT_WORKING)
		tubp->flags &= ~TUB_WORKING;
	if (dsp->dstat & DEV_STAT_UNIT_CHECK)
		tubp->sense = dsp->ii.sense;
	if ((tubp->flags & TUB_WORKING) == 0)
		tty3270_sched_bh(tubp);
}

/*
 * tty3270_refresh(), called by fs3270_close() when tubp->fsopen == 0.
 * On entry, lock is held.
 */
void
tty3270_refresh(tub_t *tubp)
{
	if (tubp->lnopen) {
		tubp->mode = TBM_LN;
		tubp->intv = tty3270_int;
		tty3270_scl_resettimer(tubp);
		tubp->cmd = TBC_UPDATE;
		tty3270_build(tubp);
	}
}

int
tty3270_try_logging(tub_t *tubp)
{
	if (tubp->flags & TUB_WORKING)
		return 0;
	if (tubp->mode == TBM_FS)
		return 0;
	if (tubp->stat == TBS_HOLD)
		return 0;
	if (tubp->stat == TBS_MORE)
		return 0;
#ifdef CONFIG_TN3270_CONSOLE
	if (CONSOLE_IS_3270 && tub3270_con_tubp == tubp)
		tub3270_con_copy(tubp);
#endif /* CONFIG_TN3270_CONSOLE */
	if (tubp->tty_bcb.bc_cnt == 0)
		return 0;
	if (tubp->intv != tty3270_int)
		return 0;
	tubp->cmd = TBC_UPDLOG;
	return tty3270_build(tubp);
}

/* tty3270 utility functions */

static void
tty3270_start_input(tub_t *tubp)
{
	if (tubp->tty_input == NULL)
		return;
	tubp->ttyccw.cda = virt_to_phys(tubp->tty_input);
	tubp->ttyccw.cmd_code = TC_READMOD;
	tubp->ttyccw.count = GEOM_INPLEN;
	tubp->ttyccw.flags = CCW_FLAG_SLI;
	tty3270_io(tubp);
	tubp->flags |= TUB_IACTIVE;
}

static void
tty3270_do_input(tub_t *tubp)
{
	int count;
	char *in;
	int aidflags;
	char *aidstring;

	count = GEOM_INPLEN - tubp->cswl;
	if ((in = tubp->tty_input) == NULL)
		goto do_build;
	tty3270_aid_get(tubp, in[0], &aidflags, &aidstring);

	if (aidflags & TA_CLEARKEY) {
		tubp->stat = TBS_RUNNING;
		tty3270_scl_resettimer(tubp);
		tubp->cmd = TBC_UPDATE;
	} else if (aidflags & TA_CLEARLOG) {
		tubp->stat = TBS_RUNNING;
		tty3270_scl_resettimer(tubp);
		tubp->cmd = TBC_CLRUPDLOG;
	} else if (aidflags & TA_DOENTER) {
		if (count <= 6) {
			switch(tubp->stat) {
			case TBS_MORE:
				tubp->stat = TBS_HOLD;
				tty3270_scl_resettimer(tubp);
				break;
			case TBS_HOLD:
				tubp->stat = TBS_MORE;
				tty3270_scl_settimer(tubp);
				break;
			case TBS_RUNNING:
                                tty3270_do_enter(tubp, in + 6, 0);
				break;
			}
			tubp->cmd = TBC_UPDSTAT;
			goto do_build;
		}
		in += 6;
		count -= 6;
		TUB_EBCASC(in, count);
		tubp->cmd = TBC_CLRINPUT;
		tty3270_do_enter(tubp, in, count);
	} else if ((aidflags & TA_DOSTRING) != 0 && aidstring != NULL) {
		tubp->cmd = TBC_KRUPDLOG;
		tty3270_do_enter(tubp, aidstring, strlen(aidstring));
	} else if ((aidflags & TA_DOSTRINGD) != 0 && aidstring != NULL) {
		tty3270_do_showi(tubp, aidstring, strlen(aidstring));
		tubp->cmd = TBC_UPDINPUT;
	} else {
		if (in[0] != 0x60)
			tubp->flags |= TUB_ALARM;
		tubp->cmd = TBC_KRUPDLOG;
	}
do_build:
	tty3270_build(tubp);
}

static void
tty3270_do_enter(tub_t *tubp, char *cp, int count)
{
	struct tty_struct *tty;
	int func = -1;

	if ((tty = tubp->tty) == NULL)
		return;
	if (count < 0)
		return;
	if (count == 2 && (cp[0] == '^' || cp[0] == '\252')) {
		switch(cp[1]) {
		case 'c':  case 'C':
			func = INTR_CHAR(tty);
			break;
		case 'd':  case 'D':
			func = EOF_CHAR(tty);
			break;
		case 'z':  case 'Z':
			func = SUSP_CHAR(tty);
			break;
		}
	} else if (count == 2 && cp[0] == 0x1b) {        /* if ESC */
		int inc = 0;
		char buf[GEOM_INPLEN + 1];
		int len;

		switch(cp[1]) {
		case 'k':  case 'K':
			inc = -1;
			break;
		case 'j':  case 'J':
			inc = 1;
			break;
		}
		if (inc == 0)
			goto not_rcl;
		len = tty3270_rcl_get(tubp, buf, sizeof buf, inc);
		if (len == 0) {
			tubp->flags |= TUB_ALARM;
			return;
		}
		tty3270_do_showi(tubp, buf, len);
		tubp->cmd = TBC_UPDINPUT;
		return;
	}
not_rcl:
	if (func != -1) {
		*tty->flip.flag_buf_ptr++ = TTY_NORMAL;
		*tty->flip.char_buf_ptr++ = func;
		tty->flip.count++;
	} else {
		tty3270_rcl_put(tubp, cp, count);
		memcpy(tty->flip.char_buf_ptr, cp, count);
		/* Add newline unless line ends with "^n" */
		if (count < 2 || cp[count - 1] != 'n' ||
		    (cp[count - 2] != '^' && cp[count - 2] != '\252')) {
			tty->flip.char_buf_ptr[count] = '\n';
			count++;
		} else {
			count -= 2;     /* Lop trailing "^n" from text */
		}
		memset(tty->flip.flag_buf_ptr, TTY_NORMAL, count);
		tty->flip.char_buf_ptr += count;
		tty->flip.flag_buf_ptr += count;
		tty->flip.count += count;
	}
	tty_flip_buffer_push(tty);
}

static void
tty3270_do_showi(tub_t *tubp, char *cp, int cl)
{
	if (cl > GEOM_INPLEN)
		cl = GEOM_INPLEN;
	memset(tubp->tty_input, 0, GEOM_INPLEN);
	memcpy(tubp->tty_input, cp, cl);
	TUB_ASCEBC(tubp->tty_input, cl);
}



/* Debugging routine */
static int
tty3270_show_tube(int minor, char *buf, int count)
{
	tub_t *tubp;
	struct tty_struct *tty;
	struct termios *mp;
	int len;

/*012345678901234567890123456789012345678901234567890123456789       */
/*Info for tub_t[dd] at xxxxxxxx:                                    */
/*    geom:  rows=dd cols=dd model=d                                 */
/*    lnopen=dd     fsopen=dd   waitq=xxxxxxxx                       */
/*    dstat=xx      mode=dd     stat=dd     flags=xxxx               */
/*    oucount=dddd  ourd=ddddd  ouwr=ddddd  nextlogx=ddddd           */
/*    tty=xxxxxxxx                                                   */
/*    write_wait=xxxxxxxx read_wait=xxxxxxxx                         */
/*    iflag=xxxxxxxx oflag=xxxxxxxx cflag=xxxxxxxx lflag=xxxxxxxx    */

	if (minor < 0 || minor > tubnummins ||
	    (tubp = (*tubminors)[minor]) == NULL)
		return sprintf(buf, "No tube at index=%d\n", minor);
	
	tty = tubp->tty;
	len = 0;

	len += sprintf(buf+len, "Info for tub_t[%d] at %p:\n", minor, tubp);

	len += sprintf(buf+len, "inattr is at %p\n", &tubp->tty_inattr);


	len += sprintf(buf+len, "    geom:  rows=%.2d cols=%.2d model=%.1d\n",
		       tubp->geom_rows, tubp->geom_cols, tubp->tubiocb.model);

	len += sprintf(buf+len,
		       "    lnopen=%-2d     fsopen=%-2d   waitq=%p\n",
		       tubp->lnopen, tubp->fsopen, &tubp->waitq);

	len += sprintf(buf+len, "    dstat=%.2x      mode=%-2d     "
		       "stat=%-2d     flags=%-4x\n", tubp->dstat,
		       tubp->mode, tubp->stat, tubp->flags);

#ifdef RBH_FIXTHIS
	len += sprintf(buf+len,
		       "    oucount=%-4d  ourd=%-5d  ouwr=%-5d"
		       "  nextlogx=%-5d\n", tubp->tty_oucount,
		       tubp->tty_ourd, tubp->tty_ouwr, tubp->tty_nextlogx);
#endif

	len += sprintf(buf+len, "    tty=%p\n",tubp->tty);

	if (tty)
		len += sprintf(buf+len,
				"    write_wait=%p read_wait=%p\n",
				&tty->write_wait, &tty->read_wait);

	if (tty && ((mp = tty->termios)))
		len += sprintf(buf+len,"    iflag=%.8x oflag=%.8x "
			       "cflag=%.8x lflag=%.8x\n", mp->c_iflag,
			       mp->c_oflag, mp->c_cflag, mp->c_lflag);


	return len;
}
