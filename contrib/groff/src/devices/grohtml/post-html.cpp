// -*- C++ -*-
/* Copyright (C) 2000, 2001, 2002, 2003 Free Software Foundation, Inc.
 *
 *  Gaius Mulley (gaius@glam.ac.uk) wrote post-html.cpp
 *  but it owes a huge amount of ideas and raw code from
 *  James Clark (jjc@jclark.com) grops/ps.cpp.
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
#include "html-text.h"
#include "html-table.h"

#include <time.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <stdio.h>
#include <fcntl.h>

extern "C" const char *Version_string;

#if !defined(TRUE)
#   define TRUE  (1==1)
#endif
#if !defined(FALSE)
#   define FALSE (1==0)
#endif

#define MAX_LINE_LENGTH                60            /* maximum characters we want in a line      */
#define SIZE_INCREMENT                  2            /* font size increment <big> = +2            */
#define BASE_POINT_SIZE                10            /* 10 points is the base size ie html size 3 */
#define CENTER_TOLERANCE                2            /* how many pixels off center will we still  */
#define ANCHOR_TEMPLATE         "heading"            /* if simple anchor is set we use this       */
#define UNICODE_DESC_START           0x80            /* all character entities above this are     */
                                                     /* either encoded by their glyph names or if */
                                                     /* there is no name then we use &#nnn;       */
typedef enum {CENTERED, LEFT, RIGHT, INLINE} TAG_ALIGNMENT;
typedef enum {col_tag, tab_tag, tab0_tag, none} colType;

#undef DEBUG_TABLES


/*
 *  prototypes
 */

char *get_html_translation (font *f, const string &name);
int char_translate_to_html (font *f, char *buf, int buflen, unsigned char ch, int b, int and_single);


static int auto_links = TRUE;                        /* by default we enable automatic links at  */
                                                     /* top of the document.                     */
static int auto_rule  = TRUE;                        /* by default we enable an automatic rule   */
                                                     /* at the top and bottom of the document    */
static int simple_anchors = FALSE;                   /* default to anchors with heading text     */
static int manufacture_headings = FALSE;             /* default is to use the Hn html headings,  */
                                                     /* rather than manufacture our own.         */
static color *default_background = NULL;             /* has user requested initial bg color?     */


/*
 *  start with a few favorites
 */

void stop () {}

static int min (int a, int b)
{
  if (a < b)
    return a;
  else
    return b;
}

static int max (int a, int b)
{
  if (a > b)
    return a;
  else
    return b;
}

/*
 *  is_intersection - returns TRUE if range a1..a2 intersects with b1..b2
 */

