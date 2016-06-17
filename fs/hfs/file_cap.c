/*
 * linux/fs/hfs/file_cap.c
 *
 * Copyright (C) 1995-1997  Paul H. Hargrove
 * This file may be distributed under the terms of the GNU General Public License.
 *
 * This file contains the file_ops and inode_ops for the metadata
 * files under the CAP representation.
 *
 * The source code distribution of the Columbia AppleTalk Package for
 * UNIX, version 6.0, (CAP) was used as a specification of the
 * location and format of files used by CAP's Aufs.  No code from CAP
 * appears in hfs_fs.  hfs_fs is not a work ``derived'' from CAP in
 * the sense of intellectual property law.
 *
 * "XXX" in a comment is a note to myself to consider changing something.
 *
 * In function preconditions the term "valid" applied to a pointer to
 * a structure means that the pointer is non-NULL and the structure it
 * points to has all fields initialized to consistent values.
 */

#include "hfs.h"
#include <linux/hfs_fs_sb.h>
#include <linux/hfs_fs_i.h>
#include <linux/hfs_fs.h>

/*================ Forward declarations ================*/
static loff_t      cap_info_llseek(struct file *, loff_t,
                                   int);
static hfs_rwret_t cap_info_read(struct file *, char *,
				 hfs_rwarg_t, loff_t *);
static hfs_rwret_t cap_info_write(struct file *, const char *,
				  hfs_rwarg_t, loff_t *);
/*================ Function-like macros ================*/

/*
 * OVERLAPS()
 *
 * Determines if a given range overlaps the specified structure member
 */
#define OVERLAPS(START, END, TYPE, MEMB) \
	((END > offsetof(TYPE, MEMB)) && \
	 (START < offsetof(TYPE, MEMB) + sizeof(((TYPE *)0)->MEMB)))

/*================ Global variables ================*/

struct file_operations hfs_cap_info_operations = {
	llseek:		cap_info_llseek,
	read:		cap_info_read,
	write:		cap_info_write,
	fsync:		file_fsync,
};

struct inode_operations hfs_cap_info_inode_operations = {
	setattr:	hfs_notify_change_cap,
};

/*================ File-local functions ================*/

/*
 * cap_build_meta()
 *
 * Build the metadata structure.
 */
static void cap_build_meta(struct hfs_cap_info *meta,
			   struct hfs_cat_entry *entry)
{
	memset(meta, 0, sizeof(*meta));
	memcpy(meta->fi_fndr, &entry->info, 32);
	if ((entry->type == HFS_CDR_FIL) &&
	    (entry->u.file.flags & HFS_FIL_LOCK)) {
		/* Couple the locked bit of the file to the
		   AFP {write,rename,delete} inhibit bits. */
		hfs_put_hs(HFS_AFP_RDONLY, meta->fi_attr);
	}
	meta->fi_magic1 = HFS_CAP_MAGIC1;
	meta->fi_version = HFS_CAP_VERSION;
	meta->fi_magic = HFS_CAP_MAGIC;
	meta->fi_bitmap = HFS_CAP_LONGNAME;
	memcpy(meta->fi_macfilename, entry->key.CName.Name,
	       entry->key.CName.Len);
	meta->fi_datemagic = HFS_CAP_DMAGIC;
	meta->fi_datevalid = HFS_CAP_MDATE | HFS_CAP_CDATE;
	hfs_put_nl(hfs_m_to_htime(entry->create_date), meta->fi_ctime);
	hfs_put_nl(hfs_m_to_htime(entry->modify_date), meta->fi_mtime);
	hfs_put_nl(CURRENT_TIME,                       meta->fi_utime);
}

static loff_t cap_info_llseek(struct file *file, loff_t offset, int origin)
{
	long long retval;

	switch (origin) {
		case 2:
			offset += file->f_dentry->d_inode->i_size;
			break;
		case 1:
			offset += file->f_pos;
	}
	retval = -EINVAL;
	if (offset>=0 && offset<=HFS_FORK_MAX) {
		if (offset != file->f_pos) {
			file->f_pos = offset;
			file->f_reada = 0;
			file->f_version = ++event;
		}
		retval = offset;
	}
	return retval;
}

/*
 * cap_info_read()
 *
 * This is the read() entry in the file_operations structure for CAP
 * metadata files.  The purpose is to transfer up to 'count' bytes
 * from the file corresponding to 'inode' beginning at offset
 * 'file->f_pos' to user-space at the address 'buf'.  The return value
 * is the number of bytes actually transferred.
 */
