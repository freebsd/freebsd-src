/*
 *  linux/drivers/char/tty_io.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

/*
 * 'tty_io.c' gives an orthogonal feeling to tty's, be they consoles
 * or rs-channels. It also implements echoing, cooked mode etc.
 *
 * Kill-line thanks to John T Kohl, who also corrected VMIN = VTIME = 0.
 *
 * Modified by Theodore Ts'o, 9/14/92, to dynamically allocate the
 * tty_struct and tty_queue structures.  Previously there was an array
 * of 256 tty_struct's which was statically allocated, and the
 * tty_queue structures were allocated at boot time.  Both are now
 * dynamically allocated only when the tty is open.
 *
 * Also restructured routines so that there is more of a separation
 * between the high-level tty routines (tty_io.c and tty_ioctl.c) and
 * the low-level tty routines (serial.c, pty.c, console.c).  This
 * makes for cleaner and more compact code.  -TYT, 9/17/92 
 *
 * Modified by Fred N. van Kempen, 01/29/93, to add line disciplines
 * which can be dynamically activated and de-activated by the line
 * discipline handling modules (like SLIP).
 *
 * NOTE: pay no attention to the line discipline code (yet); its
 * interface is still subject to change in this version...
 * -- TYT, 1/31/92
 *
 * Added functionality to the OPOST tty handling.  No delays, but all
 * other bits should be there.
 *	-- Nick Holloway <alfie@dcs.warwick.ac.uk>, 27th May 1993.
 *
 * Rewrote canonical mode and added more termios flags.
 * 	-- julian@uhunix.uhcc.hawaii.edu (J. Cowley), 13Jan94
 *
 * Reorganized FASYNC support so mouse code can share it.
 *	-- ctm@ardi.com, 9Sep95
 *
 * New TIOCLINUX variants added.
 *	-- mj@k332.feld.cvut.cz, 19-Nov-95
 * 
 * Restrict vt switching via ioctl()
 *      -- grif@cs.ucr.edu, 5-Dec-95
 *
 * Move console and virtual terminal code to more appropriate files,
 * implement CONFIG_VT and generalize console device interface.
 *	-- Marko Kohtala <Marko.Kohtala@hut.fi>, March 97
 *
 * Rewrote init_dev and release_dev to eliminate races.
 *	-- Bill Hawes <whawes@star.net>, June 97
 *
 * Added devfs support.
 *      -- C. Scott Ananian <cananian@alumni.princeton.edu>, 13-Jan-1998
 *
 * Added support for a Unix98-style ptmx device.
 *      -- C. Scott Ananian <cananian@alumni.princeton.edu>, 14-Jan-1998
 *
 * Reduced memory usage for older ARM systems
 *      -- Russell King <rmk@arm.linux.org.uk>
 *
 * Move do_SAK() into process context.  Less stack use in devfs functions.
 * alloc_tty_struct() always uses kmalloc() -- Andrew Morton <andrewm@uow.edu.eu> 17Mar01
 */

#include <linux/config.h>
#include <linux/types.h>
#include <linux/major.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/fcntl.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>
#include <linux/devpts_fs.h>
#include <linux/file.h>
#include <linux/console.h>
#include <linux/timer.h>
#include <linux/ctype.h>
#include <linux/kd.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/smp_lock.h>

#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/bitops.h>

#include <linux/kbd_kern.h>
#include <linux/vt_kern.h>
#include <linux/selection.h>
#include <linux/devfs_fs_kernel.h>

#include <linux/kmod.h>

#ifdef CONFIG_VT
extern void con_init_devfs (void);
#endif

extern void disable_early_printk(void);

#define CONSOLE_DEV MKDEV(TTY_MAJOR,0)
#define TTY_DEV MKDEV(TTYAUX_MAJOR,0)
#define SYSCONS_DEV MKDEV(TTYAUX_MAJOR,1)
#define PTMX_DEV MKDEV(TTYAUX_MAJOR,2)

#undef TTY_DEBUG_HANGUP

#define TTY_PARANOIA_CHECK 1
#define CHECK_TTY_COUNT 1

struct termios tty_std_termios;		/* for the benefit of tty drivers  */
struct tty_driver *tty_drivers;		/* linked list of tty drivers */
struct tty_ldisc ldiscs[NR_LDISCS];	/* line disc dispatch table	*/

#ifdef CONFIG_UNIX98_PTYS
extern struct tty_driver ptm_driver[];	/* Unix98 pty masters; for /dev/ptmx */
extern struct tty_driver pts_driver[];	/* Unix98 pty slaves;  for /dev/ptmx */
#endif

static void initialize_tty_struct(struct tty_struct *tty);

static ssize_t tty_read(struct file *, char *, size_t, loff_t *);
static ssize_t tty_write(struct file *, const char *, size_t, loff_t *);
static unsigned int tty_poll(struct file *, poll_table *);
static int tty_open(struct inode *, struct file *);
static int tty_release(struct inode *, struct file *);
int tty_ioctl(struct inode * inode, struct file * file,
	      unsigned int cmd, unsigned long arg);
static int tty_fasync(int fd, struct file * filp, int on);
extern int vme_scc_init (void);
extern long vme_scc_console_init(void);
extern int serial167_init(void);
extern long serial167_console_init(void);
extern void console_8xx_init(void);
extern void au1x00_serial_console_init(void);
extern int rs_8xx_init(void);
extern void mac_scc_console_init(void);
extern void hwc_console_init(void);
extern void hwc_tty_init(void);
extern void con3215_init(void);
extern void tty3215_init(void);
extern void tub3270_con_init(void);
extern void tub3270_init(void);
extern void rs285_console_init(void);
extern void sa1100_rs_console_init(void);
extern void sgi_serial_console_init(void);
extern void sn_sal_serial_console_init(void);
extern void sci_console_init(void);
extern void dec_serial_console_init(void);
extern void tx3912_console_init(void);
extern void tx3912_rs_init(void);
extern void txx927_console_init(void);
extern void txx9_rs_init(void);
extern void txx9_serial_console_init(void);
extern void sb1250_serial_console_init(void);
extern void arc_console_init(void);
extern int hvc_console_init(void);

#ifndef MIN
#define MIN(a,b)	((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a,b)	((a) < (b) ? (b) : (a))
#endif

static struct tty_struct *alloc_tty_struct(void)
{
	struct tty_struct *tty;

	tty = kmalloc(sizeof(struct tty_struct), GFP_KERNEL);
	if (tty)
		memset(tty, 0, sizeof(struct tty_struct));
	return tty;
}

static inline void free_tty_struct(struct tty_struct *tty)
{
	kfree(tty);
}

/*
 * This routine returns the name of tty.
 */
static char *
_tty_make_name(struct tty_struct *tty, const char *name, char *buf)
{
	int idx = (tty)?MINOR(tty->device) - tty->driver.minor_start:0;

	if (!tty) /* Hmm.  NULL pointer.  That's fun. */
		strcpy(buf, "NULL tty");
	else
		sprintf(buf, name,
			idx + tty->driver.name_base);
		
	return buf;
}

#define TTY_NUMBER(tty) (MINOR((tty)->device) - (tty)->driver.minor_start + \
			 (tty)->driver.name_base)

char *tty_name(struct tty_struct *tty, char *buf)
{
	return _tty_make_name(tty, (tty)?tty->driver.name:NULL, buf);
}

inline int tty_paranoia_check(struct tty_struct *tty, kdev_t device,
			      const char *routine)
{
#ifdef TTY_PARANOIA_CHECK
	static const char badmagic[] = KERN_WARNING
		"Warning: bad magic number for tty struct (%s) in %s\n";
	static const char badtty[] = KERN_WARNING
		"Warning: null TTY for (%s) in %s\n";

	if (!tty) {
		printk(badtty, kdevname(device), routine);
		return 1;
	}
	if (tty->magic != TTY_MAGIC) {
		printk(badmagic, kdevname(device), routine);
		return 1;
	}
#endif
	return 0;
}

static int check_tty_count(struct tty_struct *tty, const char *routine)
{
#ifdef CHECK_TTY_COUNT
	struct list_head *p;
	int count = 0;
	
	file_list_lock();
	for(p = tty->tty_files.next; p != &tty->tty_files; p = p->next) {
		if(list_entry(p, struct file, f_list)->private_data == tty)
			count++;
	}
	file_list_unlock();
	if (tty->driver.type == TTY_DRIVER_TYPE_PTY &&
	    tty->driver.subtype == PTY_TYPE_SLAVE &&
	    tty->link && tty->link->count)
		count++;
	if (tty->count != count) {
		printk(KERN_WARNING "Warning: dev (%s) tty->count(%d) "
				    "!= #fd's(%d) in %s\n",
		       kdevname(tty->device), tty->count, count, routine);
		return count;
       }	
#endif
	return 0;
}

int tty_register_ldisc(int disc, struct tty_ldisc *new_ldisc)
{
	if (disc < N_TTY || disc >= NR_LDISCS)
		return -EINVAL;
	
	if (new_ldisc) {
		ldiscs[disc] = *new_ldisc;
		ldiscs[disc].flags |= LDISC_FLAG_DEFINED;
		ldiscs[disc].num = disc;
	} else
		memset(&ldiscs[disc], 0, sizeof(struct tty_ldisc));
	
	return 0;
}

EXPORT_SYMBOL(tty_register_ldisc);

