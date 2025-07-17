/*-
 * SPDX-License-Identifier: BSD-4-Clause
 *
 * Copyright 1997 Sean Eric Fagan
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Sean Eric Fagan
 * 4. Neither the name of the author may be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * This file has routines used to print out system calls and their
 * arguments.
 */

#include <sys/aio.h>
#include <sys/capsicum.h>
#include <sys/types.h>
#define	_WANT_FREEBSD11_KEVENT
#include <sys/event.h>
#include <sys/ioccom.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/poll.h>
#include <sys/procfs.h>
#include <sys/ptrace.h>
#include <sys/resource.h>
#include <sys/sched.h>
#include <sys/socket.h>
#define _WANT_FREEBSD11_STAT
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netinet/sctp.h>
#include <arpa/inet.h>

#include <assert.h>
#include <ctype.h>
#include <err.h>
#define _WANT_KERNEL_ERRNO
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysdecode.h>
#include <unistd.h>
#include <vis.h>

#include "truss.h"
#include "extern.h"
#include "syscall.h"

/*
 * This should probably be in its own file, sorted alphabetically.
 *
 * Note: We only scan this table on the initial syscall number to calling
 * convention lookup, i.e. once each time a new syscall is encountered. This
 * is unlikely to be a performance issue, but if it is we could sort this array
 * and use a binary search instead.
 */
