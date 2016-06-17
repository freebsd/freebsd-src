/*
 *      Tieman Voyager braille display USB driver.
 *
 *      Copyright 2001-2002 Stephane Dalton <sdalton@videotron.ca>
 *                      and Stéphane Doyon  <s.doyon@videotron.ca>
 *            Maintained by Stéphane Doyon  <s.doyon@videotron.ca>.
 */
/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
/* History:
 * 0.8 April 2002: Integration into the kernel tree.
 * 0.7 October 2001: First public release as a module, distributed with
 *     the BRLTTY package (beta versions around 2.99y).
 */

#define DRIVER_VERSION "v0.8"
#define DATE "April 2002"
#define DRIVER_AUTHOR \
	"Stephane Dalton <sdalton@videotron.ca> " \
	"and Stéphane Doyon <s.doyon@videotron.ca>"
#define DRIVER_DESC "Tieman Voyager braille display USB driver for Linux 2.4"
#define DRIVER_SHORTDESC "Voyager"

#define BANNER \
	KERN_INFO DRIVER_SHORTDESC " " DRIVER_VERSION " (" DATE ")\n" \
	KERN_INFO "   by " DRIVER_AUTHOR "\n"

static const char longbanner[] = {
	DRIVER_DESC ", " DRIVER_VERSION " (" DATE "), by " DRIVER_AUTHOR
};

#include <linux/module.h>
#include <linux/usb.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <asm/uaccess.h>
#include <asm/atomic.h>
#include <linux/poll.h>
#include <linux/devfs_fs_kernel.h>
#include <linux/brlvger.h>

MODULE_AUTHOR( DRIVER_AUTHOR );
MODULE_DESCRIPTION( DRIVER_DESC );
MODULE_LICENSE("GPL");

/* Module parameters */

static int debug = 1;
MODULE_PARM(debug, "i");
MODULE_PARM_DESC(debug, "Debug level, 0-3");

static int write_repeats = 2;
MODULE_PARM(write_repeats, "i");
MODULE_PARM_DESC(write_repeats, "Hack: repetitions for command to "
		 "display braille pattern");
		 /* to get rid of weird extra dots (perhaps only on
		    early hardware versions?) */

static int stall_tries = 3;
MODULE_PARM(stall_tries, "i");
MODULE_PARM_DESC(stall_tries, "Hack: retransmits of stalled USB "
		 "control messages");
                 /* broken early hardware versions? */

#define BRLVGER_RAW_VOLTAGE 89
/* from 0->300V to 255->200V, we are told 265V is normal operating voltage,
   but we don't know the scale. Assuming it is linear. */
static int raw_voltage = BRLVGER_RAW_VOLTAGE;
MODULE_PARM(raw_voltage, "i");
MODULE_PARM_DESC(raw_voltage, "Parameter for the call to SET_DISPLAY_VOLTAGE");


/* protocol and display type defines */
#define MAX_BRLVGER_CELLS 72
#define MAX_INTERRUPT_DATA 8
/* control message request types */
#define BRLVGER_READ_REQ 0xC2
#define BRLVGER_WRITE_REQ 0x42
/* control message request codes */
#define BRLVGER_SET_DISPLAY_ON 0
#define BRLVGER_SET_DISPLAY_VOLTAGE 1
#define BRLVGER_GET_SERIAL 3
#define BRLVGER_GET_HWVERSION 4
#define BRLVGER_GET_FWVERSION 5
#define BRLVGER_GET_LENGTH 6
#define BRLVGER_SEND_BRAILLE 7
#define BRLVGER_BEEP 9
#if 0 /* not used and not sure they're working */
#define BRLVGER_GET_DISPLAY_VOLTAGE 2
#define BRLVGER_GET_CURRENT 8
#endif

/* Prototypes */
static void *brlvger_probe (struct usb_device *dev, unsigned ifnum,
			    const struct usb_device_id *id);
static void brlvger_disconnect(struct usb_device *dev, void *ptr);
static int brlvger_open(struct inode *inode, struct file *file);
static int brlvger_release(struct inode *inode, struct file *file);
static ssize_t brlvger_write(struct file *file, const char *buffer,
			     size_t count, loff_t *pos);
