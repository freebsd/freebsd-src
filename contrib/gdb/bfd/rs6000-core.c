/* IBM RS/6000 "XCOFF" back-end for BFD.
   Copyright (C) 1990, 1991, 1995 Free Software Foundation, Inc.
   FIXME: Can someone provide a transliteration of this name into ASCII?
   Using the following chars caused a compiler warning on HIUX (so I replaced
   them with octal escapes), and isn't useful without an understanding of what
   character set it is.
   Written by Metin G. Ozisik, Mimi Ph\373\364ng-Th\345o V\365, 
     and John Gilmore.
   Archive support from Damon A. Permezel.
   Contributed by IBM Corporation and Cygnus Support.

This file is part of BFD, the Binary File Descriptor library.

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

/* This port currently only handles reading object files, except when
   compiled on an RS/6000 host.  -- no archive support, no core files.
   In all cases, it does not support writing.

   FIXMEmgo comments are left from Metin Ozisik's original port.

   This is in a separate file from coff-rs6000.c, because it includes
   system include files that conflict with coff/rs6000.h.
  */

/* Internalcoff.h and coffcode.h modify themselves based on this flag.  */
#define RS6000COFF_C 1

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"

#ifdef AIX_CORE

/* AOUTHDR is defined by the above.  We need another defn of it, from the
   system include files.  Punt the old one and get us a new name for the
   typedef in the system include files.  */
#ifdef AOUTHDR
#undef AOUTHDR
#endif
#define	AOUTHDR	second_AOUTHDR

#undef	SCNHDR


/* ------------------------------------------------------------------------ */
/*	Support for core file stuff.. 					    */
/* ------------------------------------------------------------------------ */

#include <sys/user.h>
#include <sys/ldr.h>
#include <sys/core.h>


/* Number of special purpose registers supported by gdb.  This value
   should match `tm.h' in gdb directory.  Clean this mess up and use
   the macros in sys/reg.h.  FIXMEmgo. */

#define	NUM_OF_SPEC_REGS  7

#define	core_hdr(bfd)		(((Rs6kCorData*)(bfd->tdata.any))->hdr)
#define	core_datasec(bfd)	(((Rs6kCorData*)(bfd->tdata.any))->data_section)
#define	core_stacksec(bfd)	(((Rs6kCorData*)(bfd->tdata.any))->stack_section)
#define	core_regsec(bfd)	(((Rs6kCorData*)(bfd->tdata.any))->reg_section)
#define	core_reg2sec(bfd)	(((Rs6kCorData*)(bfd->tdata.any))->reg2_section)

/* AIX 4.1 Changed the names and locations of a few items in the core file,
   this seems to be the quickest easiet way to deal with it. 

   Note however that encoding magic addresses (STACK_END_ADDR) is going
   to be _very_ fragile.  But I don't see any easy way to get that info
   right now.  */
#ifdef CORE_VERSION_1
#define CORE_DATA_SIZE_FIELD c_u.U_dsize
#define CORE_COMM_FIELD c_u.U_comm
#define SAVE_FIELD c_mst
#define	STACK_END_ADDR 0x2ff23000
#else
#define CORE_DATA_SIZE_FIELD c_u.u_dsize
#define CORE_COMM_FIELD c_u.u_comm
#define SAVE_FIELD c_u.u_save
#define	STACK_END_ADDR 0x2ff80000
#endif

/* These are stored in the bfd's tdata */
typedef struct {
  struct core_dump hdr;		/* core file header */
  asection *data_section,
  	   *stack_section,
	   *reg_section,	/* section for GPRs and special registers. */
	   *reg2_section;	/* section for FPRs. */

  /* This tells us where everything is mapped (shared libraries and so on).
     GDB needs it.  */
  asection *ldinfo_section;
#define core_ldinfosec(bfd) (((Rs6kCorData *)(bfd->tdata.any))->ldinfo_section)
} Rs6kCorData;


/* Decide if a given bfd represents a `core' file or not. There really is no
   magic number or anything like, in rs6000coff. */

