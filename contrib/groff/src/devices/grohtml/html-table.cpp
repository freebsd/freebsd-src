// -*- C++ -*-
/* Copyright (C) 2002, 2003, 2004, 2005 Free Software Foundation, Inc.
 *
 *  Gaius Mulley (gaius@glam.ac.uk) wrote html-table.cpp
 *
 *  html-table.h
 *
 *  provides the methods necessary to handle indentation and tab
 *  positions using html tables.
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
Foundation, 51 Franklin St - Fifth Floor, Boston, MA 02110-1301, USA. */

#include "driver.h"
#include "stringclass.h"
#include "cset.h"
#include "html-table.h"
#include "ctype.h"
#include "html.h"
#include "html-text.h"

#if !defined(TRUE)
#   define TRUE  (1==1)
#endif
#if !defined(FALSE)
#   define FALSE (1==0)
#endif

tabs::tabs ()
  : tab(NULL)
{
}

tabs::~tabs ()
{
  delete_list();
}

/*
 *  delete_list - frees the tab list and sets tab to NULL.
 */

void tabs::delete_list (void)
{
  tab_position *p = tab;
  tab_position *q;

  while (p != NULL) {
    q = p;
    p = p->next;
    delete q;
  }
  tab = NULL;
}

void tabs::clear (void)
{
  delete_list();
}

/*
 *  compatible - returns TRUE if the tab stops in, s, do
 *               not conflict with the current tab stops.
 *               The new tab stops are _not_ placed into
 *               this class.
 */

int tabs::compatible (const char *s)
{
  char align;
  int  total=0;
  tab_position *last = tab;

  if (last == NULL)
    return FALSE;  // no tab stops defined

  // move over tag name
  while ((*s != (char)0) && !isspace(*s))
    s++;

  while (*s != (char)0 && last != NULL) {
    // move over white space
    while ((*s != (char)0) && isspace(*s))
      s++;
    // collect alignment
    align = *s;
    // move over alignment
    s++;
    // move over white space
    while ((*s != (char)0) && isspace(*s))
      s++;
    // collect tab position
    total = atoi(s);
    // move over tab position
    while ((*s != (char)0) && !isspace(*s))
      s++;
    if (last->alignment != align || last->position != total)
      return FALSE;

    last = last->next;
  }
  return TRUE;
}

/*
 *  init - scans the string, s, and initializes the tab stops.
 */

void tabs::init (const char *s)
{
  char align;
  int  total=0;
  tab_position *last = NULL;

  clear(); // remove any tab stops

  // move over tag name
  while ((*s != (char)0) && !isspace(*s))
    s++;

  while (*s != (char)0) {
    // move over white space
    while ((*s != (char)0) && isspace(*s))
      s++;
    // collect alignment
    align = *s;
    // move over alignment
    s++;
    // move over white space
    while ((*s != (char)0) && isspace(*s))
      s++;
    // collect tab position
    total = atoi(s);
    // move over tab position
    while ((*s != (char)0) && !isspace(*s))
      s++;
    if (last == NULL) {
      tab = new tab_position;
      last = tab;
    } else {
      last->next = new tab_position;
      last = last->next;
    }
    last->alignment = align;
    last->position = total;
    last->next = NULL;    
  }
}

/*
 *  check_init - define tab stops using, s, providing none already exist.
 */

void tabs::check_init (const char *s)
{
  if (tab == NULL)
    init(s);
}

/*
 *  find_tab - returns the tab number corresponding to the position, pos.
 */

int tabs::find_tab (int pos)
{
  tab_position *p;
  int i=0;

  for (p = tab; p != NULL; p = p->next) {
    i++;
    if (p->position == pos)
      return i;
  }
  return 0;
}

/*
 *  get_tab_pos - returns the, nth, tab position
 */

int tabs::get_tab_pos (int n)
{
  tab_position *p;

  n--;
  for (p = tab; (p != NULL) && (n>0); p = p->next) {
    n--;
    if (n == 0)
      return p->position;
  }
  return 0;
}

