// -*- C++ -*-
/* Copyright (C) 1994, 2000, 2001 Free Software Foundation, Inc.
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

/*
TODO

option to use beziers for circle/ellipse/arc
option to use lines for spline (for LJ3)
left/top offset registration
output bin selection option
paper source option
output non-integer parameters using fixed point numbers
X command to insert contents of file
X command to specify inline escape sequence (how to specify unprintable chars?)
X command to include bitmap graphics
*/

#include "driver.h"
#include "nonposix.h"

static struct {
  const char *name;
  int code;
  // at 300dpi
  int x_offset_portrait;
  int x_offset_landscape;
} paper_table[] = {
  { "letter", 2, 75, 60 },
  { "legal", 3, 75, 60 },
  { "executive", 1, 75, 60 },
  { "a4", 26, 71, 59 },
  { "com10", 81, 75, 60 },
  { "monarch", 80, 75, 60 },
  { "c5", 91, 71, 59 },
  { "b5", 100, 71, 59 },
  { "dl", 90, 71, 59 },
};

static int paper_size = -1;
static int landscape_flag = 0;
static int duplex_flag = 0;

// An upper limit on the paper size in centipoints,
// used for setting HPGL picture frame.
#define MAX_PAPER_WIDTH (12*720)
#define MAX_PAPER_HEIGHT (17*720)

// Dotted lines that are thinner than this don't work right.
#define MIN_DOT_PEN_WIDTH .351

#ifndef DEFAULT_LINE_WIDTH_FACTOR
// in ems/1000
#define DEFAULT_LINE_WIDTH_FACTOR 40
#endif

const int DEFAULT_HPGL_UNITS = 1016;
int line_width_factor = DEFAULT_LINE_WIDTH_FACTOR;
unsigned ncopies = 0;		// 0 means don't send ncopies command

class lj4_font : public font {
public:
  ~lj4_font();
  void handle_unknown_font_command(const char *command, const char *arg,
				   const char *filename, int lineno);
  static lj4_font *load_lj4_font(const char *);
  int weight;
  int style;
  int proportional;
  int typeface;
private:
  lj4_font(const char *);
};

lj4_font::lj4_font(const char *nm)
: font(nm), weight(0), style(0), proportional(0), typeface(0)
{
}

lj4_font::~lj4_font()
{
}

lj4_font *lj4_font::load_lj4_font(const char *s)
{
  lj4_font *f = new lj4_font(s);
  if (!f->load()) {
    delete f;
    return 0;
  }
  return f;
}

static struct {
  const char *s;
  int lj4_font::*ptr;
  int min;
  int max;
} command_table[] = {
  { "pclweight", &lj4_font::weight, -7, 7 },
  { "pclstyle", &lj4_font::style, 0, 32767 },
  { "pclproportional", &lj4_font::proportional, 0, 1 },
  { "pcltypeface", &lj4_font::typeface, 0, 65535 },
};

void lj4_font::handle_unknown_font_command(const char *command,
					   const char *arg,
					   const char *filename, int lineno)
{
  for (int i = 0; i < sizeof(command_table)/sizeof(command_table[0]); i++) {
    if (strcmp(command, command_table[i].s) == 0) {
      if (arg == 0)
	fatal_with_file_and_line(filename, lineno,
				 "`%1' command requires an argument",
				 command);
      char *ptr;
      long n = strtol(arg, &ptr, 10);
      if (n == 0 && ptr == arg)
	fatal_with_file_and_line(filename, lineno,
				 "`%1' command requires numeric argument",
				 command);
      if (n < command_table[i].min) {
	error_with_file_and_line(filename, lineno,
				 "argument for `%1' command must not be less than %2",
				 command, command_table[i].min);
	n = command_table[i].min;
      }
      else if (n > command_table[i].max) {
	error_with_file_and_line(filename, lineno,
				 "argument for `%1' command must not be greater than %2",
				 command, command_table[i].max);
	n = command_table[i].max;
      }
      this->*command_table[i].ptr = int(n);
      break;
    }
  }
}

class lj4_printer : public printer {
public:
  lj4_printer();
  ~lj4_printer();
  void set_char(int, font *, const environment *, int, const char *name);
  void draw(int code, int *p, int np, const environment *env);
  void begin_page(int);
  void end_page(int page_length);
  font *make_font(const char *);
  void end_of_line();
private:
  void set_line_thickness(int size, int dot = 0);
  void hpgl_init();
  void hpgl_start();
  void hpgl_end();
  int moveto(int hpos, int vpos);
  int moveto1(int hpos, int vpos);

  int cur_hpos;
  int cur_vpos;
  lj4_font *cur_font;
  int cur_size;
  unsigned short cur_symbol_set;
  int x_offset;
  int line_thickness;
  double pen_width;
  double hpgl_scale;
  int hpgl_inited;
};

