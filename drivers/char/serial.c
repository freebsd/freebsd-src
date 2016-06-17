/*
 *  linux/drivers/char/serial.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *  Copyright (C) 1992, 1993, 1994, 1995, 1996, 1997, 
 * 		1998, 1999  Theodore Ts'o
 *
 *  Extensively rewritten by Theodore Ts'o, 8/16/92 -- 9/14/92.  Now
 *  much more extensible to support other serial cards based on the
 *  16450/16550A UART's.  Added support for the AST FourPort and the
 *  Accent Async board.  
 *
 *  set_serial_info fixed to set the flags, custom divisor, and uart
 * 	type fields.  Fix suggested by Michael K. Johnson 12/12/92.
 *
 *  11/95: TIOCMIWAIT, TIOCGICOUNT by Angelo Haritsis <ah@doc.ic.ac.uk>
 *
 *  03/96: Modularised by Angelo Haritsis <ah@doc.ic.ac.uk>
 *
 *  rs_set_termios fixed to look also for changes of the input
 *      flags INPCK, BRKINT, PARMRK, IGNPAR and IGNBRK.
 *                                            Bernd Anhäupl 05/17/96.
 *
 *  1/97:  Extended dumb serial ports are a config option now.  
 *         Saves 4k.   Michael A. Griffith <grif@acm.org>
 * 
 *  8/97: Fix bug in rs_set_termios with RTS
 *        Stanislav V. Voronyi <stas@uanet.kharkov.ua>
 *
 *  3/98: Change the IRQ detection, use of probe_irq_o*(),
 *	  suppress TIOCSERGWILD and TIOCSERSWILD
 *	  Etienne Lorrain <etienne.lorrain@ibm.net>
 *
 *  4/98: Added changes to support the ARM architecture proposed by
 * 	  Russell King
 *
 *  5/99: Updated to include support for the XR16C850 and ST16C654
 *        uarts.  Stuart MacDonald <stuartm@connecttech.com>
 *
 *  8/99: Generalized PCI support added.  Theodore Ts'o
 * 
 *  3/00: Rid circular buffer of redundant xmit_cnt.  Fix a
 *	  few races on freeing buffers too.
 *	  Alan Modra <alan@linuxcare.com>
 *
 *  5/00: Support for the RSA-DV II/S card added.
 *	  Kiyokazu SUTO <suto@ks-and-ks.ne.jp>
 * 
 *  6/00: Remove old-style timer, use timer_list
 *        Andrew Morton <andrewm@uow.edu.au>
 *
 *  7/00: Support Timedia/Sunix/Exsys PCI cards
 *
 *  7/00: fix some returns on failure not using MOD_DEC_USE_COUNT.
 *	  Arnaldo Carvalho de Melo <acme@conectiva.com.br>
 *
 * 10/00: add in optional software flow control for serial console.
 *	  Kanoj Sarcar <kanoj@sgi.com>  (Modified by Theodore Ts'o)
 *
 * 02/02: Fix for AMD Elan bug in transmit irq routine, by
 *        Christer Weinigel <wingel@hog.ctrl-c.liu.se>,
 *        Robert Schwebel <robert@schwebel.de>,
 *        Juergen Beisert <jbeisert@eurodsn.de>,
 *        Theodore Ts'o <tytso@mit.edu>
 */

static char *serial_version = "5.05c";
static char *serial_revdate = "2001-07-08";

/*
 * Serial driver configuration section.  Here are the various options:
 *
 * CONFIG_HUB6
 *		Enables support for the venerable Bell Technologies
 *		HUB6 card.
 *
 * CONFIG_SERIAL_MANY_PORTS
 * 		Enables support for ports beyond the standard, stupid
 * 		COM 1/2/3/4.
 *
 * CONFIG_SERIAL_MULTIPORT
 * 		Enables support for special multiport board support.
 *
 * CONFIG_SERIAL_SHARE_IRQ
 * 		Enables support for multiple serial ports on one IRQ
 *
 * CONFIG_SERIAL_DETECT_IRQ
 *		Enable the autodetection of IRQ on standart ports
 *
 * SERIAL_PARANOIA_CHECK
 * 		Check the magic number for the async_structure where
 * 		ever possible.
 *
 * CONFIG_SERIAL_ACPI
 *		Enable support for serial console port and serial 
 *		debug port as defined by the SPCR and DBGP tables in 
 *		ACPI 2.0.
 */

#include <linux/config.h>
#include <linux/version.h>

#undef SERIAL_PARANOIA_CHECK
#define CONFIG_SERIAL_NOPAUSE_IO
#define SERIAL_DO_RESTART

#if 0
/* These defines are normally controlled by the autoconf.h */
#define CONFIG_SERIAL_MANY_PORTS
#define CONFIG_SERIAL_SHARE_IRQ
#define CONFIG_SERIAL_DETECT_IRQ
#define CONFIG_SERIAL_MULTIPORT
#define CONFIG_HUB6
#endif

#ifdef CONFIG_PCI
#define ENABLE_SERIAL_PCI
#ifndef CONFIG_SERIAL_SHARE_IRQ
#define CONFIG_SERIAL_SHARE_IRQ
#endif
#ifndef CONFIG_SERIAL_MANY_PORTS
#define CONFIG_SERIAL_MANY_PORTS
#endif
#endif

#ifdef CONFIG_SERIAL_ACPI
#define ENABLE_SERIAL_ACPI
#endif

#if defined(CONFIG_ISAPNP)|| (defined(CONFIG_ISAPNP_MODULE) && defined(MODULE))
#ifndef ENABLE_SERIAL_PNP
#define ENABLE_SERIAL_PNP
#endif
#endif

/* Set of debugging defines */

#undef SERIAL_DEBUG_INTR
#undef SERIAL_DEBUG_OPEN
#undef SERIAL_DEBUG_FLOW
#undef SERIAL_DEBUG_RS_WAIT_UNTIL_SENT
#undef SERIAL_DEBUG_PCI
#undef SERIAL_DEBUG_AUTOCONF

/* Sanity checks */

#ifdef CONFIG_SERIAL_MULTIPORT
#ifndef CONFIG_SERIAL_SHARE_IRQ
#define CONFIG_SERIAL_SHARE_IRQ
#endif
#endif

#ifdef CONFIG_HUB6
#ifndef CONFIG_SERIAL_MANY_PORTS
#define CONFIG_SERIAL_MANY_PORTS
#endif
#ifndef CONFIG_SERIAL_SHARE_IRQ
#define CONFIG_SERIAL_SHARE_IRQ
#endif
#endif

#ifdef MODULE
#undef CONFIG_SERIAL_CONSOLE
#endif

#define CONFIG_SERIAL_RSA

#define RS_STROBE_TIME (10*HZ)
#define RS_ISR_PASS_LIMIT 256

#if defined(__i386__) && (defined(CONFIG_M386) || defined(CONFIG_M486))
#define SERIAL_INLINE
#endif
  
/*
 * End of serial driver configuration section.
 */

#include <linux/module.h>

#include <linux/types.h>
#ifdef LOCAL_HEADERS
#include "serial_local.h"
#else
#include <linux/serial.h>
#include <linux/serialP.h>
#include <linux/serial_reg.h>
#include <asm/serial.h>
#define LOCAL_VERSTRING ""
#endif

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
#if (LINUX_VERSION_CODE >= 131343)
#include <linux/init.h>
#endif
#if (LINUX_VERSION_CODE >= 131336)
#include <asm/uaccess.h>
#endif
#include <linux/delay.h>
#ifdef CONFIG_SERIAL_CONSOLE
#include <linux/console.h>
#endif
#ifdef ENABLE_SERIAL_PCI
#include <linux/pci.h>
#endif
#ifdef ENABLE_SERIAL_PNP
#include <linux/isapnp.h>
#endif
#ifdef CONFIG_MAGIC_SYSRQ
#include <linux/sysrq.h>
#endif

/*
 * All of the compatibilty code so we can compile serial.c against
 * older kernels is hidden in serial_compat.h
 */
#if defined(LOCAL_HEADERS) || (LINUX_VERSION_CODE < 0x020317) /* 2.3.23 */
#include "serial_compat.h"
#endif

#include <asm/system.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/bitops.h>

#if defined(CONFIG_MAC_SERIAL)
#define SERIAL_DEV_OFFSET	((_machine == _MACH_prep || _machine == _MACH_chrp) ? 0 : 2)
#else
#define SERIAL_DEV_OFFSET	0
#endif

#ifdef SERIAL_INLINE
#define _INLINE_ inline
#else
#define _INLINE_
#endif

static char *serial_name = "Serial driver";

static DECLARE_TASK_QUEUE(tq_serial);

static struct tty_driver serial_driver, callout_driver;
static int serial_refcount;

static struct timer_list serial_timer;

/* serial subtype definitions */
#ifndef SERIAL_TYPE_NORMAL
#define SERIAL_TYPE_NORMAL	1
#define SERIAL_TYPE_CALLOUT	2
#endif

/* number of characters left in xmit buffer before we ask for more */
#define WAKEUP_CHARS 256

/*
 * IRQ_timeout		- How long the timeout should be for each IRQ
 * 				should be after the IRQ has been active.
 */

static struct async_struct *IRQ_ports[NR_IRQS];
#ifdef CONFIG_SERIAL_MULTIPORT
static struct rs_multiport_struct rs_multiport[NR_IRQS];
#endif
static int IRQ_timeout[NR_IRQS];
#ifdef CONFIG_SERIAL_CONSOLE
static struct console sercons;
static int lsr_break_flag;
#endif
#if defined(CONFIG_SERIAL_CONSOLE) && defined(CONFIG_MAGIC_SYSRQ)
static unsigned long break_pressed; /* break, really ... */
#endif

static unsigned detect_uart_irq (struct serial_state * state);
static void autoconfig(struct serial_state * state);
static void change_speed(struct async_struct *info, struct termios *old);
static void rs_wait_until_sent(struct tty_struct *tty, int timeout);

/*
 * Here we define the default xmit fifo size used for each type of
 * UART
 */
static struct serial_uart_config uart_config[] = {
	{ "unknown", 1, 0 }, 
	{ "8250", 1, 0 }, 
	{ "16450", 1, 0 }, 
	{ "16550", 1, 0 }, 
	{ "16550A", 16, UART_CLEAR_FIFO | UART_USE_FIFO }, 
	{ "cirrus", 1, 0 }, 	/* usurped by cyclades.c */
	{ "ST16650", 1, UART_CLEAR_FIFO | UART_STARTECH }, 
	{ "ST16650V2", 32, UART_CLEAR_FIFO | UART_USE_FIFO |
		  UART_STARTECH }, 
	{ "TI16750", 64, UART_CLEAR_FIFO | UART_USE_FIFO},
	{ "Startech", 1, 0},	/* usurped by cyclades.c */
	{ "16C950/954", 128, UART_CLEAR_FIFO | UART_USE_FIFO},
	{ "ST16654", 64, UART_CLEAR_FIFO | UART_USE_FIFO |
		  UART_STARTECH }, 
	{ "XR16850", 128, UART_CLEAR_FIFO | UART_USE_FIFO |
		  UART_STARTECH },
	{ "RSA", 2048, UART_CLEAR_FIFO | UART_USE_FIFO }, 
	{ 0, 0}
};

#if defined(CONFIG_SERIAL_RSA) && defined(MODULE)

#define PORT_RSA_MAX 4
static int probe_rsa[PORT_RSA_MAX];
static int force_rsa[PORT_RSA_MAX];

MODULE_PARM(probe_rsa, "1-" __MODULE_STRING(PORT_RSA_MAX) "i");
MODULE_PARM_DESC(probe_rsa, "Probe I/O ports for RSA");
MODULE_PARM(force_rsa, "1-" __MODULE_STRING(PORT_RSA_MAX) "i");
MODULE_PARM_DESC(force_rsa, "Force I/O ports for RSA");
#endif /* CONFIG_SERIAL_RSA  */

struct serial_state rs_table[RS_TABLE_SIZE] = {
	SERIAL_PORT_DFNS	/* Defined in serial.h */
};

#define NR_PORTS	(sizeof(rs_table)/sizeof(struct serial_state))
int serial_nr_ports = NR_PORTS;

#if (defined(ENABLE_SERIAL_PCI) || defined(ENABLE_SERIAL_PNP))
#define NR_PCI_BOARDS	8

static struct pci_board_inst	serial_pci_board[NR_PCI_BOARDS];

#ifndef IS_PCI_REGION_IOPORT
#define IS_PCI_REGION_IOPORT(dev, r) (pci_resource_flags((dev), (r)) & \
				      IORESOURCE_IO)
#endif
#ifndef IS_PCI_REGION_IOMEM
#define IS_PCI_REGION_IOMEM(dev, r) (pci_resource_flags((dev), (r)) & \
				      IORESOURCE_MEM)
#endif
#ifndef PCI_IRQ_RESOURCE
#define PCI_IRQ_RESOURCE(dev, r) ((dev)->irq_resource[r].start)
#endif
#ifndef pci_get_subvendor
#define pci_get_subvendor(dev) ((dev)->subsystem_vendor)
#define pci_get_subdevice(dev)  ((dev)->subsystem_device)
#endif
#endif	/* ENABLE_SERIAL_PCI || ENABLE_SERIAL_PNP  */

#ifndef PREPARE_FUNC
#define PREPARE_FUNC(dev)  (dev->prepare)
#define ACTIVATE_FUNC(dev)  (dev->activate)
#define DEACTIVATE_FUNC(dev)  (dev->deactivate)
#endif

#define HIGH_BITS_OFFSET ((sizeof(long)-sizeof(int))*8)

static struct tty_struct *serial_table[NR_PORTS];
static struct termios *serial_termios[NR_PORTS];
static struct termios *serial_termios_locked[NR_PORTS];


#if defined(MODULE) && defined(SERIAL_DEBUG_MCOUNT)
#define DBG_CNT(s) printk("(%s): [%x] refc=%d, serc=%d, ttyc=%d -> %s\n", \
 kdevname(tty->device), (info->flags), serial_refcount,info->count,tty->count,s)
#else
#define DBG_CNT(s)
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
#ifdef DECLARE_MUTEX
static DECLARE_MUTEX(tmp_buf_sem);
#else
static struct semaphore tmp_buf_sem = MUTEX;
#endif


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

static _INLINE_ unsigned int serial_in(struct async_struct *info, int offset)
{
	switch (info->io_type) {
#ifdef CONFIG_HUB6
	case SERIAL_IO_HUB6:
		outb(info->hub6 - 1 + offset, info->port);
		return inb(info->port+1);
#endif
	case SERIAL_IO_MEM:
		return readb((unsigned long) info->iomem_base +
			     (offset<<info->iomem_reg_shift));
	default:
		return inb(info->port + offset);
	}
}

static _INLINE_ void serial_out(struct async_struct *info, int offset,
				int value)
{
	switch (info->io_type) {
#ifdef CONFIG_HUB6
	case SERIAL_IO_HUB6:
		outb(info->hub6 - 1 + offset, info->port);
		outb(value, info->port+1);
		break;
#endif
	case SERIAL_IO_MEM:
		writeb(value, (unsigned long) info->iomem_base +
			      (offset<<info->iomem_reg_shift));
		break;
	default:
		outb(value, info->port+offset);
	}
}

/*
 * We used to support using pause I/O for certain machines.  We
 * haven't supported this for a while, but just in case it's badly
 * needed for certain old 386 machines, I've left these #define's
 * in....
 */
#define serial_inp(info, offset)		serial_in(info, offset)
#define serial_outp(info, offset, value)	serial_out(info, offset, value)


/*
 * For the 16C950
 */
void serial_icr_write(struct async_struct *info, int offset, int  value)
{
	serial_out(info, UART_SCR, offset);
	serial_out(info, UART_ICR, value);
}

unsigned int serial_icr_read(struct async_struct *info, int offset)
{
	int	value;

	serial_icr_write(info, UART_ACR, info->ACR | UART_ACR_ICRRD);
	serial_out(info, UART_SCR, offset);
	value = serial_in(info, UART_ICR);
	serial_icr_write(info, UART_ACR, info->ACR);
	return value;
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
	if (info->IER & UART_IER_THRI) {
		info->IER &= ~UART_IER_THRI;
		serial_out(info, UART_IER, info->IER);
	}
	if (info->state->type == PORT_16C950) {
		info->ACR |= UART_ACR_TXDIS;
		serial_icr_write(info, UART_ACR, info->ACR);
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
	if (info->xmit.head != info->xmit.tail
	    && info->xmit.buf
	    && !(info->IER & UART_IER_THRI)) {
		info->IER |= UART_IER_THRI;
		serial_out(info, UART_IER, info->IER);
	}
	if (info->state->type == PORT_16C950) {
		info->ACR &= ~UART_ACR_TXDIS;
		serial_icr_write(info, UART_ACR, info->ACR);
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
				 int *status, struct pt_regs * regs)
{
	struct tty_struct *tty = info->tty;
	unsigned char ch;
	struct	async_icount *icount;
	int	max_count = 256;

	icount = &info->state->icount;
	do {
		if (tty->flip.count >= TTY_FLIPBUF_SIZE) {
			tty->flip.tqueue.routine((void *) tty);
			if (tty->flip.count >= TTY_FLIPBUF_SIZE)
				return;		// if TTY_DONT_FLIP is set
		}
		ch = serial_inp(info, UART_RX);
		*tty->flip.char_buf_ptr = ch;
		icount->rx++;
		
#ifdef SERIAL_DEBUG_INTR
		printk("DR%02x:%02x...", ch, *status);
#endif
		*tty->flip.flag_buf_ptr = 0;
		if (*status & (UART_LSR_BI | UART_LSR_PE |
			       UART_LSR_FE | UART_LSR_OE)) {
			/*
			 * For statistics only
			 */
			if (*status & UART_LSR_BI) {
				*status &= ~(UART_LSR_FE | UART_LSR_PE);
				icount->brk++;
				/*
				 * We do the SysRQ and SAK checking
				 * here because otherwise the break
				 * may get masked by ignore_status_mask
				 * or read_status_mask.
				 */
#if defined(CONFIG_SERIAL_CONSOLE) && defined(CONFIG_MAGIC_SYSRQ)
				if (info->line == sercons.index) {
					if (!break_pressed) {
						break_pressed = jiffies;
						goto ignore_char;
					}
					break_pressed = 0;
				}
#endif
				if (info->flags & ASYNC_SAK)
					do_SAK(tty);
			} else if (*status & UART_LSR_PE)
				icount->parity++;
			else if (*status & UART_LSR_FE)
				icount->frame++;
			if (*status & UART_LSR_OE)
				icount->overrun++;

			/*
			 * Mask off conditions which should be ignored.
			 */
			*status &= info->read_status_mask;

#ifdef CONFIG_SERIAL_CONSOLE
			if (info->line == sercons.index) {
				/* Recover the break flag from console xmit */
				*status |= lsr_break_flag;
				lsr_break_flag = 0;
			}
#endif
			if (*status & (UART_LSR_BI)) {
#ifdef SERIAL_DEBUG_INTR
				printk("handling break....");
#endif
				*tty->flip.flag_buf_ptr = TTY_BREAK;
			} else if (*status & UART_LSR_PE)
				*tty->flip.flag_buf_ptr = TTY_PARITY;
			else if (*status & UART_LSR_FE)
				*tty->flip.flag_buf_ptr = TTY_FRAME;
		}
#if defined(CONFIG_SERIAL_CONSOLE) && defined(CONFIG_MAGIC_SYSRQ)
		if (break_pressed && info->line == sercons.index) {
			if (ch != 0 &&
			    time_before(jiffies, break_pressed + HZ*5)) {
				handle_sysrq(ch, regs, NULL, NULL);
				break_pressed = 0;
				goto ignore_char;
			}
			break_pressed = 0;
		}
#endif
		if ((*status & info->ignore_status_mask) == 0) {
			tty->flip.flag_buf_ptr++;
			tty->flip.char_buf_ptr++;
			tty->flip.count++;
		}
		if ((*status & UART_LSR_OE) &&
		    (tty->flip.count < TTY_FLIPBUF_SIZE)) {
			/*
			 * Overrun is special, since it's reported
			 * immediately, and doesn't affect the current
			 * character
			 */
			*tty->flip.flag_buf_ptr = TTY_OVERRUN;
			tty->flip.count++;
			tty->flip.flag_buf_ptr++;
			tty->flip.char_buf_ptr++;
		}
#if defined(CONFIG_SERIAL_CONSOLE) && defined(CONFIG_MAGIC_SYSRQ)
	ignore_char:
#endif
		*status = serial_inp(info, UART_LSR);
	} while ((*status & UART_LSR_DR) && (max_count-- > 0));
#if (LINUX_VERSION_CODE > 131394) /* 2.1.66 */
	tty_flip_buffer_push(tty);
#else
	queue_task_irq_off(&tty->flip.tqueue, &tq_timer);
#endif	
}

static _INLINE_ void transmit_chars(struct async_struct *info, int *intr_done)
{
	int count;

	if (info->x_char) {
		serial_outp(info, UART_TX, info->x_char);
		info->state->icount.tx++;
		info->x_char = 0;
		if (intr_done)
			*intr_done = 0;
		return;
	}
	if (info->xmit.head == info->xmit.tail
	    || info->tty->stopped
	    || info->tty->hw_stopped) {
		info->IER &= ~UART_IER_THRI;
		serial_out(info, UART_IER, info->IER);
		return;
	}
	
	count = info->xmit_fifo_size;
	do {
		serial_out(info, UART_TX, info->xmit.buf[info->xmit.tail]);
		info->xmit.tail = (info->xmit.tail + 1) & (SERIAL_XMIT_SIZE-1);
		info->state->icount.tx++;
		if (info->xmit.head == info->xmit.tail)
			break;
	} while (--count > 0);
	
	if (CIRC_CNT(info->xmit.head,
		     info->xmit.tail,
		     SERIAL_XMIT_SIZE) < WAKEUP_CHARS)
		rs_sched_event(info, RS_EVENT_WRITE_WAKEUP);

#ifdef SERIAL_DEBUG_INTR
	printk("THRE...");
#endif
	if (intr_done)
		*intr_done = 0;

	if (info->xmit.head == info->xmit.tail) {
		info->IER &= ~UART_IER_THRI;
		serial_out(info, UART_IER, info->IER);
	}
}

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
			printk("doing serial hangup...");
#endif
			if (info->tty)
				tty_hangup(info->tty);
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

#ifdef CONFIG_SERIAL_SHARE_IRQ
/*
 * This is the serial driver's generic interrupt routine
 */
static void rs_interrupt(int irq, void *dev_id, struct pt_regs * regs)
{
	int status, iir;
	struct async_struct * info;
	int pass_counter = 0;
	struct async_struct *end_mark = 0;
#ifdef CONFIG_SERIAL_MULTIPORT	
	int first_multi = 0;
	struct rs_multiport_struct *multi;
#endif

#ifdef SERIAL_DEBUG_INTR
	printk("rs_interrupt(%d)...", irq);
#endif

	info = IRQ_ports[irq];
	if (!info)
		return;

#ifdef CONFIG_SERIAL_MULTIPORT	
	multi = &rs_multiport[irq];
	if (multi->port_monitor)
		first_multi = inb(multi->port_monitor);
#endif

	do {
		if (!info->tty ||
		    ((iir=serial_in(info, UART_IIR)) & UART_IIR_NO_INT)) {
			if (!end_mark)
				end_mark = info;
			goto next;
		}
#ifdef SERIAL_DEBUG_INTR
		printk("IIR = %x...", serial_in(info, UART_IIR));
#endif
		end_mark = 0;

		info->last_active = jiffies;

		status = serial_inp(info, UART_LSR);
#ifdef SERIAL_DEBUG_INTR
		printk("status = %x...", status);
#endif
		if (status & UART_LSR_DR)
			receive_chars(info, &status, regs);
		check_modem_status(info);
#ifdef CONFIG_MELAN
		if ((status & UART_LSR_THRE) ||
			/* for buggy ELAN processors */
			((iir & UART_IIR_ID) == UART_IIR_THRI))
			transmit_chars(info, 0);
#else
		if (status & UART_LSR_THRE)
			transmit_chars(info, 0);
#endif

	next:
		info = info->next_port;
		if (!info) {
			info = IRQ_ports[irq];
			if (pass_counter++ > RS_ISR_PASS_LIMIT) {
#if 0
				printk("rs loop break\n");
#endif
				break; 	/* Prevent infinite loops */
			}
			continue;
		}
	} while (end_mark != info);
#ifdef CONFIG_SERIAL_MULTIPORT	
	if (multi->port_monitor)
		printk("rs port monitor (normal) irq %d: 0x%x, 0x%x\n",
		       info->state->irq, first_multi,
		       inb(multi->port_monitor));
#endif
#ifdef SERIAL_DEBUG_INTR
	printk("end.\n");
#endif
}
#endif /* #ifdef CONFIG_SERIAL_SHARE_IRQ */


/*
 * This is the serial driver's interrupt routine for a single port
 */
static void rs_interrupt_single(int irq, void *dev_id, struct pt_regs * regs)
{
	int status, iir;
	int pass_counter = 0;
	struct async_struct * info;
#ifdef CONFIG_SERIAL_MULTIPORT	
	int first_multi = 0;
	struct rs_multiport_struct *multi;
#endif
	
#ifdef SERIAL_DEBUG_INTR
	printk("rs_interrupt_single(%d)...", irq);
#endif

	info = IRQ_ports[irq];
	if (!info || !info->tty)
		return;

#ifdef CONFIG_SERIAL_MULTIPORT	
	multi = &rs_multiport[irq];
	if (multi->port_monitor)
		first_multi = inb(multi->port_monitor);
#endif

	iir = serial_in(info, UART_IIR);
	do {
		status = serial_inp(info, UART_LSR);
#ifdef SERIAL_DEBUG_INTR
		printk("status = %x...", status);
#endif
		if (status & UART_LSR_DR)
			receive_chars(info, &status, regs);
		check_modem_status(info);
#ifdef CONFIG_MELAN
		if ((status & UART_LSR_THRE) ||
		    /* For buggy ELAN processors */
		    ((iir & UART_IIR_ID) == UART_IIR_THRI))
			transmit_chars(info, 0);
#else
		if (status & UART_LSR_THRE)
			transmit_chars(info, 0);
#endif
		if (pass_counter++ > RS_ISR_PASS_LIMIT) {
#if SERIAL_DEBUG_INTR
			printk("rs_single loop break.\n");
#endif
			break;
		}
		iir = serial_in(info, UART_IIR);
#ifdef SERIAL_DEBUG_INTR
		printk("IIR = %x...", iir);
#endif
	} while ((iir & UART_IIR_NO_INT) == 0);
	info->last_active = jiffies;
#ifdef CONFIG_SERIAL_MULTIPORT	
	if (multi->port_monitor)
		printk("rs port monitor (single) irq %d: 0x%x, 0x%x\n",
		       info->state->irq, first_multi,
		       inb(multi->port_monitor));
#endif
#ifdef SERIAL_DEBUG_INTR
	printk("end.\n");
#endif
}

#ifdef CONFIG_SERIAL_MULTIPORT	
/*
 * This is the serial driver's for multiport boards
 */
static void rs_interrupt_multi(int irq, void *dev_id, struct pt_regs * regs)
{
	int status;
	struct async_struct * info;
	int pass_counter = 0;
	int first_multi= 0;
	struct rs_multiport_struct *multi;

#ifdef SERIAL_DEBUG_INTR
	printk("rs_interrupt_multi(%d)...", irq);
#endif

	info = IRQ_ports[irq];
	if (!info)
		return;
	multi = &rs_multiport[irq];
	if (!multi->port1) {
		/* Should never happen */
		printk("rs_interrupt_multi: NULL port1!\n");
		return;
	}
	if (multi->port_monitor)
		first_multi = inb(multi->port_monitor);
	
	while (1) {
		if (!info->tty ||
		    (serial_in(info, UART_IIR) & UART_IIR_NO_INT))
			goto next;

		info->last_active = jiffies;

		status = serial_inp(info, UART_LSR);
#ifdef SERIAL_DEBUG_INTR
		printk("status = %x...", status);
#endif
		if (status & UART_LSR_DR)
			receive_chars(info, &status, regs);
		check_modem_status(info);
		if (status & UART_LSR_THRE)
			transmit_chars(info, 0);

	next:
		info = info->next_port;
		if (info)
			continue;

		info = IRQ_ports[irq];
		/*
		 * The user was a bonehead, and misconfigured their
		 * multiport info.  Rather than lock up the kernel
		 * in an infinite loop, if we loop too many times,
		 * print a message and break out of the loop.
		 */
		if (pass_counter++ > RS_ISR_PASS_LIMIT) {
			printk("Misconfigured multiport serial info "
			       "for irq %d.  Breaking out irq loop\n", irq);
			break; 
		}
		if (multi->port_monitor)
			printk("rs port monitor irq %d: 0x%x, 0x%x\n",
			       info->state->irq, first_multi,
			       inb(multi->port_monitor));
		if ((inb(multi->port1) & multi->mask1) != multi->match1)
			continue;
		if (!multi->port2)
			break;
		if ((inb(multi->port2) & multi->mask2) != multi->match2)
			continue;
		if (!multi->port3)
			break;
		if ((inb(multi->port3) & multi->mask3) != multi->match3)
			continue;
		if (!multi->port4)
			break;
		if ((inb(multi->port4) & multi->mask4) != multi->match4)
			continue;
		break;
	} 
#ifdef SERIAL_DEBUG_INTR
	printk("end.\n");
#endif
}
#endif

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
#ifdef SERIAL_HAVE_POLL_WAIT
		wake_up_interruptible(&tty->poll_wait);
#endif
	}
}

