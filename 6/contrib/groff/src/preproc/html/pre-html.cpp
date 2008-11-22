// -*- C++ -*-
/* Copyright (C) 2000, 2001, 2002, 2003, 2004 Free Software Foundation, Inc.
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
Foundation, 51 Franklin St - Fifth Floor, Boston, MA 02110-1301, USA. */

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
#include "device.h"
#include "font.h"

#include <errno.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef _POSIX_VERSION
# include <sys/wait.h>
# define PID_T pid_t
#else /* not _POSIX_VERSION */
# define PID_T int
#endif /* not _POSIX_VERSION */

#include <stdarg.h>

#include "nonposix.h"

/* Establish some definitions to facilitate discrimination between
   differing runtime environments. */

#undef MAY_FORK_CHILD_PROCESS
#undef MAY_SPAWN_ASYNCHRONOUS_CHILD

#if defined(__MSDOS__) || defined(_WIN32)

// Most MS-DOS and Win32 environments will be missing the `fork' capability
// (some like Cygwin have it, but it is best avoided).

# define MAY_FORK_CHILD_PROCESS 0

// On these systems, we use `spawn...', instead of `fork' ... `exec...'.
# include <process.h>	// for `spawn...'
# include <fcntl.h>	// for attributes of pipes

# if defined(__CYGWIN__) || defined(_UWIN) || defined(_WIN32)

// These Win32 implementations allow parent and `spawn...'ed child to
// multitask asynchronously.

#  define MAY_SPAWN_ASYNCHRONOUS_CHILD 1

# else

// Others may adopt MS-DOS behaviour where parent must sleep,
// from `spawn...' until child terminates.

#  define MAY_SPAWN_ASYNCHRONOUS_CHILD 0

# endif /* not defined __CYGWIN__, _UWIN, or _WIN32 */

# if defined(DEBUGGING) && !defined(DEBUG_FILE_DIR)
/* When we are building a DEBUGGING version we need to tell pre-grohtml
   where to put intermediate files (the DEBUGGING version will preserve
   these on exit).

   On a UNIX host, we might simply use `/tmp', but MS-DOS and Win32 will
   probably not have this on all disk drives, so default to using
   `c:/temp' instead.  (Note that user may choose to override this by
   supplying a definition such as

     -DDEBUG_FILE_DIR=d:/path/to/debug/files

   in the CPPFLAGS to `make'.) */

#  define DEBUG_FILE_DIR c:/temp
# endif

#else /* not __MSDOS__ or _WIN32 */

// For non-Microsoft environments assume UNIX conventions,
// so `fork' is required and child processes are asynchronous.
# define MAY_FORK_CHILD_PROCESS 1
# define MAY_SPAWN_ASYNCHRONOUS_CHILD 1

# if defined(DEBUGGING) && !defined(DEBUG_FILE_DIR)
/* For a DEBUGGING version, on the UNIX host, we can also usually rely
   on being able to use `/tmp' for temporary file storage.  (Note that,
   as in the __MSDOS__ or _WIN32 case above, the user may override this
   by defining

     -DDEBUG_FILE_DIR=/path/to/debug/files

   in the CPPFLAGS.) */

#  define DEBUG_FILE_DIR /tmp
# endif

#endif /* not __MSDOS__ or _WIN32 */

#ifdef DEBUGGING
// For a DEBUGGING version, we need some additional macros,
// to direct the captured debug mode output to appropriately named files
// in the specified DEBUG_FILE_DIR.

# define DEBUG_TEXT(text) #text
# define DEBUG_NAME(text) DEBUG_TEXT(text)
# define DEBUG_FILE(name) DEBUG_NAME(DEBUG_FILE_DIR) "/" name
#endif

extern "C" const char *Version_string;

#include "pre-html.h"
#include "pushback.h"
#include "html-strings.h"

#define DEFAULT_LINE_LENGTH 7	// inches wide
#define DEFAULT_IMAGE_RES 100	// number of pixels per inch resolution
#define IMAGE_BOARDER_PIXELS 0
#define INLINE_LEADER_CHAR '\\'

// Don't use colour names here!  Otherwise there is a dependency on
// a file called `rgb.txt' which maps names to colours.
#define TRANSPARENT "-background rgb:f/f/f -transparent rgb:f/f/f"
#define MIN_ALPHA_BITS 0
#define MAX_ALPHA_BITS 4

#define PAGE_TEMPLATE_SHORT "pg"
#define PAGE_TEMPLATE_LONG "-page-"
#define PS_TEMPLATE_SHORT "ps"
#define PS_TEMPLATE_LONG "-ps-"
#define REGION_TEMPLATE_SHORT "rg"
#define REGION_TEMPLATE_LONG "-regions-"

#if 0
# define DEBUGGING
#endif

#if !defined(TRUE)
# define TRUE (1==1)
#endif
#if !defined(FALSE)
# define FALSE (1==0)
#endif

typedef enum {
  CENTERED, LEFT, RIGHT, INLINE
} IMAGE_ALIGNMENT;

static int postscriptRes = -1;		// postscript resolution,
					// dots per inch
