/*
 *  linux/fs/umsdos/inode.c
 *
 *      Written 1993 by Jacques Gelinas
 *      Inspired from linux/fs/msdos/... by Werner Almesberger
 */

#include <linux/module.h>

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/msdos_fs.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <asm/uaccess.h>
#include <linux/string.h>
#include <linux/stat.h>
#include <linux/umsdos_fs.h>
#include <linux/list.h>
#include <linux/pagemap.h>

extern struct dentry_operations umsdos_dentry_operations;

struct dentry *saved_root;	/* Original root if changed */
struct inode *pseudo_root;	/* Useful to simulate the pseudo DOS */
					/* directory. See UMSDOS_readdir_x() */

static struct dentry *check_pseudo_root(struct super_block *);


void UMSDOS_put_inode (struct inode *inode)
{
	PRINTK ((KERN_DEBUG 
		"put inode %p (%lu) pos %lu count=%d\n"
		 ,inode, inode->i_ino
		 ,inode->u.umsdos_i.pos
		 ,atomic_read(&inode->i_count)));

	if (inode == pseudo_root) {
		Printk ((KERN_ERR "Umsdos: debug: releasing pseudo_root - ino=%lu count=%d\n", inode->i_ino, atomic_read(&inode->i_count)));
	}

	if (atomic_read(&inode->i_count) == 1)
		inode->u.umsdos_i.i_patched = 0;
}


void UMSDOS_put_super (struct super_block *sb)
{
	Printk ((KERN_DEBUG "UMSDOS_put_super: entering\n"));
	if (saved_root && pseudo_root && sb->s_dev == ROOT_DEV) {
		shrink_dcache_parent(saved_root);
		dput(saved_root);
		saved_root = NULL;
		pseudo_root = NULL;
	}
	msdos_put_super (sb);
}


/*
 * Complete the setup of a directory dentry based on its
 * EMD/non-EMD status.  If it has an EMD, then plug the
 * umsdos function table. If not, use the msdos one.
 */
void umsdos_setup_dir(struct dentry *dir)
{
	struct inode *inode = dir->d_inode;

	if (!S_ISDIR(inode->i_mode))
		printk(KERN_ERR "umsdos_setup_dir: %s/%s not a dir!\n",
			dir->d_parent->d_name.name, dir->d_name.name);

	init_waitqueue_head (&inode->u.umsdos_i.dir_info.p);
	inode->u.umsdos_i.dir_info.looking = 0;
	inode->u.umsdos_i.dir_info.creating = 0;
	inode->u.umsdos_i.dir_info.pid = 0;

	inode->i_op = &umsdos_rdir_inode_operations;
	inode->i_fop = &umsdos_rdir_operations;
	if (umsdos_have_emd(dir)) {
Printk((KERN_DEBUG "umsdos_setup_dir: %s/%s using EMD\n",
dir->d_parent->d_name.name, dir->d_name.name));
		inode->i_op = &umsdos_dir_inode_operations;
		inode->i_fop = &umsdos_dir_operations;
	}
}


/*
 * Add some info into an inode so it can find its owner quickly
 */
void umsdos_set_dirinfo_new (struct dentry *dentry, off_t f_pos)
{
	struct inode *inode = dentry->d_inode;
	struct dentry *demd;

	inode->u.umsdos_i.pos = f_pos;

	/* now check the EMD file */
	demd = umsdos_get_emd_dentry(dentry->d_parent);
	if (!IS_ERR(demd)) {
		dput(demd);
	}
	return;
}

static struct inode_operations umsdos_file_inode_operations = {
	truncate:	fat_truncate,
	setattr:	UMSDOS_notify_change,
};

static struct inode_operations umsdos_symlink_inode_operations = {
	readlink:	page_readlink,
	follow_link:	page_follow_link,
	setattr:	UMSDOS_notify_change,
};

/*
 * Connect the proper tables in the inode and add some info.
 */
/* #Specification: inode / umsdos info
 * The first time an inode is seen (inode->i_count == 1),
 * the inode number of the EMD file which controls this inode
 * is tagged to this inode. It allows operations such as
 * notify_change to be handled.
 */
