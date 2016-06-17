/* $Id: zs.c,v 1.68.2.2 2002/01/12 07:04:33 davem Exp $
 * zs.c: Zilog serial port driver for the Sparc.
 *
 * Copyright (C) 1995 David S. Miller (davem@caip.rutgers.edu)
 * Copyright (C) 1996 Eddie C. Dost   (ecd@skynet.be)
 * Fixes by Pete A. Zaitcev <zaitcev@yahoo.com>.
 *
 * Fixed to use tty_get_baud_rate().
 *   Theodore Ts'o <tytso@mit.edu>, 2001-Oct-12
 *
 * /proc/tty/driver/serial now exists and is readable.
 *   Alex Buell <alex.buell@tahallah.demon.co.uk>, 2001-12-23
 *
 */

#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/config.h>
#include <linux/major.h>
#include <linux/string.h>
#include <linux/fcntl.h>
#include <linux/mm.h>
#include <linux/kernel.h>
#include <linux/keyboard.h>
#include <linux/console.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/bootmem.h>
#include <linux/sysrq.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/oplib.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/bitops.h>
#include <asm/kdebug.h>
#include <asm/page.h>
#include <asm/pgtable.h>

#include <asm/sbus.h>
#ifdef __sparc_v9__
#include <asm/fhc.h>
#endif
#ifdef CONFIG_PCI
#include <linux/pci.h>
#endif

#include "sunserial.h"
#include "zs.h"
#include "sunkbd.h"
#include "sunmouse.h"

static int num_serial = 2; /* sun4/sun4c/sun4m - Two chips on board. */
#define NUM_SERIAL num_serial
#define NUM_CHANNELS (NUM_SERIAL * 2)

#define KEYBOARD_LINE 0x2
#define MOUSE_LINE    0x3

/* On 32-bit sparcs we need to delay after register accesses
 * to accomodate sun4 systems, but we do not need to flush writes.
 * On 64-bit sparc we only need to flush single writes to ensure
 * completion.
 */
#ifndef __sparc_v9__
#define ZSDELAY()		udelay(5)
#define ZSDELAY_LONG()		udelay(20)
#define ZS_WSYNC(channel)	do { } while(0)
#else
#define ZSDELAY()
#define ZSDELAY_LONG()
#define ZS_WSYNC(__channel) \
	sbus_readb(&((__channel)->control))
#endif

struct sun_zslayout **zs_chips;
struct sun_zschannel **zs_channels;
struct sun_zschannel *zs_mousechan;
struct sun_zschannel *zs_kbdchan;
struct sun_zschannel *zs_kgdbchan;
int *zs_nodes;

struct sun_serial *zs_soft;
struct sun_serial *zs_chain;  /* IRQ servicing chain */
int zilog_irq;

struct tty_struct *zs_ttys;

/* Console hooks... */
#ifdef CONFIG_SERIAL_CONSOLE
static struct console zs_console;
static int zs_console_init(void);

/*
 * Define this to get the zs_fair_output() functionality.
 */
#undef SERIAL_CONSOLE_FAIR_OUTPUT
#endif /* CONFIG_SERIAL_CONSOLE */

static unsigned char kgdb_regs[16] = {
	0, 0, 0,                     /* write 0, 1, 2 */
	(Rx8 | RxENAB),              /* write 3 */
	(X16CLK | SB1 | PAR_EVEN),   /* write 4 */
	(DTR | Tx8 | TxENAB),        /* write 5 */
	0, 0, 0,                     /* write 6, 7, 8 */
	(NV),                        /* write 9 */
	(NRZ),                       /* write 10 */
	(TCBR | RCBR),               /* write 11 */
	0, 0,                        /* BRG time constant, write 12 + 13 */
	(BRSRC | BRENAB),            /* write 14 */
	(DCDIE)                      /* write 15 */
};

static unsigned char zscons_regs[16] = {
	0,                           /* write 0 */
	(EXT_INT_ENAB | INT_ALL_Rx), /* write 1 */
	0,                           /* write 2 */
	(Rx8 | RxENAB),              /* write 3 */
	(X16CLK),                    /* write 4 */
	(DTR | Tx8 | TxENAB),        /* write 5 */
	0, 0, 0,                     /* write 6, 7, 8 */
	(NV | MIE),                  /* write 9 */
	(NRZ),                       /* write 10 */
	(TCBR | RCBR),               /* write 11 */
	0, 0,                        /* BRG time constant, write 12 + 13 */
	(BRSRC | BRENAB),            /* write 14 */
	(DCDIE | CTSIE | TxUIE | BRKIE) /* write 15 */
};

#define ZS_CLOCK         4915200   /* Zilog input clock rate */

DECLARE_TASK_QUEUE(tq_serial);

static struct tty_driver serial_driver, callout_driver;
static int serial_refcount;

/* serial subtype definitions */
#define SERIAL_TYPE_NORMAL	1
#define SERIAL_TYPE_CALLOUT	2
  
/* number of characters left in xmit buffer before we ask for more */
#define WAKEUP_CHARS 256

#define SERIAL_DO_RESTART

/* Debugging... DEBUG_INTR is bad to use when one of the zs
 * lines is your console ;(
 */
#undef SERIAL_DEBUG_INTR
#undef SERIAL_DEBUG_OPEN
#undef SERIAL_DEBUG_FLOW

#define RS_STROBE_TIME 10
#define RS_ISR_PASS_LIMIT 256

#define _INLINE_ inline

int zs_init(void);
static void zs_kgdb_hook(int);

static void change_speed(struct sun_serial *info);

static struct tty_struct **serial_table;
static struct termios **serial_termios;
static struct termios **serial_termios_locked;

#ifndef MIN
#define MIN(a,b)	((a) < (b) ? (a) : (b))
#endif

#undef ZS_LOG
#ifdef ZS_LOG
struct zs_logent {
	u8 reg, val;
	u8 write, __pad;
#define REGIRQ	0xff
#define REGDATA	0xfe
#define REGCTRL	0xfd
};
struct zs_logent zslog[32];
int zs_curlog;
#define ZSLOG(__reg, __val, __write) \
do{	int index = zs_curlog; \
	zslog[index].reg = (__reg); \
	zslog[index].val = (__val); \
	zslog[index].write = (__write); \
	zs_curlog = (index + 1) & (32 - 1); \
}while(0)
int zs_dumplog(char *buffer)
{
	int len = 0;
	int i;

	for (i = 0; i < 32; i++) {
		u8 reg, val, write;

		reg = zslog[i].reg;
		val = zslog[i].val;
		write = zslog[i].write;
		len += sprintf(buffer + len,
			       "ZSLOG[%2d]: reg %2x val %2x %s\n",
			       i, reg, val, write ? "write" : "read");
	}
	len += sprintf(buffer + len, "ZS current log index %d\n",
		       zs_curlog);
	return len;
}
#else
#define ZSLOG(x,y,z)	do { } while (0)
#endif

/*
 * tmp_buf is used as a temporary buffer by serial_write.  We need to
 * lock it in case the memcpy_fromfs blocks while swapping in a page,
 * and some other program tries to do a serial write at the same time.
 * Since the lock will only come under contention when the system is
 * swapping and available memory is low, it makes sense to share one
 * buffer across all the serial ports, since it significantly saves
 * memory if large numbers of serial ports are open.
 */
static unsigned char *tmp_buf = 0;
static DECLARE_MUTEX(tmp_buf_sem);

