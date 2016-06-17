/*
 * attr.c
 *
 * Copyright (C) 1996-1999 Martin von Löwis
 * Copyright (C) 1996-1997 Régis Duchesne
 * Copyright (C) 1998 Joseph Malicki
 * Copyright (C) 1999 Steve Dodd
 * Copyright (C) 2001 Anton Altaparmakov (AIA)
 */

#include "ntfstypes.h"
#include "struct.h"
#include "attr.h"

#include <linux/errno.h>
#include <linux/ntfs_fs.h>
#include "macros.h"
#include "support.h"
#include "util.h"
#include "super.h"
#include "inode.h"
#include "unistr.h"

/**
 * ntfs_find_attr_in_mft_rec - find attribute in mft record
 * @vol:	volume on which attr resides
 * @m:		mft record to search
 * @type:	attribute type to find
 * @name:	attribute name to find (optional, i.e. NULL means don't care)
 * @name_len:	attribute name length (only needed if @name present)
 * @ic:		ignore case if 1 or case sensitive if 0 (ignored if @name NULL)
 * @instance:	instance number to find
 *
 * Only search the specified mft record and it ignores the presence of an
 * attribute list attribute (unless it is the one being searched for,
 * obviously, in which case it is returned).
 */
ntfs_u8* ntfs_find_attr_in_mft_rec(ntfs_volume *vol, ntfs_u8 *m, __u32 type,
		wchar_t *name, __u32 name_len, int ic, __u16 instance)
{
	ntfs_u8 *a;
	
	/* Iterate over attributes in mft record @m. */
	a = m + NTFS_GETU16(m + 20);	/* attrs_offset */
	for (; a >= m && a <= m + vol->mft_record_size;
				a += NTFS_GETU32(a + 4 /* length */)) {
		/* We catch $END with this more general check, too... */
		if (NTFS_GETU32(a + 0 /* type */) > type)
			return NULL;
		if (!NTFS_GETU32(a + 4 /* length */))
			break;
		if (NTFS_GETU32(a + 0 /* type */) != type)
			continue;
		/* If @name is present, compare the two names. */
		if (name && !ntfs_are_names_equal(name, name_len, (wchar_t*)
				(a + NTFS_GETU16(a + 10 /* name_offset */)),
				a[9] /* name_length */, ic, vol->upcase,
				vol->upcase_length)) {
			register int rc;
			
			rc = ntfs_collate_names(vol->upcase, vol->upcase_length,
					name, name_len, (wchar_t*)(a +
					NTFS_GETU16(a + 10 /* name_offset */)),
					a[9] /* name_length */, 1, 1);
			/*
			 * If @name collates before a->name, there is no
			 * matching attribute.
			 */
			if (rc == -1)
				return NULL;
			/* If the strings are not equal, continue search. */
			if (rc)
	 			continue;
			rc = ntfs_collate_names(vol->upcase, vol->upcase_length,
					name, name_len, (wchar_t*)(a +
					NTFS_GETU16(a + 10 /* name_offset */)),
					a[9] /* name_length */, 0, 1);
			if (rc == -1)
				return NULL;
			if (rc)
				continue;
		}
		/*
		 * The names match or @name not present. Check instance number.
		 * and if it matches we have found the attribute and are done.
		 */
		if (instance != NTFS_GETU16(a + 14 /* instance */))
			continue;
		ntfs_debug(DEBUG_FILE3, "ntfs_find_attr_in_mft_record: found: "
			"attr type 0x%x, instance number = 0x%x.\n",
			NTFS_GETU32(a + 0), instance);
		return a;
	}
	ntfs_error("ntfs_find_attr_in_mft_record: mft record 0x%x is corrupt"
			". Run chkdsk.\n", m);
	return NULL;
}

