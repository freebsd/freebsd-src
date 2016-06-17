/*
 * linux/fs/hfs/file_hdr.c
 *
 * Copyright (C) 1995-1997  Paul H. Hargrove
 * This file may be distributed under the terms of the GNU General Public License.
 *
 * This file contains the file_ops and inode_ops for the metadata
 * files under the AppleDouble and Netatalk representations.
 *
 * The source code distributions of Netatalk, versions 1.3.3b2 and
 * 1.4b2, were used as a specification of the location and format of
 * files used by Netatalk's afpd.  No code from Netatalk appears in
 * hfs_fs.  hfs_fs is not a work ``derived'' from Netatalk in the
 * sense of intellectual property law.
 *
 * "XXX" in a comment is a note to myself to consider changing something.
 *
 * In function preconditions the term "valid" applied to a pointer to
 * a structure means that the pointer is non-NULL and the structure it
 * points to has all fields initialized to consistent values.
 *
 * XXX: Note the reason that there is not bmap() for AppleDouble
 * header files is that dynamic nature of their structure make it
 * very difficult to safely mmap them.  Maybe in the distant future
 * I'll get bored enough to implement it.
 */

#include "hfs.h"
#include <linux/hfs_fs_sb.h>
#include <linux/hfs_fs_i.h>
#include <linux/hfs_fs.h>

/* prodos types */
#define PRODOSI_FTYPE_DIR   0x0F
#define PRODOSI_FTYPE_TEXT  0x04
#define PRODOSI_FTYPE_8BIT  0xFF
#define PRODOSI_FTYPE_16BIT 0xB3

#define PRODOSI_AUXTYPE_DIR 0x0200

/*================ Forward declarations ================*/
static loff_t      hdr_llseek(struct file *, loff_t, int);
static hfs_rwret_t hdr_read(struct file *, char *, hfs_rwarg_t, loff_t *);
static hfs_rwret_t hdr_write(struct file *, const char *,
			     hfs_rwarg_t, loff_t *);
/*================ Global variables ================*/

struct file_operations hfs_hdr_operations = {
	llseek:		hdr_llseek,
	read:		hdr_read,
	write:		hdr_write,
	fsync:		file_fsync,
};

struct inode_operations hfs_hdr_inode_operations = {
	setattr:	hfs_notify_change_hdr,
};

const struct hfs_hdr_layout hfs_dbl_fil_hdr_layout = {
	__constant_htonl(HFS_DBL_MAGIC),	/* magic   */
	__constant_htonl(HFS_HDR_VERSION_2),	/* version */
	6,					/* entries */
	{					/* descr[] */
		{HFS_HDR_FNAME, offsetof(struct hfs_dbl_hdr, real_name),   ~0},
		{HFS_HDR_DATES, offsetof(struct hfs_dbl_hdr, create_time), 16},
		{HFS_HDR_FINFO, offsetof(struct hfs_dbl_hdr, finderinfo),  32},
		{HFS_HDR_MACI,  offsetof(struct hfs_dbl_hdr, fileinfo),     4},
		{HFS_HDR_DID,   offsetof(struct hfs_dbl_hdr, cnid),         4},
		{HFS_HDR_RSRC,  HFS_DBL_HDR_LEN,                           ~0}
	},
	{					/* order[] */
		(struct hfs_hdr_descr *)&hfs_dbl_fil_hdr_layout.descr[0],
		(struct hfs_hdr_descr *)&hfs_dbl_fil_hdr_layout.descr[1],
		(struct hfs_hdr_descr *)&hfs_dbl_fil_hdr_layout.descr[2],
		(struct hfs_hdr_descr *)&hfs_dbl_fil_hdr_layout.descr[3],
		(struct hfs_hdr_descr *)&hfs_dbl_fil_hdr_layout.descr[4],
		(struct hfs_hdr_descr *)&hfs_dbl_fil_hdr_layout.descr[5]
	}
};

