/*
 *  drivers/char/serial_tx3912.c
 *
 *  Copyright (C) 2001 Steven J. Hill (sjhill@realitydiluted.com)
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *  
 *  Serial driver for TMPR3912/05 and PR31700 processors
 */
#include <linux/init.h>
#include <linux/config.h>
#include <linux/tty.h>
#include <linux/major.h>
#include <linux/console.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/serial.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/delay.h>
#include <asm/tx3912.h>
#include "serial_tx3912.h"

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
}; 

/*
 * Structures and usage counts
 */
static struct tty_driver rs_driver, rs_callout_driver;
static struct tty_struct **rs_tty;
static struct termios **rs_termios;
static struct termios **rs_termios_locked;
static struct rs_port *rs_port;
static int rs_refcount;
static int rs_initialized;


/*
 * Receive a character
 */
static inline void receive_char_pio(struct rs_port *port)
{
	struct tty_struct *tty = port->gs.tty;
	unsigned char ch;
	int counter = 2048;

	/* While there are characters */
	while (counter > 0) {
		if (!(inl(TX3912_UARTA_CTRL1) & TX3912_UART_CTRL1_RXHOLDFULL))
			break;
		ch = inb(TX3912_UARTA_DATA);
		if (tty->flip.count < TTY_FLIPBUF_SIZE) {
			*tty->flip.char_buf_ptr++ = ch;
			*tty->flip.flag_buf_ptr++ = 0;
			tty->flip.count++;
		}
		udelay(1);
		counter--;
	}

	tty_flip_buffer_push(tty);
}

/*
 * Transmit a character
 */
static inline void transmit_char_pio(struct rs_port *port)
{
	/* TX while bytes available */
	for (;;) {
		if (!(inl(TX3912_UARTA_CTRL1) & TX3912_UART_CTRL1_EMPTY))
			break;
		else if (port->x_char) {
			outb(port->x_char, TX3912_UARTA_DATA);
			port->icount.tx++;
			port->x_char = 0;
		}
		else if (port->gs.xmit_cnt <= 0 || port->gs.tty->stopped ||
		    port->gs.tty->hw_stopped) {
			break;
		}
		else {
			outb(port->gs.xmit_buf[port->gs.xmit_tail++],
				TX3912_UARTA_DATA);
			port->icount.tx++;
			port->gs.xmit_tail &= SERIAL_XMIT_SIZE-1;
			if (--port->gs.xmit_cnt <= 0) {
				break;
			}
		}
		udelay(10);
	}

	if (port->gs.xmit_cnt <= 0 || port->gs.tty->stopped ||
	     port->gs.tty->hw_stopped) {
		rs_disable_tx_interrupts(port);
	}
	
        if (port->gs.xmit_cnt <= port->gs.wakeup_chars) {
                if ((port->gs.tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) &&
                    port->gs.tty->ldisc.write_wakeup)
                        (port->gs.tty->ldisc.write_wakeup)(port->gs.tty);
                rs_dprintk(TX3912_UART_DEBUG_TRANSMIT, "Waking up.... ldisc (%d)....\n",
                            port->gs.wakeup_chars); 
                wake_up_interruptible(&port->gs.tty->write_wait);
       	}	
}

/*
 * We don't have MSR
 */
static inline void check_modem_status(struct rs_port *port)
{
	wake_up_interruptible(&port->gs.open_wait);
}

/*
 * RX interrupt handler
 */
