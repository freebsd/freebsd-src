/*
 *  linux/drivers/char/pdc_console.c
 *
 *  2001, Christoph Plattner
 * 
 *  Driver template was linux's serial.c
 *
 */

static char *pdc_drv_version = "0.3";
static char *pdc_drv_revdate = "2001-11-17";
#define AUTHOR "christoph.plattner@gmx.at"
#include <linux/config.h>
#include <linux/version.h>

#undef PDC_DRV_DEBUG

#undef SERIAL_PARANOIA_CHECK
#define CONFIG_SERIAL_NOPAUSE_IO
#define SERIAL_DO_RESTART

#define PDC_POLL_DELAY (30 * HZ / 1000)

/*
 * End of serial driver configuration section.
 */

#include <linux/module.h>

#include <linux/types.h>
#include <linux/serial.h>
#include <linux/serialP.h>
#include <linux/serial_reg.h>
#include <asm/serial.h>
#define LOCAL_VERSTRING ""

#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/major.h>
#include <linux/string.h>
#include <linux/fcntl.h>
#include <linux/ptrace.h>
#include <linux/ioport.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#include <linux/delay.h>

#include <asm/system.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/bitops.h>

#ifdef CONFIG_GSC
#include <asm/gsc.h>
#endif

extern int pdc_console_poll_key(void *);
extern void pdc_outc(unsigned char);

static char *pdc_drv_name = "PDC Software Console";

static struct tty_driver pdc_drv_driver;
static int pdc_drv_refcount = 0;
static struct async_struct *pdc_drv_info;

static struct timer_list pdc_drv_timer;

/* serial subtype definitions */
#ifndef SERIAL_TYPE_NORMAL
#define SERIAL_TYPE_NORMAL	1
#define SERIAL_TYPE_CALLOUT	2
#endif

#define NR_PORTS 1
#define PDC_DUMMY_BUF 2048

static struct tty_struct *pdc_drv_table[NR_PORTS];
static struct termios *pdc_drv_termios[NR_PORTS];
static struct termios *pdc_drv_termios_locked[NR_PORTS];

/*
 * tmp_buf is used as a temporary buffer by serial_write.  We need to
 * lock it in case the copy_from_user blocks while swapping in a page,
 * and some other program tries to do a serial write at the same time.
 * Since the lock will only come under contention when the system is
 * swapping and available memory is low, it makes sense to share one
 * buffer across all the serial ports, since it significantly saves
 * memory if large numbers of serial ports are open.
 */
static unsigned char *tmp_buf;
#ifdef DECLARE_MUTEX
static DECLARE_MUTEX(tmp_buf_sem);
#else
static struct semaphore tmp_buf_sem = MUTEX;
#endif

/*
 * ------------------------------------------------------------
 * pdc_stop() and pdc_start()
 *
 * This routines are called before setting or resetting tty->stopped.
 * They enable or disable transmitter interrupts, as necessary.
 * ------------------------------------------------------------
 */
static void
pdc_stop(struct tty_struct *tty)
{
}

static void
pdc_start(struct tty_struct *tty)
{
}

/*
 * ----------------------------------------------------------------------
 *
 * Here starts the interrupt handling routines.  All of the following
 * subroutines are declared as inline and are folded into
 * rs_interrupt().  They were separated out for readability's sake.
 *
 * Note: rs_interrupt() is a "fast" interrupt, which means that it
 * runs with interrupts turned off.  People who may want to modify
 * rs_interrupt() should try to keep the interrupt handler as fast as
 * possible.  After you are done making modifications, it is not a bad
 * idea to do:
 * 
 * gcc -S -DKERNEL -Wall -Wstrict-prototypes -O6 -fomit-frame-pointer serial.c
 *
 * and look at the resulting assemble code in serial.s.
 *
 * 				- Ted Ts'o (tytso@mit.edu), 7-Mar-93
 * -----------------------------------------------------------------------
 */