const struct hfs_hdr_layout hfs_dbl_dir_hdr_layout = {
	__constant_htonl(HFS_DBL_MAGIC),	/* magic   */
	__constant_htonl(HFS_HDR_VERSION_2),	/* version */
	5,					/* entries */
	{					/* descr[] */
		{HFS_HDR_FNAME, offsetof(struct hfs_dbl_hdr, real_name),   ~0},
		{HFS_HDR_DATES, offsetof(struct hfs_dbl_hdr, create_time), 16},
		{HFS_HDR_FINFO, offsetof(struct hfs_dbl_hdr, finderinfo),  32},
		{HFS_HDR_MACI,  offsetof(struct hfs_dbl_hdr, fileinfo),     4},
		{HFS_HDR_DID,   offsetof(struct hfs_dbl_hdr, cnid),         4}
	},
	{					/* order[] */
		(struct hfs_hdr_descr *)&hfs_dbl_dir_hdr_layout.descr[0],
		(struct hfs_hdr_descr *)&hfs_dbl_dir_hdr_layout.descr[1],
		(struct hfs_hdr_descr *)&hfs_dbl_dir_hdr_layout.descr[2],
		(struct hfs_hdr_descr *)&hfs_dbl_dir_hdr_layout.descr[3],
		(struct hfs_hdr_descr *)&hfs_dbl_dir_hdr_layout.descr[4]
	}
};

const struct hfs_hdr_layout hfs_nat2_hdr_layout = {
	__constant_htonl(HFS_DBL_MAGIC),	/* magic   */
	__constant_htonl(HFS_HDR_VERSION_2),	/* version */
	9,					/* entries */
	{					/* descr[] */
		{HFS_HDR_FNAME, offsetof(struct hfs_dbl_hdr, real_name),   ~0},
		{HFS_HDR_COMNT, offsetof(struct hfs_dbl_hdr, comment),      0},
		{HFS_HDR_DATES, offsetof(struct hfs_dbl_hdr, create_time), 16},
		{HFS_HDR_FINFO, offsetof(struct hfs_dbl_hdr, finderinfo),  32},
		{HFS_HDR_AFPI,  offsetof(struct hfs_dbl_hdr, fileinfo),     4},
		{HFS_HDR_DID,   offsetof(struct hfs_dbl_hdr, cnid),         4},
		{HFS_HDR_SNAME,  offsetof(struct hfs_dbl_hdr, short_name), ~0},
		{HFS_HDR_PRODOSI,  offsetof(struct hfs_dbl_hdr, prodosi),   8},
		{HFS_HDR_RSRC,  HFS_NAT_HDR_LEN,                           ~0}
	},
	{					/* order[] */
		(struct hfs_hdr_descr *)&hfs_nat_hdr_layout.descr[0],
		(struct hfs_hdr_descr *)&hfs_nat_hdr_layout.descr[1],
		(struct hfs_hdr_descr *)&hfs_nat_hdr_layout.descr[2],
		(struct hfs_hdr_descr *)&hfs_nat_hdr_layout.descr[3],
		(struct hfs_hdr_descr *)&hfs_nat_hdr_layout.descr[4],
		(struct hfs_hdr_descr *)&hfs_nat_hdr_layout.descr[5],
		(struct hfs_hdr_descr *)&hfs_nat_hdr_layout.descr[6],
		(struct hfs_hdr_descr *)&hfs_nat_hdr_layout.descr[7],
		(struct hfs_hdr_descr *)&hfs_nat_hdr_layout.descr[8]
	}
};

const struct hfs_hdr_layout hfs_nat_hdr_layout = {
	__constant_htonl(HFS_DBL_MAGIC),	/* magic   */
	__constant_htonl(HFS_HDR_VERSION_1),	/* version */
	5,					/* entries */
	{					/* descr[] */
		{HFS_HDR_FNAME, offsetof(struct hfs_dbl_hdr, real_name),   ~0},
		{HFS_HDR_COMNT, offsetof(struct hfs_dbl_hdr, comment),      0},
		{HFS_HDR_OLDI,  offsetof(struct hfs_dbl_hdr, create_time), 16},
		{HFS_HDR_FINFO, offsetof(struct hfs_dbl_hdr, finderinfo),  32},
		{HFS_HDR_RSRC,  HFS_NAT_HDR_LEN,                           ~0},
	},
	{					/* order[] */
		(struct hfs_hdr_descr *)&hfs_nat_hdr_layout.descr[0],
		(struct hfs_hdr_descr *)&hfs_nat_hdr_layout.descr[1],
		(struct hfs_hdr_descr *)&hfs_nat_hdr_layout.descr[2],
		(struct hfs_hdr_descr *)&hfs_nat_hdr_layout.descr[3],
		(struct hfs_hdr_descr *)&hfs_nat_hdr_layout.descr[4]
	}
};