static int stdoutfd = 1;		// output file descriptor -
					// normally 1 but might move
					// -1 means closed
static char *psFileName = NULL;		// name of postscript file
static char *psPageName = NULL;		// name of file containing
					// postscript current page
static char *regionFileName = NULL;	// name of file containing all
					// image regions
static char *imagePageName = NULL;	// name of bitmap image containing
					// current page
static const char *image_device = "pnmraw";
static int image_res = DEFAULT_IMAGE_RES;
static int vertical_offset = 0;
static char *image_template = NULL;	// image template filename
static char *macroset_template= NULL;	// image template passed to troff
					// by -D
static int troff_arg = 0;		// troff arg index
static char *image_dir = NULL;		// user specified image directory
static int textAlphaBits = MAX_ALPHA_BITS;
static int graphicAlphaBits = MAX_ALPHA_BITS;
static char *antiAlias = NULL;		// antialias arguments we pass to gs
static int show_progress = FALSE;	// should we display page numbers as
					// they are processed?
static int currentPageNo = -1;		// current image page number
#if defined(DEBUGGING)
static int debug = FALSE;
static char *troffFileName = NULL;	// output of pre-html output which
					// is sent to troff -Tps
static char *htmlFileName = NULL;	// output of pre-html output which
					// is sent to troff -Thtml
#endif

static char *linebuf = NULL;		// for scanning devps/DESC
static int linebufsize = 0;
static const char *image_gen = NULL;    // the `gs' program

const char *const FONT_ENV_VAR = "GROFF_FONT_PATH";
static search_path font_path(FONT_ENV_VAR, FONTPATH, 0, 0);


/*
 *  Images are generated via postscript, gs, and the pnm utilities.
 */
#define IMAGE_DEVICE "-Tps"


static int do_file(const char *filename);


/*
 *  sys_fatal - Write a fatal error message.
 *              Taken from src/roff/groff/pipeline.c.
 */

void sys_fatal(const char *s)
{
  fatal("%1: %2", s, strerror(errno));
}

/*
 *  get_line - Copy a line (w/o newline) from a file to the
 *             global line buffer.
 */

int get_line(FILE *f)
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
 *  get_resolution - Return the postscript resolution from devps/DESC.
 */

