/* Output generating routines for GDB CLI.
   Copyright 1999, 2000 Free Software Foundation, Inc.
   Contributed by Cygnus Solutions.
   Written by Fernando Nasser for Cygnus.

   This file is part of GDB.

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
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include "defs.h"
#include "ui-out.h"
#include "cli-out.h"
#include "gdb_string.h"
#include "gdb_assert.h"

/* Convenience macro for allocting typesafe memory. */

#ifndef XMALLOC
#define XMALLOC(TYPE) (TYPE*) xmalloc (sizeof (TYPE))
#endif

struct ui_out_data
  {
    struct ui_file *stream;
    int suppress_output;
  };

/* These are the CLI output functions */

static void cli_table_begin (struct ui_out *uiout, int nbrofcols,
			     int nr_rows, const char *tblid);
static void cli_table_body (struct ui_out *uiout);
static void cli_table_end (struct ui_out *uiout);
static void cli_table_header (struct ui_out *uiout, int width,
			      enum ui_align alig, const char *col_name,
			      const char *colhdr);
static void cli_begin (struct ui_out *uiout, enum ui_out_type type,
		       int level, const char *lstid);
static void cli_end (struct ui_out *uiout, enum ui_out_type type, int level);
static void cli_field_int (struct ui_out *uiout, int fldno, int width,
			   enum ui_align alig, const char *fldname, int value);
static void cli_field_skip (struct ui_out *uiout, int fldno, int width,
			    enum ui_align alig, const char *fldname);
static void cli_field_string (struct ui_out *uiout, int fldno, int width,
			      enum ui_align alig, const char *fldname,
			      const char *string);
static void cli_field_fmt (struct ui_out *uiout, int fldno,
			   int width, enum ui_align align,
			   const char *fldname, const char *format,
			   va_list args);
static void cli_spaces (struct ui_out *uiout, int numspaces);
static void cli_text (struct ui_out *uiout, const char *string);
static void cli_message (struct ui_out *uiout, int verbosity,
			 const char *format, va_list args);
static void cli_wrap_hint (struct ui_out *uiout, char *identstring);
static void cli_flush (struct ui_out *uiout);

/* This is the CLI ui-out implementation functions vector */

/* FIXME: This can be initialized dynamically after default is set to
   handle initial output in main.c */

static struct ui_out_impl cli_ui_out_impl =
{
  cli_table_begin,
  cli_table_body,
  cli_table_end,
  cli_table_header,
  cli_begin,
  cli_end,
  cli_field_int,
  cli_field_skip,
  cli_field_string,
  cli_field_fmt,
  cli_spaces,
  cli_text,
  cli_message,
  cli_wrap_hint,
  cli_flush,
  0, /* Does not need MI hacks (i.e. needs CLI hacks).  */
};

/* Prototypes for local functions */

extern void _initialize_cli_out (void);

static void field_separator (void);

static void out_field_fmt (struct ui_out *uiout, int fldno,
			   const char *fldname,
			   const char *format,...);

/* local variables */

/* (none yet) */

/* Mark beginning of a table */

void
cli_table_begin (struct ui_out *uiout, int nbrofcols,
		 int nr_rows,
		 const char *tblid)
{
  struct ui_out_data *data = ui_out_data (uiout);
  if (nr_rows == 0)
    data->suppress_output = 1;
  else
    /* Only the table suppresses the output and, fortunatly, a table
       is not a recursive data structure. */
    gdb_assert (data->suppress_output == 0);
}

/* Mark beginning of a table body */

void
cli_table_body (struct ui_out *uiout)
{
  struct ui_out_data *data = ui_out_data (uiout);
  if (data->suppress_output)
    return;
  /* first, close the table header line */
  cli_text (uiout, "\n");
}

/* Mark end of a table */

void
cli_table_end (struct ui_out *uiout)
{
  struct ui_out_data *data = ui_out_data (uiout);
  data->suppress_output = 0;
}

/* Specify table header */

void
cli_table_header (struct ui_out *uiout, int width, enum ui_align alignment,
		  const char *col_name,
		  const char *colhdr)
{
  struct ui_out_data *data = ui_out_data (uiout);
  if (data->suppress_output)
    return;
  cli_field_string (uiout, 0, width, alignment, 0, colhdr);
}

/* Mark beginning of a list */

void
cli_begin (struct ui_out *uiout,
	   enum ui_out_type type,
	   int level,
	   const char *id)
{
  struct ui_out_data *data = ui_out_data (uiout);
  if (data->suppress_output)
    return;
}

/* Mark end of a list */

