// -*- C++ -*-
/* Copyright (C) 2000, 2001 Free Software Foundation, Inc.
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

#include <stdio.h>
#include <signal.h>
#include <ctype.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include "lib.h"
#include "errarg.h"
#include "error.h"
#include "stringclass.h"
#include "posix.h"
#include "defs.h"

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
#include "pushbackbuffer.h"
#include "html-strings.h"

#define POSTSCRIPTRES          72000   // maybe there is a better way to find this? --fixme--
#define DEFAULT_IMAGE_RES         80   // 80 pixels per inch resolution
#define DEFAULT_VERTICAL_OFFSET   45   // DEFAULT_VERTICAL_OFFSET/72 of an inch
#define IMAGE_BOARDER_PIXELS       0
#define MAX_WIDTH                  8   // inches
#define INLINE_LEADER_CHAR      '\\'
#define MAX_RETRIES             4096   // number of different page directory names to try before giving up

#define TRANSPARENT  "-background \"#FFF\" -transparent \"#FFF\""

#ifdef __MSDOS__
#define PAGE_TEMPLATE "pg"
#define PS_TEMPLATE "ps"
#define REGION_TEMPLATE "rg"
#else
#define PAGE_TEMPLATE "-page-"
#define PS_TEMPLATE "-ps-"
#define REGION_TEMPLATE "-regions-"
#endif

#if 0
#   define  DEBUGGING
#   define  DEBUG_HTML
#endif

#if !defined(TRUE)
#   define TRUE (1==1)
#endif
#if !defined(FALSE)
#   define FALSE (1==0)
#endif

void stop() {}

typedef enum {CENTERED, LEFT, RIGHT, INLINE} IMAGE_ALIGNMENT;

static int   stdoutfd       = 1;                // output file descriptor - normally 1 but might move
                                                // -1 means closed
static int   copyofstdoutfd =-1;                // a copy of stdout, so we can restore stdout when
                                                // writing to post-html
static char *psFileName     = NULL;             // name of postscript file
static char *regionFileName = NULL;             // name of file containing all image regions
static char *imagePageStem  = NULL;             // stem of all files containing page images
static char *image_device   = "pnmraw";
static int   image_res      = DEFAULT_IMAGE_RES;
static int   vertical_offset= DEFAULT_VERTICAL_OFFSET;
static char *image_template = NULL;             // image template filename
static int   troff_arg      = 0;                // troff arg index
static char *command_prefix = NULL;             // optional prefix for some installations.
static char *troff_command  = NULL;
static char *image_dir      = NULL;             // user specified image directory
#if defined(DEBUGGING)
static int   debug          = FALSE;
static char *troffFileName  = NULL;             // output of pre-html output which is sent to troff -Tps
static char *htmlFileName   = NULL;             // output of pre-html output which is sent to troff -Thtml
#endif


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
  fprintf(stderr, "%s: %s: %s", program_name, s, strerror(errno));
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
}

#if 0

/*
 *  if/when vsnprintf becomes available on all *NIX machines we can use this function,
 *  until then we must use the more complex function below which performs hand built
 *  %d, %s and %%.
 */

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
	np = strdup(p);
	if (np == NULL)
	  sys_fatal("strdup");
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
#else

/*
 *  lengthOfintToStr - returns the length of the proposed string value of i.
 *                     Hand built log10.
 */

int
lengthOfintToStr (int i)
{
  int n=0;

  if (i < 0)
    sys_fatal("expecting positive integer value");

  do {
    i /= 10;
    n++;
  } while (i > 0);
  return n;
}

/*
 *  intToStr - returns a string containing the positive value of i.
 *             (int i is assumed to be positive).
 */

char *
intToStr (int i)
{
  int n=lengthOfintToStr(i)+1;
  char *p = (char *)malloc(n);

  if (p == NULL)
    sys_fatal("malloc");

  if (i < 0)
    sys_fatal("expecting positive integer value");

  n--;
  p[n] = (char)0;
  do {
    n--;
    p[n] = (char)((i % 10) + (int)'0');
    i /= 10;
  } while (i > 0);
  return( p );
}

