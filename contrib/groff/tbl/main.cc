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

#include "table.h"

#define MAX_POINT_SIZE 99
#define MAX_VERTICAL_SPACING 72

static int compatible_flag = 0;

class table_input {
  FILE *fp;
  enum { START, MIDDLE, REREAD_T, REREAD_TE, REREAD_E, END, ERROR } state;
  string unget_stack;
public:
  table_input(FILE *);
  int get();
  int ended() { return unget_stack.empty() && state == END; }
  void unget(char);
};

table_input::table_input(FILE *p)
: fp(p), state(START)
{
}

void table_input::unget(char c)
{
  assert(c != '\0');
  unget_stack += c;
  if (c == '\n')
    current_lineno--;
}

int table_input::get()
{
  int len = unget_stack.length();
  if (len != 0) {
    unsigned char c = unget_stack[len - 1];
    unget_stack.set_length(len - 1);
    if (c == '\n')
      current_lineno++;
    return c;
  }
  int c;
  for (;;) {
    switch (state) {
    case START:
      if ((c = getc(fp)) == '.') {
	if ((c = getc(fp)) == 'T') {
	  if ((c = getc(fp)) == 'E') {
	    if (compatible_flag) {
	      state = END;
	      return EOF;
	    }
	    else {
	      c = getc(fp);
	      if (c != EOF)
		ungetc(c, fp);
	      if (c == EOF || c == ' ' || c == '\n') {
		state = END;
		return EOF;
	      }
	      state = REREAD_TE;
	      return '.';
	    }
	  }
	  else {
	    if (c != EOF)
	      ungetc(c, fp);
	    state = REREAD_T;
	    return '.';
	  }
	}
	else {
	  if (c != EOF)
	    ungetc(c, fp);
	  state = MIDDLE;
	  return '.';
	}
      }
      else if (c == EOF) {
	state = ERROR;
	return EOF;
      }
      else {
	if (c == '\n')
	  current_lineno++;
	else {
	  state = MIDDLE;
	  if (c == '\0') {
	    error("illegal input character code 0");
	    break;
	  }
	}
	return c;
      }
      break;
    case MIDDLE:
      // handle line continuation
      if ((c = getc(fp)) == '\\') {
	c = getc(fp);
	if (c == '\n')
	  c = getc(fp);		// perhaps state ought to be START now
	else {
	  if (c != EOF)
	    ungetc(c, fp);
	  c = '\\';
	}
      }
      if (c == EOF) {
	state = ERROR;
	return EOF;
      }
      else {
	if (c == '\n') {
	  state = START;
	  current_lineno++;
	}
	else if (c == '\0') {
	  error("illegal input character code 0");
	  break;
	}
	return c;
      }
    case REREAD_T:
      state = MIDDLE;
      return 'T';
    case REREAD_TE:
      state = REREAD_E;
      return 'T';
    case REREAD_E:
      state = MIDDLE;
      return 'E';
    case END:
    case ERROR:
      return EOF;
    }
  }
}

void process_input_file(FILE *);
void process_table(table_input &in);

