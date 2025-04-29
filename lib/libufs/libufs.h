/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2002 Juli Mallett.  All rights reserved.
 *
 * This software was written by Juli Mallett <jmallett@FreeBSD.org> for the
 * FreeBSD project.  Redistribution and use in source and binary forms, with
 * or without modification, are permitted provided that the following
 * conditions are met:
 *
 * 1. Redistribution of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistribution in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	__LIBUFS_H__
#define	__LIBUFS_H__
#include <stdbool.h>

/*
 * Various disk controllers require their buffers to be aligned to the size
 * of a cache line. The LIBUFS_BUFALIGN defines the required alignment size.
 * The alignment must be a power of 2.
 */
#define LIBUFS_BUFALIGN	128

/*
 * userland ufs disk.
 */
struct uufsd {
	union {
		struct fs d_fs;		/* filesystem information */
		char d_sb[SBLOCKSIZE];	/* superblock as buffer */
	} d_sbunion __aligned(LIBUFS_BUFALIGN);
	union {
		struct cg d_cg;		/* cylinder group */
		char d_buf[MAXBSIZE];	/* cylinder group storage */
	} d_cgunion __aligned(LIBUFS_BUFALIGN);
	union {
		union dinodep d_ino[1];	/* inode block */
		char d_inos[MAXBSIZE];	/* inode block as buffer */
	} d_inosunion __aligned(LIBUFS_BUFALIGN);
	const char *d_name;		/* disk name */
	const char *d_error;		/* human readable disk error */
	ufs2_daddr_t d_sblock;		/* superblock location */
	struct fs_summary_info *d_si;	/* Superblock summary info */
	union dinodep d_dp;		/* pointer to currently active inode */
	ino_t d_inomin;			/* low ino */
	ino_t d_inomax;			/* high ino */
	off_t d_sblockloc;		/* where to look for the superblock */
	int64_t d_bsize;		/* device bsize */
	int64_t d_lookupflags;		/* flags to superblock lookup */
	int64_t d_mine;			/* internal flags */
	int32_t d_ccg;			/* current cylinder group */
	int32_t d_ufs;			/* decimal UFS version */
	int32_t d_fd;			/* raw device file descriptor */
	int32_t d_lcg;			/* last cylinder group (in d_cg) */
};
#define	d_inos	d_inosunion.d_inos
#define	d_fs	d_sbunion.d_fs
#define	d_cg	d_cgunion.d_cg

/*
 * libufs macros (internal, non-exported).
 */
#ifdef	_LIBUFS
/*
 * Ensure that the buffer is aligned to the I/O subsystem requirements.
 */
#define BUF_MALLOC(newbufpp, data, size) {				     \
	if (data != NULL && (((intptr_t)data) & (LIBUFS_BUFALIGN - 1)) == 0) \
		*newbufpp = (void *)data;				     \
	else								     \
		*newbufpp = aligned_alloc(LIBUFS_BUFALIGN, size);	     \
}
/*
 * Trace steps through libufs, to be used at entry and erroneous return.
 */
static inline void
ERROR(struct uufsd *u, const char *str)
{

#ifdef	_LIBUFS_DEBUGGING
	if (str != NULL) {
		fprintf(stderr, "libufs: %s", str);
		if (errno != 0)
			fprintf(stderr, ": %s", strerror(errno));
		fprintf(stderr, "\n");
	}
#endif
	if (u != NULL)
		u->d_error = str;
}
#endif	/* _LIBUFS */

__BEGIN_DECLS

/*
 * libufs prototypes.
 */

/*
 * ffs_subr.c
 */
void	ffs_clrblock(struct fs *, u_char *, ufs1_daddr_t);
void	ffs_clusteracct(struct fs *, struct cg *, ufs1_daddr_t, int);
void	ffs_fragacct(struct fs *, int, int32_t [], int);
int	ffs_isblock(struct fs *, u_char *, ufs1_daddr_t);
int	ffs_isfreeblock(struct fs *, u_char *, ufs1_daddr_t);
bool	ffs_oldfscompat_inode_read(struct fs *, union dinodep, time_t);
int	ffs_sbsearch(void *, struct fs **, int, char *,
	    int (*)(void *, off_t, void **, int));
void	ffs_setblock(struct fs *, u_char *, ufs1_daddr_t);
int	ffs_sbget(void *, struct fs **, off_t, int, char *,
	    int (*)(void *, off_t, void **, int));
int	ffs_sbput(void *, struct fs *, off_t,
	    int (*)(void *, off_t, void *, int));
void	ffs_update_dinode_ckhash(struct fs *, struct ufs2_dinode *);
int	ffs_verify_dinode_ckhash(struct fs *, struct ufs2_dinode *);

/*
 * block.c
 */
ssize_t bread(struct uufsd *, ufs2_daddr_t, void *, size_t);
ssize_t bwrite(struct uufsd *, ufs2_daddr_t, const void *, size_t);
int berase(struct uufsd *, ufs2_daddr_t, ufs2_daddr_t);

/*
 * cgroup.c
 */
ufs2_daddr_t cgballoc(struct uufsd *);
int cgbfree(struct uufsd *, ufs2_daddr_t, long);
ino_t cgialloc(struct uufsd *);
int cgget(int, struct fs *, int, struct cg *);
int cgput(int, struct fs *, struct cg *);
int cgread(struct uufsd *);
int cgread1(struct uufsd *, int);
int cgwrite(struct uufsd *);
int cgwrite1(struct uufsd *, int);

/*
 * inode.c
 */
int getinode(struct uufsd *, union dinodep *, ino_t);
int putinode(struct uufsd *);

/*
 * sblock.c
 */
int sbread(struct uufsd *);
int sbfind(struct uufsd *, int);
int sbwrite(struct uufsd *, int);
/* low level superblock read/write functions */
int sbget(int, struct fs **, off_t, int);
int sbsearch(int, struct fs **, int);
int sbput(int, struct fs *, int);

/*
 * type.c
 */
int ufs_disk_close(struct uufsd *);
int ufs_disk_fillout(struct uufsd *, const char *);
int ufs_disk_fillout_blank(struct uufsd *, const char *);
int ufs_disk_write(struct uufsd *);

/*
 * crc32c.c
 */
uint32_t calculate_crc32c(uint32_t, const void *, size_t);

__END_DECLS

#endif	/* __LIBUFS_H__ */
