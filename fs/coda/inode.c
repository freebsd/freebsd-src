/*
 * Super block/filesystem wide operations
 *
 * Copyright (C) 1996 Peter J. Braam <braam@maths.ox.ac.uk> and 
 * Michael Callahan <callahan@maths.ox.ac.uk> 
 * 
 * Rewritten for Linux 2.1.  Peter Braam <braam@cs.cmu.edu>
 * Copyright (C) Carnegie Mellon University
 */

#define __NO_VERSION__
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/stat.h>
#include <linux/errno.h>
#include <linux/locks.h>
#include <linux/unistd.h>
#include <linux/smp_lock.h>
#include <linux/file.h>

#include <asm/system.h>
#include <asm/uaccess.h>

#include <linux/fs.h>
#include <linux/vmalloc.h>

#include <linux/coda.h>
#include <linux/coda_linux.h>
#include <linux/coda_psdev.h>
#include <linux/coda_fs_i.h>
#include <linux/coda_cache.h>

/* VFS super_block ops */
static struct super_block *coda_read_super(struct super_block *, void *, int);
static void coda_read_inode(struct inode *);
static void coda_clear_inode(struct inode *);
static void coda_put_super(struct super_block *);
static int coda_statfs(struct super_block *sb, struct statfs *buf);

/* exported operations */
struct super_operations coda_super_operations =
{
	read_inode:	coda_read_inode,
	clear_inode:	coda_clear_inode,
	put_super:	coda_put_super,
	statfs:		coda_statfs,
};

static int get_device_index(struct coda_mount_data *data)
{
	struct file *file;
	struct inode *inode;
	int idx;

	if(data == NULL) {
		printk("coda_read_super: Bad mount data\n");
		return -1;
	}

	if(data->version != CODA_MOUNT_VERSION) {
		printk("coda_read_super: Bad mount version\n");
		return -1;
	}

	file = fget(data->fd);
	inode = NULL;
	if(file)
		inode = file->f_dentry->d_inode;
	
	if(!inode || !S_ISCHR(inode->i_mode) ||
	   MAJOR(inode->i_rdev) != CODA_PSDEV_MAJOR) {
		if(file)
			fput(file);

		printk("coda_read_super: Bad file\n");
		return -1;
	}

	idx = MINOR(inode->i_rdev);
	fput(file);

	if(idx < 0 || idx >= MAX_CODADEVS) {
		printk("coda_read_super: Bad minor number\n");
		return -1;
	}

	return idx;
}

static struct super_block * coda_read_super(struct super_block *sb, 
					    void *data, int silent)
{
        struct inode *root = 0; 
	struct coda_sb_info *sbi = NULL;
	struct venus_comm *vc = NULL;
        ViceFid fid;
        int error;
	int idx;

	idx = get_device_index((struct coda_mount_data *) data);

	/* Ignore errors in data, for backward compatibility */
	if(idx == -1)
		idx = 0;
	
	printk(KERN_INFO "coda_read_super: device index: %i\n", idx);

	vc = &coda_comms[idx];
	if (!vc->vc_inuse) {
		printk("coda_read_super: No pseudo device\n");
		return NULL;
	}

        if ( vc->vc_sb ) {
		printk("coda_read_super: Device already mounted\n");
		return NULL;
	}

	sbi = kmalloc(sizeof(struct coda_sb_info), GFP_KERNEL);
	if(!sbi) {
		return NULL;
	}

	vc->vc_sb = sb;

	sbi->sbi_sb = sb;
	sbi->sbi_vcomm = vc;
	INIT_LIST_HEAD(&sbi->sbi_cihead);
	init_MUTEX(&sbi->sbi_iget4_mutex);

        sb->u.generic_sbp = sbi;
        sb->s_blocksize = 1024;	/* XXXXX  what do we put here?? */
        sb->s_blocksize_bits = 10;
        sb->s_magic = CODA_SUPER_MAGIC;
        sb->s_op = &coda_super_operations;

	/* get root fid from Venus: this needs the root inode */
	error = venus_rootfid(sb, &fid);
	if ( error ) {
	        printk("coda_read_super: coda_get_rootfid failed with %d\n",
		       error);
		goto error;
	}	  
	printk("coda_read_super: rootfid is %s\n", coda_f2s(&fid));
	
	/* make root inode */
        error = coda_cnode_make(&root, &fid, sb);
        if ( error || !root ) {
	    printk("Failure of coda_cnode_make for root: error %d\n", error);
	    goto error;
	} 

	printk("coda_read_super: rootinode is %ld dev %d\n", 
	       root->i_ino, root->i_dev);
	sb->s_root = d_alloc_root(root);
        return sb;

 error:
	if (sbi) {
		kfree(sbi);
		if(vc)
			vc->vc_sb = NULL;		
	}
	if (root)
                iput(root);

        return NULL;
}

