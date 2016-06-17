/*
 *	ALi M7101 PMU Computer Watchdog Timer driver for Linux 2.4.x
 *
 *	Based on w83877f_wdt.c by Scott Jennings <management@oro.net>
 *	and the Cobalt kernel WDT timer driver by Tim Hockin
 *	                                      <thockin@cobaltnet.com>
 *
 *	(c)2002 Steve Hill <steve@navaho.co.uk>
 * 
 *  Theory of operation:
 *  A Watchdog Timer (WDT) is a hardware circuit that can 
 *  reset the computer system in case of a software fault.
 *  You probably knew that already.
 *
 *  Usually a userspace daemon will notify the kernel WDT driver
 *  via the /proc/watchdog special device file that userspace is
 *  still alive, at regular intervals.  When such a notification
 *  occurs, the driver will usually tell the hardware watchdog
 *  that everything is in order, and that the watchdog should wait
 *  for yet another little while to reset the system.
 *  If userspace fails (RAM error, kernel bug, whatever), the
 *  notifications cease to occur, and the hardware watchdog will
 *  reset the system (causing a reboot) after the timeout occurs.
 *
 *  This WDT driver is different from most other Linux WDT
 *  drivers in that the driver will ping the watchdog by itself,
 *  because this particular WDT has a very short timeout (1.6
 *  seconds) and it would be insane to count on any userspace
 *  daemon always getting scheduled within that time frame.
 */

#include <linux/module.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/miscdevice.h>
#include <linux/watchdog.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#include <linux/fcntl.h>
#include <linux/smp_lock.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/init.h>
#include <linux/pci.h>

#define OUR_NAME "alim7101_wdt"

#define WDT_ENABLE 0x9C
#define WDT_DISABLE 0x8C

#define ALI_7101_WDT    0x92
#define ALI_WDT_ARM     0x01

/*
 * We're going to use a 1 second timeout.
 * If we reset the watchdog every ~250ms we should be safe.  */

#define WDT_INTERVAL (HZ/4+1)

/*
 * We must not require too good response from the userspace daemon.
 * Here we require the userspace daemon to send us a heartbeat
 * char to /dev/watchdog every 30 seconds.
 */

#define WDT_HEARTBEAT (HZ * 30)

static void wdt_timer_ping(unsigned long);
static struct timer_list timer;
static unsigned long next_heartbeat;
static unsigned long wdt_is_open;
static int wdt_expect_close;
static struct pci_dev *alim7101_pmu;

#ifdef CONFIG_WATCHDOG_NOWAYOUT
static int nowayout = 1;
#else
static int nowayout = 0;
#endif

MODULE_PARM(nowayout,"i");
MODULE_PARM_DESC(nowayout, "Watchdog cannot be stopped once started (default=CONFIG_WATCHDOG_NOWAYOUT)");

/*
 *	Whack the dog
 */

static void wdt_timer_ping(unsigned long data)
{
	/* If we got a heartbeat pulse within the WDT_US_INTERVAL
	 * we agree to ping the WDT 
	 */
	char	tmp;

	if(time_before(jiffies, next_heartbeat)) 
	{
		/* Ping the WDT (this is actually a disarm/arm sequence) */
		pci_read_config_byte(alim7101_pmu, 0x92, &tmp);
		pci_write_config_byte(alim7101_pmu, ALI_7101_WDT, (tmp & ~ALI_WDT_ARM));
		pci_write_config_byte(alim7101_pmu, ALI_7101_WDT, (tmp | ALI_WDT_ARM));
	} else {
		printk(OUR_NAME ": Heartbeat lost! Will not ping the watchdog\n");
	}
	/* Re-set the timer interval */
	timer.expires = jiffies + WDT_INTERVAL;
	add_timer(&timer);
}

/* 
 * Utility routines
 */

static void wdt_change(int writeval)
{
	char	tmp;

	pci_read_config_byte(alim7101_pmu, 0x92, &tmp);
	if (writeval == WDT_ENABLE)
		pci_write_config_byte(alim7101_pmu, ALI_7101_WDT, (tmp | ALI_WDT_ARM));
	else
		pci_write_config_byte(alim7101_pmu, ALI_7101_WDT, (tmp & ~ALI_WDT_ARM));
}

static void wdt_startup(void)
{
	next_heartbeat = jiffies + WDT_HEARTBEAT;

	/* We must enable before we kick off the timer in case the timer
	   occurs as we ping it */

	wdt_change(WDT_ENABLE);

	/* Start the timer */
	timer.expires = jiffies + WDT_INTERVAL;	
	add_timer(&timer);


	printk(OUR_NAME ": Watchdog timer is now enabled.\n");  
}

static void wdt_turnoff(void)
{
	/* Stop the timer */
	del_timer_sync(&timer);
	wdt_change(WDT_DISABLE);
	printk(OUR_NAME ": Watchdog timer is now disabled...\n");
}

/*
 * /dev/watchdog handling
 */

