/* signals.c -- install and maintain Info signal handlers.
   $Id: signals.c,v 1.6 1998/12/06 22:00:04 karl Exp $

   Copyright (C) 1993, 94, 95, 98 Free Software Foundation, Inc.

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

/* Non-zero when our signal handler has been called to handle SIGWINCH. */
static int in_sigwinch = 0;

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

typedef RETSIGTYPE signal_handler ();

static RETSIGTYPE info_signal_handler ();
static signal_handler *old_TSTP, *old_TTOU, *old_TTIN;
static signal_handler *old_WINCH, *old_INT, *old_USR1;

void
initialize_info_signal_handler ()
{
#if defined (SIGTSTP)
  old_TSTP = (signal_handler *) signal (SIGTSTP, info_signal_handler);
  old_TTOU = (signal_handler *) signal (SIGTTOU, info_signal_handler);
  old_TTIN = (signal_handler *) signal (SIGTTIN, info_signal_handler);
#endif /* SIGTSTP */

#if defined (SIGWINCH)
  old_WINCH = (signal_handler *) signal (SIGWINCH, info_signal_handler);
#endif

#if defined (SIGINT)
  old_INT = (signal_handler *) signal (SIGINT, info_signal_handler);
#endif

#if defined (SIGUSR1)
  /* Used by DJGPP to simulate SIGTSTP on Ctrl-Z.  */
  old_USR1 = (signal_handler *) signal (SIGUSR1, info_signal_handler);
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
info_signal_handler (sig)
     int sig;
{
  signal_handler **old_signal_handler;

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
        if (sig == SIGINT)
          old_signal_handler = &old_INT;

        /* For stop signals, restore the terminal IO, leave the cursor
           at the bottom of the window, and stop us. */
        terminal_goto_xy (0, screenheight - 1);
        terminal_clear_to_eol ();
        fflush (stdout);
        terminal_unprep_terminal ();
        signal (sig, *old_signal_handler);
        UNBLOCK_SIGNAL (sig);
        kill (getpid (), sig);

        /* The program is returning now.  Restore our signal handler,
           turn on terminal handling, redraw the screen, and place the
           cursor where it belongs. */
        terminal_prep_terminal ();
        *old_signal_handler = (signal_handler *) signal (sig, info_signal_handler);
        redisplay_after_signal ();
        fflush (stdout);
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
        if (!in_sigwinch) {
          in_sigwinch++;
          
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
          terminal_unprep_terminal ();
          signal (sig, *old_signal_handler);
          UNBLOCK_SIGNAL (sig);
          kill (getpid (), sig);

          /* After our old signal handler returns... */
          *old_signal_handler
            = (signal_handler *) signal (sig, info_signal_handler);
          terminal_prep_terminal ();
          reset_info_window_sizes ();
          in_sigwinch--;
        }
      }
      break;
#endif /* SIGWINCH || SIGUSR1 */
    }
}
