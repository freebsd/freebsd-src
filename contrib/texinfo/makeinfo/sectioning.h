/* sectioning.h -- all related stuff @chapter, @section... @contents
   $Id: sectioning.h,v 1.5 2004/04/11 17:56:47 karl Exp $

   Copyright (C) 1999, 2003 Free Software Foundation, Inc.

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

/* Level 4.  */
extern void cm_chapter (void),
  cm_unnumbered (void),
  cm_appendix (void),
  cm_top (void);

/* Level 3.  */
extern void cm_section (void),
  cm_unnumberedsec (void),
  cm_appendixsec (void);

/* Level 2.  */
extern void cm_subsection (void),
  cm_unnumberedsubsec (void),
  cm_appendixsubsec (void);

/* Level 1.  */
extern void cm_subsubsection (void),
  cm_unnumberedsubsubsec (void),
  cm_appendixsubsubsec (void);

/* Headings.  */
extern void cm_heading (void),
  cm_chapheading (void),
  cm_subheading (void),
  cm_subsubheading (void),
  cm_majorheading (void);

extern void cm_raisesections (void),
  cm_lowersections (void),
  cm_ideprecated (int arg, int start, int end);

extern void
  sectioning_underscore (char *cmd),
  insert_and_underscore (int level, char *cmd);

/* needed in node.c */
extern int set_top_section_level (int level);

extern void sectioning_html (int level, char *cmd);
extern int what_section (char *text, char **secname);
extern char *current_chapter_number (void),
  *current_sectioning_number (void),
  *current_sectioning_name (void);

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
