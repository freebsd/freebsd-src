/* -*- C++ -*-
   Copyright (C) 1989, 1990, 1991, 1992 Free Software Foundation, Inc.
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

%{

#include "refer.h"
#include "refid.h"
#include "ref.h"
#include "token.h"

int yylex();
void yyerror(const char *);
int yyparse();

static const char *format_serial(char c, int n);

struct label_info {
  int start;
  int length;
  int count;
  int total;
  label_info(const string &);
};

label_info *lookup_label(const string &label);

struct expression {
  enum {
    // Does the tentative label depend on the reference?
    CONTAINS_VARIABLE = 01, 
    CONTAINS_STAR = 02,
    CONTAINS_FORMAT = 04,
    CONTAINS_AT = 010
  };
  virtual ~expression() { }
  virtual void evaluate(int, const reference &, string &,
			substring_position &) = 0;
  virtual unsigned analyze() { return 0; }
};

class at_expr : public expression {
public:
  at_expr() { }
  void evaluate(int, const reference &, string &, substring_position &);
  unsigned analyze() { return CONTAINS_VARIABLE|CONTAINS_AT; }
};

class format_expr : public expression {
  char type;
  int width;
  int first_number;
public:
  format_expr(char c, int w = 0, int f = 1)
    : type(c), width(w), first_number(f) { }
  void evaluate(int, const reference &, string &, substring_position &);
  unsigned analyze() { return CONTAINS_FORMAT; }
};

class field_expr : public expression {
  int number;
  char name;
public:
  field_expr(char nm, int num) : name(nm), number(num) { }
  void evaluate(int, const reference &, string &, substring_position &);
  unsigned analyze() { return CONTAINS_VARIABLE; }
};

class literal_expr : public expression {
  string s;
public:
  literal_expr(const char *ptr, int len) : s(ptr, len) { }
  void evaluate(int, const reference &, string &, substring_position &);
};

class unary_expr : public expression {
protected:
  expression *expr;
public:
  unary_expr(expression *e) : expr(e) { }
  ~unary_expr() { delete expr; }
  void evaluate(int, const reference &, string &, substring_position &) = 0;
  unsigned analyze() { return expr ? expr->analyze() : 0; }
};

// This caches the analysis of an expression.

class analyzed_expr : public unary_expr {
  unsigned flags;
public:
  analyzed_expr(expression *);
  void evaluate(int, const reference &, string &, substring_position &);
  unsigned analyze() { return flags; }
};

class star_expr : public unary_expr {
public:
  star_expr(expression *e) : unary_expr(e) { }
  void evaluate(int, const reference &, string &, substring_position &);
  unsigned analyze() {
    return ((expr ? (expr->analyze() & ~CONTAINS_VARIABLE) : 0)
	    | CONTAINS_STAR);
  }
};

typedef void map_func(const char *, const char *, string &);

class map_expr : public unary_expr {
  map_func *func;
public:
  map_expr(expression *e, map_func *f) : unary_expr(e), func(f) { }
  void evaluate(int, const reference &, string &, substring_position &);
};
  
typedef const char *extractor_func(const char *, const char *, const char **);

class extractor_expr : public unary_expr {
  int part;
  extractor_func *func;
public:
  enum { BEFORE = +1, MATCH = 0, AFTER = -1 };
  extractor_expr(expression *e, extractor_func *f, int pt)
    : unary_expr(e), func(f), part(pt) { }
  void evaluate(int, const reference &, string &, substring_position &);
};

class truncate_expr : public unary_expr {
  int n;
public:
  truncate_expr(expression *e, int i) : n(i), unary_expr(e) { } 
  void evaluate(int, const reference &, string &, substring_position &);
};

class separator_expr : public unary_expr {
public:
  separator_expr(expression *e) : unary_expr(e) { }
  void evaluate(int, const reference &, string &, substring_position &);
};

class binary_expr : public expression {
protected:
  expression *expr1;
  expression *expr2;
public:
  binary_expr(expression *e1, expression *e2) : expr1(e1), expr2(e2) { }
  ~binary_expr() { delete expr1; delete expr2; }
  void evaluate(int, const reference &, string &, substring_position &) = 0;
  unsigned analyze() {
    return (expr1 ? expr1->analyze() : 0) | (expr2 ? expr2->analyze() : 0);
  }
};

class alternative_expr : public binary_expr {
public:
  alternative_expr(expression *e1, expression *e2) : binary_expr(e1, e2) { }
  void evaluate(int, const reference &, string &, substring_position &);
};

class list_expr : public binary_expr {
public:
  list_expr(expression *e1, expression *e2) : binary_expr(e1, e2) { }
  void evaluate(int, const reference &, string &, substring_position &);
};

class substitute_expr : public binary_expr {
public:
  substitute_expr(expression *e1, expression *e2) : binary_expr(e1, e2) { }
  void evaluate(int, const reference &, string &, substring_position &);
};

class ternary_expr : public expression {
protected:
  expression *expr1;
  expression *expr2;
  expression *expr3;
public:
  ternary_expr(expression *e1, expression *e2, expression *e3)
    : expr1(e1), expr2(e2), expr3(e3) { }
  ~ternary_expr() { delete expr1; delete expr2; delete expr3; }
  void evaluate(int, const reference &, string &, substring_position &) = 0;
  unsigned analyze() {
    return ((expr1 ? expr1->analyze() : 0)
	    | (expr2 ? expr2->analyze() : 0)
	    | (expr3 ? expr3->analyze() : 0));
  }
};

class conditional_expr : public ternary_expr {
public:
  conditional_expr(expression *e1, expression *e2, expression *e3)
    : ternary_expr(e1, e2, e3) { }
  void evaluate(int, const reference &, string &, substring_position &);
};

static expression *parsed_label = 0;
static expression *parsed_date_label = 0;
static expression *parsed_short_label = 0;

static expression *parse_result;

string literals;

%}

%union {
  int num;
  expression *expr;
  struct { int ndigits; int val; } dig;
  struct { int start; int len; } str;
}

/* uppercase or lowercase letter */
%token <num> TOKEN_LETTER
/* literal characters */
%token <str> TOKEN_LITERAL
/* digit */
%token <num> TOKEN_DIGIT

