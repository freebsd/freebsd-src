/*
 * C-Brick Serial Port (and console) driver for SGI Altix machines.
 *
 * This driver is NOT suitable for talking to the l1-controller for
 * anything other than 'console activities' --- please use the l1
 * driver for that.
 *
 *
 * Copyright (c) 2003 Silicon Graphics, Inc.  All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Further, this software is distributed without any warranty that it is
 * free of the rightful claim of any third person regarding infringement
 * or the like.  Any license provided herein, whether implied or
 * otherwise, applies only to this software file.  Patent licenses, if
 * any, provided herein do not apply to combinations of this program with
 * other software, or any other product whatsoever.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * Contact information:  Silicon Graphics, Inc., 1600 Amphitheatre Pkwy,
 * Mountain View, CA  94043, or:
 *
 * http://www.sgi.com
 *
 * For further information regarding this notice, see:
 *
 * http://oss.sgi.com/projects/GenInfo/NoticeExplan
 */

#include <linux/config.h>
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/serial.h>
#include <linux/console.h>
#include <linux/module.h>
#include <linux/sysrq.h>
#include <linux/circ_buf.h>
#include <linux/serial_reg.h>
#include <asm/uaccess.h>
#include <asm/sn/sn_sal.h>
#include <asm/sn/pci/pciio.h>		/* this is needed for get_console_nasid */
#include <asm/sn/simulator.h>
#include <asm/sn/sn2/sn_private.h>

#if defined(CONFIG_SGI_L1_SERIAL_CONSOLE) && defined(CONFIG_MAGIC_SYSRQ)
static char sysrq_serial_str[] = "\eSYS";
static char *sysrq_serial_ptr = sysrq_serial_str;
static unsigned long sysrq_requested;
#endif /* CONFIG_SGI_L1_SERIAL_CONSOLE && CONFIG_MAGIC_SYSRQ */

/* minor device number */
#define SN_SAL_MINOR 64

#define SN_SAL_SUBTYPE	1

/* number of characters left in xmit buffer before we ask for more */
#define WAKEUP_CHARS 128

/* number of characters we can transmit to the SAL console at a time */
#define SN_SAL_MAX_CHARS 120

#define SN_SAL_EVENT_WRITE_WAKEUP 0

#define CONSOLE_RESTART 1


/* 64K, when we're asynch, it must be at least printk's LOG_BUF_LEN to
 * avoid losing chars, (always has to be a power of 2) */
#define SN_SAL_BUFFER_SIZE (64 * (1 << 10))

#define SN_SAL_UART_FIFO_DEPTH 16
#define SN_SAL_UART_FIFO_SPEED_CPS 9600/10

/* we don't kmalloc/get_free_page these as we want them available
 * before either of those are initialized */
static char sn_xmit_buff_mem[SN_SAL_BUFFER_SIZE];

struct volatile_circ_buf {
	char *cb_buf;
	int cb_head;
	int cb_tail;
};

static struct volatile_circ_buf xmit = { .cb_buf = sn_xmit_buff_mem };
static char sn_tmp_buffer[SN_SAL_BUFFER_SIZE];

static struct tty_struct *sn_sal_tty;

static struct timer_list sn_sal_timer;
static int sn_sal_event; /* event type for task queue */
static int sn_sal_refcount;

static int sn_sal_is_asynch;
static int sn_sal_irq;
static spinlock_t sn_sal_lock = SPIN_LOCK_UNLOCKED;
static int sn_total_tx_count;
static int sn_total_rx_count;

static struct tty_struct *sn_sal_table;
static struct termios *sn_sal_termios;
static struct termios *sn_sal_termios_locked;

static void sn_sal_tasklet_action(unsigned long data);
static DECLARE_TASKLET(sn_sal_tasklet, sn_sal_tasklet_action, 0);

static unsigned long sn_interrupt_timeout;

extern u64 master_node_bedrock_address;

static int sn_debug_printf(const char *fmt, ...);

#undef DEBUG
#ifdef DEBUG
#define DPRINTF(x...) sn_debug_printf(x)
#else
#define DPRINTF(x...) do { } while (0)
#endif

struct sn_sal_ops {
	int (*sal_puts)(const char *s, int len);
	int (*sal_getc)(void);
	int (*sal_input_pending)(void);
	void (*sal_wakeup_transmit)(void);
};

