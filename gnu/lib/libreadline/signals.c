/* signals.c -- signal handling support for readline. */

/* Copyright (C) 1987, 1989, 1992 Free Software Foundation, Inc.

   This file is part of the GNU Readline Library, a library for
   reading lines of text with interactive input and history editing.

   The GNU Readline Library is free software; you can redistribute it
   and/or modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 1, or
   (at your option) any later version.

   The GNU Readline Library is distributed in the hope that it will be
   useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   The GNU General Public License is often shipped with GNU software, and
   is generally kept in a file called COPYING or LICENSE.  If you do not
   have a copy of the license, write to the Free Software Foundation,
   675 Mass Ave, Cambridge, MA 02139, USA. */

#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#if !defined (NO_SYS_FILE)
#  include <sys/file.h>
#endif /* !NO_SYS_FILE */
#include <signal.h>

/* This is needed to include support for TIOCGWINSZ and window resizing. */
#if defined (OSF1) || defined (BSD386) || defined (_386BSD) || defined (__BSD_4_4__) || defined (AIX)
#  include <sys/ioctl.h>
#endif /* OSF1 || BSD386 || _386BSD || __BSD_4_4__ || AIX */

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif /* HAVE_UNISTD_H */

#if defined (HAVE_STDLIB_H)
#  include <stdlib.h>
#else
#  include "ansi_stdlib.h"
#endif /* HAVE_STDLIB_H */

#include <errno.h>
/* Not all systems declare ERRNO in errno.h... and some systems #define it! */
#if !defined (errno)
extern int errno;
#endif /* !errno */

#include "posixstat.h"

/* System-specific feature definitions and include files. */
#include "rldefs.h"

/* Some standard library routines. */
#include "readline.h"
#include "history.h"

static void cr ();

extern int readline_echoing_p;
extern int rl_pending_input;
extern char *term_cr;

extern int _rl_meta_flag;

extern int _rl_output_character_function ();

extern void free_undo_list ();

#if defined (VOID_SIGHANDLER)
#  define sighandler void
#else
#  define sighandler int
#endif /* VOID_SIGHANDLER */

/* This typedef is equivalant to the one for Function; it allows us
   to say SigHandler *foo = signal (SIGKILL, SIG_IGN); */
typedef sighandler SigHandler ();

#if defined (__GO32__)
#  undef HANDLE_SIGNALS
#endif /* __GO32__ */

#if defined (STATIC_MALLOC)
static char *xmalloc (), *xrealloc ();
#else
extern char *xmalloc (), *xrealloc ();
#endif /* STATIC_MALLOC */


/* **************************************************************** */
/*					        		    */
/*			   Signal Handling                          */
/*								    */
/* **************************************************************** */

#if defined (SIGWINCH)
static SigHandler *old_sigwinch = (SigHandler *)NULL;

static sighandler
rl_handle_sigwinch (sig)
     int sig;
{
  if (readline_echoing_p)
    {
      _rl_set_screen_size (fileno (rl_instream), 1);

      cr ();				/* was crlf () */
      rl_forced_update_display ();
    }

  if (old_sigwinch &&
      old_sigwinch != (SigHandler *)SIG_IGN &&
      old_sigwinch != (SigHandler *)SIG_DFL)
    (*old_sigwinch) (sig);
#if !defined (VOID_SIGHANDLER)
  return (0);
#endif /* VOID_SIGHANDLER */
}
#endif  /* SIGWINCH */

#if defined (HANDLE_SIGNALS)
/* Interrupt handling. */
static SigHandler
  *old_int  = (SigHandler *)NULL,
  *old_alrm = (SigHandler *)NULL;
#if !defined (SHELL)
static SigHandler
  *old_tstp = (SigHandler *)NULL,
  *old_ttou = (SigHandler *)NULL,
  *old_ttin = (SigHandler *)NULL,
  *old_cont = (SigHandler *)NULL;
#endif /* !SHELL */

