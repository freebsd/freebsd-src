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


struct charinfo;
struct node;
struct vunits;

class token {
  symbol nm;
  node *nd;
  unsigned char c;
  int val;
  units dim;
  enum token_type {
    TOKEN_BACKSPACE,
    TOKEN_BEGIN_TRAP,
    TOKEN_CHAR,			// a normal printing character
    TOKEN_DUMMY,
    TOKEN_EMPTY,		// this is the initial value
    TOKEN_END_TRAP,
    TOKEN_ESCAPE,		// \e
    TOKEN_HYPHEN_INDICATOR,
    TOKEN_INTERRUPT,		// \c
    TOKEN_ITALIC_CORRECTION,	// \/
    TOKEN_LEADER,		// ^A
    TOKEN_LEFT_BRACE,
    TOKEN_MARK_INPUT,		// \k -- `nm' is the name of the register
    TOKEN_NEWLINE,		// newline
    TOKEN_NODE,
    TOKEN_NUMBERED_CHAR,
    TOKEN_PAGE_EJECTOR,
    TOKEN_REQUEST,
    TOKEN_RIGHT_BRACE,
    TOKEN_SPACE,		// ` ' -- ordinary space
    TOKEN_SPECIAL,		// a special character -- \' \` \- \(xx
    TOKEN_SPREAD,		// \p -- break and spread output line 
    TOKEN_TAB,			// tab
    TOKEN_TRANSPARENT,		// \!
    TOKEN_EOF			// end of file
    } type;
public:
  token();
  ~token();
  token(const token &);
  void operator=(const token &);
  void next();
  void process();
  void skip();
  int eof();
  int nspaces();		// 1 if space, 2 if double space, 0 otherwise
  int space();			// is it a space or double space?
  int white_space();		// is the current token space or tab?
  int newline();		// is the current token a newline?
  int tab();			// is the current token a tab?
  int leader();
  int backspace();
  int delimiter(int warn = 0);	// is it suitable for use as a delimiter?
  int dummy();
  int transparent();
  int left_brace();
  int right_brace();
  int page_ejector();
  int hyphen_indicator();
  int operator==(const token &); // need this for delimiters, and for conditions
  int operator!=(const token &); // ditto
  unsigned char ch();
  charinfo *get_char(int required = 0);
  int add_to_node_list(node **);
  int title();
  void make_space();
  void make_newline();
  const char *description();

  friend void process_input_stack();
};

extern token tok;		// the current token

extern symbol get_name(int required = 0);
extern symbol get_long_name(int required = 0);
extern charinfo *get_optional_char();
extern void check_missing_character();
extern void skip_line();
extern void handle_initial_title();

struct hunits;
extern void read_title_parts(node **part, hunits *part_width);

extern int get_number(units *result, unsigned char si);
extern int get_integer(int *result);

extern int get_number(units *result, unsigned char si, units prev_value);
extern int get_integer(int *result, int prev_value);

void interpolate_number_reg(symbol, int);

const char *asciify(int c);

inline int token::newline()
{ 
  return type == TOKEN_NEWLINE; 
}

inline int token::space()
{ 
  return type == TOKEN_SPACE;
}

inline int token::nspaces()
{
  if (type == TOKEN_SPACE)
    return 1;
  else
    return 0;
}

inline int token::white_space()
{
  return type == TOKEN_SPACE || type == TOKEN_TAB;
}

inline int token::transparent()
{
  return type == TOKEN_TRANSPARENT;
}

inline int token::page_ejector()
{
  return type == TOKEN_PAGE_EJECTOR;
}

inline unsigned char token::ch()
{
  return type == TOKEN_CHAR ? c : 0;
} 

inline int token::eof()
{
  return type == TOKEN_EOF;
}

inline int token::dummy()
{
  return type == TOKEN_DUMMY;
}

inline int token::left_brace()
{
  return type == TOKEN_LEFT_BRACE;
}

inline int token::right_brace()
{
  return type == TOKEN_RIGHT_BRACE;
}

inline int token::tab()
{
  return type == TOKEN_TAB;
}

inline int token::leader()
{
  return type == TOKEN_LEADER;
}

inline int token::backspace()
{
  return type == TOKEN_BACKSPACE;
}

inline int token::hyphen_indicator()
{
  return type == TOKEN_HYPHEN_INDICATOR;
}

int has_arg();
