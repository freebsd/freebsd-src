// -*- C++ -*-
/* Copyright (C) 2000, 2001 Free Software Foundation, Inc.
 *
 *  Gaius Mulley (gaius@glam.ac.uk) wrote post-html.cc
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
#include "html-text.h"

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

#define MAX_STRING_LENGTH            4096
#define MAX_LINE_LENGTH                60            /* maximum characters we want in a line      */
#define SIZE_INCREMENT                  2            /* font size increment <big> = +2            */
#define BASE_POINT_SIZE                10            /* 10 points is the base size ie html size 3 */
#define CENTER_TOLERANCE                2            /* how many pixels off center will we still  */
#define ANCHOR_TEMPLATE       "heading%d"            /* if simple anchor is set we use this       */
#define UNICODE_DESC_START           0x80            /* all character entities above this are     */
                                                     /* either encoded by their glyph names or if */
                                                     /* there is no name then we use &#nnn;       */
#define INDENTATION                                  /* #undef INDENTATION to remove .in handling */

typedef enum {CENTERED, LEFT, RIGHT, INLINE} TAG_ALIGNMENT;

/*
 *  prototypes
 */

void str_translate_to_html (font *f, char *buf, int buflen, char *str, int len, int and_single);
char *get_html_translation (font *f, const char *name);


static int auto_links = TRUE;                        /* by default we enable automatic links at  */
                                                     /* top of the document.                     */
static int auto_rule  = TRUE;                        /* by default we enable an automatic rule   */
                                                     /* at the top and bottom of the document    */
static int simple_anchors = FALSE;                   /* default to anchors with heading text     */


/*
 *  start with a few favorites
 */

void stop () {}

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

#if 0
static int is_subsection (int a1, int a2, int b1, int b2)
{
  // easier to see whether this is not the case
  return( !((a1 < b1) || (a1 > b2) || (a2 < b1) || (a2 > b2)) );
}
#endif

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
 *  the classes and methods for maintaining glyph positions.
 */

class text_glob {
public:
  text_glob  (style *s, char *string, unsigned int length,
	      int min_vertical , int min_horizontal,
	      int max_vertical , int max_horizontal,
	      int is_html      , int is_troff_command,
	      int is_auto_image,
	      int is_a_line    , int thickness);
  text_glob  (void);
  ~text_glob (void);
  int         is_a_line   (void);
  int         is_a_tag    (void);
  int         is_raw      (void);
  int         is_eol      (void);
  int         is_auto_img (void);
  int         is_br       (void);

  style           text_style;
  char           *text_string;
  unsigned int    text_length;
  int             minv, minh, maxv, maxh;
  int             is_raw_command;       // should the text be sent directly to the device?
  int             is_tag;               // is this a .br, .sp, .tl etc
  int             is_img_auto;          // image created by eqn delim
  int             is_line;              // is the command a <line>?
  int             thickness;            // the thickness of a line
};

text_glob::text_glob (style *s, char *string, unsigned int length,
		      int min_vertical, int min_horizontal,
		      int max_vertical, int max_horizontal,
		      int is_html, int is_troff_command,
		      int is_auto_image,
		      int is_a_line, int line_thickness)
  : text_style(*s), text_string(string), text_length(length),
    minv(min_vertical), minh(min_horizontal), maxv(max_vertical), maxh(max_horizontal),
    is_raw_command(is_html), is_tag(is_troff_command), is_img_auto(is_auto_image),
    is_line(is_a_line), thickness(line_thickness)
{
}

text_glob::text_glob ()
  : text_string(0), text_length(0), minv(-1), minh(-1), maxv(-1), maxh(-1),
    is_raw_command(FALSE), is_tag(FALSE), is_line(FALSE), thickness(0)
{
}

text_glob::~text_glob ()
{
}

/*
 *  is_a_line - returns TRUE if glob should be converted into an <hr>
 */

int text_glob::is_a_line (void)
{
  return( is_line );
}

/*
 *  is_a_tag - returns TRUE if glob contains a troff directive.
 */

int text_glob::is_a_tag (void)
{
  return( is_tag );
}

/*
 *  is_eol - returns TRUE if glob contains the tag eol
 */

int text_glob::is_eol (void)
{
  return( is_tag && (strcmp(text_string, "html-tag:eol") == 0) );
}

/*
 *  is_raw - returns TRUE if glob contains raw html.
 */

int text_glob::is_raw (void)
{
  return( is_raw_command );
}

/*
 *  is_auto_img - returns TRUE if the glob contains an automatically
 *                generated image.
 */

int text_glob::is_auto_img (void)
{
  return( is_img_auto );
}

/*
 *  is_br - returns TRUE if the glob is a tag containing a .br
 */