/* Look if an attribute already exists in the inode, and if not, create it. */
int ntfs_new_attr(ntfs_inode *ino, int type, void *name, int namelen,
		  void *value, int value_len, int *pos, int *found)
{
	int do_insert = 0;
	int i, m;
	ntfs_attribute *a;

	for (i = 0; i < ino->attr_count; i++)
	{
		a = ino->attrs + i;
		if (a->type < type)
			continue;
		if (a->type > type) {
			do_insert = 1;
			break;
		}
		/* If @name is present, compare the two names. */
		if (namelen && !ntfs_are_names_equal((wchar_t*)name, namelen,
				a->name, a->namelen /* name_length */,
				1 /* ignore case*/, ino->vol->upcase,
				ino->vol->upcase_length)) {
			register int rc;

			rc = ntfs_collate_names(ino->vol->upcase,
					ino->vol->upcase_length, a->name,
					a->namelen, (wchar_t*)name, namelen,
					1 /* ignore case */, 1);
			if (rc == -1)
				continue;
			if (rc == 1) {
	 			do_insert = 1;
				break;
			}
			rc = ntfs_collate_names(ino->vol->upcase,
					ino->vol->upcase_length, a->name,
					a->namelen, (wchar_t*)name, namelen,
					0 /* case sensitive */, 1);
			if (rc == -1)
				continue;
			if (rc == 1) {
				do_insert = 1;
				break;
			}
		}
		/* Names are equal or no name was asked for. */
		/* If a value was specified compare the values. */
		if (value_len && a->resident) {
			if (!a->resident) {
				ntfs_error("ntfs_new_attr: Value specified but "
					"attribute non-resident. Bug!\n");
				return -EINVAL;
			}
			m = value_len;
			if (m > a->size)
				m = a->size;
			m = memcmp(value, a->d.data, m);
			if (m > 0)
				continue;
			if (m < 0) {
				do_insert = 1;
				break;
			}
			/* Values match until min of value lengths. */
			if (value_len > a->size)
				continue;
			if (value_len < a->size) {
				do_insert = 1;
				break;
			}
		}
		/* Full match! */
		*found = 1;
		*pos = i;
		return 0;
	}
	/* Re-allocate space. */
	if (ino->attr_count % 8 == 0)
	{
		ntfs_attribute* new;
		new = (ntfs_attribute*)ntfs_malloc((ino->attr_count + 8) *
							sizeof(ntfs_attribute));
		if (!new)
			return -ENOMEM;
		if (ino->attrs) {
			ntfs_memcpy(new, ino->attrs, ino->attr_count *
							sizeof(ntfs_attribute));
			ntfs_free(ino->attrs);
		}
		ino->attrs = new;
	}
	if (do_insert)
		ntfs_memmove(ino->attrs + i + 1, ino->attrs + i,
			     (ino->attr_count - i) * sizeof(ntfs_attribute));
	ino->attr_count++;
	ino->attrs[i].type = type;
	ino->attrs[i].namelen = namelen;
	ino->attrs[i].name = name;
	*pos = i;
	*found = 0;
	return 0;
}

int ntfs_make_attr_resident(ntfs_inode *ino, ntfs_attribute *attr)
{
	__s64 size = attr->size;
	if (size > 0) {
		/* FIXME: read data, free clusters */
		return -EOPNOTSUPP;
	}
	attr->resident = 1;
	return 0;
}

