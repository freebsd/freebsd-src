/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2017, Fedor Uporov
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
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/sdt.h>
#include <sys/stat.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/vnode.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/endian.h>
#include <sys/conf.h>
#include <sys/gsb_crc32.h>
#include <sys/crc16.h>
#include <sys/mount.h>

#include <fs/ext2fs/fs.h>
#include <fs/ext2fs/ext2fs.h>
#include <fs/ext2fs/ext2_dinode.h>
#include <fs/ext2fs/inode.h>
#include <fs/ext2fs/ext2_dir.h>
#include <fs/ext2fs/htree.h>
#include <fs/ext2fs/ext2_extattr.h>
#include <fs/ext2fs/ext2_extern.h>

SDT_PROVIDER_DECLARE(ext2fs);
/*
 * ext2fs trace probe:
 * arg0: verbosity. Higher numbers give more verbose messages
 * arg1: Textual message
 */
SDT_PROBE_DEFINE2(ext2fs, , trace, csum, "int", "char*");

#define EXT2_BG_INODE_BITMAP_CSUM_HI_END	\
	(offsetof(struct ext2_gd, ext4bgd_i_bmap_csum_hi) + \
	 sizeof(uint16_t))

#define EXT2_INODE_CSUM_HI_EXTRA_END	\
	(offsetof(struct ext2fs_dinode, e2di_chksum_hi) + sizeof(uint16_t) - \
	 E2FS_REV0_INODE_SIZE)

#define EXT2_BG_BLOCK_BITMAP_CSUM_HI_LOCATION	\
	(offsetof(struct ext2_gd, ext4bgd_b_bmap_csum_hi) + \
	 sizeof(uint16_t))

void
ext2_sb_csum_set_seed(struct m_ext2fs *fs)
{

	if (EXT2_HAS_INCOMPAT_FEATURE(fs, EXT2F_INCOMPAT_CSUM_SEED))
		fs->e2fs_csum_seed = le32toh(fs->e2fs->e4fs_chksum_seed);
	else if (EXT2_HAS_RO_COMPAT_FEATURE(fs, EXT2F_ROCOMPAT_METADATA_CKSUM)) {
		fs->e2fs_csum_seed = calculate_crc32c(~0, fs->e2fs->e2fs_uuid,
		    sizeof(fs->e2fs->e2fs_uuid));
	}
	else
		fs->e2fs_csum_seed = 0;
}

int
ext2_sb_csum_verify(struct m_ext2fs *fs)
{

	if (fs->e2fs->e4fs_chksum_type != EXT4_CRC32C_CHKSUM) {
		printf(
"WARNING: mount of %s denied due bad sb csum type\n", fs->e2fs_fsmnt);
		return (EINVAL);
	}
	if (le32toh(fs->e2fs->e4fs_sbchksum) !=
	    calculate_crc32c(~0, (const char *)fs->e2fs,
	    offsetof(struct ext2fs, e4fs_sbchksum))) {
		printf(
"WARNING: mount of %s denied due bad sb csum=0x%x, expected=0x%x - run fsck\n",
		    fs->e2fs_fsmnt, le32toh(fs->e2fs->e4fs_sbchksum),
		    calculate_crc32c(~0, (const char *)fs->e2fs,
		    offsetof(struct ext2fs, e4fs_sbchksum)));
		return (EINVAL);
	}

	return (0);
}

void
ext2_sb_csum_set(struct m_ext2fs *fs)
{

	fs->e2fs->e4fs_sbchksum =
	    htole32(calculate_crc32c(~0, (const char *)fs->e2fs,
	    offsetof(struct ext2fs, e4fs_sbchksum)));
}

static uint32_t
ext2_extattr_blk_csum(struct inode *ip, uint64_t facl,
    struct ext2fs_extattr_header *header)
{
	struct m_ext2fs *fs;
	uint32_t crc, dummy_crc = 0;
	uint64_t facl_bn = htole64(facl);
	int offset = offsetof(struct ext2fs_extattr_header, h_checksum);

	fs = ip->i_e2fs;

