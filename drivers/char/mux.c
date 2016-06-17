/*
** mux.c:
**      MUX console for the NOVA and K-Class systems.
**
**	(c) Copyright 2002 Ryan Bradetich
**	(c) Copyright 2002 Hewlett-Packard Company
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
**
** This Driver used Christoph Plattner's pdc_console.c as a driver
** template.
**
** This Driver currently only supports the console (port 0) on the MUX.
** Additional work will be needed on this driver to enable the full
** functionality of the MUX.
**
*/

static char *mux_drv_version = "0.1";

#include <linux/config.h>
#include <linux/version.h>

#undef SERIAL_PARANOIA_CHECK
#define CONFIG_SERIAL_NOPAUSE_IO
#define SERIAL_DO_RESTART

#include <linux/module.h>
#include <linux/serial.h>
#include <linux/serialP.h>
#include <linux/tty.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <asm/uaccess.h>

#ifdef CONFIG_MAGIC_SYSRQ
#include <linux/sysrq.h>
static unsigned long break_pressed;
#endif

#ifdef CONFIG_GSC
#include <asm/gsc.h>
#endif

static unsigned long hpa;

#define MUX_OFFSET 0x800
#define MUX_LINE_OFFSET 0x80

#define MUX_FIFO_SIZE 255
#define MUX_MIN_FREE_SIZE 32

#define MUX_FIFO_DRAIN_DELAY 1
#define MUX_POLL_DELAY (30 * HZ / 1000)

#define IO_COMMAND_REG_OFFSET 0x30
#define IO_STATUS_REG_OFFSET 0x34
#define IO_DATA_REG_OFFSET 0x3c
#define IO_DCOUNT_REG_OFFSET 0x40
#define IO_UCOUNT_REG_OFFSET 0x44
#define IO_FIFOS_REG_OFFSET 0x48

#define MUX_EOFIFO(status) ((status & 0xF000) == 0xF000)
#define MUX_STATUS(status) ((status & 0xF000) == 0x8000)
#define MUX_BREAK(status) ((status & 0xF000) == 0x2000)

static int mux_drv_refcount; /* = 0 */
static struct tty_driver mux_drv_driver;
static struct async_struct *mux_drv_info;
static struct timer_list mux_drv_timer;

#define NR_PORTS 1
static struct tty_struct *mux_drv_table[NR_PORTS];
static struct termios *mux_drv_termios[NR_PORTS];
static struct termios *mux_drv_termios_locked[NR_PORTS];

/**
 * mux_read_fifo - Read chars from the mux fifo.
 * @info: Ptr to the async structure.
 *
 * This reads all available data from the mux's fifo and pushes
 * the data to the tty layer.
 */
static void
mux_read_fifo(struct async_struct *info)
{
	int data;
	struct tty_struct *tty = info->tty;

	while(1) {
		data = __raw_readl((unsigned long)info->iomem_base 
				   + IO_DATA_REG_OFFSET);

		if (MUX_STATUS(data))
			continue;

		if (MUX_EOFIFO(data))
			break;

		if (tty->flip.count >= TTY_FLIPBUF_SIZE)
			continue;

		*tty->flip.char_buf_ptr = data & 0xffu;
		*tty->flip.flag_buf_ptr = 0;

#ifdef CONFIG_MAGIC_SYSRQ
		if (MUX_BREAK(data) && !break_pressed) {
 			break_pressed = jiffies;
			continue;
		}

		if(MUX_BREAK(data)) {
			*tty->flip.flag_buf_ptr = TTY_BREAK;
		}

		if(break_pressed) {
			if(time_before(jiffies, break_pressed + HZ * 5)) {
				handle_sysrq(data & 0xffu, NULL, NULL, NULL);
				break_pressed = 0;
				continue; 
			}
			break_pressed = 0;
		}
#endif

		tty->flip.flag_buf_ptr++;
		tty->flip.char_buf_ptr++;
		tty->flip.count++;
	}

	tty_flip_buffer_push(tty);
}


/**
 * mux_drv_poll - Mux poll function.
 * @unused: Unused variable
 *
 * This function periodically polls the Mux to check for new data.
 */
static void
mux_drv_poll(unsigned long unused)
{
	struct async_struct *info = mux_drv_info;

	if(info && info->tty && mux_drv_refcount) {
		mux_read_fifo(info);
		info->last_active = jiffies;
	}

	mod_timer(&mux_drv_timer, jiffies + MUX_POLL_DELAY);
}

/**
 * mux_chars_in_buffer - Returns the number of chars present in the outbound fifo.
 * @tty: Ptr to the tty structure.
 *
 * This function returns the number of chars sitting in the outbound fifo.
 * [Note: This function is required for the normal_poll function in 
 *  drivers/char/n_tty.c].
 */
