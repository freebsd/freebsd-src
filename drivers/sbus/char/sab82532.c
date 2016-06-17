/* $Id: sab82532.c,v 1.65 2001/10/13 08:27:50 davem Exp $
 * sab82532.c: ASYNC Driver for the SIEMENS SAB82532 DUSCC.
 *
 * Copyright (C) 1997  Eddie C. Dost  (ecd@skynet.be)
 *
 * Rewrote buffer handling to use CIRC(Circular Buffer) macros.
 *   Maxim Krasnyanskiy <maxk@qualcomm.com>
 *
 * Fixed to use tty_get_baud_rate, and to allow for arbitrary baud
 * rates to be programmed into the UART.  Also eliminated a lot of
 * duplicated code in the console setup.
 *   Theodore Ts'o <tytso@mit.edu>, 2001-Oct-12
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
#include <linux/serialP.h>
#include <linux/serial_reg.h>
#include <linux/console.h>
#include <linux/major.h>
#include <linux/string.h>
#include <linux/fcntl.h>
#include <linux/ptrace.h>
#include <linux/ioport.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/delay.h>

#include <asm/sab82532.h>
#include <asm/uaccess.h>
#include <asm/ebus.h>
#include <asm/irq.h>

#include "sunserial.h"

static DECLARE_TASK_QUEUE(tq_serial);

/* This is (one of many) a special gross hack to allow SU and
 * SAB serials to co-exist on the same machine. -DaveM
 */
#undef SERIAL_BH
#define SERIAL_BH	AURORA_BH

static struct tty_driver serial_driver, callout_driver;
static int sab82532_refcount;

/* number of characters left in xmit buffer before we ask for more */
#define WAKEUP_CHARS 256

#undef SERIAL_PARANOIA_CHECK
#define SERIAL_DO_RESTART

/* Set of debugging defines */
#undef SERIAL_DEBUG_OPEN
#undef SERIAL_DEBUG_FLOW
#undef SERIAL_DEBUG_MODEM
#undef SERIAL_DEBUG_WAIT_UNTIL_SENT
#undef SERIAL_DEBUG_SEND_BREAK
#undef SERIAL_DEBUG_INTR
#undef SERIAL_DEBUG_FIFO
#define SERIAL_DEBUG_OVERFLOW 1

/* Trace things on serial device, useful for console debugging: */
#undef SERIAL_LOG_DEVICE

#ifdef SERIAL_LOG_DEVICE
static void dprint_init(int tty);
#endif

static void change_speed(struct sab82532 *info);
static void sab82532_wait_until_sent(struct tty_struct *tty, int timeout);

/*
 * This assumes you have a 29.4912 MHz clock for your UART.
 */
#define BASE_BAUD ( 29491200 / 16 )

static struct sab82532 *sab82532_chain = 0;
static struct tty_struct *sab82532_table[NR_PORTS];
static struct termios *sab82532_termios[NR_PORTS];
static struct termios *sab82532_termios_locked[NR_PORTS];

#ifdef MODULE
#undef CONFIG_SERIAL_CONSOLE
#endif

#ifdef CONFIG_SERIAL_CONSOLE
extern int serial_console;
static struct console sab82532_console;
static int sab82532_console_init(void);
static void batten_down_hatches(struct sab82532 *info);
#endif

#ifndef MIN
#define MIN(a,b)	((a) < (b) ? (a) : (b))
#endif

static char *sab82532_version[16] = {
	"V1.0", "V2.0", "V3.2", "V(0x03)",
	"V(0x04)", "V(0x05)", "V(0x06)", "V(0x07)",
	"V(0x08)", "V(0x09)", "V(0x0a)", "V(0x0b)",
	"V(0x0c)", "V(0x0d)", "V(0x0e)", "V(0x0f)"
};
static char serial_version[16];

/*
 * tmp_buf is used as a temporary buffer by sab82532_write.  We need to
 * lock it in case the copy_from_user blocks while swapping in a page,
 * and some other program tries to do a serial write at the same time.
 * Since the lock will only come under contention when the system is
 * swapping and available memory is low, it makes sense to share one
 * buffer across all the serial ports, since it significantly saves
 * memory if large numbers of serial ports are open.
 */
static unsigned char *tmp_buf = 0;
static DECLARE_MUTEX(tmp_buf_sem);

static inline int serial_paranoia_check(struct sab82532 *info,
					kdev_t device, const char *routine)
{
#ifdef SERIAL_PARANOIA_CHECK
	static const char *badmagic =
		"Warning: bad magic number for serial struct (%s) in %s\n";
	static const char *badinfo =
		"Warning: null sab82532 for (%s) in %s\n";

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
 * This is used to figure out the divisor speeds.
 *
 * The formula is:    Baud = BASE_BAUD / ((N + 1) * (1 << M)),
 *
 * with               0 <= N < 64 and 0 <= M < 16
 * 
 * 12-Oct-2001 - Replaced table driven approach with code written by
 * Theodore Ts'o <tytso@alum.mit.edu> which exactly replicates the
 * table.  (Modulo bugs for the 307200 and 61440 baud rates, which
 * were clearly incorrectly calculated in the original table.  This is
 * why tables filled with magic constants are evil.)
 */

static void calc_ebrg(int baud, int *n_ret, int *m_ret)
{
	int	n, m;

	if (baud == 0) {
		*n_ret = 0;
		*m_ret = 0;
		return;
	}
     
	/*
	 * We scale numbers by 10 so that we get better accuracy
	 * without having to use floating point.  Here we increment m
	 * until n is within the valid range.
	 */
	n = (BASE_BAUD*10) / baud;
	m = 0;
	while (n >= 640) {
		n = n / 2;
		m++;
	}
	n = (n+5) / 10;
	/*
	 * We try very hard to avoid speeds with M == 0 since they may
	 * not work correctly for XTAL frequences above 10 MHz.
	 */
	if ((m == 0) && ((n & 1) == 0)) {
		n = n / 2;
		m++;
	}
	*n_ret = n - 1;
	*m_ret = m;
}

#define SAB82532_MAX_TEC_TIMEOUT 200000	/* 1 character time (at 50 baud) */
#define SAB82532_MAX_CEC_TIMEOUT  50000	/* 2.5 TX CLKs (at 50 baud) */

static __inline__ void sab82532_tec_wait(struct sab82532 *info)
{
	int timeout = info->tec_timeout;

	while ((readb(&info->regs->r.star) & SAB82532_STAR_TEC) && --timeout)
		udelay(1);
}

static __inline__ void sab82532_cec_wait(struct sab82532 *info)
{
	int timeout = info->cec_timeout;

	while ((readb(&info->regs->r.star) & SAB82532_STAR_CEC) && --timeout)
		udelay(1);
}

static __inline__ void sab82532_start_tx(struct sab82532 *info)
{
	unsigned long flags;
	int i;

	save_flags(flags); cli();

	if (info->xmit.head == info->xmit.tail)
		goto out;

	if (!test_bit(SAB82532_XPR, &info->irqflags))
		goto out;

	info->interrupt_mask1 &= ~(SAB82532_IMR1_ALLS);
	writeb(info->interrupt_mask1, &info->regs->w.imr1);
	clear_bit(SAB82532_ALLS, &info->irqflags);

	clear_bit(SAB82532_XPR, &info->irqflags);
	for (i = 0; i < info->xmit_fifo_size; i++) {
		writeb(info->xmit.buf[info->xmit.tail],
		       &info->regs->w.xfifo[i]);
		info->xmit.tail = (info->xmit.tail + 1) & (SERIAL_XMIT_SIZE-1);
		info->icount.tx++;
		if (info->xmit.head == info->xmit.tail)
			break;
	}

	/* Issue a Transmit Frame command. */
	sab82532_cec_wait(info);
	writeb(SAB82532_CMDR_XF, &info->regs->w.cmdr);

out:
	restore_flags(flags);
}


/*
 * ------------------------------------------------------------
 * sab82532_stop() and sab82532_start()
 *
 * This routines are called before setting or resetting tty->stopped.
 * They enable or disable transmitter interrupts, as necessary.
 * ------------------------------------------------------------
 */
static void sab82532_stop(struct tty_struct *tty)
{
	struct sab82532 *info = (struct sab82532 *)tty->driver_data;
	unsigned long flags;

	if (serial_paranoia_check(info, tty->device, "sab82532_stop"))
		return;

	save_flags(flags); cli();
	info->interrupt_mask1 |= SAB82532_IMR1_XPR;
	writeb(info->interrupt_mask1, &info->regs->w.imr1);
	restore_flags(flags);
}

static void sab82532_start(struct tty_struct *tty)
{
	struct sab82532 *info = (struct sab82532 *)tty->driver_data;
	unsigned long flags;
	
	if (serial_paranoia_check(info, tty->device, "sab82532_start"))
		return;

	save_flags(flags); cli();
	info->interrupt_mask1 &= ~(SAB82532_IMR1_XPR);
	writeb(info->interrupt_mask1, &info->regs->w.imr1);
	sab82532_start_tx(info);
	restore_flags(flags);
}

/*
 * ----------------------------------------------------------------------
 *
 * Here starts the interrupt handling routines.  All of the following
 * subroutines are declared as inline and are folded into
 * sab82532_interrupt().  They were separated out for readability's sake.
 *
 * Note: sab82532_interrupt() is a "fast" interrupt, which means that it
 * runs with interrupts turned off.  People who may want to modify
 * sab82532_interrupt() should try to keep the interrupt handler as fast as
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
static void sab82532_sched_event(struct sab82532 *info, int event)
{
	info->event |= 1 << event;
	queue_task(&info->tqueue, &tq_serial);
	mark_bh(SERIAL_BH);
}

static void receive_chars(struct sab82532 *info,
			  union sab82532_irq_status *stat)
{
	struct tty_struct *tty = info->tty;
	unsigned char buf[32];
	unsigned char status;
	int free_fifo = 0;
	int i, count = 0;

	/* Read number of BYTES (Character + Status) available. */
	if (stat->sreg.isr0 & SAB82532_ISR0_RPF) {
		count = info->recv_fifo_size;
		free_fifo++;
	}

	if (stat->sreg.isr0 & SAB82532_ISR0_TCD) {
		count = readb(&info->regs->r.rbcl) & (info->recv_fifo_size - 1);
		free_fifo++;
	}

	/* Issue a FIFO read command in case we where idle. */
	if (stat->sreg.isr0 & SAB82532_ISR0_TIME) {
		sab82532_cec_wait(info);
		writeb(SAB82532_CMDR_RFRD, &info->regs->w.cmdr);
		return;
	}

	if (stat->sreg.isr0 & SAB82532_ISR0_RFO) {
#ifdef SERIAL_DEBUG_OVERFLOW
		printk("sab82532: receive_chars: RFO");
#endif
		free_fifo++;
	}

	/* Read the FIFO. */
	for (i = 0; i < count; i++)
		buf[i] = readb(&info->regs->r.rfifo[i]);

	/* Issue Receive Message Complete command. */
	if (free_fifo) {
		sab82532_cec_wait(info);
		writeb(SAB82532_CMDR_RMC, &info->regs->w.cmdr);
	}

	if (!tty)
		return;

	for (i = 0; i < count; ) {
		if (tty->flip.count >= TTY_FLIPBUF_SIZE) {
#ifdef SERIAL_DEBUG_OVERFLOW
			printk("sab82532: receive_chars: tty overrun\n");
#endif
			info->icount.buf_overrun++;
			break;
		}

		tty->flip.count++;
		*tty->flip.char_buf_ptr++ = buf[i++];
		status = buf[i++];
		info->icount.rx++;

#ifdef SERIAL_DEBUG_INTR
                printk("DR%02x:%02x...", (unsigned char)*(tty->flip.char_buf_ptr - 1), status);
#endif

		if (status & SAB82532_RSTAT_PE) {
			*tty->flip.flag_buf_ptr++ = TTY_PARITY;
			info->icount.parity++;
		} else if (status & SAB82532_RSTAT_FE) {
			*tty->flip.flag_buf_ptr++ = TTY_FRAME;
			info->icount.frame++;
		}
		else
			*tty->flip.flag_buf_ptr++ = TTY_NORMAL;
	}

	queue_task(&tty->flip.tqueue, &tq_timer);
}

