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


// diversions

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

int exit_started = 0;		// the exit process has started
int done_end_macro = 0;		// the end macro (if any) has finished
int seen_last_page_ejector = 0;	// seen the LAST_PAGE_EJECTOR cookie
int last_page_number = 0;	// if > 0, the number of the last page
				// specified with -o
static int began_page_in_end_macro = 0;	// a new page was begun during the end macro

static int last_post_line_extra_space = 0; // needed for \n(.a
static int nl_reg_contents = -1;
static int dl_reg_contents = 0;
static int dn_reg_contents = 0;
static int vertical_position_traps_flag = 1;
static vunits truncated_space;
static vunits needed_space;

diversion::diversion(symbol s) 
: prev(0), nm(s), vertical_position(V0), high_water_mark(V0), marked_place(V0)
{
}

struct vertical_size {
  vunits pre_extra, post_extra, pre, post;
  vertical_size(vunits vs, vunits post_vs);
};

vertical_size::vertical_size(vunits vs, vunits post_vs)
: pre_extra(V0), post_extra(V0), pre(vs), post(post_vs)
{
}

void node::set_vertical_size(vertical_size *)
{
}

void extra_size_node::set_vertical_size(vertical_size *v)
{
  if (n < V0) {
    if (-n > v->pre_extra)
      v->pre_extra = -n;
  }
  else if (n > v->post_extra)
    v->post_extra = n;
}

void vertical_size_node::set_vertical_size(vertical_size *v)
{
  if (n < V0)
    v->pre = -n;
  else
    v->post = n;
}

top_level_diversion *topdiv;

diversion *curdiv;

void do_divert(int append)
{
  tok.skip();
  symbol nm = get_name();
  if (nm.is_null()) {
    if (curdiv->prev) {
      diversion *temp = curdiv;
      curdiv = curdiv->prev;
      delete temp;
    }
    else
      warning(WARN_DI, "diversion stack underflow");
  }
  else {
    macro_diversion *md = new macro_diversion(nm, append);
    md->prev = curdiv;
    curdiv = md;
  }
  skip_line();
}

void divert()
{
  do_divert(0);
}

void divert_append()
{
  do_divert(1);
}
  
void diversion::need(vunits n)
{
  vunits d = distance_to_next_trap();
  if (d < n) {
    space(d, 1);
    truncated_space = -d;
    needed_space = n;
  }
}

macro_diversion::macro_diversion(symbol s, int append)
: diversion(s), max_width(H0)
{
#if 0
  if (append) {
    /* We don't allow recursive appends eg:

      .da a
      .a
      .di
      
      This causes an infinite loop in troff anyway.
      This is because the user could do

      .as a foo

      in the diversion, and this would mess things up royally,
      since there would be two things appending to the same
      macro_header.
      To make it work, we would have to copy the _contents_
      of the macro into which we were diverting; this doesn't
      strike me as worthwhile.
      However,

      .di a
      .a
      .a
      .di

       will work and will make `a' contain two copies of what it contained
       before; in troff, `a' would contain nothing. */
    request_or_macro *rm 
      = (request_or_macro *)request_dictionary.remove(s);
    if (!rm || (mac = rm->to_macro()) == 0)
      mac = new macro;
  }
  else
    mac = new macro;
#endif
  // We can now catch the situation described above by comparing
  // the length of the charlist in the macro_header with the length
  // stored in the macro. When we detect this, we copy the contents.
  mac = new macro;
  if (append) {
    request_or_macro *rm 
      = (request_or_macro *)request_dictionary.lookup(s);
    if (rm) {
      macro *m = rm->to_macro();
      if (m)
	*mac = *m;
    }
  }
}

macro_diversion::~macro_diversion()
{
  request_or_macro *rm = (request_or_macro *)request_dictionary.lookup(nm);
  macro *m = rm ? rm->to_macro() : 0;
  if (m) {
    *m = *mac;
    delete mac;
  }
  else
    request_dictionary.define(nm, mac);
  mac = 0;
  dl_reg_contents = max_width.to_units();
  dn_reg_contents = vertical_position.to_units();
}

