/* General utility routines for GDB, the GNU debugger.
   Copyright 1986, 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995,
   1996, 1997, 1998, 1999, 2000, 2001, 2002
   Free Software Foundation, Inc.

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
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

/* FIXME: cagney/2002-02-28: The GDB coding standard indicates that
   "defs.h" should be included first.  Unfortunatly some systems
   (currently Debian GNU/Linux) include the <stdbool.h> via <curses.h>
   and they clash with "bfd.h"'s definiton of true/false.  The correct
   fix is to remove true/false from "bfd.h", however, until that
   happens, hack around it by including "config.h" and <curses.h>
   first.  */

#include "config.h"

#ifdef HAVE_CURSES_H
#include <curses.h>
#endif
#ifdef HAVE_TERM_H
#include <term.h>
#endif

#include "defs.h"
#include "gdb_assert.h"
#include <ctype.h>
#include "gdb_string.h"
#include "event-top.h"

#ifdef __GO32__
#include <pc.h>
#endif

/* SunOS's curses.h has a '#define reg register' in it.  Thank you Sun. */
#ifdef reg
#undef reg
#endif

#include <signal.h>
#include "gdbcmd.h"
#include "serial.h"
#include "bfd.h"
#include "target.h"
#include "demangle.h"
#include "expression.h"
#include "language.h"
#include "annotate.h"

#include "inferior.h" /* for signed_pointer_to_address */

#include <sys/param.h>		/* For MAXPATHLEN */

#include <readline/readline.h>

#ifdef USE_MMALLOC
#include "mmalloc.h"
#endif

#ifdef NEED_DECLARATION_MALLOC
extern PTR malloc ();
#endif
#ifdef NEED_DECLARATION_REALLOC
extern PTR realloc ();
#endif
#ifdef NEED_DECLARATION_FREE
extern void free ();
#endif
/* Actually, we'll never have the decl, since we don't define _GNU_SOURCE.  */
#if defined(HAVE_CANONICALIZE_FILE_NAME) \
    && defined(NEED_DECLARATION_CANONICALIZE_FILE_NAME)
extern char *canonicalize_file_name (const char *);
#endif

#undef XMALLOC
#define XMALLOC(TYPE) ((TYPE*) xmalloc (sizeof (TYPE)))

/* readline defines this.  */
#undef savestring

void (*error_begin_hook) (void);

/* Holds the last error message issued by gdb */

static struct ui_file *gdb_lasterr;

/* Prototypes for local functions */

static void vfprintf_maybe_filtered (struct ui_file *, const char *,
				     va_list, int);

static void fputs_maybe_filtered (const char *, struct ui_file *, int);

#if defined (USE_MMALLOC) && !defined (NO_MMCHECK)
static void malloc_botch (void);
#endif

static void prompt_for_continue (void);

static void set_width_command (char *, int, struct cmd_list_element *);

static void set_width (void);

/* Chain of cleanup actions established with make_cleanup,
   to be executed if an error happens.  */

static struct cleanup *cleanup_chain;	/* cleaned up after a failed command */
static struct cleanup *final_cleanup_chain;	/* cleaned up when gdb exits */
static struct cleanup *run_cleanup_chain;	/* cleaned up on each 'run' */
static struct cleanup *exec_cleanup_chain;	/* cleaned up on each execution command */
/* cleaned up on each error from within an execution command */
static struct cleanup *exec_error_cleanup_chain; 

/* Pointer to what is left to do for an execution command after the
   target stops. Used only in asynchronous mode, by targets that
   support async execution.  The finish and until commands use it. So
   does the target extended-remote command. */
struct continuation *cmd_continuation;
struct continuation *intermediate_continuation;

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
make_cleanup (make_cleanup_ftype *function, void *arg)
{
  return make_my_cleanup (&cleanup_chain, function, arg);
}

struct cleanup *
make_final_cleanup (make_cleanup_ftype *function, void *arg)
{
  return make_my_cleanup (&final_cleanup_chain, function, arg);
}

struct cleanup *
make_run_cleanup (make_cleanup_ftype *function, void *arg)
{
  return make_my_cleanup (&run_cleanup_chain, function, arg);
}

struct cleanup *
make_exec_cleanup (make_cleanup_ftype *function, void *arg)
{
  return make_my_cleanup (&exec_cleanup_chain, function, arg);
}

struct cleanup *
make_exec_error_cleanup (make_cleanup_ftype *function, void *arg)
{
  return make_my_cleanup (&exec_error_cleanup_chain, function, arg);
}

static void
do_freeargv (void *arg)
{
  freeargv ((char **) arg);
}

struct cleanup *
make_cleanup_freeargv (char **arg)
{
  return make_my_cleanup (&cleanup_chain, do_freeargv, arg);
}

static void
do_bfd_close_cleanup (void *arg)
{
  bfd_close (arg);
}

struct cleanup *
make_cleanup_bfd_close (bfd *abfd)
{
  return make_cleanup (do_bfd_close_cleanup, abfd);
}

static void
do_close_cleanup (void *arg)
{
  int *fd = arg;
  close (*fd);
  xfree (fd);
}

struct cleanup *
make_cleanup_close (int fd)
{
  int *saved_fd = xmalloc (sizeof (fd));
  *saved_fd = fd;
  return make_cleanup (do_close_cleanup, saved_fd);
}

static void
do_ui_file_delete (void *arg)
{
  ui_file_delete (arg);
}

struct cleanup *
make_cleanup_ui_file_delete (struct ui_file *arg)
{
  return make_my_cleanup (&cleanup_chain, do_ui_file_delete, arg);
}

struct cleanup *
make_my_cleanup (struct cleanup **pmy_chain, make_cleanup_ftype *function,
		 void *arg)
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
do_cleanups (register struct cleanup *old_chain)
{
  do_my_cleanups (&cleanup_chain, old_chain);
}

void
do_final_cleanups (register struct cleanup *old_chain)
{
  do_my_cleanups (&final_cleanup_chain, old_chain);
}

void
do_run_cleanups (register struct cleanup *old_chain)
{
  do_my_cleanups (&run_cleanup_chain, old_chain);
}

