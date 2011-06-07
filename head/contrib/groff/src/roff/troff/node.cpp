// -*- C++ -*-
/* Copyright (C) 1989, 1990, 1991, 1992, 2000, 2001, 2002, 2003, 2004, 2005
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

extern int debug_state;

#include "troff.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "dictionary.h"
#include "hvunits.h"
#include "stringclass.h"
#include "mtsm.h"
#include "env.h"
#include "request.h"
#include "node.h"
#include "token.h"
#include "div.h"
#include "reg.h"
#include "charinfo.h"
#include "font.h"
#include "input.h"
#include "geometry.h"

#include "nonposix.h"

#ifdef _POSIX_VERSION

#include <sys/wait.h>

#else /* not _POSIX_VERSION */

/* traditional Unix */

#define WIFEXITED(s) (((s) & 0377) == 0)
#define WEXITSTATUS(s) (((s) >> 8) & 0377)
#define WTERMSIG(s) ((s) & 0177)
#define WIFSTOPPED(s) (((s) & 0377) == 0177)
#define WSTOPSIG(s) (((s) >> 8) & 0377)
#define WIFSIGNALED(s) (((s) & 0377) != 0 && (((s) & 0377) != 0177))

#endif /* not _POSIX_VERSION */

// declarations to avoid friend name injections
class tfont;
class tfont_spec;
tfont *make_tfont(tfont_spec &);


/*
 *  how many boundaries of images have been written? Useful for
 *  debugging grohtml
 */

int image_no = 0;
static int suppress_start_page = 0;

#define STORE_WIDTH 1

symbol HYPHEN_SYMBOL("hy");

// Character used when a hyphen is inserted at a line break.
static charinfo *soft_hyphen_char;

enum constant_space_type {
  CONSTANT_SPACE_NONE,
  CONSTANT_SPACE_RELATIVE,
  CONSTANT_SPACE_ABSOLUTE
  };

struct special_font_list {
  int n;
  special_font_list *next;
};

special_font_list *global_special_fonts;
static int global_ligature_mode = 1;
static int global_kern_mode = 1;

class track_kerning_function {
  int non_zero;
  units min_size;
  hunits min_amount;
  units max_size;
  hunits max_amount;
public:
  track_kerning_function();
  track_kerning_function(units, hunits, units, hunits);
  int operator==(const track_kerning_function &);
  int operator!=(const track_kerning_function &);
  hunits compute(int point_size);
};

// embolden fontno when this is the current font

struct conditional_bold {
  conditional_bold *next;
  int fontno;
  hunits offset;
  conditional_bold(int, hunits, conditional_bold * = 0);
};

class font_info {
  tfont *last_tfont;
  int number;
  font_size last_size;
  int last_height;
  int last_slant;
  symbol internal_name;
  symbol external_name;
  font *fm;
  char is_bold;
  hunits bold_offset;
  track_kerning_function track_kern;
  constant_space_type is_constant_spaced;
  units constant_space;
  int last_ligature_mode;
  int last_kern_mode;
  conditional_bold *cond_bold_list;
  void flush();
public:
  special_font_list *sf;
  font_info(symbol, int, symbol, font *);
  int contains(charinfo *);
  void set_bold(hunits);
  void unbold();
  void set_conditional_bold(int, hunits);
  void conditional_unbold(int);
  void set_track_kern(track_kerning_function &);
  void set_constant_space(constant_space_type, units = 0);
  int is_named(symbol);
  symbol get_name();
  tfont *get_tfont(font_size, int, int, int);
  hunits get_space_width(font_size, int);
  hunits get_narrow_space_width(font_size);
  hunits get_half_narrow_space_width(font_size);
  int get_bold(hunits *);
  int is_special();
  int is_style();
  friend symbol get_font_name(int, environment *);
  friend symbol get_style_name(int);
};

class tfont_spec {
protected:
  symbol name;
  int input_position;
  font *fm;
  font_size size;
  char is_bold;
  char is_constant_spaced;
  int ligature_mode;
  int kern_mode;
  hunits bold_offset;
  hunits track_kern;			// add this to the width
  hunits constant_space_width;
  int height;
  int slant;
public:
  tfont_spec(symbol, int, font *, font_size, int, int);
  tfont_spec(const tfont_spec &spec) { *this = spec; }
  tfont_spec plain();
  int operator==(const tfont_spec &);
  friend tfont *font_info::get_tfont(font_size fs, int, int, int);
};

class tfont : public tfont_spec {
  static tfont *tfont_list;
  tfont *next;
  tfont *plain_version;
public:
  tfont(tfont_spec &);
  int contains(charinfo *);
  hunits get_width(charinfo *c);
  int get_bold(hunits *);
  int get_constant_space(hunits *);
  hunits get_track_kern();
  tfont *get_plain();
  font_size get_size();
  symbol get_name();
  charinfo *get_lig(charinfo *c1, charinfo *c2);
  int get_kern(charinfo *c1, charinfo *c2, hunits *res);
  int get_input_position();
  int get_character_type(charinfo *);
  int get_height();
  int get_slant();
  vunits get_char_height(charinfo *);
  vunits get_char_depth(charinfo *);
  hunits get_char_skew(charinfo *);
  hunits get_italic_correction(charinfo *);
  hunits get_left_italic_correction(charinfo *);
  hunits get_subscript_correction(charinfo *);
  friend tfont *make_tfont(tfont_spec &);
};

inline int env_definite_font(environment *env)
{
  return env->get_family()->make_definite(env->get_font());
}

/* font_info functions */

static font_info **font_table = 0;
static int font_table_size = 0;

font_info::font_info(symbol nm, int n, symbol enm, font *f)
: last_tfont(0), number(n), last_size(0),
  internal_name(nm), external_name(enm), fm(f),
  is_bold(0), is_constant_spaced(CONSTANT_SPACE_NONE), last_ligature_mode(1),
  last_kern_mode(1), cond_bold_list(0), sf(0)
{
}

inline int font_info::contains(charinfo *ci)
{
  return fm != 0 && fm->contains(ci->get_index());
}

inline int font_info::is_special()
{
  return fm != 0 && fm->is_special();
}

inline int font_info::is_style()
{
  return fm == 0;
}

tfont *make_tfont(tfont_spec &spec)
{
  for (tfont *p = tfont::tfont_list; p; p = p->next)
    if (*p == spec)
      return p;
  return new tfont(spec);
}

// this is the current_font, fontno is where we found the character,
// presumably a special font

tfont *font_info::get_tfont(font_size fs, int height, int slant, int fontno)
{
  if (last_tfont == 0 || fs != last_size
      || height != last_height || slant != last_slant
      || global_ligature_mode != last_ligature_mode
      || global_kern_mode != last_kern_mode
      || fontno != number) {
	font_info *f = font_table[fontno];
	tfont_spec spec(f->external_name, f->number, f->fm, fs, height, slant);
	for (conditional_bold *p = cond_bold_list; p; p = p->next)
	  if (p->fontno == fontno) {
	    spec.is_bold = 1;
	    spec.bold_offset = p->offset;
	    break;
	  }
	if (!spec.is_bold && is_bold) {
	  spec.is_bold = 1;
	  spec.bold_offset = bold_offset;
	}
	spec.track_kern = track_kern.compute(fs.to_scaled_points());
	spec.ligature_mode = global_ligature_mode;
	spec.kern_mode = global_kern_mode;
	switch (is_constant_spaced) {
	case CONSTANT_SPACE_NONE:
	  break;
	case CONSTANT_SPACE_ABSOLUTE:
	  spec.is_constant_spaced = 1;
	  spec.constant_space_width = constant_space;
	  break;
	case CONSTANT_SPACE_RELATIVE:
	  spec.is_constant_spaced = 1;
	  spec.constant_space_width
	    = scale(constant_space*fs.to_scaled_points(),
		    units_per_inch,
		    36*72*sizescale);
	  break;
	default:
	  assert(0);
	}
	if (fontno != number)
	  return make_tfont(spec);
	last_tfont = make_tfont(spec);
	last_size = fs;
	last_height = height;
	last_slant = slant;
	last_ligature_mode = global_ligature_mode;
	last_kern_mode = global_kern_mode;
      }
  return last_tfont;
}

int font_info::get_bold(hunits *res)
{
  if (is_bold) {
    *res = bold_offset;
    return 1;
  }
  else
    return 0;
}

void font_info::unbold()
{
  if (is_bold) {
    is_bold = 0;
    flush();
  }
}

void font_info::set_bold(hunits offset)
{
  if (!is_bold || offset != bold_offset) {
    is_bold = 1;
    bold_offset = offset;
    flush();
  }
}

void font_info::set_conditional_bold(int fontno, hunits offset)
{
  for (conditional_bold *p = cond_bold_list; p; p = p->next)
    if (p->fontno == fontno) {
      if (offset != p->offset) {
	p->offset = offset;
	flush();
      }
      return;
    }
  cond_bold_list = new conditional_bold(fontno, offset, cond_bold_list);
}

conditional_bold::conditional_bold(int f, hunits h, conditional_bold *x)
: next(x), fontno(f), offset(h)
{
}

void font_info::conditional_unbold(int fontno)
{
  for (conditional_bold **p = &cond_bold_list; *p; p = &(*p)->next)
    if ((*p)->fontno == fontno) {
      conditional_bold *tem = *p;
      *p = (*p)->next;
      delete tem;
      flush();
      return;
    }
}

void font_info::set_constant_space(constant_space_type type, units x)
{
  if (type != is_constant_spaced
      || (type != CONSTANT_SPACE_NONE && x != constant_space)) {
    flush();
    is_constant_spaced = type;
    constant_space = x;
  }
}

void font_info::set_track_kern(track_kerning_function &tk)
{
  if (track_kern != tk) {
    track_kern = tk;
    flush();
  }
}

void font_info::flush()
{
  last_tfont = 0;
}

int font_info::is_named(symbol s)
{
  return internal_name == s;
}

symbol font_info::get_name()
{
  return internal_name;
}

symbol get_font_name(int fontno, environment *env)
{
  symbol f = font_table[fontno]->get_name();
  if (font_table[fontno]->is_style()) {
    return concat(env->get_family()->nm, f);
  }
  return f;
}

symbol get_style_name(int fontno)
{
  if (font_table[fontno]->is_style())
    return font_table[fontno]->get_name();
  else
    return EMPTY_SYMBOL;
}

hunits font_info::get_space_width(font_size fs, int space_sz)
{
  if (is_constant_spaced == CONSTANT_SPACE_NONE)
    return scale(hunits(fm->get_space_width(fs.to_scaled_points())),
			space_sz, 12);
  else if (is_constant_spaced == CONSTANT_SPACE_ABSOLUTE)
    return constant_space;
  else
    return scale(constant_space*fs.to_scaled_points(),
		 units_per_inch, 36*72*sizescale);
}

hunits font_info::get_narrow_space_width(font_size fs)
{
  charinfo *ci = get_charinfo(symbol("|"));
  if (fm->contains(ci->get_index()))
    return hunits(fm->get_width(ci->get_index(), fs.to_scaled_points()));
  else
    return hunits(fs.to_units()/6);
}

hunits font_info::get_half_narrow_space_width(font_size fs)
{
  charinfo *ci = get_charinfo(symbol("^"));
  if (fm->contains(ci->get_index()))
    return hunits(fm->get_width(ci->get_index(), fs.to_scaled_points()));
  else
    return hunits(fs.to_units()/12);
}

/* tfont */

tfont_spec::tfont_spec(symbol nm, int n, font *f,
		       font_size s, int h, int sl)
: name(nm), input_position(n), fm(f), size(s),
  is_bold(0), is_constant_spaced(0), ligature_mode(1), kern_mode(1),
  height(h), slant(sl)
{
  if (height == size.to_scaled_points())
    height = 0;
}

int tfont_spec::operator==(const tfont_spec &spec)
{
  if (fm == spec.fm
      && size == spec.size
      && input_position == spec.input_position
      && name == spec.name
      && height == spec.height
      && slant == spec.slant
      && (is_bold
	  ? (spec.is_bold && bold_offset == spec.bold_offset)
	  : !spec.is_bold)
      && track_kern == spec.track_kern
      && (is_constant_spaced
	  ? (spec.is_constant_spaced
	     && constant_space_width == spec.constant_space_width)
	  : !spec.is_constant_spaced)
      && ligature_mode == spec.ligature_mode
      && kern_mode == spec.kern_mode)
    return 1;
  else
    return 0;
}

tfont_spec tfont_spec::plain()
{
  return tfont_spec(name, input_position, fm, size, height, slant);
}

hunits tfont::get_width(charinfo *c)
{
  if (is_constant_spaced)
    return constant_space_width;
  else if (is_bold)
    return (hunits(fm->get_width(c->get_index(), size.to_scaled_points()))
	    + track_kern + bold_offset);
  else
    return (hunits(fm->get_width(c->get_index(), size.to_scaled_points()))
	    + track_kern);
}

vunits tfont::get_char_height(charinfo *c)
{
  vunits v = fm->get_height(c->get_index(), size.to_scaled_points());
  if (height != 0 && height != size.to_scaled_points())
    return scale(v, height, size.to_scaled_points());
  else
    return v;
}

vunits tfont::get_char_depth(charinfo *c)
{
  vunits v = fm->get_depth(c->get_index(), size.to_scaled_points());
  if (height != 0 && height != size.to_scaled_points())
    return scale(v, height, size.to_scaled_points());
  else
    return v;
}

hunits tfont::get_char_skew(charinfo *c)
{
  return hunits(fm->get_skew(c->get_index(), size.to_scaled_points(), slant));
}

hunits tfont::get_italic_correction(charinfo *c)
{
  return hunits(fm->get_italic_correction(c->get_index(), size.to_scaled_points()));
}

hunits tfont::get_left_italic_correction(charinfo *c)
{
  return hunits(fm->get_left_italic_correction(c->get_index(),
					       size.to_scaled_points()));
}

hunits tfont::get_subscript_correction(charinfo *c)
{
  return hunits(fm->get_subscript_correction(c->get_index(),
					     size.to_scaled_points()));
}

inline int tfont::get_input_position()
{
  return input_position;
}

inline int tfont::contains(charinfo *ci)
{
  return fm->contains(ci->get_index());
}

inline int tfont::get_character_type(charinfo *ci)
{
  return fm->get_character_type(ci->get_index());
}

inline int tfont::get_bold(hunits *res)
{
  if (is_bold) {
    *res = bold_offset;
    return 1;
  }
  else
    return 0;
}

inline int tfont::get_constant_space(hunits *res)
{
  if (is_constant_spaced) {
    *res = constant_space_width;
    return 1;
  }
  else
    return 0;
}

inline hunits tfont::get_track_kern()
{
  return track_kern;
}

inline tfont *tfont::get_plain()
{
  return plain_version;
}

inline font_size tfont::get_size()
{
  return size;
}

inline symbol tfont::get_name()
{
  return name;
}

inline int tfont::get_height()
{
  return height;
}

inline int tfont::get_slant()
{
  return slant;
}

symbol SYMBOL_ff("ff");
symbol SYMBOL_fi("fi");
symbol SYMBOL_fl("fl");
symbol SYMBOL_Fi("Fi");
symbol SYMBOL_Fl("Fl");

charinfo *tfont::get_lig(charinfo *c1, charinfo *c2)
{
  if (ligature_mode == 0)
    return 0;
  charinfo *ci = 0;
  if (c1->get_ascii_code() == 'f') {
    switch (c2->get_ascii_code()) {
    case 'f':
      if (fm->has_ligature(font::LIG_ff))
	ci = get_charinfo(SYMBOL_ff);
      break;
    case 'i':
      if (fm->has_ligature(font::LIG_fi))
	ci = get_charinfo(SYMBOL_fi);
      break;
    case 'l':
      if (fm->has_ligature(font::LIG_fl))
	ci = get_charinfo(SYMBOL_fl);
      break;
    }
  }
  else if (ligature_mode != 2 && c1->nm == SYMBOL_ff) {
    switch (c2->get_ascii_code()) {
    case 'i':
      if (fm->has_ligature(font::LIG_ffi))
	ci = get_charinfo(SYMBOL_Fi);
      break;
    case 'l':
      if (fm->has_ligature(font::LIG_ffl))
	ci = get_charinfo(SYMBOL_Fl);
      break;
    }
  }
  if (ci != 0 && fm->contains(ci->get_index()))
    return ci;
  return 0;
}

inline int tfont::get_kern(charinfo *c1, charinfo *c2, hunits *res)
{
  if (kern_mode == 0)
    return 0;
  else {
    int n = fm->get_kern(c1->get_index(),
			 c2->get_index(),
			 size.to_scaled_points());
    if (n) {
      *res = hunits(n);
      return 1;
    }
    else
      return 0;
  }
}

tfont *tfont::tfont_list = 0;

tfont::tfont(tfont_spec &spec) : tfont_spec(spec)
{
  next = tfont_list;
  tfont_list = this;
  tfont_spec plain_spec = plain();
  tfont *p;
  for (p = tfont_list; p; p = p->next)
    if (*p == plain_spec) {
      plain_version = p;
      break;
    }
  if (!p)
    plain_version = new tfont(plain_spec);
}

/* output_file */

class real_output_file : public output_file {
#ifndef POPEN_MISSING
  int piped;
#endif
  int printing;        // decision via optional page list
  int output_on;       // \O[0] or \O[1] escape calls
  virtual void really_transparent_char(unsigned char) = 0;
  virtual void really_print_line(hunits x, vunits y, node *n,
				 vunits before, vunits after, hunits width) = 0;
  virtual void really_begin_page(int pageno, vunits page_length) = 0;
  virtual void really_copy_file(hunits x, vunits y, const char *filename);
  virtual void really_put_filename(const char *filename);
  virtual void really_on();
  virtual void really_off();
public:
  FILE *fp;
  real_output_file();
  ~real_output_file();
  void flush();
  void transparent_char(unsigned char);
  void print_line(hunits x, vunits y, node *n, vunits before, vunits after, hunits width);
  void begin_page(int pageno, vunits page_length);
  void put_filename(const char *filename);
  void on();
  void off();
  int is_on();
  int is_printing();
  void copy_file(hunits x, vunits y, const char *filename);
};

class suppress_output_file : public real_output_file {
public:
  suppress_output_file();
  void really_transparent_char(unsigned char);
  void really_print_line(hunits x, vunits y, node *n, vunits, vunits, hunits width);
  void really_begin_page(int pageno, vunits page_length);
};

