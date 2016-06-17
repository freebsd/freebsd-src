/*
 *  Name                         : qnx4_fs_sb.h
 *  Author                       : Richard Frowijn
 *  Function                     : qnx4 superblock definitions
 *  Version                      : 1.0.2
 *  Last modified                : 2000-01-06
 * 
 *  History                      : 23-03-1998 created
 * 
 */
#ifndef _QNX4_FS_SB
#define _QNX4_FS_SB

#include <linux/qnx4_fs.h>

/*
 * qnx4 super-block data in memory
 */

struct qnx4_sb_info {
	struct buffer_head	*sb_buf;	/* superblock buffer */
	struct qnx4_super_block	*sb;		/* our superblock */
	unsigned int		Version;	/* may be useful */
	struct qnx4_inode_entry	*BitMap;	/* useful */
};

#endif
