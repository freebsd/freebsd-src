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

#include "driver.h"
#include "nonposix.h"

extern "C" const char *Version_string;

#define DEFAULT_LINEWIDTH 40
static int linewidth = DEFAULT_LINEWIDTH;

static int draw_flag = 1;

/* These values were chosen because:

(MULTIPLIER*SIZESCALE)/(RES*UNITWIDTH) == 1/(2^20 * 72.27)

and 57816 is an exact multiple of both 72.27*SIZESCALE and 72.

The width in the groff font file is the product of MULTIPLIER and the
width in the tfm file. */

#define RES 57816
#define RES_7227 (RES/7227)
#define UNITWIDTH 131072
#define SIZESCALE 100
#define MULTIPLIER 1

#define FILL_MAX 1000

class dvi_font : public font {
  dvi_font(const char *);
public:
  int checksum;
  int design_size;
  ~dvi_font();
  void handle_unknown_font_command(const char *command, const char *arg,
				   const char *filename, int lineno);
  static dvi_font *load_dvi_font(const char *);
};

dvi_font *dvi_font::load_dvi_font(const char *s)
{
  dvi_font *f = new dvi_font(s);
  if (!f->load()) {
    delete f;
    return 0;
  }
  return f;
}

dvi_font::dvi_font(const char *nm)
: font(nm), checksum(0), design_size(0)
{
}

dvi_font::~dvi_font()
{
}

void dvi_font::handle_unknown_font_command(const char *command,
					   const char *arg,
					   const char *filename, int lineno)
{
  char *ptr;
  if (strcmp(command, "checksum") == 0) {
    if (arg == 0)
      fatal_with_file_and_line(filename, lineno,
			       "`checksum' command requires an argument");
    checksum = int(strtol(arg, &ptr, 10));
    if (checksum == 0 && ptr == arg) {
      fatal_with_file_and_line(filename, lineno, "bad checksum");
    }
  }
  else if (strcmp(command, "designsize") == 0) {
    if (arg == 0)
      fatal_with_file_and_line(filename, lineno,
			       "`designsize' command requires an argument");
    design_size = int(strtol(arg, &ptr, 10));
    if (design_size == 0 && ptr == arg) {
      fatal_with_file_and_line(filename, lineno, "bad design size");
    }
  }
}

#define FONTS_MAX 256

struct output_font {
  dvi_font *f;
  int point_size;
  output_font() : f(0) { }
};

class dvi_printer : public printer {
  FILE *fp;
  int max_drift;
  int byte_count;
  int last_bop;
  int page_count;
  int cur_h;
  int cur_v;
  int end_h;
  int max_h;
  int max_v;
  output_font output_font_table[FONTS_MAX];
  font *cur_font;
  int cur_point_size;
  int pushed;
  int pushed_h;
  int pushed_v;
  int have_pushed;
  void preamble();
  void postamble();
  void define_font(int);
  void set_font(int);
  void possibly_begin_line();
protected:
  enum {
    id_byte = 2,
    set1 = 128,
    put1 = 133,
    put_rule = 137,
    bop = 139,
    eop = 140,
    push = 141,
    pop = 142,
    right1 = 143,
    down1 = 157,
    fnt_num_0 = 171,
    fnt1 = 235,
    xxx1 = 239,
    fnt_def1 = 243,
    pre = 247,
    post = 248,
    post_post = 249,
    filler = 223
  };
  int line_thickness;

  void out1(int);
  void out2(int);
  void out3(int);
  void out4(int);
  void moveto(int, int);
  void out_string(const char *);
  void out_signed(unsigned char, int);
  void out_unsigned(unsigned char, int);
  void do_special(const char *);
public:
  dvi_printer();
  ~dvi_printer();
  font *make_font(const char *);
  void begin_page(int);
  void end_page(int);
  void set_char(int, font *, const environment *, int w, const char *name);
  void special(char *arg, const environment *env, char type);
  void end_of_line();
  void draw(int code, int *p, int np, const environment *env);
};


class draw_dvi_printer : public dvi_printer {
  int output_pen_size;
  int fill;
  void set_line_thickness(const environment *);
  void fill_next();
public:
  draw_dvi_printer();
  ~draw_dvi_printer();
  void draw(int code, int *p, int np, const environment *env);
  void end_page(int);
};