class ascii_output_file : public real_output_file {
public:
  ascii_output_file();
  void really_transparent_char(unsigned char);
  void really_print_line(hunits x, vunits y, node *n, vunits, vunits, hunits width);
  void really_begin_page(int pageno, vunits page_length);
  void outc(unsigned char c);
  void outs(const char *s);
};

void ascii_output_file::outc(unsigned char c)
{
  fputc(c, fp);
}

void ascii_output_file::outs(const char *s)
{
  fputc('<', fp);
  if (s)
    fputs(s, fp);
  fputc('>', fp);
}

struct hvpair;

class troff_output_file : public real_output_file {
  units hpos;
  units vpos;
  units output_vpos;
  units output_hpos;
  int force_motion;
  int current_size;
  int current_slant;
  int current_height;
  tfont *current_tfont;
  color *current_fill_color;
  color *current_glyph_color;
  int current_font_number;
  symbol *font_position;
  int nfont_positions;
  enum { TBUF_SIZE = 256 };
  char tbuf[TBUF_SIZE];
  int tbuf_len;
  int tbuf_kern;
  int begun_page;
  int cur_div_level;
  string tag_list;
  void do_motion();
  void put(char c);
  void put(unsigned char c);
  void put(int i);
  void put(unsigned int i);
  void put(const char *s);
  void set_font(tfont *tf);
  void flush_tbuf();
public:
  troff_output_file();
  ~troff_output_file();
  void trailer(vunits page_length);
  void put_char(charinfo *, tfont *, color *, color *);
  void put_char_width(charinfo *, tfont *, color *, color *, hunits, hunits);
  void right(hunits);
  void down(vunits);
  void moveto(hunits, vunits);
  void start_special(tfont *, color *, color *, int = 0);
  void start_special();
  void special_char(unsigned char c);
  void end_special();
  void word_marker();
  void really_transparent_char(unsigned char c);
  void really_print_line(hunits x, vunits y, node *n, vunits before, vunits after, hunits width);
  void really_begin_page(int pageno, vunits page_length);
  void really_copy_file(hunits x, vunits y, const char *filename);
  void really_put_filename(const char *filename);
  void really_on();
  void really_off();
  void draw(char, hvpair *, int, font_size, color *, color *);
  void determine_line_limits (char code, hvpair *point, int npoints);
  void check_charinfo(tfont *tf, charinfo *ci);
  void glyph_color(color *c);
  void fill_color(color *c);
  int get_hpos() { return hpos; }
  int get_vpos() { return vpos; }
  void add_to_tag_list(string s);
  friend void space_char_hmotion_node::tprint(troff_output_file *);
  friend void unbreakable_space_node::tprint(troff_output_file *);
};

static void put_string(const char *s, FILE *fp)
{
  for (; *s != '\0'; ++s)
    putc(*s, fp);
}

inline void troff_output_file::put(char c)
{
  putc(c, fp);
}

inline void troff_output_file::put(unsigned char c)
{
  putc(c, fp);
}

inline void troff_output_file::put(const char *s)
{
  put_string(s, fp);
}

inline void troff_output_file::put(int i)
{
  put_string(i_to_a(i), fp);
}

inline void troff_output_file::put(unsigned int i)
{
  put_string(ui_to_a(i), fp);
}

void troff_output_file::start_special(tfont *tf, color *gcol, color *fcol,
				      int no_init_string)
{
  set_font(tf);
  glyph_color(gcol);
  fill_color(fcol);
  flush_tbuf();
  do_motion();
  if (!no_init_string)
    put("x X ");
}

void troff_output_file::start_special()
{
  flush_tbuf();
  do_motion();
  put("x X ");
}

void troff_output_file::special_char(unsigned char c)
{
  put(c);
  if (c == '\n')
    put('+');
}

void troff_output_file::end_special()
{
  put('\n');
}

inline void troff_output_file::moveto(hunits h, vunits v)
{
  hpos = h.to_units();
  vpos = v.to_units();
}

void troff_output_file::really_print_line(hunits x, vunits y, node *n,
					  vunits before, vunits after, hunits)
{
  moveto(x, y);
  while (n != 0) {
    // Check whether we should push the current troff state and use
    // the state at the start of the invocation of this diversion.
    if (n->div_nest_level > cur_div_level && n->push_state) {
      state.push_state(n->push_state);
      cur_div_level = n->div_nest_level;
    }
    // Has the current diversion level decreased?  Then we must pop the
    // troff state.
    while (n->div_nest_level < cur_div_level) {
      state.pop_state();
      cur_div_level = n->div_nest_level;
    }
    // Now check whether the state has changed.
    if ((is_on() || n->force_tprint())
	&& (state.changed(n->state) || n->is_tag() || n->is_special)) {
      flush_tbuf();
      do_motion();
      force_motion = 1;
      flush();
      state.flush(fp, n->state, tag_list);
      tag_list = string("");
      flush();
    }
    n->tprint(this);
    n = n->next;
  }
  flush_tbuf();
  // This ensures that transparent throughput will have a more predictable
  // position.
  do_motion();
  force_motion = 1;
  hpos = 0;
  put('n');
  put(before.to_units());
  put(' ');
  put(after.to_units());
  put('\n');
}

inline void troff_output_file::word_marker()
{
  flush_tbuf();
  if (is_on())
    put('w');
}

inline void troff_output_file::right(hunits n)
{
  hpos += n.to_units();
}

inline void troff_output_file::down(vunits n)
{
  vpos += n.to_units();
}

void troff_output_file::do_motion()
{
  if (force_motion) {
    put('V');
    put(vpos);
    put('\n');
    put('H');
    put(hpos);
    put('\n');
  }
  else {
    if (hpos != output_hpos) {
      units n = hpos - output_hpos;
      if (n > 0 && n < hpos) {
	put('h');
	put(n);
      }
      else {
	put('H');
	put(hpos);
      }
      put('\n');
    }
    if (vpos != output_vpos) {
      units n = vpos - output_vpos;
      if (n > 0 && n < vpos) {
	put('v');
	put(n);
      }
      else {
	put('V');
	put(vpos);
      }
      put('\n');
    }
  }
  output_vpos = vpos;
  output_hpos = hpos;
  force_motion = 0;
}

void troff_output_file::flush_tbuf()
{
  if (!is_on()) {
    tbuf_len = 0;
    return;
  }

  if (tbuf_len == 0)
    return;
  if (tbuf_kern == 0)
    put('t');
  else {
    put('u');
    put(tbuf_kern);
    put(' ');
  }
  check_output_limits(hpos, vpos);
  check_output_limits(hpos, vpos - current_size);

  for (int i = 0; i < tbuf_len; i++)
    put(tbuf[i]);
  put('\n');
  tbuf_len = 0;
}

void troff_output_file::check_charinfo(tfont *tf, charinfo *ci)
{
  if (!is_on())
    return;

  int height = tf->get_char_height(ci).to_units();
  int width = tf->get_width(ci).to_units()
	      + tf->get_italic_correction(ci).to_units();
  int depth = tf->get_char_depth(ci).to_units();
  check_output_limits(output_hpos, output_vpos - height);
  check_output_limits(output_hpos + width, output_vpos + depth);
}

void troff_output_file::put_char_width(charinfo *ci, tfont *tf,
				       color *gcol, color *fcol,
				       hunits w, hunits k)
{
  int kk = k.to_units();
  if (!is_on()) {
    flush_tbuf();
    hpos += w.to_units() + kk;
    return;
  }
  set_font(tf);
  unsigned char c = ci->get_ascii_code();
  if (c == '\0') {
    glyph_color(gcol);
    fill_color(fcol);
    flush_tbuf();
    do_motion();
    check_charinfo(tf, ci);
    if (ci->numbered()) {
      put('N');
      put(ci->get_number());
    }
    else {
      put('C');
      const char *s = ci->nm.contents();
      if (s[1] == 0) {
	put('\\');
	put(s[0]);
      }
      else
	put(s);
    }
    put('\n');
    hpos += w.to_units() + kk;
  }
  else if (tcommand_flag) {
    if (tbuf_len > 0 && hpos == output_hpos && vpos == output_vpos
	&& (!gcol || gcol == current_glyph_color)
	&& (!fcol || fcol == current_fill_color)
	&& kk == tbuf_kern
	&& tbuf_len < TBUF_SIZE) {
      check_charinfo(tf, ci);
      tbuf[tbuf_len++] = c;
      output_hpos += w.to_units() + kk;
      hpos = output_hpos;
      return;
    }
    glyph_color(gcol);
    fill_color(fcol);
    flush_tbuf();
    do_motion();
    check_charinfo(tf, ci);
    tbuf[tbuf_len++] = c;
    output_hpos += w.to_units() + kk;
    tbuf_kern = kk;
    hpos = output_hpos;
  }
  else {
    // flush_tbuf();
    int n = hpos - output_hpos;
    check_charinfo(tf, ci);
    // check_output_limits(output_hpos, output_vpos);
    if (vpos == output_vpos
	&& (!gcol || gcol == current_glyph_color)
	&& (!fcol || fcol == current_fill_color)
	&& n > 0 && n < 100 && !force_motion) {
      put(char(n/10 + '0'));
      put(char(n%10 + '0'));
      put(c);
      output_hpos = hpos;
    }
    else {
      glyph_color(gcol);
      fill_color(fcol);
      do_motion();
      put('c');
      put(c);
    }
    hpos += w.to_units() + kk;
  }
}

void troff_output_file::put_char(charinfo *ci, tfont *tf,
				 color *gcol, color *fcol)
{
  flush_tbuf();
  if (!is_on())
    return;
  set_font(tf);
  unsigned char c = ci->get_ascii_code();
  if (c == '\0') {
    glyph_color(gcol);
    fill_color(fcol);
    flush_tbuf();
    do_motion();
    if (ci->numbered()) {
      put('N');
      put(ci->get_number());
    }
    else {
      put('C');
      const char *s = ci->nm.contents();
      if (s[1] == 0) {
	put('\\');
	put(s[0]);
      }
      else
	put(s);
    }
    put('\n');
  }
  else {
    int n = hpos - output_hpos;
    if (vpos == output_vpos
	&& (!gcol || gcol == current_glyph_color)
	&& (!fcol || fcol == current_fill_color)
	&& n > 0 && n < 100) {
      put(char(n/10 + '0'));
      put(char(n%10 + '0'));
      put(c);
      output_hpos = hpos;
    }
    else {
      glyph_color(gcol);
      fill_color(fcol);
      flush_tbuf();
      do_motion();
      put('c');
      put(c);
    }
  }
}

// set_font calls `flush_tbuf' if necessary.

void troff_output_file::set_font(tfont *tf)
{
  if (current_tfont == tf)
    return;
  flush_tbuf();
  int n = tf->get_input_position();
  symbol nm = tf->get_name();
  if (n >= nfont_positions || font_position[n] != nm) {
    put("x font ");
    put(n);
    put(' ');
    put(nm.contents());
    put('\n');
    if (n >= nfont_positions) {
      int old_nfont_positions = nfont_positions;
      symbol *old_font_position = font_position;
      nfont_positions *= 3;
      nfont_positions /= 2;
      if (nfont_positions <= n)
	nfont_positions = n + 10;
      font_position = new symbol[nfont_positions];
      memcpy(font_position, old_font_position,
	     old_nfont_positions*sizeof(symbol));
      a_delete old_font_position;
    }
    font_position[n] = nm;
  }
  if (current_font_number != n) {
    put('f');
    put(n);
    put('\n');
    current_font_number = n;
  }
  int size = tf->get_size().to_scaled_points();
  if (current_size != size) {
    put('s');
    put(size);
    put('\n');
    current_size = size;
  }
  int slant = tf->get_slant();
  if (current_slant != slant) {
    put("x Slant ");
    put(slant);
    put('\n');
    current_slant = slant;
  }
  int height = tf->get_height();
  if (current_height != height) {
    put("x Height ");
    put(height == 0 ? current_size : height);
    put('\n');
    current_height = height;
  }
  current_tfont = tf;
}

// fill_color calls `flush_tbuf' and `do_motion' if necessary.

void troff_output_file::fill_color(color *col)
{
  if (!col || current_fill_color == col)
    return;
  current_fill_color = col;
  if (!color_flag)
    return;
  flush_tbuf();
  do_motion();
  put("DF");
  unsigned int components[4];
  color_scheme cs;
  cs = col->get_components(components);
  switch (cs) {
  case DEFAULT:
    put('d');
    break;
  case RGB:
    put("r ");
    put(Red);
    put(' ');
    put(Green);
    put(' ');
    put(Blue);
    break;
  case CMY:
    put("c ");
    put(Cyan);
    put(' ');
    put(Magenta);
    put(' ');
    put(Yellow);
    break;
  case CMYK:
    put("k ");
    put(Cyan);
    put(' ');
    put(Magenta);
    put(' ');
    put(Yellow);
    put(' ');
    put(Black);
    break;
  case GRAY:
    put("g ");
    put(Gray);
    break;
  }
  put('\n');
}

// glyph_color calls `flush_tbuf' and `do_motion' if necessary.

void troff_output_file::glyph_color(color *col)
{
  if (!col || current_glyph_color == col)
    return;
  current_glyph_color = col;
  if (!color_flag)
    return;
  flush_tbuf();
  // grotty doesn't like a color command if the vertical position is zero.
  do_motion();
  put("m");
  unsigned int components[4];
  color_scheme cs;
  cs = col->get_components(components);
  switch (cs) {
  case DEFAULT:
    put('d');
    break;
  case RGB:
    put("r ");
    put(Red);
    put(' ');
    put(Green);
    put(' ');
    put(Blue);
    break;
  case CMY:
    put("c ");
    put(Cyan);
    put(' ');
    put(Magenta);
    put(' ');
    put(Yellow);
    break;
  case CMYK:
    put("k ");
    put(Cyan);
    put(' ');
    put(Magenta);
    put(' ');
    put(Yellow);
    put(' ');
    put(Black);
    break;
  case GRAY:
    put("g ");
    put(Gray);
    break;
  }
  put('\n');
}

void troff_output_file::add_to_tag_list(string s)
{
  if (tag_list == string(""))
    tag_list = s;
  else {
    tag_list += string("\n");
    tag_list += s;
  }
}

// determine_line_limits - works out the smallest box which will contain
//			   the entity, code, built from the point array.
void troff_output_file::determine_line_limits(char code, hvpair *point,
					      int npoints)
{
  int i, x, y;

  if (!is_on())
    return;

  switch (code) {
  case 'c':
  case 'C':
    // only the h field is used when defining a circle
    check_output_limits(output_hpos,
			output_vpos - point[0].h.to_units()/2);
    check_output_limits(output_hpos + point[0].h.to_units(),
			output_vpos + point[0].h.to_units()/2);
    break;
  case 'E':
  case 'e':
    check_output_limits(output_hpos,
			output_vpos - point[0].v.to_units()/2);
    check_output_limits(output_hpos + point[0].h.to_units(),
			output_vpos + point[0].v.to_units()/2);
    break;
  case 'P':
  case 'p':
    x = output_hpos;
    y = output_vpos;
    check_output_limits(x, y);
    for (i = 0; i < npoints; i++) {
      x += point[i].h.to_units();
      y += point[i].v.to_units();
      check_output_limits(x, y);
    }
    break;
  case 't':
    x = output_hpos;
    y = output_vpos;
    for (i = 0; i < npoints; i++) {
      x += point[i].h.to_units();
      y += point[i].v.to_units();
      check_output_limits(x, y);
    }
    break;
  case 'a':
    double c[2];
    int p[4];
    int minx, miny, maxx, maxy;
    x = output_hpos;
    y = output_vpos;
    p[0] = point[0].h.to_units();
    p[1] = point[0].v.to_units();
    p[2] = point[1].h.to_units();
    p[3] = point[1].v.to_units();
    if (adjust_arc_center(p, c)) {
      check_output_arc_limits(x, y,
			      p[0], p[1], p[2], p[3],
			      c[0], c[1],
			      &minx, &maxx, &miny, &maxy);
      check_output_limits(minx, miny);
      check_output_limits(maxx, maxy);
      break;
    }
    // fall through
  case 'l':
    x = output_hpos;
    y = output_vpos;
    check_output_limits(x, y);
    for (i = 0; i < npoints; i++) {
      x += point[i].h.to_units();
      y += point[i].v.to_units();
      check_output_limits(x, y);
    }
    break;
  default:
    x = output_hpos;
    y = output_vpos;
    for (i = 0; i < npoints; i++) {
      x += point[i].h.to_units();
      y += point[i].v.to_units();
      check_output_limits(x, y);
    }
  }
}

void troff_output_file::draw(char code, hvpair *point, int npoints,
			     font_size fsize, color *gcol, color *fcol)
{
  int i;
  glyph_color(gcol);
  fill_color(fcol);
  flush_tbuf();
  do_motion();
  if (is_on()) {
    int size = fsize.to_scaled_points();
    if (current_size != size) {
      put('s');
      put(size);
      put('\n');
      current_size = size;
      current_tfont = 0;
    }
    put('D');
    put(code);
    if (code == 'c') {
      put(' ');
      put(point[0].h.to_units());
    }
    else
      for (i = 0; i < npoints; i++) {
	put(' ');
	put(point[i].h.to_units());
	put(' ');
	put(point[i].v.to_units());
      }
    determine_line_limits(code, point, npoints);
  }

  for (i = 0; i < npoints; i++)
    output_hpos += point[i].h.to_units();
  hpos = output_hpos;
  if (code != 'e') {
    for (i = 0; i < npoints; i++)
      output_vpos += point[i].v.to_units();
    vpos = output_vpos;
  }
  if (is_on())
    put('\n');
}

void troff_output_file::really_on()
{
  flush_tbuf();
  force_motion = 1;
  do_motion();
}

void troff_output_file::really_off()
{
  flush_tbuf();
}

void troff_output_file::really_put_filename(const char *filename)
{
  flush_tbuf();
  put("F ");
  put(filename);
  put('\n');
}