/* This is the pointer used. It is assigned to point to one of
 * the tables below.
 */
static struct sn_sal_ops *sn_func;

/* Prototypes */
static int snt_hw_puts(const char *, int);
static int snt_poll_getc(void);
static int snt_poll_input_pending(void);
static int snt_sim_puts(const char *, int);
static int snt_sim_getc(void);
static int snt_sim_input_pending(void);
static int snt_intr_getc(void);
static int snt_intr_input_pending(void);
static void sn_intr_transmit_chars(void);

/* A table for polling */
static struct sn_sal_ops poll_ops = {
	.sal_puts		= snt_hw_puts,
	.sal_getc 		= snt_poll_getc,
	.sal_input_pending	= snt_poll_input_pending
};

/* A table for the simulator */
static struct sn_sal_ops sim_ops = {
	.sal_puts		= snt_sim_puts,
	.sal_getc		= snt_sim_getc,
	.sal_input_pending	= snt_sim_input_pending
};

/* A table for interrupts enabled */
static struct sn_sal_ops intr_ops = {
	.sal_puts		= snt_hw_puts,
	.sal_getc		= snt_intr_getc,
	.sal_input_pending	= snt_intr_input_pending,
	.sal_wakeup_transmit	= sn_intr_transmit_chars
};


/* the console does output in two distinctly different ways:
 * synchronous and asynchronous (buffered).  initally, early_printk
 * does synchronous output.  any data written goes directly to the SAL
 * to be output (incidentally, it is internally buffered by the SAL)
 * after interrupts and timers are initialized and available for use,
 * the console init code switches to asynchronous output.  this is
 * also the earliest opportunity to begin polling for console input.
 * after console initialization, console output and tty (serial port)
 * output is buffered and sent to the SAL asynchronously (either by
 * timer callback or by UART interrupt) */


/* routines for running the console in polling mode */

static int
snt_hw_puts(const char *s, int len)
{
	/* looking at the PROM source code, putb calls the flush
	 * routine, so if we send characters in FIFO sized chunks, it
	 * should go out by the next time the timer gets called */
	return ia64_sn_console_putb(s, len);
}

static int
snt_poll_getc(void)
{
	int ch;
	ia64_sn_console_getc(&ch);
	return ch;
}

static int
snt_poll_input_pending(void)
{
	int status, input;

	status = ia64_sn_console_check(&input);
	return !status && input;
}


/* routines for running the console on the simulator */

static int
snt_sim_puts(const char *str, int count)
{
	int counter = count;

#ifdef FLAG_DIRECT_CONSOLE_WRITES
	/* This is an easy way to pre-pend the output to know whether the output
	 * was done via sal or directly */
	writeb('[', master_node_bedrock_address + (UART_TX << 3));
	writeb('+', master_node_bedrock_address + (UART_TX << 3));
	writeb(']', master_node_bedrock_address + (UART_TX << 3));
	writeb(' ', master_node_bedrock_address + (UART_TX << 3));
#endif /* FLAG_DIRECT_CONSOLE_WRITES */
	while (counter > 0) {
		writeb(*str, master_node_bedrock_address + (UART_TX << 3));
		counter--;
		str++;
	}

	return count;
}

static int
snt_sim_getc(void)
{
	return readb(master_node_bedrock_address + (UART_RX << 3));
}

static int
snt_sim_input_pending(void)
{
	return readb(master_node_bedrock_address + (UART_LSR << 3)) & UART_LSR_DR;
}


/* routines for an interrupt driven console (normal) */

static int
snt_intr_getc(void)
{
	return ia64_sn_console_readc();
}

static int
snt_intr_input_pending(void)
{
	return ia64_sn_console_intr_status() & SAL_CONSOLE_INTR_RECV;
}

/* The early printk (possible setup) and function call */

void
early_printk_sn_sal(const char *s, unsigned count)
{
#if defined(CONFIG_IA64_EARLY_PRINTK_SGI_SN) || defined(CONFIG_IA64_SGI_SN_SIM)
	extern void early_sn_setup(void);
#endif

	if (!sn_func) {
		if (IS_RUNNING_ON_SIMULATOR())
			sn_func = &sim_ops;
		else
			sn_func = &poll_ops;

#if defined(CONFIG_IA64_EARLY_PRINTK_SGI_SN) || defined(CONFIG_IA64_SGI_SN_SIM)
		early_sn_setup();
#endif
	}
	sn_func->sal_puts(s, count);
}

