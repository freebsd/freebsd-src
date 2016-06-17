/*

	Hardware driver for Intel i810 Random Number Generator (RNG)
	Copyright 2000,2001 Jeff Garzik <jgarzik@pobox.com>
	Copyright 2000,2001 Philipp Rumpf <prumpf@mandrakesoft.com>

	Driver Web site:  http://sourceforge.net/projects/gkernel/

	Please read Documentation/i810_rng.txt for details on use.

	----------------------------------------------------------

	This software may be used and distributed according to the terms
        of the GNU General Public License, incorporated herein by reference.

 */


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/random.h>
#include <linux/miscdevice.h>
#include <linux/smp_lock.h>
#include <linux/mm.h>
#include <linux/delay.h>

#include <asm/io.h>
#include <asm/uaccess.h>


/*
 * core module and version information
 */
#define RNG_VERSION "0.9.8"
#define RNG_MODULE_NAME "i810_rng"
#define RNG_DRIVER_NAME   RNG_MODULE_NAME " hardware driver " RNG_VERSION
#define PFX RNG_MODULE_NAME ": "


/*
 * debugging macros
 */
#undef RNG_DEBUG /* define to enable copious debugging info */

#ifdef RNG_DEBUG
/* note: prints function name for you */
#define DPRINTK(fmt, args...) printk(KERN_DEBUG "%s: " fmt, __FUNCTION__ , ## args)
#else
#define DPRINTK(fmt, args...)
#endif