	crc = calculate_crc32c(fs->e2fs_csum_seed, (uint8_t *)&facl_bn,
	    sizeof(facl_bn));
	crc = calculate_crc32c(crc, (uint8_t *)header, offset);
	crc = calculate_crc32c(crc, (uint8_t *)&dummy_crc,
	    sizeof(dummy_crc));
	offset += sizeof(dummy_crc);
	crc = calculate_crc32c(crc, (uint8_t *)header + offset,
	    fs->e2fs_bsize - offset);

	return (htole32(crc));
}

int
ext2_extattr_blk_csum_verify(struct inode *ip, struct buf *bp)
{
	struct ext2fs_extattr_header *header;

	header = (struct ext2fs_extattr_header *)bp->b_data;

	if (EXT2_HAS_RO_COMPAT_FEATURE(ip->i_e2fs, EXT2F_ROCOMPAT_METADATA_CKSUM) &&
	    (header->h_checksum != ext2_extattr_blk_csum(ip, ip->i_facl, header))) {
		SDT_PROBE2(ext2fs, , trace, csum, 1, "bad extattr csum detected");
		return (EIO);
	}

	return (0);
}

void
ext2_extattr_blk_csum_set(struct inode *ip, struct buf *bp)
{
	struct ext2fs_extattr_header *header;

	if (!EXT2_HAS_RO_COMPAT_FEATURE(ip->i_e2fs, EXT2F_ROCOMPAT_METADATA_CKSUM))
		return;

	header = (struct ext2fs_extattr_header *)bp->b_data;
	header->h_checksum = ext2_extattr_blk_csum(ip, ip->i_facl, header);
}

void
ext2_init_dirent_tail(struct ext2fs_direct_tail *tp)
{
	memset(tp, 0, sizeof(struct ext2fs_direct_tail));
	tp->e2dt_rec_len = le16toh(sizeof(struct ext2fs_direct_tail));
	tp->e2dt_reserved_ft = EXT2_FT_DIR_CSUM;
}

int
ext2_is_dirent_tail(struct inode *ip, struct ext2fs_direct_2 *ep)
{
	struct m_ext2fs *fs;
	struct ext2fs_direct_tail *tp;

	fs = ip->i_e2fs;

	if (!EXT2_HAS_RO_COMPAT_FEATURE(fs, EXT2F_ROCOMPAT_METADATA_CKSUM))
		return (0);

	tp = (struct ext2fs_direct_tail *)ep;
	if (tp->e2dt_reserved_zero1 == 0 &&
	    le16toh(tp->e2dt_rec_len) == sizeof(struct ext2fs_direct_tail) &&
	    tp->e2dt_reserved_zero2 == 0 &&
	    tp->e2dt_reserved_ft == EXT2_FT_DIR_CSUM)
		return (1);

	return (0);
}

struct ext2fs_direct_tail *
ext2_dirent_get_tail(struct inode *ip, struct ext2fs_direct_2 *ep)
{
	struct ext2fs_direct_2 *dep;
	void *top;
	unsigned int rec_len;

	dep = ep;
	top = EXT2_DIRENT_TAIL(ep, ip->i_e2fs->e2fs_bsize);
	rec_len = le16toh(dep->e2d_reclen);

	while (rec_len && !(rec_len & 0x3)) {
		dep = (struct ext2fs_direct_2 *)(((char *)dep) + rec_len);
		if ((void *)dep >= top)
			break;
		rec_len = le16toh(dep->e2d_reclen);
	}

	if (dep != top)
		return (NULL);

	if (ext2_is_dirent_tail(ip, dep))
		return ((struct ext2fs_direct_tail *)dep);

	return (NULL);
}

static uint32_t
ext2_dirent_csum(struct inode *ip, struct ext2fs_direct_2 *ep, int size)
{
	struct m_ext2fs *fs;
	char *buf;
	uint32_t inum, gen, crc;

	fs = ip->i_e2fs;

	buf = (char *)ep;

	inum = htole32(ip->i_number);
	gen = htole32(ip->i_gen);
	crc = calculate_crc32c(fs->e2fs_csum_seed, (uint8_t *)&inum, sizeof(inum));
	crc = calculate_crc32c(crc, (uint8_t *)&gen, sizeof(gen));
	crc = calculate_crc32c(crc, (uint8_t *)buf, size);

	return (crc);
}

