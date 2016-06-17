/*
 * Drivers for the Total Impact PPC based computer "BRIQ"
 * by Dr. Karsten Jeppesen
 *
 *
 * 010407		Coding started
 * 
 * 04/20/2002	1.1	Adapted to 2.4, small cleanups
 */

#include <linux/module.h>

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/tty.h>
#include <linux/timer.h>
#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/wait.h>
#include <linux/string.h>
#include <linux/malloc.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/init.h>

#include <asm/uaccess.h>
#include <asm/io.h>

#define		TOTALIMPACT_VFD_MINOR	156
#define		VFD_IOPORT		0x0390
#define		LED_IOPORT		0x0398
#define		TI_VER                  "1.1 (04/20/2002)"
#define		TI_MSG0			"Loading Linux"

static int		vfd_is_open;
static unsigned char	vfd[40];
static int		vfd_cursor;
static unsigned char	ledpb, led;


static void UpdateVFD( void )
{
	int	i;
	outb(0x02, VFD_IOPORT);	/* cursor home */
	for (i=0; i<20; i++) outb(vfd[i], VFD_IOPORT + 1);
	outb(0xc0, VFD_IOPORT); /* cursor to next line */
	for (i=20; i<40; i++) outb(vfd[i], VFD_IOPORT + 1);

}

static void SetLED( char state)
{
	if ( state == 'R' ) led = 0x01;
	else if ( state == 'G' ) led = 0x02;
	else if ( state == 'Y' ) led = 0x03;
	else if ( state == 'X' ) led = 0x00;
	outb(led, LED_IOPORT);
}

static int do_open(struct inode *ino, struct file *filep)
{
	if (vfd_is_open) return -EBUSY;
	MOD_INC_USE_COUNT;
	vfd_is_open = 1;
	return(0);
}

static int do_release(struct inode *ino, struct file *filep)
{
	if (!vfd_is_open) return -ENODEV;
	MOD_DEC_USE_COUNT;
	vfd_is_open = 0;
	return(0);
}


static ssize_t do_read(struct file *file, char *buf, size_t count,
			 loff_t *ppos)
{
	unsigned short c;
	unsigned char cp;

	/*  Can't seek (pread) on this device  */
	if (ppos != &file->f_pos) return -ESPIPE;
	if (!vfd_is_open) return -ENODEV;
	c = (inb( LED_IOPORT ) & 0x000c) | (ledpb & 0x0003);
	SetLED(' ');
	if ((!(ledpb & 0x0004)) && (c & 0x0004))
	{	/* upper button released */
		cp = ' ';
		ledpb = c;
		if (copy_to_user(buf, &cp, 1)) return -EFAULT;
		return(1);
	}
	else if ((!(ledpb & 0x0008)) && (c & 0x0008))
	{	/* lower button released */
		cp = '\r';
		ledpb = c;
		if (copy_to_user(buf, &cp, 1)) return -EFAULT;
		return(1);
	} else
	{
		ledpb = c;
		return(0);
	}
}


static void ScrollVFD( void )
{
	int	i;
	for (i=0; i<20; i++)
	{
		vfd[i] = vfd[i+20];
		vfd[i+20] = ' ';
	}
	vfd_cursor = 20;
}


static ssize_t do_write(struct file *file, const char *buf, size_t len,
			  loff_t *ppos)
{
	size_t		indx = len;
	int		i, esc=0;
	/*  Can't seek (pwrite) on this device  */
	if (ppos != &file->f_pos) return -ESPIPE;
	if (!vfd_is_open) return -EBUSY;
	for (;;)
	{
		if (!indx) break;
		if (esc)
		{
			SetLED(*buf);
			esc = 0;
		}
		else if (*buf == 27)
		{
			esc = 1;
		}
		else if (*buf == 12)
		{ /* do a form feed */
			for (i=0; i<40; i++) vfd[i] = ' ';
			vfd_cursor = 0;
		}
		else if (*buf == 10)
		{
			if (vfd_cursor < 20) vfd_cursor = 20;
			else if (vfd_cursor < 40) vfd_cursor = 40;
			else if (vfd_cursor < 60) vfd_cursor = 60;
			if (vfd_cursor > 59) ScrollVFD();
		}
		else
		{
			/* just a character */
			if (vfd_cursor > 39) ScrollVFD();
			vfd[vfd_cursor++] = *buf;
		}
		indx--;
		buf++;
	}
	UpdateVFD();
	return len;
}


static struct file_operations vfd_fops = {
	read:		do_read,	/* Read */
	write:		do_write,	/* Write */
	open:		do_open,	/* Open */
	release:	do_release,	/* Release */
};


static struct miscdevice ti_vfd_miscdev = {
	TOTALIMPACT_VFD_MINOR,
	"vfd",
	&vfd_fops
};


static int __init briq_panel_init(void)
{
	struct device_node *root = find_path_device("/");
	char *machine;
	int i;

	machine = get_property(root, "model", NULL);
	if (!machine || strncmp(machine, "TotalImpact,BRIQ-1", 18) != 0)
		return -ENODEV;

	printk(KERN_INFO "ti_briq: v%s Dr. Karsten Jeppesen (kj@totalimpact.com)\n", TI_VER);

	if (!request_region( VFD_IOPORT, 4, "BRIQ Front Panel"))
		return -EBUSY;
	if (!request_region( LED_IOPORT, 2, "BRIQ Front Panel")) {
		release_region(VFD_IOPORT, 4);
		return -EBUSY;
	}
	ledpb = inb( LED_IOPORT ) & 0x000c;

	if (misc_register(&ti_vfd_miscdev) < 0) {
		release_region(VFD_IOPORT, 4);
		release_region(LED_IOPORT, 2);
		return -EBUSY;
	}

	outb(0x38, VFD_IOPORT);	/* Function set */
	outb(0x01, VFD_IOPORT);	/* Clear display */
	outb(0x0c, VFD_IOPORT);	/* Display on */
	outb(0x06, VFD_IOPORT);	/* Entry normal */
	for (i=0; i<40; i++) vfd[i]=' ';
#ifndef MODULE
	vfd[0] = 'L';
	vfd[1] = 'o';
	vfd[2] = 'a';
	vfd[3] = 'd';
	vfd[4] = 'i';
	vfd[5] = 'n';
	vfd[6] = 'g';
	vfd[7] = ' ';
	vfd[8] = '.';
	vfd[9] = '.';
	vfd[10] = '.';
#endif /* !MODULE */	
	UpdateVFD();

	return 0;
}


static void __exit briq_panel_exit(void)
{
	misc_deregister(&ti_vfd_miscdev);
	release_region(VFD_IOPORT, 4);
	release_region(LED_IOPORT, 2);
}


module_init(briq_panel_init);
module_exit(briq_panel_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Karsten Jeppesen <karsten@jeppesens.com>");
MODULE_DESCRIPTION("Driver for the Total Impact briQ front panel");
EXPORT_NO_SYMBOLS;
