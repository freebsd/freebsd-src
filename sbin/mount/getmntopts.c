/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 */

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/uio.h>

#include <err.h>
#include <errno.h>
#include <paths.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mntopts.h"

int getmnt_silent = 0;

void
getmntopts(const char *options, const struct mntopt *m0, int *flagp,
	int *altflagp)
{
	const struct mntopt *m;
	int negative, len;
	char *opt, *optbuf, *p;
	int *thisflagp;

	/* Copy option string, since it is about to be torn asunder... */
	if ((optbuf = strdup(options)) == NULL)
		err(1, NULL);

	for (opt = optbuf; (opt = strtok(opt, ",")) != NULL; opt = NULL) {
		/* Check for "no" prefix. */
		if (opt[0] == 'n' && opt[1] == 'o') {
			negative = 1;
			opt += 2;
		} else
			negative = 0;

		/*
		 * for options with assignments in them (ie. quotas)
		 * ignore the assignment as it's handled elsewhere
		 */
		p = strchr(opt, '=');
		if (p != NULL)
			 *++p = '\0';

		/* Scan option table. */
		for (m = m0; m->m_option != NULL; ++m) {
			len = strlen(m->m_option);
			if (strncasecmp(opt, m->m_option, len) == 0)
				if (opt[len] == '\0' || opt[len] == '=')
					break;
		}

		/* Save flag, or fail if option is not recognized. */
		if (m->m_option) {
			thisflagp = m->m_altloc ? altflagp : flagp;
			if (negative == m->m_inverse)
				*thisflagp |= m->m_flag;
			else
				*thisflagp &= ~m->m_flag;
		} else if (!getmnt_silent) {
			errx(1, "-o %s: option not supported", opt);
		}
	}

	free(optbuf);
}

void
rmslashes(char *rrpin, char *rrpout)
{
	char *rrpoutstart;

	*rrpout = *rrpin;
	for (rrpoutstart = rrpout; *rrpin != '\0'; *rrpout++ = *rrpin++) {

		/* skip all double slashes */
		while (*rrpin == '/' && *(rrpin + 1) == '/')
			 rrpin++;
	}

	/* remove trailing slash if necessary */
	if (rrpout - rrpoutstart > 1 && *(rrpout - 1) == '/')
		*(rrpout - 1) = '\0';
	else
		*rrpout = '\0';
}

int
checkpath(const char *path, char *resolved)
{
	struct stat sb;

	if (realpath(path, resolved) == NULL || stat(resolved, &sb) != 0)
		return (1);
	if (!S_ISDIR(sb.st_mode)) {
		errno = ENOTDIR;
		return (1);
	}
	return (0);
}

int
checkpath_allow_file(const char *path, char *resolved)
{
	struct stat sb;

	if (realpath(path, resolved) == NULL || stat(resolved, &sb) != 0)
		return (1);
	if (!S_ISDIR(sb.st_mode) && !S_ISREG(sb.st_mode)) {
		errno = ENOTDIR;
		return (1);
	}
	return (0);
}

/*
 * Get the mount point information for name. Name may be mount point name
 * or device name (with or without /dev/ preprended).
 */
struct statfs *
getmntpoint(const char *name)
{
	struct stat devstat, mntdevstat;
	char device[sizeof(_PATH_DEV) - 1 + MNAMELEN];
	char *ddevname;
	struct statfs *mntbuf, *statfsp;
	int i, mntsize, isdev;
	u_long len;

	if (stat(name, &devstat) != 0)
		return (NULL);
	if (S_ISCHR(devstat.st_mode) || S_ISBLK(devstat.st_mode))
		isdev = 1;
	else
		isdev = 0;
	mntsize = getmntinfo(&mntbuf, MNT_NOWAIT);
	for (i = 0; i < mntsize; i++) {
		statfsp = &mntbuf[i];
		if (isdev == 0) {
			if (strcmp(name, statfsp->f_mntonname))
				continue;
			return (statfsp);
		}
		ddevname = statfsp->f_mntfromname;
		if (*ddevname != '/') {
			if ((len = strlen(_PATH_DEV) + strlen(ddevname) + 1) >
			    sizeof(statfsp->f_mntfromname) ||
			    len > sizeof(device))
				continue;
			strncpy(device, _PATH_DEV, len);
			strncat(device, ddevname, len);
			if (stat(device, &mntdevstat) == 0)
				strncpy(statfsp->f_mntfromname, device, len);
		}
		if (stat(ddevname, &mntdevstat) == 0 &&
		    mntdevstat.st_rdev == devstat.st_rdev)
			return (statfsp);
	}
	return (NULL);
}