int
ext2_dirent_csum_verify(struct inode *ip, struct ext2fs_direct_2 *ep)
{
	uint32_t calculated;
	struct ext2fs_direct_tail *tp;

	tp = ext2_dirent_get_tail(ip, ep);
	if (tp == NULL)
		return (0);

	calculated = ext2_dirent_csum(ip, ep, (char *)tp - (char *)ep);
	if (calculated != le32toh(tp->e2dt_checksum))
		return (EIO);

	return (0);
}

static struct ext2fs_htree_count *
ext2_get_dx_count(struct inode *ip, struct ext2fs_direct_2 *ep, int *offset)
{
	struct ext2fs_direct_2 *dp;
	struct ext2fs_htree_root_info *root;
	int count_offset;

	if (le16toh(ep->e2d_reclen) == EXT2_BLOCK_SIZE(ip->i_e2fs))
		count_offset = 8;
	else if (le16toh(ep->e2d_reclen) == 12) {
		dp = (struct ext2fs_direct_2 *)(((char *)ep) + 12);
		if (le16toh(dp->e2d_reclen) != EXT2_BLOCK_SIZE(ip->i_e2fs) - 12)
			return (NULL);

		root = (struct ext2fs_htree_root_info *)(((char *)dp + 12));
		if (root->h_reserved1 ||
		    root->h_info_len != sizeof(struct ext2fs_htree_root_info))
			return (NULL);

		count_offset = 32;
	} else
		return (NULL);

	if (offset)
		*offset = count_offset;

	return ((struct ext2fs_htree_count *)(((char *)ep) + count_offset));
}

static uint32_t
ext2_dx_csum(struct inode *ip, struct ext2fs_direct_2 *ep, int count_offset,
    int count, struct ext2fs_htree_tail *tp)
{
	struct m_ext2fs *fs;
	char *buf;
	int size;
	uint32_t inum, old_csum, gen, crc;

	fs = ip->i_e2fs;

	buf = (char *)ep;

	size = count_offset + (count * sizeof(struct ext2fs_htree_entry));
	old_csum = tp->ht_checksum;
	tp->ht_checksum = 0;

	inum = htole32(ip->i_number);
	gen = htole32(ip->i_gen);
	crc = calculate_crc32c(fs->e2fs_csum_seed, (uint8_t *)&inum, sizeof(inum));
	crc = calculate_crc32c(crc, (uint8_t *)&gen, sizeof(gen));
	crc = calculate_crc32c(crc, (uint8_t *)buf, size);
	crc = calculate_crc32c(crc, (uint8_t *)tp, sizeof(struct ext2fs_htree_tail));
	tp->ht_checksum = old_csum;

	return htole32(crc);
}

int
ext2_dx_csum_verify(struct inode *ip, struct ext2fs_direct_2 *ep)
{
	uint32_t calculated;
	struct ext2fs_htree_count *cp;
	struct ext2fs_htree_tail *tp;
	int count_offset, limit, count;

	cp = ext2_get_dx_count(ip, ep, &count_offset);
	if (cp == NULL)
		return (0);

	limit = le16toh(cp->h_entries_max);
	count = le16toh(cp->h_entries_num);
	if (count_offset + (limit * sizeof(struct ext2fs_htree_entry)) >
	    ip->i_e2fs->e2fs_bsize - sizeof(struct ext2fs_htree_tail))
		return (EIO);

	tp = (struct ext2fs_htree_tail *)(((struct ext2fs_htree_entry *)cp) + limit);
	calculated = ext2_dx_csum(ip, ep,  count_offset, count, tp);

	if (tp->ht_checksum != calculated)
		return (EIO);

	return (0);
}