void troff_output_file::really_begin_page(int pageno, vunits page_length)
{
  flush_tbuf();
  if (begun_page) {
    if (page_length > V0) {
      put('V');
      put(page_length.to_units());
      put('\n');
    }
  }
  else
    begun_page = 1;
  current_tfont = 0;
  current_font_number = -1;
  current_size = 0;
  // current_height = 0;
  // current_slant = 0;
  hpos = 0;
  vpos = 0;
  output_hpos = 0;
  output_vpos = 0;
  force_motion = 1;
  for (int i = 0; i < nfont_positions; i++)
    font_position[i] = NULL_SYMBOL;
  put('p');
  put(pageno);
  put('\n');
}

void troff_output_file::really_copy_file(hunits x, vunits y,
					 const char *filename)
{
  moveto(x, y);
  flush_tbuf();
  do_motion();
  errno = 0;
  FILE *ifp = include_search_path.open_file_cautious(filename);
  if (ifp == 0)
    error("can't open `%1': %2", filename, strerror(errno));
  else {
    int c;
    while ((c = getc(ifp)) != EOF)
      put(char(c));
    fclose(ifp);
  }
  force_motion = 1;
  current_size = 0;
  current_tfont = 0;
  current_font_number = -1;
  for (int i = 0; i < nfont_positions; i++)
    font_position[i] = NULL_SYMBOL;
}

void troff_output_file::really_transparent_char(unsigned char c)
{
  put(c);
}

troff_output_file::~troff_output_file()
{
  a_delete font_position;
}

void troff_output_file::trailer(vunits page_length)
{
  flush_tbuf();
  if (page_length > V0) {
    put("x trailer\n");
    put('V');
    put(page_length.to_units());
    put('\n');
  }
  put("x stop\n");
}

troff_output_file::troff_output_file()
: current_slant(0), current_height(0), current_fill_color(0),
  current_glyph_color(0), nfont_positions(10), tbuf_len(0), begun_page(0),
  cur_div_level(0)
{
  font_position = new symbol[nfont_positions];
  put("x T ");
  put(device);
  put('\n');
  put("x res ");
  put(units_per_inch);
  put(' ');
  put(hresolution);
  put(' ');
  put(vresolution);
  put('\n');
  put("x init\n");
}

/* output_file */

output_file *the_output = 0;

output_file::output_file()
{
}

output_file::~output_file()
{
}

void output_file::trailer(vunits)
{
}

void output_file::put_filename(const char *)
{
}

void output_file::on()
{
}

void output_file::off()
{
}

real_output_file::real_output_file()
: printing(0), output_on(1)
{
#ifndef POPEN_MISSING
  if (pipe_command) {
    if ((fp = popen(pipe_command, POPEN_WT)) != 0) {
      piped = 1;
      return;
    }
    error("pipe open failed: %1", strerror(errno));
  }
  piped = 0;
#endif /* not POPEN_MISSING */
  fp = stdout;
}

real_output_file::~real_output_file()
{
  if (!fp)
    return;
  // To avoid looping, set fp to 0 before calling fatal().
  if (ferror(fp) || fflush(fp) < 0) {
    fp = 0;
    fatal("error writing output file");
  }
#ifndef POPEN_MISSING
  if (piped) {
    int result = pclose(fp);
    fp = 0;
    if (result < 0)
      fatal("pclose failed");
    if (!WIFEXITED(result))
      error("output process `%1' got fatal signal %2",
	    pipe_command,
	    WIFSIGNALED(result) ? WTERMSIG(result) : WSTOPSIG(result));
    else {
      int exit_status = WEXITSTATUS(result);
      if (exit_status != 0)
	error("output process `%1' exited with status %2",
	      pipe_command, exit_status);
    }
  }
  else
#endif /* not POPEN MISSING */
  if (fclose(fp) < 0) {
    fp = 0;
    fatal("error closing output file");
  }
}

void real_output_file::flush()
{
  if (fflush(fp) < 0)
    fatal("error writing output file");
}

int real_output_file::is_printing()
{
  return printing;
}

void real_output_file::begin_page(int pageno, vunits page_length)
{
  printing = in_output_page_list(pageno);
  if (printing)
    really_begin_page(pageno, page_length);
}

void real_output_file::copy_file(hunits x, vunits y, const char *filename)
{
  if (printing && output_on)
    really_copy_file(x, y, filename);
  check_output_limits(x.to_units(), y.to_units());
}

void real_output_file::transparent_char(unsigned char c)
{
  if (printing && output_on)
    really_transparent_char(c);
}

void real_output_file::print_line(hunits x, vunits y, node *n,
			     vunits before, vunits after, hunits width)
{
  if (printing)
    really_print_line(x, y, n, before, after, width);
  delete_node_list(n);
}

void real_output_file::really_copy_file(hunits, vunits, const char *)
{
  // do nothing
}

void real_output_file::put_filename(const char *filename)
{
  really_put_filename(filename);
}

void real_output_file::really_put_filename(const char *)
{
}

void real_output_file::on()
{
  really_on();
  if (output_on == 0)
    output_on = 1;
}

void real_output_file::off()
{
  really_off();
  output_on = 0;
}

int real_output_file::is_on()
{
  return output_on;
}

void real_output_file::really_on()
{
}

void real_output_file::really_off()
{
}

/* ascii_output_file */

void ascii_output_file::really_transparent_char(unsigned char c)
{
  putc(c, fp);
}

void ascii_output_file::really_print_line(hunits, vunits, node *n,
					  vunits, vunits, hunits)
{
  while (n != 0) {
    n->ascii_print(this);
    n = n->next;
  }
  fputc('\n', fp);
}

void ascii_output_file::really_begin_page(int /*pageno*/, vunits /*page_length*/)
{
  fputs("<beginning of page>\n", fp);
}

ascii_output_file::ascii_output_file()
{
}

/* suppress_output_file */

suppress_output_file::suppress_output_file()
{
}

void suppress_output_file::really_print_line(hunits, vunits, node *, vunits, vunits, hunits)
{
}

void suppress_output_file::really_begin_page(int, vunits)
{
}

void suppress_output_file::really_transparent_char(unsigned char)
{
}

/* glyphs, ligatures, kerns, discretionary breaks */

class charinfo_node : public node {
protected:
  charinfo *ci;
public:
  charinfo_node(charinfo *, statem *, int, node * = 0);
  int ends_sentence();
  int overlaps_vertically();
  int overlaps_horizontally();
};

charinfo_node::charinfo_node(charinfo *c, statem *s, int pop, node *x)
: node(x, s, pop), ci(c)
{
}

int charinfo_node::ends_sentence()
{
  if (ci->ends_sentence())
    return 1;
  else if (ci->transparent())
    return 2;
  else
    return 0;
}

int charinfo_node::overlaps_horizontally()
{
  return ci->overlaps_horizontally();
}

int charinfo_node::overlaps_vertically()
{
  return ci->overlaps_vertically();
}

class glyph_node : public charinfo_node {
  static glyph_node *free_list;
protected:
  tfont *tf;
  color *gcol;
  color *fcol;		/* this is needed for grotty */
#ifdef STORE_WIDTH
  hunits wid;
  glyph_node(charinfo *, tfont *, color *, color *, hunits,
	     statem *, int, node * = 0);
#endif
public:
  void *operator new(size_t);
  void operator delete(void *);
  glyph_node(charinfo *, tfont *, color *, color *,
	     statem *, int, node * = 0);
  ~glyph_node() {}
  node *copy();
  node *merge_glyph_node(glyph_node *);
  node *merge_self(node *);
  hunits width();
  node *last_char_node();
  units size();
  void vertical_extent(vunits *, vunits *);
  hunits subscript_correction();
  hunits italic_correction();
  hunits left_italic_correction();
  hunits skew();
  hyphenation_type get_hyphenation_type();
  tfont *get_tfont();
  color *get_glyph_color();
  color *get_fill_color();
  void tprint(troff_output_file *);
  void zero_width_tprint(troff_output_file *);
  hyphen_list *get_hyphen_list(hyphen_list *, int *);
  node *add_self(node *, hyphen_list **);
  void ascii_print(ascii_output_file *);
  void asciify(macro *);
  int character_type();
  int same(node *);
  const char *type();
  int force_tprint();
  int is_tag();
  void debug_node();
};

glyph_node *glyph_node::free_list = 0;

class ligature_node : public glyph_node {
  node *n1;
  node *n2;
#ifdef STORE_WIDTH
  ligature_node(charinfo *, tfont *, color *, color *, hunits,
		node *, node *, statem *, int, node * = 0);
#endif
public:
  void *operator new(size_t);
  void operator delete(void *);
  ligature_node(charinfo *, tfont *, color *, color *,
		node *, node *, statem *, int, node * = 0);
  ~ligature_node();
  node *copy();
  node *add_self(node *, hyphen_list **);
  hyphen_list *get_hyphen_list(hyphen_list *, int *);
  void ascii_print(ascii_output_file *);
  void asciify(macro *);
  int same(node *);
  const char *type();
  int force_tprint();
  int is_tag();
};

class kern_pair_node : public node {
  hunits amount;
  node *n1;
  node *n2;
public:
  kern_pair_node(hunits, node *, node *, statem *, int, node * = 0);
  ~kern_pair_node();
  node *copy();
  node *merge_glyph_node(glyph_node *);
  node *add_self(node *, hyphen_list **);
  hyphen_list *get_hyphen_list(hyphen_list *, int *);
  node *add_discretionary_hyphen();
  hunits width();
  node *last_char_node();
  hunits italic_correction();
  hunits subscript_correction();
  void tprint(troff_output_file *);
  hyphenation_type get_hyphenation_type();
  int ends_sentence();
  void ascii_print(ascii_output_file *);
  void asciify(macro *);
  int same(node *);
  const char *type();
  int force_tprint();
  int is_tag();
  void vertical_extent(vunits *, vunits *);
};

class dbreak_node : public node {
  node *none;
  node *pre;
  node *post;
public:
  dbreak_node(node *, node *, statem *, int, node * = 0);
  ~dbreak_node();
  node *copy();
  node *merge_glyph_node(glyph_node *);
  node *add_discretionary_hyphen();
  hunits width();
  node *last_char_node();
  hunits italic_correction();
  hunits subscript_correction();
  void tprint(troff_output_file *);
  breakpoint *get_breakpoints(hunits width, int ns, breakpoint *rest = 0,
			      int is_inner = 0);
  int nbreaks();
  int ends_sentence();
  void split(int, node **, node **);
  hyphenation_type get_hyphenation_type();
  void ascii_print(ascii_output_file *);
  void asciify(macro *);
  int same(node *);
  const char *type();
  int force_tprint();
  int is_tag();
};

void *glyph_node::operator new(size_t n)
{
  assert(n == sizeof(glyph_node));
  if (!free_list) {
    const int BLOCK = 1024;
    free_list = (glyph_node *)new char[sizeof(glyph_node)*BLOCK];
    for (int i = 0; i < BLOCK - 1; i++)
      free_list[i].next = free_list + i + 1;
    free_list[BLOCK-1].next = 0;
  }
  glyph_node *p = free_list;
  free_list = (glyph_node *)(free_list->next);
  p->next = 0;
  return p;
}

void *ligature_node::operator new(size_t n)
{
  return new char[n];
}

void glyph_node::operator delete(void *p)
{
  if (p) {
    ((glyph_node *)p)->next = free_list;
    free_list = (glyph_node *)p;
  }
}

void ligature_node::operator delete(void *p)
{
  delete[] (char *)p;
}

glyph_node::glyph_node(charinfo *c, tfont *t, color *gc, color *fc,
		       statem *s, int pop, node *x)
: charinfo_node(c, s, pop, x), tf(t), gcol(gc), fcol(fc)
{
#ifdef STORE_WIDTH
  wid = tf->get_width(ci);
#endif
}

#ifdef STORE_WIDTH
glyph_node::glyph_node(charinfo *c, tfont *t,
		       color *gc, color *fc, hunits w,
		       statem *s, int pop, node *x)
: charinfo_node(c, s, pop, x), tf(t), gcol(gc), fcol(fc), wid(w)
{
}
#endif

node *glyph_node::copy()
{
#ifdef STORE_WIDTH
  return new glyph_node(ci, tf, gcol, fcol, wid, state, div_nest_level);
#else
  return new glyph_node(ci, tf, gcol, fcol, state, div_nest_level);
#endif
}

node *glyph_node::merge_self(node *nd)
{
  return nd->merge_glyph_node(this);
}

int glyph_node::character_type()
{
  return tf->get_character_type(ci);
}

node *glyph_node::add_self(node *n, hyphen_list **p)
{
  assert(ci->get_hyphenation_code() == (*p)->hyphenation_code);
  next = 0;
  node *nn;
  if (n == 0 || (nn = n->merge_glyph_node(this)) == 0) {
    next = n;
    nn = this;
  }
  if ((*p)->hyphen)
    nn = nn->add_discretionary_hyphen();
  hyphen_list *pp = *p;
  *p = (*p)->next;
  delete pp;
  return nn;
}

units glyph_node::size()
{
  return tf->get_size().to_units();
}

hyphen_list *glyph_node::get_hyphen_list(hyphen_list *tail, int *count)
{
  (*count)++;
  return new hyphen_list(ci->get_hyphenation_code(), tail);
}

tfont *node::get_tfont()
{
  return 0;
}

tfont *glyph_node::get_tfont()
{
  return tf;
}

color *node::get_glyph_color()
{
  return 0;
}

color *glyph_node::get_glyph_color()
{
  return gcol;
}

color *node::get_fill_color()
{
  return 0;
}

color *glyph_node::get_fill_color()
{
  return fcol;
}

node *node::merge_glyph_node(glyph_node *)
{
  return 0;
}

node *glyph_node::merge_glyph_node(glyph_node *gn)
{
  if (tf == gn->tf && gcol == gn->gcol && fcol == gn->fcol) {
    charinfo *lig;
    if ((lig = tf->get_lig(ci, gn->ci)) != 0) {
      node *next1 = next;
      next = 0;
      return new ligature_node(lig, tf, gcol, fcol, this, gn, state,
			       gn->div_nest_level, next1);
    }
    hunits kern;
    if (tf->get_kern(ci, gn->ci, &kern)) {
      node *next1 = next;
      next = 0;
      return new kern_pair_node(kern, this, gn, state,
				gn->div_nest_level, next1);
    }
  }
  return 0;
}

#ifdef STORE_WIDTH
inline
#endif
hunits glyph_node::width()
{
#ifdef STORE_WIDTH
  return wid;
#else
  return tf->get_width(ci);
#endif
}

node *glyph_node::last_char_node()
{
  return this;
}

void glyph_node::vertical_extent(vunits *min, vunits *max)
{
  *min = -tf->get_char_height(ci);
  *max = tf->get_char_depth(ci);
}

hunits glyph_node::skew()
{
  return tf->get_char_skew(ci);
}

hunits glyph_node::subscript_correction()
{
  return tf->get_subscript_correction(ci);
}

hunits glyph_node::italic_correction()
{
  return tf->get_italic_correction(ci);
}

hunits glyph_node::left_italic_correction()
{
  return tf->get_left_italic_correction(ci);
}

hyphenation_type glyph_node::get_hyphenation_type()
{
  return HYPHEN_MIDDLE;
}

void glyph_node::ascii_print(ascii_output_file *ascii)
{
  unsigned char c = ci->get_ascii_code();
  if (c != 0)
    ascii->outc(c);
  else
    ascii->outs(ci->nm.contents());
}

void glyph_node::debug_node()
{
  unsigned char c = ci->get_ascii_code();
  fprintf(stderr, "{ %s [", type());
  if (c)
    fprintf(stderr, "%c", c);
  else
    fputs(ci->nm.contents(), stderr);
  if (push_state)
    fprintf(stderr, " <push_state>");
  if (state)
    state->display_state();
  fprintf(stderr, " nest level %d", div_nest_level);
  fprintf(stderr, "]}\n");
  fflush(stderr);
}

ligature_node::ligature_node(charinfo *c, tfont *t, color *gc, color *fc,
			     node *gn1, node *gn2, statem *s,
			     int pop, node *x)
: glyph_node(c, t, gc, fc, s, pop, x), n1(gn1), n2(gn2)
{
}

#ifdef STORE_WIDTH
ligature_node::ligature_node(charinfo *c, tfont *t, color *gc, color *fc,
			     hunits w, node *gn1, node *gn2, statem *s,
			     int pop, node *x)
: glyph_node(c, t, gc, fc, w, s, pop, x), n1(gn1), n2(gn2)
{
}
#endif

ligature_node::~ligature_node()
{
  delete n1;
  delete n2;
}

node *ligature_node::copy()
{
#ifdef STORE_WIDTH
  return new ligature_node(ci, tf, gcol, fcol, wid, n1->copy(), n2->copy(),
			   state, div_nest_level);
#else
  return new ligature_node(ci, tf, gcol, fcol, n1->copy(), n2->copy(),
			   state, div_nest_level);
#endif
}

void ligature_node::ascii_print(ascii_output_file *ascii)
{
  n1->ascii_print(ascii);
  n2->ascii_print(ascii);
}

hyphen_list *ligature_node::get_hyphen_list(hyphen_list *tail, int *count)
{
  hyphen_list *hl = n2->get_hyphen_list(tail, count);
  return n1->get_hyphen_list(hl, count);
}

node *ligature_node::add_self(node *n, hyphen_list **p)
{
  n = n1->add_self(n, p);
  n = n2->add_self(n, p);
  n1 = n2 = 0;
  delete this;
  return n;
}

kern_pair_node::kern_pair_node(hunits n, node *first, node *second,
			       statem* s, int pop, node *x)
: node(x, s, pop), amount(n), n1(first), n2(second)
{
}

dbreak_node::dbreak_node(node *n, node *p, statem *s, int pop, node *x)
: node(x, s, pop), none(n), pre(p), post(0)
{
}

node *dbreak_node::merge_glyph_node(glyph_node *gn)
{
  glyph_node *gn2 = (glyph_node *)gn->copy();
  node *new_none = none ? none->merge_glyph_node(gn) : 0;
  node *new_post = post ? post->merge_glyph_node(gn2) : 0;
  if (new_none == 0 && new_post == 0) {
    delete gn2;
    return 0;
  }
  if (new_none != 0)
    none = new_none;
  else {
    gn->next = none;
    none = gn;
  }
  if (new_post != 0)
    post = new_post;
  else {
    gn2->next = post;
    post = gn2;
  }
  return this;
}