void process_input_file(FILE *fp)
{
  enum { START, MIDDLE, HAD_DOT, HAD_T, HAD_TS, HAD_l, HAD_lf } state;
  state = START;
  int c;
  while ((c = getc(fp)) != EOF)
    switch (state) {
    case START:
      if (c == '.')
	state = HAD_DOT;
      else {
	if (c == '\n')
	  current_lineno++;
	else
	  state = MIDDLE;
	putchar(c);
      }
      break;
    case MIDDLE:
      if (c == '\n') {
	current_lineno++;
	state = START;
      }
      putchar(c);
      break;
    case HAD_DOT:
      if (c == 'T')
	state = HAD_T;
      else if (c == 'l')
	state = HAD_l;
      else {
	putchar('.');
	putchar(c);
	if (c == '\n') {
	  current_lineno++;
	  state = START;
	}
	else
	  state = MIDDLE;
      }
      break;
    case HAD_T:
      if (c == 'S')
	state = HAD_TS;
      else {
	putchar('.');
	putchar('T');
	putchar(c);
	if (c == '\n') {
 	  current_lineno++;
	  state = START;
	}
	else
	  state = MIDDLE;
      }
      break;
    case HAD_TS:
      if (c == ' ' || c == '\n' || compatible_flag) {
	putchar('.');
	putchar('T');
	putchar('S');
	while (c != '\n') {
	  if (c == EOF) {
	    error("end of file at beginning of table");
	    return;
	  }
	  putchar(c);
	  c = getc(fp);
	}
	putchar('\n');
	printf(".if '\\*(.T'html' \\X(graphic-start(\n");
	current_lineno++;
	{
	  table_input input(fp);
	  process_table(input);
	  set_troff_location(current_filename, current_lineno);
	  if (input.ended()) {
	    printf(".if '\\*(.T'html' \\X(graphic-end(\n");
	    fputs(".TE", stdout);
	    while ((c = getc(fp)) != '\n') {
	      if (c == EOF) {
		putchar('\n');
		return;
	      }
	      putchar(c);
	    }
	    putchar('\n');
	    current_lineno++;
	  }
	}
	state = START;
      }
      else {
	fputs(".TS", stdout);
	putchar(c);
	state = MIDDLE;
      }
      break;
    case HAD_l:
      if (c == 'f')
	state = HAD_lf;
      else {
	putchar('.');
	putchar('l');
	putchar(c);
	if (c == '\n') {
 	  current_lineno++;
	  state = START;
	}
	else
	  state = MIDDLE;
      }
      break;
    case HAD_lf:
      if (c == ' ' || c == '\n' || compatible_flag) {
	string line;
	while (c != EOF) {
	  line += c;
	  if (c == '\n') {
	    current_lineno++;
	    break;
	  }
	  c = getc(fp);
	}
	line += '\0';
	interpret_lf_args(line.contents());
	printf(".lf%s", line.contents());
	state = START;
      }
      else {
	fputs(".lf", stdout);
	putchar(c);
	state = MIDDLE;
      }
      break;
    default:
      assert(0);
    }
  switch(state) {
  case START:
    break;
  case MIDDLE:
    putchar('\n');
    break;
  case HAD_DOT:
    fputs(".\n", stdout);
    break;
  case HAD_l:
    fputs(".l\n", stdout);
    break;
  case HAD_T:
    fputs(".T\n", stdout);
    break;
  case HAD_lf:
    fputs(".lf\n", stdout);
    break;
  case HAD_TS:
    fputs(".TS\n", stdout);
    break;
  }
  if (fp != stdin)
    fclose(fp);
}

struct options {
  unsigned flags;
  int linesize;
  char delim[2];
  char tab_char;
  char decimal_point_char;

  options();
};

options::options()
: flags(0), tab_char('\t'), decimal_point_char('.'), linesize(0)
{
  delim[0] = delim[1] = '\0';
}

// Return non-zero if p and q are the same ignoring case.

int strieq(const char *p, const char *q)
{
  for (; cmlower(*p) == cmlower(*q); p++, q++)
    if (*p == '\0')
      return 1;
  return 0;
}

// return 0 if we should give up in this table

options *process_options(table_input &in)
{
  options *opt = new options;
  string line;
  int level = 0;
  for (;;) {
    int c = in.get();
    if (c == EOF) {
      int i = line.length();
      while (--i >= 0)
	in.unget(line[i]);
      return opt;
    }
    if (c == '\n') {
      in.unget(c);
      int i = line.length();
      while (--i >= 0)
	in.unget(line[i]);
      return opt;
    }
    else if (c == '(')
      level++;
    else if (c == ')')
      level--;
    else if (c == ';' && level == 0) {
      line += '\0';
      break;
    }
    line += c;
  }
  if (line.empty())
    return opt;
  char *p = &line[0];
  for (;;) {
    while (!csalpha(*p) && *p != '\0')
      p++;
    if (*p == '\0')
      break;
    char *q = p;
    while (csalpha(*q))
      q++;
    char *arg = 0;
    if (*q != '(' && *q != '\0')
      *q++ = '\0';
    while (csspace(*q))
      q++;
    if (*q == '(') {
      *q++ = '\0';
      arg = q;
      while (*q != ')' && *q != '\0')
	q++;
      if (*q == '\0')
	error("missing `)'");
      else
	*q++ = '\0';
    }
    if (*p == '\0') {
      if (arg)
	error("argument without option");
    }
    else if (strieq(p, "tab")) {
      if (!arg)
	error("`tab' option requires argument in parentheses");
      else {
	if (arg[0] == '\0' || arg[1] != '\0')
	  error("argument to `tab' option must be a single character");
	else
	  opt->tab_char = arg[0];
      }
    }
    else if (strieq(p, "linesize")) {
      if (!arg)
	error("`linesize' option requires argument in parentheses");
      else {
	if (sscanf(arg, "%d", &opt->linesize) != 1)
	  error("bad linesize `%s'", arg);
	else if (opt->linesize <= 0) {
	  error("linesize must be positive");
	  opt->linesize = 0;
	}
      }
    }
    else if (strieq(p, "delim")) {
      if (!arg)
	error("`delim' option requires argument in parentheses");
      else if (arg[0] == '\0' || arg[1] == '\0' || arg[2] != '\0')
	error("argument to `delim' option must be two characters");
      else {
	opt->delim[0] = arg[0];
	opt->delim[1] = arg[1];
      }
    }
    else if (strieq(p, "center") || strieq(p, "centre")) {
      if (arg)
	error("`center' option does not take a argument");
      opt->flags |= table::CENTER;
    }
    else if (strieq(p, "expand")) {
      if (arg)
	error("`expand' option does not take a argument");
      opt->flags |= table::EXPAND;
    }
    else if (strieq(p, "box") || strieq(p, "frame")) {
      if (arg)
	error("`box' option does not take a argument");
      opt->flags |= table::BOX;
    }
    else if (strieq(p, "doublebox") || strieq(p, "doubleframe")) {
      if (arg)
	error("`doublebox' option does not take a argument");
      opt->flags |= table::DOUBLEBOX;
    }
    else if (strieq(p, "allbox")) {
      if (arg)
	error("`allbox' option does not take a argument");
      opt->flags |= table::ALLBOX;
    }
    else if (strieq(p, "nokeep")) {
      if (arg)
	error("`nokeep' option does not take a argument");
      opt->flags |= table::NOKEEP;
    }
    else if (strieq(p, "decimalpoint")) {
      if (!arg)
	error("`decimalpoint' option requires argument in parentheses");
      else {
	if (arg[0] == '\0' || arg[1] != '\0')
	  error("argument to `decimalpoint' option must be a single character");
	else
	  opt->decimal_point_char = arg[0];
      }
    }
    else {
      error("unrecognised global option `%1'", p);
      // delete opt;
      // return 0;
    }
    p = q;
  }
  return opt;
}

