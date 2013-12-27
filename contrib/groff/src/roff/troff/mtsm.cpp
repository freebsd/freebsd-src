// -*- C++ -*-
/* Copyright (C) 2003, 2004 Free Software Foundation, Inc.
     Written by Gaius Mulley (gaius@glam.ac.uk)

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

#define DEBUGGING

extern int debug_state;

#include "troff.h"
#include "hvunits.h"
#include "stringclass.h"
#include "mtsm.h"
#include "env.h"

static int no_of_statems = 0;	// debugging aid

int_value::int_value()
: value(0), is_known(0)
{
}

int_value::~int_value()
{
}

void int_value::diff(FILE *fp, const char *s, int_value compare)
{
  if (differs(compare)) {
    fputs("x X ", fp);
    fputs(s, fp);
    fputs(" ", fp);
    fputs(i_to_a(compare.value), fp);
    fputs("\n", fp);
    value = compare.value;
    is_known = 1;
    if (debug_state)
      fflush(fp);
  }
}

void int_value::set(int v)
{
  is_known = 1;
  value = v;
}

void int_value::unset()
{
  is_known = 0;
}

void int_value::set_if_unknown(int v)
{
  if (!is_known)
    set(v);
}

int int_value::differs(int_value compare)
{
  return compare.is_known
	 && (!is_known || value != compare.value);
}

bool_value::bool_value()
{
}

bool_value::~bool_value()
{
}

void bool_value::diff(FILE *fp, const char *s, bool_value compare)
{
  if (differs(compare)) {
    fputs("x X ", fp);
    fputs(s, fp);
    fputs("\n", fp);
    value = compare.value;
    is_known = 1;
    if (debug_state)
      fflush(fp);
  }
}

units_value::units_value()
{
}

units_value::~units_value()
{
}

void units_value::diff(FILE *fp, const char *s, units_value compare)
{
  if (differs(compare)) {
    fputs("x X ", fp);
    fputs(s, fp);
    fputs(" ", fp);
    fputs(i_to_a(compare.value), fp);
    fputs("\n", fp);
    value = compare.value;
    is_known = 1;
    if (debug_state)
      fflush(fp);
  }
}

void units_value::set(hunits v)
{
  is_known = 1;
  value = v.to_units();
}

int units_value::differs(units_value compare)
{
  return compare.is_known
	 && (!is_known || value != compare.value);
}

string_value::string_value()
: value(string("")), is_known(0)
{
}

string_value::~string_value()
{
}

void string_value::diff(FILE *fp, const char *s, string_value compare)
{
  if (differs(compare)) {
    fputs("x X ", fp);
    fputs(s, fp);
    fputs(" ", fp);
    fputs(compare.value.contents(), fp);
    fputs("\n", fp);
    value = compare.value;
    is_known = 1;
  }
}

void string_value::set(string v)
{
  is_known = 1;
  value = v;
}

void string_value::unset()
{
  is_known = 0;
}

int string_value::differs(string_value compare)
{
  return compare.is_known
	 && (!is_known || value != compare.value);
}

statem::statem()
{
  issue_no = no_of_statems;
  no_of_statems++;
}

statem::statem(statem *copy)
{
  int i;
  for (i = 0; i < LAST_BOOL; i++)
    bool_values[i] = copy->bool_values[i];
  for (i = 0; i < LAST_INT; i++)
    int_values[i] = copy->int_values[i];
  for (i = 0; i < LAST_UNITS; i++)
    units_values[i] = copy->units_values[i];
  for (i = 0; i < LAST_STRING; i++)
    string_values[i] = copy->string_values[i];
  issue_no = copy->issue_no;
}

statem::~statem()
{
}

void statem::flush(FILE *fp, statem *compare)
{
  int_values[MTSM_FI].diff(fp, "devtag:.fi",
			   compare->int_values[MTSM_FI]);
  int_values[MTSM_RJ].diff(fp, "devtag:.rj",
			   compare->int_values[MTSM_RJ]);
  int_values[MTSM_SP].diff(fp, "devtag:.sp",
			   compare->int_values[MTSM_SP]);
  units_values[MTSM_IN].diff(fp, "devtag:.in",
			     compare->units_values[MTSM_IN]);
  units_values[MTSM_LL].diff(fp, "devtag:.ll",
			     compare->units_values[MTSM_LL]);
  units_values[MTSM_PO].diff(fp, "devtag:.po",
			     compare->units_values[MTSM_PO]);
  string_values[MTSM_TA].diff(fp, "devtag:.ta",
			      compare->string_values[MTSM_TA]);
  units_values[MTSM_TI].diff(fp, "devtag:.ti",
			     compare->units_values[MTSM_TI]);
  int_values[MTSM_CE].diff(fp, "devtag:.ce",
			   compare->int_values[MTSM_CE]);
  bool_values[MTSM_EOL].diff(fp, "devtag:.eol",
			     compare->bool_values[MTSM_EOL]);
  bool_values[MTSM_BR].diff(fp, "devtag:.br",
			    compare->bool_values[MTSM_BR]);
  if (debug_state) {
    fprintf(stderr, "compared state %d\n", compare->issue_no);
    fflush(stderr);
  }
}

void statem::add_tag(int_value_state t, int v)
{
  int_values[t].set(v);
}

void statem::add_tag(units_value_state t, hunits v)
{
  units_values[t].set(v);
}

void statem::add_tag(bool_value_state t)
{
  bool_values[t].set(1);
}

void statem::add_tag(string_value_state t, string v)
{
  string_values[t].set(v);
}

void statem::add_tag_if_unknown(int_value_state t, int v)
{
  int_values[t].set_if_unknown(v);
}

void statem::sub_tag_ce()
{
  int_values[MTSM_CE].unset();
}

/*
 *  add_tag_ta - add the tab settings to the minimum troff state machine
 */

