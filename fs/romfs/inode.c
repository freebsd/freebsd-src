/*
 * ROMFS file system, Linux implementation
 *
 * Copyright (C) 1997-1999  Janos Farkas <chexum@shadow.banki.hu>
 *
 * Using parts of the minix filesystem
 * Copyright (C) 1991, 1992  Linus Torvalds
 *
 * and parts of the affs filesystem additionally
 * Copyright (C) 1993  Ray Burr
 * Copyright (C) 1996  Hans-Joachim Widmaier
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 * Changes
 *					Changed for 2.1.19 modules
 *	Jan 1997			Initial release
 *	Jun 1997			2.1.43+ changes
 *					Proper page locking in readpage
 *					Changed to work with 2.1.45+ fs
 *	Jul 1997			Fixed follow_link
 *			2.1.47
 *					lookup shouldn't return -ENOENT
 *					from Horst von Brand:
 *					  fail on wrong checksum
 *					  double unlock_super was possible
 *					  correct namelen for statfs
 *					spotted by Bill Hawes:
 *					  readlink shouldn't iput()
 *	Jun 1998	2.1.106		from Avery Pennarun: glibc scandir()
 *					  exposed a problem in readdir
 *			2.1.107		code-freeze spellchecker run
 *	Aug 1998			2.1.118+ VFS changes
 *	Sep 1998	2.1.122		another VFS change (follow_link)
 *	Apr 1999	2.2.7		no more EBADF checking in
 *					  lookup/readdir, use ERR_PTR
 *	Jun 1999	2.3.6		d_alloc_root use changed
 *			2.3.9		clean up usage of ENOENT/negative
 *					  dentries in lookup
 *					clean up page flags setting
 *					  (error, uptodate, locking) in
 *					  in readpage
 *					use init_special_inode for
 *					  fifos/sockets (and streamline) in
 *					  read_inode, fix _ops table order
 *	Aug 1999	2.3.16		__initfunc() => __init change
 *	Oct 1999	2.3.24		page->owner hack obsoleted
 *	Nov 1999	2.3.27		2.3.25+ page->offset => index change
 */

/* todo:
 *	- see Documentation/filesystems/romfs.txt
 *	- use allocated, not stack memory for file names?
 *	- considering write access...
 *	- network (tftp) files?
 *	- merge back some _op tables
 */

/*
 * Sorry about some optimizations and for some goto's.  I just wanted
 * to squeeze some more bytes out of this code.. :)
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/romfs_fs.h>
#include <linux/fs.h>
#include <linux/locks.h>
#include <linux/init.h>
#include <linux/smp_lock.h>

#include <asm/uaccess.h>

static __s32
romfs_checksum(void *data, int size)
{
	__s32 sum, *ptr;

	sum = 0; ptr = data;
	size>>=2;
	while (size>0) {
		sum += ntohl(*ptr++);
		size--;
	}
	return sum;
}

static struct super_operations romfs_ops;

static struct super_block *
romfs_read_super(struct super_block *s, void *data, int silent)
{
	struct buffer_head *bh;
	kdev_t dev = s->s_dev;
	struct romfs_super_block *rsb;
	int sz;

	/* I would parse the options here, but there are none.. :) */

	set_blocksize(dev, ROMBSIZE);
	s->s_blocksize = ROMBSIZE;
	s->s_blocksize_bits = ROMBSBITS;
	s->u.generic_sbp = (void *) 0;
	s->s_maxbytes = 0xFFFFFFFF;

	bh = sb_bread(s, 0);
	if (!bh) {
		/* XXX merge with other printk? */
                printk ("romfs: unable to read superblock\n");
		goto outnobh;
	}

	rsb = (struct romfs_super_block *)bh->b_data;
	sz = ntohl(rsb->size);
	if (rsb->word0 != ROMSB_WORD0 || rsb->word1 != ROMSB_WORD1
	   || sz < ROMFH_SIZE) {
		if (!silent)
			printk ("VFS: Can't find a romfs filesystem on dev "
				"%s.\n", kdevname(dev));
		goto out;
	}
	if (romfs_checksum(rsb, min_t(int, sz, 512))) {
		printk ("romfs: bad initial checksum on dev "
			"%s.\n", kdevname(dev));
		goto out;
	}

	s->s_magic = ROMFS_MAGIC;
	s->u.romfs_sb.s_maxsize = sz;

	s->s_flags |= MS_RDONLY;

	/* Find the start of the fs */
	sz = (ROMFH_SIZE +
	      strnlen(rsb->name, ROMFS_MAXFN) + 1 + ROMFH_PAD)
	     & ROMFH_MASK;

	brelse(bh);

	s->s_op	= &romfs_ops;
	s->s_root = d_alloc_root(iget(s, sz));

	if (!s->s_root)
		goto outnobh;

	/* Ehrhm; sorry.. :)  And thanks to Hans-Joachim Widmaier  :) */
	if (0) {
out:
		brelse(bh);
outnobh:
		s = NULL;
	}

	return s;
}

