/*-
 * Copyright (c) 2010 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Pawel Jakub Dawidek under sponsorship from
 * the FreeBSD Foundation.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
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

#include <sys/types.h>
#include <sys/disk.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <unistd.h>

#include <pjdlog.h>

#include "hast.h"
#include "subr.h"

int
provinfo(struct hast_resource *res, bool dowrite)
{
	struct stat sb;

	PJDLOG_ASSERT(res->hr_localpath != NULL &&
	    res->hr_localpath[0] != '\0');

	if (res->hr_localfd == -1) {
		res->hr_localfd = open(res->hr_localpath,
		    dowrite ? O_RDWR : O_RDONLY);
		if (res->hr_localfd < 0) {
			KEEP_ERRNO(pjdlog_errno(LOG_ERR, "Unable to open %s",
			    res->hr_localpath));
			return (-1);
		}
	}
	if (fstat(res->hr_localfd, &sb) < 0) {
		KEEP_ERRNO(pjdlog_errno(LOG_ERR, "Unable to stat %s",
		    res->hr_localpath));
		return (-1);
	}
	if (S_ISCHR(sb.st_mode)) {
		/*
		 * If this is character device, it is most likely GEOM provider.
		 */
		if (ioctl(res->hr_localfd, DIOCGMEDIASIZE,
		    &res->hr_local_mediasize) < 0) {
			KEEP_ERRNO(pjdlog_errno(LOG_ERR,
			    "Unable obtain provider %s mediasize",
			    res->hr_localpath));
			return (-1);
		}
		if (ioctl(res->hr_localfd, DIOCGSECTORSIZE,
		    &res->hr_local_sectorsize) < 0) {
			KEEP_ERRNO(pjdlog_errno(LOG_ERR,
			    "Unable obtain provider %s sectorsize",
			    res->hr_localpath));
			return (-1);
		}
	} else if (S_ISREG(sb.st_mode)) {
		/*
		 * We also support regular files for which we hardcode
		 * sector size of 512 bytes.
		 */
		res->hr_local_mediasize = sb.st_size;
		res->hr_local_sectorsize = 512;
	} else {
		/*
		 * We support no other file types.
		 */
		pjdlog_error("%s is neither GEOM provider nor regular file.",
		    res->hr_localpath);
		errno = EFTYPE;
		return (-1);
	}
	return (0);
}

const char *
role2str(int role)
{

	switch (role) {																			
	case HAST_ROLE_INIT:																				
		return ("init");
	case HAST_ROLE_PRIMARY:																			
		return ("primary");
	case HAST_ROLE_SECONDARY:																			
		return ("secondary");
	}
	return ("unknown");
}

int
drop_privs(void)
{
	struct passwd *pw;
	uid_t ruid, euid, suid;
	gid_t rgid, egid, sgid;
	gid_t gidset[1];

	/*
	 * According to getpwnam(3) we have to clear errno before calling the
	 * function to be able to distinguish between an error and missing
	 * entry (with is not treated as error by getpwnam(3)).
	 */
	errno = 0;
	pw = getpwnam(HAST_USER);
	if (pw == NULL) {
		if (errno != 0) {
			KEEP_ERRNO(pjdlog_errno(LOG_ERR,
			    "Unable to find info about '%s' user", HAST_USER));
			return (-1);
		} else {
			pjdlog_error("'%s' user doesn't exist.", HAST_USER);
			errno = ENOENT;
			return (-1);
		}
	}
	if (chroot(pw->pw_dir) == -1) {
		KEEP_ERRNO(pjdlog_errno(LOG_ERR,
		    "Unable to change root directory to %s", pw->pw_dir));
		return (-1);
	}
	PJDLOG_VERIFY(chdir("/") == 0);
	gidset[0] = pw->pw_gid;
	if (setgroups(1, gidset) == -1) {
		KEEP_ERRNO(pjdlog_errno(LOG_ERR,
		    "Unable to set groups to gid %u",
		    (unsigned int)pw->pw_gid));
		return (-1);
	}
	if (setgid(pw->pw_gid) == -1) {
		KEEP_ERRNO(pjdlog_errno(LOG_ERR, "Unable to set gid to %u",
		    (unsigned int)pw->pw_gid));
		return (-1);
	}
	if (setuid(pw->pw_uid) == -1) {
		KEEP_ERRNO(pjdlog_errno(LOG_ERR, "Unable to set uid to %u",
		    (unsigned int)pw->pw_uid));
		return (-1);
	}

	/*
	 * Better be sure that everything succeeded.
	 */
	PJDLOG_VERIFY(getresuid(&ruid, &euid, &suid) == 0);
	PJDLOG_VERIFY(ruid == pw->pw_uid);
	PJDLOG_VERIFY(euid == pw->pw_uid);
	PJDLOG_VERIFY(suid == pw->pw_uid);
	PJDLOG_VERIFY(getresgid(&rgid, &egid, &sgid) == 0);
	PJDLOG_VERIFY(rgid == pw->pw_gid);
	PJDLOG_VERIFY(egid == pw->pw_gid);
	PJDLOG_VERIFY(sgid == pw->pw_gid);
	PJDLOG_VERIFY(getgroups(0, NULL) == 1);
	PJDLOG_VERIFY(getgroups(1, gidset) == 1);
	PJDLOG_VERIFY(gidset[0] == pw->pw_gid);

	return (0);
}