/* Set the discipline of a tty line. */
static int tty_set_ldisc(struct tty_struct *tty, int ldisc)
{
	int	retval = 0;
	struct	tty_ldisc o_ldisc;
	char buf[64];

	if ((ldisc < N_TTY) || (ldisc >= NR_LDISCS))
		return -EINVAL;
	/* Eduardo Blanco <ejbs@cs.cs.com.uy> */
	/* Cyrus Durgin <cider@speakeasy.org> */
	if (!(ldiscs[ldisc].flags & LDISC_FLAG_DEFINED)) {
		char modname [20];
		sprintf(modname, "tty-ldisc-%d", ldisc);
		request_module (modname);
	}
	if (!(ldiscs[ldisc].flags & LDISC_FLAG_DEFINED))
		return -EINVAL;

	if (tty->ldisc.num == ldisc)
		return 0;	/* We are already in the desired discipline */
	o_ldisc = tty->ldisc;

	tty_wait_until_sent(tty, 0);
	
	/* Shutdown the current discipline. */
	if (tty->ldisc.close)
		(tty->ldisc.close)(tty);

	/* Now set up the new line discipline. */
	tty->ldisc = ldiscs[ldisc];
	tty->termios->c_line = ldisc;
	if (tty->ldisc.open)
		retval = (tty->ldisc.open)(tty);
	if (retval < 0) {
		tty->ldisc = o_ldisc;
		tty->termios->c_line = tty->ldisc.num;
		if (tty->ldisc.open && (tty->ldisc.open(tty) < 0)) {
			tty->ldisc = ldiscs[N_TTY];
			tty->termios->c_line = N_TTY;
			if (tty->ldisc.open) {
				int r = tty->ldisc.open(tty);

				if (r < 0)
					panic("Couldn't open N_TTY ldisc for "
					      "%s --- error %d.",
					      tty_name(tty, buf), r);
			}
		}
	}
	if (tty->ldisc.num != o_ldisc.num && tty->driver.set_ldisc)
		tty->driver.set_ldisc(tty);
	return retval;
}

/*
 * This routine returns a tty driver structure, given a device number
 */
struct tty_driver *get_tty_driver(kdev_t device)
{
	int	major, minor;
	struct tty_driver *p;
	
	minor = MINOR(device);
	major = MAJOR(device);

	for (p = tty_drivers; p; p = p->next) {
		if (p->major != major)
			continue;
		if (minor < p->minor_start)
			continue;
		if (minor >= p->minor_start + p->num)
			continue;
		return p;
	}
	return NULL;
}

/*
 * If we try to write to, or set the state of, a terminal and we're
 * not in the foreground, send a SIGTTOU.  If the signal is blocked or
 * ignored, go ahead and perform the operation.  (POSIX 7.2)
 */
int tty_check_change(struct tty_struct * tty)
{
	if (current->tty != tty)
		return 0;
	if (tty->pgrp <= 0) {
		printk(KERN_WARNING "tty_check_change: tty->pgrp <= 0!\n");
		return 0;
	}
	if (current->pgrp == tty->pgrp)
		return 0;
	if (is_ignored(SIGTTOU))
		return 0;
	if (is_orphaned_pgrp(current->pgrp))
		return -EIO;
	(void) kill_pg(current->pgrp,SIGTTOU,1);
	return -ERESTARTSYS;
}

static ssize_t hung_up_tty_read(struct file * file, char * buf,
				size_t count, loff_t *ppos)
{
	/* Can't seek (pread) on ttys.  */
	if (ppos != &file->f_pos)
		return -ESPIPE;
	return 0;
}

static ssize_t hung_up_tty_write(struct file * file, const char * buf,
				 size_t count, loff_t *ppos)
{
	/* Can't seek (pwrite) on ttys.  */
	if (ppos != &file->f_pos)
		return -ESPIPE;
	return -EIO;
}

/* No kernel lock held - none needed ;) */
static unsigned int hung_up_tty_poll(struct file * filp, poll_table * wait)
{
	return POLLIN | POLLOUT | POLLERR | POLLHUP | POLLRDNORM | POLLWRNORM;
}

static int hung_up_tty_ioctl(struct inode * inode, struct file * file,
			     unsigned int cmd, unsigned long arg)
{
	return cmd == TIOCSPGRP ? -ENOTTY : -EIO;
}

static struct file_operations tty_fops = {
	llseek:		no_llseek,
	read:		tty_read,
	write:		tty_write,
	poll:		tty_poll,
	ioctl:		tty_ioctl,
	open:		tty_open,
	release:	tty_release,
	fasync:		tty_fasync,
};

static struct file_operations hung_up_tty_fops = {
	llseek:		no_llseek,
	read:		hung_up_tty_read,
	write:		hung_up_tty_write,
	poll:		hung_up_tty_poll,
	ioctl:		hung_up_tty_ioctl,
	release:	tty_release,
};

static spinlock_t redirect_lock = SPIN_LOCK_UNLOCKED;
static struct file *redirect;
/*
 * This can be called by the "eventd" kernel thread.  That is process synchronous,
 * but doesn't hold any locks, so we need to make sure we have the appropriate
 * locks for what we're doing..
 */
void do_tty_hangup(void *data)
{
	struct tty_struct *tty = (struct tty_struct *) data;
	struct file * cons_filp = NULL;
	struct file *f = NULL;
	struct task_struct *p;
	struct list_head *l;
	int    closecount = 0, n;

	if (!tty)
		return;

	/* inuse_filps is protected by the single kernel lock */
	lock_kernel();

	spin_lock(&redirect_lock);
	if (redirect && redirect->private_data == tty) {
		f = redirect;
		redirect = NULL;
	}
	spin_unlock(&redirect_lock);
	
	check_tty_count(tty, "do_tty_hangup");
	file_list_lock();
	for (l = tty->tty_files.next; l != &tty->tty_files; l = l->next) {
		struct file * filp = list_entry(l, struct file, f_list);
		if (filp->f_dentry->d_inode->i_rdev == CONSOLE_DEV ||
		    filp->f_dentry->d_inode->i_rdev == SYSCONS_DEV) {
			cons_filp = filp;
			continue;
		}
		if (filp->f_op != &tty_fops)
			continue;
		closecount++;
		tty_fasync(-1, filp, 0);	/* can't block */
		filp->f_op = &hung_up_tty_fops;
	}
	file_list_unlock();
	
	/* FIXME! What are the locking issues here? This may me overdoing things.. */
	{
		unsigned long flags;

		save_flags(flags); cli();
		if (tty->ldisc.flush_buffer)
			tty->ldisc.flush_buffer(tty);
		if (tty->driver.flush_buffer)
			tty->driver.flush_buffer(tty);
		if ((test_bit(TTY_DO_WRITE_WAKEUP, &tty->flags)) &&
		    tty->ldisc.write_wakeup)
			(tty->ldisc.write_wakeup)(tty);
		restore_flags(flags);
	}

	wake_up_interruptible(&tty->write_wait);
	wake_up_interruptible(&tty->read_wait);

	/*
	 * Shutdown the current line discipline, and reset it to
	 * N_TTY.
	 */
	if (tty->driver.flags & TTY_DRIVER_RESET_TERMIOS)
		*tty->termios = tty->driver.init_termios;
	if (tty->ldisc.num != ldiscs[N_TTY].num) {
		if (tty->ldisc.close)
			(tty->ldisc.close)(tty);
		tty->ldisc = ldiscs[N_TTY];
		tty->termios->c_line = N_TTY;
		if (tty->ldisc.open) {
			int i = (tty->ldisc.open)(tty);
			if (i < 0)
				printk(KERN_ERR "do_tty_hangup: N_TTY open: "
						"error %d\n", -i);
		}
	}
	
	read_lock(&tasklist_lock);
 	for_each_task(p) {
		if ((tty->session > 0) && (p->session == tty->session) &&
		    p->leader) {
			send_sig(SIGHUP,p,1);
			send_sig(SIGCONT,p,1);
			if (tty->pgrp > 0)
				p->tty_old_pgrp = tty->pgrp;
		}
		if (p->tty == tty)
			p->tty = NULL;
	}
	read_unlock(&tasklist_lock);

	tty->flags = 0;
	tty->session = 0;
	tty->pgrp = -1;
	tty->ctrl_status = 0;
	/*
	 *	If one of the devices matches a console pointer, we
	 *	cannot just call hangup() because that will cause
	 *	tty->count and state->count to go out of sync.
	 *	So we just call close() the right number of times.
	 */
	if (cons_filp) {
		if (tty->driver.close)
			for (n = 0; n < closecount; n++)
				tty->driver.close(tty, cons_filp);
	} else if (tty->driver.hangup)
		(tty->driver.hangup)(tty);
	unlock_kernel();
	if (f)
		fput(f);
}

void tty_hangup(struct tty_struct * tty)
{
#ifdef TTY_DEBUG_HANGUP
	char	buf[64];
	
	printk(KERN_DEBUG "%s hangup...\n", tty_name(tty, buf));
#endif
	schedule_task(&tty->tq_hangup);
}

void tty_vhangup(struct tty_struct * tty)
{
#ifdef TTY_DEBUG_HANGUP
	char	buf[64];

	printk(KERN_DEBUG "%s vhangup...\n", tty_name(tty, buf));
#endif
	do_tty_hangup((void *) tty);
}

int tty_hung_up_p(struct file * filp)
{
	return (filp->f_op == &hung_up_tty_fops);
}

/*
 * This function is typically called only by the session leader, when
 * it wants to disassociate itself from its controlling tty.
 *
 * It performs the following functions:
 * 	(1)  Sends a SIGHUP and SIGCONT to the foreground process group
 * 	(2)  Clears the tty from being controlling the session
 * 	(3)  Clears the controlling tty for all processes in the
 * 		session group.
 *
 * The argument on_exit is set to 1 if called when a process is
 * exiting; it is 0 if called by the ioctl TIOCNOTTY.
 */
void disassociate_ctty(int on_exit)
{
	struct tty_struct *tty = current->tty;
	struct task_struct *p;
	int tty_pgrp = -1;

	if (tty) {
		tty_pgrp = tty->pgrp;
		if (on_exit && tty->driver.type != TTY_DRIVER_TYPE_PTY)
			tty_vhangup(tty);
	} else {
		if (current->tty_old_pgrp) {
			kill_pg(current->tty_old_pgrp, SIGHUP, on_exit);
			kill_pg(current->tty_old_pgrp, SIGCONT, on_exit);
		}
		return;
	}
	if (tty_pgrp > 0) {
		kill_pg(tty_pgrp, SIGHUP, on_exit);
		if (!on_exit)
			kill_pg(tty_pgrp, SIGCONT, on_exit);
	}

	current->tty_old_pgrp = 0;
	tty->session = 0;
	tty->pgrp = -1;

	read_lock(&tasklist_lock);
	for_each_task(p)
	  	if (p->session == current->session)
			p->tty = NULL;
	read_unlock(&tasklist_lock);
}

