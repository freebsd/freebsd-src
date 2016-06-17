/*
 *  linux/fs/ncpfs/symlink.c
 *
 *  Code for allowing symbolic links on NCPFS (i.e. NetWare)
 *  Symbolic links are not supported on native NetWare, so we use an
 *  infrequently-used flag (Sh) and store a two-word magic header in
 *  the file to make sure we don't accidentally use a non-link file
 *  as a link.
 *
 *  from linux/fs/ext2/symlink.c
 *
 *  Copyright (C) 1998-99, Frank A. Vorstenbosch
 *
 *  ncpfs symlink handling code
 *  NLS support (c) 1999 Petr Vandrovec
 *
 */

#include <linux/config.h>

#ifdef CONFIG_NCPFS_EXTRAS

#include <asm/uaccess.h>
#include <asm/segment.h>

#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/ncp_fs.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/stat.h>
#include "ncplib_kernel.h"


/* these magic numbers must appear in the symlink file -- this makes it a bit
   more resilient against the magic attributes being set on random files. */

#define NCP_SYMLINK_MAGIC0	le32_to_cpu(0x6c6d7973)     /* "symlnk->" */
#define NCP_SYMLINK_MAGIC1	le32_to_cpu(0x3e2d6b6e)

int ncp_create_new(struct inode *dir, struct dentry *dentry,
                          int mode,int attributes);

/* ----- read a symbolic link ------------------------------------------ */

static int ncp_symlink_readpage(struct file *file, struct page *page)
{
	struct inode *inode = page->mapping->host;
	int error, length, len, cnt;
	char *link;
	char *buf = kmap(page);

	error = -ENOMEM;
	for (cnt = 0; (link=(char *)kmalloc(NCP_MAX_SYMLINK_SIZE, GFP_NFS))==NULL; cnt++) {
		if (cnt > 10)
			goto fail;
		schedule();
	}

	if (ncp_make_open(inode,O_RDONLY))
		goto failEIO;

	error=ncp_read_kernel(NCP_SERVER(inode),NCP_FINFO(inode)->file_handle,
                         0,NCP_MAX_SYMLINK_SIZE,link,&length);

	ncp_inode_close(inode);
	/* Close file handle if no other users... */
	ncp_make_closed(inode);
	if (error)
		goto failEIO;

	if (length<NCP_MIN_SYMLINK_SIZE || 
	    ((__u32 *)link)[0]!=NCP_SYMLINK_MAGIC0 ||
	    ((__u32 *)link)[1]!=NCP_SYMLINK_MAGIC1)
	    	goto failEIO;

	len = NCP_MAX_SYMLINK_SIZE;
	error = ncp_vol2io(NCP_SERVER(inode), buf, &len, link+8, length-8, 0);
	kfree(link);
	if (error)
		goto fail;
	SetPageUptodate(page);
	kunmap(page);
	UnlockPage(page);
	return 0;

failEIO:
	error = -EIO;
	kfree(link);
fail:
	SetPageError(page);
	kunmap(page);
	UnlockPage(page);
	return error;
}

/*
 * symlinks can't do much...
 */
struct address_space_operations ncp_symlink_aops = {
	readpage:	ncp_symlink_readpage,
};
	
/* ----- create a new symbolic link -------------------------------------- */
 
int ncp_symlink(struct inode *dir, struct dentry *dentry, const char *symname) {
	struct inode *inode;
	char *link;
	int length, err, i;

#ifdef DEBUG
	PRINTK("ncp_symlink(dir=%p,dentry=%p,symname=%s)\n",dir,dentry,symname);
#endif

	if (!(NCP_SERVER(dir)->m.flags & NCP_MOUNT_SYMLINKS))
		return -EPERM;	/* EPERM is returned by VFS if symlink procedure does not exist */

	if ((length=strlen(symname))>NCP_MAX_SYMLINK_SIZE-8)
		return -EINVAL;

	if ((link=(char *)kmalloc(length+9,GFP_NFS))==NULL)
		return -ENOMEM;

	err = -EIO;
	if (ncp_create_new(dir,dentry,0,aSHARED|aHIDDEN))
		goto failfree;

	inode=dentry->d_inode;

	if (ncp_make_open(inode, O_WRONLY))
		goto failfree;

	((__u32 *)link)[0]=NCP_SYMLINK_MAGIC0;
	((__u32 *)link)[1]=NCP_SYMLINK_MAGIC1;

	/* map to/from server charset, do not touch upper/lower case as
	   symlink can point out of ncp filesystem */
	length += 1;
	err = ncp_io2vol(NCP_SERVER(inode),link+8,&length,symname,length-1,0);
	if (err)
		goto fail;

	if(ncp_write_kernel(NCP_SERVER(inode), NCP_FINFO(inode)->file_handle, 
	    		    0, length+8, link, &i) || i!=length+8) {
		err = -EIO;
		goto fail;
	}

	ncp_inode_close(inode);
	ncp_make_closed(inode);
	kfree(link);
	return 0;

fail:
	ncp_inode_close(inode);
	ncp_make_closed(inode);
failfree:
	kfree(link);
	return err;	
}
#endif

/* ----- EOF ----- */
