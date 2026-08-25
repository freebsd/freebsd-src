/*
 * win-dirent.h - minimal POSIX dirent shim for Windows (MSVC / MinGW-w64)
 *
 * Implements only opendir(), readdir(), closedir(): the subset used by
 * pkgconf and its test suite.
 *
 * Deliberately not included under Cygwin, which provides its own dirent.h.
 *
 * SPDX-License-Identifier: pkgconf
 *
 * Copyright (c) 2026 Elizabeth Ashford. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */

#ifndef PKGCONF_WIN_DIRENT_H
#define PKGCONF_WIN_DIRENT_H

#if defined(_WIN32) && !defined(__CYGWIN__)

#ifndef WIN32_LEAN_AND_MEAN
#   define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#ifndef PATH_MAX
#	define PATH_MAX MAX_PATH
#endif
#ifndef NAME_MAX
#	define NAME_MAX MAX_PATH
#endif

// d_type constants: only the values pkgconf actually tests against
#define DT_UNKNOWN  0
#define DT_REG      8 // S_IFREG >> 12
#define DT_DIR      4 // S_IFDIR >> 12

#ifdef __cplusplus
extern "C" {
#endif

/* ============ */
/* Public types */
/* ============ */

struct dirent
{
	// Standard fields pkgconf actually use
	char d_name[PATH_MAX + 1];
	size_t d_namlen;
	int d_type;

	unsigned short d_reclen;
};

typedef struct pkgconf_DIR
{
	WIN32_FIND_DATAW data;
	HANDLE handle;
	struct dirent ent;
	int cached; // non-zero when data holds an unread entry
} DIR;

/* ================ */
/* Internal helpers */
/* ================ */

/*
 * Convert a Win32 file-attribute word to a DT_* constant.
 * Devices can't be distinguished from regular files on win32, so we map everything that isn't a
 * directory to DT_REG.
 */
static inline int
pkgconf__attr_to_dtype(DWORD attr)
{
	if (attr & FILE_ATTRIBUTE_DIRECTORY)
		return DT_DIR;
	return DT_REG;
}

/*
 * Populate ent from the current data field.
 * Returns 0 on success, -1 if the filename could not be converted to UTF-8.
 */
static inline int
pkgconf__fill_dirent(DIR *dirp)
{
	int n = WideCharToMultiByte(
		CP_UTF8, 0,
		dirp->data.cFileName, -1,
		dirp->ent.d_name, PATH_MAX + 1,
		NULL, NULL);

	if (n <= 0)
	{
		/*
		 * Conversion failed.  Rather than returning a broken entry, signal the error so the caller can
		 * skip or abort.
		 */
		errno = EILSEQ;
		return -1;
	}

	dirp->ent.d_namlen = (size_t)(n - 1);
	dirp->ent.d_type = pkgconf__attr_to_dtype(dirp->data.dwFileAttributes);
	dirp->ent.d_reclen = sizeof(struct dirent);
	return 0;
}

/* ========= */
/* API shims */
/* ========= */

static inline DIR *
opendir(const char *path)
{
	DIR *dirp;
	wchar_t wpath[PATH_MAX + 3]; // +3: possible '\', '*', NUL
	wchar_t *p;
	int wlen;

	if (path == NULL || path[0] == '\0')
	{
		errno = ENOENT;
		return NULL;
	}

	// Convert caller-supplied UTF-8 path to wide string
	wlen = MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, PATH_MAX + 1);
	if (wlen <= 0)
	{
		errno = ENOENT;
		return NULL;
	}

	// Append \* search glob, handling paths that already end in a separator
	p = wpath + wlen - 1;
	if (*p != L'\\' && *p != L'/' && *p != L':')
		*p++ = L'\\';
	*p++ = L'*';
	*p = L'\0';

	dirp = (DIR *)malloc(sizeof(DIR));
	if (dirp == NULL)
	{
		errno = ENOMEM;
		return NULL;
	}

	dirp->handle = FindFirstFileExW(
		wpath,
		FindExInfoBasic, // skip short (8.3) names, faster
		&dirp->data,
		FindExSearchNameMatch,
		NULL,
		FIND_FIRST_EX_LARGE_FETCH);

	if (dirp->handle == INVALID_HANDLE_VALUE)
	{
		DWORD err = GetLastError();
		free(dirp);
		switch (err)
		{
		case ERROR_ACCESS_DENIED:
			errno = EACCES;
			break;
		case ERROR_DIRECTORY:
			errno = ENOTDIR;
			break;
		default:       
			errno = ENOENT; 
			break;
		}
		return NULL;
	}

	dirp->cached = 1; // first entry already sitting in data
	return dirp;
}

static inline struct dirent *
readdir(DIR *dirp)
{
	if (dirp == NULL)
	{
		errno = EBADF;
		return NULL;
	}

	if (dirp->cached)
	{
		// Consume the entry that opendir() or the previous FindNextFileW already placed in data
		dirp->cached = 0;
	}
	else
	{
		// Advance to the next entry
		if (dirp->handle == INVALID_HANDLE_VALUE)
			return NULL;

		if (!FindNextFileW(dirp->handle, &dirp->data))
		{
			// End of directory or hard error, stop
			FindClose(dirp->handle);
			dirp->handle = INVALID_HANDLE_VALUE;
			return NULL;
		}
	}

	if (pkgconf__fill_dirent(dirp) != 0)
		return NULL;

	return &dirp->ent;
}

static inline int
closedir(DIR *dirp)
{
	if (dirp == NULL)
	{
		errno = EBADF;
		return -1;
	}

	if (dirp->handle != INVALID_HANDLE_VALUE)
		FindClose(dirp->handle);

	free(dirp);
	return 0;
}

#ifdef __cplusplus
}
#endif

#endif // defined(_WIN32) && !defined(__CYGWIN__)
#endif // PKGCONF_WIN_DIRENT_H