/* That's simple too. */

static int
romfs_statfs(struct super_block *sb, struct statfs *buf)
{
	buf->f_type = ROMFS_MAGIC;
	buf->f_bsize = ROMBSIZE;
	buf->f_bfree = buf->f_bavail = buf->f_ffree;
	buf->f_blocks = (sb->u.romfs_sb.s_maxsize+ROMBSIZE-1)>>ROMBSBITS;
	buf->f_namelen = ROMFS_MAXFN;
	return 0;
}

/* some helper routines */

static int
romfs_strnlen(struct inode *i, unsigned long offset, unsigned long count)
{
	struct buffer_head *bh;
	unsigned long avail, maxsize, res;

	maxsize = i->i_sb->u.romfs_sb.s_maxsize;
	if (offset >= maxsize)
		return -1;

	/* strnlen is almost always valid */
	if (count > maxsize || offset+count > maxsize)
		count = maxsize-offset;

	bh = sb_bread(i->i_sb, offset>>ROMBSBITS);
	if (!bh)
		return -1;		/* error */

	avail = ROMBSIZE - (offset & ROMBMASK);
	maxsize = min_t(unsigned long, count, avail);
	res = strnlen(((char *)bh->b_data)+(offset&ROMBMASK), maxsize);
	brelse(bh);

	if (res < maxsize)
		return res;		/* found all of it */

	while (res < count) {
		offset += maxsize;

		bh = sb_bread(i->i_sb, offset>>ROMBSBITS);
		if (!bh)
			return -1;
		maxsize = min_t(unsigned long, count - res, ROMBSIZE);
		avail = strnlen(bh->b_data, maxsize);
		res += avail;
		brelse(bh);
		if (avail < maxsize)
			return res;
	}
	return res;
}

static int
romfs_copyfrom(struct inode *i, void *dest, unsigned long offset, unsigned long count)
{
	struct buffer_head *bh;
	unsigned long avail, maxsize, res;

	maxsize = i->i_sb->u.romfs_sb.s_maxsize;
	if (offset >= maxsize || count > maxsize || offset+count>maxsize)
		return -1;

	bh = sb_bread(i->i_sb, offset>>ROMBSBITS);
	if (!bh)
		return -1;		/* error */

	avail = ROMBSIZE - (offset & ROMBMASK);
	maxsize = min_t(unsigned long, count, avail);
	memcpy(dest, ((char *)bh->b_data) + (offset & ROMBMASK), maxsize);
	brelse(bh);

	res = maxsize;			/* all of it */

	while (res < count) {
		offset += maxsize;
		dest += maxsize;

		bh = sb_bread(i->i_sb, offset>>ROMBSBITS);
		if (!bh)
			return -1;
		maxsize = min_t(unsigned long, count - res, ROMBSIZE);
		memcpy(dest, bh->b_data, maxsize);
		brelse(bh);
		res += maxsize;
	}
	return res;
}

static unsigned char romfs_dtype_table[] = {
	DT_UNKNOWN, DT_DIR, DT_REG, DT_LNK, DT_BLK, DT_CHR, DT_SOCK, DT_FIFO
};