/*================ File-local variables ================*/

static const char fstype[16] =
	{'M','a','c','i','n','t','o','s','h',' ',' ',' ',' ',' ',' ',' '};

/*================ File-local data types ================*/

struct hdr_hdr {
        hfs_lword_t	magic;
        hfs_lword_t	version;
        hfs_byte_t	filler[16];
        hfs_word_t	entries;
        hfs_byte_t	descrs[12*HFS_HDR_MAX];
}  __attribute__((packed));

/*================ File-local functions ================*/

/*
 * dlength()
 */
static int dlength(const struct hfs_hdr_descr *descr,
		   const struct hfs_cat_entry *entry)
{
	hfs_u32 length = descr->length;

	/* handle auto-sized entries */
	if (length == ~0) {
		switch (descr->id) {
		case HFS_HDR_DATA:
			if (entry->type == HFS_CDR_FIL) {
				length = entry->u.file.data_fork.lsize;
			} else {
				length = 0;
			}
			break;

		case HFS_HDR_RSRC:
			if (entry->type == HFS_CDR_FIL) {
				length = entry->u.file.rsrc_fork.lsize;
			} else {
				length = 0;
			}
			break;

		case HFS_HDR_FNAME:
			length = entry->key.CName.Len;
			break;

		case HFS_HDR_SNAME:
		default:
			length = 0;
		}
	}
	return length;
}

/*
 * hdr_build_meta()
 */
static void hdr_build_meta(struct hdr_hdr *meta,
			   const struct hfs_hdr_layout *layout,
			   const struct hfs_cat_entry *entry)
{
	const struct hfs_hdr_descr *descr;
	hfs_byte_t *ptr;
	int lcv;

	hfs_put_nl(layout->magic,   meta->magic);
	hfs_put_nl(layout->version, meta->version);
	if (layout->version == htonl(HFS_HDR_VERSION_1)) {
		memcpy(meta->filler, fstype, 16);
	} else {
		memset(meta->filler, 0, 16);
	}
	hfs_put_hs(layout->entries, meta->entries);
	memset(meta->descrs, 0, sizeof(meta->descrs));
	for (lcv = 0, descr = layout->descr, ptr = meta->descrs;
	     lcv < layout->entries; ++lcv, ++descr, ptr += 12) {
		hfs_put_hl(descr->id,             ptr);
		hfs_put_hl(descr->offset,         ptr + 4);
		hfs_put_hl(dlength(descr, entry), ptr + 8);
	}
}

/*
 * dup_layout ()
 */
static struct hfs_hdr_layout *dup_layout(const struct hfs_hdr_layout *old)
{
	struct hfs_hdr_layout *new;
	int lcv;

	if (HFS_NEW(new)) {
		memcpy(new, old, sizeof(*new));
		for (lcv = 0; lcv < new->entries; ++lcv) {
			(char *)(new->order[lcv]) += (char *)new - (char *)old;
		}
	}
	return new;
}

/*
 * init_layout()
 */
static inline void init_layout(struct hfs_hdr_layout *layout,
			       const hfs_byte_t *descrs)
{
	struct hfs_hdr_descr **base, **p, **q, *tmp;
	int lcv, entries = layout->entries;

	for (lcv = 0; lcv < entries; ++lcv, descrs += 12) {
		layout->order[lcv] = &layout->descr[lcv];
		layout->descr[lcv].id     = hfs_get_hl(descrs);
		layout->descr[lcv].offset = hfs_get_hl(descrs + 4);
		layout->descr[lcv].length = hfs_get_hl(descrs + 8);
	}
	for (lcv = layout->entries; lcv < HFS_HDR_MAX; ++lcv) {
		layout->order[lcv] = NULL;
		layout->descr[lcv].id     = 0;
		layout->descr[lcv].offset = 0;
		layout->descr[lcv].length = 0;
	}

	/* Sort the 'order' array using an insertion sort */
	base = &layout->order[0];
	for (p = (base+1); p < (base+entries); ++p) {
		q=p;
		while ((*q)->offset < (*(q-1))->offset) {
			tmp = *q;
			*q = *(q-1);
			*(--q) = tmp;
			if (q == base) break;
		}
	}
}