char tabs::get_tab_align (int n)
{
  tab_position *p;

  n--;
  for (p = tab; (p != NULL) && (n>0); p = p->next) {
    n--;
    if (n == 0)
      return p->alignment;
  }
  return 'L';
}

/*
 *  dump_tab - display tab positions
 */

void tabs::dump_tabs (void)
{
  int i=1;
  tab_position *p;

  for (p = tab; p != NULL; p = p->next) {
    printf("tab %d is %d\n", i, p->position);
    i++;
  }
}

/*
 *  html_table - methods
 */

html_table::html_table (simple_output *op, int linelen)
  : out(op), columns(NULL), linelength(linelen), last_col(NULL), start_space(FALSE)
{
  tab_stops = new tabs();
}

html_table::~html_table ()
{
  cols *c;
  if (tab_stops != NULL)
    delete tab_stops;
  
  c = columns;
  while (columns != NULL) {
    columns = columns->next;
    delete c;
    c = columns;
  }
}

/*
 *  remove_cols - remove a list of columns as defined by, c.
 */

void html_table::remove_cols (cols *c)
{
  cols *p;

  while (c != NULL) {
    p = c;
    c = c->next;
    delete p;
  }
}

/*
 *  set_linelength - sets the line length value in this table.
 *                   It also adds an extra blank column to the
 *                   table should linelen exceed the last column.
 */

void html_table::set_linelength (int linelen)
{
  cols *p = NULL;
  cols *c;
  linelength = linelen;

  for (c = columns; c != NULL; c = c->next) {
    if (c->right > linelength) {
      c->right = linelength;
      remove_cols(c->next);
      c->next = NULL;
      return;
    }
    p = c;
  }
  if (p != NULL && p->right > 0)
    add_column(p->no+1, p->right, linelength, 'L');
}

/*
 *  get_effective_linelength -
 */

int html_table::get_effective_linelength (void)
{
  if (columns != NULL)
    return linelength - columns->left;
  else
    return linelength;
}

/*
 *  add_indent - adds the indent to a table.
 */

void html_table::add_indent (int indent)
{
  if (columns != NULL && columns->left > indent)
    add_column(0, indent, columns->left, 'L');
}

/*
 *  emit_table_header - emits the html header for this table.
 */

void html_table::emit_table_header (int space)
{
  if (columns == NULL)
    return;

  // dump_table();

  last_col = NULL;
  if (linelength > 0) {
    out->nl();
    out->nl();

    out->put_string("<table width=\"100%\"")
      .put_string(" border=0 rules=\"none\" frame=\"void\"\n")
      .put_string("       cellspacing=\"0\" cellpadding=\"0\"");
    out->put_string(">")
      .nl();
    out->put_string("<tr valign=\"top\" align=\"left\"");
    if (space) {
      out->put_string(" style=\"margin-top: ");
      out->put_string(STYLE_VERTICAL_SPACE);
      out->put_string("\"");
    }
    out->put_string(">").nl();
  }
}

/*
 *  get_right - returns the right most position of this column.
 */

int html_table::get_right (cols *c)
{
  if (c != NULL && c->right > 0)
    return c->right;
  if (c->next != NULL)
    return c->left;
  return linelength;
}

/*
 *  set_space - assigns start_space. Used to determine the
 *              vertical alignment when generating the next table row.
 */

void html_table::set_space (int space)
{
  start_space = space;
}

/*
 *  emit_col - moves onto column, n.
 */

