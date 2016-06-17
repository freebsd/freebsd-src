/*
 *   Copyright (C) International Business Machines Corp., 2000-2003
 *   Portions Copyright (C) Christoph Hellwig, 2001-2002
 *
 *   This program is free software;  you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or 
 *   (at your option) any later version.
 * 
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY;  without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 *   the GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program;  if not, write to the Free Software 
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <linux/config.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/blkdev.h>
#include <linux/completion.h>
#include <asm/uaccess.h>
#include "jfs_incore.h"
#include "jfs_filsys.h"
#include "jfs_metapage.h"
#include "jfs_superblock.h"
#include "jfs_dmap.h"
#include "jfs_imap.h"
#include "jfs_debug.h"

MODULE_DESCRIPTION("The Journaled Filesystem (JFS)");
MODULE_AUTHOR("Steve Best/Dave Kleikamp/Barry Arndt, IBM");
MODULE_LICENSE("GPL");

static struct super_operations jfs_super_operations;
static struct file_system_type jfs_fs_type;

int jfs_stop_threads;
static pid_t jfsIOthread;
static pid_t jfsCommitThread;
static pid_t jfsSyncThread;
DECLARE_COMPLETION(jfsIOwait);

#ifdef CONFIG_JFS_DEBUG
int jfsloglevel = JFS_LOGLEVEL_WARN;
MODULE_PARM(jfsloglevel, "i");
MODULE_PARM_DESC(jfsloglevel, "Specify JFS loglevel (0, 1 or 2)");
#endif

/*
 * External declarations
 */
extern int jfs_mount(struct super_block *);
extern int jfs_mount_rw(struct super_block *, int);
extern int jfs_umount(struct super_block *);
extern int jfs_umount_rw(struct super_block *);

extern int jfsIOWait(void *);
extern int jfs_lazycommit(void *);
extern int jfs_sync(void *);
extern void jfs_clear_inode(struct inode *inode);
extern void jfs_read_inode(struct inode *inode);
extern void jfs_dirty_inode(struct inode *inode);
extern void jfs_delete_inode(struct inode *inode);
extern void jfs_write_inode(struct inode *inode, int wait);
extern int jfs_extendfs(struct super_block *, s64, int);

#ifdef PROC_FS_JFS		/* see jfs_debug.h */
extern void jfs_proc_init(void);
extern void jfs_proc_clean(void);
#endif

extern wait_queue_head_t jfs_IO_thread_wait;
extern wait_queue_head_t jfs_commit_thread_wait;
extern wait_queue_head_t jfs_sync_thread_wait;

static void jfs_handle_error(struct super_block *sb)
{
	struct jfs_sb_info *sbi = JFS_SBI(sb);

	if (sb->s_flags & MS_RDONLY)
		return;

	updateSuper(sb, FM_DIRTY);

	if (sbi->flag & JFS_ERR_PANIC)
		panic("JFS (device %s): panic forced after error\n",
			bdevname(sb->s_dev));
	else if (sbi->flag & JFS_ERR_REMOUNT_RO) {
		jfs_err("ERROR: (device %s): remounting filesystem "
			"as read-only\n",
			bdevname(sb->s_dev));
		sb->s_flags |= MS_RDONLY;
	} 

	/* nothing is done for continue beyond marking the superblock dirty */
}

void jfs_error(struct super_block *sb, const char * function, ...)
{
	static char error_buf[256];
	va_list args;

	va_start(args, function);
	vsprintf(error_buf, function, args);
	va_end(args);

	printk(KERN_ERR "ERROR: (device %s): %s\n", bdevname(sb->s_dev),
	       error_buf);

	jfs_handle_error(sb);
}