/*
 * adjust_forks()
 */
static inline void adjust_forks(struct hfs_cat_entry *entry,
				const struct hfs_hdr_layout *layout)
{
	int lcv;

	for (lcv = 0; lcv < layout->entries; ++lcv) {
		const struct hfs_hdr_descr *descr = &layout->descr[lcv];

		if ((descr->id == HFS_HDR_DATA) &&
		    (descr->length != entry->u.file.data_fork.lsize)) {
			entry->u.file.data_fork.lsize = descr->length;
			hfs_extent_adj(&entry->u.file.data_fork);
		} else if ((descr->id == HFS_HDR_RSRC) &&
			   (descr->length != entry->u.file.rsrc_fork.lsize)) {
			entry->u.file.rsrc_fork.lsize = descr->length;
			hfs_extent_adj(&entry->u.file.rsrc_fork);
		}
	}
}

/*
 * get_dates()
 */
static void get_dates(const struct hfs_cat_entry *entry,
		      const struct inode *inode,  hfs_u32 dates[3])
{
	dates[0] = hfs_m_to_htime(entry->create_date);
	dates[1] = hfs_m_to_htime(entry->modify_date);
	dates[2] = hfs_m_to_htime(entry->backup_date);
}

/*
 * set_dates()
 */
static void set_dates(struct hfs_cat_entry *entry, struct inode *inode,
		      const hfs_u32 *dates)
{
	hfs_u32 tmp;

	tmp = hfs_h_to_mtime(dates[0]);
	if (entry->create_date != tmp) {
		entry->create_date = tmp;
		hfs_cat_mark_dirty(entry);
	}
	tmp = hfs_h_to_mtime(dates[1]);
	if (entry->modify_date != tmp) {
		entry->modify_date = tmp;
		inode->i_ctime = inode->i_atime = inode->i_mtime = 
			hfs_h_to_utime(dates[1]);
		hfs_cat_mark_dirty(entry);
	}
	tmp = hfs_h_to_mtime(dates[2]);
	if (entry->backup_date != tmp) {
		entry->backup_date = tmp;
		hfs_cat_mark_dirty(entry);
	}
}

