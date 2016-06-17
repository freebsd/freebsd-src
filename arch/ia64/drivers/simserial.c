/*
 * Simulated Serial Driver (fake serial)
 *
 * This driver is mostly used for bringup purposes and will go away.
 * It has a strong dependency on the system console. All outputs
 * are rerouted to the same facility as the one used by printk which, in our
 * case means sys_sim.c console (goes via the simulator). The code hereafter
 * is completely leveraged from the serial.c driver.
 *
 * Copyright (C) 1999-2000, 2002-2003 Hewlett-Packard Co
 *	Stephane Eranian <eranian@hpl.hp.com>
 *	David Mosberger-Tang <davidm@hpl.hp.com>
 *
 * 02/04/00 D. Mosberger	Merged in serial.c bug fixes in rs_close().
 * 02/25/00 D. Mosberger	Synced up with 2.3.99pre-5 version of serial.c.
 */

#include <linux/config.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/major.h>
#include <linux/fcntl.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/console.h>
#include <linux/module.h>
#include <linux/serial.h>
#include <linux/serialP.h>

#include <asm/irq.h>
#include <asm/uaccess.h>

#undef SIMSERIAL_DEBUG	/* define this to get some debug information */

#define KEYBOARD_INTR	3	/* must match with simulator! */

#define NR_PORTS	1	/* only one port for now */
#define SERIAL_INLINE	1

#ifdef SERIAL_INLINE
#define _INLINE_ inline
#endif

#ifndef MIN
#define MIN(a,b)	((a) < (b) ? (a) : (b))
#endif

#define IRQ_T(info) ((info->flags & ASYNC_SHARE_IRQ) ? SA_SHIRQ : SA_INTERRUPT)

#define SSC_GETCHAR	21

extern long ia64_ssc (long, long, long, long, int);
extern void ia64_ssc_connect_irq (long intr, long irq);

static char *serial_name = "SimSerial driver";
static char *serial_version = "0.6";

/*
 * This has been extracted from asm/serial.h. We need one eventually but
 * I don't know exactly what we're going to put in it so just fake one
 * for now.
 */
#define BASE_BAUD ( 1843200 / 16 )

#define STD_COM_FLAGS (ASYNC_BOOT_AUTOCONF | ASYNC_SKIP_TEST)

/*
 * Most of the values here are meaningless to this particular driver.
 * However some values must be preserved for the code (leveraged from serial.c
 * to work correctly).
 * port must not be 0
 * type must not be UNKNOWN
 * So I picked arbitrary (guess from where?) values instead
 */
static struct serial_state rs_table[NR_PORTS]={
  /* UART CLK   PORT IRQ     FLAGS        */
  { 0, BASE_BAUD, 0x3F8, 0, STD_COM_FLAGS,0,PORT_16550 }  /* ttyS0 */
};

/*
 * Just for the fun of it !
 */
static struct serial_uart_config uart_config[] = {
	{ "unknown", 1, 0 },
	{ "8250", 1, 0 },
	{ "16450", 1, 0 },
	{ "16550", 1, 0 },
	{ "16550A", 16, UART_CLEAR_FIFO | UART_USE_FIFO },
	{ "cirrus", 1, 0 },
	{ "ST16650", 1, UART_CLEAR_FIFO | UART_STARTECH },
	{ "ST16650V2", 32, UART_CLEAR_FIFO | UART_USE_FIFO |
		  UART_STARTECH },
	{ "TI16750", 64, UART_CLEAR_FIFO | UART_USE_FIFO},
	{ 0, 0}
};

static struct tty_driver serial_driver, callout_driver;
static int serial_refcount;

static struct async_struct *IRQ_ports[NR_IRQS];
static struct tty_struct *serial_table[NR_PORTS];
static struct termios *serial_termios[NR_PORTS];
static struct termios *serial_termios_locked[NR_PORTS];

static struct console *console;

static unsigned char *tmp_buf;
static DECLARE_MUTEX(tmp_buf_sem);

extern struct console *console_drivers; /* from kernel/printk.c */

/*
 * ------------------------------------------------------------
 * rs_stop() and rs_start()
 *
 * This routines are called before setting or resetting tty->stopped.
 * They enable or disable transmitter interrupts, as necessary.
 * ------------------------------------------------------------
 */
