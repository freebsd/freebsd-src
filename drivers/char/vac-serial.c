/*
 * vacserial.c: VAC UART serial driver
 *              This code stealed and adopted from linux/drivers/char/serial.c
 *              See that for author info
 *
 * Copyright (C) 1998 Gleb Raiko & Vladimir Roganov
 */

#undef  SERIAL_PARANOIA_CHECK
#define CONFIG_SERIAL_NOPAUSE_IO
#define SERIAL_DO_RESTART

#ifndef CONFIG_SERIAL_SHARE_IRQ
#define CONFIG_SERIAL_SHARE_IRQ
#endif

/* Set of debugging defines */

#undef  SERIAL_DEBUG_INTR
#undef  SERIAL_DEBUG_OPEN
#undef  SERIAL_DEBUG_FLOW
#undef  SERIAL_DEBUG_RS_WAIT_UNTIL_SENT

#define RS_STROBE_TIME (10*HZ)
#define RS_ISR_PASS_LIMIT  2 /* Beget is not a super-computer (old=256) */

#define IRQ_T(state) \
 ((state->flags & ASYNC_SHARE_IRQ) ? SA_SHIRQ : SA_INTERRUPT)

#define SERIAL_INLINE

#if defined(MODULE) && defined(SERIAL_DEBUG_MCOUNT)
#define DBG_CNT(s) baget_printk("(%s):[%x] refc=%d, serc=%d, ttyc=%d-> %s\n", \
  kdevname(tty->device),(info->flags),serial_refcount,info->count,tty->count,s)
#else
#define DBG_CNT(s)
#endif

#define  QUAD_UART_SPEED  /* Useful for Baget */

/*
 * End of serial driver configuration section.
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial.h>
#include <linux/major.h>
#include <linux/string.h>
#include <linux/fcntl.h>
#include <linux/ptrace.h>
#include <linux/ioport.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/delay.h>
#ifdef CONFIG_SERIAL_CONSOLE
#include <linux/console.h>
#endif

#include <asm/system.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/bitops.h>
#include <asm/serial.h>
#include <asm/baget/baget.h>

#define BAGET_VAC_UART_IRQ 0x35

/*
 *  Implementation note:
 *  It was descovered by means of advanced electronic tools,
 *  if the driver works via TX_READY interrupts then VIC generates
 *  strange self-eliminating traps. Thus, the driver is rewritten to work
 *  via TX_EMPTY
 */

/* VAC-specific check/debug switches */

#undef CHECK_REG_INDEX
#undef DEBUG_IO_PORT_A

#ifdef SERIAL_INLINE
#define _INLINE_ inline
#endif

static char *serial_name = "VAC Serial driver";
static char *serial_version = "4.26";

static DECLARE_TASK_QUEUE(tq_serial);

static struct tty_driver serial_driver, callout_driver;
static int serial_refcount;

/* number of characters left in xmit buffer before we ask for more */
#define WAKEUP_CHARS 256

/*
 * IRQ_timeout		- How long the timeout should be for each IRQ
 * 				should be after the IRQ has been active.
 */

static struct async_struct *IRQ_ports[NR_IRQS];
static int IRQ_timeout[NR_IRQS];
#ifdef CONFIG_SERIAL_CONSOLE
static struct console sercons;
#endif

static void autoconfig(struct serial_state * info);
static void change_speed(struct async_struct *info);
static void rs_wait_until_sent(struct tty_struct *tty, int timeout);
static void rs_timer(unsigned long dummy);

static struct timer_list vacs_timer;

/*
 * Here we define the default xmit fifo size used for each type of
 * UART
 */
static struct serial_uart_config uart_config[] = {
        { "unknown",  1, 0 },  /* Must go first  --  used as unasigned */
        { "VAC UART", 1, 0 }
};
#define VAC_UART_TYPE 1   /* Just index in above array */

static struct serial_state rs_table[] = {
/*
 * VAC has tricky layout for pair of his SIO registers,
 *  so we need special function to access ones.
 *  To identify port we use their TX offset
 */
        { 0, 9600, VAC_UART_B_TX, BAGET_VAC_UART_IRQ,
          STD_COM_FLAGS }, /* VAC UART B */
        { 0, 9600, VAC_UART_A_TX, BAGET_VAC_UART_IRQ,
          STD_COM_FLAGS }  /* VAC UART A */
};

#define NR_PORTS	(sizeof(rs_table)/sizeof(struct serial_state))

static struct tty_struct *serial_table[NR_PORTS];
static struct termios *serial_termios[NR_PORTS];
static struct termios *serial_termios_locked[NR_PORTS];

#ifndef MIN
#define MIN(a,b)	((a) < (b) ? (a) : (b))
#endif

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
static DECLARE_MUTEX(tmp_buf_sem);

static inline int serial_paranoia_check(struct async_struct *info,
					kdev_t device, const char *routine)
{
#ifdef SERIAL_PARANOIA_CHECK
	static const char *badmagic =
		"Warning: bad magic number for serial struct (%s) in %s\n";
	static const char *badinfo =
		"Warning: null async_struct for (%s) in %s\n";

	if (!info) {
		printk(badinfo, kdevname(device), routine);
		return 1;
	}
	if (info->magic != SERIAL_MAGIC) {
		printk(badmagic, kdevname(device), routine);
		return 1;
	}
#endif
	return 0;
}

/*
  To unify UART A/B access we will use following function
  to compute register offsets by register index.
 */

#define VAC_UART_MODE       0
#define VAC_UART_TX         1
#define VAC_UART_RX         2
#define VAC_UART_INT_MASK   3
#define VAC_UART_INT_STATUS 4

#define VAC_UART_REG_NR     5

static inline int uart_offset_map(unsigned long port, int reg_index)
{
	static const unsigned int ind_to_reg[VAC_UART_REG_NR][NR_PORTS] = {
		{ VAC_UART_B_MODE,       VAC_UART_A_MODE       },
		{ VAC_UART_B_TX,         VAC_UART_A_TX         },
		{ VAC_UART_B_RX,         VAC_UART_A_RX         },
		{ VAC_UART_B_INT_MASK,   VAC_UART_A_INT_MASK   },
		{ VAC_UART_B_INT_STATUS, VAC_UART_A_INT_STATUS }
	};
#ifdef CHECK_REG_INDEX
	if (reg_index > VAC_UART_REG_NR) panic("vacserial: bad reg_index");
#endif
        return ind_to_reg[reg_index][port == VAC_UART_B_TX ? 0 : 1];
}

static inline unsigned int serial_inw(struct async_struct *info, int offset)
{
	int val = vac_inw(uart_offset_map(info->port,offset));
#ifdef DEBUG_IO_PORT_A
	if (info->port == VAC_UART_A_TX)
		printk("UART_A_IN: reg = 0x%04x, val = 0x%04x\n",
		       uart_offset_map(info->port,offset), val);
#endif
	return val;
}

static inline unsigned int serial_inp(struct async_struct *info, int offset)
{
	return serial_inw(info, offset);
}

static inline unsigned int serial_in(struct async_struct *info, int offset)
{
	return serial_inw(info, offset);
}

static inline void serial_outw(struct async_struct *info,int offset, int value)
{
#ifdef DEBUG_IO_PORT_A
	if (info->port == VAC_UART_A_TX)
		printk("UART_A_OUT: offset = 0x%04x, val = 0x%04x\n",
		       uart_offset_map(info->port,offset), value);
#endif
	vac_outw(value, uart_offset_map(info->port,offset));
}

static inline void serial_outp(struct async_struct *info,int offset, int value)
{
	serial_outw(info,offset,value);
}

static inline void serial_out(struct async_struct *info,int offset, int value)
{
	serial_outw(info,offset,value);
}

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
	struct async_struct *info = (struct async_struct *)tty->driver_data;
	unsigned long flags;

	if (serial_paranoia_check(info, tty->device, "rs_stop"))
		return;

	save_flags(flags); cli();
        if (info->IER & VAC_UART_INT_TX_EMPTY) {
                info->IER &= ~VAC_UART_INT_TX_EMPTY;
		serial_out(info, VAC_UART_INT_MASK, info->IER);
	}
	restore_flags(flags);
}

