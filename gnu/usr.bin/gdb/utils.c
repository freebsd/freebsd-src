/*-
 * This code is derived from software copyrighted by the Free Software
 * Foundation.
 *
 * Modified 1991 by Donn Seeley at UUNET Technologies, Inc.
 * Modified 1990 by Van Jacobson at Lawrence Berkeley Laboratory.
 *
 * $Header: /home/cvs/386BSD/src/usr.bin/gdb/utils.c,v 1.1.1.1 1993/06/12 14:52:20 rgrimes Exp $;
 */

#ifndef lint
static char sccsid[] = "@(#)utils.c	6.4 (Berkeley) 5/8/91";
#endif /* not lint */

/* General utility routines for GDB, the GNU debugger.
   Copyright (C) 1986, 1989 Free Software Foundation, Inc.

This file is part of GDB.

GDB is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

GDB is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GDB; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include "param.h"

#include <stdio.h>
#include <ctype.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <pwd.h>
#include "defs.h"
#ifdef HAVE_TERMIO
#include <termio.h>
#endif

/* If this definition isn't overridden by the header files, assume
   that isatty and fileno exist on this system.  */
#ifndef ISATTY
#define ISATTY(FP)	(isatty (fileno (FP)))
#endif

extern FILE *instream;

void error ();
void fatal ();

/* Chain of cleanup actions established with make_cleanup,
   to be executed if an error happens.  */

static struct cleanup *cleanup_chain;

/* Nonzero means a quit has been requested.  */

int quit_flag;

/* Nonzero means quit immediately if Control-C is typed now,
   rather than waiting until QUIT is executed.  */

int immediate_quit;

/* Add a new cleanup to the cleanup_chain,
   and return the previous chain pointer
   to be passed later to do_cleanups or discard_cleanups.
   Args are FUNCTION to clean up with, and ARG to pass to it.  */

struct cleanup *
make_cleanup (function, arg)
     void (*function) ();
     int arg;
{
  register struct cleanup *new
    = (struct cleanup *) xmalloc (sizeof (struct cleanup));
  register struct cleanup *old_chain = cleanup_chain;

  new->next = cleanup_chain;
  new->function = function;
  new->arg = arg;
  cleanup_chain = new;

  return old_chain;
}

/* Discard cleanups and do the actions they describe
   until we get back to the point OLD_CHAIN in the cleanup_chain.  */

void
do_cleanups (old_chain)
     register struct cleanup *old_chain;
{
  register struct cleanup *ptr;
  while ((ptr = cleanup_chain) != old_chain)
    {
      (*ptr->function) (ptr->arg);
      cleanup_chain = ptr->next;
      free (ptr);
    }
}

/* Discard cleanups, not doing the actions they describe,
   until we get back to the point OLD_CHAIN in the cleanup_chain.  */

void
discard_cleanups (old_chain)
     register struct cleanup *old_chain;
{
  register struct cleanup *ptr;
  while ((ptr = cleanup_chain) != old_chain)
    {
      cleanup_chain = ptr->next;
      free (ptr);
    }
}

/* Set the cleanup_chain to 0, and return the old cleanup chain.  */
struct cleanup *
save_cleanups ()
{
  struct cleanup *old_chain = cleanup_chain;

  cleanup_chain = 0;
  return old_chain;
}

/* Restore the cleanup chain from a previously saved chain.  */
void
restore_cleanups (chain)
     struct cleanup *chain;
{
  cleanup_chain = chain;
}

/* This function is useful for cleanups.
   Do

     foo = xmalloc (...);
     old_chain = make_cleanup (free_current_contents, &foo);

   to arrange to free the object thus allocated.  */

void
free_current_contents (location)
     char **location;
{
  free (*location);
}

/* Generally useful subroutines used throughout the program.  */

/* Like malloc but get error if no storage available.  */

char *
xmalloc (size)
     long size;
{
  register char *val = (char *) malloc (size);
  if (!val)
    fatal ("virtual memory exhausted.", 0);
  return val;
}

