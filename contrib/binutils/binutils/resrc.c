/* resrc.c -- read and write Windows rc files.
   Copyright 1997, 1998, 1999, 2000, 2001, 2002 Free Software Foundation, Inc.
   Written by Ian Lance Taylor, Cygnus Support.

   This file is part of GNU Binutils.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

/* This file contains functions that read and write Windows rc files.
   These are text files that represent resources.  */

#include "bfd.h"
#include "bucomm.h"
#include "libiberty.h"
#include "safe-ctype.h"
#include "windres.h"

#include <assert.h>
#include <errno.h>
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#else /* ! HAVE_SYS_WAIT_H */
#if ! defined (_WIN32) || defined (__CYGWIN__)
#ifndef WIFEXITED
#define WIFEXITED(w)	(((w)&0377) == 0)
#endif
#ifndef WIFSIGNALED
#define WIFSIGNALED(w)	(((w)&0377) != 0177 && ((w)&~0377) == 0)
#endif
#ifndef WTERMSIG
#define WTERMSIG(w)	((w) & 0177)
#endif
#ifndef WEXITSTATUS
#define WEXITSTATUS(w)	(((w) >> 8) & 0377)
#endif
#else /* defined (_WIN32) && ! defined (__CYGWIN__) */
#ifndef WIFEXITED
#define WIFEXITED(w)	(((w) & 0xff) == 0)
#endif
#ifndef WIFSIGNALED
#define WIFSIGNALED(w)	(((w) & 0xff) != 0 && ((w) & 0xff) != 0x7f)
#endif
#ifndef WTERMSIG
#define WTERMSIG(w)	((w) & 0x7f)
#endif
#ifndef WEXITSTATUS
#define WEXITSTATUS(w)	(((w) & 0xff00) >> 8)
#endif
#endif /* defined (_WIN32) && ! defined (__CYGWIN__) */
#endif /* ! HAVE_SYS_WAIT_H */

#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif

#if defined (_WIN32) && ! defined (__CYGWIN__)
#define popen _popen
#define pclose _pclose
#endif

/* The default preprocessor.  */

#define DEFAULT_PREPROCESSOR "gcc -E -xc -DRC_INVOKED"

/* We read the directory entries in a cursor or icon file into
   instances of this structure.  */

struct icondir
{
  /* Width of image.  */
  unsigned char width;
  /* Height of image.  */
  unsigned char height;
  /* Number of colors in image.  */
  unsigned char colorcount;
  union
  {
    struct
    {
      /* Color planes.  */
      unsigned short planes;
      /* Bits per pixel.  */
      unsigned short bits;
    } icon;
    struct
    {
      /* X coordinate of hotspot.  */
      unsigned short xhotspot;
      /* Y coordinate of hotspot.  */
      unsigned short yhotspot;
    } cursor;
  } u;
  /* Bytes in image.  */
  unsigned long bytes;
  /* File offset of image.  */
  unsigned long offset;
};

/* The name of the rc file we are reading.  */

char *rc_filename;

/* The line number in the rc file.  */

int rc_lineno;

/* The pipe we are reading from, so that we can close it if we exit.  */

static FILE *cpp_pipe;

/* The temporary file used if we're not using popen, so we can delete it
   if we exit.  */

static char *cpp_temp_file;

/* Input stream is either a file or a pipe.  */

static enum {ISTREAM_PIPE, ISTREAM_FILE} istream_type;

/* As we read the rc file, we attach information to this structure.  */

static struct res_directory *resources;

/* The number of cursor resources we have written out.  */

static int cursors;

/* The number of font resources we have written out.  */

static int fonts;

/* Font directory information.  */

struct fontdir *fontdirs;

/* Resource info to use for fontdirs.  */

struct res_res_info fontdirs_resinfo;

/* The number of icon resources we have written out.  */

static int icons;

/* Local functions.  */

static int run_cmd PARAMS ((char *, const char *));
static FILE *open_input_stream PARAMS ((char *));
static FILE *look_for_default PARAMS ((char *, const char *, int,
				       const char *, const char *));
static void close_input_stream PARAMS ((void));
static void unexpected_eof PARAMS ((const char *));
static int get_word PARAMS ((FILE *, const char *));
static unsigned long get_long PARAMS ((FILE *, const char *));
static void get_data
  PARAMS ((FILE *, unsigned char *, unsigned long, const char *));
static void define_fontdirs PARAMS ((void));

/* Run `cmd' and redirect the output to `redir'.  */

static int
run_cmd (cmd, redir)
     char *cmd;
     const char *redir;
{
  char *s;
  int pid, wait_status, retcode;
  int i;
  const char **argv;
  char *errmsg_fmt, *errmsg_arg;
  char *temp_base = choose_temp_base ();
  int in_quote;
  char sep;
  int redir_handle = -1;
  int stdout_save = -1;

  /* Count the args.  */
  i = 0;

  for (s = cmd; *s; s++)
    if (*s == ' ')
      i++;

  i++;
  argv = alloca (sizeof (char *) * (i + 3));
  i = 0;
  s = cmd;

  while (1)
    {
      while (*s == ' ' && *s != 0)
	s++;

      if (*s == 0)
	break;

      in_quote = (*s == '\'' || *s == '"');
      sep = (in_quote) ? *s++ : ' ';
      argv[i++] = s;

      while (*s != sep && *s != 0)
	s++;

      if (*s == 0)
	break;

      *s++ = 0;

      if (in_quote)
	s++;
    }
  argv[i++] = NULL;

  /* Setup the redirection.  We can't use the usual fork/exec and redirect
     since we may be running on non-POSIX Windows host.  */

  fflush (stdout);
  fflush (stderr);

  /* Open temporary output file.  */
  redir_handle = open (redir, O_WRONLY | O_TRUNC | O_CREAT, 0666);
  if (redir_handle == -1)
    fatal (_("can't open temporary file `%s': %s"), redir,
	   strerror (errno));

  /* Duplicate the stdout file handle so it can be restored later.  */
  stdout_save = dup (STDOUT_FILENO);
  if (stdout_save == -1)
    fatal (_("can't redirect stdout: `%s': %s"), redir, strerror (errno));

  /* Redirect stdout to our output file.  */
  dup2 (redir_handle, STDOUT_FILENO);

  pid = pexecute (argv[0], (char * const *) argv, program_name, temp_base,
		  &errmsg_fmt, &errmsg_arg, PEXECUTE_ONE | PEXECUTE_SEARCH);

  /* Restore stdout to its previous setting.  */
  dup2 (stdout_save, STDOUT_FILENO);

  /* Close reponse file.  */
  close (redir_handle);

  if (pid == -1)
    {
      fatal (_("%s %s: %s"), errmsg_fmt, errmsg_arg, strerror (errno));
      return 1;
    }

  retcode = 0;
  pid = pwait (pid, &wait_status, 0);

  if (pid == -1)
    {
      fatal (_("wait: %s"), strerror (errno));
      retcode = 1;
    }
  else if (WIFSIGNALED (wait_status))
    {
      fatal (_("subprocess got fatal signal %d"), WTERMSIG (wait_status));
      retcode = 1;
    }
  else if (WIFEXITED (wait_status))
    {
      if (WEXITSTATUS (wait_status) != 0)
	{
	  fatal (_("%s exited with status %d"), cmd,
	         WEXITSTATUS (wait_status));
	  retcode = 1;
	}
    }
  else
    retcode = 1;

  return retcode;
}

static FILE *
open_input_stream (cmd)
     char *cmd;
{
  if (istream_type == ISTREAM_FILE)
    {
      char *fileprefix;

      fileprefix = choose_temp_base ();
      cpp_temp_file = (char *) xmalloc (strlen (fileprefix) + 5);
      sprintf (cpp_temp_file, "%s.irc", fileprefix);
      free (fileprefix);

      if (run_cmd (cmd, cpp_temp_file))
	fatal (_("can't execute `%s': %s"), cmd, strerror (errno));

      cpp_pipe = fopen (cpp_temp_file, FOPEN_RT);;
      if (cpp_pipe == NULL)
	fatal (_("can't open temporary file `%s': %s"),
	       cpp_temp_file, strerror (errno));

      if (verbose)
	fprintf (stderr,
	         _("Using temporary file `%s' to read preprocessor output\n"),
		 cpp_temp_file);
    }
  else
    {
      cpp_pipe = popen (cmd, FOPEN_RT);
      if (cpp_pipe == NULL)
	fatal (_("can't popen `%s': %s"), cmd, strerror (errno));
      if (verbose)
	fprintf (stderr, _("Using popen to read preprocessor output\n"));
    }

  xatexit (close_input_stream);
  return cpp_pipe;
}

/* look for the preprocessor program */

static FILE *
look_for_default (cmd, prefix, end_prefix, preprocargs, filename)
     char *cmd;
     const char *prefix;
     int end_prefix;
     const char *preprocargs;
     const char *filename;
{
  char *space;
  int found;
  struct stat s;

  strcpy (cmd, prefix);

  sprintf (cmd + end_prefix, "%s", DEFAULT_PREPROCESSOR);
  space = strchr (cmd + end_prefix, ' ');
  if (space)
    *space = 0;

  if (
#if defined (__DJGPP__) || defined (__CYGWIN__) || defined (_WIN32)
      strchr (cmd, '\\') ||
#endif
      strchr (cmd, '/'))
    {
      found = (stat (cmd, &s) == 0
#ifdef HAVE_EXECUTABLE_SUFFIX
	       || stat (strcat (cmd, EXECUTABLE_SUFFIX), &s) == 0
#endif
	       );

      if (! found)
	{
	  if (verbose)
	    fprintf (stderr, _("Tried `%s'\n"), cmd);
	  return NULL;
	}
    }

  strcpy (cmd, prefix);

  sprintf (cmd + end_prefix, "%s %s %s",
	   DEFAULT_PREPROCESSOR, preprocargs, filename);

  if (verbose)
    fprintf (stderr, _("Using `%s'\n"), cmd);

  cpp_pipe = open_input_stream (cmd);
  return cpp_pipe;
}

