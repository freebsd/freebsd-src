/*
 *      specialix.c  -- specialix IO8+ multiport serial driver.
 *
 *      Copyright (C) 1997  Roger Wolff (R.E.Wolff@BitWizard.nl)
 *      Copyright (C) 1994-1996  Dmitry Gorodchanin (pgmdsg@ibi.com)
 *
 *      Specialix pays for the development and support of this driver.
 *      Please DO contact io8-linux@specialix.co.uk if you require
 *      support. But please read the documentation (specialix.txt)
 *      first.
 *
 *      This driver was developped in the BitWizard linux device
 *      driver service. If you require a linux device driver for your
 *      product, please contact devices@BitWizard.nl for a quote.
 *
 *      This code is firmly based on the riscom/8 serial driver,
 *      written by Dmitry Gorodchanin. The specialix IO8+ card
 *      programming information was obtained from the CL-CD1865 Data
 *      Book, and Specialix document number 6200059: IO8+ Hardware
 *      Functional Specification.
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License as
 *      published by the Free Software Foundation; either version 2 of
 *      the License, or (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be
 *      useful, but WITHOUT ANY WARRANTY; without even the implied
 *      warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *      PURPOSE.  See the GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public
 *      License along with this program; if not, write to the Free
 *      Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139,
 *      USA.
 *
 * Revision history:
 *
 * Revision 1.0:  April 1st 1997.
 *                Initial release for alpha testing.
 * Revision 1.1:  April 14th 1997. 
 *                Incorporated Richard Hudsons suggestions, 
 *                removed some debugging printk's.
 * Revision 1.2:  April 15th 1997.
 *                Ported to 2.1.x kernels.
 * Revision 1.3:  April 17th 1997 
 *                Backported to 2.0. (Compatibility macros). 
 * Revision 1.4:  April 18th 1997
 *                Fixed DTR/RTS bug that caused the card to indicate 
 *                "don't send data" to a modem after the password prompt.  
 *                Fixed bug for premature (fake) interrupts.
 * Revision 1.5:  April 19th 1997
 *                fixed a minor typo in the header file, cleanup a little. 
 *                performance warnings are now MAXed at once per minute.
 * Revision 1.6:  May 23 1997
 *                Changed the specialix=... format to include interrupt.
 * Revision 1.7:  May 27 1997
 *                Made many more debug printk's a compile time option.
 * Revision 1.8:  Jul 1  1997
 *                port to linux-2.1.43 kernel.
 * Revision 1.9:  Oct 9  1998
 *                Added stuff for the IO8+/PCI version.
 * Revision 1.10: Oct 22  1999 / Jan 21 2000. 
 *                Added stuff for setserial. 
 *                Nicolas Mailhot (Nicolas.Mailhot@email.enst.fr)
 * 
 */

#define VERSION "1.10"


/*
 * There is a bunch of documentation about the card, jumpers, config
 * settings, restrictions, cables, device names and numbers in
 * ../../Documentation/specialix.txt 
 */

#include <linux/config.h>
#include <linux/module.h>

#include <asm/io.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/errno.h>
#include <linux/tty.h>
#include <linux/mm.h>
#include <linux/serial.h>
#include <linux/fcntl.h>
#include <linux/major.h>
#include <linux/delay.h>
#include <linux/tqueue.h>
#include <linux/version.h>
#include <linux/pci.h>


/* ************************************************************** */
/* * This section can be removed when 2.0 becomes outdated....  * */
/* ************************************************************** */

#if LINUX_VERSION_CODE < 131328    /* Less than 2.1.0 */
#define TWO_ZERO
#else
#if LINUX_VERSION_CODE < 131371   /* less than 2.1.43 */
/* This has not been extensively tested yet. Sorry. */
#warning "You're on your own between 2.1.0 and 2.1.43.... "
#warning "Please use a recent kernel."
#endif
#endif


#ifdef TWO_ZERO
#define Get_user(a,b)         a = get_user(b)
#define copy_from_user(a,b,c) memcpy_fromfs(a,b,c)
#define copy_to_user(a,b,c)   memcpy_tofs(a,b,c)
#define queue_task            queue_task_irq_off
#else
#define Get_user(a,b)         get_user(a,b)
#endif

/* ************************************************************** */
/* *                End of compatibility section..              * */
/* ************************************************************** */


#ifndef TWO_ZERO
#include <asm/uaccess.h>
#endif

#include "specialix_io8.h"
#include "cd1865.h"



/* Configurable options: */

/* Am I paranoid or not ? ;-) */
#define SPECIALIX_PARANOIA_CHECK

/* Do I trust the IRQ from the card? (enabeling it doesn't seem to help)
   When the IRQ routine leaves the chip in a state that is keeps on
   requiring attention, the timer doesn't help either. */
#undef SPECIALIX_TIMER

/* 
 * The following defines are mostly for testing purposes. But if you need
 * some nice reporting in your syslog, you can define them also.
 */
#undef SX_REPORT_FIFO
#undef SX_REPORT_OVERRUN



#ifdef CONFIG_SPECIALIX_RTSCTS
#define SX_CRTSCTS(bla) 1
#else
#define SX_CRTSCTS(tty) C_CRTSCTS(tty)
#endif


/* Used to be outb (0xff, 0x80); */
#define short_pause() udelay (1)


#define SPECIALIX_LEGAL_FLAGS \
	(ASYNC_HUP_NOTIFY   | ASYNC_SAK          | ASYNC_SPLIT_TERMIOS   | \
	 ASYNC_SPD_HI       | ASYNC_SPEED_VHI    | ASYNC_SESSION_LOCKOUT | \
	 ASYNC_PGRP_LOCKOUT | ASYNC_CALLOUT_NOHUP)

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

DECLARE_TASK_QUEUE(tq_specialix);

#undef RS_EVENT_WRITE_WAKEUP
#define RS_EVENT_WRITE_WAKEUP	0

#define SPECIALIX_TYPE_NORMAL	1
#define SPECIALIX_TYPE_CALLOUT	2

static struct tty_driver specialix_driver, specialix_callout_driver;
static int    specialix_refcount;
static struct tty_struct * specialix_table[SX_NBOARD * SX_NPORT];
static struct termios * specialix_termios[SX_NBOARD * SX_NPORT];
static struct termios * specialix_termios_locked[SX_NBOARD * SX_NPORT];
static unsigned char * tmp_buf;
static DECLARE_MUTEX(tmp_buf_sem);

static unsigned long baud_table[] =  {
	0, 50, 75, 110, 134, 150, 200, 300, 600, 1200, 1800, 2400, 4800,
	9600, 19200, 38400, 57600, 115200, 0, 
};

static struct specialix_board sx_board[SX_NBOARD] =  {
	{ 0, SX_IOBASE1,  9, },
	{ 0, SX_IOBASE2, 11, },
	{ 0, SX_IOBASE3, 12, },
	{ 0, SX_IOBASE4, 15, },
};

static struct specialix_port sx_port[SX_NBOARD * SX_NPORT];
		

#ifdef SPECIALIX_TIMER
static struct timer_list missed_irq_timer;
static void sx_interrupt(int irq, void * dev_id, struct pt_regs * regs);
#endif



static inline int sx_paranoia_check(struct specialix_port const * port,
				    kdev_t device, const char *routine)
{
#ifdef SPECIALIX_PARANOIA_CHECK
	static const char *badmagic =
		KERN_ERR "sx: Warning: bad specialix port magic number for device %s in %s\n";
	static const char *badinfo =
		KERN_ERR "sx: Warning: null specialix port for device %s in %s\n";
 
	if (!port) {
		printk(badinfo, kdevname(device), routine);
		return 1;
	}
	if (port->magic != SPECIALIX_MAGIC) {
		printk(badmagic, kdevname(device), routine);
		return 1;
	}
#endif
	return 0;
}


/*
 * 
 *  Service functions for specialix IO8+ driver.
 * 
 */

/* Get board number from pointer */
static inline int board_No (struct specialix_board * bp)
{
	return bp - sx_board;
}


/* Get port number from pointer */
static inline int port_No (struct specialix_port const * port)
{
	return SX_PORT(port - sx_port); 
}


/* Get pointer to board from pointer to port */
static inline struct specialix_board * port_Board(struct specialix_port const * port)
{
	return &sx_board[SX_BOARD(port - sx_port)];
}


/* Input Byte from CL CD186x register */
static inline unsigned char sx_in(struct specialix_board  * bp, unsigned short reg)
{
	bp->reg = reg | 0x80;
	outb (reg | 0x80, bp->base + SX_ADDR_REG);
	return inb  (bp->base + SX_DATA_REG);
}


/* Output Byte to CL CD186x register */
static inline void sx_out(struct specialix_board  * bp, unsigned short reg,
			  unsigned char val)
{
	bp->reg = reg | 0x80;
	outb (reg | 0x80, bp->base + SX_ADDR_REG);
	outb (val, bp->base + SX_DATA_REG);
}


/* Input Byte from CL CD186x register */
static inline unsigned char sx_in_off(struct specialix_board  * bp, unsigned short reg)
{
	bp->reg = reg;
	outb (reg, bp->base + SX_ADDR_REG);
	return inb  (bp->base + SX_DATA_REG);
}


/* Output Byte to CL CD186x register */
static inline void sx_out_off(struct specialix_board  * bp, unsigned short reg,
			  unsigned char val)
{
	bp->reg = reg;
	outb (reg, bp->base + SX_ADDR_REG);
	outb (val, bp->base + SX_DATA_REG);
}


/* Wait for Channel Command Register ready */
static inline void sx_wait_CCR(struct specialix_board  * bp)
{
	unsigned long delay;

	for (delay = SX_CCR_TIMEOUT; delay; delay--) 
		if (!sx_in(bp, CD186x_CCR))
			return;
	
	printk(KERN_ERR "sx%d: Timeout waiting for CCR.\n", board_No(bp));
}


/* Wait for Channel Command Register ready */
static inline void sx_wait_CCR_off(struct specialix_board  * bp)
{
	unsigned long delay;

	for (delay = SX_CCR_TIMEOUT; delay; delay--) 
		if (!sx_in_off(bp, CD186x_CCR))
			return;
	
	printk(KERN_ERR "sx%d: Timeout waiting for CCR.\n", board_No(bp));
}


/*
 *  specialix IO8+ IO range functions.
 */

static inline int sx_check_io_range(struct specialix_board * bp)
{
	return check_region (bp->base, SX_IO_SPACE);
}