int
ext2_dir_blk_csum_verify(struct inode *ip, struct buf *bp)
{
	struct m_ext2fs *fs;
	struct ext2fs_direct_2 *ep;
	int error = 0;

	fs = ip->i_e2fs;

	if (!EXT2_HAS_RO_COMPAT_FEATURE(fs, EXT2F_ROCOMPAT_METADATA_CKSUM))
		return (error);

	ep = (struct ext2fs_direct_2 *)bp->b_data;

	if (ext2_dirent_get_tail(ip, ep) != NULL)
		error = ext2_dirent_csum_verify(ip, ep);
	else if (ext2_get_dx_count(ip, ep, NULL) != NULL)
		error = ext2_dx_csum_verify(ip, ep);

	if (error)
		SDT_PROBE2(ext2fs, , trace, csum, 1, "bad directory csum detected");

	return (error);
}

void
ext2_dirent_csum_set(struct inode *ip, struct ext2fs_direct_2 *ep)
{
	struct m_ext2fs *fs;
	struct ext2fs_direct_tail *tp;

	fs = ip->i_e2fs;

	if (!EXT2_HAS_RO_COMPAT_FEATURE(fs, EXT2F_ROCOMPAT_METADATA_CKSUM))
		return;

	tp = ext2_dirent_get_tail(ip, ep);
	if (tp == NULL)
		return;

	tp->e2dt_checksum =
	    htole32(ext2_dirent_csum(ip, ep, (char *)tp - (char *)ep));
}

void
ext2_dx_csum_set(struct inode *ip, struct ext2fs_direct_2 *ep)
{
	struct m_ext2fs *fs;
	struct ext2fs_htree_count *cp;
	struct ext2fs_htree_tail *tp;
	int count_offset, limit, count;

	fs = ip->i_e2fs;

	if (!EXT2_HAS_RO_COMPAT_FEATURE(fs, EXT2F_ROCOMPAT_METADATA_CKSUM))
		return;

	cp = ext2_get_dx_count(ip, ep, &count_offset);
	if (cp == NULL)
		return;

	limit = le16toh(cp->h_entries_max);
	count = le16toh(cp->h_entries_num);
	if (count_offset + (limit * sizeof(struct ext2fs_htree_entry)) >
	    ip->i_e2fs->e2fs_bsize - sizeof(struct ext2fs_htree_tail))
		return;

	tp = (struct ext2fs_htree_tail *)(((struct ext2fs_htree_entry *)cp) + limit);
	tp->ht_checksum = ext2_dx_csum(ip, ep,  count_offset, count, tp);
}

static uint32_t
ext2_extent_blk_csum(struct inode *ip, struct ext4_extent_header *ehp)
{
	struct m_ext2fs *fs;
	size_t size;
	uint32_t inum, gen, crc;

	fs = ip->i_e2fs;

	size = EXT4_EXTENT_TAIL_OFFSET(ehp) +
	    offsetof(struct ext4_extent_tail, et_checksum);

	inum = htole32(ip->i_number);
	gen = htole32(ip->i_gen);
	crc = calculate_crc32c(fs->e2fs_csum_seed, (uint8_t *)&inum, sizeof(inum));
	crc = calculate_crc32c(crc, (uint8_t *)&gen, sizeof(gen));
	crc = calculate_crc32c(crc, (uint8_t *)ehp, size);

	return (crc);
}

int
ext2_extent_blk_csum_verify(struct inode *ip, void *data)
{
	struct m_ext2fs *fs;
	struct ext4_extent_header *ehp;
	struct ext4_extent_tail *etp;
	uint32_t provided, calculated;

	fs = ip->i_e2fs;

	if (!EXT2_HAS_RO_COMPAT_FEATURE(fs, EXT2F_ROCOMPAT_METADATA_CKSUM))
		return (0);

	ehp = (struct ext4_extent_header *)data;
	etp = (struct ext4_extent_tail *)(((char *)ehp) +
	    EXT4_EXTENT_TAIL_OFFSET(ehp));

	provided = le32toh(etp->et_checksum);
	calculated = ext2_extent_blk_csum(ip, ehp);

	if (provided != calculated) {
		SDT_PROBE2(ext2fs, , trace, csum, 1, "bad extent csum detected");
		return (EIO);
	}

	return (0);
}