/* Like realloc but get error if no storage available.  */

char *
xrealloc (ptr, size)
     char *ptr;
     long size;
{
  register char *val = (char *) realloc (ptr, size);
  if (!val)
    fatal ("virtual memory exhausted.", 0);
  return val;
}

/* Print the system error message for errno, and also mention STRING
   as the file name for which the error was encountered.
   Then return to command level.  */

void
perror_with_name (string)
     char *string;
{
  extern int sys_nerr;
  extern char *sys_errlist[];
  extern int errno;
  char *err;
  char *combined;

  if (errno < sys_nerr)
    err = sys_errlist[errno];
  else
    err = "unknown error";

  combined = (char *) alloca (strlen (err) + strlen (string) + 3);
  strcpy (combined, string);
  strcat (combined, ": ");
  strcat (combined, err);

  error ("%s.", combined);
}

/* Print the system error message for ERRCODE, and also mention STRING
   as the file name for which the error was encountered.  */

void
print_sys_errmsg (string, errcode)
     char *string;
     int errcode;
{
  extern int sys_nerr;
  extern char *sys_errlist[];
  char *err;
  char *combined;

  if (errcode < sys_nerr)
    err = sys_errlist[errcode];
  else
    err = "unknown error";

  combined = (char *) alloca (strlen (err) + strlen (string) + 3);
  strcpy (combined, string);
  strcat (combined, ": ");
  strcat (combined, err);

  printf ("%s.\n", combined);
}

void
quit ()
{
#ifdef HAVE_TERMIO
  ioctl (fileno (stdout), TCFLSH, 1);
#else /* not HAVE_TERMIO */
  ioctl (fileno (stdout), TIOCFLUSH, 0);
#endif /* not HAVE_TERMIO */
#ifdef TIOCGPGRP
  error ("Quit");
#else
  error ("Quit (expect signal %d when inferior is resumed)", SIGINT);
#endif /* TIOCGPGRP */
}

/* Control C comes here */

void
request_quit ()
{
  extern int remote_debugging;

  quit_flag = 1;

#ifdef USG
  /* Restore the signal handler.  */
  signal (SIGINT, request_quit);
#endif

  if (immediate_quit)
	  quit();
}

/* Print an error message and return to command level.
   STRING is the error message, used as a fprintf string,
   and ARG is passed as an argument to it.  */

void
error (string, arg1, arg2, arg3)
     char *string;
     int arg1, arg2, arg3;
{
  terminal_ours ();		/* Should be ok even if no inf.  */
  fflush (stdout);
  fprintf (stderr, string, arg1, arg2, arg3);
  fprintf (stderr, "\n");
  return_to_top_level ();
}

/* Print an error message and exit reporting failure.
   This is for a error that we cannot continue from.
   STRING and ARG are passed to fprintf.  */

void
fatal (string, arg)
     char *string;
     int arg;
{
  fprintf (stderr, "gdb: ");
  fprintf (stderr, string, arg);
  fprintf (stderr, "\n");
  exit (1);
}

/* Print an error message and exit, dumping core.
   STRING is a printf-style control string, and ARG is a corresponding
   argument.  */
void
fatal_dump_core (string, arg)
     char *string;
     int arg;
{
  /* "internal error" is always correct, since GDB should never dump
     core, no matter what the input.  */
  fprintf (stderr, "gdb internal error: ");
  fprintf (stderr, string, arg);
  fprintf (stderr, "\n");
  signal (SIGQUIT, SIG_DFL);
  kill (getpid (), SIGQUIT);
  /* We should never get here, but just in case...  */
  exit (1);
}

/* Make a copy of the string at PTR with SIZE characters
   (and add a null character at the end in the copy).
   Uses malloc to get the space.  Returns the address of the copy.  */