static inline void sx_request_io_range(struct specialix_board * bp)
{
	request_region(bp->base, 
	               bp->flags&SX_BOARD_IS_PCI?SX_PCI_IO_SPACE:SX_IO_SPACE,
	               "specialix IO8+" );
}


static inline void sx_release_io_range(struct specialix_board * bp)
{
	release_region(bp->base, 
	               bp->flags&SX_BOARD_IS_PCI?SX_PCI_IO_SPACE:SX_IO_SPACE);
}

	
/* Must be called with enabled interrupts */
/* Ugly. Very ugly. Don't use this for anything else than initialization 
   code */
static inline void sx_long_delay(unsigned long delay)
{
	unsigned long i;
	
	for (i = jiffies + delay; time_after(i, jiffies); ) ;
}



/* Set the IRQ using the RTS lines that run to the PAL on the board.... */
int sx_set_irq ( struct specialix_board *bp)
{
	int virq;
	int i;

	if (bp->flags & SX_BOARD_IS_PCI) 
		return 1;
	switch (bp->irq) {
	/* In the same order as in the docs... */
	case 15: virq = 0;break;
	case 12: virq = 1;break;
	case 11: virq = 2;break;
	case 9:  virq = 3;break;
	default: printk (KERN_ERR "Speclialix: cannot set irq to %d.\n", bp->irq);
	         return 0;
	}

	for (i=0;i<2;i++) {
		sx_out(bp, CD186x_CAR, i);
		sx_out(bp, CD186x_MSVRTS, ((virq >> i) & 0x1)? MSVR_RTS:0);
	}
	return 1;
}


/* Reset and setup CD186x chip */
static int sx_init_CD186x(struct specialix_board  * bp)
{
	unsigned long flags;
	int scaler;
	int rv = 1;
	
	save_flags(flags); cli();

	sx_wait_CCR_off(bp);			   /* Wait for CCR ready        */
	sx_out_off(bp, CD186x_CCR, CCR_HARDRESET);      /* Reset CD186x chip          */
	sti();
	sx_long_delay(HZ/20);                      /* Delay 0.05 sec            */
	cli();
	sx_out_off(bp, CD186x_GIVR, SX_ID);             /* Set ID for this chip      */
	sx_out_off(bp, CD186x_GICR, 0);                 /* Clear all bits            */
	sx_out_off(bp, CD186x_PILR1, SX_ACK_MINT);      /* Prio for modem intr       */
	sx_out_off(bp, CD186x_PILR2, SX_ACK_TINT);      /* Prio for transmitter intr */
	sx_out_off(bp, CD186x_PILR3, SX_ACK_RINT);      /* Prio for receiver intr    */
	/* Set RegAckEn */
	sx_out_off(bp, CD186x_SRCR, sx_in (bp, CD186x_SRCR) | SRCR_REGACKEN);
	
	/* Setting up prescaler. We need 4 ticks per 1 ms */
	scaler =  SX_OSCFREQ/SPECIALIX_TPS;

	sx_out_off(bp, CD186x_PPRH, scaler >> 8);
	sx_out_off(bp, CD186x_PPRL, scaler & 0xff);

	if (!sx_set_irq (bp)) {
		/* Figure out how to pass this along... */
		printk (KERN_ERR "Cannot set irq to %d.\n", bp->irq);
		rv = 0;
	}

	restore_flags(flags);
	return rv;
}


int read_cross_byte (struct specialix_board *bp, int reg, int bit)
{
	int i;
	int t;

	for (i=0, t=0;i<8;i++) {
		sx_out_off (bp, CD186x_CAR, i);
		if (sx_in_off (bp, reg) & bit) 
			t |= 1 << i;
	}
	return t;
}


#ifdef SPECIALIX_TIMER
void missed_irq (unsigned long data)
{
	if (sx_in ((struct specialix_board *)data, CD186x_SRSR) &  
	                                            (SRSR_RREQint |
	                                             SRSR_TREQint |
	                                             SRSR_MREQint)) {
		printk (KERN_INFO "Missed interrupt... Calling int from timer. \n");
		sx_interrupt (((struct specialix_board *)data)->irq, 
		              NULL, NULL);
	}
	missed_irq_timer.expires = jiffies + HZ;
	add_timer (&missed_irq_timer);
}
#endif



/* Main probing routine, also sets irq. */
static int sx_probe(struct specialix_board *bp)
{
	unsigned char val1, val2;
#if 0
	int irqs = 0;
	int retries;
#endif
	int rev;
	int chip;

	if (sx_check_io_range(bp)) 
		return 1;

	/* Are the I/O ports here ? */
	sx_out_off(bp, CD186x_PPRL, 0x5a);
	short_pause ();
	val1 = sx_in_off(bp, CD186x_PPRL);

	sx_out_off(bp, CD186x_PPRL, 0xa5);
	short_pause ();
	val2 = sx_in_off(bp, CD186x_PPRL);

	
	if ((val1 != 0x5a) || (val2 != 0xa5)) {
		printk(KERN_INFO "sx%d: specialix IO8+ Board at 0x%03x not found.\n",
		       board_No(bp), bp->base);
		return 1;
	}

	/* Check the DSR lines that Specialix uses as board 
	   identification */
	val1 = read_cross_byte (bp, CD186x_MSVR, MSVR_DSR);
	val2 = read_cross_byte (bp, CD186x_MSVR, MSVR_RTS);
#ifdef SPECIALIX_DEBUG
	printk (KERN_DEBUG "sx%d: DSR lines are: %02x, rts lines are: %02x\n", 
	        board_No(bp),  val1, val2);
#endif
	/* They managed to switch the bit order between the docs and
	   the IO8+ card. The new PCI card now conforms to old docs.
	   They changed the PCI docs to reflect the situation on the
	   old card. */
	val2 = (bp->flags & SX_BOARD_IS_PCI)?0x4d : 0xb2;
	if (val1 != val2) {
		printk(KERN_INFO "sx%d: specialix IO8+ ID %02x at 0x%03x not found (%02x).\n",
		       board_No(bp), val2, bp->base, val1);
		return 1;
	}


#if 0
	/* It's time to find IRQ for this board */
	for (retries = 0; retries < 5 && irqs <= 0; retries++) {
		irqs = probe_irq_on();
		sx_init_CD186x(bp);	       		/* Reset CD186x chip       */
		sx_out(bp, CD186x_CAR, 2);               /* Select port 2          */
		sx_wait_CCR(bp);
		sx_out(bp, CD186x_CCR, CCR_TXEN);        /* Enable transmitter     */
		sx_out(bp, CD186x_IER, IER_TXRDY);       /* Enable tx empty intr   */
		sx_long_delay(HZ/20);	       		
		irqs = probe_irq_off(irqs);

#if SPECIALIX_DEBUG > 2
		printk (KERN_DEBUG "SRSR = %02x, ",  sx_in(bp, CD186x_SRSR));
		printk (           "TRAR = %02x, ",  sx_in(bp, CD186x_TRAR));
		printk (           "GIVR = %02x, ",  sx_in(bp, CD186x_GIVR));
		printk (           "GICR = %02x, ",  sx_in(bp, CD186x_GICR));
		printk (           "\n");
#endif
		/* Reset CD186x again      */
		if (!sx_init_CD186x(bp)) {
			/* Hmmm. This is dead code anyway. */
		}
#if SPECIALIX_DEBUG > 2
		printk (KERN_DEBUG "val1 = %02x, val2 = %02x, val3 = %02x.\n", 
		        val1, val2, val3); 
#endif
	
	}
	
#if 0
	if (irqs <= 0) {
		printk(KERN_ERR "sx%d: Can't find IRQ for specialix IO8+ board at 0x%03x.\n",
		       board_No(bp), bp->base);
		return 1;
	}
#endif
	printk (KERN_INFO "Started with irq=%d, but now have irq=%d.\n", bp->irq, irqs);
	if (irqs > 0)
		bp->irq = irqs;
#endif
	/* Reset CD186x again  */
	if (!sx_init_CD186x(bp)) {
		return -EIO;
	}

	sx_request_io_range(bp);
	bp->flags |= SX_BOARD_PRESENT;
	
	/* Chip           revcode   pkgtype
	                  GFRCR     SRCR bit 7
	   CD180 rev B    0x81      0
	   CD180 rev C    0x82      0
	   CD1864 rev A   0x82      1
	   CD1865 rev A   0x83      1  -- Do not use!!! Does not work. 
	   CD1865 rev B   0x84      1
	 -- Thanks to Gwen Wang, Cirrus Logic.
	 */

	switch (sx_in_off(bp, CD186x_GFRCR)) {
	case 0x82:chip = 1864;rev='A';break;
	case 0x83:chip = 1865;rev='A';break;
	case 0x84:chip = 1865;rev='B';break;
	case 0x85:chip = 1865;rev='C';break; /* Does not exist at this time */
	default:chip=-1;rev='x';
	}

#if SPECIALIX_DEBUG > 2
	printk (KERN_DEBUG " GFCR = 0x%02x\n", sx_in_off(bp, CD186x_GFRCR) );
#endif

#ifdef SPECIALIX_TIMER
	init_timer (&missed_irq_timer);
	missed_irq_timer.function = missed_irq;
	missed_irq_timer.data = (unsigned long) bp;
	missed_irq_timer.expires = jiffies + HZ;
	add_timer (&missed_irq_timer);
#endif

	printk(KERN_INFO"sx%d: specialix IO8+ board detected at 0x%03x, IRQ %d, CD%d Rev. %c.\n",
	       board_No(bp),
	       bp->base, bp->irq,
	       chip, rev);

	return 0;
}

/* 
 * 
 *  Interrupt processing routines.
 * */

static inline void sx_mark_event(struct specialix_port * port, int event)
{
	/* 
	 * I'm not quite happy with current scheme all serial
	 * drivers use their own BH routine.
	 * It seems this easily can be done with one BH routine
	 * serving for all serial drivers.
	 * For now I must introduce another one - SPECIALIX_BH.
	 * Still hope this will be changed in near future.
	 * -- Dmitry.
	 */
	set_bit(event, &port->event);
	queue_task(&port->tqueue, &tq_specialix);
	mark_bh(SPECIALIX_BH);
}


