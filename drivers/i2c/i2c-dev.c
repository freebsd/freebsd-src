/*
    i2c-dev.c - i2c-bus driver, char device interface  

    Copyright (C) 1995-97 Simon G. Vogl
    Copyright (C) 1998-99 Frodo Looijaard <frodol@dds.nl>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

/* Note that this is a complete rewrite of Simon Vogl's i2c-dev module.
   But I have used so much of his original code and ideas that it seems
   only fair to recognize him as co-author -- Frodo */

/* The I2C_RDWR ioctl code is written by Kolja Waschk <waschk@telos.de> */

/* The devfs code is contributed by Philipp Matthias Hahn 
   <pmhahn@titan.lahn.de> */

/* $Id: i2c-dev.c,v 1.40 2001/08/25 01:28:01 mds Exp $ */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/smp_lock.h>
#ifdef CONFIG_DEVFS_FS
#include <linux/devfs_fs_kernel.h>
#endif


/* If you want debugging uncomment: */
/* #define DEBUG */

#include <linux/init.h>
#include <asm/uaccess.h>

#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#ifdef MODULE
extern int init_module(void);
extern int cleanup_module(void);
#endif /* def MODULE */

/* struct file_operations changed too often in the 2.1 series for nice code */

static ssize_t i2cdev_read (struct file *file, char *buf, size_t count, 
                            loff_t *offset);
static ssize_t i2cdev_write (struct file *file, const char *buf, size_t count, 
                             loff_t *offset);

static int i2cdev_ioctl (struct inode *inode, struct file *file, 
                         unsigned int cmd, unsigned long arg);
static int i2cdev_open (struct inode *inode, struct file *file);

static int i2cdev_release (struct inode *inode, struct file *file);

static int i2cdev_attach_adapter(struct i2c_adapter *adap);
static int i2cdev_detach_client(struct i2c_client *client);
static int i2cdev_command(struct i2c_client *client, unsigned int cmd,
                           void *arg);

#ifdef MODULE
static
#else
extern
#endif
       int __init i2c_dev_init(void);
static int i2cdev_cleanup(void);

static struct file_operations i2cdev_fops = {
	owner:		THIS_MODULE,
	llseek:		no_llseek,
	read:		i2cdev_read,
	write:		i2cdev_write,
	ioctl:		i2cdev_ioctl,
	open:		i2cdev_open,
	release:	i2cdev_release,
};

#define I2CDEV_ADAPS_MAX I2C_ADAP_MAX
static struct i2c_adapter *i2cdev_adaps[I2CDEV_ADAPS_MAX];
#ifdef CONFIG_DEVFS_FS
static devfs_handle_t devfs_i2c[I2CDEV_ADAPS_MAX];
static devfs_handle_t devfs_handle = NULL;
#endif

static struct i2c_driver i2cdev_driver = {
	name:		"i2c-dev dummy driver",
	id:		I2C_DRIVERID_I2CDEV,
	flags:		I2C_DF_DUMMY,
	attach_adapter:	i2cdev_attach_adapter,
	detach_client:	i2cdev_detach_client,
	command:	i2cdev_command,
/*	inc_use:	NULL,
	dec_use:	NULL, */
};

static struct i2c_client i2cdev_client_template = {
	name:		"I2C /dev entry",
	id:		1,
	flags:		0,
	addr:		-1,
/*	adapter:	NULL, */
	driver:		&i2cdev_driver,
/*	data:		NULL */
};

static int i2cdev_initialized;

static ssize_t i2cdev_read (struct file *file, char *buf, size_t count,
                            loff_t *offset)
{
	char *tmp;
	int ret;

#ifdef DEBUG
	struct inode *inode = file->f_dentry->d_inode;
#endif /* DEBUG */

	struct i2c_client *client = (struct i2c_client *)file->private_data;

	if (count > 8192)
		count = 8192;

	/* copy user space data to kernel space. */
	tmp = kmalloc(count,GFP_KERNEL);
	if (tmp==NULL)
		return -ENOMEM;

#ifdef DEBUG
	printk(KERN_DEBUG "i2c-dev.o: i2c-%d reading %d bytes.\n",MINOR(inode->i_rdev),
	       count);
#endif

	ret = i2c_master_recv(client,tmp,count);
	if (ret >= 0)
		ret = copy_to_user(buf,tmp,count)?-EFAULT:ret;
	kfree(tmp);
	return ret;
}

