/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Robert Elz at The University of Melbourne.
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
 *   This product includes software developed by the University of
 *   California, Berkeley and its contributors.
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
 * Version: $Id: quota.h,v 2.0 1996/11/17 16:48:14 mvw Exp mvw $
 */

#ifndef _LINUX_QUOTA_
#define _LINUX_QUOTA_

#include <linux/errno.h>
#include <linux/types.h>

#define __DQUOT_VERSION__	"dquot_6.5.1"
#define __DQUOT_NUM_VERSION__	6*10000+5*100+1

typedef __kernel_uid32_t qid_t; /* Type in which we store ids in memory */
typedef __u64 qsize_t;          /* Type in which we store sizes */

/* Size of blocks in which are counted size limits */
#define QUOTABLOCK_BITS 10
#define QUOTABLOCK_SIZE (1 << QUOTABLOCK_BITS)

/* Conversion routines from and to quota blocks */
#define qb2kb(x) ((x) << (QUOTABLOCK_BITS-10))
#define kb2qb(x) ((x) >> (QUOTABLOCK_BITS-10))
#define toqb(x) (((x) + QUOTABLOCK_SIZE - 1) >> QUOTABLOCK_BITS)

#define MAXQUOTAS 2
#define USRQUOTA  0		/* element used for user quotas */
#define GRPQUOTA  1		/* element used for group quotas */

/*
 * Definitions for the default names of the quotas files.
 */
#define INITQFNAMES { \
	"user",    /* USRQUOTA */ \
	"group",   /* GRPQUOTA */ \
	"undefined", \
};

/*
 * Command definitions for the 'quotactl' system call.
 * The commands are broken into a main command defined below
 * and a subcommand that is used to convey the type of
 * quota that is being manipulated (see above).
 */
#define SUBCMDMASK  0x00ff
#define SUBCMDSHIFT 8
#define QCMD(cmd, type)  (((cmd) << SUBCMDSHIFT) | ((type) & SUBCMDMASK))

#define Q_SYNC     0x800001	/* sync disk copy of a filesystems quotas */
#define Q_QUOTAON  0x800002	/* turn quotas on */
#define Q_QUOTAOFF 0x800003	/* turn quotas off */
#define Q_GETFMT   0x800004	/* get quota format used on given filesystem */
#define Q_GETINFO  0x800005	/* get information about quota files */
#define Q_SETINFO  0x800006	/* set information about quota files */
#define Q_GETQUOTA 0x800007	/* get user quota structure */
#define Q_SETQUOTA 0x800008	/* set user quota structure */

/*
 * Quota structure used for communication with userspace via quotactl
 * Following flags are used to specify which fields are valid
 */
#define QIF_BLIMITS	1
#define QIF_SPACE	2
#define QIF_ILIMITS	4
#define QIF_INODES	8
#define QIF_BTIME	16
#define QIF_ITIME	32
#define QIF_LIMITS	(QIF_BLIMITS | QIF_ILIMITS)
#define QIF_USAGE	(QIF_SPACE | QIF_INODES)
#define QIF_TIMES	(QIF_BTIME | QIF_ITIME)
#define QIF_ALL		(QIF_LIMITS | QIF_USAGE | QIF_TIMES)

struct if_dqblk {
	__u64 dqb_bhardlimit;
	__u64 dqb_bsoftlimit;
	__u64 dqb_curspace;
	__u64 dqb_ihardlimit;
	__u64 dqb_isoftlimit;
	__u64 dqb_curinodes;
	__u64 dqb_btime;
	__u64 dqb_itime;
	__u32 dqb_valid;
};

/*
 * Structure used for setting quota information about file via quotactl
 * Following flags are used to specify which fields are valid
 */
#define IIF_BGRACE	1
#define IIF_IGRACE	2
#define IIF_FLAGS	4
#define IIF_ALL		(IIF_BGRACE | IIF_IGRACE | IIF_FLAGS)

struct if_dqinfo {
	__u64 dqi_bgrace;
	__u64 dqi_igrace;
	__u32 dqi_flags;
	__u32 dqi_valid;
};

