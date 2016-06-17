/* MiroSOUND PCM20 radio rds interface driver
 * (c) 2001 Robert Siemer <Robert.Siemer@gmx.de>
 * Thanks to Fred Seidel. See miropcm20-rds-core.c for further information.
 */

/* Revision history:
 *
 *   2001-04-18  Robert Siemer <Robert.Siemer@gmx.de>
 *        separate file for user interface driver
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/devfs_fs_kernel.h>
#include "miropcm20-rds-core.h"

devfs_handle_t dfsh;
char * text_buffer;
static int rds_users = 0;


static int rds_f_open(struct inode *in, struct file *fi)
{
	if(rds_users)
		return -EBUSY;

	if ((text_buffer=kmalloc(66, GFP_KERNEL)) == 0) {
		printk(KERN_NOTICE "aci-rds: Out of memory by open()...\n");
		return -ENOMEM;
	}

	rds_users++;
	MOD_INC_USE_COUNT;
	return 0;
}

static int rds_f_release(struct inode *in, struct file *fi)
{
	kfree(text_buffer);

	rds_users--;
	MOD_DEC_USE_COUNT;
	return 0;
}

static void print_matrix(char *ch, char out[])
{
        int j;

	for (j=7; j>=0; j--) {
		 out[7-j] = ((*ch >> j) & 0x1) + '0';
	}
}

static ssize_t rds_f_read(struct file *file, char *buffer, size_t length, loff_t *offset)
{
//	i = sprintf(text_buffer, "length: %d, offset: %d\n", length, *offset);

	char c;
	char bits[8];

	current->state=TASK_UNINTERRUPTIBLE;
	schedule_timeout(2*HZ);
	aci_rds_cmd(RDS_STATUS, &c, 1);
	print_matrix(&c, bits);
	if (copy_to_user(buffer, bits, 8))
		return -EFAULT;

/*	if ((c >> 3) & 1) {
		aci_rds_cmd(RDS_STATIONNAME, text_buffer+1, 8);
		text_buffer[0]  = ' ' ;
		text_buffer[9]  = '\n';
		return copy_to_user(buffer+8, text_buffer, 10) ? -EFAULT: 18;
	}
*/
/*	if ((c >> 6) & 1) {
		aci_rds_cmd(RDS_PTYTATP, &c, 1);
		if ( c & 1)
			sprintf(text_buffer, " M");
		else
			sprintf(text_buffer, " S");
		if ((c >> 1) & 1)
			sprintf(text_buffer+2, " TA");
		else
			sprintf(text_buffer+2, " --");
		if ((c >> 7) & 1)
			sprintf(text_buffer+5, " TP");
		else
			sprintf(text_buffer+5, " --");
		sprintf(text_buffer+8, " %2d\n", (c >> 2) & 0x1f);
		return copy_to_user(buffer+8, text_buffer, 12) ? -EFAULT: 20;
	}
*/

	if ((c >> 4) & 1) {
		aci_rds_cmd(RDS_TEXT, text_buffer, 65);
		text_buffer[0]  = ' ' ;
		text_buffer[65] = '\n';
		return copy_to_user(buffer+8, text_buffer,66) ? -EFAULT : 66+8;
	} else {
		put_user('\n', buffer+8);
		return 9;
	}
}

static struct file_operations rds_f_ops = {
	read:    rds_f_read,
	open:    rds_f_open,
	release: rds_f_release
};


static int __init miropcm20_rds_init(void)
{
	if ((dfsh = devfs_register(NULL, "v4l/rds/radiotext", 
				   DEVFS_FL_DEFAULT | DEVFS_FL_AUTO_DEVNUM,
				   0, 0, S_IRUGO | S_IFCHR, &rds_f_ops, NULL))
	    == NULL)
		goto devfs_register;

	printk("miropcm20-rds: userinterface driver loaded.\n");
#if DEBUG
	printk("v4l-name: %s\n", devfs_get_name(pcm20_radio.devfs_handle, 0));
#endif

	return 0;

 devfs_register:
	return -EINVAL;
}

static void __exit miropcm20_rds_cleanup(void)
{
	devfs_unregister(dfsh);
}

module_init(miropcm20_rds_init);
module_exit(miropcm20_rds_cleanup);
MODULE_LICENSE("GPL");
