/*-
 * Copyright (c) 2009 Michihiro NAKAJIMA
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */
#ifndef CPIO_WINDOWS_H
#define CPIO_WINDOWS_H 1

#include <io.h>
#include <string.h>

#define getgrgid(id)	NULL
#define getgrnam(name)	NULL
#define getpwnam(name)	NULL
#define getpwuid(id)	NULL

#ifdef _MSC_VER
#define snprintf	sprintf_s
#define strdup		_strdup
#define open	_open
#define read	_read
#define close	_close
#endif

struct passwd {
	char	*pw_name;
	uid_t	 pw_uid;
	gid_t	 pw_gid;
};

struct group {
	char	*gr_name;
	gid_t	 gr_gid;
};

struct _timeval64i32 {
	time_t		tv_sec;
	long		tv_usec;
};
#define __timeval _timeval64i32

extern int futimes(int fd, const struct __timeval *times);
#ifndef HAVE_FUTIMES
#define HAVE_FUTIMES 1
#endif
extern int utimes(const char *name, const struct __timeval *times);
#ifndef HAVE_UTIMES
#define HAVE_UTIMES 1
#endif

#endif /* CPIO_WINDOWS_H */
