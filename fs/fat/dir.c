/*
 *  linux/fs/fat/dir.c
 *
 *  directory handling functions for fat-based filesystems
 *
 *  Written 1992,1993 by Werner Almesberger
 *
 *  Hidden files 1995 by Albert Cahalan <albert@ccs.neu.edu> <adc@coe.neu.edu>
 *
 *  VFAT extensions by Gordon Chaffee <chaffee@plateau.cs.berkeley.edu>
 *  Merged with msdos fs by Henrik Storner <storner@osiris.ping.dk>
 *  Rewritten for constant inumbers. Plugged buffer overrun in readdir(). AV
 *  Short name translation 1999, 2001 by Wolfram Pienkoss <wp@bszh.de>
 */

#include <linux/fs.h>
#include <linux/msdos_fs.h>
#include <linux/nls.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/ioctl.h>
#include <linux/dirent.h>
#include <linux/mm.h>
#include <linux/ctype.h>

#include <asm/uaccess.h>

#define PRINTK(X)

struct file_operations fat_dir_operations = {
	read:		generic_read_dir,
	readdir:	fat_readdir,
	ioctl:		fat_dir_ioctl,
	fsync:		file_fsync,
};

/*
 * Convert Unicode 16 to UTF8, translated Unicode, or ASCII.
 * If uni_xlate is enabled and we can't get a 1:1 conversion, use a
 * colon as an escape character since it is normally invalid on the vfat
 * filesystem. The following four characters are the hexadecimal digits
 * of Unicode value. This lets us do a full dump and restore of Unicode
 * filenames. We could get into some trouble with long Unicode names,
 * but ignore that right now.
 * Ahem... Stack smashing in ring 0 isn't fun. Fixed.
 */
static int
uni16_to_x8(unsigned char *ascii, wchar_t *uni, int uni_xlate,
	    struct nls_table *nls)
{
	wchar_t *ip, ec;
	unsigned char *op, nc;
	int charlen;
	int k;

	ip = uni;
	op = ascii;

	while (*ip) {
		ec = *ip++;
		if ( (charlen = nls->uni2char(ec, op, NLS_MAX_CHARSET_SIZE)) > 0) {
			op += charlen;
		} else {
			if (uni_xlate == 1) {
				*op = ':';
				for (k = 4; k > 0; k--) {
					nc = ec & 0xF;
					op[k] = nc > 9	? nc + ('a' - 10)
							: nc + '0';
					ec >>= 4;
				}
				op += 5;
			} else {
				*op++ = '?';
			}
		}
		/* We have some slack there, so it's OK */
		if (op>ascii+256) {
			op = ascii + 256;
			break;
		}
	}
	*op = 0;
	return (op - ascii);
}

#if 0
static void dump_de(struct msdos_dir_entry *de)
{
	int i;
	unsigned char *p = (unsigned char *) de;
	printk("[");

	for (i = 0; i < 32; i++, p++) {
		printk("%02x ", *p);
	}
	printk("]\n");
}
#endif

static inline unsigned char
fat_tolower(struct nls_table *t, unsigned char c)
{
	unsigned char nc = t->charset2lower[c];

	return nc ? nc : c;
}

static inline int
fat_short2uni(struct nls_table *t, unsigned char *c, int clen, wchar_t *uni)
{
	int charlen;

	charlen = t->char2uni(c, clen, uni);
	if (charlen < 0) {
		*uni = 0x003f;	/* a question mark */
		charlen = 1;
	}
	return charlen;
}

static inline int
fat_short2lower_uni(struct nls_table *t, unsigned char *c, int clen, wchar_t *uni)
{
	int charlen;
	wchar_t wc;

	charlen = t->char2uni(c, clen, &wc);
	if (charlen < 0) {
		*uni = 0x003f;	/* a question mark */
		charlen = 1;
	} else if (charlen <= 1) {
		unsigned char nc = t->charset2lower[*c];
		
		if (!nc)
			nc = *c;
		
		if ( (charlen = t->char2uni(&nc, 1, uni)) < 0) {
			*uni = 0x003f;	/* a question mark */
			charlen = 1;
		}
	} else
		*uni = wc;
	
	return charlen;
}

