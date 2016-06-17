/*
 *  linux/drivers/char/pty.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *
 *  Added support for a Unix98-style ptmx device.
 *    -- C. Scott Ananian <cananian@alumni.princeton.edu>, 14-Jan-1998
 *  Added TTY_DO_WRITE_WAKEUP to enable n_tty to send POLL_OUT to
 *      waiting writers -- Sapan Bhatia <sapan@corewars.org>
 *
 *
 */

#include <linux/config.h>
#include <linux/module.h>	/* For EXPORT_SYMBOL */

#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/fcntl.h>
#include <linux/string.h>
#include <linux/major.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/devfs_fs_kernel.h>

#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/bitops.h>

#define BUILDING_PTY_C 1
#include <linux/devpts_fs.h>

struct pty_struct {
	int	magic;
	wait_queue_head_t open_wait;
};

#define PTY_MAGIC 0x5001

static struct tty_driver pty_driver, pty_slave_driver;
static int pty_refcount;

/* Note: one set of tables for BSD and one for Unix98 */
static struct tty_struct *pty_table[NR_PTYS];
static struct termios *pty_termios[NR_PTYS];
static struct termios *pty_termios_locked[NR_PTYS];
static struct tty_struct *ttyp_table[NR_PTYS];
static struct termios *ttyp_termios[NR_PTYS];
static struct termios *ttyp_termios_locked[NR_PTYS];
static struct pty_struct pty_state[NR_PTYS];

#ifdef CONFIG_UNIX98_PTYS
/* These are global because they are accessed in tty_io.c */
struct tty_driver ptm_driver[UNIX98_NR_MAJORS];
struct tty_driver pts_driver[UNIX98_NR_MAJORS];

static struct tty_struct *ptm_table[UNIX98_NR_MAJORS][NR_PTYS];
static struct termios *ptm_termios[UNIX98_NR_MAJORS][NR_PTYS];
static struct termios *ptm_termios_locked[UNIX98_NR_MAJORS][NR_PTYS];
static struct tty_struct *pts_table[UNIX98_NR_MAJORS][NR_PTYS];
static struct termios *pts_termios[UNIX98_NR_MAJORS][NR_PTYS];
static struct termios *pts_termios_locked[UNIX98_NR_MAJORS][NR_PTYS];
static struct pty_struct ptm_state[UNIX98_NR_MAJORS][NR_PTYS];
#endif

#define MIN(a,b)	((a) < (b) ? (a) : (b))

static void pty_close(struct tty_struct * tty, struct file * filp)
{
	if (!tty)
		return;
	if (tty->driver.subtype == PTY_TYPE_MASTER) {
		if (tty->count > 1)
			printk("master pty_close: count = %d!!\n", tty->count);
	} else {
		if (tty->count > 2)
			return;
	}
	wake_up_interruptible(&tty->read_wait);
	wake_up_interruptible(&tty->write_wait);
	tty->packet = 0;
	if (!tty->link)
		return;
	tty->link->packet = 0;
	wake_up_interruptible(&tty->link->read_wait);
	wake_up_interruptible(&tty->link->write_wait);
	set_bit(TTY_OTHER_CLOSED, &tty->link->flags);
	if (tty->driver.subtype == PTY_TYPE_MASTER) {
		set_bit(TTY_OTHER_CLOSED, &tty->flags);
#ifdef CONFIG_UNIX98_PTYS
		{
			unsigned int major = MAJOR(tty->device) - UNIX98_PTY_MASTER_MAJOR;
			if ( major < UNIX98_NR_MAJORS ) {
				devpts_pty_kill( MINOR(tty->device)
			  - tty->driver.minor_start + tty->driver.name_base );
			}
		}
#endif
		tty_unregister_devfs (&tty->link->driver, MINOR (tty->device));
		tty_vhangup(tty->link);
	}
}

/*
 * The unthrottle routine is called by the line discipline to signal
 * that it can receive more characters.  For PTY's, the TTY_THROTTLED
 * flag is always set, to force the line discipline to always call the
 * unthrottle routine when there are fewer than TTY_THRESHOLD_UNTHROTTLE 
 * characters in the queue.  This is necessary since each time this
 * happens, we need to wake up any sleeping processes that could be
 * (1) trying to send data to the pty, or (2) waiting in wait_until_sent()
 * for the pty buffer to be drained.
 */
static void pty_unthrottle(struct tty_struct * tty)
{
	struct tty_struct *o_tty = tty->link;

	if (!o_tty)
		return;

	if ((o_tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) &&
	    o_tty->ldisc.write_wakeup)
		(o_tty->ldisc.write_wakeup)(o_tty);
	wake_up_interruptible(&o_tty->write_wait);
	set_bit(TTY_THROTTLED, &tty->flags);
}

