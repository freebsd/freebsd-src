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

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

/*
 * This file has routines used to print out system calls and their
 * arguments.
 */

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/ptrace.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioccom.h>
#include <machine/atomic.h>
#include <errno.h>
#include <sys/umtx.h>
#include <sys/event.h>
#include <sys/stat.h>
#include <sys/resource.h>

#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <vis.h>

#include "truss.h"
#include "extern.h"
#include "syscall.h"

/* 64-bit alignment on 32-bit platforms. */
#ifdef __powerpc__
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
struct syscall syscalls[] = {
	{ .name = "fcntl", .ret_type = 1, .nargs = 3,
	  .args = { { Int, 0 } , { Fcntl, 1 }, { Fcntlflag | OUT, 2 } } },
	{ .name = "fork", .ret_type = 1, .nargs = 0 },
	{ .name = "getegid", .ret_type = 1, .nargs = 0 },
	{ .name = "geteuid", .ret_type = 1, .nargs = 0 },
	{ .name = "getgid", .ret_type = 1, .nargs = 0 },
	{ .name = "getpid", .ret_type = 1, .nargs = 0 },
	{ .name = "getpgid", .ret_type = 1, .nargs = 1,
	  .args = { { Int, 0 } } },
	{ .name = "getpgrp", .ret_type = 1, .nargs = 0 },
	{ .name = "getppid", .ret_type = 1, .nargs = 0 },
	{ .name = "getsid", .ret_type = 1, .nargs = 1,
	  .args = { { Int, 0 } } },
	{ .name = "getuid", .ret_type = 1, .nargs = 0 },
	{ .name = "readlink", .ret_type = 1, .nargs = 3,
	  .args = { { Name, 0 } , { Readlinkres | OUT, 1 }, { Int, 2 } } },
	{ .name = "lseek", .ret_type = 2, .nargs = 3,
	  .args = { { Int, 0 }, { Quad, 1 + QUAD_ALIGN }, { Whence, 1 + QUAD_SLOTS + QUAD_ALIGN } } },
	{ .name = "linux_lseek", .ret_type = 2, .nargs = 3,
	  .args = { { Int, 0 }, { Int, 1 }, { Whence, 2 } } },
	{ .name = "mmap", .ret_type = 2, .nargs = 6,
	  .args = { { Ptr, 0 }, { Int, 1 }, { Mprot, 2 }, { Mmapflags, 3 }, { Int, 4 }, { Quad, 5 + QUAD_ALIGN } } },
	{ .name = "mprotect", .ret_type = 1, .nargs = 3,
	  .args = { { Ptr, 0 }, { Int, 1 }, { Mprot, 2 } } },
	{ .name = "open", .ret_type = 1, .nargs = 3,
	  .args = { { Name | IN, 0 } , { Open, 1 }, { Octal, 2 } } },
	{ .name = "mkdir", .ret_type = 1, .nargs = 2,
	  .args = { { Name, 0 } , { Octal, 1 } } },
	{ .name = "linux_open", .ret_type = 1, .nargs = 3,
	  .args = { { Name, 0 }, { Hex, 1 }, { Octal, 2 } } },
	{ .name = "close", .ret_type = 1, .nargs = 1,
	  .args = { { Int, 0 } } },
	{ .name = "link", .ret_type = 0, .nargs = 2,
	  .args = { { Name, 0 }, { Name, 1 } } },
	{ .name = "unlink", .ret_type = 0, .nargs = 1,
	  .args = { { Name, 0 } } },
	{ .name = "chdir", .ret_type = 0, .nargs = 1,
	  .args = { { Name, 0 } } },
	{ .name = "chroot", .ret_type = 0, .nargs = 1,
	  .args = { { Name, 0 } } },
	{ .name = "mknod", .ret_type = 0, .nargs = 3,
	  .args = { { Name, 0 }, { Octal, 1 }, { Int, 3 } } },
	{ .name = "chmod", .ret_type = 0, .nargs = 2,
	  .args = { { Name, 0 }, { Octal, 1 } } },
	{ .name = "chown", .ret_type = 0, .nargs = 3,
	  .args = { { Name, 0 }, { Int, 1 }, { Int, 2 } } },
	{ .name = "mount", .ret_type = 0, .nargs = 4,
	  .args = { { Name, 0 }, { Name, 1 }, { Int, 2 }, { Ptr, 3 } } },
	{ .name = "umount", .ret_type = 0, .nargs = 2,
	  .args = { { Name, 0 }, { Int, 2 } } },
	{ .name = "fstat", .ret_type = 1, .nargs = 2,
	  .args = { { Int, 0 }, { Stat | OUT , 1 } } },
	{ .name = "stat", .ret_type = 1, .nargs = 2,
	  .args = { { Name | IN, 0 }, { Stat | OUT, 1 } } },
	{ .name = "lstat", .ret_type = 1, .nargs = 2,
	  .args = { { Name | IN, 0 }, { Stat | OUT, 1 } } },
	{ .name = "linux_newstat", .ret_type = 1, .nargs = 2,
	  .args = { { Name | IN, 0 }, { Ptr | OUT, 1 } } },
	{ .name = "linux_newfstat", .ret_type = 1, .nargs = 2,
	  .args = { { Int, 0 }, { Ptr | OUT, 1 } } },
	{ .name = "write", .ret_type = 1, .nargs = 3,
	  .args = { { Int, 0 }, { BinString | IN, 1 }, { Int, 2 } } },
	{ .name = "ioctl", .ret_type = 1, .nargs = 3,
	  .args = { { Int, 0 }, { Ioctl, 1 }, { Hex, 2 } } },
	{ .name = "break", .ret_type = 1, .nargs = 1,
	  .args = { { Ptr, 0 } } },
	{ .name = "exit", .ret_type = 0, .nargs = 1,
	  .args = { { Hex, 0 } } },
	{ .name = "access", .ret_type = 1, .nargs = 2,
	  .args = { { Name | IN, 0 }, { Int, 1 } } },
	{ .name = "sigaction", .ret_type = 1, .nargs = 3,
	  .args = { { Signal, 0 }, { Sigaction | IN, 1 }, { Sigaction | OUT, 2 } } },
	{ .name = "accept", .ret_type = 1, .nargs = 3,
	  .args = { { Int, 0 }, { Sockaddr | OUT, 1 }, { Ptr | OUT, 2 } } },
	{ .name = "bind", .ret_type = 1, .nargs = 3,
	  .args = { { Int, 0 }, { Sockaddr | IN, 1 }, { Int, 2 } } },
	{ .name = "connect", .ret_type = 1, .nargs = 3,
	  .args = { { Int, 0 }, { Sockaddr | IN, 1 }, { Int, 2 } } },
	{ .name = "getpeername", .ret_type = 1, .nargs = 3,
	  .args = { { Int, 0 }, { Sockaddr | OUT, 1 }, { Ptr | OUT, 2 } } },
	{ .name = "getsockname", .ret_type = 1, .nargs = 3,
	  .args = { { Int, 0 }, { Sockaddr | OUT, 1 }, { Ptr | OUT, 2 } } },
	{ .name = "recvfrom", .ret_type = 1, .nargs = 6,
	  .args = { { Int, 0 }, { BinString | OUT, 1 }, { Int, 2 }, { Hex, 3 }, { Sockaddr | OUT, 4 }, { Ptr | OUT, 5 } } },
	{ .name = "sendto", .ret_type = 1, .nargs = 6,
	  .args = { { Int, 0 }, { BinString | IN, 1 }, { Int, 2 }, { Hex, 3 }, { Sockaddr | IN, 4 }, { Ptr | IN, 5 } } },
	{ .name = "execve", .ret_type = 1, .nargs = 3,
	  .args = { { Name | IN, 0 }, { StringArray | IN, 1 }, { StringArray | IN, 2 } } },
	{ .name = "linux_execve", .ret_type = 1, .nargs = 3,
	  .args = { { Name | IN, 0 }, { StringArray | IN, 1 }, { StringArray | IN, 2 } } },
	{ .name = "kldload", .ret_type = 0, .nargs = 1,
	  .args = { { Name | IN, 0 } } },
	{ .name = "kldunload", .ret_type = 0, .nargs = 1,
	  .args = { { Int, 0 } } },
	{ .name = "kldfind", .ret_type = 0, .nargs = 1,
	  .args = { { Name | IN, 0 } } },
	{ .name = "kldnext", .ret_type = 0, .nargs = 1,
	  .args = { { Int, 0 } } },
	{ .name = "kldstat", .ret_type = 0, .nargs = 2,
	  .args = { { Int, 0 }, { Ptr, 1 } } },
	{ .name = "kldfirstmod", .ret_type = 0, .nargs = 1,
	  .args = { { Int, 0 } } },
	{ .name = "nanosleep", .ret_type = 0, .nargs = 1,
	  .args = { { Timespec, 0 } } },
	{ .name = "select", .ret_type = 1, .nargs = 5,
	  .args = { { Int, 0 }, { Fd_set, 1 }, { Fd_set, 2 }, { Fd_set, 3 }, { Timeval, 4 } } },
	{ .name = "poll", .ret_type = 1, .nargs = 3,
	  .args = { { Pollfd, 0 }, { Int, 1 }, { Int, 2 } } },
	{ .name = "gettimeofday", .ret_type = 1, .nargs = 2,
	  .args = { { Timeval | OUT, 0 }, { Ptr, 1 } } },
	{ .name = "clock_gettime", .ret_type = 1, .nargs = 2,
	  .args = { { Int, 0 }, { Timespec | OUT, 1 } } },
	{ .name = "getitimer", .ret_type = 1, .nargs = 2,
	  .args = { { Int, 0 }, { Itimerval | OUT, 2 } } },
	{ .name = "setitimer", .ret_type = 1, .nargs = 3,
	  .args = { { Int, 0 }, { Itimerval, 1 } , { Itimerval | OUT, 2 } } },
	{ .name = "kse_release", .ret_type = 0, .nargs = 1,
	  .args = { { Timespec, 0 } } },
	{ .name = "kevent", .ret_type = 0, .nargs = 6,
	  .args = { { Int, 0 }, { Kevent, 1 }, { Int, 2 }, { Kevent | OUT, 3 }, { Int, 4 }, { Timespec, 5 } } },
	{ .name = "_umtx_lock", .ret_type = 0, .nargs = 1,
	  .args = { { Umtx, 0 } } },
	{ .name = "_umtx_unlock", .ret_type = 0, .nargs = 1,
	  .args = { { Umtx, 0 } } },
	{ .name = "sigprocmask", .ret_type = 0, .nargs = 3,
	  .args = { { Sigprocmask, 0 }, { Sigset, 1 }, { Sigset | OUT, 2 } } },
	{ .name = "unmount", .ret_type = 1, .nargs = 2,
	  .args = { { Name, 0 }, { Int, 1 } } },
	{ .name = "socket", .ret_type = 1, .nargs = 3,
	  .args = { { Sockdomain, 0 }, { Socktype, 1 }, { Int, 2 } } },
	{ .name = "getrusage", .ret_type = 1, .nargs = 2,
	  .args = { { Int, 0 }, { Rusage | OUT, 1 } } },
	{ .name = "__getcwd", .ret_type = 1, .nargs = 2,
	  .args = { { Name | OUT, 0 }, { Int, 1 } } },
	{ .name = "shutdown", .ret_type = 1, .nargs = 2,
	  .args = { { Int, 0 }, { Shutdown, 1 } } },
	{ .name = "getrlimit", .ret_type = 1, .nargs = 2,
	  .args = { { Resource, 0 }, { Rlimit | OUT, 1 } } },
	{ .name = "setrlimit", .ret_type = 1, .nargs = 2,
	  .args = { { Resource, 0 }, { Rlimit | IN, 1 } } },
	{ .name = "utimes", .ret_type = 1, .nargs = 2,
	  .args = { { Name | IN, 0 }, { Timeval2 | IN, 1 } } },
	{ .name = "lutimes", .ret_type = 1, .nargs = 2,
	  .args = { { Name | IN, 0 }, { Timeval2 | IN, 1 } } },
	{ .name = "futimes", .ret_type = 1, .nargs = 2,
	  .args = { { Int, 0 }, { Timeval | IN, 1 } } },
	{ .name = "chflags", .ret_type = 1, .nargs = 2,
	  .args = { { Name | IN, 0 }, { Hex, 1 } } },
	{ .name = "lchflags", .ret_type = 1, .nargs = 2,
	  .args = { { Name | IN, 0 }, { Hex, 1 } } },
	{ .name = "pathconf", .ret_type = 1, .nargs = 2,
	  .args = { { Name | IN, 0 }, { Pathconf, 1 } } },
	{ .name = "pipe", .ret_type = 1, .nargs = 1,
	  .args = { { Ptr, 0 } } },
	{ .name = "truncate", .ret_type = 1, .nargs = 3,
	  .args = { { Name | IN, 0 }, { Int | IN, 1 }, { Quad | IN, 2 } } },
	{ .name = "ftruncate", .ret_type = 1, .nargs = 3,
	  .args = { { Int | IN, 0 }, { Int | IN, 1 }, { Quad | IN, 2 } } },
	{ .name = "kill", .ret_type = 1, .nargs = 2,
	  .args = { { Int | IN, 0 }, { Signal | IN, 1 } } },
	{ .name = "munmap", .ret_type = 1, .nargs = 2,
	  .args = { { Ptr, 0 }, { Int, 1 } } },
	{ .name = "read", .ret_type = 1, .nargs = 3,
	  .args = { { Int, 0 }, { BinString | OUT, 1 }, { Int, 2 } } },
	{ .name = "rename", .ret_type = 1, .nargs = 2,
	  .args = { { Name , 0 } , { Name, 1 } } },
	{ .name = "symlink", .ret_type = 1, .nargs = 2,
	  .args = { { Name , 0 } , { Name, 1 } } },
	{ .name = "posix_openpt", .ret_type = 1, .nargs = 1,
	  .args = { { Open, 0 } } },
	{ .name = 0 },
};