static inline struct specialix_port * sx_get_port(struct specialix_board * bp,
					       unsigned char const * what)
{
	unsigned char channel;
	struct specialix_port * port;
	
	channel = sx_in(bp, CD186x_GICR) >> GICR_CHAN_OFF;
	if (channel < CD186x_NCH) {
		port = &sx_port[board_No(bp) * SX_NPORT + channel];
		if (port->flags & ASYNC_INITIALIZED) {
			return port;
		}
	}
	printk(KERN_INFO "sx%d: %s interrupt from invalid port %d\n", 
	       board_No(bp), what, channel);
	return NULL;
}


static inline void sx_receive_exc(struct specialix_board * bp)
{
	struct specialix_port *port;
	struct tty_struct *tty;
	unsigned char status;
	unsigned char ch;

	if (!(port = sx_get_port(bp, "Receive")))
		return;

	tty = port->tty;
	if (tty->flip.count >= TTY_FLIPBUF_SIZE) {
		printk(KERN_INFO "sx%d: port %d: Working around flip buffer overflow.\n",
		       board_No(bp), port_No(port));
		return;
	}
	
#ifdef SX_REPORT_OVERRUN	
	status = sx_in(bp, CD186x_RCSR);
	if (status & RCSR_OE) {
		port->overrun++;
#if SPECIALIX_DEBUG 
		printk(KERN_DEBUG "sx%d: port %d: Overrun. Total %ld overruns.\n", 
		       board_No(bp), port_No(port), port->overrun);
#endif		
	}
	status &= port->mark_mask;
#else	
	status = sx_in(bp, CD186x_RCSR) & port->mark_mask;
#endif	
	ch = sx_in(bp, CD186x_RDR);
	if (!status) {
		return;
	}
	if (status & RCSR_TOUT) {
		printk(KERN_INFO "sx%d: port %d: Receiver timeout. Hardware problems ?\n", 
		       board_No(bp), port_No(port));
		return;
		
	} else if (status & RCSR_BREAK) {
#ifdef SPECIALIX_DEBUG
		printk(KERN_DEBUG "sx%d: port %d: Handling break...\n",
		       board_No(bp), port_No(port));
#endif
		*tty->flip.flag_buf_ptr++ = TTY_BREAK;
		if (port->flags & ASYNC_SAK)
			do_SAK(tty);
		
	} else if (status & RCSR_PE) 
		*tty->flip.flag_buf_ptr++ = TTY_PARITY;
	
	else if (status & RCSR_FE) 
		*tty->flip.flag_buf_ptr++ = TTY_FRAME;
	
	else if (status & RCSR_OE)
		*tty->flip.flag_buf_ptr++ = TTY_OVERRUN;
	
	else
		*tty->flip.flag_buf_ptr++ = 0;
	
	*tty->flip.char_buf_ptr++ = ch;
	tty->flip.count++;
	queue_task(&tty->flip.tqueue, &tq_timer);
}


static inline void sx_receive(struct specialix_board * bp)
{
	struct specialix_port *port;
	struct tty_struct *tty;
	unsigned char count;
	
	if (!(port = sx_get_port(bp, "Receive")))
		return;
	
	tty = port->tty;
	
	count = sx_in(bp, CD186x_RDCR);
	
#ifdef SX_REPORT_FIFO
	port->hits[count > 8 ? 9 : count]++;
#endif	
	
	while (count--) {
		if (tty->flip.count >= TTY_FLIPBUF_SIZE) {
			printk(KERN_INFO "sx%d: port %d: Working around flip buffer overflow.\n",
			       board_No(bp), port_No(port));
			break;
		}
		*tty->flip.char_buf_ptr++ = sx_in(bp, CD186x_RDR);
		*tty->flip.flag_buf_ptr++ = 0;
		tty->flip.count++;
	}
	queue_task(&tty->flip.tqueue, &tq_timer);
}


static inline void sx_transmit(struct specialix_board * bp)
{
	struct specialix_port *port;
	struct tty_struct *tty;
	unsigned char count;
	
	
	if (!(port = sx_get_port(bp, "Transmit")))
		return;
	
	tty = port->tty;
	
	if (port->IER & IER_TXEMPTY) {
		/* FIFO drained */
		sx_out(bp, CD186x_CAR, port_No(port));
		port->IER &= ~IER_TXEMPTY;
		sx_out(bp, CD186x_IER, port->IER);
		return;
	}
	
	if ((port->xmit_cnt <= 0 && !port->break_length)
	    || tty->stopped || tty->hw_stopped) {
		sx_out(bp, CD186x_CAR, port_No(port));
		port->IER &= ~IER_TXRDY;
		sx_out(bp, CD186x_IER, port->IER);
		return;
	}
	
	if (port->break_length) {
		if (port->break_length > 0) {
			if (port->COR2 & COR2_ETC) {
				sx_out(bp, CD186x_TDR, CD186x_C_ESC);
				sx_out(bp, CD186x_TDR, CD186x_C_SBRK);
				port->COR2 &= ~COR2_ETC;
			}
			count = MIN(port->break_length, 0xff);
			sx_out(bp, CD186x_TDR, CD186x_C_ESC);
			sx_out(bp, CD186x_TDR, CD186x_C_DELAY);
			sx_out(bp, CD186x_TDR, count);
			if (!(port->break_length -= count))
				port->break_length--;
		} else {
			sx_out(bp, CD186x_TDR, CD186x_C_ESC);
			sx_out(bp, CD186x_TDR, CD186x_C_EBRK);
			sx_out(bp, CD186x_COR2, port->COR2);
			sx_wait_CCR(bp);
			sx_out(bp, CD186x_CCR, CCR_CORCHG2);
			port->break_length = 0;
		}
		return;
	}
	
	count = CD186x_NFIFO;
	do {
		sx_out(bp, CD186x_TDR, port->xmit_buf[port->xmit_tail++]);
		port->xmit_tail = port->xmit_tail & (SERIAL_XMIT_SIZE-1);
		if (--port->xmit_cnt <= 0)
			break;
	} while (--count > 0);
	
	if (port->xmit_cnt <= 0) {
		sx_out(bp, CD186x_CAR, port_No(port));
		port->IER &= ~IER_TXRDY;
		sx_out(bp, CD186x_IER, port->IER);
	}
	if (port->xmit_cnt <= port->wakeup_chars)
		sx_mark_event(port, RS_EVENT_WRITE_WAKEUP);
}


static inline void sx_check_modem(struct specialix_board * bp)
{
	struct specialix_port *port;
	struct tty_struct *tty;
	unsigned char mcr;

#ifdef SPECIALIX_DEBUG
	printk (KERN_DEBUG "Modem intr. ");
#endif
	if (!(port = sx_get_port(bp, "Modem")))
		return;
	
	tty = port->tty;
	
	mcr = sx_in(bp, CD186x_MCR);
	printk ("mcr = %02x.\n", mcr);

	if ((mcr & MCR_CDCHG)) {
#ifdef SPECIALIX_DEBUG 
		printk (KERN_DEBUG "CD just changed... ");
#endif
		if (sx_in(bp, CD186x_MSVR) & MSVR_CD) {
#ifdef SPECIALIX_DEBUG
			printk ( "Waking up guys in open.\n");
#endif
			wake_up_interruptible(&port->open_wait);
		}
		else if (!((port->flags & ASYNC_CALLOUT_ACTIVE) &&
		           (port->flags & ASYNC_CALLOUT_NOHUP))) {
#ifdef SPECIALIX_DEBUG
			printk ( "Sending HUP.\n");
#endif
			MOD_INC_USE_COUNT;
			if (schedule_task(&port->tqueue_hangup) == 0)
				MOD_DEC_USE_COUNT;
		} else {
#ifdef SPECIALIX_DEBUG
			printk ( "Don't need to send HUP.\n");
#endif
		}
	}
	
#ifdef SPECIALIX_BRAIN_DAMAGED_CTS
	if (mcr & MCR_CTSCHG) {
		if (sx_in(bp, CD186x_MSVR) & MSVR_CTS) {
			tty->hw_stopped = 0;
			port->IER |= IER_TXRDY;
			if (port->xmit_cnt <= port->wakeup_chars)
				sx_mark_event(port, RS_EVENT_WRITE_WAKEUP);
		} else {
			tty->hw_stopped = 1;
			port->IER &= ~IER_TXRDY;
		}
		sx_out(bp, CD186x_IER, port->IER);
	}
	if (mcr & MCR_DSSXHG) {
		if (sx_in(bp, CD186x_MSVR) & MSVR_DSR) {
			tty->hw_stopped = 0;
			port->IER |= IER_TXRDY;
			if (port->xmit_cnt <= port->wakeup_chars)
				sx_mark_event(port, RS_EVENT_WRITE_WAKEUP);
		} else {
			tty->hw_stopped = 1;
			port->IER &= ~IER_TXRDY;
		}
		sx_out(bp, CD186x_IER, port->IER);
	}
#endif /* SPECIALIX_BRAIN_DAMAGED_CTS */
	
	/* Clear change bits */
	sx_out(bp, CD186x_MCR, 0);
}


/* The main interrupt processing routine */
static void sx_interrupt(int irq, void * dev_id, struct pt_regs * regs)
{
	unsigned char status;
	unsigned char ack;
	struct specialix_board *bp;
	unsigned long loop = 0;
	int saved_reg;

	bp = dev_id;
	
	if (!bp || !(bp->flags & SX_BOARD_ACTIVE)) {
#ifdef SPECIALIX_DEBUG 
		printk (KERN_DEBUG "sx: False interrupt. irq %d.\n", irq);
#endif
		return;
	}

	saved_reg = bp->reg;

	while ((++loop < 16) && (status = (sx_in(bp, CD186x_SRSR) &
	                                   (SRSR_RREQint |
		                            SRSR_TREQint |
	                                    SRSR_MREQint)))) {	
		if (status & SRSR_RREQint) {
			ack = sx_in(bp, CD186x_RRAR);

			if (ack == (SX_ID | GIVR_IT_RCV))
				sx_receive(bp);
			else if (ack == (SX_ID | GIVR_IT_REXC))
				sx_receive_exc(bp);
			else
				printk(KERN_ERR "sx%d: Bad receive ack 0x%02x.\n",
				       board_No(bp), ack);
		
		} else if (status & SRSR_TREQint) {
			ack = sx_in(bp, CD186x_TRAR);

			if (ack == (SX_ID | GIVR_IT_TX))
				sx_transmit(bp);
			else
				printk(KERN_ERR "sx%d: Bad transmit ack 0x%02x.\n",
				       board_No(bp), ack);
		} else if (status & SRSR_MREQint) {
			ack = sx_in(bp, CD186x_MRAR);

			if (ack == (SX_ID | GIVR_IT_MODEM)) 
				sx_check_modem(bp);
			else
				printk(KERN_ERR "sx%d: Bad modem ack 0x%02x.\n",
				       board_No(bp), ack);
		
		} 

		sx_out(bp, CD186x_EOIR, 0);   /* Mark end of interrupt */
	}
	bp->reg = saved_reg;
	outb (bp->reg, bp->base + SX_ADDR_REG);
}


