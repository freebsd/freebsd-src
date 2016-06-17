/*
 * Pioctl operations for Coda.
 * Original version: (C) 1996 Peter Braam 
 * Rewritten for Linux 2.1: (C) 1997 Carnegie Mellon University
 *
 * Carnegie Mellon encourages users of this code to contribute improvements
 * to the Coda project. Contact Peter Braam <coda@cs.cmu.edu>.
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/stat.h>
#include <linux/errno.h>
#include <linux/locks.h>
#include <linux/string.h>
#define __NO_VERSION__
#include <linux/module.h>
#include <asm/uaccess.h>

#include <linux/coda.h>
#include <linux/coda_linux.h>
#include <linux/coda_fs_i.h>
#include <linux/coda_psdev.h>

/* pioctl ops */
static int coda_ioctl_permission(struct inode *inode, int mask);
static int coda_pioctl(struct inode * inode, struct file * filp, 
                       unsigned int cmd, unsigned long user_data);

/* exported from this file */
struct inode_operations coda_ioctl_inode_operations =
{
	permission:	coda_ioctl_permission,
	setattr:	coda_notify_change,
};

struct file_operations coda_ioctl_operations = {
	owner:		THIS_MODULE,
	ioctl:		coda_pioctl,
};

/* the coda pioctl inode ops */
static int coda_ioctl_permission(struct inode *inode, int mask)
{
        return 0;
}

static int coda_pioctl(struct inode * inode, struct file * filp, 
                       unsigned int cmd, unsigned long user_data)
{
	struct nameidata nd;
        int error;
	struct PioctlData data;
        struct inode *target_inode = NULL;
        struct coda_inode_info *cnp;

        /* get the Pioctl data arguments from user space */
        if (copy_from_user(&data, (int *)user_data, sizeof(data))) {
	    return -EINVAL;
	}
       
        /* 
         * Look up the pathname. Note that the pathname is in 
         * user memory, and namei takes care of this
         */
	CDEBUG(D_PIOCTL, "namei, data.follow = %d\n", 
	       data.follow);
        if ( data.follow ) {
                error = user_path_walk(data.path, &nd);
	} else {
	        error = user_path_walk_link(data.path, &nd);
	}
		
	if ( error ) {
                CDEBUG(D_PIOCTL, "error: lookup fails.\n");
		return error;
        } else {
	        target_inode = nd.dentry->d_inode;
	}
	
	CDEBUG(D_PIOCTL, "target ino: 0x%ld, dev: 0x%x\n",
	       target_inode->i_ino, target_inode->i_dev);

	/* return if it is not a Coda inode */
	if ( target_inode->i_sb != inode->i_sb ) {
		path_release(&nd);
	        return  -EINVAL;
	}

	/* now proceed to make the upcall */
        cnp = ITOC(target_inode);

	error = venus_pioctl(inode->i_sb, &(cnp->c_fid), cmd, &data);

        CDEBUG(D_PIOCTL, "ioctl on inode %ld\n", target_inode->i_ino);
	CDEBUG(D_DOWNCALL, "dput on ino: %ld, icount %d, dcount %d\n", target_inode->i_ino, 
	       atomic_read(&target_inode->i_count), atomic_read(&nd.dentry->d_count));
	path_release(&nd);
        return error;
}