void stop_tty(struct tty_struct *tty)
{
	if (tty->stopped)
		return;
	tty->stopped = 1;
	if (tty->link && tty->link->packet) {
		tty->ctrl_status &= ~TIOCPKT_START;
		tty->ctrl_status |= TIOCPKT_STOP;
		wake_up_interruptible(&tty->link->read_wait);
	}
	if (tty->driver.stop)
		(tty->driver.stop)(tty);
}

void start_tty(struct tty_struct *tty)
{
	if (!tty->stopped || tty->flow_stopped)
		return;
	tty->stopped = 0;
	if (tty->link && tty->link->packet) {
		tty->ctrl_status &= ~TIOCPKT_STOP;
		tty->ctrl_status |= TIOCPKT_START;
		wake_up_interruptible(&tty->link->read_wait);
	}
	if (tty->driver.start)
		(tty->driver.start)(tty);
	if ((test_bit(TTY_DO_WRITE_WAKEUP, &tty->flags)) &&
	    tty->ldisc.write_wakeup)
		(tty->ldisc.write_wakeup)(tty);
	wake_up_interruptible(&tty->write_wait);
}

static ssize_t tty_read(struct file * file, char * buf, size_t count, 
			loff_t *ppos)
{
	int i;
	struct tty_struct * tty;
	struct inode *inode;

	/* Can't seek (pread) on ttys.  */
	if (ppos != &file->f_pos)
		return -ESPIPE;

	tty = (struct tty_struct *)file->private_data;
	inode = file->f_dentry->d_inode;
	if (tty_paranoia_check(tty, inode->i_rdev, "tty_read"))
		return -EIO;
	if (!tty || (test_bit(TTY_IO_ERROR, &tty->flags)))
		return -EIO;

	/* This check not only needs to be done before reading, but also
	   whenever read_chan() gets woken up after sleeping, so I've
	   moved it to there.  This should only be done for the N_TTY
	   line discipline, anyway.  Same goes for write_chan(). -- jlc. */
#if 0
	if ((inode->i_rdev != CONSOLE_DEV) && /* don't stop on /dev/console */
	    (tty->pgrp > 0) &&
	    (current->tty == tty) &&
	    (tty->pgrp != current->pgrp))
		if (is_ignored(SIGTTIN) || is_orphaned_pgrp(current->pgrp))
			return -EIO;
		else {
			(void) kill_pg(current->pgrp, SIGTTIN, 1);
			return -ERESTARTSYS;
		}
#endif
	lock_kernel();
	if (tty->ldisc.read)
		i = (tty->ldisc.read)(tty,file,buf,count);
	else
		i = -EIO;
	unlock_kernel();
	if (i > 0)
		inode->i_atime = CURRENT_TIME;
	return i;
}

/*
 * Split writes up in sane blocksizes to avoid
 * denial-of-service type attacks
 */
static inline ssize_t do_tty_write(
	ssize_t (*write)(struct tty_struct *, struct file *, const unsigned char *, size_t),
	struct tty_struct *tty,
	struct file *file,
	const unsigned char *buf,
	size_t count)
{
	ssize_t ret = 0, written = 0;
	
	if (file->f_flags & O_NONBLOCK) {
		if (down_trylock(&tty->atomic_write))
			return -EAGAIN;
	}
	else {
		if (down_interruptible(&tty->atomic_write))
			return -ERESTARTSYS;
	}
	if ( test_bit(TTY_NO_WRITE_SPLIT, &tty->flags) ) {
		lock_kernel();
		written = write(tty, file, buf, count);
		unlock_kernel();
	} else {
		for (;;) {
			unsigned long size = MAX(PAGE_SIZE*2,16384);
			if (size > count)
				size = count;
			lock_kernel();
			ret = write(tty, file, buf, size);
			unlock_kernel();
			if (ret <= 0)
				break;
			written += ret;
			buf += ret;
			count -= ret;
			if (!count)
				break;
			ret = -ERESTARTSYS;
			if (signal_pending(current))
				break;
			if (current->need_resched)
				schedule();
		}
	}
	if (written) {
		file->f_dentry->d_inode->i_mtime = CURRENT_TIME;
		ret = written;
	}
	up(&tty->atomic_write);
	return ret;
}


static ssize_t tty_write(struct file * file, const char * buf, size_t count,
			 loff_t *ppos)
{
	int is_console;
	struct tty_struct * tty;
	struct inode *inode = file->f_dentry->d_inode;

	/* Can't seek (pwrite) on ttys.  */
	if (ppos != &file->f_pos)
		return -ESPIPE;

	/*
	 *      For now, we redirect writes from /dev/console as
	 *      well as /dev/tty0.
	 */
	inode = file->f_dentry->d_inode;
	is_console = (inode->i_rdev == SYSCONS_DEV ||
		      inode->i_rdev == CONSOLE_DEV);

	if (is_console) {
		struct file *p = NULL;

		spin_lock(&redirect_lock);
		if (redirect) {
			get_file(redirect);
			p = redirect;
		}
		spin_unlock(&redirect_lock);

		if (p) {
			ssize_t res = p->f_op->write(p, buf, count, &p->f_pos);
			fput(p);
			return res;
		}
	}

	tty = (struct tty_struct *)file->private_data;
	if (tty_paranoia_check(tty, inode->i_rdev, "tty_write"))
		return -EIO;
	if (!tty || !tty->driver.write || (test_bit(TTY_IO_ERROR, &tty->flags)))
		return -EIO;
#if 0
	if (!is_console && L_TOSTOP(tty) && (tty->pgrp > 0) &&
	    (current->tty == tty) && (tty->pgrp != current->pgrp)) {
		if (is_orphaned_pgrp(current->pgrp))
			return -EIO;
		if (!is_ignored(SIGTTOU)) {
			(void) kill_pg(current->pgrp, SIGTTOU, 1);
			return -ERESTARTSYS;
		}
	}
#endif
	if (!tty->ldisc.write)
		return -EIO;
	return do_tty_write(tty->ldisc.write, tty, file,
			    (const unsigned char *)buf, count);
}

/* Semaphore to protect creating and releasing a tty */
static DECLARE_MUTEX(tty_sem);

static void down_tty_sem(int index)
{
	down(&tty_sem);
}

static void up_tty_sem(int index)
{
	up(&tty_sem);
}

static void release_mem(struct tty_struct *tty, int idx);

/*
 * WSH 06/09/97: Rewritten to remove races and properly clean up after a
 * failed open.  The new code protects the open with a semaphore, so it's
 * really quite straightforward.  The semaphore locking can probably be
 * relaxed for the (most common) case of reopening a tty.
 */
