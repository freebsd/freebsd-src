/* sgiserial.c: Serial port driver for SGI machines.
 *
 * Copyright (C) 1996 David S. Miller (dm@engr.sgi.com)
 */

/*
 * Note: This driver seems to have been derived from some
 * version of the sbus/char/zs.c driver.  A lot of clean-up
 * and bug fixes seem to have happened to the Sun driver in
 * the intervening time.  As of 21.09.1999, I have merged in
 * ONLY the changes necessary to fix observed functional
 * problems on the Indy.  Someone really ought to do a
 * thorough pass to merge in the rest of the updates.
 * Better still, someone really ought to make it a common
 * code module for both platforms.   kevink@mips.com
 */

#include <linux/config.h> /* for CONFIG_KGDB */
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
#include <linux/mm.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/console.h>
#include <linux/init.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/system.h>
#include <asm/bitops.h>
#include <asm/uaccess.h>
#include <asm/sgialib.h>
#include <asm/sgi/ioc.h>
#include <asm/sgi/ip22.h>

#include "sgiserial.h"

#define NUM_SERIAL 1     /* One chip on board. */
#define NUM_CHANNELS (NUM_SERIAL * 2)

struct sgi_zslayout *zs_chips[NUM_SERIAL];
struct sgi_zschannel *zs_channels[NUM_CHANNELS];
struct sgi_zschannel *zs_conschan;
struct sgi_zschannel *zs_kgdbchan;

struct sgi_serial zs_soft[NUM_CHANNELS];
struct sgi_serial *zs_chain;  /* IRQ servicing chain */
static int zilog_irq = SGI_SERIAL_IRQ;

/* Console hooks... */
static int zs_cons_chanout;
static int zs_cons_chanin;
struct sgi_serial *zs_consinfo;

static unsigned char kgdb_regs[16] = {
	0, 0, 0,                     /* write 0, 1, 2 */
	(Rx8 | RxENABLE),            /* write 3 */
	(X16CLK | SB1 | PAR_EVEN),   /* write 4 */
	(Tx8 | TxENAB),              /* write 5 */
	0, 0, 0,                     /* write 6, 7, 8 */
	(NV),                        /* write 9 */
	(NRZ),                       /* write 10 */
	(TCBR | RCBR),               /* write 11 */
	0, 0,                        /* BRG time constant, write 12 + 13 */
	(BRENABL),                   /* write 14 */
	(DCDIE)                      /* write 15 */
};

static unsigned char zscons_regs[16] = {
	0,                           /* write 0 */
	(EXT_INT_ENAB | INT_ALL_Rx), /* write 1 */
	0,                           /* write 2 */
	(Rx8 | RxENABLE),            /* write 3 */
	(X16CLK),                    /* write 4 */
	(DTR | Tx8 | TxENAB),        /* write 5 */
	0, 0, 0,                     /* write 6, 7, 8 */
	(NV | MIE),                  /* write 9 */
	(NRZ),                       /* write 10 */
	(TCBR | RCBR),               /* write 11 */
	0, 0,                        /* BRG time constant, write 12 + 13 */
	(BRENABL),                   /* write 14 */
	(DCDIE | CTSIE | TxUIE | BRKIE) /* write 15 */
};

#define ZS_CLOCK         3672000   /* Zilog input clock rate */

DECLARE_TASK_QUEUE(tq_serial);

struct tty_driver serial_driver, callout_driver;
struct console *sgisercon;
static int serial_refcount;

/* serial subtype definitions */
#define SERIAL_TYPE_NORMAL	1
#define SERIAL_TYPE_CALLOUT	2

/* number of characters left in xmit buffer before we ask for more */
#define WAKEUP_CHARS 256

#undef SERIAL_DEBUG_OPEN

static void change_speed(struct sgi_serial *info);

static struct tty_struct *serial_table[NUM_CHANNELS];
static struct termios *serial_termios[NUM_CHANNELS];
static struct termios *serial_termios_locked[NUM_CHANNELS];

/*
 * tmp_buf is used as a temporary buffer by serial_write.  We need to
 * lock it in case the memcpy_fromfs blocks while swapping in a page,
 * and some other program tries to do a serial write at the same time.
 * Since the lock will only come under contention when the system is
 * swapping and available memory is low, it makes sense to share one
 * buffer across all the serial ports, since it significantly saves
 * memory if large numbers of serial ports are open.
 */
static unsigned char tmp_buf[PAGE_SIZE]; /* This is cheating */
static DECLARE_MUTEX(tmp_buf_sem);

static inline int serial_paranoia_check(struct sgi_serial *info,
					dev_t device, const char *routine)
{
#ifdef SERIAL_PARANOIA_CHECK
	static const char *badmagic = KERN_WARNING
		"Warning: bad magic number for serial struct (%d, %d) in %s\n";
	static const char *badinfo = KERN_WARNING
		"Warning: null sgi_serial for (%d, %d) in %s\n";

	if (!info) {
		printk(badinfo, MAJOR(device), MINOR(device), routine);
		return 1;
	}
	if (info->magic != SERIAL_MAGIC) {
		printk(badmagic, MAJOR(device), MINOR(device), routine);
		return 1;
	}
#endif
	return 0;
}

/*
 * This is used to figure out the divisor speeds and the timeouts
 */
static int baud_table[] = {
	0, 50, 75, 110, 134, 150, 200, 300, 600, 1200, 1800, 2400, 4800,
	9600, 19200, 38400, 57600, 115200, 0 };

/*
 * Reading and writing Zilog8530 registers.  The delays are to make this
 * driver work on the Sun4 which needs a settling delay after each chip
 * register access, other machines handle this in hardware via auxiliary
 * flip-flops which implement the settle time we do in software.
 *
 * read_zsreg() and write_zsreg() may get called from rs_kgdb_hook() before
 * interrupts are enabled. Therefore we have to check ioc_iocontrol before we
 * access it.
 */
static inline unsigned char read_zsreg(struct sgi_zschannel *channel,
                                       unsigned char reg)
{
	unsigned char retval;
	volatile unsigned char junk;

	udelay(2);
	channel->control = reg;
	junk = sgint->istat0;
	udelay(1);
	retval = channel->control;
	return retval;
}

static inline void write_zsreg(struct sgi_zschannel *channel,
                               unsigned char reg, unsigned char value)
{
	volatile unsigned char junk;

	udelay(2);
	channel->control = reg;
	junk = sgint->istat0;
	udelay(1);
	channel->control = value;
	junk = sgint->istat0;
	return;
}

static inline void load_zsregs(struct sgi_zschannel *channel, unsigned char *regs)
{
	ZS_CLEARERR(channel);
	ZS_CLEARFIFO(channel);
	/* Load 'em up */
	write_zsreg(channel, R4, regs[R4]);
	write_zsreg(channel, R10, regs[R10]);
	write_zsreg(channel, R3, regs[R3] & ~RxENABLE);
	write_zsreg(channel, R5, regs[R5] & ~TxENAB);
	write_zsreg(channel, R1, regs[R1]);
	write_zsreg(channel, R9, regs[R9]);
	write_zsreg(channel, R11, regs[R11]);
	write_zsreg(channel, R12, regs[R12]);
	write_zsreg(channel, R13, regs[R13]);
	write_zsreg(channel, R14, regs[R14]);
	write_zsreg(channel, R15, regs[R15]);
	write_zsreg(channel, R3, regs[R3]);
	write_zsreg(channel, R5, regs[R5]);
	return;
}

/* Sets or clears DTR/RTS on the requested line */
static inline void zs_rtsdtr(struct sgi_serial *ss, int set)
{
	if(set) {
		ss->curregs[5] |= (RTS | DTR);
		write_zsreg(ss->zs_channel, 5, ss->curregs[5]);
	} else {
		ss->curregs[5] &= ~(RTS | DTR);
		write_zsreg(ss->zs_channel, 5, ss->curregs[5]);
	}
	return;
}