/*
 * If possible reload a mounted filesystem.
 * When prtmsg != NULL print a warning if a reload is attempted, but fails.
 * Return 0 on success, 1 on failure.
 */
int
chkdoreload(struct statfs *mntp,
	void (*prtmsg)(const char *, ...) __printflike(1,2))
{
	struct iovec *iov;
	int iovlen, error;
	char errmsg[255];

	/*
	 * If the filesystem is not mounted it does not need to be reloaded.
	 * If it is mounted for writing, then it could not have been opened
	 * for writing by a utility, so does not need to be reloaded.
	 */
	if (mntp == NULL || (mntp->f_flags & MNT_RDONLY) == 0)
		return (0);

	/*
	 * We modified a mounted file system.  Do a mount update on
	 * it so we can continue using it as safely as possible.
	 */
	iov = NULL;
	iovlen = 0;
	errmsg[0] = '\0';
	build_iovec(&iov, &iovlen, "fstype", __DECONST(void *, "ffs"), 4);
	build_iovec(&iov, &iovlen, "from", mntp->f_mntfromname, (size_t)-1);
	build_iovec(&iov, &iovlen, "fspath", mntp->f_mntonname, (size_t)-1);
	build_iovec(&iov, &iovlen, "errmsg", errmsg, sizeof(errmsg));
	build_iovec(&iov, &iovlen, "update", NULL, 0);
	build_iovec(&iov, &iovlen, "reload", NULL, 0);
	/*
	 * XX: We need the following line until we clean up
	 * nmount parsing of root mounts and NFS root mounts.
	 */
	build_iovec(&iov, &iovlen, "ro", NULL, 0);
	error = nmount(iov, iovlen, mntp->f_flags);
	free_iovec(&iov, &iovlen);
	if (error == 0)
		return (0);
	if (prtmsg != NULL)
		prtmsg("mount reload of '%s' failed: %s %s\n\n",
		    mntp->f_mntonname, strerror(errno), errmsg);
	return (1);
}

void
build_iovec(struct iovec **iov, int *iovlen, const char *name, void *val,
	    size_t len)
{
	int i;

	if (*iovlen < 0)
		return;
	i = *iovlen;
	*iov = realloc(*iov, sizeof **iov * (i + 2));
	if (*iov == NULL) {
		*iovlen = -1;
		return;
	}
	(*iov)[i].iov_base = strdup(name);
	(*iov)[i].iov_len = strlen(name) + 1;
	i++;
	(*iov)[i].iov_base = val;
	if (len == (size_t)-1) {
		if (val != NULL)
			len = strlen(val) + 1;
		else
			len = 0;
	}
	(*iov)[i].iov_len = (int)len;
	*iovlen = ++i;
}

/*
 * This function is needed for compatibility with parameters
 * which used to use the mount_argf() command for the old mount() syscall.
 */
void
build_iovec_argf(struct iovec **iov, int *iovlen, const char *name,
    const char *fmt, ...)
{
	va_list ap;
	char val[255] = { 0 };

	va_start(ap, fmt);
	vsnprintf(val, sizeof(val), fmt, ap);
	va_end(ap);
	build_iovec(iov, iovlen, name, strdup(val), (size_t)-1);
}

/*
 * Free the iovec and reset to NULL with zero length.  Useful for calling
 * nmount in a loop.
 */
void
free_iovec(struct iovec **iov, int *iovlen)
{
	int i;

	for (i = 0; i < *iovlen; i += 2)
		free((*iov)[i].iov_base);
	free(*iov);
}