static int jfs_statfs(struct super_block *sb, struct statfs *buf)
{
	struct jfs_sb_info *sbi = JFS_SBI(sb);
	s64 maxinodes;
	struct inomap *imap = JFS_IP(sbi->ipimap)->i_imap;

	jfs_info("In jfs_statfs");
	buf->f_type = JFS_SUPER_MAGIC;
	buf->f_bsize = sbi->bsize;
	buf->f_blocks = sbi->bmap->db_mapsize;
	buf->f_bfree = sbi->bmap->db_nfree;
	buf->f_bavail = sbi->bmap->db_nfree;
	/*
	 * If we really return the number of allocated & free inodes, some
	 * applications will fail because they won't see enough free inodes.
	 * We'll try to calculate some guess as to how may inodes we can
	 * really allocate
	 *
	 * buf->f_files = atomic_read(&imap->im_numinos);
	 * buf->f_ffree = atomic_read(&imap->im_numfree);
	 */
	maxinodes = min((s64) atomic_read(&imap->im_numinos) +
			((sbi->bmap->db_nfree >> imap->im_l2nbperiext)
			 << L2INOSPEREXT), (s64) 0xffffffffLL);
	buf->f_files = maxinodes;
	buf->f_ffree = maxinodes - (atomic_read(&imap->im_numinos) -
				    atomic_read(&imap->im_numfree));

	buf->f_namelen = JFS_NAME_MAX;
	return 0;
}

static void jfs_put_super(struct super_block *sb)
{
	struct jfs_sb_info *sbi = JFS_SBI(sb);
	int rc;

	jfs_info("In jfs_put_super");
	rc = jfs_umount(sb);
	if (rc)
		jfs_err("jfs_umount failed with return code %d", rc);
	unload_nls(sbi->nls_tab);
	sbi->nls_tab = NULL;

	kfree(sbi);
}

s64 jfs_get_volume_size(struct super_block *sb)
{
	uint blocks = 0;
	s64 bytes;
	kdev_t dev = sb->s_dev;
	int major = MAJOR(dev);
	int minor = MINOR(dev);

	if (blk_size[major]) {
		blocks = blk_size[major][minor];
		if (blocks) {
			bytes = ((s64)blocks) << BLOCK_SIZE_BITS;
			return bytes >> sb->s_blocksize_bits;
		}
	}
	return 0;
}

static int parse_options(char *options, struct super_block *sb, s64 *newLVSize,
			 int *flag)
{
	void *nls_map = NULL;
	char *this_char;
	char *value;
	struct jfs_sb_info *sbi = JFS_SBI(sb);

	*newLVSize = 0;

	if (!options)
		return 1;
	while ((this_char = strsep(&options, ",")) != NULL) {
		if (!*this_char)
			continue;
		if ((value = strchr(this_char, '=')) != NULL)
			*value++ = 0;
		if (!strcmp(this_char, "errors")) {
			if (!value || !*value)
				goto needs_arg;
			if (!strcmp(value, "continue")) {
				*flag &= ~JFS_ERR_REMOUNT_RO;
				*flag &= ~JFS_ERR_PANIC;
				*flag |= JFS_ERR_CONTINUE;
			} else if (!strcmp(value, "remount-ro")) {
				*flag &= ~JFS_ERR_CONTINUE;
				*flag &= ~JFS_ERR_PANIC;
				*flag |= JFS_ERR_REMOUNT_RO;
			} else if (!strcmp(value, "panic")) {
				*flag &= ~JFS_ERR_CONTINUE;
				*flag &= ~JFS_ERR_REMOUNT_RO;
				*flag |= JFS_ERR_PANIC;
			} else {
				printk(KERN_ERR "JFS: %s is an invalid error handler\n", value);
				goto cleanup;
			}
		} else if (!strcmp(this_char, "integrity")) {
			*flag &= ~JFS_NOINTEGRITY;
		} else 	if (!strcmp(this_char, "nointegrity")) {
			*flag |= JFS_NOINTEGRITY;
		} else if (!strcmp(this_char, "iocharset")) {
			if (!value || !*value)
				goto needs_arg;
			if (nls_map)	/* specified iocharset twice! */
				unload_nls(nls_map);
			nls_map = load_nls(value);
			if (!nls_map) {
				printk(KERN_ERR "JFS: charset not found\n");
				goto cleanup;
			}
		} else if (!strcmp(this_char, "resize")) {
			if (!value || !*value) {
				*newLVSize = jfs_get_volume_size(sb);
				if (*newLVSize == 0)
					printk(KERN_ERR
					 "JFS: Cannot determine volume size\n");
			} else
				*newLVSize = simple_strtoull(value, &value, 0);

			/* Silently ignore the quota options */
		} else if (!strcmp(this_char, "grpquota")
			   || !strcmp(this_char, "noquota")
			   || !strcmp(this_char, "quota")
			   || !strcmp(this_char, "usrquota"))
			/* Don't do anything ;-) */ ;
		else {
			printk("jfs: Unrecognized mount option %s\n",
			       this_char);
			goto cleanup;
		}
	}
	if (nls_map) {
		/* Discard old (if remount) */
		if (sbi->nls_tab)
			unload_nls(sbi->nls_tab);
		sbi->nls_tab = nls_map;
	}
	return 1;
needs_arg:
	printk(KERN_ERR "JFS: %s needs an argument\n", this_char);
cleanup:
	if (nls_map)
		unload_nls(nls_map);
	return 0;
}