void
do_exec_cleanups (register struct cleanup *old_chain)
{
  do_my_cleanups (&exec_cleanup_chain, old_chain);
}

void
do_exec_error_cleanups (register struct cleanup *old_chain)
{
  do_my_cleanups (&exec_error_cleanup_chain, old_chain);
}

void
do_my_cleanups (register struct cleanup **pmy_chain,
		register struct cleanup *old_chain)
{
  register struct cleanup *ptr;
  while ((ptr = *pmy_chain) != old_chain)
    {
      *pmy_chain = ptr->next;	/* Do this first incase recursion */
      (*ptr->function) (ptr->arg);
      xfree (ptr);
    }
}

/* Discard cleanups, not doing the actions they describe,
   until we get back to the point OLD_CHAIN in the cleanup_chain.  */

void
discard_cleanups (register struct cleanup *old_chain)
{
  discard_my_cleanups (&cleanup_chain, old_chain);
}

void
discard_final_cleanups (register struct cleanup *old_chain)
{
  discard_my_cleanups (&final_cleanup_chain, old_chain);
}

void
discard_exec_error_cleanups (register struct cleanup *old_chain)
{
  discard_my_cleanups (&exec_error_cleanup_chain, old_chain);
}

void
discard_my_cleanups (register struct cleanup **pmy_chain,
		     register struct cleanup *old_chain)
{
  register struct cleanup *ptr;
  while ((ptr = *pmy_chain) != old_chain)
    {
      *pmy_chain = ptr->next;
      xfree (ptr);
    }
}

/* Set the cleanup_chain to 0, and return the old cleanup chain.  */
struct cleanup *
save_cleanups (void)
{
  return save_my_cleanups (&cleanup_chain);
}

struct cleanup *
save_final_cleanups (void)
{
  return save_my_cleanups (&final_cleanup_chain);
}

struct cleanup *
save_my_cleanups (struct cleanup **pmy_chain)
{
  struct cleanup *old_chain = *pmy_chain;

  *pmy_chain = 0;
  return old_chain;
}

/* Restore the cleanup chain from a previously saved chain.  */
void
restore_cleanups (struct cleanup *chain)
{
  restore_my_cleanups (&cleanup_chain, chain);
}

void
restore_final_cleanups (struct cleanup *chain)
{
  restore_my_cleanups (&final_cleanup_chain, chain);
}

void
restore_my_cleanups (struct cleanup **pmy_chain, struct cleanup *chain)
{
  *pmy_chain = chain;
}

/* This function is useful for cleanups.
   Do

   foo = xmalloc (...);
   old_chain = make_cleanup (free_current_contents, &foo);

   to arrange to free the object thus allocated.  */

void
free_current_contents (void *ptr)
{
  void **location = ptr;
  if (location == NULL)
    internal_error (__FILE__, __LINE__,
		    "free_current_contents: NULL pointer");
  if (*location != NULL)
    {
      xfree (*location);
      *location = NULL;
    }
}

/* Provide a known function that does nothing, to use as a base for
   for a possibly long chain of cleanups.  This is useful where we
   use the cleanup chain for handling normal cleanups as well as dealing
   with cleanups that need to be done as a result of a call to error().
   In such cases, we may not be certain where the first cleanup is, unless
   we have a do-nothing one to always use as the base. */

/* ARGSUSED */
void
null_cleanup (void *arg)
{
}

/* Add a continuation to the continuation list, the global list
   cmd_continuation. The new continuation will be added at the front.*/
void
add_continuation (void (*continuation_hook) (struct continuation_arg *),
		  struct continuation_arg *arg_list)
{
  struct continuation *continuation_ptr;

  continuation_ptr = (struct continuation *) xmalloc (sizeof (struct continuation));
  continuation_ptr->continuation_hook = continuation_hook;
  continuation_ptr->arg_list = arg_list;
  continuation_ptr->next = cmd_continuation;
  cmd_continuation = continuation_ptr;
}

/* Walk down the cmd_continuation list, and execute all the
   continuations. There is a problem though. In some cases new
   continuations may be added while we are in the middle of this
   loop. If this happens they will be added in the front, and done
   before we have a chance of exhausting those that were already
   there. We need to then save the beginning of the list in a pointer
   and do the continuations from there on, instead of using the
   global beginning of list as our iteration pointer.*/
void
do_all_continuations (void)
{
  struct continuation *continuation_ptr;
  struct continuation *saved_continuation;

  /* Copy the list header into another pointer, and set the global
     list header to null, so that the global list can change as a side
     effect of invoking the continuations and the processing of
     the preexisting continuations will not be affected. */
  continuation_ptr = cmd_continuation;
  cmd_continuation = NULL;

  /* Work now on the list we have set aside. */
  while (continuation_ptr)
     {
       (continuation_ptr->continuation_hook) (continuation_ptr->arg_list);
       saved_continuation = continuation_ptr;
       continuation_ptr = continuation_ptr->next;
       xfree (saved_continuation);
     }
}

/* Walk down the cmd_continuation list, and get rid of all the
   continuations. */
void
discard_all_continuations (void)
{
  struct continuation *continuation_ptr;

  while (cmd_continuation)
    {
      continuation_ptr = cmd_continuation;
      cmd_continuation = continuation_ptr->next;
      xfree (continuation_ptr);
    }
}

/* Add a continuation to the continuation list, the global list
   intermediate_continuation. The new continuation will be added at the front.*/
void
add_intermediate_continuation (void (*continuation_hook)
			       (struct continuation_arg *),
			       struct continuation_arg *arg_list)
{
  struct continuation *continuation_ptr;

  continuation_ptr = (struct continuation *) xmalloc (sizeof (struct continuation));
  continuation_ptr->continuation_hook = continuation_hook;
  continuation_ptr->arg_list = arg_list;
  continuation_ptr->next = intermediate_continuation;
  intermediate_continuation = continuation_ptr;
}

/* Walk down the cmd_continuation list, and execute all the
   continuations. There is a problem though. In some cases new
   continuations may be added while we are in the middle of this
   loop. If this happens they will be added in the front, and done
   before we have a chance of exhausting those that were already
   there. We need to then save the beginning of the list in a pointer
   and do the continuations from there on, instead of using the
   global beginning of list as our iteration pointer.*/
