/* IBM RS/6000 native-dependent code for GDB, the GNU debugger.
   Copyright 1986, 1987, 1989, 1991, 1992, 1994, 1995, 1996
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

#include "defs.h"
#include "inferior.h"
#include "target.h"
#include "gdbcore.h"
#include "xcoffsolib.h"
#include "symfile.h"
#include "objfiles.h"
#include "libbfd.h"  /* For bfd_cache_lookup (FIXME) */
#include "bfd.h"
#include "gdb-stabs.h"

#include <sys/ptrace.h>
#include <sys/reg.h>

#include <sys/param.h>
#include <sys/dir.h>
#include <sys/user.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include <a.out.h>
#include <sys/file.h>
#include "gdb_stat.h"
#include <sys/core.h>
#include <sys/ldr.h>

extern int errno;

extern struct vmap * map_vmap PARAMS ((bfd *bf, bfd *arch));

extern struct target_ops exec_ops;

static void
exec_one_dummy_insn PARAMS ((void));

extern void
add_text_to_loadinfo PARAMS ((CORE_ADDR textaddr, CORE_ADDR dataaddr));

extern void
fixup_breakpoints PARAMS ((CORE_ADDR low, CORE_ADDR high, CORE_ADDR delta));

/* Conversion from gdb-to-system special purpose register numbers.. */

static int special_regs[] = {
  IAR,				/* PC_REGNUM	*/
  MSR,				/* PS_REGNUM	*/
  CR,				/* CR_REGNUM	*/
  LR,				/* LR_REGNUM	*/
  CTR,				/* CTR_REGNUM	*/
  XER,				/* XER_REGNUM   */
  MQ				/* MQ_REGNUM	*/
};

void
fetch_inferior_registers (regno)
  int regno;
{
  int ii;
  extern char registers[];

  if (regno < 0) {			/* for all registers */

    /* read 32 general purpose registers. */

    for (ii=0; ii < 32; ++ii)
      *(int*)&registers[REGISTER_BYTE (ii)] = 
	ptrace (PT_READ_GPR, inferior_pid, (PTRACE_ARG3_TYPE) ii, 0, 0);

    /* read general purpose floating point registers. */

    for (ii=0; ii < 32; ++ii)
      ptrace (PT_READ_FPR, inferior_pid, 
	      (PTRACE_ARG3_TYPE) &registers [REGISTER_BYTE (FP0_REGNUM+ii)],
	      FPR0+ii, 0);

    /* read special registers. */
    for (ii=0; ii <= LAST_SP_REGNUM-FIRST_SP_REGNUM; ++ii)
      *(int*)&registers[REGISTER_BYTE (FIRST_SP_REGNUM+ii)] = 
	ptrace (PT_READ_GPR, inferior_pid, (PTRACE_ARG3_TYPE) special_regs[ii],
		0, 0);

    registers_fetched ();
    return;
  }

  /* else an individual register is addressed. */

  else if (regno < FP0_REGNUM) {		/* a GPR */
    *(int*)&registers[REGISTER_BYTE (regno)] =
	ptrace (PT_READ_GPR, inferior_pid, (PTRACE_ARG3_TYPE) regno, 0, 0);
  }
  else if (regno <= FPLAST_REGNUM) {		/* a FPR */
    ptrace (PT_READ_FPR, inferior_pid,
	    (PTRACE_ARG3_TYPE) &registers [REGISTER_BYTE (regno)],
	    (regno-FP0_REGNUM+FPR0), 0);
  }
  else if (regno <= LAST_SP_REGNUM) {		/* a special register */
    *(int*)&registers[REGISTER_BYTE (regno)] =
	ptrace (PT_READ_GPR, inferior_pid,
		(PTRACE_ARG3_TYPE) special_regs[regno-FIRST_SP_REGNUM], 0, 0);
  }
  else
    fprintf_unfiltered (gdb_stderr, "gdb error: register no %d not implemented.\n", regno);

  register_valid [regno] = 1;
}

