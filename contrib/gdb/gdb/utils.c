/* General utility routines for GDB, the GNU debugger.
   Copyright 1986, 89, 90, 91, 92, 95, 96, 1998 Free Software Foundation, Inc.

This file is part of GDB.

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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "defs.h"
#include <ctype.h>
#include "gdb_string.h"
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_CURSES_H
#include <curses.h>
#endif
#ifdef HAVE_TERM_H
#include <term.h>
#endif

/* SunOS's curses.h has a '#define reg register' in it.  Thank you Sun. */
#ifdef reg
#undef reg
#endif

#include "signals.h"
#include "gdbcmd.h"
#include "serial.h"
#include "bfd.h"
#include "target.h"
#include "demangle.h"
#include "expression.h"
#include "language.h"
#include "annotate.h"

#include <readline/readline.h>

/* readline defines this.  */
#undef savestring

void (*error_begin_hook) PARAMS ((void));

/* Prototypes for local functions */

static void vfprintf_maybe_filtered PARAMS ((GDB_FILE *, const char *,
					     va_list, int));

static void fputs_maybe_filtered PARAMS ((const char *, GDB_FILE *, int));

#if defined (USE_MMALLOC) && !defined (NO_MMCHECK)
static void malloc_botch PARAMS ((void));
#endif

static void
fatal_dump_core PARAMS((char *, ...));

static void
prompt_for_continue PARAMS ((void));

static void 
set_width_command PARAMS ((char *, int, struct cmd_list_element *));

static void
set_width PARAMS ((void));

/* If this definition isn't overridden by the header files, assume
   that isatty and fileno exist on this system.  */
#ifndef ISATTY
#define ISATTY(FP)	(isatty (fileno (FP)))
#endif

#ifndef GDB_FILE_ISATTY
#define GDB_FILE_ISATTY(GDB_FILE_PTR)   (gdb_file_isatty(GDB_FILE_PTR))   
#endif

/* Chain of cleanup actions established with make_cleanup,
   to be executed if an error happens.  */

static struct cleanup *cleanup_chain; /* cleaned up after a failed command */
static struct cleanup *final_cleanup_chain; /* cleaned up when gdb exits */
static struct cleanup *run_cleanup_chain; /* cleaned up on each 'run' */

/* Nonzero if we have job control. */

int job_control;

/* Nonzero means a quit has been requested.  */

int quit_flag;

/* Nonzero means quit immediately if Control-C is typed now, rather
   than waiting until QUIT is executed.  Be careful in setting this;
   code which executes with immediate_quit set has to be very careful
   about being able to deal with being interrupted at any time.  It is
   almost always better to use QUIT; the only exception I can think of
   is being able to quit out of a system call (using EINTR loses if
   the SIGINT happens between the previous QUIT and the system call).
   To immediately quit in the case in which a SIGINT happens between
   the previous QUIT and setting immediate_quit (desirable anytime we
   expect to block), call QUIT after setting immediate_quit.  */

int immediate_quit;

/* Nonzero means that encoded C++ names should be printed out in their
   C++ form rather than raw.  */

int demangle = 1;

/* Nonzero means that encoded C++ names should be printed out in their
   C++ form even in assembler language displays.  If this is set, but
   DEMANGLE is zero, names are printed raw, i.e. DEMANGLE controls.  */

int asm_demangle = 0;

/* Nonzero means that strings with character values >0x7F should be printed
   as octal escapes.  Zero means just print the value (e.g. it's an
   international character, and the terminal or window can cope.)  */

int sevenbit_strings = 0;

/* String to be printed before error messages, if any.  */

char *error_pre_print;

/* String to be printed before quit messages, if any.  */

char *quit_pre_print;

/* String to be printed before warning messages, if any.  */

char *warning_pre_print = "\nwarning: ";

int pagination_enabled = 1;


/* Add a new cleanup to the cleanup_chain,
   and return the previous chain pointer
   to be passed later to do_cleanups or discard_cleanups.
   Args are FUNCTION to clean up with, and ARG to pass to it.  */

struct cleanup *
make_cleanup (function, arg)
     void (*function) PARAMS ((PTR));
     PTR arg;
{
    return make_my_cleanup (&cleanup_chain, function, arg);
}

struct cleanup *
make_final_cleanup (function, arg)
     void (*function) PARAMS ((PTR));
     PTR arg;
{
    return make_my_cleanup (&final_cleanup_chain, function, arg);
}
struct cleanup *
make_run_cleanup (function, arg)
     void (*function) PARAMS ((PTR));
     PTR arg;
{
    return make_my_cleanup (&run_cleanup_chain, function, arg);
}
struct cleanup *
make_my_cleanup (pmy_chain, function, arg)
     struct cleanup **pmy_chain;
     void (*function) PARAMS ((PTR));
     PTR arg;
{
  register struct cleanup *new
    = (struct cleanup *) xmalloc (sizeof (struct cleanup));
  register struct cleanup *old_chain = *pmy_chain;

  new->next = *pmy_chain;
  new->function = function;
  new->arg = arg;
  *pmy_chain = new;

  return old_chain;
}

/* Discard cleanups and do the actions they describe
   until we get back to the point OLD_CHAIN in the cleanup_chain.  */

void
do_cleanups (old_chain)
     register struct cleanup *old_chain;
{
    do_my_cleanups (&cleanup_chain, old_chain);
}

void
do_final_cleanups (old_chain)
     register struct cleanup *old_chain;
{
    do_my_cleanups (&final_cleanup_chain, old_chain);
}

void
do_run_cleanups (old_chain)
     register struct cleanup *old_chain;
{
    do_my_cleanups (&run_cleanup_chain, old_chain);
}

void
do_my_cleanups (pmy_chain, old_chain)
     register struct cleanup **pmy_chain;
     register struct cleanup *old_chain;
{
  register struct cleanup *ptr;
  while ((ptr = *pmy_chain) != old_chain)
    {
      *pmy_chain = ptr->next;	/* Do this first incase recursion */
      (*ptr->function) (ptr->arg);
      free (ptr);
    }
}

/* Discard cleanups, not doing the actions they describe,
   until we get back to the point OLD_CHAIN in the cleanup_chain.  */

void
discard_cleanups (old_chain)
     register struct cleanup *old_chain;
{
    discard_my_cleanups (&cleanup_chain, old_chain);
}

void
discard_final_cleanups (old_chain)
     register struct cleanup *old_chain;
{
    discard_my_cleanups (&final_cleanup_chain, old_chain);
}

void
discard_my_cleanups (pmy_chain, old_chain)
     register struct cleanup **pmy_chain;
     register struct cleanup *old_chain;
{
  register struct cleanup *ptr;
  while ((ptr = *pmy_chain) != old_chain)
    {
      *pmy_chain = ptr->next;
      free ((PTR)ptr);
    }
}

/* Set the cleanup_chain to 0, and return the old cleanup chain.  */
struct cleanup *
save_cleanups ()
{
    return save_my_cleanups (&cleanup_chain);
}

struct cleanup *
save_final_cleanups ()
{
    return save_my_cleanups (&final_cleanup_chain);
}

struct cleanup *
save_my_cleanups (pmy_chain)
    struct cleanup **pmy_chain;
{
  struct cleanup *old_chain = *pmy_chain;

  *pmy_chain = 0;
  return old_chain;
}

/* Restore the cleanup chain from a previously saved chain.  */
void
restore_cleanups (chain)
     struct cleanup *chain;
{
    restore_my_cleanups (&cleanup_chain, chain);
}

void
restore_final_cleanups (chain)
     struct cleanup *chain;
{
    restore_my_cleanups (&final_cleanup_chain, chain);
}