void umsdos_patch_dentry_inode(struct dentry *dentry, off_t f_pos)
{
	struct inode *inode = dentry->d_inode;

PRINTK (("umsdos_patch_dentry_inode: inode=%lu\n", inode->i_ino));

	/*
	 * Classify the inode based on EMD/non-EMD status.
	 */
PRINTK (("umsdos_patch_inode: call umsdos_set_dirinfo_new(%p,%lu)\n",
dentry, f_pos));
	umsdos_set_dirinfo_new(dentry, f_pos);

	inode->i_op = &umsdos_file_inode_operations;
	if (S_ISREG (inode->i_mode)) {
		/* address_space operations already set */
	} else if (S_ISDIR (inode->i_mode)) {
		umsdos_setup_dir(dentry);
	} else if (S_ISLNK (inode->i_mode)) {
		/* address_space operations already set */
		inode->i_op = &umsdos_symlink_inode_operations;
	} else
		init_special_inode(inode, inode->i_mode,
					kdev_t_to_nr(inode->i_rdev));
}


/*
 * lock the parent dir before starting ...
 * also handles hardlink converting
 */
int UMSDOS_notify_change (struct dentry *dentry, struct iattr *attr)
{
	struct inode *dir, *inode;
	struct umsdos_info info;
	struct dentry *temp, *old_dentry = NULL;
	int ret;

	ret = umsdos_parse (dentry->d_name.name, dentry->d_name.len,
				&info);
	if (ret)
		goto out;
	ret = umsdos_findentry (dentry->d_parent, &info, 0);
	if (ret) {
printk("UMSDOS_notify_change: %s/%s not in EMD, ret=%d\n",
dentry->d_parent->d_name.name, dentry->d_name.name, ret);
		goto out;
	}

	if (info.entry.flags & UMSDOS_HLINK) {
		/*
		 * In order to get the correct (real) inode, we just drop
		 * the original dentry.
		 */ 
		d_drop(dentry);
Printk(("UMSDOS_notify_change: hard link %s/%s, fake=%s\n",
dentry->d_parent->d_name.name, dentry->d_name.name, info.fake.fname));
	
		/* Do a real lookup to get the short name dentry */
		temp = umsdos_covered(dentry->d_parent, info.fake.fname,
						info.fake.len);
		ret = PTR_ERR(temp);
		if (IS_ERR(temp))
			goto out;
	
		/* now resolve the link ... */
		temp = umsdos_solve_hlink(temp);
		ret = PTR_ERR(temp);
		if (IS_ERR(temp))
			goto out;
		old_dentry = dentry;
		dentry = temp;	/* so umsdos_notify_change_locked will operate on that */
	}

	dir = dentry->d_parent->d_inode;
	inode = dentry->d_inode;

	ret = inode_change_ok (inode, attr);
	if (ret)
		goto out;

	down(&dir->i_sem);
	ret = umsdos_notify_change_locked(dentry, attr);
	up(&dir->i_sem);
	if (ret == 0)
		ret = inode_setattr (inode, attr);
out:
	if (old_dentry)
		dput (dentry);	/* if we had to use fake dentry for hardlinks, dput() it now */
	return ret;
}


/*
 * Must be called with the parent lock held.
 */
