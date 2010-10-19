/* BFD back-end for HP/UX core files.
   Copyright 1993, 1994, 1996, 1998, 1999, 2001, 2002, 2003, 2004
   Free Software Foundation, Inc.
   Written by Stu Grossman, Cygnus Support.
   Converted to back-end form by Ian Lance Taylor, Cygnus SUpport

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
Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.  */

/* This file can only be compiled on systems which use HP/UX style
   core files.  */

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"

#if defined (HOST_HPPAHPUX) || defined (HOST_HP300HPUX) || defined (HOST_HPPAMPEIX)

/* FIXME: sys/core.h doesn't exist for HPUX version 7.  HPUX version
   5, 6, and 7 core files seem to be standard trad-core.c type core
   files; can we just use trad-core.c in addition to this file?  */

#include <sys/core.h>
#include <sys/utsname.h>

#endif /* HOST_HPPAHPUX */

#ifdef HOST_HPPABSD

/* Not a very swift place to put it, but that's where the BSD port
   puts them.  */
#include "/hpux/usr/include/sys/core.h"

#endif /* HOST_HPPABSD */

#include <sys/param.h>
#ifdef HAVE_DIRENT_H
# include <dirent.h>
#else
# ifdef HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# ifdef HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# ifdef HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif
#include <signal.h>
#ifdef HPUX_CORE
#include <machine/reg.h>
#endif
#include <sys/user.h>		/* After a.out.h  */
#include <sys/file.h>

/* Kludge: There's no explicit mechanism provided by sys/core.h to
   conditionally know whether a proc_info has thread id fields.
   However, CORE_ANON_SHMEM shows up first at 10.30, which is
   happily also when meaningful thread id's show up in proc_info. */
#if defined(CORE_ANON_SHMEM)
#define PROC_INFO_HAS_THREAD_ID (1)
#endif

/* This type appears at HP-UX 10.30.  Defining it if not defined
   by sys/core.h allows us to build for older HP-UX's, and (since
   it won't be encountered in core-dumps from older HP-UX's) is
   harmless. */
#if !defined(CORE_ANON_SHMEM)
#define CORE_ANON_SHMEM 0x00000200         /* anonymous shared memory */
#endif

/* These are stored in the bfd's tdata */

/* .lwpid and .user_tid are only valid if PROC_INFO_HAS_THREAD_ID, else they
   are set to 0.  Also, until HP-UX implements MxN threads, .user_tid and
   .lwpid are synonymous. */
struct hpux_core_struct
{
  int sig;
  int lwpid;               /* Kernel thread ID. */
  unsigned long user_tid;  /* User thread ID. */
  char cmd[MAXCOMLEN + 1];
};

#define core_hdr(bfd) ((bfd)->tdata.hpux_core_data)
#define core_signal(bfd) (core_hdr(bfd)->sig)
#define core_command(bfd) (core_hdr(bfd)->cmd)
#define core_kernel_thread_id(bfd) (core_hdr(bfd)->lwpid)
#define core_user_thread_id(bfd) (core_hdr(bfd)->user_tid)
#define hpux_core_core_file_matches_executable_p generic_core_file_matches_executable_p

static asection *make_bfd_asection (bfd *, const char *, flagword,
                                    bfd_size_type, bfd_vma, unsigned int);
static const bfd_target *hpux_core_core_file_p (bfd *);
static char *hpux_core_core_file_failing_command (bfd *);
static int hpux_core_core_file_failing_signal (bfd *);
static void swap_abort (void);

static asection *
make_bfd_asection (bfd *abfd, const char *name, flagword flags,
                   bfd_size_type size, bfd_vma vma,
                   unsigned int alignment_power)
{
  asection *asect;
  char *newname;

  newname = bfd_alloc (abfd, (bfd_size_type) strlen (name) + 1);
  if (!newname)
    return NULL;

  strcpy (newname, name);

  asect = bfd_make_section_anyway (abfd, newname);
  if (!asect)
    return NULL;

  asect->flags = flags;
  asect->size = size;
  asect->vma = vma;
  asect->filepos = bfd_tell (abfd);
  asect->alignment_power = alignment_power;

  return asect;
}

/* Return true if the given core file section corresponds to a thread,
   based on its name.  */

static int
thread_section_p (bfd *abfd ATTRIBUTE_UNUSED,
                  asection *sect,
                  void *obj ATTRIBUTE_UNUSED)
{
  return (strncmp (sect->name, ".reg/", 5) == 0);
}