static const struct syscall_decode decoded_syscalls[] = {
	/* Native ABI */
	{ .name = "__acl_aclcheck_fd", .ret_type = 1, .nargs = 3,
	  .args = { { Int, 0 }, { Acltype, 1 }, { Ptr, 2 } } },
	{ .name = "__acl_aclcheck_file", .ret_type = 1, .nargs = 3,
	  .args = { { Name, 0 }, { Acltype, 1 }, { Ptr, 2 } } },
	{ .name = "__acl_aclcheck_link", .ret_type = 1, .nargs = 3,
	  .args = { { Name, 0 }, { Acltype, 1 }, { Ptr, 2 } } },
	{ .name = "__acl_delete_fd", .ret_type = 1, .nargs = 2,
	  .args = { { Int, 0 }, { Acltype, 1 } } },
	{ .name = "__acl_delete_file", .ret_type = 1, .nargs = 2,
	  .args = { { Name, 0 }, { Acltype, 1 } } },
	{ .name = "__acl_delete_link", .ret_type = 1, .nargs = 2,
	  .args = { { Name, 0 }, { Acltype, 1 } } },
	{ .name = "__acl_get_fd", .ret_type = 1, .nargs = 3,
	  .args = { { Int, 0 }, { Acltype, 1 }, { Ptr, 2 } } },
	{ .name = "__acl_get_file", .ret_type = 1, .nargs = 3,
	  .args = { { Name, 0 }, { Acltype, 1 }, { Ptr, 2 } } },
	{ .name = "__acl_get_link", .ret_type = 1, .nargs = 3,
	  .args = { { Name, 0 }, { Acltype, 1 }, { Ptr, 2 } } },
	{ .name = "__acl_set_fd", .ret_type = 1, .nargs = 3,
	  .args = { { Int, 0 }, { Acltype, 1 }, { Ptr, 2 } } },
	{ .name = "__acl_set_file", .ret_type = 1, .nargs = 3,
	  .args = { { Name, 0 }, { Acltype, 1 }, { Ptr, 2 } } },
	{ .name = "__acl_set_link", .ret_type = 1, .nargs = 3,
	  .args = { { Name, 0 }, { Acltype, 1 }, { Ptr, 2 } } },
	{ .name = "__cap_rights_get", .ret_type = 1, .nargs = 3,
	  .args = { { Int, 0 }, { Int, 1 }, { CapRights | OUT, 2 } } },
	{ .name = "__getcwd", .ret_type = 1, .nargs = 2,
	  .args = { { Name | OUT, 0 }, { Int, 1 } } },
	{ .name = "__realpathat", .ret_type = 1, .nargs = 5,
	  .args = { { Atfd, 0 }, { Name | IN, 1 }, { Name | OUT, 2 },
		    { Sizet, 3 }, { Int, 4} } },
	{ .name = "_umtx_op", .ret_type = 1, .nargs = 5,
	  .args = { { Ptr, 0 }, { Umtxop, 1 }, { LongHex, 2 }, { Ptr, 3 },
		    { Ptr, 4 } } },
	{ .name = "accept", .ret_type = 1, .nargs = 3,
	  .args = { { Int, 0 }, { Sockaddr | OUT, 1 }, { Ptr | OUT, 2 } } },
	{ .name = "access", .ret_type = 1, .nargs = 2,
	  .args = { { Name | IN, 0 }, { Accessmode, 1 } } },
	{ .name = "aio_cancel", .ret_type = 1, .nargs = 2,
	  .args = { { Int, 0 }, { Aiocb, 1 } } },
	{ .name = "aio_error", .ret_type = 1, .nargs = 1,
	  .args = { { Aiocb, 0 } } },
	{ .name = "aio_fsync", .ret_type = 1, .nargs = 2,
	  .args = { { AiofsyncOp, 0 }, { Aiocb, 1 } } },
	{ .name = "aio_mlock", .ret_type = 1, .nargs = 1,
	  .args = { { Aiocb, 0 } } },
	{ .name = "aio_read", .ret_type = 1, .nargs = 1,
	  .args = { { Aiocb, 0 } } },
	{ .name = "aio_return", .ret_type = 1, .nargs = 1,
	  .args = { { Aiocb, 0 } } },
	{ .name = "aio_suspend", .ret_type = 1, .nargs = 3,
	  .args = { { AiocbArray, 0 }, { Int, 1 }, { Timespec, 2 } } },
	{ .name = "aio_waitcomplete", .ret_type = 1, .nargs = 2,
	  .args = { { AiocbPointer | OUT, 0 }, { Timespec, 1 } } },
	{ .name = "aio_write", .ret_type = 1, .nargs = 1,
	  .args = { { Aiocb, 0 } } },
	{ .name = "bind", .ret_type = 1, .nargs = 3,
	  .args = { { Int, 0 }, { Sockaddr | IN, 1 }, { Socklent, 2 } } },
	{ .name = "bindat", .ret_type = 1, .nargs = 4,
	  .args = { { Atfd, 0 }, { Int, 1 }, { Sockaddr | IN, 2 },
		    { Int, 3 } } },
	{ .name = "break", .ret_type = 1, .nargs = 1,
	  .args = { { Ptr, 0 } } },
	{ .name = "cap_fcntls_get", .ret_type = 1, .nargs = 2,
	  .args = { { Int, 0 }, { CapFcntlRights | OUT, 1 } } },
	{ .name = "cap_fcntls_limit", .ret_type = 1, .nargs = 2,
	  .args = { { Int, 0 }, { CapFcntlRights, 1 } } },
	{ .name = "cap_getmode", .ret_type = 1, .nargs = 1,
	  .args = { { PUInt | OUT, 0 } } },
	{ .name = "cap_rights_limit", .ret_type = 1, .nargs = 2,
	  .args = { { Int, 0 }, { CapRights, 1 } } },
	{ .name = "chdir", .ret_type = 1, .nargs = 1,
	  .args = { { Name, 0 } } },
	{ .name = "chflags", .ret_type = 1, .nargs = 2,
	  .args = { { Name | IN, 0 }, { FileFlags, 1 } } },
	{ .name = "chflagsat", .ret_type = 1, .nargs = 4,
	  .args = { { Atfd, 0 }, { Name | IN, 1 }, { FileFlags, 2 },
		    { Atflags, 3 } } },
	{ .name = "chmod", .ret_type = 1, .nargs = 2,
	  .args = { { Name, 0 }, { Octal, 1 } } },
	{ .name = "chown", .ret_type = 1, .nargs = 3,
	  .args = { { Name, 0 }, { Int, 1 }, { Int, 2 } } },
	{ .name = "chroot", .ret_type = 1, .nargs = 1,
	  .args = { { Name, 0 } } },
	{ .name = "clock_gettime", .ret_type = 1, .nargs = 2,
	  .args = { { Int, 0 }, { Timespec | OUT, 1 } } },
	{ .name = "close", .ret_type = 1, .nargs = 1,
	  .args = { { Int, 0 } } },
	{ .name = "closefrom", .ret_type = 1, .nargs = 1,
	  .args = { { Int, 0 } } },
	{ .name = "close_range", .ret_type = 1, .nargs = 3,
	  .args = { { Int, 0 }, { Int, 1 }, { Closerangeflags, 2 } } },
	{ .name = "compat11.fstat", .ret_type = 1, .nargs = 2,
	  .args = { { Int, 0 }, { Stat11 | OUT, 1 } } },
	{ .name = "compat11.fstatat", .ret_type = 1, .nargs = 4,
	  .args = { { Atfd, 0 }, { Name | IN, 1 }, { Stat11 | OUT, 2 },
		    { Atflags, 3 } } },
	{ .name = "compat11.kevent", .ret_type = 1, .nargs = 6,
	  .args = { { Int, 0 }, { Kevent11, 1 }, { Int, 2 },
		    { Kevent11 | OUT, 3 }, { Int, 4 }, { Timespec, 5 } } },
	{ .name = "compat11.lstat", .ret_type = 1, .nargs = 2,
	  .args = { { Name | IN, 0 }, { Stat11 | OUT, 1 } } },
	{ .name = "compat11.mknod", .ret_type = 1, .nargs = 3,
	  .args = { { Name, 0 }, { Octal, 1 }, { Int, 2 } } },
	{ .name = "compat11.mknodat", .ret_type = 1, .nargs = 4,
	  .args = { { Atfd, 0 }, { Name, 1 }, { Octal, 2 }, { Int, 3 } } },
	{ .name = "compat11.stat", .ret_type = 1, .nargs = 2,
	  .args = { { Name | IN, 0 }, { Stat11 | OUT, 1 } } },
	{ .name = "connect", .ret_type = 1, .nargs = 3,
	  .args = { { Int, 0 }, { Sockaddr | IN, 1 }, { Socklent, 2 } } },
	{ .name = "connectat", .ret_type = 1, .nargs = 4,
	  .args = { { Atfd, 0 }, { Int, 1 }, { Sockaddr | IN, 2 },
		    { Int, 3 } } },
	{ .name = "dup", .ret_type = 1, .nargs = 1,
	  .args = { { Int, 0 } } },
	{ .name = "dup2", .ret_type = 1, .nargs = 2,
	  .args = { { Int, 0 }, { Int, 1 } } },
	{ .name = "eaccess", .ret_type = 1, .nargs = 2,
	  .args = { { Name | IN, 0 }, { Accessmode, 1 } } },
	{ .name = "execve", .ret_type = 1, .nargs = 3,
	  .args = { { Name | IN, 0 }, { ExecArgs | IN, 1 },
		    { ExecEnv | IN, 2 } } },
	{ .name = "exit", .ret_type = 0, .nargs = 1,
	  .args = { { Hex, 0 } } },
	{ .name = "extattr_delete_fd", .ret_type = 1, .nargs = 3,
	  .args = { { Int, 0 }, { Extattrnamespace, 1 }, { Name, 2 } } },
	{ .name = "extattr_delete_file", .ret_type = 1, .nargs = 3,
	  .args = { { Name, 0 }, { Extattrnamespace, 1 }, { Name, 2 } } },
	{ .name = "extattr_delete_link", .ret_type = 1, .nargs = 3,
	  .args = { { Name, 0 }, { Extattrnamespace, 1 }, { Name, 2 } } },
	{ .name = "extattr_get_fd", .ret_type = 1, .nargs = 5,
	  .args = { { Int, 0 }, { Extattrnamespace, 1 }, { Name, 2 },
		    { BinString | OUT, 3 }, { Sizet, 4 } } },
	{ .name = "extattr_get_file", .ret_type = 1, .nargs = 5,
	  .args = { { Name, 0 }, { Extattrnamespace, 1 }, { Name, 2 },
		    { BinString | OUT, 3 }, { Sizet, 4 } } },
	{ .name = "extattr_get_link", .ret_type = 1, .nargs = 5,
	  .args = { { Name, 0 }, { Extattrnamespace, 1 }, { Name, 2 },
		    { BinString | OUT, 3 }, { Sizet, 4 } } },
	{ .name = "extattr_list_fd", .ret_type = 1, .nargs = 4,
	  .args = { { Int, 0 }, { Extattrnamespace, 1 }, { BinString | OUT, 2 },
		    { Sizet, 3 } } },
	{ .name = "extattr_list_file", .ret_type = 1, .nargs = 4,
	  .args = { { Name, 0 }, { Extattrnamespace, 1 }, { BinString | OUT, 2 },
		    { Sizet, 3 } } },
	{ .name = "extattr_list_link", .ret_type = 1, .nargs = 4,
	  .args = { { Name, 0 }, { Extattrnamespace, 1 }, { BinString | OUT, 2 },
		    { Sizet, 3 } } },
	{ .name = "extattr_set_fd", .ret_type = 1, .nargs = 5,
	  .args = { { Int, 0 }, { Extattrnamespace, 1 }, { Name, 2 },
		    { BinString | IN, 3 }, { Sizet, 4 } } },
	{ .name = "extattr_set_file", .ret_type = 1, .nargs = 5,
	  .args = { { Name, 0 }, { Extattrnamespace, 1 }, { Name, 2 },
		    { BinString | IN, 3 }, { Sizet, 4 } } },
	{ .name = "extattr_set_link", .ret_type = 1, .nargs = 5,
	  .args = { { Name, 0 }, { Extattrnamespace, 1 }, { Name, 2 },
		    { BinString | IN, 3 }, { Sizet, 4 } } },
	{ .name = "extattrctl", .ret_type = 1, .nargs = 5,
	  .args = { { Name, 0 }, { Hex, 1 }, { Name, 2 },
		    { Extattrnamespace, 3 }, { Name, 4 } } },
	{ .name = "faccessat", .ret_type = 1, .nargs = 4,
	  .args = { { Atfd, 0 }, { Name | IN, 1 }, { Accessmode, 2 },
		    { Atflags, 3 } } },
	{ .name = "fchflags", .ret_type = 1, .nargs = 2,
	  .args = { { Int, 0 }, { FileFlags, 1 } } },
	{ .name = "fchmod", .ret_type = 1, .nargs = 2,
	  .args = { { Int, 0 }, { Octal, 1 } } },
	{ .name = "fchmodat", .ret_type = 1, .nargs = 4,
	  .args = { { Atfd, 0 }, { Name, 1 }, { Octal, 2 }, { Atflags, 3 } } },
	{ .name = "fchown", .ret_type = 1, .nargs = 3,
	  .args = { { Int, 0 }, { Int, 1 }, { Int, 2 } } },
	{ .name = "fchownat", .ret_type = 1, .nargs = 5,
	  .args = { { Atfd, 0 }, { Name, 1 }, { Int, 2 }, { Int, 3 },
		    { Atflags, 4 } } },
	{ .name = "fcntl", .ret_type = 1, .nargs = 3,
	  .args = { { Int, 0 }, { Fcntl, 1 }, { Fcntlflag, 2 } } },
	{ .name = "fdatasync", .ret_type = 1, .nargs = 1,
	  .args = { { Int, 0 } } },
	{ .name = "flock", .ret_type = 1, .nargs = 2,
	  .args = { { Int, 0 }, { Flockop, 1 } } },
	{ .name = "fstat", .ret_type = 1, .nargs = 2,
	  .args = { { Int, 0 }, { Stat | OUT, 1 } } },
	{ .name = "fstatat", .ret_type = 1, .nargs = 4,
	  .args = { { Atfd, 0 }, { Name | IN, 1 }, { Stat | OUT, 2 },
		    { Atflags, 3 } } },
	{ .name = "fstatfs", .ret_type = 1, .nargs = 2,
	  .args = { { Int, 0 }, { StatFs | OUT, 1 } } },
	{ .name = "fsync", .ret_type = 1, .nargs = 1,
	  .args = { { Int, 0 } } },
	{ .name = "ftruncate", .ret_type = 1, .nargs = 2,
	  .args = { { Int | IN, 0 }, { QuadHex | IN, 1 } } },
	{ .name = "futimens", .ret_type = 1, .nargs = 2,
	  .args = { { Int, 0 }, { Timespec2 | IN, 1 } } },
	{ .name = "futimes", .ret_type = 1, .nargs = 2,
	  .args = { { Int, 0 }, { Timeval2 | IN, 1 } } },
	{ .name = "futimesat", .ret_type = 1, .nargs = 3,
	  .args = { { Atfd, 0 }, { Name | IN, 1 }, { Timeval2 | IN, 2 } } },
	{ .name = "getdirentries", .ret_type = 1, .nargs = 4,
	  .args = { { Int, 0 }, { BinString | OUT, 1 }, { Int, 2 },
		    { PQuadHex | OUT, 3 } } },
	{ .name = "getfsstat", .ret_type = 1, .nargs = 3,
	  .args = { { Ptr, 0 }, { Long, 1 }, { Getfsstatmode, 2 } } },
	{ .name = "getitimer", .ret_type = 1, .nargs = 2,
	  .args = { { Itimerwhich, 0 }, { Itimerval | OUT, 2 } } },
	{ .name = "getpeername", .ret_type = 1, .nargs = 3,
	  .args = { { Int, 0 }, { Sockaddr | OUT, 1 }, { Ptr | OUT, 2 } } },
	{ .name = "getpgid", .ret_type = 1, .nargs = 1,
	  .args = { { Int, 0 } } },
	{ .name = "getpriority", .ret_type = 1, .nargs = 2,
	  .args = { { Priowhich, 0 }, { Int, 1 } } },
	{ .name = "getrandom", .ret_type = 1, .nargs = 3,
	  .args = { { BinString | OUT, 0 }, { Sizet, 1 }, { UInt, 2 } } },
	{ .name = "getrlimit", .ret_type = 1, .nargs = 2,
	  .args = { { Resource, 0 }, { Rlimit | OUT, 1 } } },
	{ .name = "getrusage", .ret_type = 1, .nargs = 2,
	  .args = { { RusageWho, 0 }, { Rusage | OUT, 1 } } },
	{ .name = "getsid", .ret_type = 1, .nargs = 1,
	  .args = { { Int, 0 } } },
	{ .name = "getsockname", .ret_type = 1, .nargs = 3,
	  .args = { { Int, 0 }, { Sockaddr | OUT, 1 }, { Ptr | OUT, 2 } } },
	{ .name = "getsockopt", .ret_type = 1, .nargs = 5,
	  .args = { { Int, 0 }, { Sockoptlevel, 1 }, { Sockoptname, 2 },
		    { Ptr | OUT, 3 }, { Ptr | OUT, 4 } } },
	{ .name = "gettimeofday", .ret_type = 1, .nargs = 2,
	  .args = { { Timeval | OUT, 0 }, { Ptr, 1 } } },
	{ .name = "inotify_add_watch_at", .ret_type = 1, .nargs = 4,
	  .args = { { Int, 0 }, { Atfd, 1 }, { Name | IN, 2 },
	            { Inotifyflags, 3 } } },
	{ .name = "ioctl", .ret_type = 1, .nargs = 3,
	  .args = { { Int, 0 }, { Ioctl, 1 }, { Ptr, 2 } } },
	{ .name = "kevent", .ret_type = 1, .nargs = 6,
	  .args = { { Int, 0 }, { Kevent, 1 }, { Int, 2 }, { Kevent | OUT, 3 },
		    { Int, 4 }, { Timespec, 5 } } },
	{ .name = "kill", .ret_type = 1, .nargs = 2,
	  .args = { { Int | IN, 0 }, { Signal | IN, 1 } } },
	{ .name = "kldfind", .ret_type = 1, .nargs = 1,
	  .args = { { Name | IN, 0 } } },
	{ .name = "kldfirstmod", .ret_type = 1, .nargs = 1,
	  .args = { { Int, 0 } } },
	{ .name = "kldload", .ret_type = 1, .nargs = 1,
	  .args = { { Name | IN, 0 } } },
	{ .name = "kldnext", .ret_type = 1, .nargs = 1,
	  .args = { { Int, 0 } } },
	{ .name = "kldstat", .ret_type = 1, .nargs = 2,
	  .args = { { Int, 0 }, { Ptr, 1 } } },
	{ .name = "kldsym", .ret_type = 1, .nargs = 3,
	  .args = { { Int, 0 }, { Kldsymcmd, 1 }, { Ptr, 2 } } },
	{ .name = "kldunload", .ret_type = 1, .nargs = 1,
	  .args = { { Int, 0 } } },
	{ .name = "kldunloadf", .ret_type = 1, .nargs = 2,
	  .args = { { Int, 0 }, { Kldunloadflags, 1 } } },
	{ .name = "kse_release", .ret_type = 0, .nargs = 1,
	  .args = { { Timespec, 0 } } },
	{ .name = "lchflags", .ret_type = 1, .nargs = 2,
	  .args = { { Name | IN, 0 }, { FileFlags, 1 } } },
	{ .name = "lchmod", .ret_type = 1, .nargs = 2,
	  .args = { { Name, 0 }, { Octal, 1 } } },
	{ .name = "lchown", .ret_type = 1, .nargs = 3,
	  .args = { { Name, 0 }, { Int, 1 }, { Int, 2 } } },
	{ .name = "link", .ret_type = 1, .nargs = 2,
	  .args = { { Name, 0 }, { Name, 1 } } },
	{ .name = "linkat", .ret_type = 1, .nargs = 5,
	  .args = { { Atfd, 0 }, { Name, 1 }, { Atfd, 2 }, { Name, 3 },
		    { Atflags, 4 } } },
	{ .name = "lio_listio", .ret_type = 1, .nargs = 4,
	  .args = { { LioMode, 0 }, { AiocbArray, 1 }, { Int, 2 },
		    { Sigevent, 3 } } },
	{ .name = "listen", .ret_type = 1, .nargs = 2,
	  .args = { { Int, 0 }, { Int, 1 } } },
 	{ .name = "lseek", .ret_type = 2, .nargs = 3,
	  .args = { { Int, 0 }, { QuadHex, 1 }, { Whence, 2 } } },
	{ .name = "lstat", .ret_type = 1, .nargs = 2,
	  .args = { { Name | IN, 0 }, { Stat | OUT, 1 } } },
	{ .name = "lutimes", .ret_type = 1, .nargs = 2,
	  .args = { { Name | IN, 0 }, { Timeval2 | IN, 1 } } },
	{ .name = "madvise", .ret_type = 1, .nargs = 3,
	  .args = { { Ptr, 0 }, { Sizet, 1 }, { Madvice, 2 } } },
	{ .name = "minherit", .ret_type = 1, .nargs = 3,
	  .args = { { Ptr, 0 }, { Sizet, 1 }, { Minherit, 2 } } },
	{ .name = "mkdir", .ret_type = 1, .nargs = 2,
	  .args = { { Name, 0 }, { Octal, 1 } } },
	{ .name = "mkdirat", .ret_type = 1, .nargs = 3,
	  .args = { { Atfd, 0 }, { Name, 1 }, { Octal, 2 } } },
	{ .name = "mkfifo", .ret_type = 1, .nargs = 2,
	  .args = { { Name, 0 }, { Octal, 1 } } },
	{ .name = "mkfifoat", .ret_type = 1, .nargs = 3,
	  .args = { { Atfd, 0 }, { Name, 1 }, { Octal, 2 } } },
	{ .name = "mknod", .ret_type = 1, .nargs = 3,
	  .args = { { Name, 0 }, { Octal, 1 }, { Quad, 2 } } },
	{ .name = "mknodat", .ret_type = 1, .nargs = 4,
	  .args = { { Atfd, 0 }, { Name, 1 }, { Octal, 2 }, { Quad, 3 } } },
	{ .name = "mlock", .ret_type = 1, .nargs = 2,
	  .args = { { Ptr, 0 }, { Sizet, 1 } } },
	{ .name = "mlockall", .ret_type = 1, .nargs = 1,
	  .args = { { Mlockall, 0 } } },
	{ .name = "mmap", .ret_type = 1, .nargs = 6,
	  .args = { { Ptr, 0 }, { Sizet, 1 }, { Mprot, 2 }, { Mmapflags, 3 },
		    { Int, 4 }, { QuadHex, 5 } } },
	{ .name = "modfind", .ret_type = 1, .nargs = 1,
	  .args = { { Name | IN, 0 } } },
	{ .name = "mount", .ret_type = 1, .nargs = 4,
	  .args = { { Name, 0 }, { Name, 1 }, { Mountflags, 2 }, { Ptr, 3 } } },
	{ .name = "mprotect", .ret_type = 1, .nargs = 3,
	  .args = { { Ptr, 0 }, { Sizet, 1 }, { Mprot, 2 } } },
	{ .name = "msync", .ret_type = 1, .nargs = 3,
	  .args = { { Ptr, 0 }, { Sizet, 1 }, { Msync, 2 } } },
	{ .name = "munlock", .ret_type = 1, .nargs = 2,
	  .args = { { Ptr, 0 }, { Sizet, 1 } } },
	{ .name = "munmap", .ret_type = 1, .nargs = 2,
	  .args = { { Ptr, 0 }, { Sizet, 1 } } },
	{ .name = "nanosleep", .ret_type = 1, .nargs = 1,
	  .args = { { Timespec, 0 } } },
	{ .name = "nmount", .ret_type = 1, .nargs = 3,
	  .args = { { Ptr, 0 }, { UInt, 1 }, { Mountflags, 2 } } },
	{ .name = "open", .ret_type = 1, .nargs = 3,
	  .args = { { Name | IN, 0 }, { Open, 1 }, { Octal, 2 } } },
	{ .name = "openat", .ret_type = 1, .nargs = 4,
	  .args = { { Atfd, 0 }, { Name | IN, 1 }, { Open, 2 },
		    { Octal, 3 } } },
	{ .name = "pathconf", .ret_type = 1, .nargs = 2,
	  .args = { { Name | IN, 0 }, { Pathconf, 1 } } },
	{ .name = "pipe", .ret_type = 1, .nargs = 1,
	  .args = { { PipeFds | OUT, 0 } } },
	{ .name = "pipe2", .ret_type = 1, .nargs = 2,
	  .args = { { Ptr, 0 }, { Pipe2, 1 } } },
	{ .name = "poll", .ret_type = 1, .nargs = 3,
	  .args = { { Pollfd, 0 }, { Int, 1 }, { Int, 2 } } },
	{ .name = "posix_fadvise", .ret_type = 1, .nargs = 4,
	  .args = { { Int, 0 }, { QuadHex, 1 }, { QuadHex, 2 },
		    { Fadvice, 3 } } },
	{ .name = "posix_openpt", .ret_type = 1, .nargs = 1,
	  .args = { { Open, 0 } } },
	{ .name = "ppoll", .ret_type = 1, .nargs = 4,
	  .args = { { Pollfd, 0 }, { Int, 1 }, { Timespec | IN, 2 },
 		    { Sigset | IN, 3 } } },
	{ .name = "pread", .ret_type = 1, .nargs = 4,
	  .args = { { Int, 0 }, { BinString | OUT, 1 }, { Sizet, 2 },
		    { QuadHex, 3 } } },
	{ .name = "preadv", .ret_type = 1, .nargs = 4,
	  .args = { { Int, 0 }, { Iovec | OUT, 1 }, { Int, 2 },
		    { QuadHex, 3 } } },
	{ .name = "procctl", .ret_type = 1, .nargs = 4,
	  .args = { { Idtype, 0 }, { Quad, 1 }, { Procctl, 2 }, { Ptr, 3 } } },
	{ .name = "ptrace", .ret_type = 1, .nargs = 4,
	  .args = { { Ptraceop, 0 }, { Int, 1 }, { Ptr, 2 }, { Int, 3 } } },
	{ .name = "pwrite", .ret_type = 1, .nargs = 4,
	  .args = { { Int, 0 }, { BinString | IN, 1 }, { Sizet, 2 },
		    { QuadHex, 3 } } },
	{ .name = "pwritev", .ret_type = 1, .nargs = 4,
	  .args = { { Int, 0 }, { Iovec | IN, 1 }, { Int, 2 },
		    { QuadHex, 3 } } },
	{ .name = "quotactl", .ret_type = 1, .nargs = 4,
	  .args = { { Name, 0 }, { Quotactlcmd, 1 }, { Int, 2 }, { Ptr, 3 } } },
	{ .name = "read", .ret_type = 1, .nargs = 3,
	  .args = { { Int, 0 }, { BinString | OUT, 1 }, { Sizet, 2 } } },
	{ .name = "readlink", .ret_type = 1, .nargs = 3,
	  .args = { { Name, 0 }, { Readlinkres | OUT, 1 }, { Sizet, 2 } } },
	{ .name = "readlinkat", .ret_type = 1, .nargs = 4,
	  .args = { { Atfd, 0 }, { Name, 1 }, { Readlinkres | OUT, 2 },
		    { Sizet, 3 } } },
	{ .name = "readv", .ret_type = 1, .nargs = 3,
	  .args = { { Int, 0 }, { Iovec | OUT, 1 }, { Int, 2 } } },
	{ .name = "reboot", .ret_type = 1, .nargs = 1,
	  .args = { { Reboothowto, 0 } } },
	{ .name = "recvfrom", .ret_type = 1, .nargs = 6,
	  .args = { { Int, 0 }, { BinString | OUT, 1 }, { Sizet, 2 },
	            { Msgflags, 3 }, { Sockaddr | OUT, 4 },
	            { Ptr | OUT, 5 } } },
	{ .name = "recvmsg", .ret_type = 1, .nargs = 3,
	  .args = { { Int, 0 }, { Msghdr | OUT, 1 }, { Msgflags, 2 } } },
	{ .name = "rename", .ret_type = 1, .nargs = 2,
	  .args = { { Name, 0 }, { Name, 1 } } },
	{ .name = "renameat", .ret_type = 1, .nargs = 4,
	  .args = { { Atfd, 0 }, { Name, 1 }, { Atfd, 2 }, { Name, 3 } } },
	{ .name = "rfork", .ret_type = 1, .nargs = 1,
	  .args = { { Rforkflags, 0 } } },
	{ .name = "rmdir", .ret_type = 1, .nargs = 1,
	  .args = { { Name, 0 } } },
	{ .name = "rtprio", .ret_type = 1, .nargs = 3,
	  .args = { { Rtpriofunc, 0 }, { Int, 1 }, { Ptr, 2 } } },
	{ .name = "rtprio_thread", .ret_type = 1, .nargs = 3,
	  .args = { { Rtpriofunc, 0 }, { Int, 1 }, { Ptr, 2 } } },
	{ .name = "sched_get_priority_max", .ret_type = 1, .nargs = 1,
	  .args = { { Schedpolicy, 0 } } },
	{ .name = "sched_get_priority_min", .ret_type = 1, .nargs = 1,
	  .args = { { Schedpolicy, 0 } } },
	{ .name = "sched_getparam", .ret_type = 1, .nargs = 2,
	  .args = { { Int, 0 }, { Schedparam | OUT, 1 } } },
	{ .name = "sched_getscheduler", .ret_type = 1, .nargs = 1,
	  .args = { { Int, 0 } } },
	{ .name = "sched_rr_get_interval", .ret_type = 1, .nargs = 2,
	  .args = { { Int, 0 }, { Timespec | OUT, 1 } } },
	{ .name = "sched_setparam", .ret_type = 1, .nargs = 2,
	  .args = { { Int, 0 }, { Schedparam, 1 } } },
	{ .name = "sched_setscheduler", .ret_type = 1, .nargs = 3,
	  .args = { { Int, 0 }, { Schedpolicy, 1 }, { Schedparam, 2 } } },
	{ .name = "sctp_generic_recvmsg", .ret_type = 1, .nargs = 7,
	  .args = { { Int, 0 }, { Iovec | OUT, 1 }, { Int, 2 },
	            { Sockaddr | OUT, 3 }, { Ptr | OUT, 4 },
	            { Sctpsndrcvinfo | OUT, 5 }, { Ptr | OUT, 6 } } },
	{ .name = "sctp_generic_sendmsg", .ret_type = 1, .nargs = 7,
	  .args = { { Int, 0 }, { BinString | IN, 1 }, { Int, 2 },
	            { Sockaddr | IN, 3 }, { Socklent, 4 },
	            { Sctpsndrcvinfo | IN, 5 }, { Msgflags, 6 } } },
	{ .name = "sctp_generic_sendmsg_iov", .ret_type = 1, .nargs = 7,
	  .args = { { Int, 0 }, { Iovec | IN, 1 }, { Int, 2 },
	            { Sockaddr | IN, 3 }, { Socklent, 4 },
	            { Sctpsndrcvinfo | IN, 5 }, { Msgflags, 6 } } },
	{ .name = "sendfile", .ret_type = 1, .nargs = 7,
	  .args = { { Int, 0 }, { Int, 1 }, { QuadHex, 2 }, { Sizet, 3 },
		    { Sendfilehdtr, 4 }, { QuadHex | OUT, 5 },
		    { Sendfileflags, 6 } } },
	{ .name = "select", .ret_type = 1, .nargs = 5,
	  .args = { { Int, 0 }, { Fd_set, 1 }, { Fd_set, 2 }, { Fd_set, 3 },
		    { Timeval, 4 } } },
	{ .name = "sendmsg", .ret_type = 1, .nargs = 3,
	  .args = { { Int, 0 }, { Msghdr | IN, 1 }, { Msgflags, 2 } } },
	{ .name = "sendto", .ret_type = 1, .nargs = 6,
	  .args = { { Int, 0 }, { BinString | IN, 1 }, { Sizet, 2 },
	            { Msgflags, 3 }, { Sockaddr | IN, 4 },
	            { Socklent | IN, 5 } } },
	{ .name = "setitimer", .ret_type = 1, .nargs = 3,
	  .args = { { Itimerwhich, 0 }, { Itimerval, 1 },
		    { Itimerval | OUT, 2 } } },
	{ .name = "setpriority", .ret_type = 1, .nargs = 3,
	  .args = { { Priowhich, 0 }, { Int, 1 }, { Int, 2 } } },
	{ .name = "setrlimit", .ret_type = 1, .nargs = 2,
	  .args = { { Resource, 0 }, { Rlimit | IN, 1 } } },
	{ .name = "setsockopt", .ret_type = 1, .nargs = 5,
	  .args = { { Int, 0 }, { Sockoptlevel, 1 }, { Sockoptname, 2 },
		    { Ptr | IN, 3 }, { Socklent, 4 } } },
	{ .name = "shm_open", .ret_type = 1, .nargs = 3,
	  .args = { { ShmName | IN, 0 }, { Open, 1 }, { Octal, 2 } } },
	{ .name = "shm_open2", .ret_type = 1, .nargs = 5,
	  .args = { { ShmName | IN, 0 }, { Open, 1 }, { Octal, 2 },
		    { ShmFlags, 3 }, { Name | IN, 4 } } },
	{ .name = "shm_rename", .ret_type = 1, .nargs = 3,
	  .args = { { Name | IN, 0 }, { Name | IN, 1 }, { Hex, 2 } } },
	{ .name = "shm_unlink", .ret_type = 1, .nargs = 1,
	  .args = { { Name | IN, 0 } } },
	{ .name = "shutdown", .ret_type = 1, .nargs = 2,
	  .args = { { Int, 0 }, { Shutdown, 1 } } },
	{ .name = "sigaction", .ret_type = 1, .nargs = 3,
	  .args = { { Signal, 0 }, { Sigaction | IN, 1 },
		    { Sigaction | OUT, 2 } } },
	{ .name = "sigpending", .ret_type = 1, .nargs = 1,
	  .args = { { Sigset | OUT, 0 } } },
	{ .name = "sigprocmask", .ret_type = 1, .nargs = 3,
	  .args = { { Sigprocmask, 0 }, { Sigset, 1 }, { Sigset | OUT, 2 } } },
	{ .name = "sigqueue", .ret_type = 1, .nargs = 3,
	  .args = { { Int, 0 }, { Signal, 1 }, { LongHex, 2 } } },
	{ .name = "sigreturn", .ret_type = 1, .nargs = 1,
	  .args = { { Ptr, 0 } } },
	{ .name = "sigsuspend", .ret_type = 1, .nargs = 1,
	  .args = { { Sigset | IN, 0 } } },
	{ .name = "sigtimedwait", .ret_type = 1, .nargs = 3,
	  .args = { { Sigset | IN, 0 }, { Siginfo | OUT, 1 },
		    { Timespec | IN, 2 } } },
	{ .name = "sigwait", .ret_type = 1, .nargs = 2,
	  .args = { { Sigset | IN, 0 }, { PSig | OUT, 1 } } },
	{ .name = "sigwaitinfo", .ret_type = 1, .nargs = 2,
	  .args = { { Sigset | IN, 0 }, { Siginfo | OUT, 1 } } },
	{ .name = "socket", .ret_type = 1, .nargs = 3,
	  .args = { { Sockdomain, 0 }, { Socktype, 1 }, { Sockprotocol, 2 } } },
	{ .name = "stat", .ret_type = 1, .nargs = 2,
	  .args = { { Name | IN, 0 }, { Stat | OUT, 1 } } },
	{ .name = "statfs", .ret_type = 1, .nargs = 2,
	  .args = { { Name | IN, 0 }, { StatFs | OUT, 1 } } },
	{ .name = "symlink", .ret_type = 1, .nargs = 2,
	  .args = { { Name, 0 }, { Name, 1 } } },
	{ .name = "symlinkat", .ret_type = 1, .nargs = 3,
	  .args = { { Name, 0 }, { Atfd, 1 }, { Name, 2 } } },
	{ .name = "sysarch", .ret_type = 1, .nargs = 2,
	  .args = { { Sysarch, 0 }, { Ptr, 1 } } },
	{ .name = "__sysctl", .ret_type = 1, .nargs = 6,
	  .args = { { Sysctl, 0 }, { Sizet, 1 }, { Ptr, 2 }, { Ptr, 3 },
	            { Ptr, 4 }, { Sizet, 5 } } },
	{ .name = "__sysctlbyname", .ret_type = 1, .nargs = 6,
	  .args = { { Name, 0 }, { Sizet, 1 }, { Ptr, 2 }, { Ptr, 3 },
	            { Ptr, 4}, { Sizet, 5 } } },
	{ .name = "thr_kill", .ret_type = 1, .nargs = 2,
	  .args = { { Long, 0 }, { Signal, 1 } } },
	{ .name = "thr_self", .ret_type = 1, .nargs = 1,
	  .args = { { Ptr, 0 } } },
	{ .name = "thr_set_name", .ret_type = 1, .nargs = 2,
	  .args = { { Long, 0 }, { Name, 1 } } },
	{ .name = "truncate", .ret_type = 1, .nargs = 2,
	  .args = { { Name | IN, 0 }, { QuadHex | IN, 1 } } },
	{ .name = "unlink", .ret_type = 1, .nargs = 1,
	  .args = { { Name, 0 } } },
	{ .name = "unlinkat", .ret_type = 1, .nargs = 3,
	  .args = { { Atfd, 0 }, { Name, 1 }, { Atflags, 2 } } },
	{ .name = "unmount", .ret_type = 1, .nargs = 2,
	  .args = { { Name, 0 }, { Mountflags, 1 } } },
	{ .name = "utimensat", .ret_type = 1, .nargs = 4,
	  .args = { { Atfd, 0 }, { Name | IN, 1 }, { Timespec2 | IN, 2 },
		    { Atflags, 3 } } },
	{ .name = "utimes", .ret_type = 1, .nargs = 2,
	  .args = { { Name | IN, 0 }, { Timeval2 | IN, 1 } } },
	{ .name = "utrace", .ret_type = 1, .nargs = 1,
	  .args = { { Utrace, 0 } } },
	{ .name = "wait4", .ret_type = 1, .nargs = 4,
	  .args = { { Int, 0 }, { ExitStatus | OUT, 1 }, { Waitoptions, 2 },
		    { Rusage | OUT, 3 } } },
	{ .name = "wait6", .ret_type = 1, .nargs = 6,
	  .args = { { Idtype, 0 }, { Quad, 1 }, { ExitStatus | OUT, 2 },
		    { Waitoptions, 3 }, { Rusage | OUT, 4 },
		    { Siginfo | OUT, 5 } } },
	{ .name = "write", .ret_type = 1, .nargs = 3,
	  .args = { { Int, 0 }, { BinString | IN, 1 }, { Sizet, 2 } } },
	{ .name = "writev", .ret_type = 1, .nargs = 3,
	  .args = { { Int, 0 }, { Iovec | IN, 1 }, { Int, 2 } } },

	/* Linux ABI */
	{ .name = "linux_access", .ret_type = 1, .nargs = 2,
	  .args = { { Name, 0 }, { Accessmode, 1 } } },
	{ .name = "linux_execve", .ret_type = 1, .nargs = 3,
	  .args = { { Name | IN, 0 }, { ExecArgs | IN, 1 },
		    { ExecEnv | IN, 2 } } },
	{ .name = "linux_getitimer", .ret_type = 1, .nargs = 2,
	  .args = { { Itimerwhich, 0 }, { Itimerval | OUT, 2 } } },
	{ .name = "linux_lseek", .ret_type = 2, .nargs = 3,
	  .args = { { Int, 0 }, { Int, 1 }, { Whence, 2 } } },
	{ .name = "linux_mkdir", .ret_type = 1, .nargs = 2,
	  .args = { { Name | IN, 0 }, { Int, 1 } } },
	{ .name = "linux_newfstat", .ret_type = 1, .nargs = 2,
	  .args = { { Int, 0 }, { Ptr | OUT, 1 } } },
	{ .name = "linux_newlstat", .ret_type = 1, .nargs = 2,
	  .args = { { Name | IN, 0 }, { Ptr | OUT, 1 } } },
	{ .name = "linux_newstat", .ret_type = 1, .nargs = 2,
	  .args = { { Name | IN, 0 }, { Ptr | OUT, 1 } } },
	{ .name = "linux_open", .ret_type = 1, .nargs = 3,
	  .args = { { Name, 0 }, { Hex, 1 }, { Octal, 2 } } },
	{ .name = "linux_readlink", .ret_type = 1, .nargs = 3,
	  .args = { { Name, 0 }, { Name | OUT, 1 }, { Sizet, 2 } } },
	{ .name = "linux_setitimer", .ret_type = 1, .nargs = 3,
	  .args = { { Itimerwhich, 0 }, { Itimerval, 1 },
		    { Itimerval | OUT, 2 } } },
	{ .name = "linux_socketcall", .ret_type = 1, .nargs = 2,
	  .args = { { Int, 0 }, { LinuxSockArgs, 1 } } },
	{ .name = "linux_stat64", .ret_type = 1, .nargs = 2,
	  .args = { { Name | IN, 0 }, { Ptr | OUT, 1 } } },
};
static STAILQ_HEAD(, syscall) seen_syscalls;

