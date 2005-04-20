// -*- C++ -*-
/* Copyright (C) 1989, 1990, 1991, 1992, 2001, 2002
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
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */


#include "troff.h"
#include "symbol.h"
#include "hvunits.h"
#include "env.h"
#include "token.h"
#include "div.h"

vunits V0;
hunits H0;

int hresolution = 1;
int vresolution = 1;
int units_per_inch;
int sizescale;

static int parse_expr(units *v, int scale_indicator,
		      int parenthesised, int rigid = 0);
static int start_number();

int get_vunits(vunits *res, unsigned char si)
{
  if (!start_number())
    return 0;
  units x;
  if (parse_expr(&x, si, 0)) {
    *res = vunits(x);
    return 1;
  }
  else
    return 0;
}

int get_hunits(hunits *res, unsigned char si)
{
  if (!start_number())
    return 0;
  units x;
  if (parse_expr(&x, si, 0)) {
    *res = hunits(x);
    return 1;
  }
  else
    return 0;
}

// for \B

int get_number_rigidly(units *res, unsigned char si)
{
  if (!start_number())
    return 0;
  units x;
  if (parse_expr(&x, si, 0, 1)) {
    *res = x;
    return 1;
  }
  else
    return 0;
}

int get_number(units *res, unsigned char si)
{
  if (!start_number())
    return 0;
  units x;
  if (parse_expr(&x, si, 0)) {
    *res = x;
    return 1;
  }
  else
    return 0;
}

int get_integer(int *res)
{
  if (!start_number())
    return 0;
  units x;
  if (parse_expr(&x, 0, 0)) {
    *res = x;
    return 1;
  }
  else
    return 0;
}

enum incr_number_result { BAD, ABSOLUTE, INCREMENT, DECREMENT };

static incr_number_result get_incr_number(units *res, unsigned char);

int get_vunits(vunits *res, unsigned char si, vunits prev_value)
{
  units v;
  switch (get_incr_number(&v, si)) {
  case BAD:
    return 0;
  case ABSOLUTE:
    *res = v;
    break;
  case INCREMENT:
    *res = prev_value + v;
    break;
  case DECREMENT:
    *res = prev_value - v;
    break;
  default:
    assert(0);
  }
  return 1;
}

int get_hunits(hunits *res, unsigned char si, hunits prev_value)
{
  units v;
  switch (get_incr_number(&v, si)) {
  case BAD:
    return 0;
  case ABSOLUTE:
    *res = v;
    break;
  case INCREMENT:
    *res = prev_value + v;
    break;
  case DECREMENT:
    *res = prev_value - v;
    break;
  default:
    assert(0);
  }
  return 1;
}

int get_number(units *res, unsigned char si, units prev_value)
{
  units v;
  switch (get_incr_number(&v, si)) {
  case BAD:
    return 0;
  case ABSOLUTE:
    *res = v;
    break;
  case INCREMENT:
    *res = prev_value + v;
    break;
  case DECREMENT:
    *res = prev_value - v;
    break;
  default:
    assert(0);
  }
  return 1;
}

int get_integer(int *res, int prev_value)
{
  units v;
  switch (get_incr_number(&v, 0)) {
  case BAD:
    return 0;
  case ABSOLUTE:
    *res = v;
    break;
  case INCREMENT:
    *res = prev_value + int(v);
    break;
  case DECREMENT:
    *res = prev_value - int(v);
    break;
  default:
    assert(0);
  }
  return 1;
}


static incr_number_result get_incr_number(units *res, unsigned char si)
{
  if (!start_number())
    return BAD;
  incr_number_result result = ABSOLUTE;
  if (tok.ch() == '+') {
    tok.next();
    result = INCREMENT;
  }
  else if (tok.ch() == '-') {
    tok.next();
    result = DECREMENT;
  }
  if (parse_expr(res, si, 0))
    return result;
  else
    return BAD;
}

static int start_number()
{
  while (tok.space())
    tok.next();
  if (tok.newline()) {
    warning(WARN_MISSING, "missing number");
    return 0;
  }
  if (tok.tab()) {
    warning(WARN_TAB, "tab character where number expected");
    return 0;
  }
  if (tok.right_brace()) {
    warning(WARN_RIGHT_BRACE, "`\\}' where number expected");
    return 0;
  }
  return 1;
}

enum { OP_LEQ = 'L', OP_GEQ = 'G', OP_MAX = 'X', OP_MIN = 'N' };

#define SCALE_INDICATOR_CHARS "icfPmnpuvMsz"

static int parse_term(units *v, int scale_indicator,
		      int parenthesised, int rigid);

