/* This file is ho-sunos.h
   Copyright (C) 1987-1992 Free Software Foundation, Inc.
   
   This file is part of GAS, the GNU Assembler.
   
   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.
   
   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to
   the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

#if __STDC__ != 1
#define NO_STDARG
#endif /* not __STDC__ */

#if !defined(__GNUC__) && (__STDC__ != 1)
#include <memory.h>
#else
extern int memset();
#endif

/* #include <sys/stdtypes.h> before <stddef.h> when compiling by GCC.  */
#include <sys/stdtypes.h>
#include <stddef.h>
#include <ctype.h>
#include <string.h>

/* externs for system libraries. */

/*extern int abort();*/
/*extern int exit();*/
extern char *malloc();
extern char *realloc();
extern char *strchr();
extern char *strrchr();
extern int _filbuf();
extern int _flsbuf();
extern int fclose();
extern int fgetc();
extern int fprintf();
extern int fread();
extern int free();
extern int perror();
extern int printf();
extern int rewind();
extern int setvbuf();
extern int sscanf();
extern int strcmp();
extern int strlen();
extern int strncmp();
extern int time();
extern int ungetc();
extern int vfprintf();
extern int vprintf();
extern int vsprintf();
extern long atol();

#ifndef tolower
extern int tolower();
#endif /* tolower */

#ifndef toupper
extern int toupper();
#endif /* toupper */

/*
 * Local Variables:
 * fill-column: 80
 * comment-column: 0
 * End:
 */

/* end of ho-sunos.h */
