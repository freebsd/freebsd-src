/*
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
 * $FreeBSD: src/sys/gnu/ext2fs/ext2_inode_cnv.c,v 1.11 2000/01/01 17:39:21 bde Exp $
 */

/*
 * routines to convert on disk ext2 inodes in dinodes and back
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/stat.h>
#include <sys/vnode.h>

#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>

/*
 * Undo the definitions in <ufs/ufs/inode.h> that would destroy the include
 * of <gnu/ext2fs/ext2_fs.h>.
 */
#undef i_atime
#undef i_blocks
#undef i_ctime
#undef i_db
#undef i_flags
#undef i_gen
#undef i_gid
#undef i_ib
#undef i_mode
#undef i_mtime
#undef i_nlink
#undef i_rdev
#undef i_shortlink
#undef i_size
#undef i_uid

#include <gnu/ext2fs/ext2_fs.h>
#include <gnu/ext2fs/ext2_extern.h>

void
ext2_print_dinode( di )
	struct dinode *di;
{
	int i;
	printf( /* "Inode: %5d" */
		" Type: %10s Mode: 0x%o Flags: 0x%x  Version: %d\n",
		"n/a", di->di_mode, di->di_flags, di->di_gen);
	printf( "User: %5lu Group: %5lu  Size: %lu\n",
		(unsigned long)di->di_uid, (unsigned long)di->di_gid,
		(unsigned long)di->di_size);
	printf( "Links: %3d Blockcount: %d\n",
		di->di_nlink, di->di_blocks);
	printf( "ctime: 0x%x", di->di_ctime); 
	printf( "atime: 0x%x", di->di_atime); 
	printf( "mtime: 0x%x", di->di_mtime); 
	printf( "BLOCKS: ");
	for(i=0; i < (di->di_blocks <= 24 ? ((di->di_blocks+1)/2): 12); i++)
		printf("%d ", di->di_db[i]);
	printf("\n");
}

void
ext2_print_inode( in )
	struct inode *in;
{
	printf( "Inode: %5d", in->i_number);
	ext2_print_dinode(&in->i_din);
}

/*
 *	raw ext2 inode to dinode
 */
void
ext2_ei2di(ei, di)
        struct ext2_inode *ei;
        struct dinode *di;
{
        int     i;

        di->di_nlink    = ei->i_links_count;
	/* Godmar thinks - if the link count is zero, then the inode is
	   unused - according to ext2 standards. Ufs marks this fact
	   by setting i_mode to zero - why ?
	   I can see that this might lead to problems in an undelete.
	*/
	di->di_mode     = ei->i_links_count ? ei->i_mode : 0;
        di->di_size     = ei->i_size;
        di->di_atime	= ei->i_atime;
        di->di_mtime	= ei->i_mtime;
        di->di_ctime	= ei->i_ctime;
        di->di_flags    = 0;
        di->di_flags    |= (ei->i_flags & EXT2_APPEND_FL) ? APPEND : 0;
        di->di_flags    |= (ei->i_flags & EXT2_IMMUTABLE_FL) ? IMMUTABLE : 0;
        di->di_blocks   = ei->i_blocks;
        di->di_gen      = ei->i_generation;
        di->di_uid      = ei->i_uid;
        di->di_gid      = ei->i_gid;
	/* XXX use memcpy */
        for(i = 0; i < NDADDR; i++)
                di->di_db[i] = ei->i_block[i];
        for(i = 0; i < NIADDR; i++)
                di->di_ib[i] = ei->i_block[EXT2_NDIR_BLOCKS + i];
}

/*
 *	dinode to raw ext2 inode
 */
void
ext2_di2ei(di, ei)
        struct dinode *di;
        struct ext2_inode *ei;
{
        int     i;

        ei->i_mode              = di->di_mode;
        ei->i_links_count       = di->di_nlink;
	/* 
	   Godmar thinks: if dtime is nonzero, ext2 says this inode
	   has been deleted, this would correspond to a zero link count
	 */
        ei->i_dtime             = ei->i_links_count ? 0 : di->di_mtime;
        ei->i_size              = di->di_size;
        ei->i_atime             = di->di_atime;
        ei->i_mtime             = di->di_mtime;
        ei->i_ctime             = di->di_ctime;
        ei->i_flags             = di->di_flags;
        ei->i_flags    		= 0;
        ei->i_flags    		|= (di->di_flags & APPEND) ? EXT2_APPEND_FL: 0;
        ei->i_flags    		|= (di->di_flags & IMMUTABLE) 
							? EXT2_IMMUTABLE_FL: 0;
        ei->i_blocks            = di->di_blocks;
        ei->i_generation        = di->di_gen;
        ei->i_uid               = di->di_uid;
        ei->i_gid               = di->di_gid;
	/* XXX use memcpy */
        for(i = 0; i < NDADDR; i++)
                ei->i_block[i] = di->di_db[i];
        for(i = 0; i < NIADDR; i++)
                ei->i_block[EXT2_NDIR_BLOCKS + i] = di->di_ib[i];
}