static void rs_stop(struct tty_struct *tty)
{
#ifdef SIMSERIAL_DEBUG
	printk("rs_stop: tty->stopped=%d tty->hw_stopped=%d tty->flow_stopped=%d\n",
		tty->stopped, tty->hw_stopped, tty->flow_stopped);
#endif

}

static void rs_start(struct tty_struct *tty)
{
#if SIMSERIAL_DEBUG
	printk("rs_start: tty->stopped=%d tty->hw_stopped=%d tty->flow_stopped=%d\n",
		tty->stopped, tty->hw_stopped, tty->flow_stopped);
#endif
}

static  void receive_chars(struct tty_struct *tty, struct pt_regs *regs)
{
	unsigned char ch;
	static unsigned char seen_esc = 0;

	while ( (ch = ia64_ssc(0, 0, 0, 0, SSC_GETCHAR)) ) {
		if ( ch == 27 && seen_esc == 0 ) {
			seen_esc = 1;
			continue;
		} else {
			if ( seen_esc==1 && ch == 'O' ) {
				seen_esc = 2;
				continue;
			} else if ( seen_esc == 2 ) {
				if ( ch == 'P' ) show_state();		/* F1 key */
				if ( ch == 'Q' ) show_buffers();	/* F2 key */
				seen_esc = 0;
				continue;
			}
		}
		seen_esc = 0;
		if (tty->flip.count >= TTY_FLIPBUF_SIZE) break;

		*tty->flip.char_buf_ptr = ch;

		*tty->flip.flag_buf_ptr = 0;

		tty->flip.flag_buf_ptr++;
		tty->flip.char_buf_ptr++;
		tty->flip.count++;
	}
	tty_flip_buffer_push(tty);
}

/*
 * This is the serial driver's interrupt routine for a single port
 */
static void rs_interrupt_single(int irq, void *dev_id, struct pt_regs * regs)
{
	struct async_struct * info;

	/*
	 * I don't know exactly why they don't use the dev_id opaque data
	 * pointer instead of this extra lookup table
	 */
	info = IRQ_ports[irq];
	if (!info || !info->tty) {
		printk(KERN_INFO "simrs_interrupt_single: info|tty=0 info=%p problem\n", info);
		return;
	}
	/*
	 * pretty simple in our case, because we only get interrupts
	 * on inbound traffic
	 */
	receive_chars(info->tty, regs);
}

/*
 * -------------------------------------------------------------------
 * Here ends the serial interrupt routines.
 * -------------------------------------------------------------------
 */

#if 0
/*
 * not really used in our situation so keep them commented out for now
 */
static DECLARE_TASK_QUEUE(tq_serial); /* used to be at the top of the file */
static void do_serial_bh(void)
{
	run_task_queue(&tq_serial);
	printk(KERN_ERR "do_serial_bh: called\n");
}
#endif

static void do_softint(void *private_)
{
	printk(KERN_ERR "simserial: do_softint called\n");
}

static void rs_put_char(struct tty_struct *tty, unsigned char ch)
{
	struct async_struct *info = (struct async_struct *)tty->driver_data;
	unsigned long flags;

	if (!tty || !info->xmit.buf) return;

	save_flags(flags); cli();
	if (CIRC_SPACE(info->xmit.head, info->xmit.tail, SERIAL_XMIT_SIZE) == 0) {
		restore_flags(flags);
		return;
	}
	info->xmit.buf[info->xmit.head] = ch;
	info->xmit.head = (info->xmit.head + 1) & (SERIAL_XMIT_SIZE-1);
	restore_flags(flags);
}

static _INLINE_ void transmit_chars(struct async_struct *info, int *intr_done)
{
	int count;
	unsigned long flags;

	save_flags(flags); cli();

	if (info->x_char) {
		char c = info->x_char;

		console->write(console, &c, 1);

		info->state->icount.tx++;
		info->x_char = 0;

		goto out;
	}

	if (info->xmit.head == info->xmit.tail || info->tty->stopped || info->tty->hw_stopped) {
#ifdef SIMSERIAL_DEBUG
		printk("transmit_chars: head=%d, tail=%d, stopped=%d\n",
		       info->xmit.head, info->xmit.tail, info->tty->stopped);
#endif
		goto out;
	}
	/*
	 * We removed the loop and try to do it in to chunks. We need
	 * 2 operations maximum because it's a ring buffer.
	 *
	 * First from current to tail if possible.
	 * Then from the beginning of the buffer until necessary
	 */

	count = MIN(CIRC_CNT(info->xmit.head, info->xmit.tail, SERIAL_XMIT_SIZE),
		    SERIAL_XMIT_SIZE - info->xmit.tail);
	console->write(console, info->xmit.buf+info->xmit.tail, count);

	info->xmit.tail = (info->xmit.tail+count) & (SERIAL_XMIT_SIZE-1);

	/*
	 * We have more at the beginning of the buffer
	 */
	count = CIRC_CNT(info->xmit.head, info->xmit.tail, SERIAL_XMIT_SIZE);
	if (count) {
		console->write(console, info->xmit.buf, count);
		info->xmit.tail += count;
	}
out:
	restore_flags(flags);
}