/* Store in the inode readable information about a run. */
int ntfs_insert_run(ntfs_attribute *attr, int cnum, ntfs_cluster_t cluster,
		     int len)
{
	/* (re-)allocate space if necessary. */
	if ((attr->d.r.len * sizeof(ntfs_runlist)) % PAGE_SIZE == 0) {
		ntfs_runlist* new;
		unsigned long new_size;

		ntfs_debug(DEBUG_MALLOC, "ntfs_insert_run: re-allocating "
				"space: old attr->d.r.len = 0x%x\n",
				attr->d.r.len);
		new_size = attr->d.r.len * sizeof(ntfs_runlist) + PAGE_SIZE;
		if ((new_size >> PAGE_SHIFT) > num_physpages) {
			ntfs_error("ntfs_insert_run: attempted to allocate "
					"more pages than num_physpages."
					"This might be a bug or a corrupt"
					"file system.\n");
			return -1;
		}
		new = ntfs_vmalloc(new_size);
		if (!new) {
			ntfs_error("ntfs_insert_run: ntfs_vmalloc(new_size = "
					"0x%x) failed\n", new_size);
			return -1;
		}
		if (attr->d.r.runlist) {
			ntfs_memcpy(new, attr->d.r.runlist, attr->d.r.len
					* sizeof(ntfs_runlist));
			ntfs_vfree(attr->d.r.runlist);
		}
		attr->d.r.runlist = new;
	}
	if (attr->d.r.len > cnum)
		ntfs_memmove(attr->d.r.runlist + cnum + 1,
			     attr->d.r.runlist + cnum,
			     (attr->d.r.len - cnum) * sizeof(ntfs_runlist));
	attr->d.r.runlist[cnum].lcn = cluster;
	attr->d.r.runlist[cnum].len = len;
	attr->d.r.len++;
	return 0;
}

/**
 * ntfs_extend_attr - extend allocated size of an attribute
 * @ino:	ntfs inode containing the attribute to extend
 * @attr:	attribute which to extend
 * @len:	desired new length for @attr (_not_ the amount to extend by)
 *
 * Extends an attribute. Allocate clusters on the volume which @ino belongs to.
 * Extends the run list accordingly, preferably by extending the last run of
 * the existing run list, first.
 *
 * Only modifies attr->allocated, i.e. doesn't touch attr->size, nor
 * attr->initialized.
 */
int ntfs_extend_attr(ntfs_inode *ino, ntfs_attribute *attr, const __s64 len)
{
	int rlen, rl2_len, err = 0;
	ntfs_cluster_t cluster, clen;
	ntfs_runlist *rl, *rl2;

	if ((attr->flags & (ATTR_IS_COMPRESSED | ATTR_IS_ENCRYPTED)) ||
			ino->record_count > 1)
		return -EOPNOTSUPP;
	/*
	 * FIXME: Don't make non-resident if the attribute type is not right.
	 * For example cannot make index attribute non-resident! (AIA)
	 */
	if (attr->resident) {
		err = ntfs_make_attr_nonresident(ino, attr);
		if (err)
			return err;
	}
	if (len <= attr->allocated)
		return 0;	/* Truly stupid things do sometimes happen. */
	rl = attr->d.r.runlist;
	rlen = attr->d.r.len;
	if (rlen > 0)
		cluster = rl[rlen - 1].lcn + rl[rlen - 1].len;
	else
		/* No preference for allocation space. */
		cluster = (ntfs_cluster_t)-1;
	/*
	 * Calculate the extra space we need, and round up to multiple of
	 * cluster size to get number of new clusters needed.
	 */
	clen = (len - attr->allocated + ino->vol->cluster_size - 1) >>
			ino->vol->cluster_size_bits;
	if (!clen)
		return 0;
	err = ntfs_allocate_clusters(ino->vol, &cluster, &clen, &rl2,
			&rl2_len, DATA_ZONE);
	if (err)
		return err;
	attr->allocated += (__s64)clen << ino->vol->cluster_size_bits;
	if (rlen > 0) {
		err = splice_runlists(&rl, &rlen, rl2, rl2_len);
		ntfs_vfree(rl2);
		if (err)
			return err;
	} else {
		if (rl)
			ntfs_vfree(rl);
		rl = rl2;
		rlen = rl2_len;
	}
	attr->d.r.runlist = rl;
	attr->d.r.len = rlen;
	return 0;
}