/*
 * This subroutine is called when the RS_TIMER goes off.  It is used
 * by the serial driver to handle ports that do not have an interrupt
 * (irq=0).  This doesn't work very well for 16450's, but gives barely
 * passable results for a 16550A.  (Although at the expense of much
 * CPU overhead).
 */
static void rs_timer(unsigned long dummy)
{
	static unsigned long last_strobe;
	struct async_struct *info;
	unsigned int	i;
	unsigned long flags;

	if ((jiffies - last_strobe) >= RS_STROBE_TIME) {
		for (i=0; i < NR_IRQS; i++) {
			info = IRQ_ports[i];
			if (!info)
				continue;
			save_flags(flags); cli();
#ifdef CONFIG_SERIAL_SHARE_IRQ
			if (info->next_port) {
				do {
					serial_out(info, UART_IER, 0);
					info->IER |= UART_IER_THRI;
					serial_out(info, UART_IER, info->IER);
					info = info->next_port;
				} while (info);
#ifdef CONFIG_SERIAL_MULTIPORT
				if (rs_multiport[i].port1)
					rs_interrupt_multi(i, NULL, NULL);
				else
#endif
					rs_interrupt(i, NULL, NULL);
			} else
#endif /* CONFIG_SERIAL_SHARE_IRQ */
				rs_interrupt_single(i, NULL, NULL);
			restore_flags(flags);
		}
	}
	last_strobe = jiffies;
	mod_timer(&serial_timer, jiffies + RS_STROBE_TIME);

	if (IRQ_ports[0]) {
		save_flags(flags); cli();
#ifdef CONFIG_SERIAL_SHARE_IRQ
		rs_interrupt(0, NULL, NULL);
#else
		rs_interrupt_single(0, NULL, NULL);
#endif
		restore_flags(flags);

		mod_timer(&serial_timer, jiffies + IRQ_timeout[0]);
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
	IRQ_timeout[irq] = (timeout > 3) ? timeout-2 : 1;
}

#ifdef CONFIG_SERIAL_RSA
/* Attempts to turn on the RSA FIFO.  Returns zero on failure */
static int enable_rsa(struct async_struct *info)
{
	unsigned char mode;
	int result;
	unsigned long flags;

	save_flags(flags); cli();
	mode = serial_inp(info, UART_RSA_MSR);
	result = mode & UART_RSA_MSR_FIFO;

	if (!result) {
		serial_outp(info, UART_RSA_MSR, mode | UART_RSA_MSR_FIFO);
		mode = serial_inp(info, UART_RSA_MSR);
		result = mode & UART_RSA_MSR_FIFO;
	}

	restore_flags(flags);
	return result;
}

/* Attempts to turn off the RSA FIFO.  Returns zero on failure */
static int disable_rsa(struct async_struct *info)
{
	unsigned char mode;
	int result;
	unsigned long flags;

	save_flags(flags); cli();
	mode = serial_inp(info, UART_RSA_MSR);
	result = !(mode & UART_RSA_MSR_FIFO);

	if (!result) {
		serial_outp(info, UART_RSA_MSR, mode & ~UART_RSA_MSR_FIFO);
		mode = serial_inp(info, UART_RSA_MSR);
		result = !(mode & UART_RSA_MSR_FIFO);
	}

	restore_flags(flags);
	return result;
}
#endif /* CONFIG_SERIAL_RSA */

static int startup(struct async_struct * info)
{
	unsigned long flags;
	int	retval=0;
	void (*handler)(int, void *, struct pt_regs *);
	struct serial_state *state= info->state;
	unsigned long page;
#ifdef CONFIG_SERIAL_MANY_PORTS
	unsigned short ICP;
#endif

	page = get_zeroed_page(GFP_KERNEL);
	if (!page)
		return -ENOMEM;

	save_flags(flags); cli();

	if (info->flags & ASYNC_INITIALIZED) {
		free_page(page);
		goto errout;
	}

	if (!CONFIGURED_SERIAL_PORT(state) || !state->type) {
		if (info->tty)
			set_bit(TTY_IO_ERROR, &info->tty->flags);
		free_page(page);
		goto errout;
	}
	if (info->xmit.buf)
		free_page(page);
	else
		info->xmit.buf = (unsigned char *) page;

#ifdef SERIAL_DEBUG_OPEN
	printk("starting up ttys%d (irq %d)...", info->line, state->irq);
#endif

	if (uart_config[state->type].flags & UART_STARTECH) {
		/* Wake up UART */
		serial_outp(info, UART_LCR, 0xBF);
		serial_outp(info, UART_EFR, UART_EFR_ECB);
		/*
		 * Turn off LCR == 0xBF so we actually set the IER
		 * register on the XR16C850
		 */
		serial_outp(info, UART_LCR, 0);
		serial_outp(info, UART_IER, 0);
		/*
		 * Now reset LCR so we can turn off the ECB bit
		 */
		serial_outp(info, UART_LCR, 0xBF);
		serial_outp(info, UART_EFR, 0);
		/*
		 * For a XR16C850, we need to set the trigger levels
		 */
		if (state->type == PORT_16850) {
			serial_outp(info, UART_FCTR, UART_FCTR_TRGD |
					UART_FCTR_RX);
			serial_outp(info, UART_TRG, UART_TRG_96);
			serial_outp(info, UART_FCTR, UART_FCTR_TRGD |
					UART_FCTR_TX);
			serial_outp(info, UART_TRG, UART_TRG_96);
		}
		serial_outp(info, UART_LCR, 0);
	}

	if (state->type == PORT_16750) {
		/* Wake up UART */
		serial_outp(info, UART_IER, 0);
	}

	if (state->type == PORT_16C950) {
		/* Wake up and initialize UART */
		info->ACR = 0;
		serial_outp(info, UART_LCR, 0xBF);
		serial_outp(info, UART_EFR, UART_EFR_ECB);
		serial_outp(info, UART_IER, 0);
		serial_outp(info, UART_LCR, 0);
		serial_icr_write(info, UART_CSR, 0); /* Reset the UART */
		serial_outp(info, UART_LCR, 0xBF);
		serial_outp(info, UART_EFR, UART_EFR_ECB);
		serial_outp(info, UART_LCR, 0);
	}

#ifdef CONFIG_SERIAL_RSA
	/*
	 * If this is an RSA port, see if we can kick it up to the
	 * higher speed clock.
	 */
	if (state->type == PORT_RSA) {
		if (state->baud_base != SERIAL_RSA_BAUD_BASE &&
		    enable_rsa(info))
			state->baud_base = SERIAL_RSA_BAUD_BASE;
		if (state->baud_base == SERIAL_RSA_BAUD_BASE)
			serial_outp(info, UART_RSA_FRR, 0);
	}
#endif

	/*
	 * Clear the FIFO buffers and disable them
	 * (they will be reenabled in change_speed())
	 */
	if (uart_config[state->type].flags & UART_CLEAR_FIFO) {
		serial_outp(info, UART_FCR, UART_FCR_ENABLE_FIFO);
		serial_outp(info, UART_FCR, (UART_FCR_ENABLE_FIFO |
					     UART_FCR_CLEAR_RCVR |
					     UART_FCR_CLEAR_XMIT));
		serial_outp(info, UART_FCR, 0);
	}

	/*
	 * Clear the interrupt registers.
	 */
	(void) serial_inp(info, UART_LSR);
	(void) serial_inp(info, UART_RX);
	(void) serial_inp(info, UART_IIR);
	(void) serial_inp(info, UART_MSR);

	/*
	 * At this point there's no way the LSR could still be 0xFF;
	 * if it is, then bail out, because there's likely no UART
	 * here.
	 */
	if (!(info->flags & ASYNC_BUGGY_UART) &&
	    (serial_inp(info, UART_LSR) == 0xff)) {
		printk("ttyS%d: LSR safety check engaged!\n", state->line);
		if (capable(CAP_SYS_ADMIN)) {
			if (info->tty)
				set_bit(TTY_IO_ERROR, &info->tty->flags);
		} else
			retval = -ENODEV;
		goto errout;
	}
	
	/*
	 * Allocate the IRQ if necessary
	 */
	if (state->irq && (!IRQ_ports[state->irq] ||
			  !IRQ_ports[state->irq]->next_port)) {
		if (IRQ_ports[state->irq]) {
#ifdef CONFIG_SERIAL_SHARE_IRQ
			free_irq(state->irq, &IRQ_ports[state->irq]);
#ifdef CONFIG_SERIAL_MULTIPORT				
			if (rs_multiport[state->irq].port1)
				handler = rs_interrupt_multi;
			else
#endif
				handler = rs_interrupt;
#else
			retval = -EBUSY;
			goto errout;
#endif /* CONFIG_SERIAL_SHARE_IRQ */
		} else 
			handler = rs_interrupt_single;

		retval = request_irq(state->irq, handler, SA_SHIRQ,
				     "serial", &IRQ_ports[state->irq]);
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
	 * Now, initialize the UART 
	 */
	serial_outp(info, UART_LCR, UART_LCR_WLEN8);	/* reset DLAB */

	info->MCR = 0;
	if (info->tty->termios->c_cflag & CBAUD)
		info->MCR = UART_MCR_DTR | UART_MCR_RTS;
#ifdef CONFIG_SERIAL_MANY_PORTS
	if (info->flags & ASYNC_FOURPORT) {
		if (state->irq == 0)
			info->MCR |= UART_MCR_OUT1;
	} else
#endif
	{
		if (state->irq != 0)
			info->MCR |= UART_MCR_OUT2;
	}
	info->MCR |= ALPHA_KLUDGE_MCR; 		/* Don't ask */
	serial_outp(info, UART_MCR, info->MCR);
	
	/*
	 * Finally, enable interrupts
	 */
	info->IER = UART_IER_MSI | UART_IER_RLSI | UART_IER_RDI;
	serial_outp(info, UART_IER, info->IER);	/* enable interrupts */
	
#ifdef CONFIG_SERIAL_MANY_PORTS
	if (info->flags & ASYNC_FOURPORT) {
		/* Enable interrupts on the AST Fourport board */
		ICP = (info->port & 0xFE0) | 0x01F;
		outb_p(0x80, ICP);
		(void) inb_p(ICP);
	}
#endif

	/*
	 * And clear the interrupt registers again for luck.
	 */
	(void)serial_inp(info, UART_LSR);
	(void)serial_inp(info, UART_RX);
	(void)serial_inp(info, UART_IIR);
	(void)serial_inp(info, UART_MSR);

	if (info->tty)
		clear_bit(TTY_IO_ERROR, &info->tty->flags);
	info->xmit.head = info->xmit.tail = 0;

	/*
	 * Set up serial timers...
	 */
	mod_timer(&serial_timer, jiffies + 2*HZ/100);

	/*
	 * Set up the tty->alt_speed kludge
	 */
#if (LINUX_VERSION_CODE >= 131394) /* Linux 2.1.66 */
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
#endif
	
	/*
	 * and set the speed of the serial port
	 */
	change_speed(info, 0);

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
	printk("Shutting down serial port %d (irq %d)....", info->line,
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
			free_irq(state->irq, &IRQ_ports[state->irq]);
			retval = request_irq(state->irq, rs_interrupt_single,
					     SA_SHIRQ, "serial",
					     &IRQ_ports[state->irq]);
			
			if (retval)
				printk("serial shutdown: request_irq: error %d"
				       "  Couldn't reacquire IRQ.\n", retval);
		} else
			free_irq(state->irq, &IRQ_ports[state->irq]);
	}

	if (info->xmit.buf) {
		unsigned long pg = (unsigned long) info->xmit.buf;
		info->xmit.buf = 0;
		free_page(pg);
	}

	info->IER = 0;
	serial_outp(info, UART_IER, 0x00);	/* disable all intrs */
#ifdef CONFIG_SERIAL_MANY_PORTS
	if (info->flags & ASYNC_FOURPORT) {
		/* reset interrupts on the AST Fourport board */
		(void) inb((info->port & 0xFE0) | 0x01F);
		info->MCR |= UART_MCR_OUT1;
	} else
#endif
		info->MCR &= ~UART_MCR_OUT2;
	info->MCR |= ALPHA_KLUDGE_MCR; 		/* Don't ask */
	
	/* disable break condition */
	serial_out(info, UART_LCR, serial_inp(info, UART_LCR) & ~UART_LCR_SBC);
	
	if (!info->tty || (info->tty->termios->c_cflag & HUPCL))
		info->MCR &= ~(UART_MCR_DTR|UART_MCR_RTS);
	serial_outp(info, UART_MCR, info->MCR);

	/* disable FIFO's */	
	serial_outp(info, UART_FCR, (UART_FCR_ENABLE_FIFO |
				     UART_FCR_CLEAR_RCVR |
				     UART_FCR_CLEAR_XMIT));
	serial_outp(info, UART_FCR, 0);

#ifdef CONFIG_SERIAL_RSA
	/*
	 * Reset the RSA board back to 115kbps compat mode.
	 */
	if ((state->type == PORT_RSA) &&
	    (state->baud_base == SERIAL_RSA_BAUD_BASE &&
	     disable_rsa(info)))
		state->baud_base = SERIAL_RSA_BAUD_BASE_LO;
#endif
	

	(void)serial_in(info, UART_RX);    /* read data port to reset things */
	
	if (info->tty)
		set_bit(TTY_IO_ERROR, &info->tty->flags);

	if (uart_config[info->state->type].flags & UART_STARTECH) {
		/* Arrange to enter sleep mode */
		serial_outp(info, UART_LCR, 0xBF);
		serial_outp(info, UART_EFR, UART_EFR_ECB);
		serial_outp(info, UART_LCR, 0);
		serial_outp(info, UART_IER, UART_IERX_SLEEP);
		serial_outp(info, UART_LCR, 0xBF);
		serial_outp(info, UART_EFR, 0);
		serial_outp(info, UART_LCR, 0);
	}
	if (info->state->type == PORT_16750) {
		/* Arrange to enter sleep mode */
		serial_outp(info, UART_IER, UART_IERX_SLEEP);
	}
	info->flags &= ~ASYNC_INITIALIZED;
	restore_flags(flags);
}

#if (LINUX_VERSION_CODE < 131394) /* Linux 2.1.66 */
static int baud_table[] = {
	0, 50, 75, 110, 134, 150, 200, 300,
	600, 1200, 1800, 2400, 4800, 9600, 19200,
	38400, 57600, 115200, 230400, 460800, 0 };

static int tty_get_baud_rate(struct tty_struct *tty)
{
	struct async_struct * info = (struct async_struct *)tty->driver_data;
	unsigned int cflag, i;

	cflag = tty->termios->c_cflag;

	i = cflag & CBAUD;
	if (i & CBAUDEX) {
		i &= ~CBAUDEX;
		if (i < 1 || i > 2) 
			tty->termios->c_cflag &= ~CBAUDEX;
		else
			i += 15;
	}
	if (i == 15) {
		if ((info->flags & ASYNC_SPD_MASK) == ASYNC_SPD_HI)
			i += 1;
		if ((info->flags & ASYNC_SPD_MASK) == ASYNC_SPD_VHI)
			i += 2;
	}
	return baud_table[i];
}
#endif

/*
 * This routine is called to set the UART divisor registers to match
 * the specified baud rate for a serial port.
 */
static void change_speed(struct async_struct *info,
			 struct termios *old_termios)
{
	int	quot = 0, baud_base, baud;
	unsigned cflag, cval, fcr = 0;
	int	bits;
	unsigned long	flags;

	if (!info->tty || !info->tty->termios)
		return;
	cflag = info->tty->termios->c_cflag;
	if (!CONFIGURED_SERIAL_PORT(info))
		return;

	/* byte size and parity */
	switch (cflag & CSIZE) {
	      case CS5: cval = 0x00; bits = 7; break;
	      case CS6: cval = 0x01; bits = 8; break;
	      case CS7: cval = 0x02; bits = 9; break;
	      case CS8: cval = 0x03; bits = 10; break;
	      /* Never happens, but GCC is too dumb to figure it out */
	      default:  cval = 0x00; bits = 7; break;
	      }
	if (cflag & CSTOPB) {
		cval |= 0x04;
		bits++;
	}
	if (cflag & PARENB) {
		cval |= UART_LCR_PARITY;
		bits++;
	}
	if (!(cflag & PARODD))
		cval |= UART_LCR_EPAR;
#ifdef CMSPAR
	if (cflag & CMSPAR)
		cval |= UART_LCR_SPAR;
#endif

	/* Determine divisor based on baud rate */
	baud = tty_get_baud_rate(info->tty);
	if (!baud)
		baud = 9600;	/* B0 transition handled in rs_set_termios */
#ifdef CONFIG_SERIAL_RSA
	if ((info->state->type == PORT_RSA) &&
	    (info->state->baud_base != SERIAL_RSA_BAUD_BASE) &&
	    enable_rsa(info))
		info->state->baud_base = SERIAL_RSA_BAUD_BASE;
#endif
	baud_base = info->state->baud_base;
	if (info->state->type == PORT_16C950) {
		if (baud <= baud_base)
			serial_icr_write(info, UART_TCR, 0);
		else if (baud <= 2*baud_base) {
			serial_icr_write(info, UART_TCR, 0x8);
			baud_base = baud_base * 2;
		} else if (baud <= 4*baud_base) {
			serial_icr_write(info, UART_TCR, 0x4);
			baud_base = baud_base * 4;
		} else
			serial_icr_write(info, UART_TCR, 0);
	}
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
	/* If the quotient is zero refuse the change */
	if (!quot && old_termios) {
		info->tty->termios->c_cflag &= ~CBAUD;
		info->tty->termios->c_cflag |= (old_termios->c_cflag & CBAUD);
		baud = tty_get_baud_rate(info->tty);
		if (!baud)
			baud = 9600;
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
	}
	/* As a last resort, if the quotient is zero, default to 9600 bps */
	if (!quot)
		quot = baud_base / 9600;
	/*
	 * Work around a bug in the Oxford Semiconductor 952 rev B
	 * chip which causes it to seriously miscalculate baud rates
	 * when DLL is 0.
	 */
	if (((quot & 0xFF) == 0) && (info->state->type == PORT_16C950) &&
	    (info->state->revision == 0x5201))
		quot++;
	
	info->quot = quot;
	info->timeout = ((info->xmit_fifo_size*HZ*bits*quot) / baud_base);
	info->timeout += HZ/50;		/* Add .02 seconds of slop */

	/* Set up FIFO's */
	if (uart_config[info->state->type].flags & UART_USE_FIFO) {
		if ((info->state->baud_base / quot) < 2400)
			fcr = UART_FCR_ENABLE_FIFO | UART_FCR_TRIGGER_1;
#ifdef CONFIG_SERIAL_RSA
		else if (info->state->type == PORT_RSA)
			fcr = UART_FCR_ENABLE_FIFO | UART_FCR_TRIGGER_14;
#endif
		else
			fcr = UART_FCR_ENABLE_FIFO | UART_FCR_TRIGGER_8;
	}
	if (info->state->type == PORT_16750)
		fcr |= UART_FCR7_64BYTE;
	
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

	/*
	 * Set up parity check flag
	 */
#define RELEVANT_IFLAG(iflag) (iflag & (IGNBRK|BRKINT|IGNPAR|PARMRK|INPCK))

	info->read_status_mask = UART_LSR_OE | UART_LSR_THRE | UART_LSR_DR;
	if (I_INPCK(info->tty))
		info->read_status_mask |= UART_LSR_FE | UART_LSR_PE;
	if (I_BRKINT(info->tty) || I_PARMRK(info->tty))
		info->read_status_mask |= UART_LSR_BI;
	
	/*
	 * Characters to ignore
	 */
	info->ignore_status_mask = 0;
	if (I_IGNPAR(info->tty))
		info->ignore_status_mask |= UART_LSR_PE | UART_LSR_FE;
	if (I_IGNBRK(info->tty)) {
		info->ignore_status_mask |= UART_LSR_BI;
		/*
		 * If we're ignore parity and break indicators, ignore 
		 * overruns too.  (For real raw support).
		 */
		if (I_IGNPAR(info->tty))
			info->ignore_status_mask |= UART_LSR_OE;
	}
	/*
	 * !!! ignore all characters if CREAD is not set
	 */
	if ((cflag & CREAD) == 0)
		info->ignore_status_mask |= UART_LSR_DR;
	save_flags(flags); cli();
	if (uart_config[info->state->type].flags & UART_STARTECH) {
		serial_outp(info, UART_LCR, 0xBF);
		serial_outp(info, UART_EFR,
			    (cflag & CRTSCTS) ? UART_EFR_CTS : 0);
	}
	serial_outp(info, UART_LCR, cval | UART_LCR_DLAB);	/* set DLAB */
	serial_outp(info, UART_DLL, quot & 0xff);	/* LS of divisor */
	serial_outp(info, UART_DLM, quot >> 8);		/* MS of divisor */
	if (info->state->type == PORT_16750)
		serial_outp(info, UART_FCR, fcr); 	/* set fcr */
	serial_outp(info, UART_LCR, cval);		/* reset DLAB */
	info->LCR = cval;				/* Save LCR */
 	if (info->state->type != PORT_16750) {
 		if (fcr & UART_FCR_ENABLE_FIFO) {
 			/* emulated UARTs (Lucent Venus 167x) need two steps */
 			serial_outp(info, UART_FCR, UART_FCR_ENABLE_FIFO);
 		}
		serial_outp(info, UART_FCR, fcr); 	/* set fcr */
	}
	restore_flags(flags);
}

static void rs_put_char(struct tty_struct *tty, unsigned char ch)
{
	struct async_struct *info = (struct async_struct *)tty->driver_data;
	unsigned long flags;

	if (serial_paranoia_check(info, tty->device, "rs_put_char"))
		return;

	if (!tty || !info->xmit.buf)
		return;

	save_flags(flags); cli();
	if (CIRC_SPACE(info->xmit.head,
		       info->xmit.tail,
		       SERIAL_XMIT_SIZE) == 0) {
		restore_flags(flags);
		return;
	}

	info->xmit.buf[info->xmit.head] = ch;
	info->xmit.head = (info->xmit.head + 1) & (SERIAL_XMIT_SIZE-1);
	restore_flags(flags);
}

static void rs_flush_chars(struct tty_struct *tty)
{
	struct async_struct *info = (struct async_struct *)tty->driver_data;
	unsigned long flags;
				
	if (serial_paranoia_check(info, tty->device, "rs_flush_chars"))
		return;

	if (info->xmit.head == info->xmit.tail
	    || tty->stopped
	    || tty->hw_stopped
	    || !info->xmit.buf)
		return;

	save_flags(flags); cli();
	info->IER |= UART_IER_THRI;
	serial_out(info, UART_IER, info->IER);
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
			c = CIRC_SPACE_TO_END(info->xmit.head,
					      info->xmit.tail,
					      SERIAL_XMIT_SIZE);
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
	if (info->xmit.head != info->xmit.tail
	    && !tty->stopped
	    && !tty->hw_stopped
	    && !(info->IER & UART_IER_THRI)) {
		info->IER |= UART_IER_THRI;
		serial_out(info, UART_IER, info->IER);
	}
	return ret;
}

static int rs_write_room(struct tty_struct *tty)
{
	struct async_struct *info = (struct async_struct *)tty->driver_data;

	if (serial_paranoia_check(info, tty->device, "rs_write_room"))
		return 0;
	return CIRC_SPACE(info->xmit.head, info->xmit.tail, SERIAL_XMIT_SIZE);
}

static int rs_chars_in_buffer(struct tty_struct *tty)
{
	struct async_struct *info = (struct async_struct *)tty->driver_data;
				
	if (serial_paranoia_check(info, tty->device, "rs_chars_in_buffer"))
		return 0;
	return CIRC_CNT(info->xmit.head, info->xmit.tail, SERIAL_XMIT_SIZE);
}

static void rs_flush_buffer(struct tty_struct *tty)
{
	struct async_struct *info = (struct async_struct *)tty->driver_data;
	unsigned long flags;
	
	if (serial_paranoia_check(info, tty->device, "rs_flush_buffer"))
		return;
	save_flags(flags); cli();
	info->xmit.head = info->xmit.tail = 0;
	restore_flags(flags);
	wake_up_interruptible(&tty->write_wait);
#ifdef SERIAL_HAVE_POLL_WAIT
	wake_up_interruptible(&tty->poll_wait);
#endif
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
		info->IER |= UART_IER_THRI;
		serial_out(info, UART_IER, info->IER);
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
	unsigned long flags;
#ifdef SERIAL_DEBUG_THROTTLE
	char	buf[64];
	
	printk("throttle %s: %d....\n", tty_name(tty, buf),
	       tty->ldisc.chars_in_buffer(tty));
#endif

	if (serial_paranoia_check(info, tty->device, "rs_throttle"))
		return;
	
	if (I_IXOFF(tty))
		rs_send_xchar(tty, STOP_CHAR(tty));

	if (tty->termios->c_cflag & CRTSCTS)
		info->MCR &= ~UART_MCR_RTS;

	save_flags(flags); cli();
	serial_out(info, UART_MCR, info->MCR);
	restore_flags(flags);
}

static void rs_unthrottle(struct tty_struct * tty)
{
	struct async_struct *info = (struct async_struct *)tty->driver_data;
	unsigned long flags;
#ifdef SERIAL_DEBUG_THROTTLE
	char	buf[64];
	
	printk("unthrottle %s: %d....\n", tty_name(tty, buf),
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
	if (tty->termios->c_cflag & CRTSCTS)
		info->MCR |= UART_MCR_RTS;
	save_flags(flags); cli();
	serial_out(info, UART_MCR, info->MCR);
	restore_flags(flags);
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
	if (HIGH_BITS_OFFSET)
		tmp.port_high = state->port >> HIGH_BITS_OFFSET;
	else
		tmp.port_high = 0;
	tmp.irq = state->irq;
	tmp.flags = state->flags;
	tmp.xmit_fifo_size = state->xmit_fifo_size;
	tmp.baud_base = state->baud_base;
	tmp.close_delay = state->close_delay;
	tmp.closing_wait = state->closing_wait;
	tmp.custom_divisor = state->custom_divisor;
	tmp.hub6 = state->hub6;
	tmp.io_type = state->io_type;
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
	unsigned long		new_port;

	if (copy_from_user(&new_serial,new_info,sizeof(new_serial)))
		return -EFAULT;
	state = info->state;
	old_state = *state;

	new_port = new_serial.port;
	if (HIGH_BITS_OFFSET)
		new_port += (unsigned long) new_serial.port_high << HIGH_BITS_OFFSET;

	change_irq = new_serial.irq != state->irq;
	change_port = (new_port != ((int) state->port)) ||
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
		info->flags = ((info->flags & ~ASYNC_USR_MASK) |
			       (new_serial.flags & ASYNC_USR_MASK));
		state->custom_divisor = new_serial.custom_divisor;
		goto check_and_exit;
	}

	new_serial.irq = irq_cannonicalize(new_serial.irq);

	if ((new_serial.irq >= NR_IRQS) || (new_serial.irq < 0) || 
	    (new_serial.baud_base < 9600)|| (new_serial.type < PORT_UNKNOWN) ||
	    (new_serial.type > PORT_MAX) || (new_serial.type == PORT_CIRRUS) ||
	    (new_serial.type == PORT_STARTECH)) {
		return -EINVAL;
	}

	if ((new_serial.type != state->type) ||
	    (new_serial.xmit_fifo_size <= 0))
		new_serial.xmit_fifo_size =
			uart_config[new_serial.type].dfl_xmit_fifo_size;

	/* Make sure address is not already in use */
	if (new_serial.type) {
		for (i = 0 ; i < NR_PORTS; i++)
			if ((state != &rs_table[i]) &&
			    (rs_table[i].io_type == SERIAL_IO_PORT) &&
			    (rs_table[i].port == new_port) &&
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
	state->close_delay = new_serial.close_delay * HZ/100;
	state->closing_wait = new_serial.closing_wait * HZ/100;
#if (LINUX_VERSION_CODE > 0x20100)
	info->tty->low_latency = (info->flags & ASYNC_LOW_LATENCY) ? 1 : 0;
#endif
	info->xmit_fifo_size = state->xmit_fifo_size =
		new_serial.xmit_fifo_size;

	if ((state->type != PORT_UNKNOWN) && state->port) {
#ifdef CONFIG_SERIAL_RSA
		if (old_state.type == PORT_RSA)
			release_region(state->port + UART_RSA_BASE, 16);
		else
#endif
		release_region(state->port,8);
	}
	state->type = new_serial.type;
	if (change_port || change_irq) {
		/*
		 * We need to shutdown the serial port at the old
		 * port/irq combination.
		 */
		shutdown(info);
		state->irq = new_serial.irq;
		info->port = state->port = new_port;
		info->hub6 = state->hub6 = new_serial.hub6;
		if (info->hub6)
			info->io_type = state->io_type = SERIAL_IO_HUB6;
		else if (info->io_type == SERIAL_IO_HUB6)
			info->io_type = state->io_type = SERIAL_IO_PORT;
	}
	if ((state->type != PORT_UNKNOWN) && state->port) {
#ifdef CONFIG_SERIAL_RSA
		if (state->type == PORT_RSA)
			request_region(state->port + UART_RSA_BASE,
				       16, "serial_rsa(set)");
		else
#endif
			request_region(state->port,8,"serial(set)");
	}

	
check_and_exit:
	if ((!state->port && !state->iomem_base) || !state->type)
		return 0;
	if (info->flags & ASYNC_INITIALIZED) {
		if (((old_state.flags & ASYNC_SPD_MASK) !=
		     (state->flags & ASYNC_SPD_MASK)) ||
		    (old_state.custom_divisor != state->custom_divisor)) {
#if (LINUX_VERSION_CODE >= 131394) /* Linux 2.1.66 */
			if ((state->flags & ASYNC_SPD_MASK) == ASYNC_SPD_HI)
				info->tty->alt_speed = 57600;
			if ((state->flags & ASYNC_SPD_MASK) == ASYNC_SPD_VHI)
				info->tty->alt_speed = 115200;
			if ((state->flags & ASYNC_SPD_MASK) == ASYNC_SPD_SHI)
				info->tty->alt_speed = 230400;
			if ((state->flags & ASYNC_SPD_MASK) == ASYNC_SPD_WARP)
				info->tty->alt_speed = 460800;
#endif
			change_speed(info, 0);
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
	unsigned char status;
	unsigned int result;
	unsigned long flags;

	save_flags(flags); cli();
	status = serial_in(info, UART_LSR);
	restore_flags(flags);
	result = ((status & UART_LSR_TEMT) ? TIOCSER_TEMT : 0);

	/*
	 * If we're about to load something into the transmit
	 * register, we'll pretend the transmitter isn't empty to
	 * avoid a race condition (depending on when the transmit
	 * interrupt happens).
	 */
	if (info->x_char || 
	    ((CIRC_CNT(info->xmit.head, info->xmit.tail,
		       SERIAL_XMIT_SIZE) > 0) &&
	     !info->tty->stopped && !info->tty->hw_stopped))
		result &= ~TIOCSER_TEMT;

	if (copy_to_user(value, &result, sizeof(int)))
		return -EFAULT;
	return 0;
}


static int get_modem_info(struct async_struct * info, unsigned int *value)
{
	unsigned char control, status;
	unsigned int result;
	unsigned long flags;

	control = info->MCR;
	save_flags(flags); cli();
	status = serial_in(info, UART_MSR);
	restore_flags(flags);
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

	if (copy_to_user(value, &result, sizeof(int)))
		return -EFAULT;
	return 0;
}

static int set_modem_info(struct async_struct * info, unsigned int cmd,
			  unsigned int *value)
{
	unsigned int arg;
	unsigned long flags;

	if (copy_from_user(&arg, value, sizeof(int)))
		return -EFAULT;

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
		if (arg & TIOCM_LOOP)
			info->MCR |= UART_MCR_LOOP;
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
		if (arg & TIOCM_LOOP)
			info->MCR &= ~UART_MCR_LOOP;
		break;
	case TIOCMSET:
		info->MCR = ((info->MCR & ~(UART_MCR_RTS |
#ifdef TIOCM_OUT1
					    UART_MCR_OUT1 |
					    UART_MCR_OUT2 |
#endif
					    UART_MCR_LOOP |
					    UART_MCR_DTR))
			     | ((arg & TIOCM_RTS) ? UART_MCR_RTS : 0)
#ifdef TIOCM_OUT1
			     | ((arg & TIOCM_OUT1) ? UART_MCR_OUT1 : 0)
			     | ((arg & TIOCM_OUT2) ? UART_MCR_OUT2 : 0)
#endif
			     | ((arg & TIOCM_LOOP) ? UART_MCR_LOOP : 0)
			     | ((arg & TIOCM_DTR) ? UART_MCR_DTR : 0));
		break;
	default:
		return -EINVAL;
	}
	save_flags(flags); cli();
	info->MCR |= ALPHA_KLUDGE_MCR; 		/* Don't ask */
	serial_out(info, UART_MCR, info->MCR);
	restore_flags(flags);
	return 0;
}

static int do_autoconfig(struct async_struct * info)
{
	int irq, retval;
	
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	
	if (info->state->count > 1)
		return -EBUSY;
	
	shutdown(info);

	autoconfig(info->state);
	if ((info->state->flags & ASYNC_AUTO_IRQ) &&
	    (info->state->port != 0  || info->state->iomem_base != 0) &&
	    (info->state->type != PORT_UNKNOWN)) {
		irq = detect_uart_irq(info->state);
		if (irq > 0)
			info->state->irq = irq;
	}

	retval = startup(info);
	if (retval)
		return retval;
	return 0;
}

/*
 * rs_break() --- routine which turns the break handling on or off
 */
#if (LINUX_VERSION_CODE < 131394) /* Linux 2.1.66 */
static void send_break(	struct async_struct * info, int duration)
{
	if (!CONFIGURED_SERIAL_PORT(info))
		return;
	current->state = TASK_INTERRUPTIBLE;
	current->timeout = jiffies + duration;
	cli();
	info->LCR |= UART_LCR_SBC;
	serial_out(info, UART_LCR, info->LCR);
	schedule();
	info->LCR &= ~UART_LCR_SBC;
	serial_out(info, UART_LCR, info->LCR);
	sti();
}
#else
static void rs_break(struct tty_struct *tty, int break_state)
{
	struct async_struct * info = (struct async_struct *)tty->driver_data;
	unsigned long flags;
	
	if (serial_paranoia_check(info, tty->device, "rs_break"))
		return;

	if (!CONFIGURED_SERIAL_PORT(info))
		return;
	save_flags(flags); cli();
	if (break_state == -1)
		info->LCR |= UART_LCR_SBC;
	else
		info->LCR &= ~UART_LCR_SBC;
	serial_out(info, UART_LCR, info->LCR);
	restore_flags(flags);
}
#endif

#ifdef CONFIG_SERIAL_MULTIPORT
static int get_multiport_struct(struct async_struct * info,
				struct serial_multiport_struct *retinfo)
{
	struct serial_multiport_struct ret;
	struct rs_multiport_struct *multi;
	
	multi = &rs_multiport[info->state->irq];

	ret.port_monitor = multi->port_monitor;
	
	ret.port1 = multi->port1;
	ret.mask1 = multi->mask1;
	ret.match1 = multi->match1;
	
	ret.port2 = multi->port2;
	ret.mask2 = multi->mask2;
	ret.match2 = multi->match2;
	
	ret.port3 = multi->port3;
	ret.mask3 = multi->mask3;
	ret.match3 = multi->match3;
	
	ret.port4 = multi->port4;
	ret.mask4 = multi->mask4;
	ret.match4 = multi->match4;

	ret.irq = info->state->irq;

	if (copy_to_user(retinfo,&ret,sizeof(*retinfo)))
		return -EFAULT;
	return 0;
}

static int set_multiport_struct(struct async_struct * info,
				struct serial_multiport_struct *in_multi)
{
	struct serial_multiport_struct new_multi;
	struct rs_multiport_struct *multi;
	struct serial_state *state;
	int	was_multi, now_multi;
	int	retval;
	void (*handler)(int, void *, struct pt_regs *);

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	state = info->state;
	
	if (copy_from_user(&new_multi, in_multi,
			   sizeof(struct serial_multiport_struct)))
		return -EFAULT;
	
	if (new_multi.irq != state->irq || state->irq == 0 ||
	    !IRQ_ports[state->irq])
		return -EINVAL;

	multi = &rs_multiport[state->irq];
	was_multi = (multi->port1 != 0);
	
	multi->port_monitor = new_multi.port_monitor;
	
	if (multi->port1)
		release_region(multi->port1,1);
	multi->port1 = new_multi.port1;
	multi->mask1 = new_multi.mask1;
	multi->match1 = new_multi.match1;
	if (multi->port1)
		request_region(multi->port1,1,"serial(multiport1)");

	if (multi->port2)
		release_region(multi->port2,1);
	multi->port2 = new_multi.port2;
	multi->mask2 = new_multi.mask2;
	multi->match2 = new_multi.match2;
	if (multi->port2)
		request_region(multi->port2,1,"serial(multiport2)");

	if (multi->port3)
		release_region(multi->port3,1);
	multi->port3 = new_multi.port3;
	multi->mask3 = new_multi.mask3;
	multi->match3 = new_multi.match3;
	if (multi->port3)
		request_region(multi->port3,1,"serial(multiport3)");

	if (multi->port4)
		release_region(multi->port4,1);
	multi->port4 = new_multi.port4;
	multi->mask4 = new_multi.mask4;
	multi->match4 = new_multi.match4;
	if (multi->port4)
		request_region(multi->port4,1,"serial(multiport4)");

	now_multi = (multi->port1 != 0);
	
	if (IRQ_ports[state->irq]->next_port &&
	    (was_multi != now_multi)) {
		free_irq(state->irq, &IRQ_ports[state->irq]);
		if (now_multi)
			handler = rs_interrupt_multi;
		else
			handler = rs_interrupt;

		retval = request_irq(state->irq, handler, SA_SHIRQ,
				     "serial", &IRQ_ports[state->irq]);
		if (retval) {
			printk("Couldn't reallocate serial interrupt "
			       "driver!!\n");
		}
	}
	return 0;
}
#endif

static int rs_ioctl(struct tty_struct *tty, struct file * file,
		    unsigned int cmd, unsigned long arg)
{
	struct async_struct * info = (struct async_struct *)tty->driver_data;
	struct async_icount cprev, cnow;	/* kernel counter temps */
	struct serial_icounter_struct icount;
	unsigned long flags;
#if (LINUX_VERSION_CODE < 131394) /* Linux 2.1.66 */
	int retval, tmp;
#endif
	
	if (serial_paranoia_check(info, tty->device, "rs_ioctl"))
		return -ENODEV;

	if ((cmd != TIOCGSERIAL) && (cmd != TIOCSSERIAL) &&
	    (cmd != TIOCSERCONFIG) && (cmd != TIOCSERGSTRUCT) &&
	    (cmd != TIOCMIWAIT) && (cmd != TIOCGICOUNT)) {
		if (tty->flags & (1 << TTY_IO_ERROR))
		    return -EIO;
	}
	
	switch (cmd) {
#if (LINUX_VERSION_CODE < 131394) /* Linux 2.1.66 */
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
		case TIOCGSOFTCAR:
			tmp = C_CLOCAL(tty) ? 1 : 0;
			if (copy_to_user((void *)arg, &tmp, sizeof(int)))
				return -EFAULT;
			return 0;
		case TIOCSSOFTCAR:
			if (copy_from_user(&tmp, (void *)arg, sizeof(int)))
				return -EFAULT;

			tty->termios->c_cflag =
				((tty->termios->c_cflag & ~CLOCAL) |
				 (tmp ? CLOCAL : 0));
			return 0;
#endif
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
				
#ifdef CONFIG_SERIAL_MULTIPORT
		case TIOCSERGETMULTI:
			return get_multiport_struct(info,
				       (struct serial_multiport_struct *) arg);
		case TIOCSERSETMULTI:
			return set_multiport_struct(info,
				       (struct serial_multiport_struct *) arg);
#endif
			
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
			restore_flags(flags);
			/* Force modem status interrupts on */
			info->IER |= UART_IER_MSI;
			serial_out(info, UART_IER, info->IER);
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
			save_flags(flags); cli();
			cnow = info->state->icount;
			restore_flags(flags);
			icount.cts = cnow.cts;
			icount.dsr = cnow.dsr;
			icount.rng = cnow.rng;
			icount.dcd = cnow.dcd;
			icount.rx = cnow.rx;
			icount.tx = cnow.tx;
			icount.frame = cnow.frame;
			icount.overrun = cnow.overrun;
			icount.parity = cnow.parity;
			icount.brk = cnow.brk;
			icount.buf_overrun = cnow.buf_overrun;
			
			if (copy_to_user((void *)arg, &icount, sizeof(icount)))
				return -EFAULT;
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
	unsigned long flags;
	unsigned int cflag = tty->termios->c_cflag;
	
	if (   (cflag == old_termios->c_cflag)
	    && (   RELEVANT_IFLAG(tty->termios->c_iflag) 
		== RELEVANT_IFLAG(old_termios->c_iflag)))
	  return;

	change_speed(info, old_termios);

	/* Handle transition to B0 status */
	if ((old_termios->c_cflag & CBAUD) &&
	    !(cflag & CBAUD)) {
		info->MCR &= ~(UART_MCR_DTR|UART_MCR_RTS);
		save_flags(flags); cli();
		serial_out(info, UART_MCR, info->MCR);
		restore_flags(flags);
	}
	
	/* Handle transition away from B0 status */
	if (!(old_termios->c_cflag & CBAUD) &&
	    (cflag & CBAUD)) {
		info->MCR |= UART_MCR_DTR;
		if (!(tty->termios->c_cflag & CRTSCTS) || 
		    !test_bit(TTY_THROTTLED, &tty->flags)) {
			info->MCR |= UART_MCR_RTS;
		}
		save_flags(flags); cli();
		serial_out(info, UART_MCR, info->MCR);
		restore_flags(flags);
	}
	
	/* Handle turning off CRTSCTS */
	if ((old_termios->c_cflag & CRTSCTS) &&
	    !(tty->termios->c_cflag & CRTSCTS)) {
		tty->hw_stopped = 0;
		rs_start(tty);
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
	if (info->closing_wait != ASYNC_CLOSING_WAIT_NONE)
		tty_wait_until_sent(tty, info->closing_wait);
	/*
	 * At this point we stop accepting input.  To do this, we
	 * disable the receive line status interrupts, and tell the
	 * interrupt driver to stop checking the data ready bit in the
	 * line status register.
	 */
	info->IER &= ~UART_IER_RLSI;
	info->read_status_mask &= ~UART_LSR_DR;
	if (info->flags & ASYNC_INITIALIZED) {
		serial_out(info, UART_IER, info->IER);
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
			set_current_state(TASK_INTERRUPTIBLE);
			schedule_timeout(info->close_delay);
		}
		wake_up_interruptible(&info->open_wait);
	}
	info->flags &= ~(ASYNC_NORMAL_ACTIVE|ASYNC_CALLOUT_ACTIVE|
			 ASYNC_CLOSING);
	wake_up_interruptible(&info->close_wait);
	MOD_DEC_USE_COUNT;
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
	if (!timeout || timeout > 2*info->timeout)
		timeout = 2*info->timeout;
#ifdef SERIAL_DEBUG_RS_WAIT_UNTIL_SENT
	printk("In rs_wait_until_sent(%d) check=%lu...", timeout, char_time);
	printk("jiff=%lu...", jiffies);
#endif
	while (!((lsr = serial_inp(info, UART_LSR)) & UART_LSR_TEMT)) {
#ifdef SERIAL_DEBUG_RS_WAIT_UNTIL_SENT
		printk("lsr = %d (jiff=%lu)...", lsr, jiffies);
#endif
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(char_time);
		if (signal_pending(current))
			break;
		if (timeout && time_after(jiffies, orig_jiffies + timeout))
			break;
	}
#ifdef SERIAL_DEBUG_RS_WAIT_UNTIL_SENT
	printk("lsr = %d (jiff=%lu)...done\n", lsr, jiffies);
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
	if (info->flags & ASYNC_CLOSING)
		return;
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
	int		do_clocal = 0, extra_count = 0;
	unsigned long	flags;

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
	printk("block_til_ready before block: ttys%d, count = %d\n",
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
		save_flags(flags); cli();
		if (!(info->flags & ASYNC_CALLOUT_ACTIVE) &&
		    (tty->termios->c_cflag & CBAUD))
			serial_out(info, UART_MCR,
				   serial_inp(info, UART_MCR) |
				   (UART_MCR_DTR | UART_MCR_RTS));
		restore_flags(flags);
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
	set_current_state(TASK_RUNNING);
	remove_wait_queue(&info->open_wait, &wait);
	if (extra_count)
		state->count++;
	info->blocked_open--;
#ifdef SERIAL_DEBUG_OPEN
	printk("block_til_ready after blocking: ttys%d, count = %d\n",
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
	info->io_type = sstate->io_type;
	info->iomem_base = sstate->iomem_base;
	info->iomem_reg_shift = sstate->iomem_reg_shift;
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
 *
 * Note that on failure, we don't decrement the module use count - the tty
 * later will call rs_close, which will decrement it for us as long as
 * tty->driver_data is set non-NULL. --rmk
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
	if (serial_paranoia_check(info, tty->device, "rs_open"))
		return -ENODEV;

#ifdef SERIAL_DEBUG_OPEN
	printk("rs_open %s%d, count = %d\n", tty->driver.name, info->line,
	       info->state->count);
#endif
#if (LINUX_VERSION_CODE > 0x20100)
	info->tty->low_latency = (info->flags & ASYNC_LOW_LATENCY) ? 1 : 0;
#endif

	/*
	 *	This relies on lock_kernel() stuff so wants tidying for 2.5
	 */
	if (!tmp_buf) {
		page = get_zeroed_page(GFP_KERNEL);
		if (!page)
			return -ENOMEM;
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
		change_speed(info, 0);
	}
#ifdef CONFIG_SERIAL_CONSOLE
	if (sercons.cflag && sercons.index == line) {
		tty->termios->c_cflag = sercons.cflag;
		sercons.cflag = 0;
		change_speed(info, 0);
	}
#endif
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

static inline int line_info(char *buf, struct serial_state *state)
{
	struct async_struct *info = state->info, scr_info;
	char	stat_buf[30], control, status;
	int	ret;
	unsigned long flags;

	/*
	 * Return zero characters for ports not claimed by driver.
	 */
	if (state->type == PORT_UNKNOWN) {
		return 0;	/* ignore unused ports */
	}

	ret = sprintf(buf, "%d: uart:%s port:%lX irq:%d",
		      state->line, uart_config[state->type].name, 
		      (state->port ? state->port : (long)state->iomem_base),
		      state->irq);

	/*
	 * Figure out the current RS-232 lines
	 */
	if (!info) {
		info = &scr_info;	/* This is just for serial_{in,out} */

		info->magic = SERIAL_MAGIC;
		info->port = state->port;
		info->flags = state->flags;
		info->hub6 = state->hub6;
		info->io_type = state->io_type;
		info->iomem_base = state->iomem_base;
		info->iomem_reg_shift = state->iomem_reg_shift;
		info->quot = 0;
		info->tty = 0;
	}
	save_flags(flags); cli();
	status = serial_in(info, UART_MSR);
	control = info != &scr_info ? info->MCR : serial_in(info, UART_MCR);
	restore_flags(flags); 

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
	return ret;
}

static int rs_read_proc(char *page, char **start, off_t off, int count,
			int *eof, void *data)
{
	int i, len = 0, l;
	off_t	begin = 0;

	len += sprintf(page, "serinfo:1.0 driver:%s%s revision:%s\n",
		       serial_version, LOCAL_VERSTRING, serial_revdate);
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
static char serial_options[] __initdata =
#ifdef CONFIG_HUB6
       " HUB-6"
#define SERIAL_OPT
#endif
#ifdef CONFIG_SERIAL_MANY_PORTS
       " MANY_PORTS"
#define SERIAL_OPT
#endif
#ifdef CONFIG_SERIAL_MULTIPORT
       " MULTIPORT"
#define SERIAL_OPT
#endif
#ifdef CONFIG_SERIAL_SHARE_IRQ
       " SHARE_IRQ"
#define SERIAL_OPT
#endif
#ifdef CONFIG_SERIAL_DETECT_IRQ
       " DETECT_IRQ"
#define SERIAL_OPT
#endif
#ifdef ENABLE_SERIAL_PCI
       " SERIAL_PCI"
#define SERIAL_OPT
#endif
#ifdef ENABLE_SERIAL_PNP
       " ISAPNP"
#define SERIAL_OPT
#endif
#ifdef ENABLE_SERIAL_ACPI
       " SERIAL_ACPI"
#define SERIAL_OPT
#endif
#ifdef SERIAL_OPT
       " enabled\n";
#else
       " no serial options enabled\n";
#endif
#undef SERIAL_OPT

static _INLINE_ void show_serial_version(void)
{
 	printk(KERN_INFO "%s version %s%s (%s) with%s", serial_name,
	       serial_version, LOCAL_VERSTRING, serial_revdate,
	       serial_options);
}

/*
 * This routine detect the IRQ of a serial port by clearing OUT2 when
 * no UART interrupt are requested (IER = 0) (*GPL*). This seems to work at
 * each time, as long as no other device permanently request the IRQ.
 * If no IRQ is detected, or multiple IRQ appear, this function returns 0.
 * The variable "state" and the field "state->port" should not be null.
 */
static unsigned detect_uart_irq (struct serial_state * state)
{
	int irq;
	unsigned long irqs;
	unsigned char save_mcr, save_ier;
	struct async_struct scr_info; /* serial_{in,out} because HUB6 */

#ifdef CONFIG_SERIAL_MANY_PORTS
	unsigned char save_ICP=0; /* no warning */
	unsigned short ICP=0;

	if (state->flags & ASYNC_FOURPORT)  {
		ICP = (state->port & 0xFE0) | 0x01F;
		save_ICP = inb_p(ICP);
		outb_p(0x80, ICP);
		(void) inb_p(ICP);
	}
#endif
	scr_info.magic = SERIAL_MAGIC;
	scr_info.state = state;
	scr_info.port = state->port;
	scr_info.flags = state->flags;
#ifdef CONFIG_HUB6
	scr_info.hub6 = state->hub6;
#endif
	scr_info.io_type = state->io_type;
	scr_info.iomem_base = state->iomem_base;
	scr_info.iomem_reg_shift = state->iomem_reg_shift;

	/* forget possible initially masked and pending IRQ */
	probe_irq_off(probe_irq_on());
	save_mcr = serial_inp(&scr_info, UART_MCR);
	save_ier = serial_inp(&scr_info, UART_IER);
	serial_outp(&scr_info, UART_MCR, UART_MCR_OUT1 | UART_MCR_OUT2);
	
	irqs = probe_irq_on();
	serial_outp(&scr_info, UART_MCR, 0);
	udelay (10);
	if (state->flags & ASYNC_FOURPORT)  {
		serial_outp(&scr_info, UART_MCR,
			    UART_MCR_DTR | UART_MCR_RTS);
	} else {
		serial_outp(&scr_info, UART_MCR,
			    UART_MCR_DTR | UART_MCR_RTS | UART_MCR_OUT2);
	}
	serial_outp(&scr_info, UART_IER, 0x0f);	/* enable all intrs */
	(void)serial_inp(&scr_info, UART_LSR);
	(void)serial_inp(&scr_info, UART_RX);
	(void)serial_inp(&scr_info, UART_IIR);
	(void)serial_inp(&scr_info, UART_MSR);
	serial_outp(&scr_info, UART_TX, 0xFF);
	udelay (20);
	irq = probe_irq_off(irqs);

	serial_outp(&scr_info, UART_MCR, save_mcr);
	serial_outp(&scr_info, UART_IER, save_ier);
#ifdef CONFIG_SERIAL_MANY_PORTS
	if (state->flags & ASYNC_FOURPORT)
		outb_p(save_ICP, ICP);
#endif
	return (irq > 0)? irq : 0;
}

/*
 * This is a quickie test to see how big the FIFO is.
 * It doesn't work at all the time, more's the pity.
 */
static int size_fifo(struct async_struct *info)
{
	unsigned char old_fcr, old_mcr, old_dll, old_dlm;
	int count;

	old_fcr = serial_inp(info, UART_FCR);
	old_mcr = serial_inp(info, UART_MCR);
	serial_outp(info, UART_FCR, UART_FCR_ENABLE_FIFO |
		    UART_FCR_CLEAR_RCVR | UART_FCR_CLEAR_XMIT);
	serial_outp(info, UART_MCR, UART_MCR_LOOP);
	serial_outp(info, UART_LCR, UART_LCR_DLAB);
	old_dll = serial_inp(info, UART_DLL);
	old_dlm = serial_inp(info, UART_DLM);
	serial_outp(info, UART_DLL, 0x01);
	serial_outp(info, UART_DLM, 0x00);
	serial_outp(info, UART_LCR, 0x03);
	for (count = 0; count < 256; count++)
		serial_outp(info, UART_TX, count);
	mdelay(20);
	for (count = 0; (serial_inp(info, UART_LSR) & UART_LSR_DR) &&
	     (count < 256); count++)
		serial_inp(info, UART_RX);
	serial_outp(info, UART_FCR, old_fcr);
	serial_outp(info, UART_MCR, old_mcr);
	serial_outp(info, UART_LCR, UART_LCR_DLAB);
	serial_outp(info, UART_DLL, old_dll);
	serial_outp(info, UART_DLM, old_dlm);

	return count;
}

/*
 * This is a helper routine to autodetect StarTech/Exar/Oxsemi UART's.
 * When this function is called we know it is at least a StarTech
 * 16650 V2, but it might be one of several StarTech UARTs, or one of
 * its clones.  (We treat the broken original StarTech 16650 V1 as a
 * 16550, and why not?  Startech doesn't seem to even acknowledge its
 * existence.)
 * 
 * What evil have men's minds wrought...
 */
static void autoconfig_startech_uarts(struct async_struct *info,
				      struct serial_state *state,
				      unsigned long flags)
{
	unsigned char scratch, scratch2, scratch3, scratch4;

	/*
	 * First we check to see if it's an Oxford Semiconductor UART.
	 *
	 * If we have to do this here because some non-National
	 * Semiconductor clone chips lock up if you try writing to the
	 * LSR register (which serial_icr_read does)
	 */
	if (state->type == PORT_16550A) {
		/*
		 * EFR [4] must be set else this test fails
		 *
		 * This shouldn't be necessary, but Mike Hudson
		 * (Exoray@isys.ca) claims that it's needed for 952
		 * dual UART's (which are not recommended for new designs).
		 */
		info->ACR = 0;
		serial_out(info, UART_LCR, 0xBF);
		serial_out(info, UART_EFR, 0x10);
		serial_out(info, UART_LCR, 0x00);
		/* Check for Oxford Semiconductor 16C950 */
		scratch = serial_icr_read(info, UART_ID1);
		scratch2 = serial_icr_read(info, UART_ID2);
		scratch3 = serial_icr_read(info, UART_ID3);
		
		if (scratch == 0x16 && scratch2 == 0xC9 &&
		    (scratch3 == 0x50 || scratch3 == 0x52 ||
		     scratch3 == 0x54)) {
			state->type = PORT_16C950;
			state->revision = serial_icr_read(info, UART_REV) |
				(scratch3 << 8);
			return;
		}
	}
	
	/*
	 * We check for a XR16C850 by setting DLL and DLM to 0, and
	 * then reading back DLL and DLM.  If DLM reads back 0x10,
	 * then the UART is a XR16C850 and the DLL contains the chip
	 * revision.  If DLM reads back 0x14, then the UART is a
	 * XR16C854.
	 * 
	 */

	/* Save the DLL and DLM */

	serial_outp(info, UART_LCR, UART_LCR_DLAB);
	scratch3 = serial_inp(info, UART_DLL);
	scratch4 = serial_inp(info, UART_DLM);

	serial_outp(info, UART_DLL, 0);
	serial_outp(info, UART_DLM, 0);
	scratch2 = serial_inp(info, UART_DLL);
	scratch = serial_inp(info, UART_DLM);
	serial_outp(info, UART_LCR, 0);

	if (scratch == 0x10 || scratch == 0x14) {
		if (scratch == 0x10)
			state->revision = scratch2;
		state->type = PORT_16850;
		return;
	}

	/* Restore the DLL and DLM */

	serial_outp(info, UART_LCR, UART_LCR_DLAB);
	serial_outp(info, UART_DLL, scratch3);
	serial_outp(info, UART_DLM, scratch4);
	serial_outp(info, UART_LCR, 0);
	/*
	 * We distinguish between the '654 and the '650 by counting
	 * how many bytes are in the FIFO.  I'm using this for now,
	 * since that's the technique that was sent to me in the
	 * serial driver update, but I'm not convinced this works.
	 * I've had problems doing this in the past.  -TYT
	 */
	if (size_fifo(info) == 64)
		state->type = PORT_16654;
	else
		state->type = PORT_16650V2;
}

/*
 * This routine is called by rs_init() to initialize a specific serial
 * port.  It determines what type of UART chip this serial port is
 * using: 8250, 16450, 16550, 16550A.  The important question is
 * whether or not this UART is a 16550A or not, since this will
 * determine whether or not we can use its FIFO features or not.
 */
static void autoconfig(struct serial_state * state)
{
	unsigned char status1, status2, scratch, scratch2, scratch3;
	unsigned char save_lcr, save_mcr;
	struct async_struct *info, scr_info;
	unsigned long flags;

	state->type = PORT_UNKNOWN;

#ifdef SERIAL_DEBUG_AUTOCONF
	printk("Testing ttyS%d (0x%04lx, 0x%04x)...\n", state->line,
	       state->port, (unsigned) state->iomem_base);
#endif
	
	if (!CONFIGURED_SERIAL_PORT(state))
		return;
		
	info = &scr_info;	/* This is just for serial_{in,out} */

	info->magic = SERIAL_MAGIC;
	info->state = state;
	info->port = state->port;
	info->flags = state->flags;
#ifdef CONFIG_HUB6
	info->hub6 = state->hub6;
#endif
	info->io_type = state->io_type;
	info->iomem_base = state->iomem_base;
	info->iomem_reg_shift = state->iomem_reg_shift;

	save_flags(flags); cli();
	
	if (!(state->flags & ASYNC_BUGGY_UART) &&
	    !state->iomem_base) {
		/*
		 * Do a simple existence test first; if we fail this,
		 * there's no point trying anything else.
		 * 
		 * 0x80 is used as a nonsense port to prevent against
		 * false positives due to ISA bus float.  The
		 * assumption is that 0x80 is a non-existent port;
		 * which should be safe since include/asm/io.h also
		 * makes this assumption.
		 */
		scratch = serial_inp(info, UART_IER);
		serial_outp(info, UART_IER, 0);
#ifdef __i386__
		outb(0xff, 0x080);
#endif
		scratch2 = serial_inp(info, UART_IER);
		serial_outp(info, UART_IER, 0x0F);
#ifdef __i386__
		outb(0, 0x080);
#endif
		scratch3 = serial_inp(info, UART_IER);
		serial_outp(info, UART_IER, scratch);
		if (scratch2 || scratch3 != 0x0F) {
#ifdef SERIAL_DEBUG_AUTOCONF
			printk("serial: ttyS%d: simple autoconfig failed "
			       "(%02x, %02x)\n", state->line, 
			       scratch2, scratch3);
#endif
			restore_flags(flags);
			return;		/* We failed; there's nothing here */
		}
	}

	save_mcr = serial_in(info, UART_MCR);
	save_lcr = serial_in(info, UART_LCR);

	/* 
	 * Check to see if a UART is really there.  Certain broken
	 * internal modems based on the Rockwell chipset fail this
	 * test, because they apparently don't implement the loopback
	 * test mode.  So this test is skipped on the COM 1 through
	 * COM 4 ports.  This *should* be safe, since no board
	 * manufacturer would be stupid enough to design a board
	 * that conflicts with COM 1-4 --- we hope!
	 */
	if (!(state->flags & ASYNC_SKIP_TEST)) {
		serial_outp(info, UART_MCR, UART_MCR_LOOP | 0x0A);
		status1 = serial_inp(info, UART_MSR) & 0xF0;
		serial_outp(info, UART_MCR, save_mcr);
		if (status1 != 0x90) {
#ifdef SERIAL_DEBUG_AUTOCONF
			printk("serial: ttyS%d: no UART loopback failed\n",
			       state->line);
#endif
			restore_flags(flags);
			return;
		}
	}
	serial_outp(info, UART_LCR, 0xBF); /* set up for StarTech test */
	serial_outp(info, UART_EFR, 0);	/* EFR is the same as FCR */
	serial_outp(info, UART_LCR, 0);
	serial_outp(info, UART_FCR, UART_FCR_ENABLE_FIFO);
	scratch = serial_in(info, UART_IIR) >> 6;
	switch (scratch) {
		case 0:
			state->type = PORT_16450;
			break;
		case 1:
			state->type = PORT_UNKNOWN;
			break;
		case 2:
			state->type = PORT_16550;
			break;
		case 3:
			state->type = PORT_16550A;
			break;
	}
	if (state->type == PORT_16550A) {
		/* Check for Startech UART's */
		serial_outp(info, UART_LCR, UART_LCR_DLAB);
		if (serial_in(info, UART_EFR) == 0) {
			serial_outp(info, UART_EFR, 0xA8);
			if (serial_in(info, UART_EFR) == 0) {
				/* We are a NS16552D/Motorola
				 * 8xxx DUART, stop. */
				goto out;
			}
			state->type = PORT_16650;
			serial_outp(info, UART_EFR, 0);
		} else {
			serial_outp(info, UART_LCR, 0xBF);
			if (serial_in(info, UART_EFR) == 0)
				autoconfig_startech_uarts(info, state, flags);
		}
	}
	if (state->type == PORT_16550A) {
		/* Check for TI 16750 */
		serial_outp(info, UART_LCR, save_lcr | UART_LCR_DLAB);
		serial_outp(info, UART_FCR,
			    UART_FCR_ENABLE_FIFO | UART_FCR7_64BYTE);
		scratch = serial_in(info, UART_IIR) >> 5;
		if (scratch == 7) {
			/*
			 * If this is a 16750, and not a cheap UART
			 * clone, then it should only go into 64 byte
			 * mode if the UART_FCR7_64BYTE bit was set
			 * while UART_LCR_DLAB was latched.
			 */
 			serial_outp(info, UART_FCR, UART_FCR_ENABLE_FIFO);
			serial_outp(info, UART_LCR, 0);
			serial_outp(info, UART_FCR,
				    UART_FCR_ENABLE_FIFO | UART_FCR7_64BYTE);
			scratch = serial_in(info, UART_IIR) >> 5;
			if (scratch == 6)
				state->type = PORT_16750;
		}
		serial_outp(info, UART_FCR, UART_FCR_ENABLE_FIFO);
	}
#if defined(CONFIG_SERIAL_RSA) && defined(MODULE)
	if (state->type == PORT_16550A) {
		int i;

		for (i = 0 ; i < PORT_RSA_MAX ; ++i) {
			if (!probe_rsa[i] && !force_rsa[i])
				break;
			if (((probe_rsa[i] != state->port) ||
			     check_region(state->port + UART_RSA_BASE, 16)) &&
			    (force_rsa[i] != state->port))
				continue;
			if (!enable_rsa(info))
				continue;
			state->type = PORT_RSA;
			state->baud_base = SERIAL_RSA_BAUD_BASE;
			break;
		}
	}
#endif
out:
	serial_outp(info, UART_LCR, save_lcr);
	if (state->type == PORT_16450) {
		scratch = serial_in(info, UART_SCR);
		serial_outp(info, UART_SCR, 0xa5);
		status1 = serial_in(info, UART_SCR);
		serial_outp(info, UART_SCR, 0x5a);
		status2 = serial_in(info, UART_SCR);
		serial_outp(info, UART_SCR, scratch);

		if ((status1 != 0xa5) || (status2 != 0x5a))
			state->type = PORT_8250;
	}
	state->xmit_fifo_size =	uart_config[state->type].dfl_xmit_fifo_size;

	if (state->type == PORT_UNKNOWN) {
		restore_flags(flags);
		return;
	}

	if (info->port) {
#ifdef CONFIG_SERIAL_RSA
		if (state->type == PORT_RSA)
			request_region(info->port + UART_RSA_BASE, 16,
				       "serial_rsa(auto)");
		else
#endif
			request_region(info->port,8,"serial(auto)");
	}

	/*
	 * Reset the UART.
	 */
#ifdef CONFIG_SERIAL_RSA
	if (state->type == PORT_RSA)
		serial_outp(info, UART_RSA_FRR, 0);
#endif
	serial_outp(info, UART_MCR, save_mcr);
	serial_outp(info, UART_FCR, (UART_FCR_ENABLE_FIFO |
				     UART_FCR_CLEAR_RCVR |
				     UART_FCR_CLEAR_XMIT));
	serial_outp(info, UART_FCR, 0);
	(void)serial_in(info, UART_RX);
	serial_outp(info, UART_IER, 0);
	
	restore_flags(flags);
}

int register_serial(struct serial_struct *req);
void unregister_serial(int line);

#if (LINUX_VERSION_CODE > 0x20100)
EXPORT_SYMBOL(register_serial);
EXPORT_SYMBOL(unregister_serial);
#else
static struct symbol_table serial_syms = {
#include <linux/symtab_begin.h>
	X(register_serial),
	X(unregister_serial),
#include <linux/symtab_end.h>
};
#endif


#if defined(ENABLE_SERIAL_PCI) || defined(ENABLE_SERIAL_PNP) 

static void __devinit printk_pnp_dev_id(unsigned short vendor,
				     unsigned short device)
{
	printk("%c%c%c%x%x%x%x",
	       'A' + ((vendor >> 2) & 0x3f) - 1,
	       'A' + (((vendor & 3) << 3) | ((vendor >> 13) & 7)) - 1,
	       'A' + ((vendor >> 8) & 0x1f) - 1,
	       (device >> 4) & 0x0f,
	       device & 0x0f,
	       (device >> 12) & 0x0f,
	       (device >> 8) & 0x0f);
}

static _INLINE_ int get_pci_port(struct pci_dev *dev,
				  struct pci_board *board,
				  struct serial_struct *req,
				  int idx)
{
	unsigned long port;
	int base_idx;
	int max_port;
	int offset;

	base_idx = SPCI_FL_GET_BASE(board->flags);
	if (board->flags & SPCI_FL_BASE_TABLE)
		base_idx += idx;

	if (board->flags & SPCI_FL_REGION_SZ_CAP) {
		max_port = pci_resource_len(dev, base_idx) / 8;
		if (idx >= max_port)
			return 1;
	}
			
	offset = board->first_uart_offset;

	/* Timedia/SUNIX uses a mixture of BARs and offsets */
	/* Ugh, this is ugly as all hell --- TYT */
	if(dev->vendor == PCI_VENDOR_ID_TIMEDIA )  /* 0x1409 */
		switch(idx) {
			case 0: base_idx=0;
				break;
			case 1: base_idx=0; offset=8;
				break;
			case 2: base_idx=1; 
				break;
			case 3: base_idx=1; offset=8;
				break;
			case 4: /* BAR 2*/
			case 5: /* BAR 3 */
			case 6: /* BAR 4*/
			case 7: base_idx=idx-2; /* BAR 5*/
		}

	/* Some Titan cards are also a little weird */
	if (dev->vendor == PCI_VENDOR_ID_TITAN &&
	    (dev->device == PCI_DEVICE_ID_TITAN_400L ||
	     dev->device == PCI_DEVICE_ID_TITAN_800L)) {
		switch (idx) {
		case 0: base_idx = 1;
			break;
		case 1: base_idx = 2;
			break;
		default:
			base_idx = 4;
			offset = 8 * (idx - 2);
		}
		
	}
  
	/* HP's Diva chip puts the 4th/5th serial port further out, and
	 * some serial ports are supposed to be hidden on certain models.
	 */
	if (dev->vendor == PCI_VENDOR_ID_HP &&
			dev->device == PCI_DEVICE_ID_HP_SAS) {
		switch (dev->subsystem_device) {
		case 0x104B: /* Maestro */
			if (idx == 3) idx++;
			break;
		case 0x1282: /* Everest / Longs Peak */
			if (idx > 0) idx++;
			if (idx > 2) idx++;
			break;
		}
		if (idx > 2) {
			offset = 0x18;
		}
	}

	port =  pci_resource_start(dev, base_idx) + offset;

	if ((board->flags & SPCI_FL_BASE_TABLE) == 0)
		port += idx * (board->uart_offset ? board->uart_offset : 8);

	if (IS_PCI_REGION_IOPORT(dev, base_idx)) {
		req->port = port;
		if (HIGH_BITS_OFFSET)
			req->port_high = port >> HIGH_BITS_OFFSET;
		else
			req->port_high = 0;
		return 0;
	}
	req->io_type = SERIAL_IO_MEM;
	req->iomem_base = ioremap(port, board->uart_offset);
	req->iomem_reg_shift = board->reg_shift;
	req->port = 0;
	return 0;
}

static _INLINE_ int get_pci_irq(struct pci_dev *dev,
				struct pci_board *board,
				int idx)
{
	int base_idx;

	if ((board->flags & SPCI_FL_IRQRESOURCE) == 0)
		return dev->irq;

	base_idx = SPCI_FL_GET_IRQBASE(board->flags);
	if (board->flags & SPCI_FL_IRQ_TABLE)
		base_idx += idx;
	
	return PCI_IRQ_RESOURCE(dev, base_idx);
}

/*
 * Common enabler code shared by both PCI and ISAPNP probes
 */
static void __devinit start_pci_pnp_board(struct pci_dev *dev,
				       struct pci_board *board)
{
	int k, line;
	struct serial_struct serial_req;
	int base_baud;

       if (PREPARE_FUNC(dev) && (PREPARE_FUNC(dev))(dev) < 0) {
	       printk("serial: PNP device '");
	       printk_pnp_dev_id(dev->vendor, dev->device);
	       printk("' prepare failed\n");
	       return;
       }

       if (ACTIVATE_FUNC(dev) && (ACTIVATE_FUNC(dev))(dev) < 0) {
	       printk("serial: PNP device '");
	       printk_pnp_dev_id(dev->vendor, dev->device);
	       printk("' activate failed\n");
	       return;
       }

	/*
	 * Run the initialization function, if any
	 */
	if (board->init_fn && ((board->init_fn)(dev, board, 1) != 0))
		return;

	/*
	 * Register the serial board in the array if we need to
	 * shutdown the board on a module unload or card removal
	 */
	if (DEACTIVATE_FUNC(dev) || board->init_fn) {
		for (k=0; k < NR_PCI_BOARDS; k++)
			if (serial_pci_board[k].dev == 0)
				break;
		if (k >= NR_PCI_BOARDS)
			return;
		serial_pci_board[k].board = *board;
		serial_pci_board[k].dev = dev;
	}

	base_baud = board->base_baud;
	if (!base_baud)
		base_baud = BASE_BAUD;
	memset(&serial_req, 0, sizeof(serial_req));

	for (k=0; k < board->num_ports; k++) {
		serial_req.irq = get_pci_irq(dev, board, k);
		if (get_pci_port(dev, board, &serial_req, k))
			break;
		serial_req.flags = ASYNC_SKIP_TEST | ASYNC_AUTOPROBE;
#ifdef SERIAL_DEBUG_PCI
		printk("Setup PCI/PNP port: port %x, irq %d, type %d\n",
		       serial_req.port, serial_req.irq, serial_req.io_type);
#endif
		line = register_serial(&serial_req);
		if (line < 0)
			break;
		rs_table[line].baud_base = base_baud;
		rs_table[line].dev = dev;
	}
}
#endif	/* ENABLE_SERIAL_PCI || ENABLE_SERIAL_PNP */

#ifdef ENABLE_SERIAL_PCI
/*
 * Some PCI serial cards using the PLX 9050 PCI interface chip require
 * that the card interrupt be explicitly enabled or disabled.  This
 * seems to be mainly needed on card using the PLX which also use I/O
 * mapped memory.
 */
static int __devinit
pci_plx9050_fn(struct pci_dev *dev, struct pci_board *board, int enable)
{
	u8 data, *p, irq_config;
	int pci_config;

	irq_config = 0x41;
	pci_config = PCI_COMMAND_MEMORY;
	if (dev->vendor == PCI_VENDOR_ID_PANACOM)
		irq_config = 0x43;
	if ((dev->vendor == PCI_VENDOR_ID_PLX) &&
	    (dev->device == PCI_DEVICE_ID_PLX_ROMULUS)) {
		/*
		 * As the megawolf cards have the int pins active
		 * high, and have 2 UART chips, both ints must be
		 * enabled on the 9050. Also, the UARTS are set in
		 * 16450 mode by default, so we have to enable the
		 * 16C950 'enhanced' mode so that we can use the deep
		 * FIFOs
		 */
		irq_config = 0x5b;
		pci_config = PCI_COMMAND_MEMORY | PCI_COMMAND_IO;
	}
	
	pci_read_config_byte(dev, PCI_COMMAND, &data);

	if (enable)
		pci_write_config_byte(dev, PCI_COMMAND,
				      data | pci_config);
	
	/* enable/disable interrupts */
	p = ioremap(pci_resource_start(dev, 0), 0x80);
	writel(enable ? irq_config : 0x00, (unsigned long)p + 0x4c);
	iounmap(p);

	if (!enable)
		pci_write_config_byte(dev, PCI_COMMAND,
				      data & ~pci_config);
	return 0;
}


/*
 * SIIG serial cards have an PCI interface chip which also controls
 * the UART clocking frequency. Each UART can be clocked independently
 * (except cards equiped with 4 UARTs) and initial clocking settings
 * are stored in the EEPROM chip. It can cause problems because this
 * version of serial driver doesn't support differently clocked UART's
 * on single PCI card. To prevent this, initialization functions set
 * high frequency clocking for all UART's on given card. It is safe (I
 * hope) because it doesn't touch EEPROM settings to prevent conflicts
 * with other OSes (like M$ DOS).
 *
 *  SIIG support added by Andrey Panin <pazke@mail.tp.ru>, 10/1999
 * 
 * There is two family of SIIG serial cards with different PCI
 * interface chip and different configuration methods:
 *     - 10x cards have control registers in IO and/or memory space;
 *     - 20x cards have control registers in standard PCI configuration space.
 *
 * SIIG initialization functions exported for use by parport_serial.c module.
 */

#define PCI_DEVICE_ID_SIIG_1S_10x (PCI_DEVICE_ID_SIIG_1S_10x_550 & 0xfffc)
#define PCI_DEVICE_ID_SIIG_2S_10x (PCI_DEVICE_ID_SIIG_2S_10x_550 & 0xfff8)

int __devinit
pci_siig10x_fn(struct pci_dev *dev, struct pci_board *board, int enable)
{
       u16 data, *p;

       if (!enable) return 0;

       p = ioremap(pci_resource_start(dev, 0), 0x80);

       switch (dev->device & 0xfff8) {
               case PCI_DEVICE_ID_SIIG_1S_10x:         /* 1S */
                       data = 0xffdf;
                       break;
               case PCI_DEVICE_ID_SIIG_2S_10x:         /* 2S, 2S1P */
                       data = 0xf7ff;
                       break;
               default:                                /* 1S1P, 4S */
                       data = 0xfffb;
                       break;
       }

       writew(readw((unsigned long) p + 0x28) & data, (unsigned long) p + 0x28);
       iounmap(p);
       return 0;
}
EXPORT_SYMBOL(pci_siig10x_fn);

#define PCI_DEVICE_ID_SIIG_2S_20x (PCI_DEVICE_ID_SIIG_2S_20x_550 & 0xfffc)
#define PCI_DEVICE_ID_SIIG_2S1P_20x (PCI_DEVICE_ID_SIIG_2S1P_20x_550 & 0xfffc)

int __devinit
pci_siig20x_fn(struct pci_dev *dev, struct pci_board *board, int enable)
{
       u8 data;

       if (!enable) return 0;

       /* Change clock frequency for the first UART. */
       pci_read_config_byte(dev, 0x6f, &data);
       pci_write_config_byte(dev, 0x6f, data & 0xef);

       /* If this card has 2 UART, we have to do the same with second UART. */
       if (((dev->device & 0xfffc) == PCI_DEVICE_ID_SIIG_2S_20x) ||
           ((dev->device & 0xfffc) == PCI_DEVICE_ID_SIIG_2S1P_20x)) {
               pci_read_config_byte(dev, 0x73, &data);
               pci_write_config_byte(dev, 0x73, data & 0xef);
       }
       return 0;
}
EXPORT_SYMBOL(pci_siig20x_fn);

/* Added for EKF Intel i960 serial boards */
static int __devinit
pci_inteli960ni_fn(struct pci_dev *dev,
		   struct pci_board *board,
		   int enable)
{
	unsigned long oldval;
	
	if (!(pci_get_subdevice(dev) & 0x1000))
		return(-1);

	if (!enable) /* is there something to deinit? */
		return(0);
   
#ifdef SERIAL_DEBUG_PCI
	printk(KERN_DEBUG " Subsystem ID %lx (intel 960)\n",
	       (unsigned long) board->subdevice);
#endif
	/* is firmware started? */
	pci_read_config_dword(dev, 0x44, (void*) &oldval); 
	if (oldval == 0x00001000L) { /* RESET value */ 
		printk(KERN_DEBUG "Local i960 firmware missing");
		return(-1); 
	}
	return(0);
}

/*
 * Timedia has an explosion of boards, and to avoid the PCI table from
 * growing *huge*, we use this function to collapse some 70 entries
 * in the PCI table into one, for sanity's and compactness's sake.
 */
static unsigned short timedia_single_port[] = {
	0x4025, 0x4027, 0x4028, 0x5025, 0x5027, 0 };
static unsigned short timedia_dual_port[] = {
	0x0002, 0x4036, 0x4037, 0x4038, 0x4078, 0x4079, 0x4085,
	0x4088, 0x4089, 0x5037, 0x5078, 0x5079, 0x5085, 0x6079, 
	0x7079, 0x8079, 0x8137, 0x8138, 0x8237, 0x8238, 0x9079, 
	0x9137, 0x9138, 0x9237, 0x9238, 0xA079, 0xB079, 0xC079,
	0xD079, 0 };
static unsigned short timedia_quad_port[] = {
	0x4055, 0x4056, 0x4095, 0x4096, 0x5056, 0x8156, 0x8157, 
	0x8256, 0x8257, 0x9056, 0x9156, 0x9157, 0x9158, 0x9159, 
	0x9256, 0x9257, 0xA056, 0xA157, 0xA158, 0xA159, 0xB056,
	0xB157, 0 };
static unsigned short timedia_eight_port[] = {
	0x4065, 0x4066, 0x5065, 0x5066, 0x8166, 0x9066, 0x9166, 
	0x9167, 0x9168, 0xA066, 0xA167, 0xA168, 0 };
static struct timedia_struct {
	int num;
	unsigned short *ids;
} timedia_data[] = {
	{ 1, timedia_single_port },
	{ 2, timedia_dual_port },
	{ 4, timedia_quad_port },
	{ 8, timedia_eight_port },
	{ 0, 0 }
};

static int __devinit
pci_timedia_fn(struct pci_dev *dev, struct pci_board *board, int enable)
{
	int	i, j;
	unsigned short *ids;

	if (!enable)
		return 0;

	for (i=0; timedia_data[i].num; i++) {
		ids = timedia_data[i].ids;
		for (j=0; ids[j]; j++) {
			if (pci_get_subdevice(dev) == ids[j]) {
				board->num_ports = timedia_data[i].num;
				return 0;
			}
		}
	}
	return 0;
}

/*
 * HP's Remote Management Console.  The Diva chip came in several
 * different versions.  N-class, L2000 and A500 have two Diva chips, each
 * with 3 UARTs (the third UART on the second chip is unused).  Superdome
 * and Keystone have one Diva chip with 3 UARTs.  Some later machines have
 * one Diva chip, but it has been expanded to 5 UARTs.
 */
static int __devinit
pci_hp_diva(struct pci_dev *dev, struct pci_board *board, int enable)
{
	if (!enable)
		return 0;

	switch (dev->subsystem_device) {
	case 0x1049: /* Prelude Diva 1 */
	case 0x1223: /* Superdome */
	case 0x1226: /* Keystone */
	case 0x1282: /* Everest / Longs Peak */
		board->num_ports = 3;
		break;
	case 0x104A: /* Prelude Diva 2 */
		board->num_ports = 2;
		break;
	case 0x104B: /* Maestro */
		board->num_ports = 4;
		break;
	case 0x1227: /* Powerbar */
		board->num_ports = 1;
		break;
	}

	return 0;
}

static int __devinit
pci_xircom_fn(struct pci_dev *dev, struct pci_board *board, int enable)
{
	__set_current_state(TASK_UNINTERRUPTIBLE);
	schedule_timeout(HZ/10);
	return 0;
}

/*
 * This is the configuration table for all of the PCI serial boards
 * which we support.  It is directly indexed by the pci_board_num_t enum
 * value, which is encoded in the pci_device_id PCI probe table's
 * driver_data member.
 */
enum pci_board_num_t {
	pbn_b0_1_115200,
	pbn_default = 0,

	pbn_b0_2_115200,
	pbn_b0_4_115200,

	pbn_b0_1_921600,
	pbn_b0_2_921600,
	pbn_b0_4_921600,

	pbn_b0_bt_1_115200,
	pbn_b0_bt_2_115200,
	pbn_b0_bt_1_460800,
	pbn_b0_bt_2_460800,
	pbn_b0_bt_2_921600,

	pbn_b1_1_115200,
	pbn_b1_2_115200,
	pbn_b1_4_115200,
	pbn_b1_8_115200,

	pbn_b1_2_921600,
	pbn_b1_4_921600,
	pbn_b1_8_921600,

	pbn_b1_2_1382400,
	pbn_b1_4_1382400,
	pbn_b1_8_1382400,

	pbn_b2_1_115200,
	pbn_b2_8_115200,
	pbn_b2_4_460800,
	pbn_b2_8_460800,
	pbn_b2_16_460800,
	pbn_b2_4_921600,
	pbn_b2_8_921600,

	pbn_b2_bt_1_115200,
	pbn_b2_bt_2_115200,
	pbn_b2_bt_4_115200,
	pbn_b2_bt_2_921600,

	pbn_panacom,
	pbn_panacom2,
	pbn_panacom4,
	pbn_plx_romulus,
	pbn_oxsemi,
	pbn_timedia,
	pbn_intel_i960,
	pbn_sgi_ioc3,
	pbn_hp_diva,
#ifdef CONFIG_DDB5074
	pbn_nec_nile4,
#endif

	pbn_dci_pccom4,
	pbn_dci_pccom8,

	pbn_xircom_combo,

	pbn_siig10x_0,
	pbn_siig10x_1,
	pbn_siig10x_2,
	pbn_siig10x_4,
	pbn_siig20x_0,
	pbn_siig20x_2,
	pbn_siig20x_4,
	
	pbn_computone_4,
	pbn_computone_6,
	pbn_computone_8,
};

static struct pci_board pci_boards[] __devinitdata = {
	/*
	 * PCI Flags, Number of Ports, Base (Maximum) Baud Rate,
	 * Offset to get to next UART's registers,
	 * Register shift to use for memory-mapped I/O,
	 * Initialization function, first UART offset
	 */

	/* Generic serial board, pbn_b0_1_115200, pbn_default */
	{ SPCI_FL_BASE0, 1, 115200 },		/* pbn_b0_1_115200,
						   pbn_default */

	{ SPCI_FL_BASE0, 2, 115200 },		/* pbn_b0_2_115200 */
	{ SPCI_FL_BASE0, 4, 115200 },		/* pbn_b0_4_115200 */

	{ SPCI_FL_BASE0, 1, 921600 },		/* pbn_b0_1_921600 */
	{ SPCI_FL_BASE0, 2, 921600 },		/* pbn_b0_2_921600 */
	{ SPCI_FL_BASE0, 4, 921600 },		/* pbn_b0_4_921600 */

	{ SPCI_FL_BASE0 | SPCI_FL_BASE_TABLE, 1, 115200 }, /* pbn_b0_bt_1_115200 */
	{ SPCI_FL_BASE0 | SPCI_FL_BASE_TABLE, 2, 115200 }, /* pbn_b0_bt_2_115200 */
	{ SPCI_FL_BASE0 | SPCI_FL_BASE_TABLE, 1, 460800 }, /* pbn_b0_bt_1_460800 */
	{ SPCI_FL_BASE0 | SPCI_FL_BASE_TABLE, 2, 460800 }, /* pbn_b0_bt_2_460800 */
	{ SPCI_FL_BASE0 | SPCI_FL_BASE_TABLE, 2, 921600 }, /* pbn_b0_bt_2_921600 */

	{ SPCI_FL_BASE1, 1, 115200 },		/* pbn_b1_1_115200 */
	{ SPCI_FL_BASE1, 2, 115200 },		/* pbn_b1_2_115200 */
	{ SPCI_FL_BASE1, 4, 115200 },		/* pbn_b1_4_115200 */
	{ SPCI_FL_BASE1, 8, 115200 },		/* pbn_b1_8_115200 */

	{ SPCI_FL_BASE1, 2, 921600 },		/* pbn_b1_2_921600 */
	{ SPCI_FL_BASE1, 4, 921600 },		/* pbn_b1_4_921600 */
	{ SPCI_FL_BASE1, 8, 921600 },		/* pbn_b1_8_921600 */

	{ SPCI_FL_BASE1, 2, 1382400 },		/* pbn_b1_2_1382400 */
	{ SPCI_FL_BASE1, 4, 1382400 },		/* pbn_b1_4_1382400 */
	{ SPCI_FL_BASE1, 8, 1382400 },		/* pbn_b1_8_1382400 */

	{ SPCI_FL_BASE2, 1, 115200 },		/* pbn_b2_1_115200 */
	{ SPCI_FL_BASE2, 8, 115200 },		/* pbn_b2_8_115200 */
	{ SPCI_FL_BASE2, 4, 460800 },		/* pbn_b2_4_460800 */
	{ SPCI_FL_BASE2, 8, 460800 },		/* pbn_b2_8_460800 */
	{ SPCI_FL_BASE2, 16, 460800 },		/* pbn_b2_16_460800 */
	{ SPCI_FL_BASE2, 4, 921600 },		/* pbn_b2_4_921600 */
	{ SPCI_FL_BASE2, 8, 921600 },		/* pbn_b2_8_921600 */

	{ SPCI_FL_BASE2 | SPCI_FL_BASE_TABLE, 1, 115200 }, /* pbn_b2_bt_1_115200 */
	{ SPCI_FL_BASE2 | SPCI_FL_BASE_TABLE, 2, 115200 }, /* pbn_b2_bt_2_115200 */
	{ SPCI_FL_BASE2 | SPCI_FL_BASE_TABLE, 4, 115200 }, /* pbn_b2_bt_4_115200 */
	{ SPCI_FL_BASE2 | SPCI_FL_BASE_TABLE, 2, 921600 }, /* pbn_b2_bt_2_921600 */

	{ SPCI_FL_BASE2, 2, 921600, /* IOMEM */		   /* pbn_panacom */
		0x400, 7, pci_plx9050_fn },
	{ SPCI_FL_BASE2 | SPCI_FL_BASE_TABLE, 2, 921600,   /* pbn_panacom2 */
		0x400, 7, pci_plx9050_fn },
	{ SPCI_FL_BASE2 | SPCI_FL_BASE_TABLE, 4, 921600,   /* pbn_panacom4 */
		0x400, 7, pci_plx9050_fn },
	{ SPCI_FL_BASE2, 4, 921600,			   /* pbn_plx_romulus */
		0x20, 2, pci_plx9050_fn, 0x03 },
		/* This board uses the size of PCI Base region 0 to
		 * signal now many ports are available */
	{ SPCI_FL_BASE0 | SPCI_FL_REGION_SZ_CAP, 32, 115200 }, /* pbn_oxsemi */
	{ SPCI_FL_BASE_TABLE, 1, 921600,		   /* pbn_timedia */
		0, 0, pci_timedia_fn },
	/* EKF addition for i960 Boards form EKF with serial port */
	{ SPCI_FL_BASE0, 32, 921600, /* max 256 ports */   /* pbn_intel_i960 */
		8<<2, 2, pci_inteli960ni_fn, 0x10000},
	{ SPCI_FL_BASE0 | SPCI_FL_IRQRESOURCE,		   /* pbn_sgi_ioc3 */
		1, 458333, 0, 0, 0, 0x20178 },
	{ SPCI_FL_BASE0, 5, 115200, 8, 0, pci_hp_diva, 0},   /* pbn_hp_diva */
#ifdef CONFIG_DDB5074
	/*
	 * NEC Vrc-5074 (Nile 4) builtin UART.
	 * Conditionally compiled in since this is a motherboard device.
	 */
	{ SPCI_FL_BASE0, 1, 520833,			   /* pbn_nec_nile4 */
		64, 3, NULL, 0x300 },
#endif

	{SPCI_FL_BASE3, 4, 115200, 8},			   /* pbn_dci_pccom4 */
	{SPCI_FL_BASE3, 8, 115200, 8},			   /* pbn_dci_pccom8 */

	{ SPCI_FL_BASE0, 1, 115200,			  /* pbn_xircom_combo */
		0, 0, pci_xircom_fn },

	{ SPCI_FL_BASE2, 1, 460800,			   /* pbn_siig10x_0 */
		0, 0, pci_siig10x_fn },
	{ SPCI_FL_BASE2, 1, 921600,			   /* pbn_siig10x_1 */
		0, 0, pci_siig10x_fn },
	{ SPCI_FL_BASE2 | SPCI_FL_BASE_TABLE, 2, 921600,   /* pbn_siig10x_2 */
		0, 0, pci_siig10x_fn },
	{ SPCI_FL_BASE2 | SPCI_FL_BASE_TABLE, 4, 921600,   /* pbn_siig10x_4 */
		0, 0, pci_siig10x_fn },
	{ SPCI_FL_BASE0, 1, 921600,			   /* pbn_siix20x_0 */
		0, 0, pci_siig20x_fn },
	{ SPCI_FL_BASE0 | SPCI_FL_BASE_TABLE, 2, 921600,   /* pbn_siix20x_2 */
		0, 0, pci_siig20x_fn },
	{ SPCI_FL_BASE0 | SPCI_FL_BASE_TABLE, 4, 921600,   /* pbn_siix20x_4 */
		0, 0, pci_siig20x_fn },

	{ SPCI_FL_BASE0, 4, 921600, /* IOMEM */		   /* pbn_computone_4 */
		0x40, 2, NULL, 0x200 },
	{ SPCI_FL_BASE0, 6, 921600, /* IOMEM */		   /* pbn_computone_6 */
		0x40, 2, NULL, 0x200 },
	{ SPCI_FL_BASE0, 8, 921600, /* IOMEM */		   /* pbn_computone_8 */
		0x40, 2, NULL, 0x200 },
};

/*
 * Given a complete unknown PCI device, try to use some heuristics to
 * guess what the configuration might be, based on the pitiful PCI
 * serial specs.  Returns 0 on success, 1 on failure.
 */
static int __devinit serial_pci_guess_board(struct pci_dev *dev,
					   struct pci_board *board)
{
	int	num_iomem = 0, num_port = 0, first_port = -1;
	int	i;
	
	/*
	 * If it is not a communications device or the programming
	 * interface is greater than 6, give up.
	 *
	 * (Should we try to make guesses for multiport serial devices
	 * later?) 
	 */
	if ((((dev->class >> 8) != PCI_CLASS_COMMUNICATION_SERIAL) &&
	    ((dev->class >> 8) != PCI_CLASS_COMMUNICATION_MODEM)) ||
	    (dev->class & 0xff) > 6)
		return 1;

	for (i=0; i < 6; i++) {
		if (IS_PCI_REGION_IOPORT(dev, i)) {
			num_port++;
			if (first_port == -1)
				first_port = i;
		}
		if (IS_PCI_REGION_IOMEM(dev, i))
			num_iomem++;
	}

	/*
	 * If there is 1 or 0 iomem regions, and exactly one port, use
	 * it.
	 */
	if (num_iomem <= 1 && num_port == 1) {
		board->flags = first_port;
		return 0;
	}
	return 1;
}

static int __devinit serial_init_one(struct pci_dev *dev,
				     const struct pci_device_id *ent)
{
	struct pci_board *board, tmp;
	int rc;

	board = &pci_boards[ent->driver_data];

	rc = pci_enable_device(dev);
	if (rc) return rc;

	if (ent->driver_data == pbn_default &&
	    serial_pci_guess_board(dev, board))
		return -ENODEV;
	else if (serial_pci_guess_board(dev, &tmp) == 0) {
		printk(KERN_INFO "Redundant entry in serial pci_table.  "
		       "Please send the output of\n"
		       "lspci -vv, this message (%04x,%04x,%04x,%04x)\n"
		       "and the manufacturer and name of "
		       "serial board or modem board\n"
		       "to serial-pci-info@lists.sourceforge.net.\n",
		       dev->vendor, dev->device,
		       pci_get_subvendor(dev), pci_get_subdevice(dev));
	}
		       
	start_pci_pnp_board(dev, board);

	return 0;
}

static void __devexit serial_remove_one(struct pci_dev *dev)
{
	int	i;

	/*
	 * Iterate through all of the ports finding those that belong
	 * to this PCI device.
	 */
	for(i = 0; i < NR_PORTS; i++) {
		if (rs_table[i].dev != dev)
			continue;
		unregister_serial(i);
		rs_table[i].dev = 0;
	}
	/*
	 * Now execute any board-specific shutdown procedure
	 */
	for (i=0; i < NR_PCI_BOARDS; i++) {
		struct pci_board_inst *brd = &serial_pci_board[i];

		if (serial_pci_board[i].dev != dev)
			continue;
		if (brd->board.init_fn)
			(brd->board.init_fn)(brd->dev, &brd->board, 0);
		if (DEACTIVATE_FUNC(brd->dev))
			(DEACTIVATE_FUNC(brd->dev))(brd->dev);
		serial_pci_board[i].dev = 0;
	}
}


static struct pci_device_id serial_pci_tbl[] __devinitdata = {
	{	PCI_VENDOR_ID_V3, PCI_DEVICE_ID_V3_V960,
		PCI_SUBVENDOR_ID_CONNECT_TECH,
		PCI_SUBDEVICE_ID_CONNECT_TECH_BH8_232, 0, 0,
		pbn_b1_8_1382400 },
	{	PCI_VENDOR_ID_V3, PCI_DEVICE_ID_V3_V960,
		PCI_SUBVENDOR_ID_CONNECT_TECH,
		PCI_SUBDEVICE_ID_CONNECT_TECH_BH4_232, 0, 0,
		pbn_b1_4_1382400 },
	{	PCI_VENDOR_ID_V3, PCI_DEVICE_ID_V3_V960,
		PCI_SUBVENDOR_ID_CONNECT_TECH,
		PCI_SUBDEVICE_ID_CONNECT_TECH_BH2_232, 0, 0,
		pbn_b1_2_1382400 },
	{	PCI_VENDOR_ID_V3, PCI_DEVICE_ID_V3_V351,
		PCI_SUBVENDOR_ID_CONNECT_TECH,
		PCI_SUBDEVICE_ID_CONNECT_TECH_BH8_232, 0, 0,
		pbn_b1_8_1382400 },
	{	PCI_VENDOR_ID_V3, PCI_DEVICE_ID_V3_V351,
		PCI_SUBVENDOR_ID_CONNECT_TECH,
		PCI_SUBDEVICE_ID_CONNECT_TECH_BH4_232, 0, 0,
		pbn_b1_4_1382400 },
	{	PCI_VENDOR_ID_V3, PCI_DEVICE_ID_V3_V351,
		PCI_SUBVENDOR_ID_CONNECT_TECH,
		PCI_SUBDEVICE_ID_CONNECT_TECH_BH2_232, 0, 0,
		pbn_b1_2_1382400 },
	{	PCI_VENDOR_ID_V3, PCI_DEVICE_ID_V3_V351,
		PCI_SUBVENDOR_ID_CONNECT_TECH,
		PCI_SUBDEVICE_ID_CONNECT_TECH_BH8_485, 0, 0,
		pbn_b1_8_921600 },
	{	PCI_VENDOR_ID_V3, PCI_DEVICE_ID_V3_V351,
		PCI_SUBVENDOR_ID_CONNECT_TECH,
		PCI_SUBDEVICE_ID_CONNECT_TECH_BH8_485_4_4, 0, 0,
		pbn_b1_8_921600 },
	{	PCI_VENDOR_ID_V3, PCI_DEVICE_ID_V3_V351,
		PCI_SUBVENDOR_ID_CONNECT_TECH,
		PCI_SUBDEVICE_ID_CONNECT_TECH_BH4_485, 0, 0,
		pbn_b1_4_921600 },
	{	PCI_VENDOR_ID_V3, PCI_DEVICE_ID_V3_V351,
		PCI_SUBVENDOR_ID_CONNECT_TECH,
		PCI_SUBDEVICE_ID_CONNECT_TECH_BH4_485_2_2, 0, 0,
		pbn_b1_4_921600 },
	{	PCI_VENDOR_ID_V3, PCI_DEVICE_ID_V3_V351,
		PCI_SUBVENDOR_ID_CONNECT_TECH,
		PCI_SUBDEVICE_ID_CONNECT_TECH_BH2_485, 0, 0,
		pbn_b1_2_921600 },
	{	PCI_VENDOR_ID_V3, PCI_DEVICE_ID_V3_V351,
		PCI_SUBVENDOR_ID_CONNECT_TECH,
		PCI_SUBDEVICE_ID_CONNECT_TECH_BH8_485_2_6, 0, 0,
		pbn_b1_8_921600 },
	{	PCI_VENDOR_ID_V3, PCI_DEVICE_ID_V3_V351,
		PCI_SUBVENDOR_ID_CONNECT_TECH,
		PCI_SUBDEVICE_ID_CONNECT_TECH_BH081101V1, 0, 0,
		pbn_b1_8_921600 },
	{	PCI_VENDOR_ID_V3, PCI_DEVICE_ID_V3_V351,
		PCI_SUBVENDOR_ID_CONNECT_TECH,
		PCI_SUBDEVICE_ID_CONNECT_TECH_BH041101V1, 0, 0,
		pbn_b1_4_921600 },

	{	PCI_VENDOR_ID_SEALEVEL, PCI_DEVICE_ID_SEALEVEL_U530,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0, 
		pbn_b2_bt_1_115200 },
	{	PCI_VENDOR_ID_SEALEVEL, PCI_DEVICE_ID_SEALEVEL_UCOMM2,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0, 
		pbn_b2_bt_2_115200 },
	{	PCI_VENDOR_ID_SEALEVEL, PCI_DEVICE_ID_SEALEVEL_UCOMM422,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0, 
		pbn_b2_bt_4_115200 },
	{	PCI_VENDOR_ID_SEALEVEL, PCI_DEVICE_ID_SEALEVEL_UCOMM232,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0, 
		pbn_b2_bt_2_115200 },
	{	PCI_VENDOR_ID_SEALEVEL, PCI_DEVICE_ID_SEALEVEL_COMM4,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0, 
		pbn_b2_bt_4_115200 },
	{	PCI_VENDOR_ID_SEALEVEL, PCI_DEVICE_ID_SEALEVEL_COMM8,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0, 
		pbn_b2_8_115200 },

	{	PCI_VENDOR_ID_PLX, PCI_DEVICE_ID_PLX_GTEK_SERIAL2,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b2_bt_2_115200 },
	{	PCI_VENDOR_ID_PLX, PCI_DEVICE_ID_PLX_SPCOM200,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b2_bt_2_921600 },
	/* VScom SPCOM800, from sl@s.pl */
	{	PCI_VENDOR_ID_PLX, PCI_DEVICE_ID_PLX_SPCOM800, 
		PCI_ANY_ID, PCI_ANY_ID, 0, 0, 
		pbn_b2_8_921600 },
	{	PCI_VENDOR_ID_PLX, PCI_DEVICE_ID_PLX_1077,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0, 
		pbn_b2_4_921600 },
	{	PCI_VENDOR_ID_PLX, PCI_DEVICE_ID_PLX_9050,
		PCI_SUBVENDOR_ID_KEYSPAN,
		PCI_SUBDEVICE_ID_KEYSPAN_SX2, 0, 0,
		pbn_panacom },
	{	PCI_VENDOR_ID_PANACOM, PCI_DEVICE_ID_PANACOM_QUADMODEM,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_panacom4 },
	{	PCI_VENDOR_ID_PANACOM, PCI_DEVICE_ID_PANACOM_DUALMODEM,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_panacom2 },
	{	PCI_VENDOR_ID_PLX, PCI_DEVICE_ID_PLX_9050,
		PCI_SUBVENDOR_ID_CHASE_PCIFAST,
		PCI_SUBDEVICE_ID_CHASE_PCIFAST4, 0, 0, 
		pbn_b2_4_460800 },
	{	PCI_VENDOR_ID_PLX, PCI_DEVICE_ID_PLX_9050,
		PCI_SUBVENDOR_ID_CHASE_PCIFAST,
		PCI_SUBDEVICE_ID_CHASE_PCIFAST8, 0, 0, 
		pbn_b2_8_460800 },
	{	PCI_VENDOR_ID_PLX, PCI_DEVICE_ID_PLX_9050,
		PCI_SUBVENDOR_ID_CHASE_PCIFAST,
		PCI_SUBDEVICE_ID_CHASE_PCIFAST16, 0, 0, 
		pbn_b2_16_460800 },
	{	PCI_VENDOR_ID_PLX, PCI_DEVICE_ID_PLX_9050,
		PCI_SUBVENDOR_ID_CHASE_PCIFAST,
		PCI_SUBDEVICE_ID_CHASE_PCIFAST16FMC, 0, 0, 
		pbn_b2_16_460800 },
	{	PCI_VENDOR_ID_PLX, PCI_DEVICE_ID_PLX_9050,
		PCI_SUBVENDOR_ID_CHASE_PCIRAS,
		PCI_SUBDEVICE_ID_CHASE_PCIRAS4, 0, 0, 
		pbn_b2_4_460800 },
	{	PCI_VENDOR_ID_PLX, PCI_DEVICE_ID_PLX_9050,
		PCI_SUBVENDOR_ID_CHASE_PCIRAS,
		PCI_SUBDEVICE_ID_CHASE_PCIRAS8, 0, 0, 
		pbn_b2_8_460800 },
	/* Megawolf Romulus PCI Serial Card, from Mike Hudson */
	/* (Exoray@isys.ca) */
	{	PCI_VENDOR_ID_PLX, PCI_DEVICE_ID_PLX_ROMULUS,
		0x10b5, 0x106a, 0, 0,
		pbn_plx_romulus },
	{	PCI_VENDOR_ID_QUATECH, PCI_DEVICE_ID_QUATECH_QSC100,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0, 
		pbn_b1_4_115200 },
	{	PCI_VENDOR_ID_QUATECH, PCI_DEVICE_ID_QUATECH_DSC100,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0, 
		pbn_b1_2_115200 },
	{	PCI_VENDOR_ID_QUATECH, PCI_DEVICE_ID_QUATECH_ESC100D,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0, 
		pbn_b1_8_115200 },
	{	PCI_VENDOR_ID_QUATECH, PCI_DEVICE_ID_QUATECH_ESC100M,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0, 
		pbn_b1_8_115200 },
	{	PCI_VENDOR_ID_SPECIALIX, PCI_DEVICE_ID_OXSEMI_16PCI954,
		PCI_VENDOR_ID_SPECIALIX, PCI_SUBDEVICE_ID_SPECIALIX_SPEED4, 0, 0, 
		pbn_b0_4_921600 },
	{	PCI_VENDOR_ID_OXSEMI, PCI_DEVICE_ID_OXSEMI_16PCI954,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0, 
		pbn_b0_4_115200 },
	{	PCI_VENDOR_ID_OXSEMI, PCI_DEVICE_ID_OXSEMI_16PCI952,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0, 
		pbn_b0_bt_2_921600 },

	/* Digitan DS560-558, from jimd@esoft.com */
	{	PCI_VENDOR_ID_ATT, PCI_DEVICE_ID_ATT_VENUS_MODEM,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0, 
		pbn_b1_1_115200 },

	/* 3Com US Robotics 56k Voice Internal PCI model 5610 */
	{	PCI_VENDOR_ID_USR, 0x1008,
		PCI_ANY_ID, PCI_ANY_ID, },

	/* Titan Electronic cards */
	{	PCI_VENDOR_ID_TITAN, PCI_DEVICE_ID_TITAN_100,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0, 
		pbn_b0_1_921600 },
	{	PCI_VENDOR_ID_TITAN, PCI_DEVICE_ID_TITAN_200,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0, 
		pbn_b0_2_921600 },
	{	PCI_VENDOR_ID_TITAN, PCI_DEVICE_ID_TITAN_400,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0, 
		pbn_b0_4_921600 },
	{	PCI_VENDOR_ID_TITAN, PCI_DEVICE_ID_TITAN_800B,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0, 
		pbn_b0_4_921600 },
	{	PCI_VENDOR_ID_TITAN, PCI_DEVICE_ID_TITAN_100L,
		PCI_ANY_ID, PCI_ANY_ID,
		SPCI_FL_BASE1, 1, 921600 },
	{	PCI_VENDOR_ID_TITAN, PCI_DEVICE_ID_TITAN_200L,
		PCI_ANY_ID, PCI_ANY_ID,
		SPCI_FL_BASE1 | SPCI_FL_BASE_TABLE, 2, 921600 },
	/* The 400L and 800L have a custom hack in get_pci_port */
	{	PCI_VENDOR_ID_TITAN, PCI_DEVICE_ID_TITAN_400L,
		PCI_ANY_ID, PCI_ANY_ID,
		SPCI_FL_BASE_TABLE, 4, 921600 },
	{	PCI_VENDOR_ID_TITAN, PCI_DEVICE_ID_TITAN_800L,
		PCI_ANY_ID, PCI_ANY_ID,
		SPCI_FL_BASE_TABLE, 8, 921600 },

	{	PCI_VENDOR_ID_SIIG, PCI_DEVICE_ID_SIIG_1S_10x_550,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_siig10x_0 },
	{	PCI_VENDOR_ID_SIIG, PCI_DEVICE_ID_SIIG_1S_10x_650,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_siig10x_0 },
	{	PCI_VENDOR_ID_SIIG, PCI_DEVICE_ID_SIIG_1S_10x_850,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_siig10x_0 },
	{	PCI_VENDOR_ID_SIIG, PCI_DEVICE_ID_SIIG_2S_10x_550,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_siig10x_2 },
	{	PCI_VENDOR_ID_SIIG, PCI_DEVICE_ID_SIIG_2S_10x_650,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_siig10x_2 },
	{	PCI_VENDOR_ID_SIIG, PCI_DEVICE_ID_SIIG_2S_10x_850,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_siig10x_2 },
	{	PCI_VENDOR_ID_SIIG, PCI_DEVICE_ID_SIIG_4S_10x_550,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_siig10x_4 },
	{	PCI_VENDOR_ID_SIIG, PCI_DEVICE_ID_SIIG_4S_10x_650,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_siig10x_4 },
	{	PCI_VENDOR_ID_SIIG, PCI_DEVICE_ID_SIIG_4S_10x_850,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_siig10x_4 },
	{	PCI_VENDOR_ID_SIIG, PCI_DEVICE_ID_SIIG_1S_20x_550,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_siig20x_0 },
	{	PCI_VENDOR_ID_SIIG, PCI_DEVICE_ID_SIIG_1S_20x_650,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_siig20x_0 },
	{	PCI_VENDOR_ID_SIIG, PCI_DEVICE_ID_SIIG_1S_20x_850,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_siig20x_0 },
	{	PCI_VENDOR_ID_SIIG, PCI_DEVICE_ID_SIIG_2S_20x_550,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_siig20x_2 },
	{	PCI_VENDOR_ID_SIIG, PCI_DEVICE_ID_SIIG_2S_20x_650,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_siig20x_2 },
	{	PCI_VENDOR_ID_SIIG, PCI_DEVICE_ID_SIIG_2S_20x_850,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_siig20x_2 },
	{	PCI_VENDOR_ID_SIIG, PCI_DEVICE_ID_SIIG_4S_20x_550,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_siig20x_4 },
	{	PCI_VENDOR_ID_SIIG, PCI_DEVICE_ID_SIIG_4S_20x_650,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_siig20x_4 },
	{	PCI_VENDOR_ID_SIIG, PCI_DEVICE_ID_SIIG_4S_20x_850,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_siig20x_4 },

	/* Computone devices submitted by Doug McNash dmcnash@computone.com */
	{	PCI_VENDOR_ID_COMPUTONE, PCI_DEVICE_ID_COMPUTONE_PG,
		PCI_SUBVENDOR_ID_COMPUTONE, PCI_SUBDEVICE_ID_COMPUTONE_PG4,
		0, 0, pbn_computone_4 },
	{	PCI_VENDOR_ID_COMPUTONE, PCI_DEVICE_ID_COMPUTONE_PG,
		PCI_SUBVENDOR_ID_COMPUTONE, PCI_SUBDEVICE_ID_COMPUTONE_PG8,
		0, 0, pbn_computone_8 },
	{	PCI_VENDOR_ID_COMPUTONE, PCI_DEVICE_ID_COMPUTONE_PG,
		PCI_SUBVENDOR_ID_COMPUTONE, PCI_SUBDEVICE_ID_COMPUTONE_PG6,
		0, 0, pbn_computone_6 },

	{	PCI_VENDOR_ID_OXSEMI, PCI_DEVICE_ID_OXSEMI_16PCI95N,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0, pbn_oxsemi },
	{	PCI_VENDOR_ID_TIMEDIA, PCI_DEVICE_ID_TIMEDIA_1889,
		PCI_VENDOR_ID_TIMEDIA, PCI_ANY_ID, 0, 0, pbn_timedia },

	{	PCI_VENDOR_ID_LAVA, PCI_DEVICE_ID_LAVA_DSERIAL,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_2_115200 },
	{	PCI_VENDOR_ID_LAVA, PCI_DEVICE_ID_LAVA_QUATRO_A,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_2_115200 },
	{	PCI_VENDOR_ID_LAVA, PCI_DEVICE_ID_LAVA_QUATRO_B,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_2_115200 },
	{	PCI_VENDOR_ID_LAVA, PCI_DEVICE_ID_LAVA_PORT_PLUS,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_2_460800 },
	{	PCI_VENDOR_ID_LAVA, PCI_DEVICE_ID_LAVA_QUAD_A,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_2_460800 },
	{	PCI_VENDOR_ID_LAVA, PCI_DEVICE_ID_LAVA_QUAD_B,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_2_460800 },
	{	PCI_VENDOR_ID_LAVA, PCI_DEVICE_ID_LAVA_SSERIAL,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_1_115200 },
	{	PCI_VENDOR_ID_LAVA, PCI_DEVICE_ID_LAVA_PORT_650,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_1_460800 },

	/* RAStel 2 port modem, gerg@moreton.com.au */
	{	PCI_VENDOR_ID_MORETON, PCI_DEVICE_ID_RASTEL_2PORT,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b2_bt_2_115200 },

	/* EKF addition for i960 Boards form EKF with serial port */
	{	PCI_VENDOR_ID_INTEL, 0x1960,
		0xE4BF, PCI_ANY_ID, 0, 0,
		pbn_intel_i960 },

	/* Xircom Cardbus/Ethernet combos */
	{	PCI_VENDOR_ID_XIRCOM, PCI_DEVICE_ID_XIRCOM_X3201_MDM,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_xircom_combo },

	/*
	 * Untested PCI modems, sent in from various folks...
	 */

	/* Elsa Model 56K PCI Modem, from Andreas Rath <arh@01019freenet.de> */
	{	PCI_VENDOR_ID_ROCKWELL, 0x1004,
		0x1048, 0x1500, 0, 0,
		pbn_b1_1_115200 },

	{	PCI_VENDOR_ID_SGI, PCI_DEVICE_ID_SGI_IOC3,
		0xFF00, 0, 0, 0,
		pbn_sgi_ioc3 },

	/* HP Diva card */
	{	PCI_VENDOR_ID_HP, PCI_DEVICE_ID_HP_SAS,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_hp_diva },
	{	PCI_VENDOR_ID_HP, 0x1290,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b2_1_115200 },

#ifdef CONFIG_DDB5074
	/*
	 * NEC Vrc-5074 (Nile 4) builtin UART.
	 * Conditionally compiled in since this is a motherboard device.
	 */
	{	PCI_VENDOR_ID_NEC, PCI_DEVICE_ID_NEC_NILE4,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_nec_nile4 },
#endif

	{	PCI_VENDOR_ID_DCI, PCI_DEVICE_ID_DCI_PCCOM4,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_dci_pccom4 },
	{	PCI_VENDOR_ID_DCI, PCI_DEVICE_ID_DCI_PCCOM8,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_dci_pccom8 },

       { PCI_ANY_ID, PCI_ANY_ID, PCI_ANY_ID, PCI_ANY_ID,
	 PCI_CLASS_COMMUNICATION_SERIAL << 8, 0xffff00, },
       { PCI_ANY_ID, PCI_ANY_ID, PCI_ANY_ID, PCI_ANY_ID,
	 PCI_CLASS_COMMUNICATION_MODEM << 8, 0xffff00, },
       { 0, }
};

MODULE_DEVICE_TABLE(pci, serial_pci_tbl);

static struct pci_driver serial_pci_driver = {
       name:           "serial",
       probe:          serial_init_one,
       remove:	       __devexit_p(serial_remove_one),
       id_table:       serial_pci_tbl,
};


/*
 * Query PCI space for known serial boards
 * If found, add them to the PCI device space in rs_table[]
 *
 * Accept a maximum of eight boards
 *
 */
static void __devinit probe_serial_pci(void) 
{
#ifdef SERIAL_DEBUG_PCI
	printk(KERN_DEBUG "Entered probe_serial_pci()\n");
#endif

	/* Register call PCI serial devices.  Null out
	 * the driver name upon failure, as a signal
	 * not to attempt to unregister the driver later
	 */
	if (pci_module_init (&serial_pci_driver) != 0)
		serial_pci_driver.name = "";

#ifdef SERIAL_DEBUG_PCI
	printk(KERN_DEBUG "Leaving probe_serial_pci() (probe finished)\n");
#endif
	return;
}

#endif /* ENABLE_SERIAL_PCI */

#ifdef ENABLE_SERIAL_PNP

struct pnp_board {
	unsigned short vendor;
	unsigned short device;
};

static struct pnp_board pnp_devices[] __devinitdata = {
	/* Archtek America Corp. */
	/* Archtek SmartLink Modem 3334BT Plug & Play */
	{	ISAPNP_VENDOR('A', 'A', 'C'), ISAPNP_DEVICE(0x000F) },
	/* Anchor Datacomm BV */
	/* SXPro 144 External Data Fax Modem Plug & Play */
	{	ISAPNP_VENDOR('A', 'D', 'C'), ISAPNP_DEVICE(0x0001) },
	/* SXPro 288 External Data Fax Modem Plug & Play */
	{	ISAPNP_VENDOR('A', 'D', 'C'), ISAPNP_DEVICE(0x0002) },
	/* Rockwell 56K ACF II Fax+Data+Voice Modem */
	{	ISAPNP_VENDOR('A', 'K', 'Y'), ISAPNP_DEVICE(0x1021) },
	/* AZT3005 PnP SOUND DEVICE */
	{	ISAPNP_VENDOR('A', 'Z', 'T'), ISAPNP_DEVICE(0x4001) },
	/* Best Data Products Inc. Smart One 336F PnP Modem */
	{	ISAPNP_VENDOR('B', 'D', 'P'), ISAPNP_DEVICE(0x3336) },
	/*  Boca Research */
	/* Boca Complete Ofc Communicator 14.4 Data-FAX */
	{	ISAPNP_VENDOR('B', 'R', 'I'), ISAPNP_DEVICE(0x0A49) },
	/* Boca Research 33,600 ACF Modem */
	{	ISAPNP_VENDOR('B', 'R', 'I'), ISAPNP_DEVICE(0x1400) },
	/* Boca 33.6 Kbps Internal FD34FSVD */
	{	ISAPNP_VENDOR('B', 'R', 'I'), ISAPNP_DEVICE(0x3400) },
	/* Boca 33.6 Kbps Internal FD34FSVD */
	{	ISAPNP_VENDOR('B', 'R', 'I'), ISAPNP_DEVICE(0x0A49) },
	/* Best Data Products Inc. Smart One 336F PnP Modem */
	{	ISAPNP_VENDOR('B', 'D', 'P'), ISAPNP_DEVICE(0x3336) },
	/* Computer Peripherals Inc */
	/* EuroViVa CommCenter-33.6 SP PnP */
	{	ISAPNP_VENDOR('C', 'P', 'I'), ISAPNP_DEVICE(0x4050) },
	/* Creative Labs */
	/* Creative Labs Phone Blaster 28.8 DSVD PnP Voice */
	{	ISAPNP_VENDOR('C', 'T', 'L'), ISAPNP_DEVICE(0x3001) },
	/* Creative Labs Modem Blaster 28.8 DSVD PnP Voice */
	{	ISAPNP_VENDOR('C', 'T', 'L'), ISAPNP_DEVICE(0x3011) },
	/* Creative */
	/* Creative Modem Blaster Flash56 DI5601-1 */
	{	ISAPNP_VENDOR('D', 'M', 'B'), ISAPNP_DEVICE(0x1032) },
	/* Creative Modem Blaster V.90 DI5660 */
	{	ISAPNP_VENDOR('D', 'M', 'B'), ISAPNP_DEVICE(0x2001) },
	/* FUJITSU */
	/* Fujitsu 33600 PnP-I2 R Plug & Play */
	{	ISAPNP_VENDOR('F', 'U', 'J'), ISAPNP_DEVICE(0x0202) },
	/* Fujitsu FMV-FX431 Plug & Play */
	{	ISAPNP_VENDOR('F', 'U', 'J'), ISAPNP_DEVICE(0x0205) },
	/* Fujitsu 33600 PnP-I4 R Plug & Play */
	{	ISAPNP_VENDOR('F', 'U', 'J'), ISAPNP_DEVICE(0x0206) },
	/* Fujitsu Fax Voice 33600 PNP-I5 R Plug & Play */
	{	ISAPNP_VENDOR('F', 'U', 'J'), ISAPNP_DEVICE(0x0209) },
	/* Archtek America Corp. */
	/* Archtek SmartLink Modem 3334BT Plug & Play */
	{	ISAPNP_VENDOR('G', 'V', 'C'), ISAPNP_DEVICE(0x000F) },
	/* Hayes */
	/* Hayes Optima 288 V.34-V.FC + FAX + Voice Plug & Play */
	{	ISAPNP_VENDOR('H', 'A', 'Y'), ISAPNP_DEVICE(0x0001) },
	/* Hayes Optima 336 V.34 + FAX + Voice PnP */
	{	ISAPNP_VENDOR('H', 'A', 'Y'), ISAPNP_DEVICE(0x000C) },
	/* Hayes Optima 336B V.34 + FAX + Voice PnP */
	{	ISAPNP_VENDOR('H', 'A', 'Y'), ISAPNP_DEVICE(0x000D) },
	/* Hayes Accura 56K Ext Fax Modem PnP */
	{	ISAPNP_VENDOR('H', 'A', 'Y'), ISAPNP_DEVICE(0x5670) },
	/* Hayes Accura 56K Ext Fax Modem PnP */
	{	ISAPNP_VENDOR('H', 'A', 'Y'), ISAPNP_DEVICE(0x5674) },
	/* Hayes Accura 56K Fax Modem PnP */
	{	ISAPNP_VENDOR('H', 'A', 'Y'), ISAPNP_DEVICE(0x5675) },
	/* Hayes 288, V.34 + FAX */
	{	ISAPNP_VENDOR('H', 'A', 'Y'), ISAPNP_DEVICE(0xF000) },
	/* Hayes Optima 288 V.34 + FAX + Voice, Plug & Play */
	{	ISAPNP_VENDOR('H', 'A', 'Y'), ISAPNP_DEVICE(0xF001) },
	/* IBM */
	/* IBM Thinkpad 701 Internal Modem Voice */
	{	ISAPNP_VENDOR('I', 'B', 'M'), ISAPNP_DEVICE(0x0033) },
	/* Intertex */
	/* Intertex 28k8 33k6 Voice EXT PnP */
	{	ISAPNP_VENDOR('I', 'X', 'D'), ISAPNP_DEVICE(0xC801) },
	/* Intertex 33k6 56k Voice EXT PnP */
	{	ISAPNP_VENDOR('I', 'X', 'D'), ISAPNP_DEVICE(0xC901) },
	/* Intertex 28k8 33k6 Voice SP EXT PnP */
	{	ISAPNP_VENDOR('I', 'X', 'D'), ISAPNP_DEVICE(0xD801) },
	/* Intertex 33k6 56k Voice SP EXT PnP */
	{	ISAPNP_VENDOR('I', 'X', 'D'), ISAPNP_DEVICE(0xD901) },
	/* Intertex 28k8 33k6 Voice SP INT PnP */
	{	ISAPNP_VENDOR('I', 'X', 'D'), ISAPNP_DEVICE(0xF401) },
	/* Intertex 28k8 33k6 Voice SP EXT PnP */
	{	ISAPNP_VENDOR('I', 'X', 'D'), ISAPNP_DEVICE(0xF801) },
	/* Intertex 33k6 56k Voice SP EXT PnP */
	{	ISAPNP_VENDOR('I', 'X', 'D'), ISAPNP_DEVICE(0xF901) },
	/* Kortex International */
	/* KORTEX 28800 Externe PnP */
	{	ISAPNP_VENDOR('K', 'O', 'R'), ISAPNP_DEVICE(0x4522) },
	/* KXPro 33.6 Vocal ASVD PnP */
	{	ISAPNP_VENDOR('K', 'O', 'R'), ISAPNP_DEVICE(0xF661) },
	/* Lasat */
	/* LASAT Internet 33600 PnP */
	{	ISAPNP_VENDOR('L', 'A', 'S'), ISAPNP_DEVICE(0x4040) },
	/* Lasat Safire 560 PnP */
	{	ISAPNP_VENDOR('L', 'A', 'S'), ISAPNP_DEVICE(0x4540) },
	/* Lasat Safire 336  PnP */
	{	ISAPNP_VENDOR('L', 'A', 'S'), ISAPNP_DEVICE(0x5440) },
	/* Microcom, Inc. */
	/* Microcom TravelPorte FAST V.34 Plug & Play */
	{	ISAPNP_VENDOR('M', 'N', 'P'), ISAPNP_DEVICE(0x281) },
	/* Microcom DeskPorte V.34 FAST or FAST+ Plug & Play */
	{	ISAPNP_VENDOR('M', 'N', 'P'), ISAPNP_DEVICE(0x0336) },
	/* Microcom DeskPorte FAST EP 28.8 Plug & Play */
	{	ISAPNP_VENDOR('M', 'N', 'P'), ISAPNP_DEVICE(0x0339) },
	/* Microcom DeskPorte 28.8P Plug & Play */
	{	ISAPNP_VENDOR('M', 'N', 'P'), ISAPNP_DEVICE(0x0342) },
	/* Microcom DeskPorte FAST ES 28.8 Plug & Play */
	{	ISAPNP_VENDOR('M', 'N', 'P'), ISAPNP_DEVICE(0x0500) },
	/* Microcom DeskPorte FAST ES 28.8 Plug & Play */
	{	ISAPNP_VENDOR('M', 'N', 'P'), ISAPNP_DEVICE(0x0501) },
	/* Microcom DeskPorte 28.8S Internal Plug & Play */
	{	ISAPNP_VENDOR('M', 'N', 'P'), ISAPNP_DEVICE(0x0502) },
	/* Motorola */
	/* Motorola BitSURFR Plug & Play */
	{	ISAPNP_VENDOR('M', 'O', 'T'), ISAPNP_DEVICE(0x1105) },
	/* Motorola TA210 Plug & Play */
	{	ISAPNP_VENDOR('M', 'O', 'T'), ISAPNP_DEVICE(0x1111) },
	/* Motorola HMTA 200 (ISDN) Plug & Play */
	{	ISAPNP_VENDOR('M', 'O', 'T'), ISAPNP_DEVICE(0x1114) },
	/* Motorola BitSURFR Plug & Play */
	{	ISAPNP_VENDOR('M', 'O', 'T'), ISAPNP_DEVICE(0x1115) },
	/* Motorola Lifestyle 28.8 Internal */
	{	ISAPNP_VENDOR('M', 'O', 'T'), ISAPNP_DEVICE(0x1190) },
	/* Motorola V.3400 Plug & Play */
	{	ISAPNP_VENDOR('M', 'O', 'T'), ISAPNP_DEVICE(0x1501) },
	/* Motorola Lifestyle 28.8 V.34 Plug & Play */
	{	ISAPNP_VENDOR('M', 'O', 'T'), ISAPNP_DEVICE(0x1502) },
	/* Motorola Power 28.8 V.34 Plug & Play */
	{	ISAPNP_VENDOR('M', 'O', 'T'), ISAPNP_DEVICE(0x1505) },
	/* Motorola ModemSURFR External 28.8 Plug & Play */
	{	ISAPNP_VENDOR('M', 'O', 'T'), ISAPNP_DEVICE(0x1509) },
	/* Motorola Premier 33.6 Desktop Plug & Play */
	{	ISAPNP_VENDOR('M', 'O', 'T'), ISAPNP_DEVICE(0x150A) },
	/* Motorola VoiceSURFR 56K External PnP */
	{	ISAPNP_VENDOR('M', 'O', 'T'), ISAPNP_DEVICE(0x150F) },
	/* Motorola ModemSURFR 56K External PnP */
	{	ISAPNP_VENDOR('M', 'O', 'T'), ISAPNP_DEVICE(0x1510) },
	/* Motorola ModemSURFR 56K Internal PnP */
	{	ISAPNP_VENDOR('M', 'O', 'T'), ISAPNP_DEVICE(0x1550) },
	/* Motorola ModemSURFR Internal 28.8 Plug & Play */
	{	ISAPNP_VENDOR('M', 'O', 'T'), ISAPNP_DEVICE(0x1560) },
	/* Motorola Premier 33.6 Internal Plug & Play */
	{	ISAPNP_VENDOR('M', 'O', 'T'), ISAPNP_DEVICE(0x1580) },
	/* Motorola OnlineSURFR 28.8 Internal Plug & Play */
	{	ISAPNP_VENDOR('M', 'O', 'T'), ISAPNP_DEVICE(0x15B0) },
	/* Motorola VoiceSURFR 56K Internal PnP */
	{	ISAPNP_VENDOR('M', 'O', 'T'), ISAPNP_DEVICE(0x15F0) },
	/* Com 1 */
	/*  Deskline K56 Phone System PnP */
	{	ISAPNP_VENDOR('M', 'V', 'X'), ISAPNP_DEVICE(0x00A1) },
	/* PC Rider K56 Phone System PnP */
	{	ISAPNP_VENDOR('M', 'V', 'X'), ISAPNP_DEVICE(0x00F2) },
	/* Pace 56 Voice Internal Plug & Play Modem */
	{	ISAPNP_VENDOR('P', 'M', 'C'), ISAPNP_DEVICE(0x2430) },
	/* Generic */
	/* Generic standard PC COM port	 */
	{	ISAPNP_VENDOR('P', 'N', 'P'), ISAPNP_DEVICE(0x0500) },
	/* Generic 16550A-compatible COM port */
	{	ISAPNP_VENDOR('P', 'N', 'P'), ISAPNP_DEVICE(0x0501) },
	/* Compaq 14400 Modem */
	{	ISAPNP_VENDOR('P', 'N', 'P'), ISAPNP_DEVICE(0xC000) },
	/* Compaq 2400/9600 Modem */
	{	ISAPNP_VENDOR('P', 'N', 'P'), ISAPNP_DEVICE(0xC001) },
	/* Dial-Up Networking Serial Cable between 2 PCs */
	{	ISAPNP_VENDOR('P', 'N', 'P'), ISAPNP_DEVICE(0xC031) },
	/* Dial-Up Networking Parallel Cable between 2 PCs */
	{	ISAPNP_VENDOR('P', 'N', 'P'), ISAPNP_DEVICE(0xC032) },
	/* Standard 9600 bps Modem */
	{	ISAPNP_VENDOR('P', 'N', 'P'), ISAPNP_DEVICE(0xC100) },
	/* Standard 14400 bps Modem */
	{	ISAPNP_VENDOR('P', 'N', 'P'), ISAPNP_DEVICE(0xC101) },
	/*  Standard 28800 bps Modem*/
	{	ISAPNP_VENDOR('P', 'N', 'P'), ISAPNP_DEVICE(0xC102) },
	/*  Standard Modem*/
	{	ISAPNP_VENDOR('P', 'N', 'P'), ISAPNP_DEVICE(0xC103) },
	/*  Standard 9600 bps Modem*/
	{	ISAPNP_VENDOR('P', 'N', 'P'), ISAPNP_DEVICE(0xC104) },
	/*  Standard 14400 bps Modem*/
	{	ISAPNP_VENDOR('P', 'N', 'P'), ISAPNP_DEVICE(0xC105) },
	/*  Standard 28800 bps Modem*/
	{	ISAPNP_VENDOR('P', 'N', 'P'), ISAPNP_DEVICE(0xC106) },
	/*  Standard Modem */
	{	ISAPNP_VENDOR('P', 'N', 'P'), ISAPNP_DEVICE(0xC107) },
	/* Standard 9600 bps Modem */
	{	ISAPNP_VENDOR('P', 'N', 'P'), ISAPNP_DEVICE(0xC108) },
	/* Standard 14400 bps Modem */
	{	ISAPNP_VENDOR('P', 'N', 'P'), ISAPNP_DEVICE(0xC109) },
	/* Standard 28800 bps Modem */
	{	ISAPNP_VENDOR('P', 'N', 'P'), ISAPNP_DEVICE(0xC10A) },
	/* Standard Modem */
	{	ISAPNP_VENDOR('P', 'N', 'P'), ISAPNP_DEVICE(0xC10B) },
	/* Standard 9600 bps Modem */
	{	ISAPNP_VENDOR('P', 'N', 'P'), ISAPNP_DEVICE(0xC10C) },
	/* Standard 14400 bps Modem */
	{	ISAPNP_VENDOR('P', 'N', 'P'), ISAPNP_DEVICE(0xC10D) },
	/* Standard 28800 bps Modem */
	{	ISAPNP_VENDOR('P', 'N', 'P'), ISAPNP_DEVICE(0xC10E) },
	/* Standard Modem */
	{	ISAPNP_VENDOR('P', 'N', 'P'), ISAPNP_DEVICE(0xC10F) },
	/* Standard PCMCIA Card Modem */
	{	ISAPNP_VENDOR('P', 'N', 'P'), ISAPNP_DEVICE(0x2000) },
	/* Rockwell */
	/* Modular Technology */
	/* Rockwell 33.6 DPF Internal PnP */
	/* Modular Technology 33.6 Internal PnP */
	{	ISAPNP_VENDOR('R', 'O', 'K'), ISAPNP_DEVICE(0x0030) },
	/* Kortex International */
	/* KORTEX 14400 Externe PnP */
	{	ISAPNP_VENDOR('R', 'O', 'K'), ISAPNP_DEVICE(0x0100) },
	/* Viking Components, Inc */
	/* Viking 28.8 INTERNAL Fax+Data+Voice PnP */
	{	ISAPNP_VENDOR('R', 'O', 'K'), ISAPNP_DEVICE(0x4920) },
	/* Rockwell */
	/* British Telecom */
	/* Modular Technology */
	/* Rockwell 33.6 DPF External PnP */
	/* BT Prologue 33.6 External PnP */
	/* Modular Technology 33.6 External PnP */
	{	ISAPNP_VENDOR('R', 'S', 'S'), ISAPNP_DEVICE(0x00A0) },
	/* Viking 56K FAX INT */
	{	ISAPNP_VENDOR('R', 'S', 'S'), ISAPNP_DEVICE(0x0262) },
	/* SupraExpress 28.8 Data/Fax PnP modem */
	{	ISAPNP_VENDOR('S', 'U', 'P'), ISAPNP_DEVICE(0x1310) },
	/* SupraExpress 33.6 Data/Fax PnP modem */
	{	ISAPNP_VENDOR('S', 'U', 'P'), ISAPNP_DEVICE(0x1421) },
	/* SupraExpress 33.6 Data/Fax PnP modem */
	{	ISAPNP_VENDOR('S', 'U', 'P'), ISAPNP_DEVICE(0x1590) },
	/* SupraExpress 33.6 Data/Fax PnP modem */
	{	ISAPNP_VENDOR('S', 'U', 'P'), ISAPNP_DEVICE(0x1760) },
	/* Phoebe Micro */
	/* Phoebe Micro 33.6 Data Fax 1433VQH Plug & Play */
	{	ISAPNP_VENDOR('T', 'E', 'X'), ISAPNP_DEVICE(0x0011) },
	/* Archtek America Corp. */
	/* Archtek SmartLink Modem 3334BT Plug & Play */
	{	ISAPNP_VENDOR('U', 'A', 'C'), ISAPNP_DEVICE(0x000F) },
	/* 3Com Corp. */
	/* Gateway Telepath IIvi 33.6 */
	{	ISAPNP_VENDOR('U', 'S', 'R'), ISAPNP_DEVICE(0x0000) },
	/*  Sportster Vi 14.4 PnP FAX Voicemail */
	{	ISAPNP_VENDOR('U', 'S', 'R'), ISAPNP_DEVICE(0x0004) },
	/* U.S. Robotics 33.6K Voice INT PnP */
	{	ISAPNP_VENDOR('U', 'S', 'R'), ISAPNP_DEVICE(0x0006) },
	/* U.S. Robotics 33.6K Voice EXT PnP */
	{	ISAPNP_VENDOR('U', 'S', 'R'), ISAPNP_DEVICE(0x0007) },
	/* U.S. Robotics 33.6K Voice INT PnP */
	{	ISAPNP_VENDOR('U', 'S', 'R'), ISAPNP_DEVICE(0x2002) },
	/* U.S. Robotics 56K Voice INT PnP */
	{	ISAPNP_VENDOR('U', 'S', 'R'), ISAPNP_DEVICE(0x2070) },
	/* U.S. Robotics 56K Voice EXT PnP */
	{	ISAPNP_VENDOR('U', 'S', 'R'), ISAPNP_DEVICE(0x2080) },
	/* U.S. Robotics 56K FAX INT */
	{	ISAPNP_VENDOR('U', 'S', 'R'), ISAPNP_DEVICE(0x3031) },
	/* U.S. Robotics 56K Voice INT PnP */
	{	ISAPNP_VENDOR('U', 'S', 'R'), ISAPNP_DEVICE(0x3070) },
	/* U.S. Robotics 56K Voice EXT PnP */
	{	ISAPNP_VENDOR('U', 'S', 'R'), ISAPNP_DEVICE(0x3080) },
	/* U.S. Robotics 56K Voice INT PnP */
	{	ISAPNP_VENDOR('U', 'S', 'R'), ISAPNP_DEVICE(0x3090) },
	/* U.S. Robotics 56K Message  */
	{	ISAPNP_VENDOR('U', 'S', 'R'), ISAPNP_DEVICE(0x9100) },
	/*  U.S. Robotics 56K FAX EXT PnP*/
	{	ISAPNP_VENDOR('U', 'S', 'R'), ISAPNP_DEVICE(0x9160) },
	/*  U.S. Robotics 56K FAX INT PnP*/
	{	ISAPNP_VENDOR('U', 'S', 'R'), ISAPNP_DEVICE(0x9170) },
	/*  U.S. Robotics 56K Voice EXT PnP*/
	{	ISAPNP_VENDOR('U', 'S', 'R'), ISAPNP_DEVICE(0x9180) },
	/*  U.S. Robotics 56K Voice INT PnP*/
	{	ISAPNP_VENDOR('U', 'S', 'R'), ISAPNP_DEVICE(0x9190) },
	{	0, }
};

static inline void avoid_irq_share(struct pci_dev *dev)
{
	int i, map = 0x1FF8;
	struct serial_state *state = rs_table;
	struct isapnp_irq *irq;
	struct isapnp_resources *res = dev->sysdata;

	for (i = 0; i < NR_PORTS; i++) {
		if (state->type != PORT_UNKNOWN)
			clear_bit(state->irq, &map);
		state++;
	}

	for ( ; res; res = res->alt)
		for(irq = res->irq; irq; irq = irq->next)
			irq->map = map;
}

static char *modem_names[] __devinitdata = {
       "MODEM", "Modem", "modem", "FAX", "Fax", "fax",
       "56K", "56k", "K56", "33.6", "28.8", "14.4",
       "33,600", "28,800", "14,400", "33.600", "28.800", "14.400",
       "33600", "28800", "14400", "V.90", "V.34", "V.32", 0
};

static int __devinit check_name(char *name)
{
       char **tmp = modem_names;

       while (*tmp) {
               if (strstr(name, *tmp))
                       return 1;
               tmp++;
       }
       return 0;
}

static inline int check_compatible_id(struct pci_dev *dev)
{
       int i;
       for (i = 0; i < DEVICE_COUNT_COMPATIBLE; i++)
	       if ((dev->vendor_compatible[i] ==
		    ISAPNP_VENDOR('P', 'N', 'P')) &&
		   (swab16(dev->device_compatible[i]) >= 0xc000) &&
		   (swab16(dev->device_compatible[i]) <= 0xdfff))
		       return 0;
       return 1;
}

/*
 * Given a complete unknown ISA PnP device, try to use some heuristics to
 * detect modems. Currently use such heuristic set:
 *     - dev->name or dev->bus->name must contain "modem" substring;
 *     - device must have only one IO region (8 byte long) with base adress
 *       0x2e8, 0x3e8, 0x2f8 or 0x3f8.
 *
 * Such detection looks very ugly, but can detect at least some of numerous
 * ISA PnP modems, alternatively we must hardcode all modems in pnp_devices[]
 * table.
 */
static int _INLINE_ serial_pnp_guess_board(struct pci_dev *dev,
					  struct pci_board *board)
{
       struct isapnp_resources *res = (struct isapnp_resources *)dev->sysdata;
       struct isapnp_resources *resa;

       if (!(check_name(dev->name) || check_name(dev->bus->name)) &&
	   !(check_compatible_id(dev)))
	       return 1;

       if (!res || res->next)
	       return 1;

       for (resa = res->alt; resa; resa = resa->alt) {
	       struct isapnp_port *port;
	       for (port = res->port; port; port = port->next)
		       if ((port->size == 8) &&
			   ((port->min == 0x2f8) ||
			    (port->min == 0x3f8) ||
			    (port->min == 0x2e8) ||
			    (port->min == 0x3e8)))
			       return 0;
       }

       return 1;
}

static void __devinit probe_serial_pnp(void)
{
       struct pci_dev *dev = NULL;
       struct pnp_board *pnp_board;
       struct pci_board board;

#ifdef SERIAL_DEBUG_PNP
       printk("Entered probe_serial_pnp()\n");
#endif
       if (!isapnp_present()) {
#ifdef SERIAL_DEBUG_PNP
               printk("Leaving probe_serial_pnp() (no isapnp)\n");
#endif
               return;
       }

       isapnp_for_each_dev(dev) {
	       if (dev->active)
		       continue;

	       memset(&board, 0, sizeof(board));
	       board.flags = SPCI_FL_BASE0 | SPCI_FL_PNPDEFAULT;
	       board.num_ports = 1;
	       board.base_baud = 115200;
	       
	       for (pnp_board = pnp_devices; pnp_board->vendor; pnp_board++)
		       if ((dev->vendor == pnp_board->vendor) &&
			   (dev->device == pnp_board->device))
			       break;

	       if (pnp_board->vendor) {
		       /* Special case that's more efficient to hardcode */
		       if ((pnp_board->vendor == ISAPNP_VENDOR('A', 'K', 'Y') &&
			    pnp_board->device == ISAPNP_DEVICE(0x1021)))
			       board.flags |= SPCI_FL_NO_SHIRQ;
	       } else {
		       if (serial_pnp_guess_board(dev, &board))
			       continue;
	       }
	       
	       if (board.flags & SPCI_FL_NO_SHIRQ)
		       avoid_irq_share(dev);
	       start_pci_pnp_board(dev, &board);
       }

#ifdef SERIAL_DEBUG_PNP
       printk("Leaving probe_serial_pnp() (probe finished)\n");
#endif
       return;
}

#endif /* ENABLE_SERIAL_PNP */

/*
 * The serial driver boot-time initialization code!
 */
static int __init rs_init(void)
{
	int i;
	struct serial_state * state;

	init_bh(SERIAL_BH, do_serial_bh);
	init_timer(&serial_timer);
	serial_timer.function = rs_timer;
	mod_timer(&serial_timer, jiffies + RS_STROBE_TIME);

	for (i = 0; i < NR_IRQS; i++) {
		IRQ_ports[i] = 0;
		IRQ_timeout[i] = 0;
#ifdef CONFIG_SERIAL_MULTIPORT
		memset(&rs_multiport[i], 0,
		       sizeof(struct rs_multiport_struct));
#endif
	}
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
	show_serial_version();

	/* Initialize the tty_driver structure */
	
	memset(&serial_driver, 0, sizeof(struct tty_driver));
	serial_driver.magic = TTY_DRIVER_MAGIC;
#if (LINUX_VERSION_CODE > 0x20100)
	serial_driver.driver_name = "serial";
#endif
#if (LINUX_VERSION_CODE > 0x2032D && defined(CONFIG_DEVFS_FS))
	serial_driver.name = "tts/%d";
#else
	serial_driver.name = "ttyS";
#endif
	serial_driver.major = TTY_MAJOR;
	serial_driver.minor_start = 64 + SERIAL_DEV_OFFSET;
	serial_driver.name_base = SERIAL_DEV_OFFSET;
	serial_driver.num = NR_PORTS;
	serial_driver.type = TTY_DRIVER_TYPE_SERIAL;
	serial_driver.subtype = SERIAL_TYPE_NORMAL;
	serial_driver.init_termios = tty_std_termios;
	serial_driver.init_termios.c_cflag =
		B9600 | CS8 | CREAD | HUPCL | CLOCAL;
	serial_driver.flags = TTY_DRIVER_REAL_RAW | TTY_DRIVER_NO_DEVFS;
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
	serial_driver.set_termios = rs_set_termios;
	serial_driver.stop = rs_stop;
	serial_driver.start = rs_start;
	serial_driver.hangup = rs_hangup;
#if (LINUX_VERSION_CODE >= 131394) /* Linux 2.1.66 */
	serial_driver.break_ctl = rs_break;
#endif
#if (LINUX_VERSION_CODE >= 131343)
	serial_driver.send_xchar = rs_send_xchar;
	serial_driver.wait_until_sent = rs_wait_until_sent;
	serial_driver.read_proc = rs_read_proc;
#endif
	
	/*
	 * The callout device is just like normal device except for
	 * major number and the subtype code.
	 */
	callout_driver = serial_driver;
#if (LINUX_VERSION_CODE > 0x2032D && defined(CONFIG_DEVFS_FS))
	callout_driver.name = "cua/%d";
#else
	callout_driver.name = "cua";
#endif
	callout_driver.major = TTYAUX_MAJOR;
	callout_driver.subtype = SERIAL_TYPE_CALLOUT;
#if (LINUX_VERSION_CODE >= 131343)
	callout_driver.read_proc = 0;
	callout_driver.proc_entry = 0;
#endif

	if (tty_register_driver(&serial_driver))
		panic("Couldn't register serial driver\n");
	if (tty_register_driver(&callout_driver))
		panic("Couldn't register callout driver\n");
	
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
		state->irq = irq_cannonicalize(state->irq);
		if (state->hub6)
			state->io_type = SERIAL_IO_HUB6;
		if (state->port && check_region(state->port,8))
			continue;
#ifdef CONFIG_MCA			
		if ((state->flags & ASYNC_BOOT_ONLYMCA) && !MCA_bus)
			continue;
#endif			
		if (state->flags & ASYNC_BOOT_AUTOCONF)
			autoconfig(state);
	}
	for (i = 0, state = rs_table; i < NR_PORTS; i++,state++) {
		if (state->type == PORT_UNKNOWN)
			continue;
		if (   (state->flags & ASYNC_BOOT_AUTOCONF)
		    && (state->flags & ASYNC_AUTO_IRQ)
		    && (state->port != 0 || state->iomem_base != 0))
			state->irq = detect_uart_irq(state);
		if (state->io_type == SERIAL_IO_MEM) {
			printk(KERN_INFO"ttyS%02d%s at 0x%p (irq = %d) is a %s\n",
	 		       state->line + SERIAL_DEV_OFFSET,
			       (state->flags & ASYNC_FOURPORT) ? " FourPort" : "",
			       state->iomem_base, state->irq,
			       uart_config[state->type].name);
		}
		else {
			printk(KERN_INFO "ttyS%02d%s at 0x%04lx (irq = %d) is a %s\n",
	 		       state->line + SERIAL_DEV_OFFSET,
			       (state->flags & ASYNC_FOURPORT) ? " FourPort" : "",
			       state->port, state->irq,
			       uart_config[state->type].name);
		}
		tty_register_devfs(&serial_driver, 0,
				   serial_driver.minor_start + state->line);
		tty_register_devfs(&callout_driver, 0,
				   callout_driver.minor_start + state->line);
	}
#ifdef ENABLE_SERIAL_PCI
	probe_serial_pci();
#endif
#ifdef ENABLE_SERIAL_PNP
       probe_serial_pnp();
#endif
	return 0;
}

/*
 * This is for use by architectures that know their serial console 
 * attributes only at run time. Not to be invoked after rs_init().
 */
int __init early_serial_setup(struct serial_struct *req)
{
	int i = req->line;

	if (i >= NR_IRQS)
		return(-ENOENT);
	rs_table[i].magic = 0;
	rs_table[i].baud_base = req->baud_base;
	rs_table[i].port = req->port;
	if (HIGH_BITS_OFFSET)
		rs_table[i].port += (unsigned long) req->port_high << 
							HIGH_BITS_OFFSET;
	rs_table[i].irq = req->irq;
	rs_table[i].flags = req->flags;
	rs_table[i].close_delay = req->close_delay;
	rs_table[i].io_type = req->io_type;
	rs_table[i].hub6 = req->hub6;
	rs_table[i].iomem_base = req->iomem_base;
	rs_table[i].iomem_reg_shift = req->iomem_reg_shift;
	rs_table[i].type = req->type;
	rs_table[i].xmit_fifo_size = req->xmit_fifo_size;
	rs_table[i].custom_divisor = req->custom_divisor;
	rs_table[i].closing_wait = req->closing_wait;
	return(0);
}

/*
 * register_serial and unregister_serial allows for 16x50 serial ports to be
 * configured at run-time, to support PCMCIA modems.
 */
 
/**
 *	register_serial - configure a 16x50 serial port at runtime
 *	@req: request structure
 *
 *	Configure the serial port specified by the request. If the
 *	port exists and is in use an error is returned. If the port
 *	is not currently in the table it is added.
 *
 *	The port is then probed and if neccessary the IRQ is autodetected
 *	If this fails an error is returned.
 *
 *	On success the port is ready to use and the line number is returned.
 */
 
int register_serial(struct serial_struct *req)
{
	int i;
	unsigned long flags;
	struct serial_state *state;
	struct async_struct *info;
	unsigned long port;

	port = req->port;
	if (HIGH_BITS_OFFSET)
		port += (unsigned long) req->port_high << HIGH_BITS_OFFSET;

	save_flags(flags); cli();
	for (i = 0; i < NR_PORTS; i++) {
		if ((rs_table[i].port == port) &&
		    (rs_table[i].iomem_base == req->iomem_base))
			break;
	}
#ifdef __i386__
	if (i == NR_PORTS) {
		for (i = 4; i < NR_PORTS; i++)
			if ((rs_table[i].type == PORT_UNKNOWN) &&
			    (rs_table[i].count == 0))
				break;
	}
#endif
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
		printk("Couldn't configure serial #%d (port=%ld,irq=%d): "
		       "device already open\n", i, port, req->irq);
		return -1;
	}
	state->irq = req->irq;
	state->port = port;
	state->flags = req->flags;
	state->io_type = req->io_type;
	state->iomem_base = req->iomem_base;
	state->iomem_reg_shift = req->iomem_reg_shift;
	if (req->baud_base)
		state->baud_base = req->baud_base;
	if ((info = state->info) != NULL) {
		info->port = port;
		info->flags = req->flags;
		info->io_type = req->io_type;
		info->iomem_base = req->iomem_base;
		info->iomem_reg_shift = req->iomem_reg_shift;
	}
	autoconfig(state);
	if (state->type == PORT_UNKNOWN) {
		restore_flags(flags);
		printk("register_serial(): autoconfig failed\n");
		return -1;
	}
	restore_flags(flags);

	if ((state->flags & ASYNC_AUTO_IRQ) && CONFIGURED_SERIAL_PORT(state))
		state->irq = detect_uart_irq(state);

       printk(KERN_INFO "ttyS%02d at %s 0x%04lx (irq = %d) is a %s\n",
	      state->line + SERIAL_DEV_OFFSET,
	      state->iomem_base ? "iomem" : "port",
	      state->iomem_base ? (unsigned long)state->iomem_base :
	      state->port, state->irq, uart_config[state->type].name);
	tty_register_devfs(&serial_driver, 0,
			   serial_driver.minor_start + state->line); 
	tty_register_devfs(&callout_driver, 0,
			   callout_driver.minor_start + state->line);
	return state->line + SERIAL_DEV_OFFSET;
}

/**
 *	unregister_serial - deconfigure a 16x50 serial port
 *	@line: line to deconfigure
 *
 *	The port specified is deconfigured and its resources are freed. Any
 *	user of the port is disconnected as if carrier was dropped. Line is
 *	the port number returned by register_serial().
 */

void unregister_serial(int line)
{
	unsigned long flags;
	struct serial_state *state = &rs_table[line];

	save_flags(flags); cli();
	if (state->info && state->info->tty)
		tty_hangup(state->info->tty);
	state->type = PORT_UNKNOWN;
	printk(KERN_INFO "ttyS%02d unloaded\n", state->line);
	/* These will be hidden, because they are devices that will no longer
	 * be available to the system. (ie, PCMCIA modems, once ejected)
	 */
	tty_unregister_devfs(&serial_driver,
			     serial_driver.minor_start + state->line);
	tty_unregister_devfs(&callout_driver,
			     callout_driver.minor_start + state->line);
	restore_flags(flags);
}

static void __exit rs_fini(void) 
{
	unsigned long flags;
	int e1, e2;
	int i;
	struct async_struct *info;

	/* printk("Unloading %s: version %s\n", serial_name, serial_version); */
	del_timer_sync(&serial_timer);
	save_flags(flags); cli();
        remove_bh(SERIAL_BH);
	if ((e1 = tty_unregister_driver(&serial_driver)))
		printk("serial: failed to unregister serial driver (%d)\n",
		       e1);
	if ((e2 = tty_unregister_driver(&callout_driver)))
		printk("serial: failed to unregister callout driver (%d)\n", 
		       e2);
	restore_flags(flags);

	for (i = 0; i < NR_PORTS; i++) {
		if ((info = rs_table[i].info)) {
			rs_table[i].info = NULL;
			kfree(info);
		}
		if ((rs_table[i].type != PORT_UNKNOWN) && rs_table[i].port) {
#ifdef CONFIG_SERIAL_RSA
			if (rs_table[i].type == PORT_RSA)
				release_region(rs_table[i].port +
					       UART_RSA_BASE, 16);
			else
#endif
				release_region(rs_table[i].port, 8);
		}
#if defined(ENABLE_SERIAL_PCI) || defined(ENABLE_SERIAL_PNP)
		if (rs_table[i].iomem_base)
			iounmap(rs_table[i].iomem_base);
#endif
	}
#if defined(ENABLE_SERIAL_PCI) || defined(ENABLE_SERIAL_PNP)
	for (i=0; i < NR_PCI_BOARDS; i++) {
		struct pci_board_inst *brd = &serial_pci_board[i];

		if (serial_pci_board[i].dev == 0)
			continue;
		if (brd->board.init_fn)
			(brd->board.init_fn)(brd->dev, &brd->board, 0);
		if (DEACTIVATE_FUNC(brd->dev))
			(DEACTIVATE_FUNC(brd->dev))(brd->dev);
	}
#endif	
	if (tmp_buf) {
		unsigned long pg = (unsigned long) tmp_buf;
		tmp_buf = NULL;
		free_page(pg);
	}
	
#ifdef ENABLE_SERIAL_PCI
	if (serial_pci_driver.name[0])
		pci_unregister_driver (&serial_pci_driver);
#endif
}

module_init(rs_init);
module_exit(rs_fini);
MODULE_DESCRIPTION("Standard/generic (dumb) serial driver");
MODULE_AUTHOR("Theodore Ts'o <tytso@mit.edu>");
MODULE_LICENSE("GPL");


/*
 * ------------------------------------------------------------
 * Serial console driver
 * ------------------------------------------------------------
 */
#ifdef CONFIG_SERIAL_CONSOLE

#define BOTH_EMPTY (UART_LSR_TEMT | UART_LSR_THRE)

static struct async_struct async_sercons;

/*
 *	Wait for transmitter & holding register to empty
 */
static inline void wait_for_xmitr(struct async_struct *info)
{
	unsigned int status, tmout = 1000000;

	do {
		status = serial_in(info, UART_LSR);

		if (status & UART_LSR_BI)
			lsr_break_flag = UART_LSR_BI;
		
		if (--tmout == 0)
			break;
	} while((status & BOTH_EMPTY) != BOTH_EMPTY);

	/* Wait for flow control if necessary */
	if (info->flags & ASYNC_CONS_FLOW) {
		tmout = 1000000;
		while (--tmout &&
		       ((serial_in(info, UART_MSR) & UART_MSR_CTS) == 0));
	}	
}


/*
 *	Print a string to the serial port trying not to disturb
 *	any possible real use of the port...
 *
 *	The console must be locked when we get here.
 */
static void serial_console_write(struct console *co, const char *s,
				unsigned count)
{
	static struct async_struct *info = &async_sercons;
	int ier;
	unsigned i;

	/*
	 *	First save the IER then disable the interrupts
	 */
	ier = serial_in(info, UART_IER);
	serial_out(info, UART_IER, 0x00);

	/*
	 *	Now, do each character
	 */
	for (i = 0; i < count; i++, s++) {
		wait_for_xmitr(info);

		/*
		 *	Send the character out.
		 *	If a LF, also do CR...
		 */
		serial_out(info, UART_TX, *s);
		if (*s == 10) {
			wait_for_xmitr(info);
			serial_out(info, UART_TX, 13);
		}
	}

	/*
	 *	Finally, Wait for transmitter & holding register to empty
	 * 	and restore the IER
	 */
	wait_for_xmitr(info);
	serial_out(info, UART_IER, ier);
}

static kdev_t serial_console_device(struct console *c)
{
	return MKDEV(TTY_MAJOR, 64 + c->index);
}

/*
 *	Setup initial baud/bits/parity/flow control. We do two things here:
 *	- construct a cflag setting for the first rs_open()
 *	- initialize the serial port
 *	Return non-zero if we didn't find a serial port.
 */
static int __init serial_console_setup(struct console *co, char *options)
{
	static struct async_struct *info;
	struct serial_state *state;
	unsigned cval;
	int	baud = 9600;
	int	bits = 8;
	int	parity = 'n';
	int	doflow = 0;
	int	cflag = CREAD | HUPCL | CLOCAL;
	int	quot = 0;
	char	*s;

	if (options) {
		baud = simple_strtoul(options, NULL, 10);
		s = options;
		while(*s >= '0' && *s <= '9')
			s++;
		if (*s) parity = *s++;
		if (*s) bits   = *s++ - '0';
		if (*s) doflow = (*s++ == 'r');
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
			/*
			 * Set this to a sane value to prevent a divide error
			 */
			baud  = 9600;
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
	state = rs_table + co->index;
	if (doflow)
		state->flags |= ASYNC_CONS_FLOW;
	info = &async_sercons;
	info->magic = SERIAL_MAGIC;
	info->state = state;
	info->port = state->port;
	info->flags = state->flags;
#ifdef CONFIG_HUB6
	info->hub6 = state->hub6;
#endif
	info->io_type = state->io_type;
	info->iomem_base = state->iomem_base;
	info->iomem_reg_shift = state->iomem_reg_shift;
	quot = state->baud_base / baud;
	cval = cflag & (CSIZE | CSTOPB);
#if defined(__powerpc__) || defined(__alpha__)
	cval >>= 8;
#else /* !__powerpc__ && !__alpha__ */
	cval >>= 4;
#endif /* !__powerpc__ && !__alpha__ */
	if (cflag & PARENB)
		cval |= UART_LCR_PARITY;
	if (!(cflag & PARODD))
		cval |= UART_LCR_EPAR;

	/*
	 *	Disable UART interrupts, set DTR and RTS high
	 *	and set speed.
	 */
	serial_out(info, UART_LCR, cval | UART_LCR_DLAB);	/* set DLAB */
	serial_out(info, UART_DLL, quot & 0xff);	/* LS of divisor */
	serial_out(info, UART_DLM, quot >> 8);		/* MS of divisor */
	serial_out(info, UART_LCR, cval);		/* reset DLAB */
	serial_out(info, UART_IER, 0);
	serial_out(info, UART_MCR, UART_MCR_DTR | UART_MCR_RTS);

	/*
	 *	If we read 0xff from the LSR, there is no UART here.
	 */
	if (serial_in(info, UART_LSR) == 0xff)
		return -1;

	return 0;
}

static struct console sercons = {
	name:		"ttyS",
	write:		serial_console_write,
	device:		serial_console_device,
	setup:		serial_console_setup,
	flags:		CON_PRINTBUFFER,
	index:		-1,
};

/*
 *	Register console.
 */
void __init serial_console_init(void)
{
	register_console(&sercons);
}
#endif

/*
  Local variables:
  compile-command: "gcc -D__KERNEL__ -I../../include -Wall -Wstrict-prototypes -O2 -fomit-frame-pointer -fno-strict-aliasing -pipe -fno-strength-reduce -march=i586 -DMODULE -DMODVERSIONS -include ../../include/linux/modversions.h   -DEXPORT_SYMTAB -c serial.c"
  End:
*/
