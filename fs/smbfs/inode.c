/*
 *  inode.c
 *
 *  Copyright (C) 1995, 1996 by Paal-Kr. Engstad and Volker Lendecke
 *  Copyright (C) 1997 by Volker Lendecke
 *
 *  Please add a note about your changes to smbfs in the ChangeLog file.
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/stat.h>
#include <linux/errno.h>
#include <linux/locks.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/file.h>
#include <linux/dcache.h>
#include <linux/smp_lock.h>
#include <linux/nls.h>
#include <linux/seq_file.h>

#include <linux/smb_fs.h>
#include <linux/smbno.h>
#include <linux/smb_mount.h>

#include <asm/system.h>
#include <asm/uaccess.h>

#include "smb_debug.h"
#include "getopt.h"
#include "proto.h"

/* Always pick a default string */
#ifdef CONFIG_SMB_NLS_REMOTE
#define SMB_NLS_REMOTE CONFIG_SMB_NLS_REMOTE
#else
#define SMB_NLS_REMOTE ""
#endif

#define SMB_TTL_DEFAULT 1000
#define SMB_TIMEO_DEFAULT 30

static void smb_delete_inode(struct inode *);
static void smb_put_super(struct super_block *);
static int  smb_statfs(struct super_block *, struct statfs *);
static int  smb_show_options(struct seq_file *, struct vfsmount *);

static struct super_operations smb_sops =
{
	put_inode:	force_delete,
	delete_inode:	smb_delete_inode,
	put_super:	smb_put_super,
	statfs:		smb_statfs,
	show_options:	smb_show_options,
};


/* We are always generating a new inode here */
struct inode *
smb_iget(struct super_block *sb, struct smb_fattr *fattr)
{
	struct inode *result;

	DEBUG1("smb_iget: %p\n", fattr);

	result = new_inode(sb);
	if (!result)
		return result;
	result->i_ino = fattr->f_ino;
	memset(&(result->u.smbfs_i), 0, sizeof(result->u.smbfs_i));
	smb_set_inode_attr(result, fattr);
	if (S_ISREG(result->i_mode)) {
		result->i_op = &smb_file_inode_operations;
		result->i_fop = &smb_file_operations;
		result->i_data.a_ops = &smb_file_aops;
	} else if (S_ISDIR(result->i_mode)) {
		struct smb_sb_info *server = &(sb->u.smbfs_sb);
		if (server->opt.capabilities & SMB_CAP_UNIX)
			result->i_op = &smb_dir_inode_operations_unix;
		else
			result->i_op = &smb_dir_inode_operations;
		result->i_fop = &smb_dir_operations;
	} else if(S_ISLNK(result->i_mode)) {
		result->i_op = &smb_link_inode_operations;
	} else {
		init_special_inode(result, result->i_mode, fattr->f_rdev);
	}
	insert_inode_hash(result);
	return result;
}

/*
 * Copy the inode data to a smb_fattr structure.
 */
void
smb_get_inode_attr(struct inode *inode, struct smb_fattr *fattr)
{
	memset(fattr, 0, sizeof(struct smb_fattr));
	fattr->f_mode	= inode->i_mode;
	fattr->f_nlink	= inode->i_nlink;
	fattr->f_ino	= inode->i_ino;
	fattr->f_uid	= inode->i_uid;
	fattr->f_gid	= inode->i_gid;
	fattr->f_rdev	= inode->i_rdev;
	fattr->f_size	= inode->i_size;
	fattr->f_mtime	= inode->i_mtime;
	fattr->f_ctime	= inode->i_ctime;
	fattr->f_atime	= inode->i_atime;
	fattr->f_blksize= inode->i_blksize;
	fattr->f_blocks	= inode->i_blocks;

	fattr->attr	= inode->u.smbfs_i.attr;
	/*
	 * Keep the attributes in sync with the inode permissions.
	 */
	if (fattr->f_mode & S_IWUSR)
		fattr->attr &= ~aRONLY;
	else
		fattr->attr |= aRONLY;
}

