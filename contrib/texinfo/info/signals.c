/* signals.c -- install and maintain Info signal handlers.
   $Id: signals.c,v 1.4 2003/01/29 19:23:22 karl Exp $

   Copyright (C) 1993, 1994, 1995, 1998, 2002, 2003 Free Software
   Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

   Written by Brian Fox (bfox@ai.mit.edu). */

#include "info.h"
#include "signals.h"

/* **************************************************************** */
/*                                                                  */
/*              Pretending That We Have POSIX Signals               */
/*                                                                  */
/* **************************************************************** */

#if !defined (HAVE_SIGPROCMASK) && defined (HAVE_SIGSETMASK)
/* Perform OPERATION on NEWSET, perhaps leaving information in OLDSET. */
static void
sigprocmask (operation, newset, oldset)
     int operation, *newset, *oldset;
{
  switch (operation)
    {
    case SIG_UNBLOCK:
      sigsetmask (sigblock (0) & ~(*newset));
      break;

    case SIG_BLOCK:
      *oldset = sigblock (*newset);
      break;

    case SIG_SETMASK:
      sigsetmask (*newset);
      break;

    default:
      abort ();
    }
}
#endif /* !HAVE_SIGPROCMASK && HAVE_SIGSETMASK */

/* **************************************************************** */
/*                                                                  */
/*                  Signal Handling for Info                        */
/*                                                                  */
/* **************************************************************** */

#if defined (HAVE_SIGACTION) || defined (HAVE_SIGPROCMASK) ||\
  defined (HAVE_SIGSETMASK)
static void
mask_termsig (set)
  sigset_t *set;
{
# if defined (SIGTSTP)
  sigaddset (set, SIGTSTP);
  sigaddset (set, SIGTTOU);
  sigaddset (set, SIGTTIN);
# endif
# if defined (SIGWINCH)
  sigaddset (set, SIGWINCH);
# endif
#if defined (SIGINT)
  sigaddset (set, SIGINT);
#endif
# if defined (SIGUSR1)
  sigaddset (set, SIGUSR1);
# endif
}
#endif /* HAVE_SIGACTION || HAVE_SIGPROCMASK || HAVE_SIGSETMASK */

static RETSIGTYPE info_signal_proc ();
#if defined (HAVE_SIGACTION)
typedef struct sigaction signal_info;
signal_info info_signal_handler;

static void
set_termsig (sig, old)
  int sig;
  signal_info *old;
{
  sigaction (sig, &info_signal_handler, old);
}

static void
restore_termsig (sig, saved)
  int sig;
  const signal_info *saved;
{
  sigaction (sig, saved, NULL);
}
#else /* !HAVE_SIGACTION */
typedef RETSIGTYPE (*signal_info) ();
#define set_termsig(sig, old) (void)(*(old) = signal (sig, info_signal_proc))
#define restore_termsig(sig, saved) (void)signal (sig, *(saved))
#define info_signal_handler info_signal_proc
static int term_conf_busy = 0;
#endif /* !HAVE_SIGACTION */

static signal_info old_TSTP, old_TTOU, old_TTIN;
static signal_info old_WINCH, old_INT, old_USR1;

void
initialize_info_signal_handler ()
{
#if defined (HAVE_SIGACTION)
  info_signal_handler.sa_handler = info_signal_proc;
  info_signal_handler.sa_flags = 0;
  mask_termsig (&info_signal_handler.sa_mask);
#endif /* HAVE_SIGACTION */

#if defined (SIGTSTP)
  set_termsig (SIGTSTP, &old_TSTP);
  set_termsig (SIGTTOU, &old_TTOU);
  set_termsig (SIGTTIN, &old_TTIN);
#endif /* SIGTSTP */

#if defined (SIGWINCH)
  set_termsig (SIGWINCH, &old_WINCH);
#endif

#if defined (SIGINT)
  set_termsig (SIGINT, &old_INT);
#endif

#if defined (SIGUSR1)
  /* Used by DJGPP to simulate SIGTSTP on Ctrl-Z.  */
  set_termsig (SIGUSR1, &old_USR1);
#endif
}

static void
redisplay_after_signal ()
{
  terminal_clear_screen ();
  display_clear_display (the_display);
  window_mark_chain (windows, W_UpdateWindow);
  display_update_display (windows);
  display_cursor_at_point (active_window);
  fflush (stdout);
}