#undef RNG_NDEBUG        /* define to disable lightweight runtime checks */
#ifdef RNG_NDEBUG
#define assert(expr)
#else
#define assert(expr) \
        if(!(expr)) {                                   \
        printk( "Assertion failed! %s,%s,%s,line=%d\n", \
        #expr,__FILE__,__FUNCTION__,__LINE__);          \
        }
#endif


/*
 * RNG registers (offsets from rng_mem)
 */
#define RNG_HW_STATUS			0
#define		RNG_PRESENT		0x40
#define		RNG_ENABLED		0x01
#define RNG_STATUS			1
#define		RNG_DATA_PRESENT	0x01
#define RNG_DATA			2

/*
 * Magic address at which Intel PCI bridges locate the RNG
 */
#define RNG_ADDR			0xFFBC015F
#define RNG_ADDR_LEN			3

#define RNG_MISCDEV_MINOR		183 /* official */

/*
 * various RNG status variables.  they are globals
 * as we only support a single RNG device
 */
static void *rng_mem;			/* token to our ioremap'd RNG register area */
static struct semaphore rng_open_sem;	/* Semaphore for serializing rng_open/release */


/*
 * inlined helper functions for accessing RNG registers
 */
static inline u8 rng_hwstatus (void)
{
	assert (rng_mem != NULL);
	return readb (rng_mem + RNG_HW_STATUS);
}

static inline u8 rng_hwstatus_set (u8 hw_status)
{
	assert (rng_mem != NULL);
	writeb (hw_status, rng_mem + RNG_HW_STATUS);
	return rng_hwstatus ();
}


static inline int rng_data_present (void)
{
	assert (rng_mem != NULL);

	return (readb (rng_mem + RNG_STATUS) & RNG_DATA_PRESENT) ? 1 : 0;
}


static inline int rng_data_read (void)
{
	assert (rng_mem != NULL);

	return readb (rng_mem + RNG_DATA);
}

/*
 * rng_enable - enable the RNG hardware
 */

static int rng_enable (void)
{
	int rc = 0;
	u8 hw_status, new_status;

	DPRINTK ("ENTER\n");

	hw_status = rng_hwstatus ();

	if ((hw_status & RNG_ENABLED) == 0) {
		new_status = rng_hwstatus_set (hw_status | RNG_ENABLED);

		if (new_status & RNG_ENABLED)
			printk (KERN_INFO PFX "RNG h/w enabled\n");
		else {
			printk (KERN_ERR PFX "Unable to enable the RNG\n");
			rc = -EIO;
		}
	}

	DPRINTK ("EXIT, returning %d\n", rc);
	return rc;
}

/*
 * rng_disable - disable the RNG hardware
 */

static void rng_disable(void)
{
	u8 hw_status, new_status;

	DPRINTK ("ENTER\n");

	hw_status = rng_hwstatus ();

	if (hw_status & RNG_ENABLED) {
		new_status = rng_hwstatus_set (hw_status & ~RNG_ENABLED);
	
		if ((new_status & RNG_ENABLED) == 0)
			printk (KERN_INFO PFX "RNG h/w disabled\n");
		else {
			printk (KERN_ERR PFX "Unable to disable the RNG\n");
		}
	}

	DPRINTK ("EXIT\n");
}

static int rng_dev_open (struct inode *inode, struct file *filp)
{
	int rc;

	if ((filp->f_mode & FMODE_READ) == 0)
		return -EINVAL;
	if (filp->f_mode & FMODE_WRITE)
		return -EINVAL;

	/* wait for device to become free */
	if (filp->f_flags & O_NONBLOCK) {
		if (down_trylock (&rng_open_sem))
			return -EAGAIN;
	} else {
		if (down_interruptible (&rng_open_sem))
			return -ERESTARTSYS;
	}

	rc = rng_enable ();
	if (rc) {
		up (&rng_open_sem);
		return rc;
	}

	return 0;
}


static int rng_dev_release (struct inode *inode, struct file *filp)
{
	rng_disable ();
	up (&rng_open_sem);
	return 0;
}


static ssize_t rng_dev_read (struct file *filp, char *buf, size_t size,
			     loff_t * offp)
{
	static spinlock_t rng_lock = SPIN_LOCK_UNLOCKED;
	int have_data;
	u8 data = 0;
	ssize_t ret = 0;

	while (size) {
		spin_lock (&rng_lock);

		have_data = 0;
		if (rng_data_present ()) {
			data = rng_data_read ();
			have_data = 1;
		}

		spin_unlock (&rng_lock);

		if (have_data) {
			if (put_user (data, buf++)) {
				ret = ret ? : -EFAULT;
				break;
			}
			size--;
			ret++;
		}

		if (filp->f_flags & O_NONBLOCK)
			return ret ? : -EAGAIN;

		if (current->need_resched)
		{
			current->state = TASK_INTERRUPTIBLE;
			schedule_timeout(1);
		}
		else
			udelay(200);

		if (signal_pending (current))
			return ret ? : -ERESTARTSYS;
	}

	return ret;
}


static struct file_operations rng_chrdev_ops = {
	owner:		THIS_MODULE,
	open:		rng_dev_open,
	release:	rng_dev_release,
	read:		rng_dev_read,
};


static struct miscdevice rng_miscdev = {
	RNG_MISCDEV_MINOR,
	RNG_MODULE_NAME,
	&rng_chrdev_ops,
};


/*
 * rng_init_one - look for and attempt to init a single RNG
 */
static int __init rng_init_one (struct pci_dev *dev)
{
	int rc;
	u8 hw_status;

	DPRINTK ("ENTER\n");

	rc = misc_register (&rng_miscdev);
	if (rc) {
		printk (KERN_ERR PFX "cannot register misc device\n");
		DPRINTK ("EXIT, returning %d\n", rc);
		goto err_out;
	}

	rng_mem = ioremap (RNG_ADDR, RNG_ADDR_LEN);
	if (rng_mem == NULL) {
		printk (KERN_ERR PFX "cannot ioremap RNG Memory\n");
		DPRINTK ("EXIT, returning -EBUSY\n");
		rc = -EBUSY;
		goto err_out_free_miscdev;
	}

	/* Check for Intel 82802 */
	hw_status = rng_hwstatus ();
	if ((hw_status & RNG_PRESENT) == 0) {
		printk (KERN_ERR PFX "RNG not detected\n");
		DPRINTK ("EXIT, returning -ENODEV\n");
		rc = -ENODEV;
		goto err_out_free_map;
	}

	/* turn RNG h/w off, if it's on */
	if (hw_status & RNG_ENABLED)
		hw_status = rng_hwstatus_set (hw_status & ~RNG_ENABLED);
	if (hw_status & RNG_ENABLED) {
		printk (KERN_ERR PFX "cannot disable RNG, aborting\n");
		goto err_out_free_map;
	}

	DPRINTK ("EXIT, returning 0\n");
	return 0;

err_out_free_map:
	iounmap (rng_mem);
err_out_free_miscdev:
	misc_deregister (&rng_miscdev);
err_out:
	return rc;
}


/*
 * Data for PCI driver interface
 *
 * This data only exists for exporting the supported
 * PCI ids via MODULE_DEVICE_TABLE.  We do not actually
 * register a pci_driver, because someone else might one day
 * want to register another driver on the same PCI id.
 */
static struct pci_device_id rng_pci_tbl[] __initdata = {
	{ 0x8086, 0x2418, PCI_ANY_ID, PCI_ANY_ID, },
	{ 0x8086, 0x2428, PCI_ANY_ID, PCI_ANY_ID, },
	{ 0x8086, 0x2448, PCI_ANY_ID, PCI_ANY_ID, },
	{ 0x8086, 0x244e, PCI_ANY_ID, PCI_ANY_ID, },
	{ 0x8086, 0x245e, PCI_ANY_ID, PCI_ANY_ID, },
	{ 0, },
};
MODULE_DEVICE_TABLE (pci, rng_pci_tbl);


MODULE_AUTHOR("Jeff Garzik, Philipp Rumpf, Matt Sottek");
MODULE_DESCRIPTION("Intel i8xx chipset Random Number Generator (RNG) driver");
MODULE_LICENSE("GPL");


/*
 * rng_init - initialize RNG module
 */
static int __init rng_init (void)
{
	int rc;
	struct pci_dev *pdev;

	DPRINTK ("ENTER\n");

	init_MUTEX (&rng_open_sem);

	pci_for_each_dev(pdev) {
		if (pci_match_device (rng_pci_tbl, pdev) != NULL)
			goto match;
	}

	DPRINTK ("EXIT, returning -ENODEV\n");
	return -ENODEV;

match:
	rc = rng_init_one (pdev);
	if (rc)
		return rc;

	printk (KERN_INFO RNG_DRIVER_NAME " loaded\n");

	DPRINTK ("EXIT, returning 0\n");
	return 0;
}


/*
 * rng_init - shutdown RNG module
 */
static void __exit rng_cleanup (void)
{
	DPRINTK ("ENTER\n");

	misc_deregister (&rng_miscdev);

	iounmap (rng_mem);

	DPRINTK ("EXIT\n");
}


module_init (rng_init);
module_exit (rng_cleanup);
