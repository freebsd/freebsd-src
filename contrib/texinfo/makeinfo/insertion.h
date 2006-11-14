/* insertion.h -- declarations for insertion.c.
   $Id: insertion.h,v 1.10 2004/04/11 17:56:47 karl Exp $

   Copyright (C) 1998, 1999, 2001, 2002, 2003 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef INSERTION_H
#define INSERTION_H

/* Must match list in insertion.c.  */
enum insertion_type
{ 
  cartouche, copying, defcv, deffn, defivar, defmac, defmethod, defop,
  defopt, defspec, deftp, deftypecv, deftypefn, deftypefun, deftypeivar,
  deftypemethod, deftypeop, deftypevar, deftypevr, defun, defvar, defvr,
  detailmenu, direntry, display, documentdescription, enumerate,
  example, floatenv, flushleft, flushright, format, ftable, group,
  ifclear, ifdocbook, ifhtml, ifinfo, ifnotdocbook, ifnothtml, ifnotinfo,
  ifnotplaintext, ifnottex, ifnotxml, ifplaintext, ifset, iftex, ifxml,
  itemize, lisp, menu, multitable, quotation, rawdocbook, rawhtml, rawtex,
  rawxml, smalldisplay, smallexample, smallformat, smalllisp, verbatim,
  table, tex, vtable, titlepage, bad_type
};

typedef struct istack_elt
{
  struct istack_elt *next;
  char *item_function;
  char *filename;
  int line_number;
  int filling_enabled;
  int indented_fill;
  int insertion;
  int inhibited;
  int in_fixed_width_font;
} INSERTION_ELT;

extern int insertion_level;
extern INSERTION_ELT *insertion_stack;
extern int in_menu;
extern int in_detailmenu;
extern int had_menu_commentary;
extern int in_paragraph;

extern int headitem_flag;
extern int after_headitem;

extern void init_insertion_stack (void);
extern void command_name_condition (void);
extern void cm_ifdocbook (void), cm_ifnotdocbook(void), cm_docbook (int arg);
extern void cm_ifhtml (void), cm_ifnothtml(void), cm_html (int arg);
extern void cm_ifinfo (void), cm_ifnotinfo (void);
extern void cm_ifplaintext (void), cm_ifnotplaintext(void);
extern void cm_iftex (void), cm_ifnottex (void), cm_tex (void);
extern void cm_ifxml (void), cm_ifnotxml (void), cm_xml (int arg);
extern void handle_verbatim_environment (int find_end_verbatim);
extern void begin_insertion (enum insertion_type type);
extern void pop_insertion (void);
extern void discard_insertions (int specials_ok);

extern int is_in_insertion_of_type (int type);
extern int command_needs_braces (char *cmd);

extern enum insertion_type find_type_from_name (char *name);
#endif /* !INSERTION_H */