void
do_all_intermediate_continuations (void)
{
  struct continuation *continuation_ptr;
  struct continuation *saved_continuation;

  /* Copy the list header into another pointer, and set the global
     list header to null, so that the global list can change as a side
     effect of invoking the continuations and the processing of
     the preexisting continuations will not be affected. */
  continuation_ptr = intermediate_continuation;
  intermediate_continuation = NULL;

  /* Work now on the list we have set aside. */
  while (continuation_ptr)
     {
       (continuation_ptr->continuation_hook) (continuation_ptr->arg_list);
       saved_continuation = continuation_ptr;
       continuation_ptr = continuation_ptr->next;
       xfree (saved_continuation);
     }
}

/* Walk down the cmd_continuation list, and get rid of all the
   continuations. */
void
discard_all_intermediate_continuations (void)
{
  struct continuation *continuation_ptr;

  while (intermediate_continuation)
    {
      continuation_ptr = intermediate_continuation;
      intermediate_continuation = continuation_ptr->next;
      xfree (continuation_ptr);
    }
}



/* Print a warning message.  The first argument STRING is the warning
   message, used as an fprintf format string, the second is the
   va_list of arguments for that string.  A warning is unfiltered (not
   paginated) so that the user does not need to page through each
   screen full of warnings when there are lots of them.  */

void
vwarning (const char *string, va_list args)
{
  if (warning_hook)
    (*warning_hook) (string, args);
  else
    {
      target_terminal_ours ();
      wrap_here ("");		/* Force out any buffered output */
      gdb_flush (gdb_stdout);
      if (warning_pre_print)
	fprintf_unfiltered (gdb_stderr, warning_pre_print);
      vfprintf_unfiltered (gdb_stderr, string, args);
      fprintf_unfiltered (gdb_stderr, "\n");
      va_end (args);
    }
}

/* Print a warning message.
   The first argument STRING is the warning message, used as a fprintf string,
   and the remaining args are passed as arguments to it.
   The primary difference between warnings and errors is that a warning
   does not force the return to command level.  */

void
warning (const char *string,...)
{
  va_list args;
  va_start (args, string);
  vwarning (string, args);
  va_end (args);
}

/* Print an error message and return to command level.
   The first argument STRING is the error message, used as a fprintf string,
   and the remaining args are passed as arguments to it.  */

NORETURN void
verror (const char *string, va_list args)
{
  struct ui_file *tmp_stream = mem_fileopen ();
  make_cleanup_ui_file_delete (tmp_stream);
  vfprintf_unfiltered (tmp_stream, string, args);
  error_stream (tmp_stream);
}

NORETURN void
error (const char *string,...)
{
  va_list args;
  va_start (args, string);
  verror (string, args);
  va_end (args);
}

static void
do_write (void *data, const char *buffer, long length_buffer)
{
  ui_file_write (data, buffer, length_buffer);
}

NORETURN void
error_stream (struct ui_file *stream)
{
  if (error_begin_hook)
    error_begin_hook ();

  /* Copy the stream into the GDB_LASTERR buffer.  */
  ui_file_rewind (gdb_lasterr);
  ui_file_put (stream, do_write, gdb_lasterr);

  /* Write the message plus any error_pre_print to gdb_stderr.  */
  target_terminal_ours ();
  wrap_here ("");		/* Force out any buffered output */
  gdb_flush (gdb_stdout);
  annotate_error_begin ();
  if (error_pre_print)
    fprintf_filtered (gdb_stderr, error_pre_print);
  ui_file_put (stream, do_write, gdb_stderr);
  fprintf_filtered (gdb_stderr, "\n");

  throw_exception (RETURN_ERROR);
}

/* Get the last error message issued by gdb */

char *
error_last_message (void)
{
  long len;
  return ui_file_xstrdup (gdb_lasterr, &len);
}
  
/* This is to be called by main() at the very beginning */

void
error_init (void)
{
  gdb_lasterr = mem_fileopen ();
}

/* Print a message reporting an internal error. Ask the user if they
   want to continue, dump core, or just exit. */

NORETURN void
internal_verror (const char *file, int line,
		 const char *fmt, va_list ap)
{
  static char msg[] = "Internal GDB error: recursive internal error.\n";
  static int dejavu = 0;
  int quit_p;
  int dump_core_p;

  /* don't allow infinite error recursion. */
  switch (dejavu)
    {
    case 0:
      dejavu = 1;
      break;
    case 1:
      dejavu = 2;
      fputs_unfiltered (msg, gdb_stderr);
      abort (); /* NOTE: GDB has only three calls to abort().  */
    default:
      dejavu = 3;
      write (STDERR_FILENO, msg, sizeof (msg));
      exit (1);
    }

  /* Try to get the message out */
  target_terminal_ours ();
  fprintf_unfiltered (gdb_stderr, "%s:%d: gdb-internal-error: ", file, line);
  vfprintf_unfiltered (gdb_stderr, fmt, ap);
  fputs_unfiltered ("\n", gdb_stderr);

  /* Default (yes/batch case) is to quit GDB.  When in batch mode this
     lessens the likelhood of GDB going into an infinate loop. */
  quit_p = query ("\
An internal GDB error was detected.  This may make further\n\
debugging unreliable.  Quit this debugging session? ");

  /* Default (yes/batch case) is to dump core.  This leaves a GDB
     dropping so that it is easier to see that something went wrong to
     GDB. */
  dump_core_p = query ("\
Create a core file containing the current state of GDB? ");

  if (quit_p)
    {
      if (dump_core_p)
	abort (); /* NOTE: GDB has only three calls to abort().  */
      else
	exit (1);
    }
  else
    {
      if (dump_core_p)
	{
	  if (fork () == 0)
	    abort (); /* NOTE: GDB has only three calls to abort().  */
	}
    }

  dejavu = 0;
  throw_exception (RETURN_ERROR);
}

NORETURN void
internal_error (const char *file, int line, const char *string, ...)
{
  va_list ap;
  va_start (ap, string);

  internal_verror (file, line, string, ap);
  va_end (ap);
}