%type <expr> conditional
%type <expr> alternative
%type <expr> list
%type <expr> string
%type <expr> substitute
%type <expr> optional_conditional
%type <num> number
%type <dig> digits
%type <num> optional_number
%type <num> flag

%%

expr:
	optional_conditional
		{ parse_result = ($1 ? new analyzed_expr($1) : 0); }
	;

conditional:
	alternative
		{ $$ = $1; }
	| alternative '?' optional_conditional ':' conditional
		{ $$ = new conditional_expr($1, $3, $5); }
	;

optional_conditional:
	/* empty */
		{ $$ = 0; }
	| conditional
		{ $$ = $1; }
	;

alternative:
	list
		{ $$ = $1; }
	| alternative '|' list
		{ $$ = new alternative_expr($1, $3); }
	| alternative '&' list
		{ $$ = new conditional_expr($1, $3, 0); }
	;	

list:
	substitute
		{ $$ = $1; }
	| list substitute
		{ $$ = new list_expr($1, $2); }
	;

substitute:
	string
		{ $$ = $1; }
	| substitute '~' string
		{ $$ = new substitute_expr($1, $3); }
	;

string:
	'@'
		{ $$ = new at_expr; }
	| TOKEN_LITERAL
		{
		  $$ = new literal_expr(literals.contents() + $1.start,
					$1.len);
		}
	| TOKEN_LETTER
		{ $$ = new field_expr($1, 0); }
	| TOKEN_LETTER number
		{ $$ = new field_expr($1, $2 - 1); }
	| '%' TOKEN_LETTER
		{
		  switch ($2) {
		  case 'I':
		  case 'i':
		  case 'A':
		  case 'a':
		    $$ = new format_expr($2);
		    break;
		  default:
		    command_error("unrecognized format `%1'", char($2));
		    $$ = new format_expr('a');
		    break;
		  }
		}
	
	| '%' digits
		{
		  $$ = new format_expr('0', $2.ndigits, $2.val);
		}
	| string '.' flag TOKEN_LETTER optional_number
		{
		  switch ($4) {
		  case 'l':
		    $$ = new map_expr($1, lowercase);
		    break;
		  case 'u':
		    $$ = new map_expr($1, uppercase);
		    break;
		  case 'c':
		    $$ = new map_expr($1, capitalize);
		    break;
		  case 'r':
		    $$ = new map_expr($1, reverse_name);
		    break;
		  case 'a':
		    $$ = new map_expr($1, abbreviate_name);
		    break;
		  case 'y':
		    $$ = new extractor_expr($1, find_year, $3);
		    break;
		  case 'n':
		    $$ = new extractor_expr($1, find_last_name, $3);
		    break;
		  default:
		    $$ = $1;
		    command_error("unknown function `%1'", char($4));
		    break;
		  }
		}

	| string '+' number
		{ $$ = new truncate_expr($1, $3); }
	| string '-' number
		{ $$ = new truncate_expr($1, -$3); }
	| string '*'
		{ $$ = new star_expr($1); }
	| '(' optional_conditional ')'
		{ $$ = $2; }
	| '<' optional_conditional '>'
		{ $$ = new separator_expr($2); }
	;

