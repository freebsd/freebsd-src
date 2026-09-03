/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 * libseccomp hooks.
 */
#include "file.h"

#ifndef	lint
FILE_RCSID("@(#)$File: seccomp.c,v 1.37 2026/05/11 16:06:03 christos Exp $")
#endif	/* lint */

#if HAVE_LIBSECCOMP
#include "magic.h"
#include <seccomp.h> /* libseccomp */
#include <sys/prctl.h> /* prctl */
#include <sys/socket.h>
// See: https://sourceware.org/bugzilla/show_bug.cgi?id=32806
#include <asm/termbits.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#define DENY_RULE(call) \
    do \
	if (seccomp_rule_add (ctx, SCMP_ACT_KILL, SCMP_SYS(call), 0) == -1) \
	    goto out; \
    while (/*CONSTCOND*/0)
#define ALLOW_RULE(call) \
    do \
	if (seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS(call), 0) == -1) \
	    goto out; \
    while (/*CONSTCOND*/0)
/* ENOSYS makes glibc try an older syscall instead of dying. */
#define ERRNO_RULE(call) \
    do \
	if (seccomp_rule_add(ctx, SCMP_ACT_ERRNO(ENOSYS), SCMP_SYS(call), 0) \
	    == -1) goto out; \
    while (/*CONSTCOND*/0)

#define ALLOW_IOCTL_RULE(param) \
    do \
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(ioctl), 1, \
	    SCMP_CMP(1, SCMP_CMP_EQ, (scmp_datum_t)param, \
		     (scmp_datum_t)0)) == -1) \
		goto out; \
    while (/*CONSTCOND*/0)

static scmp_filter_ctx ctx;

int
enable_sandbox(int flags, int action)
{
	struct stat sb;
	int needs_write;

	/* Writes are needed when building the magic file (-C, -c, -l)
	   or when stdin is a pipe and we copy it to a tempfile. */
	needs_write = (action == FILE_COMPILE || action == FILE_CHECK ||
	    action == FILE_LIST);
	if (!needs_write && fstat(STDIN_FILENO, &sb) == 0 &&
	    S_ISFIFO(sb.st_mode))
		needs_write = 1;

	if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) == -1)
		return -1;

#if 0
	if (prctl(PR_SET_DUMPABLE, 0, 0, 0, 0) == -1)
		return -1;
#endif

	ctx = seccomp_init(SCMP_ACT_KILL);
	if (ctx == NULL)
		return -1;

	ALLOW_RULE(access);
	ALLOW_RULE(brk);
	ALLOW_RULE(close);
	ALLOW_RULE(dup2);
	ALLOW_RULE(exit);
	ALLOW_RULE(exit_group);
#ifdef __NR_faccessat
	ALLOW_RULE(faccessat);
#endif
	ALLOW_RULE(fcntl);
 	ALLOW_RULE(fcntl64);
#ifdef __NR_fstat
	ALLOW_RULE(fstat);
#endif
 	ALLOW_RULE(fstat64);
#ifdef __NR_fstatat64
	ALLOW_RULE(fstatat64);
#endif
	ALLOW_RULE(futex);
	ALLOW_RULE(getdents);
#ifdef __NR_getdents64
	ALLOW_RULE(getdents64);
#endif
	ALLOW_RULE(getpid);	// Used by glibc in file_pipe2file()
	ALLOW_RULE(getrandom);	// Used by glibc in file_pipe2file()
#ifdef __NR_getcwd
	ALLOW_RULE(getcwd);	// GCONV_PATH=
#endif
#ifdef FIONREAD
	// called in src/compress.c under sread
	ALLOW_IOCTL_RULE(FIONREAD);
#endif
#ifdef TIOCGWINSZ
	// musl libc may call ioctl TIOCGWINSZ on stdout
	ALLOW_IOCTL_RULE(TIOCGWINSZ);
#endif
#ifdef TCGETS
	// glibc may call ioctl TCGETS on stdout on physical terminal
	ALLOW_IOCTL_RULE(TCGETS);
#endif
#ifdef TCGETS2
	// glibc may call ioctl TCGETS2 on stdout on physical terminal
	ALLOW_IOCTL_RULE(TCGETS2);
#endif
	ALLOW_RULE(lseek);
 	ALLOW_RULE(_llseek);
	ALLOW_RULE(lstat);
 	ALLOW_RULE(lstat64);
	ALLOW_RULE(madvise);
	ALLOW_RULE(mmap);
 	ALLOW_RULE(mmap2);
	ALLOW_RULE(mprotect);
	ALLOW_RULE(mremap);
	ALLOW_RULE(munmap);
#ifdef __NR_newfstatat
	ALLOW_RULE(newfstatat);
