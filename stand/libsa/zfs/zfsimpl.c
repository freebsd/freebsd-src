/*-
 * Copyright (c) 2007 Doug Rabson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 *	Stand-alone ZFS file reader.
 */

#include <stdbool.h>
#include <sys/endian.h>
#include <sys/stat.h>
#include <sys/stdint.h>
#include <sys/list.h>
#include <sys/zfs_bootenv.h>
#include <machine/_inttypes.h>

#include "zfsimpl.h"
#include "zfssubr.c"

#ifdef HAS_ZSTD_ZFS
extern int zstd_init(void);
#endif

struct zfsmount {
	char			*path;
	const spa_t		*spa;
	objset_phys_t		objset;
	uint64_t		rootobj;
	STAILQ_ENTRY(zfsmount)	next;
};

typedef STAILQ_HEAD(zfs_mnt_list, zfsmount) zfs_mnt_list_t;
static zfs_mnt_list_t zfsmount = STAILQ_HEAD_INITIALIZER(zfsmount);

/*
 * The indirect_child_t represents the vdev that we will read from, when we
 * need to read all copies of the data (e.g. for scrub or reconstruction).
 * For plain (non-mirror) top-level vdevs (i.e. is_vdev is not a mirror),
 * ic_vdev is the same as is_vdev.  However, for mirror top-level vdevs,
 * ic_vdev is a child of the mirror.
 */
typedef struct indirect_child {
	void *ic_data;
	vdev_t *ic_vdev;
} indirect_child_t;

/*
 * The indirect_split_t represents one mapped segment of an i/o to the
 * indirect vdev. For non-split (contiguously-mapped) blocks, there will be
 * only one indirect_split_t, with is_split_offset==0 and is_size==io_size.
 * For split blocks, there will be several of these.
 */
typedef struct indirect_split {
	list_node_t is_node; /* link on iv_splits */

	/*
	 * is_split_offset is the offset into the i/o.
	 * This is the sum of the previous splits' is_size's.
	 */
	uint64_t is_split_offset;

	vdev_t *is_vdev; /* top-level vdev */
	uint64_t is_target_offset; /* offset on is_vdev */
	uint64_t is_size;
	int is_children; /* number of entries in is_child[] */

	/*
	 * is_good_child is the child that we are currently using to
	 * attempt reconstruction.
	 */
	int is_good_child;

	indirect_child_t is_child[1]; /* variable-length */
} indirect_split_t;

/*
 * The indirect_vsd_t is associated with each i/o to the indirect vdev.
 * It is the "Vdev-Specific Data" in the zio_t's io_vsd.
 */
typedef struct indirect_vsd {
	boolean_t iv_split_block;
	boolean_t iv_reconstruct;

	list_t iv_splits; /* list of indirect_split_t's */
} indirect_vsd_t;

/*
 * List of all vdevs, chained through v_alllink.
 */
static vdev_list_t zfs_vdevs;

/*
 * List of supported read-incompatible ZFS features.  Do not add here features
 * marked as ZFEATURE_FLAG_READONLY_COMPAT, they are irrelevant for read-only!
 */
static const char *features_for_read[] = {
	"com.datto:bookmark_v2",
	"com.datto:encryption",
	"com.delphix:bookmark_written",
	"com.delphix:device_removal",
	"com.delphix:embedded_data",
	"com.delphix:extensible_dataset",
	"com.delphix:head_errlog",
	"com.delphix:hole_birth",
	"com.joyent:multi_vdev_crash_dump",
	"com.klarasystems:vdev_zaps_v2",
	"org.freebsd:zstd_compress",
	"org.illumos:lz4_compress",
	"org.illumos:sha512",
	"org.illumos:skein",
	"org.open-zfs:large_blocks",
	"org.openzfs:blake3",
	"org.zfsonlinux:large_dnode",
	NULL
};

/*
 * List of all pools, chained through spa_link.
 */
static spa_list_t zfs_pools;

static const dnode_phys_t *dnode_cache_obj;
static uint64_t dnode_cache_bn;
static char *dnode_cache_buf;

static int zio_read(const spa_t *spa, const blkptr_t *bp, void *buf);
static int zfs_get_root(const spa_t *spa, uint64_t *objid);
static int zfs_rlookup(const spa_t *spa, uint64_t objnum, char *result);
static int zap_lookup(const spa_t *spa, const dnode_phys_t *dnode,
    const char *name, uint64_t integer_size, uint64_t num_integers,
    void *value);
static int objset_get_dnode(const spa_t *, const objset_phys_t *, uint64_t,
    dnode_phys_t *);
static int dnode_read(const spa_t *, const dnode_phys_t *, off_t, void *,
    size_t);
static int vdev_indirect_read(vdev_t *, const blkptr_t *, void *, off_t,
    size_t);
static int vdev_mirror_read(vdev_t *, const blkptr_t *, void *, off_t, size_t);
vdev_indirect_mapping_t *vdev_indirect_mapping_open(spa_t *, objset_phys_t *,
    uint64_t);
vdev_indirect_mapping_entry_phys_t *
    vdev_indirect_mapping_duplicate_adjacent_entries(vdev_t *, uint64_t,
    uint64_t, uint64_t *);

static void
zfs_init(void)
{
	STAILQ_INIT(&zfs_vdevs);
	STAILQ_INIT(&zfs_pools);

	dnode_cache_buf = malloc(SPA_MAXBLOCKSIZE);

	zfs_init_crc();
#ifdef HAS_ZSTD_ZFS
	zstd_init();
#endif
}

static int
nvlist_check_features_for_read(nvlist_t *nvl)
{
	nvlist_t *features = NULL;
	nvs_data_t *data;
	nvp_header_t *nvp;
	nv_string_t *nvp_name;
	int rc;

	rc = nvlist_find(nvl, ZPOOL_CONFIG_FEATURES_FOR_READ,
	    DATA_TYPE_NVLIST, NULL, &features, NULL);
	switch (rc) {
	case 0:
		break;		/* Continue with checks */

	case ENOENT:
		return (0);	/* All features are disabled */

	default:
		return (rc);	/* Error while reading nvlist */
	}

	data = (nvs_data_t *)features->nv_data;
	nvp = &data->nvl_pair;	/* first pair in nvlist */

	while (nvp->encoded_size != 0 && nvp->decoded_size != 0) {
		int i, found;

		nvp_name = (nv_string_t *)((uintptr_t)nvp + sizeof(*nvp));
		found = 0;

		for (i = 0; features_for_read[i] != NULL; i++) {
			if (memcmp(nvp_name->nv_data, features_for_read[i],
			    nvp_name->nv_size) == 0) {
				found = 1;
				break;
			}
		}

		if (!found) {
			printf("ZFS: unsupported feature: %.*s\n",
			    nvp_name->nv_size, nvp_name->nv_data);
			rc = EIO;
		}
		nvp = (nvp_header_t *)((uint8_t *)nvp + nvp->encoded_size);
	}
	nvlist_destroy(features);

	return (rc);
}

static int
vdev_read_phys(vdev_t *vdev, const blkptr_t *bp, void *buf,
    off_t offset, size_t size)
{
	size_t psize;
	int rc;

	if (vdev->v_phys_read == NULL)
		return (ENOTSUP);

	if (bp) {
		psize = BP_GET_PSIZE(bp);
	} else {
		psize = size;
	}

	rc = vdev->v_phys_read(vdev, vdev->v_priv, offset, buf, psize);
	if (rc == 0) {
		if (bp != NULL)
			rc = zio_checksum_verify(vdev->v_spa, bp, buf);
	}

	return (rc);
}

static int
vdev_write_phys(vdev_t *vdev, void *buf, off_t offset, size_t size)
{
	if (vdev->v_phys_write == NULL)
		return (ENOTSUP);

	return (vdev->v_phys_write(vdev, offset, buf, size));
}

typedef struct remap_segment {
	vdev_t *rs_vd;
	uint64_t rs_offset;
	uint64_t rs_asize;
	uint64_t rs_split_offset;
	list_node_t rs_node;
} remap_segment_t;

static remap_segment_t *
rs_alloc(vdev_t *vd, uint64_t offset, uint64_t asize, uint64_t split_offset)
{
	remap_segment_t *rs = malloc(sizeof (remap_segment_t));

	if (rs != NULL) {
		rs->rs_vd = vd;
		rs->rs_offset = offset;
		rs->rs_asize = asize;
		rs->rs_split_offset = split_offset;
	}

	return (rs);
}

vdev_indirect_mapping_t *
vdev_indirect_mapping_open(spa_t *spa, objset_phys_t *os,
    uint64_t mapping_object)
{
	vdev_indirect_mapping_t *vim;
	vdev_indirect_mapping_phys_t *vim_phys;
	int rc;

	vim = calloc(1, sizeof (*vim));
	if (vim == NULL)
		return (NULL);

	vim->vim_dn = calloc(1, sizeof (*vim->vim_dn));
	if (vim->vim_dn == NULL) {
		free(vim);
		return (NULL);
	}

	rc = objset_get_dnode(spa, os, mapping_object, vim->vim_dn);
	if (rc != 0) {
		free(vim->vim_dn);
		free(vim);
		return (NULL);
	}

	vim->vim_spa = spa;
	vim->vim_phys = malloc(sizeof (*vim->vim_phys));
	if (vim->vim_phys == NULL) {
		free(vim->vim_dn);
		free(vim);
		return (NULL);
	}

	vim_phys = (vdev_indirect_mapping_phys_t *)DN_BONUS(vim->vim_dn);
	*vim->vim_phys = *vim_phys;

	vim->vim_objset = os;
	vim->vim_object = mapping_object;
	vim->vim_entries = NULL;

	vim->vim_havecounts =
	    (vim->vim_dn->dn_bonuslen > VDEV_INDIRECT_MAPPING_SIZE_V0);

	return (vim);
}

/*
 * Compare an offset with an indirect mapping entry; there are three
 * possible scenarios:
 *
 *     1. The offset is "less than" the mapping entry; meaning the
 *        offset is less than the source offset of the mapping entry. In
 *        this case, there is no overlap between the offset and the
 *        mapping entry and -1 will be returned.
 *
 *     2. The offset is "greater than" the mapping entry; meaning the
 *        offset is greater than the mapping entry's source offset plus
 *        the entry's size. In this case, there is no overlap between
 *        the offset and the mapping entry and 1 will be returned.
 *
 *        NOTE: If the offset is actually equal to the entry's offset
 *        plus size, this is considered to be "greater" than the entry,
 *        and this case applies (i.e. 1 will be returned). Thus, the
 *        entry's "range" can be considered to be inclusive at its
 *        start, but exclusive at its end: e.g. [src, src + size).
 *
 *     3. The last case to consider is if the offset actually falls
 *        within the mapping entry's range. If this is the case, the
 *        offset is considered to be "equal to" the mapping entry and
 *        0 will be returned.
 *
 *        NOTE: If the offset is equal to the entry's source offset,
 *        this case applies and 0 will be returned. If the offset is
 *        equal to the entry's source plus its size, this case does
 *        *not* apply (see "NOTE" above for scenario 2), and 1 will be
 *        returned.
 */
static int
dva_mapping_overlap_compare(const void *v_key, const void *v_array_elem)
{
	const uint64_t *key = v_key;
	const vdev_indirect_mapping_entry_phys_t *array_elem =
	    v_array_elem;
	uint64_t src_offset = DVA_MAPPING_GET_SRC_OFFSET(array_elem);

	if (*key < src_offset) {
		return (-1);
	} else if (*key < src_offset + DVA_GET_ASIZE(&array_elem->vimep_dst)) {
		return (0);
	} else {
		return (1);
	}
}

/*
 * Return array entry.
 */
static vdev_indirect_mapping_entry_phys_t *
vdev_indirect_mapping_entry(vdev_indirect_mapping_t *vim, uint64_t index)
{
	uint64_t size;
	off_t offset = 0;
	int rc;

	if (vim->vim_phys->vimp_num_entries == 0)
		return (NULL);

	if (vim->vim_entries == NULL) {
		uint64_t bsize;

		bsize = vim->vim_dn->dn_datablkszsec << SPA_MINBLOCKSHIFT;
		size = vim->vim_phys->vimp_num_entries *
		    sizeof (*vim->vim_entries);
		if (size > bsize) {
			size = bsize / sizeof (*vim->vim_entries);
			size *= sizeof (*vim->vim_entries);
		}
		vim->vim_entries = malloc(size);
		if (vim->vim_entries == NULL)
			return (NULL);
		vim->vim_num_entries = size / sizeof (*vim->vim_entries);
		offset = index * sizeof (*vim->vim_entries);
	}

	/* We have data in vim_entries */
	if (offset == 0) {
		if (index >= vim->vim_entry_offset &&
		    index <= vim->vim_entry_offset + vim->vim_num_entries) {
			index -= vim->vim_entry_offset;
			return (&vim->vim_entries[index]);
		}
		offset = index * sizeof (*vim->vim_entries);
	}

	vim->vim_entry_offset = index;
	size = vim->vim_num_entries * sizeof (*vim->vim_entries);
	rc = dnode_read(vim->vim_spa, vim->vim_dn, offset, vim->vim_entries,
	    size);
	if (rc != 0) {
		/* Read error, invalidate vim_entries. */
		free(vim->vim_entries);
		vim->vim_entries = NULL;
		return (NULL);
	}
	index -= vim->vim_entry_offset;
	return (&vim->vim_entries[index]);
}

/*
 * Returns the mapping entry for the given offset.
 *
 * It's possible that the given offset will not be in the mapping table
 * (i.e. no mapping entries contain this offset), in which case, the
 * return value depends on the "next_if_missing" parameter.
 *
 * If the offset is not found in the table and "next_if_missing" is
 * B_FALSE, then NULL will always be returned. The behavior is intended
 * to allow consumers to get the entry corresponding to the offset
 * parameter, iff the offset overlaps with an entry in the table.
 *
 * If the offset is not found in the table and "next_if_missing" is
 * B_TRUE, then the entry nearest to the given offset will be returned,
 * such that the entry's source offset is greater than the offset
 * passed in (i.e. the "next" mapping entry in the table is returned, if
 * the offset is missing from the table). If there are no entries whose
 * source offset is greater than the passed in offset, NULL is returned.
 */
static vdev_indirect_mapping_entry_phys_t *
vdev_indirect_mapping_entry_for_offset(vdev_indirect_mapping_t *vim,
    uint64_t offset)
{
	ASSERT(vim->vim_phys->vimp_num_entries > 0);

	vdev_indirect_mapping_entry_phys_t *entry;

	uint64_t last = vim->vim_phys->vimp_num_entries - 1;
	uint64_t base = 0;

	/*
	 * We don't define these inside of the while loop because we use
	 * their value in the case that offset isn't in the mapping.
	 */
	uint64_t mid;
	int result;

	while (last >= base) {
		mid = base + ((last - base) >> 1);

		entry = vdev_indirect_mapping_entry(vim, mid);
		if (entry == NULL)
			break;
		result = dva_mapping_overlap_compare(&offset, entry);

		if (result == 0) {
			break;
		} else if (result < 0) {
			last = mid - 1;
		} else {
			base = mid + 1;
		}
	}
	return (entry);
}

/*
 * Given an indirect vdev and an extent on that vdev, it duplicates the
 * physical entries of the indirect mapping that correspond to the extent
 * to a new array and returns a pointer to it. In addition, copied_entries
 * is populated with the number of mapping entries that were duplicated.
 *
 * Finally, since we are doing an allocation, it is up to the caller to
 * free the array allocated in this function.
 */