/* this is as "close to the metal" as we can get, used when the driver
 * itself may be broken */
static int
sn_debug_printf(const char *fmt, ...)
{
	static char printk_buf[1024];
	int printed_len;
	va_list args;

	va_start(args, fmt);
	printed_len = vsnprintf(printk_buf, sizeof(printk_buf), fmt, args);
	early_printk_sn_sal(printk_buf, printed_len);
	va_end(args);
	return printed_len;
}

/*
 * Interrupt handling routines.
 */

static void
sn_sal_sched_event(int event)
{
	sn_sal_event |= (1 << event);
	tasklet_schedule(&sn_sal_tasklet);
}

/* sn_receive_chars can be called before sn_sal_tty is initialized.  in
 * that case, its only use is to trigger sysrq and kdb */
static void
sn_receive_chars(struct pt_regs *regs, unsigned long *flags)
{
	int ch;

	while (sn_func->sal_input_pending()) {
		ch = sn_func->sal_getc();
		if (ch < 0) {
			printk(KERN_ERR "sn_serial: An error occured while "
			       "obtaining data from the console (0x%0x)\n", ch);
			break;
		}
#if defined(CONFIG_SGI_L1_SERIAL_CONSOLE) && defined(CONFIG_MAGIC_SYSRQ)
		if (sysrq_requested) {
			unsigned long sysrq_timeout = sysrq_requested + HZ*5;

			sysrq_requested = 0;
			if (ch && time_before(jiffies, sysrq_timeout)) {
				spin_unlock_irqrestore(&sn_sal_lock, *flags);
				handle_sysrq(ch, regs, NULL, NULL);
				spin_lock_irqsave(&sn_sal_lock, *flags);
				/* don't record this char */
				continue;
			}
		}
		if (ch == *sysrq_serial_ptr) {
			if (!(*++sysrq_serial_ptr)) {
				sysrq_requested = jiffies;
				sysrq_serial_ptr = sysrq_serial_str;
			}
		} else
			sysrq_serial_ptr = sysrq_serial_str;
#endif /* CONFIG_SGI_L1_SERIAL_CONSOLE && CONFIG_MAGIC_SYSRQ */

		/* record the character to pass up to the tty layer */
		if (sn_sal_tty) {
			*sn_sal_tty->flip.char_buf_ptr = ch;
			sn_sal_tty->flip.char_buf_ptr++;
			sn_sal_tty->flip.count++;
			if (sn_sal_tty->flip.count == TTY_FLIPBUF_SIZE)
				break;
		}
		sn_total_rx_count++;
	}

	if (sn_sal_tty)
		tty_flip_buffer_push((struct tty_struct *)sn_sal_tty);
}


/* synch_flush_xmit must be called with sn_sal_lock */
static void
synch_flush_xmit(void)
{
	int xmit_count, tail, head, loops, ii;
	int result;
	char *start;

	if (xmit.cb_head == xmit.cb_tail)
		return; /* Nothing to do. */

	head = xmit.cb_head;
	tail = xmit.cb_tail;
	start = &xmit.cb_buf[tail];

	/* twice around gets the tail to the end of the buffer and
	 * then to the head, if needed */
	loops = (head < tail) ? 2 : 1;

	for (ii = 0; ii < loops; ii++) {
		xmit_count = (head < tail) ?  (SN_SAL_BUFFER_SIZE - tail) : (head - tail);

		if (xmit_count > 0) {
			result = sn_func->sal_puts((char *)start, xmit_count);
			if (!result)
				sn_debug_printf("\n*** synch_flush_xmit failed to flush\n");
			if (result > 0) {
				xmit_count -= result;
				sn_total_tx_count += result;
				tail += result;
				tail &= SN_SAL_BUFFER_SIZE - 1;
				xmit.cb_tail = tail;
				start = (char *)&xmit.cb_buf[tail];
			}
		}
	}
}

/* must be called with a lock protecting the circular buffer and
 * sn_sal_tty */
