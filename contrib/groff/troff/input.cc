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

#include "troff.h"
#include "symbol.h"
#include "dictionary.h"
#include "hvunits.h"
#include "env.h"
#include "request.h"
#include "node.h"
#include "reg.h"
#include "token.h"
#include "div.h"
#include "charinfo.h"
#include "font.h"
#include "searchpath.h"
#include "macropath.h"
#include "defs.h"

// Needed for getpid().
#include "posix.h"

#ifdef ISATTY_MISSING
#undef isatty
#define isatty(n) (1)
#else /* not ISATTY_MISSING */
#ifndef isatty
extern "C" {
  int isatty(int);
}
#endif /* not isatty */
#endif /* not ISATTY_MISSING */

#define USAGE_EXIT_CODE 1
#define MACRO_PREFIX "tmac."
#define STARTUP_FILE "troffrc"
#define DEFAULT_INPUT_STACK_LIMIT 1000

#ifndef DEFAULT_WARNING_MASK
// warnings that are enabled by default
#define DEFAULT_WARNING_MASK \
     (WARN_CHAR|WARN_NUMBER|WARN_BREAK|WARN_SPACE|WARN_FONT)
#endif

// initial size of buffer for reading names; expanded as necessary
#define ABUF_SIZE 16

#ifdef COLUMN
void init_column_requests();
#endif /* COLUMN */

static node *read_draw_node();
void handle_first_page_transition();
static void push_token(const token &);
void copy_file();
#ifdef COLUMN
void vjustify();
#endif /* COLUMN */
void transparent();
void transparent_file();

const char *program_name = 0;
token tok;
int break_flag = 0;
static int backtrace_flag = 0;
#ifndef POPEN_MISSING
char *pipe_command = 0;
#endif
charinfo *charset_table[256];

static int warning_mask = DEFAULT_WARNING_MASK;
static int inhibit_errors = 0;
static int ignoring = 0;

static void enable_warning(const char *);
static void disable_warning(const char *);

static int escape_char = '\\';
static symbol end_macro_name;
static symbol blank_line_macro_name;
static int compatible_flag = 0;
int ascii_output_flag = 0;
int suppress_output_flag = 0;

int tcommand_flag = 0;

static int get_copy(node**, int = 0);
static void copy_mode_error(const char *,
			    const errarg & = empty_errarg,
			    const errarg & = empty_errarg,
			    const errarg & = empty_errarg);

static symbol read_escape_name();
static void interpolate_string(symbol);
static void interpolate_macro(symbol);
static void interpolate_number_format(symbol);
static void interpolate_environment_variable(symbol);

static void interpolate_arg(symbol);
static request_or_macro *lookup_request(symbol);
static int get_delim_number(units *, int);
static int get_delim_number(units *, int, units);
static int get_line_arg(units *res, int si, charinfo **cp);
static int read_size(int *);
static symbol get_delim_name();
static void init_registers();
static void trapping_blank_line();

struct input_iterator;
input_iterator *make_temp_iterator(const char *);
const char *input_char_description(int);

const int ESCAPE_QUESTION = 015;
const int BEGIN_TRAP = 016;
const int END_TRAP = 017;
const int PAGE_EJECTOR = 020;
const int ESCAPE_NEWLINE = 021;
const int ESCAPE_AMPERSAND = 022;
const int ESCAPE_UNDERSCORE = 023;
const int ESCAPE_BAR = 024;
const int ESCAPE_CIRCUMFLEX = 025;
const int ESCAPE_LEFT_BRACE = 026;
const int ESCAPE_RIGHT_BRACE = 027;
const int ESCAPE_LEFT_QUOTE = 030;
const int ESCAPE_RIGHT_QUOTE = 031;
const int ESCAPE_HYPHEN = 032;
const int ESCAPE_BANG = 033;
const int ESCAPE_c = 034;
const int ESCAPE_e = 035;
const int ESCAPE_PERCENT = 036;
const int ESCAPE_SPACE = 037;

const int TITLE_REQUEST = 0200;
const int COPY_FILE_REQUEST = 0201;
const int TRANSPARENT_FILE_REQUEST = 0202;
#ifdef COLUMN
const int VJUSTIFY_REQUEST = 0203;
#endif /* COLUMN */
const int ESCAPE_E = 0204;
const int LAST_PAGE_EJECTOR = 0205;
const int ESCAPE_RIGHT_PARENTHESIS = 0206;

void set_escape_char()
{
  if (has_arg()) {
    if (tok.ch() == 0) {
      error("bad escape character");
      escape_char = '\\';
    }
    else
      escape_char = tok.ch();
  }
  else
    escape_char = '\\';
  skip_line();
}

void escape_off()
{
  escape_char = 0;
  skip_line();
}

class input_iterator {
public:
  input_iterator();
  virtual ~input_iterator();
  int get(node **);
  friend class input_stack;
protected:
  const unsigned char *ptr;
  const unsigned char *eptr;
  input_iterator *next;
private:
  virtual int fill(node **);
  virtual int peek();
  virtual int has_args() { return 0; }
  virtual int nargs() { return 0; }
  virtual input_iterator *get_arg(int) { return NULL; }
  virtual int get_location(int, const char **,  int *)
    { return 0; }
  virtual void backtrace() {}
  virtual int set_location(const char *, int)
    { return 0; }
  virtual int next_file(FILE *, const char *) { return 0; }
  virtual void shift(int) {}
  virtual int is_boundary();
  virtual int internal_level() { return 0; }
  virtual int is_file() { return 0; }
};

input_iterator::input_iterator()
: ptr(0), eptr(0)
{
}

input_iterator::~input_iterator()
{
}

int input_iterator::fill(node **)
{
  return EOF;
}

int input_iterator::peek()
{
  return EOF;
}

int input_iterator::is_boundary()
{
  return 0;
}

inline int input_iterator::get(node **p)
{
  return ptr < eptr ? *ptr++ : fill(p);
}


class input_boundary : public input_iterator {
public:
  int is_boundary() { return 1; }
};

class file_iterator : public input_iterator {
  FILE *fp;
  int lineno;
  const char *filename;
  int popened;
  int newline_flag;
  enum { BUF_SIZE = 512 };
  unsigned char buf[BUF_SIZE];
  void close();
public:
  file_iterator(FILE *, const char *, int = 0);
  ~file_iterator();
  int fill(node **);
  int peek();
  int get_location(int, const char **, int *);
  void backtrace();
  int set_location(const char *, int);
  int next_file(FILE *, const char *);
  int is_file();
};

file_iterator::file_iterator(FILE *f, const char *fn, int po)
: fp(f), filename(fn), lineno(1), newline_flag(0), popened(po)
{
}

file_iterator::~file_iterator()
{
  close();
}

void file_iterator::close()
{
  if (fp == stdin)
    clearerr(stdin);
#ifndef POPEN_MISSING
  else if (popened)
    pclose(fp);
#endif /* not POPEN_MISSING */
  else
    fclose(fp);
}

int file_iterator::is_file()
{
  return 1;
}

int file_iterator::next_file(FILE *f, const char *s)
{
  close();
  filename = s;
  fp = f;
  lineno = 1;
  newline_flag = 0;
  popened = 0;
  ptr = 0;
  eptr = 0;
  return 1;
}

int file_iterator::fill(node **)
{
  if (newline_flag)
    lineno++;
  newline_flag = 0;
  unsigned char *p = buf;
  ptr = p;
  unsigned char *e = p + BUF_SIZE;
  while (p < e) {
    int c = getc(fp);
    if (c == EOF)
      break;
    if (illegal_input_char(c))
      warning(WARN_INPUT, "illegal input character code %1", int(c));
    else {
      *p++ = c;
      if (c == '\n') {
	newline_flag = 1;
	break;
      }
    }
  }
  if (p > buf) {
    eptr = p;
    return *ptr++;
  }
  else {
    eptr = p;
    return EOF;
  }
}

int file_iterator::peek()
{
  int c = getc(fp);
  while (illegal_input_char(c)) {
    warning(WARN_INPUT, "illegal input character code %1", int(c));
    c = getc(fp);
  }
  if (c != EOF)
    ungetc(c, fp);
  return c;
}

int file_iterator::get_location(int /*allow_macro*/,
				const char **filenamep, int *linenop)
{
  *linenop = lineno;
  if (filename != 0 && strcmp(filename, "-") == 0)
    *filenamep = "<standard input>";
  else
    *filenamep = filename;
  return 1;
}

void file_iterator::backtrace()
{
  errprint("%1:%2: backtrace: %3 `%1'\n", filename, lineno,
	   popened ? "process" : "file");
}

int file_iterator::set_location(const char *f, int ln)
{
  if (f)
    filename = f;
  lineno = ln;
  return 1;
}

input_iterator nil_iterator;  

class input_stack {
public:
  static int get(node **);
  static int peek();
  static void push(input_iterator *);
  static input_iterator *get_arg(int);
  static int nargs();
  static int get_location(int, const char **, int *);
  static int set_location(const char *, int);
  static void backtrace();
  static void backtrace_all();
  static void next_file(FILE *, const char *);
  static void end_file();
  static void shift(int n);
  static void add_boundary();
  static void remove_boundary();
  static int get_level();
  static void clear();

  static int limit;
private:
  static input_iterator *top;
  static int level;

  static int finish_get(node **);
  static int finish_peek();
};

input_iterator *input_stack::top = &nil_iterator;
int input_stack::level = 0;
int input_stack::limit = DEFAULT_INPUT_STACK_LIMIT;

inline int input_stack::get_level()
{
  return level + top->internal_level();
}

inline int input_stack::get(node **np)
{
  return (top->ptr < top->eptr) ? *top->ptr++ : finish_get(np);
}

int input_stack::finish_get(node **np)
{
  for (;;) {
    int c = top->fill(np);
    if (c != EOF || top->is_boundary())
      return c;
    if (top == &nil_iterator)
      break;
    input_iterator *tem = top;
    top = top->next;
    level--;
    delete tem;
    if (top->ptr < top->eptr)
      return *top->ptr++;
  }
  assert(level == 0);
  return EOF;
}

inline int input_stack::peek()
{
  return (top->ptr < top->eptr) ? *top->ptr : finish_peek();
}

int input_stack::finish_peek()
{
  for (;;) {
    int c = top->peek();
    if (c != EOF || top->is_boundary())
      return c;
    if (top == &nil_iterator)
      break;
    input_iterator *tem = top;
    top = top->next;
    level--;
    delete tem;
    if (top->ptr < top->eptr)
      return *top->ptr;
  }
  assert(level == 0);
  return EOF;
}

void input_stack::add_boundary()
{
  push(new input_boundary);
}

void input_stack::remove_boundary()
{
  assert(top->is_boundary());
  input_iterator *temp = top->next;
  delete top;
  top = temp;
  level--;
}

void input_stack::push(input_iterator *in)
{
  if (in == 0)
    return;
  if (++level > limit && limit > 0)
    fatal("input stack limit exceeded (probable infinite loop)");
  in->next = top;
  top = in;
}

input_iterator *input_stack::get_arg(int i)
{
  input_iterator *p;
  for (p = top; p != NULL; p = p->next)
    if (p->has_args())
      return p->get_arg(i);
  return 0;
}

void input_stack::shift(int n)
{
  for (input_iterator *p = top; p; p = p->next)
    if (p->has_args()) {
      p->shift(n);
      return;
    }
}

int input_stack::nargs()
{
  for (input_iterator *p =top; p != 0; p = p->next)
    if (p->has_args())
      return p->nargs();
  return 0;
}

int input_stack::get_location(int allow_macro, const char **filenamep, int *linenop)
{
  for (input_iterator *p = top; p; p = p->next)
    if (p->get_location(allow_macro, filenamep, linenop))
      return 1;
  return 0;
}

void input_stack::backtrace()
{
  const char *f;
  int n;
  // only backtrace down to (not including) the topmost file
  for (input_iterator *p = top;
       p && !p->get_location(0, &f, &n);
       p = p->next)
    p->backtrace();
}

void input_stack::backtrace_all()
{
  for (input_iterator *p = top; p; p = p->next)
    p->backtrace();
}

int input_stack::set_location(const char *filename, int lineno)
{
  for (input_iterator *p = top; p; p = p->next)
    if (p->set_location(filename, lineno))
      return 1;
  return 0;
}

void input_stack::next_file(FILE *fp, const char *s)
{
  input_iterator **pp;
  for (pp = &top; *pp != &nil_iterator; pp = &(*pp)->next)
    if ((*pp)->next_file(fp, s))
      return;
  if (++level > limit && limit > 0)
    fatal("input stack limit exceeded");
  *pp = new file_iterator(fp, s);
  (*pp)->next = &nil_iterator;
}

void input_stack::end_file()
{
  for (input_iterator **pp = &top; *pp != &nil_iterator; pp = &(*pp)->next)
    if ((*pp)->is_file()) {
      input_iterator *tem = *pp;
      *pp = (*pp)->next;
      delete tem;
      level--;
      return;
    }
}

void input_stack::clear()
{
  int nboundaries = 0;
  while (top != &nil_iterator) {
    if (top->is_boundary())
      nboundaries++;
    input_iterator *tem = top;
    top = top->next;
    level--;
    delete tem;
  }
  // Keep while_request happy.
  for (; nboundaries > 0; --nboundaries)
    add_boundary();
}

void backtrace_request()
{
  input_stack::backtrace_all();
  fflush(stderr);
  skip_line();
}

void next_file()
{
  symbol nm = get_long_name(0);
  while (!tok.newline() && !tok.eof())
    tok.next();
  if (nm.is_null())
    input_stack::end_file();
  else {
    errno = 0;
    FILE *fp = fopen(nm.contents(), "r");
    if (!fp)
      error("can't open `%1': %2", nm.contents(), strerror(errno));
    else
      input_stack::next_file(fp, nm.contents());
  }
  tok.next();
}

void shift()
{
  int n;
  if (!has_arg() || !get_integer(&n))
    n = 1;
  input_stack::shift(n);
  skip_line();
}

static int get_char_for_escape_name()
{
  int c = get_copy(NULL);
  switch (c) {
  case EOF:
    copy_mode_error("end of input in escape name");
    return '\0';
  default:
    if (!illegal_input_char(c))
      break;
    // fall through
  case '\n':
    if (c == '\n')
      input_stack::push(make_temp_iterator("\n"));
  case ' ':
  case '\t':
  case '\001':
  case '\b':
    copy_mode_error("%1 is not allowed in an escape name",
		    input_char_description(c));
    return '\0';
  }
  return c;
}

static symbol read_two_char_escape_name()
{
  char buf[3];
  buf[0] = get_char_for_escape_name();
  if (buf[0] != '\0') {
    buf[1] = get_char_for_escape_name();
    if (buf[1] == '\0')
      buf[0] = 0;
    else
      buf[2] = 0;
  }
  return symbol(buf);
}

static symbol read_long_escape_name()
{
  int start_level = input_stack::get_level();
  char abuf[ABUF_SIZE];
  char *buf = abuf;
  int buf_size = ABUF_SIZE;
  int i = 0;
  for (;;) {
    int c = get_char_for_escape_name();
    if (c == 0) {
      if (buf != abuf)
	a_delete buf;
      return NULL_SYMBOL;
    }
    if (i + 2 > buf_size) {
      if (buf == abuf) {
	buf = new char [ABUF_SIZE*2];
	memcpy(buf, abuf, buf_size);
	buf_size = ABUF_SIZE*2;
      }
      else {
	char *old_buf = buf;
	buf = new char[buf_size*2];
	memcpy(buf, old_buf, buf_size);
	buf_size *= 2;
	a_delete old_buf;
      }
    }
    if (c == ']' && input_stack::get_level() == start_level)
      break;
    buf[i++] = c;
  }
  buf[i] = 0;
  if (buf == abuf) {
    if (i == 0) {
      copy_mode_error("empty escape name");
      return NULL_SYMBOL;
    }
    return symbol(abuf);
  }
  else {
    symbol s(buf);
    a_delete buf;
    return s;
  }
}

static symbol read_escape_name()
{
  int c = get_char_for_escape_name();
  if (c == 0)
    return NULL_SYMBOL;
  if (c == '(')
    return read_two_char_escape_name();
  if (c == '[' && !compatible_flag)
    return read_long_escape_name();
  char buf[2];
  buf[0] = c;
  buf[1] = '\0';
  return symbol(buf);
}

static symbol read_increment_and_escape_name(int *incp)
{
  int c = get_char_for_escape_name();
  switch (c) {
  case 0:
    *incp = 0;
    return NULL_SYMBOL;
  case '(':
    *incp = 0;
    return read_two_char_escape_name();
  case '+':
    *incp = 1;
    return read_escape_name();
  case '-':
    *incp = -1;
    return read_escape_name();
  case '[':
    if (!compatible_flag) {
      *incp = 0;
      return read_long_escape_name();
    }
    break;
  }
  *incp = 0;
  char buf[2];
  buf[0] = c;
  buf[1] = '\0';
  return symbol(buf);
}

