/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Dave Borman at Cray Research, Inc.
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

#include "archive_platform.h"
#include "archive_umask_private.h"

#ifdef HAVE_SYS_SINGLE_THREADED_H
#include <sys/single_threaded.h>
#endif
#include <stddef.h>
#include <stdio.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/stat.h>

/* Return the current umask. Attempt to read it without changing. */
mode_t
__archive_get_umask(void)
{
	mode_t mask;

#ifdef __linux__
#ifdef HAVE_SYS_SINGLE_THREADED_H
	if (!__libc_single_threaded)
#endif
	{
		/*
		* Starting with Linux 4.7, the current process umask can be accessed
		* via /proc/self/status.
		*/

		// Lines in status files are almost always less than 100 bytes, so
		// 1 KiB should be plenty. Plus, the umask field should show up in
		// the first few lines & always be very short. So even if we split
		// other lines, that should never happen with umask. We assume any
		// lines we split do not contain "Umask:" in the middle of them.
		char line[1024];
		// The standard doesn't guarantee mode_t size, so scan a specific
		// size ourselves. We know the umask will always be 9 bits, so 32
		// bits should be plenty.
		int int_umask = -1;

		FILE *fp = fopen("/proc/self/status", "re");
		if (fp != NULL) {
			while (fgets(line, sizeof(line), fp)) {
				if (sscanf(line, "Umask: %o", &int_umask) == 1)
					break;
			}
			fclose(fp);
			if (int_umask != -1)
				return (int_umask);
		}
	}
#elif defined(KERN_PROC_UMASK)
	/*
	 * FreeBSD has a sysctl interface for requesting the umask without
	 * temporarily modifying it. Note that this does not work if the sysctl
	 * security.bsd.unprivileged_proc_debug is set to 0.
	 */
	u_short smask;
	size_t len = sizeof(smask);
	if (sysctl((int[4]){ CTL_KERN, KERN_PROC, KERN_PROC_UMASK, 0 },
	    4, &smask, &len, NULL, 0) == 0)
		return (smask);
#endif

	umask(mask = umask(0));
	return (mask);
}