/* Xlat idea taken from strace */
struct xlat {
	int val;
	const char *str;
};

#define	X(a)	{ a, #a },
#define	XEND	{ 0, NULL }

static struct xlat poll_flags[] = {
	X(POLLSTANDARD) X(POLLIN) X(POLLPRI) X(POLLOUT) X(POLLERR)
	X(POLLHUP) X(POLLNVAL) X(POLLRDNORM) X(POLLRDBAND)
	X(POLLWRBAND) X(POLLINIGNEOF) X(POLLRDHUP) XEND
};

static struct xlat sigaction_flags[] = {
	X(SA_ONSTACK) X(SA_RESTART) X(SA_RESETHAND) X(SA_NOCLDSTOP)
	X(SA_NODEFER) X(SA_NOCLDWAIT) X(SA_SIGINFO) XEND
};

static struct xlat linux_socketcall_ops[] = {
	X(LINUX_SOCKET) X(LINUX_BIND) X(LINUX_CONNECT) X(LINUX_LISTEN)
	X(LINUX_ACCEPT) X(LINUX_GETSOCKNAME) X(LINUX_GETPEERNAME)
	X(LINUX_SOCKETPAIR) X(LINUX_SEND) X(LINUX_RECV) X(LINUX_SENDTO)
	X(LINUX_RECVFROM) X(LINUX_SHUTDOWN) X(LINUX_SETSOCKOPT)
	X(LINUX_GETSOCKOPT) X(LINUX_SENDMSG) X(LINUX_RECVMSG)
	XEND
};