static void
sn_poll_transmit_chars(void)
{
	int xmit_count, tail, head;
	int result;
	char *start;

	BUG_ON(!sn_sal_is_asynch);

	if (xmit.cb_head == xmit.cb_tail ||
	    (sn_sal_tty && (sn_sal_tty->stopped || sn_sal_tty->hw_stopped))) {
		/* Nothing to do. */
		return;
	}

	head = xmit.cb_head;
	tail = xmit.cb_tail;
	start = &xmit.cb_buf[tail];

	xmit_count = (head < tail) ?  (SN_SAL_BUFFER_SIZE - tail) : (head - tail);

	if (xmit_count == 0)
		sn_debug_printf("\n*** empty xmit_count\n");

	/* use the ops, as we could be on the simulator */
	result = sn_func->sal_puts((char *)start, xmit_count);
	if (!result)
		sn_debug_printf("\n*** error in synchronous sal_puts\n");
	/* XXX chadt clean this up */
	if (result > 0) {
		xmit_count -= result;
		sn_total_tx_count += result;
		tail += result;
		tail &= SN_SAL_BUFFER_SIZE - 1;
		xmit.cb_tail = tail;
		start = &xmit.cb_buf[tail];
	}

	/* if there's few enough characters left in the xmit buffer
	 * that we could stand for the upper layer to send us some
	 * more, ask for it. */
	if (sn_sal_tty)
		if (CIRC_CNT(xmit.cb_head, xmit.cb_tail, SN_SAL_BUFFER_SIZE) < WAKEUP_CHARS)
			sn_sal_sched_event(SN_SAL_EVENT_WRITE_WAKEUP);
}


/* must be called with a lock protecting the circular buffer and
 * sn_sal_tty */
static void
sn_intr_transmit_chars(void)
{
	int xmit_count, tail, head, loops, ii;
	int result;
	char *start;

	BUG_ON(!sn_sal_is_asynch);

	if (xmit.cb_head == xmit.cb_tail ||
	    (sn_sal_tty && (sn_sal_tty->stopped || sn_sal_tty->hw_stopped))) {
		/* Nothing to do. */
		return;
	}

	head = xmit.cb_head;
	tail = xmit.cb_tail;
	start = &xmit.cb_buf[tail];

	/* twice around gets the tail to the end of the buffer and
	 * then to the head, if needed */
	loops = (head < tail) ? 2 : 1;

	for (ii = 0; ii < loops; ii++) {
		xmit_count = (head < tail) ?
			(SN_SAL_BUFFER_SIZE - tail) : (head - tail);

		if (xmit_count > 0) {
			result = ia64_sn_console_xmit_chars((char *)start, xmit_count);
#ifdef DEBUG
			if (!result)
				sn_debug_printf("`");
#endif
			if (result > 0) {
				xmit_count -= result;
				sn_total_tx_count += result;
				tail += result;
				tail &= SN_SAL_BUFFER_SIZE - 1;
				xmit.cb_tail = tail;
				start = &xmit.cb_buf[tail];
			}
		}
	}

	/* if there's few enough characters left in the xmit buffer
	 * that we could stand for the upper layer to send us some
	 * more, ask for it. */
	if (sn_sal_tty)
		if (CIRC_CNT(xmit.cb_head, xmit.cb_tail, SN_SAL_BUFFER_SIZE) < WAKEUP_CHARS)
			sn_sal_sched_event(SN_SAL_EVENT_WRITE_WAKEUP);
}


static void
sn_sal_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	/* this call is necessary to pass the interrupt back to the
	 * SAL, since it doesn't intercept the UART interrupts
	 * itself */
	int status = ia64_sn_console_intr_status();
	unsigned long flags;

	spin_lock_irqsave(&sn_sal_lock, flags);
	if (status & SAL_CONSOLE_INTR_RECV)
		sn_receive_chars(regs, &flags);
	if (status & SAL_CONSOLE_INTR_XMIT)
		sn_intr_transmit_chars();
	spin_unlock_irqrestore(&sn_sal_lock, flags);
}


/* returns the console irq if interrupt is successfully registered,
 * else 0 */