/*
 * Update the inode, possibly causing it to invalidate its pages if mtime/size
 * is different from last time.
 */
void
smb_set_inode_attr(struct inode *inode, struct smb_fattr *fattr)
{
	/*
	 * A size change should have a different mtime, or same mtime
	 * but different size.
	 */
	time_t last_time = inode->i_mtime;
	loff_t last_sz = inode->i_size;

	inode->i_mode	= fattr->f_mode;
	inode->i_nlink	= fattr->f_nlink;
	inode->i_uid	= fattr->f_uid;
	inode->i_gid	= fattr->f_gid;
	inode->i_rdev	= fattr->f_rdev;
	inode->i_ctime	= fattr->f_ctime;
	inode->i_blksize= fattr->f_blksize;
	inode->i_blocks = fattr->f_blocks;
	inode->i_size	= fattr->f_size;
	inode->i_mtime	= fattr->f_mtime;
	inode->i_atime	= fattr->f_atime;
	inode->u.smbfs_i.attr = fattr->attr;
	/*
	 * Update the "last time refreshed" field for revalidation.
	 */
	inode->u.smbfs_i.oldmtime = jiffies;

	if (inode->i_mtime != last_time || inode->i_size != last_sz) {
		VERBOSE("%ld changed, old=%ld, new=%ld, oz=%ld, nz=%ld\n",
			inode->i_ino,
			(long) last_time, (long) inode->i_mtime,
			(long) last_sz, (long) inode->i_size);

		if (!S_ISDIR(inode->i_mode))
			invalidate_inode_pages(inode);
	}
}

/*
 * This is called if the connection has gone bad ...
 * try to kill off all the current inodes.
 */
void
smb_invalidate_inodes(struct smb_sb_info *server)
{
	VERBOSE("\n");
	shrink_dcache_sb(SB_of(server));
	invalidate_inodes(SB_of(server));
}

/*
 * This is called to update the inode attributes after
 * we've made changes to a file or directory.
 */
static int
smb_refresh_inode(struct dentry *dentry)
{
	struct inode *inode = dentry->d_inode;
	int error;
	struct smb_fattr fattr;

	error = smb_proc_getattr(dentry, &fattr);
	if (!error) {
		smb_renew_times(dentry);
		/*
		 * Check whether the type part of the mode changed,
		 * and don't update the attributes if it did.
		 *
		 * And don't dick with the root inode
		 */
		if (inode->i_ino == 2)
			return error;
		if (S_ISLNK(inode->i_mode))
			return error;	/* VFS will deal with it */

		if ((inode->i_mode & S_IFMT) == (fattr.f_mode & S_IFMT)) {
			smb_set_inode_attr(inode, &fattr);
		} else {
			/*
			 * Big trouble! The inode has become a new object,
			 * so any operations attempted on it are invalid.
			 *
			 * To limit damage, mark the inode as bad so that
			 * subsequent lookup validations will fail.
			 */
			PARANOIA("%s/%s changed mode, %07o to %07o\n",
				 DENTRY_PATH(dentry),
				 inode->i_mode, fattr.f_mode);

			fattr.f_mode = inode->i_mode; /* save mode */
			make_bad_inode(inode);
			inode->i_mode = fattr.f_mode; /* restore mode */
			/*
			 * No need to worry about unhashing the dentry: the
			 * lookup validation will see that the inode is bad.
			 * But we do want to invalidate the caches ...
			 */
			if (!S_ISDIR(inode->i_mode))
				invalidate_inode_pages(inode);
			else
				smb_invalid_dir_cache(inode);
			error = -EIO;
		}
	}
	return error;
}

/*
 * This is called when we want to check whether the inode
 * has changed on the server.  If it has changed, we must
 * invalidate our local caches.
 */