static hfs_rwret_t cap_info_read(struct file *filp, char *buf,
				 hfs_rwarg_t count, loff_t *ppos)
{
	struct inode *inode = filp->f_dentry->d_inode;
	struct hfs_cat_entry *entry = HFS_I(inode)->entry;
	hfs_s32 left, size, read = 0;
	hfs_u32 pos;

	if (!S_ISREG(inode->i_mode)) {
		hfs_warn("hfs_cap_info_read: mode = %07o\n", inode->i_mode);
		return -EINVAL;
	}

	pos = *ppos;
	if (pos > HFS_FORK_MAX) {
		return 0;
	}
	size = inode->i_size;
	if (pos > size) {
		left = 0;
	} else {
		left = size - pos;
	}
	if (left > count) {
		left = count;
	}
	if (left <= 0) {
		return 0;
	}

	if (pos < sizeof(struct hfs_cap_info)) {
		int memcount = sizeof(struct hfs_cap_info) - pos;
		struct hfs_cap_info meta;

		if (memcount > left) {
			memcount = left;
		}
		cap_build_meta(&meta, entry);
		memcount -= copy_to_user(buf, ((char *)&meta) + pos, memcount);
		left -= memcount;
		read += memcount;
		pos += memcount;
		buf += memcount;
	}

	if (left > 0) {
		clear_user(buf, left);
	        pos += left;
	}

	if (read) {
		inode->i_atime = CURRENT_TIME;
		*ppos = pos;
		mark_inode_dirty(inode);
	}

	return read;
}

/*
 * cap_info_write()
 *
 * This is the write() entry in the file_operations structure for CAP
 * metadata files.  The purpose is to transfer up to 'count' bytes
 * to the file corresponding to 'inode' beginning at offset
 * '*ppos' from user-space at the address 'buf'.
 * The return value is the number of bytes actually transferred.
 */
static hfs_rwret_t cap_info_write(struct file *filp, const char *buf, 
				  hfs_rwarg_t count, loff_t *ppos)
{
        struct inode *inode = filp->f_dentry->d_inode;
	hfs_u32 pos;

	if (!S_ISREG(inode->i_mode)) {
		hfs_warn("hfs_file_write: mode = %07o\n", inode->i_mode);
		return -EINVAL;
	}
	if (count <= 0) {
		return 0;
	}
	
	pos = (filp->f_flags & O_APPEND) ? inode->i_size : *ppos;

	if (pos > HFS_FORK_MAX) {
		return 0;
	}

	*ppos += count;
	if (*ppos > HFS_FORK_MAX) {
		*ppos = HFS_FORK_MAX;
		count = HFS_FORK_MAX - pos;
	}

	if (*ppos > inode->i_size)
	        inode->i_size = *ppos;

	/* Only deal with the part we store in memory */
	if (pos < sizeof(struct hfs_cap_info)) {
		int end, mem_count;
		struct hfs_cat_entry *entry = HFS_I(inode)->entry;
		struct hfs_cap_info meta;

		mem_count = sizeof(struct hfs_cap_info) - pos;
		if (mem_count > count) {
			mem_count = count;
		}
		end = pos + mem_count;

		cap_build_meta(&meta, entry);
		mem_count -= copy_from_user(((char *)&meta) + pos, buf, mem_count);

		/* Update finder attributes if changed */
		if (OVERLAPS(pos, end, struct hfs_cap_info, fi_fndr)) {
			memcpy(&entry->info, meta.fi_fndr, 32);
			hfs_cat_mark_dirty(entry);
		}

		/* Update file flags if changed */
		if (OVERLAPS(pos, end, struct hfs_cap_info, fi_attr) &&
		    (entry->type == HFS_CDR_FIL)) {
			int locked = hfs_get_ns(&meta.fi_attr) &
							htons(HFS_AFP_WRI);
			hfs_u8 new_flags;

			if (locked) {
				new_flags = entry->u.file.flags | HFS_FIL_LOCK;
			} else {
				new_flags = entry->u.file.flags & ~HFS_FIL_LOCK;
			}

			if (new_flags != entry->u.file.flags) {
				entry->u.file.flags = new_flags;
				hfs_cat_mark_dirty(entry);
				hfs_file_fix_mode(entry);
			}
		}

		/* Update CrDat if changed */
		if (OVERLAPS(pos, end, struct hfs_cap_info, fi_ctime)) {
			entry->create_date =
				hfs_h_to_mtime(hfs_get_nl(meta.fi_ctime));
			hfs_cat_mark_dirty(entry);
		}

		/* Update MdDat if changed */
		if (OVERLAPS(pos, end, struct hfs_cap_info, fi_mtime)) {
			entry->modify_date =
				hfs_h_to_mtime(hfs_get_nl(meta.fi_mtime));
			hfs_cat_mark_dirty(entry);
		}
	}

	inode->i_mtime = inode->i_ctime = CURRENT_TIME;
	mark_inode_dirty(inode);
	return count;
}
