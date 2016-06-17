/*
 *  drivers/char/serial_txx9.c
 *
 *  Copyright (C) 1999 Harald Koerfgen
 *  Copyright (C) 2000 Jim Pick <jim@jimpick.com>
 *  Copyright (C) 2001 Steven J. Hill (sjhill@realitydiluted.com)
 *  Copyright (C) 2000-2002 Toshiba Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Serial driver for TX3927/TX4927/TX4925/TX4938 internal SIO controller
 */
#include <linux/init.h>
#include <linux/config.h>
#include <linux/tty.h>
#include <linux/major.h>
#include <linux/ptrace.h>
#include <linux/init.h>
#include <linux/console.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/delay.h>
#include <linux/serial.h>
#include <linux/generic_serial.h>
#include <linux/pci.h>
#ifdef CONFIG_MAGIC_SYSRQ
#include <linux/sysrq.h>
#endif

#define  DEBUG
#ifdef  DEBUG
#define DBG(x...)       printk(x)
#else
#define DBG(x...)       
#endif

static char *serial_version = "0.30-mvl";
static char *serial_name = "TX39/49 Serial driver";

#define GS_INTERNAL_FLAGS (GS_TX_INTEN|GS_RX_INTEN|GS_ACTIVE)

#define TXX9_SERIAL_MAGIC 0x39274927

#ifdef CONFIG_SERIAL
/* "ttyS","cua" is used for standard serial driver */
#define TXX9_TTY_NAME "ttyTX"
#define TXX9_TTY_DEVFS_NAME "tts/TX%d"
#define TXX9_TTY_MINOR_START	(64 + 64)	/* ttyTX0(128), ttyTX1(129) */
#define TXX9_CU_NAME "cuatx"
#define TXX9_CU_DEVFS_NAME "cua/TX%d"
#else
/* acts like standard serial driver */
#define TXX9_TTY_NAME "ttyS"
#define TXX9_TTY_DEVFS_NAME "tts/%d"
#define TXX9_TTY_MINOR_START	64
#define TXX9_CU_NAME "cua"
#define TXX9_CU_DEVFS_NAME "cua/%d"
#endif
#define TXX9_TTY_MAJOR	TTY_MAJOR
#define TXX9_TTYAUX_MAJOR	TTYAUX_MAJOR

#define TXX9_SERIAL_HAVE_CTS_LINE	1

#ifdef CONFIG_PCI
/* support for Toshiba TC86C001 SIO */
#define ENABLE_SERIAL_TXX9_PCI
#endif

/*
 * Number of serial ports
 */
#ifdef ENABLE_SERIAL_TXX9_PCI
#define NR_PCI_BOARDS	4
#define NR_PORTS  (2 + NR_PCI_BOARDS)
#else
#define NR_PORTS  2
#endif

/*
 * Hardware specific serial port structure
 */
static struct rs_port {
	struct gs_port		gs;		/* Must be first field! */

	unsigned long		base;
	int			irq;
	int			baud_base;
	int			flags;
        struct async_icount	icount;
	int			x_char;		/* XON/XOFF character */
	int			read_status_mask;
	int			ignore_status_mask;
	int			quot;
	char			io_type;
#ifdef ENABLE_SERIAL_TXX9_PCI
	struct pci_dev *	pci_dev;
#endif
} rs_ports[NR_PORTS]
#ifdef CONFIG_TOSHIBA_RBTX4925
= {
	/* NR_PORTS = 0 */
	{base:		0xff1f0000 + 0xf300,
	 irq:		36,
	 baud_base:	40000000 / 16 / 2,
	 io_type:	-1,		/* virtual memory mapped */
	 flags: 	TXX9_SERIAL_HAVE_CTS_LINE,},
	/* NR_PORTS = 1 */
	{base:		0xff1f0000 + 0xf400,
	 irq:		37,
	 baud_base:	40000000 / 16 / 2,
	 io_type:	-1,		/* virtual memory mapped */
	 flags: 	TXX9_SERIAL_HAVE_CTS_LINE,}
};
#endif
#ifdef CONFIG_TOSHIBA_RBTX4927
= {
	/* NR_PORTS = 0 */
	{base:		0xff1f0000 + 0xf300,
	 irq:		32,
	 baud_base:	50000000 / 16 / 2,
	 io_type:	-1,		/* virtual memory mapped */
	 flags: 	TXX9_SERIAL_HAVE_CTS_LINE,},
	/* NR_PORTS = 1 */
	{base:		0xff1f0000 + 0xf400,
	 irq:		33,
	 baud_base:	50000000 / 16 / 2,
	 io_type:	-1,		/* virtual memory mapped */
	 flags: 	TXX9_SERIAL_HAVE_CTS_LINE,}
};
#endif

/* TXX9 Serial Registers */
#define TXX9_SILCR	0x00
#define TXX9_SIDICR	0x04
#define TXX9_SIDISR	0x08
#define TXX9_SICISR	0x0c
#define TXX9_SIFCR	0x10
#define TXX9_SIFLCR	0x14
#define TXX9_SIBGR	0x18
#define TXX9_SITFIFO	0x1c
#define TXX9_SIRFIFO	0x20

/* SILCR : Line Control */
#define TXX9_SILCR_SCS_MASK	0x00000060
#define TXX9_SILCR_SCS_IMCLK	0x00000000
#define TXX9_SILCR_SCS_IMCLK_BG	0x00000020
#define TXX9_SILCR_SCS_SCLK	0x00000040
#define TXX9_SILCR_SCS_SCLK_BG	0x00000060
#define TXX9_SILCR_UEPS	0x00000010
#define TXX9_SILCR_UPEN	0x00000008
#define TXX9_SILCR_USBL_MASK	0x00000004
//#define TXX9_SILCR_USBL_1BIT	0x00000004
//#define TXX9_SILCR_USBL_2BIT	0x00000000
#define TXX9_SILCR_USBL_1BIT	0x00000000
#define TXX9_SILCR_USBL_2BIT	0x00000004
#define TXX9_SILCR_UMODE_MASK	0x00000003
#define TXX9_SILCR_UMODE_8BIT	0x00000000
#define TXX9_SILCR_UMODE_7BIT	0x00000001

/* SIDICR : DMA/Int. Control */
#define TXX9_SIDICR_TDE	0x00008000
#define TXX9_SIDICR_RDE	0x00004000
#define TXX9_SIDICR_TIE	0x00002000
#define TXX9_SIDICR_RIE	0x00001000
#define TXX9_SIDICR_SPIE	0x00000800
#define TXX9_SIDICR_CTSAC	0x00000600
#define TXX9_SIDICR_STIE_MASK	0x0000003f
#define TXX9_SIDICR_STIE_OERS		0x00000020
#define TXX9_SIDICR_STIE_CTSS		0x00000010
#define TXX9_SIDICR_STIE_RBRKD	0x00000008
#define TXX9_SIDICR_STIE_TRDY		0x00000004
#define TXX9_SIDICR_STIE_TXALS	0x00000002
#define TXX9_SIDICR_STIE_UBRKD	0x00000001

/* SIDISR : DMA/Int. Status */
#define TXX9_SIDISR_UBRK	0x00008000
#define TXX9_SIDISR_UVALID	0x00004000
#define TXX9_SIDISR_UFER	0x00002000
#define TXX9_SIDISR_UPER	0x00001000
#define TXX9_SIDISR_UOER	0x00000800
#define TXX9_SIDISR_ERI	0x00000400
#define TXX9_SIDISR_TOUT	0x00000200
#define TXX9_SIDISR_TDIS	0x00000100
#define TXX9_SIDISR_RDIS	0x00000080
#define TXX9_SIDISR_STIS	0x00000040
#define TXX9_SIDISR_RFDN_MASK	0x0000001f

