/*-
 * Copyright (c) 2014 Pedro Souza <pedrosouza@freebsd.org>
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

#ifndef LSTD_H
#define LSTD_H

#include <stand.h>
#include <sys/types.h>
#include <sys/stdarg.h>
#include <sys/stdint.h>
#include <limits.h>
#include <string.h>

/*
 * Mini stdio FILE and DIR routines. These are the minimal routines needed by
 * the lfs module and lua's base code. We define them minimally here so we don't
 * have to modify lua on every import. Further, since they aren't completely
 * standard, we #define them to other names so they don't conflict with other
 * tooling that makes assumptions about these routines that might not be, in
 * fact, correct.
 */

typedef struct FILE
{
	int fd;
	size_t offset;
	size_t size;
} FILE;

typedef struct DIR
{
	int fd;
} DIR;

#define fopen lua_loader_fopen
#define freopen lua_loader_freopen
#define fread lua_loader_fread
#define fwrite lua_loader_fwrite
#define fclose lua_loader_fclose
#define ferror lua_loader_ferror
#define feof lua_loader_feof
#define getc lua_loader_getc
#define opendir lua_loader_opendir
#define fdopendir lua_loader_fdopendir
#define closedir lua_loader_closedir

FILE *fopen(const char *filename, const char *mode);
FILE *freopen( const char *filename, const char *mode, FILE *stream);
size_t fread(void *ptr, size_t size, size_t count, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t count, FILE *stream);
int fclose(FILE *stream);
int ferror(FILE *stream);
int feof(FILE *stream);
int getc(FILE * stream);
DIR *opendir(const char *name);
DIR *fdopendir(int fd);
int closedir(DIR *);

#ifndef EOF
#define EOF (-1)
#endif

#define stdin ((FILE*)NULL)
#define stdout 1

#ifndef BUFSIZ
#define BUFSIZ 512
#endif

#define lua_writestringerror(s, p) do { printf((s), (p)); } while (0)

void luai_writestring(const char *, int);

#define lua_writestring(s,l) luai_writestring(s,l)

#define fflush	/* */
#define fgets(b, l, s) fgetstr((b), (l), 0)

#endif /* LSTD_H */
