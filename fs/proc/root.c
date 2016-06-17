/*
 *  linux/fs/proc/root.c
 *
 *  Copyright (C) 1991, 1992 Linus Torvalds
 *
 *  proc root directory handling functions
 */

#include <asm/uaccess.h>

#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/proc_fs.h>
#include <linux/stat.h>
#include <linux/config.h>
#include <linux/init.h>
#include <linux/module.h>
#include <asm/bitops.h>

struct proc_dir_entry *proc_net, *proc_bus, *proc_root_fs, *proc_root_driver;

#ifdef CONFIG_SYSCTL
struct proc_dir_entry *proc_sys_root;
#endif

static DECLARE_FSTYPE(proc_fs_type, "proc", proc_read_super, FS_SINGLE);

void __init proc_root_init(void)
{
	int err = register_filesystem(&proc_fs_type);
	if (err)
		return;
	proc_mnt = kern_mount(&proc_fs_type);
	err = PTR_ERR(proc_mnt);
	if (IS_ERR(proc_mnt)) {
		unregister_filesystem(&proc_fs_type);
		return;
	}
	proc_misc_init();
	proc_net = proc_mkdir("net", 0);
#ifdef CONFIG_SYSVIPC
	proc_mkdir("sysvipc", 0);
#endif
#ifdef CONFIG_SYSCTL
	proc_sys_root = proc_mkdir("sys", 0);
#endif
#if defined(CONFIG_BINFMT_MISC) || defined(CONFIG_BINFMT_MISC_MODULE)
	proc_mkdir("sys/fs", 0);
	proc_mkdir("sys/fs/binfmt_misc", 0);
#endif
	proc_root_fs = proc_mkdir("fs", 0);
	proc_root_driver = proc_mkdir("driver", 0);
#if defined(CONFIG_SUN_OPENPROMFS) || defined(CONFIG_SUN_OPENPROMFS_MODULE)
	/* just give it a mountpoint */
	proc_mkdir("openprom", 0);
#endif
	proc_tty_init();
#ifdef CONFIG_PROC_DEVICETREE
	proc_device_tree_init();
#endif
#ifdef CONFIG_PPC_ISERIES
	iSeries_proc_create();
#endif
#ifdef CONFIG_PPC64
	proc_ppc64_init(); 
#endif
#ifdef CONFIG_PPC_RTAS
	proc_rtas_init();
#endif
	proc_bus = proc_mkdir("bus", 0);
}

static struct dentry *proc_root_lookup(struct inode * dir, struct dentry * dentry)
{
	if (dir->i_ino == PROC_ROOT_INO) { /* check for safety... */
		int nlink = proc_root.nlink;

		nlink += nr_threads;

		dir->i_nlink = nlink;
	}

	if (!proc_lookup(dir, dentry))
		return NULL;
	
	return proc_pid_lookup(dir, dentry);
}

static int proc_root_readdir(struct file * filp,
	void * dirent, filldir_t filldir)
{
	unsigned int nr = filp->f_pos;

	if (nr < FIRST_PROCESS_ENTRY) {
		int error = proc_readdir(filp, dirent, filldir);
		if (error <= 0)
			return error;
		filp->f_pos = FIRST_PROCESS_ENTRY;
	}

	return proc_pid_readdir(filp, dirent, filldir);
}

/*
 * The root /proc directory is special, as it has the
 * <pid> directories. Thus we don't use the generic
 * directory handling functions for that..
 */
static struct file_operations proc_root_operations = {
	read:		 generic_read_dir,
	readdir:	 proc_root_readdir,
};

/*
 * proc root can do almost nothing..
 */
static struct inode_operations proc_root_inode_operations = {
	lookup:		proc_root_lookup,
};

/*
 * This is the root "inode" in the /proc tree..
 */
struct proc_dir_entry proc_root = {
	low_ino:	PROC_ROOT_INO, 
	namelen:	5, 
	name:		"/proc",
	mode:		S_IFDIR | S_IRUGO | S_IXUGO, 
	nlink:		2, 
	proc_iops:	&proc_root_inode_operations, 
	proc_fops:	&proc_root_operations,
	parent:		&proc_root,
};

#ifdef CONFIG_SYSCTL
EXPORT_SYMBOL(proc_sys_root);
#endif
EXPORT_SYMBOL(proc_symlink);
EXPORT_SYMBOL(proc_mknod);
EXPORT_SYMBOL(proc_mkdir);
EXPORT_SYMBOL(create_proc_entry);
EXPORT_SYMBOL(remove_proc_entry);
EXPORT_SYMBOL(proc_root);
EXPORT_SYMBOL(proc_root_fs);
EXPORT_SYMBOL(proc_net);
EXPORT_SYMBOL(proc_bus);
EXPORT_SYMBOL(proc_root_driver);