static ssize_t i2cdev_write (struct file *file, const char *buf, size_t count,
                             loff_t *offset)
{
	int ret;
	char *tmp;
	struct i2c_client *client = (struct i2c_client *)file->private_data;

#ifdef DEBUG
	struct inode *inode = file->f_dentry->d_inode;
#endif /* DEBUG */

	if (count > 8192)
		count = 8192;

	/* copy user space data to kernel space. */
	tmp = kmalloc(count,GFP_KERNEL);
	if (tmp==NULL)
		return -ENOMEM;
	if (copy_from_user(tmp,buf,count)) {
		kfree(tmp);
		return -EFAULT;
	}

#ifdef DEBUG
	printk(KERN_DEBUG "i2c-dev.o: i2c-%d writing %d bytes.\n",MINOR(inode->i_rdev),
	       count);
#endif
	ret = i2c_master_send(client,tmp,count);
	kfree(tmp);
	return ret;
}

int i2cdev_ioctl (struct inode *inode, struct file *file, unsigned int cmd, 
                  unsigned long arg)
{
	struct i2c_client *client = (struct i2c_client *)file->private_data;
	struct i2c_rdwr_ioctl_data rdwr_arg;
	struct i2c_smbus_ioctl_data data_arg;
	union i2c_smbus_data temp;
	struct i2c_msg *rdwr_pa;
	u8 **data_ptrs;
	int i,datasize,res;
	unsigned long funcs;

#ifdef DEBUG
	printk(KERN_DEBUG "i2c-dev.o: i2c-%d ioctl, cmd: 0x%x, arg: %lx.\n", 
	       MINOR(inode->i_rdev),cmd, arg);
#endif /* DEBUG */

	switch ( cmd ) {
	case I2C_SLAVE:
	case I2C_SLAVE_FORCE:
		if ((arg > 0x3ff) || 
		    (((client->flags & I2C_M_TEN) == 0) && arg > 0x7f))
			return -EINVAL;
		if ((cmd == I2C_SLAVE) && i2c_check_addr(client->adapter,arg))
			return -EBUSY;
		client->addr = arg;
		return 0;
	case I2C_TENBIT:
		if (arg)
			client->flags |= I2C_M_TEN;
		else
			client->flags &= ~I2C_M_TEN;
		return 0;
	case I2C_FUNCS:
		funcs = i2c_get_functionality(client->adapter);
		return (copy_to_user((unsigned long *)arg,&funcs,
		                     sizeof(unsigned long)))?-EFAULT:0;

	case I2C_RDWR:
		if (copy_from_user(&rdwr_arg, 
				   (struct i2c_rdwr_ioctl_data *)arg, 
				   sizeof(rdwr_arg)))
			return -EFAULT;

		/* Put an arbritrary limit on the number of messages that can
		 * be sent at once */
		if (rdwr_arg.nmsgs > 42)
			return -EINVAL;
		
		rdwr_pa = (struct i2c_msg *)
			kmalloc(rdwr_arg.nmsgs * sizeof(struct i2c_msg), 
			GFP_KERNEL);

		if (rdwr_pa == NULL) return -ENOMEM;

		if (copy_from_user(rdwr_pa, rdwr_arg.msgs,
				   rdwr_arg.nmsgs * sizeof(struct i2c_msg))) {
			kfree(rdwr_pa);
			return -EFAULT;
		}

		data_ptrs = (u8 **) kmalloc(rdwr_arg.nmsgs * sizeof(u8 *),
					    GFP_KERNEL);
		if (data_ptrs == NULL) {
			kfree(rdwr_pa);
			return -ENOMEM;
		}

		res = 0;
		for( i=0; i<rdwr_arg.nmsgs; i++ )
		{
			/* Limit the size of the message to a sane amount */
			if (rdwr_pa[i].len > 8192) {
				res = -EINVAL;
				break;
			}
			data_ptrs[i] = rdwr_pa[i].buf;
			rdwr_pa[i].buf = kmalloc(rdwr_pa[i].len, GFP_KERNEL);
			if(rdwr_pa[i].buf == NULL)
			{
				res = -ENOMEM;
				break;
			}
			if(copy_from_user(rdwr_pa[i].buf,
				data_ptrs[i],
				rdwr_pa[i].len))
			{
				++i; /* Needs to be kfreed too */
				res = -EFAULT;
				break;
			}
		}
		if (res < 0) {
			int j;
			for (j = 0; j < i; ++j)
				kfree(rdwr_pa[j].buf);
			kfree(data_ptrs);
			kfree(rdwr_pa);
			return res;
		}

		res = i2c_transfer(client->adapter,
			rdwr_pa,
			rdwr_arg.nmsgs);
		while(i-- > 0)
		{
			if( res>=0 && (rdwr_pa[i].flags & I2C_M_RD))
			{
				if(copy_to_user(
					data_ptrs[i],
					rdwr_pa[i].buf,
					rdwr_pa[i].len))
				{
					res = -EFAULT;
				}
			}
			kfree(rdwr_pa[i].buf);
		}
		kfree(data_ptrs);
		kfree(rdwr_pa);
		return res;

	case I2C_SMBUS:
		if (copy_from_user(&data_arg,
		                   (struct i2c_smbus_ioctl_data *) arg,
		                   sizeof(struct i2c_smbus_ioctl_data)))
			return -EFAULT;
		if ((data_arg.size != I2C_SMBUS_BYTE) && 
		    (data_arg.size != I2C_SMBUS_QUICK) &&
		    (data_arg.size != I2C_SMBUS_BYTE_DATA) && 
		    (data_arg.size != I2C_SMBUS_WORD_DATA) &&
		    (data_arg.size != I2C_SMBUS_PROC_CALL) &&
		    (data_arg.size != I2C_SMBUS_BLOCK_DATA) &&
		    (data_arg.size != I2C_SMBUS_I2C_BLOCK_DATA)) {
#ifdef DEBUG
			printk(KERN_DEBUG "i2c-dev.o: size out of range (%x) in ioctl I2C_SMBUS.\n",
			       data_arg.size);
#endif
			return -EINVAL;
		}
		/* Note that I2C_SMBUS_READ and I2C_SMBUS_WRITE are 0 and 1, 
		   so the check is valid if size==I2C_SMBUS_QUICK too. */
		if ((data_arg.read_write != I2C_SMBUS_READ) && 
		    (data_arg.read_write != I2C_SMBUS_WRITE)) {
#ifdef DEBUG
			printk(KERN_DEBUG "i2c-dev.o: read_write out of range (%x) in ioctl I2C_SMBUS.\n",
			       data_arg.read_write);
#endif
			return -EINVAL;
		}

		/* Note that command values are always valid! */

		if ((data_arg.size == I2C_SMBUS_QUICK) ||
		    ((data_arg.size == I2C_SMBUS_BYTE) && 
		    (data_arg.read_write == I2C_SMBUS_WRITE)))
			/* These are special: we do not use data */
			return i2c_smbus_xfer(client->adapter, client->addr,
			                      client->flags,
			                      data_arg.read_write,
			                      data_arg.command,
			                      data_arg.size, NULL);

		if (data_arg.data == NULL) {
#ifdef DEBUG
			printk(KERN_DEBUG "i2c-dev.o: data is NULL pointer in ioctl I2C_SMBUS.\n");
#endif
			return -EINVAL;
		}

		if ((data_arg.size == I2C_SMBUS_BYTE_DATA) ||
		    (data_arg.size == I2C_SMBUS_BYTE))
			datasize = sizeof(data_arg.data->byte);
		else if ((data_arg.size == I2C_SMBUS_WORD_DATA) || 
		         (data_arg.size == I2C_SMBUS_PROC_CALL))
			datasize = sizeof(data_arg.data->word);
		else /* size == I2C_SMBUS_BLOCK_DATA */
			datasize = sizeof(data_arg.data->block);

		if ((data_arg.size == I2C_SMBUS_PROC_CALL) || 
		    (data_arg.read_write == I2C_SMBUS_WRITE)) {
			if (copy_from_user(&temp, data_arg.data, datasize))
				return -EFAULT;
		}
		res = i2c_smbus_xfer(client->adapter,client->addr,client->flags,
		      data_arg.read_write,
		      data_arg.command,data_arg.size,&temp);
		if (! res && ((data_arg.size == I2C_SMBUS_PROC_CALL) || 
			      (data_arg.read_write == I2C_SMBUS_READ))) {
			if (copy_to_user(data_arg.data, &temp, datasize))
				return -EFAULT;
		}
		return res;

	default:
		return i2c_control(client,cmd,arg);
	}
	return 0;
}