void html_table::emit_col (int n)
{
  cols *c = columns;
  cols *b = columns;
  int   width = 0;

  // must be a different row
  if (last_col != NULL && n <= last_col->no)
    emit_new_row();

  while (c != NULL && c->no < n)
    c = c->next;

  // can we find column, n?
  if (c != NULL && c->no == n) {
    // shutdown previous column
    if (last_col != NULL)
      out->put_string("</td>").nl();

    // find previous column
    if (last_col == NULL)
      b = columns;
    else
      b = last_col;
    
    // have we a gap?
    if (last_col != NULL) {
      if (is_gap(b))
	out->put_string("<td width=\"")
	    .put_number(is_gap(b))
	    .put_string("%\"></td>")
	    .nl();
      b = b->next;
    }

    // move across to column n
    while (b != c) {
      // we compute the difference after converting positions
      // to avoid rounding errors
      width = (get_right(b)*100 + get_effective_linelength()/2)
		/ get_effective_linelength()
	      - (b->left*100 + get_effective_linelength()/2)
		  /get_effective_linelength();
      if (width)
	out->put_string("<td width=\"")
	    .put_number(width)
	    .put_string("%\"></td>")
	    .nl();
      // have we a gap?
      if (is_gap(b))
	out->put_string("<td width=\"")
	    .put_number(is_gap(b))
	    .put_string("%\"></td>")
	    .nl();
      b = b->next;
    }
    width = (get_right(b)*100 + get_effective_linelength()/2)
	      / get_effective_linelength()
	    - (b->left*100 + get_effective_linelength()/2)
		/get_effective_linelength();
    switch (b->alignment) {
    case 'C':
      out->put_string("<td width=\"")
	  .put_number(width)
	  .put_string("%\" align=center>")
	  .nl();
      break;
    case 'R':
      out->put_string("<td width=\"")
	  .put_number(width)
	  .put_string("%\" align=right>")
	  .nl();
      break;
    default:
      out->put_string("<td width=\"")
	  .put_number(width)
	  .put_string("%\">")
	  .nl();
    }
    // remember column, b
    last_col = b;
  }
}

/*
 *  finish_row -
 */

void html_table::finish_row (void)
{
  int n = 0;
  cols *c;

  if (last_col != NULL) {
    for (c = last_col->next; c != NULL; c = c->next)
      n = c->no;
    
    if (n > 0)
      emit_col(n);
    out->put_string("</td>").nl();
  }
}

/*
 *  emit_new_row - move to the next row.
 */

void html_table::emit_new_row (void)
{
  finish_row();

  out->put_string("<tr valign=\"top\" align=\"left\"");
  if (start_space) {
    out->put_string(" style=\"margin-top: ");
    out->put_string(STYLE_VERTICAL_SPACE);
    out->put_string("\"");
  }
  out->put_string(">").nl();
  start_space = FALSE;
  last_col = NULL;
}

void html_table::emit_finish_table (void)
{
  finish_row();
  out->put_string("</table>");
}

/*
 *  add_column - adds a column. It returns FALSE if hstart..hend
 *               crosses into a different columns.
 */

int html_table::add_column (int coln, int hstart, int hend, char align)
{
  cols *c = get_column(coln);

  if (c == NULL)
    return insert_column(coln, hstart, hend, align);
  else
    return modify_column(c, hstart, hend, align);
}

/*
 *  get_column - returns the column, coln.
 */

cols *html_table::get_column (int coln)
{
  cols *c = columns;

  while (c != NULL && coln != c->no)
    c = c->next;

  if (c != NULL && coln == c->no)
    return c;
  else
    return NULL;
}

/*
 *  insert_column - inserts a column, coln.
 *                  It returns TRUE if it does not bump into
 *                  another column.
 */

int html_table::insert_column (int coln, int hstart, int hend, char align)
{
  cols *c = columns;
  cols *l = columns;
  cols *n = NULL;

  while (c != NULL && c->no < coln) {
    l = c;
    c = c->next;
  }
  if (l != NULL && l->no>coln && hend > l->left)
    return FALSE;	// new column bumps into previous one

  l = NULL;
  c = columns;
  while (c != NULL && c->no < coln) {
    l = c;
    c = c->next;
  }

  if ((l != NULL) && (hstart < l->right))
    return FALSE;	// new column bumps into previous one
  
  if ((l != NULL) && (l->next != NULL) &&
      (l->next->left < hend))
    return FALSE;  // new column bumps into next one

  n = new cols;
  if (l == NULL) {
    n->next = columns;
    columns = n;
  } else {
    n->next = l->next;
    l->next = n;
  }
  n->left = hstart;
  n->right = hend;
  n->no = coln;
  n->alignment = align;
  return TRUE;
}

/*
 *  modify_column - given a column, c, modify the width to
 *                  contain hstart..hend.
 *                  It returns TRUE if it does not clash with
 *                  the next or previous column.
 */