/* Xlat idea taken from strace */
struct xlat {
	int val;
	const char *str;
};

#define X(a) { a, #a },
#define XEND { 0, NULL }

static struct xlat kevent_filters[] = {
	X(EVFILT_READ) X(EVFILT_WRITE) X(EVFILT_AIO) X(EVFILT_VNODE)
	X(EVFILT_PROC) X(EVFILT_SIGNAL) X(EVFILT_TIMER)
	X(EVFILT_FS) X(EVFILT_READ) XEND
};

static struct xlat kevent_flags[] = {
	X(EV_ADD) X(EV_DELETE) X(EV_ENABLE) X(EV_DISABLE) X(EV_ONESHOT)
	X(EV_CLEAR) X(EV_FLAG1) X(EV_ERROR) X(EV_EOF) XEND
};

struct xlat poll_flags[] = {
	X(POLLSTANDARD) X(POLLIN) X(POLLPRI) X(POLLOUT) X(POLLERR)
	X(POLLHUP) X(POLLNVAL) X(POLLRDNORM) X(POLLRDBAND)
	X(POLLWRBAND) X(POLLINIGNEOF) XEND
};

static struct xlat mmap_flags[] = {
	X(MAP_SHARED) X(MAP_PRIVATE) X(MAP_FIXED) X(MAP_RENAME)
	X(MAP_NORESERVE) X(MAP_RESERVED0080) X(MAP_RESERVED0100)
	X(MAP_HASSEMAPHORE) X(MAP_STACK) X(MAP_NOSYNC) X(MAP_ANON)
	X(MAP_NOCORE) XEND
};