static int
fat_strnicmp(struct nls_table *t, const unsigned char *s1,
					const unsigned char *s2, int len)
{
	while(len--)
		if (fat_tolower(t, *s1++) != fat_tolower(t, *s2++))
			return 1;

	return 0;
}

static inline int
fat_shortname2uni(struct nls_table *nls, char *buf, int buf_size,
		  wchar_t *uni_buf, unsigned short opt, int lower)
{
	int len = 0;

	if (opt & VFAT_SFN_DISPLAY_LOWER)
		len =  fat_short2lower_uni(nls, buf, buf_size, uni_buf);
	else if (opt & VFAT_SFN_DISPLAY_WIN95)
		len = fat_short2uni(nls, buf, buf_size, uni_buf);
	else if (opt & VFAT_SFN_DISPLAY_WINNT) {
		if (lower)
			len = fat_short2lower_uni(nls, buf, buf_size, uni_buf);
		else 
			len = fat_short2uni(nls, buf, buf_size, uni_buf);
	} else
		len = fat_short2uni(nls, buf, buf_size, uni_buf);

	return len;
}

/*
 * Return values: negative -> error, 0 -> not found, positive -> found,
 * value is the total amount of slots, including the shortname entry.
 */
int fat_search_long(struct inode *inode, const char *name, int name_len,
			int anycase, loff_t *spos, loff_t *lpos)
{
	struct super_block *sb = inode->i_sb;
	struct buffer_head *bh = NULL;
	struct msdos_dir_entry *de;
	struct nls_table *nls_io = MSDOS_SB(sb)->nls_io;
	struct nls_table *nls_disk = MSDOS_SB(sb)->nls_disk;
	wchar_t bufuname[14];
	unsigned char xlate_len, long_slots;
	wchar_t *unicode = NULL;
	char work[8], bufname[260];	/* 256 + 4 */
	int uni_xlate = MSDOS_SB(sb)->options.unicode_xlate;
	int utf8 = MSDOS_SB(sb)->options.utf8;
	unsigned short opt_shortname = MSDOS_SB(sb)->options.shortname;
	int chl, i, j, last_u, res = 0;
	loff_t i_pos, cpos = 0;

	while(1) {
		if (fat_get_entry(inode,&cpos,&bh,&de,&i_pos) == -1)
			goto EODir;
parse_record:
		long_slots = 0;
		if (de->name[0] == (__s8) DELETED_FLAG)
			continue;
		if (de->attr != ATTR_EXT && (de->attr & ATTR_VOLUME))
			continue;
		if (de->attr != ATTR_EXT && IS_FREE(de->name))
			continue;
		if (de->attr == ATTR_EXT) {
			struct msdos_dir_slot *ds;
			unsigned char id;
			unsigned char slot;
			unsigned char slots;
			unsigned char sum;
			unsigned char alias_checksum;

			if (!unicode) {
				unicode = (wchar_t *)
					__get_free_page(GFP_KERNEL);
				if (!unicode) {
					fat_brelse(sb, bh);
					return -ENOMEM;
				}
			}
parse_long:
			slots = 0;
			ds = (struct msdos_dir_slot *) de;
			id = ds->id;
			if (!(id & 0x40))
				continue;
			slots = id & ~0x40;
			if (slots > 20 || !slots)	/* ceil(256 * 2 / 26) */
				continue;
			long_slots = slots;
			alias_checksum = ds->alias_checksum;

			slot = slots;
			while (1) {
				int offset;

				slot--;
				offset = slot * 13;
				fat16_towchar(unicode + offset, ds->name0_4, 5);
				fat16_towchar(unicode + offset + 5, ds->name5_10, 6);
				fat16_towchar(unicode + offset + 11, ds->name11_12, 2);

				if (ds->id & 0x40) {
					unicode[offset + 13] = 0;
				}
				if (fat_get_entry(inode,&cpos,&bh,&de,&i_pos)<0)
					goto EODir;
				if (slot == 0)
					break;
				ds = (struct msdos_dir_slot *) de;
				if (ds->attr !=  ATTR_EXT)
					goto parse_record;
				if ((ds->id & ~0x40) != slot)
					goto parse_long;
				if (ds->alias_checksum != alias_checksum)
					goto parse_long;
			}
			if (de->name[0] == (__s8) DELETED_FLAG)
				continue;
			if (de->attr ==  ATTR_EXT)
				goto parse_long;
			if (IS_FREE(de->name) || (de->attr & ATTR_VOLUME))
				continue;
			for (sum = 0, i = 0; i < 11; i++)
				sum = (((sum&1)<<7)|((sum&0xfe)>>1)) + de->name[i];
			if (sum != alias_checksum)
				long_slots = 0;
		}

		for (i = 0; i < 8; i++) {
			/* see namei.c, msdos_format_name */
			if (de->name[i] == 0x05)
				work[i] = 0xE5;
			else
				work[i] = de->name[i];
		}
		for (i = 0, j = 0, last_u = 0; i < 8;) {
			if (!work[i]) break;
			chl = fat_shortname2uni(nls_disk, &work[i], 8 - i,
						&bufuname[j++], opt_shortname,
						de->lcase & CASE_LOWER_BASE);
			if (chl <= 1) {
				if (work[i] != ' ')
					last_u = j;
			} else {
				last_u = j;
			}
			i += chl;
		}
		j = last_u;
		fat_short2uni(nls_disk, ".", 1, &bufuname[j++]);
		for (i = 0; i < 3;) {
			if (!de->ext[i]) break;
			chl = fat_shortname2uni(nls_disk, &de->ext[i], 3 - i,
						&bufuname[j++], opt_shortname,
						de->lcase & CASE_LOWER_EXT);
			if (chl <= 1) {
				if (de->ext[i] != ' ')
					last_u = j;
			} else {
				last_u = j;
			}
			i += chl;
		}
		if (!last_u)
			continue;

		bufuname[last_u] = 0x0000;
		xlate_len = utf8
			?utf8_wcstombs(bufname, bufuname, sizeof(bufname))
			:uni16_to_x8(bufname, bufuname, uni_xlate, nls_io);
		if (xlate_len == name_len)
			if ((!anycase && !memcmp(name, bufname, xlate_len)) ||
			    (anycase && !fat_strnicmp(nls_io, name, bufname,
								xlate_len)))
				goto Found;

		if (long_slots) {
			xlate_len = utf8
				?utf8_wcstombs(bufname, unicode, sizeof(bufname))
				:uni16_to_x8(bufname, unicode, uni_xlate, nls_io);
			if (xlate_len != name_len)
				continue;
			if ((!anycase && !memcmp(name, bufname, xlate_len)) ||
			    (anycase && !fat_strnicmp(nls_io, name, bufname,
								xlate_len)))
				goto Found;
		}
	}

Found:
	res = long_slots + 1;
	*spos = cpos - sizeof(struct msdos_dir_entry);
	*lpos = cpos - res*sizeof(struct msdos_dir_entry);
EODir:
	fat_brelse(sb, bh);
	if (unicode) {
		free_page((unsigned long) unicode);
	}
	return res;
}

