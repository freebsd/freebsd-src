/*-
 * Copyright (c) 2002 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by NAI Labs, the
 * Security Research Division of Network Associates, Inc. under
 * DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"), as part of the DARPA
 * CHATS research program.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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
#include <sys/errno.h>
#include <sys/sysctl.h>

#include <stdlib.h>
#include <string.h>

/*
 * Simply test whether the TrustedBSD/MAC MIB tree is present; if so,
 * return 1 to indicate that the system has MAC enabled overall or for
 * a given policy.
 */

int
mac_is_present_np(const char *policyname)
{
	int mib[5];
	size_t siz;
	char *mibname;
	int error;

	if (policyname != NULL) {
		if (policyname[strcspn(policyname, ".=")] != '\0') {
			errno = EINVAL;
			return (-1);
		}
		mibname = malloc(sizeof("security.mac.") - 1 +
		    strlen(policyname) + sizeof(".enabled"));
		if (mibname == NULL)
			return (-1);
		strcpy(mibname, "security.mac.");
		strcat(mibname, policyname);
		strcat(mibname, ".enabled");
		siz = 5;
		error = sysctlnametomib(mibname, mib, &siz);
		free(mibname);
	} else {
		siz = 3;
		error = sysctlnametomib("security.mac", mib, &siz);
	}
	if (error == -1) {
		switch (errno) {
		case ENOTDIR:
		case ENOENT:
			return (0);
		default:
			return (error);
		}
	}
	return (1);
}