static inline void kgdb_chaninit(struct sgi_serial *ss, int intson, int bps)
{
	int brg;

	if(intson) {
		kgdb_regs[R1] = INT_ALL_Rx;
		kgdb_regs[R9] |= MIE;
	} else {
		kgdb_regs[R1] = 0;
		kgdb_regs[R9] &= ~MIE;
	}
	brg = BPS_TO_BRG(bps, ZS_CLOCK/ss->clk_divisor);
	kgdb_regs[R12] = (brg & 255);
	kgdb_regs[R13] = ((brg >> 8) & 255);
	load_zsregs(ss->zs_channel, kgdb_regs);
}

/* Utility routines for the Zilog */
static inline int get_zsbaud(struct sgi_serial *ss)
{
	struct sgi_zschannel *channel = ss->zs_channel;
	int brg;

	/* The baud rate is split up between two 8-bit registers in
	 * what is termed 'BRG time constant' format in my docs for
	 * the chip, it is a function of the clk rate the chip is
	 * receiving which happens to be constant.
	 */
	brg = ((read_zsreg(channel, 13)&0xff) << 8);
	brg |= (read_zsreg(channel, 12)&0xff);
	return BRG_TO_BPS(brg, (ZS_CLOCK/(ss->clk_divisor)));
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
	struct sgi_serial *info = (struct sgi_serial *)tty->driver_data;
	unsigned long flags;

	if (serial_paranoia_check(info, tty->device, "rs_stop"))
		return;

	save_flags(flags); cli();
	if (info->curregs[5] & TxENAB) {
		info->curregs[5] &= ~TxENAB;
		write_zsreg(info->zs_channel, 5, info->curregs[5]);
	}
	restore_flags(flags);
}

static void rs_start(struct tty_struct *tty)
{
	struct sgi_serial *info = (struct sgi_serial *)tty->driver_data;
	unsigned long flags;

	if (serial_paranoia_check(info, tty->device, "rs_start"))
		return;

	save_flags(flags); cli();
	if (info->xmit_cnt && info->xmit_buf && !(info->curregs[5] & TxENAB)) {
		info->curregs[5] |= TxENAB;
		write_zsreg(info->zs_channel, 5, info->curregs[5]);
	}
	restore_flags(flags);
}

/* Drop into either the boot monitor or kadb upon receiving a break
 * from keyboard/console input.
 */
static void batten_down_hatches(void)
{
	ArcEnterInteractiveMode();
#if 0
	/* If we are doing kadb, we call the debugger
	 * else we just drop into the boot monitor.
	 * Note that we must flush the user windows
	 * first before giving up control.
	 */
	printk("\n");
	if((((unsigned long)linux_dbvec)>=DEBUG_FIRSTVADDR) &&
	   (((unsigned long)linux_dbvec)<=DEBUG_LASTVADDR))
		sp_enter_debugger();
	else
		prom_halt();

	/* XXX We want to notify the keyboard driver that all
	 * XXX keys are in the up state or else weird things
	 * XXX happen...
	 */
#endif
	return;
}

