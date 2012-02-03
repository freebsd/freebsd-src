// -*- C++ -*-
/* Copyright (C) 1989, 1990, 1991, 1992, 2001, 2002, 2004
   Free Software Foundation, Inc.
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
Foundation, 51 Franklin St - Fifth Floor, Boston, MA 02110-1301, USA. */

#include "refer.h"
#include "refid.h"
#include "search.h"
#include "command.h"

cset cs_field_name = csalpha;

class input_item {
  input_item *next;
  char *filename;
  int first_lineno;
  string buffer;
  const char *ptr;
  const char *end;
public:
  input_item(string &, const char *, int = 1);
  ~input_item();
  int get_char();
  int peek_char();
  void skip_char();
  int get_location(const char **, int *);

  friend class input_stack;
};

input_item::input_item(string &s, const char *fn, int ln)
: filename(strsave(fn)), first_lineno(ln)
{
  buffer.move(s);
  ptr = buffer.contents();
  end = ptr + buffer.length();
}

input_item::~input_item()
{
  a_delete filename;
}

inline int input_item::peek_char()
{
  if (ptr >= end)
    return EOF;
  else
    return (unsigned char)*ptr;
}

inline int input_item::get_char()
{
  if (ptr >= end)
    return EOF;
  else
    return (unsigned char)*ptr++;
}

inline void input_item::skip_char()
{
  ptr++;
}

int input_item::get_location(const char **filenamep, int *linenop)
{
  *filenamep = filename;
  if (ptr == buffer.contents())
    *linenop = first_lineno;
  else {
    int ln = first_lineno;
    const char *e = ptr - 1;
    for (const char *p = buffer.contents(); p < e; p++)
      if (*p == '\n')
	ln++;
    *linenop = ln;
  }
  return 1;
}

class input_stack {
  static input_item *top;
public:
  static void init();
  static int get_char();
  static int peek_char();
  static void skip_char() { top->skip_char(); }
  static void push_file(const char *);
  static void push_string(string &, const char *, int);
  static void error(const char *format,
		    const errarg &arg1 = empty_errarg,
		    const errarg &arg2 = empty_errarg,
		    const errarg &arg3 = empty_errarg);
};

input_item *input_stack::top = 0;

void input_stack::init()
{
  while (top) {
    input_item *tem = top;
    top = top->next;
    delete tem;
  }
}

int input_stack::get_char()
{
  while (top) {
    int c = top->get_char();
    if (c >= 0)
      return c;
    input_item *tem = top;
    top = top->next;
    delete tem;
  }
  return -1;
}

int input_stack::peek_char()
{
  while (top) {
    int c = top->peek_char();
    if (c >= 0)
      return c;
    input_item *tem = top;
    top = top->next;
    delete tem;
  }
  return -1;
}

void input_stack::push_file(const char *fn)
{
  FILE *fp;
  if (strcmp(fn, "-") == 0) {
    fp = stdin;
    fn = "<standard input>";
  }
  else {
    errno = 0;
    fp = fopen(fn, "r");
    if (fp == 0) {
      error("can't open `%1': %2", fn, strerror(errno));
      return;
    }
  }
  string buf;
  int bol = 1;
  int lineno = 1;
  for (;;) {
    int c = getc(fp);
    if (bol && c == '.') {
      // replace lines beginning with .R1 or .R2 with a blank line
      c = getc(fp);
      if (c == 'R') {
	c = getc(fp);
	if (c == '1' || c == '2') {
	  int cc = c;
	  c = getc(fp);
	  if (compatible_flag || c == ' ' || c == '\n' || c == EOF) {
	    while (c != '\n' && c != EOF)
	      c = getc(fp);
	  }
	  else {
	    buf += '.';
	    buf += 'R';
	    buf += cc;
	  }
	}
	else {
	  buf += '.';
	  buf += 'R';
	}
      }
      else
	buf += '.';
    }
    if (c == EOF)
      break;
    if (invalid_input_char(c))
      error_with_file_and_line(fn, lineno,
			       "invalid input character code %1", int(c));
    else {
      buf += c;
      if (c == '\n') {
	bol = 1;
	lineno++;
      }
      else
	bol = 0;
    }
  }
  if (fp != stdin)
    fclose(fp);
  if (buf.length() > 0 && buf[buf.length() - 1] != '\n')
    buf += '\n';
  input_item *it = new input_item(buf, fn);
  it->next = top;
  top = it;
}