static void transmit_chars(struct sab82532 *info,
			   union sab82532_irq_status *stat)
{
	int i;

	if (stat->sreg.isr1 & SAB82532_ISR1_ALLS) {
		info->interrupt_mask1 |= SAB82532_IMR1_ALLS;
		writeb(info->interrupt_mask1, &info->regs->w.imr1);
		set_bit(SAB82532_ALLS, &info->irqflags);
	}

	if (!(stat->sreg.isr1 & SAB82532_ISR1_XPR))
		return;

	if (!(readb(&info->regs->r.star) & SAB82532_STAR_XFW)) {
#ifdef SERIAL_DEBUG_FIFO
		printk("%s: XPR, but no XFW (?)\n", __FUNCTION__);
#endif
		return;
	}

	set_bit(SAB82532_XPR, &info->irqflags);

	if (!info->tty) {
		info->interrupt_mask1 |= SAB82532_IMR1_XPR;
		writeb(info->interrupt_mask1, &info->regs->w.imr1);
		return;
	}

	if ((info->xmit.head == info->xmit.tail) ||
	    info->tty->stopped || info->tty->hw_stopped) {
		info->interrupt_mask1 |= SAB82532_IMR1_XPR;
		writeb(info->interrupt_mask1, &info->regs->w.imr1);
		return;
	}

	info->interrupt_mask1 &= ~(SAB82532_IMR1_ALLS);
	writeb(info->interrupt_mask1, &info->regs->w.imr1);
	clear_bit(SAB82532_ALLS, &info->irqflags);

	/* Stuff 32 bytes into Transmit FIFO. */
	clear_bit(SAB82532_XPR, &info->irqflags);
	for (i = 0; i < info->xmit_fifo_size; i++) {
		writeb(info->xmit.buf[info->xmit.tail],
		       &info->regs->w.xfifo[i]);
		info->xmit.tail = (info->xmit.tail + 1) & (SERIAL_XMIT_SIZE-1);
		info->icount.tx++;
		if (info->xmit.head == info->xmit.tail)
			break;
	}

	/* Issue a Transmit Frame command. */
	sab82532_cec_wait(info);
	writeb(SAB82532_CMDR_XF, &info->regs->w.cmdr);

	if (CIRC_CNT(info->xmit.head, info->xmit.tail, SERIAL_XMIT_SIZE) < WAKEUP_CHARS)
		sab82532_sched_event(info, RS_EVENT_WRITE_WAKEUP);

#ifdef SERIAL_DEBUG_INTR
	printk("THRE...");
#endif
}

static void check_status(struct sab82532 *info,
			 union sab82532_irq_status *stat)
{
	struct tty_struct *tty = info->tty;
	int modem_change = 0;

	if (stat->sreg.isr1 & SAB82532_ISR1_BRK) {
#ifdef CONFIG_SERIAL_CONSOLE
		if (info->is_console) {
			batten_down_hatches(info);
			return;
		}
#endif
		if (tty->flip.count >= TTY_FLIPBUF_SIZE) {
			info->icount.buf_overrun++;
			goto check_modem;
		}
		tty->flip.count++;
		*tty->flip.flag_buf_ptr++ = TTY_PARITY;
		*tty->flip.char_buf_ptr++ = 0;
		info->icount.brk++;
	}

	if (!tty)
		return;

	if (stat->sreg.isr0 & SAB82532_ISR0_RFO) {
		if (tty->flip.count >= TTY_FLIPBUF_SIZE) {
			info->icount.buf_overrun++;
			goto check_modem;
		}
		tty->flip.count++;
		*tty->flip.flag_buf_ptr++ = TTY_PARITY;
		*tty->flip.char_buf_ptr++ = 0;
		info->icount.overrun++;
	}

check_modem:
	if (stat->sreg.isr0 & SAB82532_ISR0_CDSC) {
		info->dcd = (readb(&info->regs->r.vstr) & SAB82532_VSTR_CD) ? 0 : 1;
		info->icount.dcd++;
		modem_change++;
#ifdef SERIAL_DEBUG_MODEM
		printk("DCD change: %d\n", info->icount.dcd);
#endif
	}
	if (stat->sreg.isr1 & SAB82532_ISR1_CSC) {
		info->cts = readb(&info->regs->r.star) & SAB82532_STAR_CTS;
		info->icount.cts++;
		modem_change++;
#ifdef SERIAL_DEBUG_MODEM
		printk("CTS change: %d, CTS %s\n", info->icount.cts, info->cts ? "on" : "off");
#endif
	}
	if ((readb(&info->regs->r.pvr) & info->pvr_dsr_bit) ^ info->dsr) {
		info->dsr = (readb(&info->regs->r.pvr) & info->pvr_dsr_bit) ? 0 : 1;
		info->icount.dsr++;
		modem_change++;
#ifdef SERIAL_DEBUG_MODEM
		printk("DSR change: %d\n", info->icount.dsr);
#endif
	}
	if (modem_change)
		wake_up_interruptible(&info->delta_msr_wait);

	if ((info->flags & ASYNC_CHECK_CD) &&
	    (stat->sreg.isr0 & SAB82532_ISR0_CDSC)) {

#if (defined(SERIAL_DEBUG_OPEN) || defined(SERIAL_DEBUG_INTR))
		printk("ttys%d CD now %s...", info->line,
		       (info->dcd) ? "on" : "off");
#endif		

		if (info->dcd)
			wake_up_interruptible(&info->open_wait);
		else if (!((info->flags & ASYNC_CALLOUT_ACTIVE) &&
			   (info->flags & ASYNC_CALLOUT_NOHUP))) {

#ifdef SERIAL_DEBUG_OPEN
			printk("scheduling hangup...");
#endif
			MOD_INC_USE_COUNT;
			if (schedule_task(&info->tqueue_hangup) == 0)
				MOD_DEC_USE_COUNT;
		}
	}

	if (info->flags & ASYNC_CTS_FLOW) {
		if (info->tty->hw_stopped) {
			if (info->cts) {

#if (defined(SERIAL_DEBUG_INTR) || defined(SERIAL_DEBUG_FLOW))
				printk("CTS tx start...");
#endif
				info->tty->hw_stopped = 0;
				sab82532_sched_event(info,
						     RS_EVENT_WRITE_WAKEUP);
				info->interrupt_mask1 &= ~(SAB82532_IMR1_XPR);
				writeb(info->interrupt_mask1, &info->regs->w.imr1);
				sab82532_start_tx(info);
			}
		} else {
			if (!(info->cts)) {

#if (defined(SERIAL_DEBUG_INTR) || defined(SERIAL_DEBUG_FLOW))
				printk("CTS tx stop...");
#endif
				info->tty->hw_stopped = 1;
			}
		}
	}
}

/*
 * This is the serial driver's generic interrupt routine
 */
static void sab82532_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	struct sab82532 *info = dev_id;
	union sab82532_irq_status status;

#ifdef SERIAL_DEBUG_INTR
	printk("sab82532_interrupt(%d)...", irq);
#endif

	status.stat = 0;
	if (readb(&info->regs->r.gis) & SAB82532_GIS_ISA0)
		status.sreg.isr0 = readb(&info->regs->r.isr0);
	if (readb(&info->regs->r.gis) & SAB82532_GIS_ISA1)
		status.sreg.isr1 = readb(&info->regs->r.isr1);

#ifdef SERIAL_DEBUG_INTR
	printk("%d<%02x.%02x>", info->line,
	       status.sreg.isr0, status.sreg.isr1);
#endif

	if (!status.stat)
		goto next;

	if (status.sreg.isr0 & (SAB82532_ISR0_TCD | SAB82532_ISR0_TIME |
				SAB82532_ISR0_RFO | SAB82532_ISR0_RPF))
		receive_chars(info, &status);
	if ((status.sreg.isr0 & SAB82532_ISR0_CDSC) ||
	    (status.sreg.isr1 & (SAB82532_ISR1_BRK | SAB82532_ISR1_CSC)))
		check_status(info, &status);
	if (status.sreg.isr1 & (SAB82532_ISR1_ALLS | SAB82532_ISR1_XPR))
		transmit_chars(info, &status);

next:
	info = info->next;
	status.stat = 0;
	if (readb(&info->regs->r.gis) & SAB82532_GIS_ISB0)
		status.sreg.isr0 = readb(&info->regs->r.isr0);
	if (readb(&info->regs->r.gis) & SAB82532_GIS_ISB1)
		status.sreg.isr1 = readb(&info->regs->r.isr1);

#ifdef SERIAL_DEBUG_INTR
	printk("%d<%02x.%02x>", info->line,
	       status.sreg.isr0, status.sreg.isr1);
