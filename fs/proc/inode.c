/*
 *  linux/fs/proc/inode.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

#include <linux/sched.h>
#include <linux/proc_fs.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/stat.h>
#include <linux/file.h>
#include <linux/locks.h>
#include <linux/limits.h>
#define __NO_VERSION__
#include <linux/module.h>
#include <linux/smp_lock.h>

#include <asm/system.h>
#include <asm/uaccess.h>

extern void free_proc_entry(struct proc_dir_entry *);

static inline struct proc_dir_entry * de_get(struct proc_dir_entry *de)
{
	if (de)
		atomic_inc(&de->count);
	return de;
}

/*
 * Decrements the use count and checks for deferred deletion.
 */
static void de_put(struct proc_dir_entry *de)
{
	if (de) {	
		lock_kernel();		
		if (!atomic_read(&de->count)) {
			printk("de_put: entry %s already free!\n", de->name);
			unlock_kernel();
			return;
		}

		if (atomic_dec_and_test(&de->count)) {
			if (de->deleted) {
				printk("de_put: deferred delete of %s\n",
					de->name);
				free_proc_entry(de);
			}
		}		
		unlock_kernel();
	}
}

/*
 * Decrement the use count of the proc_dir_entry.
 */
static void proc_delete_inode(struct inode *inode)
{
	struct proc_dir_entry *de = inode->u.generic_ip;

	inode->i_state = I_CLEAR;

	if (PROC_INODE_PROPER(inode)) {
		proc_pid_delete_inode(inode);
		return;
	}
	if (de) {
		if (de->owner)
			__MOD_DEC_USE_COUNT(de->owner);
		de_put(de);
	}
}

struct vfsmount *proc_mnt;

static void proc_read_inode(struct inode * inode)
{
	inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME;
}

static int proc_statfs(struct super_block *sb, struct statfs *buf)
{
	buf->f_type = PROC_SUPER_MAGIC;
	buf->f_bsize = PAGE_SIZE/sizeof(long);
	buf->f_bfree = 0;
	buf->f_bavail = 0;
	buf->f_ffree = 0;
	buf->f_namelen = NAME_MAX;
	return 0;
}

static struct super_operations proc_sops = { 
	read_inode:	proc_read_inode,
	put_inode:	force_delete,
	delete_inode:	proc_delete_inode,
	statfs:		proc_statfs,
};


static int parse_options(char *options,uid_t *uid,gid_t *gid)
{
	char *this_char,*value;

	*uid = current->uid;
	*gid = current->gid;
	if (!options) return 1;
	for (this_char = strtok(options,","); this_char; this_char = strtok(NULL,",")) {
		if ((value = strchr(this_char,'=')) != NULL)
			*value++ = 0;
		if (!strcmp(this_char,"uid")) {
			if (!value || !*value)
				return 0;
			*uid = simple_strtoul(value,&value,0);
			if (*value)
				return 0;
		}
		else if (!strcmp(this_char,"gid")) {
			if (!value || !*value)
				return 0;
			*gid = simple_strtoul(value,&value,0);
			if (*value)
				return 0;
		}
		else return 1;
	}
	return 1;
}

struct inode * proc_get_inode(struct super_block * sb, int ino,
				struct proc_dir_entry * de)
{
	struct inode * inode;

	/*
	 * Increment the use count so the dir entry can't disappear.
	 */
	de_get(de);
#if 1
/* shouldn't ever happen */
if (de && de->deleted)
printk("proc_iget: using deleted entry %s, count=%d\n", de->name, atomic_read(&de->count));
#endif

	inode = iget(sb, ino);
	if (!inode)
		goto out_fail;
	
	inode->u.generic_ip = (void *) de;
	if (de) {
		if (de->mode) {
			inode->i_mode = de->mode;
			inode->i_uid = de->uid;
			inode->i_gid = de->gid;
		}
		if (de->size)
			inode->i_size = de->size;
		if (de->nlink)
			inode->i_nlink = de->nlink;
		if (de->owner)
			__MOD_INC_USE_COUNT(de->owner);
		if (de->proc_iops)
			inode->i_op = de->proc_iops;
		if (de->proc_fops)
			inode->i_fop = de->proc_fops;
		else if (S_ISBLK(de->mode)||S_ISCHR(de->mode)||S_ISFIFO(de->mode))
			init_special_inode(inode,de->mode,kdev_t_to_nr(de->rdev));
	}

out:
	return inode;

out_fail:
	de_put(de);
	goto out;
}			

struct super_block *proc_read_super(struct super_block *s,void *data, 
				    int silent)
{
	struct inode * root_inode;
	struct task_struct *p;

	s->s_blocksize = 1024;
	s->s_blocksize_bits = 10;
	s->s_magic = PROC_SUPER_MAGIC;
	s->s_op = &proc_sops;
	s->s_maxbytes = ~0UL;
	
	root_inode = proc_get_inode(s, PROC_ROOT_INO, &proc_root);
	if (!root_inode)
		goto out_no_root;
	/*
	 * Fixup the root inode's nlink value
	 */
	read_lock(&tasklist_lock);
	for_each_task(p) if (p->pid) root_inode->i_nlink++;
	read_unlock(&tasklist_lock);
	s->s_root = d_alloc_root(root_inode);
	if (!s->s_root)
		goto out_no_root;
	parse_options(data, &root_inode->i_uid, &root_inode->i_gid);
	return s;

out_no_root:
	printk("proc_read_super: get root inode failed\n");
	iput(root_inode);
	return NULL;
}
MODULE_LICENSE("GPL");
