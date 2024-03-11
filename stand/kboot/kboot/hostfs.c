/*-
 * Copyright (c) 2022 Netflix, Inc
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/types.h>
#include "stand.h"
#include "host_syscall.h"
#include "kboot.h"

#define HOST_PATH_MAX	1025

extern struct devsw host_dev;

const char *hostfs_root = "/";

enum FTYPE {
	regular,
	dir,
};

typedef struct _hostfs_file {
	enum FTYPE	hf_type;
	int		hf_fd;
	/* The following are only used for FTYPE == dir */
	char		hf_dents[2048];
	char		*hf_curdent;
	int		hf_dentlen;	/* Valid part of hf_dents */
} hostfs_file;

static hostfs_file *
hostfs_alloc(void)
{
	hostfs_file *hf;

	hf = malloc(sizeof(*hf));
	if (hf != NULL)
		memset(hf, 0, sizeof(*hf));
	return (hf);
}

static void
hostfs_free(hostfs_file *hf)
{
	free(hf);
}

static int
hostfs_open(const char *fn, struct open_file *f)
{
	hostfs_file *hf;
	struct host_kstat ksb;
	char path[HOST_PATH_MAX];

	if (f->f_dev != &host_dev) {
		return (EINVAL);
	}

	/*
	 * Normally, we root everything at hostfs_root. However, there are two
	 * exceptions that make it easier to write code. First is /sys and /proc
	 * are special Linux filesystems, so we pass those paths
	 * through. Second, if the path starts with //, then we strip off the
	 * first / and pass it through (in a weird way, this is actually in
	 * POSIX: hosts are allowed to do specail things with paths that start
	 * with two //, but one or three or more are required to be treated as
	 * one).
	 */
	if (strncmp("/sys/", fn, 5) == 0 || strncmp("/proc/", fn, 6) == 0)
		strlcpy(path, fn, sizeof(path));
	else if (fn[0] == '/' && fn[1] == '/' && fn[2] != '/')
		strlcpy(path, fn + 1, sizeof(path));
	else
		snprintf(path, sizeof(path), "%s/%s", hostfs_root, fn);
	hf = hostfs_alloc();
	hf->hf_fd = host_open(path, HOST_O_RDONLY, 0);
	if (hf->hf_fd < 0) {
		hostfs_free(hf);
		return (EINVAL);
	}

	if (host_fstat(hf->hf_fd, &ksb) < 0) {
		hostfs_free(hf);
		return (EINVAL);
	}
	if (S_ISDIR(hf->hf_fd)) {
		hf->hf_type = dir;
	} else {
		hf->hf_type = regular;
	}
	f->f_fsdata = hf;
	return (0);
}

static int
hostfs_close(struct open_file *f)
{
	hostfs_file *hf = f->f_fsdata;

	host_close(hf->hf_fd);
	hostfs_free(hf);
	f->f_fsdata = NULL;

	return (0);
}

static int
hostfs_read(struct open_file *f, void *start, size_t size, size_t *resid)
{
	hostfs_file *hf = f->f_fsdata;
	ssize_t sz;

	sz = host_read(hf->hf_fd, start, size);
	if (sz < 0)
		return (host_to_stand_errno(sz));
	*resid = size - sz;

	return (0);
}

static off_t
hostfs_seek(struct open_file *f, off_t offset, int whence)
{
	hostfs_file *hf = f->f_fsdata;
	uint32_t offl, offh;
	int err;
	uint64_t res;

	/*
	 * Assumes Linux host with 'reduced' system call wrappers. Also assume
	 * host and libstand have same whence encoding (safe since it all comes
	 * from V7 later ISO-C). Also assumes we have to support powerpc still,
	 * it's interface is weird for legacy reasons....
	 */
	res = (uint64_t)offset;
	offl = res & 0xfffffffful;
	offh = (res >> 32) & 0xfffffffful;
	err = host_llseek(hf->hf_fd, offh, offl, &res, whence);
	if (err < 0)
		return (err);
	return (res);
}