/* this function builds a bfd target if the file is a corefile.
   It returns null or 0 if it finds out thaat it is not a core file.
   The way it checks this is by looking for allowed 'type' field values.
   These are declared in sys/core.h
   There are some values which are 'reserved for future use'. In particular
   CORE_NONE is actually defined as 0. This may be a catch-all for cases
   in which the core file is generated by some non-hpux application.
   (I am just guessing here!)
*/
static const bfd_target *
hpux_core_core_file_p (bfd *abfd)
{
  int  good_sections = 0;
  int  unknown_sections = 0;

  core_hdr (abfd) = (struct hpux_core_struct *)
    bfd_zalloc (abfd, (bfd_size_type) sizeof (struct hpux_core_struct));
  if (!core_hdr (abfd))
    return NULL;

  while (1)
    {
      int val;
      struct corehead core_header;

      val = bfd_bread ((void *) &core_header,
		      (bfd_size_type) sizeof core_header, abfd);
      if (val <= 0)
	break;
      switch (core_header.type)
	{
	case CORE_KERNEL:
	case CORE_FORMAT:
	  /* Just skip this.  */
	  bfd_seek (abfd, (file_ptr) core_header.len, SEEK_CUR);
          good_sections++;
	  break;
	case CORE_EXEC:
	  {
	    struct proc_exec proc_exec;
	    if (bfd_bread ((void *) &proc_exec, (bfd_size_type) core_header.len,
			  abfd) != core_header.len)
	      break;
	    strncpy (core_command (abfd), proc_exec.cmd, MAXCOMLEN + 1);
            good_sections++;
	  }
	  break;
	case CORE_PROC:
	  {
	    struct proc_info proc_info;
	    char  secname[100];  /* Of arbitrary size, but plenty large. */

            /* We need to read this section, 'cause we need to determine
               whether the core-dumped app was threaded before we create
               any .reg sections. */
	    if (bfd_bread (&proc_info, (bfd_size_type) core_header.len, abfd)
		!= core_header.len)
	      break;

              /* However, we also want to create those sections with the
                 file positioned at the start of the record, it seems. */
            if (bfd_seek (abfd, -((file_ptr) core_header.len), SEEK_CUR) != 0)
              break;

#if defined(PROC_INFO_HAS_THREAD_ID)
            core_kernel_thread_id (abfd) = proc_info.lwpid;
            core_user_thread_id (abfd) = proc_info.user_tid;
#else
            core_kernel_thread_id (abfd) = 0;
            core_user_thread_id (abfd) = 0;
#endif
            /* If the program was unthreaded, then we'll just create a
               .reg section.

               If the program was threaded, then we'll create .reg/XXXXX
               section for each thread, where XXXXX is a printable
               representation of the kernel thread id.  We'll also
               create a .reg section for the thread that was running
               and signalled at the time of the core-dump (i.e., this
               is effectively an alias, needed to keep GDB happy.)

               Note that we use `.reg/XXXXX' as opposed to '.regXXXXX'
               because GDB expects that .reg2 will be the floating-
               point registers. */
            if (core_kernel_thread_id (abfd) == 0)
              {
                if (!make_bfd_asection (abfd, ".reg",
					SEC_HAS_CONTENTS,
					core_header.len,
					(bfd_vma) offsetof (struct proc_info,
							    hw_regs),
					2))
		  goto fail;
              }
            else
              {
                /* There are threads.  Is this the one that caused the
                   core-dump?  We'll claim it was the running thread. */
                if (proc_info.sig != -1)
                  {
		    if (!make_bfd_asection (abfd, ".reg",
					    SEC_HAS_CONTENTS,
					    core_header.len,
					    (bfd_vma)offsetof (struct proc_info,
							       hw_regs),
					    2))
		      goto fail;
                  }
                /* We always make one of these sections, for every thread. */
                sprintf (secname, ".reg/%d", core_kernel_thread_id (abfd));
                if (!make_bfd_asection (abfd, secname,
					SEC_HAS_CONTENTS,
					core_header.len,
					(bfd_vma) offsetof (struct proc_info,
							    hw_regs),
					2))
		  goto fail;
              }
	    core_signal (abfd) = proc_info.sig;
            if (bfd_seek (abfd, (file_ptr) core_header.len, SEEK_CUR) != 0)
              break;
            good_sections++;
	  }
	  break;

	case CORE_DATA:
	case CORE_STACK:
	case CORE_TEXT:
	case CORE_MMF:
	case CORE_SHM:
	case CORE_ANON_SHMEM:
	  if (!make_bfd_asection (abfd, ".data",
				  SEC_ALLOC + SEC_LOAD + SEC_HAS_CONTENTS,
				  core_header.len,
				  (bfd_vma) core_header.addr, 2))
	    goto fail;

	  bfd_seek (abfd, (file_ptr) core_header.len, SEEK_CUR);
          good_sections++;
	  break;

	case CORE_NONE:
          /* Let's not punt if we encounter a section of unknown
             type.  Rather, let's make a note of it.  If we later
             see that there were also "good" sections, then we'll
             declare that this a core file, but we'll also warn that
             it may be incompatible with this gdb.
             */
	  unknown_sections++;
          break;

         default:
	   goto fail; /*unrecognized core file type */
	}
    }

  /* OK, we believe you.  You're a core file (sure, sure).  */

  /* On HP/UX, we sometimes encounter core files where none of the threads
     was found to be the running thread (ie the signal was set to -1 for
     all threads).  This happens when the program was aborted externally
     via a TT_CORE ttrace system call.  In that case, we just pick one
     thread at random to be the active thread.  */
  if (core_kernel_thread_id (abfd) != 0
      && bfd_get_section_by_name (abfd, ".reg") == NULL)
    {
      asection *asect = bfd_sections_find_if (abfd, thread_section_p, NULL);
      asection *reg_sect;

      if (asect != NULL)
        {
          reg_sect = make_bfd_asection (abfd, ".reg", asect->flags,
                                        asect->size, asect->vma,
                                        asect->alignment_power);
          if (reg_sect == NULL)
            goto fail;

          reg_sect->filepos = asect->filepos;
        }
    }

  /* Were there sections of unknown type?  If so, yet there were
     at least some complete sections of known type, then, issue
     a warning.  Possibly the core file was generated on a version
     of HP-UX that is incompatible with that for which this gdb was
     built.
     */
  if ((unknown_sections > 0) && (good_sections > 0))
    (*_bfd_error_handler)
      ("%s appears to be a core file,\nbut contains unknown sections.  It may have been created on an incompatible\nversion of HP-UX.  As a result, some information may be unavailable.\n",
       abfd->filename);

  return abfd->xvec;

 fail:
  bfd_release (abfd, core_hdr (abfd));
  core_hdr (abfd) = NULL;
  bfd_section_list_clear (abfd);
  return NULL;
}