static int get_copy(node **nd, int defining)
{
  for (;;) {
    int c = input_stack::get(nd);
    if (c == ESCAPE_NEWLINE) {
      if (defining)
	return c;
      do {
	c = input_stack::get(nd);
      } while (c == ESCAPE_NEWLINE);
    }
    if (c != escape_char || escape_char <= 0)
      return c;
    c = input_stack::peek();
    switch(c) {
    case 0:
      return escape_char;
    case '"':
      (void)input_stack::get(NULL);
      while ((c = input_stack::get(NULL)) != '\n' && c != EOF)
	;
      return c;
    case '#':			// Like \" but newline is ignored.
      (void)input_stack::get(NULL);
      while ((c = input_stack::get(NULL)) != '\n')
	if (c == EOF)
	  return EOF;
      break;
    case '$':
      {
	(void)input_stack::get(NULL);
	symbol s = read_escape_name();
	if (!s.is_null())
	  interpolate_arg(s);
	break;
      }
    case '*':
      {
	(void)input_stack::get(NULL);
	symbol s = read_escape_name();
	if (!s.is_null())
	  interpolate_string(s);
	break;
      }
    case 'a':
      (void)input_stack::get(NULL);
      return '\001';
    case 'e':
      (void)input_stack::get(NULL);
      return ESCAPE_e;
    case 'E':
      (void)input_stack::get(NULL);
      return ESCAPE_E;
    case 'n':
      {
	(void)input_stack::get(NULL);
	int inc;
	symbol s = read_increment_and_escape_name(&inc);
	if (!s.is_null())
	  interpolate_number_reg(s, inc);
	break;
      }
    case 'g':
      {
        (void)input_stack::get(NULL);
        symbol s = read_escape_name();
	if (!s.is_null())
	  interpolate_number_format(s);
        break;
      }
    case 't':
      (void)input_stack::get(NULL);
      return '\t';
    case 'V':
      {
        (void)input_stack::get(NULL);
	symbol s = read_escape_name();
	if (!s.is_null())
	  interpolate_environment_variable(s);
	break;
      }
    case '\n':
      (void)input_stack::get(NULL);
      if (defining)
	return ESCAPE_NEWLINE;
      break;
    case ' ':
      (void)input_stack::get(NULL);
      return ESCAPE_SPACE;
    case '|':
      (void)input_stack::get(NULL);
      return ESCAPE_BAR;
    case '^': 
      (void)input_stack::get(NULL);
      return ESCAPE_CIRCUMFLEX;
    case '{':
      (void)input_stack::get(NULL);
      return ESCAPE_LEFT_BRACE;
    case '}':
      (void)input_stack::get(NULL);
      return ESCAPE_RIGHT_BRACE;
    case '`':
      (void)input_stack::get(NULL);
      return ESCAPE_LEFT_QUOTE;
    case '\'':
      (void)input_stack::get(NULL);
      return ESCAPE_RIGHT_QUOTE;
    case '-':
      (void)input_stack::get(NULL);
      return ESCAPE_HYPHEN;
    case '_':
      (void)input_stack::get(NULL);
      return ESCAPE_UNDERSCORE;
    case 'c':
      (void)input_stack::get(NULL);
      return ESCAPE_c;
    case '!':
      (void)input_stack::get(NULL);
      return ESCAPE_BANG;
    case '?':
      (void)input_stack::get(NULL);
      return ESCAPE_QUESTION;
    case '&':
      (void)input_stack::get(NULL);
      return ESCAPE_AMPERSAND;
    case ')':
      (void)input_stack::get(NULL);
      return ESCAPE_RIGHT_PARENTHESIS;
    case '.':
      (void)input_stack::get(NULL);
      return c;			
    case '%':
      (void)input_stack::get(NULL);
      return ESCAPE_PERCENT;
    default:
      if (c == escape_char) {
	(void)input_stack::get(NULL);
	return c;
      }
      else
	return escape_char;
    }
  }
}

class non_interpreted_char_node : public node {
  unsigned char c;
public:
  non_interpreted_char_node(unsigned char);
  node *copy();
  int interpret(macro *);
  int same(node *);
  const char *type();
};

int non_interpreted_char_node::same(node *nd)
{
  return c == ((non_interpreted_char_node *)nd)->c;
}

const char *non_interpreted_char_node::type()
{
  return "non_interpreted_char_node";
}

non_interpreted_char_node::non_interpreted_char_node(unsigned char n) : c(n)
{
  assert(n != 0);
}

node *non_interpreted_char_node::copy()
{
  return new non_interpreted_char_node(c);
}

int non_interpreted_char_node::interpret(macro *mac)
{
  mac->append(c);
  return 1;
}

static void do_width();
static node *do_non_interpreted();
static node *do_special();
static void do_register();

static node *do_overstrike()
{
  token start;
  overstrike_node *on = new overstrike_node;
  start.next();
  tok.next();
  while (tok != start) {
    if (tok.newline() || tok.eof()) {
      warning(WARN_DELIM, "missing closing delimiter");
      break;
    }
    charinfo *ci = tok.get_char(1);
    if (ci) {
      node *n = curenv->make_char_node(ci);
      if (n)
	on->overstrike(n);
    }
    tok.next();
  }
  return on;
}

static node *do_bracket()
{
  token start;
  bracket_node *bn = new bracket_node;
  start.next();
  tok.next();
  while (tok != start) {
    if (tok.eof()) {
      warning(WARN_DELIM, "missing closing delimiter");
      break;
    }
    if (tok.newline()) {
      warning(WARN_DELIM, "missing closing delimiter");
      input_stack::push(make_temp_iterator("\n"));
      break;
    }
    charinfo *ci = tok.get_char(1);
    if (ci) {
      node *n = curenv->make_char_node(ci);
      if (n)
	bn->bracket(n);
    }
    tok.next();
  }
  return bn;
}

static int do_name_test()
{
  token start;
  start.next();
  int start_level = input_stack::get_level();
  int bad_char = 0;
  int some_char = 0;
  for (;;) {
    tok.next();
    if (tok.newline() || tok.eof()) {
      warning(WARN_DELIM, "missing closing delimiter");
      break;
    }
    if (tok == start
	&& (compatible_flag || input_stack::get_level() == start_level))
      break;
    if (!tok.ch())
      bad_char = 1;
    some_char = 1;
  }
  return some_char && !bad_char;
}

#if 0
static node *do_zero_width()
{
  token start;
  start.next();
  int start_level = input_stack::get_level();
  environment env(curenv);
  environment *oldenv = curenv;
  curenv = &env;
  for (;;) {
    tok.next();
    if (tok.newline() || tok.eof()) {
      error("missing closing delimiter");
      break;
    }
    if (tok == start
	&& (compatible_flag || input_stack::get_level() == start_level))
      break;
    tok.process();
  }
  curenv = oldenv;
  node *rev = env.extract_output_line();
  node *n = 0;
  while (rev) {
    node *tem = rev;
    rev = rev->next;
    tem->next = n;
    n = tem;
  }
  return new zero_width_node(n);
}  

#else

// It's undesirable for \Z to change environments, because then
// \n(.w won't work as expected.

static node *do_zero_width()
{
  node *rev = new dummy_node;
  token start;
  start.next();
  int start_level = input_stack::get_level();
  for (;;) {
    tok.next();
    if (tok.newline() || tok.eof()) {
      warning(WARN_DELIM, "missing closing delimiter");
      break;
    }
    if (tok == start
	&& (compatible_flag || input_stack::get_level() == start_level))
      break;
    if (!tok.add_to_node_list(&rev))
      error("illegal token in argument to \\Z");
  }
  node *n = 0;
  while (rev) {
    node *tem = rev;
    rev = rev->next;
    tem->next = n;
    n = tem;
  }
  return new zero_width_node(n);
}

#endif

token_node *node::get_token_node()
{
  return 0;
}

class token_node : public node {
public:
  token tk;
  token_node(const token &t);
  node *copy();
  token_node *get_token_node();
  int same(node *);
  const char *type();
};

token_node::token_node(const token &t) : tk(t)
{
}

node *token_node::copy()
{
  return new token_node(tk);
}

token_node *token_node::get_token_node()
{
  return this;
}

int token_node::same(node *nd)
{
  return tk == ((token_node *)nd)->tk;
}

const char *token_node::type()
{
  return "token_node";
}

token::token() : nd(0), type(TOKEN_EMPTY)
{
}

token::~token()
{
  delete nd;
}

token::token(const token &t)
: nm(t.nm), c(t.c), val(t.val), dim(t.dim), type(t.type)
{
  // Use two statements to work around bug in SGI C++.
  node *tem = t.nd;
  nd = tem ? tem->copy() : 0;
}

void token::operator=(const token &t)
{
  delete nd;
  nm = t.nm;
  // Use two statements to work around bug in SGI C++.
  node *tem = t.nd;
  nd = tem ? tem->copy() : 0;
  c = t.c;
  val = t.val;
  dim = t.dim;
  type = t.type;
}

void token::skip()
{
  while (space())
    next();
}

int has_arg()
{
  while (tok.space())
    tok.next();
  return !tok.newline();
}

void token::make_space()
{
  type = TOKEN_SPACE;
}

void token::make_newline()
{
  type = TOKEN_NEWLINE;
}

void token::next()
{
  if (nd) {
    delete nd;
    nd = 0;
  }
  units x;
  for (;;) {
    node *n;
    int cc = input_stack::get(&n);
    if (cc != escape_char || escape_char == 0) {
    handle_normal_char:
      switch(cc) {
      case EOF:
	type = TOKEN_EOF;
	return;
      case TRANSPARENT_FILE_REQUEST:
      case TITLE_REQUEST:
      case COPY_FILE_REQUEST:
#ifdef COLUMN
      case VJUSTIFY_REQUEST:
#endif /* COLUMN */
	type = TOKEN_REQUEST;
	c = cc;
	return;
      case BEGIN_TRAP:
	type = TOKEN_BEGIN_TRAP;
	return;
      case END_TRAP:
	type = TOKEN_END_TRAP;
	return;
      case LAST_PAGE_EJECTOR:
	seen_last_page_ejector = 1;
	// fall through
      case PAGE_EJECTOR:
	type = TOKEN_PAGE_EJECTOR;
	return;
      case ESCAPE_PERCENT:
      ESCAPE_PERCENT:
	type = TOKEN_HYPHEN_INDICATOR;
	return;
      case ESCAPE_SPACE:
      ESCAPE_SPACE:
	type = TOKEN_NODE;
	nd = new space_char_hmotion_node(curenv->get_space_width());
	return;
      case ESCAPE_e:
      ESCAPE_e:
	type = TOKEN_ESCAPE;
	return;
      case ESCAPE_E:
	goto handle_escape_char;
      case ESCAPE_BAR:
      ESCAPE_BAR:
	type = TOKEN_NODE;
	nd = new hmotion_node(curenv->get_narrow_space_width());
	return;
      case ESCAPE_CIRCUMFLEX:
      ESCAPE_CIRCUMFLEX:
	type = TOKEN_NODE;
	nd = new hmotion_node(curenv->get_half_narrow_space_width());
	return;
      case ESCAPE_NEWLINE:
	break;
      case ESCAPE_LEFT_BRACE:
      ESCAPE_LEFT_BRACE:
	type = TOKEN_LEFT_BRACE;
	return;
      case ESCAPE_RIGHT_BRACE:
      ESCAPE_RIGHT_BRACE:
	type = TOKEN_RIGHT_BRACE;
	return;
      case ESCAPE_LEFT_QUOTE:
      ESCAPE_LEFT_QUOTE:
	type = TOKEN_SPECIAL;
	nm = symbol("ga");
	return;
      case ESCAPE_RIGHT_QUOTE:
      ESCAPE_RIGHT_QUOTE:
	type = TOKEN_SPECIAL;
	nm = symbol("aa");
	return;
      case ESCAPE_HYPHEN:
      ESCAPE_HYPHEN:
	type = TOKEN_SPECIAL;
	nm = symbol("-");
	return;
      case ESCAPE_UNDERSCORE:
      ESCAPE_UNDERSCORE:
	type = TOKEN_SPECIAL;
	nm = symbol("ul");
	return;
      case ESCAPE_c:
      ESCAPE_c:
	type = TOKEN_INTERRUPT;
	return;
      case ESCAPE_BANG:
      ESCAPE_BANG:
	type = TOKEN_TRANSPARENT;
	return;
      case ESCAPE_QUESTION:
      ESCAPE_QUESTION:
	nd = do_non_interpreted();
	if (nd) {
	  type = TOKEN_NODE;
	  return;
	}
	break;
      case ESCAPE_AMPERSAND:
      ESCAPE_AMPERSAND:
	type = TOKEN_DUMMY;
	return;
      case ESCAPE_RIGHT_PARENTHESIS:
      ESCAPE_RIGHT_PARENTHESIS:
	type = TOKEN_NODE;
	nd = new transparent_dummy_node;
	return;
      case '\b':
	type = TOKEN_BACKSPACE;
	return;
      case ' ':
	type = TOKEN_SPACE;
	return;
      case '\t':
	type = TOKEN_TAB;
	return;
      case '\n':
	type = TOKEN_NEWLINE;
	return;
      case '\001':
	type = TOKEN_LEADER;
	return;
      case 0:
	{
	  assert(n != 0);
	  token_node *tn = n->get_token_node();
	  if (tn) {
	    *this = tn->tk;
	    delete tn;
	  }
	  else {
	    nd = n;
	    type = TOKEN_NODE;
	  }
	}
	return;
      default:
	type = TOKEN_CHAR;
	c = cc;
	return;
      }
    }
    else {
    handle_escape_char:
      cc = input_stack::get(NULL);
      switch(cc) {
      case '(':
	nm = read_two_char_escape_name();
	type = TOKEN_SPECIAL;
	return;
      case EOF:
	type = TOKEN_EOF;
	error("end of input after escape character");
	return;
      case '`':
	goto ESCAPE_LEFT_QUOTE;
      case '\'':
	goto ESCAPE_RIGHT_QUOTE;
      case '-':
	goto ESCAPE_HYPHEN;
      case '_':
	goto ESCAPE_UNDERSCORE;
      case '%':
	goto ESCAPE_PERCENT;
      case ' ':
	goto ESCAPE_SPACE;
      case '0':
	nd = new hmotion_node(curenv->get_digit_width());
	type = TOKEN_NODE;
	return;
      case '|':
	goto ESCAPE_BAR;
      case '^':
	goto ESCAPE_CIRCUMFLEX;
      case '/':
	type = TOKEN_ITALIC_CORRECTION;
	return;
      case ',':
	type = TOKEN_NODE;
	nd = new left_italic_corrected_node;
	return;
      case '&':
	goto ESCAPE_AMPERSAND;
      case ')':
	goto ESCAPE_RIGHT_PARENTHESIS;
      case '!':
	goto ESCAPE_BANG;
      case '?':
	goto ESCAPE_QUESTION;
      case '~':
	nd = new unbreakable_space_node(curenv->get_space_width());
	type = TOKEN_NODE;
	return;
      case '"':
	while ((cc = input_stack::get(NULL)) != '\n' && cc != EOF)
	  ;
	if (cc == '\n')
	  type = TOKEN_NEWLINE;
	else
	  type = TOKEN_EOF;
	return;
      case '#':			// Like \" but newline is ignored.
	while ((cc = input_stack::get(NULL)) != '\n')
	  if (cc == EOF) {
	    type = TOKEN_EOF;
	    return;
	  }
	break;
      case '$':
	{
	  symbol nm = read_escape_name();
	  if (!nm.is_null())
	    interpolate_arg(nm);
	  break;
	}
      case '*':
	{
	  symbol nm = read_escape_name();
	  if (!nm.is_null())
	    interpolate_string(nm);
	  break;
	}
      case 'a':
	nd = new non_interpreted_char_node('\001');
	type = TOKEN_NODE;
	return;
      case 'A':
	c = '0' + do_name_test();
	type = TOKEN_CHAR;
	return;
      case 'b':
	nd = do_bracket();
	type = TOKEN_NODE;
	return;
      case 'c':
	goto ESCAPE_c;
      case 'C':
	nm = get_delim_name();
	if (nm.is_null())
	  break;
	type = TOKEN_SPECIAL;
	return;
      case 'd':
	type = TOKEN_NODE;
	nd = new vmotion_node(curenv->get_size()/2);
	return;
      case 'D':
	nd = read_draw_node();
	if (!nd)
	  break;
	type = TOKEN_NODE;
	return;
      case 'e':
	goto ESCAPE_e;
      case 'E':
	goto handle_escape_char;
      case 'f':
	{
	  symbol s = read_escape_name();
	  if (s.is_null())
	    break;
	  const char *p;
	  for (p = s.contents(); *p != '\0'; p++)
	    if (!csdigit(*p))
	      break;
	  if (*p)
	    curenv->set_font(s);
	  else
	    curenv->set_font(atoi(s.contents()));
	  break;
	}
      case 'g':
	{
	  symbol s = read_escape_name();
	  if (!s.is_null())
	    interpolate_number_format(s);
	  break;
	}
      case 'h':
	if (!get_delim_number(&x, 'm'))
	  break;
	type = TOKEN_NODE;
	nd = new hmotion_node(x);
	return;
      case 'H':
	if (get_delim_number(&x, 'z', curenv->get_requested_point_size()))
	  curenv->set_char_height(x);
	break;
      case 'k':
	nm = read_escape_name();
	if (nm.is_null())
	  break;
	type = TOKEN_MARK_INPUT;
	return;
      case 'l':
      case 'L':
	{
	  charinfo *s = 0;
	  if (!get_line_arg(&x, (cc == 'l' ? 'm': 'v'), &s))
	    break;
	  if (s == 0)
	    s = get_charinfo(cc == 'l' ? "ru" : "br");
	  type = TOKEN_NODE;
	  node *n = curenv->make_char_node(s);
	  if (cc == 'l')
	    nd = new hline_node(x, n);
	  else
	    nd = new vline_node(x, n);
	  return;
	}
      case 'n':
	{
	  int inc;
	  symbol nm = read_increment_and_escape_name(&inc);
	  if (!nm.is_null())
	    interpolate_number_reg(nm, inc);
	  break;
	}
      case 'N':
	if (!get_delim_number(&val, 0))
	  break;
	type = TOKEN_NUMBERED_CHAR;
	return;
      case 'o':
	nd = do_overstrike();
	type = TOKEN_NODE;
	return;
      case 'p':
	type = TOKEN_SPREAD;
	return;
      case 'r':
	type = TOKEN_NODE;
	nd = new vmotion_node(-curenv->get_size());
	return;
      case 'R':
	do_register();
	break;
      case 's':
	if (read_size(&x))
	  curenv->set_size(x);
	break;
      case 'S':
	if (get_delim_number(&x, 0))
	  curenv->set_char_slant(x);
	break;
      case 't':
	type = TOKEN_NODE;
	nd = new non_interpreted_char_node('\t');
	return;
      case 'u':
	type = TOKEN_NODE;
	nd = new vmotion_node(-curenv->get_size()/2);
	return;
      case 'v':
	if (!get_delim_number(&x, 'v'))
	  break;
	type = TOKEN_NODE;
	nd = new vmotion_node(x);
	return;
      case 'V':
        {
	  symbol nm = read_escape_name();
	  if (!nm.is_null())
	    interpolate_environment_variable(nm);
	  break;
	}
      case 'w':
	do_width();
	break;
      case 'x':
	if (!get_delim_number(&x, 'v'))
	  break;
	type = TOKEN_NODE;
	nd = new extra_size_node(x);
	return;
      case 'X':
	nd = do_special();
	if (!nd)
	  break;
	type = TOKEN_NODE;
	return;
      case 'Y':
	{
	  symbol s = read_escape_name();
	  if (s.is_null())
	    break;
	  request_or_macro *p = lookup_request(s);
	  macro *m = p->to_macro();
	  if (!m) {
	    error("can't transparently throughput a request");
	    break;
	  }
	  nd = new special_node(*m);
	  type = TOKEN_NODE;
	  return;
	}
      case 'z':
	{
	  next();
          if (type == TOKEN_NODE)
            nd = new zero_width_node(nd);
          else {
  	    charinfo *ci = get_char(1);
	    if (ci == 0)
	      break;
	    node *gn = curenv->make_char_node(ci);
	    if (gn == 0)
	      break;
	    nd = new zero_width_node(gn);
	    type = TOKEN_NODE;
          }
	  return;
	}
      case 'Z':
	nd = do_zero_width();
	if (nd == 0)
	  break;
	type = TOKEN_NODE;
	return;
      case '{':
	goto ESCAPE_LEFT_BRACE;
      case '}':
	goto ESCAPE_RIGHT_BRACE;
      case '\n':
	break;
      case '[':
	if (!compatible_flag) {
	  nm = read_long_escape_name();
	  if (nm.is_null())
	    break;
	  type = TOKEN_SPECIAL;
	  return;
	}
	goto handle_normal_char;
      default:
	if (cc != escape_char && cc != '.')
	  warning(WARN_ESCAPE, "escape character ignored before %1",
		  input_char_description(cc));
	goto handle_normal_char;
      }
    }
  }
}