vunits macro_diversion::distance_to_next_trap()
{
  if (!diversion_trap.is_null() && diversion_trap_pos > vertical_position)
    return diversion_trap_pos - vertical_position;
  else
    // Substract vresolution so that vunits::vunits does not overflow.
    return vunits(INT_MAX - vresolution);
}

void macro_diversion::transparent_output(unsigned char c)
{
  mac->append(c);
}

void macro_diversion::transparent_output(node *n)
{
  mac->append(n);
}

void macro_diversion::output(node *nd, int retain_size,
			     vunits vs, vunits post_vs, hunits width)
{
  vertical_size v(vs, post_vs);
  while (nd != 0) {
    nd->set_vertical_size(&v);
    node *temp = nd;
    nd = nd->next;
    if (temp->interpret(mac)) {
      delete temp;
    }
    else {
#if 1
      temp->freeze_space();
#endif
      mac->append(temp);
    }
  }
  last_post_line_extra_space = v.post_extra.to_units();
  if (!retain_size) {
    v.pre = vs;
    v.post = post_vs;
  }
  if (width > max_width)
    max_width = width;
  vunits x = v.pre + v.pre_extra + v.post + v.post_extra;
  if (vertical_position_traps_flag
      && !diversion_trap.is_null() && diversion_trap_pos > vertical_position
      && diversion_trap_pos <= vertical_position + x) {
    vunits trunc = vertical_position + x - diversion_trap_pos;
    if (trunc > v.post)
      trunc = v.post;
    v.post -= trunc;
    x -= trunc;
    truncated_space = trunc;
    spring_trap(diversion_trap);
  }
  mac->append(new vertical_size_node(-v.pre));
  mac->append(new vertical_size_node(v.post));
  mac->append('\n');
  vertical_position += x;
  if (vertical_position - v.post > high_water_mark)
    high_water_mark = vertical_position - v.post;
}

void macro_diversion::space(vunits n, int)
{
  if (vertical_position_traps_flag
      && !diversion_trap.is_null() && diversion_trap_pos > vertical_position
      && diversion_trap_pos <= vertical_position + n) {
    truncated_space = vertical_position + n - diversion_trap_pos;
    n = diversion_trap_pos - vertical_position;
    spring_trap(diversion_trap);
  }
  else if (n + vertical_position < V0)
    n = -vertical_position;
  mac->append(new diverted_space_node(n));
  vertical_position += n;
}

void macro_diversion::copy_file(const char *filename)
{
  mac->append(new diverted_copy_file_node(filename));
}

top_level_diversion::top_level_diversion()
: page_number(0), page_count(0), last_page_count(-1),
  page_length(units_per_inch*11),
  prev_page_offset(units_per_inch), page_offset(units_per_inch),
  page_trap_list(0), have_next_page_number(0),
  ejecting_page(0), before_first_page(1), no_space_mode(0)
{
}

// find the next trap after pos

trap *top_level_diversion::find_next_trap(vunits *next_trap_pos)
{
  trap *next_trap = 0;
  for (trap *pt = page_trap_list; pt != 0; pt = pt->next)
    if (!pt->nm.is_null()) {
      if (pt->position >= V0) {
	if (pt->position > vertical_position 
	    && pt->position < page_length
	    && (next_trap == 0 || pt->position < *next_trap_pos)) {
	      next_trap = pt;
	      *next_trap_pos = pt->position;
	    }
      }
      else {
	vunits pos = pt->position;
	pos += page_length;
	if (pos > 0 && pos > vertical_position && (next_trap == 0 || pos < *next_trap_pos)) {
	  next_trap = pt;
	  *next_trap_pos = pos;
	}
      }
    }
  return next_trap;
}

vunits top_level_diversion::distance_to_next_trap()
{
  vunits d;
  if (!find_next_trap(&d))
    return page_length - vertical_position;
  else
    return d - vertical_position;
}

