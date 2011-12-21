/*-
 * Copyright (c) 2009 Michihiro NAKAJIMA
 * Copyright (c) 2003-2007 Kees Zeelenberg
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

/*
 * A set of compatibility glue for building libarchive on Windows platforms.
 *
 * Originally created as "libarchive-nonposix.c" by Kees Zeelenberg
 * for the GnuWin32 project, trimmed significantly by Tim Kientzle.
 *
 * Much of the original file was unnecessary for libarchive, because
 * many of the features it emulated were not strictly necessary for
 * libarchive.  I hope for this to shrink further as libarchive
 * internals are gradually reworked to sit more naturally on both
 * POSIX and Windows.  Any ideas for this are greatly appreciated.
 *
 * The biggest remaining issue is the dev/ino emulation; libarchive
 * has a couple of public APIs that rely on dev/ino uniquely
 * identifying a file.  This doesn't match well with Windows.  I'm
 * considering alternative APIs.
 */

#if defined(_WIN32) && !defined(__CYGWIN__)

#include "archive_platform.h"
#include "archive_private.h"
#include "archive_hash.h"
#include <ctype.h>
#include <errno.h>
#include <stddef.h>
#ifdef HAVE_SYS_UTIME_H
#include <sys/utime.h>
#endif
#include <sys/stat.h>
#include <process.h>
#include <stdlib.h>
#include <wchar.h>
#include <windows.h>

#define EPOC_TIME ARCHIVE_LITERAL_ULL(116444736000000000)

#if defined(_MSC_VER) && _MSC_VER < 1300
/* VS 6 does not provide SetFilePointerEx, so define it here.  */
static BOOL SetFilePointerEx(HANDLE hFile,
                             LARGE_INTEGER liDistanceToMove,
                             PLARGE_INTEGER lpNewFilePointer,
                             DWORD dwMoveMethod)
{
	LARGE_INTEGER li;
	li.QuadPart = liDistanceToMove.QuadPart;
	li.LowPart = SetFilePointer(
	    hFile, li.LowPart, &li.HighPart, dwMoveMethod);
	if(lpNewFilePointer) {
		lpNewFilePointer->QuadPart = li.QuadPart;
	}
	return li.LowPart != -1 || GetLastError() == NO_ERROR;
}
#endif

struct ustat {
	int64_t		st_atime;
	uint32_t	st_atime_nsec;
	int64_t		st_ctime;
	uint32_t	st_ctime_nsec;
	int64_t		st_mtime;
	uint32_t	st_mtime_nsec;
	gid_t		st_gid;
	/* 64bits ino */
	int64_t		st_ino;
	mode_t		st_mode;
	uint32_t	st_nlink;
	uint64_t	st_size;
	uid_t		st_uid;
	dev_t		st_dev;
	dev_t		st_rdev;
};

/* Local replacement for undocumented Windows CRT function. */
static void la_dosmaperr(unsigned long e);

/* Transform 64-bits ino into 32-bits by hashing.
 * You do not forget that really unique number size is 64-bits.
 */
#define INOSIZE (8*sizeof(ino_t)) /* 32 */
static __inline ino_t
getino(struct ustat *ub)
{
	ULARGE_INTEGER ino64;
	ino64.QuadPart = ub->st_ino;
	/* I don't know this hashing is correct way */
	return (ino64.LowPart ^ (ino64.LowPart >> INOSIZE));
}

/*
 * Prepend "\\?\" to the path name and convert it to unicode to permit
 * an extended-length path for a maximum total path length of 32767
 * characters.
 * see also http://msdn.microsoft.com/en-us/library/aa365247.aspx
 */