optional_number:
	/* empty */
		{ $$ = -1; }
	| number
		{ $$ = $1; }
	;

number:
	TOKEN_DIGIT
		{ $$ = $1; }
	| number TOKEN_DIGIT
		{ $$ = $1*10 + $2; }
	;

digits:
	TOKEN_DIGIT
		{ $$.ndigits = 1; $$.val = $1; }
	| digits TOKEN_DIGIT
		{ $$.ndigits = $1.ndigits + 1; $$.val = $1.val*10 + $2; }
	;
	
      
flag:
	/* empty */
		{ $$ = 0; }
	| '+'
		{ $$ = 1; }
	| '-'
		{ $$ = -1; }
	;

%%

/* bison defines const to be empty unless __STDC__ is defined, which it
isn't under cfront */

#ifdef const
#undef const
#endif

const char *spec_ptr;
const char *spec_end;
const char *spec_cur;

int yylex()
{
  while (spec_ptr < spec_end && csspace(*spec_ptr))
    spec_ptr++;
  spec_cur = spec_ptr;
  if (spec_ptr >= spec_end)
    return 0;
  unsigned char c = *spec_ptr++;
  if (csalpha(c)) {
    yylval.num = c;
    return TOKEN_LETTER;
  }
  if (csdigit(c)) {
    yylval.num = c - '0';
    return TOKEN_DIGIT;
  }
  if (c == '\'') {
    yylval.str.start = literals.length();
    for (; spec_ptr < spec_end; spec_ptr++) {
      if (*spec_ptr == '\'') {
	if (++spec_ptr < spec_end && *spec_ptr == '\'')
	  literals += '\'';
	else {
	  yylval.str.len = literals.length() - yylval.str.start;
	  return TOKEN_LITERAL;
	}
      }
      else
	literals += *spec_ptr;
    }
    yylval.str.len = literals.length() - yylval.str.start;
    return TOKEN_LITERAL;
  }
  return c;
}

int set_label_spec(const char *label_spec)
{
  spec_cur = spec_ptr = label_spec;
  spec_end = strchr(label_spec, '\0');
  literals.clear();
  if (yyparse())
    return 0;
  delete parsed_label;
  parsed_label = parse_result;
  return 1;
}