/* Read an rc file.  */

struct res_directory *
read_rc_file (filename, preprocessor, preprocargs, language, use_temp_file)
     const char *filename;
     const char *preprocessor;
     const char *preprocargs;
     int language;
     int use_temp_file;
{
  char *cmd;

  istream_type = (use_temp_file) ? ISTREAM_FILE : ISTREAM_PIPE;

  if (preprocargs == NULL)
    preprocargs = "";
  if (filename == NULL)
    filename = "-";

  if (preprocessor)
    {
      cmd = xmalloc (strlen (preprocessor)
		     + strlen (preprocargs)
		     + strlen (filename)
		     + 10);
      sprintf (cmd, "%s %s %s", preprocessor, preprocargs, filename);

      cpp_pipe = open_input_stream (cmd);
    }
  else
    {
      char *dash, *slash, *cp;

      preprocessor = DEFAULT_PREPROCESSOR;

      cmd = xmalloc (strlen (program_name)
		     + strlen (preprocessor)
		     + strlen (preprocargs)
		     + strlen (filename)
#ifdef HAVE_EXECUTABLE_SUFFIX
		     + strlen (EXECUTABLE_SUFFIX)
#endif
		     + 10);


      dash = slash = 0;
      for (cp = program_name; *cp; cp++)
	{
	  if (*cp == '-')
	    dash = cp;
	  if (
#if defined (__DJGPP__) || defined (__CYGWIN__) || defined(_WIN32)
	      *cp == ':' || *cp == '\\' ||
#endif
	      *cp == '/')
	    {
	      slash = cp;
	      dash = 0;
	    }
	}

      cpp_pipe = 0;

      if (dash)
	{
	  /* First, try looking for a prefixed gcc in the windres
	     directory, with the same prefix as windres */

	  cpp_pipe = look_for_default (cmd, program_name, dash-program_name+1,
				       preprocargs, filename);
	}

      if (slash && !cpp_pipe)
	{
	  /* Next, try looking for a gcc in the same directory as
             that windres */

	  cpp_pipe = look_for_default (cmd, program_name, slash-program_name+1,
				       preprocargs, filename);
	}

      if (!cpp_pipe)
	{
	  /* Sigh, try the default */

	  cpp_pipe = look_for_default (cmd, "", 0, preprocargs, filename);
	}

    }

  free (cmd);

  rc_filename = xstrdup (filename);
  rc_lineno = 1;
  if (language != -1)
    rcparse_set_language (language);
  yyin = cpp_pipe;
  yyparse ();
  rcparse_discard_strings ();

  close_input_stream ();

  if (fontdirs != NULL)
    define_fontdirs ();

  free (rc_filename);
  rc_filename = NULL;

  return resources;
}

/* Close the input stream if it is open.  */

static void
close_input_stream ()
{
  if (istream_type == ISTREAM_FILE)
    {
      if (cpp_pipe != NULL)
	fclose (cpp_pipe);

      if (cpp_temp_file != NULL)
	{
	  int errno_save = errno;

	  unlink (cpp_temp_file);
	  errno = errno_save;
	  free (cpp_temp_file);
	}
    }
  else
    {
      if (cpp_pipe != NULL)
	pclose (cpp_pipe);
    }

  /* Since this is also run via xatexit, safeguard.  */
  cpp_pipe = NULL;
  cpp_temp_file = NULL;
}

/* Report an error while reading an rc file.  */

void
yyerror (msg)
     const char *msg;
{
  fatal ("%s:%d: %s", rc_filename, rc_lineno, msg);
}

/* Issue a warning while reading an rc file.  */

void
rcparse_warning (msg)
     const char *msg;
{
  fprintf (stderr, _("%s:%d: %s\n"), rc_filename, rc_lineno, msg);
}

/* Die if we get an unexpected end of file.  */

static void
unexpected_eof (msg)
     const char *msg;
{
  fatal (_("%s: unexpected EOF"), msg);
}

/* Read a 16 bit word from a file.  The data is assumed to be little
   endian.  */

static int
get_word (e, msg)
     FILE *e;
     const char *msg;
{
  int b1, b2;

  b1 = getc (e);
  b2 = getc (e);
  if (feof (e))
    unexpected_eof (msg);
  return ((b2 & 0xff) << 8) | (b1 & 0xff);
}

/* Read a 32 bit word from a file.  The data is assumed to be little
   endian.  */

static unsigned long
get_long (e, msg)
     FILE *e;
     const char *msg;
{
  int b1, b2, b3, b4;

  b1 = getc (e);
  b2 = getc (e);
  b3 = getc (e);
  b4 = getc (e);
  if (feof (e))
    unexpected_eof (msg);
  return (((((((b4 & 0xff) << 8)
	      | (b3 & 0xff)) << 8)
	    | (b2 & 0xff)) << 8)
	  | (b1 & 0xff));
}

/* Read data from a file.  This is a wrapper to do error checking.  */

static void
get_data (e, p, c, msg)
     FILE *e;
     unsigned char *p;
     unsigned long c;
     const char *msg;
{
  unsigned long got;

  got = fread (p, 1, c, e);
  if (got == c)
    return;

  fatal (_("%s: read of %lu returned %lu"), msg, c, got);
}

/* Define an accelerator resource.  */

void
define_accelerator (id, resinfo, data)
     struct res_id id;
     const struct res_res_info *resinfo;
     struct accelerator *data;
{
  struct res_resource *r;

  r = define_standard_resource (&resources, RT_ACCELERATOR, id,
				resinfo->language, 0);
  r->type = RES_TYPE_ACCELERATOR;
  r->u.acc = data;
  r->res_info = *resinfo;
}

/* Define a bitmap resource.  Bitmap data is stored in a file.  The
   first 14 bytes of the file are a standard header, which is not
   included in the resource data.  */

#define BITMAP_SKIP (14)

void
define_bitmap (id, resinfo, filename)
     struct res_id id;
     const struct res_res_info *resinfo;
     const char *filename;
{
  FILE *e;
  char *real_filename;
  struct stat s;
  unsigned char *data;
  int i;
  struct res_resource *r;

  e = open_file_search (filename, FOPEN_RB, "bitmap file", &real_filename);

  if (stat (real_filename, &s) < 0)
    fatal (_("stat failed on bitmap file `%s': %s"), real_filename,
	   strerror (errno));

  data = (unsigned char *) res_alloc (s.st_size - BITMAP_SKIP);

  for (i = 0; i < BITMAP_SKIP; i++)
    getc (e);

  get_data (e, data, s.st_size - BITMAP_SKIP, real_filename);

  fclose (e);
  free (real_filename);

  r = define_standard_resource (&resources, RT_BITMAP, id,
				resinfo->language, 0);

  r->type = RES_TYPE_BITMAP;
  r->u.data.length = s.st_size - BITMAP_SKIP;
  r->u.data.data = data;
  r->res_info = *resinfo;
}

/* Define a cursor resource.  A cursor file may contain a set of
   bitmaps, each representing the same cursor at various different
   resolutions.  They each get written out with a different ID.  The
   real cursor resource is then a group resource which can be used to
   select one of the actual cursors.  */

void
define_cursor (id, resinfo, filename)
     struct res_id id;
     const struct res_res_info *resinfo;
     const char *filename;
{
  FILE *e;
  char *real_filename;
  int type, count, i;
  struct icondir *icondirs;
  int first_cursor;
  struct res_resource *r;
  struct group_cursor *first, **pp;

  e = open_file_search (filename, FOPEN_RB, "cursor file", &real_filename);

  /* A cursor file is basically an icon file.  The start of the file
     is a three word structure.  The first word is ignored.  The
     second word is the type of data.  The third word is the number of
     entries.  */

  get_word (e, real_filename);
  type = get_word (e, real_filename);
  count = get_word (e, real_filename);
  if (type != 2)
    fatal (_("cursor file `%s' does not contain cursor data"), real_filename);

  /* Read in the icon directory entries.  */

  icondirs = (struct icondir *) xmalloc (count * sizeof *icondirs);

  for (i = 0; i < count; i++)
    {
      icondirs[i].width = getc (e);
      icondirs[i].height = getc (e);
      icondirs[i].colorcount = getc (e);
      getc (e);
      icondirs[i].u.cursor.xhotspot = get_word (e, real_filename);
      icondirs[i].u.cursor.yhotspot = get_word (e, real_filename);
      icondirs[i].bytes = get_long (e, real_filename);
      icondirs[i].offset = get_long (e, real_filename);

      if (feof (e))
	unexpected_eof (real_filename);
    }

  /* Define each cursor as a unique resource.  */

  first_cursor = cursors;

  for (i = 0; i < count; i++)
    {
      unsigned char *data;
      struct res_id name;
      struct cursor *c;

      if (fseek (e, icondirs[i].offset, SEEK_SET) != 0)
	fatal (_("%s: fseek to %lu failed: %s"), real_filename,
	       icondirs[i].offset, strerror (errno));

      data = (unsigned char *) res_alloc (icondirs[i].bytes);

      get_data (e, data, icondirs[i].bytes, real_filename);

      c = (struct cursor *) res_alloc (sizeof *c);
      c->xhotspot = icondirs[i].u.cursor.xhotspot;
      c->yhotspot = icondirs[i].u.cursor.yhotspot;
      c->length = icondirs[i].bytes;
      c->data = data;

      ++cursors;

      name.named = 0;
      name.u.id = cursors;

      r = define_standard_resource (&resources, RT_CURSOR, name,
				    resinfo->language, 0);
      r->type = RES_TYPE_CURSOR;
      r->u.cursor = c;
      r->res_info = *resinfo;
    }

  fclose (e);
  free (real_filename);

  /* Define a cursor group resource.  */

  first = NULL;
  pp = &first;
  for (i = 0; i < count; i++)
    {
      struct group_cursor *cg;

      cg = (struct group_cursor *) res_alloc (sizeof *cg);
      cg->next = NULL;
      cg->width = icondirs[i].width;
      cg->height = 2 * icondirs[i].height;

      /* FIXME: What should these be set to?  */
      cg->planes = 1;
      cg->bits = 1;

      cg->bytes = icondirs[i].bytes + 4;
      cg->index = first_cursor + i + 1;

      *pp = cg;
      pp = &(*pp)->next;
    }

  free (icondirs);

  r = define_standard_resource (&resources, RT_GROUP_CURSOR, id,
				resinfo->language, 0);
  r->type = RES_TYPE_GROUP_CURSOR;
  r->u.group_cursor = first;
  r->res_info = *resinfo;
}