/*
 *  Routines for open & close processing.
 */

void turn_ints_off (struct specialix_board *bp)
{
	if (bp->flags & SX_BOARD_IS_PCI) {
		/* This was intended for enabeling the interrupt on the
		 * PCI card. However it seems that it's already enabled
		 * and as PCI interrupts can be shared, there is no real
		 * reason to have to turn it off. */
	}
	(void) sx_in_off (bp, 0); /* Turn off interrupts. */
}

void turn_ints_on (struct specialix_board *bp)
{
	if (bp->flags & SX_BOARD_IS_PCI) {
		/* play with the PCI chip. See comment above. */
	}
	(void) sx_in (bp, 0); /* Turn ON interrupts. */
}


/* Called with disabled interrupts */
static inline int sx_setup_board(struct specialix_board * bp)
{
	int error;

	if (bp->flags & SX_BOARD_ACTIVE) 
		return 0;

	if (bp->flags & SX_BOARD_IS_PCI)
		error = request_irq(bp->irq, sx_interrupt, SA_INTERRUPT | SA_SHIRQ, "specialix IO8+", bp);
	else
		error = request_irq(bp->irq, sx_interrupt, SA_INTERRUPT, "specialix IO8+", bp);

	if (error) 
		return error;

	turn_ints_on (bp);
	bp->flags |= SX_BOARD_ACTIVE;

	MOD_INC_USE_COUNT;
	return 0;
}


/* Called with disabled interrupts */
static inline void sx_shutdown_board(struct specialix_board *bp)
{
	if (!(bp->flags & SX_BOARD_ACTIVE))
		return;
	
	bp->flags &= ~SX_BOARD_ACTIVE;
	
#if SPECIALIX_DEBUG > 2
	printk ("Freeing IRQ%d for board %d.\n", bp->irq, board_No (bp));
#endif
	free_irq(bp->irq, bp);

	turn_ints_off (bp);

	MOD_DEC_USE_COUNT;
}


/*
 * Setting up port characteristics. 
 * Must be called with disabled interrupts
 */
static void sx_change_speed(struct specialix_board *bp, struct specialix_port *port)
{
	struct tty_struct *tty;
	unsigned long baud;
	long tmp;
	unsigned char cor1 = 0, cor3 = 0;
	unsigned char mcor1 = 0, mcor2 = 0;
	static int again;
	
	if (!(tty = port->tty) || !tty->termios)
		return;

	port->IER  = 0;
	port->COR2 = 0;
	/* Select port on the board */
	sx_out(bp, CD186x_CAR, port_No(port));

	/* The Specialix board doens't implement the RTS lines.
	   They are used to set the IRQ level. Don't touch them. */
	if (SX_CRTSCTS(tty))
		port->MSVR = MSVR_DTR | (sx_in(bp, CD186x_MSVR) & MSVR_RTS);
	else
		port->MSVR =  (sx_in(bp, CD186x_MSVR) & MSVR_RTS);
#ifdef DEBUG_SPECIALIX
	printk (KERN_DEBUG "sx: got MSVR=%02x.\n", port->MSVR);
#endif
	baud = C_BAUD(tty);
	
	if (baud & CBAUDEX) {
		baud &= ~CBAUDEX;
		if (baud < 1 || baud > 2) 
			port->tty->termios->c_cflag &= ~CBAUDEX;
		else
			baud += 15;
	}
	if (baud == 15) {
		if ((port->flags & ASYNC_SPD_MASK) == ASYNC_SPD_HI)
			baud ++;
		if ((port->flags & ASYNC_SPD_MASK) == ASYNC_SPD_VHI)
			baud += 2;
	}
	
	
	if (!baud_table[baud]) {
		/* Drop DTR & exit */
#ifdef SPECIALIX_DEBUG
		printk (KERN_DEBUG "Dropping DTR...  Hmm....\n");
#endif
		if (!SX_CRTSCTS (tty)) {
			port -> MSVR &= ~ MSVR_DTR;
			sx_out(bp, CD186x_MSVR, port->MSVR );
		} 
#ifdef DEBUG_SPECIALIX
		else
			printk (KERN_DEBUG "Can't drop DTR: no DTR.\n");
#endif
		return;
	} else {
		/* Set DTR on */
		if (!SX_CRTSCTS (tty)) {
			port ->MSVR |= MSVR_DTR;
		}
	}
	
	/*
	 * Now we must calculate some speed depended things 
	 */

	/* Set baud rate for port */
	tmp = port->custom_divisor ;
	if ( tmp )
		printk (KERN_INFO "sx%d: Using custom baud rate divisor %ld. \n"
		                  "This is an untested option, please be carefull.\n",
		                  port_No (port), tmp);
	else
		tmp = (((SX_OSCFREQ + baud_table[baud]/2) / baud_table[baud] +
		         CD186x_TPC/2) / CD186x_TPC);

	if ((tmp < 0x10) && time_before(again, jiffies)) { 
		again = jiffies + HZ * 60;
		/* Page 48 of version 2.0 of the CL-CD1865 databook */
		if (tmp >= 12) {
			printk (KERN_INFO "sx%d: Baud rate divisor is %ld. \n"
			        "Performance degradation is possible.\n"
			        "Read specialix.txt for more info.\n",
			        port_No (port), tmp);
		} else {
			printk (KERN_INFO "sx%d: Baud rate divisor is %ld. \n"
			        "Warning: overstressing Cirrus chip. "
			        "This might not work.\n"
			        "Read specialix.txt for more info.\n", 
			        port_No (port), tmp);
		}
	}

	sx_out(bp, CD186x_RBPRH, (tmp >> 8) & 0xff); 
	sx_out(bp, CD186x_TBPRH, (tmp >> 8) & 0xff); 
	sx_out(bp, CD186x_RBPRL, tmp & 0xff); 
	sx_out(bp, CD186x_TBPRL, tmp & 0xff);

	if (port->custom_divisor) {
		baud = (SX_OSCFREQ + port->custom_divisor/2) / port->custom_divisor;
		baud = ( baud + 5 ) / 10;
	} else 
		baud = (baud_table[baud] + 5) / 10;   /* Estimated CPS */

	/* Two timer ticks seems enough to wakeup something like SLIP driver */
	tmp = ((baud + HZ/2) / HZ) * 2 - CD186x_NFIFO;		
	port->wakeup_chars = (tmp < 0) ? 0 : ((tmp >= SERIAL_XMIT_SIZE) ?
					      SERIAL_XMIT_SIZE - 1 : tmp);
	
	/* Receiver timeout will be transmission time for 1.5 chars */
	tmp = (SPECIALIX_TPS + SPECIALIX_TPS/2 + baud/2) / baud;
	tmp = (tmp > 0xff) ? 0xff : tmp;
	sx_out(bp, CD186x_RTPR, tmp);
	
	switch (C_CSIZE(tty)) {
	 case CS5:
		cor1 |= COR1_5BITS;
		break;
	 case CS6:
		cor1 |= COR1_6BITS;
		break;
	 case CS7:
		cor1 |= COR1_7BITS;
		break;
	 case CS8:
		cor1 |= COR1_8BITS;
		break;
	}
	
	if (C_CSTOPB(tty)) 
		cor1 |= COR1_2SB;
	
	cor1 |= COR1_IGNORE;
	if (C_PARENB(tty)) {
		cor1 |= COR1_NORMPAR;
		if (C_PARODD(tty)) 
			cor1 |= COR1_ODDP;
		if (I_INPCK(tty)) 
			cor1 &= ~COR1_IGNORE;
	}
	/* Set marking of some errors */
	port->mark_mask = RCSR_OE | RCSR_TOUT;
	if (I_INPCK(tty)) 
		port->mark_mask |= RCSR_FE | RCSR_PE;
	if (I_BRKINT(tty) || I_PARMRK(tty)) 
		port->mark_mask |= RCSR_BREAK;
	if (I_IGNPAR(tty)) 
		port->mark_mask &= ~(RCSR_FE | RCSR_PE);
	if (I_IGNBRK(tty)) {
		port->mark_mask &= ~RCSR_BREAK;
		if (I_IGNPAR(tty)) 
			/* Real raw mode. Ignore all */
			port->mark_mask &= ~RCSR_OE;
	}
	/* Enable Hardware Flow Control */
	if (C_CRTSCTS(tty)) {
#ifdef SPECIALIX_BRAIN_DAMAGED_CTS
		port->IER |= IER_DSR | IER_CTS;
		mcor1 |= MCOR1_DSRZD | MCOR1_CTSZD;
		mcor2 |= MCOR2_DSROD | MCOR2_CTSOD;
		tty->hw_stopped = !(sx_in(bp, CD186x_MSVR) & (MSVR_CTS|MSVR_DSR));
#else
		port->COR2 |= COR2_CTSAE; 
#endif
	}
	/* Enable Software Flow Control. FIXME: I'm not sure about this */
	/* Some people reported that it works, but I still doubt it */
	if (I_IXON(tty)) {
		port->COR2 |= COR2_TXIBE;
		cor3 |= (COR3_FCT | COR3_SCDE);
		if (I_IXANY(tty))
			port->COR2 |= COR2_IXM;
		sx_out(bp, CD186x_SCHR1, START_CHAR(tty));
		sx_out(bp, CD186x_SCHR2, STOP_CHAR(tty));
		sx_out(bp, CD186x_SCHR3, START_CHAR(tty));
		sx_out(bp, CD186x_SCHR4, STOP_CHAR(tty));
	}
	if (!C_CLOCAL(tty)) {
		/* Enable CD check */
		port->IER |= IER_CD;
		mcor1 |= MCOR1_CDZD;
		mcor2 |= MCOR2_CDOD;
	}
	
	if (C_CREAD(tty)) 
		/* Enable receiver */
		port->IER |= IER_RXD;
	
	/* Set input FIFO size (1-8 bytes) */
	cor3 |= SPECIALIX_RXFIFO; 
	/* Setting up CD186x channel registers */
	sx_out(bp, CD186x_COR1, cor1);
	sx_out(bp, CD186x_COR2, port->COR2);
	sx_out(bp, CD186x_COR3, cor3);
	/* Make CD186x know about registers change */
	sx_wait_CCR(bp);
	sx_out(bp, CD186x_CCR, CCR_CORCHG1 | CCR_CORCHG2 | CCR_CORCHG3);
	/* Setting up modem option registers */
#ifdef DEBUG_SPECIALIX
	printk ("Mcor1 = %02x, mcor2 = %02x.\n", mcor1, mcor2);
#endif
	sx_out(bp, CD186x_MCOR1, mcor1);
	sx_out(bp, CD186x_MCOR2, mcor2);
	/* Enable CD186x transmitter & receiver */
	sx_wait_CCR(bp);
	sx_out(bp, CD186x_CCR, CCR_TXEN | CCR_RXEN);
	/* Enable interrupts */
	sx_out(bp, CD186x_IER, port->IER);
	/* And finally set the modem lines... */
	sx_out(bp, CD186x_MSVR, port->MSVR);
}


