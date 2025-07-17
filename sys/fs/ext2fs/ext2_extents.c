/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2010 Zheng Liu <lz@freebsd.org>
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/vnode.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/endian.h>
#include <sys/conf.h>
#include <sys/sdt.h>
#include <sys/stat.h>

#include <fs/ext2fs/ext2_mount.h>
#include <fs/ext2fs/fs.h>
#include <fs/ext2fs/inode.h>
#include <fs/ext2fs/ext2fs.h>
#include <fs/ext2fs/ext2_extents.h>
#include <fs/ext2fs/ext2_extern.h>

SDT_PROVIDER_DECLARE(ext2fs);
/*
 * ext2fs trace probe:
 * arg0: verbosity. Higher numbers give more verbose messages
 * arg1: Textual message
 */
SDT_PROBE_DEFINE2(ext2fs, , trace, extents, "int", "char*");

static MALLOC_DEFINE(M_EXT2EXTENTS, "ext2_extents", "EXT2 extents");

#ifdef EXT2FS_PRINT_EXTENTS
static const bool print_extents_walk = true;

static int ext4_ext_check_header(struct inode *, struct ext4_extent_header *,
    int);
static int ext4_ext_walk_header(struct inode *, struct ext4_extent_header *,
    int);
static inline e4fs_daddr_t ext4_ext_index_pblock(struct ext4_extent_index *);
static inline e4fs_daddr_t ext4_ext_extent_pblock(struct ext4_extent *);

static int
ext4_ext_blk_check(struct inode *ip, e4fs_daddr_t blk)
{
	struct m_ext2fs *fs;

	fs = ip->i_e2fs;

	if (blk < fs->e2fs->e2fs_first_dblock || blk >= fs->e2fs_bcount)
		return (EIO);

	return (0);
}

static int
ext4_ext_walk_index(struct inode *ip, struct ext4_extent_index *ex, int depth,
    bool do_walk)
{
	struct m_ext2fs *fs;
	struct buf *bp;
	e4fs_daddr_t blk;
	int error;

	fs = ip->i_e2fs;

	if (print_extents_walk)
		printf("    index %p => (blk %u pblk %ju)\n", ex,
		    le32toh(ex->ei_blk),
		    (uint64_t)le16toh(ex->ei_leaf_hi) << 32 |
		    le32toh(ex->ei_leaf_lo));

	if(!do_walk)
		return (0);

	blk = ext4_ext_index_pblock(ex);
	error = ext4_ext_blk_check(ip, blk);
	if (error)
		return (error);

	if ((error = bread(ip->i_devvp,
	    fsbtodb(fs, blk), (int)fs->e2fs_bsize, NOCRED, &bp)) != 0) {
		brelse(bp);
		return (error);
	}

	error = ext4_ext_walk_header(ip,
	    (struct ext4_extent_header *)bp->b_data, depth);

	brelse(bp);

	return (error);
}

static int
ext4_ext_walk_extent(struct inode *ip, struct ext4_extent *ep)
{
	e4fs_daddr_t blk;
	int error;

	blk = ext4_ext_extent_pblock(ep);
	error = ext4_ext_blk_check(ip, blk);
	if (error)
		return (error);

	if (print_extents_walk)
		printf("    ext %p => (blk %u len %u start %ju)\n",
		    ep, le32toh(ep->e_blk), le16toh(ep->e_len),
		    (uint64_t)blk);

	return (0);
}

static int
ext4_ext_walk_header(struct inode *ip, struct ext4_extent_header *eh, int depth)
{
	int i, error = 0;

	error = ext4_ext_check_header(ip, eh, depth);
	if (error)
		return (error);

	if (print_extents_walk)
		printf("header %p => (entries %d max %d depth %d gen %d)\n",
		    eh, le16toh(eh->eh_ecount),
		    le16toh(eh->eh_max), le16toh(eh->eh_depth),
		    le32toh(eh->eh_gen));

	for (i = 0; i < le16toh(eh->eh_ecount) && error == 0; i++)
		if (eh->eh_depth != 0)
			error = ext4_ext_walk_index(ip,
			    (struct ext4_extent_index *)(eh + 1 + i), depth - 1,
			    true);
		else
			error = ext4_ext_walk_extent(ip,
			    (struct ext4_extent *)(eh + 1 + i));

	return (error);
}

int
ext4_ext_walk(struct inode *ip)
{
	struct ext4_extent_header *ehp;

	ehp = (struct ext4_extent_header *)ip->i_db;

	if (print_extents_walk)
		printf("Extent status:ip=%ju\n", ip->i_number);

	if (!(ip->i_flag & IN_E4EXTENTS))
		return (0);

	return (ext4_ext_walk_header(ip, ehp, 0));
}

static int
ext4_ext_print_path(struct inode *ip, struct ext4_extent_path *path)
{
	int k, depth, error = 0;

	depth = path->ep_depth;

	if (print_extents_walk)
		printf("ip=%ju, Path:\n", ip->i_number);

	for (k = 0; k <= depth && error == 0; k++, path++) {
		if (path->ep_index) {
			error = ext4_ext_walk_index(ip, path->ep_index,
			    depth - 1, false);
		} else if (path->ep_ext) {
			error = ext4_ext_walk_extent(ip, path->ep_ext);
		}
	}

	return (error);
}
#endif

static inline struct ext4_extent_header *
ext4_ext_inode_header(struct inode *ip)
{

	return ((struct ext4_extent_header *)ip->i_db);
}

static inline struct ext4_extent_header *
ext4_ext_block_header(char *bdata)
{

	return ((struct ext4_extent_header *)bdata);
}

static inline unsigned short
ext4_ext_inode_depth(struct inode *ip)
{
	struct ext4_extent_header *ehp;

	ehp = (struct ext4_extent_header *)ip->i_data;
	return (le16toh(ehp->eh_depth));
}

static inline e4fs_daddr_t
ext4_ext_index_pblock(struct ext4_extent_index *index)
{
	e4fs_daddr_t blk;

	blk = le32toh(index->ei_leaf_lo);
	blk |= (e4fs_daddr_t)le16toh(index->ei_leaf_hi) << 32;

	return (blk);
}

static inline void
ext4_index_store_pblock(struct ext4_extent_index *index, e4fs_daddr_t pb)
{

	index->ei_leaf_lo = htole32(pb & 0xffffffff);
	index->ei_leaf_hi = htole16((pb >> 32) & 0xffff);
}

static inline e4fs_daddr_t
ext4_ext_extent_pblock(struct ext4_extent *extent)
{
	e4fs_daddr_t blk;

	blk = le32toh(extent->e_start_lo);
	blk |= (e4fs_daddr_t)le16toh(extent->e_start_hi) << 32;

	return (blk);
}