static void rs_start(struct tty_struct *tty)
{
	struct async_struct *info = (struct async_struct *)tty->driver_data;
	unsigned long flags;

	if (serial_paranoia_check(info, tty->device, "rs_start"))
		return;

	save_flags(flags); cli();
	if (info->xmit_cnt && info->xmit_buf
            && !(info->IER & VAC_UART_INT_TX_EMPTY)) {
                info->IER |= VAC_UART_INT_TX_EMPTY;
		serial_out(info, VAC_UART_INT_MASK, info->IER);
	}
	restore_flags(flags);
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

/*
 * This routine is used by the interrupt handler to schedule
 * processing in the software interrupt portion of the driver.
 */
static _INLINE_ void rs_sched_event(struct async_struct *info,
				  int event)
{
	info->event |= 1 << event;
	queue_task(&info->tqueue, &tq_serial);
	mark_bh(SERIAL_BH);
}

static _INLINE_ void receive_chars(struct async_struct *info,
				   int *status)
{
	struct tty_struct *tty = info->tty;
	unsigned short rx;
	unsigned char ch;
	int ignored = 0;
	struct	async_icount *icount;

	icount = &info->state->icount;
	do {
		rx = serial_inw(info, VAC_UART_RX);
		ch = VAC_UART_RX_DATA_MASK & rx;

		if (tty->flip.count >= TTY_FLIPBUF_SIZE)
			break;
		*tty->flip.char_buf_ptr = ch;
		icount->rx++;

#ifdef SERIAL_DEBUG_INTR
		baget_printk("DR%02x:%02x...", rx, *status);
#endif
		*tty->flip.flag_buf_ptr = 0;
		if (*status & (VAC_UART_STATUS_RX_BREAK_CHANGE
			       | VAC_UART_STATUS_RX_ERR_PARITY
			       | VAC_UART_STATUS_RX_ERR_FRAME
			       | VAC_UART_STATUS_RX_ERR_OVERRUN)) {
			/*
			 * For statistics only
			 */
			if (*status & VAC_UART_STATUS_RX_BREAK_CHANGE) {
				*status &= ~(VAC_UART_STATUS_RX_ERR_FRAME
					     | VAC_UART_STATUS_RX_ERR_PARITY);
				icount->brk++;
			} else if (*status & VAC_UART_STATUS_RX_ERR_PARITY)
				icount->parity++;
			else if (*status & VAC_UART_STATUS_RX_ERR_FRAME)
				icount->frame++;
			if (*status & VAC_UART_STATUS_RX_ERR_OVERRUN)
				icount->overrun++;

			/*
			 * Now check to see if character should be
			 * ignored, and mask off conditions which
			 * should be ignored.
			 */
			if (*status & info->ignore_status_mask) {
				if (++ignored > 100)
					break;
				goto ignore_char;
			}
			*status &= info->read_status_mask;

			if (*status & (VAC_UART_STATUS_RX_BREAK_CHANGE)) {
#ifdef SERIAL_DEBUG_INTR
				baget_printk("handling break....");
#endif
				*tty->flip.flag_buf_ptr = TTY_BREAK;
				if (info->flags & ASYNC_SAK)
					do_SAK(tty);
			} else if (*status & VAC_UART_STATUS_RX_ERR_PARITY)
				*tty->flip.flag_buf_ptr = TTY_PARITY;
			else if (*status & VAC_UART_STATUS_RX_ERR_FRAME)
				*tty->flip.flag_buf_ptr = TTY_FRAME;
			if (*status & VAC_UART_STATUS_RX_ERR_OVERRUN) {
				/*
				 * Overrun is special, since it's
				 * reported immediately, and doesn't
				 * affect the current character
				 */
				if (tty->flip.count < TTY_FLIPBUF_SIZE) {
					tty->flip.count++;
					tty->flip.flag_buf_ptr++;
					tty->flip.char_buf_ptr++;
					*tty->flip.flag_buf_ptr = TTY_OVERRUN;
				}
			}
		}
		tty->flip.flag_buf_ptr++;
		tty->flip.char_buf_ptr++;
		tty->flip.count++;
	ignore_char:
		*status = serial_inw(info, VAC_UART_INT_STATUS);
	} while ((*status & VAC_UART_STATUS_RX_READY));
	tty_flip_buffer_push(tty);
}

static _INLINE_ void transmit_chars(struct async_struct *info, int *intr_done)
{
	int count;

	if (info->x_char) {
		serial_outw(info, VAC_UART_TX,
			    (((unsigned short)info->x_char)<<8));
		info->state->icount.tx++;
		info->x_char = 0;
		if (intr_done)
			*intr_done = 0;
		return;
	}
	if ((info->xmit_cnt <= 0) || info->tty->stopped ||
	    info->tty->hw_stopped) {
                info->IER &= ~VAC_UART_INT_TX_EMPTY;
		serial_outw(info, VAC_UART_INT_MASK, info->IER);
		return;
	}
	count = info->xmit_fifo_size;
	do {
		serial_out(info, VAC_UART_TX,
			   (unsigned short)info->xmit_buf[info->xmit_tail++] \
			   << 8);
		info->xmit_tail = info->xmit_tail & (SERIAL_XMIT_SIZE-1);
		info->state->icount.tx++;
		if (--info->xmit_cnt <= 0)
			break;
	} while (--count > 0);

	if (info->xmit_cnt < WAKEUP_CHARS)
		rs_sched_event(info, RS_EVENT_WRITE_WAKEUP);

#ifdef SERIAL_DEBUG_INTR
	baget_printk("THRE...");
#endif
	if (intr_done)
		*intr_done = 0;

	if (info->xmit_cnt <= 0) {
                info->IER &= ~VAC_UART_INT_TX_EMPTY;
		serial_outw(info, VAC_UART_INT_MASK, info->IER);
	}
}

static _INLINE_ void check_modem_status(struct async_struct *info)
{
#if 0 /* VAC hasn't modem control */
	wake_up_interruptible(&info->open_wait);
	rs_sched_event(info, RS_EVENT_WRITE_WAKEUP);
#endif
}

#ifdef CONFIG_SERIAL_SHARE_IRQ


/*
 *  Specific functions needed for VAC UART interrupt enter/leave
 */

#define VAC_INT_CTRL_UART_ENABLE  \
   (VAC_INT_CTRL_TIMER_PIO10|VAC_INT_CTRL_UART_B_PIO7|VAC_INT_CTRL_UART_A_PIO7)

#define VAC_INT_CTRL_UART_DISABLE(info) \
   (VAC_INT_CTRL_TIMER_PIO10 | \
    ((info->port == VAC_UART_A_TX) ? \
     (VAC_INT_CTRL_UART_A_DISABLE|VAC_INT_CTRL_UART_B_PIO7) : \
     (VAC_INT_CTRL_UART_A_PIO7|VAC_INT_CTRL_UART_B_DISABLE)))

/*
 *  Following two functions were proposed by Pavel Osipenko
 *  to make VAC/VIC behaviour more regular.
 */
static void intr_begin(struct async_struct* info)
{
	serial_outw(info, VAC_UART_INT_MASK, 0);
}

static void intr_end(struct async_struct* info)
{
	vac_outw(VAC_INT_CTRL_UART_DISABLE(info), VAC_INT_CTRL);
	vac_outw(VAC_INT_CTRL_UART_ENABLE,        VAC_INT_CTRL);

	serial_outw(info, VAC_UART_INT_MASK, info->IER);
}

/*
 * This is the serial driver's generic interrupt routine
 */
static void rs_interrupt(int irq, void *dev_id, struct pt_regs * regs)
{
	int status;
	struct async_struct * info;
	int pass_counter = 0;
	struct async_struct *end_mark = 0;

#ifdef SERIAL_DEBUG_INTR
	baget_printk("rs_interrupt(%d)...", irq);
#endif

	info = IRQ_ports[irq];
	if (!info)
		return;

	do {
	        intr_begin(info);  /* Mark we begin port handling */

		if (!info->tty ||
		     (serial_inw (info, VAC_UART_INT_STATUS)
		      & VAC_UART_STATUS_INTS) == 0)
		    {
			if (!end_mark)
				end_mark = info;
			goto next;
		    }
		end_mark = 0;

		info->last_active = jiffies;

		status = serial_inw(info, VAC_UART_INT_STATUS);
#ifdef SERIAL_DEBUG_INTR
		baget_printk("status = %x...", status);
#endif
		if (status & VAC_UART_STATUS_RX_READY) {
			receive_chars(info, &status);
		}
		check_modem_status(info);
                if (status & VAC_UART_STATUS_TX_EMPTY)
			transmit_chars(info, 0);

	next:
		intr_end(info);   /* Mark this port handled */

		info = info->next_port;
		if (!info) {
			info = IRQ_ports[irq];
			if (pass_counter++ > RS_ISR_PASS_LIMIT) {
				break; 	/* Prevent infinite loops */
			}
			continue;
		}
	} while (end_mark != info);
#ifdef SERIAL_DEBUG_INTR
	baget_printk("end.\n");
#endif


}
#endif /* #ifdef CONFIG_SERIAL_SHARE_IRQ */


/* The original driver was simplified here:
   two functions were joined to reduce code */

#define rs_interrupt_single rs_interrupt


/*
 * -------------------------------------------------------------------
 * Here ends the serial interrupt routines.
 * -------------------------------------------------------------------
 */

/*
 * This routine is used to handle the "bottom half" processing for the
 * serial driver, known also the "software interrupt" processing.
 * This processing is done at the kernel interrupt level, after the
 * rs_interrupt() has returned, BUT WITH INTERRUPTS TURNED ON.  This
 * is where time-consuming activities which can not be done in the
 * interrupt driver proper are done; the interrupt driver schedules
 * them using rs_sched_event(), and they get done here.
 */
static void do_serial_bh(void)
{
	run_task_queue(&tq_serial);
}

static void do_softint(void *private_)
{
	struct async_struct	*info = (struct async_struct *) private_;
	struct tty_struct	*tty;

	tty = info->tty;
	if (!tty)
		return;

	if (test_and_clear_bit(RS_EVENT_WRITE_WAKEUP, &info->event)) {
		if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) &&
		    tty->ldisc.write_wakeup)
			(tty->ldisc.write_wakeup)(tty);
		wake_up_interruptible(&tty->write_wait);
	}
}

/*
 * ---------------------------------------------------------------
 * Low level utility subroutines for the serial driver:  routines to
 * figure out the appropriate timeout for an interrupt chain, routines
 * to initialize and startup a serial port, and routines to shutdown a
 * serial port.  Useful stuff like that.
 * ---------------------------------------------------------------
 */

/*
 * This routine figures out the correct timeout for a particular IRQ.
 * It uses the smallest timeout of all of the serial ports in a
 * particular interrupt chain.  Now only used for IRQ 0....
 */
static void figure_IRQ_timeout(int irq)
{
	struct	async_struct	*info;
	int	timeout = 60*HZ;	/* 60 seconds === a long time :-) */

	info = IRQ_ports[irq];
	if (!info) {
		IRQ_timeout[irq] = 60*HZ;
		return;
	}
	while (info) {
		if (info->timeout < timeout)
			timeout = info->timeout;
		info = info->next_port;
	}
	if (!irq)
		timeout = timeout / 2;
	IRQ_timeout[irq] = timeout ? timeout : 1;
}

static int startup(struct async_struct * info)
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
		if (info->tty)
			set_bit(TTY_IO_ERROR, &info->tty->flags);
		free_page(page);
		goto errout;
	}
	if (info->xmit_buf)
		free_page(page);
	else
		info->xmit_buf = (unsigned char *) page;

#ifdef SERIAL_DEBUG_OPEN
	baget_printk("starting up ttys%d (irq %d)...", info->line, state->irq);
