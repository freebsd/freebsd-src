/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003-2010 Tim Kientzle
 * Copyright (c) 2003-2007 Kees Zeelenberg
 * Copyright (c) 2009-2012 Michihiro NAKAJIMA
 * Copyright (c) 2026 Tobias Stoeckmann
 * All rights reserved.
 */

#if defined(_WIN32) && !defined(__CYGWIN__)

#include "bsdunzip_platform.h"

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_IO_H
#include <io.h>
#endif
#include <winnt.h>

#ifndef S_IFIFO
#define	S_IFIFO		_S_IFIFO
#endif

#define	_S_IRWXU	(_S_IREAD | _S_IWRITE | _S_IEXEC)
#define	_S_IXUSR	_S_IEXEC  /* read permission, user */
#define	_S_IWUSR	_S_IWRITE /* write permission, user */
#define	_S_IRUSR	_S_IREAD  /* execute/search permission, user */
#define	_S_IRWXG	(_S_IRWXU >> 3)
#define	_S_IXGRP	(_S_IXUSR >> 3) /* read permission, group */
#define	_S_IWGRP	(_S_IWUSR >> 3) /* write permission, group */
#define	_S_IRGRP	(_S_IRUSR >> 3) /* execute/search permission, group */
#define	_S_IRWXO	(_S_IRWXG >> 3)
#define	_S_IXOTH	(_S_IXGRP >> 3) /* read permission, other */
#define	_S_IWOTH	(_S_IWGRP >> 3) /* write permission, other */
#define	_S_IROTH	(_S_IRGRP  >> 3) /* execute/search permission, other */

#ifndef S_IRWXU
#define	S_IRWXU		_S_IRWXU
#define	S_IXUSR		_S_IXUSR
#define	S_IWUSR		_S_IWUSR
#define	S_IRUSR		_S_IRUSR
#endif
#ifndef S_IRWXG
#define	S_IRWXG		_S_IRWXG
#define	S_IXGRP		_S_IXGRP
#define	S_IWGRP		_S_IWGRP
#endif
#ifndef S_IRGRP
#define	S_IRGRP		_S_IRGRP
#endif
#ifndef S_IRWXO
#define	S_IRWXO		_S_IRWXO
#define	S_IXOTH		_S_IXOTH
#define	S_IWOTH		_S_IWOTH
#define	S_IROTH		_S_IROTH
#endif

#ifndef SYMBOLIC_LINK_FLAG_DIRECTORY
#define SYMBOLIC_LINK_FLAG_DIRECTORY	0x1
#endif

#define NTFS_EPOC_TIME		11644473600ULL
#define NTFS_TICKS		10000000ULL
#define NTFS_EPOC_TICKS		(NTFS_EPOC_TIME * NTFS_TICKS)
#define WINTIME(sec, usec)	(((sec * 10000000LL) + NTFS_EPOC_TICKS) + (usec * 10))

static void unzip_dosmaperr(unsigned long);

/*
 * Convert the path name to unicode.
 */
static wchar_t *
unzip_strtowcs(const char *name)
{
	wchar_t *wn;
	DWORD l, len;

	len = (DWORD)strlen(name);
	wn = malloc((len + 1) * sizeof(wchar_t));
	if (wn == NULL)
		return (NULL);
	l = MultiByteToWideChar(CP_ACP, 0, name, len, wn, len);
	if (l == 0) {
		free(wn);
        errno = EILSEQ;
		return (NULL);
	}
	wn[l] = L'\0';

	return wn;
}

