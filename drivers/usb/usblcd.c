/***************************************************************************** 
 *                          USBLCD Kernel Driver                             *
 *        See http://www.usblcd.de for Hardware and Documentation.           *
 *                            Version 1.03                                   *
 *             (C) 2002 Adams IT Services <info@usblcd.de>                   *
 *                                                                           *
 *     This file is licensed under the GPL. See COPYING in the package.      *
 * Based on rio500.c by Cesar Miquel (miquel@df.uba.ar) which is based on    *
 * hp_scanner.c by David E. Nelson (dnelson@jump.net)                        *
 *                                                                           *
 * 23.7.02 RA changed minor device number to the official assigned one       *
 * 18.9.02 RA Vendor ID change, longer timeouts                              *
 *****************************************************************************/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <asm/uaccess.h>
#include <linux/usb.h>

#define DRIVER_VERSION "USBLCD Driver Version 1.03"

#define USBLCD_MINOR		144

#define IOCTL_GET_HARD_VERSION	1
#define IOCTL_GET_DRV_VERSION	2

/* stall/wait timeout for USBLCD */
#define NAK_TIMEOUT	(10*HZ)

#define IBUF_SIZE	0x1000
#define OBUF_SIZE	0x10000

struct lcd_usb_data {
	struct usb_device *lcd_dev;	/* init: probe_lcd */
	unsigned int ifnum;		/* Interface number of the USB device */
	int isopen;			/* nz if open */
	int present;			/* Device is present on the bus */
	char *obuf, *ibuf;		/* transfer buffers */
	char bulk_in_ep, bulk_out_ep;	/* Endpoint assignments */
	wait_queue_head_t wait_q;	/* for timeouts */
};

static struct lcd_usb_data lcd_instance;

static int open_lcd(struct inode *inode, struct file *file)
{
	struct lcd_usb_data *lcd = &lcd_instance;

	if (lcd->isopen || !lcd->present) {
		return -EBUSY;
	}
	lcd->isopen = 1;

	init_waitqueue_head(&lcd->wait_q);

	info("USBLCD opened.");

	return 0;
}

static int close_lcd(struct inode *inode, struct file *file)
{
	struct lcd_usb_data *lcd = &lcd_instance;

	lcd->isopen = 0;

	info("USBLCD closed.");
	return 0;
}

static int
ioctl_lcd(struct inode *inode, struct file *file, unsigned int cmd,
	  unsigned long arg)
{
	struct lcd_usb_data *lcd = &lcd_instance;
	int i;
	char buf[30];

	/* Sanity check to make sure lcd is connected, powered, etc */
	if (lcd == NULL ||
	    lcd->present == 0 ||
	    lcd->lcd_dev == NULL)
		return -1;

	switch (cmd) {
	case IOCTL_GET_HARD_VERSION:
		i = (lcd->lcd_dev)->descriptor.bcdDevice;
		sprintf(buf,"%1d%1d.%1d%1d",(i & 0xF000)>>12,(i & 0xF00)>>8,
			(i & 0xF0)>>4,(i & 0xF));
		if (copy_to_user((void *)arg,buf,strlen(buf))!=0)
			return -EFAULT;
		break;
	case IOCTL_GET_DRV_VERSION:
		sprintf(buf,DRIVER_VERSION);
		if (copy_to_user((void *)arg,buf,strlen(buf))!=0)
			return -EFAULT;
		break;
	default:
		return -ENOIOCTLCMD;
		break;
	}

	return 0;
}

static ssize_t
write_lcd(struct file *file, const char *buffer,
	  size_t count, loff_t * ppos)
{
	struct lcd_usb_data *lcd = &lcd_instance;

	unsigned long copy_size;
	unsigned long bytes_written = 0;
	unsigned int partial;

	int result = 0;
	int maxretry;

	/* Sanity check to make sure lcd is connected, powered, etc */
	if (lcd == NULL ||
	    lcd->present == 0 ||
	    lcd->lcd_dev == NULL)
		return -1;

	do {
		unsigned long thistime;
		char *obuf = lcd->obuf;

		thistime = copy_size =
		    (count >= OBUF_SIZE) ? OBUF_SIZE : count;
		if (copy_from_user(lcd->obuf, buffer, copy_size))
			return -EFAULT;
		maxretry = 5;
		while (thistime) {
			if (!lcd->lcd_dev)
				return -ENODEV;
			if (signal_pending(current)) {
				return bytes_written ? bytes_written : -EINTR;
			}

			result = usb_bulk_msg(lcd->lcd_dev,
					 usb_sndbulkpipe(lcd->lcd_dev, 1),
					 obuf, thistime, &partial, 10 * HZ);

			dbg("write stats: result:%d thistime:%lu partial:%u",
			     result, thistime, partial);

			if (result == USB_ST_TIMEOUT) {	/* NAK - so hold for a while */
				if (!maxretry--) {
					return -ETIME;
				}
				interruptible_sleep_on_timeout(&lcd-> wait_q, NAK_TIMEOUT);
				continue;
			} else if (!result & partial) {
				obuf += partial;
				thistime -= partial;
			} else
				break;
		};
		if (result) {
			err("Write Whoops - %x", result);
			return -EIO;
		}
		bytes_written += copy_size;
		count -= copy_size;
		buffer += copy_size;
	} while (count > 0);

	return bytes_written ? bytes_written : -EIO;
}

