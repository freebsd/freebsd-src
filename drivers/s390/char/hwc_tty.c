/*
 *  drivers/s390/char/hwc_tty.c
 *    HWC line mode terminal driver.
 *
 *  S390 version
 *    Copyright (C) 1999 IBM Deutschland Entwicklung GmbH, IBM Corporation
 *    Author(s): Martin Peschke <mpeschke@de.ibm.com>
 *
 *  Thanks to Martin Schwidefsky.
 */

#include <linux/config.h>
#include <linux/major.h>
#include <linux/termios.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/devfs_fs_kernel.h>
#include <linux/init.h>

#include <asm/uaccess.h>

#include "hwc_rw.h"
#include "ctrlchar.h"

#define HWC_TTY_PRINT_HEADER "hwc tty driver: "

#define HWC_TTY_BUF_SIZE 512

typedef struct {

	struct tty_struct *tty;

	unsigned char buf[HWC_TTY_BUF_SIZE];

	unsigned short int buf_count;

	spinlock_t lock;

	hwc_high_level_calls_t calls;
} hwc_tty_data_struct;

static hwc_tty_data_struct hwc_tty_data =
{ /* NULL/0 */ };
static struct tty_driver hwc_tty_driver;
static struct tty_struct *hwc_tty_table[1];
static struct termios *hwc_tty_termios[1];
static struct termios *hwc_tty_termios_locked[1];
static int hwc_tty_refcount = 0;

extern struct termios tty_std_termios;

void hwc_tty_wake_up (void);
void hwc_tty_input (unsigned char *, unsigned int);

static int 
hwc_tty_open (struct tty_struct *tty,
	      struct file *filp)
{

	if (MINOR (tty->device) - tty->driver.minor_start)
		return -ENODEV;

	tty->driver_data = &hwc_tty_data;
	hwc_tty_data.buf_count = 0;
	hwc_tty_data.tty = tty;
	tty->low_latency = 0;

	hwc_tty_data.calls.wake_up = hwc_tty_wake_up;
	hwc_tty_data.calls.move_input = hwc_tty_input;
	hwc_register_calls (&(hwc_tty_data.calls));

	return 0;
}

static void 
hwc_tty_close (struct tty_struct *tty,
	       struct file *filp)
{
	if (MINOR (tty->device) != tty->driver.minor_start) {
		printk (KERN_WARNING HWC_TTY_PRINT_HEADER
			"do not close hwc tty because of wrong device number");
		return;
	}
	if (tty->count > 1)
		return;

	hwc_tty_data.tty = NULL;

	hwc_unregister_calls (&(hwc_tty_data.calls));
}

static int 
hwc_tty_write_room (struct tty_struct *tty)
{
	int retval;

	retval = hwc_write_room (IN_BUFS_TOTAL);
	return retval;
}

static int 
hwc_tty_write (struct tty_struct *tty,
	       int from_user,
	       const unsigned char *buf,
	       int count)
{
	int retval;

	if (hwc_tty_data.buf_count > 0) {
		hwc_write (0, hwc_tty_data.buf, hwc_tty_data.buf_count);
		hwc_tty_data.buf_count = 0;
	}
	retval = hwc_write (from_user, buf, count);
	return retval;
}

static void 
hwc_tty_put_char (struct tty_struct *tty,
		  unsigned char ch)
{
	unsigned long flags;

	spin_lock_irqsave (&hwc_tty_data.lock, flags);
	if (hwc_tty_data.buf_count >= HWC_TTY_BUF_SIZE) {
		hwc_write (0, hwc_tty_data.buf, hwc_tty_data.buf_count);
		hwc_tty_data.buf_count = 0;
	}
	hwc_tty_data.buf[hwc_tty_data.buf_count] = ch;
	hwc_tty_data.buf_count++;
	spin_unlock_irqrestore (&hwc_tty_data.lock, flags);
}

static void 
hwc_tty_flush_chars (struct tty_struct *tty)
{
	unsigned long flags;

	spin_lock_irqsave (&hwc_tty_data.lock, flags);
	hwc_write (0, hwc_tty_data.buf, hwc_tty_data.buf_count);
	hwc_tty_data.buf_count = 0;
	spin_unlock_irqrestore (&hwc_tty_data.lock, flags);
}

static int 
hwc_tty_chars_in_buffer (struct tty_struct *tty)
{
	int retval;

	retval = hwc_chars_in_buffer (IN_BUFS_TOTAL);
	return retval;
}

static void 
hwc_tty_flush_buffer (struct tty_struct *tty)
{
	hwc_tty_wake_up ();
}