/* The strerror() function can return NULL for errno values that are
   out of range.  Provide a "safe" version that always returns a
   printable string. */

char *
safe_strerror (int errnum)
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

/* Print the system error message for errno, and also mention STRING
   as the file name for which the error was encountered.
   Then return to command level.  */

NORETURN void
perror_with_name (const char *string)
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
print_sys_errmsg (const char *string, int errcode)
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
quit (void)
{
  struct serial *gdb_stdout_serial = serial_fdopen (1);

  target_terminal_ours ();

  /* We want all output to appear now, before we print "Quit".  We
     have 3 levels of buffering we have to flush (it's possible that
     some of these should be changed to flush the lower-level ones
     too):  */

  /* 1.  The _filtered buffer.  */
  wrap_here ((char *) 0);

  /* 2.  The stdio buffer.  */
  gdb_flush (gdb_stdout);
  gdb_flush (gdb_stderr);

  /* 3.  The system-level buffer.  */
  serial_drain_output (gdb_stdout_serial);
  serial_un_fdopen (gdb_stdout_serial);

  annotate_error_begin ();

  /* Don't use *_filtered; we don't want to prompt the user to continue.  */
  if (quit_pre_print)
    fprintf_unfiltered (gdb_stderr, quit_pre_print);

#ifdef __MSDOS__
  /* No steenking SIGINT will ever be coming our way when the
     program is resumed.  Don't lie.  */
  fprintf_unfiltered (gdb_stderr, "Quit\n");
#else
  if (job_control
  /* If there is no terminal switching for this target, then we can't
     possibly get screwed by the lack of job control.  */
      || current_target.to_terminal_ours == NULL)
    fprintf_unfiltered (gdb_stderr, "Quit\n");
  else
    fprintf_unfiltered (gdb_stderr,
	       "Quit (expect signal SIGINT when the program is resumed)\n");
#endif
  throw_exception (RETURN_QUIT);
}

/* Control C comes here */
void
request_quit (int signo)
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

#if !defined (USE_MMALLOC)

/* NOTE: These must use PTR so that their definition matches the
   declaration found in "mmalloc.h". */

static void *
mmalloc (void *md, size_t size)
{
  return malloc (size); /* NOTE: GDB's only call to malloc() */
}

static void *
mrealloc (void *md, void *ptr, size_t size)
{
  if (ptr == 0)			/* Guard against old realloc's */
    return mmalloc (md, size);
  else
    return realloc (ptr, size); /* NOTE: GDB's only call to ralloc() */
}

static void *
mcalloc (void *md, size_t number, size_t size)
{
  return calloc (number, size); /* NOTE: GDB's only call to calloc() */
}

static void
mfree (void *md, void *ptr)
{
  free (ptr); /* NOTE: GDB's only call to free() */
}

#endif /* USE_MMALLOC */

#if !defined (USE_MMALLOC) || defined (NO_MMCHECK)

void
init_malloc (void *md)
{
}

#else /* Have mmalloc and want corruption checking */