static int fat_readdirx(struct inode *inode, struct file *filp, void *dirent,
			filldir_t filldir, int shortnames, int both)
{
	struct super_block *sb = inode->i_sb;
	struct buffer_head *bh;
	struct msdos_dir_entry *de;
	struct nls_table *nls_io = MSDOS_SB(sb)->nls_io;
	struct nls_table *nls_disk = MSDOS_SB(sb)->nls_disk;
	wchar_t bufuname[14];
	unsigned char long_slots;
	wchar_t *unicode = NULL;
	char c, work[8], bufname[56], *ptname = bufname;
	unsigned long lpos, dummy, *furrfu = &lpos;
	int uni_xlate = MSDOS_SB(sb)->options.unicode_xlate;
	int isvfat = MSDOS_SB(sb)->options.isvfat;
	int utf8 = MSDOS_SB(sb)->options.utf8;
	int nocase = MSDOS_SB(sb)->options.nocase;
	unsigned short opt_shortname = MSDOS_SB(sb)->options.shortname;
	unsigned long inum;
	int chi, chl, i, i2, j, last, last_u, dotoffset = 0;
	loff_t i_pos, cpos;

	cpos = filp->f_pos;
/* Fake . and .. for the root directory. */
	if (inode->i_ino == MSDOS_ROOT_INO) {
		while (cpos < 2) {
			if (filldir(dirent, "..", cpos+1, cpos, MSDOS_ROOT_INO, DT_DIR) < 0)
				return 0;
			cpos++;
			filp->f_pos++;
		}
		if (cpos == 2) {
			dummy = 2;
			furrfu = &dummy;
			cpos = 0;
		}
	}
	if (cpos & (sizeof(struct msdos_dir_entry)-1))
		return -ENOENT;

 	bh = NULL;
GetNew:
	long_slots = 0;
	if (fat_get_entry(inode,&cpos,&bh,&de,&i_pos) == -1)
		goto EODir;
	/* Check for long filename entry */
	if (isvfat) {
		if (de->name[0] == (__s8) DELETED_FLAG)
			goto RecEnd;
		if (de->attr != ATTR_EXT && (de->attr & ATTR_VOLUME))
			goto RecEnd;
		if (de->attr != ATTR_EXT && IS_FREE(de->name))
			goto RecEnd;
	} else {
		if ((de->attr & ATTR_VOLUME) || IS_FREE(de->name))
			goto RecEnd;
	}

	if (isvfat && de->attr == ATTR_EXT) {
		struct msdos_dir_slot *ds;
		unsigned char id;
		unsigned char slot;
		unsigned char slots;
		unsigned char sum;
		unsigned char alias_checksum;

		if (!unicode) {
			unicode = (wchar_t *)
				__get_free_page(GFP_KERNEL);
			if (!unicode) {
				filp->f_pos = cpos;
				fat_brelse(sb, bh);
				return -ENOMEM;
			}
		}
ParseLong:
		slots = 0;
		ds = (struct msdos_dir_slot *) de;
		id = ds->id;
		if (!(id & 0x40))
			goto RecEnd;
		slots = id & ~0x40;
		if (slots > 20 || !slots)	/* ceil(256 * 2 / 26) */
			goto RecEnd;
		long_slots = slots;
		alias_checksum = ds->alias_checksum;

		slot = slots;
		while (1) {
			int offset;

			slot--;
			offset = slot * 13;
			fat16_towchar(unicode + offset, ds->name0_4, 5);
			fat16_towchar(unicode + offset + 5, ds->name5_10, 6);
			fat16_towchar(unicode + offset + 11, ds->name11_12, 2);

			if (ds->id & 0x40) {
				unicode[offset + 13] = 0;
			}
			if (fat_get_entry(inode,&cpos,&bh,&de,&i_pos) == -1)
				goto EODir;
			if (slot == 0)
				break;
			ds = (struct msdos_dir_slot *) de;
			if (ds->attr !=  ATTR_EXT)
				goto RecEnd;	/* XXX */
			if ((ds->id & ~0x40) != slot)
				goto ParseLong;
			if (ds->alias_checksum != alias_checksum)
				goto ParseLong;
		}
		if (de->name[0] == (__s8) DELETED_FLAG)
			goto RecEnd;
		if (de->attr ==  ATTR_EXT)
			goto ParseLong;
		if (IS_FREE(de->name) || (de->attr & ATTR_VOLUME))
			goto RecEnd;
		for (sum = 0, i = 0; i < 11; i++)
			sum = (((sum&1)<<7)|((sum&0xfe)>>1)) + de->name[i];
		if (sum != alias_checksum)
			long_slots = 0;
	}

	if ((de->attr & ATTR_HIDDEN) && MSDOS_SB(sb)->options.dotsOK) {
		*ptname++ = '.';
		dotoffset = 1;
	}

	for (i = 0; i < 8; i++) {
		/* see namei.c, msdos_format_name */
		if (de->name[i] == 0x05)
			work[i] = 0xE5;
		else
			work[i] = de->name[i];
	}
	for (i = 0, j = 0, last = 0, last_u = 0; i < 8;) {
		if (!(c = work[i])) break;
		chl = fat_shortname2uni(nls_disk, &work[i], 8 - i,
					&bufuname[j++], opt_shortname,
					de->lcase & CASE_LOWER_BASE);
		if (chl <= 1) {
			ptname[i++] = (!nocase && c>='A' && c<='Z') ? c+32 : c;
			if (c != ' ') {
				last = i;
				last_u = j;
			}
		} else {
			last_u = j;
			for (chi = 0; chi < chl && i < 8; chi++) {
				ptname[i] = work[i];
				i++; last = i;
			}
		}
	}
	i = last;
	j = last_u;
	fat_short2uni(nls_disk, ".", 1, &bufuname[j++]);
	ptname[i++] = '.';
	for (i2 = 0; i2 < 3;) {
		if (!(c = de->ext[i2])) break;
		chl = fat_shortname2uni(nls_disk, &de->ext[i2], 3 - i2,
					&bufuname[j++], opt_shortname,
					de->lcase & CASE_LOWER_EXT);
		if (chl <= 1) {
			i2++;
			ptname[i++] = (!nocase && c>='A' && c<='Z') ? c+32 : c;
			if (c != ' ') {
				last = i;
				last_u = j;
			}
		} else {
			last_u = j;
			for (chi = 0; chi < chl && i2 < 3; chi++) {
				ptname[i++] = de->ext[i2++];
				last = i;
			}
		}
	}
	if (!last)
		goto RecEnd;

	i = last + dotoffset;
	j = last_u;

	lpos = cpos - (long_slots+1)*sizeof(struct msdos_dir_entry);
	if (!memcmp(de->name,MSDOS_DOT,11))
		inum = inode->i_ino;
	else if (!memcmp(de->name,MSDOS_DOTDOT,11)) {
/*		inum = fat_parent_ino(inode,0); */
		inum = filp->f_dentry->d_parent->d_inode->i_ino;
	} else {
		struct inode *tmp = fat_iget(sb, i_pos);
		if (tmp) {
			inum = tmp->i_ino;
			iput(tmp);
		} else
			inum = iunique(sb, MSDOS_ROOT_INO);
	}

	if (isvfat) {
		bufuname[j] = 0x0000;
		i = utf8 ? utf8_wcstombs(bufname, bufuname, sizeof(bufname))
			 : uni16_to_x8(bufname, bufuname, uni_xlate, nls_io);
	}

	if (!long_slots||shortnames) {
		if (both)
			bufname[i] = '\0';
		if (filldir(dirent, bufname, i, *furrfu, inum,
			    (de->attr & ATTR_DIR) ? DT_DIR : DT_REG) < 0)
			goto FillFailed;
	} else {
		char longname[275];
		int long_len = utf8
			? utf8_wcstombs(longname, unicode, sizeof(longname))
			: uni16_to_x8(longname, unicode, uni_xlate,
				      nls_io);
		if (both) {
			memcpy(&longname[long_len+1], bufname, i);
			long_len += i;
		}
		if (filldir(dirent, longname, long_len, *furrfu, inum,
			    (de->attr & ATTR_DIR) ? DT_DIR : DT_REG) < 0)
			goto FillFailed;
	}

RecEnd:
	furrfu = &lpos;
	filp->f_pos = cpos;
	goto GetNew;
EODir:
	filp->f_pos = cpos;
FillFailed:
	if (bh)
		fat_brelse(sb, bh);
	if (unicode) {
		free_page((unsigned long) unicode);
	}
	return 0;
}