node *kern_pair_node::merge_glyph_node(glyph_node *gn)
{
  node *nd = n2->merge_glyph_node(gn);
  if (nd == 0)
    return 0;
  n2 = nd;
  nd = n2->merge_self(n1);
  if (nd) {
    nd->next = next;
    n1 = 0;
    n2 = 0;
    delete this;
    return nd;
  }
  return this;
}

hunits kern_pair_node::italic_correction()
{
  return n2->italic_correction();
}

hunits kern_pair_node::subscript_correction()
{
  return n2->subscript_correction();
}

void kern_pair_node::vertical_extent(vunits *min, vunits *max)
{
  n1->vertical_extent(min, max);
  vunits min2, max2;
  n2->vertical_extent(&min2, &max2);
  if (min2 < *min)
    *min = min2;
  if (max2 > *max)
    *max = max2;
}

node *kern_pair_node::add_discretionary_hyphen()
{
  tfont *tf = n2->get_tfont();
  if (tf) {
    if (tf->contains(soft_hyphen_char)) {
      color *gcol = n2->get_glyph_color();
      color *fcol = n2->get_fill_color();
      node *next1 = next;
      next = 0;
      node *n = copy();
      glyph_node *gn = new glyph_node(soft_hyphen_char, tf, gcol, fcol,
				      state, div_nest_level);
      node *nn = n->merge_glyph_node(gn);
      if (nn == 0) {
	gn->next = n;
	nn = gn;
      }
      return new dbreak_node(this, nn, state, div_nest_level, next1);
    }
  }
  return this;
}

kern_pair_node::~kern_pair_node()
{
  if (n1 != 0)
    delete n1;
  if (n2 != 0)
    delete n2;
}

dbreak_node::~dbreak_node()
{
  delete_node_list(pre);
  delete_node_list(post);
  delete_node_list(none);
}

node *kern_pair_node::copy()
{
  return new kern_pair_node(amount, n1->copy(), n2->copy(), state,
			    div_nest_level);
}

node *copy_node_list(node *n)
{
  node *p = 0;
  while (n != 0) {
    node *nn = n->copy();
    nn->next = p;
    p = nn;
    n = n->next;
  }
  while (p != 0) {
    node *pp = p->next;
    p->next = n;
    n = p;
    p = pp;
  }
  return n;
}

void delete_node_list(node *n)
{
  while (n != 0) {
    node *tem = n;
    n = n->next;
    delete tem;
  }
}

node *dbreak_node::copy()
{
  dbreak_node *p = new dbreak_node(copy_node_list(none), copy_node_list(pre),
				   state, div_nest_level);
  p->post = copy_node_list(post);
  return p;
}

hyphen_list *node::get_hyphen_list(hyphen_list *tail, int *)
{
  return tail;
}

hyphen_list *kern_pair_node::get_hyphen_list(hyphen_list *tail, int *count)
{
  hyphen_list *hl = n2->get_hyphen_list(tail, count);
  return n1->get_hyphen_list(hl, count);
}

class hyphen_inhibitor_node : public node {
public:
  hyphen_inhibitor_node(node * = 0);
  node *copy();
  int same(node *);
  const char *type();
  int force_tprint();
  int is_tag();
  hyphenation_type get_hyphenation_type();
};

hyphen_inhibitor_node::hyphen_inhibitor_node(node *nd) : node(nd)
{
}

node *hyphen_inhibitor_node::copy()
{
  return new hyphen_inhibitor_node;
}

int hyphen_inhibitor_node::same(node *)
{
  return 1;
}

const char *hyphen_inhibitor_node::type()
{
  return "hyphen_inhibitor_node";
}

int hyphen_inhibitor_node::force_tprint()
{
  return 0;
}

int hyphen_inhibitor_node::is_tag()
{
  return 0;
}

hyphenation_type hyphen_inhibitor_node::get_hyphenation_type()
{
  return HYPHEN_INHIBIT;
}

/* add_discretionary_hyphen methods */

node *dbreak_node::add_discretionary_hyphen()
{
  if (post)
    post = post->add_discretionary_hyphen();
  if (none)
    none = none->add_discretionary_hyphen();
  return this;
}

node *node::add_discretionary_hyphen()
{
  tfont *tf = get_tfont();
  if (!tf)
    return new hyphen_inhibitor_node(this);
  if (tf->contains(soft_hyphen_char)) {
    color *gcol = get_glyph_color();
    color *fcol = get_fill_color();
    node *next1 = next;
    next = 0;
    node *n = copy();
    glyph_node *gn = new glyph_node(soft_hyphen_char, tf, gcol, fcol,
				    state, div_nest_level);
    node *n1 = n->merge_glyph_node(gn);
    if (n1 == 0) {
      gn->next = n;
      n1 = gn;
    }
    return new dbreak_node(this, n1, state, div_nest_level, next1);
  }
  return this;
}

node *node::merge_self(node *)
{
  return 0;
}

node *node::add_self(node *n, hyphen_list ** /*p*/)
{
  next = n;
  return this;
}

node *kern_pair_node::add_self(node *n, hyphen_list **p)
{
  n = n1->add_self(n, p);
  n = n2->add_self(n, p);
  n1 = n2 = 0;
  delete this;
  return n;
}

hunits node::width()
{
  return H0;
}

node *node::last_char_node()
{
  return 0;
}

int node::force_tprint()
{
  return 0;
}

int node::is_tag()
{
  return 0;
}

hunits hmotion_node::width()
{
  return n;
}

units node::size()
{
  return points_to_units(10);
}

void node::debug_node()
{
  fprintf(stderr, "{ %s ", type());
  if (push_state)
    fprintf(stderr, " <push_state>");
  if (state)
    fprintf(stderr, " <state>");
  fprintf(stderr, " nest level %d", div_nest_level);
  fprintf(stderr, " }\n");
  fflush(stderr);
}

void node::debug_node_list()
{
  node *n = next;

  debug_node();
  while (n != 0) {
    n->debug_node();
    n = n->next;
  }
}

hunits kern_pair_node::width()
{
  return n1->width() + n2->width() + amount;
}

node *kern_pair_node::last_char_node()
{
  node *nd = n2->last_char_node();
  if (nd)
    return nd;
  return n1->last_char_node();
}

hunits dbreak_node::width()
{
  hunits x = H0;
  for (node *n = none; n != 0; n = n->next)
    x += n->width();
  return x;
}

node *dbreak_node::last_char_node()
{
  for (node *n = none; n; n = n->next) {
    node *last_node = n->last_char_node();
    if (last_node)
      return last_node;
  }
  return 0;
}

hunits dbreak_node::italic_correction()
{
  return none ? none->italic_correction() : H0;
}

hunits dbreak_node::subscript_correction()
{
  return none ? none->subscript_correction() : H0;
}

class italic_corrected_node : public node {
  node *n;
  hunits x;
public:
  italic_corrected_node(node *, hunits, statem *, int, node * = 0);
  ~italic_corrected_node();
  node *copy();
  void ascii_print(ascii_output_file *);
  void asciify(macro *);
  hunits width();
  node *last_char_node();
  void vertical_extent(vunits *, vunits *);
  int ends_sentence();
  int overlaps_horizontally();
  int overlaps_vertically();
  int same(node *);
  hyphenation_type get_hyphenation_type();
  tfont *get_tfont();
  hyphen_list *get_hyphen_list(hyphen_list *, int *);
  int character_type();
  void tprint(troff_output_file *);
  hunits subscript_correction();
  hunits skew();
  node *add_self(node *, hyphen_list **);
  const char *type();
  int force_tprint();
  int is_tag();
};

node *node::add_italic_correction(hunits *wd)
{
  hunits ic = italic_correction();
  if (ic.is_zero())
    return this;
  else {
    node *next1 = next;
    next = 0;
    *wd += ic;
    return new italic_corrected_node(this, ic, state, div_nest_level, next1);
  }
}

italic_corrected_node::italic_corrected_node(node *nn, hunits xx, statem *s,
					     int pop, node *p)
: node(p, s, pop), n(nn), x(xx)
{
  assert(n != 0);
}

italic_corrected_node::~italic_corrected_node()
{
  delete n;
}

node *italic_corrected_node::copy()
{
  return new italic_corrected_node(n->copy(), x, state, div_nest_level);
}

hunits italic_corrected_node::width()
{
  return n->width() + x;
}

void italic_corrected_node::vertical_extent(vunits *min, vunits *max)
{
  n->vertical_extent(min, max);
}

void italic_corrected_node::tprint(troff_output_file *out)
{
  n->tprint(out);
  out->right(x);
}

hunits italic_corrected_node::skew()
{
  return n->skew() - x/2;
}

hunits italic_corrected_node::subscript_correction()
{
  return n->subscript_correction() - x;
}

void italic_corrected_node::ascii_print(ascii_output_file *out)
{
  n->ascii_print(out);
}

int italic_corrected_node::ends_sentence()
{
  return n->ends_sentence();
}

int italic_corrected_node::overlaps_horizontally()
{
  return n->overlaps_horizontally();
}

int italic_corrected_node::overlaps_vertically()
{
  return n->overlaps_vertically();
}

node *italic_corrected_node::last_char_node()
{
  return n->last_char_node();
}

tfont *italic_corrected_node::get_tfont()
{
  return n->get_tfont();
}

hyphenation_type italic_corrected_node::get_hyphenation_type()
{
  return n->get_hyphenation_type();
}

node *italic_corrected_node::add_self(node *nd, hyphen_list **p)
{
  nd = n->add_self(nd, p);
  hunits not_interested;
  nd = nd->add_italic_correction(&not_interested);
  n = 0;
  delete this;
  return nd;
}

hyphen_list *italic_corrected_node::get_hyphen_list(hyphen_list *tail,
						    int *count)
{
  return n->get_hyphen_list(tail, count);
}

int italic_corrected_node::character_type()
{
  return n->character_type();
}

class break_char_node : public node {
  node *ch;
  char break_code;
  color *col;
public:
  break_char_node(node *, int, color *, node * = 0);
  break_char_node(node *, int, color *, statem *, int, node * = 0);
  ~break_char_node();
  node *copy();
  hunits width();
  vunits vertical_width();
  node *last_char_node();
  int character_type();
  int ends_sentence();
  node *add_self(node *, hyphen_list **);
  hyphen_list *get_hyphen_list(hyphen_list *, int *);
  void tprint(troff_output_file *);
  void zero_width_tprint(troff_output_file *);
  void ascii_print(ascii_output_file *);
  void asciify(macro *);
  hyphenation_type get_hyphenation_type();
  int overlaps_vertically();
  int overlaps_horizontally();
  units size();
  tfont *get_tfont();
  int same(node *);
  const char *type();
  int force_tprint();
  int is_tag();
};

break_char_node::break_char_node(node *n, int bc, color *c, node *x)
: node(x), ch(n), break_code(bc), col(c)
{
}

break_char_node::break_char_node(node *n, int bc, color *c, statem *s,
				 int pop, node *x)
: node(x, s, pop), ch(n), break_code(bc), col(c)
{
}

break_char_node::~break_char_node()
{
  delete ch;
}

node *break_char_node::copy()
{
  return new break_char_node(ch->copy(), break_code, col, state,
			     div_nest_level);
}

hunits break_char_node::width()
{
  return ch->width();
}

vunits break_char_node::vertical_width()
{
  return ch->vertical_width();
}

node *break_char_node::last_char_node()
{
  return ch->last_char_node();
}

int break_char_node::character_type()
{
  return ch->character_type();
}

int break_char_node::ends_sentence()
{
  return ch->ends_sentence();
}

node *break_char_node::add_self(node *n, hyphen_list **p)
{
  assert((*p)->hyphenation_code == 0);
  if ((*p)->breakable && (break_code & 1)) {
    n = new space_node(H0, col, n);
    n->freeze_space();
  }
  next = n;
  n = this;
  if ((*p)->breakable && (break_code & 2)) {
    n = new space_node(H0, col, n);
    n->freeze_space();
  }
  hyphen_list *pp = *p;
  *p = (*p)->next;
  delete pp;
  return n;
}

hyphen_list *break_char_node::get_hyphen_list(hyphen_list *tail, int *)
{
  return new hyphen_list(0, tail);
}

hyphenation_type break_char_node::get_hyphenation_type()
{
  return HYPHEN_MIDDLE;
}

void break_char_node::ascii_print(ascii_output_file *ascii)
{
  ch->ascii_print(ascii);
}

int break_char_node::overlaps_vertically()
{
  return ch->overlaps_vertically();
}

int break_char_node::overlaps_horizontally()
{
  return ch->overlaps_horizontally();
}

units break_char_node::size()
{
  return ch->size();
}

tfont *break_char_node::get_tfont()
{
  return ch->get_tfont();
}

node *extra_size_node::copy()
{
  return new extra_size_node(n, state, div_nest_level);
}

extra_size_node::extra_size_node(vunits i, statem *s, int pop)
: node(0, s, pop), n(i)
{
}

extra_size_node::extra_size_node(vunits i)
: n(i)
{
}

node *vertical_size_node::copy()
{
  return new vertical_size_node(n, state, div_nest_level);
}

vertical_size_node::vertical_size_node(vunits i, statem *s, int pop)
: node(0, s, pop), n(i)
{
}

vertical_size_node::vertical_size_node(vunits i)
: n(i)
{
}

node *hmotion_node::copy()
{
  return new hmotion_node(n, was_tab, unformat, col, state, div_nest_level);
}

node *space_char_hmotion_node::copy()
{
  return new space_char_hmotion_node(n, col, state, div_nest_level);
}

vmotion_node::vmotion_node(vunits i, color *c)
: n(i), col(c)
{
}

vmotion_node::vmotion_node(vunits i, color *c, statem *s, int pop)
: node(0, s, pop), n(i), col(c)
{
}

node *vmotion_node::copy()
{
  return new vmotion_node(n, col, state, div_nest_level);
}

node *dummy_node::copy()
{
  return new dummy_node;
}

node *transparent_dummy_node::copy()
{
  return new transparent_dummy_node;
}

hline_node::~hline_node()
{
  if (n)
    delete n;
}

hline_node::hline_node(hunits i, node *c, node *nxt)
: node(nxt), x(i), n(c)
{
}

hline_node::hline_node(hunits i, node *c, statem *s, int pop, node *nxt)
: node(nxt, s, pop), x(i), n(c)
{
}

node *hline_node::copy()
{
  return new hline_node(x, n ? n->copy() : 0, state, div_nest_level);
}

hunits hline_node::width()
{
  return x < H0 ? H0 : x;
}

vline_node::vline_node(vunits i, node *c, node *nxt)
: node(nxt), x(i), n(c)
{
}

vline_node::vline_node(vunits i, node *c, statem *s, int pop, node *nxt)
: node(nxt, s, pop), x(i), n(c)
{
}

vline_node::~vline_node()
{
  if (n)
    delete n;
}

node *vline_node::copy()
{
  return new vline_node(x, n ? n->copy() : 0, state, div_nest_level);
}

hunits vline_node::width()
{
  return n == 0 ? H0 : n->width();
}

zero_width_node::zero_width_node(node *nd, statem *s, int pop)
: node(0, s, pop), n(nd)
{
}

zero_width_node::zero_width_node(node *nd)
: n(nd)
{
}

zero_width_node::~zero_width_node()
{
  delete_node_list(n);
}

node *zero_width_node::copy()
{
  return new zero_width_node(copy_node_list(n), state, div_nest_level);
}

int node_list_character_type(node *p)
{
  int t = 0;
  for (; p; p = p->next)
    t |= p->character_type();
  return t;
}

int zero_width_node::character_type()
{
  return node_list_character_type(n);
}

void node_list_vertical_extent(node *p, vunits *min, vunits *max)
{
  *min = V0;
  *max = V0;
  vunits cur_vpos = V0;
  vunits v1, v2;
  for (; p; p = p->next) {
    p->vertical_extent(&v1, &v2);
    v1 += cur_vpos;
    if (v1 < *min)
      *min = v1;
    v2 += cur_vpos;
    if (v2 > *max)
      *max = v2;
    cur_vpos += p->vertical_width();
  }
}

void zero_width_node::vertical_extent(vunits *min, vunits *max)
{
  node_list_vertical_extent(n, min, max);
}

overstrike_node::overstrike_node()
: list(0), max_width(H0)
{
}

overstrike_node::overstrike_node(statem *s, int pop)
: node(0, s, pop), list(0), max_width(H0)
{
}

overstrike_node::~overstrike_node()
{
  delete_node_list(list);
}

node *overstrike_node::copy()
{
  overstrike_node *on = new overstrike_node(state, div_nest_level);
  for (node *tem = list; tem; tem = tem->next)
    on->overstrike(tem->copy());
  return on;
}

void overstrike_node::overstrike(node *n)
{
  if (n == 0)
    return;
  hunits w = n->width();
  if (w > max_width)
    max_width = w;
  node **p;
  for (p = &list; *p; p = &(*p)->next)
    ;
  n->next = 0;
  *p = n;
}

hunits overstrike_node::width()
{
  return max_width;
}

bracket_node::bracket_node()
: list(0), max_width(H0)
{
}

bracket_node::bracket_node(statem *s, int pop)
: node(0, s, pop), list(0), max_width(H0)
{
}

bracket_node::~bracket_node()
{
  delete_node_list(list);
}

node *bracket_node::copy()
{
  bracket_node *on = new bracket_node(state, div_nest_level);
  node *last_node = 0;
  node *tem;
  if (list)
    list->last = 0;
  for (tem = list; tem; tem = tem->next) {
    if (tem->next)
      tem->next->last = tem;
    last_node = tem;
  }
  for (tem = last_node; tem; tem = tem->last)
    on->bracket(tem->copy());
  return on;
}

void bracket_node::bracket(node *n)
{
  if (n == 0)
    return;
  hunits w = n->width();
  if (w > max_width)
    max_width = w;
  n->next = list;
  list = n;
}

hunits bracket_node::width()
{
  return max_width;
}

int node::nspaces()
{
  return 0;
}

int node::merge_space(hunits, hunits, hunits)
{
  return 0;
}

#if 0
space_node *space_node::free_list = 0;

void *space_node::operator new(size_t n)
{
  assert(n == sizeof(space_node));
  if (!free_list) {
    free_list = (space_node *)new char[sizeof(space_node)*BLOCK];
    for (int i = 0; i < BLOCK - 1; i++)
      free_list[i].next = free_list + i + 1;
    free_list[BLOCK-1].next = 0;
  }
  space_node *p = free_list;
  free_list = (space_node *)(free_list->next);
  p->next = 0;
  return p;
}