int
smb_revalidate_inode(struct dentry *dentry)
{
	struct smb_sb_info *s = server_from_dentry(dentry);
	struct inode *inode = dentry->d_inode;
	int error = 0;

	DEBUG1("smb_revalidate_inode\n");
	lock_kernel();

	/*
	 * Check whether we've recently refreshed the inode.
	 */
	if (time_before(jiffies, inode->u.smbfs_i.oldmtime + SMB_MAX_AGE(s))) {
		VERBOSE("up-to-date, ino=%ld, jiffies=%lu, oldtime=%lu\n",
			inode->i_ino, jiffies, inode->u.smbfs_i.oldmtime);
		goto out;
	}

	error = smb_refresh_inode(dentry);
out:
	unlock_kernel();
	return error;
}

/*
 * This routine is called when i_nlink == 0 and i_count goes to 0.
 * All blocking cleanup operations need to go here to avoid races.
 */
static void
smb_delete_inode(struct inode *ino)
{
	DEBUG1("ino=%ld\n", ino->i_ino);
	lock_kernel();
	if (smb_close(ino))
		PARANOIA("could not close inode %ld\n", ino->i_ino);
	unlock_kernel();
	clear_inode(ino);
}

static struct option opts[] = {
	{ "version",	0, 'v' },
	{ "win95",	SMB_MOUNT_WIN95, 1 },
	{ "oldattr",	SMB_MOUNT_OLDATTR, 1 },
	{ "dirattr",	SMB_MOUNT_DIRATTR, 1 },
	{ "case",	SMB_MOUNT_CASE, 1 },
	{ "uid",	0, 'u' },
	{ "gid",	0, 'g' },
	{ "file_mode",	0, 'f' },
	{ "dir_mode",	0, 'd' },
	{ "iocharset",	0, 'i' },
	{ "codepage",	0, 'c' },
	{ "ttl",	0, 't' },
	{ "timeo",	0, 'o' },
	{ NULL,		0, 0}
};

static int
parse_options(struct smb_mount_data_kernel *mnt, char *options)
{
	int c;
	unsigned long flags;
	unsigned long value;
	char *optarg;
	char *optopt;

	flags = 0;
	while ( (c = smb_getopt("smbfs", &options, opts,
				&optopt, &optarg, &flags, &value)) > 0) {

		VERBOSE("'%s' -> '%s'\n", optopt, optarg ? optarg : "<none>");

		switch (c) {
		case 1:
			/* got a "flag" option */
			break;
		case 'v':
			if (value != SMB_MOUNT_VERSION) {
			printk ("smbfs: Bad mount version %ld, expected %d\n",
				value, SMB_MOUNT_VERSION);
				return 0;
			}
			mnt->version = value;
			break;
		case 'u':
			mnt->uid = value;
			break;
		case 'g':
			mnt->gid = value;
			break;
		case 'f':
			mnt->file_mode = (value & S_IRWXUGO) | S_IFREG;
			break;
		case 'd':
			mnt->dir_mode = (value & S_IRWXUGO) | S_IFDIR;
			break;
		case 'i':
			strncpy(mnt->codepage.local_name, optarg, 
				SMB_NLS_MAXNAMELEN);
			break;
		case 'c':
			strncpy(mnt->codepage.remote_name, optarg,
				SMB_NLS_MAXNAMELEN);
			break;
		case 't':
			mnt->ttl = value;
			break;
		case 'o':
			mnt->timeo = value;
			break;
		default:
			printk ("smbfs: Unrecognized mount option %s\n",
				optopt);
			return -1;
		}
	}
	mnt->flags = flags;
	return c;
}

/*
 * smb_show_options() is for displaying mount options in /proc/mounts.
 * It tries to avoid showing settings that were not changed from their
 * defaults.
 */
