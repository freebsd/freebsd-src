/* Table of stab names for the BFD library.
   Copyright (C) 1990-1991 Free Software Foundation, Inc.
   Written by Cygnus Support.

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
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include "bfd.h"

#define ARCH_SIZE 32 /* Value doesn't matter. */
#include "libaout.h"
#include "aout/aout64.h"

/* Create a table of debugging stab-codes and corresponding names.  */

#define __define_name(CODE, STRING) {(int)CODE, STRING},
#define __define_stab(NAME, CODE, STRING) __define_name(CODE, STRING)
CONST struct {short code; char string[10];} aout_stab_names[]
  = {
#include "aout/stab.def"

/* These are not really stab symbols, but it is
   convenient to have them here for the sake of nm.
   For completeness, we could also add N_TEXT etc, but those
   are never needed, since nm treats those specially. */
__define_name (N_SETA, "SETA")	/* Absolute set element symbol */
__define_name (N_SETT, "SETT")	/* Text set element symbol */
__define_name (N_SETD, "SETD")	/* Data set element symbol */
__define_name (N_SETB, "SETB")	/* Bss set element symbol */
__define_name (N_SETV, "SETV")  /* Pointer to set vector in data area. */
__define_name (N_INDR, "INDR")
__define_name (N_WARNING, "WARNING")
    };
#undef __define_stab
#undef GNU_EXTRA_STABS

CONST char *
DEFUN(aout_stab_name,(code),
int code)
{
  register int i = sizeof(aout_stab_names) / sizeof(aout_stab_names[0]);
  while (--i >= 0)
    if (aout_stab_names[i].code == code)
      return aout_stab_names[i].string;
  return 0;
}