static int
romfs_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
	struct inode *i = filp->f_dentry->d_inode;
	struct romfs_inode ri;
	unsigned long offset, maxoff;
	int j, ino, nextfh;
	int stored = 0;
	char fsname[ROMFS_MAXFN];	/* XXX dynamic? */

	maxoff = i->i_sb->u.romfs_sb.s_maxsize;

	offset = filp->f_pos;
	if (!offset) {
		offset = i->i_ino & ROMFH_MASK;
		if (romfs_copyfrom(i, &ri, offset, ROMFH_SIZE) <= 0)
			return stored;
		offset = ntohl(ri.spec) & ROMFH_MASK;
	}

	/* Not really failsafe, but we are read-only... */
	for(;;) {
		if (!offset || offset >= maxoff) {
			offset = maxoff;
			filp->f_pos = offset;
			return stored;
		}
		filp->f_pos = offset;

		/* Fetch inode info */
		if (romfs_copyfrom(i, &ri, offset, ROMFH_SIZE) <= 0)
			return stored;

		j = romfs_strnlen(i, offset+ROMFH_SIZE, sizeof(fsname)-1);
		if (j < 0)
			return stored;

		fsname[j]=0;
		romfs_copyfrom(i, fsname, offset+ROMFH_SIZE, j);

		ino = offset;
		nextfh = ntohl(ri.next);
		if ((nextfh & ROMFH_TYPE) == ROMFH_HRD)
			ino = ntohl(ri.spec);
		if (filldir(dirent, fsname, j, offset, ino,
			    romfs_dtype_table[nextfh & ROMFH_TYPE]) < 0) {
			return stored;
		}
		stored++;
		offset = nextfh & ROMFH_MASK;
	}
}

static struct dentry *
romfs_lookup(struct inode *dir, struct dentry *dentry)
{
	unsigned long offset, maxoff;
	int fslen, res;
	struct inode *inode;
	char fsname[ROMFS_MAXFN];	/* XXX dynamic? */
	struct romfs_inode ri;
	const char *name;		/* got from dentry */
	int len;

	res = -EACCES;			/* placeholder for "no data here" */
	offset = dir->i_ino & ROMFH_MASK;
	if (romfs_copyfrom(dir, &ri, offset, ROMFH_SIZE) <= 0)
		goto out;

	maxoff = dir->i_sb->u.romfs_sb.s_maxsize;
	offset = ntohl(ri.spec) & ROMFH_MASK;

	/* OK, now find the file whose name is in "dentry" in the
	 * directory specified by "dir".  */

	name = dentry->d_name.name;
	len = dentry->d_name.len;

	for(;;) {
		if (!offset || offset >= maxoff)
			goto out0;
		if (romfs_copyfrom(dir, &ri, offset, ROMFH_SIZE) <= 0)
			goto out;

		/* try to match the first 16 bytes of name */
		fslen = romfs_strnlen(dir, offset+ROMFH_SIZE, ROMFH_SIZE);
		if (len < ROMFH_SIZE) {
			if (len == fslen) {
				/* both are shorter, and same size */
				romfs_copyfrom(dir, fsname, offset+ROMFH_SIZE, len+1);
				if (strncmp (name, fsname, len) == 0)
					break;
			}
		} else if (fslen >= ROMFH_SIZE) {
			/* both are longer; XXX optimize max size */
			fslen = romfs_strnlen(dir, offset+ROMFH_SIZE, sizeof(fsname)-1);
			if (len == fslen) {
				romfs_copyfrom(dir, fsname, offset+ROMFH_SIZE, len+1);
				if (strncmp(name, fsname, len) == 0)
					break;
			}
		}
		/* next entry */
		offset = ntohl(ri.next) & ROMFH_MASK;
	}

	/* Hard link handling */
	if ((ntohl(ri.next) & ROMFH_TYPE) == ROMFH_HRD)
		offset = ntohl(ri.spec) & ROMFH_MASK;

	if ((inode = iget(dir->i_sb, offset)))
		goto outi;

	/*
	 * it's a bit funky, _lookup needs to return an error code
	 * (negative) or a NULL, both as a dentry.  ENOENT should not
	 * be returned, instead we need to create a negative dentry by
	 * d_add(dentry, NULL); and return 0 as no error.
	 * (Although as I see, it only matters on writable file
	 * systems).
	 */

out0:	inode = NULL;
outi:	res = 0;
	d_add (dentry, inode);

out:	return ERR_PTR(res);
}

/*
 * Ok, we do readpage, to be able to execute programs.  Unfortunately,
 * we can't use bmap, since we may have looser alignments.
 */