static void
malloc_botch (void)
{
  fprintf_unfiltered (gdb_stderr, "Memory corruption\n");
  internal_error (__FILE__, __LINE__, "failed internal consistency check");
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
init_malloc (void *md)
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
nomem (long size)
{
  if (size > 0)
    {
      internal_error (__FILE__, __LINE__,
		      "virtual memory exhausted: can't allocate %ld bytes.", size);
    }
  else
    {
      internal_error (__FILE__, __LINE__,
		      "virtual memory exhausted.");
    }
}

/* The xmmalloc() family of memory management routines.

   These are are like the mmalloc() family except that they implement
   consistent semantics and guard against typical memory management
   problems: if a malloc fails, an internal error is thrown; if
   free(NULL) is called, it is ignored; if *alloc(0) is called, NULL
   is returned.

   All these routines are implemented using the mmalloc() family. */

void *
xmmalloc (void *md, size_t size)
{
  void *val;

  if (size == 0)
    {
      val = NULL;
    }
  else
    {
      val = mmalloc (md, size);
      if (val == NULL)
	nomem (size);
    }
  return (val);
}

void *
xmrealloc (void *md, void *ptr, size_t size)
{
  void *val;

  if (size == 0)
    {
      if (ptr != NULL)
	mfree (md, ptr);
      val = NULL;
    }
  else
    {
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
    }
  return (val);
}

void *
xmcalloc (void *md, size_t number, size_t size)
{
  void *mem;
  if (number == 0 || size == 0)
    mem = NULL;
  else
    {
      mem = mcalloc (md, number, size);
      if (mem == NULL)
	nomem (number * size);
    }
  return mem;
}

void
xmfree (void *md, void *ptr)
{
  if (ptr != NULL)
    mfree (md, ptr);
}

/* The xmalloc() (libiberty.h) family of memory management routines.

   These are like the ISO-C malloc() family except that they implement
   consistent semantics and guard against typical memory management
   problems.  See xmmalloc() above for further information.

   All these routines are wrappers to the xmmalloc() family. */

/* NOTE: These are declared using PTR to ensure consistency with
   "libiberty.h".  xfree() is GDB local.  */

PTR
xmalloc (size_t size)
{
  return xmmalloc (NULL, size);
}

PTR
xrealloc (PTR ptr, size_t size)
{
  return xmrealloc (NULL, ptr, size);
}

PTR
xcalloc (size_t number, size_t size)
{
  return xmcalloc (NULL, number, size);
}

void
xfree (void *ptr)
{
  xmfree (NULL, ptr);
}


/* Like asprintf/vasprintf but get an internal_error if the call
   fails. */

void
xasprintf (char **ret, const char *format, ...)
{
  va_list args;
  va_start (args, format);
  xvasprintf (ret, format, args);
  va_end (args);
}

void
xvasprintf (char **ret, const char *format, va_list ap)
{
  int status = vasprintf (ret, format, ap);
  /* NULL could be returned due to a memory allocation problem; a
     badly format string; or something else. */
  if ((*ret) == NULL)
    internal_error (__FILE__, __LINE__,
		    "vasprintf returned NULL buffer (errno %d)",
		    errno);
  /* A negative status with a non-NULL buffer shouldn't never
     happen. But to be sure. */
  if (status < 0)
    internal_error (__FILE__, __LINE__,
		    "vasprintf call failed (errno %d)",
		    errno);
}


/* My replacement for the read system call.
   Used like `read' but keeps going if `read' returns too soon.  */

int
myread (int desc, char *addr, int len)
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
savestring (const char *ptr, size_t size)
{
  register char *p = (char *) xmalloc (size + 1);
  memcpy (p, ptr, size);
  p[size] = 0;
  return p;
}

char *
msavestring (void *md, const char *ptr, size_t size)
{
  register char *p = (char *) xmmalloc (md, size + 1);
  memcpy (p, ptr, size);
  p[size] = 0;
  return p;
}

char *
mstrsave (void *md, const char *ptr)
{
  return (msavestring (md, ptr, strlen (ptr)));
}

void
print_spaces (register int n, register struct ui_file *file)
{
  fputs_unfiltered (n_spaces (n), file);
}

/* Print a host address.  */

void
gdb_print_host_address (void *addr, struct ui_file *stream)
{

  /* We could use the %p conversion specifier to fprintf if we had any
     way of knowing whether this host supports it.  But the following
     should work on the Alpha and on 32 bit machines.  */

  fprintf_filtered (stream, "0x%lx", (unsigned long) addr);
}

/* Ask user a y-or-n question and return 1 iff answer is yes.
   Takes three args which are given to printf to print the question.
   The first, a control string, should end in "? ".
   It should not say how to answer, because we do that.  */

/* VARARGS */
int
query (const char *ctlstr,...)
{
  va_list args;
  register int answer;
  register int ans2;
  int retval;

  va_start (args, ctlstr);

  if (query_hook)
    {
      return query_hook (ctlstr, args);
    }

  /* Automatically answer "yes" if input is not from a terminal.  */
  if (!input_from_terminal_p ())
    return 1;

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

      wrap_here ("");
      gdb_flush (gdb_stdout);

      answer = fgetc (stdin);
      clearerr (stdin);		/* in case of C-d */
      if (answer == EOF)	/* C-d */
	{
	  retval = 1;
	  break;
	}
      /* Eat rest of input line, to EOF or newline */
      if (answer != '\n')
	do
	  {
            ans2 = fgetc (stdin);
	    clearerr (stdin);
	  }
	while (ans2 != EOF && ans2 != '\n' && ans2 != '\r');

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
parse_escape (char **string_ptr)
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

static void
printchar (int c, void (*do_fputs) (const char *, struct ui_file *),
	   void (*do_fprintf) (struct ui_file *, const char *, ...),
	   struct ui_file *stream, int quoter)
{

  c &= 0xFF;			/* Avoid sign bit follies */

  if (c < 0x20 ||		/* Low control chars */
      (c >= 0x7F && c < 0xA0) ||	/* DEL, High controls */
      (sevenbit_strings && c >= 0x80))
    {				/* high order bit set */
      switch (c)
	{
	case '\n':
	  do_fputs ("\\n", stream);
	  break;
	case '\b':
	  do_fputs ("\\b", stream);
	  break;
	case '\t':
	  do_fputs ("\\t", stream);
	  break;
	case '\f':
	  do_fputs ("\\f", stream);
	  break;
	case '\r':
	  do_fputs ("\\r", stream);
	  break;
	case '\033':
	  do_fputs ("\\e", stream);
	  break;
	case '\007':
	  do_fputs ("\\a", stream);
	  break;
	default:
	  do_fprintf (stream, "\\%.3o", (unsigned int) c);
	  break;
	}
    }
  else
    {
      if (c == '\\' || c == quoter)
	do_fputs ("\\", stream);
      do_fprintf (stream, "%c", c);
    }
}

/* Print the character C on STREAM as part of the contents of a
   literal string whose delimiter is QUOTER.  Note that these routines
   should only be call for printing things which are independent of
   the language of the program being debugged. */

void
fputstr_filtered (const char *str, int quoter, struct ui_file *stream)
{
  while (*str)
    printchar (*str++, fputs_filtered, fprintf_filtered, stream, quoter);
}

void
fputstr_unfiltered (const char *str, int quoter, struct ui_file *stream)
{
  while (*str)
    printchar (*str++, fputs_unfiltered, fprintf_unfiltered, stream, quoter);
}

void
fputstrn_unfiltered (const char *str, int n, int quoter, struct ui_file *stream)
{
  int i;
  for (i = 0; i < n; i++)
    printchar (str[i], fputs_unfiltered, fprintf_unfiltered, stream, quoter);
}



/* Number of lines per page or UINT_MAX if paging is disabled.  */
static unsigned int lines_per_page;
/* Number of chars per line or UINT_MAX if line folding is disabled.  */
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
init_page_info (void)
{
#if defined(TUI)
  if (!tui_get_command_dimension (&chars_per_line, &lines_per_page))
#endif
    {
      /* These defaults will be used if we are unable to get the correct
         values from termcap.  */
#if defined(__GO32__)
      lines_per_page = ScreenRows ();
      chars_per_line = ScreenCols ();
#else
      lines_per_page = 24;
      chars_per_line = 80;

#if !defined (_WIN32)
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
      if (!ui_file_isatty (gdb_stdout))
	lines_per_page = UINT_MAX;
    }				/* the command_line_version */
  set_width ();
}

static void
set_width (void)
{
  if (chars_per_line == 0)
    init_page_info ();

  if (!wrap_buffer)
    {
      wrap_buffer = (char *) xmalloc (chars_per_line + 2);
      wrap_buffer[0] = '\0';
    }
  else
    wrap_buffer = (char *) xrealloc (wrap_buffer, chars_per_line + 2);
  wrap_pointer = wrap_buffer;	/* Start it at the beginning */
}

/* ARGSUSED */
static void
set_width_command (char *args, int from_tty, struct cmd_list_element *c)
{
  set_width ();
}

/* Wait, so the user can read what's on the screen.  Prompt the user
   to continue by pressing RETURN.  */

static void
prompt_for_continue (void)
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
	{
	  if (!event_loop_p)
	    request_quit (SIGINT);
	  else
	    async_request_quit (0);
	}
      xfree (ignore);
    }
  immediate_quit--;

  /* Now we have to do this again, so that GDB will know that it doesn't
     need to save the ---Type <return>--- line at the top of the screen.  */
  reinitialize_more_filter ();

  dont_repeat ();		/* Forget prev cmd -- CR won't repeat it. */
}

