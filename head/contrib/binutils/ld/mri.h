/* mri.h -- header file for MRI scripting functions
   Copyright 1993, 1995, 1996, 2003 Free Software Foundation, Inc.

This file is part of GLD, the Gnu Linker.

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

#ifndef MRI_H
#define MRI_H

extern unsigned int symbol_truncate;

extern void mri_output_section (const char *, etree_type *);
extern void mri_only_load (const char *);
extern void mri_base (etree_type *);
extern void mri_load (const char *);
extern void mri_order (const char *);
extern void mri_alias (const char *, const char *, int);
extern void mri_name (const char *);
extern void mri_format (const char *);
extern void mri_public (const char *, etree_type *);
extern void mri_align (const char *, etree_type *);
extern void mri_alignmod (const char *, etree_type *);
extern void mri_truncate (unsigned int);
extern void mri_draw_tree (void);

#endif