void
ext2_extent_blk_csum_set(struct inode *ip, void *data)
{
	struct m_ext2fs *fs;
	struct ext4_extent_header *ehp;
	struct ext4_extent_tail *etp;

	fs = ip->i_e2fs;

	if (!EXT2_HAS_RO_COMPAT_FEATURE(fs, EXT2F_ROCOMPAT_METADATA_CKSUM))
		return;

	ehp = (struct ext4_extent_header *)data;
	etp = (struct ext4_extent_tail *)(((char *)data) +
	    EXT4_EXTENT_TAIL_OFFSET(ehp));

	etp->et_checksum = htole32(ext2_extent_blk_csum(ip,
	    (struct ext4_extent_header *)data));
}

int
ext2_gd_i_bitmap_csum_verify(struct m_ext2fs *fs, int cg, struct buf *bp)
{
	uint32_t hi, provided, calculated;

	if (!EXT2_HAS_RO_COMPAT_FEATURE(fs, EXT2F_ROCOMPAT_METADATA_CKSUM))
		return (0);

	provided = le16toh(fs->e2fs_gd[cg].ext4bgd_i_bmap_csum);
	calculated = calculate_crc32c(fs->e2fs_csum_seed, bp->b_data,
	    fs->e2fs_ipg / 8);
	if (le16toh(fs->e2fs->e3fs_desc_size) >=
	    EXT2_BG_INODE_BITMAP_CSUM_HI_END) {
		hi = le16toh(fs->e2fs_gd[cg].ext4bgd_i_bmap_csum_hi);
		provided |= (hi << 16);
	} else
		calculated &= 0xFFFF;

	if (provided != calculated) {
		SDT_PROBE2(ext2fs, , trace, csum, 1, "bad inode bitmap csum detected");
		return (EIO);
	}

	return (0);
}

void
ext2_gd_i_bitmap_csum_set(struct m_ext2fs *fs, int cg, struct buf *bp)
{
	uint32_t csum;

	if (!EXT2_HAS_RO_COMPAT_FEATURE(fs, EXT2F_ROCOMPAT_METADATA_CKSUM))
		return;

	csum = calculate_crc32c(fs->e2fs_csum_seed, bp->b_data,
	    fs->e2fs_ipg / 8);
	fs->e2fs_gd[cg].ext4bgd_i_bmap_csum = htole16(csum & 0xFFFF);
	if (le16toh(fs->e2fs->e3fs_desc_size) >= EXT2_BG_INODE_BITMAP_CSUM_HI_END)
		fs->e2fs_gd[cg].ext4bgd_i_bmap_csum_hi = htole16(csum >> 16);
}

int
ext2_gd_b_bitmap_csum_verify(struct m_ext2fs *fs, int cg, struct buf *bp)
{
	uint32_t hi, provided, calculated, size;

	if (!EXT2_HAS_RO_COMPAT_FEATURE(fs, EXT2F_ROCOMPAT_METADATA_CKSUM))
		return (0);

	size = fs->e2fs_fpg / 8;
	provided = le16toh(fs->e2fs_gd[cg].ext4bgd_b_bmap_csum);
	calculated = calculate_crc32c(fs->e2fs_csum_seed, bp->b_data, size);
	if (le16toh(fs->e2fs->e3fs_desc_size) >=
	    EXT2_BG_BLOCK_BITMAP_CSUM_HI_LOCATION) {
		hi = le16toh(fs->e2fs_gd[cg].ext4bgd_b_bmap_csum_hi);
		provided |= (hi << 16);
	} else
		calculated &= 0xFFFF;

	if (provided != calculated) {
		SDT_PROBE2(ext2fs, , trace, csum, 1, "bad block bitmap csum detected");
		return (EIO);
	}

	return (0);
}