inline void space_node::operator delete(void *p)
{
  if (p) {
    ((space_node *)p)->next = free_list;
    free_list = (space_node *)p;
  }
}
#endif

space_node::space_node(hunits nn, color *c, node *p)
: node(p, 0, 0), n(nn), set(0), was_escape_colon(0), col(c)
{
}

space_node::space_node(hunits nn, color *c, statem *s, int pop, node *p)
: node(p, s, pop), n(nn), set(0), was_escape_colon(0), col(c)
{
}

space_node::space_node(hunits nn, int s, int flag, color *c, statem *st,
		       int pop, node *p)
: node(p, st, pop), n(nn), set(s), was_escape_colon(flag), col(c)
{
}

#if 0
space_node::~space_node()
{
}
#endif

node *space_node::copy()
{
  return new space_node(n, set, was_escape_colon, col, state, div_nest_level);
}

int space_node::force_tprint()
{
  return 0;
}

int space_node::is_tag()
{
  return 0;
}

int space_node::nspaces()
{
  return set ? 0 : 1;
}

int space_node::merge_space(hunits h, hunits, hunits)
{
  n += h;
  return 1;
}

hunits space_node::width()
{
  return n;
}

void node::spread_space(int*, hunits*)
{
}

void space_node::spread_space(int *n_spaces, hunits *desired_space)
{
  if (!set) {
    assert(*n_spaces > 0);
    if (*n_spaces == 1) {
      n += *desired_space;
      *desired_space = H0;
    }
    else {
      hunits extra = *desired_space / *n_spaces;
      *desired_space -= extra;
      n += extra;
    }
    *n_spaces -= 1;
    set = 1;
  }
}

void node::freeze_space()
{
}

void space_node::freeze_space()
{
  set = 1;
}

void node::is_escape_colon()
{
}

void space_node::is_escape_colon()
{
  was_escape_colon = 1;
}

diverted_space_node::diverted_space_node(vunits d, statem *s, int pop,
					 node *p)
: node(p, s, pop), n(d)
{
}

diverted_space_node::diverted_space_node(vunits d, node *p)
: node(p), n(d)
{
}

node *diverted_space_node::copy()
{
  return new diverted_space_node(n, state, div_nest_level);
}

diverted_copy_file_node::diverted_copy_file_node(symbol s, statem *st,
						 int pop, node *p)
: node(p, st, pop), filename(s)
{
}

diverted_copy_file_node::diverted_copy_file_node(symbol s, node *p)
: node(p), filename(s)
{
}

node *diverted_copy_file_node::copy()
{
  return new diverted_copy_file_node(filename, state, div_nest_level);
}

int node::ends_sentence()
{
  return 0;
}

int kern_pair_node::ends_sentence()
{
  switch (n2->ends_sentence()) {
  case 0:
    return 0;
  case 1:
    return 1;
  case 2:
    break;
  default:
    assert(0);
  }
  return n1->ends_sentence();
}

int node_list_ends_sentence(node *n)
{
  for (; n != 0; n = n->next)
    switch (n->ends_sentence()) {
    case 0:
      return 0;
    case 1:
      return 1;
    case 2:
      break;
    default:
      assert(0);
    }
  return 2;
}

int dbreak_node::ends_sentence()
{
  return node_list_ends_sentence(none);
}

int node::overlaps_horizontally()
{
  return 0;
}

int node::overlaps_vertically()
{
  return 0;
}

int node::discardable()
{
  return 0;
}

int space_node::discardable()
{
  return set ? 0 : 1;
}

vunits node::vertical_width()
{
  return V0;
}

vunits vline_node::vertical_width()
{
  return x;
}

vunits vmotion_node::vertical_width()
{
  return n;
}

int node::set_unformat_flag()
{
  return 1;
}

int node::character_type()
{
  return 0;
}

hunits node::subscript_correction()
{
  return H0;
}

hunits node::italic_correction()
{
  return H0;
}

hunits node::left_italic_correction()
{
  return H0;
}

hunits node::skew()
{
  return H0;
}

/* vertical_extent methods */

void node::vertical_extent(vunits *min, vunits *max)
{
  vunits v = vertical_width();
  if (v < V0) {
    *min = v;
    *max = V0;
  }
  else {
    *max = v;
    *min = V0;
  }
}

void vline_node::vertical_extent(vunits *min, vunits *max)
{
  if (n == 0)
    node::vertical_extent(min, max);
  else {
    vunits cmin, cmax;
    n->vertical_extent(&cmin, &cmax);
    vunits h = n->size();
    if (x < V0) {
      if (-x < h) {
	*min = x;
	*max = V0;
      }
      else {
	// we print the first character and then move up, so
	*max = cmax;
	// we print the last character and then move up h
	*min = cmin + h;
	if (*min > V0)
	  *min = V0;
	*min += x;
      }
    }
    else {
      if (x < h) {
	*max = x;
	*min = V0;
      }
      else {
	// we move down by h and then print the first character, so
	*min = cmin + h;
	if (*min > V0)
	  *min = V0;
	*max = x + cmax;
      }
    }
  }
}

/* ascii_print methods */

static void ascii_print_reverse_node_list(ascii_output_file *ascii, node *n)
{
  if (n == 0)
    return;
  ascii_print_reverse_node_list(ascii, n->next);
  n->ascii_print(ascii);
}

void dbreak_node::ascii_print(ascii_output_file *ascii)
{
  ascii_print_reverse_node_list(ascii, none);
}

void kern_pair_node::ascii_print(ascii_output_file *ascii)
{
  n1->ascii_print(ascii);
  n2->ascii_print(ascii);
}

void node::ascii_print(ascii_output_file *)
{
}

void space_node::ascii_print(ascii_output_file *ascii)
{
  if (!n.is_zero())
    ascii->outc(' ');
}

void hmotion_node::ascii_print(ascii_output_file *ascii)
{
  // this is pretty arbitrary
  if (n >= points_to_units(2))
    ascii->outc(' ');
}

void space_char_hmotion_node::ascii_print(ascii_output_file *ascii)
{
  ascii->outc(' ');
}

/* asciify methods */

void node::asciify(macro *m)
{
  m->append(this);
}

void glyph_node::asciify(macro *m)
{
  unsigned char c = ci->get_asciify_code();
  if (c == 0)
    c = ci->get_ascii_code();
  if (c != 0) {
    m->append(c);
    delete this;
  }
  else
    m->append(this);
}

void kern_pair_node::asciify(macro *m)
{
  n1->asciify(m);
  n2->asciify(m);
  n1 = n2 = 0;
  delete this;
}

static void asciify_reverse_node_list(macro *m, node *n)
{
  if (n == 0)
    return;
  asciify_reverse_node_list(m, n->next);
  n->asciify(m);
}

void dbreak_node::asciify(macro *m)
{
  asciify_reverse_node_list(m, none);
  none = 0;
  delete this;
}

void ligature_node::asciify(macro *m)
{
  n1->asciify(m);
  n2->asciify(m);
  n1 = n2 = 0;
  delete this;
}

void break_char_node::asciify(macro *m)
{
  ch->asciify(m);
  ch = 0;
  delete this;
}

void italic_corrected_node::asciify(macro *m)
{
  n->asciify(m);
  n = 0;
  delete this;
}

void left_italic_corrected_node::asciify(macro *m)
{
  if (n) {
    n->asciify(m);
    n = 0;
  }
  delete this;
}

void hmotion_node::asciify(macro *m)
{
  if (was_tab) {
    m->append('\t');
    delete this;
  }
  else
    m->append(this);
}

space_char_hmotion_node::space_char_hmotion_node(hunits i, color *c,
						 statem *s, int pop,
						 node *nxt)
: hmotion_node(i, c, s, pop, nxt)
{
}

space_char_hmotion_node::space_char_hmotion_node(hunits i, color *c,
 						 node *nxt)
: hmotion_node(i, c, 0, 0, nxt)
{
}

void space_char_hmotion_node::asciify(macro *m)
{
  m->append(ESCAPE_SPACE);
  delete this;
}

void space_node::asciify(macro *m)
{
  if (was_escape_colon) {
    m->append(ESCAPE_COLON);
    delete this;
  }
  else
    m->append(this);
}

void word_space_node::asciify(macro *m)
{
  for (width_list *w = orig_width; w; w = w->next)
    m->append(' ');
  delete this;
}

void unbreakable_space_node::asciify(macro *m)
{
  m->append(ESCAPE_TILDE);
  delete this;
}

void line_start_node::asciify(macro *)
{
  delete this;
}

void vertical_size_node::asciify(macro *)
{
  delete this;
}

breakpoint *node::get_breakpoints(hunits /*width*/, int /*nspaces*/,
				  breakpoint *rest, int /*is_inner*/)
{
  return rest;
}

int node::nbreaks()
{
  return 0;
}

breakpoint *space_node::get_breakpoints(hunits wd, int ns,
					breakpoint *rest, int is_inner)
{
  if (next && next->discardable())
    return rest;
  breakpoint *bp = new breakpoint;
  bp->next = rest;
  bp->width = wd;
  bp->nspaces = ns;
  bp->hyphenated = 0;
  if (is_inner) {
    assert(rest != 0);
    bp->index = rest->index + 1;
    bp->nd = rest->nd;
  }
  else {
    bp->nd = this;
    bp->index = 0;
  }
  return bp;
}

int space_node::nbreaks()
{
  if (next && next->discardable())
    return 0;
  else
    return 1;
}

static breakpoint *node_list_get_breakpoints(node *p, hunits *widthp,
					     int ns, breakpoint *rest)
{
  if (p != 0) {
    rest = p->get_breakpoints(*widthp,
			      ns,
			      node_list_get_breakpoints(p->next, widthp, ns,
							rest),
			      1);
    *widthp += p->width();
  }
  return rest;
}

breakpoint *dbreak_node::get_breakpoints(hunits wd, int ns,
					 breakpoint *rest, int is_inner)
{
  breakpoint *bp = new breakpoint;
  bp->next = rest;
  bp->width = wd;
  for (node *tem = pre; tem != 0; tem = tem->next)
    bp->width += tem->width();
  bp->nspaces = ns;
  bp->hyphenated = 1;
  if (is_inner) {
    assert(rest != 0);
    bp->index = rest->index + 1;
    bp->nd = rest->nd;
  }
  else {
    bp->nd = this;
    bp->index = 0;
  }
  return node_list_get_breakpoints(none, &wd, ns, bp);
}

int dbreak_node::nbreaks()
{
  int i = 1;
  for (node *tem = none; tem != 0; tem = tem->next)
    i += tem->nbreaks();
  return i;
}

void node::split(int /*where*/, node ** /*prep*/, node ** /*postp*/)
{
  assert(0);
}

void space_node::split(int where, node **pre, node **post)
{
  assert(where == 0);
  *pre = next;
  *post = 0;
  delete this;
}

static void node_list_split(node *p, int *wherep, node **prep, node **postp)
{
  if (p == 0)
    return;
  int nb = p->nbreaks();
  node_list_split(p->next, wherep, prep, postp);
  if (*wherep < 0) {
    p->next = *postp;
    *postp = p;
  }
  else if (*wherep < nb) {
    p->next = *prep;
    p->split(*wherep, prep, postp);
  }
  else {
    p->next = *prep;
    *prep = p;
  }
  *wherep -= nb;
}

void dbreak_node::split(int where, node **prep, node **postp)
{
  assert(where >= 0);
  if (where == 0) {
    *postp = post;
    post = 0;
    if (pre == 0)
      *prep = next;
    else {
      node *tem;
      for (tem = pre; tem->next != 0; tem = tem->next)
	;
      tem->next = next;
      *prep = pre;
    }
    pre = 0;
    delete this;
  }
  else {
    *prep = next;
    where -= 1;
    node_list_split(none, &where, prep, postp);
    none = 0;
    delete this;
  }
}

hyphenation_type node::get_hyphenation_type()
{
  return HYPHEN_BOUNDARY;
}

hyphenation_type dbreak_node::get_hyphenation_type()
{
  return HYPHEN_INHIBIT;
}

hyphenation_type kern_pair_node::get_hyphenation_type()
{
  return HYPHEN_MIDDLE;
}

hyphenation_type dummy_node::get_hyphenation_type()
{
  return HYPHEN_MIDDLE;
}

hyphenation_type transparent_dummy_node::get_hyphenation_type()
{
  return HYPHEN_MIDDLE;
}

hyphenation_type hmotion_node::get_hyphenation_type()
{
  return HYPHEN_MIDDLE;
}

hyphenation_type space_char_hmotion_node::get_hyphenation_type()
{
  return HYPHEN_MIDDLE;
}

hyphenation_type overstrike_node::get_hyphenation_type()
{
  return HYPHEN_MIDDLE;
}

hyphenation_type space_node::get_hyphenation_type()
{
  if (was_escape_colon)
    return HYPHEN_MIDDLE;
  return HYPHEN_BOUNDARY;
}

hyphenation_type unbreakable_space_node::get_hyphenation_type()
{
  return HYPHEN_MIDDLE;
}

int node::interpret(macro *)
{
  return 0;
}

special_node::special_node(const macro &m, int n)
: mac(m), no_init_string(n)
{
  font_size fs = curenv->get_font_size();
  int char_height = curenv->get_char_height();
  int char_slant = curenv->get_char_slant();
  int fontno = env_definite_font(curenv);
  tf = font_table[fontno]->get_tfont(fs, char_height, char_slant, fontno);
  if (curenv->is_composite())
    tf = tf->get_plain();
  gcol = curenv->get_glyph_color();
  fcol = curenv->get_fill_color();
  is_special = 1;
}

special_node::special_node(const macro &m, tfont *t,
			   color *gc, color *fc,
			   statem *s, int pop,
			   int n)
: node(0, s, pop), mac(m), tf(t), gcol(gc), fcol(fc), no_init_string(n)
{
  is_special = 1;
}

int special_node::same(node *n)
{
  return mac == ((special_node *)n)->mac
	 && tf == ((special_node *)n)->tf
	 && gcol == ((special_node *)n)->gcol
	 && fcol == ((special_node *)n)->fcol
	 && no_init_string == ((special_node *)n)->no_init_string;
}

const char *special_node::type()
{
  return "special_node";
}

int special_node::ends_sentence()
{
  return 2;
}

int special_node::force_tprint()
{
  return 0;
}

int special_node::is_tag()
{
  return 0;
}

node *special_node::copy()
{
  return new special_node(mac, tf, gcol, fcol, state, div_nest_level,
			  no_init_string);
}

void special_node::tprint_start(troff_output_file *out)
{
  out->start_special(tf, gcol, fcol, no_init_string);
}

void special_node::tprint_char(troff_output_file *out, unsigned char c)
{
  out->special_char(c);
}

void special_node::tprint_end(troff_output_file *out)
{
  out->end_special();
}

tfont *special_node::get_tfont()
{
  return tf;
}

/* suppress_node */

suppress_node::suppress_node(int on_or_off, int issue_limits)
: is_on(on_or_off), emit_limits(issue_limits), filename(0), position(0),
  image_id(0)
{
}

suppress_node::suppress_node(symbol f, char p, int id)
: is_on(2), emit_limits(0), filename(f), position(p), image_id(id)
{
  is_special = 1;
}

suppress_node::suppress_node(int issue_limits, int on_or_off,
			     symbol f, char p, int id,
			     statem *s, int pop)
: node(0, s, pop), is_on(on_or_off), emit_limits(issue_limits), filename(f),
  position(p), image_id(id)
{
}

int suppress_node::same(node *n)
{
  return ((is_on == ((suppress_node *)n)->is_on)
	  && (emit_limits == ((suppress_node *)n)->emit_limits)
	  && (filename == ((suppress_node *)n)->filename)
	  && (position == ((suppress_node *)n)->position)
	  && (image_id == ((suppress_node *)n)->image_id));
}

const char *suppress_node::type()
{
  return "suppress_node";
}

node *suppress_node::copy()
{
  return new suppress_node(emit_limits, is_on, filename, position, image_id,
			   state, div_nest_level);
}

/* tag_node */

tag_node::tag_node()
: delayed(0)
{
  is_special = 1;
}

tag_node::tag_node(string s, int delay)
: tag_string(s), delayed(delay)
{
  is_special = !delay;
}

tag_node::tag_node(string s, statem *st, int pop, int delay)
: node(0, st, pop), tag_string(s), delayed(delay)
{
  is_special = !delay;
}

node *tag_node::copy()
{
  return new tag_node(tag_string, state, div_nest_level, delayed);
}

void tag_node::tprint(troff_output_file *out)
{
  if (delayed)
    out->add_to_tag_list(tag_string);
  else
    out->state.add_tag(out->fp, tag_string);
}

int tag_node::same(node *nd)
{
  return tag_string == ((tag_node *)nd)->tag_string
	 && delayed == ((tag_node *)nd)->delayed;
}

const char *tag_node::type()
{
  return "tag_node";
}

int tag_node::force_tprint()
{
  return !delayed;
}

int tag_node::is_tag()
{
  return !delayed;
}

int tag_node::ends_sentence()
{
  return 2;
}

int get_reg_int(const char *p)
{
  reg *r = (reg *)number_reg_dictionary.lookup(p);
  units prev_value;
  if (r && (r->get_value(&prev_value)))
    return (int)prev_value;
  else
    warning(WARN_REG, "number register `%1' not defined", p);
  return 0;
}

const char *get_reg_str(const char *p)
{
  reg *r = (reg *)number_reg_dictionary.lookup(p);
  if (r)
    return r->get_string();
  else
    warning(WARN_REG, "register `%1' not defined", p);
  return 0;
}

void suppress_node::put(troff_output_file *out, const char *s)
{
  int i = 0;
  while (s[i] != (char)0) {
    out->special_char(s[i]);
    i++;
  }
}

/*
 *  We need to remember the start of the image and its name.
 */

static char last_position = 0;
static const char *last_image_filename = 0;
static int last_image_id = 0;

inline int min(int a, int b)
{
  return a < b ? a : b;
}

/*
 *  tprint - if (is_on == 2)
 *               remember current position (l, r, c, i) and filename
 *           else
 *               if (emit_limits)
 *                   if (html)
 *                      emit image tag
 *                   else
 *                      emit postscript bounds for image
 *               else
 *                  if (suppress boolean differs from current state)
 *                      alter state
 *                  reset registers
 *                  record current page
 *                  set low water mark.
 */