void
restore_my_cleanups (pmy_chain, chain)
     struct cleanup **pmy_chain;
     struct cleanup *chain;
{
  *pmy_chain = chain;
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

/* Provide a known function that does nothing, to use as a base for
   for a possibly long chain of cleanups.  This is useful where we
   use the cleanup chain for handling normal cleanups as well as dealing
   with cleanups that need to be done as a result of a call to error().
   In such cases, we may not be certain where the first cleanup is, unless
   we have a do-nothing one to always use as the base. */

/* ARGSUSED */
void
null_cleanup (arg)
    PTR arg;
{
}


/* Print a warning message.  Way to use this is to call warning_begin,
   output the warning message (use unfiltered output to gdb_stderr),
   ending in a newline.  There is not currently a warning_end that you
   call afterwards, but such a thing might be added if it is useful
   for a GUI to separate warning messages from other output.

   FIXME: Why do warnings use unfiltered output and errors filtered?
   Is this anything other than a historical accident?  */

void
warning_begin ()
{
  target_terminal_ours ();
  wrap_here("");			/* Force out any buffered output */
  gdb_flush (gdb_stdout);
  if (warning_pre_print)
    fprintf_unfiltered (gdb_stderr, warning_pre_print);
}

/* Print a warning message.
   The first argument STRING is the warning message, used as a fprintf string,
   and the remaining args are passed as arguments to it.
   The primary difference between warnings and errors is that a warning
   does not force the return to command level.  */

/* VARARGS */
void
#ifdef ANSI_PROTOTYPES
warning (const char *string, ...)
#else
warning (va_alist)
     va_dcl
#endif
{
  va_list args;
#ifdef ANSI_PROTOTYPES
  va_start (args, string);
#else
  char *string;

  va_start (args);
  string = va_arg (args, char *);
#endif
  if (warning_hook)
    (*warning_hook) (string, args);
  else
  {
    warning_begin ();
    vfprintf_unfiltered (gdb_stderr, string, args);
    fprintf_unfiltered (gdb_stderr, "\n");
    va_end (args);
  }
}

/* Start the printing of an error message.  Way to use this is to call
   this, output the error message (use filtered output to gdb_stderr
   (FIXME: Some callers, like memory_error, use gdb_stdout)), ending
   in a newline, and then call return_to_top_level (RETURN_ERROR).
   error() provides a convenient way to do this for the special case
   that the error message can be formatted with a single printf call,
   but this is more general.  */
void
error_begin ()
{
  if (error_begin_hook)
    error_begin_hook ();

  target_terminal_ours ();
  wrap_here ("");			/* Force out any buffered output */
  gdb_flush (gdb_stdout);

  annotate_error_begin ();

  if (error_pre_print)
    fprintf_filtered (gdb_stderr, error_pre_print);
}

/* Print an error message and return to command level.
   The first argument STRING is the error message, used as a fprintf string,
   and the remaining args are passed as arguments to it.  */

/* VARARGS */
NORETURN void
#ifdef ANSI_PROTOTYPES
error (const char *string, ...)
#else
error (va_alist)
     va_dcl
#endif
{
  va_list args;
#ifdef ANSI_PROTOTYPES
  va_start (args, string);
#else
  va_start (args);
#endif
  if (error_hook)
    (*error_hook) ();
  else 
    {
      error_begin ();
#ifdef ANSI_PROTOTYPES
      vfprintf_filtered (gdb_stderr, string, args);
#else
      {
	char *string1;

	string1 = va_arg (args, char *);
	vfprintf_filtered (gdb_stderr, string1, args);
      }
#endif
      fprintf_filtered (gdb_stderr, "\n");
      va_end (args);
      return_to_top_level (RETURN_ERROR);
    }
}


/* Print an error message and exit reporting failure.
   This is for a error that we cannot continue from.
   The arguments are printed a la printf.

   This function cannot be declared volatile (NORETURN) in an
   ANSI environment because exit() is not declared volatile. */

/* VARARGS */
NORETURN void
#ifdef ANSI_PROTOTYPES
fatal (char *string, ...)
#else
fatal (va_alist)
     va_dcl
#endif
{
  va_list args;
#ifdef ANSI_PROTOTYPES
  va_start (args, string);
#else
  char *string;
  va_start (args);
  string = va_arg (args, char *);
#endif
  fprintf_unfiltered (gdb_stderr, "\ngdb: ");
  vfprintf_unfiltered (gdb_stderr, string, args);
  fprintf_unfiltered (gdb_stderr, "\n");
  va_end (args);
  exit (1);
}

/* Print an error message and exit, dumping core.
   The arguments are printed a la printf ().  */

/* VARARGS */
static void
#ifdef ANSI_PROTOTYPES
fatal_dump_core (char *string, ...)
#else
fatal_dump_core (va_alist)
     va_dcl
#endif
{
  va_list args;
#ifdef ANSI_PROTOTYPES
  va_start (args, string);
#else
  char *string;

  va_start (args);
  string = va_arg (args, char *);
#endif
  /* "internal error" is always correct, since GDB should never dump
     core, no matter what the input.  */
  fprintf_unfiltered (gdb_stderr, "\ngdb internal error: ");
  vfprintf_unfiltered (gdb_stderr, string, args);
  fprintf_unfiltered (gdb_stderr, "\n");
  va_end (args);

  signal (SIGQUIT, SIG_DFL);
  kill (getpid (), SIGQUIT);
  /* We should never get here, but just in case...  */
  exit (1);
}

/* The strerror() function can return NULL for errno values that are
   out of range.  Provide a "safe" version that always returns a
   printable string. */

char *
safe_strerror (errnum)
     int errnum;
{
  char *msg;
  static char buf[32];

  if ((msg = strerror (errnum)) == NULL)
    {
      sprintf (buf, "(undocumented errno %d)", errnum);
      msg = buf;
    }
  return (msg);
}

/* The strsignal() function can return NULL for signal values that are
   out of range.  Provide a "safe" version that always returns a
   printable string. */

char *
safe_strsignal (signo)
     int signo;
{
  char *msg;
  static char buf[32];

  if ((msg = strsignal (signo)) == NULL)
    {
      sprintf (buf, "(undocumented signal %d)", signo);
      msg = buf;
    }
  return (msg);
}


/* Print the system error message for errno, and also mention STRING
   as the file name for which the error was encountered.
   Then return to command level.  */

NORETURN void
perror_with_name (string)
     char *string;
{
  char *err;
  char *combined;

  err = safe_strerror (errno);
  combined = (char *) alloca (strlen (err) + strlen (string) + 3);
  strcpy (combined, string);
  strcat (combined, ": ");
  strcat (combined, err);

  /* I understand setting these is a matter of taste.  Still, some people
     may clear errno but not know about bfd_error.  Doing this here is not
     unreasonable. */
  bfd_set_error (bfd_error_no_error);
  errno = 0;

  error ("%s.", combined); 
}

/* Print the system error message for ERRCODE, and also mention STRING
   as the file name for which the error was encountered.  */

void
print_sys_errmsg (string, errcode)
     char *string;
     int errcode;
{
  char *err;
  char *combined;

  err = safe_strerror (errcode);
  combined = (char *) alloca (strlen (err) + strlen (string) + 3);
  strcpy (combined, string);
  strcat (combined, ": ");
  strcat (combined, err);

  /* We want anything which was printed on stdout to come out first, before
     this message.  */
  gdb_flush (gdb_stdout);
  fprintf_unfiltered (gdb_stderr, "%s.\n", combined);
}

/* Control C eventually causes this to be called, at a convenient time.  */

void
quit ()
{
  serial_t gdb_stdout_serial = serial_fdopen (1);

  target_terminal_ours ();

  /* We want all output to appear now, before we print "Quit".  We
     have 3 levels of buffering we have to flush (it's possible that
     some of these should be changed to flush the lower-level ones
     too):  */

  /* 1.  The _filtered buffer.  */
  wrap_here ((char *)0);

  /* 2.  The stdio buffer.  */
  gdb_flush (gdb_stdout);
  gdb_flush (gdb_stderr);

  /* 3.  The system-level buffer.  */
  SERIAL_DRAIN_OUTPUT (gdb_stdout_serial);
  SERIAL_UN_FDOPEN (gdb_stdout_serial);

  annotate_error_begin ();

  /* Don't use *_filtered; we don't want to prompt the user to continue.  */
  if (quit_pre_print)
    fprintf_unfiltered (gdb_stderr, quit_pre_print);

  if (job_control
      /* If there is no terminal switching for this target, then we can't
	 possibly get screwed by the lack of job control.  */
      || current_target.to_terminal_ours == NULL)
    fprintf_unfiltered (gdb_stderr, "Quit\n");
  else
    fprintf_unfiltered (gdb_stderr,
	     "Quit (expect signal SIGINT when the program is resumed)\n");
  return_to_top_level (RETURN_QUIT);
}


#if defined(__GO32__)

/* In the absence of signals, poll keyboard for a quit.
   Called from #define QUIT pollquit() in xm-go32.h. */

void
notice_quit()
{
  if (kbhit ())
    switch (getkey ())
      {
      case 1:
	quit_flag = 1;
	break;
      case 2:
	immediate_quit = 2;
	break;
      default:
	/* We just ignore it */
	/* FIXME!! Don't think this actually works! */
	fprintf_unfiltered (gdb_stderr, "CTRL-A to quit, CTRL-B to quit harder\n");
	break;
      }
}

#elif defined(_MSC_VER) /* should test for wingdb instead? */

/*
 * Windows translates all keyboard and mouse events 
 * into a message which is appended to the message 
 * queue for the process.
 */

void notice_quit()
{
  int k = win32pollquit();
  if (k == 1)
    quit_flag = 1;
  else if (k == 2)
    immediate_quit = 1;
}

#else /* !defined(__GO32__) && !defined(_MSC_VER) */

void notice_quit()
{
  /* Done by signals */
}

#endif /* !defined(__GO32__) && !defined(_MSC_VER) */

void
pollquit()
{
  notice_quit ();
  if (quit_flag || immediate_quit)
    quit ();
}

/* Control C comes here */

void
request_quit (signo)
     int signo;
{
  quit_flag = 1;
  /* Restore the signal handler.  Harmless with BSD-style signals, needed
     for System V-style signals.  So just always do it, rather than worrying
     about USG defines and stuff like that.  */
  signal (signo, request_quit);

#ifdef REQUEST_QUIT
  REQUEST_QUIT;
#else
  if (immediate_quit) 
    quit ();
#endif
}


/* Memory management stuff (malloc friends).  */

/* Make a substitute size_t for non-ANSI compilers. */

#ifndef HAVE_STDDEF_H
#ifndef size_t
#define size_t unsigned int
#endif
#endif

#if !defined (USE_MMALLOC)

PTR
mmalloc (md, size)
     PTR md;
     size_t size;
{
  return malloc (size);
}

PTR
mrealloc (md, ptr, size)
     PTR md;
     PTR ptr;
     size_t size;
{
  if (ptr == 0)		/* Guard against old realloc's */
    return malloc (size);
  else
    return realloc (ptr, size);
}

void
mfree (md, ptr)
     PTR md;
     PTR ptr;
{
  free (ptr);
}

#endif	/* USE_MMALLOC */

#if !defined (USE_MMALLOC) || defined (NO_MMCHECK)

void
init_malloc (md)
     PTR md;
{
}

#else /* Have mmalloc and want corruption checking */

static void
malloc_botch ()
{
  fatal_dump_core ("Memory corruption");
}

/* Attempt to install hooks in mmalloc/mrealloc/mfree for the heap specified
   by MD, to detect memory corruption.  Note that MD may be NULL to specify
   the default heap that grows via sbrk.

   Note that for freshly created regions, we must call mmcheckf prior to any
   mallocs in the region.  Otherwise, any region which was allocated prior to
   installing the checking hooks, which is later reallocated or freed, will
   fail the checks!  The mmcheck function only allows initial hooks to be
   installed before the first mmalloc.  However, anytime after we have called
   mmcheck the first time to install the checking hooks, we can call it again
   to update the function pointer to the memory corruption handler.

   Returns zero on failure, non-zero on success. */

#ifndef MMCHECK_FORCE
#define MMCHECK_FORCE 0
#endif

void
init_malloc (md)
     PTR md;
{
  if (!mmcheckf (md, malloc_botch, MMCHECK_FORCE))
    {
      /* Don't use warning(), which relies on current_target being set
	 to something other than dummy_target, until after
	 initialize_all_files(). */

      fprintf_unfiltered
	(gdb_stderr, "warning: failed to install memory consistency checks; ");
      fprintf_unfiltered
	(gdb_stderr, "configuration should define NO_MMCHECK or MMCHECK_FORCE\n");
    }

  mmtrace ();
}

#endif /* Have mmalloc and want corruption checking  */

/* Called when a memory allocation fails, with the number of bytes of
   memory requested in SIZE. */

NORETURN void
nomem (size)
     long size;
{
  if (size > 0)
    {
      fatal ("virtual memory exhausted: can't allocate %ld bytes.", size);
    }
  else
    {
      fatal ("virtual memory exhausted.");
    }
}

/* Like mmalloc but get error if no storage available, and protect against
   the caller wanting to allocate zero bytes.  Whether to return NULL for
   a zero byte request, or translate the request into a request for one
   byte of zero'd storage, is a religious issue. */

PTR
xmmalloc (md, size)
     PTR md;
     long size;
{
  register PTR val;

  if (size == 0)
    {
      val = NULL;
    }
  else if ((val = mmalloc (md, size)) == NULL)
    {
      nomem (size);
    }
  return (val);
}

/* Like mrealloc but get error if no storage available.  */

PTR
xmrealloc (md, ptr, size)
     PTR md;
     PTR ptr;
     long size;
{
  register PTR val;

  if (ptr != NULL)
    {
      val = mrealloc (md, ptr, size);
    }
  else
    {
      val = mmalloc (md, size);
    }
  if (val == NULL)
    {
      nomem (size);
    }
  return (val);
}

/* Like malloc but get error if no storage available, and protect against
   the caller wanting to allocate zero bytes.  */

PTR
xmalloc (size)
     size_t size;
{
  return (xmmalloc ((PTR) NULL, size));
}

/* Like mrealloc but get error if no storage available.  */

PTR
xrealloc (ptr, size)
     PTR ptr;
     size_t size;
{
  return (xmrealloc ((PTR) NULL, ptr, size));
}


/* My replacement for the read system call.
   Used like `read' but keeps going if `read' returns too soon.  */

int
myread (desc, addr, len)
     int desc;
     char *addr;
     int len;
{
  register int val;
  int orglen = len;

  while (len > 0)
    {
      val = read (desc, addr, len);
      if (val < 0)
	return val;
      if (val == 0)
	return orglen - len;
      len -= val;
      addr += val;
    }
  return orglen;
}

/* Make a copy of the string at PTR with SIZE characters
   (and add a null character at the end in the copy).
   Uses malloc to get the space.  Returns the address of the copy.  */

char *
savestring (ptr, size)
     const char *ptr;
     int size;
{
  register char *p = (char *) xmalloc (size + 1);
  memcpy (p, ptr, size);
  p[size] = 0;
  return p;
}

char *
msavestring (md, ptr, size)
     PTR md;
     const char *ptr;
     int size;
{
  register char *p = (char *) xmmalloc (md, size + 1);
  memcpy (p, ptr, size);
  p[size] = 0;
  return p;
}

/* The "const" is so it compiles under DGUX (which prototypes strsave
   in <string.h>.  FIXME: This should be named "xstrsave", shouldn't it?
   Doesn't real strsave return NULL if out of memory?  */
char *
strsave (ptr)
     const char *ptr;
{
  return savestring (ptr, strlen (ptr));
}

char *
mstrsave (md, ptr)
     PTR md;
     const char *ptr;
{
  return (msavestring (md, ptr, strlen (ptr)));
}

void
print_spaces (n, file)
     register int n;
     register GDB_FILE *file;
{
  if (file->ts_streamtype == astring)
    {
      char *p;

      gdb_file_adjust_strbuf (n, file);
      p = file->ts_strbuf + strlen (file->ts_strbuf);

      memset (p, ' ', n);
      p[n] = '\000';
    }
  else
    {
      while (n-- > 0)
	fputc (' ', file->ts_filestream);
    }
}

/* Print a host address.  */

void
gdb_print_address (addr, stream)
     PTR addr;
     GDB_FILE *stream;
{

  /* We could use the %p conversion specifier to fprintf if we had any
     way of knowing whether this host supports it.  But the following
     should work on the Alpha and on 32 bit machines.  */

  fprintf_filtered (stream, "0x%lx", (unsigned long)addr);
}

/* Ask user a y-or-n question and return 1 iff answer is yes.
   Takes three args which are given to printf to print the question.
   The first, a control string, should end in "? ".
   It should not say how to answer, because we do that.  */

/* VARARGS */
int
#ifdef ANSI_PROTOTYPES
query (char *ctlstr, ...)
#else
query (va_alist)
     va_dcl
#endif
{
  va_list args;
  register int answer;
  register int ans2;
  int retval;

#ifdef ANSI_PROTOTYPES
  va_start (args, ctlstr);
#else
  char *ctlstr;
  va_start (args);
  ctlstr = va_arg (args, char *);
#endif

  if (query_hook)
    {
      return query_hook (ctlstr, args);
    }

  /* Automatically answer "yes" if input is not from a terminal.  */
  if (!input_from_terminal_p ())
    return 1;
#ifdef MPW
  /* FIXME Automatically answer "yes" if called from MacGDB.  */
  if (mac_app)
    return 1;
#endif /* MPW */

  while (1)
    {
      wrap_here ("");		/* Flush any buffered output */
      gdb_flush (gdb_stdout);

      if (annotation_level > 1)
	printf_filtered ("\n\032\032pre-query\n");

      vfprintf_filtered (gdb_stdout, ctlstr, args);
      printf_filtered ("(y or n) ");

      if (annotation_level > 1)
	printf_filtered ("\n\032\032query\n");

#ifdef MPW
      /* If not in MacGDB, move to a new line so the entered line doesn't
	 have a prompt on the front of it. */
      if (!mac_app)
	fputs_unfiltered ("\n", gdb_stdout);
#endif /* MPW */

      wrap_here("");
      gdb_flush (gdb_stdout);

#if defined(TUI)
      if (!tui_version || cmdWin == tuiWinWithFocus())
#endif
	answer = fgetc (stdin);
#if defined(TUI)
      else

        answer = (unsigned char)tuiBufferGetc();

#endif
      clearerr (stdin);		/* in case of C-d */
      if (answer == EOF)	/* C-d */
        {
	  retval = 1;
	  break;
	}
      /* Eat rest of input line, to EOF or newline */
      if ((answer != '\n') || (tui_version && answer != '\r'))
	do 
	  {
#if defined(TUI)
	    if (!tui_version || cmdWin == tuiWinWithFocus())
#endif
	      ans2 = fgetc (stdin);
#if defined(TUI)
	    else

              ans2 = (unsigned char)tuiBufferGetc(); 
#endif
	    clearerr (stdin);
	  }
        while (ans2 != EOF && ans2 != '\n' && ans2 != '\r');
      TUIDO(((TuiOpaqueFuncPtr)tui_vStartNewLines, 1));

      if (answer >= 'a')
	answer -= 040;
      if (answer == 'Y')
	{
	  retval = 1;
	  break;
	}
      if (answer == 'N')
	{
	  retval = 0;
	  break;
	}
      printf_filtered ("Please answer y or n.\n");
    }

  if (annotation_level > 1)
    printf_filtered ("\n\032\032post-query\n");
  return retval;
}


/* Parse a C escape sequence.  STRING_PTR points to a variable
   containing a pointer to the string to parse.  That pointer
   should point to the character after the \.  That pointer
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
      return 007;		/* Bell (alert) char */
    case 'b':
      return '\b';
    case 'e':			/* Escape character */
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

/* Print the character C on STREAM as part of the contents of a literal
   string whose delimiter is QUOTER.  Note that this routine should only
   be call for printing things which are independent of the language
   of the program being debugged. */

void
gdb_printchar (c, stream, quoter)
     register int c;
     GDB_FILE *stream;
     int quoter;
{

  c &= 0xFF;			/* Avoid sign bit follies */

  if (              c < 0x20  ||		/* Low control chars */	
      (c >= 0x7F && c < 0xA0) ||		/* DEL, High controls */
      (sevenbit_strings && c >= 0x80)) {	/* high order bit set */
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
  } else {
    if (c == '\\' || c == quoter)
      fputs_filtered ("\\", stream);
    fprintf_filtered (stream, "%c", c);
  }
}




static char * hexlate = "0123456789abcdef" ;
int fmthex(inbuf,outbuff,length,linelength)
     unsigned char * inbuf ;
     unsigned char * outbuff;
     int length;
     int linelength;
{
  unsigned char byte , nib ;
  int outlength = 0 ;

  while (length)
    {
      if (outlength >= linelength) break ;
      byte = *inbuf ;
      inbuf++ ;
      nib = byte >> 4 ;
      *outbuff++ = hexlate[nib] ;
      nib = byte &0x0f ;
      *outbuff++ = hexlate[nib] ;
      *outbuff++ = ' ' ;
      length-- ;
      outlength += 3 ;
    }
  *outbuff = '\0' ; /* null terminate our output line */
  return outlength ;
}


/* Number of lines per page or UINT_MAX if paging is disabled.  */
static unsigned int lines_per_page;
/* Number of chars per line or UNIT_MAX is line folding is disabled.  */
static unsigned int chars_per_line;
/* Current count of lines printed on this page, chars on this line.  */
static unsigned int lines_printed, chars_printed;

/* Buffer and start column of buffered text, for doing smarter word-
   wrapping.  When someone calls wrap_here(), we start buffering output
   that comes through fputs_filtered().  If we see a newline, we just
   spit it out and forget about the wrap_here().  If we see another
   wrap_here(), we spit it out and remember the newer one.  If we see
   the end of the line, we spit out a newline, the indent, and then
   the buffered output.  */

/* Malloc'd buffer with chars_per_line+2 bytes.  Contains characters which
   are waiting to be output (they have already been counted in chars_printed).
   When wrap_buffer[0] is null, the buffer is empty.  */
static char *wrap_buffer;

/* Pointer in wrap_buffer to the next character to fill.  */
static char *wrap_pointer;

/* String to indent by if the wrap occurs.  Must not be NULL if wrap_column
   is non-zero.  */
static char *wrap_indent;

/* Column number on the screen where wrap_buffer begins, or 0 if wrapping
   is not in effect.  */
static int wrap_column;


/* Inialize the lines and chars per page */
void
init_page_info()
{
#if defined(TUI)
  if (tui_version && m_winPtrNotNull(cmdWin))
    {
      lines_per_page = cmdWin->generic.height;
      chars_per_line = cmdWin->generic.width;
    }
  else
#endif
    {
      /* These defaults will be used if we are unable to get the correct
         values from termcap.  */
#if defined(__GO32__)
      lines_per_page = ScreenRows();
      chars_per_line = ScreenCols();
#else  
      lines_per_page = 24;
      chars_per_line = 80;

#if !defined (MPW) && !defined (_WIN32)
      /* No termcap under MPW, although might be cool to do something
         by looking at worksheet or console window sizes. */
      /* Initialize the screen height and width from termcap.  */
      {
        char *termtype = getenv ("TERM");

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
		int running_in_emacs = getenv ("EMACS") != NULL;
	    
	        val = tgetnum ("li");
	        if (val >= 0 && !running_in_emacs)
	          lines_per_page = val;
	        else
	          /* The number of lines per page is not mentioned
		     in the terminal description.  This probably means
		     that paging is not useful (e.g. emacs shell window),
		     so disable paging.  */
	          lines_per_page = UINT_MAX;
	    
	        val = tgetnum ("co");
	        if (val >= 0)
	          chars_per_line = val;
	      }
          }
      }
#endif /* MPW */

#if defined(SIGWINCH) && defined(SIGWINCH_HANDLER)