static int
romfs_readpage(struct file *file, struct page * page)
{
	struct inode *inode = page->mapping->host;
	unsigned long offset, avail, readlen;
	void *buf;
	int result = -EIO;

	page_cache_get(page);
	lock_kernel();
	buf = kmap(page);
	if (!buf)
		goto err_out;

	/* 32 bit warning -- but not for us :) */
	offset = page->index << PAGE_CACHE_SHIFT;
	if (offset < inode->i_size) {
		avail = inode->i_size-offset;
		readlen = min_t(unsigned long, avail, PAGE_SIZE);
		if (romfs_copyfrom(inode, buf, inode->u.romfs_i.i_dataoffset+offset, readlen) == readlen) {
			if (readlen < PAGE_SIZE) {
				memset(buf + readlen,0,PAGE_SIZE-readlen);
			}
			SetPageUptodate(page);
			result = 0;
		}
	}
	if (result) {
		memset(buf, 0, PAGE_SIZE);
		SetPageError(page);
	}
	flush_dcache_page(page);

	UnlockPage(page);

	kunmap(page);
err_out:
	page_cache_release(page);
	unlock_kernel();

	return result;
}

/* Mapping from our types to the kernel */

static struct address_space_operations romfs_aops = {
	readpage: romfs_readpage
};

static struct file_operations romfs_dir_operations = {
	read:		generic_read_dir,
	readdir:	romfs_readdir,
};

static struct inode_operations romfs_dir_inode_operations = {
	lookup:		romfs_lookup,
};

static mode_t romfs_modemap[] =
{
	0, S_IFDIR+0644, S_IFREG+0644, S_IFLNK+0777,
	S_IFBLK+0600, S_IFCHR+0600, S_IFSOCK+0644, S_IFIFO+0644
};

static void
romfs_read_inode(struct inode *i)
{
	int nextfh, ino;
	struct romfs_inode ri;

	ino = i->i_ino & ROMFH_MASK;
	i->i_mode = 0;

	/* Loop for finding the real hard link */
	for(;;) {
		if (romfs_copyfrom(i, &ri, ino, ROMFH_SIZE) <= 0) {
			printk("romfs: read error for inode 0x%x\n", ino);
			return;
		}
		/* XXX: do romfs_checksum here too (with name) */

		nextfh = ntohl(ri.next);
		if ((nextfh & ROMFH_TYPE) != ROMFH_HRD)
			break;

		ino = ntohl(ri.spec) & ROMFH_MASK;
	}

	i->i_nlink = 1;		/* Hard to decide.. */
	i->i_size = ntohl(ri.size);
	i->i_mtime = i->i_atime = i->i_ctime = 0;
	i->i_uid = i->i_gid = 0;

        /* Precalculate the data offset */
        ino = romfs_strnlen(i, ino+ROMFH_SIZE, ROMFS_MAXFN);
        if (ino >= 0)
                ino = ((ROMFH_SIZE+ino+1+ROMFH_PAD)&ROMFH_MASK);
        else
                ino = 0;

        i->u.romfs_i.i_metasize = ino;
        i->u.romfs_i.i_dataoffset = ino+(i->i_ino&ROMFH_MASK);

        /* Compute permissions */
        ino = romfs_modemap[nextfh & ROMFH_TYPE];
	/* only "normal" files have ops */
	switch (nextfh & ROMFH_TYPE) {
		case 1:
			i->i_size = i->u.romfs_i.i_metasize;
			i->i_op = &romfs_dir_inode_operations;
			i->i_fop = &romfs_dir_operations;
			if (nextfh & ROMFH_EXEC)
				ino |= S_IXUGO;
			i->i_mode = ino;
			break;
		case 2:
			i->i_fop = &generic_ro_fops;
			i->i_data.a_ops = &romfs_aops;
			if (nextfh & ROMFH_EXEC)
				ino |= S_IXUGO;
			i->i_mode = ino;
			break;
		case 3:
			i->i_op = &page_symlink_inode_operations;
			i->i_data.a_ops = &romfs_aops;
			i->i_mode = ino | S_IRWXUGO;
			break;
		default:
			/* depending on MBZ for sock/fifos */
			nextfh = ntohl(ri.spec);
			nextfh = kdev_t_to_nr(MKDEV(nextfh>>16,nextfh&0xffff));
			init_special_inode(i, ino, nextfh);
	}
}

static struct super_operations romfs_ops = {
	read_inode:	romfs_read_inode,
	statfs:		romfs_statfs,
};

static DECLARE_FSTYPE_DEV(romfs_fs_type, "romfs", romfs_read_super);

static int __init init_romfs_fs(void)
{
	return register_filesystem(&romfs_fs_type);
}

static void __exit exit_romfs_fs(void)
{
	unregister_filesystem(&romfs_fs_type);
}

/* Yes, works even as a module... :) */

EXPORT_NO_SYMBOLS;

module_init(init_romfs_fs)
module_exit(exit_romfs_fs)
MODULE_LICENSE("GPL");