static void
receive_chars(struct async_struct *info, int *status, struct pt_regs *regs)
{
	struct tty_struct *tty = info->tty;
	unsigned char ch;
	int __ch;

	while (1) {
		__ch = pdc_console_poll_key(NULL);

		if (__ch == -1)	/* no character available */
			break;

		ch = (unsigned char) ((unsigned) __ch & 0x000000ffu);

		if (tty->flip.count >= TTY_FLIPBUF_SIZE)
			continue;

		*tty->flip.char_buf_ptr = ch;
		*tty->flip.flag_buf_ptr = 0;

		tty->flip.flag_buf_ptr++;
		tty->flip.char_buf_ptr++;
		tty->flip.count++;
	}

	tty_flip_buffer_push(tty);
}

static void
pdc_drv_poll(unsigned long dummy)
{
	struct async_struct *info;
	int status = 0;

	info = pdc_drv_info;

	if (!info || !info->tty || (pdc_drv_refcount == 0)) {
		/* do nothing */
	} else {
		receive_chars(info, &status, NULL);
		info->last_active = jiffies;
	}

	mod_timer(&pdc_drv_timer, jiffies + PDC_POLL_DELAY);
}

static void
pdc_put_char(struct tty_struct *tty, unsigned char ch)
{
#ifdef PDC_DRV_DEBUG
	printk(KERN_NOTICE "[%s] %c return\n", __FUNCTION__, ch);
#endif
	pdc_outc(ch);
}

static void
pdc_flush_chars(struct tty_struct *tty)
{
	/* PCD console always flushed all characters */

#ifdef PDC_DRV_DEBUG
	printk(KERN_NOTICE "[%s] return\n", __FUNCTION__);
#endif

	/* nothing to do */
}

static int
pdc_write(struct tty_struct *tty, int from_user,
	  const unsigned char *buf, int count)
{
	char pdc_tmp_buf[PDC_DUMMY_BUF];
	char *pdc_tmp_buf_ptr;
	int len;
	int ret = 0;

#ifdef PDC_DRV_DEBUG
	printk(KERN_NOTICE "[%s] entry\n", __FUNCTION__);
#endif
	while (count) {
		if (count < PDC_DUMMY_BUF)
			len = count;
		else
			len = PDC_DUMMY_BUF;

		if (from_user) {
			copy_from_user(pdc_tmp_buf, buf, len);
			pdc_tmp_buf_ptr = pdc_tmp_buf;
		} else
			pdc_tmp_buf_ptr = (char *) buf;

		while (len) {
			pdc_outc(*pdc_tmp_buf_ptr);
			buf++;
			pdc_tmp_buf_ptr++;
			ret++;
			count--;
			len--;
		}
	}
#ifdef PDC_DRV_DEBUG
	printk(KERN_NOTICE "[%s] return\n", __FUNCTION__);
#endif
	return ret;
}

static int
pdc_write_room(struct tty_struct *tty)
{
#ifdef PDC_DRV_DEBUG
	printk(KERN_NOTICE "[%s] entry\n", __FUNCTION__);
#endif
	return PDC_DUMMY_BUF;
}

static int
pdc_chars_in_buffer(struct tty_struct *tty)
{
#ifdef PDC_DRV_DEBUG
	printk(KERN_NOTICE "[%s] entry\n", __FUNCTION__);
#endif
	return 0;		/* no characters in buffer, always flushed ! */
}

static void
pdc_flush_buffer(struct tty_struct *tty)
{
#ifdef PDC_DRV_DEBUG
	printk(KERN_NOTICE "[%s] return\n", __FUNCTION__);
#endif
}

/*
 * This function is used to send a high-priority XON/XOFF character to
 * the device
 */
static void
pdc_send_xchar(struct tty_struct *tty, char ch)
{
}

/*
 * ------------------------------------------------------------
 * pdc_throttle()
 * 
 * This routine is called by the upper-layer tty layer to signal that
 * incoming characters should be throttled.
 * ------------------------------------------------------------
 */
static void
pdc_throttle(struct tty_struct *tty)
{
}

static void
pdc_unthrottle(struct tty_struct *tty)
{
}

/*
 * ------------------------------------------------------------
 * pdc_ioctl() and friends
 * ------------------------------------------------------------
 */

static void
pdc_break(struct tty_struct *tty, int break_state)
{
}

static int
get_serial_info(struct async_struct * info,
                           struct serial_struct * retinfo)
{
	struct serial_struct tmp;