void input_stack::push_string(string &s, const char *filename, int lineno)
{
  input_item *it = new input_item(s, filename, lineno);
  it->next = top;
  top = it;
}

void input_stack::error(const char *format, const errarg &arg1,
			const errarg &arg2, const errarg &arg3)
{
  const char *filename;
  int lineno;
  for (input_item *it = top; it; it = it->next)
    if (it->get_location(&filename, &lineno)) {
      error_with_file_and_line(filename, lineno, format, arg1, arg2, arg3);
      return;
    }
  ::error(format, arg1, arg2, arg3);
}

void command_error(const char *format, const errarg &arg1,
		   const errarg &arg2, const errarg &arg3)
{
  input_stack::error(format, arg1, arg2, arg3);
}

// # not recognized in ""
// \<newline> is recognized in ""
// # does not conceal newline
// if missing closing quote, word extends to end of line
// no special treatment of \ other than before newline
// \<newline> not recognized after #
// ; allowed as alternative to newline
// ; not recognized in ""
// don't clear word_buffer; just append on
// return -1 for EOF, 0 for newline, 1 for word

int get_word(string &word_buffer)
{
  int c = input_stack::get_char();
  for (;;) {
    if (c == '#') {
      do {
	c = input_stack::get_char();
      } while (c != '\n' && c != EOF);
      break;
    }
    if (c == '\\' && input_stack::peek_char() == '\n')
      input_stack::skip_char();
    else if (c != ' ' && c != '\t')
      break;
    c = input_stack::get_char();
  }
  if (c == EOF)
    return -1;
  if (c == '\n' || c == ';')
    return 0;
  if (c == '"') {
    for (;;) {
      c = input_stack::peek_char();
      if (c == EOF || c == '\n')
	break;
      input_stack::skip_char();
      if (c == '"') {
	int d = input_stack::peek_char();
	if (d == '"')
	  input_stack::skip_char();
	else
	  break;
      }
      else if (c == '\\') {
	int d = input_stack::peek_char();
	if (d == '\n')
	  input_stack::skip_char();
	else
	  word_buffer += '\\';
      }
      else
	word_buffer += c;
    }
    return 1;
  }
  word_buffer += c;
  for (;;) {
    c = input_stack::peek_char();
    if (c == ' ' || c == '\t' || c == '\n' || c == '#' || c == ';')
      break;
    input_stack::skip_char();
    if (c == '\\') {
      int d = input_stack::peek_char();
      if (d == '\n')
	input_stack::skip_char();
      else
	word_buffer += '\\';
    }
    else
      word_buffer += c;
  }
  return 1;
}

union argument {
  const char *s;
  int n;
};

// This is for debugging.

static void echo_command(int argc, argument *argv)
{
  for (int i = 0; i < argc; i++)
    fprintf(stderr, "%s\n", argv[i].s);
}

static void include_command(int argc, argument *argv)
{
  assert(argc == 1);
  input_stack::push_file(argv[0].s);
}

static void capitalize_command(int argc, argument *argv)
{
  if (argc > 0)
    capitalize_fields = argv[0].s;
  else
    capitalize_fields.clear();
}

static void accumulate_command(int, argument *)
{
  accumulate = 1;
}

static void no_accumulate_command(int, argument *)
{
  accumulate = 0;
}

static void move_punctuation_command(int, argument *)
{
  move_punctuation = 1;
}

static void no_move_punctuation_command(int, argument *)
{
  move_punctuation = 0;
}

