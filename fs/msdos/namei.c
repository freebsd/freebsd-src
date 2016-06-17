/*
 *  linux/fs/msdos/namei.c
 *
 *  Written 1992,1993 by Werner Almesberger
 *  Hidden files 1995 by Albert Cahalan <albert@ccs.neu.edu> <adc@coe.neu.edu>
 *  Rewritten for constant inumbers 1999 by Al Viro
 */


#define __NO_VERSION__
#include <linux/module.h>

#include <linux/sched.h>
#include <linux/msdos_fs.h>
#include <linux/errno.h>
#include <linux/string.h>

#include <asm/uaccess.h>

#define MSDOS_DEBUG 0
#define PRINTK(x)

/* MS-DOS "device special files" */

static const char *reserved_names[] = {
    "CON     ","PRN     ","NUL     ","AUX     ",
    "LPT1    ","LPT2    ","LPT3    ","LPT4    ",
    "COM1    ","COM2    ","COM3    ","COM4    ",
    NULL };


/* Characters that are undesirable in an MS-DOS file name */
  
static char bad_chars[] = "*?<>|\"";
static char bad_if_strict_pc[] = "+=,; ";
static char bad_if_strict_atari[] = " "; /* GEMDOS is less restrictive */
#define	bad_if_strict(opts) ((opts)->atari ? bad_if_strict_atari : bad_if_strict_pc)

/* Must die */
void msdos_put_super(struct super_block *sb)
{
	fat_put_super(sb);
}

/***** Formats an MS-DOS file name. Rejects invalid names. */
static int msdos_format_name(const char *name,int len,
	char *res,struct fat_mount_options *opts)
	/* name is the proposed name, len is its length, res is
	 * the resulting name, opts->name_check is either (r)elaxed,
	 * (n)ormal or (s)trict, opts->dotsOK allows dots at the
	 * beginning of name (for hidden files)
	 */
{
	char *walk;
	const char **reserved;
	unsigned char c;
	int space;

	if (name[0] == '.') {  /* dotfile because . and .. already done */
		if (opts->dotsOK) {
			/* Get rid of dot - test for it elsewhere */
			name++; len--;
		}
		else if (!opts->atari) return -EINVAL;
	}
	/* disallow names that _really_ start with a dot for MS-DOS, GEMDOS does
	 * not care */
	space = !opts->atari;
	c = 0;
	for (walk = res; len && walk-res < 8; walk++) {
	    	c = *name++;
		len--;
		if (opts->name_check != 'r' && strchr(bad_chars,c))
			return -EINVAL;
		if (opts->name_check == 's' && strchr(bad_if_strict(opts),c))
			return -EINVAL;
  		if (c >= 'A' && c <= 'Z' && opts->name_check == 's')
			return -EINVAL;
		if (c < ' ' || c == ':' || c == '\\') return -EINVAL;
/*  0xE5 is legal as a first character, but we must substitute 0x05     */
/*  because 0xE5 marks deleted files.  Yes, DOS really does this.       */
/*  It seems that Microsoft hacked DOS to support non-US characters     */
/*  after the 0xE5 character was already in use to mark deleted files.  */
		if((res==walk) && (c==0xE5)) c=0x05;
		if (c == '.') break;
		space = (c == ' ');
		*walk = (!opts->nocase && c >= 'a' && c <= 'z') ? c-32 : c;
	}
	if (space) return -EINVAL;
	if (opts->name_check == 's' && len && c != '.') {
		c = *name++;
		len--;
		if (c != '.') return -EINVAL;
	}
	while (c != '.' && len--) c = *name++;
	if (c == '.') {
		while (walk-res < 8) *walk++ = ' ';
		while (len > 0 && walk-res < MSDOS_NAME) {
			c = *name++;
			len--;
			if (opts->name_check != 'r' && strchr(bad_chars,c))
				return -EINVAL;
			if (opts->name_check == 's' &&
			    strchr(bad_if_strict(opts),c))
				return -EINVAL;
			if (c < ' ' || c == ':' || c == '\\')
				return -EINVAL;
			if (c == '.') {
				if (opts->name_check == 's')
					return -EINVAL;
				break;
			}
			if (c >= 'A' && c <= 'Z' && opts->name_check == 's')
				return -EINVAL;
			space = c == ' ';
			*walk++ = (!opts->nocase && c >= 'a' && c <= 'z') ? c-32 : c;
		}
		if (space) return -EINVAL;
		if (opts->name_check == 's' && len) return -EINVAL;
	}
	while (walk-res < MSDOS_NAME) *walk++ = ' ';
	if (!opts->atari)
		/* GEMDOS is less stupid and has no reserved names */
		for (reserved = reserved_names; *reserved; reserved++)
			if (!strncmp(res,*reserved,8)) return -EINVAL;
	return 0;
}

