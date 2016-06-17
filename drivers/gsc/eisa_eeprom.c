#include <linux/config.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <asm/gsc.h>
#include <asm/uaccess.h>
#include <asm/eisa_eeprom.h>

#define 	EISA_EEPROM_MINOR 241

static unsigned long eeprom_addr;

static long long eisa_eeprom_llseek(struct file *file, loff_t offset, int origin )
{
	switch (origin) {
	  case 0:
		/* nothing to do */
		break;
	  case 1:
		offset += file->f_pos;
		break;
	  case 2:
		offset += HPEE_MAX_LENGTH;
		break;
	}
	return (offset >= 0 && offset < HPEE_MAX_LENGTH) ? (file->f_pos = offset) : -EINVAL;
}

static ssize_t eisa_eeprom_read(struct file * file,
			      char *buf, size_t count, loff_t *ppos )
{
	unsigned char *tmp;
	ssize_t ret;
	int i;
	
	if (*ppos >= HPEE_MAX_LENGTH)
		return 0;
	
	count = *ppos + count < HPEE_MAX_LENGTH ? count : HPEE_MAX_LENGTH - *ppos;
	tmp = kmalloc(count, GFP_KERNEL);
	if (tmp) {
		for (i = 0; i < count; i++)
			tmp[i] = gsc_readb(eeprom_addr+(*ppos)++);

		if (copy_to_user (buf, tmp, count))
			ret = -EFAULT;
		else
			ret = count;
		kfree (tmp);
	} else
		ret = -ENOMEM;
	
	return ret;
}

static int eisa_eeprom_ioctl(struct inode *inode, struct file *file, 
			   unsigned int cmd,
			   unsigned long arg)
{
	return -ENOTTY;
}

static int eisa_eeprom_open(struct inode *inode, struct file *file)
{
	if (file->f_mode & 2 || eeprom_addr == 0)
		return -EINVAL;
   
	return 0;
}

static int eisa_eeprom_release(struct inode *inode, struct file *file)
{
	return 0;
}

/*
 *	The various file operations we support.
 */
static struct file_operations eisa_eeprom_fops = {
	owner:		THIS_MODULE,
	llseek:		eisa_eeprom_llseek,
	read:		eisa_eeprom_read,
	ioctl:		eisa_eeprom_ioctl,
	open:		eisa_eeprom_open,
	release:	eisa_eeprom_release,
};

static struct miscdevice eisa_eeprom_dev=
{
	EISA_EEPROM_MINOR,
	"eisa eeprom",
	&eisa_eeprom_fops
};

int __init eisa_eeprom_init(unsigned long addr)
{
	if (addr) {
		eeprom_addr = addr;
		misc_register(&eisa_eeprom_dev);
		printk(KERN_INFO "EISA EEPROM at 0x%lx\n", eeprom_addr);
	}
	return 0;
}

MODULE_LICENSE("GPL");
