/* Generate a core file for the inferior process.
   Copyright 2001, 2002 Free Software Foundation, Inc.

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
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include "defs.h"
#include "cli/cli-decode.h"
#include "inferior.h"
#include "gdbcore.h"
#include "elf-bfd.h"
#include <sys/procfs.h>
#include "symfile.h"
#include "objfiles.h"

static char                  *default_gcore_target (void);
static enum bfd_architecture  default_gcore_arch (void);
static unsigned long          default_gcore_mach (void);
static int                    gcore_memory_sections (bfd *);

/* Function: gcore_command
   Generate a core file from the inferior process.  */

static void
gcore_command (char *args, int from_tty)
{
  struct cleanup *old_chain;
  char *corefilename, corefilename_buffer[40];
  asection *note_sec = NULL;
  bfd *obfd;
  void *note_data = NULL;
  int note_size = 0;

  /* No use generating a corefile without a target process.  */
  if (!(target_has_execution))
    noprocess ();

  if (args && *args)
    corefilename = args;
  else
    {
      /* Default corefile name is "core.PID".  */
      sprintf (corefilename_buffer, "core.%d", PIDGET (inferior_ptid));
      corefilename = corefilename_buffer;
    }

  if (info_verbose)
    fprintf_filtered (gdb_stdout, 
		      "Opening corefile '%s' for output.\n", corefilename);

  /* Open the output file. */
  if (!(obfd = bfd_openw (corefilename, default_gcore_target ())))
    {
      error ("Failed to open '%s' for output.", corefilename);
    }

  /* Need a cleanup that will close the file (FIXME: delete it?). */
  old_chain = make_cleanup_bfd_close (obfd);

  bfd_set_format (obfd, bfd_core);
  bfd_set_arch_mach (obfd, default_gcore_arch (), default_gcore_mach ());

  /* An external target method must build the notes section. */
  note_data = (char *) target_make_corefile_notes (obfd, &note_size);

  /* Create the note section. */
  if (note_data != NULL && note_size != 0)
    {
      if ((note_sec = bfd_make_section_anyway (obfd, "note0")) == NULL)
	error ("Failed to create 'note' section for corefile: %s", 
	       bfd_errmsg (bfd_get_error ()));

      bfd_set_section_vma (obfd, note_sec, 0);
      bfd_set_section_flags (obfd, note_sec, 
			     SEC_HAS_CONTENTS | SEC_READONLY | SEC_ALLOC);
      bfd_set_section_alignment (obfd, note_sec, 0);
      bfd_set_section_size (obfd, note_sec, note_size);
    }

  /* Now create the memory/load sections. */
  if (gcore_memory_sections (obfd) == 0)
    error ("gcore: failed to get corefile memory sections from target.");

  /* Write out the contents of the note section. */
  if (note_data != NULL && note_size != 0)
    {
      if (!bfd_set_section_contents (obfd, note_sec, note_data, 0, note_size))
	{
	  warning ("writing note section (%s)", 
		   bfd_errmsg (bfd_get_error ()));
	}
    }

  /* Succeeded. */
  fprintf_filtered (gdb_stdout, 
		    "Saved corefile %s\n", corefilename);

  /* Clean-ups will close the output file and free malloc memory. */
  do_cleanups (old_chain);
  return;
}

static unsigned long
default_gcore_mach (void)
{
#if 1	/* See if this even matters... */
  return 0;
#else
#ifdef TARGET_ARCHITECTURE
  const struct bfd_arch_info * bfdarch = TARGET_ARCHITECTURE;

  if (bfdarch != NULL)
    return bfdarch->mach;
#endif /* TARGET_ARCHITECTURE */
  if (exec_bfd == NULL)
    error ("Can't find default bfd machine type (need execfile).");

  return bfd_get_mach (exec_bfd);
#endif /* 1 */
}

static enum bfd_architecture
default_gcore_arch (void)
{
#ifdef TARGET_ARCHITECTURE
  const struct bfd_arch_info * bfdarch = TARGET_ARCHITECTURE;

  if (bfdarch != NULL)
    return bfdarch->arch;
#endif
  if (exec_bfd == NULL)
    error ("Can't find bfd architecture for corefile (need execfile).");

  return bfd_get_arch (exec_bfd);
}

static char *
default_gcore_target (void)
{
  /* FIXME -- this may only work for ELF targets.  */
  if (exec_bfd == NULL)
    return NULL;
  else
    return bfd_get_target (exec_bfd);
}

/*
 * Default method for stack segment (preemptable by target).
 */

static int (*override_derive_stack_segment) (bfd_vma *, bfd_vma *);

extern void
preempt_derive_stack_segment (int (*override_func) (bfd_vma *, bfd_vma *))
{
  override_derive_stack_segment = override_func;
}

/* Function: default_derive_stack_segment
   Derive a reasonable stack segment by unwinding the target stack. 
   
   Returns 0 for failure, 1 for success.  */