static int parse_expr(units *v, int scale_indicator,
		      int parenthesised, int rigid)
{
  int result = parse_term(v, scale_indicator, parenthesised, rigid);
  while (result) {
    if (parenthesised)
      tok.skip();
    int op = tok.ch();
    switch (op) {
    case '+':
    case '-':
    case '/':
    case '*':
    case '%':
    case ':':
    case '&':
      tok.next();
      break;
    case '>':
      tok.next();
      if (tok.ch() == '=') {
	tok.next();
	op = OP_GEQ;
      }
      else if (tok.ch() == '?') {
	tok.next();
	op = OP_MAX;
      }
      break;
    case '<':
      tok.next();
      if (tok.ch() == '=') {
	tok.next();
	op = OP_LEQ;
      }
      else if (tok.ch() == '?') {
	tok.next();
	op = OP_MIN;
      }
      break;
    case '=':
      tok.next();
      if (tok.ch() == '=')
	tok.next();
      break;
    default:
      return result;
    }
    units v2;
    if (!parse_term(&v2, scale_indicator, parenthesised, rigid))
      return 0;
    int overflow = 0;
    switch (op) {
    case '<':
      *v = *v < v2;
      break;
    case '>':
      *v = *v > v2;
      break;
    case OP_LEQ:
      *v = *v <= v2;
      break;
    case OP_GEQ:
      *v = *v >= v2;
      break;
    case OP_MIN:
      if (*v > v2)
	*v = v2;
      break;
    case OP_MAX:
      if (*v < v2)
	*v = v2;
      break;
    case '=':
      *v = *v == v2;
      break;
    case '&':
      *v = *v > 0 && v2 > 0;
      break;
    case ':':
      *v = *v > 0 || v2 > 0;
      break;
    case '+':
      if (v2 < 0) {
	if (*v < INT_MIN - v2)
	  overflow = 1;
      }
      else if (v2 > 0) {
	if (*v > INT_MAX - v2)
	  overflow = 1;
      }
      if (overflow) {
	error("addition overflow");
	return 0;
      }
      *v += v2;
      break;
    case '-':
      if (v2 < 0) {
	if (*v > INT_MAX + v2)
	  overflow = 1;
      }
      else if (v2 > 0) {
	if (*v < INT_MIN + v2)
	  overflow = 1;
      }
      if (overflow) {
	error("subtraction overflow");
	return 0;
      }
      *v -= v2;
      break;
    case '*':
      if (v2 < 0) {
	if (*v > 0) {
	  if (*v > -(unsigned)INT_MIN / -(unsigned)v2)
	    overflow = 1;
	}
	else if (-(unsigned)*v > INT_MAX / -(unsigned)v2)
	  overflow = 1;
      }
      else if (v2 > 0) {
	if (*v > 0) {
	  if (*v > INT_MAX / v2)
	    overflow = 1;
	}
	else if (-(unsigned)*v > -(unsigned)INT_MIN / v2)
	  overflow = 1;
      }
      if (overflow) {
	error("multiplication overflow");
	return 0;
      }
      *v *= v2;
      break;
    case '/':
      if (v2 == 0) {
	error("division by zero");
	return 0;
      }
      *v /= v2;
      break;
    case '%':
      if (v2 == 0) {
	error("modulus by zero");
	return 0;
      }
      *v %= v2;
      break;
    default:
      assert(0);
    }
  }
  return result;
}