static int
sn_sal_connect_interrupt(void)
{
	cpuid_t intr_cpuid;
	unsigned int intr_cpuloc;
	nasid_t console_nasid;
	unsigned int console_irq;
	int result;

	/* if it is an old prom, run in poll mode */
	if ((sn_sal_rev_major() <= 1) && (sn_sal_rev_minor() <= 3)) {
		/* before version 1.06 doesn't work */
		printk(KERN_INFO "sn_serial: old prom version %x.%02x"
		       " - running in polled mode\n",
		       sn_sal_rev_major(), sn_sal_rev_minor());
		return 0;
	}

	console_nasid = ia64_sn_get_console_nasid();
	intr_cpuid = NODEPDA(NASID_TO_COMPACT_NODEID(console_nasid))->node_first_cpu;
	intr_cpuloc = cpu_physical_id(intr_cpuid);
	console_irq = CPU_VECTOR_TO_IRQ(intr_cpuloc, SGI_UART_VECTOR);

	result = intr_connect_level(intr_cpuid, SGI_UART_VECTOR, 0 /*not used*/, 0 /*not used*/);
	BUG_ON(result != SGI_UART_VECTOR);

	result = request_irq(console_irq, sn_sal_interrupt, SA_INTERRUPT,
		       			"SAL console driver", &sn_sal_tty);
	if (result >= 0)
		return console_irq;

	printk(KERN_INFO "sn_serial: console proceeding in polled mode\n");
	return 0;
}

static void
sn_sal_tasklet_action(unsigned long data)
{
	unsigned long flags;

	if (sn_sal_tty) {
		spin_lock_irqsave(&sn_sal_lock, flags);
		if (sn_sal_tty) {
			if (test_and_clear_bit(SN_SAL_EVENT_WRITE_WAKEUP, &sn_sal_event)) {
				if ((sn_sal_tty->flags & (1 << TTY_DO_WRITE_WAKEUP))
							&& sn_sal_tty->ldisc.write_wakeup)
					(sn_sal_tty->ldisc.write_wakeup)((struct tty_struct *)sn_sal_tty);
				wake_up_interruptible((wait_queue_head_t *)&sn_sal_tty->write_wait);
			}
		}
		spin_unlock_irqrestore(&sn_sal_lock, flags);
	}
}


/*
 * This function handles polled mode.
 */
static void
sn_sal_timer_poll(unsigned long dummy)
{
	unsigned long flags;

	if (!sn_sal_irq) {
		spin_lock_irqsave(&sn_sal_lock, flags);
		sn_receive_chars(NULL, &flags);
		sn_poll_transmit_chars();
		spin_unlock_irqrestore(&sn_sal_lock, flags);
		mod_timer(&sn_sal_timer, jiffies + sn_interrupt_timeout);
	}
}

static void
sn_sal_timer_restart(unsigned long dummy)
{
	unsigned long flags;

	local_irq_save(flags);
	sn_sal_interrupt(0, NULL, NULL);
	local_irq_restore(flags);
	mod_timer(&sn_sal_timer, jiffies + sn_interrupt_timeout);
}

/*
 * User-level console routines
 */

static int
sn_sal_open(struct tty_struct *tty, struct file *filp)
{
	unsigned long flags;

	DPRINTF("sn_sal_open: sn_sal_tty = %p, tty = %p, filp = %p\n",
		sn_sal_tty, tty, filp);

	spin_lock_irqsave(&sn_sal_lock, flags);
	if (!sn_sal_tty)
		sn_sal_tty = tty;
	spin_unlock_irqrestore(&sn_sal_lock, flags);

	return 0;
}


/* We're keeping all our resources.  We're keeping interrupts turned
 * on.  Maybe just let the tty layer finish its stuff...? GMSH
 */
static void
sn_sal_close(struct tty_struct *tty, struct file * filp)
{
	if (tty->count == 1) {
		unsigned long flags;
		tty->closing = 1;
		if (tty->driver.flush_buffer)
			tty->driver.flush_buffer(tty);
		if (tty->ldisc.flush_buffer)
			tty->ldisc.flush_buffer(tty);
		tty->closing = 0;
		spin_lock_irqsave(&sn_sal_lock, flags);
		sn_sal_tty = NULL;
		spin_unlock_irqrestore(&sn_sal_lock, flags);
	}
}