static int init_dev(kdev_t device, struct tty_struct **ret_tty)
{
	struct tty_struct *tty, *o_tty;
	struct termios *tp, **tp_loc, *o_tp, **o_tp_loc;
	struct termios *ltp, **ltp_loc, *o_ltp, **o_ltp_loc;
	struct tty_driver *driver;	
	int retval=0;
	int idx;

	driver = get_tty_driver(device);
	if (!driver)
		return -ENODEV;

	idx = MINOR(device) - driver->minor_start;

	/* 
	 * Check whether we need to acquire the tty semaphore to avoid
	 * race conditions.  For now, play it safe.
	 */
	down_tty_sem(idx);

	/* check whether we're reopening an existing tty */
	tty = driver->table[idx];
	if (tty) goto fast_track;

	/*
	 * First time open is complex, especially for PTY devices.
	 * This code guarantees that either everything succeeds and the
	 * TTY is ready for operation, or else the table slots are vacated
	 * and the allocated memory released.  (Except that the termios 
	 * and locked termios may be retained.)
	 */

	o_tty = NULL;
	tp = o_tp = NULL;
	ltp = o_ltp = NULL;

	tty = alloc_tty_struct();
	if(!tty)
		goto fail_no_mem;
	initialize_tty_struct(tty);
	tty->device = device;
	tty->driver = *driver;

	tp_loc = &driver->termios[idx];
	if (!*tp_loc) {
		tp = (struct termios *) kmalloc(sizeof(struct termios),
						GFP_KERNEL);
		if (!tp)
			goto free_mem_out;
		*tp = driver->init_termios;
	}

	ltp_loc = &driver->termios_locked[idx];
	if (!*ltp_loc) {
		ltp = (struct termios *) kmalloc(sizeof(struct termios),
						 GFP_KERNEL);
		if (!ltp)
			goto free_mem_out;
		memset(ltp, 0, sizeof(struct termios));
	}

	if (driver->type == TTY_DRIVER_TYPE_PTY) {
		o_tty = alloc_tty_struct();
		if (!o_tty)
			goto free_mem_out;
		initialize_tty_struct(o_tty);
		o_tty->device = (kdev_t) MKDEV(driver->other->major,
					driver->other->minor_start + idx);
		o_tty->driver = *driver->other;

		o_tp_loc  = &driver->other->termios[idx];
		if (!*o_tp_loc) {
			o_tp = (struct termios *)
				kmalloc(sizeof(struct termios), GFP_KERNEL);
			if (!o_tp)
				goto free_mem_out;
			*o_tp = driver->other->init_termios;
		}

		o_ltp_loc = &driver->other->termios_locked[idx];
		if (!*o_ltp_loc) {
			o_ltp = (struct termios *)
				kmalloc(sizeof(struct termios), GFP_KERNEL);
			if (!o_ltp)
				goto free_mem_out;
			memset(o_ltp, 0, sizeof(struct termios));
		}

		/*
		 * Everything allocated ... set up the o_tty structure.
		 */
		driver->other->table[idx] = o_tty;
		if (!*o_tp_loc)
			*o_tp_loc = o_tp;
		if (!*o_ltp_loc)
			*o_ltp_loc = o_ltp;
		o_tty->termios = *o_tp_loc;
		o_tty->termios_locked = *o_ltp_loc;
		(*driver->other->refcount)++;
		if (driver->subtype == PTY_TYPE_MASTER)
			o_tty->count++;

		/* Establish the links in both directions */
		tty->link   = o_tty;
		o_tty->link = tty;
	}

	/* 
	 * All structures have been allocated, so now we install them.
	 * Failures after this point use release_mem to clean up, so 
	 * there's no need to null out the local pointers.
	 */
	driver->table[idx] = tty;
	
	if (!*tp_loc)
		*tp_loc = tp;
	if (!*ltp_loc)
		*ltp_loc = ltp;
	tty->termios = *tp_loc;
	tty->termios_locked = *ltp_loc;
	(*driver->refcount)++;
	tty->count++;

	/* 
	 * Structures all installed ... call the ldisc open routines.
	 * If we fail here just call release_mem to clean up.  No need
	 * to decrement the use counts, as release_mem doesn't care.
	 */
	if (tty->ldisc.open) {
		retval = (tty->ldisc.open)(tty);
		if (retval)
			goto release_mem_out;
	}
	if (o_tty && o_tty->ldisc.open) {
		retval = (o_tty->ldisc.open)(o_tty);
		if (retval) {
			if (tty->ldisc.close)
				(tty->ldisc.close)(tty);
			goto release_mem_out;
		}
	}
	goto success;

	/*
	 * This fast open can be used if the tty is already open.
	 * No memory is allocated, and the only failures are from
	 * attempting to open a closing tty or attempting multiple
	 * opens on a pty master.
	 */
fast_track:
	if (test_bit(TTY_CLOSING, &tty->flags)) {
		retval = -EIO;
		goto end_init;
	}
	if (driver->type == TTY_DRIVER_TYPE_PTY &&
	    driver->subtype == PTY_TYPE_MASTER) {
		/*
		 * special case for PTY masters: only one open permitted, 
		 * and the slave side open count is incremented as well.
		 */
		if (tty->count) {
			retval = -EIO;
			goto end_init;
		}
		tty->link->count++;
	}
	tty->count++;
	tty->driver = *driver; /* N.B. why do this every time?? */

success:
	*ret_tty = tty;
	
	/* All paths come through here to release the semaphore */
end_init:
	up_tty_sem(idx);
	return retval;

	/* Release locally allocated memory ... nothing placed in slots */
free_mem_out:
	if (o_tp)
		kfree(o_tp);
	if (o_tty)
		free_tty_struct(o_tty);
	if (ltp)
		kfree(ltp);
	if (tp)
		kfree(tp);
	free_tty_struct(tty);

fail_no_mem:
	retval = -ENOMEM;
	goto end_init;

	/* call the tty release_mem routine to clean out this slot */
release_mem_out:
	printk(KERN_INFO "init_dev: ldisc open failed, "
			 "clearing slot %d\n", idx);
	release_mem(tty, idx);
	goto end_init;
}

/*
 * Releases memory associated with a tty structure, and clears out the
 * driver table slots.
 */
static void release_mem(struct tty_struct *tty, int idx)
{
	struct tty_struct *o_tty;
	struct termios *tp;

	if ((o_tty = tty->link) != NULL) {
		o_tty->driver.table[idx] = NULL;
		if (o_tty->driver.flags & TTY_DRIVER_RESET_TERMIOS) {
			tp = o_tty->driver.termios[idx];
			o_tty->driver.termios[idx] = NULL;
			kfree(tp);
		}
		o_tty->magic = 0;
		(*o_tty->driver.refcount)--;
		list_del_init(&o_tty->tty_files);
		free_tty_struct(o_tty);
	}

	tty->driver.table[idx] = NULL;
	if (tty->driver.flags & TTY_DRIVER_RESET_TERMIOS) {
		tp = tty->driver.termios[idx];
		tty->driver.termios[idx] = NULL;
		kfree(tp);
	}
	tty->magic = 0;
	(*tty->driver.refcount)--;
	list_del_init(&tty->tty_files);
	free_tty_struct(tty);
}

/*
 * Even releasing the tty structures is a tricky business.. We have
 * to be very careful that the structures are all released at the
 * same time, as interrupts might otherwise get the wrong pointers.
 *
 * WSH 09/09/97: rewritten to avoid some nasty race conditions that could
 * lead to double frees or releasing memory still in use.
 */
static void release_dev(struct file * filp)
{
	struct tty_struct *tty, *o_tty;
	int	pty_master, tty_closing, o_tty_closing, do_sleep;
	int	idx;
	char	buf[64];
	
	tty = (struct tty_struct *)filp->private_data;
	if (tty_paranoia_check(tty, filp->f_dentry->d_inode->i_rdev, "release_dev"))
		return;

	check_tty_count(tty, "release_dev");

	tty_fasync(-1, filp, 0);

	idx = MINOR(tty->device) - tty->driver.minor_start;
	pty_master = (tty->driver.type == TTY_DRIVER_TYPE_PTY &&
		      tty->driver.subtype == PTY_TYPE_MASTER);
	o_tty = tty->link;

#ifdef TTY_PARANOIA_CHECK
	if (idx < 0 || idx >= tty->driver.num) {
		printk(KERN_DEBUG "release_dev: bad idx when trying to "
				  "free (%s)\n", kdevname(tty->device));
		return;
	}
	if (tty != tty->driver.table[idx]) {
		printk(KERN_DEBUG "release_dev: driver.table[%d] not tty "
				  "for (%s)\n", idx, kdevname(tty->device));
		return;
	}
	if (tty->termios != tty->driver.termios[idx]) {
		printk(KERN_DEBUG "release_dev: driver.termios[%d] not termios "
		       "for (%s)\n",
		       idx, kdevname(tty->device));
		return;
	}
	if (tty->termios_locked != tty->driver.termios_locked[idx]) {
		printk(KERN_DEBUG "release_dev: driver.termios_locked[%d] not "
		       "termios_locked for (%s)\n",
		       idx, kdevname(tty->device));
		return;
	}
#endif

#ifdef TTY_DEBUG_HANGUP
	printk(KERN_DEBUG "release_dev of %s (tty count=%d)...",
	       tty_name(tty, buf), tty->count);
#endif

#ifdef TTY_PARANOIA_CHECK
	if (tty->driver.other) {
		if (o_tty != tty->driver.other->table[idx]) {
			printk(KERN_DEBUG "release_dev: other->table[%d] "
					  "not o_tty for (%s)\n",
			       idx, kdevname(tty->device));
			return;
		}
		if (o_tty->termios != tty->driver.other->termios[idx]) {
			printk(KERN_DEBUG "release_dev: other->termios[%d] "
					  "not o_termios for (%s)\n",
			       idx, kdevname(tty->device));
			return;
		}
		if (o_tty->termios_locked != 
		      tty->driver.other->termios_locked[idx]) {
			printk(KERN_DEBUG "release_dev: other->termios_locked["
					  "%d] not o_termios_locked for (%s)\n",
			       idx, kdevname(tty->device));
			return;
		}
		if (o_tty->link != tty) {
			printk(KERN_DEBUG "release_dev: bad pty pointers\n");
			return;
		}
	}
#endif

	if (tty->driver.close)
		tty->driver.close(tty, filp);

	/*
	 * Sanity check: if tty->count is going to zero, there shouldn't be
	 * any waiters on tty->read_wait or tty->write_wait.  We test the
	 * wait queues and kick everyone out _before_ actually starting to
	 * close.  This ensures that we won't block while releasing the tty
	 * structure.
	 *
	 * The test for the o_tty closing is necessary, since the master and
	 * slave sides may close in any order.  If the slave side closes out
	 * first, its count will be one, since the master side holds an open.
	 * Thus this test wouldn't be triggered at the time the slave closes,
	 * so we do it now.
	 *
	 * Note that it's possible for the tty to be opened again while we're
	 * flushing out waiters.  By recalculating the closing flags before
	 * each iteration we avoid any problems.
	 */
	while (1) {
		tty_closing = tty->count <= 1;
		o_tty_closing = o_tty &&
			(o_tty->count <= (pty_master ? 1 : 0));
		do_sleep = 0;

		if (tty_closing) {
			if (waitqueue_active(&tty->read_wait)) {
				wake_up(&tty->read_wait);
				do_sleep++;
			}
			if (waitqueue_active(&tty->write_wait)) {
				wake_up(&tty->write_wait);
				do_sleep++;
			}
		}
		if (o_tty_closing) {
			if (waitqueue_active(&o_tty->read_wait)) {
				wake_up(&o_tty->read_wait);
				do_sleep++;
			}
			if (waitqueue_active(&o_tty->write_wait)) {
				wake_up(&o_tty->write_wait);
				do_sleep++;
			}
		}
		if (!do_sleep)
			break;

		printk(KERN_WARNING "release_dev: %s: read/write wait queue "
				    "active!\n", tty_name(tty, buf));
		schedule();
	}	

	/*
	 * The closing flags are now consistent with the open counts on 
	 * both sides, and we've completed the last operation that could 
	 * block, so it's safe to proceed with closing.
	 */
	if (pty_master) {
		if (--o_tty->count < 0) {
			printk(KERN_WARNING "release_dev: bad pty slave count "
					    "(%d) for %s\n",
			       o_tty->count, tty_name(o_tty, buf));
			o_tty->count = 0;
		}
	}
	if (--tty->count < 0) {
		printk(KERN_WARNING "release_dev: bad tty->count (%d) for %s\n",
		       tty->count, tty_name(tty, buf));
		tty->count = 0;
	}

	/*
	 * We've decremented tty->count, so we should zero out
	 * filp->private_data, to break the link between the tty and
	 * the file descriptor.  Otherwise if filp_close() blocks before
	 * the file descriptor is removed from the inuse_filp
	 * list, check_tty_count() could observe a discrepancy and
	 * printk a warning message to the user.
	 */
	filp->private_data = 0;

	/*
	 * Perform some housekeeping before deciding whether to return.
	 *
	 * Set the TTY_CLOSING flag if this was the last open.  In the
	 * case of a pty we may have to wait around for the other side
	 * to close, and TTY_CLOSING makes sure we can't be reopened.
	 */
	if(tty_closing)
		set_bit(TTY_CLOSING, &tty->flags);
	if(o_tty_closing)
		set_bit(TTY_CLOSING, &o_tty->flags);

	/*
	 * If _either_ side is closing, make sure there aren't any
	 * processes that still think tty or o_tty is their controlling
	 * tty.
	 */
	if (tty_closing || o_tty_closing) {
		struct task_struct *p;

		read_lock(&tasklist_lock);
		for_each_task(p) {
			if (p->tty == tty || (o_tty && p->tty == o_tty))
				p->tty = NULL;
		}
		read_unlock(&tasklist_lock);
	}

	/* check whether both sides are closing ... */
	if (!tty_closing || (o_tty && !o_tty_closing))
		return;
	
#ifdef TTY_DEBUG_HANGUP
	printk(KERN_DEBUG "freeing tty structure...");
#endif

	/*
	 * Shutdown the current line discipline, and reset it to N_TTY.
	 * N.B. why reset ldisc when we're releasing the memory??
	 */
	if (tty->ldisc.close)
		(tty->ldisc.close)(tty);
	tty->ldisc = ldiscs[N_TTY];
	tty->termios->c_line = N_TTY;
	if (o_tty) {
		if (o_tty->ldisc.close)
			(o_tty->ldisc.close)(o_tty);
		o_tty->ldisc = ldiscs[N_TTY];
	}
	
	/*
	 * Make sure that the tty's task queue isn't activated. 
	 */
	run_task_queue(&tq_timer);
	flush_scheduled_tasks();

	/* 
	 * The release_mem function takes care of the details of clearing
	 * the slots and preserving the termios structure.
	 */
	release_mem(tty, idx);
}