static struct xlat lio_modes[] = {
	X(LIO_WAIT) X(LIO_NOWAIT)
	XEND
};

static struct xlat lio_opcodes[] = {
	X(LIO_WRITE) X(LIO_READ) X(LIO_READV) X(LIO_WRITEV) X(LIO_NOP)
	XEND
};

static struct xlat aio_fsync_ops[] = {
	X(O_SYNC)
	XEND
};

#undef X
#undef XEND

/*
 * Searches an xlat array for a value, and returns it if found.  Otherwise
 * return a string representation.
 */
static const char *
lookup(struct xlat *xlat, int val, int base)
{
	static char tmp[16];

	for (; xlat->str != NULL; xlat++)
		if (xlat->val == val)
			return (xlat->str);
	switch (base) {
	case 8:
		sprintf(tmp, "0%o", val);
		break;
	case 16:
		sprintf(tmp, "0x%x", val);
		break;
	case 10:
		sprintf(tmp, "%u", val);
		break;
	default:
		errx(1, "Unknown lookup base");
	}
	return (tmp);
}

static const char *
xlookup(struct xlat *xlat, int val)
{

	return (lookup(xlat, val, 16));
}

/*
 * Searches an xlat array containing bitfield values.  Remaining bits
 * set after removing the known ones are printed at the end:
 * IN|0x400.
 */
static char *
xlookup_bits(struct xlat *xlat, int val)
{
	int len, rem;
	static char str[512];

	len = 0;
	rem = val;
	for (; xlat->str != NULL; xlat++) {
		if ((xlat->val & rem) == xlat->val) {
			/*
			 * Don't print the "all-bits-zero" string unless all
			 * bits are really zero.
			 */
			if (xlat->val == 0 && val != 0)
				continue;
			len += sprintf(str + len, "%s|", xlat->str);
			rem &= ~(xlat->val);
		}
	}

	/*
	 * If we have leftover bits or didn't match anything, print
	 * the remainder.
	 */
	if (rem || len == 0)
		len += sprintf(str + len, "0x%x", rem);
	if (len && str[len - 1] == '|')
		len--;
	str[len] = 0;
	return (str);
}

static void
print_integer_arg(const char *(*decoder)(int), FILE *fp, int value)
{
	const char *str;

	str = decoder(value);
	if (str != NULL)
		fputs(str, fp);
	else
		fprintf(fp, "%d", value);
}

static bool
print_mask_arg_part(bool (*decoder)(FILE *, int, int *), FILE *fp, int value,
    int *rem)
{

	return (decoder(fp, value, rem));
}

static void
print_mask_arg(bool (*decoder)(FILE *, int, int *), FILE *fp, int value)
{
	int rem;

	if (!print_mask_arg_part(decoder, fp, value, &rem))
		fprintf(fp, "0x%x", rem);
	else if (rem != 0)
		fprintf(fp, "|0x%x", rem);
}

static void
print_mask_arg32(bool (*decoder)(FILE *, uint32_t, uint32_t *), FILE *fp,
    uint32_t value)
{
	uint32_t rem;

	if (!decoder(fp, value, &rem))
		fprintf(fp, "0x%x", rem);
	else if (rem != 0)
		fprintf(fp, "|0x%x", rem);
}

/*
 * Add argument padding to subsequent system calls after Quad
 * syscall arguments as needed.  This used to be done by hand in the
 * decoded_syscalls table which was ugly and error prone.  It is
 * simpler to do the fixup of offsets at initialization time than when
 * decoding arguments.
 */
static void
quad_fixup(struct syscall_decode *sc)
{
	int offset, prev;
	u_int i;

	offset = 0;
	prev = -1;
	for (i = 0; i < sc->nargs; i++) {
		/* This arg type is a dummy that doesn't use offset. */
		if ((sc->args[i].type & ARG_MASK) == PipeFds)
			continue;

		assert(prev < sc->args[i].offset);
		prev = sc->args[i].offset;
		sc->args[i].offset += offset;
		switch (sc->args[i].type & ARG_MASK) {
		case Quad:
		case QuadHex:
#if defined(__powerpc__) || defined(__arm__) || defined(__aarch64__)
			/*
			 * 64-bit arguments on 32-bit powerpc and arm must be
			 * 64-bit aligned.  If the current offset is
			 * not aligned, the calling convention inserts
			 * a 32-bit pad argument that should be skipped.
			 */
			if (sc->args[i].offset % 2 == 1) {
				sc->args[i].offset++;
				offset++;
			}
#endif
			offset++;
		default:
			break;
		}
	}
}

static struct syscall *
find_syscall(struct procabi *abi, u_int number)
{
	struct extra_syscall *es;

	if (number < nitems(abi->syscalls))
		return (abi->syscalls[number]);
	STAILQ_FOREACH(es, &abi->extra_syscalls, entries) {
		if (es->number == number)
			return (es->sc);
	}
	return (NULL);
}

static void
add_syscall(struct procabi *abi, u_int number, struct syscall *sc)
{
	struct extra_syscall *es;

	/*
	 * quad_fixup() is currently needed for all 32-bit ABIs.
	 * TODO: This should probably be a function pointer inside struct
	 *  procabi instead.
	 */
	if (abi->pointer_size == 4)
		quad_fixup(&sc->decode);

	if (number < nitems(abi->syscalls)) {
		assert(abi->syscalls[number] == NULL);
		abi->syscalls[number] = sc;
	} else {
		es = malloc(sizeof(*es));
		es->sc = sc;
		es->number = number;
		STAILQ_INSERT_TAIL(&abi->extra_syscalls, es, entries);
	}

	STAILQ_INSERT_HEAD(&seen_syscalls, sc, entries);
}

/*
 * If/when the list gets big, it might be desirable to do it
 * as a hash table or binary search.
 */
struct syscall *
get_syscall(struct threadinfo *t, u_int number, u_int nargs)
{
	struct syscall *sc;
	struct procabi *procabi;
	const char *sysdecode_name;
	const char *lookup_name;
	const char *name;
	u_int i;

	procabi = t->proc->abi;
	sc = find_syscall(procabi, number);
	if (sc != NULL)
		return (sc);

	/* Memory is not explicitly deallocated, it's released on exit(). */
	sysdecode_name = sysdecode_syscallname(procabi->abi, number);
	if (sysdecode_name == NULL)
		asprintf(__DECONST(char **, &name), "#%d", number);
	else
		name = sysdecode_name;

	sc = calloc(1, sizeof(*sc));
	sc->name = name;

	/* Also decode compat syscalls arguments by stripping the prefix. */
	lookup_name = name;
	if (procabi->compat_prefix != NULL && strncmp(procabi->compat_prefix,
	    name, strlen(procabi->compat_prefix)) == 0)
		lookup_name += strlen(procabi->compat_prefix);

	for (i = 0; i < nitems(decoded_syscalls); i++) {
		if (strcmp(lookup_name, decoded_syscalls[i].name) == 0) {
			sc->decode = decoded_syscalls[i];
			add_syscall(t->proc->abi, number, sc);
			return (sc);
		}
	}

	/* It is unknown.  Add it into the list. */
#if DEBUG
	fprintf(stderr, "unknown syscall %s -- setting args to %d\n", name,
	    nargs);
#endif
	sc->unknown = sysdecode_name == NULL;
	sc->decode.ret_type = 1; /* Assume 1 return value. */
	sc->decode.nargs = nargs;
	for (i = 0; i < nargs; i++) {
		sc->decode.args[i].offset = i;
		/* Treat all unknown arguments as LongHex. */
		sc->decode.args[i].type = LongHex;
	}
	add_syscall(t->proc->abi, number, sc);
	return (sc);
}

/*
 * Copy a fixed amount of bytes from the process.
 */
static int
get_struct(pid_t pid, psaddr_t offset, void *buf, size_t len)
{
	struct ptrace_io_desc iorequest;

	iorequest.piod_op = PIOD_READ_D;
	iorequest.piod_offs = (void *)(uintptr_t)offset;
	iorequest.piod_addr = buf;
	iorequest.piod_len = len;
	if (ptrace(PT_IO, pid, (caddr_t)&iorequest, 0) < 0)
		return (-1);
	return (0);
}

#define	MAXSIZE		4096

/*
 * Copy a string from the process.  Note that it is
 * expected to be a C string, but if max is set, it will
 * only get that much.
 */
static char *
get_string(pid_t pid, psaddr_t addr, int max)
{
	struct ptrace_io_desc iorequest;
	char *buf, *nbuf;
	size_t offset, size, totalsize;

	offset = 0;
	if (max)
		size = max + 1;
	else {
		/* Read up to the end of the current page. */
		size = PAGE_SIZE - (addr % PAGE_SIZE);
		if (size > MAXSIZE)
			size = MAXSIZE;
	}
	totalsize = size;
	buf = malloc(totalsize);
	if (buf == NULL)
		return (NULL);
	for (;;) {
		iorequest.piod_op = PIOD_READ_D;
		iorequest.piod_offs = (void *)((uintptr_t)addr + offset);
		iorequest.piod_addr = buf + offset;
		iorequest.piod_len = size;
		if (ptrace(PT_IO, pid, (caddr_t)&iorequest, 0) < 0) {
			free(buf);
			return (NULL);
		}
		if (memchr(buf + offset, '\0', size) != NULL)
			return (buf);
		offset += size;
		if (totalsize < MAXSIZE && max == 0) {
			size = MAXSIZE - totalsize;
			if (size > PAGE_SIZE)
				size = PAGE_SIZE;
			nbuf = realloc(buf, totalsize + size);
			if (nbuf == NULL) {
				buf[totalsize - 1] = '\0';
				return (buf);
			}
			buf = nbuf;
			totalsize += size;
		} else {
			buf[totalsize - 1] = '\0';
			return (buf);
		}
	}
}

static const char *
strsig2(int sig)
{
	static char tmp[32];
	const char *signame;

	signame = sysdecode_signal(sig);
	if (signame == NULL) {
		snprintf(tmp, sizeof(tmp), "%d", sig);
		signame = tmp;
	}
	return (signame);
}

static void
print_kevent(FILE *fp, struct kevent *ke)
{

	switch (ke->filter) {
	case EVFILT_READ:
	case EVFILT_WRITE:
	case EVFILT_VNODE:
	case EVFILT_PROC:
	case EVFILT_TIMER:
	case EVFILT_PROCDESC:
	case EVFILT_EMPTY:
		fprintf(fp, "%ju", (uintmax_t)ke->ident);
		break;
	case EVFILT_SIGNAL:
		fputs(strsig2(ke->ident), fp);
		break;
	default:
		fprintf(fp, "%p", (void *)ke->ident);
	}
	fprintf(fp, ",");
	print_integer_arg(sysdecode_kevent_filter, fp, ke->filter);
	fprintf(fp, ",");
	print_mask_arg(sysdecode_kevent_flags, fp, ke->flags);
	fprintf(fp, ",");
	sysdecode_kevent_fflags(fp, ke->filter, ke->fflags, 16);
	fprintf(fp, ",%#jx,%p", (uintmax_t)ke->data, ke->udata);
}

static void
print_utrace(FILE *fp, void *utrace_addr, size_t len)
{
	unsigned char *utrace_buffer;

	fprintf(fp, "{ ");
	if (sysdecode_utrace(fp, utrace_addr, len)) {
		fprintf(fp, " }");
		return;
	}

	utrace_buffer = utrace_addr;
	fprintf(fp, "%zu:", len);
	while (len--)
		fprintf(fp, " %02x", *utrace_buffer++);
	fprintf(fp, " }");
}

static void
print_pointer(FILE *fp, uintptr_t arg)
{

	fprintf(fp, "%p", (void *)arg);
}