/* SICISR : Change Int. Status */
#define TXX9_SICISR_OERS	0x00000020
#define TXX9_SICISR_CTSS	0x00000010
#define TXX9_SICISR_RBRKD	0x00000008
#define TXX9_SICISR_TRDY	0x00000004
#define TXX9_SICISR_TXALS	0x00000002
#define TXX9_SICISR_UBRKD	0x00000001

/* SIFCR : FIFO Control */
#define TXX9_SIFCR_SWRST	0x00008000
#define TXX9_SIFCR_RDIL_MASK	0x00000180
#define TXX9_SIFCR_RDIL_1	0x00000000
#define TXX9_SIFCR_RDIL_4	0x00000080
#define TXX9_SIFCR_RDIL_8	0x00000100
#define TXX9_SIFCR_RDIL_12	0x00000180
#define TXX9_SIFCR_RDIL_MAX	0x00000180
#define TXX9_SIFCR_TDIL_MASK	0x00000018
#define TXX9_SIFCR_TDIL_MASK	0x00000018
#define TXX9_SIFCR_TDIL_1	0x00000000
#define TXX9_SIFCR_TDIL_4	0x00000001
#define TXX9_SIFCR_TDIL_8	0x00000010
#define TXX9_SIFCR_TDIL_MAX	0x00000010
#define TXX9_SIFCR_TFRST	0x00000004
#define TXX9_SIFCR_RFRST	0x00000002
#define TXX9_SIFCR_FRSTE	0x00000001
#define TXX9_SIO_TX_FIFO	8
#define TXX9_SIO_RX_FIFO	16

/* SIFLCR : Flow Control */
#define TXX9_SIFLCR_RCS	0x00001000
#define TXX9_SIFLCR_TES	0x00000800
#define TXX9_SIFLCR_RTSSC	0x00000200
#define TXX9_SIFLCR_RSDE	0x00000100
#define TXX9_SIFLCR_TSDE	0x00000080
#define TXX9_SIFLCR_RTSTL_MASK	0x0000001e
#define TXX9_SIFLCR_RTSTL_MAX	0x0000001e
#define TXX9_SIFLCR_TBRK	0x00000001

/* SIBGR : Baudrate Control */
#define TXX9_SIBGR_BCLK_MASK	0x00000300
#define TXX9_SIBGR_BCLK_T0	0x00000000
#define TXX9_SIBGR_BCLK_T2	0x00000100
#define TXX9_SIBGR_BCLK_T4	0x00000200
#define TXX9_SIBGR_BCLK_T6	0x00000300
#define TXX9_SIBGR_BRD_MASK	0x000000ff

static /*inline*/ unsigned int
sio_in(struct rs_port *port, int offset)
{
	if (port->io_type < 0)
		/* don't use __raw_readl that does swapping for big endian */
		return (*(volatile unsigned long*)(port->base + offset));
	else
		return inl(port->base + offset);
}

static /*inline*/ void
sio_out(struct rs_port *port, int offset, unsigned int value)
{
	if (port->io_type < 0)
		/* don't use __raw_writel that does swapping for big endian */
                (*(volatile unsigned long*)(port->base + offset))=(value);
	else
		outl(value, port->base + offset);
}

static inline void
sio_mask(struct rs_port *port, int offset, unsigned int value)
{
	sio_out(port, offset, sio_in(port, offset) & ~value);
}
static inline void
sio_set(struct rs_port *port, int offset, unsigned int value)
{
	sio_out(port, offset, sio_in(port, offset) | value);
}

/*
 * Forward declarations for serial routines
 */
static void rs_disable_tx_interrupts (void * ptr);
static void rs_enable_tx_interrupts (void * ptr);
static void rs_disable_rx_interrupts (void * ptr);
static void rs_enable_rx_interrupts (void * ptr);
static int rs_get_CD (void * ptr);
static void rs_shutdown_port (void * ptr);
static int rs_set_real_termios (void *ptr);
static int rs_chars_in_buffer (void * ptr);
static void rs_hungup (void *ptr);
static void rs_getserial (void*, struct serial_struct *sp);
static void rs_close (void *ptr);

/*
 * Used by generic serial driver to access hardware
 */
static struct real_driver rs_real_driver = {
	disable_tx_interrupts: rs_disable_tx_interrupts,
	enable_tx_interrupts:  rs_enable_tx_interrupts,
	disable_rx_interrupts: rs_disable_rx_interrupts,
	enable_rx_interrupts:  rs_enable_rx_interrupts,
	get_CD:                rs_get_CD,
	shutdown_port:         rs_shutdown_port,
	set_real_termios:      rs_set_real_termios,
	chars_in_buffer:       rs_chars_in_buffer,
	close:                 rs_close,
	hungup:                rs_hungup,
	getserial:             rs_getserial,
};

/*
 * Structures and such for TTY sessions and usage counts
 */
static struct tty_driver rs_driver, rs_callout_driver;
static struct tty_struct *rs_table[NR_PORTS];
static struct termios *rs_termios[NR_PORTS];
static struct termios *rs_termios_locked[NR_PORTS];
int rs_refcount;
int rs_initialized = 0;

#ifdef CONFIG_SERIAL_TXX9_CONSOLE
static struct console sercons;
#endif
#if defined(CONFIG_SERIAL_TXX9_CONSOLE) && defined(CONFIG_MAGIC_SYSRQ)
static unsigned long break_pressed; /* break, really ... */
#endif

