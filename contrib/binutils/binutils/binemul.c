/* Binutils emulation layer.
   Copyright (C) 2002 Free Software Foundation, Inc.
   Written by Tom Rix, Redhat.

   This file is part of GNU Binutils.

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

#include "binemul.h"

extern bin_emulation_xfer_type bin_dummy_emulation;

void
ar_emul_usage (fp)
     FILE *fp;
{
  if (bin_dummy_emulation.ar_usage)
    bin_dummy_emulation.ar_usage (fp);
}

void
ar_emul_default_usage (fp)
     FILE *fp;
{
  AR_EMUL_USAGE_PRINT_OPTION_HEADER (fp);
  /* xgettext:c-format */
  fprintf (fp, _("  No emulation specific options\n"));
}

boolean
ar_emul_append (after_bfd, file_name, verbose)
     bfd **after_bfd;
     char *file_name;
     boolean verbose;
{
  if (bin_dummy_emulation.ar_append)
    return bin_dummy_emulation.ar_append (after_bfd, file_name, verbose);

  return false;
}

boolean
ar_emul_default_append (after_bfd, file_name, verbose)
     bfd **after_bfd;
     char *file_name;
     boolean verbose;
{
  bfd *temp;

  temp = *after_bfd;
  *after_bfd = bfd_openr (file_name, NULL);

  AR_EMUL_ELEMENT_CHECK (*after_bfd, file_name);
  AR_EMUL_APPEND_PRINT_VERBOSE (verbose, file_name);

  (*after_bfd)->next = temp;

  return true;
}

boolean
ar_emul_replace (after_bfd, file_name, verbose)
     bfd **after_bfd;
     char *file_name;
     boolean verbose;
{
  if (bin_dummy_emulation.ar_replace)
    return bin_dummy_emulation.ar_replace (after_bfd, file_name, verbose);

  return false;
}

boolean
ar_emul_default_replace (after_bfd, file_name, verbose)
     bfd **after_bfd;
     char *file_name;
     boolean verbose;
{
  bfd *temp;

  temp = *after_bfd;
  *after_bfd = bfd_openr (file_name, NULL);

  AR_EMUL_ELEMENT_CHECK (*after_bfd, file_name);
  AR_EMUL_REPLACE_PRINT_VERBOSE (verbose, file_name);

  (*after_bfd)->next = temp;

  return true;
}

boolean
ar_emul_create (abfd_out, archive_file_name, file_name)
     bfd **abfd_out;
     char *archive_file_name;
     char *file_name;
{
  if (bin_dummy_emulation.ar_create)
    return bin_dummy_emulation.ar_create (abfd_out, archive_file_name,
					  file_name);

  return false;
}

boolean
ar_emul_default_create (abfd_out, archive_file_name, file_name)
     bfd **abfd_out;
     char *archive_file_name;
     char *file_name;
{
  char *target = NULL;

  /* Try to figure out the target to use for the archive from the
     first object on the list.  */
  if (file_name != NULL)
    {
      bfd *obj;

      obj = bfd_openr (file_name, NULL);
      if (obj != NULL)
	{
	  if (bfd_check_format (obj, bfd_object))
	    target = bfd_get_target (obj);
	  (void) bfd_close (obj);
	}
    }

  /* Create an empty archive.  */
  *abfd_out = bfd_openw (archive_file_name, target);
  if (*abfd_out == NULL
      || ! bfd_set_format (*abfd_out, bfd_archive)
      || ! bfd_close (*abfd_out))
    bfd_fatal (archive_file_name);

  return true;
}

boolean
ar_emul_parse_arg (arg)
     char *arg;
{
  if (bin_dummy_emulation.ar_parse_arg)
    return bin_dummy_emulation.ar_parse_arg (arg);

  return false;
}

boolean
ar_emul_default_parse_arg (arg)
     char *arg ATTRIBUTE_UNUSED;
{
  return false;
}