static ssize_t brlvger_read(struct file *file, char *buffer,
			    size_t count, loff_t *unused_pos);
static int brlvger_ioctl(struct inode *inode, struct file *file,
			 unsigned cmd, unsigned long arg);
static unsigned brlvger_poll(struct file *file, poll_table *wait);
static loff_t brlvger_llseek(struct file * file, loff_t offset, int orig);
static void intr_callback(struct urb *urb);
struct brlvger_priv;
static int brlvger_get_hw_version(struct brlvger_priv *priv,
				  unsigned char *verbuf);
static int brlvger_get_fw_version(struct brlvger_priv *priv,
				  unsigned char *buf);
static int brlvger_get_serial(struct brlvger_priv *priv,
			      unsigned char *buf);
static int brlvger_get_display_length(struct brlvger_priv *priv);
static int brlvger_set_display_on_off(struct brlvger_priv *priv, __u16 on);
static int brlvger_beep(struct brlvger_priv *priv, __u16 duration);
static int brlvger_set_display_voltage(struct brlvger_priv *priv,
				       __u16 voltage);
static int mycontrolmsg(const char *funcname,
                        struct brlvger_priv *priv, unsigned pipe_dir,
                        __u8 request, __u8 requesttype, __u16 value,
                        __u16 index, void *data, __u16 size);

#define controlmsg(priv,pipe_dir,a,b,c,d,e,f) \
     mycontrolmsg(__FUNCTION__, priv, pipe_dir, \
                  a,b,c,d,e,f)
#define sndcontrolmsg(priv,a,b,c,d,e,f) \
    controlmsg(priv, 0, a,b,c,d,e,f)
#define rcvcontrolmsg(priv,a,b,c,d,e,f) \
    controlmsg(priv, USB_DIR_IN, a,b,c,d,e,f)

extern devfs_handle_t usb_devfs_handle; /* /dev/usb dir. */

/* ----------------------------------------------------------------------- */

/* Data */

/* key event queue size */
#define MAX_INTERRUPT_BUFFER 10

/* private state */
struct brlvger_priv {
	struct usb_device   *dev; /* USB device handle */
	struct usb_endpoint_descriptor *in_interrupt;
	struct urb *intr_urb;
	devfs_handle_t devfs;

	int subminor; /* which minor dev #? */

	unsigned char hwver[BRLVGER_HWVER_SIZE]; /* hardware version */
	unsigned char fwver[BRLVGER_FWVER_SIZE]; /* firmware version */
	unsigned char serialnum[BRLVGER_SERIAL_SIZE];

	int llength; /* logical length */
	int plength; /* physical length */

	__u8 obuf[MAX_BRLVGER_CELLS];
	__u8 intr_buff[MAX_INTERRUPT_DATA];
	__u8 event_queue[MAX_INTERRUPT_BUFFER][MAX_INTERRUPT_DATA];
	atomic_t intr_idx, read_idx;
	spinlock_t intr_idx_lock; /* protects intr_idx */
	wait_queue_head_t read_wait;

	int opened;
	struct semaphore open_sem; /* protects ->opened */
	struct semaphore dev_sem; /* protects ->dev */
};

/* Globals */

/* Table of connected devices, a different minor for each. */
static struct brlvger_priv *display_table[ MAX_NR_BRLVGER_DEVS ];

/* Mutex for the operation of removing a device from display_table */
static DECLARE_MUTEX(disconnect_sem);

/* For blocking open */
static DECLARE_WAIT_QUEUE_HEAD(open_wait);

/* Some print macros */
#ifdef dbg
#undef dbg
#endif
#ifdef info
#undef info
#endif
#ifdef err
#undef err
#endif
#define info(args...) \
    ({ printk(KERN_INFO "Voyager: " args); \
       printk("\n"); })
#define err(args...) \
    ({ printk(KERN_ERR "Voyager: " args); \
       printk("\n"); })