/* Define a dialog resource.  */

void
define_dialog (id, resinfo, dialog)
     struct res_id id;
     const struct res_res_info *resinfo;
     const struct dialog *dialog;
{
  struct dialog *copy;
  struct res_resource *r;

  copy = (struct dialog *) res_alloc (sizeof *copy);
  *copy = *dialog;

  r = define_standard_resource (&resources, RT_DIALOG, id,
				resinfo->language, 0);
  r->type = RES_TYPE_DIALOG;
  r->u.dialog = copy;
  r->res_info = *resinfo;
}

/* Define a dialog control.  This does not define a resource, but
   merely allocates and fills in a structure.  */

struct dialog_control *
define_control (text, id, x, y, width, height, class, style, exstyle)
     const char *text;
     unsigned long id;
     unsigned long x;
     unsigned long y;
     unsigned long width;
     unsigned long height;
     unsigned long class;
     unsigned long style;
     unsigned long exstyle;
{
  struct dialog_control *n;

  n = (struct dialog_control *) res_alloc (sizeof *n);
  n->next = NULL;
  n->id = id;
  n->style = style;
  n->exstyle = exstyle;
  n->x = x;
  n->y = y;
  n->width = width;
  n->height = height;
  n->class.named = 0;
  n->class.u.id = class;
  if (text == NULL)
    text = "";
  res_string_to_id (&n->text, text);
  n->data = NULL;
  n->help = 0;

  return n;
}

struct dialog_control *
define_icon_control (iid, id, x, y, style, exstyle, help, data, ex)
     struct res_id iid;
     unsigned long id;
     unsigned long x;
     unsigned long y;
     unsigned long style;
     unsigned long exstyle;
     unsigned long help;
     struct rcdata_item *data;
     struct dialog_ex *ex;
{
  struct dialog_control *n;
  if (style == 0)
    style = SS_ICON | WS_CHILD | WS_VISIBLE;
  n = define_control (0, id, x, y, 0, 0, CTL_STATIC, style, exstyle);
  n->text = iid;
  if (help && !ex)
    rcparse_warning (_("help ID requires DIALOGEX"));
  if (data && !ex)
    rcparse_warning (_("control data requires DIALOGEX"));
  n->help = help;
  n->data = data;

  return n;
}

/* Define a font resource.  */

void
define_font (id, resinfo, filename)
     struct res_id id;
     const struct res_res_info *resinfo;
     const char *filename;
{
  FILE *e;
  char *real_filename;
  struct stat s;
  unsigned char *data;
  struct res_resource *r;
  long offset;
  long fontdatalength;
  unsigned char *fontdata;
  struct fontdir *fd;
  const char *device, *face;
  struct fontdir **pp;

  e = open_file_search (filename, FOPEN_RB, "font file", &real_filename);

  if (stat (real_filename, &s) < 0)
    fatal (_("stat failed on bitmap file `%s': %s"), real_filename,
	   strerror (errno));

  data = (unsigned char *) res_alloc (s.st_size);

  get_data (e, data, s.st_size, real_filename);

  fclose (e);
  free (real_filename);

  r = define_standard_resource (&resources, RT_FONT, id,
				resinfo->language, 0);

  r->type = RES_TYPE_FONT;
  r->u.data.length = s.st_size;
  r->u.data.data = data;
  r->res_info = *resinfo;

  /* For each font resource, we must add an entry in the FONTDIR
     resource.  The FONTDIR resource includes some strings in the font
     file.  To find them, we have to do some magic on the data we have
     read.  */

  offset = ((((((data[47] << 8)
		| data[46]) << 8)
	      | data[45]) << 8)
	    | data[44]);
  if (offset > 0 && offset < s.st_size)
    device = (char *) data + offset;
  else
    device = "";

  offset = ((((((data[51] << 8)
		| data[50]) << 8)
	      | data[49]) << 8)
	    | data[48]);
  if (offset > 0 && offset < s.st_size)
    face = (char *) data + offset;
  else
    face = "";

  ++fonts;

  fontdatalength = 58 + strlen (device) + strlen (face);
  fontdata = (unsigned char *) res_alloc (fontdatalength);
  memcpy (fontdata, data, 56);
  strcpy ((char *) fontdata + 56, device);
  strcpy ((char *) fontdata + 57 + strlen (device), face);

  fd = (struct fontdir *) res_alloc (sizeof *fd);
  fd->next = NULL;
  fd->index = fonts;
  fd->length = fontdatalength;
  fd->data = fontdata;

  for (pp = &fontdirs; *pp != NULL; pp = &(*pp)->next)
    ;
  *pp = fd;

  /* For the single fontdirs resource, we always use the resource
     information of the last font.  I don't know what else to do.  */
  fontdirs_resinfo = *resinfo;
}

/* Define the fontdirs resource.  This is called after the entire rc
   file has been parsed, if any font resources were seen.  */

static void
define_fontdirs ()
{
  struct res_resource *r;
  struct res_id id;

  id.named = 0;
  id.u.id = 1;

  r = define_standard_resource (&resources, RT_FONTDIR, id, 0x409, 0);

  r->type = RES_TYPE_FONTDIR;
  r->u.fontdir = fontdirs;
  r->res_info = fontdirs_resinfo;
}

/* Define an icon resource.  An icon file may contain a set of
   bitmaps, each representing the same icon at various different
   resolutions.  They each get written out with a different ID.  The
   real icon resource is then a group resource which can be used to
   select one of the actual icon bitmaps.  */

void
define_icon (id, resinfo, filename)
     struct res_id id;
     const struct res_res_info *resinfo;
     const char *filename;
{
  FILE *e;
  char *real_filename;
  int type, count, i;
  struct icondir *icondirs;
  int first_icon;
  struct res_resource *r;
  struct group_icon *first, **pp;

  e = open_file_search (filename, FOPEN_RB, "icon file", &real_filename);

  /* The start of an icon file is a three word structure.  The first
     word is ignored.  The second word is the type of data.  The third
     word is the number of entries.  */

  get_word (e, real_filename);
  type = get_word (e, real_filename);
  count = get_word (e, real_filename);
  if (type != 1)
    fatal (_("icon file `%s' does not contain icon data"), real_filename);

  /* Read in the icon directory entries.  */

  icondirs = (struct icondir *) xmalloc (count * sizeof *icondirs);

  for (i = 0; i < count; i++)
    {
      icondirs[i].width = getc (e);
      icondirs[i].height = getc (e);
      icondirs[i].colorcount = getc (e);
      getc (e);
      icondirs[i].u.icon.planes = get_word (e, real_filename);
      icondirs[i].u.icon.bits = get_word (e, real_filename);
      icondirs[i].bytes = get_long (e, real_filename);
      icondirs[i].offset = get_long (e, real_filename);

      if (feof (e))
	unexpected_eof (real_filename);
    }

  /* Define each icon as a unique resource.  */

  first_icon = icons;

  for (i = 0; i < count; i++)
    {
      unsigned char *data;
      struct res_id name;

      if (fseek (e, icondirs[i].offset, SEEK_SET) != 0)
	fatal (_("%s: fseek to %lu failed: %s"), real_filename,
	       icondirs[i].offset, strerror (errno));

      data = (unsigned char *) res_alloc (icondirs[i].bytes);

      get_data (e, data, icondirs[i].bytes, real_filename);

      ++icons;

      name.named = 0;
      name.u.id = icons;

      r = define_standard_resource (&resources, RT_ICON, name,
				    resinfo->language, 0);
      r->type = RES_TYPE_ICON;
      r->u.data.length = icondirs[i].bytes;
      r->u.data.data = data;
      r->res_info = *resinfo;
    }

  fclose (e);
  free (real_filename);

  /* Define an icon group resource.  */

  first = NULL;
  pp = &first;
  for (i = 0; i < count; i++)
    {
      struct group_icon *cg;

      /* For some reason, at least in some files the planes and bits
         are zero.  We instead set them from the color.  This is
         copied from rcl.  */

      cg = (struct group_icon *) res_alloc (sizeof *cg);
      cg->next = NULL;
      cg->width = icondirs[i].width;
      cg->height = icondirs[i].height;
      cg->colors = icondirs[i].colorcount;

      cg->planes = 1;
      cg->bits = 0;
      while ((1 << cg->bits) < cg->colors)
	++cg->bits;

      cg->bytes = icondirs[i].bytes;
      cg->index = first_icon + i + 1;

      *pp = cg;
      pp = &(*pp)->next;
    }

  free (icondirs);

  r = define_standard_resource (&resources, RT_GROUP_ICON, id,
				resinfo->language, 0);
  r->type = RES_TYPE_GROUP_ICON;
  r->u.group_icon = first;
  r->res_info = *resinfo;
}