int ntfs_make_attr_nonresident(ntfs_inode *ino, ntfs_attribute *attr)
{
	int error;
	ntfs_io io;
	void *data = attr->d.data;
	__s64 len = attr->size;

	attr->d.r.len = 0;
	attr->d.r.runlist = NULL;
	attr->resident = 0;
	/*
	 * ->allocated is updated by ntfs_extend_attr(), while ->initialized
	 * and ->size are updated by ntfs_readwrite_attr(). (AIA)
	 */
	attr->allocated = attr->initialized = 0;
	error = ntfs_extend_attr(ino, attr, len);
	if (error)
		return error; /* FIXME: On error, restore old values. */
	io.fn_put = ntfs_put;
	io.fn_get = ntfs_get;
	io.param = data;
	io.size = len;
	io.do_read = 0;
	return ntfs_readwrite_attr(ino, attr, 0, &io);
}

int ntfs_attr_allnonresident(ntfs_inode *ino)
{
	int i, error = 0;
        ntfs_volume *vol = ino->vol;

	for (i = 0; !error && i < ino->attr_count; i++)
	{
		if (ino->attrs[i].type != vol->at_security_descriptor &&
		    ino->attrs[i].type != vol->at_data)
			continue;
		error = ntfs_make_attr_nonresident(ino, ino->attrs + i);
	}
	return error;
}

/*
 * Resize the attribute to a newsize. attr->allocated and attr->size are
 * updated, but attr->initialized is not changed unless it becomes bigger than
 * attr->size, in which case it is set to attr->size.
 */
int ntfs_resize_attr(ntfs_inode *ino, ntfs_attribute *attr, __s64 newsize)
{
	int error = 0;
	__s64 oldsize = attr->size;
	int clustersizebits = ino->vol->cluster_size_bits;
	int i, count, newcount;
	ntfs_runlist *rl, *rlt;

	if (newsize == oldsize)
		return 0;
	if (attr->flags & (ATTR_IS_COMPRESSED | ATTR_IS_ENCRYPTED))
		return -EOPNOTSUPP;
	if (attr->resident) {
		void *v;
		if (newsize > ino->vol->mft_record_size) {
			error = ntfs_make_attr_nonresident(ino, attr);
			if (error)
				return error;
			return ntfs_resize_attr(ino, attr, newsize);
		}
		v = attr->d.data;
		if (newsize) {
			__s64 minsize = newsize;
			attr->d.data = ntfs_malloc(newsize);
			if (!attr->d.data) {
				ntfs_free(v);
				return -ENOMEM;
			}
			if (newsize > oldsize) {
				minsize = oldsize;
				ntfs_bzero((char*)attr->d.data + oldsize,
					   newsize - oldsize);
			}
			ntfs_memcpy((char*)attr->d.data, v, minsize);
		} else
			attr->d.data = 0;
		ntfs_free(v);
		attr->size = newsize;
		return 0;
	}
	/* Non-resident attribute. */
	rl = attr->d.r.runlist;
	if (newsize < oldsize) {
		int rl_size;
		/*
		 * FIXME: We might be going awfully wrong for newsize = 0,
		 * possibly even leaking memory really badly. But considering
		 * in that case there is more breakage due to -EOPNOTSUPP stuff
		 * further down the code path, who cares for the moment... (AIA)
		 */
		for (i = 0, count = 0; i < attr->d.r.len; i++) {
			if ((__s64)(count + rl[i].len) << clustersizebits >
					newsize) {
				i++;
				break;
			}
			count += (int)rl[i].len;
		}
		newcount = count;
		/* Free unused clusters in current run, unless sparse. */
		if (rl[--i].lcn != (ntfs_cluster_t)-1) {
			ntfs_cluster_t rounded = newsize - ((__s64)count <<
					clustersizebits);
			rounded = (rounded + ino->vol->cluster_size - 1) >>
					clustersizebits;
			error = ntfs_deallocate_cluster_run(ino->vol, 
					rl[i].lcn + rounded,
					rl[i].len - rounded);
			if (error)
				return error; /* FIXME: Incomplete operation. */
			rl[i].len = rounded;
			newcount = count + rounded;
		}
		/* Free all other runs. */
		i++;
		error = ntfs_deallocate_clusters(ino->vol, rl + i,
				attr->d.r.len - i);
		if (error)
			return error; /* FIXME: Incomplete operation. */
		/*
		 * Free space for extra runs in memory if enough memory left
		 * to do so. FIXME: Only do it if it would free memory. (AIA)
		 */
		rl_size = ((i + 1) * sizeof(ntfs_runlist) + PAGE_SIZE - 1) &
				PAGE_MASK;
		if (rl_size < ((attr->d.r.len * sizeof(ntfs_runlist) +
				PAGE_SIZE - 1) & PAGE_MASK)) {
			rlt = ntfs_vmalloc(rl_size);
			if (rlt) {
				ntfs_memcpy(rlt, rl, i * sizeof(ntfs_runlist));
				ntfs_vfree(rl);
				attr->d.r.runlist = rl = rlt;
			}
		}
		rl[i].lcn = (ntfs_cluster_t)-1;
		rl[i].len = (ntfs_cluster_t)0;
		attr->d.r.len = i;
	} else {
		error = ntfs_extend_attr(ino, attr, newsize);
		if (error)
			return error; /* FIXME: Incomplete operation. */
		newcount = (newsize + ino->vol->cluster_size - 1) >>
				clustersizebits;
	}
	/* Fill in new sizes. */
	attr->allocated = (__s64)newcount << clustersizebits;
	attr->size = newsize;
	if (attr->initialized > newsize)
		attr->initialized = newsize;
	if (!newsize)
		error = ntfs_make_attr_resident(ino, attr);
	return error;
}