vdev_indirect_mapping_entry_phys_t *
vdev_indirect_mapping_duplicate_adjacent_entries(vdev_t *vd, uint64_t offset,
    uint64_t asize, uint64_t *copied_entries)
{
	vdev_indirect_mapping_entry_phys_t *duplicate_mappings = NULL;
	vdev_indirect_mapping_t *vim = vd->v_mapping;
	uint64_t entries = 0;

	vdev_indirect_mapping_entry_phys_t *first_mapping =
	    vdev_indirect_mapping_entry_for_offset(vim, offset);
	ASSERT3P(first_mapping, !=, NULL);

	vdev_indirect_mapping_entry_phys_t *m = first_mapping;
	while (asize > 0) {
		uint64_t size = DVA_GET_ASIZE(&m->vimep_dst);
		uint64_t inner_offset = offset - DVA_MAPPING_GET_SRC_OFFSET(m);
		uint64_t inner_size = MIN(asize, size - inner_offset);

		offset += inner_size;
		asize -= inner_size;
		entries++;
		m++;
	}

	size_t copy_length = entries * sizeof (*first_mapping);
	duplicate_mappings = malloc(copy_length);
	if (duplicate_mappings != NULL)
		bcopy(first_mapping, duplicate_mappings, copy_length);
	else
		entries = 0;

	*copied_entries = entries;

	return (duplicate_mappings);
}

static vdev_t *
vdev_lookup_top(spa_t *spa, uint64_t vdev)
{
	vdev_t *rvd;
	vdev_list_t *vlist;

	vlist = &spa->spa_root_vdev->v_children;
	STAILQ_FOREACH(rvd, vlist, v_childlink)
		if (rvd->v_id == vdev)
			break;

	return (rvd);
}

/*
 * This is a callback for vdev_indirect_remap() which allocates an
 * indirect_split_t for each split segment and adds it to iv_splits.
 */
static void
vdev_indirect_gather_splits(uint64_t split_offset, vdev_t *vd, uint64_t offset,
    uint64_t size, void *arg)
{
	int n = 1;
	zio_t *zio = arg;
	indirect_vsd_t *iv = zio->io_vsd;

	if (vd->v_read == vdev_indirect_read)
		return;

	if (vd->v_read == vdev_mirror_read)
		n = vd->v_nchildren;

	indirect_split_t *is =
	    malloc(offsetof(indirect_split_t, is_child[n]));
	if (is == NULL) {
		zio->io_error = ENOMEM;
		return;
	}
	bzero(is, offsetof(indirect_split_t, is_child[n]));

	is->is_children = n;
	is->is_size = size;
	is->is_split_offset = split_offset;
	is->is_target_offset = offset;
	is->is_vdev = vd;

	/*
	 * Note that we only consider multiple copies of the data for
	 * *mirror* vdevs.  We don't for "replacing" or "spare" vdevs, even
	 * though they use the same ops as mirror, because there's only one
	 * "good" copy under the replacing/spare.
	 */
	if (vd->v_read == vdev_mirror_read) {
		int i = 0;
		vdev_t *kid;

		STAILQ_FOREACH(kid, &vd->v_children, v_childlink) {
			is->is_child[i++].ic_vdev = kid;
		}
	} else {
		is->is_child[0].ic_vdev = vd;
	}

	list_insert_tail(&iv->iv_splits, is);
}

static void
vdev_indirect_remap(vdev_t *vd, uint64_t offset, uint64_t asize, void *arg)
{
	list_t stack;
	spa_t *spa = vd->v_spa;
	zio_t *zio = arg;
	remap_segment_t *rs;

	list_create(&stack, sizeof (remap_segment_t),
	    offsetof(remap_segment_t, rs_node));

	rs = rs_alloc(vd, offset, asize, 0);
	if (rs == NULL) {
		printf("vdev_indirect_remap: out of memory.\n");
		zio->io_error = ENOMEM;
	}
	for (; rs != NULL; rs = list_remove_head(&stack)) {
		vdev_t *v = rs->rs_vd;
		uint64_t num_entries = 0;
		/* vdev_indirect_mapping_t *vim = v->v_mapping; */
		vdev_indirect_mapping_entry_phys_t *mapping =
		    vdev_indirect_mapping_duplicate_adjacent_entries(v,
		    rs->rs_offset, rs->rs_asize, &num_entries);

		if (num_entries == 0)
			zio->io_error = ENOMEM;

		for (uint64_t i = 0; i < num_entries; i++) {
			vdev_indirect_mapping_entry_phys_t *m = &mapping[i];
			uint64_t size = DVA_GET_ASIZE(&m->vimep_dst);
			uint64_t dst_offset = DVA_GET_OFFSET(&m->vimep_dst);
			uint64_t dst_vdev = DVA_GET_VDEV(&m->vimep_dst);
			uint64_t inner_offset = rs->rs_offset -
			    DVA_MAPPING_GET_SRC_OFFSET(m);
			uint64_t inner_size =
			    MIN(rs->rs_asize, size - inner_offset);
			vdev_t *dst_v = vdev_lookup_top(spa, dst_vdev);

			if (dst_v->v_read == vdev_indirect_read) {
				remap_segment_t *o;

				o = rs_alloc(dst_v, dst_offset + inner_offset,
				    inner_size, rs->rs_split_offset);
				if (o == NULL) {
					printf("vdev_indirect_remap: "
					    "out of memory.\n");
					zio->io_error = ENOMEM;
					break;
				}

				list_insert_head(&stack, o);
			}
			vdev_indirect_gather_splits(rs->rs_split_offset, dst_v,
			    dst_offset + inner_offset,
			    inner_size, arg);

			/*
			 * vdev_indirect_gather_splits can have memory
			 * allocation error, we can not recover from it.
			 */
			if (zio->io_error != 0)
				break;
			rs->rs_offset += inner_size;
			rs->rs_asize -= inner_size;
			rs->rs_split_offset += inner_size;
		}

		free(mapping);
		free(rs);
		if (zio->io_error != 0)
			break;
	}

	list_destroy(&stack);
}

static void
vdev_indirect_map_free(zio_t *zio)
{
	indirect_vsd_t *iv = zio->io_vsd;
	indirect_split_t *is;

	while ((is = list_head(&iv->iv_splits)) != NULL) {
		for (int c = 0; c < is->is_children; c++) {
			indirect_child_t *ic = &is->is_child[c];
			free(ic->ic_data);
		}
		list_remove(&iv->iv_splits, is);
		free(is);
	}
	free(iv);
}

static int
vdev_indirect_read(vdev_t *vdev, const blkptr_t *bp, void *buf,
    off_t offset, size_t bytes)
{
	zio_t zio;
	spa_t *spa = vdev->v_spa;
	indirect_vsd_t *iv;
	indirect_split_t *first;
	int rc = EIO;

	iv = calloc(1, sizeof(*iv));
	if (iv == NULL)
		return (ENOMEM);

	list_create(&iv->iv_splits,
	    sizeof (indirect_split_t), offsetof(indirect_split_t, is_node));

	bzero(&zio, sizeof(zio));
	zio.io_spa = spa;
	zio.io_bp = (blkptr_t *)bp;
	zio.io_data = buf;
	zio.io_size = bytes;
	zio.io_offset = offset;
	zio.io_vd = vdev;
	zio.io_vsd = iv;

	if (vdev->v_mapping == NULL) {
		vdev_indirect_config_t *vic;

		vic = &vdev->vdev_indirect_config;
		vdev->v_mapping = vdev_indirect_mapping_open(spa,
		    spa->spa_mos, vic->vic_mapping_object);
	}

	vdev_indirect_remap(vdev, offset, bytes, &zio);
	if (zio.io_error != 0)
		return (zio.io_error);

	first = list_head(&iv->iv_splits);
	if (first->is_size == zio.io_size) {
		/*
		 * This is not a split block; we are pointing to the entire
		 * data, which will checksum the same as the original data.
		 * Pass the BP down so that the child i/o can verify the
		 * checksum, and try a different location if available
		 * (e.g. on a mirror).
		 *
		 * While this special case could be handled the same as the
		 * general (split block) case, doing it this way ensures
		 * that the vast majority of blocks on indirect vdevs
		 * (which are not split) are handled identically to blocks
		 * on non-indirect vdevs.  This allows us to be less strict
		 * about performance in the general (but rare) case.
		 */
		rc = first->is_vdev->v_read(first->is_vdev, zio.io_bp,
		    zio.io_data, first->is_target_offset, bytes);
	} else {
		iv->iv_split_block = B_TRUE;
		/*
		 * Read one copy of each split segment, from the
		 * top-level vdev.  Since we don't know the
		 * checksum of each split individually, the child
		 * zio can't ensure that we get the right data.
		 * E.g. if it's a mirror, it will just read from a
		 * random (healthy) leaf vdev.  We have to verify
		 * the checksum in vdev_indirect_io_done().
		 */
		for (indirect_split_t *is = list_head(&iv->iv_splits);
		    is != NULL; is = list_next(&iv->iv_splits, is)) {
			char *ptr = zio.io_data;

			rc = is->is_vdev->v_read(is->is_vdev, zio.io_bp,
			    ptr + is->is_split_offset, is->is_target_offset,
			    is->is_size);
		}
		if (zio_checksum_verify(spa, zio.io_bp, zio.io_data))
			rc = ECKSUM;
		else
			rc = 0;
	}

	vdev_indirect_map_free(&zio);
	if (rc == 0)
		rc = zio.io_error;

	return (rc);
}

static int
vdev_disk_read(vdev_t *vdev, const blkptr_t *bp, void *buf,
    off_t offset, size_t bytes)
{

	return (vdev_read_phys(vdev, bp, buf,
	    offset + VDEV_LABEL_START_SIZE, bytes));
}

static int
vdev_missing_read(vdev_t *vdev __unused, const blkptr_t *bp __unused,
    void *buf __unused, off_t offset __unused, size_t bytes __unused)
{

	return (ENOTSUP);
}

static int
vdev_mirror_read(vdev_t *vdev, const blkptr_t *bp, void *buf,
    off_t offset, size_t bytes)
{
	vdev_t *kid;
	int rc;

	rc = EIO;
	STAILQ_FOREACH(kid, &vdev->v_children, v_childlink) {
		if (kid->v_state != VDEV_STATE_HEALTHY)
			continue;
		rc = kid->v_read(kid, bp, buf, offset, bytes);
		if (!rc)
			return (0);
	}

	return (rc);
}

static int
vdev_replacing_read(vdev_t *vdev, const blkptr_t *bp, void *buf,
    off_t offset, size_t bytes)
{
	vdev_t *kid;

	/*
	 * Here we should have two kids:
	 * First one which is the one we are replacing and we can trust
	 * only this one to have valid data, but it might not be present.
	 * Second one is that one we are replacing with. It is most likely
	 * healthy, but we can't trust it has needed data, so we won't use it.
	 */
	kid = STAILQ_FIRST(&vdev->v_children);
	if (kid == NULL)
		return (EIO);
	if (kid->v_state != VDEV_STATE_HEALTHY)
		return (EIO);
	return (kid->v_read(kid, bp, buf, offset, bytes));
}

static vdev_t *
vdev_find(uint64_t guid)
{
	vdev_t *vdev;

	STAILQ_FOREACH(vdev, &zfs_vdevs, v_alllink)
		if (vdev->v_guid == guid)
			return (vdev);

	return (0);
}

static vdev_t *
vdev_create(uint64_t guid, vdev_read_t *_read)
{
	vdev_t *vdev;
	vdev_indirect_config_t *vic;

	vdev = calloc(1, sizeof(vdev_t));
	if (vdev != NULL) {
		STAILQ_INIT(&vdev->v_children);
		vdev->v_guid = guid;
		vdev->v_read = _read;

		/*
		 * root vdev has no read function, we use this fact to
		 * skip setting up data we do not need for root vdev.
		 * We only point root vdev from spa.
		 */
		if (_read != NULL) {
			vic = &vdev->vdev_indirect_config;
			vic->vic_prev_indirect_vdev = UINT64_MAX;
			STAILQ_INSERT_TAIL(&zfs_vdevs, vdev, v_alllink);
		}
	}

	return (vdev);
}

static void
vdev_set_initial_state(vdev_t *vdev, const nvlist_t *nvlist)
{
	uint64_t is_offline, is_faulted, is_degraded, is_removed, isnt_present;
	uint64_t is_log;

	is_offline = is_removed = is_faulted = is_degraded = isnt_present = 0;
	is_log = 0;
	(void) nvlist_find(nvlist, ZPOOL_CONFIG_OFFLINE, DATA_TYPE_UINT64, NULL,
	    &is_offline, NULL);
	(void) nvlist_find(nvlist, ZPOOL_CONFIG_REMOVED, DATA_TYPE_UINT64, NULL,
	    &is_removed, NULL);
	(void) nvlist_find(nvlist, ZPOOL_CONFIG_FAULTED, DATA_TYPE_UINT64, NULL,
	    &is_faulted, NULL);
	(void) nvlist_find(nvlist, ZPOOL_CONFIG_DEGRADED, DATA_TYPE_UINT64,
	    NULL, &is_degraded, NULL);
	(void) nvlist_find(nvlist, ZPOOL_CONFIG_NOT_PRESENT, DATA_TYPE_UINT64,
	    NULL, &isnt_present, NULL);
	(void) nvlist_find(nvlist, ZPOOL_CONFIG_IS_LOG, DATA_TYPE_UINT64, NULL,
	    &is_log, NULL);

	if (is_offline != 0)
		vdev->v_state = VDEV_STATE_OFFLINE;
	else if (is_removed != 0)
		vdev->v_state = VDEV_STATE_REMOVED;
	else if (is_faulted != 0)
		vdev->v_state = VDEV_STATE_FAULTED;
	else if (is_degraded != 0)
		vdev->v_state = VDEV_STATE_DEGRADED;
	else if (isnt_present != 0)
		vdev->v_state = VDEV_STATE_CANT_OPEN;

	vdev->v_islog = is_log != 0;
}

