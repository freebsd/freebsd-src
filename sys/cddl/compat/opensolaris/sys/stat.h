/*
 * Copyright (C) 2007 John Birrell <jb@freebsd.org>
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 *
 */

#ifndef _COMPAT_OPENSOLARIS_SYS_STAT_H_
#define _COMPAT_OPENSOLARIS_SYS_STAT_H_

#include_next <sys/stat.h>

/*
 * When bootstrapping on Linux a stat64/fstat64 functions exists in both
 * glibc and musl libc. To avoid compilation errors, use those functions instead
 * of redefining them to stat/fstat.
 * Similarly, macOS provides (deprecated) stat64 functions that we can use
 * for now.
 */
#if !defined(__linux__) && !defined(__APPLE__)
#define	stat64	stat

#define	MAXOFFSET_T	OFF_MAX

#if !defined(_KERNEL)
#include <sys/disk.h>

static __inline int
fstat64(int fd, struct stat *sb)
{
	int ret;

	ret = fstat(fd, sb);
	if (ret == 0) {
		if (S_ISCHR(sb->st_mode))
			(void)ioctl(fd, DIOCGMEDIASIZE, &sb->st_size);
	}
	return (ret);
}
#endif /* !defined(_KERNEL) */
#endif /* !defined(__linux__) && !defined(__APPLE__) */

#endif	/* !_COMPAT_OPENSOLARIS_SYS_STAT_H_ */
