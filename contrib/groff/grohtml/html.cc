// -*- C++ -*-
/* Copyright (C) 1999 Free Software Foundation, Inc.
 *
 *  Gaius Mulley (gaius@glam.ac.uk) wrote grohtml
 *  but it owes a huge amount of ideas and raw code from
 *  James Clark (jjc@jclark.com) grops/ps.cc.
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
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */

#include "driver.h"
#include "stringclass.h"
#include "cset.h"

#include "html.h"
#include <time.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "ordered_list.h"

#if !defined(TRUE)
#   define TRUE  (1==1)
#endif
#if !defined(FALSE)
#   define FALSE (1==0)
#endif

#define MAX_TEMP_NAME                1024
#define MAX_STRING_LENGTH            4096

#define Y_FUDGE_MARGIN              +0.83
#define A4_PAGE_LENGTH              (11.6944-Y_FUDGE_MARGIN)
#define DEFAULT_IMAGE_RES              80
#define IMAGE_BOARDER_PIXELS           10
#define MAX_WORDS_PER_LINE           1000        // only used for table indentation
#define GAP_SPACES                      3        // how many spaces needed to guess a gap?
#define GAP_WIDTH_ONE_LINE              2        // 1/GAP_WIDTH_ONE_LINE inches required for one line table
#define CENTER_TOLERANCE                2        // how many pixels off center will we think a line or region is centered
#define MIN_COLUMN                      7        // minimum column size pixels


/*
 *  Only uncomment one of the following to determine default image type.
 */

#define IMAGE_DEFAULT_PNG
/* #define IMAGE_DEFAULT_GIF */


#if defined(IMAGE_DEFAULT_GIF)
static enum { gif, png } image_type = gif;
static char *image_device           = "gif";
#elif defined(IMAGE_DEFAULT_PNG)
static enum { gif, png } image_type = png;
static char *image_device           = "png256";
#else
#   error "you must define either IMAGE_DEFAULT_GIF or IMAGE_DEFAULT_PNG"
#endif

static int debug_on                 = FALSE;
static int guess_on                 =  TRUE;
static int margin_on                = FALSE;
static int auto_on                  =  TRUE;
static int table_on                 =  TRUE;
static int image_res                = DEFAULT_IMAGE_RES;
static int debug_table_on           = FALSE;

static int linewidth = -1;

#define DEFAULT_LINEWIDTH 40	/* in ems/1000 */
#define MAX_LINE_LENGTH 72
#define FILL_MAX 1000

void stop () {}


/*
 *  start with a few favorites
 */

static int min (int a, int b)
{
  if (a < b) {
    return( a );
  } else {
    return( b );
  }
}

static int max (int a, int b)
{
  if (a > b) {
    return( a );
  } else {
    return( b );
  }
}


/*
 *  is_subsection - returns TRUE if a1..a2 is within b1..b2
 */

static int is_subsection (int a1, int a2, int b1, int b2)
{
  // easier to see whether this is not the case
  return( !((a1 < b1) || (a1 > b2) || (a2 < b1) || (a2 > b2)) );
}


/*
 *  is_intersection - returns TRUE if range a1..a2 intersects with b1..b2
 */

static int is_intersection (int a1, int a2, int b1, int b2)
{
  // again easier to prove NOT outside limits
  return( ! ((a1 > b2) || (a2 < b1)) );
}


/*
 *  is_digit - returns TRUE if character, ch, is a digit.
 */

static int is_digit (char ch)
{
  return( (ch >= '0') && (ch <= '9') );
}


/*
 * more_than_line_break - returns TRUE should v1 and v2 differ by more than
 *                        a simple line break.
 */

static int more_than_line_break (int v1, int v2, int size)
{
  return( abs(v1-v2)>size );
}


/*
 *  the class and methods for styles
 */

struct style {
  font        *f;
  int          point_size;
  int          font_no;
  int          height;
  int          slant;
               style       ();
               style       (font *, int, int, int, int);
  int          operator == (const style &) const;
  int          operator != (const style &) const;
};

style::style()
  : f(0)
{
}

style::style(font *p, int sz, int h, int sl, int no)
  : f(p), point_size(sz), height(h), slant(sl), font_no(no)
{
}

int style::operator==(const style &s) const
{
  return (f == s.f && point_size == s.point_size
	  && height == s.height && slant == s.slant);
}

int style::operator!=(const style &s) const
{
  return !(*this == s);
}


/*
 *  the class and methods for retaining ascii text
 */

struct char_block {
  enum { SIZE = 256 };
  char          buffer[SIZE];
  int           used;
  char_block   *next;

  char_block();
};

char_block::char_block()
: next(0), used(0)
{
}

class char_buffer {
public:
  char_buffer();
  ~char_buffer();
  char  *add_string(char *, unsigned int);
private:
  char_block *head;
  char_block *tail;
};

char_buffer::char_buffer()
: head(0), tail(0)
{
}

char_buffer::~char_buffer()
{
  while (head != 0) {
    char_block *temp = head;
    head = head->next;
    delete temp;
  }
}

char *char_buffer::add_string (char *s, unsigned int length)
{
  int i=0;
  unsigned int old_used;

  if (tail == 0) {
    tail = new char_block;
    head = tail;
  } else {
    if (tail->used + length+1 > char_block::SIZE) {
      tail->next = new char_block;
      tail       = tail->next;
    }
  }
  // at this point we have a tail which is ready for the string.
  if (tail->used + length+1 > char_block::SIZE) {
    fatal("need to increase char_block::SIZE");
  }

  old_used = tail->used;
  do {
    tail->buffer[tail->used] = s[i];
    tail->used++;
    i++;
    length--;
  } while (length>0);

  // add terminating nul character

  tail->buffer[tail->used] = '\0';
  tail->used++;

  // and return start of new string

  return( &tail->buffer[old_used] );
}

/*
 *  the classes and methods for maintaining pages and text positions and graphic regions
 */

class text_glob {
public:
  int        is_less (text_glob *a, text_glob *b);
  text_glob  (style *s, char *string, unsigned int length,
	      int min_vertical, int min_horizontal,
	      int max_vertical, int max_horizontal, int is_command, int is_html);
  text_glob  (void);
  ~text_glob (void);

  style           text_style;
  char           *text_string;
  unsigned int    text_length;
  int             minv, maxv, minh, maxh;
  int             is_raw_command;       // should the text be sent directly to the device?
  int             is_html_command;      // is the raw command definitely for the html device ie not an eqn?
};

text_glob::text_glob (style *s, char *string, unsigned int length,
		      int min_vertical, int min_horizontal,
		      int max_vertical, int max_horizontal, int is_command, int is_html)
  : text_style(*s), text_string(string), text_length(length),
    minv(min_vertical), minh(min_horizontal), maxv(max_vertical), maxh(max_horizontal),
    is_raw_command(is_command), is_html_command(is_html)
{
}

text_glob::text_glob ()
  : text_string(0), text_length(0), minv(-1), maxv(-1), minh(-1), maxh(-1),
    is_raw_command(FALSE), is_html_command(FALSE)
{
}

text_glob::~text_glob ()
{
}

int text_glob::is_less (text_glob *a, text_glob *b)
{
  if (is_intersection(a->minv, a->maxv, b->minv, b->maxv)) {
    return( a->minh < b->minh );
  } else {
    return( a->maxv < b->maxv );
  }
}

struct xycoord {
  int x;
  int y;
};

class graphic_glob {
public:
  int             is_less (graphic_glob *a, graphic_glob *b);
  graphic_glob    (int troff_code);
  graphic_glob    (void);
  ~graphic_glob   (void);

  int             minv, maxv, minh, maxh;
  int             xc, yc;
  int             nopoints;           // number of points allocated in array below
  struct xycoord *point;
  int             size;
  int             fill;
  int             code;
};

graphic_glob::graphic_glob ()
  : minv(-1), maxv(-1), minh(-1), maxh(-1), code(0), size(0), nopoints(0), point(0)
{
}

graphic_glob::~graphic_glob ()
{
  if (point != 0) {
    free(point);
  }
}

graphic_glob::graphic_glob (int troff_code)
  : minv(-1), maxv(-1), minh(-1), maxh(-1), code(troff_code), size(0), nopoints(0), point(0)
{
}

int graphic_glob::is_less (graphic_glob *a, graphic_glob *b)
{
  return( (a->minv < b->minv) || ((a->minv == b->minv) && (a->minh < b->minh)) );
}

class region_glob {
public:
                  region_glob (void);
                 ~region_glob (void);
  int             is_less     (region_glob *a, region_glob *b);

  int minv, maxv, minh, maxh;
};

int region_glob::is_less (region_glob *a, region_glob *b)
{
  return( (a->minv < b->minv) || ((a->minv == b->minv) && (a->minh < b->minh)) );
}

region_glob::region_glob (void)
  : minv(-1), maxv(-1), minh(-1), maxh(-1)
{
}

region_glob::~region_glob (void)
{
}

class page {
public:
                              page                     (void);
  void                        add                      (style *s, char *string, unsigned int length,
							int min_vertical, int min_horizontal,
							int max_vertical, int max_horizontal);
  void                        add_html_command         (style *s, char *string, unsigned int length,
							int min_vertical, int min_horizontal,
							int max_vertical, int max_horizontal);
  void                        add_special_char         (style *s, char *string, unsigned int length,
							int min_vertical, int min_horizontal,
							int max_vertical, int max_horizontal);
  void                        add_line                 (int code, int x1, int y1, int x2, int y2, int size, int fill);
  void                        add_arc                  (int code, int xc, int yc, int *p, double *c, int size, int fill);
  void                        add_polygon              (int code, int np, int *p, int oh, int ov, int size, int fill);
  void                        add_spline               (int code, int xc, int yc, int np, int *p, int size, int fill);
  void                        calculate_region         (void);
  int                         is_in_region             (graphic_glob *g);
  int                         can_grow_region          (graphic_glob *g);
  void                        make_new_region          (graphic_glob *g);
  int                         has_line                 (region_glob *r);
  int                         has_word                 (region_glob *r);
  int                         no_raw_commands          (int minv, int maxv);

  // and the data

  ordered_list <region_glob>  regions;       // squares of bitmapped pics,eqn,tbl's
  ordered_list <text_glob>    words;         // position of words on page
  ordered_list <graphic_glob> lines;         // position of lines on page
  char_buffer                 buffer;        // all characters for this page
  int                         is_in_graphic; // should graphics and words go below or above
  ordered_list <text_glob>    region_words;  // temporary accumulation of words in a region
  ordered_list <graphic_glob> region_lines;  // (as above) and used so that we can determine
                                             // the regions vertical limits
};

page::page()
  : is_in_graphic(FALSE)
{
}

void page::add (style *s, char *string, unsigned int length,
		int min_vertical, int min_horizontal,
		int max_vertical, int max_horizontal)
{
  if (length > 0) {
    text_glob *g=new text_glob(s, buffer.add_string(string, length), length,
			       min_vertical, min_horizontal, max_vertical, max_horizontal, FALSE, FALSE);
    if (is_in_graphic) {
      region_words.add(g);
    } else {
      words.add(g);
    }
  }
}

/*
 *  add_html_command - it only makes sense to add html commands when we are not inside
 *                     a graphical entity.
 */

void page::add_html_command (style *s, char *string, unsigned int length,
			     int min_vertical, int min_horizontal,
			     int max_vertical, int max_horizontal)
{
  if ((length > 0) && (! is_in_graphic)) {
    text_glob *g=new text_glob(s, buffer.add_string(string, length), length,
			       min_vertical, min_horizontal, max_vertical, max_horizontal, TRUE, TRUE);
    words.add(g);
  }
}

/*
 *  add_special_char - it only makes sense to add special characters when we are inside
 *                     a graphical entity.
 */

void page::add_special_char (style *s, char *string, unsigned int length,
			     int min_vertical, int min_horizontal,
			     int max_vertical, int max_horizontal)
{
  if ((length > 0) && (is_in_graphic)) {
    text_glob *g=new text_glob(s, buffer.add_string(string, length), length,
			       min_vertical, min_horizontal, max_vertical, max_horizontal, TRUE, FALSE);
    region_words.add(g);
  }
}

void page::add_line (int code, int x1, int y1, int x2, int y2, int size, int fill)
{
  graphic_glob *g = new graphic_glob(code);

  g->minh       = min(x1, x2);
  g->maxh       = max(x1, x2);
  g->minv       = min(y1, y2);
  g->maxv       = max(y1, y2);
  g->point      = (struct xycoord *)malloc(sizeof(xycoord)*2);
  g->nopoints   = 2;
  g->point[0].x = x1 ;
  g->point[0].y = y1 ;
  g->point[1].x = x2 ;
  g->point[1].y = y2 ;
  g->xc         = 0;
  g->yc         = 0;
  g->size       = size;
  g->fill       = fill;

  if (is_in_graphic) {
    region_lines.add(g);
  } else {
    lines.add(g);
  }
}

/*
 *  assign_min_max_for_arc - works out the smallest box that will encompass an
 *                           arc defined by:  origin: g->xc, g->xc
 *                           and vector (p[0], p[1]) and (p[2], p[3])
 */

void assign_min_max_for_arc (graphic_glob *g, int *p, double *c)
{
  int radius = (int) sqrt(c[0]*c[0]+c[1]*c[1]);
  int xv1    = p[0];
  int yv1    = p[1];
  int xv2    = p[2];
  int yv2    = p[3];
  int x1     = g->xc+xv1;
  int y1     = g->yc+yv1;
  int x2     = g->xc+xv1+xv2;
  int y2     = g->yc+yv1+yv2;

  // firstly lets use the 'circle' limitation
  g->minh = x1-radius;
  g->maxh = x1+radius;
  g->minv = y1-radius;
  g->maxv = y1+radius;

  // incidentally I'm sure there is a better way to do this, but I don't know it
  // please can someone let me know or "improve" this function

  // now see which min/max can be reduced and increased for the limits of the arc
  //
  //
  //       Q2   |   Q1
  //       -----+-----
  //       Q3   |   Q4
  //


  if ((xv1>=0) && (yv1>=0)) {
    // first vector in Q3
    if ((xv2>=0) && (yv2>=0)) {
      // second in Q1
      g->maxh = x2;
      g->minv = y1;
    } else if ((xv2<0) && (yv2>=0)) {
      // second in Q2
      g->maxh = x2;
      g->minv = y1;
    } else if ((xv2>=0) && (yv2<0)) {
      // second in Q4
      g->minv = min(y1, y2);
    } else if ((xv2<0) && (yv2<0)) {
      // second in Q3
      if (x1>=x2) {
	g->minh = x2;
	g->maxh = x1;
	g->minv = min(y1, y2);
	g->maxv = max(y1, y2);
      } else {
	// xv2, yv2 could all be zero?
      }
    }
  } else if ((xv1>=0) && (yv1<0)) {
    // first vector in Q2
    if ((xv2>=0) && (yv2>=0)) {
      // second in Q1
      g->maxh = max(x1, x2);
      g->minh = min(x1, x2);
      g->minv = y1;
    } else if ((xv2<0) && (yv2>=0)) {
      // second in Q2
      if (x1<x2) {
	g->maxh = x2;
	g->minh = x1;
	g->minv = min(y1, y2);
	g->maxv = max(y1, y2);
      } else {
	// otherwise almost full circle anyway
      }
    } else if ((xv2>=0) && (yv2<0)) {
      // second in Q4
      g->minv = y2;
      g->minh = x1;
    } else if ((xv2<0) && (yv2<0)) {
      // second in Q3
      g->minh = min(x1, x2);
    }
  } else if ((xv1<0) && (yv1<0)) {
    // first vector in Q1
    if ((xv2>=0) && (yv2>=0)) {
      // second in Q1
      if (x1<x2) {
	g->minh = x1;
	g->maxh = x2;
	g->minv = min(y1, y2);
	g->maxv = max(y1, y2);
      } else {
	// nearly full circle
      }
    } else if ((xv2<0) && (yv2>=0)) {
      // second in Q2
      g->maxv = max(y1, y2);
    } else if ((xv2>=0) && (yv2<0)) {
      // second in Q4
      g->minv = min(y1, y2);
      g->maxv = max(y1, y2);
      g->minh = min(x1, x2);
    } else if ((xv2<0) && (yv2<0)) {
      // second in Q3
      g->minh = x2;
      g->maxv = y1;
    }
  } else if ((xv1<0) && (yv1>=0)) {
    // first vector in Q4
    if ((xv2>=0) && (yv2>=0)) {
      // second in Q1
      g->maxh = max(x1, x2);
    } else if ((xv2<0) && (yv2>=0)) {
      // second in Q2
      g->maxv = max(y1, y2);
      g->maxh = max(x1, x2);
    } else if ((xv2>=0) && (yv2<0)) {
      // second in Q4
      if (x1>=x2) {
	g->minv = min(y1, y2);
	g->maxv = max(y1, y2);
	g->minh = min(x1, x2);
	g->maxh = max(x2, x2);
      } else {
	// nearly full circle
      }
    } else if ((xv2<0) && (yv2<0)) {
      // second in Q3
      g->maxv = max(y1, y2);
      g->minh = min(x1, x2);
      g->maxh = max(x1, x2);
    }
  }
  // this should *never* happen but if it does it means a case above is wrong..

  // this code is only present for safety sake
  if (g->maxh < g->minh) {
    if (debug_on) {
      fprintf(stderr, "assert failed minh > maxh\n"); fflush(stderr);
      stop();
    }
    g->maxh = g->minh;
  }
  if (g->maxv < g->minv) {
    if (debug_on) {
      fprintf(stderr, "assert failed minv > maxv\n"); fflush(stderr);
      stop();
    }
    g->maxv = g->minv;
  }
}

