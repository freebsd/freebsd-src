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
Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. */

#ifdef COLUMN

#include "troff.h"
#include "symbol.h"
#include "dictionary.h"
#include "hvunits.h"
#include "env.h"
#include "request.h"
#include "node.h"
#include "token.h"
#include "div.h"
#include "reg.h"
#include "stringclass.h"

void output_file::vjustify(vunits, symbol)
{
  // do nothing
}

struct justification_spec;
struct output_line;

class column : public output_file {
private:
  output_file *out;
  vunits bottom;
  output_line *col;
  output_line **tail;
  void add_output_line(output_line *);
  void begin_page(int pageno, vunits page_length);
  void flush();
  void print_line(hunits, vunits, node *, vunits, vunits);
  void vjustify(vunits, symbol);
  void transparent_char(unsigned char c);
  void copy_file(hunits, vunits, const char *);
  int is_printing();
  void check_bottom();
public:
  column();
  ~column();
  void start();
  void output();
  void justify(const justification_spec &);
  void trim();
  void reset();
  vunits get_bottom();
  vunits get_last_extra_space();
  int is_active() { return out != 0; }
};

column *the_column = 0;

struct transparent_output_line;
struct vjustify_output_line;

class output_line {
  output_line *next;
public:
  output_line();
  virtual ~output_line();
  virtual void output(output_file *, vunits);
  virtual transparent_output_line *as_transparent_output_line();
  virtual vjustify_output_line *as_vjustify_output_line();
  virtual vunits distance();
  virtual vunits height();
  virtual void reset();
  virtual vunits extra_space();	// post line
  friend class column;
  friend class justification_spec;
};

class position_output_line : public output_line {
  vunits dist;
public:
  position_output_line(vunits);
  vunits distance();
};
  
class node_output_line : public position_output_line {
  node *nd;
  hunits page_offset;
  vunits before;
  vunits after;
public:
  node_output_line(vunits, node *, hunits, vunits, vunits);
  ~node_output_line();
  void output(output_file *, vunits);
  vunits height();
  vunits extra_space();
};

class vjustify_output_line : public position_output_line {
  vunits current;
  symbol typ;
public:
  vjustify_output_line(vunits dist, symbol);
  vunits height();
  vjustify_output_line *as_vjustify_output_line();
  void vary(vunits amount);
  void reset();
  symbol type();
};

inline symbol vjustify_output_line::type()
{
  return typ;
}

class copy_file_output_line : public position_output_line {
  symbol filename;
  hunits hpos;
public:
  copy_file_output_line(vunits, const char *, hunits);
  void output(output_file *, vunits);
};

class transparent_output_line : public output_line {
  string buf;
public:
  transparent_output_line();
  void output(output_file *, vunits);
  void append_char(unsigned char c);
  transparent_output_line *as_transparent_output_line();
};

output_line::output_line() : next(0)
{
}

output_line::~output_line()
{
}

void output_line::reset()
{
}

transparent_output_line *output_line::as_transparent_output_line()
{
  return 0;
}

vjustify_output_line *output_line::as_vjustify_output_line()
{
  return 0;
}

void output_line::output(output_file *, vunits)
{
}

vunits output_line::distance()
{
  return V0;
}

vunits output_line::height()
{
  return V0;
}

vunits output_line::extra_space()
{
  return V0;
}

position_output_line::position_output_line(vunits d)
: dist(d)
{
}

vunits position_output_line::distance()
{
  return dist;
}

node_output_line::node_output_line(vunits d, node *n, hunits po, vunits b, vunits a)
: position_output_line(d), nd(n), page_offset(po), before(b), after(a)
{
}

node_output_line::~node_output_line()
{
  delete_node_list(nd);
}

void node_output_line::output(output_file *out, vunits pos)
{
  out->print_line(page_offset, pos, nd, before, after);
  nd = 0;
}

vunits node_output_line::height()
{
  return after;
}

vunits node_output_line::extra_space()
{
  return after;
}

vjustify_output_line::vjustify_output_line(vunits d, symbol t)
: position_output_line(d), typ(t)
{
}

void vjustify_output_line::reset()
{
  current = V0;
}

vunits vjustify_output_line::height()
{
  return current;
}

vjustify_output_line *vjustify_output_line::as_vjustify_output_line()
{
  return this;
}

inline void vjustify_output_line::vary(vunits amount)
{
  current += amount;
}

transparent_output_line::transparent_output_line()
{
}

transparent_output_line *transparent_output_line::as_transparent_output_line()
{
  return this;
}

void transparent_output_line::append_char(unsigned char c)
{
  assert(c != 0);
  buf += c;
}

void transparent_output_line::output(output_file *out, vunits)
{
  int len = buf.length();
  for (int i = 0; i < len; i++)
    out->transparent_char(buf[i]);
}

copy_file_output_line::copy_file_output_line(vunits d, const char *f, hunits h)
: position_output_line(d), hpos(h), filename(f)
{
}