static inline void
ext4_ext_store_pblock(struct ext4_extent *ex, e4fs_daddr_t pb)
{

	ex->e_start_lo = htole32(pb & 0xffffffff);
	ex->e_start_hi = htole16((pb >> 32) & 0xffff);
}

int
ext4_ext_in_cache(struct inode *ip, daddr_t lbn, struct ext4_extent *ep)
{
	struct ext4_extent_cache *ecp;
	int ret = EXT4_EXT_CACHE_NO;

	ecp = &ip->i_ext_cache;
	if (ecp->ec_type == EXT4_EXT_CACHE_NO)
		return (ret);

	if (lbn >= ecp->ec_blk && lbn < ecp->ec_blk + ecp->ec_len) {
		ep->e_blk = htole32(ecp->ec_blk);
		ep->e_start_lo = htole32(ecp->ec_start & 0xffffffff);
		ep->e_start_hi = htole16(ecp->ec_start >> 32 & 0xffff);
		ep->e_len = htole16(ecp->ec_len);
		ret = ecp->ec_type;
	}
	return (ret);
}

static inline int
ext4_ext_space_root(struct inode *ip)
{
	int size;

	size = sizeof(ip->i_data);
	size -= sizeof(struct ext4_extent_header);
	size /= sizeof(struct ext4_extent);

	return (size);
}

static inline int
ext4_ext_space_block(struct inode *ip)
{
	struct m_ext2fs *fs;
	int size;

	fs = ip->i_e2fs;

	size = (fs->e2fs_bsize - sizeof(struct ext4_extent_header)) /
	    sizeof(struct ext4_extent);

	return (size);
}

static inline int
ext4_ext_space_root_idx(struct inode *ip)
{
	int size;

	size = sizeof(ip->i_data);
	size -= sizeof(struct ext4_extent_header);
	size /= sizeof(struct ext4_extent_index);

	return (size);
}

static inline int
ext4_ext_space_block_idx(struct inode *ip)
{
	struct m_ext2fs *fs;
	int size;

	fs = ip->i_e2fs;

	size = (fs->e2fs_bsize - sizeof(struct ext4_extent_header)) /
	    sizeof(struct ext4_extent_index);

	return (size);
}

static int
ext4_ext_max_entries(struct inode *ip, int depth)
{

	if (depth == ext4_ext_inode_depth(ip)) {
		if (depth == 0)
			return (ext4_ext_space_root(ip));
		else
			return (ext4_ext_space_root_idx(ip));
	} else {
		if (depth == 0)
			return (ext4_ext_space_block(ip));
		else
			return (ext4_ext_space_block_idx(ip));
	}
}

static inline uint16_t
ext4_ext_get_actual_len(struct ext4_extent *ext)
{

	return (le16toh(ext->e_len) <= EXT_INIT_MAX_LEN ?
	    le16toh(ext->e_len) : (le16toh(ext->e_len) - EXT_INIT_MAX_LEN));
}


static int
ext4_inode_block_validate(struct inode *ip, e4fs_daddr_t start_blk,
    unsigned int count)
{
	struct m_ext2fs *fs;

	fs = ip->i_e2fs;

	if ((start_blk <= le32toh(fs->e2fs->e2fs_first_dblock)) ||
	    (start_blk + count < start_blk) ||
	    (start_blk + count > fs->e2fs_bcount))
		return (EIO);

	return (0);
}

static int
ext4_validate_extent(struct inode *ip, struct ext4_extent *ext)
{
	e4fs_daddr_t blk = ext4_ext_extent_pblock(ext);
	uint32_t lblk = le32toh(ext->e_blk);
	int len = ext4_ext_get_actual_len(ext);

	if (lblk + len <= lblk)
		return (EIO);

	return (ext4_inode_block_validate(ip, blk, len));
}

static int
ext4_validate_extent_idx(struct inode *ip, struct ext4_extent_index *ext_idx)
{
	e4fs_daddr_t blk = ext4_ext_index_pblock(ext_idx);

	return (ext4_inode_block_validate(ip, blk, 1));
}

static int
ext4_validate_extent_entries(struct inode *ip, struct ext4_extent_header *eh,
    int depth)
{
	unsigned int count;

	count = le16toh(eh->eh_ecount);
	if (count == 0)
		return (0);

	if (depth == 0) {
		struct ext4_extent *ext = EXT_FIRST_EXTENT(eh);
		uint32_t lblk = 0;
		uint32_t prev = 0;
		int len = 0;
		while (count) {
			/* leaf entries */
			if (ext4_validate_extent(ip, ext))
				return (EIO);

			/* Check for overlapping extents */
			lblk = le32toh(ext->e_blk);
			len = ext4_ext_get_actual_len(ext);
			if ((lblk <= prev) && prev)
				return (EIO);

			ext++;
			count--;
			prev = lblk + len - 1;
		}
	} else {
		struct ext4_extent_index *ext_idx = EXT_FIRST_INDEX(eh);
		while (count) {
			if (ext4_validate_extent_idx(ip, ext_idx))
				return (EIO);

			ext_idx++;
			count--;
		}
	}

	return (0);
}

static int
ext4_ext_check_header(struct inode *ip, struct ext4_extent_header *eh,
    int depth)
{
#ifdef KDTRACE_HOOKS
	char *error_msg;
#else
	char *error_msg __unused;
#endif

	if (le16toh(eh->eh_magic) != EXT4_EXT_MAGIC) {
		error_msg = "header: invalid magic";
		goto corrupted;
	}
	if (le16toh(eh->eh_depth) != depth ||
	    le16toh(eh->eh_depth) > EXT4_EXT_DEPTH_MAX)
	{
		error_msg = "header: invalid eh_depth";
		goto corrupted;
	}
	if (eh->eh_max == 0) {
		error_msg = "header: invalid eh_max";
		goto corrupted;
	}
	if (le16toh(eh->eh_max) > ext4_ext_max_entries(ip, depth)) {
		error_msg = "header: too large eh_max";
		goto corrupted;
	}
	if (le16toh(eh->eh_ecount) > le16toh(eh->eh_max)) {
		error_msg = "header: invalid eh_entries";
		goto corrupted;
	}
	if (le16toh(eh->eh_depth) > EXT4_EXT_DEPTH_MAX) {
		error_msg = "header: invalid eh_depth";
		goto corrupted;
	}
	if (ext4_validate_extent_entries(ip, eh, depth)) {
		error_msg = "header: invalid extent entries";
		goto corrupted;
	}

	return (0);

corrupted:
	SDT_PROBE2(ext2fs, , trace, extents, 1, error_msg);
	return (EIO);
}

