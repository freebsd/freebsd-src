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

#include "eqn.h"
#include "eqn.tab.h"
#include "stringclass.h"
#include "ptable.h"

struct definition {
  char is_macro;
  char is_simple;
  union {
    int tok;
    char *contents;
  };
  definition();
  ~definition();
};

definition::definition() : is_macro(1), is_simple(0)
{
  contents = 0;
}

definition::~definition()
{
  if (is_macro)
    a_delete contents;
}

declare_ptable(definition)
implement_ptable(definition)

PTABLE(definition) macro_table;

static struct {
  const char *name;
  int token;
} token_table[] = {
  { "over", OVER },
  { "smallover", SMALLOVER },
  { "sqrt", SQRT },
  { "sub", SUB },
  { "sup", SUP },
  { "lpile", LPILE },
  { "rpile", RPILE },
  { "cpile", CPILE },
  { "pile", PILE },
  { "left", LEFT },
  { "right", RIGHT },
  { "to", TO },
  { "from", FROM },
  { "size", SIZE },
  { "font", FONT },
  { "roman", ROMAN },
  { "bold", BOLD },
  { "italic", ITALIC },
  { "fat", FAT },
  { "bar", BAR },
  { "under", UNDER },
  { "accent", ACCENT },
  { "uaccent", UACCENT },
  { "above", ABOVE },
  { "fwd", FWD },
  { "back", BACK },
  { "down", DOWN },
  { "up", UP },
  { "matrix", MATRIX },
  { "col", COL },
  { "lcol", LCOL },
  { "rcol", RCOL },
  { "ccol", CCOL },
  { "mark", MARK },
  { "lineup", LINEUP },
  { "space", SPACE },
  { "gfont", GFONT },
  { "gsize", GSIZE },
  { "define", DEFINE },
  { "sdefine", SDEFINE },
  { "ndefine", NDEFINE },
  { "tdefine", TDEFINE },
  { "undef", UNDEF },
  { "ifdef", IFDEF },
  { "include", INCLUDE },
  { "copy", INCLUDE },
  { "delim", DELIM },
  { "chartype", CHARTYPE },
  { "type", TYPE },
  { "vcenter", VCENTER },
  { "set", SET },
  { "opprime", PRIME },
  { "grfont", GRFONT },
  { "gbfont", GBFONT },
  { "split", SPLIT },
  { "nosplit", NOSPLIT },
  { "special", SPECIAL },
};