char *
savestring (ptr, size)
     char *ptr;
     int size;
{
  register char *p = (char *) xmalloc (size + 1);
  bcopy (ptr, p, size);
  p[size] = 0;
  return p;
}

char *
concat (s1, s2, s3)
     char *s1, *s2, *s3;
{
  register int len = strlen (s1) + strlen (s2) + strlen (s3) + 1;
  register char *val = (char *) xmalloc (len);
  strcpy (val, s1);
  strcat (val, s2);
  strcat (val, s3);
  return val;
}

void
print_spaces (n, file)
     register int n;
     register FILE *file;
{
  while (n-- > 0)
    fputc (' ', file);
}

/* Ask user a y-or-n question and return 1 iff answer is yes.
   Takes three args which are given to printf to print the question.
   The first, a control string, should end in "? ".
   It should not say how to answer, because we do that.  */

int
query (ctlstr, arg1, arg2)
     char *ctlstr;
{
  register int answer;

  /* Automatically answer "yes" if input is not from a terminal.  */
  if (!input_from_terminal_p ())
    return 1;

  while (1)
    {
      printf (ctlstr, arg1, arg2);
      printf ("(y or n) ");
      fflush (stdout);
      answer = fgetc (stdin);
      clearerr (stdin);		/* in case of C-d */
      if (answer != '\n')
	while (fgetc (stdin) != '\n') clearerr (stdin);
      if (answer >= 'a')
	answer -= 040;
      if (answer == 'Y')
	return 1;
      if (answer == 'N')
	return 0;
      printf ("Please answer y or n.\n");
    }
}

/* Parse a C escape sequence.  STRING_PTR points to a variable
   containing a pointer to the string to parse.  That pointer
   is updated past the characters we use.  The value of the
   escape sequence is returned.

   A negative value means the sequence \ newline was seen,
   which is supposed to be equivalent to nothing at all.

   If \ is followed by a null character, we return a negative
   value and leave the string pointer pointing at the null character.

   If \ is followed by 000, we return 0 and leave the string pointer
   after the zeros.  A value of 0 does not mean end of string.  */

int
parse_escape (string_ptr)
     char **string_ptr;
{
  register int c = *(*string_ptr)++;
  switch (c)
    {
    case 'a':
      return '\a';
    case 'b':
      return '\b';
    case 'e':
      return 033;
    case 'f':
      return '\f';
    case 'n':
      return '\n';
    case 'r':
      return '\r';
    case 't':
      return '\t';
    case 'v':
      return '\v';
    case '\n':
      return -2;
    case 0:
      (*string_ptr)--;
      return 0;
    case '^':
      c = *(*string_ptr)++;
      if (c == '\\')
	c = parse_escape (string_ptr);
      if (c == '?')
	return 0177;
      return (c & 0200) | (c & 037);
      
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
      {
	register int i = c - '0';
	register int count = 0;
	while (++count < 3)
	  {
	    if ((c = *(*string_ptr)++) >= '0' && c <= '7')
	      {
		i *= 8;
		i += c - '0';
	      }
	    else
	      {
		(*string_ptr)--;
		break;
	      }
	  }
	return i;
      }
    default:
      return c;
    }
}

/* Print the character CH on STREAM as part of the contents
   of a literal string whose delimiter is QUOTER.  */

void
printchar (ch, stream, quoter)
     unsigned char ch;
     FILE *stream;
     int quoter;
{
  register int c = ch;
  if (c < 040 || c >= 0177)
    switch (c)
      {
      case '\n':
	fputs_filtered ("\\n", stream);
	break;
      case '\b':
	fputs_filtered ("\\b", stream);
	break;
      case '\t':
	fputs_filtered ("\\t", stream);
	break;
      case '\f':
	fputs_filtered ("\\f", stream);
	break;
      case '\r':
	fputs_filtered ("\\r", stream);
	break;
      case '\033':
	fputs_filtered ("\\e", stream);
	break;
      case '\007':
	fputs_filtered ("\\a", stream);
	break;
      default:
	fprintf_filtered (stream, "\\%.3o", (unsigned int) c);
	break;
      }
  else
    {
      if (c == '\\' || c == quoter)
	fputs_filtered ("\\", stream);
      fprintf_filtered (stream, "%c", c);
    }
}