/* Store our register values back into the inferior.
   If REGNO is -1, do this for all registers.
   Otherwise, REGNO specifies which register (so we can save time).  */

void
store_inferior_registers (regno)
     int regno;
{
  extern char registers[];

  errno = 0;

  if (regno == -1)
    {			/* for all registers..	*/
      int ii;

       /* execute one dummy instruction (which is a breakpoint) in inferior
          process. So give kernel a chance to do internal house keeping.
	  Otherwise the following ptrace(2) calls will mess up user stack
	  since kernel will get confused about the bottom of the stack (%sp) */

       exec_one_dummy_insn ();

      /* write general purpose registers first! */
      for ( ii=GPR0; ii<=GPR31; ++ii)
	{
	  ptrace (PT_WRITE_GPR, inferior_pid, (PTRACE_ARG3_TYPE) ii,
		  *(int*)&registers[REGISTER_BYTE (ii)], 0);
	  if (errno)
	    { 
	      perror ("ptrace write_gpr");
	      errno = 0;
	    }
	}

      /* write floating point registers now. */
      for ( ii=0; ii < 32; ++ii)
	{
	  ptrace (PT_WRITE_FPR, inferior_pid, 
		  (PTRACE_ARG3_TYPE) &registers[REGISTER_BYTE (FP0_REGNUM+ii)],
		  FPR0+ii, 0);
	  if (errno)
	    {
	      perror ("ptrace write_fpr");
	      errno = 0;
	    }
	}

      /* write special registers. */
      for (ii=0; ii <= LAST_SP_REGNUM-FIRST_SP_REGNUM; ++ii)
	{
	  ptrace (PT_WRITE_GPR, inferior_pid,
		  (PTRACE_ARG3_TYPE) special_regs[ii],
		  *(int*)&registers[REGISTER_BYTE (FIRST_SP_REGNUM+ii)], 0);
	  if (errno)
	    {
	      perror ("ptrace write_gpr");
	      errno = 0;
	    }
	}
    }

  /* else, a specific register number is given... */

  else if (regno < FP0_REGNUM)			/* a GPR */
    {
      ptrace (PT_WRITE_GPR, inferior_pid, (PTRACE_ARG3_TYPE) regno,
	      *(int*)&registers[REGISTER_BYTE (regno)], 0);
    }

  else if (regno <= FPLAST_REGNUM)		/* a FPR */
    {
      ptrace (PT_WRITE_FPR, inferior_pid, 
	      (PTRACE_ARG3_TYPE) &registers[REGISTER_BYTE (regno)],
	      regno - FP0_REGNUM + FPR0, 0);
    }

  else if (regno <= LAST_SP_REGNUM)		/* a special register */
    {
      ptrace (PT_WRITE_GPR, inferior_pid,
	      (PTRACE_ARG3_TYPE) special_regs [regno-FIRST_SP_REGNUM],
	      *(int*)&registers[REGISTER_BYTE (regno)], 0);
    }

  else
    fprintf_unfiltered (gdb_stderr, "Gdb error: register no %d not implemented.\n", regno);

  if (errno)
    {
      perror ("ptrace write");
      errno = 0;
    }
}

/* Execute one dummy breakpoint instruction.  This way we give the kernel
   a chance to do some housekeeping and update inferior's internal data,
   including u_area. */

