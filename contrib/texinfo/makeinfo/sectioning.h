/* sectioning.h -- all related stuff @chapter, @section... @contents
   $Id: sectioning.h,v 1.2 1999/03/09 22:48:15 karl Exp $

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

#ifndef SECTIONING_H
#define SECTIONING_H

/* Sectioning.  */
extern void
  cm_chapter (), cm_unnumbered (), cm_appendix (), cm_top (),
  cm_section (), cm_unnumberedsec (), cm_appendixsec (),
  cm_subsection (), cm_unnumberedsubsec (), cm_appendixsubsec (),
  cm_subsubsection (), cm_unnumberedsubsubsec (), cm_appendixsubsubsec (),
  cm_heading (), cm_chapheading (), cm_subheading (), cm_subsubheading (),
  cm_majorheading (), cm_raisesections (), cm_lowersections (),

  cm_ideprecated ();

extern void
  sectioning_underscore (), insert_and_underscore ();

extern int what_section ();



/* is needed in node.c */
extern int set_top_section_level ();

extern void sectioning_html ();
extern int what_section ();

/* The argument of @settitle, used for HTML. */
extern char *title;


/* Here is a structure which associates sectioning commands with
   an integer that reflects the depth of the current section. */
typedef struct
{
  char *name;
  int level; /* I can't replace the levels with defines
                because it is changed during run */
  int num; /* ENUM_SECT_NO means no enumeration...
              ENUM_SECT_YES means enumerated version
              ENUM_SECT_APP appendix (Character enumerated
                            at first position */
  int toc; /* TOC_NO means do not enter in toc;
              TOC_YES means enter it in toc */
} section_alist_type;

extern section_alist_type section_alist[];

/* enumerate sections */
#define ENUM_SECT_NO  0
#define ENUM_SECT_YES 1
#define ENUM_SECT_APP 2

/* make entries into toc no/yes */
#define TOC_NO  0
#define TOC_YES 1


#endif /* not SECTIONING_H */