/*
 * tty_open and tty_release keep up the tty count that contains the
 * number of opens done on a tty. We cannot use the inode-count, as
 * different inodes might point to the same tty.
 *
 * Open-counting is needed for pty masters, as well as for keeping
 * track of serial lines: DTR is dropped when the last close happens.
 * (This is not done solely through tty->count, now.  - Ted 1/27/92)
 *
 * The termios state of a pty is reset on first open so that
 * settings don't persist across reuse.
 */
static int tty_open(struct inode * inode, struct file * filp)
{
	struct tty_struct *tty;
	int noctty, retval;
	kdev_t device;
	unsigned short saved_flags;
	char	buf[64];

	saved_flags = filp->f_flags;
retry_open:
	noctty = filp->f_flags & O_NOCTTY;
	device = inode->i_rdev;
	if (device == TTY_DEV) {
		if (!current->tty)
			return -ENXIO;
		device = current->tty->device;
		filp->f_flags |= O_NONBLOCK; /* Don't let /dev/tty block */
		/* noctty = 1; */
	}
#ifdef CONFIG_VT
	if (device == CONSOLE_DEV) {
		extern int fg_console;
		device = MKDEV(TTY_MAJOR, fg_console + 1);
		noctty = 1;
	}
#endif
	if (device == SYSCONS_DEV) {
		struct console *c = console_drivers;
		while(c && !c->device)
			c = c->next;
		if (!c)
                        return -ENODEV;
                device = c->device(c);
		filp->f_flags |= O_NONBLOCK; /* Don't let /dev/console block */
		noctty = 1;
	}

	if (device == PTMX_DEV) {
#ifdef CONFIG_UNIX98_PTYS

		/* find a free pty. */
		int major, minor;
		struct tty_driver *driver;

		/* find a device that is not in use. */
		retval = -1;
		for ( major = 0 ; major < UNIX98_NR_MAJORS ; major++ ) {
			driver = &ptm_driver[major];
			for (minor = driver->minor_start ;
			     minor < driver->minor_start + driver->num ;
			     minor++) {
				device = MKDEV(driver->major, minor);
				if (!init_dev(device, &tty)) goto ptmx_found; /* ok! */
			}
		}
		return -EIO; /* no free ptys */
	ptmx_found:
		set_bit(TTY_PTY_LOCK, &tty->flags); /* LOCK THE SLAVE */
		minor -= driver->minor_start;
		devpts_pty_new(driver->other->name_base + minor, MKDEV(driver->other->major, minor + driver->other->minor_start));
		tty_register_devfs(&pts_driver[major], DEVFS_FL_DEFAULT,
				   pts_driver[major].minor_start + minor);
		noctty = 1;
		goto init_dev_done;

#else   /* CONFIG_UNIX_98_PTYS */

		return -ENODEV;

#endif  /* CONFIG_UNIX_98_PTYS */
	}

	retval = init_dev(device, &tty);
	if (retval)
		return retval;

#ifdef CONFIG_UNIX98_PTYS
init_dev_done:
#endif
	filp->private_data = tty;
	file_move(filp, &tty->tty_files);
	check_tty_count(tty, "tty_open");
	if (tty->driver.type == TTY_DRIVER_TYPE_PTY &&
	    tty->driver.subtype == PTY_TYPE_MASTER)
		noctty = 1;
#ifdef TTY_DEBUG_HANGUP
	printk(KERN_DEBUG "opening %s...", tty_name(tty, buf));
#endif
	if (tty->driver.open)
		retval = tty->driver.open(tty, filp);
	else
		retval = -ENODEV;
	filp->f_flags = saved_flags;

	if (!retval && test_bit(TTY_EXCLUSIVE, &tty->flags) && !suser())
		retval = -EBUSY;

	if (retval) {
#ifdef TTY_DEBUG_HANGUP
		printk(KERN_DEBUG "error %d in opening %s...", retval,
		       tty_name(tty, buf));
#endif

		release_dev(filp);
		if (retval != -ERESTARTSYS)
			return retval;
		if (signal_pending(current))
			return retval;
		schedule();
		/*
		 * Need to reset f_op in case a hangup happened.
		 */
		filp->f_op = &tty_fops;
		goto retry_open;
	}
	if (!noctty &&
	    current->leader &&
	    !current->tty &&
	    tty->session == 0) {
	    	task_lock(current);
		current->tty = tty;
		task_unlock(current);
		current->tty_old_pgrp = 0;
		tty->session = current->session;
		tty->pgrp = current->pgrp;
	}
	if ((tty->driver.type == TTY_DRIVER_TYPE_SERIAL) &&
	    (tty->driver.subtype == SERIAL_TYPE_CALLOUT) &&
	    (tty->count == 1)) {
		static int nr_warns;
		if (nr_warns < 5) {
			printk(KERN_WARNING "tty_io.c: "
				"process %d (%s) used obsolete /dev/%s - "
				"update software to use /dev/ttyS%d\n",
				current->pid, current->comm,
				tty_name(tty, buf), TTY_NUMBER(tty));
			nr_warns++;
		}
	}
	return 0;
}

static int tty_release(struct inode * inode, struct file * filp)
{
	lock_kernel();
	release_dev(filp);
	unlock_kernel();
	return 0;
}

/* No kernel lock held - fine */
static unsigned int tty_poll(struct file * filp, poll_table * wait)
{
	struct tty_struct * tty;

	tty = (struct tty_struct *)filp->private_data;
	if (tty_paranoia_check(tty, filp->f_dentry->d_inode->i_rdev, "tty_poll"))
		return 0;

	if (tty->ldisc.poll)
		return (tty->ldisc.poll)(tty, filp, wait);
	return 0;
}

static int tty_fasync(int fd, struct file * filp, int on)
{
	struct tty_struct * tty;
	int retval;

	tty = (struct tty_struct *)filp->private_data;
	if (tty_paranoia_check(tty, filp->f_dentry->d_inode->i_rdev, "tty_fasync"))
		return 0;
	
	retval = fasync_helper(fd, filp, on, &tty->fasync);
	if (retval <= 0)
		return retval;

	if (on) {
		if (!waitqueue_active(&tty->read_wait))
			tty->minimum_to_wake = 1;
		if (filp->f_owner.pid == 0) {
			filp->f_owner.pid = (-tty->pgrp) ? : current->pid;
			filp->f_owner.uid = current->uid;
			filp->f_owner.euid = current->euid;
		}
	} else {
		if (!tty->fasync && !waitqueue_active(&tty->read_wait))
			tty->minimum_to_wake = N_TTY_BUF_SIZE;
	}
	return 0;
}

static int tiocsti(struct tty_struct *tty, char * arg)
{
	char ch, mbz = 0;

	if ((current->tty != tty) && !suser())
		return -EPERM;
	if (get_user(ch, arg))
		return -EFAULT;
	tty->ldisc.receive_buf(tty, &ch, &mbz, 1);
	return 0;
}

static int tiocgwinsz(struct tty_struct *tty, struct winsize * arg)
{
	if (copy_to_user(arg, &tty->winsize, sizeof(*arg)))
		return -EFAULT;
	return 0;
}

static int tiocswinsz(struct tty_struct *tty, struct tty_struct *real_tty,
	struct winsize * arg)
{
	struct winsize tmp_ws;

	if (copy_from_user(&tmp_ws, arg, sizeof(*arg)))
		return -EFAULT;
	if (!memcmp(&tmp_ws, &tty->winsize, sizeof(*arg)))
		return 0;
	if (tty->pgrp > 0)
		kill_pg(tty->pgrp, SIGWINCH, 1);
	if ((real_tty->pgrp != tty->pgrp) && (real_tty->pgrp > 0))
		kill_pg(real_tty->pgrp, SIGWINCH, 1);
	tty->winsize = tmp_ws;
	real_tty->winsize = tmp_ws;
	return 0;
}