#endif

	if (!status.stat)
		goto done;

	if (status.sreg.isr0 & (SAB82532_ISR0_TCD | SAB82532_ISR0_TIME |
				SAB82532_ISR0_RFO | SAB82532_ISR0_RPF))
		receive_chars(info, &status);
	if ((status.sreg.isr0 & SAB82532_ISR0_CDSC) ||
	    (status.sreg.isr1 & (SAB82532_ISR1_BRK | SAB82532_ISR1_CSC)))
		check_status(info, &status);
	if (status.sreg.isr1 & (SAB82532_ISR1_ALLS | SAB82532_ISR1_XPR))
		transmit_chars(info, &status);

done:
	;
#ifdef SERIAL_DEBUG_INTR
	printk("end.\n");
#endif
}

/*
 * -------------------------------------------------------------------
 * Here ends the serial interrupt routines.
 * -------------------------------------------------------------------
 */

/*
 * This routine is used to handle the "bottom half" processing for the
 * serial driver, known also the "software interrupt" processing.
 * This processing is done at the kernel interrupt level, after the
 * sab82532_interrupt() has returned, BUT WITH INTERRUPTS TURNED ON.  This
 * is where time-consuming activities which can not be done in the
 * interrupt driver proper are done; the interrupt driver schedules
 * them using sab82532_sched_event(), and they get done here.
 */
static void do_serial_bh(void)
{
	run_task_queue(&tq_serial);
}

static void do_softint(void *private_)
{
	struct sab82532	*info = (struct sab82532 *)private_;
	struct tty_struct *tty;

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
 * This routine is called from the scheduler tqueue when the interrupt
 * routine has signalled that a hangup has occurred.  The path of
 * hangup processing is:
 *
 * 	serial interrupt routine -> (scheduler tqueue) ->
 * 	do_serial_hangup() -> tty->hangup() -> sab82532_hangup()
 * 
 */
static void do_serial_hangup(void *private_)
{
	struct sab82532	*info = (struct sab82532 *) private_;
	struct tty_struct *tty;

	tty = info->tty;
	if (tty)
		tty_hangup(tty);
	MOD_DEC_USE_COUNT;
}

static void
sab82532_init_line(struct sab82532 *info)
{
	unsigned char stat, tmp;

	/*
	 * Wait for any commands or immediate characters
	 */
	sab82532_cec_wait(info);
	sab82532_tec_wait(info);

	/*
	 * Clear the FIFO buffers.
	 */
	writeb(SAB82532_CMDR_RRES, &info->regs->w.cmdr);
	sab82532_cec_wait(info);
	writeb(SAB82532_CMDR_XRES, &info->regs->w.cmdr);

	/*
	 * Clear the interrupt registers.
	 */
	stat = readb(&info->regs->r.isr0);
	stat = readb(&info->regs->r.isr1);

	/*
	 * Now, initialize the UART 
	 */
	writeb(0, &info->regs->w.ccr0);				/* power-down */
	writeb(SAB82532_CCR0_MCE | SAB82532_CCR0_SC_NRZ |
	       SAB82532_CCR0_SM_ASYNC, &info->regs->w.ccr0);
	writeb(SAB82532_CCR1_ODS | SAB82532_CCR1_BCR | 7, &info->regs->w.ccr1);
	writeb(SAB82532_CCR2_BDF | SAB82532_CCR2_SSEL |
	       SAB82532_CCR2_TOE, &info->regs->w.ccr2);
	writeb(0, &info->regs->w.ccr3);
	writeb(SAB82532_CCR4_MCK4 | SAB82532_CCR4_EBRG, &info->regs->w.ccr4);
	writeb(SAB82532_MODE_RTS | SAB82532_MODE_FCTS |
	       SAB82532_MODE_RAC, &info->regs->w.mode);
	writeb(SAB82532_RFC_DPS | SAB82532_RFC_RFDF, &info->regs->w.rfc);
	switch (info->recv_fifo_size) {
		case 1:
			tmp = readb(&info->regs->w.rfc);
			tmp |= SAB82532_RFC_RFTH_1;
			writeb(tmp, &info->regs->w.rfc);
			break;
		case 4:
			tmp = readb(&info->regs->w.rfc);
			tmp |= SAB82532_RFC_RFTH_4;
			writeb(tmp, &info->regs->w.rfc);
			break;
		case 16:
			tmp = readb(&info->regs->w.rfc);
			tmp |= SAB82532_RFC_RFTH_16;
			writeb(tmp, &info->regs->w.rfc);
			break;
		default:
			info->recv_fifo_size = 32;
			/* fall through */
		case 32:
			tmp = readb(&info->regs->w.rfc);
			tmp |= SAB82532_RFC_RFTH_32;
			writeb(tmp, &info->regs->w.rfc);
			break;
	}
	tmp = readb(&info->regs->rw.ccr0);
	tmp |= SAB82532_CCR0_PU;	/* power-up */
	writeb(tmp, &info->regs->rw.ccr0);
}

static int startup(struct sab82532 *info)
{
	unsigned long flags;
	unsigned long page;
	int retval = 0;

	page = get_free_page(GFP_KERNEL);
	if (!page)
		return -ENOMEM;

	save_flags(flags); cli();

	if (info->flags & ASYNC_INITIALIZED) {
		free_page(page);
		goto errout;
	}

	if (!info->regs) {
		if (info->tty)
			set_bit(TTY_IO_ERROR, &info->tty->flags);
		free_page(page);
		retval = -ENODEV;
		goto errout;
	}
	if (info->xmit.buf)
		free_page(page);
	else
		info->xmit.buf = (unsigned char *)page;

#ifdef SERIAL_DEBUG_OPEN
	printk("starting up serial port %d...", info->line);
#endif

	/*
	 * Initialize the Hardware
	 */
	sab82532_init_line(info);

	if (info->tty->termios->c_cflag & CBAUD) {
		u8 tmp;

		tmp = readb(&info->regs->rw.mode);
		tmp &= ~(SAB82532_MODE_FRTS);
		tmp |= SAB82532_MODE_RTS;
		writeb(tmp, &info->regs->rw.mode);

		tmp = readb(&info->regs->rw.pvr);
		tmp &= ~(info->pvr_dtr_bit);
		writeb(tmp, &info->regs->rw.pvr);
	}

	/*
	 * Finally, enable interrupts
	 */
	info->interrupt_mask0 = SAB82532_IMR0_PERR | SAB82532_IMR0_FERR |
				SAB82532_IMR0_PLLA;
	writeb(info->interrupt_mask0, &info->regs->w.imr0);
	info->interrupt_mask1 = SAB82532_IMR1_BRKT | SAB82532_IMR1_ALLS |
				SAB82532_IMR1_XOFF | SAB82532_IMR1_TIN |
				SAB82532_IMR1_CSC | SAB82532_IMR1_XON |
				SAB82532_IMR1_XPR;
	writeb(info->interrupt_mask1, &info->regs->w.imr1);
	set_bit(SAB82532_ALLS, &info->irqflags);

	if (info->tty)
		clear_bit(TTY_IO_ERROR, &info->tty->flags);
	info->xmit.head = info->xmit.tail = 0;

	set_bit(SAB82532_XPR, &info->irqflags);

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
static void shutdown(struct sab82532 *info)
{
	unsigned long flags;
	u8 tmp;

	if (!(info->flags & ASYNC_INITIALIZED))
		return;

#ifdef SERIAL_DEBUG_OPEN
	printk("Shutting down serial port %d...", info->line);
#endif

	save_flags(flags); cli(); /* Disable interrupts */

	/*
	 * clear delta_msr_wait queue to avoid mem leaks: we may free the irq
	 * here so the queue might never be waken up
	 */
	wake_up_interruptible(&info->delta_msr_wait);

	if (info->xmit.buf) {
		free_page((unsigned long)info->xmit.buf);
		info->xmit.buf = 0;
	}

#ifdef CONFIG_SERIAL_CONSOLE
	if (info->is_console) {
		info->interrupt_mask0 = SAB82532_IMR0_PERR | SAB82532_IMR0_FERR |
					SAB82532_IMR0_PLLA | SAB82532_IMR0_CDSC;
		writeb(info->interrupt_mask0, &info->regs->w.imr0);
		info->interrupt_mask1 = SAB82532_IMR1_BRKT | SAB82532_IMR1_ALLS |
					SAB82532_IMR1_XOFF | SAB82532_IMR1_TIN |
					SAB82532_IMR1_CSC | SAB82532_IMR1_XON |
					SAB82532_IMR1_XPR;
		writeb(info->interrupt_mask1, &info->regs->w.imr1);
		if (info->tty)
			set_bit(TTY_IO_ERROR, &info->tty->flags);
		info->flags &= ~ASYNC_INITIALIZED;
		restore_flags(flags);
		return;
	}
#endif

	/* Disable Interrupts */
	info->interrupt_mask0 = 0xff;
	writeb(info->interrupt_mask0, &info->regs->w.imr0);
	info->interrupt_mask1 = 0xff;
	writeb(info->interrupt_mask1, &info->regs->w.imr1);

	if (!info->tty || (info->tty->termios->c_cflag & HUPCL)) {
		tmp = readb(&info->regs->r.mode);
		tmp |= (SAB82532_MODE_FRTS | SAB82532_MODE_RTS);
		writeb(tmp, &info->regs->rw.mode);
		writeb(readb(&info->regs->rw.pvr) | info->pvr_dtr_bit,
		       &info->regs->rw.pvr);
	}

	/* Disable break condition */
	tmp = readb(&info->regs->rw.dafo);
	tmp &= ~(SAB82532_DAFO_XBRK);
	writeb(tmp, &info->regs->rw.dafo);

	/* Disable Receiver */	
	tmp = readb(&info->regs->rw.mode);
	tmp &= ~(SAB82532_MODE_RAC);
	writeb(tmp, &info->regs->rw.mode);

	/* Power Down */	
	tmp = readb(&info->regs->rw.ccr0);
	tmp &= ~(SAB82532_CCR0_PU);
	writeb(tmp, &info->regs->rw.ccr0);

	if (info->tty)
		set_bit(TTY_IO_ERROR, &info->tty->flags);

	info->flags &= ~ASYNC_INITIALIZED;
	restore_flags(flags);
}

/*
 * This routine is called to set the UART divisor registers to match
 * the specified baud rate for a serial port.
 */
static void change_speed(struct sab82532 *info)
{
	unsigned long	flags;
	unsigned int	ebrg;
	tcflag_t	cflag;
	unsigned char	dafo;
	int		bits, n, m;

	if (!info->tty || !info->tty->termios)
		return;
	cflag = info->tty->termios->c_cflag;

	/* Byte size and parity */
	switch (cflag & CSIZE) {
	      case CS5: dafo = SAB82532_DAFO_CHL5; bits = 7; break;
	      case CS6: dafo = SAB82532_DAFO_CHL6; bits = 8; break;
	      case CS7: dafo = SAB82532_DAFO_CHL7; bits = 9; break;
	      case CS8: dafo = SAB82532_DAFO_CHL8; bits = 10; break;
	      /* Never happens, but GCC is too dumb to figure it out */
	      default:  dafo = SAB82532_DAFO_CHL5; bits = 7; break;
	}

	if (cflag & CSTOPB) {
		dafo |= SAB82532_DAFO_STOP;
		bits++;
	}

	if (cflag & PARENB) {
		dafo |= SAB82532_DAFO_PARE;
		bits++;
	}

	if (cflag & PARODD) {
#ifdef CMSPAR
		if (cflag & CMSPAR)
			dafo |= SAB82532_DAFO_PAR_MARK;
		else
#endif
			dafo |= SAB82532_DAFO_PAR_ODD;
	} else {
#ifdef CMSPAR
		if (cflag & CMSPAR)
			dafo |= SAB82532_DAFO_PAR_SPACE;
		else
#endif
			dafo |= SAB82532_DAFO_PAR_EVEN;
	}

	/* Determine EBRG values based on baud rate */
	info->baud = tty_get_baud_rate(info->tty);
	calc_ebrg(info->baud, &n, &m);
	
	ebrg = n | (m << 6);

	if (info->baud) {
		info->timeout = (info->xmit_fifo_size * HZ * bits) / info->baud;
		info->tec_timeout = (10 * 1000000) / info->baud;
		info->cec_timeout = info->tec_timeout >> 2;
	} else {
		info->timeout = 0;
		info->tec_timeout = SAB82532_MAX_TEC_TIMEOUT;
		info->cec_timeout = SAB82532_MAX_CEC_TIMEOUT;
	}
	info->timeout += HZ / 50;		/* Add .02 seconds of slop */

	/* CTS flow control flags */
	if (cflag & CRTSCTS)
		info->flags |= ASYNC_CTS_FLOW;
	else
		info->flags &= ~(ASYNC_CTS_FLOW);

	if (cflag & CLOCAL)
		info->flags &= ~(ASYNC_CHECK_CD);
	else
		info->flags |= ASYNC_CHECK_CD;
	if (info->tty)
		info->tty->hw_stopped = 0;

	/*
	 * Set up parity check flag
	 * XXX: not implemented, yet.
	 */
#define RELEVANT_IFLAG(iflag) (iflag & (IGNBRK|BRKINT|IGNPAR|PARMRK|INPCK))

	/*
	 * Characters to ignore
	 * XXX: not implemented, yet.
	 */

	/*
	 * !!! ignore all characters if CREAD is not set
	 * XXX: not implemented, yet.
	 */
	if ((cflag & CREAD) == 0)
		info->ignore_status_mask |= SAB82532_ISR0_RPF |
					    SAB82532_ISR0_TCD |
					    SAB82532_ISR0_TIME;

	save_flags(flags); cli();
	sab82532_cec_wait(info);
	sab82532_tec_wait(info);
	writeb(dafo, &info->regs->w.dafo);
	writeb(ebrg & 0xff, &info->regs->w.bgr);
	writeb(readb(&info->regs->rw.ccr2) & ~(0xc0), &info->regs->rw.ccr2);
	writeb(readb(&info->regs->rw.ccr2) | ((ebrg >> 2) & 0xc0), &info->regs->rw.ccr2);
	if (info->flags & ASYNC_CTS_FLOW) {
		writeb(readb(&info->regs->rw.mode) & ~(SAB82532_MODE_RTS), &info->regs->rw.mode);
		writeb(readb(&info->regs->rw.mode) | SAB82532_MODE_FRTS, &info->regs->rw.mode);
		writeb(readb(&info->regs->rw.mode) & ~(SAB82532_MODE_FCTS), &info->regs->rw.mode);
		info->interrupt_mask1 &= ~(SAB82532_IMR1_CSC);
		writeb(info->interrupt_mask1, &info->regs->w.imr1);
	} else {
		writeb(readb(&info->regs->rw.mode) | SAB82532_MODE_RTS, &info->regs->rw.mode);
		writeb(readb(&info->regs->rw.mode) & ~(SAB82532_MODE_FRTS), &info->regs->rw.mode);
		writeb(readb(&info->regs->rw.mode) | SAB82532_MODE_FCTS, &info->regs->rw.mode);
		info->interrupt_mask1 |= SAB82532_IMR1_CSC;
		writeb(info->interrupt_mask1, &info->regs->w.imr1);
	}
	writeb(readb(&info->regs->rw.mode) | SAB82532_MODE_RAC, &info->regs->rw.mode);
	restore_flags(flags);
}

static void sab82532_put_char(struct tty_struct *tty, unsigned char ch)
{
	struct sab82532 *info = (struct sab82532 *)tty->driver_data;
	unsigned long flags;

	if (serial_paranoia_check(info, tty->device, "sab82532_put_char"))
		return;

	if (!tty || !info->xmit.buf)
		return;

	save_flags(flags); cli();
	if (!CIRC_SPACE(info->xmit.head, info->xmit.tail, SERIAL_XMIT_SIZE)) {
		restore_flags(flags);
		return;
	}

	info->xmit.buf[info->xmit.head] = ch;
	info->xmit.head = (info->xmit.head + 1) & (SERIAL_XMIT_SIZE-1);
	restore_flags(flags);
}

static void sab82532_flush_chars(struct tty_struct *tty)
{
	struct sab82532 *info = (struct sab82532 *)tty->driver_data;
	unsigned long flags;

	if (serial_paranoia_check(info, tty->device, "sab82532_flush_chars"))
		return;

	if ((info->xmit.head == info->xmit.tail) ||
	    tty->stopped || tty->hw_stopped || !info->xmit.buf)
		return;

	save_flags(flags); cli();
	info->interrupt_mask1 &= ~(SAB82532_IMR1_XPR);
	writeb(info->interrupt_mask1, &info->regs->w.imr1);
	sab82532_start_tx(info);
	restore_flags(flags);
}

static int sab82532_write(struct tty_struct * tty, int from_user,
			  const unsigned char *buf, int count)
{
	int c, ret = 0;
	struct sab82532 *info = (struct sab82532 *)tty->driver_data;
	unsigned long flags;

	if (serial_paranoia_check(info, tty->device, "sab82532_write"))
		return 0;

	if (!tty || !info->xmit.buf || !tmp_buf)
		return 0;
	    
	save_flags(flags);
	if (from_user) {
		down(&tmp_buf_sem);
		while (1) {
			int c1;
			c = CIRC_SPACE_TO_END(info->xmit.head,
					      info->xmit.tail,
					      SERIAL_XMIT_SIZE);
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
					       SERIAL_XMIT_SIZE);
			if (c1 < c)
				c = c1;
			memcpy(info->xmit.buf + info->xmit.head, tmp_buf, c);
			info->xmit.head = (info->xmit.head + c) & (SERIAL_XMIT_SIZE-1);
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
					      SERIAL_XMIT_SIZE);
			if (count < c)
				c = count;
			if (c <= 0)
				break;
			memcpy(info->xmit.buf + info->xmit.head, buf, c);
			info->xmit.head = (info->xmit.head + c) & (SERIAL_XMIT_SIZE-1);
			buf += c;
			count -= c;
			ret += c;
		}
		restore_flags(flags);
	}

	if ((info->xmit.head != info->xmit.tail) &&
	    !tty->stopped && !tty->hw_stopped) {
		info->interrupt_mask1 &= ~(SAB82532_IMR1_XPR);
		writeb(info->interrupt_mask1, &info->regs->w.imr1);
		sab82532_start_tx(info);
	}

	restore_flags(flags);
	return ret;
}

