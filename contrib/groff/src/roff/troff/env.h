// -*- C++ -*-
/* Copyright (C) 1989, 1990, 1991, 1992, 2000, 2001
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

struct size_range {
  int min;
  int max;
};

class font_size {
  static size_range *size_table;
  static int nranges;
  int p;
public:
  font_size();
  font_size(int points);
  int to_points();
  int to_scaled_points();
  int to_units();
  int operator==(font_size);
  int operator!=(font_size);
  static void init_size_table(int *sizes);
};

inline font_size::font_size() : p(0)
{
}

inline int font_size::operator==(font_size fs)
{
  return p == fs.p;
}

inline int font_size::operator!=(font_size fs)
{
  return p != fs.p;
}

inline int font_size::to_scaled_points()
{
  return p;
}

inline int font_size::to_points()
{
  return p/sizescale;
}

struct environment;

hunits env_digit_width(environment *);
hunits env_space_width(environment *);
hunits env_sentence_space_width(environment *);
hunits env_narrow_space_width(environment *);
hunits env_half_narrow_space_width(environment *);

struct tab;

enum tab_type { TAB_NONE, TAB_LEFT, TAB_CENTER, TAB_RIGHT };

class tab_stops {
  tab *initial_list;
  tab *repeated_list;
public:
  tab_stops();
  tab_stops(hunits distance, tab_type type);
  tab_stops(const tab_stops &);
  ~tab_stops();
  void operator=(const tab_stops &);
  tab_type distance_to_next_tab(hunits pos, hunits *distance);
  void clear();
  void add_tab(hunits pos, tab_type type, int repeated);
  const char *to_string();
};

const unsigned MARGIN_CHARACTER_ON = 1;
const unsigned MARGIN_CHARACTER_NEXT = 2;

struct charinfo;
struct node;
struct breakpoint;
struct font_family;
struct pending_output_line;

class environment {
  int dummy;			// dummy environment used for \w
  hunits prev_line_length;
  hunits line_length;
  hunits prev_title_length;
  hunits title_length;
  font_size prev_size;
  font_size size;
  int requested_size;
  int prev_requested_size;
  int char_height;
  int char_slant;
  int prev_fontno;
  int fontno;
  font_family *prev_family;
  font_family *family;
  int space_size;		// in 36ths of an em
  int sentence_space_size;	// same but for spaces at the end of sentences
  int adjust_mode;
  int fill;
  int interrupted;
  int prev_line_interrupted;
  int center_lines;
  int right_justify_lines;
  vunits prev_vertical_spacing;
  vunits vertical_spacing;
  vunits prev_post_vertical_spacing;
  vunits post_vertical_spacing;
  int prev_line_spacing;
  int line_spacing;
  hunits prev_indent;
  hunits indent;
  hunits temporary_indent;
  int have_temporary_indent;
  hunits saved_indent;
  hunits target_text_length;
  int pre_underline_fontno;
  int underline_lines;
  int underline_spaces;
  symbol input_trap;
  int input_trap_count;
  node *line;			// in reverse order
  hunits prev_text_length;
  hunits width_total;
  int space_total;
  hunits input_line_start;
  tab_stops tabs;
  node *tab_contents;
  hunits tab_width;
  hunits tab_distance;
  int line_tabs;
  tab_type current_tab;
  node *leader_node;
  charinfo *tab_char;
  charinfo *leader_char;
  int current_field;		// is there a current field?
  hunits field_distance;
  hunits pre_field_width;
  int field_spaces;
  int tab_field_spaces;
  int tab_precedes_field;
  int discarding;
  int spread_flag;		// set by \p
  unsigned margin_character_flags;
  node *margin_character_node;
  hunits margin_character_distance;
  node *numbering_nodes;
  hunits line_number_digit_width;
  int number_text_separation;	// in digit spaces
  int line_number_indent;	// in digit spaces
  int line_number_multiple;
  int no_number_count;
  unsigned hyphenation_flags;
  int hyphen_line_count;
  int hyphen_line_max;
  hunits hyphenation_space;
  hunits hyphenation_margin;
  int composite;		// used for construction of composite char?
  pending_output_line *pending_lines;
#ifdef WIDOW_CONTROL
  int widow_control;
#endif /* WIDOW_CONTROL */
  int need_eol;
  int ignore_next_eol;
  int emitted_node;    // have we emitted a node since the last html eol tag?

  tab_type distance_to_next_tab(hunits *);
  void start_line();
  void output_line(node *, hunits);
  void output(node *nd, int retain_size, vunits vs, vunits post_vs,
	      hunits width);
  void output_title(node *nd, int retain_size, vunits vs, vunits post_vs,
		    hunits width);
#ifdef WIDOW_CONTROL
  void mark_last_line();