/***** Locates a directory entry.  Uses unformatted name. */
static int msdos_find(struct inode *dir, const char *name, int len,
		      struct buffer_head **bh, struct msdos_dir_entry **de,
		      loff_t *i_pos)
{
	int res;
	char dotsOK;
	char msdos_name[MSDOS_NAME];

	dotsOK = MSDOS_SB(dir->i_sb)->options.dotsOK;
	res = msdos_format_name(name,len, msdos_name,&MSDOS_SB(dir->i_sb)->options);
	if (res < 0)
		return -ENOENT;
	res = fat_scan(dir, msdos_name, bh, de, i_pos);
	if (!res && dotsOK) {
		if (name[0]=='.') {
			if (!((*de)->attr & ATTR_HIDDEN))
				res = -ENOENT;
		} else {
			if ((*de)->attr & ATTR_HIDDEN)
				res = -ENOENT;
		}
	}
	return res;
}

/*
 * Compute the hash for the msdos name corresponding to the dentry.
 * Note: if the name is invalid, we leave the hash code unchanged so
 * that the existing dentry can be used. The msdos fs routines will
 * return ENOENT or EINVAL as appropriate.
 */
static int msdos_hash(struct dentry *dentry, struct qstr *qstr)
{
	struct fat_mount_options *options = & (MSDOS_SB(dentry->d_sb)->options);
	int error;
	char msdos_name[MSDOS_NAME];
	
	error = msdos_format_name(qstr->name, qstr->len, msdos_name, options);
	if (!error)
		qstr->hash = full_name_hash(msdos_name, MSDOS_NAME);
	return 0;
}

/*
 * Compare two msdos names. If either of the names are invalid,
 * we fall back to doing the standard name comparison.
 */
static int msdos_cmp(struct dentry *dentry, struct qstr *a, struct qstr *b)
{
	struct fat_mount_options *options = & (MSDOS_SB(dentry->d_sb)->options);
	int error;
	char a_msdos_name[MSDOS_NAME], b_msdos_name[MSDOS_NAME];

	error = msdos_format_name(a->name, a->len, a_msdos_name, options);
	if (error)
		goto old_compare;
	error = msdos_format_name(b->name, b->len, b_msdos_name, options);
	if (error)
		goto old_compare;
	error = memcmp(a_msdos_name, b_msdos_name, MSDOS_NAME);
out:
	return error;

old_compare:
	error = 1;
	if (a->len == b->len)
		error = memcmp(a->name, b->name, a->len);
	goto out;
}


static struct dentry_operations msdos_dentry_operations = {
	d_hash:		msdos_hash,
	d_compare:	msdos_cmp,
};

/*
 * AV. Wrappers for FAT sb operations. Is it wise?
 */

/***** Get inode using directory and name */
struct dentry *msdos_lookup(struct inode *dir,struct dentry *dentry)
{
	struct super_block *sb = dir->i_sb;
	struct inode *inode = NULL;
	struct msdos_dir_entry *de;
	struct buffer_head *bh = NULL;
	loff_t i_pos;
	int res;
	
	PRINTK (("msdos_lookup\n"));

	dentry->d_op = &msdos_dentry_operations;

	res = msdos_find(dir, dentry->d_name.name, dentry->d_name.len, &bh,
			 &de, &i_pos);
	if (res == -ENOENT)
		goto add;
	if (res < 0)
		goto out;
	inode = fat_build_inode(sb, de, i_pos, &res);
	if (res)
		goto out;
add:
	d_add(dentry, inode);
	res = 0;
out:
	if (bh)
		fat_brelse(sb, bh);
	return ERR_PTR(res);
}