      /* If there is a better way to determine the window size, use it. */
      SIGWINCH_HANDLER (SIGWINCH);
#endif
#endif
      /* If the output is not a terminal, don't paginate it.  */
      if (!GDB_FILE_ISATTY (gdb_stdout))
        lines_per_page = UINT_MAX;
  } /* the command_line_version */
  set_width();
}

static void
set_width()
{
  if (chars_per_line == 0)
    init_page_info();

  if (!wrap_buffer)
    {
      wrap_buffer = (char *) xmalloc (chars_per_line + 2);
      wrap_buffer[0] = '\0';
    }
  else
    wrap_buffer = (char *) xrealloc (wrap_buffer, chars_per_line + 2);
  wrap_pointer = wrap_buffer;   /* Start it at the beginning */
}

/* ARGSUSED */
static void 
set_width_command (args, from_tty, c)
     char *args;
     int from_tty;
     struct cmd_list_element *c;
{
  set_width ();
}

/* Wait, so the user can read what's on the screen.  Prompt the user
   to continue by pressing RETURN.  */

static void
prompt_for_continue ()
{
  char *ignore;
  char cont_prompt[120];

  if (annotation_level > 1)
    printf_unfiltered ("\n\032\032pre-prompt-for-continue\n");

  strcpy (cont_prompt,
	  "---Type <return> to continue, or q <return> to quit---");
  if (annotation_level > 1)
    strcat (cont_prompt, "\n\032\032prompt-for-continue\n");

  /* We must do this *before* we call gdb_readline, else it will eventually
     call us -- thinking that we're trying to print beyond the end of the 
     screen.  */
  reinitialize_more_filter ();

  immediate_quit++;
  /* On a real operating system, the user can quit with SIGINT.
     But not on GO32.

     'q' is provided on all systems so users don't have to change habits
     from system to system, and because telling them what to do in
     the prompt is more user-friendly than expecting them to think of
     SIGINT.  */
  /* Call readline, not gdb_readline, because GO32 readline handles control-C
     whereas control-C to gdb_readline will cause the user to get dumped
     out to DOS.  */
  ignore = readline (cont_prompt);

  if (annotation_level > 1)
    printf_unfiltered ("\n\032\032post-prompt-for-continue\n");

  if (ignore)
    {
      char *p = ignore;
      while (*p == ' ' || *p == '\t')
	++p;
      if (p[0] == 'q')
	request_quit (SIGINT);
      free (ignore);
    }
  immediate_quit--;

  /* Now we have to do this again, so that GDB will know that it doesn't
     need to save the ---Type <return>--- line at the top of the screen.  */
  reinitialize_more_filter ();

  dont_repeat ();		/* Forget prev cmd -- CR won't repeat it. */
}