static int is_intersection (int a1, int a2, int b1, int b2)
{
  // easier to prove NOT outside limits
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
 *  the classes and methods for maintaining a list of files.
 */

struct file {
  FILE    *fp;
  file    *next;

  file     (FILE *f);
};

/*
 *  file - initialize all fields to NULL
 */

file::file (FILE *f)
  : fp(f), next(0)
{
}

class files {
public:
            files         ();
  FILE     *get_file      (void);
  void      start_of_list (void);
  void      move_next     (void);
  void      add_new_file  (FILE *f);
private:
  file     *head;
  file     *tail;
  file     *ptr;
};

/*
 *  files - create an empty list of files.
 */

files::files ()
  : head(0), tail(0), ptr(0)
{
}

/*
 *  get_file - returns the FILE associated with ptr.
 */

FILE *files::get_file (void)
{
  if (ptr) {
    return( ptr->fp );
  } else {
    return( 0 );
  }
}

/*
 *  start_of_list - reset the ptr to the start of the list.
 */

void files::start_of_list (void)
{
  ptr = head;
}

/*
 *  move_next - moves the ptr to the next element on the list.
 */

void files::move_next (void)
{
  if (ptr != 0)
    ptr = ptr->next;
}

/*
 *  add_new_file - adds a new file, f, to the list.
 */

void files::add_new_file (FILE *f)
{
  if (head == 0) {
    head = new file(f);
    tail = head;
  } else {
    tail->next = new file(f);
    tail       = tail->next;
  }
  ptr = tail;
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
  color        col;
               style       ();
               style       (font *, int, int, int, int, color);
  int          operator == (const style &) const;
  int          operator != (const style &) const;
};

style::style()
  : f(0)
{
}

style::style(font *p, int sz, int h, int sl, int no, color c)
  : f(p), point_size(sz), font_no(no), height(h), slant(sl), col(c)
{
}

int style::operator==(const style &s) const
{
  return (f == s.f && point_size == s.point_size
	  && height == s.height && slant == s.slant && col == s.col);
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
  char         *buffer;
  int           used;
  char_block   *next;

  char_block();
  char_block(int length);
  ~char_block();
};

char_block::char_block()
: buffer(NULL), used(0), next(0)
{
}

char_block::char_block(int length)
: used(0), next(0)
{
  buffer = (char *)malloc(max(length, char_block::SIZE));
  if (buffer == NULL)
    fatal("out of memory error");
}

char_block::~char_block()
{
  if (buffer != NULL)
    free(buffer);
}

class char_buffer {
public:
  char_buffer();
  ~char_buffer();
  char  *add_string(const char *, unsigned int);
  char  *add_string(const string &);
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

char *char_buffer::add_string (const char *s, unsigned int length)
{
  int i=0;
  unsigned int old_used;

  if (s == NULL || length == 0)
    return NULL;

  if (tail == 0) {
    tail = new char_block(length+1);
    head = tail;
  } else {
    if (tail->used + length+1 > char_block::SIZE) {
      tail->next  = new char_block(length+1);
      tail        = tail->next;
    }
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

char *char_buffer::add_string (const string &s)
{
  return add_string(s.contents(), s.length());
}

/*
 *  the classes and methods for maintaining glyph positions.
 */

class text_glob {
public:
  void text_glob_html      (style *s, char *str, int length,
			    int min_vertical, int min_horizontal,
			    int max_vertical, int max_horizontal);
  void text_glob_special   (style *s, char *str, int length,
			    int min_vertical, int min_horizontal,
			    int max_vertical, int max_horizontal);
  void text_glob_line      (style *s,
			    int min_vertical, int min_horizontal,
			    int max_vertical, int max_horizontal,
			    int thickness);
  void text_glob_auto_image(style *s, char *str, int length,
			    int min_vertical, int min_horizontal,
			    int max_vertical, int max_horizontal);
  void text_glob_tag       (style *s, char *str, int length,
			    int min_vertical, int min_horizontal,
			    int max_vertical, int max_horizontal);
		       
  text_glob                (void);
  ~text_glob               (void);
  int  is_a_line           (void);
  int  is_a_tag            (void);
  int  is_eol              (void);
  int  is_auto_img         (void);
  int  is_br               (void);
  int  is_in               (void);
  int  is_po               (void);
  int  is_ti               (void);
  int  is_ce               (void);
  int  is_eol_ce           (void);
  int  is_col              (void);
  int  is_tab              (void);
  int  is_tab0             (void);
  int  is_ta               (void);
  int  is_tab_ts           (void);
  int  is_tab_te           (void);
  int  is_nf               (void);
  int  is_fi               (void);
  int  get_arg             (void);
  int  get_tab_args        (char *align);

  void        remember_table (html_table *t);
  html_table *get_table      (void);

  style           text_style;
  const char     *text_string;
  unsigned int    text_length;
  int             minv, minh, maxv, maxh;
  int             is_tag;               // is this a .br, .sp, .tl etc
  int             is_img_auto;          // image created by eqn delim
  int             is_special;           // text has come via 'x X html:'
  int             is_line;              // is the command a <line>?
  int             thickness;            // the thickness of a line
  html_table     *tab;                  // table description

private:
  text_glob           (style *s, const char *str, int length,
		       int min_vertical , int min_horizontal,
		       int max_vertical , int max_horizontal,
		       bool is_troff_command,
		       bool is_auto_image, bool is_special_command,
		       bool is_a_line    , int  thickness);
};

text_glob::text_glob (style *s, const char *str, int length,
		      int min_vertical, int min_horizontal,
		      int max_vertical, int max_horizontal,
		      bool is_troff_command,
		      bool is_auto_image, bool is_special_command,
		      bool is_a_line, int line_thickness)
  : text_style(*s), text_string(str), text_length(length),
    minv(min_vertical), minh(min_horizontal), maxv(max_vertical), maxh(max_horizontal),
    is_tag(is_troff_command), is_img_auto(is_auto_image), is_special(is_special_command),
    is_line(is_a_line), thickness(line_thickness), tab(NULL)
{
}

text_glob::text_glob ()
  : text_string(0), text_length(0), minv(-1), minh(-1), maxv(-1), maxh(-1),
    is_tag(FALSE), is_special(FALSE), is_line(FALSE), thickness(0), tab(NULL)
{
}

text_glob::~text_glob ()
{
  if (tab != NULL)
    delete tab;
}

/*
 *  text_glob_html - used to place html text into the glob buffer.
 */

void text_glob::text_glob_html (style *s, char *str, int length,
				int min_vertical , int min_horizontal,
				int max_vertical , int max_horizontal)
{
  text_glob *g = new text_glob(s, str, length,
			       min_vertical, min_horizontal, max_vertical, max_horizontal,
			       FALSE, FALSE, FALSE, FALSE, 0);
  *this = *g;
  delete g;
}

/*
 *  text_glob_html - used to place html specials into the glob buffer.
 *                   This text is essentially html commands coming through
 *                   from the macro sets, with special designated sequences of
 *                   characters translated into html. See add_and_encode.
 */

void text_glob::text_glob_special (style *s, char *str, int length,
				   int min_vertical , int min_horizontal,
				   int max_vertical , int max_horizontal)
{
  text_glob *g = new text_glob(s, str, length,
			       min_vertical, min_horizontal, max_vertical, max_horizontal,
			       FALSE, FALSE, TRUE, FALSE, 0);
  *this = *g;
  delete g;
}

/*
 *  text_glob_line - record horizontal draw line commands.
 */

void text_glob::text_glob_line (style *s,
				int min_vertical , int min_horizontal,
				int max_vertical , int max_horizontal,
				int thickness)
{
  text_glob *g = new text_glob(s, "", 0,
			       min_vertical, min_horizontal, max_vertical, max_horizontal,
			       FALSE, FALSE, FALSE, TRUE, thickness);
  *this = *g;
  delete g;
}

/*
 *  text_glob_auto_image - record the presence of a .auto-image tag command.
 *                         Used to mark that an image has been created automatically
 *                         by a preprocessor and (pre-grohtml/troff) combination.
 *                         Under some circumstances images may not be created.
 *                         (consider .EQ
 *                                   delim $$
 *                                   .EN
 *                                   .TS
 *                                   tab(!), center;
 *                                   l!l.
 *                                   $1 over x$!recripical of x
 *                                   .TE
 *
 *                          the first auto-image marker is created via .EQ/.EN pair
 *                          and no image is created.
 *                          The second auto-image marker occurs at $1 over x$
 *                          Currently this image will not be created
 *                          as the whole of the table is created as an image.
 *                          (Once html tables are handled by grohtml this will change.
 *                           Shortly this will be the case).
 */

void text_glob::text_glob_auto_image(style *s, char *str, int length,
				     int min_vertical, int min_horizontal,
				     int max_vertical, int max_horizontal)
{
  text_glob *g = new text_glob(s, str, length,
			       min_vertical, min_horizontal, max_vertical, max_horizontal,
			       TRUE, TRUE, FALSE, FALSE, 0);
  *this = *g;
  delete g;
}

/*
 *  text_glob_tag - records a troff tag.
 */

void text_glob::text_glob_tag (style *s, char *str, int length,
			       int min_vertical, int min_horizontal,
			       int max_vertical, int max_horizontal)
{
  text_glob *g = new text_glob(s, str, length,
			       min_vertical, min_horizontal, max_vertical, max_horizontal,
			       TRUE, FALSE, FALSE, FALSE, 0);
  *this = *g;
  delete g;
}

/*
 *  is_a_line - returns TRUE if glob should be converted into an <hr>
 */

int text_glob::is_a_line (void)
{
  return is_line;
}

/*
 *  is_a_tag - returns TRUE if glob contains a troff directive.
 */

int text_glob::is_a_tag (void)
{
  return is_tag;
}

/*
 *  is_eol - returns TRUE if glob contains the tag eol
 */

int text_glob::is_eol (void)
{
  return( is_tag && (strcmp(text_string, "html-tag:eol") == 0) );
}

/*
 *  is_eol_ce - returns TRUE if glob contains the tag eol.ce
 */

int text_glob::is_eol_ce (void)
{
  return( is_tag && (strcmp(text_string, "html-tag:eol.ce") == 0) );
}


/*
 *  is_nf - returns TRUE if glob contains the tag .nf
 */

int text_glob::is_nf (void)
{
  return( is_tag && (strcmp(text_string, "html-tag:.nf") == 0) );
}

/*
 *  is_fi - returns TRUE if glob contains the tag .fi
 */

int text_glob::is_fi (void)
{
  return( is_tag && (strcmp(text_string, "html-tag:.fi") == 0) );
}

/*
 *  is_ce - returns TRUE if glob contains the tag .ce
 */

int text_glob::is_ce (void)
{
  return( is_tag && (strcmp(text_string, "html-tag:.ce") == 0) );
}

/*
 *  is_in - returns TRUE if glob contains the tag .in
 */

int text_glob::is_in (void)
{
  return( is_tag && (strncmp(text_string, "html-tag:.in ", strlen("html-tag:.in ")) == 0) );
}

/*
 *  is_po - returns TRUE if glob contains the tag .po
 */

int text_glob::is_po (void)
{
  return( is_tag && (strncmp(text_string, "html-tag:.po ", strlen("html-tag:.po ")) == 0) );
}

/*
 *  is_ti - returns TRUE if glob contains the tag .ti
 */

int text_glob::is_ti (void)
{
  return( is_tag && (strncmp(text_string, "html-tag:.ti ", strlen("html-tag:.ti ")) == 0) );
}

/*
 *  is_col - returns TRUE if glob contains the tag .col
 */

int text_glob::is_col (void)
{
  return( is_tag && (strncmp(text_string, "html-tag:.col", strlen("html-tag:.col")) == 0) );
}

/*
 *  is_tab_ts - returns TRUE if glob contains the tag .tab_ts
 */

int text_glob::is_tab_ts (void)
{
  return( is_tag && (strcmp(text_string, "html-tag:.tab-ts") == 0) );
}

/*
 *  is_tab_te - returns TRUE if glob contains the tag .tab_te
 */

int text_glob::is_tab_te (void)
{
  return( is_tag && (strcmp(text_string, "html-tag:.tab-te") == 0) );
}

/*
 *  is_ta - returns TRUE if glob contains the tag .ta
 */

int text_glob::is_ta (void)
{
  return( is_tag && (strncmp(text_string, "html-tag:.ta ", strlen("html-tag:.ta ")) == 0) );
}

/*
 *  is_tab - returns TRUE if glob contains the tag tab
 */

int text_glob::is_tab (void)
{
  return( is_tag && (strncmp(text_string, "html-tag:tab ", strlen("html-tag:tab ")) == 0) );
}

/*
 *  is_tab0 - returns TRUE if glob contains the tag tab0
 */

int text_glob::is_tab0 (void)
{
  return( is_tag && (strncmp(text_string, "html-tag:tab0", strlen("html-tag:tab0")) == 0) );
}

/*
 *  is_auto_img - returns TRUE if the glob contains an automatically
 *                generated image.
 */

int text_glob::is_auto_img (void)
{
  return is_img_auto;
}

/*
 *  is_br - returns TRUE if the glob is a tag containing a .br
 *          or an implied .br. Note that we do not include .nf or .fi
 *          as grohtml will place a .br after these commands if they
 *          should break the line.
 */

int text_glob::is_br (void)
{
  return( is_a_tag() && ((strcmp ("html-tag:.br", text_string) == 0) ||
			 (strncmp("html-tag:.sp", text_string, 11) == 0) ||
			 (strcmp ("html-tag:.ce", text_string) == 0)) );
}

int text_glob::get_arg (void)
{
  if (strncmp("html-tag:", text_string, strlen("html-tag:")) == 0) {
    const char *p = text_string;

    while ((*p != (char)0) && (!isspace(*p)))
      p++;
    while ((*p != (char)0) && (isspace(*p)))
      p++;
    if (*p == (char)0)
      return -1;
    return atoi(p);
  }
  return -1;
}

/*
 *  get_tab_args - returns the tab position and alignment of the tab tag
 */

int text_glob::get_tab_args (char *align)
{
  if (strncmp("html-tag:", text_string, strlen("html-tag:")) == 0) {
    const char *p = text_string;

    // firstly the alignment C|R|L
    while ((*p != (char)0) && (!isspace(*p)))
      p++;
    while ((*p != (char)0) && (isspace(*p)))
      p++;
    *align = *p;
    // now the int value
    while ((*p != (char)0) && (!isspace(*p)))
      p++;
    while ((*p != (char)0) && (isspace(*p)))
      p++;
    if (*p == (char)0)
      return -1;
    return atoi(p);
  }
  return -1;
}

/*
 *  remember_table - saves table, t, in the text_glob.
 */

void text_glob::remember_table (html_table *t)
{
  if (tab != NULL)
    delete tab;
  tab = t;
}

/*
 *  get_table - returns the stored table description.
 */

html_table *text_glob::get_table (void)
{
  return tab;
}

/*
 *  the class and methods used to construct ordered double linked lists.
 *  In a previous implementation we used templates via #include "ordered-list.h",
 *  but this does assume that all C++ compilers can handle this feature. Pragmatically
 *  it is safer to assume this is not the case.
 */

struct element_list {
  element_list *right;
  element_list *left;
  text_glob    *datum;
  int           lineno;
  int           minv, minh, maxv, maxh;

  element_list  (text_glob *d,
		 int line_number,
		 int min_vertical, int min_horizontal,
		 int max_vertical, int max_horizontal);
  element_list  ();
  ~element_list ();
};

element_list::element_list ()
  : right(0), left(0), datum(0), lineno(0), minv(-1), minh(-1), maxv(-1), maxh(-1)
{
}

/*
 *  element_list - create a list element assigning the datum and region parameters.
 */

element_list::element_list (text_glob *in,
			    int line_number,
			    int min_vertical, int min_horizontal,
			    int max_vertical, int max_horizontal)
  : right(0), left(0), datum(in), lineno(line_number),
    minv(min_vertical), minh(min_horizontal), maxv(max_vertical), maxh(max_horizontal)
{
}

element_list::~element_list ()
{
  if (datum != NULL)
    delete datum;
}

class list {
public:
       list             ();
      ~list             ();
  int  is_less          (element_list *a, element_list *b);
  void add              (text_glob *in,
		         int line_number,
		         int min_vertical, int min_horizontal,
		         int max_vertical, int max_horizontal);
  void                  sub_move_right      (void);
  void                  move_right          (void);
  void                  move_left           (void);
  int                   is_empty            (void);
  int                   is_equal_to_tail    (void);
  int                   is_equal_to_head    (void);
  void                  start_from_head     (void);
  void                  start_from_tail     (void);
  void                  insert              (text_glob *in);
  void                  move_to             (text_glob *in);
  text_glob            *move_right_get_data (void);
  text_glob            *move_left_get_data  (void);
  text_glob            *get_data            (void);
private:
  element_list *head;
  element_list *tail;
  element_list *ptr;
};

/*
 *  list - construct an empty list.
 */

list::list ()
  : head(NULL), tail(NULL), ptr(NULL)
{
}

/*
 *  ~list - destroy a complete list.
 */

list::~list()
{
  element_list *temp=head;

  do {
    temp = head;
    if (temp != NULL) {
      head = head->right;
      delete temp;
    }
  } while ((head != NULL) && (head != tail));
}

/*
 *  is_less - returns TRUE if a is left of b if on the same line or
 *            if a is higher up the page than b.
 */

int list::is_less (element_list *a, element_list *b)
{
  // was if (is_intersection(a->minv+1, a->maxv-1, b->minv+1, b->maxv-1)) {
  if (a->lineno < b->lineno) {
    return( TRUE );
  } else if (a->lineno > b->lineno) {
    return( FALSE );
  } else if (is_intersection(a->minv, a->maxv, b->minv, b->maxv)) {
    return( a->minh < b->minh );
  } else {
    return( a->maxv < b->maxv );
  }
}

/*
 *  add - adds a datum to the list in the order specified by the region position.
 */

void list::add (text_glob *in, int line_number, int min_vertical, int min_horizontal, int max_vertical, int max_horizontal)
{
  // create a new list element with datum and position fields initialized
  element_list *t    = new element_list(in, line_number, min_vertical, min_horizontal, max_vertical, max_horizontal);
  element_list *last;

  if (head == 0) {
    head     = t;
    tail     = t;
    ptr      = t;
    t->left  = t;
    t->right = t;
  } else {
    last = tail;

    while ((last != head) && (is_less(t, last))) {
      last = last->left;
    }

    if (is_less(t, last)) {
      t->right          = last;
      last->left->right = t;
      t->left           = last->left;
      last->left        = t;
      // now check for a new head
      if (last == head) {
	head = t;
      }
    } else {
      // add t beyond last
      t->right          = last->right;
      t->left           = last;
      last->right->left = t;
      last->right       = t;
      // now check for a new tail
      if (last == tail) {
	tail = t;
      }
    }
  }
}

/*
 *  sub_move_right - removes the element which is currently pointed to by ptr
 *                   from the list and moves ptr to the right.
 */

void list::sub_move_right (void)
{
  element_list *t=ptr->right;

  if (head == tail) {
    head = 0;
    if (tail != 0) {
      delete tail;
    }
    tail = 0;
    ptr  = 0;
  } else {
    if (head == ptr) {
      head = head->right;
    }
    if (tail == ptr) {
      tail = tail->left;
    }
    ptr->left->right = ptr->right;
    ptr->right->left = ptr->left;
    ptr=t;
  }
}

/*
 *  start_from_head - assigns ptr to the head.
 */

void list::start_from_head (void)
{
  ptr = head;
}

/*
 *  start_from_tail - assigns ptr to the tail.
 */

void list::start_from_tail (void)
{
  ptr = tail;
}

/*
 *  is_empty - returns TRUE if the list has no elements.
 */

int list::is_empty (void)
{
  return( head == 0 );
}

/*
 *  is_equal_to_tail - returns TRUE if the ptr equals the tail.
 */

int list::is_equal_to_tail (void)
{
  return( ptr == tail );
}

/*
 *  is_equal_to_head - returns TRUE if the ptr equals the head.
 */

int list::is_equal_to_head (void)
{
  return( ptr == head );
}

/*
 *  move_left - moves the ptr left.
 */

void list::move_left (void)
{
  ptr = ptr->left;
}

/*
 *  move_right - moves the ptr right.
 */

void list::move_right (void)
{
  ptr = ptr->right;
}

/*
 *  get_datum - returns the datum referenced via ptr.
 */

text_glob* list::get_data (void)
{
  return( ptr->datum );
}

/*
 *  move_right_get_data - returns the datum referenced via ptr and moves
 *                        ptr right.
 */

text_glob* list::move_right_get_data (void)
{
  ptr = ptr->right;
  if (ptr == head) {
    return( 0 );
  } else {
    return( ptr->datum );
  }
}

/*
 *  move_left_get_data - returns the datum referenced via ptr and moves
 *                       ptr right.
 */

text_glob* list::move_left_get_data (void)
{
  ptr = ptr->left;
  if (ptr == tail) {
    return( 0 );
  } else {
    return( ptr->datum );
  }
}

/*
 *  insert - inserts data after the current position.
 */

void list::insert (text_glob *in)
{
  if (is_empty())
    fatal("list must not be empty if we are inserting data");
  else {
    if (ptr == 0)
      ptr = head;
    
    element_list *t = new element_list(in, ptr->lineno, ptr->minv, ptr->minh, ptr->maxv, ptr->maxh);
    if (ptr == tail)
      tail = t;
    ptr->right->left = t;
    t->right = ptr->right;
    ptr->right = t;
    t->left = ptr;
  }
}

/*
 *  move_to - moves the current position to the point where data, in, exists.
 *            This is an expensive method and should be used sparingly.
 */

void list::move_to (text_glob *in)
{
  ptr = head;
  while (ptr != tail && ptr->datum != in)
    ptr = ptr->right;
}

/*
 *  page class and methods
 */

class page {
public:
                              page            (void);
  void                        add             (style *s, const string &str,
					       int line_number,
					       int min_vertical, int min_horizontal,
					       int max_vertical, int max_horizontal);
  void                        add_tag         (style *s, const string &str,
					       int line_number,
					       int min_vertical, int min_horizontal,
					       int max_vertical, int max_horizontal);
  void                        add_and_encode  (style *s, const string &str,
					       int line_number,
					       int min_vertical, int min_horizontal,
					       int max_vertical, int max_horizontal);
  void                        add_line        (style *s,
					       int line_number,
					       int x1, int y1, int x2, int y2,
					       int thickness);
  void                        insert_tag      (const string &str);
  void                        dump_page       (void);   // debugging method

  // and the data

  list                        glyphs;         // position of glyphs and specials on page
  char_buffer                 buffer;         // all characters for this page
};

page::page()
{
}

/*
 *  insert_tag - inserts a tag after the current position.
 */

void page::insert_tag (const string &str)
{
  if (str.length() > 0) {
    text_glob *g=new text_glob();
    text_glob *f=glyphs.get_data();
    g->text_glob_tag(&f->text_style, buffer.add_string(str), str.length(),
		     f->minv, f->minh, f->maxv, f->maxh);
    glyphs.insert(g);
  }
}

/*
 *  add - add html text to the list of glyphs.
 */

void page::add (style *s, const string &str,
		int line_number,
		int min_vertical, int min_horizontal,
		int max_vertical, int max_horizontal)
{
  if (str.length() > 0) {
    text_glob *g=new text_glob();
    g->text_glob_html(s, buffer.add_string(str), str.length(),
		      min_vertical, min_horizontal, max_vertical, max_horizontal);
    glyphs.add(g, line_number, min_vertical, min_horizontal, max_vertical, max_horizontal);
  }
}

/*
 *  add_tag - adds a troff tag, for example: .tl .sp .br
 */

void page::add_tag (style *s, const string &str,
		    int line_number,
		    int min_vertical, int min_horizontal,
		    int max_vertical, int max_horizontal)
{
  if (str.length() > 0) {
    text_glob *g;

    if (strncmp((str+'\0').contents(), "html-tag:.auto-image", 20) == 0) {
      g = new text_glob();
      g->text_glob_auto_image(s, buffer.add_string(str), str.length(),
			      min_vertical, min_horizontal, max_vertical, max_horizontal);
    } else {
      g = new text_glob();
      g->text_glob_tag(s, buffer.add_string(str), str.length(),
		       min_vertical, min_horizontal, max_vertical, max_horizontal);
    }
    glyphs.add(g, line_number, min_vertical, min_horizontal, max_vertical, max_horizontal);
  }
}

/*
 *  add_line - adds the <line> primitive providing that y1==y2
 */

void page::add_line (style *s,
		     int line_number,
		     int x1, int y1, int x2, int y2,
		     int thickness)
{
  if (y1 == y2) {
    text_glob *g = new text_glob();
    g->text_glob_line(s,
		      min(y1, y2), min(x1, y2), max(y1, y2), max(x1, x2),
		      thickness);
    glyphs.add(g, line_number, min(y1, y2), min(x1, y2), max(y1, y2), max(x1, x2));
  }
}

/*
 *  to_unicode - returns a unicode translation of int, ch.
 */

static char *to_unicode (unsigned int ch)
{
  static char buf[30];

  sprintf(buf, "&#%u;", ch);
  return buf;
}

/*
 *  add_and_encode - adds a special string to the page, it translates the string
 *                   into html glyphs. The special string will have come from x X html:
 *                   and can contain troff character encodings which appear as
 *                   \(char\). A sequence of \\ represents \.
 *                   So for example we can write:
 *                      "cost = \(Po\)3.00 file = \\foo\\bar"
 *                   which is translated into:
 *                      "cost = &pound;3.00 file = \foo\bar"
 */

void page::add_and_encode (style *s, const string &str,
			   int line_number,
			   int min_vertical, int min_horizontal,
			   int max_vertical, int max_horizontal)
{
  string html_string;
  char *html_glyph;
  int i=0;

  if (s->f == NULL)
    return;
  while (i < str.length()) {
    if ((i+1<str.length()) && (str.substring(i, 2) == string("\\("))) {
      // start of escape
      i += 2; // move over \(
      int a = i;
      while ((i+1<str.length()) && (str.substring(i, 2) != string("\\)"))) {
	i++;
      }
      int n = i;
      if ((i+1<str.length()) && (str.substring(i, 2) == string("\\)")))
	i++;
      else
	n = -1;
      if (n > 0) {
	string troff_charname = str.substring(a, n-a);
	html_glyph = get_html_translation(s->f, troff_charname);
	if (html_glyph)
	  html_string += html_glyph;
	else {
	  int index=s->f->name_to_index((troff_charname + '\0').contents());
	  
	  if (s->f->contains(index) && (index != 0))
	    html_string += s->f->get_code(index);
	}
      }
    } else
      html_string += str[i];
    i++;
  }
  if (html_string.length() > 0) {
    text_glob *g=new text_glob();
    g->text_glob_special(s, buffer.add_string(html_string), html_string.length(),
			 min_vertical, min_horizontal, max_vertical, max_horizontal);
    glyphs.add(g, line_number, min_vertical, min_horizontal, max_vertical, max_horizontal);
  }
}

/*
 *  dump_page - dump the page contents for debugging purposes.
 */

void page::dump_page(void)
{
#if defined(DEBUG_TABLES)
  text_glob *old_pos = glyphs.get_data();
  text_glob *g;

  printf("\n<!--\n");
  printf("\n\ndebugging start\n");
  glyphs.start_from_head();
  do {
    g = glyphs.get_data();
    if (g->is_tab_ts()) {
      printf("\n\n");
      if (g->get_table() != NULL)
	g->get_table()->dump_table();
    }
    printf("%s ", g->text_string);
    if (g->is_tab_te())
      printf("\n\n");
    glyphs.move_right();
  } while (! glyphs.is_equal_to_head());
  glyphs.move_to(old_pos);
  printf("\ndebugging end\n\n");
  printf("\n-->\n");
  fflush(stdout);
#endif
}

/*
 *  font classes and methods
 */

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
  int     with_h1;
  string  text;
};


title_desc::title_desc ()
  : has_been_written(FALSE), has_been_found(FALSE), with_h1(FALSE)
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
  list                      headers;             // list of headers built from .NH and .SH
  int                       header_level;        // current header level
  int                       written_header;      // have we written the header yet?
  string                    header_buffer;       // current header text

  void                      write_headings (FILE *f, int force);
};

header_desc::header_desc ()
  :   no_of_headings(0), header_level(2), written_header(0)
{
}

header_desc::~header_desc ()
{
}

/*
 *  write_headings - emits a list of links for the headings in this document
 */

void header_desc::write_headings (FILE *f, int force)
{
  text_glob *g;

  if (auto_links || force) {
    if (! headers.is_empty()) {
      int h=1;

      headers.start_from_head();
      do {
	g = headers.get_data();
	fputs("<a href=\"#", f);
	if (simple_anchors) {
	  string buffer(ANCHOR_TEMPLATE);

	  buffer += as_string(h);
	  buffer += '\0';
	  fprintf(f, buffer.contents());
	} else
	  fputs(g->text_string, f);
	h++;
	fputs("\">", f);
	fputs(g->text_string, f);
        fputs("</a><br>\n", f);
	headers.move_right();
      } while (! headers.is_equal_to_head());
      fputs("\n", f);
    }
  }
}

class html_printer : public printer {
  files                file_list;
  simple_output        html;
  int                  res;
  int                  space_char_index;
  int                  space_width;
  int                  no_of_printed_pages;
  int                  paper_length;
  string               sbuf;
  int                  sbuf_start_hpos;
  int                  sbuf_vpos;
  int                  sbuf_end_hpos;
  int                  sbuf_prev_hpos;
  int                  sbuf_kern;
  style                sbuf_style;
  int                  last_sbuf_length;
  int                  overstrike_detected;
  style                output_style;
  int                  output_hpos;
  int                  output_vpos;
  int                  output_vpos_max;
  int                  output_draw_point_size;
  int                  line_thickness;
  int                  output_line_thickness;
  unsigned char        output_space_code;
  char                *inside_font_style;
  int                  page_number;
  title_desc           title;
  header_desc          header;
  int                  header_indent;
  int                  supress_sub_sup;
  int                  cutoff_heading;
  page                *page_contents;
  html_text           *current_paragraph;
  html_indent         *indent;
  html_table          *table;
  int                  end_center;
  int                  end_tempindent;
  TAG_ALIGNMENT        next_tag;
  int                  fill_on;
  int                  max_linelength;
  int                  linelength;
  int                  pageoffset;
  int                  indentation;
  int                  prev_indent;
  int                  pointsize;
  int                  vertical_spacing;
  int                  line_number;
  color               *background;

  void  flush_sbuf                    ();
  void  set_style                     (const style &);
  void  set_space_code                (unsigned char c);
  void  do_exec                       (char *, const environment *);
  void  do_import                     (char *, const environment *);
  void  do_def                        (char *, const environment *);
  void  do_mdef                       (char *, const environment *);
  void  do_file                       (char *, const environment *);
  void  set_line_thickness            (const environment *);
  void  terminate_current_font        (void);
  void  flush_font                    (void);
  void  add_to_sbuf                   (int index, const string &s);
  void  write_title                   (int in_head);
  int   sbuf_continuation             (int index, const char *name, const environment *env, int w);
  void  flush_page                    (void);
  void  troff_tag                     (text_glob *g);
  void  flush_globs                   (void);
  void  emit_line                     (text_glob *g);
  void  emit_raw                      (text_glob *g);
  void  emit_html                     (text_glob *g);
  void  determine_space               (text_glob *g);
  void  start_font                    (const char *name);
  void  end_font                      (const char *name);
  int   is_font_courier               (font *f);
  int   is_courier_until_eol          (void);
  void  start_size                    (int from, int to);
  void  do_font                       (text_glob *g);
  void  do_center                     (char *arg);
  void  do_break                      (void);
  void  do_eol                        (void);
  void  do_eol_ce                     (void);
  void  do_title                      (void);
  void  do_fill                       (int on);
  void  do_heading                    (char *arg);
  void  write_header                  (void);
  void  determine_header_level        (int level);
  void  do_linelength                 (char *arg);
  void  do_pageoffset                 (char *arg);
  void  do_indentation                (char *arg);
  void  do_tempindent                 (char *arg);
  void  do_indentedparagraph          (void);
  void  do_verticalspacing            (char *arg);
  void  do_pointsize                  (char *arg);
  void  do_centered_image             (void);
  void  do_left_image                 (void);
  void  do_right_image                (void);
  void  do_auto_image                 (text_glob *g, const char *filename);
  void  do_links                      (void);
  void  do_flush                      (void);
  int   is_in_middle                  (int left, int right);
  void  do_sup_or_sub                 (text_glob *g);
  int   start_subscript               (text_glob *g);
  int   end_subscript                 (text_glob *g);
  int   start_superscript             (text_glob *g);
  int   end_superscript               (text_glob *g);
  void  outstanding_eol               (int n);
  int   is_bold                       (font *f);
  font *make_bold                     (font *f);
  int   overstrike                    (int index, const char *name, const environment *env, int w);
  void  do_body                       (void);
  int   next_horiz_pos                (text_glob *g, int nf);
  void  lookahead_for_tables          (void);
  void  insert_tab_te                 (void);
  text_glob *insert_tab_ts            (text_glob *where);
  void insert_tab0_foreach_tab        (void);
  void insert_tab_0                   (text_glob *where);
  void do_indent                      (int in, int pageoff, int linelen);
  void shutdown_table                 (void);
  void do_tab_ts                      (text_glob *g);
  void do_tab_te                      (void);
  void do_col                         (char *s);
  void do_tab                         (char *s);
  void do_tab0                        (void);
  int  calc_nf                        (text_glob *g, int nf);
  void calc_po_in                     (text_glob *g, int nf);
  void remove_tabs                    (void);
  void remove_courier_tabs            (void);
  void update_min_max                 (colType type_of_col, int *minimum, int *maximum, text_glob *g);
  void add_table_end                  (const char *);
  // ADD HERE

public:
  html_printer          ();
  ~html_printer         ();
  void set_char         (int i, font *f, const environment *env, int w, const char *name);
  void set_numbered_char(int num, const environment *env, int *widthp);
  void draw             (int code, int *p, int np, const environment *env);
  void begin_page       (int);
  void end_page         (int);
  void special          (char *arg, const environment *env, char type);
  font *make_font       (const char *);
  void end_of_line      ();
};

printer *make_printer()
{
  return new html_printer;
}

static void usage(FILE *stream);

void html_printer::set_style(const style &sty)
{
  const char *fontname = sty.f->get_name();
  if (fontname == NULL)
    fatal("no internalname specified for font");

#if 0
  change_font(fontname, (font::res/(72*font::sizescale))*sty.point_size);
#endif
}

/*
 *  is_bold - returns TRUE if font, f, is bold.
 */

int html_printer::is_bold (font *f)
{
  const char *fontname = f->get_name();
  return (strcmp(fontname, "B") == 0) || (strcmp(fontname, "BI") == 0);
}

/*
 *  make_bold - if a bold font of, f, exists then return it.
 */

font *html_printer::make_bold (font *f)
{
  const char *fontname = f->get_name();

  if (strcmp(fontname, "B") == 0)
    return f;
  if (strcmp(fontname, "I") == 0)
    return font::load_font("BI");
  if (strcmp(fontname, "BI") == 0)
    return f;
  return NULL;
}

void html_printer::end_of_line()
{
  flush_sbuf();
  line_number++;
}

/*
 *  emit_line - writes out a horizontal rule.
 */

void html_printer::emit_line (text_glob *)
{
  // --fixme-- needs to know the length in percentage
  html.put_string("<hr>");
}

/*
 *  emit_raw - writes the raw html information directly to the device.
 */

void html_printer::emit_raw (text_glob *g)
{
  do_font(g);
  if (next_tag == INLINE) {
    determine_space(g);
    current_paragraph->do_emittext(g->text_string, g->text_length);
  } else {
    current_paragraph->done_para();
    switch (next_tag) {

    case CENTERED:
      current_paragraph->do_para("align=center");
      break;
    case LEFT:
      current_paragraph->do_para(&html, "align=left", indentation, pageoffset, linelength);
      break;
    case RIGHT:
      current_paragraph->do_para(&html, "align=right", indentation, pageoffset, linelength);
      break;
    default:
      fatal("unknown enumeration");
    }
    current_paragraph->do_emittext(g->text_string, g->text_length);
    current_paragraph->done_para();
    next_tag        = INLINE;
    supress_sub_sup = TRUE;
    if (indentation > 0) {
      /*
       *  restore indentation
       */
      int newin = indentation;
      indentation = 0;
      do_indent(newin, pageoffset, linelength);
    }
  }
}

/*
 *  do_center - handle the .ce commands from troff.
 */

void html_printer::do_center (char *arg)
{
  int n = atoi(arg);
  current_paragraph->do_break();

  if (n > 0) {
    current_paragraph->done_para();
    supress_sub_sup = TRUE;
    current_paragraph->do_para("align=center");
    end_center += n;
  } else {
    end_center = 0;
    current_paragraph->remove_para_align();
  }
}

/*
 *  do_centered_image - set a flag such that the next html-tag is
 *                      placed inside a centered paragraph.
 */

void html_printer::do_centered_image (void)
{
  next_tag = CENTERED;
}

/*
 *  do_right_image - set a flag such that the next html-tag is
 *                   placed inside a right aligned paragraph.
 */

void html_printer::do_right_image (void)
{
  next_tag = RIGHT;
}

/*
 *  do_left_image - set a flag such that the next html-tag is
 *                  placed inside a left aligned paragraph.
 */

void html_printer::do_left_image (void)
{
  next_tag = LEFT;
}

/*
 *  exists - returns TRUE if filename exists.
 */

static int exists (const char *filename)
{
  FILE *fp = fopen(filename, "r");

  if (fp == 0) {
    return( FALSE );
  } else {
    fclose(fp);
    return( TRUE );
  }
}

/*
 *  generate_img_src - returns a html image tag for the filename
 *                     providing that the image exists.
 */

static string &generate_img_src (const char *filename)
{
  string *s = new string("");

  while (filename && (filename[0] == ' ')) {
    filename++;
  }
  if (exists(filename))
    *s += string("<img src=\"") + filename + "\">";
  return *s;
}

/*
 *  do_auto_image - tests whether the image, indicated by filename,
 *                  is present, if so then it emits an html image tag.
 *                  An image tag may be passed through from pic, eqn
 *                  but the corresponding image might not be created.
 *                  Consider .EQ delim $$ .EN  or an empty .PS .PE.
 */

void html_printer::do_auto_image (text_glob *g, const char *filename)
{
  string buffer = generate_img_src(filename);
  
  if (! buffer.empty()) {
    /*
     *  utilize emit_raw by creating a new text_glob.
     */
    text_glob h = *g;

    h.text_string = buffer.contents();
    h.text_length = buffer.length();
    emit_raw(&h);
  } else
    next_tag = INLINE;
}

/*
 *  outstanding_eol - call do_eol, n, times.
 */

void html_printer::outstanding_eol (int n)
{
  while (n > 0) {
    do_eol();
    n--;
  }
}

/*
 *  do_title - handle the .tl commands from troff.
 */

void html_printer::do_title (void)
{
  text_glob    *t;
  int           removed_from_head;
  int           eol_ce = 0;

  if (page_number == 1) {
    int found_title_start  = FALSE;
    if (! page_contents->glyphs.is_empty()) {
      page_contents->glyphs.sub_move_right();       /* move onto next word */
      do {
	t = page_contents->glyphs.get_data();
	removed_from_head = FALSE;
	if (t->is_auto_img()) {
	  string img = generate_img_src((char *)(t->text_string + 20));

	  if (! img.empty()) {
	    if (found_title_start)
	      title.text += " ";
	    found_title_start = TRUE;
	    title.has_been_found = TRUE;
	    title.text += img;
	  }
	  page_contents->glyphs.sub_move_right(); 	  /* move onto next word */
	  removed_from_head = ((!page_contents->glyphs.is_empty()) &&
			       (page_contents->glyphs.is_equal_to_head()));
	} else if (t->is_eol_ce()) {
	  /*  process the eol associated with .ce
	   */
	  eol_ce++;
	  page_contents->glyphs.sub_move_right(); 	  /* move onto next word */
	} else if (t->is_eol()) {
	  /* end of title found
	   */
	  title.has_been_found = TRUE;
	  outstanding_eol(eol_ce);
	  return;
	} else if (t->is_a_tag()) {
	  /* end of title found, but move back so that we read this tag and process it
	   */
	  page_contents->glyphs.move_left();           /* move backwards to last word */
	  title.has_been_found = TRUE;
	  outstanding_eol(eol_ce);
	  return;
	} else if (found_title_start) {
	  title.text += " " + string(t->text_string, t->text_length);
	  page_contents->glyphs.sub_move_right(); 	  /* move onto next word */
	  removed_from_head = ((!page_contents->glyphs.is_empty()) &&
			       (page_contents->glyphs.is_equal_to_head()));
	} else {
	  title.text += string(t->text_string, t->text_length);
	  found_title_start    = TRUE;
	  title.has_been_found = TRUE;
	  page_contents->glyphs.sub_move_right(); 	  /* move onto next word */
	  removed_from_head = ((!page_contents->glyphs.is_empty()) &&
			       (page_contents->glyphs.is_equal_to_head()));
	}
      } while ((! page_contents->glyphs.is_equal_to_head()) || (removed_from_head));
    }
    outstanding_eol(eol_ce);
  }
}

void html_printer::write_header (void)
{
  if (! header.header_buffer.empty()) {
    if (header.header_level > 7) {
      header.header_level = 7;
    }

    // firstly we must terminate any font and type faces
    current_paragraph->done_para();
    supress_sub_sup = TRUE;

    if (cutoff_heading+2 > header.header_level) {
      // now we save the header so we can issue a list of links
      header.no_of_headings++;
      style st;

      text_glob *h=new text_glob();
      h->text_glob_html(&st,
			header.headings.add_string(header.header_buffer),
			header.header_buffer.length(),
			header.no_of_headings, header.header_level,
			header.no_of_headings, header.header_level);

      header.headers.add(h,
			 header.no_of_headings,
			 header.no_of_headings, header.no_of_headings,
			 header.no_of_headings, header.no_of_headings);   // and add this header to the header list

      // lastly we generate a tag

      html.nl().put_string("<a name=\"");
      if (simple_anchors) {
	string buffer(ANCHOR_TEMPLATE);

	buffer += as_string(header.no_of_headings);
	buffer += '\0';
	html.put_string(buffer.contents());
      } else {
	html.put_string(header.header_buffer);
      }
      html.put_string("\"></a>").nl();
    }

    if (manufacture_headings) {
      // line break before a header
      if (!current_paragraph->emitted_text())
	current_paragraph->do_space();
      // user wants manufactured headings which look better than <Hn></Hn>
      if (header.header_level<4) {
	html.put_string("<b><font size=\"+1\">");
	html.put_string(header.header_buffer);
	html.put_string("</font></b>").nl();
      }
      else {
	html.put_string("<b>");
	html.put_string(header.header_buffer);
	html.put_string("</b>").nl();
      }
    }
    else {
      // and now we issue the real header
      html.put_string("<h");
      html.put_number(header.header_level);
      html.put_string(">");
      html.put_string(header.header_buffer);
      html.put_string("</h");
      html.put_number(header.header_level);
      html.put_string(">").nl();
    }

    current_paragraph->do_para(&html, "", indentation, pageoffset, linelength);
  }
}

void html_printer::determine_header_level (int level)
{
  if (level == 0) {
    int i;

    for (i=0; ((i<header.header_buffer.length())
	       && ((header.header_buffer[i] == '.')
		   || is_digit(header.header_buffer[i]))) ; i++) {
      if (header.header_buffer[i] == '.') {
	level++;
      }
    }
  }
  header.header_level = level+1;
}

/*
 *  do_heading - handle the .SH and .NH and equivalent commands from troff.
 */

void html_printer::do_heading (char *arg)
{
  text_glob *g;
  text_glob *l = 0;
  int  level=atoi(arg);

  header.header_buffer.clear();
  page_contents->glyphs.move_right();
  if (! page_contents->glyphs.is_equal_to_head()) {
    g = page_contents->glyphs.get_data();
    do {
      if (g->is_auto_img()) {
	string img=generate_img_src((char *)(g->text_string + 20));

	if (! img.empty()) {
	  simple_anchors = TRUE;  // we cannot use full heading anchors with images
	  if (l != 0)
	    header.header_buffer += " ";
	  
	  l = g;
	  header.header_buffer += img;
	}
      } else if (! (g->is_a_line() || g->is_a_tag())) {
	/*
	 *  we ignore tags commands when constructing a heading
	 */
	if (l != 0)
	  header.header_buffer += " ";
	l = g;

	header.header_buffer += string(g->text_string, g->text_length);
      }
      page_contents->glyphs.move_right();
      g = page_contents->glyphs.get_data();
    } while ((! page_contents->glyphs.is_equal_to_head()) &&
	     (! g->is_br()));
  }

  determine_header_level(level);
  write_header();

  // finally set the output to neutral for after the header
  g = page_contents->glyphs.get_data();
  page_contents->glyphs.move_left();     // so that next time we use old g
}

/*
 *  is_courier_until_eol - returns TRUE if we can see a whole line which is courier
 */

int html_printer::is_courier_until_eol (void)
{
  text_glob *orig = page_contents->glyphs.get_data();
  int result      = TRUE;
  text_glob *g;

  if (! page_contents->glyphs.is_equal_to_tail()) {
    page_contents->glyphs.move_right();
    do {
      g = page_contents->glyphs.get_data();
      if (! g->is_a_tag() && (! is_font_courier(g->text_style.f)))
	result = FALSE;
      page_contents->glyphs.move_right();
    } while (result &&
	     (! page_contents->glyphs.is_equal_to_head()) &&
	     (! g->is_fi()) && (! g->is_eol()));
    
    /*
     *  now restore our previous position.
     */
    while (page_contents->glyphs.get_data() != orig)
      page_contents->glyphs.move_left();
  }
  return result;
}

/*
 *  do_linelength - handle the .ll command from troff.
 */

void html_printer::do_linelength (char *arg)
{
  if (max_linelength == -1)
    max_linelength = atoi(arg);

  if (fill_on)
    do_indent(indentation, pageoffset, atoi(arg));
}

/*
 *  do_pageoffset - handle the .po command from troff.
 */

void html_printer::do_pageoffset (char *arg)
{
  if (fill_on)
    do_indent(indentation, atoi(arg), linelength);
}

/*
 *  do_indentation - handle the .in command from troff.
 */

void html_printer::do_indentation (char *arg)
{
  if (fill_on)
    do_indent(atoi(arg), pageoffset, linelength);
}

/*
 *  do_tempindent - handle the .ti command from troff.
 */

void html_printer::do_tempindent (char *arg)
{
  if (fill_on) {
    end_tempindent = 1;
    prev_indent    = indentation;
    do_indent(atoi(arg), pageoffset, linelength);
  }
}

/*
 *  shutdown_table - shuts down the current table.
 */

void html_printer::shutdown_table (void)
{
  if (table != NULL) {
    current_paragraph->done_para();
    table->emit_finish_table();
    // dont delete this table as it will be deleted when we destroy the text_glob
    table = NULL;
  }
}

/*
 *  do_indent - remember the indent parameters and if
 *              indent is > pageoff and indent has changed
 *              then we start a html table to implement the indentation.
 */

void html_printer::do_indent (int in, int pageoff, int linelen)
{
  if ((indentation != -1) &&
      (pageoffset+indentation != in+pageoff)) {
    
    current_paragraph->done_para();
      
    indentation = in;
    pageoffset  = pageoff;
    if (linelen <= max_linelength)
      linelength  = linelen;

    current_paragraph->do_para(&html, "", indentation, pageoffset, max_linelength);
  }
}

/*
 *  do_verticalspacing - handle the .vs command from troff.
 */

void html_printer::do_verticalspacing (char *arg)
{
  vertical_spacing = atoi(arg);
}

/*
 *  do_pointsize - handle the .ps command from troff.
 */

void html_printer::do_pointsize (char *arg)
{
  pointsize = atoi(arg);
}

/*
 *  do_fill - records whether troff has requested that text be filled.
 */

void html_printer::do_fill (int on)
{
  current_paragraph->do_break();
  output_hpos = indentation+pageoffset;
  supress_sub_sup = TRUE;

  if (fill_on != on) {
    if (on)
      current_paragraph->do_para("");
    else
      current_paragraph->do_pre();
    fill_on = on;
  }
}

/*
 *  do_eol - handle the end of line
 */

void html_printer::do_eol (void)
{
  if (! fill_on) {
    if (current_paragraph->ever_emitted_text()) {
      current_paragraph->do_newline();
      current_paragraph->do_break();
    }
  }
  output_hpos = indentation+pageoffset;
}

/*
 *  do_eol_ce - handle end of line specifically for a .ce
 */

void html_printer::do_eol_ce (void)
{
  if (end_center > 0) {
    if (end_center > 1)
      if (current_paragraph->emitted_text())
	current_paragraph->do_break();
    
    end_center--;
    if (end_center == 0) {
      current_paragraph->done_para();
      supress_sub_sup = TRUE;
    }
  }
}

/*
 *  do_flush - flushes all output and tags.
 */

void html_printer::do_flush (void)
{
  current_paragraph->done_para();
}

/*
 *  do_links - moves onto a new temporary file and sets auto_links to FALSE.
 */

void html_printer::do_links (void)
{
  current_paragraph->done_para();
  auto_links = FALSE;   /* from now on only emit under user request */
  file_list.add_new_file(xtmpfile());
  html.set_file(file_list.get_file());
}

/*
 *  do_break - handles the ".br" request and also
 *             undoes an outstanding ".ti" command.
 */

void html_printer::do_break (void)
{
  current_paragraph->do_break();
  if (end_tempindent > 0) {
    end_tempindent--;
    if (end_tempindent == 0)
      do_indent(prev_indent, pageoffset, linelength);
  }
  output_hpos = indentation+pageoffset;
  supress_sub_sup = TRUE;
}

/*
 *  do_tab_ts - start a table, which will have already been defined.
 */

void html_printer::do_tab_ts (text_glob *g)
{
  html_table *t = g->get_table();

  if (t != NULL) {
    current_paragraph->done_pre();
    current_paragraph->done_para();

    html.simple_comment("TABS");

    t->set_linelength(max_linelength);
    t->add_indent(pageoffset);
    t->emit_table_header(FALSE);
  }

  table = t;
}

/*
 *  do_tab_te - finish a table.
 */

void html_printer::do_tab_te (void)
{
  if (table) {
    current_paragraph->done_para();
    table->emit_finish_table();
  }

  table = NULL;

  if (indentation > 0) {
    /*
     *  restore indentation
     */
    int newin = indentation;
    indentation = 0;
    do_indent(newin, pageoffset, linelength);
  }
}

/*
 *  do_tab - handle the "html-tag:tab" tag
 */

void html_printer::do_tab (char *s)
{
  if (table) {
    while (isspace(*s))
      s++;
    s++;
    int col = table->find_column(atoi(s) + pageoffset + indentation);
    if (col > 0) {
      current_paragraph->done_para();
      table->emit_col(col);
    }
  }
}

/*
 *  do_tab0 - handle the "html-tag:tab0" tag
 */

void html_printer::do_tab0 (void)
{
  if (table) {
    int col = table->find_column(pageoffset+indentation);
    if (col > 0) {
      current_paragraph->done_para();
      table->emit_col(col);
    }
  }
}

/*
 *  do_col - start column, s.
 */

void html_printer::do_col (char *s)
{
  if (table) {
    current_paragraph->done_para();
    table->emit_col(atoi(s));
  }
}

/*
 *  troff_tag - processes the troff tag and manipulates the troff state machine.
 */

void html_printer::troff_tag (text_glob *g)
{
  /*
   *  firstly skip over html-tag:
   */
  char *t=(char *)g->text_string+9;

  if (g->is_eol()) {
    do_eol();
  } else if (g->is_eol_ce()) {
    do_eol_ce();
  } else if (strncmp(t, ".sp", 3) == 0) {
    if (g->get_arg() > 0)
      current_paragraph->do_space();
    else
      current_paragraph->do_break();
    supress_sub_sup = TRUE;
  } else if (strncmp(t, ".br", 3) == 0) {
    do_break();
  } else if (strcmp(t, ".centered-image") == 0) {
    do_centered_image();
  } else if (strcmp(t, ".right-image") == 0) {
    do_right_image();
  } else if (strcmp(t, ".left-image") == 0) {
    do_left_image();
  } else if (strncmp(t, ".auto-image", 11) == 0) {
    char *a = (char *)t+11;
    do_auto_image(g, a);
  } else if (strncmp(t, ".ce", 3) == 0) {
    char *a = (char *)t+3;
    supress_sub_sup = TRUE;
    do_center(a);
  } else if (strncmp(t, ".tl", 3) == 0) {
    supress_sub_sup = TRUE;
    title.with_h1 = TRUE;
    do_title();
  } else if (strncmp(t, ".html-tl", 8) == 0) {
    supress_sub_sup = TRUE;
    title.with_h1 = FALSE;
    do_title();
  } else if (strncmp(t, ".fi", 3) == 0) {
    do_fill(TRUE);
  } else if (strncmp(t, ".nf", 3) == 0) {
    do_fill(FALSE);
  } else if ((strncmp(t, ".SH", 3) == 0) || (strncmp(t, ".NH", 3) == 0)) {
    char *a = (char *)t+3;
    do_heading(a);
  } else if (strncmp(t, ".ll", 3) == 0) {
    char *a = (char *)t+3;
    do_linelength(a);
  } else if (strncmp(t, ".po", 3) == 0) {
    char *a = (char *)t+3;
    do_pageoffset(a);
  } else if (strncmp(t, ".in", 3) == 0) {
    char *a = (char *)t+3;
    do_indentation(a);
  } else if (strncmp(t, ".ti", 3) == 0) {
    char *a = (char *)t+3;
    do_tempindent(a);
  } else if (strncmp(t, ".vs", 3) == 0) {
    char *a = (char *)t+3;
    do_verticalspacing(a);
  } else if (strncmp(t, ".ps", 3) == 0) {
    char *a = (char *)t+3;
    do_pointsize(a);
  } else if (strcmp(t, ".links") == 0) {
    do_links();
  } else if (strcmp(t, ".no-auto-rule") == 0) {
    auto_rule = FALSE;
  } else if (strcmp(t, ".tab-ts") == 0) {
    do_tab_ts(g);
  } else if (strcmp(t, ".tab-te") == 0) {
    do_tab_te();
  } else if (strncmp(t, ".col ", 5) == 0) {
    char *a = (char *)t+4;
    do_col(a);
  } else if (strncmp(t, "tab ", 4) == 0) {
    char *a = (char *)t+3;
    do_tab(a);
  } else if (strncmp(t, "tab0", 4) == 0) {
    do_tab0();
  }
}

/*
 *  is_in_middle - returns TRUE if the positions left..right are in the center of the page.
 */

int html_printer::is_in_middle (int left, int right)
{
  return( abs(abs(left-pageoffset) - abs(pageoffset+linelength-right)) <= CENTER_TOLERANCE );
}

/*
 *  flush_globs - runs through the text glob list and emits html.
 */

void html_printer::flush_globs (void)
{
  text_glob *g;

  if (! page_contents->glyphs.is_empty()) {
    page_contents->glyphs.start_from_head();
    do {
      g = page_contents->glyphs.get_data();

      if (strcmp(g->text_string, "XXXXXXX") == 0)
	stop();

      if (g->is_a_tag()) {
	troff_tag(g);
      } else if (g->is_a_line()) {
	emit_line(g);
      } else {
	emit_html(g);
      }
      /*
       *  after processing the title (and removing it) the glyph list might be empty
       */
      if (! page_contents->glyphs.is_empty()) {
	page_contents->glyphs.move_right();
      }
    } while (! page_contents->glyphs.is_equal_to_head());
  }
}

/*
 *  calc_nf - calculates the _no_ format flag, given the
 *            text glob, g.
 */

int html_printer::calc_nf (text_glob *g, int nf)
{
  if (g != NULL) {
    if (g->is_fi())
      return FALSE;
    if (g->is_nf())
      return TRUE;
  }
  return nf;
}

/*
 *  calc_po_in - calculates the, in, po, registers
 */

void html_printer::calc_po_in (text_glob *g, int nf)
{
  if (g->is_in())
    indentation = g->get_arg();
  else if (g->is_po())
    pageoffset = g->get_arg();
  else if (g->is_ti()) {
    prev_indent = indentation;
    indentation = g->get_arg();
    end_tempindent = 1;
  } else if (g->is_br() && ((end_tempindent > 0) || (nf && g->is_eol()))) {
    end_tempindent = 0;
    indentation = prev_indent;
  }
}

/*
 *  next_horiz_pos - returns the next horiz position.
 *                   -1 is returned if it doesn't exist.
 */

int html_printer::next_horiz_pos (text_glob *g, int nf)
{
  int next     = -1;

  if ((g != NULL) && (g->is_br() || (nf && g->is_eol())))
    if (! page_contents->glyphs.is_empty()) {
      page_contents->glyphs.move_right_get_data();
      if (g == NULL)
	page_contents->glyphs.start_from_head();
      else {
	next = g->minh;
	page_contents->glyphs.move_left();
      }
    }
  return next;
}

/*
 *  insert_tab_ts - inserts a tab-ts before, where.
 */

text_glob *html_printer::insert_tab_ts (text_glob *where)
{
  text_glob *start_of_table;
  text_glob *old_pos = page_contents->glyphs.get_data();

  page_contents->glyphs.move_to(where);
  page_contents->glyphs.move_left();
  page_contents->insert_tag(string("html-tag:.tab-ts"));  // tab table start
  page_contents->glyphs.move_right();
  start_of_table = page_contents->glyphs.get_data();
  page_contents->glyphs.move_to(old_pos);
  return start_of_table;
}

/*
 *  insert_tab_te - inserts a tab-te before the current position
 *                  (it skips backwards over .sp/.br)
 */

void html_printer::insert_tab_te (void)
{
  text_glob *g = page_contents->glyphs.get_data();
  page_contents->dump_page();

  while (page_contents->glyphs.get_data()->is_a_tag())
    page_contents->glyphs.move_left();

  page_contents->insert_tag(string("html-tag:.tab-te"));  // tab table end
  while (g != page_contents->glyphs.get_data())
    page_contents->glyphs.move_right();
  page_contents->dump_page();
}

/*
 *  insert_tab_0 - inserts a tab0 before, where.
 */

void html_printer::insert_tab_0 (text_glob *where)
{
  text_glob *old_pos = page_contents->glyphs.get_data();

  page_contents->glyphs.move_to(where);
  page_contents->glyphs.move_left();
  page_contents->insert_tag(string("html-tag:tab0"));  // tab0 start of line
  page_contents->glyphs.move_right();
  page_contents->glyphs.move_to(old_pos);
}

/*
 *  remove_tabs - removes the tabs tags on this line.
 */

void html_printer::remove_tabs (void)
{
  text_glob *orig = page_contents->glyphs.get_data();
  text_glob *g;

  if (! page_contents->glyphs.is_equal_to_tail()) {
    do {
      g = page_contents->glyphs.get_data();
      if (g->is_tab()) {
	page_contents->glyphs.sub_move_right();
	if (g == orig)
	  orig = page_contents->glyphs.get_data();
      }	else
	page_contents->glyphs.move_right();
    } while ((! page_contents->glyphs.is_equal_to_head()) &&
	     (! g->is_eol()));
    
    /*
     *  now restore our previous position.
     */
    while (page_contents->glyphs.get_data() != orig)
      page_contents->glyphs.move_left();
  }
}

void html_printer::remove_courier_tabs (void)
{
  text_glob  *g;
  int line_start = TRUE;
  int nf         = FALSE;

  if (! page_contents->glyphs.is_empty()) {
    page_contents->glyphs.start_from_head();
    line_start = TRUE;
    do {
      g = page_contents->glyphs.get_data();
      
      nf = calc_nf(g, nf);

      if (line_start) {
	if (line_start && nf && is_courier_until_eol()) {
	  remove_tabs();
	  g = page_contents->glyphs.get_data();
	}
      }

      line_start = g->is_br() || g->is_nf() || g->is_fi() || (nf && g->is_eol());
      page_contents->glyphs.move_right();
    } while (! page_contents->glyphs.is_equal_to_head());
  }
}

void html_printer::insert_tab0_foreach_tab (void)
{
  text_glob  *start_of_line  = NULL;
  text_glob  *g              = NULL;
  int seen_tab               = FALSE;
  int seen_col               = FALSE;
  int nf                     = FALSE;

  if (! page_contents->glyphs.is_empty()) {
    page_contents->glyphs.start_from_head();
    start_of_line = page_contents->glyphs.get_data();
    do {
      g = page_contents->glyphs.get_data();
      
      nf = calc_nf(g, nf);

      if (g->is_tab())
	seen_tab = TRUE;
      
      if (g->is_col())
	seen_col = TRUE;

      if (g->is_br() || (nf && g->is_eol())) {
	do {
	  page_contents->glyphs.move_right();
	  g = page_contents->glyphs.get_data();
	  nf = calc_nf(g, nf);
	  if (page_contents->glyphs.is_equal_to_head()) {
	    if (seen_tab && !seen_col)
	      insert_tab_0(start_of_line);
	    return;
	  }
	} while (g->is_br() || (nf && g->is_eol()) || g->is_ta());
	// printf("\nstart_of_line is: %s\n", g->text_string);
	if (seen_tab && !seen_col) {
	  insert_tab_0(start_of_line);
	  page_contents->glyphs.move_to(g);
	}

	seen_tab = FALSE;
	seen_col = FALSE;
	start_of_line = g;
      }
      page_contents->glyphs.move_right();
    } while (! page_contents->glyphs.is_equal_to_head());
    if (seen_tab && !seen_col)
      insert_tab_0(start_of_line);

  }
}

/*
 *  update_min_max - updates the extent of a column, given the left and right
 *                   extents of a glyph, g.
 */

void html_printer::update_min_max (colType type_of_col, int *minimum, int *maximum, text_glob *g)
{
  switch (type_of_col) {
    
  case tab_tag:
    break;
  case tab0_tag:
    *minimum = g->minh;
    break;
  case col_tag:
    *minimum = g->minh;
    *maximum = g->maxh;
    break;
  default:
    break;
  }
}

/*
 *  add_table_end - moves left one glyph, adds a table end tag and adds a
 *                  debugging string.
 */

void html_printer::add_table_end (const char *
#if defined(DEBUG_TABLES)
  debug_string
#endif
)
{
  page_contents->glyphs.move_left();
  insert_tab_te();
#if defined(DEBUG_TABLES)
  page_contents->insert_tag(string(debug_string));
#endif
}

/*
 *  lookahead_for_tables - checks for .col tags and inserts table start/end tags
 */

void html_printer::lookahead_for_tables (void)
{
  text_glob  *g;
  text_glob  *start_of_line  = NULL;
  text_glob  *start_of_table = NULL;
  text_glob  *last           = NULL;
  colType     type_of_col    = none;
  int         left           = 0;
  int         found_col      = FALSE;
  int         seen_text      = FALSE;
  int         ncol           = 0;
  int         colmin;
  int         colmax;
  html_table *table          = new html_table(&html, -1);
  const char *tab_defs       = NULL;
  char        align          = 'L';
  int         nf             = FALSE;
  int         old_pageoffset = pageoffset;

  remove_courier_tabs();
  page_contents->dump_page();
  insert_tab0_foreach_tab();
  page_contents->dump_page();
  if (! page_contents->glyphs.is_empty()) {
    page_contents->glyphs.start_from_head();
    g = page_contents->glyphs.get_data();
    do {
#if defined(DEBUG_TABLES)
      fprintf(stderr, " [") ;
      fprintf(stderr, g->text_string) ;
      fprintf(stderr, "] ") ;
      fflush(stderr);
      if (strcmp(g->text_string, "XXXXXXX") == 0)
	stop();
#endif

      nf = calc_nf(g, nf);
      calc_po_in(g, nf);
      if (g->is_col()) {
	if (type_of_col == tab_tag && start_of_table != NULL) {
	  page_contents->glyphs.move_left();
	  insert_tab_te();
	  start_of_table->remember_table(table);
	  table = new html_table(&html, -1);
	  page_contents->insert_tag(string("*** TAB -> COL ***"));
	  if (tab_defs != NULL)
	    table->tab_stops->init(tab_defs);
	  start_of_table = NULL;
	  last = NULL;
	}
	type_of_col = col_tag;
	found_col = TRUE;
	ncol = g->get_arg();
	align = 'L';
	colmin = 0;
	colmax = 0;
      } else if (g->is_tab()) {
	type_of_col = tab_tag;
	colmin = g->get_tab_args(&align);
	align = 'L'; // for now as 'C' and 'R' are broken
	ncol = table->find_tab_column(colmin);
	colmin += pageoffset + indentation;
	colmax = table->get_tab_pos(ncol+1);
	if (colmax > 0)
	  colmax += pageoffset + indentation;
      } else if (g->is_tab0()) {
	if (type_of_col == col_tag && start_of_table != NULL) {
	  page_contents->glyphs.move_left();
	  insert_tab_te();
	  start_of_table->remember_table(table);
	  table = new html_table(&html, -1);
	  page_contents->insert_tag(string("*** COL -> TAB ***"));
	  start_of_table = NULL;
	  last = NULL;
	}
	if (tab_defs != NULL)
	  table->tab_stops->init(tab_defs);

	type_of_col = tab0_tag;
	ncol = 1;
	colmin = 0;
	colmax = table->get_tab_pos(2) + pageoffset + indentation;
      } else if (! g->is_a_tag())
	update_min_max(type_of_col, &colmin, &colmax, g);

      if ((! g->is_a_tag()) || g->is_tab())
	seen_text = TRUE;

      if ((g->is_col() || g->is_tab() || g->is_tab0())
	  && (start_of_line != NULL) && (start_of_table == NULL)) {
	start_of_table = insert_tab_ts(start_of_line);
	start_of_line = NULL;
	seen_text = FALSE;
      } else if (g->is_ce() && (start_of_table != NULL)) {
	add_table_end("*** CE ***");
	start_of_table->remember_table(table);
	start_of_table = NULL;
	last = NULL;
      } else if (g->is_ta()) {
	tab_defs = g->text_string;
	if (!table->tab_stops->compatible(tab_defs)) {
	  if (start_of_table != NULL) {
	    add_table_end("*** TABS ***");
	    start_of_table->remember_table(table);
	    table = new html_table(&html, -1);
	    start_of_table = NULL;
	    type_of_col = none;
	    last = NULL;
	  }
	  table->tab_stops->init(tab_defs);
	}
      }

      if (((! g->is_a_tag()) || g->is_tab()) && (start_of_table != NULL)) {
	// we are in a table and have a glyph
	if ((ncol == 0) || (! table->add_column(ncol, colmin, colmax, align))) {
	  if (ncol == 0)
	    add_table_end("*** NCOL == 0 ***");
	  else
	    add_table_end("*** CROSSED COLS ***");

	  start_of_table->remember_table(table);
	  table = new html_table(&html, -1);
	  start_of_table = NULL;
	  type_of_col = none;
	  last = NULL;
	}
      }
      
      /*
       *  move onto next glob, check whether we are starting a new line
       */
      g = page_contents->glyphs.move_right_get_data();

      if (g == NULL) {
	if (found_col) {
	  page_contents->glyphs.start_from_head();
	  last = g;
	  found_col = FALSE;
	}
      } else if (g->is_br() || (nf && g->is_eol())) {
	do {
	  g = page_contents->glyphs.move_right_get_data();
	  nf = calc_nf(g, nf);
	} while ((g != NULL) && (g->is_br() || (nf && g->is_eol())));
	start_of_line = g;
	seen_text = FALSE;
	ncol = 0;
	left = next_horiz_pos(g, nf);
	if (found_col)
	  last = g;
	found_col = FALSE;
      }
    } while ((g != NULL) && (! page_contents->glyphs.is_equal_to_head()));

#if defined(DEBUG_TABLES)
    fprintf(stderr, "finished scanning for tables\n");
#endif

    page_contents->glyphs.start_from_head();
    if (start_of_table != NULL) {
      if (last != NULL)
	while (last != page_contents->glyphs.get_data())
	  page_contents->glyphs.move_left();

      insert_tab_te();
      start_of_table->remember_table(table);
      table = NULL;
      page_contents->insert_tag(string("*** LAST ***"));      
    }
  }
  if (table != NULL)
    delete table;

  // and reset the registers
  pageoffset = old_pageoffset;
  indentation = 0;
  prev_indent = 0;
  end_tempindent = 0;
}

void html_printer::flush_page (void)
{
  supress_sub_sup = TRUE;
  flush_sbuf();
  page_contents->dump_page();
  lookahead_for_tables();
  page_contents->dump_page();

  flush_globs();
  current_paragraph->done_para();
  
  // move onto a new page
  delete page_contents;
#if defined(DEBUG_TABLES)
  fprintf(stderr, "\n\n*** flushed page ***\n\n");

  html.simple_comment("new page called");
#endif
  page_contents = new page;
}

/*
 *  determine_space - works out whether we need to write a space.
 *                    If last glyph is ajoining then no space emitted.
 */

void html_printer::determine_space (text_glob *g)
{
  if (current_paragraph->is_in_pre()) {
    /*
     *  .nf has been specified
     */
    while (output_hpos < g->minh) {
      output_hpos += space_width;
      current_paragraph->emit_space();
    }
  } else {
    if ((output_vpos != g->minv) || (output_hpos < g->minh)) {
      current_paragraph->emit_space();
    }
  }
}

/*
 *  is_font_courier - returns TRUE if the font, f, is courier.
 */

int html_printer::is_font_courier (font *f)
{
  if (f != 0) {
    const char *fontname = f->get_name();

    return( (fontname != 0) && (fontname[0] == 'C') );
  }
  return( FALSE );
}

/*
 *  end_font - shuts down the font corresponding to fontname.
 */

void html_printer::end_font (const char *fontname)
{
  if (strcmp(fontname, "B") == 0) {
    current_paragraph->done_bold();
  } else if (strcmp(fontname, "I") == 0) {
    current_paragraph->done_italic();
  } else if (strcmp(fontname, "BI") == 0) {
    current_paragraph->done_bold();
    current_paragraph->done_italic();
  } else if (strcmp(fontname, "CR") == 0) {
    current_paragraph->done_tt();
  } else if (strcmp(fontname, "CI") == 0) {
    current_paragraph->done_italic();
    current_paragraph->done_tt();
  } else if (strcmp(fontname, "CB") == 0) {
    current_paragraph->done_bold();
    current_paragraph->done_tt();
  } else if (strcmp(fontname, "CBI") == 0) {
    current_paragraph->done_bold();
    current_paragraph->done_italic();
    current_paragraph->done_tt();
  }
}

/*
 *  start_font - starts the font corresponding to name.
 */

void html_printer::start_font (const char *fontname)
{
  if (strcmp(fontname, "R") == 0) {
    current_paragraph->done_bold();
    current_paragraph->done_italic();
    current_paragraph->done_tt();
  } else if (strcmp(fontname, "B") == 0) {
    current_paragraph->do_bold();
  } else if (strcmp(fontname, "I") == 0) {
    current_paragraph->do_italic();
  } else if (strcmp(fontname, "BI") == 0) {
    current_paragraph->do_bold();
    current_paragraph->do_italic();
  } else if (strcmp(fontname, "CR") == 0) {
    if ((! fill_on) && (is_courier_until_eol())) {
      current_paragraph->do_pre();
    }
    current_paragraph->do_tt();
  } else if (strcmp(fontname, "CI") == 0) {
    if ((! fill_on) && (is_courier_until_eol())) {
      current_paragraph->do_pre();
    }
    current_paragraph->do_tt();
    current_paragraph->do_italic();
  } else if (strcmp(fontname, "CB") == 0) {
    if ((! fill_on) && (is_courier_until_eol())) {
      current_paragraph->do_pre();
    }
    current_paragraph->do_tt();
    current_paragraph->do_bold();
  } else if (strcmp(fontname, "CBI") == 0) {
    if ((! fill_on) && (is_courier_until_eol())) {
      current_paragraph->do_pre();
    }
    current_paragraph->do_tt();
    current_paragraph->do_italic();
    current_paragraph->do_bold();
  }
}

/*
 *  start_size - from is old font size, to is the new font size.
 *               The html increase <big> and <small> decrease alters the
 *               font size by 20%. We try and map these onto glyph sizes.
 */

void html_printer::start_size (int from, int to)
{
  if (from < to) {
    while (from < to) {
      current_paragraph->do_big();
      from += SIZE_INCREMENT;
    }
  } else if (from > to) {
    while (from > to) {
      current_paragraph->do_small();
      from -= SIZE_INCREMENT;
    }
  }
}

/*
 *  do_font - checks to see whether we need to alter the html font.
 */

void html_printer::do_font (text_glob *g)
{
  /*
   *  check if the output_style.point_size has not been set yet
   *  this allow users to place .ps at the top of their troff files
   *  and grohtml can then treat the .ps value as the base font size (3)
   */
  if (output_style.point_size == -1) {
    output_style.point_size = pointsize;
  }

  if (g->text_style.f != output_style.f) {
    if (output_style.f != 0) {
      end_font(output_style.f->get_name());
    }
    output_style.f = g->text_style.f;
    if (output_style.f != 0) {
      start_font(output_style.f->get_name());
    }
  }
  if (output_style.point_size != g->text_style.point_size) {
    do_sup_or_sub(g);
    if ((output_style.point_size > 0) &&
	(g->text_style.point_size > 0)) {
      start_size(output_style.point_size, g->text_style.point_size);
    }
    if (g->text_style.point_size > 0) {
      output_style.point_size = g->text_style.point_size;
    }
  }
  if (output_style.col != g->text_style.col) {
    current_paragraph->done_color();
    output_style.col = g->text_style.col;
    current_paragraph->do_color(&output_style.col);
  }
}

/*
 *  start_subscript - returns TRUE if, g, looks like a subscript start.
 */

int html_printer::start_subscript (text_glob *g)
{
  int r        = font::res;
  int height   = output_style.point_size*r/72;

  return( (output_style.point_size != 0) &&
	  (output_vpos < g->minv) &&
	  (output_vpos-height > g->maxv) &&
	  (output_style.point_size > g->text_style.point_size) );
}

/*
 *  start_superscript - returns TRUE if, g, looks like a superscript start.
 */

int html_printer::start_superscript (text_glob *g)
{
  int r        = font::res;
  int height   = output_style.point_size*r/72;

  return( (output_style.point_size != 0) &&
	  (output_vpos > g->minv) &&
	  (output_vpos-height < g->maxv) &&
	  (output_style.point_size > g->text_style.point_size) );
}

/*
 *  end_subscript - returns TRUE if, g, looks like the end of a subscript.
 */

int html_printer::end_subscript (text_glob *g)
{
  int r        = font::res;
  int height   = output_style.point_size*r/72;

  return( (output_style.point_size != 0) &&
	  (g->minv < output_vpos) &&
	  (output_vpos-height > g->maxv) &&
	  (output_style.point_size < g->text_style.point_size) );
}

/*
 *  end_superscript - returns TRUE if, g, looks like the end of a superscript.
 */

int html_printer::end_superscript (text_glob *g)
{
  int r        = font::res;
  int height   = output_style.point_size*r/72;

  return( (output_style.point_size != 0) &&
	  (g->minv > output_vpos) &&
	  (output_vpos-height < g->maxv) &&
	  (output_style.point_size < g->text_style.point_size) );
}

/*
 *  do_sup_or_sub - checks to see whether the next glyph is a subscript/superscript
 *                  start/end and it calls the services of html-text to issue the
 *                  appropriate tags.
 */

void html_printer::do_sup_or_sub (text_glob *g)
{
  if (! supress_sub_sup) {
    if (start_subscript(g)) {
      current_paragraph->do_sub();
    } else if (start_superscript(g)) {
      current_paragraph->do_sup();
    } else if (end_subscript(g)) {
      current_paragraph->done_sub();
    } else if (end_superscript(g)) {
      current_paragraph->done_sup();
    }
  }
}

/*
 *  emit_html - write out the html text
 */

void html_printer::emit_html (text_glob *g)
{
  do_font(g);
  determine_space(g);
  current_paragraph->do_emittext(g->text_string, g->text_length);
  output_vpos     = g->minv;
  output_hpos     = g->maxh;
  output_vpos_max = g->maxv;
  supress_sub_sup = FALSE;
}

/*
 *  flush_sbuf - flushes the current sbuf into the list of glyphs.
 */

void html_printer::flush_sbuf()
{
  if (sbuf.length() > 0) {
    int r=font::res;   // resolution of the device
    set_style(sbuf_style);
    if (overstrike_detected && (! is_bold(sbuf_style.f))) {
      font *bold_font = make_bold(sbuf_style.f);
      if (bold_font != NULL)
	sbuf_style.f = bold_font;
    }

    page_contents->add(&sbuf_style, sbuf,
		       line_number,
		       sbuf_vpos-sbuf_style.point_size*r/72, sbuf_start_hpos,
		       sbuf_vpos                           , sbuf_end_hpos);
	     
    output_hpos = sbuf_end_hpos;
    output_vpos = sbuf_vpos;
    last_sbuf_length = 0;
    sbuf_prev_hpos = sbuf_end_hpos;
    overstrike_detected = FALSE;
    sbuf.clear();
  }
}

void html_printer::set_line_thickness(const environment *env)
{
  line_thickness = env->size;
}

void html_printer::draw(int code, int *p, int np, const environment *env)
{
  switch (code) {

  case 'l':
# if 0
    if (np == 2) {
      page_contents->add_line(&sbuf_style,
			      line_number,
			      env->hpos, env->vpos, env->hpos+p[0], env->vpos+p[1], line_thickness);
    } else {
      error("2 arguments required for line");
    }
# endif
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
    break;
  case 'p':
    break;
  case 'E':
    break;
  case 'e':
    break;
  case 'C':
    break;
  case 'c':
    break;
  case 'a':
    break;
  case '~':
    break;
  case 'f':
    break;
  case 'F':
    // fill with color env->fill
    if (background != NULL)
      delete background;
    background = new color;
    *background = *env->fill;
    break;

  default:
    error("unrecognised drawing command `%1'", char(code));
    break;
  }
}

html_printer::html_printer()
: html(0, MAX_LINE_LENGTH),
  no_of_printed_pages(0),
  last_sbuf_length(0),
  overstrike_detected(FALSE),
  output_hpos(-1),
  output_vpos(-1),
  output_vpos_max(-1),
  line_thickness(-1),
  inside_font_style(0),
  page_number(0),
  header_indent(-1),
  supress_sub_sup(TRUE),
  cutoff_heading(100),
  indent(NULL),
  table(NULL),
  end_center(0),
  end_tempindent(0),
  next_tag(INLINE),
  fill_on(TRUE),
  max_linelength(-1),
  linelength(0),
  pageoffset(0),
  indentation(0),
  prev_indent(0),
  pointsize(0),
  line_number(0),
  background(default_background)
{
  file_list.add_new_file(xtmpfile());
  html.set_file(file_list.get_file());
  if (font::hor != 24)
    fatal("horizontal resolution must be 24");
  if (font::vert != 40)
    fatal("vertical resolution must be 40");
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
  space_width       = font::hor;
  paper_length      = font::paperlength;
  linelength        = font::res*13/2;
  if (paper_length == 0)
    paper_length    = 11*font::res;

  page_contents = new page();
}

/*
 *  add_to_sbuf - adds character code or name to the sbuf.
 */

void html_printer::add_to_sbuf (int index, const string &s)
{
  if (sbuf_style.f == NULL)
    return;

  char *html_glyph = NULL;
  unsigned int code = sbuf_style.f->get_code(index);

  if (s.empty()) {
    if (sbuf_style.f->contains(index))
      html_glyph = (char *)sbuf_style.f->get_special_device_encoding(index);
    else
      html_glyph = NULL;
    
    if ((html_glyph == NULL) && (code >= UNICODE_DESC_START))
      html_glyph = to_unicode(code);
  } else 
    html_glyph = get_html_translation(sbuf_style.f, s);

  last_sbuf_length = sbuf.length();
  if (html_glyph == NULL)
    sbuf += ((char)code);
  else
    sbuf += html_glyph;
}

int html_printer::sbuf_continuation (int index, const char *name,
				     const environment *env, int w)
{
  /*
   *  lets see whether the glyph is closer to the end of sbuf
   */
  if ((sbuf_end_hpos == env->hpos)
      || ((sbuf_prev_hpos < sbuf_end_hpos)
	  && (env->hpos < sbuf_end_hpos)
	  && ((sbuf_end_hpos-env->hpos < env->hpos-sbuf_prev_hpos)))) {
    add_to_sbuf(index, name);
    sbuf_prev_hpos = sbuf_end_hpos;
    sbuf_end_hpos += w + sbuf_kern;
    return TRUE;
  } else {
    if ((env->hpos >= sbuf_end_hpos) &&
	((sbuf_kern == 0) || (sbuf_end_hpos - sbuf_kern != env->hpos))) {
      /*
       *  lets see whether a space is needed or not
       */

      if (env->hpos-sbuf_end_hpos < space_width) {
	add_to_sbuf(index, name);
	sbuf_prev_hpos = sbuf_end_hpos;
	sbuf_end_hpos = env->hpos + w;
	return TRUE;
      }
    }
  }
  return FALSE ;
}

/*
 *  get_html_translation - given the position of the character and its name
 *                         return the device encoding for such character.
 */

char *get_html_translation (font *f, const string &name)
{
  int index;

  if ((f == 0) || name.empty())
    return NULL;
  else {
    index = f->name_to_index((char *)(name + '\0').contents());
    if (index == 0) {
      error("character `%s' not found", (name + '\0').contents());
      return NULL;
    } else
      if (f->contains(index))
	return (char *)f->get_special_device_encoding(index);
      else
	return NULL;
  }
}

/*
 *  overstrike - returns TRUE if the glyph (i, name) is going to overstrike
 *               a previous glyph in sbuf.
 *               If TRUE the font is changed to bold and the previous sbuf
 *               is flushed.
 */

int html_printer::overstrike(int index, const char *name, const environment *env, int w)
{
  if ((env->hpos < sbuf_end_hpos)
      || ((sbuf_kern != 0) && (sbuf_end_hpos - sbuf_kern < env->hpos))) {
    /*
     *  at this point we have detected an overlap
     */
    if (overstrike_detected) {
      /* already detected, remove previous glyph and use this glyph */
      sbuf.set_length(last_sbuf_length);
      add_to_sbuf(index, name);
      sbuf_end_hpos = env->hpos + w;
      return TRUE;
    } else {
      /* first time we have detected an overstrike in the sbuf */
      sbuf.set_length(last_sbuf_length); /* remove previous glyph */
      if (! is_bold(sbuf_style.f))
	flush_sbuf();
      overstrike_detected = TRUE;
      add_to_sbuf(index, name);
      sbuf_end_hpos = env->hpos + w;
      return TRUE;
    }
  }
  return FALSE ;
}

/*
 *  set_char - adds a character into the sbuf if it is a continuation with the previous
 *             word otherwise flush the current sbuf and add character anew.
 */

void html_printer::set_char(int i, font *f, const environment *env, int w, const char *name)
{
  style sty(f, env->size, env->height, env->slant, env->fontno, *env->col);
  if (sty.slant != 0) {
    if (sty.slant > 80 || sty.slant < -80) {
      error("silly slant `%1' degrees", sty.slant);
      sty.slant = 0;
    }
  }
  if (((! sbuf.empty()) && (sty == sbuf_style) && (sbuf_vpos == env->vpos))
      && (sbuf_continuation(i, name, env, w) || overstrike(i, name, env, w)))
    return;
  
  flush_sbuf();
  add_to_sbuf(i, name);
  sbuf_end_hpos = env->hpos + w;
  sbuf_start_hpos = env->hpos;
  sbuf_prev_hpos = env->hpos;
  sbuf_vpos = env->vpos;
  sbuf_style = sty;
  sbuf_kern = 0;
}

/*
 *  set_numbered_char - handle numbered characters.
 *                      Negative values are interpreted as unbreakable spaces;
 *                      the value (taken positive) gives the width.
 */

void html_printer::set_numbered_char(int num, const environment *env,
				     int *widthp)
{
  int nbsp_width = 0;
  if (num < 0) {
    nbsp_width = -num;
    num = 160;		// &nbsp;
  }
  int i = font::number_to_index(num);
  int fn = env->fontno;
  if (fn < 0 || fn >= nfonts) {
    error("bad font position `%1'", fn);
    return;
  }
  font *f = font_table[fn];
  if (f == 0) {
    error("no font mounted at `%1'", fn);
    return;
  }
  if (!f->contains(i)) {
    error("font `%1' does not contain numbered character %2",
	  f->get_name(),
	  num);
    return;
  }
  int w;
  if (nbsp_width)
    w = nbsp_width;
  else
    w = f->get_width(i, env->size);
  if (widthp)
    *widthp = w;
  set_char(i, f, env, w, 0);
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
      html.put_string("</title>").nl().nl();
    } else {
      title.has_been_written = TRUE;
      if (title.with_h1) {
	html.put_string("<h1 align=center>");
	html.put_string(title.text);
	html.put_string("</h1>").nl().nl();
      }
    }
  } else if (in_head) {
    // place empty title tags to help conform to `tidy'
    html.put_string("<title></title>").nl();
  }
}

