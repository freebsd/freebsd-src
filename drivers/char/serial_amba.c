/*
 *  linux/drivers/char/serial_amba.c
 *
 *  Driver for AMBA serial ports
 *
 *  Based on drivers/char/serial.c, by Linus Torvalds, Theodore Ts'o.
 *
 *  Copyright 1999 ARM Limited
 *  Copyright (C) 2000 Deep Blue Solutions Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 * This is a generic driver for ARM AMBA-type serial ports.  They
 * have a lot of 16550-like features, but are not register compatable.
 * Note that although they do have CTS, DCD and DSR inputs, they do
 * not have an RI input, nor do they have DTR or RTS outputs.  If
 * required, these have to be supplied via some other means (eg, GPIO)
 * and hooked into this driver.
 *
 * This could very easily become a generic serial driver for dumb UARTs
 * (eg, {82,16x}50, 21285, SA1100).
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
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
#include <linux/circ_buf.h>
#include <linux/serial.h>
#include <linux/console.h>
#include <linux/sysrq.h>

#include <asm/system.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/bitops.h>

#include <asm/hardware/serial_amba.h>

#define SERIAL_AMBA_NAME	"ttyAM"
#define SERIAL_AMBA_MAJOR	204
#define SERIAL_AMBA_MINOR	16
#define SERIAL_AMBA_NR		2

#define CALLOUT_AMBA_NAME	"cuaam"
#define CALLOUT_AMBA_MAJOR	205
#define CALLOUT_AMBA_MINOR	16
#define CALLOUT_AMBA_NR		SERIAL_AMBA_NR

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#define DEBUG 0
#define DEBUG_LEDS 0

#if DEBUG_LEDS
extern int get_leds(void);
extern int set_leds(int);
#endif

/*
 * Access routines for the AMBA UARTs
 */
#define UART_GET_INT_STATUS(p)	IO_READ((p)->uart_base + AMBA_UARTIIR)
#define UART_GET_FR(p)		IO_READ((p)->uart_base + AMBA_UARTFR)
#define UART_GET_CHAR(p)	IO_READ((p)->uart_base + AMBA_UARTDR)
#define UART_PUT_CHAR(p, c)	IO_WRITE((p)->uart_base + AMBA_UARTDR, (c))
#define UART_GET_RSR(p)		IO_READ((p)->uart_base + AMBA_UARTRSR)
#define UART_GET_CR(p)		IO_READ((p)->uart_base + AMBA_UARTCR)
#define UART_PUT_CR(p,c)	IO_WRITE((p)->uart_base + AMBA_UARTCR, (c))
#define UART_GET_LCRL(p)	IO_READ((p)->uart_base + AMBA_UARTLCR_L)
#define UART_PUT_LCRL(p,c)	IO_WRITE((p)->uart_base + AMBA_UARTLCR_L, (c))
#define UART_GET_LCRM(p)	IO_READ((p)->uart_base + AMBA_UARTLCR_M)
#define UART_PUT_LCRM(p,c)	IO_WRITE((p)->uart_base + AMBA_UARTLCR_M, (c))
#define UART_GET_LCRH(p)	IO_READ((p)->uart_base + AMBA_UARTLCR_H)
#define UART_PUT_LCRH(p,c)	IO_WRITE((p)->uart_base + AMBA_UARTLCR_H, (c))
#define UART_RX_DATA(s)		(((s) & AMBA_UARTFR_RXFE) == 0)
#define UART_TX_READY(s)	(((s) & AMBA_UARTFR_TXFF) == 0)
#define UART_TX_EMPTY(p)	((UART_GET_FR(p) & AMBA_UARTFR_TMSK) == 0)

#define AMBA_UARTRSR_ANY	(AMBA_UARTRSR_OE|AMBA_UARTRSR_BE|AMBA_UARTRSR_PE|AMBA_UARTRSR_FE)
#define AMBA_UARTFR_MODEM_ANY	(AMBA_UARTFR_DCD|AMBA_UARTFR_DSR|AMBA_UARTFR_CTS)

/*
 * Things needed by tty driver
 */
static struct tty_driver ambanormal_driver, ambacallout_driver;
static int ambauart_refcount;
static struct tty_struct *ambauart_table[SERIAL_AMBA_NR];
static struct termios *ambauart_termios[SERIAL_AMBA_NR];
static struct termios *ambauart_termios_locked[SERIAL_AMBA_NR];

#if defined(CONFIG_SERIAL_AMBA_CONSOLE) && defined(CONFIG_MAGIC_SYSRQ)
#define SUPPORT_SYSRQ
#endif

/*
 * Things needed internally to this driver
 */

/*
 * tmp_buf is used as a temporary buffer by serial_write.  We need to
 * lock it in case the copy_from_user blocks while swapping in a page,
 * and some other program tries to do a serial write at the same time.
 * Since the lock will only come under contention when the system is
 * swapping and available memory is low, it makes sense to share one
 * buffer across all the serial ports, since it significantly saves
 * memory if large numbers of serial ports are open.
 */
static u_char *tmp_buf;
static DECLARE_MUTEX(tmp_buf_sem);

#define HIGH_BITS_OFFSET	((sizeof(long)-sizeof(int))*8)

/* number of characters left in xmit buffer before we ask for more */
#define WAKEUP_CHARS		256
#define AMBA_ISR_PASS_LIMIT	256

#define EVT_WRITE_WAKEUP	0

struct amba_icount {
	__u32	cts;
	__u32	dsr;
	__u32	rng;
	__u32	dcd;
	__u32	rx;
	__u32	tx;
	__u32	frame;
	__u32	overrun;
	__u32	parity;
	__u32	brk;
	__u32	buf_overrun;
};

/*
 * Static information about the port
 */
struct amba_port {
	unsigned int		uart_base;
	unsigned int		irq;
	unsigned int		uartclk;
	unsigned int		fifosize;
	unsigned int		tiocm_support;
	void (*set_mctrl)(struct amba_port *, u_int mctrl);
};	

/*
 * This is the state information which is persistent across opens
 */
struct amba_state {
	struct amba_icount	icount;
	unsigned int		line;
	unsigned int		close_delay;
	unsigned int		closing_wait;
	unsigned int		custom_divisor;
	unsigned int		flags;
	struct termios		normal_termios;
	struct termios		callout_termios;

	int			count;
	struct amba_info	*info;
};

#define AMBA_XMIT_SIZE 1024
/*
 * This is the state information which is only valid when the port is open.
 */
struct amba_info {
	struct amba_port	*port;
	struct amba_state	*state;
	struct tty_struct	*tty;
	unsigned char		x_char;
	unsigned char		old_status;
	unsigned char		read_status_mask;
	unsigned char		ignore_status_mask;
	struct circ_buf		xmit;
	unsigned int		flags;
#ifdef SUPPORT_SYSRQ
	unsigned long		sysrq;
#endif

	unsigned int		event;
	unsigned int		timeout;
	unsigned int		lcr_h;
	unsigned int		mctrl;
	int			blocked_open;
	pid_t			session;
	pid_t			pgrp;

	struct tasklet_struct	tlet;

	wait_queue_head_t	open_wait;
	wait_queue_head_t	close_wait;
	wait_queue_head_t	delta_msr_wait;
};

#ifdef CONFIG_SERIAL_AMBA_CONSOLE
static struct console ambauart_cons;
#endif
static void ambauart_change_speed(struct amba_info *info, struct termios *old_termios);
static void ambauart_wait_until_sent(struct tty_struct *tty, int timeout);

#if 1 //def CONFIG_SERIAL_INTEGRATOR
static void amba_set_mctrl_null(struct amba_port *port, u_int mctrl)
{
}