void page::add_arc (int code, int xc, int yc, int *p, double *c, int size, int fill)
{
  graphic_glob *g = new graphic_glob(code);

  g->point      = (struct xycoord *)malloc(sizeof(xycoord)*2);
  g->nopoints   = 2;
  g->point[0].x = p[0] ;
  g->point[0].y = p[1] ;
  g->point[1].x = p[2] ;
  g->point[1].y = p[3] ;
  g->xc         = xc;
  g->yc         = yc;
  g->size       = size;
  g->fill       = fill;

  assign_min_max_for_arc(g, p, c);

  if (is_in_graphic) {
    region_lines.add(g);
  } else {
    lines.add(g);
  }
}


void page::add_polygon (int code, int np, int *p, int oh, int ov, int size, int fill)
{
  graphic_glob *g = new graphic_glob(code);
  int           j = 0;
  int           i;

  g->point      = (struct xycoord *)malloc(sizeof(xycoord)*np/2);
  g->nopoints   = np/2;

  for (i=0; i<g->nopoints; i++) {
    g->point[i].x = p[j];
    j++;
    g->point[i].y = p[j];
    j++;
  }
  // now calculate min/max
  g->minh = g->point[0].x;
  g->minv = g->point[0].y;
  g->maxh = g->point[0].x;
  g->maxv = g->point[0].y;
  for (i=1; i<g->nopoints; i++) {
    g->minh = min(g->minh, g->point[i].x);
    g->minv = min(g->minv, g->point[i].y);
    g->maxh = max(g->maxh, g->point[i].x);
    g->maxv = max(g->maxv, g->point[i].y);
  }
  g->size = size;
  g->xc   = oh;
  g->yc   = ov;
  g->fill = fill;

  if (is_in_graphic) {
    region_lines.add(g);
  } else {
    lines.add(g);
  }
}

void page::add_spline (int code, int xc, int yc, int np, int *p, int size, int fill)
{
  graphic_glob *g = new graphic_glob(code);
  int           j = 0;
  int           i;

  g->point      = (struct xycoord *)malloc(sizeof(xycoord)*np/2);
  g->nopoints   = np/2;

  for (i=0; i<g->nopoints; i++) {
    g->point[i].x = p[j];
    j++;
    g->point[i].y = p[j];
    j++;
  }
  // now calculate min/max
  g->minh = min(g->point[0].x, g->point[0].x/2);
  g->minv = min(g->point[0].y, g->point[0].y/2);
  g->maxh = max(g->point[0].x, g->point[0].x/2);
  g->maxv = max(g->point[0].y, g->point[0].y/2);

  /* tnum/tden should be between 0 and 1; the closer it is to 1
     the tighter the curve will be to the guiding lines; 2/3
     is the standard value */
  const int tnum = 2;
  const int tden = 3;

  for (i=1; i<g->nopoints-1; i++) {
    g->minh = min(g->minh, g->point[i].x*tnum/(2*tden));
    g->minv = min(g->minv, g->point[i].y*tnum/(2*tden));
    g->maxh = max(g->maxh, g->point[i].x*tnum/(2*tden));
    g->maxv = max(g->maxv, g->point[i].y*tnum/(2*tden));

    g->minh = min(g->minh, g->point[i].x/2+(g->point[i+1].x*(tden-tden))/(2*tden));
    g->minv = min(g->minv, g->point[i].y/2+(g->point[i+1].y*(tden-tden))/(2*tden));
    g->maxh = max(g->maxh, g->point[i].x/2+(g->point[i+1].x*(tden-tden))/(2*tden));
    g->maxv = max(g->maxv, g->point[i].y/2+(g->point[i+1].y*(tden-tden))/(2*tden));

    g->minh = min(g->minh, (g->point[i].x-g->point[i].x/2) + g->point[i+1].x/2);
    g->minv = min(g->minv, (g->point[i].y-g->point[i].y/2) + g->point[i+1].y/2);
    g->maxh = max(g->maxh, (g->point[i].x-g->point[i].x/2) + g->point[i+1].x/2);
    g->maxv = max(g->maxv, (g->point[i].y-g->point[i].y/2) + g->point[i+1].y/2);
  }
  i = g->nopoints-1;

  g->minh = min(g->minh, (g->point[i].x-g->point[i].x/2)) + xc;
  g->minv = min(g->minv, (g->point[i].y-g->point[i].y/2)) + yc;
  g->maxh = max(g->maxh, (g->point[i].x-g->point[i].x/2)) + xc;
  g->maxv = max(g->maxv, (g->point[i].y-g->point[i].y/2)) + yc;

  g->size = size;
  g->xc   = xc;
  g->yc   = yc;
  g->fill = fill;

  if (is_in_graphic) {
    region_lines.add(g);
  } else {
    lines.add(g);
  }
}

/*
 *  the classes and methods for simple_output manipulation
 */

simple_output::simple_output(FILE *f, int n)
: fp(f), max_line_length(n), col(0), need_space(0), fixed_point(0)
{
}

simple_output &simple_output::set_file(FILE *f)
{
  fp = f;
  col = 0;
  return *this;
}

simple_output &simple_output::copy_file(FILE *infp)
{
  int c;
  while ((c = getc(infp)) != EOF)
    putc(c, fp);
  return *this;
}

simple_output &simple_output::end_line()
{
  if (col != 0) {
    putc('\n', fp);
    col = 0;
    need_space = 0;
  }
  return *this;
}

simple_output &simple_output::special(const char *s)
{
  return *this;
}

simple_output &simple_output::simple_comment(const char *s)
{
  if (col != 0)
    putc('\n', fp);
  fputs("<!-- ", fp);
  fputs(s, fp);
  fputs(" -->\n", fp);
  col = 0;
  need_space = 0;
  return *this;
}

simple_output &simple_output::begin_comment(const char *s)
{
  if (col != 0)
    putc('\n', fp);
  fputs("<!-- ", fp);
  fputs(s, fp);
  col = 5 + strlen(s);
  return *this;
}

simple_output &simple_output::end_comment()
{
  if (need_space) {
    putc(' ', fp);
  }
  fputs(" -->\n", fp);
  col = 0;
  need_space = 0;
  return *this;
}

simple_output &simple_output::comment_arg(const char *s)
{
  int len = strlen(s);

  if (col + len + 1 > max_line_length) {
    fputs("\n ", fp);
    col = 1;
  }
  fputs(s, fp);
  col += len + 1;
  return *this;
}

simple_output &simple_output::set_fixed_point(int n)
{
  assert(n >= 0 && n <= 10);
  fixed_point = n;
  return *this;
}

simple_output &simple_output::put_delimiter(char c)
{
  putc(c, fp);
  col++;
  need_space = 0;
  return *this;
}

simple_output &simple_output::put_string(const char *s, int n)
{
  int i=0;

  while (i<n) {
    fputc(s[i], fp);
    i++;
  }
  col += n;
  return *this;
}

simple_output &simple_output::put_translated_string(const char *s)
{
  int i=0;

  while (s[i] != (char)0) {
    if ((s[i] & 0x7f) == s[i]) {
      fputc(s[i], fp);
    }
    i++;
  }
  col += i;
  return *this;
}

simple_output &simple_output::put_string(const char *s)
{
  int i=0;

  while (s[i] != '\0') {
    fputc(s[i], fp);
    i++;
  }
  col += i;
  return *this;
}

struct html_2_postscript {
  char *html_char;
  char *postscript_char;
};

static struct html_2_postscript char_conversions[] = {
  "+-", "char177",
  "eq", "=",
  "mu", "char215",
  NULL, NULL,
};


// this is an aweful hack which attempts to translate html characters onto
// postscript characters. Can this be done inside the devhtml files?
//
// or should we read the devps files and find out the translations?
//

simple_output &simple_output::put_translated_char (const char *s)
{
  int i=0;

  while (char_conversions[i].html_char != NULL) {
    if (strcmp(s, char_conversions[i].html_char) == 0) {
      put_string(char_conversions[i].postscript_char);
      return *this;
    } else {
      i++;
    }
  }
  put_string(s);
  return *this;
}

simple_output &simple_output::put_number(int n)
{
  char buf[1 + INT_DIGITS + 1];
  sprintf(buf, "%d", n);
  int len = strlen(buf);
  put_string(buf, len);
  need_space = 1;
  return *this;
}

simple_output &simple_output::put_float(double d)
{
  char buf[128];

  sprintf(buf, "%.4f", d);
  int len = strlen(buf);
  put_string(buf, len);
  need_space = 1;
  return *this;
}


simple_output &simple_output::put_symbol(const char *s)
{
  int len = strlen(s);

  if (need_space) {
    putc(' ', fp);
    col++;
  }
  fputs(s, fp);
  col += len;
  need_space = 1;
  return *this;
}

class html_font : public font {
  html_font(const char *);
public:
  int encoding_index;
  char *encoding;
  char *reencoded_name;
  ~html_font();
  void handle_unknown_font_command(const char *command, const char *arg,
				   const char *filename, int lineno);
  static html_font *load_html_font(const char *);
};

html_font *html_font::load_html_font(const char *s)
{
  html_font *f = new html_font(s);
  if (!f->load()) {
    delete f;
    return 0;
  }
  return f;
}

html_font::html_font(const char *nm)
: font(nm)
{
}

html_font::~html_font()
{
}

void html_font::handle_unknown_font_command(const char *command, const char *arg,
					    const char *filename, int lineno)
{
  if (strcmp(command, "encoding") == 0) {
    if (arg == 0)
      error_with_file_and_line(filename, lineno,
			       "`encoding' command requires an argument");
    else
      encoding = strsave(arg);
  }
}


/*
 *  a simple class to contain the header to this document
 */

class title_desc {
public:
          title_desc ();
         ~title_desc ();

  int     has_been_written;
  int     has_been_found;
  char    text[MAX_STRING_LENGTH];
};


title_desc::title_desc ()
  : has_been_found(FALSE), has_been_written(FALSE)
{
}

title_desc::~title_desc ()
{
}

class header_desc {
public:
                            header_desc ();
                           ~header_desc ();

  int                       no_of_headings;      // how many headings have we found?
  char_buffer               headings;            // all the headings used in the document
  ordered_list  <text_glob> headers;
  int                       header_level;        // current header level
  int                       written_header;      // have we written the header yet?
  char                      header_buffer[MAX_STRING_LENGTH];  // current header text

  void                      write_headings (FILE *f);
};

header_desc::header_desc ()
  :   no_of_headings(0), header_level(2), written_header(0)
{
}

header_desc::~header_desc ()
{
}

/*
 *  paragraph_type - alignment for a new paragraph
 */

typedef enum { left_alignment, center_alignment } paragraph_type;

/*
 *  text_defn - defines the limit of text, initially these are stored in the
 *              column array as words. Later we examine the white space between
 *              the words in successive lines to find out whether we can detect
 *              distinct columns. The columns are generated via html tables.
 */

struct text_defn {
  int left;       // the start of a word or text
  int right;      // the end of the text and beginning of white space
  int is_used;    // will this this column be used for words or space
};


/*
 * note that html_tables are currently only used to provide a better
 * indentation mechanism for html text (in particular it allows grohtml
 * to render .IP and .2C together with autoformatting).
 */

class html_table {
public:
                            html_table ();
                           ~html_table ();

  int                       no_of_columns;     // how many columns are we using?
  struct text_defn         *columns;           // left and right margins for each column
  int                       vertical_limit;    // the limit of the table
};

html_table::html_table ()
  : no_of_columns(0), columns(0), vertical_limit(0)
{
}

html_table::~html_table ()
{
}

class html_printer : public printer {
  FILE                *tempfp;
  simple_output        html;
  simple_output        troff;
  int                  res;
  int                  postscript_res;
  int                  space_char_index;
  int                  no_of_printed_pages;
  int                  paper_length;
  enum                 { SBUF_SIZE = 256 };
  char                 sbuf[SBUF_SIZE];
  int                  sbuf_len;
  int                  sbuf_start_hpos;
  int                  sbuf_vpos;
  int                  sbuf_end_hpos;
  int                  sbuf_space_width;
  int                  sbuf_space_count;
  int                  sbuf_space_diff_count;
  int                  sbuf_space_code;
  int                  sbuf_kern;
  style                sbuf_style;
  style                output_style;
  int                  output_hpos;
  int                  output_vpos;
  int                  output_draw_point_size;
  int                  line_thickness;
  int                  output_line_thickness;
  int                  fill;
  unsigned char        output_space_code;
  string               defs;
  char                *inside_font_style;
  int                  page_number;
  title_desc           title;
  header_desc          header;
  page                *page_contents;
  html_table           indentation;
  int                  left_margin_indent;
  int                  right_margin_indent;
  int                  need_one_newline;
  int                  issued_newline;
  int                  in_paragraph;
  int                  need_paragraph;
  paragraph_type       para_type;
  char                 image_name[MAX_STRING_LENGTH];
  int                  image_number;
  int                  graphic_level;

  int                  start_region_vpos;
  int                  start_region_hpos;
  int                  end_region_vpos;
  int                  end_region_hpos;
  int                  cutoff_heading;

  struct graphic_glob *start_graphic;
  struct text_glob    *start_text;


  void  flush_sbuf                    ();
  void  set_style                     (const style &);
  void  set_space_code                (unsigned char c);
  void  do_exec                       (char *, const environment *);
  void  do_import                     (char *, const environment *);
  void  do_def                        (char *, const environment *);
  void  do_mdef                       (char *, const environment *);
  void  do_file                       (char *, const environment *);
  void  set_line_thickness            (const environment *);
  void  change_font                   (text_glob *g, int is_to_html);
  void  terminate_current_font        (void);
  void  flush_font                    (void);
  void  flush_page                    (void);
  void  display_word                  (text_glob *g, int is_to_html);
  void  html_display_word             (text_glob *g);
  void  troff_display_word            (text_glob *g);
  void  display_line                  (graphic_glob *g, int is_to_html);
  void  display_fill                  (graphic_glob *g);
  void  calculate_margin              (void);
  void  traverse_page_regions         (void);
  void  dump_page                     (void);
  int   is_within_region              (graphic_glob *g);
  int   is_within_region              (text_glob *t);
  int   is_less                       (graphic_glob *g, text_glob *t);
  void  display_globs                 (int is_to_html);
  void  move_horizontal               (text_glob *g, int left_margin);
  void  move_vertical                 (text_glob *g, paragraph_type p);
  void  write_html_font_face          (const char *fontname, const char *left, const char *right);
  void  write_html_font_type          (const char *fontname, const char *left, const char *right);
  void  html_change_font              (text_glob *g, const char *fontname, int size);
  char *html_position_text            (text_glob *g, int left_margin, int right_margin);
  int   html_position_region          (void);
  void  troff_change_font             (const char *fontname, int size, int font_no);
  void  troff_position_text           (text_glob *g);
  int   pretend_is_on_same_line       (text_glob *g, int left_margin, int right_margin);
  int   is_on_same_line               (text_glob *g, int vpos);
  int   looks_like_subscript          (text_glob *g);
  int   looks_like_superscript        (text_glob *g);
  void  begin_paragraph               (paragraph_type p);
  void  begin_paragraph_no_height     (paragraph_type p);
  void  force_begin_paragraph         (void);
  void  end_paragraph                 (void);
  void  html_newline                  (void);
  void  convert_to_image              (char *name);
  void  write_title                   (int in_head);
  void  find_title                    (void);
  int   is_bold                       (text_glob *g);
  void  write_header                  (void);
  void  determine_header_level        (void);
  void  build_header                  (text_glob *g);
  void  make_html_indent              (int indent);
  int   is_whole_line_bold            (text_glob *g);
  int   is_a_header                   (text_glob *g);
  int   processed_header              (text_glob *g);
  void  make_new_image_name           (void);
  void  create_temp_name              (char *name, char *extension);
  void  calculate_region_margins      (region_glob *r);
  void  remove_redundant_regions      (void);
  void  remove_duplicate_regions      (void);
  void  move_region_to_page           (void);
  void  calculate_region_range        (graphic_glob *r);
  void  flush_graphic                 (void);
  void  write_string                  (graphic_glob *g, int is_to_html);
  void  prologue                      (void);
  int   gs_x                          (int x);
  int   gs_y                          (int y);
  void  display_regions               (void);
  int   check_able_to_use_table       (text_glob *g);
  int   using_table_for_indent        (void);
  int   collect_columns               (struct text_defn *line, struct text_defn *last, int max_words);
  void  include_into_list             (struct text_defn *line, struct text_defn *item);
  int   is_in_column                  (struct text_defn *line, struct text_defn *item, int max_words);
  int   is_column_match               (struct text_defn *match, struct text_defn *line1, struct text_defn *line2, int max_words);
  int   count_columns                 (struct text_defn *line);
  void  rewind_text_to                (text_glob *g);
  int   found_use_for_table           (text_glob *start);
  void  column_display_word           (int vert, int left, int right, int next);
  void  start_table                   (void);
  void  end_table                     (void);
  void  foreach_column_include_text   (text_glob *start);
  void  define_cell                   (int left, int right);
  int   column_calculate_left_margin  (int left, int right);
  int   column_calculate_right_margin (int left, int right);
  void  display_columns               (const char *word, const char *name, text_defn *line);
  void  calculate_right               (struct text_defn *line, int max_words);
  void  determine_right_most_column   (struct text_defn *line, int max_words);
  int   remove_white_using_words      (struct text_defn *next_guess, struct text_defn *last_guess, struct text_defn *next_line);
  int   copy_line                     (struct text_defn *dest, struct text_defn *src);
  void  combine_line                  (struct text_defn *dest, struct text_defn *src);
  int   conflict_with_words           (struct text_defn *column_guess, struct text_defn *words);
  void  remove_entry_in_line          (struct text_defn *line, int j);
  void  remove_redundant_columns      (struct text_defn *line);
  void  add_column_gaps               (struct text_defn *line);
  int   continue_searching_column     (text_defn *next_col, text_defn *last_col, text_defn *all_words);
  void  add_right_full_width          (struct text_defn *line, int mingap);
  int   is_continueous_column         (text_defn *last_col, text_defn *next_line);
  int   is_exact_left                 (text_defn *last_col, text_defn *next_line);
  void  emit_space                    (text_glob *g, int force_space);
  int   is_in_middle                  (int left, int right);
  int   check_able_to_use_center      (text_glob *g);
  void  write_centered_line           (text_glob *g);
  int   single_centered_line          (text_defn *first, text_defn *second, text_glob *g);
  int   determine_row_limit           (text_glob *start, int v);
  void  assign_used_columns           (text_glob *start);
  int   find_column_index             (text_glob *t);
  int   large_enough_gap              (text_defn *last_col);
  int   is_worth_column               (int left, int right);
  int   is_subset_of_columns          (text_defn *a, text_defn *b);
  void  count_hits                    (text_defn *col);
  int   calculate_min_gap             (text_glob *g);

public:
  html_printer();
  ~html_printer();
  void set_char(int i, font *f, const environment *env, int w, const char *name);
  void draw(int code, int *p, int np, const environment *env);
  void begin_page(int);
  void end_page(int);
  void special(char *arg, const environment *env);
  font *make_font(const char *);
  void end_of_line();
};

