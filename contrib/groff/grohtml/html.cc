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
#include "html_chars.h"
#include <time.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

extern "C" {
  // SunOS 4.1.3 fails to declare this in stdlib.h
  char *mktemp(char *);
}

#include <stdio.h>
#include <fcntl.h>

#ifndef _POSIX_VERSION

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif /* HAVE_LIMITS_H */

#ifdef HAVE_DIRENT_H
#include <dirent.h>
#else /* not HAVE_DIRENT_H */
#ifdef HAVE_SYS_DIR_H
#include <sys/dir.h>
#endif /* HAVE_SYS_DIR_H */
#endif /* not HAVE_DIRENT_H */

#ifndef NAME_MAX
#ifdef MAXNAMLEN
#define NAME_MAX MAXNAMLEN
#else /* !MAXNAMLEN */
#ifdef MAXNAMELEN
#define NAME_MAX MAXNAMELEN
#else /* !MAXNAMELEN */
#define NAME_MAX 14
#endif /* !MAXNAMELEN */
#endif /* !MAXNAMLEN */
#endif /* !NAME_MAX */

#endif /* not _POSIX_VERSION */

#include "nonposix.h"

#include "ordered_list.h"

#if !defined(TRUE)
#   define TRUE  (1==1)
#endif
#if !defined(FALSE)
#   define FALSE (1==0)
#endif

#define MAX_TEMP_NAME                1024
#define MAX_STRING_LENGTH            4096
#define MAX_CHAR_SIZE                  50        // maximum length of character name

#define Y_FUDGE_MARGIN              +0.83
#define A4_PAGE_LENGTH              (11.6944-Y_FUDGE_MARGIN)
#define DEFAULT_IMAGE_RES              80
#define IMAGE_BOARDER_PIXELS           10
#define MAX_WORDS_PER_LINE           1000        // only used for table indentation
#define GAP_SPACES                      3        // how many spaces needed to guess a gap?
#define GAP_WIDTH_ONE_LINE              2        // 1/GAP_WIDTH_ONE_LINE inches required for one line table
#define CENTER_TOLERANCE                2        // how many pixels off center will we think a line or region is centered
#define MIN_COLUMN                      7        // minimum column size pixels for multiple lines
#define MIN_COLUMN_FOR_TWO_LINES       20        // minimum column size pixels for a 2 line table
#define MIN_TEXT_PERCENT                5        // try and round to this percentage value for used columns
#define PERCENT_THRESHOLD              20        // don't bother trying to increase and width greater than this


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
static int table_image_on           =  TRUE;   // default is to create images for tbl

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
  : f(p), point_size(sz), font_no(no), height(h), slant(sl)
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
: used(0), next(0)
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
    minv(min_vertical), maxv(max_vertical), minh(min_horizontal), maxh(max_horizontal),
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
  if (is_intersection(a->minv+1, a->maxv-1, b->minv+1, b->maxv-1)) {
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
  : minv(-1), maxv(-1), minh(-1), maxh(-1), nopoints(0), point(0), size(0), code(0)
{
}

graphic_glob::~graphic_glob ()
{
  if (point != 0) {
    free(point);
  }
}

graphic_glob::graphic_glob (int troff_code)
  : minv(-1), maxv(-1), minh(-1), maxh(-1), nopoints(0), point(0), size(0), code(troff_code)
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
      // stop();
    }
    g->maxh = g->minh;
  }
  if (g->maxv < g->minv) {
    if (debug_on) {
      fprintf(stderr, "assert failed minv > maxv\n"); fflush(stderr);
      // stop();
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

class html_font : public font {
  html_font(const char *);
public:
  int encoding_index;
  char *encoding;
  char *reencoded_name;
  ~html_font();
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
  : has_been_written(FALSE), has_been_found(FALSE)
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
  int right_hits; // count of the number of words touching right position
  int percent;    // what percentage width should we use for this cell?
};

/*
 *  introduce a paragraph class so that we can nest paragraphs
 *  from plain html text and html tables.
 */

class html_paragraph {
public:
                            html_paragraph (int in, int need, paragraph_type type, html_paragraph *prev);
                           ~html_paragraph ();

  int                      in_paragraph;
  int                      need_paragraph;
  paragraph_type           para_type;
  html_paragraph          *previous;
};

/*
 *  html_paragraph - constructor, fill in the public fields.
 */

html_paragraph::html_paragraph (int in, int need, paragraph_type type, html_paragraph *prev)
  : in_paragraph(in), need_paragraph(need),
    para_type(type), previous(prev)
{
}

/*
 *  html_paragraph - deconstructor
 */

html_paragraph::~html_paragraph ()
{
}

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
  int                       wrap_margin;       // is the current rightmost margin able to wrap words?
};

html_table::html_table ()
  : no_of_columns(0), columns(0), vertical_limit(0), wrap_margin(0)
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
  enum                 { SBUF_SIZE = 8192 };
  char                 sbuf[SBUF_SIZE];
  int                  sbuf_len;
  int                  sbuf_start_hpos;
  int                  sbuf_vpos;
  int                  sbuf_end_hpos;
  int                  sbuf_kern;
  style                sbuf_style;
  int                  sbuf_dmark_hpos;
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
  int                  header_indent;
  page                *page_contents;
  html_table           indentation;
  int                  left_margin_indent;
  int                  right_margin_indent;
  int                  need_one_newline;
  int                  issued_newline;
  html_paragraph      *current_paragraph;
  char                 image_name[MAX_STRING_LENGTH];
  int                  image_number;
  int                  graphic_level;
  int                  supress_sub_sup;

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
  void  add_char_to_sbuf              (unsigned char code);
  void  add_to_sbuf                   (char code, const char *name);
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
  int   looks_like_smaller_font       (text_glob *g);
  int   looks_like_larger_font        (text_glob *g);
  void  begin_paragraph               (paragraph_type p);
  void  begin_paragraph_no_height     (paragraph_type p);
  void  force_begin_paragraph         (void);
  void  end_paragraph                 (void);
  void  save_paragraph                (void);
  void  restore_paragraph             (void);
  void  html_newline                  (void);
  void  convert_to_image              (char *troff_src, char *image_name);
  void  write_title                   (int in_head);
  void  find_title                    (void);
  int   is_bold                       (text_glob *g);
  void  write_header                  (text_glob *g);
  void  determine_header_level        (void);
  void  build_header                  (text_glob *g);
  void  make_html_indent              (int indent);
  int   is_whole_line_bold            (text_glob *g);
  int   is_a_header                   (text_glob *g);
  int   processed_header              (text_glob *g);
  void  make_new_image_name           (void);
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
  int   collect_columns               (struct text_defn *next_words, struct text_defn *next_cols,
				       struct text_defn *last_words, struct text_defn *last_cols,
				       int max_words);
  void  include_into_list             (struct text_defn *line, struct text_defn *item);
  int   is_in_column                  (struct text_defn *line, struct text_defn *item, int max_words);
  int   is_column_match               (struct text_defn *match, struct text_defn *line1,
				       struct text_defn *line2, int max_words);
  int   count_columns                 (struct text_defn *line);
  void  rewind_text_to                (text_glob *g);
  int   found_use_for_table           (text_glob *start);
  void  column_display_word           (int cell, int vert, int left, int right, int next);
  void  start_table                   (void);
  void  end_table                     (void);
  void  foreach_column_include_text   (text_glob *start);
  void  define_cell                   (int i);
  int   column_calculate_left_margin  (int left, int right);
  int   column_calculate_right_margin (int left, int right);
  void  display_columns               (const char *word, const char *name, text_defn *line);
  void  calculate_right               (struct text_defn *line, int max_words);
  void  determine_right_most_column   (struct text_defn *line, int max_words);
  int   remove_white_using_words      (struct text_defn *next_guess, struct text_defn *last_guess, struct text_defn *next_line);
  void  copy_line                     (struct text_defn *dest, struct text_defn *src);
  void  combine_line                  (struct text_defn *dest, struct text_defn *src);
  int   conflict_with_words           (struct text_defn *column_guess, struct text_defn *words);
  void  remove_entry_in_line          (struct text_defn *line, int j);
  void  remove_redundant_columns      (struct text_defn *line);
  void  add_column_gaps               (struct text_defn *line);
  int   continue_searching_column     (text_defn *next_col, text_defn *last_col, text_defn *all_words);
  void  add_right_full_width          (struct text_defn *line, int mingap);
  int   is_continueous_column         (text_defn *last_col, text_defn *next_line);
  int   is_exact_left                 (text_defn *last_col, text_defn *next_line);
  int   find_column_index_in_line     (text_glob *t, text_defn *line);
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
  void  count_hits                    (text_defn *col, int no_of_columns, int limit);
  void  count_right_hits              (text_defn *col, int no_of_columns);
  int   calculate_min_gap             (text_glob *g);
  int   right_indentation             (struct text_defn *last_guess);
  void  calculate_percentage_width    (text_glob *start);
  int   able_to_steal_width           (void);
  int   need_to_steal_width           (void);
  int   can_distribute_fairly         (void);
  void  utilize_round_off             (void);
  int   will_wrap_text                (int i, text_glob *start);
  int   next_line_on_left_column      (int i, text_glob *start);
  void  remove_table_column           (int i);
  void  remove_unnecessary_unused     (text_glob *start);
  int   is_small_table                (int lines, struct text_defn *last_guess,
				       struct text_defn *words_1, struct text_defn *cols_1,
				       struct text_defn *words_2, struct text_defn *cols_2,
				       int *limit, int *limit_1);
  int   is_column_subset              (struct text_defn *cols_1, struct text_defn *cols_2);
  int   is_appropriate_to_start_table (struct text_defn *cols_1, struct text_defn *cols_2,
				       struct text_defn *last_guess);
  int   is_a_full_width_column        (void);
  int   right_most_column             (struct text_defn *col);
  int   large_enough_gap_for_two      (struct text_defn *col);
  void  remove_zero_percentage_column (void);
  void  translate_to_html             (text_glob *g);
  int   html_knows_about              (char *troff);
  void  determine_diacritical_mark    (const char *name, const environment *env);
  int   sbuf_continuation             (unsigned char code, const char *name, const environment *env, int w);
  char *remove_last_char_from_sbuf    ();
  const char *check_diacritical_combination (unsigned char code, const char *name);
  int   seen_backwards_escape         (char *s, int l);
  int   should_defer_table            (int lines, struct text_glob *start, struct text_defn *cols_1);
  int   is_new_exact_right            (struct text_defn *last_guess, struct text_defn *last_cols, struct text_defn *next_cols);
  void  issue_left_paragraph          (void);
  void  adjust_margin_percentages     (void);
  int   total_percentages             (void);
  int   get_left                      (void);
  void  can_loose_column              (text_glob *start, struct text_defn *last_guess, int limit);
  int   check_lack_of_hits            (struct text_defn *next_guess, struct text_defn *last_guess, text_glob *start, int limit);
  int   is_in_table                   (void);

  // ADD HERE

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
: html(0, MAX_LINE_LENGTH),
  troff(0, MAX_LINE_LENGTH),
  no_of_printed_pages(0),
  sbuf_len(0),
  sbuf_dmark_hpos(-1),
  output_hpos(-1),
  output_vpos(-1),
  line_thickness(-1),
  fill(FILL_MAX + 1),
  inside_font_style(0),
  page_number(0),
  header_indent(-1),
  left_margin_indent(0),
  right_margin_indent(0),
  need_one_newline(0),
  issued_newline(0),
  image_number(0),
  graphic_level(0),
  supress_sub_sup(TRUE),
  start_region_vpos(0),
  start_region_hpos(0),
  end_region_vpos(0),
  end_region_hpos(0),
  cutoff_heading(100)
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
  res               = r;
  html.set_fixed_point(point);
  space_char_index  = font::name_to_index("space");
  paper_length      = font::paperlength;
  if (paper_length == 0)
    paper_length    = 11*font::res;
  page_contents     = new page;

  postscript_res    = 72000;
  current_paragraph = new html_paragraph(FALSE, FALSE, left_alignment, 0);
}