static int 
hwc_tty_ioctl (
		      struct tty_struct *tty,
		      struct file *file,
		      unsigned int cmd,
		      unsigned long arg)
{
	if (tty->flags & (1 << TTY_IO_ERROR))
		return -EIO;

	return hwc_ioctl (cmd, arg);
}

void 
hwc_tty_wake_up (void)
{
	if (hwc_tty_data.tty == NULL)
		return;
	if ((hwc_tty_data.tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) &&
	    hwc_tty_data.tty->ldisc.write_wakeup)
		(hwc_tty_data.tty->ldisc.write_wakeup) (hwc_tty_data.tty);
	wake_up_interruptible (&hwc_tty_data.tty->write_wait);
}

void 
hwc_tty_input (unsigned char *buf, unsigned int count)
{
	struct tty_struct *tty = hwc_tty_data.tty;

	if (tty != NULL) {
		char *cchar;
		if ((cchar = ctrlchar_handle (buf, count, tty))) {
			if (cchar == (char *) -1)
				return;
			tty->flip.count++;
			*tty->flip.flag_buf_ptr++ = TTY_NORMAL;
			*tty->flip.char_buf_ptr++ = *cchar;
		} else {

			memcpy (tty->flip.char_buf_ptr, buf, count);
			if (count < 2 || (
					 strncmp (buf + count - 2, "^n", 2) ||
				    strncmp (buf + count - 2, "\0252n", 2))) {
				tty->flip.char_buf_ptr[count] = '\n';
				count++;
			} else
				count -= 2;
			memset (tty->flip.flag_buf_ptr, TTY_NORMAL, count);
			tty->flip.char_buf_ptr += count;
			tty->flip.flag_buf_ptr += count;
			tty->flip.count += count;
		}
		tty_flip_buffer_push (tty);
		hwc_tty_wake_up ();
	}
}

void 
hwc_tty_init (void)
{
	if (!CONSOLE_IS_HWC)
		return;

	ctrlchar_init ();

	memset (&hwc_tty_driver, 0, sizeof (struct tty_driver));
	memset (&hwc_tty_data, 0, sizeof (hwc_tty_data_struct));
	hwc_tty_driver.magic = TTY_DRIVER_MAGIC;
	hwc_tty_driver.driver_name = "tty_hwc";
	hwc_tty_driver.name = "ttyS";
	hwc_tty_driver.name_base = 0;
	hwc_tty_driver.major = TTY_MAJOR;
	hwc_tty_driver.minor_start = 64;
	hwc_tty_driver.num = 1;
	hwc_tty_driver.type = TTY_DRIVER_TYPE_SYSTEM;
	hwc_tty_driver.subtype = SYSTEM_TYPE_TTY;
	hwc_tty_driver.init_termios = tty_std_termios;
	hwc_tty_driver.init_termios.c_iflag = IGNBRK | IGNPAR;
	hwc_tty_driver.init_termios.c_oflag = ONLCR;
	hwc_tty_driver.init_termios.c_lflag = ISIG | ECHO;
	hwc_tty_driver.flags = TTY_DRIVER_REAL_RAW;
	hwc_tty_driver.refcount = &hwc_tty_refcount;

	hwc_tty_driver.table = hwc_tty_table;
	hwc_tty_driver.termios = hwc_tty_termios;
	hwc_tty_driver.termios_locked = hwc_tty_termios_locked;

	hwc_tty_driver.open = hwc_tty_open;
	hwc_tty_driver.close = hwc_tty_close;
	hwc_tty_driver.write = hwc_tty_write;
	hwc_tty_driver.put_char = hwc_tty_put_char;
	hwc_tty_driver.flush_chars = hwc_tty_flush_chars;
	hwc_tty_driver.write_room = hwc_tty_write_room;
	hwc_tty_driver.chars_in_buffer = hwc_tty_chars_in_buffer;
	hwc_tty_driver.flush_buffer = hwc_tty_flush_buffer;
	hwc_tty_driver.ioctl = hwc_tty_ioctl;

	hwc_tty_driver.throttle = NULL;
	hwc_tty_driver.unthrottle = NULL;
	hwc_tty_driver.send_xchar = NULL;
	hwc_tty_driver.set_termios = NULL;
	hwc_tty_driver.set_ldisc = NULL;
	hwc_tty_driver.stop = NULL;
	hwc_tty_driver.start = NULL;
	hwc_tty_driver.hangup = NULL;
	hwc_tty_driver.break_ctl = NULL;
	hwc_tty_driver.wait_until_sent = NULL;
	hwc_tty_driver.read_proc = NULL;
	hwc_tty_driver.write_proc = NULL;

	if (tty_register_driver (&hwc_tty_driver))
		panic ("Couldn't register hwc_tty driver\n");
}