static void
ext4_ext_binsearch_index(struct ext4_extent_path *path, int blk)
{
	struct ext4_extent_header *eh;
	struct ext4_extent_index *r, *l, *m;

	eh = path->ep_header;

	KASSERT(le16toh(eh->eh_ecount) <= le16toh(eh->eh_max) &&
	    le16toh(eh->eh_ecount) > 0,
	    ("ext4_ext_binsearch_index: bad args"));

	l = EXT_FIRST_INDEX(eh) + 1;
	r = EXT_FIRST_INDEX(eh) + le16toh(eh->eh_ecount) - 1;
	while (l <= r) {
		m = l + (r - l) / 2;
		if (blk < le32toh(m->ei_blk))
			r = m - 1;
		else
			l = m + 1;
	}

	path->ep_index = l - 1;
}

static void
ext4_ext_binsearch_ext(struct ext4_extent_path *path, int blk)
{
	struct ext4_extent_header *eh;
	struct ext4_extent *r, *l, *m;

	eh = path->ep_header;

	KASSERT(le16toh(eh->eh_ecount) <= le16toh(eh->eh_max),
	    ("ext4_ext_binsearch_ext: bad args"));

	if (eh->eh_ecount == 0)
		return;

	l = EXT_FIRST_EXTENT(eh) + 1;
	r = EXT_FIRST_EXTENT(eh) + le16toh(eh->eh_ecount) - 1;

	while (l <= r) {
		m = l + (r - l) / 2;
		if (blk < le32toh(m->e_blk))
			r = m - 1;
		else
			l = m + 1;
	}

	path->ep_ext = l - 1;
}

static int
ext4_ext_fill_path_bdata(struct ext4_extent_path *path,
    struct buf *bp, uint64_t blk)
{

	KASSERT(path->ep_data == NULL,
	    ("ext4_ext_fill_path_bdata: bad ep_data"));

	path->ep_data = malloc(bp->b_bufsize, M_EXT2EXTENTS, M_WAITOK);
	memcpy(path->ep_data, bp->b_data, bp->b_bufsize);
	path->ep_blk = blk;

	return (0);
}

static void
ext4_ext_fill_path_buf(struct ext4_extent_path *path, struct buf *bp)
{

	KASSERT(path->ep_data != NULL,
	    ("ext4_ext_fill_path_buf: bad ep_data"));

	memcpy(bp->b_data, path->ep_data, bp->b_bufsize);
}

static void
ext4_ext_drop_refs(struct ext4_extent_path *path)
{
	int depth, i;

	if (!path)
		return;

	depth = path->ep_depth;
	for (i = 0; i <= depth; i++, path++)
		if (path->ep_data) {
			free(path->ep_data, M_EXT2EXTENTS);
			path->ep_data = NULL;
		}
}

void
ext4_ext_path_free(struct ext4_extent_path *path)
{

	if (!path)
		return;

	ext4_ext_drop_refs(path);
	free(path, M_EXT2EXTENTS);
}

int
ext4_ext_find_extent(struct inode *ip, daddr_t block,
    struct ext4_extent_path **ppath)
{
	struct ext4_extent_header *eh;
	struct ext4_extent_path *path;
	struct buf *bp;
	uint64_t blk;
	int error, depth, i, ppos, alloc;

	eh = ext4_ext_inode_header(ip);
	depth = ext4_ext_inode_depth(ip);
	ppos = 0;
	alloc = 0;

	error = ext4_ext_check_header(ip, eh, depth);
	if (error)
		return (error);

	if (ppath == NULL)
		return (EINVAL);

	path = *ppath;
	if (path == NULL) {
		path = malloc(EXT4_EXT_DEPTH_MAX *
		    sizeof(struct ext4_extent_path),
		    M_EXT2EXTENTS, M_WAITOK | M_ZERO);
		*ppath = path;
		alloc = 1;
	}

	path[0].ep_header = eh;
	path[0].ep_data = NULL;

	/* Walk through the tree. */
	i = depth;
	while (i) {
		ext4_ext_binsearch_index(&path[ppos], block);
		blk = ext4_ext_index_pblock(path[ppos].ep_index);
		path[ppos].ep_depth = i;
		path[ppos].ep_ext = NULL;

		error = bread(ip->i_devvp, fsbtodb(ip->i_e2fs, blk),
		    ip->i_e2fs->e2fs_bsize, NOCRED, &bp);
		if (error) {
			goto error;
		}

		ppos++;
		if (ppos > depth) {
			SDT_PROBE2(ext2fs, , trace, extents, 1,
			    "ppos > depth => extent corrupted");
			error = EIO;
			brelse(bp);
			goto error;
		}

		ext4_ext_fill_path_bdata(&path[ppos], bp, blk);
		bqrelse(bp);

		eh = ext4_ext_block_header(path[ppos].ep_data);
		if (ext4_ext_check_header(ip, eh, i - 1) ||
		    ext2_extent_blk_csum_verify(ip, path[ppos].ep_data)) {
			error = EIO;
			goto error;
		}

		path[ppos].ep_header = eh;

		i--;
	}

	error = ext4_ext_check_header(ip, eh, 0);
	if (error)
		goto error;

	/* Find extent. */
	path[ppos].ep_depth = i;
	path[ppos].ep_header = eh;
	path[ppos].ep_ext = NULL;
	path[ppos].ep_index = NULL;
	ext4_ext_binsearch_ext(&path[ppos], block);
	return (0);

error:
	ext4_ext_drop_refs(path);
	if (alloc)
		free(path, M_EXT2EXTENTS);

	*ppath = NULL;

	return (error);
}

static inline int
ext4_ext_space_block_index(struct inode *ip)
{
	struct m_ext2fs *fs;
	int size;

	fs = ip->i_e2fs;

	size = (fs->e2fs_bsize - sizeof(struct ext4_extent_header)) /
	    sizeof(struct ext4_extent_index);

	return (size);
}

void
ext4_ext_tree_init(struct inode *ip)
{
	struct ext4_extent_header *ehp;

	ip->i_flag |= IN_E4EXTENTS;

	memset(ip->i_data, 0, sizeof(ip->i_data));
	ehp = (struct ext4_extent_header *)ip->i_data;
	ehp->eh_magic = htole16(EXT4_EXT_MAGIC);
	ehp->eh_max = htole16(ext4_ext_space_root(ip));
	ip->i_ext_cache.ec_type = EXT4_EXT_CACHE_NO;
	ip->i_flag |= IN_CHANGE | IN_UPDATE;
	ext2_update(ip->i_vnode, 1);
}