/* Reinitialize filter; ie. tell it to reset to original values.  */

void
reinitialize_more_filter ()
{
  lines_printed = 0;
  chars_printed = 0;
}

/* Indicate that if the next sequence of characters overflows the line,
   a newline should be inserted here rather than when it hits the end. 
   If INDENT is non-null, it is a string to be printed to indent the
   wrapped part on the next line.  INDENT must remain accessible until
   the next call to wrap_here() or until a newline is printed through
   fputs_filtered().

   If the line is already overfull, we immediately print a newline and
   the indentation, and disable further wrapping.

   If we don't know the width of lines, but we know the page height,
   we must not wrap words, but should still keep track of newlines
   that were explicitly printed.

   INDENT should not contain tabs, as that will mess up the char count
   on the next line.  FIXME.

   This routine is guaranteed to force out any output which has been
   squirreled away in the wrap_buffer, so wrap_here ((char *)0) can be
   used to force out output from the wrap_buffer.  */

void
wrap_here(indent)
     char *indent;
{
  /* This should have been allocated, but be paranoid anyway. */
  if (!wrap_buffer)
    abort ();

  if (wrap_buffer[0])
    {
      *wrap_pointer = '\0';
      fputs_unfiltered (wrap_buffer, gdb_stdout);
    }
  wrap_pointer = wrap_buffer;
  wrap_buffer[0] = '\0';
  if (chars_per_line == UINT_MAX)		/* No line overflow checking */
    {
      wrap_column = 0;
    }
  else if (chars_printed >= chars_per_line)
    {
      puts_filtered ("\n");
      if (indent != NULL)
	puts_filtered (indent);
      wrap_column = 0;
    }
  else
    {
      wrap_column = chars_printed;
      if (indent == NULL)
	wrap_indent = "";
      else
	wrap_indent = indent;
    }
}

/* Ensure that whatever gets printed next, using the filtered output
   commands, starts at the beginning of the line.  I.E. if there is
   any pending output for the current line, flush it and start a new
   line.  Otherwise do nothing. */

void
begin_line ()
{
  if (chars_printed > 0)
    {
      puts_filtered ("\n");
    }
}

int 
gdb_file_isatty (stream)
    GDB_FILE *stream;
{

  if (stream->ts_streamtype == afile)
     return (isatty(fileno(stream->ts_filestream)));
  else return 0;
}

GDB_FILE *
gdb_file_init_astring (n)
    int n;
{
  GDB_FILE *tmpstream;

  tmpstream = xmalloc (sizeof(GDB_FILE));
  tmpstream->ts_streamtype = astring;
  tmpstream->ts_filestream = NULL;
  if (n > 0)
    {
      tmpstream->ts_strbuf = xmalloc ((n + 1)*sizeof(char));
      tmpstream->ts_strbuf[0] = '\0';
    }
  else
     tmpstream->ts_strbuf = NULL;
  tmpstream->ts_buflen = n;

  return tmpstream;
}

