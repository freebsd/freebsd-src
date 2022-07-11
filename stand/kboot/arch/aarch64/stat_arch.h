/*
 * Copyright (c) 2005-2020 Rich Felker, et al.
 *
 * SPDX-License-Identifier: MIT
 *
 * Note: From the musl project
 */

struct host_kstat {
	host_dev_t st_dev;
	host_ino_t st_ino;
	host_mode_t st_mode;
	host_nlink_t st_nlink;
	host_uid_t st_uid;
	host_gid_t st_gid;
	host_dev_t st_rdev;
	unsigned long __pad;
	host_off_t st_size;
	host_blksize_t st_blksize;
	int __pad2;
	host_blkcnt_t st_blocks;
	long st_atime_sec;
	long st_atime_nsec;
	long st_mtime_sec;
	long st_mtime_nsec;
	long st_ctime_sec;
	long st_ctime_nsec;
	unsigned __pad_for_future[2];
};