void top_level_diversion::output(node *nd, int retain_size,
				 vunits vs, vunits post_vs, hunits /*width*/)
{
  no_space_mode = 0;
  vunits next_trap_pos;
  trap *next_trap = find_next_trap(&next_trap_pos);
  if (before_first_page && begin_page()) 
    fatal("sorry, I didn't manage to begin the first page in time: use an explicit .br request");
  vertical_size v(vs, post_vs);
  for (node *tem = nd; tem != 0; tem = tem->next)
    tem->set_vertical_size(&v);
  last_post_line_extra_space = v.post_extra.to_units();
  if (!retain_size) {
    v.pre = vs;
    v.post = post_vs;
  }
  vertical_position += v.pre;
  vertical_position += v.pre_extra;
  the_output->print_line(page_offset, vertical_position, nd,
			 v.pre + v.pre_extra, v.post_extra);
  vertical_position += v.post_extra;
  if (vertical_position > high_water_mark)
    high_water_mark = vertical_position;
  if (vertical_position_traps_flag && vertical_position >= page_length)
    begin_page();
  else if (vertical_position_traps_flag
	   && next_trap != 0 && vertical_position >= next_trap_pos) {
    nl_reg_contents = vertical_position.to_units();
    truncated_space = v.post;
    spring_trap(next_trap->nm);
  }
  else if (v.post > V0) {
    vertical_position += v.post;
    if (vertical_position_traps_flag
	&& next_trap != 0 && vertical_position >= next_trap_pos) {
      truncated_space = vertical_position - next_trap_pos;
      vertical_position = next_trap_pos;
      nl_reg_contents = vertical_position.to_units();
      spring_trap(next_trap->nm);
    }
    else if (vertical_position_traps_flag && vertical_position >= page_length)
      begin_page();
    else
      nl_reg_contents = vertical_position.to_units();
  }
  else
    nl_reg_contents = vertical_position.to_units();
}

void top_level_diversion::transparent_output(unsigned char c)
{
  if (before_first_page && begin_page())
    // This can only happen with the transparent() request.
    fatal("sorry, I didn't manage to begin the first page in time: use an explicit .br request");
  const char *s = asciify(c);
  while (*s)
    the_output->transparent_char(*s++);
}

void top_level_diversion::transparent_output(node * /*n*/)
{
  error("can't transparently output node at top level");
}

void top_level_diversion::copy_file(const char *filename)
{
  if (before_first_page && begin_page())
    fatal("sorry, I didn't manage to begin the first page in time: use an explicit .br request");
  the_output->copy_file(page_offset, vertical_position, filename);
}

void top_level_diversion::space(vunits n, int forced)
{
  if (no_space_mode) {
    if (!forced)
      return;
    else
      no_space_mode = 0;
  }
  if (before_first_page) {
    if (begin_page()) {
      // This happens if there's a top of page trap, and the first-page
      // transition is caused by `'sp'.
      truncated_space = n > V0 ? n : V0;
      return;
    }
  }
  vunits next_trap_pos;
  trap *next_trap = find_next_trap(&next_trap_pos);
  vunits y = vertical_position + n;
  if (vertical_position_traps_flag && next_trap != 0 && y >= next_trap_pos) {
    vertical_position = next_trap_pos;
    nl_reg_contents = vertical_position.to_units();
    truncated_space = y - vertical_position;
    spring_trap(next_trap->nm);
  }
  else if (y < V0) {
    vertical_position = V0;
    nl_reg_contents = vertical_position.to_units();
  }
  else if (vertical_position_traps_flag && y >= page_length && n >= V0)
    begin_page();
  else {
    vertical_position = y;
    nl_reg_contents = vertical_position.to_units();
  }
}

trap::trap(symbol s, vunits n, trap *p)
     : next(p), position(n), nm(s)
{
}

void top_level_diversion::add_trap(symbol nm, vunits pos)
{
  trap *first_free_slot = 0;
  trap **p;
  for (p = &page_trap_list; *p; p = &(*p)->next) {
    if ((*p)->nm.is_null()) {
      if (first_free_slot == 0)
	first_free_slot = *p;
    }
    else if ((*p)->position == pos) {
      (*p)->nm = nm;
      return;
    }
  }
  if (first_free_slot) {
    first_free_slot->nm = nm;
    first_free_slot->position = pos;
  }
  else
    *p = new trap(nm, pos, 0);
}  