#ifdef __KERNEL__

#include <linux/dqblk_xfs.h>
#include <linux/dqblk_v1.h>
#include <linux/dqblk_v2.h>

/*
 * Data for one user/group kept in memory
 */
struct mem_dqblk {
	__u32 dqb_bhardlimit;	/* absolute limit on disk blks alloc */
	__u32 dqb_bsoftlimit;	/* preferred limit on disk blks */
	qsize_t dqb_curspace;	/* current used space */
	__u32 dqb_ihardlimit;	/* absolute limit on allocated inodes */
	__u32 dqb_isoftlimit;	/* preferred inode limit */
	__u32 dqb_curinodes;	/* current # allocated inodes */
	time_t dqb_btime;	/* time limit for excessive disk use */
	time_t dqb_itime;	/* time limit for excessive inode use */
};

/*
 * Data for one quotafile kept in memory
 */
struct quota_format_type;

struct mem_dqinfo {
	struct quota_format_type *dqi_format;
	int dqi_flags;
	unsigned int dqi_bgrace;
	unsigned int dqi_igrace;
	union {
		struct v1_mem_dqinfo v1_i;
		struct v2_mem_dqinfo v2_i;
	} u;
};

#define DQF_MASK 0xffff		/* Mask for format specific flags */
#define DQF_INFO_DIRTY 0x10000  /* Is info dirty? */
#define DQF_ANY_DQUOT_DIRTY 0x20000	/* Is any dquot dirty? */

extern inline void mark_info_dirty(struct mem_dqinfo *info)
{
	info->dqi_flags |= DQF_INFO_DIRTY;
}

#define info_dirty(info) ((info)->dqi_flags & DQF_INFO_DIRTY)

#define info_any_dirty(info) ((info)->dqi_flags & DQF_INFO_DIRTY ||\
			      (info)->dqi_flags & DQF_ANY_DQUOT_DIRTY)

#define sb_dqopt(sb) (&(sb)->s_dquot)

struct dqstats {
	int lookups;
	int drops;
	int reads;
	int writes;
	int cache_hits;
	int allocated_dquots;
	int free_dquots;
	int syncs;
};

extern struct dqstats dqstats;

#define NR_DQHASH 43            /* Just an arbitrary number */

#define DQ_LOCKED     0x01	/* dquot under IO */
#define DQ_MOD        0x02	/* dquot modified since read */
#define DQ_BLKS       0x10	/* uid/gid has been warned about blk limit */
#define DQ_INODES     0x20	/* uid/gid has been warned about inode limit */
#define DQ_FAKE       0x40	/* no limits only usage */
#define DQ_INVAL      0x80	/* dquot is going to be invalidated */

struct dquot {
	struct list_head dq_hash;	/* Hash list in memory */
	struct list_head dq_inuse;	/* List of all quotas */
	struct list_head dq_free;	/* Free list element */
	wait_queue_head_t dq_wait_lock;	/* Pointer to waitqueue on dquot lock */
	wait_queue_head_t dq_wait_free;	/* Pointer to waitqueue for quota to be unused */
	int dq_count;			/* Use count */
	int dq_dup_ref;			/* Number of duplicated refences */

	/* fields after this point are cleared when invalidating */
	struct super_block *dq_sb;	/* superblock this applies to */
	unsigned int dq_id;		/* ID this applies to (uid, gid) */
	kdev_t dq_dev;			/* Device this applies to */
	loff_t dq_off;			/* Offset of dquot on disk */
	short dq_type;			/* Type of quota */
	short dq_flags;			/* See DQ_* */
	struct mem_dqblk dq_dqb;	/* Diskquota usage */
};

#define NODQUOT (struct dquot *)NULL

#define QUOTA_OK          0
#define NO_QUOTA          1

/* Operations which must be implemented by each quota format */
struct quota_format_ops {
	int (*check_quota_file)(struct super_block *sb, int type);	/* Detect whether file is in our format */
	int (*read_file_info)(struct super_block *sb, int type);	/* Read main info about file - called on quotaon() */
	int (*write_file_info)(struct super_block *sb, int type);	/* Write main info about file */
	int (*free_file_info)(struct super_block *sb, int type);	/* Called on quotaoff() */
	int (*read_dqblk)(struct dquot *dquot);		/* Read structure for one user */
	int (*commit_dqblk)(struct dquot *dquot);	/* Write (or delete) structure for one user */
};