static wchar_t *
permissive_name(const char *name)
{
	wchar_t *wn, *wnp;
	wchar_t *ws, *wsp;
	DWORD l, len, slen;
	int unc;

	len = (DWORD)strlen(name);
	wn = malloc((len + 1) * sizeof(wchar_t));
	if (wn == NULL)
		return (NULL);
	l = MultiByteToWideChar(CP_ACP, 0, name, (int)len, wn, (int)len);
	if (l == 0) {
		free(wn);
		return (NULL);
	}
	wn[l] = L'\0';

	/* Get a full path names */
	l = GetFullPathNameW(wn, 0, NULL, NULL);
	if (l == 0) {
		free(wn);
		return (NULL);
	}
	wnp = malloc(l * sizeof(wchar_t));
	if (wnp == NULL) {
		free(wn);
		return (NULL);
	}
	len = GetFullPathNameW(wn, l, wnp, NULL);
	free(wn);
	wn = wnp;

	if (wnp[0] == L'\\' && wnp[1] == L'\\' &&
	    wnp[2] == L'?' && wnp[3] == L'\\')
		/* We have already permissive names. */
		return (wn);

	if (wnp[0] == L'\\' && wnp[1] == L'\\' &&
		wnp[2] == L'.' && wnp[3] == L'\\') {
		/* Device names */
		if (((wnp[4] >= L'a' && wnp[4] <= L'z') ||
		     (wnp[4] >= L'A' && wnp[4] <= L'Z')) &&
		    wnp[5] == L':' && wnp[6] == L'\\')
			wnp[2] = L'?';/* Not device names. */
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

static HANDLE
la_CreateFile(const char *path, DWORD dwDesiredAccess, DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
	wchar_t *wpath;
	HANDLE handle;

	handle = CreateFileA(path, dwDesiredAccess, dwShareMode,
	    lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes,
	    hTemplateFile);
	if (handle != INVALID_HANDLE_VALUE)
		return (handle);
	if (GetLastError() != ERROR_PATH_NOT_FOUND)
		return (handle);
	wpath = permissive_name(path);
	if (wpath == NULL)
		return (handle);
	handle = CreateFileW(wpath, dwDesiredAccess, dwShareMode,
	    lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes,
	    hTemplateFile);
	free(wpath);
	return (handle);
}

static void *
la_GetFunctionKernel32(const char *name)
{
	static HINSTANCE lib;
	static int set;
	if (!set) {
		set = 1;
		lib = LoadLibrary("kernel32.dll");
	}
	if (lib == NULL) {
		fprintf(stderr, "Can't load kernel32.dll?!\n");
		exit(1);
	}
	return (void *)GetProcAddress(lib, name);
}

static int
la_CreateHardLinkW(wchar_t *linkname, wchar_t *target)
{
	static BOOLEAN (WINAPI *f)(LPWSTR, LPWSTR, LPSECURITY_ATTRIBUTES);
	static int set;
	if (!set) {
		set = 1;
		f = la_GetFunctionKernel32("CreateHardLinkW");
	}
	return f == NULL ? 0 : (*f)(linkname, target, NULL);
}


/* Make a link to src called dst.  */
static int
__link(const char *src, const char *dst)
{
	wchar_t *wsrc, *wdst;
	int res, retval;
	DWORD attr;

	if (src == NULL || dst == NULL) {
		set_errno (EINVAL);
		return -1;
	}

	wsrc = permissive_name(src);
	wdst = permissive_name(dst);
	if (wsrc == NULL || wdst == NULL) {
		free(wsrc);
		free(wdst);
		set_errno (EINVAL);
		return -1;
	}

	if ((attr = GetFileAttributesW(wsrc)) != (DWORD)-1) {
		res = la_CreateHardLinkW(wdst, wsrc);
	} else {
		/* wsrc does not exist; try src prepend it with the dirname of wdst */
		wchar_t *wnewsrc, *slash;
		int i, n, slen, wlen;

		if (strlen(src) >= 3 && isalpha((unsigned char)src[0]) &&
		    src[1] == ':' && src[2] == '\\') {
			/* Original src name is already full-path */
			retval = -1;
			goto exit;
		}
		if (src[0] == '\\') {
			/* Original src name is almost full-path
			 * (maybe src name is without drive) */
			retval = -1;
			goto exit;
		}

		wnewsrc = malloc ((wcslen(wsrc) + wcslen(wdst) + 1) * sizeof(wchar_t));
		if (wnewsrc == NULL) {
			errno = ENOMEM;
			retval = -1;
			goto exit;
		}
		/* Copying a dirname of wdst */
		wcscpy(wnewsrc, wdst);
		slash = wcsrchr(wnewsrc, L'\\');
		if (slash != NULL)
			*++slash = L'\0';
		else
			wcscat(wnewsrc, L"\\");
		/* Converting multi-byte src to wide-char src */
		wlen = (int)wcslen(wsrc);
		slen = (int)strlen(src);
		n = MultiByteToWideChar(CP_ACP, 0, src, slen, wsrc, wlen);
		if (n == 0) {
			free (wnewsrc);
			retval = -1;
			goto exit;
		}
		for (i = 0; i < n; i++)
			if (wsrc[i] == L'/')
				wsrc[i] = L'\\';
		wcsncat(wnewsrc, wsrc, n);
		/* Check again */
		attr = GetFileAttributesW(wnewsrc);
		if (attr == (DWORD)-1 || (attr & FILE_ATTRIBUTE_DIRECTORY) != 0) {
			if (attr == (DWORD)-1)
				la_dosmaperr(GetLastError());
			else
				errno = EPERM;
			free (wnewsrc);
			retval = -1;
			goto exit;
		}
		res = la_CreateHardLinkW(wdst, wnewsrc);
		free (wnewsrc);
	}
	if (res == 0) {
		la_dosmaperr(GetLastError());
		retval = -1;
	} else
		retval = 0;
exit:
	free(wsrc);
	free(wdst);
	return (retval);
}

/* Make a hard link to src called dst.  */
int
__la_link(const char *src, const char *dst)
{
	return __link(src, dst);
}

int
__la_ftruncate(int fd, off_t length)
{
	LARGE_INTEGER distance;
	HANDLE handle;

	if (fd < 0) {
		errno = EBADF;
		return (-1);
	}
	handle = (HANDLE)_get_osfhandle(fd);
	if (GetFileType(handle) != FILE_TYPE_DISK) {
		errno = EBADF;
		return (-1);
	}
	distance.QuadPart = length;
	if (!SetFilePointerEx(handle, distance, NULL, FILE_BEGIN)) {
		la_dosmaperr(GetLastError());
		return (-1);
	}
	if (!SetEndOfFile(handle)) {
		la_dosmaperr(GetLastError());
		return (-1);
	}
	return (0);
}

#define WINTIME(sec, usec)	((Int32x32To64(sec, 10000000) + EPOC_TIME) + (usec * 10))
static int
__hutimes(HANDLE handle, const struct __timeval *times)
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
		errno = EINVAL;
		return (-1);
	}
	return (0);
}