int token::operator==(const token &t)
{
  if (type != t.type)
    return 0;
  switch(type) {
  case TOKEN_CHAR:
    return c == t.c;
  case TOKEN_SPECIAL:
    return nm == t.nm;
  case TOKEN_NUMBERED_CHAR:
    return val == t.val;
  default:
    return 1;
  }
}

int token::operator!=(const token &t)
{
  return !(*this == t);
}

// is token a suitable delimiter (like ')?

int token::delimiter(int err)
{
  switch(type) {
  case TOKEN_CHAR:
    switch(c) {
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
    case '+':
    case '-':
    case '/':
    case '*':
    case '%':
    case '<':
    case '>':
    case '=':
    case '&':
    case ':':
    case '(':
    case ')':
    case '.':
      if (err)
	error("cannot use character `%1' as a starting delimiter", char(c));
      return 0;
    default:
      return 1;
    }
  case TOKEN_NODE:
  case TOKEN_SPACE:
  case TOKEN_TAB:
  case TOKEN_NEWLINE:
    if (err)
      error("cannot use %1 as a starting delimiter", description());
    return 0;
  default:
    return 1;
  }
}

const char *token::description()
{
  static char buf[4];
  switch (type) {
  case TOKEN_BACKSPACE:
    return "a backspace character";
  case TOKEN_CHAR:
    buf[0] = '`';
    buf[1] = c;
    buf[2] = '\'';
    buf[3] = '\0';
    return buf;
  case TOKEN_DUMMY:
    return "`\\&'";
  case TOKEN_ESCAPE:
    return "`\\e'";
  case TOKEN_HYPHEN_INDICATOR:
    return "`\\%'";
  case TOKEN_INTERRUPT:
    return "`\\c'";
  case TOKEN_ITALIC_CORRECTION:
    return "`\\/'";
  case TOKEN_LEADER:
    return "a leader character";
  case TOKEN_LEFT_BRACE:
    return "`\\{'";
  case TOKEN_MARK_INPUT:
    return "`\\k'";
  case TOKEN_NEWLINE:
    return "newline";
  case TOKEN_NODE:
    return "a node";
  case TOKEN_NUMBERED_CHAR:
    return "`\\N'";
  case TOKEN_RIGHT_BRACE:
    return "`\\}'";
  case TOKEN_SPACE:
    return "a space";
  case TOKEN_SPECIAL:
    return "a special character";
  case TOKEN_SPREAD:
    return "`\\p'";
  case TOKEN_TAB:
    return "a tab character";
  case TOKEN_TRANSPARENT:
    return "`\\!'";
  case TOKEN_EOF:
    return "end of input";
  default:
    break;
  }
  return "a magic token";
}

void skip_line()
{
  while (!tok.newline())
    if (tok.eof())
      return;
    else
      tok.next();
  tok.next();
}

void compatible()
{
  int n;
  if (has_arg() && get_integer(&n))
    compatible_flag = n != 0;
  else
    compatible_flag = 1;
  skip_line();
}

static void empty_name_warning(int required)
{
  if (tok.newline() || tok.eof()) {
    if (required)
      warning(WARN_MISSING, "missing name");
  }
  else if (tok.right_brace() || tok.tab()) {
    const char *start = tok.description();
    do {
      tok.next();
    } while (tok.space() || tok.right_brace() || tok.tab());
    if (!tok.newline() && !tok.eof())
      error("%1 is not allowed before an argument", start);
    else if (required)
      warning(WARN_MISSING, "missing name");
  }
  else if (required)
    error("name expected (got %1)", tok.description());
  else
    error("name expected (got %1): treated as missing", tok.description());
}

static void non_empty_name_warning()
{
  if (!tok.newline() && !tok.eof() && !tok.space() && !tok.tab()
      && !tok.right_brace()
      // We don't want to give a warning for .el\{
      && !tok.left_brace())
    error("%1 is not allowed in a name", tok.description());
}

symbol get_name(int required)
{
  if (compatible_flag) {
    char buf[3];
    tok.skip();
    if ((buf[0] = tok.ch()) != 0) {
      tok.next();
      if ((buf[1] = tok.ch()) != 0) {
	buf[2] = 0;
	tok.make_space();
      }
      else
	non_empty_name_warning();
      return symbol(buf);
    }
    else {
      empty_name_warning(required);
      return NULL_SYMBOL;
    }
  }
  else 
    return get_long_name(required);
}

symbol get_long_name(int required)
{
  while (tok.space())
    tok.next();
  char abuf[ABUF_SIZE];
  char *buf = abuf;
  int buf_size = ABUF_SIZE;
  int i = 0;
  for (;;) {
    if (i + 1 > buf_size) {
      if (buf == abuf) {
	buf = new char [ABUF_SIZE*2];
	memcpy(buf, abuf, buf_size);
	buf_size = ABUF_SIZE*2;
      }
      else {
	char *old_buf = buf;
	buf = new char[buf_size*2];
	memcpy(buf, old_buf, buf_size);
	buf_size *= 2;
	a_delete old_buf;
      }
    }
    if ((buf[i] = tok.ch()) == 0)
      break;
    i++;
    tok.next();
  }
  if (i == 0) {
    empty_name_warning(required);
    return NULL_SYMBOL;
  }
  non_empty_name_warning();
  if (buf == abuf)
    return symbol(buf);
  else {
    symbol s(buf);
    a_delete buf;
    return s;
  }
}

void exit_troff()
{
  exit_started = 1;
  topdiv->set_last_page();
  if (!end_macro_name.is_null()) {
    spring_trap(end_macro_name);
    tok.next();
    process_input_stack();
  }
  curenv->final_break();
  tok.next();
  process_input_stack();
  end_diversions();
  done_end_macro = 1;
  topdiv->set_ejecting();
  static unsigned char buf[2] = { LAST_PAGE_EJECTOR, '\0' };
  input_stack::push(make_temp_iterator((char *)buf));
  topdiv->space(topdiv->get_page_length(), 1);
  tok.next();
  process_input_stack();
  seen_last_page_ejector = 1;	// should be set already
  topdiv->set_ejecting();
  push_page_ejector();
  topdiv->space(topdiv->get_page_length(), 1);
  tok.next();
  process_input_stack();
  // This will only happen if a trap-invoked macro starts a diversion,
  // or if vertical position traps have been disabled.
  cleanup_and_exit(0);
}

// This implements .ex.  The input stack must be cleared before calling
// exit_troff().

void exit_request()
{
  input_stack::clear();
  if (exit_started)
    tok.next();
  else
    exit_troff();
}

void end_macro()
{
  end_macro_name = get_name();
  skip_line();
}

void blank_line_macro()
{
  blank_line_macro_name = get_name();
  skip_line();
}

static void trapping_blank_line()
{
  if (!blank_line_macro_name.is_null())
    spring_trap(blank_line_macro_name);
  else
    blank_line();
}

void do_request()
{
  int saved_compatible_flag = compatible_flag;
  compatible_flag = 0;
  symbol nm = get_name();
  if (nm.is_null())
    skip_line();
  else
    interpolate_macro(nm);
  compatible_flag = saved_compatible_flag;
}

inline int possibly_handle_first_page_transition()
{
  if (topdiv->before_first_page && curdiv == topdiv && !curenv->is_dummy()) {
    handle_first_page_transition();
    return 1;
  }
  else
    return 0;
}

static int transparent_translate(int cc)
{
  if (!illegal_input_char(cc)) {
    charinfo *ci = charset_table[cc];
    switch (ci->get_special_translation(1)) {
    case charinfo::TRANSLATE_SPACE:
      return ' ';
    case charinfo::TRANSLATE_DUMMY:
      return ESCAPE_AMPERSAND;
    case charinfo::TRANSLATE_HYPHEN_INDICATOR:
      return ESCAPE_PERCENT;
    }
    // This is realy ugly.
    ci = ci->get_translation(1);
    if (ci) {
      int c = ci->get_ascii_code();
      if (c != '\0')
	return c;
      error("can't translate %1 to special character `%2'"
	    " in transparent throughput",
	    input_char_description(cc),
	    ci->nm.contents());
    }
  }
  return cc;
}

class int_stack {
  struct int_stack_element {
    int n;
    int_stack_element *next;
  } *top;
public:
  int_stack();
  ~int_stack();
  void push(int);
  int is_empty();
  int pop();
};

int_stack::int_stack()
{
  top = 0;
}

int_stack::~int_stack()
{
  while (top != 0) {
    int_stack_element *temp = top;
    top = top->next;
    delete temp;
  }
  
}

int int_stack::is_empty()
{
  return top == 0;
}

void int_stack::push(int n)
{
  int_stack_element *p = new int_stack_element;
  p->next = top;
  p->n = n;
  top = p;
}


int int_stack::pop()
{
  assert(top != 0);
  int_stack_element *p = top;
  top = top->next;
  int n = p->n;
  delete p;
  return n;
}

int node::reread(int *)
{
  return 0;
}

int diverted_space_node::reread(int *bolp)
{
  if (curenv->get_fill())
    trapping_blank_line();
  else
    curdiv->space(n);
  *bolp = 1;
  return 1;
}

int diverted_copy_file_node::reread(int *bolp)
{
  curdiv->copy_file(filename.contents());
  *bolp = 1;
  return 1;
}

void process_input_stack()
{
  int_stack trap_bol_stack;
  int bol = 1;
  for (;;) {
    int suppress_next = 0;
    switch (tok.type) {
    case token::TOKEN_CHAR:
      {
	unsigned char ch = tok.c;
	if (bol && 
	    (ch == curenv->control_char
	     || ch == curenv->no_break_control_char)) {
	  break_flag = ch == curenv->control_char;
	  // skip tabs as well as spaces here
	  do {
	    tok.next();
	  } while (tok.white_space());
	  symbol nm = get_name();
	  if (nm.is_null())
	    skip_line();
	  else
	    interpolate_macro(nm);
	  suppress_next = 1;
	}
	else {
	  if (possibly_handle_first_page_transition())
	    ;
	  else {
	    for (;;) {
	      curenv->add_char(charset_table[ch]);
	      tok.next();
	      if (tok.type != token::TOKEN_CHAR)
		break;
	      ch = tok.c;
	    }
	    suppress_next = 1;
	    bol = 0;
	  }
	}
	break;
      }
    case token::TOKEN_TRANSPARENT:
      {
	if (bol) {
	  if (possibly_handle_first_page_transition())
	    ;
	  else {
	    int cc;
	    do {
	      node *n;
	      cc = get_copy(&n);
	      if (cc != EOF)
		if (cc != '\0')
		  curdiv->transparent_output(transparent_translate(cc));
		else
		  curdiv->transparent_output(n);
	    } while (cc != '\n' && cc != EOF);
	    if (cc == EOF)
	      curdiv->transparent_output('\n');
	  }
	}
	break;
      }
    case token::TOKEN_NEWLINE:
      {
	if (bol && !curenv->get_prev_line_interrupted())
	  trapping_blank_line();
	else {
	  curenv->newline();
	  bol = 1;
	}
	break;
      }
    case token::TOKEN_REQUEST:
      {
	int request_code = tok.c;
	tok.next();
	switch (request_code) {
	case TITLE_REQUEST:
	  title();
	  break;
	case COPY_FILE_REQUEST:
	  copy_file();
	  break;
	case TRANSPARENT_FILE_REQUEST:
	  transparent_file();
	  break;
#ifdef COLUMN
	case VJUSTIFY_REQUEST:
	  vjustify();
	  break;
#endif /* COLUMN */
	default:
	  assert(0);
	  break;
	}
	suppress_next = 1;
	break;
      }
    case token::TOKEN_SPACE:
      {
	if (possibly_handle_first_page_transition())
	  ;
	else if (bol && !curenv->get_prev_line_interrupted()) {
	  int nspaces = 0;
	  do {
	    nspaces += tok.nspaces();
	    tok.next();
	  } while (tok.space());
	  if (tok.newline())
	    trapping_blank_line();
	  else {
	    push_token(tok);
	    curenv->do_break();
	    curenv->add_node(new hmotion_node(curenv->get_space_width()*nspaces));
	    bol = 0;
	  }
	}
	else {
	  curenv->space();
	  bol = 0;
	}
	break;
      }
    case token::TOKEN_EOF:
      return;
    case token::TOKEN_NODE:
      {
	if (possibly_handle_first_page_transition())
	  ;
	else if (tok.nd->reread(&bol)) {
	  delete tok.nd;
	  tok.nd = 0;
	}
	else {
	  curenv->add_node(tok.nd);
	  tok.nd = 0;
	  bol = 0;
	}
	break;
      }
    case token::TOKEN_PAGE_EJECTOR:
      {
	continue_page_eject();
	// I think we just want to preserve bol.
	// bol = 1;
	break;
      }
    case token::TOKEN_BEGIN_TRAP:
      {
	trap_bol_stack.push(bol);
	bol = 1;
	break;
      }
    case token::TOKEN_END_TRAP:
      {
	if (trap_bol_stack.is_empty())
	  error("spurious end trap token detected!");
	else
	  bol = trap_bol_stack.pop();

	/* I'm not totally happy about this.  But I can't think of any other
	  way to do it.  Doing an output_pending_lines() whenever a
	  TOKEN_END_TRAP is detected doesn't work: for example,

          .wh -1i x
          .de x
          'bp
	  ..
	  .wh -.5i y
	  .de y
	  .tl ''-%-''
	  ..
	  .br
	  .ll .5i
	  .sp |\n(.pu-1i-.5v
	  a\%very\%very\%long\%word

          will print all but the first lines from the word immediately
          after the footer, rather than on the next page. */

	if (trap_bol_stack.is_empty())
	  curenv->output_pending_lines();
	break;
      }
    default:
      {
	bol = 0;
	tok.process();
	break;
      }
    }
    if (!suppress_next)
      tok.next();
    trap_sprung_flag = 0;
  }
}

#ifdef WIDOW_CONTROL

void flush_pending_lines()
{
  while (!tok.newline() && !tok.eof())
    tok.next();
  curenv->output_pending_lines();
  tok.next();
}

#endif /* WIDOW_CONTROL */

request_or_macro::request_or_macro()
{
}

macro *request_or_macro::to_macro()
{
  return 0;
}

request::request(REQUEST_FUNCP pp) : p(pp)
{
}

void request::invoke(symbol)
{
  (*p)();
}

struct char_block {
  enum { SIZE = 128 };
  unsigned char s[SIZE];
  char_block *next;
  char_block();
};

char_block::char_block()
: next(0)
{
}

class char_list {
public:
  char_list();
  ~char_list();
  void append(unsigned char);
  int length();
private:
  unsigned char *ptr;
  int len;
  char_block *head;
  char_block *tail;
  friend class macro_header;
  friend class string_iterator;
};

char_list::char_list()
: head(0), tail(0), ptr(0), len(0)
{
}

char_list::~char_list()
{
  while (head != 0) {
    char_block *tem = head;
    head = head->next;
    delete tem;
  }
}

int char_list::length()
{
  return len;
}

void char_list::append(unsigned char c)
{
  if (tail == 0) {
    head = tail = new char_block;
    ptr = tail->s;
  }
  else {
    if (ptr >= tail->s + char_block::SIZE) {
      tail->next = new char_block;
      tail = tail->next;
      ptr = tail->s;
    }
  }
  *ptr++ = c;
  len++;
}

class node_list {
  node *head;
  node *tail;
public:
  node_list();
  ~node_list();
  void append(node *);
  int length();
  node *extract();

  friend class macro_header;
  friend class string_iterator;
};