/*
 *  write_rule - emits a html rule tag, if the auto_rule boolean is true.
 */

static void write_rule (void)
{
  if (auto_rule)
    fputs("<hr>\n", stdout);
}

void html_printer::begin_page(int n)
{
  page_number            =  n;
#if defined(DEBUGGING)
  html.begin_comment("Page: ").put_string(i_to_a(page_number)).end_comment();;
#endif
  no_of_printed_pages++;

  output_style.f         =  0;
  output_style.point_size= -1;
  output_space_code      = 32;
  output_draw_point_size = -1;
  output_line_thickness  = -1;
  output_hpos            = -1;
  output_vpos            = -1;
  output_vpos_max        = -1;
  current_paragraph      = new html_text(&html);
  do_indent(indentation, pageoffset, linelength);
  current_paragraph->do_para("");
}

void html_printer::end_page(int)
{
  flush_sbuf();
  flush_page();
}

font *html_printer::make_font(const char *nm)
{
  return html_font::load_html_font(nm);
}

void html_printer::do_body (void)
{
  if (background == NULL)
    fputs("<body>\n\n", stdout);
  else {
    unsigned int r, g, b;
    char buf[6+1];

    background->get_rgb(&r, &g, &b);
    // we have to scale 0..0xFFFF to 0..0xFF
    sprintf(buf, "%.2X%.2X%.2X", r/0x101, g/0x101, b/0x101);

    fputs("<body bgcolor=\"#", stdout);
    fputs(buf, stdout);
    fputs("\">\n\n", stdout);
  }
}