static wchar_t *
unzip_win_permissive_name_w(const wchar_t *wname)
{
	wchar_t *wn, *wnp;
	wchar_t *ws, *wsp;
	DWORD l, len, slen;
	int unc;

	/* Get a full-pathname. */
	l = GetFullPathNameW(wname, 0, NULL, NULL);
	if (l == 0)
		return (NULL);
	/* NOTE: GetFullPathNameW has a bug that if the length of the file
	 * name is just 1 then it returns incomplete buffer size. Thus, we
	 * have to add three to the size to allocate a sufficient buffer
	 * size for the full-pathname of the file name. */
	l += 3;
	wnp = malloc(l * sizeof(wchar_t));
	if (wnp == NULL)
		return (NULL);
	len = GetFullPathNameW(wname, l, wnp, NULL);
	wn = wnp;

	if (wnp[0] == L'\\' && wnp[1] == L'\\' &&
	    wnp[2] == L'?' && wnp[3] == L'\\')
		/* We have already a permissive name. */
		return (wn);

	if (wnp[0] == L'\\' && wnp[1] == L'\\' &&
		wnp[2] == L'.' && wnp[3] == L'\\') {
		/* This is a device name */
		if (((wnp[4] >= L'a' && wnp[4] <= L'z') ||
		     (wnp[4] >= L'A' && wnp[4] <= L'Z')) &&
		    wnp[5] == L':' && wnp[6] == L'\\')
			wnp[2] = L'?';/* Not device name. */
		return (wn);
	}

	unc = 0;
	if (wnp[0] == L'\\' && wnp[1] == L'\\' && wnp[2] != L'\\') {
		wchar_t *p = &wnp[2];

		/* Skip server-name letters. */
		while (*p != L'\\' && *p != L'\0')
			++p;
		if (*p == L'\\') {
			wchar_t *rp = ++p;
			/* Skip share-name letters. */
			while (*p != L'\\' && *p != L'\0')
				++p;
			if (*p == L'\\' && p != rp) {
				/* Now, match patterns such as
				 * "\\server-name\share-name\" */
				wnp += 2;
				len -= 2;
				unc = 1;
			}
		}
	}

	slen = 4 + (unc * 4) + len + 1;
	ws = wsp = malloc(slen * sizeof(wchar_t));
	if (ws == NULL) {
		free(wn);
		return (NULL);
	}
	/* prepend "\\?\" */
	wcsncpy(wsp, L"\\\\?\\", 4);
	wsp += 4;
	slen -= 4;
	if (unc) {
		/* append "UNC\" ---> "\\?\UNC\" */
		wcsncpy(wsp, L"UNC\\", 4);
		wsp += 4;
		slen -= 4;
	}
	wcsncpy(wsp, wnp, slen);
	wsp[slen - 1] = L'\0'; /* Ensure null termination. */
	free(wn);
	return (ws);
}

/* Windows FILETIME to unix time. */
static uint64_t
unzip_FILETIME_to_unix(const FILETIME *filetime)
{
	ULARGE_INTEGER utc;
	uint64_t ntfs, unix;

	utc.HighPart = filetime->dwHighDateTime;
	utc.LowPart = filetime->dwLowDateTime;
	ntfs = utc.QuadPart;

	if (ntfs > INT64_MAX) {
		ntfs -= NTFS_EPOC_TICKS;
		unix = ntfs / NTFS_TICKS;
	} else {
		lldiv_t tdiv;
		int64_t value = (int64_t)ntfs - (int64_t)NTFS_EPOC_TICKS;

		tdiv = lldiv(value, NTFS_TICKS);
		unix = tdiv.quot;
	}
	return unix;
}

static HANDLE
unzip_get_handle(const wchar_t *path, int access, int open_link, int *is_link)
{
	HANDLE h;
	DWORD flag = FILE_FLAG_BACKUP_SEMANTICS;
	WIN32_FIND_DATAW findData;
#if _WIN32_WINNT >= 0x0602 /* _WIN32_WINNT_WIN8 */
	CREATEFILE2_EXTENDED_PARAMETERS createExParams;
#endif

	h = FindFirstFileW(path, &findData);
	if (h == INVALID_HANDLE_VALUE &&
	    GetLastError() == ERROR_INVALID_NAME) {
		wchar_t *full;
		full = unzip_win_permissive_name_w(path);
		h = FindFirstFileW(full, &findData);
		free(full);
	}
	if (h == INVALID_HANDLE_VALUE)
		return (h);
	FindClose(h);

	/* Is symlink file? */
	if (((findData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) &&
	    (findData.dwReserved0 == IO_REPARSE_TAG_SYMLINK))) {
		if (open_link)
			flag |= FILE_FLAG_OPEN_REPARSE_POINT;
		*is_link = 1;
	} else
		*is_link = 0;

#if _WIN32_WINNT >= 0x0602 /* _WIN32_WINNT_WIN8 */
	ZeroMemory(&createExParams, sizeof(createExParams));
	createExParams.dwSize = sizeof(createExParams);
	createExParams.dwFileFlags = flag;
	h = CreateFile2(path, access, 0, OPEN_EXISTING, &createExParams);
#else
	h = CreateFileW(path, access, 0, NULL, OPEN_EXISTING, flag, NULL);
#endif
	if (h == INVALID_HANDLE_VALUE &&
	    GetLastError() == ERROR_INVALID_NAME) {
		wchar_t *full;
		full = unzip_win_permissive_name_w(path);
#if _WIN32_WINNT >= 0x0602 /* _WIN32_WINNT_WIN8 */
		h = CreateFile2(full, access, 0,
			OPEN_EXISTING, &createExParams);
#else
		h = CreateFileW(full, access, 0, NULL,
		    OPEN_EXISTING, flag, NULL);
#endif
		free(full);
	}

	return (h);
}