int fat_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
	struct inode *inode = filp->f_dentry->d_inode;
	return fat_readdirx(inode, filp, dirent, filldir, 0, 0);
}

static int vfat_ioctl_fill(
	void * buf,
	const char * name,
	int name_len,
	loff_t offset,
	ino_t ino,
	unsigned int d_type)
{
	struct dirent *d1 = (struct dirent *)buf;
	struct dirent *d2 = d1 + 1;
	int len, slen;
	int dotdir;

	get_user(len, &d1->d_reclen);
	if (len != 0) {
		return -1;
	}

	if ((name_len == 1 && name[0] == '.') ||
	    (name_len == 2 && name[0] == '.' && name[1] == '.')) {
		dotdir = 1;
		len = name_len;
	} else {
		dotdir = 0;
		len = strlen(name);
	}
	if (len != name_len) {
		copy_to_user(d2->d_name, name, len);
		put_user(0, d2->d_name + len);
		put_user(len, &d2->d_reclen);
		put_user(ino, &d2->d_ino);
		put_user(offset, &d2->d_off);
		slen = name_len - len;
		copy_to_user(d1->d_name, name+len+1, slen);
		put_user(0, d1->d_name+slen);
		put_user(slen, &d1->d_reclen);
	} else {
		put_user(0, d2->d_name);
		put_user(0, &d2->d_reclen);
		copy_to_user(d1->d_name, name, len);
		put_user(0, d1->d_name+len);
		put_user(len, &d1->d_reclen);
	}
	PRINTK(("FAT d1=%p d2=%p len=%d, name_len=%d\n",
		d1, d2, len, name_len));

	return 0;
}