static inline void
ext4_ext_put_in_cache(struct inode *ip, uint32_t blk,
			uint32_t len, uint32_t start, int type)
{

	KASSERT(len != 0, ("ext4_ext_put_in_cache: bad input"));

	ip->i_ext_cache.ec_type = type;
	ip->i_ext_cache.ec_blk = blk;
	ip->i_ext_cache.ec_len = len;
	ip->i_ext_cache.ec_start = start;
}

static e4fs_daddr_t
ext4_ext_blkpref(struct inode *ip, struct ext4_extent_path *path,
    e4fs_daddr_t block)
{
	struct m_ext2fs *fs;
	struct ext4_extent *ex;
	e4fs_daddr_t bg_start;
	int depth;

	fs = ip->i_e2fs;

	if (path) {
		depth = path->ep_depth;
		ex = path[depth].ep_ext;
		if (ex) {
			e4fs_daddr_t pblk = ext4_ext_extent_pblock(ex);
			e2fs_daddr_t blk = le32toh(ex->e_blk);

			if (block > blk)
				return (pblk + (block - blk));
			else
				return (pblk - (blk - block));
		}

		/* Try to get block from index itself. */
		if (path[depth].ep_data)
			return (path[depth].ep_blk);
	}

	/* Use inode's group. */
	bg_start = (ip->i_block_group * EXT2_BLOCKS_PER_GROUP(ip->i_e2fs)) +
	    le32toh(fs->e2fs->e2fs_first_dblock);

	return (bg_start + block);
}

static int inline
ext4_can_extents_be_merged(struct ext4_extent *ex1,
    struct ext4_extent *ex2)
{

	if (le32toh(ex1->e_blk) + le16toh(ex1->e_len) != le32toh(ex2->e_blk))
		return (0);

	if (le16toh(ex1->e_len) + le16toh(ex2->e_len) > EXT4_MAX_LEN)
		return (0);

	if (ext4_ext_extent_pblock(ex1) + le16toh(ex1->e_len) ==
	    ext4_ext_extent_pblock(ex2))
		return (1);

	return (0);
}

static unsigned
ext4_ext_next_leaf_block(struct inode *ip, struct ext4_extent_path *path)
{
	int depth = path->ep_depth;

	/* Empty tree */
	if (depth == 0)
		return (EXT4_MAX_BLOCKS);

	/* Go to indexes. */
	depth--;

	while (depth >= 0) {
		if (path[depth].ep_index !=
		    EXT_LAST_INDEX(path[depth].ep_header))
			return (le32toh(path[depth].ep_index[1].ei_blk));

		depth--;
	}

	return (EXT4_MAX_BLOCKS);
}

static int
ext4_ext_dirty(struct inode *ip, struct ext4_extent_path *path)
{
	struct m_ext2fs *fs;
	struct buf *bp;
	uint64_t blk;
	int error;

	fs = ip->i_e2fs;

	if (!path)
		return (EINVAL);

	if (path->ep_data) {
		blk = path->ep_blk;
		bp = getblk(ip->i_devvp, fsbtodb(fs, blk),
		    fs->e2fs_bsize, 0, 0, 0);
		if (!bp)
			return (EIO);
		ext4_ext_fill_path_buf(path, bp);
		ext2_extent_blk_csum_set(ip, bp->b_data);
		error = bwrite(bp);
	} else {
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
		error = ext2_update(ip->i_vnode, 1);
	}

	return (error);
}

static int
ext4_ext_insert_index(struct inode *ip, struct ext4_extent_path *path,
    uint32_t lblk, e4fs_daddr_t blk)
{
	struct ext4_extent_index *idx;
	int len;

	if (lblk == le32toh(path->ep_index->ei_blk)) {
		SDT_PROBE2(ext2fs, , trace, extents, 1,
		    "lblk == index blk => extent corrupted");
		return (EIO);
	}

	if (le16toh(path->ep_header->eh_ecount) >=
	    le16toh(path->ep_header->eh_max)) {
		SDT_PROBE2(ext2fs, , trace, extents, 1,
		    "ecout > maxcount => extent corrupted");
		return (EIO);
	}

	if (lblk > le32toh(path->ep_index->ei_blk)) {
		/* Insert after. */
		idx = path->ep_index + 1;
	} else {
		/* Insert before. */
		idx = path->ep_index;
	}

	len = EXT_LAST_INDEX(path->ep_header) - idx + 1;
	if (len > 0)
		memmove(idx + 1, idx, len * sizeof(struct ext4_extent_index));

	if (idx > EXT_MAX_INDEX(path->ep_header)) {
		SDT_PROBE2(ext2fs, , trace, extents, 1,
		    "index is out of range => extent corrupted");
		return (EIO);
	}

	idx->ei_blk = htole32(lblk);
	ext4_index_store_pblock(idx, blk);
	path->ep_header->eh_ecount =
	    htole16(le16toh(path->ep_header->eh_ecount) + 1);

	return (ext4_ext_dirty(ip, path));
}

static e4fs_daddr_t
ext4_ext_alloc_meta(struct inode *ip)
{
	e4fs_daddr_t blk = ext2_alloc_meta(ip);
	if (blk) {
		ip->i_blocks += btodb(ip->i_e2fs->e2fs_bsize);
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
		ext2_update(ip->i_vnode, 1);
	}

	return (blk);
}

static void
ext4_ext_blkfree(struct inode *ip, uint64_t blk, int count, int flags)
{
	struct m_ext2fs *fs;
	int i, blocksreleased;

	fs = ip->i_e2fs;
	blocksreleased = count;

	for(i = 0; i < count; i++)
		ext2_blkfree(ip, blk + i, fs->e2fs_bsize);

	if (ip->i_blocks >= blocksreleased)
		ip->i_blocks -= (btodb(fs->e2fs_bsize)*blocksreleased);
	else
		ip->i_blocks = 0;

	ip->i_flag |= IN_CHANGE | IN_UPDATE;
	ext2_update(ip->i_vnode, 1);
}

static int
ext4_ext_split(struct inode *ip, struct ext4_extent_path *path,
    struct ext4_extent *newext, int at)
{
	struct m_ext2fs *fs;
	struct  buf *bp;
	int depth = ext4_ext_inode_depth(ip);
	struct ext4_extent_header *neh;
	struct ext4_extent_index *fidx;
	struct ext4_extent *ex;
	int i = at, k, m, a;
	e4fs_daddr_t newblk, oldblk;
	uint32_t border;
	e4fs_daddr_t *ablks = NULL;
	int error = 0;

	fs = ip->i_e2fs;
	bp = NULL;

	/*
	 * We will split at current extent for now.
	 */
	if (path[depth].ep_ext > EXT_MAX_EXTENT(path[depth].ep_header)) {
		SDT_PROBE2(ext2fs, , trace, extents, 1,
		    "extent is out of range => extent corrupted");
		return (EIO);
	}