void
ext2_gd_b_bitmap_csum_set(struct m_ext2fs *fs, int cg, struct buf *bp)
{
	uint32_t csum, size;

	if (!EXT2_HAS_RO_COMPAT_FEATURE(fs, EXT2F_ROCOMPAT_METADATA_CKSUM))
		return;

	size = fs->e2fs_fpg / 8;
	csum = calculate_crc32c(fs->e2fs_csum_seed, bp->b_data, size);
	fs->e2fs_gd[cg].ext4bgd_b_bmap_csum = htole16(csum & 0xFFFF);
	if (le16toh(fs->e2fs->e3fs_desc_size) >= EXT2_BG_BLOCK_BITMAP_CSUM_HI_LOCATION)
		fs->e2fs_gd[cg].ext4bgd_b_bmap_csum_hi = htole16(csum >> 16);
}

static uint32_t
ext2_ei_csum(struct inode *ip, struct ext2fs_dinode *ei)
{
	struct m_ext2fs *fs;
	uint32_t inode_csum_seed, inum, gen, crc;
	uint16_t dummy_csum = 0;
	unsigned int offset, csum_size;

	fs = ip->i_e2fs;
	offset = offsetof(struct ext2fs_dinode, e2di_chksum_lo);
	csum_size = sizeof(dummy_csum);
	inum = htole32(ip->i_number);
	crc = calculate_crc32c(fs->e2fs_csum_seed,
	    (uint8_t *)&inum, sizeof(inum));
	gen = htole32(ip->i_gen);
	inode_csum_seed = calculate_crc32c(crc,
	    (uint8_t *)&gen, sizeof(gen));

	crc = calculate_crc32c(inode_csum_seed, (uint8_t *)ei, offset);
	crc = calculate_crc32c(crc, (uint8_t *)&dummy_csum, csum_size);
	offset += csum_size;
	crc = calculate_crc32c(crc, (uint8_t *)ei + offset,
	    E2FS_REV0_INODE_SIZE - offset);

	if (EXT2_INODE_SIZE(fs) > E2FS_REV0_INODE_SIZE) {
		offset = offsetof(struct ext2fs_dinode, e2di_chksum_hi);
		crc = calculate_crc32c(crc, (uint8_t *)ei +
		    E2FS_REV0_INODE_SIZE, offset - E2FS_REV0_INODE_SIZE);

		if ((EXT2_INODE_SIZE(ip->i_e2fs) > E2FS_REV0_INODE_SIZE &&
		    le16toh(ei->e2di_extra_isize) >=
		    EXT2_INODE_CSUM_HI_EXTRA_END)) {
			crc = calculate_crc32c(crc, (uint8_t *)&dummy_csum,
			    csum_size);
			offset += csum_size;
		}

		crc = calculate_crc32c(crc, (uint8_t *)ei + offset,
		    EXT2_INODE_SIZE(fs) - offset);
	}

	return (crc);
}

int
ext2_ei_csum_verify(struct inode *ip, struct ext2fs_dinode *ei)
{
	struct m_ext2fs *fs;
	const static struct ext2fs_dinode ei_zero;
	uint32_t hi, provided, calculated;

	fs = ip->i_e2fs;

	if (!EXT2_HAS_RO_COMPAT_FEATURE(fs, EXT2F_ROCOMPAT_METADATA_CKSUM))
		return (0);

	provided = le16toh(ei->e2di_chksum_lo);
	calculated = ext2_ei_csum(ip, ei);

	if ((EXT2_INODE_SIZE(fs) > E2FS_REV0_INODE_SIZE &&
	    le16toh(ei->e2di_extra_isize) >= EXT2_INODE_CSUM_HI_EXTRA_END)) {
		hi = le16toh(ei->e2di_chksum_hi);
		provided |= hi << 16;
	} else
		calculated &= 0xFFFF;

	if (provided != calculated) {
		/*
		 * If it is first time used dinode,
		 * it is expected that it will be zeroed
		 * and we will not return checksum error in this case.
		 */
		if (!memcmp(ei, &ei_zero, sizeof(struct ext2fs_dinode)))
			return (0);

		SDT_PROBE2(ext2fs, , trace, csum, 1, "bad inode csum");

		return (EIO);
	}

	return (0);
}

