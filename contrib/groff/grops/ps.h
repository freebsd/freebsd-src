// -*- C++ -*-
/* Copyright (C) 1989, 1990, 1991, 1992 Free Software Foundation, Inc.
     Written by James Clark (jjc@jclark.com)

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

class ps_output {
public:
  ps_output(FILE *, int max_line_length);
  ps_output &put_string(const char *, int);
  ps_output &put_number(int);
  ps_output &put_fix_number(int);
  ps_output &put_float(double);
  ps_output &put_symbol(const char *);
  ps_output &put_literal_symbol(const char *);
  ps_output &set_fixed_point(int);
  ps_output &simple_comment(const char *);
  ps_output &begin_comment(const char *);
  ps_output &comment_arg(const char *);
  ps_output &end_comment();
  ps_output &set_file(FILE *);
  ps_output &include_file(FILE *);
  ps_output &copy_file(FILE *);
  ps_output &end_line();
  ps_output &put_delimiter(char);
  ps_output &special(const char *);
  FILE *get_file();
private:
  FILE *fp;
  int col;
  int max_line_length;		// not including newline
  int need_space;
  int fixed_point;
};

inline FILE *ps_output::get_file()
{
  return fp;
}

enum resource_type {
  RESOURCE_FONT,
  RESOURCE_PROCSET,
  RESOURCE_FILE,
  RESOURCE_ENCODING,
  RESOURCE_FORM,
  RESOURCE_PATTERN
  };

struct resource;

extern string an_empty_string;

class resource_manager {
public:
  resource_manager();
  ~resource_manager();
  void import_file(const char *filename, ps_output &);
  void need_font(const char *name);
  void print_header_comments(ps_output &);
  void document_setup(ps_output &);
  void output_prolog(ps_output &);
private:
  unsigned extensions;
  unsigned language_level;
  resource *procset_resource;
  resource *resource_list;
  resource *lookup_resource(resource_type type, string &name,
			    string &version = an_empty_string,
			    unsigned revision = 0);
  resource *lookup_font(const char *name);
  void read_download_file();
  void supply_resource(resource *r, int rank, FILE *outfp,
		       int is_document = 0);
  void process_file(int rank, FILE *fp, const char *filename, FILE *outfp);
  resource *read_file_arg(const char **);
  resource *read_procset_arg(const char **);
  resource *read_font_arg(const char **);
  resource *read_resource_arg(const char **);
  void print_resources_comment(unsigned flag, FILE *outfp);
  void print_extensions_comment(FILE *outfp);
  void print_language_level_comment(FILE *outfp);
  int do_begin_resource(const char *ptr, int rank, FILE *fp, FILE *outfp);
  int do_include_resource(const char *ptr, int rank, FILE *fp, FILE *outfp);
  int do_begin_document(const char *ptr, int rank, FILE *fp, FILE *outfp);
  int do_include_document(const char *ptr, int rank, FILE *fp, FILE *outfp);
  int do_begin_procset(const char *ptr, int rank, FILE *fp, FILE *outfp);
  int do_include_procset(const char *ptr, int rank, FILE *fp, FILE *outfp);
  int do_begin_font(const char *ptr, int rank, FILE *fp, FILE *outfp);
  int do_include_font(const char *ptr, int rank, FILE *fp, FILE *outfp);
  int do_begin_file(const char *ptr, int rank, FILE *fp, FILE *outfp);
  int do_include_file(const char *ptr, int rank, FILE *fp, FILE *outfp);
  int change_to_end_resource(const char *ptr, int rank, FILE *fp, FILE *outfp);
  int do_begin_preview(const char *ptr, int rank, FILE *fp, FILE *outfp);
  int do_begin_data(const char *ptr, int rank, FILE *fp, FILE *outfp);
  int do_begin_binary(const char *ptr, int rank, FILE *fp, FILE *outfp);
};

extern unsigned broken_flags;

// broken_flags is ored from these

enum {
  NO_SETUP_SECTION = 01,
  STRIP_PERCENT_BANG = 02,
  STRIP_STRUCTURE_COMMENTS = 04,
  USE_PS_ADOBE_2_0 = 010
};