/***** Creates a directory entry (name is already formatted). */
static int msdos_add_entry(struct inode *dir, const char *name,
			   struct buffer_head **bh,
			   struct msdos_dir_entry **de,
			   loff_t *i_pos, int is_dir, int is_hid)
{
	struct super_block *sb = dir->i_sb;
	int res;

	res = fat_add_entries(dir, 1, bh, de, i_pos);
 	if (res < 0)
		return res;
	/*
	 * XXX all times should be set by caller upon successful completion.
	 */
	dir->i_ctime = dir->i_mtime = CURRENT_TIME;
	mark_inode_dirty(dir);
	memcpy((*de)->name,name,MSDOS_NAME);
	(*de)->attr = is_dir ? ATTR_DIR : ATTR_ARCH;
	if (is_hid)
		(*de)->attr |= ATTR_HIDDEN;
	(*de)->start = 0;
	(*de)->starthi = 0;
	fat_date_unix2dos(dir->i_mtime,&(*de)->time,&(*de)->date);
	(*de)->size = 0;
	fat_mark_buffer_dirty(sb, *bh);
	return 0;
}

/*
 * AV. Huh??? It's exported. Oughtta check usage.
 */

/***** Create a file */
int msdos_create(struct inode *dir,struct dentry *dentry,int mode)
{
	struct super_block *sb = dir->i_sb;
	struct buffer_head *bh;
	struct msdos_dir_entry *de;
	struct inode *inode;
	loff_t i_pos;
	int res, is_hid;
	char msdos_name[MSDOS_NAME];

	res = msdos_format_name(dentry->d_name.name,dentry->d_name.len,
				msdos_name, &MSDOS_SB(sb)->options);
	if (res < 0)
		return res;
	is_hid = (dentry->d_name.name[0]=='.') && (msdos_name[0]!='.');
	/* Have to do it due to foo vs. .foo conflicts */
	if (fat_scan(dir, msdos_name, &bh, &de, &i_pos) >= 0) {
		fat_brelse(sb, bh);
		return -EINVAL;
 	}
	inode = NULL;
	res = msdos_add_entry(dir, msdos_name, &bh, &de, &i_pos, 0, is_hid);
	if (res)
		return res;
	inode = fat_build_inode(dir->i_sb, de, i_pos, &res);
	fat_brelse(sb, bh);
	if (!inode)
		return res;
	inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME;
	mark_inode_dirty(inode);
	d_instantiate(dentry, inode);
	return 0;
}

/***** Remove a directory */
int msdos_rmdir(struct inode *dir, struct dentry *dentry)
{
	struct super_block *sb = dir->i_sb;
	struct inode *inode = dentry->d_inode;
	loff_t i_pos;
	int res;
	struct buffer_head *bh;
	struct msdos_dir_entry *de;

	bh = NULL;
	res = msdos_find(dir, dentry->d_name.name, dentry->d_name.len,
			 &bh, &de, &i_pos);
	if (res < 0)
		goto rmdir_done;
	/*
	 * Check whether the directory is not in use, then check
	 * whether it is empty.
	 */
	res = fat_dir_empty(inode);
	if (res)
		goto rmdir_done;

	de->name[0] = DELETED_FLAG;
	fat_mark_buffer_dirty(sb, bh);
	fat_detach(inode);
	inode->i_nlink = 0;
	inode->i_ctime = dir->i_ctime = dir->i_mtime = CURRENT_TIME;
	dir->i_nlink--;
	mark_inode_dirty(inode);
	mark_inode_dirty(dir);
	res = 0;

rmdir_done:
	fat_brelse(sb, bh);
	return res;
}