static int
vdev_init(uint64_t guid, const nvlist_t *nvlist, vdev_t **vdevp)
{
	uint64_t id, ashift, asize, nparity;
	const char *path;
	const char *type;
	int len, pathlen;
	char *name;
	vdev_t *vdev;

	if (nvlist_find(nvlist, ZPOOL_CONFIG_ID, DATA_TYPE_UINT64, NULL, &id,
	    NULL) ||
	    nvlist_find(nvlist, ZPOOL_CONFIG_TYPE, DATA_TYPE_STRING, NULL,
	    &type, &len)) {
		return (ENOENT);
	}

	if (memcmp(type, VDEV_TYPE_MIRROR, len) != 0 &&
	    memcmp(type, VDEV_TYPE_DISK, len) != 0 &&
#ifdef ZFS_TEST
	    memcmp(type, VDEV_TYPE_FILE, len) != 0 &&
#endif
	    memcmp(type, VDEV_TYPE_RAIDZ, len) != 0 &&
	    memcmp(type, VDEV_TYPE_INDIRECT, len) != 0 &&
	    memcmp(type, VDEV_TYPE_REPLACING, len) != 0 &&
	    memcmp(type, VDEV_TYPE_HOLE, len) != 0) {
		printf("ZFS: can only boot from disk, mirror, raidz1, "
		    "raidz2 and raidz3 vdevs, got: %.*s\n", len, type);
		return (EIO);
	}

	if (memcmp(type, VDEV_TYPE_MIRROR, len) == 0)
		vdev = vdev_create(guid, vdev_mirror_read);
	else if (memcmp(type, VDEV_TYPE_RAIDZ, len) == 0)
		vdev = vdev_create(guid, vdev_raidz_read);
	else if (memcmp(type, VDEV_TYPE_REPLACING, len) == 0)
		vdev = vdev_create(guid, vdev_replacing_read);
	else if (memcmp(type, VDEV_TYPE_INDIRECT, len) == 0) {
		vdev_indirect_config_t *vic;

		vdev = vdev_create(guid, vdev_indirect_read);
		if (vdev != NULL) {
			vdev->v_state = VDEV_STATE_HEALTHY;
			vic = &vdev->vdev_indirect_config;

			nvlist_find(nvlist,
			    ZPOOL_CONFIG_INDIRECT_OBJECT,
			    DATA_TYPE_UINT64,
			    NULL, &vic->vic_mapping_object, NULL);
			nvlist_find(nvlist,
			    ZPOOL_CONFIG_INDIRECT_BIRTHS,
			    DATA_TYPE_UINT64,
			    NULL, &vic->vic_births_object, NULL);
			nvlist_find(nvlist,
			    ZPOOL_CONFIG_PREV_INDIRECT_VDEV,
			    DATA_TYPE_UINT64,
			    NULL, &vic->vic_prev_indirect_vdev, NULL);
		}
	} else if (memcmp(type, VDEV_TYPE_HOLE, len) == 0) {
		vdev = vdev_create(guid, vdev_missing_read);
	} else {
		vdev = vdev_create(guid, vdev_disk_read);
	}

	if (vdev == NULL)
		return (ENOMEM);

	vdev_set_initial_state(vdev, nvlist);
	vdev->v_id = id;
	if (nvlist_find(nvlist, ZPOOL_CONFIG_ASHIFT,
	    DATA_TYPE_UINT64, NULL, &ashift, NULL) == 0)
		vdev->v_ashift = ashift;

	if (nvlist_find(nvlist, ZPOOL_CONFIG_ASIZE,
	    DATA_TYPE_UINT64, NULL, &asize, NULL) == 0) {
		vdev->v_psize = asize +
		    VDEV_LABEL_START_SIZE + VDEV_LABEL_END_SIZE;
	}

	if (nvlist_find(nvlist, ZPOOL_CONFIG_NPARITY,
	    DATA_TYPE_UINT64, NULL, &nparity, NULL) == 0)
		vdev->v_nparity = nparity;

	if (nvlist_find(nvlist, ZPOOL_CONFIG_PATH,
	    DATA_TYPE_STRING, NULL, &path, &pathlen) == 0) {
		char prefix[] = "/dev/";

		len = strlen(prefix);
		if (len < pathlen && memcmp(path, prefix, len) == 0) {
			path += len;
			pathlen -= len;
		}
		name = malloc(pathlen + 1);
		bcopy(path, name, pathlen);
		name[pathlen] = '\0';
		vdev->v_name = name;
	} else {
		name = NULL;
		if (memcmp(type, VDEV_TYPE_RAIDZ, len) == 0) {
			if (vdev->v_nparity < 1 ||
			    vdev->v_nparity > 3) {
				printf("ZFS: invalid raidz parity: %d\n",
				    vdev->v_nparity);
				return (EIO);
			}
			(void) asprintf(&name, "%.*s%d-%" PRIu64, len, type,
			    vdev->v_nparity, id);
		} else {
			(void) asprintf(&name, "%.*s-%" PRIu64, len, type, id);
		}
		vdev->v_name = name;
	}
	*vdevp = vdev;
	return (0);
}

/*
 * Find slot for vdev. We return either NULL to signal to use
 * STAILQ_INSERT_HEAD, or we return link element to be used with
 * STAILQ_INSERT_AFTER.
 */
static vdev_t *
vdev_find_previous(vdev_t *top_vdev, vdev_t *vdev)
{
	vdev_t *v, *previous;

	if (STAILQ_EMPTY(&top_vdev->v_children))
		return (NULL);

	previous = NULL;
	STAILQ_FOREACH(v, &top_vdev->v_children, v_childlink) {
		if (v->v_id > vdev->v_id)
			return (previous);

		if (v->v_id == vdev->v_id)
			return (v);

		if (v->v_id < vdev->v_id)
			previous = v;
	}
	return (previous);
}

static size_t
vdev_child_count(vdev_t *vdev)
{
	vdev_t *v;
	size_t count;

	count = 0;
	STAILQ_FOREACH(v, &vdev->v_children, v_childlink) {
		count++;
	}
	return (count);
}

/*
 * Insert vdev into top_vdev children list. List is ordered by v_id.
 */
static void
vdev_insert(vdev_t *top_vdev, vdev_t *vdev)
{
	vdev_t *previous;
	size_t count;

	/*
	 * The top level vdev can appear in random order, depending how
	 * the firmware is presenting the disk devices.
	 * However, we will insert vdev to create list ordered by v_id,
	 * so we can use either STAILQ_INSERT_HEAD or STAILQ_INSERT_AFTER
	 * as STAILQ does not have insert before.
	 */
	previous = vdev_find_previous(top_vdev, vdev);

	if (previous == NULL) {
		STAILQ_INSERT_HEAD(&top_vdev->v_children, vdev, v_childlink);
	} else if (previous->v_id == vdev->v_id) {
		/*
		 * This vdev was configured from label config,
		 * do not insert duplicate.
		 */
		return;
	} else {
		STAILQ_INSERT_AFTER(&top_vdev->v_children, previous, vdev,
		    v_childlink);
	}

	count = vdev_child_count(top_vdev);
	if (top_vdev->v_nchildren < count)
		top_vdev->v_nchildren = count;
}

static int
vdev_from_nvlist(spa_t *spa, uint64_t top_guid, const nvlist_t *nvlist)
{
	vdev_t *top_vdev, *vdev;
	nvlist_t **kids = NULL;
	int rc, nkids;

	/* Get top vdev. */
	top_vdev = vdev_find(top_guid);
	if (top_vdev == NULL) {
		rc = vdev_init(top_guid, nvlist, &top_vdev);
		if (rc != 0)
			return (rc);
		top_vdev->v_spa = spa;
		top_vdev->v_top = top_vdev;
		vdev_insert(spa->spa_root_vdev, top_vdev);
	}

	/* Add children if there are any. */
	rc = nvlist_find(nvlist, ZPOOL_CONFIG_CHILDREN, DATA_TYPE_NVLIST_ARRAY,
	    &nkids, &kids, NULL);
	if (rc == 0) {
		for (int i = 0; i < nkids; i++) {
			uint64_t guid;

			rc = nvlist_find(kids[i], ZPOOL_CONFIG_GUID,
			    DATA_TYPE_UINT64, NULL, &guid, NULL);
			if (rc != 0)
				goto done;

			rc = vdev_init(guid, kids[i], &vdev);
			if (rc != 0)
				goto done;

			vdev->v_spa = spa;
			vdev->v_top = top_vdev;
			vdev_insert(top_vdev, vdev);
		}
	} else {
		/*
		 * When there are no children, nvlist_find() does return
		 * error, reset it because leaf devices have no children.
		 */
		rc = 0;
	}
done:
	if (kids != NULL) {
		for (int i = 0; i < nkids; i++)
			nvlist_destroy(kids[i]);
		free(kids);
	}

	return (rc);
}

static int
vdev_init_from_label(spa_t *spa, const nvlist_t *nvlist)
{
	uint64_t pool_guid, top_guid;
	nvlist_t *vdevs;
	int rc;

	if (nvlist_find(nvlist, ZPOOL_CONFIG_POOL_GUID, DATA_TYPE_UINT64,
	    NULL, &pool_guid, NULL) ||
	    nvlist_find(nvlist, ZPOOL_CONFIG_TOP_GUID, DATA_TYPE_UINT64,
	    NULL, &top_guid, NULL) ||
	    nvlist_find(nvlist, ZPOOL_CONFIG_VDEV_TREE, DATA_TYPE_NVLIST,
	    NULL, &vdevs, NULL)) {
		printf("ZFS: can't find vdev details\n");
		return (ENOENT);
	}

	rc = vdev_from_nvlist(spa, top_guid, vdevs);
	nvlist_destroy(vdevs);
	return (rc);
}

static void
vdev_set_state(vdev_t *vdev)
{
	vdev_t *kid;
	int good_kids;
	int bad_kids;

	STAILQ_FOREACH(kid, &vdev->v_children, v_childlink) {
		vdev_set_state(kid);
	}

	/*
	 * A mirror or raidz is healthy if all its kids are healthy. A
	 * mirror is degraded if any of its kids is healthy; a raidz
	 * is degraded if at most nparity kids are offline.
	 */
	if (STAILQ_FIRST(&vdev->v_children)) {
		good_kids = 0;
		bad_kids = 0;
		STAILQ_FOREACH(kid, &vdev->v_children, v_childlink) {
			if (kid->v_state == VDEV_STATE_HEALTHY)
				good_kids++;
			else
				bad_kids++;
		}
		if (bad_kids == 0) {
			vdev->v_state = VDEV_STATE_HEALTHY;
		} else {
			if (vdev->v_read == vdev_mirror_read) {
				if (good_kids) {
					vdev->v_state = VDEV_STATE_DEGRADED;
				} else {
					vdev->v_state = VDEV_STATE_OFFLINE;
				}
			} else if (vdev->v_read == vdev_raidz_read) {
				if (bad_kids > vdev->v_nparity) {
					vdev->v_state = VDEV_STATE_OFFLINE;
				} else {
					vdev->v_state = VDEV_STATE_DEGRADED;
				}
			}
		}
	}
}

static int
vdev_update_from_nvlist(uint64_t top_guid, const nvlist_t *nvlist)
{
	vdev_t *vdev;
	nvlist_t **kids = NULL;
	int rc, nkids;

	/* Update top vdev. */
	vdev = vdev_find(top_guid);
	if (vdev != NULL)
		vdev_set_initial_state(vdev, nvlist);

	/* Update children if there are any. */
	rc = nvlist_find(nvlist, ZPOOL_CONFIG_CHILDREN, DATA_TYPE_NVLIST_ARRAY,
	    &nkids, &kids, NULL);
	if (rc == 0) {
		for (int i = 0; i < nkids; i++) {
			uint64_t guid;

			rc = nvlist_find(kids[i], ZPOOL_CONFIG_GUID,
			    DATA_TYPE_UINT64, NULL, &guid, NULL);
			if (rc != 0)
				break;

			vdev = vdev_find(guid);
			if (vdev != NULL)
				vdev_set_initial_state(vdev, kids[i]);
		}
	} else {
		rc = 0;
	}
	if (kids != NULL) {
		for (int i = 0; i < nkids; i++)
			nvlist_destroy(kids[i]);
		free(kids);
	}

	return (rc);
}

static int
vdev_init_from_nvlist(spa_t *spa, const nvlist_t *nvlist)
{
	uint64_t pool_guid, vdev_children;
	nvlist_t *vdevs = NULL, **kids = NULL;
	int rc, nkids;

	if (nvlist_find(nvlist, ZPOOL_CONFIG_POOL_GUID, DATA_TYPE_UINT64,
	    NULL, &pool_guid, NULL) ||
	    nvlist_find(nvlist, ZPOOL_CONFIG_VDEV_CHILDREN, DATA_TYPE_UINT64,
	    NULL, &vdev_children, NULL) ||
	    nvlist_find(nvlist, ZPOOL_CONFIG_VDEV_TREE, DATA_TYPE_NVLIST,
	    NULL, &vdevs, NULL)) {
		printf("ZFS: can't find vdev details\n");
		return (ENOENT);
	}

	/* Wrong guid?! */
	if (spa->spa_guid != pool_guid) {
		nvlist_destroy(vdevs);
		return (EINVAL);
	}

	spa->spa_root_vdev->v_nchildren = vdev_children;

	rc = nvlist_find(vdevs, ZPOOL_CONFIG_CHILDREN, DATA_TYPE_NVLIST_ARRAY,
	    &nkids, &kids, NULL);
	nvlist_destroy(vdevs);

	/*
	 * MOS config has at least one child for root vdev.
	 */
	if (rc != 0)
		return (rc);

	for (int i = 0; i < nkids; i++) {
		uint64_t guid;
		vdev_t *vdev;

		rc = nvlist_find(kids[i], ZPOOL_CONFIG_GUID, DATA_TYPE_UINT64,
		    NULL, &guid, NULL);
		if (rc != 0)
			break;
		vdev = vdev_find(guid);
		/*
		 * Top level vdev is missing, create it.
		 */
		if (vdev == NULL)
			rc = vdev_from_nvlist(spa, guid, kids[i]);
		else
			rc = vdev_update_from_nvlist(guid, kids[i]);
		if (rc != 0)
			break;
	}
	if (kids != NULL) {
		for (int i = 0; i < nkids; i++)
			nvlist_destroy(kids[i]);
		free(kids);
	}

	/*
	 * Re-evaluate top-level vdev state.
	 */
	vdev_set_state(spa->spa_root_vdev);

	return (rc);
}

static spa_t *
spa_find_by_guid(uint64_t guid)
{
	spa_t *spa;

	STAILQ_FOREACH(spa, &zfs_pools, spa_link)
		if (spa->spa_guid == guid)
			return (spa);

	return (NULL);
}

static spa_t *
spa_find_by_name(const char *name)
{
	spa_t *spa;

	STAILQ_FOREACH(spa, &zfs_pools, spa_link)
		if (strcmp(spa->spa_name, name) == 0)
			return (spa);

	return (NULL);
}

static spa_t *
spa_create(uint64_t guid, const char *name)
{
	spa_t *spa;

	if ((spa = calloc(1, sizeof(spa_t))) == NULL)
		return (NULL);
	if ((spa->spa_name = strdup(name)) == NULL) {
		free(spa);
		return (NULL);
	}
	spa->spa_uberblock = &spa->spa_uberblock_master;
	spa->spa_mos = &spa->spa_mos_master;
	spa->spa_guid = guid;
	spa->spa_root_vdev = vdev_create(guid, NULL);
	if (spa->spa_root_vdev == NULL) {
		free(spa->spa_name);
		free(spa);
		return (NULL);
	}
	spa->spa_root_vdev->v_name = strdup("root");
	STAILQ_INSERT_TAIL(&zfs_pools, spa, spa_link);

	return (spa);
}

static const char *
state_name(vdev_state_t state)
{
	static const char *names[] = {
		"UNKNOWN",
		"CLOSED",
		"OFFLINE",
		"REMOVED",
		"CANT_OPEN",
		"FAULTED",
		"DEGRADED",
		"ONLINE"
	};
	return (names[state]);
}

#ifdef BOOT2

#define pager_printf printf

#else

static int
pager_printf(const char *fmt, ...)
{
	char line[80];
	va_list args;

	va_start(args, fmt);
	vsnprintf(line, sizeof(line), fmt, args);
	va_end(args);
	return (pager_output(line));
}

#endif

#define	STATUS_FORMAT	"        %s %s\n"

static int
print_state(int indent, const char *name, vdev_state_t state)
{
	int i;
	char buf[512];

	buf[0] = 0;
	for (i = 0; i < indent; i++)
		strcat(buf, "  ");
	strcat(buf, name);
	return (pager_printf(STATUS_FORMAT, buf, state_name(state)));
}