const bfd_target *
rs6000coff_core_p (abfd)
     bfd *abfd;
{
  int fd;
  struct core_dump coredata;
  struct stat statbuf;
  char *tmpptr;

  /* Use bfd_xxx routines, rather than O/S primitives to read coredata. FIXMEmgo */
  fd = open (abfd->filename, O_RDONLY);
  if (fd < 0)
    {
      bfd_set_error (bfd_error_system_call);
      return NULL;
    }

  if (fstat (fd, &statbuf) < 0)
    {
      bfd_set_error (bfd_error_system_call);
      close (fd);
      return NULL;
    }
  if (read (fd, &coredata, sizeof (struct core_dump))
      != sizeof (struct core_dump))
    {
      bfd_set_error (bfd_error_wrong_format);
      close (fd);
      return NULL;
    }

  if (close (fd) < 0)
    {
      bfd_set_error (bfd_error_system_call);
      return NULL;
    }

  /* If the core file ulimit is too small, the system will first
     omit the data segment, then omit the stack, then decline to
     dump core altogether (as far as I know UBLOCK_VALID and LE_VALID
     are always set) (this is based on experimentation on AIX 3.2).
     Now, the thing is that GDB users will be surprised
     if segments just silently don't appear (well, maybe they would
     think to check "info files", I don't know), but we have no way of
     returning warnings (as opposed to errors).

     For the data segment, we have no choice but to keep going if it's
     not there, since the default behavior is not to dump it (regardless
     of the ulimit, it's based on SA_FULLDUMP).  But for the stack segment,
     if it's not there, we refuse to have anything to do with this core
     file.  The usefulness of a core dump without a stack segment is pretty
     limited anyway.  */
     
  if (!(coredata.c_flag & UBLOCK_VALID)
      || !(coredata.c_flag & LE_VALID))
    {
      bfd_set_error (bfd_error_wrong_format);
      return NULL;
    }

  if ((coredata.c_flag & CORE_TRUNC)
      || !(coredata.c_flag & USTACK_VALID))
    {
      bfd_set_error (bfd_error_file_truncated);
      return NULL;
    }

  /* Don't check the core file size for a full core, AIX 4.1 includes
     additional shared library sections in a full core.  */
  if (!(coredata.c_flag & FULL_CORE)
      && ((bfd_vma)coredata.c_stack + coredata.c_size) != statbuf.st_size)
    {
      /* If the size is wrong, it means we're misinterpreting something.  */
      bfd_set_error (bfd_error_wrong_format);
      return NULL;
    }

  /* Sanity check on the c_tab field.  */
  if ((u_long) coredata.c_tab < sizeof coredata ||
      (u_long) coredata.c_tab >= statbuf.st_size ||
      (long) coredata.c_tab >= (long)coredata.c_stack)
    {
      bfd_set_error (bfd_error_wrong_format);
      return NULL;
    }

  /* maybe you should alloc space for the whole core chunk over here!! FIXMEmgo */
  tmpptr = (char*)bfd_zalloc (abfd, sizeof (Rs6kCorData));
  if (!tmpptr)
    return NULL;
      
  set_tdata (abfd, tmpptr);

  /* Copy core file header.  */
  core_hdr (abfd) = coredata;

  /* .stack section. */
  if ((core_stacksec (abfd) = (asection*) bfd_zalloc (abfd, sizeof (asection)))
       == NULL)
    return NULL;
  core_stacksec (abfd)->name = ".stack";
  core_stacksec (abfd)->flags = SEC_ALLOC + SEC_LOAD + SEC_HAS_CONTENTS;
  core_stacksec (abfd)->_raw_size = coredata.c_size;
  core_stacksec (abfd)->vma = STACK_END_ADDR - coredata.c_size;
  core_stacksec (abfd)->filepos = (int)coredata.c_stack;	/*???? */

  /* .reg section for GPRs and special registers. */
  if ((core_regsec (abfd) = (asection*) bfd_zalloc (abfd, sizeof (asection)))
       == NULL)
    return NULL;
  core_regsec (abfd)->name = ".reg";
  core_regsec (abfd)->flags = SEC_HAS_CONTENTS;
  core_regsec (abfd)->_raw_size = (32 + NUM_OF_SPEC_REGS) * 4;
  core_regsec (abfd)->vma = 0;			/* not used?? */
  core_regsec (abfd)->filepos = 
  	(char*)&coredata.SAVE_FIELD - (char*)&coredata;

  /* .reg2 section for FPRs (floating point registers). */
  if ((core_reg2sec (abfd) = (asection*) bfd_zalloc (abfd, sizeof (asection)))
       == NULL)
    return NULL;
  core_reg2sec (abfd)->name = ".reg2";
  core_reg2sec (abfd)->flags = SEC_HAS_CONTENTS;
  core_reg2sec (abfd)->_raw_size = 8 * 32;			/* 32 FPRs. */
  core_reg2sec (abfd)->vma = 0;			/* not used?? */
  core_reg2sec (abfd)->filepos = 
  	(char*)&coredata.SAVE_FIELD.fpr[0] - (char*)&coredata;

  if ((core_ldinfosec (abfd) = (asection*) bfd_zalloc (abfd, sizeof (asection)))
       == NULL)
    return NULL;
  core_ldinfosec (abfd)->name = ".ldinfo";
  core_ldinfosec (abfd)->flags = SEC_HAS_CONTENTS;
  /* To actually find out how long this section is in this particular
     core dump would require going down the whole list of struct ld_info's.
     See if we can just fake it.  */
  core_ldinfosec (abfd)->_raw_size = 0x7fffffff;
  /* Not relevant for ldinfo section.  */
  core_ldinfosec (abfd)->vma = 0;
  core_ldinfosec (abfd)->filepos = (file_ptr) coredata.c_tab;

  /* set up section chain here. */
  abfd->section_count = 4;
  abfd->sections = core_stacksec (abfd);
  core_stacksec (abfd)->next = core_regsec(abfd);
  core_regsec (abfd)->next = core_reg2sec (abfd);
  core_reg2sec (abfd)->next = core_ldinfosec (abfd);
  core_ldinfosec (abfd)->next = NULL;

  if (coredata.c_flag & FULL_CORE)
    {
      asection *sec = (asection *) bfd_zalloc (abfd, sizeof (asection));
      if (sec == NULL)
	return NULL;
      sec->name = ".data";
      sec->flags = SEC_ALLOC | SEC_LOAD | SEC_HAS_CONTENTS;
      sec->_raw_size = coredata.CORE_DATA_SIZE_FIELD;
      sec->vma = CDATA_ADDR (coredata.CORE_DATA_SIZE_FIELD);
      sec->filepos = (int)coredata.c_stack + coredata.c_size;

      sec->next = abfd->sections;
      abfd->sections = sec;
      ++abfd->section_count;
    }

  return abfd->xvec;				/* this is garbage for now. */
}



