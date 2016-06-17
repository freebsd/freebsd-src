/*
 *  linux/fs/isofs/dir.c
 *
 *  (C) 1992, 1993, 1994  Eric Youngdale Modified for ISO 9660 filesystem.
 *
 *  (C) 1991  Linus Torvalds - minix filesystem
 *
 *  Steve Beynon		       : Missing last directory entries fixed
 *  (stephen@askone.demon.co.uk)      : 21st June 1996
 * 
 *  isofs directory handling functions
 */
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/iso_fs.h>
#include <linux/kernel.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/locks.h>
#include <linux/config.h>

#include <asm/uaccess.h>

static int isofs_readdir(struct file *, void *, filldir_t);

struct file_operations isofs_dir_operations =
{
	read:		generic_read_dir,
	readdir:	isofs_readdir,
};

/*
 * directories can handle most operations...
 */
struct inode_operations isofs_dir_inode_operations =
{
	lookup:		isofs_lookup,
};

int isofs_name_translate(struct iso_directory_record *de, char *new, struct inode *inode)
{
	char * old = de->name;
	int len = de->name_len[0];
	int i;
			
	for (i = 0; i < len; i++) {
		unsigned char c = old[i];
		if (!c)
			break;

		if (c >= 'A' && c <= 'Z')
			c |= 0x20;	/* lower case */

		/* Drop trailing '.;1' (ISO 9660:1988 7.5.1 requires period) */
		if (c == '.' && i == len - 3 && old[i + 1] == ';' && old[i + 2] == '1')
			break;

		/* Drop trailing ';1' */
		if (c == ';' && i == len - 2 && old[i + 1] == '1')
			break;

		/* Convert remaining ';' to '.' */
		if (c == ';')
			c = '.';

		new[i] = c;
	}
	return i;
}

/* Acorn extensions written by Matthew Wilcox <willy@bofh.ai> 1998 */
int get_acorn_filename(struct iso_directory_record * de,
			    char * retname, struct inode * inode)
{
	int std;
	unsigned char * chr;
	int retnamlen = isofs_name_translate(de, retname, inode);
	if (retnamlen == 0) return 0;
	std = sizeof(struct iso_directory_record) + de->name_len[0];
	if (std & 1) std++;
	if ((*((unsigned char *) de) - std) != 32) return retnamlen;
	chr = ((unsigned char *) de) + std;
	if (strncmp(chr, "ARCHIMEDES", 10)) return retnamlen;
	if ((*retname == '_') && ((chr[19] & 1) == 1)) *retname = '!';
	if (((de->flags[0] & 2) == 0) && (chr[13] == 0xff)
		&& ((chr[12] & 0xf0) == 0xf0))
	{
		retname[retnamlen] = ',';
		sprintf(retname+retnamlen+1, "%3.3x",
			((chr[12] & 0xf) << 8) | chr[11]);
		retnamlen += 4;
	}
	return retnamlen;
}

/*
 * This should _really_ be cleaned up some day..
 */
