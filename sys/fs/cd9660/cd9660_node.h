/*-
 * Copyright (c) 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley
 * by Pace Willisson (pace@blitz.com).  The Rock Ridge Extension
 * Support code is derived from software contributed to Berkeley
 * by Atsushi Murai (amurai@spec.co.jp).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 *	@(#)cd9660_node.h	8.2 (Berkeley) 1/23/94
 */

/*
 * Theoretically, directories can be more than 2Gb in length,
 * however, in practice this seems unlikely. So, we define
 * the type doff_t as a long to keep down the cost of doing
 * lookup on a 32-bit machine. If you are porting to a 64-bit
 * architecture, you should make doff_t the same as off_t.
 */
#define doff_t	long

typedef	struct	{
	struct timespec	iso_atime;	/* time of last access */
	struct timespec	iso_mtime;	/* time of last modification */
	struct timespec	iso_ctime;	/* time file changed */
	u_short		iso_mode;	/* files access mode and type */
	uid_t		iso_uid;	/* owner user id */
	gid_t		iso_gid;	/* owner group id */
	short		iso_links;	/* links of file */
	dev_t		iso_rdev;	/* Major/Minor number for special */
} ISO_RRIP_INODE;

#ifdef	ISODEVMAP
/*
 * FOr device# (major,minor) translation table
 */
struct iso_dnode {
	struct iso_dnode *d_chain[2];	/* hash chain, MUST be first */
	dev_t		i_dev;		/* device where dnode resides */
	ino_t		i_number;	/* the identity of the inode */
	dev_t		d_dev;		/* device # for translation */
};
#define	d_forw		d_chain[0]
#define	d_back		d_chain[1]
#endif

struct iso_node {
	struct	iso_node *i_chain[2]; /* hash chain, MUST be first */
	struct	vnode *i_vnode;	/* vnode associated with this inode */
	struct	vnode *i_devvp;	/* vnode for block I/O */
	u_long	i_flag;		/* see below */
	dev_t	i_dev;		/* device where inode resides */
	ino_t	i_number;	/* the identity of the inode */
				/* we use the actual starting block of the file */
	struct	iso_mnt *i_mnt;	/* filesystem associated with this inode */
	struct	lockf *i_lockf;	/* head of byte-level lock list */
	doff_t	i_endoff;	/* end of useful stuff in directory */
	doff_t	i_diroff;	/* offset in dir, where we found last entry */
	doff_t	i_offset;	/* offset of free space in directory */
	ino_t	i_ino;		/* inode number of found directory */
	long	i_spare0;
	long	i_spare1;

	long iso_extent;	/* extent of file */
	long i_size;
	long iso_start;		/* actual start of data of file (may be different */
				/* from iso_extent, if file has extended attributes) */
	ISO_RRIP_INODE  inode;
};

#define	i_forw		i_chain[0]
#define	i_back		i_chain[1]

/* flags */
#define	ILOCKED		0x0001		/* inode is locked */
#define	IWANT		0x0002		/* some process waiting on lock */
#define	IACC		0x0020		/* inode access time to be updated */

#define VTOI(vp) ((struct iso_node *)(vp)->v_data)
#define ITOV(ip) ((ip)->i_vnode)

#define ISO_ILOCK(ip)	iso_ilock(ip)
#define ISO_IUNLOCK(ip)	iso_iunlock(ip)

/*
 * Prototypes for ISOFS vnode operations
 */
int cd9660_lookup __P((struct vop_lookup_args *));
int cd9660_open __P((struct vop_open_args *));
int cd9660_close __P((struct vop_close_args *));
int cd9660_access __P((struct vop_access_args *));
int cd9660_getattr __P((struct vop_getattr_args *));
int cd9660_read __P((struct vop_read_args *));
int cd9660_ioctl __P((struct vop_ioctl_args *));
int cd9660_select __P((struct vop_select_args *));
int cd9660_mmap __P((struct vop_mmap_args *));
int cd9660_seek __P((struct vop_seek_args *));
int cd9660_readdir __P((struct vop_readdir_args *));
int cd9660_abortop __P((struct vop_abortop_args *));
int cd9660_inactive __P((struct vop_inactive_args *));
int cd9660_reclaim __P((struct vop_reclaim_args *));
int cd9660_bmap __P((struct vop_bmap_args *));
int cd9660_lock __P((struct vop_lock_args *));
int cd9660_unlock __P((struct vop_unlock_args *));
int cd9660_strategy __P((struct vop_strategy_args *));
int cd9660_print __P((struct vop_print_args *));
int cd9660_islocked __P((struct vop_islocked_args *));
void cd9660_defattr __P((struct iso_directory_record *,
			struct iso_node *, struct buf *));
void cd9660_deftstamp __P((struct iso_directory_record *,
			struct iso_node *, struct buf *));
#ifdef	ISODEVMAP
struct iso_dnode *iso_dmap __P((dev_t, ino_t, int));
void iso_dunmap __P((dev_t));
#endif