static int
mux_chars_in_buffer(struct tty_struct *tty)
{
	struct async_struct *info = (struct async_struct *)tty->driver_data;
	return __raw_readl((unsigned long)info->iomem_base 
			   + IO_DCOUNT_REG_OFFSET);
}

/**
 * mux_flush_buffer - Pause until the fifo is empty.
 * @tty: Ptr to the tty structure.
 *
 * Since the mux fifo is self draining, this function just
 * waits until the fifo has completely drained.
 */
static void
mux_flush_buffer(struct tty_struct *tty)
{
	while(mux_chars_in_buffer(tty))
		mdelay(MUX_FIFO_DRAIN_DELAY);
}

/**
 * mux_write_room - How much room is left in the fifo.
 * @tty: Ptr to the tty structure.
 *
 * This function returns how much room is in the fifo for
 * writing.
 */
static int
mux_write_room(struct tty_struct *tty)
{
	int room = mux_chars_in_buffer(tty);
	if(room > MUX_FIFO_SIZE)
		return 0;

	return MUX_FIFO_SIZE - room;
}

/**
 * mux_write - Write chars to the mux fifo.
 * @tty: Ptr to the tty structure.
 * @from_user: Is the buffer from user space?
 * @buf: The buffer to write to the mux fifo.
 * @count: The number of chars to write to the mux fifo.
 *
 * This function writes the data from buf to the mux fifo.
 * [Note: we need the mux_flush_buffer() at the end of the 
 * function, otherwise the system will wait for LONG_MAX
 * if the fifo is not empty when the TCSETSW ioctl is called.]
 */
static int
mux_write(struct tty_struct *tty, int from_user,
	  const unsigned char *buf, int count)
{
	int size, len, ret = count;
	char buffer[MUX_FIFO_SIZE], *buf_p;
	unsigned long iomem_base = 
		(unsigned long)((struct async_struct *)tty->driver_data)->iomem_base;

	while (count) {
		size = mux_write_room(tty);
		len = (size < count) ? size : count;

		if (from_user) {
			copy_from_user(buffer, buf, len);
			buf_p = buffer;
		} else {
			buf_p = (char *)buf;
		}

		count -= len;
		buf += len;

		if(size < MUX_MIN_FREE_SIZE)
			mux_flush_buffer(tty);

		while(len--) {
			__raw_writel(*buf_p++, iomem_base + IO_DATA_REG_OFFSET);
		}
	}

	mux_flush_buffer(tty);
	return ret;
}

/**
 * mux_break - Turn break handling on or off.
 * @tty: Ptr to the tty structure.
 * @break_state: break value.
 *
 * This function must be defined because the send_break() in
 * drivers/char/tty_io.c requires it.  Currently the Serial Mux
 * does nothing when this function is called.
 */
static void
mux_break(struct tty_struct *tty, int break_state)
{
}

/**
 * get_serial_info - Return the serial structure to userspace.
 * @info: Ptr to the async structure.
 * @retinfo: Ptr to the users space buffer.
 *
 * Fill in this serial structure and return it to userspace.
 */
static int
get_serial_info(struct async_struct *info,
		struct serial_struct *retinfo)
{
	struct serial_struct tmp;

	if (!retinfo)
		return -EFAULT;

	memset(&tmp, 0, sizeof(tmp));
	tmp.line = info->line;
	tmp.port = info->line;
	tmp.flags = info->flags;
	tmp.close_delay = info->close_delay;
	return copy_to_user(retinfo, &tmp, sizeof(*retinfo)) ? -EFAULT : 0;
}

/**
 * get_modem_info - Return the modem control and status signals to userspace.
 * @info: Ptr to the async structure.
 * @value: The return buffer.
 *
 * The Serial MUX driver always returns these values to userspace:
 *      Data Terminal Ready, Carrier Detect, Clear To Send,
 *      Request To Send.
 *
 */
static int 
get_modem_info(struct async_struct *info, unsigned int *value)
{
	unsigned int result = TIOCM_DTR|TIOCM_CAR|TIOCM_CTS|TIOCM_RTS;
	return copy_to_user(value, &result, sizeof(int)) ? -EFAULT : 0;
}

/**
 * get_lsr_info - Return line status register info to userspace.
 * @info: Ptr to the async structure.
 * @value: The return buffer.
 *
 * The Serial MUX driver always returns empty transmitter to userspace.
 */
static int 
get_lsr_info(struct async_struct *info, unsigned int *value)
{
	unsigned int result = TIOCSER_TEMT;
	return copy_to_user(value, &result, sizeof(int)) ? -EFAULT : 0;
}

