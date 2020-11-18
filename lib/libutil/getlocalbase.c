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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/limits.h>
#include <stdlib.h>
#include <paths.h>
#include <libutil.h>
#include <unistd.h>

#ifndef _PATH_LOCALBASE
#define _PATH_LOCALBASE "/usr/local"
#endif

const char *
getlocalbase(void)
{
	static const int localbase_oid[2] = {CTL_USER, USER_LOCALBASE};
	char *tmppath;
	size_t tmplen;
	static const char *localbase = NULL;

	if (issetugid() == 0) {
		tmppath = getenv("LOCALBASE");
		if (tmppath != NULL && tmppath[0] != '\0')
			return (tmppath);
	}
	if (sysctl(localbase_oid, 2, NULL, &tmplen, NULL, 0) == 0 &&
	    (tmppath = malloc(tmplen)) != NULL && 
	    sysctl(localbase_oid, 2, tmppath, &tmplen, NULL, 0) == 0) {
		/*
		 * Check for some other thread already having 
		 * set localbase - this should use atomic ops.
		 * The amount of memory allocated above may leak,
		 * if a parallel update in another thread is not
		 * detected and the non-NULL pointer is overwritten.
		 */
		if (tmppath[0] != '\0' &&
		    (volatile const char*)localbase == NULL)
			localbase = tmppath;
		else
			free((void*)tmppath);
		return (localbase);
	}
	return (_PATH_LOCALBASE);
}