static int
vdev_status(vdev_t *vdev, int indent)
{
	vdev_t *kid;
	int ret;

	if (vdev->v_islog) {
		(void) pager_output("        logs\n");
		indent++;
	}

	ret = print_state(indent, vdev->v_name, vdev->v_state);
	if (ret != 0)
		return (ret);

	STAILQ_FOREACH(kid, &vdev->v_children, v_childlink) {
		ret = vdev_status(kid, indent + 1);
		if (ret != 0)
			return (ret);
	}
	return (ret);
}

static int
spa_status(spa_t *spa)
{
	static char bootfs[ZFS_MAXNAMELEN];
	uint64_t rootid;
	vdev_list_t *vlist;
	vdev_t *vdev;
	int good_kids, bad_kids, degraded_kids, ret;
	vdev_state_t state;

	ret = pager_printf("  pool: %s\n", spa->spa_name);
	if (ret != 0)
		return (ret);

	if (zfs_get_root(spa, &rootid) == 0 &&
	    zfs_rlookup(spa, rootid, bootfs) == 0) {
		if (bootfs[0] == '\0')
			ret = pager_printf("bootfs: %s\n", spa->spa_name);
		else
			ret = pager_printf("bootfs: %s/%s\n", spa->spa_name,
			    bootfs);
		if (ret != 0)
			return (ret);
	}
	ret = pager_printf("config:\n\n");
	if (ret != 0)
		return (ret);
	ret = pager_printf(STATUS_FORMAT, "NAME", "STATE");
	if (ret != 0)
		return (ret);

	good_kids = 0;
	degraded_kids = 0;
	bad_kids = 0;
	vlist = &spa->spa_root_vdev->v_children;
	STAILQ_FOREACH(vdev, vlist, v_childlink) {
		if (vdev->v_state == VDEV_STATE_HEALTHY)
			good_kids++;
		else if (vdev->v_state == VDEV_STATE_DEGRADED)
			degraded_kids++;
		else
			bad_kids++;
	}

	state = VDEV_STATE_CLOSED;
	if (good_kids > 0 && (degraded_kids + bad_kids) == 0)
		state = VDEV_STATE_HEALTHY;
	else if ((good_kids + degraded_kids) > 0)
		state = VDEV_STATE_DEGRADED;

	ret = print_state(0, spa->spa_name, state);
	if (ret != 0)
		return (ret);

	STAILQ_FOREACH(vdev, vlist, v_childlink) {
		ret = vdev_status(vdev, 1);
		if (ret != 0)
			return (ret);
	}
	return (ret);
}

static int
spa_all_status(void)
{
	spa_t *spa;
	int first = 1, ret = 0;

	STAILQ_FOREACH(spa, &zfs_pools, spa_link) {
		if (!first) {
			ret = pager_printf("\n");
			if (ret != 0)
				return (ret);
		}
		first = 0;
		ret = spa_status(spa);
		if (ret != 0)
			return (ret);
	}
	return (ret);
}

static uint64_t
vdev_label_offset(uint64_t psize, int l, uint64_t offset)
{
	uint64_t label_offset;

	if (l < VDEV_LABELS / 2)
		label_offset = 0;
	else
		label_offset = psize - VDEV_LABELS * sizeof (vdev_label_t);

	return (offset + l * sizeof (vdev_label_t) + label_offset);
}

static int
vdev_uberblock_compare(const uberblock_t *ub1, const uberblock_t *ub2)
{
	unsigned int seq1 = 0;
	unsigned int seq2 = 0;
	int cmp = AVL_CMP(ub1->ub_txg, ub2->ub_txg);

	if (cmp != 0)
		return (cmp);

	cmp = AVL_CMP(ub1->ub_timestamp, ub2->ub_timestamp);
	if (cmp != 0)
		return (cmp);

	if (MMP_VALID(ub1) && MMP_SEQ_VALID(ub1))
		seq1 = MMP_SEQ(ub1);

	if (MMP_VALID(ub2) && MMP_SEQ_VALID(ub2))
		seq2 = MMP_SEQ(ub2);

	return (AVL_CMP(seq1, seq2));
}

static int
uberblock_verify(uberblock_t *ub)
{
	if (ub->ub_magic == BSWAP_64((uint64_t)UBERBLOCK_MAGIC)) {
		byteswap_uint64_array(ub, sizeof (uberblock_t));
	}

	if (ub->ub_magic != UBERBLOCK_MAGIC ||
	    !SPA_VERSION_IS_SUPPORTED(ub->ub_version))
		return (EINVAL);

	return (0);
}

static int
vdev_label_read(vdev_t *vd, int l, void *buf, uint64_t offset,
    size_t size)
{
	blkptr_t bp;
	off_t off;

	off = vdev_label_offset(vd->v_psize, l, offset);

	BP_ZERO(&bp);
	BP_SET_LSIZE(&bp, size);
	BP_SET_PSIZE(&bp, size);
	BP_SET_CHECKSUM(&bp, ZIO_CHECKSUM_LABEL);
	BP_SET_COMPRESS(&bp, ZIO_COMPRESS_OFF);
	DVA_SET_OFFSET(BP_IDENTITY(&bp), off);
	ZIO_SET_CHECKSUM(&bp.blk_cksum, off, 0, 0, 0);

	return (vdev_read_phys(vd, &bp, buf, off, size));
}

/*
 * We do need to be sure we write to correct location.
 * Our vdev label does consist of 4 fields:
 * pad1 (8k), reserved.
 * bootenv (8k), checksummed, previously reserved, may contian garbage.
 * vdev_phys (112k), checksummed
 * uberblock ring (128k), checksummed.
 *
 * Since bootenv area may contain garbage, we can not reliably read it, as
 * we can get checksum errors.
 * Next best thing is vdev_phys - it is just after bootenv. It still may
 * be corrupted, but in such case we will miss this one write.
 */
static int
vdev_label_write_validate(vdev_t *vd, int l, uint64_t offset)
{
	uint64_t off, o_phys;
	void *buf;
	size_t size = VDEV_PHYS_SIZE;
	int rc;

	o_phys = offsetof(vdev_label_t, vl_vdev_phys);
	off = vdev_label_offset(vd->v_psize, l, o_phys);

	/* off should be 8K from bootenv */
	if (vdev_label_offset(vd->v_psize, l, offset) + VDEV_PAD_SIZE != off)
		return (EINVAL);

	buf = malloc(size);
	if (buf == NULL)
		return (ENOMEM);

	/* Read vdev_phys */
	rc = vdev_label_read(vd, l, buf, o_phys, size);
	free(buf);
	return (rc);
}

static int
vdev_label_write(vdev_t *vd, int l, vdev_boot_envblock_t *be, uint64_t offset)
{
	zio_checksum_info_t *ci;
	zio_cksum_t cksum;
	off_t off;
	size_t size = VDEV_PAD_SIZE;
	int rc;

	if (vd->v_phys_write == NULL)
		return (ENOTSUP);

	off = vdev_label_offset(vd->v_psize, l, offset);

	rc = vdev_label_write_validate(vd, l, offset);
	if (rc != 0) {
		return (rc);
	}

	ci = &zio_checksum_table[ZIO_CHECKSUM_LABEL];
	be->vbe_zbt.zec_magic = ZEC_MAGIC;
	zio_checksum_label_verifier(&be->vbe_zbt.zec_cksum, off);
	ci->ci_func[0](be, size, NULL, &cksum);
	be->vbe_zbt.zec_cksum = cksum;

	return (vdev_write_phys(vd, be, off, size));
}

static int
vdev_write_bootenv_impl(vdev_t *vdev, vdev_boot_envblock_t *be)
{
	vdev_t *kid;
	int rv = 0, err;

	STAILQ_FOREACH(kid, &vdev->v_children, v_childlink) {
		if (kid->v_state != VDEV_STATE_HEALTHY)
			continue;
		err = vdev_write_bootenv_impl(kid, be);
		if (err != 0)
			rv = err;
	}

	/*
	 * Non-leaf vdevs do not have v_phys_write.
	 */
	if (vdev->v_phys_write == NULL)
		return (rv);

	for (int l = 0; l < VDEV_LABELS; l++) {
		err = vdev_label_write(vdev, l, be,
		    offsetof(vdev_label_t, vl_be));
		if (err != 0) {
			printf("failed to write bootenv to %s label %d: %d\n",
			    vdev->v_name ? vdev->v_name : "unknown", l, err);
			rv = err;
		}
	}
	return (rv);
}

int
vdev_write_bootenv(vdev_t *vdev, nvlist_t *nvl)
{
	vdev_boot_envblock_t *be;
	nvlist_t nv, *nvp;
	uint64_t version;
	int rv;

	if (nvl->nv_size > sizeof(be->vbe_bootenv))
		return (E2BIG);

	version = VB_RAW;
	nvp = vdev_read_bootenv(vdev);
	if (nvp != NULL) {
		nvlist_find(nvp, BOOTENV_VERSION, DATA_TYPE_UINT64, NULL,
		    &version, NULL);
		nvlist_destroy(nvp);
	}

	be = calloc(1, sizeof(*be));
	if (be == NULL)
		return (ENOMEM);

	be->vbe_version = version;
	switch (version) {
	case VB_RAW:
		/*
		 * If there is no envmap, we will just wipe bootenv.
		 */
		nvlist_find(nvl, GRUB_ENVMAP, DATA_TYPE_STRING, NULL,
		    be->vbe_bootenv, NULL);
		rv = 0;
		break;

	case VB_NVLIST:
		nv.nv_header = nvl->nv_header;
		nv.nv_asize = nvl->nv_asize;
		nv.nv_size = nvl->nv_size;

		bcopy(&nv.nv_header, be->vbe_bootenv, sizeof(nv.nv_header));
		nv.nv_data = be->vbe_bootenv + sizeof(nvs_header_t);
		bcopy(nvl->nv_data, nv.nv_data, nv.nv_size);
		rv = nvlist_export(&nv);
		break;

	default:
		rv = EINVAL;
		break;
	}

	if (rv == 0) {
		be->vbe_version = htobe64(be->vbe_version);
		rv = vdev_write_bootenv_impl(vdev, be);
	}
	free(be);
	return (rv);
}

/*
 * Read the bootenv area from pool label, return the nvlist from it.
 * We return from first successful read.
 */
nvlist_t *
vdev_read_bootenv(vdev_t *vdev)
{
	vdev_t *kid;
	nvlist_t *benv;
	vdev_boot_envblock_t *be;
	char *command;
	bool ok;
	int rv;

	STAILQ_FOREACH(kid, &vdev->v_children, v_childlink) {
		if (kid->v_state != VDEV_STATE_HEALTHY)
			continue;

		benv = vdev_read_bootenv(kid);
		if (benv != NULL)
			return (benv);
	}

	be = malloc(sizeof (*be));
	if (be == NULL)
		return (NULL);

	rv = 0;
	for (int l = 0; l < VDEV_LABELS; l++) {
		rv = vdev_label_read(vdev, l, be,
		    offsetof(vdev_label_t, vl_be),
		    sizeof (*be));
		if (rv == 0)
			break;
	}
	if (rv != 0) {
		free(be);
		return (NULL);
	}

	be->vbe_version = be64toh(be->vbe_version);
	switch (be->vbe_version) {
	case VB_RAW:
		/*
		 * we have textual data in vbe_bootenv, create nvlist
		 * with key "envmap".
		 */
		benv = nvlist_create(NV_UNIQUE_NAME);
		if (benv != NULL) {
			if (*be->vbe_bootenv == '\0') {
				nvlist_add_uint64(benv, BOOTENV_VERSION,
				    VB_NVLIST);
				break;
			}
			nvlist_add_uint64(benv, BOOTENV_VERSION, VB_RAW);
			be->vbe_bootenv[sizeof (be->vbe_bootenv) - 1] = '\0';
			nvlist_add_string(benv, GRUB_ENVMAP, be->vbe_bootenv);
		}
		break;

	case VB_NVLIST:
		benv = nvlist_import(be->vbe_bootenv, sizeof(be->vbe_bootenv));
		break;

	default:
		command = (char *)be;
		ok = false;

		/* Check for legacy zfsbootcfg command string */
		for (int i = 0; command[i] != '\0'; i++) {
			if (iscntrl(command[i])) {
				ok = false;
				break;
			} else {
				ok = true;
			}
		}
		benv = nvlist_create(NV_UNIQUE_NAME);
		if (benv != NULL) {
			if (ok)
				nvlist_add_string(benv, FREEBSD_BOOTONCE,
				    command);
			else
				nvlist_add_uint64(benv, BOOTENV_VERSION,
				    VB_NVLIST);
		}
		break;
	}
	free(be);
	return (benv);
}

static uint64_t
vdev_get_label_asize(nvlist_t *nvl)
{
	nvlist_t *vdevs;
	uint64_t asize;
	const char *type;
	int len;

	asize = 0;
	/* Get vdev tree */
	if (nvlist_find(nvl, ZPOOL_CONFIG_VDEV_TREE, DATA_TYPE_NVLIST,
	    NULL, &vdevs, NULL) != 0)
		return (asize);

	/*
	 * Get vdev type. We will calculate asize for raidz, mirror and disk.
	 * For raidz, the asize is raw size of all children.
	 */
	if (nvlist_find(vdevs, ZPOOL_CONFIG_TYPE, DATA_TYPE_STRING,
	    NULL, &type, &len) != 0)
		goto done;

	if (memcmp(type, VDEV_TYPE_MIRROR, len) != 0 &&
	    memcmp(type, VDEV_TYPE_DISK, len) != 0 &&
	    memcmp(type, VDEV_TYPE_RAIDZ, len) != 0)
		goto done;

	if (nvlist_find(vdevs, ZPOOL_CONFIG_ASIZE, DATA_TYPE_UINT64,
	    NULL, &asize, NULL) != 0)
		goto done;

	if (memcmp(type, VDEV_TYPE_RAIDZ, len) == 0) {
		nvlist_t **kids;
		int nkids;

		if (nvlist_find(vdevs, ZPOOL_CONFIG_CHILDREN,
		    DATA_TYPE_NVLIST_ARRAY, &nkids, &kids, NULL) != 0) {
			asize = 0;
			goto done;
		}

		asize /= nkids;
		for (int i = 0; i < nkids; i++)
			nvlist_destroy(kids[i]);
		free(kids);
	}

	asize += VDEV_LABEL_START_SIZE + VDEV_LABEL_END_SIZE;
done:
	nvlist_destroy(vdevs);
	return (asize);
}

