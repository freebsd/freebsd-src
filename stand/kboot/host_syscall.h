/*-
 * Copyright (C) 2014 Nathan Whitehorn
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _HOST_SYSCALL_H
#define _HOST_SYSCALL_H

#include <stand.h>

long host_syscall(int number, ...);

/*
 * Sizes taken from musl's include/alltypes.h.in and expanded for LP64 hosts
 */
typedef uint64_t host_dev_t;
typedef uint64_t host_ino_t;
typedef int64_t host_nlink_t;
typedef unsigned int host_mode_t;
typedef unsigned int host_uid_t;
typedef unsigned int host_gid_t;
typedef int64_t host_off_t;
typedef long host_blksize_t;
typedef int64_t host_blkcnt_t;

#include "stat_arch.h"

/*
 * Constants for open, fcntl, etc
 *
 * Note: Some of these are arch dependent on Linux, but are the same for
 * powerpc, x86, arm*, and riscv. We should be futureproof, though, since these
 * are the 'generic' values and only older architectures (no longer supported by
 * FreeBSD) vary.
 *
 * These are from tools/include/uapi/asm-generic/fcntl.h and use the octal
 * notation. Beware, hex is used in other places creating potential confsion.
 */
#define HOST_O_RDONLY		    0
#define HOST_O_WRONLY		    1
#define HOST_O_RDWR		    2
#define HOST_O_CREAT		00100
#define HOST_O_EXCL		00200
#define HOST_O_NOCTTY		00400
#define HOST_O_TRUNC		01000
#define HOST_O_APPEND		02000
#define HOST_O_NONBLOCK		04000

/*
 * Data types
 */
struct old_utsname {
	char sysname[65];
	char nodename[65];
	char release[65];
	char version[65];
	char machine[65];
};

struct host_timeval {
	time_t tv_sec;
	long tv_usec;
};

#define HOST_AT_FDCWD		-100		/* Relative to current directory */

/*
 * System Calls
 */
int host_close(int fd);
int host_fstat(int fd, struct host_kstat *sb);
int host_getdents(int fd, void *dirp, int count);
int host_gettimeofday(struct host_timeval *a, void *b);
int host_kexec_load(uint32_t start, int nsegs, uint32_t segs, uint32_t flags);
ssize_t host_llseek(int fd, int32_t offset_high, int32_t offset_lo, uint64_t *result, int whence);
void *host_mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off);
int host_munmap(void *addr, size_t len);
int host_open(const char *path, int flags, int mode);
ssize_t host_read(int fd, void *buf, size_t nbyte);
int host_reboot(int, int, int, uintptr_t);
int host_select(int nfds, long *readfds, long *writefds, long *exceptfds,
    struct host_timeval *timeout);
int host_stat(const char *path, struct host_kstat *sb);
int host_uname(struct old_utsname *);
ssize_t host_write(int fd, const void *buf, size_t nbyte);

/*
 * Wrappers / one-liners
 */
#define host_getmem(size) host_mmap(0, size, 3 /* RW */, 0x22 /* ANON */, -1, 0);

#endif