static int lines_per_page, lines_printed, chars_per_line, chars_printed;

/* Set values of page and line size.  */
static void
set_screensize_command (arg, from_tty)
     char *arg;
     int from_tty;
{
  char *p = arg;
  char *p1;
  int tolinesize = lines_per_page;
  int tocharsize = chars_per_line;

  if (p == 0)
    error_no_arg ("set screensize");

  while (*p >= '0' && *p <= '9')
    p++;

  if (*p && *p != ' ' && *p != '\t')
    error ("Non-integral argument given to \"set screensize\".");

  tolinesize = atoi (arg);

  while (*p == ' ' || *p == '\t')
    p++;

  if (*p)
    {
      p1 = p;
      while (*p1 >= '0' && *p1 <= '9')
	p1++;

      if (*p1)
	error ("Non-integral second argument given to \"set screensize\".");

      tocharsize = atoi (p);
    }

  lines_per_page = tolinesize;
  chars_per_line = tocharsize;
}

static void
instream_cleanup(stream)
    FILE *stream;
{
  instream = stream;
}

static void
prompt_for_continue ()
{
  if (ISATTY(stdin) && ISATTY(stdout))
    {
      struct cleanup *old_chain = make_cleanup(instream_cleanup, instream);
      char *cp, *gdb_readline();

      instream = stdin;
      immediate_quit++;
      if (cp = gdb_readline ("---Type <return> to continue---"))
	free(cp);
      chars_printed = lines_printed = 0;
      immediate_quit--;
      do_cleanups(old_chain);
    }
}

/* Reinitialize filter; ie. tell it to reset to original values.  */

void
reinitialize_more_filter ()
{
  lines_printed = 0;
  chars_printed = 0;
}

static void
screensize_info (arg, from_tty)
     char *arg;
     int from_tty;
{
  if (arg)
    error ("\"info screensize\" does not take any arguments.");
  
  if (!lines_per_page)
    printf ("Output more filtering is disabled.\n");
  else
    {
      printf ("Output more filtering is enabled with\n");
      printf ("%d lines per page and %d characters per line.\n",
	      lines_per_page, chars_per_line);
    }
}

/* Like fputs but pause after every screenful.
   Unlike fputs, fputs_filtered does not return a value.
   It is OK for LINEBUFFER to be NULL, in which case just don't print
   anything.

   Note that a longjmp to top level may occur in this routine
   (since prompt_for_continue may do so) so this routine should not be
   called when cleanups are not in place.  */

void
fputs_filtered (linebuffer, stream)
     char *linebuffer;
     FILE *stream;
{
  char *lineptr;

  if (linebuffer == 0)
    return;
  
  /* Don't do any filtering if it is disabled.  */
  if (stream != stdout || !ISATTY(stdout) || lines_per_page == 0)
    {
      fputs (linebuffer, stream);
      return;
    }

  /* Go through and output each character.  Show line extension
     when this is necessary; prompt user for new page when this is
     necessary.  */
  
  lineptr = linebuffer;
  while (*lineptr)
    {
      /* Possible new page.  */
      if (lines_printed >= lines_per_page - 1)
	prompt_for_continue ();

      while (*lineptr && *lineptr != '\n')
	{
	  /* Print a single line.  */
	  if (*lineptr == '\t')
	    {
	      putc ('\t', stream);
	      /* Shifting right by 3 produces the number of tab stops
	         we have already passed, and then adding one and
		 shifting left 3 advances to the next tab stop.  */
	      chars_printed = ((chars_printed >> 3) + 1) << 3;
	      lineptr++;
	    }
	  else
	    {
	      putc (*lineptr, stream);
	      chars_printed++;
	      lineptr++;
	    }
      
	  if (chars_printed >= chars_per_line)
	    {
	      chars_printed = 0;
	      lines_printed++;
	      /* Possible new page.  */
	      if (lines_printed >= lines_per_page - 1)
		prompt_for_continue ();
	    }
	}

      if (*lineptr == '\n')
	{
	  lines_printed++;
	  putc ('\n', stream);
	  lineptr++;
	  chars_printed = 0;
	}
    }
}