static int jfs_remount(struct super_block *sb, int *flags, char *data)
{
	s64 newLVSize = 0;
	int rc = 0;
	int flag = JFS_SBI(sb)->flag;

	if (!parse_options(data, sb, &newLVSize, &flag)) {
		return -EINVAL;
	}
	if (newLVSize) {
		if (sb->s_flags & MS_RDONLY) {
			printk(KERN_ERR
		  "JFS: resize requires volume to be mounted read-write\n");
			return -EROFS;
		}
		rc = jfs_extendfs(sb, newLVSize, 0);
		if (rc)
			return rc;
	}

	if ((sb->s_flags & MS_RDONLY) && !(*flags & MS_RDONLY)) {
		JFS_SBI(sb)->flag = flag;
		return jfs_mount_rw(sb, 1);
	}
	if ((!(sb->s_flags & MS_RDONLY)) && (*flags & MS_RDONLY)) {
		rc = jfs_umount_rw(sb);
		JFS_SBI(sb)->flag = flag;
		return rc;
	}
	if ((JFS_SBI(sb)->flag & JFS_NOINTEGRITY) != (flag & JFS_NOINTEGRITY))
		if (!(sb->s_flags & MS_RDONLY)) {
			rc = jfs_umount_rw(sb);
			if (rc)
				return rc;
			JFS_SBI(sb)->flag = flag;
			return jfs_mount_rw(sb, 1);
		}
	JFS_SBI(sb)->flag = flag;

	return 0;
}

