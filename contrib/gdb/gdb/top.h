/* Top level stuff for GDB, the GNU debugger.
   Copyright 1986, 1987, 1988, 1989, 1990, 1991, 1992, 1993, 1994
   Free Software Foundation, Inc.

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

/* From top.c.  */
extern char *line;
extern int linesize;
extern FILE *instream;
extern char gdb_dirbuf[1024];
extern int inhibit_gdbinit;
extern int epoch_interface;
extern char gdbinit[];

/* Generally one should use catch_errors rather than manipulating these
   directly.  The exception is main().  */
#if defined(HAVE_SIGSETJMP)
#define SIGJMP_BUF		sigjmp_buf
#define SIGSETJMP(buf)		sigsetjmp(buf, 1)
#define SIGLONGJMP(buf,val)	siglongjmp(buf,val)
#else
#define SIGJMP_BUF		jmp_buf
#define SIGSETJMP(buf)		setjmp(buf)
#define SIGLONGJMP(buf,val)	longjmp(buf,val)
#endif

extern SIGJMP_BUF error_return;
extern SIGJMP_BUF quit_return;

extern void print_gdb_version PARAMS ((GDB_FILE *));

extern void source_command PARAMS ((char *, int));
extern void cd_command PARAMS ((char *, int));
extern void read_command_file PARAMS ((FILE *));
extern void init_history PARAMS ((void));
extern void command_loop PARAMS ((void));
extern int quit_confirm PARAMS ((void));
extern void quit_force PARAMS ((char *, int));
extern void quit_command PARAMS ((char *, int));

/* This function returns a pointer to the string that is used
   by gdb for its command prompt. */
extern char *get_prompt PARAMS((void));
 
/* This function copies the specified string into the string that
   is used by gdb for its command prompt. */
extern void set_prompt PARAMS ((char *));

/* From random places.  */
extern int mapped_symbol_files;
extern int readnow_symbol_files;
#define	ALL_CLEANUPS	((struct cleanup *)0)