#define dbgprint(fmt, args...) \
    ({ printk(KERN_DEBUG "Voyager: %s: " fmt, __FUNCTION__ , ##args); \
       printk("\n"); })
#define dbg(args...) \
    ({ if(debug >= 1) dbgprint(args); })
#define dbg2(args...) \
    ({ if(debug >= 2) dbgprint(args); })
#define dbg3(args...) \
    ({ if(debug >= 3) dbgprint(args); })

/* ----------------------------------------------------------------------- */

/* Driver registration */

static struct usb_device_id brlvger_ids [] = {
	{ USB_DEVICE(0x0798, 0x0001) },
	{ }                     /* Terminating entry */
};
MODULE_DEVICE_TABLE (usb, brlvger_ids);

static struct file_operations brlvger_fops =
{
	owner:		THIS_MODULE,
	llseek:		brlvger_llseek,
	read:		brlvger_read,
	write:		brlvger_write,
	ioctl:		brlvger_ioctl,
	open:		brlvger_open,
	release:	brlvger_release,
	poll:		brlvger_poll,
};

static struct usb_driver brlvger_driver =
{
	name:		"brlvger",
	probe:		brlvger_probe,
	disconnect:	brlvger_disconnect,
	fops:		&brlvger_fops,
	minor:		BRLVGER_MINOR,
	id_table:	brlvger_ids,
};

static int
__init brlvger_init (void)
{
	printk(BANNER);

	if(stall_tries < 1 || write_repeats < 1)
	  return -EINVAL;

	memset(display_table, 0, sizeof(display_table));

	if (usb_register(&brlvger_driver)) {
		err("USB registration failed");
		return -ENOSYS;
	}

	return 0;
}

static void
__exit brlvger_cleanup (void)
{
	usb_deregister (&brlvger_driver);
	dbg("Driver unregistered");
}

module_init (brlvger_init);
module_exit (brlvger_cleanup);

/* ----------------------------------------------------------------------- */

/* Probe and disconnect functions */

static void *
brlvger_probe (struct usb_device *dev, unsigned ifnum,
	       const struct usb_device_id *id)
{
	struct brlvger_priv *priv = NULL;
	int i;
	struct usb_endpoint_descriptor *endpoint;
	struct usb_interface_descriptor *actifsettings;
	/* protects against reentrance: once we've found a free slot
	   we reserve it.*/
	static DECLARE_MUTEX(reserve_sem);
        char devfs_name[16];

	actifsettings = dev->actconfig->interface->altsetting;

	if( dev->descriptor.bNumConfigurations != 1
			|| dev->config->bNumInterfaces != 1 
			|| actifsettings->bNumEndpoints != 1 ) {
		err ("Bogus braille display config info");
		return NULL;
	}

	endpoint = actifsettings->endpoint;
	if (!(endpoint->bEndpointAddress & 0x80) ||
		((endpoint->bmAttributes & 3) != 0x03)) {
		err ("Bogus braille display config info, wrong endpoints");
		return NULL;
	}

	down(&reserve_sem);

	for( i = 0; i < MAX_NR_BRLVGER_DEVS; i++ )
		if( display_table[i] == NULL )
			break;

	if( i == MAX_NR_BRLVGER_DEVS ) {
		err( "This driver cannot handle more than %d "
				"braille displays", MAX_NR_BRLVGER_DEVS);
		goto error;
	}

	if( !(priv = kmalloc (sizeof *priv, GFP_KERNEL)) ){
		err("No more memory");
		goto error;
	}

	memset(priv, 0, sizeof(*priv));
	atomic_set(&priv->intr_idx, 0);
	atomic_set(&priv->read_idx, MAX_INTERRUPT_BUFFER-1);
	spin_lock_init(&priv->intr_idx_lock);
	init_waitqueue_head(&priv->read_wait);
	/* opened is memset'ed to 0 */
	init_MUTEX(&priv->open_sem);
	init_MUTEX(&priv->dev_sem);

	priv->subminor = i;

	/* we found a interrupt in endpoint */
	priv->in_interrupt = endpoint;

	priv->dev = dev;

	if(brlvger_get_hw_version(priv, priv->hwver) <0) {
		err("Unable to get hardware version");
		goto error;
	}
	dbg("Hw ver %d.%d", priv->hwver[0], priv->hwver[1]);
	if(brlvger_get_fw_version(priv, priv->fwver) <0) {
		err("Unable to get firmware version");
		goto error;
	}
	dbg("Fw ver: %s", priv->fwver);

	if(brlvger_get_serial(priv, priv->serialnum) <0) {
		err("Unable to get serial number");
		goto error;
	}
	dbg("Serial number: %s", priv->serialnum);

	if( (priv->llength = brlvger_get_display_length(priv)) <0 ){
		err("Unable to get display length");
		goto error;
	}
	switch(priv->llength) {
	case 48:
		priv->plength = 44;
		break;
	case 72:
		priv->plength = 70;
		break;
	default:
		err("Unsupported display length: %d", priv->llength);
		goto error;
	};
	dbg("Display length: %d", priv->plength);

	sprintf(devfs_name, "brlvger%d", priv->subminor);
	priv->devfs = devfs_register(usb_devfs_handle, devfs_name,
				     DEVFS_FL_DEFAULT, USB_MAJOR,
				     BRLVGER_MINOR+priv->subminor,
				     S_IFCHR |S_IRUSR|S_IWUSR |S_IRGRP|S_IWGRP,
				     &brlvger_fops, NULL);
	if (!priv->devfs) {
#ifdef CONFIG_DEVFS_FS
		err("devfs node registration failed");
#endif
	}

	display_table[i] = priv;

	info( "Braille display %d is device major %d minor %d",
				i, USB_MAJOR, BRLVGER_MINOR + i);

	/* Tell anyone waiting on a blocking open */
	wake_up_interruptible(&open_wait);

	goto out;

 error:
	if(priv) {
		kfree( priv );
		priv = NULL;
	}

 out:
	up(&reserve_sem);
	return priv;
}

static void
brlvger_disconnect(struct usb_device *dev, void *ptr)
{
	struct brlvger_priv *priv = (struct brlvger_priv *)ptr;
	int r;

	if(priv){
		info("Display %d disconnecting", priv->subminor);

		devfs_unregister(priv->devfs);
		
		down(&disconnect_sem);
		display_table[priv->subminor] = NULL;
		up(&disconnect_sem);

		down(&priv->open_sem);
		down(&priv->dev_sem);
		if(priv->opened) {
			/* Disable interrupts */
			if((r = usb_unlink_urb(priv->intr_urb)) <0)
				err("usb_unlink_urb returns %d", r);
			usb_free_urb(priv->intr_urb);
			/* mark device as dead and prevent control
			   messages to it */
			priv->dev = NULL;
			/* Tell anyone hung up on a read that it
			   won't be coming */
			wake_up_interruptible(&priv->read_wait);
			up(&priv->dev_sem);
			up(&priv->open_sem);
		}else
			/* no corresponding up()s */
			kfree(priv);
	}
}

/* ----------------------------------------------------------------------- */

/* fops implementation */

static int
brlvger_open(struct inode *inode, struct file *file)
{
	int devnum = MINOR (inode->i_rdev);
	struct brlvger_priv *priv;
	int n, ret;

	if (devnum < BRLVGER_MINOR
	    || devnum >= (BRLVGER_MINOR + MAX_NR_BRLVGER_DEVS))
		return -ENXIO;

	n = devnum - BRLVGER_MINOR;

	MOD_INC_USE_COUNT;

	do {
		down(&disconnect_sem);
		priv = display_table[n];
		
		if(!priv) {
			up(&disconnect_sem);
			if (file->f_flags & O_NONBLOCK) {
				dbg3("Failing non-blocking open: "
				     "device %d not connected", n);
				MOD_DEC_USE_COUNT;
				return -EAGAIN;
			}
			/* Blocking open. One global wait queue will
			   suffice. We wait until a device for the selected
			   minor is connected. */
			dbg2("Waiting for device %d to be connected", n);
			ret = wait_event_interruptible(open_wait,
						       display_table[n]
						       != NULL);
			if(ret) {
				dbg2("Interrupted wait for device %d", n);
				MOD_DEC_USE_COUNT;
				return ret;
			}
		}
	} while(!priv);
	/* We grabbed an existing device. */

	if(down_interruptible(&priv->open_sem))
		return -ERESTARTSYS;
	up(&disconnect_sem);

	/* Only one process can open each device, no sharing. */
	ret = -EBUSY;
	if(priv->opened)
		goto error;

	dbg("Opening display %d", priv->subminor);

	/* Setup interrupt handler for receiving key input */
	priv->intr_urb = usb_alloc_urb(0);
	if(!priv->intr_urb) {
		err("Unable to allocate URB");
		goto error;
	}
	FILL_INT_URB( priv->intr_urb, priv->dev,
			usb_rcvintpipe(priv->dev,
				       priv->in_interrupt->bEndpointAddress),
			priv->intr_buff, sizeof(priv->intr_buff),
			intr_callback, priv, priv->in_interrupt->bInterval);
	if((ret = usb_submit_urb(priv->intr_urb)) <0){
		err("Error %d while submitting URB", ret);
		goto error;
	}

	/* Set voltage */
	if(brlvger_set_display_voltage(priv, raw_voltage) <0) {
		err("Unable to set voltage");
		goto error;
	}

	/* Turn display on */
	if((ret = brlvger_set_display_on_off(priv, 1)) <0) {
		err("Error %d while turning display on", ret);
		goto error;
	}

	/* Mark as opened, so disconnect cannot free priv. */
	priv->opened = 1;

	file->private_data = priv;

	ret = 0;
	goto out;

 error:
	MOD_DEC_USE_COUNT;
 out:
	up(&priv->open_sem);
	return ret;
}

static int
brlvger_release(struct inode *inode, struct file *file)
{
	struct brlvger_priv *priv = file->private_data;
	int r;

	/* Turn display off. Safe even if disconnected. */
	brlvger_set_display_on_off(priv, 0);

	/* mutex with disconnect and with open */
	down(&priv->open_sem);

	if(!priv->dev) {
		dbg("Releasing disconnected device %d", priv->subminor);
		/* no up(&priv->open_sem) */
		kfree(priv);
	}else{
		dbg("Closing display %d", priv->subminor);
		/* Disable interrupts */
		if((r = usb_unlink_urb(priv->intr_urb)) <0)
			err("usb_unlink_urb returns %d", r);
		usb_free_urb(priv->intr_urb);
		priv->opened = 0;
		up(&priv->open_sem);
	}

	MOD_DEC_USE_COUNT;

	return 0;
}

static ssize_t
brlvger_write(struct file *file, const char *buffer,
	      size_t count, loff_t *pos)
{
	struct brlvger_priv *priv = file->private_data;
	char buf[MAX_BRLVGER_CELLS];
	int ret;
	size_t rs;
	loff_t off;

	__u16 written;

	if(!priv->dev)
		return -ENOLINK;

	off = *pos;

	if(off > priv->plength)
		return -ESPIPE;;

	rs = priv->plength - off;

	if(count > rs)
		count = rs;
	written = count;

	if (copy_from_user (buf, buffer, count ) )
		return -EFAULT;

	memset(priv->obuf, 0xaa, sizeof(priv->obuf));

	/* Firmware supports multiples of 8cells, so some cells are absent
	   and for some reason there actually are holes! euurkkk! */

	if( priv->plength == 44 ) {
		/* Two ghost cells at the beginning of the display, plus
		   two more after the sixth physical cell. */
		if(off > 5) {
			off +=4;
			memcpy(priv->obuf, buf, count);
		}else{
			int firstpart = 6 - off;
			
#ifdef WRITE_DEBUG
			dbg3("off: %lld, rs: %d, count: %d, firstpart: %d",
			     off, rs, count, firstpart);
#endif

			firstpart = (firstpart < count) ? firstpart : count;

#ifdef WRITE_DEBUG
			dbg3("off: %lld", off);
			dbg3("firstpart: %d", firstpart);
#endif

			memcpy(priv->obuf, buf, firstpart);

			if(firstpart != count) {
				int secondpart = count - firstpart;
#ifdef WRITE_DEBUG
				dbg3("secondpart: %d", secondpart);
#endif

				memcpy(priv->obuf+(firstpart+2),
				       buf+firstpart, secondpart);
				written +=2;
			}

			off +=2;

#ifdef WRITE_DEBUG
			dbg3("off: %lld, rs: %d, count: %d, firstpart: %d, "
				"written: %d", 	off, rs, count, firstpart, written);
#endif
		}
	}else{
		/* Two ghost cells at the beginningg of the display. */
		memcpy(priv->obuf, buf, count);
		off += 2;
	}

	{
		int repeat = write_repeats;
		/* Dirty hack: sometimes some of the dots are wrong and somehow
		   right themselves if the command is repeated. */
		while(repeat--) {
			ret = sndcontrolmsg(priv,
				BRLVGER_SEND_BRAILLE, BRLVGER_WRITE_REQ, 0,
				off, priv->obuf, written);
			if(ret <0)
				return ret;
		}
	}

	return count;
}

static int
read_index(struct brlvger_priv *priv)
{
	int intr_idx, read_idx;

	read_idx = atomic_read(&priv->read_idx);
	read_idx = ++read_idx == MAX_INTERRUPT_BUFFER ? 0 : read_idx;

	intr_idx = atomic_read(&priv->intr_idx);

	return(read_idx == intr_idx ? -1 : read_idx);
}

static ssize_t
brlvger_read(struct file *file, char *buffer,
	     size_t count, loff_t *unused_pos)
{
	struct brlvger_priv *priv = file->private_data;
	int read_idx;

	if(count != MAX_INTERRUPT_DATA)
		return -EINVAL;

	if(!priv->dev)
		return -ENOLINK;

	if((read_idx = read_index(priv)) == -1) {
		/* queue empty */
		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN;
		else{
			int r = wait_event_interruptible(priv->read_wait,
							 (!priv->dev || (read_idx = read_index(priv)) != -1));
			if(!priv->dev)
				return -ENOLINK;
			if(r)
				return r;
			if(read_idx == -1)
				/* should not happen */
				return 0;
		}
	}

	if (copy_to_user (buffer, priv->event_queue[read_idx], count) )
		return( -EFAULT);

	atomic_set(&priv->read_idx, read_idx);
	/* Multiple opens are not allowed. Yet on SMP, two processes could
	   read at the same time (on a shared file descriptor); then it is not
	   deterministic whether or not they will get duplicates of a key
	   event. */
	return MAX_INTERRUPT_DATA;
}

static int
brlvger_ioctl(struct inode *inode, struct file *file,
	      unsigned cmd, unsigned long arg)
{
	struct brlvger_priv *priv = file->private_data;

	if(!priv->dev)
		return -ENOLINK;

	switch(cmd) {
	case BRLVGER_GET_INFO: {
		struct brlvger_info vi;

		strncpy(vi.driver_version, DRIVER_VERSION,
			sizeof(vi.driver_version));
		vi.driver_version[sizeof(vi.driver_version)-1] = 0;
		strncpy(vi.driver_banner, longbanner,
			sizeof(vi.driver_banner));
		vi.driver_banner[sizeof(vi.driver_banner)-1] = 0;

		vi.display_length = priv->plength;
		
		memcpy(&vi.hwver, priv->hwver, BRLVGER_HWVER_SIZE);
		memcpy(&vi.fwver, priv->fwver, BRLVGER_FWVER_SIZE);
		memcpy(&vi.serialnum, priv->serialnum, BRLVGER_SERIAL_SIZE);

		if(copy_to_user((void *)arg, &vi, sizeof(vi)))
			return -EFAULT;
		return 0;
	}
	case BRLVGER_DISPLAY_ON:
		return brlvger_set_display_on_off(priv, 1);
	case BRLVGER_DISPLAY_OFF:
		return brlvger_set_display_on_off(priv, 0);
	case BRLVGER_BUZZ: {
		__u16 duration;
		if(get_user(duration, (__u16 *)arg))
			return -EFAULT;
		return brlvger_beep(priv, duration);
	}

#if 0 /* Underlying commands don't seem to work for some reason; not clear if
	 we'd want to export these anyway. */
	case BRLVGER_SET_VOLTAGE: {
		__u16 voltage;
		if(get_user(voltage, (__u16 *)arg))
			return -EFAULT;
		return brlvger_set_display_voltage(priv, voltage);
	}
	case BRLVGER_GET_VOLTAGE: {
		__u8 voltage;
		int r = brlvger_get_display_voltage(priv);
		if(r <0)
			return r;
		voltage = r;
		if(put_user(voltage, (__u8 *)arg))
			return -EFAULT;
		return 0;
	}
#endif
	default:
		return -EINVAL;
	};
}

static loff_t
brlvger_llseek(struct file *file, loff_t offset, int orig)
{
	struct brlvger_priv *priv = file->private_data;

	if(!priv->dev)
		return -ENOLINK;

	switch (orig) {
		case 0:
			/*  nothing to do */
			break;
		case 1:
			offset +=file->f_pos;
			break;
		case 2:
			offset += priv->plength;
		default:
			return -EINVAL;
	}

	if((offset >= priv->plength) || (offset < 0))
		return -EINVAL;

	return (file->f_pos = offset);
}

static unsigned
brlvger_poll(struct file *file, poll_table *wait) 
{
	struct brlvger_priv *priv = file->private_data;

	if(!priv->dev)
		return POLLERR | POLLHUP;

	poll_wait(file, &priv->read_wait, wait);

	if(!priv->dev)
		return POLLERR | POLLHUP;
	if(read_index(priv) != -1)
		return POLLIN | POLLRDNORM;

	return 0;
}

static void
intr_callback(struct urb *urb)
{
	struct brlvger_priv *priv = urb->context;
	int intr_idx, read_idx;

	if( urb->status ) {
		if(urb->status == -ETIMEDOUT)
			dbg2("Status -ETIMEDOUT, "
			     "probably disconnected");
		else if(urb->status != -ENOENT)
			err("Status: %d", urb->status);
		return;
	}

	read_idx = atomic_read(&priv->read_idx);
	spin_lock(&priv->intr_idx_lock);
	intr_idx = atomic_read(&priv->intr_idx);
	if(read_idx == intr_idx) {
		dbg2("Queue full, dropping braille display input");
		spin_unlock(&priv->intr_idx_lock);
		return;	/* queue full */
	}

	memcpy(priv->event_queue[intr_idx], urb->transfer_buffer,
	       MAX_INTERRUPT_DATA);

	intr_idx = (++intr_idx == MAX_INTERRUPT_BUFFER)? 0 : intr_idx;
	atomic_set(&priv->intr_idx, intr_idx);
	spin_unlock(&priv->intr_idx_lock);

	wake_up_interruptible(&priv->read_wait);
}

/* ----------------------------------------------------------------------- */

/* Hardware access functions */

static int
mycontrolmsg(const char *funcname,
	     struct brlvger_priv *priv, unsigned pipe_dir,
	     __u8 request, __u8 requesttype, __u16 value,
	     __u16 index, void *data, __u16 size)
{
	int ret=0, tries = stall_tries;

	/* Make sure the device was not disconnected */
	if(down_interruptible(&priv->dev_sem))
		return -ERESTARTSYS;
	if(!priv->dev) {
		up(&priv->dev_sem);
		return -ENOLINK;
	}

	/* Dirty hack for retransmission: stalls and fails all the time
	   without this on the hardware we tested. */
	while(tries--) {
		ret = usb_control_msg(priv->dev,
		    usb_sndctrlpipe(priv->dev,0) |pipe_dir,
		    request, requesttype, value,
		    index, data, size,
		    HZ);
		if(ret != -EPIPE)
			break;
		dbg2("Stalled, remaining %d tries", tries);
	}
	up(&priv->dev_sem);
	if(ret <0) {
		err("%s: usb_control_msg returns %d",
				funcname, ret);
		return -EIO;
	}
	return 0;
}

static int
brlvger_get_hw_version(struct brlvger_priv *priv, unsigned char *verbuf)
{
	return rcvcontrolmsg(priv,
	    BRLVGER_GET_HWVERSION, BRLVGER_READ_REQ, 0,
	    0, verbuf, BRLVGER_HWVER_SIZE);
	/* verbuf should be 2 bytes */
}

static int
brlvger_get_fw_version(struct brlvger_priv *priv, unsigned char *buf)
{
	unsigned char rawbuf[(BRLVGER_FWVER_SIZE-1)*2+2];
	int i, len;
	int r = rcvcontrolmsg(priv,
			      BRLVGER_GET_FWVERSION, BRLVGER_READ_REQ, 0,
			      0, rawbuf, sizeof(rawbuf));
	if(r<0)
		return r;

	/* If I guess correctly: succession of 16bit words, the string is
           formed of the first byte of each of these words. First byte in
           buffer indicates total length of data; not sure what second byte is
           for. */
	len = rawbuf[0]-2;
	if(len<0)
		len = 0;
	else if(len+1 > BRLVGER_FWVER_SIZE)
		len = BRLVGER_FWVER_SIZE-1;
	for(i=0; i<len; i++)
		buf[i] = rawbuf[2+2*i];
	buf[i] = 0;
	return 0;
}

static int
brlvger_get_serial(struct brlvger_priv *priv, unsigned char *buf)
{
	unsigned char rawserial[BRLVGER_SERIAL_BIN_SIZE];
	int i;
	int r = rcvcontrolmsg(priv,
			      BRLVGER_GET_SERIAL, BRLVGER_READ_REQ, 0,
			      0, rawserial, sizeof(rawserial));
	if(r<0)
		return r;

	for(i=0; i<BRLVGER_SERIAL_BIN_SIZE; i++) {
#define NUM_TO_HEX(n) (((n)>9) ? (n)+'A' : (n)+'0')
		buf[2*i] = NUM_TO_HEX(rawserial[i] >>4);
		buf[2*i+1] = NUM_TO_HEX(rawserial[i] &0xf);
	}
	buf[2*i] = 0;
	return 0;
}

static int
brlvger_get_display_length(struct brlvger_priv *priv)
{
	unsigned char data[2];
	int ret = rcvcontrolmsg(priv,
	    BRLVGER_GET_LENGTH, BRLVGER_READ_REQ, 0,
	    0, data, 2);
	if(ret<0)
		return ret;
	return data[1];
}

static int
brlvger_beep(struct brlvger_priv *priv, __u16 duration)
{
	return sndcontrolmsg(priv,
	    BRLVGER_BEEP, BRLVGER_WRITE_REQ, duration,
	    0, NULL, 0);
}

static int
brlvger_set_display_on_off(struct brlvger_priv *priv, __u16 on)
{
	dbg2("Turning display %s", ((on) ? "on" : "off"));
	return sndcontrolmsg(priv,
	    BRLVGER_SET_DISPLAY_ON,	BRLVGER_WRITE_REQ, on,
	    0, NULL, 0);
}

static int
brlvger_set_display_voltage(struct brlvger_priv *priv, __u16 voltage)
{
	dbg("SET_DISPLAY_VOLTAGE to %u", voltage);
        return sndcontrolmsg(priv,
	     BRLVGER_SET_DISPLAY_VOLTAGE, BRLVGER_WRITE_REQ, voltage,
	     0, NULL, 0);
}

#if 0 /* Had problems testing these commands. Not particularly useful anyway.*/

static int
brlvger_get_display_voltage(struct brlvger_priv *priv)
{
	__u8 voltage = 0;
	int ret = rcvcontrolmsg(priv,
	    BRLVGER_GET_DISPLAY_VOLTAGE, BRLVGER_READ_REQ, 0,
	    0, &voltage, 1);
	if(ret<0)
		return ret;
	return voltage;
}

static int
brlvger_get_current(struct brlvger_priv *priv)
{
	unsigned char data;
	int ret = rcvcontrolmsg(priv,
	    BRLVGER_GET_CURRENT,	BRLVGER_READ_REQ,	0,
	    0, &data, 1);
	if(ret<0)
		return ret;
	return data;
}
#endif