static unsigned int get_resolution(void)
{
  char *pathp;
  FILE *f;
  unsigned int res;
  f = font_path.open_file("devps/DESC", &pathp);
  a_delete pathp;
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
 *  html_system - A wrapper for system().
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
 *  make_message - Create a string via malloc and place the result of the
 *                 va args into string.  Finally the new string is returned.
 *                 Taken from man page of printf(3).
 */

char *make_message(const char *fmt, ...)
{
  /* Guess we need no more than 100 bytes. */
  int n, size = 100;
  char *p;
  char *np;
  va_list ap;
  if ((p = (char *)malloc(size)) == NULL)
    return NULL;
  while (1) {
    /* Try to print in the allocated space. */
    va_start(ap, fmt);
    n = vsnprintf(p, size, fmt, ap);
    va_end(ap);
    /* If that worked, return the string. */
    if (n > -1 && n < size - 1) { /* glibc 2.1 and pre-ANSI C 99 */
      if (size > n + 1) {
	np = strsave(p);
	free(p);
	return np;
      }
      return p;
    }
    /* Else try again with more space. */
    else		/* glibc 2.0 */
      size *= 2;	/* twice the old size */
    if ((np = (char *)realloc(p, size)) == NULL) {
      free(p);		/* realloc failed, free old, p. */
      return NULL;
    }
    p = np;		/* use realloc'ed, p */
  }
}

/*
 *  the class and methods for retaining ascii text
 */

struct char_block {
  enum { SIZE = 256 };
  char buffer[SIZE];
  int used;
  char_block *next;

  char_block();
};

/*
 *  char_block - Constructor.  Set the, used, and, next, fields to zero.
 */

char_block::char_block()
: used(0), next(0)
{
  for (int i = 0; i < SIZE; i++)
    buffer[i] = 0;
}

class char_buffer {
public:
  char_buffer();
  ~char_buffer();
  int read_file(FILE *fp);
  int do_html(int argc, char *argv[]);
  int do_image(int argc, char *argv[]);
  void emit_troff_output(int device_format_selector);
  void write_upto_newline(char_block **t, int *i, int is_html);
  int can_see(char_block **t, int *i, const char *string);
  int skip_spaces(char_block **t, int *i);
  void skip_until_newline(char_block **t, int *i);
private:
  char_block *head;
  char_block *tail;
  int run_output_filter(int device_format_selector, int argc, char *argv[]);
};

/*
 *  char_buffer - Constructor.
 */

char_buffer::char_buffer()
: head(0), tail(0)
{
}

/*
 *  char_buffer - Destructor.  Throw away the whole buffer list.
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
 *  read_file - Read in a complete file, fp, placing the contents inside
 *              char_blocks.
 */

int char_buffer::read_file(FILE *fp)
{
  int n;
  while (!feof(fp)) {
    if (tail == NULL) {
      tail = new char_block;
      head = tail;
    }
    else {
      if (tail->used == char_block::SIZE) {
	tail->next = new char_block;
	tail = tail->next;
      }
    }
    // at this point we have a tail which is ready for the next SIZE
    // bytes of the file
    n = fread(tail->buffer, sizeof(char), char_block::SIZE-tail->used, fp);
    if (n <= 0)
      // error
      return 0;
    else
      tail->used += n * sizeof(char);
  }
  return 1;
}

/*
 *  writeNbytes - Write n bytes to stdout.
 */

static void writeNbytes(const char *s, int l)
{
  int n = 0;
  int r;

  while (n < l) {
    r = write(stdoutfd, s, l - n);
    if (r < 0)
      sys_fatal("write");
    n += r;
    s += r;
  }
}

/*
 *  writeString - Write a string to stdout.
 */

static void writeString(const char *s)
{
  writeNbytes(s, strlen(s));
}

/*
 *  makeFileName - Create the image filename template
 *                 and the macroset image template.
 */

static void makeFileName(void)
{
  if ((image_dir != NULL) && (strchr(image_dir, '%') != NULL)) {
    error("cannot use a `%%' within the image directory name");
    exit(1);
  }

  if ((image_template != NULL) && (strchr(image_template, '%') != NULL)) {
    error("cannot use a `%%' within the image template");
    exit(1);
  }

  if (image_dir == NULL)
    image_dir = (char *)"";
  else if (strlen(image_dir) > 0
	   && image_dir[strlen(image_dir) - 1] != '/') {
    image_dir = make_message("%s/", image_dir);
    if (image_dir == NULL)
      sys_fatal("make_message");
  }

  if (image_template == NULL)
    macroset_template = make_message("%sgrohtml-%d", image_dir,
				     (int)getpid());
  else
    macroset_template = make_message("%s%s", image_dir, image_template);

  if (macroset_template == NULL)
    sys_fatal("make_message");

  image_template =
    (char *)malloc(strlen("-%d") + strlen(macroset_template) + 1);
  if (image_template == NULL)
    sys_fatal("malloc");
  strcpy(image_template, macroset_template);
  strcat(image_template, "-%d");
}

/*
 *  setupAntiAlias - Set up the antialias string, used when we call gs.
 */

static void setupAntiAlias(void)
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
 *  checkImageDir - Check whether the image directory is available.
 */

static void checkImageDir(void)
{
  if (image_dir != NULL && strcmp(image_dir, "") != 0)
    if (!(mkdir(image_dir, 0777) == 0 || errno == EEXIST)) {
      error("cannot create directory `%1'", image_dir);
      exit(1);
    }
}

/*
 *  write_end_image - End the image.  Write out the image extents if we
 *                    are using -Tps.
 */

static void write_end_image(int is_html)
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
 *  write_start_image - Write troff code which will:
 *
 *                      (i)  disable html output for the following image
 *                      (ii) reset the max/min x/y registers during postscript
 *                           rendering.
 */

static void write_start_image(IMAGE_ALIGNMENT pos, int is_html)
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
  writeString(image_template);
  writeString(".png]");
  if (is_html)
    writeString("\\O[0]\\O[3]");
  else
    // reset min/max registers
    writeString("\\O[1]\\O[3]");
}

/*
 *  write_upto_newline - Write the contents of the buffer until a newline
 *                       is seen.  Check for HTML_IMAGE_INLINE_BEGIN and
 *                       HTML_IMAGE_INLINE_END; process them if they are
 *                       present.
 */

void char_buffer::write_upto_newline(char_block **t, int *i, int is_html)
{
  int j = *i;

  if (*t) {
    while (j < (*t)->used
	   && (*t)->buffer[j] != '\n'
	   && (*t)->buffer[j] != INLINE_LEADER_CHAR)
      j++;
    if (j < (*t)->used
	&& (*t)->buffer[j] == '\n')
      j++;
    writeNbytes((*t)->buffer + (*i), j - (*i));
    if ((*t)->buffer[j] == INLINE_LEADER_CHAR) {
      if (can_see(t, &j, HTML_IMAGE_INLINE_BEGIN))
	write_start_image(INLINE, is_html);
      else if (can_see(t, &j, HTML_IMAGE_INLINE_END))
	write_end_image(is_html);
      else {
	if (j < (*t)->used) {
	  *i = j;
	  j++;
	  writeNbytes((*t)->buffer + (*i), j - (*i));
	}
      }
    }
    if (j == (*t)->used) {
      *i = 0;
      *t = (*t)->next;
      if (*t && (*t)->buffer[j - 1] != '\n')
	write_upto_newline(t, i, is_html);
    }
    else
      // newline was seen
      *i = j;
  }
}

/*
 *  can_see - Return TRUE if we can see string in t->buffer[i] onwards.
 */

int char_buffer::can_see(char_block **t, int *i, const char *str)
{
  int j = 0;
  int l = strlen(str);
  int k = *i;
  char_block *s = *t;

  while (s) {
    while (k < s->used && j < l && s->buffer[k] == str[j]) {
      j++;
      k++;
    }
    if (j == l) {
      *i = k;
      *t = s;
      return TRUE;
    }
    else if (k < s->used && s->buffer[k] != str[j])
      return( FALSE );
    s = s->next;
    k = 0;
  }
  return FALSE;
}

