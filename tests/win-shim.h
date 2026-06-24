/*
 * win-shim.h
 * Windows function/define shims
 *
 * SPDX-License-Identifier: pkgconf
 *
 * Copyright (c) 2025 pkgconf authors (see AUTHORS).
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */

#ifndef WIN_SHIM_H
#define WIN_SHIM_H

#ifdef _WIN32

#include <direct.h>
#include <io.h>

// Shims shared by both MSVC and MSYS2
#define mkdir(p, m) _mkdir(p)
#define setenv(n, v, o) _putenv_s(n, v)
#define unsetenv(n) _putenv_s(n, "")

#ifndef PATH_MAX
#	define PATH_MAX MAX_PATH
#endif // !PATH_MAX

// MSVC-specific shims
#ifdef _MSC_VER
#	define getcwd _getcwd
#	define chdir _chdir
#	define rmdir _rmdir
#	define lstat _lstat
#	define unlink _unlink
#	define popen _popen
#	define pclose _pclose

static inline char *
mkdtemp(char *tmpl)
{
	if (_mktemp_s(tmpl, strlen(tmpl) + 1) != 0)
		return NULL;
	if (_mkdir(tmpl) != 0)
		return NULL;
	return tmpl;
}
#endif // _MSC_VER

#endif // _WIN32
#endif // WIN_SHIM_H 
