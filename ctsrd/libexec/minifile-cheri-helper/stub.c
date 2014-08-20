/*-
 * Copyright (c) 2012 Robert N. M. Watson
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

/*
 * This variable is zero until a process has created a thread.
 * It is used to avoid calling locking functions in libc when they
 * are not required. By default, libc is intended to be(come)
 * thread-safe, but without a (significant) penalty to non-threaded
 * processes.
 */
int     __isthreaded    = 0;
const char *__progname = "";

static int	stub_errno;

FILE *__stdinp;
FILE *__stdoutp;
FILE *__stderrp;

void
__assert(const char *func __unused, const char *file __unused,
    int line __unused, const char *failedexpr __unused)
{

	abort();
}

int *
__error(void)
{

	return (&stub_errno);
}

int
access(const char *path __unused, int mode __unused)
{

	errno = ECAPMODE;
	return (-1);
}

int _close(int d __unused);
int
_close(int d __unused)
{

	errno = ECAPMODE;
	return (-1);
}

int
close(int d __unused)
{

	errno = ECAPMODE;
	return (-1);
}

int
closedir(DIR *dirp __unused)
{

	errno = ECAPMODE;
	return (-1);
}

int
dup(int fildes __unused)
{

	errno = ECAPMODE;
	return (-1);
}

int
dup2(int oldd __unused, int newd __unused)
{

	errno = ECAPMODE;
	return (-1);
}

int
execvp(const char *file __unused, char *const argv[] __unused)
{

	errno = ECAPMODE;
	return (-1);
}

void
exit(int status __unused)
{

	abort();
}

void
_exit(int status __unused)
{

	abort();
}

char *
getenv(const char *name __unused)
{

	return NULL;
}

ssize_t getline(char **linep __unused, size_t *linecapp __unused, FILE *stream __unused);
ssize_t
getline(char **linep __unused, size_t *linecapp __unused, FILE *stream __unused)
{

	errno = ECAPMODE;
	return (-1);
}

int
fclose(FILE *stream __unused)
{

	errno = ECAPMODE;
	return (-1);
}

int
fcntl(int fd __unused, int cmd __unused, ...)
{
	
	errno = ECAPMODE;
	return EOF;
}

int
fflush(FILE *stream __unused)
{

	errno = ECAPMODE;
	return (EOF);
}

FILE *
fopen(const char *path __unused, const char *mode __unused)
{

	errno = ECAPMODE;
	return (NULL);
}

pid_t
fork(void)
{

	errno = ECAPMODE;
	return (-1);
}

int
fprintf(FILE *stream __unused, const char *format __unused, ...)
{

	errno = ECAPMODE;
	return (-1);
}

int fprintf_l(FILE *stream __unused, locale_t loc __unused, const char *format __unused, ...);
int
fprintf_l(FILE *stream __unused, locale_t loc __unused,
    const char *format __unused, ...)
{

	errno = ECAPMODE;
	return (-1);
}

int
fputc(int c __unused, FILE *stream __unused)
{

	errno = ECAPMODE;
	return (-1);
}

size_t
fread(void *ptr __unused, size_t size __unused, size_t nmemb __unused,
	      FILE *stream __unused)
{

	errno = ECAPMODE;
	return (-1);
}

int _fstat(int fd __unused, struct stat *sb __unused);
int
_fstat(int fd __unused, struct stat *sb __unused)
{

	errno = ECAPMODE;
	return (-1);
}

int
fstat(int fd __unused, struct stat *sb __unused)
{

	errno = ECAPMODE;
	return (-1);
}

size_t
fwrite(const void *ptr __unused, size_t size __unused, size_t nitems __unused,
    FILE *stream __unused)
{
	
	errno = ECAPMODE;
	return (0);
}

int
ioctl(int d __unused, unsigned long request __unused, ...)
{
	
	errno = ECAPMODE;
	return (-1);
}

int
issetugid(void)
{

	return (1);	/* XXXBD: seems more paranoid than 0 */
}

