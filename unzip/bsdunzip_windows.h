/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003-2010 Tim Kientzle
 * Copyright (c) 2003-2007 Kees Zeelenberg
 * Copyright (c) 2009-2012 Michihiro NAKAJIMA
 * Copyright (c) 2026 Tobias Stoeckmann
 * All rights reserved.
 */
#ifndef BSDUNZIP_WINDOWS_H
#define BSDUNZIP_WINDOWS_H 1

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_WINDOWS_H
#include <windows.h>
#endif
#ifdef HAVE_DIRECT_H
#include <direct.h>
#endif

#ifndef _S_IFLNK
#define	_S_IFLNK    0120000   /* symbolic link */
#endif
#ifndef S_IFLNK
#define	S_IFLNK     _S_IFLNK
#endif

#ifndef S_ISDIR
#define	S_ISDIR(m)	(((m) & S_IFMT) == S_IFDIR)	/* directory */
#endif
#ifndef S_ISREG
#define	S_ISREG(m)	(((m) & S_IFMT) == S_IFREG)	/* regular file */
#endif
#ifndef S_ISLNK
#define	S_ISLNK(m)  (((m) & S_IFMT) == S_IFLNK) /* symbolic link */
#endif

#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif

#define HAVE_FUTIMES 1
#define HAVE_LUTIMES 1

#ifndef HAVE_GETTIMEOFDAY
struct timezone {
    int tz_minuteswest;
    int tz_dsttime;
};
int gettimeofday(struct timeval *tv, struct timezone *tz);
#endif

int futimes(int fd, const struct timeval *times);
int lstat(const char *path, struct stat *st);
int lutimes(const char *path, const struct timeval *times);
int symlink(const char *target, const char *source);

#endif /* !BSDUNZIP_WINDOWS_H */
