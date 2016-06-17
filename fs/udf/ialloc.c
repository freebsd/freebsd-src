/*
 * ialloc.c
 *
 * PURPOSE
 *	Inode allocation handling routines for the OSTA-UDF(tm) filesystem.
 *
 * CONTACTS
 *	E-mail regarding any portion of the Linux UDF file system should be
 *	directed to the development team mailing list (run by majordomo):
 *		linux_udf@hpesjro.fc.hp.com
 *
 * COPYRIGHT
 *	This file is distributed under the terms of the GNU General Public
 *	License (GPL). Copies of the GPL can be obtained from:
 *		ftp://prep.ai.mit.edu/pub/gnu/GPL
 *	Each contributing author retains all rights to their own work.
 *
 *  (C) 1998-2001 Ben Fennema
 *
 * HISTORY
 *
 *  02/24/99 blf  Created.
 *
 */

#include "udfdecl.h"
#include <linux/fs.h>
#include <linux/locks.h>
#include <linux/quotaops.h>
#include <linux/udf_fs.h>

#include "udf_i.h"
#include "udf_sb.h"

void udf_free_inode(struct inode * inode)
{
	struct super_block * sb = inode->i_sb;
	int is_directory;
	unsigned long ino;

	ino = inode->i_ino;

	/*
	 * Note: we must free any quota before locking the superblock,
	 * as writing the quota to disk may need the lock as well.
	 */
	DQUOT_FREE_INODE(inode);
	DQUOT_DROP(inode);

	lock_super(sb);

	is_directory = S_ISDIR(inode->i_mode);

	clear_inode(inode);

	if (UDF_SB_LVIDBH(sb))
	{
		if (is_directory)
			UDF_SB_LVIDIU(sb)->numDirs =
				cpu_to_le32(le32_to_cpu(UDF_SB_LVIDIU(sb)->numDirs) - 1);
		else
			UDF_SB_LVIDIU(sb)->numFiles =
				cpu_to_le32(le32_to_cpu(UDF_SB_LVIDIU(sb)->numFiles) - 1);
		
		mark_buffer_dirty(UDF_SB_LVIDBH(sb));
	}
	unlock_super(sb);

	udf_free_blocks(sb, NULL, UDF_I_LOCATION(inode), 0, 1);
}

struct inode * udf_new_inode (struct inode *dir, int mode, int * err)
{
	struct super_block *sb;
	struct inode * inode;
	int block;
	uint32_t start = UDF_I_LOCATION(dir).logicalBlockNum;

	sb = dir->i_sb;
	inode = new_inode(sb);

	if (!inode)
	{
		*err = -ENOMEM;
		return NULL;
	}
	*err = -ENOSPC;

	block = udf_new_block(dir->i_sb, NULL, UDF_I_LOCATION(dir).partitionReferenceNum,
		start, err);
	if (*err)
	{
		iput(inode);
		return NULL;
	}
	lock_super(sb);

	if (UDF_SB_LVIDBH(sb))
	{
		struct logicalVolHeaderDesc *lvhd;
		uint64_t uniqueID;
		lvhd = (struct logicalVolHeaderDesc *)(UDF_SB_LVID(sb)->logicalVolContentsUse);
		if (S_ISDIR(mode))
			UDF_SB_LVIDIU(sb)->numDirs =
				cpu_to_le32(le32_to_cpu(UDF_SB_LVIDIU(sb)->numDirs) + 1);
		else
			UDF_SB_LVIDIU(sb)->numFiles =
				cpu_to_le32(le32_to_cpu(UDF_SB_LVIDIU(sb)->numFiles) + 1);
		UDF_I_UNIQUE(inode) = uniqueID = le64_to_cpu(lvhd->uniqueID);
		if (!(++uniqueID & 0x00000000FFFFFFFFUL))
			uniqueID += 16;
		lvhd->uniqueID = cpu_to_le64(uniqueID);
		mark_buffer_dirty(UDF_SB_LVIDBH(sb));
	}
	inode->i_mode = mode;
	inode->i_uid = current->fsuid;
	if (dir->i_mode & S_ISGID)
	{
		inode->i_gid = dir->i_gid;
		if (S_ISDIR(mode))
			mode |= S_ISGID;
	}
	else
		inode->i_gid = current->fsgid;

	UDF_I_LOCATION(inode).logicalBlockNum = block;
	UDF_I_LOCATION(inode).partitionReferenceNum = UDF_I_LOCATION(dir).partitionReferenceNum;
	inode->i_ino = udf_get_lb_pblock(sb, UDF_I_LOCATION(inode), 0);
	inode->i_blksize = PAGE_SIZE;
	inode->i_blocks = 0;
	UDF_I_LENEATTR(inode) = 0;
	UDF_I_LENALLOC(inode) = 0;
	if (UDF_QUERY_FLAG(inode->i_sb, UDF_FLAG_USE_EXTENDED_FE))
	{
		UDF_I_EXTENDED_FE(inode) = 1;
		UDF_UPDATE_UDFREV(inode->i_sb, UDF_VERS_USE_EXTENDED_FE);
	}
	else
		UDF_I_EXTENDED_FE(inode) = 0;
	if (UDF_QUERY_FLAG(inode->i_sb, UDF_FLAG_USE_AD_IN_ICB))
		UDF_I_ALLOCTYPE(inode) = ICBTAG_FLAG_AD_IN_ICB;
	else if (UDF_QUERY_FLAG(inode->i_sb, UDF_FLAG_USE_SHORT_AD))
		UDF_I_ALLOCTYPE(inode) = ICBTAG_FLAG_AD_SHORT;
	else
		UDF_I_ALLOCTYPE(inode) = ICBTAG_FLAG_AD_LONG;
	inode->i_mtime = inode->i_atime = inode->i_ctime =
		UDF_I_CRTIME(inode) = CURRENT_TIME;
	UDF_I_UMTIME(inode) = UDF_I_UCTIME(inode) =
		UDF_I_UCRTIME(inode) = CURRENT_UTIME;
	UDF_I_NEW_INODE(inode) = 1;
	insert_inode_hash(inode);
	mark_inode_dirty(inode);

	unlock_super(sb);
	if (DQUOT_ALLOC_INODE(inode))
	{
		DQUOT_DROP(inode);
		inode->i_flags |= S_NOQUOTA;
		inode->i_nlink = 0;
		iput(inode);
		*err = -EDQUOT;
		return NULL;
	}

	*err = 0;
	return inode;
}