static nvlist_t *
vdev_label_read_config(vdev_t *vd, uint64_t txg)
{
	vdev_phys_t *label;
	uint64_t best_txg = 0;
	uint64_t label_txg = 0;
	uint64_t asize;
	nvlist_t *nvl = NULL, *tmp;
	int error;

	label = malloc(sizeof (vdev_phys_t));
	if (label == NULL)
		return (NULL);

	for (int l = 0; l < VDEV_LABELS; l++) {
		if (vdev_label_read(vd, l, label,
		    offsetof(vdev_label_t, vl_vdev_phys),
		    sizeof (vdev_phys_t)))
			continue;

		tmp = nvlist_import(label->vp_nvlist,
		    sizeof(label->vp_nvlist));
		if (tmp == NULL)
			continue;

		error = nvlist_find(tmp, ZPOOL_CONFIG_POOL_TXG,
		    DATA_TYPE_UINT64, NULL, &label_txg, NULL);
		if (error != 0 || label_txg == 0) {
			nvlist_destroy(nvl);
			nvl = tmp;
			goto done;
		}

		if (label_txg <= txg && label_txg > best_txg) {
			best_txg = label_txg;
			nvlist_destroy(nvl);
			nvl = tmp;
			tmp = NULL;

			/*
			 * Use asize from pool config. We need this
			 * because we can get bad value from BIOS.
			 */
			asize = vdev_get_label_asize(nvl);
			if (asize != 0) {
				vd->v_psize = asize;
			}
		}
		nvlist_destroy(tmp);
	}

	if (best_txg == 0) {
		nvlist_destroy(nvl);
		nvl = NULL;
	}
done:
	free(label);
	return (nvl);
}

static void
vdev_uberblock_load(vdev_t *vd, uberblock_t *ub)
{
	uberblock_t *buf;

	buf = malloc(VDEV_UBERBLOCK_SIZE(vd));
	if (buf == NULL)
		return;

	for (int l = 0; l < VDEV_LABELS; l++) {
		for (int n = 0; n < VDEV_UBERBLOCK_COUNT(vd); n++) {
			if (vdev_label_read(vd, l, buf,
			    VDEV_UBERBLOCK_OFFSET(vd, n),
			    VDEV_UBERBLOCK_SIZE(vd)))
				continue;
			if (uberblock_verify(buf) != 0)
				continue;

			if (vdev_uberblock_compare(buf, ub) > 0)
				*ub = *buf;
		}
	}
	free(buf);
}

static int
vdev_probe(vdev_phys_read_t *_read, vdev_phys_write_t *_write, void *priv,
    spa_t **spap)
{
	vdev_t vtmp;
	spa_t *spa;
	vdev_t *vdev;
	nvlist_t *nvl;
	uint64_t val;
	uint64_t guid, vdev_children;
	uint64_t pool_txg, pool_guid;
	const char *pool_name;
	int rc, namelen;

	/*
	 * Load the vdev label and figure out which
	 * uberblock is most current.
	 */
	memset(&vtmp, 0, sizeof(vtmp));
	vtmp.v_phys_read = _read;
	vtmp.v_phys_write = _write;
	vtmp.v_priv = priv;
	vtmp.v_psize = P2ALIGN(ldi_get_size(priv),
	    (uint64_t)sizeof (vdev_label_t));

	/* Test for minimum device size. */
	if (vtmp.v_psize < SPA_MINDEVSIZE)
		return (EIO);

	nvl = vdev_label_read_config(&vtmp, UINT64_MAX);
	if (nvl == NULL)
		return (EIO);

	if (nvlist_find(nvl, ZPOOL_CONFIG_VERSION, DATA_TYPE_UINT64,
	    NULL, &val, NULL) != 0) {
		nvlist_destroy(nvl);
		return (EIO);
	}

	if (!SPA_VERSION_IS_SUPPORTED(val)) {
		printf("ZFS: unsupported ZFS version %u (should be %u)\n",
		    (unsigned)val, (unsigned)SPA_VERSION);
		nvlist_destroy(nvl);
		return (EIO);
	}

	/* Check ZFS features for read */
	rc = nvlist_check_features_for_read(nvl);
	if (rc != 0) {
		nvlist_destroy(nvl);
		return (EIO);
	}

	if (nvlist_find(nvl, ZPOOL_CONFIG_POOL_STATE, DATA_TYPE_UINT64,
	    NULL, &val, NULL) != 0) {
		nvlist_destroy(nvl);
		return (EIO);
	}

	if (val == POOL_STATE_DESTROYED) {
		/* We don't boot only from destroyed pools. */
		nvlist_destroy(nvl);
		return (EIO);
	}

	if (nvlist_find(nvl, ZPOOL_CONFIG_POOL_TXG, DATA_TYPE_UINT64,
	    NULL, &pool_txg, NULL) != 0 ||
	    nvlist_find(nvl, ZPOOL_CONFIG_POOL_GUID, DATA_TYPE_UINT64,
	    NULL, &pool_guid, NULL) != 0 ||
	    nvlist_find(nvl, ZPOOL_CONFIG_POOL_NAME, DATA_TYPE_STRING,
	    NULL, &pool_name, &namelen) != 0) {
		/*
		 * Cache and spare devices end up here - just ignore
		 * them.
		 */
		nvlist_destroy(nvl);
		return (EIO);
	}

	/*
	 * Create the pool if this is the first time we've seen it.
	 */
	spa = spa_find_by_guid(pool_guid);
	if (spa == NULL) {
		char *name;

		nvlist_find(nvl, ZPOOL_CONFIG_VDEV_CHILDREN,
		    DATA_TYPE_UINT64, NULL, &vdev_children, NULL);
		name = malloc(namelen + 1);
		if (name == NULL) {
			nvlist_destroy(nvl);
			return (ENOMEM);
		}
		bcopy(pool_name, name, namelen);
		name[namelen] = '\0';
		spa = spa_create(pool_guid, name);
		free(name);
		if (spa == NULL) {
			nvlist_destroy(nvl);
			return (ENOMEM);
		}
		spa->spa_root_vdev->v_nchildren = vdev_children;
	}
	if (pool_txg > spa->spa_txg)
		spa->spa_txg = pool_txg;

	/*
	 * Get the vdev tree and create our in-core copy of it.
	 * If we already have a vdev with this guid, this must
	 * be some kind of alias (overlapping slices, dangerously dedicated
	 * disks etc).
	 */
	if (nvlist_find(nvl, ZPOOL_CONFIG_GUID, DATA_TYPE_UINT64,
	    NULL, &guid, NULL) != 0) {
		nvlist_destroy(nvl);
		return (EIO);
	}
	vdev = vdev_find(guid);
	/* Has this vdev already been inited? */
	if (vdev && vdev->v_phys_read) {
		nvlist_destroy(nvl);
		return (EIO);
	}

	rc = vdev_init_from_label(spa, nvl);
	nvlist_destroy(nvl);
	if (rc != 0)
		return (rc);

	/*
	 * We should already have created an incomplete vdev for this
	 * vdev. Find it and initialise it with our read proc.
	 */
	vdev = vdev_find(guid);
	if (vdev != NULL) {
		vdev->v_phys_read = _read;
		vdev->v_phys_write = _write;
		vdev->v_priv = priv;
		vdev->v_psize = vtmp.v_psize;
		/*
		 * If no other state is set, mark vdev healthy.
		 */
		if (vdev->v_state == VDEV_STATE_UNKNOWN)
			vdev->v_state = VDEV_STATE_HEALTHY;
	} else {
		printf("ZFS: inconsistent nvlist contents\n");
		return (EIO);
	}

	if (vdev->v_islog)
		spa->spa_with_log = vdev->v_islog;

	/*
	 * Re-evaluate top-level vdev state.
	 */
	vdev_set_state(vdev->v_top);

	/*
	 * Ok, we are happy with the pool so far. Lets find
	 * the best uberblock and then we can actually access
	 * the contents of the pool.
	 */
	vdev_uberblock_load(vdev, spa->spa_uberblock);

	if (spap != NULL)
		*spap = spa;
	return (0);
}

static int
ilog2(int n)
{
	int v;

	for (v = 0; v < 32; v++)
		if (n == (1 << v))
			return (v);
	return (-1);
}

static int
zio_read_gang(const spa_t *spa, const blkptr_t *bp, void *buf)
{
	blkptr_t gbh_bp;
	zio_gbh_phys_t zio_gb;
	char *pbuf;
	int i;

	/* Artificial BP for gang block header. */
	gbh_bp = *bp;
	BP_SET_PSIZE(&gbh_bp, SPA_GANGBLOCKSIZE);
	BP_SET_LSIZE(&gbh_bp, SPA_GANGBLOCKSIZE);
	BP_SET_CHECKSUM(&gbh_bp, ZIO_CHECKSUM_GANG_HEADER);
	BP_SET_COMPRESS(&gbh_bp, ZIO_COMPRESS_OFF);
	for (i = 0; i < SPA_DVAS_PER_BP; i++)
		DVA_SET_GANG(&gbh_bp.blk_dva[i], 0);

	/* Read gang header block using the artificial BP. */
	if (zio_read(spa, &gbh_bp, &zio_gb))
		return (EIO);

	pbuf = buf;
	for (i = 0; i < SPA_GBH_NBLKPTRS; i++) {
		blkptr_t *gbp = &zio_gb.zg_blkptr[i];

		if (BP_IS_HOLE(gbp))
			continue;
		if (zio_read(spa, gbp, pbuf))
			return (EIO);
		pbuf += BP_GET_PSIZE(gbp);
	}

	if (zio_checksum_verify(spa, bp, buf))
		return (EIO);
	return (0);
}

static int
zio_read(const spa_t *spa, const blkptr_t *bp, void *buf)
{
	int cpfunc = BP_GET_COMPRESS(bp);
	uint64_t align, size;
	void *pbuf;
	int i, error;

	/*
	 * Process data embedded in block pointer
	 */
	if (BP_IS_EMBEDDED(bp)) {
		ASSERT(BPE_GET_ETYPE(bp) == BP_EMBEDDED_TYPE_DATA);

		size = BPE_GET_PSIZE(bp);
		ASSERT(size <= BPE_PAYLOAD_SIZE);

		if (cpfunc != ZIO_COMPRESS_OFF)
			pbuf = malloc(size);
		else
			pbuf = buf;

		if (pbuf == NULL)
			return (ENOMEM);

		decode_embedded_bp_compressed(bp, pbuf);
		error = 0;

		if (cpfunc != ZIO_COMPRESS_OFF) {
			error = zio_decompress_data(cpfunc, pbuf,
			    size, buf, BP_GET_LSIZE(bp));
			free(pbuf);
		}
		if (error != 0)
			printf("ZFS: i/o error - unable to decompress "
			    "block pointer data, error %d\n", error);
		return (error);
	}

	error = EIO;

	for (i = 0; i < SPA_DVAS_PER_BP; i++) {
		const dva_t *dva = &bp->blk_dva[i];
		vdev_t *vdev;
		vdev_list_t *vlist;
		uint64_t vdevid;
		off_t offset;

		if (!dva->dva_word[0] && !dva->dva_word[1])
			continue;

		vdevid = DVA_GET_VDEV(dva);
		offset = DVA_GET_OFFSET(dva);
		vlist = &spa->spa_root_vdev->v_children;
		STAILQ_FOREACH(vdev, vlist, v_childlink) {
			if (vdev->v_id == vdevid)
				break;
		}
		if (!vdev || !vdev->v_read)
			continue;

		size = BP_GET_PSIZE(bp);
		if (vdev->v_read == vdev_raidz_read) {
			align = 1ULL << vdev->v_ashift;
			if (P2PHASE(size, align) != 0)
				size = P2ROUNDUP(size, align);
		}
		if (size != BP_GET_PSIZE(bp) || cpfunc != ZIO_COMPRESS_OFF)
			pbuf = malloc(size);
		else
			pbuf = buf;

		if (pbuf == NULL) {
			error = ENOMEM;
			break;
		}

		if (DVA_GET_GANG(dva))
			error = zio_read_gang(spa, bp, pbuf);
		else
			error = vdev->v_read(vdev, bp, pbuf, offset, size);
		if (error == 0) {
			if (cpfunc != ZIO_COMPRESS_OFF)
				error = zio_decompress_data(cpfunc, pbuf,
				    BP_GET_PSIZE(bp), buf, BP_GET_LSIZE(bp));
			else if (size != BP_GET_PSIZE(bp))
				bcopy(pbuf, buf, BP_GET_PSIZE(bp));
		} else {
			printf("zio_read error: %d\n", error);
		}
		if (buf != pbuf)
			free(pbuf);
		if (error == 0)
			break;
	}
	if (error != 0)
		printf("ZFS: i/o error - all block copies unavailable\n");

	return (error);
}

static int
dnode_read(const spa_t *spa, const dnode_phys_t *dnode, off_t offset,
    void *buf, size_t buflen)
{
	int ibshift = dnode->dn_indblkshift - SPA_BLKPTRSHIFT;
	int bsize = dnode->dn_datablkszsec << SPA_MINBLOCKSHIFT;
	int nlevels = dnode->dn_nlevels;
	int i, rc;

	if (bsize > SPA_MAXBLOCKSIZE) {
		printf("ZFS: I/O error - blocks larger than %llu are not "
		    "supported\n", SPA_MAXBLOCKSIZE);
		return (EIO);
	}

	/*
	 * Handle odd block sizes, mirrors dmu_read_impl().  Data can't exist
	 * past the first block, so we'll clip the read to the portion of the
	 * buffer within bsize and zero out the remainder.
	 */
	if (dnode->dn_maxblkid == 0) {
		size_t newbuflen;

		newbuflen = offset > bsize ? 0 : MIN(buflen, bsize - offset);
		bzero((char *)buf + newbuflen, buflen - newbuflen);
		buflen = newbuflen;
	}

	/*
	 * Note: bsize may not be a power of two here so we need to do an
	 * actual divide rather than a bitshift.
	 */
	while (buflen > 0) {
		uint64_t bn = offset / bsize;
		int boff = offset % bsize;
		int ibn;
		const blkptr_t *indbp;
		blkptr_t bp;

		if (bn > dnode->dn_maxblkid)
			return (EIO);

		if (dnode == dnode_cache_obj && bn == dnode_cache_bn)
			goto cached;

		indbp = dnode->dn_blkptr;
		for (i = 0; i < nlevels; i++) {
			/*
			 * Copy the bp from the indirect array so that
			 * we can re-use the scratch buffer for multi-level
			 * objects.
			 */
			ibn = bn >> ((nlevels - i - 1) * ibshift);
			ibn &= ((1 << ibshift) - 1);
			bp = indbp[ibn];
			if (BP_IS_HOLE(&bp)) {
				memset(dnode_cache_buf, 0, bsize);
				break;
			}
			rc = zio_read(spa, &bp, dnode_cache_buf);
			if (rc)
				return (rc);
			indbp = (const blkptr_t *) dnode_cache_buf;
		}
		dnode_cache_obj = dnode;
		dnode_cache_bn = bn;
	cached:

		/*
		 * The buffer contains our data block. Copy what we
		 * need from it and loop.
		 */
		i = bsize - boff;
		if (i > buflen) i = buflen;
		memcpy(buf, &dnode_cache_buf[boff], i);
		buf = ((char *)buf) + i;
		offset += i;
		buflen -= i;
	}

	return (0);
}

/*
 * Lookup a value in a microzap directory.
 */
static int
mzap_lookup(const mzap_phys_t *mz, size_t size, const char *name,
    uint64_t *value)
{
	const mzap_ent_phys_t *mze;
	int chunks, i;

	/*
	 * Microzap objects use exactly one block. Read the whole
	 * thing.
	 */
	chunks = size / MZAP_ENT_LEN - 1;
	for (i = 0; i < chunks; i++) {
		mze = &mz->mz_chunk[i];
		if (strcmp(mze->mze_name, name) == 0) {
			*value = mze->mze_value;
			return (0);
		}
	}

	return (ENOENT);
}

/*
 * Compare a name with a zap leaf entry. Return non-zero if the name
 * matches.
 */
