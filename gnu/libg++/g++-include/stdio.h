// This may look like C code, but it is really -*- C++ -*-
/* 
Copyright (C) 1988 Free Software Foundation
    written by Doug Lea (dl@rocky.oswego.edu)

This file is part of the GNU C++ Library.  This library is free
software; you can redistribute it and/or modify it under the terms of
the GNU Library General Public License as published by the Free
Software Foundation; either version 2 of the License, or (at your
option) any later version.  This library is distributed in the hope
that it will be useful, but WITHOUT ANY WARRANTY; without even the
implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.  See the GNU Library General Public License for more details.
You should have received a copy of the GNU Library General Public
License along with this library; if not, write to the Free Software
Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef _stdio_h
#ifdef __GNUG__
#pragma interface
#endif

#ifdef __stdio_h_recursive
#include_next <stdio.h>
#else
#define __stdio_h_recursive

// Note:  The #define _stdio_h is at the end of this file,
// in case #include_next <stdio.h> finds an installed version of this
// same file -- we want it to continue until it finds the C version.

#include <_G_config.h>

extern "C" {

#undef NULL

#define fdopen __hide_fdopen
#define fopen __hide_fopen
#define fprintf __hide_fprintf
#define fputs __hide_fputs
#define fread __hide_fread
#define freopen __hide_freopen
#define fscanf __hide_fscanf
#define ftell __hide_ftell
#define fwrite __hide_fwrite
#define new __hide_new /* In case 'new' is used as a parameter name. */
#define perror __hide_perror
#define popen __hide_popen
#define printf __hide_printf
#define puts __hide_puts
#define putw __hide_putw
#define rewind __hide_rewind
#define tempnam __hide_tempnam
#define scanf __hide_scanf
#define setbuf __hide_setbuf
#define setbuffer __hide_setbuffer
#define setlinebuf __hide_setlinebuf
#define setvbuf __hide_setvbuf
#define sprintf __hide_sprintf
#define sscanf __hide_sscanf
#define tempnam __hide_tempnam
#define vfprintf __hide_vfprintf
#define vprintf __hide_vprintf
#define vsprintf __hide_vsprintf
#define _flsbuf __hide__flsbuf

#include_next <stdio.h>

#undef fdopen
#undef fopen
#undef fprintf
#undef fputs
#undef fread
#undef freopen
#undef fscanf
#undef ftell
#undef fwrite
#undef new
#undef perror
#undef popen
#undef printf
#undef puts
#undef putw
/* SCO defines remove to call unlink; that's very dangerous for us.  */
#undef remove
#undef rewind
#undef tempnam
#undef scanf
#undef setbuf
#undef setbuffer
#undef setlinebuf
#undef setvbuf
#undef sprintf
#undef sscanf
#undef tempnam
#undef vprintf
#undef vfprintf
#undef vsprintf
#undef _flsbuf

#ifndef NULL
#define NULL _G_NULL
#endif

#ifndef size_t
#define size_t _G_size_t
#endif
}

extern "C" {

int    fclose(FILE*);
FILE*  fdopen(int, const char*);
int    fflush(FILE*);
int    fgetc(FILE*);
#ifndef __386BSD__
char*  fgets _G_ARGS((char*, int, FILE *));
#else
char*  fgets _G_ARGS((char*, _G_size_t, FILE *));
#endif
FILE*  fopen(const char*, const char*);
int    fprintf(FILE*, const char* ...);
int    fputc(int, FILE*);
int    fputs(const char*, FILE*);
size_t fread(void*, size_t, size_t, FILE*);
#ifdef VMS
FILE*  freopen(const char*, const char*, FILE* ...);
#else
FILE*  freopen(const char*, const char*, FILE*);
#endif
int    fscanf(FILE*, const char* ...);
int    fseek(FILE*, long, int);
long   ftell(FILE *);
size_t fwrite(const void*, size_t, size_t, FILE*);
char*  gets(char*);
int    getw(FILE*);
int    pclose(FILE*);
void   perror(const char*);
FILE*  popen(const char*, const char*);
int    printf(const char* ...);
int    puts(const char*);
int    putw(int, FILE*);
int    rewind(FILE*);
int    scanf(const char* ...);
void   setbuf(FILE*, char*);
void   setbuffer(FILE*, char*, int);
int    setlinebuf(FILE*);
int    setvbuf(FILE*, char*, int, size_t);
int    sscanf(char*, const char* ...);
FILE*  tmpfile();
int    ungetc(int, FILE*);
int    vfprintf _G_ARGS((FILE*, const char*, _G_va_list));
int    vprintf _G_ARGS((const char*, _G_va_list));
_G_sprintf_return_type sprintf _G_ARGS((char*, const char* ...));
_G_sprintf_return_type vsprintf _G_ARGS((char*, const char*, _G_va_list));

extern int _filbuf _G_ARGS((FILE*));
extern int _flsbuf _G_ARGS((unsigned, FILE*));

}

#ifndef L_ctermid
#define L_ctermid	9 
#endif
#ifndef L_cuserid
#define L_cuserid	9
#endif
#ifndef P_tmpdir
#define	P_tmpdir    "/tmp/"
#endif
#ifndef L_tmpnam
#define	L_tmpnam    (sizeof(P_tmpdir) + 15)
#endif

#define _stdio_h 1

#endif
#endif // _stdio_h