int text_glob::is_br (void)
{
  return( is_a_tag() && (strcmp("html-tag:.br", text_string) == 0) );
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
  : head(0), tail(0), ptr(0)
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
    if (temp != 0) {
      head = head->right;
      delete temp;
    }
  } while ((head != 0) && (head != tail));
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
 *                        ptr right.
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
 *  page class and methods
 */

class page {
public:
                              page            (void);
  void                        add             (style *s, char *string, unsigned int length,
					       int line_number,
					       int min_vertical, int min_horizontal,
					       int max_vertical, int max_horizontal);
  void                        add_html        (style *s, char *string, unsigned int length,
					       int line_number,
					       int min_vertical, int min_horizontal,
					       int max_vertical, int max_horizontal);
  void                        add_tag         (style *s, char *string, unsigned int length,
					       int line_number,
					       int min_vertical, int min_horizontal,
					       int max_vertical, int max_horizontal);
  void                        add_line        (style *s,
					       int line_number,
					       int x1, int y1, int x2, int y2,
					       int thickness);
  void                        dump_page       (void);   // debugging method

  // and the data

  list                        glyphs;         // position of glyphs and specials on page
  char_buffer                 buffer;         // all characters for this page
};

page::page()
{
}

void page::add (style *s, char *string, unsigned int length,
		int line_number,
		int min_vertical, int min_horizontal,
		int max_vertical, int max_horizontal)
{
  if (length > 0) {
    text_glob *g=new text_glob(s, buffer.add_string(string, length), length,
			       min_vertical, min_horizontal, max_vertical, max_horizontal,
			       FALSE, FALSE, FALSE, FALSE, 0);
    glyphs.add(g, line_number, min_vertical, min_horizontal, max_vertical, max_horizontal);
  }
}

/*
 *  add_html - add a raw html command, for example mailto, line, background, image etc.
 */

void page::add_html (style *s, char *string, unsigned int length,
		     int line_number,
		     int min_vertical, int min_horizontal,
		     int max_vertical, int max_horizontal)
{
  if (length > 0) {
    text_glob *g=new text_glob(s, buffer.add_string(string, length), length,
			       min_vertical, min_horizontal, max_vertical, max_horizontal,
			       TRUE, FALSE, FALSE, FALSE, 0);
    glyphs.add(g, line_number, min_vertical, min_horizontal, max_vertical, max_horizontal);
  }
}

/*
 *  add_tag - adds a troff tag, for example: .tl .sp .br
 */

void page::add_tag (style *s, char *string, unsigned int length,
		    int line_number,
		    int min_vertical, int min_horizontal,
		    int max_vertical, int max_horizontal)
{
  if (length > 0) {
    text_glob *g=new text_glob(s, buffer.add_string(string, length), length,
			       min_vertical, min_horizontal, max_vertical, max_horizontal,
			       FALSE, TRUE,
			       (strncmp(string, "html-tag:.auto-image", 20) == 0),
			       FALSE, 0);
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
    text_glob *g = new text_glob(s, "", 0,
				 min(y1, y2), min(x1, y2), max(y1, y2), max(x1, x2),
				 FALSE, TRUE, FALSE, FALSE, thickness);
    glyphs.add(g, line_number, min(y1, y2), min(x1, y2), max(y1, y2), max(x1, x2));
  }
}

/*
 *  dump_page - dump the page contents for debugging purposes.
 */

void page::dump_page(void)
{
  text_glob *g;

  printf("\n\ndebugging start\n");
  glyphs.start_from_head();
  do {
    g = glyphs.get_data();
    printf("%s ", g->text_string);
    glyphs.move_right();
  } while (! glyphs.is_equal_to_head());
  printf("\ndebugging end\n\n");
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
  list                      headers;             // list of headers built from .NH and .SH
  int                       header_level;        // current header level
  int                       written_header;      // have we written the header yet?
  char                      header_buffer[MAX_STRING_LENGTH];  // current header text

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
	if (simple_anchors)
	  fprintf(f, ANCHOR_TEMPLATE, h);
	else
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
  style                output_style;
  int                  output_hpos;
  int                  output_vpos;
  int                  output_vpos_max;
  int                  output_draw_point_size;
  int                  line_thickness;
  int                  output_line_thickness;
  unsigned char        output_space_code;
  string               defs;
  char                *inside_font_style;
  int                  page_number;
  title_desc           title;
  title_desc           indent;  // use title class to remember $1 of .ip
  header_desc          header;
  int                  header_indent;
  int                  supress_sub_sup;
  int                  cutoff_heading;
  page                *page_contents;
  html_text           *current_paragraph;
  int                  end_center;
  int                  end_tempindent;
  TAG_ALIGNMENT        next_tag;
  int                  fill_on;
  int                  linelength;
  int                  pageoffset;
  int                  indentation;
  int                  prev_indent;
  int                  pointsize;
  int                  vertical_spacing;
  int                  line_number;

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
  void  add_char_to_sbuf              (unsigned char code);
  void  add_to_sbuf                   (unsigned char code, const char *name);
  void  write_title                   (int in_head);
  void  determine_diacritical_mark    (const char *name, const environment *env);
  int   sbuf_continuation             (unsigned char code, const char *name, const environment *env, int w);
  char *remove_last_char_from_sbuf    ();
  int   seen_backwards_escape         (char *s, int l);
  void  flush_page                    (void);
  void  troff_tag                     (text_glob *g);
  void  flush_globs                   (void);
  void  emit_line                     (text_glob *g);
  void  emit_raw                      (text_glob *g);
  void  translate_to_html             (text_glob *g);
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

  // ADD HERE

public:
  html_printer     ();
  ~html_printer    ();
  void set_char    (int i, font *f, const environment *env, int w, const char *name);
  void draw        (int code, int *p, int np, const environment *env);
  void begin_page  (int);
  void end_page    (int);
  void special     (char *arg, const environment *env, char type);
  font *make_font  (const char *);
  void end_of_line ();
};