entry_modifier::entry_modifier()
: vertical_alignment(CENTER), zero_width(0), stagger(0)
{
  vertical_spacing.inc = vertical_spacing.val = 0;
  point_size.inc = point_size.val = 0;
}

entry_modifier::~entry_modifier()
{
}

entry_format::entry_format() : type(FORMAT_LEFT)
{
}

entry_format::entry_format(format_type t) : type(t)
{
}

void entry_format::debug_print() const
{
  switch (type) {
  case FORMAT_LEFT:
    putc('l', stderr);
    break;
  case FORMAT_CENTER:
    putc('c', stderr);
    break;
  case FORMAT_RIGHT:
    putc('r', stderr);
    break;
  case FORMAT_NUMERIC:
    putc('n', stderr);
    break;
  case FORMAT_ALPHABETIC:
    putc('a', stderr);
    break;
  case FORMAT_SPAN:
    putc('s', stderr);
    break;
  case FORMAT_VSPAN:
    putc('^', stderr);
    break;
  case FORMAT_HLINE:
    putc('_', stderr);
    break;
  case FORMAT_DOUBLE_HLINE:
    putc('=', stderr);
    break;
  default:
    assert(0);
    break;
  }
  if (point_size.val != 0) {
    putc('p', stderr);
    if (point_size.inc > 0)
      putc('+', stderr);
    else if (point_size.inc < 0)
      putc('-', stderr);
    fprintf(stderr, "%d ", point_size.val);
  }
  if (vertical_spacing.val != 0) {
    putc('v', stderr);
    if (vertical_spacing.inc > 0)
      putc('+', stderr);
    else if (vertical_spacing.inc < 0)
      putc('-', stderr);
    fprintf(stderr, "%d ", vertical_spacing.val);
  }
  if (!font.empty()) {
    putc('f', stderr);
    put_string(font, stderr);
    putc(' ', stderr);
  }
  switch (vertical_alignment) {
  case entry_modifier::CENTER:
    break;
  case entry_modifier::TOP:
    putc('t', stderr);
    break;
  case entry_modifier::BOTTOM:
    putc('d', stderr);
    break;
  }
  if (zero_width)
    putc('z', stderr);
  if (stagger)
    putc('u', stderr);
}

struct format {
  int nrows;
  int ncolumns;
  int *separation;
  string *width;
  char *equal;
  entry_format **entry;
  char **vline;

  format(int nr, int nc);
  ~format();
  void add_rows(int n);
};

