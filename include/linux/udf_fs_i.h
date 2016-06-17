/*
 * udf_fs_i.h
 *
 * This file is intended for the Linux kernel/module. 
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
 */

#ifndef _UDF_FS_I_H
#define _UDF_FS_I_H 1

#ifdef __KERNEL__

#ifndef _ECMA_167_H
typedef struct
{
	__u32 logicalBlockNum;
	__u16 partitionReferenceNum;
} __attribute__ ((packed)) lb_addr;
#endif

struct udf_inode_info
{
	long i_umtime;
	long i_uctime;
	long i_crtime;
	long i_ucrtime;
	/* Physical address of inode */
	lb_addr i_location;
	__u64 i_unique;
	__u32 i_lenEAttr;
	__u32 i_lenAlloc;
	__u64 i_lenExtents;
	__u32 i_next_alloc_block;
	__u32 i_next_alloc_goal;
	unsigned i_alloc_type : 3;
	unsigned i_extended_fe : 1;
	unsigned i_strat_4096 : 1;
	unsigned i_new_inode : 1;
	unsigned reserved : 26;
};

#endif

/* exported IOCTLs, we have 'l', 0x40-0x7f */

#define UDF_GETEASIZE   _IOR('l', 0x40, int)
#define UDF_GETEABLOCK  _IOR('l', 0x41, void *)
#define UDF_GETVOLIDENT _IOR('l', 0x42, void *)
#define UDF_RELOCATE_BLOCKS _IOWR('l', 0x43, long)

#endif /* _UDF_FS_I_H */
