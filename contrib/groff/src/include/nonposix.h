/* Copyright (C) 2000, 2001, 2002 Free Software Foundation, Inc.
     Written by Eli Zaretskii (eliz@is.elta.co.il)

This file is part of groff.

groff is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

groff is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License along
with groff; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */

/* This header file compartmentalize all idiosyncrasies of non-Posix
   systems, such as MS-DOS, MS-Windows, etc.  */

#if defined _MSC_VER
# ifndef _WIN32
#  define _WIN32
# endif
#endif

#if defined(__MSDOS__) \
    || (defined(_WIN32) && !defined(_UWIN) && !defined(__CYGWIN__))

/* Binary I/O nuisances.  Note: "setmode" is right for DJGPP and
   Borland; Windows compilers might need _setmode or some such.  */
# include <fcntl.h>
# include <io.h>
# ifdef HAVE_UNISTD_H
#  include <unistd.h>
# endif
# ifdef _MSC_VER
#  define POPEN_RT     "rt"
#  define POPEN_WT     "wt"
#  define popen(c,m)   _popen(c,m)
#  define pclose(p)    _pclose(p)
#  define getpid()     (1)
#  define mkdir(p,m)   _mkdir(p)
#  define setmode(f,m) _setmode(f,m)
#  define WAIT(s,p,m)  _cwait(s,p,m)
#  define creat(p,m)   _creat(p,m)
# endif
# define SET_BINARY(f) do {if (!isatty(f)) setmode(f,O_BINARY);} while(0)
# define FOPEN_RB      "rb"
# define FOPEN_WB      "wb"
# define FOPEN_RWB     "wb+"
# ifndef O_BINARY
#  ifdef _O_BINARY
#   define O_BINARY    (_O_BINARY)
#  endif
# endif

/* The system shell.  Groff assumes a Unixy shell, but non-Posix
   systems don't have standard places where it lives, and might not
   have it installed to begin with.  We want to give them some leeway.  */
# define BSHELL        (system_shell_name())
# define BSHELL_DASH_C (system_shell_dash_c())
# define IS_BSHELL(s)  (is_system_shell(s))

/* The separator for directories in PATH and other environment
   variables.  */
# define PATH_SEP      ";"

/* Characters that separate directories in a path name.  */
# define DIR_SEPS      "/\\:"

/* How to tell if the argument is an absolute file name.  */
# define IS_ABSOLUTE(f) \
 ((f)[0] == '/' || (f)[0] == '\\' || (f)[0] && (f)[1] == ':')

/* The executable extension.  */
# define EXE_EXT       ".exe"

/* The system null device.  */
# define NULL_DEV      "NUL"

/* Prototypes.  */
# ifdef __cplusplus
  extern "C" {
# endif
    const char * system_shell_name(void);
    const char * system_shell_dash_c(void);
    int          is_system_shell(const char *);
# ifdef __cplusplus
  }
# endif

#endif

/* Defaults, for Posix systems.  */

#ifndef FOPEN_RB
# define FOPEN_RB      "r"
#endif
#ifndef FOPEN_WB
# define FOPEN_WB      "w"
#endif
#ifndef FOPEN_RWB
# define FOPEN_RWB     "w+"
#endif
#ifndef POPEN_RT
# define POPEN_RT      "r"
#endif
#ifndef POPEN_WT
# define POPEN_WT      "w"
#endif
#ifndef O_BINARY
# define O_BINARY      0
#endif
#ifndef BSHELL
# define BSHELL	       "/bin/sh"
#endif
#ifndef BSHELL_DASH_C
# define BSHELL_DASH_C "-c"
#endif
#ifndef IS_BSHELL
# define IS_BSHELL(s)  ((s) && strcmp(s,BSHELL) == 0)
#endif
#ifndef PATH_SEP
# define PATH_SEP      ":"
#endif
#ifndef DIR_SEPS
# define DIR_SEPS      "/"
#endif
#ifndef IS_ABSOLUTE
# define IS_ABSOLUTE(f) ((f)[0] == '/')
#endif
#ifndef EXE_EXT
# define EXE_EXT       ""
#endif
#ifndef NULL_DEV
# define NULL_DEV      "/dev/null"
#endif
#ifndef GS_NAME
# define GS_NAME       "gs"
#endif
#ifndef WAIT
# define WAIT(s,p,m)   wait(s)
#endif
#ifndef _WAIT_CHILD
# define _WAIT_CHILD   0
#endif