static int
sn_sal_write(struct tty_struct *tty, int from_user,
	     const unsigned char *buf, int count)
{
	int c, ret = 0;
	unsigned long flags;

	if (from_user) {
		while (1) {
			int c1;
			c = CIRC_SPACE_TO_END(xmit.cb_head, xmit.cb_tail,
					      SN_SAL_BUFFER_SIZE);

			if (count < c)
				c = count;
			if (c <= 0)
				break;

			c -= copy_from_user(sn_tmp_buffer, buf, c);
			if (!c) {
				if (!ret)
					ret = -EFAULT;
				break;
			}

			/* Turn off interrupts and see if the xmit buffer has
			 * moved since the last time we looked.
			 */
			spin_lock_irqsave(&sn_sal_lock, flags);
			c1 = CIRC_SPACE_TO_END(xmit.cb_head, xmit.cb_tail, SN_SAL_BUFFER_SIZE);

			if (c1 < c)
				c = c1;

			memcpy(xmit.cb_buf + xmit.cb_head, sn_tmp_buffer, c);
			xmit.cb_head = ((xmit.cb_head + c) & (SN_SAL_BUFFER_SIZE - 1));
			spin_unlock_irqrestore(&sn_sal_lock, flags);

			buf += c;
			count -= c;
			ret += c;
		}
	} else {
		/* The buffer passed in isn't coming from userland,
		 * so cut out the middleman (sn_tmp_buffer).
		 */
		spin_lock_irqsave(&sn_sal_lock, flags);
		while (1) {
			c = CIRC_SPACE_TO_END(xmit.cb_head, xmit.cb_tail, SN_SAL_BUFFER_SIZE);

			if (count < c)
				c = count;
			if (c <= 0) {
				break;
			}
			memcpy(xmit.cb_buf + xmit.cb_head, buf, c);
			xmit.cb_head = ((xmit.cb_head + c) & (SN_SAL_BUFFER_SIZE - 1));
			buf += c;
			count -= c;
			ret += c;
		}
		spin_unlock_irqrestore(&sn_sal_lock, flags);
	}

	spin_lock_irqsave(&sn_sal_lock, flags);
	if (xmit.cb_head != xmit.cb_tail && !(tty && (tty->stopped || tty->hw_stopped)))
		if (sn_func->sal_wakeup_transmit)
			sn_func->sal_wakeup_transmit();
	spin_unlock_irqrestore(&sn_sal_lock, flags);

	return ret;
}


static void
sn_sal_put_char(struct tty_struct *tty, unsigned char ch)
{
	unsigned long flags;

	spin_lock_irqsave(&sn_sal_lock, flags);
	if (CIRC_SPACE(xmit.cb_head, xmit.cb_tail, SN_SAL_BUFFER_SIZE) != 0) {
		xmit.cb_buf[xmit.cb_head] = ch;
		xmit.cb_head = (xmit.cb_head + 1) & (SN_SAL_BUFFER_SIZE-1);
		if ( sn_func->sal_wakeup_transmit )
			sn_func->sal_wakeup_transmit();
	}
	spin_unlock_irqrestore(&sn_sal_lock, flags);
}


static void
sn_sal_flush_chars(struct tty_struct *tty)
{
	unsigned long flags;

	spin_lock_irqsave(&sn_sal_lock, flags);
	if (CIRC_CNT(xmit.cb_head, xmit.cb_tail, SN_SAL_BUFFER_SIZE))
		if (sn_func->sal_wakeup_transmit)
			sn_func->sal_wakeup_transmit();
	spin_unlock_irqrestore(&sn_sal_lock, flags);
}


static int
sn_sal_write_room(struct tty_struct *tty)
{
	unsigned long flags;
	int space;

	spin_lock_irqsave(&sn_sal_lock, flags);
	space = CIRC_SPACE(xmit.cb_head, xmit.cb_tail, SN_SAL_BUFFER_SIZE);
	spin_unlock_irqrestore(&sn_sal_lock, flags);
	return space;
}


static int
sn_sal_chars_in_buffer(struct tty_struct *tty)
{
	unsigned long flags;
	int space;

	spin_lock_irqsave(&sn_sal_lock, flags);
	space = CIRC_CNT(xmit.cb_head, xmit.cb_tail, SN_SAL_BUFFER_SIZE);
	DPRINTF("<%d>", space);
	spin_unlock_irqrestore(&sn_sal_lock, flags);
	return space;
}


static void
sn_sal_flush_buffer(struct tty_struct *tty)
{
	unsigned long flags;

	/* drop everything */
	spin_lock_irqsave(&sn_sal_lock, flags);
	xmit.cb_head = xmit.cb_tail = 0;
	spin_unlock_irqrestore(&sn_sal_lock, flags);

	/* wake up tty level */
	wake_up_interruptible(&tty->write_wait);
	if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) && tty->ldisc.write_wakeup)
		(tty->ldisc.write_wakeup)(tty);
}