void statem::add_tag_ta()
{
  if (is_html) {
    string s = string("");
    hunits d, l;
    enum tab_type t;
    do {
      t = curenv->tabs.distance_to_next_tab(l, &d);
      l += d;
      switch (t) {
      case TAB_LEFT:
	s += " L ";
	s += as_string(l.to_units());
	break;
      case TAB_CENTER:
	s += " C ";
	s += as_string(l.to_units());
	break;
      case TAB_RIGHT:
	s += " R ";
	s += as_string(l.to_units());
	break;
      case TAB_NONE:
	break;
      }
    } while (t != TAB_NONE && l < curenv->get_line_length());
    s += '\0';
    string_values[MTSM_TA].set(s);
  }
}

void statem::update(statem *older, statem *newer, int_value_state t)
{
  if (newer->int_values[t].differs(older->int_values[t])
      && !newer->int_values[t].is_known)
    newer->int_values[t].set(older->int_values[t].value);
}

void statem::update(statem *older, statem *newer, units_value_state t)
{
  if (newer->units_values[t].differs(older->units_values[t])
      && !newer->units_values[t].is_known)
    newer->units_values[t].set(older->units_values[t].value);
}

void statem::update(statem *older, statem *newer, bool_value_state t)
{
  if (newer->bool_values[t].differs(older->bool_values[t])
      && !newer->bool_values[t].is_known)
    newer->bool_values[t].set(older->bool_values[t].value);
}

void statem::update(statem *older, statem *newer, string_value_state t)
{
  if (newer->string_values[t].differs(older->string_values[t])
      && !newer->string_values[t].is_known)
    newer->string_values[t].set(older->string_values[t].value);
}

void statem::merge(statem *newer, statem *older)
{
  if (newer == 0 || older == 0)
    return;
  update(older, newer, MTSM_EOL);
  update(older, newer, MTSM_BR);
  update(older, newer, MTSM_FI);
  update(older, newer, MTSM_LL);
  update(older, newer, MTSM_PO);
  update(older, newer, MTSM_RJ);
  update(older, newer, MTSM_SP);
  update(older, newer, MTSM_TA);
  update(older, newer, MTSM_TI);
  update(older, newer, MTSM_CE);
}

stack::stack()
: next(0), state(0)
{
}

