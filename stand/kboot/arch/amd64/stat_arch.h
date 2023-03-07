/*
 * Copyright (c) 2005-2020 Rich Felker, et al.
 *
 * SPDX-License-Identifier: MIT
 *
 * Note: From the musl project
 */

typedef uint64_t host_nlink_t;

struct host_kstat {
	host_dev_t st_dev;
	host_ino_t st_ino;
	host_nlink_t st_nlink;

	host_mode_t st_mode;
	host_uid_t st_uid;
	host_gid_t st_gid;
	unsigned int    __pad0;
	host_dev_t st_rdev;
	host_off_t st_size;
	host_blksize_t st_blksize;
	host_blkcnt_t st_blocks;

	long st_atime_sec;
	long st_atime_nsec;
	long st_mtime_sec;
	long st_mtime_nsec;
	long st_ctime_sec;
	long st_ctime_nsec;
	long __pad_for_future[3];
};