static ssize_t fop_write(struct file * file, const char * buf, size_t count, loff_t * ppos)
{
	/* We can't seek */
	if(ppos != &file->f_pos)
		return -ESPIPE;

	/* See if we got the magic character */
	if(count) 
	{
		if (!nowayout) {
			size_t ofs;
	
			/* note: just in case someone wrote the magic character
			 * five months ago... */
			wdt_expect_close = 0;
	
			/* now scan */
			for(ofs = 0; ofs != count; ofs++)
			{
				char c;
				if(get_user(c, buf+ofs))
					return -EFAULT;
				if(c == 'V')
					wdt_expect_close = 1;
			}
		}

		/* someone wrote to us, we should restart timer */
		next_heartbeat = jiffies + WDT_HEARTBEAT;
		return 1;
	};
	return 0;
}

static ssize_t fop_read(struct file * file, char * buf, size_t count, loff_t * ppos)
{
	/* No can do */
	return -EINVAL;
}

static int fop_open(struct inode * inode, struct file * file)
{
	/* Just in case we're already talking to someone... */
	if(test_and_set_bit(0, &wdt_is_open))
		return -EBUSY;
	/* Good, fire up the show */
	wdt_startup();
	return 0;
}

static int fop_close(struct inode * inode, struct file * file)
{
	if ((wdt_expect_close) || (! nowayout))
		wdt_turnoff();
	else {
		printk(OUR_NAME ": device file closed unexpectedly. Will not stop the WDT!\n");
	}
	clear_bit(0, &wdt_is_open);
	return 0;
}

static int fop_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	static struct watchdog_info ident=
	{
		WDIOF_MAGICCLOSE,
		1,
		"ALiM7101"
	};
	
	switch(cmd)
	{
		case WDIOC_GETSUPPORT:
			return copy_to_user((struct watchdog_info *)arg, &ident, sizeof(ident))?-EFAULT:0;
		case WDIOC_KEEPALIVE:
			next_heartbeat = jiffies + WDT_HEARTBEAT;
			return 0;
		default:
			return -ENOTTY;
	}
}

static struct file_operations wdt_fops = {
	owner:		THIS_MODULE,
	llseek:		no_llseek,
	read:		fop_read,
	write:		fop_write,
	open:		fop_open,
	release:	fop_close,
	ioctl:		fop_ioctl
};

static struct miscdevice wdt_miscdev = {
	WATCHDOG_MINOR,
	"watchdog",
	&wdt_fops
};

/*
 *	Notifier for system down
 */

static int wdt_notify_sys(struct notifier_block *this, unsigned long code, void *unused)
{
	if (code==SYS_DOWN || code==SYS_HALT) wdt_turnoff();
	if (code==SYS_RESTART) {
		/*
		 * Cobalt devices have no way of rebooting themselves other than
		 * getting the watchdog to pull reset, so we restart the watchdog on
		 * reboot with no heartbeat
		 */
		wdt_change(WDT_ENABLE);
		printk(OUR_NAME ": Watchdog timer is now enabled with no heartbeat - should reboot in ~1 second.\n");
	};
	return NOTIFY_DONE;
}
 
/*
 *	The WDT needs to learn about soft shutdowns in order to
 *	turn the timebomb registers off. 
 */
 
static struct notifier_block wdt_notifier=
{
	wdt_notify_sys,
	0,
	0
};

static void __exit alim7101_wdt_unload(void)
{
	wdt_turnoff();
	/* Deregister */
	misc_deregister(&wdt_miscdev);
	unregister_reboot_notifier(&wdt_notifier);
}

static int __init alim7101_wdt_init(void)
{
	int rc = -EBUSY;
	struct pci_dev *ali1543_south;
	char tmp;

	printk(KERN_INFO OUR_NAME ": Steve Hill <steve@navaho.co.uk>.\n");
	alim7101_pmu = pci_find_device(PCI_VENDOR_ID_AL, PCI_DEVICE_ID_AL_M7101,NULL);
	if (!alim7101_pmu) {
		printk(KERN_INFO OUR_NAME ": ALi M7101 PMU not present - WDT not set\n");
		return -EBUSY;
	};
	
	/* Set the WDT in the PMU to 1 second */
	pci_write_config_byte(alim7101_pmu, ALI_7101_WDT, 0x02);

	ali1543_south = pci_find_device(PCI_VENDOR_ID_AL, PCI_DEVICE_ID_AL_M1533, NULL);
	if (!ali1543_south) {
		printk(KERN_INFO OUR_NAME ": ALi 1543 South-Bridge not present - WDT not set\n");
		return -EBUSY;
	};
	pci_read_config_byte(ali1543_south, 0x5e, &tmp);
	if ((tmp & 0x1e) != 0x12) {
		printk(KERN_INFO OUR_NAME ": ALi 1543 South-Bridge does not have the correct revision number (???1001?) - WDT not set\n");
		return -EBUSY;
	};

	init_timer(&timer);
	timer.function = wdt_timer_ping;
	timer.data = 1;
		
	rc = misc_register(&wdt_miscdev);
	if (rc)
		return rc;

	rc = register_reboot_notifier(&wdt_notifier);
	if (rc) {
		misc_deregister(&wdt_miscdev);
		return rc;
	};
	
	printk(KERN_INFO OUR_NAME ": WDT driver for ALi M7101 initialised.\n");
	return 0;
}

module_init(alim7101_wdt_init);
module_exit(alim7101_wdt_unload);

EXPORT_NO_SYMBOLS;
MODULE_AUTHOR("Steve Hill");
MODULE_LICENSE("GPL");