static struct {
  const char *name;
  const char *def;
} def_table[] = {
  { "ALPHA", "\\(*A" },
  { "BETA", "\\(*B" },
  { "CHI", "\\(*X" },
  { "DELTA", "\\(*D" },
  { "EPSILON", "\\(*E" },
  { "ETA", "\\(*Y" },
  { "GAMMA", "\\(*G" },
  { "IOTA", "\\(*I" },
  { "KAPPA", "\\(*K" },
  { "LAMBDA", "\\(*L" },
  { "MU", "\\(*M" },
  { "NU", "\\(*N" },
  { "OMEGA", "\\(*W" },
  { "OMICRON", "\\(*O" },
  { "PHI", "\\(*F" },
  { "PI", "\\(*P" },
  { "PSI", "\\(*Q" },
  { "RHO", "\\(*R" },
  { "SIGMA", "\\(*S" },
  { "TAU", "\\(*T" },
  { "THETA", "\\(*H" },
  { "UPSILON", "\\(*U" },
  { "XI", "\\(*C" },
  { "ZETA", "\\(*Z" },
  { "Alpha", "\\(*A" },
  { "Beta", "\\(*B" },
  { "Chi", "\\(*X" },
  { "Delta", "\\(*D" },
  { "Epsilon", "\\(*E" },
  { "Eta", "\\(*Y" },
  { "Gamma", "\\(*G" },
  { "Iota", "\\(*I" },
  { "Kappa", "\\(*K" },
  { "Lambda", "\\(*L" },
  { "Mu", "\\(*M" },
  { "Nu", "\\(*N" },
  { "Omega", "\\(*W" },
  { "Omicron", "\\(*O" },
  { "Phi", "\\(*F" },
  { "Pi", "\\(*P" },
  { "Psi", "\\(*Q" },
  { "Rho", "\\(*R" },
  { "Sigma", "\\(*S" },
  { "Tau", "\\(*T" },
  { "Theta", "\\(*H" },
  { "Upsilon", "\\(*U" },
  { "Xi", "\\(*C" },
  { "Zeta", "\\(*Z" },
  { "alpha", "\\(*a" },
  { "beta", "\\(*b" },
  { "chi", "\\(*x" },
  { "delta", "\\(*d" },
  { "epsilon", "\\(*e" },
  { "eta", "\\(*y" },
  { "gamma", "\\(*g" },
  { "iota", "\\(*i" },
  { "kappa", "\\(*k" },
  { "lambda", "\\(*l" },
  { "mu", "\\(*m" },
  { "nu", "\\(*n" },
  { "omega", "\\(*w" },
  { "omicron", "\\(*o" },
  { "phi", "\\(*f" },
  { "pi", "\\(*p" },
  { "psi", "\\(*q" },
  { "rho", "\\(*r" },
  { "sigma", "\\(*s" },
  { "tau", "\\(*t" },
  { "theta", "\\(*h" },
  { "upsilon", "\\(*u" },
  { "xi", "\\(*c" },
  { "zeta", "\\(*z" },
  { "max", "{type \"operator\" roman \"max\"}" },
  { "min", "{type \"operator\" roman \"min\"}" },
  { "lim", "{type \"operator\" roman \"lim\"}" },
  { "sin", "{type \"operator\" roman \"sin\"}" },
  { "cos", "{type \"operator\" roman \"cos\"}" },
  { "tan", "{type \"operator\" roman \"tan\"}" },
  { "sinh", "{type \"operator\" roman \"sinh\"}" },
  { "cosh", "{type \"operator\" roman \"cosh\"}" },
  { "tanh", "{type \"operator\" roman \"tanh\"}" },
  { "arc", "{type \"operator\" roman \"arc\"}" },
  { "log", "{type \"operator\" roman \"log\"}" },
  { "ln", "{type \"operator\" roman \"ln\"}" },
  { "exp", "{type \"operator\" roman \"exp\"}" },
  { "Re", "{type \"operator\" roman \"Re\"}" },
  { "Im", "{type \"operator\" roman \"Im\"}" },
  { "det", "{type \"operator\" roman \"det\"}" },
  { "and", "{roman \"and\"}" },
  { "if", "{roman \"if\"}" },
  { "for", "{roman \"for\"}" },
  { "sum", "{type \"operator\" vcenter size +5 \\(*S}" },
  { "prod", "{type \"operator\" vcenter size +5 \\(*P}" },
  { "int", "{type \"operator\" vcenter size +8 \\(is}" },
  { "union", "{type \"operator\" vcenter size +5 \\(cu}" },
  { "inter", "{type \"operator\" vcenter size +5 \\(ca}" },
  { "times", "type \"binary\" \\(mu" },
  { "ldots", "type \"inner\" { . . . }" },
  { "inf", "\\(if" },
  { "partial", "\\(pd" },
  { "nothing", "\"\"" },
  { "half", "{1 smallover 2}" },
  { "hat_def", "roman \"^\"" },
  { "hat", "accent { hat_def }" },
  { "dot_def", "back 15 \"\\v'-52M'.\\v'52M'\"" },
  { "dot", "accent { dot_def }" },
  { "dotdot_def", "back 25 \"\\v'-52M'..\\v'52M'\"" },
  { "dotdot", "accent { dotdot_def }" },
  { "tilde_def", "\"~\"" },
  { "tilde", "accent { tilde_def }" },
  { "utilde_def", "\"\\v'75M'~\\v'-75M'\"" },
  { "utilde", "uaccent { utilde_def }" },
  { "vec_def", "up 52 size -5 \\(->" },
  { "vec", "accent { vec_def }" },
  { "dyad_def", "up 52 size -5 {\\(<- back 60 \\(->}" },
  { "dyad", "accent { dyad_def }" },
  { "==", "type \"relation\" \\(==" },
  { "!=", "type \"relation\" \\(!=" },
  { "+-", "type \"binary\" \\(+-" },
  { "->", "type \"relation\" \\(->" },
  { "<-", "type \"relation\" \\(<-" },
  { "<<", "{ < back 20 < }" },
  { ">>", "{ > back 20 > }" },
  { "...", "type \"inner\" vcenter { . . . }" },
  { "prime", "'" },
  { "approx", "type \"relation\" \"\\(~=\"" },
  { "grad", "\\(gr" },
  { "del", "\\(gr" },
  { "cdot", "type \"binary\" vcenter ." },
  { "dollar", "$" },
};  