static int
fzap_name_equal(const zap_leaf_t *zl, const zap_leaf_chunk_t *zc,
    const char *name)
{
	size_t namelen;
	const zap_leaf_chunk_t *nc;
	const char *p;

	namelen = zc->l_entry.le_name_numints;

	nc = &ZAP_LEAF_CHUNK(zl, zc->l_entry.le_name_chunk);
	p = name;
	while (namelen > 0) {
		size_t len;

		len = namelen;
		if (len > ZAP_LEAF_ARRAY_BYTES)
			len = ZAP_LEAF_ARRAY_BYTES;
		if (memcmp(p, nc->l_array.la_array, len))
			return (0);
		p += len;
		namelen -= len;
		nc = &ZAP_LEAF_CHUNK(zl, nc->l_array.la_next);
	}

	return (1);
}

/*
 * Extract a uint64_t value from a zap leaf entry.
 */
static uint64_t
fzap_leaf_value(const zap_leaf_t *zl, const zap_leaf_chunk_t *zc)
{
	const zap_leaf_chunk_t *vc;
	int i;
	uint64_t value;
	const uint8_t *p;

	vc = &ZAP_LEAF_CHUNK(zl, zc->l_entry.le_value_chunk);
	for (i = 0, value = 0, p = vc->l_array.la_array; i < 8; i++) {
		value = (value << 8) | p[i];
	}

	return (value);
}

static void
stv(int len, void *addr, uint64_t value)
{
	switch (len) {
	case 1:
		*(uint8_t *)addr = value;
		return;
	case 2:
		*(uint16_t *)addr = value;
		return;
	case 4:
		*(uint32_t *)addr = value;
		return;
	case 8:
		*(uint64_t *)addr = value;
		return;
	}
}

/*
 * Extract a array from a zap leaf entry.
 */
static void
fzap_leaf_array(const zap_leaf_t *zl, const zap_leaf_chunk_t *zc,
    uint64_t integer_size, uint64_t num_integers, void *buf)
{
	uint64_t array_int_len = zc->l_entry.le_value_intlen;
	uint64_t value = 0;
	uint64_t *u64 = buf;
	char *p = buf;
	int len = MIN(zc->l_entry.le_value_numints, num_integers);
	int chunk = zc->l_entry.le_value_chunk;
	int byten = 0;

	if (integer_size == 8 && len == 1) {
		*u64 = fzap_leaf_value(zl, zc);
		return;
	}

	while (len > 0) {
		struct zap_leaf_array *la = &ZAP_LEAF_CHUNK(zl, chunk).l_array;
		int i;

		ASSERT3U(chunk, <, ZAP_LEAF_NUMCHUNKS(zl));
		for (i = 0; i < ZAP_LEAF_ARRAY_BYTES && len > 0; i++) {
			value = (value << 8) | la->la_array[i];
			byten++;
			if (byten == array_int_len) {
				stv(integer_size, p, value);
				byten = 0;
				len--;
				if (len == 0)
					return;
				p += integer_size;
			}
		}
		chunk = la->la_next;
	}
}

static int
fzap_check_size(uint64_t integer_size, uint64_t num_integers)
{

	switch (integer_size) {
	case 1:
	case 2:
	case 4:
	case 8:
		break;
	default:
		return (EINVAL);
	}

	if (integer_size * num_integers > ZAP_MAXVALUELEN)
		return (E2BIG);

	return (0);
}

static void
zap_leaf_free(zap_leaf_t *leaf)
{
	free(leaf->l_phys);
	free(leaf);
}

static int
zap_get_leaf_byblk(fat_zap_t *zap, uint64_t blk, zap_leaf_t **lp)
{
	int bs = FZAP_BLOCK_SHIFT(zap);
	int err;

	*lp = malloc(sizeof(**lp));
	if (*lp == NULL)
		return (ENOMEM);

	(*lp)->l_bs = bs;
	(*lp)->l_phys = malloc(1 << bs);

	if ((*lp)->l_phys == NULL) {
		free(*lp);
		return (ENOMEM);
	}
	err = dnode_read(zap->zap_spa, zap->zap_dnode, blk << bs, (*lp)->l_phys,
	    1 << bs);
	if (err != 0) {
		zap_leaf_free(*lp);
	}
	return (err);
}

static int
zap_table_load(fat_zap_t *zap, zap_table_phys_t *tbl, uint64_t idx,
    uint64_t *valp)
{
	int bs = FZAP_BLOCK_SHIFT(zap);
	uint64_t blk = idx >> (bs - 3);
	uint64_t off = idx & ((1 << (bs - 3)) - 1);
	uint64_t *buf;
	int rc;

	buf = malloc(1 << zap->zap_block_shift);
	if (buf == NULL)
		return (ENOMEM);
	rc = dnode_read(zap->zap_spa, zap->zap_dnode, (tbl->zt_blk + blk) << bs,
	    buf, 1 << zap->zap_block_shift);
	if (rc == 0)
		*valp = buf[off];
	free(buf);
	return (rc);
}

static int
zap_idx_to_blk(fat_zap_t *zap, uint64_t idx, uint64_t *valp)
{
	if (zap->zap_phys->zap_ptrtbl.zt_numblks == 0) {
		*valp = ZAP_EMBEDDED_PTRTBL_ENT(zap, idx);
		return (0);
	} else {
		return (zap_table_load(zap, &zap->zap_phys->zap_ptrtbl,
		    idx, valp));
	}
}

#define	ZAP_HASH_IDX(hash, n)	(((n) == 0) ? 0 : ((hash) >> (64 - (n))))
static int
zap_deref_leaf(fat_zap_t *zap, uint64_t h, zap_leaf_t **lp)
{
	uint64_t idx, blk;
	int err;

	idx = ZAP_HASH_IDX(h, zap->zap_phys->zap_ptrtbl.zt_shift);
	err = zap_idx_to_blk(zap, idx, &blk);
	if (err != 0)
		return (err);
	return (zap_get_leaf_byblk(zap, blk, lp));
}

#define	CHAIN_END	0xffff	/* end of the chunk chain */
#define	LEAF_HASH(l, h) \
	((ZAP_LEAF_HASH_NUMENTRIES(l)-1) & \
	((h) >> \
	(64 - ZAP_LEAF_HASH_SHIFT(l) - (l)->l_phys->l_hdr.lh_prefix_len)))
#define	LEAF_HASH_ENTPTR(l, h)	(&(l)->l_phys->l_hash[LEAF_HASH(l, h)])

static int
zap_leaf_lookup(zap_leaf_t *zl, uint64_t hash, const char *name,
    uint64_t integer_size, uint64_t num_integers, void *value)
{
	int rc;
	uint16_t *chunkp;
	struct zap_leaf_entry *le;

	/*
	 * Make sure this chunk matches our hash.
	 */
	if (zl->l_phys->l_hdr.lh_prefix_len > 0 &&
	    zl->l_phys->l_hdr.lh_prefix !=
	    hash >> (64 - zl->l_phys->l_hdr.lh_prefix_len))
		return (EIO);

	rc = ENOENT;
	for (chunkp = LEAF_HASH_ENTPTR(zl, hash);
	    *chunkp != CHAIN_END; chunkp = &le->le_next) {
		zap_leaf_chunk_t *zc;
		uint16_t chunk = *chunkp;

		le = ZAP_LEAF_ENTRY(zl, chunk);
		if (le->le_hash != hash)
			continue;
		zc = &ZAP_LEAF_CHUNK(zl, chunk);
		if (fzap_name_equal(zl, zc, name)) {
			if (zc->l_entry.le_value_intlen > integer_size) {
				rc = EINVAL;
			} else {
				fzap_leaf_array(zl, zc, integer_size,
				    num_integers, value);
				rc = 0;
			}
			break;
		}
	}
	return (rc);
}

/*
 * Lookup a value in a fatzap directory.
 */
static int
fzap_lookup(const spa_t *spa, const dnode_phys_t *dnode, zap_phys_t *zh,
    const char *name, uint64_t integer_size, uint64_t num_integers,
    void *value)
{
	int bsize = dnode->dn_datablkszsec << SPA_MINBLOCKSHIFT;
	fat_zap_t z;
	zap_leaf_t *zl;
	uint64_t hash;
	int rc;

	if (zh->zap_magic != ZAP_MAGIC)
		return (EIO);

	if ((rc = fzap_check_size(integer_size, num_integers)) != 0) {
		return (rc);
	}

	z.zap_block_shift = ilog2(bsize);
	z.zap_phys = zh;
	z.zap_spa = spa;
	z.zap_dnode = dnode;

	hash = zap_hash(zh->zap_salt, name);
	rc = zap_deref_leaf(&z, hash, &zl);
	if (rc != 0)
		return (rc);

	rc = zap_leaf_lookup(zl, hash, name, integer_size, num_integers, value);

	zap_leaf_free(zl);
	return (rc);
}

/*
 * Lookup a name in a zap object and return its value as a uint64_t.
 */
static int
zap_lookup(const spa_t *spa, const dnode_phys_t *dnode, const char *name,
    uint64_t integer_size, uint64_t num_integers, void *value)
{
	int rc;
	zap_phys_t *zap;
	size_t size = dnode->dn_datablkszsec << SPA_MINBLOCKSHIFT;

	zap = malloc(size);
	if (zap == NULL)
		return (ENOMEM);

	rc = dnode_read(spa, dnode, 0, zap, size);
	if (rc)
		goto done;

	switch (zap->zap_block_type) {
	case ZBT_MICRO:
		rc = mzap_lookup((const mzap_phys_t *)zap, size, name, value);
		break;
	case ZBT_HEADER:
		rc = fzap_lookup(spa, dnode, zap, name, integer_size,
		    num_integers, value);
		break;
	default:
		printf("ZFS: invalid zap_type=%" PRIx64 "\n",
		    zap->zap_block_type);
		rc = EIO;
	}
done:
	free(zap);
	return (rc);
}

/*
 * List a microzap directory.
 */
static int
mzap_list(const mzap_phys_t *mz, size_t size,
    int (*callback)(const char *, uint64_t))
{
	const mzap_ent_phys_t *mze;
	int chunks, i, rc;

	/*
	 * Microzap objects use exactly one block. Read the whole
	 * thing.
	 */
	rc = 0;
	chunks = size / MZAP_ENT_LEN - 1;
	for (i = 0; i < chunks; i++) {
		mze = &mz->mz_chunk[i];
		if (mze->mze_name[0]) {
			rc = callback(mze->mze_name, mze->mze_value);
			if (rc != 0)
				break;
		}
	}

	return (rc);
}

/*
 * List a fatzap directory.
 */
static int
fzap_list(const spa_t *spa, const dnode_phys_t *dnode, zap_phys_t *zh,
    int (*callback)(const char *, uint64_t))
{
	int bsize = dnode->dn_datablkszsec << SPA_MINBLOCKSHIFT;
	fat_zap_t z;
	uint64_t i;
	int j, rc;

	if (zh->zap_magic != ZAP_MAGIC)
		return (EIO);

	z.zap_block_shift = ilog2(bsize);
	z.zap_phys = zh;

	/*
	 * This assumes that the leaf blocks start at block 1. The
	 * documentation isn't exactly clear on this.
	 */
	zap_leaf_t zl;
	zl.l_bs = z.zap_block_shift;
	zl.l_phys = malloc(bsize);
	if (zl.l_phys == NULL)
		return (ENOMEM);

	for (i = 0; i < zh->zap_num_leafs; i++) {
		off_t off = ((off_t)(i + 1)) << zl.l_bs;
		char name[256], *p;
		uint64_t value;

		if (dnode_read(spa, dnode, off, zl.l_phys, bsize)) {
			free(zl.l_phys);
			return (EIO);
		}

		for (j = 0; j < ZAP_LEAF_NUMCHUNKS(&zl); j++) {
			zap_leaf_chunk_t *zc, *nc;
			int namelen;

			zc = &ZAP_LEAF_CHUNK(&zl, j);
			if (zc->l_entry.le_type != ZAP_CHUNK_ENTRY)
				continue;
			namelen = zc->l_entry.le_name_numints;
			if (namelen > sizeof(name))
				namelen = sizeof(name);

			/*
			 * Paste the name back together.
			 */
			nc = &ZAP_LEAF_CHUNK(&zl, zc->l_entry.le_name_chunk);
			p = name;
			while (namelen > 0) {
				int len;
				len = namelen;
				if (len > ZAP_LEAF_ARRAY_BYTES)
					len = ZAP_LEAF_ARRAY_BYTES;
				memcpy(p, nc->l_array.la_array, len);
				p += len;
				namelen -= len;
				nc = &ZAP_LEAF_CHUNK(&zl, nc->l_array.la_next);
			}

			/*
			 * Assume the first eight bytes of the value are
			 * a uint64_t.
			 */
			value = fzap_leaf_value(&zl, zc);

			/* printf("%s 0x%jx\n", name, (uintmax_t)value); */
			rc = callback((const char *)name, value);
			if (rc != 0) {
				free(zl.l_phys);
				return (rc);
			}
		}
	}

	free(zl.l_phys);
	return (0);
}

static int zfs_printf(const char *name, uint64_t value __unused)
{

	printf("%s\n", name);

	return (0);
}

/*
 * List a zap directory.
 */
static int
zap_list(const spa_t *spa, const dnode_phys_t *dnode)
{
	zap_phys_t *zap;
	size_t size = dnode->dn_datablkszsec << SPA_MINBLOCKSHIFT;
	int rc;

	zap = malloc(size);
	if (zap == NULL)
		return (ENOMEM);

	rc = dnode_read(spa, dnode, 0, zap, size);
	if (rc == 0) {
		if (zap->zap_block_type == ZBT_MICRO)
			rc = mzap_list((const mzap_phys_t *)zap, size,
			    zfs_printf);
		else
			rc = fzap_list(spa, dnode, zap, zfs_printf);
	}
	free(zap);
	return (rc);
}

static int
objset_get_dnode(const spa_t *spa, const objset_phys_t *os, uint64_t objnum,
    dnode_phys_t *dnode)
{
	off_t offset;

	offset = objnum * sizeof(dnode_phys_t);
	return dnode_read(spa, &os->os_meta_dnode, offset,
		dnode, sizeof(dnode_phys_t));
}

/*
 * Lookup a name in a microzap directory.
 */
static int
mzap_rlookup(const mzap_phys_t *mz, size_t size, char *name, uint64_t value)
{
	const mzap_ent_phys_t *mze;
	int chunks, i;

	/*
	 * Microzap objects use exactly one block. Read the whole
	 * thing.
	 */
	chunks = size / MZAP_ENT_LEN - 1;
	for (i = 0; i < chunks; i++) {
		mze = &mz->mz_chunk[i];
		if (value == mze->mze_value) {
			strcpy(name, mze->mze_name);
			return (0);
		}
	}

	return (ENOENT);
}

static void
fzap_name_copy(const zap_leaf_t *zl, const zap_leaf_chunk_t *zc, char *name)
{
	size_t namelen;
	const zap_leaf_chunk_t *nc;
	char *p;

	namelen = zc->l_entry.le_name_numints;

	nc = &ZAP_LEAF_CHUNK(zl, zc->l_entry.le_name_chunk);
	p = name;
	while (namelen > 0) {
		size_t len;
		len = namelen;
		if (len > ZAP_LEAF_ARRAY_BYTES)
			len = ZAP_LEAF_ARRAY_BYTES;
		memcpy(p, nc->l_array.la_array, len);
		p += len;
		namelen -= len;
		nc = &ZAP_LEAF_CHUNK(zl, nc->l_array.la_next);
	}

	*p = '\0';
}

