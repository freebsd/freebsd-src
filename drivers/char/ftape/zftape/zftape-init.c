/*
 *      Copyright (C) 1996, 1997 Claus-Justus Heine.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2, or (at your option)
 any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; see the file COPYING.  If not, write to
 the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

 *
 *      This file contains the code that registers the zftape frontend 
 *      to the ftape floppy tape driver for Linux
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <asm/segment.h>
#include <linux/kernel.h>
#include <linux/signal.h>
#include <linux/major.h>
#include <linux/slab.h>
#ifdef CONFIG_KMOD
#include <linux/kmod.h>
#endif
#include <linux/fcntl.h>
#include <linux/wrapper.h>
#include <linux/smp_lock.h>
#include <linux/devfs_fs_kernel.h>

#include <linux/zftape.h>
#include <linux/init.h>

#include "../zftape/zftape-init.h"
#include "../zftape/zftape-read.h"
#include "../zftape/zftape-write.h"
#include "../zftape/zftape-ctl.h"
#include "../zftape/zftape-buffers.h"
#include "../zftape/zftape_syms.h"

char zft_src[] __initdata = "$Source: /homes/cvs/ftape-stacked/ftape/zftape/zftape-init.c,v $";
char zft_rev[] __initdata = "$Revision: 1.8 $";
char zft_dat[] __initdata = "$Date: 1997/11/06 00:48:56 $";

MODULE_AUTHOR("(c) 1996, 1997 Claus-Justus Heine "
	      "(claus@momo.math.rwth-aachen.de)");
MODULE_DESCRIPTION(ZFTAPE_VERSION " - "
		   "VFS interface for the Linux floppy tape driver. "
		   "Support for QIC-113 compatible volume table "
		   "and builtin compression (lzrw3 algorithm)");
MODULE_SUPPORTED_DEVICE("char-major-27");
MODULE_LICENSE("GPL");

/*      Global vars.
 */
struct zft_cmpr_ops *zft_cmpr_ops = NULL;
const ftape_info *zft_status;

/*      Local vars.
 */
static int busy_flag = 0;
static sigset_t orig_sigmask;

/*  the interface to the kernel vfs layer
 */

/* Note about llseek():
 *
 * st.c and tpqic.c update fp->f_pos but don't implment llseek() and
 * initialize the llseek component of the file_ops struct with NULL.
 * This means that the user will get the default seek, but the tape
 * device will not respect the new position, but happily read from the
 * old position. Think a zftape specific llseek() function would be
 * better, returning -ESPIPE. TODO.
 */

static int  zft_open (struct inode *ino, struct file *filep);
static int zft_close(struct inode *ino, struct file *filep);
static int  zft_ioctl(struct inode *ino, struct file *filep,
		      unsigned int command, unsigned long arg);
static int  zft_mmap(struct file *filep, struct vm_area_struct *vma);
static ssize_t zft_read (struct file *fp, char *buff,
			 size_t req_len, loff_t *ppos);
static ssize_t zft_write(struct file *fp, const char *buff,
			 size_t req_len, loff_t *ppos);

static struct file_operations zft_cdev =
{
	owner:		THIS_MODULE,
	read:		zft_read,
	write:		zft_write,
	ioctl:		zft_ioctl,
	mmap:		zft_mmap,
	open:		zft_open,
	release:	zft_close,
};

/*      Open floppy tape device
 */
static int zft_open(struct inode *ino, struct file *filep)
{
	int result;
	TRACE_FUN(ft_t_flow);

	TRACE(ft_t_flow, "called for minor %d", MINOR(ino->i_rdev));
	if (busy_flag) {
		TRACE_ABORT(-EBUSY, ft_t_warn, "failed: already busy");
	}
	busy_flag = 1;
	if ((MINOR(ino->i_rdev) & ~(ZFT_MINOR_OP_MASK | FTAPE_NO_REWIND))
	     > 
	    FTAPE_SEL_D) {
		busy_flag = 0;
		TRACE_ABORT(-ENXIO, ft_t_err, "failed: illegal unit nr");
	}
	orig_sigmask = current->blocked;
	sigfillset(&current->blocked);
	result = _zft_open(MINOR(ino->i_rdev), filep->f_flags & O_ACCMODE);
	if (result < 0) {
		current->blocked = orig_sigmask; /* restore mask */
		busy_flag = 0;
		TRACE_ABORT(result, ft_t_err, "_ftape_open failed");
	} else {
		/* Mask signals that will disturb proper operation of the
		 * program that is calling.
		 */
		current->blocked = orig_sigmask;
		sigaddsetmask (&current->blocked, _DO_BLOCK);
		TRACE_EXIT 0;
	}
}