void copy_file_output_line::output(output_file *out, vunits pos)
{
  out->copy_file(hpos, pos, filename.contents());
}

column::column()
: bottom(V0), col(0), tail(&col), out(0)
{
}

column::~column()
{
  assert(out != 0);
  error("automatically outputting column before exiting");
  output();
  delete the_output;
}

void column::start()
{
  assert(out == 0);
  if (!the_output)
    init_output();
  assert(the_output != 0);
  out = the_output;
  the_output = this;
}

void column::begin_page(int pageno, vunits page_length)
{
  assert(out != 0);
  if (col) {
    error("automatically outputting column before beginning next page");
    output();
    the_output->begin_page(pageno, page_length);
  }
  else
    out->begin_page(pageno, page_length);
    
}

void column::flush()
{
  assert(out != 0);
  out->flush();
}

int column::is_printing()
{
  assert(out != 0);
  return out->is_printing();
}

vunits column::get_bottom()
{
  return bottom;
}

void column::add_output_line(output_line *ln)
{
  *tail = ln;
  bottom += ln->distance();
  bottom += ln->height();
  ln->next = 0;
  tail = &(*tail)->next;
}

void column::print_line(hunits page_offset, vunits pos, node *nd,
			vunits before, vunits after)
{
  assert(out != 0);
  add_output_line(new node_output_line(pos - bottom, nd, page_offset, before, after));
}

void column::vjustify(vunits pos, symbol typ)
{
  assert(out != 0);
  add_output_line(new vjustify_output_line(pos - bottom, typ));
}

void column::transparent_char(unsigned char c)
{
  assert(out != 0);
  transparent_output_line *tl = 0;
  if (*tail)
    tl = (*tail)->as_transparent_output_line();
  if (!tl) {
    tl = new transparent_output_line;
    add_output_line(tl);
  }
  tl->append_char(c);
}

void column::copy_file(hunits page_offset, vunits pos, const char *filename)
{
  assert(out != 0);
  add_output_line(new copy_file_output_line(pos - bottom, filename, page_offset));
}

void column::trim()
{
  output_line **spp = 0;
  for (output_line **pp = &col; *pp; pp = &(*pp)->next)
    if ((*pp)->as_vjustify_output_line() == 0)
      spp = 0;
    else if (!spp)
      spp = pp;
  if (spp) {
    output_line *ln = *spp;
    *spp = 0;
    tail = spp;
    while (ln) {
      output_line *tem = ln->next;
      bottom -= ln->distance();
      bottom -= ln->height();
      delete ln;
      ln = tem;
    }
  }
}

void column::reset()
{
  bottom = V0;
  for (output_line *ln = col; ln; ln = ln->next) {
    bottom += ln->distance();
    ln->reset();
    bottom += ln->height();
  }
}

void column::check_bottom()
{
  vunits b;
  for (output_line *ln = col; ln; ln = ln->next) {
    b += ln->distance();
    b += ln->height();
  }
  assert(b == bottom);
}

void column::output()
{
  assert(out != 0);
  vunits vpos(V0);
  output_line *ln = col;
  while (ln) {
    vpos += ln->distance();
    ln->output(out, vpos);
    vpos += ln->height();
    output_line *tem = ln->next;
    delete ln;
    ln = tem;
  }
  tail = &col;
  bottom = V0;
  col = 0;
  the_output = out;
  out = 0;
}

vunits column::get_last_extra_space()
{
  if (!col)
    return V0;
  for (output_line *p = col; p->next; p = p->next)
    ;
  return p->extra_space();
}

class justification_spec {
  vunits height;
  symbol *type;
  vunits *amount;
  int n;
  int maxn;
public:
  justification_spec(vunits);
  ~justification_spec();
  void append(symbol t, vunits v);
  void justify(output_line *, vunits *bottomp) const;
};

justification_spec::justification_spec(vunits h)
: height(h), n(0), maxn(10)
{
  type = new symbol[maxn];
  amount = new vunits[maxn];
}

justification_spec::~justification_spec()
{
  a_delete type;
  a_delete amount;
}

void justification_spec::append(symbol t, vunits v)
{
  if (v <= V0) {
    if (v < V0)
      warning(WARN_RANGE,
	      "maximum space for vertical justification must not be negative");
    else
      warning(WARN_RANGE,
	      "maximum space for vertical justification must not be zero");
    return;
  }
  if (n >= maxn) {
    maxn *= 2;
    symbol *old_type = type;
    type = new symbol[maxn];
    int i;
    for (i = 0; i < n; i++)
      type[i] = old_type[i];
    a_delete old_type;
    vunits *old_amount = amount;
    amount = new vunits[maxn];
    for (i = 0; i < n; i++)
      amount[i] = old_amount[i];
    a_delete old_amount;
  }
  assert(n < maxn);
  type[n] = t;
  amount[n] = v;
  n++;
}