inline
int lj4_printer::moveto(int hpos, int vpos)
{
  if (cur_hpos != hpos || cur_vpos != vpos || cur_hpos < 0)
    return moveto1(hpos, vpos);
  else
    return 1;
}

inline
void lj4_printer::hpgl_start()
{
  fputs("\033%1B", stdout);
}

inline
void lj4_printer::hpgl_end()
{
  fputs(";\033%0A", stdout);
}

lj4_printer::lj4_printer()
: cur_hpos(-1),
  cur_font(0),
  cur_size(0),
  cur_symbol_set(0),
  line_thickness(-1),
  pen_width(-1.0),
  hpgl_inited(0)
{
  if (7200 % font::res != 0)
    fatal("invalid resolution %1: resolution must be a factor of 7200",
	  font::res);
  fputs("\033E", stdout);		// reset
  if (font::res != 300)
    printf("\033&u%dD", font::res); // unit of measure
  if (ncopies > 0)
    printf("\033&l%uX", ncopies);
  if (paper_size < 0)
    paper_size = 0;		// default to letter
  printf("\033&l%dA"		// paper size
	 "\033&l%dO"		// orientation
	 "\033&l0E",		// no top margin
	 paper_table[paper_size].code,
	 landscape_flag != 0);
  if (landscape_flag)
    x_offset = paper_table[paper_size].x_offset_landscape;
  else
    x_offset = paper_table[paper_size].x_offset_portrait;
  x_offset = (x_offset * font::res) / 300;
  if (duplex_flag)
     printf("\033&l%dS", duplex_flag);
}

lj4_printer::~lj4_printer()
{
  fputs("\033E", stdout);
}

void lj4_printer::begin_page(int)
{
}

void lj4_printer::end_page(int)
{
  putchar('\f');
  cur_hpos = -1;
}

void lj4_printer::end_of_line()
{
  cur_hpos = -1;		// force absolute motion
}

inline
int is_unprintable(unsigned char c)
{
  return c < 32 && (c == 0 || (7 <= c && c <= 15) || c == 27);
}

void lj4_printer::set_char(int index, font *f, const environment *env, int w, const char *name)
{
  int code = f->get_code(index);

  unsigned char ch = code & 0xff;
  unsigned short symbol_set = code >> 8;
  if (symbol_set != cur_symbol_set) {
    printf("\033(%d%c", symbol_set/32, (symbol_set & 31) + 64);
    cur_symbol_set = symbol_set;
  }
  if (f != cur_font) {
    lj4_font *psf = (lj4_font *)f;
    // FIXME only output those that are needed
    printf("\033(s%dp%ds%db%dT",
	   psf->proportional,
	   psf->style,
	   psf->weight,
	   psf->typeface);
    if (!psf->proportional || !cur_font || !cur_font->proportional)
      cur_size = 0;
    cur_font = psf;
  }
  if (env->size != cur_size) {
    if (cur_font->proportional) {
      static const char *quarters[] = { "", ".25", ".5", ".75" };
      printf("\033(s%d%sV", env->size/4, quarters[env->size & 3]);
    }
    else {
      double pitch = double(font::res)/w;
      // PCL uses the next largest pitch, so round it down.
      pitch = floor(pitch*100.0)/100.0;
      printf("\033(s%.2fH", pitch);
    }
    cur_size = env->size;
  }
  if (!moveto(env->hpos, env->vpos))
    return;
  if (is_unprintable(ch))
    fputs("\033&p1X", stdout);
  putchar(ch);
  cur_hpos += w;
}

int lj4_printer::moveto1(int hpos, int vpos)
{
  if (hpos < x_offset || vpos < 0)
    return 0;
  fputs("\033*p", stdout);
  if (cur_hpos < 0)
    printf("%dx%dY", hpos - x_offset, vpos);
  else {
    if (cur_hpos != hpos)
      printf("%s%d%c", hpos > cur_hpos ? "+" : "",
	     hpos - cur_hpos, vpos == cur_vpos ? 'X' : 'x');
    if (cur_vpos != vpos)
      printf("%s%dY", vpos > cur_vpos ? "+" : "", vpos - cur_vpos);
  }
  cur_hpos = hpos;
  cur_vpos = vpos;
  return 1;
}