format::format(int nr, int nc) : nrows(nr), ncolumns(nc)
{
  int i;
  separation = ncolumns > 1 ? new int[ncolumns - 1] : 0;
  for (i = 0; i < ncolumns-1; i++)
    separation[i] = -1;
  width = new string[ncolumns];
  equal = new char[ncolumns];
  for (i = 0; i < ncolumns; i++)
    equal[i] = 0;
  entry = new entry_format *[nrows];
  for (i = 0; i < nrows; i++)
    entry[i] = new entry_format[ncolumns];
  vline = new char*[nrows];
  for (i = 0; i < nrows; i++) {
    vline[i] = new char[ncolumns+1];
    for (int j = 0; j < ncolumns+1; j++)
      vline[i][j] = 0;
  }
}

void format::add_rows(int n)
{
  int i;
  char **old_vline = vline;
  vline = new char*[nrows + n];
  for (i = 0; i < nrows; i++)
    vline[i] = old_vline[i];
  a_delete old_vline;
  for (i = 0; i < n; i++) {
    vline[nrows + i] = new char[ncolumns + 1];
    for (int j = 0; j < ncolumns + 1; j++)
      vline[nrows + i][j] = 0;
  }
  entry_format **old_entry = entry;
  entry = new entry_format *[nrows + n];
  for (i = 0; i < nrows; i++)
    entry[i] = old_entry[i];
  a_delete old_entry;
  for (i = 0; i < n; i++)
    entry[nrows + i] = new entry_format[ncolumns];
  nrows += n;
}

format::~format()
{
  a_delete separation;
  ad_delete(ncolumns) width;
  a_delete equal;
  for (int i = 0; i < nrows; i++) {
    a_delete vline[i];
    ad_delete(ncolumns) entry[i];
  }
  a_delete vline;
  a_delete entry;
}

struct input_entry_format : public entry_format {
  input_entry_format *next;
  string width;
  int separation;
  int vline;
  int pre_vline;
  int last_column;
  int equal;
  input_entry_format(format_type, input_entry_format * = 0);
  ~input_entry_format();
  void debug_print();
};

input_entry_format::input_entry_format(format_type t, input_entry_format *p)
: entry_format(t), next(p)
{
  separation = -1;
  last_column = 0;
  vline = 0;
  pre_vline = 0;
  equal = 0;
}

input_entry_format::~input_entry_format()
{
}

void free_input_entry_format_list(input_entry_format *list)
{
  while (list) {
    input_entry_format *tem = list;
    list = list->next;
    delete tem;
  }
}

void input_entry_format::debug_print()
{
  int i;
  for (i = 0; i < pre_vline; i++)
    putc('|', stderr);
  entry_format::debug_print();
  if (!width.empty()) {
    putc('w', stderr);
    putc('(', stderr);
    put_string(width, stderr);
    putc(')', stderr);
  }
  if (equal)
    putc('e', stderr);
  if (separation >= 0)
    fprintf(stderr, "%d", separation); 
  for (i = 0; i < vline; i++)
    putc('|', stderr);
  if (last_column)
    putc(',', stderr);
}

// Return zero if we should give up on this table.
// If this is a continuation format line, current_format will be the current
// format line.

