/*-
 * Copyright (c) 2008 Peter Holm <pho@FreeBSD.org>
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
 *
 */

/* Get various resource limits for the tests */

#include <sys/types.h>
#include <sys/sysctl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <kvm.h>
#include <vm/vm_param.h>
#include <errno.h>
#include <err.h>
#include <stdarg.h>
#include <libutil.h>

#include "stress.h"

static int lockfd;
static int dffd;
static int flags;
static char lockpath[128];
static char dfpath[128];

static int64_t
inodes(void)
{
	char path[MAXPATHLEN+1];
	struct statfs buf;

	if (op->inodes != 0)
		return (op->inodes);
	if (getcwd(path, sizeof(path)) == NULL)
		err(1, "getcwd()");

	if (statfs(path, &buf) < 0)
		err(1, "statfs(%s)", path);
	if (!strcmp(buf.f_fstypename, "msdosfs"))
			buf.f_ffree = 9999;
	flags = buf.f_flags & MNT_VISFLAGMASK;
	if (op->verbose > 2)
		printf("Free inodes on %s (%s): %jd\n", path,
		    buf.f_mntonname, buf.f_ffree);
	return (buf.f_ffree);
}

static int64_t
df(void)
{
	char path[MAXPATHLEN+1];
	struct statfs buf;

	if (op->kblocks != 0)
		return (op->kblocks * (uint64_t)1024);

	if (getcwd(path, sizeof(path)) == NULL)
		err(1, "getcwd()");

	if (statfs(path, &buf) < 0)
		err(1, "statfs(%s)", path);
	if (buf.f_bavail > (int64_t)buf.f_blocks || buf.f_bavail < 0) {
		warnx("Corrupt statfs(%s). f_bavail = %jd!", path,
		    buf.f_bavail);
		buf.f_bavail = 100;
	}
	if (op->verbose > 2)
		printf("Free space on %s: %jd Mb\n", path, buf.f_bavail *
		    buf.f_bsize / 1024 / 1024);
	return (buf.f_bavail * buf.f_bsize);
}

int64_t
swap(void)
{
	struct xswdev xsw;
	size_t mibsize, size;
	int mib[16], n;
	int64_t sz;

	mibsize = sizeof mib / sizeof mib[0];
	sz = 0;

	if (sysctlnametomib("vm.swap_info", mib, &mibsize) == -1)
		err(1, "sysctlnametomib()");

	for (n = 0; ; ++n) {
		mib[mibsize] = n;
		size = sizeof xsw;
		if (sysctl(mib, mibsize + 1, &xsw, &size, NULL, 0) == -1)
			break;
		if (xsw.xsw_version != XSWDEV_VERSION)
			errx(1, "xswdev version mismatch");
		sz = sz + xsw.xsw_nblks - xsw.xsw_used;
	}
	if (errno != ENOENT)
		err(1, "sysctl()");

	if (op->verbose > 2)
		printf("Total free swap space %jd Mb\n",
			sz * getpagesize() / 1024 / 1024);

	return (sz * getpagesize());
}

unsigned long
usermem(void)
{
	unsigned long mem;
	size_t nlen = sizeof(mem);

	if (sysctlbyname("hw.usermem", &mem, &nlen, NULL, 0) == -1)
		err(1, "sysctlbyname() %s:%d", __FILE__, __LINE__);

	if (op->verbose > 2)
		printf("Total free user memory %lu Mb\n",
			mem / 1024 / 1024);

	return (mem);
}

static void
cleanupdf(void)
{
	unlink(dfpath);
}

void
getdf(int64_t *block, int64_t *inode)
{
	int i, j;
	char buf[128];

	snprintf(lockpath, sizeof(lockpath), "%s/lock", op->cd);
	for (j = 0; j < 2; j++) {
		for (i = 0; i < 10000; i++) {
			if ((lockfd = open(lockpath,
			    O_CREAT | O_TRUNC | O_WRONLY |
			    O_EXCL, 0644)) != -1)
				break;
			usleep(10000); /* sleep 1/100 sec */
			if (i > 0 && i % 1000 == 0)
				fprintf(stderr, "%s is waiting for lock file"
				    " %s\n",
				    getprogname(), lockpath);
		}
		if (lockfd != -1)
			break;
		fprintf(stderr, "%s. Removing stale %s\n", getprogname(),
		    lockpath);
		unlink(lockpath);
	}
	if (lockfd == -1)
		errx(1, "%s. Can not create %s\n", getprogname(), lockpath);
	snprintf(dfpath, sizeof(dfpath), "%s/df", op->cd);
	if ((dffd = open(dfpath, O_RDWR, 0644)) == -1) {
		if ((dffd = open(dfpath,
				O_CREAT | O_TRUNC | O_WRONLY, 0644)) == -1) {
			unlink(lockpath);
			err(1, "creat(%s) %s:%d", dfpath, __FILE__,
			    __LINE__);
		}
		atexit(cleanupdf);
		*block = df();
		*inode = inodes();
		snprintf(buf, sizeof(buf), "%jd %jd", *block, *inode);

		if (write(dffd, buf, strlen(buf) + 1) !=
		    (ssize_t)strlen(buf) +1)
			err(1, "write df. %s:%d", __FILE__, __LINE__);
	} else {
		if (read(dffd, buf, sizeof(buf)) < 1) {
			system("ls -l /tmp/stressX.control");
			unlink(lockpath);
			err(1, "read df. %s:%d", __FILE__, __LINE__);
		}
		sscanf(buf, "%jd %jd", block, inode);
	}
	close(dffd);
}

void
reservedf(int64_t blks, int64_t inos)
{
	char buf[128];
	int64_t blocks, inodes;

	if ((dffd = open(dfpath, O_RDWR, 0644)) == -1) {
		warn("open(%s) %s:%d. %s", dfpath, __FILE__, __LINE__,
		    getprogname());
		goto err;
	}
	if (read(dffd, buf, sizeof(buf)) < 1) {
		warn("read df. %s:%d", __FILE__, __LINE__);
		goto err;
	}
	sscanf(buf, "%jd %jd", &blocks, &inodes);

	if (op->verbose > 2)
		printf("%-8s: reservefd(%9jdK, %6jd) out of (%9jdK, %6jd)\n",
				getprogname(), blks/1024, inos, blocks/1024,
				inodes);
	blocks -= blks;
	inodes -= inos;

	snprintf(buf, sizeof(buf), "%jd %jd", blocks, inodes);
	if (blocks < 0 || inodes < 0)
		printf("******************************** %s: %s\n",
		    getprogname(), buf);
	if (lseek(dffd, 0, 0) == -1)
		err(1, "lseek. %s:%d", __FILE__, __LINE__);
	if (write(dffd, buf, strlen(buf) + 1) != (ssize_t)strlen(buf) +1)
		warn("write df. %s:%d", __FILE__, __LINE__);
err:
	close(dffd);
	close(lockfd);
	if (unlink(lockpath) == -1)
		err(1, "unlink(%s)", lockpath);
}
