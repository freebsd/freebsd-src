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
typedef unsigned int host_mode_t;
typedef unsigned int host_uid_t;
typedef unsigned int host_gid_t;
typedef int64_t host_off_t;
typedef long host_blksize_t;
typedef int64_t host_blkcnt_t;

#include "stat_arch.h"

/*
 * stat flags
 * These are arch independent and match the values in nolib and uapi headers
 * with HOST_ prepended.
 */
#define	HOST_S_IFMT	0170000
#define	HOST_S_IFIFO	0010000
#define	HOST_S_IFCHR	0020000
#define	HOST_S_IFDIR	0040000
#define	HOST_S_IFBLK	0060000
#define	HOST_S_IFREG	0100000
#define	HOST_S_IFLNK	0120000
#define	HOST_S_IFSOCK	0140000

#define	HOST_S_ISBLK(mode)	(((mode) & HOST_S_IFMT) == HOST_S_IFBLK)
#define	HOST_S_ISCHR(mode)	(((mode) & HOST_S_IFMT) == HOST_S_IFCHR)
#define	HOST_S_ISDIR(mode)	(((mode) & HOST_S_IFMT) == HOST_S_IFDIR)
#define	HOST_S_ISFIFO(mode)	(((mode) & HOST_S_IFMT) == HOST_S_IFIFO)
#define	HOST_S_ISLNK(mode)	(((mode) & HOST_S_IFMT) == HOST_S_IFLNK)
#define	HOST_S_ISREG(mode)	(((mode) & HOST_S_IFMT) == HOST_S_IFREG)
#define	HOST_S_ISSOCK(mode)	(((mode) & HOST_S_IFMT) == HOST_S_IFSOCK)

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

#define HOST_AT_FDCWD		-100            /* Relative to current directory */

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

/*
 * Must match Linux's values see linux/tools/include/uapi/asm-generic/mman-common.h
 * and linux/tools/include/linux/mman.h
 *
 * And pre-pend HOST_ here.
 */
#define HOST_PROT_READ	0x1
#define HOST_PROT_WRITE	0x2
#define HOST_PROT_EXEC	0x4

#define HOST_MAP_SHARED		0x01
#define	HOST_MAP_PRIVATE	0x02
#define HOST_MAP_FIXED		0x10
#define HOST_MAP_ANONYMOUS	0x20

/* Mount flags from uapi */
#define MS_RELATIME (1 << 21)

#define HOST_REBOOT_MAGIC1	0xfee1dead
#define HOST_REBOOT_MAGIC2	672274793
#define HOST_REBOOT_CMD_KEXEC	0x45584543

/*
 * Values from linux/tools/include/uapi/linux/kexec.h
 */

/*
 * Values match ELF architecture types.
 */
#define HOST_KEXEC_ARCH_X86_64  (62 << 16)
#define HOST_KEXEC_ARCH_PPC64   (21 << 16)
#define HOST_KEXEC_ARCH_ARM     (40 << 16)
#define HOST_KEXEC_ARCH_AARCH64 (183 << 16)
#define HOST_KEXEC_ARCH_RISCV   (243 << 16)

/* Arbitrary cap on segments */
#define HOST_KEXEC_SEGMENT_MAX 16

struct host_kexec_segment {
	void *buf;
	int bufsz;
	void *mem;
	int memsz;
};

struct host_dirent64 {
	uint64_t	d_ino;		/* 64-bit inode number */
	int64_t		d_off;		/* 64-bit offset to next structure */
	unsigned short	d_reclen;	/* Size of this dirent */
	unsigned char	d_type;		/* File type */
	char		d_name[];	/* Filename (null-terminated) */
};

/* d_type values */
#define HOST_DT_UNKNOWN		 0
#define HOST_DT_FIFO		 1
#define HOST_DT_CHR		 2
#define HOST_DT_DIR		 4
#define HOST_DT_BLK		 6
#define HOST_DT_REG		 8
#define HOST_DT_LNK		10
#define HOST_DT_SOCK		12
#define HOST_DT_WHT		14

/*
 * System Calls
 */
int host_close(int fd);
int host_dup(int fd);
int host_exit(int code);
int host_fstat(int fd, struct host_kstat *sb);
int host_getdents64(int fd, void *dirp, int count);
int host_getpid(void);
int host_gettimeofday(struct host_timeval *a, void *b);
int host_ioctl(int fd, unsigned long request, unsigned long arg);
int host_kexec_load(unsigned long entry, unsigned long nsegs, struct host_kexec_segment *segs, unsigned long flags);
ssize_t host_llseek(int fd, int32_t offset_high, int32_t offset_lo, uint64_t *result, int whence);
int host_mkdir(const char *, host_mode_t);
void *host_mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off);
int host_mount(const char *src, const char *target, const char *type,
    unsigned long flags, void *data);
int host_munmap(void *addr, size_t len);
int host_open(const char *path, int flags, int mode);
ssize_t host_read(int fd, void *buf, size_t nbyte);
int host_reboot(int, int, int, uintptr_t);
int host_select(int nfds, long *readfds, long *writefds, long *exceptfds,
    struct host_timeval *timeout);
int host_stat(const char *path, struct host_kstat *sb);
int host_symlink(const char *path1, const char *path2);
int host_uname(struct old_utsname *);
ssize_t host_write(int fd, const void *buf, size_t nbyte);

/*
 * Wrappers / one-liners
 */
#define host_getmem(size) \
	host_mmap(0, size, HOST_PROT_READ | HOST_PROT_WRITE, \
	    HOST_MAP_PRIVATE | HOST_MAP_ANONYMOUS, -1, 0);

#endif