dvi_printer::dvi_printer()
: fp(stdout), byte_count(0), last_bop(-1), page_count(0), max_h(0), max_v(0),
  cur_font(0), cur_point_size(-1), pushed(0), line_thickness(-1)
{
  if (font::res != RES)
    fatal("resolution must be %1", RES);
  if (font::unitwidth != UNITWIDTH)
    fatal("unitwidth must be %1", UNITWIDTH);
  if (font::hor != 1)
    fatal("hor must be equal to 1");
  if (font::vert != 1)
    fatal("vert must be equal to 1");
  if (font::sizescale != SIZESCALE)
    fatal("sizescale must be equal to %1", SIZESCALE);
  max_drift = font::res/1000;	// this is fairly arbitrary
  preamble();
}

dvi_printer::~dvi_printer()
{
  postamble();
}


draw_dvi_printer::draw_dvi_printer()
: output_pen_size(-1), fill(FILL_MAX)
{
}

draw_dvi_printer::~draw_dvi_printer()
{
}


void dvi_printer::out1(int n)
{
  byte_count += 1;
  putc(n & 0xff, fp);
}

void dvi_printer::out2(int n)
{
  byte_count += 2;
  putc((n >> 8) & 0xff, fp);
  putc(n & 0xff, fp);
}

void dvi_printer::out3(int n)
{
  byte_count += 3;
  putc((n >> 16) & 0xff, fp);
  putc((n >> 8) & 0xff, fp);
  putc(n & 0xff, fp);
}

void dvi_printer::out4(int n)
{
  byte_count += 4;
  putc((n >> 24) & 0xff, fp);
  putc((n >> 16) & 0xff, fp);
  putc((n >> 8) & 0xff, fp);
  putc(n & 0xff, fp);
}

void dvi_printer::out_string(const char *s)
{
  out1(strlen(s));
  while (*s != 0)
    out1(*s++);
}


void dvi_printer::end_of_line()
{
  if (pushed) {
    out1(pop);
    pushed = 0;
    cur_h = pushed_h;
    cur_v = pushed_v;
  }
}

void dvi_printer::possibly_begin_line()
{
  if (!pushed) {
    have_pushed = pushed = 1;
    pushed_h = cur_h;
    pushed_v = cur_v;
    out1(push);
  }
}

int scale(int x, int z)
{
  int sw;
  int a, b, c, d;
  int alpha, beta;
  alpha = 16*z; beta = 16;
  while (z >= 040000000L) {
    z /= 2; beta /= 2;
  }
  d = x & 255;
  c = (x >> 8) & 255;
  b = (x >> 16) & 255;
  a = (x >> 24) & 255;
  sw = (((((d * z) / 0400) + (c * z)) / 0400) + (b * z)) / beta;
  if (a == 255)
    sw -= alpha;
  else
    assert(a == 0);
  return sw;
}


void dvi_printer::set_char(int index, font *f, const environment *env, int w, const char *name)
{
  int code = f->get_code(index);
  if (env->size != cur_point_size || f != cur_font) {
    cur_font = f;
    cur_point_size = env->size;
    int i;
    for (i = 0;; i++) {
      if (i >= FONTS_MAX) {
	fatal("too many output fonts required");
      }
      if (output_font_table[i].f == 0) {
	output_font_table[i].f = (dvi_font *)cur_font;
	output_font_table[i].point_size = cur_point_size;
	define_font(i);
      }
      if (output_font_table[i].f == cur_font
	  && output_font_table[i].point_size == cur_point_size)
	break;
    }
    set_font(i);
  }
  int distance = env->hpos - cur_h;
  if (env->hpos != end_h && distance != 0) {
    out_signed(right1, distance);
    cur_h = env->hpos;
  }
  else if (distance > max_drift) {
    out_signed(right1, distance - max_drift);
    cur_h = env->hpos - max_drift;
  }
  else if (distance < -max_drift) {
    out_signed(right1, distance + max_drift);
    cur_h = env->hpos + max_drift;
  }
  if (env->vpos != cur_v) {
    out_signed(down1, env->vpos - cur_v);
    cur_v = env->vpos;
  }
  possibly_begin_line();
  end_h = env->hpos + w;
  cur_h += scale(f->get_width(index, UNITWIDTH)/MULTIPLIER,
		cur_point_size*RES_7227);
  if (cur_h > max_h)
    max_h = cur_h;
  if (cur_v > max_v)
    max_v = cur_v;
  if (code >= 0 && code <= 127)
    out1(code);
  else
    out_unsigned(set1, code);
}