static struct super_block *jfs_read_super(struct super_block *sb,
					  void *data, int silent)
{
	struct jfs_sb_info *sbi;
	struct inode *inode;
	int rc;
	s64 newLVSize = 0;
	int flag;

	jfs_info("In jfs_read_super s_dev=0x%x s_flags=0x%lx", sb->s_dev,
		 sb->s_flags);

	sbi = kmalloc(sizeof (struct jfs_sb_info), GFP_KERNEL);
	if (!sbi)
		return NULL;
	memset(sbi, 0, sizeof (struct jfs_sb_info));
	sb->u.generic_sbp = sbi;

	/* initialize the mount flag and determine the default error handler */
	flag = JFS_ERR_REMOUNT_RO;

	if (!parse_options((char *) data, sb, &newLVSize, &flag)) {
		kfree(sbi);
		return NULL;
	}
	sbi->flag = flag;

	if (newLVSize) {
		printk(KERN_ERR "resize option for remount only\n");
		return NULL;
	}

	/*
	 * Initialize blocksize to 4K.
	 */
	sb_set_blocksize(sb, PSIZE);

	/*
	 * Set method vectors.
	 */
	sb->s_op = &jfs_super_operations;

	rc = jfs_mount(sb);
	if (rc) {
		if (!silent) {
			jfs_err("jfs_mount failed w/return code = %d", rc);
		}
		goto out_kfree;
	}
	if (sb->s_flags & MS_RDONLY)
		sbi->log = 0;
	else {
		rc = jfs_mount_rw(sb, 0);
		if (rc) {
			if (!silent) {
				jfs_err("jfs_mount_rw failed, return code = %d",
					rc);
			}
			goto out_no_rw;
		}
	}

	sb->s_magic = JFS_SUPER_MAGIC;

	inode = iget(sb, ROOT_I);
	if (!inode || is_bad_inode(inode))
		goto out_no_root;
	sb->s_root = d_alloc_root(inode);
	if (!sb->s_root)
		goto out_no_root;

	if (!sbi->nls_tab)
		sbi->nls_tab = load_nls_default();

	/* logical blocks are represented by 40 bits in pxd_t, etc. */
	sb->s_maxbytes = ((u64) sb->s_blocksize) << 40;
#if BITS_PER_LONG == 32
	/*
	 * Page cache is indexed by long.
	 * I would use MAX_LFS_FILESIZE, but it's only half as big
	 */
	sb->s_maxbytes = min(((u64) PAGE_CACHE_SIZE << 32) - 1, sb->s_maxbytes);
#endif

	return sb;

out_no_root:
	jfs_err("jfs_read_super: get root inode failed");
	if (inode)
		iput(inode);

out_no_rw:
	rc = jfs_umount(sb);
	if (rc) {
		jfs_err("jfs_umount failed with return code %d", rc);
	}
out_kfree:
	if (sbi->nls_tab)
		unload_nls(sbi->nls_tab);
	kfree(sbi);
	return NULL;
}

static void jfs_write_super_lockfs(struct super_block *sb)
{
	struct jfs_sb_info *sbi = JFS_SBI(sb);
	struct jfs_log *log = sbi->log;

	if (!(sb->s_flags & MS_RDONLY)) {
		txQuiesce(sb);
		lmLogShutdown(log);
		updateSuper(sb, FM_CLEAN);
	}
}

static void jfs_unlockfs(struct super_block *sb)
{
	struct jfs_sb_info *sbi = JFS_SBI(sb);
	struct jfs_log *log = sbi->log;
	int rc = 0;

	if (!(sb->s_flags & MS_RDONLY)) {
		updateSuper(sb, FM_MOUNT);
		if ((rc = lmLogInit(log)))
			jfs_err("jfs_unlock failed with return code %d", rc);
		else
			txResume(sb);
	}
}


static int jfs_sync_fs(struct super_block *sb)
{
	struct jfs_log *log = JFS_SBI(sb)->log;

	/* log == NULL indicates read-only mount */
	if (log)
		jfs_flush_journal(log, 1);

	return 0;
}

static struct super_operations jfs_super_operations = {
	.read_inode	= jfs_read_inode,
	.dirty_inode	= jfs_dirty_inode,
	.write_inode	= jfs_write_inode,
	.clear_inode	= jfs_clear_inode,
	.delete_inode	= jfs_delete_inode,
	.put_super	= jfs_put_super,
	.sync_fs	= jfs_sync_fs,
	.write_super_lockfs = jfs_write_super_lockfs,
	.unlockfs       = jfs_unlockfs,
	.statfs		= jfs_statfs,
	.remount_fs	= jfs_remount,
};

static struct file_system_type jfs_fs_type = {
	.owner		= THIS_MODULE,
	.name		= "jfs",
	.read_super	= jfs_read_super,
	.fs_flags	= FS_REQUIRES_DEV,
};

extern int metapage_init(void);
extern int txInit(void);
extern void txExit(void);
extern void metapage_exit(void);

