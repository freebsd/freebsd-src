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
#include <string.h>
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

extern char *strerror();

#include "pre-html.h"
#include "pushbackbuffer.h"
#include "html-strings.h"

#define POSTSCRIPTRES          72000   // maybe there is a better way to find this? --fixme--
#define DEFAULT_IMAGE_RES         80   // 80 pixels per inch resolution
#define DEFAULT_VERTICAL_OFFSET   45   // DEFAULT_VERTICAL_OFFSET/72 of an inch
#define IMAGE_BOARDER_PIXELS       0
#define MAX_WIDTH                  8   // inches
#define INLINE_LEADER_CHAR      '\\'

#define TRANSPARENT  "-background \"#FFF\" -transparent \"#FFF\""

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
static char *psFileName     = 0;                // name of postscript file
static char *regionFileName = 0;                // name of file containing all image regions
static char *imagePageStem  = 0;                // stem of all files containing page images
static char *image_device   = "pnmraw";
static int   image_res      = DEFAULT_IMAGE_RES;
static int   vertical_offset= DEFAULT_VERTICAL_OFFSET;
static char *image_template = 0;                // image template filename
static int   troff_arg      = 0;                // troff arg index
static char *command_prefix = 0;                // optional prefix for some installations.
static char *troff_command  = 0;
#if defined(DEBUGGING)
static int   debug          = FALSE;
static char *troffFileName  = 0;                // output of pre-html output which is sent to troff -Tps
static char *htmlFileName   = 0;                // output of pre-html output which is sent to troff -Thtml
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
 *  sys_fatal - writes a fatal error message. Taken from src/roff/groff/pipeline.c
 */

void sys_fatal (const char *s)
{
  fprintf(stderr, "%s: %s: %s", program_name, s, strerror(errno));
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
  while (head != 0) {
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
  int i=0;
  unsigned int old_used;
  int n;

  while (! feof(fp)) {
    if (tail == 0) {
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

void makeFileName ()
{
  char buffer[8192];

  sprintf(buffer, "grohtml-%d", (int)getpid());
  strcat(buffer, "-%d");
  image_template = (char *)malloc(strlen(buffer)+1);
  strcpy(image_template, buffer);
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
    writeString("\\O2\\O1\\O4\n");
  } else {
    /*
     *  postscript, therefore emit image boundaries
     */
    writeString("\\O2\\O4\n");
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
    writeString("\\O3\\O5'");
    writeString(image_template); writeString(".png'");
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
    writeString("\\O0\n");
  } else {
    // reset min/max registers
    writeString("\\O0\\O1\n");
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
  int r;
  int         i=0;

  if (t != 0) {
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
    } while (t != 0);
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
  next       = 0;
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
  while (head != 0) {
    imageItem *i = head;
    head = head->next;
    delete i;
  }
}

/*
 *  createAllPages - creates a set of images, one per page.
 */

static void createAllPages (void)
{
  char buffer[4096];

  sprintf(buffer,
	  "echo showpage | gs -q -dSAFER -sDEVICE=%s -r%d -sOutputFile=%s%%d %s - > /dev/null 2>&1 \n",
	  image_device,
	  image_res,
	  imagePageStem,
	  psFileName);
#if defined(DEBUGGING)
  fwrite(buffer, sizeof(char), strlen(buffer), stderr);
  fflush(stderr);
#endif
  system(buffer);
}

/*
 *  removeAllPages - removes all page images.
 */

static void removeAllPages (void)
{
#if !defined(DEBUGGING)
  char buffer[4096];
  int  i=1;

  do {
    sprintf(buffer, "%s%d", imagePageStem, i);
    i++;
  } while (remove(buffer) == 0);
#endif
}

/*
 *  abs - returns the absolute value.
 */

int abs (int x)
{
  if (x < 0) {
    return( -x );
  } else {
    return( x );
  }
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
    char buffer[4096];
    int  x1 = max(min(i->X1, i->X2)*image_res/POSTSCRIPTRES-1*IMAGE_BOARDER_PIXELS, 0);
    int  y1 = max((image_res*vertical_offset/72)+min(i->Y1, i->Y2)*image_res/POSTSCRIPTRES-IMAGE_BOARDER_PIXELS, 0);
    int  x2 = max(i->X1, i->X2)*image_res/POSTSCRIPTRES+1*IMAGE_BOARDER_PIXELS;
    int  y2 = (image_res*vertical_offset/72)+max(i->Y1, i->Y2)*image_res/POSTSCRIPTRES+1*IMAGE_BOARDER_PIXELS;

    sprintf(buffer,
	    "pnmcut %d %d %d %d < %s%d | pnmtopng %s > %s \n",
	    x1, y1, x2-x1+1, y2-y1+1,
	    imagePageStem,
	    i->pageNo,
	    TRANSPARENT,
	    i->imageName);
#if defined(DEBUGGING)
    fprintf(stderr, buffer);
#endif
    system(buffer);
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

  if (head == 0) {
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
  char       *name;
  int         i=0;

  if (t != 0) {
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
    } while (t != 0);
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
  char ch;

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
	ch = f->getPB();
      }
      if (f->putPB(f->getPB()) == '\n') {
	ch = f->getPB();
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
  fprintf(stream, "usage: %s troffname [-P-o vertical_image_offset] [-P-i image_resolution] [troff flags] [files]\n", program_name);
  fprintf(stream, "    vertical_image_offset (default %d/72 of an inch)\n", vertical_offset);
  fprintf(stream, "    image_resolution (default %d) pixels per inch\n", image_res);
}

/*
 *  scanArguments - scans for -P-i and -P-o arguments.
 */

int scanArguments (int argc, char **argv)
{
  int i=1;

  while (i<argc) {
    if (strncmp(argv[i], "-i", 2) == 0) {
      image_res = atoi((char *)(argv[i]+2));
    } else if (strncmp(argv[i], "-o", 2) == 0) {
      vertical_offset = atoi((char *)(argv[i]+2));
    } else if ((strcmp(argv[i], "-v") == 0)
	       || (strcmp(argv[i], "--version") == 0)) {
      extern const char *Version_string;
      printf("GNU pre-grohtml (groff) version %s\n", Version_string);
      exit(0);
    } else if ((strcmp(argv[i], "-h") == 0)
	       || (strcmp(argv[i], "--help") == 0)
	       || (strcmp(argv[i], "-?") == 0)) {
      usage(stdout);
      exit(0);
    } else if (strcmp(argv[i], "troff") == 0) {
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

static void makeTempFiles (void)
{
#if defined(DEBUGGING)
  psFileName     = "/tmp/prehtml-ps";
  regionFileName = "/tmp/prehtml-region";
  imagePageStem  = "/tmp/prehtml-page";
  troffFileName  = "/tmp/prehtml-troff";
  htmlFileName   = "/tmp/prehtml-html";
#else
  psFileName     = mktemp(xtmptemplate("-ps-"));
  regionFileName = mktemp(xtmptemplate("-regions-"));
  imagePageStem  = mktemp(xtmptemplate("-page-"));
#endif
}

/*
 *  removeTempFiles - remove the temporary files
 */

static void removeTempFiles (void)
{
#if !defined(DEBUGGING)
  remove(psFileName);
  remove(regionFileName);
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
  makeFileName();
  i = scanArguments(argc, argv);
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
  makeTempFiles();
  ok = inputFile.do_image(argc, argv);
  if (ok == 0) {
    createAllPages();
    generateImages(regionFileName);
    ok = inputFile.do_html(argc, argv);
    removeAllPages();
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
  current_filename = 0;
  return 1;
}
