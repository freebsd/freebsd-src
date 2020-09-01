/*-
 * SPDX-License-Identifier: MIT-CMU
 *
 * Copyright (c) 1995 The University of Utah and
 * the Computer Systems Laboratory at the University of Utah (CSL).
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software is hereby
 * granted provided that (1) source code retains these copyright, permission,
 * and disclaimer notices, and (2) redistributions including binaries
 * reproduce the notices in supporting documentation, and (3) all advertising
 * materials mentioning features or use of this software display the following
 * acknowledgement: ``This product includes software developed by the
 * Computer Systems Laboratory at the University of Utah.''
 *
 * THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS
 * IS" CONDITION.  THE UNIVERSITY OF UTAH AND CSL DISCLAIM ANY LIABILITY OF
 * ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 *
 *      Utah $Hdr$
 * $FreeBSD$
 */

/*
 * routines to convert on disk ext2 inodes into inodes and back
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/endian.h>
#include <sys/lock.h>
#include <sys/sdt.h>
#include <sys/stat.h>
#include <sys/vnode.h>

#include <fs/ext2fs/fs.h>
#include <fs/ext2fs/inode.h>
#include <fs/ext2fs/ext2fs.h>
#include <fs/ext2fs/ext2_dinode.h>
#include <fs/ext2fs/ext2_extern.h>

SDT_PROVIDER_DECLARE(ext2fs);
/*
 * ext2fs trace probe:
 * arg0: verbosity. Higher numbers give more verbose messages
 * arg1: Textual message
 */
SDT_PROBE_DEFINE2(ext2fs, , trace, inode_cnv, "int", "char*");

#ifdef EXT2FS_PRINT_EXTENTS
void
ext2_print_inode(struct inode *in)
{
	int i;
	struct ext4_extent_header *ehp;
	struct ext4_extent *ep;

	printf("Inode: %5ju", (uintmax_t)in->i_number);
	printf(	/* "Inode: %5d" */
	    " Type: %10s Mode: 0x%o Flags: 0x%x  Version: %d acl: 0x%jx\n",
	    "n/a", in->i_mode, in->i_flags, in->i_gen, in->i_facl);
	printf("User: %5u Group: %5u  Size: %ju\n",
	    in->i_uid, in->i_gid, (uintmax_t)in->i_size);
	printf("Links: %3d Blockcount: %ju\n",
	    in->i_nlink, (uintmax_t)in->i_blocks);
	printf("ctime: 0x%x ", in->i_ctime);
	printf("atime: 0x%x ", in->i_atime);
	printf("mtime: 0x%x ", in->i_mtime);
	if (E2DI_HAS_XTIME(in))
		printf("crtime %#x\n", in->i_birthtime);
	else
		printf("\n");
	if (in->i_flag & IN_E4EXTENTS) {
		printf("Extents:\n");
		ehp = (struct ext4_extent_header *)in->i_db;
		printf("Header (magic 0x%x entries %d max %d depth %d gen %d)\n",
		    le16toh(ehp->eh_magic), le16toh(ehp->eh_ecount),
		    le16toh(ehp->eh_max), le16toh(ehp->eh_depth),
		    le32toh(ehp->eh_gen));
		ep = (struct ext4_extent *)(char *)(ehp + 1);
		printf("Index (blk %d len %d start_lo %d start_hi %d)\n",
		    le32toh(ep->e_blk),
		    le16toh(ep->e_len), le32toh(ep->e_start_lo),
		    le16toh(ep->e_start_hi));
		printf("\n");
	} else {
		printf("BLOCKS:");
		for (i = 0; i < (in->i_blocks <= 24 ? (in->i_blocks + 1) / 2 : 12); i++)
			printf("  %d", in->i_db[i]);
		printf("\n");
	}
}
#endif	/* EXT2FS_PRINT_EXTENTS */

#define XTIME_TO_NSEC(x)	((le32toh(x) & EXT3_NSEC_MASK) >> 2)

/*
 *	raw ext2 inode LE to host inode conversion
 */