/*
 *  skip_spaces - Return TRUE if we have not run out of data.
 *                Consume spaces also.
 */

int char_buffer::skip_spaces(char_block **t, int *i)
{
  char_block *s = *t;
  int k = *i;

  while (s) {
    while (k < s->used && isspace(s->buffer[k]))
      k++;
    if (k == s->used) {
      k = 0;
      s = s->next;
    }
    else {
      *i = k;
      return TRUE;
    }
  }
  return FALSE;
}

/*
 *  skip_until_newline - Skip all characters until a newline is seen.
 *                       The newline is not consumed.
 */

void char_buffer::skip_until_newline(char_block **t, int *i)
{
  int j = *i;

  if (*t) {
    while (j < (*t)->used && (*t)->buffer[j] != '\n')
      j++;
    if (j == (*t)->used) {
      *i = 0;
      *t = (*t)->next;
      skip_until_newline(t, i);
    }
    else
      // newline was seen
      *i = j;
  }
}

#define DEVICE_FORMAT(filter) (filter == HTML_OUTPUT_FILTER)
#define HTML_OUTPUT_FILTER     0
#define IMAGE_OUTPUT_FILTER    1
#define OUTPUT_STREAM(name)   creat((name), S_IWUSR | S_IRUSR)
#define PS_OUTPUT_STREAM      OUTPUT_STREAM(psFileName)
#define REGION_OUTPUT_STREAM  OUTPUT_STREAM(regionFileName)

/*
 *  emit_troff_output - Write formatted buffer content to the troff
 *                      post-processor data pipeline.
 */

void char_buffer::emit_troff_output(int device_format_selector)
{
  // Handle output for BOTH html and image device formats
  // if `device_format_selector' is passed as
  //
  //   HTML_FORMAT(HTML_OUTPUT_FILTER)
  //     Buffer data is written to the output stream
  //     with template image names translated to actual image names.
  //
  //   HTML_FORMAT(IMAGE_OUTPUT_FILTER)
  //     Buffer data is written to the output stream
  //     with no translation, for image file creation in the post-processor.

  int idx = 0;
  char_block *element = head;

  while (element != NULL)
    write_upto_newline(&element, &idx, device_format_selector);

#if 0
  if (close(stdoutfd) < 0)
    sys_fatal ("close");

  // now we grab fd=1 so that the next pipe cannot use fd=1
  if (stdoutfd == 1) {
    if (dup(2) != stdoutfd)
      sys_fatal ("dup failed to use fd=1");
  }
#endif /* 0 */
}

/*
 *  The image class remembers the position of all images in the
 *  postscript file and assigns names for each image.
 */

struct imageItem {
  imageItem *next;
  int X1;
  int Y1;
  int X2;
  int Y2;
  char *imageName;
  int resolution;
  int maxx;
  int pageNo;

  imageItem(int x1, int y1, int x2, int y2,
	    int page, int res, int max_width, char *name);
  ~imageItem();
};

/*
 *  imageItem - Constructor.
 */

imageItem::imageItem(int x1, int y1, int x2, int y2,
		     int page, int res, int max_width, char *name)
{
  X1 = x1;
  Y1 = y1;
  X2 = x2;
  Y2 = y2;
  pageNo = page;
  resolution = res;
  maxx = max_width;
  imageName = name;
  next = NULL;
}

/*
 *  imageItem - Destructor.
 */

imageItem::~imageItem()
{
  if (imageName)
    free(imageName);
}

/*
 *  imageList - A class containing a list of imageItems.
 */

class imageList {
private:
  imageItem *head;
  imageItem *tail;
  int count;
public:
  imageList();
  ~imageList();
  void add(int x1, int y1, int x2, int y2,
	   int page, int res, int maxx, char *name);
  void createImages(void);
  int createPage(int pageno);
  void createImage(imageItem *i);
  int getMaxX(int pageno);
};

/*
 *  imageList - Constructor.
 */

imageList::imageList()
: head(0), tail(0), count(0)
{
}

/*
 *  imageList - Destructor.
 */

imageList::~imageList()
{
  while (head != NULL) {
    imageItem *i = head;
    head = head->next;
    delete i;
  }
}

/*
 *  createPage - Create one image of, page pageno, from the postscript file.
 */

