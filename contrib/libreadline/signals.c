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
#define READLINE_LIBRARY

#if defined (HAVE_CONFIG_H)
#  include <config.h>
#endif

#include <stdio.h>		/* Just for NULL.  Yuck. */
#include <sys/types.h>
#include <signal.h>

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif /* HAVE_UNISTD_H */

/* System-specific feature definitions and include files. */
#include "rldefs.h"

#if defined (GWINSZ_IN_SYS_IOCTL)
#  include <sys/ioctl.h>
#endif /* GWINSZ_IN_SYS_IOCTL */

#if defined (__GO32__)
#  undef HANDLE_SIGNALS
#endif /* __GO32__ */

#if defined (HANDLE_SIGNALS)
/* Some standard library routines. */
#include "readline.h"
#include "history.h"

extern int readline_echoing_p;
extern int rl_pending_input;
extern int _rl_meta_flag;

extern void free_undo_list ();
extern void _rl_get_screen_size ();
extern void _rl_redisplay_after_sigwinch ();
extern void _rl_clean_up_for_exit ();
extern void _rl_kill_kbd_macro ();
extern void _rl_init_argument ();
extern void rl_deprep_terminal (), rl_prep_terminal ();

#if !defined (RETSIGTYPE)
#  if defined (VOID_SIGHANDLER)
#    define RETSIGTYPE void
#  else
#    define RETSIGTYPE int
#  endif /* !VOID_SIGHANDLER */
#endif /* !RETSIGTYPE */

#if defined (VOID_SIGHANDLER)
#  define SIGHANDLER_RETURN return
#else
#  define SIGHANDLER_RETURN return (0)
#endif

/* This typedef is equivalant to the one for Function; it allows us
   to say SigHandler *foo = signal (SIGKILL, SIG_IGN); */
typedef RETSIGTYPE SigHandler ();

static SigHandler *rl_set_sighandler ();

/* **************************************************************** */
/*					        		    */
/*			   Signal Handling                          */
/*								    */
/* **************************************************************** */

/* If we're not being compiled as part of bash, initialize handlers for
   and catch the job control signals (SIGTTIN, SIGTTOU, SIGTSTP) and
   SIGTERM. */
#if !defined (SHELL)
#  define HANDLE_JOB_SIGNALS
#  define HANDLE_SIGTERM
#endif /* !SHELL */

#if defined (HAVE_POSIX_SIGNALS)
typedef struct sigaction sighandler_cxt;
#  define rl_sigaction(s, nh, oh)	sigaction(s, nh, oh)
#else
typedef struct { SigHandler *sa_handler; } sighandler_cxt;
#  define sigemptyset(m)
#endif /* !HAVE_POSIX_SIGNALS */

static sighandler_cxt old_int, old_alrm;

#if defined (HANDLE_JOB_SIGNALS)
static sighandler_cxt old_tstp, old_ttou, old_ttin;
#endif /* HANDLE_JOB_SIGNALS */

#if defined (HANDLE_SIGTERM)
static sighandler_cxt old_term;
#endif

#if defined (SIGWINCH)
static sighandler_cxt old_winch;
#endif

/* Readline signal handler functions. */

static RETSIGTYPE
rl_signal_handler (sig)
     int sig;
{
#if defined (HAVE_POSIX_SIGNALS)
  sigset_t set;
#else /* !HAVE_POSIX_SIGNALS */
#  if defined (HAVE_BSD_SIGNALS)
  long omask;
#  else /* !HAVE_BSD_SIGNALS */
  sighandler_cxt dummy_cxt;	/* needed for rl_set_sighandler call */
#  endif /* !HAVE_BSD_SIGNALS */
#endif /* !HAVE_POSIX_SIGNALS */

#if !defined (HAVE_BSD_SIGNALS) && !defined (HAVE_POSIX_SIGNALS)
  /* Since the signal will not be blocked while we are in the signal
     handler, ignore it until rl_clear_signals resets the catcher. */
  if (sig == SIGINT || sig == SIGALRM)
    rl_set_sighandler (sig, SIG_IGN, &dummy_cxt);
#endif /* !HAVE_BSD_SIGNALS && !HAVE_POSIX_SIGNALS */

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
      _rl_init_argument ();

#if defined (SIGTSTP)
    case SIGTSTP:
    case SIGTTOU:
    case SIGTTIN:
#endif /* SIGTSTP */
    case SIGALRM:
    case SIGTERM:
      _rl_clean_up_for_exit ();
      (*rl_deprep_term_function) ();
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

      (*rl_prep_term_function) (_rl_meta_flag);
      rl_set_signals ();
    }

  SIGHANDLER_RETURN;
}

#if defined (SIGWINCH)
static RETSIGTYPE
rl_handle_sigwinch (sig)
     int sig;
{
  SigHandler *oh;

#if defined (MUST_REINSTALL_SIGHANDLERS)
  sighandler_cxt dummy_winch;

  /* We don't want to change old_winch -- it holds the state of SIGWINCH
     disposition set by the calling application.  We need this state
     because we call the application's SIGWINCH handler after updating
     our own idea of the screen size. */
  rl_set_sighandler (SIGWINCH, rl_handle_sigwinch, &dummy_winch);
#endif

  if (readline_echoing_p)
    {
      _rl_get_screen_size (fileno (rl_instream), 1);
      _rl_redisplay_after_sigwinch ();
    }

  /* If another sigwinch handler has been installed, call it. */
  oh = (SigHandler *)old_winch.sa_handler;
  if (oh &&  oh != (SigHandler *)SIG_IGN && oh != (SigHandler *)SIG_DFL)
    (*oh) (sig);

  SIGHANDLER_RETURN;
}
#endif  /* SIGWINCH */

