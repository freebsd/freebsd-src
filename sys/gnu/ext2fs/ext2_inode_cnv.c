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
 */

/*
 * routines to convert on disk ext2 inodes in dinodes and back
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>

/* these defs would destroy the ext2_fs_i #include */
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
#include <gnu/ext2fs/ext2_fs_i.h>

void
ext2_print_dinode( di )
	struct dinode *di;
{
	int i;
	printf( /* "Inode: %5d" */
		" Type: %10s Mode: 0x%o Flags: 0x%x  Version: %d\n",
		"n/a", di->di_mode, di->di_flags, di->di_gen);
	printf( "User: %5d Group: %5d  Size: %d\n",
		di->di_uid, di->di_gid, di->di_size);
	printf( "Links: %3d Blockcount: %d\n",
		di->di_nlink, di->di_blocks);
	printf( "ctime: 0x%x", di->di_ctime.ts_sec); 
#if !defined(__FreeBSD__)
	print_time(" -- %s\n", di->di_ctime.ts_sec);
#endif
	printf( "atime: 0x%x", di->di_atime.ts_sec); 
#if !defined(__FreeBSD__)
	print_time(" -- %s\n", di->di_atime.ts_sec);
#endif
	printf( "mtime: 0x%x", di->di_mtime.ts_sec); 
#if !defined(__FreeBSD__)
	print_time(" -- %s\n", di->di_mtime.ts_sec);
#endif
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
        di->di_atime.ts_sec    = ei->i_atime;
        di->di_mtime.ts_sec    = ei->i_mtime;
        di->di_ctime.ts_sec    = ei->i_ctime;
        di->di_flags    = 0;
        di->di_flags    |= (ei->i_flags & EXT2_APPEND_FL) ? APPEND : 0;
        di->di_flags    |= (ei->i_flags & EXT2_IMMUTABLE_FL) ? IMMUTABLE : 0;
        di->di_blocks   = ei->i_blocks;
        di->di_gen      = ei->i_version;        /* XXX is that true ??? */
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
        ei->i_dtime             = ei->i_links_count ? 0 : di->di_mtime.ts_sec;
        ei->i_size              = di->di_size;
        ei->i_atime             = di->di_atime.ts_sec;
        ei->i_mtime             = di->di_mtime.ts_sec;
        ei->i_ctime             = di->di_ctime.ts_sec;
        ei->i_flags             = di->di_flags;
        ei->i_flags    		= 0;
        ei->i_flags    		|= (di->di_flags & APPEND) ? EXT2_APPEND_FL: 0;
        ei->i_flags    		|= (di->di_flags & IMMUTABLE) 
							? EXT2_IMMUTABLE_FL: 0;
        ei->i_blocks            = di->di_blocks;
        ei->i_version           = di->di_gen;   /* XXX is that true ??? */
        ei->i_uid               = di->di_uid;
        ei->i_gid               = di->di_gid;
	/* XXX use memcpy */
        for(i = 0; i < NDADDR; i++)
                ei->i_block[i] = di->di_db[i];
        for(i = 0; i < NIADDR; i++)
                ei->i_block[EXT2_NDIR_BLOCKS + i] = di->di_ib[i];
}