void
ext2_ei_csum_set(struct inode *ip, struct ext2fs_dinode *ei)
{
	struct m_ext2fs *fs;
	uint32_t crc;

	fs = ip->i_e2fs;

	if (!EXT2_HAS_RO_COMPAT_FEATURE(fs, EXT2F_ROCOMPAT_METADATA_CKSUM))
		return;

	crc = ext2_ei_csum(ip, ei);

	ei->e2di_chksum_lo = htole16(crc & 0xFFFF);
	if ((EXT2_INODE_SIZE(fs) > E2FS_REV0_INODE_SIZE &&
	    le16toh(ei->e2di_extra_isize) >= EXT2_INODE_CSUM_HI_EXTRA_END))
		ei->e2di_chksum_hi = htole16(crc >> 16);
}

static uint16_t
ext2_gd_csum(struct m_ext2fs *fs, uint32_t block_group, struct ext2_gd *gd)
{
	size_t offset;
	uint32_t csum32;
	uint16_t crc, dummy_csum;

	offset = offsetof(struct ext2_gd, ext4bgd_csum);

	block_group = htole32(block_group);

	if (EXT2_HAS_RO_COMPAT_FEATURE(fs, EXT2F_ROCOMPAT_METADATA_CKSUM)) {
		csum32 = calculate_crc32c(fs->e2fs_csum_seed,
		    (uint8_t *)&block_group, sizeof(block_group));
		csum32 = calculate_crc32c(csum32, (uint8_t *)gd, offset);
		dummy_csum = 0;
		csum32 = calculate_crc32c(csum32, (uint8_t *)&dummy_csum,
		    sizeof(dummy_csum));
		offset += sizeof(dummy_csum);
		if (offset < le16toh(fs->e2fs->e3fs_desc_size))
			csum32 = calculate_crc32c(csum32, (uint8_t *)gd + offset,
			    le16toh(fs->e2fs->e3fs_desc_size) - offset);

		crc = csum32 & 0xFFFF;
		return (htole16(crc));
	} else if (EXT2_HAS_RO_COMPAT_FEATURE(fs, EXT2F_ROCOMPAT_GDT_CSUM)) {
		crc = crc16(~0, fs->e2fs->e2fs_uuid,
		    sizeof(fs->e2fs->e2fs_uuid));
		crc = crc16(crc, (uint8_t *)&block_group,
		    sizeof(block_group));
		crc = crc16(crc, (uint8_t *)gd, offset);
		offset += sizeof(gd->ext4bgd_csum); /* skip checksum */
		if (EXT2_HAS_INCOMPAT_FEATURE(fs, EXT2F_INCOMPAT_64BIT) &&
		    offset < le16toh(fs->e2fs->e3fs_desc_size))
			crc = crc16(crc, (uint8_t *)gd + offset,
			    le16toh(fs->e2fs->e3fs_desc_size) - offset);
		return (htole16(crc));
	}

	return (0);
}

int
ext2_gd_csum_verify(struct m_ext2fs *fs, struct cdev *dev)
{
	unsigned int i;
	int error = 0;

	for (i = 0; i < fs->e2fs_gcount; i++) {
		if (fs->e2fs_gd[i].ext4bgd_csum !=
		    ext2_gd_csum(fs, i, &fs->e2fs_gd[i])) {
			printf(
"WARNING: mount of %s denied due bad gd=%d csum=0x%x, expected=0x%x - run fsck\n",
			    devtoname(dev), i, fs->e2fs_gd[i].ext4bgd_csum,
			    ext2_gd_csum(fs, i, &fs->e2fs_gd[i]));
			error = EIO;
			break;
		}
	}

	return (error);
}

void
ext2_gd_csum_set(struct m_ext2fs *fs)
{
	unsigned int i;

	for (i = 0; i < fs->e2fs_gcount; i++)
		fs->e2fs_gd[i].ext4bgd_csum = ext2_gd_csum(fs, i, &fs->e2fs_gd[i]);
}
