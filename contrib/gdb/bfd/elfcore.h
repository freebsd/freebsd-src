/* ELF core file support for BFD.
   Copyright (C) 1995, 1996 Free Software Foundation, Inc.

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
/* Core file support */

#ifdef HAVE_SYS_PROCFS_H		/* Some core file support requires host /proc files */
#include <signal.h>
#include <sys/procfs.h>
#else
#define bfd_prstatus(abfd, descdata, descsz, filepos) true
#define bfd_fpregset(abfd, descdata, descsz, filepos) true
#define bfd_prpsinfo(abfd, descdata, descsz, filepos) true
#endif

#ifdef HAVE_SYS_PROCFS_H

static boolean
bfd_prstatus (abfd, descdata, descsz, filepos)
     bfd *abfd;
     char *descdata;
     int descsz;
     long filepos;
{
  asection *newsect;
  prstatus_t *status = (prstatus_t *) 0;

  if (descsz == sizeof (prstatus_t))
    {
      newsect = bfd_make_section (abfd, ".reg");
      if (newsect == NULL)
	return false;
      newsect->_raw_size = sizeof (status->pr_reg);
      newsect->filepos = filepos + (long) &status->pr_reg;
      newsect->flags = SEC_HAS_CONTENTS;
      newsect->alignment_power = 2;
      if ((core_prstatus (abfd) = bfd_alloc (abfd, descsz)) != NULL)
	{
	  memcpy (core_prstatus (abfd), descdata, descsz);
	}
    }
  return true;
}

/* Stash a copy of the prpsinfo structure away for future use. */

static boolean
bfd_prpsinfo (abfd, descdata, descsz, filepos)
     bfd *abfd;
     char *descdata;
     int descsz;
     long filepos;
{
  if (descsz == sizeof (prpsinfo_t))
    {
      if ((core_prpsinfo (abfd) = bfd_alloc (abfd, descsz)) == NULL)
	return false;
      memcpy (core_prpsinfo (abfd), descdata, descsz);
    }
  return true;
}

static boolean
bfd_fpregset (abfd, descdata, descsz, filepos)
     bfd *abfd;
     char *descdata;
     int descsz;
     long filepos;
{
  asection *newsect;

  newsect = bfd_make_section (abfd, ".reg2");
  if (newsect == NULL)
    return false;
  newsect->_raw_size = descsz;
  newsect->filepos = filepos;
  newsect->flags = SEC_HAS_CONTENTS;
  newsect->alignment_power = 2;
  return true;
}

#endif /* HAVE_SYS_PROCFS_H */

/* Return a pointer to the args (including the command name) that were
   seen by the program that generated the core dump.  Note that for
   some reason, a spurious space is tacked onto the end of the args
   in some (at least one anyway) implementations, so strip it off if
   it exists. */

char *
elf_core_file_failing_command (abfd)
     bfd *abfd;
{
#ifdef HAVE_SYS_PROCFS_H
  if (core_prpsinfo (abfd))
    {
      prpsinfo_t *p = core_prpsinfo (abfd);
      char *scan = p->pr_psargs;
      while (*scan++)
	{;
	}
      scan -= 2;
      if ((scan > p->pr_psargs) && (*scan == ' '))
	{
	  *scan = '\000';
	}
      return p->pr_psargs;
    }
#endif
  return NULL;
}

/* Return the number of the signal that caused the core dump.  Presumably,
   since we have a core file, we got a signal of some kind, so don't bother
   checking the other process status fields, just return the signal number.
   */

int
elf_core_file_failing_signal (abfd)
     bfd *abfd;
{
#ifdef HAVE_SYS_PROCFS_H
  if (core_prstatus (abfd))
    {
      return ((prstatus_t *) (core_prstatus (abfd)))->pr_cursig;
    }
#endif
  return -1;
}

/* Check to see if the core file could reasonably be expected to have
   come for the current executable file.  Note that by default we return
   true unless we find something that indicates that there might be a
   problem.
   */

boolean
elf_core_file_matches_executable_p (core_bfd, exec_bfd)
     bfd *core_bfd;
     bfd *exec_bfd;
{
#ifdef HAVE_SYS_PROCFS_H
  char *corename;
  char *execname;
#endif

  /* First, xvecs must match since both are ELF files for the same target. */

  if (core_bfd->xvec != exec_bfd->xvec)
    {
      bfd_set_error (bfd_error_system_call);
      return false;
    }

#ifdef HAVE_SYS_PROCFS_H

  /* If no prpsinfo, just return true.  Otherwise, grab the last component
     of the exec'd pathname from the prpsinfo. */

  if (core_prpsinfo (core_bfd))
    {
      corename = (((prpsinfo_t *) core_prpsinfo (core_bfd))->pr_fname);
    }
  else
    {
      return true;
    }

  /* Find the last component of the executable pathname. */

  if ((execname = strrchr (exec_bfd->filename, '/')) != NULL)
    {
      execname++;
    }
  else
    {
      execname = (char *) exec_bfd->filename;
    }

  /* See if they match */

  return strcmp (execname, corename) ? false : true;

#else

  return true;

#endif /* HAVE_SYS_PROCFS_H */
}

