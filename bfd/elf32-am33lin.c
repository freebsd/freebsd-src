/* Matsushita AM33/2.0 support for 32-bit GNU/Linux ELF
   Copyright 2003, 2005
   Free Software Foundation, Inc.

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

#include "bfd.h"
#include "sysdep.h"
#include "elf-bfd.h"
#include "elf/mn10300.h"

#define elf_symbol_leading_char 0

#define TARGET_LITTLE_SYM	bfd_elf32_am33lin_vec
#define TARGET_LITTLE_NAME	"elf32-am33lin"
#define ELF_ARCH		bfd_arch_mn10300
#define ELF_MACHINE_CODE	EM_MN10300
#define ELF_MACHINE_ALT1	EM_CYGNUS_MN10300
#define ELF_MAXPAGESIZE		0x1000

/* Rename global functions.  */
#define _bfd_mn10300_elf_merge_private_bfd_data  _bfd_am33_elf_merge_private_bfd_data
#define _bfd_mn10300_elf_object_p                _bfd_am33_elf_object_p
#define _bfd_mn10300_elf_final_write_processing  _bfd_am33_elf_final_write_processing

/* Support for core dump NOTE sections.  */
static bfd_boolean
elf32_am33lin_grok_prstatus (bfd *abfd, Elf_Internal_Note *note)
{
  int offset;
  unsigned int size;

  switch (note->descsz)
    {
      default:
	return FALSE;

      case 188:		/* Linux/am33 */
	/* pr_cursig */
	elf_tdata (abfd)->core_signal = bfd_get_16 (abfd, note->descdata + 12);

	/* pr_pid */
	elf_tdata (abfd)->core_pid = bfd_get_32 (abfd, note->descdata + 24);

	/* pr_reg */
	offset = 72;
	size = 112;

	break;
    }

  /* Make a ".reg/999" section.  */
  return _bfd_elfcore_make_pseudosection (abfd, ".reg", size,
					  note->descpos + offset);
}

static bfd_boolean
elf32_am33lin_grok_psinfo (bfd *abfd, Elf_Internal_Note *note)
{
  switch (note->descsz)
    {
      default:
	return FALSE;

      case 124:		/* Linux/am33 elf_prpsinfo */
	elf_tdata (abfd)->core_program
	 = _bfd_elfcore_strndup (abfd, note->descdata + 28, 16);
	elf_tdata (abfd)->core_command
	 = _bfd_elfcore_strndup (abfd, note->descdata + 44, 80);
    }

  /* Note that for some reason, a spurious space is tacked
     onto the end of the args in some (at least one anyway)
     implementations, so strip it off if it exists.  */

  {
    char *command = elf_tdata (abfd)->core_command;
    int n = strlen (command);

    if (0 < n && command[n - 1] == ' ')
      command[n - 1] = '\0';
  }

  return TRUE;
}

#define elf_backend_grok_prstatus	elf32_am33lin_grok_prstatus
#define elf_backend_grok_psinfo		elf32_am33lin_grok_psinfo

#include "elf-m10300.c"
