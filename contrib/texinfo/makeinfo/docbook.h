/* docbook.h -- docbook declarations.
   $Id: docbook.h,v 1.2 2001/12/31 16:51:32 karl Exp $

   Copyright (C) 2001 Free Software Foundation, Inc.

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

#ifndef DOCBOOK_H
#define DOCBOOK_H

#define DB_B "emphasis role=\"bold\""
#define DB_CITE "citetitle"
#define DB_CODE "literal"
#define DB_COMMAND "command"
#define DB_DFN  "firstterm"
#define DB_EMPH "emphasis"
#define DB_ENV "envar"
#define DB_FILE "filename"
#define DB_FUNCTION "function"
#define DB_I "emphasis"
#define DB_KBD  "userinput"
#define DB_KEY  "keycap"
#define DB_OPTION "option"
#define DB_STRONG "emphasis role=\"bold\""
#define DB_TT "literal"
#define DB_URL "systemitem role=\"sitename\""
#define DB_VAR "replaceable"

extern int docbook_version_inserted;
extern int docbook_begin_book_p;
extern int docbook_first_chapter_found;
extern int docbook_must_insert_node_anchor;
extern int docbook_no_new_paragraph;

void docbook_begin_section ();
void docbook_begin_paragraph ();
void docbook_begin_book ();
void docbook_end_book ();

void docbook_insert_tag ();

void docbook_xref1 ();
void docbook_xref2 ();

int docbook_quote ();

int docbook_is_punctuation ();
void docbook_punctuation ();

void docbook_begin_itemize ();
void docbook_end_itemize ();
void docbook_begin_enumerate ();
void docbook_end_enumerate ();

void docbook_begin_table ();
void docbook_end_table ();
void docbook_add_item ();
void docbook_add_table_item ();
void docbook_close_table_item ();
void docbook_add_anchor ();

void docbook_footnote ();

void docbook_begin_index ();

void docbook_begin_example ();
void docbook_end_example ();

#endif /* DOCBOOK_H */