int ntfs_create_attr(ntfs_inode *ino, int anum, char *aname, void *data,
		int dsize, ntfs_attribute **rattr)
{
	void *name;
	int namelen;
	int found, i;
	int error;
	ntfs_attribute *attr;
	
	if (dsize > ino->vol->mft_record_size)
		/* FIXME: Non-resident attributes. */
		return -EOPNOTSUPP;
	if (aname) {
		namelen = strlen(aname);
		name = ntfs_malloc(2 * namelen);
		if (!name)
			return -ENOMEM;
		ntfs_ascii2uni(name, aname, namelen);
	} else {
		name = 0;
		namelen = 0;
	}
	error = ntfs_new_attr(ino, anum, name, namelen, data, dsize, &i,
			&found);
	if (error || found) {
		ntfs_free(name);
		return error ? error : -EEXIST;
	}
	*rattr = attr = ino->attrs + i;
	/* Allocate a new number.
	 * FIXME: Should this happen on inode writeback?
	 * FIXME: Extension records not supported. */
	error = ntfs_allocate_attr_number(ino, &i);
	if (error)
		return error;
	attr->attrno = i;
	if (attr->attrno + 1 != NTFS_GETU16(ino->attr + 0x28))
		ntfs_error("UH OH! attr->attrno (%i) != NTFS_GETU16(ino->attr "
				"+ 0x28) (%i)\n", attr->attrno,
				NTFS_GETU16(ino->attr + 0x28));
	attr->resident = 1;
	attr->flags = 0;
	attr->cengine = 0;
	attr->size = attr->allocated = attr->initialized = dsize;

	/* FIXME: INDEXED information should come from $AttrDef
	 * Currently, only file names are indexed. As of NTFS v3.0 (Win2k),
	 * this is no longer true. Different attributes can be indexed now. */
	if (anum == ino->vol->at_file_name)
		attr->indexed = 1;
	else
		attr->indexed = 0;
	attr->d.data = ntfs_malloc(dsize);
	if (!attr->d.data)
		return -ENOMEM;
	ntfs_memcpy(attr->d.data, data, dsize);
	return 0;
}

/*
 * Non-resident attributes are stored in runs (intervals of clusters).
 *
 * This function stores in the inode readable information about a non-resident
 * attribute.
 */