static void
reset_info_window_sizes ()
{
  terminal_goto_xy (0, 0);
  fflush (stdout);
  terminal_unprep_terminal ();
  terminal_get_screen_size ();
  terminal_prep_terminal ();
  display_initialize_display (screenwidth, screenheight);
  window_new_screen_size (screenwidth, screenheight, NULL);
  redisplay_after_signal ();
}

static RETSIGTYPE
info_signal_proc (sig)
     int sig;
{
  signal_info *old_signal_handler;

#if !defined (HAVE_SIGACTION)
  /* best effort: first increment this counter and later block signals */
  if (term_conf_busy)
    return;
  term_conf_busy++;
#if defined (HAVE_SIGPROCMASK) || defined (HAVE_SIGSETMASK)
    {
      sigset_t nvar, ovar;
      sigemptyset (&nvar);
      mask_termsig (&nvar);
      sigprocmask (SIG_BLOCK, &nvar, &ovar);
    }
#endif /* HAVE_SIGPROCMASK || HAVE_SIGSETMASK */
#endif /* !HAVE_SIGACTION */
  switch (sig)
    {
#if defined (SIGTSTP)
    case SIGTSTP:
    case SIGTTOU:
    case SIGTTIN:
#endif
#if defined (SIGINT)
    case SIGINT:
#endif
      {
#if defined (SIGTSTP)
        if (sig == SIGTSTP)
          old_signal_handler = &old_TSTP;
        if (sig == SIGTTOU)
          old_signal_handler = &old_TTOU;
        if (sig == SIGTTIN)
          old_signal_handler = &old_TTIN;
#endif /* SIGTSTP */
#if defined (SIGINT)
        if (sig == SIGINT)
          old_signal_handler = &old_INT;
#endif /* SIGINT */

        /* For stop signals, restore the terminal IO, leave the cursor
           at the bottom of the window, and stop us. */
        terminal_goto_xy (0, screenheight - 1);
        terminal_clear_to_eol ();
        fflush (stdout);
        terminal_unprep_terminal ();
	restore_termsig (sig, old_signal_handler);
	UNBLOCK_SIGNAL (sig);
	kill (getpid (), sig);

        /* The program is returning now.  Restore our signal handler,
           turn on terminal handling, redraw the screen, and place the
           cursor where it belongs. */
        terminal_prep_terminal ();
	set_termsig (sig, old_signal_handler);
	/* window size might be changed while sleeping */
	reset_info_window_sizes ();
      }
      break;

#if defined (SIGWINCH) || defined (SIGUSR1)
#ifdef SIGWINCH
    case SIGWINCH:
#endif
#ifdef SIGUSR1
    case SIGUSR1:
#endif
      {
	/* Turn off terminal IO, tell our parent that the window has changed,
	   then reinitialize the terminal and rebuild our windows. */
#ifdef SIGWINCH
	if (sig == SIGWINCH)
	  old_signal_handler = &old_WINCH;
#endif
#ifdef SIGUSR1
	if (sig == SIGUSR1)
	  old_signal_handler = &old_USR1;
#endif
	terminal_goto_xy (0, 0);
	fflush (stdout);
	terminal_unprep_terminal (); /* needless? */
	restore_termsig (sig, old_signal_handler);
	UNBLOCK_SIGNAL (sig);
	kill (getpid (), sig);

	/* After our old signal handler returns... */
	set_termsig (sig, old_signal_handler); /* needless? */
	terminal_prep_terminal ();
	reset_info_window_sizes ();
      }
      break;
#endif /* SIGWINCH || SIGUSR1 */
    }
#if !defined (HAVE_SIGACTION)
  /* at this time it is safer to perform unblock after decrement */
  term_conf_busy--;
#if defined (HAVE_SIGPROCMASK) || defined (HAVE_SIGSETMASK)
    {
      sigset_t nvar, ovar;
      sigemptyset (&nvar);
      mask_termsig (&nvar);
      sigprocmask (SIG_UNBLOCK, &nvar, &ovar);
    }
#endif /* HAVE_SIGPROCMASK || HAVE_SIGSETMASK */
#endif /* !HAVE_SIGACTION */
}
/* vim: set sw=2 cino={1s>2sn-s^-se-s: */
