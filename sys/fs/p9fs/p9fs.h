/*-
 * Copyright (c) 2017-2020 Juniper Networks, Inc.
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/* This file has prototypes specific to the p9fs file system */

#ifndef FS_P9FS_P9FS_H
#define FS_P9FS_P9FS_H

struct p9fs_session;

/* QID: Unique identification for the file being accessed */
struct p9fs_qid {
	uint8_t qid_mode;	/* file mode specifiying file type */
	uint32_t qid_version;	/* version of the file */
	uint64_t qid_path;	/* unique integer among all files in hierarchy */
};

/*
 * The in memory representation of the on disk inode. Save the current
 * fields to write it back later.
 */
struct p9fs_inode {
        /* Make it simple first, Add more fields later */
	uint64_t i_size;	/* size of the inode */
	uint16_t i_type;	/* type of inode */
	uint32_t i_dev;		/* type of device */
	uint32_t i_mode;	/* mode of the inode */
	uint32_t i_atime;	/* time of last access */
	uint32_t i_mtime;	/* time of last modification */
	uint32_t i_ctime;	/* time of last status change */
	uint32_t i_atime_nsec;	/* times of last access in nanoseconds resolution */
	uint32_t i_mtime_nsec;	/* time of last modification in nanoseconds resolution */
	uint32_t i_ctime_nsec;	/* time of last status change in nanoseconds resolution */
	uint64_t i_length;
	char *i_name;		/* inode name */
	char *i_uid;		/* inode user id */
	char *i_gid;		/* inode group id */
	char *i_muid;
	char *i_extension;       /* 9p2000.u extensions */
	uid_t n_uid;            /* 9p2000.u extensions */
	gid_t n_gid;            /* 9p2000.u extensions */
	uid_t n_muid;           /* 9p2000.u extensions */
	/* bookkeeping info on the client. */
	uint16_t i_links_count;  /*number of references to the inode*/
	uint64_t i_qid_path;    /* using inode number for reference. */
	uint64_t i_flags;
	uint64_t blksize;	/* block size for file system */
	uint64_t blocks;	/* number of 512B blocks allocated */
	uint64_t gen;		/* reserved for future use */
	uint64_t data_version;	/* reserved for future use */

};

#define P9FS_VFID_MTX(_sc) (&(_sc)->vfid_mtx)
#define P9FS_VFID_LOCK(_sc) mtx_lock(P9FS_VFID_MTX(_sc))
#define P9FS_VFID_UNLOCK(_sc) mtx_unlock(P9FS_VFID_MTX(_sc))
#define P9FS_VFID_LOCK_INIT(_sc) mtx_init(P9FS_VFID_MTX(_sc), \
    "VFID List lock", NULL, MTX_DEF)
#define P9FS_VFID_LOCK_DESTROY(_sc) mtx_destroy(P9FS_VFID_MTX(_sc))

#define P9FS_VOFID_MTX(_sc) (&(_sc)->vofid_mtx)
#define P9FS_VOFID_LOCK(_sc) mtx_lock(P9FS_VOFID_MTX(_sc))
#define P9FS_VOFID_UNLOCK(_sc) mtx_unlock(P9FS_VOFID_MTX(_sc))
#define P9FS_VOFID_LOCK_INIT(_sc) mtx_init(P9FS_VOFID_MTX(_sc), \
    "VOFID List lock", NULL, MTX_DEF)
#define P9FS_VOFID_LOCK_DESTROY(_sc) mtx_destroy(P9FS_VOFID_MTX(_sc))

#define VFID	0x01
#define VOFID	0x02

/* A Plan9 node. */
struct p9fs_node {
	STAILQ_HEAD( ,p9_fid) vfid_list;	/* vfid related to uid */
	struct mtx vfid_mtx;			/* mutex for vfid list */
	STAILQ_HEAD( ,p9_fid) vofid_list;	/* vofid related to uid */
	struct mtx vofid_mtx;			/* mutex for vofid list */
	struct p9fs_node *parent;		/* pointer to parent p9fs node */
	struct p9fs_qid vqid;			/* the server qid, will be from the host */
	struct vnode *v_node;			/* vnode for this fs_node. */
	struct p9fs_inode inode;		/* in memory representation of ondisk information*/
	struct p9fs_session *p9fs_ses;	/*  Session_ptr for this node */
	STAILQ_ENTRY(p9fs_node) p9fs_node_next;
	uint64_t flags;
};

#define P9FS_VTON(vp) ((struct p9fs_node *)(vp)->v_data)
#define P9FS_NTOV(node) ((node)->v_node)
#define	VFSTOP9(mp) ((struct p9fs_mount *)(mp)->mnt_data)
#define QEMU_DIRENTRY_SZ	25
#define P9FS_NODE_MODIFIED	0x1  /* indicating file change */
#define P9FS_ROOT		0x2  /* indicating root p9fs node */
#define P9FS_NODE_DELETED	0x4  /* indicating file or directory delete */
#define P9FS_NODE_IN_SESSION	0x8  /* p9fs_node is in the session - virt_node_list */
#define IS_ROOT(node)	(node->flags & P9FS_ROOT)

