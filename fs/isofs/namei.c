/*
 *  linux/fs/isofs/namei.c
 *
 *  (C) 1992  Eric Youngdale Modified for ISO 9660 filesystem.
 *
 *  (C) 1991  Linus Torvalds - minix filesystem
 */

#include <linux/sched.h>
#include <linux/iso_fs.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/stat.h>
#include <linux/fcntl.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/config.h>	/* Joliet? */

#include <asm/uaccess.h>

/*
 * ok, we cannot use strncmp, as the name is not in our data space.
 * Thus we'll have to use isofs_match. No big problem. Match also makes
 * some sanity tests.
 */
static int
isofs_cmp(struct dentry * dentry, const char * compare, int dlen)
{
	struct qstr qstr;

	if (!compare)
		return 1;

	/* check special "." and ".." files */
	if (dlen == 1) {
		/* "." */
		if (compare[0] == 0) {
			if (!dentry->d_name.len)
				return 0;
			compare = ".";
		} else if (compare[0] == 1) {
			compare = "..";
			dlen = 2;
		}
	}

	qstr.name = compare;
	qstr.len = dlen;
	return dentry->d_op->d_compare(dentry, &dentry->d_name, &qstr);
}

/*
 *	isofs_find_entry()
 *
 * finds an entry in the specified directory with the wanted name. It
 * returns the inode number of the found entry, or 0 on error.
 */
static unsigned long
isofs_find_entry(struct inode *dir, struct dentry *dentry,
	char * tmpname, struct iso_directory_record * tmpde)
{
	unsigned long inode_number;
	unsigned long bufsize = ISOFS_BUFFER_SIZE(dir);
	unsigned char bufbits = ISOFS_BUFFER_BITS(dir);
	unsigned int block, f_pos, offset;
	struct buffer_head * bh = NULL;

	if (!dir->u.isofs_i.i_first_extent)
		return 0;
  
	f_pos = 0;
	offset = 0;
	block = 0;

	while (f_pos < dir->i_size) {
		struct iso_directory_record * de;
		int de_len, match, i, dlen;
		char *dpnt;

		if (!bh) {
			bh = isofs_bread(dir, block);
			if (!bh)
				return 0;
		}

		de = (struct iso_directory_record *) (bh->b_data + offset);
		inode_number = (bh->b_blocknr << bufbits) + offset;

		de_len = *(unsigned char *) de;
		if (!de_len) {
			brelse(bh);
			bh = NULL;
			f_pos = (f_pos + ISOFS_BLOCK_SIZE) & ~(ISOFS_BLOCK_SIZE - 1);
			block = f_pos >> bufbits;
			offset = 0;
			continue;
		}

		offset += de_len;
		f_pos += de_len;

		/* Make sure we have a full directory entry */
		if (offset >= bufsize) {
			int slop = bufsize - offset + de_len;
			memcpy(tmpde, de, slop);
			offset &= bufsize - 1;
			block++;
			brelse(bh);
			bh = NULL;
			if (offset) {
				bh = isofs_bread(dir, block);
				if (!bh)
					return 0;
				memcpy((void *) tmpde + slop, bh->b_data, offset);
			}
			de = tmpde;
		}

		dlen = de->name_len[0];
		dpnt = de->name;

		if (dir->i_sb->u.isofs_sb.s_rock &&
		    ((i = get_rock_ridge_filename(de, tmpname, dir)))) {
			dlen = i; 	/* possibly -1 */
			dpnt = tmpname;
#ifdef CONFIG_JOLIET
		} else if (dir->i_sb->u.isofs_sb.s_joliet_level) {
			dlen = get_joliet_filename(de, tmpname, dir);
			dpnt = tmpname;
#endif
		} else if (dir->i_sb->u.isofs_sb.s_mapping == 'a') {
			dlen = get_acorn_filename(de, tmpname, dir);
			dpnt = tmpname;
		} else if (dir->i_sb->u.isofs_sb.s_mapping == 'n') {
			dlen = isofs_name_translate(de, tmpname, dir);
			dpnt = tmpname;
		}

		/*
		 * Skip hidden or associated files unless unhide is set 
		 */
		match = 0;
		if (dlen > 0 &&
		    (!(de->flags[-dir->i_sb->u.isofs_sb.s_high_sierra] & 5)
		     || dir->i_sb->u.isofs_sb.s_unhide == 'y'))
		{
			match = (isofs_cmp(dentry,dpnt,dlen) == 0);
		}
		if (match) {
			if (bh) brelse(bh);
			return inode_number;
		}
	}
	if (bh) brelse(bh);
	return 0;
}

struct dentry *isofs_lookup(struct inode * dir, struct dentry * dentry)
{
	unsigned long ino;
	struct inode *inode;
	struct page *page;

	dentry->d_op = dir->i_sb->s_root->d_op;

	page = alloc_page(GFP_USER);
	if (!page)
		return ERR_PTR(-ENOMEM);

	ino = isofs_find_entry(dir, dentry, page_address(page),
			       1024 + page_address(page));
	__free_page(page);

	inode = NULL;
	if (ino) {
		inode = iget(dir->i_sb, ino);
		if (!inode)
			return ERR_PTR(-EACCES);
	}
	d_add(dentry, inode);
	return NULL;
}