/* return `true' if given core is from the given executable.. */
boolean
rs6000coff_core_file_matches_executable_p (core_bfd, exec_bfd)
     bfd *core_bfd;
     bfd *exec_bfd;
{
  FILE *fd;
  struct core_dump coredata;
  struct ld_info ldinfo;
  char pathname [1024];
  const char *str1, *str2;

  /* Use bfd_xxx routines, rather than O/S primitives, do error checking!!
  								FIXMEmgo */
  /* Actually should be able to use bfd_get_section_contents now that
     we have a .ldinfo section.  */
  fd = fopen (core_bfd->filename, FOPEN_RB);

  fread (&coredata, sizeof (struct core_dump), 1, fd);
  fseek (fd, (long)coredata.c_tab, 0);
  fread (&ldinfo, (char*)&ldinfo.ldinfo_filename[0] - (char*)&ldinfo.ldinfo_next,
	 1, fd);
  fscanf (fd, "%s", pathname);
  
  str1 = strrchr (pathname, '/');
  str2 = strrchr (exec_bfd->filename, '/');

  /* step over character '/' */
  str1 = str1 ? str1+1 : &pathname[0];
  str2 = str2 ? str2+1 : exec_bfd->filename;

  fclose (fd);
  return strcmp (str1, str2) == 0;
}

char *
rs6000coff_core_file_failing_command (abfd)
     bfd *abfd;
{
  char *com = core_hdr (abfd).CORE_COMM_FIELD;
  if (*com)
    return com;
  else
    return 0;
}

int
rs6000coff_core_file_failing_signal (abfd)
     bfd *abfd;
{
  return core_hdr (abfd).c_signo;
}


boolean
rs6000coff_get_section_contents (abfd, section, location, offset, count)
     bfd *abfd;
     sec_ptr section;
     PTR location;
     file_ptr offset;
     int count;
{
    if (count == 0)
	return true;

    /* Reading a core file's sections will be slightly different. For the
       rest of them we can use bfd_generic_get_section_contents () I suppose. */
    /* Make sure this routine works for any bfd and any section. FIXMEmgo. */

    if (abfd->format == bfd_core && strcmp (section->name, ".reg") == 0) {

      struct mstsave mstatus;
      int    regoffset = (char*)&mstatus.gpr[0] - (char*)&mstatus;

      /* Assert that the only way this code will be executed is reading the
         whole section. */
      if (offset || count != (sizeof(mstatus.gpr) + (4 * NUM_OF_SPEC_REGS)))
        (*_bfd_error_handler)
	  ("ERROR! in rs6000coff_get_section_contents()\n");

      /* for `.reg' section, `filepos' is a pointer to the `mstsave' structure
         in the core file. */

      /* read GPR's into the location. */
      if ( bfd_seek(abfd, section->filepos + regoffset, SEEK_SET) == -1
	|| bfd_read(location, sizeof (mstatus.gpr), 1, abfd) != sizeof (mstatus.gpr))
	return (false); /* on error */

      /* increment location to the beginning of special registers in the section,
         reset register offset value to the beginning of first special register
	 in mstsave structure, and read special registers. */

      location = (PTR) ((char*)location + sizeof (mstatus.gpr));
      regoffset = (char*)&mstatus.iar - (char*)&mstatus;

      if ( bfd_seek(abfd, section->filepos + regoffset, SEEK_SET) == -1
	|| bfd_read(location, 4 * NUM_OF_SPEC_REGS, 1, abfd) != 
							4 * NUM_OF_SPEC_REGS)
	return (false); /* on error */
      
      /* increment location address, and read the special registers.. */
      /* FIXMEmgo */
      return (true);
    }

    /* else, use default bfd section content transfer. */
    else
      return _bfd_generic_get_section_contents 
      			(abfd, section, location, offset, count);
}

#endif /* AIX_CORE */
