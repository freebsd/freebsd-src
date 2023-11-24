/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2020 Stefan Eßer <se@freebsd.org>
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
#include <sys/sysctl.h>
#include <sys/limits.h>
#include <errno.h>
#include <stdlib.h>
#include <paths.h>
#include <libutil.h>
#include <unistd.h>

#ifndef LOCALBASE_PATH
#define LOCALBASE_PATH _PATH_LOCALBASE
#endif

#ifndef LOCALBASE_CTL_LEN
#define LOCALBASE_CTL_LEN MAXPATHLEN
#endif

/* Any prefix guaranteed to not be the start of a valid path name */
#define ILLEGAL_PREFIX "/dev/null/"

const char *
getlocalbase(void)
{
#if LOCALBASE_CTL_LEN > 0
	int localbase_oid[2] = {CTL_USER, USER_LOCALBASE};
	static char localpath[LOCALBASE_CTL_LEN];
	size_t localpathlen = LOCALBASE_CTL_LEN;
#endif
	char *tmppath;
	static const char *localbase = NULL;

	if (localbase != NULL)
		return (localbase);

	if (issetugid() == 0) {
		tmppath = getenv("LOCALBASE");
		if (tmppath != NULL && tmppath[0] != '\0') {
			localbase = tmppath;
			return (localbase);
		}
	}

#if LOCALBASE_CTL_LEN > 0
	if (sysctl(localbase_oid, 2, localpath, &localpathlen, NULL, 0) != 0) {
		if (errno != ENOMEM)
			localbase = LOCALBASE_PATH;
		else
			localbase = ILLEGAL_PREFIX;
	} else {
		if (localpath[0] != '\0')
			localbase = localpath;
		else
			localbase = LOCALBASE_PATH;
	}
#else
	localbase = LOCALBASE_PATH;
#endif

	return (localbase);
}