format *process_format(table_input &in, options *opt,
		       format *current_format = 0)
{
  input_entry_format *list = 0;
  int c = in.get();
  for (;;) {
    int pre_vline = 0;
    int got_format = 0;
    int got_period = 0;
    format_type t;
    for (;;) {
      if (c == EOF) {
	error("end of input while processing format");
	free_input_entry_format_list(list);
	return 0;
      }
      switch (c) {
      case 'n':
      case 'N':
	t = FORMAT_NUMERIC;
	got_format = 1;
	break;
      case 'a':
      case 'A':
	got_format = 1;
	t = FORMAT_ALPHABETIC;
	break;
      case 'c':
      case 'C':
	got_format = 1;
	t = FORMAT_CENTER;
	break;
      case 'l':
      case 'L':
	got_format = 1;
	t = FORMAT_LEFT;
	break;
      case 'r':
      case 'R':
	got_format = 1;
	t = FORMAT_RIGHT;
	break;
      case 's':
      case 'S':
	got_format = 1;
	t = FORMAT_SPAN;
	break;
      case '^':
	got_format = 1;
	t = FORMAT_VSPAN;
	break;
      case '_':
      case '-':			// tbl also accepts this
	got_format = 1;
	t = FORMAT_HLINE;
	break;
      case '=':
	got_format = 1;
	t = FORMAT_DOUBLE_HLINE;
	break;
      case '.':
	got_period = 1;
	break;
      case '|':
	pre_vline++;
	break;
      case ' ':
      case '\t':
      case '\n':
	break;
      default:
	if (c == opt->tab_char)
	  break;
	error("unrecognised format `%1'", char(c));
	free_input_entry_format_list(list);
	return 0;
      }
      if (got_period)
	break;
      c = in.get();
      if (got_format)
	break;
    }
    if (got_period)
      break;
    list = new input_entry_format(t, list);
    if (pre_vline)
      list->pre_vline = pre_vline;
    int success = 1;
    do {
      switch (c) {
      case 't':
      case 'T':
	c = in.get();
	list->vertical_alignment = entry_modifier::TOP;
	break;
      case 'd':
      case 'D':
	c = in.get();
	list->vertical_alignment = entry_modifier::BOTTOM;
	break;
      case 'u':
      case 'U':
	c = in.get();
	list->stagger = 1;
	break;
      case 'z':
      case 'Z':
	c = in.get();
	list->zero_width = 1;
	break;
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
	{
	  int w = 0;
	  do {
	    w = w*10 + (c - '0');
	    c = in.get();
	  } while (c != EOF && csdigit(c));
	  list->separation = w;
	}
	break;
      case 'f':
      case 'F':
	do {
	  c = in.get();
	} while (c == ' ' || c == '\t');
	if (c == EOF) {
	  error("missing font name");
	  break;
	}
	if (c == '(') {
	  for (;;) {
	    c = in.get();
	    if (c == EOF || c == ' ' || c == '\t') {
	      error("missing `)'");
	      break;
	    }
	    if (c == ')') {
	      c = in.get();
	      break;
	    }
	    list->font += char(c);
	  }
	}
	else {
	  list->font = c;
	  char cc = c;
	  c = in.get();
	  if (!csdigit(cc)
	      && c != EOF && c != ' ' && c != '\t' && c != '.' && c != '\n') {
	    list->font += char(c);
	    c = in.get();
	  }
	}
	break;
      case 'v':
      case 'V':
	c = in.get();
	list->vertical_spacing.val = 0;
	list->vertical_spacing.inc = 0;
	if (c == '+' || c == '-') {
	  list->vertical_spacing.inc = (c == '+' ? 1 : -1);
	  c = in.get();
	}
	if (c == EOF || !csdigit(c)) {
	  error("`v' modifier must be followed by number");
	  list->vertical_spacing.inc = 0;
	}
	else {
	  do {
	    list->vertical_spacing.val *= 10;
	    list->vertical_spacing.val += c - '0';
	    c = in.get();
	  } while (c != EOF && csdigit(c));
	}
	if (list->vertical_spacing.val > MAX_VERTICAL_SPACING
	    || list->vertical_spacing.val < -MAX_VERTICAL_SPACING) {
	  error("unreasonable point size");
	  list->vertical_spacing.val = 0;
	  list->vertical_spacing.inc = 0;
	}
	break;
      case 'p':
      case 'P':
	c = in.get();
	list->point_size.val = 0;
	list->point_size.inc = 0;
	if (c == '+' || c == '-') {
	  list->point_size.inc = (c == '+' ? 1 : -1);
	  c = in.get();
	}
	if (c == EOF || !csdigit(c)) {
	  error("`p' modifier must be followed by number");
	  list->point_size.inc = 0;
	}
	else {
	  do {
	    list->point_size.val *= 10;
	    list->point_size.val += c - '0';
	    c = in.get();
	  } while (c != EOF && csdigit(c));
	}
	if (list->point_size.val > MAX_POINT_SIZE
	    || list->point_size.val < -MAX_POINT_SIZE) {
	  error("unreasonable point size");
	  list->point_size.val = 0;
	  list->point_size.inc = 0;
	}
	break;
      case 'w':
      case 'W':
	c = in.get();
	while (c == ' ' || c == '\t')
	  c = in.get();
	if (c == '(') {
	  list->width = "";
	  c = in.get();
	  while (c != ')') {
	    if (c == EOF || c == '\n') {
	      error("missing `)'");
	      free_input_entry_format_list(list);
	      return 0;
	    }
	    list->width += c;
	    c = in.get();
	  }
	  c = in.get();
	}
	else {
	  if (c == '+' || c == '-') {
	    list->width = char(c);
	    c = in.get();
	  }
	  else
	    list->width = "";
	  if (c == EOF || !csdigit(c))
	    error("bad argument for `w' modifier");
	  else {
	    do {
	      list->width += char(c);
	      c = in.get();
	    } while (c != EOF && csdigit(c));
	  }
	}
	break;
      case 'e':
      case 'E':
	c = in.get();
	list->equal++;
	break;
      case '|':
	c = in.get();
	list->vline++;
	break;
      case 'B':
      case 'b':
	c = in.get();
	list->font = "B";
	break;
      case 'I':
      case 'i':
	c = in.get();
	list->font = "I";
	break;
      case ' ':
      case '\t':
	c = in.get();
	break;
      default:
	if (c == opt->tab_char)
	  c = in.get();
	else
	  success = 0;
	break;
      }
    } while (success);
    if (list->vline > 2) {
      list->vline = 2;
      error("more than 2 vertical bars between key letters");
    }
    if (c == '\n' || c == ',') {
      c = in.get();
      list->last_column = 1;
    }
  }
  if (c == '.') {
    do {
      c = in.get();
    } while (c == ' ' || c == '\t');
    if (c != '\n') {
      error("`.' not last character on line");
      free_input_entry_format_list(list);
      return 0;
    }
  }
  if (!list) {
    error("no format");
    free_input_entry_format_list(list);
    return 0;
  }
  list->last_column = 1;
  // now reverse the list so that the first row is at the beginning
  input_entry_format *rev = 0;
  while (list != 0) {
    input_entry_format *tem = list->next;
    list->next = rev;
    rev = list;
    list = tem;
  }
  list = rev;
  input_entry_format *tem;

#if 0
  for (tem = list; tem; tem = tem->next)
    tem->debug_print();
  putc('\n', stderr);
#endif
  // compute number of columns and rows
  int ncolumns = 0;
  int nrows = 0;
  int col = 0;
  for (tem = list; tem; tem = tem->next) {
    if (tem->last_column) {
      if (col >= ncolumns)
	ncolumns = col + 1;
      col = 0;
      nrows++;
    }
    else
      col++;
  }
  int row;
  format *f;
  if (current_format) {
    if (ncolumns > current_format->ncolumns) {
      error("cannot increase the number of columns in a continued format");
      free_input_entry_format_list(list);
      return 0;
    }
    f = current_format;
    row = f->nrows;
    f->add_rows(nrows);
  }
  else {
    f = new format(nrows, ncolumns);
    row = 0;
  }
  col = 0;
  for (tem = list; tem; tem = tem->next) {
    f->entry[row][col] = *tem;
    if (col < ncolumns-1) {
      // use the greatest separation
      if (tem->separation > f->separation[col]) {
	if (current_format)
	  error("cannot change column separation in continued format");
	else
	  f->separation[col] = tem->separation;
      }
    }
    else if (tem->separation >= 0)
      error("column separation specified for last column");
    if (tem->equal && !f->equal[col]) {
      if (current_format)
	error("cannot change which columns are equal in continued format");
      else
	f->equal[col] = 1;
    }
    if (!tem->width.empty()) {
      // use the last width
      if (!f->width[col].empty() && f->width[col] != tem->width)
	error("multiple widths for column %1", col+1);
      f->width[col] = tem->width;
    }
    if (tem->pre_vline) {
      assert(col == 0);
      f->vline[row][col] = tem->pre_vline;
    }
    f->vline[row][col+1] = tem->vline;
    if (tem->last_column) {
      row++;
      col = 0;
    }
    else
      col++;
  }
  free_input_entry_format_list(list);
  for (col = 0; col < ncolumns; col++) {
    entry_format *e = f->entry[f->nrows-1] + col;
    if (e->type != FORMAT_HLINE
	&& e->type != FORMAT_DOUBLE_HLINE
	&& e->type != FORMAT_SPAN)
      break;
  }
  if (col >= ncolumns) {
    error("last row of format is all lines");
    delete f;
    return 0;
  }
  return f;
}