void init_table(const char *device)
{
  int i;
  for (i = 0; i < sizeof(token_table)/sizeof(token_table[0]); i++) {
    definition *def = new definition;
    def->is_macro = 0;
    def->tok = token_table[i].token;
    macro_table.define(token_table[i].name, def);
  }
  for (i = 0; i < sizeof(def_table)/sizeof(def_table[0]); i++) {
    definition *def = new definition;
    def->is_macro = 1;
    def->contents = strsave(def_table[i].def);
    def->is_simple = 1;
    macro_table.define(def_table[i].name, def);
  }
  definition *def = new definition;
  def->is_macro = 1;
  def->contents = strsave("1");
  macro_table.define(device, def);
}

class input {
  input *next;
public:
  input(input *p);
  virtual ~input();
  virtual int get() = 0;
  virtual int peek() = 0;
  virtual int get_location(char **, int *);

  friend int get_char();
  friend int peek_char();
  friend int get_location(char **, int *);
  friend void init_lex(const char *str, const char *filename, int lineno);
};

class file_input : public input {
  FILE *fp;
  char *filename;
  int lineno;
  string line;
  const char *ptr;
  int read_line();
public:
  file_input(FILE *, const char *, input *);
  ~file_input();
  int get();
  int peek();
  int get_location(char **, int *);
};


class macro_input : public input {
  char *s;
  char *p;
public:
  macro_input(const char *, input *);
  ~macro_input();
  int get();
  int peek();
};

class top_input : public macro_input {
  char *filename;
  int lineno;
 public:
  top_input(const char *, const char *, int, input *);
  ~top_input();
  int get();
  int get_location(char **, int *);
};

class argument_macro_input: public input {
  char *s;
  char *p;
  char *ap;
  int argc;
  char *argv[9];
public:
  argument_macro_input(const char *, int, char **, input *);
  ~argument_macro_input();
  int get();
  int peek();
};

input::input(input *x) : next(x)
{
}

input::~input()
{
}

int input::get_location(char **, int *)
{
  return 0;
}

file_input::file_input(FILE *f, const char *fn, input *p)
: input(p), lineno(0), ptr("")
{
  fp = f;
  filename = strsave(fn);
}

file_input::~file_input()
{
  a_delete filename;
  fclose(fp);
}

int file_input::read_line()
{
  for (;;) {
    line.clear();
    lineno++;
    for (;;) {
      int c = getc(fp);
      if (c == EOF)
	break;
      else if (illegal_input_char(c))
	lex_error("illegal input character code %1", c);
      else {
	line += char(c);
	if (c == '\n') 
	  break;
      }
    }
    if (line.length() == 0)
      return 0;
    if (!(line.length() >= 3 && line[0] == '.' && line[1] == 'E'
	  && (line[2] == 'Q' || line[2] == 'N')
	  && (line.length() == 3 || line[3] == ' ' || line[3] == '\n'
	      || compatible_flag))) {
      line += '\0';
      ptr = line.contents();
      return 1;
    }
  }
}

int file_input::get()
{
  if (*ptr != '\0' || read_line())
    return *ptr++ & 0377;
  else
    return EOF;
}

int file_input::peek()
{
  if (*ptr != '\0' || read_line())
    return *ptr;
  else
    return EOF;
}

int file_input::get_location(char **fnp, int *lnp)
{
  *fnp = filename;
  *lnp = lineno;
  return 1;
}

macro_input::macro_input(const char *str, input *x) : input(x)
{
  p = s = strsave(str);
}

macro_input::~macro_input()
{
  a_delete s;
}

int macro_input::get()
{
  if (p == 0 || *p == '\0')
    return EOF;
  else
    return *p++ & 0377;
}