void lj4_printer::draw(int code, int *p, int np, const environment *env)
{
  switch (code) {
  case 'R':
    {
      if (np != 2) {
	error("2 arguments required for rule");
	break;
      }
      int hpos = env->hpos;
      int vpos = env->vpos;
      int hsize = p[0];
      int vsize = p[1];
      if (hsize < 0) {
	hpos += hsize;
	hsize = -hsize;
      }
      if (vsize < 0) {
	vpos += vsize;
	vsize = -vsize;
      }
      if (!moveto(hpos, vpos))
	return;
      printf("\033*c%da%db0P", hsize, vsize);
      break;
    }
  case 'l':
    if (np != 2) {
      error("2 arguments required for line");
      break;
    }
    hpgl_init();
    if (!moveto(env->hpos, env->vpos))
      return;
    hpgl_start();
    set_line_thickness(env->size, p[0] == 0 && p[1] == 0);
    printf("PD%d,%d", p[0], p[1]);
    hpgl_end();
    break;
  case 'p':
  case 'P':
    {
      if (np & 1) {
	error("even number of arguments required for polygon");
	break;
      }
      if (np == 0) {
	error("no arguments for polygon");
	break;
      }
      hpgl_init();
      if (!moveto(env->hpos, env->vpos))
	return;
      hpgl_start();
      if (code == 'p')
	set_line_thickness(env->size);
      printf("PMPD%d", p[0]);
      for (int i = 1; i < np; i++)
	printf(",%d", p[i]);
      printf("PM2%cP", code == 'p' ? 'E' : 'F');
      hpgl_end();
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
      hpgl_init();
      if (!moveto(env->hpos, env->vpos))
	return;
      hpgl_start();
      set_line_thickness(env->size);
      printf("PD%d,%d", p[0]/2, p[1]/2);
      const int tnum = 2;
      const int tden = 3;
      if (np > 2) {
	fputs("BR", stdout);
	for (int i = 0; i < np - 2; i += 2) {
	  if (i != 0)
	    putchar(',');
	  printf("%d,%d,%d,%d,%d,%d",
		 (p[i]*tnum)/(2*tden),
		 (p[i + 1]*tnum)/(2*tden),
		 p[i]/2 + (p[i + 2]*(tden - tnum))/(2*tden),
		 p[i + 1]/2 + (p[i + 3]*(tden - tnum))/(2*tden),
		 (p[i] - p[i]/2) + p[i + 2]/2,
		 (p[i + 1] - p[i + 1]/2) + p[i + 3]/2);
	}
      }
      printf("PR%d,%d", p[np - 2] - p[np - 2]/2, p[np - 1] - p[np - 1]/2);
      hpgl_end();
      break;
    }
  case 'c':
  case 'C':
    // troff adds an extra argument to C
    if (np != 1 && !(code == 'C' && np == 2)) {
      error("1 argument required for circle");
      break;
    }
    hpgl_init();
    if (!moveto(env->hpos + p[0]/2, env->vpos))
      return;
    hpgl_start();
    if (code == 'c') {
      set_line_thickness(env->size);
      printf("CI%d", p[0]/2);
    }
    else
      printf("WG%d,0,360", p[0]/2);
    hpgl_end();
    break;
  case 'e':
  case 'E':
    if (np != 2) {
      error("2 arguments required for ellipse");
      break;
    }
    hpgl_init();
    if (!moveto(env->hpos + p[0]/2, env->vpos))
      return;
    hpgl_start();
    printf("SC0,%.4f,0,-%.4f,2", hpgl_scale * double(p[0])/p[1], hpgl_scale);
    if (code == 'e') {
      set_line_thickness(env->size);
      printf("CI%d", p[1]/2);
    }
    else
      printf("WG%d,0,360", p[1]/2);
    printf("SC0,%.4f,0,-%.4f,2", hpgl_scale, hpgl_scale);
    hpgl_end();
    break;
  case 'a':
    {
      if (np != 4) {
	error("4 arguments required for arc");
	break;
      }
      hpgl_init();
      if (!moveto(env->hpos, env->vpos))
	return;
      hpgl_start();
      set_line_thickness(env->size);
      double c[2];
      if (adjust_arc_center(p, c)) {
	double sweep = ((atan2(p[1] + p[3] - c[1], p[0] + p[2] - c[0])
			 - atan2(-c[1], -c[0]))
			* 180.0/PI);
	if (sweep > 0.0)
	  sweep -= 360.0;
	printf("PDAR%d,%d,%f", int(c[0]), int(c[1]), sweep);
      }
      else
	printf("PD%d,%d", p[0] + p[2], p[1] + p[3]);
      hpgl_end();
    }
    break;
  case 'f':
    if (np != 1 && np != 2) {
      error("1 argument required for fill");
      break;
    }
    hpgl_init();
    hpgl_start();
    if (p[0] >= 0 && p[0] <= 1000)
      printf("FT10,%d", p[0]/10);
    hpgl_end();
    break;
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
  default:
    error("unrecognised drawing command `%1'", char(code));
    break;
  }
}