table *process_data(table_input &in, format *f, options *opt)
{
  char tab_char = opt->tab_char;
  int ncolumns = f->ncolumns;
  int current_row = 0;
  int format_index = 0;
  int give_up = 0;
  enum { DATA_INPUT_LINE, TROFF_INPUT_LINE, SINGLE_HLINE, DOUBLE_HLINE } type;
  table *tbl = new table(ncolumns, opt->flags, opt->linesize,
			 opt->decimal_point_char);
  if (opt->delim[0] != '\0')
    tbl->set_delim(opt->delim[0], opt->delim[1]);
  for (;;) {
    // first determine what type of line this is
    int c = in.get();
    if (c == EOF)
      break;
    if (c == '.') {
      int d = in.get();
      if (d != EOF && csdigit(d)) {
	in.unget(d);
	type = DATA_INPUT_LINE;
      }
      else {
	in.unget(d);
	type = TROFF_INPUT_LINE;
      }
    }
    else if (c == '_' || c == '=') {
      int d = in.get();
      if (d == '\n') {
	if (c == '_')
	  type = SINGLE_HLINE;
	else
	  type = DOUBLE_HLINE;
      }
      else {
	in.unget(d);
	type = DATA_INPUT_LINE;
      }
    }
    else {
      type = DATA_INPUT_LINE;
    }
    switch (type) {
    case DATA_INPUT_LINE:
      {
	string input_entry;
	if (format_index >= f->nrows)
	  format_index = f->nrows - 1;
	// A format row that is all lines doesn't use up a data line.
	while (format_index < f->nrows - 1) {
	  int c;
	  for (c = 0; c < ncolumns; c++) {
	    entry_format *e = f->entry[format_index] + c;
	    if (e->type != FORMAT_HLINE
		&& e->type != FORMAT_DOUBLE_HLINE
		// Unfortunately tbl treats a span as needing data.
		// && e->type != FORMAT_SPAN
		)
	      break;
	  }
	  if (c < ncolumns)
	    break;
	  for (c = 0; c < ncolumns; c++)
	    tbl->add_entry(current_row, c, input_entry,
			   f->entry[format_index] + c, current_filename,
			   current_lineno);
	  tbl->add_vlines(current_row, f->vline[format_index]);
	  format_index++;
	  current_row++;
	}
	entry_format *line_format = f->entry[format_index];
	int col = 0;
	int row_comment = 0;
	for (;;) {
	  if (c == tab_char || c == '\n') {
	    int ln = current_lineno;
	    if (c == '\n')
	      --ln;
	    while (col < ncolumns
		   && line_format[col].type == FORMAT_SPAN) {
	      tbl->add_entry(current_row, col, "", &line_format[col],
			     current_filename, ln);
	      col++;
	    }
	    if (c == '\n' && input_entry.length() == 2
		&& input_entry[0] == 'T' && input_entry[1] == '{') {
	      input_entry = "";
	      ln++;
	      enum {
		START, MIDDLE, GOT_T, GOT_RIGHT_BRACE, GOT_DOT,
		GOT_l, GOT_lf, END
	      } state = START;
	      while (state != END) {
		c = in.get();
		if (c == EOF)
		  break;
		switch (state) {
		case START:
		  if (c == 'T')
		    state = GOT_T;
		  else if (c == '.')
		    state = GOT_DOT;
		  else {
		    input_entry += c;
		    if (c != '\n')
		      state = MIDDLE;
		  }
		  break;
		case GOT_T:
		  if (c == '}')
		    state = GOT_RIGHT_BRACE;
		  else {
		    input_entry += 'T';
		    input_entry += c;
		    state = c == '\n' ? START : MIDDLE;
		  }
		  break;
		case GOT_DOT:
		  if (c == 'l')
		    state = GOT_l;
		  else {
		    input_entry += '.';
		    input_entry += c;
		    state = c == '\n' ? START : MIDDLE;
		  }
		  break;
		case GOT_l:
		  if (c == 'f')
		    state = GOT_lf;
		  else {
		    input_entry += ".l";
		    input_entry += c;
		    state = c == '\n' ? START : MIDDLE;
		  }
		  break;
		case GOT_lf:
		  if (c == ' ' || c == '\n' || compatible_flag) {
		    string args;
		    input_entry += ".lf";
		    while (c != EOF) {
		      args += c;
		      if (c == '\n')
			break;
		      c = in.get();
		    }
		    args += '\0';
		    interpret_lf_args(args.contents());
		    // remove the '\0'
		    args.set_length(args.length() - 1);
		    input_entry += args;
		    state = START;
		  }
		  else {
		    input_entry += ".lf";
		    input_entry += c;
		    state = MIDDLE;
		  }
		  break;
		case GOT_RIGHT_BRACE:
		  if (c == '\n' || c == tab_char)
		    state = END;
		  else {
		    input_entry += 'T';
		    input_entry += '}';
		    input_entry += c;
		    state = c == '\n' ? START : MIDDLE;
		  }
		  break;
		case MIDDLE:
		  if (c == '\n')
		    state = START;
		  input_entry += c;
		  break;
		case END:
		default:
		  assert(0);
		}
	      }
	      if (c == EOF) {
		error("end of data in middle of text block");
		give_up = 1;
		break;
	      }
	    }
	    if (col >= ncolumns) {
	      if (!input_entry.empty()) {
		if (input_entry.length() >= 2
		    && input_entry[0] == '\\'
		    && input_entry[1] == '"')
		  row_comment = 1;
		else if (!row_comment) {
		  if (c == '\n')
		    in.unget(c);
		  input_entry += '\0';
		  error("excess data entry `%1' discarded",
			input_entry.contents());
		  if (c == '\n')
		    (void)in.get();
		}
	      }
	    }
	    else
	      tbl->add_entry(current_row, col, input_entry,
			     &line_format[col], current_filename, ln);
	    col++;
	    if (c == '\n')
	      break;
	    input_entry = "";
	  }
	  else
	    input_entry += c;
	  c = in.get();
	  if (c == EOF)
	    break;
	}
	if (give_up)
	  break;
	input_entry = "";
	for (; col < ncolumns; col++)
	  tbl->add_entry(current_row, col, input_entry, &line_format[col],
			 current_filename, current_lineno - 1);
	tbl->add_vlines(current_row, f->vline[format_index]);
	current_row++;
	format_index++;
      }
      break;
    case TROFF_INPUT_LINE:
      {
	string line;
	int ln = current_lineno;
	for (;;) {
	  line += c;
	  if (c == '\n')
	    break;
	  c = in.get();
	  if (c == EOF) {
	    break;
	  }
	}
	tbl->add_text_line(current_row, line, current_filename, ln);
	if (line.length() >= 4 
	    && line[0] == '.' && line[1] == 'T' && line[2] == '&') {
	  format *newf = process_format(in, opt, f);
	  if (newf == 0)
	    give_up = 1;
	  else
	    f = newf;
	}
	if (line.length() >= 3
	    && line[0] == '.' && line[1] == 'f' && line[2] == 'f') {
	  line += '\0';
	  interpret_lf_args(line.contents() + 3);
	}
      }
      break;
    case SINGLE_HLINE:
      tbl->add_single_hline(current_row);
      break;
    case DOUBLE_HLINE:
      tbl->add_double_hline(current_row);
      break;
    default:
      assert(0);
    }
    if (give_up)
      break;
  }
  if (!give_up && current_row == 0) {
    error("no real data");
    give_up = 1;
  }
  if (give_up) {
    delete tbl;
    return 0;
  }
  // Do this here rather than at the beginning in case continued formats
  // change it.
  int i;
  for (i = 0; i < ncolumns - 1; i++)
    if (f->separation[i] >= 0)
      tbl->set_column_separation(i, f->separation[i]);
  for (i = 0; i < ncolumns; i++)
    if (!f->width[i].empty())
      tbl->set_minimum_width(i, f->width[i]);
  for (i = 0; i < ncolumns; i++)
    if (f->equal[i])
      tbl->set_equal_column(i);
  return tbl;
}