static int ntfs_process_runs(ntfs_inode *ino, ntfs_attribute* attr,
		unsigned char *data)
{
	int startvcn, endvcn;
	int vcn, cnum;
	ntfs_cluster_t cluster;
	int len, ctype;
	int er = 0;
	startvcn = NTFS_GETS64(data + 0x10);
	endvcn = NTFS_GETS64(data + 0x18);

	/* Check whether this chunk really belongs to the end. Problem with
	 * this: this functions can get called on the last extent first, before
	 * it is called on the other extents in sequence. This happens when the
	 * base mft record contains the last extent instead of the first one
	 * and the first extent is stored, like any intermediate extents in
	 * extension mft records. This would be difficult to allow the way the
	 * runlist is stored in memory. Thus we fix elsewhere by causing the
	 * attribute list attribute to be processed immediately when found. The
	 * extents will then be processed starting with the first one. */
	for (cnum = 0, vcn = 0; cnum < attr->d.r.len; cnum++)
		vcn += attr->d.r.runlist[cnum].len;
	if (vcn != startvcn) {
		ntfs_debug(DEBUG_FILE3, "ntfs_process_runs: ino = 0x%x, "
			"attr->type = 0x%x, startvcn = 0x%x, endvcn = 0x%x, "
			"vcn = 0x%x, cnum = 0x%x\n", ino->i_number, attr->type,
			startvcn, endvcn, vcn, cnum);
		if (vcn < startvcn) {
			ntfs_error("Problem with runlist in extended record\n");
			return -1;
		}
		/* Tried to insert an already inserted runlist. */
		return 0;
	}
	if (!endvcn) {
		if (!startvcn) {
			/* Allocated length. */
			endvcn = NTFS_GETS64(data + 0x28) - 1;
			endvcn >>= ino->vol->cluster_size_bits;
		} else {
			/* This is an extent. Allocated length is not defined!
			 * Extents must have an endvcn though so this is an
			 * error. */
			ntfs_error("Corrupt attribute extent. (endvcn is "
				"missing)\n");
			return -1;
		}
	}
	data = data + NTFS_GETU16(data + 0x20);
	cnum = attr->d.r.len;
	cluster = 0;
	for (vcn = startvcn; vcn <= endvcn; vcn += len)	{
		if (ntfs_decompress_run(&data, &len, &cluster, &ctype)) {
			ntfs_debug(DEBUG_FILE3, "ntfs_process_runs: "
				"ntfs_decompress_run failed. i_number = 0x%x\n",
				ino->i_number);
			return -1;
		}
		if (ctype)
			er = ntfs_insert_run(attr, cnum, -1, len);
		else
			er = ntfs_insert_run(attr, cnum, cluster, len);
		if (er)
			break;
		cnum++;
	}
	if (er)
		ntfs_error("ntfs_process_runs: ntfs_insert_run failed\n");
	ntfs_debug(DEBUG_FILE3, "ntfs_process_runs: startvcn = 0x%x, vcn = 0x%x"
				", endvcn = 0x%x, cnum = %i\n", startvcn, vcn,
				endvcn, cnum);
	return er;
}
  