#endif

	if (uart_config[info->state->type].flags & UART_STARTECH) {
		/* Wake up UART */
		serial_outp(info, VAC_UART_MODE, 0);
		serial_outp(info, VAC_UART_INT_MASK, 0);
	}

	/*
	 * Allocate the IRQ if necessary
	 */
	if (state->irq && (!IRQ_ports[state->irq] ||
			  !IRQ_ports[state->irq]->next_port)) {

		if (IRQ_ports[state->irq]) {
#ifdef CONFIG_SERIAL_SHARE_IRQ
			free_irq(state->irq, NULL);
				handler = rs_interrupt;
#else
			retval = -EBUSY;
			goto errout;
#endif /* CONFIG_SERIAL_SHARE_IRQ */
		} else
			handler = rs_interrupt_single;


		retval = request_irq(state->irq, handler, IRQ_T(state),
				     "serial", NULL);
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
	figure_IRQ_timeout(state->irq);

	/*
	 * Clear the interrupt registers.
	 */
     /* (void) serial_inw(info, VAC_UART_INT_STATUS); */   /* (see above) */
	(void) serial_inw(info, VAC_UART_RX);

	/*
	 * Now, initialize the UART
	 */
	serial_outp(info, VAC_UART_MODE, VAC_UART_MODE_INITIAL); /*reset DLAB*/

	/*
	 * Finally, enable interrupts
	 */
	info->IER = VAC_UART_INT_RX_BREAK_CHANGE | VAC_UART_INT_RX_ERRS | \
                        VAC_UART_INT_RX_READY;
	serial_outp(info, VAC_UART_INT_MASK, info->IER); /*enable interrupts*/

	/*
	 * And clear the interrupt registers again for luck.
	 */
	(void)serial_inp(info, VAC_UART_INT_STATUS);
	(void)serial_inp(info, VAC_UART_RX);

	if (info->tty)
		clear_bit(TTY_IO_ERROR, &info->tty->flags);
	info->xmit_cnt = info->xmit_head = info->xmit_tail = 0;

	/*
	 * Set up serial timers...
	 */
	mod_timer(&vacs_timer, jiffies + 2*HZ/100);

	/*
	 * and set the speed of the serial port
	 */
	change_speed(info);

	info->flags |= ASYNC_INITIALIZED;
	restore_flags(flags);
	return 0;

errout:
	restore_flags(flags);
	return retval;
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

	if (!(info->flags & ASYNC_INITIALIZED))
		return;

	state = info->state;

#ifdef SERIAL_DEBUG_OPEN
	baget_printk("Shutting down serial port %d (irq %d)....", info->line,
	       state->irq);
#endif

	save_flags(flags); cli(); /* Disable interrupts */

	/*
	 * clear delta_msr_wait queue to avoid mem leaks: we may free the irq
	 * here so the queue might never be waken up
	 */
	wake_up_interruptible(&info->delta_msr_wait);

	/*
	 * First unlink the serial port from the IRQ chain...
	 */
	if (info->next_port)
		info->next_port->prev_port = info->prev_port;
	if (info->prev_port)
		info->prev_port->next_port = info->next_port;
	else
		IRQ_ports[state->irq] = info->next_port;
	figure_IRQ_timeout(state->irq);

	/*
	 * Free the IRQ, if necessary
	 */
	if (state->irq && (!IRQ_ports[state->irq] ||
			  !IRQ_ports[state->irq]->next_port)) {
		if (IRQ_ports[state->irq]) {
			free_irq(state->irq, NULL);
			retval = request_irq(state->irq, rs_interrupt_single,
					     IRQ_T(state), "serial", NULL);

			if (retval)
				printk("serial shutdown: request_irq: error %d"
				       "  Couldn't reacquire IRQ.\n", retval);
		} else
			free_irq(state->irq, NULL);
	}

	if (info->xmit_buf) {
		free_page((unsigned long) info->xmit_buf);
		info->xmit_buf = 0;
	}

	info->IER = 0;
	serial_outp(info, VAC_UART_INT_MASK, 0x00);	/* disable all intrs */

	/* disable break condition */
	serial_out(info, VAC_UART_MODE, serial_inp(info, VAC_UART_MODE) & \
		   ~VAC_UART_MODE_SEND_BREAK);

	if (info->tty)
		set_bit(TTY_IO_ERROR, &info->tty->flags);

	info->flags &= ~ASYNC_INITIALIZED;
	restore_flags(flags);
}

/*
 *  When we set line mode, we call this function
 *  for Baget-specific adjustments.
 */

static inline unsigned short vac_uart_mode_fixup (unsigned short cval)
{
#ifdef QUAD_UART_SPEED
	/*
	 *  When we are using 4-x advantage in speed:
	 *
	 *  Disadvantage : can't support 75, 150 bauds
	 *  Advantage    : can support 19200, 38400 bauds
	 */
	char speed = 7 & (cval >> 10);
	cval &= ~(7 << 10);
	cval |= VAC_UART_MODE_BAUD(speed-2);
#endif

	/*
	 *  In general, we have Tx and Rx ON all time
	 *  and use int mask flag for their disabling.
	 */
	cval |= VAC_UART_MODE_RX_ENABLE;
	cval |= VAC_UART_MODE_TX_ENABLE;
	cval |= VAC_UART_MODE_CHAR_RX_ENABLE;
	cval |= VAC_UART_MODE_CHAR_TX_ENABLE;

        /* Low 4 bits are not used in UART */
	cval &= ~0xf;

	return cval;
}

/*
 * This routine is called to set the UART divisor registers to match
 * the specified baud rate for a serial port.
 */
static void change_speed(struct async_struct *info)
{
	unsigned short port;
	int	quot = 0, baud_base, baud;
	unsigned cflag, cval;
	int	bits;
	unsigned long	flags;

	if (!info->tty || !info->tty->termios)
		return;
	cflag = info->tty->termios->c_cflag;
	if (!(port = info->port))
		return;

	/* byte size and parity */
	switch (cflag & CSIZE) {
	      case CS7: cval = 0x0;  bits = 9; break;
	      case CS8: cval = VAC_UART_MODE_8BIT_CHAR; bits = 10; break;
	      /* Never happens, but GCC is too dumb to figure it out */
	      case CS5:
	      case CS6:
	      default:  cval = 0x0; bits = 9; break;
	      }
	cval &= ~VAC_UART_MODE_PARITY_ENABLE;
	if (cflag & PARENB) {
		cval |= VAC_UART_MODE_PARITY_ENABLE;
		bits++;
	}
	if (cflag & PARODD)
		cval |= VAC_UART_MODE_PARITY_ODD;

	/* Determine divisor based on baud rate */
	baud = tty_get_baud_rate(info->tty);
	if (!baud)
		baud = 9600;    /* B0 transition handled in rs_set_termios */
	baud_base = info->state->baud_base;
	if (baud == 38400 &&
	    ((info->flags & ASYNC_SPD_MASK) == ASYNC_SPD_CUST))
		quot = info->state->custom_divisor;
	else {
		if (baud == 134)
			/* Special case since 134 is really 134.5 */
			quot = (2*baud_base / 269);
		else if (baud)
			quot = baud_base / baud;
	}
	/* If the quotient is ever zero, default to 9600 bps */
	if (!quot)
		quot = baud_base / 9600;
	info->quot = quot;
	info->timeout = ((info->xmit_fifo_size*HZ*bits*quot) / baud_base);
	info->timeout += HZ/50;		/* Add .02 seconds of slop */

	serial_out(info, VAC_UART_INT_MASK, info->IER);

	/*
	 * Set up parity check flag
	 */
#define RELEVANT_IFLAG(iflag) (iflag & (IGNBRK|BRKINT|IGNPAR|PARMRK|INPCK))

	info->read_status_mask = VAC_UART_STATUS_RX_ERR_OVERRUN | \
                VAC_UART_STATUS_TX_EMPTY | VAC_UART_STATUS_RX_READY;
	if (I_INPCK(info->tty))
		info->read_status_mask |= VAC_UART_STATUS_RX_ERR_FRAME | \
			VAC_UART_STATUS_RX_ERR_PARITY;
	if (I_BRKINT(info->tty) || I_PARMRK(info->tty))
		info->read_status_mask |= VAC_UART_STATUS_RX_BREAK_CHANGE;

	/*
	 * Characters to ignore
	 */
	info->ignore_status_mask = 0;
	if (I_IGNPAR(info->tty))
		info->ignore_status_mask |= VAC_UART_STATUS_RX_ERR_PARITY | \
			VAC_UART_STATUS_RX_ERR_FRAME;
	if (I_IGNBRK(info->tty)) {
		info->ignore_status_mask |= VAC_UART_STATUS_RX_BREAK_CHANGE;
		/*
		 * If we're ignore parity and break indicators, ignore
		 * overruns too.  (For real raw support).
		 */
		if (I_IGNPAR(info->tty))
			info->ignore_status_mask |= \
				VAC_UART_STATUS_RX_ERR_OVERRUN;
	}
	/*
	 * !!! ignore all characters if CREAD is not set
	 */
	if ((cflag & CREAD) == 0)
		info->ignore_status_mask |= VAC_UART_STATUS_RX_READY;
	save_flags(flags); cli();


	switch (baud) {
	default:
	case 9600:
		cval |= VAC_UART_MODE_BAUD(7);
		break;
	case 4800:
		cval |= VAC_UART_MODE_BAUD(6);
		break;
	case 2400:
		cval |= VAC_UART_MODE_BAUD(5);
		break;
	case 1200:
		cval |= VAC_UART_MODE_BAUD(4);
		break;
	case 600:
		cval |= VAC_UART_MODE_BAUD(3);
		break;
	case 300:
		cval |= VAC_UART_MODE_BAUD(2);
		break;
#ifndef QUAD_UART_SPEED
	case 150:
#else
	case 38400:
#endif
		cval |= VAC_UART_MODE_BAUD(1);
		break;
#ifndef QUAD_UART_SPEED
	case 75:
#else
	case 19200:
#endif
		cval |= VAC_UART_MODE_BAUD(0);
		break;
	}

	/* Baget VAC need some adjustments for computed value */
	cval = vac_uart_mode_fixup(cval);

	serial_outp(info, VAC_UART_MODE, cval);
	restore_flags(flags);
}

static void rs_put_char(struct tty_struct *tty, unsigned char ch)
{
	struct async_struct *info = (struct async_struct *)tty->driver_data;
	unsigned long flags;

	if (serial_paranoia_check(info, tty->device, "rs_put_char"))
		return;

	if (!tty || !info->xmit_buf)
		return;

	save_flags(flags); cli();
	if (info->xmit_cnt >= SERIAL_XMIT_SIZE - 1) {
		restore_flags(flags);
		return;
	}

	info->xmit_buf[info->xmit_head++] = ch;
	info->xmit_head &= SERIAL_XMIT_SIZE-1;
	info->xmit_cnt++;
	restore_flags(flags);
}