/* On receive, this clears errors and the receiver interrupts */
static inline void rs_recv_clear(struct sgi_zschannel *zsc)
{
	volatile unsigned char junk;

	udelay(2);
	zsc->control = ERR_RES;
	junk = sgint->istat0;
	udelay(2);
	zsc->control = RES_H_IUS;
	junk = sgint->istat0;
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
static inline void rs_sched_event(struct sgi_serial *info,
				    int event)
{
	info->event |= 1 << event;
	queue_task(&info->tqueue, &tq_serial);
	mark_bh(SERIAL_BH);
}

#ifdef CONFIG_KGDB
extern void set_async_breakpoint(unsigned int epc);
#endif

static inline void receive_chars(struct sgi_serial *info, struct pt_regs *regs)
{
	struct tty_struct *tty = info->tty;
	volatile unsigned char junk;
	unsigned char ch, stat;

	udelay(2);
	ch = info->zs_channel->data;
	junk = sgint->istat0;
	udelay(2);
	stat = read_zsreg(info->zs_channel, R1);

	/* If this is the console keyboard, we need to handle
	 * L1-A's here.
	 */
	if(info->is_cons) {
		if(ch==0) { /* whee, break received */
			batten_down_hatches();
			rs_recv_clear(info->zs_channel);
			return;
		} else if (ch == 1) {
			show_state();
			return;
		} else if (ch == 2) {
			show_buffers();
			return;
		}
	}
	/* Look for kgdb 'stop' character, consult the gdb documentation
	 * for remote target debugging and arch/sparc/kernel/sparc-stub.c
	 * to see how all this works.
	 */
#ifdef CONFIG_KGDB
	if((info->kgdb_channel) && (ch =='\003')) {
		set_async_breakpoint(read_32bit_cp0_register(CP0_EPC));
		goto clear_and_exit;
	}
#endif
	if(!tty)
		goto clear_and_exit;

	if (tty->flip.count >= TTY_FLIPBUF_SIZE)
		queue_task(&tty->flip.tqueue, &tq_timer);
	tty->flip.count++;
	if(stat & PAR_ERR)
		*tty->flip.flag_buf_ptr++ = TTY_PARITY;
	else if(stat & Rx_OVR)
		*tty->flip.flag_buf_ptr++ = TTY_OVERRUN;
	else if(stat & CRC_ERR)
		*tty->flip.flag_buf_ptr++ = TTY_FRAME;
	else
		*tty->flip.flag_buf_ptr++ = 0; /* XXX */
	*tty->flip.char_buf_ptr++ = ch;

	queue_task(&tty->flip.tqueue, &tq_timer);

clear_and_exit:
	rs_recv_clear(info->zs_channel);
	return;
}

static inline void transmit_chars(struct sgi_serial *info)
{
	volatile unsigned char junk;

	/* P3: In theory we have to test readiness here because a
	 * serial console can clog the chip through zs_cons_put_char().
	 * David did not do this. I think he relies on 3-chars FIFO in 8530.
	 * Let's watch for lost _output_ characters. XXX
	 */

	/* SGI ADDENDUM: On most SGI machines, the Zilog does possess
	 *               a 16 or 17 byte fifo, so no worries. -dm
	 */

	if (info->x_char) {
		/* Send next char */
		udelay(2);
		info->zs_channel->data = info->x_char;
		junk = sgint->istat0;

		info->x_char = 0;
		goto clear_and_return;
	}

	if((info->xmit_cnt <= 0) || info->tty->stopped) {
		/* That's peculiar... */
		udelay(2);
		info->zs_channel->control = RES_Tx_P;
		junk = sgint->istat0;
		goto clear_and_return;
	}

	/* Send char */
	udelay(2);
	info->zs_channel->data = info->xmit_buf[info->xmit_tail++];
	junk = sgint->istat0;

	info->xmit_tail = info->xmit_tail & (SERIAL_XMIT_SIZE-1);
	info->xmit_cnt--;

	if (info->xmit_cnt < WAKEUP_CHARS)
		rs_sched_event(info, RS_EVENT_WRITE_WAKEUP);

	if(info->xmit_cnt <= 0) {
		udelay(2);
		info->zs_channel->control = RES_Tx_P;
		junk = sgint->istat0;
		goto clear_and_return;
	}

clear_and_return:
	/* Clear interrupt */
	udelay(2);
	info->zs_channel->control = RES_H_IUS;
	junk = sgint->istat0;
	return;
}

static inline void status_handle(struct sgi_serial *info)
{
	volatile unsigned char junk;
	unsigned char status;

	/* Get status from Read Register 0 */
	udelay(2);
	status = info->zs_channel->control;
	junk = sgint->istat0;
	/* Clear status condition... */
	udelay(2);
	info->zs_channel->control = RES_EXT_INT;
	junk = sgint->istat0;
	/* Clear the interrupt */
	udelay(2);
	info->zs_channel->control = RES_H_IUS;
	junk = sgint->istat0;

#if 0
	if(status & DCD) {
		if((info->tty->termios->c_cflag & CRTSCTS) &&
		   ((info->curregs[3] & AUTO_ENAB)==0)) {
			info->curregs[3] |= AUTO_ENAB;
			write_zsreg(info->zs_channel, 3, info->curregs[3]);
		}
	} else {
		if((info->curregs[3] & AUTO_ENAB)) {
			info->curregs[3] &= ~AUTO_ENAB;
			write_zsreg(info->zs_channel, 3, info->curregs[3]);
		}
	}
#endif
	/* Whee, if this is console input and this is a
	 * 'break asserted' status change interrupt, call
	 * the boot prom.
	 */
	if((status & BRK_ABRT) && info->break_abort)
		batten_down_hatches();

	/* XXX Whee, put in a buffer somewhere, the status information
	 * XXX whee whee whee... Where does the information go...
	 */
	return;
}

/*
 * This is the serial driver's generic interrupt routine
 */
void rs_interrupt(int irq, void *dev_id, struct pt_regs * regs)
{
	struct sgi_serial * info = (struct sgi_serial *) dev_id;
	unsigned char zs_intreg;

	zs_intreg = read_zsreg(info->zs_next->zs_channel, 3);

	/* NOTE: The read register 3, which holds the irq status,
	 *       does so for both channels on each chip.  Although
	 *       the status value itself must be read from the A
	 *       channel and is only valid when read from channel A.
	 *       Yes... broken hardware...
	 */
#define CHAN_A_IRQMASK (CHARxIP | CHATxIP | CHAEXT)
#define CHAN_B_IRQMASK (CHBRxIP | CHBTxIP | CHBEXT)

	/* *** Chip 1 *** */
	/* Channel B -- /dev/ttyb, could be the console */
	if(zs_intreg & CHAN_B_IRQMASK) {
		if (zs_intreg & CHBRxIP)
			receive_chars(info, regs);
		if (zs_intreg & CHBTxIP)
			transmit_chars(info);
		if (zs_intreg & CHBEXT)
			status_handle(info);
	}

	info=info->zs_next;

	/* Channel A -- /dev/ttya, could be the console */
	if(zs_intreg & CHAN_A_IRQMASK) {
		if (zs_intreg & CHARxIP)
			receive_chars(info, regs);
		if (zs_intreg & CHATxIP)
			transmit_chars(info);
		if (zs_intreg & CHAEXT)
			status_handle(info);
	}
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
	struct sgi_serial	*info = (struct sgi_serial *) private_;
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
	struct sgi_serial	*info = (struct sgi_serial *) private_;
	struct tty_struct	*tty;

	tty = info->tty;
	if (!tty)
		return;

	tty_hangup(tty);
}


static int startup(struct sgi_serial * info)
{
	volatile unsigned char junk;
	unsigned long flags;

	if (info->flags & ZILOG_INITIALIZED)
		return 0;

	if (!info->xmit_buf) {
		info->xmit_buf = (unsigned char *) get_free_page(GFP_KERNEL);
		if (!info->xmit_buf)
			return -ENOMEM;
	}

	save_flags(flags); cli();

#ifdef SERIAL_DEBUG_OPEN
	printk("starting up ttys%d (irq %d)...\n", info->line, info->irq);
#endif

	/*
	 * Clear the FIFO buffers and disable them
	 * (they will be reenabled in change_speed())
	 */
	ZS_CLEARFIFO(info->zs_channel);
	info->xmit_fifo_size = 1;

	/*
	 * Clear the interrupt registers.
	 */
	udelay(2);
	info->zs_channel->control = ERR_RES;
	junk = sgint->istat0;
	udelay(2);
	info->zs_channel->control = RES_H_IUS;
	junk = sgint->istat0;

	/*
	 * Now, initialize the Zilog
	 */
	zs_rtsdtr(info, 1);

	/*
	 * Finally, enable sequencing and interrupts
	 */
	info->curregs[1] |= (info->curregs[1] & ~0x18) | (EXT_INT_ENAB|INT_ALL_Rx);
	info->curregs[3] |= (RxENABLE | Rx8);
	/* We enable Tx interrupts as needed. */
	info->curregs[5] |= (TxENAB | Tx8);
	info->curregs[9] |= (NV | MIE);
	write_zsreg(info->zs_channel, 3, info->curregs[3]);
	write_zsreg(info->zs_channel, 5, info->curregs[5]);
	write_zsreg(info->zs_channel, 9, info->curregs[9]);

	/*
	 * And clear the interrupt registers again for luck.
	 */
	udelay(2);
	info->zs_channel->control = ERR_RES;
	junk = sgint->istat0;
	udelay(2);
	info->zs_channel->control = RES_H_IUS;
	junk = sgint->istat0;

	if (info->tty)
		clear_bit(TTY_IO_ERROR, &info->tty->flags);
	info->xmit_cnt = info->xmit_head = info->xmit_tail = 0;

	/*
	 * and set the speed of the serial port
	 */
	change_speed(info);

	info->flags |= ZILOG_INITIALIZED;
	restore_flags(flags);
	return 0;
}

/*
 * This routine will shutdown a serial port; interrupts are disabled, and
 * DTR is dropped if the hangup on close termio flag is on.
 */
static void shutdown(struct sgi_serial * info)
{
	unsigned long	flags;

	if (!(info->flags & ZILOG_INITIALIZED))
		return;

#ifdef SERIAL_DEBUG_OPEN
	printk("Shutting down serial port %d (irq %d)....", info->line,
	       info->irq);
#endif

	save_flags(flags); cli(); /* Disable interrupts */

	if (info->xmit_buf) {
		free_page((unsigned long) info->xmit_buf);
		info->xmit_buf = 0;
	}

	if (info->tty)
		set_bit(TTY_IO_ERROR, &info->tty->flags);

	info->flags &= ~ZILOG_INITIALIZED;
	restore_flags(flags);
}

/*
 * This routine is called to set the UART divisor registers to match
 * the specified baud rate for a serial port.
 */
static void change_speed(struct sgi_serial *info)
{
	unsigned int port, cflag;
	int	i;
	int	brg;

	if (!info->tty || !info->tty->termios)
		return;
	cflag = info->tty->termios->c_cflag;
	if (!(port = info->port))
		return;
	i = cflag & CBAUD;
	if (i & CBAUDEX) {
		/* XXX CBAUDEX is not obeyed.
		 * It is impossible at a 32bits SPARC.
		 * But we have to report this to user ... someday.
		 */
		i = B9600;
	}
	if (i == 0) {
		/* XXX B0, hangup the line. */
		do_serial_hangup(info);
	} else if (baud_table[i]) {
		info->zs_baud = baud_table[i];
		info->clk_divisor = 16;

		info->curregs[4] = X16CLK;
		info->curregs[11] = TCBR | RCBR;
		brg = BPS_TO_BRG(info->zs_baud, ZS_CLOCK/info->clk_divisor);
		info->curregs[12] = (brg & 255);
		info->curregs[13] = ((brg >> 8) & 255);
		info->curregs[14] = BRENABL;
	}

	/* byte size and parity */
	switch (cflag & CSIZE) {
	case CS5:
		info->curregs[3] &= ~(0xc0);
		info->curregs[3] |= Rx5;
		info->curregs[5] &= ~(0xe0);
		info->curregs[5] |= Tx5;
		break;
	case CS6:
		info->curregs[3] &= ~(0xc0);
		info->curregs[3] |= Rx6;
		info->curregs[5] &= ~(0xe0);
		info->curregs[5] |= Tx6;
		break;
	case CS7:
		info->curregs[3] &= ~(0xc0);
		info->curregs[3] |= Rx7;
		info->curregs[5] &= ~(0xe0);
		info->curregs[5] |= Tx7;
		break;
	case CS8:
	default: /* defaults to 8 bits */
		info->curregs[3] &= ~(0xc0);
		info->curregs[3] |= Rx8;
		info->curregs[5] &= ~(0xe0);
		info->curregs[5] |= Tx8;
		break;
	}
	info->curregs[4] &= ~(0x0c);
	if (cflag & CSTOPB)
		info->curregs[4] |= SB2;
	else
		info->curregs[4] |= SB1;

	if (cflag & PARENB)
		info->curregs[4] |= PAR_ENA;
	else
		info->curregs[4] &= ~PAR_ENA;

	if (!(cflag & PARODD))
		info->curregs[4] |= PAR_EVEN;
	else
		info->curregs[4] &= ~PAR_EVEN;

	/* Load up the new values */
	load_zsregs(info->zs_channel, info->curregs);

	return;
}

/* This is for console output over ttya/ttyb */
static void zs_cons_put_char(char ch)
{
	struct sgi_zschannel *chan = zs_conschan;
	volatile unsigned char junk;
	unsigned long flags;
	int loops = 0;

	save_flags(flags); cli();
	while(((junk = chan->control) & Tx_BUF_EMP)==0 && loops < 10000) {
		loops++;
		udelay(2);
	}

	udelay(2);
	chan->data = ch;
	junk = sgint->istat0;
	restore_flags(flags);
}

/*
 * This is the more generic put_char function for the driver.
 * In earlier versions of this driver, "rs_put_char" was the
 * name of the console-specific fucntion, now called zs_cons_put_char
 */

static void rs_put_char(struct tty_struct *tty, char ch)
{
	struct sgi_zschannel *chan =
		((struct sgi_serial *)tty->driver_data)->zs_channel;
	volatile unsigned char junk;
	unsigned long flags;
	int loops = 0;

	save_flags(flags); cli();
	while(((junk = chan->control) & Tx_BUF_EMP)==0 && loops < 10000) {
		loops++;
		udelay(2);
	}

	udelay(2);
	chan->data = ch;
	junk = sgint->istat0;
	restore_flags(flags);
}

/* These are for receiving and sending characters under the kgdb
 * source level kernel debugger.
 */
int putDebugChar(char kgdb_char)
{
	struct sgi_zschannel *chan = zs_kgdbchan;
	volatile unsigned char junk;
	unsigned long flags;

	save_flags(flags); cli();
	udelay(2);
	while((chan->control & Tx_BUF_EMP)==0)
		udelay(2);

	udelay(2);
	chan->data = kgdb_char;
	junk = sgint->istat0;
	restore_flags(flags);

	return 1;
}

char getDebugChar(void)
{
	struct sgi_zschannel *chan = zs_kgdbchan;
	unsigned char junk;

	while((chan->control & Rx_CH_AV)==0)
		udelay(2);

	junk = sgint->istat0;
	udelay(2);
	return chan->data;
}

/*
 * Fair output driver allows a process to speak.
 */
static void rs_fair_output(void)
{
	int left;		/* Output no more than that */
	unsigned long flags;
	struct sgi_serial *info = zs_consinfo;
	volatile unsigned char junk;
	char c;

	if (info == 0) return;
	if (info->xmit_buf == 0) return;

	save_flags(flags);  cli();
	left = info->xmit_cnt;
	while (left != 0) {
		c = info->xmit_buf[info->xmit_tail];
		info->xmit_tail = (info->xmit_tail+1) & (SERIAL_XMIT_SIZE-1);
		info->xmit_cnt--;
		restore_flags(flags);

		zs_cons_put_char(c);

		save_flags(flags);  cli();
		left = min(info->xmit_cnt, left-1);
	}

	/* Last character is being transmitted now (hopefully). */
	udelay(2);
	zs_conschan->control = RES_Tx_P;
	junk = sgint->istat0;

	restore_flags(flags);
	return;
}


static int rs_write(struct tty_struct * tty, int from_user,
		    const unsigned char *buf, int count)
{
	int	c, total = 0;
	struct sgi_serial *info = (struct sgi_serial *)tty->driver_data;
	unsigned long flags;

	if (serial_paranoia_check(info, tty->device, "rs_write"))
		return 0;

	if (!tty || !info->xmit_buf)
		return 0;

	save_flags(flags);
	while (1) {
		cli();
		c = min(count, min(SERIAL_XMIT_SIZE - info->xmit_cnt - 1,
				   SERIAL_XMIT_SIZE - info->xmit_head));
		if (c <= 0)
			break;

		if (from_user) {
			down(&tmp_buf_sem);
			copy_from_user(tmp_buf, buf, c);
			c = min(c, min(SERIAL_XMIT_SIZE - info->xmit_cnt - 1,
				       SERIAL_XMIT_SIZE - info->xmit_head));
			memcpy(info->xmit_buf + info->xmit_head, tmp_buf, c);
			up(&tmp_buf_sem);
		} else
			memcpy(info->xmit_buf + info->xmit_head, buf, c);
		info->xmit_head = (info->xmit_head + c) & (SERIAL_XMIT_SIZE-1);
		info->xmit_cnt += c;
		restore_flags(flags);
		buf += c;
		count -= c;
		total += c;
	}

	if (info->xmit_cnt && !tty->stopped && !tty->hw_stopped) {
	/*
	 * The above test used to include the condition
 	 * "&& !(info->curregs[5] & TxENAB)", but there
	 * is reason to suspect that it is never statisfied
	 * when the port is running.  The problem may in fact
	 * have been masked by the fact that, if O_POST is set,
	 * there is always a rs_flush_xx operation following the
	 * rs_write, and the flush ignores that condition when
	 * it kicks off the transmit.
	 */
		/* Enable transmitter */
		info->curregs[1] |= TxINT_ENAB|EXT_INT_ENAB;
		write_zsreg(info->zs_channel, 1, info->curregs[1]);
		info->curregs[5] |= TxENAB;
		write_zsreg(info->zs_channel, 5, info->curregs[5]);

	/*
	 * The following code is imported from the 2.3.6 Sun sbus zs.c
	 * driver, of which an earlier version served as the basis
	 * for sgiserial.c.  Perhaps due to changes over time in
	 * the line discipline code, ns_write()s with from_user
	 * set would not otherwise actually kick-off output in
	 * Linux 2.2.x or later.  Maybe it never really worked.
	 */

		rs_put_char(tty, info->xmit_buf[info->xmit_tail++]);
                info->xmit_tail = info->xmit_tail & (SERIAL_XMIT_SIZE-1);
                info->xmit_cnt--;
	}

	restore_flags(flags);
	return total;
}

static int rs_write_room(struct tty_struct *tty)
{
	struct sgi_serial *info = (struct sgi_serial *)tty->driver_data;
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
	struct sgi_serial *info = (struct sgi_serial *)tty->driver_data;

	if (serial_paranoia_check(info, tty->device, "rs_chars_in_buffer"))
		return 0;
	return info->xmit_cnt;
}

static void rs_flush_buffer(struct tty_struct *tty)
{
	struct sgi_serial *info = (struct sgi_serial *)tty->driver_data;

	if (serial_paranoia_check(info, tty->device, "rs_flush_buffer"))
		return;
	cli();
	info->xmit_cnt = info->xmit_head = info->xmit_tail = 0;
	sti();
	wake_up_interruptible(&tty->write_wait);
	if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) &&
	    tty->ldisc.write_wakeup)
		(tty->ldisc.write_wakeup)(tty);
}