html_printer::html_printer()
: no_of_printed_pages(0),
  sbuf_len(0),
  output_hpos(-1),
  output_vpos(-1),
  html(0, MAX_LINE_LENGTH),
  troff(0, MAX_LINE_LENGTH),
  line_thickness(-1),
  inside_font_style(0),
  fill(FILL_MAX + 1),
  page_number(0),
  left_margin_indent(0),
  right_margin_indent(0),
  start_region_vpos(0),
  start_region_hpos(0),
  end_region_vpos(0),
  end_region_hpos(0),
  need_one_newline(0),
  issued_newline(0),
  image_number(0),
  graphic_level(0),
  cutoff_heading(100),
  in_paragraph(0),
  need_paragraph(0),
  para_type(left_alignment)
{
  tempfp = xtmpfile();
  html.set_file(tempfp);
  if (linewidth < 0)
    linewidth = DEFAULT_LINEWIDTH;
  if (font::hor != 1)
    fatal("horizontal resolution must be 1");
  if (font::vert != 1)
    fatal("vertical resolution must be 1");
#if 0
  // should be sorted html..
  if (font::res % (font::sizescale*72) != 0)
    fatal("res must be a multiple of 72*sizescale");
#endif
  int r = font::res;
  int point = 0;
  while (r % 10 == 0) {
    r /= 10;
    point++;
  }
  res = r;
  html.set_fixed_point(point);
  space_char_index = font::name_to_index("space");
  paper_length = font::paperlength;
  if (paper_length == 0)
    paper_length = 11*font::res;
  page_contents   = new page;

  postscript_res = 72000;
}


void html_printer::set_char(int i, font *f, const environment *env, int w, const char *name)
{
  unsigned char code = f->get_code(i);

  style sty(f, env->size, env->height, env->slant, env->fontno);
  if (sty.slant != 0) {
    if (sty.slant > 80 || sty.slant < -80) {
      error("silly slant `%1' degrees", sty.slant);
      sty.slant = 0;
    }
  }
  if ((name != 0) && (page_contents->is_in_graphic)) {
    flush_sbuf();
    int r=font::res;   // resolution of the device actually
    page_contents->add_special_char(&sty, (char *)name, strlen(name),
				    env->vpos-sty.point_size*r/72, env->hpos,
				    env->vpos, env->hpos+w);
    sbuf_end_hpos   = env->hpos + w;
    sbuf_start_hpos = env->hpos;
    sbuf_vpos       = env->vpos;
    sbuf_style      = sty;
    sbuf_kern       = 0;
    return;
  }

  if (sbuf_len > 0) {
    if (sbuf_len < SBUF_SIZE
	&& sty == sbuf_style
	&& sbuf_vpos == env->vpos) {
      if (sbuf_end_hpos == env->hpos) {
	sbuf[sbuf_len++] = code;
	sbuf_end_hpos += w + sbuf_kern;
	return;
      }
      /* If sbuf_end_hpos - sbuf_kern == env->hpos, we are better off
	 starting a new string. */
      if (sbuf_len < SBUF_SIZE - 1 && env->hpos >= sbuf_end_hpos
	  && (sbuf_kern == 0 || sbuf_end_hpos - sbuf_kern != env->hpos)) {
	if (sbuf_space_code < 0) {
#if 0
	  sbuf_space_code = ' ';
	  sbuf_space_count++;
	  sbuf_space_width = env->hpos - sbuf_end_hpos;
	  sbuf_end_hpos = env->hpos + w + sbuf_kern;
	  sbuf[sbuf_len++] = ' ';
	  sbuf[sbuf_len++] = code;
	  return;
#endif
	} else {
	  int diff = env->hpos - sbuf_end_hpos - sbuf_space_width;
	  if (diff == 0) {
	    sbuf_end_hpos = env->hpos + w + sbuf_kern;
	    sbuf[sbuf_len++] = sbuf_space_code;
	    sbuf[sbuf_len++] = code;
	    sbuf_space_count++;
	    if (diff == 1)
	      sbuf_space_diff_count++;
	    else if (diff == -1)
	      sbuf_space_diff_count--;
	    return;
	  }
	}
      }
    }
    flush_sbuf();
  }
  sbuf_len = 1;
  sbuf[0] = code;
  sbuf_end_hpos = env->hpos + w;
  sbuf_start_hpos = env->hpos;
  sbuf_vpos = env->vpos;
  sbuf_style = sty;
  sbuf_space_code = -1;
  sbuf_space_width = 0;
  sbuf_space_count = sbuf_space_diff_count = 0;
  sbuf_kern = 0;
}


/*
 *  make_new_image_name - creates a new file name ready for a image file.
 *                        it leaves the extension off.
 */

void html_printer::make_new_image_name (void)
{
  image_number++;
  sprintf(image_name, "groff-html-%d-%d", image_number, getpid());
}

/*
 *  write_title - writes the title to this document
 */

void html_printer::write_title (int in_head)
{
  if (title.has_been_found) {
    if (in_head) {
      html.put_string("<title>");
      html.put_string(title.text);
      html.put_string("</title>\n");
    } else {
      title.has_been_written = TRUE;
      html.put_string("<h1 align=center>");
      html.put_string(title.text);
      html.put_string("</h1>\n");
    }
  }
}


/*
 *  find_title - finds a title to this document, if it exists.
 */

void html_printer::find_title (void)
{
  text_glob    *t;
  int           r=font::res;
  int           removed_from_head;

  if ((page_number == 1) && (guess_on)) {
    if (! page_contents->words.is_empty()) {

      int end_title_hpos     = 0;
      int start_title_hpos   = 0;
      int start_title_vpos   = 0;
      int found_title_start  = FALSE;
      int height             = 0;
      int start_region       =-1;

      if (! page_contents->regions.is_empty()) {
	region_glob *r;

	page_contents->regions.start_from_head();
	r = page_contents->regions.get_data();
	if (r->minv > 0) {
	  start_region = r->minv;
	}
      }
      
      page_contents->words.start_from_head();
      do {
	t = page_contents->words.get_data();
	removed_from_head = FALSE;
	if ((found_title_start) && (start_region != -1) && (t->maxv >= start_region)) {
	  /*
	   * we have just encountered the first graphic region so
	   * we stop looking for a title.
	   */
	  title.has_been_found = TRUE;
	  return;
	} else if (t->is_raw_command) {
	  // skip raw commands
	} else if ((!found_title_start) && (t->minh > left_margin_indent) &&
		   ((start_region == -1) || (t->maxv < start_region))) {
	  start_title_vpos     = t->minv;
	  end_title_hpos       = t->minh;
	  strcpy((char *)title.text, (char *)t->text_string);
	  height               = t->text_style.point_size*r/72;
	  found_title_start    = TRUE;
	  page_contents->words.sub_move_right();
	  removed_from_head = ((!page_contents->words.is_empty()) &&
			       (page_contents->words.is_equal_to_head()));
	} else if (found_title_start) {
	  if ((t->minv == start_title_vpos) ||
	      ((!more_than_line_break(start_title_vpos, t->minv, (height*3)/2)) &&
	       (t->minh > left_margin_indent)) ||
	      (is_bold(t) && (t->minh > left_margin_indent))) {
	    start_title_vpos = min(t->minv, start_title_vpos);
	    end_title_hpos   = max(t->maxh, end_title_hpos);
	    strcat(title.text, " ");
	    strcat(title.text, (char *)t->text_string);
	    page_contents->words.sub_move_right();
	    removed_from_head = ((!page_contents->words.is_empty()) &&
				 (page_contents->words.is_equal_to_head()));
	  } else {
	    // end of title
	    title.has_been_found = TRUE;
	    return;
	  }
	} else if (t->minh == left_margin_indent) {
	  // no margin exists
	  return;
	} else {
	  // move onto next word
	  page_contents->words.move_right();
	}
      } while ((! page_contents->words.is_equal_to_head()) || (removed_from_head));
    }
  }
}

/*
 *  html_newline - generates a newline <br>
 */

void html_printer::html_newline (void)
{
  int r        = font::res;
  int height   = output_style.point_size*r/72;

  if (in_paragraph) {
    // safe to generate a pretty newline
    html.put_string("<br>\n");
  } else {
    html.put_string("<br>");
  }
  output_vpos += height;
  issued_newline = TRUE;
}

/*
 *  force_begin_paragraph - force the begin_paragraph to be emitted.
 */

void html_printer::force_begin_paragraph (void)
{
  if (in_paragraph && need_paragraph) {
    switch (para_type) {

    case left_alignment:   html.put_string("<p>");
                           break;
    case center_alignment: html.put_string("<p align=center>");
                           break;
    default:               fatal("unknown paragraph alignment type");
    }
    need_paragraph   = FALSE;
  }
}

/*
 *  begin_paragraph - starts a new paragraph. It does nothing if a paragraph
 *                    has already been started.
 */

void html_printer::begin_paragraph (paragraph_type p)
{
  if (! in_paragraph) {
    int r        = font::res;
    int height   = output_style.point_size*r/72;

    if (output_vpos >=0) {
      // we leave it alone if it is set to the top of page
      output_vpos += height;
    }
    need_paragraph = TRUE;   // delay the <p> just in case we don't actually emit text
    in_paragraph   = TRUE;
    issued_newline = TRUE;
    para_type      = p;
  }
}


/*
 *  begin_paragraph_no_height - starts a new paragraph. It does nothing if a paragraph
 *                              has already been started. Note it does not alter output_vpos.
 */

void html_printer::begin_paragraph_no_height (paragraph_type p)
{
  if (! in_paragraph) {
    need_paragraph = TRUE;   // delay the <p> just in case we don't actually emit text
    in_paragraph   = TRUE;
    issued_newline = TRUE;
    para_type      = p;
  }
}

/*
 *  end_paragraph - end the current paragraph. It does nothing if a paragraph
 *                  has not been started.
 */

void html_printer::end_paragraph (void)
{
  if (in_paragraph) {
    // check whether we have generated any text inbetween the potential paragraph begin end
    if (! need_paragraph) {
      int r        = font::res;
      int height   = output_style.point_size*r/72;

      output_vpos += height;
      html.put_string("</p>\n");
    }
    terminate_current_font();
    para_type    = left_alignment;
    in_paragraph = FALSE;
  }
}

/*
 *  calculate_margin - runs through the words and graphics globs
 *                     and finds the start of the left most margin.
 */

void html_printer::calculate_margin (void)
{
  if (! margin_on) {
    text_glob    *w;
    graphic_glob *g;

    // remove margin

    right_margin_indent = 0;

    if (! page_contents->words.is_empty()) {

      // firstly check the words right margin

      page_contents->words.start_from_head();
      do {
	w = page_contents->words.get_data();
	if ((w->maxh >= 0) && (w->maxh > right_margin_indent)) {
	  right_margin_indent = w->maxh;
#if 0
	  if (right_margin_indent == 950) stop();
#endif
	}
	page_contents->words.move_right();
      } while (! page_contents->words.is_equal_to_head());
    }

    if (! page_contents->lines.is_empty()) {
      // now check for diagrams for right margin
      page_contents->lines.start_from_head();
      do {
	g = page_contents->lines.get_data();
	if ((g->maxh >= 0) && (g->maxh > right_margin_indent)) {
	  right_margin_indent = g->maxh;
#if 0
	  if (right_margin_indent == 950) stop();
#endif
	}
	page_contents->lines.move_right();
      } while (! page_contents->lines.is_equal_to_head());
    }

    // now we know the right margin lets do the same to find left margin

    left_margin_indent  = right_margin_indent;

    if (! page_contents->words.is_empty()) {
      do {
	w = page_contents->words.get_data();
	if ((w->minh >= 0) && (w->minh < left_margin_indent)) {
	  left_margin_indent = w->minh;
	}
	page_contents->words.move_right();
      } while (! page_contents->words.is_equal_to_head());
    }

    if (! page_contents->lines.is_empty()) {
      // now check for diagrams
      page_contents->lines.start_from_head();
      do {
	g = page_contents->lines.get_data();
	if ((g->minh >= 0) && (g->minh < left_margin_indent)) {
	  left_margin_indent = g->minh;
	}
	page_contents->lines.move_right();
      } while (! page_contents->lines.is_equal_to_head());
    }
  }
}


/*
 *  calculate_region - runs through the graphics globs and text globs
 *                     and ensures that all graphic routines
 *                     are defined by the region lists.
 *                     This then allows us to easily
 *                     determine the range of vertical and
 *                     horizontal boundaries for pictures,
 *                     tbl's and eqn's.
 *
 */

void page::calculate_region (void)
{
  graphic_glob *g;

  if (! lines.is_empty()) {
    lines.start_from_head();
    do {
      g = lines.get_data();
      if (! is_in_region(g)) {
	if (! can_grow_region(g)) {
	  make_new_region(g);
	}
      }
      lines.move_right();
    } while (! lines.is_equal_to_head());
  }
}

/*
 *  remove_redundant_regions - runs through the regions and ensures that
 *                             all are needed. This is required as
 *                             a picture may be empty, or EQ EN pair
 *                             maybe empty.
 */

void html_printer::remove_redundant_regions (void)
{
  region_glob  *r;
  graphic_glob *g;
  
  // firstly run through the region making sure that all are needed
  // ie all contain a line or word
  if (! page_contents->regions.is_empty()) {
    page_contents->regions.start_from_tail();
    do {
      r = page_contents->regions.get_data();
      calculate_region_margins(r);
      if (page_contents->has_line(r) || page_contents->has_word(r)) {
	page_contents->regions.move_right();
      } else {
	page_contents->regions.sub_move_right();
      }
    } while ((! page_contents->regions.is_empty()) &&
	     (! page_contents->regions.is_equal_to_tail()));
  }
}

void html_printer::display_regions (void)
{
  if (debug_table_on) {
    region_glob  *r;  

    fprintf(stderr, "==========s t a r t===========\n");
    if (! page_contents->regions.is_empty()) {
      page_contents->regions.start_from_head();
      do {
	r = page_contents->regions.get_data();
	fprintf(stderr, "region minv %d  maxv %d\n", r->minv, r->maxv);
	page_contents->regions.move_right();      
      } while (! page_contents->regions.is_equal_to_head());
    }
    fprintf(stderr, "============e n d=============\n");
    fflush(stderr);
  }
}

/*
 *  remove_duplicate_regions - runs through the regions and ensures that
 *                             no duplicates exist.
 */

void html_printer::remove_duplicate_regions (void)
{
  region_glob  *r;
  region_glob  *l=0;

  if (! page_contents->regions.is_empty()) {
    page_contents->regions.start_from_head();
    l = page_contents->regions.get_data();
    page_contents->regions.move_right();
    r = page_contents->regions.get_data();
    if (l != r) {
      do {
	r = page_contents->regions.get_data();
	// we have a legit region so we check for an intersection
	if (is_intersection(r->minv, r->minv, l->minv, l->maxv) &&
	    is_intersection(r->minh, r->maxh, l->minh, l->maxh)) {
	  l->minv = min(r->minv, l->minv);
	  l->maxv = max(r->maxv, l->maxv);
	  l->minh = min(r->minh, l->minh);
	  l->maxh = max(r->maxh, l->maxh);
	  calculate_region_margins(l);
	  page_contents->regions.sub_move_right();
	} else {
	  l = r;
	  page_contents->regions.move_right();
	}
      } while ((! page_contents->regions.is_empty()) &&
	       (! page_contents->regions.is_equal_to_head()));
    }
  }
}

int page::has_line (region_glob *r)
{
  graphic_glob *g;

  if (! lines.is_empty()) {
    lines.start_from_head();
    do {
      g = lines.get_data();
      if (is_subsection(g->minv, g->maxv, r->minv, r->maxv) &&
	  is_subsection(g->minh, g->maxh, r->minh, r->maxh)) {
	return( TRUE );
      }
      lines.move_right();
    } while (! lines.is_equal_to_head());
  }
  return( FALSE );
}


int page::has_word (region_glob *r)
{
  text_glob *g;

  if (! words.is_empty()) {
    words.start_from_head();
    do {
      g = words.get_data();
      if (is_subsection(g->minv, g->maxv, r->minv, r->maxv) &&
	  is_subsection(g->minh, g->maxh, r->minh, r->maxh)) {
	return( TRUE );
      }
      words.move_right();
    } while (! words.is_equal_to_head());
  }
  return( FALSE );
}