int
ext2_ei2i(struct ext2fs_dinode *ei, struct inode *ip)
{
	struct m_ext2fs *fs = ip->i_e2fs;
	uint32_t ei_flags_host;
	uint16_t ei_extra_isize_le;
	int i;

	if ((ip->i_number < EXT2_FIRST_INO(fs) && ip->i_number != EXT2_ROOTINO) ||
	    (ip->i_number < EXT2_ROOTINO) ||
	    (ip->i_number > le32toh(fs->e2fs->e2fs_icount))) {
		SDT_PROBE2(ext2fs, , trace, inode_cnv, 1, "bad inode number");
		return (EINVAL);
	}

	ip->i_nlink = le16toh(ei->e2di_nlink);
	if (ip->i_number == EXT2_ROOTINO && ip->i_nlink == 0) {
		SDT_PROBE2(ext2fs, , trace, inode_cnv, 1, "root inode unallocated");
		return (EINVAL);
	}

	/* Check extra inode size */
	ei_extra_isize_le = le16toh(ei->e2di_extra_isize);
	if (EXT2_INODE_SIZE(fs) > E2FS_REV0_INODE_SIZE) {
		if (E2FS_REV0_INODE_SIZE + ei_extra_isize_le >
		    EXT2_INODE_SIZE(fs) || (ei_extra_isize_le & 3)) {
			SDT_PROBE2(ext2fs, , trace, inode_cnv, 1,
			    "bad extra inode size");
			return (EINVAL);
		}
	}

	/*
	 * Godmar thinks - if the link count is zero, then the inode is
	 * unused - according to ext2 standards. Ufs marks this fact by
	 * setting i_mode to zero - why ? I can see that this might lead to
	 * problems in an undelete.
	 */
	ip->i_mode = ip->i_nlink ? le16toh(ei->e2di_mode) : 0;
	ip->i_size = le32toh(ei->e2di_size);
	if (S_ISREG(ip->i_mode))
		ip->i_size |= (uint64_t)le32toh(ei->e2di_size_high) << 32;
	ip->i_atime = le32toh(ei->e2di_atime);
	ip->i_mtime = le32toh(ei->e2di_mtime);
	ip->i_ctime = le32toh(ei->e2di_ctime);
	if (E2DI_HAS_XTIME(ip)) {
		ip->i_atimensec = XTIME_TO_NSEC(ei->e2di_atime_extra);
		ip->i_mtimensec = XTIME_TO_NSEC(ei->e2di_mtime_extra);
		ip->i_ctimensec = XTIME_TO_NSEC(ei->e2di_ctime_extra);
		ip->i_birthtime = le32toh(ei->e2di_crtime);
		ip->i_birthnsec = XTIME_TO_NSEC(ei->e2di_crtime_extra);
	}
	ip->i_flags = 0;
	ei_flags_host = le32toh(ei->e2di_flags);
	ip->i_flags |= (ei_flags_host & EXT2_APPEND) ? SF_APPEND : 0;
	ip->i_flags |= (ei_flags_host & EXT2_IMMUTABLE) ? SF_IMMUTABLE : 0;
	ip->i_flags |= (ei_flags_host & EXT2_NODUMP) ? UF_NODUMP : 0;
	ip->i_flag |= (ei_flags_host & EXT3_INDEX) ? IN_E3INDEX : 0;
	ip->i_flag |= (ei_flags_host & EXT4_EXTENTS) ? IN_E4EXTENTS : 0;
	ip->i_blocks = le32toh(ei->e2di_nblock);
	ip->i_facl = le32toh(ei->e2di_facl);
	if (E2DI_HAS_HUGE_FILE(ip)) {
		ip->i_blocks |= (uint64_t)le16toh(ei->e2di_nblock_high) << 32;
		ip->i_facl |= (uint64_t)le16toh(ei->e2di_facl_high) << 32;
		if (ei_flags_host & EXT4_HUGE_FILE)
			ip->i_blocks = fsbtodb(ip->i_e2fs, ip->i_blocks);
	}
	ip->i_gen = le32toh(ei->e2di_gen);
	ip->i_uid = le16toh(ei->e2di_uid);
	ip->i_gid = le16toh(ei->e2di_gid);
	ip->i_uid |= (uint32_t)le16toh(ei->e2di_uid_high) << 16;
	ip->i_gid |= (uint32_t)le16toh(ei->e2di_gid_high) << 16;

	if ((ip->i_flag & IN_E4EXTENTS)) {
		memcpy(ip->i_data, ei->e2di_blocks, sizeof(ei->e2di_blocks));
	} else {
		for (i = 0; i < EXT2_NDADDR; i++)
			ip->i_db[i] = le32toh(ei->e2di_blocks[i]);
		for (i = 0; i < EXT2_NIADDR; i++)
			ip->i_ib[i] = le32toh(ei->e2di_blocks[EXT2_NDIR_BLOCKS + i]);
	}

	/* Verify inode csum. */
	return (ext2_ei_csum_verify(ip, ei));
}