int
__la_futimes(int fd, const struct __timeval *times)
{

	return (__hutimes((HANDLE)_get_osfhandle(fd), times));
}

int
__la_utimes(const char *name, const struct __timeval *times)
{
	int ret;
	HANDLE handle;

	handle = la_CreateFile(name, GENERIC_READ | GENERIC_WRITE,
	    FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
	    FILE_FLAG_BACKUP_SEMANTICS, NULL);
	if (handle == INVALID_HANDLE_VALUE) {
		la_dosmaperr(GetLastError());
		return (-1);
	}
	ret = __hutimes(handle, times);
	CloseHandle(handle);
	return (ret);
}

int
__la_chdir(const char *path)
{
	wchar_t *ws;
	int r;

	r = SetCurrentDirectoryA(path);
	if (r == 0) {
		if (GetLastError() != ERROR_FILE_NOT_FOUND) {
			la_dosmaperr(GetLastError());
			return (-1);
		}
	} else
		return (0);
	ws = permissive_name(path);
	if (ws == NULL) {
		errno = EINVAL;
		return (-1);
	}
	r = SetCurrentDirectoryW(ws);
	free(ws);
	if (r == 0) {
		la_dosmaperr(GetLastError());
		return (-1);
	}
	return (0);
}

int
__la_chmod(const char *path, mode_t mode)
{
	wchar_t *ws;
	DWORD attr;
	BOOL r;

	ws = NULL;
	attr = GetFileAttributesA(path);
	if (attr == (DWORD)-1) {
		if (GetLastError() != ERROR_FILE_NOT_FOUND) {
			la_dosmaperr(GetLastError());
			return (-1);
		}
		ws = permissive_name(path);
		if (ws == NULL) {
			errno = EINVAL;
			return (-1);
		}
		attr = GetFileAttributesW(ws);
		if (attr == (DWORD)-1) {
			free(ws);
			la_dosmaperr(GetLastError());
			return (-1);
		}
	}
	if (mode & _S_IWRITE)
		attr &= ~FILE_ATTRIBUTE_READONLY;
	else
		attr |= FILE_ATTRIBUTE_READONLY;
	if (ws == NULL)
		r = SetFileAttributesA(path, attr);
	else {
		r = SetFileAttributesW(ws, attr);
		free(ws);
	}
	if (r == 0) {
		la_dosmaperr(GetLastError());
		return (-1);
	}
	return (0);
}