int macro_input::peek()
{
  if (p == 0 || *p == '\0')
    return EOF;
  else
    return *p & 0377;
}

top_input::top_input(const char *str, const char *fn, int ln, input *x)
: macro_input(str, x), lineno(ln)
{
  filename = strsave(fn);
}

top_input::~top_input()
{
  a_delete filename;
}

int top_input::get()
{
  int c = macro_input::get();
  if (c == '\n')
    lineno++;
  return c;
}

int top_input::get_location(char **fnp, int *lnp)
{
  *fnp = filename;
  *lnp = lineno;
  return 1;
}

// Character representing $1.  Must be illegal input character.
#define ARG1 14

argument_macro_input::argument_macro_input(const char *body, int ac, 
					   char **av, input *x)
: input(x), argc(ac), ap(0)
{
  int i;
  for (i = 0; i < argc; i++)
    argv[i] = av[i];
  p = s = strsave(body);
  int j = 0;
  for (i = 0; s[i] != '\0'; i++)
    if (s[i] == '$' && s[i+1] >= '0' && s[i+1] <= '9') {
      if (s[i+1] != '0')
	s[j++] = ARG1 + s[++i] - '1';
    }
    else
      s[j++] = s[i];
  s[j] = '\0';
}


argument_macro_input::~argument_macro_input()
{
  for (int i = 0; i < argc; i++)
    a_delete argv[i];
  a_delete s;
}

int argument_macro_input::get()
{
  if (ap) {
    if (*ap != '\0')
      return *ap++ & 0377;
    ap = 0;
  }
  if (p == 0)
    return EOF;
  while (*p >= ARG1 && *p <= ARG1 + 8) {
    int i = *p++ - ARG1;
    if (i < argc && argv[i] != 0 && argv[i][0] != '\0') {
      ap = argv[i];
      return *ap++ & 0377;
    }
  }
  if (*p == '\0')
    return EOF;
  return *p++ & 0377;
}

int argument_macro_input::peek()
{
  if (ap) {
    if (*ap != '\0')
      return *ap & 0377;
    ap = 0;
  }
  if (p == 0)
    return EOF;
  while (*p >= ARG1 && *p <= ARG1 + 8) {
    int i = *p++ - ARG1;
    if (i < argc && argv[i] != 0 && argv[i][0] != '\0') {
      ap = argv[i];
      return *ap & 0377;
    }
  }
  if (*p == '\0')
    return EOF;
  return *p & 0377;
}

static input *current_input = 0;

/* we insert a newline between input from different levels */

int get_char()
{
  if (current_input == 0)
    return EOF;
  else {
    int c = current_input->get();
    if (c != EOF)
      return c;
    else {
      input *tem = current_input;
      current_input = current_input->next;
      delete tem;
      return '\n';
    }
  }
}

int peek_char()
{
  if (current_input == 0)
    return EOF;
  else {
    int c = current_input->peek();
    if (c != EOF)
      return c;
    else
      return '\n';
  }
}

int get_location(char **fnp, int *lnp)
{
  for (input *p = current_input; p; p = p->next)
    if (p->get_location(fnp, lnp))
      return 1;
  return 0;
}

string token_buffer;
const int NCONTEXT = 4;
string context_ring[NCONTEXT];
int context_index = 0;

void flush_context()
{
  for (int i = 0; i < NCONTEXT; i++)
    context_ring[i] = "";
  context_index = 0;
}

void show_context()
{
  int i = context_index;
  fputs(" context is\n\t", stderr);
  for (;;) {
    int j = (i + 1) % NCONTEXT;
    if (j == context_index) {
      fputs(">>> ", stderr);
      put_string(context_ring[i], stderr);
      fputs(" <<<", stderr);
      break;
    }
    else if (context_ring[i].length() > 0) {
      put_string(context_ring[i], stderr);
      putc(' ', stderr);
    }
    i = j;
  }
  putc('\n', stderr);
}

void add_context(const string &s)
{
  context_ring[context_index] = s;
  context_index = (context_index + 1) % NCONTEXT;
}

void add_context(char c)
{
  context_ring[context_index] = c;
  context_index = (context_index + 1) % NCONTEXT;
}

