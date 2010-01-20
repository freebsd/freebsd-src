// -*- C++ -*-
/* Copyright (C) 1989, 1990, 1991, 1992, 2000, 2002, 2003, 2004
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

#include "pic.h"
#include "ptable.h"
#include "object.h"
#include "pic_tab.h"

declare_ptable(char)
implement_ptable(char)

PTABLE(char) macro_table;

class macro_input : public input {
  char *s;
  char *p;
public:
  macro_input(const char *);
  ~macro_input();
  int get();
  int peek();
};

class argument_macro_input : public input {
  char *s;
  char *p;
  char *ap;
  int argc;
  char *argv[9];
public:
  argument_macro_input(const char *, int, char **);
  ~argument_macro_input();
  int get();
  int peek();
};

input::input() : next(0)
{
}

input::~input()
{
}

int input::get_location(const char **, int *)
{
  return 0;
}

file_input::file_input(FILE *f, const char *fn)
: fp(f), filename(fn), lineno(0), ptr("")
{
}

file_input::~file_input()
{
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
      else if (invalid_input_char(c))
	lex_error("invalid input character code %1", c);
      else {
	line += char(c);
	if (c == '\n') 
	  break;
      }
    }
    if (line.length() == 0)
      return 0;
    if (!(line.length() >= 3 && line[0] == '.' && line[1] == 'P'
	  && (line[2] == 'S' || line[2] == 'E' || line[2] == 'F')
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
    return (unsigned char)*ptr++;
  else
    return EOF;
}

int file_input::peek()
{
  if (*ptr != '\0' || read_line())
    return (unsigned char)*ptr;
  else
    return EOF;
}

int file_input::get_location(const char **fnp, int *lnp)
{
  *fnp = filename;
  *lnp = lineno;
  return 1;
}

macro_input::macro_input(const char *str)
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
    return (unsigned char)*p++;
}

int macro_input::peek()
{
  if (p == 0 || *p == '\0')
    return EOF;
  else
    return (unsigned char)*p;
}

// Character representing $1.  Must be invalid input character.
#define ARG1 14

char *process_body(const char *body)
{
  char *s = strsave(body);
  int j = 0;
  for (int i = 0; s[i] != '\0'; i++)
    if (s[i] == '$' && s[i+1] >= '0' && s[i+1] <= '9') {
      if (s[i+1] != '0')
	s[j++] = ARG1 + s[++i] - '1';
    }
    else
      s[j++] = s[i];
  s[j] = '\0';
  return s;
}


argument_macro_input::argument_macro_input(const char *body, int ac, char **av)
: ap(0), argc(ac)
{
  for (int i = 0; i < argc; i++)
    argv[i] = av[i];
  p = s = process_body(body);
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
      return (unsigned char)*ap++;
    ap = 0;
  }
  if (p == 0)
    return EOF;
  while (*p >= ARG1 && *p <= ARG1 + 8) {
    int i = *p++ - ARG1;
    if (i < argc && argv[i] != 0 && argv[i][0] != '\0') {
      ap = argv[i];
      return (unsigned char)*ap++;
    }
  }
  if (*p == '\0')
    return EOF;
  return (unsigned char)*p++;
}

int argument_macro_input::peek()
{
  if (ap) {
    if (*ap != '\0')
      return (unsigned char)*ap;
    ap = 0;
  }
  if (p == 0)
    return EOF;
  while (*p >= ARG1 && *p <= ARG1 + 8) {
    int i = *p++ - ARG1;
    if (i < argc && argv[i] != 0 && argv[i][0] != '\0') {
      ap = argv[i];
      return (unsigned char)*ap;
    }
  }
  if (*p == '\0')
    return EOF;
  return (unsigned char)*p;
}

class input_stack {
  static input *current_input;
  static int bol_flag;
public:
  static void push(input *);
  static void clear();
  static int get_char();
  static int peek_char();
  static int get_location(const char **fnp, int *lnp);
  static void push_back(unsigned char c, int was_bol = 0);
  static int bol();
};

input *input_stack::current_input = 0;
int input_stack::bol_flag = 0;

inline int input_stack::bol()
{
  return bol_flag;
}

void input_stack::clear()
{
  while (current_input != 0) {
    input *tem = current_input;
    current_input = current_input->next;
    delete tem;
  }
  bol_flag = 1;
}

void input_stack::push(input *in)
{
  in->next = current_input;
  current_input = in;
}

void lex_init(input *top)
{
  input_stack::clear();
  input_stack::push(top);
}

void lex_cleanup()
{
  while (input_stack::get_char() != EOF)
    ;
}

int input_stack::get_char()
{
  while (current_input != 0) {
    int c = current_input->get();
    if (c != EOF) {
      bol_flag = c == '\n';
      return c;
    }
    // don't pop the top-level input off the stack
    if (current_input->next == 0)
      return EOF;
    input *tem = current_input;
    current_input = current_input->next;
    delete tem;
  }
  return EOF;
}

int input_stack::peek_char()
{
  while (current_input != 0) {
    int c = current_input->peek();
    if (c != EOF)
      return c;
    if (current_input->next == 0)
      return EOF;
    input *tem = current_input;
    current_input = current_input->next;
    delete tem;
  }
  return EOF;
}

class char_input : public input {
  int c;
public:
  char_input(int);
  int get();
  int peek();
};

char_input::char_input(int n) : c((unsigned char)n)
{
}

int char_input::get()
{
  int n = c;
  c = EOF;
  return n;
}

int char_input::peek()
{
  return c;
}

void input_stack::push_back(unsigned char c, int was_bol)
{
  push(new char_input(c));
  bol_flag = was_bol;
}

int input_stack::get_location(const char **fnp, int *lnp)
{
  for (input *p = current_input; p; p = p->next)
    if (p->get_location(fnp, lnp))
      return 1;
  return 0;
}

string context_buffer;

string token_buffer;
double token_double;
int token_int;

void interpolate_macro_with_args(const char *body)
{
  char *argv[9];
  int argc = 0;
  int i;
  for (i = 0; i < 9; i++)
    argv[i] = 0;
  int level = 0;
  int c;
  enum { NORMAL, IN_STRING, IN_STRING_QUOTED } state = NORMAL;
  do {
    token_buffer.clear();
    for (;;) {
      c = input_stack::get_char();
      if (c == EOF) {
	lex_error("end of input while scanning macro arguments");
	break;
      }
      if (state == NORMAL && level == 0 && (c == ',' || c == ')')) {
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
      switch (state) {
      case NORMAL:
	if (c == '"')
	  state = IN_STRING;
	else if (c == '(')
	  level++;
	else if (c == ')')
	  level--;
	break;
      case IN_STRING:
	if (c == '"')
	  state = NORMAL;
	else if (c == '\\')
	  state = IN_STRING_QUOTED;
	break;
      case IN_STRING_QUOTED:
	state = IN_STRING;
	break;
      }
    }
  } while (c != ')' && c != EOF);
  input_stack::push(new argument_macro_input(body, argc, argv));
}

static int docmp(const char *s1, int n1, const char *s2, int n2)
{
  if (n1 < n2) {
    int r = memcmp(s1, s2, n1);
    return r ? r : -1;
  }
  else if (n1 > n2) {
    int r = memcmp(s1, s2, n2);
    return r ? r : 1;
  }
  else
    return memcmp(s1, s2, n1);
}

int lookup_keyword(const char *str, int len)
{
  static struct keyword {
    const char *name;
    int token;
  } table[] = {
    { "Here", HERE },
    { "above", ABOVE },
    { "aligned", ALIGNED },
    { "and", AND },
    { "arc", ARC },
    { "arrow", ARROW },
    { "at", AT },
    { "atan2", ATAN2 },
    { "below", BELOW },
    { "between", BETWEEN },
    { "bottom", BOTTOM },
    { "box", BOX },
    { "by", BY },
    { "ccw", CCW },
    { "center", CENTER },
    { "chop", CHOP },
    { "circle", CIRCLE },
    { "color", COLORED },
    { "colored", COLORED },
    { "colour", COLORED },
    { "coloured", COLORED },
    { "command", COMMAND },
    { "copy", COPY },
    { "cos", COS },
    { "cw", CW },
    { "dashed", DASHED },
    { "define", DEFINE },
    { "diam", DIAMETER },
    { "diameter", DIAMETER },
    { "do", DO },
    { "dotted", DOTTED },
    { "down", DOWN },
    { "east", EAST },
    { "ellipse", ELLIPSE },
    { "else", ELSE },
    { "end", END },
    { "exp", EXP },
    { "figname", FIGNAME },
    { "fill", FILL },
    { "filled", FILL },
    { "for", FOR },
    { "from", FROM },
    { "height", HEIGHT },
    { "ht", HEIGHT },
    { "if", IF },
    { "int", INT },
    { "invis", INVISIBLE },
    { "invisible", INVISIBLE },
    { "last", LAST },
    { "left", LEFT },
    { "line", LINE },
    { "ljust", LJUST },
    { "log", LOG },
    { "lower", LOWER },
    { "max", K_MAX },
    { "min", K_MIN },
    { "move", MOVE },
    { "north", NORTH },
    { "of", OF },
    { "outline", OUTLINED },
    { "outlined", OUTLINED },
    { "plot", PLOT },
    { "print", PRINT },
    { "rad", RADIUS },
    { "radius", RADIUS },
    { "rand", RAND },
    { "reset", RESET },
    { "right", RIGHT },
    { "rjust", RJUST },
    { "same", SAME },
    { "sh", SH },
    { "shaded", SHADED },
    { "sin", SIN },
    { "solid", SOLID },
    { "south", SOUTH },
    { "spline", SPLINE },
    { "sprintf", SPRINTF },
    { "sqrt", SQRT },
    { "srand", SRAND },
    { "start", START },
    { "the", THE },
    { "then", THEN },
    { "thick", THICKNESS },
    { "thickness", THICKNESS },
    { "thru", THRU },
    { "to", TO },
    { "top", TOP },
    { "undef", UNDEF },
    { "until", UNTIL },
    { "up", UP },
    { "upper", UPPER },
    { "way", WAY },
    { "west", WEST },
    { "wid", WIDTH },
    { "width", WIDTH },
    { "with", WITH },
  };
  
  const keyword *start = table;
  const keyword *end = table + sizeof(table)/sizeof(table[0]);
  while (start < end) {
    // start <= target < end
    const keyword *mid = start + (end - start)/2;
    
    int cmp = docmp(str, len, mid->name, strlen(mid->name));
    if (cmp == 0)
      return mid->token;
    if (cmp < 0)
      end = mid;
    else
      start = mid + 1;
  }
  return 0;
}

int get_token_after_dot(int c)
{
  // get_token deals with the case where c is a digit
  switch (c) {
  case 'h':
    input_stack::get_char();
    c = input_stack::peek_char();
    if (c == 't') {
      input_stack::get_char();
      context_buffer = ".ht";
      return DOT_HT;
    }
    else if (c == 'e') {
      input_stack::get_char();
      c = input_stack::peek_char();
      if (c == 'i') {
	input_stack::get_char();
	c = input_stack::peek_char();
	if (c == 'g') {
	  input_stack::get_char();
	  c = input_stack::peek_char();
	  if (c == 'h') {
	    input_stack::get_char();
	    c = input_stack::peek_char();
	    if (c == 't') {
	      input_stack::get_char();
	      context_buffer = ".height";
	      return DOT_HT;
	    }
	    input_stack::push_back('h');
	  }
	  input_stack::push_back('g');
	}
	input_stack::push_back('i');
      }
      input_stack::push_back('e');
    }
    input_stack::push_back('h');
    return '.';
  case 'x':
    input_stack::get_char();
    context_buffer = ".x";
    return DOT_X;
  case 'y':
    input_stack::get_char();
    context_buffer = ".y";
    return DOT_Y;
  case 'c':
    input_stack::get_char();
    c = input_stack::peek_char();
    if (c == 'e') {
      input_stack::get_char();
      c = input_stack::peek_char();
      if (c == 'n') {
	input_stack::get_char();
	c = input_stack::peek_char();
	if (c == 't') {
	  input_stack::get_char();
	  c = input_stack::peek_char();
	  if (c == 'e') {
	    input_stack::get_char();
	    c = input_stack::peek_char();
	    if (c == 'r') {
	      input_stack::get_char();
	      context_buffer = ".center";
	      return DOT_C;
	    }
	    input_stack::push_back('e');
	  }
	  input_stack::push_back('t');
	}
	input_stack::push_back('n');
      }
      input_stack::push_back('e');
    }
    context_buffer = ".c";
    return DOT_C;
  case 'n':
    input_stack::get_char();
    c = input_stack::peek_char();
    if (c == 'e') {
      input_stack::get_char();
      context_buffer = ".ne";
      return DOT_NE;
    }
    else if (c == 'w') {
      input_stack::get_char();
      context_buffer = ".nw";
      return DOT_NW;
    }
    else {
      context_buffer = ".n";
      return DOT_N;
    }
    break;
  case 'e':
    input_stack::get_char();
    c = input_stack::peek_char();
    if (c == 'n') {
      input_stack::get_char();
      c = input_stack::peek_char();
      if (c == 'd') {
	input_stack::get_char();
	context_buffer = ".end";
	return DOT_END;
      }
      input_stack::push_back('n');
      context_buffer = ".e";
      return DOT_E;
    }
    context_buffer = ".e";
    return DOT_E;
  case 'w':
    input_stack::get_char();
    c = input_stack::peek_char();
    if (c == 'i') {
      input_stack::get_char();
      c = input_stack::peek_char();
      if (c == 'd') {
	input_stack::get_char();
	c = input_stack::peek_char();
	if (c == 't') {
	  input_stack::get_char();
	  c = input_stack::peek_char();
	  if (c == 'h') {
	    input_stack::get_char();
	    context_buffer = ".width";
	    return DOT_WID;
	  }
	  input_stack::push_back('t');
	}
	context_buffer = ".wid";
	return DOT_WID;
      }
      input_stack::push_back('i');
    }
    context_buffer = ".w";
    return DOT_W;
  case 's':
    input_stack::get_char();
    c = input_stack::peek_char();
    if (c == 'e') {
      input_stack::get_char();
      context_buffer = ".se";
      return DOT_SE;
    }
    else if (c == 'w') {
      input_stack::get_char();
      context_buffer = ".sw";
      return DOT_SW;
    }
    else {
      if (c == 't') {
	input_stack::get_char();
	c = input_stack::peek_char();
	if (c == 'a') {
	  input_stack::get_char();
	  c = input_stack::peek_char();
	  if (c == 'r') {
	    input_stack::get_char();
	    c = input_stack::peek_char();
	    if (c == 't') {
	      input_stack::get_char();
	      context_buffer = ".start";
	      return DOT_START;
	    }
	    input_stack::push_back('r');
	  }
	  input_stack::push_back('a');
	}
	input_stack::push_back('t');
      }
      context_buffer = ".s";
      return DOT_S;
    }
    break;
  case 't':
    input_stack::get_char();
    c = input_stack::peek_char();
    if (c == 'o') {
      input_stack::get_char();
      c = input_stack::peek_char();
      if (c == 'p') {
	input_stack::get_char();
	context_buffer = ".top";
	return DOT_N;
      }
      input_stack::push_back('o');
    }
    context_buffer = ".t";
    return DOT_N;
  case 'l':
    input_stack::get_char();
    c = input_stack::peek_char();
    if (c == 'e') {
      input_stack::get_char();
      c = input_stack::peek_char();
      if (c == 'f') {
	input_stack::get_char();
	c = input_stack::peek_char();
	if (c == 't') {
	  input_stack::get_char();
	  context_buffer = ".left";
	  return DOT_W;
	}
	input_stack::push_back('f');
      }
      input_stack::push_back('e');
    }
    context_buffer = ".l";
    return DOT_W;
  case 'r':
    input_stack::get_char();
    c = input_stack::peek_char();
    if (c == 'a') {
      input_stack::get_char();
      c = input_stack::peek_char();
      if (c == 'd') {
	input_stack::get_char();
	context_buffer = ".rad";
	return DOT_RAD;
      }
      input_stack::push_back('a');
    }
    else if (c == 'i') {
      input_stack::get_char();
      c = input_stack::peek_char();
      if (c == 'g') {
	input_stack::get_char();
	c = input_stack::peek_char();
	if (c == 'h') {
	  input_stack::get_char();
	  c = input_stack::peek_char();
	  if (c == 't') {
	    input_stack::get_char();
	    context_buffer = ".right";
	    return DOT_E;
	  }
	  input_stack::push_back('h');
	}
	input_stack::push_back('g');
      }
      input_stack::push_back('i');
    }
    context_buffer = ".r";
    return DOT_E;
  case 'b':
    input_stack::get_char();
    c = input_stack::peek_char();
    if (c == 'o') {
      input_stack::get_char();
      c = input_stack::peek_char();
      if (c == 't') {
	input_stack::get_char();
	c = input_stack::peek_char();
	if (c == 't') {
	  input_stack::get_char();
	  c = input_stack::peek_char();
	  if (c == 'o') {
	    input_stack::get_char();
	    c = input_stack::peek_char();
	    if (c == 'm') {
	      input_stack::get_char();
	      context_buffer = ".bottom";
	      return DOT_S;
	    }
	    input_stack::push_back('o');
	  }
	  input_stack::push_back('t');
	}
	context_buffer = ".bot";
	return DOT_S;
      }
      input_stack::push_back('o');
    }
    context_buffer = ".b";
    return DOT_S;
  default:
    context_buffer = '.';
    return '.';
  }
}

int get_token(int lookup_flag)
{
  context_buffer.clear();
  for (;;) {
    int n = 0;
    int bol = input_stack::bol();
    int c = input_stack::get_char();
    if (bol && c == command_char) {
      token_buffer.clear();
      token_buffer += c;
      // the newline is not part of the token
      for (;;) {
	c = input_stack::peek_char();
	if (c == EOF || c == '\n')
	  break;
	input_stack::get_char();
	token_buffer += char(c);
      }
      context_buffer = token_buffer;
      return COMMAND_LINE;
    }
    switch (c) {
    case EOF:
      return EOF;
    case ' ':
    case '\t':
      break;
    case '\\':
      {
	int d = input_stack::peek_char();
	if (d != '\n') {
	  context_buffer = '\\';
	  return '\\';
	}
	input_stack::get_char();
	break;
      }
    case '#':
      do {
	c = input_stack::get_char();
      } while (c != '\n' && c != EOF);
      if (c == '\n')
	context_buffer = '\n';
      return c;
    case '"':
      context_buffer = '"';
      token_buffer.clear();
      for (;;) {
	c = input_stack::get_char();
	if (c == '\\') {
	  context_buffer += '\\';
	  c = input_stack::peek_char();
	  if (c == '"') {
	    input_stack::get_char();
	    token_buffer += '"';
	    context_buffer += '"';
	  }
	  else
	    token_buffer += '\\';
	}
	else if (c == '\n') {
	  error("newline in string");
	  break;
	}
	else if (c == EOF) {
	  error("missing `\"'");
	  break;
	}
	else if (c == '"') {
	  context_buffer += '"';
	  break;
	}
	else {
	  context_buffer += char(c);
	  token_buffer += char(c);
	}
      }
      return TEXT;
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
	int overflow = 0;
	n = 0;
	for (;;) {
	  if (n > (INT_MAX - 9)/10) {
	    overflow = 1;
	    break;
	  }
	  n *= 10;
	  n += c - '0';
	  context_buffer += char(c);
	  c = input_stack::peek_char();
	  if (c == EOF || !csdigit(c))
	    break;
	  c = input_stack::get_char();
	}
	token_double = n;
	if (overflow) {
	  for (;;) {
	    token_double *= 10.0;
	    token_double += c - '0';
	    context_buffer += char(c);
	    c = input_stack::peek_char();
	    if (c == EOF || !csdigit(c))
	      break;
	    c = input_stack::get_char();
	  }
	  // if somebody asks for 1000000000000th, we will silently
	  // give them INT_MAXth
	  double temp = token_double; // work around gas 1.34/sparc bug
	  if (token_double > INT_MAX)
	    n = INT_MAX;
	  else
	    n = int(temp);
	}
      }
      switch (c) {
      case 'i':
      case 'I':
	context_buffer += char(c);
	input_stack::get_char();
	return NUMBER;
      case '.':
	{
	  context_buffer += '.';
	  input_stack::get_char();
	got_dot:
	  double factor = 1.0;
	  for (;;) {
	    c = input_stack::peek_char();
	    if (c == EOF || !csdigit(c))
	      break;
	    input_stack::get_char();
	    context_buffer += char(c);
	    factor /= 10.0;
	    if (c != '0')
	      token_double += factor*(c - '0');
	  }
	  if (c != 'e' && c != 'E') {
	    if (c == 'i' || c == 'I') {
	      context_buffer += char(c);
	      input_stack::get_char();
	    }
	    return NUMBER;
	  }
	}
	// fall through
      case 'e':
      case 'E':
	{
	  int echar = c;
	  input_stack::get_char();
	  c = input_stack::peek_char();
	  int sign = '+';
	  if (c == '+' || c == '-') {
	    sign = c;
	    input_stack::get_char();
	    c = input_stack::peek_char();
	    if (c == EOF || !csdigit(c)) {
	      input_stack::push_back(sign);
	      input_stack::push_back(echar);
	      return NUMBER;
	    }
	    context_buffer += char(echar);
	    context_buffer += char(sign);
	  }
	  else {
	    if (c == EOF || !csdigit(c)) {
	      input_stack::push_back(echar);
	      return NUMBER;
	    }
	    context_buffer += char(echar);
	  }
	  input_stack::get_char();
	  context_buffer += char(c);
	  n = c - '0';
	  for (;;) {
	    c = input_stack::peek_char();
	    if (c == EOF || !csdigit(c))
	      break;
	    input_stack::get_char();
	    context_buffer += char(c);
	    n = n*10 + (c - '0');
	  }
	  if (sign == '-')
	    n = -n;
	  if (c == 'i' || c == 'I') {
	    context_buffer += char(c);
	    input_stack::get_char();
	  }
	  token_double *= pow(10.0, n);
	  return NUMBER;
	}
      case 'n':
	input_stack::get_char();
	c = input_stack::peek_char();
	if (c == 'd') {
	  input_stack::get_char();
	  token_int = n;
	  context_buffer += "nd";
	  return ORDINAL;
	}
	input_stack::push_back('n');
	return NUMBER;
      case 'r':
	input_stack::get_char();
	c = input_stack::peek_char();
	if (c == 'd') {
	  input_stack::get_char();
	  token_int = n;
	  context_buffer += "rd";
	  return ORDINAL;
	}
	input_stack::push_back('r');
	return NUMBER;
      case 't':
	input_stack::get_char();
	c = input_stack::peek_char();
	if (c == 'h') {
	  input_stack::get_char();
	  token_int = n;
	  context_buffer += "th";
	  return ORDINAL;
	}
	input_stack::push_back('t');
	return NUMBER;
      case 's':
	input_stack::get_char();
	c = input_stack::peek_char();
	if (c == 't') {
	  input_stack::get_char();
	  token_int = n;
	  context_buffer += "st";
	  return ORDINAL;
	}
	input_stack::push_back('s');
	return NUMBER;
      default:
	return NUMBER;
      }
      break;
    case '\'':
      {
	c = input_stack::peek_char();
	if (c == 't') {
	  input_stack::get_char();
	  c = input_stack::peek_char();
	  if (c == 'h') {
	    input_stack::get_char();
	    context_buffer = "'th";
	    return TH;
	  }
	  else
	    input_stack::push_back('t');
	}
	context_buffer = "'";
	return '\'';
      }
    case '.':
      {
	c = input_stack::peek_char();
	if (c != EOF && csdigit(c)) {
	  n = 0;
	  token_double = 0.0;
	  context_buffer = '.';
	  goto got_dot;
	}
	return get_token_after_dot(c);
      }
    case '<':
      c = input_stack::peek_char();
      if (c == '-') {
	input_stack::get_char();
	c = input_stack::peek_char();
	if (c == '>') {
	  input_stack::get_char();
	  context_buffer = "<->";
	  return DOUBLE_ARROW_HEAD;
	}
	context_buffer = "<-";
	return LEFT_ARROW_HEAD;
      }
      else if (c == '=') {
	input_stack::get_char();
	context_buffer = "<=";
	return LESSEQUAL;
      }
      context_buffer = "<";
      return '<';
    case '-':
      c = input_stack::peek_char();
      if (c == '>') {
	input_stack::get_char();
	context_buffer = "->";
	return RIGHT_ARROW_HEAD;
      }
      context_buffer = "-";
      return '-';
    case '!':
      c = input_stack::peek_char();
      if (c == '=') {
	input_stack::get_char();
	context_buffer = "!=";
	return NOTEQUAL;
      }
      context_buffer = "!";
      return '!';
    case '>':
      c = input_stack::peek_char();
      if (c == '=') {
	input_stack::get_char();
	context_buffer = ">=";
	return GREATEREQUAL;
      }
      context_buffer = ">";
      return '>';
    case '=':
      c = input_stack::peek_char();
      if (c == '=') {
	input_stack::get_char();
	context_buffer = "==";
	return EQUALEQUAL;
      }
      context_buffer = "=";
      return '=';
    case '&':
      c = input_stack::peek_char();
      if (c == '&') {
	input_stack::get_char();
	context_buffer = "&&";
	return ANDAND;
      }
      context_buffer = "&";
      return '&';
    case '|':
      c = input_stack::peek_char();
      if (c == '|') {
	input_stack::get_char();
	context_buffer = "||";
	return OROR;
      }
      context_buffer = "|";
      return '|';
    default:
      if (c != EOF && csalpha(c)) {
	token_buffer.clear();
	token_buffer = c;
	for (;;) {
	  c = input_stack::peek_char();
	  if (c == EOF || (!csalnum(c) && c != '_'))
	    break;
	  input_stack::get_char();
	  token_buffer += char(c);
	}
	int tok = lookup_keyword(token_buffer.contents(),
				 token_buffer.length());
	if (tok != 0) {
	  context_buffer = token_buffer;
	  return tok;
	}
	char *def = 0;
	if (lookup_flag) {
	  token_buffer += '\0';
	  def = macro_table.lookup(token_buffer.contents());
	  token_buffer.set_length(token_buffer.length() - 1);
	  if (def) {
	    if (c == '(') {
	      input_stack::get_char();
	      interpolate_macro_with_args(def);
	    }
	    else
	      input_stack::push(new macro_input(def));
	  }
	}
	if (!def) {
	  context_buffer = token_buffer;
	  if (csupper(token_buffer[0]))
	    return LABEL;
	  else
	    return VARIABLE;
	}
      }
      else {
	context_buffer = char(c);
	return (unsigned char)c;
      }
      break;
    }
  }
}

int get_delimited()
{
  token_buffer.clear();
  int c = input_stack::get_char();
  while (c == ' ' || c == '\t' || c == '\n')
    c = input_stack::get_char();
  if (c == EOF) {
    lex_error("missing delimiter");
    return 0;
  }
  context_buffer = char(c);
  int had_newline = 0;
  int start = c;
  int level = 0;
  enum { NORMAL, IN_STRING, IN_STRING_QUOTED, DELIM_END } state = NORMAL;
  for (;;) {
    c = input_stack::get_char();
    if (c == EOF) {
      lex_error("missing closing delimiter");
      return 0;
    }
    if (c == '\n')
      had_newline = 1;
    else if (!had_newline)
      context_buffer += char(c);
    switch (state) {
    case NORMAL:
      if (start == '{') {
	if (c == '{') {
	  level++;
	  break;
	}
	if (c == '}') {
	  if (--level < 0)
	    state = DELIM_END;
	  break;
	}
      }
      else {
	if (c == start) {
	  state = DELIM_END;
	  break;
	}
      }
      if (c == '"')
	state = IN_STRING;
      break;
    case IN_STRING_QUOTED:
      if (c == '\n')
	state = NORMAL;
      else
	state = IN_STRING;
      break;
    case IN_STRING:
      if (c == '"' || c == '\n')
	state = NORMAL;
      else if (c == '\\')
	state = IN_STRING_QUOTED;
      break;
    case DELIM_END:
      // This case it just to shut cfront 2.0 up.
    default:
      assert(0);
    }
    if (state == DELIM_END)
      break;
    token_buffer += c;
  }
  return 1;
}

void do_define()
{
  int t = get_token(0);		// do not expand what we are defining
  if (t != VARIABLE && t != LABEL) {
    lex_error("can only define variable or placename");
    return;
  }
  token_buffer += '\0';
  string nm = token_buffer;
  const char *name = nm.contents();
  if (!get_delimited())
    return;
  token_buffer += '\0';
  macro_table.define(name, strsave(token_buffer.contents()));
}

void do_undef()
{
  int t = get_token(0);		// do not expand what we are undefining
  if (t != VARIABLE && t != LABEL) {
    lex_error("can only define variable or placename");
    return;
  }
  token_buffer += '\0';
  macro_table.define(token_buffer.contents(), 0);
}


class for_input : public input {
  char *var;
  char *body;
  double from;
  double to;
  int by_is_multiplicative;
  double by;
  const char *p;
  int done_newline;
public:
  for_input(char *, double, double, int, double, char *);
  ~for_input();
  int get();
  int peek();
};

for_input::for_input(char *vr, double f, double t,
		     int bim, double b, char *bd)
: var(vr), body(bd), from(f), to(t), by_is_multiplicative(bim), by(b),
  p(body), done_newline(0)
{
}

for_input::~for_input()
{
  a_delete var;
  a_delete body;
}

int for_input::get()
{
  if (p == 0)
    return EOF;
  for (;;) {
    if (*p != '\0')
      return (unsigned char)*p++;
    if (!done_newline) {
      done_newline = 1;
      return '\n';
    }
    double val;
    if (!lookup_variable(var, &val)) {
      lex_error("body of `for' terminated enclosing block");
      return EOF;
    }
    if (by_is_multiplicative)
      val *= by;
    else
      val += by;
    define_variable(var, val);
    if ((from <= to && val > to)
	|| (from >= to && val < to)) {
      p = 0;
      return EOF;
    }
    p = body;
    done_newline = 0;
  }
}

int for_input::peek()
{
  if (p == 0)
    return EOF;
  if (*p != '\0')
    return (unsigned char)*p;
  if (!done_newline)
    return '\n';
  double val;
  if (!lookup_variable(var, &val))
    return EOF;
  if (by_is_multiplicative) {
    if (val * by > to)
      return EOF;
  }
  else {
    if ((from <= to && val + by > to)
	|| (from >= to && val + by < to))
      return EOF;
  }
  if (*body == '\0')
    return EOF;
  return (unsigned char)*body;
}

void do_for(char *var, double from, double to, int by_is_multiplicative,
	    double by, char *body)
{
  define_variable(var, from);
  if ((by_is_multiplicative && by <= 0)
      || (by > 0 && from > to)
      || (by < 0 && from < to))
    return;
  input_stack::push(new for_input(var, from, to,
				  by_is_multiplicative, by, body));
}


void do_copy(const char *filename)
{
  errno = 0;
  FILE *fp = fopen(filename, "r");
  if (fp == 0) {
    lex_error("can't open `%1': %2", filename, strerror(errno));
    return;
  }
  input_stack::push(new file_input(fp, filename));
}

class copy_thru_input : public input {
  int done;
  char *body;
  char *until;
  const char *p;
  const char *ap;
  int argv[9];
  int argc;
  string line;
  int get_line();
  virtual int inget() = 0;
public:
  copy_thru_input(const char *b, const char *u);
  ~copy_thru_input();
  int get();
  int peek();
};

class copy_file_thru_input : public copy_thru_input {
  input *in;
public:
  copy_file_thru_input(input *, const char *b, const char *u);
  ~copy_file_thru_input();
  int inget();
};

copy_file_thru_input::copy_file_thru_input(input *i, const char *b,
					   const char *u)
: copy_thru_input(b, u), in(i)
{
}

copy_file_thru_input::~copy_file_thru_input()
{
  delete in;
}

int copy_file_thru_input::inget()
{
  if (!in)
    return EOF;
  else
    return in->get();
}

class copy_rest_thru_input : public copy_thru_input {
public:
  copy_rest_thru_input(const char *, const char *u);
  int inget();
};

copy_rest_thru_input::copy_rest_thru_input(const char *b, const char *u)
: copy_thru_input(b, u)
{
}

int copy_rest_thru_input::inget()
{
  while (next != 0) {
    int c = next->get();
    if (c != EOF)
      return c;
    if (next->next == 0)
      return EOF;
    input *tem = next;
    next = next->next;
    delete tem;
  }
  return EOF;

}

copy_thru_input::copy_thru_input(const char *b, const char *u)
: done(0)
{
  ap = 0;
  body = process_body(b);
  p = 0;
  until = strsave(u);
}


copy_thru_input::~copy_thru_input()
{
  a_delete body;
  a_delete until;
}

int copy_thru_input::get()
{
  if (ap) {
    if (*ap != '\0')
      return (unsigned char)*ap++;
    ap = 0;
  }
  for (;;) {
    if (p == 0) {
      if (!get_line())
	break;
      p = body;
    }
    if (*p == '\0') {
      p = 0;
      return '\n';
    }
    while (*p >= ARG1 && *p <= ARG1 + 8) {
      int i = *p++ - ARG1;
      if (i < argc && line[argv[i]] != '\0') {
	ap = line.contents() + argv[i];
	return (unsigned char)*ap++;
      }
    }
    if (*p != '\0')
      return (unsigned char)*p++;
  }
  return EOF;
}

int copy_thru_input::peek()
{
  if (ap) {
    if (*ap != '\0')
      return (unsigned char)*ap;
    ap = 0;
  }
  for (;;) {
    if (p == 0) {
      if (!get_line())
	break;
      p = body;
    }
    if (*p == '\0')
      return '\n';
    while (*p >= ARG1 && *p <= ARG1 + 8) {
      int i = *p++ - ARG1;
      if (i < argc && line[argv[i]] != '\0') {
	ap = line.contents() + argv[i];
	return (unsigned char)*ap;
      }
    }
    if (*p != '\0')
      return (unsigned char)*p;
  }
  return EOF;
}

int copy_thru_input::get_line()
{
  if (done)
    return 0;
  line.clear();
  argc = 0;
  int c = inget();
  for (;;) {
    while (c == ' ')
      c = inget();
    if (c == EOF || c == '\n')
      break;
    if (argc == 9) {
      do {
	c = inget();
      } while (c != '\n' && c != EOF);
      break;
    }
    argv[argc++] = line.length();
    do {
      line += char(c);
      c = inget();
    } while (c != ' ' && c != '\n');
    line += '\0';
  }
  if (until != 0 && argc > 0 && strcmp(&line[argv[0]], until) == 0) {
    done = 1;
    return 0;
  }
  return argc > 0 || c == '\n';
}

class simple_file_input : public input {
  const char *filename;
  int lineno;
  FILE *fp;
public:
  simple_file_input(FILE *, const char *);
  ~simple_file_input();
  int get();
  int peek();
  int get_location(const char **, int *);
};

simple_file_input::simple_file_input(FILE *p, const char *s)
: filename(s), lineno(1), fp(p)
{
}

simple_file_input::~simple_file_input()
{
  // don't delete the filename
  fclose(fp);
}

int simple_file_input::get()
{
  int c = getc(fp);
  while (invalid_input_char(c)) {
    error("invalid input character code %1", c);
    c = getc(fp);
  }
  if (c == '\n')
    lineno++;
  return c;
}

int simple_file_input::peek()
{
  int c = getc(fp);
  while (invalid_input_char(c)) {
    error("invalid input character code %1", c);
    c = getc(fp);
  }
  if (c != EOF)
    ungetc(c, fp);
  return c;
}

int simple_file_input::get_location(const char **fnp, int *lnp)
{
  *fnp = filename;
  *lnp = lineno;
  return 1;
}


void copy_file_thru(const char *filename, const char *body, const char *until)
{
  errno = 0;
  FILE *fp = fopen(filename, "r");
  if (fp == 0) {
    lex_error("can't open `%1': %2", filename, strerror(errno));
    return;
  }
  input *in = new copy_file_thru_input(new simple_file_input(fp, filename),
				       body, until);
  input_stack::push(in);
}

void copy_rest_thru(const char *body, const char *until)
{
  input_stack::push(new copy_rest_thru_input(body, until));
}

void push_body(const char *s)
{
  input_stack::push(new char_input('\n'));
  input_stack::push(new macro_input(s));
}

int delim_flag = 0;

char *get_thru_arg()
{
  int c = input_stack::peek_char();
  while (c == ' ') {
    input_stack::get_char();
    c = input_stack::peek_char();
  }
  if (c != EOF && csalpha(c)) {
    // looks like a macro
    input_stack::get_char();
    token_buffer = c;
    for (;;) {
      c = input_stack::peek_char();
      if (c == EOF || (!csalnum(c) && c != '_'))
	break;
      input_stack::get_char();
      token_buffer += char(c);
    }
    context_buffer = token_buffer;
    token_buffer += '\0';
    char *def = macro_table.lookup(token_buffer.contents());
    if (def)
      return strsave(def);
    // I guess it wasn't a macro after all; so push the macro name back.
    // -2 because we added a '\0'
    for (int i = token_buffer.length() - 2; i >= 0; i--)
      input_stack::push_back(token_buffer[i]);
  }
  if (get_delimited()) {
    token_buffer += '\0';
    return strsave(token_buffer.contents());
  }
  else
    return 0;
}

int lookahead_token = -1;
string old_context_buffer;

void do_lookahead()
{
  if (lookahead_token == -1) {
    old_context_buffer = context_buffer;
    lookahead_token = get_token(1);
  }
}

int yylex()
{
  if (delim_flag) {
    assert(lookahead_token == -1);
    if (delim_flag == 2) {
      if ((yylval.str = get_thru_arg()) != 0)
	return DELIMITED;
      else
	return 0;
    }
    else {
      if (get_delimited()) {
	token_buffer += '\0';
	yylval.str = strsave(token_buffer.contents());
	return DELIMITED;
      }
      else
	return 0;
    }
  }
  for (;;) {
    int t;
    if (lookahead_token >= 0) {
      t = lookahead_token;
      lookahead_token = -1;
    }
    else
      t = get_token(1);
    switch (t) {
    case '\n':
      return ';';
    case EOF:
      return 0;
    case DEFINE:
      do_define();
      break;
    case UNDEF:
      do_undef();
      break;
    case ORDINAL:
      yylval.n = token_int;
      return t;
    case NUMBER:
      yylval.x = token_double;
      return t;
    case COMMAND_LINE:
    case TEXT:
      token_buffer += '\0';
      if (!input_stack::get_location(&yylval.lstr.filename,
				     &yylval.lstr.lineno)) {
	yylval.lstr.filename = 0;
	yylval.lstr.lineno = -1;
      }
      yylval.lstr.str = strsave(token_buffer.contents());
      return t;
    case LABEL:
    case VARIABLE:
      token_buffer += '\0';
      yylval.str = strsave(token_buffer.contents());
      return t;
    case LEFT:
      // change LEFT to LEFT_CORNER when followed by OF
      old_context_buffer = context_buffer;
      lookahead_token = get_token(1);
      if (lookahead_token == OF)
	return LEFT_CORNER;
      else
	return t;
    case RIGHT:
      // change RIGHT to RIGHT_CORNER when followed by OF
      old_context_buffer = context_buffer;
      lookahead_token = get_token(1);
      if (lookahead_token == OF)
	return RIGHT_CORNER;
      else
	return t;
    case UPPER:
      // recognise UPPER only before LEFT or RIGHT
      old_context_buffer = context_buffer;
      lookahead_token = get_token(1);
      if (lookahead_token != LEFT && lookahead_token != RIGHT) {
	yylval.str = strsave("upper");
	return VARIABLE;
      }
      else
	return t;
    case LOWER:
      // recognise LOWER only before LEFT or RIGHT
      old_context_buffer = context_buffer;
      lookahead_token = get_token(1);
      if (lookahead_token != LEFT && lookahead_token != RIGHT) {
	yylval.str = strsave("lower");
	return VARIABLE;
      }
      else
	return t;
    case NORTH:
      // recognise NORTH only before OF
      old_context_buffer = context_buffer;
      lookahead_token = get_token(1);
      if (lookahead_token != OF) {
	yylval.str = strsave("north");
	return VARIABLE;
      }
      else
	return t;
    case SOUTH:
      // recognise SOUTH only before OF
      old_context_buffer = context_buffer;
      lookahead_token = get_token(1);
      if (lookahead_token != OF) {
	yylval.str = strsave("south");
	return VARIABLE;
      }
      else
	return t;
    case EAST:
      // recognise EAST only before OF
      old_context_buffer = context_buffer;
      lookahead_token = get_token(1);
      if (lookahead_token != OF) {
	yylval.str = strsave("east");
	return VARIABLE;
      }
      else
	return t;
    case WEST:
      // recognise WEST only before OF
      old_context_buffer = context_buffer;
      lookahead_token = get_token(1);
      if (lookahead_token != OF) {
	yylval.str = strsave("west");
	return VARIABLE;
      }
      else
	return t;
    case TOP:
      // recognise TOP only before OF
      old_context_buffer = context_buffer;
      lookahead_token = get_token(1);
      if (lookahead_token != OF) {
	yylval.str = strsave("top");
	return VARIABLE;
      }
      else
	return t;
    case BOTTOM:
      // recognise BOTTOM only before OF
      old_context_buffer = context_buffer;
      lookahead_token = get_token(1);
      if (lookahead_token != OF) {
	yylval.str = strsave("bottom");
	return VARIABLE;
      }
      else
	return t;
    case CENTER:
      // recognise CENTER only before OF
      old_context_buffer = context_buffer;
      lookahead_token = get_token(1);
      if (lookahead_token != OF) {
	yylval.str = strsave("center");
	return VARIABLE;
      }
      else
	return t;
    case START:
      // recognise START only before OF
      old_context_buffer = context_buffer;
      lookahead_token = get_token(1);
      if (lookahead_token != OF) {
	yylval.str = strsave("start");
	return VARIABLE;
      }
      else
	return t;
    case END:
      // recognise END only before OF
      old_context_buffer = context_buffer;
      lookahead_token = get_token(1);
      if (lookahead_token != OF) {
	yylval.str = strsave("end");
	return VARIABLE;
      }
      else
	return t;
    default:
      return t;
    }
  }
}

void lex_error(const char *message,
	       const errarg &arg1,
	       const errarg &arg2,
	       const errarg &arg3)
{
  const char *filename;
  int lineno;
  if (!input_stack::get_location(&filename, &lineno))
    error(message, arg1, arg2, arg3);
  else
    error_with_file_and_line(filename, lineno, message, arg1, arg2, arg3);
}

void lex_warning(const char *message,
		 const errarg &arg1,
		 const errarg &arg2,
		 const errarg &arg3)
{
  const char *filename;
  int lineno;
  if (!input_stack::get_location(&filename, &lineno))
    warning(message, arg1, arg2, arg3);
  else
    warning_with_file_and_line(filename, lineno, message, arg1, arg2, arg3);
}

void yyerror(const char *s)
{
  const char *filename;
  int lineno;
  const char *context = 0;
  if (lookahead_token == -1) {
    if (context_buffer.length() > 0) {
      context_buffer += '\0';
      context = context_buffer.contents();
    }
  }
  else {
    if (old_context_buffer.length() > 0) {
      old_context_buffer += '\0';
      context = old_context_buffer.contents();
    }
  }
  if (!input_stack::get_location(&filename, &lineno)) {
    if (context) {
      if (context[0] == '\n' && context[1] == '\0')
	error("%1 before newline", s);
      else
	error("%1 before `%2'", s, context);
    }
    else
      error("%1 at end of picture", s);
  }
  else {
    if (context) {
      if (context[0] == '\n' && context[1] == '\0')
	error_with_file_and_line(filename, lineno, "%1 before newline", s);
      else
	error_with_file_and_line(filename, lineno, "%1 before `%2'",
				 s, context);
    }
    else
      error_with_file_and_line(filename, lineno, "%1 at end of picture", s);
  }
}