/* ELF core files contain a segment of type PT_NOTE, that holds much of
   the information that would normally be available from the /proc interface
   for the process, at the time the process dumped core.  Currently this
   includes copies of the prstatus, prpsinfo, and fpregset structures.

   Since these structures are potentially machine dependent in size and
   ordering, bfd provides two levels of support for them.  The first level,
   available on all machines since it does not require that the host
   have /proc support or the relevant include files, is to create a bfd
   section for each of the prstatus, prpsinfo, and fpregset structures,
   without any interpretation of their contents.  With just this support,
   the bfd client will have to interpret the structures itself.  Even with
   /proc support, it might want these full structures for it's own reasons.

   In the second level of support, where HAVE_SYS_PROCFS_H is defined,
   bfd will pick apart the structures to gather some additional
   information that clients may want, such as the general register
   set, the name of the exec'ed file and its arguments, the signal (if
   any) that caused the core dump, etc.

   */

static boolean
elf_corefile_note (abfd, hdr)
     bfd *abfd;
     Elf_Internal_Phdr *hdr;
{
  Elf_External_Note *x_note_p;	/* Elf note, external form */
  Elf_Internal_Note i_note;	/* Elf note, internal form */
  char *buf = NULL;		/* Entire note segment contents */
  char *namedata;		/* Name portion of the note */
  char *descdata;		/* Descriptor portion of the note */
  char *sectname;		/* Name to use for new section */
  long filepos;			/* File offset to descriptor data */
  asection *newsect;

  if (hdr->p_filesz > 0
      && (buf = (char *) bfd_malloc ((size_t) hdr->p_filesz)) != NULL
      && bfd_seek (abfd, hdr->p_offset, SEEK_SET) != -1
      && bfd_read ((PTR) buf, hdr->p_filesz, 1, abfd) == hdr->p_filesz)
    {
      x_note_p = (Elf_External_Note *) buf;
      while ((char *) x_note_p < (buf + hdr->p_filesz))
	{
	  i_note.namesz = bfd_h_get_32 (abfd, (bfd_byte *) x_note_p->namesz);
	  i_note.descsz = bfd_h_get_32 (abfd, (bfd_byte *) x_note_p->descsz);
	  i_note.type = bfd_h_get_32 (abfd, (bfd_byte *) x_note_p->type);
	  namedata = x_note_p->name;
	  descdata = namedata + BFD_ALIGN (i_note.namesz, 4);
	  filepos = hdr->p_offset + (descdata - buf);
	  switch (i_note.type)
	    {
	    case NT_PRSTATUS:
	      /* process descdata as prstatus info */
	      if (! bfd_prstatus (abfd, descdata, i_note.descsz, filepos))
		return false;
	      sectname = ".prstatus";
	      break;
	    case NT_FPREGSET:
	      /* process descdata as fpregset info */
	      if (! bfd_fpregset (abfd, descdata, i_note.descsz, filepos))
		return false;
	      sectname = ".fpregset";
	      break;
	    case NT_PRPSINFO:
	      /* process descdata as prpsinfo */
	      if (! bfd_prpsinfo (abfd, descdata, i_note.descsz, filepos))
		return false;
	      sectname = ".prpsinfo";
	      break;
	    default:
	      /* Unknown descriptor, just ignore it. */
	      sectname = NULL;
	      break;
	    }
	  if (sectname != NULL)
	    {
	      newsect = bfd_make_section (abfd, sectname);
	      if (newsect == NULL)
		return false;
	      newsect->_raw_size = i_note.descsz;
	      newsect->filepos = filepos;
	      newsect->flags = SEC_ALLOC | SEC_HAS_CONTENTS;
	      newsect->alignment_power = 2;
	    }
	  x_note_p = (Elf_External_Note *)
	    (descdata + BFD_ALIGN (i_note.descsz, 4));
	}
    }
  if (buf != NULL)
    {
      free (buf);
    }
  else if (hdr->p_filesz > 0)
    {
      return false;
    }
  return true;

}

/*  Core files are simply standard ELF formatted files that partition
    the file using the execution view of the file (program header table)
    rather than the linking view.  In fact, there is no section header
    table in a core file.

    The process status information (including the contents of the general
    register set) and the floating point register set are stored in a
    segment of type PT_NOTE.  We handcraft a couple of extra bfd sections
    that allow standard bfd access to the general registers (.reg) and the
    floating point registers (.reg2).

 */