/*
 *  make_message - returns a string built from a format specifier.
 *                 This function does not use vsnprintf; it only
 *                 understands primitive %%, %s, and %d specifiers.
 */

char *
make_message (const char *fmt, ...)
{
  char *p = strdup(fmt);  /* so we can splat a nul anywhere in the string */
  char *np;
  char *l;
  char *s;
  char *num;
  int   search=0;
  va_list ap;

  if (p == NULL)
    sys_fatal("strdup");
  
  va_start(ap, fmt);
  while (p) {
    int   lenp=strlen(p);
    char *f   = strchr(p+search, '%');

    search = f-p;
    np = p;

    if (f == NULL) {
      va_end(ap);
      return p;
    }
    switch (*(f+1)) {

    case 'd':
      l = strdup(f+2);
      if (l == NULL)
	sys_fatal("strdup");
      *f = (char)0;
      num = intToStr(va_arg(ap, int));
      np = (char *)malloc(strlen(p)+strlen(num)+strlen(l)+1);
      if (np == NULL)
	sys_fatal("malloc");
      strcpy(np, p);
      strcat(np, num);
      strcat(np, l);
      search += strlen(np)-lenp;
      free(num);
      free(l);
      break;
    case 's':
      /* concat */
      l = f+2;
      if (l == NULL)
	sys_fatal("strdup");
      s = va_arg(ap, char *);
      *f = (char)0;
      np = (char *)malloc(strlen(l)+1+strlen(p)+strlen(s));
      if (np == NULL)
	sys_fatal("malloc");
      strcpy(np, p);
      strcat(np, s);
      strcat(np, l);
      search += strlen(s);
      break;
    case '%':
      /* remove one of the two % that we have seen */
      *f = (char)0;
      l = f+1;
      np = (char *)malloc(strlen(l)+1+strlen(p));
      if (np == NULL)
	sys_fatal("malloc");
      strcpy(np, p);
      strcat(np, l);
      search++;
      break;
    default:
      sys_fatal("unexpected format specifier");
      return NULL;
    }
    free(p);
    p = np;
  }
  va_end(ap);
  return NULL;
}
#endif

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
  void skip_to_newline(char_block **t, int *i);
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
 *  makeFileName - creates the image filename template.
 */

static void makeFileName (void)
{
  char *s;

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
    s = make_message("%sgrohtml-%d", image_dir, (int)getpid());
  else {
    s = make_message("%s%s", image_dir, image_template);
  }
  if (s == NULL)
    sys_fatal("make_message");

  image_template = (char *)malloc(strlen("-%d")+strlen(s)+1);
  if (image_template == NULL)
    sys_fatal("malloc");
  strcpy(image_template, s);
  strcat(image_template, "-%d");
  free(s);
}

/*
 *  checkImageDir - checks to see whether the image directory is available.
 */

static void checkImageDir (void)
{
  if ((image_dir != NULL) && (strcmp(image_dir, "") != 0))
    if (! ((mkdir(image_dir, 0700) == 0) || (errno == EEXIST))) {
      error("cannot create directory `%1'", image_dir);
      exit(1);
    }
}

/*
 *  write_end_image - ends the image. It writes out the image extents if we are using -Tps.
 */