void process_table(table_input &in)
{
  int c;
  options *opt = 0;
  format *form = 0;
  table *tbl = 0;
  if ((opt = process_options(in)) != 0 
      && (form = process_format(in, opt)) != 0
      && (tbl = process_data(in, form, opt)) != 0) {
    tbl->print();
    delete tbl;
  }
  else {
    error("giving up on this table");
    while ((c = in.get()) != EOF)
      ;
  }
  delete opt;
  delete form;
  if (!in.ended())
    error("premature end of file");
}

static void usage()
{
  fprintf(stderr, "usage: %s [ -vC ] [ files... ]\n", program_name);
  exit(1);
}

int main(int argc, char **argv)
{
  program_name = argv[0];
  static char stderr_buf[BUFSIZ];
  setbuf(stderr, stderr_buf);
  int opt;
  while ((opt = getopt(argc, argv, "vCT:")) != EOF)
    switch (opt) {
    case 'C':
      compatible_flag = 1;
      break;
    case 'v':
      {
	extern const char *version_string;
	fprintf(stderr, "GNU tbl version %s\n", version_string);
	fflush(stderr);
	break;
      }
    case 'T':
      // I'm sick of getting bug reports from IRIX users
      break;
    case '?':
      usage();
      break;
    default:
      assert(0);
    }
  printf(".if !\\n(.g .ab GNU tbl requires GNU troff.\n"
	 ".if !dTS .ds TS\n"
	 ".if !dTE .ds TE\n");
  if (argc > optind) {
    for (int i = optind; i < argc; i++) 
      if (argv[i][0] == '-' && argv[i][1] == '\0') {
	current_filename = "-";
	current_lineno = 1;
	printf(".lf 1 -\n");
	process_input_file(stdin);
      }
      else {
	errno = 0;
	FILE *fp = fopen(argv[i], "r");
	if (fp == 0) {
	  current_lineno = -1;
	  error("can't open `%1': %2", argv[i], strerror(errno));
	}
	else {
	  current_lineno = 1;
	  current_filename = argv[i];
	  printf(".lf 1 %s\n", current_filename);
	  process_input_file(fp);
	}
      }
  }
  else {
    current_filename = "-";
    current_lineno = 1;
    printf(".lf 1 -\n");
    process_input_file(stdin);
  }
  if (ferror(stdout) || fflush(stdout) < 0)
    fatal("output error");
  return 0;
}