void html_printer::calculate_region_margins (region_glob *r)
{
  text_glob    *w;
  graphic_glob *g;

  r->minh = right_margin_indent;
  r->maxh = left_margin_indent;
  
  if (! page_contents->lines.is_empty()) {
    page_contents->lines.start_from_head();
    do {
      g = page_contents->lines.get_data();
      if (is_subsection(g->minv, g->maxv, r->minv, r->maxv)) {
	r->minh = min(r->minh, g->minh);
	r->maxh = max(r->maxh, g->maxh);
      }
      page_contents->lines.move_right();
    } while (! page_contents->lines.is_equal_to_head());
  }
  if (! page_contents->words.is_empty()) {
    page_contents->words.start_from_head();
    do {
      w = page_contents->words.get_data();
      if (is_subsection(w->minv, w->maxv, r->minv, r->maxv)) {
	r->minh = min(r->minh, w->minh);
	r->maxh = max(r->maxh, w->maxh);
      }
      page_contents->words.move_right();
    } while (! page_contents->words.is_equal_to_head());
  }
}


int page::is_in_region (graphic_glob *g)
{
  region_glob *r;

  if (! regions.is_empty()) {
    regions.start_from_head();
    do {
      r = regions.get_data();
      if (is_subsection(g->minv, g->maxv, r->minv, r->maxv) &&
	  is_subsection(g->minh, g->maxh, r->minh, r->maxh)) {
	return( TRUE );
      }
      regions.move_right();
    } while (! regions.is_equal_to_head());
  }
  return( FALSE );
}


/*
 *  no_raw_commands - returns TRUE if no html raw commands exist between
 *                    minv and maxv.
 */

int page::no_raw_commands (int minv, int maxv)
{
  text_glob *g;

  if (! words.is_empty()) {
    words.start_from_head();
    do {
      g = words.get_data();
      if ((g->is_raw_command) && (g->is_html_command) &&
	  (is_intersection(g->minv, g->maxv, minv, maxv))) {
	return( FALSE );
      }
      words.move_right();
    } while (! words.is_equal_to_head());
  }
  return( TRUE );
}

/*
 *  can_grow_region - returns TRUE if a region exists which can be extended
 *                    to include graphic_glob *g. The region is extended.
 */

int page::can_grow_region (graphic_glob *g)
{
  region_glob *r;
  int          quarter_inch=font::res/4;

  if (! regions.is_empty()) {
    regions.start_from_head();
    do {
      r = regions.get_data();
      // must prevent grohtml from growing a region through a html raw command
      if (is_intersection(g->minv, g->maxv, r->minv, r->maxv+quarter_inch) &&
	  (no_raw_commands(r->minv, r->maxv+quarter_inch))) {
#if defined(DEBUGGING)
	stop();
	printf("r minh=%d  minv=%d  maxh=%d  maxv=%d\n",
	       r->minh, r->minv, r->maxh, r->maxv);
	printf("g minh=%d  minv=%d  maxh=%d  maxv=%d\n",
	       g->minh, g->minv, g->maxh, g->maxv);
#endif
	r->minv = min(r->minv, g->minv);
	r->maxv = max(r->maxv, g->maxv);
	r->minh = min(r->minh, g->minh);
	r->maxh = max(r->maxh, g->maxh);
#if defined(DEBUGGING)
	printf("           r minh=%d  minv=%d  maxh=%d  maxv=%d\n",
	       r->minh, r->minv, r->maxh, r->maxv);
#endif
	return( TRUE );
      }
      regions.move_right();
    } while (! regions.is_equal_to_head());
  }
  return( FALSE );
}


/*
 *  make_new_region - creates a new region to contain, g.
 */

void page::make_new_region (graphic_glob *g)
{
  region_glob *r=new region_glob;

  r->minv = g->minv;
  r->maxv = g->maxv;
  r->minh = g->minh;
  r->maxv = g->maxv;
  regions.add(r);
}


void html_printer::dump_page(void)
{
  text_glob *g;

  printf("\n\ndebugging start\n");
  page_contents->words.start_from_head();
  do {
    g = page_contents->words.get_data();
    printf("%s ", g->text_string);
    page_contents->words.move_right();
  } while (! page_contents->words.is_equal_to_head());
  printf("\ndebugging end\n\n");
}


/*
 *  traverse_page_regions - runs through the regions in current_page
 *                          and generate html for text, and troff output
 *                          for all graphics.
 */

void html_printer::traverse_page_regions (void)
{
  region_glob *r;

  start_region_vpos =  0;
  start_region_hpos =  0;
  end_region_vpos   = -1;
  end_region_hpos   = -1;

  if (! page_contents->regions.is_empty()) {
    page_contents->regions.start_from_head();
    do {
      r = page_contents->regions.get_data();
      if (r->minv > 0) {
	end_region_vpos = r->minv-1;
      } else {
	end_region_vpos = 0;
      }
      end_region_hpos = -1;
      display_globs(TRUE);
      calculate_region_margins(r);
      start_region_vpos = end_region_vpos;
      end_region_vpos   = r->maxv;
      start_region_hpos = r->minh;
      end_region_hpos   = r->maxh;
      display_globs(FALSE);
      start_region_vpos = end_region_vpos+1;
      start_region_hpos = 0;
      page_contents->regions.move_right();
    } while (! page_contents->regions.is_equal_to_head());
    start_region_vpos = end_region_vpos+1;
    start_region_hpos = 0;
    end_region_vpos = -1;
    end_region_hpos = -1;
  }
  display_globs(TRUE);
}

int html_printer::is_within_region (text_glob *t)
{
  int he, ve, hs;

  if (start_region_hpos == -1) {
    hs = t->minh;
  } else {
    hs = start_region_hpos;
  }
  if (end_region_vpos == -1) {
    ve = t->maxv;
  } else {
    ve = end_region_vpos;
  }
  if (end_region_hpos == -1) {
    he = t->maxh;
  } else {
    he = end_region_hpos;
  }
  return( is_subsection(t->minv, t->maxv, start_region_vpos, ve) &&
	  is_subsection(t->minh, t->maxh, hs, he) );
}

int html_printer::is_within_region (graphic_glob *g)
{
  int he, ve, hs;

  if (start_region_hpos == -1) {
    hs = g->minh;
  } else {
    hs = start_region_hpos;
  }
  if (end_region_vpos == -1) {
    ve = g->maxv;
  } else {
    ve = end_region_vpos;
  }
  if (end_region_hpos == -1) {
    he = g->maxh;
  } else {
    he = end_region_hpos;
  }
  return( is_subsection(g->minv, g->maxv, start_region_vpos, ve) &&
	  is_subsection(g->minh, g->maxh, hs, he) );
}

int html_printer::is_less (graphic_glob *g, text_glob *t)
{
  return( (g->minv < t->minv) || ((g->minv == t->minv) && (g->minh < t->minh)) );
}

static FILE *create_file (char *filename)
{
  FILE *f;

  errno = 0;
  f = fopen(filename, "w");
  if (f == 0) {
    error("can't create `%1'", filename);
    return( 0 );
  } else {
    return( f );
  }
}

void html_printer::convert_to_image (char *name)
{
  char buffer[1024];

  sprintf(buffer, "grops %s > %s.ps\n", name, name);
  if (debug_on) {
    fprintf(stderr, "%s", buffer);
  }
  system(buffer);

  if (image_type == gif) {
    sprintf(buffer,
	    "echo showpage | gs -q -dSAFER -sDEVICE=ppmraw -r%d -g%dx%d -sOutputFile=- %s.ps - | ppmquant 256  2> /dev/null | ppmtogif  2> /dev/null > %s.gif \n",
	    image_res,
	    (end_region_hpos-start_region_hpos)*image_res/font::res+IMAGE_BOARDER_PIXELS,
	    (end_region_vpos-start_region_vpos)*image_res/font::res+IMAGE_BOARDER_PIXELS,
	    name, image_name);
  } else {
    sprintf(buffer,
	    "echo showpage | gs -q -dSAFER -sDEVICE=%s -r%d -g%dx%d -sOutputFile=- %s.ps - 2> /dev/null > %s.png \n",
	    image_device,
	    image_res,
	    (end_region_hpos-start_region_hpos)*image_res/font::res+IMAGE_BOARDER_PIXELS,
	    (end_region_vpos-start_region_vpos)*image_res/font::res+IMAGE_BOARDER_PIXELS,
	    name, image_name);
  }
  if (debug_on) {
    fprintf(stderr, "%s", buffer);
  }
  system(buffer);
  sprintf(buffer, "/bin/rm -f %s %s.ps\n", name, name);
  if (debug_on) {
    fprintf(stderr, "%s", buffer);
  } else {
    system(buffer);
  }
}

void html_printer::prologue (void)
{
  troff.put_string("x T ps\nx res ");
  troff.put_number(postscript_res);
  troff.put_string(" 1 1\nx init\np1\n");
}

void html_printer::create_temp_name (char *name, char *extension)
{
  make_new_image_name();
  sprintf(name, "/tmp/%s.%s", image_name, extension);
}

void html_printer::display_globs (int is_to_html)
{
  text_glob    *t=0;
  graphic_glob *g=0;
  FILE         *f=0;
  char          name[MAX_TEMP_NAME];
  char          buffer[1024];
  int           r=font::res;
  int           something=FALSE;
  int           is_center=FALSE;

  end_paragraph();

  if (! is_to_html) {
    is_center = html_position_region();
    create_temp_name(name, "troff");
    f = create_file(name);
    troff.set_file(f);
    prologue();
    output_style.f = 0;
  }
  if (! page_contents->words.is_empty()) {
    page_contents->words.start_from_head();
    t = page_contents->words.get_data();
  }

  if (! page_contents->lines.is_empty()) {
    page_contents->lines.start_from_head();
    g = page_contents->lines.get_data();
  }

  do {
#if 0
    if ((t != 0) && (strcmp(t->text_string, "(1.a)") == 0)) {
      stop();
    }
#endif
    if ((t == 0) && (g != 0)) {
      if (is_within_region(g)) {
	something = TRUE;
	display_line(g, is_to_html);
      }
      if (page_contents->lines.is_empty() || page_contents->lines.is_equal_to_tail()) {
	g = 0;
      } else {
	g = page_contents->lines.move_right_get_data();
      }
    } else if ((g == 0) && (t != 0)) {
      if (is_within_region(t)) {
	display_word(t, is_to_html);
	something = TRUE;
      }
      if (page_contents->words.is_empty() || page_contents->words.is_equal_to_tail()) {
	t = 0;
      } else {
	t = page_contents->words.move_right_get_data();
      }
    } else {
      if ((g == 0) || (t == 0)) {
	// hmm nothing to print out...
      } else if (is_less(g, t)) {
	if (is_within_region(g)) {
	  display_line(g, is_to_html);
	  something = TRUE;
	}
	if (page_contents->lines.is_empty() || page_contents->lines.is_equal_to_tail()) {
	  g = 0;
	} else {
	  g = page_contents->lines.move_right_get_data();
	}
      } else {
	if (is_within_region(t)) {
	  display_word(t, is_to_html);
	  something = TRUE;
	}
	if (page_contents->words.is_empty() || page_contents->words.is_equal_to_tail()) {
	  t = 0;
	} else {
	  t = page_contents->words.move_right_get_data();
	}
      }
    }
  } while ((t != 0) || (g != 0));

  if ((! is_to_html) && (f != 0)) {
    fclose(troff.get_file());
    if (something) {
      convert_to_image(name);

      if (is_center) {
	begin_paragraph(center_alignment);
      } else {
	begin_paragraph(left_alignment);
      }
      force_begin_paragraph();
      html.put_string("<img src=\"");
      html.put_string(image_name);
      if (image_type == gif) {
	html.put_string(".gif\"");
      } else {
	html.put_string(".png\"");
      }
      if (is_center) {
	html.put_string(" align=\"middle\"");
      }
      html.put_string(">\n");
      html_newline();
      end_paragraph();

      output_vpos      = end_region_vpos;
      output_hpos      = 0;
      need_one_newline = FALSE;
      output_style.f   = 0;
    }
    // unlink(name);  // remove troff file
  }
}

void html_printer::flush_page (void)
{
  calculate_margin();
  output_vpos = -1;
  output_hpos = left_margin_indent;
#if 0
  dump_page();
#endif
  html.begin_comment("left  margin: ").comment_arg(itoa(left_margin_indent)).end_comment();;
  html.begin_comment("right margin: ").comment_arg(itoa(right_margin_indent)).end_comment();;
  remove_redundant_regions();
  page_contents->calculate_region();
  remove_duplicate_regions();
  find_title();

  traverse_page_regions();
  terminate_current_font();
  if (need_one_newline) {
    html_newline();
  }
  end_paragraph();
  
  // move onto a new page
  delete page_contents;
  page_contents = new page;
}

static int convertSizeToHTML (int size)
{
  if (size < 6) {
    return( 0 );
  } else if (size < 8) {
    return( 1 );
  } else if (size < 10) {
    return( 2 );
  } else if (size < 12) {
    return( 3 );
  } else if (size < 14) {
    return( 4 );
  } else if (size < 16) {
    return( 5 );
  } else if (size < 18) {
    return( 6 );
  } else {
    return( 7 );
  }
}


void html_printer::write_html_font_face (const char *fontname, const char *left, const char *right)
{
  switch (fontname[0]) {

  case 'C':  html.put_string(left) ; html.put_string("tt"); html.put_string(right);
             break;
  case 'H':  break;
  case 'T':  break;
  default:   break;
  }
}


void html_printer::write_html_font_type (const char *fontname, const char *left, const char *right)
{
  if (strcmp(&fontname[1], "B") == 0) {
    html.put_string(left) ; html.put_string("B"); html.put_string(right);
  } else if (strcmp(&fontname[1], "I") == 0) {
    html.put_string(left) ; html.put_string("I"); html.put_string(right);
  } else if (strcmp(&fontname[1], "BI") == 0) {
    html.put_string(left) ; html.put_string("EM"); html.put_string(right);
  }
}


void html_printer::html_change_font (text_glob *g, const char *fontname, int size)
{
  char        buffer[1024];

  if (output_style.f != 0) {
    const char       *oldfontname = output_style.f->get_name();

    // firstly terminate the current font face and type
    if ((oldfontname != 0) && (oldfontname != fontname)) {
      write_html_font_face(oldfontname, "</", ">");
      write_html_font_type(oldfontname, "</", ">");
    }
  }
  if (fontname != 0) {
    // now emit the size if it has changed
    if (((output_style.f == 0) || (output_style.point_size != size)) && (size != 0)) {
      sprintf(buffer, "<font size=%d>", convertSizeToHTML(size));
      html.put_string(buffer);
      output_style.point_size = size;  // and remember the size
    }

    if (! g->is_raw_command) {
      // now emit the new font
      write_html_font_face(fontname, "<", ">");
  
      // now emit the new font type
      write_html_font_type(fontname, "<", ">");

      output_style = g->text_style;  // remember style for next time
    }
  } else {
    output_style.f = 0;   // no style at present
  }
}


void html_printer::change_font (text_glob *g, int is_to_html)
{
  if (is_to_html) {
    if (output_style != g->text_style) {
      const char *fontname=0;
      int   size=0;

      if (g->text_style.f != 0) {
	fontname = g->text_style.f->get_name();
	size     = (font::res/(72*font::sizescale))*g->text_style.point_size;

	html_change_font(g, fontname, size);
      } else {
	html_change_font(g, fontname, size);
      }
    }
  } else {
    // is to troff
    if (output_style != g->text_style) {
      if (g->text_style.f != 0) {
	const char *fontname = g->text_style.f->get_name();
	int size             = (font::res/(72*font::sizescale))*g->text_style.point_size;

	if (fontname == 0) {
	  fatal("no internalname specified for font");
	}

	troff_change_font(fontname, size, g->text_style.font_no);
	output_style = g->text_style;  // remember style for next time
      }
    }
  }
}


/*
 *  is_bold - returns TRUE if the text inside, g, is using a bold face.
 *            It returns FALSE is g contains a raw html command, even if this uses
 *            a bold font.
 */

int html_printer::is_bold (text_glob *g)
{
  if (g->text_style.f == 0) {
    // unknown font
    return( FALSE );
  } else if (g->is_raw_command) {
    return( FALSE );
  } else {
    const char *fontname = g->text_style.f->get_name();
    
    if (strlen(fontname) >= 2) {
      return( fontname[1] == 'B' );
    } else {
      return( FALSE );
    }
  }
}

void html_printer::terminate_current_font (void)
{
  text_glob g;

  // we create a dummy glob just so we can tell html_change_font not to start up
  // a new font
  g.is_raw_command = TRUE;
  html_change_font(&g, 0, 0);   
}

void html_printer::write_header (void)
{
  if (strlen(header.header_buffer) > 0) {
    if (header.header_level > 7) {
      header.header_level = 7;
    }

    if (cutoff_heading+2 > header.header_level) {
      // firstly we must terminate any font and type faces
      terminate_current_font();
      end_paragraph();

      // secondly we generate a tag
      html.put_string("<a name=\"");
      html.put_string(header.header_buffer);
      html.put_string("\"></a>");
      // now we save the header so we can issue a list of link
      style st;

      header.no_of_headings++;

      text_glob *g=new text_glob(&st,
				 header.headings.add_string(header.header_buffer, strlen(header.header_buffer)),
				 strlen(header.header_buffer),
				 header.no_of_headings, header.header_level,
				 header.no_of_headings, header.header_level,
				 FALSE, FALSE);
      header.headers.add(g);   // and add this header to the header list
    }

    end_paragraph();
    // and now we issue the real header
    html.put_string("<h");
    html.put_number(header.header_level);
    html.put_string(">");
    html.put_string(header.header_buffer);
    html.put_string("</h");
    html.put_number(header.header_level);
    html.put_string(">");
    need_one_newline = FALSE;
    begin_paragraph(left_alignment);
    header.written_header = TRUE;
  }
}

/*
 *  write_headings - emits a list of links for the headings in this document
 */

void header_desc::write_headings (FILE *f)
{
  text_glob *g;

  if (! headers.is_empty()) {
    headers.start_from_head();
    do {
      g = headers.get_data();
      fprintf(f, "<a href=\"#%s\">%s</a><br>\n", g->text_string, g->text_string);
      headers.move_right();
    } while (! headers.is_equal_to_head());
  }
}