static int do_isofs_readdir(struct inode *inode, struct file *filp,
		void *dirent, filldir_t filldir,
		char * tmpname, struct iso_directory_record * tmpde)
{
	unsigned long bufsize = ISOFS_BUFFER_SIZE(inode);
	unsigned char bufbits = ISOFS_BUFFER_BITS(inode);
	unsigned int block, offset;
	int inode_number = 0;	/* Quiet GCC */
	struct buffer_head *bh = NULL;
	int len;
	int map;
	int high_sierra;
	int first_de = 1;
	char *p = NULL;		/* Quiet GCC */
	struct iso_directory_record *de;

	offset = filp->f_pos & (bufsize - 1);
	block = filp->f_pos >> bufbits;
	high_sierra = inode->i_sb->u.isofs_sb.s_high_sierra;

	while (filp->f_pos < inode->i_size) {
		int de_len;

		if (!bh) {
			bh = isofs_bread(inode, block);
			if (!bh)
				return 0;
		}

		de = (struct iso_directory_record *) (bh->b_data + offset);
		if (first_de)
			inode_number = (bh->b_blocknr << bufbits) + offset;

		de_len = *(unsigned char *) de;

		/* If the length byte is zero, we should move on to the next
		   CDROM sector.  If we are at the end of the directory, we
		   kick out of the while loop. */

		if (de_len == 0) {
			brelse(bh);
			bh = NULL;
			filp->f_pos = (filp->f_pos + ISOFS_BLOCK_SIZE) & ~(ISOFS_BLOCK_SIZE - 1);
			block = filp->f_pos >> bufbits;
			offset = 0;
			continue;
		}

		offset += de_len;

		/* Make sure we have a full directory entry */
		if (offset >= bufsize) {
			int slop = bufsize - offset + de_len;
			memcpy(tmpde, de, slop);
			offset &= bufsize - 1;
			block++;
			brelse(bh);
			bh = NULL;
			if (offset) {
				bh = isofs_bread(inode, block);
				if (!bh)
					return 0;
				memcpy((void *) tmpde + slop, bh->b_data, offset);
			}
			de = tmpde;
		}

		if (de->flags[-high_sierra] & 0x80) {
			first_de = 0;
			filp->f_pos += de_len;
			continue;
		}
		first_de = 1;

		/* Handle the case of the '.' directory */
		if (de->name_len[0] == 1 && de->name[0] == 0) {
			if (filldir(dirent, ".", 1, filp->f_pos, inode->i_ino, DT_DIR) < 0)
				break;
			filp->f_pos += de_len;
			continue;
		}

		len = 0;

		/* Handle the case of the '..' directory */
		if (de->name_len[0] == 1 && de->name[0] == 1) {
			inode_number = filp->f_dentry->d_parent->d_inode->i_ino;
			if (filldir(dirent, "..", 2, filp->f_pos, inode_number, DT_DIR) < 0)
				break;
			filp->f_pos += de_len;
			continue;
		}

		/* Handle everything else.  Do name translation if there
		   is no Rock Ridge NM field. */
		if (inode->i_sb->u.isofs_sb.s_unhide == 'n') {
			/* Do not report hidden or associated files */
			if (de->flags[-high_sierra] & 5) {
				filp->f_pos += de_len;
				continue;
			}
		}

		map = 1;
		if (inode->i_sb->u.isofs_sb.s_rock) {
			len = get_rock_ridge_filename(de, tmpname, inode);
			if (len != 0) {		/* may be -1 */
				p = tmpname;
				map = 0;
			}
		}
		if (map) {
#ifdef CONFIG_JOLIET
			if (inode->i_sb->u.isofs_sb.s_joliet_level) {
				len = get_joliet_filename(de, tmpname, inode);
				p = tmpname;
			} else
#endif
			if (inode->i_sb->u.isofs_sb.s_mapping == 'a') {
				len = get_acorn_filename(de, tmpname, inode);
				p = tmpname;
			} else
			if (inode->i_sb->u.isofs_sb.s_mapping == 'n') {
				len = isofs_name_translate(de, tmpname, inode);
				p = tmpname;
			} else {
				p = de->name;
				len = de->name_len[0];
			}
		}
		if (len > 0) {
			if (filldir(dirent, p, len, filp->f_pos, inode_number, DT_UNKNOWN) < 0)
				break;
		}
		filp->f_pos += de_len;

		continue;
	}
	if (bh) brelse(bh);
	return 0;
}

/*
 * Handle allocation of temporary space for name translation and
 * handling split directory entries.. The real work is done by
 * "do_isofs_readdir()".
 */
static int isofs_readdir(struct file *filp,
		void *dirent, filldir_t filldir)
{
	int result;
	char * tmpname;
	struct iso_directory_record * tmpde;
	struct inode *inode = filp->f_dentry->d_inode;

	tmpname = (char *) __get_free_page(GFP_KERNEL);
	if (!tmpname)
		return -ENOMEM;
	tmpde = (struct iso_directory_record *) (tmpname+1024);

	result = do_isofs_readdir(inode, filp, dirent, filldir, tmpname, tmpde);

	free_page((unsigned long) tmpname);
	return result;
}