static void
print_sockaddr(FILE *fp, struct trussinfo *trussinfo, uintptr_t arg,
    socklen_t len)
{
	char addr[64];
	struct sockaddr_in *lsin;
	struct sockaddr_in6 *lsin6;
	struct sockaddr_un *sun;
	struct sockaddr *sa;
	u_char *q;
	pid_t pid = trussinfo->curthread->proc->pid;

	if (arg == 0) {
		fputs("NULL", fp);
		return;
	}
	/* If the length is too small, just bail. */
	if (len < sizeof(*sa)) {
		print_pointer(fp, arg);
		return;
	}

	sa = calloc(1, len);
	if (get_struct(pid, arg, sa, len) == -1) {
		free(sa);
		print_pointer(fp, arg);
		return;
	}

	switch (sa->sa_family) {
	case AF_INET:
		if (len < sizeof(*lsin))
			goto sockaddr_short;
		lsin = (struct sockaddr_in *)(void *)sa;
		inet_ntop(AF_INET, &lsin->sin_addr, addr, sizeof(addr));
		fprintf(fp, "{ AF_INET %s:%d }", addr,
		    htons(lsin->sin_port));
		break;
	case AF_INET6:
		if (len < sizeof(*lsin6))
			goto sockaddr_short;
		lsin6 = (struct sockaddr_in6 *)(void *)sa;
		inet_ntop(AF_INET6, &lsin6->sin6_addr, addr,
		    sizeof(addr));
		fprintf(fp, "{ AF_INET6 [%s]:%d }", addr,
		    htons(lsin6->sin6_port));
		break;
	case AF_UNIX:
		sun = (struct sockaddr_un *)sa;
		fprintf(fp, "{ AF_UNIX \"%.*s\" }",
		    (int)(len - offsetof(struct sockaddr_un, sun_path)),
		    sun->sun_path);
		break;
	default:
	sockaddr_short:
		fprintf(fp,
		    "{ sa_len = %d, sa_family = %d, sa_data = {",
		    (int)sa->sa_len, (int)sa->sa_family);
		for (q = (u_char *)sa->sa_data;
		     q < (u_char *)sa + len; q++)
			fprintf(fp, "%s 0x%02x",
			    q == (u_char *)sa->sa_data ? "" : ",",
			    *q);
		fputs(" } }", fp);
	}
	free(sa);
}

#define IOV_LIMIT 16

static void
print_iovec(FILE *fp, struct trussinfo *trussinfo, uintptr_t arg, int iovcnt)
{
	struct iovec iov[IOV_LIMIT];
	size_t max_string = trussinfo->strsize;
	char tmp2[max_string + 1], *tmp3;
	size_t len;
	pid_t pid = trussinfo->curthread->proc->pid;
	int i;
	bool buf_truncated, iov_truncated;

	if (iovcnt <= 0) {
		print_pointer(fp, arg);
		return;
	}
	if (iovcnt > IOV_LIMIT) {
		iovcnt = IOV_LIMIT;
		iov_truncated = true;
	} else {
		iov_truncated = false;
	}
	if (get_struct(pid, arg, &iov, iovcnt * sizeof(struct iovec)) == -1) {
		print_pointer(fp, arg);
		return;
	}

	fputs("[", fp);
	for (i = 0; i < iovcnt; i++) {
		len = iov[i].iov_len;
		if (len > max_string) {
			len = max_string;
			buf_truncated = true;
		} else {
			buf_truncated = false;
		}
		fprintf(fp, "%s{", (i > 0) ? "," : "");
		if (len && get_struct(pid, (uintptr_t)iov[i].iov_base, &tmp2, len) != -1) {
			tmp3 = malloc(len * 4 + 1);
			while (len) {
				if (strvisx(tmp3, tmp2, len,
				    VIS_CSTYLE|VIS_TAB|VIS_NL) <=
				    (int)max_string)
					break;
				len--;
				buf_truncated = true;
			}
			fprintf(fp, "\"%s\"%s", tmp3,
			    buf_truncated ? "..." : "");
			free(tmp3);
		} else {
			print_pointer(fp, (uintptr_t)iov[i].iov_base);
		}
		fprintf(fp, ",%zu}", iov[i].iov_len);
	}
	fprintf(fp, "%s%s", iov_truncated ? ",..." : "", "]");
}

static void
print_sigval(FILE *fp, union sigval *sv)
{
	fprintf(fp, "{ %d, %p }", sv->sival_int, sv->sival_ptr);
}

static void
print_sigevent(FILE *fp, struct sigevent *se)
{
	fputs("{ sigev_notify=", fp);
	switch (se->sigev_notify) {
	case SIGEV_NONE:
		fputs("SIGEV_NONE", fp);
		break;
	case SIGEV_SIGNAL:
		fprintf(fp, "SIGEV_SIGNAL, sigev_signo=%s, sigev_value=",
				strsig2(se->sigev_signo));
		print_sigval(fp, &se->sigev_value);
		break;
	case SIGEV_THREAD:
		fputs("SIGEV_THREAD, sigev_value=", fp);
		print_sigval(fp, &se->sigev_value);
		break;
	case SIGEV_KEVENT:
		fprintf(fp, "SIGEV_KEVENT, sigev_notify_kqueue=%d, sigev_notify_kevent_flags=",
				se->sigev_notify_kqueue);
		print_mask_arg(sysdecode_kevent_flags, fp, se->sigev_notify_kevent_flags);
		break;
	case SIGEV_THREAD_ID:
		fprintf(fp, "SIGEV_THREAD_ID, sigev_notify_thread_id=%d, sigev_signo=%s, sigev_value=",
				se->sigev_notify_thread_id, strsig2(se->sigev_signo));
		print_sigval(fp, &se->sigev_value);
		break;
	default:
		fprintf(fp, "%d", se->sigev_notify);
		break;
	}
	fputs(" }", fp);
}

static void
print_aiocb(FILE *fp, struct aiocb *cb)
{
	fprintf(fp, "{ %d,%jd,%p,%zu,%s,",
			cb->aio_fildes,
			cb->aio_offset,
			cb->aio_buf,
			cb->aio_nbytes,
			xlookup(lio_opcodes, cb->aio_lio_opcode));
	print_sigevent(fp, &cb->aio_sigevent);
	fputs(" }", fp);
}

static void
print_gen_cmsg(FILE *fp, struct cmsghdr *cmsghdr)
{
	u_char *q;

	fputs("{", fp);
	for (q = CMSG_DATA(cmsghdr);
	     q < (u_char *)cmsghdr + cmsghdr->cmsg_len; q++) {
		fprintf(fp, "%s0x%02x", q == CMSG_DATA(cmsghdr) ? "" : ",", *q);
	}
	fputs("}", fp);
}

static void
print_sctp_initmsg(FILE *fp, struct sctp_initmsg *init)
{
	fprintf(fp, "{out=%u,", init->sinit_num_ostreams);
	fprintf(fp, "in=%u,", init->sinit_max_instreams);
	fprintf(fp, "max_rtx=%u,", init->sinit_max_attempts);
	fprintf(fp, "max_rto=%u}", init->sinit_max_init_timeo);
}

static void
print_sctp_sndrcvinfo(FILE *fp, bool receive, struct sctp_sndrcvinfo *info)
{
	fprintf(fp, "{sid=%u,", info->sinfo_stream);
	if (receive) {
		fprintf(fp, "ssn=%u,", info->sinfo_ssn);
	}
	fputs("flgs=", fp);
	sysdecode_sctp_sinfo_flags(fp, info->sinfo_flags);
	fprintf(fp, ",ppid=%u,", ntohl(info->sinfo_ppid));
	if (!receive) {
		fprintf(fp, "ctx=%u,", info->sinfo_context);
		fprintf(fp, "ttl=%u,", info->sinfo_timetolive);
	}
	if (receive) {
		fprintf(fp, "tsn=%u,", info->sinfo_tsn);
		fprintf(fp, "cumtsn=%u,", info->sinfo_cumtsn);
	}
	fprintf(fp, "id=%u}", info->sinfo_assoc_id);
}

static void
print_sctp_sndinfo(FILE *fp, struct sctp_sndinfo *info)
{
	fprintf(fp, "{sid=%u,", info->snd_sid);
	fputs("flgs=", fp);
	print_mask_arg(sysdecode_sctp_snd_flags, fp, info->snd_flags);
	fprintf(fp, ",ppid=%u,", ntohl(info->snd_ppid));
	fprintf(fp, "ctx=%u,", info->snd_context);
	fprintf(fp, "id=%u}", info->snd_assoc_id);
}

static void
print_sctp_rcvinfo(FILE *fp, struct sctp_rcvinfo *info)
{
	fprintf(fp, "{sid=%u,", info->rcv_sid);
	fprintf(fp, "ssn=%u,", info->rcv_ssn);
	fputs("flgs=", fp);
	print_mask_arg(sysdecode_sctp_rcv_flags, fp, info->rcv_flags);
	fprintf(fp, ",ppid=%u,", ntohl(info->rcv_ppid));
	fprintf(fp, "tsn=%u,", info->rcv_tsn);
	fprintf(fp, "cumtsn=%u,", info->rcv_cumtsn);
	fprintf(fp, "ctx=%u,", info->rcv_context);
	fprintf(fp, "id=%u}", info->rcv_assoc_id);
}

static void
print_sctp_nxtinfo(FILE *fp, struct sctp_nxtinfo *info)
{
	fprintf(fp, "{sid=%u,", info->nxt_sid);
	fputs("flgs=", fp);
	print_mask_arg(sysdecode_sctp_nxt_flags, fp, info->nxt_flags);
	fprintf(fp, ",ppid=%u,", ntohl(info->nxt_ppid));
	fprintf(fp, "len=%u,", info->nxt_length);
	fprintf(fp, "id=%u}", info->nxt_assoc_id);
}

static void
print_sctp_prinfo(FILE *fp, struct sctp_prinfo *info)
{
	fputs("{pol=", fp);
	print_integer_arg(sysdecode_sctp_pr_policy, fp, info->pr_policy);
	fprintf(fp, ",val=%u}", info->pr_value);
}

static void
print_sctp_authinfo(FILE *fp, struct sctp_authinfo *info)
{
	fprintf(fp, "{num=%u}", info->auth_keynumber);
}

static void
print_sctp_ipv4_addr(FILE *fp, struct in_addr *addr)
{
	char buf[INET_ADDRSTRLEN];
	const char *s;

	s = inet_ntop(AF_INET, addr, buf, INET_ADDRSTRLEN);
	if (s != NULL)
		fprintf(fp, "{addr=%s}", s);
	else
		fputs("{addr=???}", fp);
}

static void
print_sctp_ipv6_addr(FILE *fp, struct in6_addr *addr)
{
	char buf[INET6_ADDRSTRLEN];
	const char *s;

	s = inet_ntop(AF_INET6, addr, buf, INET6_ADDRSTRLEN);
	if (s != NULL)
		fprintf(fp, "{addr=%s}", s);
	else
		fputs("{addr=???}", fp);
}

static void
print_sctp_cmsg(FILE *fp, bool receive, struct cmsghdr *cmsghdr)
{
	void *data;
	socklen_t len;

	len = cmsghdr->cmsg_len;
	data = CMSG_DATA(cmsghdr);
	switch (cmsghdr->cmsg_type) {
	case SCTP_INIT:
		if (len == CMSG_LEN(sizeof(struct sctp_initmsg)))
			print_sctp_initmsg(fp, (struct sctp_initmsg *)data);
		else
			print_gen_cmsg(fp, cmsghdr);
		break;
	case SCTP_SNDRCV:
		if (len == CMSG_LEN(sizeof(struct sctp_sndrcvinfo)))
			print_sctp_sndrcvinfo(fp, receive,
			    (struct sctp_sndrcvinfo *)data);
		else
			print_gen_cmsg(fp, cmsghdr);
		break;
#if 0
	case SCTP_EXTRCV:
		if (len == CMSG_LEN(sizeof(struct sctp_extrcvinfo)))
			print_sctp_extrcvinfo(fp,
			    (struct sctp_extrcvinfo *)data);
		else
			print_gen_cmsg(fp, cmsghdr);
		break;
#endif
	case SCTP_SNDINFO:
		if (len == CMSG_LEN(sizeof(struct sctp_sndinfo)))
			print_sctp_sndinfo(fp, (struct sctp_sndinfo *)data);
		else
			print_gen_cmsg(fp, cmsghdr);
		break;
	case SCTP_RCVINFO:
		if (len == CMSG_LEN(sizeof(struct sctp_rcvinfo)))
			print_sctp_rcvinfo(fp, (struct sctp_rcvinfo *)data);
		else
			print_gen_cmsg(fp, cmsghdr);
		break;
	case SCTP_NXTINFO:
		if (len == CMSG_LEN(sizeof(struct sctp_nxtinfo)))
			print_sctp_nxtinfo(fp, (struct sctp_nxtinfo *)data);
		else
			print_gen_cmsg(fp, cmsghdr);
		break;
	case SCTP_PRINFO:
		if (len == CMSG_LEN(sizeof(struct sctp_prinfo)))
			print_sctp_prinfo(fp, (struct sctp_prinfo *)data);
		else
			print_gen_cmsg(fp, cmsghdr);
		break;
	case SCTP_AUTHINFO:
		if (len == CMSG_LEN(sizeof(struct sctp_authinfo)))
			print_sctp_authinfo(fp, (struct sctp_authinfo *)data);
		else
			print_gen_cmsg(fp, cmsghdr);
		break;
	case SCTP_DSTADDRV4:
		if (len == CMSG_LEN(sizeof(struct in_addr)))
			print_sctp_ipv4_addr(fp, (struct in_addr *)data);
		else
			print_gen_cmsg(fp, cmsghdr);
		break;
	case SCTP_DSTADDRV6:
		if (len == CMSG_LEN(sizeof(struct in6_addr)))
			print_sctp_ipv6_addr(fp, (struct in6_addr *)data);
		else
			print_gen_cmsg(fp, cmsghdr);
		break;
	default:
		print_gen_cmsg(fp, cmsghdr);
	}
}

static void
print_cmsgs(FILE *fp, pid_t pid, bool receive, struct msghdr *msghdr)
{
	struct cmsghdr *cmsghdr;
	char *cmsgbuf;
	const char *temp;
	socklen_t len;
	int level, type;
	bool first;

	len = msghdr->msg_controllen;
	if (len == 0) {
		fputs("{}", fp);
		return;
	}
	cmsgbuf = calloc(1, len);
	if (get_struct(pid, (uintptr_t)msghdr->msg_control, cmsgbuf, len) == -1) {
		print_pointer(fp, (uintptr_t)msghdr->msg_control);
		free(cmsgbuf);
		return;
	}
	msghdr->msg_control = cmsgbuf;
	first = true;
	fputs("{", fp);
	for (cmsghdr = CMSG_FIRSTHDR(msghdr);
	   cmsghdr != NULL;
	   cmsghdr = CMSG_NXTHDR(msghdr, cmsghdr)) {
		if (cmsghdr->cmsg_len < sizeof(*cmsghdr)) {
			fprintf(fp, "{<invalid cmsg, len=%u>}",
			    cmsghdr->cmsg_len);
			if (cmsghdr->cmsg_len == 0) {
				/* Avoid looping forever. */
				break;
			}
			continue;
		}

		level = cmsghdr->cmsg_level;
		type = cmsghdr->cmsg_type;
		len = cmsghdr->cmsg_len;
		fprintf(fp, "%s{level=", first ? "" : ",");
		print_integer_arg(sysdecode_sockopt_level, fp, level);
		fputs(",type=", fp);
		temp = sysdecode_cmsg_type(level, type);
		if (temp) {
			fputs(temp, fp);
		} else {
			fprintf(fp, "%d", type);
		}
		fputs(",data=", fp);
		switch (level) {
		case IPPROTO_SCTP:
			print_sctp_cmsg(fp, receive, cmsghdr);
			break;
		default:
			print_gen_cmsg(fp, cmsghdr);
			break;
		}
		fputs("}", fp);
		first = false;
	}
	fputs("}", fp);
	free(cmsgbuf);
}

