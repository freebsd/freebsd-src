/*
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * This file has routines used to print out system calls and their
 * arguments.
 */

#include <sys/types.h>
#include <sys/event.h>
#include <sys/ioccom.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/procctl.h>
#include <sys/ptrace.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/umtx.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <machine/sysarch.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysdecode.h>
#include <time.h>
#include <unistd.h>
#include <vis.h>

#include <contrib/cloudabi/cloudabi_types_common.h>

#include "truss.h"
#include "extern.h"
#include "syscall.h"

/* 64-bit alignment on 32-bit platforms. */
#if !defined(__LP64__) && defined(__powerpc__)
#define	QUAD_ALIGN	1
#else
#define	QUAD_ALIGN	0
#endif

/* Number of slots needed for a 64-bit argument. */
#ifdef __LP64__
#define	QUAD_SLOTS	1
#else
#define	QUAD_SLOTS	2
#endif

/*
 * This should probably be in its own file, sorted alphabetically.
 */
static struct syscall decoded_syscalls[] = {
	/* Native ABI */
	{ .name = "__getcwd", .ret_type = 1, .nargs = 2,
	  .args = { { Name | OUT, 0 }, { Int, 1 } } },
	{ .name = "_umtx_op", .ret_type = 1, .nargs = 5,
	  .args = { { Ptr, 0 }, { Umtxop, 1 }, { LongHex, 2 }, { Ptr, 3 },
		    { Ptr, 4 } } },
	{ .name = "accept", .ret_type = 1, .nargs = 3,
	  .args = { { Int, 0 }, { Sockaddr | OUT, 1 }, { Ptr | OUT, 2 } } },
	{ .name = "access", .ret_type = 1, .nargs = 2,
	  .args = { { Name | IN, 0 }, { Accessmode, 1 } } },
	{ .name = "bind", .ret_type = 1, .nargs = 3,
	  .args = { { Int, 0 }, { Sockaddr | IN, 1 }, { Int, 2 } } },
	{ .name = "bindat", .ret_type = 1, .nargs = 4,
	  .args = { { Atfd, 0 }, { Int, 1 }, { Sockaddr | IN, 2 },
		    { Int, 3 } } },
	{ .name = "break", .ret_type = 1, .nargs = 1,
	  .args = { { Ptr, 0 } } },
	{ .name = "chdir", .ret_type = 1, .nargs = 1,
	  .args = { { Name, 0 } } },
	{ .name = "chflags", .ret_type = 1, .nargs = 2,
	  .args = { { Name | IN, 0 }, { Hex, 1 } } },
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
	{ .name = "connect", .ret_type = 1, .nargs = 3,
	  .args = { { Int, 0 }, { Sockaddr | IN, 1 }, { Int, 2 } } },
	{ .name = "connectat", .ret_type = 1, .nargs = 4,
	  .args = { { Atfd, 0 }, { Int, 1 }, { Sockaddr | IN, 2 },
		    { Int, 3 } } },
	{ .name = "eaccess", .ret_type = 1, .nargs = 2,
	  .args = { { Name | IN, 0 }, { Accessmode, 1 } } },
	{ .name = "execve", .ret_type = 1, .nargs = 3,
	  .args = { { Name | IN, 0 }, { ExecArgs | IN, 1 },
		    { ExecEnv | IN, 2 } } },
	{ .name = "exit", .ret_type = 0, .nargs = 1,
	  .args = { { Hex, 0 } } },
	{ .name = "faccessat", .ret_type = 1, .nargs = 4,
	  .args = { { Atfd, 0 }, { Name | IN, 1 }, { Accessmode, 2 },
		    { Atflags, 3 } } },
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
	{ .name = "fstat", .ret_type = 1, .nargs = 2,
	  .args = { { Int, 0 }, { Stat | OUT, 1 } } },
	{ .name = "fstatat", .ret_type = 1, .nargs = 4,
	  .args = { { Atfd, 0 }, { Name | IN, 1 }, { Stat | OUT, 2 },
		    { Atflags, 3 } } },
	{ .name = "fstatfs", .ret_type = 1, .nargs = 2,
	  .args = { { Int, 0 }, { StatFs | OUT, 1 } } },
	{ .name = "ftruncate", .ret_type = 1, .nargs = 2,
	  .args = { { Int | IN, 0 }, { QuadHex | IN, 1 + QUAD_ALIGN } } },
	{ .name = "futimens", .ret_type = 1, .nargs = 2,
	  .args = { { Int, 0 }, { Timespec2 | IN, 1 } } },
	{ .name = "futimes", .ret_type = 1, .nargs = 2,
	  .args = { { Int, 0 }, { Timeval2 | IN, 1 } } },
	{ .name = "futimesat", .ret_type = 1, .nargs = 3,
	  .args = { { Atfd, 0 }, { Name | IN, 1 }, { Timeval2 | IN, 2 } } },
	{ .name = "getitimer", .ret_type = 1, .nargs = 2,
	  .args = { { Int, 0 }, { Itimerval | OUT, 2 } } },
	{ .name = "getpeername", .ret_type = 1, .nargs = 3,
	  .args = { { Int, 0 }, { Sockaddr | OUT, 1 }, { Ptr | OUT, 2 } } },
	{ .name = "getpgid", .ret_type = 1, .nargs = 1,
	  .args = { { Int, 0 } } },
	{ .name = "getrlimit", .ret_type = 1, .nargs = 2,
	  .args = { { Resource, 0 }, { Rlimit | OUT, 1 } } },
	{ .name = "getrusage", .ret_type = 1, .nargs = 2,
	  .args = { { Int, 0 }, { Rusage | OUT, 1 } } },
	{ .name = "getsid", .ret_type = 1, .nargs = 1,
	  .args = { { Int, 0 } } },
	{ .name = "getsockname", .ret_type = 1, .nargs = 3,
	  .args = { { Int, 0 }, { Sockaddr | OUT, 1 }, { Ptr | OUT, 2 } } },
	{ .name = "gettimeofday", .ret_type = 1, .nargs = 2,
	  .args = { { Timeval | OUT, 0 }, { Ptr, 1 } } },
	{ .name = "ioctl", .ret_type = 1, .nargs = 3,
	  .args = { { Int, 0 }, { Ioctl, 1 }, { Hex, 2 } } },
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
	{ .name = "kldunload", .ret_type = 1, .nargs = 1,
	  .args = { { Int, 0 } } },
	{ .name = "kse_release", .ret_type = 0, .nargs = 1,
	  .args = { { Timespec, 0 } } },
	{ .name = "lchflags", .ret_type = 1, .nargs = 2,
	  .args = { { Name | IN, 0 }, { Hex, 1 } } },
	{ .name = "lchmod", .ret_type = 1, .nargs = 2,
	  .args = { { Name, 0 }, { Octal, 1 } } },
	{ .name = "lchown", .ret_type = 1, .nargs = 3,
	  .args = { { Name, 0 }, { Int, 1 }, { Int, 2 } } },
	{ .name = "link", .ret_type = 1, .nargs = 2,
	  .args = { { Name, 0 }, { Name, 1 } } },
	{ .name = "linkat", .ret_type = 1, .nargs = 5,
	  .args = { { Atfd, 0 }, { Name, 1 }, { Atfd, 2 }, { Name, 3 },
		    { Atflags, 4 } } },
	{ .name = "lseek", .ret_type = 2, .nargs = 3,
	  .args = { { Int, 0 }, { QuadHex, 1 + QUAD_ALIGN },
		    { Whence, 1 + QUAD_SLOTS + QUAD_ALIGN } } },
	{ .name = "lstat", .ret_type = 1, .nargs = 2,
	  .args = { { Name | IN, 0 }, { Stat | OUT, 1 } } },
	{ .name = "lutimes", .ret_type = 1, .nargs = 2,
	  .args = { { Name | IN, 0 }, { Timeval2 | IN, 1 } } },
	{ .name = "mkdir", .ret_type = 1, .nargs = 2,
	  .args = { { Name, 0 }, { Octal, 1 } } },
	{ .name = "mkdirat", .ret_type = 1, .nargs = 3,
	  .args = { { Atfd, 0 }, { Name, 1 }, { Octal, 2 } } },
	{ .name = "mkfifo", .ret_type = 1, .nargs = 2,
	  .args = { { Name, 0 }, { Octal, 1 } } },
	{ .name = "mkfifoat", .ret_type = 1, .nargs = 3,
	  .args = { { Atfd, 0 }, { Name, 1 }, { Octal, 2 } } },
	{ .name = "mknod", .ret_type = 1, .nargs = 3,
	  .args = { { Name, 0 }, { Octal, 1 }, { Int, 2 } } },
	{ .name = "mknodat", .ret_type = 1, .nargs = 4,
	  .args = { { Atfd, 0 }, { Name, 1 }, { Octal, 2 }, { Int, 3 } } },
	{ .name = "mmap", .ret_type = 1, .nargs = 6,
	  .args = { { Ptr, 0 }, { Int, 1 }, { Mprot, 2 }, { Mmapflags, 3 },
		    { Int, 4 }, { QuadHex, 5 + QUAD_ALIGN } } },
	{ .name = "modfind", .ret_type = 1, .nargs = 1,
	  .args = { { Name | IN, 0 } } },
	{ .name = "mount", .ret_type = 1, .nargs = 4,
	  .args = { { Name, 0 }, { Name, 1 }, { Int, 2 }, { Ptr, 3 } } },
	{ .name = "mprotect", .ret_type = 1, .nargs = 3,
	  .args = { { Ptr, 0 }, { Int, 1 }, { Mprot, 2 } } },
	{ .name = "munmap", .ret_type = 1, .nargs = 2,
	  .args = { { Ptr, 0 }, { Int, 1 } } },
	{ .name = "nanosleep", .ret_type = 1, .nargs = 1,
	  .args = { { Timespec, 0 } } },
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
	  .args = { { Ptr, 0 }, { Open, 1 } } },
	{ .name = "poll", .ret_type = 1, .nargs = 3,
	  .args = { { Pollfd, 0 }, { Int, 1 }, { Int, 2 } } },
	{ .name = "posix_openpt", .ret_type = 1, .nargs = 1,
	  .args = { { Open, 0 } } },
	{ .name = "procctl", .ret_type = 1, .nargs = 4,
	  .args = { { Idtype, 0 }, { Quad, 1 + QUAD_ALIGN },
		    { Procctl, 1 + QUAD_ALIGN + QUAD_SLOTS },
		    { Ptr, 2 + QUAD_ALIGN + QUAD_SLOTS } } },
	{ .name = "read", .ret_type = 1, .nargs = 3,
	  .args = { { Int, 0 }, { BinString | OUT, 1 }, { Int, 2 } } },
	{ .name = "readlink", .ret_type = 1, .nargs = 3,
	  .args = { { Name, 0 }, { Readlinkres | OUT, 1 }, { Int, 2 } } },
	{ .name = "readlinkat", .ret_type = 1, .nargs = 4,
	  .args = { { Atfd, 0 }, { Name, 1 }, { Readlinkres | OUT, 2 },
		    { Int, 3 } } },
	{ .name = "recvfrom", .ret_type = 1, .nargs = 6,
	  .args = { { Int, 0 }, { BinString | OUT, 1 }, { Int, 2 }, { Hex, 3 },
		    { Sockaddr | OUT, 4 }, { Ptr | OUT, 5 } } },
	{ .name = "rename", .ret_type = 1, .nargs = 2,
	  .args = { { Name, 0 }, { Name, 1 } } },
	{ .name = "renameat", .ret_type = 1, .nargs = 4,
	  .args = { { Atfd, 0 }, { Name, 1 }, { Atfd, 2 }, { Name, 3 } } },
	{ .name = "rfork", .ret_type = 1, .nargs = 1,
	  .args = { { Rforkflags, 0 } } },
	{ .name = "select", .ret_type = 1, .nargs = 5,
	  .args = { { Int, 0 }, { Fd_set, 1 }, { Fd_set, 2 }, { Fd_set, 3 },
		    { Timeval, 4 } } },
	{ .name = "sendto", .ret_type = 1, .nargs = 6,
	  .args = { { Int, 0 }, { BinString | IN, 1 }, { Int, 2 }, { Hex, 3 },
		    { Sockaddr | IN, 4 }, { Ptr | IN, 5 } } },
	{ .name = "setitimer", .ret_type = 1, .nargs = 3,
	  .args = { { Int, 0 }, { Itimerval, 1 }, { Itimerval | OUT, 2 } } },
	{ .name = "setrlimit", .ret_type = 1, .nargs = 2,
	  .args = { { Resource, 0 }, { Rlimit | IN, 1 } } },
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
	  .args = { { Sigset | IN, 0 }, { Ptr, 1 }, { Timespec | IN, 2 } } },
	{ .name = "sigwait", .ret_type = 1, .nargs = 2,
	  .args = { { Sigset | IN, 0 }, { Ptr, 1 } } },
	{ .name = "sigwaitinfo", .ret_type = 1, .nargs = 2,
	  .args = { { Sigset | IN, 0 }, { Ptr, 1 } } },
	{ .name = "socket", .ret_type = 1, .nargs = 3,
	  .args = { { Sockdomain, 0 }, { Socktype, 1 }, { Int, 2 } } },
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
	{ .name = "thr_kill", .ret_type = 1, .nargs = 2,
	  .args = { { Long, 0 }, { Signal, 1 } } },
	{ .name = "thr_self", .ret_type = 1, .nargs = 1,
	  .args = { { Ptr, 0 } } },
	{ .name = "truncate", .ret_type = 1, .nargs = 2,
	  .args = { { Name | IN, 0 }, { QuadHex | IN, 1 + QUAD_ALIGN } } },