void
gdb_file_deallocate (streamptr)
    GDB_FILE **streamptr;
{
  GDB_FILE *tmpstream;

  tmpstream = *streamptr;
  if ((tmpstream->ts_streamtype == astring) &&
      (tmpstream->ts_strbuf != NULL)) 
    {
      free (tmpstream->ts_strbuf);
    }

  free (tmpstream);
  *streamptr = NULL;
}
 
char *
gdb_file_get_strbuf (stream)
     GDB_FILE *stream;
{
  return (stream->ts_strbuf);
}

/* adjust the length of the buffer by the amount necessary
   to accomodate appending a string of length N to the buffer contents */
void
gdb_file_adjust_strbuf (n, stream)
     int n;
     GDB_FILE *stream;
{
  int non_null_chars;
  
  non_null_chars = strlen(stream->ts_strbuf);
 
  if (n > (stream->ts_buflen - non_null_chars - 1)) 
    {
      stream->ts_buflen = n + non_null_chars + 1;
      stream->ts_strbuf = xrealloc (stream->ts_strbuf, stream->ts_buflen);
    }  
} 

GDB_FILE *
gdb_fopen (name, mode)
     char * name;
     char * mode;
{
  int       gdb_file_size;
  GDB_FILE *tmp;

  gdb_file_size = sizeof(GDB_FILE);
  tmp = (GDB_FILE *) xmalloc (gdb_file_size);
  tmp->ts_streamtype = afile;
  tmp->ts_filestream = fopen (name, mode);
  tmp->ts_strbuf = NULL;
  tmp->ts_buflen = 0;
  
  return tmp;
}

void
gdb_flush (stream)
     GDB_FILE *stream;
{
  if (flush_hook
      && (stream == gdb_stdout
	  || stream == gdb_stderr))
    {
      flush_hook (stream);
      return;
    }

  fflush (stream->ts_filestream);
}

void
gdb_fclose(streamptr)
     GDB_FILE **streamptr;
{
  GDB_FILE *tmpstream;

  tmpstream = *streamptr;
  fclose (tmpstream->ts_filestream);
  gdb_file_deallocate (streamptr);
}

/* Like fputs but if FILTER is true, pause after every screenful.

   Regardless of FILTER can wrap at points other than the final
   character of a line.

   Unlike fputs, fputs_maybe_filtered does not return a value.
   It is OK for LINEBUFFER to be NULL, in which case just don't print
   anything.

   Note that a longjmp to top level may occur in this routine (only if
   FILTER is true) (since prompt_for_continue may do so) so this
   routine should not be called when cleanups are not in place.  */

static void
fputs_maybe_filtered (linebuffer, stream, filter)
     const char *linebuffer;
     GDB_FILE *stream;
     int filter;
{
  const char *lineptr;

  if (linebuffer == 0)
    return;

  /* Don't do any filtering if it is disabled.  */
  if (stream != gdb_stdout
   || (lines_per_page == UINT_MAX && chars_per_line == UINT_MAX))
    {
      fputs_unfiltered (linebuffer, stream);
      return;
    }

  /* Go through and output each character.  Show line extension
     when this is necessary; prompt user for new page when this is
     necessary.  */
  
  lineptr = linebuffer;
  while (*lineptr)
    {
      /* Possible new page.  */
      if (filter &&
	  (lines_printed >= lines_per_page - 1))
	prompt_for_continue ();

      while (*lineptr && *lineptr != '\n')
	{
	  /* Print a single line.  */
	  if (*lineptr == '\t')
	    {
	      if (wrap_column)
		*wrap_pointer++ = '\t';
	      else
		fputc_unfiltered ('\t', stream);
	      /* Shifting right by 3 produces the number of tab stops
	         we have already passed, and then adding one and
		 shifting left 3 advances to the next tab stop.  */
	      chars_printed = ((chars_printed >> 3) + 1) << 3;
	      lineptr++;
	    }
	  else
	    {
	      if (wrap_column)
		*wrap_pointer++ = *lineptr;
	      else
	        fputc_unfiltered (*lineptr, stream);
	      chars_printed++;
	      lineptr++;
	    }
      
	  if (chars_printed >= chars_per_line)
	    {
	      unsigned int save_chars = chars_printed;

	      chars_printed = 0;
	      lines_printed++;
	      /* If we aren't actually wrapping, don't output newline --
		 if chars_per_line is right, we probably just overflowed
		 anyway; if it's wrong, let us keep going.  */
	      if (wrap_column)
		fputc_unfiltered ('\n', stream);

	      /* Possible new page.  */
	      if (lines_printed >= lines_per_page - 1)
		prompt_for_continue ();

	      /* Now output indentation and wrapped string */
	      if (wrap_column)
		{
		  fputs_unfiltered (wrap_indent, stream);
		  *wrap_pointer = '\0';	/* Null-terminate saved stuff */
		  fputs_unfiltered (wrap_buffer, stream); /* and eject it */
		  /* FIXME, this strlen is what prevents wrap_indent from
		     containing tabs.  However, if we recurse to print it
		     and count its chars, we risk trouble if wrap_indent is
		     longer than (the user settable) chars_per_line. 
		     Note also that this can set chars_printed > chars_per_line
		     if we are printing a long string.  */
		  chars_printed = strlen (wrap_indent)
				+ (save_chars - wrap_column);
		  wrap_pointer = wrap_buffer;	/* Reset buffer */
		  wrap_buffer[0] = '\0';
		  wrap_column = 0;		/* And disable fancy wrap */
 		}
	    }
	}

      if (*lineptr == '\n')
	{
	  chars_printed = 0;
	  wrap_here ((char *)0);  /* Spit out chars, cancel further wraps */
	  lines_printed++;
	  fputc_unfiltered ('\n', stream);
	  lineptr++;
	}
    }
}

void
fputs_filtered (linebuffer, stream)
     const char *linebuffer;
     GDB_FILE *stream;
{
  fputs_maybe_filtered (linebuffer, stream, 1);
}

int
putchar_unfiltered (c)
     int c;
{
  char buf[2];

  buf[0] = c;
  buf[1] = 0;
  fputs_unfiltered (buf, gdb_stdout);
  return c;
}

int
fputc_unfiltered (c, stream)
     int c;
     GDB_FILE * stream;
{
  char buf[2];

  buf[0] = c;
  buf[1] = 0;
  fputs_unfiltered (buf, stream);
  return c;
}

int
fputc_filtered (c, stream)
     int c;
     GDB_FILE * stream;
{
  char buf[2];

  buf[0] = c;
  buf[1] = 0;
  fputs_filtered (buf, stream);
  return c;
}

/* puts_debug is like fputs_unfiltered, except it prints special
   characters in printable fashion.  */

void
puts_debug (prefix, string, suffix)
     char *prefix;
     char *string;
     char *suffix;
{
  int ch;

  /* Print prefix and suffix after each line.  */
  static int new_line = 1;
  static int return_p = 0;
  static char *prev_prefix = "";
  static char *prev_suffix = "";

  if (*string == '\n')
    return_p = 0;

  /* If the prefix is changing, print the previous suffix, a new line,
     and the new prefix.  */
  if ((return_p || (strcmp(prev_prefix, prefix) != 0)) && !new_line)
    {
      fputs_unfiltered (prev_suffix, gdb_stderr);
      fputs_unfiltered ("\n", gdb_stderr);
      fputs_unfiltered (prefix, gdb_stderr);
    }

  /* Print prefix if we printed a newline during the previous call.  */
  if (new_line)
    {
      new_line = 0;
      fputs_unfiltered (prefix, gdb_stderr);
    }

  prev_prefix = prefix;
  prev_suffix = suffix;

  /* Output characters in a printable format.  */
  while ((ch = *string++) != '\0')
    {
      switch (ch)
        {
	default:
	  if (isprint (ch))
	    fputc_unfiltered (ch, gdb_stderr);

	  else
	    fprintf_unfiltered (gdb_stderr, "\\x%02x", ch & 0xff);
	  break;

	case '\\': fputs_unfiltered ("\\\\",  gdb_stderr);	break;
	case '\b': fputs_unfiltered ("\\b",   gdb_stderr);	break;
	case '\f': fputs_unfiltered ("\\f",   gdb_stderr);	break;
	case '\n': new_line = 1;
		   fputs_unfiltered ("\\n",   gdb_stderr);	break;
	case '\r': fputs_unfiltered ("\\r",   gdb_stderr);	break;
	case '\t': fputs_unfiltered ("\\t",   gdb_stderr);	break;
	case '\v': fputs_unfiltered ("\\v",   gdb_stderr);	break;
        }

      return_p = ch == '\r';
    }

  /* Print suffix if we printed a newline.  */
  if (new_line)
    {
      fputs_unfiltered (suffix, gdb_stderr);
      fputs_unfiltered ("\n", gdb_stderr);
    }
}


/* Print a variable number of ARGS using format FORMAT.  If this
   information is going to put the amount written (since the last call
   to REINITIALIZE_MORE_FILTER or the last page break) over the page size,
   call prompt_for_continue to get the users permision to continue.

   Unlike fprintf, this function does not return a value.

   We implement three variants, vfprintf (takes a vararg list and stream),
   fprintf (takes a stream to write on), and printf (the usual).

   Note also that a longjmp to top level may occur in this routine
   (since prompt_for_continue may do so) so this routine should not be
   called when cleanups are not in place.  */