static void
exec_one_dummy_insn ()
{
#define	DUMMY_INSN_ADDR	(TEXT_SEGMENT_BASE)+0x200

  char shadow_contents[BREAKPOINT_MAX];	/* Stash old bkpt addr contents */
  unsigned int status, pid;
  CORE_ADDR prev_pc;

  /* We plant one dummy breakpoint into DUMMY_INSN_ADDR address. We assume that
     this address will never be executed again by the real code. */

  target_insert_breakpoint (DUMMY_INSN_ADDR, shadow_contents);

  errno = 0;

  /* You might think this could be done with a single ptrace call, and
     you'd be correct for just about every platform I've ever worked
     on.  However, rs6000-ibm-aix4.1.3 seems to have screwed this up --
     the inferior never hits the breakpoint (it's also worth noting
     powerpc-ibm-aix4.1.3 works correctly).  */
  prev_pc = read_pc ();
  write_pc (DUMMY_INSN_ADDR);
  ptrace (PT_CONTINUE, inferior_pid, (PTRACE_ARG3_TYPE)1, 0, 0);

  if (errno)
    perror ("pt_continue");

  do {
    pid = wait (&status);
  } while (pid != inferior_pid);
    
  write_pc (prev_pc);
  target_remove_breakpoint (DUMMY_INSN_ADDR, shadow_contents);
}

static void
fetch_core_registers (core_reg_sect, core_reg_size, which, reg_addr)
     char *core_reg_sect;
     unsigned core_reg_size;
     int which;
     unsigned int reg_addr;	/* Unused in this version */
{
  /* fetch GPRs and special registers from the first register section
     in core bfd. */
  if (which == 0)
    {
      /* copy GPRs first. */
      memcpy (registers, core_reg_sect, 32 * 4);

      /* gdb's internal register template and bfd's register section layout
	 should share a common include file. FIXMEmgo */
      /* then comes special registes. They are supposed to be in the same
	 order in gdb template and bfd `.reg' section. */
      core_reg_sect += (32 * 4);
      memcpy (&registers [REGISTER_BYTE (FIRST_SP_REGNUM)], core_reg_sect, 
	      (LAST_SP_REGNUM - FIRST_SP_REGNUM + 1) * 4);
    }

  /* fetch floating point registers from register section 2 in core bfd. */
  else if (which == 2)
    memcpy (&registers [REGISTER_BYTE (FP0_REGNUM)], core_reg_sect, 32 * 8);

  else
    fprintf_unfiltered (gdb_stderr, "Gdb error: unknown parameter to fetch_core_registers().\n");
}

/* handle symbol translation on vmapping */

static void
vmap_symtab (vp)
     register struct vmap *vp;
{
  register struct objfile *objfile;
  CORE_ADDR text_delta;
  CORE_ADDR data_delta;
  CORE_ADDR bss_delta;
  struct section_offsets *new_offsets;
  int i;
  
  objfile = vp->objfile;
  if (objfile == NULL)
    {
      /* OK, it's not an objfile we opened ourselves.
	 Currently, that can only happen with the exec file, so
	 relocate the symbols for the symfile.  */
      if (symfile_objfile == NULL)
	return;
      objfile = symfile_objfile;
    }

  new_offsets = alloca
    (sizeof (struct section_offsets)
     + sizeof (new_offsets->offsets) * objfile->num_sections);

  for (i = 0; i < objfile->num_sections; ++i)
    ANOFFSET (new_offsets, i) = ANOFFSET (objfile->section_offsets, i);
  
  text_delta =
    vp->tstart - ANOFFSET (objfile->section_offsets, SECT_OFF_TEXT);
  ANOFFSET (new_offsets, SECT_OFF_TEXT) = vp->tstart;

  data_delta =
    vp->dstart - ANOFFSET (objfile->section_offsets, SECT_OFF_DATA);
  ANOFFSET (new_offsets, SECT_OFF_DATA) = vp->dstart;
  
  bss_delta =
    vp->dstart - ANOFFSET (objfile->section_offsets, SECT_OFF_BSS);
  ANOFFSET (new_offsets, SECT_OFF_BSS) = vp->dstart;

  objfile_relocate (objfile, new_offsets);
}

/* Add symbols for an objfile.  */

static int
objfile_symbol_add (arg)
     char *arg;
{
  struct objfile *obj = (struct objfile *) arg;

  syms_from_objfile (obj, 0, 0, 0);
  new_symfile_objfile (obj, 0, 0);
  return 1;
}

/* Add a new vmap entry based on ldinfo() information.

   If ldi->ldinfo_fd is not valid (e.g. this struct ld_info is from a
   core file), the caller should set it to -1, and we will open the file.

   Return the vmap new entry.  */

