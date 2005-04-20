// -*- C++ -*-
/* Copyright (C) 1989-2000, 2001, 2002, 2003 Free Software Foundation, Inc.
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

#include "driver.h"
#include "device.h"
#include "ptable.h"

declare_ptable(char)
implement_ptable(char)

extern "C" const char *Version_string;

#define putstring(s) fputs(s, stdout)

#ifndef SHRT_MIN
#define SHRT_MIN (-32768)
#endif

#ifndef SHRT_MAX
#define SHRT_MAX 32767
#endif

#define TAB_WIDTH 8

static int horizontal_tab_flag = 0;
static int form_feed_flag = 0;
static int bold_flag = 1;
static int underline_flag = 1;
static int overstrike_flag = 1;
static int draw_flag = 1;
static int italic_flag = 0;
static int reverse_flag = 0;
static int old_drawing_scheme = 0;

enum {
  UNDERLINE_MODE = 0x01,
  BOLD_MODE = 0x02,
  VDRAW_MODE = 0x04,
  HDRAW_MODE = 0x08,
  CU_MODE = 0x10,
  COLOR_CHANGE = 0x20
};

// Mode to use for bold-underlining.
static unsigned char bold_underline_mode = BOLD_MODE|UNDERLINE_MODE;

#ifndef IS_EBCDIC_HOST
#define CSI "\033["
#else
#define CSI "\047["
#endif

// SGR handling (ISO 6429)
#define SGR_BOLD CSI "1m"
#define SGR_NO_BOLD CSI "22m"
#define SGR_ITALIC CSI "3m"
#define SGR_NO_ITALIC CSI "23m"
#define SGR_UNDERLINE CSI "4m"
#define SGR_NO_UNDERLINE CSI "24m"
#define SGR_REVERSE CSI "7m"
#define SGR_NO_REVERSE CSI "27m"
// many terminals can't handle `CSI 39 m' and `CSI 49 m' to reset
// the foreground and bachground color, respectively; thus we use
// `CSI 0 m' exclusively
#define SGR_DEFAULT CSI "0m"

#define DEFAULT_COLOR_IDX -1

class tty_font : public font {
  tty_font(const char *);
  unsigned char mode;
public:
  ~tty_font();
  unsigned char get_mode() { return mode; }
#if 0
  void handle_x_command(int argc, const char **argv);
#endif
  static tty_font *load_tty_font(const char *);
};

tty_font *tty_font::load_tty_font(const char *s)
{
  tty_font *f = new tty_font(s);
  if (!f->load()) {
    delete f;
    return 0;
  }
  const char *num = f->get_internal_name();
  long n;
  if (num != 0 && (n = strtol(num, 0, 0)) != 0)
    f->mode = int(n & (BOLD_MODE|UNDERLINE_MODE));
  if (!underline_flag)
    f->mode &= ~UNDERLINE_MODE;
  if (!bold_flag)
    f->mode &= ~BOLD_MODE;
  if ((f->mode & (BOLD_MODE|UNDERLINE_MODE)) == (BOLD_MODE|UNDERLINE_MODE))
    f->mode = (f->mode & ~(BOLD_MODE|UNDERLINE_MODE)) | bold_underline_mode;
  return f;
}

tty_font::tty_font(const char *nm)
: font(nm), mode(0)
{
}

tty_font::~tty_font()
{
}

#if 0
void tty_font::handle_x_command(int argc, const char **argv)
{
  if (argc >= 1 && strcmp(argv[0], "bold") == 0)
    mode |= BOLD_MODE;
  else if (argc >= 1 && strcmp(argv[0], "underline") == 0)
    mode |= UNDERLINE_MODE;
}
#endif

class glyph {
  static glyph *free_list;
public:
  glyph *next;
  short hpos;
  unsigned int code;
  unsigned char mode;
  char back_color_idx;
  char fore_color_idx;
  void *operator new(size_t);
  void operator delete(void *);
  inline int draw_mode() { return mode & (VDRAW_MODE|HDRAW_MODE); }
  inline int order() {
    return mode & (VDRAW_MODE|HDRAW_MODE|CU_MODE|COLOR_CHANGE); }
};

glyph *glyph::free_list = 0;

void *glyph::operator new(size_t)
{
  if (!free_list) {
    const int BLOCK = 1024;
    free_list = (glyph *)new char[sizeof(glyph) * BLOCK];
    for (int i = 0; i < BLOCK - 1; i++)
      free_list[i].next = free_list + i + 1;
    free_list[BLOCK - 1].next = 0;
  }
  glyph *p = free_list;
  free_list = free_list->next;
  p->next = 0;
  return p;
}

void glyph::operator delete(void *p)
{
  if (p) {
    ((glyph *)p)->next = free_list;
    free_list = (glyph *)p;
  }
}

class tty_printer : public printer {
  int is_utf8;
  glyph **lines;
  int nlines;
  int cached_v;
  int cached_vpos;
  char curr_fore_idx;
  char curr_back_idx;
  int is_underline;
  int is_bold;
  int cu_flag;
  PTABLE(char) tty_colors;
  void make_underline();
  void make_bold(unsigned int);
  char color_to_idx(color *col);
  void add_char(unsigned int, int, int, color *, color *, unsigned char);
  char *make_rgb_string(unsigned int, unsigned int, unsigned int);
  int tty_color(unsigned int, unsigned int, unsigned int, char *,
		char = DEFAULT_COLOR_IDX);
public:
  tty_printer(const char *device);
  ~tty_printer();
  void set_char(int, font *, const environment *, int, const char *name);
  void draw(int code, int *p, int np, const environment *env);
  void special(char *arg, const environment *env, char type);
  void change_color(const environment * const env);
  void change_fill_color(const environment * const env);
  void put_char(unsigned int);
  void put_color(char, int);
  void begin_page(int) { }
  void end_page(int page_length);
  font *make_font(const char *);
};

char *tty_printer::make_rgb_string(unsigned int r,
				   unsigned int g,
				   unsigned int b)
{
  char *s = new char[8];
  s[0] = char(r >> 8);
  s[1] = char(r & 0xff);
  s[2] = char(g >> 8);
  s[3] = char(g & 0xff);
  s[4] = char(b >> 8);
  s[5] = char(b & 0xff);
  s[6] = char(0x80);
  s[7] = 0;
  // avoid null-bytes in string
  for (int i = 0; i < 6; i++)
    if (!s[i]) {
      s[i] = 1;
      s[6] |= 1 << i;
    }
  return s;
}

int tty_printer::tty_color(unsigned int r,
			   unsigned int g,
			   unsigned int b, char *idx, char value)
{
  int unknown_color = 0;
  char *s = make_rgb_string(r, g, b);
  char *i = tty_colors.lookup(s);
  if (!i) {
    unknown_color = 1;
    i = new char[1];
    *i = value;
    tty_colors.define(s, i);
  }
  *idx = *i;
  a_delete s;
  return unknown_color;
}

tty_printer::tty_printer(const char *device) : cached_v(0)
{
  is_utf8 = !strcmp(device, "utf8");
  char dummy;
  // black, white
  (void)tty_color(0, 0, 0, &dummy, 0);
  (void)tty_color(color::MAX_COLOR_VAL,
		  color::MAX_COLOR_VAL,
		  color::MAX_COLOR_VAL, &dummy, 7);
  // red, green, blue
  (void)tty_color(color::MAX_COLOR_VAL, 0, 0, &dummy, 1);
  (void)tty_color(0, color::MAX_COLOR_VAL, 0, &dummy, 2);
  (void)tty_color(0, 0, color::MAX_COLOR_VAL, &dummy, 4);
  // yellow, magenta, cyan
  (void)tty_color(color::MAX_COLOR_VAL, color::MAX_COLOR_VAL, 0, &dummy, 3);
  (void)tty_color(color::MAX_COLOR_VAL, 0, color::MAX_COLOR_VAL, &dummy, 5);
  (void)tty_color(0, color::MAX_COLOR_VAL, color::MAX_COLOR_VAL, &dummy, 6);
  nlines = 66;
  lines = new glyph *[nlines];
  for (int i = 0; i < nlines; i++)
    lines[i] = 0;
  cu_flag = 0;
}

tty_printer::~tty_printer()
{
  a_delete lines;
}

void tty_printer::make_underline()
{
  if (old_drawing_scheme) {
    putchar('_');
    putchar('\b');
  }
  else {
    if (!is_underline) {
      if (italic_flag)
	putstring(SGR_ITALIC);
      else if (reverse_flag)
	putstring(SGR_REVERSE);
      else
	putstring(SGR_UNDERLINE);
    }
    is_underline = 1;
  }
}

void tty_printer::make_bold(unsigned int c)
{
  if (old_drawing_scheme) {
    put_char(c);
    putchar('\b');
  }
  else {
    if (!is_bold)
      putstring(SGR_BOLD);
    is_bold = 1;
  }
}

char tty_printer::color_to_idx(color *col)
{
  if (col->is_default())
    return DEFAULT_COLOR_IDX;
  unsigned int r, g, b;
  col->get_rgb(&r, &g, &b);
  char idx;
  if (tty_color(r, g, b, &idx)) {
    char *s = col->print_color();
    error("Unknown color (%1) mapped to default", s);
    a_delete s;
  }
  return idx;
}

void tty_printer::set_char(int i, font *f, const environment *env,
			   int w, const char *)
{
  if (w != font::hor)
    fatal("width of character not equal to horizontal resolution");
  add_char(f->get_code(i),
	   env->hpos, env->vpos,
	   env->col, env->fill,
	   ((tty_font *)f)->get_mode());
}

void tty_printer::add_char(unsigned int c,
			   int h, int v,
			   color *fore, color *back,
			   unsigned char mode)
{
#if 0
  // This is too expensive.
  if (h % font::hor != 0)
    fatal("horizontal position not a multiple of horizontal resolution");
#endif
  int hpos = h / font::hor;
  if (hpos < SHRT_MIN || hpos > SHRT_MAX) {
    error("character with ridiculous horizontal position discarded");
    return;
  }
  int vpos;
  if (v == cached_v && cached_v != 0)
    vpos = cached_vpos;
  else {
    if (v % font::vert != 0)
      fatal("vertical position not a multiple of vertical resolution");
    vpos = v / font::vert;
    if (vpos > nlines) {
      glyph **old_lines = lines;
      lines = new glyph *[vpos + 1];
      memcpy(lines, old_lines, nlines * sizeof(glyph *));
      for (int i = nlines; i <= vpos; i++)
	lines[i] = 0;
      a_delete old_lines;
      nlines = vpos + 1;
    }
    // Note that the first output line corresponds to groff
    // position font::vert.
    if (vpos <= 0) {
      error("character above first line discarded");
      return;
    }
    cached_v = v;
    cached_vpos = vpos;
  }
  glyph *g = new glyph;
  g->hpos = hpos;
  g->code = c;
  g->fore_color_idx = color_to_idx(fore);
  g->back_color_idx = color_to_idx(back);
  g->mode = mode;

  // The list will be reversed later.  After reversal, it must be in
  // increasing order of hpos, with COLOR_CHANGE and CU specials before
  // HDRAW characters before VDRAW characters before normal characters
  // at each hpos, and otherwise in order of occurrence.

  glyph **pp;
  for (pp = lines + (vpos - 1); *pp; pp = &(*pp)->next)
    if ((*pp)->hpos < hpos
	|| ((*pp)->hpos == hpos && (*pp)->order() >= g->order()))
      break;
  g->next = *pp;
  *pp = g;
}

void tty_printer::special(char *arg, const environment *env, char type)
{
  if (type == 'u') {
    add_char(*arg - '0', env->hpos, env->vpos, env->col, env->fill, CU_MODE);
    return;
  }
  if (type != 'p')
    return;
  char *p;
  for (p = arg; *p == ' ' || *p == '\n'; p++)
    ;
  char *tag = p;
  for (; *p != '\0' && *p != ':' && *p != ' ' && *p != '\n'; p++)
    ;
  if (*p == '\0' || strncmp(tag, "tty", p - tag) != 0) {
    error("X command without `tty:' tag ignored");
    return;
  }
  p++;
  for (; *p == ' ' || *p == '\n'; p++)
    ;
  char *command = p;
  for (; *p != '\0' && *p != ' ' && *p != '\n'; p++)
    ;
  if (*command == '\0') {
    error("empty X command ignored");
    return;
  }
  if (strncmp(command, "sgr", p - command) == 0) {
    for (; *p == ' ' || *p == '\n'; p++)
      ;
    int n;
    if (*p != '\0' && sscanf(p, "%d", &n) == 1 && n == 0)
      old_drawing_scheme = 1;
    else
      old_drawing_scheme = 0;
  }
}

void tty_printer::change_color(const environment * const env)
{
  add_char(0, env->hpos, env->vpos, env->col, env->fill, COLOR_CHANGE);
}

void tty_printer::change_fill_color(const environment * const env)
{
  add_char(0, env->hpos, env->vpos, env->col, env->fill, COLOR_CHANGE);
}

void tty_printer::draw(int code, int *p, int np, const environment *env)
{
  if (code != 'l' || !draw_flag)
    return;
  if (np != 2) {
    error("2 arguments required for line");
    return;
  }
  if (p[0] == 0) {
    // vertical line
    int v = env->vpos;
    int len = p[1];
    if (len < 0) {
      v += len;
      len = -len;
    }
    while (len >= 0) {
      add_char('|', env->hpos, v, env->col, env->fill, VDRAW_MODE);
      len -= font::vert;
      v += font::vert;
    }
  }
  if (p[1] == 0) {
    // horizontal line
    int h = env->hpos;
    int len = p[0];
    if (len < 0) {
      h += len;
      len = -len;
    }
    while (len >= 0) {
      add_char('-', h, env->vpos, env->col, env->fill, HDRAW_MODE);
      len -= font::hor;
      h += font::hor;
    }
  }
}

void tty_printer::put_char(unsigned int wc)
{
  if (is_utf8 && wc >= 0x80) {
    char buf[6 + 1];
    int count;
    char *p = buf;
    if (wc < 0x800)
      count = 1, *p = (unsigned char)((wc >> 6) | 0xc0);
    else if (wc < 0x10000)
      count = 2, *p = (unsigned char)((wc >> 12) | 0xe0);
    else if (wc < 0x200000)
      count = 3, *p = (unsigned char)((wc >> 18) | 0xf0);
    else if (wc < 0x4000000)
      count = 4, *p = (unsigned char)((wc >> 24) | 0xf8);
    else if (wc <= 0x7fffffff)
      count = 5, *p = (unsigned char)((wc >> 30) | 0xfC);
    else
      return;
    do *++p = (unsigned char)(((wc >> (6 * --count)) & 0x3f) | 0x80);
      while (count > 0);
    *++p = '\0';
    putstring(buf);
  }
  else
    putchar(wc);
}

void tty_printer::put_color(char color_index, int back)
{
  if (color_index == DEFAULT_COLOR_IDX) {
    putstring(SGR_DEFAULT);
    // set bold and underline again
    if (is_bold)
      putstring(SGR_BOLD);
    if (is_underline) {
      if (italic_flag)
	putstring(SGR_ITALIC);
      else if (reverse_flag)
	putstring(SGR_REVERSE);
      else
	putstring(SGR_UNDERLINE);
    }
    // set other color again
    back = !back;
    color_index = back ? curr_back_idx : curr_fore_idx;
  }
  if (color_index != DEFAULT_COLOR_IDX) {
    putstring(CSI);
    if (back)
      putchar('4');
    else
      putchar('3');
    putchar(color_index + '0');
    putchar('m');
  }
}

void tty_printer::end_page(int page_length)
{
  if (page_length % font::vert != 0)
    error("vertical position at end of page not multiple of vertical resolution");
  int lines_per_page = page_length / font::vert;
  int last_line;
  for (last_line = nlines; last_line > 0; last_line--)
    if (lines[last_line - 1])
      break;
#if 0
  if (last_line > lines_per_page) {
    error("characters past last line discarded");
    do {
      --last_line;
      while (lines[last_line]) {
	glyph *tem = lines[last_line];
	lines[last_line] = tem->next;
	delete tem;
      }
    } while (last_line > lines_per_page);
  }
#endif
  for (int i = 0; i < last_line; i++) {
    glyph *p = lines[i];
    lines[i] = 0;
    glyph *g = 0;
    while (p) {
      glyph *tem = p->next;
      p->next = g;
      g = p;
      p = tem;
    }
    int hpos = 0;
    glyph *nextp;
    curr_fore_idx = DEFAULT_COLOR_IDX;
    curr_back_idx = DEFAULT_COLOR_IDX;
    is_underline = 0;
    is_bold = 0;
    for (p = g; p; delete p, p = nextp) {
      nextp = p->next;
      if (p->mode & CU_MODE) {
	cu_flag = p->code;
	continue;
      }
      if (nextp && p->hpos == nextp->hpos) {
	if (p->draw_mode() == HDRAW_MODE &&
	    nextp->draw_mode() == VDRAW_MODE) {
	  nextp->code = '+';
	  continue;
	}
	if (p->draw_mode() != 0 && p->draw_mode() == nextp->draw_mode()) {
	  nextp->code = p->code;
	  continue;
	}
	if (!overstrike_flag)
	  continue;
      }
      if (hpos > p->hpos) {
	do {
	  putchar('\b');
	  hpos--;
	} while (hpos > p->hpos);
      }
      else {
	if (horizontal_tab_flag) {
	  for (;;) {
	    int next_tab_pos = ((hpos + TAB_WIDTH) / TAB_WIDTH) * TAB_WIDTH;
	    if (next_tab_pos > p->hpos)
	      break;
	    if (cu_flag)
	      make_underline();
	    else if (!old_drawing_scheme && is_underline) {
	      if (italic_flag)
		putstring(SGR_NO_ITALIC);
	      else if (reverse_flag)
		putstring(SGR_NO_REVERSE);
	      else
		putstring(SGR_NO_UNDERLINE);
	      is_underline = 0;
	    }
	    putchar('\t');
	    hpos = next_tab_pos;
	  }
	}
	for (; hpos < p->hpos; hpos++) {
	  if (cu_flag)
	    make_underline();
	  else if (!old_drawing_scheme && is_underline) {
	    if (italic_flag)
	      putstring(SGR_NO_ITALIC);
	    else if (reverse_flag)
	      putstring(SGR_NO_REVERSE);
	    else
	      putstring(SGR_NO_UNDERLINE);
	    is_underline = 0;
	  }
	  putchar(' ');
	}
      }
      assert(hpos == p->hpos);
      if (p->mode & COLOR_CHANGE) {
	if (!old_drawing_scheme) {
	  if (p->fore_color_idx != curr_fore_idx) {
	    put_color(p->fore_color_idx, 0);
	    curr_fore_idx = p->fore_color_idx;
	  }
	  if (p->back_color_idx != curr_back_idx) {
	    put_color(p->back_color_idx, 1);
	    curr_back_idx = p->back_color_idx;
	  }
	}
	continue;
      }
      if (p->mode & UNDERLINE_MODE)
	make_underline();
      else if (!old_drawing_scheme && is_underline) {
	if (italic_flag)
	  putstring(SGR_NO_ITALIC);
	else if (reverse_flag)
	  putstring(SGR_NO_REVERSE);
	else
	  putstring(SGR_NO_UNDERLINE);
	is_underline = 0;
      }
      if (p->mode & BOLD_MODE)
	make_bold(p->code);
      else if (!old_drawing_scheme && is_bold) {
	putstring(SGR_NO_BOLD);
	is_bold = 0;
      }
      if (!old_drawing_scheme) {
	if (p->fore_color_idx != curr_fore_idx) {
	  put_color(p->fore_color_idx, 0);
	  curr_fore_idx = p->fore_color_idx;
	}
	if (p->back_color_idx != curr_back_idx) {
	  put_color(p->back_color_idx, 1);
	  curr_back_idx = p->back_color_idx;
	}
      }
      put_char(p->code);
      hpos++;
    }
    if (!old_drawing_scheme
	&& (is_bold || is_underline
	    || curr_fore_idx != DEFAULT_COLOR_IDX
	    || curr_back_idx != DEFAULT_COLOR_IDX))
      putstring(SGR_DEFAULT);
    putchar('\n');
  }
  if (form_feed_flag) {
    if (last_line < lines_per_page)
      putchar('\f');
  }
  else {
    for (; last_line < lines_per_page; last_line++)
      putchar('\n');
  }
}

font *tty_printer::make_font(const char *nm)
{
  return tty_font::load_tty_font(nm);
}

printer *make_printer()
{
  return new tty_printer(device);
}

static void usage(FILE *stream);

int main(int argc, char **argv)
{
  program_name = argv[0];
  static char stderr_buf[BUFSIZ];
  if (getenv("GROFF_NO_SGR"))
    old_drawing_scheme = 1;
  setbuf(stderr, stderr_buf);
  int c;
  static const struct option long_options[] = {
    { "help", no_argument, 0, CHAR_MAX + 1 },
    { "version", no_argument, 0, 'v' },
    { NULL, 0, 0, 0 }
  };
  while ((c = getopt_long(argc, argv, "bBcdfF:hioruUv", long_options, NULL))
	 != EOF)
    switch(c) {
    case 'v':
      printf("GNU grotty (groff) version %s\n", Version_string);
      exit(0);
      break;
    case 'i':
      // Use italic font instead of underlining.
      italic_flag = 1;
      break;
    case 'b':
      // Do not embolden by overstriking.
      bold_flag = 0;
      break;
    case 'c':
      // Use old scheme for emboldening and underline.
      old_drawing_scheme = 1;
      break;
    case 'u':
      // Do not underline.
      underline_flag = 0;
      break;
    case 'o':
      // Do not overstrike (other than emboldening and underlining).
      overstrike_flag = 0;
      break;
    case 'r':
      // Use reverse mode instead of underlining.
      reverse_flag = 1;
      break;
    case 'B':
      // Do bold-underlining as bold.
      bold_underline_mode = BOLD_MODE;
      break;
    case 'U':
      // Do bold-underlining as underlining.
      bold_underline_mode = UNDERLINE_MODE;
      break;
    case 'h':
      // Use horizontal tabs.
      horizontal_tab_flag = 1;
      break;
    case 'f':
      form_feed_flag = 1;
      break;
    case 'F':
      font::command_line_font_dir(optarg);
      break;
    case 'd':
      // Ignore \D commands.
      draw_flag = 0;
      break;
    case CHAR_MAX + 1: // --help
      usage(stdout);
      exit(0);
      break;
    case '?':
      usage(stderr);
      exit(1);
      break;
    default:
      assert(0);
    }
  if (old_drawing_scheme) {
    italic_flag = 0;
    reverse_flag = 0;
  }
  else {
    bold_underline_mode = BOLD_MODE|UNDERLINE_MODE;
    bold_flag = 1;
    underline_flag = 1;
  }
  if (optind >= argc)
    do_file("-");
  else {
    for (int i = optind; i < argc; i++)
      do_file(argv[i]);
  }
  return 0;
}

static void usage(FILE *stream)
{
  fprintf(stream, "usage: %s [-bBcdfhioruUv] [-F dir] [files ...]\n",
	  program_name);
}