void top_level_diversion::remove_trap(symbol nm)
{
  for (trap *p = page_trap_list; p; p = p->next)
    if (p->nm == nm) {
      p->nm = NULL_SYMBOL;
      return;
    }
}

void top_level_diversion::remove_trap_at(vunits pos)
{
  for (trap *p = page_trap_list; p; p = p->next)
    if (p->position == pos) {
      p->nm = NULL_SYMBOL;
      return;
    }
}
      
void top_level_diversion::change_trap(symbol nm, vunits pos)
{
  for (trap *p = page_trap_list; p; p = p->next)
    if (p->nm == nm) {
      p->position = pos;
      return;
    }
}

void top_level_diversion::print_traps()
{
  for (trap *p = page_trap_list; p; p = p->next)
    if (p->nm.is_null())
      fprintf(stderr, "  empty\n");
    else
      fprintf(stderr, "%s\t%d\n", p->nm.contents(), p->position.to_units());
  fflush(stderr);
}

void end_diversions()
{
  while (curdiv != topdiv) {
    error("automatically ending diversion `%1' on exit",
	    curdiv->nm.contents());
    diversion *tem = curdiv;
    curdiv = curdiv->prev;
    delete tem;
  }
}

void cleanup_and_exit(int exit_code)
{
  if (the_output) {
    the_output->trailer(topdiv->get_page_length());
    delete the_output;
  }
  exit(exit_code);
}

// returns non-zero if it sprung a top of page trap

int top_level_diversion::begin_page()
{
  if (exit_started) {
    if (page_count == last_page_count
	? curenv->is_empty()
	: (done_end_macro && (seen_last_page_ejector || began_page_in_end_macro)))
      cleanup_and_exit(0);
    if (!done_end_macro)
      began_page_in_end_macro = 1;
  }
  if (last_page_number > 0 && page_number == last_page_number)
    cleanup_and_exit(0);
  if (!the_output)
    init_output();
  ++page_count;
  if (have_next_page_number) {
    page_number = next_page_number;
    have_next_page_number = 0;
  }
  else if (before_first_page == 1)
    page_number = 1;
  else
    page_number++;
  // spring the top of page trap if there is one
  vunits next_trap_pos;
  vertical_position = -vresolution;
  trap *next_trap = find_next_trap(&next_trap_pos);
  vertical_position = V0;
  high_water_mark = V0;
  ejecting_page = 0;
  // If before_first_page was 2, then the top of page transition was undone
  // using eg .nr nl 0-1.  See nl_reg::set_value.
  if (before_first_page != 2)
    the_output->begin_page(page_number, page_length);
  before_first_page = 0;
  nl_reg_contents = vertical_position.to_units();
  if (vertical_position_traps_flag && next_trap != 0 && next_trap_pos == V0) {
    truncated_space = V0;
    spring_trap(next_trap->nm);
    return 1;
  }
  else
    return 0;
}

void continue_page_eject()
{
  if (topdiv->get_ejecting()) {
    if (curdiv != topdiv)
      error("can't continue page ejection because of current diversion");
    else if (!vertical_position_traps_flag)
      error("can't continue page ejection because vertical position traps disabled");
    else {
      push_page_ejector();
      topdiv->space(topdiv->get_page_length(), 1);
    }
  }
}

void top_level_diversion::set_next_page_number(int n)
{
  next_page_number= n;
  have_next_page_number = 1;
}

int top_level_diversion::get_next_page_number()
{
  return have_next_page_number ? next_page_number : page_number + 1;
}

void top_level_diversion::set_page_length(vunits n)
{
  page_length = n;
}

diversion::~diversion()
{
}

void page_offset()
{
  hunits n;
  // The troff manual says that the default scaling indicator is v,
  // but it is in fact m: v wouldn't make sense for a horizontally
  // oriented request.
  if (!has_arg() || !get_hunits(&n, 'm', topdiv->page_offset))
    n = topdiv->prev_page_offset;
  topdiv->prev_page_offset = topdiv->page_offset;
  topdiv->page_offset = n;
  skip_line();
}

void page_length()
{
  vunits n;
  if (has_arg() && get_vunits(&n, 'v', topdiv->get_page_length()))
    topdiv->set_page_length(n);
  else
    topdiv->set_page_length(11*units_per_inch);
  skip_line();
}