/*
 * This fcntl is limited implemention.
 */
int
__la_fcntl(int fd, int cmd, int val)
{
	HANDLE handle;

	handle = (HANDLE)_get_osfhandle(fd);
	if (GetFileType(handle) == FILE_TYPE_PIPE) {
		if (cmd == F_SETFL && val == 0) {
			DWORD mode = PIPE_WAIT;
			if (SetNamedPipeHandleState(
			    handle, &mode, NULL, NULL) != 0)
				return (0);
		}
	}
	errno = EINVAL;
	return (-1);
}

__int64
__la_lseek(int fd, __int64 offset, int whence)
{
	LARGE_INTEGER distance;
	LARGE_INTEGER newpointer;
	HANDLE handle;

	if (fd < 0) {
		errno = EBADF;
		return (-1);
	}
	handle = (HANDLE)_get_osfhandle(fd);
	if (GetFileType(handle) != FILE_TYPE_DISK) {
		errno = EBADF;
		return (-1);
	}
	distance.QuadPart = offset;
	if (!SetFilePointerEx(handle, distance, &newpointer, whence)) {
		DWORD lasterr;

		lasterr = GetLastError();
		if (lasterr == ERROR_BROKEN_PIPE)
			return (0);
		if (lasterr == ERROR_ACCESS_DENIED)
			errno = EBADF;
		else
			la_dosmaperr(lasterr);
		return (-1);
	}
	return (newpointer.QuadPart);
}

int
__la_mkdir(const char *path, mode_t mode)
{
	wchar_t *ws;
	int r;

	(void)mode;/* UNUSED */
	r = CreateDirectoryA(path, NULL);
	if (r == 0) {
		DWORD lasterr = GetLastError();
		if (lasterr != ERROR_FILENAME_EXCED_RANGE &&
			lasterr != ERROR_PATH_NOT_FOUND) {
			la_dosmaperr(GetLastError());
			return (-1);
		}
	} else
		return (0);
	ws = permissive_name(path);
	if (ws == NULL) {
		errno = EINVAL;
		return (-1);
	}
	r = CreateDirectoryW(ws, NULL);
	free(ws);
	if (r == 0) {
		la_dosmaperr(GetLastError());
		return (-1);
	}
	return (0);
}

/* Windows' mbstowcs is differrent error handling from other unix mbstowcs.
 * That one is using MultiByteToWideChar function with MB_PRECOMPOSED and
 * MB_ERR_INVALID_CHARS flags.
 * This implements for only to pass libarchive_test.
 */