/* Define a menu resource.  */

void
define_menu (id, resinfo, menuitems)
     struct res_id id;
     const struct res_res_info *resinfo;
     struct menuitem *menuitems;
{
  struct menu *m;
  struct res_resource *r;

  m = (struct menu *) res_alloc (sizeof *m);
  m->items = menuitems;
  m->help = 0;

  r = define_standard_resource (&resources, RT_MENU, id, resinfo->language, 0);
  r->type = RES_TYPE_MENU;
  r->u.menu = m;
  r->res_info = *resinfo;
}

/* Define a menu item.  This does not define a resource, but merely
   allocates and fills in a structure.  */

struct menuitem *
define_menuitem (text, menuid, type, state, help, menuitems)
     const char *text;
     int menuid;
     unsigned long type;
     unsigned long state;
     unsigned long help;
     struct menuitem *menuitems;
{
  struct menuitem *mi;

  mi = (struct menuitem *) res_alloc (sizeof *mi);
  mi->next = NULL;
  mi->type = type;
  mi->state = state;
  mi->id = menuid;
  if (text == NULL)
    mi->text = NULL;
  else
    unicode_from_ascii ((int *) NULL, &mi->text, text);
  mi->help = help;
  mi->popup = menuitems;
  return mi;
}

/* Define a messagetable resource.  */

void
define_messagetable (id, resinfo, filename)
     struct res_id id;
     const struct res_res_info *resinfo;
     const char *filename;
{
  FILE *e;
  char *real_filename;
  struct stat s;
  unsigned char *data;
  struct res_resource *r;

  e = open_file_search (filename, FOPEN_RB, "messagetable file",
			&real_filename);

  if (stat (real_filename, &s) < 0)
    fatal (_("stat failed on bitmap file `%s': %s"), real_filename,
	   strerror (errno));

  data = (unsigned char *) res_alloc (s.st_size);

  get_data (e, data, s.st_size, real_filename);

  fclose (e);
  free (real_filename);

  r = define_standard_resource (&resources, RT_MESSAGETABLE, id,
				resinfo->language, 0);

  r->type = RES_TYPE_MESSAGETABLE;
  r->u.data.length = s.st_size;
  r->u.data.data = data;
  r->res_info = *resinfo;
}

/* Define an rcdata resource.  */

void
define_rcdata (id, resinfo, data)
     struct res_id id;
     const struct res_res_info *resinfo;
     struct rcdata_item *data;
{
  struct res_resource *r;

  r = define_standard_resource (&resources, RT_RCDATA, id,
				resinfo->language, 0);
  r->type = RES_TYPE_RCDATA;
  r->u.rcdata = data;
  r->res_info = *resinfo;
}

/* Create an rcdata item holding a string.  */

struct rcdata_item *
define_rcdata_string (string, len)
     const char *string;
     unsigned long len;
{
  struct rcdata_item *ri;
  char *s;

  ri = (struct rcdata_item *) res_alloc (sizeof *ri);
  ri->next = NULL;
  ri->type = RCDATA_STRING;
  ri->u.string.length = len;
  s = (char *) res_alloc (len);
  memcpy (s, string, len);
  ri->u.string.s = s;

  return ri;
}

/* Create an rcdata item holding a number.  */

struct rcdata_item *
define_rcdata_number (val, dword)
     unsigned long val;
     int dword;
{
  struct rcdata_item *ri;

  ri = (struct rcdata_item *) res_alloc (sizeof *ri);
  ri->next = NULL;
  ri->type = dword ? RCDATA_DWORD : RCDATA_WORD;
  ri->u.word = val;

  return ri;
}

/* Define a stringtable resource.  This is called for each string
   which appears in a STRINGTABLE statement.  */

void
define_stringtable (resinfo, stringid, string)
     const struct res_res_info *resinfo;
     unsigned long stringid;
     const char *string;
{
  struct res_id id;
  struct res_resource *r;

  id.named = 0;
  id.u.id = (stringid >> 4) + 1;
  r = define_standard_resource (&resources, RT_STRING, id,
				resinfo->language, 1);

  if (r->type == RES_TYPE_UNINITIALIZED)
    {
      int i;

      r->type = RES_TYPE_STRINGTABLE;
      r->u.stringtable = ((struct stringtable *)
			  res_alloc (sizeof (struct stringtable)));
      for (i = 0; i < 16; i++)
	{
	  r->u.stringtable->strings[i].length = 0;
	  r->u.stringtable->strings[i].string = NULL;
	}

      r->res_info = *resinfo;
    }

  unicode_from_ascii (&r->u.stringtable->strings[stringid & 0xf].length,
		      &r->u.stringtable->strings[stringid & 0xf].string,
		      string);
}

/* Define a user data resource where the data is in the rc file.  */

void
define_user_data (id, type, resinfo, data)
     struct res_id id;
     struct res_id type;
     const struct res_res_info *resinfo;
     struct rcdata_item *data;
{
  struct res_id ids[3];
  struct res_resource *r;

  ids[0] = type;
  ids[1] = id;
  ids[2].named = 0;
  ids[2].u.id = resinfo->language;

  r = define_resource (&resources, 3, ids, 0);
  r->type = RES_TYPE_USERDATA;
  r->u.userdata = data;
  r->res_info = *resinfo;
}

/* Define a user data resource where the data is in a file.  */

void
define_user_file (id, type, resinfo, filename)
     struct res_id id;
     struct res_id type;
     const struct res_res_info *resinfo;
     const char *filename;
{
  FILE *e;
  char *real_filename;
  struct stat s;
  unsigned char *data;
  struct res_id ids[3];
  struct res_resource *r;

  e = open_file_search (filename, FOPEN_RB, "font file", &real_filename);

  if (stat (real_filename, &s) < 0)
    fatal (_("stat failed on bitmap file `%s': %s"), real_filename,
	   strerror (errno));

  data = (unsigned char *) res_alloc (s.st_size);

  get_data (e, data, s.st_size, real_filename);

  fclose (e);
  free (real_filename);

  ids[0] = type;
  ids[1] = id;
  ids[2].named = 0;
  ids[2].u.id = resinfo->language;

  r = define_resource (&resources, 3, ids, 0);
  r->type = RES_TYPE_USERDATA;
  r->u.userdata = ((struct rcdata_item *)
		   res_alloc (sizeof (struct rcdata_item)));
  r->u.userdata->next = NULL;
  r->u.userdata->type = RCDATA_BUFFER;
  r->u.userdata->u.buffer.length = s.st_size;
  r->u.userdata->u.buffer.data = data;
  r->res_info = *resinfo;
}

/* Define a versioninfo resource.  */

void
define_versioninfo (id, language, fixedverinfo, verinfo)
     struct res_id id;
     int language;
     struct fixed_versioninfo *fixedverinfo;
     struct ver_info *verinfo;
{
  struct res_resource *r;

  r = define_standard_resource (&resources, RT_VERSION, id, language, 0);
  r->type = RES_TYPE_VERSIONINFO;
  r->u.versioninfo = ((struct versioninfo *)
		      res_alloc (sizeof (struct versioninfo)));
  r->u.versioninfo->fixed = fixedverinfo;
  r->u.versioninfo->var = verinfo;
  r->res_info.language = language;
}

/* Add string version info to a list of version information.  */

struct ver_info *
append_ver_stringfileinfo (verinfo, language, strings)
     struct ver_info *verinfo;
     const char *language;
     struct ver_stringinfo *strings;
{
  struct ver_info *vi, **pp;

  vi = (struct ver_info *) res_alloc (sizeof *vi);
  vi->next = NULL;
  vi->type = VERINFO_STRING;
  unicode_from_ascii ((int *) NULL, &vi->u.string.language, language);
  vi->u.string.strings = strings;

  for (pp = &verinfo; *pp != NULL; pp = &(*pp)->next)
    ;
  *pp = vi;

  return verinfo;
}

/* Add variable version info to a list of version information.  */

struct ver_info *
append_ver_varfileinfo (verinfo, key, var)
     struct ver_info *verinfo;
     const char *key;
     struct ver_varinfo *var;
{
  struct ver_info *vi, **pp;

  vi = (struct ver_info *) res_alloc (sizeof *vi);
  vi->next = NULL;
  vi->type = VERINFO_VAR;
  unicode_from_ascii ((int *) NULL, &vi->u.var.key, key);
  vi->u.var.var = var;

  for (pp = &verinfo; *pp != NULL; pp = &(*pp)->next)
    ;
  *pp = vi;

  return verinfo;
}

/* Append version string information to a list.  */