static void sort_command(int argc, argument *argv)
{
  if (argc == 0)
    sort_fields = "AD";
  else
    sort_fields = argv[0].s;
  accumulate = 1;
}

static void no_sort_command(int, argument *)
{
  sort_fields.clear();
}

static void articles_command(int argc, argument *argv)
{
  articles.clear();
  int i;
  for (i = 0; i < argc; i++) {
    articles += argv[i].s;
    articles += '\0';
  }
  int len = articles.length();
  for (i = 0; i < len; i++)
    articles[i] = cmlower(articles[i]);
}

static void database_command(int argc, argument *argv)
{
  for (int i = 0; i < argc; i++)
    database_list.add_file(argv[i].s);
}

static void default_database_command(int, argument *)
{
  search_default = 1;
}

static void no_default_database_command(int, argument *)
{
  search_default = 0;
}

static void bibliography_command(int argc, argument *argv)
{
  const char *saved_filename = current_filename;
  int saved_lineno = current_lineno;
  int saved_label_in_text = label_in_text;
  label_in_text = 0;
  if (!accumulate)
    fputs(".]<\n", stdout);
  for (int i = 0; i < argc; i++)
    do_bib(argv[i].s);
  if (accumulate)
    output_references();
  else
    fputs(".]>\n", stdout);
  current_filename = saved_filename;
  current_lineno = saved_lineno;
  label_in_text = saved_label_in_text;
}

static void annotate_command(int argc, argument *argv)
{
  if (argc > 0)
    annotation_field = argv[0].s[0];
  else
    annotation_field = 'X';
  if (argc == 2)
    annotation_macro = argv[1].s;
  else
    annotation_macro = "AP";
}

static void no_annotate_command(int, argument *)
{
  annotation_macro.clear();
  annotation_field = -1;
}

static void reverse_command(int, argument *argv)
{
  reverse_fields = argv[0].s;
}

static void no_reverse_command(int, argument *)
{
  reverse_fields.clear();
}

static void abbreviate_command(int argc, argument *argv)
{
  abbreviate_fields = argv[0].s;
  period_before_initial = argc > 1 ? argv[1].s : ". ";
  period_before_last_name = argc > 2 ? argv[2].s : ". ";
  period_before_other = argc > 3 ? argv[3].s : ". ";
  period_before_hyphen = argc > 4 ? argv[4].s : ".";
}

static void no_abbreviate_command(int, argument *)
{
  abbreviate_fields.clear();
}

string search_ignore_fields;

static void search_ignore_command(int argc, argument *argv)
{
  if (argc > 0)
    search_ignore_fields = argv[0].s;
  else
    search_ignore_fields = "XYZ";
  search_ignore_fields += '\0';
  linear_ignore_fields = search_ignore_fields.contents();
}

static void no_search_ignore_command(int, argument *)
{
  linear_ignore_fields = "";
}

static void search_truncate_command(int argc, argument *argv)
{
  if (argc > 0)
    linear_truncate_len = argv[0].n;
  else
    linear_truncate_len = 6;
}

static void no_search_truncate_command(int, argument *)
{
  linear_truncate_len = -1;
}

static void discard_command(int argc, argument *argv)
{
  if (argc == 0)
    discard_fields = "XYZ";
  else
    discard_fields = argv[0].s;
  accumulate = 1;
}

static void no_discard_command(int, argument *)
{
  discard_fields.clear();
}

static void label_command(int, argument *argv)
{
  set_label_spec(argv[0].s);
}

static void abbreviate_label_ranges_command(int argc, argument *argv)
{
  abbreviate_label_ranges = 1;
  label_range_indicator = argc > 0 ? argv[0].s : "-";
}

static void no_abbreviate_label_ranges_command(int, argument *)
{
  abbreviate_label_ranges = 0;
}

static void label_in_reference_command(int, argument *)
{
  label_in_reference = 1;
}

static void no_label_in_reference_command(int, argument *)
{
  label_in_reference = 0;
}