int fat_dir_ioctl(struct inode * inode, struct file * filp,
		  unsigned int cmd, unsigned long arg)
{
	int err;
	/*
	 * We want to provide an interface for Samba to be able
	 * to get the short filename for a given long filename.
	 * Samba should use this ioctl instead of readdir() to
	 * get the information it needs.
	 */
	switch (cmd) {
	case VFAT_IOCTL_READDIR_BOTH: {
		struct dirent *d1 = (struct dirent *)arg;
		err = verify_area(VERIFY_WRITE, d1, sizeof(struct dirent[2]));
		if (err)
			return err;
		put_user(0, &d1->d_reclen);
		return fat_readdirx(inode,filp,(void *)arg,
				    vfat_ioctl_fill, 0, 1);
	}
	case VFAT_IOCTL_READDIR_SHORT: {
		struct dirent *d1 = (struct dirent *)arg;
		put_user(0, &d1->d_reclen);
		err = verify_area(VERIFY_WRITE, d1, sizeof(struct dirent[2]));
		if (err)
			return err;
		return fat_readdirx(inode,filp,(void *)arg,
				    vfat_ioctl_fill, 1, 1);
	}
	default:
		/* forward ioctl to CVF extension */
	       if (MSDOS_SB(inode->i_sb)->cvf_format &&
		   MSDOS_SB(inode->i_sb)->cvf_format->cvf_dir_ioctl)
		       return MSDOS_SB(inode->i_sb)->cvf_format
			       ->cvf_dir_ioctl(inode,filp,cmd,arg);
		return -EINVAL;
	}

	return 0;
}

