/* toc.h -- table of contents handling.
   $Id: toc.h,v 1.4 1999/04/25 19:49:22 karl Exp $

   Copyright (C) 1999 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

   Written by Karl Heinz Marbaise <kama@hippo.fido.de>.  */

#ifndef TOC_H
#define TOC_H

/* the file where we found the @contents directive */
extern char *contents_filename;

/* the file where we found the @shortcontents directive */
extern char *shortcontents_filename;

/* Structure to hold one entry for the toc. */
typedef struct toc_entry_elt {
  char *name;
  char *containing_node; /* Name of node containing this section.  */
  int number;            /* counting number from 0...n independent from
                            chapter/section can be used for anchors or
                            references to it.  */
  int level;             /* level: chapter, section, subsection... */
} TOC_ENTRY_ELT;

/* all routines which have relationship with TOC should start with
   toc_ (this is a kind of name-space) */
extern int toc_add_entry (); /* return the number for the toc-entry */
extern void toc_free ();
extern char *toc_find_section_of_node ();

extern void cm_contents (), cm_shortcontents ();

#endif /* not TOC_H */