static void coda_put_super(struct super_block *sb)
{
        struct coda_sb_info *sbi;

	sbi = coda_sbp(sb);
	sbi->sbi_vcomm->vc_sb = NULL;
        list_del_init(&sbi->sbi_cihead);

	printk("Coda: Bye bye.\n");
	kfree(sbi);
}

/* all filling in of inodes postponed until lookup */
static void coda_read_inode(struct inode *inode)
{
	struct coda_sb_info *sbi = coda_sbp(inode->i_sb);
	struct coda_inode_info *cii;

        if (!sbi) BUG();

	cii = ITOC(inode);
	if (!coda_isnullfid(&cii->c_fid)) {
            printk("coda_read_inode: initialized inode");
            return;
        }

	cii->c_mapcount = 0;
	list_add(&cii->c_cilist, &sbi->sbi_cihead);
}

static void coda_clear_inode(struct inode *inode)
{
	struct coda_inode_info *cii = ITOC(inode);

        CDEBUG(D_SUPER, " inode->ino: %ld, count: %d\n", 
	       inode->i_ino, atomic_read(&inode->i_count));        
	CDEBUG(D_DOWNCALL, "clearing inode: %ld, %x\n", inode->i_ino, cii->c_flags);

        list_del_init(&cii->c_cilist);
	coda_cache_clear_inode(inode);
}

int coda_notify_change(struct dentry *de, struct iattr *iattr)
{
	struct inode *inode = de->d_inode;
	struct coda_vattr vattr;
	int error;

	memset(&vattr, 0, sizeof(vattr)); 

	inode->i_ctime = CURRENT_TIME;
	coda_iattr_to_vattr(iattr, &vattr);
	vattr.va_type = C_VNON; /* cannot set type */
	CDEBUG(D_SUPER, "vattr.va_mode %o\n", vattr.va_mode);

	/* Venus is responsible for truncating the container-file!!! */
	error = venus_setattr(inode->i_sb, coda_i2f(inode), &vattr);

	if ( !error ) {
	        coda_vattr_to_iattr(inode, &vattr); 
		coda_cache_clear_inode(inode);
	}
	CDEBUG(D_SUPER, "inode.i_mode %o, error %d\n", inode->i_mode, error);

	return error;
}

struct inode_operations coda_file_inode_operations = {
	permission:	coda_permission,
	revalidate:	coda_revalidate_inode,
	setattr:	coda_notify_change,
};

static int coda_statfs(struct super_block *sb, struct statfs *buf)
{
	int error;

	error = venus_statfs(sb, buf);

	if (error) {
		/* fake something like AFS does */
		buf->f_blocks = 9000000;
		buf->f_bfree  = 9000000;
		buf->f_bavail = 9000000;
		buf->f_files  = 9000000;
		buf->f_ffree  = 9000000;
	}

	/* and fill in the rest */
	buf->f_type = CODA_SUPER_MAGIC;
	buf->f_bsize = 1024;
	buf->f_namelen = CODA_MAXNAMLEN;

	return 0; 
}

/* init_coda: used by filesystems.c to register coda */

DECLARE_FSTYPE( coda_fs_type, "coda", coda_read_super, 0);