/* Must be called with interrupts enabled */
static int sx_setup_port(struct specialix_board *bp, struct specialix_port *port)
{
	unsigned long flags;
	
	if (port->flags & ASYNC_INITIALIZED)
		return 0;
	
	if (!port->xmit_buf) {
		/* We may sleep in get_free_page() */
		unsigned long tmp;
		
		if (!(tmp = get_free_page(GFP_KERNEL)))
			return -ENOMEM;

		if (port->xmit_buf) {
			free_page(tmp);
			return -ERESTARTSYS;
		}
		port->xmit_buf = (unsigned char *) tmp;
	}
		
	save_flags(flags); cli();
		
	if (port->tty) 
		clear_bit(TTY_IO_ERROR, &port->tty->flags);
		
	if (port->count == 1) 
		bp->count++;
		
	port->xmit_cnt = port->xmit_head = port->xmit_tail = 0;
	sx_change_speed(bp, port);
	port->flags |= ASYNC_INITIALIZED;
		
	restore_flags(flags);
	return 0;
}


/* Must be called with interrupts disabled */
static void sx_shutdown_port(struct specialix_board *bp, struct specialix_port *port)
{
	struct tty_struct *tty;
	
	if (!(port->flags & ASYNC_INITIALIZED)) 
		return;
	
#ifdef SX_REPORT_OVERRUN
	printk(KERN_INFO "sx%d: port %d: Total %ld overruns were detected.\n",
	       board_No(bp), port_No(port), port->overrun);
#endif	
#ifdef SX_REPORT_FIFO
	{
		int i;
		
		printk(KERN_INFO "sx%d: port %d: FIFO hits [ ",
		       board_No(bp), port_No(port));
		for (i = 0; i < 10; i++) {
			printk("%ld ", port->hits[i]);
		}
		printk("].\n");
	}
#endif	
	if (port->xmit_buf) {
		free_page((unsigned long) port->xmit_buf);
		port->xmit_buf = NULL;
	}

	/* Select port */
	sx_out(bp, CD186x_CAR, port_No(port));

	if (!(tty = port->tty) || C_HUPCL(tty)) {
		/* Drop DTR */
		sx_out(bp, CD186x_MSVDTR, 0);
	}
	
	/* Reset port */
	sx_wait_CCR(bp);
	sx_out(bp, CD186x_CCR, CCR_SOFTRESET);
	/* Disable all interrupts from this port */
	port->IER = 0;
	sx_out(bp, CD186x_IER, port->IER);
	
	if (tty)
		set_bit(TTY_IO_ERROR, &tty->flags);
	port->flags &= ~ASYNC_INITIALIZED;
	
	if (--bp->count < 0) {
		printk(KERN_ERR "sx%d: sx_shutdown_port: bad board count: %d\n",
		       board_No(bp), bp->count);
		bp->count = 0;
	}
	
	/*
	 * If this is the last opened port on the board
	 * shutdown whole board
	 */
	if (!bp->count) 
		sx_shutdown_board(bp);
}

	
static int block_til_ready(struct tty_struct *tty, struct file * filp,
                           struct specialix_port *port)
{
	DECLARE_WAITQUEUE(wait,  current);
	struct specialix_board *bp = port_Board(port);
	int    retval;
	int    do_clocal = 0;
	int    CD;

	/*
	 * If the device is in the middle of being closed, then block
	 * until it's done, and then try again.
	 */
	if (tty_hung_up_p(filp) || port->flags & ASYNC_CLOSING) {
		interruptible_sleep_on(&port->close_wait);
		if (port->flags & ASYNC_HUP_NOTIFY)
			return -EAGAIN;
		else
			return -ERESTARTSYS;
	}

	/*
	 * If this is a callout device, then just make sure the normal
	 * device isn't being used.
	 */
	if (tty->driver.subtype == SPECIALIX_TYPE_CALLOUT) {
		if (port->flags & ASYNC_NORMAL_ACTIVE)
			return -EBUSY;
		if ((port->flags & ASYNC_CALLOUT_ACTIVE) &&
		    (port->flags & ASYNC_SESSION_LOCKOUT) &&
		    (port->session != current->session))
			return -EBUSY;
		if ((port->flags & ASYNC_CALLOUT_ACTIVE) &&
		    (port->flags & ASYNC_PGRP_LOCKOUT) &&
		    (port->pgrp != current->pgrp))
			return -EBUSY;
		port->flags |= ASYNC_CALLOUT_ACTIVE;
		return 0;
	}
	
	/*
	 * If non-blocking mode is set, or the port is not enabled,
	 * then make the check up front and then exit.
	 */
	if ((filp->f_flags & O_NONBLOCK) ||
	    (tty->flags & (1 << TTY_IO_ERROR))) {
		if (port->flags & ASYNC_CALLOUT_ACTIVE)
			return -EBUSY;
		port->flags |= ASYNC_NORMAL_ACTIVE;
		return 0;
	}