void html_printer::determine_header_level (void)
{
  int i;
  int l=strlen(header.header_buffer);
  int stops=0;

  for (i=0; ((i<l) && ((header.header_buffer[i] == '.') || is_digit(header.header_buffer[i]))) ; i++) {
    if (header.header_buffer[i] == '.') {
      stops++;
    }
  }
  if (stops > 0) {
    header.header_level = stops;
  }
}


void html_printer::build_header (text_glob *g)
{
  int r            = font::res;
  int height       = g->text_style.point_size*r/72;
  text_glob *l;
  int current_vpos;

  strcpy(header.header_buffer, "");
  do {
    l = g;
    current_vpos = g->minv;
    strcat(header.header_buffer, (char *)g->text_string);
    page_contents->words.move_right();
    g = page_contents->words.get_data();
    if (g->minv == current_vpos) {
      strcat(header.header_buffer, " ");
    }
  } while ((! page_contents->words.is_equal_to_head()) &&
	   ((g->minv == current_vpos) || (l->maxh == right_margin_indent)));

  determine_header_level();
  // finally set the output to neutral for after the header

  g = page_contents->words.get_data();
  output_vpos = g->minv;                // set output_vpos to the next line since
  output_hpos = left_margin_indent;     // html header forces a newline anyway
  page_contents->words.move_left();     // so that next time we use old g

  need_one_newline = FALSE;
}


/*
 *  is_whole_line_bold - returns TRUE if the whole line is bold.
 */

int html_printer::is_whole_line_bold (text_glob *g)
{
  text_glob *n=g;
  int        current_vpos=g->minv;

  do {
    if (is_bold(n)) {
      page_contents->words.move_right();
      n = page_contents->words.get_data();
    } else {
      while (page_contents->words.get_data() != g) {
	page_contents->words.move_left();
      }
      return( FALSE );
    }
  } while ((! page_contents->words.is_equal_to_head()) && (is_on_same_line(n, current_vpos)));
  // was (n->minv == current_vpos)
  while (page_contents->words.get_data() != g) {
    page_contents->words.move_left();
  }
  return( TRUE );
}


/*
 *  is_a_header - returns TRUE if the whole sequence of contineous lines are bold.
 *                It checks to see whether a line is likely to be contineous and
 *                then checks that all words are bold.
 */

int html_printer::is_a_header (text_glob *g)
{
  text_glob *l;
  text_glob *n=g;
  int        current_vpos;

  do {
    l = n;
    current_vpos = n->minv;
    if (is_bold(n)) {
      page_contents->words.move_right();
      n = page_contents->words.get_data();
    } else {
      while (page_contents->words.get_data() != g) {
	page_contents->words.move_left();
      }
      return( FALSE );
    }
  } while ((! page_contents->words.is_equal_to_head()) &&
	   ((n->minv == current_vpos) || (l->maxh == right_margin_indent)));
  while (page_contents->words.get_data() != g) {
    page_contents->words.move_left();
  }
  return( TRUE );
}


int html_printer::processed_header (text_glob *g)
{
  if ((guess_on) && (g->minh == left_margin_indent) && (! using_table_for_indent()) &&
      (is_a_header(g))) {
    build_header(g);
    write_header();
    return( TRUE );
  } else {
    return( FALSE );
  }
}

int is_punctuation (char *s, int length)
{
  return( (length == 1) &&
	  ((s[0] == '(') || (s[0] == ')') || (s[0] == '!') || (s[0] == '.') || (s[0] == '[') ||
	   (s[0] == ']') || (s[0] == '?') || (s[0] == ',') || (s[0] == ';') || (s[0] == ':') ||
	   (s[0] == '@') || (s[0] == '#') || (s[0] == '$') || (s[0] == '%') || (s[0] == '^') ||
	   (s[0] == '&') || (s[0] == '*') || (s[0] == '+') || (s[0] == '-') || (s[0] == '=') ||
	   (s[0] == '{') || (s[0] == '}') || (s[0] == '|') || (s[0] == '\"') || (s[0] == '\''))
	  );
}

/*
 *  move_horizontal - moves right into the position, g->minh.
 */

void html_printer::move_horizontal (text_glob *g, int left_margin)
{
  if (g->text_style.f != 0) {
    int w = g->text_style.f->get_space_width(g->text_style.point_size);

    if (w == 0) {
      fatal("space width is zero");
    }
    if ((output_hpos == left_margin) && (g->minh > output_hpos)) {
      make_html_indent(g->minh-output_hpos);
    } else {
      emit_space(g, FALSE);
    }
    output_hpos = g->maxh;
    output_vpos = g->minv;

    change_font(g, TRUE);
  }
}

int html_printer::looks_like_subscript (text_glob *g)
{
  return(((output_vpos < g->minv) && (output_style.point_size != 0) &&
	  (output_style.point_size > g->text_style.point_size)));
}


int html_printer::looks_like_superscript (text_glob *g)
{
  return(((output_vpos > g->minv) && (output_style.point_size != 0) &&
	  (output_style.point_size > g->text_style.point_size)));
}

/*
 *  pretend_is_on_same_line - returns TRUE if we think, g, is on the same line as the previous glob.
 *                            Note that it believes a single word spanning the left..right as being
 *                            on a different line.
 */

int html_printer::pretend_is_on_same_line (text_glob *g, int left_margin, int right_margin)
{
  return( auto_on && (right_margin == output_hpos) && (left_margin == g->minh) &&
	  (right_margin != g->maxh) && ((! is_whole_line_bold(g)) || (g->text_style.f == output_style.f)) );
}

int html_printer::is_on_same_line (text_glob *g, int vpos)
{
  return(
	 (vpos >= 0) &&
	 is_intersection(vpos, vpos+g->text_style.point_size*font::res/72-1, g->minv, g->maxv)
        );
}


/*
 *  make_html_indent - creates a relative indentation.
 */

void html_printer::make_html_indent (int indent)
{
  int  r=font::res;

  html.put_string("<span style=\" text-indent: ");
  html.put_float(((double)(indent)/((double)r)));
  html.put_string("in;\"></span>");
}

/*
 *  using_table_for_indent - returns TRUE if we currently using a table for indentation
 *                           purposes.
 */

int html_printer::using_table_for_indent (void)
{
  return( indentation.no_of_columns != 0 );
}

/*
 *  calculate_min_gap - returns the minimum gap by which we deduce columns.
 *                      This is a rough heuristic.
 */

int html_printer::calculate_min_gap (text_glob *g)
{
  return( g->text_style.f->get_space_width(g->text_style.point_size)*GAP_SPACES );
}

/*
 *  collect_columns - place html text in a column and return the vertical limit reached.
 */

int html_printer::collect_columns (struct text_defn *line, struct text_defn *last, int max_words)
{
  text_glob *start = page_contents->words.get_data();
  text_glob *t     = start;
  int  upper_limit = 0;

  line[0].left  = 0;
  line[0].right = 0;
  if (start != 0) {
    int graphic_limit = end_region_vpos;

    if (is_whole_line_bold(t) && (t->minh == left_margin_indent)) {
      // found header therefore terminate indentation table
      upper_limit = -t->minv;  // so we know a header has stopped the column
    } else {
      int i      =0;
      int j      =0;
      int prevh  =0;
      int mingap =calculate_min_gap(start);

      while ((t != 0) && (is_on_same_line(t, start->minv) && (i<max_words)) &&
	     ((graphic_limit == -1) || (graphic_limit > t->minv))) {
	while ((last != 0) && (j<max_words) && (last[j].left != 0) && (last[j].left < t->minh)) {
	  j++;
	}
	// t->minh might equal t->maxh when we are passing a special device character via \X
	// we currently ignore these when considering tables
	if (((t->minh - prevh >= mingap) || ((last != 0) && (last[j].left != 0) && (t->minh == last[j].left))) &&
	    (t->minh != t->maxh)) {
	  line[i].left    = t->minh;
	  line[i].right   = t->maxh;
	  i++;
	} else if (i>0) {
	  line[i-1].right = t->maxh;
	}

	// and record the vertical upper limit
	upper_limit = max(t->minv, upper_limit);

	prevh = t->maxh;
	page_contents->words.move_right();
	t = page_contents->words.get_data();
	if (page_contents->words.is_equal_to_head()) {
	  t = 0;
	}
      }

      if (i<max_words) {
	line[i].left  = 0;
	line[i].right = 0;
      }
    }
  }
  return( upper_limit );
}

/*
 *  conflict_with_words - returns TRUE if a word sequence crosses a column.
 */

int html_printer::conflict_with_words (struct text_defn *column_guess, struct text_defn *words)
{
  int i=0;
  int j;

  while ((column_guess[i].left != 0) && (i<MAX_WORDS_PER_LINE)) {
    j=0;
    while ((words[j].left != 0) && (j<MAX_WORDS_PER_LINE)) {
      if ((words[j].left <= column_guess[i].right) && (i+1<MAX_WORDS_PER_LINE) &&
	  (column_guess[i+1].left != 0) && (words[j].right >= column_guess[i+1].left)) {
	if (debug_table_on) {
	  fprintf(stderr, "is a conflict with words\n");
	  fflush(stderr);
	}
	return( TRUE );
      }
      j++;
    }
    i++;
  }
  if (debug_table_on) {
    fprintf(stderr, "is NOT a conflict with words\n");
    fflush(stderr);
  }
  return( FALSE );
}

/*
 *  combine_line - combines dest and src.
 */

void html_printer::combine_line (struct text_defn *dest, struct text_defn *src)
{
  int i;

  for (i=0; (i<MAX_WORDS_PER_LINE) && (src[i].left != 0); i++) {
    include_into_list(dest, &src[i]);
  }
  remove_redundant_columns(dest);
}

/*
 *  remove_entry_in_line - removes an entry, j, in, line.
 */

void html_printer::remove_entry_in_line (struct text_defn *line, int j)
{
  while (line[j].left != 0) {
    line[j].left  = line[j+1].left;
    line[j].right = line[j+1].right;
    j++;
  }
}

/*
 *  remove_redundant_columns - searches through the array columns and removes any redundant entries.
 */

void html_printer::remove_redundant_columns (struct text_defn *line)
{
  int i=0;
  int j=0;

  while (line[i].left != 0) {
    if ((i<MAX_WORDS_PER_LINE) && (line[i+1].left != 0)) {
      j = 0;
      while ((j<MAX_WORDS_PER_LINE) && (line[j].left != 0)) {
	if ((j != i) && (is_intersection(line[i].left, line[i].right, line[j].left, line[j].right))) {
	  line[i].left  = min(line[i].left , line[j].left);
	  line[i].right = max(line[i].right, line[j].right);
	  remove_entry_in_line(line, j);
	} else {
	  j++;
	}
      }
    }
    i++;
  }
}

/*
 *  include_into_list - performs an order set inclusion
 */

void html_printer::include_into_list (struct text_defn *line, struct text_defn *item)
{
  int i=0;

  while ((i<MAX_WORDS_PER_LINE) && (line[i].left != 0) && (line[i].left<item->left)) {
    i++;
  }

  if (line[i].left == 0) {
    // add to the end
    if (i<MAX_WORDS_PER_LINE) {
      if ((i>0) && (line[i-1].left > item->left)) {
	fatal("insertion error");
      }
      line[i].left  = item->left;
      line[i].right = item->right;
      i++;
      line[i].left  = 0;
      line[i].right = 0;
    }
  } else {
    if (line[i].left == item->left) {
      line[i].right = max(item->right, line[i].right);
    } else {
      // insert
      int left  = item->left;
      int right = item->right;
      int l     = line[i].left;
      int r     = line[i].right;

      while ((i+1<MAX_WORDS_PER_LINE) && (line[i].left != 0)) {
	line[i].left  = left;
	line[i].right = right;
	i++;
	left          = l;
	right         = r;
	l             = line[i].left;
	r             = line[i].right;
      }
      if (i+1<MAX_WORDS_PER_LINE) {
	line[i].left    = left;
	line[i].right   = right;
	line[i+1].left  = 0;
	line[i+1].right = 0;
      }
    }
  }
}

/*
 *  is_in_column - return TRUE if value is present in line.
 */

int html_printer::is_in_column (struct text_defn *line, struct text_defn *item, int max_words)
{
  int i=0;

  while ((i<max_words) && (line[i].left != 0)) {
    if (line[i].left == item->left) {
      return( TRUE );
    } else {
      i++;
    }
  }
  return( FALSE );
}

/*
 *  calculate_right - calculate the right most margin for each column in line.
 */

void html_printer::calculate_right (struct text_defn *line, int max_words)
{
  int i=0;

  while ((i<max_words) && (line[i].left != 0)) {
    if (i>0) {
      line[i-1].right = line[i].left;
    }
    i++;
  }
}

/*
 *  add_right_full_width - adds an extra column to the right to bring the table up to
 *                         full width.
 */

void html_printer::add_right_full_width (struct text_defn *line, int mingap)
{
  int i=0;

  while ((i<MAX_WORDS_PER_LINE) && (line[i].left != 0)) {
    i++;
  }

  if ((i>0) && (line[i-1].right != right_margin_indent) && (i+1<MAX_WORDS_PER_LINE)) {
    line[i].left    = min(line[i-1].right+mingap, right_margin_indent);
    line[i].right   = right_margin_indent;
    i++;
    if (i<MAX_WORDS_PER_LINE) {
      line[i].left  = 0;
      line[i].right = 0;
    }
  }
}

/*
 *  determine_right_most_column - works out the right most limit of the right most column.
 *                                Required as we might be performing a .2C and only
 *                                have enough text to fill the left column.
 */

void html_printer::determine_right_most_column (struct text_defn *line, int max_words)
{
  int i=0;

  while ((i<max_words) && (line[i].left != 0)) {
    i++;
  }
  if (i>0) {
    // remember right_margin_indent is the right most position for this page
    line[i-1].right = column_calculate_right_margin(line[i-1].left, right_margin_indent);
  }
}

/*
 *  is_column_match - returns TRUE if a word is aligned in the same horizontal alignment
 *                    between two lines, line1 and line2. If so then this horizontal
 *                    position is saved in match.
 */

int html_printer::is_column_match (struct text_defn *match,
				   struct text_defn *line1, struct text_defn *line2, int max_words)
{
  int i=0;
  int j=0;
  int found=FALSE;
  int first=(match[0].left==0);

  if (first) {
    struct text_defn   t;

    t.left  = left_margin_indent;
    t.right = 0;

    include_into_list(match, &t);
  }
  while ((line1[i].left != 0) && (line2[i].left != 0)) {
    if (line1[i].left == line2[j].left) {
      // same horizontal alignment found
      include_into_list(match, &line1[i]);
      i++;
      j++;
      found = TRUE;
    } else if (line1[i].left < line2[j].left) {
      i++;
    } else {
      j++;
    }
  }
  calculate_right(match, max_words);
  return( found );
}


/*
 *  remove_white_using_words - remove white space in, last_guess, by examining, next_line
 *                             placing results into next_guess.
 *                             It returns TRUE if the same columns exist in next_guess and last_guess
 *                             we do allow columns to shrink but if a column disappears then we return FALSE.
 */

int html_printer::remove_white_using_words (struct text_defn *next_guess,
					    struct text_defn *last_guess, struct text_defn *next_line)
{
  int i=0;
  int j=0;
  int k=0;
  int removed=FALSE;

  while ((last_guess[j].left != 0) && (next_line[k].left != 0)) {
    if (last_guess[j].left == next_line[k].left) {
      // same horizontal alignment found
      next_guess[i].left  = last_guess[j].left;
      next_guess[i].right = max(last_guess[j].right, next_line[k].right);
      i++;
      j++;
      k++;
      if ((next_guess[i-1].right > last_guess[j].left) && (last_guess[j].left != 0)) {
	removed = TRUE;
      }
    } else if (last_guess[j].right < next_line[k].left) {
      next_guess[i].left  = last_guess[j].left;
      next_guess[i].right = last_guess[j].right;
      i++;
      j++;
    } else if (last_guess[j].left > next_line[k].right) {
      // insert a word sequence from next_line[k]
      next_guess[i].left  = next_line[k].left;
      next_guess[i].right = next_line[k].right;
      i++;
      k++;
    } else if (is_intersection(last_guess[j].left, last_guess[j].right, next_line[k].left, next_line[k].right)) {
      // potential for a column disappearing
      next_guess[i].left  = min(last_guess[j].left , next_line[k].left);
      next_guess[i].right = max(last_guess[j].right, next_line[k].right);
      i++;
      j++;
      k++;
      if ((next_guess[i-1].right > last_guess[j].left) && (last_guess[j].left != 0)) {
	removed = TRUE;
      }
    }
  }
  if (i<MAX_WORDS_PER_LINE) {
    next_guess[i].left  = 0;
    next_guess[i].right = 0;
  }
  if (debug_table_on) {
    if (removed) {
      fprintf(stderr, "have removed column\n");
    } else {
      fprintf(stderr, "have NOT removed column\n");
    }
    fflush(stderr);
  }
  remove_redundant_columns(next_guess);
  return( removed );
}

/*
 *  count_columns - returns the number of elements inside, line.
 */

int html_printer::count_columns (struct text_defn *line)
{
  int i=0;

  while (line[i].left != 0) {
    i++;
  }
  return( i );
}

/*
 *  rewind_text_to - moves backwards until page_contents is looking at, g.
 */

void html_printer::rewind_text_to (text_glob *g)
{
  while (page_contents->words.get_data() != g) {
    if (page_contents->words.is_equal_to_head()) {
      page_contents->words.start_from_tail();
    } else {
      page_contents->words.move_left();
    }
  }
}

/*
 *  display_columns - a long overdue debugging function, as this column code is causing me grief :-(
 */