/* fputs_demangled is a variant of fputs_filtered that
   demangles g++ names.*/

void
fputs_demangled (linebuffer, stream, arg_mode)
     char *linebuffer;
     FILE *stream;
{
#ifdef __STDC__
  extern char *cplus_demangle (const char *, int);
#else
  extern char *cplus_demangle ();
#endif
#define SYMBOL_MAX 1024

#define SYMBOL_CHAR(c) (isascii(c) && (isalnum(c) || (c) == '_' || (c) == '$'))

  char buf[SYMBOL_MAX+1];
  char *p;

  if (linebuffer == NULL)
    return;

  p = linebuffer;

  while ( *p != (char) 0 ) {
    int i = 0;

    /* collect non-interesting characters into buf */
    while ( *p != (char) 0 && !SYMBOL_CHAR(*p) ) {
      buf[i++] = *p;
      p++;
    }
    if (i > 0) {
      /* output the non-interesting characters without demangling */
      buf[i] = (char) 0;
      fputs_filtered(buf, stream);
      i = 0;  /* reset buf */
    }

    /* and now the interesting characters */
    while (i < SYMBOL_MAX && *p != (char) 0 && SYMBOL_CHAR(*p) ) {
      buf[i++] = *p;
      p++;
    }
    buf[i] = (char) 0;
    if (i > 0) {
      char * result;
      
      if ( (result = cplus_demangle(buf, arg_mode)) != NULL ) {
	fputs_filtered(result, stream);
	free(result);
      }
      else {
	fputs_filtered(buf, stream);
      }
    }
  }
}

/* Print ARG1, ARG2, and ARG3 on stdout using format FORMAT.  If this
   information is going to put the amount written since the last call
   to INIIALIZE_MORE_FILTER or the last page break over the page size,
   print out a pause message and do a gdb_readline to get the users
   permision to continue.

   Unlike fprintf, this function does not return a value.

   Note that this routine has a restriction that the length of the
   final output line must be less than 255 characters *or* it must be
   less than twice the size of the format string.  This is a very
   arbitrary restriction, but it is an internal restriction, so I'll
   put it in.  This means that the %s format specifier is almost
   useless; unless the caller can GUARANTEE that the string is short
   enough, fputs_filtered should be used instead.

   Note also that a longjmp to top level may occur in this routine
   (since prompt_for_continue may do so) so this routine should not be
   called when cleanups are not in place.  */

void
fprintf_filtered (stream, format, arg1, arg2, arg3, arg4, arg5, arg6)
     FILE *stream;
     char *format;
     int arg1, arg2, arg3, arg4, arg5, arg6;
{
  static char *linebuffer = (char *) 0;
  static int line_size;
  int format_length = strlen (format);
  int numchars;

  /* Allocated linebuffer for the first time.  */
  if (!linebuffer)
    {
      linebuffer = (char *) xmalloc (255);
      line_size = 255;
    }

  /* Reallocate buffer to a larger size if this is necessary.  */
  if (format_length * 2 > line_size)
    {
      line_size = format_length * 2;

      /* You don't have to copy.  */
      free (linebuffer);
      linebuffer = (char *) xmalloc (line_size);
    }

  /* This won't blow up if the restrictions described above are
     followed.   */
  (void) sprintf (linebuffer, format, arg1, arg2, arg3, arg4, arg5, arg6);

  fputs_filtered (linebuffer, stream);
}