static int 
default_derive_stack_segment (bfd_vma *bottom, bfd_vma *top)
{
  bfd_vma tmp_vma;
  struct frame_info *fi, *tmp_fi;

  if (bottom == NULL || top == NULL)
    return 0;	/* Paranoia. */

  if (!target_has_stack || !target_has_registers)
    return 0;	/* Can't succeed without stack and registers. */

  if ((fi = get_current_frame ()) == NULL)
    return 0;	/* Can't succeed without current frame. */

  /* Save frame pointer of TOS frame. */
  *top = fi->frame;
  /* If current stack pointer is more "inner", use that instead. */
  if (INNER_THAN (read_sp (), *top))
    *top = read_sp ();

  /* Find prev-most frame. */
  while ((tmp_fi = get_prev_frame (fi)) != NULL)
    fi = tmp_fi;

  /* Save frame pointer of prev-most frame. */
  *bottom = fi->frame;

  /* Now canonicalize their order, so that 'bottom' is a lower address
   (as opposed to a lower stack frame). */
  if (*bottom > *top)
    {
      tmp_vma = *top;
      *top = *bottom;
      *bottom = tmp_vma;
    }

  return 1;	/* success */
}

static int
derive_stack_segment (bfd_vma *bottom, bfd_vma *top)
{
  if (override_derive_stack_segment)
    return override_derive_stack_segment (bottom, top);
  else
    return default_derive_stack_segment (bottom, top);
}

/*
 * Default method for heap segment (preemptable by target).
 */

static int (*override_derive_heap_segment) (bfd *, bfd_vma *, bfd_vma *);

extern void
preempt_derive_heap_segment (int (*override_func) (bfd *, 
						   bfd_vma *, bfd_vma *))
{
  override_derive_heap_segment = override_func;
}

/* Function: default_derive_heap_segment
   Derive a reasonable heap segment by looking at sbrk and
   the static data sections.
   
   Returns 0 for failure, 1 for success.  */

static int 
default_derive_heap_segment (bfd *abfd, bfd_vma *bottom, bfd_vma *top)
{
  bfd_vma top_of_data_memory = 0;
  bfd_vma top_of_heap = 0;
  bfd_size_type sec_size;
  struct value *zero, *sbrk;
  bfd_vma sec_vaddr;
  asection *sec;

  if (bottom == NULL || top == NULL)
    return 0;		/* Paranoia. */

  if (!target_has_execution)
    return 0;		/* This function depends on being able
			   to call a function in the inferior.  */

  /* Assumption: link map is arranged as follows (low to high addresses):
     text sections
     data sections (including bss)
     heap
  */

  for (sec = abfd->sections; sec; sec = sec->next)
    {
      if (bfd_get_section_flags (abfd, sec) & SEC_DATA ||
	  strcmp (".bss", bfd_get_section_name (abfd, sec)) == 0)
	{
	  sec_vaddr = bfd_get_section_vma (abfd, sec);
	  sec_size = bfd_get_section_size_before_reloc (sec);
	  if (sec_vaddr + sec_size > top_of_data_memory)
	    top_of_data_memory = sec_vaddr + sec_size;
	}
    }
  /* Now get the top-of-heap by calling sbrk in the inferior.  */
  if ((sbrk = find_function_in_inferior ("sbrk")) == NULL)
    return 0;
  if ((zero = value_from_longest (builtin_type_int, (LONGEST) 0)) == NULL)
    return 0;
  if ((sbrk = call_function_by_hand (sbrk, 1, &zero)) == NULL)
    return 0;
  top_of_heap = value_as_long (sbrk);

  /* Return results. */
  if (top_of_heap > top_of_data_memory)
    {
      *bottom = top_of_data_memory;
      *top = top_of_heap;
      return 1;	/* success */
    }
  else
    return 0;	/* No additional heap space needs to be saved. */
}

static int
derive_heap_segment (bfd *abfd, bfd_vma *bottom, bfd_vma *top)
{
  if (override_derive_heap_segment)
    return override_derive_heap_segment (abfd, bottom, top);
  else
    return default_derive_heap_segment (abfd, bottom, top);
}

/* ARGSUSED */
static void
make_output_phdrs (bfd *obfd, asection *osec, void *ignored)
{
  int p_flags = 0;
  int p_type;

  /* FIXME: these constants may only be applicable for ELF.  */
  if (strncmp (osec->name, "load", 4) == 0)
    p_type = PT_LOAD;
  else
    p_type = PT_NOTE;

  p_flags |= PF_R;	/* Segment is readable.  */
  if (!(bfd_get_section_flags (obfd, osec) & SEC_READONLY))
    p_flags |= PF_W;	/* Segment is writable.  */
  if (bfd_get_section_flags (obfd, osec) & SEC_CODE)
    p_flags |= PF_X;	/* Segment is executable.  */

  bfd_record_phdr (obfd, p_type, 1, p_flags, 0, 0, 
		   0, 0, 1, &osec);
}