	if (path[depth].ep_ext != EXT_MAX_EXTENT(path[depth].ep_header))
		border = le32toh(path[depth].ep_ext[1].e_blk);
	else
		border = le32toh(newext->e_blk);

	/* Allocate new blocks. */
	ablks = malloc(sizeof(e4fs_daddr_t) * depth,
	    M_EXT2EXTENTS, M_WAITOK | M_ZERO);
	for (a = 0; a < depth - at; a++) {
		newblk = ext4_ext_alloc_meta(ip);
		if (newblk == 0)
			goto cleanup;
		ablks[a] = newblk;
	}

	newblk = ablks[--a];
	bp = getblk(ip->i_devvp, fsbtodb(fs, newblk), fs->e2fs_bsize, 0, 0, 0);
	if (!bp) {
		error = EIO;
		goto cleanup;
	}

	neh = ext4_ext_block_header(bp->b_data);
	neh->eh_ecount = 0;
	neh->eh_max = le16toh(ext4_ext_space_block(ip));
	neh->eh_magic = le16toh(EXT4_EXT_MAGIC);
	neh->eh_depth = 0;
	ex = EXT_FIRST_EXTENT(neh);

	if (le16toh(path[depth].ep_header->eh_ecount) !=
	    le16toh(path[depth].ep_header->eh_max)) {
		SDT_PROBE2(ext2fs, , trace, extents, 1,
		    "extents count out of range => extent corrupted");
		error = EIO;
		goto cleanup;
	}

	/* Start copy from next extent. */
	m = 0;
	path[depth].ep_ext++;
	while (path[depth].ep_ext <= EXT_MAX_EXTENT(path[depth].ep_header)) {
		path[depth].ep_ext++;
		m++;
	}
	if (m) {
		memmove(ex, path[depth].ep_ext - m,
		    sizeof(struct ext4_extent) * m);
		neh->eh_ecount = htole16(le16toh(neh->eh_ecount) + m);
	}

	ext2_extent_blk_csum_set(ip, bp->b_data);
	bwrite(bp);
	bp = NULL;

	/* Fix old leaf. */
	if (m) {
		path[depth].ep_header->eh_ecount =
		    htole16(le16toh(path[depth].ep_header->eh_ecount) - m);
		ext4_ext_dirty(ip, path + depth);
	}

	/* Create intermediate indexes. */
	k = depth - at - 1;
	KASSERT(k >= 0, ("ext4_ext_split: negative k"));

	/* Insert new index into current index block. */
	i = depth - 1;
	while (k--) {
		oldblk = newblk;
		newblk = ablks[--a];
		error = bread(ip->i_devvp, fsbtodb(fs, newblk),
		    (int)fs->e2fs_bsize, NOCRED, &bp);
		if (error) {
			goto cleanup;
		}

		neh = (struct ext4_extent_header *)bp->b_data;
		neh->eh_ecount = htole16(1);
		neh->eh_magic = htole16(EXT4_EXT_MAGIC);
		neh->eh_max = htole16(ext4_ext_space_block_index(ip));
		neh->eh_depth = htole16(depth - i);
		fidx = EXT_FIRST_INDEX(neh);
		fidx->ei_blk = htole32(border);
		ext4_index_store_pblock(fidx, oldblk);

		m = 0;
		path[i].ep_index++;
		while (path[i].ep_index <= EXT_MAX_INDEX(path[i].ep_header)) {
			path[i].ep_index++;
			m++;
		}
		if (m) {
			memmove(++fidx, path[i].ep_index - m,
			    sizeof(struct ext4_extent_index) * m);
			neh->eh_ecount = htole16(le16toh(neh->eh_ecount) + m);
		}

		ext2_extent_blk_csum_set(ip, bp->b_data);
		bwrite(bp);
		bp = NULL;

		/* Fix old index. */
		if (m) {
			path[i].ep_header->eh_ecount =
			    htole16(le16toh(path[i].ep_header->eh_ecount) - m);
			ext4_ext_dirty(ip, path + i);
		}

		i--;
	}

	error = ext4_ext_insert_index(ip, path + at, border, newblk);

cleanup:
	if (bp)
		brelse(bp);

	if (error) {
		for (i = 0; i < depth; i++) {
			if (!ablks[i])
				continue;
			ext4_ext_blkfree(ip, ablks[i], 1, 0);
		}
	}

	free(ablks, M_EXT2EXTENTS);

	return (error);
}

static int
ext4_ext_grow_indepth(struct inode *ip, struct ext4_extent_path *path,
    struct ext4_extent *newext)
{
	struct m_ext2fs *fs;
	struct ext4_extent_path *curpath;
	struct ext4_extent_header *neh;
	struct buf *bp;
	e4fs_daddr_t newblk;
	int error = 0;

	fs = ip->i_e2fs;
	curpath = path;

	newblk = ext4_ext_alloc_meta(ip);
	if (newblk == 0)
		return (error);

	bp = getblk(ip->i_devvp, fsbtodb(fs, newblk), fs->e2fs_bsize, 0, 0, 0);
	if (!bp) {
		ext4_ext_blkfree(ip, newblk, 1, 0);
		return (EIO);
	}

	/* Move top-level index/leaf into new block. */
	memmove(bp->b_data, curpath->ep_header, sizeof(ip->i_data));

	/* Set size of new block */
	neh = ext4_ext_block_header(bp->b_data);
	neh->eh_magic = htole16(EXT4_EXT_MAGIC);

	if (ext4_ext_inode_depth(ip))
		neh->eh_max = htole16(ext4_ext_space_block_index(ip));
	else
		neh->eh_max = htole16(ext4_ext_space_block(ip));

	ext2_extent_blk_csum_set(ip, bp->b_data);
	error = bwrite(bp);
	if (error) {
		ext4_ext_blkfree(ip, newblk, 1, 0);
		goto out;
	}

	bp = NULL;

	curpath->ep_header->eh_magic = htole16(EXT4_EXT_MAGIC);
	curpath->ep_header->eh_max = htole16(ext4_ext_space_root(ip));
	curpath->ep_header->eh_ecount = htole16(1);
	curpath->ep_index = EXT_FIRST_INDEX(curpath->ep_header);
	curpath->ep_index->ei_blk = EXT_FIRST_EXTENT(path[0].ep_header)->e_blk;
	ext4_index_store_pblock(curpath->ep_index, newblk);

	neh = ext4_ext_inode_header(ip);
	neh->eh_depth = htole16(path->ep_depth + 1);
	ext4_ext_dirty(ip, curpath);
out:
	brelse(bp);