/**
 * mux_ioctl - Handle driver specific ioctl commands.
 * @tty: Ptr to the tty structure.
 * @file: Unused.
 * @cmd: The ioctl number.
 * @arg: The ioctl argument.
 *
 * This function handles ioctls specific to the Serial MUX driver,
 * or ioctls that need driver specific information.
 *
 */
static int
mux_ioctl(struct tty_struct *tty, struct file *file,
	  unsigned int cmd, unsigned long arg)
{
	struct async_struct *info = (struct async_struct *) tty->driver_data;

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
		return 0;

	case TIOCGSERIAL:
		return get_serial_info(info, (struct serial_struct *) arg);

	case TIOCSSERIAL:
		return 0;

	case TIOCSERCONFIG:
		return 0;

	case TIOCSERGETLSR:
		return get_lsr_info(info, (unsigned int *) arg);

	case TIOCSERGSTRUCT:
		if (copy_to_user((struct async_struct *) arg,
				 info, sizeof (struct async_struct)))
			return -EFAULT;
		return 0;

	case TIOCMIWAIT:
		return 0;

	case TIOCGICOUNT:
		return 0;

	case TIOCSERGWILD:
	case TIOCSERSWILD:
		/* "setserial -W" is called in Debian boot */
		printk("TIOCSER?WILD ioctl obsolete, ignored.\n");
		return 0;

	default:
		return -ENOIOCTLCMD;
	}
	return 0;
}

/**
 * mux_close - Close the serial mux driver.
 * @tty: Ptr to the tty structure.
 * @filp: Unused.
 * 
 * This routine is called when the serial port gets closed.  First, we
 * wait for the last remaining data to be sent.  Then, we unlink its
 * async structure from the interrupt chain if necessary, and we free
 * that IRQ if nothing is left in the chain.
 */
static void
mux_close(struct tty_struct *tty, struct file *filp)
{
	struct async_struct *info = (struct async_struct *) tty->driver_data;

	mux_drv_refcount--;
	if (mux_drv_refcount > 0)
		return;

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
	 * At this point we stop accepting input.  To do this, we
	 * disable the receive line status interrupts, and tell the
	 * interrupt driver to stop checking the data ready bit in the
	 * line status register.
	 */

	/* XXX CP: make mask for receive !!! */

	if (tty->driver.flush_buffer)
		tty->driver.flush_buffer(tty);
	if (tty->ldisc.flush_buffer)
		tty->ldisc.flush_buffer(tty);
	tty->closing = 0;
	info->event = 0;
	info->tty = 0;
	mux_drv_info = NULL;
	if (info->blocked_open) {
		if (info->close_delay) {
			set_current_state(TASK_INTERRUPTIBLE);
			schedule_timeout(info->close_delay);
		}
		wake_up_interruptible(&info->open_wait);
	}
	info->flags &= ~(ASYNC_NORMAL_ACTIVE | ASYNC_CALLOUT_ACTIVE |
			 ASYNC_CLOSING);
	wake_up_interruptible(&info->close_wait);
	MOD_DEC_USE_COUNT;
}

/**
 * get_async_struct - Get the async structure.
 * @line: Minor number of the tty device.
 * @ret_info: Ptr to the newly allocated async structure.
 *
 * Allocate and return an async structure for the specified
 * tty device line.
 */
static int
get_async_struct(int line, struct async_struct **ret_info)
{
	struct async_struct *info;

	info = kmalloc(sizeof (struct async_struct), GFP_KERNEL);
	if (!info) {
		return -ENOMEM;
	}
	memset(info, 0, sizeof (struct async_struct));
	init_waitqueue_head(&info->open_wait);
	init_waitqueue_head(&info->close_wait);
	init_waitqueue_head(&info->delta_msr_wait);
	info->magic = SERIAL_MAGIC;
	info->port = 0;
	info->flags = 0;
	info->io_type = 0;
	info->iomem_base = (void *)(hpa + MUX_OFFSET);
	info->iomem_reg_shift = 0;
	info->xmit_fifo_size = MUX_FIFO_SIZE;
	info->line = line;
	info->tqueue.routine = NULL;
	info->tqueue.data = info;
	info->state = NULL;
	*ret_info = info;
	return 0;
}

/**
 * mux_open - Open the serial mux driver.
 * @tty: Ptr to the tty structure.
 * @filp: Unused.
 *
 * This routine is called whenever a serial port is opened.  It
 * enables interrupts for a serial port, linking in its async structure 
 * into the IRQ chain.   It also performs the serial-specific
 * initialization for the tty structure.
 */