void dvi_printer::define_font(int i)
{
  out_unsigned(fnt_def1, i);
  dvi_font *f = output_font_table[i].f;
  out4(f->checksum);
  out4(output_font_table[i].point_size*RES_7227);
  out4(int((double(f->design_size)/(1<<20))*RES_7227*100 + .5));
  const char *nm = f->get_internal_name();
  out1(0);
  out_string(nm);
}

void dvi_printer::set_font(int i)
{
  if (i >= 0 && i <= 63)
    out1(fnt_num_0 + i);
  else
    out_unsigned(fnt1, i);
}

void dvi_printer::out_signed(unsigned char base, int param)
{
  if (-128 <= param && param < 128) {
    out1(base);
    out1(param);
  }
  else if (-32768 <= param && param < 32768) {
    out1(base+1);
    out2(param);
  }
  else if (-(1 << 23) <= param && param < (1 << 23)) {
    out1(base+2);
    out3(param);
  }
  else {
    out1(base+3);
    out4(param);
  }
}

void dvi_printer::out_unsigned(unsigned char base, int param)
{
  if (param >= 0) {
    if (param < 256) {
      out1(base);
      out1(param);
    }
    else if (param < 65536) {
      out1(base+1);
      out2(param);
    }
    else if (param < (1 << 24)) {
      out1(base+2);
      out3(param);
    }
    else {
      out1(base+3);
      out4(param);
    }
  }
  else {
    out1(base+3);
    out4(param);
  }
}

void dvi_printer::preamble()
{
  out1(pre);
  out1(id_byte);
  out4(254000);
  out4(font::res);
  out4(1000);
  out1(0);
}

void dvi_printer::postamble()
{
  int tem = byte_count;
  out1(post);
  out4(last_bop);
  out4(254000);
  out4(font::res);
  out4(1000);
  out4(max_v);
  out4(max_h);
  out2(have_pushed); // stack depth
  out2(page_count);
  int i;
  for (i = 0; i < FONTS_MAX && output_font_table[i].f != 0; i++)
    define_font(i);
  out1(post_post);
  out4(tem);
  out1(id_byte);
  for (i = 0; i < 4 || byte_count % 4 != 0; i++)
    out1(filler);
}  
  
void dvi_printer::begin_page(int i)
{
  page_count++;
  int tem = byte_count;
  out1(bop);
  out4(i);
  for (int j = 1; j < 10; j++)
    out4(0);
  out4(last_bop);
  last_bop = tem;
  // By convention position (0,0) in a dvi file is placed at (1in, 1in).
  cur_h = font::res;
  cur_v = font::res;
  end_h = 0;
}

void dvi_printer::end_page(int)
{
  if (pushed)
    end_of_line();
  out1(eop);
  cur_font = 0;
}

void draw_dvi_printer::end_page(int len)
{
  dvi_printer::end_page(len);
  output_pen_size = -1;
}

void dvi_printer::do_special(const char *s)
{
  int len = strlen(s);
  if (len == 0)
    return;
  possibly_begin_line();
  out_unsigned(xxx1, len);
  while (*s)
    out1(*s++);
}

void dvi_printer::special(char *arg, const environment *env, char type)
{
  if (type != 'p')
    return;
  moveto(env->hpos, env->vpos);
  do_special(arg);
}

void dvi_printer::moveto(int h, int v)
{
  if (h != cur_h) {
    out_signed(right1, h - cur_h);
    cur_h = h;
    if (cur_h > max_h)
      max_h = cur_h;
  }
  if (v != cur_v) {
    out_signed(down1, v - cur_v);
    cur_v = v;
    if (cur_v > max_v)
      max_v = cur_v;
  }
  end_h = 0;
}