/*      Close floppy tape device
 */
static int zft_close(struct inode *ino, struct file *filep)
{
	int result;
	TRACE_FUN(ft_t_flow);

	lock_kernel();
	if (!busy_flag || MINOR(ino->i_rdev) != zft_unit) {
		TRACE(ft_t_err, "failed: not busy or wrong unit");
		unlock_kernel();
		TRACE_EXIT 0;
	}
	sigfillset(&current->blocked);
	result = _zft_close();
	if (result < 0) {
		TRACE(ft_t_err, "_zft_close failed");
	}
	current->blocked = orig_sigmask; /* restore before open state */
	busy_flag = 0;
	unlock_kernel();
	TRACE_EXIT 0;
}

/*      Ioctl for floppy tape device
 */
static int zft_ioctl(struct inode *ino, struct file *filep,
		     unsigned int command, unsigned long arg)
{
	int result = -EIO;
	sigset_t old_sigmask;
	TRACE_FUN(ft_t_flow);

	if (!busy_flag || MINOR(ino->i_rdev) != zft_unit || ft_failure) {
		TRACE_ABORT(-EIO, ft_t_err,
			    "failed: not busy, failure or wrong unit");
	}
	old_sigmask = current->blocked; /* save mask */
	sigfillset(&current->blocked);
	/* This will work as long as sizeof(void *) == sizeof(long) */
	result = _zft_ioctl(command, (void *) arg);
	current->blocked = old_sigmask; /* restore mask */
	TRACE_EXIT result;
}

/*      Ioctl for floppy tape device
 */
static int  zft_mmap(struct file *filep, struct vm_area_struct *vma)
{
	int result = -EIO;
	sigset_t old_sigmask;
	TRACE_FUN(ft_t_flow);

	if (!busy_flag || 
	    MINOR(filep->f_dentry->d_inode->i_rdev) != zft_unit || 
	    ft_failure)
	{
		TRACE_ABORT(-EIO, ft_t_err,
			    "failed: not busy, failure or wrong unit");
	}
	old_sigmask = current->blocked; /* save mask */
	sigfillset(&current->blocked);
	lock_kernel();
	if ((result = ftape_mmap(vma)) >= 0) {
#ifndef MSYNC_BUG_WAS_FIXED
		static struct vm_operations_struct dummy = { NULL, };
		vma->vm_ops = &dummy;
#endif
	}
	unlock_kernel();
	current->blocked = old_sigmask; /* restore mask */
	TRACE_EXIT result;
}

/*      Read from floppy tape device
 */
static ssize_t zft_read(struct file *fp, char *buff,
			size_t req_len, loff_t *ppos)
{
	int result = -EIO;
	sigset_t old_sigmask;
	struct inode *ino = fp->f_dentry->d_inode;
	TRACE_FUN(ft_t_flow);

	TRACE(ft_t_data_flow, "called with count: %ld", (unsigned long)req_len);
	if (!busy_flag || MINOR(ino->i_rdev) != zft_unit || ft_failure) {
		TRACE_ABORT(-EIO, ft_t_err,
			    "failed: not busy, failure or wrong unit");
	}
	old_sigmask = current->blocked; /* save mask */
	sigfillset(&current->blocked);
	result = _zft_read(buff, req_len);
	current->blocked = old_sigmask; /* restore mask */
	TRACE(ft_t_data_flow, "return with count: %d", result);
	TRACE_EXIT result;
}

/*      Write to tape device
 */