int set_date_label_spec(const char *label_spec)
{
  spec_cur = spec_ptr = label_spec;
  spec_end = strchr(label_spec, '\0');
  literals.clear();
  if (yyparse())
    return 0;
  delete parsed_date_label;
  parsed_date_label = parse_result;
  return 1;
}

int set_short_label_spec(const char *label_spec)
{
  spec_cur = spec_ptr = label_spec;
  spec_end = strchr(label_spec, '\0');
  literals.clear();
  if (yyparse())
    return 0;
  delete parsed_short_label;
  parsed_short_label = parse_result;
  return 1;
}

void yyerror(const char *message)
{
  if (spec_cur < spec_end)
    command_error("label specification %1 before `%2'", message, spec_cur);
  else
    command_error("label specification %1 at end of string",
		  message, spec_cur);
}

void at_expr::evaluate(int tentative, const reference &ref,
		       string &result, substring_position &)
{
  if (tentative)
    ref.canonicalize_authors(result);
  else {
    const char *end, *start = ref.get_authors(&end);
    if (start)
      result.append(start, end - start);
  }
}

void format_expr::evaluate(int tentative, const reference &ref,
			   string &result, substring_position &)
{
  if (tentative)
    return;
  const label_info *lp = ref.get_label_ptr();
  int num = lp == 0 ? ref.get_number() : lp->count;
  if (type != '0')
    result += format_serial(type, num + 1);
  else {
    const char *ptr = itoa(num + first_number);
    int pad = width - strlen(ptr);
    while (--pad >= 0)
      result += '0';
    result += ptr;
  }
}

static const char *format_serial(char c, int n)
{
  assert(n > 0);
  static char buf[128]; // more than enough.
  switch (c) {
  case 'i':
  case 'I':
    {
      char *p = buf;
      // troff uses z and w to represent 10000 and 5000 in Roman
      // numerals; I can find no historical basis for this usage
      const char *s = c == 'i' ? "zwmdclxvi" : "ZWMDCLXVI";
      if (n >= 40000)
	return itoa(n);
      while (n >= 10000) {
	*p++ = s[0];
	n -= 10000;
      }
      for (int i = 1000; i > 0; i /= 10, s += 2) {
	int m = n/i;
	n -= m*i;
	switch (m) {
	case 3:
	  *p++ = s[2];
	  /* falls through */
	case 2:
	  *p++ = s[2];
	  /* falls through */
	case 1:
	  *p++ = s[2];
	  break;
	case 4:
	  *p++ = s[2];
	  *p++ = s[1];
	  break;
	case 8:
	  *p++ = s[1];
	  *p++ = s[2];
	  *p++ = s[2];
	  *p++ = s[2];
	  break;
	case 7:
	  *p++ = s[1];
	  *p++ = s[2];
	  *p++ = s[2];
	  break;
	case 6:
	  *p++ = s[1];
	  *p++ = s[2];
	  break;
	case 5:
	  *p++ = s[1];
	  break;
	case 9:
	  *p++ = s[2];
	  *p++ = s[0];
	}
      }
      *p = 0;
      break;
    }
  case 'a':
  case 'A':
    {
      char *p = buf;
      // this is derived from troff/reg.c
      while (n > 0) {
	int d = n % 26;
	if (d == 0)
	  d = 26;
	n -= d;
	n /= 26;
	*p++ = c + d - 1;	// ASCII dependent
      }
      *p-- = 0;
      // Reverse it.
      char *q = buf;
      while (q < p) {
	char temp = *q;
	*q = *p;
	*p = temp;
	--p;
	++q;
      }
      break;
    }
  default:
    assert(0);
  }
  return buf;
}

void field_expr::evaluate(int, const reference &ref,
			  string &result, substring_position &)
{
  const char *end;
  const char *start = ref.get_field(name, &end);
  if (start) {
    start = nth_field(number, start, &end);
    if (start)
      result.append(start, end - start);
  }
}

void literal_expr::evaluate(int, const reference &,
			    string &result, substring_position &)
{
  result += s;
}