static int parse_term(units *v, int scale_indicator,
		      int parenthesised, int rigid)
{
  int negative = 0;
  for (;;)
    if (parenthesised && tok.space())
      tok.next();
    else if (tok.ch() == '+')
      tok.next();
    else if (tok.ch() == '-') {
      tok.next();
      negative = !negative;
    }
    else
      break;
  unsigned char c = tok.ch();
  switch (c) {
  case '|':
    // | is not restricted to the outermost level
    // tbl uses this
    tok.next();
    if (!parse_term(v, scale_indicator, parenthesised, rigid))
      return 0;
    int tem;
    tem = (scale_indicator == 'v'
	   ? curdiv->get_vertical_position().to_units()
	   : curenv->get_input_line_position().to_units());
    if (tem >= 0) {
      if (*v < INT_MIN + tem) {
	error("numeric overflow");
	return 0;
      }
    }
    else {
      if (*v > INT_MAX + tem) {
	error("numeric overflow");
	return 0;
      }
    }
    *v -= tem;
    if (negative) {
      if (*v == INT_MIN) {
	error("numeric overflow");
	return 0;
      }
      *v = -*v;
    }
    return 1;
  case '(':
    tok.next();
    c = tok.ch();
    if (c == ')') {
      if (rigid)
	return 0;
      warning(WARN_SYNTAX, "empty parentheses");
      tok.next();
      *v = 0;
      return 1;
    }
    else if (c != 0 && strchr(SCALE_INDICATOR_CHARS, c) != 0) {
      tok.next();
      if (tok.ch() == ';') {
	tok.next();
	scale_indicator = c;
      }
      else {
	error("expected `;' after scale-indicator (got %1)",
	      tok.description());
	return 0;
      }
    }
    else if (c == ';') {
      scale_indicator = 0;
      tok.next();
    }
    if (!parse_expr(v, scale_indicator, 1, rigid))
      return 0;
    tok.skip();
    if (tok.ch() != ')') {
      if (rigid)
	return 0;
      warning(WARN_SYNTAX, "missing `)' (got %1)", tok.description());
    }
    else
      tok.next();
    if (negative) {
      if (*v == INT_MIN) {
	error("numeric overflow");
	return 0;
      }
      *v = -*v;
    }
    return 1;
  case '.':
    *v = 0;
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
    *v = 0;
    do {
      if (*v > INT_MAX/10) {
	error("numeric overflow");
	return 0;
      }
      *v *= 10;
      if (*v > INT_MAX - (int(c) - '0')) {
	error("numeric overflow");
	return 0;
      }
      *v += c - '0';
      tok.next();
      c = tok.ch();
    } while (csdigit(c));
    break;
  case '/':
  case '*':
  case '%':
  case ':':
  case '&':
  case '>':
  case '<':
  case '=':
    warning(WARN_SYNTAX, "empty left operand");
    *v = 0;
    return rigid ? 0 : 1;
  default:
    warning(WARN_NUMBER, "numeric expression expected (got %1)",
	    tok.description());
    return 0;
  }
  int divisor = 1;
  if (tok.ch() == '.') {
    tok.next();
    for (;;) {
      c = tok.ch();
      if (!csdigit(c))
	break;
      // we may multiply the divisor by 254 later on
      if (divisor <= INT_MAX/2540 && *v <= (INT_MAX - 9)/10) {
	*v *= 10;
	*v += c - '0';
	divisor *= 10;
      }
      tok.next();
    }
  }
  int si = scale_indicator;
  int do_next = 0;
  if ((c = tok.ch()) != 0 && strchr(SCALE_INDICATOR_CHARS, c) != 0) {
    switch (scale_indicator) {
    case 'z':
      if (c != 'u' && c != 'z') {
	warning(WARN_SCALE,
		"only `z' and `u' scale indicators valid in this context");
	break;
      }
      si = c;
      break;
    case 0:
      warning(WARN_SCALE, "scale indicator invalid in this context");
      break;
    case 'u':
      si = c;
      break;
    default:
      if (c == 'z') {
	warning(WARN_SCALE, "`z' scale indicator invalid in this context");
	break;
      }
      si = c;
      break;
    }
    // Don't do tok.next() here because the next token might be \s, which
    // would affect the interpretation of m.
    do_next = 1;
  }
  switch (si) {
  case 'i':
    *v = scale(*v, units_per_inch, divisor);
    break;
  case 'c':
    *v = scale(*v, units_per_inch*100, divisor*254);
    break;
  case 0:
  case 'u':
    if (divisor != 1)
      *v /= divisor;
    break;
  case 'f':
    *v = scale(*v, 65536, divisor);
    break;
  case 'p':
    *v = scale(*v, units_per_inch, divisor*72);
    break;
  case 'P':
    *v = scale(*v, units_per_inch, divisor*6);
    break;
  case 'm':
    {
      // Convert to hunits so that with -Tascii `m' behaves as in nroff.
      hunits em = curenv->get_size();
      *v = scale(*v, em.is_zero() ? hresolution : em.to_units(), divisor);
    }
    break;
  case 'M':
    {
      hunits em = curenv->get_size();
      *v = scale(*v, em.is_zero() ? hresolution : em.to_units(), divisor*100);
    }
    break;
  case 'n':
    {
      // Convert to hunits so that with -Tascii `n' behaves as in nroff.
      hunits en = curenv->get_size()/2;
      *v = scale(*v, en.is_zero() ? hresolution : en.to_units(), divisor);
    }
    break;
  case 'v':
    *v = scale(*v, curenv->get_vertical_spacing().to_units(), divisor);
    break;
  case 's':
    while (divisor > INT_MAX/(sizescale*72)) {
      divisor /= 10;
      *v /= 10;
    }
    *v = scale(*v, units_per_inch, divisor*sizescale*72);
    break;
  case 'z':
    *v = scale(*v, sizescale, divisor);
    break;
  default:
    assert(0);
  }
  if (do_next)
    tok.next();
  if (negative) {
    if (*v == INT_MIN) {
      error("numeric overflow");
      return 0;
    }
    *v = -*v;
  }
  return 1;
}

units scale(units n, units x, units y)
{
  assert(x >= 0 && y > 0);
  if (x == 0)
    return 0;
  if (n >= 0) {
    if (n <= INT_MAX/x)
      return (n*x)/y;
  }
  else {
    if (-(unsigned)n <= -(unsigned)INT_MIN/x)
      return (n*x)/y;
  }
  double res = n*double(x)/double(y);
  if (res > INT_MAX) {
    error("numeric overflow");
    return INT_MAX;
  }
  else if (res < INT_MIN) {
    error("numeric overflow");
    return INT_MIN;
  }
  return int(res);
}

vunits::vunits(units x)
{
  // don't depend on the rounding direction for division of negative integers
  if (vresolution == 1)
    n = x;
  else
    n = (x < 0
	 ? -((-x + vresolution/2 - 1)/vresolution)
	 : (x + vresolution/2 - 1)/vresolution);
}

hunits::hunits(units x)
{
  // don't depend on the rounding direction for division of negative integers
  if (hresolution == 1)
    n = x;
  else
    n = (x < 0
	 ? -((-x + hresolution/2 - 1)/hresolution)
	 : (x + hresolution/2 - 1)/hresolution);
}
