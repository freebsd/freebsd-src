// -*- C++ -*-
/* Copyright (C) 2000, 2001 Free Software Foundation, Inc.
 *
 *  Gaius Mulley (gaius@glam.ac.uk) wrote html-text.cc
 *
 *  html-text.h
 *
 *  provides a state machine interface which generates html text.
 */

/*
This file is part of groff.

groff is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

groff is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License along
with groff; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */

#include "html.h"

/*
 *  html tags
 */

typedef enum {I_TAG, B_TAG, P_TAG, SUB_TAG, SUP_TAG, TT_TAG,
	      PRE_TAG, SMALL_TAG, BIG_TAG, BREAK_TAG, TABLE_TAG} HTML_TAG;

typedef struct tag_definition {
  HTML_TAG        type;
  char           *arg1;
  int             text_emitted;
  tag_definition *next;
} tag_definition ;

/*
 *  the state of the current paragraph.
 *  It allows post-html.cc to request font changes, paragraph start/end
 *  and emits balanced tags with a small amount of peephole optimization.
 */

class html_text {
public:
         html_text      (simple_output *op);
        ~html_text      (void);
  void   flush_text     (void);
  void   do_emittext    (char *s, int length);
  void   do_italic      (void);
  void   do_bold        (void);
  void   do_roman       (void);
  void   do_tt          (void);
  void   do_pre         (void);
  void   do_small       (void);
  void   do_big         (void);
  void   do_para        (char *arg1);
  void   do_sup         (void);
  void   do_sub         (void);
  void   do_space       (void);
  void   do_break       (void);
  void   do_newline     (void);
  void   do_table       (char *arg);
  void   done_bold      (void);
  void   done_italic    (void);
  char  *done_para      (void);
  void   done_sup       (void);
  void   done_sub       (void);
  void   done_tt        (void);
  void   done_pre       (void);
  void   done_small     (void);
  void   done_big       (void);
  void   do_indent      (char *arg, int indent, int pageoff, int linelen);
  int    emitted_text   (void);
  int    emit_space     (void);
  int    is_in_pre      (void);
  void   remove_tag     (HTML_TAG tag);
  void   remove_sub_sup (void);
  void   done_table     (void);
  int    is_in_table    (void);

private:
  tag_definition   *stackptr;    /* the current paragraph state */
  tag_definition   *lastptr;     /* the end of the stack        */
  simple_output    *out;
  int               space_emitted;
  int               current_indentation;  /* current .in value */
  int               pageoffset;           /* .po value         */
  int               linelength;         /* current line length */

  int    is_present        (HTML_TAG t);
  void   end_tag           (tag_definition *t);
  void   start_tag         (tag_definition *t);
  void   push_para         (HTML_TAG t, char *arg);
  char  *shutdown          (HTML_TAG t);
  void   check_emit_text   (tag_definition *t);
  int    remove_break      (void);
  void   issue_tag         (char *tagname, char *arg);
  void   issue_table_begin (tag_definition *t);
  void   issue_table_end   (void);
  int    table_is_void     (tag_definition *t);
  void   remove_def        (tag_definition *t);
};