int imageList::createPage(int pageno)
{
  char *s;

  if (currentPageNo == pageno)
    return 0;

  if (currentPageNo >= 1) {
    /*
     *  We need to unlink the files which change each time a new page is
     *  processed.  The final unlink is done by xtmpfile when pre-grohtml
     *  exits.
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
		   "%s%s -q -dBATCH -dSAFER "
		   "-dDEVICEHEIGHTPOINTS=792 "
		   "-dDEVICEWIDTHPOINTS=%d -dFIXEDMEDIA=true "
		   "-sDEVICE=%s -r%d %s "
		   "-sOutputFile=%s %s -\n",
		   image_gen,
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
  free(s);
  currentPageNo = pageno;
  return 0;
}

/*
 *  min - Return the minimum of two numbers.
 */

int min(int x, int y)
{
  if (x < y)
    return x;
  else
    return y;
}

/*
 *  max - Return the maximum of two numbers.
 */

int max(int x, int y)
{
  if (x > y)
    return x;
  else
    return y;
}

/*
 *  getMaxX - Return the largest right-hand position for any image
 *            on, pageno.
 */

int imageList::getMaxX(int pageno)
{
  imageItem *h = head;
  int x = postscriptRes * DEFAULT_LINE_LENGTH;

  while (h != NULL) {
    if (h->pageNo == pageno)
      x = max(h->X2, x);
    h = h->next;
  }
  return x;
}

/*
 *  createImage - Generate a minimal png file from the set of page images.
 */

void imageList::createImage(imageItem *i)
{
  if (i->X1 != -1) {
    char *s;
    int x1 = max(min(i->X1, i->X2) * image_res / postscriptRes
		   - IMAGE_BOARDER_PIXELS,
		 0);
    int y1 = max(image_res * vertical_offset / 72
		   + min(i->Y1, i->Y2) * image_res / postscriptRes
		   - IMAGE_BOARDER_PIXELS,
		 0);
    int x2 = max(i->X1, i->X2) * image_res / postscriptRes
	     + IMAGE_BOARDER_PIXELS;
    int y2 = image_res * vertical_offset / 72
	     + max(i->Y1, i->Y2) * image_res / postscriptRes
	     + 1 + IMAGE_BOARDER_PIXELS;
    if (createPage(i->pageNo) == 0) {
      s = make_message("pnmcut%s %d %d %d %d < %s "
		       "| pnmcrop -quiet | pnmtopng%s %s > %s\n",
		       EXE_EXT,
		       x1, y1, x2 - x1 + 1, y2 - y1 + 1,
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
      free(s);
    }
    else {
      fprintf(stderr, "failed to generate image of page %d\n", i->pageNo);
      fflush(stderr);
    }
#if defined(DEBUGGING)
  }
  else {
    if (debug) {
      fprintf(stderr, "ignoring image as x1 coord is -1\n");
      fflush(stderr);
    }
#endif
  }
}

/*
 *  add - Add an image description to the imageList.
 */

void imageList::add(int x1, int y1, int x2, int y2,
		    int page, int res, int maxx, char *name)
{
  imageItem *i = new imageItem(x1, y1, x2, y2, page, res, maxx, name);

  if (head == NULL) {
    head = i;
    tail = i;
  }
  else {
    tail->next = i;
    tail = i;
  }
}

/*
 *  createImages - For each image descriptor on the imageList,
 *                 create the actual image.
 */

void imageList::createImages(void)
{
  imageItem *h = head;

  while (h != NULL) {
    createImage(h);
    h = h->next;
  }
}

static imageList listOfImages;	// List of images defined by the region file.

/*
 *  generateImages - Parse the region file and generate images
 *                   from the postscript file.  The region file
 *                   contains the x1,y1--x2,y2 extents of each
 *                   image.
 */

static void generateImages(char *region_file_name)
{
  pushBackBuffer *f=new pushBackBuffer(region_file_name);

  while (f->putPB(f->getPB()) != eof) {
    if (f->isString("grohtml-info:page")) {
      int page = f->readInt();
      int x1 = f->readInt();
      int y1 = f->readInt();
      int x2 = f->readInt();
      int y2 = f->readInt();
      int maxx = f->readInt();
      char *name = f->readString();
      int res = postscriptRes;
      listOfImages.add(x1, y1, x2, y2, page, res, maxx, name);
      while (f->putPB(f->getPB()) != '\n'
	     && f->putPB(f->getPB()) != eof)
	(void)f->getPB();
      if (f->putPB(f->getPB()) == '\n')
	(void)f->getPB();
    }
    else {
      /* Write any error messages out to the user. */
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
 *  set_redirection - Set up I/O Redirection for handle, was, to refer to
 *                    stream on handle, willbe.
 */

static void set_redirection(int was, int willbe)
{
  // Nothing to do if `was' and `willbe' already have same handle.
  if (was != willbe) {
    // Otherwise attempt the specified redirection.
    if (dup2 (willbe, was) < 0) {
      // Redirection failed, so issue diagnostic and bail out.
      fprintf(stderr, "failed to replace fd=%d with %d\n", was, willbe);
      if (willbe == STDOUT_FILENO)
	fprintf(stderr,
		"likely that stdout should be opened before %d\n", was);
      sys_fatal("dup2");
    }

    // When redirection has been successfully completed assume redundant
    // handle `willbe' is no longer required, so close it.
    if (close(willbe) < 0)
      // Issue diagnostic if `close' fails.
      sys_fatal("close");
  }
}

/*
 *  save_and_redirect - Get duplicate handle for stream, was, then
 *                      redirect, was, to refer to, willbe.
 */

static int save_and_redirect(int was, int willbe)
{
  if (was == willbe)
    // No redirection specified so don't do anything but silently bailing out.
    return (was);

  // Proceeding with redirection so first save and verify our duplicate
  // handle for `was'.
  int saved = dup(was);
  if (saved < 0) {
    fprintf(stderr, "unable to get duplicate handle for %d\n", was);
    sys_fatal("dup");
  }

  // Duplicate handle safely established so complete redirection.
  set_redirection(was, willbe);

  // Finally return the saved duplicate descriptor for the
  // original `was' stream.
  return saved;
}

/*
 *  alterDeviceTo - If, toImage, is set
 *                     the argument list is altered to include
 *                     IMAGE_DEVICE and we invoke groff rather than troff.
 *                  Else
 *                     set -Thtml and groff.
 */

static void alterDeviceTo(int argc, char *argv[], int toImage)
{
  int i = 0;

  if (toImage) {
    while (i < argc) {
      if (strcmp(argv[i], "-Thtml") == 0)
	argv[i] = (char *)IMAGE_DEVICE;
      i++;
    }
    argv[troff_arg] = (char *)"groff";	/* rather than troff */
  }
  else {
    while (i < argc) {
      if (strcmp(argv[i], IMAGE_DEVICE) == 0)
	argv[i] = (char *)"-Thtml";
      i++;
    }
    argv[troff_arg] = (char *)"groff";	/* use groff -Z */
  }
}

/*
 *  addZ - Append -Z onto the command list for groff.
 */

char **addZ(int argc, char *argv[])
{
  char **new_argv = (char **)malloc((argc + 2) * sizeof(char *));
  int i = 0;

  if (new_argv == NULL)
    sys_fatal("malloc");

  if (argc > 0) {
    new_argv[i] = argv[i];
    i++;
  }
  new_argv[i] = (char *)"-Z";
  while (i < argc) {
    new_argv[i + 1] = argv[i];
    i++;
  }
  argc++;
  new_argv[argc] = NULL;
  return new_argv;
}

/*
 *  addRegDef - Append a defined register or string onto the command
 *              list for troff.
 */

char **addRegDef(int argc, char *argv[], const char *numReg)
{
  char **new_argv = (char **)malloc((argc + 2) * sizeof(char *));
  int i = 0;

  if (new_argv == NULL)
    sys_fatal("malloc");

  while (i < argc) {
    new_argv[i] = argv[i];
    i++;
  }
  new_argv[argc] = strsave(numReg);
  argc++;
  new_argv[argc] = NULL;
  return new_argv;
}

/*
 *  dump_args - Display the argument list.
 */

void dump_args(int argc, char *argv[])
{
  fprintf(stderr, "  %d arguments:", argc);
  for (int i = 0; i < argc; i++)
    fprintf(stderr, " %s", argv[i]);
  fprintf(stderr, "\n");
}

int char_buffer::run_output_filter(int filter, int /* argc */, char **argv)
{
  int pipedes[2];
  PID_T child_pid;
  int status;

  if (pipe(pipedes) < 0)
    sys_fatal("pipe");

#if MAY_FORK_CHILD_PROCESS
  // This is the UNIX process model.  To invoke our post-processor,
  // we must `fork' the current process.

  if ((child_pid = fork()) < 0)
    sys_fatal("fork");

  else if (child_pid == 0) {
    // This is the child process fork.  We redirect its `stdin' stream
    // to read data emerging from our pipe.  There is no point in saving,
    // since we won't be able to restore later!

    set_redirection(STDIN_FILENO, pipedes[0]);

    // The parent process will be writing this data, so we should release
    // the child's writeable handle on the pipe, since we have no use for it.

    if (close(pipedes[1]) < 0)
      sys_fatal("close");

    // The IMAGE_OUTPUT_FILTER needs special output redirection...

    if (filter == IMAGE_OUTPUT_FILTER) {
      // with BOTH `stdout' AND `stderr' diverted to files.

      set_redirection(STDOUT_FILENO, PS_OUTPUT_STREAM);
      set_redirection(STDERR_FILENO, REGION_OUTPUT_STREAM);
    }

    // Now we are ready to launch the output filter.

    execvp(argv[0], argv);

    // If we get to here then the `exec...' request for the output filter
    // failed.  Diagnose it and bail out.

    error("couldn't exec %1: %2", argv[0], strerror(errno), ((char *)0));
    fflush(stderr);	// just in case error() didn't
    exit(1);
  }

  else {
    // This is the parent process fork.  We will be writing data to the
    // filter pipeline, and the child will be reading it.  We have no further
    // use for our read handle on the pipe, and should close it.

    if (close(pipedes[0]) < 0)
      sys_fatal("close");

    // Now we redirect the `stdout' stream to the inlet end of the pipe,
    // and push out the appropiately formatted data to the filter.

    pipedes[1] = save_and_redirect(STDOUT_FILENO, pipedes[1]);
    emit_troff_output(DEVICE_FORMAT(filter));

    // After emitting all the data we close our connection to the inlet
    // end of the pipe so the child process will detect end of data.

    set_redirection(STDOUT_FILENO, pipedes[1]);

    // Finally, we must wait for the child process to complete.

    if (WAIT(&status, child_pid, _WAIT_CHILD) != child_pid)
      sys_fatal("wait");
  }

#elif MAY_SPAWN_ASYNCHRONOUS_CHILD

  // We do not have `fork', (or we prefer not to use it),
  // but asynchronous processes are allowed, passing data through pipes.
  // This should be ok for most Win32 systems and is preferred to `fork'
  // for starting child processes under Cygwin.

  // Before we start the post-processor we bind its inherited `stdin'
  // stream to the readable end of our pipe, saving our own `stdin' stream
  // in `pipedes[0]'.

  pipedes[0] = save_and_redirect(STDIN_FILENO, pipedes[0]);

  // for the Win32 model,
  // we need special provision for saving BOTH `stdout' and `stderr'.

  int saved_stdout = dup(STDOUT_FILENO);
  int saved_stderr = STDERR_FILENO;

  // The IMAGE_OUTPUT_FILTER needs special output redirection...

  if (filter == IMAGE_OUTPUT_FILTER) {
    // with BOTH `stdout' AND `stderr' diverted to files while saving a
    // duplicate handle for `stderr'.

    set_redirection(STDOUT_FILENO, PS_OUTPUT_STREAM);
    saved_stderr = save_and_redirect(STDERR_FILENO, REGION_OUTPUT_STREAM);
  }

  // We then use an asynchronous spawn request to start the post-processor.

  if ((child_pid = spawnvp(_P_NOWAIT, argv[0], argv)) < 0) {
    // Should the spawn request fail we issue a diagnostic and bail out.

    error("cannot spawn %1: %2", argv[0], strerror(errno), ((char *)0));
    exit(1);
  }

  // Once the post-processor has been started we revert our `stdin'
  // to its original saved source, which also closes the readable handle
  // for the pipe.

  set_redirection(STDIN_FILENO, pipedes[0]);

  // if we redirected `stderr', for use by the image post-processor,
  // then we also need to reinstate its original assignment.

  if (filter == IMAGE_OUTPUT_FILTER)
    set_redirection(STDERR_FILENO, saved_stderr);

  // Now we redirect the `stdout' stream to the inlet end of the pipe,
  // and push out the appropiately formatted data to the filter.

  set_redirection(STDOUT_FILENO, pipedes[1]);
  emit_troff_output(DEVICE_FORMAT(filter));

  // After emitting all the data we close our connection to the inlet
  // end of the pipe so the child process will detect end of data.

  set_redirection(STDOUT_FILENO, saved_stdout);

  // And finally, we must wait for the child process to complete.

  if (WAIT(&status, child_pid, _WAIT_CHILD) != child_pid)
    sys_fatal("wait");

#else /* can't do asynchronous pipes! */

  // TODO: code to support an MS-DOS style process model
  //        should go here

#endif /* MAY_FORK_CHILD_PROCESS or MAY_SPAWN_ASYNCHRONOUS_CHILD */

  return 0;
}

/*
 *  do_html - Set the troff number htmlflip and
 *            write out the buffer to troff -Thtml.
 */

int char_buffer::do_html(int argc, char *argv[])
{
  string s;

  alterDeviceTo(argc, argv, 0);
  argv += troff_arg;		// skip all arguments up to groff
  argc -= troff_arg;
  argv = addZ(argc, argv);
  argc++;

  s = "-dwww-image-template=";
  s += macroset_template;	// do not combine these statements,
				// otherwise they will not work
  s += '\0';                	// the trailing `\0' is ignored
  argv = addRegDef(argc, argv, s.contents());
  argc++;

#if defined(DEBUGGING)
# define HTML_DEBUG_STREAM  OUTPUT_STREAM(htmlFileName)
  // slight security risk so only enabled if compiled with defined(DEBUGGING)
  if (debug) {
    int saved_stdout = save_and_redirect(STDOUT_FILENO, HTML_DEBUG_STREAM);
    emit_troff_output(DEVICE_FORMAT(HTML_OUTPUT_FILTER));
    set_redirection(STDOUT_FILENO, saved_stdout);
  }
#endif

  return run_output_filter(HTML_OUTPUT_FILTER, argc, argv);
}

/*
 *  do_image - Write out the buffer to troff -Tps.
 */

int char_buffer::do_image(int argc, char *argv[])
{
  string s;

  alterDeviceTo(argc, argv, 1);
  argv += troff_arg;		// skip all arguments up to troff/groff
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

#if defined(DEBUGGING)
# define IMAGE_DEBUG_STREAM  OUTPUT_STREAM(troffFileName)
  // slight security risk so only enabled if compiled with defined(DEBUGGING)
  if (debug) {
    int saved_stdout = save_and_redirect(STDOUT_FILENO, IMAGE_DEBUG_STREAM);
    emit_troff_output(DEVICE_FORMAT(IMAGE_OUTPUT_FILTER));
    set_redirection(STDOUT_FILENO, saved_stdout);
  }
#endif

  return run_output_filter(IMAGE_OUTPUT_FILTER, argc, argv);
}

static char_buffer inputFile;

/*
 *  usage - Emit usage arguments.
 */

static void usage(FILE *stream)
{
  fprintf(stream,
	  "usage: %s troffname [-Iimage_name] [-Dimage_directory]\n"
	  "       [-P-o vertical_image_offset] [-P-i image_resolution]\n"
	  "       [troff flags]\n",
	  program_name);
  fprintf(stream,
	  "    vertical_image_offset (default %d/72 of an inch)\n",
	  vertical_offset);
  fprintf(stream,
	  "    image_resolution (default %d) pixels per inch\n",
	  image_res);
  fprintf(stream,
	  "    image_name is the name of the stem for all images\n"
	  "    (default is grohtml-<pid>)\n");
  fprintf(stream,
	  "    place all png files into image_directory\n");
}

/*
 *  scanArguments - Scan for all arguments including -P-i, -P-o, -P-D,
 *                  and -P-I.  Return the argument index of the first
 *                  non-option.
 */

static int scanArguments(int argc, char **argv)
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
  while ((c = getopt_long(argc, argv, "+a:bdD:F:g:hi:I:j:lno:prs:S:v",
			  long_options, NULL))
	 != EOF)
    switch(c) {
    case 'a':
      textAlphaBits = min(max(MIN_ALPHA_BITS, atoi(optarg)),
			  MAX_ALPHA_BITS);
      if (textAlphaBits == 3) {
	error("cannot use 3 bits of antialiasing information");
	exit(1);
      }
      break;
    case 'b':
      // handled by post-grohtml (set background color to white)
      break;
    case 'd':
#if defined(DEBUGGING)
      debug = TRUE;
#endif
      break;
    case 'D':
      image_dir = optarg;
      break;
    case 'F':
      font_path.command_line_dir(optarg);
      break;
    case 'g':
      graphicAlphaBits = min(max(MIN_ALPHA_BITS, atoi(optarg)),
			     MAX_ALPHA_BITS);
      if (graphicAlphaBits == 3) {
	error("cannot use 3 bits of antialiasing information");
	exit(1);
      }
      break;
    case 'h':
      // handled by post-grohtml
      break;
    case 'i':
      image_res = atoi(optarg);
      break;
    case 'I':
      image_template = optarg;
      break;
    case 'j':
      // handled by post-grohtml (set job name for multiple file output)
      break;
    case 'l':
      // handled by post-grohtml (no automatic section links)
      break;
    case 'n':
      // handled by post-grohtml (generate simple heading anchors)
      break;
    case 'o':
      vertical_offset = atoi(optarg);
      break;
    case 'p':
      show_progress = TRUE;
      break;
    case 'r':
      // handled by post-grohtml (no header and footer lines)
      break;
    case 's':
      // handled by post-grohtml (use font size n as the html base font size)
      break;
    case 'S':
      // handled by post-grohtml (set file split level)
      break;
    case 'v':
      printf("GNU pre-grohtml (groff) version %s\n", Version_string);
      exit(0);
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
 *  makeTempFiles - Name the temporary files.
 */

static int makeTempFiles(void)
{
#if defined(DEBUGGING)
  psFileName = DEBUG_FILE("prehtml-ps");
  regionFileName = DEBUG_FILE("prehtml-region");
  imagePageName = DEBUG_FILE("prehtml-page");
  psPageName = DEBUG_FILE("prehtml-psn");
  troffFileName = DEBUG_FILE("prehtml-troff");
  htmlFileName = DEBUG_FILE("prehtml-html");
#else /* not DEBUGGING */
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

#endif /* not DEBUGGING */
  return 0;
}

int main(int argc, char **argv)
{
  program_name = argv[0];
  int i;
  int found = 0;
  int ok = 1;

#ifdef CAPTURE_MODE
  FILE *dump;
  fprintf(stderr, "%s: invoked with %d arguments ...\n", argv[0], argc);
  for (i = 0; i < argc; i++)
    fprintf(stderr, "%2d: %s\n", i, argv[i]);
  if ((dump = fopen(DEBUG_FILE("pre-html-data"), "wb")) != NULL) {
    while((i = fgetc(stdin)) >= 0)
      fputc(i, dump);
    fclose(dump);
  }
  exit(1);
#endif /* CAPTURE_MODE */
  device = "html";
  if (!font::load_desc())
    fatal("cannot find devhtml/DESC exiting");
  image_gen = font::image_generator;
  if (image_gen == NULL || (strcmp(image_gen, "") == 0))
    fatal("devhtml/DESC must set the image_generator field, exiting");
  postscriptRes = get_resolution();
  i = scanArguments(argc, argv);
  setupAntiAlias();
  checkImageDir();
  makeFileName();
  while (i < argc) {
    if (argv[i][0] != '-') {
      /* found source file */
      ok = do_file(argv[i]);
      if (!ok)
	return 0;
      found = 1;
    }
    i++;
  }

  if (!found)
    do_file("-");
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
  if (strcmp(filename, "-") == 0)
    fp = stdin;
  else {
    fp = fopen(filename, "r");
    if (fp == 0) {
      error("can't open `%1': %2", filename, strerror(errno));
      return 0;
    }
  }

  if (inputFile.read_file(fp)) {
    // XXX
  }

  if (fp != stdin)
    fclose(fp);
  current_filename = NULL;
  return 1;
}