void
printf_filtered (format, arg1, arg2, arg3, arg4, arg5, arg6)
     char *format;
     int arg1, arg2, arg3, arg4, arg5, arg6;
{
  fprintf_filtered (stdout, format, arg1, arg2, arg3, arg4, arg5, arg6);
}

/* Print N spaces.  */
void
print_spaces_filtered (n, stream)
     int n;
     FILE *stream;
{
  register char *s = (char *) alloca (n + 1);
  register char *t = s;

  while (n--)
    *t++ = ' ';
  *t = '\0';

  fputs_filtered (s, stream);
}


#ifdef USG
bcopy (from, to, count)
char *from, *to;
{
	memcpy (to, from, count);
}

bcmp (from, to, count)
{
	return (memcmp (to, from, count));
}

bzero (to, count)
char *to;
{
	while (count--)
		*to++ = 0;
}

getwd (buf)
char *buf;
{
  getcwd (buf, MAXPATHLEN);
}

char *
index (s, c)
     char *s;
{
  char *strchr ();
  return strchr (s, c);
}

char *
rindex (s, c)
     char *s;
{
  char *strrchr ();
  return strrchr (s, c);
}

#ifndef USG
char *sys_siglist[32] = {
	"SIG0",
	"SIGHUP",
	"SIGINT",
	"SIGQUIT",
	"SIGILL",
	"SIGTRAP",
	"SIGIOT",
	"SIGEMT",
	"SIGFPE",
	"SIGKILL",
	"SIGBUS",
	"SIGSEGV",
	"SIGSYS",
	"SIGPIPE",
	"SIGALRM",
	"SIGTERM",
	"SIGUSR1",
	"SIGUSR2",
	"SIGCLD",
	"SIGPWR",
	"SIGWIND",
	"SIGPHONE",
	"SIGPOLL",
};
#endif

/* Queue routines */

struct queue {
	struct queue *forw;
	struct queue *back;
};

insque (item, after)
struct queue *item;
struct queue *after;
{
	item->forw = after->forw;
	after->forw->back = item;

	item->back = after;
	after->forw = item;
}

remque (item)
struct queue *item;
{
	item->forw->back = item->back;
	item->back->forw = item->forw;
}
#endif /* USG */

#ifdef USG
/* There is too much variation in Sys V signal numbers and names, so
   we must initialize them at runtime.  */
static char undoc[] = "(undocumented)";

char *sys_siglist[NSIG];
#endif /* USG */

extern struct cmd_list_element *setlist;