static void
vfprintf_maybe_filtered (stream, format, args, filter)
     GDB_FILE *stream;
     const char *format;
     va_list args;
     int filter;
{
  char *linebuffer;
  struct cleanup *old_cleanups;

  vasprintf (&linebuffer, format, args);
  if (linebuffer == NULL)
    {
      fputs_unfiltered ("\ngdb: virtual memory exhausted.\n", gdb_stderr);
      exit (1);
    }
  old_cleanups = make_cleanup (free, linebuffer);
  fputs_maybe_filtered (linebuffer, stream, filter);
  do_cleanups (old_cleanups);
}


void
vfprintf_filtered (stream, format, args)
     GDB_FILE *stream;
     const char *format;
     va_list args;
{
  vfprintf_maybe_filtered (stream, format, args, 1);
}

void
vfprintf_unfiltered (stream, format, args)
     GDB_FILE *stream;
     const char *format;
     va_list args;
{
  char *linebuffer;
  struct cleanup *old_cleanups;

  vasprintf (&linebuffer, format, args);
  if (linebuffer == NULL)
    {
      fputs_unfiltered ("\ngdb: virtual memory exhausted.\n", gdb_stderr);
      exit (1);
    }
  old_cleanups = make_cleanup (free, linebuffer);
  fputs_unfiltered (linebuffer, stream);
  do_cleanups (old_cleanups);
}

void
vprintf_filtered (format, args)
     const char *format;
     va_list args;
{
  vfprintf_maybe_filtered (gdb_stdout, format, args, 1);
}

void
vprintf_unfiltered (format, args)
     const char *format;
     va_list args;
{
  vfprintf_unfiltered (gdb_stdout, format, args);
}

/* VARARGS */
void
#ifdef ANSI_PROTOTYPES
fprintf_filtered (GDB_FILE *stream, const char *format, ...)
#else
fprintf_filtered (va_alist)
     va_dcl
#endif
{
  va_list args;
#ifdef ANSI_PROTOTYPES
  va_start (args, format);
#else
  GDB_FILE *stream;
  char *format;

  va_start (args);
  stream = va_arg (args, GDB_FILE *);
  format = va_arg (args, char *);
#endif
  vfprintf_filtered (stream, format, args);
  va_end (args);
}

/* VARARGS */
void
#ifdef ANSI_PROTOTYPES
fprintf_unfiltered (GDB_FILE *stream, const char *format, ...)
#else
fprintf_unfiltered (va_alist)
     va_dcl
#endif
{
  va_list args;
#ifdef ANSI_PROTOTYPES
  va_start (args, format);
#else
  GDB_FILE *stream;
  char *format;

  va_start (args);
  stream = va_arg (args, GDB_FILE *);
  format = va_arg (args, char *);
#endif
  vfprintf_unfiltered (stream, format, args);
  va_end (args);
}

/* Like fprintf_filtered, but prints its result indented.
   Called as fprintfi_filtered (spaces, stream, format, ...);  */

/* VARARGS */
void
#ifdef ANSI_PROTOTYPES
fprintfi_filtered (int spaces, GDB_FILE *stream, const char *format, ...)
#else
fprintfi_filtered (va_alist)
     va_dcl
#endif
{
  va_list args;
#ifdef ANSI_PROTOTYPES
  va_start (args, format);
#else
  int spaces;
  GDB_FILE *stream;
  char *format;

  va_start (args);
  spaces = va_arg (args, int);
  stream = va_arg (args, GDB_FILE *);
  format = va_arg (args, char *);
#endif
  print_spaces_filtered (spaces, stream);

  vfprintf_filtered (stream, format, args);
  va_end (args);
}


/* VARARGS */
void
#ifdef ANSI_PROTOTYPES
printf_filtered (const char *format, ...)
#else
printf_filtered (va_alist)
     va_dcl
#endif
{
  va_list args;
#ifdef ANSI_PROTOTYPES
  va_start (args, format);
#else
  char *format;

  va_start (args);
  format = va_arg (args, char *);
#endif
  vfprintf_filtered (gdb_stdout, format, args);
  va_end (args);
}


/* VARARGS */
void
#ifdef ANSI_PROTOTYPES
printf_unfiltered (const char *format, ...)
#else
printf_unfiltered (va_alist)
     va_dcl
#endif
{
  va_list args;
#ifdef ANSI_PROTOTYPES
  va_start (args, format);
#else
  char *format;

  va_start (args);
  format = va_arg (args, char *);
#endif
  vfprintf_unfiltered (gdb_stdout, format, args);
  va_end (args);
}

/* Like printf_filtered, but prints it's result indented.
   Called as printfi_filtered (spaces, format, ...);  */

/* VARARGS */
void
#ifdef ANSI_PROTOTYPES
printfi_filtered (int spaces, const char *format, ...)
#else
printfi_filtered (va_alist)
     va_dcl
#endif
{
  va_list args;
#ifdef ANSI_PROTOTYPES
  va_start (args, format);
#else
  int spaces;
  char *format;

  va_start (args);
  spaces = va_arg (args, int);
  format = va_arg (args, char *);
#endif
  print_spaces_filtered (spaces, gdb_stdout);
  vfprintf_filtered (gdb_stdout, format, args);
  va_end (args);
}

/* Easy -- but watch out!

   This routine is *not* a replacement for puts()!  puts() appends a newline.
   This one doesn't, and had better not!  */

void
puts_filtered (string)
     const char *string;
{
  fputs_filtered (string, gdb_stdout);
}

void
puts_unfiltered (string)
     const char *string;
{
  fputs_unfiltered (string, gdb_stdout);
}

/* Return a pointer to N spaces and a null.  The pointer is good
   until the next call to here.  */
char *
n_spaces (n)
     int n;
{
  register char *t;
  static char *spaces;
  static int max_spaces;

  if (n > max_spaces)
    {
      if (spaces)
	free (spaces);
      spaces = (char *) xmalloc (n+1);
      for (t = spaces+n; t != spaces;)
	*--t = ' ';
      spaces[n] = '\0';
      max_spaces = n;
    }

  return spaces + max_spaces - n;
}

/* Print N spaces.  */
void
print_spaces_filtered (n, stream)
     int n;
     GDB_FILE *stream;
{
  fputs_filtered (n_spaces (n), stream);
}

/* C++ demangler stuff.  */

/* fprintf_symbol_filtered attempts to demangle NAME, a symbol in language
   LANG, using demangling args ARG_MODE, and print it filtered to STREAM.
   If the name is not mangled, or the language for the name is unknown, or
   demangling is off, the name is printed in its "raw" form. */

void
fprintf_symbol_filtered (stream, name, lang, arg_mode)
     GDB_FILE *stream;
     char *name;
     enum language lang;
     int arg_mode;
{
  char *demangled;

  if (name != NULL)
    {
      /* If user wants to see raw output, no problem.  */
      if (!demangle)
	{
	  fputs_filtered (name, stream);
	}
      else
	{
	  switch (lang)
	    {
	    case language_cplus:
	      demangled = cplus_demangle (name, arg_mode);
	      break;
	    case language_java:
	      demangled = cplus_demangle (name, arg_mode | DMGL_JAVA);
	      break;
	    case language_chill:
	      demangled = chill_demangle (name);
	      break;
	    default:
	      demangled = NULL;
	      break;
	    }
	  fputs_filtered (demangled ? demangled : name, stream);
	  if (demangled != NULL)
	    {
	      free (demangled);
	    }
	}
    }
}

/* Do a strcmp() type operation on STRING1 and STRING2, ignoring any
   differences in whitespace.  Returns 0 if they match, non-zero if they
   don't (slightly different than strcmp()'s range of return values).
   
   As an extra hack, string1=="FOO(ARGS)" matches string2=="FOO".
   This "feature" is useful when searching for matching C++ function names
   (such as if the user types 'break FOO', where FOO is a mangled C++
   function). */

int
strcmp_iw (string1, string2)
     const char *string1;
     const char *string2;
{
  while ((*string1 != '\0') && (*string2 != '\0'))
    {
      while (isspace (*string1))
	{
	  string1++;
	}
      while (isspace (*string2))
	{
	  string2++;
	}
      if (*string1 != *string2)
	{
	  break;
	}
      if (*string1 != '\0')
	{
	  string1++;
	  string2++;
	}
    }
  return (*string1 != '\0' && *string1 != '(') || (*string2 != '\0');
}


/*
** subsetCompare()
**    Answer whether stringToCompare is a full or partial match to
**    templateString.  The partial match must be in sequence starting
**    at index 0.
*/
int
#ifdef _STDC__
subsetCompare(
    char *stringToCompare,
    char *templateString)
#else
subsetCompare(stringToCompare, templateString)
    char *stringToCompare;
    char *templateString;
#endif
{
    int    match = 0;

    if (templateString != (char *)NULL && stringToCompare != (char *)NULL &&
	strlen(stringToCompare) <= strlen(templateString))
      match = (strncmp(templateString,
		       stringToCompare,
		       strlen(stringToCompare)) == 0);

    return match;
} /* subsetCompare */


void pagination_on_command(arg, from_tty)
  char *arg;
  int from_tty;
{
  pagination_enabled = 1;
}

void pagination_off_command(arg, from_tty)
  char *arg;
  int from_tty;
{
  pagination_enabled = 0;
}