struct ver_stringinfo *
append_verval (strings, key, value)
     struct ver_stringinfo *strings;
     const char *key;
     const char *value;
{
  struct ver_stringinfo *vs, **pp;

  vs = (struct ver_stringinfo *) res_alloc (sizeof *vs);
  vs->next = NULL;
  unicode_from_ascii ((int *) NULL, &vs->key, key);
  unicode_from_ascii ((int *) NULL, &vs->value, value);

  for (pp = &strings; *pp != NULL; pp = &(*pp)->next)
    ;
  *pp = vs;

  return strings;
}

/* Append version variable information to a list.  */

struct ver_varinfo *
append_vertrans (var, language, charset)
     struct ver_varinfo *var;
     unsigned long language;
     unsigned long charset;
{
  struct ver_varinfo *vv, **pp;

  vv = (struct ver_varinfo *) res_alloc (sizeof *vv);
  vv->next = NULL;
  vv->language = language;
  vv->charset = charset;

  for (pp = &var; *pp != NULL; pp = &(*pp)->next)
    ;
  *pp = vv;

  return var;
}

/* Local functions used to write out an rc file.  */

static void indent PARAMS ((FILE *, int));
static void write_rc_directory
  PARAMS ((FILE *, const struct res_directory *, const struct res_id *,
	   const struct res_id *, int *, int));
static void write_rc_subdir
  PARAMS ((FILE *, const struct res_entry *, const struct res_id *,
	   const struct res_id *, int *, int));
static void write_rc_resource
  PARAMS ((FILE *, const struct res_id *, const struct res_id *,
	   const struct res_resource *, int *));
static void write_rc_accelerators
  PARAMS ((FILE *, const struct accelerator *));
static void write_rc_cursor PARAMS ((FILE *, const struct cursor *));
static void write_rc_group_cursor
  PARAMS ((FILE *, const struct group_cursor *));
static void write_rc_dialog PARAMS ((FILE *, const struct dialog *));
static void write_rc_dialog_control
  PARAMS ((FILE *, const struct dialog_control *));
static void write_rc_fontdir PARAMS ((FILE *, const struct fontdir *));
static void write_rc_group_icon PARAMS ((FILE *, const struct group_icon *));
static void write_rc_menu PARAMS ((FILE *, const struct menu *, int));
static void write_rc_menuitems
  PARAMS ((FILE *, const struct menuitem *, int, int));
static void write_rc_rcdata PARAMS ((FILE *, const struct rcdata_item *, int));
static void write_rc_stringtable
  PARAMS ((FILE *, const struct res_id *, const struct stringtable *));
static void write_rc_versioninfo PARAMS ((FILE *, const struct versioninfo *));
static void write_rc_filedata
  PARAMS ((FILE *, unsigned long, const unsigned char *));

/* Indent a given number of spaces.  */

static void
indent (e, c)
     FILE *e;
     int c;
{
  int i;

  for (i = 0; i < c; i++)
    putc (' ', e);
}

/* Dump the resources we have read in the format of an rc file.

   Actually, we don't use the format of an rc file, because it's way
   too much of a pain--for example, we'd have to write icon resources
   into a file and refer to that file.  We just generate a readable
   format that kind of looks like an rc file, and is useful for
   understanding the contents of a resource file.  Someday we may want
   to generate an rc file which the rc compiler can read; if that day
   comes, this code will have to be fixed up.  */

void
write_rc_file (filename, resources)
     const char *filename;
     const struct res_directory *resources;
{
  FILE *e;
  int language;

  if (filename == NULL)
    e = stdout;
  else
    {
      e = fopen (filename, FOPEN_WT);
      if (e == NULL)
	fatal (_("can't open `%s' for output: %s"), filename, strerror (errno));
    }

  language = -1;
  write_rc_directory (e, resources, (const struct res_id *) NULL,
		      (const struct res_id *) NULL, &language, 1);
}

/* Write out a directory.  E is the file to write to.  RD is the
   directory.  TYPE is a pointer to the level 1 ID which serves as the
   resource type.  NAME is a pointer to the level 2 ID which serves as
   an individual resource name.  LANGUAGE is a pointer to the current
   language.  LEVEL is the level in the tree.  */

static void
write_rc_directory (e, rd, type, name, language, level)
     FILE *e;
     const struct res_directory *rd;
     const struct res_id *type;
     const struct res_id *name;
     int *language;
     int level;
{
  const struct res_entry *re;

  /* Print out some COFF information that rc files can't represent.  */

  if (rd->time != 0)
    fprintf (e, "// Time stamp: %lu\n", rd->time);
  if (rd->characteristics != 0)
    fprintf (e, "// Characteristics: %lu\n", rd->characteristics);
  if (rd->major != 0 || rd->minor != 0)
    fprintf (e, "// Version: %d %d\n", rd->major, rd->minor);

  for (re = rd->entries;  re != NULL; re = re->next)
    {
      switch (level)
	{
	case 1:
	  /* If we're at level 1, the key of this resource is the
             type.  This normally duplicates the information we have
             stored with the resource itself, but we need to remember
             the type if this is a user define resource type.  */
	  type = &re->id;
	  break;

	case 2:
	  /* If we're at level 2, the key of this resource is the name
	     we are going to use in the rc printout.  */
	  name = &re->id;
	  break;

	case 3:
	  /* If we're at level 3, then this key represents a language.
	     Use it to update the current language.  */
	  if (! re->id.named
	      && re->id.u.id != (unsigned long) (unsigned int) *language
	      && (re->id.u.id & 0xffff) == re->id.u.id)
	    {
	      fprintf (e, "LANGUAGE %lu, %lu\n",
		       re->id.u.id & ((1 << SUBLANG_SHIFT) - 1),
		       (re->id.u.id >> SUBLANG_SHIFT) & 0xff);
	      *language = re->id.u.id;
	    }
	  break;

	default:
	  break;
	}

      if (re->subdir)
	write_rc_subdir (e, re, type, name, language, level);
      else
	{
	  if (level == 3)
	    {
	      /* This is the normal case: the three levels are
                 TYPE/NAME/LANGUAGE.  NAME will have been set at level
                 2, and represents the name to use.  We probably just
                 set LANGUAGE, and it will probably match what the
                 resource itself records if anything.  */
	      write_rc_resource (e, type, name, re->u.res, language);
	    }
	  else
	    {
	      fprintf (e, "// Resource at unexpected level %d\n", level);
	      write_rc_resource (e, type, (struct res_id *) NULL, re->u.res,
				 language);
	    }
	}
    }
}

/* Write out a subdirectory entry.  E is the file to write to.  RE is
   the subdirectory entry.  TYPE and NAME are pointers to higher level
   IDs, or NULL.  LANGUAGE is a pointer to the current language.
   LEVEL is the level in the tree.  */

static void
write_rc_subdir (e, re, type, name, language, level)
     FILE *e;
     const struct res_entry *re;
     const struct res_id *type;
     const struct res_id *name;
     int *language;
     int level;
{
  fprintf (e, "\n");
  switch (level)
    {
    case 1:
      fprintf (e, "// Type: ");
      if (re->id.named)
	res_id_print (e, re->id, 1);
      else
	{
	  const char *s;

	  switch (re->id.u.id)
	    {
	    case RT_CURSOR: s = "cursor"; break;
	    case RT_BITMAP: s = "bitmap"; break;
	    case RT_ICON: s = "icon"; break;
	    case RT_MENU: s = "menu"; break;
	    case RT_DIALOG: s = "dialog"; break;
	    case RT_STRING: s = "stringtable"; break;
	    case RT_FONTDIR: s = "fontdir"; break;
	    case RT_FONT: s = "font"; break;
	    case RT_ACCELERATOR: s = "accelerators"; break;
	    case RT_RCDATA: s = "rcdata"; break;
	    case RT_MESSAGETABLE: s = "messagetable"; break;
	    case RT_GROUP_CURSOR: s = "group cursor"; break;
	    case RT_GROUP_ICON: s = "group icon"; break;
	    case RT_VERSION: s = "version"; break;
	    case RT_DLGINCLUDE: s = "dlginclude"; break;
	    case RT_PLUGPLAY: s = "plugplay"; break;
	    case RT_VXD: s = "vxd"; break;
	    case RT_ANICURSOR: s = "anicursor"; break;
	    case RT_ANIICON: s = "aniicon"; break;
	    default: s = NULL; break;
	    }

	  if (s != NULL)
	    fprintf (e, "%s", s);
	  else
	    res_id_print (e, re->id, 1);
	}
      fprintf (e, "\n");
      break;

    case 2:
      fprintf (e, "// Name: ");
      res_id_print (e, re->id, 1);
      fprintf (e, "\n");
      break;

    case 3:
      fprintf (e, "// Language: ");
      res_id_print (e, re->id, 1);
      fprintf (e, "\n");
      break;

    default:
      fprintf (e, "// Level %d: ", level);
      res_id_print (e, re->id, 1);
      fprintf (e, "\n");
    }

  write_rc_directory (e, re->u.dir, type, name, language, level + 1);
}

/* Write out a single resource.  E is the file to write to.  TYPE is a
   pointer to the type of the resource.  NAME is a pointer to the name
   of the resource; it will be NULL if there is a level mismatch.  RES
   is the resource data.  LANGUAGE is a pointer to the current
   language.  */

