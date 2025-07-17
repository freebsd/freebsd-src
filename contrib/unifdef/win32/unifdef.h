/*
 * Copyright (c) 2012 - 2015 Tony Finch <dot@dotat.at>
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

/* Stop being stupid about POSIX functions */
#define _CRT_SECURE_NO_WARNINGS
#define _SCL_SECURE_NO_WARNINGS
#define _CRT_NONSTDC_NO_WARNINGS

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Windows POSIX-flavoured headers */

#include <sys/stat.h>
#include <fcntl.h>
#include <io.h>

#define stat     _stat

/* fake stdbool.h */
#define true 1
#define false 0
#define bool int

/* used by err.c and getopt.c */
#define _getprogname() "unifdef"

/*
 * The snprintf() workaround is unnecessary in Visual Studio 2015 or later
 * but dogma dictates that #if directives are not allowed inside unifdef.
 */
#define snprintf c99_snprintf

/* win32.c */
int replace(const char *oldname, const char *newname);
FILE *mktempmode(char *tmp, int mode);
FILE *fbinmode(FILE *fp);
int c99_snprintf(char *buf, size_t buflen, const char *format, ...);

/* err.c */
void err(int, const char *, ...);
void verr(int, const char *, va_list);
void errc(int, int, const char *, ...);
void verrc(int, int, const char *, va_list);
void errx(int, const char *, ...);
void verrx(int, const char *, va_list);
void warn(const char *, ...);
void vwarn(const char *, va_list);
void warnc(int, const char *, ...);
void vwarnc(int, const char *, va_list);
void warnx(const char *, ...);
void vwarnx(const char *, va_list);

/* getopt.c */
int getopt(int nargc, char *nargv[], const char *ostr);
extern int opterr, optind, optopt, optreset;
extern char *optarg;