void
cli_end (struct ui_out *uiout,
	 enum ui_out_type type,
	 int level)
{
  struct ui_out_data *data = ui_out_data (uiout);
  if (data->suppress_output)
    return;
}

/* output an int field */

void
cli_field_int (struct ui_out *uiout, int fldno, int width,
	       enum ui_align alignment,
	       const char *fldname, int value)
{
  char buffer[20];		/* FIXME: how many chars long a %d can become? */

  struct ui_out_data *data = ui_out_data (uiout);
  if (data->suppress_output)
    return;
  sprintf (buffer, "%d", value);
  cli_field_string (uiout, fldno, width, alignment, fldname, buffer);
}

/* used to ommit a field */

void
cli_field_skip (struct ui_out *uiout, int fldno, int width,
		enum ui_align alignment,
		const char *fldname)
{
  struct ui_out_data *data = ui_out_data (uiout);
  if (data->suppress_output)
    return;
  cli_field_string (uiout, fldno, width, alignment, fldname, "");
}

/* other specific cli_field_* end up here so alignment and field
   separators are both handled by cli_field_string */

void
cli_field_string (struct ui_out *uiout,
		  int fldno,
		  int width,
		  enum ui_align align,
		  const char *fldname,
		  const char *string)
{
  int before = 0;
  int after = 0;

  struct ui_out_data *data = ui_out_data (uiout);
  if (data->suppress_output)
    return;

  if ((align != ui_noalign) && string)
    {
      before = width - strlen (string);
      if (before <= 0)
	before = 0;
      else
	{
	  if (align == ui_right)
	    after = 0;
	  else if (align == ui_left)
	    {
	      after = before;
	      before = 0;
	    }
	  else
	    /* ui_center */
	    {
	      after = before / 2;
	      before -= after;
	    }
	}
    }

  if (before)
    ui_out_spaces (uiout, before);
  if (string)
    out_field_fmt (uiout, fldno, fldname, "%s", string);
  if (after)
    ui_out_spaces (uiout, after);

  if (align != ui_noalign)
    field_separator ();
}

/* This is the only field function that does not align */

void
cli_field_fmt (struct ui_out *uiout, int fldno,
	       int width, enum ui_align align,
	       const char *fldname,
	       const char *format,
	       va_list args)
{
  struct ui_out_data *data = ui_out_data (uiout);
  if (data->suppress_output)
    return;

  vfprintf_filtered (data->stream, format, args);

  if (align != ui_noalign)
    field_separator ();
}

void
cli_spaces (struct ui_out *uiout, int numspaces)
{
  struct ui_out_data *data = ui_out_data (uiout);
  if (data->suppress_output)
    return;
  print_spaces_filtered (numspaces, data->stream);
}

void
cli_text (struct ui_out *uiout, const char *string)
{
  struct ui_out_data *data = ui_out_data (uiout);
  if (data->suppress_output)
    return;
  fputs_filtered (string, data->stream);
}

void
cli_message (struct ui_out *uiout, int verbosity,
	     const char *format, va_list args)
{
  struct ui_out_data *data = ui_out_data (uiout);
  if (data->suppress_output)
    return;
  if (ui_out_get_verblvl (uiout) >= verbosity)
    vfprintf_unfiltered (data->stream, format, args);
}

void
cli_wrap_hint (struct ui_out *uiout, char *identstring)
{
  struct ui_out_data *data = ui_out_data (uiout);
  if (data->suppress_output)
    return;
  wrap_here (identstring);
}

void
cli_flush (struct ui_out *uiout)
{
  struct ui_out_data *data = ui_out_data (uiout);
  gdb_flush (data->stream);
}

/* local functions */

/* Like cli_field_fmt, but takes a variable number of args
   and makes a va_list and does not insert a separator */

/* VARARGS */
static void
out_field_fmt (struct ui_out *uiout, int fldno,
	       const char *fldname,
	       const char *format,...)
{
  struct ui_out_data *data = ui_out_data (uiout);
  va_list args;

  va_start (args, format);
  vfprintf_filtered (data->stream, format, args);

  va_end (args);
}

/* access to ui_out format private members */

static void
field_separator (void)
{
  struct ui_out_data *data = ui_out_data (uiout);
  fputc_filtered (' ', data->stream);
}

/* initalize private members at startup */

struct ui_out *
cli_out_new (struct ui_file *stream)
{
  int flags = ui_source_list;

  struct ui_out_data *data = XMALLOC (struct ui_out_data);
  data->stream = stream;
  data->suppress_output = 0;
  return ui_out_new (&cli_ui_out_impl, data, flags);
}

/* standard gdb initialization hook */
void
_initialize_cli_out (void)
{
  /* nothing needs to be done */
}