html_printer::~html_printer()
{
#ifdef LONG_FOR_TIME_T
  long t;
#else
  time_t t;
#endif

  current_paragraph->flush_text();
  html.end_line();
  html.set_file(stdout);
  html.begin_comment("Creator     : ")
    .put_string("groff ")
    .put_string("version ")
    .put_string(Version_string)
    .end_comment();

  t = time(0);
  html.begin_comment("CreationDate: ")
    .put_string(ctime(&t), strlen(ctime(&t))-1)
    .end_comment();

  fputs("<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\"\n", stdout);
  fputs("\"http://www.w3.org/TR/html4/loose.dtd\">\n", stdout);

  fputs("<html>\n", stdout);
  fputs("<head>\n", stdout);
  fputs("<meta name=\"generator\" "
	      "content=\"groff -Thtml, see www.gnu.org\">\n", stdout);
  fputs("<meta http-equiv=\"Content-Type\" "
	      "content=\"text/html; charset=US-ASCII\">\n", stdout);
  fputs("<meta name=\"Content-Style\" content=\"text/css\">\n", stdout);
  write_title(TRUE);
  fputs("</head>\n", stdout);
  do_body();

  write_title(FALSE);
  header.write_headings(stdout, FALSE);
  write_rule();
#if defined(DEBUGGING)
  html.begin_comment("Total number of pages: ").put_string(i_to_a(no_of_printed_pages)).end_comment();
#endif
  html.end_line();
  html.end_line();
  /*
   *  now run through the file list copying each temporary file in turn and emitting the links.
   */
  file_list.start_of_list();
  while (file_list.get_file() != 0) {
    if (fseek(file_list.get_file(), 0L, 0) < 0)
      fatal("fseek on temporary file failed");
    html.copy_file(file_list.get_file());
    fclose(file_list.get_file());
    file_list.move_next();
    if (file_list.get_file() != 0)
      header.write_headings(stdout, TRUE);
  }
  write_rule();
  fputs("</body>\n", stdout);
  fputs("</html>\n", stdout);
}