loff_t hdr_llseek(struct file *file, loff_t offset, int origin)
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
	if (offset>=0 && offset<file->f_dentry->d_inode->i_size) {
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
 * hdr_read()
 *
 * This is the read field in the inode_operations structure for
 * header files.  The purpose is to transfer up to 'count' bytes
 * from the file corresponding to 'inode', beginning at
 * 'filp->offset' bytes into the file.	The data is transferred to
 * user-space at the address 'buf'.  Returns the number of bytes
 * successfully transferred.
 */
/* XXX: what about the entry count changing on us? */
static hfs_rwret_t hdr_read(struct file * filp, char * buf, 
			    hfs_rwarg_t count, loff_t *ppos)
{
	struct inode *inode = filp->f_dentry->d_inode;
	struct hfs_cat_entry *entry = HFS_I(inode)->entry;
	const struct hfs_hdr_layout *layout;
	off_t start, length, offset;
	off_t pos = *ppos;
	int left, lcv, read = 0;

	if (!S_ISREG(inode->i_mode)) {
		hfs_warn("hfs_hdr_read: mode = %07o\n",inode->i_mode);
		return -EINVAL;
	}

	if (HFS_I(inode)->layout) {
		layout = HFS_I(inode)->layout;
	} else {
		layout = HFS_I(inode)->default_layout;
	}

	/* Adjust count to fit within the bounds of the file */
	if ((pos >= inode->i_size) || (count <= 0)) {
		return 0;
	} else if (count > inode->i_size - pos) {
		count = inode->i_size - pos;
	}

	/* Handle the fixed-location portion */
	length = sizeof(hfs_u32) + sizeof(hfs_u32) + 16 +
		 sizeof(hfs_u16) + layout->entries * (3 * sizeof(hfs_u32));
	if (pos < length) {
		struct hdr_hdr meta;

		left = length - pos;
		if (left > count) {
			left = count;
		}

		hdr_build_meta(&meta, layout, entry);
		left -= copy_to_user(buf, ((char *)&meta) + pos, left);
		count -= left;
		read += left;
		pos += left;
		buf += left;
	}
	if (!count) {
		goto done;
	}

	/* Handle the actual data */
	for (lcv = 0; count && (lcv < layout->entries); ++lcv) {
		const struct hfs_hdr_descr *descr = layout->order[lcv];
		struct hfs_fork *fork;
		char tmp[16], *p;
		off_t limit;

		/* stop reading if we run out of descriptors early */
		if (!descr) {
			break;
		}

		/* find start and length of this entry */
		start = descr->offset;
		length = dlength(descr, entry);

		/* Skip to next entry if this one is empty or isn't needed */
		if (!length || (pos >= start + length)) {
			continue;
		}

		/* Pad with zeros to the start of this entry if needed */
		if (pos < start) {
			left = start - pos;
			if (left > count) {
				left = count;
			}
			clear_user(buf, left);
			count -= left;
			read += left;
			pos += left;
			buf += left;
		}
		if (!count) {
			goto done;
		}

		/* locate and/or construct the data for this entry */
		fork = NULL;
		p = NULL;
		switch (descr->id) {
		case HFS_HDR_DATA:
			fork = &entry->u.file.data_fork;
			limit = fork->lsize;
			break;

		case HFS_HDR_RSRC:
			fork = &entry->u.file.rsrc_fork;
			limit = fork->lsize;
			break;

		case HFS_HDR_FNAME:
			p = entry->key.CName.Name;
			limit = entry->key.CName.Len;
			break;

		case HFS_HDR_OLDI:
		case HFS_HDR_DATES:
			get_dates(entry, inode, (hfs_u32 *)tmp);
			if (descr->id == HFS_HDR_DATES) {
				/* XXX: access date. hfsplus actually
                                   has this. */
				memcpy(tmp + 12, tmp + 4, 4);
			} else if ((entry->type == HFS_CDR_FIL) &&
				   (entry->u.file.flags & HFS_FIL_LOCK)) {
				hfs_put_hl(HFS_AFP_RDONLY, tmp + 12);
			} else {
				hfs_put_nl(0, tmp + 12);
			}
			p = tmp;
			limit = 16;
			break;

		case HFS_HDR_FINFO:
			p = (char *)&entry->info;
			limit = 32;
			break;

		case HFS_HDR_AFPI:
			/* XXX: this needs to do more mac->afp mappings */
			hfs_put_ns(0, tmp);
			if ((entry->type == HFS_CDR_FIL) &&
			    (entry->u.file.flags & HFS_FIL_LOCK)) {
				hfs_put_hs(HFS_AFP_RDONLY, tmp + 2);
			} else {
				hfs_put_ns(0, tmp + 2);
			}
			p = tmp;
			limit = 4;
		        break;

		case HFS_HDR_PRODOSI:
			/* XXX: this needs to do mac->prodos translations */
			memset(tmp, 0, 8);
#if 0
			hfs_put_ns(0, tmp); /* access */
			hfs_put_ns(0, tmp); /* type */
			hfs_put_nl(0, tmp); /* aux type */
#endif
			p = tmp;
			limit = 8;
		        break;

		case HFS_HDR_MACI:
			hfs_put_ns(0, tmp);
			if (entry->type == HFS_CDR_FIL) {
				hfs_put_hs(entry->u.file.flags, tmp + 2);
			} else {
				hfs_put_ns(entry->u.dir.flags, tmp + 2);
			}
			p = tmp;
			limit = 4;
			break;

		case HFS_HDR_DID:
		        /* if it's rootinfo, stick the next available did in
			 * the did slot. */
			limit = 4;
			if (entry->cnid == htonl(HFS_ROOT_CNID)) {
				struct hfs_mdb *mdb = entry->mdb;
				const struct hfs_name *reserved = 
				HFS_SB(mdb->sys_mdb)->s_reserved2;
				
				while (reserved->Len) {
					if (hfs_streq(reserved->Name,
						      reserved->Len,
						      entry->key.CName.Name,
						      entry->key.CName.Len)) {
						hfs_put_hl(mdb->next_id, tmp);
						p = tmp;
						goto hfs_did_done;
					}
					reserved++;
				}
			}
			p = (char *) &entry->cnid;
hfs_did_done:
			break;

		case HFS_HDR_SNAME:
		default:
			limit = 0;
		}
		
		/* limit the transfer to the available data
		   of to the stated length of the entry. */
		if (length > limit) {
			length = limit;
		}
		offset = pos - start;
		left = length - offset;
		if (left > count) {
			left = count;
		}
		if (left <= 0) {
			continue;
		}

		/* transfer the data */
		if (p) {
			left -= copy_to_user(buf, p + offset, left);
		} else if (fork) {
			left = hfs_do_read(inode, fork, offset, buf, left,
					   filp->f_reada != 0);
			if (left > 0) {
				filp->f_reada = 1;
			} else if (!read) {
				return left;
			} else {
				goto done;
			}
		}
		count -= left;
		read += left;
		pos += left;
		buf += left;
	}

	/* Pad the file out with zeros */
	if (count) {
		clear_user(buf, count);
		read += count;
		pos += count;
	}
		
done:
	if (read) {
		inode->i_atime = CURRENT_TIME;
		*ppos = pos;
		mark_inode_dirty(inode);
	}
	return read;
}

/*
 * hdr_write()
 *
 * This is the write() entry in the file_operations structure for
 * header files.  The purpose is to transfer up to 'count' bytes
 * to the file corresponding to 'inode' beginning at offset
 * '*ppos' from user-space at the address 'buf'.
 * The return value is the number of bytes actually transferred.
 */
static hfs_rwret_t hdr_write(struct file *filp, const char *buf,
			     hfs_rwarg_t count, loff_t *ppos)
{
	struct inode *inode = filp->f_dentry->d_inode;
        struct hfs_cat_entry *entry = HFS_I(inode)->entry;
        struct hfs_hdr_layout *layout;
        off_t start, length, offset;
        int left, lcv, written = 0;
	struct hdr_hdr meta;
	int built_meta = 0;
        off_t pos;

	if (!S_ISREG(inode->i_mode)) {
		hfs_warn("hfs_hdr_write: mode = %07o\n", inode->i_mode);
		return -EINVAL;
	}
	if (count <= 0) {
		return 0;
	}

	pos = (filp->f_flags & O_APPEND) ? inode->i_size : *ppos;

	if (!HFS_I(inode)->layout) {
		HFS_I(inode)->layout = dup_layout(HFS_I(inode)->default_layout);
	}
	layout = HFS_I(inode)->layout;

	/* Handle the 'magic', 'version', 'filler' and 'entries' fields */
	length = sizeof(hfs_u32) + sizeof(hfs_u32) + 16 + sizeof(hfs_u16);
	if (pos < length) {
		hdr_build_meta(&meta, layout, entry);
		built_meta = 1;

		left = length - pos;
		if (left > count) {
			left = count;
		}

		left -= copy_from_user(((char *)&meta) + pos, buf, left);
		layout->magic   = hfs_get_nl(meta.magic);
		layout->version = hfs_get_nl(meta.version);
		layout->entries = hfs_get_hs(meta.entries);
		if (layout->entries > HFS_HDR_MAX) {
			/* XXX: should allocate slots dynamically */
			hfs_warn("hfs_hdr_write: TRUNCATING TO %d "
				 "DESCRIPTORS\n", HFS_HDR_MAX);
			layout->entries = HFS_HDR_MAX;
		}

		count -= left;
		written += left;
		pos += left;
		buf += left;
	}
	if (!count) {
		goto done;
	}

	/* We know for certain how many entries we have, so process them */
	length += layout->entries * 3 * sizeof(hfs_u32);
	if (pos < length) {
		if (!built_meta) {
			hdr_build_meta(&meta, layout, entry);
		}

		left = length - pos;
		if (left > count) {
			left = count;
		}

		left -= copy_from_user(((char *)&meta) + pos, buf, left);
		init_layout(layout, meta.descrs);

		count -= left;
		written += left;
		pos += left;
		buf += left;

		/* Handle possible size changes for the forks */
		if (entry->type == HFS_CDR_FIL) {
			adjust_forks(entry, layout);
			hfs_cat_mark_dirty(entry);
		}
	}

	/* Handle the actual data */
	for (lcv = 0; count && (lcv < layout->entries); ++lcv) {
		struct hfs_hdr_descr *descr = layout->order[lcv];
		struct hfs_fork *fork;
		char tmp[16], *p;
		off_t limit;

		/* stop writing if we run out of descriptors early */
		if (!descr) {
			break;
		}

		/* find start and length of this entry */
		start = descr->offset;
		if ((descr->id == HFS_HDR_DATA) ||
		    (descr->id == HFS_HDR_RSRC)) {
			if (entry->type == HFS_CDR_FIL) {
				length = 0x7fffffff - start;
			} else {
				continue;
			}
		} else {
			length = dlength(descr, entry);
		}

		/* Trim length to avoid overlap with the next entry */
		if (layout->order[lcv+1] &&
		    ((start + length) > layout->order[lcv+1]->offset)) {
			length = layout->order[lcv+1]->offset - start;
		}

		/* Skip to next entry if this one is empty or isn't needed */
		if (!length || (pos >= start + length)) {
			continue;
		}

		/* Skip any padding that may exist between entries */
		if (pos < start) {
			left = start - pos;
			if (left > count) {
				left = count;
			}
			count -= left;
			written += left;
			pos += left;
			buf += left;
		}
		if (!count) {
			goto done;
		}

		/* locate and/or construct the data for this entry */
		fork = NULL;
		p = NULL;
		switch (descr->id) {
		case HFS_HDR_DATA:
#if 0
/* Can't yet write to the data fork via a header file, since there is the
 * possibility to write via the data file, and the only locking is at the
 * inode level.
 */
			fork = &entry->u.file.data_fork;
			limit = length;
#else
			limit = 0;
#endif
			break;

		case HFS_HDR_RSRC:
			fork = &entry->u.file.rsrc_fork;
			limit = length;
			break;

		case HFS_HDR_OLDI:
		case HFS_HDR_DATES:
			get_dates(entry, inode, (hfs_u32 *)tmp);
			if (descr->id == HFS_HDR_DATES) {
				memcpy(tmp + 12, tmp + 4, 4);
			} else if ((entry->type == HFS_CDR_FIL) &&
				   (entry->u.file.flags & HFS_FIL_LOCK)) {
				hfs_put_hl(HFS_AFP_RDONLY, tmp + 12);
			} else {
				hfs_put_nl(0, tmp + 12);
			}
			p = tmp;
			limit = 16;
			break;

		case HFS_HDR_FINFO:
			p = (char *)&entry->info;
			limit = 32;
			break;

		case HFS_HDR_AFPI:
			hfs_put_ns(0, tmp);
			if ((entry->type == HFS_CDR_FIL) &&
			    (entry->u.file.flags & HFS_FIL_LOCK)) {
				hfs_put_hs(HFS_AFP_RDONLY, tmp + 2);
			} else {
				hfs_put_ns(0, tmp + 2);
			}			
			p = tmp;
			limit = 4;
			break;

		case HFS_HDR_PRODOSI:
			/* XXX: this needs to do mac->prodos translations */
			memset(tmp, 0, 8); 
#if 0
			hfs_put_ns(0, tmp); /* access */
			hfs_put_ns(0, tmp); /* type */
			hfs_put_nl(0, tmp); /* aux type */
#endif
			p = tmp;
			limit = 8;
		        break;

		case HFS_HDR_MACI:
			hfs_put_ns(0, tmp);
			if (entry->type == HFS_CDR_FIL) {
				hfs_put_hs(entry->u.file.flags, tmp + 2);
			} else {
				hfs_put_ns(entry->u.dir.flags, tmp + 2);
			}
			p = tmp;
			limit = 4;
			break;

		case HFS_HDR_FNAME:	/* Can't rename a file this way */
		case HFS_HDR_DID:       /* can't specify a did this way */
		default:
			limit = 0;
		}
		
		/* limit the transfer to the available data
		   of to the stated length of the entry. */
		if (length > limit) {
			length = limit;
		}
		offset = pos - start;
		left = length - offset;
		if (left > count) {
			left = count;
		}
		if (left <= 0) {
			continue;
		}

		/* transfer the data from user space */
		if (p) {
			left -= copy_from_user(p + offset, buf, left);
		} else if (fork) {
			left = hfs_do_write(inode, fork, offset, buf, left);
		}

		/* process the data */
		switch (descr->id) {
		case HFS_HDR_OLDI:
			set_dates(entry, inode, (hfs_u32 *)tmp);
			if (entry->type == HFS_CDR_FIL) {
				hfs_u8 new_flags = entry->u.file.flags;

				if (hfs_get_nl(tmp+12) & htonl(HFS_AFP_WRI)) {
					new_flags |= HFS_FIL_LOCK;
				} else {
					new_flags &= ~HFS_FIL_LOCK;
				}

				if (new_flags != entry->u.file.flags) {
					entry->u.file.flags = new_flags;
					hfs_cat_mark_dirty(entry);
					hfs_file_fix_mode(entry);
				}
			}
			break;

		case HFS_HDR_DATES:
			set_dates(entry, inode, (hfs_u32 *)tmp);
			break;

		case HFS_HDR_FINFO:
			hfs_cat_mark_dirty(entry);
			break;

		case HFS_HDR_MACI:
			if (entry->type == HFS_CDR_DIR) {
				hfs_u16 new_flags = hfs_get_ns(tmp + 2);

				if (entry->u.dir.flags != new_flags) {
					entry->u.dir.flags = new_flags;
					hfs_cat_mark_dirty(entry);
				}
			} else {
				hfs_u8 new_flags = tmp[3];
				hfs_u8 changed = entry->u.file.flags^new_flags;
				
				if (changed) {
					entry->u.file.flags = new_flags;
					hfs_cat_mark_dirty(entry);
					if (changed & HFS_FIL_LOCK) {
						hfs_file_fix_mode(entry);
					}
				}
			}
			break;

		case HFS_HDR_DATA:
		case HFS_HDR_RSRC:
			if (left <= 0) {
				if (!written) {
					return left;
				} else {
					goto done;
				}
			} else if (fork->lsize > descr->length) {
				descr->length = fork->lsize;
			}
			break;

		case HFS_HDR_FNAME:	/* Can't rename a file this way */
		case HFS_HDR_DID:       /* Can't specify a did this way */
		case HFS_HDR_PRODOSI:   /* not implemented yet */
		case HFS_HDR_AFPI:      /* ditto */
		default:
			break;
		}

		count -= left;
		written += left;
		pos += left;
		buf += left;
	}

	/* Skip any padding at the end */
	if (count) {
		written += count;
		pos += count;
	}
		
done:
	*ppos = pos;
	if (written > 0) {
	        if (pos > inode->i_size)
		        inode->i_size = pos;
	        inode->i_mtime = inode->i_atime = CURRENT_TIME;
		mark_inode_dirty(inode);
	}
	return written;
}

/*
 * hdr_truncate()
 *
 * This is the truncate field in the inode_operations structure for
 * header files.  The purpose is to allocate or release blocks as needed
 * to satisfy a change in file length.
 */
void hdr_truncate(struct inode *inode, size_t size)
{
	struct hfs_cat_entry *entry = HFS_I(inode)->entry;
	struct hfs_hdr_layout *layout;
	int lcv, last;

	inode->i_size = size;
	if (!HFS_I(inode)->layout) {
		HFS_I(inode)->layout = dup_layout(HFS_I(inode)->default_layout);
	}
	layout = HFS_I(inode)->layout;

	last = layout->entries - 1;
	for (lcv = 0; lcv <= last; ++lcv) {
		struct hfs_hdr_descr *descr = layout->order[lcv];
		struct hfs_fork *fork;
		hfs_u32 offset;

		if (!descr) {
			break;
		}

		if (descr->id == HFS_HDR_RSRC) {
			fork = &entry->u.file.rsrc_fork;
#if 0
/* Can't yet truncate the data fork via a header file, since there is the
 * possibility to truncate via the data file, and the only locking is at
 * the inode level.
 */
		} else if (descr->id == HFS_HDR_DATA) {
			fork = &entry->u.file.data_fork;
#endif
		} else {
			continue;
		}

		offset = descr->offset;

		if ((lcv != last) && ((offset + descr->length) <= size)) {
			continue;
		}

		if (offset < size) {
			descr->length = size - offset;
		} else {
			descr->length = 0;
		}
		if (fork->lsize != descr->length) {
			fork->lsize = descr->length;
			hfs_extent_adj(fork);
			hfs_cat_mark_dirty(entry);
		}
	}
}