#define NSEC_TO_XTIME(t)	(htole32((t << 2) & EXT3_NSEC_MASK))

/*
 *	inode to raw ext2 LE inode conversion
 */
int
ext2_i2ei(struct inode *ip, struct ext2fs_dinode *ei)
{
	struct m_ext2fs *fs;
	int i;

	fs = ip->i_e2fs;
	ei->e2di_mode = htole16(ip->i_mode);
	ei->e2di_nlink = htole16(ip->i_nlink);
	ei->e2di_size = htole32(ip->i_size);
	if (S_ISREG(ip->i_mode))
		ei->e2di_size_high = htole32(ip->i_size >> 32);
	ei->e2di_atime = htole32(ip->i_atime);
	ei->e2di_mtime = htole32(ip->i_mtime);
	ei->e2di_ctime = htole32(ip->i_ctime);
	/*
	 * Godmar thinks: if dtime is nonzero, ext2 says this inode has been
	 * deleted, this would correspond to a zero link count
	 */
	ei->e2di_dtime = htole32(le16toh(ei->e2di_nlink) ? 0 :
	    le32toh(ei->e2di_mtime));
	if (E2DI_HAS_XTIME(ip)) {
		ei->e2di_ctime_extra = NSEC_TO_XTIME(ip->i_ctimensec);
		ei->e2di_mtime_extra = NSEC_TO_XTIME(ip->i_mtimensec);
		ei->e2di_atime_extra = NSEC_TO_XTIME(ip->i_atimensec);
		ei->e2di_crtime = htole32(ip->i_birthtime);
		ei->e2di_crtime_extra = NSEC_TO_XTIME(ip->i_birthnsec);
	}
	/* Keep these in host endian for a while since they change a lot */
	ei->e2di_flags = 0;
	ei->e2di_flags |= htole32((ip->i_flags & SF_APPEND) ? EXT2_APPEND : 0);
	ei->e2di_flags |= htole32((ip->i_flags & SF_IMMUTABLE) ? EXT2_IMMUTABLE : 0);
	ei->e2di_flags |= htole32((ip->i_flags & UF_NODUMP) ? EXT2_NODUMP : 0);
	ei->e2di_flags |= htole32((ip->i_flag & IN_E3INDEX) ? EXT3_INDEX : 0);
	ei->e2di_flags |= htole32((ip->i_flag & IN_E4EXTENTS) ? EXT4_EXTENTS : 0);
	if (ip->i_blocks > ~0U &&
	    !EXT2_HAS_RO_COMPAT_FEATURE(fs, EXT2F_ROCOMPAT_HUGE_FILE)) {
		SDT_PROBE2(ext2fs, , trace, inode_cnv, 1, "i_blocks value is out of range");
		return (EIO);
	}
	if (ip->i_blocks <= 0xffffffffffffULL) {
		ei->e2di_nblock = htole32(ip->i_blocks & 0xffffffff);
		ei->e2di_nblock_high = htole16(ip->i_blocks >> 32 & 0xffff);
	} else {
		ei->e2di_flags |= htole32(EXT4_HUGE_FILE);
		ei->e2di_nblock = htole32(dbtofsb(fs, ip->i_blocks));
		ei->e2di_nblock_high = htole16(dbtofsb(fs, ip->i_blocks) >> 32 & 0xffff);
	}

	ei->e2di_facl = htole32(ip->i_facl & 0xffffffff);
	ei->e2di_facl_high = htole16(ip->i_facl >> 32 & 0xffff);
	ei->e2di_gen = htole32(ip->i_gen);
	ei->e2di_uid = htole16(ip->i_uid & 0xffff);
	ei->e2di_uid_high = htole16(ip->i_uid >> 16 & 0xffff);
	ei->e2di_gid = htole16(ip->i_gid & 0xffff);
	ei->e2di_gid_high = htole16(ip->i_gid >> 16 & 0xffff);

	if ((ip->i_flag & IN_E4EXTENTS)) {
		memcpy(ei->e2di_blocks, ip->i_data, sizeof(ei->e2di_blocks));
	} else {
		for (i = 0; i < EXT2_NDADDR; i++)
			ei->e2di_blocks[i] = htole32(ip->i_db[i]);
		for (i = 0; i < EXT2_NIADDR; i++)
			ei->e2di_blocks[EXT2_NDIR_BLOCKS + i] = htole32(ip->i_ib[i]);
	}

	/* Set inode csum. */
	ext2_ei_csum_set(ip, ei);

	return (0);
}