size_t
__la_mbstowcs(wchar_t *wcstr, const char *mbstr, size_t nwchars)
{

	return (MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS,
	    mbstr, (int)strlen(mbstr), wcstr,
	    (int)nwchars));
}

int
__la_open(const char *path, int flags, ...)
{
	va_list ap;
	wchar_t *ws;
	int r, pmode;
	DWORD attr;

	va_start(ap, flags);
	pmode = va_arg(ap, int);
	va_end(ap);
	ws = NULL;
	if ((flags & ~O_BINARY) == O_RDONLY) {
		/*
		 * When we open a directory, _open function returns 
		 * "Permission denied" error.
		 */
		attr = GetFileAttributesA(path);
		if (attr == (DWORD)-1 && GetLastError() == ERROR_PATH_NOT_FOUND) {
			ws = permissive_name(path);
			if (ws == NULL) {
				errno = EINVAL;
				return (-1);
			}
			attr = GetFileAttributesW(ws);
		}
		if (attr == (DWORD)-1) {
			la_dosmaperr(GetLastError());
			free(ws);
			return (-1);
		}
		if (attr & FILE_ATTRIBUTE_DIRECTORY) {
			HANDLE handle;

			if (ws != NULL)
				handle = CreateFileW(ws, 0, 0, NULL,
				    OPEN_EXISTING,
				    FILE_FLAG_BACKUP_SEMANTICS |
				    FILE_ATTRIBUTE_READONLY,
					NULL);
			else
				handle = CreateFileA(path, 0, 0, NULL,
				    OPEN_EXISTING,
				    FILE_FLAG_BACKUP_SEMANTICS |
				    FILE_ATTRIBUTE_READONLY,
					NULL);
			free(ws);
			if (handle == INVALID_HANDLE_VALUE) {
				la_dosmaperr(GetLastError());
				return (-1);
			}
			r = _open_osfhandle((intptr_t)handle, _O_RDONLY);
			return (r);
		}
	}
	if (ws == NULL) {
#if defined(__BORLANDC__)
		/* Borland has no mode argument.
		   TODO: Fix mode of new file.  */
		r = _open(path, flags);
#else
		r = _open(path, flags, pmode);
#endif
		if (r < 0 && errno == EACCES && (flags & O_CREAT) != 0) {
			/* simular other POSIX system action to pass a test */
			attr = GetFileAttributesA(path);
			if (attr == (DWORD)-1)
				la_dosmaperr(GetLastError());
			else if (attr & FILE_ATTRIBUTE_DIRECTORY)
				errno = EISDIR;
			else
				errno = EACCES;
			return (-1);
		}
		if (r >= 0 || errno != ENOENT)
			return (r);
		ws = permissive_name(path);
		if (ws == NULL) {
			errno = EINVAL;
			return (-1);
		}
	}
	r = _wopen(ws, flags, pmode);
	if (r < 0 && errno == EACCES && (flags & O_CREAT) != 0) {
		/* simular other POSIX system action to pass a test */
		attr = GetFileAttributesW(ws);
		if (attr == (DWORD)-1)
			la_dosmaperr(GetLastError());
		else if (attr & FILE_ATTRIBUTE_DIRECTORY)
			errno = EISDIR;
		else
			errno = EACCES;
	}
	free(ws);
	return (r);
}