static void
write_rc_resource (e, type, name, res, language)
     FILE *e;
     const struct res_id *type;
     const struct res_id *name;
     const struct res_resource *res;
     int *language;
{
  const char *s;
  int rt;
  int menuex = 0;

  fprintf (e, "\n");

  switch (res->type)
    {
    default:
      abort ();

    case RES_TYPE_ACCELERATOR:
      s = "ACCELERATOR";
      rt = RT_ACCELERATOR;
      break;

    case RES_TYPE_BITMAP:
      s = "BITMAP";
      rt = RT_BITMAP;
      break;

    case RES_TYPE_CURSOR:
      s = "CURSOR";
      rt = RT_CURSOR;
      break;

    case RES_TYPE_GROUP_CURSOR:
      s = "GROUP_CURSOR";
      rt = RT_GROUP_CURSOR;
      break;

    case RES_TYPE_DIALOG:
      if (extended_dialog (res->u.dialog))
	s = "DIALOGEX";
      else
	s = "DIALOG";
      rt = RT_DIALOG;
      break;

    case RES_TYPE_FONT:
      s = "FONT";
      rt = RT_FONT;
      break;

    case RES_TYPE_FONTDIR:
      s = "FONTDIR";
      rt = RT_FONTDIR;
      break;

    case RES_TYPE_ICON:
      s = "ICON";
      rt = RT_ICON;
      break;

    case RES_TYPE_GROUP_ICON:
      s = "GROUP_ICON";
      rt = RT_GROUP_ICON;
      break;

    case RES_TYPE_MENU:
      if (extended_menu (res->u.menu))
	{
	  s = "MENUEX";
	  menuex = 1;
	}
      else
	{
	  s = "MENU";
	  menuex = 0;
	}
      rt = RT_MENU;
      break;

    case RES_TYPE_MESSAGETABLE:
      s = "MESSAGETABLE";
      rt = RT_MESSAGETABLE;
      break;

    case RES_TYPE_RCDATA:
      s = "RCDATA";
      rt = RT_RCDATA;
      break;

    case RES_TYPE_STRINGTABLE:
      s = "STRINGTABLE";
      rt = RT_STRING;
      break;

    case RES_TYPE_USERDATA:
      s = NULL;
      rt = 0;
      break;

    case RES_TYPE_VERSIONINFO:
      s = "VERSIONINFO";
      rt = RT_VERSION;
      break;
    }

  if (rt != 0
      && type != NULL
      && (type->named || type->u.id != (unsigned long) rt))
    {
      fprintf (e, "// Unexpected resource type mismatch: ");
      res_id_print (e, *type, 1);
      fprintf (e, " != %d", rt);
    }

  if (res->coff_info.codepage != 0)
    fprintf (e, "// Code page: %lu\n", res->coff_info.codepage);
  if (res->coff_info.reserved != 0)
    fprintf (e, "// COFF reserved value: %lu\n", res->coff_info.reserved);

  if (name != NULL)
    res_id_print (e, *name, 0);
  else
    fprintf (e, "??Unknown-Name??");

  fprintf (e, " ");
  if (s != NULL)
    fprintf (e, "%s", s);
  else if (type != NULL)
    res_id_print (e, *type, 0);
  else
    fprintf (e, "??Unknown-Type??");

  if (res->res_info.memflags != 0)
    {
      if ((res->res_info.memflags & MEMFLAG_MOVEABLE) != 0)
	fprintf (e, " MOVEABLE");
      if ((res->res_info.memflags & MEMFLAG_PURE) != 0)
	fprintf (e, " PURE");
      if ((res->res_info.memflags & MEMFLAG_PRELOAD) != 0)
	fprintf (e, " PRELOAD");
      if ((res->res_info.memflags & MEMFLAG_DISCARDABLE) != 0)
	fprintf (e, " DISCARDABLE");
    }

  if (res->type == RES_TYPE_DIALOG)
    {
      fprintf (e, " %d, %d, %d, %d", res->u.dialog->x, res->u.dialog->y,
	       res->u.dialog->width, res->u.dialog->height);
      if (res->u.dialog->ex != NULL
	  && res->u.dialog->ex->help != 0)
	fprintf (e, ", %lu", res->u.dialog->ex->help);
    }

  fprintf (e, "\n");

  if ((res->res_info.language != 0 && res->res_info.language != *language)
      || res->res_info.characteristics != 0
      || res->res_info.version != 0)
    {
      int modifiers;

      switch (res->type)
	{
	case RES_TYPE_ACCELERATOR:
	case RES_TYPE_DIALOG:
	case RES_TYPE_MENU:
	case RES_TYPE_RCDATA:
	case RES_TYPE_STRINGTABLE:
	  modifiers = 1;
	  break;

	default:
	  modifiers = 0;
	  break;
	}

      if (res->res_info.language != 0 && res->res_info.language != *language)
	fprintf (e, "%sLANGUAGE %d, %d\n",
		 modifiers ? "// " : "",
		 res->res_info.language & ((1<<SUBLANG_SHIFT)-1),
		 (res->res_info.language >> SUBLANG_SHIFT) & 0xff);
      if (res->res_info.characteristics != 0)
	fprintf (e, "%sCHARACTERISTICS %lu\n",
		 modifiers ? "// " : "",
		 res->res_info.characteristics);
      if (res->res_info.version != 0)
	fprintf (e, "%sVERSION %lu\n",
		 modifiers ? "// " : "",
		 res->res_info.version);
    }

  switch (res->type)
    {
    default:
      abort ();

    case RES_TYPE_ACCELERATOR:
      write_rc_accelerators (e, res->u.acc);
      break;

    case RES_TYPE_CURSOR:
      write_rc_cursor (e, res->u.cursor);
      break;

    case RES_TYPE_GROUP_CURSOR:
      write_rc_group_cursor (e, res->u.group_cursor);
      break;

    case RES_TYPE_DIALOG:
      write_rc_dialog (e, res->u.dialog);
      break;

    case RES_TYPE_FONTDIR:
      write_rc_fontdir (e, res->u.fontdir);
      break;

    case RES_TYPE_GROUP_ICON:
      write_rc_group_icon (e, res->u.group_icon);
      break;

    case RES_TYPE_MENU:
      write_rc_menu (e, res->u.menu, menuex);
      break;

    case RES_TYPE_RCDATA:
      write_rc_rcdata (e, res->u.rcdata, 0);
      break;

    case RES_TYPE_STRINGTABLE:
      write_rc_stringtable (e, name, res->u.stringtable);
      break;

    case RES_TYPE_USERDATA:
      write_rc_rcdata (e, res->u.userdata, 0);
      break;

    case RES_TYPE_VERSIONINFO:
      write_rc_versioninfo (e, res->u.versioninfo);
      break;

    case RES_TYPE_BITMAP:
    case RES_TYPE_FONT:
    case RES_TYPE_ICON:
    case RES_TYPE_MESSAGETABLE:
      write_rc_filedata (e, res->u.data.length, res->u.data.data);
      break;
    }
}

/* Write out accelerator information.  */

static void
write_rc_accelerators (e, accelerators)
     FILE *e;
     const struct accelerator *accelerators;
{
  const struct accelerator *acc;

  fprintf (e, "BEGIN\n");
  for (acc = accelerators; acc != NULL; acc = acc->next)
    {
      int printable;

      fprintf (e, "  ");

      if ((acc->key & 0x7f) == acc->key
	  && ISPRINT (acc->key)
	  && (acc->flags & ACC_VIRTKEY) == 0)
	{
	  fprintf (e, "\"%c\"", acc->key);
	  printable = 1;
	}
      else
	{
	  fprintf (e, "%d", acc->key);
	  printable = 0;
	}

      fprintf (e, ", %d", acc->id);

      if (! printable)
	{
	  if ((acc->flags & ACC_VIRTKEY) != 0)
	    fprintf (e, ", VIRTKEY");
	  else
	    fprintf (e, ", ASCII");
	}

      if ((acc->flags & ACC_SHIFT) != 0)
	fprintf (e, ", SHIFT");
      if ((acc->flags & ACC_CONTROL) != 0)
	fprintf (e, ", CONTROL");
      if ((acc->flags & ACC_ALT) != 0)
	fprintf (e, ", ALT");

      fprintf (e, "\n");
    }

  fprintf (e, "END\n");
}

/* Write out cursor information.  This would normally be in a separate
   file, which the rc file would include.  */

static void
write_rc_cursor (e, cursor)
     FILE *e;
     const struct cursor *cursor;
{
  fprintf (e, "// Hotspot: x: %d; y: %d\n", cursor->xhotspot,
	   cursor->yhotspot);
  write_rc_filedata (e, cursor->length, cursor->data);
}

/* Write out group cursor data.  This would normally be built from the
   cursor data.  */

static void
write_rc_group_cursor (e, group_cursor)
     FILE *e;
     const struct group_cursor *group_cursor;
{
  const struct group_cursor *gc;

  for (gc = group_cursor; gc != NULL; gc = gc->next)
    {
      fprintf (e, "// width: %d; height %d; planes %d; bits %d\n",
	     gc->width, gc->height, gc->planes, gc->bits);
      fprintf (e, "// data bytes: %lu; index: %d\n",
	       gc->bytes, gc->index);
    }
}

/* Write dialog data.  */