/*
 * WSH 05/24/97: modified to 
 *   (1) use space in tty->flip instead of a shared temp buffer
 *	 The flip buffers aren't being used for a pty, so there's lots
 *	 of space available.  The buffer is protected by a per-pty
 *	 semaphore that should almost never come under contention.
 *   (2) avoid redundant copying for cases where count >> receive_room
 * N.B. Calls from user space may now return an error code instead of
 * a count.
 */
static int pty_write(struct tty_struct * tty, int from_user,
		       const unsigned char *buf, int count)
{
	struct tty_struct *to = tty->link;
	int	c=0, n, room;
	char	*temp_buffer;

	if (!to || tty->stopped)
		return 0;

	if (from_user) {
		down(&tty->flip.pty_sem);
		temp_buffer = &tty->flip.char_buf[0];
		while (count > 0) {
			/* check space so we don't copy needlessly */ 
			n = to->ldisc.receive_room(to);
			if (n > count)
				n = count;
			if (!n) break;

			n  = MIN(n, PTY_BUF_SIZE);
			n -= copy_from_user(temp_buffer, buf, n);
			if (!n) {
				if (!c)
					c = -EFAULT;
				break;
			}

			/* check again in case the buffer filled up */
			room = to->ldisc.receive_room(to);
			if (n > room)
				n = room;
			if (!n) break;
			buf   += n; 
			c     += n;
			count -= n;
			to->ldisc.receive_buf(to, temp_buffer, 0, n);
		}
		up(&tty->flip.pty_sem);
	} else {
		c = to->ldisc.receive_room(to);
		if (c > count)
			c = count;
		to->ldisc.receive_buf(to, buf, 0, c);
	}
	
	return c;
}

static int pty_write_room(struct tty_struct *tty)
{
	struct tty_struct *to = tty->link;

	if (!to || tty->stopped)
		return 0;

	return to->ldisc.receive_room(to);
}

/*
 *	WSH 05/24/97:  Modified for asymmetric MASTER/SLAVE behavior
 *	The chars_in_buffer() value is used by the ldisc select() function 
 *	to hold off writing when chars_in_buffer > WAKEUP_CHARS (== 256).
 *	The pty driver chars_in_buffer() Master/Slave must behave differently:
 *
 *      The Master side needs to allow typed-ahead commands to accumulate
 *      while being canonicalized, so we report "our buffer" as empty until
 *	some threshold is reached, and then report the count. (Any count >
 *	WAKEUP_CHARS is regarded by select() as "full".)  To avoid deadlock 
 *	the count returned must be 0 if no canonical data is available to be 
 *	read. (The N_TTY ldisc.chars_in_buffer now knows this.)
 *  
 *	The Slave side passes all characters in raw mode to the Master side's
 *	buffer where they can be read immediately, so in this case we can
 *	return the true count in the buffer.
 */
static int pty_chars_in_buffer(struct tty_struct *tty)
{
	struct tty_struct *to = tty->link;
	int count;

	if (!to || !to->ldisc.chars_in_buffer)
		return 0;

	/* The ldisc must report 0 if no characters available to be read */
	count = to->ldisc.chars_in_buffer(to);

	if (tty->driver.subtype == PTY_TYPE_SLAVE) return count;

	/* Master side driver ... if the other side's read buffer is less than 
	 * half full, return 0 to allow writers to proceed; otherwise return
	 * the count.  This leaves a comfortable margin to avoid overflow, 
	 * and still allows half a buffer's worth of typed-ahead commands.
	 */
	return ((count < N_TTY_BUF_SIZE/2) ? 0 : count);
}

/* 
 * Return the device number of a Unix98 PTY (only!).  This lets us open a
 * master pty with the multi-headed ptmx device, then find out which
 * one we got after it is open, with an ioctl.
 */
#ifdef CONFIG_UNIX98_PTYS
static int pty_get_device_number(struct tty_struct *tty, unsigned int *value)
{
	unsigned int result = MINOR(tty->device)
		- tty->driver.minor_start + tty->driver.name_base;
	return put_user(result, value);
}
#endif

/* Set the lock flag on a pty */
static int pty_set_lock(struct tty_struct *tty, int * arg)
{
	int val;
	if (get_user(val,arg))
		return -EFAULT;
	if (val)
		set_bit(TTY_PTY_LOCK, &tty->flags);
	else
		clear_bit(TTY_PTY_LOCK, &tty->flags);
	return 0;
}