static inline void receive_chars(struct rs_port *port,
				 int *status, struct pt_regs *regs)
{
	struct tty_struct *tty = port->gs.tty;
	unsigned char ch;
	struct	async_icount *icount;
	int	max_count = 256;

	icount = &port->icount;
	do {
		if (tty->flip.count >= TTY_FLIPBUF_SIZE) {
			tty->flip.tqueue.routine((void *) tty);
			if (tty->flip.count >= TTY_FLIPBUF_SIZE)
				return;		// if TTY_DONT_FLIP is set
		}
		ch = sio_in(port, TXX9_SIRFIFO);
		*tty->flip.char_buf_ptr = ch;
		icount->rx++;

		*tty->flip.flag_buf_ptr = 0;
		if (*status & (TXX9_SIDISR_UBRK | TXX9_SIDISR_UPER |
			       TXX9_SIDISR_UFER | TXX9_SIDISR_UOER)) {
			/*
			 * For statistics only
			 */
			if (*status & TXX9_SIDISR_UBRK) {
				*status &= ~(TXX9_SIDISR_UFER | TXX9_SIDISR_UPER);
				icount->brk++;
				/*
				 * We do the SysRQ and SAK checking
				 * here because otherwise the break
				 * may get masked by ignore_status_mask
				 * or read_status_mask.
				 */
#if defined(CONFIG_SERIAL_TXX9_CONSOLE) && defined(CONFIG_MAGIC_SYSRQ)
				if (port == &rs_ports[sercons.index]) {
					if (!break_pressed) {
						break_pressed = jiffies;
						goto ignore_char;
					}
					break_pressed = 0;
				}
#endif
				if (port->gs.flags & ASYNC_SAK)
					do_SAK(tty);
			} else if (*status & TXX9_SIDISR_UPER)
				icount->parity++;
			else if (*status & TXX9_SIDISR_UFER)
				icount->frame++;
			if (*status & TXX9_SIDISR_UOER)
				icount->overrun++;

			/*
			 * Mask off conditions which should be ignored.
			 */
			*status &= port->read_status_mask;

#ifdef CONFIG_SERIAL_TXX9_CONSOLE
			/* Break flag is updated by reading RFIFO. */
#endif
			if (*status & (TXX9_SIDISR_UBRK)) {
				*tty->flip.flag_buf_ptr = TTY_BREAK;
			} else if (*status & TXX9_SIDISR_UPER)
				*tty->flip.flag_buf_ptr = TTY_PARITY;
			else if (*status & TXX9_SIDISR_UFER)
				*tty->flip.flag_buf_ptr = TTY_FRAME;
		}
#if defined(CONFIG_SERIAL_TXX9_CONSOLE) && defined(CONFIG_MAGIC_SYSRQ)
		if (break_pressed && port == &rs_ports[sercons.index]) {
			if (ch != 0 &&
			    time_before(jiffies, break_pressed + HZ*5)) {
				handle_sysrq(ch, regs, NULL, NULL);
				break_pressed = 0;
				goto ignore_char;
			}
			break_pressed = 0;
		}
#endif
		if ((*status & port->ignore_status_mask) == 0) {
			tty->flip.flag_buf_ptr++;
			tty->flip.char_buf_ptr++;
			tty->flip.count++;
		}
		if ((*status & TXX9_SIDISR_UOER) &&
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
#if defined(CONFIG_SERIAL_TXX9_CONSOLE) && defined(CONFIG_MAGIC_SYSRQ)
	ignore_char:
#endif
		*status = sio_in(port, TXX9_SIDISR);
	} while ((!(*status & TXX9_SIDISR_UVALID)) && (max_count-- > 0));
	tty_flip_buffer_push(tty);
}

static inline void transmit_chars(struct rs_port *port)
{
	int count;

	if (port->x_char) {
		sio_out(port, TXX9_SITFIFO, port->x_char);
		port->icount.tx++;
		port->x_char = 0;
		return;
	}
	if (port->gs.xmit_cnt <= 0
	    || port->gs.tty->stopped
	    || port->gs.tty->hw_stopped) {
		rs_disable_tx_interrupts(port);
		return;
	}

	count = TXX9_SIO_TX_FIFO;
	do {
		sio_out(port, TXX9_SITFIFO, port->gs.xmit_buf[port->gs.xmit_tail++]);
		port->gs.xmit_tail &= SERIAL_XMIT_SIZE-1;
		port->icount.tx++;
		if (--port->gs.xmit_cnt <= 0)
			break;
	} while (--count > 0);

	if (port->gs.xmit_cnt <= 0 || port->gs.tty->stopped ||
	     port->gs.tty->hw_stopped) {
		rs_disable_tx_interrupts(port);
	}

	if (port->gs.xmit_cnt <= port->gs.wakeup_chars) {
                if ((port->gs.tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) &&
                    port->gs.tty->ldisc.write_wakeup)
                        (port->gs.tty->ldisc.write_wakeup)(port->gs.tty);
                wake_up_interruptible(&port->gs.tty->write_wait);
	}
}

static inline void check_modem_status(struct rs_port *port)
{
        /* We don't have a carrier detect line - but just respond
           like we had one anyways so that open() becomes unblocked */
	wake_up_interruptible(&port->gs.open_wait);
}

#define RS_ISR_PASS_LIMIT 256

static void rs_interrupt(int irq, void *dev_id, struct pt_regs * regs)
{
	struct rs_port * port = (struct rs_port *)dev_id;
	unsigned long flags;
	int status;
	int pass_counter = 0;

	local_irq_save(flags);

	if (!port || !port->gs.tty) {
		local_irq_restore(flags);
		return;
	}

	do {
		status = sio_in(port, TXX9_SIDISR);
		if (!(sio_in(port, TXX9_SIDICR) & TXX9_SIDICR_TIE))
			status &= ~TXX9_SIDISR_TDIS;
		if (!(status & (TXX9_SIDISR_TDIS | TXX9_SIDISR_RDIS |
				TXX9_SIDISR_TOUT)))
			break;

		if (status & TXX9_SIDISR_RDIS)
			receive_chars(port, &status, regs);
#if 0		/* RTS/CTS are controled by HW. (if possible) */
		check_modem_status(port);
#endif
		if (status & TXX9_SIDISR_TDIS)
			transmit_chars(port);
		/* Clear TX/RX Int. Status */
		sio_mask(port, TXX9_SIDISR,
			 TXX9_SIDISR_TDIS | TXX9_SIDISR_RDIS |
			 TXX9_SIDISR_TOUT);

		if (pass_counter++ > RS_ISR_PASS_LIMIT) {
			break;
		}
	} while (1);
	local_irq_restore(flags);
}

/*
 ***********************************************************************
 *                Here are the routines that actually                  *
 *              interface with the generic_serial driver               *
 ***********************************************************************
 */
static void rs_disable_tx_interrupts (void * ptr)
{
	struct rs_port *port = ptr;
	unsigned long flags;

	local_irq_save(flags);
        port->gs.flags &= ~GS_TX_INTEN;
	sio_mask(port, TXX9_SIDICR, TXX9_SIDICR_TIE);
	local_irq_restore(flags);
}

static void rs_enable_tx_interrupts (void * ptr)
{
	struct rs_port *port = ptr;
	unsigned long flags;

	local_irq_save(flags);
	sio_set(port, TXX9_SIDICR, TXX9_SIDICR_TIE);
	local_irq_restore(flags);
}

static void rs_disable_rx_interrupts (void * ptr)
{
	struct rs_port *port = ptr;
	unsigned long flags;

	local_irq_save(flags);
	port->read_status_mask &= ~TXX9_SIDISR_RDIS;
#if 0
	sio_mask(port, TXX9_SIDICR, TXX9_SIDICR_RIE);
#endif
	local_irq_restore(flags);
}

static void rs_enable_rx_interrupts (void * ptr)
{
	struct rs_port *port = ptr;
	sio_set(port, TXX9_SIDICR, TXX9_SIDICR_RIE);
}


static int rs_get_CD (void * ptr)
{
	/* No Carried Detect in Hardware - just return true */
	return (1);
}

static void rs_shutdown_port (void * ptr)
{
	struct rs_port *port = ptr;

	port->gs.flags &= ~GS_ACTIVE;

	free_irq(port->irq, port);
	sio_out(port, TXX9_SIDICR, 0);	/* disable all intrs */
	/* disable break condition */
	sio_mask(port, TXX9_SIFLCR, TXX9_SIFLCR_TBRK);

#ifdef CONFIG_SERIAL_TXX9_CONSOLE
	if (port == &rs_ports[sercons.index]) {
#endif
	if (!port->gs.tty || (port->gs.tty->termios->c_cflag & HUPCL)) {
		/* drop RTS */
		sio_set(port, TXX9_SIFLCR,
			TXX9_SIFLCR_RTSSC | TXX9_SIFLCR_RSDE);
		/* TXX9-SIO can not control DTR... */
	}

	/* reset FIFO's */
	sio_set(port, TXX9_SIFCR,
		TXX9_SIFCR_TFRST | TXX9_SIFCR_RFRST | TXX9_SIFCR_FRSTE);
	/* clear reset */
	sio_mask(port, TXX9_SIFCR,
		 TXX9_SIFCR_TFRST | TXX9_SIFCR_RFRST | TXX9_SIFCR_FRSTE);
	/* Disable RX/TX */
	sio_set(port, TXX9_SIFLCR, TXX9_SIFLCR_RSDE | TXX9_SIFLCR_TSDE);
#ifdef CONFIG_SERIAL_TXX9_CONSOLE
	}
#endif
}

static int rs_set_real_termios (void *ptr)
{
	struct rs_port *port = ptr;
	int	quot = 0, baud_base, baud;
	unsigned cflag, cval, fcr = 0;
	int	bits;
	unsigned long	flags;

	if (!port->gs.tty || !port->gs.tty->termios)
		return 0;
	cflag = port->gs.tty->termios->c_cflag;
	cval = sio_in(port, TXX9_SILCR);
	/* byte size and parity */
	cval &= ~TXX9_SILCR_UMODE_MASK;
	switch (cflag & CSIZE) {
	case CS7:
		cval |= TXX9_SILCR_UMODE_7BIT;
		bits = 9;
		break;
	case CS5:	/* not supported */
	case CS6:	/* not supported */
	case CS8:
	default:
		cval |= TXX9_SILCR_UMODE_8BIT;
		bits = 10;
		break;
	}
	cval &= ~TXX9_SILCR_USBL_MASK;
	if (cflag & CSTOPB) {
		cval |= TXX9_SILCR_USBL_2BIT;
		bits++;
	} else {
		cval |= TXX9_SILCR_USBL_1BIT;
	}

	cval &= ~(TXX9_SILCR_UPEN | TXX9_SILCR_UEPS);
	if (cflag & PARENB) {
		cval |= TXX9_SILCR_UPEN;
		bits++;
	}
	if (!(cflag & PARODD))
		cval |= TXX9_SILCR_UEPS;

	/* Determine divisor based on baud rate */
	baud = tty_get_baud_rate(port->gs.tty);
	if (!baud)
		baud = 9600;	/* B0 transition handled in rs_set_termios */
	baud_base = port->baud_base;
	quot = (baud_base + baud / 2) / baud;
	/* As a last resort, if the quotient is zero, default to 9600 bps */
	if (!quot)
		quot = (baud_base + 9600 / 2) / 9600;
	port->quot = quot;

	/* Set up FIFO's */
	/* TX Int by FIFO Empty, RX Int by Receiving 1 char. */
	fcr = TXX9_SIFCR_TDIL_MAX | TXX9_SIFCR_RDIL_1;

	/* CTS flow control flag */
	if (cflag & CRTSCTS) {
		port->gs.flags |= ASYNC_CTS_FLOW;
		if (port->flags & TXX9_SERIAL_HAVE_CTS_LINE)
			sio_out(port, TXX9_SIFLCR,
				TXX9_SIFLCR_RCS | TXX9_SIFLCR_TES |
				TXX9_SIFLCR_RTSTL_MAX /* 15 */);
	} else {
		port->gs.flags &= ~ASYNC_CTS_FLOW;
		sio_mask(port, TXX9_SIFLCR,
			 TXX9_SIFLCR_RCS | TXX9_SIFLCR_TES |
			 TXX9_SIFLCR_RSDE | TXX9_SIFLCR_TSDE);
	}

	/*
	 * Set up parity check flag
	 */
#define RELEVANT_IFLAG(iflag) (iflag & (IGNBRK|BRKINT|IGNPAR|PARMRK|INPCK))

	port->read_status_mask = TXX9_SIDISR_UOER |
		TXX9_SIDISR_TDIS | TXX9_SIDISR_RDIS;
	if (I_INPCK(port->gs.tty))
		port->read_status_mask |= TXX9_SIDISR_UFER | TXX9_SIDISR_UPER;
	if (I_BRKINT(port->gs.tty) || I_PARMRK(port->gs.tty))
		port->read_status_mask |= TXX9_SIDISR_UBRK;

	/*
	 * Characters to ignore
	 */
	port->ignore_status_mask = 0;
	if (I_IGNPAR(port->gs.tty))
		port->ignore_status_mask |= TXX9_SIDISR_UPER | TXX9_SIDISR_UFER;
	if (I_IGNBRK(port->gs.tty)) {
		port->ignore_status_mask |= TXX9_SIDISR_UBRK;
		/*
		 * If we're ignore parity and break indicators, ignore
		 * overruns too.  (For real raw support).
		 */
		if (I_IGNPAR(port->gs.tty))
			port->ignore_status_mask |= TXX9_SIDISR_UOER;
	}
#if 0	/* XXX: This cause problem with some programs(init, mingetty, etc) */
	/*
	 * !!! ignore all characters if CREAD is not set
	 */
	if ((cflag & CREAD) == 0)
		port->ignore_status_mask |= TXX9_SIDISR_RDIS;
#endif
	local_irq_save(flags);
	cval &= ~TXX9_SILCR_SCS_IMCLK;
	sio_out(port, TXX9_SILCR, cval | TXX9_SILCR_SCS_IMCLK_BG);
	sio_out(port, TXX9_SIBGR, quot | TXX9_SIBGR_BCLK_T0);
	sio_out(port, TXX9_SIFCR, fcr);
	local_irq_restore(flags);
	return 0;
}

static int rs_chars_in_buffer (void * ptr)
{
	struct rs_port *port = ptr;

	/* return 0 if transmitter disabled. */
	if (sio_in(port, TXX9_SIFLCR) & TXX9_SIFLCR_TSDE)
		return 0;
	return (sio_in(port, TXX9_SICISR) & TXX9_SICISR_TXALS) ? 0 : 1;
}

/* ********************************************************************** *
 *                Here are the routines that actually                     *
 *               interface with the rest of the system                    *
 * ********************************************************************** */
static int rs_open (struct tty_struct * tty, struct file * filp)
{
	struct rs_port *port;
	int retval, line;

	if (!rs_initialized) {
		return -EIO;
	}

	line = MINOR(tty->device) - tty->driver.minor_start;

	if ((line < 0) || (line >= NR_PORTS))
		return -ENODEV;

	/* Pre-initialized already */
	port = & rs_ports[line];

	if (!port->base) 
		return -ENODEV;

	tty->driver_data = port;
	port->gs.tty = tty;
	port->gs.count++;

	/*
	 * Start up serial port
	 */
	retval = gs_init_port(&port->gs);
	if (retval) {
		port->gs.count--;
		return retval;
	}

	port->gs.flags |= GS_ACTIVE;

	if (port->gs.count == 1) {
		MOD_INC_USE_COUNT;

		/*
		 * Clear the FIFO buffers and disable them
		 * (they will be reenabled in rs_set_real_termios())
		 */
		sio_set(port, TXX9_SIFCR,
			TXX9_SIFCR_TFRST | TXX9_SIFCR_RFRST |
			TXX9_SIFCR_FRSTE);
		/* clear reset */
		sio_mask(port, TXX9_SIFCR,
			 TXX9_SIFCR_TFRST | TXX9_SIFCR_RFRST |
			 TXX9_SIFCR_FRSTE);
		sio_out(port, TXX9_SIDICR, 0);

		retval = request_irq(port->irq, rs_interrupt, SA_SHIRQ,
				     "serial_txx9", port);
		if (retval) {
			printk(KERN_ERR "serial_txx9: request_irq: err %d\n",
			       retval);
			MOD_DEC_USE_COUNT;
			port->gs.count--;
			return retval;
		}
		/*
		 * Clear the interrupt registers.
		 */
		sio_out(port, TXX9_SIDISR, 0);

		/* HW RTS/CTS control */
		if (port->flags & TXX9_SERIAL_HAVE_CTS_LINE)
			sio_out(port, TXX9_SIFLCR,
				TXX9_SIFLCR_RCS | TXX9_SIFLCR_TES |
				TXX9_SIFLCR_RTSTL_MAX /* 15 */);
	}

	/* Enable RX/TX */
	sio_mask(port, TXX9_SIFLCR, TXX9_SIFLCR_RSDE | TXX9_SIFLCR_TSDE);

	/*
	 * Finally, enable interrupts
	 */
	rs_enable_rx_interrupts(port);

	/*
	 * and set the speed of the serial port
	 */
	rs_set_real_termios(port);

	retval = gs_block_til_ready(&port->gs, filp);

	if (retval) {
		if (port->gs.count == 1) {
			free_irq(port->irq, port);
			MOD_DEC_USE_COUNT;
		}
		port->gs.count--;
		return retval;
	}
	/* tty->low_latency = 1; */

	if ((port->gs.count == 1) && (port->gs.flags & ASYNC_SPLIT_TERMIOS)) {
		if (tty->driver.subtype == SERIAL_TYPE_NORMAL)
			*tty->termios = port->gs.normal_termios;
		else
			*tty->termios = port->gs.callout_termios;
		rs_set_real_termios(port);
	}
#ifdef CONFIG_SERIAL_TXX9_CONSOLE
	if (sercons.cflag && sercons.index == line) {
		tty->termios->c_cflag = sercons.cflag;
		sercons.cflag = 0;
		rs_set_real_termios(port);
	}
#endif
	port->gs.session = current->session;
	port->gs.pgrp = current->pgrp;
	return 0;
}

/*
 * /proc fs routines....
 */

static inline int line_info(char *buf, struct rs_port *port)
{
	char	stat_buf[30];
	int	ret;
	unsigned long flags;

	ret = sprintf(buf, "%d: uart:txx9 io%s:%lx irq:%d",
		      port - &rs_ports[0],
		      port->io_type < 0 ? "mem" : "port",
		      port->base, port->irq);

	if (!port->base) {
		ret += sprintf(buf+ret, "\n");
		return ret;
	}

	/*
	 * Figure out the current RS-232 lines
	 */
	stat_buf[0] = 0;
	stat_buf[1] = 0;
	local_irq_save(flags);
	if (!(sio_in(port, TXX9_SIFLCR) & TXX9_SIFLCR_RTSSC))
		strcat(stat_buf, "|RTS");
	if (!(sio_in(port, TXX9_SICISR) & TXX9_SICISR_CTSS))
		strcat(stat_buf, "|CTS");
	local_irq_restore(flags);

	if (port->quot) {
		ret += sprintf(buf+ret, " baud:%d",
			       port->baud_base / port->quot);
	}

	ret += sprintf(buf+ret, " tx:%d rx:%d",
		       port->icount.tx, port->icount.rx);

	if (port->icount.frame)
		ret += sprintf(buf+ret, " fe:%d", port->icount.frame);

	if (port->icount.parity)
		ret += sprintf(buf+ret, " pe:%d", port->icount.parity);

	if (port->icount.brk)
		ret += sprintf(buf+ret, " brk:%d", port->icount.brk);

	if (port->icount.overrun)
		ret += sprintf(buf+ret, " oe:%d", port->icount.overrun);

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

	len += sprintf(page, "serinfo:1.0 driver:%s\n", serial_version);
	for (i = 0; i < NR_PORTS && len < 4000; i++) {
		l = line_info(page + len, &rs_ports[i]);
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

static void rs_close (void *ptr)
{
#if 0
	struct rs_port *port = ptr;
	free_irq(port->irq, port);
#endif
	MOD_DEC_USE_COUNT;
}

/* I haven't the foggiest why the decrement use count has to happen
   here. The whole linux serial drivers stuff needs to be redesigned.
   My guess is that this is a hack to minimize the impact of a bug
   elsewhere. Thinking about it some more. (try it sometime) Try
   running minicom on a serial port that is driven by a modularized
   driver. Have the modem hangup. Then remove the driver module. Then
   exit minicom.  I expect an "oops".  -- REW */
static void rs_hungup (void *ptr)
{
	MOD_DEC_USE_COUNT;
}

static void rs_getserial (void *ptr, struct serial_struct *sp)
{
	struct rs_port *port = ptr;
	struct tty_struct *tty = port->gs.tty;
	/* some applications (busybox, dbootstrap, etc.) look this */
	sp->line = MINOR(tty->device) - tty->driver.minor_start;
}

/*
 * rs_break() --- routine which turns the break handling on or off
 */
static void rs_break(struct tty_struct *tty, int break_state)
{
	struct rs_port *port = tty->driver_data;
	unsigned long flags;

	if (!port->base)
		return;
	local_irq_save(flags);
	if (break_state == -1)
		sio_set(port, TXX9_SIFLCR, TXX9_SIFLCR_TBRK);
	else
		sio_mask(port, TXX9_SIFLCR, TXX9_SIFLCR_TBRK);
	local_irq_restore(flags);
}

static int get_modem_info(struct rs_port *port, unsigned int *value)
{
	unsigned int result;
	unsigned long flags;

	local_irq_save(flags);
	result =  ((sio_in(port, TXX9_SIFLCR) & TXX9_SIFLCR_RTSSC) ? 0 : TIOCM_RTS)
		| ((sio_in(port, TXX9_SICISR) & TXX9_SICISR_CTSS) ? 0 : TIOCM_CTS);
	local_irq_restore(flags);
	return put_user(result,value);
}

static int set_modem_info(struct rs_port *port, unsigned int cmd,
			  unsigned int *value)
{
	int error = 0;
	unsigned int arg;
	unsigned long flags;

	if (copy_from_user(&arg, value, sizeof(int)))
		return -EFAULT;

	local_irq_save(flags);
	switch (cmd) {
	case TIOCMBIS:
		if (arg & TIOCM_RTS)
			sio_mask(port, TXX9_SIFLCR,
				 TXX9_SIFLCR_RTSSC | TXX9_SIFLCR_RSDE);
		break;
	case TIOCMBIC:
		if (arg & TIOCM_RTS)
			sio_set(port, TXX9_SIFLCR,
				TXX9_SIFLCR_RTSSC | TXX9_SIFLCR_RSDE);
		break;
	case TIOCMSET:
		if (arg & TIOCM_RTS)
			sio_mask(port, TXX9_SIFLCR,
				 TXX9_SIFLCR_RTSSC | TXX9_SIFLCR_RSDE);
		else
			sio_set(port, TXX9_SIFLCR,
				TXX9_SIFLCR_RTSSC | TXX9_SIFLCR_RSDE);
		break;
	default:
		error = -EINVAL;
	}
	local_irq_restore(flags);
	return error;
}

static int rs_ioctl (struct tty_struct * tty, struct file * filp,
                     unsigned int cmd, unsigned long arg)
{
	int rc;
	struct rs_port *port = tty->driver_data;
	int ival;

	rc = 0;
	switch (cmd) {
	case TIOCMGET:
		return get_modem_info(port, (unsigned int *) arg);
	case TIOCMBIS:
	case TIOCMBIC:
	case TIOCMSET:
		return set_modem_info(port, cmd, (unsigned int *) arg);
		return 0;
	case TIOCGSOFTCAR:
		rc = put_user(((tty->termios->c_cflag & CLOCAL) ? 1 : 0),
		              (unsigned int *) arg);
		break;
	case TIOCSSOFTCAR:
		if ((rc = verify_area(VERIFY_READ, (void *) arg,
		                      sizeof(int))) == 0) {
			get_user(ival, (unsigned int *) arg);
			tty->termios->c_cflag =
				(tty->termios->c_cflag & ~CLOCAL) |
				(ival ? CLOCAL : 0);
		}
		break;
	case TIOCGSERIAL:
		if ((rc = verify_area(VERIFY_WRITE, (void *) arg,
		                      sizeof(struct serial_struct))) == 0)
			rc = gs_getserial(&port->gs, (struct serial_struct *) arg);
		break;
	case TIOCSSERIAL:
		if ((rc = verify_area(VERIFY_READ, (void *) arg,
		                      sizeof(struct serial_struct))) == 0)
			rc = gs_setserial(&port->gs, (struct serial_struct *) arg);
		break;
	default:
		rc = -ENOIOCTLCMD;
		break;
	}

	return rc;
}


/*
 * This function is used to send a high-priority XON/XOFF character to
 * the device
 */
static void rs_send_xchar(struct tty_struct * tty, char ch)
{
	struct rs_port *port = (struct rs_port *)tty->driver_data;

	port->x_char = ch;
	if (ch) {
		/* Make sure transmit interrupts are on */
		rs_enable_tx_interrupts(tty);
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
	struct rs_port *port = (struct rs_port *)tty->driver_data;
	unsigned long flags;

	if (I_IXOFF(tty))
		rs_send_xchar(tty, STOP_CHAR(tty));
	if (tty->termios->c_cflag & CRTSCTS) {
		local_irq_save(flags);
		/* drop RTS */
		sio_set(port, TXX9_SIFLCR,
			TXX9_SIFLCR_RTSSC | TXX9_SIFLCR_RSDE);
		local_irq_restore(flags);
	}
}

static void rs_unthrottle(struct tty_struct * tty)
{
	struct rs_port *port = tty->driver_data;
	unsigned long flags;

	if (I_IXOFF(tty)) {
		if (port->x_char)
			port->x_char = 0;
		else
			rs_send_xchar(tty, START_CHAR(tty));
	}
	if (tty->termios->c_cflag & CRTSCTS) {
		local_irq_save(flags);
		sio_mask(port, TXX9_SIFLCR,
			 TXX9_SIFLCR_RTSSC | TXX9_SIFLCR_RSDE);
		local_irq_restore(flags);
	}
}

/* ********************************************************************** *
 *                    Here are the initialization routines.               *
 * ********************************************************************** */

static inline void show_serial_version(void)
{
 	printk(KERN_INFO "%s version %s\n", serial_name, serial_version);
}

static int rs_init_portstructs(void)
{
	struct rs_port *port;
	int i;

	/* Adjust the values in the "driver" */
	rs_driver.termios = rs_termios;
	rs_driver.termios_locked = rs_termios_locked;

	port = rs_ports;
	for (i=0; i < NR_PORTS;i++) {
		port->gs.callout_termios = tty_std_termios;
		port->gs.normal_termios	= tty_std_termios;
		port->gs.magic = TXX9_SERIAL_MAGIC;
		port->gs.close_delay = HZ/2;
		port->gs.closing_wait = 30 * HZ;
		port->gs.rd = &rs_real_driver;
#ifdef NEW_WRITE_LOCKING
		port->gs.port_write_sem = MUTEX;
#endif
#ifdef DECLARE_WAITQUEUE
		init_waitqueue_head(&port->gs.open_wait);
		init_waitqueue_head(&port->gs.close_wait);
#endif
		port++;
	}

	return 0;
}

static int rs_init_drivers(void)
{
	int error;

	memset(&rs_driver, 0, sizeof(rs_driver));
	rs_driver.magic = TTY_DRIVER_MAGIC;
	rs_driver.driver_name = "serial_txx9";
#if defined(CONFIG_DEVFS_FS)
	rs_driver.name = TXX9_TTY_DEVFS_NAME;
#else
	rs_driver.name = TXX9_TTY_NAME;
#endif
	rs_driver.major = TXX9_TTY_MAJOR;
	rs_driver.minor_start = TXX9_TTY_MINOR_START;
	rs_driver.num = NR_PORTS;
	rs_driver.type = TTY_DRIVER_TYPE_SERIAL;
	rs_driver.subtype = SERIAL_TYPE_NORMAL;
	rs_driver.init_termios = tty_std_termios;
	rs_driver.init_termios.c_cflag =
		B9600 | CS8 | CREAD | HUPCL | CLOCAL;
	rs_driver.refcount = &rs_refcount;
	rs_driver.table = rs_table;
	rs_driver.termios = rs_termios;
	rs_driver.termios_locked = rs_termios_locked;

	rs_driver.open	= rs_open;
	rs_driver.close = gs_close;
	rs_driver.write = gs_write;
	rs_driver.put_char = gs_put_char;
	rs_driver.flush_chars = gs_flush_chars;
	rs_driver.write_room = gs_write_room;
	rs_driver.chars_in_buffer = gs_chars_in_buffer;
	rs_driver.flush_buffer = gs_flush_buffer;
	rs_driver.ioctl = rs_ioctl;
	rs_driver.throttle = rs_throttle;
	rs_driver.unthrottle = rs_unthrottle;
	rs_driver.set_termios = gs_set_termios;
	rs_driver.stop = gs_stop;
	rs_driver.start = gs_start;
	rs_driver.hangup = gs_hangup;
	rs_driver.break_ctl = rs_break;
	rs_driver.read_proc = rs_read_proc;

	rs_callout_driver = rs_driver;
#if defined(CONFIG_DEVFS_FS)
	rs_callout_driver.name = TXX9_CU_DEVFS_NAME;
#else
	rs_callout_driver.name = TXX9_CU_NAME;
#endif
	rs_callout_driver.major = TXX9_TTYAUX_MAJOR;
	rs_callout_driver.subtype = SERIAL_TYPE_CALLOUT;
	rs_callout_driver.read_proc = 0;
	rs_callout_driver.proc_entry = 0;

	if ((error = tty_register_driver(&rs_driver))) {
		printk(KERN_ERR
		       "Couldn't register serial driver, error = %d\n",
		       error);
		return 1;
	}
	if ((error = tty_register_driver(&rs_callout_driver))) {
		tty_unregister_driver(&rs_driver);
		printk(KERN_ERR
		       "Couldn't register callout driver, error = %d\n",
		       error);
		return 1;
	}

	return 0;
}

/*
 * This routine is called by txx9_rs_init() to initialize a specific serial
 * port.
 */
static void txx9_config(struct rs_port *port)
{
	unsigned long flags;

	if (port - &rs_ports[0] != sercons.index) {
		local_irq_save(flags);
		/*
		 * Reset the UART.
		 */
		sio_out(port, TXX9_SIFCR, TXX9_SIFCR_SWRST);
#ifdef CONFIG_CPU_TX49XX
		/* TX4925 BUG WORKAROUND.  Accessing SIOC register
		 * immediately after soft reset causes bus error. */
		wbflush();/* change iob(); */
		udelay(1);
#endif
		while (sio_in(port, TXX9_SIFCR) & TXX9_SIFCR_SWRST)
			;
		/* TX Int by FIFO Empty, RX Int by Receiving 1 char. */
		sio_set(port, TXX9_SIFCR,
			TXX9_SIFCR_TDIL_MAX | TXX9_SIFCR_RDIL_1);
		/* initial settings */
		sio_out(port, TXX9_SILCR,
			TXX9_SILCR_UMODE_8BIT | TXX9_SILCR_USBL_1BIT |
			TXX9_SILCR_SCS_IMCLK_BG);
		sio_out(port, TXX9_SIBGR,
			((port->baud_base + 9600 / 2) / 9600) |
			TXX9_SIBGR_BCLK_T0);
		local_irq_restore(flags);
	}
	DBG("txx9_config: port->io_type is %d\n", port->io_type);
	if (port->io_type < 0)
		request_mem_region(port->base, 36, "serial_txx9");
	else
		request_region(port->base, 36, "serial_txx9");
}

#ifdef ENABLE_SERIAL_TXX9_PCI
static int __devinit serial_txx9_init_one(struct pci_dev *dev,
					  const struct pci_device_id *ent)
{
	int rc, i;
	struct rs_port *port;

	rc = pci_enable_device(dev);
	if (rc) return rc;

	/* find empty slot */
	for (i = 0; i < NR_PORTS && rs_ports[i].base; i++)
		;
	if (i == NR_PORTS)
		return -ENODEV;
	port = &rs_ports[i];
	DBG("port number is %d\n",i);

	port->pci_dev = dev;
	port->base = pci_resource_start(dev, 1);

	DBG("port->base is %x\n",(u32)port->base);
	port->io_type = SERIAL_IO_PORT;
	port->irq = dev->irq;
	port->flags |= TXX9_SERIAL_HAVE_CTS_LINE;
	port->baud_base = 66670000 / 16 / 2;	/* 66.67MHz */
	DBG("port->baud_base %x\n",port->baud_base);

	txx9_config(port);

	printk(KERN_INFO
		"%s%d at 0x%08lx (irq = %d) is a TX39/49 SIO\n",
		TXX9_TTY_NAME, i, port->base, port->irq);
	return 0;
}

static void __devexit serial_txx9_remove_one(struct pci_dev *dev)
{
	int i;
	for (i = 0; i < NR_PORTS; i++) {
		if (rs_ports[i].pci_dev == dev) {
			rs_ports[i].irq = 0;
			rs_ports[i].base = 0;
			rs_ports[i].pci_dev = 0;
			/* XXX NOT IMPLEMENTED YET */
			break;
		}
	}
}

static struct pci_device_id serial_txx9_pci_tbl[] __devinitdata = {
#ifdef PCI_DEVICE_ID_TOSHIBA_TC86C001_MISC
	{	PCI_VENDOR_ID_TOSHIBA_2, PCI_DEVICE_ID_TOSHIBA_TC86C001_MISC,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0, 
		0 },
#endif
       { 0, }
};

MODULE_DEVICE_TABLE(pci, serial_txx9_pci_tbl);

static struct pci_driver serial_txx9_pci_driver = {
       name:           "serial_txx9",
       probe:          serial_txx9_init_one,
       remove:	       __devexit_p(serial_txx9_remove_one),
       id_table:       serial_txx9_pci_tbl,
};

/*
 * Query PCI space for known serial boards
 * If found, add them to the PCI device space in rs_table[]
 */
static void __devinit probe_serial_txx9_pci(void) 
{
	/* Register call PCI serial devices.  Null out
	 * the driver name upon failure, as a signal
	 * not to attempt to unregister the driver later
	 */
	if (pci_module_init (&serial_txx9_pci_driver) != 0)
		serial_txx9_pci_driver.name = "";

	return;
}
#endif /* ENABLE_SERIAL_TXX9_PCI */

static int __init txx9_rs_init(void)
{
	int rc;
	struct rs_port *port;
	int i;

#ifndef ENABLE_SERIAL_TXX9_PCI
	for (i = 0, port = &rs_ports[0]; i < NR_PORTS; i++,port++) {
		if (port->base)
			goto config_ok;
	}
	return -ENODEV;
 config_ok:
#endif

	show_serial_version();
	rc = rs_init_portstructs ();
	rs_init_drivers ();
	for (i = 0, port = &rs_ports[0]; i < NR_PORTS; i++,port++) {
		if (!port->base)
			continue;
		if (port->io_type < 0) {
			if (check_mem_region(port->base, 36))
				continue;
		} else {
			if (check_region(port->base, 36))
				continue;
		}
		txx9_config(port);
		printk(KERN_INFO
		       "%s%d at 0x%08lx (irq = %d) is a TX39/49 SIO\n",
		       TXX9_TTY_NAME, i, port->base, port->irq);
	}

	/* Note: I didn't do anything to enable the second UART */
	if (rc >= 0)
		rs_initialized++;

#ifdef ENABLE_SERIAL_TXX9_PCI
	probe_serial_txx9_pci();
#endif
	return 0;
}

/*
 * This is for use by architectures that know their serial console
 * attributes only at run time. Not to be invoked after rs_init().
 */
int __init early_serial_txx9_setup(int line, unsigned long base, int irq,
				   int baud_base, int have_cts)
{
	if (line >= NR_PORTS)
		return(-ENOENT);
	rs_ports[line].base = base;
	rs_ports[line].irq = irq;
	rs_ports[line].baud_base = baud_base;
	rs_ports[line].io_type = -1;	/* virtual memory mapped */
	if (have_cts)
		rs_ports[line].flags |= TXX9_SERIAL_HAVE_CTS_LINE;
	return(0);
}

static void __exit txx9_rs_fini(void)
{
	unsigned long flags;
	int e1, e2;
	int i;

	local_irq_save(flags);
	if ((e1 = tty_unregister_driver(&rs_driver)))
		printk("serial: failed to unregister serial driver (%d)\n",
		       e1);
	if ((e2 = tty_unregister_driver(&rs_callout_driver)))
		printk("serial: failed to unregister callout driver (%d)\n",
		       e2);
	local_irq_restore(flags);

	for (i = 0; i < NR_PORTS; i++) {
		if (!rs_ports[i].base)
			continue;
		if (rs_ports[i].io_type < 0)
			release_mem_region(rs_ports[i].base, 36);
		else
			release_region(rs_ports[i].base, 36);
	}

#ifdef ENABLE_SERIAL_PCI
	if (serial_txx9_pci_driver.name[0])
		pci_unregister_driver (&serial_txx9_pci_driver);
#endif
}

module_init(txx9_rs_init);
module_exit(txx9_rs_fini);
MODULE_DESCRIPTION("TX39/49 serial driver");
MODULE_AUTHOR("TOSHIBA Corporation");
MODULE_LICENSE("GPL");

/*
 * Begin serial console routines
 */
#ifdef CONFIG_SERIAL_TXX9_CONSOLE

/*
 *	Wait for transmitter & holding register to empty
 */
static inline void wait_for_xmitr(struct rs_port *port)
{
	unsigned int tmout = 1000000;

	do {
		if (--tmout == 0) break;
	} while (!(sio_in(port, TXX9_SICISR) & TXX9_SICISR_TXALS));

	/* Wait for flow control if necessary */
#if (ASYNC_INTERNAL_FLAGS & GS_INTERNAL_FLAGS) == 0	/* check conflict... */
	if (port->gs.flags & ASYNC_CONS_FLOW) {
		tmout = 1000000;
		while (--tmout &&
		       (sio_in(port, TXX9_SICISR) & TXX9_SICISR_CTSS));
	}
#endif
}

/*
 *	Print a string to the serial port trying not to disturb
 *	any possible real use of the port...
 *
 *	The console_lock must be held when we get here.
 */
static void serial_console_write(struct console *co, const char *s,
				 unsigned count)
{
	struct rs_port *port = &rs_ports[co->index];
	int ier;
	unsigned i;

	/*
	 *	First save the IER then disable the interrupts
	 */
	ier = sio_in(port, TXX9_SIDICR);
	sio_out(port, TXX9_SIDICR, 0);

	/*
	 *	Now, do each character
	 */
	for (i = 0; i < count; i++, s++) {
		wait_for_xmitr(port);

		/*
		 *	Send the character out.
		 *	If a LF, also do CR...
		 */
		sio_out(port, TXX9_SITFIFO, *s);
		if (*s == 10) {
			wait_for_xmitr(port);
			sio_out(port, TXX9_SITFIFO, 13);
		}
	}

	/*
	 *	Finally, Wait for transmitter & holding register to empty
	 * 	and restore the IER
	 */
	wait_for_xmitr(port);
	sio_out(port, TXX9_SIDICR, ier);
}

static kdev_t serial_console_device(struct console *c)
{
	return MKDEV(TXX9_TTY_MAJOR, TXX9_TTY_MINOR_START + c->index);
}

static __init int serial_console_setup(struct console *co, char *options)
{
	struct rs_port *port;
	unsigned cval;
	int	baud = 9600;
	int	bits = 8;
	int	parity = 'n';
	int	doflow = 0;
	int	cflag = CREAD | HUPCL | CLOCAL;
	int	quot = 0;
	char	*s;

	if (co->index < 0 || co->index >= NR_PORTS)
		return -1;
	if (options) {
		baud = simple_strtoul(options, NULL, 10);
		s = options;
		while(*s >= '0' && *s <= '9')
			s++;
		if (*s) parity = *s++;
		if (*s) bits   = *s - '0';
		if (*s) doflow = (*s++ == 'r');
	}

	/*
	 *	Now construct a cflag setting.
	 */
	switch(baud) {
	case 1200:	cflag |= B1200;	break;
	case 2400:	cflag |= B2400;	break;
	case 4800:	cflag |= B4800;	break;
	case 19200:	cflag |= B19200;	break;
	case 38400:	cflag |= B38400;	break;
	case 57600:	cflag |= B57600;	break;
	case 115200:	cflag |= B115200;	break;
	default:
		/*
		 * Set this to a sane value to prevent a divide error
		 */
		baud  = 9600;
	case 9600:	cflag |= B9600;		break;
	}
	
	switch(bits) {
	case 7:		cflag |= CS7;	break;
	default:
	case 8:		cflag |= CS8;	break;
	}
	switch(parity) {
	case 'o': case 'O':	cflag |= PARODD;	break;
	case 'e': case 'E':	cflag |= PARENB;	break;
	}
	co->cflag = cflag;

	port = &rs_ports[co->index];
	if (!port->base)
		return -1;

	/*
	 *	Divisor, bytesize and parity
	 */
#if (ASYNC_INTERNAL_FLAGS & GS_INTERNAL_FLAGS) == 0	/* check conflict... */
	if (doflow)
		port->gs.flags |= ASYNC_CONS_FLOW;
#endif
	quot = port->baud_base / baud;
	switch (cflag & CSIZE) {
	case CS7: cval = TXX9_SILCR_UMODE_7BIT; break;
	default:
	case CS8: cval = TXX9_SILCR_UMODE_8BIT; break;
	}
	if (cflag & CSTOPB)
		cval |= TXX9_SILCR_USBL_2BIT;
	else
		cval |= TXX9_SILCR_USBL_1BIT;
	if (cflag & PARENB)
		cval |= TXX9_SILCR_UPEN;
	if (!(cflag & PARODD))
		cval |= TXX9_SILCR_UEPS;

	/*
	 *	Disable UART interrupts, set DTR and RTS high
	 *	and set speed.
	 */
	sio_out(port, TXX9_SIDICR, 0);
	sio_out(port, TXX9_SILCR, cval | TXX9_SILCR_SCS_IMCLK_BG);
	sio_out(port, TXX9_SIBGR, quot | TXX9_SIBGR_BCLK_T0);
	/* no RTS/CTS control */
	sio_out(port, TXX9_SIFLCR, TXX9_SIFLCR_RTSTL_MAX /* 15 */);
	/* Enable RX/TX */
	sio_mask(port, TXX9_SIFLCR, TXX9_SIFLCR_RSDE | TXX9_SIFLCR_TSDE);

	/* console port should not use RTC/CTS. */
	port->flags &= ~TXX9_SERIAL_HAVE_CTS_LINE;
	return 0;
}

static struct console sercons = {
	name:		TXX9_TTY_NAME,
	write:		serial_console_write,
	device:		serial_console_device,
	setup:		serial_console_setup,
	flags:		CON_PRINTBUFFER,
	index:		-1,
};

void __init txx9_serial_console_init(void)
{
	register_console(&sercons);
}

#endif

/******************************************************************************/
/* BEG: KDBG Routines                                                         */
/******************************************************************************/

#ifdef CONFIG_KGDB
int kgdb_init_count = 0;
#endif

#ifdef CONFIG_KGDB
void txx9_sio_kgdb_hook(unsigned int port, unsigned int baud_rate)
{
	static struct resource kgdb_resource;
	int ret;

	/* prevent initialization by driver */
	kgdb_resource.name = "serial_txx9(debug)";
	kgdb_resource.start = rs_ports[port].base;
	kgdb_resource.end = rs_ports[port].base + 36 - 1;
	kgdb_resource.flags = IORESOURCE_MEM | IORESOURCE_BUSY;

	ret = request_resource(&iomem_resource, &kgdb_resource);
	if(ret == -EBUSY)
		printk(" serial_txx9(debug): request_resource failed\n");

	return;
}
#endif /* CONFIG_KGDB */

#ifdef CONFIG_KGDB
void
txx9_sio_kdbg_init( unsigned int port_number )
{
  if ( port_number == 1 ) {
    txx9_sio_kgdb_hook( port_number, 38400 );
  } else {
	printk("Bad Port Number [%u] != [1]\n",port_number);
  }
  return; 
}
#endif /* CONFIG_KGDB */

#ifdef CONFIG_KGDB
u8 
txx9_sio_kdbg_rd( void )
{
    unsigned int status,ch;

  if ( kgdb_init_count == 0 )
  {
    txx9_sio_kdbg_init( 1 );
    kgdb_init_count = 1;
  }

  while ( 1 )
  {
    status = sio_in(&rs_ports[1], TXX9_SIDISR);
    if ( status & 0x1f )
    {
      ch = sio_in(&rs_ports[1], TXX9_SIRFIFO );
      break;
    }
  }

  return( ch );
}
#endif /* CONFIG_KGDB */

#ifdef CONFIG_KGDB
int 
txx9_sio_kdbg_wr( u8 ch )
{
    unsigned int status;

  if ( kgdb_init_count == 0 )
  {
    txx9_sio_kdbg_init( 1 );
    kgdb_init_count = 1;
  }

  while ( 1 )
  {
    status = sio_in(&rs_ports[1], TXX9_SICISR);
    if (status & TXX9_SICISR_TRDY)
    {
      if ( ch == '\n' )
      {
        txx9_sio_kdbg_wr( '\r' );
      }
      sio_out(&rs_ports[1], TXX9_SITFIFO, (u32)ch );

      break;
    }
  }

  return( 1 );
}
#endif /* CONFIG_KGDB */

/******************************************************************************/
/* END: KDBG Routines                                                         */
/******************************************************************************/

void txx9_raw_output(char c)
{
	struct rs_port *port = &rs_ports[0];
	if ( c == '\n' )
	{
	  sio_out(port, TXX9_SITFIFO, '\r');
	  wait_for_xmitr(port);
	}
	sio_out(port, TXX9_SITFIFO, c);
	wait_for_xmitr(port);
	return;
}