void dvi_printer::draw(int code, int *p, int np, const environment *env)
{
  if (code == 'l') {
    int x = 0, y = 0;
    int height = 0, width = 0;
    int thickness;
    if (line_thickness < 0)
      thickness = env->size*RES_7227*linewidth/1000;
    else if (line_thickness > 0)
      thickness = line_thickness;
    else
      thickness = 1;
    if (np != 2) {
      error("2 arguments required for line");
    }
    else if (p[0] == 0) {
      // vertical rule
      if (p[1] > 0) {
	x = env->hpos - thickness/2;
	y = env->vpos + p[1] + thickness/2;
	height = p[1] + thickness;
	width = thickness;
      }
      else if (p[1] < 0) {
	x = env->hpos - thickness/2;
	y = env->vpos + thickness/2;
	height = thickness - p[1];
	width = thickness;
      }
    }
    else if (p[1] == 0) {
      if (p[0] > 0) {
	x = env->hpos - thickness/2;
	y = env->vpos + thickness/2;
	height = thickness;
	width = p[0] + thickness;
      }
      else if (p[0] < 0) {
	x = env->hpos - p[0] - thickness/2;
	y = env->vpos + thickness/2;
	height = thickness;
	width = thickness - p[0];
      }
    }
    if (height != 0) {
      moveto(x, y);
      out1(put_rule);
      out4(height);
      out4(width);
    }
  }
  else if (code == 't') {
    if (np == 0) {
      line_thickness = -1;
    }
    else {
      // troff gratuitously adds an extra 0
      if (np != 1 && np != 2)
	error("0 or 1 argument required for thickness");
      else
	line_thickness = p[0];
    }
  }
  else if (code == 'R') {
    if (np != 2)
      error("2 arguments required for rule");
    else if (p[0] != 0 || p[1] != 0) {
      int dh = p[0];
      int dv = p[1];
      int oh = env->hpos;
      int ov = env->vpos;
      if (dv > 0) {
	ov += dv;
	dv = -dv;
      }
      if (dh < 0) {
	oh += dh;
	dh = -dh;
      }
      moveto(oh, ov);
      out1(put_rule);
      out4(-dv);
      out4(dh);
    }
  }
}

// XXX Will this overflow?

inline int milliinches(int n)
{
  return (n*1000 + font::res/2)/font::res;
}

void draw_dvi_printer::set_line_thickness(const environment *env)
{
  int desired_pen_size
    = milliinches(line_thickness < 0
		  // Will this overflow?
		  ? env->size*RES_7227*linewidth/1000
		  : line_thickness);
  if (desired_pen_size != output_pen_size) {
    char buf[256];
    sprintf(buf, "pn %d", desired_pen_size);
    do_special(buf);
    output_pen_size = desired_pen_size;
  }
}

void draw_dvi_printer::fill_next()
{
  char buf[256];
  sprintf(buf, "sh %.3f", double(fill)/FILL_MAX);
  do_special(buf);
}