static void rs_flush_chars(struct tty_struct *tty)
{
	struct async_struct *info = (struct async_struct *)tty->driver_data;
	unsigned long flags;

	if (serial_paranoia_check(info, tty->device, "rs_flush_chars"))
		return;

	if (info->xmit_cnt <= 0 || tty->stopped || tty->hw_stopped ||
	    !info->xmit_buf)
		return;

	save_flags(flags); cli();
        info->IER |= VAC_UART_INT_TX_EMPTY;
	serial_out(info, VAC_UART_INT_MASK, info->IER);
	restore_flags(flags);
}

static int rs_write(struct tty_struct * tty, int from_user,
		    const unsigned char *buf, int count)
{
	int	c, ret = 0;
	struct async_struct *info = (struct async_struct *)tty->driver_data;
	unsigned long flags;

	if (serial_paranoia_check(info, tty->device, "rs_write"))
		return 0;

	if (!tty || !info->xmit_buf || !tmp_buf)
		return 0;

	save_flags(flags);
	if (from_user) {
		down(&tmp_buf_sem);
		while (1) {
			c = MIN(count,
				MIN(SERIAL_XMIT_SIZE - info->xmit_cnt - 1,
				    SERIAL_XMIT_SIZE - info->xmit_head));
			if (c <= 0)
				break;

			c -= copy_from_user(tmp_buf, buf, c);
			if (!c) {
				if (!ret)
					ret = -EFAULT;
				break;
			}
			cli();
			c = MIN(c, MIN(SERIAL_XMIT_SIZE - info->xmit_cnt - 1,
				       SERIAL_XMIT_SIZE - info->xmit_head));
			memcpy(info->xmit_buf + info->xmit_head, tmp_buf, c);
			info->xmit_head = ((info->xmit_head + c) &
					   (SERIAL_XMIT_SIZE-1));
			info->xmit_cnt += c;
			restore_flags(flags);
			buf += c;
			count -= c;
			ret += c;
		}
		up(&tmp_buf_sem);
	} else {
		while (1) {
			cli();
			c = MIN(count,
				MIN(SERIAL_XMIT_SIZE - info->xmit_cnt - 1,
				    SERIAL_XMIT_SIZE - info->xmit_head));
			if (c <= 0) {
				restore_flags(flags);
				break;
			}
			memcpy(info->xmit_buf + info->xmit_head, buf, c);
			info->xmit_head = ((info->xmit_head + c) &
					   (SERIAL_XMIT_SIZE-1));
			info->xmit_cnt += c;
			restore_flags(flags);
			buf += c;
			count -= c;
			ret += c;
		}
	}
	if (info->xmit_cnt && !tty->stopped && !tty->hw_stopped &&
            !(info->IER & VAC_UART_INT_TX_EMPTY)) {
                info->IER |= VAC_UART_INT_TX_EMPTY;
		serial_out(info, VAC_UART_INT_MASK, info->IER);
	}
	return ret;
}

static int rs_write_room(struct tty_struct *tty)
{
	struct async_struct *info = (struct async_struct *)tty->driver_data;
	int	ret;

	if (serial_paranoia_check(info, tty->device, "rs_write_room"))
		return 0;
	ret = SERIAL_XMIT_SIZE - info->xmit_cnt - 1;
	if (ret < 0)
		ret = 0;
	return ret;
}

static int rs_chars_in_buffer(struct tty_struct *tty)
{
	struct async_struct *info = (struct async_struct *)tty->driver_data;

	if (serial_paranoia_check(info, tty->device, "rs_chars_in_buffer"))
		return 0;
	return info->xmit_cnt;
}

