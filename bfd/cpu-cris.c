/* BFD support for the Axis CRIS architecture.
   Copyright 2000, 2002, 2004, 2005 Free Software Foundation, Inc.
   Contributed by Axis Communications AB.
   Written by Hans-Peter Nilsson.

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

/* This routine is provided two arch_infos and returns the lowest common
   denominator.  CRIS v0..v10 vs. v32 are not compatible in general, but
   there's a compatible subset for which we provide an arch_info.  */

static const bfd_arch_info_type * get_compatible
  PARAMS ((const bfd_arch_info_type *, const bfd_arch_info_type *));

static const bfd_arch_info_type *
get_compatible (a,b)
     const bfd_arch_info_type *a;
     const bfd_arch_info_type *b;
{
  /* Arches must match.  */
  if (a->arch != b->arch)
   return NULL;

  /* If either is the compatible mach, return the other.  */
  if (a->mach == bfd_mach_cris_v10_v32)
    return b;
  if (b->mach == bfd_mach_cris_v10_v32)
    return a;

#if 0
  /* The code below is disabled but kept as a warning.
     See ldlang.c:lang_check.  Quite illogically, incompatible arches
     (as signalled by this function) are only *warned* about, while with
     this function signalling compatible ones, we can have the
     cris_elf_merge_private_bfd_data function return an error.  This is
     undoubtedly a FIXME: in general.  Also, the
     command_line.warn_mismatch flag and the --no-warn-mismatch option
     are misnamed for the multitude of ports that signal compatibility:
     it is there an error, not a warning.  We work around it by
     pretending matching machs here.  */

  /* Except for the compatible mach, machs must match.  */
  if (a->mach != b->mach)
    return NULL;
#endif

  return a;
}

#define N(NUMBER, PRINT, NEXT)  \
 { 32, 32, 8, bfd_arch_cris, NUMBER, "cris", PRINT, 1, FALSE, \
   get_compatible, bfd_default_scan, NEXT }

static const bfd_arch_info_type bfd_cris_arch_compat_v10_v32 =
 N (bfd_mach_cris_v10_v32, "cris:common_v10_v32", NULL);

static const bfd_arch_info_type bfd_cris_arch_v32 =
 N (bfd_mach_cris_v32, "crisv32", &bfd_cris_arch_compat_v10_v32);

const bfd_arch_info_type bfd_cris_arch =
{
  32,				/* There's 32 bits_per_word.  */
  32,				/* There's 32 bits_per_address.  */
  8,				/* There's 8 bits_per_byte.  */
  bfd_arch_cris,		/* One of enum bfd_architecture, defined
				   in archures.c and provided in
				   generated header files.  */
  bfd_mach_cris_v0_v10,		/* Random BFD-internal number for this
				   machine, similarly listed in
				   archures.c.  Not emitted in output.  */
  "cris",			/* The arch_name.  */
  "cris",			/* The printable name is the same.  */
  1,				/* Section alignment power; each section
				   is aligned to (only) 2^1 bytes.  */
  TRUE,				/* This is the default "machine".  */
  get_compatible,		/* A function for testing
				   "machine" compatibility of two
				   bfd_arch_info_type.  */
  bfd_default_scan,		/* Check if a bfd_arch_info_type is a
				   match.  */
  &bfd_cris_arch_v32		/* Pointer to next bfd_arch_info_type in
				   the same family.  */
};

/*
 * Local variables:
 * eval: (c-set-style "gnu")
 * indent-tabs-mode: t
 * End:
 */