int umsdos_notify_change_locked(struct dentry *dentry, struct iattr *attr)
{
	struct inode *inode = dentry->d_inode;
	struct dentry *demd;
	struct address_space *mapping;
	struct page *page;
	int ret = 0;
	struct umsdos_dirent *entry;
	int offs;

Printk(("UMSDOS_notify_change: entering for %s/%s (%d)\n",
dentry->d_parent->d_name.name, dentry->d_name.name, inode->u.umsdos_i.i_patched));

	if (inode->i_nlink == 0)
		goto out;
	if (inode->i_ino == UMSDOS_ROOT_INO)
		goto out;

	/* get the EMD file dentry */
	demd = umsdos_get_emd_dentry(dentry->d_parent);
	ret = PTR_ERR(demd);
	if (IS_ERR(demd))
		goto out;
	ret = 0;
	/* don't do anything if directory is not promoted to umsdos yet */
	if (!demd->d_inode) { 
		Printk((KERN_DEBUG
			"UMSDOS_notify_change: no EMD file %s/%s\n",
			demd->d_parent->d_name.name, demd->d_name.name));
		goto out_dput;
	}

	/* don't do anything if this is the EMD itself */
	if (inode == demd->d_inode)
		goto out_dput;

	/* This inode is not a EMD file nor an inode used internally
	 * by MSDOS, so we can update its status.
	 * See emd.c
	 */

	/* Read only the start of the entry since we don't touch the name */
	mapping = demd->d_inode->i_mapping;
	offs = inode->u.umsdos_i.pos & ~PAGE_CACHE_MASK;
	ret = -ENOMEM;
	page=grab_cache_page(mapping,inode->u.umsdos_i.pos>>PAGE_CACHE_SHIFT);
	if (!page)
		goto out_dput;
	ret=mapping->a_ops->prepare_write(NULL,page,offs,offs+UMSDOS_REC_SIZE);
	if (ret)
		goto out_unlock;
	entry = (struct umsdos_dirent *) (page_address(page) + offs);
	if (attr->ia_valid & ATTR_UID)
		entry->uid = cpu_to_le16(attr->ia_uid);
	if (attr->ia_valid & ATTR_GID)
		entry->gid = cpu_to_le16(attr->ia_gid);
	if (attr->ia_valid & ATTR_MODE)
		entry->mode = cpu_to_le16(attr->ia_mode);
	if (attr->ia_valid & ATTR_ATIME)
		entry->atime = cpu_to_le32(attr->ia_atime);
	if (attr->ia_valid & ATTR_MTIME)
		entry->mtime = cpu_to_le32(attr->ia_mtime);
	if (attr->ia_valid & ATTR_CTIME)
		entry->ctime = cpu_to_le32(attr->ia_ctime);
	entry->nlink = cpu_to_le16(inode->i_nlink);
	ret=mapping->a_ops->commit_write(NULL,page,offs,offs+UMSDOS_REC_SIZE);
	if (ret)
		printk(KERN_WARNING
			"umsdos_notify_change: %s/%s EMD write error, ret=%d\n",
			dentry->d_parent->d_name.name, dentry->d_name.name,ret);

	/* #Specification: notify_change / msdos fs
	 * notify_change operation are done only on the
	 * EMD file. The msdos fs is not even called.
	 */
out_unlock:
	UnlockPage(page);
	page_cache_release(page);
out_dput:
	dput(demd);
out:
	return ret;
}


/*
 * Update the disk with the inode content
 */
void UMSDOS_write_inode (struct inode *inode, int wait)
{
	struct iattr newattrs;

	fat_write_inode (inode, wait);
	newattrs.ia_mtime = inode->i_mtime;
	newattrs.ia_atime = inode->i_atime;
	newattrs.ia_ctime = inode->i_ctime;
	newattrs.ia_valid = ATTR_MTIME | ATTR_ATIME | ATTR_CTIME;
	/*
	 * UMSDOS_notify_change is convenient to call here
	 * to update the EMD entry associated with this inode.
	 * But it has the side effect to re"dirt" the inode.
	 */
/*      
 * UMSDOS_notify_change (inode, &newattrs);

 * inode->i_state &= ~I_DIRTY; / * FIXME: this doesn't work.  We need to remove ourselves from list on dirty inodes. /mn/ */
}


static struct super_operations umsdos_sops =
{
	write_inode:	UMSDOS_write_inode,
	put_inode:	UMSDOS_put_inode,
	delete_inode:	fat_delete_inode,
	put_super:	UMSDOS_put_super,
	statfs:		UMSDOS_statfs,
	clear_inode:	fat_clear_inode,
};

int UMSDOS_statfs(struct super_block *sb,struct statfs *buf)
{
	int ret;
	ret = fat_statfs (sb, buf);
	if (!ret)	
		buf->f_namelen = UMSDOS_MAXNAME;
	return ret;
}