stack::stack(statem *s, stack *n)
: next(n), state(s)
{
}

stack::~stack()
{
  if (state)
    delete state;
  if (next)
    delete next;
}

mtsm::mtsm()
: sp(0)
{
  driver = new statem();
}

mtsm::~mtsm()
{
  delete driver;
  if (sp)
    delete sp;
}

/*
 *  push_state - push the current troff state and use `n' as
 *               the new troff state.
 */

void mtsm::push_state(statem *n)
{
  if (is_html) {
#if defined(DEBUGGING)
    if (debug_state)
      fprintf(stderr, "--> state %d pushed\n", n->issue_no) ; fflush(stderr);
#endif
    sp = new stack(n, sp);
  }
}

void mtsm::pop_state()
{
  if (is_html) {
#if defined(DEBUGGING)
    if (debug_state)
      fprintf(stderr, "--> state popped\n") ; fflush(stderr);
#endif
    if (sp == 0)
      fatal("empty state machine stack");
    if (sp->state)
      delete sp->state;
    sp->state = 0;
    stack *t = sp;
    sp = sp->next;
    t->next = 0;
    delete t;
  }
}

/*
 *  inherit - scan the stack and collects inherited values.
 */

void mtsm::inherit(statem *s, int reset_bool)
{
  if (sp && sp->state) {
    if (s->units_values[MTSM_IN].is_known
	&& sp->state->units_values[MTSM_IN].is_known)
      s->units_values[MTSM_IN].value += sp->state->units_values[MTSM_IN].value;
    s->update(sp->state, s, MTSM_FI);
    s->update(sp->state, s, MTSM_LL);
    s->update(sp->state, s, MTSM_PO);
    s->update(sp->state, s, MTSM_RJ);
    s->update(sp->state, s, MTSM_TA);
    s->update(sp->state, s, MTSM_TI);
    s->update(sp->state, s, MTSM_CE);
    if (sp->state->bool_values[MTSM_BR].is_known
	&& sp->state->bool_values[MTSM_BR].value) {
      if (reset_bool)
	sp->state->bool_values[MTSM_BR].set(0);
      s->bool_values[MTSM_BR].set(1);
      if (debug_state)
	fprintf(stderr, "inherited br from pushed state %d\n",
		sp->state->issue_no);
    }
    else if (s->bool_values[MTSM_BR].is_known
	     && s->bool_values[MTSM_BR].value)
      if (! s->int_values[MTSM_CE].is_known)
	s->bool_values[MTSM_BR].unset();
    if (sp->state->bool_values[MTSM_EOL].is_known
	&& sp->state->bool_values[MTSM_EOL].value) {
      if (reset_bool)
	sp->state->bool_values[MTSM_EOL].set(0);
      s->bool_values[MTSM_EOL].set(1);
    }
  }
}

void mtsm::flush(FILE *fp, statem *s, string tag_list)
{
  if (is_html && s) {
    inherit(s, 1);
    driver->flush(fp, s);
    // Set rj, ce, ti to unknown if they were known and
    // we have seen an eol or br.  This ensures that these values
    // are emitted during the next glyph (as they step from n..0
    // at each newline).
    if ((driver->bool_values[MTSM_EOL].is_known
	 && driver->bool_values[MTSM_EOL].value)
	|| (driver->bool_values[MTSM_BR].is_known
	    && driver->bool_values[MTSM_BR].value)) {
      if (driver->units_values[MTSM_TI].is_known)
	driver->units_values[MTSM_TI].is_known = 0;
      if (driver->int_values[MTSM_RJ].is_known
	  && driver->int_values[MTSM_RJ].value > 0)
	driver->int_values[MTSM_RJ].is_known = 0;
      if (driver->int_values[MTSM_CE].is_known
	  && driver->int_values[MTSM_CE].value > 0)
	driver->int_values[MTSM_CE].is_known = 0;
    }
    // reset the boolean values
    driver->bool_values[MTSM_BR].set(0);
    driver->bool_values[MTSM_EOL].set(0);
    // reset space value
    driver->int_values[MTSM_SP].set(0);
    // lastly write out any direct tag entries
    if (tag_list != string("")) {
      string t = tag_list + '\0';
      fputs(t.contents(), fp);
    }
  }
}