static inline void rs_rx_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned long flags, status;

	save_and_cli(flags);

	rs_dprintk(TX3912_UART_DEBUG_INTERRUPTS, "rs_rx_interrupt...");

	/* Get the interrupts */
	status = inl(TX3912_INT2_STATUS);

	/* Clear any interrupts we might be about to handle */
	outl(TX3912_INT2_UARTA_RX_BITS, TX3912_INT2_CLEAR);

	if(!rs_port || !rs_port->gs.tty) {
		restore_flags(flags);
		return;
	}

	/* RX Receiver Holding Register Overrun */
	if(status & TX3912_INT2_UARTATXOVERRUNINT) {
		rs_dprintk(TX3912_UART_DEBUG_INTERRUPTS, "overrun");
		rs_port->icount.overrun++;
	}

	/* RX Frame Error */
	if(status & TX3912_INT2_UARTAFRAMEERRINT) {
		rs_dprintk(TX3912_UART_DEBUG_INTERRUPTS, "frame error");
		rs_port->icount.frame++;
	}

	/* Break signal received */
	if(status & TX3912_INT2_UARTABREAKINT) {
		rs_dprintk(TX3912_UART_DEBUG_INTERRUPTS, "break");
		rs_port->icount.brk++;
      	}

	/* RX Parity Error */
	if(status & TX3912_INT2_UARTAPARITYINT) {
		rs_dprintk(TX3912_UART_DEBUG_INTERRUPTS, "parity error");
		rs_port->icount.parity++;
	}

	/* Byte received */
	if(status & TX3912_INT2_UARTARXINT) {
		receive_char_pio(rs_port);
	}

	restore_flags(flags);

	rs_dprintk(TX3912_UART_DEBUG_INTERRUPTS, "end.\n");
}

/*
 * TX interrupt handler
 */
static inline void rs_tx_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned long flags, status;

	save_and_cli(flags);

	rs_dprintk(TX3912_UART_DEBUG_INTERRUPTS, "rs_tx_interrupt...");

	/* Get the interrupts */
	status = inl(TX3912_INT2_STATUS);

	if(!rs_port || !rs_port->gs.tty) {
		restore_flags(flags);
		return;
	}

	/* Clear interrupts */
	outl(TX3912_INT2_UARTA_TX_BITS, TX3912_INT2_CLEAR);

	/* TX holding register empty - transmit a byte */
	if(status & TX3912_INT2_UARTAEMPTYINT) {
		transmit_char_pio(rs_port);
	}

	/* TX Transmit Holding Register Overrun (shouldn't happen) */
	if(status & TX3912_INT2_UARTATXOVERRUNINT) {
		printk( "rs_tx_interrupt: TX overrun\n");
	}

	restore_flags(flags);

	rs_dprintk(TX3912_UART_DEBUG_INTERRUPTS, "end.\n");
}

/*
 * Here are the routines that actually interface with the generic driver
 */
static void rs_disable_tx_interrupts (void * ptr) 
{
	unsigned long flags;

	save_and_cli(flags);

	outl(inl(TX3912_INT2_ENABLE) & ~TX3912_INT2_UARTA_TX_BITS,
		TX3912_INT2_ENABLE);
	outl(TX3912_INT2_UARTA_TX_BITS, TX3912_INT2_CLEAR);

	restore_flags(flags);
}

static void rs_enable_tx_interrupts (void * ptr) 
{
	unsigned long flags;

	save_and_cli(flags);

	outl(TX3912_INT2_UARTA_TX_BITS, TX3912_INT2_CLEAR);
	outl(inl(TX3912_INT2_ENABLE) | TX3912_INT2_UARTA_TX_BITS,
		TX3912_INT2_ENABLE);
	transmit_char_pio(rs_port);

	restore_flags(flags);
}

static void rs_disable_rx_interrupts (void * ptr) 
{
	unsigned long flags;

	save_and_cli(flags);

	outl(inl(TX3912_INT2_ENABLE) & ~TX3912_INT2_UARTA_RX_BITS,
		TX3912_INT2_ENABLE);
	outl(TX3912_INT2_UARTA_RX_BITS, TX3912_INT2_CLEAR);

	restore_flags(flags);
}

static void rs_enable_rx_interrupts (void * ptr) 
{
	unsigned long flags;

	save_and_cli(flags);

	outl(inl(TX3912_INT2_ENABLE) | TX3912_INT2_UARTA_RX_BITS,
		TX3912_INT2_ENABLE);
	while (inl(TX3912_UARTA_CTRL1) & TX3912_UART_CTRL1_RXHOLDFULL)
		inb(TX3912_UARTA_DATA);
	outl(TX3912_INT2_UARTA_RX_BITS, TX3912_INT2_CLEAR);

	restore_flags(flags);
}

/*
 * We have no CD
 */
static int rs_get_CD (void * ptr) 
{
	return 1;
}

/*
 * Shut down the port
 */