	if (port->flags & ASYNC_CALLOUT_ACTIVE) {
		if (port->normal_termios.c_cflag & CLOCAL) 
			do_clocal = 1;
	} else {
		if (C_CLOCAL(tty))
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
	add_wait_queue(&port->open_wait, &wait);
	cli();
	if (!tty_hung_up_p(filp))
		port->count--;
	sti();
	port->blocked_open++;
	while (1) {
		cli();
		sx_out(bp, CD186x_CAR, port_No(port));
		CD = sx_in(bp, CD186x_MSVR) & MSVR_CD;
		if (!(port->flags & ASYNC_CALLOUT_ACTIVE)) {
			if (SX_CRTSCTS (tty)) {
				/* Activate RTS */
				port->MSVR |= MSVR_DTR;
				sx_out (bp, CD186x_MSVR, port->MSVR);
			} else {
				/* Activate DTR */
				port->MSVR |= MSVR_DTR;
				sx_out (bp, CD186x_MSVR, port->MSVR);
			} 
		}
		sti();
		set_current_state(TASK_INTERRUPTIBLE);
		if (tty_hung_up_p(filp) ||
		    !(port->flags & ASYNC_INITIALIZED)) {
			if (port->flags & ASYNC_HUP_NOTIFY)
				retval = -EAGAIN;
			else
				retval = -ERESTARTSYS;	
			break;
		}
		if (!(port->flags & ASYNC_CALLOUT_ACTIVE) &&
		    !(port->flags & ASYNC_CLOSING) &&
		    (do_clocal || CD))
			break;
		if (signal_pending(current)) {
			retval = -ERESTARTSYS;
			break;
		}
		schedule();
	}
	current->state = TASK_RUNNING;
	remove_wait_queue(&port->open_wait, &wait);
	if (!tty_hung_up_p(filp))
		port->count++;
	port->blocked_open--;
	if (retval)
		return retval;
	
	port->flags |= ASYNC_NORMAL_ACTIVE;
	return 0;
}	


static int sx_open(struct tty_struct * tty, struct file * filp)
{
	int board;
	int error;
	struct specialix_port * port;
	struct specialix_board * bp;
	unsigned long flags;
	
	board = SX_BOARD(MINOR(tty->device));

	if (board > SX_NBOARD || !(sx_board[board].flags & SX_BOARD_PRESENT))
		return -ENODEV;
	
	bp = &sx_board[board];
	port = sx_port + board * SX_NPORT + SX_PORT(MINOR(tty->device));

#ifdef DEBUG_SPECIALIX
	printk (KERN_DEBUG "Board = %d, bp = %p, port = %p, portno = %d.\n", 
	        board, bp, port, SX_PORT(MINOR(tty->device)));
#endif

	if (sx_paranoia_check(port, tty->device, "sx_open"))
		return -ENODEV;

	if ((error = sx_setup_board(bp)))
		return error;

	port->count++;
	tty->driver_data = port;
	port->tty = tty;

	if ((error = sx_setup_port(bp, port))) 
		return error;
	
	if ((error = block_til_ready(tty, filp, port)))
		return error;

	if ((port->count == 1) && (port->flags & ASYNC_SPLIT_TERMIOS)) {
		if (tty->driver.subtype == SPECIALIX_TYPE_NORMAL)
			*tty->termios = port->normal_termios;
		else
			*tty->termios = port->callout_termios;
		save_flags(flags); cli();
		sx_change_speed(bp, port);
		restore_flags(flags);
	}

	port->session = current->session;
	port->pgrp = current->pgrp;
	return 0;
}


static void sx_close(struct tty_struct * tty, struct file * filp)
{
	struct specialix_port *port = (struct specialix_port *) tty->driver_data;
	struct specialix_board *bp;
	unsigned long flags;
	unsigned long timeout;
	
	if (!port || sx_paranoia_check(port, tty->device, "close"))
		return;
	
	save_flags(flags); cli();
	if (tty_hung_up_p(filp)) {
		restore_flags(flags);
		return;
	}
	
	bp = port_Board(port);
	if ((tty->count == 1) && (port->count != 1)) {
		printk(KERN_ERR "sx%d: sx_close: bad port count;"
		       " tty->count is 1, port count is %d\n",
		       board_No(bp), port->count);
		port->count = 1;
	}
	if (--port->count < 0) {
		printk(KERN_ERR "sx%d: sx_close: bad port count for tty%d: %d\n",
		       board_No(bp), port_No(port), port->count);
		port->count = 0;
	}
	if (port->count) {
		restore_flags(flags);
		return;
	}
	port->flags |= ASYNC_CLOSING;
	/*
	 * Save the termios structure, since this port may have
	 * separate termios for callout and dialin.
	 */
	if (port->flags & ASYNC_NORMAL_ACTIVE)
		port->normal_termios = *tty->termios;
	if (port->flags & ASYNC_CALLOUT_ACTIVE)
		port->callout_termios = *tty->termios;
	/*
	 * Now we wait for the transmit buffer to clear; and we notify 
	 * the line discipline to only process XON/XOFF characters.
	 */
	tty->closing = 1;
	if (port->closing_wait != ASYNC_CLOSING_WAIT_NONE)
		tty_wait_until_sent(tty, port->closing_wait);
	/*
	 * At this point we stop accepting input.  To do this, we
	 * disable the receive line status interrupts, and tell the
	 * interrupt driver to stop checking the data ready bit in the
	 * line status register.
	 */
	port->IER &= ~IER_RXD;
	if (port->flags & ASYNC_INITIALIZED) {
		port->IER &= ~IER_TXRDY;
		port->IER |= IER_TXEMPTY;
		sx_out(bp, CD186x_CAR, port_No(port));
		sx_out(bp, CD186x_IER, port->IER);
		/*
		 * Before we drop DTR, make sure the UART transmitter
		 * has completely drained; this is especially
		 * important if there is a transmit FIFO!
		 */
		timeout = jiffies+HZ;
		while(port->IER & IER_TXEMPTY) {
			current->state = TASK_INTERRUPTIBLE;
 			schedule_timeout(port->timeout);
			if (time_after(jiffies, timeout)) {
				printk (KERN_INFO "Timeout waiting for close\n");
				break;
			}
		}

	}
	sx_shutdown_port(bp, port);
	if (tty->driver.flush_buffer)
		tty->driver.flush_buffer(tty);
	if (tty->ldisc.flush_buffer)
		tty->ldisc.flush_buffer(tty);
	tty->closing = 0;
	port->event = 0;
	port->tty = 0;
	if (port->blocked_open) {
		if (port->close_delay) {
			current->state = TASK_INTERRUPTIBLE;
			schedule_timeout(port->close_delay);
		}
		wake_up_interruptible(&port->open_wait);
	}
	port->flags &= ~(ASYNC_NORMAL_ACTIVE|ASYNC_CALLOUT_ACTIVE|
			 ASYNC_CLOSING);
	wake_up_interruptible(&port->close_wait);
	restore_flags(flags);
}


static int sx_write(struct tty_struct * tty, int from_user, 
                    const unsigned char *buf, int count)
{
	struct specialix_port *port = (struct specialix_port *)tty->driver_data;
	struct specialix_board *bp;
	int c, total = 0;
	unsigned long flags;
				
	if (sx_paranoia_check(port, tty->device, "sx_write"))
		return 0;
	
	bp = port_Board(port);

	if (!tty || !port->xmit_buf || !tmp_buf)
		return 0;

	save_flags(flags);
	if (from_user) {
		down(&tmp_buf_sem);
		while (1) {
			c = MIN(count, MIN(SERIAL_XMIT_SIZE - port->xmit_cnt - 1,
					   SERIAL_XMIT_SIZE - port->xmit_head));
			if (c <= 0)
				break;

			c -= copy_from_user(tmp_buf, buf, c);
			if (!c) {
				if (!total)
					total = -EFAULT;
				break;
			}

			cli();
			c = MIN(c, MIN(SERIAL_XMIT_SIZE - port->xmit_cnt - 1,
				       SERIAL_XMIT_SIZE - port->xmit_head));
			memcpy(port->xmit_buf + port->xmit_head, tmp_buf, c);
			port->xmit_head = (port->xmit_head + c) & (SERIAL_XMIT_SIZE-1);
			port->xmit_cnt += c;
			restore_flags(flags);

			buf += c;
			count -= c;
			total += c;
		}
		up(&tmp_buf_sem);
	} else {
		while (1) {
			cli();
			c = MIN(count, MIN(SERIAL_XMIT_SIZE - port->xmit_cnt - 1,
					   SERIAL_XMIT_SIZE - port->xmit_head));
			if (c <= 0) {
				restore_flags(flags);
				break;
			}
			memcpy(port->xmit_buf + port->xmit_head, buf, c);
			port->xmit_head = (port->xmit_head + c) & (SERIAL_XMIT_SIZE-1);
			port->xmit_cnt += c;
			restore_flags(flags);

			buf += c;
			count -= c;
			total += c;
		}
	}

	cli();
	if (port->xmit_cnt && !tty->stopped && !tty->hw_stopped &&
	    !(port->IER & IER_TXRDY)) {
		port->IER |= IER_TXRDY;
		sx_out(bp, CD186x_CAR, port_No(port));
		sx_out(bp, CD186x_IER, port->IER);
	}
	restore_flags(flags);
	return total;
}


static void sx_put_char(struct tty_struct * tty, unsigned char ch)
{
	struct specialix_port *port = (struct specialix_port *)tty->driver_data;
	unsigned long flags;

	if (sx_paranoia_check(port, tty->device, "sx_put_char"))
		return;

	if (!tty || !port->xmit_buf)
		return;

	save_flags(flags); cli();
	
	if (port->xmit_cnt >= SERIAL_XMIT_SIZE - 1) {
		restore_flags(flags);
		return;
	}

	port->xmit_buf[port->xmit_head++] = ch;
	port->xmit_head &= SERIAL_XMIT_SIZE - 1;
	port->xmit_cnt++;
	restore_flags(flags);
}


static void sx_flush_chars(struct tty_struct * tty)
{
	struct specialix_port *port = (struct specialix_port *)tty->driver_data;
	unsigned long flags;
				
	if (sx_paranoia_check(port, tty->device, "sx_flush_chars"))
		return;
	
	if (port->xmit_cnt <= 0 || tty->stopped || tty->hw_stopped ||
	    !port->xmit_buf)
		return;

	save_flags(flags); cli();
	port->IER |= IER_TXRDY;
	sx_out(port_Board(port), CD186x_CAR, port_No(port));
	sx_out(port_Board(port), CD186x_IER, port->IER);
	restore_flags(flags);
}


static int sx_write_room(struct tty_struct * tty)
{
	struct specialix_port *port = (struct specialix_port *)tty->driver_data;
	int	ret;
				
	if (sx_paranoia_check(port, tty->device, "sx_write_room"))
		return 0;

	ret = SERIAL_XMIT_SIZE - port->xmit_cnt - 1;
	if (ret < 0)
		ret = 0;
	return ret;
}


static int sx_chars_in_buffer(struct tty_struct *tty)
{
	struct specialix_port *port = (struct specialix_port *)tty->driver_data;
				
	if (sx_paranoia_check(port, tty->device, "sx_chars_in_buffer"))
		return 0;
	
	return port->xmit_cnt;
}


static void sx_flush_buffer(struct tty_struct *tty)
{
	struct specialix_port *port = (struct specialix_port *)tty->driver_data;
	unsigned long flags;
				
	if (sx_paranoia_check(port, tty->device, "sx_flush_buffer"))
		return;

	save_flags(flags); cli();
	port->xmit_cnt = port->xmit_head = port->xmit_tail = 0;
	restore_flags(flags);
	
	wake_up_interruptible(&tty->write_wait);
	if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) &&
	    tty->ldisc.write_wakeup)
		(tty->ldisc.write_wakeup)(tty);
}


static int sx_get_modem_info(struct specialix_port * port, unsigned int *value)
{
	struct specialix_board * bp;
	unsigned char status;
	unsigned int result;
	unsigned long flags;

	bp = port_Board(port);
	save_flags(flags); cli();
	sx_out(bp, CD186x_CAR, port_No(port));
	status = sx_in(bp, CD186x_MSVR);
	restore_flags(flags);
#ifdef DEBUG_SPECIALIX
	printk (KERN_DEBUG "Got msvr[%d] = %02x, car = %d.\n", 
		port_No(port), status, sx_in (bp, CD186x_CAR));
	printk (KERN_DEBUG "sx_port = %p, port = %p\n", sx_port, port);
#endif
	if (SX_CRTSCTS(port->tty)) {
		result  = /*   (status & MSVR_RTS) ? */ TIOCM_DTR /* : 0) */ 
		          |   ((status & MSVR_DTR) ? TIOCM_RTS : 0)
		          |   ((status & MSVR_CD)  ? TIOCM_CAR : 0)
		          |/* ((status & MSVR_DSR) ? */ TIOCM_DSR /* : 0) */
		          |   ((status & MSVR_CTS) ? TIOCM_CTS : 0);
	} else {
		result  = /*   (status & MSVR_RTS) ? */ TIOCM_RTS /* : 0) */ 
		          |   ((status & MSVR_DTR) ? TIOCM_DTR : 0)
		          |   ((status & MSVR_CD)  ? TIOCM_CAR : 0)
		          |/* ((status & MSVR_DSR) ? */ TIOCM_DSR /* : 0) */
		          |   ((status & MSVR_CTS) ? TIOCM_CTS : 0);
	}
	put_user(result,(unsigned int *) value);
	return 0;
}


static int sx_set_modem_info(struct specialix_port * port, unsigned int cmd,
                             unsigned int *value)
{
	int error;
	unsigned int arg;
	unsigned long flags;
	struct specialix_board *bp = port_Board(port);

	error = verify_area(VERIFY_READ, value, sizeof(int));
	if (error) 
		return error;

	Get_user(arg, (unsigned long *) value);
	switch (cmd) {
	case TIOCMBIS: 
	   /*	if (arg & TIOCM_RTS) 
			port->MSVR |= MSVR_RTS; */
	   /*   if (arg & TIOCM_DTR)
			port->MSVR |= MSVR_DTR; */

		if (SX_CRTSCTS(port->tty)) {
			if (arg & TIOCM_RTS)
				port->MSVR |= MSVR_DTR; 
		} else {
			if (arg & TIOCM_DTR)
				port->MSVR |= MSVR_DTR; 
		}	     
		break;
	case TIOCMBIC:
	  /*	if (arg & TIOCM_RTS)
			port->MSVR &= ~MSVR_RTS; */
	  /*    if (arg & TIOCM_DTR)
			port->MSVR &= ~MSVR_DTR; */
		if (SX_CRTSCTS(port->tty)) {
			if (arg & TIOCM_RTS)
				port->MSVR &= ~MSVR_DTR;
		} else {
			if (arg & TIOCM_DTR)
				port->MSVR &= ~MSVR_DTR;
		}
		break;
	case TIOCMSET:
	  /* port->MSVR = (arg & TIOCM_RTS) ? (port->MSVR | MSVR_RTS) : 
						 (port->MSVR & ~MSVR_RTS); */
	  /* port->MSVR = (arg & TIOCM_DTR) ? (port->MSVR | MSVR_DTR) : 
						 (port->MSVR & ~MSVR_DTR); */
		if (SX_CRTSCTS(port->tty)) {
	  		port->MSVR = (arg & TIOCM_RTS) ? 
			                         (port->MSVR |  MSVR_DTR) : 
			                         (port->MSVR & ~MSVR_DTR);
		} else {
			port->MSVR = (arg & TIOCM_DTR) ?
			                         (port->MSVR |  MSVR_DTR):
			                         (port->MSVR & ~MSVR_DTR);
		}
		break;
	default:
		return -EINVAL;
	}
	save_flags(flags); cli();
	sx_out(bp, CD186x_CAR, port_No(port));
	sx_out(bp, CD186x_MSVR, port->MSVR);
	restore_flags(flags);
	return 0;
}