static struct xlat mprot_flags[] = {
	X(PROT_NONE) X(PROT_READ) X(PROT_WRITE) X(PROT_EXEC) XEND
};

static struct xlat whence_arg[] = {
	X(SEEK_SET) X(SEEK_CUR) X(SEEK_END) XEND
};

static struct xlat sigaction_flags[] = {
	X(SA_ONSTACK) X(SA_RESTART) X(SA_RESETHAND) X(SA_NOCLDSTOP)
	X(SA_NODEFER) X(SA_NOCLDWAIT) X(SA_SIGINFO) XEND
};

static struct xlat fcntl_arg[] = {
	X(F_DUPFD) X(F_GETFD) X(F_SETFD) X(F_GETFL) X(F_SETFL)
	X(F_GETOWN) X(F_SETOWN) X(F_GETLK) X(F_SETLK) X(F_SETLKW) XEND
};

static struct xlat fcntlfd_arg[] = {
	X(FD_CLOEXEC) XEND
};

static struct xlat fcntlfl_arg[] = {
	X(O_APPEND) X(O_ASYNC) X(O_FSYNC) X(O_NONBLOCK) X(O_NOFOLLOW)
	X(O_DIRECT) XEND
};

static struct xlat sockdomain_arg[] = {
	X(PF_UNSPEC) X(PF_LOCAL) X(PF_UNIX) X(PF_INET) X(PF_IMPLINK)
	X(PF_PUP) X(PF_CHAOS) X(PF_NETBIOS) X(PF_ISO) X(PF_OSI)
	X(PF_ECMA) X(PF_DATAKIT) X(PF_CCITT) X(PF_SNA) X(PF_DECnet)
	X(PF_DLI) X(PF_LAT) X(PF_HYLINK) X(PF_APPLETALK) X(PF_ROUTE)
	X(PF_LINK) X(PF_XTP) X(PF_COIP) X(PF_CNT) X(PF_SIP) X(PF_IPX)
	X(PF_RTIP) X(PF_PIP) X(PF_ISDN) X(PF_KEY) X(PF_INET6)
	X(PF_NATM) X(PF_ATM) X(PF_NETGRAPH) X(PF_SLOW) X(PF_SCLUSTER)
	X(PF_ARP) X(PF_BLUETOOTH) XEND
};

