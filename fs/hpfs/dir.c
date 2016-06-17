/*
 *  linux/fs/hpfs/dir.c
 *
 *  Mikulas Patocka (mikulas@artax.karlin.mff.cuni.cz), 1998-1999
 *
 *  directory VFS functions
 */

#include "hpfs_fn.h"
#include <linux/sched.h>
#include <linux/smp_lock.h>

int hpfs_dir_release(struct inode *inode, struct file *filp)
{
	lock_kernel();
	hpfs_del_pos(inode, &filp->f_pos);
	/*hpfs_write_if_changed(inode);*/
	unlock_kernel();
	return 0;
}

/* This is slow, but it's not used often */

loff_t hpfs_dir_lseek(struct file *filp, loff_t off, int whence)
{
	loff_t new_off = off + (whence == 1 ? filp->f_pos : 0);
	loff_t pos;
	struct quad_buffer_head qbh;
	struct inode *i = filp->f_dentry->d_inode;
	struct super_block *s = i->i_sb;
	/*printk("dir lseek\n");*/
	if (new_off == 0 || new_off == 1 || new_off == 11 || new_off == 12 || new_off == 13) goto ok;
	hpfs_lock_inode(i);
	pos = ((loff_t) hpfs_de_as_down_as_possible(s, i->i_hpfs_dno) << 4) + 1;
	while (pos != new_off) {
		if (map_pos_dirent(i, &pos, &qbh)) hpfs_brelse4(&qbh);
		else goto fail;
		if (pos == 12) goto fail;
	}
	hpfs_unlock_inode(i);
	ok:
	return filp->f_pos = new_off;
	fail:
	hpfs_unlock_inode(i);
	/*printk("illegal lseek: %016llx\n", new_off);*/
	return -ESPIPE;
}

int hpfs_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
	struct inode *inode = filp->f_dentry->d_inode;
	struct quad_buffer_head qbh;
	struct hpfs_dirent *de;
	int lc;
	long old_pos;
	char *tempname;
	int c1, c2 = 0;

	if (inode->i_sb->s_hpfs_chk) {
		if (hpfs_chk_sectors(inode->i_sb, inode->i_ino, 1, "dir_fnode"))
			return -EFSERROR;
		if (hpfs_chk_sectors(inode->i_sb, inode->i_hpfs_dno, 4, "dir_dnode"))
			return -EFSERROR;
	}
	if (inode->i_sb->s_hpfs_chk >= 2) {
		struct buffer_head *bh;
		struct fnode *fno;
		int e = 0;
		if (!(fno = hpfs_map_fnode(inode->i_sb, inode->i_ino, &bh)))
			return -EIOERROR;
		if (!fno->dirflag) {
			e = 1;
			hpfs_error(inode->i_sb, "not a directory, fnode %08x",inode->i_ino);
		}
		if (inode->i_hpfs_dno != fno->u.external[0].disk_secno) {
			e = 1;
			hpfs_error(inode->i_sb, "corrupted inode: i_hpfs_dno == %08x, fnode -> dnode == %08x", inode->i_hpfs_dno, fno->u.external[0].disk_secno);
		}
		brelse(bh);
		if (e) return -EFSERROR;
	}
	lc = inode->i_sb->s_hpfs_lowercase;
	if (filp->f_pos == 12) { /* diff -r requires this (note, that diff -r */
		filp->f_pos = 13; /* also fails on msdos filesystem in 2.0) */
		return 0;
	}
	if (filp->f_pos == 13) return -ENOENT;
	
	hpfs_lock_inode(inode);
	
	while (1) {
		again:
		/* This won't work when cycle is longer than number of dirents
		   accepted by filldir, but what can I do?
		   maybe killall -9 ls helps */
		if (inode->i_sb->s_hpfs_chk)
			if (hpfs_stop_cycles(inode->i_sb, filp->f_pos, &c1, &c2, "hpfs_readdir")) {
				hpfs_unlock_inode(inode);
				return -EFSERROR;
			}
		if (filp->f_pos == 12) {
			hpfs_unlock_inode(inode);
			return 0;
		}
		if (filp->f_pos == 3 || filp->f_pos == 4 || filp->f_pos == 5) {
			printk("HPFS: warning: pos==%d\n",(int)filp->f_pos);
			hpfs_unlock_inode(inode);
			return 0;
		}
		if (filp->f_pos == 0) {
			if (filldir(dirent, ".", 1, filp->f_pos, inode->i_ino, DT_DIR) < 0) {
				hpfs_unlock_inode(inode);
				return 0;
			}
			filp->f_pos = 11;
		}
		if (filp->f_pos == 11) {
			if (filldir(dirent, "..", 2, filp->f_pos, inode->i_hpfs_parent_dir, DT_DIR) < 0) {
				hpfs_unlock_inode(inode);
				return 0;
			}
			filp->f_pos = 1;
		}
		if (filp->f_pos == 1) {
			filp->f_pos = ((loff_t) hpfs_de_as_down_as_possible(inode->i_sb, inode->i_hpfs_dno) << 4) + 1;
			hpfs_add_pos(inode, &filp->f_pos);
			filp->f_version = inode->i_version;
		}
			/*if (filp->f_version != inode->i_version) {
				hpfs_unlock_inode(inode);
				return -ENOENT;
			}*/	
			old_pos = filp->f_pos;
			if (!(de = map_pos_dirent(inode, &filp->f_pos, &qbh))) {
				hpfs_unlock_inode(inode);
				return -EIOERROR;
			}
			if (de->first || de->last) {
				if (inode->i_sb->s_hpfs_chk) {
					if (de->first && !de->last && (de->namelen != 2 || de ->name[0] != 1 || de->name[1] != 1)) hpfs_error(inode->i_sb, "hpfs_readdir: bad ^A^A entry; pos = %08x", old_pos);
					if (de->last && (de->namelen != 1 || de ->name[0] != 255)) hpfs_error(inode->i_sb, "hpfs_readdir: bad \\377 entry; pos = %08x", old_pos);
				}
				hpfs_brelse4(&qbh);
				goto again;
			}
			tempname = hpfs_translate_name(inode->i_sb, de->name, de->namelen, lc, de->not_8x3);
			if (filldir(dirent, tempname, de->namelen, old_pos, de->fnode, DT_UNKNOWN) < 0) {
				filp->f_pos = old_pos;
				if (tempname != (char *)de->name) kfree(tempname);
				hpfs_brelse4(&qbh);
				hpfs_unlock_inode(inode);
				return 0;
			}
			if (tempname != (char *)de->name) kfree(tempname);
			hpfs_brelse4(&qbh);
	}
}