static int tioccons(struct inode *inode, struct file *file)
{
	if (inode->i_rdev == SYSCONS_DEV ||
	    inode->i_rdev == CONSOLE_DEV) {
		struct file *f;
		if (!suser())
			return -EPERM;
		spin_lock(&redirect_lock);
		f = redirect;
		redirect = NULL;
		spin_unlock(&redirect_lock);
		if (f)
			fput(f);
		return 0;
	}
	spin_lock(&redirect_lock);
	if (redirect) {
		spin_unlock(&redirect_lock);
		return -EBUSY;
	}
	get_file(file);
	redirect = file;
	spin_unlock(&redirect_lock);
	return 0;
}


static int fionbio(struct file *file, int *arg)
{
	int nonblock;

	if (get_user(nonblock, arg))
		return -EFAULT;

	if (nonblock)
		file->f_flags |= O_NONBLOCK;
	else
		file->f_flags &= ~O_NONBLOCK;
	return 0;
}

static int tiocsctty(struct tty_struct *tty, int arg)
{
	if (current->leader &&
	    (current->session == tty->session))
		return 0;
	/*
	 * The process must be a session leader and
	 * not have a controlling tty already.
	 */
	if (!current->leader || current->tty)
		return -EPERM;
	if (tty->session > 0) {
		/*
		 * This tty is already the controlling
		 * tty for another session group!
		 */
		if ((arg == 1) && suser()) {
			/*
			 * Steal it away
			 */
			struct task_struct *p;

			read_lock(&tasklist_lock);
			for_each_task(p)
				if (p->tty == tty)
					p->tty = NULL;
			read_unlock(&tasklist_lock);
		} else
			return -EPERM;
	}
	task_lock(current);
	current->tty = tty;
	task_unlock(current);
	current->tty_old_pgrp = 0;
	tty->session = current->session;
	tty->pgrp = current->pgrp;
	return 0;
}

static int tiocgpgrp(struct tty_struct *tty, struct tty_struct *real_tty, pid_t *arg)
{
	/*
	 * (tty == real_tty) is a cheap way of
	 * testing if the tty is NOT a master pty.
	 */
	if (tty == real_tty && current->tty != real_tty)
		return -ENOTTY;
	return put_user(real_tty->pgrp, arg);
}

static int tiocspgrp(struct tty_struct *tty, struct tty_struct *real_tty, pid_t *arg)
{
	pid_t pgrp;
	int retval = tty_check_change(real_tty);

	if (retval == -EIO)
		return -ENOTTY;
	if (retval)
		return retval;
	if (!current->tty ||
	    (current->tty != real_tty) ||
	    (real_tty->session != current->session))
		return -ENOTTY;
	if (get_user(pgrp, (pid_t *) arg))
		return -EFAULT;
	if (pgrp < 0)
		return -EINVAL;
	if (session_of_pgrp(pgrp) != current->session)
		return -EPERM;
	real_tty->pgrp = pgrp;
	return 0;
}

static int tiocgsid(struct tty_struct *tty, struct tty_struct *real_tty, pid_t *arg)
{
	/*
	 * (tty == real_tty) is a cheap way of
	 * testing if the tty is NOT a master pty.
	*/
	if (tty == real_tty && current->tty != real_tty)
		return -ENOTTY;
	if (real_tty->session <= 0)
		return -ENOTTY;
	return put_user(real_tty->session, arg);
}

static int tiocttygstruct(struct tty_struct *tty, struct tty_struct *arg)
{
	if (copy_to_user(arg, tty, sizeof(*arg)))
		return -EFAULT;
	return 0;
}

static int tiocsetd(struct tty_struct *tty, int *arg)
{
	int ldisc;

	if (get_user(ldisc, arg))
		return -EFAULT;
	return tty_set_ldisc(tty, ldisc);
}

static int send_break(struct tty_struct *tty, int duration)
{
	set_current_state(TASK_INTERRUPTIBLE);

	tty->driver.break_ctl(tty, -1);
	if (!signal_pending(current))
		schedule_timeout(duration);
	tty->driver.break_ctl(tty, 0);
	if (signal_pending(current))
		return -EINTR;
	return 0;
}

static int tty_generic_brk(struct tty_struct *tty, struct file *file, unsigned int cmd, unsigned long arg)
{
	if (cmd == TCSBRK && arg) 
	{
		/* tcdrain case */
		int retval = tty_check_change(tty);
		if (retval)
			return retval;
		tty_wait_until_sent(tty, 0);
		if (signal_pending(current))
			return -EINTR;
	}
	return 0;
}

/*
 * Split this up, as gcc can choke on it otherwise..
 */
int tty_ioctl(struct inode * inode, struct file * file,
	      unsigned int cmd, unsigned long arg)
{
	struct tty_struct *tty, *real_tty;
	int retval;
	
	tty = (struct tty_struct *)file->private_data;
	if (tty_paranoia_check(tty, inode->i_rdev, "tty_ioctl"))
		return -EINVAL;

	real_tty = tty;
	if (tty->driver.type == TTY_DRIVER_TYPE_PTY &&
	    tty->driver.subtype == PTY_TYPE_MASTER)
		real_tty = tty->link;

	/*
	 * Break handling by driver
	 */
	if (!tty->driver.break_ctl) {
		switch(cmd) {
		case TIOCSBRK:
		case TIOCCBRK:
			if (tty->driver.ioctl)
				return tty->driver.ioctl(tty, file, cmd, arg);
			return -EINVAL;
			
		/* These two ioctl's always return success; even if */
		/* the driver doesn't support them. */
		case TCSBRK:
		case TCSBRKP:
			retval = -ENOIOCTLCMD;
			if (tty->driver.ioctl)
				retval = tty->driver.ioctl(tty, file, cmd, arg);
			/* Not driver handled */
			if (retval == -ENOIOCTLCMD)
				retval = tty_generic_brk(tty, file, cmd, arg);
			return retval;
		}
	}

	/*
	 * Factor out some common prep work
	 */
	switch (cmd) {
	case TIOCSETD:
	case TIOCSBRK:
	case TIOCCBRK:
	case TCSBRK:
	case TCSBRKP:			
		retval = tty_check_change(tty);
		if (retval)
			return retval;
		if (cmd != TIOCCBRK) {
			tty_wait_until_sent(tty, 0);
			if (signal_pending(current))
				return -EINTR;
		}
		break;
	}

	switch (cmd) {
		case TIOCSTI:
			return tiocsti(tty, (char *)arg);
		case TIOCGWINSZ:
			return tiocgwinsz(tty, (struct winsize *) arg);
		case TIOCSWINSZ:
			return tiocswinsz(tty, real_tty, (struct winsize *) arg);
		case TIOCCONS:
			return real_tty!=tty ? -EINVAL : tioccons(inode, file);
		case FIONBIO:
			return fionbio(file, (int *) arg);
		case TIOCEXCL:
			set_bit(TTY_EXCLUSIVE, &tty->flags);
			return 0;
		case TIOCNXCL:
			clear_bit(TTY_EXCLUSIVE, &tty->flags);
			return 0;
		case TIOCNOTTY:
			if (current->tty != tty)
				return -ENOTTY;
			if (current->leader)
				disassociate_ctty(0);
			task_lock(current);
			current->tty = NULL;
			task_unlock(current);
			return 0;
		case TIOCSCTTY:
			return tiocsctty(tty, arg);
		case TIOCGPGRP:
			return tiocgpgrp(tty, real_tty, (pid_t *) arg);
		case TIOCSPGRP:
			return tiocspgrp(tty, real_tty, (pid_t *) arg);
		case TIOCGSID:
			return tiocgsid(tty, real_tty, (pid_t *) arg);
		case TIOCGETD:
			return put_user(tty->ldisc.num, (int *) arg);
		case TIOCSETD:
			return tiocsetd(tty, (int *) arg);
#ifdef CONFIG_VT
		case TIOCLINUX:
			return tioclinux(tty, arg);
#endif
		case TIOCTTYGSTRUCT:
			return tiocttygstruct(tty, (struct tty_struct *) arg);

		/*
		 * Break handling
		 */
		case TIOCSBRK:	/* Turn break on, unconditionally */
			tty->driver.break_ctl(tty, -1);
			return 0;
			
		case TIOCCBRK:	/* Turn break off, unconditionally */
			tty->driver.break_ctl(tty, 0);
			return 0;
		case TCSBRK:   /* SVID version: non-zero arg --> no break */
			/*
			 * XXX is the above comment correct, or the
			 * code below correct?  Is this ioctl used at
			 * all by anyone?
			 */
			if (!arg)
				return send_break(tty, HZ/4);
			return 0;
		case TCSBRKP:	/* support for POSIX tcsendbreak() */	
			return send_break(tty, arg ? arg*(HZ/10) : HZ/4);
	}
	if (tty->driver.ioctl) {
		int retval = (tty->driver.ioctl)(tty, file, cmd, arg);
		if (retval != -ENOIOCTLCMD)
			return retval;
	}
	if (tty->ldisc.ioctl) {
		int retval = (tty->ldisc.ioctl)(tty, file, cmd, arg);
		if (retval != -ENOIOCTLCMD)
			return retval;
	}
	return -EINVAL;
}


/*
 * This implements the "Secure Attention Key" ---  the idea is to
 * prevent trojan horses by killing all processes associated with this
 * tty when the user hits the "Secure Attention Key".  Required for
 * super-paranoid applications --- see the Orange Book for more details.
 * 
 * This code could be nicer; ideally it should send a HUP, wait a few
 * seconds, then send a INT, and then a KILL signal.  But you then
 * have to coordinate with the init process, since all processes associated
 * with the current tty must be dead before the new getty is allowed
 * to spawn.
 *
 * Now, if it would be correct ;-/ The current code has a nasty hole -
 * it doesn't catch files in flight. We may send the descriptor to ourselves
 * via AF_UNIX socket, close it and later fetch from socket. FIXME.
 *
 * Nasty bug: do_SAK is being called in interrupt context.  This can
 * deadlock.  We punt it up to process context.  AKPM - 16Mar2001
 */
