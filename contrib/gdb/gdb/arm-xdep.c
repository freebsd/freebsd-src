/* OBSOLETE /* Acorn Risc Machine host machine support. */
/* OBSOLETE    Copyright (C) 1988, 1989, 1991 Free Software Foundation, Inc. */
/* OBSOLETE  */
/* OBSOLETE    This file is part of GDB. */
/* OBSOLETE  */
/* OBSOLETE    This program is free software; you can redistribute it and/or modify */
/* OBSOLETE    it under the terms of the GNU General Public License as published by */
/* OBSOLETE    the Free Software Foundation; either version 2 of the License, or */
/* OBSOLETE    (at your option) any later version. */
/* OBSOLETE  */
/* OBSOLETE    This program is distributed in the hope that it will be useful, */
/* OBSOLETE    but WITHOUT ANY WARRANTY; without even the implied warranty of */
/* OBSOLETE    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the */
/* OBSOLETE    GNU General Public License for more details. */
/* OBSOLETE  */
/* OBSOLETE    You should have received a copy of the GNU General Public License */
/* OBSOLETE    along with this program; if not, write to the Free Software */
/* OBSOLETE    Foundation, Inc., 59 Temple Place - Suite 330, */
/* OBSOLETE    Boston, MA 02111-1307, USA.  *x/ */
/* OBSOLETE  */
/* OBSOLETE #include "defs.h" */
/* OBSOLETE #include "frame.h" */
/* OBSOLETE #include "inferior.h" */
/* OBSOLETE #include "opcode/arm.h" */
/* OBSOLETE  */
/* OBSOLETE #include <sys/param.h> */
/* OBSOLETE #include <sys/dir.h> */
/* OBSOLETE #include <signal.h> */
/* OBSOLETE #include <sys/ioctl.h> */
/* OBSOLETE #include <sys/ptrace.h> */
/* OBSOLETE #include <machine/reg.h> */
/* OBSOLETE  */
/* OBSOLETE #define N_TXTADDR(hdr) 0x8000 */
/* OBSOLETE #define N_DATADDR(hdr) (hdr.a_text + 0x8000) */
/* OBSOLETE  */
/* OBSOLETE #include "gdbcore.h" */
/* OBSOLETE  */
/* OBSOLETE #include <sys/user.h>		/* After a.out.h  *x/ */
/* OBSOLETE #include <sys/file.h> */
/* OBSOLETE #include "gdb_stat.h" */
/* OBSOLETE  */
/* OBSOLETE #include <errno.h> */
/* OBSOLETE  */
/* OBSOLETE void */
/* OBSOLETE fetch_inferior_registers (regno) */
/* OBSOLETE      int regno;			/* Original value discarded *x/ */
/* OBSOLETE { */
/* OBSOLETE   register unsigned int regaddr; */
/* OBSOLETE   char buf[MAX_REGISTER_RAW_SIZE]; */
/* OBSOLETE   register int i; */
/* OBSOLETE  */
/* OBSOLETE   struct user u; */
/* OBSOLETE   unsigned int offset = (char *) &u.u_ar0 - (char *) &u; */
/* OBSOLETE   offset = ptrace (PT_READ_U, inferior_pid, (PTRACE_ARG3_TYPE) offset, 0) */
/* OBSOLETE     - KERNEL_U_ADDR; */
/* OBSOLETE  */
/* OBSOLETE   registers_fetched (); */
/* OBSOLETE  */
/* OBSOLETE   for (regno = 0; regno < 16; regno++) */
/* OBSOLETE     { */
/* OBSOLETE       regaddr = offset + regno * 4; */
/* OBSOLETE       *(int *) &buf[0] = ptrace (PT_READ_U, inferior_pid, */
/* OBSOLETE 				 (PTRACE_ARG3_TYPE) regaddr, 0); */
/* OBSOLETE       if (regno == PC_REGNUM) */
/* OBSOLETE 	*(int *) &buf[0] = GET_PC_PART (*(int *) &buf[0]); */
/* OBSOLETE       supply_register (regno, buf); */
/* OBSOLETE     } */
/* OBSOLETE   *(int *) &buf[0] = ptrace (PT_READ_U, inferior_pid, */
/* OBSOLETE 			     (PTRACE_ARG3_TYPE) (offset + PC * 4), 0); */
/* OBSOLETE   supply_register (PS_REGNUM, buf);	/* set virtual register ps same as pc *x/ */
/* OBSOLETE  */
/* OBSOLETE   /* read the floating point registers *x/ */
/* OBSOLETE   offset = (char *) &u.u_fp_regs - (char *) &u; */
/* OBSOLETE   *(int *) buf = ptrace (PT_READ_U, inferior_pid, (PTRACE_ARG3_TYPE) offset, 0); */
/* OBSOLETE   supply_register (FPS_REGNUM, buf); */
/* OBSOLETE   for (regno = 16; regno < 24; regno++) */
/* OBSOLETE     { */
/* OBSOLETE       regaddr = offset + 4 + 12 * (regno - 16); */
/* OBSOLETE       for (i = 0; i < 12; i += sizeof (int)) */
/* OBSOLETE 	 *(int *) &buf[i] = ptrace (PT_READ_U, inferior_pid, */
/* OBSOLETE 				      (PTRACE_ARG3_TYPE) (regaddr + i), 0); */
/* OBSOLETE       supply_register (regno, buf); */
/* OBSOLETE     } */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /* Store our register values back into the inferior. */
/* OBSOLETE    If REGNO is -1, do this for all registers. */
/* OBSOLETE    Otherwise, REGNO specifies which register (so we can save time).  *x/ */
/* OBSOLETE  */
/* OBSOLETE void */
/* OBSOLETE store_inferior_registers (regno) */
/* OBSOLETE      int regno; */
/* OBSOLETE { */
/* OBSOLETE   register unsigned int regaddr; */
/* OBSOLETE   char buf[80]; */
/* OBSOLETE  */
/* OBSOLETE   struct user u; */
/* OBSOLETE   unsigned long value; */
/* OBSOLETE   unsigned int offset = (char *) &u.u_ar0 - (char *) &u; */
/* OBSOLETE   offset = ptrace (PT_READ_U, inferior_pid, (PTRACE_ARG3_TYPE) offset, 0) */
/* OBSOLETE     - KERNEL_U_ADDR; */
/* OBSOLETE  */
/* OBSOLETE   if (regno >= 0) */
/* OBSOLETE     { */
/* OBSOLETE       if (regno >= 16) */
/* OBSOLETE 	return; */
/* OBSOLETE       regaddr = offset + 4 * regno; */
/* OBSOLETE       errno = 0; */
/* OBSOLETE       value = read_register (regno); */
/* OBSOLETE       if (regno == PC_REGNUM) */
/* OBSOLETE 	value = SET_PC_PART (read_register (PS_REGNUM), value); */
/* OBSOLETE       ptrace (PT_WRITE_U, inferior_pid, (PTRACE_ARG3_TYPE) regaddr, value); */
/* OBSOLETE       if (errno != 0) */
/* OBSOLETE 	{ */
/* OBSOLETE 	  sprintf (buf, "writing register number %d", regno); */
/* OBSOLETE 	  perror_with_name (buf); */
/* OBSOLETE 	} */
/* OBSOLETE     } */
/* OBSOLETE   else */
/* OBSOLETE     for (regno = 0; regno < 15; regno++) */
/* OBSOLETE       { */
/* OBSOLETE 	regaddr = offset + regno * 4; */
/* OBSOLETE 	errno = 0; */
/* OBSOLETE 	value = read_register (regno); */
/* OBSOLETE 	if (regno == PC_REGNUM) */
/* OBSOLETE 	  value = SET_PC_PART (read_register (PS_REGNUM), value); */
/* OBSOLETE 	ptrace (6, inferior_pid, (PTRACE_ARG3_TYPE) regaddr, value); */
/* OBSOLETE 	if (errno != 0) */
/* OBSOLETE 	  { */
/* OBSOLETE 	    sprintf (buf, "writing all regs, number %d", regno); */
/* OBSOLETE 	    perror_with_name (buf); */
/* OBSOLETE 	  } */
/* OBSOLETE       } */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /* Work with core dump and executable files, for GDB.  */
/* OBSOLETE    This code would be in corefile.c if it weren't machine-dependent. *x/ */
/* OBSOLETE  */
/* OBSOLETE /* Structure to describe the chain of shared libraries used */
/* OBSOLETE    by the execfile. */
/* OBSOLETE    e.g. prog shares Xt which shares X11 which shares c. *x/ */
/* OBSOLETE  */
/* OBSOLETE struct shared_library */
/* OBSOLETE { */
/* OBSOLETE   struct exec_header header; */
/* OBSOLETE   char name[SHLIBLEN]; */
/* OBSOLETE   CORE_ADDR text_start;		/* CORE_ADDR of 1st byte of text, this file *x/ */
/* OBSOLETE   long data_offset;		/* offset of data section in file *x/ */
/* OBSOLETE   int chan;			/* file descriptor for the file *x/ */
/* OBSOLETE   struct shared_library *shares;	/* library this one shares *x/ */
/* OBSOLETE }; */
/* OBSOLETE static struct shared_library *shlib = 0; */
/* OBSOLETE  */
/* OBSOLETE /* Hook for `exec_file_command' command to call.  *x/ */
/* OBSOLETE  */
/* OBSOLETE extern void (*exec_file_display_hook) (); */
/* OBSOLETE  */
/* OBSOLETE static CORE_ADDR unshared_text_start; */
/* OBSOLETE  */
/* OBSOLETE /* extended header from exec file (for shared library info) *x/ */
/* OBSOLETE  */
/* OBSOLETE static struct exec_header exec_header; */
/* OBSOLETE  */
/* OBSOLETE void */
/* OBSOLETE core_file_command (filename, from_tty) */
/* OBSOLETE      char *filename; */
/* OBSOLETE      int from_tty; */
/* OBSOLETE { */
/* OBSOLETE   int val; */
/* OBSOLETE  */
/* OBSOLETE   /* Discard all vestiges of any previous core file */
/* OBSOLETE      and mark data and stack spaces as empty.  *x/ */
/* OBSOLETE  */
/* OBSOLETE   if (corefile) */
/* OBSOLETE     free (corefile); */
/* OBSOLETE   corefile = 0; */
/* OBSOLETE  */
/* OBSOLETE   if (corechan >= 0) */
/* OBSOLETE     close (corechan); */
/* OBSOLETE   corechan = -1; */
/* OBSOLETE  */
/* OBSOLETE   data_start = 0; */
/* OBSOLETE   data_end = 0; */
/* OBSOLETE   stack_start = STACK_END_ADDR; */
/* OBSOLETE   stack_end = STACK_END_ADDR; */
/* OBSOLETE  */
/* OBSOLETE   /* Now, if a new core file was specified, open it and digest it.  *x/ */
/* OBSOLETE  */
/* OBSOLETE   if (filename) */
/* OBSOLETE     { */
/* OBSOLETE       filename = tilde_expand (filename); */
/* OBSOLETE       make_cleanup (free, filename); */
/* OBSOLETE  */
/* OBSOLETE       if (have_inferior_p ()) */
/* OBSOLETE 	error ("To look at a core file, you must kill the program with \"kill\"."); */
/* OBSOLETE       corechan = open (filename, O_RDONLY, 0); */
/* OBSOLETE       if (corechan < 0) */
/* OBSOLETE 	perror_with_name (filename); */
/* OBSOLETE       /* 4.2-style (and perhaps also sysV-style) core dump file.  *x/ */
/* OBSOLETE       { */
/* OBSOLETE 	struct user u; */
/* OBSOLETE  */
/* OBSOLETE 	unsigned int reg_offset, fp_reg_offset; */
/* OBSOLETE  */
/* OBSOLETE 	val = myread (corechan, &u, sizeof u); */
/* OBSOLETE 	if (val < 0) */
/* OBSOLETE 	  perror_with_name ("Not a core file: reading upage"); */
/* OBSOLETE 	if (val != sizeof u) */
/* OBSOLETE 	  error ("Not a core file: could only read %d bytes", val); */
/* OBSOLETE  */
/* OBSOLETE 	/* We are depending on exec_file_command having been called */
/* OBSOLETE 	   previously to set exec_data_start.  Since the executable */
/* OBSOLETE 	   and the core file share the same text segment, the address */
/* OBSOLETE 	   of the data segment will be the same in both.  *x/ */
/* OBSOLETE 	data_start = exec_data_start; */
/* OBSOLETE  */
/* OBSOLETE 	data_end = data_start + NBPG * u.u_dsize; */
/* OBSOLETE 	stack_start = stack_end - NBPG * u.u_ssize; */
/* OBSOLETE 	data_offset = NBPG * UPAGES; */
/* OBSOLETE 	stack_offset = NBPG * (UPAGES + u.u_dsize); */
/* OBSOLETE  */
/* OBSOLETE 	/* Some machines put an absolute address in here and some put */
/* OBSOLETE 	   the offset in the upage of the regs.  *x/ */
/* OBSOLETE 	reg_offset = (int) u.u_ar0; */
/* OBSOLETE 	if (reg_offset > NBPG * UPAGES) */
/* OBSOLETE 	  reg_offset -= KERNEL_U_ADDR; */
/* OBSOLETE 	fp_reg_offset = (char *) &u.u_fp_regs - (char *) &u; */
/* OBSOLETE  */
/* OBSOLETE 	/* I don't know where to find this info. */
/* OBSOLETE 	   So, for now, mark it as not available.  *x/ */
/* OBSOLETE 	N_SET_MAGIC (core_aouthdr, 0); */
/* OBSOLETE  */
/* OBSOLETE 	/* Read the register values out of the core file and store */
/* OBSOLETE 	   them where `read_register' will find them.  *x/ */
/* OBSOLETE  */
/* OBSOLETE 	{ */
/* OBSOLETE 	  register int regno; */
/* OBSOLETE  */
/* OBSOLETE 	  for (regno = 0; regno < NUM_REGS; regno++) */
/* OBSOLETE 	    { */
/* OBSOLETE 	      char buf[MAX_REGISTER_RAW_SIZE]; */
/* OBSOLETE  */
/* OBSOLETE 	      if (regno < 16) */
/* OBSOLETE 		val = lseek (corechan, reg_offset + 4 * regno, 0); */
/* OBSOLETE 	      else if (regno < 24) */
/* OBSOLETE 		val = lseek (corechan, fp_reg_offset + 4 + 12 * (regno - 24), 0); */
/* OBSOLETE 	      else if (regno == 24) */
/* OBSOLETE 		val = lseek (corechan, fp_reg_offset, 0); */
/* OBSOLETE 	      else if (regno == 25) */
/* OBSOLETE 		val = lseek (corechan, reg_offset + 4 * PC, 0); */
/* OBSOLETE 	      if (val < 0 */
/* OBSOLETE 		  || (val = myread (corechan, buf, sizeof buf)) < 0) */
/* OBSOLETE 		{ */
/* OBSOLETE 		  char *buffer = (char *) alloca (strlen (REGISTER_NAME (regno)) */
/* OBSOLETE 						  + 30); */
/* OBSOLETE 		  strcpy (buffer, "Reading register "); */
/* OBSOLETE 		  strcat (buffer, REGISTER_NAME (regno)); */
/* OBSOLETE  */
/* OBSOLETE 		  perror_with_name (buffer); */
/* OBSOLETE 		} */
/* OBSOLETE  */
/* OBSOLETE 	      if (regno == PC_REGNUM) */
/* OBSOLETE 		*(int *) buf = GET_PC_PART (*(int *) buf); */
/* OBSOLETE 	      supply_register (regno, buf); */
/* OBSOLETE 	    } */
/* OBSOLETE 	} */
/* OBSOLETE       } */
/* OBSOLETE       if (filename[0] == '/') */
/* OBSOLETE 	corefile = savestring (filename, strlen (filename)); */
/* OBSOLETE       else */
/* OBSOLETE 	{ */
/* OBSOLETE 	  corefile = concat (current_directory, "/", filename, NULL); */
/* OBSOLETE 	} */
/* OBSOLETE  */
/* OBSOLETE       flush_cached_frames (); */
/* OBSOLETE       select_frame (get_current_frame (), 0); */
/* OBSOLETE       validate_files (); */
/* OBSOLETE     } */
/* OBSOLETE   else if (from_tty) */
/* OBSOLETE     printf ("No core file now.\n"); */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE #if 0 */
/* OBSOLETE /* Work with core dump and executable files, for GDB.  */
/* OBSOLETE    This code would be in corefile.c if it weren't machine-dependent. *x/ */
/* OBSOLETE  */
/* OBSOLETE /* Structure to describe the chain of shared libraries used */
/* OBSOLETE    by the execfile. */
/* OBSOLETE    e.g. prog shares Xt which shares X11 which shares c. *x/ */
/* OBSOLETE  */
/* OBSOLETE struct shared_library */
/* OBSOLETE { */
/* OBSOLETE   struct exec_header header; */
/* OBSOLETE   char name[SHLIBLEN]; */
/* OBSOLETE   CORE_ADDR text_start;		/* CORE_ADDR of 1st byte of text, this file *x/ */
/* OBSOLETE   long data_offset;		/* offset of data section in file *x/ */
/* OBSOLETE   int chan;			/* file descriptor for the file *x/ */
/* OBSOLETE   struct shared_library *shares;	/* library this one shares *x/ */
/* OBSOLETE }; */
/* OBSOLETE static struct shared_library *shlib = 0; */
/* OBSOLETE  */
/* OBSOLETE /* Hook for `exec_file_command' command to call.  *x/ */
/* OBSOLETE  */
/* OBSOLETE extern void (*exec_file_display_hook) (); */
/* OBSOLETE  */
/* OBSOLETE static CORE_ADDR unshared_text_start; */
/* OBSOLETE  */
/* OBSOLETE /* extended header from exec file (for shared library info) *x/ */
/* OBSOLETE  */
/* OBSOLETE static struct exec_header exec_header; */
/* OBSOLETE  */
/* OBSOLETE void */
/* OBSOLETE exec_file_command (filename, from_tty) */
/* OBSOLETE      char *filename; */
/* OBSOLETE      int from_tty; */
/* OBSOLETE { */
/* OBSOLETE   int val; */
/* OBSOLETE  */
/* OBSOLETE   /* Eliminate all traces of old exec file. */
/* OBSOLETE      Mark text segment as empty.  *x/ */
/* OBSOLETE  */
/* OBSOLETE   if (execfile) */
/* OBSOLETE     free (execfile); */
/* OBSOLETE   execfile = 0; */
/* OBSOLETE   data_start = 0; */
/* OBSOLETE   data_end -= exec_data_start; */
/* OBSOLETE   text_start = 0; */
/* OBSOLETE   unshared_text_start = 0; */
/* OBSOLETE   text_end = 0; */
/* OBSOLETE   exec_data_start = 0; */
/* OBSOLETE   exec_data_end = 0; */
/* OBSOLETE   if (execchan >= 0) */
/* OBSOLETE     close (execchan); */
/* OBSOLETE   execchan = -1; */
/* OBSOLETE   if (shlib) */
/* OBSOLETE     { */
/* OBSOLETE       close_shared_library (shlib); */
/* OBSOLETE       shlib = 0; */
/* OBSOLETE     } */
/* OBSOLETE  */
/* OBSOLETE   /* Now open and digest the file the user requested, if any.  *x/ */
/* OBSOLETE  */
/* OBSOLETE   if (filename) */
/* OBSOLETE     { */
/* OBSOLETE       filename = tilde_expand (filename); */
/* OBSOLETE       make_cleanup (free, filename); */
/* OBSOLETE  */
/* OBSOLETE       execchan = openp (getenv ("PATH"), 1, filename, O_RDONLY, 0, */
/* OBSOLETE 			&execfile); */
/* OBSOLETE       if (execchan < 0) */
/* OBSOLETE 	perror_with_name (filename); */
/* OBSOLETE  */
/* OBSOLETE       { */
/* OBSOLETE 	struct stat st_exec; */
/* OBSOLETE  */
/* OBSOLETE #ifdef HEADER_SEEK_FD */
/* OBSOLETE 	HEADER_SEEK_FD (execchan); */
/* OBSOLETE #endif */
/* OBSOLETE  */
/* OBSOLETE 	val = myread (execchan, &exec_header, sizeof exec_header); */
/* OBSOLETE 	exec_aouthdr = exec_header.a_exec; */
/* OBSOLETE  */
/* OBSOLETE 	if (val < 0) */
/* OBSOLETE 	  perror_with_name (filename); */
/* OBSOLETE  */
/* OBSOLETE 	text_start = 0x8000; */
/* OBSOLETE  */
/* OBSOLETE 	/* Look for shared library if needed *x/ */
/* OBSOLETE 	if (exec_header.a_exec.a_magic & MF_USES_SL) */
/* OBSOLETE 	  shlib = open_shared_library (exec_header.a_shlibname, text_start); */
/* OBSOLETE  */
/* OBSOLETE 	text_offset = N_TXTOFF (exec_aouthdr); */
/* OBSOLETE 	exec_data_offset = N_TXTOFF (exec_aouthdr) + exec_aouthdr.a_text; */
/* OBSOLETE  */
/* OBSOLETE 	if (shlib) */
/* OBSOLETE 	  { */
/* OBSOLETE 	    unshared_text_start = shared_text_end (shlib) & ~0x7fff; */
/* OBSOLETE 	    stack_start = shlib->header.a_exec.a_sldatabase; */
/* OBSOLETE 	    stack_end = STACK_END_ADDR; */
/* OBSOLETE 	  } */
/* OBSOLETE 	else */
/* OBSOLETE 	  unshared_text_start = 0x8000; */
/* OBSOLETE 	text_end = unshared_text_start + exec_aouthdr.a_text; */
/* OBSOLETE  */
/* OBSOLETE 	exec_data_start = unshared_text_start + exec_aouthdr.a_text; */
/* OBSOLETE 	exec_data_end = exec_data_start + exec_aouthdr.a_data; */
/* OBSOLETE  */
/* OBSOLETE 	data_start = exec_data_start; */
/* OBSOLETE 	data_end += exec_data_start; */
/* OBSOLETE  */
/* OBSOLETE 	fstat (execchan, &st_exec); */
/* OBSOLETE 	exec_mtime = st_exec.st_mtime; */
/* OBSOLETE       } */
/* OBSOLETE  */
/* OBSOLETE       validate_files (); */
/* OBSOLETE     } */
/* OBSOLETE   else if (from_tty) */
/* OBSOLETE     printf ("No executable file now.\n"); */
/* OBSOLETE  */
/* OBSOLETE   /* Tell display code (if any) about the changed file name.  *x/ */
/* OBSOLETE   if (exec_file_display_hook) */
/* OBSOLETE     (*exec_file_display_hook) (filename); */
/* OBSOLETE } */
/* OBSOLETE #endif */
/* OBSOLETE  */
/* OBSOLETE #if 0 */
/* OBSOLETE /* Read from the program's memory (except for inferior processes). */
/* OBSOLETE    This function is misnamed, since it only reads, never writes; and */
/* OBSOLETE    since it will use the core file and/or executable file as necessary. */
/* OBSOLETE  */
/* OBSOLETE    It should be extended to write as well as read, FIXME, for patching files. */
/* OBSOLETE  */
/* OBSOLETE    Return 0 if address could be read, EIO if addresss out of bounds.  *x/ */
/* OBSOLETE  */
/* OBSOLETE int */
/* OBSOLETE xfer_core_file (memaddr, myaddr, len) */
/* OBSOLETE      CORE_ADDR memaddr; */
/* OBSOLETE      char *myaddr; */
/* OBSOLETE      int len; */
/* OBSOLETE { */
/* OBSOLETE   register int i; */
/* OBSOLETE   register int val; */
/* OBSOLETE   int xferchan; */
/* OBSOLETE   char **xferfile; */
/* OBSOLETE   int fileptr; */
/* OBSOLETE   int returnval = 0; */
/* OBSOLETE  */
/* OBSOLETE   while (len > 0) */
/* OBSOLETE     { */
/* OBSOLETE       xferfile = 0; */
/* OBSOLETE       xferchan = 0; */
/* OBSOLETE  */
/* OBSOLETE       /* Determine which file the next bunch of addresses reside in, */
/* OBSOLETE          and where in the file.  Set the file's read/write pointer */
/* OBSOLETE          to point at the proper place for the desired address */
/* OBSOLETE          and set xferfile and xferchan for the correct file. */
/* OBSOLETE  */
/* OBSOLETE          If desired address is nonexistent, leave them zero. */
/* OBSOLETE  */
/* OBSOLETE          i is set to the number of bytes that can be handled */
/* OBSOLETE          along with the next address. */
/* OBSOLETE  */
/* OBSOLETE          We put the most likely tests first for efficiency.  *x/ */
/* OBSOLETE  */
/* OBSOLETE       /* Note that if there is no core file */
/* OBSOLETE          data_start and data_end are equal.  *x/ */
/* OBSOLETE       if (memaddr >= data_start && memaddr < data_end) */
/* OBSOLETE 	{ */
/* OBSOLETE 	  i = min (len, data_end - memaddr); */
/* OBSOLETE 	  fileptr = memaddr - data_start + data_offset; */
/* OBSOLETE 	  xferfile = &corefile; */
/* OBSOLETE 	  xferchan = corechan; */
/* OBSOLETE 	} */
/* OBSOLETE       /* Note that if there is no core file */
/* OBSOLETE          stack_start and stack_end define the shared library data.  *x/ */
/* OBSOLETE       else if (memaddr >= stack_start && memaddr < stack_end) */
/* OBSOLETE 	{ */
/* OBSOLETE 	  if (corechan < 0) */
/* OBSOLETE 	    { */
/* OBSOLETE 	      struct shared_library *lib; */
/* OBSOLETE 	      for (lib = shlib; lib; lib = lib->shares) */
/* OBSOLETE 		if (memaddr >= lib->header.a_exec.a_sldatabase && */
/* OBSOLETE 		    memaddr < lib->header.a_exec.a_sldatabase + */
/* OBSOLETE 		    lib->header.a_exec.a_data) */
/* OBSOLETE 		  break; */
/* OBSOLETE 	      if (lib) */
/* OBSOLETE 		{ */
/* OBSOLETE 		  i = min (len, lib->header.a_exec.a_sldatabase + */
/* OBSOLETE 			   lib->header.a_exec.a_data - memaddr); */
/* OBSOLETE 		  fileptr = lib->data_offset + memaddr - */
/* OBSOLETE 		    lib->header.a_exec.a_sldatabase; */
/* OBSOLETE 		  xferfile = execfile; */
/* OBSOLETE 		  xferchan = lib->chan; */
/* OBSOLETE 		} */
/* OBSOLETE 	    } */
/* OBSOLETE 	  else */
/* OBSOLETE 	    { */
/* OBSOLETE 	      i = min (len, stack_end - memaddr); */
/* OBSOLETE 	      fileptr = memaddr - stack_start + stack_offset; */
/* OBSOLETE 	      xferfile = &corefile; */
/* OBSOLETE 	      xferchan = corechan; */
/* OBSOLETE 	    } */
/* OBSOLETE 	} */
/* OBSOLETE       else if (corechan < 0 */
/* OBSOLETE 	       && memaddr >= exec_data_start && memaddr < exec_data_end) */
/* OBSOLETE 	{ */
/* OBSOLETE 	  i = min (len, exec_data_end - memaddr); */
/* OBSOLETE 	  fileptr = memaddr - exec_data_start + exec_data_offset; */
/* OBSOLETE 	  xferfile = &execfile; */
/* OBSOLETE 	  xferchan = execchan; */
/* OBSOLETE 	} */
/* OBSOLETE       else if (memaddr >= text_start && memaddr < text_end) */
/* OBSOLETE 	{ */
/* OBSOLETE 	  struct shared_library *lib; */
/* OBSOLETE 	  for (lib = shlib; lib; lib = lib->shares) */
/* OBSOLETE 	    if (memaddr >= lib->text_start && */
/* OBSOLETE 		memaddr < lib->text_start + lib->header.a_exec.a_text) */
/* OBSOLETE 	      break; */
/* OBSOLETE 	  if (lib) */
/* OBSOLETE 	    { */
/* OBSOLETE 	      i = min (len, lib->header.a_exec.a_text + */
/* OBSOLETE 		       lib->text_start - memaddr); */
/* OBSOLETE 	      fileptr = memaddr - lib->text_start + text_offset; */
/* OBSOLETE 	      xferfile = &execfile; */
/* OBSOLETE 	      xferchan = lib->chan; */
/* OBSOLETE 	    } */
/* OBSOLETE 	  else */
/* OBSOLETE 	    { */
/* OBSOLETE 	      i = min (len, text_end - memaddr); */
/* OBSOLETE 	      fileptr = memaddr - unshared_text_start + text_offset; */
/* OBSOLETE 	      xferfile = &execfile; */
/* OBSOLETE 	      xferchan = execchan; */
/* OBSOLETE 	    } */
/* OBSOLETE 	} */
/* OBSOLETE       else if (memaddr < text_start) */
/* OBSOLETE 	{ */
/* OBSOLETE 	  i = min (len, text_start - memaddr); */
/* OBSOLETE 	} */
/* OBSOLETE       else if (memaddr >= text_end */
/* OBSOLETE 	       && memaddr < (corechan >= 0 ? data_start : exec_data_start)) */
/* OBSOLETE 	{ */
/* OBSOLETE 	  i = min (len, data_start - memaddr); */
/* OBSOLETE 	} */
/* OBSOLETE       else if (corechan >= 0 */
/* OBSOLETE 	       && memaddr >= data_end && memaddr < stack_start) */
/* OBSOLETE 	{ */
/* OBSOLETE 	  i = min (len, stack_start - memaddr); */
/* OBSOLETE 	} */
/* OBSOLETE       else if (corechan < 0 && memaddr >= exec_data_end) */
/* OBSOLETE 	{ */
/* OBSOLETE 	  i = min (len, -memaddr); */
/* OBSOLETE 	} */
/* OBSOLETE       else if (memaddr >= stack_end && stack_end != 0) */
/* OBSOLETE 	{ */
/* OBSOLETE 	  i = min (len, -memaddr); */
/* OBSOLETE 	} */
/* OBSOLETE       else */
/* OBSOLETE 	{ */
/* OBSOLETE 	  /* Address did not classify into one of the known ranges. */
/* OBSOLETE 	     This shouldn't happen; we catch the endpoints.  *x/ */
/* OBSOLETE 	  internal_error ("Bad case logic in xfer_core_file."); */
/* OBSOLETE 	} */
/* OBSOLETE  */
/* OBSOLETE       /* Now we know which file to use. */
/* OBSOLETE          Set up its pointer and transfer the data.  *x/ */
/* OBSOLETE       if (xferfile) */
/* OBSOLETE 	{ */
/* OBSOLETE 	  if (*xferfile == 0) */
/* OBSOLETE 	    if (xferfile == &execfile) */
/* OBSOLETE 	      error ("No program file to examine."); */
/* OBSOLETE 	    else */
/* OBSOLETE 	      error ("No core dump file or running program to examine."); */
/* OBSOLETE 	  val = lseek (xferchan, fileptr, 0); */
/* OBSOLETE 	  if (val < 0) */
/* OBSOLETE 	    perror_with_name (*xferfile); */
/* OBSOLETE 	  val = myread (xferchan, myaddr, i); */
/* OBSOLETE 	  if (val < 0) */
/* OBSOLETE 	    perror_with_name (*xferfile); */
/* OBSOLETE 	} */
/* OBSOLETE       /* If this address is for nonexistent memory, */
/* OBSOLETE          read zeros if reading, or do nothing if writing. */
/* OBSOLETE          Actually, we never right.  *x/ */
/* OBSOLETE       else */
/* OBSOLETE 	{ */
/* OBSOLETE 	  memset (myaddr, '\0', i); */
/* OBSOLETE 	  returnval = EIO; */
/* OBSOLETE 	} */
/* OBSOLETE  */
/* OBSOLETE       memaddr += i; */
/* OBSOLETE       myaddr += i; */
/* OBSOLETE       len -= i; */
/* OBSOLETE     } */
/* OBSOLETE   return returnval; */
/* OBSOLETE } */
/* OBSOLETE #endif */