static void label_in_text_command(int, argument *)
{
  label_in_text = 1;
}

static void no_label_in_text_command(int, argument *)
{
  label_in_text = 0;
}

static void sort_adjacent_labels_command(int, argument *)
{
  sort_adjacent_labels = 1;
}

static void no_sort_adjacent_labels_command(int, argument *)
{
  sort_adjacent_labels = 0;
}

static void date_as_label_command(int argc, argument *argv)
{
  if (set_date_label_spec(argc > 0 ? argv[0].s : "D%a*"))
    date_as_label = 1;
}

static void no_date_as_label_command(int, argument *)
{
  date_as_label = 0;
}

static void short_label_command(int, argument *argv)
{
  if (set_short_label_spec(argv[0].s))
    short_label_flag = 1;
}

static void no_short_label_command(int, argument *)
{
  short_label_flag = 0;
}

static void compatible_command(int, argument *)
{
  compatible_flag = 1;
}

static void no_compatible_command(int, argument *)
{
  compatible_flag = 0;
}

static void join_authors_command(int argc, argument *argv)
{
  join_authors_exactly_two = argv[0].s;
  join_authors_default = argc > 1 ? argv[1].s : argv[0].s;
  join_authors_last_two = argc == 3 ? argv[2].s : argv[0].s;
}

static void bracket_label_command(int, argument *argv)
{
  pre_label = argv[0].s;
  post_label = argv[1].s;
  sep_label = argv[2].s;
}

static void separate_label_second_parts_command(int, argument *argv)
{
  separate_label_second_parts = argv[0].s;
}

static void et_al_command(int argc, argument *argv)
{
  et_al = argv[0].s;
  et_al_min_elide = argv[1].n;
  if (et_al_min_elide < 1)
    et_al_min_elide = 1;
  et_al_min_total = argc >= 3 ? argv[2].n : 0;
}

static void no_et_al_command(int, argument *)
{
  et_al.clear();
  et_al_min_elide = 0;
}

typedef void (*command_t)(int, argument *);

/* arg_types is a string describing the numbers and types of arguments.
s means a string, i means an integer, f is a list of fields, F is
a single field,
? means that the previous argument is optional, * means that the
previous argument can occur any number of times. */

struct S {
  const char *name;
  command_t func;
  const char *arg_types;
} command_table[] = {
  { "include", include_command, "s" },
  { "echo", echo_command, "s*" },
  { "capitalize", capitalize_command, "f?" },
  { "accumulate", accumulate_command, "" },
  { "no-accumulate", no_accumulate_command, "" },
  { "move-punctuation", move_punctuation_command, "" },
  { "no-move-punctuation", no_move_punctuation_command, "" },
  { "sort", sort_command, "s?" },
  { "no-sort", no_sort_command, "" },
  { "articles", articles_command, "s*" },
  { "database", database_command, "ss*" },
  { "default-database", default_database_command, "" },
  { "no-default-database", no_default_database_command, "" },
  { "bibliography", bibliography_command, "ss*" },
  { "annotate", annotate_command, "F?s?" },
  { "no-annotate", no_annotate_command, "" },
  { "reverse", reverse_command, "s" },
  { "no-reverse", no_reverse_command, "" },
  { "abbreviate", abbreviate_command, "ss?s?s?s?" },
  { "no-abbreviate", no_abbreviate_command, "" },
  { "search-ignore", search_ignore_command, "f?" },
  { "no-search-ignore", no_search_ignore_command, "" },
  { "search-truncate", search_truncate_command, "i?" },
  { "no-search-truncate", no_search_truncate_command, "" },
  { "discard", discard_command, "f?" },
  { "no-discard", no_discard_command, "" },
  { "label", label_command, "s" },
  { "abbreviate-label-ranges", abbreviate_label_ranges_command, "s?" },
  { "no-abbreviate-label-ranges", no_abbreviate_label_ranges_command, "" },
  { "label-in-reference", label_in_reference_command, "" },
  { "no-label-in-reference", no_label_in_reference_command, "" },
  { "label-in-text", label_in_text_command, "" },
  { "no-label-in-text", no_label_in_text_command, "" },
  { "sort-adjacent-labels", sort_adjacent_labels_command, "" },
  { "no-sort-adjacent-labels", no_sort_adjacent_labels_command, "" },
  { "date-as-label", date_as_label_command, "s?" },
  { "no-date-as-label", no_date_as_label_command, "" },
  { "short-label", short_label_command, "s" },
  { "no-short-label", no_short_label_command, "" },
  { "compatible", compatible_command, "" },
  { "no-compatible", no_compatible_command, "" },
  { "join-authors", join_authors_command, "sss?" },
  { "bracket-label", bracket_label_command, "sss" },
  { "separate-label-second-parts", separate_label_second_parts_command, "s" },
  { "et-al", et_al_command, "sii?" },
  { "no-et-al", no_et_al_command, "" },
};