static void rs_flush_chars(struct tty_struct *tty)
{
	struct async_struct *info = (struct async_struct *)tty->driver_data;

	if (info->xmit.head == info->xmit.tail || tty->stopped || tty->hw_stopped ||
	    !info->xmit.buf)
		return;

	transmit_chars(info, NULL);
}


static int rs_write(struct tty_struct * tty, int from_user,
		    const unsigned char *buf, int count)
{
	int	c, ret = 0;
	struct async_struct *info = (struct async_struct *)tty->driver_data;
	unsigned long flags;

	if (!tty || !info->xmit.buf || !tmp_buf) return 0;

	save_flags(flags);
	if (from_user) {
		down(&tmp_buf_sem);
		while (1) {
			int c1;
			c = CIRC_SPACE_TO_END(info->xmit.head, info->xmit.tail, SERIAL_XMIT_SIZE);
			if (count < c)
				c = count;
			if (c <= 0)
				break;

			c -= copy_from_user(tmp_buf, buf, c);
			if (!c) {
				if (!ret)
					ret = -EFAULT;
				break;
			}
			cli();
			c1 = CIRC_SPACE_TO_END(info->xmit.head, info->xmit.tail, SERIAL_XMIT_SIZE);
			if (c1 < c)
				c = c1;
			memcpy(info->xmit.buf + info->xmit.head, tmp_buf, c);
			info->xmit.head = ((info->xmit.head + c) &
					   (SERIAL_XMIT_SIZE-1));
			restore_flags(flags);
			buf += c;
			count -= c;
			ret += c;
		}
		up(&tmp_buf_sem);
	} else {
		cli();
		while (1) {
			c = CIRC_SPACE_TO_END(info->xmit.head, info->xmit.tail, SERIAL_XMIT_SIZE);
			if (count < c)
				c = count;
			if (c <= 0) {
				break;
			}
			memcpy(info->xmit.buf + info->xmit.head, buf, c);
			info->xmit.head = ((info->xmit.head + c) &
					   (SERIAL_XMIT_SIZE-1));
			buf += c;
			count -= c;
			ret += c;
		}
		restore_flags(flags);
	}
	/*
	 * Hey, we transmit directly from here in our case
	 */
	if (CIRC_CNT(info->xmit.head, info->xmit.tail, SERIAL_XMIT_SIZE)
	    && !tty->stopped && !tty->hw_stopped) {
		transmit_chars(info, NULL);
	}
	return ret;
}

static int rs_write_room(struct tty_struct *tty)
{
	struct async_struct *info = (struct async_struct *)tty->driver_data;

	return CIRC_SPACE(info->xmit.head, info->xmit.tail, SERIAL_XMIT_SIZE);
}

static int rs_chars_in_buffer(struct tty_struct *tty)
{
	struct async_struct *info = (struct async_struct *)tty->driver_data;

	return CIRC_CNT(info->xmit.head, info->xmit.tail, SERIAL_XMIT_SIZE);
}

static void rs_flush_buffer(struct tty_struct *tty)
{
	struct async_struct *info = (struct async_struct *)tty->driver_data;
	unsigned long flags;

	save_flags(flags); cli();
	info->xmit.head = info->xmit.tail = 0;
	restore_flags(flags);

	wake_up_interruptible(&tty->write_wait);

	if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) &&
	    tty->ldisc.write_wakeup)
		(tty->ldisc.write_wakeup)(tty);
}

/*
 * This function is used to send a high-priority XON/XOFF character to
 * the device
 */