static ssize_t
read_lcd(struct file *file, char *buffer, size_t count, loff_t * ppos)
{
	struct lcd_usb_data *lcd = &lcd_instance;
	ssize_t read_count;
	unsigned int partial;
	int this_read;
	int result;
	int maxretry = 10;
	char *ibuf = lcd->ibuf;

	/* Sanity check to make sure lcd is connected, powered, etc */
	if (lcd == NULL ||
	    lcd->present == 0 ||
	    lcd->lcd_dev == NULL)
		return -1;

	read_count = 0;

	while (count > 0) {
		if (signal_pending(current)) {
			return read_count ? read_count : -EINTR;
		}
		if (!lcd->lcd_dev)
			return -ENODEV;
		this_read = (count >= IBUF_SIZE) ? IBUF_SIZE : count;

		result = usb_bulk_msg(lcd->lcd_dev,
				      usb_rcvbulkpipe(lcd->lcd_dev, 0),
				      ibuf, this_read, &partial,
				      (int) (HZ * 8));

		dbg(KERN_DEBUG "read stats: result:%d this_read:%u partial:%u",
		       result, this_read, partial);

		if (partial) {
			count = this_read = partial;
		} else if (result == USB_ST_TIMEOUT || result == 15) {	/* FIXME: 15 ??? */
			if (!maxretry--) {
				err("read_lcd: maxretry timeout");
				return -ETIME;
			}
			interruptible_sleep_on_timeout(&lcd->wait_q,
						       NAK_TIMEOUT);
			continue;
		} else if (result != USB_ST_DATAUNDERRUN) {
			err("Read Whoops - result:%u partial:%u this_read:%u",
			     result, partial, this_read);
			return -EIO;
		} else {
			return (0);
		}

		if (this_read) {
			if (copy_to_user(buffer, ibuf, this_read))
				return -EFAULT;
			count -= this_read;
			read_count += this_read;
			buffer += this_read;
		}
	}
	return read_count;
}

static void *probe_lcd(struct usb_device *dev, unsigned int ifnum)
{
	struct lcd_usb_data *lcd = &lcd_instance;
	int i;
	
	if (dev->descriptor.idProduct != 0x0001  ) {
		warn(KERN_INFO "USBLCD model not supported.");
		return NULL;
	}

	if (lcd->present == 1) {
		warn(KERN_INFO "Multiple USBLCDs are not supported!");
		return NULL;
	}

	i = dev->descriptor.bcdDevice;

	info("USBLCD Version %1d%1d.%1d%1d found at address %d",
		(i & 0xF000)>>12,(i & 0xF00)>>8,(i & 0xF0)>>4,(i & 0xF),
		dev->devnum);

	lcd->present = 1;
	lcd->lcd_dev = dev;

	if (!(lcd->obuf = (char *) kmalloc(OBUF_SIZE, GFP_KERNEL))) {
		err("probe_lcd: Not enough memory for the output buffer");
		return NULL;
	}
	dbg("probe_lcd: obuf address:%p", lcd->obuf);

	if (!(lcd->ibuf = (char *) kmalloc(IBUF_SIZE, GFP_KERNEL))) {
		err("probe_lcd: Not enough memory for the input buffer");
		kfree(lcd->obuf);
		return NULL;
	}
	dbg("probe_lcd: ibuf address:%p", lcd->ibuf);

	return lcd;
}

static void disconnect_lcd(struct usb_device *dev, void *ptr)
{
	struct lcd_usb_data *lcd = (struct lcd_usb_data *) ptr;

	if (lcd->isopen) {
		lcd->isopen = 0;
		/* better let it finish - the release will do whats needed */
		lcd->lcd_dev = NULL;
		return;
	}
	kfree(lcd->ibuf);
	kfree(lcd->obuf);

	info("USBLCD disconnected.");

	lcd->present = 0;
}

static struct usb_device_id id_table [] = {
	{ .idVendor = 0x10D2, .match_flags = USB_DEVICE_ID_MATCH_VENDOR, },
	{},
};

MODULE_DEVICE_TABLE (usb, id_table);

static struct
file_operations usb_lcd_fops = {
	.owner =	THIS_MODULE,
	.read =		read_lcd,
	.write =	write_lcd,
	.ioctl =	ioctl_lcd,
	.open =		open_lcd,
	.release =	close_lcd,
};

static struct
usb_driver lcd_driver = {
	.name =		"usblcd",
	.probe =	(void *)probe_lcd,
	.disconnect =	disconnect_lcd,
	.id_table =	id_table,
	.fops =		&usb_lcd_fops,
	.minor =	USBLCD_MINOR,
};

int usb_lcd_init(void)
{
	if (usb_register(&lcd_driver) < 0)
		return -1;

	info("%s (C) Adams IT Services http://www.usblcd.de", DRIVER_VERSION);
	info("USBLCD support registered.");
	return 0;
}


void usb_lcd_cleanup(void)
{
	struct lcd_usb_data *lcd = &lcd_instance;

	lcd->present = 0;
	usb_deregister(&lcd_driver);
}

module_init(usb_lcd_init);
module_exit(usb_lcd_cleanup);

MODULE_AUTHOR("Adams IT Services <info@usblcd.de>");
MODULE_DESCRIPTION(DRIVER_VERSION);
MODULE_LICENSE("GPL");