static int
smb_show_options(struct seq_file *s, struct vfsmount *m)
{
	struct smb_mount_data_kernel *mnt = m->mnt_sb->u.smbfs_sb.mnt;
	int i;

	for (i = 0; opts[i].name != NULL; i++)
		if (mnt->flags & opts[i].flag)
			seq_printf(s, ",%s", opts[i].name);

	if (mnt->uid != 0)
		seq_printf(s, ",uid=%d", mnt->uid);
	if (mnt->gid != 0)
		seq_printf(s, ",gid=%d", mnt->gid);
	if (mnt->mounted_uid != 0)
		seq_printf(s, ",mounted_uid=%d", mnt->mounted_uid);

	/* 
	 * Defaults for file_mode and dir_mode are unknown to us; they
	 * depend on the current umask of the user doing the mount.
	 */
	seq_printf(s, ",file_mode=%04o", mnt->file_mode & S_IRWXUGO);
	seq_printf(s, ",dir_mode=%04o", mnt->dir_mode & S_IRWXUGO);

	if (strcmp(mnt->codepage.local_name, CONFIG_NLS_DEFAULT))
		seq_printf(s, ",iocharset=%s", mnt->codepage.local_name);
	if (strcmp(mnt->codepage.remote_name, SMB_NLS_REMOTE))
		seq_printf(s, ",codepage=%s", mnt->codepage.remote_name);

	if (mnt->ttl != SMB_TTL_DEFAULT)
		seq_printf(s, ",ttl=%d", mnt->ttl);
	if (mnt->timeo != SMB_TIMEO_DEFAULT)
		seq_printf(s, ",timeo=%d", mnt->timeo);

	return 0;
}

static void
smb_put_super(struct super_block *sb)
{
	struct smb_sb_info *server = &(sb->u.smbfs_sb);

	if (server->sock_file) {
		smb_dont_catch_keepalive(server);
		fput(server->sock_file);
	}

	if (server->conn_pid)
	       kill_proc(server->conn_pid, SIGTERM, 1);

	smb_kfree(server->mnt);
	smb_kfree(server->temp_buf);
	if (server->packet)
		smb_vfree(server->packet);

	if (server->remote_nls) {
		unload_nls(server->remote_nls);
		server->remote_nls = NULL;
	}
	if (server->local_nls) {
		unload_nls(server->local_nls);
		server->local_nls = NULL;
	}
}