static struct vmap *
add_vmap (ldi)
     register struct ld_info *ldi; 
{
  bfd *abfd, *last;
  register char *mem, *objname;
  struct objfile *obj;
  struct vmap *vp;

  /* This ldi structure was allocated using alloca() in 
     xcoff_relocate_symtab(). Now we need to have persistent object 
     and member names, so we should save them. */

  mem = ldi->ldinfo_filename + strlen (ldi->ldinfo_filename) + 1;
  mem = savestring (mem, strlen (mem));
  objname = savestring (ldi->ldinfo_filename, strlen (ldi->ldinfo_filename));

  if (ldi->ldinfo_fd < 0)
    /* Note that this opens it once for every member; a possible
       enhancement would be to only open it once for every object.  */
    abfd = bfd_openr (objname, gnutarget);
  else
    abfd = bfd_fdopenr (objname, gnutarget, ldi->ldinfo_fd);
  if (!abfd)
    error ("Could not open `%s' as an executable file: %s",
	   objname, bfd_errmsg (bfd_get_error ()));

  /* make sure we have an object file */

  if (bfd_check_format (abfd, bfd_object))
    vp = map_vmap (abfd, 0);

  else if (bfd_check_format (abfd, bfd_archive))
    {
      last = 0;
      /* FIXME??? am I tossing BFDs?  bfd? */
      while ((last = bfd_openr_next_archived_file (abfd, last)))
	if (STREQ (mem, last->filename))
	  break;

      if (!last)
	{
	  bfd_close (abfd);
	  /* FIXME -- should be error */
	  warning ("\"%s\": member \"%s\" missing.", abfd->filename, mem);
	  return;
	}

      if (!bfd_check_format(last, bfd_object))
	{
	  bfd_close (last);	/* XXX???	*/
	  goto obj_err;
	}

      vp = map_vmap (last, abfd);
    }
  else
    {
    obj_err:
      bfd_close (abfd);
      error ("\"%s\": not in executable format: %s.",
	     objname, bfd_errmsg (bfd_get_error ()));
      /*NOTREACHED*/
    }
  obj = allocate_objfile (vp->bfd, 0);
  vp->objfile = obj;

#ifndef SOLIB_SYMBOLS_MANUAL
  if (catch_errors (objfile_symbol_add, (char *)obj,
		    "Error while reading shared library symbols:\n",
		    RETURN_MASK_ALL))
    {
      /* Note this is only done if symbol reading was successful.  */
      vmap_symtab (vp);
      vp->loaded = 1;
    }
#endif
  return vp;
}

/* update VMAP info with ldinfo() information
   Input is ptr to ldinfo() results.  */