static void rs_send_xchar(struct tty_struct *tty, char ch)
{
	struct async_struct *info = (struct async_struct *)tty->driver_data;

	info->x_char = ch;
	if (ch) {
		/*
		 * I guess we could call console->write() directly but
		 * let's do that for now.
		 */
		transmit_chars(info, NULL);
	}
}

/*
 * ------------------------------------------------------------
 * rs_throttle()
 *
 * This routine is called by the upper-layer tty layer to signal that
 * incoming characters should be throttled.
 * ------------------------------------------------------------
 */
static void rs_throttle(struct tty_struct * tty)
{
	if (I_IXOFF(tty)) rs_send_xchar(tty, STOP_CHAR(tty));

	printk(KERN_INFO "simrs_throttle called\n");
}

static void rs_unthrottle(struct tty_struct * tty)
{
	struct async_struct *info = (struct async_struct *)tty->driver_data;

	if (I_IXOFF(tty)) {
		if (info->x_char)
			info->x_char = 0;
		else
			rs_send_xchar(tty, START_CHAR(tty));
	}
	printk(KERN_INFO "simrs_unthrottle called\n");
}

/*
 * rs_break() --- routine which turns the break handling on or off
 */
static void rs_break(struct tty_struct *tty, int break_state)
{
}

static int rs_ioctl(struct tty_struct *tty, struct file * file,
		    unsigned int cmd, unsigned long arg)
{
	if ((cmd != TIOCGSERIAL) && (cmd != TIOCSSERIAL) &&
	    (cmd != TIOCSERCONFIG) && (cmd != TIOCSERGSTRUCT) &&
	    (cmd != TIOCMIWAIT) && (cmd != TIOCGICOUNT)) {
		if (tty->flags & (1 << TTY_IO_ERROR))
		    return -EIO;
	}

	switch (cmd) {
		case TIOCMGET:
			printk(KERN_INFO "rs_ioctl: TIOCMGET called\n");
			return -EINVAL;
		case TIOCMBIS:
		case TIOCMBIC:
		case TIOCMSET:
			printk(KERN_INFO "rs_ioctl: TIOCMBIS/BIC/SET called\n");
			return -EINVAL;
		case TIOCGSERIAL:
			printk(KERN_INFO "simrs_ioctl TIOCGSERIAL called\n");
			return 0;
		case TIOCSSERIAL:
			printk(KERN_INFO "simrs_ioctl TIOCSSERIAL called\n");
			return 0;
		case TIOCSERCONFIG:
			printk(KERN_INFO "rs_ioctl: TIOCSERCONFIG called\n");
			return -EINVAL;

		case TIOCSERGETLSR: /* Get line status register */
			printk(KERN_INFO "rs_ioctl: TIOCSERGETLSR called\n");
			return  -EINVAL;

		case TIOCSERGSTRUCT:
			printk(KERN_INFO "rs_ioctl: TIOCSERGSTRUCT called\n");
#if 0
			if (copy_to_user((struct async_struct *) arg,
					 info, sizeof(struct async_struct)))
				return -EFAULT;
#endif
			return 0;

		/*
		 * Wait for any of the 4 modem inputs (DCD,RI,DSR,CTS) to change
		 * - mask passed in arg for lines of interest
		 *   (use |'ed TIOCM_RNG/DSR/CD/CTS for masking)
		 * Caller should use TIOCGICOUNT to see which one it was
		 */
		case TIOCMIWAIT:
			printk(KERN_INFO "rs_ioctl: TIOCMIWAIT: called\n");
			return 0;
		/*
		 * Get counter of input serial line interrupts (DCD,RI,DSR,CTS)
		 * Return: write counters to the user passed counter struct
		 * NB: both 1->0 and 0->1 transitions are counted except for
		 *     RI where only 0->1 is counted.
		 */
		case TIOCGICOUNT:
			printk(KERN_INFO "rs_ioctl: TIOCGICOUNT called\n");
			return 0;

		case TIOCSERGWILD:
		case TIOCSERSWILD:
			/* "setserial -W" is called in Debian boot */
			printk (KERN_INFO "TIOCSER?WILD ioctl obsolete, ignored.\n");
			return 0;

		default:
			return -ENOIOCTLCMD;
		}
	return 0;
}

#define RELEVANT_IFLAG(iflag) (iflag & (IGNBRK|BRKINT|IGNPAR|PARMRK|INPCK))