/*
 *  special - handle all x X requests from troff. For post-html they allow users
 *            to pass raw html commands, turn auto linked headings off/on and
 *            also allow troff to emit tags to indicate when a: .br, .sp etc occurs.
 */

void html_printer::special(char *s, const environment *env, char type)
{
  if (type != 'p')
    return;
  if (s != 0) {
    flush_sbuf();
    if (env->fontno >= 0) {
      style sty(get_font_from_index(env->fontno), env->size, env->height,
		env->slant, env->fontno, *env->col);
      sbuf_style = sty;
    }

    if (strncmp(s, "html:", 5) == 0) {
      int r=font::res;   /* resolution of the device */
      font *f=sbuf_style.f;

      if (f == NULL) {
	int found=FALSE;

	f = font::load_font("TR", &found);
      }

      /*
       *  need to pass rest of string through to html output during flush
       */
      page_contents->add_and_encode(&sbuf_style, string(&s[5]),
				    line_number,
				    env->vpos-env->size*r/72, env->hpos,
				    env->vpos               , env->hpos);

      /*
       * assume that the html command has no width, if it does then hopefully troff
       * will have fudged this in a macro by requesting that the formatting move right by
       * the appropriate amount.
       */
    } else if (strncmp(s, "index:", 6) == 0) {
      cutoff_heading = atoi(&s[6]);
    } else if (strncmp(s, "html-tag:", 9) == 0) {
      int r=font::res;   /* resolution of the device */

      page_contents->add_tag(&sbuf_style, string(s),
			     line_number,
			     env->vpos-env->size*r/72, env->hpos,
			     env->vpos               , env->hpos);
    }
  }
}

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
  while ((c = getopt_long(argc, argv, "a:g:o:i:I:D:F:vbdhlrnp", long_options, NULL))
	 != EOF)
    switch(c) {
    case 'v':
      printf("GNU post-grohtml (groff) version %s\n", Version_string);
      exit(0);
      break;
    case 'a':
      /* text antialiasing bits - handled by pre-html */
      break;
    case 'g':
      /* graphic antialiasing bits - handled by pre-html */
      break;
    case 'b':
      // set background color to white
      default_background = new color;
      default_background->set_gray(color::MAX_COLOR_VAL);
      break;
    case 'F':
      font::command_line_font_dir(optarg);
      break;
    case 'l':
      auto_links = FALSE;
      break;
    case 'r':
      auto_rule = FALSE;
      break;
    case 'd':
      /* handled by pre-html */
      break;
    case 'h':
      /* do not use the Hn headings of html, but manufacture our own */
      manufacture_headings = TRUE;
      break;
    case 'o':
      /* handled by pre-html */
      break;
    case 'p':
      /* handled by pre-html */
      break;
    case 'i':
      /* handled by pre-html */
      break;
    case 'I':
      /* handled by pre-html */
      break;
    case 'D':
      /* handled by pre-html */
      break;
    case 'n':
      simple_anchors = TRUE;
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
  if (optind >= argc) {
    do_file("-");
  } else {
    for (int i = optind; i < argc; i++)
      do_file(argv[i]);
  }
  return 0;
}

static void usage(FILE *stream)
{
  fprintf(stream, "usage: %s [-vblnh] [-D dir] [-I image_stem] [-F dir] [files ...]\n",
	  program_name);
}