static void
write_rc_dialog (e, dialog)
     FILE *e;
     const struct dialog *dialog;
{
  const struct dialog_control *control;

  fprintf (e, "STYLE 0x%lx\n", dialog->style);

  if (dialog->exstyle != 0)
    fprintf (e, "EXSTYLE 0x%lx\n", dialog->exstyle);

  if ((dialog->class.named && dialog->class.u.n.length > 0)
      || dialog->class.u.id != 0)
    {
      fprintf (e, "CLASS ");
      res_id_print (e, dialog->class, 1);
      fprintf (e, "\n");
    }

  if (dialog->caption != NULL)
    {
      fprintf (e, "CAPTION \"");
      unicode_print (e, dialog->caption, -1);
      fprintf (e, "\"\n");
    }

  if ((dialog->menu.named && dialog->menu.u.n.length > 0)
      || dialog->menu.u.id != 0)
    {
      fprintf (e, "MENU ");
      res_id_print (e, dialog->menu, 0);
      fprintf (e, "\n");
    }

  if (dialog->font != NULL)
    {
      fprintf (e, "FONT %d, \"", dialog->pointsize);
      unicode_print (e, dialog->font, -1);
      fprintf (e, "\"");
      if (dialog->ex != NULL
	  && (dialog->ex->weight != 0
	      || dialog->ex->italic != 0
	      || dialog->ex->charset != 1))
	fprintf (e, ", %d, %d, %d",
		 dialog->ex->weight, dialog->ex->italic, dialog->ex->charset);
      fprintf (e, "\n");
    }

  fprintf (e, "BEGIN\n");

  for (control = dialog->controls; control != NULL; control = control->next)
    write_rc_dialog_control (e, control);

  fprintf (e, "END\n");
}

/* For each predefined control keyword, this table provides the class
   and the style.  */

struct control_info
{
  const char *name;
  unsigned short class;
  unsigned long style;
};

static const struct control_info control_info[] =
{
  { "AUTO3STATE", CTL_BUTTON, BS_AUTO3STATE },
  { "AUTOCHECKBOX", CTL_BUTTON, BS_AUTOCHECKBOX },
  { "AUTORADIOBUTTON", CTL_BUTTON, BS_AUTORADIOBUTTON },
  { "CHECKBOX", CTL_BUTTON, BS_CHECKBOX },
  { "COMBOBOX", CTL_COMBOBOX, (unsigned long) -1 },
  { "CTEXT", CTL_STATIC, SS_CENTER },
  { "DEFPUSHBUTTON", CTL_BUTTON, BS_DEFPUSHBUTTON },
  { "EDITTEXT", CTL_EDIT, (unsigned long) -1 },
  { "GROUPBOX", CTL_BUTTON, BS_GROUPBOX },
  { "ICON", CTL_STATIC, SS_ICON },
  { "LISTBOX", CTL_LISTBOX, (unsigned long) -1 },
  { "LTEXT", CTL_STATIC, SS_LEFT },
  { "PUSHBOX", CTL_BUTTON, BS_PUSHBOX },
  { "PUSHBUTTON", CTL_BUTTON, BS_PUSHBUTTON },
  { "RADIOBUTTON", CTL_BUTTON, BS_RADIOBUTTON },
  { "RTEXT", CTL_STATIC, SS_RIGHT },
  { "SCROLLBAR", CTL_SCROLLBAR, (unsigned long) -1 },
  { "STATE3", CTL_BUTTON, BS_3STATE },
  /* It's important that USERBUTTON come after all the other button
     types, so that it won't be matched too early.  */
  { "USERBUTTON", CTL_BUTTON, (unsigned long) -1 },
  { NULL, 0, 0 }
};

/* Write a dialog control.  */

static void
write_rc_dialog_control (e, control)
     FILE *e;
     const struct dialog_control *control;
{
  const struct control_info *ci;

  fprintf (e, "  ");

  if (control->class.named)
    ci = NULL;
  else
    {
      for (ci = control_info; ci->name != NULL; ++ci)
	if (ci->class == control->class.u.id
	    && (ci->style == (unsigned long) -1
		|| ci->style == (control->style & 0xff)))
	  break;
    }
  if (ci == NULL)
    fprintf (e, "CONTROL");
  else if (ci->name != NULL)
    fprintf (e, "%s", ci->name);
  else
    fprintf (e, "CONTROL");

  if (control->text.named || control->text.u.id != 0)
    {
      fprintf (e, " ");
      res_id_print (e, control->text, 1);
      fprintf (e, ",");
    }

  fprintf (e, " %d, ", control->id);

  if (ci == NULL)
    {
      if (control->class.named)
	fprintf (e, "\"");
      res_id_print (e, control->class, 0);
      if (control->class.named)
	fprintf (e, "\"");
      fprintf (e, ", 0x%lx, ", control->style);
    }

  fprintf (e, "%d, %d", control->x, control->y);

  if (control->style != SS_ICON
      || control->exstyle != 0
      || control->width != 0
      || control->height != 0
      || control->help != 0)
    {
      fprintf (e, ", %d, %d", control->width, control->height);

      /* FIXME: We don't need to print the style if it is the default.
	 More importantly, in certain cases we actually need to turn
	 off parts of the forced style, by using NOT.  */
      fprintf (e, ", 0x%lx", control->style);

      if (control->exstyle != 0 || control->help != 0)
	fprintf (e, ", 0x%lx, %lu", control->exstyle, control->help);
    }

  fprintf (e, "\n");

  if (control->data != NULL)
    write_rc_rcdata (e, control->data, 2);
}

/* Write out font directory data.  This would normally be built from
   the font data.  */

static void
write_rc_fontdir (e, fontdir)
     FILE *e;
     const struct fontdir *fontdir;
{
  const struct fontdir *fc;

  for (fc = fontdir; fc != NULL; fc = fc->next)
    {
      fprintf (e, "// Font index: %d\n", fc->index);
      write_rc_filedata (e, fc->length, fc->data);
    }
}

/* Write out group icon data.  This would normally be built from the
   icon data.  */

static void
write_rc_group_icon (e, group_icon)
     FILE *e;
     const struct group_icon *group_icon;
{
  const struct group_icon *gi;

  for (gi = group_icon; gi != NULL; gi = gi->next)
    {
      fprintf (e, "// width: %d; height %d; colors: %d; planes %d; bits %d\n",
	       gi->width, gi->height, gi->colors, gi->planes, gi->bits);
      fprintf (e, "// data bytes: %lu; index: %d\n",
	       gi->bytes, gi->index);
    }
}

/* Write out a menu resource.  */

static void
write_rc_menu (e, menu, menuex)
     FILE *e;
     const struct menu *menu;
     int menuex;
{
  if (menu->help != 0)
    fprintf (e, "// Help ID: %lu\n", menu->help);
  write_rc_menuitems (e, menu->items, menuex, 0);
}

/* Write out menuitems.  */

static void
write_rc_menuitems (e, menuitems, menuex, ind)
     FILE *e;
     const struct menuitem *menuitems;
     int menuex;
     int ind;
{
  const struct menuitem *mi;

  indent (e, ind);
  fprintf (e, "BEGIN\n");

  for (mi = menuitems; mi != NULL; mi = mi->next)
    {
      indent (e, ind + 2);

      if (mi->popup == NULL)
	fprintf (e, "MENUITEM");
      else
	fprintf (e, "POPUP");

      if (! menuex
	  && mi->popup == NULL
	  && mi->text == NULL
	  && mi->type == 0
	  && mi->id == 0)
	{
	  fprintf (e, " SEPARATOR\n");
	  continue;
	}

      if (mi->text == NULL)
	fprintf (e, " \"\"");
      else
	{
	  fprintf (e, " \"");
	  unicode_print (e, mi->text, -1);
	  fprintf (e, "\"");
	}

      if (! menuex)
	{
	  if (mi->popup == NULL)
	    fprintf (e, ", %d", mi->id);

	  if ((mi->type & MENUITEM_CHECKED) != 0)
	    fprintf (e, ", CHECKED");
	  if ((mi->type & MENUITEM_GRAYED) != 0)
	    fprintf (e, ", GRAYED");
	  if ((mi->type & MENUITEM_HELP) != 0)
	    fprintf (e, ", HELP");
	  if ((mi->type & MENUITEM_INACTIVE) != 0)
	    fprintf (e, ", INACTIVE");
	  if ((mi->type & MENUITEM_MENUBARBREAK) != 0)
	    fprintf (e, ", MENUBARBREAK");
	  if ((mi->type & MENUITEM_MENUBREAK) != 0)
	    fprintf (e, ", MENUBREAK");
	}
      else
	{
	  if (mi->id != 0 || mi->type != 0 || mi->state != 0 || mi->help != 0)
	    {
	      fprintf (e, ", %d", mi->id);
	      if (mi->type != 0 || mi->state != 0 || mi->help != 0)
		{
		  fprintf (e, ", %lu", mi->type);
		  if (mi->state != 0 || mi->help != 0)
		    {
		      fprintf (e, ", %lu", mi->state);
		      if (mi->help != 0)
			fprintf (e, ", %lu", mi->help);
		    }
		}
	    }
	}

      fprintf (e, "\n");

      if (mi->popup != NULL)
	write_rc_menuitems (e, mi->popup, menuex, ind + 2);
    }

  indent (e, ind);
  fprintf (e, "END\n");
}

/* Write out an rcdata resource.  This is also used for other types of
   resources that need to print arbitrary data.  */