/***** Make a directory */
int msdos_mkdir(struct inode *dir,struct dentry *dentry,int mode)
{
	struct super_block *sb = dir->i_sb;
	struct buffer_head *bh;
	struct msdos_dir_entry *de;
	struct inode *inode;
	int res,is_hid;
	char msdos_name[MSDOS_NAME];
	loff_t i_pos;

	res = msdos_format_name(dentry->d_name.name,dentry->d_name.len,
				msdos_name, &MSDOS_SB(sb)->options);
	if (res < 0)
		return res;
	is_hid = (dentry->d_name.name[0]=='.') && (msdos_name[0]!='.');
	/* foo vs .foo situation */
	if (fat_scan(dir, msdos_name, &bh, &de, &i_pos) >= 0)
		goto out_exist;

	res = msdos_add_entry(dir, msdos_name, &bh, &de, &i_pos, 1, is_hid);
	if (res)
		goto out_unlock;
	inode = fat_build_inode(dir->i_sb, de, i_pos, &res);
	if (!inode) {
		fat_brelse(sb, bh);
		goto out_unlock;
	}
	res = 0;

	dir->i_nlink++;
	inode->i_nlink = 2; /* no need to mark them dirty */

	res = fat_new_dir(inode, dir, 0);
	if (res)
		goto mkdir_error;

	fat_brelse(sb, bh);
	d_instantiate(dentry, inode);
	res = 0;

out_unlock:
	return res;

mkdir_error:
	printk(KERN_WARNING "msdos_mkdir: error=%d, attempting cleanup\n", res);
	inode->i_nlink = 0;
	inode->i_ctime = dir->i_ctime = dir->i_mtime = CURRENT_TIME;
	dir->i_nlink--;
	mark_inode_dirty(inode);
	mark_inode_dirty(dir);
	de->name[0] = DELETED_FLAG;
	fat_mark_buffer_dirty(sb, bh);
	fat_brelse(sb, bh);
	fat_detach(inode);
	iput(inode);
	goto out_unlock;

out_exist:
	fat_brelse(sb, bh);
	res = -EINVAL;
	goto out_unlock;
}

/***** Unlink a file */
int msdos_unlink( struct inode *dir, struct dentry *dentry)
{
	struct super_block *sb = dir->i_sb;
	struct inode *inode = dentry->d_inode;
	loff_t i_pos;
	int res;
	struct buffer_head *bh;
	struct msdos_dir_entry *de;

	bh = NULL;
	res = msdos_find(dir, dentry->d_name.name, dentry->d_name.len,
			 &bh, &de, &i_pos);
	if (res < 0)
		goto unlink_done;

	de->name[0] = DELETED_FLAG;
	fat_mark_buffer_dirty(sb, bh);
	fat_detach(inode);
	fat_brelse(sb, bh);
	inode->i_nlink = 0;
	inode->i_ctime = dir->i_ctime = dir->i_mtime = CURRENT_TIME;
	mark_inode_dirty(inode);
	mark_inode_dirty(dir);
	res = 0;
unlink_done:
	return res;
}

static int do_msdos_rename(struct inode *old_dir, char *old_name,
    struct dentry *old_dentry,
    struct inode *new_dir,char *new_name, struct dentry *new_dentry,
    struct buffer_head *old_bh,
    struct msdos_dir_entry *old_de, loff_t old_i_pos, int is_hid)
{
	struct super_block *sb = old_dir->i_sb;
	struct buffer_head *new_bh=NULL,*dotdot_bh=NULL;
	struct msdos_dir_entry *new_de,*dotdot_de;
	struct inode *old_inode,*new_inode;
	loff_t new_i_pos, dotdot_i_pos;
	int error;
	int is_dir;

	old_inode = old_dentry->d_inode;
	new_inode = new_dentry->d_inode;
	is_dir = S_ISDIR(old_inode->i_mode);

	if (fat_scan(new_dir, new_name, &new_bh, &new_de, &new_i_pos) >= 0
	    && !new_inode)
		goto degenerate_case;
	if (is_dir) {
		if (new_inode) {
			error = fat_dir_empty(new_inode);
			if (error)
				goto out;
		}
		error = fat_scan(old_inode, MSDOS_DOTDOT, &dotdot_bh,
				&dotdot_de, &dotdot_i_pos);
		if (error < 0) {
			printk(KERN_WARNING
				"MSDOS: %s/%s, get dotdot failed, ret=%d\n",
				old_dentry->d_parent->d_name.name,
				old_dentry->d_name.name, error);
			goto out;
		}
	}
	if (!new_bh) {
		error = msdos_add_entry(new_dir, new_name, &new_bh, &new_de,
					&new_i_pos, is_dir, is_hid);
		if (error)
			goto out;
	}
	new_dir->i_version = ++event;

	/* There we go */