static void rs_flush_buffer(struct tty_struct *tty)
{
	struct async_struct *info = (struct async_struct *)tty->driver_data;
	unsigned long flags;

	if (serial_paranoia_check(info, tty->device, "rs_flush_buffer"))
		return;

	save_flags(flags); cli();
	info->xmit_cnt = info->xmit_head = info->xmit_tail = 0;
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

	if (serial_paranoia_check(info, tty->device, "rs_send_char"))
		return;

	info->x_char = ch;
	if (ch) {
		/* Make sure transmit interrupts are on */
                info->IER |= VAC_UART_INT_TX_EMPTY;
		serial_out(info, VAC_UART_INT_MASK, info->IER);
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
	struct async_struct *info = (struct async_struct *)tty->driver_data;

#ifdef SERIAL_DEBUG_THROTTLE
	char	buf[64];

	baget_printk("throttle %s: %d....\n", tty_name(tty, buf),
	       tty->ldisc.chars_in_buffer(tty));
#endif

	if (serial_paranoia_check(info, tty->device, "rs_throttle"))
		return;

	if (I_IXOFF(tty))
		rs_send_xchar(tty, STOP_CHAR(tty));
}

static void rs_unthrottle(struct tty_struct * tty)
{
	struct async_struct *info = (struct async_struct *)tty->driver_data;
#ifdef SERIAL_DEBUG_THROTTLE
	char	buf[64];

	baget_printk("unthrottle %s: %d....\n", tty_name(tty, buf),
	       tty->ldisc.chars_in_buffer(tty));
#endif

	if (serial_paranoia_check(info, tty->device, "rs_unthrottle"))
		return;

	if (I_IXOFF(tty)) {
		if (info->x_char)
			info->x_char = 0;
		else
			rs_send_xchar(tty, START_CHAR(tty));
	}
}

/*
 * ------------------------------------------------------------
 * rs_ioctl() and friends
 * ------------------------------------------------------------
 */

static int get_serial_info(struct async_struct * info,
			   struct serial_struct * retinfo)
{
	struct serial_struct tmp;
	struct serial_state *state = info->state;

	if (!retinfo)
		return -EFAULT;
	memset(&tmp, 0, sizeof(tmp));
	tmp.type = state->type;
	tmp.line = state->line;
	tmp.port = state->port;
	tmp.irq = state->irq;
	tmp.flags = state->flags;
	tmp.xmit_fifo_size = state->xmit_fifo_size;
	tmp.baud_base = state->baud_base;
	tmp.close_delay = state->close_delay;
	tmp.closing_wait = state->closing_wait;
	tmp.custom_divisor = state->custom_divisor;
	tmp.hub6 = state->hub6;
	if (copy_to_user(retinfo,&tmp,sizeof(*retinfo)))
		return -EFAULT;
	return 0;
}

static int set_serial_info(struct async_struct * info,
			   struct serial_struct * new_info)
{
	struct serial_struct new_serial;
 	struct serial_state old_state, *state;
	unsigned int		i,change_irq,change_port;
	int 			retval = 0;

	if (copy_from_user(&new_serial,new_info,sizeof(new_serial)))
		return -EFAULT;
	state = info->state;
	old_state = *state;

	change_irq = new_serial.irq != state->irq;
	change_port = (new_serial.port != state->port) ||
		(new_serial.hub6 != state->hub6);

	if (!capable(CAP_SYS_ADMIN)) {
		if (change_irq || change_port ||
		    (new_serial.baud_base != state->baud_base) ||
		    (new_serial.type != state->type) ||
		    (new_serial.close_delay != state->close_delay) ||
		    (new_serial.xmit_fifo_size != state->xmit_fifo_size) ||
		    ((new_serial.flags & ~ASYNC_USR_MASK) !=
		     (state->flags & ~ASYNC_USR_MASK)))
			return -EPERM;
		state->flags = ((state->flags & ~ASYNC_USR_MASK) |
			       (new_serial.flags & ASYNC_USR_MASK));
		info->flags = ((state->flags & ~ASYNC_USR_MASK) |
			       (info->flags & ASYNC_USR_MASK));
		state->custom_divisor = new_serial.custom_divisor;
		goto check_and_exit;
	}

	new_serial.irq = new_serial.irq;

	if ((new_serial.irq >= NR_IRQS) || (new_serial.port > 0xffff) ||
	    (new_serial.baud_base == 0) || (new_serial.type < PORT_UNKNOWN) ||
	    (new_serial.type > PORT_MAX) || (new_serial.type == PORT_CIRRUS) ||
	    (new_serial.type == PORT_STARTECH)) {
		return -EINVAL;
	}

	if ((new_serial.type != state->type) ||
	    (new_serial.xmit_fifo_size <= 0))
		new_serial.xmit_fifo_size =
			uart_config[state->type].dfl_xmit_fifo_size;

	/* Make sure address is not already in use */
	if (new_serial.type) {
		for (i = 0 ; i < NR_PORTS; i++)
			if ((state != &rs_table[i]) &&
			    (rs_table[i].port == new_serial.port) &&
			    rs_table[i].type)
				return -EADDRINUSE;
	}

	if ((change_port || change_irq) && (state->count > 1))
		return -EBUSY;

	/*
	 * OK, past this point, all the error checking has been done.
	 * At this point, we start making changes.....
	 */

	state->baud_base = new_serial.baud_base;
	state->flags = ((state->flags & ~ASYNC_FLAGS) |
			(new_serial.flags & ASYNC_FLAGS));
	info->flags = ((state->flags & ~ASYNC_INTERNAL_FLAGS) |
		       (info->flags & ASYNC_INTERNAL_FLAGS));
	state->custom_divisor = new_serial.custom_divisor;
	state->type = new_serial.type;
	state->close_delay = new_serial.close_delay * HZ/100;
	state->closing_wait = new_serial.closing_wait * HZ/100;
	info->tty->low_latency = (info->flags & ASYNC_LOW_LATENCY) ? 1 : 0;
	info->xmit_fifo_size = state->xmit_fifo_size =
		new_serial.xmit_fifo_size;

	release_region(state->port,8);
	if (change_port || change_irq) {
		/*
		 * We need to shutdown the serial port at the old
		 * port/irq combination.
		 */
		shutdown(info);
		state->irq = new_serial.irq;
		info->port = state->port = new_serial.port;
		info->hub6 = state->hub6 = new_serial.hub6;
	}
	if (state->type != PORT_UNKNOWN)
		request_region(state->port,8,"serial(set)");


check_and_exit:
	if (!state->port || !state->type)
		return 0;
	if (info->flags & ASYNC_INITIALIZED) {
		if (((old_state.flags & ASYNC_SPD_MASK) !=
		     (state->flags & ASYNC_SPD_MASK)) ||
		    (old_state.custom_divisor != state->custom_divisor)) {
			if ((state->flags & ASYNC_SPD_MASK) == ASYNC_SPD_HI)
				info->tty->alt_speed = 57600;
			if ((state->flags & ASYNC_SPD_MASK) == ASYNC_SPD_VHI)
				info->tty->alt_speed = 115200;
			if ((state->flags & ASYNC_SPD_MASK) == ASYNC_SPD_SHI)
				info->tty->alt_speed = 230400;
			if ((state->flags & ASYNC_SPD_MASK) == ASYNC_SPD_WARP)
				info->tty->alt_speed = 460800;
			change_speed(info);
		}
	} else
		retval = startup(info);
	return retval;
}


/*
 * get_lsr_info - get line status register info
 *
 * Purpose: Let user call ioctl() to get info when the UART physically
 * 	    is emptied.  On bus types like RS485, the transmitter must
 * 	    release the bus after transmitting. This must be done when
 * 	    the transmit shift register is empty, not be done when the
 * 	    transmit holding register is empty.  This functionality
 * 	    allows an RS485 driver to be written in user space.
 */
static int get_lsr_info(struct async_struct * info, unsigned int *value)
{
	unsigned short status;
	unsigned int result;
	unsigned long flags;

	save_flags(flags); cli();
	status = serial_inw(info, VAC_UART_INT_STATUS);
	restore_flags(flags);
	result = ((status & VAC_UART_STATUS_TX_EMPTY) ? TIOCSER_TEMT : 0);
	return put_user(result,value);
}


static int get_modem_info(struct async_struct * info, unsigned int *value)
{
	unsigned int result;

	result = TIOCM_CAR | TIOCM_DSR;
	return put_user(result,value);
}

static int set_modem_info(struct async_struct * info, unsigned int cmd,
			  unsigned int *value)
{
	unsigned int arg;

	if (get_user(arg, value))
		return -EFAULT;
	switch (cmd) {
	default:
		return -EINVAL;
	}
	return 0;
}

static int do_autoconfig(struct async_struct * info)
{
	int			retval;

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	if (info->state->count > 1)
		return -EBUSY;

	shutdown(info);

	autoconfig(info->state);

	retval = startup(info);
	if (retval)
		return retval;
	return 0;
}

/*
 * rs_break() --- routine which turns the break handling on or off
 */
static void rs_break(struct tty_struct *tty, int break_state)
{
	struct async_struct * info = (struct async_struct *)tty->driver_data;
	unsigned long flags;

	if (serial_paranoia_check(info, tty->device, "rs_break"))
		return;

	if (!info->port)
		return;
	save_flags(flags); cli();
	if (break_state == -1)
		serial_outp(info, VAC_UART_MODE,
			   serial_inp(info, VAC_UART_MODE) | \
			    VAC_UART_MODE_SEND_BREAK);
	else
		serial_outp(info, VAC_UART_MODE,
			   serial_inp(info, VAC_UART_MODE) & \
			    ~VAC_UART_MODE_SEND_BREAK);
	restore_flags(flags);
}

static int rs_ioctl(struct tty_struct *tty, struct file * file,
		    unsigned int cmd, unsigned long arg)
{
	int error;
	struct async_struct * info = (struct async_struct *)tty->driver_data;
	struct async_icount cprev, cnow;	/* kernel counter temps */
	struct serial_icounter_struct *p_cuser;	/* user space */
	unsigned long flags;

	if (serial_paranoia_check(info, tty->device, "rs_ioctl"))
		return -ENODEV;

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
			return set_modem_info(info, cmd, (unsigned int *) arg);
		case TIOCGSERIAL:
			return get_serial_info(info,
					       (struct serial_struct *) arg);
		case TIOCSSERIAL:
			return set_serial_info(info,
					       (struct serial_struct *) arg);
		case TIOCSERCONFIG:
			return do_autoconfig(info);

		case TIOCSERGETLSR: /* Get line status register */
			return get_lsr_info(info, (unsigned int *) arg);

		case TIOCSERGSTRUCT:
			if (copy_to_user((struct async_struct *) arg,
					 info, sizeof(struct async_struct)))
				return -EFAULT;
			return 0;

		/*
		 * Wait for any of the 4 modem inputs (DCD,RI,DSR,CTS)to change
		 * - mask passed in arg for lines of interest
 		 *   (use |'ed TIOCM_RNG/DSR/CD/CTS for masking)
		 * Caller should use TIOCGICOUNT to see which one it was
		 */
		case TIOCMIWAIT:
			save_flags(flags); cli();
			/* note the counters on entry */
			cprev = info->state->icount;
			restore_flags(flags);
			while (1) {
				interruptible_sleep_on(&info->delta_msr_wait);
				/* see if a signal did it */
				if (signal_pending(current))
					return -ERESTARTSYS;
				save_flags(flags); cli();
				cnow = info->state->icount; /* atomic copy */
				restore_flags(flags);
				if (cnow.rng == cprev.rng &&
				    cnow.dsr == cprev.dsr &&
				    cnow.dcd == cprev.dcd &&
				    cnow.cts == cprev.cts)
					return -EIO; /* no change => error */
				if ( ((arg & TIOCM_RNG) &&
				      (cnow.rng != cprev.rng)) ||
				     ((arg & TIOCM_DSR) &&
				      (cnow.dsr != cprev.dsr)) ||
				     ((arg & TIOCM_CD)  &&
				      (cnow.dcd != cprev.dcd)) ||
				     ((arg & TIOCM_CTS) &&
				      (cnow.cts != cprev.cts)) ) {
					return 0;
				}
				cprev = cnow;
			}
			/* NOTREACHED */

		/*
		 * Get counter of input serial line interrupts (DCD,RI,DSR,CTS)
		 * Return: write counters to the user passed counter struct
		 * NB: both 1->0 and 0->1 transitions are counted except for
		 *     RI where only 0->1 is counted.
		 */
		case TIOCGICOUNT:
			save_flags(flags); cli();
			cnow = info->state->icount;
			restore_flags(flags);
			p_cuser = (struct serial_icounter_struct *) arg;
			error = put_user(cnow.cts, &p_cuser->cts);
			if (error) return error;
			error = put_user(cnow.dsr, &p_cuser->dsr);
			if (error) return error;
			error = put_user(cnow.rng, &p_cuser->rng);
			if (error) return error;
			error = put_user(cnow.dcd, &p_cuser->dcd);
			if (error) return error;
                        error = put_user(cnow.rx, &p_cuser->rx);
                        if (error) return error;
                        error = put_user(cnow.tx, &p_cuser->tx);
                        if (error) return error;
                        error = put_user(cnow.frame, &p_cuser->frame);
                        if (error) return error;
                        error = put_user(cnow.overrun, &p_cuser->overrun);
                        if (error) return error;
                        error = put_user(cnow.parity, &p_cuser->parity);
                        if (error) return error;
                        error = put_user(cnow.brk, &p_cuser->brk);
                        if (error) return error;
                        error = put_user(cnow.buf_overrun, &p_cuser->buf_overrun);

			if (error) return error;
			return 0;

                case TIOCSERGWILD:
                case TIOCSERSWILD:
                       /* "setserial -W" is called in Debian boot */
                        printk ("TIOCSER?WILD ioctl obsolete, ignored.\n");
                        return 0;

		default:
			return -ENOIOCTLCMD;
		}
	return 0;
}

static void rs_set_termios(struct tty_struct *tty, struct termios *old_termios)
{
	struct async_struct *info = (struct async_struct *)tty->driver_data;
	unsigned int cflag = tty->termios->c_cflag;

	if (   (cflag == old_termios->c_cflag)
	    && (   RELEVANT_IFLAG(tty->termios->c_iflag)
		== RELEVANT_IFLAG(old_termios->c_iflag)))
	  return;

	change_speed(info);

	/* Handle turning off CRTSCTS */
	if ((old_termios->c_cflag & CRTSCTS) &&
	    !(cflag & CRTSCTS)) {
		tty->hw_stopped = 0;
		rs_start(tty);
	}

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

	if (!info || serial_paranoia_check(info, tty->device, "rs_close"))
		return;

	state = info->state;

	save_flags(flags); cli();

	if (tty_hung_up_p(filp)) {
		DBG_CNT("before DEC-hung");
		MOD_DEC_USE_COUNT;
		restore_flags(flags);
		return;
	}

#ifdef SERIAL_DEBUG_OPEN
	baget_printk("rs_close ttys%d, count = %d\n",
		     info->line, state->count);
#endif
	if ((tty->count == 1) && (state->count != 1)) {
		/*
		 * Uh, oh.  tty->count is 1, which means that the tty
		 * structure will be freed.  state->count should always
		 * be one in these conditions.  If it's greater than
		 * one, we've got real problems, since it means the
		 * serial port won't be shutdown.
		 */
		baget_printk("rs_close: bad serial port count; "
			     "tty->count is 1, "
			     "state->count is %d\n", state->count);
		state->count = 1;
	}
	if (--state->count < 0) {
		baget_printk("rs_close: bad serial port count for "
			     "ttys%d: %d\n",
			     info->line, state->count);
		state->count = 0;
	}
	if (state->count) {
		DBG_CNT("before DEC-2");
		MOD_DEC_USE_COUNT;
		restore_flags(flags);
		return;
	}
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
	 * Now we wait for the transmit buffer to clear; and we notify
	 * the line discipline to only process XON/XOFF characters.
	 */
	tty->closing = 1;
	if (info->closing_wait != ASYNC_CLOSING_WAIT_NONE)
		tty_wait_until_sent(tty, info->closing_wait);
	/*
	 * At this point we stop accepting input.  To do this, we
	 * disable the receive line status interrupts, and tell the
	 * interrupt driver to stop checking the data ready bit in the
	 * line status register.
	 */
	info->IER &= ~(VAC_UART_INT_RX_BREAK_CHANGE | VAC_UART_INT_RX_ERRS);
	info->read_status_mask &= ~VAC_UART_STATUS_RX_READY;
	if (info->flags & ASYNC_INITIALIZED) {
		serial_outw(info, VAC_UART_INT_MASK, info->IER);
		/*
		 * Before we drop DTR, make sure the UART transmitter
		 * has completely drained; this is especially
		 * important if there is a transmit FIFO!
		 */
		rs_wait_until_sent(tty, info->timeout);
	}
	shutdown(info);
	if (tty->driver.flush_buffer)
		tty->driver.flush_buffer(tty);
	if (tty->ldisc.flush_buffer)
		tty->ldisc.flush_buffer(tty);
	tty->closing = 0;
	info->event = 0;
	info->tty = 0;
	if (info->blocked_open) {
		if (info->close_delay) {
			current->state = TASK_INTERRUPTIBLE;
			schedule_timeout(info->close_delay);
		}
		wake_up_interruptible(&info->open_wait);
	}
	info->flags &= ~(ASYNC_NORMAL_ACTIVE|ASYNC_CALLOUT_ACTIVE|
			 ASYNC_CLOSING);
	wake_up_interruptible(&info->close_wait);
	MOD_DEC_USE_COUNT;
	restore_flags(flags);
}