static void
print_sysctl_oid(FILE *fp, int *oid, size_t len)
{
	size_t i;
	bool first;

	first = true;
	fprintf(fp, "{ ");
	for (i = 0; i < len; i++) {
		fprintf(fp, "%s%d", first ? "" : ".", oid[i]);
		first = false;
	}
	fprintf(fp, " }");
}

static void
print_sysctl(FILE *fp, int *oid, size_t len)
{
	char name[BUFSIZ];
	int qoid[CTL_MAXNAME + 2];
	size_t i;

	qoid[0] = CTL_SYSCTL;
	qoid[1] = CTL_SYSCTL_NAME;
	memcpy(qoid + 2, oid, len * sizeof(int));
	i = sizeof(name);
	if (sysctl(qoid, len + 2, name, &i, 0, 0) == -1)
		print_sysctl_oid(fp, oid, len);
	else
		fprintf(fp, "%s", name);
}

/*
 * Convert a 32-bit user-space pointer to psaddr_t by zero-extending.
 */
static psaddr_t
user_ptr32_to_psaddr(int32_t user_pointer)
{
	return ((psaddr_t)(uintptr_t)user_pointer);
}

/*
 * Converts a syscall argument into a string.  Said string is
 * allocated via malloc(), so needs to be free()'d.  sc is
 * a pointer to the syscall description (see above); args is
 * an array of all of the system call arguments.
 */
char *
print_arg(struct syscall_arg *sc, syscallarg_t *args, syscallarg_t *retval,
    struct trussinfo *trussinfo)
{
	FILE *fp;
	char *tmp;
	size_t tmplen;
	pid_t pid;