static inline int serial_paranoia_check(struct sun_serial *info,
					dev_t device, const char *routine)
{
#ifdef SERIAL_PARANOIA_CHECK
	static const char *badmagic =
		"Warning: bad magic number for serial struct (%d, %d) in %s\n";
	static const char *badinfo =
		"Warning: null sun_serial for (%d, %d) in %s\n";

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

/* Reading and writing Zilog8530 registers.  The delays are to make this
 * driver work on the Sun4 which needs a settling delay after each chip
 * register access, other machines handle this in hardware via auxiliary
 * flip-flops which implement the settle time we do in software.
 */
static unsigned char read_zsreg(struct sun_zschannel *channel,
				unsigned char reg)
{
	unsigned char retval;

	sbus_writeb(reg, &channel->control);
	ZSDELAY();
	retval = sbus_readb(&channel->control);
	ZSDELAY();
	ZSLOG(reg, retval, 0);
	return retval;
}

static void write_zsreg(struct sun_zschannel *channel,
			unsigned char reg, unsigned char value)
{
	ZSLOG(reg, value, 1);
	sbus_writeb(reg, &channel->control);
	ZSDELAY();
	sbus_writeb(value, &channel->control);
	ZSDELAY();
}

static void load_zsregs(struct sun_serial *info, unsigned char *regs)
{
	struct sun_zschannel *channel = info->zs_channel;
	unsigned long flags;
	unsigned char stat;
	int i;

	for (i = 0; i < 1000; i++) {
		stat = read_zsreg(channel, R1);
		if (stat & ALL_SNT)
			break;
		udelay(100);
	}
	write_zsreg(channel, R3, 0);
	ZS_CLEARSTAT(channel);
	ZS_CLEARERR(channel);
	ZS_CLEARFIFO(channel);

	/* Load 'em up */
	save_flags(flags); cli();
	if (info->channelA)
		write_zsreg(channel, R9, CHRA);
	else
		write_zsreg(channel, R9, CHRB);
	ZSDELAY_LONG();
	write_zsreg(channel, R4, regs[R4]);
	write_zsreg(channel, R3, regs[R3] & ~RxENAB);
	write_zsreg(channel, R5, regs[R5] & ~TxENAB);
	write_zsreg(channel, R9, regs[R9] & ~MIE);
	write_zsreg(channel, R10, regs[R10]);
	write_zsreg(channel, R11, regs[R11]);
	write_zsreg(channel, R12, regs[R12]);
	write_zsreg(channel, R13, regs[R13]);
	write_zsreg(channel, R14, regs[R14] & ~BRENAB);
	write_zsreg(channel, R14, regs[R14]);
	write_zsreg(channel, R14, (regs[R14] & ~SNRZI) | BRENAB);
	write_zsreg(channel, R3, regs[R3]);
	write_zsreg(channel, R5, regs[R5]);
	write_zsreg(channel, R15, regs[R15]);
	write_zsreg(channel, R0, RES_EXT_INT);
	write_zsreg(channel, R0, ERR_RES);
	write_zsreg(channel, R1, regs[R1]);
	write_zsreg(channel, R9, regs[R9]);
	restore_flags(flags);
}

#define ZS_PUT_CHAR_MAX_DELAY	2000	/* 10 ms */

static void zs_put_char(struct sun_zschannel *channel, char ch)
{
	int loops = ZS_PUT_CHAR_MAX_DELAY;

	/* Do not change this to use ZSDELAY as this is
	 * a timed polling loop and on sparc64 ZSDELAY
	 * is a nop.  -DaveM
	 */
	do {
		u8 val = sbus_readb(&channel->control);
		ZSLOG(REGCTRL, val, 0);
		if (val & Tx_BUF_EMP)
			break;
		udelay(5);
	} while (--loops);

	sbus_writeb(ch, &channel->data);
	ZSDELAY();
	ZS_WSYNC(channel);
	ZSLOG(REGDATA, ch, 1);
}

/* Sets or clears DTR/RTS on the requested line */
static void zs_rtsdtr(struct sun_serial *ss, int set)
{
	unsigned long flags;

	save_flags(flags); cli();
	if(set) {
		ss->curregs[5] |= (RTS | DTR);
		write_zsreg(ss->zs_channel, 5, ss->curregs[5]);
	} else {
		ss->curregs[5] &= ~(RTS | DTR);
		write_zsreg(ss->zs_channel, 5, ss->curregs[5]);
	}
	restore_flags(flags);
	return;
}

static void kgdb_chaninit(struct sun_serial *ss, int intson, int bps)
{
	int brg;

	if(intson) {
		kgdb_regs[R1] = INT_ALL_Rx;
		kgdb_regs[R9] |= MIE;
	} else {
		kgdb_regs[R1] = 0;
		kgdb_regs[R9] &= ~MIE;
	}
	brg = BPS_TO_BRG(bps, ZS_CLOCK/16);
	kgdb_regs[R12] = (brg & 255);
	kgdb_regs[R13] = ((brg >> 8) & 255);
	load_zsregs(ss, kgdb_regs);
}

/*
 * ------------------------------------------------------------
 * zs_stop() and zs_start()
 *
 * This routines are called before setting or resetting tty->stopped.
 * They enable or disable transmitter interrupts, as necessary.
 * ------------------------------------------------------------
 */
static void zs_stop(struct tty_struct *tty)
{
	struct sun_serial *info = (struct sun_serial *) tty->driver_data;
	unsigned long flags;

	if (serial_paranoia_check(info, tty->device, "zs_stop"))
		return;
	
	save_flags(flags); cli();
	if (info->curregs[5] & TxENAB) {
		info->curregs[5] &= ~TxENAB;
		write_zsreg(info->zs_channel, 5, info->curregs[5]);
	}
	restore_flags(flags);
}

static void zs_start(struct tty_struct *tty)
{
	struct sun_serial *info = (struct sun_serial *) tty->driver_data;
	unsigned long flags;
	
	if (serial_paranoia_check(info, tty->device, "zs_start"))
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
void batten_down_hatches(void)
{
	if (!stop_a_enabled)
		return;
	/* If we are doing kadb, we call the debugger
	 * else we just drop into the boot monitor.
	 * Note that we must flush the user windows
	 * first before giving up control.
	 */
	printk("\n");
	flush_user_windows();
#ifndef __sparc_v9__
	if((((unsigned long)linux_dbvec)>=DEBUG_FIRSTVADDR) &&
	   (((unsigned long)linux_dbvec)<=DEBUG_LASTVADDR))
		sp_enter_debugger();
	else
#endif
		prom_cmdline();

	/* XXX We want to notify the keyboard driver that all
	 * XXX keys are in the up state or else weird things
	 * XXX happen...
	 */

	return;
}


/*
 * ----------------------------------------------------------------------
 *
 * Here starts the interrupt handling routines.  All of the following
 * subroutines are declared as inline and are folded into
 * zs_interrupt().  They were separated out for readability's sake.
 *
 * Note: zs_interrupt() is a "fast" interrupt, which means that it
 * runs with interrupts turned off.  People who may want to modify
 * zs_interrupt() should try to keep the interrupt handler as fast as
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
static void zs_sched_event(struct sun_serial *info, int event)
{
	info->event |= 1 << event;
	queue_task(&info->tqueue, &tq_serial);
	mark_bh(SERIAL_BH);
}

#ifndef __sparc_v9__
extern void breakpoint(void);  /* For the KGDB frame character */
#endif

static void receive_chars(struct sun_serial *info, struct pt_regs *regs)
{
	struct tty_struct *tty = info->tty;
	int do_queue_task = 0;

	while (1) {
		unsigned char ch, r1;

		r1 = read_zsreg(info->zs_channel, R1);
		if (r1 & (PAR_ERR | Rx_OVR | CRC_ERR)) {
			sbus_writeb(ERR_RES, &info->zs_channel->control);
			ZSDELAY();
			ZS_WSYNC(info->zs_channel);
			ZSLOG(REGCTRL, ERR_RES, 1);
		}

		ch = sbus_readb(&info->zs_channel->data);
		ZSLOG(REGDATA, ch, 0);
		ch &= info->parity_mask;
		ZSDELAY();

		/* If this is the console keyboard, we need to handle
		 * L1-A's here.
		 */
		if (info->cons_keyb) {
			if (ch == SUNKBD_RESET) {
				l1a_state.kbd_id = 1;
				l1a_state.l1_down = 0;
			} else if (l1a_state.kbd_id) {
				l1a_state.kbd_id = 0;
			} else if (ch == SUNKBD_L1) {
				l1a_state.l1_down = 1;
			} else if (ch == (SUNKBD_L1|SUNKBD_UP)) {
				l1a_state.l1_down = 0;
			} else if (ch == SUNKBD_A && l1a_state.l1_down) {
				/* whee... */
				batten_down_hatches();
				/* Continue execution... */
				l1a_state.l1_down = 0;
				l1a_state.kbd_id = 0;
				return;
			}
			sunkbd_inchar(ch, regs);
			goto next_char;
		}
		if (info->cons_mouse) {
			sun_mouse_inbyte(ch, 0);
			goto next_char;
		}
		if (info->is_cons) {
			if (ch == 0) {
				/* whee, break received */
				batten_down_hatches();
				/* Continue execution... */
				return;
			}
		}
#ifndef __sparc_v9__
		/* Look for kgdb 'stop' character, consult the gdb
		 * documentation for remote target debugging and
		 * arch/sparc/kernel/sparc-stub.c to see how all this works.
		 */
		if (info->kgdb_channel && (ch =='\003')) {
			breakpoint();
			return;
		}
#endif
		if (!tty)
			return;

		do_queue_task++;

		if (tty->flip.count >= TTY_FLIPBUF_SIZE)
			break;

		tty->flip.count++;
		if (r1 & PAR_ERR)
			*tty->flip.flag_buf_ptr++ = TTY_PARITY;
		else if (r1 & Rx_OVR)
			*tty->flip.flag_buf_ptr++ = TTY_OVERRUN;
		else if (r1 & CRC_ERR)
			*tty->flip.flag_buf_ptr++ = TTY_FRAME;
		else
			*tty->flip.flag_buf_ptr++ = 0;
		*tty->flip.char_buf_ptr++ = ch;

	next_char:
		{
			unsigned char stat;

			/* Check if we have another character... */
			stat = sbus_readb(&info->zs_channel->control);
			ZSDELAY();
			ZSLOG(REGCTRL, stat, 0);
			if (!(stat & Rx_CH_AV))
				break;
		}
	}

	if (do_queue_task != 0)
		queue_task(&tty->flip.tqueue, &tq_timer);
}

static void transmit_chars(struct sun_serial *info)
{
	struct tty_struct *tty = info->tty;

	if (info->x_char) {
		/* Send next char */
		zs_put_char(info->zs_channel, info->x_char);
		info->x_char = 0;
		return;
	}

	if ((info->xmit_cnt <= 0) || (tty != 0 && tty->stopped)) {
		/* That's peculiar... */
		sbus_writeb(RES_Tx_P, &info->zs_channel->control);
		ZSDELAY();
		ZS_WSYNC(info->zs_channel);
		ZSLOG(REGCTRL, RES_Tx_P, 1);
		return;
	}

	/* Send char */
	zs_put_char(info->zs_channel, info->xmit_buf[info->xmit_tail++]);
	info->xmit_tail = info->xmit_tail & (SERIAL_XMIT_SIZE-1);
	info->xmit_cnt--;

	if (info->xmit_cnt < WAKEUP_CHARS)
		zs_sched_event(info, RS_EVENT_WRITE_WAKEUP);

	if (info->xmit_cnt <= 0) {
		sbus_writeb(RES_Tx_P, &info->zs_channel->control);
		ZSDELAY();
		ZS_WSYNC(info->zs_channel);
		ZSLOG(REGCTRL, RES_Tx_P, 1);
	}
}

static void status_handle(struct sun_serial *info)
{
	unsigned char status;

	/* Get status from Read Register 0 */
	status = sbus_readb(&info->zs_channel->control);
	ZSDELAY();
	ZSLOG(REGCTRL, status, 0);
	/* Clear status condition... */
	sbus_writeb(RES_EXT_INT, &info->zs_channel->control);
	ZSDELAY();
	ZS_WSYNC(info->zs_channel);
	ZSLOG(REGCTRL, RES_EXT_INT, 1);
#if 0
	if (status & DCD) {
		if ((info->tty->termios->c_cflag & CRTSCTS) &&
		    ((info->curregs[3] & AUTO_ENAB)==0)) {
			info->curregs[3] |= AUTO_ENAB;
			write_zsreg(info->zs_channel, 3, info->curregs[3]);
		}
	} else {
		if ((info->curregs[3] & AUTO_ENAB)) {
			info->curregs[3] &= ~AUTO_ENAB;
			write_zsreg(info->zs_channel, 3, info->curregs[3]);
		}
	}
#endif
	/* Whee, if this is console input and this is a
	 * 'break asserted' status change interrupt, call
	 * the boot prom.
	 */
	if (status & BRK_ABRT) {
		if (info->break_abort)
			batten_down_hatches();
		if (info->cons_mouse)
			sun_mouse_inbyte(0, 1);
	}

	/* XXX Whee, put in a buffer somewhere, the status information
	 * XXX whee whee whee... Where does the information go...
	 */
	return;
}

/*
 * This is the serial driver's generic interrupt routine
 */
void zs_interrupt(int irq, void *dev_id, struct pt_regs * regs)
{
	struct sun_serial *info;
	int i;

	info = (struct sun_serial *)dev_id;
	ZSLOG(REGIRQ, 0, 0);
	for (i = 0; i < NUM_SERIAL; i++) {
		unsigned char r3 = read_zsreg(info->zs_channel, 3);

		/* Channel A -- /dev/ttya or /dev/kbd, could be the console */
		if (r3 & (CHAEXT | CHATxIP | CHARxIP)) {
			sbus_writeb(RES_H_IUS, &info->zs_channel->control);
			ZSDELAY();
			ZS_WSYNC(info->zs_channel);
			ZSLOG(REGCTRL, RES_H_IUS, 1);
			if (r3 & CHARxIP)
				receive_chars(info, regs);
			if (r3 & CHAEXT)
				status_handle(info);
			if (r3 & CHATxIP)
				transmit_chars(info);
		}

		/* Channel B -- /dev/ttyb or /dev/mouse, could be the console */
		info = info->zs_next;
		if (r3 & (CHBEXT | CHBTxIP | CHBRxIP)) {
			sbus_writeb(RES_H_IUS, &info->zs_channel->control);
			ZSDELAY();
			ZS_WSYNC(info->zs_channel);
			ZSLOG(REGCTRL, RES_H_IUS, 1);
			if (r3 & CHBRxIP)
				receive_chars(info, regs);
			if (r3 & CHBEXT)
				status_handle(info);
			if (r3 & CHBTxIP)
				transmit_chars(info);
		}

		info = info->zs_next;
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
 * zs_interrupt() has returned, BUT WITH INTERRUPTS TURNED ON.  This
 * is where time-consuming activities which can not be done in the
 * interrupt driver proper are done; the interrupt driver schedules
 * them using zs_sched_event(), and they get done here.
 */
static void do_serial_bh(void)
{
	run_task_queue(&tq_serial);
}

static void do_softint(void *private_)
{
	struct sun_serial	*info = (struct sun_serial *) private_;
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
 * 	do_serial_hangup() -> tty->hangup() -> zs_hangup()
 * 
 */
static void do_serial_hangup(void *private_)
{
	struct sun_serial	*info = (struct sun_serial *) private_;
	struct tty_struct	*tty;
	
	tty = info->tty;
	if (!tty)
		return;
#ifdef SERIAL_DEBUG_OPEN
	printk("do_serial_hangup<%p: tty-%d\n",
		__builtin_return_address(0), info->line);
#endif

	tty_hangup(tty);
}

static int startup(struct sun_serial * info)
{
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
	printk("Starting up tty-%d (irq %d)...\n", info->line, info->irq);
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
	sbus_writeb(ERR_RES, &info->zs_channel->control);
	ZSDELAY();
	ZS_WSYNC(info->zs_channel);
	ZSLOG(REGCTRL, ERR_RES, 1);

	sbus_writeb(RES_H_IUS, &info->zs_channel->control);
	ZSDELAY();
	ZS_WSYNC(info->zs_channel);
	ZSLOG(REGCTRL, RES_H_IUS, 1);

	/*
	 * Now, initialize the Zilog
	 */
	zs_rtsdtr(info, 1);

	/*
	 * Finally, enable sequencing and interrupts
	 */
	info->curregs[1] |= (info->curregs[1] & ~(RxINT_MASK)) |
				(EXT_INT_ENAB | INT_ALL_Rx);
	info->curregs[3] |= (RxENAB | Rx8);
	/* We enable Tx interrupts as needed. */
	info->curregs[5] |= (TxENAB | Tx8);
	info->curregs[9] |= (NV | MIE);
	write_zsreg(info->zs_channel, 3, info->curregs[3]);
	write_zsreg(info->zs_channel, 5, info->curregs[5]);
	write_zsreg(info->zs_channel, 9, info->curregs[9]);
	
	/*
	 * And clear the interrupt registers again for luck.
	 */
	sbus_writeb(ERR_RES, &info->zs_channel->control);
	ZSDELAY();
	ZS_WSYNC(info->zs_channel);
	ZSLOG(REGCTRL, ERR_RES, 1);

	sbus_writeb(RES_H_IUS, &info->zs_channel->control);
	ZSDELAY();
	ZS_WSYNC(info->zs_channel);
	ZSLOG(REGCTRL, RES_H_IUS, 1);

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
static void shutdown(struct sun_serial * info)
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
static void change_speed(struct sun_serial *info)
{
	unsigned cflag;
	int	baud, quot = 0;
	int	brg;

	if (!info->tty || !info->tty->termios)
		return;
	cflag = info->tty->termios->c_cflag;
	if (!info->port)
		return;
	baud = tty_get_baud_rate(info->tty);
	
	if ((baud == 38400) && 
	    ((info->flags & ZILOG_SPD_MASK) == ZILOG_SPD_CUST))
		quot = info->custom_divisor;

	if (quot) {
		info->zs_baud = info->baud_base / quot;
		info->clk_divisor = 16;

		info->curregs[4] = X16CLK;
		info->curregs[11] = TCBR | RCBR;
		brg = BPS_TO_BRG(info->zs_baud, ZS_CLOCK/info->clk_divisor);
		info->curregs[12] = (brg & 255);
		info->curregs[13] = ((brg >> 8) & 255);
		info->curregs[14] = BRSRC | BRENAB;
		zs_rtsdtr(info, 1);
	} else if (baud) {
		info->zs_baud = baud;
		info->clk_divisor = 16;

		info->curregs[4] = X16CLK;
		info->curregs[11] = TCBR | RCBR;
		brg = BPS_TO_BRG(info->zs_baud, ZS_CLOCK/info->clk_divisor);
		info->curregs[12] = (brg & 255);
		info->curregs[13] = ((brg >> 8) & 255);
		info->curregs[14] = BRSRC | BRENAB;
		zs_rtsdtr(info, 1);
	} else {
		zs_rtsdtr(info, 0);
		return;
	}

	/* byte size and parity */
	switch (cflag & CSIZE) {
	case CS5:
		info->curregs[3] &= ~(RxN_MASK);
		info->curregs[3] |= Rx5;
		info->curregs[5] &= ~(TxN_MASK);
		info->curregs[5] |= Tx5;
		info->parity_mask = 0x1f;
		break;
	case CS6:
		info->curregs[3] &= ~(RxN_MASK);
		info->curregs[3] |= Rx6;
		info->curregs[5] &= ~(TxN_MASK);
		info->curregs[5] |= Tx6;
		info->parity_mask = 0x3f;
		break;
	case CS7:
		info->curregs[3] &= ~(RxN_MASK);
		info->curregs[3] |= Rx7;
		info->curregs[5] &= ~(TxN_MASK);
		info->curregs[5] |= Tx7;
		info->parity_mask = 0x7f;
		break;
	case CS8:
	default: /* defaults to 8 bits */
		info->curregs[3] &= ~(RxN_MASK);
		info->curregs[3] |= Rx8;
		info->curregs[5] &= ~(TxN_MASK);
		info->curregs[5] |= Tx8;
		info->parity_mask = 0xff;
		break;
	}
	info->curregs[4] &= ~(0x0c);
	if (cflag & CSTOPB) {
		info->curregs[4] |= SB2;
	} else {
		info->curregs[4] |= SB1;
	}
	if (cflag & PARENB) {
		info->curregs[4] |= PAR_ENAB;
	} else {
		info->curregs[4] &= ~PAR_ENAB;
	}
	if (!(cflag & PARODD)) {
		info->curregs[4] |= PAR_EVEN;
	} else {
		info->curregs[4] &= ~PAR_EVEN;
	}

	/* Load up the new values */
	load_zsregs(info, info->curregs);

	return;
}

/* This is for mouse/keyboard output.
 * XXX mouse output??? can we send it commands??? XXX
 */
static void kbd_put_char(unsigned char ch)
{
	struct sun_zschannel *chan = zs_kbdchan;
	unsigned long flags;

	if(!chan)
		return;

	save_flags(flags); cli();
	zs_put_char(chan, ch);
	restore_flags(flags);
}

void mouse_put_char(char ch)
{
	struct sun_zschannel *chan = zs_mousechan;
	unsigned long flags;

	if(!chan)
		return;

	save_flags(flags); cli();
	zs_put_char(chan, ch);
	restore_flags(flags);
}

/* These are for receiving and sending characters under the kgdb
 * source level kernel debugger.
 */
void putDebugChar(char kgdb_char)
{
	struct sun_zschannel *chan = zs_kgdbchan;

	while((sbus_readb(&chan->control) & Tx_BUF_EMP)==0)
		udelay(5);
	sbus_writeb(kgdb_char, &chan->data);
	ZS_WSYNC(chan);
	ZSLOG(REGDATA, kgdb_char, 1);
}

char getDebugChar(void)
{
	struct sun_zschannel *chan = zs_kgdbchan;
	u8 val;

	do {
		val = sbus_readb(&chan->control);
		ZSLOG(REGCTRL, val, 0);
		udelay(5);
	} while ((val & Rx_CH_AV) == 0);

	val = sbus_readb(&chan->data);
	ZSLOG(REGDATA, val, 0);
	return val;
}

static void zs_flush_chars(struct tty_struct *tty)
{
	struct sun_serial *info = (struct sun_serial *) tty->driver_data;
	unsigned long flags;

	if (serial_paranoia_check(info, tty->device, "zs_flush_chars"))
		return;

	save_flags(flags); cli();
	if (info->xmit_cnt <= 0 || tty->stopped || tty->hw_stopped ||
	    !info->xmit_buf)
		goto out;

	/* Enable transmitter */
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
	zs_put_char(info->zs_channel, info->xmit_buf[info->xmit_tail++]);
	info->xmit_tail = info->xmit_tail & (SERIAL_XMIT_SIZE-1);
	info->xmit_cnt--;

out:
	restore_flags(flags);
}

static int zs_write(struct tty_struct * tty, int from_user,
		    const unsigned char *buf, int count)
{
	struct sun_serial *info = (struct sun_serial *) tty->driver_data;
	unsigned long flags;
	int c, total = 0;

	if (serial_paranoia_check(info, tty->device, "zs_write"))
		return 0;

	if (!info || !info->xmit_buf || !tmp_buf)
		return 0;

	save_flags(flags);
	if (from_user) {
		down(&tmp_buf_sem);
		while (1) {
			c = MIN(count, MIN(SERIAL_XMIT_SIZE - info->xmit_cnt - 1,
					   SERIAL_XMIT_SIZE - info->xmit_head));
			if (c <= 0)
				break;
			c -= copy_from_user(tmp_buf, buf, c);
			if (!c) {
				if (!total)
					total = -EFAULT;
				break;
			}
			cli();
			c = MIN(c, MIN(SERIAL_XMIT_SIZE - info->xmit_cnt - 1,
				       SERIAL_XMIT_SIZE - info->xmit_head));
			memcpy(info->xmit_buf + info->xmit_head, tmp_buf, c);
			info->xmit_head = ((info->xmit_head + c) &
					   (SERIAL_XMIT_SIZE - 1));
			info->xmit_cnt += c;
			restore_flags(flags);

			buf += c;
			count -= c;
			total += c;
		}
		up(&tmp_buf_sem);
	} else {
		while (1) {
			cli();		
			c = MIN(count, MIN(SERIAL_XMIT_SIZE - info->xmit_cnt - 1,
					   SERIAL_XMIT_SIZE - info->xmit_head));
			if (c <= 0) {
				restore_flags(flags);
				break;
			}
			memcpy(info->xmit_buf + info->xmit_head, buf, c);
			info->xmit_head = ((info->xmit_head + c) &
					   (SERIAL_XMIT_SIZE - 1));
			info->xmit_cnt += c;
			restore_flags(flags);
			buf += c;
			count -= c;
			total += c;
		}
	}

	cli();		
	if (info->xmit_cnt && !tty->stopped && !tty->hw_stopped) {
		/* Enable transmitter */
		info->curregs[1] |= TxINT_ENAB|EXT_INT_ENAB;
		write_zsreg(info->zs_channel, 1, info->curregs[1]);
		info->curregs[5] |= TxENAB;
		write_zsreg(info->zs_channel, 5, info->curregs[5]);
#if 1
		zs_put_char(info->zs_channel,
			    info->xmit_buf[info->xmit_tail++]);
		info->xmit_tail = info->xmit_tail & (SERIAL_XMIT_SIZE-1);
		info->xmit_cnt--;
#endif
	}

	restore_flags(flags);
	return total;
}

static int zs_write_room(struct tty_struct *tty)
{
	struct sun_serial *info = (struct sun_serial *) tty->driver_data;
	int ret;

	if (serial_paranoia_check(info, tty->device, "zs_write_room"))
		return 0;
	ret = SERIAL_XMIT_SIZE - info->xmit_cnt - 1;
	if (ret < 0)
		ret = 0;
	return ret;
}

static int zs_chars_in_buffer(struct tty_struct *tty)
{
	struct sun_serial *info = (struct sun_serial *) tty->driver_data;

	if (serial_paranoia_check(info, tty->device, "zs_chars_in_buffer"))
		return 0;
	return info->xmit_cnt;
}

static void zs_flush_buffer(struct tty_struct *tty)
{
	struct sun_serial *info = (struct sun_serial *) tty->driver_data;

	if (serial_paranoia_check(info, tty->device, "zs_flush_buffer"))
		return;
	cli();
	info->xmit_cnt = info->xmit_head = info->xmit_tail = 0;
	sti();
	wake_up_interruptible(&tty->write_wait);
	if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) &&
	    tty->ldisc.write_wakeup)
		(tty->ldisc.write_wakeup)(tty);
}

/*
 * ------------------------------------------------------------
 * zs_throttle()
 * 
 * This routine is called by the upper-layer tty layer to signal that
 * incoming characters should be throttled.
 * ------------------------------------------------------------
 */
static void zs_throttle(struct tty_struct * tty)
{
	struct sun_serial *info = (struct sun_serial *) tty->driver_data;
#ifdef SERIAL_DEBUG_THROTTLE
	char	buf[64];
	
	printk("throttle %s: %d....\n", _tty_name(tty, buf),
	       tty->ldisc.chars_in_buffer(tty));
#endif

	if (serial_paranoia_check(info, tty->device, "zs_throttle"))
		return;
	
	if (I_IXOFF(tty))
		info->x_char = STOP_CHAR(tty);

	/* Turn off RTS line */
	cli();
	info->curregs[5] &= ~RTS;
	write_zsreg(info->zs_channel, 5, info->curregs[5]);
	sti();
}

static void zs_unthrottle(struct tty_struct * tty)
{
	struct sun_serial *info = (struct sun_serial *) tty->driver_data;
#ifdef SERIAL_DEBUG_THROTTLE
	char	buf[64];
	
	printk("unthrottle %s: %d....\n", _tty_name(tty, buf),
	       tty->ldisc.chars_in_buffer(tty));
#endif

	if (serial_paranoia_check(info, tty->device, "zs_unthrottle"))
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
 * zs_ioctl() and friends
 * ------------------------------------------------------------
 */

static int get_serial_info(struct sun_serial * info,
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
	if (copy_to_user(retinfo,&tmp,sizeof(*retinfo)))
		return -EFAULT;
	return 0;
}

static int set_serial_info(struct sun_serial * info,
			   struct serial_struct * new_info)
{
	struct serial_struct new_serial;
	struct sun_serial old_info;
	int retval = 0;

	if (!new_info || copy_from_user(&new_serial,new_info,sizeof(new_serial)))
		return -EFAULT;
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

	if(new_serial.baud_base < 9600)
		return -EINVAL;

	if (info->count > 1)
		return -EBUSY;

	/*
	 * OK, past this point, all the error checking has been done.
	 * At this point, we start making changes.....
	 */

	info->baud_base = new_serial.baud_base;
	info->flags = ((info->flags & ~ZILOG_FLAGS) |
			(new_serial.flags & ZILOG_FLAGS));
	info->custom_divisor = new_serial.custom_divisor;
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
static int get_lsr_info(struct sun_serial * info, unsigned int *value)
{
	unsigned char status;

	cli();
	status = sbus_readb(&info->zs_channel->control);
	ZSDELAY();
	ZSLOG(REGCTRL, status, 0);
	sti();
	if (put_user(status, value))
		return -EFAULT;
	return 0;
}

static int get_modem_info(struct sun_serial * info, unsigned int *value)
{
	unsigned char status;
	unsigned int result;

	cli();
	status = sbus_readb(&info->zs_channel->control);
	ZSDELAY();
	ZSLOG(REGCTRL, status, 0);
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

static int set_modem_info(struct sun_serial * info, unsigned int cmd,
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
static void send_break(	struct sun_serial * info, int duration)
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

static int zs_ioctl(struct tty_struct *tty, struct file * file,
		    unsigned int cmd, unsigned long arg)
{
	struct sun_serial * info = (struct sun_serial *) tty->driver_data;
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
			if (copy_to_user((struct sun_serial *) arg,
				    info, sizeof(struct sun_serial)))
				return -EFAULT;
			return 0;
			
		default:
			return -ENOIOCTLCMD;
		}
	return 0;
}

static void zs_set_termios(struct tty_struct *tty, struct termios *old_termios)
{
	struct sun_serial *info = (struct sun_serial *) tty->driver_data;

	if (tty->termios->c_cflag == old_termios->c_cflag)
		return;

	change_speed(info);

	if ((old_termios->c_cflag & CRTSCTS) &&
	    !(tty->termios->c_cflag & CRTSCTS)) {
		tty->hw_stopped = 0;
		zs_start(tty);
	}
}

/*
 * ------------------------------------------------------------
 * zs_close()
 * 
 * This routine is called when the serial port gets closed.  First, we
 * wait for the last remaining data to be sent.  Then, we unlink its
 * ZILOG structure from the interrupt chain if necessary, and we free
 * that IRQ if nothing is left in the chain.
 * ------------------------------------------------------------
 */
static void zs_close(struct tty_struct *tty, struct file * filp)
{
	struct sun_serial * info = (struct sun_serial *) tty->driver_data;
	unsigned long flags;

	if (!info || serial_paranoia_check(info, tty->device, "zs_close"))
		return;
	
	save_flags(flags); cli();
	
	if (tty_hung_up_p(filp)) {
		restore_flags(flags);
		return;
	}
	
#ifdef SERIAL_DEBUG_OPEN
	printk("zs_close tty-%d, count = %d\n", info->line, info->count);
#endif
	if ((tty->count == 1) && (info->count != 1)) {
		/*
		 * Uh, oh.  tty->count is 1, which means that the tty
		 * structure will be freed.  Info->count should always
		 * be one in these conditions.  If it's greater than
		 * one, we've got real problems, since it means the
		 * serial port won't be shutdown.
		 */
		printk("zs_close: bad serial port count; tty->count is 1, "
		       "info->count is %d\n", info->count);
		info->count = 1;
	}
	if (--info->count < 0) {
		printk("zs_close: bad serial port count for ttys%d: %d\n",
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
	info->curregs[3] &= ~RxENAB;
	write_zsreg(info->zs_channel, 3, info->curregs[3]);
	info->curregs[1] &= ~(RxINT_MASK);
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
#ifdef SERIAL_DEBUG_OPEN
	printk("zs_close tty-%d exiting, count = %d\n", info->line, info->count);
#endif
	restore_flags(flags);
}

/*
 * zs_hangup() --- called by tty_hangup() when a hangup is signaled.
 */
void zs_hangup(struct tty_struct *tty)
{
	struct sun_serial * info = (struct sun_serial *) tty->driver_data;

	if (serial_paranoia_check(info, tty->device, "zs_hangup"))
		return;

	if (info->is_cons)
		return;

#ifdef SERIAL_DEBUG_OPEN
	printk("zs_hangup<%p: tty-%d, count = %d bye\n",
		__builtin_return_address(0), info->line, info->count);
#endif

	zs_flush_buffer(tty);
	shutdown(info);
	info->event = 0;
	info->count = 0;
	info->flags &= ~(ZILOG_NORMAL_ACTIVE|ZILOG_CALLOUT_ACTIVE);
	info->tty = 0;
	wake_up_interruptible(&info->open_wait);
}

/*
 *
 * line_info - returns information about each channel
 *
 */
static inline int line_info(char *buf, struct sun_serial *info)
{
	unsigned char status;
	char stat_buf[30];
	int ret;

	ret = sprintf(buf, "%d: uart:Zilog8530 port:%x irq:%d",
		info->line, info->port, info->irq);

	cli();
	status = sbus_readb(&info->zs_channel->control);
	ZSDELAY();
	ZSLOG(REGCTRL, status, 0);
	sti();

	stat_buf[0] = 0;
	stat_buf[1] = 0;
	if (info->curregs[5] & RTS)
		strcat(stat_buf, "|RTS");
	if (status & CTS)
		strcat(stat_buf, "|CTS");
	if (info->curregs[5] & DTR)
		strcat(stat_buf, "|DTR");
	if (status & SYNC)
		strcat(stat_buf, "|DSR");
	if (status & DCD)
		strcat(stat_buf, "|CD");

	ret += sprintf(buf + ret, " baud:%d %s\n", info->zs_baud, stat_buf + 1);
	return ret;
}

/*
 *
 * zs_read_proc() - called when /proc/tty/driver/serial is read.
 *
 */
int zs_read_proc(char *page, char **start, off_t off, int count,
                 int *eof, void *data)
{
	char *revision = "$Revision: 1.68.2.2 $";
	char *version, *p;
	int i, len = 0, l;
	off_t begin = 0;

	version = strchr(revision, ' ');
	p = strchr(++version, ' ');
	*p = '\0';
	len += sprintf(page, "serinfo:1.0 driver:%s\n", version);
	*p = ' ';

	for (i = 0; i < NUM_CHANNELS && len < 4000; i++) {
		l = line_info(page + len, &zs_soft[i]);
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
 * ------------------------------------------------------------
 * zs_open() and friends
 * ------------------------------------------------------------
 */
static int block_til_ready(struct tty_struct *tty, struct file * filp,
			   struct sun_serial *info)
{
	DECLARE_WAITQUEUE(wait, current);
	int retval, do_clocal = 0;
	unsigned char r0;

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
	 * zs_close() knows when to free things.  We restore it upon
	 * exit, either normal or abnormal.
	 */
	retval = 0;
	add_wait_queue(&info->open_wait, &wait);
#ifdef SERIAL_DEBUG_OPEN
	printk("block_til_ready before block: ttys%d, count = %d\n",
	       info->line, info->count);
#endif
	cli();
	if(!tty_hung_up_p(filp))
		info->count--;
	sti();
	info->blocked_open++;
	while (1) {
		cli();
		if (!(info->flags & ZILOG_CALLOUT_ACTIVE))
			zs_rtsdtr(info, 1);
		sti();
		set_current_state(TASK_INTERRUPTIBLE);
		if (tty_hung_up_p(filp) ||
		    !(info->flags & ZILOG_INITIALIZED)) {
#ifdef SERIAL_DEBUG_OPEN
			printk("block_til_ready hup-ed: ttys%d, count = %d\n",
				info->line, info->count);
#endif
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

		cli();
		r0 = read_zsreg(info->zs_channel, R0);
		sti();
		if (!(info->flags & ZILOG_CALLOUT_ACTIVE) &&
		    !(info->flags & ZILOG_CLOSING) &&
		    (do_clocal || (DCD & r0)))
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
int zs_open(struct tty_struct *tty, struct file * filp)
{
	struct sun_serial *info;
	int retval, line;

	line = MINOR(tty->device) - tty->driver.minor_start;

	/* The zilog lines for the mouse/keyboard must be
	 * opened using their respective drivers.
	 */
	if ((line < 0) || (line >= NUM_CHANNELS))
		return -ENODEV;
	if((line == KEYBOARD_LINE) || (line == MOUSE_LINE))
		return -ENODEV;
	info = zs_soft + line;
	/* Is the kgdb running over this line? */
	if (info->kgdb_channel)
		return -ENODEV;
	if (serial_paranoia_check(info, tty->device, "zs_open"))
		return -ENODEV;
#ifdef SERIAL_DEBUG_OPEN
	printk("zs_open %s%d, count = %d\n", tty->driver.name, info->line,
	       info->count);
#endif
	if (info->tty != 0 && info->tty != tty) {
		/* Never happen? */
		printk("zs_open %s%d, tty overwrite.\n", tty->driver.name, info->line);
		return -EBUSY;
	}

	if (!tmp_buf) {
		unsigned long page = get_free_page(GFP_KERNEL);
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
	 * Start up serial port
	 */
	retval = startup(info);
	if (retval)
		return retval;

	retval = block_til_ready(tty, filp, info);
	if (retval) {
#ifdef SERIAL_DEBUG_OPEN
		printk("zs_open returning after block_til_ready with %d\n",
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

#ifdef CONFIG_SERIAL_CONSOLE
	if (zs_console.cflag && zs_console.index == line) {
		tty->termios->c_cflag = zs_console.cflag;
		zs_console.cflag = 0;
		change_speed(info);
	}
#endif

	info->session = current->session;
	info->pgrp = current->pgrp;

#ifdef SERIAL_DEBUG_OPEN
	printk("zs_open ttys%d successful...", info->line);
#endif
	return 0;
}

/* Finally, routines used to initialize the serial driver. */

static void show_serial_version(void)
{
	char *revision = "$Revision: 1.68.2.2 $";
	char *version, *p;

	version = strchr(revision, ' ');
	p = strchr(++version, ' ');
	*p = '\0';
	printk("Sparc Zilog8530 serial driver version %s\n", version);
	*p = ' ';
}

/* Probe the PROM for the request zs chip number.
 *
 * Note: The Sun Voyager shows two addresses and two intr for it's
 *       Zilogs, what the second does, I don't know. It does work
 *       with using only the first number of each property.  Also
 *       we have a special version for sun4u.
 */
#ifdef __sparc_v9__
static struct sun_zslayout * __init get_zs(int chip)
{
	unsigned int vaddr[2] = { 0, 0 };
	unsigned long mapped_addr = 0;
	int busnode, seen, zsnode, sun4u_ino;
	static int irq = 0;

	if(chip < 0 || chip >= NUM_SERIAL) {
		prom_printf("get_zs bogon zs chip number");
		prom_halt();
	}

	if(central_bus)
		busnode = central_bus->child->prom_node;
	else
		busnode = prom_searchsiblings(prom_getchild(prom_root_node), "sbus");
	if(busnode == 0 || busnode == -1) {
		prom_printf("get_zs: no zs bus to search");
		prom_halt();
	}
	zsnode = prom_getchild(busnode);
	seen = 0;
	while(zsnode) {
		int slave;

		zsnode = prom_searchsiblings(zsnode, "zs");
		slave = prom_getintdefault(zsnode, "slave", -1);
		if((slave == chip) || (seen == chip)) {
			int len = prom_getproperty(zsnode, "address",
						   (void *) vaddr, sizeof(vaddr));

			if(len == -1 || central_bus != NULL) {
				struct sbus_bus *sbus = NULL;
				struct sbus_dev *sdev = NULL;

				/* "address" property is not guarenteed,
				 * everything in I/O is implicitly mapped
				 * anyways by our clever TLB miss handling
				 * scheme, so don't fail here.  -DaveM
				 */
				if (central_bus == NULL) {
					for_each_sbus(sbus) {
						for_each_sbusdev(sdev, sbus) {
							if (sdev->prom_node == zsnode)
								goto found;
						}
					}
				}
			found:
				if (sdev == NULL && central_bus == NULL)
					prom_halt();
				if (central_bus == NULL) {
					mapped_addr =
					    sbus_ioremap(&sdev->resource[0], 0,
							 PAGE_SIZE, "Zilog Registers");
				} else {
					struct linux_prom_registers zsregs[1];
					int err;

					err = prom_getproperty(zsnode, "reg",
							       (char *)&zsregs[0],
							       sizeof(zsregs));
					if (err == -1) {
						prom_printf("ZS: Cannot map Zilog regs.\n");
						prom_halt();
					}
					apply_fhc_ranges(central_bus->child, &zsregs[0], 1);
					apply_central_ranges(central_bus, &zsregs[0], 1);
					mapped_addr =
						((((u64)zsregs[0].which_io)<<32UL)|
						 ((u64)zsregs[0].phys_addr));
				}
			} else if(len % sizeof(unsigned int)) {
				prom_printf("WHOOPS:  proplen for %s "
					    "was %d, need multiple of "
					    "%d\n", "address", len,
					    sizeof(unsigned int));
				panic("zilog: address property");
			}
			zs_nodes[chip] = zsnode;
			len = prom_getproperty(zsnode, "interrupts",
					       (char *) &sun4u_ino,
					       (sizeof(sun4u_ino)));
			if(!irq) {
				if (central_bus) {
					unsigned long iclr, imap;

					iclr = central_bus->child->fhc_regs.uregs + FHC_UREGS_ICLR;
					imap = central_bus->child->fhc_regs.uregs + FHC_UREGS_IMAP;
					irq = zilog_irq = build_irq(12, 0, iclr, imap);
				} else {
					irq = zilog_irq = 
						sbus_build_irq(sbus_root, sun4u_ino);
				}
			}
			break;
		}
		zsnode = prom_getsibling(zsnode);
		seen++;
	}
	if(!zsnode)
		panic("get_zs: whee chip not found");
	if(!vaddr[0] && !mapped_addr)
		panic("get_zs: whee no serial chip mappable");
	if (mapped_addr != 0) {
		return (struct sun_zslayout *) mapped_addr;
	} else {
		return (struct sun_zslayout *) prom_virt_to_phys((unsigned long)vaddr[0], 0);
	}
}
#else /* !(__sparc_v9__) */
static struct sun_zslayout * __init get_zs(int chip)
{
	struct linux_prom_irqs tmp_irq[2];
	unsigned int paddr = 0;
	unsigned int vaddr[2] = { 0, 0 };
	int zsnode, tmpnode, iospace, slave, len;
	int cpunode = 0, bbnode = 0;
	static int irq = 0;
	int chipid = chip;

	iospace = 0;
	if(chip < 0 || chip >= NUM_SERIAL)
		panic("get_zs bogon zs chip number");

	if(sparc_cpu_model == sun4) {
		struct resource dummy_resource;

		/* Grrr, these have to be hardcoded aieee */
		switch(chip) {
		case 0:
			paddr = 0xf1000000;
			break;
		case 1:
			paddr = 0xf0000000;
			break;
		};
		iospace = 0;
		zs_nodes[chip] = 0;
		if(!irq)
			zilog_irq = irq = 12;
		dummy_resource.start = paddr;
		dummy_resource.end = paddr + 8 - 1;
		dummy_resource.flags = IORESOURCE_IO;
		vaddr[0] = sbus_ioremap(&dummy_resource, 0,
					8, "Zilog Serial");
	} else {
		/* Can use the prom for other machine types */
		zsnode = prom_getchild(prom_root_node);
		if (sparc_cpu_model == sun4d) {
			int no = 0;

			tmpnode = zsnode;
			zsnode = 0;
			bbnode = 0;
			while (tmpnode && (tmpnode = prom_searchsiblings(tmpnode, "cpu-unit"))) {
				bbnode = prom_getchild(tmpnode);
				if (bbnode && (bbnode = prom_searchsiblings(bbnode, "bootbus"))) {
					if (no == (chip >> 1)) {
						cpunode = tmpnode;
						zsnode = prom_getchild(bbnode);
						chipid = (chip & 1);
						break;
					}
					no++;
				}
				tmpnode = prom_getsibling(tmpnode);
			}
			if (!tmpnode)
				panic ("get_zs: couldn't find %dth bootbus\n", chip >> 1);
		} else {
			tmpnode = prom_searchsiblings(zsnode, "obio");
			if(tmpnode)
				zsnode = prom_getchild(tmpnode);
		}
		if(!zsnode)
			panic("get_zs no zs serial prom node");
		while(zsnode) {
			zsnode = prom_searchsiblings(zsnode, "zs");
			slave = prom_getintdefault(zsnode, "slave", -1);
			if(slave == chipid) {
				/* The one we want */
				if (sparc_cpu_model != sun4d) {
					len = prom_getproperty(zsnode, "address",
							       (void *) vaddr,
							       sizeof(vaddr));
        				if (len % sizeof(unsigned int)) {
						prom_printf("WHOOPS:  proplen for %s "
							"was %d, need multiple of "
							"%d\n", "address", len,
							sizeof(unsigned int));
						panic("zilog: address property");
					}
				} else {
					/* On sun4d don't have address property :( */
					struct linux_prom_registers zsreg[4];
					struct resource res;
					
					if (prom_getproperty(zsnode, "reg", (char *)zsreg, sizeof(zsreg)) == -1) {
						prom_printf ("Cannot map zs regs\n");
						prom_halt();
					}
					prom_apply_generic_ranges(bbnode, cpunode, zsreg, 1);
					res.start = zsreg[0].phys_addr;
					res.end = res.start + 8 - 1;
					res.flags = zsreg[0].which_io | IORESOURCE_IO;
					vaddr[0] = sbus_ioremap(&res, 0,
								8, "Zilog Serial");
				}
				zs_nodes[chip] = zsnode;
				len = prom_getproperty(zsnode, "intr",
						       (char *) tmp_irq,
						       sizeof(tmp_irq));
				if (len % sizeof(struct linux_prom_irqs)) {
					prom_printf(
					      "WHOOPS:  proplen for %s "
					      "was %d, need multiple of "
					      "%d\n", "intr", len,
					      sizeof(struct linux_prom_irqs));
					panic("zilog: intr property");
				}
				if(!irq) {
					irq = zilog_irq = tmp_irq[0].pri;
				} else {
					if(tmp_irq[0].pri != irq)
						panic("zilog: bogon irqs");
				}
				break;
			}
			zsnode = prom_getsibling(zsnode);
		}
		if(!zsnode)
			panic("get_zs whee chip not found");
	}
	if(!vaddr[0])
		panic("get_zs whee no serial chip mappable");

	return (struct sun_zslayout *)(unsigned long) vaddr[0];
}
#endif
/* This is for the auto baud rate detection in the mouse driver. */
void zs_change_mouse_baud(int newbaud)
{
	int channel = MOUSE_LINE;
	int brg;

	zs_soft[channel].zs_baud = newbaud;
	brg = BPS_TO_BRG(zs_soft[channel].zs_baud,
			 (ZS_CLOCK / zs_soft[channel].clk_divisor));
	write_zsreg(zs_soft[channel].zs_channel, R12, (brg & 0xff));
	write_zsreg(zs_soft[channel].zs_channel, R13, ((brg >> 8) & 0xff));
}

void __init zs_init_alloc_failure(const char *table_name)
{
	prom_printf("zs_probe: Cannot alloc %s.\n", table_name);
	prom_halt();
}

void * __init zs_alloc_bootmem(unsigned long size)
{
	void *ret;

	ret = __alloc_bootmem(size, SMP_CACHE_BYTES, 0UL);
	if (ret != NULL)
		memset(ret, 0, size);

	return ret;
}

void __init zs_alloc_tables(void)
{
	zs_chips = (struct sun_zslayout **)
		zs_alloc_bootmem(NUM_SERIAL * sizeof(struct sun_zslayout *));
	if (zs_chips == NULL)
		zs_init_alloc_failure("zs_chips");
	zs_channels = (struct sun_zschannel **)
		zs_alloc_bootmem(NUM_CHANNELS * sizeof(struct sun_zschannel *));
	if (zs_channels == NULL)
		zs_init_alloc_failure("zs_channels");
	zs_nodes = (int *)
		zs_alloc_bootmem(NUM_SERIAL * sizeof(int));
	if (zs_nodes == NULL)
		zs_init_alloc_failure("zs_nodes");
	zs_soft = (struct sun_serial *)
		zs_alloc_bootmem(NUM_CHANNELS * sizeof(struct sun_serial));
	if (zs_soft == NULL)
		zs_init_alloc_failure("zs_soft");
	zs_ttys = (struct tty_struct *)
		zs_alloc_bootmem(NUM_CHANNELS * sizeof(struct tty_struct));
	if (zs_ttys == NULL)
		zs_init_alloc_failure("zs_ttys");
	serial_table = (struct tty_struct **)
		zs_alloc_bootmem(NUM_CHANNELS * sizeof(struct tty_struct *));
	if (serial_table == NULL)
		zs_init_alloc_failure("serial_table");
	serial_termios = (struct termios **)
		zs_alloc_bootmem(NUM_CHANNELS * sizeof(struct termios *));
	if (serial_termios == NULL)
		zs_init_alloc_failure("serial_termios");
	serial_termios_locked = (struct termios **)
		zs_alloc_bootmem(NUM_CHANNELS * sizeof(struct termios *));
	if (serial_termios_locked == NULL)
		zs_init_alloc_failure("serial_termios_locked");
}

int __init zs_probe(void)
{
	int node;

	if(sparc_cpu_model == sun4)
		goto no_probe;

	NUM_SERIAL = 0;
	
	node = prom_getchild(prom_root_node);
	if (sparc_cpu_model == sun4d) {
		int bbnode;
		
		while (node && (node = prom_searchsiblings(node, "cpu-unit"))) {
			bbnode = prom_getchild(node);
			if (bbnode && prom_searchsiblings(bbnode, "bootbus"))
				NUM_SERIAL += 2;
			node = prom_getsibling(node);
		}
		goto no_probe;
	}
#ifdef __sparc_v9__
	else if (sparc_cpu_model == sun4u) {
		int central_node;

		/* Central bus zilogs must be checked for first,
		 * since Enterprise boxes might have SBUSes as well.
		 */
		central_node = prom_finddevice("/central");
		if(central_node != 0 && central_node != -1)
			node = prom_searchsiblings(prom_getchild(central_node), "fhc");
		else
			node = prom_searchsiblings(node, "sbus");
		if(node != 0 && node != -1)
			node = prom_getchild(node);
		if(node == 0 || node == -1)
			return -ENODEV;
	}
#endif /* __sparc_v9__ */
	else {
		node = prom_searchsiblings(node, "obio");
		if(node)
			node = prom_getchild(node);
		NUM_SERIAL = 2;
		goto no_probe;
	}

	node = prom_searchsiblings(node, "zs");
	if (!node)
		return -ENODEV;
		
	NUM_SERIAL = 2;

no_probe:
	zs_alloc_tables();

	/* Fill in rs_ops struct... */
#ifdef CONFIG_SERIAL_CONSOLE
	sunserial_setinitfunc(zs_console_init);
#endif
	sunserial_setinitfunc(zs_init);
	rs_ops.rs_kgdb_hook = zs_kgdb_hook;
	rs_ops.rs_change_mouse_baud = zs_change_mouse_baud;

	sunkbd_setinitfunc(sun_kbd_init);
	kbd_ops.compute_shiftstate = sun_compute_shiftstate;
	kbd_ops.setledstate = sun_setledstate;
	kbd_ops.getledstate = sun_getledstate;
	kbd_ops.setkeycode = sun_setkeycode;
	kbd_ops.getkeycode = sun_getkeycode;
#if defined(__sparc_v9__) && defined(CONFIG_PCI)
	sunkbd_install_keymaps(sun_key_maps, sun_keymap_count,
			       sun_func_buf, sun_func_table,
			       sun_funcbufsize, sun_funcbufleft,
			       sun_accent_table, sun_accent_table_size);
#endif
	return 0;
}

static inline void zs_prepare(void)
{
	int channel, chip;
	unsigned long flags;

	if (!NUM_SERIAL)
		return;
	
	save_and_cli(flags);
	
	/* Set up our interrupt linked list */
	zs_chain = &zs_soft[0];
	for(channel = 0; channel < NUM_CHANNELS - 1; channel++) {
		zs_soft[channel].zs_next = &zs_soft[channel + 1];
		zs_soft[channel].line = channel;
	}
	zs_soft[channel].zs_next = 0;

	/* Initialize Softinfo */
	for(chip = 0; chip < NUM_SERIAL; chip++) {
		/* If we are doing kgdb over one of the channels on
		 * chip zero, kgdb_channel will be set to 1 by the
		 * zs_kgdb_hook() routine below.
		 */
		if(!zs_chips[chip]) {
			zs_chips[chip] = get_zs(chip);
			/* Two channels per chip */
			zs_channels[(chip*2)] = &zs_chips[chip]->channelA;
			zs_channels[(chip*2)+1] = &zs_chips[chip]->channelB;
			zs_soft[(chip*2)].kgdb_channel = 0;
			zs_soft[(chip*2)+1].kgdb_channel = 0;
		}

		/* First, set up channel A on this chip. */
		channel = chip * 2;
		zs_soft[channel].zs_channel = zs_channels[channel];
		zs_soft[channel].change_needed = 0;
		zs_soft[channel].clk_divisor = 16;
		zs_soft[channel].cons_keyb = 0;
		zs_soft[channel].cons_mouse = 0;
		zs_soft[channel].channelA = 1;

		/* Now, channel B */
		channel++;
		zs_soft[channel].zs_channel = zs_channels[channel];
		zs_soft[channel].change_needed = 0;
		zs_soft[channel].clk_divisor = 16;
		zs_soft[channel].cons_keyb = 0;
		zs_soft[channel].cons_mouse = 0;
		zs_soft[channel].channelA = 0;
	}
	
	restore_flags(flags);
}

int __init zs_init(void)
{
	int channel, brg, i;
	unsigned long flags;
	struct sun_serial *info;
	char dummy;

	/* Setup base handler, and timer table. */
	init_bh(SERIAL_BH, do_serial_bh);

	show_serial_version();

	/* Initialize the tty_driver structure */
	/* SPARC: Not all of this is exactly right for us. */
	
	memset(&serial_driver, 0, sizeof(struct tty_driver));
	serial_driver.magic = TTY_DRIVER_MAGIC;
	serial_driver.driver_name = "serial";
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

	serial_driver.open = zs_open;
	serial_driver.close = zs_close;
	serial_driver.write = zs_write;
	serial_driver.flush_chars = zs_flush_chars;
	serial_driver.write_room = zs_write_room;
	serial_driver.chars_in_buffer = zs_chars_in_buffer;
	serial_driver.flush_buffer = zs_flush_buffer;
	serial_driver.ioctl = zs_ioctl;
	serial_driver.throttle = zs_throttle;
	serial_driver.unthrottle = zs_unthrottle;
	serial_driver.set_termios = zs_set_termios;
	serial_driver.stop = zs_stop;
	serial_driver.start = zs_start;
	serial_driver.hangup = zs_hangup;

	/* I'm too lazy, someone write versions of this for us. -DaveM */
	/* I just did. :-) -AIB 2001-12-23 */
	serial_driver.read_proc = zs_read_proc;

	/*
	 * The callout device is just like normal device except for
	 * major number and the subtype code.
	 */
	callout_driver = serial_driver;
	callout_driver.name = "cua/%d";
	callout_driver.major = TTYAUX_MAJOR;
	callout_driver.subtype = SERIAL_TYPE_CALLOUT;
	callout_driver.read_proc = 0;
	callout_driver.proc_entry = 0;

	if (tty_register_driver(&serial_driver))
		panic("Couldn't register serial driver\n");
	if (tty_register_driver(&callout_driver))
		panic("Couldn't register callout driver\n");

	save_flags(flags); cli();

	/* Initialize Softinfo */
	zs_prepare();

	/* Grab IRQ line before poking the chips so we do
	 * not lose any interrupts.
	 */
	if (request_irq(zilog_irq, zs_interrupt, SA_SHIRQ,
			"Zilog8530", zs_chain)) {
		prom_printf("Unable to attach zs intr\n");
		prom_halt();
	}

	/* Initialize Hardware */
	for(channel = 0; channel < NUM_CHANNELS; channel++) {
		/* Hardware reset each chip */
		if (!(channel & 1)) {
			write_zsreg(zs_soft[channel].zs_channel, R9, FHWRES);
			ZSDELAY_LONG();
			dummy = read_zsreg(zs_soft[channel].zs_channel, R0);
		}

		if(channel == KEYBOARD_LINE) {
			zs_soft[channel].cons_keyb = 1;
			zs_soft[channel].parity_mask = 0xff;
			zs_kbdchan = zs_soft[channel].zs_channel;

			write_zsreg(zs_soft[channel].zs_channel, R4,
				    (PAR_EVEN | X16CLK | SB1));
			write_zsreg(zs_soft[channel].zs_channel, R3, Rx8);
			write_zsreg(zs_soft[channel].zs_channel, R5, Tx8);
			write_zsreg(zs_soft[channel].zs_channel, R9, NV);
			write_zsreg(zs_soft[channel].zs_channel, R10, NRZ);
			write_zsreg(zs_soft[channel].zs_channel, R11,
				    (TCBR | RCBR));
			zs_soft[channel].zs_baud = 1200;
			brg = BPS_TO_BRG(zs_soft[channel].zs_baud,
					 ZS_CLOCK/zs_soft[channel].clk_divisor);
			write_zsreg(zs_soft[channel].zs_channel, R12,
				    (brg & 0xff));
			write_zsreg(zs_soft[channel].zs_channel, R13,
				    ((brg >> 8) & 0xff));
			write_zsreg(zs_soft[channel].zs_channel, R14, BRSRC);

			/* Enable Rx/Tx, IRQs, and inform kbd driver */
			write_zsreg(zs_soft[channel].zs_channel, R14,
				    (BRSRC | BRENAB));
			write_zsreg(zs_soft[channel].zs_channel, R3,
				    (Rx8 | RxENAB));
			write_zsreg(zs_soft[channel].zs_channel, R5,
				    (Tx8 | TxENAB | DTR | RTS));

			write_zsreg(zs_soft[channel].zs_channel, R15,
				    (DCDIE | CTSIE | TxUIE | BRKIE));
			write_zsreg(zs_soft[channel].zs_channel, R0,
				    RES_EXT_INT);
			write_zsreg(zs_soft[channel].zs_channel, R0,
				    RES_EXT_INT);

			write_zsreg(zs_soft[channel].zs_channel, R1,
				    (EXT_INT_ENAB | INT_ALL_Rx));
			write_zsreg(zs_soft[channel].zs_channel, R9,
				    (NV | MIE));
			ZS_CLEARERR(zs_soft[channel].zs_channel);
			ZS_CLEARFIFO(zs_soft[channel].zs_channel);
		} else if(channel == MOUSE_LINE) {
			zs_soft[channel].cons_mouse = 1;
			zs_soft[channel].parity_mask = 0xff;
			zs_mousechan = zs_soft[channel].zs_channel;

			write_zsreg(zs_soft[channel].zs_channel, R4,
				    (PAR_EVEN | X16CLK | SB1));
			write_zsreg(zs_soft[channel].zs_channel, R3, Rx8);
			write_zsreg(zs_soft[channel].zs_channel, R5, Tx8);
			write_zsreg(zs_soft[channel].zs_channel, R9, NV);
			write_zsreg(zs_soft[channel].zs_channel, R10, NRZ);
			write_zsreg(zs_soft[channel].zs_channel, R11,
				    (TCBR | RCBR));

			zs_soft[channel].zs_baud = 4800;
			brg = BPS_TO_BRG(zs_soft[channel].zs_baud,
					 ZS_CLOCK/zs_soft[channel].clk_divisor);
			write_zsreg(zs_soft[channel].zs_channel, R12,
				    (brg & 0xff));
			write_zsreg(zs_soft[channel].zs_channel, R13,
				    ((brg >> 8) & 0xff));
			write_zsreg(zs_soft[channel].zs_channel, R14, BRSRC);

			/* Enable Rx, IRQs, and inform mouse driver */
			write_zsreg(zs_soft[channel].zs_channel, R14,
				    (BRSRC | BRENAB));
			write_zsreg(zs_soft[channel].zs_channel, R3,
				    (Rx8 | RxENAB));
			write_zsreg(zs_soft[channel].zs_channel, R5, Tx8);

			write_zsreg(zs_soft[channel].zs_channel, R15,
				    (DCDIE | CTSIE | TxUIE | BRKIE));
			write_zsreg(zs_soft[channel].zs_channel, R0,
				    RES_EXT_INT);
			write_zsreg(zs_soft[channel].zs_channel, R0,
				    RES_EXT_INT);

			write_zsreg(zs_soft[channel].zs_channel, R1,
				    (EXT_INT_ENAB | INT_ALL_Rx));
			write_zsreg(zs_soft[channel].zs_channel, R9,
				    (NV | MIE));

			sun_mouse_zsinit();
		} else if (zs_soft[channel].is_cons) {
			brg = BPS_TO_BRG(zs_soft[channel].zs_baud,
					 ZS_CLOCK/zs_soft[channel].clk_divisor);
			zscons_regs[12] = brg & 0xff;
			zscons_regs[13] = (brg >> 8) & 0xff;

			memcpy(zs_soft[channel].curregs, zscons_regs, sizeof(zscons_regs));
			load_zsregs(&zs_soft[channel], zscons_regs);

			ZS_CLEARERR(zs_soft[channel].zs_channel);
			ZS_CLEARFIFO(zs_soft[channel].zs_channel);
		} else if (zs_soft[channel].kgdb_channel) {
			/* If this is the kgdb line, enable interrupts because
			 * we now want to receive the 'control-c' character
			 * from the client attached to us asynchronously.
			 */
			zs_soft[channel].parity_mask = 0xff;
        		kgdb_chaninit(&zs_soft[channel], 1,
				      zs_soft[channel].zs_baud);
		} else {
			zs_soft[channel].parity_mask = 0xff;
			write_zsreg(zs_soft[channel].zs_channel, R4,
				    (PAR_EVEN | X16CLK | SB1));
			write_zsreg(zs_soft[channel].zs_channel, R3, Rx8);
			write_zsreg(zs_soft[channel].zs_channel, R5, Tx8);
			write_zsreg(zs_soft[channel].zs_channel, R9, NV);
			write_zsreg(zs_soft[channel].zs_channel, R10, NRZ);
			write_zsreg(zs_soft[channel].zs_channel, R11,
				    (RCBR | TCBR));
			zs_soft[channel].zs_baud = 9600;
			brg = BPS_TO_BRG(zs_soft[channel].zs_baud,
					 ZS_CLOCK/zs_soft[channel].clk_divisor);
			write_zsreg(zs_soft[channel].zs_channel, R12,
				    (brg & 0xff));
			write_zsreg(zs_soft[channel].zs_channel, R13,
				    ((brg >> 8) & 0xff));
			write_zsreg(zs_soft[channel].zs_channel, R14, BRSRC);
			write_zsreg(zs_soft[channel].zs_channel, R14,
				    (BRSRC | BRENAB));
			write_zsreg(zs_soft[channel].zs_channel, R3, Rx8);
			write_zsreg(zs_soft[channel].zs_channel, R5, Tx8);
			write_zsreg(zs_soft[channel].zs_channel, R15, DCDIE);
			write_zsreg(zs_soft[channel].zs_channel, R9, NV | MIE);
			write_zsreg(zs_soft[channel].zs_channel, R0,
				    RES_EXT_INT);
			write_zsreg(zs_soft[channel].zs_channel, R0,
				    RES_EXT_INT);
		}
	}

	for (info = zs_chain, i=0; info; info = info->zs_next, i++) {
		info->magic = SERIAL_MAGIC;
		info->port = (long) info->zs_channel;
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
		info->callout_termios = callout_driver.init_termios;
		info->normal_termios = serial_driver.init_termios;
		init_waitqueue_head(&info->open_wait);
		init_waitqueue_head(&info->close_wait);
		printk("tty%02d at 0x%04x (irq = %s)", info->line, 
		       info->port, __irq_itoa(info->irq));
		printk(" is a Zilog8530\n");
	}

	restore_flags(flags);

	keyboard_zsinit(kbd_put_char);
	return 0;
}

/* This is called at boot time to prime the kgdb serial debugging
 * serial line.  The 'tty_num' argument is 0 for /dev/ttya and 1
 * for /dev/ttyb which is determined in setup_arch() from the
 * boot command line flags.
 */
static void __init zs_kgdb_hook(int tty_num)
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
	zs_soft[tty_num].zs_baud = 9600;
	zs_soft[tty_num].kgdb_channel = 1;     /* This runs kgdb */
	zs_soft[tty_num ^ 1].kgdb_channel = 0; /* This does not */
	/* Turn on transmitter/receiver at 8-bits/char */
        kgdb_chaninit(&zs_soft[tty_num], 0, 9600);
        ZS_CLEARERR(zs_kgdbchan);
        ZS_CLEARFIFO(zs_kgdbchan);
}

#ifdef CONFIG_SERIAL_CONSOLE

/* This is for console output over ttya/ttyb */
static void
zs_console_putchar(struct sun_serial *info, char ch)
{
	int loops = ZS_PUT_CHAR_MAX_DELAY;
	unsigned long flags;

	if(!info->zs_channel)
		return;

	save_flags(flags); cli();
	zs_put_char(info->zs_channel, ch);
	while (!(read_zsreg(info->zs_channel, R1) & ALL_SNT) && --loops)
		udelay(5);
	restore_flags(flags);
}

#ifdef SERIAL_CONSOLE_FAIR_OUTPUT
/*
 * Fair output driver allows a process to speak.
 */
static void zs_fair_output(struct sun_serial *info)
{
	unsigned long flags;
	int left;		/* Output no more than that */
	char c;

	if (info == NULL)
		return;
	if (info->xmit_buf == NULL)
		return;

	save_flags(flags);  cli();
	left = info->xmit_cnt;
	while (left != 0) {
		c = info->xmit_buf[info->xmit_tail];
		info->xmit_tail = (info->xmit_tail+1) & (SERIAL_XMIT_SIZE-1);
		info->xmit_cnt--;
		restore_flags(flags);

		zs_console_putchar(info, c);

		cli();
		left = MIN(info->xmit_cnt, left-1);
	}

	/* Last character is being transmitted now (hopefully). */
	sbus_writeb(RES_Tx_P, &info->zs_channel->control);
	ZSDELAY();
	ZSLOG(REGCTRL, RES_Tx_P, 1);

	restore_flags(flags);
	return;
}
#endif

/*
 * zs_console_write is registered for printk.
 */
static void
zs_console_write(struct console *con, const char *s, unsigned count)
{
	struct sun_serial *info;
	int i;

	info = zs_soft + con->index;

	for (i = 0; i < count; i++, s++) {
		if(*s == '\n')
			zs_console_putchar(info, '\r');
		zs_console_putchar(info, *s);
	}
#ifdef SERIAL_CONSOLE_FAIR_OUTPUT
	/* Comment this if you want to have a strict interrupt-driven output */
	zs_fair_output(info);
#endif
}

static kdev_t zs_console_device(struct console *con)
{
	return MKDEV(TTY_MAJOR, 64 + con->index);
}

static int __init zs_console_setup(struct console *con, char *options)
{
	static struct tty_struct c_tty;
	static struct termios c_termios;
	struct sun_serial *info;
	int brg, baud;

	info = zs_soft + con->index;
	info->is_cons = 1;

	printk("Console: ttyS%d (Zilog8530)\n", info->line);
	
	sunserial_console_termios(con);
	memset(&c_tty, 0, sizeof(c_tty));
	memset(&c_termios, 0, sizeof(c_termios));
	c_tty.termios = &c_termios;
	c_termios.c_cflag = con->cflag;
	baud = tty_get_baud_rate(&c_tty);

	info->zs_baud = baud;

	switch (con->cflag & CSIZE) {
		case CS5:
			zscons_regs[3] = Rx5 | RxENAB;
			zscons_regs[5] = Tx5 | TxENAB;
			info->parity_mask = 0x1f;
			break;
		case CS6:
			zscons_regs[3] = Rx6 | RxENAB;
			zscons_regs[5] = Tx6 | TxENAB;
			info->parity_mask = 0x3f;
			break;
		case CS7:
			zscons_regs[3] = Rx7 | RxENAB;
			zscons_regs[5] = Tx7 | TxENAB;
			info->parity_mask = 0x7f;
			break;
		default:
		case CS8:
			zscons_regs[3] = Rx8 | RxENAB;
			zscons_regs[5] = Tx8 | TxENAB;
			info->parity_mask = 0xff;
			break;
	}
	zscons_regs[5] |= DTR;

	if (con->cflag & PARENB)
		zscons_regs[4] |= PAR_ENAB;
	if (!(con->cflag & PARODD))
		zscons_regs[4] |= PAR_EVEN;

	if (con->cflag & CSTOPB)
		zscons_regs[4] |= SB2;
	else
		zscons_regs[4] |= SB1;

	brg = BPS_TO_BRG(baud, ZS_CLOCK / info->clk_divisor);
	zscons_regs[12] = brg & 0xff;
	zscons_regs[13] = (brg >> 8) & 0xff;

	memcpy(info->curregs, zscons_regs, sizeof(zscons_regs));
	load_zsregs(info, zscons_regs);

	ZS_CLEARERR(info->zs_channel);
	ZS_CLEARFIFO(info->zs_channel);
	return 0;
}

static struct console zs_console = {
	name:		"ttyS",
	write:		zs_console_write,
	device:		zs_console_device,
	setup:		zs_console_setup,
	flags:		CON_PRINTBUFFER,
	index:		-1,
};

static int __init zs_console_init(void)
{
	extern int con_is_present(void);

	if (con_is_present())
		return 0;

	zs_console.index = serial_console - 1;
	register_console(&zs_console);
	return 0;
}

#endif /* CONFIG_SERIAL_CONSOLE */