	if (new_inode)
		fat_detach(new_inode);
	old_de->name[0] = DELETED_FLAG;
	fat_mark_buffer_dirty(sb, old_bh);
	fat_detach(old_inode);
	fat_attach(old_inode, new_i_pos);
	if (is_hid)
		MSDOS_I(old_inode)->i_attrs |= ATTR_HIDDEN;
	else
		MSDOS_I(old_inode)->i_attrs &= ~ATTR_HIDDEN;
	mark_inode_dirty(old_inode);
	old_dir->i_version = ++event;
	old_dir->i_ctime = old_dir->i_mtime = CURRENT_TIME;
	mark_inode_dirty(old_dir);
	if (new_inode) {
		new_inode->i_nlink--;
		new_inode->i_ctime = CURRENT_TIME;
		mark_inode_dirty(new_inode);
	}
	if (dotdot_bh) {
		dotdot_de->start = CT_LE_W(MSDOS_I(new_dir)->i_logstart);
		dotdot_de->starthi = CT_LE_W((MSDOS_I(new_dir)->i_logstart) >> 16);
		fat_mark_buffer_dirty(sb, dotdot_bh);
		old_dir->i_nlink--;
		mark_inode_dirty(old_dir);
		if (new_inode) {
			new_inode->i_nlink--;
			mark_inode_dirty(new_inode);
		} else {
			new_dir->i_nlink++;
			mark_inode_dirty(new_dir);
		}
	}
	error = 0;
out:
	fat_brelse(sb, new_bh);
	fat_brelse(sb, dotdot_bh);
	return error;

degenerate_case:
	error = -EINVAL;
	if (new_de!=old_de)
		goto out;
	if (is_hid)
		MSDOS_I(old_inode)->i_attrs |= ATTR_HIDDEN;
	else
		MSDOS_I(old_inode)->i_attrs &= ~ATTR_HIDDEN;
	mark_inode_dirty(old_inode);
	old_dir->i_version = ++event;
	old_dir->i_ctime = old_dir->i_mtime = CURRENT_TIME;
	mark_inode_dirty(old_dir);
	return 0;
}

/***** Rename, a wrapper for rename_same_dir & rename_diff_dir */
int msdos_rename(struct inode *old_dir,struct dentry *old_dentry,
		 struct inode *new_dir,struct dentry *new_dentry)
{
	struct super_block *sb = old_dir->i_sb;
	struct buffer_head *old_bh;
	struct msdos_dir_entry *old_de;
	loff_t old_i_pos;
	int error, is_hid, old_hid; /* if new file and old file are hidden */
	char old_msdos_name[MSDOS_NAME], new_msdos_name[MSDOS_NAME];

	error = msdos_format_name(old_dentry->d_name.name,
				  old_dentry->d_name.len,old_msdos_name,
				  &MSDOS_SB(old_dir->i_sb)->options);
	if (error < 0)
		goto rename_done;
	error = msdos_format_name(new_dentry->d_name.name,
				  new_dentry->d_name.len,new_msdos_name,
				  &MSDOS_SB(new_dir->i_sb)->options);
	if (error < 0)
		goto rename_done;

	is_hid  = (new_dentry->d_name.name[0]=='.') && (new_msdos_name[0]!='.');
	old_hid = (old_dentry->d_name.name[0]=='.') && (old_msdos_name[0]!='.');
	error = fat_scan(old_dir, old_msdos_name, &old_bh, &old_de, &old_i_pos);
	if (error < 0)
		goto rename_done;

	error = do_msdos_rename(old_dir, old_msdos_name, old_dentry,
				new_dir, new_msdos_name, new_dentry,
				old_bh, old_de, old_i_pos, is_hid);
	fat_brelse(sb, old_bh);

rename_done:
	return error;
}


/* The public inode operations for the msdos fs */
struct inode_operations msdos_dir_inode_operations = {
	create:		msdos_create,
	lookup:		msdos_lookup,
	unlink:		msdos_unlink,
	mkdir:		msdos_mkdir,
	rmdir:		msdos_rmdir,
	rename:		msdos_rename,
	setattr:	fat_notify_change,
};

struct super_block *msdos_read_super(struct super_block *sb,void *data, int silent)
{
	struct super_block *res;

	MSDOS_SB(sb)->options.isvfat = 0;
	res = fat_read_super(sb, data, silent, &msdos_dir_inode_operations);
	if (res)
		sb->s_root->d_op = &msdos_dentry_operations;
	return res;
}
