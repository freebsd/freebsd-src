// -*- C++ -*-
/* Copyright (C) 2003, 2004 Free Software Foundation, Inc.
 *
 *  mtsm.h
 *
 *    written by Gaius Mulley (gaius@glam.ac.uk)
 *
 *  provides a minimal troff state machine which is necessary to
 *  emit meta tags for the post-grohtml device driver.
 */

/*
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

struct int_value {
  int value;
  int is_known;
  int_value();
  ~int_value();
  void diff(FILE *, const char *, int_value);
  int differs(int_value);
  void set(int);
  void unset();
  void set_if_unknown(int);
};

struct bool_value : public int_value {
  bool_value();
  ~bool_value();
  void diff(FILE *, const char *, bool_value);
};

struct units_value : public int_value {
  units_value();
  ~units_value();
  void diff(FILE *, const char *, units_value);
  int differs(units_value);
  void set(hunits);
};

struct string_value {
  string value;
  int is_known;
  string_value();
  ~string_value();
  void diff(FILE *, const char *, string_value);
  int differs(string_value);
  void set(string);
  void unset();
};

enum bool_value_state {
  MTSM_EOL,
  MTSM_BR,
  LAST_BOOL
};
enum int_value_state {
  MTSM_FI,
  MTSM_RJ,
  MTSM_CE,
  MTSM_SP,
  LAST_INT
};
enum units_value_state {
  MTSM_IN,
  MTSM_LL,
  MTSM_PO,
  MTSM_TI,
  LAST_UNITS
};
enum string_value_state {
  MTSM_TA,
  LAST_STRING
};

struct statem {
  int issue_no;
  bool_value bool_values[LAST_BOOL];
  int_value int_values[LAST_INT];
  units_value units_values[LAST_UNITS];
  string_value string_values[LAST_STRING];
  statem();
  statem(statem *);
  ~statem();
  void flush(FILE *, statem *);
  int changed(statem *);
  void merge(statem *, statem *);
  void add_tag(int_value_state, int);
  void add_tag(bool_value_state);
  void add_tag(units_value_state, hunits);
  void add_tag(string_value_state, string);
  void sub_tag_ce();
  void add_tag_if_unknown(int_value_state, int);
  void add_tag_ta();
  void display_state();
  void update(statem *, statem *, int_value_state);
  void update(statem *, statem *, bool_value_state);
  void update(statem *, statem *, units_value_state);
  void update(statem *, statem *, string_value_state);
};

struct stack {
  stack *next;
  statem *state;
  stack();
  stack(statem *, stack *);
  ~stack();
};

class mtsm {
  statem *driver;
  stack *sp;
  int has_changed(int_value_state, statem *);
  int has_changed(bool_value_state, statem *);
  int has_changed(units_value_state, statem *);
  int has_changed(string_value_state, statem *);
  void inherit(statem *, int);
public:
  mtsm();
  ~mtsm();
  void push_state(statem *);
  void pop_state();
  void flush(FILE *, statem *, string);
  int changed(statem *);
  void add_tag(FILE *, string);
};

class state_set {
  int boolset;
  int intset;
  int unitsset;
  int stringset;
public:
  state_set();
  ~state_set();
  void incl(bool_value_state);
  void incl(int_value_state);
  void incl(units_value_state);
  void incl(string_value_state);
  void excl(bool_value_state);
  void excl(int_value_state);
  void excl(units_value_state);
  void excl(string_value_state);
  int is_in(bool_value_state);
  int is_in(int_value_state);
  int is_in(units_value_state);
  int is_in(string_value_state);
  void add(units_value_state, int);
  units val(units_value_state);
};