printer *make_printer()
{
  return new html_printer;
}

static void usage(FILE *stream);

void html_printer::set_style(const style &sty)
{
  const char *fontname = sty.f->get_name();
  if (fontname == 0)
    fatal("no internalname specified for font");

#if 0
  change_font(fontname, (font::res/(72*font::sizescale))*sty.point_size);
#endif
}

void html_printer::end_of_line()
{
  flush_sbuf();
  line_number++;
}

/*
 *  emit_line - writes out a horizontal rule.
 */

void html_printer::emit_line (text_glob *g)
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
    int in_table=current_paragraph->is_in_table();

    current_paragraph->done_para();
    switch (next_tag) {

    case CENTERED:
      current_paragraph->do_para("align=center");
      break;
    case LEFT:
      current_paragraph->do_para("align=left");
      break;
    case RIGHT:
      current_paragraph->do_para("align=right");
      break;
    default:
      fatal("unknown enumeration");
    }
    current_paragraph->do_emittext(g->text_string, g->text_length);
    current_paragraph->done_para();
    next_tag        = INLINE;
    supress_sub_sup = TRUE;
#if defined(INDENTATION)
    if (in_table) {
      stop();
      current_paragraph->do_indent(NULL, 0, pageoffset, linelength);
      current_paragraph->do_indent(indent.text, indentation, pageoffset, linelength);
    }
#endif
  }
}

/*
 *  do_center - handle the .ce commands from troff.
 */