/***** See if directory is empty */
int fat_dir_empty(struct inode *dir)
{
	loff_t pos, i_pos;
	struct buffer_head *bh;
	struct msdos_dir_entry *de;
	int result = 0;

	pos = 0;
	bh = NULL;
	while (fat_get_entry(dir,&pos,&bh,&de,&i_pos) > -1) {
		/* Ignore vfat longname entries */
		if (de->attr == ATTR_EXT)
			continue;
		if (!IS_FREE(de->name) && 
		    strncmp(de->name,MSDOS_DOT   , MSDOS_NAME) &&
		    strncmp(de->name,MSDOS_DOTDOT, MSDOS_NAME)) {
			result = -ENOTEMPTY;
			break;
		}
	}
	if (bh)
		fat_brelse(dir->i_sb, bh);

	return result;
}

/* This assumes that size of cluster is above the 32*slots */

int fat_add_entries(struct inode *dir,int slots, struct buffer_head **bh,
		  struct msdos_dir_entry **de, loff_t *i_pos)
{
	struct super_block *sb = dir->i_sb;
	loff_t offset, curr;
	int row;
	struct buffer_head *new_bh;

	offset = curr = 0;
	*bh = NULL;
	row = 0;
	while (fat_get_entry(dir,&curr,bh,de,i_pos) > -1) {
		if (IS_FREE((*de)->name)) {
			if (++row == slots)
				return offset;
		} else {
			row = 0;
			offset = curr;
		}
	}
	if ((dir->i_ino == MSDOS_ROOT_INO) && (MSDOS_SB(sb)->fat_bits != 32)) 
		return -ENOSPC;
	new_bh = fat_extend_dir(dir);
	if (!new_bh)
		return -ENOSPC;
	fat_brelse(sb, new_bh);
	do fat_get_entry(dir,&curr,bh,de,i_pos); while (++row<slots);
	return offset;
}