void node_list::append(node *n)
{
  if (head == 0) {
    n->next = 0;
    head = tail = n;
  }
  else {
    n->next = 0;
    tail = tail->next = n;
  }
}

int node_list::length()
{
  int total = 0;
  for (node *n = head; n != 0; n = n->next)
    ++total;
  return total;
}

node_list::node_list()
{
  head = tail = 0;
}

node *node_list::extract()
{
  node *temp = head;
  head = tail = 0;
  return temp;
}


node_list::~node_list()
{
  delete_node_list(head);
}

struct macro_header {
  int count;
  char_list cl;
  node_list nl;
  macro_header() { count = 1; }
  macro_header *copy(int);
};


macro::~macro() 
{ 
  if (p != 0 && --(p->count) <= 0) 
    delete p;
}

macro::macro()
{
  if (!input_stack::get_location(1, &filename, &lineno)) {
    filename = 0;
    lineno = 0;
  }
  length = 0;
  p = 0;
}

macro::macro(const macro &m) 
: filename(m.filename), lineno(m.lineno), p(m.p), length(m.length)
{ 
  if (p != 0)
    p->count++;
}

macro &macro::operator=(const macro &m)
{
  // don't assign object
  if (m.p != 0)
    m.p->count++;
  if (p != 0 && --(p->count) <= 0) 
    delete p; 
  p = m.p; 
  filename = m.filename;
  lineno = m.lineno;
  length = m.length;
  return *this;
}

void macro::append(unsigned char c)
{
  assert(c != 0);
  if (p == 0)
    p = new macro_header;
  if (p->cl.length() != length) {
    macro_header *tem = p->copy(length);
    if (--(p->count) <= 0)
      delete p;
    p = tem;
  }
  p->cl.append(c);
  ++length;
}

void macro::append(node *n)
{
  assert(n != 0);
  if (p == 0)
    p = new macro_header;
  if (p->cl.length() != length) {
    macro_header *tem = p->copy(length);
    if (--(p->count) <= 0)
      delete p;
    p = tem;
  }
  p->cl.append(0);
  p->nl.append(n);
  ++length;
}

void macro::print_size()
{
  errprint("%1", length);
}

// make a copy of the first n bytes

macro_header *macro_header::copy(int n)
{
  macro_header *p = new macro_header;
  char_block *bp = cl.head;
  unsigned char *ptr = bp->s;
  node *nd = nl.head;
  while (--n >= 0) {
    if (ptr >= bp->s + char_block::SIZE) {
      bp = bp->next;
      ptr = bp->s;
    }
    int c = *ptr++;
    p->cl.append(c);
    if (c == 0) {
      p->nl.append(nd->copy());
      nd = nd->next;
    }
  }
  return p;
}

void print_macros()
{
  object_dictionary_iterator iter(request_dictionary);
  request_or_macro *rm;
  symbol s;
  while (iter.get(&s, (object **)&rm)) {
    assert(!s.is_null());
    macro *m = rm->to_macro();
    if (m) {
      errprint("%1\t", s.contents());
      m->print_size();
      errprint("\n");
    }
  }
  fflush(stderr);
  skip_line();
}

class string_iterator : public input_iterator {
  macro mac;
  const char *how_invoked;
  int newline_flag;
  int lineno;
  char_block *bp;
  int count;			// of characters remaining
  node *nd;
protected:
  symbol nm;
  string_iterator();
public:
  string_iterator(const macro &m, const char *p = 0, symbol s = NULL_SYMBOL);
  int fill(node **);
  int peek();
  int get_location(int, const char **, int *);
  void backtrace();
};

string_iterator::string_iterator(const macro &m, const char *p, symbol s) 
: lineno(1), mac(m), newline_flag(0), how_invoked(p), nm(s)
{
  count = mac.length;
  if (count != 0) {
    bp = mac.p->cl.head;
    nd = mac.p->nl.head;
    ptr = eptr = bp->s;
  }
  else {
    bp = 0;
    nd = 0;
    ptr = eptr = 0;
  }
}

string_iterator::string_iterator()
{
  bp = 0;
  nd = 0;
  ptr = eptr = 0;
  newline_flag = 0;
  how_invoked = 0;
  lineno = 1;
  count = 0;
}

int string_iterator::fill(node **np)
{
  if (newline_flag)
    lineno++;
  newline_flag = 0;
  if (count <= 0)
    return EOF;
  const unsigned char *p = eptr;
  if (p >= bp->s + char_block::SIZE) {
    bp = bp->next;
    p = bp->s;
  }
  if (*p == '\0') {
    if (np)
      *np = nd->copy();
    nd = nd->next;
    eptr = ptr = p + 1;
    count--;
    return 0;
  }
  const unsigned char *e = bp->s + char_block::SIZE;
  if (e - p > count)
    e = p + count;
  ptr = p;
  while (p < e) {
    unsigned char c = *p;
    if (c == '\n' || c == ESCAPE_NEWLINE) {
      newline_flag = 1;
      p++;
      break;
    }
    if (c == '\0')
      break;
    p++;
  }
  eptr = p;
  count -= p - ptr;
  return *ptr++;
}

int string_iterator::peek()
{
  if (count <= 0)
    return EOF;
  const unsigned char *p = eptr;
  if (count <= 0)
    return EOF;
  if (p >= bp->s + char_block::SIZE) {
    p = bp->next->s;
  }
  return *p;
}

int string_iterator::get_location(int allow_macro,
				  const char **filep, int *linep)
{
  if (!allow_macro)
    return 0;
  if (mac.filename == 0)
    return 0;
  *filep = mac.filename;
  *linep = mac.lineno + lineno - 1;
  return 1;
}

void string_iterator::backtrace()
{
  if (mac.filename) {
    errprint("%1:%2: backtrace", mac.filename, mac.lineno + lineno - 1);
    if (how_invoked) {
      if (!nm.is_null())
	errprint(": %1 `%2'\n", how_invoked, nm.contents());
      else
	errprint(": %1\n", how_invoked);
    }
    else
      errprint("\n");
  }
}

class temp_iterator : public input_iterator {
  unsigned char *base;
  temp_iterator(const char *, int len);
public:
  ~temp_iterator();
  friend input_iterator *make_temp_iterator(const char *);
};

#ifdef __GNUG__
inline
#endif
temp_iterator::temp_iterator(const char *s, int len)
{
  base = new unsigned char[len];
  memcpy(base, s, len);
  ptr = base;
  eptr = base + len;
}

temp_iterator::~temp_iterator()
{
  a_delete base;
}

class small_temp_iterator : public input_iterator {
private:
  small_temp_iterator(const char *, int);
  ~small_temp_iterator();
  enum { BLOCK = 16 };
  static small_temp_iterator *free_list;
  void *operator new(size_t);
  void operator delete(void *);
  enum { SIZE = 12 };
  unsigned char buf[SIZE];
  friend input_iterator *make_temp_iterator(const char *);
};

small_temp_iterator *small_temp_iterator::free_list = 0;

void *small_temp_iterator::operator new(size_t n)
{
  assert(n == sizeof(small_temp_iterator));
  if (!free_list) {
    free_list = (small_temp_iterator *)new char[sizeof(small_temp_iterator)*BLOCK];
    for (int i = 0; i < BLOCK - 1; i++)
      free_list[i].next = free_list + i + 1;
    free_list[BLOCK-1].next = 0;
  }
  small_temp_iterator *p = free_list;
  free_list = (small_temp_iterator *)(free_list->next);
  p->next = 0;
  return p;
}

#ifdef __GNUG__
inline
#endif
void small_temp_iterator::operator delete(void *p)
{
  if (p) {
    ((small_temp_iterator *)p)->next = free_list;
    free_list = (small_temp_iterator *)p;
  }
}

small_temp_iterator::~small_temp_iterator()
{
}


#ifdef __GNUG__
inline
#endif
small_temp_iterator::small_temp_iterator(const char *s, int len)
{
  for (int i = 0; i < len; i++)
    buf[i] = s[i];
  ptr = buf;
  eptr = buf + len;
}

input_iterator *make_temp_iterator(const char *s)
{
  if (s == 0)
    return new small_temp_iterator(s, 0);
  else {
    int n = strlen(s);
    if (n <= small_temp_iterator::SIZE)
      return new small_temp_iterator(s, n);
    else
      return new temp_iterator(s, n);
  }
}

// this is used when macros are interpolated using the .macro_name notation

struct arg_list {
  macro mac;
  arg_list *next;
  arg_list(const macro &);
  ~arg_list();
};

arg_list::arg_list(const macro &m) : mac(m), next(0)
{
}

arg_list::~arg_list()
{
}

class macro_iterator : public string_iterator {
  arg_list *args;
  int argc;
public:
  macro_iterator(symbol, macro &, const char *how_invoked = "macro");
  macro_iterator();
  ~macro_iterator();
  int has_args() { return 1; }
  input_iterator *get_arg(int i);
  int nargs() { return argc; }
  void add_arg(const macro &m);
  void shift(int n);
};

input_iterator *macro_iterator::get_arg(int i)
{ 
  if (i == 0)
    return make_temp_iterator(nm.contents());
  if (i > 0 && i <= argc) {
    arg_list *p = args;
    for (int j = 1; j < i; j++) {
      assert(p != 0);
      p = p->next;
    }
    return new string_iterator(p->mac);
  }
  else
    return 0;
}

void macro_iterator::add_arg(const macro &m)
{
  arg_list **p;
  for (p = &args; *p; p = &((*p)->next))
    ;
  *p = new arg_list(m);
  ++argc;
}

void macro_iterator::shift(int n)
{
  while (n > 0 && argc > 0) {
    arg_list *tem = args;
    args = args->next;
    delete tem;
    --argc;
    --n;
  }
}

// This gets used by eg .if '\?xxx\?''.

int operator==(const macro &m1, const macro &m2)
{
  if (m1.length != m2.length)
    return 0;
  string_iterator iter1(m1);
  string_iterator iter2(m2);
  int n = m1.length;
  while (--n >= 0) {
    node *nd1 = 0;
    int c1 = iter1.get(&nd1);
    assert(c1 != EOF);
    node *nd2 = 0;
    int c2 = iter2.get(&nd2);
    assert(c2 != EOF);
    if (c1 != c2) {
      if (c1 == 0)
	delete nd1;
      else if (c2 == 0)
	delete nd2;
      return 0;
    }
    if (c1 == 0) {
      assert(nd1 != 0);
      assert(nd2 != 0);
      int are_same = nd1->type() == nd2->type() && nd1->same(nd2);
      delete nd1;
      delete nd2;
      if (!are_same)
	return 0;
    }
  }
  return 1;
}

static void interpolate_macro(symbol nm)
{
  request_or_macro *p = (request_or_macro *)request_dictionary.lookup(nm);
  if (p == 0) {
    int warned = 0;
    const char *s = nm.contents();
    if (strlen(s) > 2) {
      request_or_macro *r;
      char buf[3];
      buf[0] = s[0];
      buf[1] = s[1];
      buf[2] = '\0';
      r = (request_or_macro *)request_dictionary.lookup(symbol(buf));
      if (r) {
	macro *m = r->to_macro();
	if (!m || !m->empty())
	  warned = warning(WARN_SPACE,
			   "`%1' not defined (probable missing space after `%2')",
			   nm.contents(), buf);
      }
    }
    if (!warned) {
      warning(WARN_MAC, "`%1' not defined", nm.contents());
      p = new macro;
      request_dictionary.define(nm, p);
    }
  }
  if (p)
    p->invoke(nm);
  else {
    skip_line();
    return;
  }
}

static void decode_args(macro_iterator *mi)
{
  if (!tok.newline() && !tok.eof()) {
    node *n;
    int c = get_copy(&n);
    for (;;)  {
      while (c == ' ')
	c = get_copy(&n);
      if (c == '\n' || c == EOF)
	break;
      macro arg;
      int quote_input_level = 0;
      int done_tab_warning = 0;
      if (c == '\"') {
	quote_input_level = input_stack::get_level();
	c = get_copy(&n);
      }
      while (c != EOF && c != '\n' && !(c == ' ' && quote_input_level == 0)) {
	if (quote_input_level > 0 && c == '\"'
	    && (compatible_flag
		|| input_stack::get_level() == quote_input_level)) {
	  c = get_copy(&n);
	  if (c == '"') {
	    arg.append(c);
	    c = get_copy(&n);
	  }
	  else
	    break;
	}
	else {
	  if (c == 0)
	    arg.append(n);
	  else {
	    if (c == '\t' && quote_input_level == 0 && !done_tab_warning) {
	      warning(WARN_TAB, "tab character in unquoted macro argument");
	      done_tab_warning = 1;
	    }
	    arg.append(c);
	  }
	  c = get_copy(&n);
	}
      }
      mi->add_arg(arg);
    }
  }
}

void macro::invoke(symbol nm)
{
  macro_iterator *mi = new macro_iterator(nm, *this);
  decode_args(mi);
  input_stack::push(mi);
  tok.next();
}

macro *macro::to_macro()
{
  return this;
}

int macro::empty()
{
  return length == 0;
}

macro_iterator::macro_iterator(symbol s, macro &m, const char *how_invoked)
: string_iterator(m, how_invoked, s), args(0), argc(0)
{
}

macro_iterator::macro_iterator() : args(0), argc(0)
{
}

macro_iterator::~macro_iterator()
{
  while (args != 0) {
    arg_list *tem = args;
    args = args->next;
    delete tem;
  }
}

int trap_sprung_flag = 0;
int postpone_traps_flag = 0;
symbol postponed_trap;

void spring_trap(symbol nm)
{
  assert(!nm.is_null());
  trap_sprung_flag = 1;
  if (postpone_traps_flag) {
    postponed_trap = nm;
    return;
  }
  static char buf[2] = { BEGIN_TRAP, 0 };
  static char buf2[2] = { END_TRAP, '\0' };
  input_stack::push(make_temp_iterator(buf2));
  request_or_macro *p = lookup_request(nm);
  macro *m = p->to_macro();
  if (m)
    input_stack::push(new macro_iterator(nm, *m, "trap-invoked macro"));
  else
    error("you can't invoke a request with a trap");
  input_stack::push(make_temp_iterator(buf));
}

void postpone_traps()
{
  postpone_traps_flag = 1;
}

int unpostpone_traps()
{
  postpone_traps_flag = 0;
  if (!postponed_trap.is_null()) {
    spring_trap(postponed_trap);
    postponed_trap = NULL_SYMBOL;
    return 1;
  }
  else
    return 0;
}

void read_request()
{
  macro_iterator *mi = new macro_iterator;
  int reading_from_terminal = isatty(fileno(stdin));
  int had_prompt = 0;
  if (!tok.newline() && !tok.eof()) {
    int c = get_copy(NULL);
    while (c == ' ')
      c = get_copy(NULL);
    while (c != EOF && c != '\n' && c != ' ') {
      if (!illegal_input_char(c)) {
	if (reading_from_terminal)
	  fputc(c, stderr);
	had_prompt = 1;
      }
      c = get_copy(NULL);
    }
    if (c == ' ') {
      tok.make_space();
      decode_args(mi);
    }
  }
  if (reading_from_terminal) {
    fputc(had_prompt ? ':' : '\007', stderr);
    fflush(stderr);
  }
  input_stack::push(mi);
  macro mac;
  int nl = 0;
  int c;
  while ((c = getchar()) != EOF) {
    if (illegal_input_char(c))
      warning(WARN_INPUT, "illegal input character code %1", int(c));
    else {
      if (c == '\n') {
	if (nl)
	  break;
	else
	  nl = 1;
      }
      else
	nl = 0;
      mac.append(c);
    }
  }
  if (reading_from_terminal)
    clearerr(stdin);
  input_stack::push(new string_iterator(mac));
  tok.next();
}

void do_define_string(int append)
{
  symbol nm;
  node *n;
  int c;
  nm = get_name(1);
  if (nm.is_null()) {
    skip_line();
    return;
  }
  if (tok.newline())
    c = '\n';
  else if (tok.tab())
    c = '\t';
  else if (!tok.space()) {
    error("bad string definition");
    skip_line();
    return;
  }
  else
    c = get_copy(&n);
  while (c == ' ')
    c = get_copy(&n);
  if (c == '"')
    c = get_copy(&n);
  macro mac;
  request_or_macro *rm = (request_or_macro *)request_dictionary.lookup(nm);
  macro *mm = rm ? rm->to_macro() : 0;
  if (append && mm)
    mac = *mm;
  while (c != '\n' && c != EOF) {
    if (c == 0)
      mac.append(n);
    else
      mac.append((unsigned char)c);
    c = get_copy(&n);
  }
  if (!mm) {
    mm = new macro;
    request_dictionary.define(nm, mm);
  }
  *mm = mac;
  tok.next();
}

void define_string()
{
  do_define_string(0);
}

void append_string()
{
  do_define_string(1);
}

void define_character()
{
  node *n;
  int c;
  tok.skip();
  charinfo *ci = tok.get_char(1);
  if (ci == 0) {
    skip_line();
    return;
  }
  tok.next();
  if (tok.newline())
    c = '\n';
  else if (tok.tab())
    c = '\t';
  else if (!tok.space()) {
    error("bad character definition");
    skip_line();
    return;
  }
  else
    c = get_copy(&n);
  while (c == ' ' || c == '\t')
    c = get_copy(&n);
  if (c == '"')
    c = get_copy(&n);
  macro *m = new macro;
  while (c != '\n' && c != EOF) {
    if (c == 0)
      m->append(n);
    else
      m->append((unsigned char)c);
    c = get_copy(&n);
  }
  m = ci->set_macro(m);
  if (m)
    delete m;
  tok.next();
}


static void remove_character()
{
  tok.skip();
  while (!tok.newline() && !tok.eof()) {
    if (!tok.space() && !tok.tab()) {
      charinfo *ci = tok.get_char(1);
      if (!ci)
	break;
      macro *m = ci->set_macro(0);
      if (m)
	delete m;
    }
    tok.next();
  }
  skip_line();
}
    