/* Operations working with dquots */
struct dquot_operations {
	void (*initialize) (struct inode *, int);
	void (*drop) (struct inode *);
	int (*alloc_space) (struct inode *, qsize_t, int);
	int (*alloc_inode) (const struct inode *, unsigned long);
	void (*free_space) (struct inode *, qsize_t);
	void (*free_inode) (const struct inode *, unsigned long);
	int (*transfer) (struct inode *, struct iattr *);
	int (*write_dquot) (struct dquot *);
};

/* Operations handling requests from userspace */
struct quotactl_ops {
	int (*quota_on)(struct super_block *, int, int, char *);
	int (*quota_off)(struct super_block *, int);
	int (*quota_sync)(struct super_block *, int);
	int (*get_info)(struct super_block *, int, struct if_dqinfo *);
	int (*set_info)(struct super_block *, int, struct if_dqinfo *);
	int (*get_dqblk)(struct super_block *, int, qid_t, struct if_dqblk *);
	int (*set_dqblk)(struct super_block *, int, qid_t, struct if_dqblk *);
	int (*get_xstate)(struct super_block *, struct fs_quota_stat *);
	int (*set_xstate)(struct super_block *, unsigned int, int);
	int (*get_xquota)(struct super_block *, int, qid_t, struct fs_disk_quota *);
	int (*set_xquota)(struct super_block *, int, qid_t, struct fs_disk_quota *);
};

struct quota_format_type {
	int qf_fmt_id;	/* Quota format id */
	struct quota_format_ops *qf_ops;	/* Operations of format */
	struct module *qf_owner;		/* Module implementing quota format */
	struct quota_format_type *qf_next;
};

#define DQUOT_USR_ENABLED	0x01		/* User diskquotas enabled */
#define DQUOT_GRP_ENABLED	0x02		/* Group diskquotas enabled */

struct quota_info {
	unsigned int flags;			/* Flags for diskquotas on this device */
	struct semaphore dqio_sem;		/* lock device while I/O in progress */
	struct semaphore dqoff_sem;		/* serialize quota_off() and quota_on() on device */
	struct file *files[MAXQUOTAS];		/* fp's to quotafiles */
	struct mem_dqinfo info[MAXQUOTAS];	/* Information for each quota type */
	struct quota_format_ops *ops[MAXQUOTAS];	/* Operations for each type */
};

/* Inline would be better but we need to dereference super_block which is not defined yet */
#define mark_dquot_dirty(dquot) do {\
	dquot->dq_flags |= DQ_MOD;\
	sb_dqopt(dquot->dq_sb)->info[dquot->dq_type].dqi_flags |= DQF_ANY_DQUOT_DIRTY;\
} while (0)

#define dquot_dirty(dquot) ((dquot)->dq_flags & DQ_MOD)

static inline int is_enabled(struct quota_info *dqopt, int type)
{
	switch (type) {
		case USRQUOTA:
			return dqopt->flags & DQUOT_USR_ENABLED;
		case GRPQUOTA:
			return dqopt->flags & DQUOT_GRP_ENABLED;
	}
	return 0;
}

#define sb_any_quota_enabled(sb) (is_enabled(sb_dqopt(sb), USRQUOTA) | is_enabled(sb_dqopt(sb), GRPQUOTA))

#define sb_has_quota_enabled(sb, type) (is_enabled(sb_dqopt(sb), type))

int register_quota_format(struct quota_format_type *fmt);
void unregister_quota_format(struct quota_format_type *fmt);
void init_dquot_operations(struct dquot_operations *fsdqops);

#else

# /* nodep */ include <sys/cdefs.h>

__BEGIN_DECLS
long quotactl __P ((unsigned int, const char *, int, caddr_t));
__END_DECLS

#endif /* __KERNEL__ */
#endif /* _QUOTA_ */
