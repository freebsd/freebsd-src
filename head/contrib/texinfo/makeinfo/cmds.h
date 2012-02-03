/* cmds.h -- declarations for cmds.c.
   $Id: cmds.h,v 1.9 2004/11/26 00:48:35 karl Exp $

   Copyright (C) 1998, 1999, 2002, 2003, 2004 Free Software Foundation,
   Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef CMDS_H
#define CMDS_H

/* The three arguments a command can get are a flag saying whether it is
   before argument parsing (START) or after (END), the starting position
   of the arguments, and the ending position.  */
typedef void COMMAND_FUNCTION (); /* So we can say COMMAND_FUNCTION *foo; */

/* Each command has an associated function.  When the command is
   encountered in the text, the associated function is called with START
   as the argument.  If the function expects arguments in braces, it
   remembers itself on the stack.  When the corresponding close brace is
   encountered, the function is called with END as the argument. */
#define START 0
#define END 1

/* Does the command expect braces?  */
#define NO_BRACE_ARGS 0
#define BRACE_ARGS 1
#define MAYBE_BRACE_ARGS 2

typedef struct
{
  char *name;
  COMMAND_FUNCTION *proc;
  int argument_in_braces;
} COMMAND;

extern COMMAND command_table[];

typedef struct acronym_desc
{
  struct acronym_desc *next;
  char *acronym;
  char *description;
} ACRONYM_DESC;

/* Texinfo commands.  */
extern void insert_self (int arg),
  insert_space (int arg),
  cm_ignore_line (void),
  cm_ignore_arg (int arg, int start_pos, int end_pos),
  cm_comment (void),
  cm_no_op (void);

/* Document structure and meta information.  */
extern void cm_setfilename (void),
  cm_settitle (void),
  cm_documentdescription (void),
  cm_node (void),
  cm_menu (void),
  cm_detailmenu (void),
  cm_dircategory (void),
  cm_direntry (void),
  cm_bye (void);

/* File inclusion.  */
extern void cm_include (void),
  cm_verbatiminclude (void);

/* Cross referencing commands.  */
extern void cm_anchor (int arg),
  cm_xref (int arg),
  cm_pxref (int arg),
  cm_ref (int arg),
  cm_inforef (int arg),
  cm_uref (int arg);

/* Special insertions.  */
extern void cm_LaTeX (int arg),
  cm_TeX (int arg),
  cm_bullet (int arg),
  cm_colon (void),
  cm_comma (int arg),
  cm_copyright (int arg),
  cm_dots (int arg),
  cm_enddots (int arg),
  cm_equiv (int arg),
  cm_error (int arg),
  cm_expansion (int arg),
  cm_image (int arg),
  cm_insert_copying (void),
  cm_minus (int arg),
  cm_point (int arg),
  cm_print (int arg),
  cm_punct (int arg),
  cm_registeredsymbol (int arg),
  cm_result (int arg);

/* Emphasis and markup.  */
extern void cm_acronym (int arg),
  cm_abbr (int arg),
  cm_b (int arg),
  cm_cite (int arg, int position),
  cm_code (int arg),
  cm_dfn (int arg, int position),
  cm_dmn (int arg),
  cm_email (int arg),
  cm_emph (int arg),
  cm_i (int arg),
  cm_kbd (int arg),
  cm_key (int arg),
  cm_math (int arg),
  cm_not_fixed_width (int arg, int start, int end),
  cm_r (int arg),
  cm_sansserif (int arg),
  cm_sc (int arg, int start_pos, int end_pos),
  cm_slanted (int arg),
  cm_strong (int arg, int start_pos, int end_pos),
  cm_tt (int arg),
  cm_indicate_url (int arg, int start, int end),
  cm_var (int arg, int start_pos, int end_pos),
  cm_verb (int arg);

/* Block environments.  */
extern void cm_cartouche (void),
  cm_group (void),
  cm_display (void),
  cm_smalldisplay (void),
  cm_example (void),
  cm_smallexample (void),
  cm_smalllisp (void),
  cm_lisp (void),
  cm_format (void),
  cm_smallformat (void),
  cm_quotation (void),
  cm_copying (void),
  cm_flushleft (void),
  cm_flushright (void),
  cm_verbatim (void),
  cm_end (void);

/* Tables, lists, enumerations.  */
extern void cm_table (void),
  cm_ftable (void),
  cm_vtable (void),
  cm_itemize (void),
  cm_enumerate (void),
  cm_multitable (void),
  cm_headitem (void),
  cm_item (void),
  cm_itemx (void),
  cm_tab (void);

extern void cm_center (void),
  cm_exdent (void),
  cm_indent (void),
  cm_noindent (void),
  cm_noindent_cmd (void);

/* Line and page breaks.  */
extern void cm_asterisk (void),
  cm_sp (void),
  cm_page (void);

/* Non breaking words.  */
extern void cm_tie (int arg),
  cm_w (int arg);

/* Title page creation.  */
extern void cm_titlepage (void),
  cm_author (void),
  cm_titlepage_cmds (void),
  cm_titlefont (int arg),
  cm_today (int arg);

/* Floats.  */
extern void cm_float (void),
  cm_caption (int arg),
  cm_shortcaption (void),
  cm_listoffloats (void);

/* Indices.  */
extern void cm_kindex (void),
  cm_cindex (void),
  cm_findex (void),
  cm_pindex (void),
  cm_vindex (void),
  cm_tindex (void),
  cm_defindex (void),
  cm_defcodeindex (void),
  cm_synindex (void),
  cm_printindex (void);

/* Conditionals. */
extern void cm_set (void),
  cm_clear (void),
  cm_ifset (void),
  cm_ifclear (void),
  cm_ifeq (void),
  cm_value (int arg, int start_pos, int end_pos);

#endif /* !CMDS_H */
