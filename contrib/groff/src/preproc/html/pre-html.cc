// -*- C++ -*-
/* Copyright (C) 2000, 2001, 2002 Free Software Foundation, Inc.
     Written by Gaius Mulley (gaius@glam.ac.uk).

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

#define PREHTMLC

#include "lib.h"

#include <signal.h>
#include <ctype.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include "errarg.h"
#include "error.h"
#include "stringclass.h"
#include "posix.h"
#include "defs.h"
#include "searchpath.h"
#include "paper.h"
#include "font.h"

#include <errno.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef _POSIX_VERSION
#include <sys/wait.h>
#define PID_T pid_t
#else /* not _POSIX_VERSION */
#define PID_T int
#endif /* not _POSIX_VERSION */

#include <stdarg.h>

#include "nonposix.h"

extern "C" const char *Version_string;

#include "pre-html.h"
#include "pushback.h"
#include "html-strings.h"

#define DEFAULT_LINE_LENGTH        7   // inches wide
#define DEFAULT_IMAGE_RES        100   // number of pixels per inch resolution
#define IMAGE_BOARDER_PIXELS       0
#define INLINE_LEADER_CHAR      '\\'

#define TRANSPARENT  "-background white -transparent white"
#define MIN_ALPHA_BITS             0
#define MAX_ALPHA_BITS             4

#define PAGE_TEMPLATE_SHORT "pg"
#define PAGE_TEMPLATE_LONG "-page-"
#define PS_TEMPLATE_SHORT "ps"
#define PS_TEMPLATE_LONG "-ps-"
#define REGION_TEMPLATE_SHORT "rg"
#define REGION_TEMPLATE_LONG "-regions-"

#if 0
#   define  DEBUGGING
#endif

#if !defined(TRUE)
#   define TRUE (1==1)
#endif
#if !defined(FALSE)
#   define FALSE (1==0)
#endif

typedef enum {CENTERED, LEFT, RIGHT, INLINE} IMAGE_ALIGNMENT;

static int   postscriptRes    =-1;              // postscript resolution, dots per inch
static int   stdoutfd         = 1;              // output file descriptor - normally 1 but might move
                                                // -1 means closed
static int   copyofstdoutfd   =-1;              // a copy of stdout, so we can restore stdout when
                                                // writing to post-html
static char *psFileName       = NULL;           // name of postscript file
static char *psPageName       = NULL;           // name of file containing postscript current page
static char *regionFileName   = NULL;           // name of file containing all image regions
static char *imagePageName    = NULL;           // name of bitmap image containing current page
static char *image_device     = "pnmraw";
static int   image_res        = DEFAULT_IMAGE_RES;
static int   vertical_offset  = 0;
static char *image_template   = NULL;           // image template filename
static char *macroset_template= NULL;           // image template passed to troff by -D
static int   troff_arg        = 0;              // troff arg index
static char *image_dir        = NULL;           // user specified image directory
static int   textAlphaBits    = MAX_ALPHA_BITS;
static int   graphicAlphaBits = MAX_ALPHA_BITS;
static char *antiAlias        = NULL;           // antialias arguments we pass to gs.
static int   show_progress    = FALSE;          // should we display page numbers as they are processed?
static int   currentPageNo    = -1;             // current image page number
#if defined(DEBUGGING)
static int   debug          = FALSE;
static char *troffFileName  = NULL;             // output of pre-html output which is sent to troff -Tps
static char *htmlFileName   = NULL;             // output of pre-html output which is sent to troff -Thtml
#endif

static char *linebuf = NULL;                    // for scanning devps/DESC
static int linebufsize = 0;

const char *const FONT_ENV_VAR = "GROFF_FONT_PATH";
static search_path font_path(FONT_ENV_VAR, FONTPATH, 0, 0);


/*
 *  Images are generated via postscript, gs and the pnm utilities.
 */

#define IMAGE_DEVICE    "-Tps"

/*
 *  prototypes
 */
static int do_file(const char *filename);

/*
 *  sys_fatal - writes a fatal error message.
 *  Taken from src/roff/groff/pipeline.c.
 */

void sys_fatal (const char *s)
{
  fatal("%1: %2", s, strerror(errno));
}

/*
 *  get_line - copies a line (w/o newline) from a file to the global line buffer
 */

int get_line (FILE *f)
{
  if (f == 0)
    return 0;
  if (linebuf == 0) {
    linebuf = new char[128];
    linebufsize = 128;
  }
  int i = 0;
  // skip leading whitespace
  for (;;) {
    int c = getc(f);
    if (c == EOF)
      return 0;
    if (c != ' ' && c != '\t') {
      ungetc(c, f);
      break;
    }
  }
  for (;;) {
    int c = getc(f);
    if (c == EOF)
      break;
    if (i + 1 >= linebufsize) {
      char *old_linebuf = linebuf;
      linebuf = new char[linebufsize * 2];
      memcpy(linebuf, old_linebuf, linebufsize);
      a_delete old_linebuf;
      linebufsize *= 2;
    }
    linebuf[i++] = c;
    if (c == '\n') {
      i--;
      break;
    }
  }
  linebuf[i] = '\0';
  return 1;
}

