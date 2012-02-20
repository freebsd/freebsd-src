/*$Header: /p/tcsh/cvsroot/tcsh/win32/dirent.h,v 1.6 2006/03/03 22:08:45 amold Exp $*/
/*-
 * Copyright (c) 1980, 1991 The Regents of the University of California.
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

/*
 * dirent.h
 * directory interface functions. Sort of like dirent functions on unix.
 * -amol
 *
 */
#ifndef DIRENT_H
#define DIRENT_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define heap_alloc(s) HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,(s))
#define heap_free(p) HeapFree(GetProcessHeap(),0,(p))
#define heap_realloc(p,s) HeapReAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,(p),(s))

#define NAME_MAX MAX_PATH

#define IS_ROOT 0x01
#define IS_NET  0x02

struct dirent {
	long            d_ino;
	int             d_off;
	unsigned short  d_reclen;
	char            d_name[NAME_MAX+1];
};

typedef struct {
	HANDLE dd_fd;
	int dd_loc;
	int dd_size;
	int flags;
	char orig_dir_name[NAME_MAX +1];
	struct dirent *dd_buf;
}DIR;

DIR *opendir(const char*);
struct dirent *readdir(DIR*);
int closedir(DIR*);
void rewinddir(DIR*);
#endif DIRENT_H