	return (error);
}

static int
ext4_ext_create_new_leaf(struct inode *ip, struct ext4_extent_path *path,
    struct ext4_extent *newext)
{
	struct ext4_extent_path *curpath;
	int depth, i, error;

repeat:
	i = depth = ext4_ext_inode_depth(ip);

	/* Look for free index entry int the tree */
	curpath = path + depth;
	while (i > 0 && !EXT_HAS_FREE_INDEX(curpath)) {
		i--;
		curpath--;
	}

	/*
	 * We use already allocated block for index block,
	 * so subsequent data blocks should be contiguous.
	 */
	if (EXT_HAS_FREE_INDEX(curpath)) {
		error = ext4_ext_split(ip, path, newext, i);
		if (error)
			goto out;

		/* Refill path. */
		ext4_ext_drop_refs(path);
		error = ext4_ext_find_extent(ip, le32toh(newext->e_blk), &path);
		if (error)
			goto out;
	} else {
		/* Tree is full, do grow in depth. */
		error = ext4_ext_grow_indepth(ip, path, newext);
		if (error)
			goto out;

		/* Refill path. */
		ext4_ext_drop_refs(path);
		error = ext4_ext_find_extent(ip, le32toh(newext->e_blk), &path);
		if (error)
			goto out;

		/* Check and split tree if required. */
		depth = ext4_ext_inode_depth(ip);
		if (le16toh(path[depth].ep_header->eh_ecount) ==
		    le16toh(path[depth].ep_header->eh_max))
			goto repeat;
	}

out:
	return (error);
}

static int
ext4_ext_correct_indexes(struct inode *ip, struct ext4_extent_path *path)
{
	struct ext4_extent_header *eh;
	struct ext4_extent *ex;
	int32_t border;
	int depth, k;

	depth = ext4_ext_inode_depth(ip);
	eh = path[depth].ep_header;
	ex = path[depth].ep_ext;

	if (ex == NULL || eh == NULL)
		return (EIO);

	if (!depth)
		return (0);

	/* We will correct tree if first leaf got modified only. */
	if (ex != EXT_FIRST_EXTENT(eh))
		return (0);

	k = depth - 1;
	border = le32toh(path[depth].ep_ext->e_blk);
	path[k].ep_index->ei_blk = htole32(border);
	ext4_ext_dirty(ip, path + k);
	while (k--) {
		/* Change all left-side indexes. */
		if (path[k+1].ep_index != EXT_FIRST_INDEX(path[k+1].ep_header))
			break;

		path[k].ep_index->ei_blk = htole32(border);
		ext4_ext_dirty(ip, path + k);
	}

	return (0);
}

static int
ext4_ext_insert_extent(struct inode *ip, struct ext4_extent_path *path,
    struct ext4_extent *newext)
{
	struct ext4_extent_header * eh;
	struct ext4_extent *ex, *nex, *nearex;
	struct ext4_extent_path *npath;
	int depth, len, error, next;

	depth = ext4_ext_inode_depth(ip);
	ex = path[depth].ep_ext;
	npath = NULL;

	if (htole16(newext->e_len) == 0 || path[depth].ep_header == NULL)
		return (EINVAL);

	/* Insert block into found extent. */
	if (ex && ext4_can_extents_be_merged(ex, newext)) {
		ex->e_len = htole16(le16toh(ex->e_len) + le16toh(newext->e_len));
		eh = path[depth].ep_header;
		nearex = ex;
		goto merge;
	}

repeat:
	depth = ext4_ext_inode_depth(ip);
	eh = path[depth].ep_header;
	if (le16toh(eh->eh_ecount) < le16toh(eh->eh_max))
		goto has_space;

	/* Try next leaf */
	nex = EXT_LAST_EXTENT(eh);
	next = ext4_ext_next_leaf_block(ip, path);
	if (le32toh(newext->e_blk) > le32toh(nex->e_blk) && next !=
	    EXT4_MAX_BLOCKS) {
		KASSERT(npath == NULL,
		    ("ext4_ext_insert_extent: bad path"));

		error = ext4_ext_find_extent(ip, next, &npath);
		if (error)
			goto cleanup;

		if (npath->ep_depth != path->ep_depth) {
			error = EIO;
			goto cleanup;
		}

		eh = npath[depth].ep_header;
		if (le16toh(eh->eh_ecount) < le16toh(eh->eh_max)) {
			path = npath;
			goto repeat;
		}
	}

	/*
	 * There is no free space in the found leaf,
	 * try to add a new leaf to the tree.
	 */
	error = ext4_ext_create_new_leaf(ip, path, newext);
	if (error)
		goto cleanup;

	depth = ext4_ext_inode_depth(ip);
	eh = path[depth].ep_header;

has_space:
	nearex = path[depth].ep_ext;
	if (!nearex) {
		/* Create new extent in the leaf. */
		path[depth].ep_ext = EXT_FIRST_EXTENT(eh);
	} else if (le32toh(newext->e_blk) > le32toh(nearex->e_blk)) {
		if (nearex != EXT_LAST_EXTENT(eh)) {
			len = EXT_MAX_EXTENT(eh) - nearex;
			len = (len - 1) * sizeof(struct ext4_extent);
			len = len < 0 ? 0 : len;
			memmove(nearex + 2, nearex + 1, len);
		}
		path[depth].ep_ext = nearex + 1;
	} else {
		len = (EXT_MAX_EXTENT(eh) - nearex) * sizeof(struct ext4_extent);
		len = len < 0 ? 0 : len;
		memmove(nearex + 1, nearex, len);
		path[depth].ep_ext = nearex;
	}

	eh->eh_ecount = htole16(le16toh(eh->eh_ecount) + 1);
	nearex = path[depth].ep_ext;
	nearex->e_blk = newext->e_blk;
	nearex->e_start_lo = newext->e_start_lo;
	nearex->e_start_hi = newext->e_start_hi;
	nearex->e_len = newext->e_len;

merge:
	/* Try to merge extents to the right. */
	while (nearex < EXT_LAST_EXTENT(eh)) {
		if (!ext4_can_extents_be_merged(nearex, nearex + 1))
			break;

		/* Merge with next extent. */
		nearex->e_len = htole16(le16toh(nearex->e_len) +
		    le16toh(nearex[1].e_len));
		if (nearex + 1 < EXT_LAST_EXTENT(eh)) {
			len = (EXT_LAST_EXTENT(eh) - nearex - 1) *
			    sizeof(struct ext4_extent);
			memmove(nearex + 1, nearex + 2, len);
		}

		eh->eh_ecount = htole16(le16toh(eh->eh_ecount) - 1);
		KASSERT(le16toh(eh->eh_ecount) != 0,
		    ("ext4_ext_insert_extent: bad ecount"));
	}