static void rs_set_termios(struct tty_struct *tty, struct termios *old_termios)
{
	unsigned int cflag = tty->termios->c_cflag;

	if (   (cflag == old_termios->c_cflag)
	    && (   RELEVANT_IFLAG(tty->termios->c_iflag)
		== RELEVANT_IFLAG(old_termios->c_iflag)))
	  return;


	/* Handle turning off CRTSCTS */
	if ((old_termios->c_cflag & CRTSCTS) &&
	    !(tty->termios->c_cflag & CRTSCTS)) {
		tty->hw_stopped = 0;
		rs_start(tty);
	}
}
/*
 * This routine will shutdown a serial port; interrupts are disabled, and
 * DTR is dropped if the hangup on close termio flag is on.
 */
static void shutdown(struct async_struct * info)
{
	unsigned long	flags;
	struct serial_state *state;
	int		retval;

	if (!(info->flags & ASYNC_INITIALIZED)) return;

	state = info->state;

#ifdef SIMSERIAL_DEBUG
	printk("Shutting down serial port %d (irq %d)....", info->line,
	       state->irq);
#endif

	save_flags(flags); cli(); /* Disable interrupts */

	/*
	 * First unlink the serial port from the IRQ chain...
	 */
	if (info->next_port)
		info->next_port->prev_port = info->prev_port;
	if (info->prev_port)
		info->prev_port->next_port = info->next_port;
	else
		IRQ_ports[state->irq] = info->next_port;

	/*
	 * Free the IRQ, if necessary
	 */
	if (state->irq && (!IRQ_ports[state->irq] ||
			  !IRQ_ports[state->irq]->next_port)) {
		if (IRQ_ports[state->irq]) {
			free_irq(state->irq, NULL);
			retval = request_irq(state->irq, rs_interrupt_single,
					     IRQ_T(info), "serial", NULL);

			if (retval)
				printk(KERN_ERR "serial shutdown: request_irq: error %d"
				       "  Couldn't reacquire IRQ.\n", retval);
		} else
			free_irq(state->irq, NULL);
	}

	if (info->xmit.buf) {
		free_page((unsigned long) info->xmit.buf);
		info->xmit.buf = 0;
	}

	if (info->tty) set_bit(TTY_IO_ERROR, &info->tty->flags);

	info->flags &= ~ASYNC_INITIALIZED;
	restore_flags(flags);
}

/*
 * ------------------------------------------------------------
 * rs_close()
 *
 * This routine is called when the serial port gets closed.  First, we
 * wait for the last remaining data to be sent.  Then, we unlink its
 * async structure from the interrupt chain if necessary, and we free
 * that IRQ if nothing is left in the chain.
 * ------------------------------------------------------------
 */
static void rs_close(struct tty_struct *tty, struct file * filp)
{
	struct async_struct * info = (struct async_struct *)tty->driver_data;
	struct serial_state *state;
	unsigned long flags;

	if (!info ) return;

	state = info->state;

	save_flags(flags); cli();

	if (tty_hung_up_p(filp)) {
#ifdef SIMSERIAL_DEBUG
		printk("rs_close: hung_up\n");
#endif
		MOD_DEC_USE_COUNT;
		restore_flags(flags);
		return;
	}
#ifdef SIMSERIAL_DEBUG
	printk("rs_close ttys%d, count = %d\n", info->line, state->count);
#endif
	if ((tty->count == 1) && (state->count != 1)) {
		/*
		 * Uh, oh.  tty->count is 1, which means that the tty
		 * structure will be freed.  state->count should always
		 * be one in these conditions.  If it's greater than
		 * one, we've got real problems, since it means the
		 * serial port won't be shutdown.
		 */
		printk(KERN_ERR "rs_close: bad serial port count; tty->count is 1, "
		       "state->count is %d\n", state->count);
		state->count = 1;
	}
	if (--state->count < 0) {
		printk(KERN_ERR "rs_close: bad serial port count for ttys%d: %d\n",
		       info->line, state->count);
		state->count = 0;
	}
	if (state->count) {
		MOD_DEC_USE_COUNT;
		restore_flags(flags);
		return;
	}
	info->flags |= ASYNC_CLOSING;
	restore_flags(flags);

	/*
	 * Now we wait for the transmit buffer to clear; and we notify
	 * the line discipline to only process XON/XOFF characters.
	 */
	shutdown(info);
	if (tty->driver.flush_buffer) tty->driver.flush_buffer(tty);
	if (tty->ldisc.flush_buffer) tty->ldisc.flush_buffer(tty);
	info->event = 0;
	info->tty = 0;
	if (info->blocked_open) {
		if (info->close_delay) {
			current->state = TASK_INTERRUPTIBLE;
			schedule_timeout(info->close_delay);
		}
		wake_up_interruptible(&info->open_wait);
	}
	info->flags &= ~(ASYNC_NORMAL_ACTIVE|ASYNC_CALLOUT_ACTIVE|ASYNC_CLOSING);
	wake_up_interruptible(&info->close_wait);
	MOD_DEC_USE_COUNT;
}