static void interpolate_string(symbol nm)
{
  request_or_macro *p = lookup_request(nm);
  macro *m = p->to_macro();
  if (!m)
    error("you can only invoke a string using \\*");
  else {
    string_iterator *si = new string_iterator(*m, "string", nm);
    input_stack::push(si);
  }
}

/* This class is used for the implementation of \$@.  It is used for
each of the closing double quotes.  It artificially increases the
input level by 2, so that the closing double quote will appear to have
the same input level as the opening quote. */

class end_quote_iterator : public input_iterator {
  unsigned char buf[1];
public:
  end_quote_iterator();
  ~end_quote_iterator() { }
  int internal_level() { return 2; }
};

end_quote_iterator::end_quote_iterator()
{
  buf[0] = '"';
  ptr = buf;
  eptr = buf + 1;
}

static void interpolate_arg(symbol nm)
{
  const char *s = nm.contents();
  if (!s || *s == '\0')
    copy_mode_error("missing argument name");
  else if (s[1] == 0 && csdigit(s[0]))
    input_stack::push(input_stack::get_arg(s[0] - '0'));
  else if (s[0] == '*' && s[1] == '\0') {
    for (int i = input_stack::nargs(); i > 0; i--) {
      input_stack::push(input_stack::get_arg(i));
      if (i != 1)
	input_stack::push(make_temp_iterator(" "));
    }
  }
  else if (s[0] == '@' && s[1] == '\0') {
    for (int i = input_stack::nargs(); i > 0; i--) {
      input_stack::push(new end_quote_iterator);
      input_stack::push(input_stack::get_arg(i));
      input_stack::push(make_temp_iterator(i == 1 ? "\"" : " \""));
    }
  }
  else {
    const char *p;
    for (p = s; *p && csdigit(*p); p++)
      ;
    if (*p)
      copy_mode_error("bad argument name `%1'", s);
    else
      input_stack::push(input_stack::get_arg(atoi(s)));
  }
}

void handle_first_page_transition()
{
  push_token(tok);
  topdiv->begin_page();
}

// We push back a token by wrapping it up in a token_node, and
// wrapping that up in a string_iterator.

static void push_token(const token &t)
{
  macro m;
  m.append(new token_node(t));
  input_stack::push(new string_iterator(m));
}

void push_page_ejector()
{
  static char buf[2] = { PAGE_EJECTOR, '\0' };
  input_stack::push(make_temp_iterator(buf));
}

void handle_initial_request(unsigned char code)
{
  char buf[2];
  buf[0] = code;
  buf[1] = '\0';
  macro mac;
  mac.append(new token_node(tok));
  input_stack::push(new string_iterator(mac));
  input_stack::push(make_temp_iterator(buf));
  topdiv->begin_page();
  tok.next();
}

void handle_initial_title()
{
  handle_initial_request(TITLE_REQUEST);
}

// this should be local to define_macro, but cfront 1.2 doesn't support that
static symbol dot_symbol(".");

enum define_mode { DEFINE_NORMAL, DEFINE_APPEND, DEFINE_IGNORE };

void do_define_macro(define_mode mode)
{
  symbol nm;
  if (mode == DEFINE_NORMAL || mode == DEFINE_APPEND) {
    nm = get_name(1);
    if (nm.is_null()) {
      skip_line();
      return;
    }
  }
  symbol term = get_name();	// the request that terminates the definition
  if (term.is_null())
    term = dot_symbol;
  while (!tok.newline() && !tok.eof())
    tok.next();
  const char *start_filename;
  int start_lineno;
  int have_start_location = input_stack::get_location(0, &start_filename,
						      &start_lineno);
  node *n;
  // doing this here makes the line numbers come out right
  int c = get_copy(&n, 1);
  macro mac;
  macro *mm = 0;
  if (mode == DEFINE_NORMAL || mode == DEFINE_APPEND) {
    request_or_macro *rm =
      (request_or_macro *)request_dictionary.lookup(nm);
    if (rm)
      mm = rm->to_macro();
    if (mm && mode == DEFINE_APPEND)
      mac = *mm;
  }
  int bol = 1;
  for (;;) {
    while (c == ESCAPE_NEWLINE) {
      if (mode == DEFINE_NORMAL || mode == DEFINE_APPEND)
	mac.append(c);
      c = get_copy(&n, 1);
    }
    if (bol && c == '.') {
      const char *s = term.contents();
      int d;
      // see if it matches term
      int i;
      for (i = 0; s[i] != 0; i++) {
	d = get_copy(&n);
	if ((unsigned char)s[i] != d)
	  break;
      }
      if (s[i] == 0
	  && ((i == 2 && compatible_flag)
	      || (d = get_copy(&n)) == ' ' 
	      || d == '\n')) {	// we found it
		if (d == '\n')
		  tok.make_newline();
		else
		  tok.make_space();
		if (mode == DEFINE_APPEND || mode == DEFINE_NORMAL) {
		  if (!mm) {
		    mm = new macro;
		    request_dictionary.define(nm, mm);
		  }
		  *mm = mac;
		}
		if (term != dot_symbol) {
		  ignoring = 0;
		  interpolate_macro(term);
		}
		else
		  skip_line();
		return;
	      }
      if (mode == DEFINE_APPEND || mode == DEFINE_NORMAL) {
	mac.append(c);
	for (int j = 0; j < i; j++)
	  mac.append(s[j]);
      }
      c = d;
    }
    if (c == EOF) {
      if (mode == DEFINE_NORMAL || mode == DEFINE_APPEND) {
	if (have_start_location)
	  error_with_file_and_line(start_filename, start_lineno,
				   "end of file while defining macro `%1'",
				   nm.contents());
	else
	  error("end of file while defining macro `%1'", nm.contents());
      }
      else {
	if (have_start_location)
	  error_with_file_and_line(start_filename, start_lineno,
				   "end of file while ignoring input lines");
	else
	  error("end of file while ignoring input lines");
      }
      tok.next();
      return;
    }
    if (mode == DEFINE_NORMAL || mode == DEFINE_APPEND) {
      if (c == 0)
	mac.append(n);
      else
	mac.append(c);
    }
    bol = (c == '\n');
    c = get_copy(&n, 1);
  }
}

void define_macro()
{
  do_define_macro(DEFINE_NORMAL);
}

void append_macro()
{
  do_define_macro(DEFINE_APPEND);
}

void ignore()
{
  ignoring = 1;
  do_define_macro(DEFINE_IGNORE);
  ignoring = 0;
}

void remove_macro()
{
  for (;;) {
    symbol s = get_name();
    if (s.is_null())
      break;
    request_dictionary.remove(s);
  }
  skip_line();
}

void rename_macro()
{
  symbol s1 = get_name(1);
  if (!s1.is_null()) {
    symbol s2 = get_name(1);
    if (!s2.is_null())
      request_dictionary.rename(s1, s2);
  }
  skip_line();
}

void alias_macro()
{
  symbol s1 = get_name(1);
  if (!s1.is_null()) {
    symbol s2 = get_name(1);
    if (!s2.is_null()) {
      if (!request_dictionary.alias(s1, s2))
	warning(WARN_MAC, "`%1' not defined", s2.contents());
    }
  }
  skip_line();
}

void chop_macro()
{
  symbol s = get_name(1);
  if (!s.is_null()) {
    request_or_macro *p = lookup_request(s);
    macro *m = p->to_macro();
    if (!m)
      error("cannot chop request");
    else if (m->length == 0)
      error("cannot chop empty macro");
    else
      m->length -= 1;
  }
  skip_line();
}

void substring_macro()
{
  int start;
  symbol s = get_name(1);
  if (!s.is_null() && get_integer(&start)) {
    request_or_macro *p = lookup_request(s);
    macro *m = p->to_macro();
    if (!m)
      error("cannot substring request");
    else {
      if (start <= 0)
	start += m->length;
      else
	start--;
      int end = 0;
      if (!has_arg() || get_integer(&end)) {
	if (end <= 0)
	  end += m->length;
	else
	  end--;
	if (start > end) {
	  int tem = start;
	  start = end;
	  end = tem;
	}
	if (start >= m->length || start == end) {
	  m->length = 0;
	  if (m->p) {
	    if (--(m->p->count) <= 0)
	      delete m->p;
	    m->p = 0;
	  }
	}
	else if (start == 0)
	  m->length = end;
	else {
	  string_iterator iter(*m);
	  int i;
	  for (i = 0; i < start; i++)
	    if (iter.get(0) == EOF)
	      break;
	  macro mac;
	  for (; i < end; i++) {
	    node *nd;
	    int c = iter.get(&nd);
	    if (c == EOF)
	      break;
	    if (c == 0)
	      mac.append(nd);
	    else
	      mac.append((unsigned char)c);
	  }
	  *m = mac;
	}
      }
    }
  }
  skip_line();
}

void asciify_macro()
{
  symbol s = get_name(1);
  if (!s.is_null()) {
    request_or_macro *p = lookup_request(s);
    macro *m = p->to_macro();
    if (!m)
      error("cannot asciify request");
    else {
      macro am;
      string_iterator iter(*m);
      for (;;) {
	node *nd;
	int c = iter.get(&nd);
	if (c == EOF)
	  break;
	if (c != 0)
	  am.append(c);
	else
	  nd->asciify(&am);
      }
      *m = am;
    }
  }
  skip_line();
}

static void interpolate_environment_variable(symbol nm)
{
  const char *s = getenv(nm.contents());
  if (s && *s)
    input_stack::push(make_temp_iterator(s));
}

void interpolate_number_reg(symbol nm, int inc)
{
  reg *r = lookup_number_reg(nm);
  if (inc < 0)
    r->decrement();
  else if (inc > 0)
    r->increment();
  input_stack::push(make_temp_iterator(r->get_string()));
}

static void interpolate_number_format(symbol nm)
{
  reg *r = (reg *)number_reg_dictionary.lookup(nm);
  if (r)
    input_stack::push(make_temp_iterator(r->get_format()));
}

static int get_delim_number(units *n, int si, int prev_value)
{
  token start;
  start.next();
  if (start.delimiter(1)) {
    tok.next();
    if (get_number(n, si, prev_value)) {
      if (start != tok)
	warning(WARN_DELIM, "closing delimiter does not match");
      return 1;
    }
  }
  return 0;
}

static int get_delim_number(units *n, int si)
{
  token start;
  start.next();
  if (start.delimiter(1)) {
    tok.next();
    if (get_number(n, si)) {
      if (start != tok)
	warning(WARN_DELIM, "closing delimiter does not match");
      return 1;
    }
  }
  return 0;
}

static int get_line_arg(units *n, int si, charinfo **cp)
{
  token start;
  start.next();
  if (start.delimiter(1)) {
    tok.next();
    if (get_number(n, si)) {
      if (tok.dummy())
	tok.next();
      if (start != tok) {
	*cp = tok.get_char(1);
	tok.next();
      }
      if (start != tok)
	warning(WARN_DELIM, "closing delimiter does not match");
      return 1;
    }
  }
  return 0;
}

static int read_size(int *x)
{
  tok.next();
  int c = tok.ch();
  int inc = 0;
  if (c == '-') {
    inc = -1;
    tok.next();
    c = tok.ch();
  }
  else if (c == '+') {
    inc = 1;
    tok.next();
    c = tok.ch();
  }
  int val;
  int bad = 0;
  if (c == '(') {
    tok.next();
    c = tok.ch();
    if (!inc) {
      // allow an increment either before or after the left parenthesis
      if (c == '-') {
	inc = -1;
	tok.next();
	c = tok.ch();
      }
      else if (c == '+') {
	inc = 1;
	tok.next();
	c = tok.ch();
      }
    }
    if (!csdigit(c))
      bad = 1;
    else {
      val = c - '0';
      tok.next();
      c = tok.ch();
      if (!csdigit(c))
	bad = 1;
      else {
	val = val*10 + (c - '0');
	val *= sizescale;
      }
    }
  }
  else if (csdigit(c)) {
    val = c - '0';
    if (!inc && c != '0' && c < '4') {
      tok.next();
      c = tok.ch();
      if (!csdigit(c))
	bad = 1;
      else
	val = val*10 + (c - '0');
    }
    val *= sizescale;
  }
  else if (!tok.delimiter(1))
    return 0;
  else {
    token start(tok);
    tok.next();
    if (!(inc 
	  ? get_number(&val, 'z')
	  : get_number(&val, 'z', curenv->get_requested_point_size())))
      return 0;
    if (!(start.ch() == '[' && tok.ch() == ']') && start != tok) {
      if (start.ch() == '[')
	error("missing `]'");
      else
	error("missing closing delimiter");
      return 0;
    }
  }
  if (!bad) {
    switch (inc) {
    case 0:
      *x = val;
      break;
    case 1:
      *x = curenv->get_requested_point_size() + val;
      break;
    case -1:
      *x = curenv->get_requested_point_size() - val;
      break;
    default:
      assert(0);
    }
    return 1;
  }
  else {
    error("bad digit in point size");
    return 0;
  }
}

static symbol get_delim_name()
{
  token start;
  start.next();
  if (start.eof()) {
    error("end of input at start of delimited name");
    return NULL_SYMBOL;
  }
  if (start.newline()) {
    error("can't delimit name with a newline");
    return NULL_SYMBOL;
  }
  int start_level = input_stack::get_level();
  char abuf[ABUF_SIZE];
  char *buf = abuf;
  int buf_size = ABUF_SIZE;
  int i = 0;
  for (;;) {
    if (i + 1 > buf_size) {
      if (buf == abuf) {
	buf = new char [ABUF_SIZE*2];
	memcpy(buf, abuf, buf_size);
	buf_size = ABUF_SIZE*2;
      }
      else {
	char *old_buf = buf;
	buf = new char[buf_size*2];
	memcpy(buf, old_buf, buf_size);
	buf_size *= 2;
	a_delete old_buf;
      }
    }
    tok.next();
    if (tok == start
	&& (compatible_flag || input_stack::get_level() == start_level))
      break;
    if ((buf[i] = tok.ch()) == 0) {
      error("missing delimiter (got %1)", tok.description());
      if (buf != abuf)
	a_delete buf;
      return NULL_SYMBOL;
    }
    i++;
  }
  buf[i] = '\0';
  if (buf == abuf) {
    if (i == 0) {
      error("empty delimited name");
      return NULL_SYMBOL;
    }
    else
      return symbol(buf);
  }
  else {
    symbol s(buf);
    a_delete buf;
    return s;
  }
}


// Implement \R

static void do_register()
{
  token start;
  start.next();
  if (!start.delimiter(1))
    return;
  tok.next();
  symbol nm = get_long_name(1);
  if (nm.is_null())
    return;
  while (tok.space())
    tok.next();
  reg *r = (reg *)number_reg_dictionary.lookup(nm);
  int prev_value;
  if (!r || !r->get_value(&prev_value))
    prev_value = 0;
  int val;
  if (!get_number(&val, 'u', prev_value))
    return;
  if (start != tok)
    warning(WARN_DELIM, "closing delimiter does not match");
  if (r)
    r->set_value(val);
  else
    set_number_reg(nm, val);
}

// this implements the \w escape sequence

static void do_width()
{
  token start;
  start.next();
  int start_level = input_stack::get_level();
  environment env(curenv);
  environment *oldenv = curenv;
  curenv = &env;
  for (;;) {
    tok.next();
    if (tok.eof()) {
      warning(WARN_DELIM, "missing closing delimiter");
      break;
    }
    if (tok.newline()) {
      warning(WARN_DELIM, "missing closing delimiter");
      input_stack::push(make_temp_iterator("\n"));
      break;
    }
    if (tok == start
	&& (compatible_flag || input_stack::get_level() == start_level))
      break;
    tok.process();
  }
  env.wrap_up_tab();
  units x = env.get_input_line_position().to_units();
  input_stack::push(make_temp_iterator(itoa(x)));
  env.width_registers();
  curenv = oldenv;
}

charinfo *page_character;

void set_page_character()
{
  page_character = get_optional_char();
  skip_line();
}

static const symbol percent_symbol("%");

void read_title_parts(node **part, hunits *part_width)
{
  tok.skip();
  if (tok.newline() || tok.eof())
    return;
  token start(tok);
  int start_level = input_stack::get_level();
  tok.next();
  for (int i = 0; i < 3; i++) {
    while (!tok.newline() && !tok.eof()) {
      if (tok == start
	  && (compatible_flag || input_stack::get_level() == start_level)) {
	tok.next();
	break;
      }
      if (page_character != 0 && tok.get_char() == page_character)
	interpolate_number_reg(percent_symbol, 0);
      else
	tok.process();
      tok.next();
    }
    curenv->wrap_up_tab();
    part_width[i] = curenv->get_input_line_position();
    part[i] = curenv->extract_output_line();
  }
  while (!tok.newline() && !tok.eof())
    tok.next();
}

class non_interpreted_node : public node {
  macro mac;
public:
  non_interpreted_node(const macro &);
  int interpret(macro *);
  node *copy();
  int same(node *);
  const char *type();
};

non_interpreted_node::non_interpreted_node(const macro &m) : mac(m)
{
}

int non_interpreted_node::same(node *nd)
{
  return mac == ((non_interpreted_node *)nd)->mac;
}

const char *non_interpreted_node::type()
{
  return "non_interpreted_node";
}

node *non_interpreted_node::copy()
{
  return new non_interpreted_node(mac);
}

int non_interpreted_node::interpret(macro *m)
{
  string_iterator si(mac);
  node *n;
  for (;;) {
    int c = si.get(&n);
    if (c == EOF)
      break;
    if (c == 0)
      m->append(n);
    else
      m->append(c);
  }
  return 1;
}