static void
vmap_ldinfo (ldi)
     register struct ld_info *ldi;
{
  struct stat ii, vi;
  register struct vmap *vp;
  int got_one, retried;
  int got_exec_file;

  /* For each *ldi, see if we have a corresponding *vp.
     If so, update the mapping, and symbol table.
     If not, add an entry and symbol table.  */

  do {
    char *name = ldi->ldinfo_filename;
    char *memb = name + strlen(name) + 1;

    retried = 0;

    if (fstat (ldi->ldinfo_fd, &ii) < 0)
      fatal ("cannot fstat(fd=%d) on %s", ldi->ldinfo_fd, name);
  retry:
    for (got_one = 0, vp = vmap; vp; vp = vp->nxt)
      {
	/* First try to find a `vp', which is the same as in ldinfo.
	   If not the same, just continue and grep the next `vp'. If same,
	   relocate its tstart, tend, dstart, dend values. If no such `vp'
	   found, get out of this for loop, add this ldi entry as a new vmap
	   (add_vmap) and come back, fins its `vp' and so on... */

	/* The filenames are not always sufficient to match on. */

	if ((name[0] == '/' && !STREQ(name, vp->name))
	    || (memb[0] && !STREQ(memb, vp->member)))
	  continue;

	/* See if we are referring to the same file. */
	if (bfd_stat (vp->bfd, &vi) < 0)
	  /* An error here is innocuous, most likely meaning that
	     the file descriptor has become worthless.
	     FIXME: What does it mean for a file descriptor to become
	     "worthless"?  What makes it happen?  What error does it
	     produce (ENOENT? others?)?  Should we at least provide
	     a warning?  */
	  continue;

	if (ii.st_dev != vi.st_dev || ii.st_ino != vi.st_ino)
	  continue;

	if (!retried)
	  close (ldi->ldinfo_fd);

	++got_one;

	/* Found a corresponding VMAP.  Remap!  */

	/* We can assume pointer == CORE_ADDR, this code is native only.  */
	vp->tstart = (CORE_ADDR) ldi->ldinfo_textorg;
	vp->tend   = vp->tstart + ldi->ldinfo_textsize;
	vp->dstart = (CORE_ADDR) ldi->ldinfo_dataorg;
	vp->dend   = vp->dstart + ldi->ldinfo_datasize;

	if (vp->tadj)
	  {
	    vp->tstart += vp->tadj;
	    vp->tend   += vp->tadj;
	  }

	/* The objfile is only NULL for the exec file.  */
	if (vp->objfile == NULL)
	  got_exec_file = 1;

#ifdef DONT_RELOCATE_SYMFILE_OBJFILE
	if (vp->objfile == symfile_objfile
	    || vp->objfile == NULL)
	  {
	    ldi->ldinfo_dataorg = 0;
	    vp->dstart = (CORE_ADDR) 0;
	    vp->dend = ldi->ldinfo_datasize;
	  }
#endif

	/* relocate symbol table(s). */
	vmap_symtab (vp);

	/* There may be more, so we don't break out of the loop.  */
      }

    /* if there was no matching *vp, we must perforce create the sucker(s) */
    if (!got_one && !retried)
      {
	add_vmap (ldi);
	++retried;
	goto retry;
      }
  } while (ldi->ldinfo_next
	   && (ldi = (void *) (ldi->ldinfo_next + (char *) ldi)));

  /* If we don't find the symfile_objfile anywhere in the ldinfo, it
     is unlikely that the symbol file is relocated to the proper
     address.  And we might have attached to a process which is
     running a different copy of the same executable.  */
  if (symfile_objfile != NULL && !got_exec_file)
    {
      warning_begin ();
      fputs_unfiltered ("Symbol file ", gdb_stderr);
      fputs_unfiltered (symfile_objfile->name, gdb_stderr);
      fputs_unfiltered ("\nis not mapped; discarding it.\n\
If in fact that file has symbols which the mapped files listed by\n\
\"info files\" lack, you can load symbols with the \"symbol-file\" or\n\
\"add-symbol-file\" commands (note that you must take care of relocating\n\
symbols to the proper address).\n", gdb_stderr);
      free_objfile (symfile_objfile);
      symfile_objfile = NULL;
    }
  breakpoint_re_set ();
}

/* As well as symbol tables, exec_sections need relocation. After
   the inferior process' termination, there will be a relocated symbol
   table exist with no corresponding inferior process. At that time, we
   need to use `exec' bfd, rather than the inferior process's memory space
   to look up symbols.

   `exec_sections' need to be relocated only once, as long as the exec
   file remains unchanged.
*/

static void
vmap_exec ()
{
  static bfd *execbfd;
  int i;

  if (execbfd == exec_bfd)
    return;

  execbfd = exec_bfd;

  if (!vmap || !exec_ops.to_sections)
    error ("vmap_exec: vmap or exec_ops.to_sections == 0\n");

  for (i=0; &exec_ops.to_sections[i] < exec_ops.to_sections_end; i++)
    {
      if (STREQ(".text", exec_ops.to_sections[i].the_bfd_section->name))
	{
	  exec_ops.to_sections[i].addr += vmap->tstart;
	  exec_ops.to_sections[i].endaddr += vmap->tstart;
	}
      else if (STREQ(".data", exec_ops.to_sections[i].the_bfd_section->name))
	{
	  exec_ops.to_sections[i].addr += vmap->dstart;
	  exec_ops.to_sections[i].endaddr += vmap->dstart;
	}
    }
}