static inline void sx_send_break(struct specialix_port * port, unsigned long length)
{
	struct specialix_board *bp = port_Board(port);
	unsigned long flags;
	
	save_flags(flags); cli();
	port->break_length = SPECIALIX_TPS / HZ * length;
	port->COR2 |= COR2_ETC;
	port->IER  |= IER_TXRDY;
	sx_out(bp, CD186x_CAR, port_No(port));
	sx_out(bp, CD186x_COR2, port->COR2);
	sx_out(bp, CD186x_IER, port->IER);
	sx_wait_CCR(bp);
	sx_out(bp, CD186x_CCR, CCR_CORCHG2);
	sx_wait_CCR(bp);
	restore_flags(flags);
}


static inline int sx_set_serial_info(struct specialix_port * port,
                                     struct serial_struct * newinfo)
{
	struct serial_struct tmp;
	struct specialix_board *bp = port_Board(port);
	int change_speed;
	unsigned long flags;
	int error;
	
	error = verify_area(VERIFY_READ, (void *) newinfo, sizeof(tmp));
	if (error)
		return error;

	if (copy_from_user(&tmp, newinfo, sizeof(tmp)))
		return -EFAULT;
	
#if 0	
	if ((tmp.irq != bp->irq) ||
	    (tmp.port != bp->base) ||
	    (tmp.type != PORT_CIRRUS) ||
	    (tmp.baud_base != (SX_OSCFREQ + CD186x_TPC/2) / CD186x_TPC) ||
	    (tmp.custom_divisor != 0) ||
	    (tmp.xmit_fifo_size != CD186x_NFIFO) ||
	    (tmp.flags & ~SPECIALIX_LEGAL_FLAGS))
		return -EINVAL;
#endif	

	change_speed = ((port->flags & ASYNC_SPD_MASK) !=
			(tmp.flags & ASYNC_SPD_MASK));
	change_speed |= (tmp.custom_divisor != port->custom_divisor);
	
	if (!capable(CAP_SYS_ADMIN)) {
		if ((tmp.close_delay != port->close_delay) ||
		    (tmp.closing_wait != port->closing_wait) ||
		    ((tmp.flags & ~ASYNC_USR_MASK) !=
		     (port->flags & ~ASYNC_USR_MASK)))
			return -EPERM;
		port->flags = ((port->flags & ~ASYNC_USR_MASK) |
		                  (tmp.flags & ASYNC_USR_MASK));
		port->custom_divisor = tmp.custom_divisor;
	} else {
		port->flags = ((port->flags & ~ASYNC_FLAGS) |
		                  (tmp.flags & ASYNC_FLAGS));
		port->close_delay = tmp.close_delay;
		port->closing_wait = tmp.closing_wait;
		port->custom_divisor = tmp.custom_divisor;
	}
	if (change_speed) {
		save_flags(flags); cli();
		sx_change_speed(bp, port);
		restore_flags(flags);
	}
	return 0;
}


static inline int sx_get_serial_info(struct specialix_port * port,
				     struct serial_struct * retinfo)
{
	struct serial_struct tmp;
	struct specialix_board *bp = port_Board(port);
	int error;
	
	error = verify_area(VERIFY_WRITE, (void *) retinfo, sizeof(tmp));
	if (error)
		return error;

	memset(&tmp, 0, sizeof(tmp));
	tmp.type = PORT_CIRRUS;
	tmp.line = port - sx_port;
	tmp.port = bp->base;
	tmp.irq  = bp->irq;
	tmp.flags = port->flags;
	tmp.baud_base = (SX_OSCFREQ + CD186x_TPC/2) / CD186x_TPC;
	tmp.close_delay = port->close_delay * HZ/100;
	tmp.closing_wait = port->closing_wait * HZ/100;
	tmp.custom_divisor =  port->custom_divisor;
	tmp.xmit_fifo_size = CD186x_NFIFO;
	if (copy_to_user(retinfo, &tmp, sizeof(tmp)))
		return -EFAULT;
	return 0;
}


static int sx_ioctl(struct tty_struct * tty, struct file * filp, 
                    unsigned int cmd, unsigned long arg)
{
	struct specialix_port *port = (struct specialix_port *)tty->driver_data;
	int error;
	int retval;
				
	if (sx_paranoia_check(port, tty->device, "sx_ioctl"))
		return -ENODEV;
	
	switch (cmd) {
	 case TCSBRK:	/* SVID version: non-zero arg --> no break */
		retval = tty_check_change(tty);
		if (retval)
			return retval;
		tty_wait_until_sent(tty, 0);
		if (!arg)
			sx_send_break(port, HZ/4);	/* 1/4 second */
		return 0;
	 case TCSBRKP:	/* support for POSIX tcsendbreak() */
		retval = tty_check_change(tty);
		if (retval)
			return retval;
		tty_wait_until_sent(tty, 0);
		sx_send_break(port, arg ? arg*(HZ/10) : HZ/4);
		return 0;
	 case TIOCGSOFTCAR:
		error = verify_area(VERIFY_WRITE, (void *) arg, sizeof(long));
		if (error)
			return error;
		put_user(C_CLOCAL(tty) ? 1 : 0,
		         (unsigned long *) arg);
		return 0;
	 case TIOCSSOFTCAR:
		Get_user(arg, (unsigned long *) arg);
		tty->termios->c_cflag =
			((tty->termios->c_cflag & ~CLOCAL) |
			(arg ? CLOCAL : 0));
		return 0;
	 case TIOCMGET:
		error = verify_area(VERIFY_WRITE, (void *) arg,
		                    sizeof(unsigned int));
		if (error)
			return error;
		return sx_get_modem_info(port, (unsigned int *) arg);
	 case TIOCMBIS:
	 case TIOCMBIC:
	 case TIOCMSET:
		return sx_set_modem_info(port, cmd, (unsigned int *) arg);
	 case TIOCGSERIAL:	
		return sx_get_serial_info(port, (struct serial_struct *) arg);
	 case TIOCSSERIAL:	
		return sx_set_serial_info(port, (struct serial_struct *) arg);
	 default:
		return -ENOIOCTLCMD;
	}
	return 0;
}


static void sx_throttle(struct tty_struct * tty)
{
	struct specialix_port *port = (struct specialix_port *)tty->driver_data;
	struct specialix_board *bp;
	unsigned long flags;
				
	if (sx_paranoia_check(port, tty->device, "sx_throttle"))
		return;
	
	bp = port_Board(port);
	
	save_flags(flags); cli();

	/* Use DTR instead of RTS ! */
	if (SX_CRTSCTS (tty)) 
		port->MSVR &= ~MSVR_DTR;
	else {
		/* Auch!!! I think the system shouldn't call this then. */
		/* Or maybe we're supposed (allowed?) to do our side of hw
		   handshake anyway, even when hardware handshake is off. 
		   When you see this in your logs, please report.... */
		printk (KERN_ERR "sx%d: Need to throttle, but can't (hardware hs is off)\n",
	                 port_No (port));
	}
	sx_out(bp, CD186x_CAR, port_No(port));
	if (I_IXOFF(tty)) {
		sx_wait_CCR(bp);
		sx_out(bp, CD186x_CCR, CCR_SSCH2);
		sx_wait_CCR(bp);
	}
	sx_out(bp, CD186x_MSVR, port->MSVR);
	restore_flags(flags);
}


static void sx_unthrottle(struct tty_struct * tty)
{
	struct specialix_port *port = (struct specialix_port *)tty->driver_data;
	struct specialix_board *bp;
	unsigned long flags;
				
	if (sx_paranoia_check(port, tty->device, "sx_unthrottle"))
		return;
	
	bp = port_Board(port);
	
	save_flags(flags); cli();
	/* XXXX Use DTR INSTEAD???? */
	if (SX_CRTSCTS(tty)) {
		port->MSVR |= MSVR_DTR;
	} /* Else clause: see remark in "sx_throttle"... */

	sx_out(bp, CD186x_CAR, port_No(port));
	if (I_IXOFF(tty)) {
		sx_wait_CCR(bp);
		sx_out(bp, CD186x_CCR, CCR_SSCH1);
		sx_wait_CCR(bp);
	}
	sx_out(bp, CD186x_MSVR, port->MSVR);
	restore_flags(flags);
}


static void sx_stop(struct tty_struct * tty)
{
	struct specialix_port *port = (struct specialix_port *)tty->driver_data;
	struct specialix_board *bp;
	unsigned long flags;
				
	if (sx_paranoia_check(port, tty->device, "sx_stop"))
		return;
	
	bp = port_Board(port);
	
	save_flags(flags); cli();
	port->IER &= ~IER_TXRDY;
	sx_out(bp, CD186x_CAR, port_No(port));
	sx_out(bp, CD186x_IER, port->IER);
	restore_flags(flags);
}


static void sx_start(struct tty_struct * tty)
{
	struct specialix_port *port = (struct specialix_port *)tty->driver_data;
	struct specialix_board *bp;
	unsigned long flags;
				
	if (sx_paranoia_check(port, tty->device, "sx_start"))
		return;
	
	bp = port_Board(port);
	
	save_flags(flags); cli();
	if (port->xmit_cnt && port->xmit_buf && !(port->IER & IER_TXRDY)) {
		port->IER |= IER_TXRDY;
		sx_out(bp, CD186x_CAR, port_No(port));
		sx_out(bp, CD186x_IER, port->IER);
	}
	restore_flags(flags);
}