	/*
	 * Try to merge extents to the left,
	 * start from inexes correction.
	 */
	error = ext4_ext_correct_indexes(ip, path);
	if (error)
		goto cleanup;

	ext4_ext_dirty(ip, path + depth);

cleanup:
	if (npath) {
		ext4_ext_drop_refs(npath);
		free(npath, M_EXT2EXTENTS);
	}

	ip->i_ext_cache.ec_type = EXT4_EXT_CACHE_NO;
	return (error);
}

static e4fs_daddr_t
ext4_new_blocks(struct inode *ip, daddr_t lbn, e4fs_daddr_t pref,
    struct ucred *cred, unsigned long *count, int *perror)
{
	struct m_ext2fs *fs;
	e4fs_daddr_t newblk;

	/*
	 * We will allocate only single block for now.
	 */
	if (*count > 1)
		return (0);

	fs = ip->i_e2fs;
	EXT2_LOCK(ip->i_ump);
	*perror = ext2_alloc(ip, lbn, pref, (int)fs->e2fs_bsize, cred, &newblk);
	if (*perror)
		return (0);

	if (newblk) {
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
		ext2_update(ip->i_vnode, 1);
	}

	return (newblk);
}

int
ext4_ext_get_blocks(struct inode *ip, e4fs_daddr_t iblk,
    unsigned long max_blocks, struct ucred *cred, struct buf **bpp,
    int *pallocated, daddr_t *nb)
{
	struct m_ext2fs *fs;
	struct buf *bp = NULL;
	struct ext4_extent_path *path;
	struct ext4_extent newex, *ex;
	e4fs_daddr_t bpref, newblk = 0;
	unsigned long allocated = 0;
	int error = 0, depth;

	if(bpp)
		*bpp = NULL;
	*pallocated = 0;

	/* Check cache. */
	path = NULL;
	if ((bpref = ext4_ext_in_cache(ip, iblk, &newex))) {
		if (bpref == EXT4_EXT_CACHE_IN) {
			/* Block is already allocated. */
			newblk = iblk - le32toh(newex.e_blk) +
			    ext4_ext_extent_pblock(&newex);
			allocated = le16toh(newex.e_len) - (iblk - le32toh(newex.e_blk));
			goto out;
		} else {
			error = EIO;
			goto out2;
		}
	}

	error = ext4_ext_find_extent(ip, iblk, &path);
	if (error) {
		goto out2;
	}

	depth = ext4_ext_inode_depth(ip);
	if (path[depth].ep_ext == NULL && depth != 0) {
		error = EIO;
		goto out2;
	}

	if ((ex = path[depth].ep_ext)) {
		uint64_t lblk = le32toh(ex->e_blk);
		uint16_t e_len  = le16toh(ex->e_len);
		e4fs_daddr_t e_start = ext4_ext_extent_pblock(ex);

		if (e_len > EXT4_MAX_LEN)
			goto out2;

		/* If we found extent covers block, simply return it. */
		if (iblk >= lblk && iblk < lblk + e_len) {
			newblk = iblk - lblk + e_start;
			allocated = e_len - (iblk - lblk);
			ext4_ext_put_in_cache(ip, lblk, e_len,
			    e_start, EXT4_EXT_CACHE_IN);
			goto out;
		}
	}

	/* Allocate the new block. */
	if (S_ISREG(ip->i_mode) && (!ip->i_next_alloc_block)) {
		ip->i_next_alloc_goal = 0;
	}

	bpref = ext4_ext_blkpref(ip, path, iblk);
	allocated = max_blocks;
	newblk = ext4_new_blocks(ip, iblk, bpref, cred, &allocated, &error);
	if (!newblk)
		goto out2;

	/* Try to insert new extent into found leaf and return. */
	newex.e_blk = htole32(iblk);
	ext4_ext_store_pblock(&newex, newblk);
	newex.e_len = htole16(allocated);
	error = ext4_ext_insert_extent(ip, path, &newex);
	if (error)
		goto out2;

	newblk = ext4_ext_extent_pblock(&newex);
	ext4_ext_put_in_cache(ip, iblk, allocated, newblk, EXT4_EXT_CACHE_IN);
	*pallocated = 1;

out:
	if (allocated > max_blocks)
		allocated = max_blocks;

	if (bpp)
	{
		fs = ip->i_e2fs;
		error = bread(ip->i_devvp, fsbtodb(fs, newblk),
		    fs->e2fs_bsize, cred, &bp);
		if (error) {
			brelse(bp);
		} else {
			*bpp = bp;
		}
	}

out2:
	if (path) {
		ext4_ext_drop_refs(path);
		free(path, M_EXT2EXTENTS);
	}

	if (nb)
		*nb = newblk;

	return (error);
}

static inline struct ext4_extent_header *
ext4_ext_header(struct inode *ip)
{

	return ((struct ext4_extent_header *)ip->i_db);
}

static int
ext4_remove_blocks(struct inode *ip, struct ext4_extent *ex,
    unsigned long from, unsigned long to)
{
	unsigned long num, start;

	if (from >= le32toh(ex->e_blk) &&
	    to == le32toh(ex->e_blk) + ext4_ext_get_actual_len(ex) - 1) {
		/* Tail cleanup. */
		num = le32toh(ex->e_blk) + ext4_ext_get_actual_len(ex) - from;
		start = ext4_ext_extent_pblock(ex) +
		    ext4_ext_get_actual_len(ex) - num;
		ext4_ext_blkfree(ip, start, num, 0);
	}

	return (0);
}

static int
ext4_ext_rm_index(struct inode *ip, struct ext4_extent_path *path)
{
	e4fs_daddr_t leaf;

	/* Free index block. */
	path--;
	leaf = ext4_ext_index_pblock(path->ep_index);
	KASSERT(path->ep_header->eh_ecount != 0,
	    ("ext4_ext_rm_index: bad ecount"));
	path->ep_header->eh_ecount =
	    htole16(le16toh(path->ep_header->eh_ecount) - 1);
	ext4_ext_dirty(ip, path);
	ext4_ext_blkfree(ip, leaf, 1, 0);
	return (0);
}