void suppress_node::tprint(troff_output_file *out)
{
  int current_page = topdiv->get_page_number();
  // firstly check to see whether this suppress node contains
  // an image filename & position.
  if (is_on == 2) {
    // remember position and filename
    last_position = position;
    char *tem = (char *)last_image_filename;
    last_image_filename = strsave(filename.contents());
    if (tem)
      a_delete tem;
    last_image_id = image_id;
    // printf("start of image and page = %d\n", current_page);
  }
  else {
    // now check whether the suppress node requires us to issue limits.
    if (emit_limits) {
      char name[8192];
      // remember that the filename will contain a %d in which the
      // last_image_id is placed
      if (last_image_filename == (char *) 0)
	*name = '\0';
      else
	sprintf(name, last_image_filename, last_image_id);
      if (is_html) {
	switch (last_position) {
	case 'c':
	  out->start_special();
	  put(out, "devtag:.centered-image");
	  break;
	case 'r':
	  out->start_special();
	  put(out, "devtag:.right-image");
	  break;
	case 'l':
	  out->start_special();
	  put(out, "devtag:.left-image");
	  break;
	case 'i':
	  ;
	default:
	  ;
	}
	out->end_special();
	out->start_special();
	put(out, "devtag:.auto-image ");
	put(out, name);
	out->end_special();
      }
      else {
	// postscript (or other device)
	if (suppress_start_page > 0 && current_page != suppress_start_page)
	  error("suppression limit registers span more than one page;\n"
	        "image description %1 will be wrong", image_no);
	// if (topdiv->get_page_number() != suppress_start_page)
	//  fprintf(stderr, "end of image and topdiv page = %d   and  suppress_start_page = %d\n",
	//	  topdiv->get_page_number(), suppress_start_page);

	// remember that the filename will contain a %d in which the
	// image_no is placed
	fprintf(stderr,
		"grohtml-info:page %d  %d  %d  %d  %d  %d  %s  %d  %d  %s\n",
		topdiv->get_page_number(),
		get_reg_int("opminx"), get_reg_int("opminy"),
		get_reg_int("opmaxx"), get_reg_int("opmaxy"),
		// page offset + line length
		get_reg_int(".o") + get_reg_int(".l"),
		name, hresolution, vresolution, get_reg_str(".F"));
	fflush(stderr);
      }
    }
    else {
      if (is_on) {
	out->on();
	// lastly we reset the output registers
	reset_output_registers();
      }
      else
	out->off();
      suppress_start_page = current_page;
    }
  }
}

int suppress_node::force_tprint()
{
  return is_on;
}

int suppress_node::is_tag()
{
  return is_on;
}

hunits suppress_node::width()
{
  return H0;
}

/* composite_node */

class composite_node : public charinfo_node {
  node *n;
  tfont *tf;
public:
  composite_node(node *, charinfo *, tfont *, statem *, int, node * = 0);
  ~composite_node();
  node *copy();
  hunits width();
  node *last_char_node();
  units size();
  void tprint(troff_output_file *);
  hyphenation_type get_hyphenation_type();
  void ascii_print(ascii_output_file *);
  void asciify(macro *);
  hyphen_list *get_hyphen_list(hyphen_list *, int *);
  node *add_self(node *, hyphen_list **);
  tfont *get_tfont();
  int same(node *);
  const char *type();
  int force_tprint();
  int is_tag();
  void vertical_extent(vunits *, vunits *);
  vunits vertical_width();
};

composite_node::composite_node(node *p, charinfo *c, tfont *t, statem *s,
			       int pop, node *x)
: charinfo_node(c, s, pop, x), n(p), tf(t)
{
}

composite_node::~composite_node()
{
  delete_node_list(n);
}

node *composite_node::copy()
{
  return new composite_node(copy_node_list(n), ci, tf, state, div_nest_level);
}

hunits composite_node::width()
{
  hunits x;
  if (tf->get_constant_space(&x))
    return x;
  x = H0;
  for (node *tem = n; tem; tem = tem->next)
    x += tem->width();
  hunits offset;
  if (tf->get_bold(&offset))
    x += offset;
  x += tf->get_track_kern();
  return x;
}

node *composite_node::last_char_node()
{
  return this;
}

vunits composite_node::vertical_width()
{
  vunits v = V0;
  for (node *tem = n; tem; tem = tem->next)
    v += tem->vertical_width();
  return v;
}

units composite_node::size()
{
  return tf->get_size().to_units();
}

hyphenation_type composite_node::get_hyphenation_type()
{
  return HYPHEN_MIDDLE;
}

void composite_node::asciify(macro *m)
{
  unsigned char c = ci->get_asciify_code();
  if (c == 0)
    c = ci->get_ascii_code();
  if (c != 0) {
    m->append(c);
    delete this;
  }
  else
    m->append(this);
}

void composite_node::ascii_print(ascii_output_file *ascii)
{
  unsigned char c = ci->get_ascii_code();
  if (c != 0)
    ascii->outc(c);
  else
    ascii->outs(ci->nm.contents());

}

hyphen_list *composite_node::get_hyphen_list(hyphen_list *tail, int *count)
{
  (*count)++;
  return new hyphen_list(ci->get_hyphenation_code(), tail);
}

node *composite_node::add_self(node *nn, hyphen_list **p)
{
  assert(ci->get_hyphenation_code() == (*p)->hyphenation_code);
  next = nn;
  nn = this;
  if ((*p)->hyphen)
    nn = nn->add_discretionary_hyphen();
  hyphen_list *pp = *p;
  *p = (*p)->next;
  delete pp;
  return nn;
}

tfont *composite_node::get_tfont()
{
  return tf;
}

node *reverse_node_list(node *n)
{
  node *r = 0;
  while (n) {
    node *tem = n;
    n = n->next;
    tem->next = r;
    r = tem;
  }
  return r;
}

void composite_node::vertical_extent(vunits *minimum, vunits *maximum)
{
  n = reverse_node_list(n);
  node_list_vertical_extent(n, minimum, maximum);
  n = reverse_node_list(n);
}

width_list::width_list(hunits w, hunits s)
: width(w), sentence_width(s), next(0)
{
}

width_list::width_list(width_list *w)
: width(w->width), sentence_width(w->sentence_width), next(0)
{
}

word_space_node::word_space_node(hunits d, color *c, width_list *w, node *x)
: space_node(d, c, x), orig_width(w), unformat(0)
{
}

word_space_node::word_space_node(hunits d, int s, color *c, width_list *w,
				 int flag, statem *st, int pop, node *x)
: space_node(d, s, 0, c, st, pop, x), orig_width(w), unformat(flag)
{
}

word_space_node::~word_space_node()
{
  width_list *w = orig_width;
  while (w != 0) {
    width_list *tmp = w;
    w = w->next;
    delete tmp;
  }
}

node *word_space_node::copy()
{
  assert(orig_width != 0);
  width_list *w_old_curr = orig_width;
  width_list *w_new_curr = new width_list(w_old_curr);
  width_list *w_new = w_new_curr;
  w_old_curr = w_old_curr->next;
  while (w_old_curr != 0) {
    w_new_curr->next = new width_list(w_old_curr);
    w_new_curr = w_new_curr->next;
    w_old_curr = w_old_curr->next;
  }
  return new word_space_node(n, set, col, w_new, unformat, state,
			     div_nest_level);
}

int word_space_node::set_unformat_flag()
{
  unformat = 1;
  return 1;
}

void word_space_node::tprint(troff_output_file *out)
{
  out->fill_color(col);
  out->word_marker();
  out->right(n);
}

int word_space_node::merge_space(hunits h, hunits sw, hunits ssw)
{
  n += h;
  assert(orig_width != 0);
  width_list *w = orig_width; 
  for (; w->next; w = w->next)
    ;
  w->next = new width_list(sw, ssw);
  return 1;
}

unbreakable_space_node::unbreakable_space_node(hunits d, color *c, node *x)
: word_space_node(d, c, 0, x)
{
}

unbreakable_space_node::unbreakable_space_node(hunits d, int s,
					       color *c, statem *st, int pop,
					       node *x)
: word_space_node(d, s, c, 0, 0, st, pop, x)
{
}

node *unbreakable_space_node::copy()
{
  return new unbreakable_space_node(n, set, col, state, div_nest_level);
}

int unbreakable_space_node::force_tprint()
{
  return 0;
}

int unbreakable_space_node::is_tag()
{
  return 0;
}

breakpoint *unbreakable_space_node::get_breakpoints(hunits, int,
						    breakpoint *rest, int)
{
  return rest;
}

int unbreakable_space_node::nbreaks()
{
  return 0;
}

void unbreakable_space_node::split(int, node **, node **)
{
  assert(0);
}

int unbreakable_space_node::merge_space(hunits, hunits, hunits)
{
  return 0;
}

hvpair::hvpair()
{
}

draw_node::draw_node(char c, hvpair *p, int np, font_size s,
		     color *gc, color *fc)
: npoints(np), sz(s), gcol(gc), fcol(fc), code(c)
{
  point = new hvpair[npoints];
  for (int i = 0; i < npoints; i++)
    point[i] = p[i];
}

draw_node::draw_node(char c, hvpair *p, int np, font_size s,
		     color *gc, color *fc, statem *st, int pop)
: node(0, st, pop), npoints(np), sz(s), gcol(gc), fcol(fc), code(c)
{
  point = new hvpair[npoints];
  for (int i = 0; i < npoints; i++)
    point[i] = p[i];
}

int draw_node::same(node *n)
{
  draw_node *nd = (draw_node *)n;
  if (code != nd->code || npoints != nd->npoints || sz != nd->sz
      || gcol != nd->gcol || fcol != nd->fcol)
    return 0;
  for (int i = 0; i < npoints; i++)
    if (point[i].h != nd->point[i].h || point[i].v != nd->point[i].v)
      return 0;
  return 1;
}

const char *draw_node::type()
{
  return "draw_node";
}

int draw_node::force_tprint()
{
  return 0;
}

int draw_node::is_tag()
{
  return 0;
}

draw_node::~draw_node()
{
  if (point)
    a_delete point;
}

hunits draw_node::width()
{
  hunits x = H0;
  for (int i = 0; i < npoints; i++)
    x += point[i].h;
  return x;
}

vunits draw_node::vertical_width()
{
  if (code == 'e')
    return V0;
  vunits x = V0;
  for (int i = 0; i < npoints; i++)
    x += point[i].v;
  return x;
}

node *draw_node::copy()
{
  return new draw_node(code, point, npoints, sz, gcol, fcol, state,
		       div_nest_level);
}

void draw_node::tprint(troff_output_file *out)
{
  out->draw(code, point, npoints, sz, gcol, fcol);
}

/* tprint methods */

void glyph_node::tprint(troff_output_file *out)
{
  tfont *ptf = tf->get_plain();
  if (ptf == tf)
    out->put_char_width(ci, ptf, gcol, fcol, width(), H0);
  else {
    hunits offset;
    int bold = tf->get_bold(&offset);
    hunits w = ptf->get_width(ci);
    hunits k = H0;
    hunits x;
    int cs = tf->get_constant_space(&x);
    if (cs) {
      x -= w;
      if (bold)
	x -= offset;
      hunits x2 = x/2;
      out->right(x2);
      k = x - x2;
    }
    else
      k = tf->get_track_kern();
    if (bold) {
      out->put_char(ci, ptf, gcol, fcol);
      out->right(offset);
    }
    out->put_char_width(ci, ptf, gcol, fcol, w, k);
  }
}

void glyph_node::zero_width_tprint(troff_output_file *out)
{
  tfont *ptf = tf->get_plain();
  hunits offset;
  int bold = tf->get_bold(&offset);
  hunits x;
  int cs = tf->get_constant_space(&x);
  if (cs) {
    x -= ptf->get_width(ci);
    if (bold)
      x -= offset;
    x = x/2;
    out->right(x);
  }
  out->put_char(ci, ptf, gcol, fcol);
  if (bold) {
    out->right(offset);
    out->put_char(ci, ptf, gcol, fcol);
    out->right(-offset);
  }
  if (cs)
    out->right(-x);
}

void break_char_node::tprint(troff_output_file *t)
{
  ch->tprint(t);
}

void break_char_node::zero_width_tprint(troff_output_file *t)
{
  ch->zero_width_tprint(t);
}

void hline_node::tprint(troff_output_file *out)
{
  if (x < H0) {
    out->right(x);
    x = -x;
  }
  if (n == 0) {
    out->right(x);
    return;
  }
  hunits w = n->width();
  if (w <= H0) {
    error("horizontal line drawing character must have positive width");
    out->right(x);
    return;
  }
  int i = int(x/w);
  if (i == 0) {
    hunits xx = x - w;
    hunits xx2 = xx/2;
    out->right(xx2);
    if (out->is_on())
      n->tprint(out);
    out->right(xx - xx2);
  }
  else {
    hunits rem = x - w*i;
    if (rem > H0)
      if (n->overlaps_horizontally()) {
	if (out->is_on())
	  n->tprint(out);
	out->right(rem - w);
      }
      else
	out->right(rem);
    while (--i >= 0)
      if (out->is_on())
	n->tprint(out);
  }
}

void vline_node::tprint(troff_output_file *out)
{
  if (n == 0) {
    out->down(x);
    return;
  }
  vunits h = n->size();
  int overlaps = n->overlaps_vertically();
  vunits y = x;
  if (y < V0) {
    y = -y;
    int i = y / h;
    vunits rem = y - i*h;
    if (i == 0) {
      out->right(n->width());
      out->down(-rem);
    }
    else {
      while (--i > 0) {
	n->zero_width_tprint(out);
	out->down(-h);
      }
      if (overlaps) {
	n->zero_width_tprint(out);
	out->down(-rem);
	if (out->is_on())
	  n->tprint(out);
	out->down(-h);
      }
      else {
	if (out->is_on())
	  n->tprint(out);
	out->down(-h - rem);
      }
    }
  }
  else {
    int i = y / h;
    vunits rem = y - i*h;
    if (i == 0) {
      out->down(rem);
      out->right(n->width());
    }
    else {
      out->down(h);
      if (overlaps)
	n->zero_width_tprint(out);
      out->down(rem);
      while (--i > 0) {
	n->zero_width_tprint(out);
	out->down(h);
      }
      if (out->is_on())
	n->tprint(out);
    }
  }
}

void zero_width_node::tprint(troff_output_file *out)
{
  if (!n)
    return;
  if (!n->next) {
    n->zero_width_tprint(out);
    return;
  }
  int hpos = out->get_hpos();
  int vpos = out->get_vpos();
  node *tem = n;
  while (tem) {
    tem->tprint(out);
    tem = tem->next;
  }
  out->moveto(hpos, vpos);
}

void overstrike_node::tprint(troff_output_file *out)
{
  hunits pos = H0;
  for (node *tem = list; tem; tem = tem->next) {
    hunits x = (max_width - tem->width())/2;
    out->right(x - pos);
    pos = x;
    tem->zero_width_tprint(out);
  }
  out->right(max_width - pos);
}

void bracket_node::tprint(troff_output_file *out)
{
  if (list == 0)
    return;
  int npieces = 0;
  node *tem;
  for (tem = list; tem; tem = tem->next)
    ++npieces;
  vunits h = list->size();
  vunits totalh = h*npieces;
  vunits y = (totalh - h)/2;
  out->down(y);
  for (tem = list; tem; tem = tem->next) {
    tem->zero_width_tprint(out);
    out->down(-h);
  }
  out->right(max_width);
  out->down(totalh - y);
}

void node::tprint(troff_output_file *)
{
}

void node::zero_width_tprint(troff_output_file *out)
{
  int hpos = out->get_hpos();
  int vpos = out->get_vpos();
  tprint(out);
  out->moveto(hpos, vpos);
}

void space_node::tprint(troff_output_file *out)
{
  out->fill_color(col);
  out->right(n);
}

void hmotion_node::tprint(troff_output_file *out)
{
  out->fill_color(col);
  out->right(n);
}

void space_char_hmotion_node::tprint(troff_output_file *out)
{
  out->fill_color(col);
  if (is_html) {
    // we emit the space width as a negative glyph index
    out->flush_tbuf();
    out->do_motion();
    out->put('N');
    out->put(-n.to_units());
    out->put('\n');
  }
  out->right(n);
}

void vmotion_node::tprint(troff_output_file *out)
{
  out->fill_color(col);
  out->down(n);
}

void kern_pair_node::tprint(troff_output_file *out)
{
  n1->tprint(out);
  out->right(amount);
  n2->tprint(out);
}

static void tprint_reverse_node_list(troff_output_file *out, node *n)
{
  if (n == 0)
    return;
  tprint_reverse_node_list(out, n->next);
  n->tprint(out);
}

void dbreak_node::tprint(troff_output_file *out)
{
  tprint_reverse_node_list(out, none);
}

void composite_node::tprint(troff_output_file *out)
{
  hunits bold_offset;
  int is_bold = tf->get_bold(&bold_offset);
  hunits track_kern = tf->get_track_kern();
  hunits constant_space;
  int is_constant_spaced = tf->get_constant_space(&constant_space);
  hunits x = H0;
  if (is_constant_spaced) {
    x = constant_space;
    for (node *tem = n; tem; tem = tem->next)
      x -= tem->width();
    if (is_bold)
      x -= bold_offset;
    hunits x2 = x/2;
    out->right(x2);
    x -= x2;
  }
  if (is_bold) {
    int hpos = out->get_hpos();
    int vpos = out->get_vpos();
    tprint_reverse_node_list(out, n);
    out->moveto(hpos, vpos);
    out->right(bold_offset);
  }
  tprint_reverse_node_list(out, n);
  if (is_constant_spaced)
    out->right(x);
  else
    out->right(track_kern);
}

node *make_composite_node(charinfo *s, environment *env)
{
  int fontno = env_definite_font(env);
  if (fontno < 0) {
    error("no current font");
    return 0;
  }
  assert(fontno < font_table_size && font_table[fontno] != 0);
  node *n = charinfo_to_node_list(s, env);
  font_size fs = env->get_font_size();
  int char_height = env->get_char_height();
  int char_slant = env->get_char_slant();
  tfont *tf = font_table[fontno]->get_tfont(fs, char_height, char_slant,
					    fontno);
  if (env->is_composite())
    tf = tf->get_plain();
  return new composite_node(n, s, tf, 0, 0, 0);
}

