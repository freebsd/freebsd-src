/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2013 Juniper Networks, Inc.
 * Copyright (c) 2022-2023 Klara, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef	_FS_TARFS_TARFS_H_
#define	_FS_TARFS_TARFS_H_

#ifndef _KERNEL
#error Should only be included by kernel
#endif

MALLOC_DECLARE(M_TARFSMNT);
MALLOC_DECLARE(M_TARFSNODE);
MALLOC_DECLARE(M_TARFSNAME);

#ifdef SYSCTL_DECL
SYSCTL_DECL(_vfs_tarfs);
#endif

struct componentname;
struct mount;
struct vnode;

/*
 * Internal representation of a tarfs file system node.
 */
struct tarfs_node {
	TAILQ_ENTRY(tarfs_node)	entries;
	TAILQ_ENTRY(tarfs_node)	dirents;

	struct mtx		 lock;

	struct vnode		*vnode;
	struct tarfs_mount	*tmp;
	__enum_uint8(vtype)	 type;
	ino_t			 ino;
	off_t			 offset;
	size_t			 size;
	size_t			 physize;
	char			*name;
	size_t			 namelen;

	/* Node attributes */
	uid_t			 uid;
	gid_t			 gid;
	mode_t			 mode;
	unsigned int		 flags;
	nlink_t			 nlink;
	struct timespec		 atime;
	struct timespec		 mtime;
	struct timespec		 ctime;
	struct timespec		 birthtime;
	unsigned long		 gen;

	/* Block map */
	size_t			 nblk;
	struct tarfs_blk	*blk;

	struct tarfs_node	*parent;
	union {
		/* VDIR */
		struct {
			TAILQ_HEAD(, tarfs_node) dirhead;
			off_t			 lastcookie;
			struct tarfs_node	*lastnode;
		} dir;

		/* VLNK */
		struct {
			char			*name;
			size_t			 namelen;
		} link;

		/* VBLK or VCHR */
		dev_t			 rdev;

		/* VREG */
		struct tarfs_node	*other;
	};
};

/*
 * Entry in sparse file block map.
 */
struct tarfs_blk {
	off_t	 i;		/* input (physical) offset */
	off_t	 o;		/* output (logical) offset */
	size_t	 l;		/* length */
};

/*
 * Decompression buffer.
 */
#define TARFS_ZBUF_SIZE 1048576
struct tarfs_zbuf {
	u_char		 buf[TARFS_ZBUF_SIZE];
	size_t		 off; /* offset of contents */
	size_t		 len; /* length of contents */
};

/*
 * Internal representation of a tarfs mount point.
 */
struct tarfs_mount {
	TAILQ_HEAD(, tarfs_node) allnodes;
	struct mtx		 allnode_lock;

	struct tarfs_node	*root;
	struct vnode		*vp;
	struct mount		*vfs;
	ino_t			 ino;
	struct unrhdr		*ino_unr;
	size_t			 iosize;
	size_t			 nblocks;
	size_t			 nfiles;
	time_t			 mtime; /* default mtime for directories */

	struct tarfs_zio	*zio;
	struct vnode		*znode;
};

struct tarfs_zio {
	struct tarfs_mount	*tmp;

	/* decompression state */
#ifdef ZSTDIO
	struct tarfs_zstd	*zstd; /* decompression state (zstd) */
#endif
	off_t			 ipos; /* current input position */
	off_t			 opos; /* current output position */

	/* index of compression frames */
	unsigned int		 curidx; /* current index position*/
	unsigned int		 nidx; /* number of index entries */
	unsigned int		 szidx; /* index capacity */
	struct tarfs_idx { off_t i, o; } *idx;
};

struct tarfs_fid {
	u_short			 len;	/* length of data in bytes */
	u_short			 data0;	/* force alignment */
	ino_t			 ino;
	unsigned long		 gen;
};

#define	TARFS_NODE_LOCK(tnp) \
	mtx_lock(&(tnp)->lock)
#define	TARFS_NODE_UNLOCK(tnp) \
	mtx_unlock(&(tnp)->lock)
#define	TARFS_ALLNODES_LOCK(tnp) \
	mtx_lock(&(tmp)->allnode_lock)
#define	TARFS_ALLNODES_UNLOCK(tnp) \
	mtx_unlock(&(tmp)->allnode_lock)

/*
 * Data and metadata within tar files are aligned on 512-byte boundaries,
 * to match the block size of the magnetic tapes they were originally
 * intended for.
 */
#define	TARFS_BSHIFT		9
#define	TARFS_BLOCKSIZE		(size_t)(1U << TARFS_BSHIFT)
#define	TARFS_BLKOFF(l)		((l) % TARFS_BLOCKSIZE)
#define	TARFS_BLKNUM(l)		((l) >> TARFS_BSHIFT)
#define	TARFS_SZ2BLKS(sz)	(((sz) + TARFS_BLOCKSIZE - 1) / TARFS_BLOCKSIZE)

/*
 * Our preferred I/O size.
 */
extern unsigned int tarfs_ioshift;
#define	TARFS_IOSHIFT_MIN	TARFS_BSHIFT
#define	TARFS_IOSHIFT_DEFAULT	PAGE_SHIFT
#define	TARFS_IOSHIFT_MAX	PAGE_SHIFT

#define	TARFS_ROOTINO		((ino_t)3)
#define	TARFS_ZIOINO		((ino_t)4)
#define	TARFS_MININO		((ino_t)65535)

#define	TARFS_COOKIE_DOT	0
#define	TARFS_COOKIE_DOTDOT	1
#define	TARFS_COOKIE_EOF	OFF_MAX

#define	TARFS_ZIO_NAME		".tar"
#define	TARFS_ZIO_NAMELEN	(sizeof(TARFS_ZIO_NAME) - 1)

extern struct vop_vector tarfs_vnodeops;

static inline
struct tarfs_mount *
MP_TO_TARFS_MOUNT(struct mount *mp)
{

	MPASS(mp != NULL && mp->mnt_data != NULL);
	return (mp->mnt_data);
}

static inline
struct tarfs_node *
VP_TO_TARFS_NODE(struct vnode *vp)
{

	MPASS(vp != NULL && vp->v_data != NULL);
	return (vp->v_data);
}

int	tarfs_alloc_node(struct tarfs_mount *tmp, const char *name,
	    size_t namelen, __enum_uint8(vtype) type, off_t off, size_t sz,
	    time_t mtime, uid_t uid, gid_t gid, mode_t mode,
	    unsigned int flags, const char *linkname, dev_t rdev,
	    struct tarfs_node *parent, struct tarfs_node **node);
int	tarfs_load_blockmap(struct tarfs_node *tnp, size_t realsize);
void	tarfs_free_node(struct tarfs_node *tnp);
struct tarfs_node *
	tarfs_lookup_dir(struct tarfs_node *tnp, off_t cookie);
struct tarfs_node *
	tarfs_lookup_node(struct tarfs_node *tnp, struct tarfs_node *f,
	    struct componentname *cnp);
int	tarfs_read_file(struct tarfs_node *tnp, size_t len, struct uio *uiop);

int	tarfs_io_init(struct tarfs_mount *tmp);
int	tarfs_io_fini(struct tarfs_mount *tmp);
int	tarfs_io_read(struct tarfs_mount *tmp, bool raw,
    struct uio *uiop);
ssize_t	tarfs_io_read_buf(struct tarfs_mount *tmp, bool raw,
    void *buf, off_t off, size_t len);
unsigned int
	tarfs_strtofflags(const char *str, char **end);

#endif	/* _FS_TARFS_TARFS_H_ */