/* Functions to manage signal handling. */

#if !defined (HAVE_POSIX_SIGNALS)
static int
rl_sigaction (sig, nh, oh)
     int sig;
     sighandler_cxt *nh, *oh;
{
  oh->sa_handler = signal (sig, nh->sa_handler);
  return 0;
}
#endif /* !HAVE_POSIX_SIGNALS */

/* Set up a readline-specific signal handler, saving the old signal
   information in OHANDLER.  Return the old signal handler, like
   signal(). */
static SigHandler *
rl_set_sighandler (sig, handler, ohandler)
     int sig;
     SigHandler *handler;
     sighandler_cxt *ohandler;
{
#if defined (HAVE_POSIX_SIGNALS)
  struct sigaction act;

  act.sa_handler = handler;
  act.sa_flags = 0;
  sigemptyset (&act.sa_mask);
  sigemptyset (&ohandler->sa_mask);
  sigaction (sig, &act, ohandler);
#else
  ohandler->sa_handler = (SigHandler *)signal (sig, handler);
#endif /* !HAVE_POSIX_SIGNALS */
  return (ohandler->sa_handler);
}

int
rl_set_signals ()
{
  sighandler_cxt dummy;
  SigHandler *oh;

#if defined (HAVE_POSIX_SIGNALS)
  sigemptyset (&dummy.sa_mask);
#endif

  oh = rl_set_sighandler (SIGINT, rl_signal_handler, &old_int);
  if (oh == (SigHandler *)SIG_IGN)
    rl_sigaction (SIGINT, &old_int, &dummy);

  oh = rl_set_sighandler (SIGALRM, rl_signal_handler, &old_alrm);
  if (oh == (SigHandler *)SIG_IGN)
    rl_sigaction (SIGALRM, &old_alrm, &dummy);
#if defined (HAVE_POSIX_SIGNALS) && defined (SA_RESTART)
  /* If the application using readline has already installed a signal
     handler with SA_RESTART, SIGALRM will cause reads to be restarted
     automatically, so readline should just get out of the way.  Since
     we tested for SIG_IGN above, we can just test for SIG_DFL here. */
  if (oh != (SigHandler *)SIG_DFL && (old_alrm.sa_flags & SA_RESTART))
    rl_sigaction (SIGALRM, &old_alrm, &dummy);
#endif /* HAVE_POSIX_SIGNALS */

#if defined (HANDLE_JOB_SIGNALS)

#if defined (SIGTSTP)
  oh = rl_set_sighandler (SIGTSTP, rl_signal_handler, &old_tstp);
  if (oh == (SigHandler *)SIG_IGN)
    rl_sigaction (SIGTSTP, &old_tstp, &dummy);
#else
  oh = (SigHandler *)NULL;
#endif /* SIGTSTP */

#if defined (SIGTTOU)
  rl_set_sighandler (SIGTTOU, rl_signal_handler, &old_ttou);
  rl_set_sighandler (SIGTTIN, rl_signal_handler, &old_ttin);

  if (oh == (SigHandler *)SIG_IGN)
    {
      rl_set_sighandler (SIGTTOU, SIG_IGN, &dummy);
      rl_set_sighandler (SIGTTIN, SIG_IGN, &dummy);
    }
#endif /* SIGTTOU */

#endif /* HANDLE_JOB_SIGNALS */

#if defined (HANDLE_SIGTERM)
  /* Handle SIGTERM if we're not being compiled as part of bash. */
  rl_set_sighandler (SIGTERM, rl_signal_handler, &old_term);
#endif /* HANDLE_SIGTERM */

#if defined (SIGWINCH)
  rl_set_sighandler (SIGWINCH, rl_handle_sigwinch, &old_winch);
#endif /* SIGWINCH */

  return 0;
}

int
rl_clear_signals ()
{
  sighandler_cxt dummy;

#if defined (HAVE_POSIX_SIGNALS)
  sigemptyset (&dummy.sa_mask);
#endif

  rl_sigaction (SIGINT, &old_int, &dummy);
  rl_sigaction (SIGALRM, &old_alrm, &dummy);

#if defined (HANDLE_JOB_SIGNALS)

#if defined (SIGTSTP)
  rl_sigaction (SIGTSTP, &old_tstp, &dummy);
#endif

#if defined (SIGTTOU)
  rl_sigaction (SIGTTOU, &old_ttou, &dummy);
  rl_sigaction (SIGTTIN, &old_ttin, &dummy);
#endif /* SIGTTOU */

#endif /* HANDLE_JOB_SIGNALS */

#if defined (HANDLE_SIGTERM)
  rl_sigaction (SIGTERM, &old_term, &dummy);
#endif /* HANDLE_SIGTERM */

#if defined (SIGWINCH)
  sigemptyset (&dummy.sa_mask);
  rl_sigaction (SIGWINCH, &old_winch, &dummy);
#endif

  return 0;
}
#endif  /* HANDLE_SIGNALS */
