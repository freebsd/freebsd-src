/* ELF core file support for BFD.
   Copyright (C) 1995, 1996, 1997, 1998 Free Software Foundation, Inc.

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


char*
elf_core_file_failing_command (abfd)
     bfd *abfd;
{
  return elf_tdata (abfd)->core_command;
}

int
elf_core_file_failing_signal (abfd)
     bfd *abfd;
{
  return elf_tdata (abfd)->core_signal;
}


boolean
elf_core_file_matches_executable_p (core_bfd, exec_bfd)
     bfd *core_bfd;
     bfd *exec_bfd;
{
  char* corename;

  /* xvecs must match if both are ELF files for the same target. */

  if (core_bfd->xvec != exec_bfd->xvec)
    {
      bfd_set_error (bfd_error_system_call);
      return false;
    }

  /* See if the name in the corefile matches the executable name. */

  corename = elf_tdata (core_bfd)->core_program;
  if (corename != NULL)
    {
      const char* execname = strrchr (exec_bfd->filename, '/');
      execname = execname ? execname + 1 : exec_bfd->filename;

      if (strcmp(execname, corename) != 0)
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
  Elf_Internal_Phdr *i_phdrp;	/* Elf program header, internal form */
  unsigned int phindex;
  struct elf_backend_data *ebd;

  /* Read in the ELF header in external format.  */
  if (bfd_read ((PTR) & x_ehdr, sizeof (x_ehdr), 1, abfd) != sizeof (x_ehdr))
    {
      if (bfd_get_error () != bfd_error_system_call)
	bfd_set_error (bfd_error_wrong_format);
      return NULL;
    }

  /* Check the magic number. */
  if (elf_file_p (&x_ehdr) == false)
    {
    wrong:
      bfd_set_error (bfd_error_wrong_format);
      return NULL;
    }

  /* FIXME: Check EI_VERSION here ! */

  /* Check the address size ("class"). */
  if (x_ehdr.e_ident[EI_CLASS] != ELFCLASS)
    goto wrong;

  /* Check the byteorder. */
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
    default:
      goto wrong;
    }

  /* Give abfd an elf_obj_tdata. */
  elf_tdata (abfd) =
    (struct elf_obj_tdata *) bfd_zalloc (abfd, sizeof (struct elf_obj_tdata));
  if (elf_tdata (abfd) == NULL)
    return NULL;

  /* FIXME: from here on down, "goto wrong" will leak memory.  */

  /* Swap in the rest of the header, now that we have the byte order. */
  i_ehdrp = elf_elfheader (abfd);
  elf_swap_ehdr_in (abfd, &x_ehdr, i_ehdrp);

#if DEBUG & 1
  elf_debug_file (i_ehdrp);
#endif

  ebd = get_elf_backend_data (abfd);

  /* Check that the ELF e_machine field matches what this particular
     BFD format expects.  */

  if (ebd->elf_machine_code != i_ehdrp->e_machine
      && (ebd->elf_machine_alt1 == 0
	  || i_ehdrp->e_machine != ebd->elf_machine_alt1)
      && (ebd->elf_machine_alt2 == 0
	  || i_ehdrp->e_machine != ebd->elf_machine_alt2))
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

  /* Does BFD's idea of the phdr size match the size
     recorded in the file? */
  if (i_ehdrp->e_phentsize != sizeof (Elf_External_Phdr))
    goto wrong;

  /* Allocate space for the program headers. */
  i_phdrp = (Elf_Internal_Phdr *)
    bfd_alloc (abfd, sizeof (*i_phdrp) * i_ehdrp->e_phnum);
  if (!i_phdrp)
    return NULL;

  elf_tdata (abfd)->phdr = i_phdrp;

  /* Read and convert to internal form. */
  for (phindex = 0; phindex < i_ehdrp->e_phnum; ++phindex)
    {
      Elf_External_Phdr x_phdr;
      if (bfd_read ((PTR) &x_phdr, sizeof (x_phdr), 1, abfd)
	  != sizeof (x_phdr))
	return NULL;

      elf_swap_phdr_in (abfd, &x_phdr, i_phdrp + phindex);
    }

  /* Process each program header. */
  for (phindex = 0; phindex < i_ehdrp->e_phnum; ++phindex)
    {
      if (!_bfd_elfcore_section_from_phdr (abfd, i_phdrp + phindex, phindex))
	return NULL;
    }

  /* Set the machine architecture. */
  if (! bfd_default_set_arch_mach (abfd, ebd->arch, 0))
    {
      /* It's OK if this fails for the generic target.  */
      if (ebd->elf_machine_code != EM_NONE)
	return NULL;
    }

  /* Save the entry point from the ELF header. */
  bfd_get_start_address (abfd) = i_ehdrp->e_entry;

  return abfd->xvec;
}