/*
 * rs_wait_until_sent() --- wait until the transmitter is empty
 */
static void rs_wait_until_sent(struct tty_struct *tty, int timeout)
{
}


/*
 * rs_hangup() --- called by tty_hangup() when a hangup is signaled.
 */
static void rs_hangup(struct tty_struct *tty)
{
	struct async_struct * info = (struct async_struct *)tty->driver_data;
	struct serial_state *state = info->state;

#ifdef SIMSERIAL_DEBUG
	printk("rs_hangup: called\n");
#endif

	state = info->state;

	rs_flush_buffer(tty);
	if (info->flags & ASYNC_CLOSING)
		return;
	shutdown(info);

	info->event = 0;
	state->count = 0;
	info->flags &= ~(ASYNC_NORMAL_ACTIVE|ASYNC_CALLOUT_ACTIVE);
	info->tty = 0;
	wake_up_interruptible(&info->open_wait);
}


static int get_async_struct(int line, struct async_struct **ret_info)
{
	struct async_struct *info;
	struct serial_state *sstate;

	sstate = rs_table + line;
	sstate->count++;
	if (sstate->info) {
		*ret_info = sstate->info;
		return 0;
	}
	info = kmalloc(sizeof(struct async_struct), GFP_KERNEL);
	if (!info) {
		sstate->count--;
		return -ENOMEM;
	}
	memset(info, 0, sizeof(struct async_struct));
	init_waitqueue_head(&info->open_wait);
	init_waitqueue_head(&info->close_wait);
	init_waitqueue_head(&info->delta_msr_wait);
	info->magic = SERIAL_MAGIC;
	info->port = sstate->port;
	info->flags = sstate->flags;
	info->xmit_fifo_size = sstate->xmit_fifo_size;
	info->line = line;
	info->tqueue.routine = do_softint;
	info->tqueue.data = info;
	info->state = sstate;
	if (sstate->info) {
		kfree(info);
		*ret_info = sstate->info;
		return 0;
	}
	*ret_info = sstate->info = info;
	return 0;
}

static int
startup(struct async_struct *info)
{
	unsigned long flags;
	int	retval=0;
	void (*handler)(int, void *, struct pt_regs *);
	struct serial_state *state= info->state;
	unsigned long page;

	page = get_free_page(GFP_KERNEL);
	if (!page)
		return -ENOMEM;

	save_flags(flags); cli();

	if (info->flags & ASYNC_INITIALIZED) {
		free_page(page);
		goto errout;
	}

	if (!state->port || !state->type) {
		if (info->tty) set_bit(TTY_IO_ERROR, &info->tty->flags);
		free_page(page);
		goto errout;
	}
	if (info->xmit.buf)
		free_page(page);
	else
		info->xmit.buf = (unsigned char *) page;

#ifdef SIMSERIAL_DEBUG
	printk("startup: ttys%d (irq %d)...", info->line, state->irq);
#endif

	/*
	 * Allocate the IRQ if necessary
	 */
	if (state->irq && (!IRQ_ports[state->irq] ||
			  !IRQ_ports[state->irq]->next_port)) {
		if (IRQ_ports[state->irq]) {
			retval = -EBUSY;
			goto errout;
		} else
			handler = rs_interrupt_single;

		retval = request_irq(state->irq, handler, IRQ_T(info), "simserial", NULL);
		if (retval) {
			if (capable(CAP_SYS_ADMIN)) {
				if (info->tty)
					set_bit(TTY_IO_ERROR,
						&info->tty->flags);
				retval = 0;
			}
			goto errout;
		}
	}

	/*
	 * Insert serial port into IRQ chain.
	 */
	info->prev_port = 0;
	info->next_port = IRQ_ports[state->irq];
	if (info->next_port)
		info->next_port->prev_port = info;
	IRQ_ports[state->irq] = info;

	if (info->tty) clear_bit(TTY_IO_ERROR, &info->tty->flags);

	info->xmit.head = info->xmit.tail = 0;

#if 0
	/*
	 * Set up serial timers...
	 */
	timer_table[RS_TIMER].expires = jiffies + 2*HZ/100;
	timer_active |= 1 << RS_TIMER;
#endif

	/*
	 * Set up the tty->alt_speed kludge
	 */
	if (info->tty) {
		if ((info->flags & ASYNC_SPD_MASK) == ASYNC_SPD_HI)
			info->tty->alt_speed = 57600;
		if ((info->flags & ASYNC_SPD_MASK) == ASYNC_SPD_VHI)
			info->tty->alt_speed = 115200;
		if ((info->flags & ASYNC_SPD_MASK) == ASYNC_SPD_SHI)
			info->tty->alt_speed = 230400;
		if ((info->flags & ASYNC_SPD_MASK) == ASYNC_SPD_WARP)
			info->tty->alt_speed = 460800;
	}

	info->flags |= ASYNC_INITIALIZED;
	restore_flags(flags);
	return 0;

errout:
	restore_flags(flags);
	return retval;
}