int i2cdev_open (struct inode *inode, struct file *file)
{
	unsigned int minor = MINOR(inode->i_rdev);
	struct i2c_client *client;

	if ((minor >= I2CDEV_ADAPS_MAX) || ! (i2cdev_adaps[minor])) {
#ifdef DEBUG
		printk(KERN_DEBUG "i2c-dev.o: Trying to open unattached adapter i2c-%d\n",
		       minor);
#endif
		return -ENODEV;
	}

	/* Note that we here allocate a client for later use, but we will *not*
	   register this client! Yes, this is safe. No, it is not very clean. */
	if(! (client = kmalloc(sizeof(struct i2c_client),GFP_KERNEL)))
		return -ENOMEM;
	memcpy(client,&i2cdev_client_template,sizeof(struct i2c_client));
	client->adapter = i2cdev_adaps[minor];
	file->private_data = client;

	if (i2cdev_adaps[minor]->inc_use)
		i2cdev_adaps[minor]->inc_use(i2cdev_adaps[minor]);

#ifdef DEBUG
	printk(KERN_DEBUG "i2c-dev.o: opened i2c-%d\n",minor);
#endif
	return 0;
}

static int i2cdev_release (struct inode *inode, struct file *file)
{
	unsigned int minor = MINOR(inode->i_rdev);
	kfree(file->private_data);
	file->private_data=NULL;
#ifdef DEBUG
	printk(KERN_DEBUG "i2c-dev.o: Closed: i2c-%d\n", minor);
#endif
	lock_kernel();
	if (i2cdev_adaps[minor]->dec_use)
		i2cdev_adaps[minor]->dec_use(i2cdev_adaps[minor]);
	unlock_kernel();
	return 0;
}