static void rs_shutdown_port (void * ptr) 
{
	func_enter();
	rs_port->gs.flags &= ~GS_ACTIVE;
	func_exit();
}

static int rs_set_real_termios (void *ptr)
{
	unsigned int ctrl1 = 0;
	unsigned int ctrl2 = 0;

	/* Set baud rate */
	switch (rs_port->gs.baud) {
		case 0:
			goto done;
		case 1200:
			ctrl2 = TX3912_UART_CTRL2_B1200;
			break;
		case 2400:
			ctrl2 = TX3912_UART_CTRL2_B2400;
			break;
		case 4800:
			ctrl2 = TX3912_UART_CTRL2_B4800;
			break;
		case 9600:
			ctrl2 = TX3912_UART_CTRL2_B9600;
			break;
		case 19200:
			ctrl2 = TX3912_UART_CTRL2_B19200;
			break;
		case 38400:
			ctrl2 = TX3912_UART_CTRL2_B38400;
			break;
		case 57600:
			ctrl2 = TX3912_UART_CTRL2_B57600;
			break;
		case 115200:
		default:
			ctrl2 = TX3912_UART_CTRL2_B115200;
			break;
	}

  	/* Clear current UARTA settings */
	ctrl1 = inl(TX3912_UARTA_CTRL1) & 0xf000000f;

	/* Set parity */
	if(C_PARENB(rs_port->gs.tty)) {
		if (!C_PARODD(rs_port->gs.tty))
			ctrl1 |= (TX3912_UART_CTRL1_ENPARITY |
				 TX3912_UART_CTRL1_EVENPARITY);
		else
			ctrl1 |= TX3912_UART_CTRL1_ENPARITY;
	}

	/* Set data size */
	switch(rs_port->gs.tty->termios->c_cflag & CSIZE) {
		case CS7:
			ctrl1 |= TX3912_UART_CTRL1_BIT_7;
			break;
		case CS5:
		case CS6:
			printk(KERN_ERR "Data byte size unsupported. Defaulting to CS8\n");
		case CS8:
		default:
			ctrl1 &= ~TX3912_UART_CTRL1_BIT_7;
	}

	/* Set stop bits */
	if(C_CSTOPB(rs_port->gs.tty))
		ctrl1 |= TX3912_UART_CTRL1_TWOSTOP;

	/* Write the control registers */
	outl(ctrl2, TX3912_UARTA_CTRL2);
	outl(0, TX3912_UARTA_DMA_CTRL1);
	outl(0, TX3912_UARTA_DMA_CTRL2);
	outl(ctrl1, TX3912_UARTA_CTRL1);

	/* Loop until the UART is on */
	while(~inl(TX3912_UARTA_CTRL1) & TX3912_UART_CTRL1_UARTON);

done:
	func_exit();
	return 0;
}

/*
 * Anyone in the buffer?
 */
static int rs_chars_in_buffer (void * ptr) 
{
	return ((inl(TX3912_UARTA_CTRL1) & TX3912_UART_CTRL1_EMPTY) ? 0 : 1);
}

/*
 * Open the serial port
 */
static int rs_open(struct tty_struct * tty, struct file * filp)
{
	int retval;

	func_enter();

	if(!rs_initialized) {
		return -EIO;
	}

	if(MINOR(tty->device) - tty->driver.minor_start) {
		return -ENODEV;
	}

	rs_dprintk(TX3912_UART_DEBUG_OPEN, "Serial opening...\n");

	tty->driver_data = rs_port;
	rs_port->gs.tty = tty;
	rs_port->gs.count++;

	/*
	 * Start up serial port
	 */
	retval = gs_init_port(&rs_port->gs);
	rs_dprintk(TX3912_UART_DEBUG_OPEN, "Finished gs_init...\n");
	if(retval) {
		rs_port->gs.count--;
		return retval;
	}

	rs_port->gs.flags |= GS_ACTIVE;
	if(rs_port->gs.count == 1) {
		MOD_INC_USE_COUNT;
	}

	rs_enable_rx_interrupts(rs_port);
	rs_enable_tx_interrupts(rs_port);

	retval = gs_block_til_ready(&rs_port->gs, filp);
	if(retval) {
		MOD_DEC_USE_COUNT;
		rs_port->gs.count--;
		return retval;
	}

	if((rs_port->gs.count == 1) &&
		(rs_port->gs.flags & ASYNC_SPLIT_TERMIOS)) {
		if (tty->driver.subtype == SERIAL_TYPE_NORMAL)
			*tty->termios = rs_port->gs.normal_termios;
		else 
			*tty->termios = rs_port->gs.callout_termios;
		rs_set_real_termios(rs_port);
	}

	rs_port->gs.session = current->session;
	rs_port->gs.pgrp = current->pgrp;
	func_exit();

	return 0;
}