/* Insert the attribute starting at attr in the inode ino. */
int ntfs_insert_attribute(ntfs_inode *ino, unsigned char *attrdata)
{
	int i, found;
	int type;
	short int *name;
	int namelen;
	void *data;
	ntfs_attribute *attr;
	int error;

	type = NTFS_GETU32(attrdata);
	namelen = NTFS_GETU8(attrdata + 9);
	ntfs_debug(DEBUG_FILE3, "ntfs_insert_attribute: ino->i_number 0x%x, "
			"attr type 0x%x\n", ino->i_number, type);
	/* Read the attribute's name if it has one. */
	if (!namelen)
		name = 0;
	else {
		/* 1 Unicode character fits in 2 bytes. */
		name = ntfs_malloc(2 * namelen);
		if (!name)
			return -ENOMEM;
		ntfs_memcpy(name, attrdata + NTFS_GETU16(attrdata + 10),
			    2 * namelen);
	}
	/* If resident look for value, too. */
	if (NTFS_GETU8(attrdata + 8) == 0)
		error = ntfs_new_attr(ino, type, name, namelen,
				attrdata + NTFS_GETU16(attrdata + 0x14),
				NTFS_GETU16(attrdata + 0x10), &i, &found);
	else
		error = ntfs_new_attr(ino, type, name, namelen, NULL, 0, &i,
				&found);
	if (error) {
		ntfs_debug(DEBUG_FILE3, "ntfs_insert_attribute: ntfs_new_attr "
				"failed.\n");
		if (name)
			ntfs_free(name);
		return error;
	}
	if (found) {
		/* It's already there, if not resident just process the runs. */
		if (!ino->attrs[i].resident) {
			ntfs_debug(DEBUG_FILE3, "ntfs_insert_attribute:"
						" processing runs 1.\n");
			/* FIXME: Check error code! (AIA) */
			ntfs_process_runs(ino, ino->attrs + i, attrdata);
		}
		return 0;
	}
	attr = ino->attrs + i;
	attr->resident = NTFS_GETU8(attrdata + 8) == 0;
	attr->flags = *(__u16*)(attrdata + 0xC);
	attr->attrno = NTFS_GETU16(attrdata + 0xE);
  
	if (attr->resident) {
		attr->size = NTFS_GETU16(attrdata + 0x10);
		data = attrdata + NTFS_GETU16(attrdata + 0x14);
		attr->d.data = (void*)ntfs_malloc(attr->size);
		if (!attr->d.data)
			return -ENOMEM;
		ntfs_memcpy(attr->d.data, data, attr->size);
		attr->indexed = NTFS_GETU8(attrdata + 0x16);
	} else {
		attr->allocated = NTFS_GETS64(attrdata + 0x28);
		attr->size = NTFS_GETS64(attrdata + 0x30);
		attr->initialized = NTFS_GETS64(attrdata + 0x38);
		attr->cengine = NTFS_GETU16(attrdata + 0x22);
		if (attr->flags & ATTR_IS_COMPRESSED)
			attr->compsize = NTFS_GETS64(attrdata + 0x40);
		ntfs_debug(DEBUG_FILE3, "ntfs_insert_attribute: "
			"attr->allocated = 0x%Lx, attr->size = 0x%Lx, "
			"attr->initialized = 0x%Lx\n", attr->allocated,
			attr->size, attr->initialized);
		ino->attrs[i].d.r.runlist = 0;
		ino->attrs[i].d.r.len = 0;
		ntfs_debug(DEBUG_FILE3, "ntfs_insert_attribute: processing "
				"runs 2.\n");
		/* FIXME: Check error code! (AIA) */
		ntfs_process_runs(ino, attr, attrdata);
	}
	return 0;
}

int ntfs_read_zero(ntfs_io *dest, int size)
{
	int i;
	char *sparse = ntfs_calloc(512);
	if (!sparse)
		return -ENOMEM;
	i = 512;
	while (size) {
		if (i > size)
			i = size;
		dest->fn_put(dest, sparse, i);
		size -= i;
	}
	ntfs_free(sparse);
	return 0;
}