/*
 * rs_wait_until_sent() --- wait until the transmitter is empty
 */
static void rs_wait_until_sent(struct tty_struct *tty, int timeout)
{
	struct async_struct * info = (struct async_struct *)tty->driver_data;
	unsigned long orig_jiffies, char_time;
	int lsr;

	if (serial_paranoia_check(info, tty->device, "rs_wait_until_sent"))
		return;

	if (info->state->type == PORT_UNKNOWN)
		return;

        if (info->xmit_fifo_size == 0)
                return; /* Just in case.... */

	orig_jiffies = jiffies;
	/*
	 * Set the check interval to be 1/5 of the estimated time to
	 * send a single character, and make it at least 1.  The check
	 * interval should also be less than the timeout.
	 *
	 * Note: we have to use pretty tight timings here to satisfy
	 * the NIST-PCTS.
	 */
	char_time = (info->timeout - HZ/50) / info->xmit_fifo_size;
	char_time = char_time / 5;
	if (char_time == 0)
		char_time = 1;
	if (timeout)
	  char_time = MIN(char_time, timeout);
#ifdef SERIAL_DEBUG_RS_WAIT_UNTIL_SENT
	baget_printk("In rs_wait_until_sent(%d) check=%lu...",
		     timeout, char_time);
	baget_printk("jiff=%lu...", jiffies);
#endif
	while (!((lsr = serial_inp(info, VAC_UART_INT_STATUS)) & \
		 VAC_UART_STATUS_TX_EMPTY)) {
#ifdef SERIAL_DEBUG_RS_WAIT_UNTIL_SENT
		baget_printk("lsr = %d (jiff=%lu)...", lsr, jiffies);
#endif
		current->state = TASK_INTERRUPTIBLE;
		schedule_timeout(char_time);
		if (signal_pending(current))
			break;
		if (timeout && time_after(jiffies, orig_jiffies + timeout))
			break;
	}
	current->state = TASK_RUNNING;
#ifdef SERIAL_DEBUG_RS_WAIT_UNTIL_SENT
	baget_printk("lsr = %d (jiff=%lu)...done\n", lsr, jiffies);
#endif
}

/*
 * rs_hangup() --- called by tty_hangup() when a hangup is signaled.
 */
static void rs_hangup(struct tty_struct *tty)
{
	struct async_struct * info = (struct async_struct *)tty->driver_data;
	struct serial_state *state = info->state;

	if (serial_paranoia_check(info, tty->device, "rs_hangup"))
		return;

	state = info->state;

	rs_flush_buffer(tty);
	shutdown(info);
	info->event = 0;
	state->count = 0;
	info->flags &= ~(ASYNC_NORMAL_ACTIVE|ASYNC_CALLOUT_ACTIVE);
	info->tty = 0;
	wake_up_interruptible(&info->open_wait);
}

/*
 * ------------------------------------------------------------
 * rs_open() and friends
 * ------------------------------------------------------------
 */
static int block_til_ready(struct tty_struct *tty, struct file * filp,
			   struct async_struct *info)
{
	DECLARE_WAITQUEUE(wait, current);
	struct serial_state *state = info->state;
	int		retval;
	int		do_clocal = 0,  extra_count = 0;
	unsigned long   flags;

	/*
	 * If the device is in the middle of being closed, then block
	 * until it's done, and then try again.
	 */
	if (tty_hung_up_p(filp) ||
	    (info->flags & ASYNC_CLOSING)) {
		if (info->flags & ASYNC_CLOSING)
			interruptible_sleep_on(&info->close_wait);
#ifdef SERIAL_DO_RESTART
		return ((info->flags & ASYNC_HUP_NOTIFY) ?
			-EAGAIN : -ERESTARTSYS);
#else
		return -EAGAIN;
#endif
	}

	/*
	 * If this is a callout device, then just make sure the normal
	 * device isn't being used.
	 */
	if (tty->driver.subtype == SERIAL_TYPE_CALLOUT) {
		if (info->flags & ASYNC_NORMAL_ACTIVE)
			return -EBUSY;
		if ((info->flags & ASYNC_CALLOUT_ACTIVE) &&
		    (info->flags & ASYNC_SESSION_LOCKOUT) &&
		    (info->session != current->session))
		    return -EBUSY;
		if ((info->flags & ASYNC_CALLOUT_ACTIVE) &&
		    (info->flags & ASYNC_PGRP_LOCKOUT) &&
		    (info->pgrp != current->pgrp))
		    return -EBUSY;
		info->flags |= ASYNC_CALLOUT_ACTIVE;
		return 0;
	}

	/*
	 * If non-blocking mode is set, or the port is not enabled,
	 * then make the check up front and then exit.
	 */
	if ((filp->f_flags & O_NONBLOCK) ||
	    (tty->flags & (1 << TTY_IO_ERROR))) {
		if (info->flags & ASYNC_CALLOUT_ACTIVE)
			return -EBUSY;
		info->flags |= ASYNC_NORMAL_ACTIVE;
		return 0;
	}

	if (info->flags & ASYNC_CALLOUT_ACTIVE) {
		if (state->normal_termios.c_cflag & CLOCAL)
			do_clocal = 1;
	} else {
		if (tty->termios->c_cflag & CLOCAL)
			do_clocal = 1;
	}

