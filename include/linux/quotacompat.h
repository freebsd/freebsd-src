/*
 *	Definition of symbols used for backward compatible interface
 */

#ifndef _LINUX_QUOTACOMPAT_
#define _LINUX_QUOTACOMPAT_

#include <linux/types.h>
#include <linux/quota.h>

struct v1c_mem_dqblk {
	__u32 dqb_bhardlimit;	/* absolute limit on disk blks alloc */
	__u32 dqb_bsoftlimit;	/* preferred limit on disk blks */
	__u32 dqb_curblocks;	/* current block count */
	__u32 dqb_ihardlimit;	/* maximum # allocated inodes */
	__u32 dqb_isoftlimit;	/* preferred inode limit */
	__u32 dqb_curinodes;	/* current # allocated inodes */
	time_t dqb_btime;	/* time limit for excessive disk use */
	time_t dqb_itime;	/* time limit for excessive files */
};

struct v1c_dqstats {
	__u32 lookups;
	__u32 drops;
	__u32 reads;
	__u32 writes;
	__u32 cache_hits;
	__u32 allocated_dquots;
	__u32 free_dquots;
	__u32 syncs;
};

struct v2c_mem_dqblk {
	unsigned int dqb_ihardlimit;
	unsigned int dqb_isoftlimit;
	unsigned int dqb_curinodes;
	unsigned int dqb_bhardlimit;
	unsigned int dqb_bsoftlimit;
	qsize_t dqb_curspace;
	__kernel_time_t dqb_btime;
	__kernel_time_t dqb_itime;
};

struct v2c_mem_dqinfo {
	unsigned int dqi_bgrace;
	unsigned int dqi_igrace;
	unsigned int dqi_flags;
	unsigned int dqi_blocks;
	unsigned int dqi_free_blk;
	unsigned int dqi_free_entry;
};

struct v2c_dqstats {
	__u32 lookups;
	__u32 drops;
	__u32 reads;
	__u32 writes;
	__u32 cache_hits;
	__u32 allocated_dquots;
	__u32 free_dquots;
	__u32 syncs;
	__u32 version;
};

#define Q_COMP_QUOTAON  0x0100	/* enable quotas */
#define Q_COMP_QUOTAOFF 0x0200	/* disable quotas */
#define Q_COMP_SYNC     0x0600	/* sync disk copy of a filesystems quotas */

#define Q_V1_GETQUOTA 0x0300	/* get limits and usage */
#define Q_V1_SETQUOTA 0x0400	/* set limits and usage */
#define Q_V1_SETUSE   0x0500	/* set usage */
#define Q_V1_SETQLIM  0x0700	/* set limits */
#define Q_V1_GETSTATS 0x0800	/* get collected stats */
#define Q_V1_RSQUASH  0x1000	/* set root_squash option */

#define Q_V2_SETQLIM  0x0700	/* set limits */
#define Q_V2_GETINFO  0x0900	/* get info about quotas - graces, flags... */
#define Q_V2_SETINFO  0x0A00	/* set info about quotas */
#define Q_V2_SETGRACE 0x0B00	/* set inode and block grace */
#define Q_V2_SETFLAGS 0x0C00	/* set flags for quota */
#define Q_V2_GETQUOTA 0x0D00	/* get limits and usage */
#define Q_V2_SETQUOTA 0x0E00	/* set limits and usage */
#define Q_V2_SETUSE   0x0F00	/* set usage */
#define Q_V2_GETSTATS 0x1100	/* get collected stats */

#endif