ssize_t
__la_read(int fd, void *buf, size_t nbytes)
{
	HANDLE handle;
	DWORD bytes_read, lasterr;
	int r;

#ifdef _WIN64
	if (nbytes > UINT32_MAX)
		nbytes = UINT32_MAX;
#endif
	if (fd < 0) {
		errno = EBADF;
		return (-1);
	}
	handle = (HANDLE)_get_osfhandle(fd);
	if (GetFileType(handle) == FILE_TYPE_PIPE) {
		DWORD sta;
		if (GetNamedPipeHandleState(
		    handle, &sta, NULL, NULL, NULL, NULL, 0) != 0 &&
		    (sta & PIPE_NOWAIT) == 0) {
			DWORD avail = -1;
			int cnt = 3;

			while (PeekNamedPipe(
			    handle, NULL, 0, NULL, &avail, NULL) != 0 &&
			    avail == 0 && --cnt)
				Sleep(100);
			if (avail == 0)
				return (0);
		}
	}
	r = ReadFile(handle, buf, (uint32_t)nbytes,
	    &bytes_read, NULL);
	if (r == 0) {
		lasterr = GetLastError();
		if (lasterr == ERROR_NO_DATA) {
			errno = EAGAIN;
			return (-1);
		}
		if (lasterr == ERROR_BROKEN_PIPE)
			return (0);
		if (lasterr == ERROR_ACCESS_DENIED)
			errno = EBADF;
		else
			la_dosmaperr(lasterr);
		return (-1);
	}
	return ((ssize_t)bytes_read);
}

/* Remove directory */
int
__la_rmdir(const char *path)
{
	wchar_t *ws;
	int r;

	r = _rmdir(path);
	if (r >= 0 || errno != ENOENT)
		return (r);
	ws = permissive_name(path);
	if (ws == NULL) {
		errno = EINVAL;
		return (-1);
	}
	r = _wrmdir(ws);
	free(ws);
	return (r);
}

/* Convert Windows FILETIME to UTC */
__inline static void
fileTimeToUTC(const FILETIME *filetime, time_t *time, long *ns)
{
	ULARGE_INTEGER utc;

	utc.HighPart = filetime->dwHighDateTime;
	utc.LowPart  = filetime->dwLowDateTime;
	if (utc.QuadPart >= EPOC_TIME) {
		utc.QuadPart -= EPOC_TIME;
		*time = (time_t)(utc.QuadPart / 10000000);	/* milli seconds base */
		*ns = (long)(utc.QuadPart % 10000000) * 100;/* nano seconds base */
	} else {
		*time = 0;
		*ns = 0;
	}
}

/* Stat by handle
 * Windows' stat() does not accept path which is added "\\?\" especially "?"
 * character.
 * It means we cannot access a long name path(which is longer than MAX_PATH).
 * So I've implemented simular Windows' stat() to access the long name path.
 * And I've added some feature.
 * 1. set st_ino by nFileIndexHigh and nFileIndexLow of
 *    BY_HANDLE_FILE_INFORMATION.
 * 2. set st_nlink by nNumberOfLinks of BY_HANDLE_FILE_INFORMATION.
 * 3. set st_dev by dwVolumeSerialNumber by BY_HANDLE_FILE_INFORMATION.
 */