/* Handle an interrupt character. */
static sighandler
rl_signal_handler (sig)
     int sig;
{
#if defined (HAVE_POSIX_SIGNALS)
  sigset_t set;
#else /* !HAVE_POSIX_SIGNALS */
#  if defined (HAVE_BSD_SIGNALS)
  long omask;
#  endif /* HAVE_BSD_SIGNALS */
#endif /* !HAVE_POSIX_SIGNALS */

#if !defined (HAVE_BSD_SIGNALS) && !defined (HAVE_POSIX_SIGNALS)
  /* Since the signal will not be blocked while we are in the signal
     handler, ignore it until rl_clear_signals resets the catcher. */
  if (sig == SIGINT)
    signal (sig, SIG_IGN);
#endif /* !HAVE_BSD_SIGNALS */

  switch (sig)
    {
    case SIGINT:
      {
	register HIST_ENTRY *entry;

	free_undo_list ();

	entry = current_history ();
	if (entry)
	  entry->data = (char *)NULL;
      }
      _rl_kill_kbd_macro ();
      rl_clear_message ();
      rl_init_argument ();

#if defined (SIGTSTP)
    case SIGTSTP:
    case SIGTTOU:
    case SIGTTIN:
#endif /* SIGTSTP */
    case SIGALRM:
      rl_clean_up_for_exit ();
      rl_deprep_terminal ();
      rl_clear_signals ();
      rl_pending_input = 0;

#if defined (HAVE_POSIX_SIGNALS)
      sigprocmask (SIG_BLOCK, (sigset_t *)NULL, &set);
      sigdelset (&set, sig);
#else /* !HAVE_POSIX_SIGNALS */
#  if defined (HAVE_BSD_SIGNALS)
      omask = sigblock (0);
#  endif /* HAVE_BSD_SIGNALS */
#endif /* !HAVE_POSIX_SIGNALS */

      kill (getpid (), sig);

      /* Let the signal that we just sent through.  */
#if defined (HAVE_POSIX_SIGNALS)
      sigprocmask (SIG_SETMASK, &set, (sigset_t *)NULL);
#else /* !HAVE_POSIX_SIGNALS */
#  if defined (HAVE_BSD_SIGNALS)
      sigsetmask (omask & ~(sigmask (sig)));
#  endif /* HAVE_BSD_SIGNALS */
#endif /* !HAVE_POSIX_SIGNALS */

      rl_prep_terminal (_rl_meta_flag);
      rl_set_signals ();
    }

#if !defined (VOID_SIGHANDLER)
  return (0);
#endif /* !VOID_SIGHANDLER */
}

#if defined (HAVE_POSIX_SIGNALS)
static SigHandler *
rl_set_sighandler (sig, handler)
     int sig;
     SigHandler *handler;
{
  struct sigaction act, oact;

  act.sa_handler = handler;
  act.sa_flags = 0;
  sigemptyset (&act.sa_mask);
  sigemptyset (&oact.sa_mask);
  sigaction (sig, &act, &oact);
  return (oact.sa_handler);
}

#else /* !HAVE_POSIX_SIGNALS */
#  define rl_set_sighandler(sig, handler) (SigHandler *)signal (sig, handler)
#endif /* !HAVE_POSIX_SIGNALS */

rl_set_signals ()
{
  old_int = (SigHandler *)rl_set_sighandler (SIGINT, rl_signal_handler);
  if (old_int == (SigHandler *)SIG_IGN)
    signal (SIGINT, SIG_IGN);

  old_alrm = (SigHandler *)rl_set_sighandler (SIGALRM, rl_signal_handler);
  if (old_alrm == (SigHandler *)SIG_IGN)
    signal (SIGALRM, SIG_IGN);

#if !defined (SHELL)

#if defined (SIGTSTP)
  old_tstp = (SigHandler *)rl_set_sighandler (SIGTSTP, rl_signal_handler);
  if (old_tstp == (SigHandler *)SIG_IGN)
    signal (SIGTSTP, SIG_IGN);
#endif /* SIGTSTP */
#if defined (SIGTTOU)
  old_ttou = (SigHandler *)rl_set_sighandler (SIGTTOU, rl_signal_handler);
  old_ttin = (SigHandler *)rl_set_sighandler (SIGTTIN, rl_signal_handler);

  if (old_tstp == (SigHandler *)SIG_IGN)
    {
      signal (SIGTTOU, SIG_IGN);
      signal (SIGTTIN, SIG_IGN);
    }
#endif /* SIGTTOU */

#endif /* !SHELL */

#if defined (SIGWINCH)
  old_sigwinch =
    (SigHandler *) rl_set_sighandler (SIGWINCH, rl_handle_sigwinch);
#endif /* SIGWINCH */
  return 0;
}

rl_clear_signals ()
{
  rl_set_sighandler (SIGINT, old_int);
  rl_set_sighandler (SIGALRM, old_alrm);

#if !defined (SHELL)

#if defined (SIGTSTP)
  signal (SIGTSTP, old_tstp);
#endif

#if defined (SIGTTOU)
  signal (SIGTTOU, old_ttou);
  signal (SIGTTIN, old_ttin);
#endif /* SIGTTOU */

#endif /* !SHELL */

#if defined (SIGWINCH)
  signal (SIGWINCH, old_sigwinch);
#endif

  return 0;
}

/* Move to the start of the current line. */
static void
cr ()
{
  if (term_cr)	
    tputs (term_cr, 1, _rl_output_character_function);
}
#endif  /* HANDLE_SIGNALS */