static node *do_non_interpreted()
{
  node *n;
  int c;
  macro mac;
  while ((c = get_copy(&n)) != ESCAPE_QUESTION && c != EOF && c != '\n')
    if (c == 0)
      mac.append(n);
    else
      mac.append(c);
  if (c == EOF || c == '\n') {
    error("missing \\?");
    return 0;
  }
  return new non_interpreted_node(mac);
}

node *do_special()
{
  token start;
  start.next();
  int start_level = input_stack::get_level();
  macro mac;
  for (tok.next();
       tok != start || input_stack::get_level() != start_level;
       tok.next()) {
    if (tok.eof()) {
      warning(WARN_DELIM, "missing closing delimiter");
      return 0;
    }
    if (tok.newline()) {
      input_stack::push(make_temp_iterator("\n"));
      warning(WARN_DELIM, "missing closing delimiter");
      break;
    }
    unsigned char c;
    if (tok.space())
      c = ' ';
    else if (tok.tab())
      c = '\t';
    else if (tok.leader())
      c = '\001';
    else if (tok.backspace())
      c = '\b';
    else
      c = tok.ch();
    if (c == '\0')
      error("%1 is illegal within \\X", tok.description());
    else 
      mac.append(c);
  }
  return new special_node(mac);
}

void special_node::tprint(troff_output_file *out)
{
  tprint_start(out);
  string_iterator iter(mac);
  for (;;) {
    int c = iter.get(NULL);
    if (c == EOF)
      break;
    for (const char *s = ::asciify(c); *s; s++)
      tprint_char(out, *s);
  }
  tprint_end(out);
}

int get_file_line(const char **filename, int *lineno)
{
  return input_stack::get_location(0, filename, lineno);
}

void line_file()
{
  int n;
  if (get_integer(&n)) {
    const char *filename = 0;
    if (has_arg()) {
      symbol s = get_long_name();
      filename = s.contents();
    }
    (void)input_stack::set_location(filename, n-1);
  }
  skip_line();
}

static int nroff_mode = 0;

static void nroff_request()
{
  nroff_mode = 1;
  skip_line();
}

static void troff_request()
{
  nroff_mode = 0;
  skip_line();
}

static void skip_alternative()
{
  int level = 0;
  // ensure that ``.if 0\{'' works as expected
  if (tok.left_brace())
    level++;
  int c;
  for (;;) {
    c = input_stack::get(NULL);
    if (c == EOF)
      break;
    if (c == ESCAPE_LEFT_BRACE)
      ++level;
    else if (c == ESCAPE_RIGHT_BRACE)
      --level;
    else if (c == escape_char && escape_char > 0)
      switch(input_stack::get(NULL)) {
      case '{':
	++level;
	break;
      case '}':
	--level;
	break;
      case '"':
	while ((c = input_stack::get(NULL)) != '\n' && c != EOF)
	  ;
      }
    /*
      Note that the level can properly be < 0, eg
	
	.if 1 \{\
	.if 0 \{\
	.\}\}

      So don't give an error message in this case.
    */
    if (level <= 0 && c == '\n')
      break;
  }
  tok.next();
}

static void begin_alternative()
{
  while (tok.space() || tok.left_brace())
    tok.next();
}


static int_stack if_else_stack;

int do_if_request()
{
  int invert = 0;
  while (tok.space())
    tok.next();
  while (tok.ch() == '!') {
    tok.next();
    invert = !invert;
  }
  int result;
  unsigned char c = tok.ch();
  if (c == 't') {
    tok.next();
    result = !nroff_mode;
  }
  else if (c == 'n') {
    tok.next();
    result = nroff_mode;
  }
  else if (c == 'v') {
    tok.next();
    result = 0;
  }
  else if (c == 'o') {
    result = (topdiv->get_page_number() & 1);
    tok.next();
  }
  else if (c == 'e') {
    result = !(topdiv->get_page_number() & 1);
    tok.next();
  }
  else if (c == 'd' || c == 'r') {
    tok.next();
    symbol nm = get_name(1);
    if (nm.is_null()) {
      skip_alternative();
      return 0;
    }
    result = (c == 'd' 
	      ? request_dictionary.lookup(nm) != 0
	      : number_reg_dictionary.lookup(nm) != 0);
  }
  else if (c == 'c') {
    tok.next();
    tok.skip();
    charinfo *ci = tok.get_char(1);
    if (ci == 0) {
      skip_alternative();
      return 0;
    }
    result = character_exists(ci, curenv);
    tok.next();
  }
  else if (tok.space())
    result = 0;
  else if (tok.delimiter()) {
    token delim = tok;
    int delim_level = input_stack::get_level();
    environment env1(curenv);
    environment env2(curenv);
    environment *oldenv = curenv;
    curenv = &env1;
    for (int i = 0; i < 2; i++) {
      for (;;) {
	tok.next();
	if (tok.newline() || tok.eof()) {
	  warning(WARN_DELIM, "missing closing delimiter");
	  tok.next();
	  curenv = oldenv;
	  return 0;
	}
	if (tok == delim
	    && (compatible_flag || input_stack::get_level() == delim_level))
	  break;
	tok.process();
      }
      curenv = &env2;
    }
    node *n1 = env1.extract_output_line();
    node *n2 = env2.extract_output_line();
    result = same_node_list(n1, n2);
    delete_node_list(n1);
    delete_node_list(n2);
    curenv = oldenv;
    tok.next();
  }
  else {
    units n;
    if (!get_number(&n, 'u')) {
      skip_alternative();
      return 0;
    }
    else
      result = n > 0;
  }
  if (invert)
    result = !result;
  if (result)
    begin_alternative();
  else
    skip_alternative();
  return result;
}

void if_else_request()
{
  if_else_stack.push(do_if_request());
}

void if_request()
{
  do_if_request();
}

void else_request()
{
  if (if_else_stack.is_empty()) {
    warning(WARN_EL, "unbalanced .el request");
    skip_alternative();
  }
  else {
    if (if_else_stack.pop())
      skip_alternative();
    else
      begin_alternative();
  }
}

static int while_depth = 0;
static int while_break_flag = 0;

void while_request()
{
  macro mac;
  int escaped = 0;
  int level = 0;
  mac.append(new token_node(tok));
  for (;;) {
    node *n;
    int c = input_stack::get(&n);
    if (c == EOF)
      break;
    if (c == 0) {
      escaped = 0;
      mac.append(n);
    }
    else if (escaped) {
      if (c == '{')
	level += 1;
      else if (c == '}')
	level -= 1;
      escaped = 0;
      mac.append(c);
    }
    else {
      if (c == ESCAPE_LEFT_BRACE)
	level += 1;
      else if (c == ESCAPE_RIGHT_BRACE)
	level -= 1;
      else if (c == escape_char)
	escaped = 1;
      mac.append(c);
      if (c == '\n' && level <= 0)
	break;
    }
  }
  if (level != 0)
    error("unbalanced \\{ \\}");
  else {
    while_depth++;
    input_stack::add_boundary();
    for (;;) {
      input_stack::push(new string_iterator(mac, "while loop"));
      tok.next();
      if (!do_if_request()) {
	while (input_stack::get(NULL) != EOF)
	  ;
	break;
      }
      process_input_stack();
      if (while_break_flag) {
	while_break_flag = 0;
	break;
      }
    }
    input_stack::remove_boundary();
    while_depth--;
  }
  tok.next();
}

void while_break_request()
{
  if (!while_depth) {
    error("no while loop");
    skip_line();
  }
  else {
    while_break_flag = 1;
    while (input_stack::get(NULL) != EOF)
      ;
    tok.next();
  }
}

void while_continue_request()
{
  if (!while_depth) {
    error("no while loop");
    skip_line();
  }
  else {
    while (input_stack::get(NULL) != EOF)
      ;
    tok.next();
  }
}

// .so

void source()
{
  symbol nm = get_long_name(1);
  if (nm.is_null())
    skip_line();
  else {
    while (!tok.newline() && !tok.eof())
      tok.next();
    errno = 0;
    FILE *fp = fopen(nm.contents(), "r");
    if (fp)
      input_stack::push(new file_iterator(fp, nm.contents()));
    else
      error("can't open `%1': %2", nm.contents(), strerror(errno));
    tok.next();
  }
}

// like .so but use popen()

void pipe_source()
{
#ifdef POPEN_MISSING
  error("pipes not available on this system");
  skip_line();
#else /* not POPEN_MISSING */
  if (tok.newline() || tok.eof())
    error("missing command");
  else {
    int c;
    while ((c = get_copy(NULL)) == ' ' || c == '\t')
      ;
    int buf_size = 24;
    char *buf = new char[buf_size];
    int buf_used = 0;
    for (; c != '\n' && c != EOF; c = get_copy(NULL)) {
      const char *s = asciify(c);
      int slen = strlen(s);
      if (buf_used + slen + 1> buf_size) {
	char *old_buf = buf;
	int old_buf_size = buf_size;
	buf_size *= 2;
	buf = new char[buf_size];
	memcpy(buf, old_buf, old_buf_size);
	a_delete old_buf;
      }
      strcpy(buf + buf_used, s);
      buf_used += slen;
    }
    buf[buf_used] = '\0';
    errno = 0;
    FILE *fp = popen(buf, "r");
    if (fp)
      input_stack::push(new file_iterator(fp, symbol(buf).contents(), 1));
    else
      error("can't open pipe to process `%1': %2", buf, strerror(errno));
    a_delete buf;
  }
  tok.next();
#endif /* not POPEN_MISSING */
}

const char *asciify(int c)
{
  static char buf[3];
  buf[0] = escape_char == '\0' ? '\\' : escape_char;
  buf[1] = buf[2] = '\0';
  switch (c) {
  case ESCAPE_QUESTION:
    buf[1] = '?';
    break;
  case ESCAPE_AMPERSAND:
    buf[1] = '&';
    break;
  case ESCAPE_UNDERSCORE:
    buf[1] = '_';
    break;
  case ESCAPE_BAR:
    buf[1] = '|';
    break;
  case ESCAPE_CIRCUMFLEX:
    buf[1] = '^';
    break;
  case ESCAPE_LEFT_BRACE:
    buf[1] = '{';
    break;
  case ESCAPE_RIGHT_BRACE:
    buf[1] = '}';
    break;
  case ESCAPE_LEFT_QUOTE:
    buf[1] = '`';
    break;
  case ESCAPE_RIGHT_QUOTE:
    buf[1] = '\'';
    break;
  case ESCAPE_HYPHEN:
    buf[1] = '-';
    break;
  case ESCAPE_BANG:
    buf[1] = '!';
    break;
  case ESCAPE_c:
    buf[1] = 'c';
    break;
  case ESCAPE_e:
    buf[1] = 'e';
    break;
  case ESCAPE_E:
    buf[1] = 'E';
    break;
  case ESCAPE_PERCENT:
    buf[1] = '%';
    break;
  case ESCAPE_SPACE:
    buf[1] = ' ';
    break;
  default:
    if (illegal_input_char(c))
      buf[0] = '\0';
    else
      buf[0] = c;
    break;
  }
  return buf;
}
  

const char *input_char_description(int c)
{
  switch (c) {
  case '\n':
    return "a newline character";
  case '\b':
    return "a backspace character";
  case '\001':
    return "a leader character";
  case '\t':
    return "a tab character ";
  case ' ':
    return "a space character";
  case '\0':
    return "a node";
  }
  static char buf[sizeof("magic character code ") + 1 + INT_DIGITS];
  if (illegal_input_char(c)) {
    const char *s = asciify(c);
    if (*s) {
      buf[0] = '`';
      strcpy(buf + 1, s);
      strcat(buf, "'");
      return buf;
    }
    sprintf(buf, "magic character code %d", c);
    return buf;
  }
  if (csprint(c)) {
    buf[0] = '`';
    buf[1] = c;
    buf[2] = '\'';
    return buf;
  }
  sprintf(buf, "character code %d", c);
  return buf;
}

// .tm

void terminal()
{
  if (!tok.newline() && !tok.eof()) {
    int c;
    while ((c = get_copy(NULL)) == ' ' || c == '\t')
      ;
    for (; c != '\n' && c != EOF; c = get_copy(NULL))
      fputs(asciify(c), stderr);
  }
  fputc('\n', stderr);
  fflush(stderr);
  tok.next();
}

dictionary stream_dictionary(20);

void do_open(int append)
{
  symbol stream = get_name(1);
  if (!stream.is_null()) {
    symbol filename = get_long_name(1);
    if (!filename.is_null()) {
      errno = 0;
      FILE *fp = fopen(filename.contents(), append ? "a" : "w");
      if (!fp) {
	error("can't open `%1' for %2: %3",
	      filename.contents(),
	      append ? "appending" : "writing",
	      strerror(errno));
	fp = (FILE *)stream_dictionary.remove(stream);
      }
      else
	fp = (FILE *)stream_dictionary.lookup(stream, fp);
      if (fp)
	fclose(fp);
    }
  }
  skip_line();
}

void open_request()
{
  do_open(0);
}

void opena_request()
{
  do_open(1);
}

void close_request()
{
  symbol stream = get_name(1);
  if (!stream.is_null()) {
    FILE *fp = (FILE *)stream_dictionary.remove(stream);
    if (!fp)
      error("no stream named `%1'", stream.contents());
    else
      fclose(fp);
  }
  skip_line();
}

void write_request()
{
  symbol stream = get_name(1);
  if (stream.is_null()) {
    skip_line();
    return;
  }
  FILE *fp = (FILE *)stream_dictionary.lookup(stream);
  if (!fp) {
    error("no stream named `%1'", stream.contents());
    skip_line();
    return;
  }
  int c;
  while ((c = get_copy(NULL)) == ' ')
    ;
  if (c == '"')
    c = get_copy(NULL);
  for (; c != '\n' && c != EOF; c = get_copy(NULL))
    fputs(asciify(c), fp);
  fputc('\n', fp);
  fflush(fp);
  tok.next();
}

void write_macro_request()
{
  symbol stream = get_name(1);
  if (stream.is_null()) {
    skip_line();
    return;
  }
  FILE *fp = (FILE *)stream_dictionary.lookup(stream);
  if (!fp) {
    error("no stream named `%1'", stream.contents());
    skip_line();
    return;
  }
  symbol s = get_name(1);
  if (s.is_null()) {
    skip_line();
    return;
  }
  request_or_macro *p = lookup_request(s);
  macro *m = p->to_macro();
  if (!m)
    error("cannot write request");
  else {
    string_iterator iter(*m);
    for (;;) {
      int c = iter.get(0);
      if (c == EOF)
	break;
      fputs(asciify(c), fp);
    }
    fflush(fp);
  }
  skip_line();
}

static void init_charset_table()
{
  char buf[16];
  strcpy(buf, "char");
  for (int i = 0; i < 256; i++) {
    strcpy(buf + 4, itoa(i));
    charset_table[i] = get_charinfo(symbol(buf));
    charset_table[i]->set_ascii_code(i);
    if (csalpha(i))
      charset_table[i]->set_hyphenation_code(cmlower(i));
  }
  charset_table['.']->set_flags(charinfo::ENDS_SENTENCE);
  charset_table['?']->set_flags(charinfo::ENDS_SENTENCE);
  charset_table['!']->set_flags(charinfo::ENDS_SENTENCE);
  charset_table['-']->set_flags(charinfo::BREAK_AFTER);
  charset_table['"']->set_flags(charinfo::TRANSPARENT);
  charset_table['\'']->set_flags(charinfo::TRANSPARENT);
  charset_table[')']->set_flags(charinfo::TRANSPARENT);
  charset_table[']']->set_flags(charinfo::TRANSPARENT);
  charset_table['*']->set_flags(charinfo::TRANSPARENT);
  get_charinfo(symbol("dg"))->set_flags(charinfo::TRANSPARENT);
  get_charinfo(symbol("rq"))->set_flags(charinfo::TRANSPARENT);
  get_charinfo(symbol("em"))->set_flags(charinfo::BREAK_AFTER);
  get_charinfo(symbol("ul"))->set_flags(charinfo::OVERLAPS_HORIZONTALLY);
  get_charinfo(symbol("rn"))->set_flags(charinfo::OVERLAPS_HORIZONTALLY);
  get_charinfo(symbol("radicalex"))->set_flags(charinfo::OVERLAPS_HORIZONTALLY);
  get_charinfo(symbol("ru"))->set_flags(charinfo::OVERLAPS_HORIZONTALLY);
  get_charinfo(symbol("br"))->set_flags(charinfo::OVERLAPS_VERTICALLY);
  page_character = charset_table['%'];
}

static
void do_translate(int translate_transparent)
{
  tok.skip();
  while (!tok.newline() && !tok.eof()) {
    if (tok.space()) {
      // This is a really bizarre troff feature.
      tok.next();
      translate_space_to_dummy = tok.dummy();
      if (tok.newline() || tok.eof())
	break;
      tok.next();
      continue;
    }
    charinfo *ci1 = tok.get_char(1);
    if (ci1 == 0)
      break;
    tok.next();
    if (tok.newline() || tok.eof()) {
      ci1->set_special_translation(charinfo::TRANSLATE_SPACE,
				   translate_transparent);
      break;
    }
    if (tok.space())
      ci1->set_special_translation(charinfo::TRANSLATE_SPACE,
				   translate_transparent);
    else if (tok.dummy())
      ci1->set_special_translation(charinfo::TRANSLATE_DUMMY,
				   translate_transparent);
    else if (tok.hyphen_indicator())
      ci1->set_special_translation(charinfo::TRANSLATE_HYPHEN_INDICATOR,
				   translate_transparent);
    else {
      charinfo *ci2 = tok.get_char(1);
      if (ci2 == 0)
	break;
      if (ci1 == ci2)
	ci1->set_translation(0, translate_transparent);
      else
	ci1->set_translation(ci2, translate_transparent);
    }
    tok.next();
  }
  skip_line();
}

void translate()
{
  do_translate(1);
}

void translate_no_transparent()
{
  do_translate(0);
}

