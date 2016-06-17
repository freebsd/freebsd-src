#ifndef UMSDOS_FS_I_H
#define UMSDOS_FS_I_H

#ifndef _LINUX_TYPES_H
#include <linux/types.h>
#endif

#include <linux/msdos_fs_i.h>
#include <linux/pipe_fs_i.h>

/* #Specification: strategy / in memory inode
 * Here is the information specific to the inode of the UMSDOS file
 * system. This information is added to the end of the standard struct
 * inode. Each file system has its own extension to struct inode,
 * so do the umsdos file system.
 * 
 * The strategy is to have the umsdos_inode_info as a superset of
 * the msdos_inode_info, since most of the time the job is done
 * by the msdos fs code.
 * 
 * So we duplicate the msdos_inode_info, and add our own info at the
 * end.
 * 
 * The offset in this EMD file of the entry: pos
 * 
 * For directory, we have dir_locking_info to help synchronise
 * file creation and file lookup. See also msdos_fs_i.h for more 
 * information about msdos_inode_info.
 * 
 * Special file and fifo do have an inode which correspond to an
 * empty MSDOS file.
 * 
 * symlink are processed mostly like regular file. The content is the
 * link.
 * 
 * The UMSDOS specific extension is placed after the union.
 */

struct dir_locking_info {
	wait_queue_head_t p;
	short int looking;	/* How many process doing a lookup */
	short int creating;	/* Is there any creation going on here
				 *  Only one at a time, although one
				 *  may recursively lock, so it is a counter
				 */
	long pid;		/* pid of the process owning the creation
				 * lock */
};

struct umsdos_inode_info {
	struct msdos_inode_info msdos_info;
	struct dir_locking_info dir_info;
	int i_patched;			/* Inode has been patched */
	int i_is_hlink;			/* Resolved hardlink inode? */
	off_t pos;			/* Entry offset in the emd_owner file */
};

#endif