	fp = open_memstream(&tmp, &tmplen);
	pid = trussinfo->curthread->proc->pid;
	switch (sc->type & ARG_MASK) {
	case Hex:
		fprintf(fp, "0x%x", (int)args[sc->offset]);
		break;
	case Octal:
		fprintf(fp, "0%o", (int)args[sc->offset]);
		break;
	case Int:
		fprintf(fp, "%d", (int)args[sc->offset]);
		break;
	case UInt:
		fprintf(fp, "%u", (unsigned int)args[sc->offset]);
		break;
	case PUInt: {
		unsigned int val;

		if (get_struct(pid, args[sc->offset], &val,
		    sizeof(val)) == 0) 
			fprintf(fp, "{ %u }", val);
		else
			print_pointer(fp, args[sc->offset]);
		break;
	}
	case LongHex:
		fprintf(fp, "0x%lx", (long)args[sc->offset]);
		break;
	case Long:
		fprintf(fp, "%ld", (long)args[sc->offset]);
		break;
	case Sizet:
		fprintf(fp, "%zu", (size_t)args[sc->offset]);
		break;
	case ShmName:
		/* Handle special SHM_ANON value. */
		if ((char *)(uintptr_t)args[sc->offset] == SHM_ANON) {
			fprintf(fp, "SHM_ANON");
			break;
		}
		/* FALLTHROUGH */
	case Name: {
		/* NULL-terminated string. */
		char *tmp2;

		tmp2 = get_string(pid, args[sc->offset], 0);
		fprintf(fp, "\"%s\"", tmp2);
		free(tmp2);
		break;
	}
	case BinString: {
		/*
		 * Binary block of data that might have printable characters.
		 * XXX If type|OUT, assume that the length is the syscall's
		 * return value.  Otherwise, assume that the length of the block
		 * is in the next syscall argument.
		 */
		int max_string = trussinfo->strsize;
		char tmp2[max_string + 1], *tmp3;
		int len;
		int truncated = 0;

		if (sc->type & OUT)
			len = retval[0];
		else
			len = args[sc->offset + 1];

		/*
		 * Don't print more than max_string characters, to avoid word
		 * wrap.  If we have to truncate put some ... after the string.
		 */
		if (len > max_string) {
			len = max_string;
			truncated = 1;
		}
		if (len && get_struct(pid, args[sc->offset], &tmp2, len)
		    != -1) {
			tmp3 = malloc(len * 4 + 1);
			while (len) {
				if (strvisx(tmp3, tmp2, len,
				    VIS_CSTYLE|VIS_TAB|VIS_NL) <= max_string)
					break;
				len--;
				truncated = 1;
			}
			fprintf(fp, "\"%s\"%s", tmp3, truncated ?
			    "..." : "");
			free(tmp3);
		} else {
			print_pointer(fp, args[sc->offset]);
		}
		break;
	}
	case ExecArgs:
	case ExecEnv:
	case StringArray: {
		psaddr_t addr;
		union {
			int32_t strarray32[PAGE_SIZE / sizeof(int32_t)];
			int64_t strarray64[PAGE_SIZE / sizeof(int64_t)];
			char buf[PAGE_SIZE];
		} u;
		char *string;
		size_t len;
		u_int first, i;
		size_t pointer_size =
		    trussinfo->curthread->proc->abi->pointer_size;

		/*
		 * Only parse argv[] and environment arrays from exec calls
		 * if requested.
		 */
		if (((sc->type & ARG_MASK) == ExecArgs &&
		    (trussinfo->flags & EXECVEARGS) == 0) ||
		    ((sc->type & ARG_MASK) == ExecEnv &&
		    (trussinfo->flags & EXECVEENVS) == 0)) {
			print_pointer(fp, args[sc->offset]);
			break;
		}

		/*
		 * Read a page of pointers at a time.  Punt if the top-level
		 * pointer is not aligned.  Note that the first read is of
		 * a partial page.
		 */
		addr = args[sc->offset];
		if (!__is_aligned(addr, pointer_size)) {
			print_pointer(fp, args[sc->offset]);
			break;
		}

		len = PAGE_SIZE - (addr & PAGE_MASK);
		if (get_struct(pid, addr, u.buf, len) == -1) {
			print_pointer(fp, args[sc->offset]);
			break;
		}
		assert(len > 0);

		fputc('[', fp);
		first = 1;
		i = 0;
		for (;;) {
			psaddr_t straddr;
			if (pointer_size == 4) {
				straddr = user_ptr32_to_psaddr(u.strarray32[i]);
			} else if (pointer_size == 8) {
				straddr = (psaddr_t)u.strarray64[i];
			} else {
				errx(1, "Unsupported pointer size: %zu",
				    pointer_size);
			}

			/* Stop once we read the first NULL pointer. */
			if (straddr == 0)
				break;
			string = get_string(pid, straddr, 0);
			fprintf(fp, "%s \"%s\"", first ? "" : ",", string);
			free(string);
			first = 0;

			i++;
			if (i == len / pointer_size) {
				addr += len;
				len = PAGE_SIZE;
				if (get_struct(pid, addr, u.buf, len) == -1) {
					fprintf(fp, ", <inval>");
					break;
				}
				i = 0;
			}
		}
		fputs(" ]", fp);
		break;
	}
	case Quad:
	case QuadHex: {
		uint64_t value;
		size_t pointer_size =
		    trussinfo->curthread->proc->abi->pointer_size;

		if (pointer_size == 4) {
#if _BYTE_ORDER == _LITTLE_ENDIAN
			value = (uint64_t)args[sc->offset + 1] << 32 |
			    args[sc->offset];
#else
			value = (uint64_t)args[sc->offset] << 32 |
			    args[sc->offset + 1];
#endif
		} else {
			value = (uint64_t)args[sc->offset];
		}
		if ((sc->type & ARG_MASK) == Quad)
			fprintf(fp, "%jd", (intmax_t)value);
		else
			fprintf(fp, "0x%jx", (intmax_t)value);
		break;
	}
	case PQuadHex: {
		uint64_t val;

		if (get_struct(pid, args[sc->offset], &val,
		    sizeof(val)) == 0) 
			fprintf(fp, "{ 0x%jx }", (uintmax_t)val);
		else
			print_pointer(fp, args[sc->offset]);
		break;
	}
	case Ptr:
		print_pointer(fp, args[sc->offset]);
		break;
	case Readlinkres: {
		char *tmp2;

		if (retval[0] == -1)
			break;
		tmp2 = get_string(pid, args[sc->offset], retval[0]);
		fprintf(fp, "\"%s\"", tmp2);
		free(tmp2);
		break;
	}
	case Ioctl: {
		const char *temp;
		unsigned long cmd;

		cmd = args[sc->offset];
		temp = sysdecode_ioctlname(cmd);
		if (temp)
			fputs(temp, fp);
		else {
			fprintf(fp, "0x%lx { IO%s%s 0x%lx('%c'), %lu, %lu }",
			    cmd, cmd & IOC_OUT ? "R" : "",
			    cmd & IOC_IN ? "W" : "", IOCGROUP(cmd),
			    isprint(IOCGROUP(cmd)) ? (char)IOCGROUP(cmd) : '?',
			    cmd & 0xFF, IOCPARM_LEN(cmd));
		}
		break;
	}
	case Timespec: {
		struct timespec ts;

		if (get_struct(pid, args[sc->offset], &ts, sizeof(ts)) != -1)
			fprintf(fp, "{ %jd.%09ld }", (intmax_t)ts.tv_sec,
			    ts.tv_nsec);
		else
			print_pointer(fp, args[sc->offset]);
		break;
	}
	case Timespec2: {
		struct timespec ts[2];
		const char *sep;
		unsigned int i;

		if (get_struct(pid, args[sc->offset], &ts, sizeof(ts)) != -1) {
			fputs("{ ", fp);
			sep = "";
			for (i = 0; i < nitems(ts); i++) {
				fputs(sep, fp);
				sep = ", ";
				switch (ts[i].tv_nsec) {
				case UTIME_NOW:
					fprintf(fp, "UTIME_NOW");
					break;
				case UTIME_OMIT:
					fprintf(fp, "UTIME_OMIT");
					break;
				default:
					fprintf(fp, "%jd.%09ld",
					    (intmax_t)ts[i].tv_sec,
					    ts[i].tv_nsec);
					break;
				}
			}
			fputs(" }", fp);
		} else
			print_pointer(fp, args[sc->offset]);
		break;
	}
	case Timeval: {
		struct timeval tv;

		if (get_struct(pid, args[sc->offset], &tv, sizeof(tv)) != -1)
			fprintf(fp, "{ %jd.%06ld }", (intmax_t)tv.tv_sec,
			    tv.tv_usec);
		else
			print_pointer(fp, args[sc->offset]);
		break;
	}
	case Timeval2: {
		struct timeval tv[2];

		if (get_struct(pid, args[sc->offset], &tv, sizeof(tv)) != -1)
			fprintf(fp, "{ %jd.%06ld, %jd.%06ld }",
			    (intmax_t)tv[0].tv_sec, tv[0].tv_usec,
			    (intmax_t)tv[1].tv_sec, tv[1].tv_usec);
		else
			print_pointer(fp, args[sc->offset]);
		break;
	}
	case Itimerval: {
		struct itimerval itv;

		if (get_struct(pid, args[sc->offset], &itv, sizeof(itv)) != -1)
			fprintf(fp, "{ %jd.%06ld, %jd.%06ld }",
			    (intmax_t)itv.it_interval.tv_sec,
			    itv.it_interval.tv_usec,
			    (intmax_t)itv.it_value.tv_sec,
			    itv.it_value.tv_usec);
		else
			print_pointer(fp, args[sc->offset]);
		break;
	}
	case LinuxSockArgs:
	{
		struct linux_socketcall_args largs;

		if (get_struct(pid, args[sc->offset], (void *)&largs,
		    sizeof(largs)) != -1)
			fprintf(fp, "{ %s, 0x%lx }",
			    lookup(linux_socketcall_ops, largs.what, 10),
			    (long unsigned int)largs.args);
		else
			print_pointer(fp, args[sc->offset]);
		break;
	}
	case Pollfd: {
		/*
		 * XXX: A Pollfd argument expects the /next/ syscall argument
		 * to be the number of fds in the array. This matches the poll
		 * syscall.
		 */
		struct pollfd *pfd;
		int numfds = args[sc->offset + 1];
		size_t bytes = sizeof(struct pollfd) * numfds;
		int i;

		if ((pfd = malloc(bytes)) == NULL)
			err(1, "Cannot malloc %zu bytes for pollfd array",
			    bytes);
		if (get_struct(pid, args[sc->offset], pfd, bytes) != -1) {
			fputs("{", fp);
			for (i = 0; i < numfds; i++) {
				fprintf(fp, " %d/%s", pfd[i].fd,
				    xlookup_bits(poll_flags, pfd[i].events));
			}
			fputs(" }", fp);
		} else {
			print_pointer(fp, args[sc->offset]);
		}
		free(pfd);
		break;
	}
	case Fd_set: {
		/*
		 * XXX: A Fd_set argument expects the /first/ syscall argument
		 * to be the number of fds in the array.  This matches the
		 * select syscall.
		 */
		fd_set *fds;
		int numfds = args[0];
		size_t bytes = _howmany(numfds, _NFDBITS) * _NFDBITS;
		int i;

		if ((fds = malloc(bytes)) == NULL)
			err(1, "Cannot malloc %zu bytes for fd_set array",
			    bytes);
		if (get_struct(pid, args[sc->offset], fds, bytes) != -1) {
			fputs("{", fp);
			for (i = 0; i < numfds; i++) {
				if (FD_ISSET(i, fds))
					fprintf(fp, " %d", i);
			}
			fputs(" }", fp);
		} else
			print_pointer(fp, args[sc->offset]);
		free(fds);
		break;
	}
	case Signal:
		fputs(strsig2(args[sc->offset]), fp);
		break;
	case Sigset: {
		sigset_t ss;
		int i, first;

		if (get_struct(pid, args[sc->offset], (void *)&ss,
		    sizeof(ss)) == -1) {
			print_pointer(fp, args[sc->offset]);
			break;
		}
		fputs("{ ", fp);
		first = 1;
		for (i = 1; i < sys_nsig; i++) {
			if (sigismember(&ss, i)) {
				fprintf(fp, "%s%s", !first ? "|" : "",
				    strsig2(i));
				first = 0;
			}
		}
		if (!first)
			fputc(' ', fp);
		fputc('}', fp);
		break;
	}
	case Sigprocmask:
		print_integer_arg(sysdecode_sigprocmask_how, fp,
		    args[sc->offset]);
		break;
	case Fcntlflag:
		/* XXX: Output depends on the value of the previous argument. */
		if (sysdecode_fcntl_arg_p(args[sc->offset - 1]))
			sysdecode_fcntl_arg(fp, args[sc->offset - 1],
			    args[sc->offset], 16);
		break;
	case Open:
		print_mask_arg(sysdecode_open_flags, fp, args[sc->offset]);
		break;
	case Fcntl:
		print_integer_arg(sysdecode_fcntl_cmd, fp, args[sc->offset]);
		break;
	case Closerangeflags:
		print_mask_arg(sysdecode_close_range_flags, fp, args[sc->offset]);
		break;
	case Mprot:
		print_mask_arg(sysdecode_mmap_prot, fp, args[sc->offset]);
		break;
	case Mmapflags:
		print_mask_arg(sysdecode_mmap_flags, fp, args[sc->offset]);
		break;
	case Whence:
		print_integer_arg(sysdecode_whence, fp, args[sc->offset]);
		break;
	case ShmFlags:
		print_mask_arg(sysdecode_shmflags, fp, args[sc->offset]);
		break;
	case Sockdomain:
		print_integer_arg(sysdecode_socketdomain, fp, args[sc->offset]);
		break;
	case Socktype:
		print_mask_arg(sysdecode_socket_type, fp, args[sc->offset]);
		break;
	case Shutdown:
		print_integer_arg(sysdecode_shutdown_how, fp, args[sc->offset]);
		break;
	case Resource:
		print_integer_arg(sysdecode_rlimit, fp, args[sc->offset]);
		break;
	case RusageWho:
		print_integer_arg(sysdecode_getrusage_who, fp, args[sc->offset]);
		break;
	case Pathconf:
		print_integer_arg(sysdecode_pathconf_name, fp, args[sc->offset]);
		break;
	case Rforkflags:
		print_mask_arg(sysdecode_rfork_flags, fp, args[sc->offset]);
		break;
	case Sockaddr: {
		socklen_t len;

		if (args[sc->offset] == 0) {
			fputs("NULL", fp);
			break;
		}

		/*
		 * Extract the address length from the next argument.  If
		 * this is an output sockaddr (OUT is set), then the
		 * next argument is a pointer to a socklen_t.  Otherwise
		 * the next argument contains a socklen_t by value.
		 */
		if (sc->type & OUT) {
			if (get_struct(pid, args[sc->offset + 1], &len,
			    sizeof(len)) == -1) {
				print_pointer(fp, args[sc->offset]);
				break;
			}
		} else
			len = args[sc->offset + 1];

		print_sockaddr(fp, trussinfo, args[sc->offset], len);
		break;
	}
	case Sigaction: {
		struct sigaction sa;

		if (get_struct(pid, args[sc->offset], &sa, sizeof(sa)) != -1) {
			fputs("{ ", fp);
			if (sa.sa_handler == SIG_DFL)
				fputs("SIG_DFL", fp);
			else if (sa.sa_handler == SIG_IGN)
				fputs("SIG_IGN", fp);
			else
				fprintf(fp, "%p", sa.sa_handler);
			fprintf(fp, " %s ss_t }",
			    xlookup_bits(sigaction_flags, sa.sa_flags));
		} else
			print_pointer(fp, args[sc->offset]);
		break;
	}
	case Sigevent: {
		struct sigevent se;

		if (get_struct(pid, args[sc->offset], &se, sizeof(se)) != -1)
			print_sigevent(fp, &se);
		else
			print_pointer(fp, args[sc->offset]);
		break;
	}
	case Kevent: {
		/*
		 * XXX XXX: The size of the array is determined by either the
		 * next syscall argument, or by the syscall return value,
		 * depending on which argument number we are.  This matches the
		 * kevent syscall, but luckily that's the only syscall that uses
		 * them.
		 */
		struct kevent *ke;
		int numevents = -1;
		size_t bytes;
		int i;

		if (sc->offset == 1)
			numevents = args[sc->offset+1];
		else if (sc->offset == 3 && retval[0] != -1)
			numevents = retval[0];

		if (numevents >= 0) {
			bytes = sizeof(struct kevent) * numevents;
			if ((ke = malloc(bytes)) == NULL)
				err(1,
				    "Cannot malloc %zu bytes for kevent array",
				    bytes);
		} else
			ke = NULL;
		if (numevents >= 0 && get_struct(pid, args[sc->offset],
		    ke, bytes) != -1) {
			fputc('{', fp);
			for (i = 0; i < numevents; i++) {
				fputc(' ', fp);
				print_kevent(fp, &ke[i]);
			}
			fputs(" }", fp);
		} else {
			print_pointer(fp, args[sc->offset]);
		}
		free(ke);
		break;
	}
	case Kevent11: {
		struct freebsd11_kevent *ke11;
		struct kevent ke;
		int numevents = -1;
		size_t bytes;
		int i;

		if (sc->offset == 1)
			numevents = args[sc->offset+1];
		else if (sc->offset == 3 && retval[0] != -1)
			numevents = retval[0];

		if (numevents >= 0) {
			bytes = sizeof(struct freebsd11_kevent) * numevents;
			if ((ke11 = malloc(bytes)) == NULL)
				err(1,
				    "Cannot malloc %zu bytes for kevent array",
				    bytes);
		} else
			ke11 = NULL;
		memset(&ke, 0, sizeof(ke));
		if (numevents >= 0 && get_struct(pid, args[sc->offset],
		    ke11, bytes) != -1) {
			fputc('{', fp);
			for (i = 0; i < numevents; i++) {
				fputc(' ', fp);
				ke.ident = ke11[i].ident;
				ke.filter = ke11[i].filter;
				ke.flags = ke11[i].flags;
				ke.fflags = ke11[i].fflags;
				ke.data = ke11[i].data;
				ke.udata = ke11[i].udata;
				print_kevent(fp, &ke);
			}
			fputs(" }", fp);
		} else {
			print_pointer(fp, args[sc->offset]);
		}
		free(ke11);
		break;
	}
	case Stat: {
		struct stat st;

		if (get_struct(pid, args[sc->offset], &st, sizeof(st))
		    != -1) {
			char mode[12];

			strmode(st.st_mode, mode);
			fprintf(fp,
			    "{ mode=%s,inode=%ju,size=%jd,blksize=%ld }", mode,
			    (uintmax_t)st.st_ino, (intmax_t)st.st_size,
			    (long)st.st_blksize);
		} else {
			print_pointer(fp, args[sc->offset]);
		}
		break;
	}
	case Stat11: {
		struct freebsd11_stat st;

		if (get_struct(pid, args[sc->offset], &st, sizeof(st))
		    != -1) {
			char mode[12];

			strmode(st.st_mode, mode);
			fprintf(fp,
			    "{ mode=%s,inode=%ju,size=%jd,blksize=%ld }", mode,
			    (uintmax_t)st.st_ino, (intmax_t)st.st_size,
			    (long)st.st_blksize);
		} else {
			print_pointer(fp, args[sc->offset]);
		}
		break;
	}
	case StatFs: {
		unsigned int i;
		struct statfs buf;

		if (get_struct(pid, args[sc->offset], &buf,
		    sizeof(buf)) != -1) {
			char fsid[17];

			bzero(fsid, sizeof(fsid));
			if (buf.f_fsid.val[0] != 0 || buf.f_fsid.val[1] != 0) {
			        for (i = 0; i < sizeof(buf.f_fsid); i++)
					snprintf(&fsid[i*2],
					    sizeof(fsid) - (i*2), "%02x",
					    ((u_char *)&buf.f_fsid)[i]);
			}
			fprintf(fp,
			    "{ fstypename=%s,mntonname=%s,mntfromname=%s,"
			    "fsid=%s }", buf.f_fstypename, buf.f_mntonname,
			    buf.f_mntfromname, fsid);
		} else
			print_pointer(fp, args[sc->offset]);
		break;
	}

	case Rusage: {
		struct rusage ru;

		if (get_struct(pid, args[sc->offset], &ru, sizeof(ru))
		    != -1) {
			fprintf(fp,
			    "{ u=%jd.%06ld,s=%jd.%06ld,in=%ld,out=%ld }",
			    (intmax_t)ru.ru_utime.tv_sec, ru.ru_utime.tv_usec,
			    (intmax_t)ru.ru_stime.tv_sec, ru.ru_stime.tv_usec,
			    ru.ru_inblock, ru.ru_oublock);
		} else
			print_pointer(fp, args[sc->offset]);
		break;
	}
	case Rlimit: {
		struct rlimit rl;

		if (get_struct(pid, args[sc->offset], &rl, sizeof(rl))
		    != -1) {
			fprintf(fp, "{ cur=%ju,max=%ju }",
			    rl.rlim_cur, rl.rlim_max);
		} else
			print_pointer(fp, args[sc->offset]);
		break;
	}
	case ExitStatus: {
		int status;

		if (get_struct(pid, args[sc->offset], &status,
		    sizeof(status)) != -1) {
			fputs("{ ", fp);
			if (WIFCONTINUED(status))
				fputs("CONTINUED", fp);
			else if (WIFEXITED(status))
				fprintf(fp, "EXITED,val=%d",
				    WEXITSTATUS(status));
			else if (WIFSIGNALED(status))
				fprintf(fp, "SIGNALED,sig=%s%s",
				    strsig2(WTERMSIG(status)),
				    WCOREDUMP(status) ? ",cored" : "");
			else
				fprintf(fp, "STOPPED,sig=%s",
				    strsig2(WTERMSIG(status)));
			fputs(" }", fp);
		} else
			print_pointer(fp, args[sc->offset]);
		break;
	}
	case Waitoptions:
		print_mask_arg(sysdecode_wait6_options, fp, args[sc->offset]);
		break;
	case Idtype:
		print_integer_arg(sysdecode_idtype, fp, args[sc->offset]);
		break;
	case Procctl:
		print_integer_arg(sysdecode_procctl_cmd, fp, args[sc->offset]);
		break;
	case Umtxop: {
		int rem;

		if (print_mask_arg_part(sysdecode_umtx_op_flags, fp,
		    args[sc->offset], &rem))
			fprintf(fp, "|");
		print_integer_arg(sysdecode_umtx_op, fp, rem);
		break;
	}
	case Atfd:
		print_integer_arg(sysdecode_atfd, fp, args[sc->offset]);
		break;
	case Atflags:
		print_mask_arg(sysdecode_atflags, fp, args[sc->offset]);
		break;
	case Accessmode:
		print_mask_arg(sysdecode_access_mode, fp, args[sc->offset]);
		break;
	case Sysarch:
		print_integer_arg(sysdecode_sysarch_number, fp,
		    args[sc->offset]);
		break;
	case Sysctl: {
		char name[BUFSIZ];
		int oid[CTL_MAXNAME + 2];
		size_t len;

		memset(name, 0, sizeof(name));
		len = args[sc->offset + 1];
		if (get_struct(pid, args[sc->offset], oid,
		    len * sizeof(oid[0])) != -1) {
		    	fprintf(fp, "\"");
			if (oid[0] == CTL_SYSCTL) {
				fprintf(fp, "sysctl.");
				switch (oid[1]) {
				case CTL_SYSCTL_DEBUG:
					fprintf(fp, "debug");
					break;
				case CTL_SYSCTL_NAME:
					fprintf(fp, "name ");
					print_sysctl_oid(fp, oid + 2, len - 2);
					break;
				case CTL_SYSCTL_NEXT:
					fprintf(fp, "next");
					break;
				case CTL_SYSCTL_NAME2OID:
					fprintf(fp, "name2oid %s",
					    get_string(pid,
					        args[sc->offset + 4],
						args[sc->offset + 5]));
					break;
				case CTL_SYSCTL_OIDFMT:
					fprintf(fp, "oidfmt ");
					print_sysctl(fp, oid + 2, len - 2);
					break;
				case CTL_SYSCTL_OIDDESCR:
					fprintf(fp, "oiddescr ");
					print_sysctl(fp, oid + 2, len - 2);
					break;
				case CTL_SYSCTL_OIDLABEL:
					fprintf(fp, "oidlabel ");
					print_sysctl(fp, oid + 2, len - 2);
					break;
				case CTL_SYSCTL_NEXTNOSKIP:
					fprintf(fp, "nextnoskip");
					break;
				default:
					print_sysctl(fp, oid + 1, len - 1);
				}
			} else {
				print_sysctl(fp, oid, len);
			}
		    	fprintf(fp, "\"");
		}
		break;
	}
	case PipeFds:
		/*
		 * The pipe() system call in the kernel returns its
		 * two file descriptors via return values.  However,
		 * the interface exposed by libc is that pipe()
		 * accepts a pointer to an array of descriptors.
		 * Format the output to match the libc API by printing
		 * the returned file descriptors as a fake argument.
		 *
		 * Overwrite the first retval to signal a successful
		 * return as well.
		 */
		fprintf(fp, "{ %d, %d }", (int)retval[0], (int)retval[1]);
		retval[0] = 0;
		break;
	case Utrace: {
		size_t len;
		void *utrace_addr;

		len = args[sc->offset + 1];
		utrace_addr = calloc(1, len);
		if (get_struct(pid, args[sc->offset],
		    (void *)utrace_addr, len) != -1)
			print_utrace(fp, utrace_addr, len);
		else
			print_pointer(fp, args[sc->offset]);
		free(utrace_addr);
		break;
	}
	case IntArray: {
		int descriptors[16];
		unsigned long i, ndescriptors;
		bool truncated;

		ndescriptors = args[sc->offset + 1];
		truncated = false;
		if (ndescriptors > nitems(descriptors)) {
			ndescriptors = nitems(descriptors);
			truncated = true;
		}
		if (get_struct(pid, args[sc->offset],
		    descriptors, ndescriptors * sizeof(descriptors[0])) != -1) {
			fprintf(fp, "{");
			for (i = 0; i < ndescriptors; i++)
				fprintf(fp, i == 0 ? " %d" : ", %d",
				    descriptors[i]);
			fprintf(fp, truncated ? ", ... }" : " }");
		} else
			print_pointer(fp, args[sc->offset]);
		break;
	}
	case Pipe2:
		print_mask_arg(sysdecode_pipe2_flags, fp, args[sc->offset]);
		break;
	case CapFcntlRights: {
		uint32_t rights;

		if (sc->type & OUT) {
			if (get_struct(pid, args[sc->offset], &rights,
			    sizeof(rights)) == -1) {
				print_pointer(fp, args[sc->offset]);
				break;
			}
		} else
			rights = args[sc->offset];
		print_mask_arg32(sysdecode_cap_fcntlrights, fp, rights);
		break;
	}
	case Fadvice:
		print_integer_arg(sysdecode_fadvice, fp, args[sc->offset]);
		break;
	case FileFlags: {
		fflags_t rem;

		if (!sysdecode_fileflags(fp, args[sc->offset], &rem))
			fprintf(fp, "0x%x", rem);
		else if (rem != 0)
			fprintf(fp, "|0x%x", rem);
		break;
	}
	case Flockop:
		print_mask_arg(sysdecode_flock_operation, fp, args[sc->offset]);
		break;
	case Getfsstatmode:
		print_integer_arg(sysdecode_getfsstat_mode, fp,
		    args[sc->offset]);
		break;
	case Inotifyflags:
		print_mask_arg(sysdecode_inotifyflags, fp, args[sc->offset]);
		break;
	case Itimerwhich:
		print_integer_arg(sysdecode_itimer, fp, args[sc->offset]);
		break;
	case Kldsymcmd:
		print_integer_arg(sysdecode_kldsym_cmd, fp, args[sc->offset]);
		break;
	case Kldunloadflags:
		print_integer_arg(sysdecode_kldunload_flags, fp,
		    args[sc->offset]);
		break;
	case AiofsyncOp:
		fputs(xlookup(aio_fsync_ops, args[sc->offset]), fp);
		break;
	case LioMode:
		fputs(xlookup(lio_modes, args[sc->offset]), fp);
		break;
	case Madvice:
		print_integer_arg(sysdecode_madvice, fp, args[sc->offset]);
		break;
	case Socklent:
		fprintf(fp, "%u", (socklen_t)args[sc->offset]);
		break;
	case Sockprotocol: {
		const char *temp;
		int domain, protocol;

		domain = args[sc->offset - 2];
		protocol = args[sc->offset];
		if (protocol == 0) {
			fputs("0", fp);
		} else {
			temp = sysdecode_socket_protocol(domain, protocol);
			if (temp) {
				fputs(temp, fp);
			} else {
				fprintf(fp, "%d", protocol);
			}
		}
		break;
	}
	case Sockoptlevel:
		print_integer_arg(sysdecode_sockopt_level, fp,
		    args[sc->offset]);
		break;
	case Sockoptname: {
		const char *temp;
		int level, name;

		level = args[sc->offset - 1];
		name = args[sc->offset];
		temp = sysdecode_sockopt_name(level, name);
		if (temp) {
			fputs(temp, fp);
		} else {
			fprintf(fp, "%d", name);
		}
		break;
	}
	case Msgflags:
		print_mask_arg(sysdecode_msg_flags, fp, args[sc->offset]);
		break;
	case CapRights: {
		cap_rights_t rights;

		if (get_struct(pid, args[sc->offset], &rights,
		    sizeof(rights)) != -1) {
			fputs("{ ", fp);
			sysdecode_cap_rights(fp, &rights);
			fputs(" }", fp);
		} else
			print_pointer(fp, args[sc->offset]);
		break;
	}
	case Acltype:
		print_integer_arg(sysdecode_acltype, fp, args[sc->offset]);
		break;
	case Extattrnamespace:
		print_integer_arg(sysdecode_extattrnamespace, fp,
		    args[sc->offset]);
		break;
	case Minherit:
		print_integer_arg(sysdecode_minherit_inherit, fp,
		    args[sc->offset]);
		break;
	case Mlockall:
		print_mask_arg(sysdecode_mlockall_flags, fp, args[sc->offset]);
		break;
	case Mountflags:
		print_mask_arg(sysdecode_mount_flags, fp, args[sc->offset]);
		break;
	case Msync:
		print_mask_arg(sysdecode_msync_flags, fp, args[sc->offset]);
		break;
	case Priowhich:
		print_integer_arg(sysdecode_prio_which, fp, args[sc->offset]);
		break;
	case Ptraceop:
		print_integer_arg(sysdecode_ptrace_request, fp,
		    args[sc->offset]);
		break;
	case Sendfileflags:
		print_mask_arg(sysdecode_sendfile_flags, fp, args[sc->offset]);
		break;
	case Sendfilehdtr: {
		struct sf_hdtr hdtr;

		if (get_struct(pid, args[sc->offset], &hdtr, sizeof(hdtr)) !=
		    -1) {
			fprintf(fp, "{");
			print_iovec(fp, trussinfo, (uintptr_t)hdtr.headers,
			    hdtr.hdr_cnt);
			print_iovec(fp, trussinfo, (uintptr_t)hdtr.trailers,
			    hdtr.trl_cnt);
			fprintf(fp, "}");
		} else
			print_pointer(fp, args[sc->offset]);
		break;
	}
	case Quotactlcmd:
		if (!sysdecode_quotactl_cmd(fp, args[sc->offset]))
			fprintf(fp, "%#x", (int)args[sc->offset]);
		break;
	case Reboothowto:
		print_mask_arg(sysdecode_reboot_howto, fp, args[sc->offset]);
		break;
	case Rtpriofunc:
		print_integer_arg(sysdecode_rtprio_function, fp,
		    args[sc->offset]);
		break;
	case Schedpolicy:
		print_integer_arg(sysdecode_scheduler_policy, fp,
		    args[sc->offset]);
		break;
	case Schedparam: {
		struct sched_param sp;

		if (get_struct(pid, args[sc->offset], &sp, sizeof(sp)) != -1)
			fprintf(fp, "{ %d }", sp.sched_priority);
		else
			print_pointer(fp, args[sc->offset]);
		break;
	}
	case PSig: {
		int sig;

		if (get_struct(pid, args[sc->offset], &sig, sizeof(sig)) == 0)
			fprintf(fp, "{ %s }", strsig2(sig));
		else
			print_pointer(fp, args[sc->offset]);
		break;
	}
	case Siginfo: {
		siginfo_t si;

		if (get_struct(pid, args[sc->offset], &si, sizeof(si)) != -1) {
			fprintf(fp, "{ signo=%s", strsig2(si.si_signo));
			decode_siginfo(fp, &si);
			fprintf(fp, " }");
		} else
			print_pointer(fp, args[sc->offset]);
		break;
	}
	case Iovec:
		/*
		 * Print argument as an array of struct iovec, where the next
		 * syscall argument is the number of elements of the array.
		 */

		print_iovec(fp, trussinfo, args[sc->offset],
		    (int)args[sc->offset + 1]);
		break;
	case Aiocb: {
		struct aiocb cb;

		if (get_struct(pid, args[sc->offset], &cb, sizeof(cb)) != -1)
			print_aiocb(fp, &cb);
		else
			print_pointer(fp, args[sc->offset]);
		break;
	}
	case AiocbArray: {
		/*
		 * Print argment as an array of pointers to struct aiocb, where
		 * the next syscall argument is the number of elements.
		 */
		uintptr_t cbs[16];
		unsigned int nent;
		bool truncated;

		nent = args[sc->offset + 1];
		truncated = false;
		if (nent > nitems(cbs)) {
			nent = nitems(cbs);
			truncated = true;
		}

		if (get_struct(pid, args[sc->offset], cbs, sizeof(uintptr_t) * nent) != -1) {
			unsigned int i;
			fputs("[", fp);
			for (i = 0; i < nent; ++i) {
				struct aiocb cb;
				if (i > 0)
					fputc(',', fp);
				if (get_struct(pid, cbs[i], &cb, sizeof(cb)) != -1)
					print_aiocb(fp, &cb);
				else
					print_pointer(fp, cbs[i]);
			}
			if (truncated)
				fputs(",...", fp);
			fputs("]", fp);
		} else
			print_pointer(fp, args[sc->offset]);
		break;
	}
	case AiocbPointer: {
		/*
		 * aio_waitcomplete(2) assigns a pointer to a pointer to struct
		 * aiocb, so we need to handle the extra layer of indirection.
		 */
		uintptr_t cbp;
		struct aiocb cb;

		if (get_struct(pid, args[sc->offset], &cbp, sizeof(cbp)) != -1) {
			if (get_struct(pid, cbp, &cb, sizeof(cb)) != -1)
				print_aiocb(fp, &cb);
			else
				print_pointer(fp, cbp);
		} else
			print_pointer(fp, args[sc->offset]);
		break;
	}
	case Sctpsndrcvinfo: {
		struct sctp_sndrcvinfo info;

		if (get_struct(pid, args[sc->offset],
		    &info, sizeof(struct sctp_sndrcvinfo)) == -1) {
			print_pointer(fp, args[sc->offset]);
			break;
		}
		print_sctp_sndrcvinfo(fp, sc->type & OUT, &info);
		break;
	}
	case Msghdr: {
		struct msghdr msghdr;

		if (get_struct(pid, args[sc->offset],
		    &msghdr, sizeof(struct msghdr)) == -1) {
			print_pointer(fp, args[sc->offset]);
			break;
		}
		fputs("{", fp);
		print_sockaddr(fp, trussinfo, (uintptr_t)msghdr.msg_name, msghdr.msg_namelen);
		fprintf(fp, ",%d,", msghdr.msg_namelen);
		print_iovec(fp, trussinfo, (uintptr_t)msghdr.msg_iov, msghdr.msg_iovlen);
		fprintf(fp, ",%d,", msghdr.msg_iovlen);
		print_cmsgs(fp, pid, sc->type & OUT, &msghdr);
		fprintf(fp, ",%u,", msghdr.msg_controllen);
		print_mask_arg(sysdecode_msg_flags, fp, msghdr.msg_flags);
		fputs("}", fp);
		break;
	}

	default:
		errx(1, "Invalid argument type %d\n", sc->type & ARG_MASK);
	}
	fclose(fp);
	return (tmp);
}