/*
 *  display_state - dump out a synopsis of the state to stderr.
 */

void statem::display_state()
{
  fprintf(stderr, " <state ");
  if (bool_values[MTSM_BR].is_known) {
    if (bool_values[MTSM_BR].value)
      fprintf(stderr, "[br]");
    else
      fprintf(stderr, "[!br]");
  }
  if (bool_values[MTSM_EOL].is_known) {
    if (bool_values[MTSM_EOL].value)
      fprintf(stderr, "[eol]");
    else
      fprintf(stderr, "[!eol]");
  }
  if (int_values[MTSM_SP].is_known) {
    if (int_values[MTSM_SP].value)
      fprintf(stderr, "[sp %d]", int_values[MTSM_SP].value);
    else
      fprintf(stderr, "[!sp]");
  }
  fprintf(stderr, ">");
  fflush(stderr);
}

int mtsm::has_changed(int_value_state t, statem *s)
{
  return driver->int_values[t].differs(s->int_values[t]);
}

int mtsm::has_changed(units_value_state t, statem *s)
{
  return driver->units_values[t].differs(s->units_values[t]);
}

int mtsm::has_changed(bool_value_state t, statem *s)
{
  return driver->bool_values[t].differs(s->bool_values[t]);
}

int mtsm::has_changed(string_value_state t, statem *s)
{
  return driver->string_values[t].differs(s->string_values[t]);
}

int mtsm::changed(statem *s)
{
  if (s == 0 || !is_html)
    return 0;
  s = new statem(s);
  inherit(s, 0);
  int result = has_changed(MTSM_EOL, s)
	       || has_changed(MTSM_BR, s)
	       || has_changed(MTSM_FI, s)
	       || has_changed(MTSM_IN, s)
	       || has_changed(MTSM_LL, s)
	       || has_changed(MTSM_PO, s)
	       || has_changed(MTSM_RJ, s)
	       || has_changed(MTSM_SP, s)
	       || has_changed(MTSM_TA, s)
	       || has_changed(MTSM_CE, s);
  delete s;
  return result;
}

void mtsm::add_tag(FILE *fp, string s)
{
  fflush(fp);
  s += '\0';
  fputs(s.contents(), fp);
}

/*
 *  state_set class
 */

state_set::state_set()
: boolset(0), intset(0), unitsset(0), stringset(0)
{
}

state_set::~state_set()
{
}

void state_set::incl(bool_value_state b)
{
  boolset |= 1 << (int)b;
}

void state_set::incl(int_value_state i)
{
  intset |= 1 << (int)i;
}

void state_set::incl(units_value_state u)
{
  unitsset |= 1 << (int)u;
}

void state_set::incl(string_value_state s)
{
  stringset |= 1 << (int)s;
}

void state_set::excl(bool_value_state b)
{
  boolset &= ~(1 << (int)b);
}

void state_set::excl(int_value_state i)
{
  intset &= ~(1 << (int)i);
}

void state_set::excl(units_value_state u)
{
  unitsset &= ~(1 << (int)u);
}

void state_set::excl(string_value_state s)
{
  stringset &= ~(1 << (int)s);
}

int state_set::is_in(bool_value_state b)
{
  return (boolset & (1 << (int)b)) != 0;
}

int state_set::is_in(int_value_state i)
{
  return (intset & (1 << (int)i)) != 0;
}

// Note: this used to have a bug s.t. it always tested for bit 0 (benl 18/5/11)
int state_set::is_in(units_value_state u)
{
  return (unitsset & (1 << (int)u)) != 0;
}

// Note: this used to have a bug s.t. it always tested for bit 0 (benl 18/5/11)
int state_set::is_in(string_value_state s)
{
  return (stringset & (1 << (int)s)) != 0;
}

void state_set::add(units_value_state, int n)
{
  unitsset += n;
}

units state_set::val(units_value_state)
{
  return unitsset;
}