	if (!retinfo)
		return -EFAULT;
	memset(&tmp, 0, sizeof(tmp));
	tmp.line = info->line;
	tmp.port = info->line;
	tmp.flags = info->flags;
	tmp.close_delay = info->close_delay;
	return copy_to_user(retinfo,&tmp,sizeof(*retinfo)) ? -EFAULT : 0;
}

static int get_modem_info(struct async_struct * info, unsigned int *value)
{
	unsigned int result = TIOCM_DTR|TIOCM_CAR|TIOCM_CTS|TIOCM_RTS;

	return copy_to_user(value, &result, sizeof(int)) ? -EFAULT : 0;
}

static int get_lsr_info(struct async_struct * info, unsigned int *value)
{
	unsigned int result = TIOCSER_TEMT;

	return copy_to_user(value, &result, sizeof(int)) ? -EFAULT : 0;
}

static int
pdc_ioctl(struct tty_struct *tty, struct file *file,
	  unsigned int cmd, unsigned long arg)
{
	struct async_struct *info = (struct async_struct *) tty->driver_data;

	if ((cmd != TIOCGSERIAL) && (cmd != TIOCSSERIAL) &&
	    (cmd != TIOCSERCONFIG) && (cmd != TIOCSERGSTRUCT) &&
	    (cmd != TIOCMIWAIT) && (cmd != TIOCGICOUNT)) {
		if (tty->flags & (1 << TTY_IO_ERROR))
			return -EIO;
	}

	switch (cmd) {
	case TIOCMGET:
		return get_modem_info(info, (unsigned int *) arg);
	case TIOCMBIS:
	case TIOCMBIC:
	case TIOCMSET:
		return 0;
	case TIOCGSERIAL:
		return get_serial_info(info, (struct serial_struct *) arg);
	case TIOCSSERIAL:
		return 0;
	case TIOCSERCONFIG:
		return 0;

	case TIOCSERGETLSR:	/* Get line status register */
		return get_lsr_info(info, (unsigned int *) arg);

	case TIOCSERGSTRUCT:
		if (copy_to_user((struct async_struct *) arg,
				 info, sizeof (struct async_struct)))
			return -EFAULT;
		return 0;

	case TIOCMIWAIT:
		return 0;

	case TIOCGICOUNT:
		return 0;
	case TIOCSERGWILD:
	case TIOCSERSWILD:
		/* "setserial -W" is called in Debian boot */
		printk("TIOCSER?WILD ioctl obsolete, ignored.\n");
		return 0;

	default:
		return -ENOIOCTLCMD;
	}
	return 0;
}

static void
pdc_set_termios(struct tty_struct *tty, struct termios *old_termios)
{

#if 0				/* XXX CP, has to be checked, if there is stuff to do */
	struct async_struct *info = (struct async_struct *) tty->driver_data;
	unsigned long flags;
	unsigned int cflag = tty->termios->c_cflag;

	if ((cflag == old_termios->c_cflag)
	    && (RELEVANT_IFLAG(tty->termios->c_iflag)
		== RELEVANT_IFLAG(old_termios->c_iflag)))
		return;
#if 0
	change_speed(info, old_termios);
#endif
	/* Handle turning off CRTSCTS */
	if ((old_termios->c_cflag & CRTSCTS) &&
	    !(tty->termios->c_cflag & CRTSCTS)) {
		tty->hw_stopped = 0;
		pdc_start(tty);
	}
#endif
}

/*
 * ------------------------------------------------------------
 * pdc_close()
 * 
 * This routine is called when the serial port gets closed.  First, we
 * wait for the last remaining data to be sent.  Then, we unlink its
 * async structure from the interrupt chain if necessary, and we free
 * that IRQ if nothing is left in the chain.
 * ------------------------------------------------------------
 */