#define P9FS_SET_LINKS(inode) do {	\
	(inode)->i_links_count = 1;	\
} while (0)				\

#define P9FS_INCR_LINKS(inode) do {	\
	(inode)->i_links_count++;	\
} while (0)				\

#define P9FS_DECR_LINKS(inode) do {	\
	(inode)->i_links_count--;	\
} while (0)				\

#define P9FS_CLR_LINKS(inode) do {	\
	(inode)->i_links_count = 0;	\
} while (0)				\

#define P9FS_MTX(_sc) (&(_sc)->p9fs_mtx)
#define P9FS_LOCK(_sc) mtx_lock(P9FS_MTX(_sc))
#define P9FS_UNLOCK(_sc) mtx_unlock(P9FS_MTX(_sc))
#define P9FS_LOCK_INIT(_sc) mtx_init(P9FS_MTX(_sc), \
    "P9FS session chain lock", NULL, MTX_DEF)
#define P9FS_LOCK_DESTROY(_sc) mtx_destroy(P9FS_MTX(_sc))

/* Session structure for the FS */
struct p9fs_session {
	unsigned char flags;				/* these flags for the session */
	struct mount *p9fs_mount;			/* mount point */
	struct p9fs_node rnp;				/* root p9fs node for this session */
	uid_t uid;					/* the uid that has access */
	const char *uname;				/* user name to mount as */
	const char *aname;				/* name of remote file tree being mounted */
	struct p9_client *clnt;				/* 9p client */
	struct mtx p9fs_mtx;				/* mutex used for guarding the chain.*/
	STAILQ_HEAD( ,p9fs_node) virt_node_list;	/* list of p9fs nodes in this session*/
	struct p9_fid *mnt_fid;				/* to save nobody 's fid for unmounting as root user */
};

struct p9fs_mount {
	struct p9fs_session p9fs_session;		/* per instance session information */
	struct mount *p9fs_mountp;			/* mount point */
	int mount_tag_len;				/* length of the mount tag */
	char *mount_tag;				/* mount tag used */
};

/* All session flags based on 9p versions  */
enum virt_session_flags {
	P9FS_PROTO_2000U	= 0x01,
	P9FS_PROTO_2000L	= 0x02,
};

/* Session access flags */
#define P9_ACCESS_ANY		0x04	/* single attach for all users */
#define P9_ACCESS_SINGLE	0x08	/* access to only the user who mounts */
#define P9_ACCESS_USER		0x10	/* new attach established for every user */
#define P9_ACCESS_MASK	(P9_ACCESS_ANY|P9_ACCESS_SINGLE|P9_ACCESS_USER)

u_quad_t p9fs_round_filesize_to_bytes(uint64_t filesize, uint64_t bsize);
u_quad_t p9fs_pow2_filesize_to_bytes(uint64_t filesize, uint64_t bsize);

/* These are all the P9FS specific vops */
int p9fs_stat_vnode_l(void);
int p9fs_stat_vnode_dotl(struct p9_stat_dotl *st, struct vnode *vp);
int p9fs_reload_stats_dotl(struct vnode *vp, struct ucred *cred);
int p9fs_proto_dotl(struct p9fs_session *vses);
struct p9_fid *p9fs_init_session(struct mount *mp, int *error);
void p9fs_close_session(struct mount *mp);
void p9fs_prepare_to_close(struct mount *mp);
void p9fs_complete_close(struct mount *mp);
int p9fs_vget(struct mount *mp, ino_t ino, int flags, struct vnode **vpp);
int p9fs_vget_common(struct mount *mp, struct p9fs_node *np, int flags,
    struct p9fs_node *parent, struct p9_fid *fid, struct vnode **vpp,
    char *name);
int p9fs_node_cmp(struct vnode *vp, void *arg);
void p9fs_destroy_node(struct p9fs_node **npp);
void p9fs_dispose_node(struct p9fs_node **npp);
void p9fs_cleanup(struct p9fs_node *vp);
void p9fs_fid_remove_all(struct p9fs_node *np, int leave_ofids);
void p9fs_fid_remove(struct p9fs_node *np, struct p9_fid *vfid,
    int fid_type);
void p9fs_fid_add(struct p9fs_node *np, struct p9_fid *fid,
    int fid_type);
struct p9_fid *p9fs_get_fid(struct p9_client *clnt,
    struct p9fs_node *np, struct ucred *cred, int fid_type, int mode, int *error);

#endif /* FS_P9FS_P9FS_H */