void char_flags()
{
  int flags;
  if (get_integer(&flags))
    while (has_arg()) {
      charinfo *ci = tok.get_char(1);
      if (ci) {
	charinfo *tem = ci->get_translation();
	if (tem)
	  ci = tem;
	ci->set_flags(flags);
      }
      tok.next();
    }
  skip_line();
}

void hyphenation_code()
{
  tok.skip();
  while (!tok.newline() && !tok.eof()) {
    charinfo *ci = tok.get_char(1);
    if (ci == 0)
      break;
    tok.next();
    tok.skip();
    unsigned char c = tok.ch();
    if (c == 0) {
      error("hyphenation code must be ordinary character");
      break;
    }
    if (csdigit(c)) {
      error("hyphenation code cannot be digit");
      break;
    }
    ci->set_hyphenation_code(c);
    tok.next();
    tok.skip();
  }
  skip_line();
}

charinfo *token::get_char(int required)
{
  if (type == TOKEN_CHAR)
    return charset_table[c];
  if (type == TOKEN_SPECIAL)
    return get_charinfo(nm);
  if (type == TOKEN_NUMBERED_CHAR)
    return get_charinfo_by_number(val);
  if (type == TOKEN_ESCAPE) {
    if (escape_char != 0)
      return charset_table[escape_char];
    else {
      error("`\\e' used while no current escape character");
      return 0;
    }
  }
  if (required) {
    if (type == TOKEN_EOF || type == TOKEN_NEWLINE)
      warning(WARN_MISSING, "missing normal or special character");
    else
      error("normal or special character expected (got %1)", description());
  }
  return 0;
}

charinfo *get_optional_char()
{
  while (tok.space())
    tok.next();
  charinfo *ci = tok.get_char();
  if (!ci)
    check_missing_character();
  else
    tok.next();
  return ci;
}

void check_missing_character()
{
  if (!tok.newline() && !tok.eof() && !tok.right_brace() && !tok.tab())
    error("normal or special character expected (got %1): "
	  "treated as missing",
	  tok.description());
}

int token::add_to_node_list(node **pp)
{
  hunits w;
  node *n = 0;
  switch (type) {
  case TOKEN_CHAR:
    *pp = (*pp)->add_char(charset_table[c], curenv, &w);
    break;
  case TOKEN_DUMMY:
    n = new dummy_node;
    break;
  case TOKEN_ESCAPE:
    if (escape_char != 0)
      *pp = (*pp)->add_char(charset_table[escape_char], curenv, &w);
    break;
  case TOKEN_HYPHEN_INDICATOR:
    *pp = (*pp)->add_discretionary_hyphen();
    break;
  case TOKEN_ITALIC_CORRECTION:
    *pp = (*pp)->add_italic_correction(&w);
    break;
  case TOKEN_LEFT_BRACE:
    break;
  case TOKEN_MARK_INPUT:
    set_number_reg(nm, curenv->get_input_line_position().to_units());
    break;
  case TOKEN_NODE:
    n = nd;
    nd = 0;
    break;
  case TOKEN_NUMBERED_CHAR:
    *pp = (*pp)->add_char(get_charinfo_by_number(val), curenv, &w);
    break;
  case TOKEN_RIGHT_BRACE:
    break;
  case TOKEN_SPACE:
    n = new hmotion_node(curenv->get_space_width());
    break;
  case TOKEN_SPECIAL:
    *pp = (*pp)->add_char(get_charinfo(nm), curenv, &w);
    break;
  default:
    return 0;
  }
  if (n) {
    n->next = *pp;
    *pp = n;
  }
  return 1;
}

void token::process()
{
  if (possibly_handle_first_page_transition())
    return;
  switch (type) {
  case TOKEN_BACKSPACE:
    curenv->add_node(new hmotion_node(-curenv->get_space_width()));
    break;
  case TOKEN_CHAR:
    curenv->add_char(charset_table[c]);
    break;
  case TOKEN_DUMMY:
    curenv->add_node(new dummy_node);
    break;
  case TOKEN_EOF:
    assert(0);
    break;
  case TOKEN_EMPTY:
    assert(0);
    break;
  case TOKEN_ESCAPE:
    if (escape_char != 0)
      curenv->add_char(charset_table[escape_char]);
    break;
  case TOKEN_BEGIN_TRAP:
  case TOKEN_END_TRAP:
  case TOKEN_PAGE_EJECTOR:
    // these are all handled in process_input_stack()
    break;
  case TOKEN_HYPHEN_INDICATOR:
    curenv->add_hyphen_indicator();
    break;
  case TOKEN_INTERRUPT:
    curenv->interrupt();
    break;
  case TOKEN_ITALIC_CORRECTION:
    curenv->add_italic_correction();
    break;
  case TOKEN_LEADER:
    curenv->handle_tab(1);
    break;
  case TOKEN_LEFT_BRACE:
    break;
  case TOKEN_MARK_INPUT:
    set_number_reg(nm, curenv->get_input_line_position().to_units());
    break;
  case TOKEN_NEWLINE:
    curenv->newline();
    break;
  case TOKEN_NODE:
    curenv->add_node(nd);
    nd = 0;
    break;
  case TOKEN_NUMBERED_CHAR:
    curenv->add_char(get_charinfo_by_number(val));
    break;
  case TOKEN_REQUEST:
    // handled in process_input_stack
    break;
  case TOKEN_RIGHT_BRACE:
    break;
  case TOKEN_SPACE:
    curenv->space();
    break;
  case TOKEN_SPECIAL:
    curenv->add_char(get_charinfo(nm));
    break;
  case TOKEN_SPREAD:
    curenv->spread();
    break;
  case TOKEN_TAB:
    curenv->handle_tab(0);
    break;
  case TOKEN_TRANSPARENT:
    break;
  default:
    assert(0);
  }
}

class nargs_reg : public reg {
public:
  const char *get_string();
};

const char *nargs_reg::get_string()
{
  return itoa(input_stack::nargs());
}

class lineno_reg : public reg {
public:
  const char *get_string();
};

const char *lineno_reg::get_string()
{
  int line;
  const char *file;
  if (!input_stack::get_location(0, &file, &line))
    line = 0;
  return itoa(line);
}


class writable_lineno_reg : public general_reg {
public:
  writable_lineno_reg();
  void set_value(units);
  int get_value(units *);
};

writable_lineno_reg::writable_lineno_reg()
{
}

int writable_lineno_reg::get_value(units *res)
{
  int line;
  const char *file;
  if (!input_stack::get_location(0, &file, &line))
    return 0;
  *res = line;
  return 1;
}

void writable_lineno_reg::set_value(units n)
{
  input_stack::set_location(0, n);
}

class filename_reg : public reg {
public:
  const char *get_string();
};

const char *filename_reg::get_string()
{
  int line;
  const char *file;
  if (input_stack::get_location(0, &file, &line))
    return file;
  else
    return 0;
}


class constant_reg : public reg {
  const char *s;
public:
  constant_reg(const char *);
  const char *get_string();
};

constant_reg::constant_reg(const char *p) : s(p)
{
}

const char *constant_reg::get_string()
{
  return s;
}

constant_int_reg::constant_int_reg(int *q) : p(q)
{
}

const char *constant_int_reg::get_string()
{
  return itoa(*p);
}

void abort_request()
{
  int c;
  if (tok.eof())
    c = EOF;
  else if (tok.newline())
    c = '\n';
  else {
    while ((c = get_copy(0)) == ' ')
      ;
  }
  if (c == EOF || c == '\n')
    fputs("User Abort.", stderr);
  else {
    for (; c != '\n' && c != EOF; c = get_copy(NULL))
      fputs(asciify(c), stderr);
  }
  fputc('\n', stderr);
  cleanup_and_exit(1);
}
      
char *read_string()
{
  int len = 256;
  char *s = new char[len];
  int c;
  while ((c = get_copy(0)) == ' ')
    ;
  int i = 0;
  while (c != '\n' && c != EOF) {
    if (!illegal_input_char(c)) {
      if (i + 2 > len) {
	char *tem = s;
	s = new char[len*2];
	memcpy(s, tem, len);
	len *= 2;
	a_delete tem;
      }
      s[i++] = c;
    }
    c = get_copy(0);
  }
  s[i] = '\0';
  tok.next();
  if (i == 0) {
    a_delete s;
    return 0;
  }
  return s;
}

void pipe_output()
{
#ifdef POPEN_MISSING
  error("pipes not available on this system");
  skip_line();
#else /* not POPEN_MISSING */
  if (the_output) {
    error("can't pipe: output already started");
    skip_line();
  }
  else {
    if ((pipe_command = read_string()) == 0)
      error("can't pipe to empty command");
  }
#endif /* not POPEN_MISSING */
}

static int system_status;

void system_request()
{
  char *command = read_string();
  if (!command)
    error("empty command");
  else {
    system_status = system(command);
    a_delete command;
  }
}

void copy_file()
{
  if (curdiv == topdiv && topdiv->before_first_page) {
    handle_initial_request(COPY_FILE_REQUEST);
    return;
  }
  symbol filename = get_long_name(1);
  while (!tok.newline() && !tok.eof())
    tok.next();
  if (break_flag)
    curenv->do_break();
  if (!filename.is_null())
    curdiv->copy_file(filename.contents());
  tok.next();
}

#ifdef COLUMN

void vjustify()
{
  if (curdiv == topdiv && topdiv->before_first_page) {
    handle_initial_request(VJUSTIFY_REQUEST);
    return;
  }
  symbol type = get_long_name(1);
  if (!type.is_null())
    curdiv->vjustify(type);
  skip_line();
}

#endif /* COLUMN */

void transparent_file()
{
  if (curdiv == topdiv && topdiv->before_first_page) {
    handle_initial_request(TRANSPARENT_FILE_REQUEST);
    return;
  }
  symbol filename = get_long_name(1);
  while (!tok.newline() && !tok.eof())
    tok.next();
  if (break_flag)
    curenv->do_break();
  if (!filename.is_null()) {
    errno = 0;
    FILE *fp = fopen(filename.contents(), "r");
    if (!fp)
      error("can't open `%1': %2", filename.contents(), strerror(errno));
    else {
      int bol = 1;
      for (;;) {
	int c = getc(fp);
	if (c == EOF)
	  break;
	if (illegal_input_char(c))
	  warning(WARN_INPUT, "illegal input character code %1", int(c));
	else {
	  curdiv->transparent_output(c);
	  bol = c == '\n';
	}
      }
      if (!bol)
	curdiv->transparent_output('\n');
      fclose(fp);
    }
  }
  tok.next();
}

class page_range {
  int first;
  int last;
public:
  page_range *next;
  page_range(int, int, page_range *);
  int contains(int n);
};

page_range::page_range(int i, int j, page_range *p)
: first(i), last(j), next(p)
{
}

int page_range::contains(int n)
{
  return n >= first && (last <= 0 || n <= last);
}

page_range *output_page_list = 0;

int in_output_page_list(int n)
{
  if (!output_page_list)
    return 1;
  for (page_range *p = output_page_list; p; p = p->next)
    if (p->contains(n))
      return 1;
  return 0;
}

static void parse_output_page_list(char *p)
{
  for (;;) {
    int i;
    if (*p == '-')
      i = 1;
    else if (csdigit(*p)) {
      i = 0;
      do
	i = i*10 + *p++ - '0';
      while (csdigit(*p));
    }
    else
      break;
    int j;
    if (*p == '-') {
      p++;
      j = 0;
      if (csdigit(*p)) {
	do
	  j = j*10 + *p++ - '0';
	while (csdigit(*p));
      }
    }
    else
      j = i;
    if (j == 0)
      last_page_number = -1;
    else if (last_page_number >= 0 && j > last_page_number)
      last_page_number = j;
    output_page_list = new page_range(i, j, output_page_list);
    if (*p != ',')
      break;
    ++p;
  }
  if (*p != '\0') {
    error("bad output page list");
    output_page_list = 0;
  }
}

static FILE *open_mac_file(const char *mac, char **path)
{
  char *s = new char[strlen(mac)+strlen(MACRO_PREFIX)+1];
  strcpy(s, MACRO_PREFIX);
  strcat(s, mac);
  FILE *fp = macro_path.open_file(s, path);
  a_delete s;
  return fp;
}

static void process_macro_file(const char *mac)
{
  char *path;
  FILE *fp = open_mac_file(mac, &path);
  if (!fp)
    fatal("can't find macro file %1", mac);
  const char *s = symbol(path).contents();
  a_delete path;
  input_stack::push(new file_iterator(fp, s));
  tok.next();
  process_input_stack();
}

static void process_startup_file()
{
  char *path;
  FILE *fp = macro_path.open_file(STARTUP_FILE, &path);
  if (fp) {
    input_stack::push(new file_iterator(fp, symbol(path).contents()));
    a_delete path;
    tok.next();
    process_input_stack();
  }
}

void macro_source()
{
  symbol nm = get_long_name(1);
  if (nm.is_null())
    skip_line();
  else {
    while (!tok.newline() && !tok.eof())
      tok.next();
    char *path;
    FILE *fp = macro_path.open_file(nm.contents(), &path);
    if (fp) {
      input_stack::push(new file_iterator(fp, symbol(path).contents()));
      a_delete path;
    }
    else
      error("can't find macro file `%1'", nm.contents());
    tok.next();
  }
}

static void process_input_file(const char *name)
{
  FILE *fp;
  if (strcmp(name, "-") == 0) {
    clearerr(stdin);
    fp = stdin;
  }
  else {
    errno = 0;
    fp = fopen(name, "r");
    if (!fp)
      fatal("can't open `%1': %2", name, strerror(errno));
  }
  input_stack::push(new file_iterator(fp, name));
  tok.next();
  process_input_stack();
}

// make sure the_input is empty before calling this

static int evaluate_expression(const char *expr, units *res)
{
  input_stack::push(make_temp_iterator(expr));
  tok.next();
  int success = get_number(res, 'u');
  while (input_stack::get(NULL) != EOF)
    ;
  return success;
}

static void do_register_assignment(const char *s)
{
  const char *p = strchr(s, '=');
  if (!p) {
    char buf[2];
    buf[0] = s[0];
    buf[1] = 0;
    units n;
    if (evaluate_expression(s + 1, &n))
      set_number_reg(buf, n);
  }
  else {
    char *buf = new char[p - s + 1];
    memcpy(buf, s, p - s);
    buf[p - s] = 0;
    units n;
    if (evaluate_expression(p + 1, &n))
      set_number_reg(buf, n);
    a_delete buf;
  }
}

static void set_string(const char *name, const char *value)
{
  macro *m = new macro;
  for (const char *p = value; *p; p++)
    if (!illegal_input_char((unsigned char)*p))
      m->append(*p);
  request_dictionary.define(name, m);
}


static void do_string_assignment(const char *s)
{
  const char *p = strchr(s, '=');
  if (!p) {
    char buf[2];
    buf[0] = s[0];
    buf[1] = 0;
    set_string(buf, s + 1);
  }
  else {
    char *buf = new char[p - s + 1];
    memcpy(buf, s, p - s);
    buf[p - s] = 0;
    set_string(buf, p + 1);
    a_delete buf;
  }
}

struct string_list {
  const char *s;
  string_list *next;
  string_list(const char *ss) : s(ss), next(0) {}
};

static void prepend_string(const char *s, string_list **p)
{
  string_list *l = new string_list(s);
  l->next = *p;
  *p = l;
}

static void add_string(const char *s, string_list **p)
{
  while (*p)
    p = &((*p)->next);
  *p = new string_list(s);
}

void usage(const char *prog)
{
  errprint(
"usage: %1 -abivzCERU -wname -Wname -dcs -ffam -mname -nnum -olist\n"
"       -rcn -Tname -Fdir -Mdir [files...]\n",
	  prog);
  exit(USAGE_EXIT_CODE);
}