static char *
hpux_core_core_file_failing_command (bfd *abfd)
{
  return core_command (abfd);
}

static int
hpux_core_core_file_failing_signal (bfd *abfd)
{
  return core_signal (abfd);
}


/* If somebody calls any byte-swapping routines, shoot them.  */
static void
swap_abort (void)
{
  abort(); /* This way doesn't require any declaration for ANSI to fuck up */
}

#define	NO_GET ((bfd_vma (*) (const void *)) swap_abort)
#define	NO_PUT ((void (*) (bfd_vma, void *)) swap_abort)
#define	NO_GETS ((bfd_signed_vma (*) (const void *)) swap_abort)
#define	NO_GET64 ((bfd_uint64_t (*) (const void *)) swap_abort)
#define	NO_PUT64 ((void (*) (bfd_uint64_t, void *)) swap_abort)
#define	NO_GETS64 ((bfd_int64_t (*) (const void *)) swap_abort)

const bfd_target hpux_core_vec =
  {
    "hpux-core",
    bfd_target_unknown_flavour,
    BFD_ENDIAN_BIG,		/* target byte order */
    BFD_ENDIAN_BIG,		/* target headers byte order */
    (HAS_RELOC | EXEC_P |	/* object flags */
     HAS_LINENO | HAS_DEBUG |
     HAS_SYMS | HAS_LOCALS | WP_TEXT | D_PAGED),
    (SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD | SEC_RELOC), /* section flags */
    0,			                                   /* symbol prefix */
    ' ',						   /* ar_pad_char */
    16,							   /* ar_max_namelen */
    NO_GET64, NO_GETS64, NO_PUT64,	/* 64 bit data */
    NO_GET, NO_GETS, NO_PUT,		/* 32 bit data */
    NO_GET, NO_GETS, NO_PUT,		/* 16 bit data */
    NO_GET64, NO_GETS64, NO_PUT64,	/* 64 bit hdrs */
    NO_GET, NO_GETS, NO_PUT,		/* 32 bit hdrs */
    NO_GET, NO_GETS, NO_PUT,		/* 16 bit hdrs */

    {				/* bfd_check_format */
      _bfd_dummy_target,		/* unknown format */
      _bfd_dummy_target,		/* object file */
      _bfd_dummy_target,		/* archive */
      hpux_core_core_file_p		/* a core file */
    },
    {				/* bfd_set_format */
      bfd_false, bfd_false,
      bfd_false, bfd_false
    },
    {				/* bfd_write_contents */
      bfd_false, bfd_false,
      bfd_false, bfd_false
    },

    BFD_JUMP_TABLE_GENERIC (_bfd_generic),
    BFD_JUMP_TABLE_COPY (_bfd_generic),
    BFD_JUMP_TABLE_CORE (hpux_core),
    BFD_JUMP_TABLE_ARCHIVE (_bfd_noarchive),
    BFD_JUMP_TABLE_SYMBOLS (_bfd_nosymbols),
    BFD_JUMP_TABLE_RELOCS (_bfd_norelocs),
    BFD_JUMP_TABLE_WRITE (_bfd_generic),
    BFD_JUMP_TABLE_LINK (_bfd_nolink),
    BFD_JUMP_TABLE_DYNAMIC (_bfd_nodynamic),

    NULL,

    (PTR) 0			/* backend_data */
  };