void html_printer::display_columns (const char *word, const char *name, text_defn *line)
{
  int i=0;

  fprintf(stderr, "[%s:%s]", name, word);
  while (line[i].left != 0) {
    fprintf(stderr, " <left=%d right=%d> ", line[i].left, line[i].right);
    i++;
  }
  fprintf(stderr, "\n");
  fflush(stderr);
}

/*
 *  copy_line - dest = src
 */

int html_printer::copy_line (struct text_defn *dest, struct text_defn *src)
{ 
  int k;

  for (k=0; ((src[k].left != 0) && (k<MAX_WORDS_PER_LINE)); k++) {
    dest[k].left  = src[k].left;
    dest[k].right = src[k].right;
  }
  if (k<MAX_WORDS_PER_LINE) {
    dest[k].left  = 0;
    dest[k].right = 0;
  }
}

/*
 *  add_column_gaps - adds empty columns between columns which don't exactly align
 */

void html_printer::add_column_gaps (struct text_defn *line)
{
  int i=0;
  struct text_defn t;

  // firstly lets see whether we need an initial column on the left hand side
  if ((line[0].left != left_margin_indent) && (line[0].left != 0) &&
      (left_margin_indent < line[0].left) && (is_worth_column(left_margin_indent, line[0].left))) {
    t.left  = left_margin_indent;
    t.right = line[0].left;
    include_into_list(line, &t);
  }

  while ((i<MAX_WORDS_PER_LINE) && (line[i].left != 0)) {
    if ((i+1<MAX_WORDS_PER_LINE) && (line[i+1].left != 0) && (line[i].right != line[i+1].left) &&
	(is_worth_column(line[i].right, line[i+1].left))) {
      t.left  = line[i].right;
      t.right = line[i+1].left;
      include_into_list(line, &t);
      i=0;
    } else {
      i++;
    }
  }
  // lastly lets see whether we need a final column on the right hand side
  if ((i>0) && (line[i-1].right != right_margin_indent) &&
      (is_worth_column(line[i-1].right, right_margin_indent))) {
    t.left  = line[i-1].right;
    t.right = right_margin_indent;
    include_into_list(line, &t);
  }
}

/*
 *  is_continueous_column - returns TRUE if a line has a word on one
 *                          of the last_col right most boundaries.
 */

int html_printer::is_continueous_column (text_defn *last_col, text_defn *next_line)
{
  int w = count_columns(next_line);
  int c = count_columns(last_col);
  int i, j;

  for (i=0; i<c; i++) {
    for (j=0; j<w; j++) {
      if (last_col[i].right == next_line[j].right) {
	return( TRUE );
      }
    }
  }
  return( FALSE );
}

/*
 *  is_exact_left - returns TRUE if a line has a word on one
 *                  of the last_col left most boundaries.
 */

int html_printer::is_exact_left (text_defn *last_col, text_defn *next_line)
{
  int w = count_columns(next_line);
  int c = count_columns(last_col);
  int i, j;

  for (i=0; i<c; i++) {
    for (j=0; j<w; j++) {
      if ((last_col[i].left == next_line[j].left) ||
	  (last_col[i].left != left_margin_indent)) {
	return( TRUE );
      }
    }
  }
  return( FALSE );
}

/*
 *  continue_searching_column - decides whether we should carry on searching text for a column.
 */

int html_printer::continue_searching_column (text_defn *next_col,
					     text_defn *last_col,
					     text_defn *all_words)
{
  int count = count_columns(next_col);
  int words = count_columns(all_words);

  if ((words == 0) || ((words == 1) &&
		       (all_words[0].left == left_margin_indent) &&
		       (all_words[0].right == right_margin_indent))) {
    // no point as we have now seen a full line of contineous text
    return( FALSE );
  }
  return( (count == count_columns(last_col)) &&
	  (last_col[0].left != left_margin_indent) || (last_col[0].right != right_margin_indent) );
}

/*
 *  is_worth_column - returns TRUE if the size of this column is worth defining.
 */

int html_printer::is_worth_column (int left, int right)
{
#if 0
  return( abs(right-left) >= MIN_COLUMN );
#endif
  return( TRUE );
}

/*
 *  large_enough_gap - returns TRUE if a large enough gap for one line was seen.
 *                     We need to make sure that a single line definitely warrents
 *                     a table.
 *                     It also removes other smaller gaps.
 */

int html_printer::large_enough_gap (text_defn *last_col)
{
  int i=0;
  int found=FALSE;
  int r=font::res;
  int gap=r/GAP_WIDTH_ONE_LINE;

  if (abs(last_col[i].left - left_margin_indent) >= gap) {
    found = TRUE;
  }
  while ((last_col[i].left != 0) && (last_col[i+1].left != 0)) {
    if (abs(last_col[i+1].left-last_col[i].right) >= gap) {
      found = TRUE;
      i++;
    } else {
      // not good enough for a single line, remove it
      if (i>0) {
	last_col[i-1].right = last_col[i].right;
      }
      remove_entry_in_line(last_col, i);
    }
  }
  return( found );
}

/*
 *  is_subset_of_columns - returns TRUE if line, a, is a subset of line, b.
 */

int html_printer::is_subset_of_columns (text_defn *a, text_defn *b)
{
  int i;
  int j;

  i=0;
  while ((i<MAX_WORDS_PER_LINE) && (a[i].left != 0)) {
    j=0;
    while ((j<MAX_WORDS_PER_LINE) && (b[j].left != 0) &&
	   ((b[j].left != a[i].left) || (b[j].right != a[i].right))) {
      j++;
    }
    if ((j==MAX_WORDS_PER_LINE) || (b[j].left == 0)) {
      // found a different column - not a subset
      return( FALSE );
    }
    i++;
  }
  return( TRUE );
}

/*
 *  count_hits - counts the number of hits per column. A hit is when the
 *               left hand position of a glob hits the left hand column.
 */

void html_printer::count_hits (text_defn *col)
{
  int        i;
  text_glob *start = page_contents->words.get_data();
  text_glob *g     = start;
  int        r     = font::res;
  int        gap   = r/GAP_WIDTH_ONE_LINE;
  int        n     = count_columns(col);
  int        left;

  // firstly reset the used field
  for (i=0; i<n; i++) {
    col[i].is_used = 0;
  }
  // now calculate the left hand hits
  while ((g != 0) && (g->minv <= indentation.vertical_limit)) {
    i=0;
    while ((col[i].left < g->minh) && (col[i].left != 0)) {
      i++;
    }
    if ((col[i].left == g->minh) && (col[i].left != 0)) {
      col[i].is_used++;
    }
    page_contents->words.move_right();
    if (page_contents->words.is_equal_to_head()) {
      g = 0;
      page_contents->words.start_from_tail();
    } else {
      g=page_contents->words.get_data();
    }
  }
  // now remove any column which is less than the
  // minimal gap for one hit.
  // column 0 is excempt

  left = col[0].left;
  i=1;
  while (i<count_columns(col)) {
    if (col[i].is_used == 1) {
      if (col[i].left - left < gap) {
	col[i-1].right = col[i].right;
	remove_entry_in_line(col, i);
	left = col[i].left;
      } else {
	left = col[i].left;
	i++;
      }
    } else {
      left = col[i].left;
      i++;
    }
  }
}

/*
 *  found_use_for_table - checks whether the some words on one line directly match
 *                        the horizontal alignment of the line below.
 */

int html_printer::found_use_for_table (text_glob *start)
{
  text_glob         *t;
  struct text_defn   all_words [MAX_WORDS_PER_LINE];
  struct text_defn   last_raw  [MAX_WORDS_PER_LINE];
  struct text_defn   next_line [MAX_WORDS_PER_LINE];
  struct text_defn   prev_guess[MAX_WORDS_PER_LINE];
  struct text_defn   last_guess[MAX_WORDS_PER_LINE];
  struct text_defn   next_guess[MAX_WORDS_PER_LINE];
  int                i     =0;
  int                lines =0;
  int                mingap=calculate_min_gap(start);
  int                limit;

#if 0
  if (strcmp(start->text_string, "man") == 0) {
    stop();
  }
#endif

  // get first set of potential columns into line1
  limit = collect_columns(last_guess, 0, MAX_WORDS_PER_LINE);
  copy_line(last_raw, last_guess);
  // add_right_full_width(last_guess, mingap);   // adds extra right column to bring table to full width

  copy_line(all_words, last_guess);
  indentation.vertical_limit = limit;

  if (page_contents->words.is_equal_to_head() || (limit == 0)) {
    next_line[0].left  = 0;
    next_line[0].right = 0;
  } else {
    // and get the next line for finding columns
    limit = collect_columns(next_line, last_guess, MAX_WORDS_PER_LINE);
    lines++;
  }

  // now check to see whether the first line looks like a single centered line

  if (single_centered_line(last_raw, next_line, start)) {
    rewind_text_to(start);
    write_centered_line(start);
    indentation.no_of_columns = 0;   // center instead
    return( TRUE );
  } else if (! table_on) {
    rewind_text_to(start);
    return( FALSE );
  }

  combine_line(all_words, next_line);
  if (debug_table_on) {
    display_columns(start->text_string, "[b] all_words", all_words);
  }

  if ((! remove_white_using_words(next_guess, last_guess, next_line))) {
  }

  if ((! conflict_with_words(next_guess, all_words)) &&
      (continue_searching_column(next_guess, next_guess, all_words)) &&
      (! page_contents->words.is_equal_to_head()) &&
      ((end_region_vpos < 0) || (limit < end_region_vpos)) &&
      (limit > 0)) {

    combine_line(last_guess, next_line);
    // subtract any columns which are bridged by a sequence of words
    do {
      copy_line(prev_guess, next_guess);
      combine_line(last_guess, next_guess);

      if (debug_table_on) {
	t = page_contents->words.get_data();
	display_columns(t->text_string, "[l] last_guess", last_guess);
      }
      indentation.vertical_limit = limit;

      copy_line(last_raw, next_line);
      if (page_contents->words.is_equal_to_head()) {
	next_line[0].left  = 0;
	next_line[0].right = 0;
      } else {
	limit = collect_columns(next_line, last_guess, MAX_WORDS_PER_LINE);
	lines++;
      }

      combine_line(all_words, next_line);
      if (debug_table_on) {
	display_columns(t->text_string, "[l] all_words", all_words);
	display_columns(t->text_string, "[l] last_raw ", last_raw);
      }

      if (debug_table_on) {
	display_columns(t->text_string, "[l] next_line", next_line);
      }
      t = page_contents->words.get_data();
#if 0
      if (strcmp(t->text_string, "market,") == 0) {
	stop();
      }
#endif

    } while ((! remove_white_using_words(next_guess, last_guess, next_line)) &&
	     (! conflict_with_words(next_guess, all_words)) &&
	     (continue_searching_column(next_guess, last_guess, all_words)) &&
	     ((is_continueous_column(prev_guess, last_raw)) || (is_exact_left(last_guess, next_line))) &&
	     (! page_contents->words.is_equal_to_head()) &&
	     ((end_region_vpos <= 0) || (t->minv < end_region_vpos)) &&
	     (limit >= 0));
  }
  lines--;

  if (limit < 0) {
    indentation.vertical_limit = limit;
  }

  if (page_contents->words.is_equal_to_head()) {
    // end of page check whether we should include everything
    if ((! conflict_with_words(next_guess, all_words)) &&
	(continue_searching_column(next_guess, last_guess, all_words)) &&
	((is_continueous_column(prev_guess, last_raw)) || (is_exact_left(last_guess, next_line)))) {
      // end of page reached - therefore include everything
      page_contents->words.start_from_tail();
      t = page_contents->words.get_data();
      indentation.vertical_limit = t->minv;
    }
  } else {
    t = page_contents->words.get_data();
    if ((end_region_vpos > 0) && (t->minv > end_region_vpos)) {
      indentation.vertical_limit = min(indentation.vertical_limit, end_region_vpos+1);
    } else if (indentation.vertical_limit < 0) {
      // -1 as we don't want to include section heading itself
      indentation.vertical_limit = -indentation.vertical_limit-1;
    }
  }

  if (debug_table_on) {
    display_columns(start->text_string, "[x] last_guess", last_guess);
  }
  rewind_text_to(start);

  i = count_columns(last_guess);
  if (((lines > 2) && ((i>1) || (continue_searching_column(last_guess, last_guess, all_words)))) ||
      ((lines == 1) && (large_enough_gap(last_guess)))) {
    // copy match into permenant html_table

    if (indentation.columns != 0) {
      free(indentation.columns);
    }
    if (debug_table_on) {
      display_columns(start->text_string, "[x] last_guess", last_guess);
    }
    add_column_gaps(last_guess);
    if (debug_table_on) {
      display_columns(start->text_string, "[g] last_guess", last_guess);
    }

    indentation.no_of_columns = count_columns(last_guess);
    indentation.columns       = (struct text_defn *)malloc(indentation.no_of_columns*sizeof(struct text_defn));

    i=0;
    while (i<indentation.no_of_columns) {
      indentation.columns[i].left  = last_guess[i].left;
      indentation.columns[i].right = last_guess[i].right;
      i++;
    }
    return( TRUE );
  } else {
    return( FALSE );
  }
}

void html_printer::define_cell (int left, int right)
{
  float f=((float)(right-left))/((float)(right_margin_indent-left_margin_indent))*100.0;

  html.put_string("<td valign=\"top\" align=\"left\"  width=\"");
  if (f > 1.0) {
    html.put_float(f);
  } else {
    html.put_float(1.0);
  }
  html.put_string("%\">\n");
}

/*
 *  column_display_word - given a left, right pair and the indentation.vertical_limit
 *                        write out html text within this region.
 */

void html_printer::column_display_word (int vert, int left, int right, int next)
{
  text_glob *g=page_contents->words.get_data();

  if (left != next) {
    define_cell(left, next);
    begin_paragraph_no_height(left_alignment);
    while ((g != 0) && (g->minv <= vert)) {
      if ((left <= g->minh) && (g->minh<right)) {
	char *postword=html_position_text(g, left, right);
	
	if (header.written_header) {
	  fatal("should never generate a header inside a table");
	} else {
	  if (g->is_raw_command) {
	    html.put_string((char *)g->text_string);
	  } else {
	    html.html_write_string((char *)g->text_string);
	  }
	  if (postword != 0) {
	    html.put_string(postword);
	  }
	  issued_newline = FALSE;
	}
      }
      if (page_contents->words.is_equal_to_tail()) {
	g = 0;
      } else {
	page_contents->words.move_right();
	g=page_contents->words.get_data();
      }
#if 0
      if (page_contents->words.is_equal_to_head()) {
	g = 0;
	page_contents->words.start_from_tail();
      } else {

      }
#endif
    }
    end_paragraph();
    html.put_string("</td>\n");
    if (g != 0) {
      page_contents->words.move_left();
      // and correct output_vpos
      g=page_contents->words.get_data();
      output_vpos = g->minv;
    }
  }
}

/*
 *  start_table - creates a table according with parameters contained within class html_table.
 */

void html_printer::start_table (void)
{
  int i;

  end_paragraph();
  html.put_string("\n<table width=\"100%\"  rules=\"none\"  frame=\"none\"  cols=\"");
  html.put_number(indentation.no_of_columns);
  html.put_string("\">\n");
}

/*
 *  end_table - finishes off a table.
 */

void html_printer::end_table (void)
{
  html.put_string("</table>\n");
  indentation.no_of_columns = 0;
}

/*
 *  column_calculate_right_margin - scan through the column and find the right most margin
 */

int html_printer::column_calculate_right_margin (int left, int right)
{
  if (left == right) {
    return( right );
  } else {
    int rightmost    =-1;
    int count        = 0;
    text_glob *start = page_contents->words.get_data();
    text_glob *g     = start;

    while ((g != 0) && (g->minv <= indentation.vertical_limit)) {
      if ((left <= g->minh) && (g->minh<right)) {
	if (debug_on) {
	  fprintf(stderr, "right word = %s      %d\n", g->text_string, g->maxh); fflush(stderr);
	}
	if (g->maxh == rightmost) {
	  count++;
	} else if (g->maxh > rightmost) {
	  count = 1;
	  rightmost = g->maxh;
	}
	if (g->maxh > right) {
	  if (debug_on) {
	    fprintf(stderr, "problem as right word = %s      %d    [%d..%d]\n",
		    g->text_string, right, g->minh, g->maxh); fflush(stderr);
	    stop();
	  }
	}
      }
      page_contents->words.move_right();
      if (page_contents->words.is_equal_to_head()) {
	g = 0;
	page_contents->words.start_from_tail();
      } else {
	g=page_contents->words.get_data();
      }
    }
    rewind_text_to(start);
    if (rightmost == -1) {
      return( right );  // no words in this column
    } else {
      if (count == 1) {
	return( rightmost+1 );
      } else {
	return( rightmost );
      }
    }
  }
}

/*
 *  column_calculate_left_margin - scan through the column and find the left most margin
 */

int html_printer::column_calculate_left_margin (int left, int right)
{
  if (left == right) {
    return( left );
  } else {
    int leftmost=right;
    text_glob *start = page_contents->words.get_data();
    text_glob *g     = start;

    while ((g != 0) && (g->minv <= indentation.vertical_limit)) {
      if ((left <= g->minh) && (g->minh<right)) {
	leftmost = min(g->minh, leftmost);
      }
      page_contents->words.move_right();
      if (page_contents->words.is_equal_to_head()) {
	g = 0;
	page_contents->words.start_from_tail();
      } else {
	g=page_contents->words.get_data();
      }
    }
    rewind_text_to(start);
    if (leftmost == right) {
      return( left );  // no words in this column
    } else {
      return( leftmost );
    }
  }
}

/*
 *  find_column_index - returns the index to the column in which glob, t, exists.
 */

int html_printer::find_column_index (text_glob *t)
{
  int i=0;

  while ((i<indentation.no_of_columns) &&
	 (! ((indentation.columns[i].left<=t->minh) &&
	     (indentation.columns[i].right>t->minh)))) {
    i++;
  }
  return( i );
}

