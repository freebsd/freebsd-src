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

#ifndef BINEMUL_H
#define BINEMUL_H

#include "bfd.h"
#include "bucomm.h"

extern void ar_emul_usage                PARAMS ((FILE *));
extern void ar_emul_default_usage        PARAMS ((FILE *));
extern boolean ar_emul_append            PARAMS ((bfd **, char *, boolean));
extern boolean ar_emul_default_append    PARAMS ((bfd **, char *, boolean));
extern boolean ar_emul_replace           PARAMS ((bfd **, char *, boolean));
extern boolean ar_emul_default_replace   PARAMS ((bfd **, char *, boolean));
extern boolean ar_emul_create            PARAMS ((bfd **, char *, char *));
extern boolean ar_emul_default_create    PARAMS ((bfd **, char *, char *));
extern boolean ar_emul_parse_arg         PARAMS ((char *));
extern boolean ar_emul_default_parse_arg PARAMS ((char *));

/* Macros for common output.  */

#define AR_EMUL_USAGE_PRINT_OPTION_HEADER(fp) \
  /* xgettext:c-format */                     \
  fprintf (fp, _(" emulation options: \n"))

#define AR_EMUL_ELEMENT_CHECK(abfd, file_name) \
  do { if ((abfd) == (bfd *) NULL) bfd_fatal (file_name); } while (0)

#define AR_EMUL_APPEND_PRINT_VERBOSE(verbose, file_name) \
  do { if (verbose) printf ("a - %s\n", file_name); } while (0)

#define AR_EMUL_REPLACE_PRINT_VERBOSE(verbose, file_name) \
  do { if (verbose) printf ("r - %s\n", file_name); } while (0)

typedef struct bin_emulation_xfer_struct
{
  /* Print out the extra options.  */
  void    (* ar_usage)     PARAMS ((FILE *fp));
  boolean (* ar_append)    PARAMS ((bfd **, char *, boolean));
  boolean (* ar_replace)   PARAMS ((bfd **, char *, boolean));
  boolean (* ar_create)    PARAMS ((bfd **, char *, char *));
  boolean (* ar_parse_arg) PARAMS ((char *));
}
bin_emulation_xfer_type;

#endif