static int
mux_open(struct tty_struct *tty, struct file *filp)
{
	struct async_struct *info;
	int retval, line;

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
	mux_drv_info = info;
	info->tty->low_latency = 0;
	info->session = current->session;
	info->pgrp = current->pgrp;
	mux_drv_refcount++;
	return 0;
}

/**
 * mux_probe - Determine if the Serial Mux should claim this device.
 * @dev: The parisc device.
 *
 * Deterimine if the Sserial Mux should claim this chip (return 0)
 * or not (return 1).
 */
static int __init 
mux_probe(struct parisc_device *dev)
{
	if(hpa) {
		printk(KERN_INFO "Serial MUX driver already registered, skipping additonal MUXes for now.\n");
		return 1;
	}

	init_timer(&mux_drv_timer);
	mux_drv_timer.function = mux_drv_poll;
	mod_timer(&mux_drv_timer, jiffies + MUX_POLL_DELAY);

	hpa = dev->hpa;
	printk(KERN_INFO "Serial MUX driver version %s at 0x%lx\n",
	       mux_drv_version, hpa);

	/* Initialize the tty_driver structure */

	memset(&mux_drv_driver, 0, sizeof (struct tty_driver));
	mux_drv_driver.magic = TTY_DRIVER_MAGIC;
	mux_drv_driver.driver_name = "Serial MUX driver";
#ifdef CONFIG_DEVFS_FS
	mux_drv_driver.name = "ttb/%d";
#else
	mux_drv_driver.name = "ttyB";
#endif
	mux_drv_driver.major = MUX_MAJOR;
	mux_drv_driver.minor_start = 0;
	mux_drv_driver.num = NR_PORTS;
	mux_drv_driver.type = TTY_DRIVER_TYPE_SERIAL;
	mux_drv_driver.subtype = SERIAL_TYPE_NORMAL;
	mux_drv_driver.init_termios = tty_std_termios;
	mux_drv_driver.init_termios.c_cflag =
	    B9600 | CS8 | CREAD | HUPCL | CLOCAL;
	mux_drv_driver.flags = TTY_DRIVER_REAL_RAW | TTY_DRIVER_NO_DEVFS;
	mux_drv_driver.refcount = &mux_drv_refcount;
	mux_drv_driver.table = mux_drv_table;
	mux_drv_driver.termios = mux_drv_termios;
	mux_drv_driver.termios_locked = mux_drv_termios_locked;

	mux_drv_driver.open = mux_open;
	mux_drv_driver.close = mux_close;
	mux_drv_driver.write = mux_write;
	mux_drv_driver.put_char = NULL;
	mux_drv_driver.flush_chars = NULL;
	mux_drv_driver.write_room = mux_write_room;
	mux_drv_driver.chars_in_buffer = mux_chars_in_buffer;
	mux_drv_driver.flush_buffer = mux_flush_buffer;
	mux_drv_driver.ioctl = mux_ioctl;
	mux_drv_driver.throttle = NULL;
	mux_drv_driver.unthrottle = NULL;
	mux_drv_driver.set_termios = NULL;
	mux_drv_driver.stop = NULL;
	mux_drv_driver.start = NULL;
	mux_drv_driver.hangup = NULL;
	mux_drv_driver.break_ctl = mux_break;
	mux_drv_driver.send_xchar = NULL;
	mux_drv_driver.wait_until_sent = NULL;
	mux_drv_driver.read_proc = NULL;

	if (tty_register_driver(&mux_drv_driver))
		panic("Could not register the serial MUX driver\n");

	return 0;
}

static struct parisc_device_id mux_tbl[] = {
	{ HPHW_A_DIRECT, HVERSION_REV_ANY_ID, HVERSION_ANY_ID, 0x0000D },
	{ 0, }
};

MODULE_DEVICE_TABLE(parisc, mux_tbl);

static struct parisc_driver mux_driver = {
	name:		"Serial MUX driver",
	id_table:	mux_tbl,
	probe:		mux_probe,
};

/**
 * mux_init - Serial MUX initalization procedure.
 *
 * Register the Serial MUX driver.
 */
static int __init mux_init(void) 
{
	return register_parisc_driver(&mux_driver);
}

/**
 * mux_exit - Serial MUX cleanup procedure.
 *
 * Unregister the Serial MUX driver from the tty layer.
 */
static void __exit mux_exit(void)
{
	int status = tty_unregister_driver(&mux_drv_driver);
	if(status) {
		printk("MUX: failed to unregister the Serial MUX driver (%d)\n", status);
	}
}

module_init(mux_init);
module_exit(mux_exit);
MODULE_DESCRIPTION("Serial MUX driver");
MODULE_AUTHOR("Ryan Bradetich");
MODULE_LICENSE("GPL");