void add_quoted_context(const string &s)
{
  string &r = context_ring[context_index];
  r = '"';
  for (int i = 0; i < s.length(); i++)
    if (s[i] == '"')
      r += "\\\"";
    else
      r += s[i];
  r += '"';
  context_index = (context_index + 1) % NCONTEXT;
}

void init_lex(const char *str, const char *filename, int lineno)
{
 while (current_input != 0) {
    input *tem = current_input;
    current_input = current_input->next;
    delete tem;
  }
  current_input = new top_input(str, filename, lineno, 0);
  flush_context();
}


void get_delimited_text()
{
  char *filename;
  int lineno;
  int got_location = get_location(&filename, &lineno);
  int start = get_char();
  while (start == ' ' || start == '\t' || start == '\n')
    start = get_char();
  token_buffer.clear();
  if (start == EOF) {
    if (got_location)
      error_with_file_and_line(filename, lineno,
			       "end of input while defining macro");
    else
      error("end of input while defining macro");
    return;
  }
  for (;;) {
    int c = get_char();
    if (c == EOF) {
      if (got_location)
	error_with_file_and_line(filename, lineno,
				 "end of input while defining macro");
      else
	error("end of input while defining macro");
      add_context(start + token_buffer);
      return;
    }
    if (c == start)
      break;
    token_buffer += char(c);
  }
  add_context(start + token_buffer + start);
}

void interpolate_macro_with_args(const char *body)
{
  char *argv[9];
  int argc = 0;
  int i;
  for (i = 0; i < 9; i++)
    argv[i] = 0;
  int level = 0;
  int c;
  do {
    token_buffer.clear();
    for (;;) {
      c = get_char();
      if (c == EOF) {
	lex_error("end of input while scanning macro arguments");
	break;
      }
      if (level == 0 && (c == ',' || c == ')')) {
	if (token_buffer.length() > 0) {
	  token_buffer +=  '\0';
	  argv[argc] = strsave(token_buffer.contents());
	}
	// for `foo()', argc = 0
	if (argc > 0 || c != ')' || i > 0)
	  argc++;
	break;
      }
      token_buffer += char(c);
      if (c == '(')
	level++;
      else if (c == ')')
	level--;
    }
  } while (c != ')' && c != EOF);
  current_input = new argument_macro_input(body, argc, argv, current_input);
}

/* If lookup flag is non-zero the token will be looked up to see
if it is macro. If it's 1, it will looked up to see if it's a token.
*/