static asection *
make_mem_sec (bfd *obfd, 
	      bfd_vma addr, 
	      bfd_size_type size, 
	      unsigned int flags, 
	      unsigned int alignment)
{
  asection *osec;

  if ((osec = bfd_make_section_anyway (obfd, "load")) == NULL)
    {
      warning ("Couldn't make gcore segment: %s",
	       bfd_errmsg (bfd_get_error ()));
      return NULL;
    }

  if (info_verbose)
    {
      fprintf_filtered (gdb_stdout, 
			"Save segment, %lld bytes at 0x%s\n",
			(long long) size, paddr_nz (addr));
    }

  bfd_set_section_size (obfd, osec, size);
  bfd_set_section_vma (obfd, osec, addr);
  osec->lma = 0;	/* FIXME: there should be a macro for this! */
  bfd_set_section_alignment (obfd, osec, alignment);
  bfd_set_section_flags (obfd, osec, 
			 flags | SEC_LOAD | SEC_ALLOC | SEC_HAS_CONTENTS);
  return osec;
}

static int
gcore_create_callback (CORE_ADDR vaddr, 
		       unsigned long size,
		       int read, int write, int exec, 
		       void *data)
{
  flagword flags = 0;

  if (write == 0)
    {
      flags |= SEC_READONLY;
      /* Set size == zero for readonly sections. */
      size = 0;
    }
  if (exec)
    {
      flags |= SEC_CODE;
    }
  else
    {
      flags |= SEC_DATA;
    }

  return ((make_mem_sec ((bfd *) data, vaddr, size, flags, 0)) == NULL);
}

static int
objfile_find_memory_regions (int (*func) (CORE_ADDR, 
					  unsigned long,
					  int, int, int,
					   void *), 
			     void *obfd)
{
  /* Use objfile data to create memory sections. */
  struct objfile *objfile;
  struct obj_section *objsec;
  bfd_vma temp_bottom, temp_top;

  /* Call callback function for each objfile section. */
  ALL_OBJSECTIONS (objfile, objsec)
    {
      bfd *ibfd = objfile->obfd;
      asection *isec = objsec->the_bfd_section;
      flagword flags = bfd_get_section_flags (ibfd, isec);
      int ret;

      if ((flags & SEC_ALLOC) || (flags & SEC_LOAD))
	{
	  int size = bfd_section_size (ibfd, isec);
	  int ret;

	  if ((ret = (*func) (objsec->addr, 
			      bfd_section_size (ibfd, isec), 
			      1, /* All sections will be readable.  */
			      (flags & SEC_READONLY) == 0, /* writable */
			      (flags & SEC_CODE) != 0, /* executable */
			      obfd)) != 0)
	    return ret;
	}
    }

  /* Make a stack segment. */
  if (derive_stack_segment (&temp_bottom, &temp_top))
    (*func) (temp_bottom, 
	     temp_top - temp_bottom, 
	     1, /* Stack section will be readable */
	     1, /* Stack section will be writable */
	     0, /* Stack section will not be executable */
	     obfd);

  /* Make a heap segment. */
  if (derive_heap_segment (exec_bfd, &temp_bottom, &temp_top))
    (*func) (temp_bottom, 
	     temp_top - temp_bottom, 
	     1, /* Heap section will be readable */
	     1, /* Heap section will be writable */
	     0, /* Heap section will not be executable */
	     obfd);
  return 0;
}

static void
gcore_copy_callback (bfd *obfd, asection *osec, void *ignored)
{
  bfd_size_type size = bfd_section_size (obfd, osec);
  struct cleanup *old_chain = NULL;
  void *memhunk;

  if (size == 0)
    return;	/* Read-only sections are marked as zero-size.
		   We don't have to copy their contents. */
  if (strncmp ("load", bfd_get_section_name (obfd, osec), 4) != 0)
    return;	/* Only interested in "load" sections. */

  if ((memhunk = xmalloc (size)) == NULL)
    error ("Not enough memory to create corefile.");
  old_chain = make_cleanup (xfree, memhunk);

  if (target_read_memory (bfd_section_vma (obfd, osec), 
			  memhunk, size) != 0)
    warning ("Memory read failed for corefile section, %ld bytes at 0x%s\n",
	     (long) size, paddr (bfd_section_vma (obfd, osec)));
  if (!bfd_set_section_contents (obfd, osec, memhunk, 0, size))
    warning ("Failed to write corefile contents (%s).", 
	     bfd_errmsg (bfd_get_error ()));

  do_cleanups (old_chain);	/* frees the xmalloc buffer */
}

static int
gcore_memory_sections (bfd *obfd)
{
  if (target_find_memory_regions (gcore_create_callback, obfd) != 0)
    return 0;	/* FIXME error return/msg? */

  /* Record phdrs for section-to-segment mapping. */
  bfd_map_over_sections (obfd, make_output_phdrs, NULL);

  /* Copy memory region contents. */
  bfd_map_over_sections (obfd, gcore_copy_callback, NULL);

  return 1;	/* success */
}

void
_initialize_gcore (void)
{
  add_com ("generate-core-file", class_files, gcore_command,
	   "Save a core file with the current state of the debugged process.\n\
Argument is optional filename.  Default filename is 'core.<process_id>'.");

  add_com_alias ("gcore", "generate-core-file", class_files, 1);
  exec_set_find_memory_regions (objfile_find_memory_regions);
}