int main(int argc, char **argv)
{
  program_name = argv[0];
  static char stderr_buf[BUFSIZ];
  setbuf(stderr, stderr_buf);
  int c;
  string_list *macros = 0;
  string_list *register_assignments = 0;
  string_list *string_assignments = 0;
  int iflag = 0;
  int tflag = 0;
  int fflag = 0;
  int nflag = 0;
  int safer_flag = 1;		// safer by default
  int no_rc = 0;		// don't process troffrc
  int next_page_number;
  opterr = 0;
  hresolution = vresolution = 1;
  while ((c = getopt(argc, argv, "abivw:W:zCEf:m:n:o:r:d:F:M:T:tqs:RU"))
	 != EOF)
    switch(c) {
    case 'v':
      {
	extern const char *version_string;
	fprintf(stderr, "GNU troff version %s\n", version_string);
	fflush(stderr);
	break;
      }
    case 'T':
      device = optarg;
      tflag = 1;
      break;
    case 'C':
      compatible_flag = 1;
      break;
    case 'M':
      macro_path.command_line_dir(optarg);
      break;
    case 'F':
      font::command_line_font_dir(optarg);
      break;
    case 'm':
      add_string(optarg, &macros);
      break;
    case 'E':
      inhibit_errors = 1;
      break;
    case 'R':
      no_rc = 1;
      break;
    case 'w':
      enable_warning(optarg);
      break;
    case 'W':
      disable_warning(optarg);
      break;
    case 'i':
      iflag = 1;
      break;
    case 'b':
      backtrace_flag = 1;
      break;
    case 'a':
      ascii_output_flag = 1;
      break;
    case 'z':
      suppress_output_flag = 1;
      break;
    case 'n':
      if (sscanf(optarg, "%d", &next_page_number) == 1)
	nflag++;
      else
	error("bad page number");
      break;
    case 'o':
      parse_output_page_list(optarg);
      break;
    case 'd':
      if (*optarg == '\0')
	error("`-d' requires non-empty argument");
      else
	add_string(optarg, &string_assignments);
      break;
    case 'r':
      if (*optarg == '\0')
	error("`-r' requires non-empty argument");
      else
	add_string(optarg, &register_assignments);
      break;
    case 'f':
      default_family = symbol(optarg);
      fflag = 1;
      break;
    case 'q':
    case 's':
    case 't':
      // silently ignore these
      break;
    case 'U':
      safer_flag = 0;	// unsafe behaviour
      break;
    case '?':
      usage(argv[0]);
    default:
      assert(0);
    }
  set_string(".T", device);
  init_charset_table();
  if (!font::load_desc())
    fatal("sorry, I can't continue");
  units_per_inch = font::res;
  hresolution = font::hor;
  vresolution = font::vert;
  sizescale = font::sizescale;
  tcommand_flag = font::tcommand;
  if (!fflag && font::family != 0 && *font::family != '\0')
    default_family = symbol(font::family);
  font_size::init_size_table(font::sizes);
  int i;
  int j = 1;
  if (font::style_table) {
    for (i = 0; font::style_table[i]; i++)
      mount_style(j++, symbol(font::style_table[i]));
  }
  for (i = 0; font::font_name_table[i]; i++, j++)
    // In the DESC file a font name of 0 (zero) means leave this
    // position empty.
    if (strcmp(font::font_name_table[i], "0") != 0)
      mount_font(j, symbol(font::font_name_table[i]));
  curdiv = topdiv = new top_level_diversion;
  if (nflag)
    topdiv->set_next_page_number(next_page_number);
  init_input_requests();
  init_env_requests();
  init_div_requests();
#ifdef COLUMN
  init_column_requests();
#endif /* COLUMN */
  init_node_requests();
  number_reg_dictionary.define(".T", new constant_reg(tflag ? "1" : "0"));
  init_registers();
  init_reg_requests();
  init_hyphen_requests();
  init_environments();
  while (string_assignments) {
    do_string_assignment(string_assignments->s);
    string_list *tem = string_assignments;
    string_assignments = string_assignments->next;
    delete tem;
  }
  while (register_assignments) {
    do_register_assignment(register_assignments->s);
    string_list *tem = register_assignments;
    register_assignments = register_assignments->next;
    delete tem;
  }
  if (!no_rc)
    process_startup_file();
  if (safer_flag)
    prepend_string("safer", &macros);
  while (macros) {
    process_macro_file(macros->s);
    string_list *tem = macros;
    macros = macros->next;
    delete tem;
  }
  for (i = optind; i < argc; i++)
    process_input_file(argv[i]);
  if (optind >= argc || iflag)
    process_input_file("-");
  exit_troff();
  return 0;			// not reached
}

void warn_request()
{
  int n;
  if (has_arg() && get_integer(&n)) {
    if (n & ~WARN_TOTAL) {
      warning(WARN_RANGE, "warning mask must be between 0 and %1", WARN_TOTAL);
      n &= WARN_TOTAL;
    }
    warning_mask = n;
  }
  else
    warning_mask = WARN_TOTAL;
  skip_line();
}

static void init_registers()
{
#ifdef LONG_FOR_TIME_T
  long
#else /* not LONG_FOR_TIME_T */
  time_t
#endif /* not LONG_FOR_TIME_T */
    t = time(0);
  // Use struct here to work around misfeature in old versions of g++.
  struct tm *tt = localtime(&t);
  set_number_reg("dw", int(tt->tm_wday + 1));
  set_number_reg("dy", int(tt->tm_mday));
  set_number_reg("mo", int(tt->tm_mon + 1));
  set_number_reg("yr", int(tt->tm_year));
  set_number_reg("$$", getpid());
  number_reg_dictionary.define(".A",
			       new constant_reg(ascii_output_flag
						? "1"
						: "0"));
}

void init_input_requests()
{
  init_request("ds", define_string);
  init_request("as", append_string);
  init_request("de", define_macro);
  init_request("am", append_macro);
  init_request("ig", ignore);
  init_request("rm", remove_macro);
  init_request("rn", rename_macro);
  init_request("if", if_request);
  init_request("ie", if_else_request);
  init_request("el", else_request);
  init_request("so", source);
  init_request("nx", next_file);
  init_request("pm", print_macros);
  init_request("eo", escape_off);
  init_request("ec", set_escape_char);
  init_request("pc", set_page_character);
  init_request("tm", terminal);
  init_request("ex", exit_request);
  init_request("em", end_macro);
  init_request("blm", blank_line_macro);
  init_request("tr", translate);
  init_request("trnt", translate_no_transparent);
  init_request("ab", abort_request);
  init_request("pi", pipe_output);
  init_request("cf", copy_file);
  init_request("sy", system_request);
  init_request("lf", line_file);
  init_request("cflags", char_flags);
  init_request("shift", shift);
  init_request("rd", read_request);
  init_request("cp", compatible);
  init_request("char", define_character);
  init_request("rchar", remove_character);
  init_request("hcode", hyphenation_code);
  init_request("while", while_request);
  init_request("break", while_break_request);
  init_request("continue", while_continue_request);
  init_request("als", alias_macro);
  init_request("backtrace", backtrace_request);
  init_request("chop", chop_macro);
  init_request("substring", substring_macro);
  init_request("asciify", asciify_macro);
  init_request("warn", warn_request);
  init_request("open", open_request);
  init_request("opena", opena_request);
  init_request("close", close_request);
  init_request("write", write_request);
  init_request("writem", write_macro_request);
  init_request("trf", transparent_file);
#ifdef WIDOW_CONTROL
  init_request("fpl", flush_pending_lines);
#endif /* WIDOW_CONTROL */
  init_request("nroff", nroff_request);
  init_request("troff", troff_request);
#ifdef COLUMN
  init_request("vj", vjustify);
#endif /* COLUMN */
  init_request("mso", macro_source);
  init_request("do", do_request);
#ifndef POPEN_MISSING
  init_request("pso", pipe_source);
#endif /* not POPEN_MISSING */
  number_reg_dictionary.define("systat", new variable_reg(&system_status));
  number_reg_dictionary.define("slimit",
			       new variable_reg(&input_stack::limit));
  number_reg_dictionary.define(".$", new nargs_reg);
  number_reg_dictionary.define(".c", new lineno_reg);
  number_reg_dictionary.define("c.", new writable_lineno_reg);
  number_reg_dictionary.define(".F", new filename_reg);
  number_reg_dictionary.define(".C", new constant_int_reg(&compatible_flag));
  number_reg_dictionary.define(".H", new constant_int_reg(&hresolution));
  number_reg_dictionary.define(".V", new constant_int_reg(&vresolution));
  number_reg_dictionary.define(".R", new constant_reg("10000"));
  extern const char *major_version;
  number_reg_dictionary.define(".x", new constant_reg(major_version));
  extern const char *minor_version;
  number_reg_dictionary.define(".y", new constant_reg(minor_version));
  number_reg_dictionary.define(".g", new constant_reg("1"));
  number_reg_dictionary.define(".warn", new constant_int_reg(&warning_mask));
}

object_dictionary request_dictionary(501);

void init_request(const char *s, REQUEST_FUNCP f)
{
  request_dictionary.define(s, new request(f));
}

static request_or_macro *lookup_request(symbol nm)
{
  assert(!nm.is_null());
  request_or_macro *p = (request_or_macro *)request_dictionary.lookup(nm);
  if (p == 0) {
    warning(WARN_MAC, "`%1' not defined", nm.contents());
    p = new macro;
    request_dictionary.define(nm, p);
  }
  return p;
}


node *charinfo_to_node_list(charinfo *ci, const environment *envp)
{
  // Don't interpret character definitions in compatible mode.
  int old_compatible_flag = compatible_flag;
  compatible_flag = 0;
  int old_escape_char = escape_char;
  escape_char = '\\';
  macro *mac = ci->set_macro(0);
  assert(mac != 0);
  environment *oldenv = curenv;
  environment env(envp);
  curenv = &env;
  curenv->set_composite();
  token old_tok = tok;
  input_stack::add_boundary();
  string_iterator *si = new string_iterator(*mac, "composite character", ci->nm);
  input_stack::push(si);
  // we don't use process_input_stack, because we don't want to recognise
  // requests
  for (;;) {
    tok.next();
    if (tok.eof())
      break;
    if (tok.newline()) {
      error("composite character mustn't contain newline");
      while (!tok.eof())
	tok.next();
      break;
    }
    else
      tok.process();
  }
  node *n = curenv->extract_output_line();
  input_stack::remove_boundary();
  ci->set_macro(mac);
  tok = old_tok;
  curenv = oldenv;
  compatible_flag = old_compatible_flag;
  escape_char = old_escape_char;
  return n;
}

static node *read_draw_node()
{
  token start;
  start.next();
  if (!start.delimiter(1)){
    do {
      tok.next();
    } while (tok != start && !tok.newline() && !tok.eof());
  }
  else {
    tok.next();
    if (tok == start)
      error("missing argument");
    else {
      unsigned char type = tok.ch();
      tok.next();
      int maxpoints = 10;
      hvpair *point = new hvpair[maxpoints];
      int npoints = 0;
      int no_last_v = 0;
      int err = 0;
      int i;
      for (i = 0; tok != start; i++) {
	if (i == maxpoints) {
	  hvpair *oldpoint = point;
	  point = new hvpair[maxpoints*2];
	  for (int j = 0; j < maxpoints; j++)
	    point[j] = oldpoint[j];
	  maxpoints *= 2;
	  a_delete oldpoint;
	}
	if (!get_hunits(&point[i].h,
			type == 'f' || type == 't' ? 'u' : 'm')) {
	  err = 1;
	  break;
	}
	++npoints;
	tok.skip();
	point[i].v = V0;
	if (tok == start) {
	  no_last_v = 1;
	  break;
	}
	if (!get_vunits(&point[i].v, 'v')) {
	  err = 1;
	  break;
	}
	tok.skip();
      }
      while (tok != start && !tok.newline() && !tok.eof())
	tok.next();
      if (!err) {
	switch (type) {
	case 'l':
	  if (npoints != 1 || no_last_v) {
	    error("two arguments needed for line");
	    npoints = 1;
	  }
	  break;
	case 'c':
	  if (npoints != 1 || !no_last_v) {
	    error("one argument needed for circle");
	    npoints = 1;
	    point[0].v = V0;
	  }
	  break;
	case 'e':
	  if (npoints != 1 || no_last_v) {
	    error("two arguments needed for ellipse");
	    npoints = 1;
	  }
	  break;
	case 'a':
	  if (npoints != 2 || no_last_v) {
	    error("four arguments needed for arc");
	    npoints = 2;
	  }
	  break;
	case '~':
	  if (no_last_v)
	    error("even number of arguments needed for spline");
	  break;
	default:
	  // silently pass it through
	  break;
	}
	draw_node *dn = new draw_node(type, point, npoints,
				      curenv->get_font_size());
	a_delete point;
	return dn;
      }
      else {
	a_delete point;
      }
    }
  }
  return 0;
}

static struct {
  const char *name;
  int mask;
} warning_table[] = {
  { "char", WARN_CHAR },
  { "range", WARN_RANGE },
  { "break", WARN_BREAK },
  { "delim", WARN_DELIM },
  { "el", WARN_EL },
  { "scale", WARN_SCALE },
  { "number", WARN_NUMBER },
  { "syntax", WARN_SYNTAX },
  { "tab", WARN_TAB },
  { "right-brace", WARN_RIGHT_BRACE },
  { "missing", WARN_MISSING },
  { "input", WARN_INPUT },
  { "escape", WARN_ESCAPE },
  { "space", WARN_SPACE },
  { "font", WARN_FONT },
  { "di", WARN_DI },
  { "mac", WARN_MAC },
  { "reg", WARN_REG },
  { "ig", WARN_IG },
  { "all", WARN_TOTAL & ~(WARN_DI | WARN_MAC | WARN_REG) },
  { "w", WARN_TOTAL },
  { "default", DEFAULT_WARNING_MASK },
};

static int lookup_warning(const char *name)
{
  for (int i = 0;
       i < sizeof(warning_table)/sizeof(warning_table[0]);
       i++)
    if (strcmp(name, warning_table[i].name) == 0)
      return warning_table[i].mask;
  return 0;
}

static void enable_warning(const char *name)
{
  int mask = lookup_warning(name);
  if (mask)
    warning_mask |= mask;
  else
    error("unknown warning `%1'", name);
}

static void disable_warning(const char *name)
{
  int mask = lookup_warning(name);
  if (mask)
    warning_mask &= ~mask;
  else
    error("unknown warning `%1'", name);
}

static void copy_mode_error(const char *format,
			    const errarg &arg1,
			    const errarg &arg2,
			    const errarg &arg3)
{
  if (ignoring) {
    static const char prefix[] = "(in ignored input) ";
    char *s = new char[sizeof(prefix) + strlen(format)];
    strcpy(s, prefix);
    strcat(s, format);
    warning(WARN_IG, s, arg1, arg2, arg3);
    a_delete s;
  }
  else
    error(format, arg1, arg2, arg3);
}

enum error_type { WARNING, ERROR, FATAL };

static void do_error(error_type type, 
		     const char *format, 
		     const errarg &arg1,
		     const errarg &arg2,
		     const errarg &arg3)
{
  const char *filename;
  int lineno;
  if (inhibit_errors && type < FATAL)
    return;
  if (backtrace_flag)
    input_stack::backtrace();
  if (!get_file_line(&filename, &lineno))
    filename = 0;
  if (filename)
    errprint("%1:%2: ", filename, lineno);
  else if (program_name)
    fprintf(stderr, "%s: ", program_name);
  switch (type) {
  case FATAL:
    fputs("fatal error: ", stderr);
    break;
  case ERROR:
    break;
  case WARNING:
    fputs("warning: ", stderr);
    break;
  }
  errprint(format, arg1, arg2, arg3);
  fputc('\n', stderr);
  fflush(stderr);
  if (type == FATAL)
    cleanup_and_exit(1);
}

int warning(warning_type t,
	    const char *format,
	    const errarg &arg1,
	    const errarg &arg2,
	    const errarg &arg3)
{
  if ((t & warning_mask) != 0) {
    do_error(WARNING, format, arg1, arg2, arg3);
    return 1;
  }
  else
    return 0;
}

void error(const char *format, 
	   const errarg &arg1,
	   const errarg &arg2,
	   const errarg &arg3)
{
  do_error(ERROR, format, arg1, arg2, arg3);
}

void fatal(const char *format, 
	   const errarg &arg1,
	   const errarg &arg2,
	   const errarg &arg3)
{
  do_error(FATAL, format, arg1, arg2, arg3);
}

void fatal_with_file_and_line(const char *filename, int lineno,
			      const char *format,
			      const errarg &arg1,
			      const errarg &arg2,
			      const errarg &arg3)
{
  fprintf(stderr, "%s:%d: fatal error: ", filename, lineno);
  errprint(format, arg1, arg2, arg3);
  fputc('\n', stderr);
  fflush(stderr);
  cleanup_and_exit(1);
}

void error_with_file_and_line(const char *filename, int lineno,
			      const char *format,
			      const errarg &arg1,
			      const errarg &arg2,
			      const errarg &arg3)
{
  fprintf(stderr, "%s:%d: error: ", filename, lineno);
  errprint(format, arg1, arg2, arg3);
  fputc('\n', stderr);
  fflush(stderr);
}

dictionary charinfo_dictionary(501);

charinfo *get_charinfo(symbol nm)
{
  void *p = charinfo_dictionary.lookup(nm);
  if (p != 0)
    return (charinfo *)p;
  charinfo *cp = new charinfo(nm);
  (void)charinfo_dictionary.lookup(nm, cp);
  return cp;
}

int charinfo::next_index = 0;

charinfo::charinfo(symbol s)
: nm(s), hyphenation_code(0), translation(0), flags(0), ascii_code(0),
  special_translation(TRANSLATE_NONE), mac(0), not_found(0),
  transparent_translate(1)
{
  index = next_index++;
}

void charinfo::set_hyphenation_code(unsigned char c)
{
  hyphenation_code = c;
}

void charinfo::set_translation(charinfo *ci, int tt)
{
  translation = ci;
  special_translation = TRANSLATE_NONE;
  transparent_translate = tt;
}

void charinfo::set_special_translation(int c, int tt)
{
  special_translation = c;
  translation = 0;
  transparent_translate = tt;
}

void charinfo::set_ascii_code(unsigned char c)
{
  ascii_code = c;
}

macro *charinfo::set_macro(macro *m)
{
  macro *tem = mac;
  mac = m;
  return tem;
}

void charinfo::set_number(int n)
{
  number = n;
  flags |= NUMBERED;
}

int charinfo::get_number()
{
  assert(flags & NUMBERED);
  return number;
}

symbol UNNAMED_SYMBOL("---");

// For numbered characters not between 0 and 255, we make a symbol out
// of the number and store them in this dictionary.

dictionary numbered_charinfo_dictionary(11);

charinfo *get_charinfo_by_number(int n)
{
  static charinfo *number_table[256];

  if (n >= 0 && n < 256) {
    charinfo *ci = number_table[n];
    if (!ci) {
      ci = new charinfo(UNNAMED_SYMBOL);
      ci->set_number(n);
      number_table[n] = ci;
    }
    return ci;
  }
  else {
    symbol ns(itoa(n));
    charinfo *ci = (charinfo *)numbered_charinfo_dictionary.lookup(ns);
    if (!ci) {
      ci = new charinfo(UNNAMED_SYMBOL);
      ci->set_number(n);
      numbered_charinfo_dictionary.lookup(ns, ci);
    }
    return ci;
  }
}

int font::name_to_index(const char *nm)
{
  charinfo *ci;
  if (nm[1] == 0)
    ci = charset_table[nm[0] & 0xff];
  else if (nm[0] == '\\' && nm[2] == 0)
    ci = get_charinfo(symbol(nm + 1));
  else
    ci = get_charinfo(symbol(nm));
  if (ci == 0)
    return -1;
  else
    return ci->get_index();
}

int font::number_to_index(int n)
{
  return get_charinfo_by_number(n)->get_index();
}
