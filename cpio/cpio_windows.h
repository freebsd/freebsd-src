/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2009 Michihiro NAKAJIMA
 * All rights reserved.
 */
#ifndef CPIO_WINDOWS_H
#define CPIO_WINDOWS_H 1
#include <windows.h>

#include <io.h>
#include <string.h>

#define getgrgid(id)	NULL
#define getgrnam(name)	NULL
#define getpwnam(name)	NULL
#define getpwuid(id)	NULL

#if defined(_MSC_VER)
 #if _MSC_VER < 1900
 #define snprintf	sprintf_s
 #endif // _MSC_VER < 1900
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
