/* Copyright (C) 2000, 2001, 2002, 2003, 2004, 2005
   Free Software Foundation, Inc.
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
Foundation, 51 Franklin St - Fifth Floor, Boston, MA 02110-1301, USA. */

/* This header file compartmentalize all idiosyncrasies of non-Posix
   systems, such as MS-DOS, MS-Windows, etc.  It should be loaded after
   the system headers like stdio.h to protect against warnings and error
   messages w.r.t. redefining macros. */

#if defined _MSC_VER
# ifndef _WIN32
#  define _WIN32
# endif
#endif

#if defined(__MSDOS__) || defined(__EMX__) \
    || (defined(_WIN32) && !defined(_UWIN) && !defined(__CYGWIN__))

/* Binary I/O nuisances. */
# include <fcntl.h>
# include <io.h>
# ifdef HAVE_UNISTD_H
#  include <unistd.h>
# endif
# ifndef STDIN_FILENO
#  define STDIN_FILENO	0
#  define STDOUT_FILENO	1
#  define STDERR_FILENO	2
# endif
# ifdef HAVE_DIRECT_H
#  include <direct.h>
# endif
# ifdef HAVE_PROCESS_H
#  include <process.h>
# endif
# if defined(_MSC_VER) || defined(__MINGW32__)
#  define POPEN_RT	"rt"
#  define POPEN_WT	"wt"
#  define popen(c,m)	_popen(c,m)
#  define pclose(p)	_pclose(p)
#  define pipe(pfd)	_pipe((pfd),0,_O_BINARY|_O_NOINHERIT)
#  define mkdir(p,m)	_mkdir(p)
#  define setmode(f,m)	_setmode(f,m)
#  define WAIT(s,p,m)	_cwait(s,p,m)
#  define creat(p,m)	_creat(p,m)
#  define read(f,b,s)	_read(f,b,s)
#  define write(f,b,s)	_write(f,b,s)
#  define dup(f)	_dup(f)
#  define dup2(f1,f2)	_dup2(f1,f2)
#  define close(f)	_close(f)
#  define isatty(f)	_isatty(f)
#  define access(p,m)	_access(p,m)
# endif
# define SET_BINARY(f)	do {if (!isatty(f)) setmode(f,O_BINARY);} while(0)
# define FOPEN_RB	"rb"
# define FOPEN_WB	"wb"
# define FOPEN_RWB	"wb+"
# ifndef O_BINARY
#  ifdef _O_BINARY
#   define O_BINARY	(_O_BINARY)
#  endif
# endif

/* The system shell.  Groff assumes a Unixy shell, but non-Posix
   systems don't have standard places where it lives, and might not
   have it installed to begin with.  We want to give them some leeway.  */
# ifdef __EMX__
#  define getcwd(b,s)	_getcwd2(b,s)
# else
#  define BSHELL	(system_shell_name())
#  define BSHELL_DASH_C	(system_shell_dash_c())
#  define IS_BSHELL(s)	(is_system_shell(s))
# endif

/* The separator for directories in PATH and other environment
   variables.  */
# define PATH_SEP	";"
# define PATH_SEP_CHAR	';'

/* Characters that separate directories in a path name.  */
# define DIR_SEPS	"/\\:"

/* How to tell if the argument is an absolute file name.  */
# define IS_ABSOLUTE(f) \
 ((f)[0] == '/' || (f)[0] == '\\' || (f)[0] && (f)[1] == ':')

/* The executable extension.  */
# define EXE_EXT	".exe"

/* Possible executable extensions.  */
# define PATH_EXT	".com;.exe;.bat;.cmd"

/* The system null device.  */
# define NULL_DEV	"NUL"

/* The default place to create temporary files.  */
# ifndef P_tmpdir
#  ifdef _P_tmpdir
#   define P_tmpdir	_P_tmpdir
#  else
#   define P_tmpdir	"c:/temp"
#  endif
# endif

/* Prototypes.  */
# ifdef __cplusplus
  extern "C" {
# endif
    char       * system_shell_name(void);
    const char * system_shell_dash_c(void);
    int		 is_system_shell(const char *);
# ifdef __cplusplus
  }
# endif

#endif

#if defined(_WIN32) && !defined(_UWIN) && !defined(__CYGWIN__)
/* Win32 implementations which use the Microsoft runtime library
 * are prone to hanging when a pipe reader quits with unread data in the pipe.
 * `gtroff' avoids this, by invoking `FLUSH_INPUT_PIPE()', defined as ... */
# define FLUSH_INPUT_PIPE(fd)		      \
 do if (!isatty(fd))			      \
 {					      \
   char drain[BUFSIZ];			      \
   while (read(fd, drain, sizeof(drain)) > 0) \
     ;					      \
 } while (0)

/* The Microsoft runtime library also has a broken argument passing mechanism,
 * which may result in improper grouping of arguments passed to a child process
 * by the `spawn()' family of functions.  In `groff', only the `spawnvp()'
 * function is affected; we work around this defect, by substituting a
 * wrapper function in place of `spawnvp()' calls. */

# ifdef __cplusplus
  extern "C" {
# endif
  int spawnvp_wrapper(int, char *, char **);
# ifdef __cplusplus
  }
# endif
# ifndef SPAWN_FUNCTION_WRAPPERS
#  undef  spawnvp
#  define spawnvp      spawnvp_wrapper
#  undef  _spawnvp
#  define _spawnvp     spawnvp
# endif /* SPAWN_FUNCTION_WRAPPERS */

#else
/* Other implementations do not suffer from Microsoft runtime bugs,
 * but `gtroff' requires a dummy definition for FLUSH_INPUT_PIPE() */
# define FLUSH_INPUT_PIPE(fd)	do {} while(0)
#endif

/* Defaults, for Posix systems.  */

#ifndef SET_BINARY
# define SET_BINARY(f)	do {} while(0)
#endif
#ifndef FOPEN_RB
# define FOPEN_RB	"r"
#endif
#ifndef FOPEN_WB
# define FOPEN_WB	"w"
#endif
#ifndef FOPEN_RWB
# define FOPEN_RWB	"w+"
#endif
#ifndef POPEN_RT
# define POPEN_RT	"r"
#endif
#ifndef POPEN_WT
# define POPEN_WT	"w"
#endif
#ifndef O_BINARY
# define O_BINARY	0
#endif
#ifndef BSHELL
# define BSHELL		"/bin/sh"
#endif
#ifndef BSHELL_DASH_C
# define BSHELL_DASH_C	"-c"
#endif
#ifndef IS_BSHELL
# define IS_BSHELL(s)	((s) && strcmp(s,BSHELL) == 0)
#endif
#ifndef PATH_SEP
# define PATH_SEP	":"
# define PATH_SEP_CHAR	':'
#endif
#ifndef DIR_SEPS
# define DIR_SEPS	"/"
#endif
#ifndef IS_ABSOLUTE
# define IS_ABSOLUTE(f)	((f)[0] == '/')
#endif
#ifndef EXE_EXT
# define EXE_EXT	""
#endif
#ifndef PATH_EXT
# define PATH_EXT	""
#endif
#ifndef NULL_DEV
# define NULL_DEV	"/dev/null"
#endif
#ifndef GS_NAME
# define GS_NAME	"gs"
#endif
#ifndef WAIT
# define WAIT(s,p,m)	wait(s)
#endif
#ifndef _WAIT_CHILD
# define _WAIT_CHILD	0
#endif