/* xcoff_relocate_symtab -	hook for symbol table relocation.
   also reads shared libraries.. */

void
xcoff_relocate_symtab (pid)
     unsigned int pid;
{
#define	MAX_LOAD_SEGS 64		/* maximum number of load segments */

  struct ld_info *ldi;

  ldi = (void *) alloca(MAX_LOAD_SEGS * sizeof (*ldi));

  /* According to my humble theory, AIX has some timing problems and
     when the user stack grows, kernel doesn't update stack info in time
     and ptrace calls step on user stack. That is why we sleep here a little,
     and give kernel to update its internals. */

  usleep (36000);

  errno = 0;
  ptrace (PT_LDINFO, pid, (PTRACE_ARG3_TYPE) ldi,
	  MAX_LOAD_SEGS * sizeof(*ldi), ldi);
  if (errno)
    perror_with_name ("ptrace ldinfo");

  vmap_ldinfo (ldi);

  do {
    /* We are allowed to assume CORE_ADDR == pointer.  This code is
       native only.  */
    add_text_to_loadinfo ((CORE_ADDR) ldi->ldinfo_textorg,
			  (CORE_ADDR) ldi->ldinfo_dataorg);
  } while (ldi->ldinfo_next
	   && (ldi = (void *) (ldi->ldinfo_next + (char *) ldi)));

#if 0
  /* Now that we've jumbled things around, re-sort them.  */
  sort_minimal_symbols ();
#endif

  /* relocate the exec and core sections as well. */
  vmap_exec ();
}

/* Core file stuff.  */

/* Relocate symtabs and read in shared library info, based on symbols
   from the core file.  */

void
xcoff_relocate_core (target)
     struct target_ops *target;
{
/* Offset of member MEMBER in a struct of type TYPE.  */
#ifndef offsetof
#define offsetof(TYPE, MEMBER) ((int) &((TYPE *)0)->MEMBER)
#endif

/* Size of a struct ld_info except for the variable-length filename.  */
#define LDINFO_SIZE (offsetof (struct ld_info, ldinfo_filename))

  sec_ptr ldinfo_sec;
  int offset = 0;
  struct ld_info *ldip;
  struct vmap *vp;

  /* Allocated size of buffer.  */
  int buffer_size = LDINFO_SIZE;
  char *buffer = xmalloc (buffer_size);
  struct cleanup *old = make_cleanup (free_current_contents, &buffer);
    
  /* FIXME, this restriction should not exist.  For now, though I'll
     avoid coredumps with error() pending a real fix.  */
  if (vmap == NULL)
    error
      ("Can't debug a core file without an executable file (on the RS/6000)");
  