const bfd_target *
elf_core_file_p (abfd)
     bfd *abfd;
{
  Elf_External_Ehdr x_ehdr;	/* Elf file header, external form */
  Elf_Internal_Ehdr *i_ehdrp;	/* Elf file header, internal form */
  Elf_External_Phdr x_phdr;	/* Program header table entry, external form */
  Elf_Internal_Phdr *i_phdrp;	/* Program header table, internal form */
  unsigned int phindex;
  struct elf_backend_data *ebd;

  /* Read in the ELF header in external format.  */

  if (bfd_read ((PTR) & x_ehdr, sizeof (x_ehdr), 1, abfd) != sizeof (x_ehdr))
    {
      if (bfd_get_error () != bfd_error_system_call)
	bfd_set_error (bfd_error_wrong_format);
      return NULL;
    }

  /* Now check to see if we have a valid ELF file, and one that BFD can
     make use of.  The magic number must match, the address size ('class')
     and byte-swapping must match our XVEC entry, and it must have a
     program header table (FIXME: See comments re segments at top of this
     file). */

  if (elf_file_p (&x_ehdr) == false)
    {
    wrong:
      bfd_set_error (bfd_error_wrong_format);
      return NULL;
    }

  /* FIXME, Check EI_VERSION here !  */

  {
#if ARCH_SIZE == 32
    int desired_address_size = ELFCLASS32;
#endif
#if ARCH_SIZE == 64
    int desired_address_size = ELFCLASS64;
#endif

    if (x_ehdr.e_ident[EI_CLASS] != desired_address_size)
      goto wrong;
  }

  /* Switch xvec to match the specified byte order.  */
  switch (x_ehdr.e_ident[EI_DATA])
    {
    case ELFDATA2MSB:		/* Big-endian */
      if (! bfd_big_endian (abfd))
	goto wrong;
      break;
    case ELFDATA2LSB:		/* Little-endian */
      if (! bfd_little_endian (abfd))
	goto wrong;
      break;
    case ELFDATANONE:		/* No data encoding specified */
    default:			/* Unknown data encoding specified */
      goto wrong;
    }

  /* Allocate an instance of the elf_obj_tdata structure and hook it up to
     the tdata pointer in the bfd. */

  elf_tdata (abfd) =
    (struct elf_obj_tdata *) bfd_zalloc (abfd, sizeof (struct elf_obj_tdata));
  if (elf_tdata (abfd) == NULL)
    return NULL;

  /* FIXME, `wrong' returns from this point onward, leak memory.  */

  /* Now that we know the byte order, swap in the rest of the header */
  i_ehdrp = elf_elfheader (abfd);
  elf_swap_ehdr_in (abfd, &x_ehdr, i_ehdrp);
#if DEBUG & 1
  elf_debug_file (i_ehdrp);
#endif

  ebd = get_elf_backend_data (abfd);

  /* Check that the ELF e_machine field matches what this particular
     BFD format expects.  */
  if (ebd->elf_machine_code != i_ehdrp->e_machine
      && (ebd->elf_machine_alt1 == 0 || i_ehdrp->e_machine != ebd->elf_machine_alt1)
      && (ebd->elf_machine_alt2 == 0 || i_ehdrp->e_machine != ebd->elf_machine_alt2))
    {
      const bfd_target * const *target_ptr;

      if (ebd->elf_machine_code != EM_NONE)
	goto wrong;

      /* This is the generic ELF target.  Let it match any ELF target
	 for which we do not have a specific backend.  */
      for (target_ptr = bfd_target_vector; *target_ptr != NULL; target_ptr++)
	{
	  struct elf_backend_data *back;

	  if ((*target_ptr)->flavour != bfd_target_elf_flavour)
	    continue;
	  back = (struct elf_backend_data *) (*target_ptr)->backend_data;
	  if (back->elf_machine_code == i_ehdrp->e_machine)
	    {
	      /* target_ptr is an ELF backend which matches this
		 object file, so reject the generic ELF target.  */
	      goto wrong;
	    }
	}
    }

  /* If there is no program header, or the type is not a core file, then
     we are hosed. */
  if (i_ehdrp->e_phoff == 0 || i_ehdrp->e_type != ET_CORE)
    goto wrong;

  /* Allocate space for a copy of the program header table in
     internal form, seek to the program header table in the file,
     read it in, and convert it to internal form.  As a simple sanity
     check, verify that the what BFD thinks is the size of each program
     header table entry actually matches the size recorded in the file. */

  if (i_ehdrp->e_phentsize != sizeof (x_phdr))
    goto wrong;
  i_phdrp = (Elf_Internal_Phdr *)
    bfd_alloc (abfd, sizeof (*i_phdrp) * i_ehdrp->e_phnum);
  if (!i_phdrp)
    return NULL;
  if (bfd_seek (abfd, i_ehdrp->e_phoff, SEEK_SET) == -1)
    return NULL;
  for (phindex = 0; phindex < i_ehdrp->e_phnum; phindex++)
    {
      if (bfd_read ((PTR) & x_phdr, sizeof (x_phdr), 1, abfd)
	  != sizeof (x_phdr))
	return NULL;
      elf_swap_phdr_in (abfd, &x_phdr, i_phdrp + phindex);
    }

  /* Once all of the program headers have been read and converted, we
     can start processing them. */

  for (phindex = 0; phindex < i_ehdrp->e_phnum; phindex++)
    {
      bfd_section_from_phdr (abfd, i_phdrp + phindex, phindex);
      if ((i_phdrp + phindex)->p_type == PT_NOTE)
	{
	  if (! elf_corefile_note (abfd, i_phdrp + phindex))
	    return NULL;
	}
    }

  /* Remember the entry point specified in the ELF file header. */

  bfd_get_start_address (abfd) = i_ehdrp->e_entry;

  return abfd->xvec;
}