static struct amba_port amba_ports[SERIAL_AMBA_NR] = {
	{
		uart_base:	IO_ADDRESS(INTEGRATOR_UART0_BASE),
		irq:		IRQ_UARTINT0,
		uartclk:	14745600,
		fifosize:	8,
		set_mctrl:	amba_set_mctrl_null,
	},
	{
		uart_base:	IO_ADDRESS(INTEGRATOR_UART1_BASE),
		irq:		IRQ_UARTINT1,
		uartclk:	14745600,
		fifosize:	8,
		set_mctrl:	amba_set_mctrl_null,
	}
};
#endif

static struct amba_state amba_state[SERIAL_AMBA_NR];

static void ambauart_enable_rx_interrupt(struct amba_info *info)
{
	unsigned int cr;

	cr = UART_GET_CR(info->port);
	cr |= AMBA_UARTCR_RIE | AMBA_UARTCR_RTIE;
	UART_PUT_CR(info->port, cr);
}

static void ambauart_disable_rx_interrupt(struct amba_info *info)
{
	unsigned int cr;

	cr = UART_GET_CR(info->port);
	cr &= ~(AMBA_UARTCR_RIE | AMBA_UARTCR_RTIE);
	UART_PUT_CR(info->port, cr);
}

static void ambauart_enable_tx_interrupt(struct amba_info *info)
{
	unsigned int cr;

	cr = UART_GET_CR(info->port);
	cr |= AMBA_UARTCR_TIE;
	UART_PUT_CR(info->port, cr);
}

static void ambauart_disable_tx_interrupt(struct amba_info *info)
{
	unsigned int cr;

	cr = UART_GET_CR(info->port);
	cr &= ~AMBA_UARTCR_TIE;
	UART_PUT_CR(info->port, cr);
}

static void ambauart_stop(struct tty_struct *tty)
{
	struct amba_info *info = tty->driver_data;
	unsigned long flags;

	save_flags(flags); cli();
	ambauart_disable_tx_interrupt(info);
	restore_flags(flags);
}

static void ambauart_start(struct tty_struct *tty)
{
	struct amba_info *info = tty->driver_data;
	unsigned long flags;

	save_flags(flags); cli();
	if (info->xmit.head != info->xmit.tail
	    && info->xmit.buf)
		ambauart_enable_tx_interrupt(info);
	restore_flags(flags);
}


/*
 * This routine is used by the interrupt handler to schedule
 * processing in the software interrupt portion of the driver.
 */
static void ambauart_event(struct amba_info *info, int event)
{
	info->event |= 1 << event;
	tasklet_schedule(&info->tlet);
}

static void
#ifdef SUPPORT_SYSRQ
ambauart_rx_chars(struct amba_info *info, struct pt_regs *regs)
#else
ambauart_rx_chars(struct amba_info *info)
#endif
{
	struct tty_struct *tty = info->tty;
	unsigned int status, ch, rsr, flg, ignored = 0;
	struct amba_icount *icount = &info->state->icount;
	struct amba_port *port = info->port;

	status = UART_GET_FR(port);
	while (UART_RX_DATA(status)) {
		ch = UART_GET_CHAR(port);

		if (tty->flip.count >= TTY_FLIPBUF_SIZE)
			goto ignore_char;
		icount->rx++;

		flg = TTY_NORMAL;

		/*
		 * Note that the error handling code is
		 * out of the main execution path
		 */
		rsr = UART_GET_RSR(port);
		if (rsr & AMBA_UARTRSR_ANY)
			goto handle_error;
#ifdef SUPPORT_SYSRQ
		if (info->sysrq) {
			if (ch && time_before(jiffies, info->sysrq)) {
				handle_sysrq(ch, regs, NULL, NULL);
				info->sysrq = 0;
				goto ignore_char;
			}
			info->sysrq = 0;
		}
#endif
	error_return:
		*tty->flip.flag_buf_ptr++ = flg;
		*tty->flip.char_buf_ptr++ = ch;
		tty->flip.count++;
	ignore_char:
		status = UART_GET_FR(port);
	}
out:
	tty_flip_buffer_push(tty);
	return;

handle_error:
	if (rsr & AMBA_UARTRSR_BE) {
		rsr &= ~(AMBA_UARTRSR_FE | AMBA_UARTRSR_PE);
		icount->brk++;

#ifdef SUPPORT_SYSRQ
		if (info->state->line == ambauart_cons.index) {
			if (!info->sysrq) {
				info->sysrq = jiffies + HZ*5;
				goto ignore_char;
			}
		}
#endif
	} else if (rsr & AMBA_UARTRSR_PE)
		icount->parity++;
	else if (rsr & AMBA_UARTRSR_FE)
		icount->frame++;
	if (rsr & AMBA_UARTRSR_OE)
		icount->overrun++;

	if (rsr & info->ignore_status_mask) {
		if (++ignored > 100)
			goto out;
		goto ignore_char;
	}
	rsr &= info->read_status_mask;

	if (rsr & AMBA_UARTRSR_BE)
		flg = TTY_BREAK;
	else if (rsr & AMBA_UARTRSR_PE)
		flg = TTY_PARITY;
	else if (rsr & AMBA_UARTRSR_FE)
		flg = TTY_FRAME;

	if (rsr & AMBA_UARTRSR_OE) {
		/*
		 * CHECK: does overrun affect the current character?
		 * ASSUMPTION: it does not.
		 */
		*tty->flip.flag_buf_ptr++ = flg;
		*tty->flip.char_buf_ptr++ = ch;
		tty->flip.count++;
		if (tty->flip.count >= TTY_FLIPBUF_SIZE)
			goto ignore_char;
		ch = 0;
		flg = TTY_OVERRUN;
	}
#ifdef SUPPORT_SYSRQ
	info->sysrq = 0;
#endif
	goto error_return;
}

static void ambauart_tx_chars(struct amba_info *info)
{
	struct amba_port *port = info->port;
	int count;

	if (info->x_char) {
		UART_PUT_CHAR(port, info->x_char);
		info->state->icount.tx++;
		info->x_char = 0;
		return;
	}
	if (info->xmit.head == info->xmit.tail
	    || info->tty->stopped
	    || info->tty->hw_stopped) {
		ambauart_disable_tx_interrupt(info);
		return;
	}

	count = port->fifosize;
	do {
		UART_PUT_CHAR(port, info->xmit.buf[info->xmit.tail]);
		info->xmit.tail = (info->xmit.tail + 1) & (AMBA_XMIT_SIZE - 1);
		info->state->icount.tx++;
		if (info->xmit.head == info->xmit.tail)
			break;
	} while (--count > 0);

	if (CIRC_CNT(info->xmit.head,
		     info->xmit.tail,
		     AMBA_XMIT_SIZE) < WAKEUP_CHARS)
		ambauart_event(info, EVT_WRITE_WAKEUP);

	if (info->xmit.head == info->xmit.tail) {
		ambauart_disable_tx_interrupt(info);
	}
}