void justification_spec::justify(output_line *col, vunits *bottomp) const
{
  if (*bottomp >= height)
    return;
  vunits total;
  output_line *p;
  for (p = col; p; p = p->next) {
    vjustify_output_line *sp = p->as_vjustify_output_line();
    if (sp) {
      symbol t = sp->type();
      for (int i = 0; i < n; i++) {
	if (t == type[i])
	  total += amount[i];
      }
    }
  }
  vunits gap = height - *bottomp;
  for (p = col; p; p = p->next) {
    vjustify_output_line *sp = p->as_vjustify_output_line();
    if (sp) {
      symbol t = sp->type();
      for (int i = 0; i < n; i++) {
	if (t == type[i]) {
	  if (total <= gap) {
	    sp->vary(amount[i]);
	    gap -= amount[i];
	  }
	  else {
	    // gap < total
	    vunits v = scale(amount[i], gap, total);
	    sp->vary(v);
	    gap -= v;
	  }
	  total -= amount[i];
	}
      }
    }
  }
  assert(total == V0);
  *bottomp = height - gap;
}
  
void column::justify(const justification_spec &js)
{
  check_bottom();
  js.justify(col, &bottom);
  check_bottom();
}

void column_justify()
{
  vunits height;
  if (!the_column->is_active())
    error("can't justify column - column not active");
  else if (get_vunits(&height, 'v')) {
    justification_spec js(height);
    symbol nm = get_long_name(1);
    if (!nm.is_null()) {
      vunits v;
      if (get_vunits(&v, 'v')) {
	js.append(nm, v);
	int err = 0;
	while (has_arg()) {
	  nm = get_long_name(1);
	  if (nm.is_null()) {
	    err = 1;
	    break;
	  }
	  if (!get_vunits(&v, 'v')) {
	    err = 1;
	    break;
	  }
	  js.append(nm, v);
	}
	if (!err)
	  the_column->justify(js);
      }
    }
  }
  skip_line();
}

void column_start()
{
  if (the_column->is_active())
    error("can't start column - column already active");
  else
    the_column->start();
  skip_line();
}

void column_output()
{
  if (!the_column->is_active())
    error("can't output column - column not active");
  else
    the_column->output();
  skip_line();
}

void column_trim()
{
  if (!the_column->is_active())
    error("can't trim column - column not active");
  else
    the_column->trim();
  skip_line();
}

void column_reset()
{
  if (!the_column->is_active())
    error("can't reset column - column not active");
  else
    the_column->reset();
  skip_line();
}

class column_bottom_reg : public reg {
public:
  const char *get_string();
};

const char *column_bottom_reg::get_string()
{
  return itoa(the_column->get_bottom().to_units());
}

class column_extra_space_reg : public reg {
public:
  const char *get_string();
};

const char *column_extra_space_reg::get_string()
{
  return itoa(the_column->get_last_extra_space().to_units());
}

class column_active_reg : public reg {
public:
  const char *get_string();
};

const char *column_active_reg::get_string()
{
  return the_column->is_active() ? "1" : "0";
}

static int no_vjustify_mode = 0;

class vjustify_node : public node {
  symbol typ;
public:
  vjustify_node(symbol);
  int reread(int *);
  const char *type();
  int same(node *);
  node *copy();
};

vjustify_node::vjustify_node(symbol t)
: typ(t)
{
}

node *vjustify_node::copy()
{
  return new vjustify_node(typ);
}

const char *vjustify_node::type()
{
  return "vjustify_node";
}

int vjustify_node::same(node *nd)
{
  return typ == ((vjustify_node *)nd)->typ;
}

int vjustify_node::reread(int *bolp)
{
  curdiv->vjustify(typ);
  *bolp = 1;
  return 1;
}

void macro_diversion::vjustify(symbol type)
{
  if (!no_vjustify_mode)
    mac->append(new vjustify_node(type));
}

void top_level_diversion::vjustify(symbol type)
{
  if (no_space_mode || no_vjustify_mode)
    return;
  assert(first_page_begun);	// I'm not sure about this.
  the_output->vjustify(vertical_position, type);
}

void no_vjustify()
{
  skip_line();
  no_vjustify_mode = 1;
}

void restore_vjustify()
{
  skip_line();
  no_vjustify_mode = 0;
}

void init_column_requests()
{
  the_column = new column;
  init_request("cols", column_start);
  init_request("colo", column_output);
  init_request("colj", column_justify);
  init_request("colr", column_reset);
  init_request("colt", column_trim);
  init_request("nvj", no_vjustify);
  init_request("rvj", restore_vjustify);
  number_reg_dictionary.define(".colb", new column_bottom_reg);
  number_reg_dictionary.define(".colx", new column_extra_space_reg);
  number_reg_dictionary.define(".cola", new column_active_reg);
  number_reg_dictionary.define(".nvj",
			       new constant_int_reg(&no_vjustify_mode));
}

#endif /* COLUMN */