void when_request()
{
  vunits n;
  if (get_vunits(&n, 'v')) {
    symbol s = get_name();
    if (s.is_null())
      topdiv->remove_trap_at(n);
    else
      topdiv->add_trap(s, n);
  }
  skip_line();
}

void begin_page()
{
  int got_arg = 0;
  int n;
  if (has_arg() && get_integer(&n, topdiv->get_page_number()))
    got_arg = 1;
  while (!tok.newline() && !tok.eof())
    tok.next();
  if (curdiv == topdiv) {
    if (topdiv->before_first_page) {
      if (!break_flag) {
	if (got_arg)
	  topdiv->set_next_page_number(n);
	if (got_arg || !topdiv->no_space_mode)
	  topdiv->begin_page();
      }
      else if (topdiv->no_space_mode && !got_arg)
	topdiv->begin_page();
      else {
	/* Given this

         .wh 0 x
	 .de x
	 .tm \\n%
	 ..
	 .bp 3

	 troff prints

	 1
	 3

	 This code makes groff do the same. */

	push_page_ejector();
	topdiv->begin_page();
	if (got_arg)
	  topdiv->set_next_page_number(n);
	topdiv->set_ejecting();
      }
    }
    else {
      push_page_ejector();
      if (break_flag)
	curenv->do_break();
      if (got_arg)
	topdiv->set_next_page_number(n);
      if (!(topdiv->no_space_mode && !got_arg))
	topdiv->set_ejecting();
    }
  }
  tok.next();
}

void no_space()
{
  if (curdiv == topdiv)
    topdiv->no_space_mode = 1;
  skip_line();
}

void restore_spacing()
{
  if (curdiv == topdiv)
    topdiv->no_space_mode = 0;
  skip_line();
}

/* It is necessary to generate a break before before reading the argument,
because otherwise arguments using | will be wrong. But if we just
generate a break as usual, then the line forced out may spring a trap
and thus push a macro onto the input stack before we have had a chance
to read the argument to the sp request. We resolve this dilemma by
setting, before generating the break, a flag which will postpone the
actual pushing of the macro associated with the trap sprung by the
outputting of the line forced out by the break till after we have read
the argument to the request.  If the break did cause a trap to be
sprung, then we don't actually do the space. */

void space_request()
{
  postpone_traps();
  if (break_flag)
    curenv->do_break();
  vunits n;
  if (!has_arg() || !get_vunits(&n, 'v'))
    n = curenv->get_vertical_spacing();
  while (!tok.newline() && !tok.eof())
    tok.next();
  if (!unpostpone_traps())
    curdiv->space(n);
  else
    // The line might have had line spacing that was truncated.
    truncated_space += n;
  tok.next();
}

void blank_line()
{
  curenv->do_break();
  if (!trap_sprung_flag)
    curdiv->space(curenv->get_vertical_spacing());
  else
    truncated_space += curenv->get_vertical_spacing();
}

/* need_space might spring a trap and so we must be careful that the
BEGIN_TRAP token is not skipped over. */

void need_space()
{
  vunits n;
  if (!has_arg() || !get_vunits(&n, 'v'))
    n = curenv->get_vertical_spacing();
  while (!tok.newline() && !tok.eof())
    tok.next();
  curdiv->need(n);
  tok.next();
}

void page_number()
{
  int n;
  if (has_arg() && get_integer(&n, topdiv->get_page_number()))
    topdiv->set_next_page_number(n);
  skip_line();
}

vunits saved_space;

void save_vertical_space()
{
  vunits x;
  if (get_vunits(&x, 'v')) {
    if (curdiv->distance_to_next_trap() > x)
      curdiv->space(x, 1);
    else
      saved_space = x;
  }
  skip_line();
}

void output_saved_vertical_space()
{
  while (!tok.newline() && !tok.eof())
    tok.next();
  if (saved_space > V0)
    curdiv->space(saved_space, 1);
  saved_space = V0;
  tok.next();
}

void flush_output()
{
  while (!tok.newline() && !tok.eof())
    tok.next();
  if (break_flag)
    curenv->do_break();
  if (the_output)
    the_output->flush();
  tok.next();
}