/*
 *  add_char_to_sbuf - adds a single character to the sbuf.
 */

void html_printer::add_char_to_sbuf (unsigned char code)
{
  if (sbuf_len < SBUF_SIZE) {
    sbuf[sbuf_len] = code;
    sbuf_len++;
  } else {
    fatal("need to increase SBUF_SIZE");
  }
}

/*
 *  add_to_sbuf - adds character code or name to the sbuf.
 *                It escapes \ with \\
 *                We need to preserve the name of characters if they exist
 *                because we may need to send this character to two different
 *                devices: html and postscript.
 */

void html_printer::add_to_sbuf (char code, const char *name)
{
  if (name == 0) {
    if (code == '\\') {
      add_char_to_sbuf('\\');
    }
    add_char_to_sbuf(code);
  } else {
    int l=strlen(name);
    int i=0;
    
    add_char_to_sbuf('\\');
    add_char_to_sbuf('(');
    while (i<l) {
      if (name[i] == '\\') {
	add_char_to_sbuf('\\');
      }
      add_char_to_sbuf(name[i]);
      i++;
    }
    add_char_to_sbuf('\\');
    add_char_to_sbuf(')');
  }
}

int html_printer::sbuf_continuation (unsigned char code, const char *name,
				     const environment *env, int w)
{
  if ((sbuf_end_hpos == env->hpos) || (sbuf_dmark_hpos == env->hpos)) {
    name = check_diacritical_combination(code, name);
    add_to_sbuf(code, name);
    determine_diacritical_mark(name, env);
    sbuf_end_hpos += w + sbuf_kern;
    return( TRUE );
  } else {
    if ((sbuf_len < SBUF_SIZE-1) && (env->hpos >= sbuf_end_hpos) &&
	((sbuf_kern == 0) || (sbuf_end_hpos - sbuf_kern != env->hpos))) {
      /*
       *  lets see whether a space is needed or not
       */
      int space_width = sbuf_style.f->get_space_width(sbuf_style.point_size);

      if (env->hpos-sbuf_end_hpos < space_width) {
	name = check_diacritical_combination(code, name);
	add_to_sbuf(code, name);
	determine_diacritical_mark(name, env);
	sbuf_end_hpos = env->hpos + w;
	return( TRUE );
      }
    } else if ((sbuf_len > 0) && (sbuf_dmark_hpos)) {
      /*
       *  check whether the diacritical mark is on the same character
       */
      int space_width = sbuf_style.f->get_space_width(sbuf_style.point_size);

      if (abs(sbuf_dmark_hpos-env->hpos) < space_width) {
	name = check_diacritical_combination(code, name);
	add_to_sbuf(code, name);
	determine_diacritical_mark(name, env);
	sbuf_end_hpos = env->hpos + w;
	return( TRUE );
      }
    }
  }
  return( FALSE );
}

/*
 *  seen_backwards_escape - returns TRUE if we can see a escape at position i..l in s
 */

int html_printer::seen_backwards_escape (char *s, int l)
{
  /*
   *  this is tricky so it is broken into components for clarity
   *  (we let the compiler put in all back into a complex expression)
   */
  if ((l>0) && (sbuf[l] == '(') && (sbuf[l-1] == '\\')) {
    /*
     *  ok seen '\(' but we must now check for '\\('
     */
    if ((l>1) && (sbuf[l-2] == '\\')) {
      /*
       *  escaped the escape
       */
      return( FALSE );
    } else {
      return( TRUE );
    }
  } else {
    return( FALSE );
  }
}

/*
 *  reverse - return reversed string.
 */

char *reverse (char *s)
{
  int i=0;
  int j=strlen(s)-1;
  char t;

  while (i<j) {
    t = s[i];
    s[i] = s[j];
    s[j] = t;
    i++;
    j--;
  }
  return( s );
}

/*
 *  remove_last_char_from_sbuf - removes the last character from sbuf.
 */

char *html_printer::remove_last_char_from_sbuf ()
{
  int l=sbuf_len;
  static char last[MAX_STRING_LENGTH];

  if (l>0) {
    l--;
    if ((sbuf[l] == ')') && (l>0) && (sbuf[l-1] == '\\')) {
      /*
       *  found terminating escape
       */
      int i=0;

      l -= 2;
      while ((l>0) && (! seen_backwards_escape(sbuf, l))) {
	if (sbuf[l] == '\\') {
	  if (sbuf[l-1] == '\\') {
	    last[i] = sbuf[l];
	    i++;
	    l--;
	  }
	} else {
	  last[i] = sbuf[l];
	  i++;
	}
	l--;
      }
      last[i] = (char)0;
      sbuf_len = l;
      if (seen_backwards_escape(sbuf, l)) {
	sbuf_len--;
      }
      return( reverse(last) );
    } else {
      if ((sbuf[l] == '\\') && (l>0) && (sbuf[l-1] == '\\')) {
	l -= 2;
	sbuf_len = l;
	return( "\\" );
      } else {
	sbuf_len--;
	last[0] = sbuf[sbuf_len];
	last[1] = (char)0;
	return( last );
      }
    }  
  } else {
    return( NULL );
  }
}

/*
 *  check_diacriticial_combination - checks to see whether the character code
 *                                   if combined with the previous diacriticial mark
 *                                   forms a new character.
 */

const char *html_printer::check_diacritical_combination (unsigned char code, const char *name)
{
  static char troff_char[2];

  if ((name == 0) && (sbuf_dmark_hpos >= 0)) {
    // last character was a diacritical mark
    char *last = remove_last_char_from_sbuf();

    int i=0;
    int j;
    
    while (diacritical_table[i].mark != NULL) {
      if (strcmp(diacritical_table[i].mark, last) == 0) {
	j=0;
	while ((diacritical_table[i].second_troff_char[j] != (char)0) &&
	       (diacritical_table[i].second_troff_char[j] != code)) {
	  j++;
	}
	if (diacritical_table[i].second_troff_char[j] == code) {
	  troff_char[0] = diacritical_table[i].translation;
	  troff_char[1] = code;
	  troff_char[2] = (char)0;
	  return( troff_char );
	}
      }
      i++;
    }
    add_to_sbuf(last[0], last);
  }
  return( name );
}

/*
 *  determine_diacritical_mark - if name is a diacriticial mark the record the position.
 *                                --fixme-- is there a better way of doing this
 *                                this must be done in troff somewhere.
 */

void html_printer::determine_diacritical_mark (const char *name, const environment *env)
{
  if (name != 0) {
    int i=0;

    while (diacritical_table[i].mark != NULL) {
      if (strcmp(name, diacritical_table[i].mark) == 0) {
	sbuf_dmark_hpos = env->hpos;
	return;
      }
      i++;
    }
  }
  sbuf_dmark_hpos = -1;
}

/*
 *  set_char - adds a character into the sbuf if it is a continuation with the previous
 *             word otherwise flush the current sbuf and add character anew.
 */

void html_printer::set_char(int i, font *f, const environment *env, int w, const char *name)
{
  unsigned char code = f->get_code(i);

#if 0
  if (code == ' ') {
    stop();
  }
#endif
  style sty(f, env->size, env->height, env->slant, env->fontno);
  if (sty.slant != 0) {
    if (sty.slant > 80 || sty.slant < -80) {
      error("silly slant `%1' degrees", sty.slant);
      sty.slant = 0;
    }
  }
  if ((name != 0) && (page_contents->is_in_graphic)) {
    flush_sbuf();
    int r=font::res;   // resolution of the device
    page_contents->add_special_char(&sty, (char *)name, strlen(name),
				    env->vpos-sty.point_size*r/72, env->hpos,
				    env->vpos                    , env->hpos+w);
    sbuf_end_hpos   = env->hpos + w;
    sbuf_start_hpos = env->hpos;
    sbuf_vpos       = env->vpos;
    sbuf_style      = sty;
    sbuf_kern       = 0;
  } else {
    if ((sbuf_len > 0) && (sbuf_len < SBUF_SIZE) && (sty == sbuf_style) &&
	(sbuf_vpos == env->vpos) && (sbuf_continuation(code, name, env, w))) {
      return;
    } else {
      flush_sbuf();
      sbuf_len = 0;
      add_to_sbuf(code, name);
      determine_diacritical_mark(name, env);
      sbuf_end_hpos = env->hpos + w;
      sbuf_start_hpos = env->hpos;
      sbuf_vpos = env->vpos;
      sbuf_style = sty;
      sbuf_kern = 0;
    }
  }
}

/*
 *  file_name_max - return the maximum file-name length permitted
 *                  by the underlying filesystem.
 *
 *  (Code shamelessly stolen from indxbib/dirnamemax.c.)
 */

static size_t
file_name_max (const char *fname)
{
#ifdef _POSIX_VERSION
  return pathconf (fname, _PC_NAME_MAX);
#else
  return NAME_MAX;
#endif
}


/*
 *  make_new_image_name - creates a new file name ready for a image file.
 */

