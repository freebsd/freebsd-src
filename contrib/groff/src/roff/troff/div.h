// -*- C++ -*-
/* Copyright (C) 1989, 1990, 1991, 1992, 2001 Free Software Foundation, Inc.
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

class diversion {
  friend void do_divert(int append, int boxing);
  friend void end_diversions();
  diversion *prev;
  node *saved_line;
  hunits saved_width_total;
  int saved_space_total;
  hunits saved_saved_indent;
  hunits saved_target_text_length;
  int saved_prev_line_interrupted;
protected:
  symbol nm;
  vunits vertical_position;
  vunits high_water_mark;
public:
  vunits marked_place;
  diversion(symbol s = NULL_SYMBOL);
  virtual ~diversion();
  virtual void output(node *nd, int retain_size, vunits vs, vunits post_vs,
		      hunits width) = 0;
  virtual void transparent_output(unsigned char) = 0;
  virtual void transparent_output(node *) = 0;
  virtual void space(vunits distance, int forced = 0) = 0;
#ifdef COLUMN
  virtual void vjustify(symbol) = 0;
#endif /* COLUMN */
  vunits get_vertical_position() { return vertical_position; }
  vunits get_high_water_mark() { return high_water_mark; }
  virtual vunits distance_to_next_trap() = 0;
  void need(vunits);
  const char *get_diversion_name() { return nm.contents(); }
  virtual void set_diversion_trap(symbol, vunits) = 0;
  virtual void clear_diversion_trap() = 0;
  virtual void copy_file(const char *filename) = 0;
};

class macro;

class macro_diversion : public diversion {
  macro *mac;
  hunits max_width;
  symbol diversion_trap;
  vunits diversion_trap_pos;
public:
  macro_diversion(symbol, int);
  ~macro_diversion();
  void output(node *nd, int retain_size, vunits vs, vunits post_vs,
	      hunits width);
  void transparent_output(unsigned char);
  void transparent_output(node *);
  void space(vunits distance, int forced = 0);
#ifdef COLUMN
  void vjustify(symbol);
#endif /* COLUMN */
  vunits distance_to_next_trap();
  void set_diversion_trap(symbol, vunits);
  void clear_diversion_trap();
  void copy_file(const char *filename);
};

struct trap {
  trap *next;
  vunits position;
  symbol nm;
  trap(symbol, vunits, trap *);
};

struct output_file;

class top_level_diversion : public diversion {
  int page_number;
  int page_count;
  int last_page_count;
  vunits page_length;
  hunits prev_page_offset;
  hunits page_offset;
  trap *page_trap_list;
  trap *find_next_trap(vunits *);
  int have_next_page_number;
  int next_page_number;
  int ejecting_page;		// Is the current page being ejected?
public:
  int before_first_page;
  int no_space_mode;
  top_level_diversion();
  void output(node *nd, int retain_size, vunits vs, vunits post_vs,
	      hunits width);
  void transparent_output(unsigned char);
  void transparent_output(node *);
  void space(vunits distance, int forced = 0);
#ifdef COLUMN
  void vjustify(symbol);
#endif /* COLUMN */
  hunits get_page_offset() { return page_offset; }
  vunits get_page_length() { return page_length; }
  vunits distance_to_next_trap();
  void add_trap(symbol nm, vunits pos);
  void change_trap(symbol nm, vunits pos);
  void remove_trap(symbol);
  void remove_trap_at(vunits pos);
  void print_traps();
  int get_page_count() { return page_count; }
  int get_page_number() { return page_number; }
  int get_next_page_number();
  void set_page_number(int n) { page_number = n; }
  int begin_page();
  void set_next_page_number(int);
  void set_page_length(vunits);
  void copy_file(const char *filename);
  int get_ejecting() { return ejecting_page; }
  void set_ejecting() { ejecting_page = 1; }
  friend void page_offset();
  void set_diversion_trap(symbol, vunits);
  void clear_diversion_trap();
  void set_last_page() { last_page_count = page_count; }
};

extern top_level_diversion *topdiv;
extern diversion *curdiv;

extern int exit_started;
extern int done_end_macro;
extern int last_page_number;
extern int seen_last_page_ejector;

void spring_trap(symbol);	// implemented by input.c
extern int trap_sprung_flag;
void postpone_traps();
int unpostpone_traps();

void push_page_ejector();
void continue_page_eject();
void handle_first_page_transition();
void blank_line();

extern void cleanup_and_exit(int);
