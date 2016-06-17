/*
 * linux/fs/ext3/ioctl.c
 *
 * Copyright (C) 1993, 1994, 1995
 * Remy Card (card@masi.ibp.fr)
 * Laboratoire MASI - Institut Blaise Pascal
 * Universite Pierre et Marie Curie (Paris VI)
 */

#include <linux/fs.h>
#include <linux/jbd.h>
#include <linux/ext3_fs.h>
#include <linux/ext3_jbd.h>
#include <linux/sched.h>
#include <asm/uaccess.h>


int ext3_ioctl (struct inode * inode, struct file * filp, unsigned int cmd,
		unsigned long arg)
{
	unsigned int flags;

	ext3_debug ("cmd = %u, arg = %lu\n", cmd, arg);

	switch (cmd) {
	case EXT3_IOC_GETFLAGS:
		flags = inode->u.ext3_i.i_flags & EXT3_FL_USER_VISIBLE;
		return put_user(flags, (int *) arg);
	case EXT3_IOC_SETFLAGS: {
		handle_t *handle = NULL;
		int err;
		struct ext3_iloc iloc;
		unsigned int oldflags;
		unsigned int jflag;

		if (IS_RDONLY(inode))
			return -EROFS;

		if ((current->fsuid != inode->i_uid) && !capable(CAP_FOWNER))
			return -EACCES;

		if (get_user(flags, (int *) arg))
			return -EFAULT;

		oldflags = inode->u.ext3_i.i_flags;

		/* The JOURNAL_DATA flag is modifiable only by root */
		jflag = flags & EXT3_JOURNAL_DATA_FL;

		/*
		 * The IMMUTABLE and APPEND_ONLY flags can only be changed by
		 * the relevant capability.
		 *
		 * This test looks nicer. Thanks to Pauline Middelink
		 */
		if ((flags ^ oldflags) & (EXT3_APPEND_FL | EXT3_IMMUTABLE_FL)) {
			if (!capable(CAP_LINUX_IMMUTABLE))
				return -EPERM;
		}
		
		/*
		 * The JOURNAL_DATA flag can only be changed by
		 * the relevant capability.
		 */
		if ((jflag ^ oldflags) & (EXT3_JOURNAL_DATA_FL)) {
			if (!capable(CAP_SYS_RESOURCE))
				return -EPERM;
		}


		handle = ext3_journal_start(inode, 1);
		if (IS_ERR(handle))
			return PTR_ERR(handle);
		if (IS_SYNC(inode))
			handle->h_sync = 1;
		err = ext3_reserve_inode_write(handle, inode, &iloc);
		if (err)
			goto flags_err;
		
		flags = flags & EXT3_FL_USER_MODIFIABLE;
		flags |= oldflags & ~EXT3_FL_USER_MODIFIABLE;
		inode->u.ext3_i.i_flags = flags;

		ext3_set_inode_flags(inode);
		inode->i_ctime = CURRENT_TIME;

		err = ext3_mark_iloc_dirty(handle, inode, &iloc);
flags_err:
		ext3_journal_stop(handle, inode);
		if (err)
			return err;
		
		if ((jflag ^ oldflags) & (EXT3_JOURNAL_DATA_FL))
			err = ext3_change_inode_journal_flag(inode, jflag);
		return err;
	}
	case EXT3_IOC_GETVERSION:
	case EXT3_IOC_GETVERSION_OLD:
		return put_user(inode->i_generation, (int *) arg);
	case EXT3_IOC_SETVERSION:
	case EXT3_IOC_SETVERSION_OLD: {
		handle_t *handle;
		struct ext3_iloc iloc;
		__u32 generation;
		int err;

		if ((current->fsuid != inode->i_uid) && !capable(CAP_FOWNER))
			return -EPERM;
		if (IS_RDONLY(inode))
			return -EROFS;
		if (get_user(generation, (int *) arg))
			return -EFAULT;

		handle = ext3_journal_start(inode, 1);
		if (IS_ERR(handle))
			return PTR_ERR(handle);
		err = ext3_reserve_inode_write(handle, inode, &iloc);
		if (err)
			return err;

		inode->i_ctime = CURRENT_TIME;
		inode->i_generation = generation;

		err = ext3_mark_iloc_dirty(handle, inode, &iloc);
		ext3_journal_stop(handle, inode);
		return err;
	}
#ifdef CONFIG_JBD_DEBUG
	case EXT3_IOC_WAIT_FOR_READONLY:
		/*
		 * This is racy - by the time we're woken up and running,
		 * the superblock could be released.  And the module could
		 * have been unloaded.  So sue me.
		 *
		 * Returns 1 if it slept, else zero.
		 */
		{
			struct super_block *sb = inode->i_sb;
			DECLARE_WAITQUEUE(wait, current);
			int ret = 0;

			set_current_state(TASK_INTERRUPTIBLE);
			add_wait_queue(&sb->u.ext3_sb.ro_wait_queue, &wait);
			if (timer_pending(&sb->u.ext3_sb.turn_ro_timer)) {
				schedule();
				ret = 1;
			}
			remove_wait_queue(&sb->u.ext3_sb.ro_wait_queue, &wait);
			return ret;
		}
#endif
	default:
		return -ENOTTY;
	}
}