static void rs_flush_chars(struct tty_struct *tty)
{
	struct sgi_serial *info = (struct sgi_serial *)tty->driver_data;
	unsigned long flags;

	if (serial_paranoia_check(info, tty->device, "rs_flush_chars"))
		return;

	if (info->xmit_cnt <= 0 || tty->stopped || tty->hw_stopped ||
	    !info->xmit_buf)
		return;

	/* Enable transmitter */
	save_flags(flags); cli();
	info->curregs[1] |= TxINT_ENAB|EXT_INT_ENAB;
	write_zsreg(info->zs_channel, 1, info->curregs[1]);
	info->curregs[5] |= TxENAB;
	write_zsreg(info->zs_channel, 5, info->curregs[5]);

	/*
	 * Send a first (bootstrapping) character. A best solution is
	 * to call transmit_chars() here which handles output in a
	 * generic way. Current transmit_chars() not only transmits,
	 * but resets interrupts also what we do not desire here.
	 * XXX Discuss with David.
	 */
	if (info->zs_channel->control & Tx_BUF_EMP) {
		volatile unsigned char junk;

		/* Send char */
		udelay(2);
		info->zs_channel->data = info->xmit_buf[info->xmit_tail++];
		junk = sgint->istat0;
		info->xmit_tail = info->xmit_tail & (SERIAL_XMIT_SIZE-1);
		info->xmit_cnt--;
	}
	restore_flags(flags);
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
	struct sgi_serial *info = (struct sgi_serial *)tty->driver_data;
#ifdef SERIAL_DEBUG_THROTTLE
	char	buf[64];

	printk("throttle %s: %d....\n", _tty_name(tty, buf),
	       tty->ldisc.chars_in_buffer(tty));
#endif

	if (serial_paranoia_check(info, tty->device, "rs_throttle"))
		return;

	if (I_IXOFF(tty))
		info->x_char = STOP_CHAR(tty);

	/* Turn off RTS line */
	cli();
	info->curregs[5] &= ~RTS;
	write_zsreg(info->zs_channel, 5, info->curregs[5]);
	sti();
}