static void
pdc_close(struct tty_struct *tty, struct file *filp)
{
	struct async_struct *info = (struct async_struct *) tty->driver_data;

#ifdef PDC_DEBUG_OPEN
	printk("pdc_close ttyB%d, count = %d\n", info->line, state->count);
#endif
	pdc_drv_refcount--;
	if (pdc_drv_refcount > 0)
		return;

	info->flags |= ASYNC_CLOSING;

	/*
	 * Save the termios structure, since this port may have
	 * separate termios for callout and dialin.
	 */
	if (info->flags & ASYNC_NORMAL_ACTIVE)
		info->state->normal_termios = *tty->termios;
	if (info->flags & ASYNC_CALLOUT_ACTIVE)
		info->state->callout_termios = *tty->termios;

	/*
	 * At this point we stop accepting input.  To do this, we
	 * disable the receive line status interrupts, and tell the
	 * interrupt driver to stop checking the data ready bit in the
	 * line status register.
	 */

	/* XXX CP: make mask for receive !!! */

	if (tty->driver.flush_buffer)
		tty->driver.flush_buffer(tty);
	if (tty->ldisc.flush_buffer)
		tty->ldisc.flush_buffer(tty);
	tty->closing = 0;
	info->event = 0;
	info->tty = 0;
	pdc_drv_info = NULL;
	if (info->blocked_open) {
		if (info->close_delay) {
			set_current_state(TASK_INTERRUPTIBLE);
			schedule_timeout(info->close_delay);
		}
		wake_up_interruptible(&info->open_wait);
	}
	info->flags &= ~(ASYNC_NORMAL_ACTIVE | ASYNC_CALLOUT_ACTIVE |
			 ASYNC_CLOSING);
	wake_up_interruptible(&info->close_wait);
	MOD_DEC_USE_COUNT;
}

/*
 * pdc_wait_until_sent() --- wait until the transmitter is empty
 */
static void
pdc_wait_until_sent(struct tty_struct *tty, int timeout)
{
	/* we always send immideate */
}

/*
 * pdc_hangup() --- called by tty_hangup() when a hangup is signaled.
 */
static void
pdc_hangup(struct tty_struct *tty)
{
}

/*
 * ------------------------------------------------------------
 * pdc_open() and friends
 * ------------------------------------------------------------
 */

static int
get_async_struct(int line, struct async_struct **ret_info)
{
	struct async_struct *info;

	info = kmalloc(sizeof (struct async_struct), GFP_KERNEL);
	if (!info) {
		return -ENOMEM;
	}
	memset(info, 0, sizeof (struct async_struct));
	init_waitqueue_head(&info->open_wait);
	init_waitqueue_head(&info->close_wait);
	init_waitqueue_head(&info->delta_msr_wait);
	info->magic = SERIAL_MAGIC;
	info->port = 0;
	info->flags = 0;
	info->io_type = 0;
	info->iomem_base = 0;
	info->iomem_reg_shift = 0;
	info->xmit_fifo_size = PDC_DUMMY_BUF;
	info->line = line;
	info->tqueue.routine = NULL;
	info->tqueue.data = info;
	info->state = NULL;
	*ret_info = info;
	return 0;
}

/*
 * This routine is called whenever a serial port is opened.  It
 * enables interrupts for a serial port, linking in its async structure into
 * the IRQ chain.   It also performs the serial-specific
 * initialization for the tty structure.
 */
static int
pdc_open(struct tty_struct *tty, struct file *filp)
{
	struct async_struct *info;
	int retval, line;
	unsigned long page;

	MOD_INC_USE_COUNT;
	line = MINOR(tty->device) - tty->driver.minor_start;
	if ((line < 0) || (line >= NR_PORTS)) {
		MOD_DEC_USE_COUNT;
		return -ENODEV;
	}
	retval = get_async_struct(line, &info);
	if (retval) {
		MOD_DEC_USE_COUNT;
		return retval;
	}
	tty->driver_data = info;
	info->tty = tty;
	pdc_drv_info = info;

#ifdef PDC_DEBUG_OPEN
	printk("pdc_open %s%d, count = %d\n", tty->driver.name, info->line,
	       info->state->count);
#endif
	info->tty->low_latency = 0;
	if (!tmp_buf) {
		page = get_zeroed_page(GFP_KERNEL);
		if (!page) {
			MOD_DEC_USE_COUNT;
			return -ENOMEM;
		}
		if (tmp_buf)
			free_page(page);
		else
			tmp_buf = (unsigned char *) page;
	}

	info->session = current->session;
	info->pgrp = current->pgrp;

#ifdef PDC_DEBUG_OPEN
	printk("pdc_open ttyB%d successful...", info->line);
#endif
	pdc_drv_refcount++;
	return 0;
}

