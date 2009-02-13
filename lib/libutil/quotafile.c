/*-
 * Copyright (c) 2008 Dag-Erling Coïdan Smørgrav
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
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
 *
 * $FreeBSD$
 */

#include <sys/types.h>
#include <sys/endian.h>
#include <sys/mount.h>
#include <sys/stat.h>

#include <ufs/ufs/quota.h>

#include <errno.h>
#include <fcntl.h>
#include <fstab.h>
#include <grp.h>
#include <pwd.h>
#include <libutil.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct quotafile {
	int fd;
	int type; /* 32 or 64 */
};

static const char *qfextension[] = INITQFNAMES;

struct quotafile *
quota_open(const char *fn)
{
	struct quotafile *qf;
	struct dqhdr64 dqh;
	int serrno;

	if ((qf = calloc(1, sizeof(*qf))) == NULL)
		return (NULL);
	if ((qf->fd = open(fn, O_RDWR)) < 0) {
		serrno = errno;
		free(qf);
		errno = serrno;
		return (NULL);
	}
	qf->type = 32;
	switch (read(qf->fd, &dqh, sizeof(dqh))) {
	case -1:
		serrno = errno;
		close(qf->fd);
		free(qf);
		errno = serrno;
		return (NULL);
	case sizeof(dqh):
		if (strcmp(dqh.dqh_magic, Q_DQHDR64_MAGIC) != 0) {
			/* no magic, assume 32 bits */
			qf->type = 32;
			return (qf);
		}
		if (be32toh(dqh.dqh_version) != Q_DQHDR64_VERSION ||
		    be32toh(dqh.dqh_hdrlen) != sizeof(struct dqhdr64) ||
		    be32toh(dqh.dqh_reclen) != sizeof(struct dqblk64)) {
			/* correct magic, wrong version / lengths */
			close(qf->fd);
			free(qf);
			errno = EINVAL;
			return (NULL);
		}
		qf->type = 64;
		return (qf);
	default:
		qf->type = 32;
		return (qf);
	}
	/* not reached */
}

struct quotafile *
quota_create(const char *fn)
{
	struct quotafile *qf;
	struct dqhdr64 dqh;
	struct group *grp;
	int serrno;

	if ((qf = calloc(1, sizeof(*qf))) == NULL)
		return (NULL);
	if ((qf->fd = open(fn, O_RDWR|O_CREAT|O_TRUNC, 0)) < 0) {
		serrno = errno;
		free(qf);
		errno = serrno;
		return (NULL);
	}
	qf->type = 64;
	memset(&dqh, 0, sizeof(dqh));
	memcpy(dqh.dqh_magic, Q_DQHDR64_MAGIC, sizeof(dqh.dqh_magic));
	dqh.dqh_version = htobe32(Q_DQHDR64_VERSION);
	dqh.dqh_hdrlen = htobe32(sizeof(struct dqhdr64));
	dqh.dqh_reclen = htobe32(sizeof(struct dqblk64));
	if (write(qf->fd, &dqh, sizeof(dqh)) != sizeof(dqh)) {
		serrno = errno;
		unlink(fn);
		close(qf->fd);
		free(qf);
		errno = serrno;
		return (NULL);
	}
	grp = getgrnam(QUOTAGROUP);
	fchown(qf->fd, 0, grp ? grp->gr_gid : 0);
	fchmod(qf->fd, 0640);
	return (qf);
}

void
quota_close(struct quotafile *qf)
{

	close(qf->fd);
	free(qf);
}

static int
quota_read32(struct quotafile *qf, struct dqblk *dqb, int id)
{
	struct dqblk32 dqb32;
	off_t off;

	off = id * sizeof(struct dqblk32);
	if (lseek(qf->fd, off, SEEK_SET) == -1)
		return (-1);
	switch (read(qf->fd, &dqb32, sizeof(dqb32))) {
	case 0:
		memset(&dqb, 0, sizeof(*dqb));
		return (0);
	case sizeof(dqb32):
		dqb->dqb_bhardlimit = dqb32.dqb_bhardlimit;
		dqb->dqb_bsoftlimit = dqb32.dqb_bsoftlimit;
		dqb->dqb_curblocks = dqb32.dqb_curblocks;
		dqb->dqb_ihardlimit = dqb32.dqb_ihardlimit;
		dqb->dqb_isoftlimit = dqb32.dqb_isoftlimit;
		dqb->dqb_curinodes = dqb32.dqb_curinodes;
		dqb->dqb_btime = dqb32.dqb_btime;
		dqb->dqb_itime = dqb32.dqb_itime;
		return (0);
	default:
		return (-1);
	}
}