/*
 * Read the super block of an Extended MS-DOS FS.
 */
struct super_block *UMSDOS_read_super (struct super_block *sb, void *data,
				      int silent)
{
	struct super_block *res;
	struct dentry *new_root;

	/*
	 * Call msdos-fs to mount the disk.
	 * Note: this returns res == sb or NULL
	 */
	res = msdos_read_super (sb, data, silent);

	if (!res)
		goto out_fail;

	printk (KERN_INFO "UMSDOS 0.86k "
		"(compatibility level %d.%d, fast msdos)\n", 
		UMSDOS_VERSION, UMSDOS_RELEASE);

	sb->s_op = &umsdos_sops;
	MSDOS_SB(sb)->options.dotsOK = 0;	/* disable hidden==dotfile */

	/* install our dentry operations ... */
	sb->s_root->d_op = &umsdos_dentry_operations;

	umsdos_patch_dentry_inode(sb->s_root, 0);

	/* Check whether to change to the /linux root */
	new_root = check_pseudo_root(sb);

	if (new_root) {
		/* sanity check */
		if (new_root->d_op != &umsdos_dentry_operations)
			printk("umsdos_read_super: pseudo-root wrong ops!\n");

		pseudo_root = new_root->d_inode;
		saved_root = sb->s_root;
		printk(KERN_INFO "UMSDOS: changed to alternate root\n");
		dget (sb->s_root); sb->s_root = dget(new_root);
	}
	return sb;

out_fail:
	printk(KERN_INFO "UMSDOS: msdos_read_super failed, mount aborted.\n");
	return NULL;
}

/*
 * Check for an alternate root if we're the root device.
 */

extern kdev_t ROOT_DEV;
static struct dentry *check_pseudo_root(struct super_block *sb)
{
	struct dentry *root, *sbin, *init;

	/*
	 * Check whether we're mounted as the root device.
	 * must check like this, because we can be used with initrd
	 */
		
	if (sb->s_dev != ROOT_DEV)
		goto out_noroot;

	/* 
	 * lookup_dentry needs a (so far non-existent) root. 
	 */
	printk(KERN_INFO "check_pseudo_root: mounted as root\n");
	root = lookup_one_len(UMSDOS_PSDROOT_NAME, sb->s_root,UMSDOS_PSDROOT_LEN); 
	if (IS_ERR(root))
		goto out_noroot;
		
	if (!root->d_inode || !S_ISDIR(root->d_inode->i_mode))
		goto out_dput;

printk(KERN_INFO "check_pseudo_root: found %s/%s\n",
root->d_parent->d_name.name, root->d_name.name);

	/* look for /sbin/init */
	sbin = lookup_one_len("sbin", root, 4);
	if (IS_ERR(sbin))
		goto out_dput;
	if (!sbin->d_inode || !S_ISDIR(sbin->d_inode->i_mode))
		goto out_dput_sbin;
	init = lookup_one_len("init", sbin, 4);
	if (IS_ERR(init))
		goto out_dput_sbin;
	if (!init->d_inode)
		goto out_dput_init;
	printk(KERN_INFO "check_pseudo_root: found %s/%s, enabling pseudo-root\n", init->d_parent->d_name.name, init->d_name.name);
	dput(sbin);
	dput(init);
	return root;

	/* Alternate root not found ... */
out_dput_init:
	dput(init);
out_dput_sbin:
	dput(sbin);
out_dput:
	dput(root);
out_noroot:
	return NULL;
}


static DECLARE_FSTYPE_DEV(umsdos_fs_type, "umsdos", UMSDOS_read_super);

static int __init init_umsdos_fs (void)
{
	return register_filesystem (&umsdos_fs_type);
}

static void __exit exit_umsdos_fs (void)
{
	unregister_filesystem (&umsdos_fs_type);
}

EXPORT_NO_SYMBOLS;

module_init(init_umsdos_fs)
module_exit(exit_umsdos_fs)
MODULE_LICENSE("GPL");
