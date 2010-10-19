/* NLM (NetWare Loadable Module) executable support for BFD.
   Copyright 1993, 1994, 2001, 2002, 2003, 2005
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
#include "libbfd.h"
#include "libnlm.h"

/* Make an NLM object.  We just need to allocate the backend
   information.  */

bfd_boolean
nlm_mkobject (bfd * abfd)
{
  bfd_size_type amt = sizeof (struct nlm_obj_tdata);

  nlm_tdata (abfd) = bfd_zalloc (abfd, amt);
  if (nlm_tdata (abfd) == NULL)
    return FALSE;

  if (nlm_architecture (abfd) != bfd_arch_unknown)
    bfd_default_set_arch_mach (abfd, nlm_architecture (abfd),
			       nlm_machine (abfd));

  /* Since everything is done at close time, do we need any initialization ?  */
  return TRUE;
}

/* Set the architecture and machine for an NLM object.  */

bfd_boolean
nlm_set_arch_mach (bfd * abfd,
		   enum bfd_architecture arch,
		   unsigned long machine)
{
  bfd_default_set_arch_mach (abfd, arch, machine);
  return arch == nlm_architecture (abfd);
}