	/*
	 * Block waiting for the carrier detect and the line to become
	 * free (i.e., not in use by the callout).  While we are in
	 * this loop, state->count is dropped by one, so that
	 * rs_close() knows when to free things.  We restore it upon
	 * exit, either normal or abnormal.
	 */
	retval = 0;
	add_wait_queue(&info->open_wait, &wait);
#ifdef SERIAL_DEBUG_OPEN
	baget_printk("block_til_ready before block: ttys%d, count = %d\n",
	       state->line, state->count);
#endif
	save_flags(flags); cli();
	if (!tty_hung_up_p(filp)) {
		extra_count = 1;
		state->count--;
	}
	restore_flags(flags);
	info->blocked_open++;
	while (1) {
		set_current_state(TASK_INTERRUPTIBLE);
		if (tty_hung_up_p(filp) ||
		    !(info->flags & ASYNC_INITIALIZED)) {
#ifdef SERIAL_DO_RESTART
			if (info->flags & ASYNC_HUP_NOTIFY)
				retval = -EAGAIN;
			else
				retval = -ERESTARTSYS;
#else
			retval = -EAGAIN;
#endif
			break;
		}
		if (!(info->flags & ASYNC_CALLOUT_ACTIVE) &&
		    !(info->flags & ASYNC_CLOSING))
			break;
		if (signal_pending(current)) {
			retval = -ERESTARTSYS;
			break;
		}
#ifdef SERIAL_DEBUG_OPEN
		baget_printk("block_til_ready blocking: ttys%d, count = %d\n",
		       info->line, state->count);
#endif
		schedule();
	}
	current->state = TASK_RUNNING;
	remove_wait_queue(&info->open_wait, &wait);
	if (extra_count)
		state->count++;
	info->blocked_open--;
#ifdef SERIAL_DEBUG_OPEN
	baget_printk("block_til_ready after blocking: ttys%d, count = %d\n",
	       info->line, state->count);
#endif
	if (retval)
		return retval;
	info->flags |= ASYNC_NORMAL_ACTIVE;
	return 0;
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

/*
 * This routine is called whenever a serial port is opened.  It
 * enables interrupts for a serial port, linking in its async structure into
 * the IRQ chain.   It also performs the serial-specific
 * initialization for the tty structure.
 */
static int rs_open(struct tty_struct *tty, struct file * filp)
{
	struct async_struct	*info;
	int 			retval, line;
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
	if (serial_paranoia_check(info, tty->device, "rs_open")) {
	        /* MOD_DEC_USE_COUNT; "info->tty" will cause this */
		return -ENODEV;
	}

#ifdef SERIAL_DEBUG_OPEN
	baget_printk("rs_open %s%d, count = %d\n",
		     tty->driver.name, info->line,
		     info->state->count);
#endif
	info->tty->low_latency = (info->flags & ASYNC_LOW_LATENCY) ? 1 : 0;

	if (!tmp_buf) {
		page = get_free_page(GFP_KERNEL);
		if (!page) {
			/* MOD_DEC_USE_COUNT; "info->tty" will cause this */
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
		/* MOD_DEC_USE_COUNT; "info->tty" will cause this */
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
		/* MOD_DEC_USE_COUNT; "info->tty" will cause this */
		return retval;
	}

	retval = block_til_ready(tty, filp, info);
	if (retval) {
		 /* MOD_DEC_USE_COUNT; "info->tty" will cause this */
#ifdef SERIAL_DEBUG_OPEN
		 baget_printk("rs_open returning after block_til_ready "
			      "with %d\n",
			      retval);
#endif
		 return retval;
	}

	if ((info->state->count == 1) &&
	    (info->flags & ASYNC_SPLIT_TERMIOS)) {
		if (tty->driver.subtype == SERIAL_TYPE_NORMAL)
			*tty->termios = info->state->normal_termios;
		else
			*tty->termios = info->state->callout_termios;
		change_speed(info);
	}
#ifdef CONFIG_SERIAL_CONSOLE
	if (sercons.cflag && sercons.index == line) {
		tty->termios->c_cflag = sercons.cflag;
		sercons.cflag = 0;
		change_speed(info);
	}
#endif
	info->session = current->session;
	info->pgrp = current->pgrp;

#ifdef SERIAL_DEBUG_OPEN
	baget_printk("rs_open ttys%d successful...", info->line);
#endif
	return 0;
}

/*
 * /proc fs routines....
 */

static inline int line_info(char *buf, struct serial_state *state)
{
	struct async_struct *info = state->info, scr_info;
	int	ret;

	ret = sprintf(buf, "%d: uart:%s port:%X irq:%d",
		      state->line, uart_config[state->type].name,
		      state->port, state->irq);

	if (!state->port || (state->type == PORT_UNKNOWN)) {
		ret += sprintf(buf+ret, "\n");
		return ret;
	}

	/*
	 * Figure out the current RS-232 lines
	 */
	if (!info) {
		info = &scr_info;	/* This is just for serial_{in,out} */

		info->magic = SERIAL_MAGIC;
		info->port = state->port;
		info->flags = state->flags;
		info->quot = 0;
		info->tty = 0;
	}

	if (info->quot) {
		ret += sprintf(buf+ret, " baud:%d",
			       state->baud_base / info->quot);
	}

	ret += sprintf(buf+ret, " tx:%d rx:%d",
		      state->icount.tx, state->icount.rx);

	if (state->icount.frame)
		ret += sprintf(buf+ret, " fe:%d", state->icount.frame);

	if (state->icount.parity)
		ret += sprintf(buf+ret, " pe:%d", state->icount.parity);

	if (state->icount.brk)
		ret += sprintf(buf+ret, " brk:%d", state->icount.brk);

	if (state->icount.overrun)
		ret += sprintf(buf+ret, " oe:%d", state->icount.overrun);

	return ret;
}

int rs_read_proc(char *page, char **start, off_t off, int count,
		 int *eof, void *data)
{
	int i, len = 0, l;
	off_t	begin = 0;

	len += sprintf(page, "serinfo:1.0 driver:%s\n", serial_version);
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
	*start = page + (off-begin);
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
static _INLINE_ void show_serial_version(void)
{
 	printk(KERN_INFO "%s version %s with", serial_name, serial_version);
#ifdef CONFIG_SERIAL_SHARE_IRQ
	printk(" SHARE_IRQ");
#endif
#define SERIAL_OPT
#ifdef CONFIG_SERIAL_DETECT_IRQ
	printk(" DETECT_IRQ");
#endif
#ifdef SERIAL_OPT
	printk(" enabled\n");
#else
	printk(" no serial options enabled\n");
#endif
#undef SERIAL_OPT
}


/*
 * This routine is called by rs_init() to initialize a specific serial
 * port.  It determines what type of UART chip this serial port is
 * using: 8250, 16450, 16550, 16550A.  The important question is
 * whether or not this UART is a 16550A or not, since this will
 * determine whether or not we can use its FIFO features or not.
 */

/*
 *  Functionality of this function is reduced: we already know we have a VAC,
 *  but still need to perform some important actions (see code :-).
 */
static void autoconfig(struct serial_state * state)
{
	struct async_struct *info, scr_info;
	unsigned long flags;

	/* Setting up important parameters */
	state->type = VAC_UART_TYPE;
	state->xmit_fifo_size = uart_config[state->type].dfl_xmit_fifo_size;

	info = &scr_info;       /* This is just for serial_{in,out} */

	info->magic = SERIAL_MAGIC;
        info->port  = state->port;
        info->flags = state->flags;

        save_flags(flags); cli();

	/* + Flush VAC input fifo */
        (void)serial_in(info, VAC_UART_RX);
        (void)serial_in(info, VAC_UART_RX);
        (void)serial_in(info, VAC_UART_RX);
        (void)serial_in(info, VAC_UART_RX);

	/* Disable interrupts */
        serial_outp(info, VAC_UART_INT_MASK, 0);

        restore_flags(flags);
}

int register_serial(struct serial_struct *req);
void unregister_serial(int line);

EXPORT_SYMBOL(register_serial);
EXPORT_SYMBOL(unregister_serial);

/*
 *  Important function for VAC UART check and reanimation.
 */

static void rs_timer(unsigned long dummy)
{
        static unsigned long last_strobe = 0;
        struct async_struct *info;
        unsigned int    i;
        unsigned long flags;

        if ((jiffies - last_strobe) >= RS_STROBE_TIME) {
                for (i=1; i < NR_IRQS; i++) {
                        info = IRQ_ports[i];
                        if (!info)
                                continue;
                        save_flags(flags); cli();
#ifdef CONFIG_SERIAL_SHARE_IRQ
                        if (info->next_port) {
                                do {
                                        serial_out(info, VAC_UART_INT_MASK, 0);
                                        info->IER |= VAC_UART_INT_TX_EMPTY;
                                        serial_out(info, VAC_UART_INT_MASK,
						   info->IER);
                                        info = info->next_port;
                                } while (info);
				rs_interrupt(i, NULL, NULL);
                        } else
#endif /* CONFIG_SERIAL_SHARE_IRQ */
                                rs_interrupt_single(i, NULL, NULL);
                        restore_flags(flags);
                }
        }
        last_strobe = jiffies;
        mod_timer(&vacs_timer, jiffies + RS_STROBE_TIME);

	/*
	 *  It looks this code for case we share IRQ with console...
	 */

        if (IRQ_ports[0]) {
                save_flags(flags); cli();
#ifdef CONFIG_SERIAL_SHARE_IRQ
                rs_interrupt(0, NULL, NULL);
#else
                rs_interrupt_single(0, NULL, NULL);
#endif
                restore_flags(flags);

                mod_timer(&vacs_timer, jiffies + IRQ_timeout[0] - 2);
        }
}

/*
 * The serial driver boot-time initialization code!
 */
int __init rs_init(void)
{
	int i;
	struct serial_state * state;
	extern void atomwide_serial_init (void);
	extern void dualsp_serial_init (void);

#ifdef CONFIG_ATOMWIDE_SERIAL
	atomwide_serial_init ();
#endif
#ifdef CONFIG_DUALSP_SERIAL
	dualsp_serial_init ();
#endif

	init_bh(SERIAL_BH, do_serial_bh);
	init_timer(&vacs_timer);
	vacs_timer.function = rs_timer;
	vacs_timer.expires = 0;

	for (i = 0; i < NR_IRQS; i++) {
		IRQ_ports[i] = 0;
		IRQ_timeout[i] = 0;
	}


/*
 *  It is not a good idea to share interrupts with console,
 *  but it looks we cannot avoid it.
 */
#if 0

#ifdef CONFIG_SERIAL_CONSOLE
	/*
	 *	The interrupt of the serial console port
	 *	can't be shared.
	 */
	if (sercons.flags & CON_CONSDEV) {
		for(i = 0; i < NR_PORTS; i++)
			if (i != sercons.index &&
			    rs_table[i].irq == rs_table[sercons.index].irq)
				rs_table[i].irq = 0;
	}
#endif

#endif
	show_serial_version();

	/* Initialize the tty_driver structure */

	memset(&serial_driver, 0, sizeof(struct tty_driver));
	serial_driver.magic = TTY_DRIVER_MAGIC;
	serial_driver.driver_name = "serial";
	serial_driver.name = "ttyS";
	serial_driver.major = TTY_MAJOR;
	serial_driver.minor_start = 64;
	serial_driver.num = NR_PORTS;
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
		panic("Couldn't register serial driver");
	if (tty_register_driver(&callout_driver))
		panic("Couldn't register callout driver");

	for (i = 0, state = rs_table; i < NR_PORTS; i++,state++) {
		state->magic = SSTATE_MAGIC;
		state->line = i;
		state->type = PORT_UNKNOWN;
		state->custom_divisor = 0;
		state->close_delay = 5*HZ/10;
		state->closing_wait = 30*HZ;
		state->callout_termios = callout_driver.init_termios;
		state->normal_termios = serial_driver.init_termios;
		state->icount.cts = state->icount.dsr =
			state->icount.rng = state->icount.dcd = 0;
		state->icount.rx = state->icount.tx = 0;
		state->icount.frame = state->icount.parity = 0;
		state->icount.overrun = state->icount.brk = 0;
		state->irq = state->irq;
		if (check_region(state->port,8))
			continue;
	        if (state->flags & ASYNC_BOOT_AUTOCONF)
 			autoconfig(state);
	}

	/*
	 * Detect the IRQ only once every port is initialised,
	 * because some 16450 do not reset to 0 the MCR register.
	 */
	for (i = 0, state = rs_table; i < NR_PORTS; i++,state++) {
		if (state->type == PORT_UNKNOWN)
			continue;
		printk(KERN_INFO "ttyS%02d%s at 0x%04x (irq = %d) is a %s\n",
		       state->line,
		       (state->flags & ASYNC_FOURPORT) ? " FourPort" : "",
		       state->port, state->irq,
		       uart_config[state->type].name);
	}
	return 0;
}

/*
 * register_serial and unregister_serial allows for serial ports to be
 * configured at run-time, to support PCMCIA modems.
 */
int register_serial(struct serial_struct *req)
{
	int i;
	unsigned long flags;
	struct serial_state *state;

	save_flags(flags);
	cli();
	for (i = 0; i < NR_PORTS; i++) {
		if (rs_table[i].port == req->port)
			break;
	}
	if (i == NR_PORTS) {
		for (i = 0; i < NR_PORTS; i++)
			if ((rs_table[i].type == PORT_UNKNOWN) &&
			    (rs_table[i].count == 0))
				break;
	}
	if (i == NR_PORTS) {
		restore_flags(flags);
		return -1;
	}
	state = &rs_table[i];
	if (rs_table[i].count) {
		restore_flags(flags);
		printk("Couldn't configure serial #%d (port=%d,irq=%d): "
		       "device already open\n", i, req->port, req->irq);
		return -1;
	}
	state->irq = req->irq;
	state->port = req->port;
	state->flags = req->flags;

	autoconfig(state);
	if (state->type == PORT_UNKNOWN) {
		restore_flags(flags);
		printk("register_serial(): autoconfig failed\n");
		return -1;
	}
	restore_flags(flags);

	printk(KERN_INFO "tty%02d at 0x%04x (irq = %d) is a %s\n",
	       state->line, state->port, state->irq,
	       uart_config[state->type].name);
	return state->line;
}

void unregister_serial(int line)
{
	unsigned long flags;
	struct serial_state *state = &rs_table[line];

	save_flags(flags);
	cli();
	if (state->info && state->info->tty)
		tty_hangup(state->info->tty);
	state->type = PORT_UNKNOWN;
	printk(KERN_INFO "tty%02d unloaded\n", state->line);
	restore_flags(flags);
}

#ifdef MODULE
int init_module(void)
{
	return rs_init();
}

void cleanup_module(void)
{
	unsigned long flags;
	int e1, e2;
	int i;

	printk("Unloading %s: version %s\n", serial_name, serial_version);
	save_flags(flags);
	cli();

	del_timer_sync(&vacs_timer);
        remove_bh(SERIAL_BH);

	if ((e1 = tty_unregister_driver(&serial_driver)))
		printk("SERIAL: failed to unregister serial driver (%d)\n",
		       e1);
	if ((e2 = tty_unregister_driver(&callout_driver)))
		printk("SERIAL: failed to unregister callout driver (%d)\n",
		       e2);
	restore_flags(flags);

	for (i = 0; i < NR_PORTS; i++) {
		if (rs_table[i].type != PORT_UNKNOWN)
			release_region(rs_table[i].port, 8);
	}
	if (tmp_buf) {
		free_page((unsigned long) tmp_buf);
		tmp_buf = NULL;
	}
}
#endif /* MODULE */


/*
 * ------------------------------------------------------------
 * Serial console driver
 * ------------------------------------------------------------
 */
#ifdef CONFIG_SERIAL_CONSOLE

#define BOTH_EMPTY (VAC_UART_STATUS_TX_EMPTY | VAC_UART_STATUS_TX_EMPTY)

/*
 *	Wait for transmitter & holding register to empty
 */
static inline void wait_for_xmitr(struct async_struct *info)
{
	int lsr;
	unsigned int tmout = 1000000;

	do {
		lsr = serial_inp(info, VAC_UART_INT_STATUS);
		if (--tmout == 0) break;
	} while ((lsr & BOTH_EMPTY) != BOTH_EMPTY);
}

/*
 *	Print a string to the serial port trying not to disturb
 *	any possible real use of the port...
 */
static void serial_console_write(struct console *co, const char *s,
				unsigned count)
{
	struct serial_state *ser;
	int ier;
	unsigned i;
	struct async_struct scr_info; /* serial_{in,out} because HUB6 */

	ser = rs_table + co->index;
	scr_info.magic = SERIAL_MAGIC;
	scr_info.port = ser->port;
	scr_info.flags = ser->flags;

	/*
	 *	First save the IER then disable the interrupts
	 */
	ier = serial_inp(&scr_info, VAC_UART_INT_MASK);
	serial_outw(&scr_info, VAC_UART_INT_MASK, 0x00);

	/*
	 *	Now, do each character
	 */
	for (i = 0; i < count; i++, s++) {
		wait_for_xmitr(&scr_info);

		/*
		 *	Send the character out.
		 *	If a LF, also do CR...
		 */
		serial_outp(&scr_info, VAC_UART_TX, (unsigned short)*s << 8);
		if (*s == 10) {
			wait_for_xmitr(&scr_info);
			serial_outp(&scr_info, VAC_UART_TX, 13 << 8);
		}
	}

	/*
	 *	Finally, Wait for transmitter & holding register to empty
	 * 	and restore the IER
	 */
	wait_for_xmitr(&scr_info);
	serial_outp(&scr_info, VAC_UART_INT_MASK, ier);
}

static kdev_t serial_console_device(struct console *c)
{
	return MKDEV(TTY_MAJOR, 64 + c->index);
}

/*
 *	Setup initial baud/bits/parity. We do two things here:
 *	- construct a cflag setting for the first rs_open()
 *	- initialize the serial port
 *	Return non-zero if we didn't find a serial port.
 */
static int __init serial_console_setup(struct console *co, char *options)
{
	struct serial_state *ser;
	unsigned cval;
	int	baud = 9600;
	int	bits = 8;
	int	parity = 'n';
	int	cflag = CREAD | HUPCL | CLOCAL;
	int	quot = 0;
	char	*s;
	struct async_struct scr_info; /* serial_{in,out} because HUB6 */

	if (options) {
		baud = simple_strtoul(options, NULL, 10);
		s = options;
		while(*s >= '0' && *s <= '9')
			s++;
		if (*s) parity = *s++;
		if (*s) bits   = *s - '0';
	}

	/*
	 *	Now construct a cflag setting.
	 */
	switch(baud) {
		case 1200:
			cflag |= B1200;
			break;
		case 2400:
			cflag |= B2400;
			break;
		case 4800:
			cflag |= B4800;
			break;
		case 19200:
			cflag |= B19200;
			break;
		case 38400:
			cflag |= B38400;
			break;
		case 57600:
			cflag |= B57600;
			break;
		case 115200:
			cflag |= B115200;
			break;
		case 9600:
		default:
			cflag |= B9600;
			break;
	}
	switch(bits) {
		case 7:
			cflag |= CS7;
			break;
		default:
		case 8:
			cflag |= CS8;
			break;
	}
	switch(parity) {
		case 'o': case 'O':
			cflag |= PARODD;
			break;
		case 'e': case 'E':
			cflag |= PARENB;
			break;
	}
	co->cflag = cflag;

	/*
	 *	Divisor, bytesize and parity
	 */
	ser = rs_table + co->index;
	scr_info.magic = SERIAL_MAGIC;
	scr_info.port = ser->port;
	scr_info.flags = ser->flags;

	quot = ser->baud_base / baud;
	cval = cflag & (CSIZE | CSTOPB);

	cval >>= 4;

	cval &= ~VAC_UART_MODE_PARITY_ENABLE;
	if (cflag & PARENB)
		cval |= VAC_UART_MODE_PARITY_ENABLE;
	if (cflag & PARODD)
		cval |= VAC_UART_MODE_PARITY_ODD;

	/*
	 *	Disable UART interrupts, set DTR and RTS high
	 *	and set speed.
	 */
	switch (baud) {
	default:
	case 9600:
		cval |= VAC_UART_MODE_BAUD(7);
		break;
	case 4800:
		cval |= VAC_UART_MODE_BAUD(6);
		break;
	case 2400:
		cval |= VAC_UART_MODE_BAUD(5);
		break;
	case 1200:
		cval |= VAC_UART_MODE_BAUD(4);
		break;
	case 600:
		cval |= VAC_UART_MODE_BAUD(3);
		break;
	case 300:
		cval |= VAC_UART_MODE_BAUD(2);
		break;
#ifndef QUAD_UART_SPEED
	case 150:
#else
	case 38400:
#endif
		cval |= VAC_UART_MODE_BAUD(1);
		break;
#ifndef QUAD_UART_SPEED
	case 75:
#else
	case 19200:
#endif
		cval |= VAC_UART_MODE_BAUD(0);
		break;
	}

	/* Baget VAC need some adjustments for computed value */
	cval = vac_uart_mode_fixup(cval);

        serial_outp(&scr_info, VAC_UART_MODE, cval);
	serial_outp(&scr_info, VAC_UART_INT_MASK, 0);

	return 0;
}

static struct console sercons = {
	.name		= "ttyS",
	.write		= serial_console_write,
	.device		= serial_console_device,
	.setup		= serial_console_setup,
	.flags		= CON_PRINTBUFFER,
	.index		= -1,
};

/*
 *	Register console.
 */
long __init serial_console_init(long kmem_start, long kmem_end)
{
	register_console(&sercons);
	return kmem_start;
}
#endif

#ifdef CONFIG_KGDB
#undef PRINT_DEBUG_PORT_INFO

/*
 * This is the interface to the remote debugger stub.
 * I've put that here to be able to control the serial
 * device more directly.
 */

static int initialized;

static int rs_debug_init(struct async_struct *info)
{
	int quot;

	autoconfig(info);	/* autoconfigure ttyS0, whatever that is */

#ifdef PRINT_DEBUG_PORT_INFO
	baget_printk("kgdb debug interface:: tty%02d at 0x%04x",
		     info->line, info->port);
	switch (info->type) {
		case PORT_8250:
			baget_printk(" is a 8250\n");
			break;
		case PORT_16450:
			baget_printk(" is a 16450\n");
			break;
		case PORT_16550:
			baget_printk(" is a 16550\n");
			break;
		case PORT_16550A:
			baget_printk(" is a 16550A\n");
			break;
		case PORT_16650:
			baget_printk(" is a 16650\n");
			break;
		default:
			baget_printk(" is of unknown type -- unusable\n");
			break;
	}
#endif

	if (info->port == PORT_UNKNOWN)
		return -1;

	/*
	 * Clear all interrupts
	 */

	(void)serial_inp(info, VAC_UART_INT_STATUS);
	(void)serial_inp(info, VAC_UART_RX);

	/*
	 * Now, initialize the UART
	 */
	serial_outp(info,VAC_UART_MODE,VAC_UART_MODE_INITIAL); /* reset DLAB */
	if (info->flags & ASYNC_FOURPORT) {
		info->MCR = UART_MCR_DTR | UART_MCR_RTS;
		info->MCR_noint = UART_MCR_DTR | UART_MCR_OUT1;
	} else {
		info->MCR = UART_MCR_DTR | UART_MCR_RTS | UART_MCR_OUT2;
		info->MCR_noint = UART_MCR_DTR | UART_MCR_RTS;
	}

	info->MCR = info->MCR_noint;	     /* no interrupts, please */
	/*
	 * and set the speed of the serial port
	 * (currently hardwired to 9600 8N1
	 */

	quot = info->baud_base / 9600;	     /* baud rate is fixed to 9600 */
	/* FIXME: if rs_debug interface is needed, we need to set speed here */

	return 0;
}

int putDebugChar(char c)
{
	struct async_struct *info = rs_table;

	if (!initialized) { 		/* need to init device first */
		if (rs_debug_init(info) == 0)
			initialized = 1;
		else
			return 0;
	}

	while ((serial_inw(info, VAC_UART_INT_STATUS) & \
                VAC_UART_STATUS_TX_EMPTY) == 0)
		;
	serial_out(info, VAC_UART_TX, (unsigned short)c << 8);

	return 1;
}

char getDebugChar(void)
{
	struct async_struct *info = rs_table;

	if (!initialized) { 		/* need to init device first */
		if (rs_debug_init(info) == 0)
			initialized = 1;
		else
			return 0;
	}
	while (!(serial_inw(info, VAC_UART_INT_STATUS) & \
		 VAC_UART_STATUS_RX_READY))
		;

	return(serial_inp(info, VAC_UART_RX));
}

#endif /* CONFIG_KGDB */