off_t
lseek(int fildes __unused, off_t offset __unused, int whence __unused)
{

	errno = ECAPMODE;
	return (-1);
}

int
lstat(const char *path __unused, struct stat *sb __unused)
{

	errno = ECAPMODE;
	return (-1);
}

int
mkstemp(char *template __unused)
{

	errno = ECAPMODE;
	return (-1);
}

void *
mmap(void *addr __unused, size_t len __unused, int prot __unused,
    int flags __unused, int fd __unused, off_t offset __unused)
{

	errno = ECAPMODE;
	return (NULL);
}

int
mprotect(const void *addr __unused, size_t len __unused, int prot __unused)
{

	errno = ECAPMODE;
	return (-1);
}

int
munmap(void *addr __unused, size_t len __unused)
{

	errno = ECAPMODE;
	return (-1);
}

int _open(const char *path __unused, int flags __unused, ...);
int
_open(const char *path __unused, int flags __unused, ...)
{

	errno = ECAPMODE;
	return (-1);
}

int
open(const char *path __unused, int flags __unused, ...)
{

	errno = ECAPMODE;
	return (-1);
}

DIR *
opendir(const char *filename __unused)
{

	errno = ECAPMODE;
	return (NULL);
}

int
pipe(int fildes[2] __unused)
{

	errno = ECAPMODE;
	return (-1);
}

ssize_t
pread(int fd __unused, void *buf __unused, size_t nbytes __unused,
    off_t offset __unused)
{

	errno = ECAPMODE;
	return (-1);
}

int
printf(const char *format __unused, ...)
{

	return(0);
}

int
puts(const char *str __unused)
{

	errno = ECAPMODE;
	return (-1);
}

int raise(int sig __unused);
int
raise(int sig __unused)
{

	errno = ECAPMODE;
	return (-1);
}

ssize_t _read(int d __unused, void *buf __unused, size_t nbytes __unused);
ssize_t
_read(int d __unused, void *buf __unused, size_t nbytes __unused)
{

	errno = ECAPMODE;
	return (-1);
}

ssize_t
read(int d __unused, void *buf __unused, size_t nbytes __unused)
{

	errno = ECAPMODE;
	return (-1);
}

struct dirent *
readdir(DIR *dirp __unused)
{

	errno = ECAPMODE;
	return (NULL);
}

ssize_t
readlink(const char *path __unused, char *buf __unused, size_t bufsiz __unused)
{

	errno = ECAPMODE;
	return (-1);
}

void
rewind(FILE *stream __unused)
{

	return;
}

int
select(int nfds __unused, fd_set *readfds __unused, fd_set *writefds __unused,
    fd_set *exceptfds __unused, struct timeval *timeout __unused)
{

	errno = ECAPMODE;
	return (-1);
}

int
stat(const char *path __unused, struct stat *sb __unused)
{
	
	errno = ECAPMODE;
	return (-1);
}

char *
strerror(int errnum __unused)
{

	return ((char *)"");
}

time_t
time(time_t *tloc)
{

	if (tloc != NULL)
		*tloc = -1;
	return (-1);
}

int
unlink(const char *path __unused)
{
	
	errno = ECAPMODE;
	return (-1);
}

int
utimes(const char *path __unused, const struct timeval *times __unused)
{
	
	errno = ECAPMODE;
	return (-1);
}

int
vfprintf(FILE *stream __unused, const char *format __unused, va_list ap
__unused)
{

	errno = ECAPMODE;
	return (-1);
}

ssize_t _write(int d __unused, const void *buf __unused, size_t nbytes __unused);
ssize_t
_write(int d __unused, const void *buf __unused, size_t nbytes __unused)
{

	errno = ECAPMODE;
	return (-1);
}

ssize_t
write(int d __unused, const void *buf __unused, size_t nbytes __unused)
{

	errno = ECAPMODE;
	return (-1);
}

pid_t waitpid(pid_t pid __unused, int *stat_loc __unused, int options __unused);
pid_t
waitpid(pid_t pid __unused, int *stat_loc __unused, int options __unused)
{

	errno = ECAPMODE;
	return (-1);
}