/* Reinitialize filter; ie. tell it to reset to original values.  */

void
reinitialize_more_filter (void)
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
wrap_here (char *indent)
{
  /* This should have been allocated, but be paranoid anyway. */
  if (!wrap_buffer)
    internal_error (__FILE__, __LINE__, "failed internal consistency check");

  if (wrap_buffer[0])
    {
      *wrap_pointer = '\0';
      fputs_unfiltered (wrap_buffer, gdb_stdout);
    }
  wrap_pointer = wrap_buffer;
  wrap_buffer[0] = '\0';
  if (chars_per_line == UINT_MAX)	/* No line overflow checking */
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
begin_line (void)
{
  if (chars_printed > 0)
    {
      puts_filtered ("\n");
    }
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
fputs_maybe_filtered (const char *linebuffer, struct ui_file *stream,
		      int filter)
{
  const char *lineptr;

  if (linebuffer == 0)
    return;

  /* Don't do any filtering if it is disabled.  */
  if ((stream != gdb_stdout) || !pagination_enabled
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
		  *wrap_pointer = '\0';		/* Null-terminate saved stuff */
		  fputs_unfiltered (wrap_buffer, stream);	/* and eject it */
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
		  wrap_column = 0;	/* And disable fancy wrap */
		}
	    }
	}

      if (*lineptr == '\n')
	{
	  chars_printed = 0;
	  wrap_here ((char *) 0);	/* Spit out chars, cancel further wraps */
	  lines_printed++;
	  fputc_unfiltered ('\n', stream);
	  lineptr++;
	}
    }
}

void
fputs_filtered (const char *linebuffer, struct ui_file *stream)
{
  fputs_maybe_filtered (linebuffer, stream, 1);
}

int
putchar_unfiltered (int c)
{
  char buf = c;
  ui_file_write (gdb_stdout, &buf, 1);
  return c;
}

/* Write character C to gdb_stdout using GDB's paging mechanism and return C.
   May return nonlocally.  */

int
putchar_filtered (int c)
{
  return fputc_filtered (c, gdb_stdout);
}

int
fputc_unfiltered (int c, struct ui_file *stream)
{
  char buf = c;
  ui_file_write (stream, &buf, 1);
  return c;
}