static void
write_rc_rcdata (e, rcdata, ind)
     FILE *e;
     const struct rcdata_item *rcdata;
     int ind;
{
  const struct rcdata_item *ri;

  indent (e, ind);
  fprintf (e, "BEGIN\n");

  for (ri = rcdata; ri != NULL; ri = ri->next)
    {
      if (ri->type == RCDATA_BUFFER && ri->u.buffer.length == 0)
	continue;

      indent (e, ind + 2);

      switch (ri->type)
	{
	default:
	  abort ();

	case RCDATA_WORD:
	  fprintf (e, "%d", ri->u.word);
	  break;

	case RCDATA_DWORD:
	  fprintf (e, "%luL", ri->u.dword);
	  break;

	case RCDATA_STRING:
	  {
	    const char *s;
	    unsigned long i;

	    fprintf (e, "\"");
	    s = ri->u.string.s;
	    for (i = 0; i < ri->u.string.length; i++)
	      {
		if (ISPRINT (*s))
		  putc (*s, e);
		else
		  fprintf (e, "\\%03o", *s);
	      }
	    fprintf (e, "\"");
	    break;
	  }

	case RCDATA_WSTRING:
	  fprintf (e, "L\"");
	  unicode_print (e, ri->u.wstring.w, ri->u.wstring.length);
	  fprintf (e, "\"");
	  break;

	case RCDATA_BUFFER:
	  {
	    unsigned long i;
	    int first;

	    /* Assume little endian data.  */

	    first = 1;
	    for (i = 0; i + 3 < ri->u.buffer.length; i += 4)
	      {
		unsigned long l;
		int j;

		if (! first)
		  indent (e, ind + 2);
		l = ((((((ri->u.buffer.data[i + 3] << 8)
			 | ri->u.buffer.data[i + 2]) << 8)
		       | ri->u.buffer.data[i + 1]) << 8)
		     | ri->u.buffer.data[i]);
		fprintf (e, "%luL", l);
		if (i + 4 < ri->u.buffer.length || ri->next != NULL)
		  fprintf (e, ",");
		for (j = 0; j < 4; ++j)
		  if (! ISPRINT (ri->u.buffer.data[i + j])
		      && ri->u.buffer.data[i + j] != 0)
		    break;
		if (j >= 4)
		  {
		    fprintf (e, "\t// ");
		    for (j = 0; j < 4; ++j)
		      {
			if (! ISPRINT (ri->u.buffer.data[i + j]))
			  fprintf (e, "\\%03o", ri->u.buffer.data[i + j]);
			else
			  {
			    if (ri->u.buffer.data[i + j] == '\\')
			      fprintf (e, "\\");
			    fprintf (e, "%c", ri->u.buffer.data[i + j]);
			  }
		      }
		  }
		fprintf (e, "\n");
		first = 0;
	      }

	    if (i + 1 < ri->u.buffer.length)
	      {
		int s;
		int j;

		if (! first)
		  indent (e, ind + 2);
		s = (ri->u.buffer.data[i + 1] << 8) | ri->u.buffer.data[i];
		fprintf (e, "%d", s);
		if (i + 2 < ri->u.buffer.length || ri->next != NULL)
		  fprintf (e, ",");
		for (j = 0; j < 2; ++j)
		  if (! ISPRINT (ri->u.buffer.data[i + j])
		      && ri->u.buffer.data[i + j] != 0)
		    break;
		if (j >= 2)
		  {
		    fprintf (e, "\t// ");
		    for (j = 0; j < 2; ++j)
		      {
			if (! ISPRINT (ri->u.buffer.data[i + j]))
			  fprintf (e, "\\%03o", ri->u.buffer.data[i + j]);
			else
			  {
			    if (ri->u.buffer.data[i + j] == '\\')
			      fprintf (e, "\\");
			    fprintf (e, "%c", ri->u.buffer.data[i + j]);
			  }
		      }
		  }
		fprintf (e, "\n");
		i += 2;
		first = 0;
	      }

	    if (i < ri->u.buffer.length)
	      {
		if (! first)
		  indent (e, ind + 2);
		if ((ri->u.buffer.data[i] & 0x7f) == ri->u.buffer.data[i]
		    && ISPRINT (ri->u.buffer.data[i]))
		  fprintf (e, "\"%c\"", ri->u.buffer.data[i]);
		else
		  fprintf (e, "\"\\%03o\"", ri->u.buffer.data[i]);
		if (ri->next != NULL)
		  fprintf (e, ",");
		fprintf (e, "\n");
		first = 0;
	      }

	    break;
	  }
	}

      if (ri->type != RCDATA_BUFFER)
	{
	  if (ri->next != NULL)
	    fprintf (e, ",");
	  fprintf (e, "\n");
	}
    }

  indent (e, ind);
  fprintf (e, "END\n");
}

/* Write out a stringtable resource.  */

static void
write_rc_stringtable (e, name, stringtable)
     FILE *e;
     const struct res_id *name;
     const struct stringtable *stringtable;
{
  unsigned long offset;
  int i;

  if (name != NULL && ! name->named)
    offset = (name->u.id - 1) << 4;
  else
    {
      fprintf (e, "// %s string table name\n",
	       name == NULL ? "Missing" : "Invalid");
      offset = 0;
    }

  fprintf (e, "BEGIN\n");

  for (i = 0; i < 16; i++)
    {
      if (stringtable->strings[i].length != 0)
	{
	  fprintf (e, "  %lu, \"", offset + i);
	  unicode_print (e, stringtable->strings[i].string,
			 stringtable->strings[i].length);
	  fprintf (e, "\"\n");
	}
    }

  fprintf (e, "END\n");
}

/* Write out a versioninfo resource.  */

static void
write_rc_versioninfo (e, versioninfo)
     FILE *e;
     const struct versioninfo *versioninfo;
{
  const struct fixed_versioninfo *f;
  const struct ver_info *vi;

  f = versioninfo->fixed;
  if (f->file_version_ms != 0 || f->file_version_ls != 0)
    fprintf (e, " FILEVERSION %lu, %lu, %lu, %lu\n",
	     (f->file_version_ms >> 16) & 0xffff,
	     f->file_version_ms & 0xffff,
	     (f->file_version_ls >> 16) & 0xffff,
	     f->file_version_ls & 0xffff);
  if (f->product_version_ms != 0 || f->product_version_ls != 0)
    fprintf (e, " PRODUCTVERSION %lu, %lu, %lu, %lu\n",
	     (f->product_version_ms >> 16) & 0xffff,
	     f->product_version_ms & 0xffff,
	     (f->product_version_ls >> 16) & 0xffff,
	     f->product_version_ls & 0xffff);
  if (f->file_flags_mask != 0)
    fprintf (e, " FILEFLAGSMASK 0x%lx\n", f->file_flags_mask);
  if (f->file_flags != 0)
    fprintf (e, " FILEFLAGS 0x%lx\n", f->file_flags);
  if (f->file_os != 0)
    fprintf (e, " FILEOS 0x%lx\n", f->file_os);
  if (f->file_type != 0)
    fprintf (e, " FILETYPE 0x%lx\n", f->file_type);
  if (f->file_subtype != 0)
    fprintf (e, " FILESUBTYPE 0x%lx\n", f->file_subtype);
  if (f->file_date_ms != 0 || f->file_date_ls != 0)
    fprintf (e, "// Date: %lu, %lu\n", f->file_date_ms, f->file_date_ls);

  fprintf (e, "BEGIN\n");

  for (vi = versioninfo->var; vi != NULL; vi = vi->next)
    {
      switch (vi->type)
	{
	case VERINFO_STRING:
	  {
	    const struct ver_stringinfo *vs;

	    fprintf (e, "  BLOCK \"StringFileInfo\"\n");
	    fprintf (e, "  BEGIN\n");
	    fprintf (e, "    BLOCK \"");
	    unicode_print (e, vi->u.string.language, -1);
	    fprintf (e, "\"\n");
	    fprintf (e, "    BEGIN\n");

	    for (vs = vi->u.string.strings; vs != NULL; vs = vs->next)
	      {
		fprintf (e, "      VALUE \"");
		unicode_print (e, vs->key, -1);
		fprintf (e, "\", \"");
		unicode_print (e, vs->value, -1);
		fprintf (e, "\"\n");
	      }

	    fprintf (e, "    END\n");
	    fprintf (e, "  END\n");
	    break;
	  }

	case VERINFO_VAR:
	  {
	    const struct ver_varinfo *vv;

	    fprintf (e, "  BLOCK \"VarFileInfo\"\n");
	    fprintf (e, "  BEGIN\n");
	    fprintf (e, "    VALUE \"");
	    unicode_print (e, vi->u.var.key, -1);
	    fprintf (e, "\"");

	    for (vv = vi->u.var.var; vv != NULL; vv = vv->next)
	      fprintf (e, ", 0x%x, %d", (unsigned int) vv->language,
		       vv->charset);

	    fprintf (e, "\n  END\n");

	    break;
	  }
	}
    }

  fprintf (e, "END\n");
}

/* Write out data which would normally be read from a file.  */

static void
write_rc_filedata (e, length, data)
     FILE *e;
     unsigned long length;
     const unsigned char *data;
{
  unsigned long i;

  for (i = 0; i + 15 < length; i += 16)
    {
      fprintf (e, "// %4lx: ", i);
      fprintf (e, "%02x %02x %02x %02x %02x %02x %02x %02x ",
	       data[i + 0], data[i + 1], data[i + 2], data[i + 3],
	       data[i + 4], data[i + 5], data[i + 6], data[i + 7]);
      fprintf (e, "%02x %02x %02x %02x %02x %02x %02x %02x\n",
	       data[i +  8], data[i +  9], data[i + 10], data[i + 11],
	       data[i + 12], data[i + 13], data[i + 14], data[i + 15]);
    }

  if (i < length)
    {
      fprintf (e, "// %4lx:", i);
      while (i < length)
	{
	  fprintf (e, " %02x", data[i]);
	  ++i;
	}
      fprintf (e, "\n");
    }
}