void html_printer::make_new_image_name (void)
{
  image_number++;

  if ((current_filename == 0) ||
      (strcmp(current_filename, "<standard input>") == 0) ||
      (strcmp(current_filename, "-") == 0) ||
      (strcspn(current_filename, DIR_SEPS) < strlen(current_filename))) {
    if (file_name_max(".") > 14)
      sprintf(image_name, "groff-html-%d-%ld", image_number, (long)getpid());
    else
      // The "-gh" part might be truncated on MS-DOS, but there's enough
      // space for the PID and up to 99 image numbers.  That's why "-gh"
      // comes last.
      sprintf(image_name, "%d-%ld-gh", image_number, (long)getpid());
  } else if (file_name_max(".") > 14) {
    sprintf(image_name, "%s-%d-%ld", current_filename, image_number, (long)getpid());
  } else { // see the commentary above
    sprintf(image_name, "%d-%ld-%s",
	    image_number, (long)getpid(), current_filename);
    // Make sure image_name does not have a dot in its trunk, since
    // convert_to_image will append .gif or .png to it, and DOS doesn't
    // allow more than a single dot in a file name.
    int i = strlen(image_name);
    for ( ; i > 0; i--) {
      if (strchr(DIR_SEPS, image_name[i - 1]))
	break;
      if (image_name[i - 1] == '.') {
	image_name[i - 1] = '\0';
	break;
      }
    }
  }
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
 *  get_html_translation - given the position of the character and its name
 *                         return the device encoding for such character.
 */

char *get_html_translation (font *f, char *name)
{
  int  index;

  if ((f == 0) || (name == 0) || (strcmp(name, "") == 0)) {
    return( NULL );
  } else {
    index = f->name_to_index(name);
    if (index == 0) {
      error("character `%s' not found", name);
      return( NULL );
    } else {
      return( (char *)f->get_special_device_encoding(index) );
    }
  }
}

/*
 *  str_translate_to_html - converts a string, str, into html text. It places
 *                          the output input buffer, buf. It truncates string, str, if
 *                          there is not enough space in buf.
 *                          It looks up the html character encoding of single characters
 *                          if, and_single, is TRUE. Characters such as < > & etc.
 */

void str_translate_to_html (font *f, char *buf, int buflen, char *str, int len, int and_single)
{
  int         l;
  char       *translation;
  int         e;
  char        escaped_char[MAX_STRING_LENGTH];
  int         i=0;
  int         b=0;
  int         t=0;

#if 0
  if (strcmp(str, "\\(\\\\-\\)") == 0) {
    stop();
  }
#endif
  while (str[i] != (char)0) {
    if ((str[i]=='\\') && (i+1<len)) {
      i++; // skip the leading backslash
      if (str[i] == '(') {
	// start of escape
	i++;
	e = 0;
	while ((str[i] != (char)0) &&
	       (! ((str[i] == '\\') && (i+1<len) && (str[i+1] == ')')))) {
	  if (str[i] == '\\') {
	    i++;
	  }
	  escaped_char[e] = str[i];
	  e++;
	  i++;
	}
	if ((str[i] == '\\') && (i+1<len) && (str[i+1] == ')')) {
	  i += 2;
	}
	escaped_char[e] = (char)0;
	if (e > 0) {
	  translation = get_html_translation(f, escaped_char);
	  if (translation) {
	    l = strlen(translation);
	    t = max(0, min(l, buflen-b));
	    strncpy(&buf[b], translation, t);
	    b += t;
	  } else {
	    int index=f->name_to_index(escaped_char);
	  
	    if (index != 0) {
	      buf[b] = f->get_code(index);
	      b++;
	    }
	  }
	}
      }
    } else {
      if (and_single) {
	char name[2];

	name[0] = str[i];
	name[1] = (char)0;
	translation = get_html_translation(f, name);
	if (translation) {
	  l = strlen(translation);
	  t = max(0, min(l, buflen-b));
	  strncpy(&buf[b], translation, t);
	  b += t;
	} else {
	  if (b<buflen) {
	    buf[b] = str[i];
	    b++;
	  }
	}
      } else {
	/*
	 *  do not attempt to encode single characters
	 */
	if (b<buflen) {
	  buf[b] = str[i];
	  b++;
	}
      }
      i++;
    }
  }
  buf[min(b, buflen)] = (char)0;
}

/*
 *  find_title - finds a title to this document, if it exists.
 */

void html_printer::find_title (void)
{
  text_glob    *t;
  int           r=font::res;
  int           removed_from_head;
  char          buf[MAX_STRING_LENGTH];

  if ((page_number == 1) && (guess_on)) {
    if (! page_contents->words.is_empty()) {

      int end_title_hpos     = 0;
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
	  page_contents->words.move_right(); 	  // move onto next word
	} else if ((!found_title_start) && (t->minh > left_margin_indent) &&
		   ((start_region == -1) || (t->maxv < start_region))) {
	  start_title_vpos     = t->minv;
	  end_title_hpos       = t->minh;
	  str_translate_to_html(t->text_style.f, buf, MAX_STRING_LENGTH, t->text_string, t->text_length, TRUE);
	  strcpy((char *)title.text, buf);
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
	    str_translate_to_html(t->text_style.f, buf, MAX_STRING_LENGTH, t->text_string, t->text_length, TRUE);
	    strcat(title.text, buf);
	    page_contents->words.sub_move_right();
	    removed_from_head = ((!page_contents->words.is_empty()) &&
				 (page_contents->words.is_equal_to_head()));
	  } else {
	    // end of title
	    title.has_been_found = TRUE;
	    return;
	  }
	} else if (t->minh <= left_margin_indent) {
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

  if (current_paragraph->in_paragraph) {
    // safe to generate a pretty newline
    html.put_string("<br>\n");
  } else {
    html.put_string("<br>");
  }
  output_vpos += height;
  issued_newline = TRUE;
}

/*
 *  issue_left_paragraph - emits a left paragraph together with appropriate
 *                         margin if header_indent is < left_margin_indent.
 */

void html_printer::issue_left_paragraph (void)
{
  if ((header_indent < left_margin_indent) && (! using_table_for_indent())) {
    html.put_string("<p style=\"margin-left: ");
    html.put_number(((left_margin_indent-header_indent)*100)/(right_margin_indent-header_indent));
    html.put_string("%\">");
  } else {
    html.put_string("<p>");
  }
}

/*
 *  force_begin_paragraph - force the begin_paragraph to be emitted.
 */

void html_printer::force_begin_paragraph (void)
{
  if (current_paragraph->in_paragraph && current_paragraph->need_paragraph) {
    switch (current_paragraph->para_type) {

    case left_alignment:   issue_left_paragraph();
                           break;
    case center_alignment: html.put_string("<p align=center>");
                           break;
    default:               fatal("unknown paragraph alignment type");
    }
    current_paragraph->need_paragraph = FALSE;
  }
}

/*
 *  begin_paragraph - starts a new paragraph. It does nothing if a paragraph
 *                    has already been started.
 */

void html_printer::begin_paragraph (paragraph_type p)
{
  if (! current_paragraph->in_paragraph) {
    int r                             = font::res;
    int height                        = output_style.point_size*r/72;

    if (output_vpos >=0) {
      // we leave it alone if it is set to the top of page
      output_vpos += height;
    }
    current_paragraph->need_paragraph = TRUE;   // delay the <p> just in case we don't actually emit text
    current_paragraph->in_paragraph   = TRUE;
    current_paragraph->para_type      = p;
    issued_newline                    = TRUE;
  }
}


/*
 *  begin_paragraph_no_height - starts a new paragraph. It does nothing if a paragraph
 *                              has already been started. Note it does not alter output_vpos.
 */

void html_printer::begin_paragraph_no_height (paragraph_type p)
{
  if (! current_paragraph->in_paragraph) {
    current_paragraph->need_paragraph = TRUE;   // delay the <p> just in case we don't actually emit text
    current_paragraph->in_paragraph   = TRUE;
    current_paragraph->para_type      = p;
    issued_newline                    = TRUE;
  }
}

/*
 *  end_paragraph - end the current paragraph. It does nothing if a paragraph
 *                  has not been started.
 */

void html_printer::end_paragraph (void)
{
  if (current_paragraph->in_paragraph) {
    // check whether we have generated any text inbetween the potential paragraph begin end
    if (! current_paragraph->need_paragraph) {
      int r        = font::res;
      int height   = output_style.point_size*r/72;

      output_vpos += height;
      terminate_current_font();
      html.put_string("</p>\n");
    } else {
      terminate_current_font();
    }
    current_paragraph->para_type    = left_alignment;
    current_paragraph->in_paragraph = FALSE;
  }
}

/*
 *  save_paragraph - saves the current paragraph state and
 *                   creates new paragraph state.
 */

void html_printer::save_paragraph (void)
{
  if (current_paragraph == 0) {
    fatal("current_paragraph is NULL");
  }
  current_paragraph = new html_paragraph(current_paragraph->in_paragraph,
					 current_paragraph->need_paragraph,
					 current_paragraph->para_type,
					 current_paragraph);
  terminate_current_font();
}

/*
 *  restore_paragraph - restores the previous paragraph state.
 */

void html_printer::restore_paragraph (void)
{
  html_paragraph *old = current_paragraph;

  current_paragraph = current_paragraph->previous;
  free(old);
}

/*
 *  calculate_margin - runs through the words and graphics globs
 *                     and finds the start of the left most margin.
 */

void html_printer::calculate_margin (void)
{
  text_glob    *w;
  graphic_glob *g;

  // remove margin

  right_margin_indent = 0;

  if (! page_contents->words.is_empty()) {
    
    // firstly check the words to determine the right margin

    page_contents->words.start_from_head();
    do {
      w = page_contents->words.get_data();
      if ((w->maxh >= 0) && (w->maxh > right_margin_indent)) {
	right_margin_indent = w->maxh;
#if 0
	if (right_margin_indent == 758) stop();
#endif
      }
      page_contents->words.move_right();
    } while (! page_contents->words.is_equal_to_head());
    
    /*
     *  only examine graphics if no words present
     */
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


    /*
     *  now we know the right margin lets do the same to find left margin
     */

    if (header_indent == -1) {
      header_indent     = right_margin_indent;
    }
    left_margin_indent  = right_margin_indent;
      
    if (! page_contents->words.is_empty()) {
      do {
	w = page_contents->words.get_data();
	if ((w->minh >= 0) && (w->minh < left_margin_indent)) {
	  if (! is_a_header(w) && (! w->is_raw_command)) {
	    left_margin_indent = w->minh;
	  }
	}
	page_contents->words.move_right();
      } while (! page_contents->words.is_equal_to_head());
    }

    /*
     *  only examine graphic for margins if text yields nothing
     */
    
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

void html_printer::convert_to_image (char *troff_src, char *image_name)
{
  char buffer[MAX_STRING_LENGTH*2 + 200];
  char *ps_src = mktemp(xtmptemplate("-ps-"));

  sprintf(buffer, "grops%s %s > %s", EXE_EXT, troff_src, ps_src);
  if (debug_on) {
    fprintf(stderr, "%s\n", buffer);
  }
  int status = system(buffer);
  if (status == -1) {
    fprintf(stderr, "\"%s\" failed (no grops on PATH?)\n", buffer);
    return;
  }
  else if (status) {
    fprintf(stderr, "\"%s\" returned status %d\n", buffer, status);
  }

  if (image_type == gif) {
    sprintf(buffer,
	    "echo showpage | gs%s -q -dSAFER -sDEVICE=ppmraw -r%d -g%dx%d -sOutputFile=- %s - | ppmquant%s 256 | ppmtogif%s > %s.gif",
	    EXE_EXT, image_res,
	    (end_region_hpos-start_region_hpos)*image_res/font::res+IMAGE_BOARDER_PIXELS,
	    (end_region_vpos-start_region_vpos)*image_res/font::res+IMAGE_BOARDER_PIXELS,
	    ps_src, EXE_EXT, EXE_EXT, image_name);
  } else {
    sprintf(buffer,
	    "echo showpage | gs%s -q -dSAFER -sDEVICE=%s -r%d -g%dx%d -sOutputFile=- %s - > %s.png",
	    EXE_EXT, image_device,
	    image_res,
	    (end_region_hpos-start_region_hpos)*image_res/font::res+IMAGE_BOARDER_PIXELS,
	    (end_region_vpos-start_region_vpos)*image_res/font::res+IMAGE_BOARDER_PIXELS,
	    ps_src, image_name);
#if 0
    sprintf(buffer,
	    "echo showpage | gs -q -dSAFER -sDEVICE=ppmraw -r%d -g%dx%d -sOutputFile=- %s.ps - > %s.pnm ; pnmtopng -transparent white %s.pnm > %s.png \n",
	    /* image_device, */
	    image_res,
	    (end_region_hpos-start_region_hpos)*image_res/font::res+IMAGE_BOARDER_PIXELS,
	    (end_region_vpos-start_region_vpos)*image_res/font::res+IMAGE_BOARDER_PIXELS,
	    name, name, name, image_name);
#endif
  }
  if (debug_on) {
    fprintf(stderr, "%s\n", buffer);
  }
  // Redirect standard error to the null device.  This is more
  // portable than using "2> /dev/null" inside the commands above,
  // since it doesn't require a Unixy shell.
  int save_stderr = dup(2);
  if (save_stderr > 2) {
    int fdnull = open(NULL_DEV, O_WRONLY|O_BINARY, 0666);
    if (fdnull > 2)
      dup2(fdnull, 2);
    if (fdnull >= 0)
      close(fdnull);
  }
  status = system(buffer);
  dup2(save_stderr, 2);
  if (status == -1) {
    fprintf(stderr,
	    "Conversion to image failed (no gs/ppmquant/ppmtogif on PATH?)\n");
  }
  else if (status) {
    fprintf(stderr,
	    "Conversion to image returned status %d\n", status);
  }
  unlink(ps_src);
  unlink(troff_src);
}

void html_printer::prologue (void)
{
  troff.put_string("x T ps\nx res ");
  troff.put_number(postscript_res);
  troff.put_string(" 1 1\nx init\np1\n");
}

void html_printer::display_globs (int is_to_html)
{
  text_glob    *t=0;
  graphic_glob *g=0;
  FILE         *f=0;
  char         *troff_src;
  int           something=FALSE;
  int           is_center=FALSE;

  end_paragraph();

  if (! is_to_html) {
    is_center = html_position_region();
    make_new_image_name();
    f = xtmpfile(&troff_src, "-troff-", FALSE);
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
      convert_to_image(troff_src, image_name);

      if (is_center) {
	end_paragraph();
	begin_paragraph(center_alignment);
	force_begin_paragraph();
      }
      html.put_string("<img src=\"");
      html.put_string(image_name);
      if (image_type == gif) {
	html.put_string(".gif\"");
      } else {
	html.put_string(".png\"");
      }
      html.put_string(">\n");
      html_newline();
      if (is_center) {
	end_paragraph();
      }

      output_vpos      = end_region_vpos;
      output_hpos      = 0;
      need_one_newline = FALSE;
      output_style.f   = 0;
      end_paragraph();
    }
  }
}

void html_printer::flush_page (void)
{
  calculate_margin();
  output_vpos     = -1;
  output_hpos     = get_left();
  supress_sub_sup = TRUE;
#if 0
  dump_page();
#endif
  html.begin_comment("left  margin: ").comment_arg(i_to_a(left_margin_indent)).end_comment();;
  html.begin_comment("right margin: ").comment_arg(i_to_a(right_margin_indent)).end_comment();;
  remove_redundant_regions();
  page_contents->calculate_region();
  remove_duplicate_regions();
  find_title();
  supress_sub_sup = TRUE;
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

  if ((output_style.point_size != size) && (output_style.point_size != 0)) {
    // shutdown the previous font size
    html.put_string("</font>");
  }

  if ((output_style.point_size != size) && (size != 0)) {
    // now emit the size if it has changed
    sprintf(buffer, "<font size=%d>", convertSizeToHTML(size));
    html.put_string(buffer);
    output_style.point_size = size;  // and remember the size
  }
  output_style.f = 0;                // no style at present
  output_style.point_size = size;    // remember current font size

  if (fontname != 0) {
    if (! g->is_raw_command) {
      // now emit the new font
      write_html_font_face(fontname, "<", ">");
  
      // now emit the new font type
      write_html_font_type(fontname, "<", ">");

      output_style = g->text_style;  // remember style for next time
    }
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
      }
      html_change_font(g, fontname, size);
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

void html_printer::write_header (text_glob *g)
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

      text_glob *h=new text_glob(&st,
				 header.headings.add_string(header.header_buffer, strlen(header.header_buffer)),
				 strlen(header.header_buffer),
				 header.no_of_headings, header.header_level,
				 header.no_of_headings, header.header_level,
				 FALSE, FALSE);
      header.headers.add(h);   // and add this header to the header list
    } else {
      terminate_current_font();
      end_paragraph();
    }

    // we adjust the margin if necessary

    if (g->minh < left_margin_indent) {
      header_indent = g->minh;
    }

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
 *  translate_str_to_html - translates a string, str, into html representation.
 *                          len indicates the string length.
 */

void translate_str_to_html (font *f, char *str, int len)
{
  char buf[MAX_STRING_LENGTH];

  str_translate_to_html(f, buf, MAX_STRING_LENGTH, str, len, TRUE);
  strncpy(str, buf, max(len, strlen(buf)+1));
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
  text_glob *l;
  int current_vpos;
  char buf[MAX_STRING_LENGTH];

  strcpy(header.header_buffer, "");
  do {
    l = g;
    current_vpos = g->minv;
    str_translate_to_html(g->text_style.f, buf, MAX_STRING_LENGTH, g->text_string, g->text_length, TRUE);
    strcat(header.header_buffer, (char *)buf);
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
  if ((guess_on) && (g->minh <= left_margin_indent) && (! using_table_for_indent()) &&
      (is_a_header(g))) {
    build_header(g);
    write_header(g);
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

/*
 *  looks_like_subscript - returns TRUE if, g, looks like a subscript.
 */

int html_printer::looks_like_subscript (text_glob *g)
{
  int r        = font::res;
  int height   = output_style.point_size*r/72;

  /* was return( ((output_vpos < g->minv) && (output_style.point_size != 0) &&
   *             (output_style.point_size > g->text_style.point_size)) );
   */

  return( (output_style.point_size != 0) && (! supress_sub_sup) && (output_vpos+height < g->maxv) );
}

/*
 *  looks_like_superscript - returns TRUE if, g, looks like a superscript.
 */

int html_printer::looks_like_superscript (text_glob *g)
{
  int r        = font::res;
  int height   = output_style.point_size*r/72;

/* was
 * return(((output_vpos > g->minv) && (output_style.point_size != 0) &&
 *        (output_style.point_size > g->text_style.point_size)));
 */

  return( (output_style.point_size != 0) && (! supress_sub_sup) && (output_vpos+height > g->maxv) );
}

/*
 *  looks_like_larger_font - returns TRUE if, g, can be treated as a larger font.
 *                           g needs to be on the same line
 */

int html_printer::looks_like_larger_font (text_glob *g)
{
  int r        = font::res;
  int height   = output_style.point_size*r/72;

  return( (output_vpos+height == g->maxv) && (output_style.point_size != 0) &&
	  (convertSizeToHTML(g->text_style.point_size)+1 == convertSizeToHTML(output_style.point_size)) );
}

/*
 *  looks_like_smaller_font - returns TRUE if, g, can be treated as a smaller font.
 *                            g needs to be on the same line
 */

int html_printer::looks_like_smaller_font (text_glob *g)
{
  int r        = font::res;
  int height   = output_style.point_size*r/72;

  return( (output_vpos+height == g->maxv) && (output_style.point_size != 0) &&
	  (convertSizeToHTML(g->text_style.point_size) == convertSizeToHTML(output_style.point_size)+1) );
}

/*
 *  pretend_is_on_same_line - returns TRUE if we think, g, is on the same line as the previous glob.
 *                            Note that it believes a single word spanning the left..right as being
 *                            on a different line.
 */

int html_printer::pretend_is_on_same_line (text_glob *g, int left_margin, int right_margin)
{
  return( auto_on && (right_margin == output_hpos) && (left_margin == g->minh) &&
	  (right_margin != g->maxh) && ((! is_whole_line_bold(g)) || (g->text_style.f == output_style.f)) &&
	  (! (using_table_for_indent()) || (indentation.wrap_margin)) );
}

int html_printer::is_on_same_line (text_glob *g, int vpos)
{
#if 0
  if (g->is_html_command) {
    stop();
  }
#endif
  return(
	 (vpos >= 0) &&
	 (is_intersection(vpos, vpos+g->text_style.point_size*font::res/72-1, g->minv, g->maxv))
	 );
}


/*
 *  make_html_indent - creates a relative indentation.
 */

void html_printer::make_html_indent (int indent)
{
  if ((indent > 0) && ((right_margin_indent-get_left()) > 0) &&
      ((indent*100)/(right_margin_indent-get_left()))) {
    html.put_string("<span style=\" text-indent: ");
    html.put_number((indent*100)/(right_margin_indent-get_left()));
    html.put_string("%;\"></span>");
  }
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
  text_glob *t = g;

  while ((t->is_raw_command) && (! page_contents->words.is_equal_to_tail()) &&
	 ((t->minv < end_region_vpos) || (end_region_vpos < 0))) {
    page_contents->words.move_right();
    t=page_contents->words.get_data();
  }
  rewind_text_to(g);
  if (t->is_raw_command) {
    return( font::res * 10 );  // impossibly large gap width
  } else {
    return( t->text_style.f->get_space_width(t->text_style.point_size)*GAP_SPACES );
  }
}

/*
 *  collect_columns - place html text in a column and return the vertical limit reached.
 */

int html_printer::collect_columns (struct text_defn *next_words,
				   struct text_defn *next_cols,
				   struct text_defn *last_words,
				   struct text_defn *last_cols,
				   int max_words)
{
  text_glob *start = page_contents->words.get_data();
  text_glob *t     = start;
  int  upper_limit = 0;

  /*
   *  initialize cols and words
   */
  next_words[0].left  = 0;
  next_words[0].right = 0;
  next_cols [0].left  = 0;
  next_cols [0].right = 0;

  /*
   *  if we have not reached the end collect the words on the current line
   */
  if (start != 0) {
    int graphic_limit = end_region_vpos;

    if (is_whole_line_bold(t) && (t->minh <= left_margin_indent) && (guess_on)) {
      /*
       *  found header therefore terminate indentation table.
       *  Return a negative number so we know a header has
       *  stopped the column
       */
      upper_limit = -t->minv;
    } else {
      int i      =0;    // is the index into next_cols
      int j      =0;    // is the column index for last_cols
      int k      =0;    // is the index into next_words
      int l      =0;    // is the index into next_words
      int prevh  =0;
      int mingap =calculate_min_gap(start);

      /*
       *  while words on the same line record them and any significant gaps
       */
      while ((t != 0) && (is_on_same_line(t, start->minv) && (i<max_words)) &&
	     ((graphic_limit == -1) || (graphic_limit > t->minv))) {

	/*
	 *  now find column index from the last line which corresponds to, t.
	 */
	j = find_column_index_in_line(t, last_cols);

	/*
	 *  now find word index from the last line which corresponds to, t.
	 */
	l = find_column_index_in_line(t, last_words);

	/*
	 *  Note t->minh might equal t->maxh when we are passing a special device character via \X
	 *       we currently ignore this when considering tables
	 *
	 *  if we have found a significant gap then record it
	 */
	if (((t->minh - prevh >= mingap) ||
	     ((last_cols  != 0) && (last_cols [j].right != 0) && (t->minh == last_cols [j].left))) &&
	    (t->minh != t->maxh)) {
	  next_cols[i].left    = t->minh;
	  next_cols[i].right   = t->maxh;
	  i++;
	  /*
	   *  terminate the array
	   */
	  if (i<max_words) {
	    next_cols[i].left  = 0;
	    next_cols[i].right = 0;
	  }
	} else if (i>0) {
	  /*
	   *  move previous right hand column to align with, t.
	   */
	  
	  if (t->minh > next_cols[i-1].left) {
	    /*
	     *  a simple precaution in case we get globs which are technically on the same line
	     *  (sadly this does occur sometimes - maybe we should be stricter with is_on_same_line)
	     *  --fixme--
	     */
	    next_cols[i-1].right = max(next_cols[i-1].right, t->maxh);
	  }
	}
	/*
	 *  remember to record the individual words
	 */
	next_words[k].left  = t->minh;
	next_words[k].right = t->maxh;
	k++;

	/*
	 *  and record the vertical upper limit
	 */
	upper_limit = max(t->minv, upper_limit);

	/*
	 *  and update prevh - used to detect a when a different line is seen
	 */
	prevh = t->maxh;

	/*
	 *  get next word into, t, which equals 0, if no word is found
	 */
	page_contents->words.move_right();
	t = page_contents->words.get_data();
	if (page_contents->words.is_equal_to_head()) {
	  t = 0;
	}
      }

      /*
       *  and terminate the next_words array
       */

      if (k<max_words) {
	next_words[k].left  = 0;
	next_words[k].right = 0;
      }

      /*
       *  consistency check, next_cols, after removing redundant colums.
       */

      remove_redundant_columns(next_cols);

#if 0
      for (k=0; k<count_columns(next_cols); k++) {
	if (next_cols[k].left > next_cols[k].right) {
	  fprintf(stderr, "left > right\n"); fflush(stderr);
	  stop();
	  fatal("next_cols has messed up columns");
	}
	if ((k>0) && (k+1<count_columns(next_cols)) && (next_cols[k].right > next_cols[k+1].left)) {
	  fprintf(stderr, "next_cols[k].right > next_cols[k+1].left\n"); fflush(stderr);
	  stop();
	  fatal("next_cols has messed up columns");
	}
      }
#endif
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

  while ((column_guess[i].right != 0) && (i<MAX_WORDS_PER_LINE)) {
    j=0;
    while ((words[j].right != 0) && (j<MAX_WORDS_PER_LINE)) {
      if ((words[j].left <= column_guess[i].right) && (i+1<MAX_WORDS_PER_LINE) &&
	  (column_guess[i+1].right != 0) && (words[j].right >= column_guess[i+1].left)) {
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

  for (i=0; (i<MAX_WORDS_PER_LINE) && (src[i].right != 0); i++) {
    include_into_list(dest, &src[i]);
  }
  remove_redundant_columns(dest);
}

/*
 *  remove_entry_in_line - removes an entry, j, in, line.
 */

void html_printer::remove_entry_in_line (struct text_defn *line, int j)
{
  while (line[j].right != 0) {
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

  while (line[i].right != 0) {
    if ((i<MAX_WORDS_PER_LINE) && (line[i+1].right != 0)) {
      j = 0;
      while ((j<MAX_WORDS_PER_LINE) && (line[j].right != 0)) {
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

  while ((i<MAX_WORDS_PER_LINE) && (line[i].right != 0) && (line[i].left<item->left)) {
    i++;
  }

  if (line[i].right == 0) {
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

      while ((i+1<MAX_WORDS_PER_LINE) && (line[i].right != 0)) {
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

  while ((i<max_words) && (line[i].right != 0)) {
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

  while ((i<max_words) && (line[i].right != 0)) {
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

  while ((i<MAX_WORDS_PER_LINE) && (line[i].right != 0)) {
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

  while ((i<max_words) && (line[i].right != 0)) {
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
  while ((line1[i].right != 0) && (line2[i].right != 0)) {
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
 *  check_lack_of_hits - returns TRUE if a column has been moved to a position
 *                       of only one hit from a position of more than one hit.
 */

int html_printer::check_lack_of_hits (struct text_defn *next_guess,
				      struct text_defn *last_guess,
				      text_glob *start, int limit)
{
  text_glob *current=page_contents->words.get_data();
  int              n=count_columns(last_guess);
  int              m=count_columns(next_guess);
  int           i, j;

  if (limit > 0) {
    rewind_text_to(start);
    count_hits(last_guess, n, limit);
    rewind_text_to(current);
    i=0;
    j=0;
    while ((i<n) && (j<m) &&
	   (last_guess[i].right != 0) && (next_guess[j].right != 0)) {
      if ((is_intersection(last_guess[i].left, last_guess[i].right,
			   next_guess[j].left, next_guess[j].right)) &&
	  (next_guess[j].left < last_guess[i].left) &&
	  (last_guess[i].is_used >= 2)) {
	/*
	 *  next_guess has to be = 1 as this position is new
	 */
	return( TRUE );
      }
      if (last_guess[i].left < next_guess[j].left) {
	i++;
      } else {
	j++;
      }
    }
  }
  return( FALSE );
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

  while ((last_guess[j].right != 0) && (next_line[k].right != 0)) {
    if (last_guess[j].left == next_line[k].left) {
      // same horizontal alignment found
      next_guess[i].left  = last_guess[j].left;
      next_guess[i].right = max(last_guess[j].right, next_line[k].right);
      i++;
      j++;
      k++;
      if ((next_guess[i-1].right > last_guess[j].left) && (last_guess[j].right != 0)) {
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
      if ((next_guess[i-1].right > last_guess[j].left) && (last_guess[j].right != 0)) {
	removed = TRUE;
      }
    }
  }
  while (next_line[k].right != 0) {
    next_guess[i].left  = next_line[k].left;
    next_guess[i].right = next_line[k].right;
    i++;
    k++;
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

  while (line[i].right != 0) {
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
 *  can_loose_column - checks to see whether we should combine two columns.
 *                     This is allowed if there are is only one hit on the
 *                     left hand edge and the previous column is very close.
 */

void html_printer::can_loose_column (text_glob *start, struct text_defn *last_guess, int limit)
{
  text_glob *current=page_contents->words.get_data();
  int              n=count_columns(last_guess);
  int              i;

  rewind_text_to(start);
  count_hits(last_guess, n, limit);
  i=0;
  while (i<n-1) {
    if ((last_guess[i+1].is_used == 1) &&
	(calculate_min_gap(start) > (last_guess[i+1].left-last_guess[i].right))) {
      last_guess[i].right = last_guess[i+1].right;
      remove_entry_in_line(last_guess, i+1);
      n = count_columns(last_guess);
      i = 0;
    } else {
      i++;
    }
  }
  rewind_text_to(current);
}

/*
 *  display_columns - a long overdue debugging function, as this column code is causing me grief :-(
 */

void html_printer::display_columns (const char *word, const char *name, text_defn *line)
{
  int i=0;

  fprintf(stderr, "[%s:%s]", name, word);
  while (line[i].right != 0) {
    fprintf(stderr, " <left=%d right=%d %d%%> ", line[i].left, line[i].right, line[i].percent);
    i++;
  }
  fprintf(stderr, "\n");
  fflush(stderr);
}

/*
 *  copy_line - dest = src
 */

void html_printer::copy_line (struct text_defn *dest, struct text_defn *src)
{ 
  int k;

  for (k=0; ((src[k].right != 0) && (k<MAX_WORDS_PER_LINE)); k++) {
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
  if ((line[0].left != get_left()) && (line[0].right != 0) &&
      (get_left() < line[0].left) && (is_worth_column(get_left(), line[0].left))) {
    t.left  = get_left();
    t.right = line[0].left;
    include_into_list(line, &t);
  }

  while ((i<MAX_WORDS_PER_LINE) && (line[i].right != 0)) {
    if ((i+1<MAX_WORDS_PER_LINE) && (line[i+1].right != 0) && (line[i].right != line[i+1].left) &&
	(is_worth_column(line[i].right, line[i+1].left))) {
      t.left  = line[i].right;
      t.right = line[i+1].left;
      include_into_list(line, &t);
      i=0;
    } else {
      i++;
    }
  }
  // now let us see whether we need a final column on the right hand side
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
		       (all_words[0].left  == left_margin_indent) &&
		       (all_words[0].right == right_margin_indent))) {
    // no point as we have now seen a full line of contineous text with no gap
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
  while ((last_col[i].right != 0) && (last_col[i+1].right != 0)) {
    if (abs(last_col[i+1].left-last_col[i].right) >= gap) {
      found = TRUE;
      i++;
    } else {
      // not good enough for a single line, remove it
      last_col[i].right = last_col[i+1].right;
      remove_entry_in_line(last_col, i+1);
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
  while ((i<MAX_WORDS_PER_LINE) && (a[i].right != 0)) {
    j=0;
    while ((j<MAX_WORDS_PER_LINE) && (b[j].right != 0) &&
	   ((b[j].left != a[i].left) || (b[j].right != a[i].right))) {
      j++;
    }
    if ((j==MAX_WORDS_PER_LINE) || (b[j].right == 0)) {
      // found a different column - not a subset
      return( FALSE );
    }
    i++;
  }
  return( TRUE );
}

/*
 *  count_hits - counts the number of hits per column. A left hit
 *               is when the left hand position of a glob hits
 *               the left hand column.
 */

void html_printer::count_hits (text_defn *col, int no_of_columns, int limit)
{
  int        i;
  text_glob *start = page_contents->words.get_data();
  text_glob *g     = start;

  // firstly reset the used field
  for (i=0; i<no_of_columns; i++) {
    col[i].is_used   = 0;
  }
  // now calculate the left hand hits
  while ((g != 0) && (g->minv <= limit)) {
    i=0;
    while ((i<no_of_columns) && (col[i].right < g->minh)) {
      i++;
    }
    if ((col[i].left == g->minh) && (col[i].right != 0)) {
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
}

/*
 *  count_right_hits - counts the number of right hits per column.
 *                     A right hit is when the left hand position
 *                     of a glob hits the right hand column.
 */

void html_printer::count_right_hits (text_defn *col, int no_of_columns)
{
  int        i;
  text_glob *start = page_contents->words.get_data();
  text_glob *g     = start;

  // firstly reset the used field
  for (i=0; i<no_of_columns; i++) {
    col[i].right_hits = 0;
  }
  // now calculate the left hand hits
  while ((g != 0) && (g->minv <= indentation.vertical_limit)) {
    i=0;
    while ((i<no_of_columns) && (col[i].right < g->minh)) {
      i++;
    }
    if ((i<no_of_columns) && (col[i].right == g->maxh)) {
      if (debug_table_on) {
	fprintf(stderr, "found right hit [%s] at %d in %d\n",
		g->text_string, g->maxh, i);
	fflush(stderr);
      }
      col[i].right_hits++;
    }
    page_contents->words.move_right();
    if (page_contents->words.is_equal_to_head()) {
      g = 0;
      page_contents->words.start_from_tail();
    } else {
      g=page_contents->words.get_data();
    }
  }
}

/*
 *  right_indentation - returns TRUE if a single column has been found and
 *                      it resembles an indentation. Ie .RS/.RE or ABSTACT
 */

int html_printer::right_indentation (struct text_defn *last_guess)
{
  // it assumes that last_guess contains a single column
  return( (last_guess[0].left > left_margin_indent) );
}

/*
 *  able_to_steal_width - returns TRUE if we have an unused column which we can steal from.
 *                        It must have more than MIN_TEXT_PERCENT to do this.
 */

int html_printer::able_to_steal_width (void)
{
  int i;

  for (i=0; i<indentation.no_of_columns; i++) {
    if ((! indentation.columns[i].is_used) &&
	(indentation.columns[i].percent > MIN_TEXT_PERCENT)) {
      return( TRUE );
    }
  }
  return( FALSE );
}

/*
 *  is_divisible_by - returns TRUE if n is divisible by d leaving no remainder.
 */

static int is_divisible_by (int n, int d)
{
  return( (n % d) == 0 );
}

/*
 *  need_to_steal_width - returns TRUE if a used column need to be
 *                        given a little extra width for safty sake.
 */

int html_printer::need_to_steal_width (void)
{
  int i;

  for (i=0; i<indentation.no_of_columns; i++) {
    if ((indentation.columns[i].is_used) &&
	(indentation.columns[i].percent == (((indentation.columns[i].right - indentation.columns[i].left) * 100) /
					    (right_margin_indent-left_margin_indent))) &&
	(indentation.columns[i].percent < PERCENT_THRESHOLD)) {
      return( TRUE );
    }
  }
  return( FALSE );
}

/*
 *  utilize_round_off - utilize the remaining percent width in text columns
 */

void html_printer::utilize_round_off (void)
{
  int total = total_percentages();
  int excess, i;

  // use up the spare excess

  excess = 100-total;

  for (i=0; (i<indentation.no_of_columns) && (excess>0); i++) {
    if ((indentation.columns[i].is_used) &&
	(indentation.columns[i].percent < PERCENT_THRESHOLD)) {
      indentation.columns[i].percent++;
      excess--;
    }
  }
  // we might as well try and keep any numbers simple if possible
  for (i=0; (i<indentation.no_of_columns) && (excess>0); i++) {
    if ((indentation.columns[i].is_used) &&
	(! is_divisible_by(indentation.columns[i].percent, MIN_TEXT_PERCENT))) {
      indentation.columns[i].percent++;
      excess--;
    }
  }
  // forget the niceties lets just use excess up now!
  for (i=0; (i<indentation.no_of_columns) && (excess>0); i++) {
    if (indentation.columns[i].is_used) {
      indentation.columns[i].percent++;
      excess--;
    }
  }
}

/*
 *  can_distribute_fairly - returns TRUE if we can redistribute some of the unused width into
 *                          columns that are used.
 */

int html_printer::can_distribute_fairly (void)
{
  int i;
  int total=0;
  int used =0;
  int excess;

  // firstly total up all percentages - so we can use round offs
  for (i=0; i<indentation.no_of_columns; i++) {
    total += indentation.columns[i].percent;
    if ((indentation.columns[i].is_used) &&
	(indentation.columns[i].percent < PERCENT_THRESHOLD)) {
      used++;
    }
  }
  // 
  excess = 100-total;
  if (excess < used) {
    for (i=0; i<indentation.no_of_columns; i++) {
      if (! indentation.columns[i].is_used) {
	if (indentation.columns[i].percent > MIN_TEXT_PERCENT) {
	  indentation.columns[i].percent--;
	  excess++;
	}
      }
    }
  }
  if (excess >= used) {
    for (i=0; i<indentation.no_of_columns; i++) {
      if ((indentation.columns[i].is_used) &&
	  (indentation.columns[i].percent < PERCENT_THRESHOLD) &&
	  (indentation.columns[i].percent == (((indentation.columns[i].right - indentation.columns[i].left) * 100) /
					      (right_margin_indent-left_margin_indent)))) {
	indentation.columns[i].percent++;
	excess--;
      }
    }
    return( TRUE );
  }
  return( FALSE );
}

/*
 *  remove_table_column - removes column, i, from the indentation.
 */

void html_printer::remove_table_column (int i)
{
  while (i<indentation.no_of_columns) {
    indentation.columns[i].left    = indentation.columns[i+1].left;
    indentation.columns[i].right   = indentation.columns[i+1].right;
    indentation.columns[i].is_used = indentation.columns[i+1].is_used;
    indentation.columns[i].percent = indentation.columns[i+1].percent;
    i++;
  }
  indentation.no_of_columns--;
}

/*
 *  next_line_on_left_column - returns TRUE if the next line in
 *                             column, i, has a word on the left margin.
 */

int html_printer::next_line_on_left_column (int i, text_glob *start)
{
  int current_vpos=start->minv;

  while ((start != 0) && (start->minv < indentation.vertical_limit) &&
	 (is_on_same_line(start, current_vpos))) {
    if (page_contents->words.is_equal_to_tail()) {
      start = 0;
    } else {
      page_contents->words.move_right();
      start = page_contents->words.get_data();
    }
  }
  if ((start != 0) && (start->minv < indentation.vertical_limit)) {
    // onto next line now
    current_vpos=start->minv;
    while ((start != 0) && (start->minv < indentation.vertical_limit) &&
	   (is_on_same_line(start, current_vpos))) {
      if (start->minh == indentation.columns[i].left) {
	return( TRUE );
      }
      if (page_contents->words.is_equal_to_tail()) {
	start = 0;
      } else {
	page_contents->words.move_right();
	start = page_contents->words.get_data();
      }
    }
  }
  return( FALSE );
}

/*
 *  will_wrap_text - returns TRUE if text is wrapped in column, i.
 */

int html_printer::will_wrap_text (int i, text_glob *start)
{
  text_glob *current=page_contents->words.get_data();

  if (auto_on) {
    rewind_text_to(start);
    while ((start != 0) && (start->minv < indentation.vertical_limit)) {
      if (indentation.columns[i].right == start->maxh) {
	// ok right word is on column boarder - check next line
	if (next_line_on_left_column(i, start)) {
	  rewind_text_to(current);
	  return( TRUE );
	}
      }
      if (page_contents->words.is_equal_to_tail()) {
	start = 0;
      } else {
	page_contents->words.move_right();
	start = page_contents->words.get_data();
      }
    }
  }
  rewind_text_to(current);
  return( FALSE );
}

/*
 *  remove_unnecessary_unused - runs through a table and decides whether an unused
 *                              column can be removed. This is only true if the
 *                              column to the left does not wrap text.
 */

void html_printer::remove_unnecessary_unused (text_glob *start)
{
  int i=0;
  int left=get_left();
  int right;

  while (i<indentation.no_of_columns) {
    if ((indentation.columns[i].is_used) &&
	(i+1<indentation.no_of_columns) && (! indentation.columns[i+1].is_used)) {
      /*
       *  so i+1 is unused and there is a used column to the left.
       *  Now we check whether we can add the unused column to the column, i.
       *  This can only be done if column, i, is not wrapping text.
       */
      if (! will_wrap_text(i, start)) {
#if 1
	if (i+1 < indentation.no_of_columns) {
	  right = indentation.columns[i+1].right;
	} else {
	  right = right_margin_indent;
	}
	indentation.columns[i].percent = (((right - indentation.columns[i].left) * 100) /
					  (right_margin_indent-left));
#else
	indentation.columns[i].percent = (((indentation.columns[i+1].right - indentation.columns[i].left) * 100) /
					  (right_margin_indent-left));
#endif
	remove_table_column(i+1);
	i=-1;
      }
    }
    i++;
  }
}

/*
 *  remove_zero_percentage_column - removes all zero percentage width columns
 */

void html_printer::remove_zero_percentage_column (void)
{
  int i=0;

  while (i<indentation.no_of_columns) {
    if (indentation.columns[i].percent == 0) {
      remove_table_column(i);
      i=0;
    } else {
      i++;
    }
  }
}

/*
 *  get_left - returns the actual left most margin.
 */

int html_printer::get_left (void)
{
  if ((header_indent < left_margin_indent) && (header_indent != -1)) {
    return( header_indent );
  } else {
    if (margin_on) {
      return( 0 );
    } else {
      return( left_margin_indent );
    }
  }
}

/*
 *  calculate_percentage_width - calculates the percentage widths,
 *                               this function will be generous to
 *                               columns which have words as some browsers
 *                               produce messy output if the percentage is exactly
 *                               that required for text..
 *                               We try and round up to MIN_TEXT_PERCENT
 *                               of course we can only do this if we can steal from
 *                               an unused column.
 */

void html_printer::calculate_percentage_width (text_glob *start)
{
  int i;
  int left=get_left();
  int right;

  // firstly calculate raw percentages
  for (i=0; i<indentation.no_of_columns; i++) {
#if 0
    indentation.columns[i].percent = (((indentation.columns[i].right - indentation.columns[i].left) * 100) /
				      (right_margin_indent-left));
#else
    if (i+1 < indentation.no_of_columns) {
      right = indentation.columns[i+1].left;
    } else {
      right = right_margin_indent;
    }
    indentation.columns[i].percent = (((right - indentation.columns[i].left) * 100) /
				      (right_margin_indent-left));
#endif
  }
  if (debug_table_on) {
    display_columns(start->text_string, "[b4 steal] indentation.columns", indentation.columns);
  }

  // now steal from the unused columns..
  remove_unnecessary_unused(start);

  if (debug_table_on) {
    display_columns(start->text_string, "[after steal] indentation.columns", indentation.columns);
  }

#if 0
  utilize_round_off();
#endif
  remove_zero_percentage_column();
}


/*
 *  is_column_subset - returns TRUE if the columns described by small can be contained in
 *                     the columns in large.
 */

int html_printer::is_column_subset (struct text_defn *small, struct text_defn *large)
{
  int ns=count_columns(small);
  int nl=count_columns(large);
  int found;
  int i=0;
  int j;

  while (i<ns) {
    j=0;
    found = FALSE;
    while (j<nl) {
      if (is_intersection(small[i].left, small[i].right, large[j].left, large[j].right)) {
	found = TRUE;
	if (! is_subsection(small[i].left, small[i].right, large[j].left, large[j].right)) {
	  // found column which is not a subset
	  return( FALSE );
	}
      }
      j++;
    }
    if (! found) {
      return( FALSE );
    }
    i++;
  }
  // small cannot be an empty set
  return( ns>0 );
}

/*
 *  right_most_column - returns the right most column position.
 */

int html_printer::right_most_column (struct text_defn *col)
{
  int i = count_columns(col);

  if (i>0) {
    return( col[i-1].right );
  } else {
    return( 0 );
  }
}

/*
 *  large_enough_gap_for_two - returns TRUE if there exists a large enough gap
 *                             for two lines.
 */

int html_printer::large_enough_gap_for_two (struct text_defn *col)
{
  int i=0;
  int found=FALSE;
  int gap=MIN_COLUMN_FOR_TWO_LINES;

  if (abs(col[i].left - left_margin_indent) >= gap) {
    found = TRUE;
  }
  while ((col[i].right != 0) && (col[i+1].right != 0)) {
    if (abs(col[i+1].left-col[i].right) >= gap) {
      found = TRUE;
      i++;
    } else {
      // not good enough for this table, remove it
      col[i].right = col[i+1].right;
      remove_entry_in_line(col, i+1);
    }
  }
  return( found );
}

/*
 *  is_small_table - applies some rigorous rules to test whether we should start this
 *                   table at this point.
 */

int html_printer::is_small_table (int lines, struct text_defn *last_guess,
				  struct text_defn *words_1, struct text_defn *cols_1,
				  struct text_defn *words_2, struct text_defn *cols_2,
				  int *limit, int *limit_1)
{
  /*
   *  firstly we check for an indented paragraph
   */

  if ((lines >= 2) &&
      (count_columns(cols_1) == count_columns(cols_2)) && (count_columns(cols_1) == 1) &&
      right_indentation(cols_1) && (! right_indentation(cols_2)) &&
      (cols_1[0].right == right_margin_indent)) {
    return( FALSE );
  }

  if (lines == 2) {
    /*
     *  as we only have two lines in our table we need to examine in detail whether
     *  we should construct a table from these two lines.
     *  For example if the text is the start of an indented paragraph and
     *  line1 and line2 are contineous then they should form one row in our table but
     *  if line1 and line2 are not contineous it is safer to treat them separately.
     *
     *  We are prepared to reduce the table to one line
     */
    if (((count_columns(cols_1) != count_columns(cols_2)) && (cols_1[0].left > cols_2[0].left)) ||
	(! ((is_column_subset(cols_1, cols_2)) ||
	    (is_column_subset(cols_2, cols_1))))) {
      /*
       *  now we must check to see whether line1 and line2 join
       */
      if ((right_most_column(cols_1) == right_margin_indent) &&
	  (cols_2[0].left == left_margin_indent)) {
	/*
	 *  looks like they join, we don't want a table at all.
	 */
	return( FALSE );
      }
      /*
       *  use single line table
       */
      lines--;
      *limit = *limit_1;
      copy_line(last_guess, cols_1);
    }
  }

  if ((count_columns(last_guess)==1) && (right_indentation(last_guess))) {
    if (lines == 1) {
      *limit = *limit_1;
    }
    return( TRUE );
  }

  /*
   *  check for large gap with single line or if multiple lines with more than one column
   */

  if (lines == 1) {
    if (large_enough_gap(last_guess)) {
      *limit = *limit_1;
      return( TRUE );
    }
  } else if (count_columns(last_guess)>1) {
    if (lines == 2) {
      return( large_enough_gap_for_two(last_guess) );
    }
    return( TRUE );
  }
  return( FALSE );
}


/*
 *  is_appropriate_to_start_table - returns TRUE if it is appropriate to start the table
 *                                  at this point.
 */

int html_printer::is_appropriate_to_start_table (struct text_defn *cols_1,
						 struct text_defn *cols_2,
						 struct text_defn *last_guess)
{
  if (count_columns(last_guess) == 1) {
    if (debug_table_on) {
      display_columns("", "[is] cols_1"    , cols_1);
      display_columns("", "[is] cols_2"    , cols_2);
      display_columns("", "[is] last_guess", last_guess);
    }

    if (! ((is_column_subset(cols_1, cols_2)) ||
	   (is_column_subset(cols_2, cols_1)))) {
      return( FALSE );
    }
    if ((count_columns(cols_1) == 1) &&
	(cols_1[0].left > left_margin_indent) && (cols_1[0].right < right_margin_indent) &&
	(cols_1[0].right != cols_2[0].right) &&
	(count_columns(last_guess) == 1)) {
      return( FALSE );
    }
  }
  return( TRUE );
}

/*
 *  is_a_full_width_column - returns TRUE if there exists a full width column.
 */

int html_printer::is_a_full_width_column (void)
{
  int i=0;

  while (i<indentation.no_of_columns) {
    if (((indentation.columns[i].left == get_left()) ||
	 (indentation.columns[i].left == left_margin_indent)) &&
	(indentation.columns[i].right == right_margin_indent)) {
      return( TRUE );
    }
    i++;
  }
  return( FALSE );
}

/*
 *  should_defer_table - returns TRUE if we should defer this table.
 *                       This can occur if the first line seen indent
 *                       is < than future lines. In which case it
 *                       will cause future lines in this table
 *                       to be indented. The lesser of the evils
 *                       is to treat the first line by itself.
 */

int html_printer::should_defer_table (int lines, struct text_glob *start, struct text_defn *cols_1)
{
  if (lines > 2) {
    int i=0;
    int c=count_columns(cols_1);

    count_hits(cols_1, count_columns(cols_1), indentation.vertical_limit);
    rewind_text_to(start);
    count_right_hits(cols_1, count_columns(cols_1));
    rewind_text_to(start);
    while (i<c) {
      if ((cols_1[i].is_used > 1) || (cols_1[i].right_hits > 1)) {
	return( FALSE );
      }
      i++;
    }
    /*
     *  first line (cols_1) is not aligned on any future column, we defer.
     */
    return( TRUE );
  }
  return( FALSE );
}

/*
 *  is_new_exact_right - returns TRUE if the, next_cols, has a word sitting
 *                       on the right hand margin of last_guess. But only
 *                       if no exact right word was found in last_cols.
 */

int html_printer::is_new_exact_right (struct text_defn *last_guess,
				      struct text_defn *last_cols,
				      struct text_defn *next_cols)
{
  int n=count_columns(last_guess)-1;
  return( FALSE );

  if ((n>=0) && (last_guess[n].right != 0) && (last_cols[n].right != 0) && (next_cols[n].right != 0)) {
    if ((last_cols[n].right != last_guess[n].right) &&
	((next_cols[n].right == last_guess[n].right) || (next_cols[n].right == right_margin_indent))) {
      return( TRUE );
    }
  }
  return( FALSE );
}

/*
 *  found_use_for_table - checks whether the some words on one line directly match
 *                        the horizontal alignment of the line below.
 *                        This is rather complex as we need to detect text tables
 *                        such as .2C .IP Abstracts and indentations
 *
 *                        Algorithm is:
 *
 *                        read first line of text and calculate the significant
 *                              gaps between words
 *                        next next line of text and do the same
 *                        if a conflict between these lines exists and
 *                           first line is centered
 *                        then
 *                           return centered line
 *                        elsif start of a table is found
 *                        then
 *                           repeat
 *                              read next line of text and calculate significant gaps
 *                           until conflict between the gaps is found
 *                           record table
 *                           return table found
 *                        else
 *                           return no table found
 *                        fi
 */

int html_printer::found_use_for_table (text_glob *start)
{
  text_glob         *t;
  struct text_defn   all_words  [MAX_WORDS_PER_LINE];    // logical OR of words on each line
  struct text_defn   words_1    [MAX_WORDS_PER_LINE];    // actual words found on first line
  struct text_defn   words_2    [MAX_WORDS_PER_LINE];    // actual words found on second line
  struct text_defn   cols_1     [MAX_WORDS_PER_LINE];    // columns found on line 1
  struct text_defn   cols_2     [MAX_WORDS_PER_LINE];    // columns found on line 2
  struct text_defn   last_words [MAX_WORDS_PER_LINE];    // actual words found on last line
  struct text_defn   last_cols  [MAX_WORDS_PER_LINE];    // columns found so far
  struct text_defn   next_words [MAX_WORDS_PER_LINE];    // actual words found on last line (new)
  struct text_defn   next_cols  [MAX_WORDS_PER_LINE];    // columns found on next line
  struct text_defn   last_guess [MAX_WORDS_PER_LINE];    // columns found on last line
  //                                                       (logical AND of gaps (treat gaps = true))
  struct text_defn   next_guess [MAX_WORDS_PER_LINE];    // columns found on next line
  //                                                       (logical AND of gaps (treat gaps = true))
  struct text_defn   prev_guess [MAX_WORDS_PER_LINE];    // temporary copy of last_guess
  int                i     =0;
  int                lines =1;                           // number of lines read
  int                limit;                              // vertical limit reached in our table
  int                limit_1;                            // vertical position after line 1

#if 0
  if (strcmp(start->text_string, "<hr>") == 0) {
    stop();
  }
#endif

  /*
   *  get first set of potential columns into last_line, call this last_guess
   */
  limit   = collect_columns(words_1, cols_1, 0, 0, MAX_WORDS_PER_LINE);
  limit_1 = limit;
  copy_line(last_guess, cols_1);

  /*
   *  initialize the all_words columns - if this should ever equal a complete line
   *  with no gaps then we terminate the table.
   */
  copy_line(all_words, cols_1);

  /*
   *  and set the current limit found
   */
  indentation.vertical_limit = limit;

  /*
   *  have we reached the end of page?
   */
  if (page_contents->words.is_equal_to_head() || (limit == 0)) {
    cols_2[0].left  = 0;
    cols_2[0].right = 0;
  } else {
    /*
     *  the answer to the previous question was no.
     *  So we need to examine the next line
     */
    limit = collect_columns(words_2, cols_2, words_1, cols_1, MAX_WORDS_PER_LINE);
    if (limit >= 0) {
      lines++;
    }
  }

  /*
   *  now check to see whether the first line looks like a single centered line
   */
  if (single_centered_line(cols_1, cols_2, start)) {
    rewind_text_to(start);
    write_centered_line(start);
    /*
     *  indicate to caller than we have centered text, not found a table.
     */
    indentation.no_of_columns = 0;
    return( TRUE );
  } else if (! table_on) {
    /*
     *  user does not allow us to find a table (we are allowed to find centered lines (above))
     */
    rewind_text_to(start);
    return( FALSE );
  }

  /*
   *  remove any gaps from all_words
   */
  combine_line(all_words, cols_2);
  if (debug_table_on) {
    display_columns(start->text_string, "[1] all_words" , all_words);
    display_columns(start->text_string, "[1] cols_1"    , cols_1);
    display_columns(start->text_string, "[1] words_1"   , words_1);
    display_columns(start->text_string, "[1] cols_2"    , cols_2);
    display_columns(start->text_string, "[1] words_2"   , words_2);
    display_columns(start->text_string, "[1] last_guess", last_guess);
  }

  /*
   *  next_guess = last_guess AND next_cols   (where gap = true)
   */

  if (remove_white_using_words(prev_guess, last_guess, cols_2)) {
  }
  if (remove_white_using_words(next_guess, prev_guess, all_words)) {
  }

  if (debug_table_on) {
    display_columns(start->text_string, "[2] next_guess", next_guess);
  }

  copy_line(prev_guess,    cols_1);
  combine_line(prev_guess, cols_2);

  /*
   *  if no sequence of words crosses a column and
   *     both the last column and all_words are not a full solid line of text
   */
  if ((! conflict_with_words(next_guess, all_words)) &&
      (continue_searching_column(next_guess, next_guess, all_words)) &&
      (is_appropriate_to_start_table(cols_1, cols_2, prev_guess)) &&
      (! page_contents->words.is_equal_to_head()) &&
      ((end_region_vpos < 0) || (limit < end_region_vpos)) &&
      (limit > 0)) {

    /*
     *  subtract any columns which are bridged by a sequence of words
     */

    copy_line(next_cols , cols_2);
    copy_line(next_words, words_2);

    do {
      copy_line(prev_guess, next_guess);       // copy next_guess away so we can compare it later
      combine_line(last_guess, next_guess);

      if (debug_table_on) {
	t = page_contents->words.get_data();
	display_columns(t->text_string, "[l] last_guess", last_guess);
      }
      indentation.vertical_limit = limit;

      copy_line(last_cols,  next_cols);
      copy_line(last_words, next_words);
      if (page_contents->words.is_equal_to_head()) {
	/*
	 *  terminate the search
	 */
	next_cols[0].left  = 0;
	next_cols[0].right = 0;
      } else {
	limit = collect_columns(next_words, next_cols, last_words, last_cols, MAX_WORDS_PER_LINE);
	lines++;
      }
      
      combine_line(all_words, next_cols);
      if (debug_table_on) {
	display_columns(t->text_string, "[l] all_words" , all_words);
	display_columns(t->text_string, "[l] last_cols" , last_cols);
	display_columns(t->text_string, "[l] next_words", next_words);
	display_columns(t->text_string, "[l] next_cols" , next_cols);
      }

      if (limit >= 0) {
	/*
	 *  (if limit is < 0 then the table ends anyway.)
	 *  we check to see whether we should combine close columns.
	 */
	can_loose_column(start, last_guess, limit);
      }
      t = page_contents->words.get_data();
#if 0
      if (strcmp(t->text_string, "heT") == 0) {
	stop();
      }
#endif
      
    } while ((! remove_white_using_words(next_guess, last_guess, next_cols)) &&
	     (! conflict_with_words(next_guess, all_words)) &&
	     (continue_searching_column(next_guess, last_guess, all_words)) &&
	     ((is_continueous_column(prev_guess, last_cols)) || (is_exact_left(last_guess, next_cols))) &&
	     (! is_new_exact_right(last_guess, last_cols, next_cols)) &&
	     (! page_contents->words.is_equal_to_head()) &&
	     (! check_lack_of_hits(next_guess, last_guess, start, limit)) &&
	     ((end_region_vpos <= 0) || (t->minv < end_region_vpos)) &&
	     (limit >= 0));
    lines--;
  }

  if (limit < 0) {
    indentation.vertical_limit = limit;
  }

  if (page_contents->words.is_equal_to_head()) {
    // end of page check whether we should include everything
    if ((! conflict_with_words(next_guess, all_words)) &&
	(continue_searching_column(next_guess, last_guess, all_words)) &&
	((is_continueous_column(prev_guess, last_cols)) || (is_exact_left(last_guess, next_cols)))) {
      // end of page reached - therefore include everything
      page_contents->words.start_from_tail();
      t = page_contents->words.get_data();
      combine_line(last_guess, next_guess);
      indentation.vertical_limit = t->minv;
    }
  } else {
    t = page_contents->words.get_data();
    if (((! conflict_with_words(last_guess, all_words))) &&
	(t->minv > end_region_vpos) && (end_region_vpos > 0)) {
      indentation.vertical_limit = limit;
    }
    if ((end_region_vpos > 0) && (t->minv > end_region_vpos)) {
      indentation.vertical_limit = min(indentation.vertical_limit, end_region_vpos+1);
    } else if (indentation.vertical_limit < 0) {
      // -1 as we don't want to include section heading itself
      indentation.vertical_limit = -indentation.vertical_limit-1;
    }
  }

  if (debug_table_on) {
    display_columns(start->text_string, "[1] all_words" , all_words);
    display_columns(start->text_string, "[1] cols_1"    , cols_1);
    display_columns(start->text_string, "[1] words_1"   , words_1);
    display_columns(start->text_string, "[1] cols_2"    , cols_2);
    display_columns(start->text_string, "[1] words_2"   , words_2);
    display_columns(start->text_string, "[1] last_guess", last_guess);
    display_columns(start->text_string, "[1] next_guess", next_guess);
  }
  rewind_text_to(start);

  i = count_columns(last_guess);
  if ((i>1) || (right_indentation(last_guess))) {

    // was (continue_searching_column(last_guess, last_guess, all_words)))) {
    if (should_defer_table(lines, start, cols_1)) {
      /*
       *  yes, but let us check for a single line table
       */
      lines = 1;
      copy_line(last_guess, cols_1);
    }

    if (is_small_table(lines, last_guess, words_1, cols_1, words_2, cols_2,
		       &indentation.vertical_limit, &limit_1)) {

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

      /*
       * +1 for the potential header_margin
       * +1 for null
       */

      indentation.no_of_columns = count_columns(last_guess);
      indentation.columns       = (struct text_defn *)malloc((indentation.no_of_columns+2)*sizeof(struct text_defn));

      i=0;
      while (i<=indentation.no_of_columns) {
	indentation.columns[i].left  = last_guess[i].left;
	indentation.columns[i].right = last_guess[i].right;
	i++;
      }

      if (indentation.no_of_columns>0) {
	assign_used_columns(start);
	rewind_text_to(start);
	calculate_percentage_width(start);

	if (debug_table_on) {
	  display_columns(start->text_string, "[g] indentation.columns", indentation.columns);
	}

	/*
	 * clearly a single column 100% is not worth using a table.
	 * Also we check to see whether the first line is sensibly
	 * part of this table.
	 */
	if (is_a_full_width_column()) {
	  indentation.no_of_columns = 0;
	  free( indentation.columns );
	  indentation.columns = 0;
	} else {
	  return( TRUE );
	}
      }
    }
  }
  return( FALSE );
}

/*
 *  define_cell - creates a table cell using the percentage width.
 */

void html_printer::define_cell (int i)
{
  html.put_string("<td valign=\"top\" align=\"left\" width=\"");
  html.put_number(indentation.columns[i].percent);
  html.put_string("%\">\n");
}

/*
 *  column_display_word - given a left, right pair and the indentation.vertical_limit
 *                        write out html text within this region.
 */

void html_printer::column_display_word (int cell, int vert, int left, int right, int next)
{
  text_glob *g=page_contents->words.get_data();

  supress_sub_sup = TRUE;
  if (left != next) {
    define_cell(cell);
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
	    translate_to_html(g);
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
 *  total_percentages - returns the total of all the percentages in the table.
 */

int html_printer::total_percentages ()
{
  int i;
  int sum=0;

  for (i=0; i<indentation.no_of_columns; i++) {
    sum += indentation.columns[i].percent;
  }
  return( sum );
}

/*
 *  start_table - creates a table according with parameters contained within class html_table.
 */

void html_printer::start_table (void)
{
  save_paragraph();
  html.put_string("\n<table width=\"");
  html.put_number(total_percentages());
  html.put_string("%\" rules=\"none\" frame=\"none\" cols=\"");
  html.put_number(indentation.no_of_columns);
  html.put_string("\" cellspacing=\"0\" cellpadding=\"0\">\n");
}

/*
 *  end_table - finishes off a table.
 */

void html_printer::end_table (void)
{
  html.put_string("</table>\n");
  indentation.no_of_columns = 0;
  restore_paragraph();
  supress_sub_sup = TRUE;
}


/*
 *  is_in_table - returns TRUE if we are inside an html table.
 */

int html_printer::is_in_table (void)
{
  return( indentation.no_of_columns != 0 );
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
	    // stop();
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
      return( rightmost );
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

int html_printer::find_column_index_in_line (text_glob *t, text_defn *line)
{
  int i=0;

  while ((line != 0) && ((line[i].right != 0) || (line[i].right != 0)) &&
	 (! ((line[i].left<=t->minh) && (line[i].right>t->minh)))) {
    i++;
  }
  return( i );
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
  int        vpos, last, prev;
  text_glob *is_gap[MAX_WORDS_PER_LINE];
  text_glob  zero(&start->text_style, 0, 0, 0, 0, 0, 0, 0, 0);

#if 1
  if ((v == -1) && (strcmp(start->text_string, "CASE") == 0)) {
    stop();
  }
#endif

  if (v >= indentation.vertical_limit) {
    return( v+1 );
  } else {
    /*
     *  initially we start with all gaps in our table
     *  after a gap we start a new row
     *  here we set the gap array to the previous line
     */

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
    if (page_contents->words.is_equal_to_head()) {
      t = &zero;
    } else {
      page_contents->words.move_left();
      t = page_contents->words.get_data();
    }

    prev = t->minv;
    for (i=0; i<indentation.no_of_columns; i++) {
      is_gap[i] = t;
    }

    if (page_contents->words.is_equal_to_tail()) {
      rewind_text_to(start);
      return( indentation.vertical_limit );
    } else {
      page_contents->words.move_right();
    }
    t = page_contents->words.get_data();
    vpos = t->minv;

    // now check each row for a gap
    do {
      last = vpos;
      vpos = t->minv;
      if (vpos > indentation.vertical_limit) {
	// we have reached the end of the table, quit
	rewind_text_to(start);
	return( indentation.vertical_limit );
      }
      
      i = find_column_index(t);
      if (i>=indentation.no_of_columns) {
	error("find_column_index has failed");
	stop();
      } else {
	if (! is_on_same_line(t, last)) {
	  prev = last;
	}

	if ((! is_on_same_line(is_gap[i], vpos)) && (! is_on_same_line(is_gap[i], prev)) &&
	    (indentation.columns[i].is_used)) {
	  // no word on previous line - must be a gap - force alignment of row
	  rewind_text_to(start);
	  return( prev );
	}
	is_gap[i] = t;
      }
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
      if (indentation.columns[i].right != 0) {
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
 *  adjust_margin_percentages - so far we have ignored the header_indent
 *                              and just considered left_margin_indent..right_margin_indent.
 *                              (We do this since we can assume 100% is total width for main text).
 *                              However as header_indent can be < left_margin_indent we need to
 *                              recalculate the real percentages in the light of the extended width.
 */

void html_printer::adjust_margin_percentages (void)
{
  if ((header_indent < left_margin_indent) && (header_indent != -1)) {
    /*
     *  recalculation necessary
     */
    int i=0;

    while (i<indentation.no_of_columns) {
      indentation.columns[i].percent = (indentation.columns[i].percent *
					(right_margin_indent - left_margin_indent)) /
	                                (right_margin_indent - header_indent);
      i++;
    }
    // remove_zero_percentage_column();
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

    start_table();
    rewind_text_to(start);
    count_right_hits(indentation.columns, indentation.no_of_columns);
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
	    // stop();
	  }
	}

	if (left<indentation.columns[i].left) {
	  if (debug_on) {
	    fprintf(stderr, "assert calculated left column edge is less than column\n"); fflush(stderr);
	    // stop();
	  }
	}

	if ((indentation.columns[i].right_hits == 1) &&
	    (indentation.columns[i].right != right_margin_indent)) {
	  indentation.wrap_margin = FALSE;
	  if (debug_on) {
	    fprintf(stderr, "turning auto wrap off during column %d for start word %s\n",
		    i, start->text_string);
	    fflush(stderr);
	    // stop();
	  }
	} else {
	  indentation.wrap_margin = TRUE;
	}

	column_display_word(i, limit, left, right, indentation.columns[i].right);
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
    } while (((limit < indentation.vertical_limit) && (start != 0) &&
	     (! page_contents->words.is_empty())) || (limit == -1));
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
  supress_sub_sup       = TRUE;
  output_vpos           = g->minv;
  output_hpos           = g->minh;
  do {
    char *postword=html_position_text(g, left_margin_indent, right_margin_indent);

    if (! header.written_header) {
      if (g->is_raw_command) {
	html.put_string((char *)g->text_string);
      } else {
	translate_to_html(g);
      }
      if (postword != 0) {
	html.put_string(postword);
      }
      need_one_newline = TRUE;
      issued_newline   = FALSE;
    }    
    page_contents->words.move_right();
    g = page_contents->words.get_data();
  } while ((! page_contents->words.is_equal_to_head()) && (is_on_same_line(g, current_vpos)));
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
    struct text_defn   last_words[MAX_WORDS_PER_LINE];

    collect_columns(last_words, last_guess, 0, 0, MAX_WORDS_PER_LINE);

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
  int  r        = font::res;
  int  height   = (g->text_style.point_size+2)*r/72;   // --fixme-- we always assume VS is PS+2 (could do better)
  int  temp_vpos;

  if (auto_on) {
    if ((more_than_line_break(output_vpos, g->minv, height)) || (p != current_paragraph->para_type)) {
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
  if (! current_paragraph->need_paragraph) {
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

  begin_paragraph(left_alignment);

  if ((! header.written_header) &&
      (is_on_same_line(g, output_vpos) ||
       pretend_is_on_same_line(g, left_margin, right_margin))) {

    /*
     *  check whether we should supress superscripts and subscripts.
     *  I guess we might be able to do better by examining text on this line
     *  --fixme--
     */

    if ((! is_on_same_line(g, output_vpos)) && (pretend_is_on_same_line(g, left_margin, right_margin))) {
      supress_sub_sup = TRUE;
    }
    header.written_header = FALSE;
    force_begin_paragraph();

    // check whether we need to insert white space between words on 'same' line
    if (pretend_is_on_same_line(g, left_margin, right_margin)) {
      emit_space(g, TRUE);
    }

    // check whether the font was reset after generating an image
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
    supress_sub_sup = FALSE;
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
      if ((! is_in_table()) && (margin_on)) {
	make_html_indent(left_margin);
      }
      if (g->minh-left_margin != 0) {
	make_html_indent(g->minh-left_margin);
      }
      change_font(g, TRUE);
      supress_sub_sup = FALSE;
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
      supress_sub_sup = TRUE;
      if (need_one_newline) {
	html_newline();
	temp_vpos += height;
      } else {
	need_one_newline = TRUE;
      }

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
    if (start_region_hpos > get_left()) {
      make_html_indent(start_region_hpos-get_left());
    }
  }
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
  if (strcmp(g->text_string, "ot") == 0) {
    stop();
  }
#endif
  if (! check_able_to_use_table(g)) {
    char *postword=html_position_text(g, left_margin_indent, right_margin_indent);

    if (! header.written_header) {
      if (g->is_raw_command) {
	html.put_string((char *)g->text_string);
      } else {
	translate_to_html(g);
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
      troff.put_troffps_char((char *)g->text_string);
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
 *  translate_to_html - translates a textual string into html text
 */

void html_printer::translate_to_html (text_glob *g)
{
  char buf[MAX_STRING_LENGTH];

  str_translate_to_html(g->text_style.f, buf, MAX_STRING_LENGTH,
			g->text_string, g->text_length, TRUE);
  html.put_string(buf);
}

/*
 *  html_knows_about - given a character name, troff, return TRUE
 *                     if we know how to display this character using
 *                     html unicode.
 */

int html_printer::html_knows_about (char *troff)
{
  // --fixme-- needs to have similar code as above
  return( FALSE );
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


/*
 *  flush_sbuf - flushes the current sbuf into the list of glyphs.
 */

void html_printer::flush_sbuf()
{
  if (sbuf_len > 0) {
    int r=font::res;   // resolution of the device
    set_style(sbuf_style);

    page_contents->add(&sbuf_style, sbuf, sbuf_len,
		       sbuf_vpos-sbuf_style.point_size*r/72, sbuf_start_hpos,
		       sbuf_vpos                           , sbuf_end_hpos);
	     
    output_hpos = sbuf_end_hpos;
    output_vpos = sbuf_vpos;
    sbuf_len = 0;
    sbuf_dmark_hpos = -1;
  }
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
  html.begin_comment("Page: ").comment_arg(i_to_a(page_number)).end_comment();;
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
    extern const char *Version_string;
    html.begin_comment("Creator     : ")
       .comment_arg("groff ")
       .comment_arg("version ")
       .comment_arg(Version_string)
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
  html.begin_comment("Total number of pages: ").comment_arg(i_to_a(no_of_printed_pages)).end_comment();
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

/*
 *  is_graphic_start - returns TRUE if the start of table, pic, eqn was seen.
 */

int is_graphic_start (char *s)
{
  return( (strcmp(s, "graphic-start") == 0) ||
	  ((strcmp(s, "table-start") == 0) && (table_image_on)) );
}

/*
 *  is_graphic_end - return TRUE if the end of a table, pic, eqn was seen.
 */

int is_graphic_end (char *s)
{
  return( (strcmp(s, "graphic-end") == 0) ||
	  ((strcmp(s, "table-end") == 0) && (table_image_on)) );
}

/*
 *  special - handle all x X requests from troff. For grohtml they allow users
 *            to pass raw html commands, turn auto linked headings off/on and
 *            also allow tbl, eqn & pic say what commands they have generated.
 */

void html_printer::special(char *s, const environment *env)
{
  if (s != 0) {
    if (is_graphic_start(s)) {
      graphic_level++;
      if (graphic_level == 1) {
	page_contents->is_in_graphic = TRUE;    // add words and lines to temporary region lists
      }
    } else if (is_graphic_end(s) && (graphic_level > 0)) {
      graphic_level--;
      if (graphic_level == 0) {
	flush_graphic();
      }
    } else if (strncmp(s, "html:", 5) == 0) {
      int r=font::res;   // resolution of the device
      char buf[MAX_STRING_LENGTH];
      font *f=sbuf_style.f;

      if (f == NULL) {
	int found=FALSE;

	f = font::load_font("TR", &found);
      }
      str_translate_to_html(f, buf, MAX_STRING_LENGTH,
			    &s[5], strlen(s)-5, FALSE);
      page_contents->add_html_command(&sbuf_style, buf, strlen(buf),

      // need to pass rest of string through to html output during flush

				      env->vpos-env->size*r/72, env->hpos,
				      env->vpos               , env->hpos);
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
  while ((c = getopt(argc, argv, "F:atTvdgmx?I:r:")) != EOF)
    switch(c) {
    case 'v':
      {
	extern const char *Version_string;
	fprintf(stderr, "grohtml version %s\n", Version_string);
	fflush(stderr);
	break;
      }
    case 'a':
      auto_on = FALSE;
      break;
    case 't':
      table_on = FALSE;
      break;
    case 'T':
      table_image_on = FALSE;
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