static int
unzip_stat_w(const wchar_t *path, int stat_link, struct stat *st)
{
	HANDLE h;
	int is_link, r;
	DWORD ftype;
	BY_HANDLE_FILE_INFORMATION info;
	ULARGE_INTEGER ino64;

	h = unzip_get_handle(path, 0, stat_link, &is_link);
	if (h == INVALID_HANDLE_VALUE) {
		unzip_dosmaperr(GetLastError());
		return (-1);
	}
	ZeroMemory(&info, sizeof(info));
	r = GetFileInformationByHandle(h, &info);

	switch (ftype = GetFileType(h)) {
	case FILE_TYPE_UNKNOWN:
		errno = EBADF;
		return (-1);
	case FILE_TYPE_CHAR:
	case FILE_TYPE_PIPE:
		if (ftype == FILE_TYPE_CHAR) {
			st->st_mode = S_IFCHR;
			st->st_size = 0;
		} else {
			DWORD avail;

			st->st_mode = S_IFIFO;
			if (PeekNamedPipe(h, NULL, 0, NULL, &avail, NULL))
				st->st_size = avail;
			else
				st->st_size = 0;
		}
		st->st_atime = 0;
		st->st_mtime = 0;
		st->st_ctime = 0;
		st->st_ino = 0;
		st->st_nlink = 1;
		st->st_uid = 0;
		st->st_gid = 0;
		st->st_rdev = 0;
		st->st_dev = 0;
		return (0);
	case FILE_TYPE_DISK:
		break;
	default:
		/* This ftype is undocumented type. */
		unzip_dosmaperr(GetLastError());
		return (-1);
	}

	CloseHandle(h);
	if (r == 0) {
		unzip_dosmaperr(GetLastError());
		return (-1);
	}

	st->st_mode = S_IRUSR | S_IRGRP | S_IROTH;
	if ((info.dwFileAttributes & FILE_ATTRIBUTE_READONLY) == 0)
		st->st_mode |= S_IWUSR | S_IWGRP | S_IWOTH;
	if ((info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) && is_link)
		st->st_mode |= S_IFLNK;
	else if (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		st->st_mode |= S_IFDIR | S_IXUSR | S_IXGRP | S_IXOTH;
	else {
		const wchar_t *p;

		st->st_mode |= S_IFREG;
		p = wcsrchr(path, L'.');
		if (p != NULL && wcslen(p) == 4) {
			switch (p[1]) {
			case L'B': case L'b':
				if ((p[2] == L'A' || p[2] == L'a' ) &&
				    (p[3] == L'T' || p[3] == L't' ))
					st->st_mode |= S_IXUSR | S_IXGRP | S_IXOTH;
				break;
			case L'C': case L'c':
				if (((p[2] == L'M' || p[2] == L'm' ) &&
				    (p[3] == L'D' || p[3] == L'd' )))
					st->st_mode |= S_IXUSR | S_IXGRP | S_IXOTH;
				break;
			case L'E': case L'e':
				if ((p[2] == L'X' || p[2] == L'x' ) &&
				    (p[3] == L'E' || p[3] == L'e' ))
					st->st_mode |= S_IXUSR | S_IXGRP | S_IXOTH;
				break;
			default:
				break;
			}
		}
	}

	st->st_atime = unzip_FILETIME_to_unix(&info.ftLastAccessTime);
	st->st_mtime = unzip_FILETIME_to_unix(&info.ftLastWriteTime);
	st->st_ctime = unzip_FILETIME_to_unix(&info.ftCreationTime);
	st->st_size =
	    ((int64_t)(info.nFileSizeHigh) * ((int64_t)MAXDWORD + 1))
		+ (int64_t)(info.nFileSizeLow);
#ifdef SIMULATE_WIN_STAT
	st->st_ino = 0;
	st->st_nlink = 1;
	st->st_dev = 0;
#else
	/* Getting FileIndex as i-node. We should remove a sequence which
	 * is high-16-bits of nFileIndexHigh. */
	ino64.HighPart = info.nFileIndexHigh & 0x0000FFFFUL;
	ino64.LowPart = info.nFileIndexLow;
	st->st_ino = ino64.QuadPart;
	st->st_nlink = info.nNumberOfLinks;
	if (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		++st->st_nlink;/* Add parent directory. */
	st->st_dev = info.dwVolumeSerialNumber;
#endif
	st->st_uid = 0;
	st->st_gid = 0;
	st->st_rdev = 0;

	return (0);
}

int
lstat(const char *path, struct stat *st)
{
	wchar_t *wcs;
	int r;

	wcs = unzip_strtowcs(path);
	if (wcs == NULL)
	        return (-1);

	r = unzip_stat_w(wcs, 1, st);
	free(wcs);
	return r == 0 ? 0 : -1;
}

static int
unzip_unlink(const wchar_t *path)
{
	wchar_t *fullname;
	int r;

	r = _wunlink(path);
	if (r != 0 && GetLastError() == ERROR_INVALID_NAME) {
		fullname = unzip_win_permissive_name_w(path);
		r = _wunlink(fullname);
		free(fullname);
	}
	return (r);
}

static int
unzip_rmdir(const wchar_t *path)
{
	wchar_t *fullname;
	int r;

	r = _wrmdir(path);
	if (r != 0 && GetLastError() == ERROR_INVALID_NAME) {
		fullname = unzip_win_permissive_name_w(path);
		r = _wrmdir(fullname);
		free(fullname);
	}
	return (r);
}

/*
 * Create file or directory symbolic link
 *
 * Guess linktype from the link target
 */
static int
unzip_CreateSymbolicLinkW(const wchar_t *linkname, const wchar_t *target) {
	BOOL ret = 0;
#if _WIN32_WINNT < _WIN32_WINNT_VISTA ||\
    !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP | WINAPI_PARTITION_SYSTEM)
	(void)linkname; /* UNUSED */
	(void)target; /* UNUSED */
#else
	struct stat st;
	wchar_t *ttarget, *p;
	size_t len;
	DWORD attrs = 0;
	DWORD flags = 0;
	DWORD newflags = 0;

	len = wcslen(target);
	if (len == 0) {
		errno = EINVAL;
		return(0);
	}
	/*
	 * When writing path targets, we need to translate slashes
	 * to backslashes
	 */
	ttarget = malloc((len + 1) * sizeof(wchar_t));
	if (ttarget == NULL)
		return(0);

	p = ttarget;

	while(*target != L'\0') {
		if (*target == L'/')
			*p = L'\\';
		else
			*p = *target;
		target++;
		p++;
	}
	*p = L'\0';

	/* Check target type if available. */
	if (unzip_stat_w(ttarget, 0, &st) == 0) {
		if (S_ISDIR(st.st_mode)) {
			flags |= SYMBOLIC_LINK_FLAG_DIRECTORY;
		}
	} else {
		/*
		 * We guess symlink type from the target.
		 * If the target equals ".", "..", ends with a backslash or a
		 * backslash followed by "." or ".." we assume it is a directory
		 * symlink. In all other cases we assume a file symlink.
		 */
		if (*(p - 1) == L'\\' || (*(p - 1) == L'.' && (
		    len == 1 || *(p - 2) == L'\\' || ( *(p - 2) == L'.' && (
		    len == 2 || *(p - 3) == L'\\'))))) {
			flags |= SYMBOLIC_LINK_FLAG_DIRECTORY;
		}
	}

#if defined(SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE)
	newflags = flags | SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE;
#else
	newflags = flags | 0x2;
#endif

	/*
	 * Windows won't overwrite existing links
	 */
	attrs = GetFileAttributesW(linkname);
	if (attrs != INVALID_FILE_ATTRIBUTES) {
		if (attrs & FILE_ATTRIBUTE_DIRECTORY)
			unzip_rmdir(linkname);
		else
			unzip_unlink(linkname);
	}

	ret = CreateSymbolicLinkW(linkname, ttarget, newflags);
	/*
	 * Prior to Windows 10 calling CreateSymbolicLinkW() will fail
	 * if SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE is set
	 */
	if (!ret) {
		ret = CreateSymbolicLinkW(linkname, ttarget, flags);
	}
	free(ttarget);
#endif
	return (ret);
}

int
symlink(const char *target, const char *source)
{
	wchar_t *wcsource, *wctarget;
	int r;

	wcsource = unzip_strtowcs(source);
	wctarget = unzip_strtowcs(target);

	if (wcsource != NULL && wctarget != NULL) {
		r = unzip_CreateSymbolicLinkW(wcsource, wctarget);
		if (r == 0)
			unzip_dosmaperr(GetLastError());
	} else {
		errno = ENOMEM;
		r = 0;
	}
	free(wcsource);
	free(wctarget);
	return r == 0 ? -1 : 0;
}

static int
unzip_set_times(HANDLE handle, const struct timeval *times)
{
	ULARGE_INTEGER wintm;
	FILETIME fatime, fmtime;

	wintm.QuadPart = WINTIME(times[0].tv_sec, times[0].tv_usec);
	fatime.dwLowDateTime = wintm.LowPart;
	fatime.dwHighDateTime = wintm.HighPart;

	wintm.QuadPart = WINTIME(times[1].tv_sec, times[1].tv_usec);
	fmtime.dwLowDateTime = wintm.LowPart;
	fmtime.dwHighDateTime = wintm.HighPart;

	if (SetFileTime(handle, NULL, &fatime, &fmtime) == 0) {
		unzip_dosmaperr(GetLastError());
		return (-1);
	}

	return (0);
}

int
futimes(int fd, const struct timeval *times)
{
	return (unzip_set_times((HANDLE)_get_osfhandle(fd), times));
}

#ifndef HAVE_GETTIMEOFDAY
int
gettimeofday(struct timeval *tv, struct timezone *tz) {
	FILETIME ft;
	uint64_t time;

	(void)tz; /* UNUSED */

	GetSystemTimePreciseAsFileTime(&ft);
	time = unzip_FILETIME_to_unix(&ft);

	tv->tv_sec = (long)(time / NTFS_TICKS);
	tv->tv_usec = (long)((time % NTFS_TICKS) / 10ULL);

	return (0);
}
#endif

int
lutimes(const char *path, const struct timeval *times)
{
	HANDLE h;
	wchar_t *p;
	int link, ret;

	p = unzip_strtowcs(path);
	if (p == NULL)
		return (-1);

	h = unzip_get_handle(p, FILE_WRITE_ATTRIBUTES, 1, &link);
	ret = unzip_set_times(h, times);

	free(p);
	CloseHandle(h);

	return (ret);
}

/*
 * The following function was modified from PostgreSQL sources and is
 * subject to the copyright below.
 */
/*-------------------------------------------------------------------------
 *
 * win32error.c
 *	  Map win32 error codes to errno values
 *
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/port/win32error.c,v 1.4 2008/01/01 19:46:00 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
/*
PostgreSQL Database Management System
(formerly known as Postgres, then as Postgres95)

Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group

Portions Copyright (c) 1994, The Regents of the University of California

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose, without fee, and without a written agreement
is hereby granted, provided that the above copyright notice and this
paragraph and the following two paragraphs appear in all copies.

IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO ANY PARTY FOR
DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING
LOST PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS
DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA HAS BEEN ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY WARRANTIES,
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
AND FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
ON AN "AS IS" BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATIONS TO
PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
*/

static const struct {
	DWORD		winerr;
	int		doserr;
} doserrors[] =
{
	{	ERROR_INVALID_FUNCTION, EINVAL	},
	{	ERROR_FILE_NOT_FOUND, ENOENT	},
	{	ERROR_PATH_NOT_FOUND, ENOENT	},
	{	ERROR_TOO_MANY_OPEN_FILES, EMFILE	},
	{	ERROR_ACCESS_DENIED, EACCES	},
	{	ERROR_INVALID_HANDLE, EBADF	},
	{	ERROR_ARENA_TRASHED, ENOMEM	},
	{	ERROR_NOT_ENOUGH_MEMORY, ENOMEM	},
	{	ERROR_INVALID_BLOCK, ENOMEM	},
	{	ERROR_BAD_ENVIRONMENT, E2BIG	},
	{	ERROR_BAD_FORMAT, ENOEXEC	},
	{	ERROR_INVALID_ACCESS, EINVAL	},
	{	ERROR_INVALID_DATA, EINVAL	},
	{	ERROR_INVALID_DRIVE, ENOENT	},
	{	ERROR_CURRENT_DIRECTORY, EACCES	},
	{	ERROR_NOT_SAME_DEVICE, EXDEV	},
	{	ERROR_NO_MORE_FILES, ENOENT	},
	{	ERROR_LOCK_VIOLATION, EACCES	},
	{	ERROR_SHARING_VIOLATION, EACCES	},
	{	ERROR_BAD_NETPATH, ENOENT	},
	{	ERROR_NETWORK_ACCESS_DENIED, EACCES	},
	{	ERROR_BAD_NET_NAME, ENOENT	},
	{	ERROR_FILE_EXISTS, EEXIST	},
	{	ERROR_CANNOT_MAKE, EACCES	},
	{	ERROR_FAIL_I24, EACCES	},
	{	ERROR_INVALID_PARAMETER, EINVAL	},
	{	ERROR_NO_PROC_SLOTS, EAGAIN	},
	{	ERROR_DRIVE_LOCKED, EACCES	},
	{	ERROR_BROKEN_PIPE, EPIPE	},
	{	ERROR_DISK_FULL, ENOSPC	},
	{	ERROR_INVALID_TARGET_HANDLE, EBADF	},
	{	ERROR_INVALID_HANDLE, EINVAL	},
	{	ERROR_WAIT_NO_CHILDREN, ECHILD	},
	{	ERROR_CHILD_NOT_COMPLETE, ECHILD	},
	{	ERROR_DIRECT_ACCESS_HANDLE, EBADF	},
	{	ERROR_NEGATIVE_SEEK, EINVAL	},
	{	ERROR_SEEK_ON_DEVICE, EACCES	},
	{	ERROR_DIR_NOT_EMPTY, ENOTEMPTY	},
	{	ERROR_NOT_LOCKED, EACCES	},
	{	ERROR_BAD_PATHNAME, ENOENT	},
	{	ERROR_MAX_THRDS_REACHED, EAGAIN	},
	{	ERROR_LOCK_FAILED, EACCES	},
	{	ERROR_ALREADY_EXISTS, EEXIST	},
	{	ERROR_FILENAME_EXCED_RANGE, ENOENT	},
	{	ERROR_NESTING_NOT_ALLOWED, EAGAIN	},
	{	ERROR_PRIVILEGE_NOT_HELD, EPERM	},
	{	ERROR_NOT_ENOUGH_QUOTA, ENOMEM	}
};

static void
unzip_dosmaperr(unsigned long e)
{
	int			i;

	if (e == 0)
	{
		errno = 0;
		return;
	}

	for (i = 0; i < (int)(sizeof(doserrors)/sizeof(doserrors[0])); i++)
	{
		if (doserrors[i].winerr == e)
		{
			errno = doserrors[i].doserr;
			return;
		}
	}

	/* fprintf(stderr, "unrecognized win32 error code: %lu", e); */
	errno = EINVAL;
	return;
}

#endif