static int
fzap_rlookup(const spa_t *spa, const dnode_phys_t *dnode, zap_phys_t *zh,
    char *name, uint64_t value)
{
	int bsize = dnode->dn_datablkszsec << SPA_MINBLOCKSHIFT;
	fat_zap_t z;
	uint64_t i;
	int j, rc;

	if (zh->zap_magic != ZAP_MAGIC)
		return (EIO);

	z.zap_block_shift = ilog2(bsize);
	z.zap_phys = zh;

	/*
	 * This assumes that the leaf blocks start at block 1. The
	 * documentation isn't exactly clear on this.
	 */
	zap_leaf_t zl;
	zl.l_bs = z.zap_block_shift;
	zl.l_phys = malloc(bsize);
	if (zl.l_phys == NULL)
		return (ENOMEM);

	for (i = 0; i < zh->zap_num_leafs; i++) {
		off_t off = ((off_t)(i + 1)) << zl.l_bs;

		rc = dnode_read(spa, dnode, off, zl.l_phys, bsize);
		if (rc != 0)
			goto done;

		for (j = 0; j < ZAP_LEAF_NUMCHUNKS(&zl); j++) {
			zap_leaf_chunk_t *zc;

			zc = &ZAP_LEAF_CHUNK(&zl, j);
			if (zc->l_entry.le_type != ZAP_CHUNK_ENTRY)
				continue;
			if (zc->l_entry.le_value_intlen != 8 ||
			    zc->l_entry.le_value_numints != 1)
				continue;

			if (fzap_leaf_value(&zl, zc) == value) {
				fzap_name_copy(&zl, zc, name);
				goto done;
			}
		}
	}

	rc = ENOENT;
done:
	free(zl.l_phys);
	return (rc);
}

static int
zap_rlookup(const spa_t *spa, const dnode_phys_t *dnode, char *name,
    uint64_t value)
{
	zap_phys_t *zap;
	size_t size = dnode->dn_datablkszsec << SPA_MINBLOCKSHIFT;
	int rc;

	zap = malloc(size);
	if (zap == NULL)
		return (ENOMEM);

	rc = dnode_read(spa, dnode, 0, zap, size);
	if (rc == 0) {
		if (zap->zap_block_type == ZBT_MICRO)
			rc = mzap_rlookup((const mzap_phys_t *)zap, size,
			    name, value);
		else
			rc = fzap_rlookup(spa, dnode, zap, name, value);
	}
	free(zap);
	return (rc);
}

static int
zfs_rlookup(const spa_t *spa, uint64_t objnum, char *result)
{
	char name[256];
	char component[256];
	uint64_t dir_obj, parent_obj, child_dir_zapobj;
	dnode_phys_t child_dir_zap, snapnames_zap, dataset, dir, parent;
	dsl_dir_phys_t *dd;
	dsl_dataset_phys_t *ds;
	char *p;
	int len;
	boolean_t issnap = B_FALSE;

	p = &name[sizeof(name) - 1];
	*p = '\0';

	if (objset_get_dnode(spa, spa->spa_mos, objnum, &dataset)) {
		printf("ZFS: can't find dataset %ju\n", (uintmax_t)objnum);
		return (EIO);
	}
	ds = (dsl_dataset_phys_t *)&dataset.dn_bonus;
	dir_obj = ds->ds_dir_obj;
	if (ds->ds_snapnames_zapobj == 0)
		issnap = B_TRUE;

	for (;;) {
		if (objset_get_dnode(spa, spa->spa_mos, dir_obj, &dir) != 0)
			return (EIO);
		dd = (dsl_dir_phys_t *)&dir.dn_bonus;

		/* Actual loop condition. */
		parent_obj = dd->dd_parent_obj;
		if (parent_obj == 0)
			break;

		if (objset_get_dnode(spa, spa->spa_mos, parent_obj,
		    &parent) != 0)
			return (EIO);
		dd = (dsl_dir_phys_t *)&parent.dn_bonus;
		if (issnap == B_TRUE) {
			/*
			 * The dataset we are looking up is a snapshot
			 * the dir_obj is the parent already, we don't want
			 * the grandparent just yet. Reset to the parent.
			 */
			dd = (dsl_dir_phys_t *)&dir.dn_bonus;
			/* Lookup the dataset to get the snapname ZAP */
			if (objset_get_dnode(spa, spa->spa_mos,
			    dd->dd_head_dataset_obj, &dataset))
				return (EIO);
			ds = (dsl_dataset_phys_t *)&dataset.dn_bonus;
			if (objset_get_dnode(spa, spa->spa_mos,
			    ds->ds_snapnames_zapobj, &snapnames_zap) != 0)
				return (EIO);
			/* Get the name of the snapshot */
			if (zap_rlookup(spa, &snapnames_zap, component,
			    objnum) != 0)
				return (EIO);
			len = strlen(component);
			p -= len;
			memcpy(p, component, len);
			--p;
			*p = '@';
			issnap = B_FALSE;
			continue;
		}

		child_dir_zapobj = dd->dd_child_dir_zapobj;
		if (objset_get_dnode(spa, spa->spa_mos, child_dir_zapobj,
		    &child_dir_zap) != 0)
			return (EIO);
		if (zap_rlookup(spa, &child_dir_zap, component, dir_obj) != 0)
			return (EIO);

		len = strlen(component);
		p -= len;
		memcpy(p, component, len);
		--p;
		*p = '/';

		/* Actual loop iteration. */
		dir_obj = parent_obj;
	}

	if (*p != '\0')
		++p;
	strcpy(result, p);

	return (0);
}

static int
zfs_lookup_dataset(const spa_t *spa, const char *name, uint64_t *objnum)
{
	char element[256];
	uint64_t dir_obj, child_dir_zapobj;
	dnode_phys_t child_dir_zap, snapnames_zap, dir, dataset;
	dsl_dir_phys_t *dd;
	dsl_dataset_phys_t *ds;
	const char *p, *q;
	boolean_t issnap = B_FALSE;

	if (objset_get_dnode(spa, spa->spa_mos,
	    DMU_POOL_DIRECTORY_OBJECT, &dir))
		return (EIO);
	if (zap_lookup(spa, &dir, DMU_POOL_ROOT_DATASET, sizeof (dir_obj),
	    1, &dir_obj))
		return (EIO);

	p = name;
	for (;;) {
		if (objset_get_dnode(spa, spa->spa_mos, dir_obj, &dir))
			return (EIO);
		dd = (dsl_dir_phys_t *)&dir.dn_bonus;

		while (*p == '/')
			p++;
		/* Actual loop condition #1. */
		if (*p == '\0')
			break;

		q = strchr(p, '/');
		if (q) {
			memcpy(element, p, q - p);
			element[q - p] = '\0';
			p = q + 1;
		} else {
			strcpy(element, p);
			p += strlen(p);
		}

		if (issnap == B_TRUE) {
		        if (objset_get_dnode(spa, spa->spa_mos,
			    dd->dd_head_dataset_obj, &dataset))
		                return (EIO);
			ds = (dsl_dataset_phys_t *)&dataset.dn_bonus;
			if (objset_get_dnode(spa, spa->spa_mos,
			    ds->ds_snapnames_zapobj, &snapnames_zap) != 0)
				return (EIO);
			/* Actual loop condition #2. */
			if (zap_lookup(spa, &snapnames_zap, element,
			    sizeof (dir_obj), 1, &dir_obj) != 0)
				return (ENOENT);
			*objnum = dir_obj;
			return (0);
		} else if ((q = strchr(element, '@')) != NULL) {
			issnap = B_TRUE;
			element[q - element] = '\0';
			p = q + 1;
		}
		child_dir_zapobj = dd->dd_child_dir_zapobj;
		if (objset_get_dnode(spa, spa->spa_mos, child_dir_zapobj,
		    &child_dir_zap) != 0)
			return (EIO);

		/* Actual loop condition #2. */
		if (zap_lookup(spa, &child_dir_zap, element, sizeof (dir_obj),
		    1, &dir_obj) != 0)
			return (ENOENT);
	}

	*objnum = dd->dd_head_dataset_obj;
	return (0);
}

#ifndef BOOT2
static int
zfs_list_dataset(const spa_t *spa, uint64_t objnum/*, int pos, char *entry*/)
{
	uint64_t dir_obj, child_dir_zapobj;
	dnode_phys_t child_dir_zap, dir, dataset;
	dsl_dataset_phys_t *ds;
	dsl_dir_phys_t *dd;

	if (objset_get_dnode(spa, spa->spa_mos, objnum, &dataset)) {
		printf("ZFS: can't find dataset %ju\n", (uintmax_t)objnum);
		return (EIO);
	}
	ds = (dsl_dataset_phys_t *)&dataset.dn_bonus;
	dir_obj = ds->ds_dir_obj;

	if (objset_get_dnode(spa, spa->spa_mos, dir_obj, &dir)) {
		printf("ZFS: can't find dirobj %ju\n", (uintmax_t)dir_obj);
		return (EIO);
	}
	dd = (dsl_dir_phys_t *)&dir.dn_bonus;

	child_dir_zapobj = dd->dd_child_dir_zapobj;
	if (objset_get_dnode(spa, spa->spa_mos, child_dir_zapobj,
	    &child_dir_zap) != 0) {
		printf("ZFS: can't find child zap %ju\n", (uintmax_t)dir_obj);
		return (EIO);
	}

	return (zap_list(spa, &child_dir_zap) != 0);
}

int
zfs_callback_dataset(const spa_t *spa, uint64_t objnum,
    int (*callback)(const char *, uint64_t))
{
	uint64_t dir_obj, child_dir_zapobj;
	dnode_phys_t child_dir_zap, dir, dataset;
	dsl_dataset_phys_t *ds;
	dsl_dir_phys_t *dd;
	zap_phys_t *zap;
	size_t size;
	int err;

	err = objset_get_dnode(spa, spa->spa_mos, objnum, &dataset);
	if (err != 0) {
		printf("ZFS: can't find dataset %ju\n", (uintmax_t)objnum);
		return (err);
	}
	ds = (dsl_dataset_phys_t *)&dataset.dn_bonus;
	dir_obj = ds->ds_dir_obj;

	err = objset_get_dnode(spa, spa->spa_mos, dir_obj, &dir);
	if (err != 0) {
		printf("ZFS: can't find dirobj %ju\n", (uintmax_t)dir_obj);
		return (err);
	}
	dd = (dsl_dir_phys_t *)&dir.dn_bonus;

	child_dir_zapobj = dd->dd_child_dir_zapobj;
	err = objset_get_dnode(spa, spa->spa_mos, child_dir_zapobj,
	    &child_dir_zap);
	if (err != 0) {
		printf("ZFS: can't find child zap %ju\n", (uintmax_t)dir_obj);
		return (err);
	}

	size = child_dir_zap.dn_datablkszsec << SPA_MINBLOCKSHIFT;
	zap = malloc(size);
	if (zap != NULL) {
		err = dnode_read(spa, &child_dir_zap, 0, zap, size);
		if (err != 0)
			goto done;

		if (zap->zap_block_type == ZBT_MICRO)
			err = mzap_list((const mzap_phys_t *)zap, size,
			    callback);
		else
			err = fzap_list(spa, &child_dir_zap, zap, callback);
	} else {
		err = ENOMEM;
	}
done:
	free(zap);
	return (err);
}
#endif

/*
 * Find the object set given the object number of its dataset object
 * and return its details in *objset
 */
static int
zfs_mount_dataset(const spa_t *spa, uint64_t objnum, objset_phys_t *objset)
{
	dnode_phys_t dataset;
	dsl_dataset_phys_t *ds;

	if (objset_get_dnode(spa, spa->spa_mos, objnum, &dataset)) {
		printf("ZFS: can't find dataset %ju\n", (uintmax_t)objnum);
		return (EIO);
	}

	ds = (dsl_dataset_phys_t *)&dataset.dn_bonus;
	if (zio_read(spa, &ds->ds_bp, objset)) {
		printf("ZFS: can't read object set for dataset %ju\n",
		    (uintmax_t)objnum);
		return (EIO);
	}

	return (0);
}

/*
 * Find the object set pointed to by the BOOTFS property or the root
 * dataset if there is none and return its details in *objset
 */
static int
zfs_get_root(const spa_t *spa, uint64_t *objid)
{
	dnode_phys_t dir, propdir;
	uint64_t props, bootfs, root;

	*objid = 0;

	/*
	 * Start with the MOS directory object.
	 */
	if (objset_get_dnode(spa, spa->spa_mos,
	    DMU_POOL_DIRECTORY_OBJECT, &dir)) {
		printf("ZFS: can't read MOS object directory\n");
		return (EIO);
	}

	/*
	 * Lookup the pool_props and see if we can find a bootfs.
	 */
	if (zap_lookup(spa, &dir, DMU_POOL_PROPS,
	    sizeof(props), 1, &props) == 0 &&
	    objset_get_dnode(spa, spa->spa_mos, props, &propdir) == 0 &&
	    zap_lookup(spa, &propdir, "bootfs",
	    sizeof(bootfs), 1, &bootfs) == 0 && bootfs != 0) {
		*objid = bootfs;
		return (0);
	}
	/*
	 * Lookup the root dataset directory
	 */
	if (zap_lookup(spa, &dir, DMU_POOL_ROOT_DATASET,
	    sizeof(root), 1, &root) ||
	    objset_get_dnode(spa, spa->spa_mos, root, &dir)) {
		printf("ZFS: can't find root dsl_dir\n");
		return (EIO);
	}

	/*
	 * Use the information from the dataset directory's bonus buffer
	 * to find the dataset object and from that the object set itself.
	 */
	dsl_dir_phys_t *dd = (dsl_dir_phys_t *)&dir.dn_bonus;
	*objid = dd->dd_head_dataset_obj;
	return (0);
}

static int
zfs_mount_impl(const spa_t *spa, uint64_t rootobj, struct zfsmount *mount)
{

	mount->spa = spa;

	/*
	 * Find the root object set if not explicitly provided
	 */
	if (rootobj == 0 && zfs_get_root(spa, &rootobj)) {
		printf("ZFS: can't find root filesystem\n");
		return (EIO);
	}

	if (zfs_mount_dataset(spa, rootobj, &mount->objset)) {
		printf("ZFS: can't open root filesystem\n");
		return (EIO);
	}

	mount->rootobj = rootobj;

	return (0);
}

/*
 * callback function for feature name checks.
 */
static int
check_feature(const char *name, uint64_t value)
{
	int i;

	if (value == 0)
		return (0);
	if (name[0] == '\0')
		return (0);

	for (i = 0; features_for_read[i] != NULL; i++) {
		if (strcmp(name, features_for_read[i]) == 0)
			return (0);
	}
	printf("ZFS: unsupported feature: %s\n", name);
	return (EIO);
}

/*
 * Checks whether the MOS features that are active are supported.
 */
