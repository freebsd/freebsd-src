/*
 *  UART driver for MPC8260 CPM SCC or SMC
 *  Copyright (c) 1999 Dan Malek (dmalek@jlc.net)
 *  Copyright (c) 2000 MontaVista Software, Inc. (source@mvista.com)
 *	2.3.99 updates
 *
 * I used the 8xx uart.c driver as the framework for this driver.
 * The original code was written for the EST8260 board.  I tried to make
 * it generic, but there may be some assumptions in the structures that
 * have to be fixed later.
 *
 * The 8xx and 8260 are similar, but not identical.  Over time we
 * could probably merge these two drivers.
 * To save porting time, I did not bother to change any object names
 * that are not accessed outside of this file.
 * It still needs lots of work........When it was easy, I included code
 * to support the SCCs.
 * Only the SCCs support modem control, so that is not complete either.
 *
 * This module exports the following rs232 io functions:
 *
 *	int rs_8xx_init(void);
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
#include <linux/major.h>
#include <linux/string.h>
#include <linux/fcntl.h>
#include <linux/ptrace.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <asm/immap_cpm2.h>
#include <asm/mpc8260.h>
#include <asm/cpm2.h>
#include <asm/irq.h>

#ifdef CONFIG_MAGIC_SYSRQ
#include <linux/sysrq.h>
#endif

#ifdef CONFIG_SERIAL_CONSOLE
#include <linux/console.h>

/* SCC Console configuration.  Not quite finished.  The SCC_CONSOLE
 * should be the number of the SCC to use, but only SCC1 will
 * work at this time.
 */
#ifdef CONFIG_SCC_CONSOLE
#define SCC_CONSOLE 1
#endif

/* this defines the index into rs_table for the port to use
*/
#ifndef CONFIG_SERIAL_CONSOLE_PORT
#define CONFIG_SERIAL_CONSOLE_PORT	0
#endif
#endif
#define CONFIG_SERIAL_CONSOLE_PORT	0

#define TX_WAKEUP	ASYNC_SHARE_IRQ

static char *serial_name = "CPM UART driver";
static char *serial_version = "0.01";

static DECLARE_TASK_QUEUE(tq_serial);

static struct tty_driver serial_driver, callout_driver;
static int serial_refcount;
static int serial_console_setup(struct console *co, char *options);

static void serial_console_write(struct console *c, const char *s,
		                                unsigned count);
static kdev_t serial_console_device(struct console *c);

#if defined(CONFIG_SERIAL_CONSOLE) && defined(CONFIG_MAGIC_SYSRQ)
static unsigned long break_pressed; /* break, really ... */
#endif

/*
 * Serial driver configuration section.  Here are the various options:
 */
#define SERIAL_PARANOIA_CHECK
#define CONFIG_SERIAL_NOPAUSE_IO
#define SERIAL_DO_RESTART

/* Set of debugging defines */

#undef SERIAL_DEBUG_INTR
#undef SERIAL_DEBUG_OPEN
#undef SERIAL_DEBUG_FLOW
#undef SERIAL_DEBUG_RS_WAIT_UNTIL_SENT

#define _INLINE_ inline

#define DBG_CNT(s)

/* We overload some of the items in the data structure to meet our
 * needs.  For example, the port address is the CPM parameter ram
 * offset for the SCC or SMC.  The maximum number of ports is 4 SCCs and
 * 2 SMCs.  The "hub6" field is used to indicate the channel number, with
 * 0 and 1 indicating the SMCs and 2, 3, 4, and 5 are the SCCs.
 * Since these ports are so versatile, I don't yet have a strategy for
 * their management.  For example, SCC1 is used for Ethernet.  Right
 * now, just don't put them in the table.  Of course, right now I just
 * want the SMC to work as a uart :-)..
 * The "type" field is currently set to 0, for PORT_UNKNOWN.  It is
 * not currently used.  I should probably use it to indicate the port
 * type of CMS or SCC.
 * The SMCs do not support any modem control signals.
 */
#define smc_scc_num	hub6

/* The choice of serial port to use for KGDB.  If the system has
 * two ports, you can use one for console and one for KGDB (which
 * doesn't make sense to me, but people asked for it).
 */
#ifdef CONFIG_KGDB_TTYS1
#define KGDB_SER_IDX 1		/* SCC2/SMC2 */
#else
#define KGDB_SER_IDX 0		/* SCC1/SMC1 */
#endif

#ifndef SCC_CONSOLE

/* SMC2 is sometimes used for low performance TDM interfaces.  Define
 * this as 1 if you want SMC2 as a serial port UART managed by this driver.
 * Define this as 0 if you wish to use SMC2 for something else.
 */
#define USE_SMC2 1

/* Define SCC to ttySx mapping.
*/
#define SCC_NUM_BASE	(USE_SMC2 + 1)	/* SCC base tty "number" */

/* Define which SCC is the first one to use for a serial port.  These
 * are 0-based numbers, i.e. this assumes the first SCC (SCC1) is used
 * for Ethernet, and the first available SCC for serial UART is SCC2.
 * NOTE:  IF YOU CHANGE THIS, you have to change the PROFF_xxx and
 * interrupt vectors in the table below to match.
 */
#define SCC_IDX_BASE	1	/* table index */

static struct serial_state rs_table[] = {
	/* UART CLK   PORT          IRQ      FLAGS  NUM   */
	{ 0,     0, PROFF_SMC1, SIU_INT_SMC1,   0,    0 },    /* SMC1 ttyS0 */
#if USE_SMC2
	{ 0,     0, PROFF_SMC2, SIU_INT_SMC2,   0,    1 },    /* SMC2 ttyS1 */
#endif
#ifndef CONFIG_SCC1_ENET
	{ 0,     0, PROFF_SCC1, SIU_INT_SCC1,   0, SCC_NUM_BASE},    /* SCC1 ttyS2 */
#endif
#ifndef CONFIG_SCC2_ENET
	{ 0,     0, PROFF_SCC2, SIU_INT_SCC2,   0, SCC_NUM_BASE + 1},    /* SCC2 ttyS3 */
#endif
};

#else /* SCC_CONSOLE */
#define SCC_NUM_BASE	0	/* SCC base tty "number" */
#define SCC_IDX_BASE	0	/* table index */
static struct serial_state rs_table[] = {
	/* UART CLK   PORT          IRQ      FLAGS  NUM   */
	{ 0,     0, PROFF_SCC1, SIU_INT_SCC1,   0, SCC_NUM_BASE},    /* SCC1 ttyS2 */
	{ 0,     0, PROFF_SCC2, SIU_INT_SCC2,   0, SCC_NUM_BASE + 1},    /* SCC2 ttyS3 */
};
#endif /* SCC_CONSOLE */

#define PORT_NUM(P)	(((P) < (SCC_NUM_BASE)) ? (P) : (P)-(SCC_NUM_BASE))

#define NR_PORTS	(sizeof(rs_table)/sizeof(struct serial_state))

static struct tty_struct *serial_table[NR_PORTS];
static struct termios *serial_termios[NR_PORTS];
static struct termios *serial_termios_locked[NR_PORTS];

/* The number of buffer descriptors and their sizes.
*/
#define RX_NUM_FIFO	4
#define RX_BUF_SIZE	32
#define TX_NUM_FIFO	4
#define TX_BUF_SIZE	32

#ifndef MIN
#define MIN(a,b)	((a) < (b) ? (a) : (b))
#endif

/* The async_struct in serial.h does not really give us what we
 * need, so define our own here.
 */
typedef struct serial_info {
	int			magic;
	int			flags;
	struct serial_state	*state;
	struct tty_struct 	*tty;
	int			read_status_mask;
	int			ignore_status_mask;
	int			timeout;
	int			line;
	int			x_char;	/* xon/xoff character */
	int			close_delay;
	unsigned short		closing_wait;
	unsigned short		closing_wait2;
	unsigned long		event;
	unsigned long		last_active;
	int			blocked_open; /* # of blocked opens */
	long			session; /* Session of opening process */
	long			pgrp; /* pgrp of opening process */
	struct tq_struct	tqueue;
	struct tq_struct	tqueue_hangup;
	wait_queue_head_t	open_wait;
	wait_queue_head_t	close_wait;

	/* CPM Buffer Descriptor pointers.
	*/
	cbd_t			*rx_bd_base;
	cbd_t			*rx_cur;
	cbd_t			*tx_bd_base;
	cbd_t			*tx_cur;
} ser_info_t;

static struct console sercons = {
        name:           "ttyS",
        write:          serial_console_write,
        device:         serial_console_device,
        setup:          serial_console_setup,
        flags:          CON_PRINTBUFFER,
        index:          CONFIG_SERIAL_CONSOLE_PORT,
};


static void change_speed(ser_info_t *info);
static void rs_8xx_wait_until_sent(struct tty_struct *tty, int timeout);