node *make_glyph_node(charinfo *s, environment *env, int no_error_message = 0)
{
  int fontno = env_definite_font(env);
  if (fontno < 0) {
    error("no current font");
    return 0;
  }
  assert(fontno < font_table_size && font_table[fontno] != 0);
  int fn = fontno;
  int found = font_table[fontno]->contains(s);
  if (!found) {
    macro *mac = s->get_macro();
    if (mac && s->is_fallback())
      return make_composite_node(s, env);
    if (s->numbered()) {
      if (!no_error_message)
	warning(WARN_CHAR, "can't find numbered character %1",
		s->get_number());
      return 0;
    }
    special_font_list *sf = font_table[fontno]->sf;
    while (sf != 0 && !found) {
      fn = sf->n;
      if (font_table[fn])
	found = font_table[fn]->contains(s);
      sf = sf->next;
    }
    if (!found) {
      symbol f = font_table[fontno]->get_name();
      string gl(f.contents());
      gl += ' ';
      gl += s->nm.contents();
      gl += '\0';
      charinfo *ci = get_charinfo(symbol(gl.contents()));
      if (ci && ci->get_macro())
	return make_composite_node(ci, env);
    }
    if (!found) {
      sf = global_special_fonts;
      while (sf != 0 && !found) {
	fn = sf->n;
	if (font_table[fn])
	  found = font_table[fn]->contains(s);
	sf = sf->next;
      }
    }
    if (!found)
      if (mac && s->is_special())
	return make_composite_node(s, env);
    if (!found) {
      for (fn = 0; fn < font_table_size; fn++)
	if (font_table[fn]
	    && font_table[fn]->is_special()
	    && font_table[fn]->contains(s)) {
	  found = 1;
	  break;
	}
    }
    if (!found) {
      if (!no_error_message && s->first_time_not_found()) {
	unsigned char input_code = s->get_ascii_code();
	if (input_code != 0) {
	  if (csgraph(input_code))
	    warning(WARN_CHAR, "can't find character `%1'", input_code);
	  else
	    warning(WARN_CHAR, "can't find character with input code %1",
		    int(input_code));
	}
	else if (s->nm.contents())
	  warning(WARN_CHAR, "can't find special character `%1'",
		  s->nm.contents());
      }
      return 0;
    }
  }
  font_size fs = env->get_font_size();
  int char_height = env->get_char_height();
  int char_slant = env->get_char_slant();
  tfont *tf = font_table[fontno]->get_tfont(fs, char_height, char_slant, fn);
  if (env->is_composite())
    tf = tf->get_plain();
  color *gcol = env->get_glyph_color();
  color *fcol = env->get_fill_color();
  return new glyph_node(s, tf, gcol, fcol, 0, 0);
}

node *make_node(charinfo *ci, environment *env)
{
  switch (ci->get_special_translation()) {
  case charinfo::TRANSLATE_SPACE:
    return new space_char_hmotion_node(env->get_space_width(),
				       env->get_fill_color());
  case charinfo::TRANSLATE_STRETCHABLE_SPACE:
    return new unbreakable_space_node(env->get_space_width(),
				      env->get_fill_color());
  case charinfo::TRANSLATE_DUMMY:
    return new dummy_node;
  case charinfo::TRANSLATE_HYPHEN_INDICATOR:
    error("translation to \\% ignored in this context");
    break;
  }
  charinfo *tem = ci->get_translation();
  if (tem)
    ci = tem;
  macro *mac = ci->get_macro();
  if (mac && ci->is_normal())
    return make_composite_node(ci, env);
  else
    return make_glyph_node(ci, env);
}

int character_exists(charinfo *ci, environment *env)
{
  if (ci->get_special_translation() != charinfo::TRANSLATE_NONE)
    return 1;
  charinfo *tem = ci->get_translation();
  if (tem)
    ci = tem;
  if (ci->get_macro())
    return 1;
  node *nd = make_glyph_node(ci, env, 1);
  if (nd) {
    delete nd;
    return 1;
  }
  return 0;
}

node *node::add_char(charinfo *ci, environment *env,
		     hunits *widthp, int *spacep, node **glyph_comp_np)
{
  node *res;
  switch (ci->get_special_translation()) {
  case charinfo::TRANSLATE_SPACE:
    res = new space_char_hmotion_node(env->get_space_width(),
				      env->get_fill_color(), this);
    *widthp += res->width();
    return res;
  case charinfo::TRANSLATE_STRETCHABLE_SPACE:
    res = new unbreakable_space_node(env->get_space_width(),
				     env->get_fill_color(), this);
    res->freeze_space();
    *widthp += res->width();
    *spacep += res->nspaces();
    return res;
  case charinfo::TRANSLATE_DUMMY:
    return new dummy_node(this);
  case charinfo::TRANSLATE_HYPHEN_INDICATOR:
    return add_discretionary_hyphen();
  }
  charinfo *tem = ci->get_translation();
  if (tem)
    ci = tem;
  macro *mac = ci->get_macro();
  if (mac && ci->is_normal()) {
    res = make_composite_node(ci, env);
    if (res) {
      res->next = this;
      *widthp += res->width();
      if (glyph_comp_np)
	*glyph_comp_np = res;
    }
    else {
      if (glyph_comp_np)
	*glyph_comp_np = res;
      return this;
    }
  }
  else {
    node *gn = make_glyph_node(ci, env);
    if (gn == 0)
      return this;
    else {
      hunits old_width = width();
      node *p = gn->merge_self(this);
      if (p == 0) {
	*widthp += gn->width();
	gn->next = this;
	res = gn;
      }
      else {
	*widthp += p->width() - old_width;
	res = p;
      }
      if (glyph_comp_np)
	*glyph_comp_np = res;
    }
  }
  int break_code = 0;
  if (ci->can_break_before())
    break_code = 1;
  if (ci->can_break_after())
    break_code |= 2;
  if (break_code) {
    node *next1 = res->next;
    res->next = 0;
    res = new break_char_node(res, break_code, env->get_fill_color(), next1);
  }
  return res;
}

#ifdef __GNUG__
inline
#endif
int same_node(node *n1, node *n2)
{
  if (n1 != 0) {
    if (n2 != 0)
      return n1->type() == n2->type() && n1->same(n2);
    else
      return 0;
  }
  else
    return n2 == 0;
}

int same_node_list(node *n1, node *n2)
{
  while (n1 && n2) {
    if (n1->type() != n2->type() || !n1->same(n2))
      return 0;
    n1 = n1->next;
    n2 = n2->next;
  }
  return !n1 && !n2;
}

int extra_size_node::same(node *nd)
{
  return n == ((extra_size_node *)nd)->n;
}

const char *extra_size_node::type()
{
  return "extra_size_node";
}

int extra_size_node::force_tprint()
{
  return 0;
}

int extra_size_node::is_tag()
{
  return 0;
}

int vertical_size_node::same(node *nd)
{
  return n == ((vertical_size_node *)nd)->n;
}

const char *vertical_size_node::type()
{
  return "vertical_size_node";
}

int vertical_size_node::set_unformat_flag()
{
  return 0;
}

int vertical_size_node::force_tprint()
{
  return 0;
}

int vertical_size_node::is_tag()
{
  return 0;
}

int hmotion_node::same(node *nd)
{
  return n == ((hmotion_node *)nd)->n
	 && col == ((hmotion_node *)nd)->col;
}

const char *hmotion_node::type()
{
  return "hmotion_node";
}

int hmotion_node::set_unformat_flag()
{
  unformat = 1;
  return 1;
}

int hmotion_node::force_tprint()
{
  return 0;
}

int hmotion_node::is_tag()
{
  return 0;
}

node *hmotion_node::add_self(node *nd, hyphen_list **p)
{
  next = nd;
  hyphen_list *pp = *p;
  *p = (*p)->next;
  delete pp;
  return this;
}

hyphen_list *hmotion_node::get_hyphen_list(hyphen_list *tail, int *)
{
  return new hyphen_list(0, tail);
}

int space_char_hmotion_node::same(node *nd)
{
  return n == ((space_char_hmotion_node *)nd)->n
	 && col == ((space_char_hmotion_node *)nd)->col;
}

const char *space_char_hmotion_node::type()
{
  return "space_char_hmotion_node";
}

int space_char_hmotion_node::force_tprint()
{
  return 0;
}

int space_char_hmotion_node::is_tag()
{
  return 0;
}

node *space_char_hmotion_node::add_self(node *nd, hyphen_list **p)
{
  next = nd;
  hyphen_list *pp = *p;
  *p = (*p)->next;
  delete pp;
  return this;
}

hyphen_list *space_char_hmotion_node::get_hyphen_list(hyphen_list *tail,
						      int *)
{
  return new hyphen_list(0, tail);
}

int vmotion_node::same(node *nd)
{
  return n == ((vmotion_node *)nd)->n
	 && col == ((vmotion_node *)nd)->col;
}

const char *vmotion_node::type()
{
  return "vmotion_node";
}

int vmotion_node::force_tprint()
{
  return 0;
}

int vmotion_node::is_tag()
{
  return 0;
}

int hline_node::same(node *nd)
{
  return x == ((hline_node *)nd)->x && same_node(n, ((hline_node *)nd)->n);
}

const char *hline_node::type()
{
  return "hline_node";
}

int hline_node::force_tprint()
{
  return 0;
}

int hline_node::is_tag()
{
  return 0;
}

int vline_node::same(node *nd)
{
  return x == ((vline_node *)nd)->x && same_node(n, ((vline_node *)nd)->n);
}

const char *vline_node::type()
{
  return "vline_node";
}

int vline_node::force_tprint()
{
  return 0;
}

int vline_node::is_tag()
{
  return 0;
}

int dummy_node::same(node * /*nd*/)
{
  return 1;
}

const char *dummy_node::type()
{
  return "dummy_node";
}

int dummy_node::force_tprint()
{
  return 0;
}

int dummy_node::is_tag()
{
  return 0;
}

int transparent_dummy_node::same(node * /*nd*/)
{
  return 1;
}

const char *transparent_dummy_node::type()
{
  return "transparent_dummy_node";
}

int transparent_dummy_node::force_tprint()
{
  return 0;
}

int transparent_dummy_node::is_tag()
{
  return 0;
}

int transparent_dummy_node::ends_sentence()
{
  return 2;
}

int zero_width_node::same(node *nd)
{
  return same_node_list(n, ((zero_width_node *)nd)->n);
}

const char *zero_width_node::type()
{
  return "zero_width_node";
}

int zero_width_node::force_tprint()
{
  return 0;
}

int zero_width_node::is_tag()
{
  return 0;
}

int italic_corrected_node::same(node *nd)
{
  return (x == ((italic_corrected_node *)nd)->x
	  && same_node(n, ((italic_corrected_node *)nd)->n));
}

const char *italic_corrected_node::type()
{
  return "italic_corrected_node";
}

int italic_corrected_node::force_tprint()
{
  return 0;
}

int italic_corrected_node::is_tag()
{
  return 0;
}

left_italic_corrected_node::left_italic_corrected_node(node *xx)
: node(xx), n(0)
{
}

left_italic_corrected_node::left_italic_corrected_node(statem *s, int pop,
						       node *xx)
: node(xx, s, pop), n(0)
{
}

left_italic_corrected_node::~left_italic_corrected_node()
{
  delete n;
}

node *left_italic_corrected_node::merge_glyph_node(glyph_node *gn)
{
  if (n == 0) {
    hunits lic = gn->left_italic_correction();
    if (!lic.is_zero()) {
      x = lic;
      n = gn;
      return this;
    }
  }
  else {
    node *nd = n->merge_glyph_node(gn);
    if (nd) {
      n = nd;
      x = n->left_italic_correction();
      return this;
    }
  }
  return 0;
}

node *left_italic_corrected_node::copy()
{
  left_italic_corrected_node *nd =
    new left_italic_corrected_node(state, div_nest_level);
  if (n) {
    nd->n = n->copy();
    nd->x = x;
  }
  return nd;
}

void left_italic_corrected_node::tprint(troff_output_file *out)
{
  if (n) {
    out->right(x);
    n->tprint(out);
  }
}

const char *left_italic_corrected_node::type()
{
  return "left_italic_corrected_node";
}

int left_italic_corrected_node::force_tprint()
{
  return 0;
}

int left_italic_corrected_node::is_tag()
{
  return 0;
}

int left_italic_corrected_node::same(node *nd)
{
  return (x == ((left_italic_corrected_node *)nd)->x
	  && same_node(n, ((left_italic_corrected_node *)nd)->n));
}

void left_italic_corrected_node::ascii_print(ascii_output_file *out)
{
  if (n)
    n->ascii_print(out);
}

hunits left_italic_corrected_node::width()
{
  return n ? n->width() + x : H0;
}

void left_italic_corrected_node::vertical_extent(vunits *minimum,
						 vunits *maximum)
{
  if (n)
    n->vertical_extent(minimum, maximum);
  else
    node::vertical_extent(minimum, maximum);
}

hunits left_italic_corrected_node::skew()
{
  return n ? n->skew() + x/2 : H0;
}

hunits left_italic_corrected_node::subscript_correction()
{
  return n ? n->subscript_correction() : H0;
}

hunits left_italic_corrected_node::italic_correction()
{
  return n ? n->italic_correction() : H0;
}

int left_italic_corrected_node::ends_sentence()
{
  return n ? n->ends_sentence() : 0;
}

int left_italic_corrected_node::overlaps_horizontally()
{
  return n ? n->overlaps_horizontally() : 0;
}

int left_italic_corrected_node::overlaps_vertically()
{
  return n ? n->overlaps_vertically() : 0;
}

node *left_italic_corrected_node::last_char_node()
{
  return n ? n->last_char_node() : 0;
}

tfont *left_italic_corrected_node::get_tfont()
{
  return n ? n->get_tfont() : 0;
}

hyphenation_type left_italic_corrected_node::get_hyphenation_type()
{
  if (n)
    return n->get_hyphenation_type();
  else
    return HYPHEN_MIDDLE;
}

hyphen_list *left_italic_corrected_node::get_hyphen_list(hyphen_list *tail,
							 int *count)
{
  return n ? n->get_hyphen_list(tail, count) : tail;
}

node *left_italic_corrected_node::add_self(node *nd, hyphen_list **p)
{
  if (n) {
    nd = new left_italic_corrected_node(state, div_nest_level, nd);
    nd = n->add_self(nd, p);
    n = 0;
    delete this;
  }
  return nd;
}

int left_italic_corrected_node::character_type()
{
  return n ? n->character_type() : 0;
}

int overstrike_node::same(node *nd)
{
  return same_node_list(list, ((overstrike_node *)nd)->list);
}

const char *overstrike_node::type()
{
  return "overstrike_node";
}

int overstrike_node::force_tprint()
{
  return 0;
}

int overstrike_node::is_tag()
{
  return 0;
}

node *overstrike_node::add_self(node *n, hyphen_list **p)
{
  next = n;
  hyphen_list *pp = *p;
  *p = (*p)->next;
  delete pp;
  return this;
}

hyphen_list *overstrike_node::get_hyphen_list(hyphen_list *tail, int *)
{
  return new hyphen_list(0, tail);
}

int bracket_node::same(node *nd)
{
  return same_node_list(list, ((bracket_node *)nd)->list);
}

const char *bracket_node::type()
{
  return "bracket_node";
}

int bracket_node::force_tprint()
{
  return 0;
}

int bracket_node::is_tag()
{
  return 0;
}

int composite_node::same(node *nd)
{
  return ci == ((composite_node *)nd)->ci
    && same_node_list(n, ((composite_node *)nd)->n);
}

const char *composite_node::type()
{
  return "composite_node";
}

int composite_node::force_tprint()
{
  return 0;
}

int composite_node::is_tag()
{
  return 0;
}

int glyph_node::same(node *nd)
{
  return ci == ((glyph_node *)nd)->ci
	 && tf == ((glyph_node *)nd)->tf
	 && gcol == ((glyph_node *)nd)->gcol
	 && fcol == ((glyph_node *)nd)->fcol;
}

const char *glyph_node::type()
{
  return "glyph_node";
}

int glyph_node::force_tprint()
{
  return 0;
}

int glyph_node::is_tag()
{
  return 0;
}

int ligature_node::same(node *nd)
{
  return (same_node(n1, ((ligature_node *)nd)->n1)
	  && same_node(n2, ((ligature_node *)nd)->n2)
	  && glyph_node::same(nd));
}

const char *ligature_node::type()
{
  return "ligature_node";
}

int ligature_node::force_tprint()
{
  return 0;
}

int ligature_node::is_tag()
{
  return 0;
}

int kern_pair_node::same(node *nd)
{
  return (amount == ((kern_pair_node *)nd)->amount
	  && same_node(n1, ((kern_pair_node *)nd)->n1)
	  && same_node(n2, ((kern_pair_node *)nd)->n2));
}

const char *kern_pair_node::type()
{
  return "kern_pair_node";
}

int kern_pair_node::force_tprint()
{
  return 0;
}

int kern_pair_node::is_tag()
{
  return 0;
}

int dbreak_node::same(node *nd)
{
  return (same_node_list(none, ((dbreak_node *)nd)->none)
	  && same_node_list(pre, ((dbreak_node *)nd)->pre)
	  && same_node_list(post, ((dbreak_node *)nd)->post));
}

const char *dbreak_node::type()
{
  return "dbreak_node";
}

int dbreak_node::force_tprint()
{
  return 0;
}

int dbreak_node::is_tag()
{
  return 0;
}

int break_char_node::same(node *nd)
{
  return break_code == ((break_char_node *)nd)->break_code
	 && col == ((break_char_node *)nd)->col
	 && same_node(ch, ((break_char_node *)nd)->ch);
}

const char *break_char_node::type()
{
  return "break_char_node";
}

int break_char_node::force_tprint()
{
  return 0;
}

int break_char_node::is_tag()
{
  return 0;
}

int line_start_node::same(node * /*nd*/)
{
  return 1;
}

const char *line_start_node::type()
{
  return "line_start_node";
}

int line_start_node::force_tprint()
{
  return 0;
}

int line_start_node::is_tag()
{
  return 0;
}

int space_node::same(node *nd)
{
  return n == ((space_node *)nd)->n
	      && set == ((space_node *)nd)->set
	      && col == ((space_node *)nd)->col;
}