struct super_block *
smb_read_super(struct super_block *sb, void *raw_data, int silent)
{
	struct smb_sb_info *server = &sb->u.smbfs_sb;
	struct smb_mount_data_kernel *mnt;
	struct smb_mount_data *oldmnt;
	struct inode *root_inode;
	struct smb_fattr root;
	int ver;
	void *mem;

	if (!raw_data)
		goto out_no_data;

	oldmnt = (struct smb_mount_data *) raw_data;
	ver = oldmnt->version;
	if (ver != SMB_MOUNT_OLDVERSION && cpu_to_be32(ver) != SMB_MOUNT_ASCII)
		goto out_wrong_data;

	sb->s_blocksize = 1024;	/* Eh...  Is this correct? */
	sb->s_blocksize_bits = 10;
	sb->s_maxbytes = MAX_NON_LFS;
	sb->s_magic = SMB_SUPER_MAGIC;
	sb->s_op = &smb_sops;

	server->mnt = NULL;
	server->sock_file = NULL;
	init_MUTEX(&server->sem);
	init_waitqueue_head(&server->wait);
	server->conn_pid = 0;
	server->state = CONN_INVALID; /* no connection yet */
	server->generation = 0;
	server->packet_size = smb_round_length(SMB_INITIAL_PACKET_SIZE);
	server->packet = smb_vmalloc(server->packet_size);
	if (!server->packet)
		goto out_no_mem;

	/* Allocate the global temp buffer */
	server->temp_buf = smb_kmalloc(2*SMB_MAXPATHLEN+20, GFP_KERNEL);
	if (!server->temp_buf)
		goto out_no_temp;

	/* Setup NLS stuff */
	server->remote_nls = NULL;
	server->local_nls = NULL;
	server->name_buf = server->temp_buf + SMB_MAXPATHLEN + 20;

	/* Allocate the mount data structure */
	/* FIXME: merge this with the other malloc and get a whole page? */
	mem = smb_kmalloc(sizeof(struct smb_ops) +
			  sizeof(struct smb_mount_data_kernel), GFP_KERNEL);
	if (!mem)
		goto out_no_mount;
	server->mnt = mnt = mem;
	server->ops = mem + sizeof(struct smb_mount_data_kernel);
	smb_install_null_ops(server->ops);

	memset(mnt, 0, sizeof(struct smb_mount_data_kernel));
	strncpy(mnt->codepage.local_name, CONFIG_NLS_DEFAULT,
		SMB_NLS_MAXNAMELEN);
	strncpy(mnt->codepage.remote_name, SMB_NLS_REMOTE,
		SMB_NLS_MAXNAMELEN);

	mnt->ttl = SMB_TTL_DEFAULT;
	mnt->timeo = SMB_TIMEO_DEFAULT;
	if (ver == SMB_MOUNT_OLDVERSION) {
		mnt->version = oldmnt->version;

		/* FIXME: is this enough to convert uid/gid's ? */
		mnt->uid = oldmnt->uid;
		mnt->gid = oldmnt->gid;

		mnt->file_mode = (oldmnt->file_mode & S_IRWXUGO) | S_IFREG;
		mnt->dir_mode = (oldmnt->dir_mode & S_IRWXUGO) | S_IFDIR;

		mnt->flags = (oldmnt->file_mode >> 9);
	} else {
		if (parse_options(mnt, raw_data))
			goto out_bad_option;
	}
	smb_setcodepage(server, &mnt->codepage);
	mnt->mounted_uid = current->uid;

	/*
	 * Display the enabled options
	 * Note: smb_proc_getattr uses these in 2.4 (but was changed in 2.2)
	 */
	if (mnt->flags & SMB_MOUNT_OLDATTR)
		printk("SMBFS: Using core getattr (Win 95 speedup)\n");
	else if (mnt->flags & SMB_MOUNT_DIRATTR)
		printk("SMBFS: Using dir ff getattr\n");

	/*
	 * Keep the super block locked while we get the root inode.
	 */
	smb_init_root_dirent(server, &root);
	root_inode = smb_iget(sb, &root);
	if (!root_inode)
		goto out_no_root;

	sb->s_root = d_alloc_root(root_inode);
	if (!sb->s_root)
		goto out_no_root;
	smb_new_dentry(sb->s_root);

	return sb;

out_no_root:
	iput(root_inode);
out_bad_option:
	smb_kfree(server->mnt);
out_no_mount:
	smb_kfree(server->temp_buf);
out_no_temp:
	smb_vfree(server->packet);
out_no_mem:
	if (!server->mnt)
		printk(KERN_ERR "smb_read_super: allocation failure\n");
	goto out_fail;
out_wrong_data:
	printk(KERN_ERR "smbfs: mount_data version %d is not supported\n", ver);
	goto out_fail;
out_no_data:
	printk(KERN_ERR "smb_read_super: missing data argument\n");
out_fail:
	return NULL;
}

static int
smb_statfs(struct super_block *sb, struct statfs *buf)
{
	int result = smb_proc_dskattr(sb, buf);

	buf->f_type = SMB_SUPER_MAGIC;
	buf->f_namelen = SMB_MAXPATHLEN;
	return result;
}