static int
quota_read64(struct quotafile *qf, struct dqblk *dqb, int id)
{
	struct dqblk64 dqb64;
	off_t off;

	off = sizeof(struct dqhdr64) + id * sizeof(struct dqblk64);
	if (lseek(qf->fd, off, SEEK_SET) == -1)
		return (-1);
	switch (read(qf->fd, &dqb64, sizeof(dqb64))) {
	case 0:
		memset(&dqb, 0, sizeof(*dqb));
		return (0);
	case sizeof(dqb64):
		dqb->dqb_bhardlimit = be64toh(dqb64.dqb_bhardlimit);
		dqb->dqb_bsoftlimit = be64toh(dqb64.dqb_bsoftlimit);
		dqb->dqb_curblocks = be64toh(dqb64.dqb_curblocks);
		dqb->dqb_ihardlimit = be64toh(dqb64.dqb_ihardlimit);
		dqb->dqb_isoftlimit = be64toh(dqb64.dqb_isoftlimit);
		dqb->dqb_curinodes = be64toh(dqb64.dqb_curinodes);
		dqb->dqb_btime = be64toh(dqb64.dqb_btime);
		dqb->dqb_itime = be64toh(dqb64.dqb_itime);
		return (0);
	default:
		return (-1);
	}
}

int
quota_read(struct quotafile *qf, struct dqblk *dqb, int id)
{

	switch (qf->type) {
	case 32:
		return quota_read32(qf, dqb, id);
	case 64:
		return quota_read64(qf, dqb, id);
	default:
		errno = EINVAL;
		return (-1);
	}
	/* not reached */
}

#define CLIP32(u64) ((u64) > UINT32_MAX ? UINT32_MAX : (uint32_t)(u64))

static int
quota_write32(struct quotafile *qf, const struct dqblk *dqb, int id)
{
	struct dqblk32 dqb32;
	off_t off;

	dqb32.dqb_bhardlimit = CLIP32(dqb->dqb_bhardlimit);
	dqb32.dqb_bsoftlimit = CLIP32(dqb->dqb_bsoftlimit);
	dqb32.dqb_curblocks = CLIP32(dqb->dqb_curblocks);
	dqb32.dqb_ihardlimit = CLIP32(dqb->dqb_ihardlimit);
	dqb32.dqb_isoftlimit = CLIP32(dqb->dqb_isoftlimit);
	dqb32.dqb_curinodes = CLIP32(dqb->dqb_curinodes);
	dqb32.dqb_btime = CLIP32(dqb->dqb_btime);
	dqb32.dqb_itime = CLIP32(dqb->dqb_itime);

	off = id * sizeof(struct dqblk32);
	if (lseek(qf->fd, off, SEEK_SET) == -1)
		return (-1);
	return (write(qf->fd, &dqb32, sizeof(dqb32)) == sizeof(dqb32));
}

static int
quota_write64(struct quotafile *qf, const struct dqblk *dqb, int id)
{
	struct dqblk64 dqb64;
	off_t off;

	dqb64.dqb_bhardlimit = htobe64(dqb->dqb_bhardlimit);
	dqb64.dqb_bsoftlimit = htobe64(dqb->dqb_bsoftlimit);
	dqb64.dqb_curblocks = htobe64(dqb->dqb_curblocks);
	dqb64.dqb_ihardlimit = htobe64(dqb->dqb_ihardlimit);
	dqb64.dqb_isoftlimit = htobe64(dqb->dqb_isoftlimit);
	dqb64.dqb_curinodes = htobe64(dqb->dqb_curinodes);
	dqb64.dqb_btime = htobe64(dqb->dqb_btime);
	dqb64.dqb_itime = htobe64(dqb->dqb_itime);

	off = sizeof(struct dqhdr64) + id * sizeof(struct dqblk64);
	if (lseek(qf->fd, off, SEEK_SET) == -1)
		return (-1);
	return (write(qf->fd, &dqb64, sizeof(dqb64)) == sizeof(dqb64));
}

int
quota_write(struct quotafile *qf, const struct dqblk *dqb, int id)
{

	switch (qf->type) {
	case 32:
		return quota_write32(qf, dqb, id);
	case 64:
		return quota_write64(qf, dqb, id);
	default:
		errno = EINVAL;
		return (-1);
	}
	/* not reached */
}

/*
 * Check to see if a particular quota is to be enabled.
 */
int
hasquota(struct fstab *fs, int type, char **qfnamep)
{
	char *opt;
	char *cp;
	struct statfs sfb;
	static char initname, usrname[100], grpname[100];
	static char buf[BUFSIZ];

	if (!initname) {
		(void)snprintf(usrname, sizeof(usrname), "%s%s",
		    qfextension[USRQUOTA], QUOTAFILENAME);
		(void)snprintf(grpname, sizeof(grpname), "%s%s",
		    qfextension[GRPQUOTA], QUOTAFILENAME);
		initname = 1;
	}
	strcpy(buf, fs->fs_mntops);
	for (opt = strtok(buf, ","); opt; opt = strtok(NULL, ",")) {
		if ((cp = index(opt, '=')))
			*cp++ = '\0';
		if (type == USRQUOTA && strcmp(opt, usrname) == 0)
			break;
		if (type == GRPQUOTA && strcmp(opt, grpname) == 0)
			break;
	}
	if (!opt)
		return (0);
	if (cp)
		*qfnamep = cp;
	else {
		(void)snprintf(buf, sizeof(buf), "%s/%s.%s", fs->fs_file,
		    QUOTAFILENAME, qfextension[type]);
		*qfnamep = buf;
	}
	/*
	 * Ensure that the filesystem is mounted.
	 */
	if (statfs(fs->fs_file, &sfb) != 0 ||
	    strcmp(fs->fs_file, sfb.f_mntonname)) {
		return (0);
	}
	return (1);
}