analyzed_expr::analyzed_expr(expression *e)
: unary_expr(e), flags(e ? e->analyze() : 0)
{
}

void analyzed_expr::evaluate(int tentative, const reference &ref,
			     string &result, substring_position &pos)
{
  if (expr)
    expr->evaluate(tentative, ref, result, pos);
}

void star_expr::evaluate(int tentative, const reference &ref,
			 string &result, substring_position &pos)
{
  const label_info *lp = ref.get_label_ptr();
  if (!tentative
      && (lp == 0 || lp->total > 1)
      && expr)
    expr->evaluate(tentative, ref, result, pos);
}

void separator_expr::evaluate(int tentative, const reference &ref,
			      string &result, substring_position &pos)
{
  int start_length = result.length();
  int is_first = pos.start < 0;
  if (expr)
    expr->evaluate(tentative, ref, result, pos);
  if (is_first) {
    pos.start = start_length;
    pos.length = result.length() - start_length;
  }
}

void map_expr::evaluate(int tentative, const reference &ref,
			string &result, substring_position &)
{
  if (expr) {
    string temp;
    substring_position temp_pos;
    expr->evaluate(tentative, ref, temp, temp_pos);
    (*func)(temp.contents(), temp.contents() + temp.length(), result);
  }
}

void extractor_expr::evaluate(int tentative, const reference &ref,
			      string &result, substring_position &)
{
  if (expr) {
    string temp;
    substring_position temp_pos;
    expr->evaluate(tentative, ref, temp, temp_pos);
    const char *end, *start = (*func)(temp.contents(),
				      temp.contents() + temp.length(),
				      &end);
    switch (part) {
    case BEFORE:
      if (start)
	result.append(temp.contents(), start - temp.contents());
      else
	result += temp;
      break;
    case MATCH:
      if (start)
	result.append(start, end - start);
      break;
    case AFTER:
      if (start)
	result.append(end, temp.contents() + temp.length() - end);
      break;
    default:
      assert(0);
    }
  }
}

static void first_part(int len, const char *ptr, const char *end,
			  string &result)
{
  for (;;) {
    const char *token_start = ptr;
    if (!get_token(&ptr, end))
      break;
    const token_info *ti = lookup_token(token_start, ptr);
    int counts = ti->sortify_non_empty(token_start, ptr);
    if (counts && --len < 0)
      break;
    if (counts || ti->is_accent())
      result.append(token_start, ptr - token_start);
  }
}

static void last_part(int len, const char *ptr, const char *end,
		      string &result)
{
  const char *start = ptr;
  int count = 0;
  for (;;) {
    const char *token_start = ptr;
    if (!get_token(&ptr, end))
      break;
    const token_info *ti = lookup_token(token_start, ptr);
    if (ti->sortify_non_empty(token_start, ptr))
      count++;
  }
  ptr = start;
  int skip = count - len;
  if (skip > 0) {
    for (;;) {
      const char *token_start = ptr;
      if (!get_token(&ptr, end))
	assert(0);
      const token_info *ti = lookup_token(token_start, ptr);
      if (ti->sortify_non_empty(token_start, ptr) && --skip < 0) {
	ptr = token_start;
	break;
      }
    }
  }
  first_part(len, ptr, end, result);
}

void truncate_expr::evaluate(int tentative, const reference &ref,
			     string &result, substring_position &)
{
  if (expr) {
    string temp;
    substring_position temp_pos;
    expr->evaluate(tentative, ref, temp, temp_pos);
    const char *start = temp.contents();
    const char *end = start + temp.length();
    if (n > 0)
      first_part(n, start, end, result);
    else if (n < 0)
      last_part(-n, start, end, result);
  }
}

void alternative_expr::evaluate(int tentative, const reference &ref,
				string &result, substring_position &pos)
{
  int start_length = result.length();
  if (expr1)
    expr1->evaluate(tentative, ref, result, pos);
  if (result.length() == start_length && expr2)
    expr2->evaluate(tentative, ref, result, pos);
}

