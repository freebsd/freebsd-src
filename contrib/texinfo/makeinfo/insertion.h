/* insertion.h -- declarations for insertion.c.
   $Id: insertion.h,v 1.2 2002/09/29 19:15:20 karl Exp $

   Copyright (C) 1998, 1999, 2001, 2002 Free Software Foundation, Inc.

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
  defopt, defspec, deftp, deftypefn, deftypefun, deftypeivar,
  deftypemethod, deftypeop, deftypevar, deftypevr, defun, defvar, defvr,
  detailmenu, direntry, display, documentdescription, enumerate,
  example, flushleft, flushright, format, ftable, group, ifclear,
  ifhtml, ifinfo, ifnothtml, ifnotinfo, ifnotplaintext, ifnottex, ifnotxml, 
  ifplaintext, ifset, iftex, ifxml, itemize, lisp, menu, multitable, quotation,
  rawhtml, rawtex, smalldisplay, smallexample, smallformat, smalllisp,
  verbatim, table, tex, vtable, bad_type
};

typedef struct istack_elt
{
  struct istack_elt *next;
  char *item_function;
  char *filename;
  int line_number;
  int filling_enabled;
  int indented_fill;
  enum insertion_type insertion;
  int inhibited;
  int in_fixed_width_font;
} INSERTION_ELT;


extern int insertion_level;
extern INSERTION_ELT *insertion_stack;
extern int in_menu;
extern int in_detailmenu;
extern int had_menu_commentary;
extern int in_paragraph;

extern void command_name_condition ();
extern void cm_ifhtml (), cm_ifnothtml(), cm_html ();
extern void cm_ifinfo (), cm_ifnotinfo ();
extern void cm_ifplaintext (), cm_ifnotplaintext();
extern void cm_iftex (), cm_ifnottex (), cm_tex ();
extern void cm_ifxml (), cm_ifnotxml ();
#endif /* !INSERTION_H */