static void ambauart_modem_status(struct amba_info *info)
{
	unsigned int status, delta;
	struct amba_icount *icount = &info->state->icount;

	status = UART_GET_FR(info->port) & AMBA_UARTFR_MODEM_ANY;

	delta = status ^ info->old_status;
	info->old_status = status;

	if (!delta)
		return;

	if (delta & AMBA_UARTFR_DCD) {
		icount->dcd++;
#ifdef CONFIG_HARD_PPS
		if ((info->flags & ASYNC_HARDPPS_CD) &&
		    (status & AMBA_UARTFR_DCD)
			hardpps();
#endif
		if (info->flags & ASYNC_CHECK_CD) {
			if (status & AMBA_UARTFR_DCD)
				wake_up_interruptible(&info->open_wait);
			else if (!((info->flags & ASYNC_CALLOUT_ACTIVE) &&
				   (info->flags & ASYNC_CALLOUT_NOHUP))) {
				if (info->tty)
					tty_hangup(info->tty);
			}
		}
	}

	if (delta & AMBA_UARTFR_DSR)
		icount->dsr++;

	if (delta & AMBA_UARTFR_CTS) {
		icount->cts++;

		if (info->flags & ASYNC_CTS_FLOW) {
			status &= AMBA_UARTFR_CTS;

			if (info->tty->hw_stopped) {
				if (status) {
					info->tty->hw_stopped = 0;
					ambauart_enable_tx_interrupt(info);
					ambauart_event(info, EVT_WRITE_WAKEUP);
				}
			} else {
				if (!status) {
					info->tty->hw_stopped = 1;
					ambauart_disable_tx_interrupt(info);
				}
			}
		}
	}
	wake_up_interruptible(&info->delta_msr_wait);

}

static void ambauart_int(int irq, void *dev_id, struct pt_regs *regs)
{
	struct amba_info *info = dev_id;
	unsigned int status, pass_counter = 0;

#if DEBUG_LEDS
	// tell the world
	set_leds(get_leds() | RED_LED);
#endif

	status = UART_GET_INT_STATUS(info->port);
	do {
		/*
		 * FIXME: what about clearing the interrupts?
		 */

		if (status & (AMBA_UARTIIR_RTIS | AMBA_UARTIIR_RIS))
#ifdef SUPPORT_SYSRQ
			ambauart_rx_chars(info, regs);
#else
			ambauart_rx_chars(info);
#endif
		if (status & AMBA_UARTIIR_TIS)
			ambauart_tx_chars(info);
		if (status & AMBA_UARTIIR_MIS)
			ambauart_modem_status(info);
		if (pass_counter++ > AMBA_ISR_PASS_LIMIT)
			break;

		status = UART_GET_INT_STATUS(info->port);
	} while (status & (AMBA_UARTIIR_RTIS | AMBA_UARTIIR_RIS | AMBA_UARTIIR_TIS));

#if DEBUG_LEDS
	// tell the world
	set_leds(get_leds() & ~RED_LED);
#endif
}

static void ambauart_tasklet_action(unsigned long data)
{
	struct amba_info *info = (struct amba_info *)data;
	struct tty_struct *tty;

	tty = info->tty;
	if (!tty || !test_and_clear_bit(EVT_WRITE_WAKEUP, &info->event))
		return;

	if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) &&
	    tty->ldisc.write_wakeup)
		(tty->ldisc.write_wakeup)(tty);
	wake_up_interruptible(&tty->write_wait);
}