void
initialize_utils ()
{
  struct cmd_list_element *c;

  c = add_set_cmd ("width", class_support, var_uinteger, 
		  (char *)&chars_per_line,
		  "Set number of characters gdb thinks are in a line.",
		  &setlist);
  add_show_from_set (c, &showlist);
  c->function.sfunc = set_width_command;

  add_show_from_set
    (add_set_cmd ("height", class_support,
		  var_uinteger, (char *)&lines_per_page,
		  "Set number of lines gdb thinks are in a page.", &setlist),
     &showlist);
  
  init_page_info ();

  /* If the output is not a terminal, don't paginate it.  */
  if (!GDB_FILE_ISATTY (gdb_stdout))
    lines_per_page = UINT_MAX;

  set_width_command ((char *)NULL, 0, c);

  add_show_from_set
    (add_set_cmd ("demangle", class_support, var_boolean, 
		  (char *)&demangle,
		"Set demangling of encoded C++ names when displaying symbols.",
		  &setprintlist),
     &showprintlist);

  add_show_from_set
    (add_set_cmd ("pagination", class_support,
		  var_boolean, (char *)&pagination_enabled,
		  "Set state of pagination.", &setlist),
     &showlist);
  if (xdb_commands)
    {
      add_com("am", class_support, pagination_on_command, 
              "Enable pagination");
      add_com("sm", class_support, pagination_off_command, 
              "Disable pagination");
    }

  add_show_from_set
    (add_set_cmd ("sevenbit-strings", class_support, var_boolean, 
		  (char *)&sevenbit_strings,
   "Set printing of 8-bit characters in strings as \\nnn.",
		  &setprintlist),
     &showprintlist);

  add_show_from_set
    (add_set_cmd ("asm-demangle", class_support, var_boolean, 
		  (char *)&asm_demangle,
	"Set demangling of C++ names in disassembly listings.",
		  &setprintlist),
     &showprintlist);
}

/* Machine specific function to handle SIGWINCH signal. */

#ifdef  SIGWINCH_HANDLER_BODY
        SIGWINCH_HANDLER_BODY
#endif

/* Support for converting target fp numbers into host DOUBLEST format.  */

/* XXX - This code should really be in libiberty/floatformat.c, however
   configuration issues with libiberty made this very difficult to do in the
   available time.  */

#include "floatformat.h"
#include <math.h>		/* ldexp */

/* The odds that CHAR_BIT will be anything but 8 are low enough that I'm not
   going to bother with trying to muck around with whether it is defined in
   a system header, what we do if not, etc.  */
#define FLOATFORMAT_CHAR_BIT 8

static unsigned long get_field PARAMS ((unsigned char *,
					enum floatformat_byteorders,
					unsigned int,
					unsigned int,
					unsigned int));

/* Extract a field which starts at START and is LEN bytes long.  DATA and
   TOTAL_LEN are the thing we are extracting it from, in byteorder ORDER.  */
static unsigned long
get_field (data, order, total_len, start, len)
     unsigned char *data;
     enum floatformat_byteorders order;
     unsigned int total_len;
     unsigned int start;
     unsigned int len;
{
  unsigned long result;
  unsigned int cur_byte;
  int cur_bitshift;

  /* Start at the least significant part of the field.  */
  cur_byte = (start + len) / FLOATFORMAT_CHAR_BIT;
  if (order == floatformat_little || order == floatformat_littlebyte_bigword)
    cur_byte = (total_len / FLOATFORMAT_CHAR_BIT) - cur_byte - 1;
  cur_bitshift =
    ((start + len) % FLOATFORMAT_CHAR_BIT) - FLOATFORMAT_CHAR_BIT;
  result = *(data + cur_byte) >> (-cur_bitshift);
  cur_bitshift += FLOATFORMAT_CHAR_BIT;
  if (order == floatformat_little || order == floatformat_littlebyte_bigword)
    ++cur_byte;
  else
    --cur_byte;

  /* Move towards the most significant part of the field.  */
  while (cur_bitshift < len)
    {
      if (len - cur_bitshift < FLOATFORMAT_CHAR_BIT)
	/* This is the last byte; zero out the bits which are not part of
	   this field.  */
	result |=
	  (*(data + cur_byte) & ((1 << (len - cur_bitshift)) - 1))
	    << cur_bitshift;
      else
	result |= *(data + cur_byte) << cur_bitshift;
      cur_bitshift += FLOATFORMAT_CHAR_BIT;
      if (order == floatformat_little || order == floatformat_littlebyte_bigword)
	++cur_byte;
      else
	--cur_byte;
    }
  return result;
}
  
/* Convert from FMT to a DOUBLEST.
   FROM is the address of the extended float.
   Store the DOUBLEST in *TO.  */

void
floatformat_to_doublest (fmt, from, to)
     const struct floatformat *fmt;
     char *from;
     DOUBLEST *to;
{
  unsigned char *ufrom = (unsigned char *)from;
  DOUBLEST dto;
  long exponent;
  unsigned long mant;
  unsigned int mant_bits, mant_off;
  int mant_bits_left;
  int special_exponent;		/* It's a NaN, denorm or zero */

  /* If the mantissa bits are not contiguous from one end of the
     mantissa to the other, we need to make a private copy of the
     source bytes that is in the right order since the unpacking
     algorithm assumes that the bits are contiguous.

     Swap the bytes individually rather than accessing them through
     "long *" since we have no guarantee that they start on a long
     alignment, and also sizeof(long) for the host could be different
     than sizeof(long) for the target.  FIXME: Assumes sizeof(long)
     for the target is 4. */

  if (fmt -> byteorder == floatformat_littlebyte_bigword)
    {
      static unsigned char *newfrom;
      unsigned char *swapin, *swapout;
      int longswaps;

      longswaps = fmt -> totalsize / FLOATFORMAT_CHAR_BIT;
      longswaps >>= 3;
      
      if (newfrom == NULL)
	{
	  newfrom = (unsigned char *) xmalloc (fmt -> totalsize);
	}
      swapout = newfrom;
      swapin = ufrom;
      ufrom = newfrom;
      while (longswaps-- > 0)
	{
	  /* This is ugly, but efficient */
	  *swapout++ = swapin[4];
	  *swapout++ = swapin[5];
	  *swapout++ = swapin[6];
	  *swapout++ = swapin[7];
	  *swapout++ = swapin[0];
	  *swapout++ = swapin[1];
	  *swapout++ = swapin[2];
	  *swapout++ = swapin[3];
	  swapin += 8;
	}
    }

  exponent = get_field (ufrom, fmt->byteorder, fmt->totalsize,
			fmt->exp_start, fmt->exp_len);
  /* Note that if exponent indicates a NaN, we can't really do anything useful
     (not knowing if the host has NaN's, or how to build one).  So it will
     end up as an infinity or something close; that is OK.  */

  mant_bits_left = fmt->man_len;
  mant_off = fmt->man_start;
  dto = 0.0;

  special_exponent = exponent == 0 || exponent == fmt->exp_nan;

/* Don't bias zero's, denorms or NaNs.  */
  if (!special_exponent)
    exponent -= fmt->exp_bias;

  /* Build the result algebraically.  Might go infinite, underflow, etc;
     who cares. */

/* If this format uses a hidden bit, explicitly add it in now.  Otherwise,
   increment the exponent by one to account for the integer bit.  */

  if (!special_exponent)
    if (fmt->intbit == floatformat_intbit_no)
      dto = ldexp (1.0, exponent);
    else
      exponent++;

  while (mant_bits_left > 0)
    {
      mant_bits = min (mant_bits_left, 32);

      mant = get_field (ufrom, fmt->byteorder, fmt->totalsize,
			 mant_off, mant_bits);

      dto += ldexp ((double)mant, exponent - mant_bits);
      exponent -= mant_bits;
      mant_off += mant_bits;
      mant_bits_left -= mant_bits;
    }

  /* Negate it if negative.  */
  if (get_field (ufrom, fmt->byteorder, fmt->totalsize, fmt->sign_start, 1))
    dto = -dto;
  *to = dto;
}

static void put_field PARAMS ((unsigned char *, enum floatformat_byteorders,
			       unsigned int,
			       unsigned int,
			       unsigned int,
			       unsigned long));

/* Set a field which starts at START and is LEN bytes long.  DATA and
   TOTAL_LEN are the thing we are extracting it from, in byteorder ORDER.  */
static void
put_field (data, order, total_len, start, len, stuff_to_put)
     unsigned char *data;
     enum floatformat_byteorders order;
     unsigned int total_len;
     unsigned int start;
     unsigned int len;
     unsigned long stuff_to_put;
{
  unsigned int cur_byte;
  int cur_bitshift;

  /* Start at the least significant part of the field.  */
  cur_byte = (start + len) / FLOATFORMAT_CHAR_BIT;
  if (order == floatformat_little || order == floatformat_littlebyte_bigword)
    cur_byte = (total_len / FLOATFORMAT_CHAR_BIT) - cur_byte - 1;
  cur_bitshift =
    ((start + len) % FLOATFORMAT_CHAR_BIT) - FLOATFORMAT_CHAR_BIT;
  *(data + cur_byte) &=
    ~(((1 << ((start + len) % FLOATFORMAT_CHAR_BIT)) - 1) << (-cur_bitshift));
  *(data + cur_byte) |=
    (stuff_to_put & ((1 << FLOATFORMAT_CHAR_BIT) - 1)) << (-cur_bitshift);
  cur_bitshift += FLOATFORMAT_CHAR_BIT;
  if (order == floatformat_little || order == floatformat_littlebyte_bigword)
    ++cur_byte;
  else
    --cur_byte;

  /* Move towards the most significant part of the field.  */
  while (cur_bitshift < len)
    {
      if (len - cur_bitshift < FLOATFORMAT_CHAR_BIT)
	{
	  /* This is the last byte.  */
	  *(data + cur_byte) &=
	    ~((1 << (len - cur_bitshift)) - 1);
	  *(data + cur_byte) |= (stuff_to_put >> cur_bitshift);
	}
      else
	*(data + cur_byte) = ((stuff_to_put >> cur_bitshift)
			      & ((1 << FLOATFORMAT_CHAR_BIT) - 1));
      cur_bitshift += FLOATFORMAT_CHAR_BIT;
      if (order == floatformat_little || order == floatformat_littlebyte_bigword)
	++cur_byte;
      else
	--cur_byte;
    }
}