/*
 * lookup.  Search the specified directory for the specified name, set
 * *result to the corresponding inode.
 *
 * lookup uses the inode number to tell read_inode whether it is reading
 * the inode of a directory or a file -- file ino's are odd, directory
 * ino's are even.  read_inode avoids i/o for file inodes; everything
 * needed is up here in the directory.  (And file fnodes are out in
 * the boondocks.)
 *
 *    - M.P.: this is over, sometimes we've got to read file's fnode for eas
 *	      inode numbers are just fnode sector numbers; iget lock is used
 *	      to tell read_inode to read fnode or not.
 */

struct dentry *hpfs_lookup(struct inode *dir, struct dentry *dentry)
{
	const char *name = dentry->d_name.name;
	unsigned len = dentry->d_name.len;
	struct quad_buffer_head qbh;
	struct hpfs_dirent *de;
	ino_t ino;
	int err;
	struct inode *result = NULL;

	if ((err = hpfs_chk_name((char *)name, &len))) {
		if (err == -ENAMETOOLONG) return ERR_PTR(-ENAMETOOLONG);
		goto end_add;
	}

	hpfs_lock_inode(dir);
	/*
	 * '.' and '..' will never be passed here.
	 */

	de = map_dirent(dir, dir->i_hpfs_dno, (char *) name, len, NULL, &qbh);

	/*
	 * This is not really a bailout, just means file not found.
	 */

	if (!de) goto end;

	/*
	 * Get inode number, what we're after.
	 */

	ino = de->fnode;

	/*
	 * Go find or make an inode.
	 */

	hpfs_lock_iget(dir->i_sb, de->directory || (de->ea_size && dir->i_sb->s_hpfs_eas) ? 1 : 2);
	if (!(result = iget(dir->i_sb, ino))) {
		hpfs_unlock_iget(dir->i_sb);
		hpfs_error(dir->i_sb, "hpfs_lookup: can't get inode");
		goto bail1;
	}
	if (!de->directory) result->i_hpfs_parent_dir = dir->i_ino;
	hpfs_unlock_iget(dir->i_sb);

	hpfs_decide_conv(result, (char *)name, len);

	if (de->has_acl || de->has_xtd_perm) if (!(dir->i_sb->s_flags & MS_RDONLY)) {
		hpfs_error(result->i_sb, "ACLs or XPERM found. This is probably HPFS386. This driver doesn't support it now. Send me some info on these structures");
		goto bail1;
	}

	/*
	 * Fill in the info from the directory if this is a newly created
	 * inode.
	 */

	if (!result->i_ctime) {
		if (!(result->i_ctime = local_to_gmt(dir->i_sb, de->creation_date)))
			result->i_ctime = 1;
		result->i_mtime = local_to_gmt(dir->i_sb, de->write_date);
		result->i_atime = local_to_gmt(dir->i_sb, de->read_date);
		result->i_hpfs_ea_size = de->ea_size;
		if (!result->i_hpfs_ea_mode && de->read_only)
			result->i_mode &= ~0222;
		if (!de->directory) {
			if (result->i_size == -1) {
				result->i_size = de->file_size;
				result->i_data.a_ops = &hpfs_aops;
				result->u.hpfs_i.mmu_private = result->i_size;
			/*
			 * i_blocks should count the fnode and any anodes.
			 * We count 1 for the fnode and don't bother about
			 * anodes -- the disk heads are on the directory band
			 * and we want them to stay there.
			 */
				result->i_blocks = 1 + ((result->i_size + 511) >> 9);
			}
		}
	}

	hpfs_brelse4(&qbh);

	/*
	 * Made it.
	 */

	end:
	hpfs_unlock_inode(dir);
	end_add:
	hpfs_set_dentry_operations(dentry);
	d_add(dentry, result);
	return NULL;

	/*
	 * Didn't.
	 */
	bail1:
	
	hpfs_brelse4(&qbh);
	
	/*bail:*/

	hpfs_unlock_inode(dir);
	return ERR_PTR(-ENOENT);
}