/*
 * Print (to outfile) the system call and its arguments.
 */
void
print_syscall(struct trussinfo *trussinfo)
{
	struct threadinfo *t;
	const char *name;
	char **s_args;
	int i, len, nargs;

	t = trussinfo->curthread;

	name = t->cs.sc->name;
	nargs = t->cs.nargs;
	s_args = t->cs.s_args;

	len = print_line_prefix(trussinfo);
	len += fprintf(trussinfo->outfile, "%s(", name);

	for (i = 0; i < nargs; i++) {
		if (s_args[i] != NULL)
			len += fprintf(trussinfo->outfile, "%s", s_args[i]);
		else
			len += fprintf(trussinfo->outfile,
			    "<missing argument>");
		len += fprintf(trussinfo->outfile, "%s", i < (nargs - 1) ?
		    "," : "");
	}
	len += fprintf(trussinfo->outfile, ")");
	for (i = 0; i < 6 - (len / 8); i++)
		fprintf(trussinfo->outfile, "\t");
}

void
print_syscall_ret(struct trussinfo *trussinfo, int error, syscallarg_t *retval)
{
	struct timespec timediff;
	struct threadinfo *t;
	struct syscall *sc;

	t = trussinfo->curthread;
	sc = t->cs.sc;
	if (trussinfo->flags & COUNTONLY) {
		timespecsub(&t->after, &t->before, &timediff);
		timespecadd(&sc->time, &timediff, &sc->time);
		sc->ncalls++;
		if (error != 0)
			sc->nerror++;
		return;
	}

	print_syscall(trussinfo);
	fflush(trussinfo->outfile);

	if (retval == NULL) {
		/*
		 * This system call resulted in the current thread's exit,
		 * so there is no return value or error to display.
		 */
		fprintf(trussinfo->outfile, "\n");
		return;
	}

	if (error == ERESTART)
		fprintf(trussinfo->outfile, " ERESTART\n");
	else if (error == EJUSTRETURN)
		fprintf(trussinfo->outfile, " EJUSTRETURN\n");
	else if (error != 0) {
		fprintf(trussinfo->outfile, " ERR#%d '%s'\n",
		    sysdecode_freebsd_to_abi_errno(t->proc->abi->abi, error),
		    strerror(error));
	} else if (sc->decode.ret_type == 2 &&
	    t->proc->abi->pointer_size == 4) {
		off_t off;
#if _BYTE_ORDER == _LITTLE_ENDIAN
		off = (off_t)retval[1] << 32 | retval[0];
#else
		off = (off_t)retval[0] << 32 | retval[1];
#endif
		fprintf(trussinfo->outfile, " = %jd (0x%jx)\n", (intmax_t)off,
		    (intmax_t)off);
	} else {
		fprintf(trussinfo->outfile, " = %jd (0x%jx)\n",
		    (intmax_t)retval[0], (intmax_t)retval[0]);
	}
}

void
print_summary(struct trussinfo *trussinfo)
{
	struct timespec total = {0, 0};
	struct syscall *sc;
	int ncall, nerror;

	fprintf(trussinfo->outfile, "%-20s%15s%8s%8s\n",
	    "syscall", "seconds", "calls", "errors");
	ncall = nerror = 0;
	STAILQ_FOREACH(sc, &seen_syscalls, entries) {
		if (sc->ncalls) {
			fprintf(trussinfo->outfile, "%-20s%5jd.%09ld%8d%8d\n",
			    sc->name, (intmax_t)sc->time.tv_sec,
			    sc->time.tv_nsec, sc->ncalls, sc->nerror);
			timespecadd(&total, &sc->time, &total);
			ncall += sc->ncalls;
			nerror += sc->nerror;
		}
	}
	fprintf(trussinfo->outfile, "%20s%15s%8s%8s\n",
	    "", "-------------", "-------", "-------");
	fprintf(trussinfo->outfile, "%-20s%5jd.%09ld%8d%8d\n",
	    "", (intmax_t)total.tv_sec, total.tv_nsec, ncall, nerror);
}