#if 0
	/* Does not exist */
	{ .name = "umount", .ret_type = 1, .nargs = 2,
	  .args = { { Name, 0 }, { Int, 2 } } },
#endif
	{ .name = "unlink", .ret_type = 1, .nargs = 1,
	  .args = { { Name, 0 } } },
	{ .name = "unlinkat", .ret_type = 1, .nargs = 3,
	  .args = { { Atfd, 0 }, { Name, 1 }, { Atflags, 2 } } },
	{ .name = "unmount", .ret_type = 1, .nargs = 2,
	  .args = { { Name, 0 }, { Int, 1 } } },
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
	  .args = { { Idtype, 0 }, { Quad, 1 + QUAD_ALIGN },
		    { ExitStatus | OUT, 1 + QUAD_ALIGN + QUAD_SLOTS },
		    { Waitoptions, 2 + QUAD_ALIGN + QUAD_SLOTS },
		    { Rusage | OUT, 3 + QUAD_ALIGN + QUAD_SLOTS },
		    { Ptr, 4 + QUAD_ALIGN + QUAD_SLOTS } } },
	{ .name = "write", .ret_type = 1, .nargs = 3,
	  .args = { { Int, 0 }, { BinString | IN, 1 }, { Int, 2 } } },

	/* Linux ABI */
	{ .name = "linux_access", .ret_type = 1, .nargs = 2,
	  .args = { { Name, 0 }, { Accessmode, 1 } } },
	{ .name = "linux_execve", .ret_type = 1, .nargs = 3,
	  .args = { { Name | IN, 0 }, { ExecArgs | IN, 1 },
		    { ExecEnv | IN, 2 } } },
	{ .name = "linux_lseek", .ret_type = 2, .nargs = 3,
	  .args = { { Int, 0 }, { Int, 1 }, { Whence, 2 } } },
	{ .name = "linux_mkdir", .ret_type = 1, .nargs = 2,
	  .args = { { Name | IN, 0 }, { Int, 1 } } },
	{ .name = "linux_newfstat", .ret_type = 1, .nargs = 2,
	  .args = { { Int, 0 }, { Ptr | OUT, 1 } } },
	{ .name = "linux_newstat", .ret_type = 1, .nargs = 2,
	  .args = { { Name | IN, 0 }, { Ptr | OUT, 1 } } },
	{ .name = "linux_open", .ret_type = 1, .nargs = 3,
	  .args = { { Name, 0 }, { Hex, 1 }, { Octal, 2 } } },
	{ .name = "linux_readlink", .ret_type = 1, .nargs = 3,
	  .args = { { Name, 0 }, { Name | OUT, 1 }, { Int, 2 } } },
	{ .name = "linux_socketcall", .ret_type = 1, .nargs = 2,
	  .args = { { Int, 0 }, { LinuxSockArgs, 1 } } },
	{ .name = "linux_stat64", .ret_type = 1, .nargs = 3,
	  .args = { { Name | IN, 0 }, { Ptr | OUT, 1 }, { Ptr | IN, 1 } } },

	/* CloudABI system calls. */
	{ .name = "cloudabi_sys_clock_res_get", .ret_type = 1, .nargs = 1,
	  .args = { { CloudABIClockID, 0 } } },
	{ .name = "cloudabi_sys_clock_time_get", .ret_type = 1, .nargs = 2,
	  .args = { { CloudABIClockID, 0 }, { CloudABITimestamp, 1 } } },
	{ .name = "cloudabi_sys_condvar_signal", .ret_type = 1, .nargs = 3,
	  .args = { { Ptr, 0 }, { CloudABIMFlags, 1 }, { UInt, 2 } } },
	{ .name = "cloudabi_sys_fd_close", .ret_type = 1, .nargs = 1,
	  .args = { { Int, 0 } } },
	{ .name = "cloudabi_sys_fd_create1", .ret_type = 1, .nargs = 1,
	  .args = { { CloudABIFileType, 0 } } },
	{ .name = "cloudabi_sys_fd_create2", .ret_type = 1, .nargs = 2,
	  .args = { { CloudABIFileType, 0 }, { PipeFds | OUT, 0 } } },
	{ .name = "cloudabi_sys_fd_datasync", .ret_type = 1, .nargs = 1,
	  .args = { { Int, 0 } } },
	{ .name = "cloudabi_sys_fd_dup", .ret_type = 1, .nargs = 1,
	  .args = { { Int, 0 } } },
	{ .name = "cloudabi_sys_fd_replace", .ret_type = 1, .nargs = 2,
	  .args = { { Int, 0 }, { Int, 1 } } },
	{ .name = "cloudabi_sys_fd_seek", .ret_type = 1, .nargs = 3,
	  .args = { { Int, 0 }, { Int, 1 }, { CloudABIWhence, 2 } } },
	{ .name = "cloudabi_sys_fd_stat_get", .ret_type = 1, .nargs = 2,
	  .args = { { Int, 0 }, { CloudABIFDStat | OUT, 1 } } },
	{ .name = "cloudabi_sys_fd_stat_put", .ret_type = 1, .nargs = 3,
	  .args = { { Int, 0 }, { CloudABIFDStat | IN, 1 },
	            { ClouduABIFDSFlags, 2 } } },
	{ .name = "cloudabi_sys_fd_sync", .ret_type = 1, .nargs = 1,
	  .args = { { Int, 0 } } },
	{ .name = "cloudabi_sys_file_advise", .ret_type = 1, .nargs = 4,
	  .args = { { Int, 0 }, { Int, 1 }, { Int, 2 },
	            { CloudABIAdvice, 3 } } },
	{ .name = "cloudabi_sys_file_allocate", .ret_type = 1, .nargs = 3,
	  .args = { { Int, 0 }, { Int, 1 }, { Int, 2 } } },
	{ .name = "cloudabi_sys_file_create", .ret_type = 1, .nargs = 3,
	  .args = { { Int, 0 }, { BinString | IN, 1 },
	            { CloudABIFileType, 3 } } },
	{ .name = "cloudabi_sys_file_link", .ret_type = 1, .nargs = 4,
	  .args = { { CloudABILookup, 0 }, { BinString | IN, 1 },
	            { Int, 3 }, { BinString | IN, 4 } } },
	{ .name = "cloudabi_sys_file_open", .ret_type = 1, .nargs = 4,
	  .args = { { Int, 0 }, { BinString | IN, 1 },
	            { CloudABIOFlags, 3 }, { CloudABIFDStat | IN, 4 } } },
	{ .name = "cloudabi_sys_file_readdir", .ret_type = 1, .nargs = 4,
	  .args = { { Int, 0 }, { BinString | OUT, 1 }, { Int, 2 },
	            { Int, 3 } } },
	{ .name = "cloudabi_sys_file_readlink", .ret_type = 1, .nargs = 4,
	  .args = { { Int, 0 }, { BinString | IN, 1 },
	            { BinString | OUT, 3 }, { Int, 4 } } },
	{ .name = "cloudabi_sys_file_rename", .ret_type = 1, .nargs = 4,
	  .args = { { Int, 0 }, { BinString | IN, 1 },
	            { Int, 3 }, { BinString | IN, 4 } } },
	{ .name = "cloudabi_sys_file_stat_fget", .ret_type = 1, .nargs = 2,
	  .args = { { Int, 0 }, { CloudABIFileStat | OUT, 1 } } },
	{ .name = "cloudabi_sys_file_stat_fput", .ret_type = 1, .nargs = 3,
	  .args = { { Int, 0 }, { CloudABIFileStat | IN, 1 },
	            { CloudABIFSFlags, 2 } } },
	{ .name = "cloudabi_sys_file_stat_get", .ret_type = 1, .nargs = 3,
	  .args = { { CloudABILookup, 0 }, { BinString | IN, 1 },
	            { CloudABIFileStat | OUT, 3 } } },
	{ .name = "cloudabi_sys_file_stat_put", .ret_type = 1, .nargs = 4,
	  .args = { { CloudABILookup, 0 }, { BinString | IN, 1 },
	            { CloudABIFileStat | IN, 3 }, { CloudABIFSFlags, 4 } } },
	{ .name = "cloudabi_sys_file_symlink", .ret_type = 1, .nargs = 3,
	  .args = { { BinString | IN, 0 },
	            { Int, 2 }, { BinString | IN, 3 } } },
	{ .name = "cloudabi_sys_file_unlink", .ret_type = 1, .nargs = 3,
	  .args = { { Int, 0 }, { BinString | IN, 1 },
	            { CloudABIULFlags, 3 } } },
	{ .name = "cloudabi_sys_lock_unlock", .ret_type = 1, .nargs = 2,
	  .args = { { Ptr, 0 }, { CloudABIMFlags, 1 } } },
	{ .name = "cloudabi_sys_mem_advise", .ret_type = 1, .nargs = 3,
	  .args = { { Ptr, 0 }, { Int, 1 }, { CloudABIAdvice, 2 } } },
	{ .name = "cloudabi_sys_mem_lock", .ret_type = 1, .nargs = 2,
	  .args = { { Ptr, 0 }, { Int, 1 } } },
	{ .name = "cloudabi_sys_mem_map", .ret_type = 1, .nargs = 6,
	  .args = { { Ptr, 0 }, { Int, 1 }, { CloudABIMProt, 2 },
	            { CloudABIMFlags, 3 }, { Int, 4 }, { Int, 5 } } },
	{ .name = "cloudabi_sys_mem_protect", .ret_type = 1, .nargs = 3,
	  .args = { { Ptr, 0 }, { Int, 1 }, { CloudABIMProt, 2 } } },
	{ .name = "cloudabi_sys_mem_sync", .ret_type = 1, .nargs = 3,
	  .args = { { Ptr, 0 }, { Int, 1 }, { CloudABIMSFlags, 2 } } },
	{ .name = "cloudabi_sys_mem_unlock", .ret_type = 1, .nargs = 2,
	  .args = { { Ptr, 0 }, { Int, 1 } } },
	{ .name = "cloudabi_sys_mem_unmap", .ret_type = 1, .nargs = 2,
	  .args = { { Ptr, 0 }, { Int, 1 } } },
	{ .name = "cloudabi_sys_proc_exec", .ret_type = 1, .nargs = 5,
	  .args = { { Int, 0 }, { BinString | IN, 1 }, { Int, 2 },
	            { IntArray, 3 }, { Int, 4 } } },
	{ .name = "cloudabi_sys_proc_exit", .ret_type = 1, .nargs = 1,
	  .args = { { Int, 0 } } },
	{ .name = "cloudabi_sys_proc_fork", .ret_type = 1, .nargs = 0 },
	{ .name = "cloudabi_sys_proc_raise", .ret_type = 1, .nargs = 1,
	  .args = { { CloudABISignal, 0 } } },
	{ .name = "cloudabi_sys_random_get", .ret_type = 1, .nargs = 2,
	  .args = { { BinString | OUT, 0 }, { Int, 1 } } },
	{ .name = "cloudabi_sys_sock_accept", .ret_type = 1, .nargs = 2,
	  .args = { { Int, 0 }, { CloudABISockStat | OUT, 1 } } },
	{ .name = "cloudabi_sys_sock_bind", .ret_type = 1, .nargs = 3,
	  .args = { { Int, 0 }, { Int, 1 }, { BinString | IN, 2 } } },
	{ .name = "cloudabi_sys_sock_connect", .ret_type = 1, .nargs = 3,
	  .args = { { Int, 0 }, { Int, 1 }, { BinString | IN, 2 } } },
	{ .name = "cloudabi_sys_sock_listen", .ret_type = 1, .nargs = 2,
	  .args = { { Int, 0 }, { Int, 1 } } },
	{ .name = "cloudabi_sys_sock_shutdown", .ret_type = 1, .nargs = 2,
	  .args = { { Int, 0 }, { CloudABISDFlags, 1 } } },
	{ .name = "cloudabi_sys_sock_stat_get", .ret_type = 1, .nargs = 3,
	  .args = { { Int, 0 }, { CloudABISockStat | OUT, 1 },
	            { CloudABISSFlags, 2 } } },
	{ .name = "cloudabi_sys_thread_exit", .ret_type = 1, .nargs = 2,
	  .args = { { Ptr, 0 }, { CloudABIMFlags, 1 } } },
	{ .name = "cloudabi_sys_thread_tcb_set", .ret_type = 1, .nargs = 1,
	  .args = { { Ptr, 0 } } },
	{ .name = "cloudabi_sys_thread_yield", .ret_type = 1, .nargs = 0 },

	{ .name = 0 },
};
static STAILQ_HEAD(, syscall) syscalls;