#endif /* WIDOW_CONTROL */
  breakpoint *choose_breakpoint();
  void hyphenate_line(int start_here = 0);
  void start_field();
  void wrap_up_field();
  void add_padding();
  node *make_tab_node(hunits d, node *next = 0);
  node *get_prev_char();
public:
  const symbol name;
  unsigned char control_char;
  unsigned char no_break_control_char;
  charinfo *hyphen_indicator_char;
  
  environment(symbol);
  environment(const environment *);	// for temporary environment
  ~environment();
  void copy(const environment *);
  int is_dummy() { return dummy; }
  int is_empty();
  int is_composite() { return composite; }
  void set_composite() { composite = 1; }
  vunits get_vertical_spacing(); // .v
  vunits get_post_vertical_spacing(); // .pvs
  int get_line_spacing();	 // .L
  vunits total_post_vertical_spacing();
  int get_point_size() { return size.to_scaled_points(); }
  font_size get_font_size() { return size; }
  int get_size() { return size.to_units(); }
  int get_requested_point_size() { return requested_size; }
  int get_char_height() { return char_height; }
  int get_char_slant() { return char_slant; }
  hunits get_digit_width();
  int get_font() { return fontno; }; // .f
  font_family *get_family() { return family; }
  int get_bold();		// .b
  int get_adjust_mode();	// .j
  int get_fill();		// .u
  hunits get_indent();		// .i
  hunits get_temporary_indent();
  hunits get_line_length();	 // .l
  hunits get_saved_line_length(); // .ll
  hunits get_saved_indent();	  // .in
  hunits get_title_length();
  hunits get_prev_char_width();	// .w
  hunits get_prev_char_skew();
  vunits get_prev_char_height();
  vunits get_prev_char_depth();
  hunits get_text_length();	// .k 
  hunits get_prev_text_length(); // .n
  hunits get_space_width() { return env_space_width(this); }
  int get_space_size() { return space_size; }	// in ems/36
  int get_sentence_space_size() { return sentence_space_size; }
  hunits get_narrow_space_width() { return env_narrow_space_width(this); }
  hunits get_half_narrow_space_width()
    { return env_half_narrow_space_width(this); }
  hunits get_input_line_position();
  const char *get_tabs();
  int get_line_tabs();
  int get_hyphenation_flags();
  int get_hyphen_line_max();
  int get_hyphen_line_count();
  hunits get_hyphenation_space();
  hunits get_hyphenation_margin();
  int get_center_lines();
  int get_right_justify_lines();
  int get_prev_line_interrupted() { return prev_line_interrupted; }
  node *make_char_node(charinfo *);
  node *extract_output_line();
  void width_registers();
  void wrap_up_tab();
  void set_font(int);
  void set_font(symbol);
  void set_family(symbol);
  void set_size(int);
  void set_char_height(int);
  void set_char_slant(int);
  void set_input_line_position(hunits);	// used by \n(hp
  void interrupt();
  void spread() { spread_flag = 1; }
  void possibly_break_line(int start_here = 0, int forced = 0);
  void do_break();			// .br
  void final_break();
  void add_html_tag_eol();
  void add_html_tag(const char *);
  void add_html_tag(const char *, int);
  void add_html_tag_tabs();
  void newline();
  void handle_tab(int is_leader = 0); // do a tab or leader
  void add_node(node *);
  void add_char(charinfo *);
  void add_hyphen_indicator();
  void add_italic_correction();
  void space();
  void space(hunits, hunits);
  void space_newline();
  const char *get_font_family_string();
  const char *get_name_string();
  const char *get_point_size_string();
  const char *get_requested_point_size_string();
  void output_pending_lines();
  
  friend void title_length();
  friend void space_size();
  friend void fill();
  friend void no_fill();
  friend void adjust();
  friend void no_adjust();
  friend void center();
  friend void right_justify();
  friend void vertical_spacing();
  friend void post_vertical_spacing();
  friend void line_spacing();
  friend void line_length();
  friend void indent();
  friend void temporary_indent();
  friend void do_underline(int);
  friend void input_trap();
  friend void set_tabs();
  friend void margin_character();
  friend void no_number();
  friend void number_lines();
  friend void leader_character();
  friend void tab_character();
  friend void hyphenate_request();
  friend void no_hyphenate();
  friend void hyphen_line_max_request();
  friend void hyphenation_space_request();
  friend void hyphenation_margin_request();
  friend void line_width();
#if 0
  friend void tabs_save();
  friend void tabs_restore();
#endif
  friend void line_tabs_request();
  friend void title();
#ifdef WIDOW_CONTROL
  friend void widow_control_request();
#endif /* WIDOW_CONTROL */

  friend void do_divert(int append, int boxing);
};
	
extern environment *curenv;
extern void pop_env();
extern void push_env(int);

void init_environments();
void read_hyphen_file(const char *name);

extern int break_flag;
extern int compatible_flag;
extern symbol default_family;
extern int translate_space_to_dummy;