int fat_new_dir(struct inode *dir, struct inode *parent, int is_vfat)
{
	struct super_block *sb = dir->i_sb;
	struct buffer_head *bh;
	struct msdos_dir_entry *de;
	__u16 date, time;

	if ((bh = fat_extend_dir(dir)) == NULL) return -ENOSPC;
	/* zeroed out, so... */
	fat_date_unix2dos(dir->i_mtime,&time,&date);
	de = (struct msdos_dir_entry*)&bh->b_data[0];
	memcpy(de[0].name,MSDOS_DOT,MSDOS_NAME);
	memcpy(de[1].name,MSDOS_DOTDOT,MSDOS_NAME);
	de[0].attr = de[1].attr = ATTR_DIR;
	de[0].time = de[1].time = CT_LE_W(time);
	de[0].date = de[1].date = CT_LE_W(date);
	if (is_vfat) {	/* extra timestamps */
		de[0].ctime = de[1].ctime = CT_LE_W(time);
		de[0].adate = de[0].cdate =
			de[1].adate = de[1].cdate = CT_LE_W(date);
	}
	de[0].start = CT_LE_W(MSDOS_I(dir)->i_logstart);
	de[0].starthi = CT_LE_W(MSDOS_I(dir)->i_logstart>>16);
	de[1].start = CT_LE_W(MSDOS_I(parent)->i_logstart);
	de[1].starthi = CT_LE_W(MSDOS_I(parent)->i_logstart>>16);
	fat_mark_buffer_dirty(sb, bh);
	fat_brelse(sb, bh);
	dir->i_atime = dir->i_ctime = dir->i_mtime = CURRENT_TIME;
	mark_inode_dirty(dir);

	return 0;
}

/*
 * Overrides for Emacs so that we follow Linus's tabbing style.
 * Emacs will notice this stuff at the end of the file and automatically
 * adjust the settings for this buffer only.  This must remain at the end
 * of the file.
 * ---------------------------------------------------------------------------
 * Local variables:
 * c-indent-level: 8
 * c-brace-imaginary-offset: 0
 * c-brace-offset: -8
 * c-argdecl-indent: 8
 * c-label-offset: -8
 * c-continued-statement-offset: 8
 * c-continued-brace-offset: 0
 * End:
 */