/* Xlat idea taken from strace */
struct xlat {
	int val;
	const char *str;
};

#define	X(a)	{ a, #a },
#define	XEND	{ 0, NULL }

static struct xlat kevent_filters[] = {
	X(EVFILT_READ) X(EVFILT_WRITE) X(EVFILT_AIO) X(EVFILT_VNODE)
	X(EVFILT_PROC) X(EVFILT_SIGNAL) X(EVFILT_TIMER)
	X(EVFILT_PROCDESC) X(EVFILT_FS) X(EVFILT_LIO) X(EVFILT_USER)
	X(EVFILT_SENDFILE) XEND
};

static struct xlat kevent_flags[] = {
	X(EV_ADD) X(EV_DELETE) X(EV_ENABLE) X(EV_DISABLE) X(EV_ONESHOT)
	X(EV_CLEAR) X(EV_RECEIPT) X(EV_DISPATCH) X(EV_FORCEONESHOT)
	X(EV_DROP) X(EV_FLAG1) X(EV_ERROR) X(EV_EOF) XEND
};

static struct xlat kevent_user_ffctrl[] = {
	X(NOTE_FFNOP) X(NOTE_FFAND) X(NOTE_FFOR) X(NOTE_FFCOPY)
	XEND
};

static struct xlat kevent_rdwr_fflags[] = {
	X(NOTE_LOWAT) X(NOTE_FILE_POLL) XEND
};