  ldinfo_sec = bfd_get_section_by_name (core_bfd, ".ldinfo");
  if (ldinfo_sec == NULL)
    {
    bfd_err:
      fprintf_filtered (gdb_stderr, "Couldn't get ldinfo from core file: %s\n",
			bfd_errmsg (bfd_get_error ()));
      do_cleanups (old);
      return;
    }
  do
    {
      int i;
      int names_found = 0;

      /* Read in everything but the name.  */
      if (bfd_get_section_contents (core_bfd, ldinfo_sec, buffer,
				    offset, LDINFO_SIZE) == 0)
	goto bfd_err;

      /* Now the name.  */
      i = LDINFO_SIZE;
      do
	{
	  if (i == buffer_size)
	    {
	      buffer_size *= 2;
	      buffer = xrealloc (buffer, buffer_size);
	    }
	  if (bfd_get_section_contents (core_bfd, ldinfo_sec, &buffer[i],
					offset + i, 1) == 0)
	    goto bfd_err;
	  if (buffer[i++] == '\0')
	    ++names_found;
	} while (names_found < 2);

      ldip = (struct ld_info *) buffer;

      /* Can't use a file descriptor from the core file; need to open it.  */
      ldip->ldinfo_fd = -1;
      
      /* The first ldinfo is for the exec file, allocated elsewhere.  */
      if (offset == 0)
	vp = vmap;
      else
	vp = add_vmap (ldip);

      offset += ldip->ldinfo_next;

      /* We can assume pointer == CORE_ADDR, this code is native only.  */
      vp->tstart = (CORE_ADDR) ldip->ldinfo_textorg;
      vp->tend = vp->tstart + ldip->ldinfo_textsize;
      vp->dstart = (CORE_ADDR) ldip->ldinfo_dataorg;
      vp->dend = vp->dstart + ldip->ldinfo_datasize;

#ifdef DONT_RELOCATE_SYMFILE_OBJFILE
      if (vp == vmap)
	{
	  vp->dstart = (CORE_ADDR) 0;
	  vp->dend = ldip->ldinfo_datasize;
	}
#endif

      if (vp->tadj != 0)
	{
	  vp->tstart += vp->tadj;
	  vp->tend += vp->tadj;
	}

      /* Unless this is the exec file,
	 add our sections to the section table for the core target.  */
      if (vp != vmap)
	{
	  int count;
	  struct section_table *stp;
	  int update_coreops;

	  /* We must update the to_sections field in the core_ops structure
	     now to avoid dangling pointer dereferences.  */
	  update_coreops = core_ops.to_sections == target->to_sections;
	  
	  count = target->to_sections_end - target->to_sections;
	  count += 2;
	  target->to_sections = (struct section_table *)
	    xrealloc (target->to_sections,
		      sizeof (struct section_table) * count);
	  target->to_sections_end = target->to_sections + count;

	  /* Update the to_sections field in the core_ops structure
	     if needed.  */
	  if (update_coreops)
	    {
	      core_ops.to_sections = target->to_sections;
	      core_ops.to_sections_end = target->to_sections_end;
	    }
	  stp = target->to_sections_end - 2;

	  /* "Why do we add bfd_section_vma?", I hear you cry.
	     Well, the start of the section in the file is actually
	     that far into the section as the struct vmap understands it.
	     So for text sections, bfd_section_vma tends to be 0x200,
	     and if vp->tstart is 0xd0002000, then the first byte of
	     the text section on disk corresponds to address 0xd0002200.  */
	  stp->bfd = vp->bfd;
	  stp->the_bfd_section = bfd_get_section_by_name (stp->bfd, ".text");
	  stp->addr = bfd_section_vma (stp->bfd, stp->the_bfd_section) + vp->tstart;
	  stp->endaddr = bfd_section_vma (stp->bfd, stp->the_bfd_section) + vp->tend;
	  stp++;
	  
	  stp->bfd = vp->bfd;
	  stp->the_bfd_section = bfd_get_section_by_name (stp->bfd, ".data");
	  stp->addr = bfd_section_vma (stp->bfd, stp->the_bfd_section) + vp->dstart;
	  stp->endaddr = bfd_section_vma (stp->bfd, stp->the_bfd_section) + vp->dend;
	}

      vmap_symtab (vp);

      add_text_to_loadinfo ((CORE_ADDR)ldip->ldinfo_textorg,
			    (CORE_ADDR)ldip->ldinfo_dataorg);
    } while (ldip->ldinfo_next != 0);
  vmap_exec ();
  breakpoint_re_set ();
  do_cleanups (old);
}

int
kernel_u_size ()
{
  return (sizeof (struct user));
}


/* Register that we are able to handle rs6000 core file formats. */

static struct core_fns rs6000_core_fns =
{
  bfd_target_coff_flavour,
  fetch_core_registers,
  NULL
};

void
_initialize_core_rs6000 ()
{
  add_core_fns (&rs6000_core_fns);
}
