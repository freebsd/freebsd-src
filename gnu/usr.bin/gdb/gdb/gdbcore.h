/* Machine independent variables that describe the core file under GDB.
   Copyright 1986, 1987, 1989, 1990, 1992 Free Software Foundation, Inc.

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
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* Interface routines for core, executable, etc.  */

#if !defined (GDBCORE_H)
#define GDBCORE_H 1

#include "bfd.h"		/* Binary File Description */

/* Return the name of the executable file as a string.
   ERR nonzero means get error if there is none specified;
   otherwise return 0 in that case.  */

extern char *
get_exec_file PARAMS ((int err));

/* Nonzero if there is a core file.  */

extern int
have_core_file_p PARAMS ((void));

/* Read "memory data" from whatever target or inferior we have. 
   Returns zero if successful, errno value if not.  EIO is used
   for address out of bounds.  If breakpoints are inserted, returns
   shadow contents, not the breakpoints themselves.  From breakpoint.c.  */

extern int
read_memory_nobpt PARAMS ((CORE_ADDR memaddr, char *myaddr, unsigned len));

/* Report a memory error with error().  */

extern void
memory_error PARAMS ((int status, CORE_ADDR memaddr));

/* Like target_read_memory, but report an error if can't read.  */

extern void
read_memory PARAMS ((CORE_ADDR memaddr, char *myaddr, int len));

/* Read an integer from debugged memory, given address and number of bytes.  */

extern LONGEST
read_memory_integer PARAMS ((CORE_ADDR memaddr, int len));

/* Read an unsigned integer from debugged memory, given address and number of bytes.  */

extern unsigned LONGEST
read_memory_unsigned_integer PARAMS ((CORE_ADDR memaddr, int len));

/* If this is prototyped, need to deal with void* vs. char*.  */

extern void
write_memory PARAMS ((CORE_ADDR memaddr, char *myaddr, int len));

/* Hook for `exec_file_command' command to call.  */

extern void (*exec_file_display_hook) PARAMS ((char *filename));
   
extern void
specify_exec_file_hook PARAMS ((void (*hook) (char *filename)));

/* Binary File Diddlers for the exec and core files */
extern bfd *core_bfd;
extern bfd *exec_bfd;

/* Whether to open exec and core files read-only or read-write.  */

extern int write_files;

extern void
core_file_command PARAMS ((char *filename, int from_tty));

extern void
exec_file_command PARAMS ((char *filename, int from_tty));

extern void
validate_files PARAMS ((void));

extern unsigned int
register_addr PARAMS ((int regno, int blockend));

extern int
xfer_core_file PARAMS ((CORE_ADDR memaddr, char *myaddr, int len));

extern void
fetch_core_registers PARAMS ((char *core_reg_sect, unsigned core_reg_size,
			      int which, unsigned int reg_addr));

extern void
registers_fetched PARAMS ((void));

#if !defined (KERNEL_U_ADDR)
extern CORE_ADDR kernel_u_addr;
#define KERNEL_U_ADDR kernel_u_addr
#endif

/* The target vector for core files */
extern struct target_ops core_ops;

 /* target vector functions called directly from elsewhere */
void
core_open PARAMS ((char *, int));

void
core_detach PARAMS ((char *, int));

/* The current default bfd target.  */
extern char *gnutarget;

extern void set_gnutarget PARAMS ((char *));

#endif	/* !defined (GDBCORE_H) */