/*
 * ---------------------------------------------------------------------
 * pdc_init() and friends
 *
 * pdc_init() is called at boot-time to initialize the pdc driver.
 * ---------------------------------------------------------------------
 */

static void
show_pdc_drv_version(void)
{
	printk(KERN_INFO "%s version %s%s (%s), %s\n", pdc_drv_name,
	       pdc_drv_version, LOCAL_VERSTRING, pdc_drv_revdate, AUTHOR);
}

/*
 * The serial driver boot-time initialization code!
 */
static int __init
pdc_drv_init(void)
{
	init_timer(&pdc_drv_timer);
	pdc_drv_timer.function = pdc_drv_poll;
	mod_timer(&pdc_drv_timer, jiffies + PDC_POLL_DELAY);

	show_pdc_drv_version();

	/* Initialize the tty_driver structure */

	memset(&pdc_drv_driver, 0, sizeof (struct tty_driver));
	pdc_drv_driver.magic = TTY_DRIVER_MAGIC;
	pdc_drv_driver.driver_name = "pdc_console";
#ifdef CONFIG_DEVFS_FS
	pdc_drv_driver.name = "ttb/%d";
#else
	pdc_drv_driver.name = "ttyB";
#endif
	pdc_drv_driver.major = MUX_MAJOR;
	pdc_drv_driver.minor_start = 0;
	pdc_drv_driver.num = NR_PORTS;
	pdc_drv_driver.type = TTY_DRIVER_TYPE_SERIAL;
	pdc_drv_driver.subtype = SERIAL_TYPE_NORMAL;
	pdc_drv_driver.init_termios = tty_std_termios;
	pdc_drv_driver.init_termios.c_cflag =
	    B9600 | CS8 | CREAD | HUPCL | CLOCAL;
	pdc_drv_driver.flags = TTY_DRIVER_REAL_RAW | TTY_DRIVER_NO_DEVFS;
	pdc_drv_driver.refcount = &pdc_drv_refcount;
	pdc_drv_driver.table = pdc_drv_table;
	pdc_drv_driver.termios = pdc_drv_termios;
	pdc_drv_driver.termios_locked = pdc_drv_termios_locked;

	pdc_drv_driver.open = pdc_open;
	pdc_drv_driver.close = pdc_close;
	pdc_drv_driver.write = pdc_write;
	pdc_drv_driver.put_char = pdc_put_char;
	pdc_drv_driver.flush_chars = pdc_flush_chars;
	pdc_drv_driver.write_room = pdc_write_room;
	pdc_drv_driver.chars_in_buffer = pdc_chars_in_buffer;
	pdc_drv_driver.flush_buffer = pdc_flush_buffer;
	pdc_drv_driver.ioctl = pdc_ioctl;
	pdc_drv_driver.throttle = pdc_throttle;
	pdc_drv_driver.unthrottle = pdc_unthrottle;
	pdc_drv_driver.set_termios = pdc_set_termios;
	pdc_drv_driver.stop = pdc_stop;
	pdc_drv_driver.start = pdc_start;
	pdc_drv_driver.hangup = pdc_hangup;
	pdc_drv_driver.break_ctl = pdc_break;
	pdc_drv_driver.send_xchar = pdc_send_xchar;
	pdc_drv_driver.wait_until_sent = pdc_wait_until_sent;
	pdc_drv_driver.read_proc = NULL;

	if (tty_register_driver(&pdc_drv_driver))
		panic("Couldn't register pdc_console driver\n");

	return 0;
}

static void __exit
pdc_fini(void)
{
	int e1;

	if ((e1 = tty_unregister_driver(&pdc_drv_driver)))
		printk("pdc_console: failed to unregister pdc_drv driver (%d)\n",
		       e1);
}

module_init(pdc_drv_init);
module_exit(pdc_fini);
MODULE_DESCRIPTION("PDC Software Console");
MODULE_AUTHOR(AUTHOR);