int html_table::modify_column (cols *c, int hstart, int hend, char align)
{
  cols *l = columns;

  while (l != NULL && l->next != c)
    l = l->next;

  if ((l != NULL) && (hstart < l->right))
    return FALSE;	// new column bumps into previous one
  
  if ((c->next != NULL) && (c->next->left < hend))
    return FALSE;  // new column bumps into next one

  if (c->left > hstart)
    c->left = hstart;

  if (c->right < hend)
    c->right = hend;

  c->alignment = align;

  return TRUE;
}

/*
 *  find_tab_column - finds the column number for position, pos.
 *                    It searches through the list tab stops.
 */

int html_table::find_tab_column (int pos)
{
  // remember the first column is reserved for untabbed glyphs
  return tab_stops->find_tab(pos)+1;
}

/*
 *  find_column - find the column number for position, pos.
 *                It searches through the list of columns.
 */

int html_table::find_column (int pos)
{
  int   p=0;
  cols *c;

  for (c = columns; c != NULL; c = c->next) {
    if (c->left > pos)
      return p;
    p = c->no;
  }
  return p;
}

/*
 *  no_columns - returns the number of table columns (rather than tabs)
 */

int html_table::no_columns (void)
{
  int n=0;
  cols *c;

  for (c = columns; c != NULL; c = c->next)
    n++;
  return n;
}

/*
 *  is_gap - returns the gap between column, c, and the next column.
 */

int html_table::is_gap (cols *c)
{
  if (c == NULL || c->right <= 0 || c->next == NULL)
    return 0;
  else
    // we compute the difference after converting positions
    // to avoid rounding errors
    return (c->next->left*100 + get_effective_linelength()/2)
	     / get_effective_linelength()
	   - (c->right*100 + get_effective_linelength()/2)
	       / get_effective_linelength();
}

/*
 *  no_gaps - returns the number of table gaps between the columns
 */

int html_table::no_gaps (void)
{
  int n=0;
  cols *c;

  for (c = columns; c != NULL; c = c->next)
    if (is_gap(c))
      n++;
  return n;
}

/*
 *  get_tab_pos - returns the, nth, tab position
 */

int html_table::get_tab_pos (int n)
{
  return tab_stops->get_tab_pos(n);
}

char html_table::get_tab_align (int n)
{
  return tab_stops->get_tab_align(n);
}


void html_table::dump_table (void)
{
  if (columns != NULL) {
    cols *c;
    for (c = columns; c != NULL; c = c->next) {
      printf("column %d  %d..%d  %c\n", c->no, c->left, c->right, c->alignment);
    }
  } else
    tab_stops->dump_tabs();
}

/*
 *  html_indent - creates an indent with indentation, ind, given
 *                a line length of linelength.
 */

html_indent::html_indent (simple_output *op, int ind, int pageoffset, int linelength)
{
  table = new html_table(op, linelength);

  table->add_column(1, ind+pageoffset, linelength, 'L');
  table->add_indent(pageoffset);
  in = ind;
  pg = pageoffset;
  ll = linelength;
}

html_indent::~html_indent (void)
{
  end();
  delete table;
}

void html_indent::begin (int space)
{
  if (in + pg == 0) {
    if (space) {
      table->out->put_string(" style=\"margin-top: ");
      table->out->put_string(STYLE_VERTICAL_SPACE);
      table->out->put_string("\"");
    }
  }
  else {
    //
    // we use exactly the same mechanism for calculating
    // indentation as html_table::emit_col
    //
    table->out->put_string(" style=\"margin-left:")
      .put_number(((in + pg) * 100 + ll/2) / ll -
		  (ll/2)/ll)
      .put_string("%;");

    if (space) {
      table->out->put_string(" margin-top: ");
      table->out->put_string(STYLE_VERTICAL_SPACE);
    }
    table->out->put_string("\"");
  }
}

void html_indent::end (void)
{
}

/*
 *  get_reg - collects the registers as supplied during initialization.
 */

void html_indent::get_reg (int *ind, int *pageoffset, int *linelength)
{
  *ind = in;
  *pageoffset = pg;
  *linelength = ll;
}
