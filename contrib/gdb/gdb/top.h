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
extern jmp_buf error_return;
extern jmp_buf quit_return;

extern void print_gdb_version PARAMS ((GDB_FILE *));
extern void print_gnu_advertisement PARAMS ((void));

extern void source_command PARAMS ((char *, int));
extern void cd_command PARAMS ((char *, int));
extern void read_command_file PARAMS ((FILE *));
extern void init_history PARAMS ((void));
extern void command_loop PARAMS ((void));
extern void quit_command PARAMS ((char *, int));

/* From random places.  */
extern int mapped_symbol_files;
extern int readnow_symbol_files;
#define	ALL_CLEANUPS	((struct cleanup *)0)