void lj4_printer::hpgl_init()
{
  if (hpgl_inited)
    return;
  hpgl_inited = 1;
  hpgl_scale = double(DEFAULT_HPGL_UNITS)/font::res;
  printf("\033&f0S"		// push position
	 "\033*p0x0Y"		// move to 0,0
	 "\033*c%dx%dy0T" // establish picture frame
	 "\033%%1B"		  // switch to HPGL
	 "SP1SC0,%.4f,0,-%.4f,2IR0,100,0,100" // set up scaling
	 "LA1,4,2,4"		// round line ends and joins
	 "PR"			// relative plotting
	 "TR0"			// opaque
	 ";\033%%1A"		// back to PCL
	 "\033&f1S",		// pop position
	 MAX_PAPER_WIDTH, MAX_PAPER_HEIGHT,
	 hpgl_scale, hpgl_scale);
}

void lj4_printer::set_line_thickness(int size, int dot)
{
  double pw;
  if (line_thickness < 0)
    pw = (size * (line_width_factor * 25.4))/(font::sizescale * 72000.0);
  else
    pw = line_thickness*25.4/font::res;
  if (dot && pw < MIN_DOT_PEN_WIDTH)
    pw = MIN_DOT_PEN_WIDTH;
  if (pw != pen_width) {
    printf("PW%f", pw);
    pen_width = pw;
  }
}

font *lj4_printer::make_font(const char *nm)
{
  return lj4_font::load_lj4_font(nm);
}

printer *make_printer()
{
  return new lj4_printer;
}

static
int lookup_paper_size(const char *s)
{
  for (int i = 0; i < sizeof(paper_table)/sizeof(paper_table[0]); i++) {
    // FIXME Perhaps allow unique prefix.
    if (strcasecmp(s, paper_table[i].name) == 0)
      return i;
  }
  return -1;
}

static
void handle_unknown_desc_command(const char *command, const char *arg,
				 const char *filename, int lineno)
{
  if (strcmp(command, "papersize") == 0) {
    if (arg == 0)
      error_with_file_and_line(filename, lineno,
			       "`papersize' command requires an argument");
    else if (paper_size < 0) {
      int n = lookup_paper_size(arg);
      if (n < 0)
	error_with_file_and_line(filename, lineno,
				 "unknown paper size `%1'", arg);
      else
	paper_size = n;
    }
  }
}

static void usage(FILE *stream);

extern "C" int optopt, optind;

int main(int argc, char **argv)
{
  program_name = argv[0];
  static char stderr_buf[BUFSIZ];
  setbuf(stderr, stderr_buf);
  font::set_unknown_desc_command_handler(handle_unknown_desc_command);
  int c;
  static const struct option long_options[] = {
    { "help", no_argument, 0, CHAR_MAX + 1 },
    { "version", no_argument, 0, 'v' },
    { NULL, 0, 0, 0 }
  };
  while ((c = getopt_long(argc, argv, ":F:p:d:lvw:c:", long_options, NULL))
	 != EOF)
    switch(c) {
    case 'l':
      landscape_flag = 1;
      break;
    case ':':
      if (optopt == 'd') {
	fprintf(stderr, "duplex assumed to be long-side\n");
	duplex_flag = 1;
      } else
	fprintf(stderr, "option -%c requires an operand\n", optopt);
      fflush(stderr);
      break;
    case 'd':
      if (!isdigit(*optarg))	// this ugly hack prevents -d without
	optind--;		//  args from messing up the arg list
      duplex_flag = atoi(optarg);
      if (duplex_flag != 1 && duplex_flag != 2) {
	fprintf(stderr, "odd value for duplex; assumed to be long-side\n");
	duplex_flag = 1;
      }
      break;
    case 'p':
      {
	int n = lookup_paper_size(optarg);
	if (n < 0)
	  error("unknown paper size `%1'", optarg);
	else
	  paper_size = n;
	break;
      }
    case 'v':
      {
	extern const char *Version_string;
	printf("GNU grolj4 (groff) version %s\n", Version_string);
	exit(0);
	break;
      }
    case 'F':
      font::command_line_font_dir(optarg);
      break;
    case 'c':
      {
	char *ptr;
	long n = strtol(optarg, &ptr, 10);
	if (n == 0 && ptr == optarg)
	  error("argument for -c must be a positive integer");
	else if (n <= 0 || n > 32767)
	  error("out of range argument for -c");
	else
	  ncopies = unsigned(n);
	break;
      }
    case 'w':
      {
	char *ptr;
	long n = strtol(optarg, &ptr, 10);
	if (n == 0 && ptr == optarg)
	  error("argument for -w must be a non-negative integer");
	else if (n < 0 || n > INT_MAX)
	  error("out of range argument for -w");
	else
	  line_width_factor = int(n);
	break;
      }
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
  fprintf(stream,
	  "usage: %s [-lv] [-d [n]] [-c n] [-p paper_size]\n"
	  "       [-w n] [-F dir] [files ...]\n",
	  program_name);
}