static struct xlat socktype_arg[] = {
	X(SOCK_STREAM) X(SOCK_DGRAM) X(SOCK_RAW) X(SOCK_RDM)
	X(SOCK_SEQPACKET) XEND
};

static struct xlat open_flags[] = {
	X(O_RDONLY) X(O_WRONLY) X(O_RDWR) X(O_ACCMODE) X(O_NONBLOCK)
	X(O_APPEND) X(O_SHLOCK) X(O_EXLOCK) X(O_ASYNC) X(O_FSYNC)
	X(O_NOFOLLOW) X(O_CREAT) X(O_TRUNC) X(O_EXCL) X(O_NOCTTY)
	X(O_DIRECT) XEND
};

static struct xlat shutdown_arg[] = {
	X(SHUT_RD) X(SHUT_WR) X(SHUT_RDWR) XEND
};

static struct xlat resource_arg[] = {
	X(RLIMIT_CPU) X(RLIMIT_FSIZE) X(RLIMIT_DATA) X(RLIMIT_STACK)
	X(RLIMIT_CORE) X(RLIMIT_RSS) X(RLIMIT_MEMLOCK) X(RLIMIT_NPROC)
	X(RLIMIT_NOFILE) X(RLIMIT_SBSIZE) X(RLIMIT_VMEM) XEND
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

/* Searches an xlat array containing bitfield values.  Remaining bits
   set after removing the known ones are printed at the end:
   IN|0x400 */
static char *
xlookup_bits(struct xlat *xlat, int val)
{
	static char str[512];
	int len = 0;
	int rem = val;

	for (; xlat->str != NULL; xlat++) {
		if ((xlat->val & rem) == xlat->val) {
			/* don't print the "all-bits-zero" string unless all
			   bits are really zero */
			if (xlat->val == 0 && val != 0)
				continue;
			len += sprintf(str + len, "%s|", xlat->str);
			rem &= ~(xlat->val);
		}
	}
	/* if we have leftover bits or didn't match anything */
	if (rem || len == 0)
		len += sprintf(str + len, "0x%x", rem);
	if (len && str[len - 1] == '|')
		len--;
	str[len] = 0;
	return (str);
}

/*
 * If/when the list gets big, it might be desirable to do it
 * as a hash table or binary search.
 */

struct syscall *
get_syscall(const char *name)
{
	struct syscall *sc = syscalls;

	if (name == NULL)
		return (NULL);
	while (sc->name) {
		if (!strcmp(name, sc->name))
			return (sc);
		sc++;
	}
	return (NULL);
}

/*
 * get_struct
 *
 * Copy a fixed amount of bytes from the process.
 */

static int
get_struct(int pid, void *offset, void *buf, int len)
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

#define MAXSIZE 4096
#define BLOCKSIZE 1024
/*
 * get_string
 * Copy a string from the process.  Note that it is
 * expected to be a C string, but if max is set, it will
 * only get that much.
 */

static char *
get_string(pid_t pid, void *offset, int max)
{
	char *buf;
	struct ptrace_io_desc iorequest;
	int totalsize, size;
	int diff = 0;
	int i;

	totalsize = size = max ? (max + 1) : BLOCKSIZE;
	buf = malloc(totalsize);
	if (buf == NULL)
		return (NULL);
	for (;;) {
		diff = totalsize - size;
		iorequest.piod_op = PIOD_READ_D;
		iorequest.piod_offs = (char *)offset + diff;
		iorequest.piod_addr = buf + diff;
		iorequest.piod_len = size;
		if (ptrace(PT_IO, pid, (caddr_t)&iorequest, 0) < 0) {
			free(buf);
			return (NULL);
		}
		for (i = 0 ; i < size; i++) {
			if (buf[diff + i] == '\0')
				return (buf);
		}
		if (totalsize < MAXSIZE - BLOCKSIZE && max == 0) {
			totalsize += BLOCKSIZE;
			buf = realloc(buf, totalsize);
			size = BLOCKSIZE;
		} else {
			buf[totalsize - 1] = '\0';
			return (buf);
		}
	}
}


/*
 * print_arg
 * Converts a syscall argument into a string.  Said string is
 * allocated via malloc(), so needs to be free()'d.  The file
 * descriptor is for the process' memory (via /proc), and is used
 * to get any data (where the argument is a pointer).  sc is
 * a pointer to the syscall description (see above); args is
 * an array of all of the system call arguments.
 */

char *
print_arg(struct syscall_args *sc, unsigned long *args, long retval, struct trussinfo *trussinfo)
{
	char *tmp = NULL;
	int pid = trussinfo->pid;

	switch (sc->type & ARG_MASK) {
	case Hex:
		asprintf(&tmp, "0x%x", (int)args[sc->offset]);
		break;
	case Octal:
		asprintf(&tmp, "0%o", (int)args[sc->offset]);
		break;
	case Int:
		asprintf(&tmp, "%d", (int)args[sc->offset]);
		break;
	case Name: {
		/* NULL-terminated string. */
		char *tmp2;
		tmp2 = get_string(pid, (void*)args[sc->offset], 0);
		asprintf(&tmp, "\"%s\"", tmp2);
		free(tmp2);
		break;
	}
	case BinString: {
		/* Binary block of data that might have printable characters.
		   XXX If type|OUT, assume that the length is the syscall's
		   return value.  Otherwise, assume that the length of the block
		   is in the next syscall argument. */
		int max_string = trussinfo->strsize;
		char tmp2[max_string+1], *tmp3;
		int len;
		int truncated = 0;

		if (sc->type & OUT)
			len = retval;
		else
			len = args[sc->offset + 1];

		/* Don't print more than max_string characters, to avoid word
		   wrap.  If we have to truncate put some ... after the string.
		*/
		if (len > max_string) {
			len = max_string;
			truncated = 1;
		}
		if (len && get_struct(pid, (void*)args[sc->offset], &tmp2, len) != -1) {
			tmp3 = malloc(len * 4 + 1);
			while (len) {
				if (strvisx(tmp3, tmp2, len, VIS_CSTYLE|VIS_TAB|VIS_NL) <= max_string)
					break;
				len--;
				truncated = 1;
			};
			asprintf(&tmp, "\"%s\"%s", tmp3, truncated?"...":"");
			free(tmp3);
		} else {
			asprintf(&tmp, "0x%lx", args[sc->offset]);
		}
		break;
	}
	case StringArray: {
		int num, size, i;
		char *tmp2;
		char *string;
		char *strarray[100];	/* XXX This is ugly. */

		if (get_struct(pid, (void *)args[sc->offset], (void *)&strarray,
			sizeof(strarray)) == -1) {
			err(1, "get_struct %p", (void *)args[sc->offset]);
		}
		num = 0;
		size = 0;

		/* Find out how large of a buffer we'll need. */
		while (strarray[num] != NULL) {
			string = get_string(pid, (void*)strarray[num], 0);
			size += strlen(string);
			free(string);
			num++;
		}
		size += 4 + (num * 4);
		tmp = (char *)malloc(size);
		tmp2 = tmp;

		tmp2 += sprintf(tmp2, " [");
		for (i = 0; i < num; i++) {
			string = get_string(pid, (void*)strarray[i], 0);
			tmp2 += sprintf(tmp2, " \"%s\"%c", string, (i+1 == num) ? ' ' : ',');
			free(string);
		}
		tmp2 += sprintf(tmp2, "]");
		break;
	}
#ifdef __LP64__
	case Quad:
		asprintf(&tmp, "0x%lx", args[sc->offset]);
		break;
#else
	case Quad: {
		unsigned long long ll;
		ll = *(unsigned long long *)(args + sc->offset);
		asprintf(&tmp, "0x%llx", ll);
		break;
	}
#endif
	case Ptr:
		asprintf(&tmp, "0x%lx", args[sc->offset]);
		break;
	case Readlinkres: {
		char *tmp2;
		if (retval == -1) {
			tmp = strdup("");
			break;
		}
		tmp2 = get_string(pid, (void*)args[sc->offset], retval);
		asprintf(&tmp, "\"%s\"", tmp2);
		free(tmp2);
		break;
	}
	case Ioctl: {
		const char *temp = ioctlname(args[sc->offset]);
		if (temp) {
			tmp = strdup(temp);
		} else {
			unsigned long arg = args[sc->offset];
			asprintf(&tmp, "0x%lx { IO%s%s 0x%lx('%c'), %lu, %lu }", arg,
			    arg&IOC_OUT?"R":"", arg&IOC_IN?"W":"",
			    IOCGROUP(arg), isprint(IOCGROUP(arg))?(char)IOCGROUP(arg):'?',
			    arg & 0xFF, IOCPARM_LEN(arg));
		}
		break;
	}
	case Umtx: {
		struct umtx umtx;
		if (get_struct(pid, (void *)args[sc->offset], &umtx, sizeof(umtx)) != -1)
			asprintf(&tmp, "{ 0x%lx }", (long)umtx.u_owner);
		else
			asprintf(&tmp, "0x%lx", args[sc->offset]);
		break;
	}
	case Timespec: {
		struct timespec ts;
		if (get_struct(pid, (void *)args[sc->offset], &ts, sizeof(ts)) != -1)
			asprintf(&tmp, "{%ld.%09ld }", (long)ts.tv_sec, ts.tv_nsec);
		else
			asprintf(&tmp, "0x%lx", args[sc->offset]);
		break;
	}
	case Timeval: {
		struct timeval tv;
		if (get_struct(pid, (void *)args[sc->offset], &tv, sizeof(tv)) != -1)
			asprintf(&tmp, "{%ld.%06ld }", (long)tv.tv_sec, tv.tv_usec);
		else
			asprintf(&tmp, "0x%lx", args[sc->offset]);
		break;
	}
	case Timeval2: {
		struct timeval tv[2];
		if (get_struct(pid, (void *)args[sc->offset], &tv, sizeof(tv)) != -1)
			asprintf(&tmp, "{%ld.%06ld, %ld.%06ld }",
			    (long)tv[0].tv_sec, tv[0].tv_usec,
			    (long)tv[1].tv_sec, tv[1].tv_usec);
		else
			asprintf(&tmp, "0x%lx", args[sc->offset]);
		break;
	}
	case Itimerval: {
		struct itimerval itv;
		if (get_struct(pid, (void *)args[sc->offset], &itv, sizeof(itv)) != -1)
			asprintf(&tmp, "{%ld.%06ld, %ld.%06ld }",
			    (long)itv.it_interval.tv_sec,
			    itv.it_interval.tv_usec,
			    (long)itv.it_value.tv_sec,
			    itv.it_value.tv_usec);
		else
			asprintf(&tmp, "0x%lx", args[sc->offset]);
		break;
	}
	case Pollfd: {
		/*
		 * XXX: A Pollfd argument expects the /next/ syscall argument to be
		 * the number of fds in the array. This matches the poll syscall.
		 */
		struct pollfd *pfd;
		int numfds = args[sc->offset+1];
		int bytes = sizeof(struct pollfd) * numfds;
		int i, tmpsize, u, used;
		const int per_fd = 100;

		if ((pfd = malloc(bytes)) == NULL)
			err(1, "Cannot malloc %d bytes for pollfd array", bytes);
		if (get_struct(pid, (void *)args[sc->offset], pfd, bytes) != -1) {

			used = 0;
			tmpsize = 1 + per_fd * numfds + 2;
			if ((tmp = malloc(tmpsize)) == NULL)
				err(1, "Cannot alloc %d bytes for poll output", tmpsize);

			tmp[used++] = '{';
			for (i = 0; i < numfds; i++) {

				u = snprintf(tmp + used, per_fd,
				    "%s%d/%s",
				    i > 0 ? " " : "",
				    pfd[i].fd,
				    xlookup_bits(poll_flags, pfd[i].events) );
				if (u > 0)
					used += u < per_fd ? u : per_fd;
			}
			tmp[used++] = '}';
			tmp[used++] = '\0';
		} else {
			asprintf(&tmp, "0x%lx", args[sc->offset]);
		}
		free(pfd);
		break;
	}
	case Fd_set: {
		/*
		 * XXX: A Fd_set argument expects the /first/ syscall argument to be
		 * the number of fds in the array.  This matches the select syscall.
		 */
		fd_set *fds;
		int numfds = args[0];
		int bytes = _howmany(numfds, _NFDBITS) * _NFDBITS;
		int i, tmpsize, u, used;
		const int per_fd = 20;

		if ((fds = malloc(bytes)) == NULL)
			err(1, "Cannot malloc %d bytes for fd_set array", bytes);
		if (get_struct(pid, (void *)args[sc->offset], fds, bytes) != -1) {
			used = 0;
			tmpsize = 1 + numfds * per_fd + 2;
			if ((tmp = malloc(tmpsize)) == NULL)
				err(1, "Cannot alloc %d bytes for fd_set output", tmpsize);

			tmp[used++] = '{';
			for (i = 0; i < numfds; i++) {
				if (FD_ISSET(i, fds)) {
					u = snprintf(tmp + used, per_fd, "%d ", i);
					if (u > 0)
						used += u < per_fd ? u : per_fd;
				}
			}
			if (tmp[used-1] == ' ')
				used--;
			tmp[used++] = '}';
			tmp[used++] = '\0';
		} else {
			asprintf(&tmp, "0x%lx", args[sc->offset]);
		}
		free(fds);
		break;
	}
	case Signal: {
		long sig;

		sig = args[sc->offset];
		tmp = strsig(sig);
		if (tmp == NULL)
			asprintf(&tmp, "%ld", sig);
		break;
	}
	case Sigset: {
		long sig;
		sigset_t ss;
		int i, used;

		sig = args[sc->offset];
		if (get_struct(pid, (void *)args[sc->offset], (void *)&ss, sizeof(ss)) == -1) {
			asprintf(&tmp, "0x%lx", args[sc->offset]);
			break;
		}
		tmp = malloc(sys_nsig * 8); /* 7 bytes avg per signal name */
		used = 0;
		for (i = 1; i < sys_nsig; i++) {
			if (sigismember(&ss, i)) {
				used += sprintf(tmp + used, "%s|", strsig(i));
			}
		}
		if (used)
			tmp[used-1] = 0;
		else
			strcpy(tmp, "0x0");
		break;
	}
	case Sigprocmask: {
		switch (args[sc->offset]) {
#define S(a)	case a: tmp = strdup(#a); break;
			S(SIG_BLOCK);
			S(SIG_UNBLOCK);
			S(SIG_SETMASK);
#undef S
		}
		if (tmp == NULL)
			asprintf(&tmp, "0x%lx", args[sc->offset]);
		break;
	}
	case Fcntlflag: {
		/* XXX output depends on the value of the previous argument */
		switch (args[sc->offset-1]) {
		case F_SETFD:
			tmp = strdup(xlookup_bits(fcntlfd_arg, args[sc->offset]));
			break;
		case F_SETFL:
			tmp = strdup(xlookup_bits(fcntlfl_arg, args[sc->offset]));
			break;
		case F_GETFD:
		case F_GETFL:
		case F_GETOWN:
			tmp = strdup("");
			break;
		default:
			asprintf(&tmp, "0x%lx", args[sc->offset]);
			break;
		}
		break;
	}
	case Open:
		tmp = strdup(xlookup_bits(open_flags, args[sc->offset]));
		break;
	case Fcntl:
		tmp = strdup(xlookup(fcntl_arg, args[sc->offset]));
		break;
	case Mprot:
		tmp = strdup(xlookup_bits(mprot_flags, args[sc->offset]));
		break;
	case Mmapflags:
		tmp = strdup(xlookup_bits(mmap_flags, args[sc->offset]));
		break;
	case Whence:
		tmp = strdup(xlookup(whence_arg, args[sc->offset]));
		break;
	case Sockdomain:
		tmp = strdup(xlookup(sockdomain_arg, args[sc->offset]));
		break;
	case Socktype:
		tmp = strdup(xlookup(socktype_arg, args[sc->offset]));
		break;
	case Shutdown:
		tmp = strdup(xlookup(shutdown_arg, args[sc->offset]));
		break;
	case Resource:
		tmp = strdup(xlookup(resource_arg, args[sc->offset]));
		break;
	case Pathconf:
		tmp = strdup(xlookup(pathconf_arg, args[sc->offset]));
		break;
	case Sockaddr: {
		struct sockaddr_storage ss;
		char addr[64];
		struct sockaddr_in *lsin;
		struct sockaddr_in6 *lsin6;
		struct sockaddr_un *sun;
		struct sockaddr *sa;
		char *p;
		u_char *q;
		int i;

		if (args[sc->offset] == 0) {
			asprintf(&tmp, "NULL");
			break;
		}

		/* yuck: get ss_len */
		if (get_struct(pid, (void *)args[sc->offset], (void *)&ss,
			sizeof(ss.ss_len) + sizeof(ss.ss_family)) == -1)
			err(1, "get_struct %p", (void *)args[sc->offset]);
		/*
		 * If ss_len is 0, then try to guess from the sockaddr type.
		 * AF_UNIX may be initialized incorrectly, so always frob
		 * it by using the "right" size.
		 */
		if (ss.ss_len == 0 || ss.ss_family == AF_UNIX) {
			switch (ss.ss_family) {
			case AF_INET:
				ss.ss_len = sizeof(*lsin);
				break;
			case AF_UNIX:
				ss.ss_len = sizeof(*sun);
				break;
			default:
				/* hurrrr */
				break;
			}
		}
		if (get_struct(pid, (void *)args[sc->offset], (void *)&ss, ss.ss_len)
		    == -1) {
			err(2, "get_struct %p", (void *)args[sc->offset]);
		}

		switch (ss.ss_family) {
		case AF_INET:
			lsin = (struct sockaddr_in *)&ss;
			inet_ntop(AF_INET, &lsin->sin_addr, addr, sizeof addr);
			asprintf(&tmp, "{ AF_INET %s:%d }", addr, htons(lsin->sin_port));
			break;
		case AF_INET6:
			lsin6 = (struct sockaddr_in6 *)&ss;
			inet_ntop(AF_INET6, &lsin6->sin6_addr, addr, sizeof addr);
			asprintf(&tmp, "{ AF_INET6 [%s]:%d }", addr, htons(lsin6->sin6_port));
			break;
		case AF_UNIX:
			sun = (struct sockaddr_un *)&ss;
			asprintf(&tmp, "{ AF_UNIX \"%s\" }", sun->sun_path);
			break;
		default:
			sa = (struct sockaddr *)&ss;
			asprintf(&tmp, "{ sa_len = %d, sa_family = %d, sa_data = {%n%*s } }",
			    (int)sa->sa_len, (int)sa->sa_family, &i,
			    6 * (int)(sa->sa_len - ((char *)&sa->sa_data - (char *)sa)), "");
			if (tmp != NULL) {
				p = tmp + i;
				for (q = (u_char *)&sa->sa_data; q < (u_char *)sa + sa->sa_len; q++)
					p += sprintf(p, " %#02x,", *q);
			}
		}
		break;
	}
	case Sigaction: {
		struct sigaction sa;
		char *hand;
		const char *h;

		if (get_struct(pid, (void *)args[sc->offset], &sa, sizeof(sa)) != -1) {

			asprintf(&hand, "%p", sa.sa_handler);
			if (sa.sa_handler == SIG_DFL)
				h = "SIG_DFL";
			else if (sa.sa_handler == SIG_IGN)
				h = "SIG_IGN";
			else
				h = hand;

			asprintf(&tmp, "{ %s %s ss_t }",
			    h,
			    xlookup_bits(sigaction_flags, sa.sa_flags));
			free(hand);
		} else {
			asprintf(&tmp, "0x%lx", args[sc->offset]);
		}
		break;
	}
	case Kevent: {
		/*
		 * XXX XXX: the size of the array is determined by either the
		 * next syscall argument, or by the syscall returnvalue,
		 * depending on which argument number we are.  This matches the
		 * kevent syscall, but luckily that's the only syscall that uses
		 * them.
		 */
		struct kevent *ke;
		int numevents = -1;
		int bytes = 0;
		int i, tmpsize, u, used;
		const int per_ke = 100;

		if (sc->offset == 1)
			numevents = args[sc->offset+1];
		else if (sc->offset == 3 && retval != -1)
			numevents = retval;

		if (numevents >= 0)
			bytes = sizeof(struct kevent) * numevents;
		if ((ke = malloc(bytes)) == NULL)
			err(1, "Cannot malloc %d bytes for kevent array", bytes);
		if (numevents >= 0 && get_struct(pid, (void *)args[sc->offset], ke, bytes) != -1) {
			used = 0;
			tmpsize = 1 + per_ke * numevents + 2;
			if ((tmp = malloc(tmpsize)) == NULL)
				err(1, "Cannot alloc %d bytes for kevent output", tmpsize);

			tmp[used++] = '{';
			for (i = 0; i < numevents; i++) {
				u = snprintf(tmp + used, per_ke,
				    "%s%p,%s,%s,%d,%p,%p",
				    i > 0 ? " " : "",
				    (void *)ke[i].ident,
				    xlookup(kevent_filters, ke[i].filter),
				    xlookup_bits(kevent_flags, ke[i].flags),
				    ke[i].fflags,
				    (void *)ke[i].data,
				    (void *)ke[i].udata);
				if (u > 0)
					used += u < per_ke ? u : per_ke;
			}
			tmp[used++] = '}';
			tmp[used++] = '\0';
		} else {
			asprintf(&tmp, "0x%lx", args[sc->offset]);
		}
		free(ke);
		break;
	}
	case Stat: {
		struct stat st;
		if (get_struct(pid, (void *)args[sc->offset], &st, sizeof(st)) != -1) {
			char mode[12];
			strmode(st.st_mode, mode);
			asprintf(&tmp, "{ mode=%s,inode=%jd,size=%jd,blksize=%ld }",
			    mode,
			    (intmax_t)st.st_ino,(intmax_t)st.st_size,(long)st.st_blksize);
		} else {
			asprintf(&tmp, "0x%lx", args[sc->offset]);
		}
		break;
	}
	case Rusage: {
		struct rusage ru;
		if (get_struct(pid, (void *)args[sc->offset], &ru, sizeof(ru)) != -1) {
			asprintf(&tmp, "{ u=%ld.%06ld,s=%ld.%06ld,in=%ld,out=%ld }",
			    (long)ru.ru_utime.tv_sec, ru.ru_utime.tv_usec,
			    (long)ru.ru_stime.tv_sec, ru.ru_stime.tv_usec,
			    ru.ru_inblock, ru.ru_oublock);
		} else {
			asprintf(&tmp, "0x%lx", args[sc->offset]);
		}
		break;
	}
	case Rlimit: {
		struct rlimit rl;
		if (get_struct(pid, (void *)args[sc->offset], &rl, sizeof(rl)) != -1) {
			asprintf(&tmp, "{ cur=%ju,max=%ju }",
			    rl.rlim_cur, rl.rlim_max);
		} else {
			asprintf(&tmp, "0x%lx", args[sc->offset]);
		}
		break;
	}
	default:
		errx(1, "Invalid argument type %d\n", sc->type & ARG_MASK);
	}
	return (tmp);
}

/*
 * print_syscall
 * Print (to outfile) the system call and its arguments.  Note that
 * nargs is the number of arguments (not the number of words; this is
 * potentially confusing, I know).
 */

void
print_syscall(struct trussinfo *trussinfo, const char *name, int nargs, char **s_args)
{
	int i;
	int len = 0;
	struct timespec timediff;

	if (trussinfo->flags & FOLLOWFORKS)
		len += fprintf(trussinfo->outfile, "%5d: ", trussinfo->pid);

	if (name != NULL && (!strcmp(name, "execve") || !strcmp(name, "exit"))) {
		clock_gettime(CLOCK_REALTIME, &trussinfo->after);
	}

	if (trussinfo->flags & ABSOLUTETIMESTAMPS) {
		timespecsubt(&trussinfo->after, &trussinfo->start_time, &timediff);
		len += fprintf(trussinfo->outfile, "%ld.%09ld ",
		    (long)timediff.tv_sec, timediff.tv_nsec);
	}

	if (trussinfo->flags & RELATIVETIMESTAMPS) {
		timespecsubt(&trussinfo->after, &trussinfo->before, &timediff);
		len += fprintf(trussinfo->outfile, "%ld.%09ld ",
		    (long)timediff.tv_sec, timediff.tv_nsec);
	}

	len += fprintf(trussinfo->outfile, "%s(", name);

	for (i = 0; i < nargs; i++) {
		if (s_args[i])
			len += fprintf(trussinfo->outfile, "%s", s_args[i]);
		else
			len += fprintf(trussinfo->outfile, "<missing argument>");
		len += fprintf(trussinfo->outfile, "%s", i < (nargs - 1) ? "," : "");
	}
	len += fprintf(trussinfo->outfile, ")");
	for (i = 0; i < 6 - (len / 8); i++)
		fprintf(trussinfo->outfile, "\t");
}

void
print_syscall_ret(struct trussinfo *trussinfo, const char *name, int nargs,
    char **s_args, int errorp, long retval, struct syscall *sc)
{
	struct timespec timediff;

	if (trussinfo->flags & COUNTONLY) {
		if (!sc)
			return;
		clock_gettime(CLOCK_REALTIME, &trussinfo->after);
		timespecsubt(&trussinfo->after, &trussinfo->before, &timediff);
		timespecadd(&sc->time, &timediff, &sc->time);
		sc->ncalls++;
		if (errorp)
			sc->nerror++;
		return;
	}

	print_syscall(trussinfo, name, nargs, s_args);
	fflush(trussinfo->outfile);
	if (errorp) {
		fprintf(trussinfo->outfile, " ERR#%ld '%s'\n", retval, strerror(retval));
	} else {
		/*
		 * Because pipe(2) has a special assembly glue to provide the
		 * libc API, we have to adjust retval.
		 */
		if (name != NULL && !strcmp(name, "pipe"))
			retval = 0;
		fprintf(trussinfo->outfile, " = %ld (0x%lx)\n", retval, retval);
	}
}

void
print_summary(struct trussinfo *trussinfo)
{
	struct syscall *sc;
	struct timespec total = {0, 0};
	int ncall, nerror;

	fprintf(trussinfo->outfile, "%-20s%15s%8s%8s\n",
		"syscall", "seconds", "calls", "errors");
	ncall = nerror = 0;
	for (sc = syscalls; sc->name != NULL; sc++)
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