int
smb_notify_change(struct dentry *dentry, struct iattr *attr)
{
	struct inode *inode = dentry->d_inode;
	struct smb_sb_info *server = server_from_dentry(dentry);
	unsigned int mask = (S_IFREG | S_IFDIR | S_IRWXUGO);
	int error, changed, refresh = 0;
	struct smb_fattr fattr;

	error = smb_revalidate_inode(dentry);
	if (error)
		goto out;

	if ((error = inode_change_ok(inode, attr)) < 0)
		goto out;

	error = -EPERM;
	if ((attr->ia_valid & ATTR_UID) && (attr->ia_uid != server->mnt->uid))
		goto out;

	if ((attr->ia_valid & ATTR_GID) && (attr->ia_uid != server->mnt->gid))
		goto out;

	if ((attr->ia_valid & ATTR_MODE) && (attr->ia_mode & ~mask))
		goto out;

	if ((attr->ia_valid & ATTR_SIZE) != 0) {
		VERBOSE("changing %s/%s, old size=%ld, new size=%ld\n",
			DENTRY_PATH(dentry),
			(long) inode->i_size, (long) attr->ia_size);

		filemap_fdatasync(inode->i_mapping);
		filemap_fdatawait(inode->i_mapping);

		error = smb_open(dentry, O_WRONLY);
		if (error)
			goto out;
		error = server->ops->truncate(inode, attr->ia_size);
		if (error)
			goto out;
		error = vmtruncate(inode, attr->ia_size);
		if (error)
			goto out;
		refresh = 1;
	}

	if (server->opt.capabilities & SMB_CAP_UNIX) {
		/* For now we don't want to set the size with setattr_unix */
		attr->ia_valid &= ~ATTR_SIZE;
		/* FIXME: only call if we actually want to set something? */
		error = smb_proc_setattr_unix(dentry, attr, 0, 0);
		if (!error)
			refresh = 1;
		goto out;
	}

	/*
	 * Initialize the fattr and check for changed fields.
	 * Note: CTIME under SMB is creation time rather than
	 * change time, so we don't attempt to change it.
	 */
	smb_get_inode_attr(inode, &fattr);

	changed = 0;
	if ((attr->ia_valid & ATTR_MTIME) != 0) {
		fattr.f_mtime = attr->ia_mtime;
		changed = 1;
	}
	if ((attr->ia_valid & ATTR_ATIME) != 0) {
		fattr.f_atime = attr->ia_atime;
		/* Earlier protocols don't have an access time */
		if (server->opt.protocol >= SMB_PROTOCOL_LANMAN2)
			changed = 1;
	}
	if (changed) {
		error = smb_proc_settime(dentry, &fattr);
		if (error)
			goto out;
		refresh = 1;
	}

	/*
	 * Check for mode changes ... we're extremely limited in
	 * what can be set for SMB servers: just the read-only bit.
	 */
	if ((attr->ia_valid & ATTR_MODE) != 0) {
		VERBOSE("%s/%s mode change, old=%x, new=%x\n",
			DENTRY_PATH(dentry), fattr.f_mode, attr->ia_mode);
		changed = 0;
		if (attr->ia_mode & S_IWUSR) {
			if (fattr.attr & aRONLY) {
				fattr.attr &= ~aRONLY;
				changed = 1;
			}
		} else {
			if (!(fattr.attr & aRONLY)) {
				fattr.attr |= aRONLY;
				changed = 1;
			}
		}
		if (changed) {
			error = smb_proc_setattr(dentry, &fattr);
			if (error)
				goto out;
			refresh = 1;
		}
	}
	error = 0;

out:
	if (refresh)
		smb_refresh_inode(dentry);
	return error;
}

#ifdef DEBUG_SMB_MALLOC
int smb_malloced;
int smb_current_kmalloced;
int smb_current_vmalloced;
#endif

static DECLARE_FSTYPE( smb_fs_type, "smbfs", smb_read_super, 0);

static int __init init_smb_fs(void)
{
	DEBUG1("registering ...\n");

#ifdef DEBUG_SMB_MALLOC
	smb_malloced = 0;
	smb_current_kmalloced = 0;
	smb_current_vmalloced = 0;
#endif

	return register_filesystem(&smb_fs_type);
}

static void __exit exit_smb_fs(void)
{
	DEBUG1("unregistering ...\n");
	unregister_filesystem(&smb_fs_type);
#ifdef DEBUG_SMB_MALLOC
	printk(KERN_DEBUG "smb_malloced: %d\n", smb_malloced);
	printk(KERN_DEBUG "smb_current_kmalloced: %d\n",smb_current_kmalloced);
	printk(KERN_DEBUG "smb_current_vmalloced: %d\n",smb_current_vmalloced);
#endif
}

EXPORT_NO_SYMBOLS;

module_init(init_smb_fs)
module_exit(exit_smb_fs)
MODULE_LICENSE("GPL");