#ifdef HAVE_LONG_DOUBLE
/* Return the fractional part of VALUE, and put the exponent of VALUE in *EPTR.
   The range of the returned value is >= 0.5 and < 1.0.  This is equivalent to
   frexp, but operates on the long double data type.  */

static long double ldfrexp PARAMS ((long double value, int *eptr));

static long double
ldfrexp (value, eptr)
     long double value;
     int *eptr;
{
  long double tmp;
  int exp;

  /* Unfortunately, there are no portable functions for extracting the exponent
     of a long double, so we have to do it iteratively by multiplying or dividing
     by two until the fraction is between 0.5 and 1.0.  */

  if (value < 0.0l)
    value = -value;

  tmp = 1.0l;
  exp = 0;

  if (value >= tmp)		/* Value >= 1.0 */
    while (value >= tmp)
      {
	tmp *= 2.0l;
	exp++;
      }
  else if (value != 0.0l)	/* Value < 1.0  and > 0.0 */
    {
      while (value < tmp)
	{
	  tmp /= 2.0l;
	  exp--;
	}
      tmp *= 2.0l;
      exp++;
    }

  *eptr = exp;
  return value/tmp;
}
#endif /* HAVE_LONG_DOUBLE */


/* The converse: convert the DOUBLEST *FROM to an extended float
   and store where TO points.  Neither FROM nor TO have any alignment
   restrictions.  */

void
floatformat_from_doublest (fmt, from, to)
     CONST struct floatformat *fmt;
     DOUBLEST *from;
     char *to;
{
  DOUBLEST dfrom;
  int exponent;
  DOUBLEST mant;
  unsigned int mant_bits, mant_off;
  int mant_bits_left;
  unsigned char *uto = (unsigned char *)to;

  memcpy (&dfrom, from, sizeof (dfrom));
  memset (uto, 0, fmt->totalsize / FLOATFORMAT_CHAR_BIT);
  if (dfrom == 0)
    return;			/* Result is zero */
  if (dfrom != dfrom)		/* Result is NaN */
    {
      /* From is NaN */
      put_field (uto, fmt->byteorder, fmt->totalsize, fmt->exp_start,
		 fmt->exp_len, fmt->exp_nan);
      /* Be sure it's not infinity, but NaN value is irrel */
      put_field (uto, fmt->byteorder, fmt->totalsize, fmt->man_start,
		 32, 1);
      return;
    }

  /* If negative, set the sign bit.  */
  if (dfrom < 0)
    {
      put_field (uto, fmt->byteorder, fmt->totalsize, fmt->sign_start, 1, 1);
      dfrom = -dfrom;
    }

  if (dfrom + dfrom == dfrom && dfrom != 0.0)	/* Result is Infinity */
    {
      /* Infinity exponent is same as NaN's.  */
      put_field (uto, fmt->byteorder, fmt->totalsize, fmt->exp_start,
		 fmt->exp_len, fmt->exp_nan);
      /* Infinity mantissa is all zeroes.  */
      put_field (uto, fmt->byteorder, fmt->totalsize, fmt->man_start,
		 fmt->man_len, 0);
      return;
    }

#ifdef HAVE_LONG_DOUBLE
  mant = ldfrexp (dfrom, &exponent);
#else
  mant = frexp (dfrom, &exponent);
#endif

  put_field (uto, fmt->byteorder, fmt->totalsize, fmt->exp_start, fmt->exp_len,
	     exponent + fmt->exp_bias - 1);

  mant_bits_left = fmt->man_len;
  mant_off = fmt->man_start;
  while (mant_bits_left > 0)
    {
      unsigned long mant_long;
      mant_bits = mant_bits_left < 32 ? mant_bits_left : 32;

      mant *= 4294967296.0;
      mant_long = (unsigned long)mant;
      mant -= mant_long;

      /* If the integer bit is implicit, then we need to discard it.
	 If we are discarding a zero, we should be (but are not) creating
	 a denormalized	number which means adjusting the exponent
	 (I think).  */
      if (mant_bits_left == fmt->man_len
	  && fmt->intbit == floatformat_intbit_no)
	{
	  mant_long <<= 1;
	  mant_bits -= 1;
	}

      if (mant_bits < 32)
	{
	  /* The bits we want are in the most significant MANT_BITS bits of
	     mant_long.  Move them to the least significant.  */
	  mant_long >>= 32 - mant_bits;
	}

      put_field (uto, fmt->byteorder, fmt->totalsize,
		 mant_off, mant_bits, mant_long);
      mant_off += mant_bits;
      mant_bits_left -= mant_bits;
    }
  if (fmt -> byteorder == floatformat_littlebyte_bigword)
    {
      int count;
      unsigned char *swaplow = uto;
      unsigned char *swaphigh = uto + 4;
      unsigned char tmp;

      for (count = 0; count < 4; count++)
	{
	  tmp = *swaplow;
	  *swaplow++ = *swaphigh;
	  *swaphigh++ = tmp;
	}
    }
}

/* temporary storage using circular buffer */
#define NUMCELLS 16
#define CELLSIZE 32
static char*
get_cell()
{
  static char buf[NUMCELLS][CELLSIZE];
  static int cell=0;
  if (++cell>=NUMCELLS) cell=0;
  return buf[cell];
}

/* print routines to handle variable size regs, etc.

   FIXME: Note that t_addr is a bfd_vma, which is currently either an
   unsigned long or unsigned long long, determined at configure time.
   If t_addr is an unsigned long long and sizeof (unsigned long long)
   is greater than sizeof (unsigned long), then I believe this code will
   probably lose, at least for little endian machines.  I believe that
   it would also be better to eliminate the switch on the absolute size
   of t_addr and replace it with a sequence of if statements that compare
   sizeof t_addr with sizeof the various types and do the right thing,
   which includes knowing whether or not the host supports long long.
   -fnf

 */

static int thirty_two = 32;	/* eliminate warning from compiler on 32-bit systems */

char* 
paddr(addr)
  t_addr addr;
{
  char *paddr_str=get_cell();
  switch (sizeof(t_addr))
    {
      case 8:
        sprintf (paddr_str, "%08lx%08lx",
		(unsigned long) (addr >> thirty_two), (unsigned long) (addr & 0xffffffff));
	break;
      case 4:
        sprintf (paddr_str, "%08lx", (unsigned long) addr);
	break;
      case 2:
        sprintf (paddr_str, "%04x", (unsigned short) (addr & 0xffff));
	break;
      default:
        sprintf (paddr_str, "%lx", (unsigned long) addr);
    }
  return paddr_str;
}

char* 
preg(reg)
  t_reg reg;
{
  char *preg_str=get_cell();
  switch (sizeof(t_reg))
    {
      case 8:
        sprintf (preg_str, "%08lx%08lx",
		(unsigned long) (reg >> thirty_two), (unsigned long) (reg & 0xffffffff));
	break;
      case 4:
        sprintf (preg_str, "%08lx", (unsigned long) reg);
	break;
      case 2:
        sprintf (preg_str, "%04x", (unsigned short) (reg & 0xffff));
	break;
      default:
        sprintf (preg_str, "%lx", (unsigned long) reg);
    }
  return preg_str;
}

char*
paddr_nz(addr)
  t_addr addr;
{
  char *paddr_str=get_cell();
  switch (sizeof(t_addr))
    {
      case 8:
	{
	  unsigned long high = (unsigned long) (addr >> thirty_two);
	  if (high == 0)
	    sprintf (paddr_str, "%lx", (unsigned long) (addr & 0xffffffff));
	  else
	    sprintf (paddr_str, "%lx%08lx",
		    high, (unsigned long) (addr & 0xffffffff));
	  break;
	}
      case 4:
        sprintf (paddr_str, "%lx", (unsigned long) addr);
	break;
      case 2:
        sprintf (paddr_str, "%x", (unsigned short) (addr & 0xffff));
	break;
      default:
        sprintf (paddr_str,"%lx", (unsigned long) addr);
    }
  return paddr_str;
}

char*
preg_nz(reg)
  t_reg reg;
{
  char *preg_str=get_cell();
  switch (sizeof(t_reg))
    {
      case 8:
	{
	  unsigned long high = (unsigned long) (reg >> thirty_two);
	  if (high == 0)
	    sprintf (preg_str, "%lx", (unsigned long) (reg & 0xffffffff));
	  else
	    sprintf (preg_str, "%lx%08lx",
		    high, (unsigned long) (reg & 0xffffffff));
	  break;
	}
      case 4:
        sprintf (preg_str, "%lx", (unsigned long) reg);
	break;
      case 2:
        sprintf (preg_str, "%x", (unsigned short) (reg & 0xffff));
	break;
      default:
        sprintf (preg_str, "%lx", (unsigned long) reg);
    }
  return preg_str;
}