static void
sn_sal_hangup(struct tty_struct *tty)
{
	sn_sal_flush_buffer(tty);
}


static void
sn_sal_wait_until_sent(struct tty_struct *tty, int timeout)
{
	/* this is SAL's problem */
	DPRINTF("<sn_serial: should wait until sent>");
}


/*
 * sn_sal_read_proc
 *
 * Console /proc interface
 */

static int
sn_sal_read_proc(char *page, char **start, off_t off, int count,
		   int *eof, void *data)
{
	int len = 0;
	off_t	begin = 0;

	len += sprintf(page, "sn_serial: nasid:%d irq:%d tx:%d rx:%d\n",
		       snia_get_console_nasid(), sn_sal_irq,
		       sn_total_tx_count, sn_total_rx_count);
	*eof = 1;

	if (off >= len+begin)
		return 0;
	*start = page + (off-begin);

	return count < begin+len-off ? count : begin+len-off;
}


static struct tty_driver sn_sal_driver = {
	.magic		= TTY_DRIVER_MAGIC,
	.driver_name	= "sn_serial",
#if defined(CONFIG_DEVFS_FS)
	.name		= "tts/%d",
#else
	.name		= "ttyS",
#endif
	.major		= TTY_MAJOR,
	.minor_start	= SN_SAL_MINOR,
	.num		= 1,
	.type		= TTY_DRIVER_TYPE_SERIAL,
	.subtype	= SN_SAL_SUBTYPE,
	.flags		= TTY_DRIVER_REAL_RAW | TTY_DRIVER_NO_DEVFS,
	.refcount	= &sn_sal_refcount,
	.table		= &sn_sal_table,
	.termios	= &sn_sal_termios,
	.termios_locked = &sn_sal_termios_locked,

	.open		= sn_sal_open,
	.close		= sn_sal_close,
	.write		= sn_sal_write,
	.put_char	= sn_sal_put_char,
	.flush_chars	= sn_sal_flush_chars,
	.write_room	= sn_sal_write_room,
	.chars_in_buffer = sn_sal_chars_in_buffer,
	.hangup		= sn_sal_hangup,
	.wait_until_sent = sn_sal_wait_until_sent,
	.read_proc	= sn_sal_read_proc,
};

/* sn_sal_init wishlist:
 * - allocate sn_tmp_buffer
 * - fix up the tty_driver struct
 * - turn on receive interrupts
 * - do any termios twiddling once and for all
 */

/*
 * Boot-time initialization code
 */

static void __init
sn_sal_switch_to_asynch(void)
{
	unsigned long flags;

	sn_debug_printf("sn_serial: about to switch to asynchronous console\n");

	/* without early_printk, we may be invoked late enough to race
	 * with other cpus doing console IO at this point, however
	 * console interrupts will never be enabled */
	spin_lock_irqsave(&sn_sal_lock, flags);

	/* early_printk invocation may have done this for us */
	if (!sn_func) {
		if (IS_RUNNING_ON_SIMULATOR())
			sn_func = &sim_ops;
		else
			sn_func = &poll_ops;
	}

	/* we can't turn on the console interrupt (as request_irq
	 * calls kmalloc, which isn't set up yet), so we rely on a
	 * timer to poll for input and push data from the console
	 * buffer.
	 */
	init_timer(&sn_sal_timer);
	sn_sal_timer.function = sn_sal_timer_poll;

	if (IS_RUNNING_ON_SIMULATOR())
		sn_interrupt_timeout = 6;
	else {
		/* 960cps / 16 char FIFO = 60HZ 
		 * HZ / (SN_SAL_FIFO_SPEED_CPS / SN_SAL_FIFO_DEPTH) */
		sn_interrupt_timeout = HZ * SN_SAL_UART_FIFO_DEPTH
					/ SN_SAL_UART_FIFO_SPEED_CPS;
	}
	mod_timer(&sn_sal_timer, jiffies + sn_interrupt_timeout);

	sn_sal_is_asynch = 1;
	spin_unlock_irqrestore(&sn_sal_lock, flags);
}