static int pty_bsd_ioctl(struct tty_struct *tty, struct file *file,
			unsigned int cmd, unsigned long arg)
{
	if (!tty) {
		printk("pty_ioctl called with NULL tty!\n");
		return -EIO;
	}
	switch(cmd) {
	case TIOCSPTLCK: /* Set PT Lock (disallow slave open) */
		return pty_set_lock(tty, (int *) arg);
	}
	return -ENOIOCTLCMD;
}

#ifdef CONFIG_UNIX98_PTYS
static int pty_unix98_ioctl(struct tty_struct *tty, struct file *file,
			    unsigned int cmd, unsigned long arg)
{
	if (!tty) {
		printk("pty_unix98_ioctl called with NULL tty!\n");
		return -EIO;
	}
	switch(cmd) {
	case TIOCGPTN: /* Get PT Number */
		return pty_get_device_number(tty, (unsigned int *)arg);
	}

	return pty_bsd_ioctl(tty,file,cmd,arg);
}
#endif

static void pty_flush_buffer(struct tty_struct *tty)
{
	struct tty_struct *to = tty->link;
	
	if (!to)
		return;
	
	if (to->ldisc.flush_buffer)
		to->ldisc.flush_buffer(to);
	
	if (to->packet) {
		tty->ctrl_status |= TIOCPKT_FLUSHWRITE;
		wake_up_interruptible(&to->read_wait);
	}
}

static int pty_open(struct tty_struct *tty, struct file * filp)
{
	int	retval;
	int	line;
	struct	pty_struct *pty;

	retval = -ENODEV;
	if (!tty || !tty->link)
		goto out;
	line = MINOR(tty->device) - tty->driver.minor_start;
	if ((line < 0) || (line >= NR_PTYS))
		goto out;
	pty = (struct pty_struct *)(tty->driver.driver_state) + line;
	tty->driver_data = pty;

	retval = -EIO;
	if (test_bit(TTY_OTHER_CLOSED, &tty->flags))
		goto out;
	if (test_bit(TTY_PTY_LOCK, &tty->link->flags))
		goto out;
	if (tty->link->count != 1)
		goto out;

	clear_bit(TTY_OTHER_CLOSED, &tty->link->flags);
	wake_up_interruptible(&pty->open_wait);
	set_bit(TTY_THROTTLED, &tty->flags);
	set_bit(TTY_DO_WRITE_WAKEUP, &tty->flags);

	/*  Register a slave for the master  */
	if (tty->driver.major == PTY_MASTER_MAJOR)
		tty_register_devfs(&tty->link->driver,
				   DEVFS_FL_CURRENT_OWNER | DEVFS_FL_WAIT,
				   tty->link->driver.minor_start +
				   MINOR(tty->device)-tty->driver.minor_start);
	retval = 0;
out:
	return retval;
}

static void pty_set_termios(struct tty_struct *tty, struct termios *old_termios)
{
        tty->termios->c_cflag &= ~(CSIZE | PARENB);
        tty->termios->c_cflag |= (CS8 | CREAD);
}