void list_expr::evaluate(int tentative, const reference &ref,
			 string &result, substring_position &pos)
{
  if (expr1)
    expr1->evaluate(tentative, ref, result, pos);
  if (expr2)
    expr2->evaluate(tentative, ref, result, pos);
}

void substitute_expr::evaluate(int tentative, const reference &ref,
			       string &result, substring_position &pos)
{
  int start_length = result.length();
  if (expr1)
    expr1->evaluate(tentative, ref, result, pos);
  if (result.length() > start_length && result[result.length() - 1] == '-') {
    // ought to see if pos covers the -
    result.set_length(result.length() - 1);
    if (expr2)
      expr2->evaluate(tentative, ref, result, pos);
  }
}

void conditional_expr::evaluate(int tentative, const reference &ref,
				string &result, substring_position &pos)
{
  string temp;
  substring_position temp_pos;
  if (expr1)
    expr1->evaluate(tentative, ref, temp, temp_pos);
  if (temp.length() > 0) {
    if (expr2)
      expr2->evaluate(tentative, ref, result, pos);
  }
  else {
    if (expr3)
      expr3->evaluate(tentative, ref, result, pos);
  }
}

void reference::pre_compute_label()
{
  if (parsed_label != 0
      && (parsed_label->analyze() & expression::CONTAINS_VARIABLE)) {
    label.clear();
    substring_position temp_pos;
    parsed_label->evaluate(1, *this, label, temp_pos);
    label_ptr = lookup_label(label);
  }
}

void reference::compute_label()
{
  label.clear();
  if (parsed_label)
    parsed_label->evaluate(0, *this, label, separator_pos);
  if (short_label_flag && parsed_short_label)
    parsed_short_label->evaluate(0, *this, short_label, short_separator_pos);
  if (date_as_label) {
    string new_date;
    if (parsed_date_label) {
      substring_position temp_pos;
      parsed_date_label->evaluate(0, *this, new_date, temp_pos);
    }
    set_date(new_date);
  }
  if (label_ptr)
    label_ptr->count += 1;
}

void reference::immediate_compute_label()
{
  if (label_ptr)
    label_ptr->total = 2;	// force use of disambiguator
  compute_label();
}

int reference::merge_labels(reference **v, int n, label_type type,
			    string &result)
{
  if (abbreviate_label_ranges)
    return merge_labels_by_number(v, n, type, result);
  else
    return merge_labels_by_parts(v, n, type, result);
}

int reference::merge_labels_by_number(reference **v, int n, label_type type,
				      string &result)
{
  if (n <= 1)
    return 0;
  int num = get_number();
  // Only merge three or more labels.
  if (v[0]->get_number() != num + 1
      || v[1]->get_number() != num + 2)
    return 0;
  int i;
  for (i = 2; i < n; i++)
    if (v[i]->get_number() != num + i + 1)
      break;
  result = get_label(type);
  result += label_range_indicator;
  result += v[i - 1]->get_label(type);
  return i;
}

const substring_position &reference::get_separator_pos(label_type type) const
{
  if (type == SHORT_LABEL && short_label_flag)
    return short_separator_pos;
  else
    return separator_pos;
}

const string &reference::get_label(label_type type) const
{
  if (type == SHORT_LABEL && short_label_flag)
    return short_label; 
  else
    return label;
}

int reference::merge_labels_by_parts(reference **v, int n, label_type type,
				     string &result)
{
  if (n <= 0)
    return 0;
  const string &lb = get_label(type);
  const substring_position &sp = get_separator_pos(type);
  if (sp.start < 0
      || sp.start != v[0]->get_separator_pos(type).start 
      || memcmp(lb.contents(), v[0]->get_label(type).contents(),
		sp.start) != 0)
    return 0;
  result = lb;
  int i = 0;
  do {
    result += separate_label_second_parts;
    const substring_position &s = v[i]->get_separator_pos(type);
    int sep_end_pos = s.start + s.length;
    result.append(v[i]->get_label(type).contents() + sep_end_pos,
		  v[i]->get_label(type).length() - sep_end_pos);
  } while (++i < n
	   && sp.start == v[i]->get_separator_pos(type).start
	   && memcmp(lb.contents(), v[i]->get_label(type).contents(),
		     sp.start) == 0);
  return i;
}