static void __init
sn_sal_switch_to_interrupts(void)
{
	int irq;

	sn_debug_printf("sn_serial: switching to interrupt driven console\n");

	irq = sn_sal_connect_interrupt();
	if (irq) {
		unsigned long flags;
		spin_lock_irqsave(&sn_sal_lock, flags);

		/* sn_sal_irq is a global variable.  When it's set to
		 * a non-zero value, we stop polling for input (since
		 * interrupts should now be enabled). */
		sn_sal_irq = irq;
		sn_func = &intr_ops;

		/* turn on receive interrupts */
		ia64_sn_console_intr_enable(SAL_CONSOLE_INTR_RECV);

		/* the polling timer is already set up, we just change the
		 * frequency.  if we've successfully enabled interrupts (and
		 * CONSOLE_RESTART isn't defined) the next timer expiration
		 * will be the last, otherwise we continue polling */
		if (CONSOLE_RESTART) {
			/* kick the console every once in a while in
			 * case we miss an interrupt */
			sn_interrupt_timeout = 20*HZ;
			sn_sal_timer.function = sn_sal_timer_restart;
			mod_timer(&sn_sal_timer, jiffies + sn_interrupt_timeout);
		}
		spin_unlock_irqrestore(&sn_sal_lock, flags);
	}
}

static int __init
sn_sal_module_init(void)
{
	int retval;

	printk("sn_serial: sn_sal_module_init\n");

	if (!ia64_platform_is("sn2"))
		return -ENODEV;

	/* when this driver is compiled in, the console initialization
	 * will have already switched us into asynchronous operation
	 * before we get here through the module initcalls */
	if (!sn_sal_is_asynch)
		sn_sal_switch_to_asynch();

	/* at this point (module_init) we can try to turn on interrupts */
	if (!IS_RUNNING_ON_SIMULATOR())
	    sn_sal_switch_to_interrupts();

	sn_sal_driver.init_termios = tty_std_termios;
	sn_sal_driver.init_termios.c_cflag = B9600 | CS8 | CREAD | HUPCL | CLOCAL;

	if ((retval = tty_register_driver(&sn_sal_driver))) {
		printk(KERN_ERR "sn_serial: Unable to register tty driver\n");
		return retval;
	}

	tty_register_devfs(&sn_sal_driver, 0, sn_sal_driver.minor_start);

	return 0;
}


static void __exit
sn_sal_module_exit(void)
{
	del_timer_sync(&sn_sal_timer);
	tty_unregister_driver(&sn_sal_driver);
}

module_init(sn_sal_module_init);
module_exit(sn_sal_module_exit);

/*
 * Kernel console definitions
 */

#ifdef CONFIG_SGI_L1_SERIAL_CONSOLE
/*
 * Print a string to the SAL console.  The console_lock must be held
 * when we get here.
 */
static void
sn_sal_console_write(struct console *co, const char *s, unsigned count)
{
	unsigned long flags;

	if (count > CIRC_SPACE_TO_END(xmit.cb_head, xmit.cb_tail,
				      SN_SAL_BUFFER_SIZE))
		sn_debug_printf("\n*** SN_SAL_BUFFER_SIZE too small, lost chars\n");

	/* somebody really wants this output, might be an
	 * oops, kdb, panic, etc.  make sure they get it. */
#if defined(CONFIG_SMP) || defined(CONFIG_PREEMPT)
	if (spin_is_locked(&sn_sal_lock)) {
		synch_flush_xmit();
		sn_func->sal_puts(s, count);
	} else
#endif
	if (in_interrupt()) {
		spin_lock_irqsave(&sn_sal_lock, flags);
		synch_flush_xmit();
		spin_unlock_irqrestore(&sn_sal_lock, flags);
		sn_func->sal_puts(s, count);
	} else {
#if defined(CONFIG_SMP) || defined(CONFIG_PREEMPT)
		sn_sal_write(NULL, 0, s, count);
#else
		synch_flush_xmit();
		sn_func->sal_puts(s, count);
#endif
	}
}

static kdev_t
sn_sal_console_device(struct console *c)
{
	return MKDEV(TTY_MAJOR, 64 + c->index);
}

static int __init
sn_sal_console_setup(struct console *co, char *options)
{
	return 0;
}


static struct console sal_console = {
	.name	= "ttyS",
	.write	= sn_sal_console_write,
	.device	= sn_sal_console_device,
	.setup	= sn_sal_console_setup,
	.index	= -1
};

void __init
sn_sal_serial_console_init(void)
{
	if (ia64_platform_is("sn2")) {
		sn_sal_switch_to_asynch();
		register_console(&sal_console);
	}
}

#endif /* CONFIG_SGI_L1_SERIAL_CONSOLE */