static struct xlat kevent_vnode_fflags[] = {
	X(NOTE_DELETE) X(NOTE_WRITE) X(NOTE_EXTEND) X(NOTE_ATTRIB)
	X(NOTE_LINK) X(NOTE_RENAME) X(NOTE_REVOKE) XEND
};

static struct xlat kevent_proc_fflags[] = {
	X(NOTE_EXIT) X(NOTE_FORK) X(NOTE_EXEC) X(NOTE_TRACK) X(NOTE_TRACKERR)
	X(NOTE_CHILD) XEND
};

static struct xlat kevent_timer_fflags[] = {
	X(NOTE_SECONDS) X(NOTE_MSECONDS) X(NOTE_USECONDS) X(NOTE_NSECONDS)
	XEND
};

static struct xlat poll_flags[] = {
	X(POLLSTANDARD) X(POLLIN) X(POLLPRI) X(POLLOUT) X(POLLERR)
	X(POLLHUP) X(POLLNVAL) X(POLLRDNORM) X(POLLRDBAND)
	X(POLLWRBAND) X(POLLINIGNEOF) XEND
};

static struct xlat mmap_flags[] = {
	X(MAP_SHARED) X(MAP_PRIVATE) X(MAP_FIXED) X(MAP_RESERVED0020)
	X(MAP_RESERVED0040) X(MAP_RESERVED0080) X(MAP_RESERVED0100)
	X(MAP_HASSEMAPHORE) X(MAP_STACK) X(MAP_NOSYNC) X(MAP_ANON)
	X(MAP_EXCL) X(MAP_NOCORE) X(MAP_PREFAULT_READ)
#ifdef MAP_32BIT
	X(MAP_32BIT)
#endif
	XEND
};

static struct xlat mprot_flags[] = {
	X(PROT_NONE) X(PROT_READ) X(PROT_WRITE) X(PROT_EXEC) XEND
};

static struct xlat whence_arg[] = {
	X(SEEK_SET) X(SEEK_CUR) X(SEEK_END) X(SEEK_DATA) X(SEEK_HOLE) XEND
};

static struct xlat sigaction_flags[] = {
	X(SA_ONSTACK) X(SA_RESTART) X(SA_RESETHAND) X(SA_NOCLDSTOP)
	X(SA_NODEFER) X(SA_NOCLDWAIT) X(SA_SIGINFO) XEND
};

static struct xlat fcntl_arg[] = {
	X(F_DUPFD) X(F_GETFD) X(F_SETFD) X(F_GETFL) X(F_SETFL)
	X(F_GETOWN) X(F_SETOWN) X(F_OGETLK) X(F_OSETLK) X(F_OSETLKW)
	X(F_DUP2FD) X(F_GETLK) X(F_SETLK) X(F_SETLKW) X(F_SETLK_REMOTE)
	X(F_READAHEAD) X(F_RDAHEAD) X(F_DUPFD_CLOEXEC) X(F_DUP2FD_CLOEXEC)
	XEND
};

static struct xlat fcntlfd_arg[] = {
	X(FD_CLOEXEC) XEND
};

static struct xlat fcntlfl_arg[] = {
	X(O_APPEND) X(O_ASYNC) X(O_FSYNC) X(O_NONBLOCK) X(O_NOFOLLOW)
	X(FRDAHEAD) X(O_DIRECT) XEND
};

static struct xlat sockdomain_arg[] = {
	X(PF_UNSPEC) X(PF_LOCAL) X(PF_UNIX) X(PF_INET) X(PF_IMPLINK)
	X(PF_PUP) X(PF_CHAOS) X(PF_NETBIOS) X(PF_ISO) X(PF_OSI)
	X(PF_ECMA) X(PF_DATAKIT) X(PF_CCITT) X(PF_SNA) X(PF_DECnet)
	X(PF_DLI) X(PF_LAT) X(PF_HYLINK) X(PF_APPLETALK) X(PF_ROUTE)
	X(PF_LINK) X(PF_XTP) X(PF_COIP) X(PF_CNT) X(PF_SIP) X(PF_IPX)
	X(PF_RTIP) X(PF_PIP) X(PF_ISDN) X(PF_KEY) X(PF_INET6)
	X(PF_NATM) X(PF_ATM) X(PF_NETGRAPH) X(PF_SLOW) X(PF_SCLUSTER)
	X(PF_ARP) X(PF_BLUETOOTH) X(PF_IEEE80211) X(PF_INET_SDP)
	X(PF_INET6_SDP) XEND
};

static struct xlat socktype_arg[] = {
	X(SOCK_STREAM) X(SOCK_DGRAM) X(SOCK_RAW) X(SOCK_RDM)
	X(SOCK_SEQPACKET) XEND
};

static struct xlat open_flags[] = {
	X(O_RDONLY) X(O_WRONLY) X(O_RDWR) X(O_ACCMODE) X(O_NONBLOCK)
	X(O_APPEND) X(O_SHLOCK) X(O_EXLOCK) X(O_ASYNC) X(O_FSYNC)
	X(O_NOFOLLOW) X(O_CREAT) X(O_TRUNC) X(O_EXCL) X(O_NOCTTY)
	X(O_DIRECT) X(O_DIRECTORY) X(O_EXEC) X(O_TTY_INIT) X(O_CLOEXEC)
	X(O_VERIFY) XEND
};

static struct xlat shutdown_arg[] = {
	X(SHUT_RD) X(SHUT_WR) X(SHUT_RDWR) XEND
};

static struct xlat resource_arg[] = {
	X(RLIMIT_CPU) X(RLIMIT_FSIZE) X(RLIMIT_DATA) X(RLIMIT_STACK)
	X(RLIMIT_CORE) X(RLIMIT_RSS) X(RLIMIT_MEMLOCK) X(RLIMIT_NPROC)
	X(RLIMIT_NOFILE) X(RLIMIT_SBSIZE) X(RLIMIT_VMEM) X(RLIMIT_NPTS)
	X(RLIMIT_SWAP) X(RLIMIT_KQUEUES) XEND
};

static struct xlat pathconf_arg[] = {
	X(_PC_LINK_MAX)  X(_PC_MAX_CANON)  X(_PC_MAX_INPUT)
	X(_PC_NAME_MAX) X(_PC_PATH_MAX) X(_PC_PIPE_BUF)
	X(_PC_CHOWN_RESTRICTED) X(_PC_NO_TRUNC) X(_PC_VDISABLE)
	X(_PC_ASYNC_IO) X(_PC_PRIO_IO) X(_PC_SYNC_IO)
	X(_PC_ALLOC_SIZE_MIN) X(_PC_FILESIZEBITS)
	X(_PC_REC_INCR_XFER_SIZE) X(_PC_REC_MAX_XFER_SIZE)
	X(_PC_REC_MIN_XFER_SIZE) X(_PC_REC_XFER_ALIGN)
	X(_PC_SYMLINK_MAX) X(_PC_ACL_EXTENDED) X(_PC_ACL_PATH_MAX)
	X(_PC_CAP_PRESENT) X(_PC_INF_PRESENT) X(_PC_MAC_PRESENT)
	X(_PC_ACL_NFS4) X(_PC_MIN_HOLE_SIZE) XEND
};

static struct xlat rfork_flags[] = {
	X(RFFDG) X(RFPROC) X(RFMEM) X(RFNOWAIT) X(RFCFDG) X(RFTHREAD)
	X(RFSIGSHARE) X(RFLINUXTHPN) X(RFTSIGZMB) X(RFPPWAIT) XEND
};

static struct xlat wait_options[] = {
	X(WNOHANG) X(WUNTRACED) X(WCONTINUED) X(WNOWAIT) X(WEXITED)
	X(WTRAPPED) XEND
};

static struct xlat idtype_arg[] = {
	X(P_PID) X(P_PPID) X(P_PGID) X(P_SID) X(P_CID) X(P_UID) X(P_GID)
	X(P_ALL) X(P_LWPID) X(P_TASKID) X(P_PROJID) X(P_POOLID) X(P_JAILID)
	X(P_CTID) X(P_CPUID) X(P_PSETID) XEND
};

static struct xlat procctl_arg[] = {
	X(PROC_SPROTECT) X(PROC_REAP_ACQUIRE) X(PROC_REAP_RELEASE)
	X(PROC_REAP_STATUS) X(PROC_REAP_GETPIDS) X(PROC_REAP_KILL)
	X(PROC_TRACE_CTL) X(PROC_TRACE_STATUS) XEND
};