static void rs_unthrottle(struct tty_struct * tty)
{
	struct sgi_serial *info = (struct sgi_serial *)tty->driver_data;
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
			info->x_char = START_CHAR(tty);
	}

	/* Assert RTS line */
	cli();
	info->curregs[5] |= RTS;
	write_zsreg(info->zs_channel, 5, info->curregs[5]);
	sti();
}

/*
 * ------------------------------------------------------------
 * rs_ioctl() and friends
 * ------------------------------------------------------------
 */

static int get_serial_info(struct sgi_serial * info,
			   struct serial_struct * retinfo)
{
	struct serial_struct tmp;

	if (!retinfo)
		return -EFAULT;
	memset(&tmp, 0, sizeof(tmp));
	tmp.type = info->type;
	tmp.line = info->line;
	tmp.port = info->port;
	tmp.irq = info->irq;
	tmp.flags = info->flags;
	tmp.baud_base = info->baud_base;
	tmp.close_delay = info->close_delay;
	tmp.closing_wait = info->closing_wait;
	tmp.custom_divisor = info->custom_divisor;
	return copy_to_user(retinfo,&tmp,sizeof(*retinfo)) ? -EFAULT : 0;
}

static int set_serial_info(struct sgi_serial * info,
			   struct serial_struct * new_info)
{
	struct serial_struct new_serial;
	struct sgi_serial old_info;
	int 			retval = 0;

	if (!new_info)
		return -EFAULT;
	copy_from_user(&new_serial,new_info,sizeof(new_serial));
	old_info = *info;

	if (!capable(CAP_SYS_ADMIN)) {
		if ((new_serial.baud_base != info->baud_base) ||
		    (new_serial.type != info->type) ||
		    (new_serial.close_delay != info->close_delay) ||
		    ((new_serial.flags & ~ZILOG_USR_MASK) !=
		     (info->flags & ~ZILOG_USR_MASK)))
			return -EPERM;
		info->flags = ((info->flags & ~ZILOG_USR_MASK) |
			       (new_serial.flags & ZILOG_USR_MASK));
		info->custom_divisor = new_serial.custom_divisor;
		goto check_and_exit;
	}

	if (info->count > 1)
		return -EBUSY;

	/*
	 * OK, past this point, all the error checking has been done.
	 * At this point, we start making changes.....
	 */