void html_printer::do_center (char *arg)
{
  int n = atoi(arg);

  current_paragraph->do_break();
  current_paragraph->done_para();
  supress_sub_sup = TRUE;
    
  if (n > 0) {
    current_paragraph->do_para("align=center");
    end_center += n;
  } else {
    end_center = 0;
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

static char *generate_img_src (const char *filename)
{
  static char buffer[MAX_STRING_LENGTH];

  while (filename && (filename[0] == ' ')) {
    filename++;
  }
  if (exists(filename)) {
    strcpy(buffer, "<img src=\"");
    strncat(buffer, filename, MAX_STRING_LENGTH-strlen("<img src=\"")-1);
    if (strlen(buffer) < MAX_STRING_LENGTH-3) {
      strncat(buffer, "\">", 3);
    }
    return( (char *)&buffer );
  } else {
    return( 0 );
  }
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
  char *buffer = generate_img_src(filename);
  
  if (buffer) {
    /*
     *  utilize emit_raw by creating a new text_glob.
     */
    text_glob h = *g;

    h.text_string = buffer;
    h.text_length = strlen(buffer);
    emit_raw(&h);
  } else {
    next_tag = INLINE;
  }
}

/*
 *  do_title - handle the .tl commands from troff.
 */

void html_printer::do_title (void)
{
  text_glob    *t;
  int           removed_from_head;
  char          buf[MAX_STRING_LENGTH];

  if (page_number == 1) {
    int found_title_start  = FALSE;
    if (! page_contents->glyphs.is_empty()) {
      page_contents->glyphs.sub_move_right();       /* move onto next word */
      do {
	t = page_contents->glyphs.get_data();
	removed_from_head = FALSE;
	if (t->is_auto_img()) {
	  char *img=generate_img_src((char *)(t->text_string + 20));

	  if (img) {
	    if (found_title_start) {
	      strcat(title.text, " ");
	    }
	    found_title_start = TRUE;
	    title.has_been_found = TRUE;
	    strcat(title.text, img);
	  }
	  page_contents->glyphs.sub_move_right(); 	  /* move onto next word */
	  removed_from_head = ((!page_contents->glyphs.is_empty()) &&
			       (page_contents->glyphs.is_equal_to_head()));
	} else if (t->is_raw_command) {
	  /* skip raw commands
	   */
	  page_contents->glyphs.sub_move_right(); 	  /* move onto next word */
	} else if (t->is_eol()) {
	  /* end of title found
	   */
	  title.has_been_found = TRUE;
	  return;
	} else if (t->is_a_tag()) {
	  /* end of title found, but move back so that we read this tag and process it
	   */
	  page_contents->glyphs.move_left();           /* move backwards to last word */
	  title.has_been_found = TRUE;
	  return;
	} else if (found_title_start) {
	    strcat(title.text, " ");
	    str_translate_to_html(t->text_style.f, buf, MAX_STRING_LENGTH, t->text_string, t->text_length, TRUE);
	    strcat(title.text, buf);
	    page_contents->glyphs.sub_move_right(); 	  /* move onto next word */
	    removed_from_head = ((!page_contents->glyphs.is_empty()) &&
				 (page_contents->glyphs.is_equal_to_head()));
	} else {
	  str_translate_to_html(t->text_style.f, buf, MAX_STRING_LENGTH, t->text_string, t->text_length, TRUE);
	  strcpy((char *)title.text, buf);
	  found_title_start    = TRUE;
	  title.has_been_found = TRUE;
	  page_contents->glyphs.sub_move_right(); 	  /* move onto next word */
	  removed_from_head = ((!page_contents->glyphs.is_empty()) &&
			       (page_contents->glyphs.is_equal_to_head()));
	}
      } while ((! page_contents->glyphs.is_equal_to_head()) || (removed_from_head));
    }
    // page_contents->glyphs.move_left();           /* move backwards to last word */
  }
}

void html_printer::write_header (void)
{
  if (strlen(header.header_buffer) > 0) {
    if (header.header_level > 7) {
      header.header_level = 7;
    }

    // firstly we must terminate any font and type faces
    current_paragraph->done_para();
    current_paragraph->done_table();
    supress_sub_sup = TRUE;

    if (cutoff_heading+2 > header.header_level) {
      // now we save the header so we can issue a list of links
      header.no_of_headings++;
      style st;

      text_glob *h=new text_glob(&st,
				 header.headings.add_string(header.header_buffer, strlen(header.header_buffer)),
				 strlen(header.header_buffer),
				 header.no_of_headings, header.header_level,
				 header.no_of_headings, header.header_level,
				 FALSE, FALSE, FALSE, FALSE, FALSE);
      header.headers.add(h,
			 header.no_of_headings,
			 header.no_of_headings, header.no_of_headings,
			 header.no_of_headings, header.no_of_headings);   // and add this header to the header list

      // lastly we generate a tag

      html.nl().put_string("<a name=\"");
      if (simple_anchors) {
	char buffer[MAX_LINE_LENGTH];

	sprintf(buffer, ANCHOR_TEMPLATE, header.no_of_headings);
	html.put_string(buffer);
      } else {
	html.put_string(header.header_buffer);
      }
      html.put_string("\"></a>").nl();
    }

    // and now we issue the real header
    html.put_string("<h");
    html.put_number(header.header_level);
    html.put_string(">");
    html.put_string(header.header_buffer);
    html.put_string("</h");
    html.put_number(header.header_level);
    html.put_string(">").nl();

    current_paragraph->do_para("");
  }
}

void html_printer::determine_header_level (int level)
{
  if (level == 0) {
    int i;
    int l=strlen(header.header_buffer);

    for (i=0; ((i<l) && ((header.header_buffer[i] == '.') || is_digit(header.header_buffer[i]))) ; i++) {
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
  char buf[MAX_STRING_LENGTH];
  int  level=atoi(arg);

  strcpy(header.header_buffer, "");
  page_contents->glyphs.move_right();
  if (! page_contents->glyphs.is_equal_to_head()) {
    g = page_contents->glyphs.get_data();
    do {
      if (g->is_auto_img()) {
	char *img=generate_img_src((char *)(g->text_string + 20));

	if (img) {
	  simple_anchors = TRUE;  // we cannot use full heading anchors with images
	  if (l != 0) {
	    strcat(header.header_buffer, " ");
	  }
	  l = g;
	  strcat(header.header_buffer, img);
	}
      } else if (! (g->is_a_line() || g->is_a_tag() || g->is_raw())) {
	/*
	 *  we ignore raw commands when constructing a heading
	 */
	if (l != 0) {
	  strcat(header.header_buffer, " ");
	}
	l = g;
	str_translate_to_html(g->text_style.f, buf, MAX_STRING_LENGTH, g->text_string, g->text_length, TRUE);
	strcat(header.header_buffer, (char *)buf);
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
      if (! is_font_courier(g->text_style.f)) {
	result = FALSE;
      }
      page_contents->glyphs.move_right();
    } while ((result) &&
	     (! page_contents->glyphs.is_equal_to_head()) &&
	     (! g->is_eol()));
    
    /*
     *  now restore our previous position.
     */
    while (page_contents->glyphs.get_data() != orig) {
      page_contents->glyphs.move_left();
    }
  }
  return( result );
}

/*
 *  do_linelength - handle the .ll command from troff.
 */

void html_printer::do_linelength (char *arg)
{
#if defined(INDENTATION)
  if (fill_on) {
    linelength = atoi(arg);
    current_paragraph->do_indent(indent.text, indentation, pageoffset, linelength);
  }
#endif
}

/*
 *  do_pageoffset - handle the .po command from troff.
 */

void html_printer::do_pageoffset (char *arg)
{
#if defined(INDENTATION)
  pageoffset = atoi(arg);
  if (fill_on) {
    current_paragraph->do_indent(indent.text, indentation, pageoffset, linelength);
  }
#endif
}

/*
 *  do_indentation - handle the .in command from troff.
 */

void html_printer::do_indentation (char *arg)
{
#if defined(INDENTATION)
  if (fill_on) {
    indentation = atoi(arg);
    current_paragraph->do_indent(indent.text, indentation, pageoffset, linelength);
  }
#endif
}

/*
 *  do_tempindent - handle the .ti command from troff.
 */

void html_printer::do_tempindent (char *arg)
{
#if defined(INDENTATION)
  if (fill_on) {
    end_tempindent = 1;
    prev_indent    = indentation;
    indentation    = atoi(arg);
    current_paragraph->do_indent(indent.text, indentation, pageoffset, linelength);
  }
#endif
}

/*
 *  do_indentedparagraph - handle the .ip tag, this buffers the next line
 *                         and passes this to text-text as the left hand
 *                         column table entry.
 */

void html_printer::do_indentedparagraph (void)
{
#if defined(INDENTATION)
  text_glob    *t;
  int           removed_from_head;
  char          buf[MAX_STRING_LENGTH];
  int           found_indent_start  = FALSE;

  indent.has_been_found = FALSE;
  indent.text[0]        = (char)0;

  if (! page_contents->glyphs.is_empty()) {
    page_contents->glyphs.sub_move_right();       /* move onto next word */
    do {
      t = page_contents->glyphs.get_data();
      removed_from_head = FALSE;
      if (t->is_auto_img()) {
	char *img=generate_img_src((char *)(t->text_string + 20));

	if (img) {
	  if (found_indent_start) {
	    strcat(indent.text, " ");
	  }
	  found_indent_start = TRUE;
	  strcat(indent.text, img);
	}
	page_contents->glyphs.sub_move_right(); 	  /* move onto next word */
      } else if (t->is_raw_command) {
	/* skip raw commands
	 */
	page_contents->glyphs.sub_move_right(); 	  /* move onto next word */
      } else if (t->is_a_tag() && (strncmp(t->text_string, "html-tag:.br", 12) == 0)) {
	/* end of indented para found, but move back so that we read this tag and process it
	 */
	page_contents->glyphs.move_left();           /* move backwards to last word */
	indent.has_been_found = TRUE;
	return;
      } else if (t->is_a_tag()) {
	page_contents->glyphs.sub_move_right(); 	  /* move onto next word */
      } else if (found_indent_start) {
	strcat(indent.text, " ");
	str_translate_to_html(t->text_style.f, buf, MAX_STRING_LENGTH, t->text_string, t->text_length, TRUE);
	strcat(indent.text, buf);
	page_contents->glyphs.sub_move_right(); 	  /* move onto next word */
	removed_from_head = ((!page_contents->glyphs.is_empty()) &&
			     (page_contents->glyphs.is_equal_to_head()));
      } else {
	str_translate_to_html(t->text_style.f, buf, MAX_STRING_LENGTH, t->text_string, t->text_length, TRUE);
	strcpy((char *)indent.text, buf);
	found_indent_start    = TRUE;
	indent.has_been_found = TRUE;
	page_contents->glyphs.sub_move_right(); 	  /* move onto next word */
	removed_from_head = ((!page_contents->glyphs.is_empty()) &&
			     (page_contents->glyphs.is_equal_to_head()));
      }
    } while ((! page_contents->glyphs.is_equal_to_head()) || (removed_from_head));
  }
  // page_contents->glyphs.move_left();           /* move backwards to last word */
#endif
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
    if (on) {
      current_paragraph->done_pre();
    } else {
      current_paragraph->do_pre();
    }
  }
  fill_on = on;
}

/*
 *  do_eol - handle the end of line
 */

void html_printer::do_eol (void)
{
  if (! fill_on) {
    current_paragraph->do_newline();
    current_paragraph->do_break();
  }
  output_hpos = indentation+pageoffset;
  if (end_center > 0) {
    if (end_center > 1) {
      current_paragraph->do_break();
    }
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
  current_paragraph->done_table();
}

/*
 *  do_links - moves onto a new temporary file and sets auto_links to FALSE.
 */

void html_printer::do_links (void)
{
  current_paragraph->done_para();
  current_paragraph->done_table();
  auto_links = FALSE;   /* from now on only emit under user request */
#if !defined(DEBUGGING)
  file_list.add_new_file(xtmpfile());
  html.set_file(file_list.get_file());
#endif
}

/*
 *  do_break - handles the ".br" request and also
 *             undoes an outstanding ".ti" command.
 */

void html_printer::do_break (void)
{
  current_paragraph->do_break();
#if defined(INDENTATION)
  if (end_tempindent > 0) {
    end_tempindent--;
    if (end_tempindent == 0) {
      indentation = prev_indent;
      current_paragraph->do_indent(indent.text, indentation, pageoffset, linelength);
    }
  }
#endif
  output_hpos = indentation+pageoffset;
  supress_sub_sup = TRUE;
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
  } else if (strncmp(t, ".sp", 3) == 0) {
    current_paragraph->do_space();
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
  } else if (strncmp(t, ".ip", 3) == 0) {
    do_indentedparagraph();
  } else if (strcmp(t, ".links") == 0) {
    do_links();
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

      if (strcmp(g->text_string, "XXXXXXX") == 0) {
	stop();
      }

      if (g->is_raw()) {
	emit_raw(g);
      } else if (g->is_a_tag()) {
	troff_tag(g);
      } else if (g->is_a_line()) {
	emit_line(g);
      } else {
	translate_to_html(g);
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

void html_printer::flush_page (void)
{
  supress_sub_sup = TRUE;
  flush_sbuf();
  // page_contents->dump_page();
  flush_globs();
  current_paragraph->done_para();
  current_paragraph->done_table();
  
  // move onto a new page
  delete page_contents;
  page_contents = new page;
}

/*
 *  determine_space - works out whether we need to write a space.
 *                    If last glyth is ajoining then no space emitted.
 */

void html_printer::determine_space (text_glob *g)
{
  if (current_paragraph->is_in_pre()) {
    int space_width  = sbuf_style.f->get_space_width(sbuf_style.point_size);
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
    current_paragraph->done_pre();
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
 *  translate_to_html - translates a textual string into html text
 */

void html_printer::translate_to_html (text_glob *g)
{
  char buf[MAX_STRING_LENGTH];

  do_font(g);
  determine_space(g);
  str_translate_to_html(g->text_style.f, buf, MAX_STRING_LENGTH,
			g->text_string, g->text_length, TRUE);
  current_paragraph->do_emittext(buf, strlen(buf));
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
  if (sbuf_len > 0) {
    int r=font::res;   // resolution of the device
    set_style(sbuf_style);

    page_contents->add(&sbuf_style, sbuf, sbuf_len,
		       line_number,
		       sbuf_vpos-sbuf_style.point_size*r/72, sbuf_start_hpos,
		       sbuf_vpos                           , sbuf_end_hpos);
	     
    output_hpos = sbuf_end_hpos;
    output_vpos = sbuf_vpos;
    sbuf_len = 0;
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
    if (np == 2) {
      page_contents->add_line(&sbuf_style,
			      line_number,
			      env->hpos, env->vpos, env->hpos+p[0], env->vpos+p[1], line_thickness);
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
#if 0
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
#endif
    }
    break;
  case 'E':
    // fall through
  case 'e':
#if 0
    if (np != 2) {
      error("2 arguments required for ellipse");
      break;
    }
    page_contents->add_line(code,
			    env->hpos, env->vpos-p[1]/2, env->hpos+p[0], env->vpos+p[1]/2,
			    env->size, fill);
#endif
    break;
  case 'C':
    // fill circle

  case 'c':
    {
#if 0
      // troff adds an extra argument to C
      if (np != 1 && !(code == 'C' && np == 2)) {
	error("1 argument required for circle");
	break;
      }
      page_contents->add_line(code,
			      env->hpos, env->vpos-p[0]/2, env->hpos+p[0], env->vpos+p[0]/2,
			      env->size, fill);
#endif
    }
    break;
  case 'a':
    {
#if 0
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
#endif
    }
    break;
  case '~':
    {
#if 0
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
#endif
    }
    break;
  case 'f':
    {
#if 0
      if (np != 1 && np != 2) {
	error("1 argument required for fill");
	break;
      }
      fill = p[0];
      if (fill < 0 || fill > FILL_MAX) {
	// This means fill with the current color.
	fill = FILL_MAX + 1;
      }
#endif
      break;
    }

  default:
    error("unrecognised drawing command `%1'", char(code));
    break;
  }
}

html_printer::html_printer()
: html(0, MAX_LINE_LENGTH),
  no_of_printed_pages(0),
  sbuf_len(0),
  output_hpos(-1),
  output_vpos(-1),
  output_vpos_max(-1),
  line_thickness(-1),
  inside_font_style(0),
  page_number(0),
  header_indent(-1),
  supress_sub_sup(TRUE),
  cutoff_heading(100),
  end_center(0),
  end_tempindent(0),
  next_tag(INLINE),
  fill_on(TRUE),
  linelength(0),
  pageoffset(0),
  indentation(0),
  prev_indent(0),
  line_number(0)
{
#if defined(DEBUGGING)
  file_list.add_new_file(stdout);
#else
  file_list.add_new_file(xtmpfile());
#endif
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
  paper_length      = font::paperlength;
  linelength        = font::res*13/2;
  if (paper_length == 0)
    paper_length    = 11*font::res;

  page_contents = new page();
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
 */

void html_printer::add_to_sbuf (unsigned char code, const char *name)
{
  if (name == 0) {
    add_char_to_sbuf(code);
  } else {
    if (sbuf_style.f != NULL) {
      char *html_glyph = get_html_translation(sbuf_style.f, name);

      if (html_glyph == NULL) {
	add_char_to_sbuf(code);
      } else {
	int   l          = strlen(html_glyph);
	int   i;

 	// Escape the name, so that "&" doesn't get expanded to "&amp;"
 	// later during translate_to_html.
 	add_char_to_sbuf('\\'); add_char_to_sbuf('(');

	for (i=0; i<l; i++) {
	  add_char_to_sbuf(html_glyph[i]);
	}
 	add_char_to_sbuf('\\'); add_char_to_sbuf(')');
      }
    }
  }
}

int html_printer::sbuf_continuation (unsigned char code, const char *name,
				     const environment *env, int w)
{
  if (sbuf_end_hpos == env->hpos) {
    add_to_sbuf(code, name);
    sbuf_end_hpos += w + sbuf_kern;
    return( TRUE );
  } else {
    if ((sbuf_len < SBUF_SIZE-1) && (env->hpos >= sbuf_end_hpos) &&
	((sbuf_kern == 0) || (sbuf_end_hpos - sbuf_kern != env->hpos))) {
      /*
       *  lets see whether a space is needed or not
       */
      int space_width = sbuf_style.f->get_space_width(sbuf_style.point_size);

      if (env->hpos-sbuf_end_hpos < space_width/2) {
	add_to_sbuf(code, name);
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
 *  get_html_translation - given the position of the character and its name
 *                         return the device encoding for such character.
 */

char *get_html_translation (font *f, const char *name)
{
  int  index;

  if ((f == 0) || (name == 0) || (strcmp(name, "") == 0)) {
    return( NULL );
  } else {
    index = f->name_to_index((char *)name);
    if (index == 0) {
      error("character `%s' not found", name);
      return( NULL );
    } else {
      if (f->contains(index)) {
	return( (char *)f->get_special_device_encoding(index) );
      } else {
	return( NULL );
      }
    }
  }
}

/*
 *  to_unicode - returns a unicode translation of char, ch.
 */

static char *to_unicode (unsigned char ch)
{
  static char buf[20];

  sprintf(buf, "&#%u;", (unsigned int)ch);
  return( buf );
}

/*
 *  char_translate_to_html - convert a single non escaped character
 *                           into the appropriate html character.
 */

int char_translate_to_html (font *f, char *buf, int buflen, unsigned char ch, int b, int and_single)
{
  if (and_single) {
    int    t, l;
    char  *translation;
    char   name[2];

    name[0] = ch;
    name[1] = (char)0;
    translation = get_html_translation(f, name);
    if ((translation == NULL) && (ch >= UNICODE_DESC_START)) {
      translation = to_unicode(ch);
    }
    if (translation) {
      l = strlen(translation);
      t = max(0, min(l, buflen-b));
      strncpy(&buf[b], translation, t);
      b += t;
    } else {
      if (b<buflen) {
	buf[b] = ch;
	b++;
      }
    }
  } else {
    /*
     *  do not attempt to encode single characters
     */
    if (b<buflen) {
      buf[b] = ch;
      b++;
    }
  }
  return( b );
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
  char       *translation;
  int         e;
  char        escaped_char[MAX_STRING_LENGTH];
  int         l;
  int         i=0;
  int         b=0;
  int         t=0;

#if 0
  if (strcmp(str, "``@,;:\\\\()[]''") == 0) {
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
	  
	    if (f->contains(index) && (index != 0)) {
	      buf[b] = f->get_code(index);
	      b++;
	    }
	  }
	}
      } else {
	b = char_translate_to_html(f, buf, buflen, str[i], b, and_single);
	i++;
      }
    } else {
      b = char_translate_to_html(f, buf, buflen, str[i], b, and_single);
      i++;
    }
  }
  buf[min(b, buflen)] = (char)0;
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
  if ((sbuf_len > 0) && (sbuf_len < SBUF_SIZE) && (sty == sbuf_style) &&
      (sbuf_vpos == env->vpos) && (sbuf_continuation(code, name, env, w))) {
    return;
  } else {
    flush_sbuf();
    sbuf_len = 0;
    add_to_sbuf(code, name);
    sbuf_end_hpos = env->hpos + w;
    sbuf_start_hpos = env->hpos;
    sbuf_vpos = env->vpos;
    sbuf_style = sty;
    sbuf_kern = 0;
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
      html.put_string("</title>").nl().nl();
    } else {
      title.has_been_written = TRUE;
      html.put_string("<h1 align=center>");
      html.put_string(title.text);
      html.put_string("</h1>").nl().nl();
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
#if defined(INDENTATION)
  current_paragraph->do_indent(indent.text, indentation, pageoffset, linelength);
#endif
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

html_printer::~html_printer()
{
  current_paragraph->flush_text();
  html.end_line();
  html.set_file(stdout);
  /*
   *  'HTML: The definitive guide', O'Reilly, p47. advises against specifying
   *         the dtd, so for the moment I'll leave this commented out.
   *         If requested we could always emit it if a command line switch
   *         was present.
   *
   *  fputs("<!doctype html public \"-//IETF//DTD HTML 4.0//EN\">\n", stdout);
   */
  fputs("<html>\n", stdout);
  fputs("<head>\n", stdout);
  fputs("<meta name=\"generator\" content=\"groff -Thtml, see www.gnu.org\">\n", stdout);
  fputs("<meta name=\"Content-Style\" content=\"text/css\">\n", stdout);
  write_title(TRUE);
  fputs("</head>\n", stdout);
  fputs("<body>\n\n", stdout);
  write_title(FALSE);
  header.write_headings(stdout, FALSE);
  write_rule();
  {
    html.begin_comment("Creator     : ")
       .put_string("groff ")
       .put_string("version ")
       .put_string(Version_string)
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
      .put_string(ctime(&t), strlen(ctime(&t))-1)
      .end_comment();
  }
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
      style sty(get_font_from_index(env->fontno), env->size, env->height, env->slant, env->fontno);
      sbuf_style = sty;
    }

    if (strncmp(s, "html:", 5) == 0) {
      int r=font::res;   /* resolution of the device */
      char buf[MAX_STRING_LENGTH];
      font *f=sbuf_style.f;

      if (f == NULL) {
	int found=FALSE;

	f = font::load_font("TR", &found);
      }
      str_translate_to_html(f, buf, MAX_STRING_LENGTH,
			    &s[5], strlen(s)-5, FALSE);

      /*
       *  need to pass rest of string through to html output during flush
       */
      page_contents->add_html(&sbuf_style, buf, strlen(buf),
			      line_number,
			      env->vpos-env->size*r/72, env->hpos,
			      env->vpos               , env->hpos);

      /*
       * assume that the html command has no width, if it does then hopefully troff
       * will have fudged this in a macro by requesting that the formatting move right by
       * the appropriate width.
       */
    } else if (strncmp(s, "index:", 6) == 0) {
      cutoff_heading = atoi(&s[6]);
    } else if (strncmp(s, "html-tag:", 9) == 0) {
      int r=font::res;   /* resolution of the device */

      page_contents->add_tag(&sbuf_style, s, strlen(s),
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
  while ((c = getopt_long(argc, argv, "o:i:I:D:F:vd?lrn", long_options, NULL))
	 != EOF)
    switch(c) {
    case 'v':
      {
	printf("GNU post-grohtml (groff) version %s\n", Version_string);
	exit(0);
	break;
      }
    case 'F':
      font::command_line_font_dir(optarg);
      break;
    case 'l':
      auto_links = FALSE;
      break;
    case 'r':
      auto_rule = FALSE;
      break;
    case 'o':
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
  delete pr;
  return 0;
}

static void usage(FILE *stream)
{
  fprintf(stream, "usage: %s [-vld?n] [-D dir] [-I image_stem] [-F dir] [files ...]\n",
	  program_name);
}