string label_pool;

label_info::label_info(const string &s)
: count(0), total(1), length(s.length()), start(label_pool.length())
{
  label_pool += s;
}

static label_info **label_table = 0;
static int label_table_size = 0;
static int label_table_used = 0;

label_info *lookup_label(const string &label)
{
  if (label_table == 0) {
    label_table = new label_info *[17];
    label_table_size = 17;
    for (int i = 0; i < 17; i++)
      label_table[i] = 0;
  }
  unsigned h = hash_string(label.contents(), label.length()) % label_table_size;
  label_info **ptr;
  for (ptr = label_table + h;
       *ptr != 0;
       (ptr == label_table)
       ? (ptr = label_table + label_table_size - 1)
       : ptr--)
    if ((*ptr)->length == label.length()
	&& memcmp(label_pool.contents() + (*ptr)->start, label.contents(),
		  label.length()) == 0) {
      (*ptr)->total += 1;
      return *ptr;
    }
  label_info *result = *ptr = new label_info(label);
  if (++label_table_used * 2 > label_table_size) {
    // Rehash the table.
    label_info **old_table = label_table;
    int old_size = label_table_size;
    label_table_size = next_size(label_table_size);
    label_table = new label_info *[label_table_size];
    int i;
    for (i = 0; i < label_table_size; i++)
      label_table[i] = 0;
    for (i = 0; i < old_size; i++)
      if (old_table[i]) {
	unsigned h = hash_string(label_pool.contents() + old_table[i]->start,
				 old_table[i]->length);
	label_info **p;
	for (p = label_table + (h % label_table_size);
	     *p != 0;
	     (p == label_table)
	     ? (p = label_table + label_table_size - 1)
	     : --p)
	    ;
	*p = old_table[i];
	}
    a_delete old_table;
  }
  return result;
}

void clear_labels()
{
  for (int i = 0; i < label_table_size; i++) {
    delete label_table[i];
    label_table[i] = 0;
  }
  label_table_used = 0;
  label_pool.clear();
}

static void consider_authors(reference **start, reference **end, int i);

void compute_labels(reference **v, int n)
{
  if (parsed_label
      && (parsed_label->analyze() & expression::CONTAINS_AT)
      && sort_fields.length() >= 2
      && sort_fields[0] == 'A'
      && sort_fields[1] == '+')
    consider_authors(v, v + n, 0);
  for (int i = 0; i < n; i++)
    v[i]->compute_label();
}


/* A reference with a list of authors <A0,A1,...,AN> _needs_ author i
where 0 <= i <= N if there exists a reference with a list of authors
<B0,B1,...,BM> such that <A0,A1,...,AN> != <B0,B1,...,BM> and M >= i
and Aj = Bj for 0 <= j < i. In this case if we can't say ``A0,
A1,...,A(i-1) et al'' because this would match both <A0,A1,...,AN> and
<B0,B1,...,BM>.  If a reference needs author i we only have to call
need_author(j) for some j >= i such that the reference also needs
author j. */

/* This function handles 2 tasks:
determine which authors are needed (cannot be elided with et al.);
determine which authors can have only last names in the labels.

References >= start and < end have the same first i author names.
Also they're sorted by A+. */