static void __do_SAK(void *arg)
{
#ifdef TTY_SOFT_SAK
	tty_hangup(tty);
#else
	struct tty_struct *tty = arg;
	struct task_struct *p;
	int session;
	int		i;
	struct file	*filp;
	
	if (!tty)
		return;
	session  = tty->session;
	if (tty->ldisc.flush_buffer)
		tty->ldisc.flush_buffer(tty);
	if (tty->driver.flush_buffer)
		tty->driver.flush_buffer(tty);
	read_lock(&tasklist_lock);
	for_each_task(p) {
		if ((p->tty == tty) ||
		    ((session > 0) && (p->session == session))) {
			send_sig(SIGKILL, p, 1);
			continue;
		}
		task_lock(p);
		if (p->files) {
			read_lock(&p->files->file_lock);
			for (i=0; i < p->files->max_fds; i++) {
				filp = fcheck_files(p->files, i);
				if (filp && (filp->f_op == &tty_fops) &&
				    (filp->private_data == tty)) {
					send_sig(SIGKILL, p, 1);
					break;
				}
			}
			read_unlock(&p->files->file_lock);
		}
		task_unlock(p);
	}
	read_unlock(&tasklist_lock);
#endif
}

/*
 * The tq handling here is a little racy - tty->SAK_tq may already be queued.
 * But there's no mechanism to fix that without futzing with tqueue_lock.
 * Fortunately we don't need to worry, because if ->SAK_tq is already queued,
 * the values which we write to it will be identical to the values which it
 * already has. --akpm
 */
void do_SAK(struct tty_struct *tty)
{
	if (!tty)
		return;
	PREPARE_TQUEUE(&tty->SAK_tq, __do_SAK, tty);
	schedule_task(&tty->SAK_tq);
}

/*
 * This routine is called out of the software interrupt to flush data
 * from the flip buffer to the line discipline.
 */
static void flush_to_ldisc(void *private_)
{
	struct tty_struct *tty = (struct tty_struct *) private_;
	unsigned char	*cp;
	char		*fp;
	int		count;
	unsigned long flags;

	if (test_bit(TTY_DONT_FLIP, &tty->flags)) {
		queue_task(&tty->flip.tqueue, &tq_timer);
		return;
	}
	if (tty->flip.buf_num) {
		cp = tty->flip.char_buf + TTY_FLIPBUF_SIZE;
		fp = tty->flip.flag_buf + TTY_FLIPBUF_SIZE;
		tty->flip.buf_num = 0;

		save_flags(flags); cli();
		tty->flip.char_buf_ptr = tty->flip.char_buf;
		tty->flip.flag_buf_ptr = tty->flip.flag_buf;
	} else {
		cp = tty->flip.char_buf;
		fp = tty->flip.flag_buf;
		tty->flip.buf_num = 1;

		save_flags(flags); cli();
		tty->flip.char_buf_ptr = tty->flip.char_buf + TTY_FLIPBUF_SIZE;
		tty->flip.flag_buf_ptr = tty->flip.flag_buf + TTY_FLIPBUF_SIZE;
	}
	count = tty->flip.count;
	tty->flip.count = 0;
	restore_flags(flags);
	
	tty->ldisc.receive_buf(tty, cp, fp, count);
}

/*
 * Routine which returns the baud rate of the tty
 *
 * Note that the baud_table needs to be kept in sync with the
 * include/asm/termbits.h file.
 */
static int baud_table[] = {
	0, 50, 75, 110, 134, 150, 200, 300, 600, 1200, 1800, 2400, 4800,
	9600, 19200, 38400, 57600, 115200, 230400, 460800,
#ifdef __sparc__
	76800, 153600, 307200, 614400, 921600
#else
	500000, 576000, 921600, 1000000, 1152000, 1500000, 2000000,
	2500000, 3000000, 3500000, 4000000
#endif
};

static int n_baud_table = sizeof(baud_table)/sizeof(int);

int tty_get_baud_rate(struct tty_struct *tty)
{
	unsigned int cflag, i;

	cflag = tty->termios->c_cflag;

	i = cflag & CBAUD;
	if (i & CBAUDEX) {
		i &= ~CBAUDEX;
		if (i < 1 || i+15 >= n_baud_table) 
			tty->termios->c_cflag &= ~CBAUDEX;
		else
			i += 15;
	}
	if (i==15 && tty->alt_speed) {
		if (!tty->warned) {
			printk(KERN_WARNING "Use of setserial/setrocket to "
					    "set SPD_* flags is deprecated\n");
			tty->warned = 1;
		}
		return(tty->alt_speed);
	}
	
	return baud_table[i];
}

void tty_flip_buffer_push(struct tty_struct *tty)
{
	if (tty->low_latency)
		flush_to_ldisc((void *) tty);
	else
		queue_task(&tty->flip.tqueue, &tq_timer);
}

/*
 * This subroutine initializes a tty structure.
 */
static void initialize_tty_struct(struct tty_struct *tty)
{
	memset(tty, 0, sizeof(struct tty_struct));
	tty->magic = TTY_MAGIC;
	tty->ldisc = ldiscs[N_TTY];
	tty->pgrp = -1;
	tty->flip.char_buf_ptr = tty->flip.char_buf;
	tty->flip.flag_buf_ptr = tty->flip.flag_buf;
	tty->flip.tqueue.routine = flush_to_ldisc;
	tty->flip.tqueue.data = tty;
	init_MUTEX(&tty->flip.pty_sem);
	init_waitqueue_head(&tty->write_wait);
	init_waitqueue_head(&tty->read_wait);
	tty->tq_hangup.routine = do_tty_hangup;
	tty->tq_hangup.data = tty;
	sema_init(&tty->atomic_read, 1);
	sema_init(&tty->atomic_write, 1);
	spin_lock_init(&tty->read_lock);
	INIT_LIST_HEAD(&tty->tty_files);
	INIT_TQUEUE(&tty->SAK_tq, 0, 0);
}

/*
 * The default put_char routine if the driver did not define one.
 */
void tty_default_put_char(struct tty_struct *tty, unsigned char ch)
{
	tty->driver.write(tty, 0, &ch, 1);
}

/*
 * Register a tty device described by <driver>, with minor number <minor>.
 */
void tty_register_devfs (struct tty_driver *driver, unsigned int flags, unsigned minor)
{
#ifdef CONFIG_DEVFS_FS
	umode_t mode = S_IFCHR | S_IRUSR | S_IWUSR;
	kdev_t device = MKDEV (driver->major, minor);
	int idx = minor - driver->minor_start;
	char buf[32];

	switch (device) {
		case TTY_DEV:
		case PTMX_DEV:
			mode |= S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
			break;
		default:
			if (driver->major == PTY_MASTER_MAJOR)
				mode |= S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
			break;
	}
	if ( (minor <  driver->minor_start) || 
	     (minor >= driver->minor_start + driver->num) ) {
		printk(KERN_ERR "Attempt to register invalid minor number "
		       "with devfs (%d:%d).\n", (int)driver->major,(int)minor);
		return;
	}
#  ifdef CONFIG_UNIX98_PTYS
	if ( (driver->major >= UNIX98_PTY_SLAVE_MAJOR) &&
	     (driver->major < UNIX98_PTY_SLAVE_MAJOR + UNIX98_NR_MAJORS) )
		flags |= DEVFS_FL_CURRENT_OWNER;
#  endif
	sprintf(buf, driver->name, idx + driver->name_base);
	devfs_register (NULL, buf, flags | DEVFS_FL_DEFAULT,
			driver->major, minor, mode, &tty_fops, NULL);
#endif /* CONFIG_DEVFS_FS */
}

void tty_unregister_devfs (struct tty_driver *driver, unsigned minor)
{
#ifdef CONFIG_DEVFS_FS
	void * handle;
	int idx = minor - driver->minor_start;
	char buf[32];

	sprintf(buf, driver->name, idx + driver->name_base);
	handle = devfs_find_handle (NULL, buf, driver->major, minor,
				    DEVFS_SPECIAL_CHR, 0);
	devfs_unregister (handle);
#endif /* CONFIG_DEVFS_FS */
}

EXPORT_SYMBOL(tty_register_devfs);
EXPORT_SYMBOL(tty_unregister_devfs);

/*
 * Called by a tty driver to register itself.
 */
int tty_register_driver(struct tty_driver *driver)
{
	int error;
        int i;

	if (driver->flags & TTY_DRIVER_INSTALLED)
		return 0;

	error = devfs_register_chrdev(driver->major, driver->name, &tty_fops);
	if (error < 0)
		return error;
	else if(driver->major == 0)
		driver->major = error;

	if (!driver->put_char)
		driver->put_char = tty_default_put_char;
	
	driver->prev = 0;
	driver->next = tty_drivers;
	if (tty_drivers) tty_drivers->prev = driver;
	tty_drivers = driver;
	
	if ( !(driver->flags & TTY_DRIVER_NO_DEVFS) ) {
		for(i = 0; i < driver->num; i++)
		    tty_register_devfs(driver, 0, driver->minor_start + i);
	}
	proc_tty_register_driver(driver);
	return error;
}

/*
 * Called by a tty driver to unregister itself.
 */
int tty_unregister_driver(struct tty_driver *driver)
{
	int	retval;
	struct tty_driver *p;
	int	i, found = 0;
	struct termios *tp;
	const char *othername = NULL;
	
	if (*driver->refcount)
		return -EBUSY;

	for (p = tty_drivers; p; p = p->next) {
		if (p == driver)
			found++;
		else if (p->major == driver->major)
			othername = p->name;
	}
	
	if (!found)
		return -ENOENT;

	if (othername == NULL) {
		retval = devfs_unregister_chrdev(driver->major, driver->name);
		if (retval)
			return retval;
	} else
		devfs_register_chrdev(driver->major, othername, &tty_fops);

	if (driver->prev)
		driver->prev->next = driver->next;
	else
		tty_drivers = driver->next;
	
	if (driver->next)
		driver->next->prev = driver->prev;

	/*
	 * Free the termios and termios_locked structures because
	 * we don't want to get memory leaks when modular tty
	 * drivers are removed from the kernel.
	 */
	for (i = 0; i < driver->num; i++) {
		tp = driver->termios[i];
		if (tp) {
			driver->termios[i] = NULL;
			kfree(tp);
		}
		tp = driver->termios_locked[i];
		if (tp) {
			driver->termios_locked[i] = NULL;
			kfree(tp);
		}
		tty_unregister_devfs(driver, driver->minor_start + i);
	}
	proc_tty_unregister_driver(driver);
	return 0;
}