	info->baud_base = new_serial.baud_base;
	info->flags = ((info->flags & ~ZILOG_FLAGS) |
			(new_serial.flags & ZILOG_FLAGS));
	info->type = new_serial.type;
	info->close_delay = new_serial.close_delay;
	info->closing_wait = new_serial.closing_wait;

check_and_exit:
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
static int get_lsr_info(struct sgi_serial * info, unsigned int *value)
{
	volatile unsigned char junk;
	unsigned char status;

	cli();
	udelay(2);
	status = info->zs_channel->control;
	junk = sgint->istat0;
	sti();
	return put_user(status,value);
}

static int get_modem_info(struct sgi_serial * info, unsigned int *value)
{
	unsigned char status;
	unsigned int result;

	cli();
	status = info->zs_channel->control;
	udelay(2);
	sti();
	result =  ((info->curregs[5] & RTS) ? TIOCM_RTS : 0)
		| ((info->curregs[5] & DTR) ? TIOCM_DTR : 0)
		| ((status  & DCD) ? TIOCM_CAR : 0)
		| ((status  & SYNC) ? TIOCM_DSR : 0)
		| ((status  & CTS) ? TIOCM_CTS : 0);
	if (put_user(result, value))
		return -EFAULT;
	return 0;
}

static int set_modem_info(struct sgi_serial * info, unsigned int cmd,
			  unsigned int *value)
{
	unsigned int arg;

	if (get_user(arg, value))
		return -EFAULT;
	switch (cmd) {
	case TIOCMBIS:
		if (arg & TIOCM_RTS)
			info->curregs[5] |= RTS;
		if (arg & TIOCM_DTR)
			info->curregs[5] |= DTR;
		break;
	case TIOCMBIC:
		if (arg & TIOCM_RTS)
			info->curregs[5] &= ~RTS;
		if (arg & TIOCM_DTR)
			info->curregs[5] &= ~DTR;
		break;
	case TIOCMSET:
		info->curregs[5] = ((info->curregs[5] & ~(RTS | DTR))
			     | ((arg & TIOCM_RTS) ? RTS : 0)
			     | ((arg & TIOCM_DTR) ? DTR : 0));
		break;
	default:
		return -EINVAL;
	}
	cli();
	write_zsreg(info->zs_channel, 5, info->curregs[5]);
	sti();
	return 0;
}

/*
 * This routine sends a break character out the serial port.
 */
static void send_break(	struct sgi_serial * info, int duration)
{
	if (!info->port)
		return;
	current->state = TASK_INTERRUPTIBLE;
	cli();
	write_zsreg(info->zs_channel, 5, (info->curregs[5] | SND_BRK));
	schedule_timeout(duration);
	write_zsreg(info->zs_channel, 5, info->curregs[5]);
	sti();
}

static int rs_ioctl(struct tty_struct *tty, struct file * file,
		    unsigned int cmd, unsigned long arg)
{
	struct sgi_serial * info = (struct sgi_serial *) tty->driver_data;
	int retval;

	if (serial_paranoia_check(info, tty->device, "zs_ioctl"))
		return -ENODEV;

	if ((cmd != TIOCGSERIAL) && (cmd != TIOCSSERIAL) &&
	    (cmd != TIOCSERCONFIG) && (cmd != TIOCSERGWILD)  &&
	    (cmd != TIOCSERSWILD) && (cmd != TIOCSERGSTRUCT)) {
		if (tty->flags & (1 << TTY_IO_ERROR))
		    return -EIO;
	}

	switch (cmd) {
		case TCSBRK:	/* SVID version: non-zero arg --> no break */
			retval = tty_check_change(tty);
			if (retval)
				return retval;
			tty_wait_until_sent(tty, 0);
			if (!arg)
				send_break(info, HZ/4);	/* 1/4 second */
			return 0;
		case TCSBRKP:	/* support for POSIX tcsendbreak() */
			retval = tty_check_change(tty);
			if (retval)
				return retval;
			tty_wait_until_sent(tty, 0);
			send_break(info, arg ? arg*(HZ/10) : HZ/4);
			return 0;
		case TIOCGSOFTCAR:
			if (put_user(C_CLOCAL(tty) ? 1 : 0,
				     (unsigned long *) arg))
				return -EFAULT;
			return 0;
		case TIOCSSOFTCAR:
			if (get_user(arg, (unsigned long *) arg))
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
			if (copy_to_user((struct sgi_serial *) arg,
				    info, sizeof(struct sgi_serial)))
				return -EFAULT;
			return 0;

		default:
			return -ENOIOCTLCMD;
		}
	return 0;
}

static void rs_set_termios(struct tty_struct *tty, struct termios *old_termios)
{
	struct sgi_serial *info = (struct sgi_serial *)tty->driver_data;

	if (tty->termios->c_cflag == old_termios->c_cflag)
		return;

	change_speed(info);

	if ((old_termios->c_cflag & CRTSCTS) &&
	    !(tty->termios->c_cflag & CRTSCTS)) {
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
 * ZILOG structure from the interrupt chain if necessary, and we free
 * that IRQ if nothing is left in the chain.
 * ------------------------------------------------------------
 */
static void rs_close(struct tty_struct *tty, struct file * filp)
{
	struct sgi_serial * info = (struct sgi_serial *)tty->driver_data;
	unsigned long flags;

	if (!info || serial_paranoia_check(info, tty->device, "rs_close"))
		return;

	save_flags(flags); cli();

	if (tty_hung_up_p(filp)) {
		restore_flags(flags);
		return;
	}

#ifdef SERIAL_DEBUG_OPEN
	printk("rs_close ttys%d, count = %d\n", info->line, info->count);
#endif
	if ((tty->count == 1) && (info->count != 1)) {
		/*
		 * Uh, oh.  tty->count is 1, which means that the tty
		 * structure will be freed.  Info->count should always
		 * be one in these conditions.  If it's greater than
		 * one, we've got real problems, since it means the
		 * serial port won't be shutdown.
		 */
		printk("rs_close: bad serial port count; tty->count is 1, "
		       "info->count is %d\n", info->count);
		info->count = 1;
	}
	if (--info->count < 0) {
		printk("rs_close: bad serial port count for ttys%d: %d\n",
		       info->line, info->count);
		info->count = 0;
	}
	if (info->count) {
		restore_flags(flags);
		return;
	}
	info->flags |= ZILOG_CLOSING;
	/*
	 * Save the termios structure, since this port may have
	 * separate termios for callout and dialin.
	 */
	if (info->flags & ZILOG_NORMAL_ACTIVE)
		info->normal_termios = *tty->termios;
	if (info->flags & ZILOG_CALLOUT_ACTIVE)
		info->callout_termios = *tty->termios;
	/*
	 * Now we wait for the transmit buffer to clear; and we notify
	 * the line discipline to only process XON/XOFF characters.
	 */
	tty->closing = 1;
	if (info->closing_wait != ZILOG_CLOSING_WAIT_NONE)
		tty_wait_until_sent(tty, info->closing_wait);
	/*
	 * At this point we stop accepting input.  To do this, we
	 * disable the receive line status interrupts, and tell the
	 * interrupt driver to stop checking the data ready bit in the
	 * line status register.
	 */
	/** if (!info->iscons) ... **/
	info->curregs[3] &= ~RxENABLE;
	write_zsreg(info->zs_channel, 3, info->curregs[3]);
	info->curregs[1] &= ~(0x18);
	write_zsreg(info->zs_channel, 1, info->curregs[1]);
	ZS_CLEARFIFO(info->zs_channel);

	shutdown(info);
	if (tty->driver.flush_buffer)
		tty->driver.flush_buffer(tty);
	if (tty->ldisc.flush_buffer)
		tty->ldisc.flush_buffer(tty);
	tty->closing = 0;
	info->event = 0;
	info->tty = 0;
	if (tty->ldisc.num != ldiscs[N_TTY].num) {
		if (tty->ldisc.close)
			(tty->ldisc.close)(tty);
		tty->ldisc = ldiscs[N_TTY];
		tty->termios->c_line = N_TTY;
		if (tty->ldisc.open)
			(tty->ldisc.open)(tty);
	}
	if (info->blocked_open) {
		if (info->close_delay) {
			current->state = TASK_INTERRUPTIBLE;
			schedule_timeout(info->close_delay);
		}
		wake_up_interruptible(&info->open_wait);
	}
	info->flags &= ~(ZILOG_NORMAL_ACTIVE|ZILOG_CALLOUT_ACTIVE|
			 ZILOG_CLOSING);
	wake_up_interruptible(&info->close_wait);
	restore_flags(flags);
}

/*
 * rs_hangup() --- called by tty_hangup() when a hangup is signaled.
 */
void rs_hangup(struct tty_struct *tty)
{
	struct sgi_serial * info = (struct sgi_serial *)tty->driver_data;

	if (serial_paranoia_check(info, tty->device, "rs_hangup"))
		return;

	rs_flush_buffer(tty);
	shutdown(info);
	info->event = 0;
	info->count = 0;
	info->flags &= ~(ZILOG_NORMAL_ACTIVE|ZILOG_CALLOUT_ACTIVE);
	info->tty = 0;
	wake_up_interruptible(&info->open_wait);
}

/*
 * ------------------------------------------------------------
 * rs_open() and friends
 * ------------------------------------------------------------
 */
static int block_til_ready(struct tty_struct *tty, struct file * filp,
			   struct sgi_serial *info)
{
	DECLARE_WAITQUEUE(wait, current);
	int		retval;
	int		do_clocal = 0;

	/*
	 * If the device is in the middle of being closed, then block
	 * until it's done, and then try again.
	 */
	if (info->flags & ZILOG_CLOSING) {
		interruptible_sleep_on(&info->close_wait);
#ifdef SERIAL_DO_RESTART
		if (info->flags & ZILOG_HUP_NOTIFY)
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
		if (info->flags & ZILOG_NORMAL_ACTIVE)
			return -EBUSY;
		if ((info->flags & ZILOG_CALLOUT_ACTIVE) &&
		    (info->flags & ZILOG_SESSION_LOCKOUT) &&
		    (info->session != current->session))
		    return -EBUSY;
		if ((info->flags & ZILOG_CALLOUT_ACTIVE) &&
		    (info->flags & ZILOG_PGRP_LOCKOUT) &&
		    (info->pgrp != current->pgrp))
		    return -EBUSY;
		info->flags |= ZILOG_CALLOUT_ACTIVE;
		return 0;
	}

	/*
	 * If non-blocking mode is set, or the port is not enabled,
	 * then make the check up front and then exit.
	 */
	if ((filp->f_flags & O_NONBLOCK) ||
	    (tty->flags & (1 << TTY_IO_ERROR))) {
		if (info->flags & ZILOG_CALLOUT_ACTIVE)
			return -EBUSY;
		info->flags |= ZILOG_NORMAL_ACTIVE;
		return 0;
	}

	if (info->flags & ZILOG_CALLOUT_ACTIVE) {
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
	 * rs_close() knows when to free things.  We restore it upon
	 * exit, either normal or abnormal.
	 */
	retval = 0;
	add_wait_queue(&info->open_wait, &wait);
#ifdef SERIAL_DEBUG_OPEN
	printk("block_til_ready before block: ttys%d, count = %d\n",
	       info->line, info->count);
#endif
	info->count--;
	info->blocked_open++;
	while (1) {
		cli();
		if (!(info->flags & ZILOG_CALLOUT_ACTIVE))
			zs_rtsdtr(info, 1);
		sti();
		set_current_state(TASK_INTERRUPTIBLE);
		if (tty_hung_up_p(filp) ||
		    !(info->flags & ZILOG_INITIALIZED)) {
#ifdef SERIAL_DO_RESTART
			if (info->flags & ZILOG_HUP_NOTIFY)
				retval = -EAGAIN;
			else
				retval = -ERESTARTSYS;
#else
			retval = -EAGAIN;
#endif
			break;
		}
		if (!(info->flags & ZILOG_CALLOUT_ACTIVE) &&
		    !(info->flags & ZILOG_CLOSING) && do_clocal)
			break;
		if (signal_pending(current)) {
			retval = -ERESTARTSYS;
			break;
		}
#ifdef SERIAL_DEBUG_OPEN
		printk("block_til_ready blocking: ttys%d, count = %d\n",
		       info->line, info->count);
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
	info->flags |= ZILOG_NORMAL_ACTIVE;
	return 0;
}

/*
 * This routine is called whenever a serial port is opened.  It
 * enables interrupts for a serial port, linking in its ZILOG structure into
 * the IRQ chain.   It also performs the serial-specific
 * initialization for the tty structure.
 */
int rs_open(struct tty_struct *tty, struct file * filp)
{
	struct sgi_serial	*info;
	int 			retval, line;

	line = MINOR(tty->device) - tty->driver.minor_start;
	/* The zilog lines for the mouse/keyboard must be
	 * opened using their respective drivers.
	 */
	if ((line < 0) || (line >= NUM_CHANNELS))
		return -ENODEV;
	info = zs_soft + line;
	/* Is the kgdb running over this line? */
	if (info->kgdb_channel)
		return -ENODEV;
	if (serial_paranoia_check(info, tty->device, "rs_open"))
		return -ENODEV;
#ifdef SERIAL_DEBUG_OPEN
	printk("rs_open %s%d, count = %d\n", tty->driver.name, info->line,
	       info->count);
#endif
	info->count++;
	tty->driver_data = info;
	info->tty = tty;

	/*
	 * Start up serial port
	 */
	retval = startup(info);
	if (retval)
		return retval;

	retval = block_til_ready(tty, filp, info);
	if (retval) {
#ifdef SERIAL_DEBUG_OPEN
		printk("rs_open returning after block_til_ready with %d\n",
		       retval);
#endif
		return retval;
	}

	if ((info->count == 1) && (info->flags & ZILOG_SPLIT_TERMIOS)) {
		if (tty->driver.subtype == SERIAL_TYPE_NORMAL)
			*tty->termios = info->normal_termios;
		else
			*tty->termios = info->callout_termios;
		change_speed(info);
	}

	/* If this is the serial console change the speed to
	 * the right value
	 */
	if (info->is_cons) {
		info->tty->termios->c_cflag = sgisercon->cflag;
		change_speed(info);
	}

	info->session = current->session;
	info->pgrp = current->pgrp;

#ifdef SERIAL_DEBUG_OPEN
	printk("rs_open ttys%d successful...\n", info->line);
#endif
	return 0;
}

/* Finally, routines used to initialize the serial driver. */

static void show_serial_version(void)
{
	printk("SGI Zilog8530 serial driver version 1.00\n");
}

/* Return layout for the requested zs chip number. */
static inline struct sgi_zslayout *get_zs(int chip)
{
	if (chip > 0)
		panic("Wheee, bogus zs chip number requested.");

	return (struct sgi_zslayout *) (&sgioc->serport);
}


static inline void
rs_cons_check(struct sgi_serial *ss, int channel)
{
	int i, o, io;
	static int msg_printed = 0;

	i = o = io = 0;

	/* Is this one of the serial console lines? */
	if((zs_cons_chanout != channel) &&
	   (zs_cons_chanin != channel))
		return;
	zs_conschan = ss->zs_channel;
	zs_consinfo = ss;



	/* If this is console input, we handle the break received
	 * status interrupt on this line to mean prom_halt().
	 */
	if(zs_cons_chanin == channel) {
		ss->break_abort = 1;
		i = 1;
	}
	if(o && i)
		io = 1;

	/* Set flag variable for this port so that it cannot be
	 * opened for other uses by accident.
	 */
	ss->is_cons = 1;

	if(io) {
		if (!msg_printed) {
			printk("zs%d: console I/O\n", ((channel>>1)&1));
			msg_printed = 1;
		}

	} else {
		printk("zs%d: console %s\n", ((channel>>1)&1),
		       (i==1 ? "input" : (o==1 ? "output" : "WEIRD")));
	}
}

/* rs_init inits the driver */
int rs_init(void)
{
	unsigned long flags;
	int chip, channel, i;
	struct sgi_serial *info;


	/* Setup base handler, and timer table. */
	init_bh(SERIAL_BH, do_serial_bh);

	show_serial_version();

	/* Initialize the tty_driver structure */
	/* SGI: Not all of this is exactly right for us. */

	memset(&serial_driver, 0, sizeof(struct tty_driver));
	serial_driver.magic = TTY_DRIVER_MAGIC;
#ifdef CONFIG_DEVFS_FS
	serial_driver.name = "tts/%d";
#else
	serial_driver.name = "ttyS";
#endif
	serial_driver.major = TTY_MAJOR;
	serial_driver.minor_start = 64;
	serial_driver.num = NUM_CHANNELS;
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
	serial_driver.flush_chars = rs_flush_chars;
	serial_driver.write_room = rs_write_room;
	serial_driver.chars_in_buffer = rs_chars_in_buffer;
	serial_driver.flush_buffer = rs_flush_buffer;
	serial_driver.ioctl = rs_ioctl;
	serial_driver.throttle = rs_throttle;
	serial_driver.unthrottle = rs_unthrottle;
	serial_driver.set_termios = rs_set_termios;
	serial_driver.stop = rs_stop;
	serial_driver.start = rs_start;
	serial_driver.hangup = rs_hangup;

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

	if (tty_register_driver(&serial_driver))
		panic("Couldn't register serial driver");
	if (tty_register_driver(&callout_driver))
		panic("Couldn't register callout driver");

	save_flags(flags); cli();

	/* Set up our interrupt linked list */
	zs_chain = &zs_soft[0];
	zs_soft[0].zs_next = &zs_soft[1];
	zs_soft[1].zs_next = 0;

	for(chip = 0; chip < NUM_SERIAL; chip++) {
		/* If we are doing kgdb over one of the channels on
		 * chip zero, kgdb_channel will be set to 1 by the
		 * rs_kgdb_hook() routine below.
		 */
		if(!zs_chips[chip]) {
			zs_chips[chip] = get_zs(chip);
			/* Two channels per chip */
			zs_channels[(chip*2)] = &zs_chips[chip]->channelB;
			zs_channels[(chip*2)+1] = &zs_chips[chip]->channelA;
			zs_soft[(chip*2)].kgdb_channel = 0;
			zs_soft[(chip*2)+1].kgdb_channel = 0;
		}
		/* First, set up channel A on this chip. */
		channel = chip * 2;
		zs_soft[channel].zs_channel = zs_channels[channel];
		zs_soft[channel].change_needed = 0;
		zs_soft[channel].clk_divisor = 16;
		zs_soft[channel].zs_baud = get_zsbaud(&zs_soft[channel]);
		zs_soft[channel].cons_mouse = 0;
		/* If not keyboard/mouse and is console serial
		 * line, then enable receiver interrupts.
		 */
		if(zs_soft[channel].is_cons) {
			write_zsreg(zs_soft[channel].zs_channel, R1,
				    (EXT_INT_ENAB | INT_ALL_Rx));
			write_zsreg(zs_soft[channel].zs_channel, R9, (NV | MIE));
			write_zsreg(zs_soft[channel].zs_channel, R10, (NRZ));
			write_zsreg(zs_soft[channel].zs_channel, R3, (Rx8|RxENABLE));
			write_zsreg(zs_soft[channel].zs_channel, R5, (Tx8 | TxENAB));
		}
		/* If this is the kgdb line, enable interrupts because we
		 * now want to receive the 'control-c' character from the
		 * client attached to us asynchronously.
		 */
		if(zs_soft[channel].kgdb_channel)
			kgdb_chaninit(&zs_soft[channel], 1,
				      zs_soft[channel].zs_baud);

		/* Now, channel B */
		channel++;
		zs_soft[channel].zs_channel = zs_channels[channel];
		zs_soft[channel].change_needed = 0;
		zs_soft[channel].clk_divisor = 16;
		zs_soft[channel].zs_baud = get_zsbaud(&zs_soft[channel]);
		zs_soft[channel].cons_keyb = 0;
		/* If console serial line, then enable receiver interrupts. */
		if(zs_soft[channel].is_cons) {
			write_zsreg(zs_soft[channel].zs_channel, R1,
				    (EXT_INT_ENAB | INT_ALL_Rx));
			write_zsreg(zs_soft[channel].zs_channel, R9,
				    (NV | MIE));
			write_zsreg(zs_soft[channel].zs_channel, R10,
				    (NRZ));
			write_zsreg(zs_soft[channel].zs_channel, R3,
				    (Rx8|RxENABLE));
			write_zsreg(zs_soft[channel].zs_channel, R5,
				    (Tx8 | TxENAB | RTS | DTR));
		}
	}

	for(info=zs_chain, i=0; info; info = info->zs_next, i++)
	{
		info->magic = SERIAL_MAGIC;
		info->port = (int) info->zs_channel;
		info->line = i;
		info->tty = 0;
		info->irq = zilog_irq;
		info->custom_divisor = 16;
		info->close_delay = 50;
		info->closing_wait = 3000;
		info->x_char = 0;
		info->event = 0;
		info->count = 0;
		info->blocked_open = 0;
		info->tqueue.routine = do_softint;
		info->tqueue.data = info;
		info->tqueue_hangup.routine = do_serial_hangup;
		info->tqueue_hangup.data = info;
		info->callout_termios =callout_driver.init_termios;
		info->normal_termios = serial_driver.init_termios;
		init_waitqueue_head(&info->open_wait);
		init_waitqueue_head(&info->close_wait);
		printk("tty%02d at 0x%04x (irq = %d)", info->line,
		       info->port, info->irq);
		printk(" is a Zilog8530\n");
	}

	if (request_irq(zilog_irq, rs_interrupt, (SA_INTERRUPT),
			"Zilog8530", zs_chain))
		panic("Unable to attach zs intr");
	restore_flags(flags);

	return 0;
}

/*
 * register_serial and unregister_serial allows for serial ports to be
 * configured at run-time, to support PCMCIA modems.
 */
/* SGI: Unused at this time, just here to make things link. */
int register_serial(struct serial_struct *req)
{
	return -1;
}

void unregister_serial(int line)
{
	return;
}

/* Hooks for running a serial console.  con_init() calls this if the
 * console is being run over one of the ttya/ttyb serial ports.
 * 'chip' should be zero, as chip 1 drives the mouse/keyboard.
 * 'channel' is decoded as 0=TTYA 1=TTYB, note that the channels
 * are addressed backwards, channel B is first, then channel A.
 */
void
rs_cons_hook(int chip, int out, int line)
{
	int channel;

	if(chip)
		panic("rs_cons_hook called with chip not zero");
	if(line != 0 && line != 1)
		panic("rs_cons_hook called with line not ttya or ttyb");
	channel = line;
	if(!zs_chips[chip]) {
		zs_chips[chip] = get_zs(chip);
		/* Two channels per chip */
		zs_channels[(chip*2)] = &zs_chips[chip]->channelB;
		zs_channels[(chip*2)+1] = &zs_chips[chip]->channelA;
	}
	zs_soft[channel].zs_channel = zs_channels[channel];
	zs_soft[channel].change_needed = 0;
	zs_soft[channel].clk_divisor = 16;
	zs_soft[channel].zs_baud = get_zsbaud(&zs_soft[channel]);
	if(out)
		zs_cons_chanout = ((chip * 2) + channel);
	else
		zs_cons_chanin = ((chip * 2) + channel);

	rs_cons_check(&zs_soft[channel], channel);
}

/* This is called at boot time to prime the kgdb serial debugging
 * serial line.  The 'tty_num' argument is 0 for /dev/ttyd2 and 1 for
 * /dev/ttyd1 (yes they are backwards on purpose) which is determined
 * in setup_arch() from the boot command line flags.
 */
void
rs_kgdb_hook(int tty_num)
{
	int chip = 0;

	if(!zs_chips[chip]) {
		zs_chips[chip] = get_zs(chip);
		/* Two channels per chip */
		zs_channels[(chip*2)] = &zs_chips[chip]->channelA;
		zs_channels[(chip*2)+1] = &zs_chips[chip]->channelB;
	}
	zs_soft[tty_num].zs_channel = zs_channels[tty_num];
	zs_kgdbchan = zs_soft[tty_num].zs_channel;
	zs_soft[tty_num].change_needed = 0;
	zs_soft[tty_num].clk_divisor = 16;
	zs_soft[tty_num].zs_baud = get_zsbaud(&zs_soft[tty_num]);
	zs_soft[tty_num].kgdb_channel = 1;     /* This runs kgdb */
	zs_soft[tty_num ^ 1].kgdb_channel = 0; /* This does not */

	/* Turn on transmitter/receiver at 8-bits/char */
	kgdb_chaninit(&zs_soft[tty_num], 0, 9600);
	ZS_CLEARERR(zs_kgdbchan);
	udelay(5);
	ZS_CLEARFIFO(zs_kgdbchan);
}

static void zs_console_write(struct console *co, const char *str,
                             unsigned int count)
{

	while(count--) {
		if(*str == '\n')
			zs_cons_put_char('\r');
		zs_cons_put_char(*str++);
	}

	/* Comment this if you want to have a strict interrupt-driven output */
	rs_fair_output();
}

static kdev_t zs_console_device(struct console *con)
{
	return MKDEV(TTY_MAJOR, 64 + con->index);
}


static int __init zs_console_setup(struct console *con, char *options)
{
	struct sgi_serial *info;
	int baud = 9600;
	int bits = 8;
	int parity = 'n';
	int cflag = CREAD | HUPCL | CLOCAL;
	char *s;
	int i, brg;

	if(options) {
		baud = simple_strtoul(options, NULL, 10);
		s = options;
		while(*s >= '0' && *s <= '9')
			s++;
		if (*s) parity = *s++;
		if (*s) bits   = *s - '0';
	}
	/* Now construct a cflag setting. */
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
	con->cflag = cflag;

        rs_cons_hook(0, 0, con->index);
	info = zs_soft + con->index;
	info->is_cons = 1;

	printk("Console: ttyS%d (Zilog8530), %d baud\n",
						info->line, baud);

	i = con->cflag & CBAUD;
	if (con->cflag & CBAUDEX) {
		i &= ~CBAUDEX;
		con->cflag &= ~CBAUDEX;
	}
	info->zs_baud = baud;

	switch (con->cflag & CSIZE) {
		case CS5:
			zscons_regs[3] = Rx5 | RxENABLE;
			zscons_regs[5] = Tx5 | TxENAB;
			break;
		case CS6:
			zscons_regs[3] = Rx6 | RxENABLE;
			zscons_regs[5] = Tx6 | TxENAB;
			break;
		case CS7:
			zscons_regs[3] = Rx7 | RxENABLE;
			zscons_regs[5] = Tx7 | TxENAB;
			break;
		default:
		case CS8:
			zscons_regs[3] = Rx8 | RxENABLE;
			zscons_regs[5] = Tx8 | TxENAB;
			break;
	}
	zscons_regs[5] |= DTR;

	if (con->cflag & PARENB)
		zscons_regs[4] |= PAR_ENA;
	if (!(con->cflag & PARODD))
		zscons_regs[4] |= PAR_EVEN;

	if (con->cflag & CSTOPB)
		zscons_regs[4] |= SB2;
	else
		zscons_regs[4] |= SB1;

	sgisercon = con;

	brg = BPS_TO_BRG(baud, ZS_CLOCK / info->clk_divisor);
	zscons_regs[12] = brg & 0xff;
	zscons_regs[13] = (brg >> 8) & 0xff;
	memcpy(info->curregs, zscons_regs, sizeof(zscons_regs));
	load_zsregs(info->zs_channel, zscons_regs);
	ZS_CLEARERR(info->zs_channel);
	ZS_CLEARFIFO(info->zs_channel);
	return 0;
}

static struct console sgi_console_driver = {
	.name		= "ttyS",
	.write		= zs_console_write,
	.device		= zs_console_device,
	.setup		= zs_console_setup,
	.flags		= CON_PRINTBUFFER,
	.index		= -1,
};

/*
 *	Register console.
 */
void __init sgi_serial_console_init(void)
{
	register_console(&sgi_console_driver);
}
__initcall(rs_init);