static struct xlat umtx_ops[] = {
	X(UMTX_OP_RESERVED0) X(UMTX_OP_RESERVED1) X(UMTX_OP_WAIT)
	X(UMTX_OP_WAKE) X(UMTX_OP_MUTEX_TRYLOCK) X(UMTX_OP_MUTEX_LOCK)
	X(UMTX_OP_MUTEX_UNLOCK) X(UMTX_OP_SET_CEILING) X(UMTX_OP_CV_WAIT)
	X(UMTX_OP_CV_SIGNAL) X(UMTX_OP_CV_BROADCAST) X(UMTX_OP_WAIT_UINT)
	X(UMTX_OP_RW_RDLOCK) X(UMTX_OP_RW_WRLOCK) X(UMTX_OP_RW_UNLOCK)
	X(UMTX_OP_WAIT_UINT_PRIVATE) X(UMTX_OP_WAKE_PRIVATE)
	X(UMTX_OP_MUTEX_WAIT) X(UMTX_OP_MUTEX_WAKE) X(UMTX_OP_SEM_WAIT)
	X(UMTX_OP_SEM_WAKE) X(UMTX_OP_NWAKE_PRIVATE) X(UMTX_OP_MUTEX_WAKE2)
	X(UMTX_OP_SEM2_WAIT) X(UMTX_OP_SEM2_WAKE)
	XEND
};

static struct xlat at_flags[] = {
	X(AT_EACCESS) X(AT_SYMLINK_NOFOLLOW) X(AT_SYMLINK_FOLLOW)
	X(AT_REMOVEDIR) XEND
};

static struct xlat access_modes[] = {
	X(R_OK) X(W_OK) X(X_OK) XEND
};

static struct xlat sysarch_ops[] = {
#if defined(__i386__) || defined(__amd64__)
	X(I386_GET_LDT) X(I386_SET_LDT) X(I386_GET_IOPERM) X(I386_SET_IOPERM)
	X(I386_VM86) X(I386_GET_FSBASE) X(I386_SET_FSBASE) X(I386_GET_GSBASE)
	X(I386_SET_GSBASE) X(I386_GET_XFPUSTATE) X(AMD64_GET_FSBASE)
	X(AMD64_SET_FSBASE) X(AMD64_GET_GSBASE) X(AMD64_SET_GSBASE)
	X(AMD64_GET_XFPUSTATE)
#endif
	XEND
};

static struct xlat linux_socketcall_ops[] = {
	X(LINUX_SOCKET) X(LINUX_BIND) X(LINUX_CONNECT) X(LINUX_LISTEN)
	X(LINUX_ACCEPT) X(LINUX_GETSOCKNAME) X(LINUX_GETPEERNAME)
	X(LINUX_SOCKETPAIR) X(LINUX_SEND) X(LINUX_RECV) X(LINUX_SENDTO)
	X(LINUX_RECVFROM) X(LINUX_SHUTDOWN) X(LINUX_SETSOCKOPT)
	X(LINUX_GETSOCKOPT) X(LINUX_SENDMSG) X(LINUX_RECVMSG)
	XEND
};

static struct xlat sigprocmask_ops[] = {
	X(SIG_BLOCK) X(SIG_UNBLOCK) X(SIG_SETMASK)
	XEND
};

#undef X
#define	X(a)	{ CLOUDABI_##a, #a },

static struct xlat cloudabi_advice[] = {
	X(ADVICE_DONTNEED) X(ADVICE_NOREUSE) X(ADVICE_NORMAL)
	X(ADVICE_RANDOM) X(ADVICE_SEQUENTIAL) X(ADVICE_WILLNEED)
	XEND
};

static struct xlat cloudabi_clockid[] = {
	X(CLOCK_MONOTONIC) X(CLOCK_PROCESS_CPUTIME_ID)
	X(CLOCK_REALTIME) X(CLOCK_THREAD_CPUTIME_ID)
	XEND
};

static struct xlat cloudabi_errno[] = {
	X(E2BIG) X(EACCES) X(EADDRINUSE) X(EADDRNOTAVAIL)
	X(EAFNOSUPPORT) X(EAGAIN) X(EALREADY) X(EBADF) X(EBADMSG)
	X(EBUSY) X(ECANCELED) X(ECHILD) X(ECONNABORTED) X(ECONNREFUSED)
	X(ECONNRESET) X(EDEADLK) X(EDESTADDRREQ) X(EDOM) X(EDQUOT)
	X(EEXIST) X(EFAULT) X(EFBIG) X(EHOSTUNREACH) X(EIDRM) X(EILSEQ)
	X(EINPROGRESS) X(EINTR) X(EINVAL) X(EIO) X(EISCONN) X(EISDIR)
	X(ELOOP) X(EMFILE) X(EMLINK) X(EMSGSIZE) X(EMULTIHOP)
	X(ENAMETOOLONG) X(ENETDOWN) X(ENETRESET) X(ENETUNREACH)
	X(ENFILE) X(ENOBUFS) X(ENODEV) X(ENOENT) X(ENOEXEC) X(ENOLCK)
	X(ENOLINK) X(ENOMEM) X(ENOMSG) X(ENOPROTOOPT) X(ENOSPC)
	X(ENOSYS) X(ENOTCONN) X(ENOTDIR) X(ENOTEMPTY) X(ENOTRECOVERABLE)
	X(ENOTSOCK) X(ENOTSUP) X(ENOTTY) X(ENXIO) X(EOVERFLOW)
	X(EOWNERDEAD) X(EPERM) X(EPIPE) X(EPROTO) X(EPROTONOSUPPORT)
	X(EPROTOTYPE) X(ERANGE) X(EROFS) X(ESPIPE) X(ESRCH) X(ESTALE)
	X(ETIMEDOUT) X(ETXTBSY) X(EXDEV) X(ENOTCAPABLE)
	XEND
};

static struct xlat cloudabi_fdflags[] = {
	X(FDFLAG_APPEND) X(FDFLAG_DSYNC) X(FDFLAG_NONBLOCK)
	X(FDFLAG_RSYNC) X(FDFLAG_SYNC)
	XEND
};

static struct xlat cloudabi_fdsflags[] = {
	X(FDSTAT_FLAGS) X(FDSTAT_RIGHTS)
	XEND
};

static struct xlat cloudabi_filetype[] = {
	X(FILETYPE_UNKNOWN) X(FILETYPE_BLOCK_DEVICE)
	X(FILETYPE_CHARACTER_DEVICE) X(FILETYPE_DIRECTORY)
	X(FILETYPE_FIFO) X(FILETYPE_POLL) X(FILETYPE_PROCESS)
	X(FILETYPE_REGULAR_FILE) X(FILETYPE_SHARED_MEMORY)
	X(FILETYPE_SOCKET_DGRAM) X(FILETYPE_SOCKET_SEQPACKET)
	X(FILETYPE_SOCKET_STREAM) X(FILETYPE_SYMBOLIC_LINK)
	XEND
};

static struct xlat cloudabi_fsflags[] = {
	X(FILESTAT_ATIM) X(FILESTAT_ATIM_NOW) X(FILESTAT_MTIM)
	X(FILESTAT_MTIM_NOW) X(FILESTAT_SIZE)
	XEND
};

static struct xlat cloudabi_mflags[] = {
	X(MAP_ANON) X(MAP_FIXED) X(MAP_PRIVATE) X(MAP_SHARED)
	XEND
};

static struct xlat cloudabi_mprot[] = {
	X(PROT_EXEC) X(PROT_WRITE) X(PROT_READ)
	XEND
};

static struct xlat cloudabi_msflags[] = {
	X(MS_ASYNC) X(MS_INVALIDATE) X(MS_SYNC)
	XEND
};

static struct xlat cloudabi_oflags[] = {
	X(O_CREAT) X(O_DIRECTORY) X(O_EXCL) X(O_TRUNC)
	XEND
};

static struct xlat cloudabi_sa_family[] = {
	X(AF_UNSPEC) X(AF_INET) X(AF_INET6) X(AF_UNIX)
	XEND
};

static struct xlat cloudabi_sdflags[] = {
	X(SHUT_RD) X(SHUT_WR)
	XEND
};

static struct xlat cloudabi_signal[] = {
	X(SIGABRT) X(SIGALRM) X(SIGBUS) X(SIGCHLD) X(SIGCONT) X(SIGFPE)
	X(SIGHUP) X(SIGILL) X(SIGINT) X(SIGKILL) X(SIGPIPE) X(SIGQUIT)
	X(SIGSEGV) X(SIGSTOP) X(SIGSYS) X(SIGTERM) X(SIGTRAP) X(SIGTSTP)
	X(SIGTTIN) X(SIGTTOU) X(SIGURG) X(SIGUSR1) X(SIGUSR2)
	X(SIGVTALRM) X(SIGXCPU) X(SIGXFSZ)
	XEND
};

static struct xlat cloudabi_ssflags[] = {
	X(SOCKSTAT_CLEAR_ERROR)
	XEND
};

static struct xlat cloudabi_ssstate[] = {
	X(SOCKSTATE_ACCEPTCONN)
	XEND
};

static struct xlat cloudabi_ulflags[] = {
	X(UNLINK_REMOVEDIR)
	XEND
};