#endif

	/* Read-only opens are always fine. Writes are allowed when we
	   need them, otherwise return EACCES instead of killing. */
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(open), 1,
	    SCMP_CMP(1, SCMP_CMP_MASKED_EQ, O_WRONLY | O_RDWR, 0)) == -1)
		goto out;
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(openat), 1,
	    SCMP_CMP(2, SCMP_CMP_MASKED_EQ, O_WRONLY | O_RDWR, 0)) == -1)
		goto out;
	{
		uint32_t act = needs_write ? SCMP_ACT_ALLOW
		                           : SCMP_ACT_ERRNO(EACCES);
		if (seccomp_rule_add(ctx, act, SCMP_SYS(open), 1,
		    SCMP_CMP(1, SCMP_CMP_MASKED_EQ, O_WRONLY, O_WRONLY)) == -1)
			goto out;
		if (seccomp_rule_add(ctx, act, SCMP_SYS(open), 1,
		    SCMP_CMP(1, SCMP_CMP_MASKED_EQ, O_RDWR, O_RDWR)) == -1)
			goto out;
		if (seccomp_rule_add(ctx, act, SCMP_SYS(openat), 1,
		    SCMP_CMP(2, SCMP_CMP_MASKED_EQ, O_WRONLY, O_WRONLY)) == -1)
			goto out;
		if (seccomp_rule_add(ctx, act, SCMP_SYS(openat), 1,
		    SCMP_CMP(2, SCMP_CMP_MASKED_EQ, O_RDWR, O_RDWR)) == -1)
			goto out;
	}
	ALLOW_RULE(pread64);
	ALLOW_RULE(read);
	ALLOW_RULE(readlink);
#ifdef __NR_readlinkat
	ALLOW_RULE(readlinkat);
#endif
	ALLOW_RULE(rseq);	// Used by glibc to randomize malloc
	ALLOW_RULE(rt_sigaction);
	ALLOW_RULE(rt_sigprocmask);
	ALLOW_RULE(rt_sigreturn);
	ALLOW_RULE(select);
	ALLOW_RULE(stat);
	ALLOW_RULE(statx);
	ALLOW_RULE(stat64);
	ALLOW_RULE(sysinfo);
	if (needs_write) {
		ALLOW_RULE(umask);	/* used in file_pipe2file() */
		ALLOW_RULE(unlinkat);	/* used in file_pipe2file() */
#ifdef __NR_unlink
		ALLOW_RULE(unlink);
#endif
	}
	if (flags & MAGIC_PRESERVE_ATIME) {
		/* glibc 2.28+ uses utimes() via utimensat. */
		ALLOW_RULE(utimensat);
#ifdef __NR_utimes
		ALLOW_RULE(utimes);
#endif
	}
	/* Always allow writes. The kernel checks the fd, and restricting
	   here would just break stdout/stderr and the tempfile. */
	ALLOW_RULE(write);
	ALLOW_RULE(writev);

	/* Newer variants we don't use. ENOSYS lets glibc fall back. */
	ERRNO_RULE(openat2);
	ERRNO_RULE(faccessat2);
	ERRNO_RULE(close_range);


#if 0
	// needed by valgrind
	ALLOW_RULE(gettid);
	ALLOW_RULE(rt_sigtimedwait);
#endif

#if 0
	 /* special restrictions for socket, only allow AF_UNIX/AF_LOCAL */
	 if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(socket), 1,
	     SCMP_CMP(0, SCMP_CMP_EQ, AF_UNIX)) == -1)
	 	goto out;

	 if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(socket), 1,
	     SCMP_CMP(0, SCMP_CMP_EQ, AF_LOCAL)) == -1)
	 	goto out;


	 /* special restrictions for open, prevent opening files for writing */
	 if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(open), 1,
	     SCMP_CMP(1, SCMP_CMP_MASKED_EQ, O_WRONLY | O_RDWR, 0)) == -1)
	 	goto out;

	 if (seccomp_rule_add(ctx, SCMP_ACT_ERRNO(EACCES), SCMP_SYS(open), 1,
	     SCMP_CMP(1, SCMP_CMP_MASKED_EQ, O_WRONLY, O_WRONLY)) == -1)
	 	goto out;

	 if (seccomp_rule_add(ctx, SCMP_ACT_ERRNO(EACCES), SCMP_SYS(open), 1,
	     SCMP_CMP(1, SCMP_CMP_MASKED_EQ, O_RDWR, O_RDWR)) == -1)
	 	goto out;


	 /* allow stderr */
	 if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(write), 1,
	     SCMP_CMP(0, SCMP_CMP_EQ, 2)) == -1)
		 goto out;
#endif

#if defined(PR_SET_VMA) && defined(PR_SET_VMA_ANON_NAME)
	/* allow glibc to name malloc areas */
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(prctl), 2,
	    SCMP_CMP32(0, SCMP_CMP_EQ, PR_SET_VMA),
	    SCMP_CMP64(1, SCMP_CMP_EQ, PR_SET_VMA_ANON_NAME)) == -1)
		goto out;
#endif

	// applying filter...
	if (seccomp_load(ctx) == -1)
		goto out;
	// free ctx after the filter has been loaded into the kernel
	seccomp_release(ctx);
	return 0;

out:
	// something went wrong
	seccomp_release(ctx);
	return -1;
}
#endif