int i2cdev_attach_adapter(struct i2c_adapter *adap)
{
	int i;
	char name[8];

	if ((i = i2c_adapter_id(adap)) < 0) {
		printk(KERN_DEBUG "i2c-dev.o: Unknown adapter ?!?\n");
		return -ENODEV;
	}
	if (i >= I2CDEV_ADAPS_MAX) {
		printk(KERN_DEBUG "i2c-dev.o: Adapter number too large?!? (%d)\n",i);
		return -ENODEV;
	}

	sprintf (name, "%d", i);
	if (! i2cdev_adaps[i]) {
		i2cdev_adaps[i] = adap;
#ifdef CONFIG_DEVFS_FS
		devfs_i2c[i] = devfs_register (devfs_handle, name,
			DEVFS_FL_DEFAULT, I2C_MAJOR, i,
			S_IFCHR | S_IRUSR | S_IWUSR,
			&i2cdev_fops, NULL);
#endif
		printk(KERN_DEBUG "i2c-dev.o: Registered '%s' as minor %d\n",adap->name,i);
	} else {
		/* This is actually a detach_adapter call! */
#ifdef CONFIG_DEVFS_FS
		devfs_unregister(devfs_i2c[i]);
#endif
		i2cdev_adaps[i] = NULL;
#ifdef DEBUG
		printk(KERN_DEBUG "i2c-dev.o: Adapter unregistered: %s\n",adap->name);
#endif
	}

	return 0;
}

int i2cdev_detach_client(struct i2c_client *client)
{
	return 0;
}

static int i2cdev_command(struct i2c_client *client, unsigned int cmd,
                           void *arg)
{
	return -1;
}

int __init i2c_dev_init(void)
{
	int res;

	printk(KERN_INFO "i2c-dev.o: i2c /dev entries driver module version %s (%s)\n", I2C_VERSION, I2C_DATE);

	i2cdev_initialized = 0;
#ifdef CONFIG_DEVFS_FS
	if (devfs_register_chrdev(I2C_MAJOR, "i2c", &i2cdev_fops)) {
#else
	if (register_chrdev(I2C_MAJOR,"i2c",&i2cdev_fops)) {
#endif
		printk(KERN_ERR "i2c-dev.o: unable to get major %d for i2c bus\n",
		       I2C_MAJOR);
		return -EIO;
	}
#ifdef CONFIG_DEVFS_FS
	devfs_handle = devfs_mk_dir(NULL, "i2c", NULL);
#endif
	i2cdev_initialized ++;

	if ((res = i2c_add_driver(&i2cdev_driver))) {
		printk(KERN_ERR "i2c-dev.o: Driver registration failed, module not inserted.\n");
		i2cdev_cleanup();
		return res;
	}
	i2cdev_initialized ++;
	return 0;
}

int i2cdev_cleanup(void)
{
	int res;

	if (i2cdev_initialized >= 2) {
		if ((res = i2c_del_driver(&i2cdev_driver))) {
			printk("i2c-dev.o: Driver deregistration failed, "
			       "module not removed.\n");
			return res;
		}
	i2cdev_initialized --;
	}

	if (i2cdev_initialized >= 1) {
#ifdef CONFIG_DEVFS_FS
		devfs_unregister(devfs_handle);
		if ((res = devfs_unregister_chrdev(I2C_MAJOR, "i2c"))) {
#else
		if ((res = unregister_chrdev(I2C_MAJOR,"i2c"))) {
#endif
			printk("i2c-dev.o: unable to release major %d for i2c bus\n",
			       I2C_MAJOR);
			return res;
		}
		i2cdev_initialized --;
	}
	return 0;
}

EXPORT_NO_SYMBOLS;

#ifdef MODULE

MODULE_AUTHOR("Frodo Looijaard <frodol@dds.nl> and Simon G. Vogl <simon@tk.uni-linz.ac.at>");
MODULE_DESCRIPTION("I2C /dev entries driver");
MODULE_LICENSE("GPL");

int init_module(void)
{
	return i2c_dev_init();
}

int cleanup_module(void)
{
	return i2cdev_cleanup();
}

#endif /* def MODULE */