static int sab82532_write_room(struct tty_struct *tty)
{
	struct sab82532 *info = (struct sab82532 *)tty->driver_data;

	if (serial_paranoia_check(info, tty->device, "sab82532_write_room"))
		return 0;

	return CIRC_SPACE(info->xmit.head, info->xmit.tail, SERIAL_XMIT_SIZE);
}

static int sab82532_chars_in_buffer(struct tty_struct *tty)
{
	struct sab82532 *info = (struct sab82532 *)tty->driver_data;
				
	if (serial_paranoia_check(info, tty->device, "sab82532_chars_in_buffer"))
		return 0;

	return CIRC_CNT(info->xmit.head, info->xmit.tail, SERIAL_XMIT_SIZE);
}

static void sab82532_flush_buffer(struct tty_struct *tty)
{
	struct sab82532 *info = (struct sab82532 *)tty->driver_data;
	unsigned long flags;

	if (serial_paranoia_check(info, tty->device, "sab82532_flush_buffer"))
		return;

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
static void sab82532_send_xchar(struct tty_struct *tty, char ch)
{
	struct sab82532 *info = (struct sab82532 *)tty->driver_data;
	unsigned long flags;

	if (serial_paranoia_check(info, tty->device, "sab82532_send_xchar"))
		return;

	save_flags(flags); cli();
	sab82532_tec_wait(info);
	writeb(ch, &info->regs->w.tic);
	restore_flags(flags);
}

/*
 * ------------------------------------------------------------
 * sab82532_throttle()
 * 
 * This routine is called by the upper-layer tty layer to signal that
 * incoming characters should be throttled.
 * ------------------------------------------------------------
 */
static void sab82532_throttle(struct tty_struct * tty)
{
	struct sab82532 *info = (struct sab82532 *)tty->driver_data;
#ifdef SERIAL_DEBUG_THROTTLE
	char	buf[64];
	
	printk("throttle %s: %d....\n", _tty_name(tty, buf),
	       tty->ldisc.chars_in_buffer(tty));
#endif

	if (serial_paranoia_check(info, tty->device, "sab82532_throttle"))
		return;
	
	if (I_IXOFF(tty))
		sab82532_send_xchar(tty, STOP_CHAR(tty));

	if (tty->termios->c_cflag & CRTSCTS) {
		u8 mode = readb(&info->regs->r.mode);
		mode &= ~(SAB82532_MODE_FRTS | SAB82532_MODE_RTS);
		writeb(mode, &info->regs->w.mode);
	}
}

static void sab82532_unthrottle(struct tty_struct * tty)
{
	struct sab82532 *info = (struct sab82532 *)tty->driver_data;
#ifdef SERIAL_DEBUG_THROTTLE
	char	buf[64];
	
	printk("unthrottle %s: %d....\n", _tty_name(tty, buf),
	       tty->ldisc.chars_in_buffer(tty));
#endif

	if (serial_paranoia_check(info, tty->device, "sab82532_unthrottle"))
		return;
	
	if (I_IXOFF(tty)) {
		if (info->x_char)
			info->x_char = 0;
		else
			sab82532_send_xchar(tty, START_CHAR(tty));
	}

	if (tty->termios->c_cflag & CRTSCTS) {
		u8 mode = readb(&info->regs->r.mode);
		mode &= ~(SAB82532_MODE_RTS);
		mode |= SAB82532_MODE_FRTS;
		writeb(mode, &info->regs->w.mode);
	}
}

/*
 * ------------------------------------------------------------
 * sab82532_ioctl() and friends
 * ------------------------------------------------------------
 */

static int get_serial_info(struct sab82532 *info,
			   struct serial_struct *retinfo)
{
	struct serial_struct tmp;
   
	if (!retinfo)
		return -EFAULT;
	memset(&tmp, 0, sizeof(tmp));
	tmp.type = info->type;
	tmp.line = info->line;
	tmp.port = (unsigned long)info->regs;
	tmp.irq = info->irq;
	tmp.flags = info->flags;
	tmp.xmit_fifo_size = info->xmit_fifo_size;
	tmp.baud_base = info->baud_base;
	tmp.close_delay = info->close_delay;
	tmp.closing_wait = info->closing_wait;
	tmp.custom_divisor = info->custom_divisor;
	tmp.hub6 = 0;
	if (copy_to_user(retinfo, &tmp, sizeof(*retinfo)))
		return -EFAULT;
	return 0;
}

static int set_serial_info(struct sab82532 *info,
			   struct serial_struct *new_info)
{
	return 0;
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
static int get_lsr_info(struct sab82532 * info, unsigned int *value)
{
	unsigned int result;

	result = (!info->xmit.buf && test_bit(SAB82532_ALLS, &info->irqflags))
							? TIOCSER_TEMT : 0;
	return put_user(result, value);
}


static int get_modem_info(struct sab82532 * info, unsigned int *value)
{
	unsigned int result;

	result =  ((readb(&info->regs->r.mode) & SAB82532_MODE_RTS) ? 
		    ((readb(&info->regs->r.mode) & SAB82532_MODE_FRTS) ? 0 : TIOCM_RTS)
							    : TIOCM_RTS)
		| ((readb(&info->regs->r.pvr) & info->pvr_dtr_bit) ? 0 : TIOCM_DTR)
		| ((readb(&info->regs->r.vstr) & SAB82532_VSTR_CD) ? 0 : TIOCM_CAR)
		| ((readb(&info->regs->r.pvr) & info->pvr_dsr_bit) ? 0 : TIOCM_DSR)
		| ((readb(&info->regs->r.star) & SAB82532_STAR_CTS) ? TIOCM_CTS : 0);
	return put_user(result,value);
}

static int set_modem_info(struct sab82532 * info, unsigned int cmd,
			  unsigned int *value)
{
	unsigned int arg;

	if (get_user(arg, value))
		return -EFAULT;
	switch (cmd) {
	case TIOCMBIS: 
		if (arg & TIOCM_RTS) {
			writeb(readb(&info->regs->rw.mode) & ~(SAB82532_MODE_FRTS), &info->regs->rw.mode);
			writeb(readb(&info->regs->rw.mode) | SAB82532_MODE_RTS, &info->regs->rw.mode);
		}
		if (arg & TIOCM_DTR) {
			writeb(readb(&info->regs->rw.pvr) & ~(info->pvr_dtr_bit), &info->regs->rw.pvr);
		}
		break;
	case TIOCMBIC:
		if (arg & TIOCM_RTS) {
			writeb(readb(&info->regs->rw.mode) | SAB82532_MODE_FRTS, &info->regs->rw.mode);
			writeb(readb(&info->regs->rw.mode) | SAB82532_MODE_RTS, &info->regs->rw.mode);
		}
		if (arg & TIOCM_DTR) {
			writeb(readb(&info->regs->rw.pvr) | info->pvr_dtr_bit, &info->regs->rw.pvr);
		}
		break;
	case TIOCMSET:
		if (arg & TIOCM_RTS) {
			writeb(readb(&info->regs->rw.mode) & ~(SAB82532_MODE_FRTS), &info->regs->rw.mode);
			writeb(readb(&info->regs->rw.mode) | SAB82532_MODE_RTS, &info->regs->rw.mode);
		} else {
			writeb(readb(&info->regs->rw.mode) | SAB82532_MODE_FRTS, &info->regs->rw.mode);
			writeb(readb(&info->regs->rw.mode) | SAB82532_MODE_RTS, &info->regs->rw.mode);
		}
		if (arg & TIOCM_DTR) {
			writeb(readb(&info->regs->rw.pvr) & ~(info->pvr_dtr_bit), &info->regs->rw.pvr);
		} else {
			writeb(readb(&info->regs->rw.pvr) | info->pvr_dtr_bit, &info->regs->rw.pvr);
		}
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

/*
 * This routine sends a break character out the serial port.
 */
static void sab82532_break(struct tty_struct *tty, int break_state)
{
	struct sab82532 * info = (struct sab82532 *)tty->driver_data;
	unsigned long flags;

	if (serial_paranoia_check(info, tty->device, "sab82532_break"))
		return;

	if (!info->regs)
		return;

#ifdef SERIAL_DEBUG_SEND_BREAK
	printk("sab82532_break(%d) jiff=%lu...", break_state, jiffies);
#endif
	save_flags(flags); cli();
	if (break_state == -1)
		writeb(readb(&info->regs->rw.dafo) | SAB82532_DAFO_XBRK, &info->regs->rw.dafo);
	else
		writeb(readb(&info->regs->rw.dafo) & ~(SAB82532_DAFO_XBRK), &info->regs->rw.dafo);
	restore_flags(flags);
}


static int sab82532_ioctl(struct tty_struct *tty, struct file * file,
		    unsigned int cmd, unsigned long arg)
{
	struct sab82532 * info = (struct sab82532 *)tty->driver_data;
	struct async_icount cprev, cnow;	/* kernel counter temps */
	struct serial_icounter_struct *p_cuser;	/* user space */

	if (serial_paranoia_check(info, tty->device, "sab82532_ioctl"))
		return -ENODEV;

	if ((cmd != TIOCGSERIAL) && (cmd != TIOCSSERIAL) &&
	    (cmd != TIOCSERCONFIG) && (cmd != TIOCSERGWILD)  &&
	    (cmd != TIOCSERSWILD) && (cmd != TIOCSERGSTRUCT) &&
	    (cmd != TIOCMIWAIT) && (cmd != TIOCGICOUNT)) {
		if (tty->flags & (1 << TTY_IO_ERROR))
		    return -EIO;
	}
	
	switch (cmd) {
		case TIOCGSOFTCAR:
			return put_user(C_CLOCAL(tty) ? 1 : 0, (int *) arg);
		case TIOCSSOFTCAR:
			if (get_user(arg, (unsigned int *) arg))
				return -EFAULT;
			tty->termios->c_cflag =
				((tty->termios->c_cflag & ~CLOCAL) |
				 (arg ? CLOCAL : 0));
			return 0;
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

		case TIOCSERGETLSR: /* Get line status register */
			return get_lsr_info(info, (unsigned int *) arg);

		case TIOCSERGSTRUCT:
			if (copy_to_user((struct sab82532 *) arg,
					 info, sizeof(struct sab82532)))
				return -EFAULT;
			return 0;
				
		/*
		 * Wait for any of the 4 modem inputs (DCD,RI,DSR,CTS) to change
		 * - mask passed in arg for lines of interest
 		 *   (use |'ed TIOCM_RNG/DSR/CD/CTS for masking)
		 * Caller should use TIOCGICOUNT to see which one it was
		 */
		 case TIOCMIWAIT:
			cli();
			/* note the counters on entry */
			cprev = info->icount;
			sti();
			while (1) {
				interruptible_sleep_on(&info->delta_msr_wait);
				/* see if a signal did it */
				if (signal_pending(current))
					return -ERESTARTSYS;
				cli();
				cnow = info->icount; /* atomic copy */
				sti();
				if (cnow.rng == cprev.rng && cnow.dsr == cprev.dsr && 
				    cnow.dcd == cprev.dcd && cnow.cts == cprev.cts)
					return -EIO; /* no change => error */
				if ( ((arg & TIOCM_RNG) && (cnow.rng != cprev.rng)) ||
				     ((arg & TIOCM_DSR) && (cnow.dsr != cprev.dsr)) ||
				     ((arg & TIOCM_CD)  && (cnow.dcd != cprev.dcd)) ||
				     ((arg & TIOCM_CTS) && (cnow.cts != cprev.cts)) ) {
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
			cli();
			cnow = info->icount;
			sti();
			p_cuser = (struct serial_icounter_struct *) arg;
			if (put_user(cnow.cts, &p_cuser->cts) ||
			    put_user(cnow.dsr, &p_cuser->dsr) ||
			    put_user(cnow.rng, &p_cuser->rng) ||
			    put_user(cnow.dcd, &p_cuser->dcd))
				return -EFAULT;
			return 0;

		default:
			return -ENOIOCTLCMD;
		}
	return 0;
}

static void sab82532_set_termios(struct tty_struct *tty,
				 struct termios *old_termios)
{
	struct sab82532 *info = (struct sab82532 *)tty->driver_data;

	if (   (tty->termios->c_cflag == old_termios->c_cflag)
	    && (   RELEVANT_IFLAG(tty->termios->c_iflag) 
		== RELEVANT_IFLAG(old_termios->c_iflag)))
	  return;

	change_speed(info);

	/* Handle transition to B0 status */
	if ((old_termios->c_cflag & CBAUD) &&
	    !(tty->termios->c_cflag & CBAUD)) {
		writeb(readb(&info->regs->w.mode) | SAB82532_MODE_FRTS, &info->regs->w.mode);
		writeb(readb(&info->regs->w.mode) | SAB82532_MODE_RTS, &info->regs->w.mode);
		writeb(readb(&info->regs->w.pvr) | info->pvr_dtr_bit, &info->regs->w.pvr);
	}
	
	/* Handle transition away from B0 status */
	if (!(old_termios->c_cflag & CBAUD) &&
	    (tty->termios->c_cflag & CBAUD)) {
		writeb(readb(&info->regs->w.pvr) & ~(info->pvr_dtr_bit), &info->regs->w.pvr);
		if (tty->termios->c_cflag & CRTSCTS) {
			writeb(readb(&info->regs->w.mode) & ~(SAB82532_MODE_RTS), &info->regs->w.mode);
			writeb(readb(&info->regs->w.mode) | SAB82532_MODE_FRTS, &info->regs->w.mode);
		} else if (test_bit(TTY_THROTTLED, &tty->flags)) {
			writeb(readb(&info->regs->w.mode) & ~(SAB82532_MODE_FRTS | SAB82532_MODE_RTS), &info->regs->w.mode);
		} else {
			writeb(readb(&info->regs->w.mode) & ~(SAB82532_MODE_FRTS), &info->regs->w.mode);
			writeb(readb(&info->regs->w.mode) | SAB82532_MODE_RTS, &info->regs->w.mode);
		}
	}
	
	/* Handle turning off CRTSCTS */
	if ((old_termios->c_cflag & CRTSCTS) &&
	    !(tty->termios->c_cflag & CRTSCTS)) {
		tty->hw_stopped = 0;
		sab82532_start(tty);
	}
}

/*
 * ------------------------------------------------------------
 * sab82532_close()
 * 
 * This routine is called when the serial port gets closed.  First, we
 * wait for the last remaining data to be sent.  Then, we unlink its
 * async structure from the interrupt chain if necessary, and we free
 * that IRQ if nothing is left in the chain.
 * ------------------------------------------------------------
 */
static void sab82532_close(struct tty_struct *tty, struct file * filp)
{
	struct sab82532 *info = (struct sab82532 *)tty->driver_data;
	unsigned long flags;

	if (!info || serial_paranoia_check(info, tty->device, "sab82532_close"))
		return;

	save_flags(flags); cli();

	if (tty_hung_up_p(filp)) {
		MOD_DEC_USE_COUNT;
		restore_flags(flags);
		return;
	}

#ifdef SERIAL_DEBUG_OPEN
	printk("sab82532_close ttys%d, count = %d\n", info->line, info->count);
#endif
	if ((tty->count == 1) && (info->count != 1)) {
		/*
		 * Uh, oh.  tty->count is 1, which means that the tty
		 * structure will be freed.  info->count should always
		 * be one in these conditions.  If it's greater than
		 * one, we've got real problems, since it means the
		 * serial port won't be shutdown.
		 */
		printk("sab82532_close: bad serial port count; tty->count is 1,"
		       " info->count is %d\n", info->count);
		info->count = 1;
	}
	if (--info->count < 0) {
		printk("sab82532_close: bad serial port count for ttys%d: %d\n",
		       info->line, info->count);
		info->count = 0;
	}
	if (info->count) {
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
		info->normal_termios = *tty->termios;
	if (info->flags & ASYNC_CALLOUT_ACTIVE)
		info->callout_termios = *tty->termios;
	/*
	 * Now we wait for the transmit buffer to clear; and we notify 
	 * the line discipline to only process XON/XOFF characters.
	 */
	tty->closing = 1;
	if (info->closing_wait != ASYNC_CLOSING_WAIT_NONE)
		tty_wait_until_sent(tty, info->closing_wait);

	/*
	 * At this point we stop accepting input.  To do this, we
	 * disable the receive line status interrupts, and turn off
	 * the receiver.
	 */
	info->interrupt_mask0 |= SAB82532_IMR0_TCD;
	writeb(info->interrupt_mask0, &info->regs->w.imr0);
	if (info->flags & ASYNC_INITIALIZED) {
		/*
		 * Before we drop DTR, make sure the UART transmitter
		 * has completely drained; this is especially
		 * important if there is a transmit FIFO!
		 */
		sab82532_wait_until_sent(tty, info->timeout);
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
 * sab82532_wait_until_sent() --- wait until the transmitter is empty
 */
static void sab82532_wait_until_sent(struct tty_struct *tty, int timeout)
{
	struct sab82532 *info = (struct sab82532 *)tty->driver_data;
	unsigned long orig_jiffies, char_time;

	if (serial_paranoia_check(info,tty->device,"sab82532_wait_until_sent"))
		return;

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
#ifdef SERIAL_DEBUG_WAIT_UNTIL_SENT
	printk("In sab82532_wait_until_sent(%d) check=%lu "
	       "xmit_cnt = %ld, alls = %d (jiff=%lu)...\n",
	       timeout, char_time,
	       CIRC_CNT(info->xmit.head, info->xmit.tail, SERIAL_XMIT_SIZE),
	       test_bit(SAB82532_ALLS, &info->irqflags), jiffies);
#endif
	orig_jiffies = jiffies;
	while ((info->xmit.head != info->xmit.tail) ||
	       !test_bit(SAB82532_ALLS, &info->irqflags)) {
		current->state = TASK_INTERRUPTIBLE;
		schedule_timeout(char_time);
		if (signal_pending(current))
			break;
		if (timeout && time_after(jiffies, orig_jiffies + timeout))
			break;
	}
#ifdef SERIAL_DEBUG_WAIT_UNTIL_SENT
	printk("xmit_cnt = %ld, alls = %d (jiff=%lu)...done\n",
	       CIRC_CNT(info->xmit.head, info->xmit.tail, SERIAL_XMIT_SIZE),
	       test_bit(SAB82532_ALLS, &info->irqflags), jiffies);
#endif
}

/*
 * sab82532_hangup() --- called by tty_hangup() when a hangup is signaled.
 */
static void sab82532_hangup(struct tty_struct *tty)
{
	struct sab82532 * info = (struct sab82532 *)tty->driver_data;

	if (serial_paranoia_check(info, tty->device, "sab82532_hangup"))
		return;

#ifdef CONFIG_SERIAL_CONSOLE
	if (info->is_console)
		return;
#endif

	sab82532_flush_buffer(tty);
	shutdown(info);
	info->event = 0;
	info->count = 0;
	info->flags &= ~(ASYNC_NORMAL_ACTIVE|ASYNC_CALLOUT_ACTIVE);
	info->tty = 0;
	wake_up_interruptible(&info->open_wait);
}

/*
 * ------------------------------------------------------------
 * sab82532_open() and friends
 * ------------------------------------------------------------
 */
static int block_til_ready(struct tty_struct *tty, struct file * filp,
			   struct sab82532 *info)
{
	DECLARE_WAITQUEUE(wait, current);
	int retval;
	int do_clocal = 0;

	/*
	 * If the device is in the middle of being closed, then block
	 * until it's done, and then try again.
	 */
	if (tty_hung_up_p(filp) ||
	    (info->flags & ASYNC_CLOSING)) {
		if (info->flags & ASYNC_CLOSING)
			interruptible_sleep_on(&info->close_wait);
#ifdef SERIAL_DO_RESTART
		if (info->flags & ASYNC_HUP_NOTIFY)
			return -EAGAIN;
		else
			return -ERESTARTSYS;
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
		if (info->normal_termios.c_cflag & CLOCAL)
			do_clocal = 1;
	} else {
		if (tty->termios->c_cflag & CLOCAL)
			do_clocal = 1;
	}
	
	/*
	 * Block waiting for the carrier detect and the line to become
	 * free (i.e., not in use by the callout).  While we are in
	 * this loop, info->count is dropped by one, so that
	 * sab82532_close() knows when to free things.  We restore it upon
	 * exit, either normal or abnormal.
	 */
	retval = 0;
	add_wait_queue(&info->open_wait, &wait);
#ifdef SERIAL_DEBUG_OPEN
	printk("block_til_ready before block: ttyS%d, count = %d\n",
	       info->line, info->count);
#endif
	cli();
	if (!tty_hung_up_p(filp)) 
		info->count--;
	sti();
	info->blocked_open++;
	while (1) {
		cli();
		if (!(info->flags & ASYNC_CALLOUT_ACTIVE) &&
		    (tty->termios->c_cflag & CBAUD)) {
			writeb(readb(&info->regs->rw.pvr) & ~(info->pvr_dtr_bit), &info->regs->rw.pvr);
			writeb(readb(&info->regs->rw.mode) | SAB82532_MODE_FRTS, &info->regs->rw.mode);
			writeb(readb(&info->regs->rw.mode) & ~(SAB82532_MODE_RTS), &info->regs->rw.mode);
		}
		sti();
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
		    !(info->flags & ASYNC_CLOSING) &&
		    (do_clocal || !(readb(&info->regs->r.vstr) & SAB82532_VSTR_CD)))
			break;
		if (signal_pending(current)) {
			retval = -ERESTARTSYS;
			break;
		}
#ifdef SERIAL_DEBUG_OPEN
		printk("block_til_ready blocking: ttyS%d, count = %d, flags = %x, clocal = %d, vstr = %02x\n",
		       info->line, info->count, info->flags, do_clocal, readb(&info->regs->r.vstr));
#endif
		schedule();
	}
	current->state = TASK_RUNNING;
	remove_wait_queue(&info->open_wait, &wait);
	if (!tty_hung_up_p(filp))
		info->count++;
	info->blocked_open--;
#ifdef SERIAL_DEBUG_OPEN
	printk("block_til_ready after blocking: ttys%d, count = %d\n",
	       info->line, info->count);
#endif
	if (retval)
		return retval;
	info->flags |= ASYNC_NORMAL_ACTIVE;
	return 0;
}

/*
 * This routine is called whenever a serial port is opened.  It
 * enables interrupts for a serial port, linking in its async structure into
 * the IRQ chain.   It also performs the serial-specific
 * initialization for the tty structure.
 */
static int sab82532_open(struct tty_struct *tty, struct file * filp)
{
	struct sab82532	*info = sab82532_chain;
	int retval, line;
	unsigned long page;

#ifdef SERIAL_DEBUG_OPEN
	printk("sab82532_open: count = %d\n", info->count);
#endif

	line = MINOR(tty->device) - tty->driver.minor_start;
	if ((line < 0) || (line >= NR_PORTS))
		return -ENODEV;

	while (info) {
		if (info->line == line)
			break;
		info = info->next;
	}
	if (!info) {
		printk("sab82532_open: can't find info for line %d\n",
		       line);
		return -ENODEV;
	}

	if (serial_paranoia_check(info, tty->device, "sab82532_open"))
		return -ENODEV;

#ifdef SERIAL_DEBUG_OPEN
	printk("sab82532_open %s%d, count = %d\n", tty->driver.name, info->line,
	       info->count);
#endif

	if (!tmp_buf) {
		page = get_free_page(GFP_KERNEL);
		if (!page)
			return -ENOMEM;
		if (tmp_buf)
			free_page(page);
		else
			tmp_buf = (unsigned char *) page;
	}

	info->count++;
	tty->driver_data = info;
	info->tty = tty;

	/*
	 * If the port is in the middle of closing, bail out now.
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
	 * Start up serial port
	 */
	retval = startup(info);
	if (retval)
		return retval;

	MOD_INC_USE_COUNT;
	retval = block_til_ready(tty, filp, info);
	if (retval) {
#ifdef SERIAL_DEBUG_OPEN
		printk("sab82532_open returning after block_til_ready with %d\n",
		       retval);
#endif
		return retval;
	}

	if ((info->count == 1) &&
	    (info->flags & ASYNC_SPLIT_TERMIOS)) {
		if (tty->driver.subtype == SERIAL_TYPE_NORMAL)
			*tty->termios = info->normal_termios;
		else 
			*tty->termios = info->callout_termios;
		change_speed(info);
	}

#ifdef CONFIG_SERIAL_CONSOLE
	if (sab82532_console.cflag && sab82532_console.index == line) {
		tty->termios->c_cflag = sab82532_console.cflag;
		sab82532_console.cflag = 0;
		change_speed(info);
	}
#endif

	info->session = current->session;
	info->pgrp = current->pgrp;

#ifdef SERIAL_DEBUG_OPEN
	printk("sab82532_open ttys%d successful... count %d", info->line, info->count);
#endif
	return 0;
}

/*
 * /proc fs routines....
 */

static __inline__ int
line_info(char *buf, struct sab82532 *info)
{
	unsigned long flags;
	char stat_buf[30];
	int ret;

	ret = sprintf(buf, "%u: uart:SAB82532 ", info->line);
	switch (info->type) {
		case 0:
			ret += sprintf(buf+ret, "V1.0 ");
			break;
		case 1:
			ret += sprintf(buf+ret, "V2.0 ");
			break;
		case 2:
			ret += sprintf(buf+ret, "V3.2 ");
			break;
		default:
			ret += sprintf(buf+ret, "V?.? ");
			break;
	}
	ret += sprintf(buf+ret, "port:%lX irq:%s",
		       (unsigned long)info->regs, __irq_itoa(info->irq));

	if (!info->regs) {
		ret += sprintf(buf+ret, "\n");
		return ret;
	}

	/*
	 * Figure out the current RS-232 lines
	 */
	stat_buf[0] = 0;
	stat_buf[1] = 0;
	save_flags(flags); cli();
	if (readb(&info->regs->r.mode) & SAB82532_MODE_RTS) {
		if (!(readb(&info->regs->r.mode) & SAB82532_MODE_FRTS))
			strcat(stat_buf, "|RTS");
	} else {
		strcat(stat_buf, "|RTS");
	}
	if (readb(&info->regs->r.star) & SAB82532_STAR_CTS)
		strcat(stat_buf, "|CTS");
	if (!(readb(&info->regs->r.pvr) & info->pvr_dtr_bit))
		strcat(stat_buf, "|DTR");
	if (!(readb(&info->regs->r.pvr) & info->pvr_dsr_bit))
		strcat(stat_buf, "|DSR");
	if (!(readb(&info->regs->r.vstr) & SAB82532_VSTR_CD))
		strcat(stat_buf, "|CD");
	restore_flags(flags);

	if (info->baud)
		ret += sprintf(buf+ret, " baud:%u", info->baud);

	ret += sprintf(buf+ret, " tx:%u rx:%u",
		       info->icount.tx, info->icount.rx);

	if (info->icount.frame)
		ret += sprintf(buf+ret, " fe:%u", info->icount.frame);

	if (info->icount.parity)
		ret += sprintf(buf+ret, " pe:%u", info->icount.parity);

	if (info->icount.brk)
		ret += sprintf(buf+ret, " brk:%u", info->icount.brk);

	if (info->icount.overrun)
		ret += sprintf(buf+ret, " oe:%u", info->icount.overrun);

	/*
	 * Last thing is the RS-232 status lines.
	 */
	ret += sprintf(buf+ret, " %s\n", stat_buf + 1);
	return ret;
}

int sab82532_read_proc(char *page, char **start, off_t off, int count,
		       int *eof, void *data)
{
	struct sab82532 *info = sab82532_chain;
	off_t begin = 0;
	int len = 0;

	len += sprintf(page, "serinfo:1.0 driver:%s\n", serial_version);
	for (info = sab82532_chain; info && len < 4000; info = info->next) {
		len += line_info(page + len, info);
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
 * sab82532_init() and friends
 *
 * sab82532_init() is called at boot-time to initialize the serial driver.
 * ---------------------------------------------------------------------
 */
static int __init get_sab82532(unsigned long *memory_start)
{
	struct linux_ebus *ebus;
	struct linux_ebus_device *edev = 0;
	struct sab82532 *sab;
	unsigned long regs, offset;
	int i;

	for_each_ebus(ebus) {
		for_each_ebusdev(edev, ebus) {
			if (!strcmp(edev->prom_name, "se"))
				goto ebus_done;

			if (!strcmp(edev->prom_name, "serial")) {
				char compat[32];
				int clen;

				/* On RIO this can be an SE, check it.  We could
				 * just check ebus->is_rio, but this is more portable.
				 */
				clen = prom_getproperty(edev->prom_node, "compatible",
							compat, sizeof(compat));
				if (clen > 0) {
					if (strncmp(compat, "sab82532", 8) == 0) {
						/* Yep. */
						goto ebus_done;
					}
				}
			}
		}
	}
ebus_done:
	if (!edev)
		return -ENODEV;

	regs = edev->resource[0].start;
	offset = sizeof(union sab82532_async_regs);

	for (i = 0; i < 2; i++) {
		if (memory_start) {
			*memory_start = (*memory_start + 7) & ~(7);
			sab = (struct sab82532 *)*memory_start;
			*memory_start += sizeof(struct sab82532);
		} else {
			sab = (struct sab82532 *)kmalloc(sizeof(struct sab82532),
							 GFP_KERNEL);
			if (!sab) {
				printk("sab82532: can't alloc sab struct\n");
				break;
			}
		}
		memset(sab, 0, sizeof(struct sab82532));

		sab->regs = ioremap(regs + offset, sizeof(union sab82532_async_regs));
		sab->irq = edev->irqs[0];
		sab->line = 1 - i;
		sab->xmit_fifo_size = 32;
		sab->recv_fifo_size = 32;

		writeb(SAB82532_IPC_IC_ACT_LOW, &sab->regs->w.ipc);

		sab->next = sab82532_chain;
		sab82532_chain = sab;

		offset -= sizeof(union sab82532_async_regs);
	}
	return 0;
}

#ifndef MODULE
static void __init sab82532_kgdb_hook(int line)
{
	prom_printf("sab82532: kgdb support is not implemented, yet\n");
	prom_halt();
}
#endif

static inline void __init show_serial_version(void)
{
	char *revision = "$Revision: 1.65 $";
	char *version, *p;

	version = strchr(revision, ' ');
	strcpy(serial_version, ++version);
	p = strchr(serial_version, ' ');
	*p = '\0';
	printk("SAB82532 serial driver version %s\n", serial_version);
}

extern int su_num_ports;

/*
 * The serial driver boot-time initialization code!
 */
int __init sab82532_init(void)
{
	struct sab82532 *info;
	int i;

	if (!sab82532_chain)
		get_sab82532(0);
	if (!sab82532_chain)
		return -ENODEV;

	init_bh(SERIAL_BH, do_serial_bh);

	show_serial_version();

	/* Initialize the tty_driver structure */
	memset(&serial_driver, 0, sizeof(struct tty_driver));
	serial_driver.magic = TTY_DRIVER_MAGIC;
	serial_driver.driver_name = "serial";
#ifdef CONFIG_DEVFS_FS
	serial_driver.name = "tts/%d";
#else
	serial_driver.name = "ttyS";
#endif
	serial_driver.major = TTY_MAJOR;
	serial_driver.minor_start = 64 + su_num_ports;
	serial_driver.num = NR_PORTS;
	serial_driver.type = TTY_DRIVER_TYPE_SERIAL;
	serial_driver.subtype = SERIAL_TYPE_NORMAL;
	serial_driver.init_termios = tty_std_termios;
	serial_driver.init_termios.c_cflag =
		B9600 | CS8 | CREAD | HUPCL | CLOCAL;
	serial_driver.flags = TTY_DRIVER_REAL_RAW;
	serial_driver.refcount = &sab82532_refcount;
	serial_driver.table = sab82532_table;
	serial_driver.termios = sab82532_termios;
	serial_driver.termios_locked = sab82532_termios_locked;

	serial_driver.open = sab82532_open;
	serial_driver.close = sab82532_close;
	serial_driver.write = sab82532_write;
	serial_driver.put_char = sab82532_put_char;
	serial_driver.flush_chars = sab82532_flush_chars;
	serial_driver.write_room = sab82532_write_room;
	serial_driver.chars_in_buffer = sab82532_chars_in_buffer;
	serial_driver.flush_buffer = sab82532_flush_buffer;
	serial_driver.ioctl = sab82532_ioctl;
	serial_driver.throttle = sab82532_throttle;
	serial_driver.unthrottle = sab82532_unthrottle;
	serial_driver.send_xchar = sab82532_send_xchar;
	serial_driver.set_termios = sab82532_set_termios;
	serial_driver.stop = sab82532_stop;
	serial_driver.start = sab82532_start;
	serial_driver.hangup = sab82532_hangup;
	serial_driver.break_ctl = sab82532_break;
	serial_driver.wait_until_sent = sab82532_wait_until_sent;
	serial_driver.read_proc = sab82532_read_proc;

	/*
	 * The callout device is just like normal device except for
	 * major number and the subtype code.
	 */
	callout_driver = serial_driver;
#ifdef CONFIG_DEVFS_FS
	callout_driver.name = "cua/%d";
#else
	callout_driver.name = "cua";
#endif
	callout_driver.major = TTYAUX_MAJOR;
	callout_driver.subtype = SERIAL_TYPE_CALLOUT;
	callout_driver.read_proc = 0;
	callout_driver.proc_entry = 0;

	if (tty_register_driver(&serial_driver))
		panic("Couldn't register serial driver\n");
	if (tty_register_driver(&callout_driver))
		panic("Couldn't register callout driver\n");

	for (info = sab82532_chain, i = 0; info; info = info->next, i++) {
		info->magic = SERIAL_MAGIC;

		info->type = readb(&info->regs->r.vstr) & 0x0f;
		writeb(~((1 << 1) | (1 << 2) | (1 << 4)), &info->regs->w.pcr);
		writeb(0xff, &info->regs->w.pim);
		if (info->line == 0) {
			info->pvr_dsr_bit = (1 << 0);
			info->pvr_dtr_bit = (1 << 1);
		} else {
			info->pvr_dsr_bit = (1 << 3);
			info->pvr_dtr_bit = (1 << 2);
		}
		writeb((1 << 1) | (1 << 2) | (1 << 4), &info->regs->w.pvr);
		writeb(readb(&info->regs->rw.mode) | SAB82532_MODE_FRTS, &info->regs->rw.mode);
		writeb(readb(&info->regs->rw.mode) | SAB82532_MODE_RTS, &info->regs->rw.mode);

		info->custom_divisor = 16;
		info->close_delay = 5*HZ/10;
		info->closing_wait = 30*HZ;
		info->tec_timeout = SAB82532_MAX_TEC_TIMEOUT;
		info->cec_timeout = SAB82532_MAX_CEC_TIMEOUT;
		info->x_char = 0;
		info->event = 0;	
		info->blocked_open = 0;
		info->tqueue.routine = do_softint;
		info->tqueue.data = info;
		info->tqueue_hangup.routine = do_serial_hangup;
		info->tqueue_hangup.data = info;
		info->callout_termios = callout_driver.init_termios;
		info->normal_termios = serial_driver.init_termios;
		init_waitqueue_head(&info->open_wait);
		init_waitqueue_head(&info->close_wait);
		init_waitqueue_head(&info->delta_msr_wait);
		info->icount.cts = info->icount.dsr = 
			info->icount.rng = info->icount.dcd = 0;
		info->icount.rx = info->icount.tx = 0;
		info->icount.frame = info->icount.parity = 0;
		info->icount.overrun = info->icount.brk = 0;

		if (!(info->line & 0x01)) {
			if (request_irq(info->irq, sab82532_interrupt, SA_SHIRQ,
					"serial(sab82532)", info)) {
				printk("sab82532: can't get IRQ %x\n",
				       info->irq);
				panic("sab82532 initialization failed");
			}
		}
	
		printk(KERN_INFO
		       "ttyS%02d at 0x%lx (irq = %s) is a SAB82532 %s\n",
		       info->line + su_num_ports, (unsigned long)info->regs,
		       __irq_itoa(info->irq), sab82532_version[info->type]);
	}

#ifdef SERIAL_LOG_DEVICE
	dprint_init(SERIAL_LOG_DEVICE);
#endif
	return 0;
}

int __init sab82532_probe(void)
{
	int node, enode, snode;
	char model[32];
	int len;

        node = prom_getchild(prom_root_node);
	node = prom_searchsiblings(node, "pci");

	/*
	 * Check for SUNW,sabre on Ultra 5/10/AXi.
	 */
	len = prom_getproperty(node, "model", model, sizeof(model));
	if ((len > 0) && !strncmp(model, "SUNW,sabre", len)) {
        	node = prom_getchild(node);
		node = prom_searchsiblings(node, "pci");
	}

	/*
	 * For each PCI bus...
	 */
	while (node) {
		enode = prom_getchild(node);
		enode = prom_searchsiblings(enode, "ebus");

		/*
		 * For each EBus on this PCI...
		 */
		while (enode) {
			int child;

			child = prom_getchild(enode);
			snode = prom_searchsiblings(child, "se");
			if (snode)
				goto found;

			snode = prom_searchsiblings(child, "serial");
			if (snode) {
				char compat[32];
				int clen;

				clen = prom_getproperty(snode, "compatible",
							compat, sizeof(compat));
				if (clen > 0) {
					if (strncmp(compat, "sab82532", 8) == 0)
						goto found;
				}
			}

			enode = prom_getsibling(enode);
			enode = prom_searchsiblings(enode, "ebus");
		}
		node = prom_getsibling(node);
		node = prom_searchsiblings(node, "pci");
	}
	return -ENODEV;

found:
#ifdef CONFIG_SERIAL_CONSOLE
	sunserial_setinitfunc(sab82532_console_init);
#endif
#ifndef MODULE
	sunserial_setinitfunc(sab82532_init);
	rs_ops.rs_kgdb_hook = sab82532_kgdb_hook;
#endif
	return 0;
}

#ifdef MODULE
MODULE_LICENSE("GPL");

int init_module(void)
{
	if (get_sab82532(0))
		return -ENODEV;

	return sab82532_init();
}

void cleanup_module(void) 
{
	struct sab82532 *sab;
	unsigned long flags;
	int e1, e2;

	/* printk("Unloading %s: version %s\n", serial_name, serial_version); */
	save_flags(flags);
	cli();
        remove_bh(SERIAL_BH);
	if ((e1 = tty_unregister_driver(&serial_driver)))
		printk("SERIAL: failed to unregister serial driver (%d)\n",
		       e1);
	if ((e2 = tty_unregister_driver(&callout_driver)))
		printk("SERIAL: failed to unregister callout driver (%d)\n", 
		       e2);
	restore_flags(flags);

	if (tmp_buf) {
		free_page((unsigned long) tmp_buf);
		tmp_buf = NULL;
	}
	for (sab = sab82532_chain; sab; sab = sab->next) {
		if (!(sab->line & 0x01))
			free_irq(sab->irq, sab);
		iounmap(sab->regs);
	}
}
#endif /* MODULE */

#ifdef CONFIG_SERIAL_CONSOLE
static void
batten_down_hatches(struct sab82532 *info)
{
	unsigned char saved_rfc, tmp;

	if (!stop_a_enabled)
		return;

	/* If we are doing kadb, we call the debugger
	 * else we just drop into the boot monitor.
	 * Note that we must flush the user windows
	 * first before giving up control.
	 */
	printk("\n");
	flush_user_windows();

	/*
	 * Set FIFO to single character mode.
	 */
	saved_rfc = readb(&info->regs->r.rfc);
	tmp = readb(&info->regs->rw.rfc);
	tmp &= ~(SAB82532_RFC_RFDF);
	writeb(tmp, &info->regs->rw.rfc);
	sab82532_cec_wait(info);
	writeb(SAB82532_CMDR_RRES, &info->regs->w.cmdr);

#ifndef __sparc_v9__
	if ((((unsigned long)linux_dbvec) >= DEBUG_FIRSTVADDR) &&
	    (((unsigned long)linux_dbvec) <= DEBUG_LASTVADDR))
		sp_enter_debugger();
	else
#endif
		prom_cmdline();

	/*
	 * Reset FIFO to character + status mode.
	 */
	writeb(saved_rfc, &info->regs->w.rfc);
	sab82532_cec_wait(info);
	writeb(SAB82532_CMDR_RRES, &info->regs->w.cmdr);
}

static __inline__ void
sab82532_console_putchar(struct sab82532 *info, char c)
{
	unsigned long flags;

	save_flags(flags); cli();
	sab82532_tec_wait(info);
	writeb(c, &info->regs->w.tic);
	restore_flags(flags);
}

static void
sab82532_console_write(struct console *con, const char *s, unsigned n)
{
	struct sab82532 *info;
	int i;

	info = sab82532_chain;
	for (i = con->index; i; i--) {
		info = info->next;
		if (!info)
			return;
	}

	for (i = 0; i < n; i++) {
		if (*s == '\n')
			sab82532_console_putchar(info, '\r');
		sab82532_console_putchar(info, *s++);
	}
	sab82532_tec_wait(info);
}

static kdev_t
sab82532_console_device(struct console *con)
{
	return MKDEV(TTY_MAJOR, 64 + con->index);
}

static int
sab82532_console_setup(struct console *con, char *options)
{
	static struct tty_struct c_tty;
	static struct termios c_termios;
	struct sab82532 *info;
	tcflag_t	cflag;
	int		i;

	info = sab82532_chain;
	for (i = con->index; i; i--) {
		info = info->next;
		if (!info)
			return -ENODEV;
	}
	info->is_console = 1;

	/*
	 * Initialize the hardware
	 */
	sab82532_init_line(info);

	/*
	 * Finally, enable interrupts
	 */
	info->interrupt_mask0 = SAB82532_IMR0_PERR | SAB82532_IMR0_FERR |
				SAB82532_IMR0_PLLA | SAB82532_IMR0_CDSC;
	writeb(info->interrupt_mask0, &info->regs->w.imr0);
	info->interrupt_mask1 = SAB82532_IMR1_BRKT | SAB82532_IMR1_ALLS |
				SAB82532_IMR1_XOFF | SAB82532_IMR1_TIN |
				SAB82532_IMR1_CSC | SAB82532_IMR1_XON |
				SAB82532_IMR1_XPR;
	writeb(info->interrupt_mask1, &info->regs->w.imr1);

	printk("Console: ttyS%d (SAB82532)\n", info->line);

	sunserial_console_termios(con);
	cflag = con->cflag;

	/*
	 * Fake up the tty and tty->termios structures so we can use
	 * change_speed (and eliminate a lot of duplicate code).
	 */
	if (!info->tty) {
		memset(&c_tty, 0, sizeof(c_tty));
		info->tty = &c_tty;
	}
	if (!info->tty->termios) {
		memset(&c_termios, 0, sizeof(c_termios));
		info->tty->termios = &c_termios;
	}
	info->tty->termios->c_cflag = con->cflag;

	change_speed(info);

	/* Now take out the pointers to static structures if necessary */
	if (info->tty->termios == &c_termios)
		info->tty->termios = 0;
	if (info->tty == &c_tty)
		info->tty = 0;
	
	return 0;
}

static struct console sab82532_console = {
	name:		"ttyS",
	write:		sab82532_console_write,
	device:		sab82532_console_device,
	setup:		sab82532_console_setup,
	flags:		CON_PRINTBUFFER,
	index:		-1,
};

int __init sab82532_console_init(void)
{
	extern int con_is_present(void);
	extern int su_console_registered;

	if (con_is_present() || su_console_registered)
		return 0;

	if (!sab82532_chain) {
		prom_printf("sab82532_console_setup: can't get SAB82532 chain");
		prom_halt();
	}

	sab82532_console.index = serial_console - 1;
	register_console(&sab82532_console);
	return 0;
}

#ifdef SERIAL_LOG_DEVICE

static int serial_log_device = 0;

static void
dprint_init(int tty)
{
	serial_console = tty + 1;
	sab82532_console.index = tty;
	sab82532_console_setup(&sab82532_console, "");
	serial_console = 0;
	serial_log_device = tty + 1;
}

int
dprintf(const char *fmt, ...)
{
	static char buffer[4096];
	va_list args;
	int i;

	if (!serial_log_device)
		return 0;

	va_start(args, fmt);
	i = vsprintf(buffer, fmt, args);
	va_end(args);
	sab82532_console.write(&sab82532_console, buffer, i);
	return i;
}
#endif /* SERIAL_LOG_DEVICE */
#endif /* CONFIG_SERIAL_CONSOLE */