static struct xlat cloudabi_whence[] = {
	X(WHENCE_CUR) X(WHENCE_END) X(WHENCE_SET)
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
			errx(1,"Unknown lookup base");
			break;
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

void
init_syscalls(void)
{
	struct syscall *sc;

	STAILQ_INIT(&syscalls);
	for (sc = decoded_syscalls; sc->name != NULL; sc++)
		STAILQ_INSERT_HEAD(&syscalls, sc, entries);
}
/*
 * If/when the list gets big, it might be desirable to do it
 * as a hash table or binary search.
 */
struct syscall *
get_syscall(const char *name, int nargs)
{
	struct syscall *sc;
	int i;

	if (name == NULL)
		return (NULL);
	STAILQ_FOREACH(sc, &syscalls, entries)
		if (strcmp(name, sc->name) == 0)
			return (sc);

	/* It is unknown.  Add it into the list. */
#if DEBUG
	fprintf(stderr, "unknown syscall %s -- setting args to %d\n", name,
	    nargs);
#endif

	sc = calloc(1, sizeof(struct syscall));
	sc->name = strdup(name);
	sc->ret_type = 1;
	sc->nargs = nargs;
	for (i = 0; i < nargs; i++) {
		sc->args[i].offset = i;
		/* Treat all unknown arguments as LongHex. */
		sc->args[i].type = LongHex;
	}
	STAILQ_INSERT_HEAD(&syscalls, sc, entries);

	return (sc);
}

/*
 * Copy a fixed amount of bytes from the process.
 */
static int
get_struct(pid_t pid, void *offset, void *buf, int len)
{
	struct ptrace_io_desc iorequest;

	iorequest.piod_op = PIOD_READ_D;
	iorequest.piod_offs = offset;
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
get_string(pid_t pid, void *addr, int max)
{
	struct ptrace_io_desc iorequest;
	char *buf, *nbuf;
	size_t offset, size, totalsize;

	offset = 0;
	if (max)
		size = max + 1;
	else {
		/* Read up to the end of the current page. */
		size = PAGE_SIZE - ((uintptr_t)addr % PAGE_SIZE);
		if (size > MAXSIZE)
			size = MAXSIZE;
	}
	totalsize = size;
	buf = malloc(totalsize);
	if (buf == NULL)
		return (NULL);
	for (;;) {
		iorequest.piod_op = PIOD_READ_D;
		iorequest.piod_offs = (char *)addr + offset;
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

static char *
strsig2(int sig)
{
	static char tmp[sizeof(int) * 3 + 1];
	char *ret;

	ret = strsig(sig);
	if (ret == NULL) {
		snprintf(tmp, sizeof(tmp), "%d", sig);
		ret = tmp;
	}
	return (ret);
}

static void
print_kevent(FILE *fp, struct kevent *ke, int input)
{

	switch (ke->filter) {
	case EVFILT_READ:
	case EVFILT_WRITE:
	case EVFILT_VNODE:
	case EVFILT_PROC:
	case EVFILT_TIMER:
	case EVFILT_PROCDESC:
		fprintf(fp, "%ju", (uintmax_t)ke->ident);
		break;
	case EVFILT_SIGNAL:
		fputs(strsig2(ke->ident), fp);
		break;
	default:
		fprintf(fp, "%p", (void *)ke->ident);
	}
	fprintf(fp, ",%s,%s,", xlookup(kevent_filters, ke->filter),
	    xlookup_bits(kevent_flags, ke->flags));
	switch (ke->filter) {
	case EVFILT_READ:
	case EVFILT_WRITE:
		fputs(xlookup_bits(kevent_rdwr_fflags, ke->fflags), fp);
		break;
	case EVFILT_VNODE:
		fputs(xlookup_bits(kevent_vnode_fflags, ke->fflags), fp);
		break;
	case EVFILT_PROC:
	case EVFILT_PROCDESC:
		fputs(xlookup_bits(kevent_proc_fflags, ke->fflags), fp);
		break;
	case EVFILT_TIMER:
		fputs(xlookup_bits(kevent_timer_fflags, ke->fflags), fp);
		break;
	case EVFILT_USER: {
		int ctrl, data;

		ctrl = ke->fflags & NOTE_FFCTRLMASK;
		data = ke->fflags & NOTE_FFLAGSMASK;
		if (input) {
			fputs(xlookup(kevent_user_ffctrl, ctrl), fp);
			if (ke->fflags & NOTE_TRIGGER)
				fputs("|NOTE_TRIGGER", fp);
			if (data != 0)
				fprintf(fp, "|%#x", data);
		} else {
			fprintf(fp, "%#x", data);
		}
		break;
	}
	default:
		fprintf(fp, "%#x", ke->fflags);
	}
	fprintf(fp, ",%p,%p", (void *)ke->data, (void *)ke->udata);
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

/*
 * Converts a syscall argument into a string.  Said string is
 * allocated via malloc(), so needs to be free()'d.  sc is
 * a pointer to the syscall description (see above); args is
 * an array of all of the system call arguments.
 */
char *
print_arg(struct syscall_args *sc, unsigned long *args, long *retval,
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
	case LongHex:
		fprintf(fp, "0x%lx", args[sc->offset]);
		break;
	case Long:
		fprintf(fp, "%ld", args[sc->offset]);
		break;
	case Name: {
		/* NULL-terminated string. */
		char *tmp2;

		tmp2 = get_string(pid, (void*)args[sc->offset], 0);
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
		if (len && get_struct(pid, (void*)args[sc->offset], &tmp2, len)
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
			fprintf(fp, "0x%lx", args[sc->offset]);
		}
		break;
	}
	case ExecArgs:
	case ExecEnv:
	case StringArray: {
		uintptr_t addr;
		union {
			char *strarray[0];
			char buf[PAGE_SIZE];
		} u;
		char *string;
		size_t len;
		u_int first, i;

		/*
		 * Only parse argv[] and environment arrays from exec calls
		 * if requested.
		 */
		if (((sc->type & ARG_MASK) == ExecArgs &&
		    (trussinfo->flags & EXECVEARGS) == 0) ||
		    ((sc->type & ARG_MASK) == ExecEnv &&
		    (trussinfo->flags & EXECVEENVS) == 0)) {
			fprintf(fp, "0x%lx", args[sc->offset]);
			break;
		}

		/*
		 * Read a page of pointers at a time.  Punt if the top-level
		 * pointer is not aligned.  Note that the first read is of
		 * a partial page.
		 */
		addr = args[sc->offset];
		if (addr % sizeof(char *) != 0) {
			fprintf(fp, "0x%lx", args[sc->offset]);
			break;
		}

		len = PAGE_SIZE - (addr & PAGE_MASK);
		if (get_struct(pid, (void *)addr, u.buf, len) == -1) {
			fprintf(fp, "0x%lx", args[sc->offset]);
			break;
		}

		fputc('[', fp);
		first = 1;
		i = 0;
		while (u.strarray[i] != NULL) {
			string = get_string(pid, u.strarray[i], 0);
			fprintf(fp, "%s \"%s\"", first ? "" : ",", string);
			free(string);
			first = 0;

			i++;
			if (i == len / sizeof(char *)) {
				addr += len;
				len = PAGE_SIZE;
				if (get_struct(pid, (void *)addr, u.buf, len) ==
				    -1) {
					fprintf(fp, ", <inval>");
					break;
				}
				i = 0;
			}
		}
		fputs(" ]", fp);
		break;
	}
#ifdef __LP64__
	case Quad:
		fprintf(fp, "%ld", args[sc->offset]);
		break;
	case QuadHex:
		fprintf(fp, "0x%lx", args[sc->offset]);
		break;
#else
	case Quad:
	case QuadHex: {
		unsigned long long ll;

#if _BYTE_ORDER == _LITTLE_ENDIAN
		ll = (unsigned long long)args[sc->offset + 1] << 32 |
		    args[sc->offset];
#else
		ll = (unsigned long long)args[sc->offset] << 32 |
		    args[sc->offset + 1];
#endif
		if ((sc->type & ARG_MASK) == Quad)
			fprintf(fp, "%lld", ll);
		else
			fprintf(fp, "0x%llx", ll);
		break;
	}
#endif
	case Ptr:
		fprintf(fp, "0x%lx", args[sc->offset]);
		break;
	case Readlinkres: {
		char *tmp2;

		if (retval[0] == -1)
			break;
		tmp2 = get_string(pid, (void*)args[sc->offset], retval[0]);
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

		if (get_struct(pid, (void *)args[sc->offset], &ts,
		    sizeof(ts)) != -1)
			fprintf(fp, "{ %jd.%09ld }", (intmax_t)ts.tv_sec,
			    ts.tv_nsec);
		else
			fprintf(fp, "0x%lx", args[sc->offset]);
		break;
	}
	case Timespec2: {
		struct timespec ts[2];
		const char *sep;
		unsigned int i;

		if (get_struct(pid, (void *)args[sc->offset], &ts, sizeof(ts))
		    != -1) {
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
			fprintf(fp, "0x%lx", args[sc->offset]);
		break;
	}
	case Timeval: {
		struct timeval tv;

		if (get_struct(pid, (void *)args[sc->offset], &tv, sizeof(tv))
		    != -1)
			fprintf(fp, "{ %jd.%06ld }", (intmax_t)tv.tv_sec,
			    tv.tv_usec);
		else
			fprintf(fp, "0x%lx", args[sc->offset]);
		break;
	}
	case Timeval2: {
		struct timeval tv[2];

		if (get_struct(pid, (void *)args[sc->offset], &tv, sizeof(tv))
		    != -1)
			fprintf(fp, "{ %jd.%06ld, %jd.%06ld }",
			    (intmax_t)tv[0].tv_sec, tv[0].tv_usec,
			    (intmax_t)tv[1].tv_sec, tv[1].tv_usec);
		else
			fprintf(fp, "0x%lx", args[sc->offset]);
		break;
	}
	case Itimerval: {
		struct itimerval itv;

		if (get_struct(pid, (void *)args[sc->offset], &itv,
		    sizeof(itv)) != -1)
			fprintf(fp, "{ %jd.%06ld, %jd.%06ld }",
			    (intmax_t)itv.it_interval.tv_sec,
			    itv.it_interval.tv_usec,
			    (intmax_t)itv.it_value.tv_sec,
			    itv.it_value.tv_usec);
		else
			fprintf(fp, "0x%lx", args[sc->offset]);
		break;
	}
	case LinuxSockArgs:
	{
		struct linux_socketcall_args largs;

		if (get_struct(pid, (void *)args[sc->offset], (void *)&largs,
		    sizeof(largs)) != -1)
			fprintf(fp, "{ %s, 0x%lx }",
			    lookup(linux_socketcall_ops, largs.what, 10),
			    (long unsigned int)largs.args);
		else
			fprintf(fp, "0x%lx", args[sc->offset]);
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
		if (get_struct(pid, (void *)args[sc->offset], pfd, bytes)
		    != -1) {
			fputs("{", fp);
			for (i = 0; i < numfds; i++) {
				fprintf(fp, " %d/%s", pfd[i].fd,
				    xlookup_bits(poll_flags, pfd[i].events));
			}
			fputs(" }", fp);
		} else {
			fprintf(fp, "0x%lx", args[sc->offset]);
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
		if (get_struct(pid, (void *)args[sc->offset], fds, bytes)
		    != -1) {
			fputs("{", fp);
			for (i = 0; i < numfds; i++) {
				if (FD_ISSET(i, fds))
					fprintf(fp, " %d", i);
			}
			fputs(" }", fp);
		} else
			fprintf(fp, "0x%lx", args[sc->offset]);
		free(fds);
		break;
	}
	case Signal:
		fputs(strsig2(args[sc->offset]), fp);
		break;
	case Sigset: {
		long sig;
		sigset_t ss;
		int i, first;

		sig = args[sc->offset];
		if (get_struct(pid, (void *)args[sc->offset], (void *)&ss,
		    sizeof(ss)) == -1) {
			fprintf(fp, "0x%lx", args[sc->offset]);
			break;
		}
		fputs("{ ", fp);
		first = 1;
		for (i = 1; i < sys_nsig; i++) {
			if (sigismember(&ss, i)) {
				fprintf(fp, "%s%s", !first ? "|" : "",
				    strsig(i));
				first = 0;
			}
		}
		if (!first)
			fputc(' ', fp);
		fputc('}', fp);
		break;
	}
	case Sigprocmask: {
		fputs(xlookup(sigprocmask_ops, args[sc->offset]), fp);
		break;
	}
	case Fcntlflag: {
		/* XXX: Output depends on the value of the previous argument. */
		switch (args[sc->offset - 1]) {
		case F_SETFD:
			fputs(xlookup_bits(fcntlfd_arg, args[sc->offset]), fp);
			break;
		case F_SETFL:
			fputs(xlookup_bits(fcntlfl_arg, args[sc->offset]), fp);
			break;
		case F_GETFD:
		case F_GETFL:
		case F_GETOWN:
			break;
		default:
			fprintf(fp, "0x%lx", args[sc->offset]);
			break;
		}
		break;
	}
	case Open:
		fputs(xlookup_bits(open_flags, args[sc->offset]), fp);
		break;
	case Fcntl:
		fputs(xlookup(fcntl_arg, args[sc->offset]), fp);
		break;
	case Mprot:
		fputs(xlookup_bits(mprot_flags, args[sc->offset]), fp);
		break;
	case Mmapflags: {
		int align, flags;

		/*
		 * MAP_ALIGNED can't be handled by xlookup_bits(), so
		 * generate that string manually and prepend it to the
		 * string from xlookup_bits().  Have to be careful to
		 * avoid outputting MAP_ALIGNED|0 if MAP_ALIGNED is
		 * the only flag.
		 */
		flags = args[sc->offset] & ~MAP_ALIGNMENT_MASK;
		align = args[sc->offset] & MAP_ALIGNMENT_MASK;
		if (align != 0) {
			if (align == MAP_ALIGNED_SUPER)
				fputs("MAP_ALIGNED_SUPER", fp);
			else
				fprintf(fp, "MAP_ALIGNED(%d)",
				    align >> MAP_ALIGNMENT_SHIFT);
			if (flags == 0)
				break;
			fputc('|', fp);
		}
		fputs(xlookup_bits(mmap_flags, flags), fp);
		break;
	}
	case Whence:
		fputs(xlookup(whence_arg, args[sc->offset]), fp);
		break;
	case Sockdomain:
		fputs(xlookup(sockdomain_arg, args[sc->offset]), fp);
		break;
	case Socktype: {
		int type, flags;

		flags = args[sc->offset] & (SOCK_CLOEXEC | SOCK_NONBLOCK);
		type = args[sc->offset] & ~flags;
		fputs(xlookup(socktype_arg, type), fp);
		if (flags & SOCK_CLOEXEC)
			fprintf(fp, "|SOCK_CLOEXEC");
		if (flags & SOCK_NONBLOCK)
			fprintf(fp, "|SOCK_NONBLOCK");
		break;
	}
	case Shutdown:
		fputs(xlookup(shutdown_arg, args[sc->offset]), fp);
		break;
	case Resource:
		fputs(xlookup(resource_arg, args[sc->offset]), fp);
		break;
	case Pathconf:
		fputs(xlookup(pathconf_arg, args[sc->offset]), fp);
		break;
	case Rforkflags:
		fputs(xlookup_bits(rfork_flags, args[sc->offset]), fp);
		break;
	case Sockaddr: {
		char addr[64];
		struct sockaddr_in *lsin;
		struct sockaddr_in6 *lsin6;
		struct sockaddr_un *sun;
		struct sockaddr *sa;
		socklen_t len;
		u_char *q;

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
			if (get_struct(pid, (void *)args[sc->offset + 1],
			    &len, sizeof(len)) == -1) {
				fprintf(fp, "0x%lx", args[sc->offset]);
				break;
			}
		} else
			len = args[sc->offset + 1];

		/* If the length is too small, just bail. */
		if (len < sizeof(*sa)) {
			fprintf(fp, "0x%lx", args[sc->offset]);
			break;
		}

		sa = calloc(1, len);
		if (get_struct(pid, (void *)args[sc->offset], sa, len) == -1) {
			free(sa);
			fprintf(fp, "0x%lx", args[sc->offset]);
			break;
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
		break;
	}
	case Sigaction: {
		struct sigaction sa;

		if (get_struct(pid, (void *)args[sc->offset], &sa, sizeof(sa))
		    != -1) {
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
			fprintf(fp, "0x%lx", args[sc->offset]);
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
		if (numevents >= 0 && get_struct(pid, (void *)args[sc->offset],
		    ke, bytes) != -1) {
			fputc('{', fp);
			for (i = 0; i < numevents; i++) {
				fputc(' ', fp);
				print_kevent(fp, &ke[i], sc->offset == 1);
			}
			fputs(" }", fp);
		} else {
			fprintf(fp, "0x%lx", args[sc->offset]);
		}
		free(ke);
		break;
	}
	case Stat: {
		struct stat st;

		if (get_struct(pid, (void *)args[sc->offset], &st, sizeof(st))
		    != -1) {
			char mode[12];

			strmode(st.st_mode, mode);
			fprintf(fp,
			    "{ mode=%s,inode=%ju,size=%jd,blksize=%ld }", mode,
			    (uintmax_t)st.st_ino, (intmax_t)st.st_size,
			    (long)st.st_blksize);
		} else {
			fprintf(fp, "0x%lx", args[sc->offset]);
		}
		break;
	}
	case StatFs: {
		unsigned int i;
		struct statfs buf;

		if (get_struct(pid, (void *)args[sc->offset], &buf,
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
			fprintf(fp, "0x%lx", args[sc->offset]);
		break;
	}

	case Rusage: {
		struct rusage ru;

		if (get_struct(pid, (void *)args[sc->offset], &ru, sizeof(ru))
		    != -1) {
			fprintf(fp,
			    "{ u=%jd.%06ld,s=%jd.%06ld,in=%ld,out=%ld }",
			    (intmax_t)ru.ru_utime.tv_sec, ru.ru_utime.tv_usec,
			    (intmax_t)ru.ru_stime.tv_sec, ru.ru_stime.tv_usec,
			    ru.ru_inblock, ru.ru_oublock);
		} else
			fprintf(fp, "0x%lx", args[sc->offset]);
		break;
	}
	case Rlimit: {
		struct rlimit rl;

		if (get_struct(pid, (void *)args[sc->offset], &rl, sizeof(rl))
		    != -1) {
			fprintf(fp, "{ cur=%ju,max=%ju }",
			    rl.rlim_cur, rl.rlim_max);
		} else
			fprintf(fp, "0x%lx", args[sc->offset]);
		break;
	}
	case ExitStatus: {
		int status;

		if (get_struct(pid, (void *)args[sc->offset], &status,
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
			fprintf(fp, "0x%lx", args[sc->offset]);
		break;
	}
	case Waitoptions:
		fputs(xlookup_bits(wait_options, args[sc->offset]), fp);
		break;
	case Idtype:
		fputs(xlookup(idtype_arg, args[sc->offset]), fp);
		break;
	case Procctl:
		fputs(xlookup(procctl_arg, args[sc->offset]), fp);
		break;
	case Umtxop:
		fputs(xlookup(umtx_ops, args[sc->offset]), fp);
		break;
	case Atfd:
		if ((int)args[sc->offset] == AT_FDCWD)
			fputs("AT_FDCWD", fp);
		else
			fprintf(fp, "%d", (int)args[sc->offset]);
		break;
	case Atflags:
		fputs(xlookup_bits(at_flags, args[sc->offset]), fp);
		break;
	case Accessmode:
		if (args[sc->offset] == F_OK)
			fputs("F_OK", fp);
		else
			fputs(xlookup_bits(access_modes, args[sc->offset]), fp);
		break;
	case Sysarch:
		fputs(xlookup(sysarch_ops, args[sc->offset]), fp);
		break;
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
		fprintf(fp, "{ %ld, %ld }", retval[0], retval[1]);
		retval[0] = 0;
		break;
	case Utrace: {
		size_t len;
		void *utrace_addr;

		len = args[sc->offset + 1];
		utrace_addr = calloc(1, len);
		if (get_struct(pid, (void *)args[sc->offset],
		    (void *)utrace_addr, len) != -1)
			print_utrace(fp, utrace_addr, len);
		else
			fprintf(fp, "0x%lx", args[sc->offset]);
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
		if (get_struct(pid, (void *)args[sc->offset],
		    descriptors, ndescriptors * sizeof(descriptors[0])) != -1) {
			fprintf(fp, "{");
			for (i = 0; i < ndescriptors; i++)
				fprintf(fp, i == 0 ? " %d" : ", %d",
				    descriptors[i]);
			fprintf(fp, truncated ? ", ... }" : " }");
		} else
			fprintf(fp, "0x%lx", args[sc->offset]);
		break;
	}

	case CloudABIAdvice:
		fputs(xlookup(cloudabi_advice, args[sc->offset]), fp);
		break;
	case CloudABIClockID:
		fputs(xlookup(cloudabi_clockid, args[sc->offset]), fp);
		break;
	case ClouduABIFDSFlags:
		fputs(xlookup_bits(cloudabi_fdsflags, args[sc->offset]), fp);
		break;
	case CloudABIFDStat: {
		cloudabi_fdstat_t fds;
		if (get_struct(pid, (void *)args[sc->offset], &fds, sizeof(fds))
		    != -1) {
			fprintf(fp, "{ %s, ",
			    xlookup(cloudabi_filetype, fds.fs_filetype));
			fprintf(fp, "%s, ... }",
			    xlookup_bits(cloudabi_fdflags, fds.fs_flags));
		} else
			fprintf(fp, "0x%lx", args[sc->offset]);
		break;
	}
	case CloudABIFileStat: {
		cloudabi_filestat_t fsb;
		if (get_struct(pid, (void *)args[sc->offset], &fsb, sizeof(fsb))
		    != -1)
			fprintf(fp, "{ %s, %lu }",
			    xlookup(cloudabi_filetype, fsb.st_filetype),
			    fsb.st_size);
		else
			fprintf(fp, "0x%lx", args[sc->offset]);
		break;
	}
	case CloudABIFileType:
		fputs(xlookup(cloudabi_filetype, args[sc->offset]), fp);
		break;
	case CloudABIFSFlags:
		fputs(xlookup_bits(cloudabi_fsflags, args[sc->offset]), fp);
		break;
	case CloudABILookup:
		if ((args[sc->offset] & CLOUDABI_LOOKUP_SYMLINK_FOLLOW) != 0)
			fprintf(fp, "%d|LOOKUP_SYMLINK_FOLLOW",
			    (int)args[sc->offset]);
		else
			fprintf(fp, "%d", (int)args[sc->offset]);
		break;
	case CloudABIMFlags:
		fputs(xlookup_bits(cloudabi_mflags, args[sc->offset]), fp);
		break;
	case CloudABIMProt:
		fputs(xlookup_bits(cloudabi_mprot, args[sc->offset]), fp);
		break;
	case CloudABIMSFlags:
		fputs(xlookup_bits(cloudabi_msflags, args[sc->offset]), fp);
		break;
	case CloudABIOFlags:
		fputs(xlookup_bits(cloudabi_oflags, args[sc->offset]), fp);
		break;
	case CloudABISDFlags:
		fputs(xlookup_bits(cloudabi_sdflags, args[sc->offset]), fp);
		break;
	case CloudABISignal:
		fputs(xlookup(cloudabi_signal, args[sc->offset]), fp);
		break;
	case CloudABISockStat: {
		cloudabi_sockstat_t ss;
		if (get_struct(pid, (void *)args[sc->offset], &ss, sizeof(ss))
		    != -1) {
			fprintf(fp, "{ %s, ", xlookup(
			    cloudabi_sa_family, ss.ss_sockname.sa_family));
			fprintf(fp, "%s, ", xlookup(
			    cloudabi_sa_family, ss.ss_peername.sa_family));
			fprintf(fp, "%s, ", xlookup(
			    cloudabi_errno, ss.ss_error));
			fprintf(fp, "%s }", xlookup_bits(
			    cloudabi_ssstate, ss.ss_state));
		} else
			fprintf(fp, "0x%lx", args[sc->offset]);
		break;
	}
	case CloudABISSFlags:
		fputs(xlookup_bits(cloudabi_ssflags, args[sc->offset]), fp);
		break;
	case CloudABITimestamp:
		fprintf(fp, "%lu.%09lus", args[sc->offset] / 1000000000,
		    args[sc->offset] % 1000000000);
		break;
	case CloudABIULFlags:
		fputs(xlookup_bits(cloudabi_ulflags, args[sc->offset]), fp);
		break;
	case CloudABIWhence:
		fputs(xlookup(cloudabi_whence, args[sc->offset]), fp);
		break;

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

	name = t->cs.name;
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
print_syscall_ret(struct trussinfo *trussinfo, int errorp, long *retval)
{
	struct timespec timediff;
	struct threadinfo *t;
	struct syscall *sc;
	int error;

	t = trussinfo->curthread;
	sc = t->cs.sc;
	if (trussinfo->flags & COUNTONLY) {
		timespecsubt(&t->after, &t->before, &timediff);
		timespecadd(&sc->time, &timediff, &sc->time);
		sc->ncalls++;
		if (errorp)
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

	if (errorp) {
		error = sysdecode_abi_to_freebsd_errno(t->proc->abi->abi,
		    retval[0]);
		fprintf(trussinfo->outfile, " ERR#%ld '%s'\n", retval[0],
		    error == INT_MAX ? "Unknown error" : strerror(error));
	}
#ifndef __LP64__
	else if (sc->ret_type == 2) {
		off_t off;

#if _BYTE_ORDER == _LITTLE_ENDIAN
		off = (off_t)retval[1] << 32 | retval[0];
#else
		off = (off_t)retval[0] << 32 | retval[1];
#endif
		fprintf(trussinfo->outfile, " = %jd (0x%jx)\n", (intmax_t)off,
		    (intmax_t)off);
	}
#endif
	else
		fprintf(trussinfo->outfile, " = %ld (0x%lx)\n", retval[0],
		    retval[0]);
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
	STAILQ_FOREACH(sc, &syscalls, entries)
		if (sc->ncalls) {
			fprintf(trussinfo->outfile, "%-20s%5jd.%09ld%8d%8d\n",
			    sc->name, (intmax_t)sc->time.tv_sec,
			    sc->time.tv_nsec, sc->ncalls, sc->nerror);
			timespecadd(&total, &sc->time, &total);
			ncall += sc->ncalls;
			nerror += sc->nerror;
		}
	fprintf(trussinfo->outfile, "%20s%15s%8s%8s\n",
	    "", "-------------", "-------", "-------");
	fprintf(trussinfo->outfile, "%-20s%5jd.%09ld%8d%8d\n",
	    "", (intmax_t)total.tv_sec, total.tv_nsec, ncall, nerror);
}