/*
 *  determine_row_limit - checks each row to see if there is a gap in a cell.
 *                        We return the vertical position after the empty cell
 *                        at the start of the next line.
 */

int html_printer::determine_row_limit (text_glob *start, int v)
{
  text_glob *t;
  int        i;
  int        vpos, prev, last;
  int is_gap[MAX_WORDS_PER_LINE];

  if (v >= indentation.vertical_limit) {
    return( v+1 );
  } else {
    // initially we start with all gaps in our table
    // after a gap we start a new row
    // here we set the gap array to the previous line

    if (v>=0) {
      t = page_contents->words.get_data();
      if (t->minv < v) {
	do {
	  page_contents->words.move_right();
	  t = page_contents->words.get_data();
	} while ((! page_contents->words.is_equal_to_head()) &&
		 (t->minv <= v));
      }
    }
    if (! page_contents->words.is_equal_to_head()) {
      page_contents->words.move_left();
    }
    t = page_contents->words.get_data();
    prev = t->minv;
    for (i=0; i<indentation.no_of_columns; i++) {
      is_gap[i] = prev;
    }

    if (! page_contents->words.is_equal_to_tail()) {
      page_contents->words.move_right();
    }
    t = page_contents->words.get_data();
    vpos = t->minv;

    // now check each row for a gap
    do {
      last = vpos;
      vpos = t->minv;
      i = find_column_index(t);
      if (! is_on_same_line(t, last)) {
	prev = last;
      }

      if ((is_gap[i] != vpos) && (is_gap[i] != prev) &&
	  (indentation.columns[i].is_used)) {
	// no word on previous line - must be a gap - force alignment of row
	rewind_text_to(start);
	return( last );
      }
      is_gap[i] = vpos;
      page_contents->words.move_right();
      t = page_contents->words.get_data();
    } while ((! page_contents->words.is_equal_to_head()) &&
	     (vpos < indentation.vertical_limit) && (vpos >= last));
    page_contents->words.move_left();
    t = page_contents->words.get_data();
    rewind_text_to(start);
    return( indentation.vertical_limit );
  }
}

/*
 *  assign_used_columns - sets the is_used field of the column array of records.
 */

void html_printer::assign_used_columns (text_glob *start)
{
  text_glob *t = start;
  int        i;

  for (i=0; i<indentation.no_of_columns; i++) {
    indentation.columns[i].is_used = FALSE;
  }

  rewind_text_to(start);
  if (! page_contents->words.is_empty()) {
    do {
      i = find_column_index(t);
      if (indentation.columns[i].left != 0) {
	if (debug_table_on) {
	  fprintf(stderr, "[%s] in column %d at %d..%d  limit %d\n", t->text_string,
		  i, t->minv, t->maxv, indentation.vertical_limit); fflush(stderr);
	}
	indentation.columns[i].is_used = TRUE;
      }
      page_contents->words.move_right();
      t = page_contents->words.get_data();
    } while ((t->minv<indentation.vertical_limit) &&
	     (! page_contents->words.is_equal_to_head()));
  }
  if (debug_table_on) {
    for (i=0; i<indentation.no_of_columns; i++) {
      fprintf(stderr, " <left=%d right=%d is_used=%d> ",
	      indentation.columns[i].left,
	      indentation.columns[i].right,
	      indentation.columns[i].is_used);
    }
    fprintf(stderr, "\n");
    fflush(stderr);
  }
}

/*
 *  foreach_column_include_text - foreach column in a table place the
 *                                appropriate html text.
 */

void html_printer::foreach_column_include_text (text_glob *start)
{
  if (indentation.no_of_columns>0) {
    int i;
    int left, right;
    int limit=-1;

    assign_used_columns(start);
    start_table();
    rewind_text_to(start);

    do {
      limit = determine_row_limit(start, limit);   // find the bottom of the next row
      html.put_string("<tr valign=\"top\" align=\"left\">\n");
      i=0;
      start = page_contents->words.get_data();
      while (i<indentation.no_of_columns) {
	// reset the output position to the start of column
	rewind_text_to(start);
	output_vpos = start->minv;
	output_hpos = indentation.columns[i].left;
	// and display each column until limit
	right = column_calculate_right_margin(indentation.columns[i].left,
					      indentation.columns[i].right);
	left  = column_calculate_left_margin(indentation.columns[i].left,
					     indentation.columns[i].right);

	if (right>indentation.columns[i].right) {
	  if (debug_on) {
	    fprintf(stderr, "assert calculated right column edge is greater than column\n"); fflush(stderr);
	    stop();
	  }
	}

	if (left<indentation.columns[i].left) {
	  if (debug_on) {
	    fprintf(stderr, "assert calculated left column edge is less than column\n"); fflush(stderr);
	    stop();
	  }
	}

	column_display_word(limit, left, right, indentation.columns[i].right);
	i++;
      }

      if (page_contents->words.is_equal_to_tail()) {
	start = 0;
      } else {
	page_contents->words.sub_move_right();
	if (page_contents->words.is_empty()) {
	  start = 0;
	} else {
	  start = page_contents->words.get_data();
	}
      }

      html.put_string("</tr>\n");
    } while ((limit < indentation.vertical_limit) && (start != 0) &&
	     (! page_contents->words.is_empty()));
    end_table();

    if (start == 0) {
      // finished page remove all words
      page_contents->words.start_from_head();
      while (! page_contents->words.is_empty()) {
	page_contents->words.sub_move_right();
      }
    } else if (! page_contents->words.is_empty()) {
      page_contents->words.move_left();
    }
  }
}

/*
 *  write_centered_line - generates a line of centered text.
 */

void html_printer::write_centered_line (text_glob *g)
{
  int current_vpos=g->minv;

  move_vertical(g, center_alignment);

  header.written_header = FALSE;
  output_vpos = g->minv;
  output_hpos = g->minh;
  do {
    char *postword=html_position_text(g, left_margin_indent, right_margin_indent);

    if (! header.written_header) {
      if (g->is_raw_command) {
	html.put_string((char *)g->text_string);
      } else {
	html.html_write_string((char *)g->text_string);
      }
      if (postword != 0) {
	html.put_string(postword);
      }
      need_one_newline = TRUE;
      issued_newline   = FALSE;
    }    
    page_contents->words.move_right();
    g = page_contents->words.get_data();
  } while ((! page_contents->words.is_equal_to_head()) && (g->minv == current_vpos));
  page_contents->words.move_left();  // so when we move right we land on the word following this centered line
  need_one_newline = TRUE;
}

/*
 *  is_in_middle - returns TRUE if the text defn, t, is in the middle of the page.
 */

int html_printer::is_in_middle (int left, int right)
{
  return( abs(abs(left-left_margin_indent) - abs(right_margin_indent-right)) <= CENTER_TOLERANCE );
}

/*
 *  single_centered_line - returns TRUE if first is a centered line with a different
 *                         margin to second.
 */

int html_printer::single_centered_line (text_defn *first, text_defn *second, text_glob *g)
{
  return(
	 ((count_columns(first) == 1) && (first[0].left != left_margin_indent) &&
	  (first[0].left != second[0].left) && is_in_middle(first->left, first->right))
	 );
}

/*
 *  check_able_to_use_center - returns TRUE if we can see a centered line.
 */

int html_printer::check_able_to_use_center (text_glob *g)
{
  if (auto_on && table_on && ((! is_on_same_line(g, output_vpos)) || issued_newline) && (! using_table_for_indent())) {
    // we are allowed to check for centered line
    // first check to see whether we might be looking at a set of columns
    struct text_defn   last_guess[MAX_WORDS_PER_LINE];
    int                limit = collect_columns(last_guess, 0, MAX_WORDS_PER_LINE);

    rewind_text_to(g);    
    if ((count_columns(last_guess) == 1) && (is_in_middle(last_guess[0].left, last_guess[0].right))) {
      write_centered_line(g);
      return( TRUE );
    }
  }
  return( FALSE );
}

/*
 *  check_able_to_use_table - examines forthcoming text to see whether we can
 *                            better format it by using an html transparent table.
 */

int html_printer::check_able_to_use_table (text_glob *g)
{
  if (auto_on && ((! is_on_same_line(g, output_vpos)) || issued_newline) && (! using_table_for_indent())) {
    // we are allowed to check for table
    
    if ((output_hpos != right_margin_indent) && (found_use_for_table(g))) {
      foreach_column_include_text(g);
      return( TRUE );
    }
  }
  return( FALSE );
}

/*
 *  move_vertical - if we are using html auto formatting then decide whether to
 *                  break the line via a <br> or a </p><p> sequence.
 */

void html_printer::move_vertical (text_glob *g, paragraph_type p)
{
  int  r        =font::res;
  int  height   = (g->text_style.point_size+2)*r/72;   // --fixme-- we always assume VS is PS+2 (could do better)
  int  temp_vpos;

  if (auto_on) {
    if ((more_than_line_break(output_vpos, g->minv, height)) || (p != para_type)) {
      end_paragraph();
      begin_paragraph(p);
    } else {
      html_newline();
    }
  } else {
    if (output_vpos == -1) {
      temp_vpos = g->minv;
    } else {
      temp_vpos = output_vpos;
    }

    force_begin_paragraph();
    if (need_one_newline) {
      html_newline();
      temp_vpos += height;
    } else {
      need_one_newline = TRUE;
    }
    
    while ((temp_vpos < g->minv) && (more_than_line_break(temp_vpos, g->minv, height))) {
      html_newline();
      temp_vpos += height;
    }
  }
}

/*
 *  emit_space - emits a space within html, it checks for the font type and
 *               will change font depending upon, g. Courier spaces are larger
 *               than roman so we need consistancy when changing between them.
 */

void html_printer::emit_space (text_glob *g, int force_space)
{
  if (! need_paragraph) {
    // only generate a space if we have written a word - as html will ignore it otherwise
    if ((output_style != g->text_style) && (g->text_style.f != 0)) {
      terminate_current_font();
    }
    if (force_space || (g->minh > output_hpos)) {
      html.put_string(" ");
    }
    change_font(g, TRUE);      
  }
}

/*
 *  html_position_text - determine whether the text is subscript/superscript/normal
 *                       or a header.
 */

char *html_printer::html_position_text (text_glob *g, int left_margin, int right_margin)
{
  char *postword=0;

#if 0
  if (strcmp(g->text_string, "increased.") == 0) {
    stop();
  }
#endif
  begin_paragraph(left_alignment);

  if ((! header.written_header) &&
      (is_on_same_line(g, output_vpos) ||
       pretend_is_on_same_line(g, left_margin, right_margin))) {
    // check whether the font was reset after generating an image

    header.written_header = FALSE;
    force_begin_paragraph();

    // check whether we need to insert white space between words on 'same' line
    if (pretend_is_on_same_line(g, left_margin, right_margin)) {
      emit_space(g, TRUE);
    }

    if (output_style.f == 0) {
      change_font(g, TRUE);
    }

    if (looks_like_subscript(g)) {

      g->text_style.point_size = output_style.point_size;
      g->minv                  = output_vpos;   // this ensures that output_vpos doesn't alter
                                                // which allows multiple subscripted words
      move_horizontal(g, left_margin);
      html.put_string("<sub>");
      postword = "</sub>";
    } else if (looks_like_superscript(g)) {

      g->text_style.point_size = output_style.point_size;
      g->minv                  = output_vpos;

      move_horizontal(g, left_margin);
      html.put_string("<sup>");
      postword = "</sup>";
    } else {
      move_horizontal(g, left_margin);
    }
  } else {
    // we have found a new line
    if (! header.written_header) {
      move_vertical(g, left_alignment);
    }
    header.written_header = FALSE;

    if (processed_header(g)) {
      // we must not alter output_vpos as we have peeped at the next word
      // and set vpos to this - to ensure we do not generate a <br> after
      // a heading. (The html heading automatically generates a line break)
      output_hpos = left_margin;
      return( postword );
    } else {
      force_begin_paragraph();
      if (g->minh-left_margin != 0) {
	make_html_indent(g->minh-left_margin);
      }
      change_font(g, TRUE);
    }
  }
  output_vpos = g->minv;
  output_hpos = g->maxh;
  return( postword );
}


int html_printer::html_position_region (void)
{
  int  r         = font::res;
  int  height    = output_style.point_size*r/72;
  int  temp_vpos;
  int  is_center = FALSE;

  if (output_style.point_size != 0) {
    if (output_vpos != start_region_vpos) {

      // graphic starts on a different line
      if (output_vpos == -1) {
	temp_vpos = start_region_vpos;
      } else {
	temp_vpos = output_vpos;
      }

#if 1
      if (need_one_newline) {
	html_newline();
	temp_vpos += height;
      } else {
	need_one_newline = TRUE;
      }
#else
      html_newline();
      temp_vpos += height;
#endif

      while ((temp_vpos < start_region_vpos) &&
	     (more_than_line_break(temp_vpos, start_region_vpos, height))) {
	html_newline();
	temp_vpos += height;
      }
    }
  }
  if (auto_on && (is_in_middle(start_region_hpos, end_region_hpos))) {
    is_center = TRUE;
  } else {
    if (start_region_hpos > left_margin_indent) {
      html.put_string("<span style=\" text-indent: ");
      html.put_float(((double)(start_region_hpos-left_margin_indent)/((double)r)));
      html.put_string("in;\"></span>");
    }
  }
#if 0
   } else {
      // on the same line
      if (start_region_hpos > output_hpos) {
	html.put_string("<span style=\" text-indent: ");
	html.put_float(((double)(start_region_hpos-output_hpos)/((double)r)));
	html.put_string("in;\"></span>");
      }
    }
  }
#endif
  output_vpos = start_region_vpos;
  output_hpos = start_region_hpos;
  return( is_center );
}


/*
 *  gs_x - translate and scale the x axis
 */

int html_printer::gs_x (int x)
{
  x += IMAGE_BOARDER_PIXELS/2;
  return((x-start_region_hpos)*postscript_res/font::res);
}


/*
 *  gs_y - translate and scale the y axis
 */

int html_printer::gs_y (int y)
{
  int yoffset=((int)(A4_PAGE_LENGTH*(double)font::res))-end_region_vpos;

  y += IMAGE_BOARDER_PIXELS/2;
  return( (y+yoffset)*postscript_res/font::res );
}


void html_printer::troff_position_text (text_glob *g)
{
  change_font(g, FALSE);

  troff.put_string("V");
  troff.put_number(gs_y(g->maxv));
  troff.put_string("\n");

  troff.put_string("H");
  troff.put_number(gs_x(g->minh));
  troff.put_string("\n");
}

void html_printer::troff_change_font (const char *fontname, int size, int font_no)
{
  troff.put_string("x font ");
  troff.put_number(font_no);
  troff.put_string(" ");
  troff.put_string(fontname);
  troff.put_string("\nf");
  troff.put_number(font_no);
  troff.put_string("\ns");
  troff.put_number(size*1000);
  troff.put_string("\n");
}


void html_printer::set_style(const style &sty)
{
#if 0
  const char *fontname = sty.f->get_name();
  if (fontname == 0)
    fatal("no internalname specified for font");

  change_font(fontname, (font::res/(72*font::sizescale))*sty.point_size);
#endif
}

void html_printer::end_of_line()
{
  flush_sbuf();
  output_hpos = -1;
}

void html_printer::html_display_word (text_glob *g)
{
#if 0
  if (strcmp(g->text_string, "increased.") == 0) {
    stop();
  }
#endif
  if (! check_able_to_use_table(g)) {
    char *postword=html_position_text(g, left_margin_indent, right_margin_indent);

    if (! header.written_header) {
      if (g->is_raw_command) {
	html.put_string((char *)g->text_string);
      } else {
	html.html_write_string((char *)g->text_string);
      }
      if (postword != 0) {
	html.put_string(postword);
      }
      need_one_newline = TRUE;
      issued_newline   = FALSE;
    }
  }
}

void html_printer::troff_display_word (text_glob *g)
{
  troff_position_text(g);
  if (g->is_raw_command) {
    int l=strlen((char *)g->text_string);
    if (l == 1) {
      troff.put_string("c");
      troff.put_string((char *)g->text_string);
      troff.put_string("\n");
    } else if (l > 1) {
      troff.put_string("C");
      troff.put_translated_char((char *)g->text_string);
      troff.put_string("\n");
    }
  } else {
    troff_position_text(g);
    troff.put_string("t");
    troff.put_translated_string((const char *)g->text_string);
    troff.put_string("\n");
  }
}

void html_printer::display_word (text_glob *g, int is_to_html)
{
  if (is_to_html) {
    html_display_word(g);
  } else if ((g->is_raw_command) && (g->is_html_command)) {
    // found a raw html command inside a graphic glob.
    // We should emit the command to the html device, but of course we
    // cannot place it correctly as we are dealing with troff words.
    // Remember output_vpos will refer to troff and not html.
    html.put_string((char *)g->text_string);
  } else {
    troff_display_word(g);
  }
}


/*
 *  this information may be better placed inside some of the font files
 *  in devhtml - however one must bare in mind that we need the ability
 *  to write out to TWO devices (image and html) and image
 *  invokes ghostscript.
 */

simple_output &simple_output::html_write_string (const char *s)
{
  int i=0;

  while (s[i] != (char)0) {
    if (s[i] == '<') {
      put_string("&lt;");
    } else if (s[i] == '>') {
      put_string("&gt;");
    } else {
      fputc(s[i], fp);
      col++;
    }
    i++;
  }
  return *this;
}

/*
 *  display_fill - generates a troff format fill command
 */

void html_printer::display_fill (graphic_glob *g)
{
  troff.put_string("Df ") ;
  troff.put_number(g->fill);
  troff.put_string(" 0\n");
}

/*
 *  display_line - displays a line using troff format
 */