static int check_args(const char *types, const char *name,
		      int argc, argument *argv)
{
  int argno = 0;
  while (*types) {
    if (argc == 0) {
      if (types[1] == '?')
	break;
      else if (types[1] == '*') {
	assert(types[2] == '\0');
	break;
      }
      else {
	input_stack::error("missing argument for command `%1'", name);
	return 0;
      }
    }
    switch (*types) {
    case 's':
      break;
    case 'i':
      {
	char *ptr;
	long n = strtol(argv->s, &ptr, 10);
	if ((n == 0 && ptr == argv->s)
	    || *ptr != '\0') {
	  input_stack::error("argument %1 for command `%2' must be an integer",
			     argno + 1, name);
	  return 0;
	}
	argv->n = (int)n;
	break;
      }
    case 'f':
      {
	for (const char *ptr = argv->s; *ptr != '\0'; ptr++)
	  if (!cs_field_name(*ptr)) {
	    input_stack::error("argument %1 for command `%2' must be a list of fields",
			     argno + 1, name);
	    return 0;
	  }
	break;
      }
    case 'F':
      if (argv->s[0] == '\0' || argv->s[1] != '\0'
	  || !cs_field_name(argv->s[0])) {
	input_stack::error("argument %1 for command `%2' must be a field name",
			   argno + 1, name);
	return 0;
      }
      break;
    default:
      assert(0);
    }
    if (types[1] == '?')
      types += 2;
    else if (types[1] != '*')
      types += 1;
    --argc;
    ++argv;
    ++argno;
  }
  if (argc > 0) {
    input_stack::error("too many arguments for command `%1'", name);
    return 0;
  }
  return 1;
}

static void execute_command(const char *name, int argc, argument *argv)
{
  for (unsigned int i = 0;
       i < sizeof(command_table)/sizeof(command_table[0]); i++)
    if (strcmp(name, command_table[i].name) == 0) {
      if (check_args(command_table[i].arg_types, name, argc, argv))
	(*command_table[i].func)(argc, argv);
      return;
    }
  input_stack::error("unknown command `%1'", name);
}

static void command_loop()
{
  string command;
  for (;;) {
    command.clear();
    int res = get_word(command);
    if (res != 1) {
      if (res == 0)
	continue;
      break;
    }
    int argc = 0;
    command += '\0';
    while ((res = get_word(command)) == 1) {
      argc++;
      command += '\0';
    }
    argument *argv = new argument[argc];
    const char *ptr = command.contents();
    for (int i = 0; i < argc; i++)
      argv[i].s = ptr = strchr(ptr, '\0') + 1;
    execute_command(command.contents(), argc, argv);
    a_delete argv;
    if (res == -1)
      break;
  }
}

void process_commands(const char *file)
{
  input_stack::init();
  input_stack::push_file(file);
  command_loop();
}

void process_commands(string &s, const char *file, int lineno)
{
  input_stack::init();
  input_stack::push_string(s, file, lineno);
  command_loop();
}