int __init pty_init(void)
{
	int i;

	/* Traditional BSD devices */

	memset(&pty_state, 0, sizeof(pty_state));
	for (i = 0; i < NR_PTYS; i++)
		init_waitqueue_head(&pty_state[i].open_wait);
	memset(&pty_driver, 0, sizeof(struct tty_driver));
	pty_driver.magic = TTY_DRIVER_MAGIC;
	pty_driver.driver_name = "pty_master";
#ifdef CONFIG_DEVFS_FS
	pty_driver.name = "pty/m%d";
#else
	pty_driver.name = "pty";
#endif
	pty_driver.major = PTY_MASTER_MAJOR;
	pty_driver.minor_start = 0;
	pty_driver.num = NR_PTYS;
	pty_driver.type = TTY_DRIVER_TYPE_PTY;
	pty_driver.subtype = PTY_TYPE_MASTER;
	pty_driver.init_termios = tty_std_termios;
	pty_driver.init_termios.c_iflag = 0;
	pty_driver.init_termios.c_oflag = 0;
	pty_driver.init_termios.c_cflag = B38400 | CS8 | CREAD;
	pty_driver.init_termios.c_lflag = 0;
	pty_driver.flags = TTY_DRIVER_RESET_TERMIOS | TTY_DRIVER_REAL_RAW;
	pty_driver.refcount = &pty_refcount;
	pty_driver.table = pty_table;
	pty_driver.termios = pty_termios;
	pty_driver.termios_locked = pty_termios_locked;
	pty_driver.driver_state = pty_state;
	pty_driver.other = &pty_slave_driver;

	pty_driver.open = pty_open;
	pty_driver.close = pty_close;
	pty_driver.write = pty_write;
	pty_driver.write_room = pty_write_room;
	pty_driver.flush_buffer = pty_flush_buffer;
	pty_driver.chars_in_buffer = pty_chars_in_buffer;
	pty_driver.unthrottle = pty_unthrottle;
	pty_driver.set_termios = pty_set_termios;

	pty_slave_driver = pty_driver;
	pty_slave_driver.driver_name = "pty_slave";
	pty_slave_driver.proc_entry = 0;
#ifdef CONFIG_DEVFS_FS
	pty_slave_driver.name = "pty/s%d";
#else
	pty_slave_driver.name = "ttyp";
#endif
	pty_slave_driver.subtype = PTY_TYPE_SLAVE;
	pty_slave_driver.major = PTY_SLAVE_MAJOR;
	pty_slave_driver.minor_start = 0;
	pty_slave_driver.init_termios = tty_std_termios;
	pty_slave_driver.init_termios.c_cflag = B38400 | CS8 | CREAD;
	/* Slave ptys are registered when their corresponding master pty
	 * is opened, and unregistered when the pair is closed.
	 */
	pty_slave_driver.flags |= TTY_DRIVER_NO_DEVFS;
	pty_slave_driver.table = ttyp_table;
	pty_slave_driver.termios = ttyp_termios;
	pty_slave_driver.termios_locked = ttyp_termios_locked;
	pty_slave_driver.driver_state = pty_state;
	pty_slave_driver.other = &pty_driver;

	if (tty_register_driver(&pty_driver))
		panic("Couldn't register pty driver");
	if (tty_register_driver(&pty_slave_driver))
		panic("Couldn't register pty slave driver");

	/* 
	 * only the master pty gets this ioctl (which is why we
	 * assign it here, instead of up with the rest of the
	 * pty_driver initialization. <cananian@alumni.princeton.edu>
	 */
	pty_driver.ioctl = pty_bsd_ioctl;

	/* Unix98 devices */
#ifdef CONFIG_UNIX98_PTYS
	devfs_mk_dir (NULL, "pts", NULL);
	printk("pty: %d Unix98 ptys configured\n", UNIX98_NR_MAJORS*NR_PTYS);
	for ( i = 0 ; i < UNIX98_NR_MAJORS ; i++ ) {
		int j;

		ptm_driver[i] = pty_driver;
		ptm_driver[i].name = "ptm";
		ptm_driver[i].proc_entry = 0;
		ptm_driver[i].major = UNIX98_PTY_MASTER_MAJOR+i;
		ptm_driver[i].minor_start = 0;
		ptm_driver[i].name_base = i*NR_PTYS;
		ptm_driver[i].num = NR_PTYS;
		ptm_driver[i].other = &pts_driver[i];
		ptm_driver[i].flags |= TTY_DRIVER_NO_DEVFS;
		ptm_driver[i].table = ptm_table[i];
		ptm_driver[i].termios = ptm_termios[i];
		ptm_driver[i].termios_locked = ptm_termios_locked[i];
		ptm_driver[i].driver_state = ptm_state[i];

		for (j = 0; j < NR_PTYS; j++)
			init_waitqueue_head(&ptm_state[i][j].open_wait);
		
		pts_driver[i] = pty_slave_driver;
#ifdef CONFIG_DEVFS_FS
		pts_driver[i].name = "pts/%d";
#else
		pts_driver[i].name = "pts";
#endif
		pts_driver[i].proc_entry = 0;
		pts_driver[i].major = UNIX98_PTY_SLAVE_MAJOR+i;
		pts_driver[i].minor_start = 0;
		pts_driver[i].name_base = i*NR_PTYS;
		pts_driver[i].num = ptm_driver[i].num;
		pts_driver[i].other = &ptm_driver[i];
		pts_driver[i].table = pts_table[i];
		pts_driver[i].termios = pts_termios[i];
		pts_driver[i].termios_locked = pts_termios_locked[i];
		pts_driver[i].driver_state = ptm_state[i];
		
		ptm_driver[i].ioctl = pty_unix98_ioctl;
		
		if (tty_register_driver(&ptm_driver[i]))
			panic("Couldn't register Unix98 ptm driver major %d",
			      ptm_driver[i].major);
		if (tty_register_driver(&pts_driver[i]))
			panic("Couldn't register Unix98 pts driver major %d",
			      pts_driver[i].major);
	}
#endif
	return 0;
}