int get_token(int lookup_flag = 0)
{
  for (;;) {
    int c = get_char();
    while (c == ' ' || c == '\n')
      c = get_char();
    switch (c) {
    case EOF:
      {
	add_context("end of input");
      }
      return 0;
    case '"':
      {
	int quoted = 0;
	token_buffer.clear();
	for (;;) {
	  c = get_char();
	  if (c == EOF) {
	    lex_error("missing \"");
	    break;
	  }
	  else if (c == '\n') {
	    lex_error("newline before end of quoted text");
	    break;
	  }
	  else if (c == '"') {
	    if (!quoted)
	      break;
	    token_buffer[token_buffer.length() - 1] = '"';
	    quoted = 0;
	  }
	  else {
	    token_buffer += c;
	    quoted = quoted ? 0 : c == '\\';
	  }
	}
      }
      add_quoted_context(token_buffer);
      return QUOTED_TEXT;
    case '{':
    case '}':
    case '^':
    case '~':
    case '\t':
      add_context(c);
      return c;
    default:
      {
	int break_flag = 0;
	int quoted = 0;
	token_buffer.clear();
	if (c == '\\')
	  quoted = 1;
	else
	  token_buffer += c;
	int done = 0;
	while (!done) {
	  c = peek_char();
	  if (!quoted && lookup_flag != 0 && c == '(') {
	    token_buffer += '\0';
	    definition *def = macro_table.lookup(token_buffer.contents());
	    if (def && def->is_macro && !def->is_simple) {
	      (void)get_char();	// skip initial '('
	      interpolate_macro_with_args(def->contents);
	      break_flag = 1;
	      break;
	    }
	    token_buffer.set_length(token_buffer.length() - 1);
	  }
	  if (quoted) {
	    quoted = 0;
	    switch (c) {
	    case EOF:
	      lex_error("`\\' ignored at end of equation");
	      done = 1;
	      break;
	    case '\n':
	      lex_error("`\\' ignored because followed by newline");
	      done = 1;
	      break;
	    case '\t':
	      lex_error("`\\' ignored because followed by tab");
	      done = 1;
	      break;
	    case '"':
	      (void)get_char();
	      token_buffer += '"';
	      break;
	    default:
	      (void)get_char();
	      token_buffer += '\\';
	      token_buffer += c;
	      break;
	    }
	  }
	  else {
	    switch (c) {
	    case EOF:
	    case '{':
	    case '}':
	    case '^':
	    case '~':
	    case '"':
	    case ' ':
	    case '\t':
	    case '\n':
	      done = 1;
	      break;
	    case '\\':
	      (void)get_char();
	      quoted = 1;
	      break;
	    default:
	      (void)get_char();
	      token_buffer += char(c);
	      break;
	    }
	  }
	}
	if (break_flag || token_buffer.length() == 0)
	  break;
	if (lookup_flag != 0) {
	  token_buffer += '\0';
	  definition *def = macro_table.lookup(token_buffer.contents());
	  token_buffer.set_length(token_buffer.length() - 1);
	  if (def) {
	    if (def->is_macro) {
	      current_input = new macro_input(def->contents, current_input);
	      break;
	    }
	    else if (lookup_flag == 1) {
	      add_context(token_buffer);
	      return def->tok;
	    }
	  }
	}
	add_context(token_buffer);
	return TEXT;
      }
    }
  }
}

void do_include()
{
  int t = get_token(2);
  if (t != TEXT && t != QUOTED_TEXT) {
    lex_error("bad filename for include");
    return;
  }
  token_buffer += '\0';
  const char *filename = token_buffer.contents();
  errno = 0;
  FILE *fp = fopen(filename, "r");
  if (fp == 0) {
    lex_error("can't open included file `%1'", filename);
    return;
  }
  current_input = new file_input(fp, filename, current_input);
}

void ignore_definition()
{
  int t = get_token();
  if (t != TEXT) {
    lex_error("bad definition");
    return;
  }
  get_delimited_text();
}

void do_definition(int is_simple)
{
  int t = get_token();
  if (t != TEXT) {
    lex_error("bad definition");
    return;
  }
  token_buffer += '\0';
  const char *name = token_buffer.contents();
  definition *def = macro_table.lookup(name);
  if (def == 0) {
    def = new definition;
    macro_table.define(name, def);
  }
  else if (def->is_macro) {
    a_delete def->contents;
  }
  get_delimited_text();
  token_buffer += '\0';
  def->is_macro = 1;
  def->contents = strsave(token_buffer.contents());
  def->is_simple = is_simple;
}

void do_undef()
{
  int t = get_token();
  if (t != TEXT) {
    lex_error("bad undef command");
    return;
  }
  token_buffer += '\0';
  macro_table.define(token_buffer.contents(), 0);
}

void do_gsize()
{
  int t = get_token(2);
  if (t != TEXT && t != QUOTED_TEXT) {
    lex_error("bad argument to gsize command");
    return;
  }
  token_buffer += '\0';
  if (!set_gsize(token_buffer.contents()))
    lex_error("invalid size `%1'", token_buffer.contents());
}

void do_gfont()
{
  int t = get_token(2);
  if (t != TEXT && t != QUOTED_TEXT) {
    lex_error("bad argument to gfont command");
    return;
  }
  token_buffer += '\0';
  set_gfont(token_buffer.contents());
}

void do_grfont()
{
  int t = get_token(2);
  if (t != TEXT && t != QUOTED_TEXT) {
    lex_error("bad argument to grfont command");
    return;
  }
  token_buffer += '\0';
  set_grfont(token_buffer.contents());
}

void do_gbfont()
{
  int t = get_token(2);
  if (t != TEXT && t != QUOTED_TEXT) {
    lex_error("bad argument to gbfont command");
    return;
  }
  token_buffer += '\0';
  set_gbfont(token_buffer.contents());
}

