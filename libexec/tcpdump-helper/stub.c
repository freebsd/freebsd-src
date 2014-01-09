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
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

struct __sFILE {
	int junk;
};
typedef struct __sFILE FILE;

int	issetugid(void);
void	exit(int status __unused);
char	*getenv(const char *name);

FILE	*fopen(const char * restrict path, const char * restrict mode);
size_t	fread(void * restrict ptr, size_t size, size_t nmemb,
	    FILE * restrict stream);
int	fileno(FILE *stream);
int	fflush(FILE *stream);

int	close(int fildes);
int	fstat(int fildes, struct stat *buf);
int	open(const char *path, int oflag, ...);
void	rewind(FILE *stream);
ssize_t	write(int fd, const void *buf, size_t nbytes);
ssize_t	_write(int fd, const void *buf, size_t nbytes);

int	__swbuf(int c, FILE *fp);
int	putc(int c, FILE *stream);
int	puts(const char *str);
int	putchar(int c);
int	fputc(int c, FILE *stream);
int	fputs(const char *str, FILE *stream);
size_t	fwrite(const void * restrict ptr, size_t size, size_t nmemb,
	    FILE* restrict stream);
int	fprintf(FILE * restrict stream, const char * restrict format, ...);
int	fprintf_l(FILE * restrict stream, locale_t loc, const char * restrict format, ...);
int	sprintf(char * restrict str, const char * restrict format, ...);
int	vfprintf(FILE * restrict stream, const char * restrict format,
	    va_list ap);
int fclose(FILE *stream);

struct tm * gmtime(const time_t *clock);
time_t	mktime(struct tm *tm);
char	*asctime(const struct tm *tm);
struct tm *localtime(const time_t *clock);

int __isthreaded = 0;
void * __getCurrentRuneLocale(void);

/* XXX no contents, they aren't real, just pointer cookies */
static FILE __sF[3];

FILE *__stdinp = &__sF[0];
FILE *__stdoutp = &__sF[1];
FILE *__stderrp = &__sF[2];

static int	stub_errno;

char *	__progname;

#define	EOF	(-1)

int *
__error(void)
{

	return (&stub_errno);
}

int
close(int d __unused)
{

	errno = ECAPMODE;
	return (-1);
}
int
_close(int d __unused)
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
int
_fstat(int fd __unused, struct stat *sb __unused)
{

	errno = ECAPMODE;
	return (-1);
}

off_t
lseek(int fildes __unused, off_t offset __unused, int whence __unused)
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
int
_open(const char *path __unused, int flags __unused, ...)
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
ssize_t
_read(int d __unused, void *buf __unused, size_t nbytes __unused)
{

	errno = ECAPMODE;
	return (-1);
}

int
printf(const char * restrict format __unused, ...)
{

	return (-1);
}

int
puts(const char *str)
{
	return (EOF);
}

int
fputs(const char *str, FILE *stream)
{
	return (EOF);
}

/* XXX: eliminate uses of this */
size_t
fwrite(const void * restrict ptr __unused, size_t size __unused,
    size_t nmemb __unused, FILE* restrict stream __unused)
{

	return (0);
}

int
snprintf(char * __restrict str __unused, size_t n __unused,
    char const * __restrict fmt __unused, ...)
{
	return(0);
}

int
putchar(int c)
{
	return (c);
}

int	
putc(int c, FILE *stream __unused)
{
	return (c);
}

int
fputc(int c, FILE *stream __unused)
{
	return (c);
}

void
exit(int status __unused)
{
	abort();
}

int
fprintf(FILE * restrict stream __unused, const char * restrict format __unused, ...)
{
	return(0);
}
int
fprintf_l(FILE * restrict stream __unused, locale_t loc __unused, const char * restrict format __unused, ...)
{
	return(0);
}

int
sprintf(char * restrict str __unused, const char * restrict format __unused, ...)
{
	return(0);
}

int
vfprintf(FILE * restrict stream __unused, const char * restrict format __unused,
    va_list ap __unused)
{
	return(0);
}

int
__swbuf(int c, FILE *fp __unused)
{
	return (c);
}

int
issetugid(void)
{
	return (1);
}

int
fclose(FILE *stream)
{
	return EOF;
}

char *
getenv(const char *name)
{
	return NULL;
}

FILE *
fopen(const char * restrict path __unused, const char * restrict mode __unused)
{
	return NULL;
}

size_t
fread(void * restrict ptr __unused, size_t size __unused, size_t nmemb __unused,
	    FILE * restrict stream __unused)
{
	return EOF;
}

int
fileno(FILE *stream __unused)
{
	return -1;
}

void
rewind(FILE *stream __unused)
{
}

ssize_t
write(int fd __unused, const void *buf __unused, size_t nbytes __unused)
{
	return -1;
}
ssize_t
_write(int fd __unused, const void *buf __unused, size_t nbytes __unused)
{
	return -1;
}

int
fflush(FILE *stream __unused)
{
	return EOF;
}

char *fgets(char * restrict str, int size, FILE * restrict stream);
char *
fgets(char * restrict str __unused, int size __unused,
    FILE * restrict stream __unused)
{
	return (NULL);
}

void * getrpcbynumber(int number)
{
	return (NULL);
}

void * getprotobynumber(int number __unused)
{
	return (NULL);
}

void
setprotoent(int stayopen __unused)
{
}

void
endprotoent(void)
{
}

void *
gethostbyaddr(const void *addr __unused, socklen_t len __unused, int af __unused)
{
	return (NULL);
}

int
ether_ntohost(char *hostname, const struct ether_addr *e)
{
	return -1;
}

void *
getservent(void)
{
	return (NULL);
}

void
endservent(void)
{
}