/*
 * This routine is called from the scheduler tqueue when the interrupt
 * routine has signalled that a hangup has occurred.  The path of
 * hangup processing is:
 *
 * 	serial interrupt routine -> (scheduler tqueue) ->
 * 	do_sx_hangup() -> tty->hangup() -> sx_hangup()
 * 
 */
static void do_sx_hangup(void *private_)
{
	struct specialix_port	*port = (struct specialix_port *) private_;
	struct tty_struct	*tty;
	
	tty = port->tty;
	if (tty)
		tty_hangup(tty);	/* FIXME: module removal race here */
	MOD_DEC_USE_COUNT;
}


static void sx_hangup(struct tty_struct * tty)
{
	struct specialix_port *port = (struct specialix_port *)tty->driver_data;
	struct specialix_board *bp;
				
	if (sx_paranoia_check(port, tty->device, "sx_hangup"))
		return;
	
	bp = port_Board(port);
	
	sx_shutdown_port(bp, port);
	port->event = 0;
	port->count = 0;
	port->flags &= ~(ASYNC_NORMAL_ACTIVE|ASYNC_CALLOUT_ACTIVE);
	port->tty = 0;
	wake_up_interruptible(&port->open_wait);
}


static void sx_set_termios(struct tty_struct * tty, struct termios * old_termios)
{
	struct specialix_port *port = (struct specialix_port *)tty->driver_data;
	unsigned long flags;
				
	if (sx_paranoia_check(port, tty->device, "sx_set_termios"))
		return;
	
	if (tty->termios->c_cflag == old_termios->c_cflag &&
	    tty->termios->c_iflag == old_termios->c_iflag)
		return;

	save_flags(flags); cli();
	sx_change_speed(port_Board(port), port);
	restore_flags(flags);

	if ((old_termios->c_cflag & CRTSCTS) &&
	    !(tty->termios->c_cflag & CRTSCTS)) {
		tty->hw_stopped = 0;
		sx_start(tty);
	}
}


static void do_specialix_bh(void)
{
	 run_task_queue(&tq_specialix);
}


static void do_softint(void *private_)
{
	struct specialix_port	*port = (struct specialix_port *) private_;
	struct tty_struct	*tty;
	
	if(!(tty = port->tty)) 
		return;

	if (test_and_clear_bit(RS_EVENT_WRITE_WAKEUP, &port->event)) {
		if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) &&
		    tty->ldisc.write_wakeup)
			(tty->ldisc.write_wakeup)(tty);
		wake_up_interruptible(&tty->write_wait);
	}
}


static int sx_init_drivers(void)
{
	int error;
	int i;

	
	if (!(tmp_buf = (unsigned char *) get_free_page(GFP_KERNEL))) {
		printk(KERN_ERR "sx: Couldn't get free page.\n");
		return 1;
	}
	init_bh(SPECIALIX_BH, do_specialix_bh);
	memset(&specialix_driver, 0, sizeof(specialix_driver));
	specialix_driver.magic = TTY_DRIVER_MAGIC;
	specialix_driver.name = "ttyW";
	specialix_driver.major = SPECIALIX_NORMAL_MAJOR;
	specialix_driver.num = SX_NBOARD * SX_NPORT;
	specialix_driver.type = TTY_DRIVER_TYPE_SERIAL;
	specialix_driver.subtype = SPECIALIX_TYPE_NORMAL;
	specialix_driver.init_termios = tty_std_termios;
	specialix_driver.init_termios.c_cflag =
		B9600 | CS8 | CREAD | HUPCL | CLOCAL;
	specialix_driver.flags = TTY_DRIVER_REAL_RAW;
	specialix_driver.refcount = &specialix_refcount;
	specialix_driver.table = specialix_table;
	specialix_driver.termios = specialix_termios;
	specialix_driver.termios_locked = specialix_termios_locked;

	specialix_driver.open  = sx_open;
	specialix_driver.close = sx_close;
	specialix_driver.write = sx_write;
	specialix_driver.put_char = sx_put_char;
	specialix_driver.flush_chars = sx_flush_chars;
	specialix_driver.write_room = sx_write_room;
	specialix_driver.chars_in_buffer = sx_chars_in_buffer;
	specialix_driver.flush_buffer = sx_flush_buffer;
	specialix_driver.ioctl = sx_ioctl;
	specialix_driver.throttle = sx_throttle;
	specialix_driver.unthrottle = sx_unthrottle;
	specialix_driver.set_termios = sx_set_termios;
	specialix_driver.stop = sx_stop;
	specialix_driver.start = sx_start;
	specialix_driver.hangup = sx_hangup;

	specialix_callout_driver = specialix_driver;
	specialix_callout_driver.name = "cuw";
	specialix_callout_driver.major = SPECIALIX_CALLOUT_MAJOR;
	specialix_callout_driver.subtype = SPECIALIX_TYPE_CALLOUT;
	
	if ((error = tty_register_driver(&specialix_driver))) {
		free_page((unsigned long)tmp_buf);
		printk(KERN_ERR "sx: Couldn't register specialix IO8+ driver, error = %d\n",
		       error);
		return 1;
	}
	if ((error = tty_register_driver(&specialix_callout_driver))) {
		free_page((unsigned long)tmp_buf);
		tty_unregister_driver(&specialix_driver);
		printk(KERN_ERR "sx: Couldn't register specialix IO8+ callout driver, error = %d\n",
		       error);
		return 1;
	}
	
	memset(sx_port, 0, sizeof(sx_port));
	for (i = 0; i < SX_NPORT * SX_NBOARD; i++) {
		sx_port[i].callout_termios = specialix_callout_driver.init_termios;
		sx_port[i].normal_termios  = specialix_driver.init_termios;
		sx_port[i].magic = SPECIALIX_MAGIC;
		sx_port[i].tqueue.routine = do_softint;
		sx_port[i].tqueue.data = &sx_port[i];
		sx_port[i].tqueue_hangup.routine = do_sx_hangup;
		sx_port[i].tqueue_hangup.data = &sx_port[i];
		sx_port[i].close_delay = 50 * HZ/100;
		sx_port[i].closing_wait = 3000 * HZ/100;
		init_waitqueue_head(&sx_port[i].open_wait);
		init_waitqueue_head(&sx_port[i].close_wait);
	}
	
	return 0;
}


static void sx_release_drivers(void)
{
	free_page((unsigned long)tmp_buf);
	tty_unregister_driver(&specialix_driver);
	tty_unregister_driver(&specialix_callout_driver);
}


#ifndef MODULE
/*
 * Called at boot time.
 * 
 * You can specify IO base for up to SX_NBOARD cards,
 * using line "specialix=0xiobase1,0xiobase2,.." at LILO prompt.
 * Note that there will be no probing at default
 * addresses in this case.
 *
 */ 
void specialix_setup(char *str, int * ints)
{
	int i;
        
	for (i=0;i<SX_NBOARD;i++) {
		sx_board[i].base = 0;
	}

	for (i = 1; i <= ints[0]; i++) {
		if (i&1)
			sx_board[i/2].base = ints[i];
		else
			sx_board[i/2 -1].irq = ints[i];
	}
}
#endif

/* 
 * This routine must be called by kernel at boot time 
 */
int specialix_init(void) 
{
	int i;
	int found = 0;

	printk(KERN_INFO "sx: Specialix IO8+ driver v" VERSION ", (c) R.E.Wolff 1997/1998.\n");
	printk(KERN_INFO "sx: derived from work (c) D.Gorodchanin 1994-1996.\n");
#ifdef CONFIG_SPECIALIX_RTSCTS
	printk (KERN_INFO "sx: DTR/RTS pin is always RTS.\n");
#else
	printk (KERN_INFO "sx: DTR/RTS pin is RTS when CRTSCTS is on.\n");
#endif
	
	if (sx_init_drivers()) 
		return -EIO;

	for (i = 0; i < SX_NBOARD; i++) 
		if (sx_board[i].base && !sx_probe(&sx_board[i]))
			found++;

#ifdef CONFIG_PCI
	if (pci_present()) {
		struct pci_dev *pdev = NULL;

		i=0;
		while (i <= SX_NBOARD) {
			if (sx_board[i].flags & SX_BOARD_PRESENT) {
				i++;
				continue;
			}
			pdev = pci_find_device (PCI_VENDOR_ID_SPECIALIX, 
			                        PCI_DEVICE_ID_SPECIALIX_IO8, 
			                        pdev);
			if (!pdev) break;

			if (pci_enable_device(pdev))
				continue;

			sx_board[i].irq = pdev->irq;

			sx_board[i].base = pci_resource_start (pdev, 2);

			sx_board[i].flags |= SX_BOARD_IS_PCI;
			if (!sx_probe(&sx_board[i]))
				found ++;
		}
	}
#endif

	if (!found) {
		sx_release_drivers();
		printk(KERN_INFO "sx: No specialix IO8+ boards detected.\n");
		return -EIO;
	}

	return 0;
}

#ifdef MODULE
int iobase[SX_NBOARD]  = {0,};

int irq [SX_NBOARD] = {0,};

MODULE_PARM(iobase,"1-" __MODULE_STRING(SX_NBOARD) "i");
MODULE_PARM(irq,"1-" __MODULE_STRING(SX_NBOARD) "i");

/*
 * You can setup up to 4 boards.
 * by specifying "iobase=0xXXX,0xXXX ..." as insmod parameter.
 * You should specify the IRQs too in that case "irq=....,...". 
 * 
 * More than 4 boards in one computer is not possible, as the card can
 * only use 4 different interrupts. 
 *
 */
int init_module(void) 
{
	int i;

	if (iobase[0] || iobase[1] || iobase[2] || iobase[3]) {
		for(i = 0; i < SX_NBOARD; i++) {
			sx_board[i].base = iobase[i];
			sx_board[i].irq = irq[i];
		}
	}

	return specialix_init();
}
	

void cleanup_module(void)
{
	int i;
	
	sx_release_drivers();
	for (i = 0; i < SX_NBOARD; i++)
		if (sx_board[i].flags & SX_BOARD_PRESENT) 
			sx_release_io_range(&sx_board[i]);
#ifdef SPECIALIX_TIMER
	del_timer (&missed_irq_timer);
#endif
	
}
#endif /* MODULE */

MODULE_LICENSE("GPL");