static void write_end_image (int is_html)
{
  if (is_html) {
    /*
     *  emit image name and enable output
     */
    writeString("\\O[2]\\O[1]\\O[4]\n");
  } else {
    /*
     *  postscript, therefore emit image boundaries
     */
    writeString("\\O[2]\\O[4]\n");
  }
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
  if (pos == INLINE) {
    writeString("\\O[3]\\O[5 ");
    writeString(image_template); writeString(".png]");
  } else {
    writeString(".begin \\{\\\n");
    switch (pos) {

    case LEFT:
      writeString(".    image l ");
      break;
    case RIGHT:
      writeString(".    image r ");
      break;
    case CENTERED:
    default:
      writeString(".    image c ");
    }
    writeString(image_template); writeString(".png\n");
    if (! is_html) {
      writeString(".bp\n");
      writeString(".tl ''''\n");
    }
    writeString("\\}\n");
  }
  if (is_html) {
    writeString("\\O[0]\n");
  } else {
    // reset min/max registers
    writeString("\\O[0]\\O[1]\n");
  }
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
 *  skip_to_newline - skips all characters until a newline is seen.
 *                    The newline is also consumed.
 */

void char_buffer::skip_to_newline (char_block **t, int *i)
{
  int j=*i;

  if (*t) {
    while ((j < (*t)->used) && ((*t)->buffer[j] != '\n')) {
      j++;
    }
    if ((j < (*t)->used) && ((*t)->buffer[j] == '\n')) {
      j++;
    }
    if (j == (*t)->used) {
      *i = 0;
      if ((*t)->buffer[j-1] == '\n') {
	*t = (*t)->next;
      } else {
	*t = (*t)->next;
	skip_to_newline(t, i);
      }
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
      /*
       *  remember to check the shortest string last
       */
      if (can_see(&t, &i, HTML_IMAGE_END)) {
	write_end_image(FALSE);
	skip_to_newline(&t, &i);
      } else if (can_see(&t, &i, HTML_IMAGE_LEFT)) {
	write_start_image(LEFT, FALSE);
	skip_to_newline(&t, &i);
      } else if (can_see(&t, &i, HTML_IMAGE_RIGHT)) {
	write_start_image(RIGHT, FALSE);
	skip_to_newline(&t, &i);
      } else if (can_see(&t, &i, HTML_IMAGE_CENTERED)) {
	write_start_image(CENTERED, FALSE);
	skip_to_newline(&t, &i);
      } else {
	write_upto_newline(&t, &i, FALSE);
      }
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
 *  createAllPages - creates a set of images, one per page.
 */

static int createAllPages (void)
{
  char buffer[4096];
  char *s;
  int retries = MAX_RETRIES;

  imagePageStem = xtmptemplate(PAGE_TEMPLATE);
  strcpy(buffer, imagePageStem);

  do {
    if (mktemp(imagePageStem) == NULL) {
      sys_fatal("mktemp");
      return -1;
    }
    if (mkdir(imagePageStem, 0700) == 0) break;
    if (errno == EEXIST) {
      // directory already exists, try another name
      retries--;
      if (retries == 0) {
	// time to give up
	sys_fatal("mkdir");
	return -1;
      }
    } else {
      // another error, quit
      sys_fatal("mkdir");
      return -1;
    }      
    strcpy(imagePageStem, buffer);
  } while (1);

  s = make_message("echo showpage | "
		   "gs%s -q -dSAFER -sDEVICE=%s -r%d "
		   "-sOutputFile=%s/%%d %s -",
		   EXE_EXT,
		   image_device,
		   image_res,
		   imagePageStem,
		   psFileName);
  if (s == NULL)
    sys_fatal("make_message");
#if defined(DEBUGGING)
  fwrite(s, sizeof(char), strlen(s), stderr);
  fflush(stderr);
#endif
  html_system(s, 1);
  free(s);
  return 0;
}

/*
 *  removeAllPages - removes all page images.
 */

static void removeAllPages (void)
{
#if !defined(DEBUGGING)
  char *s=NULL;
  int  i=1;

  do {
    if (s)
      free(s);
    s = make_message("%s/%d", imagePageStem, i);
    if (s == NULL)
      sys_fatal("make_message");
    i++;
  } while (unlink(s) == 0);
  rmdir(imagePageStem);
#endif
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
 *  createImage - generates a minimal png file from the set of page images.
 */

static void createImage (imageItem *i)
{
  if (i->X1 != -1) {
    char *s;
    int  x1 = max(min(i->X1, i->X2)*image_res/POSTSCRIPTRES-1*IMAGE_BOARDER_PIXELS, 0);
    int  y1 = max((image_res*vertical_offset/72)+min(i->Y1, i->Y2)*image_res/POSTSCRIPTRES-IMAGE_BOARDER_PIXELS, 0);
    int  x2 = max(i->X1, i->X2)*image_res/POSTSCRIPTRES+1*IMAGE_BOARDER_PIXELS;
    int  y2 = (image_res*vertical_offset/72)+max(i->Y1, i->Y2)*image_res/POSTSCRIPTRES+1*IMAGE_BOARDER_PIXELS;

    s = make_message("pnmcut%s %d %d %d %d < %s/%d | pnmtopng%s %s > %s \n",
		     EXE_EXT,
		     x1, y1, x2-x1+1, y2-y1+1,
		     imagePageStem,
		     i->pageNo,
		     EXE_EXT,
		     TRANSPARENT,
		     i->imageName);
    if (s == NULL)
      sys_fatal("make_message");

#if defined(DEBUGGING)
    fprintf(stderr, s);
#endif
    html_system(s, 0);
    free(s);
#if defined(DEBUGGING)
  } else {
    fprintf(stderr, "ignoring image as x1 coord is -1\n");
    fflush(stderr);
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
  createImage(i);
}

static imageList listOfImages;  // list of images defined by the region file.

/*
 *  write_file_html - writes the buffer to stdout (troff).
 *                    It writes out the file replacing template image names with
 *                    actual image names.
 */

void char_buffer::write_file_html (void)
{
  char_block *t      =head;
  int         i=0;

  if (t != NULL) {
    stop();
    do {
      /*
       *  remember to check the shortest string last
       */
      if (can_see(&t, &i, HTML_IMAGE_END)) {
	write_end_image(TRUE);
	skip_to_newline(&t, &i);
      } else if (can_see(&t, &i, HTML_IMAGE_LEFT)) {
	write_start_image(LEFT, TRUE);
	skip_to_newline(&t, &i);
      } else if (can_see(&t, &i, HTML_IMAGE_RIGHT)) {
	write_start_image(RIGHT, TRUE);
	skip_to_newline(&t, &i);
      } else if (can_see(&t, &i, HTML_IMAGE_CENTERED)) {
	stop();
	write_start_image(CENTERED, TRUE);
	skip_to_newline(&t, &i);
      } else {
	write_upto_newline(&t, &i, TRUE);
      }
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
      int maxx   = max(f->readInt(), MAX_WIDTH*image_res);
      char *name = f->readString();
      int res    = POSTSCRIPTRES;  // --fixme--    prefer (f->readInt()) providing that troff can discover the value
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

  waitpd = wait(&status);
  if (waitpd != pid)
    sys_fatal("wait");
}

/*
 *  alterDeviceTo - if toImage is set then the arg list is altered to include
 *                     IMAGE_DEVICE and we invoke groff rather than troff.
 *                  else 
 *                     set -Thtml and troff
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
    argv[troff_arg] = troff_command;  /* use troff */
  }
}

/*
 *  do_html - sets the troff number htmlflip and
 *            writes out the buffer to troff -Thtml
 */

int char_buffer::do_html(int argc, char *argv[])
{
  int pdes[2];
  PID_T pid;

  if (pipe(pdes) < 0)
    sys_fatal("pipe");

  alterDeviceTo(argc, argv, 0);
  argv += troff_arg;   // skip all arguments up to troff/groff
  argc -= troff_arg;

#if defined(DEBUG_HTML)
  write_file_html();
  writeString("--------------- troff --------------------------\n");
  write_file_troff();
#else
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
#endif
  return( 0 );
}

/*
 *  addps4html - appends -rps4html=1 onto the command list for troff.
 */

char **addps4html (int argc, char *argv[])
{
  char **new_argv = (char **)malloc((argc+2)*sizeof(char *));
  int   i=0;

  if (new_argv == NULL)
    sys_fatal("malloc");

  while (i<argc) {
    new_argv[i] = argv[i];
    i++;
  }
  new_argv[argc] = "-rps4html=1";
  argc++;
  new_argv[argc] = NULL;
  return( new_argv );
}

/*
 *  do_image - writes out the buffer to troff -Tps
 */

int char_buffer::do_image(int argc, char *argv[])
{
  PID_T pid;
  int pdes[2];

  if (pipe(pdes) < 0)
    sys_fatal("pipe");

  alterDeviceTo(argc, argv, 1);
  argv += troff_arg;   // skip all arguments up to troff/groff
  argc -= troff_arg;
  argv = addps4html(argc, argv);
  argc++;

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
  return( 0 );
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
 *  scanArguments - scans for -P-i, -P-o, -P-D and -P-I arguments.
 */

int scanArguments (int argc, char **argv)
{
  int i=1;

  while (i<argc) {
    if (strncmp(argv[i], "-D", 2) == 0) {
      image_dir = (char *)(argv[i]+2);
    } else if (strncmp(argv[i], "-I", 2) == 0) {
      image_template = (char *)(argv[i]+2);
    } else if (strncmp(argv[i], "-i", 2) == 0) {
      image_res = atoi((char *)(argv[i]+2));
    } else if (strncmp(argv[i], "-o", 2) == 0) {
      vertical_offset = atoi((char *)(argv[i]+2));
    } else if ((strcmp(argv[i], "-v") == 0)
	       || (strcmp(argv[i], "--version") == 0)) {
      printf("GNU pre-grohtml (groff) version %s\n", Version_string);
      exit(0);
    } else if ((strcmp(argv[i], "-h") == 0)
	       || (strcmp(argv[i], "--help") == 0)
	       || (strcmp(argv[i], "-?") == 0)) {
      usage(stdout);
      exit(0);
    } else if (strcmp(argv[i], troff_command) == 0) {
      /* remember troff argument number */
      troff_arg = i;
#if defined(DEBUGGING)
    } else if (strcmp(argv[i], "-d") == 0) {
      debug = TRUE;
#endif
    } else if (argv[i][0] != '-') {
      return( i );
    }
    i++;
  }
  return( argc );
}

/*
 *  makeTempFiles - name the temporary files
 */

static int makeTempFiles (void)
{
#if defined(DEBUGGING)
  psFileName     = "/tmp/prehtml-ps";
  regionFileName = "/tmp/prehtml-region";
  imagePageStem  = "/tmp/prehtml-page";
  troffFileName  = "/tmp/prehtml-troff";
  htmlFileName   = "/tmp/prehtml-html";
#else
  int fd;

  if ((fd = mkstemp(psFileName = xtmptemplate(PS_TEMPLATE))) == -1) {
    sys_fatal("mkstemp");
    return -1;
  }
  close(fd);
  if ((fd = mkstemp(regionFileName = xtmptemplate(REGION_TEMPLATE))) == -1) {
    sys_fatal("mkstemp");
    unlink(psFileName);
    return -1;
  }
  close(fd);
#endif
  return 0;
}

/*
 *  removeTempFiles - remove the temporary files
 */

static void removeTempFiles (void)
{
#if !defined(DEBUGGING)
  unlink(psFileName);
  unlink(regionFileName);
#endif
}

/*
 *  findPrefix - finds the optional prefix to the groff utilities.
 *               It also builds the 'troff' executable name.
 */

static void findPrefix (void)
{
  command_prefix = getenv("GROFF_COMMAND_PREFIX");
  if (!command_prefix)
    command_prefix = PROG_PREFIX;
  troff_command = (char *)malloc(strlen("troff")+strlen(command_prefix)+1);
  if (troff_command == NULL)
    sys_fatal("malloc");

  strcpy(troff_command, command_prefix);
  strcat(troff_command, "troff");
}


int main(int argc, char **argv)
{
  program_name = argv[0];
  int i;
  int found=0;
  int ok=1;

  findPrefix();
  i = scanArguments(argc, argv);
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
    ok = createAllPages();
    if (ok == 0) {
      generateImages(regionFileName);
      ok = inputFile.do_html(argc, argv);
      removeAllPages();
    }
  }
  removeTempFiles();
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
