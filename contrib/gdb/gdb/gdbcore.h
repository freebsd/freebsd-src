/* Machine independent variables that describe the core file under GDB.
   Copyright 1986, 1987, 1989, 1990, 1992, 1995 Free Software Foundation, Inc.

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

/* Interface routines for core, executable, etc.  */

#if !defined (GDBCORE_H)
#define GDBCORE_H 1

#include "bfd.h"

/* Return the name of the executable file as a string.
   ERR nonzero means get error if there is none specified;
   otherwise return 0 in that case.  */

extern char *get_exec_file PARAMS ((int err));

/* Nonzero if there is a core file.  */

extern int have_core_file_p PARAMS ((void));

/* Read "memory data" from whatever target or inferior we have.
   Returns zero if successful, errno value if not.  EIO is used for
   address out of bounds.  If breakpoints are inserted, returns shadow
   contents, not the breakpoints themselves.  From breakpoint.c.  */

extern int read_memory_nobpt PARAMS ((CORE_ADDR memaddr, char *myaddr,
				      unsigned len));

/* Report a memory error with error().  */

extern void memory_error PARAMS ((int status, CORE_ADDR memaddr));

/* Like target_read_memory, but report an error if can't read.  */

extern void read_memory PARAMS ((CORE_ADDR memaddr, char *myaddr, int len));

/* Read an integer from debugged memory, given address and number of
   bytes.  */

extern LONGEST read_memory_integer PARAMS ((CORE_ADDR memaddr, int len));

/* Read an unsigned integer from debugged memory, given address and
   number of bytes.  */

extern unsigned LONGEST read_memory_unsigned_integer PARAMS ((CORE_ADDR memaddr, int len));

/* This takes a char *, not void *.  This is probably right, because
   passing in an int * or whatever is wrong with respect to
   byteswapping, alignment, different sizes for host vs. target types,
   etc.  */

extern void write_memory PARAMS ((CORE_ADDR memaddr, char *myaddr, int len));

extern void generic_search PARAMS ((int len, char *data, char *mask,
				    CORE_ADDR startaddr, int increment,
				    CORE_ADDR lorange, CORE_ADDR hirange,
				    CORE_ADDR *addr_found, char *data_found));

/* Hook for `exec_file_command' command to call.  */

extern void (*exec_file_display_hook) PARAMS ((char *filename));
   
extern void specify_exec_file_hook PARAMS ((void (*hook) (char *filename)));

/* Binary File Diddlers for the exec and core files */

extern bfd *core_bfd;
extern bfd *exec_bfd;

/* Whether to open exec and core files read-only or read-write.  */

extern int write_files;

extern void core_file_command PARAMS ((char *filename, int from_tty));

extern void exec_file_command PARAMS ((char *filename, int from_tty));

extern void validate_files PARAMS ((void));

extern unsigned int register_addr PARAMS ((int regno, int blockend));

extern void registers_fetched PARAMS ((void));

#if !defined (KERNEL_U_ADDR)
extern CORE_ADDR kernel_u_addr;
#define KERNEL_U_ADDR kernel_u_addr
#endif

/* The target vector for core files. */

extern struct target_ops core_ops;

/* The current default bfd target.  */

extern char *gnutarget;

extern void set_gnutarget PARAMS ((char *));

/* Structure to keep track of core register reading functions for
   various core file types.  */

struct core_fns {

  /* BFD flavour that we handle.  Note that bfd_target_unknown_flavour matches
     anything, and if there is no better match, this function will be called
     as the default. */

  enum bfd_flavour core_flavour;

  /* Extract the register values out of the core file and store them where
     `read_register' will find them.

     CORE_REG_SECT points to the register values themselves, read into
     memory.

     CORE_REG_SIZE is the size of that area.

     WHICH says which set of registers we are handling (0 = int, 2 = float on
     machines where they are discontiguous).

     REG_ADDR is the offset from u.u_ar0 to the register values relative to
     core_reg_sect.  This is used with old-fashioned core files to locate the
     registers in a large upage-plus-stack ".reg" section.  Original upage
     address X is at location core_reg_sect+x+reg_addr. */

  void (*core_read_registers) PARAMS ((char *core_reg_sect, unsigned core_reg_size,
				  int which, unsigned reg_addr));

  /* Finds the next struct core_fns.  They are allocated and initialized
     in whatever module implements the functions pointed to; an 
     initializer calls add_core_fns to add them to the global chain.  */

  struct core_fns *next;

};

extern void add_core_fns PARAMS ((struct core_fns *cf));

#endif	/* !defined (GDBCORE_H) */