static ssize_t zft_write(struct file *fp, const char *buff,
			 size_t req_len, loff_t *ppos)
{
	int result = -EIO;
	sigset_t old_sigmask;
	struct inode *ino = fp->f_dentry->d_inode;
	TRACE_FUN(ft_t_flow);

	TRACE(ft_t_flow, "called with count: %ld", (unsigned long)req_len);
	if (!busy_flag || MINOR(ino->i_rdev) != zft_unit || ft_failure) {
		TRACE_ABORT(-EIO, ft_t_err,
			    "failed: not busy, failure or wrong unit");
	}
	old_sigmask = current->blocked; /* save mask */
	sigfillset(&current->blocked);
	result = _zft_write(buff, req_len);
	current->blocked = old_sigmask; /* restore mask */
	TRACE(ft_t_data_flow, "return with count: %d", result);
	TRACE_EXIT result;
}

/*                    END OF VFS INTERFACE 
 *          
 *****************************************************************************/

/*  driver/module initialization
 */

/*  the compression module has to call this function to hook into the zftape 
 *  code
 */
int zft_cmpr_register(struct zft_cmpr_ops *new_ops)
{
	TRACE_FUN(ft_t_flow);
	
	if (zft_cmpr_ops != NULL) {
		TRACE_EXIT -EBUSY;
	} else {
		zft_cmpr_ops = new_ops;
		TRACE_EXIT 0;
	}
}

struct zft_cmpr_ops *zft_cmpr_unregister(void)
{
	struct zft_cmpr_ops *old_ops = zft_cmpr_ops;
	TRACE_FUN(ft_t_flow);

	zft_cmpr_ops = NULL;
	TRACE_EXIT old_ops;
}

/*  lock the zft-compressor() module.
 */
int zft_cmpr_lock(int try_to_load)
{
	if (zft_cmpr_ops == NULL) {
#ifdef CONFIG_KMOD
		if (try_to_load) {
			request_module("zft-compressor");
			if (zft_cmpr_ops == NULL) {
				return -ENOSYS;
			}
		} else {
			return -ENOSYS;
		}
#else
		return -ENOSYS;
#endif
	}
	(*zft_cmpr_ops->lock)();
	return 0;
}

#ifdef CONFIG_ZFT_COMPRESSOR
extern int zft_compressor_init(void);
#endif

/*  Called by modules package when installing the driver or by kernel
 *  during the initialization phase
 */
int __init zft_init(void)
{
	int i;
	TRACE_FUN(ft_t_flow);

#ifdef MODULE
	printk(KERN_INFO ZFTAPE_VERSION "\n");
        if (TRACE_LEVEL >= ft_t_info) {
		printk(
KERN_INFO
"(c) 1996, 1997 Claus-Justus Heine (claus@momo.math.rwth-aachen.de)\n"
KERN_INFO
"vfs interface for ftape floppy tape driver.\n"
KERN_INFO
"Support for QIC-113 compatible volume table, dynamic memory allocation\n"
KERN_INFO
"and builtin compression (lzrw3 algorithm).\n"
KERN_INFO
"Compiled for Linux version %s"
#ifdef MODVERSIONS
		       " with versioned symbols"
#endif
		       "\n", UTS_RELEASE);
        }
#else /* !MODULE */
	/* print a short no-nonsense boot message */
	printk(KERN_INFO ZFTAPE_VERSION " for Linux " UTS_RELEASE "\n");
#endif /* MODULE */
	TRACE(ft_t_info, "zft_init @ 0x%p", zft_init);
	TRACE(ft_t_info,
	      "installing zftape VFS interface for ftape driver ...");
	TRACE_CATCH(devfs_register_chrdev(QIC117_TAPE_MAJOR, "zft", &zft_cdev),);

	for (i = 0; i < 4; i++) {
		char devname[9];

		sprintf (devname, "qft%i", i);
		devfs_register (NULL, devname, DEVFS_FL_DEFAULT,
			        QIC117_TAPE_MAJOR, i,
				S_IFCHR | S_IRUSR | S_IWUSR,
				&zft_cdev, NULL);
		sprintf (devname, "nqft%i", i);
		devfs_register (NULL, devname, DEVFS_FL_DEFAULT,
				QIC117_TAPE_MAJOR, i + 4,
				S_IFCHR | S_IRUSR | S_IWUSR,
				&zft_cdev, NULL);
		sprintf (devname, "zqft%i", i);
		devfs_register (NULL, devname, DEVFS_FL_DEFAULT,
				QIC117_TAPE_MAJOR, i + 16,
				S_IFCHR | S_IRUSR | S_IWUSR,
				&zft_cdev, NULL);
		sprintf (devname, "nzqft%i", i);
		devfs_register (NULL, devname, DEVFS_FL_DEFAULT,
				QIC117_TAPE_MAJOR, i + 20,
				S_IFCHR | S_IRUSR | S_IWUSR,
				&zft_cdev, NULL);
		sprintf (devname, "rawqft%i", i);
		devfs_register (NULL, devname, DEVFS_FL_DEFAULT,
				QIC117_TAPE_MAJOR, i + 32,
				S_IFCHR | S_IRUSR | S_IWUSR,
				&zft_cdev, NULL);
		sprintf (devname, "nrawqft%i", i);
		devfs_register (NULL, devname, DEVFS_FL_DEFAULT,
				QIC117_TAPE_MAJOR, i + 36,
				S_IFCHR | S_IRUSR | S_IWUSR,
				&zft_cdev, NULL);
	}

#ifdef CONFIG_ZFT_COMPRESSOR
	(void)zft_compressor_init();
#endif
	zft_status = ftape_get_status(); /*  fetch global data of ftape 
					  *  hardware driver 
					  */
	TRACE_EXIT 0;
}