static void consider_authors(reference **start, reference **end, int i)
{
  if (start >= end)
    return;
  reference **p = start;
  if (i >= (*p)->get_nauthors()) {
    for (++p; p < end && i >= (*p)->get_nauthors(); p++)
      ;
    if (p < end && i > 0) {
      // If we have an author list <A B C> and an author list <A B C D>,
      // then both lists need C.
      for (reference **q = start; q < end; q++)
	(*q)->need_author(i - 1);
    }
    start = p;
  }
  while (p < end) {
    reference **last_name_start = p;
    reference **name_start = p;
    for (++p;
	 p < end && i < (*p)->get_nauthors()
	 && same_author_last_name(**last_name_start, **p, i);
	 p++) {
      if (!same_author_name(**name_start, **p, i)) {
	consider_authors(name_start, p, i + 1);
	name_start = p;
      }
    }
    consider_authors(name_start, p, i + 1);
    if (last_name_start == name_start) {
      for (reference **q = last_name_start; q < p; q++)
	(*q)->set_last_name_unambiguous(i);
    }
    // If we have an author list <A B C D> and <A B C E>, then the lists
    // need author D and E respectively.
    if (name_start > start || p < end) {
      for (reference **q = last_name_start; q < p; q++)
	(*q)->need_author(i);
    }
  }
}

int same_author_last_name(const reference &r1, const reference &r2, int n)
{
  const char *ae1;
  const char *as1 = r1.get_sort_field(0, n, 0, &ae1);
  assert(as1 != 0);
  const char *ae2;
  const char *as2 = r2.get_sort_field(0, n, 0, &ae2);
  assert(as2 != 0);
  return ae1 - as1 == ae2 - as2 && memcmp(as1, as2, ae1 - as1) == 0;
}

int same_author_name(const reference &r1, const reference &r2, int n)
{
  const char *ae1;
  const char *as1 = r1.get_sort_field(0, n, -1, &ae1);
  assert(as1 != 0);
  const char *ae2;
  const char *as2 = r2.get_sort_field(0, n, -1, &ae2);
  assert(as2 != 0);
  return ae1 - as1 == ae2 - as2 && memcmp(as1, as2, ae1 - as1) == 0;
}


void int_set::set(int i)
{
  assert(i >= 0);
  int bytei = i >> 3;
  if (bytei >= v.length()) {
    int old_length = v.length();
    v.set_length(bytei + 1);
    for (int j = old_length; j <= bytei; j++)
      v[j] = 0;
  }
  v[bytei] |= 1 << (i & 7);
}

int int_set::get(int i) const
{
  assert(i >= 0);
  int bytei = i >> 3;
  return bytei >= v.length() ? 0 : (v[bytei] & (1 << (i & 7))) != 0;
}

void reference::set_last_name_unambiguous(int i)
{
  last_name_unambiguous.set(i);
}

void reference::need_author(int n)
{
  if (n > last_needed_author)
    last_needed_author = n;
}

const char *reference::get_authors(const char **end) const
{
  if (!computed_authors) {
    ((reference *)this)->computed_authors = 1;
    string &result = ((reference *)this)->authors;
    int na = get_nauthors();
    result.clear();
    for (int i = 0; i < na; i++) {
      if (last_name_unambiguous.get(i)) {
	const char *e, *start = get_author_last_name(i, &e);
	assert(start != 0);
	result.append(start, e - start);
      }
      else {
	const char *e, *start = get_author(i, &e);
	assert(start != 0);
	result.append(start, e - start);
      }
      if (i == last_needed_author
	  && et_al.length() > 0
	  && et_al_min_elide > 0
	  && last_needed_author + et_al_min_elide < na
	  && na >= et_al_min_total) {
	result += et_al;
	break;
      }
      if (i < na - 1) {
	if (na == 2)
	  result += join_authors_exactly_two;
	else if (i < na - 2)
	  result += join_authors_default;
	else
	  result += join_authors_last_two;
      }
    }
  }
  const char *start = authors.contents();
  *end = start + authors.length();
  return start;
}

int reference::get_nauthors() const
{
  if (nauthors < 0) {
    const char *dummy;
    int na;
    for (na = 0; get_author(na, &dummy) != 0; na++)
      ;
    ((reference *)this)->nauthors = na;
  }
  return nauthors;
}