void draw_dvi_printer::draw(int code, int *p, int np, const environment *env)
{
  char buf[1024];
  int fill_flag = 0;
  switch (code) {
  case 'C':
    fill_flag = 1;
    // fall through
  case 'c':
    // troff adds an extra argument to C
    if (np != 1 && !(code == 'C' && np == 2)) {
      error("1 argument required for circle");
      break;
    }
    moveto(env->hpos+p[0]/2, env->vpos);
    if (fill_flag)
      fill_next();
    else
      set_line_thickness(env);
    int rad;
    rad = milliinches(p[0]/2);
    sprintf(buf, "%s 0 0 %d %d 0 6.28319",
	    (fill_flag ? "ia" : "ar"),
	    rad,
	    rad);
    do_special(buf);
    break;
  case 'l':
    if (np != 2) {
      error("2 arguments required for line");
      break;
    }
    moveto(env->hpos, env->vpos);
    set_line_thickness(env);
    do_special("pa 0 0");
    sprintf(buf, "pa %d %d", milliinches(p[0]), milliinches(p[1]));
    do_special(buf);
    do_special("fp");
    break;
  case 'E':
    fill_flag = 1;
    // fall through
  case 'e':
    if (np != 2) {
      error("2 arguments required for ellipse");
      break;
    }
    moveto(env->hpos+p[0]/2, env->vpos);
    if (fill_flag)
      fill_next();
    sprintf(buf, "%s 0 0 %d %d 0 6.28319",
	    (fill_flag ? "ia" : "ar"),
	    milliinches(p[0]/2),
	    milliinches(p[1]/2));
    do_special(buf);
    break;
  case 'P':
    fill_flag = 1;
    // fall through
  case 'p':
    {
      if (np & 1) {
	error("even number of arguments required for polygon");
	break;
      }
      if (np == 0) {
	error("no arguments for polygon");
	break;
      }
      moveto(env->hpos, env->vpos);
      if (fill_flag)
	fill_next();
      else
	set_line_thickness(env);
      do_special("pa 0 0");
      int h = 0, v = 0;
      for (int i = 0; i < np; i += 2) {
	h += p[i];
	v += p[i+1];
	sprintf(buf, "pa %d %d", milliinches(h), milliinches(v));
	do_special(buf);
      }
      do_special("pa 0 0");
      do_special(fill_flag ? "ip" : "fp");
      break;
    }
  case '~':
    {
      if (np & 1) {
	error("even number of arguments required for spline");
	break;
      }
      if (np == 0) {
	error("no arguments for spline");
	break;
      }
      moveto(env->hpos, env->vpos);
      set_line_thickness(env);
      do_special("pa 0 0");
      int h = 0, v = 0;
      for (int i = 0; i < np; i += 2) {
	h += p[i];
	v += p[i+1];
	sprintf(buf, "pa %d %d", milliinches(h), milliinches(v));
	do_special(buf);
      }
      do_special("sp");
      break;
    }
  case 'a':
    {
      if (np != 4) {
	error("4 arguments required for arc");
	break;
      }
      set_line_thickness(env);
      double c[2];
      if (adjust_arc_center(p, c)) {
	int rad = milliinches(int(sqrt(c[0]*c[0] + c[1]*c[1]) + .5));
	moveto(env->hpos + int(c[0]), env->vpos + int(c[1]));
	sprintf(buf, "ar 0 0 %d %d %f %f",
		rad,
		rad,
		atan2(p[1] + p[3] - c[1], p[0] + p[2] - c[0]),
		atan2(-c[1], -c[0]));
	do_special(buf);
      }
      else {
	moveto(env->hpos, env->vpos);
	do_special("pa 0 0");
	sprintf(buf,
		"pa %d %d",
		milliinches(p[0] + p[2]),
		milliinches(p[1] + p[3]));
	do_special(buf);
	do_special("fp");
      }
      break;
    }
  case 't':
    {
      if (np == 0) {
	line_thickness = -1;
      }
      else {
	// troff gratuitously adds an extra 0
	if (np != 1 && np != 2) {
	  error("0 or 1 argument required for thickness");
	  break;
	}
	line_thickness = p[0];
      }
      break;
    }
  case 'f':
    {
      if (np != 1 && np != 2) {
	error("1 argument required for fill");
	break;
      }
      fill = p[0];
      if (fill < 0 || fill > FILL_MAX)
	fill = FILL_MAX;
      break;
    }
  case 'R':
    {
      if (np != 2) {
	error("2 arguments required for rule");
	break;
      }
      int dh = p[0];
      if (dh == 0)
	break;
      int dv = p[1];
      if (dv == 0)
	break;
      int oh = env->hpos;
      int ov = env->vpos;
      if (dv > 0) {
	ov += dv;
	dv = -dv;
      }
      if (dh < 0) {
	oh += dh;
	dh = -dh;
      }
      moveto(oh, ov);
      out1(put_rule);
      out4(-dv);
      out4(dh);
      break;
    }
  default:
    error("unrecognised drawing command `%1'", char(code));
    break;
  }
}

font *dvi_printer::make_font(const char *nm)
{
  return dvi_font::load_dvi_font(nm);
}

printer *make_printer()
{
  if (draw_flag)
    return new draw_dvi_printer;
  else
    return new dvi_printer;
}

static void usage(FILE *stream);

int main(int argc, char **argv)
{
  program_name = argv[0];
  static char stderr_buf[BUFSIZ];
  setbuf(stderr, stderr_buf);
  int c;
  static const struct option long_options[] = {
    { "help", no_argument, 0, CHAR_MAX + 1 },
    { "version", no_argument, 0, 'v' },
    { NULL, 0, 0, 0 }
  };
  while ((c = getopt_long(argc, argv, "F:vw:d", long_options, NULL)) != EOF)
    switch(c) {
    case 'v':
      {
	printf("GNU grodvi (groff) version %s\n", Version_string);
	exit(0);
	break;
      }
    case 'w':
      if (sscanf(optarg, "%d", &linewidth) != 1
	  || linewidth < 0 || linewidth > 1000) {
	error("bad line width");
	linewidth = DEFAULT_LINEWIDTH;
      }
      break;
    case 'd':
      draw_flag = 0;
      break;
    case 'F':
      font::command_line_font_dir(optarg);
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
#ifdef SET_BINARY
  SET_BINARY(fileno(stdout));
#endif
  if (optind >= argc)
    do_file("-");
  else {
    for (int i = optind; i < argc; i++)
      do_file(argv[i]);
  }
  delete pr;
  return 0;
}

static void usage(FILE *stream)
{
  fprintf(stream, "usage: %s [-dv] [-F dir] [-w n] [files ...]\n",
	  program_name);
}