#ifdef MODULE
/* Called by modules package before trying to unload the module
 */
static int can_unload(void)
{
	return (GET_USE_COUNT(THIS_MODULE)||zft_dirty()||busy_flag)?-EBUSY:0;
}
/* Called by modules package when installing the driver
 */
int init_module(void)
{
	if (!mod_member_present(&__this_module, can_unload)) {
		return -EBUSY;
	}
	__this_module.can_unload = can_unload;
	return zft_init();
}

/* Called by modules package when removing the driver 
 */
void cleanup_module(void)
{
	int i;
	char devname[9];

	TRACE_FUN(ft_t_flow);

	if (devfs_unregister_chrdev(QIC117_TAPE_MAJOR, "zft") != 0) {
		TRACE(ft_t_warn, "failed");
	} else {
		TRACE(ft_t_info, "successful");
	}
        for (i = 0; i < 4; i++) {
		sprintf(devname, "qft%i", i);
		devfs_unregister(devfs_find_handle(NULL, devname, QIC117_TAPE_MAJOR, i, DEVFS_SPECIAL_CHR, 0));
		sprintf(devname, "nqft%i", i);
		devfs_unregister(devfs_find_handle(NULL, devname, QIC117_TAPE_MAJOR, i + 4, DEVFS_SPECIAL_CHR, 0));
		sprintf(devname, "zqft%i", i);
		devfs_unregister(devfs_find_handle(NULL, devname, QIC117_TAPE_MAJOR, i + 16, DEVFS_SPECIAL_CHR, 0));
		sprintf(devname, "nzqft%i", i);
		devfs_unregister(devfs_find_handle(NULL, devname, QIC117_TAPE_MAJOR, i + 20, DEVFS_SPECIAL_CHR, 0));
		sprintf(devname, "rawqft%i", i);
		devfs_unregister(devfs_find_handle(NULL, devname, QIC117_TAPE_MAJOR, i + 32, DEVFS_SPECIAL_CHR, 0));
		sprintf(devname, "nrawqft%i", i);
		devfs_unregister(devfs_find_handle(NULL, devname, QIC117_TAPE_MAJOR, i + 36, DEVFS_SPECIAL_CHR, 0));
	}
	zft_uninit_mem(); /* release remaining memory, if any */
        printk(KERN_INFO "zftape successfully unloaded.\n");
	TRACE_EXIT;
}

#endif /* MODULE */