static int
__hstat(HANDLE handle, struct ustat *st)
{
	BY_HANDLE_FILE_INFORMATION info;
	ULARGE_INTEGER ino64;
	DWORD ftype;
	mode_t mode;
	time_t time;
	long ns;

	switch (ftype = GetFileType(handle)) {
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
			if (PeekNamedPipe(handle, NULL, 0, NULL, &avail, NULL))
				st->st_size = avail;
			else
				st->st_size = 0;
		}
		st->st_atime = 0;
		st->st_atime_nsec = 0;
		st->st_mtime = 0;
		st->st_mtime_nsec = 0;
		st->st_ctime = 0;
		st->st_ctime_nsec = 0;
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
		la_dosmaperr(GetLastError());
		return (-1);
	}

	ZeroMemory(&info, sizeof(info));
	if (!GetFileInformationByHandle (handle, &info)) {
		la_dosmaperr(GetLastError());
		return (-1);
	}

	mode = S_IRUSR | S_IRGRP | S_IROTH;
	if ((info.dwFileAttributes & FILE_ATTRIBUTE_READONLY) == 0)
		mode |= S_IWUSR | S_IWGRP | S_IWOTH;
	if (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		mode |= S_IFDIR | S_IXUSR | S_IXGRP | S_IXOTH;
	else
		mode |= S_IFREG;
	st->st_mode = mode;
	
	fileTimeToUTC(&info.ftLastAccessTime, &time, &ns);
	st->st_atime = time; 
	st->st_atime_nsec = ns;
	fileTimeToUTC(&info.ftLastWriteTime, &time, &ns);
	st->st_mtime = time;
	st->st_mtime_nsec = ns;
	fileTimeToUTC(&info.ftCreationTime, &time, &ns);
	st->st_ctime = time;
	st->st_ctime_nsec = ns;
	st->st_size = 
	    ((int64_t)(info.nFileSizeHigh) * ((int64_t)MAXDWORD + 1))
		+ (int64_t)(info.nFileSizeLow);
#ifdef SIMULATE_WIN_STAT
	st->st_ino = 0;
	st->st_nlink = 1;
	st->st_dev = 0;
#else
	/* Getting FileIndex as i-node. We have to remove a sequence which
	 * is high-16-bits of nFileIndexHigh. */
	ino64.HighPart = info.nFileIndexHigh & 0x0000FFFFUL;
	ino64.LowPart  = info.nFileIndexLow;
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

static void
copy_stat(struct stat *st, struct ustat *us)
{
	st->st_atime = us->st_atime;
	st->st_ctime = us->st_ctime;
	st->st_mtime = us->st_mtime;
	st->st_gid = us->st_gid;
	st->st_ino = getino(us);
	st->st_mode = us->st_mode;
	st->st_nlink = us->st_nlink;
	st->st_size = us->st_size;
	st->st_uid = us->st_uid;
	st->st_dev = us->st_dev;
	st->st_rdev = us->st_rdev;
}

int
__la_fstat(int fd, struct stat *st)
{
	struct ustat u;
	int ret;

	if (fd < 0) {
		errno = EBADF;
		return (-1);
	}
	ret = __hstat((HANDLE)_get_osfhandle(fd), &u);
	if (ret >= 0) {
		copy_stat(st, &u);
		if (u.st_mode & (S_IFCHR | S_IFIFO)) {
			st->st_dev = fd;
			st->st_rdev = fd;
		}
	}
	return (ret);
}

int
__la_stat(const char *path, struct stat *st)
{
	HANDLE handle;
	struct ustat u;
	int ret;

	handle = la_CreateFile(path, 0, 0, NULL, OPEN_EXISTING,
		FILE_FLAG_BACKUP_SEMANTICS | FILE_ATTRIBUTE_READONLY,
		NULL);
	if (handle == INVALID_HANDLE_VALUE) {
		la_dosmaperr(GetLastError());
		return (-1);
	}
	ret = __hstat(handle, &u);
	CloseHandle(handle);
	if (ret >= 0) {
		char *p;

		copy_stat(st, &u);
		p = strrchr(path, '.');
		if (p != NULL && strlen(p) == 4) {
			char exttype[4];

			++ p;
			exttype[0] = toupper(*p++);
			exttype[1] = toupper(*p++);
			exttype[2] = toupper(*p++);
			exttype[3] = '\0';
			if (!strcmp(exttype, "EXE") || !strcmp(exttype, "CMD") ||
				!strcmp(exttype, "BAT") || !strcmp(exttype, "COM"))
				st->st_mode |= S_IXUSR | S_IXGRP | S_IXOTH;
		}
	}
	return (ret);
}

int
__la_unlink(const char *path)
{
	wchar_t *ws;
	int r;

	r = _unlink(path);
	if (r >= 0 || errno != ENOENT)
		return (r);
	ws = permissive_name(path);
	if (ws == NULL) {
		errno = EINVAL;
		return (-1);
	}
	r = _wunlink(ws);
	free(ws);
	return (r);
}

/*
 * This waitpid is limited implemention.
 */
pid_t
__la_waitpid(pid_t wpid, int *status, int option)
{
	HANDLE child;
	DWORD cs, ret;

	(void)option;/* UNUSED */
	child = OpenProcess(PROCESS_QUERY_INFORMATION | SYNCHRONIZE, FALSE, wpid);
	if (child == NULL) {
		la_dosmaperr(GetLastError());
		return (-1);
	}
	ret = WaitForSingleObject(child, INFINITE);
	if (ret == WAIT_FAILED) {
		CloseHandle(child);
		la_dosmaperr(GetLastError());
		return (-1);
	}
	if (GetExitCodeProcess(child, &cs) == 0) {
		CloseHandle(child);
		la_dosmaperr(GetLastError());
		return (-1);
	}
	if (cs == STILL_ACTIVE)
		*status = 0x100;
	else
		*status = (int)(cs & 0xff);
	CloseHandle(child);
	return (wpid);
}

ssize_t
__la_write(int fd, const void *buf, size_t nbytes)
{
	DWORD bytes_written;

#ifdef _WIN64
	if (nbytes > UINT32_MAX)
		nbytes = UINT32_MAX;
#endif
	if (fd < 0) {
		errno = EBADF;
		return (-1);
	}
	if (!WriteFile((HANDLE)_get_osfhandle(fd), buf, (uint32_t)nbytes,
	    &bytes_written, NULL)) {
		DWORD lasterr;

		lasterr = GetLastError();
		if (lasterr == ERROR_ACCESS_DENIED)
			errno = EBADF;
		else
			la_dosmaperr(lasterr);
		return (-1);
	}
	return (bytes_written);
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
	{	ERROR_NOT_ENOUGH_QUOTA, ENOMEM	}
};

static void
la_dosmaperr(unsigned long e)
{
	int			i;

	if (e == 0)
	{
		errno = 0;
		return;
	}

	for (i = 0; i < sizeof(doserrors); i++)
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

#if defined(ARCHIVE_HASH_MD5_WIN)    ||\
    defined(ARCHIVE_HASH_SHA1_WIN)   || defined(ARCHIVE_HASH_SHA256_WIN) ||\
    defined(ARCHIVE_HASH_SHA384_WIN) || defined(ARCHIVE_HASH_SHA512_WIN)
/*
 * Message digest functions.
 */
void
__la_hash_Init(Digest_CTX *ctx, ALG_ID algId)
{

	ctx->valid = 0;
	if (!CryptAcquireContext(&ctx->cryptProv, NULL, NULL,
	    PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
		if (GetLastError() != (DWORD)NTE_BAD_KEYSET)
			return;
		if (!CryptAcquireContext(&ctx->cryptProv, NULL, NULL,
		    PROV_RSA_FULL, CRYPT_NEWKEYSET))
			return;
	}

	if (!CryptCreateHash(ctx->cryptProv, algId, 0, 0, &ctx->hash)) {
		CryptReleaseContext(ctx->cryptProv, 0);
		return;
	}

	ctx->valid = 1;
}

void
__la_hash_Update(Digest_CTX *ctx, const unsigned char *buf, size_t len)
{

	if (!ctx->valid)
	return;

	CryptHashData(ctx->hash,
		      (unsigned char *)(uintptr_t)buf,
		      (DWORD)len, 0);
}

void
__la_hash_Final(unsigned char *buf, size_t bufsize, Digest_CTX *ctx)
{
	DWORD siglen = bufsize;

	if (!ctx->valid)
		return;

	CryptGetHashParam(ctx->hash, HP_HASHVAL, buf, &siglen, 0);
	CryptDestroyHash(ctx->hash);
	CryptReleaseContext(ctx->cryptProv, 0);
	ctx->valid = 0;
}

#endif /* defined(ARCHIVE_HASH_*_WIN) */

#endif /* _WIN32 && !__CYGWIN__ */