/*
 * This routine is called whenever a serial port is opened.  It
 * enables interrupts for a serial port, linking in its async structure into
 * the IRQ chain.   It also performs the serial-specific
 * initialization for the tty structure.
 */
static int rs_open(struct tty_struct *tty, struct file * filp)
{
	struct async_struct	*info;
	int			retval, line;
	unsigned long		page;

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

#ifdef SIMSERIAL_DEBUG
	printk("rs_open %s%d, count = %d\n", tty->driver.name, info->line,
	       info->state->count);
#endif
	info->tty->low_latency = (info->flags & ASYNC_LOW_LATENCY) ? 1 : 0;

	if (!tmp_buf) {
		page = get_free_page(GFP_KERNEL);
		if (!page) {
			/* MOD_DEC_USE_COUNT; "info->tty" will cause this? */
			return -ENOMEM;
		}
		if (tmp_buf)
			free_page(page);
		else
			tmp_buf = (unsigned char *) page;
	}

	/*
	 * If the port is the middle of closing, bail out now
	 */
	if (tty_hung_up_p(filp) ||
	    (info->flags & ASYNC_CLOSING)) {
		if (info->flags & ASYNC_CLOSING)
			interruptible_sleep_on(&info->close_wait);
		/* MOD_DEC_USE_COUNT; "info->tty" will cause this? */
#ifdef SERIAL_DO_RESTART
		return ((info->flags & ASYNC_HUP_NOTIFY) ?
			-EAGAIN : -ERESTARTSYS);
#else
		return -EAGAIN;
#endif
	}

	/*
	 * Start up serial port
	 */
	retval = startup(info);
	if (retval) {
		/* MOD_DEC_USE_COUNT; "info->tty" will cause this? */
		return retval;
	}

	if ((info->state->count == 1) &&
	    (info->flags & ASYNC_SPLIT_TERMIOS)) {
		if (tty->driver.subtype == SERIAL_TYPE_NORMAL)
			*tty->termios = info->state->normal_termios;
		else
			*tty->termios = info->state->callout_termios;
	}

	/*
	 * figure out which console to use (should be one already)
	 */
	console = console_drivers;
	while (console) {
		if ((console->flags & CON_ENABLED) && console->write) break;
		console = console->next;
	}

	info->session = current->session;
	info->pgrp = current->pgrp;

#ifdef SIMSERIAL_DEBUG
	printk("rs_open ttys%d successful\n", info->line);
#endif
	return 0;
}

/*
 * /proc fs routines....
 */

static inline int line_info(char *buf, struct serial_state *state)
{
	return sprintf(buf, "%d: uart:%s port:%lX irq:%d\n",
		       state->line, uart_config[state->type].name,
		       state->port, state->irq);
}