/*
 *  get_resolution - returns the postscript resolution from devps/DESC
 */

static unsigned int get_resolution (void)
{
  char *pathp;
  FILE *f;
  unsigned int res;
  f = font_path.open_file("devps/DESC", &pathp);
  if (f == 0)
    fatal("can't open devps/DESC");
  while (get_line(f)) {
    int n = sscanf(linebuf, "res %u", &res);
    if (n >= 1) {
      fclose(f);
      return res;
    }
  }
  fatal("can't find `res' keyword in devps/DESC");
  return 0;
}

/*
 *  html_system - a wrapper for system()
 */

void html_system(const char *s, int redirect_stdout)
{
  // Redirect standard error to the null device.  This is more
  // portable than using "2> /dev/null", since it doesn't require a
  // Unixy shell.
  int save_stderr = dup(2);
  int save_stdout = dup(1);
  int fdnull = open(NULL_DEV, O_WRONLY|O_BINARY, 0666);
  if (save_stderr > 2 && fdnull > 2)
    dup2(fdnull, 2);
  if (redirect_stdout && save_stdout > 1 && fdnull > 1)
    dup2(fdnull, 1);
  if (fdnull >= 0)
    close(fdnull);
  int status = system(s);
  dup2(save_stderr, 2);
  if (redirect_stdout)
    dup2(save_stdout, 1);
  if (status == -1)
    fprintf(stderr, "Calling `%s' failed\n", s);
  else if (status)
    fprintf(stderr, "Calling `%s' returned status %d\n", s, status);
  close(save_stderr);
  close(save_stdout);
}

/*
 *  make_message - taken from man printf(3), creates a string via malloc
 *                 and places the result of the va args into string.
 *                 Finally the new string is returned.
 */