static int
ext4_ext_rm_leaf(struct inode *ip, struct ext4_extent_path *path,
    uint64_t start)
{
	struct ext4_extent_header *eh;
	struct ext4_extent *ex;
	unsigned int a, b, block, num;
	unsigned long ex_blk;
	unsigned short ex_len;
	int depth;
	int error, correct_index;

	depth = ext4_ext_inode_depth(ip);
	if (!path[depth].ep_header) {
		if (path[depth].ep_data == NULL)
			return (EINVAL);
		path[depth].ep_header =
		    (struct ext4_extent_header* )path[depth].ep_data;
	}

	eh = path[depth].ep_header;
	if (!eh) {
		SDT_PROBE2(ext2fs, , trace, extents, 1,
		    "bad header => extent corrupted");
		return (EIO);
	}

	ex = EXT_LAST_EXTENT(eh);
	ex_blk = le32toh(ex->e_blk);
	ex_len = ext4_ext_get_actual_len(ex);

	error = 0;
	correct_index = 0;
	while (ex >= EXT_FIRST_EXTENT(eh) && ex_blk + ex_len > start) {
		path[depth].ep_ext = ex;
		a = ex_blk > start ? ex_blk : start;
		b = (uint64_t)ex_blk + ex_len - 1 <
		    EXT4_MAX_BLOCKS ? ex_blk + ex_len - 1 : EXT4_MAX_BLOCKS;

		if (a != ex_blk && b != ex_blk + ex_len - 1)
			return (EINVAL);
		else if (a != ex_blk) {
			/* Remove tail of the extent. */
			block = ex_blk;
			num = a - block;
		} else if (b != ex_blk + ex_len - 1) {
			/* Remove head of the extent, not implemented. */
			return (EINVAL);
		} else {
			/* Remove whole extent. */
			block = ex_blk;
			num = 0;
		}

		if (ex == EXT_FIRST_EXTENT(eh))
			correct_index = 1;

		error = ext4_remove_blocks(ip, ex, a, b);
		if (error)
			goto out;

		if (num == 0) {
			ext4_ext_store_pblock(ex, 0);
			eh->eh_ecount = htole16(le16toh(eh->eh_ecount) - 1);
		}

		ex->e_blk = htole32(block);
		ex->e_len = htole16(num);

		ext4_ext_dirty(ip, path + depth);

		ex--;
		ex_blk = htole32(ex->e_blk);
		ex_len = ext4_ext_get_actual_len(ex);
	};

	if (correct_index && le16toh(eh->eh_ecount))
		error = ext4_ext_correct_indexes(ip, path);

	/*
	 * If this leaf is free, we should
	 * remove it from index block above.
	 */
	if (error == 0 && eh->eh_ecount == 0 &&
	    path[depth].ep_data != NULL)
		error = ext4_ext_rm_index(ip, path + depth);

out:
	return (error);
}

static struct buf *
ext4_read_extent_tree_block(struct inode *ip, e4fs_daddr_t pblk,
    int depth, int flags)
{
	struct m_ext2fs *fs;
	struct ext4_extent_header *eh;
	struct buf *bp;
	int error;

	fs = ip->i_e2fs;
	error = bread(ip->i_devvp, fsbtodb(fs, pblk),
	    fs->e2fs_bsize, NOCRED, &bp);
	if (error) {
		return (NULL);
	}

	eh = ext4_ext_block_header(bp->b_data);
	if (le16toh(eh->eh_depth) != depth) {
		SDT_PROBE2(ext2fs, , trace, extents, 1,
		    "unexpected eh_depth");
		goto err;
	}

	error = ext4_ext_check_header(ip, eh, depth);
	if (error)
		goto err;

	return (bp);

err:
	brelse(bp);
	return (NULL);

}

static int inline
ext4_ext_more_to_rm(struct ext4_extent_path *path)
{

	KASSERT(path->ep_index != NULL,
	    ("ext4_ext_more_to_rm: bad index from path"));

	if (path->ep_index < EXT_FIRST_INDEX(path->ep_header))
		return (0);

	if (le16toh(path->ep_header->eh_ecount) == path->index_count)
		return (0);

	return (1);
}

int
ext4_ext_remove_space(struct inode *ip, off_t length, int flags,
    struct ucred *cred, struct thread *td)
{
	struct buf *bp;
	struct ext4_extent_header *ehp;
	struct ext4_extent_path *path;
	int depth;
	int i, error;

	ehp = (struct ext4_extent_header *)ip->i_db;
	depth = ext4_ext_inode_depth(ip);

	error = ext4_ext_check_header(ip, ehp, depth);
	if(error)
		return (error);

	path = malloc(sizeof(struct ext4_extent_path) * (depth + 1),
	    M_EXT2EXTENTS, M_WAITOK | M_ZERO);
	path[0].ep_header = ehp;
	path[0].ep_depth = depth;
	i = 0;
	while (error == 0 && i >= 0) {
		if (i == depth) {
			/* This is leaf. */
			error = ext4_ext_rm_leaf(ip, path, length);
			if (error)
				break;
			free(path[i].ep_data, M_EXT2EXTENTS);
			path[i].ep_data = NULL;
			i--;
			continue;
		}

		/* This is index. */
		if (!path[i].ep_header)
			path[i].ep_header =
			    (struct ext4_extent_header *)path[i].ep_data;

		if (!path[i].ep_index) {
			/* This level hasn't touched yet. */
			path[i].ep_index = EXT_LAST_INDEX(path[i].ep_header);
			path[i].index_count =
			    le16toh(path[i].ep_header->eh_ecount) + 1;
		} else {
			/* We've already was here, see at next index. */
			path[i].ep_index--;
		}

		if (ext4_ext_more_to_rm(path + i)) {
			memset(path + i + 1, 0, sizeof(*path));
			bp = ext4_read_extent_tree_block(ip,
			    ext4_ext_index_pblock(path[i].ep_index),
			    path[0].ep_depth - (i + 1), 0);
			if (!bp) {
				error = EIO;
				break;
			}

			ext4_ext_fill_path_bdata(&path[i+1], bp,
			    ext4_ext_index_pblock(path[i].ep_index));
			brelse(bp);
			path[i].index_count =
			    le16toh(path[i].ep_header->eh_ecount);
			i++;
		} else {
			if (path[i].ep_header->eh_ecount == 0 && i > 0) {
				/* Index is empty, remove it. */
				error = ext4_ext_rm_index(ip, path + i);
			}
			free(path[i].ep_data, M_EXT2EXTENTS);
			path[i].ep_data = NULL;
			i--;
		}
	}

	if (path->ep_header->eh_ecount == 0) {
		/*
		 * Truncate the tree to zero.
		 */
		 ext4_ext_header(ip)->eh_depth = 0;
		 ext4_ext_header(ip)->eh_max = htole16(ext4_ext_space_root(ip));
		 ext4_ext_dirty(ip, path);
	}

	ext4_ext_drop_refs(path);
	free(path, M_EXT2EXTENTS);

	ip->i_ext_cache.ec_type = EXT4_EXT_CACHE_NO;
	return (error);
}
