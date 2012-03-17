/*-
 * Copyright (c) 2010-2012 Semihalf.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/stdint.h>
#include <sys/ucred.h>
#include <sys/disk.h>
#include <sys/mount.h>

#include <fs/nandfs/nandfs_fs.h>
#include <libnandfs.h>

#define	NANDFS_IS_VALID		0x1
#define	NANDFS_IS_OPENED	0x2
#define	NANDFS_IS_OPENED_DEV	0x4
#define	NANDFS_IS_ERROR		0x8
#define	NANDFS_IS_LOCKED	0x10
#define	NANDFS_IS_READONLY	0x20

#define DEBUG
#undef DEBUG
#ifdef DEBUG
#define NANDFS_DEBUG(fmt, args...) do { \
    printf("libnandfs:" fmt "\n", ##args); } while (0)
#else
#define NANDFS_DEBUG(fmt, args...)
#endif

#define	NANDFS_ASSERT_VALID(fs)		assert((fs)->n_flags & NANDFS_IS_VALID)
#define	NANDFS_ASSERT_VALID_DEV(fs)	\
	assert(((fs)->n_flags & (NANDFS_IS_VALID | NANDFS_IS_OPENED_DEV)) == \
	    (NANDFS_IS_VALID | NANDFS_IS_OPENED_DEV))
#define NANDFS_ASSERT_RDWR(fs)		\
	assert(!((fs)->n_flags & NANDFS_IS_READONLY))
#define NANDFS_ASSERT_LOCKED(fs)	\
	assert((fs)->n_flags & NANDFS_IS_LOCKED)
#define NANDFS_ASSERT_UNLOCKED(fs)	\
	assert(!((fs)->n_flags & NANDFS_IS_LOCKED))

int
nandfs_iserror(struct nandfs *fs)
{

	NANDFS_ASSERT_VALID(fs);

	return (fs->n_flags & NANDFS_IS_ERROR);
}

const char *
nandfs_errmsg(struct nandfs *fs)
{

	NANDFS_ASSERT_VALID(fs);

	assert(nandfs_iserror(fs));
	assert(fs->n_errmsg);
	return (fs->n_errmsg);
}

static void
nandfs_seterr(struct nandfs *fs, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(fs->n_errmsg, sizeof(fs->n_errmsg), fmt, ap);
	va_end(ap);
	fs->n_flags |= NANDFS_IS_ERROR;
}

int
nandfs_cleanerd_set(struct nandfs *fs)
{

	NANDFS_ASSERT_VALID_DEV(fs);

	if (ioctl(fs->n_iocfd, NANDFS_IOCTL_CLEANERD_SET) == -1) {
		nandfs_seterr(fs, "%s", strerror(errno));
		return (-1);
	}

	return (0);
}

int
nandfs_cleanerd_unset(struct nandfs *fs)
{

	NANDFS_ASSERT_VALID_DEV(fs);

	if (ioctl(fs->n_iocfd, NANDFS_IOCTL_CLEANERD_UNSET) == -1) {
		nandfs_seterr(fs, "%s", strerror(errno));
		return (-1);
	}

	return (0);
}

int
nandfs_lock(struct nandfs *fs, int rdwr)
{
	struct flock lck;
	int error;

	NANDFS_ASSERT_VALID(fs);
	NANDFS_ASSERT_RDWR(fs);
	NANDFS_ASSERT_UNLOCKED(fs);

	lck.l_type = (rdwr ? F_WRLCK : F_RDLCK);
	lck.l_start = 0;
	lck.l_whence = SEEK_SET;
	lck.l_len = 1;

	error = fcntl(fs->n_iocfd, F_SETLKW, &lck);
	if (error == -1) {
		nandfs_seterr(fs, "couldn't lock %s: %s", fs->n_ioc,
		    strerror(errno));
		return (-1);
	}

	fs->n_flags |= NANDFS_IS_LOCKED;

	return (0);
}

int
nandfs_unlock(struct nandfs *fs)
{
	struct flock lck;
	int error;

	NANDFS_ASSERT_VALID(fs);
	NANDFS_ASSERT_RDWR(fs);
	NANDFS_ASSERT_LOCKED(fs);

	lck.l_type = F_UNLCK;
	lck.l_start = 0;
	lck.l_whence = SEEK_SET;
	lck.l_len = 1;

	error = fcntl(fs->n_iocfd, F_SETLK, &lck);
	if (error == -1) {
		nandfs_seterr(fs, "couldn't unlock %s: %s", fs->n_ioc,
		    strerror(errno));
		return (-1);
	}

	fs->n_flags &= ~NANDFS_IS_LOCKED;

	return (0);
}

const char *
nandfs_dev(struct nandfs *fs)
{

	NANDFS_ASSERT_VALID(fs);
	return (fs->n_dev);
}

void
nandfs_init(struct nandfs *fs, const char *dir)
{

	snprintf(fs->n_ioc, sizeof(fs->n_ioc), "%s/%s", dir, ".nandfs");
	fs->n_iocfd = -1;
	fs->n_flags = NANDFS_IS_VALID;
}

void
nandfs_destroy(struct nandfs *fs)
{

	assert(fs->n_iocfd == -1);
	fs->n_flags &=
	    ~(NANDFS_IS_ERROR | NANDFS_IS_VALID | NANDFS_IS_READONLY);
	assert(fs->n_flags == 0);
}

static int
_nandfs_open(struct nandfs *fs, int rdwr)
{
	struct nandfs_fsinfo fsinfo;
	int flags;

	fs->n_flags |= NANDFS_IS_OPENED;

	if (rdwr)
		flags = O_RDWR;
	else {
		fs->n_flags |= NANDFS_IS_READONLY;
		flags = O_RDONLY;
	}

	fs->n_iocfd = open(fs->n_ioc, flags, S_IRUSR | S_IWUSR | S_IRGRP |
	    S_IWGRP | S_IROTH | S_IWOTH);
	if (fs->n_iocfd == -1) {
		nandfs_seterr(fs, "couldn't open %s: %s", fs->n_ioc,
		    strerror(errno));
		return (-1);
	}

	if (ioctl(fs->n_iocfd, NANDFS_IOCTL_GET_FSINFO, &fsinfo) == -1) {
		nandfs_seterr(fs, "couldn't fetch fsinfo: %s",
		    strerror(errno));
		return (-1);
	}

	memcpy(&fs->n_fsdata, &fsinfo.fs_fsdata, sizeof(fs->n_fsdata));
	memcpy(&fs->n_sb, &fsinfo.fs_super, sizeof(fs->n_sb));
	snprintf(fs->n_dev, sizeof(fs->n_dev), fsinfo.fs_dev);

	return (0);
}

int
nandfs_open(struct nandfs *fs)
{

	return (_nandfs_open(fs, 0));
}

int
nandfs_open_rw(struct nandfs *fs)
{

	return (_nandfs_open(fs, 1));
}

int
nandfs_open_dev(struct nandfs *fs)
{

	fs->n_flags |= NANDFS_IS_OPENED_DEV;

	if (nandfs_open_rw(fs) == -1)
		return (-1);

	fs->n_devfd = open(fs->n_dev, O_RDONLY);
	if (fs->n_devfd == -1) {
		nandfs_seterr(fs, "couldn't open %s: %s", fs->n_dev,
		    strerror(errno));
		return (-1);
	}

	return (0);
}

void
nandfs_close(struct nandfs *fs)
{

	NANDFS_ASSERT_VALID(fs);
	assert(fs->n_flags & NANDFS_IS_OPENED);

	close(fs->n_iocfd);
	fs->n_iocfd = -1;
	fs->n_flags &= ~NANDFS_IS_OPENED;
}

void
nandfs_close_dev(struct nandfs *fs)
{

	assert(fs->n_flags & NANDFS_IS_OPENED_DEV);

	close(fs->n_devfd);
	fs->n_devfd = -1;
	fs->n_flags &= ~NANDFS_IS_OPENED_DEV;
	nandfs_close(fs);
}

static ssize_t
nandfs_get_cpinfo(struct nandfs *fs, uint64_t cno, int mode,
    struct nandfs_cpinfo *cpinfo, size_t nci)
{
	struct nandfs_argv args;

	NANDFS_ASSERT_VALID(fs);

	args.nv_base = (u_long)cpinfo;
	args.nv_nmembs = nci;
	args.nv_index = cno;
	args.nv_flags = mode;

	if (ioctl(fs->n_iocfd, NANDFS_IOCTL_GET_CPINFO, &args) == -1) {
		nandfs_seterr(fs, "ioctl NANDFS_IOCTL_GET_CPINFO: %s",
		    strerror(errno));
		return (-1);
	}

	return (args.nv_nmembs);
}

ssize_t
nandfs_get_cp(struct nandfs *fs, uint64_t cno, struct nandfs_cpinfo *cpinfo,
    size_t nci)
{

	return (nandfs_get_cpinfo(fs, cno, NANDFS_CHECKPOINT, cpinfo, nci));
}

ssize_t
nandfs_get_snap(struct nandfs *fs, uint64_t cno, struct nandfs_cpinfo *cpinfo,
    size_t nci)
{

	return (nandfs_get_cpinfo(fs, cno, NANDFS_SNAPSHOT, cpinfo, nci));
}

int
nandfs_make_snap(struct nandfs *fs, uint64_t *cno)
{

	NANDFS_ASSERT_VALID(fs);
	NANDFS_ASSERT_RDWR(fs);
	NANDFS_ASSERT_LOCKED(fs);

	if (ioctl(fs->n_iocfd, NANDFS_IOCTL_MAKE_SNAP, cno) == -1) {
		nandfs_seterr(fs, "ioctl NANDFS_IOCTL_MAKE_SNAP: %s",
		    strerror(errno));
		return (-1);
	}

	return (0);
}

int
nandfs_delete_snap(struct nandfs *fs, uint64_t cno)
{

	NANDFS_ASSERT_VALID(fs);
	NANDFS_ASSERT_RDWR(fs);
	NANDFS_ASSERT_LOCKED(fs);

	if (ioctl(fs->n_iocfd, NANDFS_IOCTL_DELETE_SNAP, &cno) == -1) {
		nandfs_seterr(fs, "ioctl NANDFS_IOCTL_DELETE_SNAP: %s",
		    strerror(errno));
		return (-1);
	}

	return (0);
}