/*
 * Initialize the console device. This is called *early*, so
 * we can't necessarily depend on lots of kernel help here.
 * Just do some early initializations, and do the complex setup
 * later.
 */
void __init console_init(void)
{
	/* Setup the default TTY line discipline. */
	memset(ldiscs, 0, sizeof(ldiscs));
	(void) tty_register_ldisc(N_TTY, &tty_ldisc_N_TTY);

	/*
	 * Set up the standard termios.  Individual tty drivers may 
	 * deviate from this; this is used as a template.
	 */
	memset(&tty_std_termios, 0, sizeof(struct termios));
	memcpy(tty_std_termios.c_cc, INIT_C_CC, NCCS);
	tty_std_termios.c_iflag = ICRNL | IXON;
	tty_std_termios.c_oflag = OPOST | ONLCR;
	tty_std_termios.c_cflag = B38400 | CS8 | CREAD | HUPCL;
	tty_std_termios.c_lflag = ISIG | ICANON | ECHO | ECHOE | ECHOK |
		ECHOCTL | ECHOKE | IEXTEN;

	/*
	 * set up the console device so that later boot sequences can 
	 * inform about problems etc..
	 */
#ifdef CONFIG_EARLY_PRINTK
	disable_early_printk(); 
#endif
#ifdef CONFIG_HVC_CONSOLE
	hvc_console_init();
#endif
#ifdef CONFIG_VT
	con_init();
#endif
#ifdef CONFIG_AU1X00_SERIAL_CONSOLE
	au1x00_serial_console_init();
#endif
#ifdef CONFIG_SERIAL_CONSOLE
#if (defined(CONFIG_8xx) || defined(CONFIG_CPM2))
	console_8xx_init();
#elif defined(CONFIG_MAC_SERIAL) && defined(CONFIG_SERIAL)
	if (_machine == _MACH_Pmac)
 		mac_scc_console_init();
	else
		serial_console_init();
#elif defined(CONFIG_MAC_SERIAL)
 	mac_scc_console_init();
#elif defined(CONFIG_PARISC)
	pdc_console_init();
#elif defined(CONFIG_SERIAL)
	serial_console_init();
#endif /* CONFIG_8xx */
#if defined(CONFIG_MVME162_SCC) || defined(CONFIG_BVME6000_SCC) || defined(CONFIG_MVME147_SCC)
	vme_scc_console_init();
#endif
#if defined(CONFIG_SERIAL167)
	serial167_console_init();
#endif
#if defined(CONFIG_SH_SCI)
	sci_console_init();
#endif
#endif
#ifdef CONFIG_SERIAL_DEC_CONSOLE
	dec_serial_console_init();
#endif
#ifdef CONFIG_TN3270_CONSOLE
	tub3270_con_init();
#endif
#ifdef CONFIG_TN3215
	con3215_init();
#endif
#ifdef CONFIG_HWC
        hwc_console_init();
#endif
#ifdef CONFIG_STDIO_CONSOLE
	stdio_console_init();
#endif
#ifdef CONFIG_SERIAL_21285_CONSOLE
	rs285_console_init();
#endif
#ifdef CONFIG_SERIAL_SA1100_CONSOLE
	sa1100_rs_console_init();
#endif
#ifdef CONFIG_ARC_CONSOLE
	arc_console_init();
#endif
#ifdef CONFIG_SERIAL_AMBA_CONSOLE
	ambauart_console_init();
#endif
#ifdef CONFIG_SERIAL_TX3912_CONSOLE
	tx3912_console_init();
#endif
#ifdef CONFIG_TXX927_SERIAL_CONSOLE
	txx927_console_init();
#endif
#ifdef CONFIG_SERIAL_TXX9_CONSOLE
	txx9_serial_console_init();
#endif
#ifdef CONFIG_SIBYTE_SB1250_DUART_CONSOLE
	sb1250_serial_console_init();
#endif
#ifdef CONFIG_IP22_SERIAL
	sgi_serial_console_init();
#endif
}

static struct tty_driver dev_tty_driver, dev_syscons_driver;
#ifdef CONFIG_UNIX98_PTYS
static struct tty_driver dev_ptmx_driver;
#endif
#ifdef CONFIG_VT
static struct tty_driver dev_console_driver;
#endif

/*
 * Ok, now we can initialize the rest of the tty devices and can count
 * on memory allocations, interrupts etc..
 */
void __init tty_init(void)
{
	/*
	 * dev_tty_driver and dev_console_driver are actually magic
	 * devices which get redirected at open time.  Nevertheless,
	 * we register them so that register_chrdev is called
	 * appropriately.
	 */
	memset(&dev_tty_driver, 0, sizeof(struct tty_driver));
	dev_tty_driver.magic = TTY_DRIVER_MAGIC;
	dev_tty_driver.driver_name = "/dev/tty";
	dev_tty_driver.name = dev_tty_driver.driver_name + 5;
	dev_tty_driver.name_base = 0;
	dev_tty_driver.major = TTYAUX_MAJOR;
	dev_tty_driver.minor_start = 0;
	dev_tty_driver.num = 1;
	dev_tty_driver.type = TTY_DRIVER_TYPE_SYSTEM;
	dev_tty_driver.subtype = SYSTEM_TYPE_TTY;
	
	if (tty_register_driver(&dev_tty_driver))
		panic("Couldn't register /dev/tty driver\n");

	dev_syscons_driver = dev_tty_driver;
	dev_syscons_driver.driver_name = "/dev/console";
	dev_syscons_driver.name = dev_syscons_driver.driver_name + 5;
	dev_syscons_driver.major = TTYAUX_MAJOR;
	dev_syscons_driver.minor_start = 1;
	dev_syscons_driver.type = TTY_DRIVER_TYPE_SYSTEM;
	dev_syscons_driver.subtype = SYSTEM_TYPE_SYSCONS;

	if (tty_register_driver(&dev_syscons_driver))
		panic("Couldn't register /dev/console driver\n");

	/* console calls tty_register_driver() before kmalloc() works.
	 * Thus, we can't devfs_register() then.  Do so now, instead. 
	 */
#ifdef CONFIG_VT
	con_init_devfs();
#endif

#ifdef CONFIG_UNIX98_PTYS
	dev_ptmx_driver = dev_tty_driver;
	dev_ptmx_driver.driver_name = "/dev/ptmx";
	dev_ptmx_driver.name = dev_ptmx_driver.driver_name + 5;
	dev_ptmx_driver.major= MAJOR(PTMX_DEV);
	dev_ptmx_driver.minor_start = MINOR(PTMX_DEV);
	dev_ptmx_driver.type = TTY_DRIVER_TYPE_SYSTEM;
	dev_ptmx_driver.subtype = SYSTEM_TYPE_SYSPTMX;

	if (tty_register_driver(&dev_ptmx_driver))
		panic("Couldn't register /dev/ptmx driver\n");
#endif
	
#ifdef CONFIG_VT
	dev_console_driver = dev_tty_driver;
	dev_console_driver.driver_name = "/dev/vc/0";
	dev_console_driver.name = dev_console_driver.driver_name + 5;
	dev_console_driver.major = TTY_MAJOR;
	dev_console_driver.type = TTY_DRIVER_TYPE_SYSTEM;
	dev_console_driver.subtype = SYSTEM_TYPE_CONSOLE;

	if (tty_register_driver(&dev_console_driver))
		panic("Couldn't register /dev/tty0 driver\n");

	kbd_init();
#endif

#ifdef CONFIG_SGI_L1_SERIAL_CONSOLE
	if (ia64_platform_is("sn2")) {
		sn_sal_serial_console_init();
		return; /* only one console right now for SN2 */
	}
#endif
#ifdef CONFIG_ESPSERIAL  /* init ESP before rs, so rs doesn't see the port */
	espserial_init();
#endif
#if defined(CONFIG_MVME162_SCC) || defined(CONFIG_BVME6000_SCC) || defined(CONFIG_MVME147_SCC)
	vme_scc_init();
#endif
#ifdef CONFIG_SERIAL_TX3912
	tx3912_rs_init();
#endif
#ifdef CONFIG_ROCKETPORT
	rp_init();
#endif
#ifdef CONFIG_SERIAL167
	serial167_init();
#endif
#ifdef CONFIG_CYCLADES
	cy_init();
#endif
#ifdef CONFIG_STALLION
	stl_init();
#endif
#ifdef CONFIG_ISTALLION
	stli_init();
#endif
#ifdef CONFIG_DIGI
	pcxe_init();
#endif
#ifdef CONFIG_DIGIEPCA
	pc_init();
#endif
#ifdef CONFIG_SPECIALIX
	specialix_init();
#endif
#if (defined(CONFIG_8xx) || defined(CONFIG_CPM2))
	rs_8xx_init();
#endif /* CONFIG_8xx */
	pty_init();
#ifdef CONFIG_MOXA_SMARTIO
	mxser_init();
#endif	
#ifdef CONFIG_MOXA_INTELLIO
	moxa_init();
#endif	
#ifdef CONFIG_VT
	vcs_init();
#endif
#ifdef CONFIG_TN3270
	tub3270_init();
#endif
#ifdef CONFIG_TN3215
	tty3215_init();
#endif
#ifdef CONFIG_HWC
	hwc_tty_init();
#endif
#ifdef CONFIG_A2232
	a2232board_init();
#endif
}