static int ambauart_startup(struct amba_info *info)
{
	unsigned long flags;
	unsigned long page;
	int retval = 0;

	page = get_zeroed_page(GFP_KERNEL);
	if (!page)
		return -ENOMEM;

	save_flags(flags); cli();

	if (info->flags & ASYNC_INITIALIZED) {
		free_page(page);
		goto errout;
	}

	if (info->xmit.buf)
		free_page(page);
	else
		info->xmit.buf = (unsigned char *) page;

	/*
	 * Allocate the IRQ
	 */
	retval = request_irq(info->port->irq, ambauart_int, 0, "amba", info);
	if (retval) {
		if (capable(CAP_SYS_ADMIN)) {
			if (info->tty)
				set_bit(TTY_IO_ERROR, &info->tty->flags);
			retval = 0;
		}
		goto errout;
	}

	info->mctrl = 0;
	if (info->tty->termios->c_cflag & CBAUD)
		info->mctrl = TIOCM_RTS | TIOCM_DTR;
	info->port->set_mctrl(info->port, info->mctrl);

	/*
	 * initialise the old status of the modem signals
	 */
	info->old_status = UART_GET_FR(info->port) & AMBA_UARTFR_MODEM_ANY;

	/*
	 * Finally, enable interrupts
	 */
	ambauart_enable_rx_interrupt(info);

	if (info->tty)
		clear_bit(TTY_IO_ERROR, &info->tty->flags);
	info->xmit.head = info->xmit.tail = 0;

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

	/*
	 * and set the speed of the serial port
	 */
	ambauart_change_speed(info, 0);

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
static void ambauart_shutdown(struct amba_info *info)
{
	unsigned long flags;

	if (!(info->flags & ASYNC_INITIALIZED))
		return;

	save_flags(flags); cli(); /* Disable interrupts */

	/*
	 * clear delta_msr_wait queue to avoid mem leaks: we may free the irq
	 * here so the queue might never be woken up
	 */
	wake_up_interruptible(&info->delta_msr_wait);

	/*
	 * Free the IRQ
	 */
	free_irq(info->port->irq, info);

	if (info->xmit.buf) {
		unsigned long pg = (unsigned long) info->xmit.buf;
		info->xmit.buf = NULL;
		free_page(pg);
	}

	/*
	 * disable all interrupts, disable the port
	 */
	UART_PUT_CR(info->port, 0);

	/* disable break condition and fifos */
	UART_PUT_LCRH(info->port, UART_GET_LCRH(info->port) &
		~(AMBA_UARTLCR_H_BRK | AMBA_UARTLCR_H_FEN));

	if (!info->tty || (info->tty->termios->c_cflag & HUPCL))
		info->mctrl &= ~(TIOCM_DTR|TIOCM_RTS);
	info->port->set_mctrl(info->port, info->mctrl);

	/* kill off our tasklet */
	tasklet_kill(&info->tlet);
	if (info->tty)
		set_bit(TTY_IO_ERROR, &info->tty->flags);

	info->flags &= ~ASYNC_INITIALIZED;
	restore_flags(flags);
}

static void ambauart_change_speed(struct amba_info *info, struct termios *old_termios)
{
	unsigned int lcr_h, baud, quot, cflag, old_cr, bits;
	unsigned long flags;

	if (!info->tty || !info->tty->termios)
		return;

	cflag = info->tty->termios->c_cflag;

#if DEBUG
	printk("ambauart_set_cflag(0x%x) called\n", cflag);
#endif
	/* byte size and parity */
	switch (cflag & CSIZE) {
	case CS5: lcr_h = AMBA_UARTLCR_H_WLEN_5; bits = 7;  break;
	case CS6: lcr_h = AMBA_UARTLCR_H_WLEN_6; bits = 8;  break;
	case CS7: lcr_h = AMBA_UARTLCR_H_WLEN_7; bits = 9;  break;
	default:  lcr_h = AMBA_UARTLCR_H_WLEN_8; bits = 10; break; // CS8
	}
	if (cflag & CSTOPB) {
		lcr_h |= AMBA_UARTLCR_H_STP2;
		bits ++;
	}
	if (cflag & PARENB) {
		lcr_h |= AMBA_UARTLCR_H_PEN;
		bits++;
		if (!(cflag & PARODD))
			lcr_h |= AMBA_UARTLCR_H_EPS;
	}
	if (info->port->fifosize > 1)
		lcr_h |= AMBA_UARTLCR_H_FEN;

	do {
		/* Determine divisor based on baud rate */
		baud = tty_get_baud_rate(info->tty);
		if (!baud)
			baud = 9600;

		if (baud == 38400 &&
		    ((info->flags & ASYNC_SPD_MASK) == ASYNC_SPD_CUST))
			quot = info->state->custom_divisor;
		else
			quot = (info->port->uartclk / (16 * baud)) - 1;

		if (!quot && old_termios) {
			info->tty->termios->c_cflag &= ~CBAUD;
			info->tty->termios->c_cflag |= (old_termios->c_cflag & CBAUD);
			old_termios = NULL;
		}
	} while (quot == 0 && old_termios);

	/* As a last resort, if the quotient is zero, default to 9600 bps */
	if (!quot)
		quot = (info->port->uartclk / (16 * 9600)) - 1;
		
	info->timeout = (info->port->fifosize * HZ * bits * quot) /
			 (info->port->uartclk / 16);
	info->timeout += HZ/50;		/* Add .02 seconds of slop */

	if (cflag & CRTSCTS)
		info->flags |= ASYNC_CTS_FLOW;
	else
		info->flags &= ~ASYNC_CTS_FLOW;
	if (cflag & CLOCAL)
		info->flags &= ~ASYNC_CHECK_CD;
	else
		info->flags |= ASYNC_CHECK_CD;

	/*
	 * Set up parity check flag
	 */
#define RELEVENT_IFLAG(iflag)	((iflag) & (IGNBRK|BRKINT|IGNPAR|PARMRK|INPCK))

	info->read_status_mask = AMBA_UARTRSR_OE;
	if (I_INPCK(info->tty))
		info->read_status_mask |= AMBA_UARTRSR_FE | AMBA_UARTRSR_PE;
	if (I_BRKINT(info->tty) || I_PARMRK(info->tty))
		info->read_status_mask |= AMBA_UARTRSR_BE;

	/*
	 * Characters to ignore
	 */
	info->ignore_status_mask = 0;
	if (I_IGNPAR(info->tty))
		info->ignore_status_mask |= AMBA_UARTRSR_FE | AMBA_UARTRSR_PE;
	if (I_IGNBRK(info->tty)) {
		info->ignore_status_mask |= AMBA_UARTRSR_BE;
		/*
		 * If we're ignoring parity and break indicators,
		 * ignore overruns to (for real raw support).
		 */
		if (I_IGNPAR(info->tty))
			info->ignore_status_mask |= AMBA_UARTRSR_OE;
	}

	/* first, disable everything */
	save_flags(flags); cli();
	old_cr = UART_GET_CR(info->port) &= ~AMBA_UARTCR_MSIE;

	if ((info->flags & ASYNC_HARDPPS_CD) ||
	    (cflag & CRTSCTS) ||
	    !(cflag & CLOCAL))
		old_cr |= AMBA_UARTCR_MSIE;

	UART_PUT_CR(info->port, 0);
	restore_flags(flags);

	/* Set baud rate */
	UART_PUT_LCRM(info->port, ((quot & 0xf00) >> 8));
	UART_PUT_LCRL(info->port, (quot & 0xff));

	/*
	 * ----------v----------v----------v----------v-----
	 * NOTE: MUST BE WRITTEN AFTER UARTLCR_M & UARTLCR_L
	 * ----------^----------^----------^----------^-----
	 */
	UART_PUT_LCRH(info->port, lcr_h);
	UART_PUT_CR(info->port, old_cr);
}

static void ambauart_put_char(struct tty_struct *tty, u_char ch)
{
	struct amba_info *info = tty->driver_data;
	unsigned long flags;

	if (!tty || !info->xmit.buf)
		return;

	save_flags(flags); cli();
	if (CIRC_SPACE(info->xmit.head, info->xmit.tail, AMBA_XMIT_SIZE) != 0) {
		info->xmit.buf[info->xmit.head] = ch;
		info->xmit.head = (info->xmit.head + 1) & (AMBA_XMIT_SIZE - 1);
	}
	restore_flags(flags);
}

static void ambauart_flush_chars(struct tty_struct *tty)
{
	struct amba_info *info = tty->driver_data;
	unsigned long flags;

	if (info->xmit.head == info->xmit.tail
	    || tty->stopped
	    || tty->hw_stopped
	    || !info->xmit.buf)
		return;

	save_flags(flags); cli();
	ambauart_enable_tx_interrupt(info);
	restore_flags(flags);
}

static int ambauart_write(struct tty_struct *tty, int from_user,
			  const u_char * buf, int count)
{
	struct amba_info *info = tty->driver_data;
	unsigned long flags;
	int c, ret = 0;

	if (!tty || !info->xmit.buf || !tmp_buf)
		return 0;

	save_flags(flags);
	if (from_user) {
		down(&tmp_buf_sem);
		while (1) {
			int c1;
			c = CIRC_SPACE_TO_END(info->xmit.head,
					      info->xmit.tail,
					      AMBA_XMIT_SIZE);
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
			c1 = CIRC_SPACE_TO_END(info->xmit.head,
					       info->xmit.tail,
					       AMBA_XMIT_SIZE);
			if (c1 < c)
				c = c1;
			memcpy(info->xmit.buf + info->xmit.head, tmp_buf, c);
			info->xmit.head = (info->xmit.head + c) &
					  (AMBA_XMIT_SIZE - 1);
			restore_flags(flags);
			buf += c;
			count -= c;
			ret += c;
		}
		up(&tmp_buf_sem);
	} else {
		cli();
		while (1) {
			c = CIRC_SPACE_TO_END(info->xmit.head,
					      info->xmit.tail,
					      AMBA_XMIT_SIZE);
			if (count < c)
				c = count;
			if (c <= 0)
				break;
			memcpy(info->xmit.buf + info->xmit.head, buf, c);
			info->xmit.head = (info->xmit.head + c) &
					  (AMBA_XMIT_SIZE - 1);
			buf += c;
			count -= c;
			ret += c;
		}
		restore_flags(flags);
	}
	if (info->xmit.head != info->xmit.tail
	    && !tty->stopped
	    && !tty->hw_stopped)
		ambauart_enable_tx_interrupt(info);
	return ret;
}

static int ambauart_write_room(struct tty_struct *tty)
{
	struct amba_info *info = tty->driver_data;

	return CIRC_SPACE(info->xmit.head, info->xmit.tail, AMBA_XMIT_SIZE);
}

static int ambauart_chars_in_buffer(struct tty_struct *tty)
{
	struct amba_info *info = tty->driver_data;

	return CIRC_CNT(info->xmit.head, info->xmit.tail, AMBA_XMIT_SIZE);
}

static void ambauart_flush_buffer(struct tty_struct *tty)
{
	struct amba_info *info = tty->driver_data;
	unsigned long flags;

#if DEBUG
	printk("ambauart_flush_buffer(%d) called\n",
	       MINOR(tty->device) - tty->driver.minor_start);
#endif
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
static void ambauart_send_xchar(struct tty_struct *tty, char ch)
{
	struct amba_info *info = tty->driver_data;

	info->x_char = ch;
	if (ch)
		ambauart_enable_tx_interrupt(info);
}

static void ambauart_throttle(struct tty_struct *tty)
{
	struct amba_info *info = tty->driver_data;
	unsigned long flags;

	if (I_IXOFF(tty))
		ambauart_send_xchar(tty, STOP_CHAR(tty));

	if (tty->termios->c_cflag & CRTSCTS) {
		save_flags(flags); cli();
		info->mctrl &= ~TIOCM_RTS;
		info->port->set_mctrl(info->port, info->mctrl);
		restore_flags(flags);
	}
}

static void ambauart_unthrottle(struct tty_struct *tty)
{
	struct amba_info *info = (struct amba_info *) tty->driver_data;
	unsigned long flags;

	if (I_IXOFF(tty)) {
		if (info->x_char)
			info->x_char = 0;
		else
			ambauart_send_xchar(tty, START_CHAR(tty));
	}

	if (tty->termios->c_cflag & CRTSCTS) {
		save_flags(flags); cli();
		info->mctrl |= TIOCM_RTS;
		info->port->set_mctrl(info->port, info->mctrl);
		restore_flags(flags);
	}
}

static int get_serial_info(struct amba_info *info, struct serial_struct *retinfo)
{
	struct amba_state *state = info->state;
	struct amba_port *port = info->port;
	struct serial_struct tmp;

	memset(&tmp, 0, sizeof(tmp));
	tmp.type	   = 0;
	tmp.line	   = state->line;
	tmp.port	   = port->uart_base;
	if (HIGH_BITS_OFFSET)
		tmp.port_high = port->uart_base >> HIGH_BITS_OFFSET;
	tmp.irq		   = port->irq;
	tmp.flags	   = 0;
	tmp.xmit_fifo_size = port->fifosize;
	tmp.baud_base	   = port->uartclk / 16;
	tmp.close_delay	   = state->close_delay;
	tmp.closing_wait   = state->closing_wait;
	tmp.custom_divisor = state->custom_divisor;

	if (copy_to_user(retinfo, &tmp, sizeof(*retinfo)))
		return -EFAULT;
	return 0;
}

static int set_serial_info(struct amba_info *info,
			   struct serial_struct *newinfo)
{
	struct serial_struct new_serial;
	struct amba_state *state, old_state;
	struct amba_port *port;
	unsigned long new_port;
	unsigned int i, change_irq, change_port;
	int retval = 0;

	if (copy_from_user(&new_serial, newinfo, sizeof(new_serial)))
		return -EFAULT;

	state = info->state;
	old_state = *state;
	port = info->port;

	new_port = new_serial.port;
	if (HIGH_BITS_OFFSET)
		new_port += (unsigned long) new_serial.port_high << HIGH_BITS_OFFSET;

	change_irq  = new_serial.irq != port->irq;
	change_port = new_port != port->uart_base;

	if (!capable(CAP_SYS_ADMIN)) {
		if (change_irq || change_port ||
		    (new_serial.baud_base != port->uartclk / 16) ||
		    (new_serial.close_delay != state->close_delay) ||
		    (new_serial.xmit_fifo_size != port->fifosize) ||
		    ((new_serial.flags & ~ASYNC_USR_MASK) !=
		     (state->flags & ~ASYNC_USR_MASK)))
			return -EPERM;
		state->flags = ((state->flags & ~ASYNC_USR_MASK) |
				(new_serial.flags & ASYNC_USR_MASK));
		info->flags = ((info->flags & ~ASYNC_USR_MASK) |
			       (new_serial.flags & ASYNC_USR_MASK));
		state->custom_divisor = new_serial.custom_divisor;
		goto check_and_exit;
	}

	if ((new_serial.irq >= NR_IRQS) || (new_serial.irq < 0) ||
	    (new_serial.baud_base < 9600))
		return -EINVAL;

	if (new_serial.type && change_port) {
		for (i = 0; i < SERIAL_AMBA_NR; i++)
			if ((port != amba_ports + i) &&
			    amba_ports[i].uart_base != new_port)
				return -EADDRINUSE;
	}

	if ((change_port || change_irq) && (state->count > 1))
		return -EBUSY;

	/*
	 * OK, past this point, all the error checking has been done.
	 * At this point, we start making changes.....
	 */
	port->uartclk = new_serial.baud_base * 16;
	state->flags = ((state->flags & ~ASYNC_FLAGS) |
			(new_serial.flags & ASYNC_FLAGS));
	info->flags = ((state->flags & ~ASYNC_INTERNAL_FLAGS) |
		       (info->flags & ASYNC_INTERNAL_FLAGS));
	state->custom_divisor = new_serial.custom_divisor;
	state->close_delay = new_serial.close_delay * HZ / 100;
	state->closing_wait = new_serial.closing_wait * HZ / 100;
	info->tty->low_latency = (info->flags & ASYNC_LOW_LATENCY) ? 1 : 0;
	port->fifosize = new_serial.xmit_fifo_size;

	if (change_port || change_irq) {
		/*
		 * We need to shutdown the serial port at the old
		 * port/irq combination.
		 */
		ambauart_shutdown(info);
		port->irq = new_serial.irq;
		port->uart_base = new_port;
	}

check_and_exit:
	if (!port->uart_base)
		return 0;
	if (info->flags & ASYNC_INITIALIZED) {
		if ((old_state.flags & ASYNC_SPD_MASK) !=
		    (state->flags & ASYNC_SPD_MASK) ||
		    (old_state.custom_divisor != state->custom_divisor)) {
			if ((state->flags & ASYNC_SPD_MASK) == ASYNC_SPD_HI)
				info->tty->alt_speed = 57600;
			if ((state->flags & ASYNC_SPD_MASK) == ASYNC_SPD_VHI)
				info->tty->alt_speed = 115200;
			if ((state->flags & ASYNC_SPD_MASK) == ASYNC_SPD_SHI)
				info->tty->alt_speed = 230400;
			if ((state->flags & ASYNC_SPD_MASK) == ASYNC_SPD_WARP)
				info->tty->alt_speed = 460800;
			ambauart_change_speed(info, NULL);
		}
	} else
		retval = ambauart_startup(info);
	return retval;
}


/*
 * get_lsr_info - get line status register info
 */
static int get_lsr_info(struct amba_info *info, unsigned int *value)
{
	unsigned int result, status;
	unsigned long flags;

	save_flags(flags); cli();
	status = UART_GET_FR(info->port);
	restore_flags(flags);
	result = status & AMBA_UARTFR_BUSY ? TIOCSER_TEMT : 0;

	/*
	 * If we're about to load something into the transmit
	 * register, we'll pretend the transmitter isn't empty to
	 * avoid a race condition (depending on when the transmit
	 * interrupt happens).
	 */
	if (info->x_char ||
	    ((CIRC_CNT(info->xmit.head, info->xmit.tail,
		       AMBA_XMIT_SIZE) > 0) &&
	     !info->tty->stopped && !info->tty->hw_stopped))
		result &= TIOCSER_TEMT;
	
	return put_user(result, value);
}

static int get_modem_info(struct amba_info *info, unsigned int *value)
{
	unsigned int result = info->mctrl;
	unsigned int status;

	status = UART_GET_FR(info->port);
	if (status & AMBA_UARTFR_DCD)
		result |= TIOCM_CAR;
	if (status & AMBA_UARTFR_DSR)
		result |= TIOCM_DSR;
	if (status & AMBA_UARTFR_CTS)
		result |= TIOCM_CTS;

	return put_user(result, value);
}

static int set_modem_info(struct amba_info *info, unsigned int cmd,
			  unsigned int *value)
{
	unsigned int arg, old;
	unsigned long flags;

	if (get_user(arg, value))
		return -EFAULT;

	old = info->mctrl;
	switch (cmd) {
	case TIOCMBIS:
		info->mctrl |= arg;
		break;

	case TIOCMBIC:
		info->mctrl &= ~arg;
		break;

	case TIOCMSET:
		info->mctrl = arg;
		break;

	default:
		return -EINVAL;
	}
	save_flags(flags); cli();
	if (old != info->mctrl)
		info->port->set_mctrl(info->port, info->mctrl);
	restore_flags(flags);
	return 0;
}

static void ambauart_break_ctl(struct tty_struct *tty, int break_state)
{
	struct amba_info *info = tty->driver_data;
	unsigned long flags;
	unsigned int lcr_h;

	save_flags(flags); cli();
	lcr_h = UART_GET_LCRH(info->port);
	if (break_state == -1)
		lcr_h |= AMBA_UARTLCR_H_BRK;
	else
		lcr_h &= ~AMBA_UARTLCR_H_BRK;
	UART_PUT_LCRH(info->port, lcr_h);
	restore_flags(flags);
}

static int ambauart_ioctl(struct tty_struct *tty, struct file *file,
			   unsigned int cmd, unsigned long arg)
{
	struct amba_info *info = tty->driver_data;
	struct amba_icount cprev, cnow;
	struct serial_icounter_struct icount;
	unsigned long flags;

	if ((cmd != TIOCGSERIAL) && (cmd != TIOCSSERIAL) &&
	    (cmd != TIOCSERCONFIG) && (cmd != TIOCSERGSTRUCT) &&
	    (cmd != TIOCMIWAIT) && (cmd != TIOCGICOUNT)) {
		if (tty->flags & (1 << TTY_IO_ERROR))
			return -EIO;
	}

	switch (cmd) {
		case TIOCMGET:
			return get_modem_info(info, (unsigned int *)arg);
		case TIOCMBIS:
		case TIOCMBIC:
		case TIOCMSET:
			return set_modem_info(info, cmd, (unsigned int *)arg);
		case TIOCGSERIAL:
			return get_serial_info(info,
					       (struct serial_struct *)arg);
		case TIOCSSERIAL:
			return set_serial_info(info,
					       (struct serial_struct *)arg);
		case TIOCSERGETLSR: /* Get line status register */
			return get_lsr_info(info, (unsigned int *)arg);
		/*
		 * Wait for any of the 4 modem inputs (DCD,RI,DSR,CTS) to change
		 * - mask passed in arg for lines of interest
		 *   (use |'ed TIOCM_RNG/DSR/CD/CTS for masking)
		 * Caller should use TIOCGICOUNT to see which one it was
		 */
		case TIOCMIWAIT:
			save_flags(flags); cli();
			/* note the counters on entry */
			cprev = info->state->icount;
			/* Force modem status interrupts on */
			UART_PUT_CR(info->port, UART_GET_CR(info->port) | AMBA_UARTCR_MSIE);
			restore_flags(flags);
			while (1) {
				interruptible_sleep_on(&info->delta_msr_wait);
				/* see if a signal did it */
				if (signal_pending(current))
					return -ERESTARTSYS;
				save_flags(flags); cli();
				cnow = info->state->icount; /* atomic copy */
				restore_flags(flags);
				if (cnow.rng == cprev.rng && cnow.dsr == cprev.dsr &&
				    cnow.dcd == cprev.dcd && cnow.cts == cprev.cts)
					return -EIO; /* no change => error */
				if (((arg & TIOCM_RNG) && (cnow.rng != cprev.rng)) ||
				    ((arg & TIOCM_DSR) && (cnow.dsr != cprev.dsr)) ||
				    ((arg & TIOCM_CD)  && (cnow.dcd != cprev.dcd)) ||
				    ((arg & TIOCM_CTS) && (cnow.cts != cprev.cts))) {
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
			icount.cts = cnow.cts;
			icount.dsr = cnow.dsr;
			icount.rng = cnow.rng;
			icount.dcd = cnow.dcd;
			icount.rx  = cnow.rx;
			icount.tx  = cnow.tx;
			icount.frame = cnow.frame;
			icount.overrun = cnow.overrun;
			icount.parity = cnow.parity;
			icount.brk = cnow.brk;
			icount.buf_overrun = cnow.buf_overrun;

			return copy_to_user((void *)arg, &icount, sizeof(icount))
					? -EFAULT : 0;

		default:
			return -ENOIOCTLCMD;
	}
	return 0;
}

static void ambauart_set_termios(struct tty_struct *tty, struct termios *old_termios)
{
	struct amba_info *info = tty->driver_data;
	unsigned long flags;
	unsigned int cflag = tty->termios->c_cflag;

	if ((cflag ^ old_termios->c_cflag) == 0 &&
	    RELEVENT_IFLAG(tty->termios->c_iflag ^ old_termios->c_iflag) == 0)
		return;

	ambauart_change_speed(info, old_termios);

	/* Handle transition to B0 status */
	if ((old_termios->c_cflag & CBAUD) &&
	    !(cflag & CBAUD)) {
		save_flags(flags); cli();
		info->mctrl &= ~(TIOCM_RTS | TIOCM_DTR);
		info->port->set_mctrl(info->port, info->mctrl);
		restore_flags(flags);
	}

	/* Handle transition away from B0 status */
	if (!(old_termios->c_cflag & CBAUD) &&
	    (cflag & CBAUD)) {
		save_flags(flags); cli();
		info->mctrl |= TIOCM_DTR;
		if (!(cflag & CRTSCTS) ||
		    !test_bit(TTY_THROTTLED, &tty->flags))
			info->mctrl |= TIOCM_RTS;
		info->port->set_mctrl(info->port, info->mctrl);
		restore_flags(flags);
	}

	/* Handle turning off CRTSCTS */
	if ((old_termios->c_cflag & CRTSCTS) &&
	    !(cflag & CRTSCTS)) {
		tty->hw_stopped = 0;
		ambauart_start(tty);
	}

#if 0
	/*
	 * No need to wake up processes in open wait, since they
	 * sample the CLOCAL flag once, and don't recheck it.
	 * XXX  It's not clear whether the current behavior is correct
	 * or not.  Hence, this may change.....
	 */
	if (!(old_termios->c_cflag & CLOCAL) &&
	    (tty->termios->c_cflag & CLOCAL))
		wake_up_interruptible(&info->open_wait);
#endif
}

static void ambauart_close(struct tty_struct *tty, struct file *filp)
{
	struct amba_info *info = tty->driver_data;
	struct amba_state *state;
	unsigned long flags;

	if (!info)
		return;

	state = info->state;

#if DEBUG
	printk("ambauart_close() called\n");
#endif

	save_flags(flags); cli();

	if (tty_hung_up_p(filp)) {
		MOD_DEC_USE_COUNT;
		restore_flags(flags);
		return;
	}

	if ((tty->count == 1) && (state->count != 1)) {
		/*
		 * Uh, oh.  tty->count is 1, which means that the tty
		 * structure will be freed.  state->count should always
		 * be one in these conditions.  If it's greater than
		 * one, we've got real problems, since it means the
		 * serial port won't be shutdown.
		 */
		printk("ambauart_close: bad serial port count; tty->count is 1, "
		       "state->count is %d\n", state->count);
		state->count = 1;
	}
	if (--state->count < 0) {
		printk("rs_close: bad serial port count for %s%d: %d\n",
		       tty->driver.name, info->state->line, state->count);
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
	if (info->state->closing_wait != ASYNC_CLOSING_WAIT_NONE)
		tty_wait_until_sent(tty, info->state->closing_wait);
	/*
	 * At this point, we stop accepting input.  To do this, we
	 * disable the receive line status interrupts.
	 */
	if (info->flags & ASYNC_INITIALIZED) {
		ambauart_disable_rx_interrupt(info);
		/*
		 * Before we drop DTR, make sure the UART transmitter
		 * has completely drained; this is especially
		 * important if there is a transmit FIFO!
		 */
		ambauart_wait_until_sent(tty, info->timeout);
	}
	ambauart_shutdown(info);
	if (tty->driver.flush_buffer)
		tty->driver.flush_buffer(tty);
	if (tty->ldisc.flush_buffer)
		tty->ldisc.flush_buffer(tty);
	tty->closing = 0;
	info->event = 0;
	info->tty = NULL;
	if (info->blocked_open) {
		if (info->state->close_delay) {
			set_current_state(TASK_INTERRUPTIBLE);
			schedule_timeout(info->state->close_delay);
		}
		wake_up_interruptible(&info->open_wait);
	}
	info->flags &= ~(ASYNC_NORMAL_ACTIVE|ASYNC_CALLOUT_ACTIVE|
			 ASYNC_CLOSING);
	wake_up_interruptible(&info->close_wait);
	MOD_DEC_USE_COUNT;
}

static void ambauart_wait_until_sent(struct tty_struct *tty, int timeout)
{
	struct amba_info *info = (struct amba_info *) tty->driver_data;
	unsigned long char_time, expire;
	unsigned int status;

	if (info->port->fifosize == 0)
		return;

	/*
	 * Set the check interval to be 1/5 of the estimated time to
	 * send a single character, and make it at least 1.  The check
	 * interval should also be less than the timeout.
	 *
	 * Note: we have to use pretty tight timings here to satisfy
	 * the NIST-PCTS.
	 */
	char_time = (info->timeout - HZ/50) / info->port->fifosize;
	char_time = char_time / 5;
	if (char_time == 0)
		char_time = 1;
	if (timeout && timeout < char_time)
		char_time = timeout;
	/*
	 * If the transmitter hasn't cleared in twice the approximate
	 * amount of time to send the entire FIFO, it probably won't
	 * ever clear.  This assumes the UART isn't doing flow
	 * control, which is currently the case.  Hence, if it ever
	 * takes longer than info->timeout, this is probably due to a
	 * UART bug of some kind.  So, we clamp the timeout parameter at
	 * 2*info->timeout.
	 */
	if (!timeout || timeout > 2 * info->timeout)
		timeout = 2 * info->timeout;

	expire = jiffies + timeout;
#if DEBUG
	printk("ambauart_wait_until_sent(%d), jiff=%lu, expire=%lu...\n",
	       MINOR(tty->device) - tty->driver.minor_start, jiffies,
	       expire);
#endif
	while (UART_GET_FR(info->port) & AMBA_UARTFR_BUSY) {
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(char_time);
		if (signal_pending(current))
			break;
		if (timeout && time_after(jiffies, expire))
			break;
		status = UART_GET_FR(info->port);
	}
	set_current_state(TASK_RUNNING);
}

static void ambauart_hangup(struct tty_struct *tty)
{
	struct amba_info *info = tty->driver_data;
	struct amba_state *state = info->state;

	ambauart_flush_buffer(tty);
	if (info->flags & ASYNC_CLOSING)
		return;
	ambauart_shutdown(info);
	info->event = 0;
	state->count = 0;
	info->flags &= ~(ASYNC_NORMAL_ACTIVE|ASYNC_CALLOUT_ACTIVE);
	info->tty = NULL;
	wake_up_interruptible(&info->open_wait);
}

static int block_til_ready(struct tty_struct *tty, struct file *filp,
			   struct amba_info *info)
{
	DECLARE_WAITQUEUE(wait, current);
	struct amba_state *state = info->state;
	unsigned long flags;
	int do_clocal = 0, extra_count = 0, retval;

	/*
	 * If the device is in the middle of being closed, then block
	 * until it's done, and then try again.
	 */
	if (tty_hung_up_p(filp) ||
	    (info->flags & ASYNC_CLOSING)) {
		if (info->flags & ASYNC_CLOSING)
			interruptible_sleep_on(&info->close_wait);
		return (info->flags & ASYNC_HUP_NOTIFY) ?
			-EAGAIN : -ERESTARTSYS;
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
	save_flags(flags); cli();
	if (!tty_hung_up_p(filp)) {
		extra_count = 1;
		state->count--;
	}
	restore_flags(flags);
	info->blocked_open++;
	while (1) {
		save_flags(flags); cli();
		if (!(info->flags & ASYNC_CALLOUT_ACTIVE) &&
		    (tty->termios->c_cflag & CBAUD)) {
			info->mctrl = TIOCM_DTR | TIOCM_RTS;
			info->port->set_mctrl(info->port, info->mctrl);
		}
		restore_flags(flags);
		set_current_state(TASK_INTERRUPTIBLE);
		if (tty_hung_up_p(filp) ||
		    !(info->flags & ASYNC_INITIALIZED)) {
			if (info->flags & ASYNC_HUP_NOTIFY)
				retval = -EAGAIN;
			else
				retval = -ERESTARTSYS;
			break;
		}
		if (!(info->flags & ASYNC_CALLOUT_ACTIVE) &&
		    !(info->flags & ASYNC_CLOSING) &&
		    (do_clocal || (UART_GET_FR(info->port) & AMBA_UARTFR_DCD)))
			break;
		if (signal_pending(current)) {
			retval = -ERESTARTSYS;
			break;
		}
		schedule();
	}
	set_current_state(TASK_RUNNING);
	remove_wait_queue(&info->open_wait, &wait);
	if (extra_count)
		state->count++;
	info->blocked_open--;
	if (retval)
		return retval;
	info->flags |= ASYNC_NORMAL_ACTIVE;
	return 0;
}

static struct amba_info *ambauart_get(int line)
{
	struct amba_info *info;
	struct amba_state *state = amba_state + line;

	state->count++;
	if (state->info)
		return state->info;
	info = kmalloc(sizeof(struct amba_info), GFP_KERNEL);
	if (info) {
		memset(info, 0, sizeof(struct amba_info));
		init_waitqueue_head(&info->open_wait);
		init_waitqueue_head(&info->close_wait);
		init_waitqueue_head(&info->delta_msr_wait);
		info->flags = state->flags;
		info->state = state;
		info->port  = amba_ports + line;
		tasklet_init(&info->tlet, ambauart_tasklet_action,
			     (unsigned long)info);
	}
	if (state->info) {
		kfree(info);
		return state->info;
	}
	state->info = info;
	return info;
}

static int ambauart_open(struct tty_struct *tty, struct file *filp)
{
	struct amba_info *info;
	int retval, line = MINOR(tty->device) - tty->driver.minor_start;

#if DEBUG
	printk("ambauart_open(%d) called\n", line);
#endif

	// is this a line that we've got?
	MOD_INC_USE_COUNT;
	if (line >= SERIAL_AMBA_NR) {
		MOD_DEC_USE_COUNT;
		return -ENODEV;
	}

	info = ambauart_get(line);
	if (!info)
		return -ENOMEM;

	tty->driver_data = info;
	info->tty = tty;
	info->tty->low_latency = (info->flags & ASYNC_LOW_LATENCY) ? 1 : 0;

	/*
	 * Make sure we have the temporary buffer allocated
	 */
	if (!tmp_buf) {
		unsigned long page = get_zeroed_page(GFP_KERNEL);
		if (tmp_buf)
			free_page(page);
		else if (!page) {
			MOD_DEC_USE_COUNT;
			return -ENOMEM;
		}
		tmp_buf = (u_char *)page;
	}

	/*
	 * If the port is in the middle of closing, bail out now.
	 */
	if (tty_hung_up_p(filp) ||
	    (info->flags & ASYNC_CLOSING)) {
		if (info->flags & ASYNC_CLOSING)
			interruptible_sleep_on(&info->close_wait);
		MOD_DEC_USE_COUNT;
		return -EAGAIN;
	}

	/*
	 * Start up the serial port
	 */
	retval = ambauart_startup(info);
	if (retval) {
		MOD_DEC_USE_COUNT;
		return retval;
	}

	retval = block_til_ready(tty, filp, info);
	if (retval) {
		MOD_DEC_USE_COUNT;
		return retval;
	}

	if ((info->state->count == 1) &&
	    (info->flags & ASYNC_SPLIT_TERMIOS)) {
		if (tty->driver.subtype == SERIAL_TYPE_NORMAL)
			*tty->termios = info->state->normal_termios;
		else
			*tty->termios = info->state->callout_termios;
	}
#ifdef CONFIG_SERIAL_AMBA_CONSOLE
	if (ambauart_cons.cflag && ambauart_cons.index == line) {
		tty->termios->c_cflag = ambauart_cons.cflag;
		ambauart_cons.cflag = 0;
	}
#endif
	ambauart_change_speed(info, NULL);
	info->session = current->session;
	info->pgrp = current->pgrp;
	return 0;
}

int __init ambauart_init(void)
{
	int i;

	ambanormal_driver.magic = TTY_DRIVER_MAGIC;
	ambanormal_driver.driver_name = "serial_amba";
	ambanormal_driver.name = SERIAL_AMBA_NAME;
	ambanormal_driver.major = SERIAL_AMBA_MAJOR;
	ambanormal_driver.minor_start = SERIAL_AMBA_MINOR;
	ambanormal_driver.num = SERIAL_AMBA_NR;
	ambanormal_driver.type = TTY_DRIVER_TYPE_SERIAL;
	ambanormal_driver.subtype = SERIAL_TYPE_NORMAL;
	ambanormal_driver.init_termios = tty_std_termios;
	ambanormal_driver.init_termios.c_cflag = B38400 | CS8 | CREAD | HUPCL | CLOCAL;
	ambanormal_driver.flags = TTY_DRIVER_REAL_RAW | TTY_DRIVER_NO_DEVFS;
	ambanormal_driver.refcount = &ambauart_refcount;
	ambanormal_driver.table = ambauart_table;
	ambanormal_driver.termios = ambauart_termios;
	ambanormal_driver.termios_locked = ambauart_termios_locked;

	ambanormal_driver.open = ambauart_open;
	ambanormal_driver.close = ambauart_close;
	ambanormal_driver.write = ambauart_write;
	ambanormal_driver.put_char = ambauart_put_char;
	ambanormal_driver.flush_chars = ambauart_flush_chars;
	ambanormal_driver.write_room = ambauart_write_room;
	ambanormal_driver.chars_in_buffer = ambauart_chars_in_buffer;
	ambanormal_driver.flush_buffer	= ambauart_flush_buffer;
	ambanormal_driver.ioctl = ambauart_ioctl;
	ambanormal_driver.throttle = ambauart_throttle;
	ambanormal_driver.unthrottle = ambauart_unthrottle;
	ambanormal_driver.send_xchar = ambauart_send_xchar;
	ambanormal_driver.set_termios = ambauart_set_termios;
	ambanormal_driver.stop = ambauart_stop;
	ambanormal_driver.start = ambauart_start;
	ambanormal_driver.hangup = ambauart_hangup;
	ambanormal_driver.break_ctl = ambauart_break_ctl;
	ambanormal_driver.wait_until_sent = ambauart_wait_until_sent;
	ambanormal_driver.read_proc = NULL;

	/*
	 * The callout device is just like the normal device except for
	 * the major number and the subtype code.
	 */
	ambacallout_driver = ambanormal_driver;
	ambacallout_driver.name = CALLOUT_AMBA_NAME;
	ambacallout_driver.major = CALLOUT_AMBA_MAJOR;
	ambacallout_driver.subtype = SERIAL_TYPE_CALLOUT;
	ambacallout_driver.read_proc = NULL;
	ambacallout_driver.proc_entry = NULL;

	if (tty_register_driver(&ambanormal_driver))
		panic("Couldn't register AMBA serial driver\n");
	if (tty_register_driver(&ambacallout_driver))
		panic("Couldn't register AMBA callout driver\n");

	for (i = 0; i < SERIAL_AMBA_NR; i++) {
		struct amba_state *state = amba_state + i;
		state->line		= i;
		state->close_delay	= 5 * HZ / 10;
		state->closing_wait	= 30 * HZ;
		state->callout_termios	= ambacallout_driver.init_termios;
		state->normal_termios	= ambanormal_driver.init_termios;
	}

	return 0;
}

__initcall(ambauart_init);

#ifdef CONFIG_SERIAL_AMBA_CONSOLE
/************** console driver *****************/

/*
 * This code is currently never used; console->read is never called.
 * Therefore, although we have an implementation, we don't use it.
 * FIXME: the "const char *s" should be fixed to "char *s" some day.
 * (when the definition in include/linux/console.h is also fixed)
 */
#ifdef used_and_not_const_char_pointer
static int ambauart_console_read(struct console *co, const char *s, u_int count)
{
	struct amba_port *port = &amba_ports[co->index];
	unsigned int status;
	char *w;
	int c;
#if DEBUG
	printk("ambauart_console_read() called\n");
#endif

	c = 0;
	w = s;
	while (c < count) {
		status = UART_GET_FR(port);
		if (UART_RX_DATA(status)) {
			*w++ = UART_GET_CHAR(port);
			c++;
		} else {
			// nothing more to get, return
			return c;
		}
	}
	// return the count
	return c;
}
#endif

/*
 *	Print a string to the serial port trying not to disturb
 *	any possible real use of the port...
 *
 *	The console must be locked when we get here.
 */
static void ambauart_console_write(struct console *co, const char *s, u_int count)
{
	struct amba_port *port = &amba_ports[co->index];
	unsigned int status, old_cr;
	int i;

	/*
	 *	First save the CR then disable the interrupts
	 */
	old_cr = UART_GET_CR(port);
	UART_PUT_CR(port, AMBA_UARTCR_UARTEN);

	/*
	 *	Now, do each character
	 */
	for (i = 0; i < count; i++) {
		do {
			status = UART_GET_FR(port);
		} while (!UART_TX_READY(status));
		UART_PUT_CHAR(port, s[i]);
		if (s[i] == '\n') {
			do {
				status = UART_GET_FR(port);
			} while (!UART_TX_READY(status));
			UART_PUT_CHAR(port, '\r');
		}
	}

	/*
	 *	Finally, wait for transmitter to become empty
	 *	and restore the TCR
	 */
	do {
		status = UART_GET_FR(port);
	} while (status & AMBA_UARTFR_BUSY);
	UART_PUT_CR(port, old_cr);
}

static kdev_t ambauart_console_device(struct console *c)
{
	return MKDEV(SERIAL_AMBA_MAJOR, SERIAL_AMBA_MINOR + c->index);
}

static int __init ambauart_console_setup(struct console *co, char *options)
{
	struct amba_port *port;
	int baud = 38400;
	int bits = 8;
	int parity = 'n';
	u_int cflag = CREAD | HUPCL | CLOCAL;
	u_int lcr_h, quot;

	if (co->index >= SERIAL_AMBA_NR)
		co->index = 0;

	port = &amba_ports[co->index];

	if (options) {
		char *s = options;
		baud = simple_strtoul(s, NULL, 10);
		while (*s >= '0' && *s <= '9')
			s++;
		if (*s) parity = *s++;
		if (*s) bits = *s - '0';
	}

	/*
	 *    Now construct a cflag setting.
	 */
	switch (baud) {
	case 1200:	cflag |= B1200;			break;
	case 2400:	cflag |= B2400;			break;
	case 4800:	cflag |= B4800;			break;
	default:	cflag |= B9600;   baud = 9600;	break;
	case 19200:	cflag |= B19200;		break;
	case 38400:	cflag |= B38400;		break;
	case 57600:	cflag |= B57600;		break;
	case 115200:	cflag |= B115200;		break;
	}
	switch (bits) {
	case 7:	  cflag |= CS7;	lcr_h = AMBA_UARTLCR_H_WLEN_7;	break;
	default:  cflag |= CS8;	lcr_h = AMBA_UARTLCR_H_WLEN_8;	break;
	}
	switch (parity) {
	case 'o':
	case 'O': cflag |= PARODD; lcr_h |= AMBA_UARTLCR_H_PEN;	break;
	case 'e':
	case 'E': cflag |= PARENB; lcr_h |= AMBA_UARTLCR_H_PEN |
					    AMBA_UARTLCR_H_EPS; break;
	}

	co->cflag = cflag;

	if (port->fifosize > 1)
		lcr_h |= AMBA_UARTLCR_H_FEN;

	quot = (port->uartclk / (16 * baud)) - 1;

	UART_PUT_LCRL(port, (quot & 0xff));
	UART_PUT_LCRM(port, (quot >> 8));
	UART_PUT_LCRH(port, lcr_h);

	/* we will enable the port as we need it */
	UART_PUT_CR(port, 0);

	return 0;
}

static struct console ambauart_cons =
{
	name:		SERIAL_AMBA_NAME,
	write:		ambauart_console_write,
#ifdef used_and_not_const_char_pointer
	read:		ambauart_console_read,
#endif
	device:		ambauart_console_device,
	setup:		ambauart_console_setup,
	flags:		CON_PRINTBUFFER,
	index:		-1,
};

void __init ambauart_console_init(void)
{
	register_console(&ambauart_cons);
}

#endif /* CONFIG_SERIAL_AMBA_CONSOLE */

MODULE_LICENSE("GPL");
EXPORT_NO_SYMBOLS;