static void init_once(void *foo, kmem_cache_t * cachep, unsigned long flags)
{
	struct jfs_inode_info *jfs_ip = (struct jfs_inode_info *) foo;

	if ((flags & (SLAB_CTOR_VERIFY | SLAB_CTOR_CONSTRUCTOR)) ==
	    SLAB_CTOR_CONSTRUCTOR) {
		memset(jfs_ip, 0, sizeof(struct jfs_inode_info));
		INIT_LIST_HEAD(&jfs_ip->anon_inode_list);
		init_rwsem(&jfs_ip->rdwrlock);
		init_MUTEX(&jfs_ip->commit_sem);
		jfs_ip->active_ag = -1;
	}
}

static int __init init_jfs_fs(void)
{
	int rc;

	jfs_inode_cachep =
	    kmem_cache_create("jfs_ip", sizeof (struct jfs_inode_info), 0, 0,
			      init_once, NULL);
	if (jfs_inode_cachep == NULL)
		return -ENOMEM;

	/*
	 * Metapage initialization
	 */
	rc = metapage_init();
	if (rc) {
		jfs_err("metapage_init failed w/rc = %d", rc);
		goto free_slab;
	}

	/*
	 * Transaction Manager initialization
	 */
	rc = txInit();
	if (rc) {
		jfs_err("txInit failed w/rc = %d", rc);
		goto free_metapage;
	}

	/*
	 * I/O completion thread (endio)
	 */
	jfsIOthread = kernel_thread(jfsIOWait, 0,
				    CLONE_FS | CLONE_FILES | CLONE_SIGHAND);
	if (jfsIOthread < 0) {
		jfs_err("init_jfs_fs: fork failed w/rc = %d", jfsIOthread);
		goto end_txmngr;
	}
	wait_for_completion(&jfsIOwait);	/* Wait until thread starts */

	jfsCommitThread = kernel_thread(jfs_lazycommit, 0,
					CLONE_FS | CLONE_FILES | CLONE_SIGHAND);
	if (jfsCommitThread < 0) {
		jfs_err("init_jfs_fs: fork failed w/rc = %d", jfsCommitThread);
		goto kill_iotask;
	}
	wait_for_completion(&jfsIOwait);	/* Wait until thread starts */

	jfsSyncThread = kernel_thread(jfs_sync, 0,
				      CLONE_FS | CLONE_FILES | CLONE_SIGHAND);
	if (jfsSyncThread < 0) {
		jfs_err("init_jfs_fs: fork failed w/rc = %d", jfsSyncThread);
		goto kill_committask;
	}
	wait_for_completion(&jfsIOwait);	/* Wait until thread starts */

#ifdef PROC_FS_JFS
	jfs_proc_init();
#endif

	return register_filesystem(&jfs_fs_type);

kill_committask:
	jfs_stop_threads = 1;
	wake_up(&jfs_commit_thread_wait);
	wait_for_completion(&jfsIOwait);	/* Wait for thread exit */
kill_iotask:
	jfs_stop_threads = 1;
	wake_up(&jfs_IO_thread_wait);
	wait_for_completion(&jfsIOwait);	/* Wait for thread exit */
end_txmngr:
	txExit();
free_metapage:
	metapage_exit();
free_slab:
	kmem_cache_destroy(jfs_inode_cachep);
	return rc;
}

static void __exit exit_jfs_fs(void)
{
	jfs_info("exit_jfs_fs called");

	jfs_stop_threads = 1;
	txExit();
	metapage_exit();
	wake_up(&jfs_IO_thread_wait);
	wait_for_completion(&jfsIOwait);	/* Wait for IO thread exit */
	wake_up(&jfs_commit_thread_wait);
	wait_for_completion(&jfsIOwait);	/* Wait for Commit thread exit */
	wake_up(&jfs_sync_thread_wait);
	wait_for_completion(&jfsIOwait);	/* Wait for Sync thread exit */
#ifdef PROC_FS_JFS
	jfs_proc_clean();
#endif
	unregister_filesystem(&jfs_fs_type);
	kmem_cache_destroy(jfs_inode_cachep);
}

EXPORT_NO_SYMBOLS;

module_init(init_jfs_fs)
module_exit(exit_jfs_fs)