void macro_diversion::set_diversion_trap(symbol s, vunits n)
{
  diversion_trap = s;
  diversion_trap_pos = n;
}

void macro_diversion::clear_diversion_trap()
{
  diversion_trap = NULL_SYMBOL;
}

void top_level_diversion::set_diversion_trap(symbol, vunits)
{
  error("can't set diversion trap when no current diversion");
}

void top_level_diversion::clear_diversion_trap()
{
  error("can't set diversion trap when no current diversion");
}

void diversion_trap()
{
  vunits n;
  if (has_arg() && get_vunits(&n, 'v')) {
    symbol s = get_name();
    if (!s.is_null())
      curdiv->set_diversion_trap(s, n);
    else
      curdiv->clear_diversion_trap();
  }
  else
    curdiv->clear_diversion_trap();
  skip_line();
}

void change_trap()
{
  symbol s = get_name(1);
  if (!s.is_null()) {
    vunits x;
    if (has_arg() && get_vunits(&x, 'v'))
      topdiv->change_trap(s, x);
    else
      topdiv->remove_trap(s);
  }
  skip_line();
}

void print_traps()
{
  topdiv->print_traps();
  skip_line();
}

void mark()
{
  symbol s = get_name();
  if (s.is_null())
    curdiv->marked_place = curdiv->get_vertical_position();
  else if (curdiv == topdiv)
    set_number_reg(s, nl_reg_contents);
  else
    set_number_reg(s, curdiv->get_vertical_position().to_units());
  skip_line();
}

// This is truly bizarre.  It is documented in the SQ manual.

void return_request()
{
  vunits dist = curdiv->marked_place - curdiv->get_vertical_position();
  if (has_arg()) {
    if (tok.ch() == '-') {
      tok.next();
      vunits x;
      if (get_vunits(&x, 'v'))
	dist = -x;
    }
    else {
      vunits x;
      if (get_vunits(&x, 'v'))
	dist = x >= V0 ? x - curdiv->get_vertical_position() : V0;
    }
  }
  if (dist < V0)
    curdiv->space(dist);
  skip_line();
}

void vertical_position_traps()
{
  int n;
  if (has_arg() && get_integer(&n))
    vertical_position_traps_flag = (n != 0);
  else
    vertical_position_traps_flag = 1;
  skip_line();
}

class page_offset_reg : public reg {
public:
  int get_value(units *);
  const char *get_string();
};
  
int page_offset_reg::get_value(units *res)
{
  *res = topdiv->get_page_offset().to_units();
  return 1;
}

const char *page_offset_reg::get_string()
{
  return i_to_a(topdiv->get_page_offset().to_units());
}

class page_length_reg : public reg {
public:
  int get_value(units *);
  const char *get_string();
};
  
int page_length_reg::get_value(units *res)
{
  *res = topdiv->get_page_length().to_units();
  return 1;
}

const char *page_length_reg::get_string()
{
  return i_to_a(topdiv->get_page_length().to_units());
}

class vertical_position_reg : public reg {
public:
  int get_value(units *);
  const char *get_string();
};
  
int vertical_position_reg::get_value(units *res)
{
  if (curdiv == topdiv && topdiv->before_first_page)
    *res = -1;
  else
    *res = curdiv->get_vertical_position().to_units();
  return 1;
}

const char *vertical_position_reg::get_string()
{
  if (curdiv == topdiv && topdiv->before_first_page)
    return "-1";
  else
    return i_to_a(curdiv->get_vertical_position().to_units());
}

class high_water_mark_reg : public reg {
public:
  int get_value(units *);
  const char *get_string();
};
  
int high_water_mark_reg::get_value(units *res)
{
  *res = curdiv->get_high_water_mark().to_units();
  return 1;
}

const char *high_water_mark_reg::get_string()
{
  return i_to_a(curdiv->get_high_water_mark().to_units());
}

class distance_to_next_trap_reg : public reg {
public:
  int get_value(units *);
  const char *get_string();
};
  
int distance_to_next_trap_reg::get_value(units *res)
{
  *res = curdiv->distance_to_next_trap().to_units();
  return 1;
}