void
_initialize_utils ()
{
  int i;
  add_cmd ("screensize", class_support, set_screensize_command,
	   "Change gdb's notion of the size of the output screen.\n\
The first argument is the number of lines on a page.\n\
The second argument (optional) is the number of characters on a line.",
	   &setlist);
  add_info ("screensize", screensize_info,
	    "Show gdb's current notion of the size of the output screen.");

  /* These defaults will be used if we are unable to get the correct
     values from termcap.  */
  lines_per_page = 24;
  chars_per_line = 80;
  /* Initialize the screen height and width from termcap.  */
  {
    int termtype = getenv ("TERM");

    /* Positive means success, nonpositive means failure.  */
    int status;

    /* 2048 is large enough for all known terminals, according to the
       GNU termcap manual.  */
    char term_buffer[2048];

    if (termtype)
      {
	status = tgetent (term_buffer, termtype);
	if (status > 0)
	  {
	    int val;
	    
	    val = tgetnum ("li");
	    if (val >= 0)
	      lines_per_page = val;
	    else
	      /* The number of lines per page is not mentioned
		 in the terminal description.  This probably means
		 that paging is not useful (e.g. emacs shell window),
		 so disable paging.  */
	      lines_per_page = 0;
	    
	    val = tgetnum ("co");
	    if (val >= 0)
	      chars_per_line = val;
	  }
      }
  }

#ifdef USG
  /* Initialize signal names.  */
	for (i = 0; i < NSIG; i++)
		sys_siglist[i] = undoc;

#ifdef SIGHUP
	sys_siglist[SIGHUP	] = "SIGHUP";
#endif
#ifdef SIGINT
	sys_siglist[SIGINT	] = "SIGINT";
#endif
#ifdef SIGQUIT
	sys_siglist[SIGQUIT	] = "SIGQUIT";
#endif
#ifdef SIGILL
	sys_siglist[SIGILL	] = "SIGILL";
#endif
#ifdef SIGTRAP
	sys_siglist[SIGTRAP	] = "SIGTRAP";
#endif
#ifdef SIGIOT
	sys_siglist[SIGIOT	] = "SIGIOT";
#endif
#ifdef SIGEMT
	sys_siglist[SIGEMT	] = "SIGEMT";
#endif
#ifdef SIGFPE
	sys_siglist[SIGFPE	] = "SIGFPE";
#endif
#ifdef SIGKILL
	sys_siglist[SIGKILL	] = "SIGKILL";
#endif
#ifdef SIGBUS
	sys_siglist[SIGBUS	] = "SIGBUS";
#endif
#ifdef SIGSEGV
	sys_siglist[SIGSEGV	] = "SIGSEGV";
#endif
#ifdef SIGSYS
	sys_siglist[SIGSYS	] = "SIGSYS";
#endif
#ifdef SIGPIPE
	sys_siglist[SIGPIPE	] = "SIGPIPE";
#endif
#ifdef SIGALRM
	sys_siglist[SIGALRM	] = "SIGALRM";
#endif
#ifdef SIGTERM
	sys_siglist[SIGTERM	] = "SIGTERM";
#endif
#ifdef SIGUSR1
	sys_siglist[SIGUSR1	] = "SIGUSR1";
#endif
#ifdef SIGUSR2
	sys_siglist[SIGUSR2	] = "SIGUSR2";
#endif
#ifdef SIGCLD
	sys_siglist[SIGCLD	] = "SIGCLD";
#endif
#ifdef SIGCHLD
	sys_siglist[SIGCHLD	] = "SIGCHLD";
#endif
#ifdef SIGPWR
	sys_siglist[SIGPWR	] = "SIGPWR";
#endif
#ifdef SIGTSTP
	sys_siglist[SIGTSTP	] = "SIGTSTP";
#endif
#ifdef SIGTTIN
	sys_siglist[SIGTTIN	] = "SIGTTIN";
#endif
#ifdef SIGTTOU
	sys_siglist[SIGTTOU	] = "SIGTTOU";
#endif
#ifdef SIGSTOP
	sys_siglist[SIGSTOP	] = "SIGSTOP";
#endif
#ifdef SIGXCPU
	sys_siglist[SIGXCPU	] = "SIGXCPU";
#endif
#ifdef SIGXFSZ
	sys_siglist[SIGXFSZ	] = "SIGXFSZ";
#endif
#ifdef SIGVTALRM
	sys_siglist[SIGVTALRM	] = "SIGVTALRM";
#endif
#ifdef SIGPROF
	sys_siglist[SIGPROF	] = "SIGPROF";
#endif
#ifdef SIGWINCH
	sys_siglist[SIGWINCH	] = "SIGWINCH";
#endif
#ifdef SIGCONT
	sys_siglist[SIGCONT	] = "SIGCONT";
#endif
#ifdef SIGURG
	sys_siglist[SIGURG	] = "SIGURG";
#endif
#ifdef SIGIO
	sys_siglist[SIGIO	] = "SIGIO";
#endif
#ifdef SIGWIND
	sys_siglist[SIGWIND	] = "SIGWIND";
#endif
#ifdef SIGPHONE
	sys_siglist[SIGPHONE	] = "SIGPHONE";
#endif
#ifdef SIGPOLL
	sys_siglist[SIGPOLL	] = "SIGPOLL";
#endif
#endif /* USG */
}