static int
check_mos_features(const spa_t *spa)
{
	dnode_phys_t dir;
	zap_phys_t *zap;
	uint64_t objnum;
	size_t size;
	int rc;

	if ((rc = objset_get_dnode(spa, spa->spa_mos, DMU_OT_OBJECT_DIRECTORY,
	    &dir)) != 0)
		return (rc);
	if ((rc = zap_lookup(spa, &dir, DMU_POOL_FEATURES_FOR_READ,
	    sizeof (objnum), 1, &objnum)) != 0) {
		/*
		 * It is older pool without features. As we have already
		 * tested the label, just return without raising the error.
		 */
		return (0);
	}

	if ((rc = objset_get_dnode(spa, spa->spa_mos, objnum, &dir)) != 0)
		return (rc);

	if (dir.dn_type != DMU_OTN_ZAP_METADATA)
		return (EIO);

	size = dir.dn_datablkszsec << SPA_MINBLOCKSHIFT;
	zap = malloc(size);
	if (zap == NULL)
		return (ENOMEM);

	if (dnode_read(spa, &dir, 0, zap, size)) {
		free(zap);
		return (EIO);
	}

	if (zap->zap_block_type == ZBT_MICRO)
		rc = mzap_list((const mzap_phys_t *)zap, size, check_feature);
	else
		rc = fzap_list(spa, &dir, zap, check_feature);

	free(zap);
	return (rc);
}

static int
load_nvlist(spa_t *spa, uint64_t obj, nvlist_t **value)
{
	dnode_phys_t dir;
	size_t size;
	int rc;
	char *nv;

	*value = NULL;
	if ((rc = objset_get_dnode(spa, spa->spa_mos, obj, &dir)) != 0)
		return (rc);
	if (dir.dn_type != DMU_OT_PACKED_NVLIST &&
	    dir.dn_bonustype != DMU_OT_PACKED_NVLIST_SIZE) {
		return (EIO);
	}

	if (dir.dn_bonuslen != sizeof (uint64_t))
		return (EIO);

	size = *(uint64_t *)DN_BONUS(&dir);
	nv = malloc(size);
	if (nv == NULL)
		return (ENOMEM);

	rc = dnode_read(spa, &dir, 0, nv, size);
	if (rc != 0) {
		free(nv);
		nv = NULL;
		return (rc);
	}
	*value = nvlist_import(nv, size);
	free(nv);
	return (rc);
}

static int
zfs_spa_init(spa_t *spa)
{
	struct uberblock checkpoint;
	dnode_phys_t dir;
	uint64_t config_object;
	nvlist_t *nvlist;
	int rc;

	if (zio_read(spa, &spa->spa_uberblock->ub_rootbp, spa->spa_mos)) {
		printf("ZFS: can't read MOS of pool %s\n", spa->spa_name);
		return (EIO);
	}
	if (spa->spa_mos->os_type != DMU_OST_META) {
		printf("ZFS: corrupted MOS of pool %s\n", spa->spa_name);
		return (EIO);
	}

	if (objset_get_dnode(spa, &spa->spa_mos_master,
	    DMU_POOL_DIRECTORY_OBJECT, &dir)) {
		printf("ZFS: failed to read pool %s directory object\n",
		    spa->spa_name);
		return (EIO);
	}
	/* this is allowed to fail, older pools do not have salt */
	rc = zap_lookup(spa, &dir, DMU_POOL_CHECKSUM_SALT, 1,
	    sizeof (spa->spa_cksum_salt.zcs_bytes),
	    spa->spa_cksum_salt.zcs_bytes);

	rc = check_mos_features(spa);
	if (rc != 0) {
		printf("ZFS: pool %s is not supported\n", spa->spa_name);
		return (rc);
	}

	rc = zap_lookup(spa, &dir, DMU_POOL_CONFIG,
	    sizeof (config_object), 1, &config_object);
	if (rc != 0) {
		printf("ZFS: can not read MOS %s\n", DMU_POOL_CONFIG);
		return (EIO);
	}
	rc = load_nvlist(spa, config_object, &nvlist);
	if (rc != 0)
		return (rc);

	rc = zap_lookup(spa, &dir, DMU_POOL_ZPOOL_CHECKPOINT,
	    sizeof(uint64_t), sizeof(checkpoint) / sizeof(uint64_t),
	    &checkpoint);
	if (rc == 0 && checkpoint.ub_checkpoint_txg != 0) {
		memcpy(&spa->spa_uberblock_checkpoint, &checkpoint,
		    sizeof(checkpoint));
		if (zio_read(spa, &spa->spa_uberblock_checkpoint.ub_rootbp,
		    &spa->spa_mos_checkpoint)) {
			printf("ZFS: can not read checkpoint data.\n");
			return (EIO);
		}
	}

	/*
	 * Update vdevs from MOS config. Note, we do skip encoding bytes
	 * here. See also vdev_label_read_config().
	 */
	rc = vdev_init_from_nvlist(spa, nvlist);
	nvlist_destroy(nvlist);
	return (rc);
}

static int
zfs_dnode_stat(const spa_t *spa, dnode_phys_t *dn, struct stat *sb)
{

	if (dn->dn_bonustype != DMU_OT_SA) {
		znode_phys_t *zp = (znode_phys_t *)dn->dn_bonus;

		sb->st_mode = zp->zp_mode;
		sb->st_uid = zp->zp_uid;
		sb->st_gid = zp->zp_gid;
		sb->st_size = zp->zp_size;
	} else {
		sa_hdr_phys_t *sahdrp;
		int hdrsize;
		size_t size = 0;
		void *buf = NULL;

		if (dn->dn_bonuslen != 0)
			sahdrp = (sa_hdr_phys_t *)DN_BONUS(dn);
		else {
			if ((dn->dn_flags & DNODE_FLAG_SPILL_BLKPTR) != 0) {
				blkptr_t *bp = DN_SPILL_BLKPTR(dn);
				int error;

				size = BP_GET_LSIZE(bp);
				buf = malloc(size);
				if (buf == NULL)
					error = ENOMEM;
				else
					error = zio_read(spa, bp, buf);

				if (error != 0) {
					free(buf);
					return (error);
				}
				sahdrp = buf;
			} else {
				return (EIO);
			}
		}
		hdrsize = SA_HDR_SIZE(sahdrp);
		sb->st_mode = *(uint64_t *)((char *)sahdrp + hdrsize +
		    SA_MODE_OFFSET);
		sb->st_uid = *(uint64_t *)((char *)sahdrp + hdrsize +
		    SA_UID_OFFSET);
		sb->st_gid = *(uint64_t *)((char *)sahdrp + hdrsize +
		    SA_GID_OFFSET);
		sb->st_size = *(uint64_t *)((char *)sahdrp + hdrsize +
		    SA_SIZE_OFFSET);
		free(buf);
	}

	return (0);
}

static int
zfs_dnode_readlink(const spa_t *spa, dnode_phys_t *dn, char *path, size_t psize)
{
	int rc = 0;

	if (dn->dn_bonustype == DMU_OT_SA) {
		sa_hdr_phys_t *sahdrp = NULL;
		size_t size = 0;
		void *buf = NULL;
		int hdrsize;
		char *p;

		if (dn->dn_bonuslen != 0) {
			sahdrp = (sa_hdr_phys_t *)DN_BONUS(dn);
		} else {
			blkptr_t *bp;

			if ((dn->dn_flags & DNODE_FLAG_SPILL_BLKPTR) == 0)
				return (EIO);
			bp = DN_SPILL_BLKPTR(dn);

			size = BP_GET_LSIZE(bp);
			buf = malloc(size);
			if (buf == NULL)
				rc = ENOMEM;
			else
				rc = zio_read(spa, bp, buf);
			if (rc != 0) {
				free(buf);
				return (rc);
			}
			sahdrp = buf;
		}
		hdrsize = SA_HDR_SIZE(sahdrp);
		p = (char *)((uintptr_t)sahdrp + hdrsize + SA_SYMLINK_OFFSET);
		memcpy(path, p, psize);
		free(buf);
		return (0);
	}
	/*
	 * Second test is purely to silence bogus compiler
	 * warning about accessing past the end of dn_bonus.
	 */
	if (psize + sizeof(znode_phys_t) <= dn->dn_bonuslen &&
	    sizeof(znode_phys_t) <= sizeof(dn->dn_bonus)) {
		memcpy(path, &dn->dn_bonus[sizeof(znode_phys_t)], psize);
	} else {
		rc = dnode_read(spa, dn, 0, path, psize);
	}
	return (rc);
}

struct obj_list {
	uint64_t		objnum;
	STAILQ_ENTRY(obj_list)	entry;
};

/*
 * Lookup a file and return its dnode.
 */
static int
zfs_lookup(const struct zfsmount *mount, const char *upath, dnode_phys_t *dnode)
{
	int rc;
	uint64_t objnum;
	const spa_t *spa;
	dnode_phys_t dn;
	const char *p, *q;
	char element[256];
	char path[1024];
	int symlinks_followed = 0;
	struct stat sb;
	struct obj_list *entry, *tentry;
	STAILQ_HEAD(, obj_list) on_cache = STAILQ_HEAD_INITIALIZER(on_cache);

	spa = mount->spa;
	if (mount->objset.os_type != DMU_OST_ZFS) {
		printf("ZFS: unexpected object set type %ju\n",
		    (uintmax_t)mount->objset.os_type);
		return (EIO);
	}

	if ((entry = malloc(sizeof(struct obj_list))) == NULL)
		return (ENOMEM);

	/*
	 * Get the root directory dnode.
	 */
	rc = objset_get_dnode(spa, &mount->objset, MASTER_NODE_OBJ, &dn);
	if (rc) {
		free(entry);
		return (rc);
	}

	rc = zap_lookup(spa, &dn, ZFS_ROOT_OBJ, sizeof(objnum), 1, &objnum);
	if (rc) {
		free(entry);
		return (rc);
	}
	entry->objnum = objnum;
	STAILQ_INSERT_HEAD(&on_cache, entry, entry);

	rc = objset_get_dnode(spa, &mount->objset, objnum, &dn);
	if (rc != 0)
		goto done;

	p = upath;
	while (p && *p) {
		rc = objset_get_dnode(spa, &mount->objset, objnum, &dn);
		if (rc != 0)
			goto done;

		while (*p == '/')
			p++;
		if (*p == '\0')
			break;
		q = p;
		while (*q != '\0' && *q != '/')
			q++;

		/* skip dot */
		if (p + 1 == q && p[0] == '.') {
			p++;
			continue;
		}
		/* double dot */
		if (p + 2 == q && p[0] == '.' && p[1] == '.') {
			p += 2;
			if (STAILQ_FIRST(&on_cache) ==
			    STAILQ_LAST(&on_cache, obj_list, entry)) {
				rc = ENOENT;
				goto done;
			}
			entry = STAILQ_FIRST(&on_cache);
			STAILQ_REMOVE_HEAD(&on_cache, entry);
			free(entry);
			objnum = (STAILQ_FIRST(&on_cache))->objnum;
			continue;
		}
		if (q - p + 1 > sizeof(element)) {
			rc = ENAMETOOLONG;
			goto done;
		}
		memcpy(element, p, q - p);
		element[q - p] = 0;
		p = q;

		if ((rc = zfs_dnode_stat(spa, &dn, &sb)) != 0)
			goto done;
		if (!S_ISDIR(sb.st_mode)) {
			rc = ENOTDIR;
			goto done;
		}

		rc = zap_lookup(spa, &dn, element, sizeof (objnum), 1, &objnum);
		if (rc)
			goto done;
		objnum = ZFS_DIRENT_OBJ(objnum);

		if ((entry = malloc(sizeof(struct obj_list))) == NULL) {
			rc = ENOMEM;
			goto done;
		}
		entry->objnum = objnum;
		STAILQ_INSERT_HEAD(&on_cache, entry, entry);
		rc = objset_get_dnode(spa, &mount->objset, objnum, &dn);
		if (rc)
			goto done;

		/*
		 * Check for symlink.
		 */
		rc = zfs_dnode_stat(spa, &dn, &sb);
		if (rc)
			goto done;
		if (S_ISLNK(sb.st_mode)) {
			if (symlinks_followed > 10) {
				rc = EMLINK;
				goto done;
			}
			symlinks_followed++;

			/*
			 * Read the link value and copy the tail of our
			 * current path onto the end.
			 */
			if (sb.st_size + strlen(p) + 1 > sizeof(path)) {
				rc = ENAMETOOLONG;
				goto done;
			}
			strcpy(&path[sb.st_size], p);

			rc = zfs_dnode_readlink(spa, &dn, path, sb.st_size);
			if (rc != 0)
				goto done;

			/*
			 * Restart with the new path, starting either at
			 * the root or at the parent depending whether or
			 * not the link is relative.
			 */
			p = path;
			if (*p == '/') {
				while (STAILQ_FIRST(&on_cache) !=
				    STAILQ_LAST(&on_cache, obj_list, entry)) {
					entry = STAILQ_FIRST(&on_cache);
					STAILQ_REMOVE_HEAD(&on_cache, entry);
					free(entry);
				}
			} else {
				entry = STAILQ_FIRST(&on_cache);
				STAILQ_REMOVE_HEAD(&on_cache, entry);
				free(entry);
			}
			objnum = (STAILQ_FIRST(&on_cache))->objnum;
		}
	}

	*dnode = dn;
done:
	STAILQ_FOREACH_SAFE(entry, &on_cache, entry, tentry)
		free(entry);
	return (rc);
}

/*
 * Return either a cached copy of the bootenv, or read each of the vdev children
 * looking for the bootenv. Cache what's found and return the results. Returns 0
 * when benvp is filled in, and some errno when not.
 */
static int
zfs_get_bootenv_spa(spa_t *spa, nvlist_t **benvp)
{
	vdev_t *vd;
	nvlist_t *benv = NULL;

	if (spa->spa_bootenv == NULL) {
		STAILQ_FOREACH(vd, &spa->spa_root_vdev->v_children,
		    v_childlink) {
			benv = vdev_read_bootenv(vd);

			if (benv != NULL)
				break;
		}
		spa->spa_bootenv = benv;
	}
	benv = spa->spa_bootenv;

	if (benv == NULL)
		return (ENOENT);

	*benvp = benv;
	return (0);
}

/*
 * Store nvlist to pool label bootenv area. Also updates cached pointer in spa.
 */
static int
zfs_set_bootenv_spa(spa_t *spa, nvlist_t *benv)
{
	vdev_t *vd;

	STAILQ_FOREACH(vd, &spa->spa_root_vdev->v_children, v_childlink) {
		vdev_write_bootenv(vd, benv);
	}

	spa->spa_bootenv = benv;
	return (0);
}

/*
 * Get bootonce value by key. The bootonce <key, value> pair is removed from the
 * bootenv nvlist and the remaining nvlist is committed back to disk. This process
 * the bootonce flag since we've reached the point in the boot that we've 'used'
 * the BE. For chained boot scenarios, we may reach this point multiple times (but
 * only remove it and return 0 the first time).
 */
static int
zfs_get_bootonce_spa(spa_t *spa, const char *key, char *buf, size_t size)
{
	nvlist_t *benv;
	char *result = NULL;
	int result_size, rv;

	if ((rv = zfs_get_bootenv_spa(spa, &benv)) != 0)
		return (rv);

	if ((rv = nvlist_find(benv, key, DATA_TYPE_STRING, NULL,
	    &result, &result_size)) == 0) {
		if (result_size == 0) {
			/* ignore empty string */
			rv = ENOENT;
		} else if (buf != NULL) {
			size = MIN((size_t)result_size + 1, size);
			strlcpy(buf, result, size);
		}
		(void)nvlist_remove(benv, key, DATA_TYPE_STRING);
		(void)zfs_set_bootenv_spa(spa, benv);
	}

	return (rv);
}