const char *distance_to_next_trap_reg::get_string()
{
  return i_to_a(curdiv->distance_to_next_trap().to_units());
}

class diversion_name_reg : public reg {
public:
  const char *get_string();
};

const char *diversion_name_reg::get_string()
{
  return curdiv->get_diversion_name();
}

class page_number_reg : public general_reg {
public:
  page_number_reg();
  int get_value(units *);
  void set_value(units);
};

page_number_reg::page_number_reg()
{
}

void page_number_reg::set_value(units n)
{
  topdiv->set_page_number(n);
}

int page_number_reg::get_value(units *res)
{
  *res = topdiv->get_page_number();
  return 1;
}

class next_page_number_reg : public reg {
public:
  const char *get_string();
};

const char *next_page_number_reg::get_string()
{
  return i_to_a(topdiv->get_next_page_number());
}

class page_ejecting_reg : public reg {
public:
  const char *get_string();
};

const char *page_ejecting_reg::get_string()
{
  return i_to_a(topdiv->get_ejecting());
}

class constant_vunits_reg : public reg {
  vunits *p;
public:
  constant_vunits_reg(vunits *);
  const char *get_string();
};

constant_vunits_reg::constant_vunits_reg(vunits *q) : p(q)
{
}

const char *constant_vunits_reg::get_string()
{
  return i_to_a(p->to_units());
}

class nl_reg : public variable_reg {
public:
  nl_reg();
  void set_value(units);
};

nl_reg::nl_reg() : variable_reg(&nl_reg_contents)
{
}

void nl_reg::set_value(units n)
{
  variable_reg::set_value(n);
  // Setting nl to a negative value when the vertical position in
  // the top-level diversion is 0 undoes the top of page transition,
  // so that the header macro will be called as if the top of page
  // transition hasn't happened.  This is used by Larry Wall's
  // wrapman program.  Setting before_first_page to 2 rather than 1,
  // tells top_level_diversion::begin_page not to call
  // output_file::begin_page again.
  if (n < 0 && topdiv->get_vertical_position() == V0)
    topdiv->before_first_page = 2;
}

void init_div_requests()
{
  init_request("wh", when_request);
  init_request("ch", change_trap);
  init_request("pl", page_length);
  init_request("po", page_offset);
  init_request("rs", restore_spacing);
  init_request("ns", no_space);
  init_request("sp", space_request);
  init_request("di", divert);
  init_request("da", divert_append);
  init_request("bp", begin_page);
  init_request("ne", need_space);
  init_request("pn", page_number);
  init_request("dt", diversion_trap);
  init_request("rt", return_request);
  init_request("mk", mark);
  init_request("sv", save_vertical_space);
  init_request("os", output_saved_vertical_space);
  init_request("fl", flush_output);
  init_request("vpt", vertical_position_traps);
  init_request("ptr", print_traps);
  number_reg_dictionary.define(".a",
		       new constant_int_reg(&last_post_line_extra_space));
  number_reg_dictionary.define(".z", new diversion_name_reg);
  number_reg_dictionary.define(".o", new page_offset_reg);
  number_reg_dictionary.define(".p", new page_length_reg);
  number_reg_dictionary.define(".d", new vertical_position_reg);
  number_reg_dictionary.define(".h", new high_water_mark_reg);
  number_reg_dictionary.define(".t", new distance_to_next_trap_reg);
  number_reg_dictionary.define("dl", new variable_reg(&dl_reg_contents));
  number_reg_dictionary.define("dn", new variable_reg(&dn_reg_contents));
  number_reg_dictionary.define("nl", new nl_reg);
  number_reg_dictionary.define(".vpt", 
		       new constant_int_reg(&vertical_position_traps_flag));
  number_reg_dictionary.define("%", new page_number_reg);
  number_reg_dictionary.define(".pn", new next_page_number_reg);
  number_reg_dictionary.define(".trunc",
			       new constant_vunits_reg(&truncated_space));
  number_reg_dictionary.define(".ne",
			       new constant_vunits_reg(&needed_space));
  number_reg_dictionary.define(".pe", new page_ejecting_reg);
}
