/* Remote target system call callback support.
   Copyright 1997 Free Software Foundation, Inc.

This file is part of GDB.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef CALLBACK_H
#define CALLBACK_H

#ifndef va_start
#include <ansidecl.h>
#ifdef ANSI_PROTOTYPES
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#endif

typedef struct host_callback_struct host_callback;

#define MAX_CALLBACK_FDS 10

struct host_callback_struct 
{
  int (*close) PARAMS ((host_callback *,int));
  int (*get_errno) PARAMS ((host_callback *));
  int (*isatty) PARAMS ((host_callback *, int));
  int (*lseek) PARAMS ((host_callback *, int, long , int));
  int (*open) PARAMS ((host_callback *, const char*, int mode));
  int (*read) PARAMS ((host_callback *,int,  char *, int));
  int (*read_stdin) PARAMS (( host_callback *, char *, int));
  int (*rename) PARAMS ((host_callback *, const char *, const char *));
  int (*system) PARAMS ((host_callback *, const char *));
  long (*time) PARAMS ((host_callback *, long *));
  int (*unlink) PARAMS ((host_callback *, const char *));
  int (*write) PARAMS ((host_callback *,int, const char *, int));
  int (*write_stdout) PARAMS ((host_callback *, const char *, int));
  void (*flush_stdout) PARAMS ((host_callback *));
  int (*write_stderr) PARAMS ((host_callback *, const char *, int));
  void (*flush_stderr) PARAMS ((host_callback *));

  /* Used when the target has gone away, so we can close open
     handles and free memory etc etc.  */
  int (*shutdown) PARAMS ((host_callback *));
  int (*init)     PARAMS ((host_callback *));

  /* depreciated, use vprintf_filtered - Talk to the user on a console.  */
  void (*printf_filtered) PARAMS ((host_callback *, const char *, ...));

  /* Talk to the user on a console.
     The `void *' is actually `va_list *'.  */
  void (*vprintf_filtered) PARAMS ((host_callback *, const char *, va_list));

  /* Same as vprintf_filtered but to stderr.  */
  void (*evprintf_filtered) PARAMS ((host_callback *, const char *, va_list));

  /* Print an error message and "exit".
     In the case of gdb "exiting" means doing a longjmp back to the main
     command loop.  */
  void (*error) PARAMS ((host_callback *, const char *, ...));

  int last_errno;		/* host format */

  int fdmap[MAX_CALLBACK_FDS];
  char fdopen[MAX_CALLBACK_FDS];
  char alwaysopen[MAX_CALLBACK_FDS];
};

extern host_callback default_callback;

/* Mapping of host/target values.  */
/* ??? For debugging purposes, one might want to add a string of the
   name of the symbol.  */

typedef struct {
  int host_val;
  int target_val;
} target_defs_map;

extern target_defs_map errno_map[];
extern target_defs_map open_map[];

extern int host_to_target_errno PARAMS ((int));
extern int target_to_host_open PARAMS ((int));

#endif