const char *space_node::type()
{
  return "space_node";
}

int word_space_node::same(node *nd)
{
  return n == ((word_space_node *)nd)->n
	 && set == ((word_space_node *)nd)->set
	 && col == ((word_space_node *)nd)->col;
}

const char *word_space_node::type()
{
  return "word_space_node";
}

int word_space_node::force_tprint()
{
  return 0;
}

int word_space_node::is_tag()
{
  return 0;
}

void unbreakable_space_node::tprint(troff_output_file *out)
{
  out->fill_color(col);
  if (is_html) {
    // we emit the space width as a negative glyph index
    out->flush_tbuf();
    out->do_motion();
    out->put('N');
    out->put(-n.to_units());
    out->put('\n');
  }
  out->right(n);
}

int unbreakable_space_node::same(node *nd)
{
  return n == ((unbreakable_space_node *)nd)->n
	 && set == ((unbreakable_space_node *)nd)->set
	 && col == ((unbreakable_space_node *)nd)->col;
}

const char *unbreakable_space_node::type()
{
  return "unbreakable_space_node";
}

node *unbreakable_space_node::add_self(node *nd, hyphen_list **p)
{
  next = nd;
  hyphen_list *pp = *p;
  *p = (*p)->next;
  delete pp;
  return this;
}

hyphen_list *unbreakable_space_node::get_hyphen_list(hyphen_list *tail, int *)
{
  return new hyphen_list(0, tail);
}

int diverted_space_node::same(node *nd)
{
  return n == ((diverted_space_node *)nd)->n;
}

const char *diverted_space_node::type()
{
  return "diverted_space_node";
}

int diverted_space_node::force_tprint()
{
  return 0;
}

int diverted_space_node::is_tag()
{
  return 0;
}

int diverted_copy_file_node::same(node *nd)
{
  return filename == ((diverted_copy_file_node *)nd)->filename;
}

const char *diverted_copy_file_node::type()
{
  return "diverted_copy_file_node";
}

int diverted_copy_file_node::force_tprint()
{
  return 0;
}

int diverted_copy_file_node::is_tag()
{
  return 0;
}

// Grow the font_table so that its size is > n.

static void grow_font_table(int n)
{
  assert(n >= font_table_size);
  font_info **old_font_table = font_table;
  int old_font_table_size = font_table_size;
  font_table_size = font_table_size ? (font_table_size*3)/2 : 10;
  if (font_table_size <= n)
    font_table_size = n + 10;
  font_table = new font_info *[font_table_size];
  if (old_font_table_size)
    memcpy(font_table, old_font_table,
	   old_font_table_size*sizeof(font_info *));
  a_delete old_font_table;
  for (int i = old_font_table_size; i < font_table_size; i++)
    font_table[i] = 0;
}

dictionary font_translation_dictionary(17);

static symbol get_font_translation(symbol nm)
{
  void *p = font_translation_dictionary.lookup(nm);
  return p ? symbol((char *)p) : nm;
}

dictionary font_dictionary(50);

static int mount_font_no_translate(int n, symbol name, symbol external_name,
				   int check_only = 0)
{
  assert(n >= 0);
  // We store the address of this char in font_dictionary to indicate
  // that we've previously tried to mount the font and failed.
  static char a_char;
  font *fm = 0;
  void *p = font_dictionary.lookup(external_name);
  if (p == 0) {
    int not_found;
    fm = font::load_font(external_name.contents(), &not_found, check_only);
    if (check_only)
      return fm != 0;
    if (!fm) {
      if (not_found)
	warning(WARN_FONT, "can't find font `%1'", external_name.contents());
      (void)font_dictionary.lookup(external_name, &a_char);
      return 0;
    }
    (void)font_dictionary.lookup(name, fm);
  }
  else if (p == &a_char) {
#if 0
    error("invalid font `%1'", external_name.contents());
#endif
    return 0;
  }
  else
    fm = (font*)p;
  if (check_only)
    return 1;
  if (n >= font_table_size) {
    if (n - font_table_size > 1000) {
      error("font position too much larger than first unused position");
      return 0;
    }
    grow_font_table(n);
  }
  else if (font_table[n] != 0)
    delete font_table[n];
  font_table[n] = new font_info(name, n, external_name, fm);
  font_family::invalidate_fontno(n);
  return 1;
}

int mount_font(int n, symbol name, symbol external_name)
{
  assert(n >= 0);
  name = get_font_translation(name);
  if (external_name.is_null())
    external_name = name;
  else
    external_name = get_font_translation(external_name);
  return mount_font_no_translate(n, name, external_name);
}

int check_font(symbol fam, symbol name)
{
  if (check_style(name))
    name = concat(fam, name);
  return mount_font_no_translate(0, name, name, 1);
}

int check_style(symbol s)
{
  int i = symbol_fontno(s);
  return i < 0 ? 0 : font_table[i]->is_style();
}

void mount_style(int n, symbol name)
{
  assert(n >= 0);
  if (n >= font_table_size) {
    if (n - font_table_size > 1000) {
      error("font position too much larger than first unused position");
      return;
    }
    grow_font_table(n);
  }
  else if (font_table[n] != 0)
    delete font_table[n];
  font_table[n] = new font_info(get_font_translation(name), n, NULL_SYMBOL, 0);
  font_family::invalidate_fontno(n);
}

/* global functions */

void font_translate()
{
  symbol from = get_name(1);
  if (!from.is_null()) {
    symbol to = get_name();
    if (to.is_null() || from == to)
      font_translation_dictionary.remove(from);
    else
      (void)font_translation_dictionary.lookup(from, (void *)to.contents());
  }
  skip_line();
}

void font_position()
{
  int n;
  if (get_integer(&n)) {
    if (n < 0)
      error("negative font position");
    else {
      symbol internal_name = get_name(1);
      if (!internal_name.is_null()) {
	symbol external_name = get_long_name();
	mount_font(n, internal_name, external_name); // ignore error
      }
    }
  }
  skip_line();
}

font_family::font_family(symbol s)
: map_size(10), nm(s)
{
  map = new int[map_size];
  for (int i = 0; i < map_size; i++)
    map[i] = -1;
}

font_family::~font_family()
{
  a_delete map;
}

int font_family::make_definite(int i)
{
  if (i >= 0) {
    if (i < map_size && map[i] >= 0)
      return map[i];
    else {
      if (i < font_table_size && font_table[i] != 0) {
	if (i >= map_size) {
	  int old_map_size = map_size;
	  int *old_map = map;
	  map_size *= 3;
	  map_size /= 2;
	  if (i >= map_size)
	    map_size = i + 10;
	  map = new int[map_size];
	  memcpy(map, old_map, old_map_size*sizeof(int));
	  a_delete old_map;
	  for (int j = old_map_size; j < map_size; j++)
	    map[j] = -1;
	}
	if (font_table[i]->is_style()) {
	  symbol sty = font_table[i]->get_name();
	  symbol f = concat(nm, sty);
	  int n;
	  // don't use symbol_fontno, because that might return a style
	  // and because we don't want to translate the name
	  for (n = 0; n < font_table_size; n++)
	    if (font_table[n] != 0 && font_table[n]->is_named(f)
		&& !font_table[n]->is_style())
	      break;
	  if (n >= font_table_size) {
	    n = next_available_font_position();
	    if (!mount_font_no_translate(n, f, f))
	      return -1;
	  }
	  return map[i] = n;
	}
	else
	  return map[i] = i;
      }
      else
	return -1;
    }
  }
  else
    return -1;
}

dictionary family_dictionary(5);

font_family *lookup_family(symbol nm)
{
  font_family *f = (font_family *)family_dictionary.lookup(nm);
  if (!f) {
    f = new font_family(nm);
    (void)family_dictionary.lookup(nm, f);
  }
  return f;
}

void font_family::invalidate_fontno(int n)
{
  assert(n >= 0 && n < font_table_size);
  dictionary_iterator iter(family_dictionary);
  symbol nam;
  font_family *fam;
  while (iter.get(&nam, (void **)&fam)) {
    int mapsize = fam->map_size;
    if (n < mapsize)
      fam->map[n] = -1;
    for (int i = 0; i < mapsize; i++)
      if (fam->map[i] == n)
	fam->map[i] = -1;
  }
}

void style()
{
  int n;
  if (get_integer(&n)) {
    if (n < 0)
      error("negative font position");
    else {
      symbol internal_name = get_name(1);
      if (!internal_name.is_null())
	mount_style(n, internal_name);
    }
  }
  skip_line();
}

static int get_fontno()
{
  int n;
  tok.skip();
  if (tok.delimiter()) {
    symbol s = get_name(1);
    if (!s.is_null()) {
      n = symbol_fontno(s);
      if (n < 0) {
	n = next_available_font_position();
	if (!mount_font(n, s))
	  return -1;
      }
      return curenv->get_family()->make_definite(n);
    }
  }
  else if (get_integer(&n)) {
    if (n < 0 || n >= font_table_size || font_table[n] == 0)
      error("bad font number");
    else
      return curenv->get_family()->make_definite(n);
  }
  return -1;
}

static int underline_fontno = 2;

void underline_font()
{
  int n = get_fontno();
  if (n >= 0)
    underline_fontno = n;
  skip_line();
}

int get_underline_fontno()
{
  return underline_fontno;
}

void define_font_special_character()
{
  int n = get_fontno();
  if (n < 0) {
    skip_line();
    return;
  }
  symbol f = font_table[n]->get_name();
  do_define_character(CHAR_FONT_SPECIAL, f.contents());
}

void remove_font_special_character()
{
  int n = get_fontno();
  if (n < 0) {
    skip_line();
    return;
  }
  symbol f = font_table[n]->get_name();
  while (!tok.newline() && !tok.eof()) {
    if (!tok.space() && !tok.tab()) {
      charinfo *s = tok.get_char(1);
      string gl(f.contents());
      gl += ' ';
      gl += s->nm.contents();
      gl += '\0';
      charinfo *ci = get_charinfo(symbol(gl.contents()));
      if (!ci)
	break;
      macro *m = ci->set_macro(0);
      if (m)
	delete m;
    }
    tok.next();
  }
  skip_line();
}

static void read_special_fonts(special_font_list **sp)
{
  special_font_list *s = *sp;
  *sp = 0;
  while (s != 0) {
    special_font_list *tem = s;
    s = s->next;
    delete tem;
  }
  special_font_list **p = sp;
  while (has_arg()) {
    int i = get_fontno();
    if (i >= 0) {
      special_font_list *tem = new special_font_list;
      tem->n = i;
      tem->next = 0;
      *p = tem;
      p = &(tem->next);
    }
  }
}

void font_special_request()
{
  int n = get_fontno();
  if (n >= 0)
    read_special_fonts(&font_table[n]->sf);
  skip_line();
}

void special_request()
{
  read_special_fonts(&global_special_fonts);
  skip_line();
}

int next_available_font_position()
{
  int i;
  for (i = 1; i < font_table_size && font_table[i] != 0; i++)
    ;
  return i;
}

int symbol_fontno(symbol s)
{
  s = get_font_translation(s);
  for (int i = 0; i < font_table_size; i++)
    if (font_table[i] != 0 && font_table[i]->is_named(s))
      return i;
  return -1;
}

int is_good_fontno(int n)
{
  return n >= 0 && n < font_table_size && font_table[n] != 0;
}

int get_bold_fontno(int n)
{
  if (n >= 0 && n < font_table_size && font_table[n] != 0) {
    hunits offset;
    if (font_table[n]->get_bold(&offset))
      return offset.to_units() + 1;
    else
      return 0;
  }
  else
    return 0;
}

hunits env_digit_width(environment *env)
{
  node *n = make_glyph_node(charset_table['0'], env);
  if (n) {
    hunits x = n->width();
    delete n;
    return x;
  }
  else
    return H0;
}

hunits env_space_width(environment *env)
{
  int fn = env_definite_font(env);
  font_size fs = env->get_font_size();
  if (fn < 0 || fn >= font_table_size || font_table[fn] == 0)
    return scale(fs.to_units()/3, env->get_space_size(), 12);
  else
    return font_table[fn]->get_space_width(fs, env->get_space_size());
}

hunits env_sentence_space_width(environment *env)
{
  int fn = env_definite_font(env);
  font_size fs = env->get_font_size();
  if (fn < 0 || fn >= font_table_size || font_table[fn] == 0)
    return scale(fs.to_units()/3, env->get_sentence_space_size(), 12);
  else
    return font_table[fn]->get_space_width(fs, env->get_sentence_space_size());
}

hunits env_half_narrow_space_width(environment *env)
{
  int fn = env_definite_font(env);
  font_size fs = env->get_font_size();
  if (fn < 0 || fn >= font_table_size || font_table[fn] == 0)
    return 0;
  else
    return font_table[fn]->get_half_narrow_space_width(fs);
}

hunits env_narrow_space_width(environment *env)
{
  int fn = env_definite_font(env);
  font_size fs = env->get_font_size();
  if (fn < 0 || fn >= font_table_size || font_table[fn] == 0)
    return 0;
  else
    return font_table[fn]->get_narrow_space_width(fs);
}

void bold_font()
{
  int n = get_fontno();
  if (n >= 0) {
    if (has_arg()) {
      if (tok.delimiter()) {
	int f = get_fontno();
	if (f >= 0) {
	  units offset;
	  if (has_arg() && get_number(&offset, 'u') && offset >= 1)
	    font_table[f]->set_conditional_bold(n, hunits(offset - 1));
	  else
	    font_table[f]->conditional_unbold(n);
	}
      }
      else {
	units offset;
	if (get_number(&offset, 'u') && offset >= 1)
	  font_table[n]->set_bold(hunits(offset - 1));
	else
	  font_table[n]->unbold();
      }
    }
    else
      font_table[n]->unbold();
  }
  skip_line();
}

track_kerning_function::track_kerning_function() : non_zero(0)
{
}

track_kerning_function::track_kerning_function(int min_s, hunits min_a,
					       int max_s, hunits max_a)
: non_zero(1), min_size(min_s), min_amount(min_a), max_size(max_s),
  max_amount(max_a)
{
}

int track_kerning_function::operator==(const track_kerning_function &tk)
{
  if (non_zero)
    return (tk.non_zero
	    && min_size == tk.min_size
	    && min_amount == tk.min_amount
	    && max_size == tk.max_size
	    && max_amount == tk.max_amount);
  else
    return !tk.non_zero;
}

int track_kerning_function::operator!=(const track_kerning_function &tk)
{
  if (non_zero)
    return (!tk.non_zero
	    || min_size != tk.min_size
	    || min_amount != tk.min_amount
	    || max_size != tk.max_size
	    || max_amount != tk.max_amount);
  else
    return tk.non_zero;
}

hunits track_kerning_function::compute(int size)
{
  if (non_zero) {
    if (max_size <= min_size)
      return min_amount;
    else if (size <= min_size)
      return min_amount;
    else if (size >= max_size)
      return max_amount;
    else
      return (scale(max_amount, size - min_size, max_size - min_size)
	      + scale(min_amount, max_size - size, max_size - min_size));
  }
  else
    return H0;
}

void track_kern()
{
  int n = get_fontno();
  if (n >= 0) {
    int min_s, max_s;
    hunits min_a, max_a;
    if (has_arg()
	&& get_number(&min_s, 'z')
	&& get_hunits(&min_a, 'p')
	&& get_number(&max_s, 'z')
	&& get_hunits(&max_a, 'p')) {
      track_kerning_function tk(min_s, min_a, max_s, max_a);
      font_table[n]->set_track_kern(tk);
    }
    else {
      track_kerning_function tk;
      font_table[n]->set_track_kern(tk);
    }
  }
  skip_line();
}

void constant_space()
{
  int n = get_fontno();
  if (n >= 0) {
    int x, y;
    if (!has_arg() || !get_integer(&x))
      font_table[n]->set_constant_space(CONSTANT_SPACE_NONE);
    else {
      if (!has_arg() || !get_number(&y, 'z'))
	font_table[n]->set_constant_space(CONSTANT_SPACE_RELATIVE, x);
      else
	font_table[n]->set_constant_space(CONSTANT_SPACE_ABSOLUTE,
					  scale(y*x,
						units_per_inch,
						36*72*sizescale));
    }
  }
  skip_line();
}

void ligature()
{
  int lig;
  if (has_arg() && get_integer(&lig) && lig >= 0 && lig <= 2)
    global_ligature_mode = lig;
  else
    global_ligature_mode = 1;
  skip_line();
}

void kern_request()
{
  int k;
  if (has_arg() && get_integer(&k))
    global_kern_mode = k != 0;
  else
    global_kern_mode = 1;
  skip_line();
}

void set_soft_hyphen_char()
{
  soft_hyphen_char = get_optional_char();
  if (!soft_hyphen_char)
    soft_hyphen_char = get_charinfo(HYPHEN_SYMBOL);
  skip_line();
}

void init_output()
{
  if (suppress_output_flag)
    the_output = new suppress_output_file;
  else if (ascii_output_flag)
    the_output = new ascii_output_file;
  else
    the_output = new troff_output_file;
}

class next_available_font_position_reg : public reg {
public:
  const char *get_string();
};

const char *next_available_font_position_reg::get_string()
{
  return i_to_a(next_available_font_position());
}

class printing_reg : public reg {
public:
  const char *get_string();
};

const char *printing_reg::get_string()
{
  if (the_output)
    return the_output->is_printing() ? "1" : "0";
  else
    return "0";
}

void init_node_requests()
{
  init_request("bd", bold_font);
  init_request("cs", constant_space);
  init_request("fp", font_position);
  init_request("fschar", define_font_special_character);
  init_request("fspecial", font_special_request);
  init_request("ftr", font_translate);
  init_request("kern", kern_request);
  init_request("lg", ligature);
  init_request("rfschar", remove_font_special_character);
  init_request("shc", set_soft_hyphen_char);
  init_request("special", special_request);
  init_request("sty", style);
  init_request("tkf", track_kern);
  init_request("uf", underline_font);
  number_reg_dictionary.define(".fp", new next_available_font_position_reg);
  number_reg_dictionary.define(".kern",
			       new constant_int_reg(&global_kern_mode));
  number_reg_dictionary.define(".lg",
			       new constant_int_reg(&global_ligature_mode));
  number_reg_dictionary.define(".P", new printing_reg);
  soft_hyphen_char = get_charinfo(HYPHEN_SYMBOL);
}