/*
 * Close the serial port
 */
static void rs_close(void *ptr)
{
	func_enter();
	MOD_DEC_USE_COUNT;
	func_exit();
}

/*
 * Hang up the serial port
 */
static void rs_hungup(void *ptr)
{
	func_enter();
	MOD_DEC_USE_COUNT;
	func_exit();
}

/*
 * Serial ioctl call
 */
static int rs_ioctl(struct tty_struct * tty, struct file * filp, 
                     unsigned int cmd, unsigned long arg)
{
	int ival, rc;

	rc = 0;
	switch (cmd) {
		case TIOCGSOFTCAR:
			rc = put_user((tty->termios->c_cflag & CLOCAL) ? 1 : 0,
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
				rc = gs_getserial(&rs_port->gs, (struct serial_struct *) arg);
			break;
		case TIOCSSERIAL:
			if ((rc = verify_area(VERIFY_READ, (void *) arg,
				sizeof(struct serial_struct))) == 0)
				rc = gs_setserial(&rs_port->gs, (struct serial_struct *) arg);
			break;
		default:
			rc = -ENOIOCTLCMD;
			break;
	}

	return rc;
}

/*
 * Send xchar
 */
static void rs_send_xchar(struct tty_struct * tty, char ch)
{
	func_enter();
	
	rs_port->x_char = ch;
	if (ch) {
		rs_enable_tx_interrupts(tty);
	}

	func_exit();
}

/*
 * Throttle characters as directed by upper tty layer
 */
static void rs_throttle(struct tty_struct * tty)
{
#ifdef TX3912_UART_DEBUG_THROTTLE
	char	buf[64];
	
	printk("throttle %s: %d....\n", tty_name(tty, buf),
	       tty->ldisc.chars_in_buffer(tty));
#endif

	func_enter();
	
	if (I_IXOFF(tty))
		rs_send_xchar(tty, STOP_CHAR(tty));

	func_exit();
}

/*
 * Un-throttle characters as directed by upper tty layer
 */
static void rs_unthrottle(struct tty_struct * tty)
{
#ifdef TX3912_UART_DEBUG_THROTTLE
	char	buf[64];
	
	printk("unthrottle %s: %d....\n", tty_name(tty, buf),
	       tty->ldisc.chars_in_buffer(tty));
#endif

	func_enter();
	
	if (I_IXOFF(tty)) {
		if (rs_port->x_char)
			rs_port->x_char = 0;
		else
			rs_send_xchar(tty, START_CHAR(tty));
	}

	func_exit();
}

/*
 * Initialize the serial port
 */
void __init tx3912_rs_init(void)
{
	func_enter();
	rs_dprintk(TX3912_UART_DEBUG_INIT, "Initializing serial...\n");

	/* Allocate critical structures */
	if(!(rs_tty = kmalloc(sizeof(struct tty_struct), GFP_KERNEL))) {
		return;
	}
	if(!(rs_port = kmalloc(sizeof(struct rs_port), GFP_KERNEL))) {
		kfree(rs_tty);
		return;
	}
	if(!(rs_termios = kmalloc(sizeof(struct termios), GFP_KERNEL))) {
		kfree(rs_port);
		kfree(rs_tty);
		return;
	}
	if(!(rs_termios_locked = kmalloc(sizeof(struct termios), GFP_KERNEL))) {
		kfree(rs_termios);
		kfree(rs_port);
		kfree(rs_tty);
		return;
	}

	/* Zero out the structures */
	memset(rs_tty, 0, sizeof(struct tty_struct));
	memset(rs_port, 0, sizeof(struct rs_port));
	memset(rs_termios, 0, sizeof(struct termios));
	memset(rs_termios_locked, 0, sizeof(struct termios));
	memset(&rs_driver, 0, sizeof(rs_driver));
	memset(&rs_callout_driver, 0, sizeof(rs_callout_driver));

	/* Fill in hardware specific port structure */
	rs_port->gs.callout_termios = tty_std_termios;
	rs_port->gs.normal_termios	= tty_std_termios;
	rs_port->gs.magic = SERIAL_MAGIC;
	rs_port->gs.close_delay = HZ/2;
	rs_port->gs.closing_wait = 30 * HZ;
	rs_port->gs.rd = &rs_real_driver;
#ifdef NEW_WRITE_LOCKING
	rs_port->gs.port_write_sem = MUTEX;
#endif
#ifdef DECLARE_WAITQUEUE
	init_waitqueue_head(&rs_port->gs.open_wait);
	init_waitqueue_head(&rs_port->gs.close_wait);
#endif

	/* Fill in generic serial driver structures */
	rs_driver.magic = TTY_DRIVER_MAGIC;
	rs_driver.driver_name = "serial";
	rs_driver.name = "ttyS";
	rs_driver.major = TTY_MAJOR;
	rs_driver.minor_start = 64;
	rs_driver.num = 1;
	rs_driver.type = TTY_DRIVER_TYPE_SERIAL;
	rs_driver.subtype = SERIAL_TYPE_NORMAL;
	rs_driver.init_termios = tty_std_termios;
	rs_driver.init_termios.c_cflag = B115200 | CS8 | CREAD | HUPCL | CLOCAL;
	rs_driver.refcount = &rs_refcount;
	rs_driver.table = rs_tty;
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
	rs_callout_driver = rs_driver;
	rs_callout_driver.name = "cua";
	rs_callout_driver.major = TTYAUX_MAJOR;
	rs_callout_driver.subtype = SERIAL_TYPE_CALLOUT;

	/* Register serial and callout drivers */
	if(tty_register_driver(&rs_driver)) {
		printk(KERN_ERR "Unable to register serial driver\n");
		goto error;
	}
	if(tty_register_driver(&rs_callout_driver)) {
		tty_unregister_driver(&rs_driver);
		printk(KERN_ERR "Unable to register callout driver\n");
		goto error;
	}

	/* Assign IRQs */
	if(request_irq(2, rs_tx_interrupt, SA_SHIRQ,
		"uarta_tx", rs_port)) {
		printk(KERN_ERR "Cannot allocate IRQ for UARTA_TX.\n");
		goto error;
	}

	if(request_irq(3, rs_rx_interrupt, SA_SHIRQ,
		"uarta_rx", rs_port)) {
		printk(KERN_ERR "Cannot allocate IRQ for UARTA_RX.\n");
		goto error;
	}

	/* Enable the serial receive interrupt */
	rs_enable_rx_interrupts(rs_port); 

#ifndef CONFIG_SERIAL_TX3912_CONSOLE
	/* Write the control registers */
	outl(TX3912_UART_CTRL2_B115200, TX3912_UARTA_CTRL2);
	outl(0x00000000, TX3912_UARTA_DMA_CTRL1);
	outl(0x00000000, TX3912_UARTA_DMA_CTRL2);
	outl(inl(TX3912_UARTA_CTRL1) | TX3912_UART_CTRL1_ENUART,
		TX3912_UARTA_CTRL1);

	/* Loop until the UART is on */
	while(~inl(TX3912_UARTA_CTRL1) & TX3912_UART_CTRL1_UARTON);
#endif

	rs_initialized = 1;
	func_exit();
	return;

error:
	kfree(rs_termios_locked);
	kfree(rs_termios);
	kfree(rs_port);
	kfree(rs_tty);
	func_exit();
}

/*
 * Begin serial console routines
 */
#ifdef CONFIG_SERIAL_TX3912_CONSOLE
void serial_outc(unsigned char c)
{
	int i;
	unsigned long int2;

	/* Disable UARTA_TX interrupts */
	int2 = inl(TX3912_INT2_ENABLE);
	outl(inl(TX3912_INT2_ENABLE) & ~TX3912_INT2_UARTA_TX_BITS,
		 TX3912_INT2_ENABLE);

	/* Wait for UARTA_TX register to empty */
	i = 10000;
	while(!(inl(TX3912_INT2_STATUS) & TX3912_INT2_UARTATXINT) && i--);
	outl(TX3912_INT2_UARTA_TX_BITS, TX3912_INT2_CLEAR);

	/* Send the character */
	outl(c, TX3912_UARTA_DATA);

	/* Wait for UARTA_TX register to empty */
	i = 10000;
	while(!(inl(TX3912_INT2_STATUS) & TX3912_INT2_UARTATXINT) && i--);
	outl(TX3912_INT2_UARTA_TX_BITS, TX3912_INT2_CLEAR);

	/* Enable UARTA_TX interrupts */
	outl(int2, TX3912_INT2_ENABLE);
}

static int serial_console_wait_key(struct console *co)
{
	unsigned int int2, res;

	int2 = inl(TX3912_INT2_ENABLE);
	outl(0, TX3912_INT2_ENABLE);

	while (!(inl(TX3912_UARTA_CTRL1) & TX3912_UART_CTRL1_RXHOLDFULL));
	res = inl(TX3912_UARTA_DATA);
	udelay(10);
	
	outl(int2, TX3912_INT2_ENABLE);
	return res;
}

static void serial_console_write(struct console *co, const char *s,
	unsigned count)
{
	unsigned int i;

	for (i = 0; i < count; i++) {
		if (*s == '\n')
			serial_outc('\r');
		serial_outc(*s++);
    	}
}

static kdev_t serial_console_device(struct console *c)
{
	return MKDEV(TTY_MAJOR, 64 + c->index);
}

static __init int serial_console_setup(struct console *co, char *options)
{
	int baud = 115200;
	int bits = 8;
	int parity = 'n';
	char *s;
	unsigned int ctrl1 = 0;
	unsigned int ctrl2 = 0;

	if (options) {
		baud = simple_strtoul(options, NULL, 10);
		s = options;
		while(*s >= '0' && *s <= '9')
			s++;
		if (*s) parity = *s++;
		if (*s) bits   = *s++ - '0';
	}

	switch(baud) {
		case 1200:
			ctrl2 = TX3912_UART_CTRL2_B1200;
			break;
		case 2400:
			ctrl2 = TX3912_UART_CTRL2_B2400;
			break;
		case 4800:
			ctrl2 = TX3912_UART_CTRL2_B4800;
			break;
		case 9600:
			ctrl2 = TX3912_UART_CTRL2_B9600;
			break;
		case 19200:
			ctrl2 = TX3912_UART_CTRL2_B19200;
			break;
		case 38400:
			ctrl2 = TX3912_UART_CTRL2_B38400;
			break;
		case 57600:
			ctrl2 = TX3912_UART_CTRL2_B57600;
			break;
		case 115200:
		default:
			ctrl2 = TX3912_UART_CTRL2_B115200;
			break;
	}

	switch(bits) {
		case 7:
			ctrl1 = TX3912_UART_CTRL1_BIT_7;
			break;
		default:
			break;
	}

	switch(parity) {
		case 'o': case 'O':
			ctrl1 |= TX3912_UART_CTRL1_ENPARITY;
			break;
		case 'e': case 'E':
			ctrl1 |= (TX3912_UART_CTRL1_ENPARITY |
				 TX3912_UART_CTRL1_EVENPARITY);
			break;
		default:
			break;
	}

	/* Write the control registers */
	outl(ctrl2, TX3912_UARTA_CTRL2);
	outl(0x00000000, TX3912_UARTA_DMA_CTRL1);
	outl(0x00000000, TX3912_UARTA_DMA_CTRL2);
	outl((ctrl1 | TX3912_UART_CTRL1_ENUART), TX3912_UARTA_CTRL1);

	/* Loop until the UART is on */
	while(~inl(TX3912_UARTA_CTRL1) & TX3912_UART_CTRL1_UARTON);

	return 0;
}

static struct console sercons = {
	name:     "ttyS",
	write:    serial_console_write,
	device:   serial_console_device,
	setup:    serial_console_setup,
	flags:    CON_PRINTBUFFER,
	index:    -1
};

void __init tx3912_console_init(void)
{
	register_console(&sercons);
}
#endif