void html_printer::display_line (graphic_glob *g, int is_to_html)
{
  if (is_to_html) {
    fatal("cannot emit lines in html");
  }
  if (g->code == 'l') {
    // straight line

    troff.put_string("V");
    troff.put_number(gs_y(g->point[0].y));
    troff.put_string("\n");

    troff.put_string("H");
    troff.put_number(gs_x(g->point[0].x));
    troff.put_string("\n");

    display_fill(g);

    troff.put_string("Dl ");
    troff.put_number((g->point[1].x-g->point[0].x)*postscript_res/font::res);
    troff.put_string(" ");
    troff.put_number((g->point[1].y-g->point[0].y)*postscript_res/font::res);
    troff.put_string("\n");
    // printf("line %c %d %d %d %d size %d\n", (char)g->code, g->point[0].x, g->point[0].y,
    //                                         g->point[1].x, g->point[1].y, g->size);
  } else if ((g->code == 'c') || (g->code == 'C')) {
    // circle

    int xradius = (g->maxh - g->minh) / 2;
    int yradius = (g->maxv - g->minv) / 2;
    // center of circle or elipse

    troff.put_string("V");
    troff.put_number(gs_y(g->minv+yradius));
    troff.put_string("\n");

    troff.put_string("H");
    troff.put_number(gs_x(g->minh));
    troff.put_string("\n");

    display_fill(g);

    if (g->code == 'c') {
      troff.put_string("Dc ");
    } else {
      troff.put_string("DC ");
    }

    troff.put_number(xradius*2*postscript_res/font::res);
    troff.put_string("\n");

  } else if ((g->code == 'e') || (g->code == 'E')) {
    // ellipse

    int xradius = (g->maxh - g->minh) / 2;
    int yradius = (g->maxv - g->minv) / 2;
    // center of elipse - this is untested

    troff.put_string("V");
    troff.put_number(gs_y(g->minv+yradius));
    troff.put_string("\n");

    troff.put_string("H");
    troff.put_number(gs_x(g->minh));
    troff.put_string("\n");

    display_fill(g);

    if (g->code == 'e') {
      troff.put_string("De ");
    } else {
      troff.put_string("DE ");
    }

    troff.put_number(xradius*2*postscript_res/font::res);
    troff.put_string(" ");
    troff.put_number(yradius*2*postscript_res/font::res);
    troff.put_string("\n");
  } else if ((g->code == 'p') || (g->code == 'P')) {
    // polygon
    troff.put_string("V");
    troff.put_number(gs_y(g->yc));
    troff.put_string("\n");

    troff.put_string("H");
    troff.put_number(gs_x(g->xc));
    troff.put_string("\n");

    display_fill(g);

    if (g->code == 'p') {
      troff.put_string("Dp");
    } else {
      troff.put_string("DP");
    }

    int i;
    int xc=g->xc;
    int yc=g->yc;
    for (i=0; i<g->nopoints; i++) {
      troff.put_string(" ");
      troff.put_number((g->point[i].x-xc)*postscript_res/font::res);
      troff.put_string(" ");
      troff.put_number((g->point[i].y-yc)*postscript_res/font::res);
      xc = g->point[i].x;
      yc = g->point[i].y;
    }
    troff.put_string("\n");
  } else if (g->code == 'a') {
    // arc
    troff.put_string("V");
    troff.put_number(gs_y(g->yc));
    troff.put_string("\n");

    troff.put_string("H");
    troff.put_number(gs_x(g->xc));
    troff.put_string("\n");

    display_fill(g);

    troff.put_string("Da");

    int i;

    for (i=0; i<g->nopoints; i++) {
      troff.put_string(" ");
      troff.put_number(g->point[i].x*postscript_res/font::res);
      troff.put_string(" ");
      troff.put_number(g->point[i].y*postscript_res/font::res);
    }
    troff.put_string("\n");
  } else if (g->code == '~') {
    // spline
    troff.put_string("V");
    troff.put_number(gs_y(g->yc));
    troff.put_string("\n");

    troff.put_string("H");
    troff.put_number(gs_x(g->xc));
    troff.put_string("\n");

    display_fill(g);

    troff.put_string("D~");

    int i;
    int xc=g->xc;
    int yc=g->yc;
    for (i=0; i<g->nopoints; i++) {
      troff.put_string(" ");
      troff.put_number((g->point[i].x-xc)*postscript_res/font::res);
      troff.put_string(" ");
      troff.put_number((g->point[i].y-yc)*postscript_res/font::res);
      xc = g->point[i].x;
      yc = g->point[i].y;
    }
    troff.put_string("\n");
  }
}


void html_printer::flush_sbuf()
{
  if (sbuf_len > 0) {
    int r=font::res;   // resolution of the device actually
    set_style(sbuf_style);

    page_contents->add(&sbuf_style, sbuf, sbuf_len,
		       sbuf_vpos-sbuf_style.point_size*r/72, sbuf_start_hpos,
		       sbuf_vpos,                            sbuf_end_hpos);
	     
    output_hpos = sbuf_end_hpos;
    output_vpos = sbuf_vpos;
    sbuf_len = 0;
  }

#if 0
  enum {
    NONE,
    RELATIVE_H,
    RELATIVE_V,
    RELATIVE_HV,
    ABSOLUTE
    } motion = NONE;
  int space_flag = 0;
  if (sbuf_len == 0)
    return;

  if (output_style != sbuf_style) {
    set_style(sbuf_style);
    output_style = sbuf_style;
  }

  int extra_space = 0;
  if (output_hpos < 0 || output_vpos < 0)
    motion = ABSOLUTE;
  else {
    if (output_hpos != sbuf_start_hpos)
      motion = RELATIVE_H;
    if (output_vpos != sbuf_vpos) {
      if  (motion != NONE)
	motion = RELATIVE_HV;
      else
	motion = RELATIVE_V;
    }
  }
  if (sbuf_space_code >= 0) {
    int w = sbuf_style.f->get_width(space_char_index, sbuf_style.point_size);
    if (w + sbuf_kern != sbuf_space_width) {
      if (sbuf_space_code != output_space_code) {
	output_space_code = sbuf_space_code;
      }
      space_flag = 1;
      extra_space = sbuf_space_width - w - sbuf_kern;
      if (sbuf_space_diff_count > sbuf_space_count/2)
	extra_space++;
      else if (sbuf_space_diff_count < -(sbuf_space_count/2))
	extra_space--;
    }
  }

  if (space_flag)
    html.put_number(extra_space);
  if (sbuf_kern != 0)
    html.put_number(sbuf_kern);

  html.put_string(sbuf, sbuf_len);

  char sym[2];
  sym[0] = 'A' + motion*4 + space_flag + 2*(sbuf_kern != 0);
  sym[1] = '\0';
  switch (motion) {
  case NONE:
    break;
  case ABSOLUTE:
    html.put_number(sbuf_start_hpos)
       .put_number(sbuf_vpos);
    break;
  case RELATIVE_H:
    html.put_number(sbuf_start_hpos - output_hpos);
    break;
  case RELATIVE_V:
    html.put_number(sbuf_vpos - output_vpos);
    break;
  case RELATIVE_HV:
    html.put_number(sbuf_start_hpos - output_hpos)
       .put_number(sbuf_vpos - output_vpos);
    break;
  default:
    assert(0);
  }

  output_hpos = sbuf_end_hpos;
  output_vpos = sbuf_vpos;
  sbuf_len = 0;
#endif
}


void html_printer::set_line_thickness(const environment *env)
{
  line_thickness = env->size;
  printf("line thickness = %d\n", line_thickness);
}

void html_printer::draw(int code, int *p, int np, const environment *env)
{
  switch (code) {

  case 'l':
    if (np == 2) {
      page_contents->add_line(code,
			      env->hpos, env->vpos, env->hpos+p[0], env->vpos+p[1],
			      env->size, fill);
    } else {
      error("2 arguments required for line");
    }
    break;
  case 't':
    {
      if (np == 0) {
	line_thickness = -1;
      } else {
	// troff gratuitously adds an extra 0
	if (np != 1 && np != 2) {
	  error("0 or 1 argument required for thickness");
	  break;
	}
	line_thickness = p[0];
      }
      break;
    }

  case 'P':
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
      // firstly lets add our current position to polygon
      int oh=env->hpos;
      int ov=env->vpos;
      int i=0;

      while (i<np) {
	p[i+0] += oh;
	p[i+1] += ov;
	oh      = p[i+0];
	ov      = p[i+1];
	i      += 2;
      }
      // now store polygon in page
      page_contents->add_polygon(code, np, p, env->hpos, env->vpos, env->size, fill);
    }
    break;
  case 'E':
    // fall through
  case 'e':
    if (np != 2) {
      error("2 arguments required for ellipse");
      break;
    }
    page_contents->add_line(code,
			    env->hpos, env->vpos-p[1]/2, env->hpos+p[0], env->vpos+p[1]/2,
			    env->size, fill);

    break;
  case 'C':
    // fill circle

  case 'c':
    {
      // troff adds an extra argument to C
      if (np != 1 && !(code == 'C' && np == 2)) {
	error("1 argument required for circle");
	break;
      }
      page_contents->add_line(code,
			      env->hpos, env->vpos-p[0]/2, env->hpos+p[0], env->vpos+p[0]/2,
			      env->size, fill);
    }
    break;
  case 'a':
    {
      if (np == 4) {
	double c[2];

	if (adjust_arc_center(p, c)) {
	  page_contents->add_arc('a', env->hpos, env->vpos, p, c, env->size, fill);
	} else {
	  // a straignt line
	  page_contents->add_line('l', env->hpos, env->vpos, p[0]+p[2], p[1]+p[3], env->size, fill);
	}
      } else {
	error("4 arguments required for arc");
      }
    }
    break;
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
      // firstly lets add our current position to spline
      int oh=env->hpos;
      int ov=env->vpos;
      int i=0;

      while (i<np) {
	p[i+0] += oh;
	p[i+1] += ov;
	oh      = p[i+0];
	ov      = p[i+1];
	i      += 2;
      }
      page_contents->add_spline('~', env->hpos, env->vpos, np, p, env->size, fill);
    }
    break;
  case 'f':
    {
      if (np != 1 && np != 2) {
	error("1 argument required for fill");
	break;
      }
      fill = p[0];
      if (fill < 0 || fill > FILL_MAX) {
	// This means fill with the current color.
	fill = FILL_MAX + 1;
      }
      break;
    }

  default:
    error("unrecognised drawing command `%1'", char(code));
    break;
  }
}


void html_printer::begin_page(int n)
{
  page_number            =  n;
  html.begin_comment("Page: ").comment_arg(itoa(page_number)).end_comment();;
  no_of_printed_pages++;
  
  output_style.f         =  0;
  output_space_code      = 32;
  output_draw_point_size = -1;
  output_line_thickness  = -1;
  output_hpos            = -1;
  output_vpos            = -1;
}

void testing (text_glob *g) {}

void html_printer::flush_graphic (void)
{
  graphic_glob g;

  graphic_level = 0;
  page_contents->is_in_graphic = FALSE;

  g.minv = -1;
  g.maxv = -1;
  calculate_region_range(&g);
  if (g.minv != -1) {
    page_contents->make_new_region(&g);
  }
  move_region_to_page();
}

void html_printer::end_page(int)
{
  flush_sbuf();
  flush_graphic();
  flush_page();
}

font *html_printer::make_font(const char *nm)
{
  return html_font::load_html_font(nm);
}

html_printer::~html_printer()
{
  if (fseek(tempfp, 0L, 0) < 0)
    fatal("fseek on temporary file failed");
  html.set_file(stdout);
  fputs("<html>\n", stdout);
  fputs("<head>\n", stdout);
  fputs("<meta name=\"Content-Style\" content=\"text/css\">\n", stdout);
  write_title(TRUE);
  fputs("</head>\n", stdout);
  fputs("<body>\n", stdout);
  write_title(FALSE);
  header.write_headings(stdout);
  {
    extern const char *version_string;
    html.begin_comment("Creator     : ")
       .comment_arg("groff ")
       .comment_arg("version ")
       .comment_arg(version_string)
       .end_comment();
  }
  {
#ifdef LONG_FOR_TIME_T
    long
#else
    time_t
#endif
    t = time(0);
    html.begin_comment("CreationDate: ")
       .comment_arg(ctime(&t))
       .end_comment();
  }
  for (font_pointer_list *f = font_list; f; f = f->next) {
    html_font *psf = (html_font *)(f->p);
  }
  html.begin_comment("Total number of pages: ").comment_arg(itoa(no_of_printed_pages)).end_comment();
  html.end_line();
  html.copy_file(tempfp);
  fputs("</body>\n", stdout);
  fputs("</html>\n", stdout);
  fclose(tempfp);
}


/*
 *  calculate_region_range - calculates the vertical range for words and lines
 *                           within the region lists.
 */

void html_printer::calculate_region_range (graphic_glob *r)
{
  text_glob    *w;
  graphic_glob *g;

  if (! page_contents->region_lines.is_empty()) {
    page_contents->region_lines.start_from_head();
    do {
      g = page_contents->region_lines.get_data();
      if ((r->minv == -1) || (g->minv < r->minv)) {
	r->minv = g->minv;
      }
      if ((r->maxv == -1) || (g->maxv > r->maxv)) {
	r->maxv = g->maxv;
      }
      page_contents->region_lines.move_right();
    } while (! page_contents->region_lines.is_equal_to_head());
  }
  if (! page_contents->region_words.is_empty()) {
    page_contents->region_words.start_from_head();
    do {
      w = page_contents->region_words.get_data();

      if ((r->minv == -1) || (w->minv < r->minv)) {
	r->minv = w->minv;
      }
      if ((r->maxv == -1) || (w->maxv > r->maxv)) {
	r->maxv = w->maxv;
      }
      page_contents->region_words.move_right();
    } while (! page_contents->region_words.is_equal_to_head());
  }
}


/*
 *  move_region_to_page - moves lines and words held in the temporary region
 *                        list to the page list.
 */

void html_printer::move_region_to_page (void)
{
  text_glob    *w;
  graphic_glob *g;

  page_contents->region_lines.start_from_head();
  while (! page_contents->region_lines.is_empty()) {
    g = page_contents->region_lines.get_data();   // remove from our temporary region list
    page_contents->lines.add(g);                  // and add to the page list
    page_contents->region_lines.sub_move_right();
  }
  page_contents->region_words.start_from_head();
  while (! page_contents->region_words.is_empty()) {
    w = page_contents->region_words.get_data();   // remove from our temporary region list
    page_contents->words.add(w);                  // and add to the page list
    page_contents->region_words.sub_move_right();
  }
}


void html_printer::special(char *s, const environment *env)
{
  if (s != 0) {
    if (strcmp(s, "graphic-start") == 0) {
      graphic_level++;
      if (graphic_level == 1) {
	page_contents->is_in_graphic = TRUE;    // add words and lines to temporary region lists
      }
    } else if ((strcmp(s, "graphic-end") == 0) && (graphic_level > 0)) {
      graphic_level--;
      if (graphic_level == 0) {
	flush_graphic();
      }
    } else if (strncmp(s, "html:", 5) == 0) {
      int r=font::res;   // resolution of the device actually

      page_contents->add_html_command(&sbuf_style, &s[5], strlen(s)-5,

      // need to pass rest of string through to html output during flush

				      env->vpos-env->size*r/72, env->hpos,
				      env->vpos            , env->hpos);
      // assume that the html command has no width, if it does then we hopefully troff
      // will have fudged this in a macro and requested that the formatting move right by
      // the appropriate width
    } else if (strncmp(s, "index:", 6) == 0) {
      cutoff_heading = atoi(&s[6]);
    }
  }
}

void set_image_type (char *type)
{
  if (strcmp(type, "gif") == 0) {
    image_type = gif;
  } else if (strcmp(type, "png") == 0) {
    image_type = png;
    image_device = "png256";
  } else if (strncmp(type, "png", 3) == 0) {
    image_type = png;
    image_device = type;
  }
}

// A conforming PostScript document must not have lines longer
// than 255 characters (excluding line termination characters).

static int check_line_lengths(const char *p)
{
  for (;;) {
    const char *end = strchr(p, '\n');
    if (end == 0)
      end = strchr(p, '\0');
    if (end - p > 255)
      return 0;
    if (*end == '\0')
      break;
    p = end + 1;
  }
  return 1;
}

printer *make_printer()
{
  return new html_printer;
}

static void usage();

int main(int argc, char **argv)
{
  program_name = argv[0];
  static char stderr_buf[BUFSIZ];
  setbuf(stderr, stderr_buf);
  int c;
  while ((c = getopt(argc, argv, "F:atvdgmx?I:r:")) != EOF)
    switch(c) {
    case 'v':
      {
	extern const char *version_string;
	fprintf(stderr, "grohtml version %s\n", version_string);
	fflush(stderr);
	break;
      }
    case 'a':
      auto_on = FALSE;
      break;
    case 't':
      table_on = FALSE;
      break;
    case 'F':
      font::command_line_font_dir(optarg);
      break;
    case 'I':
      // user specifying the type of images we should generate
      set_image_type(optarg);
      break;
    case 'r':
      // resolution (dots per inch for an image)
      image_res = atoi(optarg);
      break;
    case 'd':
      // debugging on
      debug_on       = TRUE;
      break;
    case 'x':
      debug_table_on = TRUE;
      break;
    case 'g':
      // do not guess title and headings
      guess_on = FALSE;
      break;
    case 'm':
      // leave margins alone
      margin_on = TRUE;
      break;
    case '?':
      usage();
      break;
    default:
      assert(0);
    }
  if (optind >= argc) {
    do_file("-");
  } else {
    for (int i = optind; i < argc; i++)
      do_file(argv[i]);
  }
  delete pr;
  return 0;
}

static void usage()
{
  fprintf(stderr, "usage: %s [-avdgmt?] [-r resolution] [-F dir] [-I imagetype] [files ...]\n",
	  program_name);
  exit(1);
}