int
fputc_filtered (int c, struct ui_file *stream)
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
puts_debug (char *prefix, char *string, char *suffix)
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
  if ((return_p || (strcmp (prev_prefix, prefix) != 0)) && !new_line)
    {
      fputs_unfiltered (prev_suffix, gdb_stdlog);
      fputs_unfiltered ("\n", gdb_stdlog);
      fputs_unfiltered (prefix, gdb_stdlog);
    }

  /* Print prefix if we printed a newline during the previous call.  */
  if (new_line)
    {
      new_line = 0;
      fputs_unfiltered (prefix, gdb_stdlog);
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
	    fputc_unfiltered (ch, gdb_stdlog);

	  else
	    fprintf_unfiltered (gdb_stdlog, "\\x%02x", ch & 0xff);
	  break;

	case '\\':
	  fputs_unfiltered ("\\\\", gdb_stdlog);
	  break;
	case '\b':
	  fputs_unfiltered ("\\b", gdb_stdlog);
	  break;
	case '\f':
	  fputs_unfiltered ("\\f", gdb_stdlog);
	  break;
	case '\n':
	  new_line = 1;
	  fputs_unfiltered ("\\n", gdb_stdlog);
	  break;
	case '\r':
	  fputs_unfiltered ("\\r", gdb_stdlog);
	  break;
	case '\t':
	  fputs_unfiltered ("\\t", gdb_stdlog);
	  break;
	case '\v':
	  fputs_unfiltered ("\\v", gdb_stdlog);
	  break;
	}

      return_p = ch == '\r';
    }

  /* Print suffix if we printed a newline.  */
  if (new_line)
    {
      fputs_unfiltered (suffix, gdb_stdlog);
      fputs_unfiltered ("\n", gdb_stdlog);
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
vfprintf_maybe_filtered (struct ui_file *stream, const char *format,
			 va_list args, int filter)
{
  char *linebuffer;
  struct cleanup *old_cleanups;

  xvasprintf (&linebuffer, format, args);
  old_cleanups = make_cleanup (xfree, linebuffer);
  fputs_maybe_filtered (linebuffer, stream, filter);
  do_cleanups (old_cleanups);
}


void
vfprintf_filtered (struct ui_file *stream, const char *format, va_list args)
{
  vfprintf_maybe_filtered (stream, format, args, 1);
}

void
vfprintf_unfiltered (struct ui_file *stream, const char *format, va_list args)
{
  char *linebuffer;
  struct cleanup *old_cleanups;

  xvasprintf (&linebuffer, format, args);
  old_cleanups = make_cleanup (xfree, linebuffer);
  fputs_unfiltered (linebuffer, stream);
  do_cleanups (old_cleanups);
}

void
vprintf_filtered (const char *format, va_list args)
{
  vfprintf_maybe_filtered (gdb_stdout, format, args, 1);
}

void
vprintf_unfiltered (const char *format, va_list args)
{
  vfprintf_unfiltered (gdb_stdout, format, args);
}

void
fprintf_filtered (struct ui_file * stream, const char *format,...)
{
  va_list args;
  va_start (args, format);
  vfprintf_filtered (stream, format, args);
  va_end (args);
}

void
fprintf_unfiltered (struct ui_file * stream, const char *format,...)
{
  va_list args;
  va_start (args, format);
  vfprintf_unfiltered (stream, format, args);
  va_end (args);
}

/* Like fprintf_filtered, but prints its result indented.
   Called as fprintfi_filtered (spaces, stream, format, ...);  */

void
fprintfi_filtered (int spaces, struct ui_file * stream, const char *format,...)
{
  va_list args;
  va_start (args, format);
  print_spaces_filtered (spaces, stream);

  vfprintf_filtered (stream, format, args);
  va_end (args);
}


void
printf_filtered (const char *format,...)
{
  va_list args;
  va_start (args, format);
  vfprintf_filtered (gdb_stdout, format, args);
  va_end (args);
}


void
printf_unfiltered (const char *format,...)
{
  va_list args;
  va_start (args, format);
  vfprintf_unfiltered (gdb_stdout, format, args);
  va_end (args);
}

/* Like printf_filtered, but prints it's result indented.
   Called as printfi_filtered (spaces, format, ...);  */

void
printfi_filtered (int spaces, const char *format,...)
{
  va_list args;
  va_start (args, format);
  print_spaces_filtered (spaces, gdb_stdout);
  vfprintf_filtered (gdb_stdout, format, args);
  va_end (args);
}

/* Easy -- but watch out!

   This routine is *not* a replacement for puts()!  puts() appends a newline.
   This one doesn't, and had better not!  */

void
puts_filtered (const char *string)
{
  fputs_filtered (string, gdb_stdout);
}

void
puts_unfiltered (const char *string)
{
  fputs_unfiltered (string, gdb_stdout);
}

/* Return a pointer to N spaces and a null.  The pointer is good
   until the next call to here.  */
char *
n_spaces (int n)
{
  char *t;
  static char *spaces = 0;
  static int max_spaces = -1;

  if (n > max_spaces)
    {
      if (spaces)
	xfree (spaces);
      spaces = (char *) xmalloc (n + 1);
      for (t = spaces + n; t != spaces;)
	*--t = ' ';
      spaces[n] = '\0';
      max_spaces = n;
    }

  return spaces + max_spaces - n;
}

/* Print N spaces.  */
void
print_spaces_filtered (int n, struct ui_file *stream)
{
  fputs_filtered (n_spaces (n), stream);
}

/* C++ demangler stuff.  */

/* fprintf_symbol_filtered attempts to demangle NAME, a symbol in language
   LANG, using demangling args ARG_MODE, and print it filtered to STREAM.
   If the name is not mangled, or the language for the name is unknown, or
   demangling is off, the name is printed in its "raw" form. */

void
fprintf_symbol_filtered (struct ui_file *stream, char *name, enum language lang,
			 int arg_mode)
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
	      xfree (demangled);
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
strcmp_iw (const char *string1, const char *string2)
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
   ** subset_compare()
   **    Answer whether string_to_compare is a full or partial match to
   **    template_string.  The partial match must be in sequence starting
   **    at index 0.
 */
int
subset_compare (char *string_to_compare, char *template_string)
{
  int match;
  if (template_string != (char *) NULL && string_to_compare != (char *) NULL &&
      strlen (string_to_compare) <= strlen (template_string))
    match = (strncmp (template_string,
		      string_to_compare,
		      strlen (string_to_compare)) == 0);
  else
    match = 0;
  return match;
}


static void pagination_on_command (char *arg, int from_tty);
static void
pagination_on_command (char *arg, int from_tty)
{
  pagination_enabled = 1;
}

static void pagination_on_command (char *arg, int from_tty);
static void
pagination_off_command (char *arg, int from_tty)
{
  pagination_enabled = 0;
}


void
initialize_utils (void)
{
  struct cmd_list_element *c;

  c = add_set_cmd ("width", class_support, var_uinteger,
		   (char *) &chars_per_line,
		   "Set number of characters gdb thinks are in a line.",
		   &setlist);
  add_show_from_set (c, &showlist);
  set_cmd_sfunc (c, set_width_command);

  add_show_from_set
    (add_set_cmd ("height", class_support,
		  var_uinteger, (char *) &lines_per_page,
		  "Set number of lines gdb thinks are in a page.", &setlist),
     &showlist);

  init_page_info ();

  /* If the output is not a terminal, don't paginate it.  */
  if (!ui_file_isatty (gdb_stdout))
    lines_per_page = UINT_MAX;

  set_width_command ((char *) NULL, 0, c);

  add_show_from_set
    (add_set_cmd ("demangle", class_support, var_boolean,
		  (char *) &demangle,
	     "Set demangling of encoded C++ names when displaying symbols.",
		  &setprintlist),
     &showprintlist);

  add_show_from_set
    (add_set_cmd ("pagination", class_support,
		  var_boolean, (char *) &pagination_enabled,
		  "Set state of pagination.", &setlist),
     &showlist);

  if (xdb_commands)
    {
      add_com ("am", class_support, pagination_on_command,
	       "Enable pagination");
      add_com ("sm", class_support, pagination_off_command,
	       "Disable pagination");
    }

  add_show_from_set
    (add_set_cmd ("sevenbit-strings", class_support, var_boolean,
		  (char *) &sevenbit_strings,
		  "Set printing of 8-bit characters in strings as \\nnn.",
		  &setprintlist),
     &showprintlist);

  add_show_from_set
    (add_set_cmd ("asm-demangle", class_support, var_boolean,
		  (char *) &asm_demangle,
		  "Set demangling of C++ names in disassembly listings.",
		  &setprintlist),
     &showprintlist);
}

/* Machine specific function to handle SIGWINCH signal. */

#ifdef  SIGWINCH_HANDLER_BODY
SIGWINCH_HANDLER_BODY
#endif

/* print routines to handle variable size regs, etc. */

/* temporary storage using circular buffer */
#define NUMCELLS 16
#define CELLSIZE 32
static char *
get_cell (void)
{
  static char buf[NUMCELLS][CELLSIZE];
  static int cell = 0;
  if (++cell >= NUMCELLS)
    cell = 0;
  return buf[cell];
}

int
strlen_paddr (void)
{
  return (TARGET_ADDR_BIT / 8 * 2);
}

char *
paddr (CORE_ADDR addr)
{
  return phex (addr, TARGET_ADDR_BIT / 8);
}

char *
paddr_nz (CORE_ADDR addr)
{
  return phex_nz (addr, TARGET_ADDR_BIT / 8);
}

static void
decimal2str (char *paddr_str, char *sign, ULONGEST addr)
{
  /* steal code from valprint.c:print_decimal().  Should this worry
     about the real size of addr as the above does? */
  unsigned long temp[3];
  int i = 0;
  do
    {
      temp[i] = addr % (1000 * 1000 * 1000);
      addr /= (1000 * 1000 * 1000);
      i++;
    }
  while (addr != 0 && i < (sizeof (temp) / sizeof (temp[0])));
  switch (i)
    {
    case 1:
      sprintf (paddr_str, "%s%lu",
	       sign, temp[0]);
      break;
    case 2:
      sprintf (paddr_str, "%s%lu%09lu",
	       sign, temp[1], temp[0]);
      break;
    case 3:
      sprintf (paddr_str, "%s%lu%09lu%09lu",
	       sign, temp[2], temp[1], temp[0]);
      break;
    default:
      internal_error (__FILE__, __LINE__, "failed internal consistency check");
    }
}

char *
paddr_u (CORE_ADDR addr)
{
  char *paddr_str = get_cell ();
  decimal2str (paddr_str, "", addr);
  return paddr_str;
}

char *
paddr_d (LONGEST addr)
{
  char *paddr_str = get_cell ();
  if (addr < 0)
    decimal2str (paddr_str, "-", -addr);
  else
    decimal2str (paddr_str, "", addr);
  return paddr_str;
}

/* eliminate warning from compiler on 32-bit systems */
static int thirty_two = 32;

char *
phex (ULONGEST l, int sizeof_l)
{
  char *str;
  switch (sizeof_l)
    {
    case 8:
      str = get_cell ();
      sprintf (str, "%08lx%08lx",
	       (unsigned long) (l >> thirty_two),
	       (unsigned long) (l & 0xffffffff));
      break;
    case 4:
      str = get_cell ();
      sprintf (str, "%08lx", (unsigned long) l);
      break;
    case 2:
      str = get_cell ();
      sprintf (str, "%04x", (unsigned short) (l & 0xffff));
      break;
    default:
      str = phex (l, sizeof (l));
      break;
    }
  return str;
}

char *
phex_nz (ULONGEST l, int sizeof_l)
{
  char *str;
  switch (sizeof_l)
    {
    case 8:
      {
	unsigned long high = (unsigned long) (l >> thirty_two);
	str = get_cell ();
	if (high == 0)
	  sprintf (str, "%lx", (unsigned long) (l & 0xffffffff));
	else
	  sprintf (str, "%lx%08lx",
		   high, (unsigned long) (l & 0xffffffff));
	break;
      }
    case 4:
      str = get_cell ();
      sprintf (str, "%lx", (unsigned long) l);
      break;
    case 2:
      str = get_cell ();
      sprintf (str, "%x", (unsigned short) (l & 0xffff));
      break;
    default:
      str = phex_nz (l, sizeof (l));
      break;
    }
  return str;
}


/* Convert to / from the hosts pointer to GDB's internal CORE_ADDR
   using the target's conversion routines. */
CORE_ADDR
host_pointer_to_address (void *ptr)
{
  if (sizeof (ptr) != TYPE_LENGTH (builtin_type_void_data_ptr))
    internal_error (__FILE__, __LINE__,
		    "core_addr_to_void_ptr: bad cast");
  return POINTER_TO_ADDRESS (builtin_type_void_data_ptr, &ptr);
}

void *
address_to_host_pointer (CORE_ADDR addr)
{
  void *ptr;
  if (sizeof (ptr) != TYPE_LENGTH (builtin_type_void_data_ptr))
    internal_error (__FILE__, __LINE__,
		    "core_addr_to_void_ptr: bad cast");
  ADDRESS_TO_POINTER (builtin_type_void_data_ptr, &ptr, addr);
  return ptr;
}

/* Convert a CORE_ADDR into a string.  */
const char *
core_addr_to_string (const CORE_ADDR addr)
{
  char *str = get_cell ();
  strcpy (str, "0x");
  strcat (str, phex (addr, sizeof (addr)));
  return str;
}

const char *
core_addr_to_string_nz (const CORE_ADDR addr)
{
  char *str = get_cell ();
  strcpy (str, "0x");
  strcat (str, phex_nz (addr, sizeof (addr)));
  return str;
}

/* Convert a string back into a CORE_ADDR.  */
CORE_ADDR
string_to_core_addr (const char *my_string)
{
  CORE_ADDR addr = 0;
  if (my_string[0] == '0' && tolower (my_string[1]) == 'x')
    {
      /* Assume that it is in decimal.  */
      int i;
      for (i = 2; my_string[i] != '\0'; i++)
	{
	  if (isdigit (my_string[i]))
	    addr = (my_string[i] - '0') + (addr * 16);
	  else if (isxdigit (my_string[i])) 
	    addr = (tolower (my_string[i]) - 'a' + 0xa) + (addr * 16);
	  else
	    internal_error (__FILE__, __LINE__, "invalid hex");
	}
    }
  else
    {
      /* Assume that it is in decimal.  */
      int i;
      for (i = 0; my_string[i] != '\0'; i++)
	{
	  if (isdigit (my_string[i]))
	    addr = (my_string[i] - '0') + (addr * 10);
	  else
	    internal_error (__FILE__, __LINE__, "invalid decimal");
	}
    }
  return addr;
}

char *
gdb_realpath (const char *filename)
{
#if defined(HAVE_REALPATH)
# if defined (PATH_MAX)
  char buf[PATH_MAX];
#  define USE_REALPATH
# elif defined (MAXPATHLEN)
  char buf[MAXPATHLEN];
#  define USE_REALPATH
# elif defined (HAVE_UNISTD_H) && defined(HAVE_ALLOCA)
  char *buf = alloca ((size_t)pathconf ("/", _PC_PATH_MAX));
#  define USE_REALPATH
# endif
#endif /* HAVE_REALPATH */

#if defined(USE_REALPATH)
  char *rp = realpath (filename, buf);
  return xstrdup (rp ? rp : filename);
#elif defined(HAVE_CANONICALIZE_FILE_NAME)
  char *rp = canonicalize_file_name (filename);
  if (rp == NULL)
    return xstrdup (filename);
  else
    return rp;
#else
  return xstrdup (filename);
#endif
}