static int
hostfs_stat(struct open_file *f, struct stat *sb)
{
	struct host_kstat ksb;
	hostfs_file *hf = f->f_fsdata;

	if (host_fstat(hf->hf_fd, &ksb) < 0)
		return (EINVAL);
	/*
	 * Translate Linux stat info to lib stand's notion (which uses FreeBSD's
	 * stat structure, missing fields are zero and commented below).
	 */
	memset(sb, 0, sizeof(*sb));
	sb->st_dev		= ksb.st_dev;
	sb->st_ino		= ksb.st_ino;
	sb->st_nlink		= ksb.st_nlink;
	sb->st_mode		= ksb.st_mode;
	sb->st_uid		= ksb.st_uid;
	sb->st_gid		= ksb.st_gid;
	sb->st_rdev		= ksb.st_rdev;
	/* No st_?time_ext on i386 */
	sb->st_atim.tv_sec	= ksb.st_atime_sec;
	sb->st_atim.tv_nsec	= ksb.st_atime_nsec;
	sb->st_mtim.tv_sec	= ksb.st_mtime_sec;
	sb->st_mtim.tv_nsec	= ksb.st_mtime_nsec;
	sb->st_ctim.tv_sec	= ksb.st_ctime_sec;
	sb->st_ctim.tv_nsec	= ksb.st_ctime_nsec;
	/* No st_birthtim */
	sb->st_size		= ksb.st_size;
	sb->st_blocks		= ksb.st_blocks;
	sb->st_blksize		= ksb.st_blksize;
	/* no st_flags */
	/* no st_get */

	return (0);
}

static int
hostfs_readdir(struct open_file *f, struct dirent *d)
{
	hostfs_file *hf = f->f_fsdata;
	int dentlen;
	struct host_dirent64 *dent;

	if (hf->hf_curdent == NULL) {
		dentlen = host_getdents64(hf->hf_fd, hf->hf_dents, sizeof(hf->hf_dents));
		if (dentlen <= 0)
			return (EINVAL);
		hf->hf_dentlen = dentlen;
		hf->hf_curdent = hf->hf_dents;
	}
	dent = (struct host_dirent64 *)hf->hf_curdent;
	d->d_fileno = dent->d_ino;
	d->d_type = dent->d_type;	/* HOST_DT_XXXX == DX_XXXX for all values */
	strlcpy(d->d_name, dent->d_name, sizeof(d->d_name)); /* d_name is NUL terminated */
	d->d_namlen = strlen(d->d_name);
	hf->hf_curdent += dent->d_reclen;
	if (hf->hf_curdent >= hf->hf_dents + hf->hf_dentlen) {
		hf->hf_curdent = NULL;
		hf->hf_dentlen = 0;
	}

	return (0);
}

struct fs_ops hostfs_fsops = {
	.fs_name = "host",
	.fo_open = hostfs_open,
	.fo_close = hostfs_close,
	.fo_read = hostfs_read,
	.fo_write = null_write,
	.fo_seek = hostfs_seek,
	.fo_stat = hostfs_stat,
	.fo_readdir = hostfs_readdir
};

/*
 * Generic "null" host device. This goes hand and hand with the host fs object
 *
 * XXXX This and userboot for bhyve both use exactly the same code, modulo some
 * formatting nits. Make them common. We mostly use it to 'gate' the open of the
 * filesystem to only this device.
 */

static int
host_dev_init(void)
{
	return (0);
}

static int
host_dev_print(int verbose)
{
	char line[80];

	printf("%s devices:", host_dev.dv_name);
	if (pager_output("\n") != 0)
		return (1);

	snprintf(line, sizeof(line), "    host%d:   Host filesystem\n", 0);
	return (pager_output(line));
}

/*
 * 'Open' the host device.
 */
static int
host_dev_open(struct open_file *f, ...)
{
	return (0);
}

static int
host_dev_close(struct open_file *f)
{
	return (0);
}

static int
host_dev_strategy(void *devdata, int rw, daddr_t dblk, size_t size,
    char *buf, size_t *rsize)
{
	return (ENOSYS);
}

struct devsw host_dev = {
	.dv_name = "host",
	.dv_type = DEVT_NET,
	.dv_init = host_dev_init,
	.dv_strategy = host_dev_strategy,
	.dv_open = host_dev_open,
	.dv_close = host_dev_close,
	.dv_ioctl = noioctl,
	.dv_print = host_dev_print,
	.dv_cleanup = NULL
};