/* Process compressed attributes. */
int ntfs_read_compressed(ntfs_inode *ino, ntfs_attribute *attr, __s64 offset,
			 ntfs_io *dest)
{
	int error = 0;
	int clustersizebits;
	int s_vcn, rnum, vcn, got, l1;
	__s64 copied, len, chunk, offs1, l, chunk2;
	ntfs_cluster_t cluster, cl1;
	char *comp = 0, *comp1;
	char *decomp = 0;
	ntfs_io io;
	ntfs_runlist *rl;

	l = dest->size;
	clustersizebits = ino->vol->cluster_size_bits;
	/* Starting cluster of potential chunk. There are three situations:
	   a) In a large uncompressible or sparse chunk, s_vcn is in the middle
	      of a run.
	   b) s_vcn is right on a run border.
	   c) When several runs make a chunk, s_vcn is before the chunks. */
	s_vcn = offset >> clustersizebits;
	/* Round down to multiple of 16. */
	s_vcn &= ~15;
	rl = attr->d.r.runlist;
	for (rnum = vcn = 0; rnum < attr->d.r.len && vcn + rl->len <= s_vcn;
								rnum++, rl++)
		vcn += rl->len;
	if (rnum == attr->d.r.len) {
		/* Beyond end of file. */
		/* FIXME: Check allocated / initialized. */
		dest->size = 0;
		return 0;
	}
	io.do_read = 1;
	io.fn_put = ntfs_put;
	io.fn_get = 0;
	cluster = rl->lcn;
	len = rl->len;
	copied = 0;
	while (l) {
		chunk = 0;
		if (cluster == (ntfs_cluster_t)-1) {
			/* Sparse cluster. */
			__s64 ll;

			if ((len - (s_vcn - vcn)) & 15)
				ntfs_error("Unexpected sparse chunk size.");
			ll = ((__s64)(vcn + len) << clustersizebits) - offset;
			if (ll > l)
				ll = l;
			chunk = ll;
			error = ntfs_read_zero(dest, ll);
			if (error)
				goto out;
		} else if (dest->do_read) {
			if (!comp) {
				comp = ntfs_malloc(16 << clustersizebits);
				if (!comp) {
					error = -ENOMEM;
					goto out;
				}
			}
			got = 0;
			/* We might need to start in the middle of a run. */
			cl1 = cluster + s_vcn - vcn;
			comp1 = comp;
			do {
				int delta;

				io.param = comp1;
				delta = s_vcn - vcn;
				if (delta < 0)
					delta = 0;
				l1 = len - delta;
				if (l1 > 16 - got)
					l1 = 16 - got;
				io.size = (__s64)l1 << clustersizebits;
				error = ntfs_getput_clusters(ino->vol, cl1, 0,
					       		     &io);
				if (error)
					goto out;
				if (l1 + delta == len) {
					rnum++;
					rl++;
					vcn += len;
					cluster = cl1 = rl->lcn;
					len = rl->len;
				}
				got += l1;
				comp1 += (__s64)l1 << clustersizebits;
			} while (cluster != (ntfs_cluster_t)-1 && got < 16);
							/* Until empty run. */
			chunk = 16 << clustersizebits;
			if (cluster != (ntfs_cluster_t)-1 || got == 16)
				/* Uncompressible */
				comp1 = comp;
			else {
				if (!decomp) {
					decomp = ntfs_malloc(16 << 
							clustersizebits);
					if (!decomp) {
						error = -ENOMEM;
						goto out;
					}
				}
				/* Make sure there are null bytes after the
				 * last block. */
				*(ntfs_u32*)comp1 = 0;
				ntfs_decompress(decomp, comp, chunk);
				comp1 = decomp;
			}
			offs1 = offset - ((__s64)s_vcn << clustersizebits);
			chunk2 = (16 << clustersizebits) - offs1;
			if (chunk2 > l)
				chunk2 = l;
			if (chunk > chunk2)
				chunk = chunk2;
			dest->fn_put(dest, comp1 + offs1, chunk);
		}
		l -= chunk;
		copied += chunk;
		offset += chunk;
		s_vcn = (offset >> clustersizebits) & ~15;
		if (l && offset >= ((__s64)(vcn + len) << clustersizebits)) {
			rnum++;
			rl++;
			vcn += len;
			cluster = rl->lcn;
			len = rl->len;
		}
	}
out:
	if (comp)
		ntfs_free(comp);
	if (decomp)
		ntfs_free(decomp);
	dest->size = copied;
	return error;
}

int ntfs_write_compressed(ntfs_inode *ino, ntfs_attribute *attr, __s64 offset,
		ntfs_io *dest)
{
	return -EOPNOTSUPP;
}