static inline int serial_paranoia_check(ser_info_t *info,
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
 * This is used to figure out the divisor speeds and the timeouts,
 * indexed by the termio value.  The generic CPM functions are responsible
 * for setting and assigning baud rate generators for us.
 */
static int baud_table[] = {
	0, 50, 75, 110, 134, 150, 200, 300, 600, 1200, 1800, 2400, 4800,
	9600, 19200, 38400, 57600, 115200, 230400, 460800, 0 };


/*
 * ------------------------------------------------------------
 * rs_stop() and rs_start()
 *
 * This routines are called before setting or resetting tty->stopped.
 * They enable or disable transmitter interrupts, as necessary.
 * ------------------------------------------------------------
 */
static void rs_8xx_stop(struct tty_struct *tty)
{
	ser_info_t *info = (ser_info_t *)tty->driver_data;
	int	idx;
	unsigned long flags;
	volatile scc_t	*sccp;
	volatile smc_t	*smcp;

	if (serial_paranoia_check(info, tty->device, "rs_stop"))
		return;

	save_flags(flags); cli();
	if ((idx = info->state->smc_scc_num) < SCC_NUM_BASE) {
		smcp = &cpm2_immr->im_smc[idx];
		smcp->smc_smcm &= ~SMCM_TX;
	}
	else {
		sccp = &cpm2_immr->im_scc[idx - SCC_IDX_BASE];
		sccp->scc_sccm &= ~UART_SCCM_TX;
	}
	restore_flags(flags);
}

static void rs_8xx_start(struct tty_struct *tty)
{
	ser_info_t *info = (ser_info_t *)tty->driver_data;
	int	idx;
	unsigned long flags;
	volatile scc_t	*sccp;
	volatile smc_t	*smcp;

	if (serial_paranoia_check(info, tty->device, "rs_stop"))
		return;

	save_flags(flags); cli();
	if ((idx = info->state->smc_scc_num) < SCC_NUM_BASE) {
		smcp = &cpm2_immr->im_smc[idx];
		smcp->smc_smcm |= SMCM_TX;
	}
	else {
		sccp = &cpm2_immr->im_scc[idx - SCC_IDX_BASE];
		sccp->scc_sccm |= UART_SCCM_TX;
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
static _INLINE_ void rs_sched_event(ser_info_t *info,
				  int event)
{
	info->event |= 1 << event;
	queue_task(&info->tqueue, &tq_serial);
	mark_bh(SERIAL_BH);
}

static _INLINE_ void receive_chars(ser_info_t *info, struct pt_regs *regs)
{
	struct tty_struct *tty = info->tty;
	unsigned char ch, *cp;
	/*int	ignored = 0;*/
	int	i;
	ushort	status;
	struct	async_icount *icount;
	volatile cbd_t	*bdp;

	icount = &info->state->icount;

	/* Just loop through the closed BDs and copy the characters into
	 * the buffer.
	 */
	bdp = info->rx_cur;
	for (;;) {
		if (bdp->cbd_sc & BD_SC_EMPTY)	/* If this one is empty */
			break;			/*   we are all done */

		/* The read status mask tell us what we should do with
		 * incoming characters, especially if errors occur.
		 * One special case is the use of BD_SC_EMPTY.  If
		 * this is not set, we are supposed to be ignoring
		 * inputs.  In this case, just mark the buffer empty and
		 * continue.
		if (!(info->read_status_mask & BD_SC_EMPTY)) {
			bdp->cbd_sc |= BD_SC_EMPTY;
			bdp->cbd_sc &=
				~(BD_SC_BR | BD_SC_FR | BD_SC_PR | BD_SC_OV);

			if (bdp->cbd_sc & BD_SC_WRAP)
				bdp = info->rx_bd_base;
			else
				bdp++;
			continue;
		}
		 */

		/* Get the number of characters and the buffer pointer.
		*/
		i = bdp->cbd_datlen;
		cp = (unsigned char *)__va(bdp->cbd_bufaddr);
		status = bdp->cbd_sc;
#ifdef CONFIG_KGDB
		if (info->state->smc_scc_num == KGDB_SER_IDX &&
				(*cp == 0x03 || *cp == '$')) {
			breakpoint();
			return;
		}
#endif

		/* Check to see if there is room in the tty buffer for
		 * the characters in our BD buffer.  If not, we exit
		 * now, leaving the BD with the characters.  We'll pick
		 * them up again on the next receive interrupt (which could
		 * be a timeout).
		 */
		if ((tty->flip.count + i) >= TTY_FLIPBUF_SIZE)
			break;

		while (i-- > 0) {
			ch = *cp++;
			*tty->flip.char_buf_ptr = ch;
			icount->rx++;

#ifdef SERIAL_DEBUG_INTR
			printk("DR%02x:%02x...", ch, *status);
#endif
			*tty->flip.flag_buf_ptr = 0;
			if (status & (BD_SC_BR | BD_SC_FR |
				       BD_SC_PR | BD_SC_OV)) {
				/*
				 * For statistics only
				 */
				if (status & BD_SC_BR)
					icount->brk++;
				else if (status & BD_SC_PR)
					icount->parity++;
				else if (status & BD_SC_FR)
					icount->frame++;
				if (status & BD_SC_OV)
					icount->overrun++;

				/*
				 * Now check to see if character should be
				 * ignored, and mask off conditions which
				 * should be ignored.
				if (status & info->ignore_status_mask) {
					if (++ignored > 100)
						break;
					continue;
				}
				 */
				status &= info->read_status_mask;

				if (status & (BD_SC_BR)) {
#ifdef SERIAL_DEBUG_INTR
					printk("handling break....");
#endif
					*tty->flip.flag_buf_ptr = TTY_BREAK;
					if (info->flags & ASYNC_SAK)
						do_SAK(tty);
				} else if (status & BD_SC_PR)
					*tty->flip.flag_buf_ptr = TTY_PARITY;
				else if (status & BD_SC_FR)
					*tty->flip.flag_buf_ptr = TTY_FRAME;
				if (status & BD_SC_OV) {
					/*
					 * Overrun is special, since it's
					 * reported immediately, and doesn't
					 * affect the current character
					 */
					if (tty->flip.count < TTY_FLIPBUF_SIZE) {
						tty->flip.count++;
						tty->flip.flag_buf_ptr++;
						tty->flip.char_buf_ptr++;
						*tty->flip.flag_buf_ptr =
								TTY_OVERRUN;
					}
				}
			}

#if defined(CONFIG_SERIAL_CONSOLE) && defined(CONFIG_MAGIC_SYSRQ)
			if (break_pressed && info->line == sercons.index) {
				if (ch != 0 && time_before(jiffies,
							break_pressed + HZ*5)) {
					handle_sysrq(ch, regs, NULL, NULL);
					break_pressed = 0;
					goto ignore_char;
				} else
					break_pressed = 0;
			}
#endif
			
			if (tty->flip.count >= TTY_FLIPBUF_SIZE)
				break;

			tty->flip.flag_buf_ptr++;
			tty->flip.char_buf_ptr++;
			tty->flip.count++;
		}

#if defined(CONFIG_SERIAL_CONSOLE) && defined(CONFIG_MAGIC_SYSRQ)
	ignore_char:
#endif

		/* This BD is ready to be used again.  Clear status.
		 * Get next BD.
		 */
		bdp->cbd_sc |= BD_SC_EMPTY;
		bdp->cbd_sc &= ~(BD_SC_BR | BD_SC_FR | BD_SC_PR | BD_SC_OV);

		if (bdp->cbd_sc & BD_SC_WRAP)
			bdp = info->rx_bd_base;
		else
			bdp++;
	}

	info->rx_cur = (cbd_t *)bdp;

	queue_task(&tty->flip.tqueue, &tq_timer);
}

static _INLINE_ void receive_break(ser_info_t *info, struct pt_regs *regs)
{
	struct tty_struct *tty = info->tty;

	info->state->icount.brk++;

#if defined(CONFIG_SERIAL_CONSOLE) && defined(CONFIG_MAGIC_SYSRQ)
	if (info->line == sercons.index) {
		if (!break_pressed) {
			break_pressed = jiffies;
			return;
		} else
			break_pressed = 0;
	}
#endif

	/* Check to see if there is room in the tty buffer for
	 * the break.  If not, we exit now, losing the break.  FIXME
	 */
	if ((tty->flip.count + 1) >= TTY_FLIPBUF_SIZE)
		return;
	*(tty->flip.flag_buf_ptr++) = TTY_BREAK;
	*(tty->flip.char_buf_ptr++) = 0;
	tty->flip.count++;

	queue_task(&tty->flip.tqueue, &tq_timer);
}


static _INLINE_ void transmit_chars(ser_info_t *info, struct pt_regs *regs)
{

	if (info->flags & TX_WAKEUP) {
		rs_sched_event(info, RS_EVENT_WRITE_WAKEUP);
	}

#ifdef SERIAL_DEBUG_INTR
	printk("THRE...");
#endif
}

#ifdef notdef
	/* I need to do this for the SCCs, so it is left as a reminder.
	*/
static _INLINE_ void check_modem_status(struct async_struct *info)
{
	int	status;
	struct	async_icount *icount;

	status = serial_in(info, UART_MSR);

	if (status & UART_MSR_ANY_DELTA) {
		icount = &info->state->icount;
		/* update input line counters */
		if (status & UART_MSR_TERI)
			icount->rng++;
		if (status & UART_MSR_DDSR)
			icount->dsr++;
		if (status & UART_MSR_DDCD) {
			icount->dcd++;
#ifdef CONFIG_HARD_PPS
			if ((info->flags & ASYNC_HARDPPS_CD) &&
			    (status & UART_MSR_DCD))
				hardpps();
#endif
		}
		if (status & UART_MSR_DCTS)
			icount->cts++;
		wake_up_interruptible(&info->delta_msr_wait);
	}

	if ((info->flags & ASYNC_CHECK_CD) && (status & UART_MSR_DDCD)) {
#if (defined(SERIAL_DEBUG_OPEN) || defined(SERIAL_DEBUG_INTR))
		printk("ttys%d CD now %s...", info->line,
		       (status & UART_MSR_DCD) ? "on" : "off");
#endif
		if (status & UART_MSR_DCD)
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
			if (status & UART_MSR_CTS) {
#if (defined(SERIAL_DEBUG_INTR) || defined(SERIAL_DEBUG_FLOW))
				printk("CTS tx start...");
#endif
				info->tty->hw_stopped = 0;
				info->IER |= UART_IER_THRI;
				serial_out(info, UART_IER, info->IER);
				rs_sched_event(info, RS_EVENT_WRITE_WAKEUP);
				return;
			}
		} else {
			if (!(status & UART_MSR_CTS)) {
#if (defined(SERIAL_DEBUG_INTR) || defined(SERIAL_DEBUG_FLOW))
				printk("CTS tx stop...");
#endif
				info->tty->hw_stopped = 1;
				info->IER &= ~UART_IER_THRI;
				serial_out(info, UART_IER, info->IER);
			}
		}
	}
}
#endif

/*
 * This is the serial driver's interrupt routine for a single port
 */
static void rs_8xx_interrupt(int irq, void * dev_id, struct pt_regs * regs)
{
	u_char	events;
	int	idx;
	ser_info_t *info;
	volatile smc_t	*smcp;
	volatile scc_t	*sccp;

	info = (ser_info_t *)dev_id;

	if ((idx = info->state->smc_scc_num) < SCC_NUM_BASE) {
		smcp = &cpm2_immr->im_smc[idx];
		events = smcp->smc_smce;
		if (events & SMCM_BRKE)
			receive_break(info, regs);
		if (events & SMCM_RX)
			receive_chars(info, regs);
		if (events & SMCM_TX)
			transmit_chars(info, regs);
		smcp->smc_smce = events;
	}
	else {
		sccp = &cpm2_immr->im_scc[idx - SCC_IDX_BASE];
		events = sccp->scc_scce;
		if (events & SMCM_BRKE)
			receive_break(info, regs);
		if (events & SCCM_RX)
			receive_chars(info, regs);
		if (events & SCCM_TX)
			transmit_chars(info, regs);
		sccp->scc_scce = events;
	}

#ifdef SERIAL_DEBUG_INTR
	printk("rs_interrupt_single(%d, %x)...",
					info->state->smc_scc_num, events);
#endif
#ifdef modem_control
	check_modem_status(info);
#endif
	info->last_active = jiffies;
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
	ser_info_t	*info = (ser_info_t *) private_;
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
 * This routine is called from the scheduler tqueue when the interrupt
 * routine has signalled that a hangup has occurred.  The path of
 * hangup processing is:
 *
 * 	serial interrupt routine -> (scheduler tqueue) ->
 * 	do_serial_hangup() -> tty->hangup() -> rs_hangup()
 *
 */
static void do_serial_hangup(void *private_)
{
	struct async_struct	*info = (struct async_struct *) private_;
	struct tty_struct	*tty;

	tty = info->tty;
	if (tty)
		tty_hangup(tty);
	MOD_DEC_USE_COUNT;
}

/*static void rs_8xx_timer(void)
{
	printk("rs_8xx_timer\n");
}*/


static int startup(ser_info_t *info)
{
	unsigned long flags;
	int	retval=0;
	int	idx;
	struct serial_state *state= info->state;
	volatile smc_t		*smcp;
	volatile scc_t		*sccp;
	volatile smc_uart_t	*up;
	volatile scc_uart_t	*scup;


	save_flags(flags); cli();

	if (info->flags & ASYNC_INITIALIZED) {
		goto errout;
	}

#ifdef maybe
	if (!state->port || !state->type) {
		if (info->tty)
			set_bit(TTY_IO_ERROR, &info->tty->flags);
		goto errout;
	}
#endif

#ifdef SERIAL_DEBUG_OPEN
	printk("starting up ttys%d (irq %d)...", info->line, state->irq);
#endif


#ifdef modem_control
	info->MCR = 0;
	if (info->tty->termios->c_cflag & CBAUD)
		info->MCR = UART_MCR_DTR | UART_MCR_RTS;
#endif

	if (info->tty)
		clear_bit(TTY_IO_ERROR, &info->tty->flags);

	/*
	 * and set the speed of the serial port
	 */
	change_speed(info);

	if ((idx = info->state->smc_scc_num) < SCC_NUM_BASE) {
		smcp = &cpm2_immr->im_smc[idx];

		/* Enable interrupts and I/O.
		*/
		smcp->smc_smcm |= (SMCM_RX | SMCM_TX);
		smcp->smc_smcmr |= (SMCMR_REN | SMCMR_TEN);

		/* We can tune the buffer length and idle characters
		 * to take advantage of the entire incoming buffer size.
		 * If mrblr is something other than 1, maxidl has to be
		 * non-zero or we never get an interrupt.  The maxidl
		 * is the number of character times we wait after reception
		 * of the last character before we decide no more characters
		 * are coming.
		 */
		up = (smc_uart_t *)&cpm2_immr->im_dprambase[state->port];
#if 0
		up->smc_mrblr = 1;	/* receive buffer length */
		up->smc_maxidl = 0;	/* wait forever for next char */
#else
		up->smc_mrblr = RX_BUF_SIZE;
		up->smc_maxidl = RX_BUF_SIZE;
#endif
		up->smc_brkcr = 1;	/* number of break chars */
	}
	else {
		sccp = &cpm2_immr->im_scc[idx - SCC_IDX_BASE];
		scup = (scc_uart_t *)&cpm2_immr->im_dprambase[state->port];
#if 0
		scup->scc_genscc.scc_mrblr = 1;	/* receive buffer length */
		scup->scc_maxidl = 0;	/* wait forever for next char */
#else
		scup->scc_genscc.scc_mrblr = RX_BUF_SIZE;
		scup->scc_maxidl = RX_BUF_SIZE;
#endif

		sccp->scc_sccm |= (UART_SCCM_TX | UART_SCCM_RX);
		sccp->scc_gsmrl |= (SCC_GSMRL_ENR | SCC_GSMRL_ENT);
	}

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
static void shutdown(ser_info_t * info)
{
	unsigned long	flags;
	struct serial_state *state;
	int		idx;
	volatile smc_t	*smcp;
	volatile scc_t	*sccp;

	if (!(info->flags & ASYNC_INITIALIZED))
		return;

	state = info->state;

#ifdef SERIAL_DEBUG_OPEN
	printk("Shutting down serial port %d (irq %d)....", info->line,
	       state->irq);
#endif

	save_flags(flags); cli(); /* Disable interrupts */

	if ((idx = info->state->smc_scc_num) < SCC_NUM_BASE) {
		smcp = &cpm2_immr->im_smc[idx];

		/* Disable interrupts and I/O.
		*/
		smcp->smc_smcm &= ~(SMCM_RX | SMCM_TX);
#ifdef CONFIG_SERIAL_CONSOLE
		/* We can't disable the transmitter if this is the
		 * system console.
		 */
		if (idx != CONFIG_SERIAL_CONSOLE_PORT)
#endif
			smcp->smc_smcmr &= ~(SMCMR_REN | SMCMR_TEN);
	}
	else {
		sccp = &cpm2_immr->im_scc[idx - SCC_IDX_BASE];
		sccp->scc_sccm &= ~(UART_SCCM_TX | UART_SCCM_RX);
#ifdef CONFIG_SERIAL_CONSOLE
		if (idx != CONFIG_SERIAL_CONSOLE_PORT)
			sccp->scc_gsmrl &= ~(SCC_GSMRL_ENR | SCC_GSMRL_ENT);
#endif
	}

	if (info->tty)
		set_bit(TTY_IO_ERROR, &info->tty->flags);

	info->flags &= ~ASYNC_INITIALIZED;
	restore_flags(flags);
}

/*
 * This routine is called to set the UART divisor registers to match
 * the specified baud rate for a serial port.
 */
static void change_speed(ser_info_t *info)
{
	int	baud_rate;
	unsigned cflag, cval, scval, prev_mode;
	int	i, bits, sbits, idx;
	unsigned long	flags;
	volatile smc_t	*smcp;
	volatile scc_t	*sccp;

	if (!info->tty || !info->tty->termios)
		return;
	cflag = info->tty->termios->c_cflag;

	/* Character length programmed into the mode register is the
	 * sum of: 1 start bit, number of data bits, 0 or 1 parity bit,
	 * 1 or 2 stop bits, minus 1.
	 * The value 'bits' counts this for us.
	 */
	cval = 0;
	scval = 0;

	/* byte size and parity */
	switch (cflag & CSIZE) {
	      case CS5: bits = 5; break;
	      case CS6: bits = 6; break;
	      case CS7: bits = 7; break;
	      case CS8: bits = 8; break;
	      /* Never happens, but GCC is too dumb to figure it out */
	      default:  bits = 8; break;
	}
	sbits = bits - 5;

	if (cflag & CSTOPB) {
		cval |= SMCMR_SL;	/* Two stops */
		scval |= SCU_PSMR_SL;
		bits++;
	}
	if (cflag & PARENB) {
		cval |= SMCMR_PEN;
		scval |= SCU_PSMR_PEN;
		bits++;
	}
	if (!(cflag & PARODD)) {
		cval |= SMCMR_PM_EVEN;
		scval |= (SCU_PSMR_REVP | SCU_PSMR_TEVP);
	}

	/* Determine divisor based on baud rate */
	i = cflag & CBAUD;
	if (i >= (sizeof(baud_table)/sizeof(int)))
		baud_rate = 9600;
	else
		baud_rate = baud_table[i];

	info->timeout = (TX_BUF_SIZE*HZ*bits);
	info->timeout += HZ/50;		/* Add .02 seconds of slop */

#ifdef modem_control
	/* CTS flow control flag and modem status interrupts */
	info->IER &= ~UART_IER_MSI;
	if (info->flags & ASYNC_HARDPPS_CD)
		info->IER |= UART_IER_MSI;
	if (cflag & CRTSCTS) {
		info->flags |= ASYNC_CTS_FLOW;
		info->IER |= UART_IER_MSI;
	} else
		info->flags &= ~ASYNC_CTS_FLOW;
	if (cflag & CLOCAL)
		info->flags &= ~ASYNC_CHECK_CD;
	else {
		info->flags |= ASYNC_CHECK_CD;
		info->IER |= UART_IER_MSI;
	}
	serial_out(info, UART_IER, info->IER);
#endif

	/*
	 * Set up parity check flag
	 */
#define RELEVANT_IFLAG(iflag) (iflag & (IGNBRK|BRKINT|IGNPAR|PARMRK|INPCK))

	info->read_status_mask = (BD_SC_EMPTY | BD_SC_OV);
	if (I_INPCK(info->tty))
		info->read_status_mask |= BD_SC_FR | BD_SC_PR;
	if (I_BRKINT(info->tty) || I_PARMRK(info->tty))
		info->read_status_mask |= BD_SC_BR;

	/*
	 * Characters to ignore
	 */
	info->ignore_status_mask = 0;
	if (I_IGNPAR(info->tty))
		info->ignore_status_mask |= BD_SC_PR | BD_SC_FR;
	if (I_IGNBRK(info->tty)) {
		info->ignore_status_mask |= BD_SC_BR;
		/*
		 * If we're ignore parity and break indicators, ignore
		 * overruns too.  (For real raw support).
		 */
		if (I_IGNPAR(info->tty))
			info->ignore_status_mask |= BD_SC_OV;
	}
	/*
	 * !!! ignore all characters if CREAD is not set
	 */
	if ((cflag & CREAD) == 0)
		info->read_status_mask &= ~BD_SC_EMPTY;
	save_flags(flags); cli();

	/* Start bit has not been added (so don't, because we would just
	 * subtract it later), and we need to add one for the number of
	 * stops bits (there is always at least one).
	 */
	bits++;
	if ((idx = info->state->smc_scc_num) < SCC_NUM_BASE) {
		smcp = &cpm2_immr->im_smc[idx];

		/* Set the mode register.  We want to keep a copy of the
		 * enables, because we want to put them back if they were
		 * present.
		 */
		prev_mode = smcp->smc_smcmr & (SMCMR_REN | SMCMR_TEN);
		smcp->smc_smcmr = smcr_mk_clen(bits) | cval | SMCMR_SM_UART
			| prev_mode;
	}
	else {
		sccp = &cpm2_immr->im_scc[idx - SCC_IDX_BASE];
		sccp->scc_psmr = (sbits << 12) | scval;
	}

	cpm2_setbrg(info->state->smc_scc_num, baud_rate);

	restore_flags(flags);
}

static void rs_8xx_put_char(struct tty_struct *tty, unsigned char ch)
{
	ser_info_t *info = (ser_info_t *)tty->driver_data;
	volatile cbd_t	*bdp;

	if (serial_paranoia_check(info, tty->device, "rs_put_char"))
		return;

	if (!tty)
		return;

	bdp = info->tx_cur;
	while (bdp->cbd_sc & BD_SC_READY);

	*((char *)__va(bdp->cbd_bufaddr)) = ch;
	bdp->cbd_datlen = 1;
	bdp->cbd_sc |= BD_SC_READY;

	/* Get next BD.
	*/
	if (bdp->cbd_sc & BD_SC_WRAP)
		bdp = info->tx_bd_base;
	else
		bdp++;

	info->tx_cur = (cbd_t *)bdp;

}

static int rs_8xx_write(struct tty_struct * tty, int from_user,
		    const unsigned char *buf, int count)
{
	int	c, ret = 0;
	ser_info_t *info = (ser_info_t *)tty->driver_data;
	volatile cbd_t *bdp;

	if (serial_paranoia_check(info, tty->device, "rs_write"))
		return 0;

	if (!tty)
		return 0;

	bdp = info->tx_cur;

	while (1) {
		c = MIN(count, TX_BUF_SIZE);

		if (c <= 0)
			break;

		if (bdp->cbd_sc & BD_SC_READY) {
			info->flags |= TX_WAKEUP;
			break;
		}

		if (from_user) {
			if (copy_from_user(__va(bdp->cbd_bufaddr), buf, c)) {
				if (!ret)
					ret = -EFAULT;
				break;
			}
		} else {
			memcpy(__va(bdp->cbd_bufaddr), buf, c);
		}

		bdp->cbd_datlen = c;
		bdp->cbd_sc |= BD_SC_READY;

		buf += c;
		count -= c;
		ret += c;

		/* Get next BD.
		*/
		if (bdp->cbd_sc & BD_SC_WRAP)
			bdp = info->tx_bd_base;
		else
			bdp++;
		info->tx_cur = (cbd_t *)bdp;
	}
	return ret;
}

static int rs_8xx_write_room(struct tty_struct *tty)
{
	ser_info_t *info = (ser_info_t *)tty->driver_data;
	int	ret;

	if (serial_paranoia_check(info, tty->device, "rs_write_room"))
		return 0;

	if ((info->tx_cur->cbd_sc & BD_SC_READY) == 0) {
		info->flags &= ~TX_WAKEUP;
		ret = TX_BUF_SIZE;
	}
	else {
		info->flags |= TX_WAKEUP;
		ret = 0;
	}
	return ret;
}

/* I could track this with transmit counters....maybe later.
*/
static int rs_8xx_chars_in_buffer(struct tty_struct *tty)
{
	ser_info_t *info = (ser_info_t *)tty->driver_data;

	if (serial_paranoia_check(info, tty->device, "rs_chars_in_buffer"))
		return 0;
	return 0;
}

static void rs_8xx_flush_buffer(struct tty_struct *tty)
{
	ser_info_t *info = (ser_info_t *)tty->driver_data;

	if (serial_paranoia_check(info, tty->device, "rs_flush_buffer"))
		return;

	/* There is nothing to "flush", whatever we gave the CPM
	 * is on its way out.
	 */
	wake_up_interruptible(&tty->write_wait);
	if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) &&
	    tty->ldisc.write_wakeup)
		(tty->ldisc.write_wakeup)(tty);
	info->flags &= ~TX_WAKEUP;
}

/*
 * This function is used to send a high-priority XON/XOFF character to
 * the device
 */
static void rs_8xx_send_xchar(struct tty_struct *tty, char ch)
{
	volatile cbd_t	*bdp;

	ser_info_t *info = (ser_info_t *)tty->driver_data;

	if (serial_paranoia_check(info, tty->device, "rs_send_char"))
		return;

	bdp = info->tx_cur;
	while (bdp->cbd_sc & BD_SC_READY);

	*((char *)__va(bdp->cbd_bufaddr)) = ch;
	bdp->cbd_datlen = 1;
	bdp->cbd_sc |= BD_SC_READY;

	/* Get next BD.
	*/
	if (bdp->cbd_sc & BD_SC_WRAP)
		bdp = info->tx_bd_base;
	else
		bdp++;

	info->tx_cur = (cbd_t *)bdp;
}

/*
 * ------------------------------------------------------------
 * rs_throttle()
 *
 * This routine is called by the upper-layer tty layer to signal that
 * incoming characters should be throttled.
 * ------------------------------------------------------------
 */
static void rs_8xx_throttle(struct tty_struct * tty)
{
	ser_info_t *info = (ser_info_t *)tty->driver_data;
#ifdef SERIAL_DEBUG_THROTTLE
	char	buf[64];

	printk("throttle %s: %d....\n", _tty_name(tty, buf),
	       tty->ldisc.chars_in_buffer(tty));
#endif

	if (serial_paranoia_check(info, tty->device, "rs_throttle"))
		return;

	if (I_IXOFF(tty))
		rs_8xx_send_xchar(tty, STOP_CHAR(tty));

#ifdef modem_control
	if (tty->termios->c_cflag & CRTSCTS)
		info->MCR &= ~UART_MCR_RTS;

	cli();
	serial_out(info, UART_MCR, info->MCR);
	sti();
#endif
}

static void rs_8xx_unthrottle(struct tty_struct * tty)
{
	ser_info_t *info = (ser_info_t *)tty->driver_data;
#ifdef SERIAL_DEBUG_THROTTLE
	char	buf[64];

	printk("unthrottle %s: %d....\n", _tty_name(tty, buf),
	       tty->ldisc.chars_in_buffer(tty));
#endif

	if (serial_paranoia_check(info, tty->device, "rs_unthrottle"))
		return;

	if (I_IXOFF(tty)) {
		if (info->x_char)
			info->x_char = 0;
		else
			rs_8xx_send_xchar(tty, START_CHAR(tty));
	}
#ifdef modem_control
	if (tty->termios->c_cflag & CRTSCTS)
		info->MCR |= UART_MCR_RTS;
	cli();
	serial_out(info, UART_MCR, info->MCR);
	sti();
#endif
}

/*
 * ------------------------------------------------------------
 * rs_ioctl() and friends
 * ------------------------------------------------------------
 */

#ifdef maybe
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
	unsigned char status;
	unsigned int result;

	cli();
	status = serial_in(info, UART_LSR);
	sti();
	result = ((status & UART_LSR_TEMT) ? TIOCSER_TEMT : 0);
	return put_user(result,value);
}
#endif

static int get_modem_info(ser_info_t *info, unsigned int *value)
{
	unsigned int result = 0;
#ifdef modem_control
	unsigned char control, status;

	control = info->MCR;
	cli();
	status = serial_in(info, UART_MSR);
	sti();
	result =  ((control & UART_MCR_RTS) ? TIOCM_RTS : 0)
		| ((control & UART_MCR_DTR) ? TIOCM_DTR : 0)
#ifdef TIOCM_OUT1
		| ((control & UART_MCR_OUT1) ? TIOCM_OUT1 : 0)
		| ((control & UART_MCR_OUT2) ? TIOCM_OUT2 : 0)
#endif
		| ((status  & UART_MSR_DCD) ? TIOCM_CAR : 0)
		| ((status  & UART_MSR_RI) ? TIOCM_RNG : 0)
		| ((status  & UART_MSR_DSR) ? TIOCM_DSR : 0)
		| ((status  & UART_MSR_CTS) ? TIOCM_CTS : 0);
#endif
	return put_user(result,value);
}

static int set_modem_info(ser_info_t *info, unsigned int cmd,
			  unsigned int *value)
{
	int error;
	unsigned int arg;

	error = get_user(arg, value);
	if (error)
		return error;
#ifdef modem_control
	switch (cmd) {
	case TIOCMBIS:
		if (arg & TIOCM_RTS)
			info->MCR |= UART_MCR_RTS;
		if (arg & TIOCM_DTR)
			info->MCR |= UART_MCR_DTR;
#ifdef TIOCM_OUT1
		if (arg & TIOCM_OUT1)
			info->MCR |= UART_MCR_OUT1;
		if (arg & TIOCM_OUT2)
			info->MCR |= UART_MCR_OUT2;
#endif
		break;
	case TIOCMBIC:
		if (arg & TIOCM_RTS)
			info->MCR &= ~UART_MCR_RTS;
		if (arg & TIOCM_DTR)
			info->MCR &= ~UART_MCR_DTR;
#ifdef TIOCM_OUT1
		if (arg & TIOCM_OUT1)
			info->MCR &= ~UART_MCR_OUT1;
		if (arg & TIOCM_OUT2)
			info->MCR &= ~UART_MCR_OUT2;
#endif
		break;
	case TIOCMSET:
		info->MCR = ((info->MCR & ~(UART_MCR_RTS |
#ifdef TIOCM_OUT1
					    UART_MCR_OUT1 |
					    UART_MCR_OUT2 |
#endif
					    UART_MCR_DTR))
			     | ((arg & TIOCM_RTS) ? UART_MCR_RTS : 0)
#ifdef TIOCM_OUT1
			     | ((arg & TIOCM_OUT1) ? UART_MCR_OUT1 : 0)
			     | ((arg & TIOCM_OUT2) ? UART_MCR_OUT2 : 0)
#endif
			     | ((arg & TIOCM_DTR) ? UART_MCR_DTR : 0));
		break;
	default:
		return -EINVAL;
	}
	cli();
	serial_out(info, UART_MCR, info->MCR);
	sti();
#endif
	return 0;
}

/* Sending a break is a two step process on the SMC/SCC.  It is accomplished
 * by sending a STOP TRANSMIT command followed by a RESTART TRANSMIT
 * command.  We take advantage of the begin/end functions to make this
 * happen.
 */
static void begin_break(ser_info_t *info)
{
	volatile cpm_cpm2_t *cp;
	uint	page, sblock;
	ushort	num;

	cp = cpmp;

	if ((num = info->state->smc_scc_num) < SCC_NUM_BASE) {
		if (num == 0) {
			page = CPM_CR_SMC1_PAGE;
			sblock = CPM_CR_SMC1_SBLOCK;
		}
		else {
			page = CPM_CR_SMC2_PAGE;
			sblock = CPM_CR_SMC2_SBLOCK;
		}
	}
	else {
		num -= SCC_NUM_BASE;
		switch (num) {
		case 0:
			page = CPM_CR_SCC1_PAGE;
			sblock = CPM_CR_SCC1_SBLOCK;
			break;
		case 1:
			page = CPM_CR_SCC2_PAGE;
			sblock = CPM_CR_SCC2_SBLOCK;
			break;
		case 2:
			page = CPM_CR_SCC3_PAGE;
			sblock = CPM_CR_SCC3_SBLOCK;
			break;
		case 3:
			page = CPM_CR_SCC4_PAGE;
			sblock = CPM_CR_SCC4_SBLOCK;
			break;
		default: return;
		}
	}
	cp->cp_cpcr = mk_cr_cmd(page, sblock, 0, CPM_CR_STOP_TX) | CPM_CR_FLG;
	while (cp->cp_cpcr & CPM_CR_FLG);
}

static void end_break(ser_info_t *info)
{
	volatile cpm_cpm2_t *cp;
	uint	page, sblock;
	ushort	num;

	cp = cpmp;

	if ((num = info->state->smc_scc_num) < SCC_NUM_BASE) {
		if (num == 0) {
			page = CPM_CR_SMC1_PAGE;
			sblock = CPM_CR_SMC1_SBLOCK;
		}
		else {
			page = CPM_CR_SMC2_PAGE;
			sblock = CPM_CR_SMC2_SBLOCK;
		}
	}
	else {
		num -= SCC_NUM_BASE;
		switch (num) {
		case 0:
			page = CPM_CR_SCC1_PAGE;
			sblock = CPM_CR_SCC1_SBLOCK;
			break;
		case 1:
			page = CPM_CR_SCC2_PAGE;
			sblock = CPM_CR_SCC2_SBLOCK;
			break;
		case 2:
			page = CPM_CR_SCC3_PAGE;
			sblock = CPM_CR_SCC3_SBLOCK;
			break;
		case 3:
			page = CPM_CR_SCC4_PAGE;
			sblock = CPM_CR_SCC4_SBLOCK;
			break;
		default: return;
		}
	}
	cp->cp_cpcr = mk_cr_cmd(page, sblock, 0, CPM_CR_RESTART_TX) | CPM_CR_FLG;
	while (cp->cp_cpcr & CPM_CR_FLG);
}

/*
 * This routine sends a break character out the serial port.
 */
static void send_break(ser_info_t *info, int duration)
{
	current->state = TASK_INTERRUPTIBLE;
#ifdef SERIAL_DEBUG_SEND_BREAK
	printk("rs_send_break(%d) jiff=%lu...", duration, jiffies);
#endif
	begin_break(info);
	schedule_timeout(duration);
	end_break(info);
#ifdef SERIAL_DEBUG_SEND_BREAK
	printk("done jiffies=%lu\n", jiffies);
#endif
}


static int rs_8xx_ioctl(struct tty_struct *tty, struct file * file,
		    unsigned int cmd, unsigned long arg)
{
	int error;
	ser_info_t *info = (ser_info_t *)tty->driver_data;
	int retval;
	struct async_icount cnow;	/* kernel counter temps */
	struct serial_icounter_struct *p_cuser;	/* user space */

	if (serial_paranoia_check(info, tty->device, "rs_ioctl"))
		return -ENODEV;

	if ((cmd != TIOCMIWAIT) && (cmd != TIOCGICOUNT)) {
		if (tty->flags & (1 << TTY_IO_ERROR))
		    return -EIO;
	}

	switch (cmd) {
		case TCSBRK:	/* SVID version: non-zero arg --> no break */
			retval = tty_check_change(tty);
			if (retval)
				return retval;
			tty_wait_until_sent(tty, 0);
			if (signal_pending(current))
				return -EINTR;
			if (!arg) {
				send_break(info, HZ/4);	/* 1/4 second */
				if (signal_pending(current))
					return -EINTR;
			}
			return 0;
		case TCSBRKP:	/* support for POSIX tcsendbreak() */
			retval = tty_check_change(tty);
			if (retval)
				return retval;
			tty_wait_until_sent(tty, 0);
			if (signal_pending(current))
				return -EINTR;
			send_break(info, arg ? arg*(HZ/10) : HZ/4);
			if (signal_pending(current))
				return -EINTR;
			return 0;
		case TIOCSBRK:
			retval = tty_check_change(tty);
			if (retval)
				return retval;
			tty_wait_until_sent(tty, 0);
			begin_break(info);
			return 0;
		case TIOCCBRK:
			retval = tty_check_change(tty);
			if (retval)
				return retval;
			end_break(info);
			return 0;
		case TIOCGSOFTCAR:
			return put_user(C_CLOCAL(tty) ? 1 : 0, (int *) arg);
		case TIOCSSOFTCAR:
			error = get_user(arg, (unsigned int *) arg);
			if (error)
				return error;
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
#ifdef maybe
		case TIOCSERGETLSR: /* Get line status register */
			return get_lsr_info(info, (unsigned int *) arg);
#endif
		/*
		 * Wait for any of the 4 modem inputs (DCD,RI,DSR,CTS) to change
		 * - mask passed in arg for lines of interest
 		 *   (use |'ed TIOCM_RNG/DSR/CD/CTS for masking)
		 * Caller should use TIOCGICOUNT to see which one it was
		 */
		 case TIOCMIWAIT:
#ifdef modem_control
			cli();
			/* note the counters on entry */
			cprev = info->state->icount;
			sti();
			while (1) {
				interruptible_sleep_on(&info->delta_msr_wait);
				/* see if a signal did it */
				if (signal_pending(current))
					return -ERESTARTSYS;
				cli();
				cnow = info->state->icount; /* atomic copy */
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
#else
			return 0;
#endif

		/*
		 * Get counter of input serial line interrupts (DCD,RI,DSR,CTS)
		 * Return: write counters to the user passed counter struct
		 * NB: both 1->0 and 0->1 transitions are counted except for
		 *     RI where only 0->1 is counted.
		 */
		case TIOCGICOUNT:
			cli();
			cnow = info->state->icount;
			sti();
			p_cuser = (struct serial_icounter_struct *) arg;
			error = put_user(cnow.cts, &p_cuser->cts);
			if (error) return error;
			error = put_user(cnow.dsr, &p_cuser->dsr);
			if (error) return error;
			error = put_user(cnow.rng, &p_cuser->rng);
			if (error) return error;
			error = put_user(cnow.dcd, &p_cuser->dcd);
			if (error) return error;
			return 0;

		default:
			return -ENOIOCTLCMD;
		}
	return 0;
}

/* FIX UP modem control here someday......
*/
static void rs_8xx_set_termios(struct tty_struct *tty, struct termios *old_termios)
{
	ser_info_t *info = (ser_info_t *)tty->driver_data;

	if (   (tty->termios->c_cflag == old_termios->c_cflag)
	    && (   RELEVANT_IFLAG(tty->termios->c_iflag)
		== RELEVANT_IFLAG(old_termios->c_iflag)))
	  return;

	change_speed(info);

#ifdef modem_control
	/* Handle transition to B0 status */
	if ((old_termios->c_cflag & CBAUD) &&
	    !(tty->termios->c_cflag & CBAUD)) {
		info->MCR &= ~(UART_MCR_DTR|UART_MCR_RTS);
		cli();
		serial_out(info, UART_MCR, info->MCR);
		sti();
	}

	/* Handle transition away from B0 status */
	if (!(old_termios->c_cflag & CBAUD) &&
	    (tty->termios->c_cflag & CBAUD)) {
		info->MCR |= UART_MCR_DTR;
		if (!tty->hw_stopped ||
		    !(tty->termios->c_cflag & CRTSCTS)) {
			info->MCR |= UART_MCR_RTS;
		}
		cli();
		serial_out(info, UART_MCR, info->MCR);
		sti();
	}

	/* Handle turning off CRTSCTS */
	if ((old_termios->c_cflag & CRTSCTS) &&
	    !(tty->termios->c_cflag & CRTSCTS)) {
		tty->hw_stopped = 0;
		rs_8xx_start(tty);
	}
#endif

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
static void rs_8xx_close(struct tty_struct *tty, struct file * filp)
{
	ser_info_t *info = (ser_info_t *)tty->driver_data;
	struct serial_state *state;
	unsigned long	flags;
	int		idx;
	volatile smc_t	*smcp;
	volatile scc_t	*sccp;

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
		printk("rs_close: bad serial port count; tty->count is 1, "
		       "state->count is %d\n", state->count);
		state->count = 1;
	}
	if (--state->count < 0) {
		printk("rs_close: bad serial port count for ttys%d: %d\n",
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
	info->read_status_mask &= ~BD_SC_EMPTY;
	if (info->flags & ASYNC_INITIALIZED) {
		if ((idx = info->state->smc_scc_num) < SCC_NUM_BASE) {
			smcp = &cpm2_immr->im_smc[idx];
			smcp->smc_smcm &= ~SMCM_RX;
			smcp->smc_smcmr &= ~SMCMR_REN;
		}
		else {
			sccp = &cpm2_immr->im_scc[idx - SCC_IDX_BASE];
			sccp->scc_sccm &= ~UART_SCCM_RX;
			sccp->scc_gsmrl &= ~SCC_GSMRL_ENR;
		}
		/*
		 * Before we drop DTR, make sure the UART transmitter
		 * has completely drained; this is especially
		 * important if there is a transmit FIFO!
		 */
		rs_8xx_wait_until_sent(tty, info->timeout);
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
static void rs_8xx_wait_until_sent(struct tty_struct *tty, int timeout)
{
	ser_info_t *info = (ser_info_t *)tty->driver_data;
	unsigned long orig_jiffies, char_time;
	/*int lsr;*/
	volatile cbd_t *bdp;

	if (serial_paranoia_check(info, tty->device, "rs_wait_until_sent"))
		return;

#ifdef maybe
	if (info->state->type == PORT_UNKNOWN)
		return;
#endif

	orig_jiffies = jiffies;
	/*
	 * Set the check interval to be 1/5 of the estimated time to
	 * send a single character, and make it at least 1.  The check
	 * interval should also be less than the timeout.
	 *
	 * Note: we have to use pretty tight timings here to satisfy
	 * the NIST-PCTS.
	 */
	char_time = 1;
	if (timeout)
		char_time = MIN(char_time, timeout);
#ifdef SERIAL_DEBUG_RS_WAIT_UNTIL_SENT
	printk("In rs_wait_until_sent(%d) check=%lu...", timeout, char_time);
	printk("jiff=%lu...", jiffies);
#endif

	/* We go through the loop at least once because we can't tell
	 * exactly when the last character exits the shifter.  There can
	 * be at least two characters waiting to be sent after the buffers
	 * are empty.
	 */
	do {
#ifdef SERIAL_DEBUG_RS_WAIT_UNTIL_SENT
		printk("lsr = %d (jiff=%lu)...", lsr, jiffies);
#endif
		current->state = TASK_INTERRUPTIBLE;
/*		current->counter = 0;	 make us low-priority */
		schedule_timeout(char_time);
		if (signal_pending(current))
			break;
		if (timeout && time_after(jiffies, orig_jiffies + timeout))
			break;
		bdp = info->tx_cur;
	} while (bdp->cbd_sc & BD_SC_READY);
	current->state = TASK_RUNNING;
#ifdef SERIAL_DEBUG_RS_WAIT_UNTIL_SENT
	printk("lsr = %d (jiff=%lu)...done\n", lsr, jiffies);
#endif
}

/*
 * rs_hangup() --- called by tty_hangup() when a hangup is signaled.
 */
static void rs_8xx_hangup(struct tty_struct *tty)
{
	ser_info_t *info = (ser_info_t *)tty->driver_data;
	struct serial_state *state = info->state;

	if (serial_paranoia_check(info, tty->device, "rs_hangup"))
		return;

	state = info->state;

	rs_8xx_flush_buffer(tty);
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
			   ser_info_t *info)
{
#ifdef DO_THIS_LATER
	DECLARE_WAITQUEUE(wait, current);
#endif
	struct serial_state *state = info->state;
	int		retval;
	int		do_clocal = 0;

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
	 * If this is an SMC port, we don't have modem control to wait
	 * for, so just get out here.
	 */
	if ((filp->f_flags & O_NONBLOCK) ||
	    (tty->flags & (1 << TTY_IO_ERROR)) ||
	    (info->state->smc_scc_num < SCC_NUM_BASE)) {
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
#ifdef DO_THIS_LATER
	add_wait_queue(&info->open_wait, &wait);
#ifdef SERIAL_DEBUG_OPEN
	printk("block_til_ready before block: ttys%d, count = %d\n",
	       state->line, state->count);
#endif
	cli();
	if (!tty_hung_up_p(filp))
		state->count--;
	sti();
	info->blocked_open++;
	while (1) {
		cli();
		if (!(info->flags & ASYNC_CALLOUT_ACTIVE) &&
		    (tty->termios->c_cflag & CBAUD))
			serial_out(info, UART_MCR,
				   serial_inp(info, UART_MCR) |
				   (UART_MCR_DTR | UART_MCR_RTS));
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
		    (do_clocal || (serial_in(info, UART_MSR) &
				   UART_MSR_DCD)))
			break;
		if (signal_pending(current)) {
			retval = -ERESTARTSYS;
			break;
		}
#ifdef SERIAL_DEBUG_OPEN
		printk("block_til_ready blocking: ttys%d, count = %d\n",
		       info->line, state->count);
#endif
		schedule();
	}
	current->state = TASK_RUNNING;
	remove_wait_queue(&info->open_wait, &wait);
	if (!tty_hung_up_p(filp))
		state->count++;
	info->blocked_open--;
#ifdef SERIAL_DEBUG_OPEN
	printk("block_til_ready after blocking: ttys%d, count = %d\n",
	       info->line, state->count);
#endif
#endif /* DO_THIS_LATER */
	if (retval)
		return retval;
	info->flags |= ASYNC_NORMAL_ACTIVE;
	return 0;
}

static int get_async_struct(int line, ser_info_t **ret_info)
{
	struct serial_state *sstate;

	sstate = rs_table + line;
	if (sstate->info) {
		sstate->count++;
		*ret_info = (ser_info_t *)sstate->info;
		return 0;
	}
	else {
		return -ENOMEM;
	}
}

/*
 * This routine is called whenever a serial port is opened.  It
 * enables interrupts for a serial port, linking in its async structure into
 * the IRQ chain.   It also performs the serial-specific
 * initialization for the tty structure.
 */
static int rs_8xx_open(struct tty_struct *tty, struct file * filp)
{
	ser_info_t	*info;
	int 		retval, line;

	line = MINOR(tty->device) - tty->driver.minor_start;
	if ((line < 0) || (line >= NR_PORTS))
		return -ENODEV;
	retval = get_async_struct(line, &info);
	if (retval)
		return retval;
	if (serial_paranoia_check(info, tty->device, "rs_open"))
		return -ENODEV;

#ifdef SERIAL_DEBUG_OPEN
	printk("rs_open %s%d, count = %d\n", tty->driver.name, info->line,
	       info->state->count);
#endif
	tty->driver_data = info;
	info->tty = tty;

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
		printk("rs_open returning after block_til_ready with %d\n",
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

	info->session = current->session;
	info->pgrp = current->pgrp;

#ifdef SERIAL_DEBUG_OPEN
	printk("rs_open ttys%d successful...", info->line);
#endif
	return 0;
}

/*
 * /proc fs routines....
 */

static int inline line_info(char *buf, struct serial_state *state)
{
#ifdef notdef
	struct async_struct *info = state->info, scr_info;
	char	stat_buf[30], control, status;
#endif
	int	ret;

	ret = sprintf(buf, "%d: uart:%s port:%X irq:%d",
		      state->line,
		      (state->smc_scc_num < SCC_NUM_BASE) ? "SMC" : "SCC",
		      (unsigned int)(state->port), state->irq);

	if (!state->port || (state->type == PORT_UNKNOWN)) {
		ret += sprintf(buf+ret, "\n");
		return ret;
	}

#ifdef notdef
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
	cli();
	status = serial_in(info, UART_MSR);
	control = info ? info->MCR : serial_in(info, UART_MCR);
	sti();

	stat_buf[0] = 0;
	stat_buf[1] = 0;
	if (control & UART_MCR_RTS)
		strcat(stat_buf, "|RTS");
	if (status & UART_MSR_CTS)
		strcat(stat_buf, "|CTS");
	if (control & UART_MCR_DTR)
		strcat(stat_buf, "|DTR");
	if (status & UART_MSR_DSR)
		strcat(stat_buf, "|DSR");
	if (status & UART_MSR_DCD)
		strcat(stat_buf, "|CD");
	if (status & UART_MSR_RI)
		strcat(stat_buf, "|RI");

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

	/*
	 * Last thing is the RS-232 status lines
	 */
	ret += sprintf(buf+ret, " %s\n", stat_buf+1);
#endif
	return ret;
}

int rs_8xx_read_proc(char *page, char **start, off_t off, int count,
		 int *eof, void *data)
{
	int i, len = 0;
	off_t	begin = 0;

	len += sprintf(page, "serinfo:1.0 driver:%s\n", serial_version);
	for (i = 0; i < NR_PORTS && len < 4000; i++) {
		len += line_info(page + len, &rs_table[i]);
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
static _INLINE_ void show_serial_version(void)
{
 	printk(KERN_INFO "%s version %s\n", serial_name, serial_version);
}


/*
 * The serial console driver used during boot.  Note that these names
 * clash with those found in "serial.c", so we currently can't support
 * the 16xxx uarts and these at the same time.  I will fix this to become
 * an indirect function call from tty_io.c (or something).
 */

#ifdef CONFIG_SERIAL_CONSOLE

/*
 * Print a string to the serial port trying not to disturb any possible
 * real use of the port...
 * These funcitons work equally well for SCC, even though they are
 * designed for SMC.  Our only interests are the transmit/receive
 * buffers, which are identically mapped for either the SCC or SMC.
 */
static void my_console_write(int idx, const char *s,
				unsigned count)
{
	struct		serial_state	*ser;
	ser_info_t			*info;
	unsigned			i;
	volatile	cbd_t		*bdp, *bdbase;
	volatile	smc_uart_t	*up;
	volatile	u_char		*cp;

	ser = rs_table + idx;

	/* If the port has been initialized for general use, we have
	 * to use the buffer descriptors allocated there.  Otherwise,
	 * we simply use the single buffer allocated.
	 */
	if ((info = (ser_info_t *)ser->info) != NULL) {
		bdp = info->tx_cur;
		bdbase = info->tx_bd_base;
	}
	else {
		/* Pointer to UART in parameter ram.
		*/
		up = (smc_uart_t *)&cpm2_immr->im_dprambase[ser->port];

		/* Get the address of the host memory buffer.
		 */
		bdp = bdbase = (cbd_t *)&cpm2_immr->im_dprambase[up->smc_tbase];
	}

	/*
	 * We need to gracefully shut down the transmitter, disable
	 * interrupts, then send our bytes out.
	 */

	/*
	 * Now, do each character.  This is not as bad as it looks
	 * since this is a holding FIFO and not a transmitting FIFO.
	 * We could add the complexity of filling the entire transmit
	 * buffer, but we would just wait longer between accesses......
	 */
	for (i = 0; i < count; i++, s++) {
		/* Wait for transmitter fifo to empty.
		 * Ready indicates output is ready, and xmt is doing
		 * that, not that it is ready for us to send.
		 */
		while (bdp->cbd_sc & BD_SC_READY);

		/* Send the character out.
		 * If the buffer address is in the CPM DPRAM, don't
		 * convert it.
		 */
		if ((uint)(bdp->cbd_bufaddr) > (uint)CPM_MAP_ADDR)
			cp = (u_char *)(bdp->cbd_bufaddr);
		else
			cp = __va(bdp->cbd_bufaddr);
		*cp = *s;

		bdp->cbd_datlen = 1;
		bdp->cbd_sc |= BD_SC_READY;

		if (bdp->cbd_sc & BD_SC_WRAP)
			bdp = bdbase;
		else
			bdp++;

		/* if a LF, also do CR... */
		if (*s == 10) {
			while (bdp->cbd_sc & BD_SC_READY);
			cp = __va(bdp->cbd_bufaddr);
			*cp = 13;
			bdp->cbd_datlen = 1;
			bdp->cbd_sc |= BD_SC_READY;

			if (bdp->cbd_sc & BD_SC_WRAP) {
				bdp = bdbase;
			}
			else {
				bdp++;
			}
		}
	}

	/*
	 * Finally, Wait for transmitter & holding register to empty
	 *  and restore the IER
	 */
	while (bdp->cbd_sc & BD_SC_READY);

	if (info)
		info->tx_cur = (cbd_t *)bdp;
}

static void serial_console_write(struct console *c, const char *s,
				unsigned count)
{
#if defined(CONFIG_KGDB_CONSOLE) && !defined(CONFIG_USE_SERIAL2_KGDB)
	/* Try to let stub handle output. Returns true if it did. */ 
	if (kgdb_output_string(s, count))
		return;
#endif
	my_console_write(c->index, s, count);
}

#ifdef CONFIG_XMON
int
xmon_8xx_write(const char *s, unsigned count)
{
	my_console_write(KGDB_SER_IDX, s, count);
	return(count);
}
#endif

#ifdef CONFIG_KGDB
void
putDebugChar(char ch)
{
	my_console_write(KGDB_SER_IDX, &ch, 1);
}
#endif

#if defined(CONFIG_KGDB) || defined(CONFIG_XMON)
/*
 * Receive character from the serial port.  This only works well
 * before the port is initialize for real use.
 */
static int my_console_wait_key(int idx, int xmon, char *obuf)
{
	struct serial_state		*ser;
	u_char				c, *cp;
	ser_info_t			*info;
	volatile	cbd_t		*bdp;
	volatile	smc_uart_t	*up;
	int				i;

	ser = rs_table + idx;

	/* Pointer to UART in parameter ram.
	*/
	up = (smc_uart_t *)&cpm2_immr->im_dprambase[ser->port];

	/* Get the address of the host memory buffer.
	 * If the port has been initialized for general use, we must
	 * use information from the port structure.
	 */
	if ((info = (ser_info_t *)ser->info))
		bdp = info->rx_cur;
	else
		bdp = (cbd_t *)&cpm2_immr->im_dprambase[up->smc_rbase];

	/*
	 * We need to gracefully shut down the receiver, disable
	 * interrupts, then read the input.
	 * XMON just wants a poll.  If no character, return -1, else
	 * return the character.
	 */
	if (!xmon) {
		while (bdp->cbd_sc & BD_SC_EMPTY);
	}
	else {
		if (bdp->cbd_sc & BD_SC_EMPTY)
			return -1;
	}

	/* If the buffer address is in the CPM DPRAM, don't
	 * convert it.
	 */
	if ((uint)(bdp->cbd_bufaddr) > (uint)CPM_MAP_ADDR)
		cp = (u_char *)(bdp->cbd_bufaddr);
	else
		cp = __va(bdp->cbd_bufaddr);

	if (obuf) {
		i = c = bdp->cbd_datlen;
		while (i-- > 0)
			*obuf++ = *cp++;
	}
	else {
		c = *cp;
	}
	bdp->cbd_sc |= BD_SC_EMPTY;

	if (info) {
		if (bdp->cbd_sc & BD_SC_WRAP) {
			bdp = info->rx_bd_base;
		}
		else {
			bdp++;
		}
		info->rx_cur = (cbd_t *)bdp;
	}

	return((int)c);
}
#endif

#ifdef CONFIG_XMON
int
xmon_8xx_read_poll(void)
{
	return(my_console_wait_key(KGDB_SER_IDX, 1, NULL));
}

int
xmon_8xx_read_char(void)
{
	return(my_console_wait_key(KGDB_SER_IDX, 0, NULL));
}
#endif

#ifdef CONFIG_KGDB
static char kgdb_buf[RX_BUF_SIZE], *kgdp;
static int kgdb_chars;

char
getDebugChar(void)
{
	if (kgdb_chars <= 0) {
		kgdb_chars = my_console_wait_key(KGDB_SER_IDX, 0, kgdb_buf);
		kgdp = kgdb_buf;
	}
	kgdb_chars--;

	return(*kgdp++);
}

void kgdb_interruptible(int yes)
{
	volatile smc_t	*smcp;

	smcp = &cpm2_immr->im_smc[KGDB_SER_IDX];

	if (yes == 1)
		smcp->smc_smcm |= SMCM_RX;
	else
		smcp->smc_smcm &= ~SMCM_RX;
}

void kgdb_map_scc(void)
{
	ushort		serbase;
	uint		mem_addr;
	volatile	cbd_t		*bdp;
	volatile	smc_uart_t	*up;

	/* The serial port has already been initialized before
	 * we get here.  We have to assign some pointers needed by
	 * the kernel, and grab a memory location in the CPM that will
	 * work until the driver is really initialized.
	 */
	cpm2_immr = (cpm2_map_t *)CPM_MAP_ADDR;

	/* Right now, assume we are using SMCs.
	*/
#ifdef USE_KGDB_SMC2
	*(ushort *)(&cpm2_immr->im_dprambase[PROFF_SMC2_BASE]) = serbase = PROFF_SMC2;
#else
	*(ushort *)(&cpm2_immr->im_dprambase[PROFF_SMC1_BASE]) = serbase = PROFF_SMC1;
#endif
	up = (smc_uart_t *)&cpm2_immr->im_dprambase[serbase];

	/* Allocate space for an input FIFO, plus a few bytes for output.
	 * Allocate bytes to maintain word alignment.
	 */
	mem_addr = (uint)(&cpm2_immr->im_dprambase[0x1000]);

	/* Set the physical address of the host memory buffers in
	 * the buffer descriptors.
	 */
	bdp = (cbd_t *)&cpm2_immr->im_dprambase[up->smc_rbase];
	bdp->cbd_bufaddr = mem_addr;

	bdp = (cbd_t *)&cpm2_immr->im_dprambase[up->smc_tbase];
	bdp->cbd_bufaddr = mem_addr+RX_BUF_SIZE;

	up->smc_mrblr = RX_BUF_SIZE;		/* receive buffer length */
	up->smc_maxidl = RX_BUF_SIZE;
}
#endif

static kdev_t serial_console_device(struct console *c)
{
	return MKDEV(TTY_MAJOR, 64 + c->index);
}

/*
 *	Register console.
 */
long __init console_8xx_init(long kmem_start, long kmem_end)
{
	register_console(&sercons);
	return kmem_start;
}

#endif

/* Default console baud rate as determined by the board information
 * structure.
 */
static	int	baud_idx;

/*
 * The serial driver boot-time initialization code!
 */
int __init rs_8xx_init(void)
{
	struct serial_state * state;
	ser_info_t	*info;
	uint		mem_addr, dp_addr;
	int		i, j, idx;
	uint		page, sblock;
	volatile	cbd_t		*bdp;
	volatile	cpm_cpm2_t	*cp;
	volatile	smc_t		*sp;
	volatile	smc_uart_t	*up;
	volatile	scc_t		*scp;
	volatile	scc_uart_t	*sup;
	volatile	cpm2_map_t	*immap;
	volatile	iop_cpm2_t	*io;

	init_bh(SERIAL_BH, do_serial_bh);

	show_serial_version();

	/* Initialize the tty_driver structure */

	/*memset(&serial_driver, 0, sizeof(struct tty_driver));*/
	__clear_user(&serial_driver,sizeof(struct tty_driver));
	serial_driver.magic = TTY_DRIVER_MAGIC;
	serial_driver.driver_name = "serial";
#ifdef CONFIG_DEVFS_FS
	serial_driver.name = "tts/%d";
#else
	serial_driver.name = "ttyS";
#endif
	serial_driver.major = TTY_MAJOR;
	serial_driver.minor_start = 64;
	serial_driver.num = NR_PORTS;
	serial_driver.type = TTY_DRIVER_TYPE_SERIAL;
	serial_driver.subtype = SERIAL_TYPE_NORMAL;
	serial_driver.init_termios = tty_std_termios;
	serial_driver.init_termios.c_cflag =
		baud_idx | CS8 | CREAD | HUPCL | CLOCAL;
	serial_driver.flags = TTY_DRIVER_REAL_RAW;
	serial_driver.refcount = &serial_refcount;
	serial_driver.table = serial_table;
	serial_driver.termios = serial_termios;
	serial_driver.termios_locked = serial_termios_locked;

	serial_driver.open = rs_8xx_open;
	serial_driver.close = rs_8xx_close;
	serial_driver.write = rs_8xx_write;
	serial_driver.put_char = rs_8xx_put_char;
	serial_driver.write_room = rs_8xx_write_room;
	serial_driver.chars_in_buffer = rs_8xx_chars_in_buffer;
	serial_driver.flush_buffer = rs_8xx_flush_buffer;
	serial_driver.ioctl = rs_8xx_ioctl;
	serial_driver.throttle = rs_8xx_throttle;
	serial_driver.unthrottle = rs_8xx_unthrottle;
	serial_driver.send_xchar = rs_8xx_send_xchar;
	serial_driver.set_termios = rs_8xx_set_termios;
	serial_driver.stop = rs_8xx_stop;
	serial_driver.start = rs_8xx_start;
	serial_driver.hangup = rs_8xx_hangup;
	serial_driver.wait_until_sent = rs_8xx_wait_until_sent;
	serial_driver.read_proc = rs_8xx_read_proc;

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

	immap = cpm2_immr;
	cp = &immap->im_cpm;
	io = &immap->im_ioport;

	/* This should have been done long ago by the early boot code,
	 * but do it again to make sure.
	 */
	*(ushort *)(&immap->im_dprambase[PROFF_SMC1_BASE]) = PROFF_SMC1;
	*(ushort *)(&immap->im_dprambase[PROFF_SMC2_BASE]) = PROFF_SMC2;

	/* Geeze, here we go....Picking I/O port bits....Lots of
	 * choices.  If you don't like mine, pick your own.
	 * Configure SMCs Tx/Rx.  SMC1 is only on Port D, SMC2 is
	 * only on Port A.  You either pick 'em, or not.
	 */
#ifndef SCC_CONSOLE
	io->iop_ppard |= 0x00c00000;
	io->iop_pdird |= 0x00400000;
	io->iop_pdird &= ~0x00800000;
	io->iop_psord &= ~0x00c00000;
#if USE_SMC2
	io->iop_ppara |= 0x00c00000;
	io->iop_pdira |= 0x00400000;
	io->iop_pdira &= ~0x00800000;
	io->iop_psora &= ~0x00c00000;
#endif

	/* Configure SCC2 and SCC3.  Be careful about the fine print.
	 * Secondary options are only available when you take away
	 * the primary option.  Unless the pins are used for something
	 * else, SCC2 and SCC3 are on Port B.
	 *	Port B,  8 - SCC3 TxD
	 *	Port B, 12 - SCC2 TxD
	 *	Port B, 14 - SCC3 RxD
	 *	Port B, 15 - SCC2 RxD
	 */
	io->iop_pparb |= 0x008b0000;
	io->iop_pdirb |= 0x00880000;
	io->iop_psorb |= 0x00880000;
	io->iop_pdirb &= ~0x00030000;
	io->iop_psorb &= ~0x00030000;

	/* Wire BRG1 to SMC1 and BRG2 to SMC2.
	*/
	immap->im_cpmux.cmx_smr = 0;

	/* Connect SCC2 and SCC3 to NMSI.  Connect BRG3 to SCC2 and
	 * BRG4 to SCC3.
	 */
	immap->im_cpmux.cmx_scr &= ~0x00ffff00;
	immap->im_cpmux.cmx_scr |= 0x00121b00;
#else
	io->iop_pparb |= 0x008b0000;
	io->iop_pdirb |= 0x00880000;
	io->iop_psorb |= 0x00880000;
	io->iop_pdirb &= ~0x00030000;
	io->iop_psorb &= ~0x00030000;

	/* Use Port D for SCC1 instead of other functions.
	*/
	io->iop_ppard |= 0x00000003;
	io->iop_psord &= ~0x00000001;	/* Rx */
	io->iop_psord |= 0x00000002;	/* Tx */
	io->iop_pdird &= ~0x00000001;	/* Rx */
	io->iop_pdird |= 0x00000002;	/* Tx */

	/* Connect SCC1, SCC2, SCC3 to NMSI.  Connect BRG1 to SCC1,
	 * BRG2 to SCC2, BRG3 to SCC3.
	 */
	immap->im_cpmux.cmx_scr &= ~0xffffff00;
	immap->im_cpmux.cmx_scr |= 0x00091200;
#endif

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
 		printk (KERN_INFO "ttyS%d on %s%d at 0x%04x, BRG%d\n",
 			i,
 			(state->smc_scc_num < SCC_NUM_BASE) ? "SMC" : "SCC",
 			PORT_NUM(state->smc_scc_num) + 1,
 			(unsigned int)(state->port),
 			state->smc_scc_num + 1);
#ifdef CONFIG_SERIAL_CONSOLE
		/* If we just printed the message on the console port, and
		 * we are about to initialize it for general use, we have
		 * to wait a couple of character times for the CR/NL to
		 * make it out of the transmit buffer.
		 */
		if (i == CONFIG_SERIAL_CONSOLE_PORT)
			mdelay(300);
#endif
		info = kmalloc(sizeof(ser_info_t), GFP_KERNEL);
		if (info) {
			/*memset(info, 0, sizeof(ser_info_t));*/
			__clear_user(info,sizeof(ser_info_t));
			init_waitqueue_head(&info->open_wait);
			init_waitqueue_head(&info->close_wait);
			info->magic = SERIAL_MAGIC;
			info->flags = state->flags;
			info->tqueue.routine = do_softint;
			info->tqueue.data = info;
			info->tqueue_hangup.routine = do_serial_hangup;
			info->tqueue_hangup.data = info;
			info->line = i;
			info->state = state;
			state->info = (struct async_struct *)info;

			/* We need to allocate a transmit and receive buffer
			 * descriptors from dual port ram, and a character
			 * buffer area from host mem.
			 */
			dp_addr = cpm2_dpalloc(sizeof(cbd_t) * RX_NUM_FIFO, 8);

			/* Allocate space for FIFOs in the host memory.
			*/
			mem_addr = cpm2_hostalloc(RX_NUM_FIFO * RX_BUF_SIZE, 1);

			/* Set the physical address of the host memory
			 * buffers in the buffer descriptors, and the
			 * virtual address for us to work with.
			 */
			bdp = (cbd_t *)&immap->im_dprambase[dp_addr];
			info->rx_cur = info->rx_bd_base = (cbd_t *)bdp;

			for (j=0; j<(RX_NUM_FIFO-1); j++) {
				bdp->cbd_bufaddr = __pa(mem_addr);
				bdp->cbd_sc = BD_SC_EMPTY | BD_SC_INTRPT;
				mem_addr += RX_BUF_SIZE;
				bdp++;
			}
			bdp->cbd_bufaddr = __pa(mem_addr);
			bdp->cbd_sc = BD_SC_WRAP | BD_SC_EMPTY | BD_SC_INTRPT;

			if ((idx = state->smc_scc_num) < SCC_NUM_BASE) {
				sp = &immap->im_smc[idx];
				up = (smc_uart_t *)&immap->im_dprambase[state->port];
				up->smc_rbase = dp_addr;
			}
			else {
				scp = &immap->im_scc[idx - SCC_IDX_BASE];
				sup = (scc_uart_t *)&immap->im_dprambase[state->port];
				scp->scc_gsmrl &= ~(SCC_GSMRL_ENR | SCC_GSMRL_ENT);
				sup->scc_genscc.scc_rbase = dp_addr;
			}

			dp_addr = cpm2_dpalloc(sizeof(cbd_t) * TX_NUM_FIFO, 8);

			/* Allocate space for FIFOs in the host memory.
			*/
			mem_addr = cpm2_hostalloc(TX_NUM_FIFO * TX_BUF_SIZE, 1);

			/* Set the physical address of the host memory
			 * buffers in the buffer descriptors, and the
			 * virtual address for us to work with.
			 */
			bdp = (cbd_t *)&immap->im_dprambase[dp_addr];
			info->tx_cur = info->tx_bd_base = (cbd_t *)bdp;

			for (j=0; j<(TX_NUM_FIFO-1); j++) {
				bdp->cbd_bufaddr = __pa(mem_addr);
				bdp->cbd_sc = BD_SC_INTRPT;
				mem_addr += TX_BUF_SIZE;
				bdp++;
			}
			bdp->cbd_bufaddr = __pa(mem_addr);
			bdp->cbd_sc = (BD_SC_WRAP | BD_SC_INTRPT);

			if (idx < SCC_NUM_BASE) {
				up->smc_tbase = dp_addr;

				/* Set up the uart parameters in the
				 * parameter ram.
				 */
				up->smc_rfcr = CPMFCR_GBL | CPMFCR_EB;
				up->smc_tfcr = CPMFCR_GBL | CPMFCR_EB;

				/* Set this to 1 for now, so we get single
				 * character interrupts.  Using idle charater
				 * time requires some additional tuning.
				 */
				up->smc_mrblr = 1;
				up->smc_maxidl = 0;
				up->smc_brkcr = 1;

				/* Send the CPM an initialize command.
				*/
				if (state->smc_scc_num == 0) {
					page = CPM_CR_SMC1_PAGE;
					sblock = CPM_CR_SMC1_SBLOCK;
				}
				else {
					page = CPM_CR_SMC2_PAGE;
					sblock = CPM_CR_SMC2_SBLOCK;
				}

				cp->cp_cpcr = mk_cr_cmd(page, sblock, 0,
						CPM_CR_INIT_TRX) | CPM_CR_FLG;
				while (cp->cp_cpcr & CPM_CR_FLG);

				/* Set UART mode, 8 bit, no parity, one stop.
				 * Enable receive and transmit.
				 */
				sp->smc_smcmr = smcr_mk_clen(9) | SMCMR_SM_UART;

				/* Disable all interrupts and clear all pending
				 * events.
				 */
				sp->smc_smcm = 0;
				sp->smc_smce = 0xff;
			}
			else {
				sup->scc_genscc.scc_tbase = dp_addr;

				/* Set up the uart parameters in the
				 * parameter ram.
				 */
				sup->scc_genscc.scc_rfcr = CPMFCR_GBL | CPMFCR_EB;
				sup->scc_genscc.scc_tfcr = CPMFCR_GBL | CPMFCR_EB;

				/* Set this to 1 for now, so we get single
				 * character interrupts.  Using idle charater
				 * time requires some additional tuning.
				 */
				sup->scc_genscc.scc_mrblr = 1;
				sup->scc_maxidl = 0;
				sup->scc_brkcr = 1;
				sup->scc_parec = 0;
				sup->scc_frmec = 0;
				sup->scc_nosec = 0;
				sup->scc_brkec = 0;
				sup->scc_uaddr1 = 0;
				sup->scc_uaddr2 = 0;
				sup->scc_toseq = 0;
				sup->scc_char1 = 0x8000;
				sup->scc_char2 = 0x8000;
				sup->scc_char3 = 0x8000;
				sup->scc_char4 = 0x8000;
				sup->scc_char5 = 0x8000;
				sup->scc_char6 = 0x8000;
				sup->scc_char7 = 0x8000;
				sup->scc_char8 = 0x8000;
				sup->scc_rccm = 0xc0ff;

				/* Send the CPM an initialize command.
				*/
#ifdef SCC_CONSOLE
				switch (state->smc_scc_num) {
				case 0:
					page = CPM_CR_SCC1_PAGE;
					sblock = CPM_CR_SCC1_SBLOCK;
					break;
				case 1:
					page = CPM_CR_SCC2_PAGE;
					sblock = CPM_CR_SCC2_SBLOCK;
					break;
				case 2:
					page = CPM_CR_SCC3_PAGE;
					sblock = CPM_CR_SCC3_SBLOCK;
					break;
				}
#else
				if (state->smc_scc_num == 2) {
					page = CPM_CR_SCC2_PAGE;
					sblock = CPM_CR_SCC2_SBLOCK;
				}
				else {
					page = CPM_CR_SCC3_PAGE;
					sblock = CPM_CR_SCC3_SBLOCK;
				}
#endif

				cp->cp_cpcr = mk_cr_cmd(page, sblock, 0,
						CPM_CR_INIT_TRX) | CPM_CR_FLG;
				while (cp->cp_cpcr & CPM_CR_FLG);

				/* Set UART mode, 8 bit, no parity, one stop.
				 * Enable receive and transmit.
				 */
				scp->scc_gsmrh = 0;
				scp->scc_gsmrl =
					(SCC_GSMRL_MODE_UART | SCC_GSMRL_TDCR_16 | SCC_GSMRL_RDCR_16);

				/* Disable all interrupts and clear all pending
				 * events.
				 */
				scp->scc_sccm = 0;
				scp->scc_scce = 0xffff;
				scp->scc_dsr = 0x7e7e;
				scp->scc_psmr = 0x3000;
			}

			/* Install interrupt handler.
			*/
			request_irq(state->irq, rs_8xx_interrupt, 0, "uart",
					info);

			/* Set up the baud rate generator.
			*/
			cpm2_setbrg(state->smc_scc_num,
							baud_table[baud_idx]);

			/* If the port is the console, enable Rx and Tx.
			*/
#ifdef CONFIG_SERIAL_CONSOLE
			if (i == CONFIG_SERIAL_CONSOLE_PORT) {
				if (idx < SCC_NUM_BASE)
					sp->smc_smcmr |= SMCMR_REN | SMCMR_TEN;
				else
					scp->scc_gsmrl |= (SCC_GSMRL_ENR | SCC_GSMRL_ENT);
			}
#endif
		}
	}
	return 0;
}

/* This must always be called before the rs_8xx_init() function, otherwise
 * it blows away the port control information.
*/
static int __init serial_console_setup(struct console *co, char *options)
{
	struct		serial_state *ser;
	uint		mem_addr, dp_addr, bidx;
	volatile	cbd_t		*bdp;
	volatile	cpm_cpm2_t	*cp;
	volatile	cpm2_map_t	*immap;
#ifndef SCC_CONSOLE
	volatile	smc_t		*sp;
	volatile	smc_uart_t	*up;
#endif
	volatile	scc_t		*scp;
	volatile	scc_uart_t	*sup;
	volatile	iop_cpm2_t	*io;
	bd_t				*bd;

	bd = (bd_t *)__res;

	for (bidx = 0; bidx < (sizeof(baud_table) / sizeof(int)); bidx++)
		if (bd->bi_baudrate == baud_table[bidx])
			break;

	co->cflag = CREAD|CLOCAL|bidx|CS8;
	baud_idx = bidx;

	ser = rs_table + co->index;

	immap = cpm2_immr;
	cp = &immap->im_cpm;
	io = &immap->im_ioport;

#ifdef SCC_CONSOLE
	scp = (scc_t *)&(immap->im_scc[SCC_CONSOLE-1]);
	sup = (scc_uart_t *)&immap->im_dprambase[PROFF_SCC1 + ((SCC_CONSOLE-1) << 8)];
	scp->scc_sccm &= ~(UART_SCCM_TX | UART_SCCM_RX);
	scp->scc_gsmrl &= ~(SCC_GSMRL_ENR | SCC_GSMRL_ENT);

	/* Use Port D for SCC1 instead of other functions.
	*/
	io->iop_ppard |= 0x00000003;
	io->iop_psord &= ~0x00000001;	/* Rx */
	io->iop_psord |= 0x00000002;	/* Tx */
	io->iop_pdird &= ~0x00000001;	/* Rx */
	io->iop_pdird |= 0x00000002;	/* Tx */

#else
	/* This should have been done long ago by the early boot code,
	 * but do it again to make sure.
	 */
	*(ushort *)(&immap->im_dprambase[PROFF_SMC1_BASE]) = PROFF_SMC1;
	*(ushort *)(&immap->im_dprambase[PROFF_SMC2_BASE]) = PROFF_SMC2;

	/* Right now, assume we are using SMCs.
	*/
	sp = &immap->im_smc[ser->smc_scc_num];

	/* When we get here, the CPM has been reset, so we need
	 * to configure the port.
	 * We need to allocate a transmit and receive buffer descriptor
	 * from dual port ram, and a character buffer area from host mem.
	 */
	up = (smc_uart_t *)&immap->im_dprambase[ser->port];

	/* Disable transmitter/receiver.
	*/
	sp->smc_smcmr &= ~(SMCMR_REN | SMCMR_TEN);

	/* Use Port D for SMC1 instead of other functions.
	*/
	io->iop_ppard |= 0x00c00000;
	io->iop_pdird |= 0x00400000;
	io->iop_pdird &= ~0x00800000;
	io->iop_psord &= ~0x00c00000;
#endif

	/* Allocate space for two buffer descriptors in the DP ram.
	*/
	dp_addr = cpm2_dpalloc(sizeof(cbd_t) * 2, 8);

	/* Allocate space for two 2 byte FIFOs in the host memory.
	*/
	mem_addr = cpm2_hostalloc(4, 1);

	/* Set the physical address of the host memory buffers in
	 * the buffer descriptors.
	 */
	bdp = (cbd_t *)&immap->im_dprambase[dp_addr];
	bdp->cbd_bufaddr = __pa(mem_addr);
	(bdp+1)->cbd_bufaddr = __pa(mem_addr+2);

	/* For the receive, set empty and wrap.
	 * For transmit, set wrap.
	 */
	bdp->cbd_sc = BD_SC_EMPTY | BD_SC_WRAP;
	(bdp+1)->cbd_sc = BD_SC_WRAP;

	/* Set up the uart parameters in the parameter ram.
	*/
#ifdef SCC_CONSOLE
	sup->scc_genscc.scc_rbase = dp_addr;
	sup->scc_genscc.scc_tbase = dp_addr + sizeof(cbd_t);

	/* Set up the uart parameters in the
	 * parameter ram.
	 */
	sup->scc_genscc.scc_rfcr = CPMFCR_GBL | CPMFCR_EB;
	sup->scc_genscc.scc_tfcr = CPMFCR_GBL | CPMFCR_EB;

	sup->scc_genscc.scc_mrblr = 1;
	sup->scc_maxidl = 0;
	sup->scc_brkcr = 1;
	sup->scc_parec = 0;
	sup->scc_frmec = 0;
	sup->scc_nosec = 0;
	sup->scc_brkec = 0;
	sup->scc_uaddr1 = 0;
	sup->scc_uaddr2 = 0;
	sup->scc_toseq = 0;
	sup->scc_char1 = 0x8000;
	sup->scc_char2 = 0x8000;
	sup->scc_char3 = 0x8000;
	sup->scc_char4 = 0x8000;
	sup->scc_char5 = 0x8000;
	sup->scc_char6 = 0x8000;
	sup->scc_char7 = 0x8000;
	sup->scc_char8 = 0x8000;
	sup->scc_rccm = 0xc0ff;

	/* Send the CPM an initialize command.
	*/
	cp->cp_cpcr = mk_cr_cmd(CPM_CR_SCC1_PAGE, CPM_CR_SCC1_SBLOCK, 0,
			CPM_CR_INIT_TRX) | CPM_CR_FLG;
	while (cp->cp_cpcr & CPM_CR_FLG);

	/* Set UART mode, 8 bit, no parity, one stop.
	 * Enable receive and transmit.
	 */
	scp->scc_gsmrh = 0;
	scp->scc_gsmrl = 
		(SCC_GSMRL_MODE_UART | SCC_GSMRL_TDCR_16 | SCC_GSMRL_RDCR_16);

	/* Disable all interrupts and clear all pending
	 * events.
	 */
	scp->scc_sccm = 0;
	scp->scc_scce = 0xffff;
	scp->scc_dsr = 0x7e7e;
	scp->scc_psmr = 0x3000;

	/* Wire BRG1 to SCC1.  The serial init will take care of
	 * others.
	 */
	immap->im_cpmux.cmx_scr = 0;

	/* Set up the baud rate generator.
	*/
	cpm2_setbrg(ser->smc_scc_num, bd->bi_baudrate);

	scp->scc_gsmrl |= (SCC_GSMRL_ENR | SCC_GSMRL_ENT);
#else
	up->smc_rbase = dp_addr;	/* Base of receive buffer desc. */
	up->smc_tbase = dp_addr+sizeof(cbd_t);	/* Base of xmt buffer desc. */
	up->smc_rfcr = CPMFCR_GBL | CPMFCR_EB;
	up->smc_tfcr = CPMFCR_GBL | CPMFCR_EB;

	/* Set this to 1 for now, so we get single character interrupts.
	*/
	up->smc_mrblr = 1;		/* receive buffer length */
	up->smc_maxidl = 0;		/* wait forever for next char */

	/* Send the CPM an initialize command.
	*/
	cp->cp_cpcr = mk_cr_cmd(CPM_CR_SMC1_PAGE, CPM_CR_SMC1_SBLOCK, 0,
			CPM_CR_INIT_TRX) | CPM_CR_FLG;
	while (cp->cp_cpcr & CPM_CR_FLG);

	/* Set UART mode, 8 bit, no parity, one stop.
	 * Enable receive and transmit.
	 */
	sp->smc_smcmr = smcr_mk_clen(9) |  SMCMR_SM_UART;

	/* Set up the baud rate generator.
	*/
	cpm2_setbrg(ser->smc_scc_num, bd->bi_baudrate);

	/* And finally, enable Rx and Tx.
	*/
	sp->smc_smcmr |= SMCMR_REN | SMCMR_TEN;
#endif

	return 0;
}