char *
make_message (const char *fmt, ...)
{
  /* Guess we need no more than 100 bytes. */
  int n, size = 100;
  char *p;
  char *np;
  va_list ap;
  if ((p = (char *)malloc (size)) == NULL)
    return NULL;
  while (1) {
    /* Try to print in the allocated space. */
    va_start(ap, fmt);
    n = vsnprintf (p, size, fmt, ap);
    va_end(ap);
    /* If that worked, return the string. */
    if (n > -1 && n < size) {
      if (size > n+1) {
	np = strsave(p);
	free(p);
	return np;
      }
      return p;
    }
    /* Else try again with more space. */
    if (n > -1)    /* glibc 2.1 */
      size = n+1; /* precisely what is needed */
    else           /* glibc 2.0 */
      size *= 2;  /* twice the old size */
    if ((np = (char *)realloc (p, size)) == NULL) {
      free(p);  /* realloc failed, free old, p. */
      return NULL;
    }
    p = np;  /* use realloc'ed, p */
  }
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

/*
 *  char_block - constructor, sets the, used, and, next, fields to zero.
 */

char_block::char_block()
: used(0), next(0)
{
}

class char_buffer {
public:
  char_buffer();
  ~char_buffer();
  int  read_file(FILE *fp);
  int  do_html(int argc, char *argv[]);
  int  do_image(int argc, char *argv[]);
  void write_file_html(void);
  void write_file_troff(void);
  void write_upto_newline (char_block **t, int *i, int is_html);
  int  can_see(char_block **t, int *i, char *string);
  int  skip_spaces(char_block **t, int *i);
  void skip_until_newline(char_block **t, int *i);
private:
  char_block *head;
  char_block *tail;
};

/*
 *  char_buffer - constructor
 */

char_buffer::char_buffer()
: head(0), tail(0)
{
}

/*
 *  char_buffer - deconstructor, throws aways the whole buffer list.
 */

char_buffer::~char_buffer()
{
  while (head != NULL) {
    char_block *temp = head;
    head = head->next;
    delete temp;
  }
}

/*
 *  read_file - read in a complete file, fp, placing the contents inside char_blocks.
 */

int char_buffer::read_file (FILE *fp)
{
  int n;

  while (! feof(fp)) {
    if (tail == NULL) {
      tail = new char_block;
      head = tail;
    } else {
      if (tail->used == char_block::SIZE) {
	tail->next = new char_block;
	tail       = tail->next;
      }
    }
    // at this point we have a tail which is ready for the next SIZE bytes of the file

    n = fread(tail->buffer, sizeof(char), char_block::SIZE-tail->used, fp);
    if (n <= 0) {
      // error
      return( 0 );
    } else {
      tail->used += n*sizeof(char);
    }
  }
  return( 1 );
}

/*
 *  writeNbytes - writes n bytes to stdout.
 */

static void writeNbytes (char *s, int l)
{
  int n=0;
  int r;

  while (n<l) {
    r = write(stdoutfd, s, l-n);
    if (r<0) {
      sys_fatal("write");
    }
    n += r;
    s += r;
  }
}

/*
 *  writeString - writes a string to stdout.
 */

static void writeString (char *s)
{
  writeNbytes(s, strlen(s));
}

/*
 *  makeFileName - creates the image filename template
 *                 and the macroset image template.
 */

static void makeFileName (void)
{
  if ((image_dir != NULL) && (strchr(image_dir, '%') != NULL)) {
    error("cannot use a `%%' within the image directory name");
    exit(1);
  }

  if ((image_template != NULL) && (strchr(image_template, '%') != NULL)) {
    error("cannot use a `%%' within the image template");
    exit(1);
  }

  if (image_dir == NULL) {
    image_dir = "";
  } else if ((strlen(image_dir)>0) && (image_dir[strlen(image_dir)-1] != '/')) {
    image_dir = make_message("%s/", image_dir);
    if (image_dir == NULL)
      sys_fatal("make_message");
  }
  
  if (image_template == NULL)
    macroset_template = make_message("%sgrohtml-%d", image_dir, (int)getpid());
  else
    macroset_template = make_message("%s%s", image_dir, image_template);

  if (macroset_template == NULL)
    sys_fatal("make_message");

  image_template = (char *)malloc(strlen("-%d")+strlen(macroset_template)+1);
  if (image_template == NULL)
    sys_fatal("malloc");
  strcpy(image_template, macroset_template);
  strcat(image_template, "-%d");
}

/*
 *  setupAntiAlias - sets up the antialias string, used when we call gs.
 */

static void setupAntiAlias (void)
{
  if (textAlphaBits == 0 && graphicAlphaBits == 0)
    antiAlias = make_message(" ");
  else if (textAlphaBits == 0)
    antiAlias = make_message("-dGraphicsAlphaBits=%d ", graphicAlphaBits);
  else if (graphicAlphaBits == 0)
    antiAlias = make_message("-dTextAlphaBits=%d ", textAlphaBits);
  else
    antiAlias = make_message("-dTextAlphaBits=%d -dGraphicsAlphaBits=%d ",
			     textAlphaBits, graphicAlphaBits);
}

/*
 *  checkImageDir - checks to see whether the image directory is available.
 */

static void checkImageDir (void)
{
  if ((image_dir != NULL) && (strcmp(image_dir, "") != 0))
    if (! ((mkdir(image_dir, 0777) == 0) || (errno == EEXIST))) {
      error("cannot create directory `%1'", image_dir);
      exit(1);
    }
}

/*
 *  write_end_image - ends the image. It writes out the image extents if we are using -Tps.
 */

static void write_end_image (int is_html)
{
  /*
   *  if we are producing html then these
   *    emit image name and enable output
   *  else
   *    we are producing images
   *    in which case these generate image
   *    boundaries
   */
  writeString("\\O[4]\\O[2]");
  if (is_html)
    writeString("\\O[1]");
  else
    writeString("\\O[0]");
}

/*
 *  write_start_image - writes the troff which will:
 *
 *                      (i)  disable html output for the following image
 *                      (ii) reset the max/min x/y registers during postscript
 *                           rendering.
 */

static void write_start_image (IMAGE_ALIGNMENT pos, int is_html)
{
  writeString("\\O[5");
  switch (pos) {

  case INLINE:
    writeString("i");
    break;
  case LEFT:
    writeString("l");
    break;
  case RIGHT:
    writeString("r");
    break;
  case CENTERED:
  default:
    writeString("c");
    break;
  }
  writeString(image_template); writeString(".png]");
  if (is_html)
    writeString("\\O[0]\\O[3]");
  else
    // reset min/max registers
    writeString("\\O[1]\\O[3]");
}

/*
 *  write_upto_newline - writes the contents of the buffer until a newline is seen.
 *                       It checks for HTML_IMAGE_INLINE_BEGIN and HTML_IMAGE_INLINE_END
 *                       and if they are present it processes them.
 */

void char_buffer::write_upto_newline (char_block **t, int *i, int is_html)
{
  int j=*i;

  if (*t) {
    while ((j < (*t)->used) && ((*t)->buffer[j] != '\n') &&
	   ((*t)->buffer[j] != INLINE_LEADER_CHAR)) {
      j++;
    }
    if ((j < (*t)->used) && ((*t)->buffer[j] == '\n')) {
      j++;
    }
    writeNbytes((*t)->buffer+(*i), j-(*i));
    if ((*t)->buffer[j] == INLINE_LEADER_CHAR) {
      if (can_see(t, &j, HTML_IMAGE_INLINE_BEGIN))
	write_start_image(INLINE, is_html);
      else if (can_see(t, &j, HTML_IMAGE_INLINE_END))
	write_end_image(is_html);
      else {
	if (j < (*t)->used) {
	  *i = j;
	  j++;
	  writeNbytes((*t)->buffer+(*i), j-(*i));
	}
      }
    }
    if (j == (*t)->used) {
      *i = 0;
      if ((*t)->buffer[j-1] == '\n') {
	*t = (*t)->next;
      } else {
	*t = (*t)->next;
	write_upto_newline(t, i, is_html);
      }
    } else {
      // newline was seen
      *i = j;
    }
  }
}

/*
 *  can_see - returns TRUE if we can see string in t->buffer[i] onwards
 */

int char_buffer::can_see (char_block **t, int *i, char *string)
{
  int j         = 0;
  int l         = strlen(string);
  int k         = *i;
  char_block *s = *t;

  while (s) {
    while ((k<s->used) && (j<l) && (s->buffer[k] == string[j])) {
      j++;
      k++;
    }
    if (j == l) {
      *i = k;
      *t = s;
      return( TRUE );
    } else if ((k<s->used) && (s->buffer[k] != string[j])) {
      return( FALSE );
    }
    s = s->next;
    k = 0;
  }
  return( FALSE );
}

/*
 *  skip_spaces - returns TRUE if we have not run out of data.
 *                It also consumes spaces.
 */

int char_buffer::skip_spaces(char_block **t, int *i)
{
  char_block *s = *t;
  int k         = *i;

  while (s) {
    while ((k<s->used) && (isspace(s->buffer[k]))) {
      k++;
    }
    if (k == s->used) {
      k = 0;
      s = s->next;
    } else {
      *i = k;
      return( TRUE );
    }
  }
  return( FALSE );
}

/*
 *  skip_until_newline - skips all characters until a newline is seen.
 *                       The newline is not consumed.
 */

void char_buffer::skip_until_newline (char_block **t, int *i)
{
  int j=*i;

  if (*t) {
    while ((j < (*t)->used) && ((*t)->buffer[j] != '\n')) {
      j++;
    }
    if (j == (*t)->used) {
      *i = 0;
      *t = (*t)->next;
      skip_until_newline(t, i);
    } else {
      // newline was seen
      *i = j;
    }
  }
}

/*
 *  write_file_troff - writes the buffer to stdout (troff).
 */

void char_buffer::write_file_troff (void)
{
  char_block *t=head;
  int         i=0;

  if (t != NULL) {
    do {
      write_upto_newline(&t, &i, FALSE);
    } while (t != NULL);
  }
  if (close(stdoutfd) < 0)
    sys_fatal("close");

  // now we grab fd=1 so that the next pipe cannot use fd=1
  if (stdoutfd == 1) {
    if (dup(2) != stdoutfd) {
      sys_fatal("dup failed to use fd=1");
    }
  }
}

/*
 *  the image class remembers the position of all images in the postscript file
 *  and assigns names for each image.
 */

struct imageItem {
  imageItem  *next;
  int         X1;
  int         Y1;
  int         X2;
  int         Y2;
  char       *imageName;
  int         resolution;
  int         maxx;
  int         pageNo;

  imageItem (int x1, int y1, int x2, int y2, int page, int res, int max_width, char *name);
  ~imageItem ();
};

/*
 *  imageItem - constructor
 */

imageItem::imageItem (int x1, int y1, int x2, int y2, int page, int res, int max_width, char *name)
{
  X1         = x1;
  Y1         = y1;
  X2         = x2;
  Y2         = y2;
  pageNo     = page;
  resolution = res;
  maxx       = max_width;
  imageName  = name;
  next       = NULL;
}

/*
 *  imageItem - deconstructor
 */

imageItem::~imageItem ()
{
}

/*
 *  imageList - class containing a list of imageItems.
 */

class imageList {
private:
  imageItem *head;
  imageItem *tail;
  int        count;
public:
  imageList();
  ~imageList();
  void  add(int x1, int y1, int x2, int y2, int page, int res, int maxx, char *name);
  void  createImages (void);
  int   createPage (int pageno);
  void  createImage (imageItem *i);
  int   getMaxX (int pageno);
};

/*
 *  imageList - constructor.
 */

imageList::imageList ()
  : head(0), tail(0), count(0)
{
}

/*
 *  imageList - deconstructor.
 */

imageList::~imageList ()
{
  while (head != NULL) {
    imageItem *i = head;
    head = head->next;
    delete i;
  }
}

/*
 *  createPage - creates one image of, page pageno, from the postscript file.
 */

int imageList::createPage (int pageno)
{
  char *s;

  if (currentPageNo == pageno)
    return 0;

  if (currentPageNo >= 1) {
    /*
     *  we need to unlink the files which change each time a new page is processed.
     *  The final unlink is done by xtmpfile when pre-grohtml exits.
     */
    unlink(imagePageName);
    unlink(psPageName);
  }

  if (show_progress) {
    fprintf(stderr, "[%d] ", pageno);
    fflush(stderr);
  }

#if defined(DEBUGGING)
  if (debug)
    fprintf(stderr, "creating page %d\n", pageno);
#endif

  s = make_message("psselect -q -p%d %s %s\n",
		   pageno, psFileName, psPageName);

  if (s == NULL)
    sys_fatal("make_message");
#if defined(DEBUGGING)
  if (debug) {
    fwrite(s, sizeof(char), strlen(s), stderr);
    fflush(stderr);
  }
#endif
  html_system(s, 1);
  
  s = make_message("echo showpage | "
		   "gs%s -q -dBATCH -dSAFER "
		   "-dDEVICEHEIGHTPOINTS=792 "
		   "-dDEVICEWIDTHPOINTS=%d -dFIXEDMEDIA=true "
		   "-sDEVICE=%s -r%d %s "
		   "-sOutputFile=%s %s -\n",
		   EXE_EXT,
		   (getMaxX(pageno) * image_res) / postscriptRes,
		   image_device,
		   image_res,
		   antiAlias,
		   imagePageName,
		   psPageName);
  if (s == NULL)
    sys_fatal("make_message");
#if defined(DEBUGGING)
  if (debug) {
    fwrite(s, sizeof(char), strlen(s), stderr);
    fflush(stderr);
  }
#endif
  html_system(s, 1);
  a_delete s;
  currentPageNo = pageno;
  return 0;
}

/*
 *  min - returns the minimum of two numbers.
 */

int min (int x, int y)
{
  if (x < y) {
    return( x );
  } else {
    return( y );
  }
}

/*
 *  max - returns the maximum of two numbers.
 */

int max (int x, int y)
{
  if (x > y) {
    return( x );
  } else {
    return( y );
  }
}

/*
 *  getMaxX - returns the largest right hand position for any image on, pageno
 */

int imageList::getMaxX (int pageno)
{
  imageItem *h = head;
  int x        = postscriptRes * DEFAULT_LINE_LENGTH;

  while (h != NULL) {
    if (h->pageNo == pageno)
      x = max(h->X2, x);
    h = h->next;
  }
  return x;
}

/*
 *  createImage - generates a minimal png file from the set of page images.
 */

void imageList::createImage (imageItem *i)
{
  if (i->X1 != -1) {
    char *s;
    int  x1 = max(min(i->X1, i->X2)*image_res/postscriptRes-1*IMAGE_BOARDER_PIXELS, 0);
    int  y1 = max((image_res*vertical_offset/72)+min(i->Y1, i->Y2)*image_res/postscriptRes-IMAGE_BOARDER_PIXELS, 0);
    int  x2 = max(i->X1, i->X2)*image_res/postscriptRes+1*IMAGE_BOARDER_PIXELS;
    int  y2 = (image_res*vertical_offset/72)+(max(i->Y1, i->Y2)*image_res/postscriptRes)+1+IMAGE_BOARDER_PIXELS;
    if (createPage(i->pageNo) == 0) {
      s = make_message("pnmcut%s %d %d %d %d < %s | pnmcrop -quiet | pnmtopng%s %s > %s \n",
		       EXE_EXT,
		       x1, y1, x2-x1+1, y2-y1+1,
		       imagePageName,
		       EXE_EXT,
		       TRANSPARENT,
		       i->imageName);
      if (s == NULL)
	sys_fatal("make_message");

#if defined(DEBUGGING)
      if (debug) {
	fprintf(stderr, s);
	fflush(stderr);
      }
#endif
      html_system(s, 0);
      a_delete s;
    } else {
      fprintf(stderr, "failed to generate image of page %d\n", i->pageNo);
      fflush(stderr);
    }
#if defined(DEBUGGING)
  } else {
    if (debug) {
      fprintf(stderr, "ignoring image as x1 coord is -1\n");
      fflush(stderr);
    }
#endif
  }
}

/*
 *  add - an image description to the imageList.
 */

void imageList::add (int x1, int y1, int x2, int y2, int page, int res, int maxx, char *name)
{
  imageItem *i = new imageItem(x1, y1, x2, y2, page, res, maxx, name);

  if (head == NULL) {
    head = i;
    tail = i;
  } else {
    tail->next = i;
    tail = i;
  }
}

/*
 *  createImages - foreach image descriptor on the imageList, create the actual image.
 */

void imageList::createImages (void)
{
  imageItem *h = head;

  while (h != NULL) {
    createImage(h);
    h = h->next;
  }
}

static imageList listOfImages;  // list of images defined by the region file.

/*
 *  write_file_html - writes the buffer to stdout (troff).
 *                    It writes out the file replacing template image names with
 *                    actual image names.
 */

void char_buffer::write_file_html (void)
{
  char_block *t=head;
  int         i=0;

  if (t != NULL) {
    do {
      write_upto_newline(&t, &i, TRUE);
    } while (t != NULL);
  }
  if (close(stdoutfd) < 0)
    sys_fatal("close");

  // now we grab fd=1 so that the next pipe cannot use fd=1
  if (stdoutfd == 1) {
    if (dup(2) != stdoutfd) {
      sys_fatal("dup failed to use fd=1");
    }
  }
}

/*
 *  generateImages - parses the region file and generates images
 *                   from the postscript file. The region file
 *                   contains the x1,y1  x2,y2 extents of each
 *                   image.
 */

static void generateImages (char *regionFileName)
{
  pushBackBuffer *f=new pushBackBuffer(regionFileName);

  while (f->putPB(f->getPB()) != eof) {
    if (f->isString("grohtml-info:page")) {
      int page   = f->readInt();
      int x1     = f->readInt();
      int y1     = f->readInt();
      int x2     = f->readInt();
      int y2     = f->readInt();
      int maxx   = f->readInt();
      char *name = f->readString();
      int res    = postscriptRes;
      listOfImages.add(x1, y1, x2, y2, page, res, maxx, name);
      while ((f->putPB(f->getPB()) != '\n') &&
	     (f->putPB(f->getPB()) != eof)) {
	(void)f->getPB();
      }
      if (f->putPB(f->getPB()) == '\n') {
	(void)f->getPB();
      }
    } else {
      /*
       *  write any error messages out to the user
       */
      fputc(f->getPB(), stderr);
    }
  }
  
  listOfImages.createImages();
  if (show_progress) {
    fprintf(stderr, "done\n");
    fflush(stderr);
  }
  delete f;
}

/*
 *  replaceFd - replace a file descriptor, was, with, willbe.
 */

static void replaceFd (int was, int willbe)
{
  int dupres;

  if (was != willbe) {
    if (close(was)<0) {
      sys_fatal("close");
    }
    dupres = dup(willbe);
    if (dupres != was) {
      sys_fatal("dup");
      fprintf(stderr, "trying to replace fd=%d with %d dup used %d\n", was, willbe, dupres);
      if (willbe == 1) {
	fprintf(stderr, "likely that stdout should be opened before %d\n", was);
      }
      exit(1);
    }
    if (close(willbe) < 0) {
      sys_fatal("close");
    }
  }
}

/*
 *  waitForChild - waits for child, pid, to exit.
 */

static void waitForChild (PID_T pid)
{
  PID_T waitpd;
  int   status;

  waitpd = WAIT(&status, pid, _WAIT_CHILD);
  if (waitpd != pid)
    sys_fatal("wait");
}

/*
 *  alterDeviceTo - if toImage is set then the arg list is altered to include
 *                     IMAGE_DEVICE and we invoke groff rather than troff.
 *                  else 
 *                     set -Thtml and groff
 */

static void alterDeviceTo (int argc, char *argv[], int toImage)
{
  int i=0;

  if (toImage) {
    while (i < argc) {
      if (strcmp(argv[i], "-Thtml") == 0) {
	argv[i] = IMAGE_DEVICE;
      }
      i++;
    }
    argv[troff_arg] = "groff";  /* rather than troff */
  } else {
    while (i < argc) {
      if (strcmp(argv[i], IMAGE_DEVICE) == 0) {
	argv[i] = "-Thtml";
      }
      i++;
    }
    argv[troff_arg] = "groff";   /* use groff -Z */
  }
}

/*
 *  addZ - appends -Z onto the command list for groff.
 */

char **addZ (int argc, char *argv[])
{
  char **new_argv = (char **)malloc((argc+2)*sizeof(char *));
  int    i=0;

  if (new_argv == NULL)
    sys_fatal("malloc");

  if (argc > 0) {
    new_argv[i] = argv[i];
    i++;
  }
  new_argv[i] = "-Z";
  while (i<argc) {
    new_argv[i+1] = argv[i];
    i++;
  }
  argc++;
  new_argv[argc] = NULL;
  return new_argv;
}

/*
 *  addRegDef - appends a defined register or string onto the command list for troff.
 */

char **addRegDef (int argc, char *argv[], const char *numReg)
{
  char **new_argv = (char **)malloc((argc+2)*sizeof(char *));
  int    i=0;

  if (new_argv == NULL)
    sys_fatal("malloc");

  while (i<argc) {
    new_argv[i] = argv[i];
    i++;
  }
  new_argv[argc] = strdup(numReg);
  argc++;
  new_argv[argc] = NULL;
  return new_argv;
}

/*
 *  dump_args - display the argument list.
 */

void dump_args (int argc, char *argv[])
{
  fprintf(stderr, "  %d arguments:", argc);
  for (int i=0; i<argc; i++)
    fprintf(stderr, " %s", argv[i]);
  fprintf(stderr, "\n");
}

/*
 *  do_html - sets the troff number htmlflip and
 *            writes out the buffer to troff -Thtml
 */

int char_buffer::do_html(int argc, char *argv[])
{
  int pdes[2];
  PID_T pid;
  string s;

  alterDeviceTo(argc, argv, 0);
  argv += troff_arg;   // skip all arguments up to groff
  argc -= troff_arg;
  argv = addZ(argc, argv);
  argc++;

  s = "-dwww-image-template=";
  s += macroset_template;   // do not combine these statements otherwise they will not work
  s += '\0';                // the trailing '\0' is ignored
  argv = addRegDef(argc, argv, s.contents());
  argc++;

  if (pipe(pdes) < 0)
    sys_fatal("pipe");

  pid = fork();
  if (pid < 0)
    sys_fatal("fork");

  if (pid == 0) {
    // child
    replaceFd(0, pdes[0]);
    // close end we are not using
    if (close(pdes[1])<0)
      sys_fatal("close");
    replaceFd(1, copyofstdoutfd); // and restore stdout

    execvp(argv[0], argv);
    error("couldn't exec %1: %2", argv[0], strerror(errno), (char *)0);
    fflush(stderr);		/* just in case error() doesn't */
    exit(1);
  } else {
    // parent

#if defined(DEBUGGING)
    /*
     *  slight security risk so only enabled if compiled with defined(DEBUGGING)
     */
    if (debug) {
      replaceFd(1, creat(htmlFileName, S_IWUSR|S_IRUSR));
      write_file_html();
    }
#endif
    replaceFd(1, pdes[1]);
    // close end we are not using
    if (close(pdes[0])<0)
      sys_fatal("close");

    write_file_html();
    waitForChild(pid);
  }
  return 0;
}

/*
 *  do_image - writes out the buffer to troff -Tps
 */

int char_buffer::do_image(int argc, char *argv[])
{
  PID_T pid;
  int pdes[2];
  string s;

  alterDeviceTo(argc, argv, 1);
  argv += troff_arg;   // skip all arguments up to troff/groff
  argc -= troff_arg;
  argv = addRegDef(argc, argv, "-rps4html=1");
  argc++;

  s = "-dwww-image-template=";
  s += macroset_template;
  s += '\0';
  argv = addRegDef(argc, argv, s.contents());
  argc++;

  // override local settings and produce a page size letter postscript file
  argv = addRegDef(argc, argv, "-P-pletter");
  argc++;

  if (pipe(pdes) < 0)
    sys_fatal("pipe");

  pid = fork();
  if (pid == 0) {
    // child

    int psFd     = creat(psFileName,     S_IWUSR|S_IRUSR);
    int regionFd = creat(regionFileName, S_IWUSR|S_IRUSR);

    replaceFd(1, psFd);
    replaceFd(0, pdes[0]);
    replaceFd(2, regionFd);

    // close end we are not using
    if (close(pdes[1])<0)
      sys_fatal("close");

    execvp(argv[0], argv);
    error("couldn't exec %1: %2", argv[0], strerror(errno), (char *)0);
    fflush(stderr);		/* just in case error() doesn't */
    exit(1);
  } else {
    // parent

#if defined(DEBUGGING)
    /*
     *  slight security risk so only enabled if compiled with defined(DEBUGGING)
     */
    if (debug) {
      replaceFd(1, creat(troffFileName, S_IWUSR|S_IRUSR));
      write_file_troff();
    }
#endif
    replaceFd(1, pdes[1]);
    write_file_troff();
    waitForChild(pid);
  }
  return 0;
}

static char_buffer inputFile;


/*
 *  usage - emit usage arguments.
 */

void usage(FILE *stream)
{
  fprintf(stream, "usage: %s troffname [-Iimage_name] [-Dimage_directory] [-P-o vertical_image_offset] [-P-i image_resolution] [troff flags] [files]\n", program_name);
  fprintf(stream, "    vertical_image_offset (default %d/72 of an inch)\n", vertical_offset);
  fprintf(stream, "    image_resolution (default %d) pixels per inch\n", image_res);
  fprintf(stream, "    image_name is the name of the stem for all images (default is grohtml-<pid>)\n");
  fprintf(stream, "    place all png files into image_directory\n");
}

/*
 *  scanArguments - scans for all arguments including -P-i, -P-o, -P-D and -P-I. It returns
 *                  the argument index of the first non option.
 */

int scanArguments (int argc, char **argv)
{
  const char *command_prefix = getenv("GROFF_COMMAND_PREFIX");
  if (!command_prefix)
    command_prefix = PROG_PREFIX;
  char *troff_name = new char[strlen(command_prefix) + strlen("troff") + 1];
  strcpy(troff_name, command_prefix);
  strcat(troff_name, "troff");
  int c, i;
  static const struct option long_options[] = {
    { "help", no_argument, 0, CHAR_MAX + 1 },
    { "version", no_argument, 0, 'v' },
    { NULL, 0, 0, 0 }
  };
  while ((c = getopt_long(argc, argv, "+a:g:o:i:I:D:F:vbdhlrnp", long_options, NULL))
	 != EOF)
    switch(c) {
    case 'v':
      printf("GNU pre-grohtml (groff) version %s\n", Version_string);
      exit(0);
    case 'a':
      textAlphaBits = min(max(MIN_ALPHA_BITS, atoi(optarg)), MAX_ALPHA_BITS);
      if (textAlphaBits == 3) {
	error("cannot use 3 bits of antialiasing information");
	exit(1);
      }
      break;
    case 'g':
      graphicAlphaBits = min(max(MIN_ALPHA_BITS, atoi(optarg)), MAX_ALPHA_BITS);
      if (graphicAlphaBits == 3) {
	error("cannot use 3 bits of antialiasing information");
	exit(1);
      }
      break;
    case 'b':
      // handled by post-grohtml (set background color to white)
      break;
    case 'D':
      image_dir = optarg;
      break;
    case 'I':
      image_template = optarg;
      break;
    case 'i':
      image_res = atoi(optarg);
      break;
    case 'F':
      font_path.command_line_dir(optarg);
      break;
    case 'o':
      vertical_offset = atoi(optarg);
      break;
    case 'p':
      show_progress = TRUE;
      break;
    case 'd':
#if defined(DEBUGGING)
      debug = TRUE;
#endif
      break;
    case 'h':
      // handled by post-grohtml
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
      break;
    }

  i = optind;
  while (i < argc) {
    if (strcmp(argv[i], troff_name) == 0)
      troff_arg = i;
    else if (argv[i][0] != '-')
      return i;
    i++;
  }
  a_delete troff_name;

  return argc;
}

/*
 *  makeTempFiles - name the temporary files
 */

static int makeTempFiles (void)
{
#if defined(DEBUGGING)
  psFileName     = "/tmp/prehtml-ps";
  regionFileName = "/tmp/prehtml-region";
  imagePageName  = "/tmp/prehtml-page";
  psPageName     = "/tmp/prehtml-psn";
  troffFileName  = "/tmp/prehtml-troff";
  htmlFileName   = "/tmp/prehtml-html";
#else
  FILE *f;

  /* psPageName contains a single page of postscript */
  f = xtmpfile(&psPageName,
	       PS_TEMPLATE_LONG, PS_TEMPLATE_SHORT,
	       TRUE);
  if (f == NULL) {
    sys_fatal("xtmpfile");
    return -1;
  }
  fclose(f);

  /* imagePageName contains a bitmap image of the single postscript page */
  f = xtmpfile(&imagePageName,
	       PAGE_TEMPLATE_LONG, PAGE_TEMPLATE_SHORT,
	       TRUE);
  if (f == NULL) {
    sys_fatal("xtmpfile");
    return -1;
  }
  fclose(f);

  /* psFileName contains a postscript file of the complete document */
  f = xtmpfile(&psFileName,
	       PS_TEMPLATE_LONG, PS_TEMPLATE_SHORT,
	       TRUE);
  if (f == NULL) {
    sys_fatal("xtmpfile");
    return -1;
  }
  fclose(f);

  /* regionFileName contains a list of the images and their boxed coordinates */
  f = xtmpfile(&regionFileName,
	       REGION_TEMPLATE_LONG, REGION_TEMPLATE_SHORT,
	       TRUE);
  if (f == NULL) {
    sys_fatal("xtmpfile");
    return -1;
  }
  fclose(f);

#endif
  return 0;
}

int main(int argc, char **argv)
{
  program_name = argv[0];
  int i;
  int found=0;
  int ok=1;

  postscriptRes = get_resolution();
  i = scanArguments(argc, argv);
  setupAntiAlias();
  checkImageDir();
  makeFileName();
  while (i < argc) {
    if (argv[i][0] != '-') {
      /* found source file */
      ok = do_file(argv[i]);
      if (! ok) {
	return( 0 );
      }
      found = 1;
    }
    i++;
  }

  copyofstdoutfd=dup(stdoutfd);

  if (! found) {
    do_file("-");
  }
  if (makeTempFiles())
    return 1;
  ok = inputFile.do_image(argc, argv);
  if (ok == 0) {
    generateImages(regionFileName);
    ok = inputFile.do_html(argc, argv);
  }
  return ok;
}

static int do_file(const char *filename)
{
  FILE *fp;

  current_filename = filename;
  if (strcmp(filename, "-") == 0) {
    fp = stdin;
  } else {
    fp = fopen(filename, "r");
    if (fp == 0) {
      error("can't open `%1': %2", filename, strerror(errno));
      return 0;
    }
  }

  if (inputFile.read_file(fp)) {
  }

  if (fp != stdin)
    fclose(fp);
  current_filename = NULL;
  return 1;
}
