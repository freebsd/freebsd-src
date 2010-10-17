/* Binutils emulation layer.
   Copyright 2002, 2003 Free Software Foundation, Inc.
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
#include "bfdlink.h"
#include "coff/internal.h"
#include "coff/xcoff.h"
#include "libcoff.h"
#include "libxcoff.h"

/* Default to <bigaf>.  */
static bfd_boolean big_archive = TRUE;

/* Whether to include 32 bit objects.  */
static bfd_boolean X32 = TRUE;

/* Whether to include 64 bit objects.  */
static bfd_boolean X64 = FALSE;

static void ar_emul_aix_usage (FILE *);
static bfd_boolean ar_emul_aix_append (bfd **, char *, bfd_boolean);
static bfd_boolean ar_emul_aix5_append (bfd **, char *, bfd_boolean);
static bfd_boolean ar_emul_aix_replace (bfd **, char *, bfd_boolean);
static bfd_boolean ar_emul_aix5_replace (bfd **, char *, bfd_boolean);
static bfd_boolean ar_emul_aix_parse_arg (char *);
static bfd_boolean ar_emul_aix_internal
  (bfd **, char *, bfd_boolean, const char *, bfd_boolean);

static void
ar_emul_aix_usage (FILE *fp)
{
  AR_EMUL_USAGE_PRINT_OPTION_HEADER (fp);
  /* xgettext:c-format */
  fprintf (fp, _("  [-g]         - 32 bit small archive\n"));
  fprintf (fp, _("  [-X32]       - ignores 64 bit objects\n"));
  fprintf (fp, _("  [-X64]       - ignores 32 bit objects\n"));
  fprintf (fp, _("  [-X32_64]    - accepts 32 and 64 bit objects\n"));
}

static bfd_boolean
ar_emul_aix_internal (bfd **after_bfd, char *file_name, bfd_boolean verbose,
		      const char * target_name, bfd_boolean is_append)
{
  bfd *temp;
  bfd *try_bfd;

  temp = *after_bfd;

  /* Try 64 bit.  */
  try_bfd = bfd_openr (file_name, target_name);

  /* Failed or the object is possibly 32 bit.  */
  if (NULL == try_bfd || ! bfd_check_format (try_bfd, bfd_object))
    try_bfd = bfd_openr (file_name, "aixcoff-rs6000");

  AR_EMUL_ELEMENT_CHECK (try_bfd, file_name);

  if (bfd_xcoff_is_xcoff64 (try_bfd) && (! X64))
    return FALSE;

  if (bfd_xcoff_is_xcoff32 (try_bfd)
      && bfd_check_format (try_bfd, bfd_object) && (! X32))
    return FALSE;

  if (is_append)
    {
      AR_EMUL_APPEND_PRINT_VERBOSE (verbose, file_name);
    }
  else
    {
      AR_EMUL_REPLACE_PRINT_VERBOSE (verbose, file_name);
    }

  *after_bfd = try_bfd;
  (*after_bfd)->next = temp;

  return TRUE;
}


static bfd_boolean
ar_emul_aix_append (bfd **after_bfd, char *file_name, bfd_boolean verbose)
{
  return ar_emul_aix_internal (after_bfd, file_name, verbose,
			       "aixcoff64-rs6000", TRUE);
}

static bfd_boolean
ar_emul_aix5_append (bfd **after_bfd, char *file_name, bfd_boolean verbose)
{
  return ar_emul_aix_internal (after_bfd, file_name, verbose,
			       "aix5coff64-rs6000", TRUE);
}

static bfd_boolean
ar_emul_aix_replace (bfd **after_bfd, char *file_name, bfd_boolean verbose)
{
  return ar_emul_aix_internal (after_bfd, file_name, verbose,
			       "aixcoff64-rs6000", FALSE);
}

static bfd_boolean
ar_emul_aix5_replace (bfd **after_bfd, char *file_name, bfd_boolean verbose)
{
  return ar_emul_aix_internal (after_bfd, file_name, verbose,
			       "aix5coff64-rs6000", FALSE);
}

static bfd_boolean
ar_emul_aix_parse_arg (char *arg)
{
  if (strncmp (arg, "-X32_64", 6) == 0)
    {
      big_archive = TRUE;
      X32 = TRUE;
      X64 = TRUE;
    }
  else if (strncmp (arg, "-X32", 3) == 0)
    {
      big_archive = TRUE;
      X32 = TRUE;
      X64 = FALSE;
    }
  else if (strncmp (arg, "-X64", 3) == 0)
    {
      big_archive = TRUE;
      X32 = FALSE;
      X64 = TRUE;
    }
  else if (strncmp (arg, "-g", 2) == 0)
    {
      big_archive = FALSE;
      X32 = TRUE;
      X64 = FALSE;
    }
  else
    return FALSE;

  return TRUE;
}

struct bin_emulation_xfer_struct bin_aix_emulation =
{
  ar_emul_aix_usage,
  ar_emul_aix_append,
  ar_emul_aix_replace,
  ar_emul_default_create,
  ar_emul_aix_parse_arg,
};

struct bin_emulation_xfer_struct bin_aix5_emulation =
{
  ar_emul_aix_usage,
  ar_emul_aix5_append,
  ar_emul_aix5_replace,
  ar_emul_default_create,
  ar_emul_aix_parse_arg,
};