static int rs_read_proc(char *page, char **start, off_t off, int count,
		 int *eof, void *data)
{
	int i, len = 0, l;
	off_t	begin = 0;

	len += sprintf(page, "simserinfo:1.0 driver:%s\n", serial_version);
	for (i = 0; i < NR_PORTS && len < 4000; i++) {
		l = line_info(page + len, &rs_table[i]);
		len += l;
		if (len+begin > off+count)
			goto done;
		if (len+begin < off) {
			begin += len;
			len = 0;
		}
	}
	*eof = 1;
done:
	if (off >= len+begin)
		return 0;
	*start = page + (begin-off);
	return ((count < begin+len-off) ? count : begin+len-off);
}

/*
 * ---------------------------------------------------------------------
 * rs_init() and friends
 *
 * rs_init() is called at boot-time to initialize the serial driver.
 * ---------------------------------------------------------------------
 */

/*
 * This routine prints out the appropriate serial driver version
 * number, and identifies which options were configured into this
 * driver.
 */
static inline void show_serial_version(void)
{
	printk(KERN_INFO "%s version %s with", serial_name, serial_version);
	printk(KERN_INFO " no serial options enabled\n");
}

/*
 * The serial driver boot-time initialization code!
 */
static int __init
simrs_init (void)
{
	int			i;
	struct serial_state	*state;

	show_serial_version();

	/* Initialize the tty_driver structure */

	memset(&serial_driver, 0, sizeof(struct tty_driver));
	serial_driver.magic = TTY_DRIVER_MAGIC;
	serial_driver.driver_name = "simserial";
	serial_driver.name = "ttyS";
	serial_driver.major = TTY_MAJOR;
	serial_driver.minor_start = 64;
	serial_driver.num = 1;
	serial_driver.type = TTY_DRIVER_TYPE_SERIAL;
	serial_driver.subtype = SERIAL_TYPE_NORMAL;
	serial_driver.init_termios = tty_std_termios;
	serial_driver.init_termios.c_cflag =
		B9600 | CS8 | CREAD | HUPCL | CLOCAL;
	serial_driver.flags = TTY_DRIVER_REAL_RAW;
	serial_driver.refcount = &serial_refcount;
	serial_driver.table = serial_table;
	serial_driver.termios = serial_termios;
	serial_driver.termios_locked = serial_termios_locked;

	serial_driver.open = rs_open;
	serial_driver.close = rs_close;
	serial_driver.write = rs_write;
	serial_driver.put_char = rs_put_char;
	serial_driver.flush_chars = rs_flush_chars;
	serial_driver.write_room = rs_write_room;
	serial_driver.chars_in_buffer = rs_chars_in_buffer;
	serial_driver.flush_buffer = rs_flush_buffer;
	serial_driver.ioctl = rs_ioctl;
	serial_driver.throttle = rs_throttle;
	serial_driver.unthrottle = rs_unthrottle;
	serial_driver.send_xchar = rs_send_xchar;
	serial_driver.set_termios = rs_set_termios;
	serial_driver.stop = rs_stop;
	serial_driver.start = rs_start;
	serial_driver.hangup = rs_hangup;
	serial_driver.break_ctl = rs_break;
	serial_driver.wait_until_sent = rs_wait_until_sent;
	serial_driver.read_proc = rs_read_proc;

	/*
	 * Let's have a little bit of fun !
	 */
	for (i = 0, state = rs_table; i < NR_PORTS; i++,state++) {

		if (state->type == PORT_UNKNOWN) continue;

		if (!state->irq) {
			state->irq = ia64_alloc_vector();
			ia64_ssc_connect_irq(KEYBOARD_INTR, state->irq);
		}

		printk(KERN_INFO "ttyS%02d at 0x%04lx (irq = %d) is a %s\n",
		       state->line,
		       state->port, state->irq,
		       uart_config[state->type].name);
	}
	/*
	 * The callout device is just like normal device except for
	 * major number and the subtype code.
	 */
	callout_driver = serial_driver;
	callout_driver.name = "cua";
	callout_driver.major = TTYAUX_MAJOR;
	callout_driver.subtype = SERIAL_TYPE_CALLOUT;
	callout_driver.read_proc = 0;
	callout_driver.proc_entry = 0;

	if (tty_register_driver(&serial_driver))
		panic("Couldn't register simserial driver\n");

	if (tty_register_driver(&callout_driver))
		panic("Couldn't register callout driver\n");

	return 0;
}

#ifndef MODULE
__initcall(simrs_init);
#endif