void do_space()
{
  int t = get_token(2);
  if (t != TEXT && t != QUOTED_TEXT) {
    lex_error("bad argument to space command");
    return;
  }
  token_buffer += '\0';
  char *ptr;
  long n = strtol(token_buffer.contents(), &ptr, 10);
  if (n == 0 && ptr == token_buffer.contents())
    lex_error("bad argument `%1' to space command", token_buffer.contents());
  else
    set_space(int(n));
}

void do_ifdef()
{
  int t = get_token();
  if (t != TEXT) {
    lex_error("bad ifdef");
    return;
  }
  token_buffer += '\0';
  definition *def = macro_table.lookup(token_buffer.contents());
  int result = def && def->is_macro && !def->is_simple;
  get_delimited_text();
  if (result) {
    token_buffer += '\0';
    current_input = new macro_input(token_buffer.contents(), current_input);
  }
}

void do_delim()
{
  int c = get_char();
  while (c == ' ' || c == '\n')
    c = get_char();
  int d;
  if (c == EOF || (d = get_char()) == EOF)
    lex_error("end of file while reading argument to `delim'");
  else {
    if (c == 'o' && d == 'f' && peek_char() == 'f') {
      (void)get_char();
      start_delim = end_delim = '\0';
    }
    else {
      start_delim = c;
      end_delim = d;
    }
  }
}

void do_chartype()
{
  int t = get_token(2);
  if (t != TEXT && t != QUOTED_TEXT) {
    lex_error("bad chartype");
    return;
  }
  token_buffer += '\0';
  string type = token_buffer;
  t = get_token();
  if (t != TEXT && t != QUOTED_TEXT) {
    lex_error("bad chartype");
    return;
  }
  token_buffer += '\0';
  set_char_type(type.contents(), strsave(token_buffer.contents()));
}

void do_set()
{
  int t = get_token(2);
  if (t != TEXT && t != QUOTED_TEXT) {
    lex_error("bad set");
    return;
  }
  token_buffer += '\0';
  string param = token_buffer;
  t = get_token();
  if (t != TEXT && t != QUOTED_TEXT) {
    lex_error("bad set");
    return;
  }
  token_buffer += '\0';
  int n;
  if (sscanf(&token_buffer[0], "%d", &n) != 1) {
    lex_error("bad number `%1'", token_buffer.contents());
    return;
  }
  set_param(param.contents(), n);
}

int yylex()
{
  for (;;) {
    int tk = get_token(1);
    switch(tk) {
    case UNDEF:
      do_undef();
      break;
    case SDEFINE:
      do_definition(1);
      break;
    case DEFINE:
      do_definition(0);
      break;
    case TDEFINE:
      if (!nroff)
	do_definition(0);
      else
	ignore_definition();
      break;
    case NDEFINE:
      if (nroff)
	do_definition(0);
      else
	ignore_definition();
      break;
    case GSIZE:
      do_gsize();
      break;
    case GFONT:
      do_gfont();
      break;
    case GRFONT:
      do_grfont();
      break;
    case GBFONT:
      do_gbfont();
      break;
    case SPACE:
      do_space();
      break;
    case INCLUDE:
      do_include();
      break;
    case IFDEF:
      do_ifdef();
      break;
    case DELIM:
      do_delim();
      break;
    case CHARTYPE:
      do_chartype();
      break;
    case SET:
      do_set();
      break;
    case QUOTED_TEXT:
    case TEXT:
      token_buffer += '\0';
      yylval.str = strsave(token_buffer.contents());
      // fall through
    default:
      return tk;
    }
  }
}

void lex_error(const char *message,
	       const errarg &arg1,
	       const errarg &arg2,
	       const errarg &arg3)
{
  char *filename;
  int lineno;
  if (!get_location(&filename, &lineno))
    error(message, arg1, arg2, arg3);
  else
    error_with_file_and_line(filename, lineno, message, arg1, arg2, arg3);
}

void yyerror(const char *s)
{
  char *filename;
  int lineno;
  if (!get_location(&filename, &lineno))
    error(s);
  else
    error_with_file_and_line(filename, lineno, s);
  show_context();
}

