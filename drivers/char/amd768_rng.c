/*
 	Hardware driver for the AMD 768/8111 Random Number Generator (RNG)
	(c) Copyright 2001 Red Hat Inc <alan@redhat.com>
	(c) Copyright 2002,2003 Andi Kleen, SuSE Labs

 	derived from
 
	Hardware driver for Intel i810 Random Number Generator (RNG)
	Copyright 2000,2001 Jeff Garzik <jgarzik@pobox.com>
	Copyright 2000,2001 Philipp Rumpf <prumpf@mandrakesoft.com>

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
#define RNG_VERSION "0.1.0"
#define RNG_MODULE_NAME "amd768_rng"
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

#define RNG_MISCDEV_MINOR		183 /* official */

/*
 * various RNG status variables.  they are globals
 * as we only support a single RNG device
 */

static u32 pmbase;			/* PMxx I/O base */
static struct semaphore rng_open_sem;	/* Semaphore for serializing rng_open/release */


/*
 * inlined helper functions for accessing RNG registers
 */

static inline int rng_data_present (void)
{
      	return inl(pmbase+0xF4) & 1;
}


static inline int rng_data_read (void)
{
	return inl(pmbase+0xF0);
}

static int rng_dev_open (struct inode *inode, struct file *filp)
{
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
	return 0;
}


static int rng_dev_release (struct inode *inode, struct file *filp)
{
	up(&rng_open_sem);
	return 0;
}


static ssize_t rng_dev_read (struct file *filp, char *buf, size_t size,
			     loff_t * offp)
{
	static spinlock_t rng_lock = SPIN_LOCK_UNLOCKED;
	int have_data;
	u32 data = 0;
	ssize_t ret = 0;

	while (size) {
		spin_lock(&rng_lock);

		have_data = 0;
		if (rng_data_present()) {
			data = rng_data_read();
			have_data = 4;
		}

		spin_unlock (&rng_lock);

		while (have_data > 0) {
			if (put_user((u8)data, buf++)) {
				ret = ret ? : -EFAULT;
				break;
			}
			size--;
			ret++;
			have_data--;
			data>>=8;
		}

		if (filp->f_flags & O_NONBLOCK)
			return ret ? : -EAGAIN;

		if(current->need_resched)
		{
			current->state = TASK_INTERRUPTIBLE;
			schedule_timeout(1);
		}
		else
			udelay(200);	/* FIXME: We could poll for 250uS ?? */

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
	u8 rnen;

	DPRINTK ("ENTER\n");

	rc = misc_register (&rng_miscdev);
	if (rc) {
		printk (KERN_ERR PFX "cannot register misc device\n");
		DPRINTK ("EXIT, returning %d\n", rc);
		goto err_out;
	}

	pci_read_config_dword(dev, 0x58, &pmbase);

	pmbase&=0x0000FF00;

	if(pmbase == 0)
	{
		printk (KERN_ERR PFX "power management base not set\n");
		DPRINTK ("EXIT, returning %d\n", rc);
		goto err_out_free_miscdev;
	}

	pci_read_config_byte(dev, 0x40, &rnen);
	rnen|=(1<<7);	/* RNG on */
	pci_write_config_byte(dev, 0x40, rnen);

	pci_read_config_byte(dev, 0x41, &rnen);
	rnen|=(1<<7);	/* PMIO enable */
	pci_write_config_byte(dev, 0x41, rnen);

	printk(KERN_INFO PFX "AMD768 system management I/O registers at 0x%X.\n", pmbase);
	DPRINTK ("EXIT, returning 0\n");
	return 0;

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
	{ 0x1022, 0x7443, PCI_ANY_ID, PCI_ANY_ID, },
	{ 0x1022, 0x746b, PCI_ANY_ID, PCI_ANY_ID, },
	{ 0, },
};
MODULE_DEVICE_TABLE (pci, rng_pci_tbl);


MODULE_AUTHOR("Alan Cox, Jeff Garzik, Philipp Rumpf, Matt Sottek, Andi Kleen");
MODULE_DESCRIPTION("AMD 768/8111 Random Number Generator (RNG) driver");
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
	DPRINTK ("EXIT\n");
}


module_init (rng_init);
module_exit (rng_cleanup);
