/* rltty.c -- functions to prepare and restore the terminal for readline's
   use. */

/* Copyright (C) 1992 Free Software Foundation, Inc.

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
#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif /* HAVE_UNISTD_H */

/* This is needed to include support for TIOCGWINSZ and window resizing. */
#if defined (OSF1) || defined (BSD386) || defined (NetBSD) || \
    defined (FreeBSD) || defined (_386BSD) || defined (AIX)
#  include <sys/ioctl.h>
#endif /* OSF1 || BSD386 */

#include "rldefs.h"
#include <readline/readline.h>

#if !defined (errno)
extern int errno;
#endif /* !errno */

extern int readline_echoing_p;
extern int _rl_eof_char;

#if defined (__GO32__)
#  include <sys/pc.h>
#  undef HANDLE_SIGNALS
#endif /* __GO32__ */

/* **************************************************************** */
/*								    */
/*			   Signal Management			    */
/*								    */
/* **************************************************************** */

#if defined (HAVE_POSIX_SIGNALS)
static sigset_t sigint_set, sigint_oset;
#else /* !HAVE_POSIX_SIGNALS */
#  if defined (HAVE_BSD_SIGNALS)
static int sigint_oldmask;
#  endif /* HAVE_BSD_SIGNALS */
#endif /* !HAVE_POSIX_SIGNALS */

static int sigint_blocked = 0;

/* Cause SIGINT to not be delivered until the corresponding call to
   release_sigint(). */
static void
block_sigint ()
{
  if (sigint_blocked)
    return;

#if defined (HAVE_POSIX_SIGNALS)
  sigemptyset (&sigint_set);
  sigemptyset (&sigint_oset);
  sigaddset (&sigint_set, SIGINT);
  sigprocmask (SIG_BLOCK, &sigint_set, &sigint_oset);
#else /* !HAVE_POSIX_SIGNALS */
#  if defined (HAVE_BSD_SIGNALS)
  sigint_oldmask = sigblock (sigmask (SIGINT));
#  else /* !HAVE_BSD_SIGNALS */
#    if defined (HAVE_USG_SIGHOLD)
  sighold (SIGINT);
#    endif /* HAVE_USG_SIGHOLD */
#  endif /* !HAVE_BSD_SIGNALS */
#endif /* !HAVE_POSIX_SIGNALS */
  sigint_blocked = 1;
}

/* Allow SIGINT to be delivered. */
static void
release_sigint ()
{
  if (!sigint_blocked)
    return;

#if defined (HAVE_POSIX_SIGNALS)
  sigprocmask (SIG_SETMASK, &sigint_oset, (sigset_t *)NULL);
#else
#  if defined (HAVE_BSD_SIGNALS)
  sigsetmask (sigint_oldmask);
#  else /* !HAVE_BSD_SIGNALS */
#    if defined (HAVE_USG_SIGHOLD)
  sigrelse (SIGINT);
#    endif /* HAVE_USG_SIGHOLD */
#  endif /* !HAVE_BSD_SIGNALS */
#endif /* !HAVE_POSIX_SIGNALS */

  sigint_blocked = 0;
}

/* **************************************************************** */
/*								    */
/*	 	Controlling the Meta Key and Keypad		    */
/*								    */
/* **************************************************************** */

extern int term_has_meta;
extern char *term_mm;
extern char *term_mo;

extern char *term_ks;
extern char *term_ke;
static int
outchar (c)
     int c;
{
  return putc (c, rl_outstream);
}

/* Turn on/off the meta key depending on ON. */
static void
control_meta_key (on)
     int on;
{
  if (term_has_meta)
    {
      if (on && term_mm)
	tputs (term_mm, 1, outchar);
      else if (!on && term_mo)
	tputs (term_mo, 1, outchar);
    }
}

static void
control_keypad (on)
     int on;
{
  if (on && term_ks)
    tputs (term_ks, 1, outchar);
  else if (!on && term_ke)
    tputs (term_ke, 1, outchar);
}

/* **************************************************************** */
/*								    */
/*		      Saving and Restoring the TTY	    	    */
/*								    */
/* **************************************************************** */

/* Non-zero means that the terminal is in a prepped state. */
static int terminal_prepped = 0;

/* If non-zero, means that this process has called tcflow(fd, TCOOFF)
   and output is suspended. */
#if defined (__ksr1__)
static int ksrflow = 0;
#endif
#if defined (NEW_TTY_DRIVER)

/* Values for the `flags' field of a struct bsdtty.  This tells which
   elements of the struct bsdtty have been fetched from the system and
   are valid. */
#define SGTTY_SET	0x01
#define LFLAG_SET	0x02
#define TCHARS_SET	0x04
#define LTCHARS_SET	0x08

struct bsdtty {
  struct sgttyb sgttyb;	/* Basic BSD tty driver information. */
  int lflag;		/* Local mode flags, like LPASS8. */
#if defined (TIOCGETC)
  struct tchars tchars;	/* Terminal special characters, including ^S and ^Q. */
#endif
#if defined (TIOCGLTC)
  struct ltchars ltchars; /* 4.2 BSD editing characters */
#endif
  int flags;		/* Bitmap saying which parts of the struct are valid. */
};

#define TIOTYPE struct bsdtty

static TIOTYPE otio;

static int
get_tty_settings (tty, tiop)
     int tty;
     TIOTYPE *tiop;
{
  tiop->flags = tiop->lflag = 0;

  ioctl (tty, TIOCGETP, &(tiop->sgttyb));
  tiop->flags |= SGTTY_SET;

#if defined (TIOCLGET)
  ioctl (tty, TIOCLGET, &(tiop->lflag));
  tiop->flags |= LFLAG_SET;
#endif

#if defined (TIOCGETC)
  ioctl (tty, TIOCGETC, &(tiop->tchars));
  tiop->flags |= TCHARS_SET;
#endif

#if defined (TIOCGLTC)
  ioctl (tty, TIOCGLTC, &(tiop->ltchars));
  tiop->flags |= LTCHARS_SET;
#endif

  return 0;
}

set_tty_settings (tty, tiop)
     int tty;
     TIOTYPE *tiop;
{
  if (tiop->flags & SGTTY_SET)
    {
      ioctl (tty, TIOCSETN, &(tiop->sgttyb));
      tiop->flags &= ~SGTTY_SET;
    }
  readline_echoing_p = 1;

#if defined (TIOCLSET)
  if (tiop->flags & LFLAG_SET)
    {
      ioctl (tty, TIOCLSET, &(tiop->lflag));
      tiop->flags &= ~LFLAG_SET;
    }
#endif

#if defined (TIOCSETC)
  if (tiop->flags & TCHARS_SET)
    {
      ioctl (tty, TIOCSETC, &(tiop->tchars));
      tiop->flags &= ~TCHARS_SET;
    }
#endif

#if defined (TIOCSLTC)
  if (tiop->flags & LTCHARS_SET)
    {
      ioctl (tty, TIOCSLTC, &(tiop->ltchars));
      tiop->flags &= ~LTCHARS_SET;
    }
#endif

  return 0;
}

static void
prepare_terminal_settings (meta_flag, otio, tiop)
     int meta_flag;
     TIOTYPE otio, *tiop;
{
#if !defined (__GO32__)
  readline_echoing_p = (otio.sgttyb.sg_flags & ECHO);

  /* Copy the original settings to the structure we're going to use for
     our settings. */
  tiop->sgttyb = otio.sgttyb;
  tiop->lflag = otio.lflag;
#if defined (TIOCGETC)
  tiop->tchars = otio.tchars;
#endif
#if defined (TIOCGLTC)
  tiop->ltchars = otio.ltchars;
#endif
  tiop->flags = otio.flags;

  /* First, the basic settings to put us into character-at-a-time, no-echo
     input mode. */
  tiop->sgttyb.sg_flags &= ~(ECHO | CRMOD);
  tiop->sgttyb.sg_flags |= CBREAK;

  /* If this terminal doesn't care how the 8th bit is used, then we can
     use it for the meta-key.  If only one of even or odd parity is
     specified, then the terminal is using parity, and we cannot. */
#if !defined (ANYP)
#  define ANYP (EVENP | ODDP)
#endif
  if (((otio.sgttyb.sg_flags & ANYP) == ANYP) ||
      ((otio.sgttyb.sg_flags & ANYP) == 0))
    {
      tiop->sgttyb.sg_flags |= ANYP;

      /* Hack on local mode flags if we can. */
#if defined (TIOCLGET)
#  if defined (LPASS8)
      tiop->lflag |= LPASS8;
#  endif /* LPASS8 */
#endif /* TIOCLGET */
    }

#if defined (TIOCGETC)
#  if defined (USE_XON_XOFF)
  /* Get rid of terminal output start and stop characters. */
  tiop->tchars.t_stopc = -1; /* C-s */
  tiop->tchars.t_startc = -1; /* C-q */

  /* If there is an XON character, bind it to restart the output. */
  if (otio.tchars.t_startc != -1)
    rl_bind_key (otio.tchars.t_startc, rl_restart_output);
#  endif /* USE_XON_XOFF */

  /* If there is an EOF char, bind _rl_eof_char to it. */
  if (otio.tchars.t_eofc != -1)
    _rl_eof_char = otio.tchars.t_eofc;

#  if defined (NO_KILL_INTR)
  /* Get rid of terminal-generated SIGQUIT and SIGINT. */
  tiop->tchars.t_quitc = -1; /* C-\ */
  tiop->tchars.t_intrc = -1; /* C-c */
#  endif /* NO_KILL_INTR */
#endif /* TIOCGETC */

#if defined (TIOCGLTC)
  /* Make the interrupt keys go away.  Just enough to make people happy. */
  tiop->ltchars.t_dsuspc = -1;	/* C-y */
  tiop->ltchars.t_lnextc = -1;	/* C-v */
#endif /* TIOCGLTC */
#endif /* !__GO32__ */
}

#else  /* !defined (NEW_TTY_DRIVER) */

#if !defined (VMIN)
#  define VMIN VEOF
#endif

#if !defined (VTIME)
#  define VTIME VEOL
#endif

#if defined (TERMIOS_TTY_DRIVER)
#  define TIOTYPE struct termios
#  define DRAIN_OUTPUT(fd)	tcdrain (fd)
#  define GETATTR(tty, tiop)	(tcgetattr (tty, tiop))
#  define SETATTR(tty, tiop)	(tcsetattr (tty, TCSANOW, tiop))
#else
#  define TIOTYPE struct termio
#  define DRAIN_OUTPUT(fd)
#  define GETATTR(tty, tiop)	(ioctl (tty, TCGETA, tiop))
#  define SETATTR(tty, tiop)	(ioctl (tty, TCSETA, tiop))
#endif /* !TERMIOS_TTY_DRIVER */

static TIOTYPE otio;

static int
get_tty_settings (tty, tiop)
     int tty;
     TIOTYPE *tiop;
{
#ifdef TIOCGWINSZ
/* XXX this prevents to got editing mode from tcsh. Ache */
  struct winsize w;

  if (ioctl (tty, TIOCGWINSZ, &w) == 0)
      (void) ioctl (tty, TIOCSWINSZ, &w);
#endif

  while (GETATTR (tty, tiop) < 0)
    {
      if (errno != EINTR)
	return -1;
      errno = 0;
    }
  return 0;
}

static int
set_tty_settings (tty, tiop)
     int tty;
     TIOTYPE *tiop;
{
  while (SETATTR (tty, tiop) < 0)
    {
      if (errno != EINTR)
	return -1;
      errno = 0;
    }

#if 0

#if defined (TERMIOS_TTY_DRIVER)
#  if defined (__ksr1__)
  if (ksrflow)
    {
      ksrflow = 0;
      tcflow (tty, TCOON);
    }
#  else /* !ksr1 */
  tcflow (tty, TCOON);		/* Simulate a ^Q. */
#  endif /* !ksr1 */
#else
  ioctl (tty, TCXONC, 1);	/* Simulate a ^Q. */
#endif /* !TERMIOS_TTY_DRIVER */

#endif

  return 0;
}

static void
prepare_terminal_settings (meta_flag, otio, tiop)
     int meta_flag;
     TIOTYPE otio, *tiop;
{
  readline_echoing_p = (otio.c_lflag & ECHO);

  tiop->c_lflag &= ~(ICANON | ECHO);

  if ((unsigned char) otio.c_cc[VEOF] != (unsigned char) _POSIX_VDISABLE)
    _rl_eof_char = otio.c_cc[VEOF];

#if defined (USE_XON_XOFF)
#if defined (IXANY)
  tiop->c_iflag &= ~(IXON | IXOFF | IXANY);
#else
  /* `strict' Posix systems do not define IXANY. */
  tiop->c_iflag &= ~(IXON | IXOFF);
#endif /* IXANY */
#endif /* USE_XON_XOFF */

  /* Only turn this off if we are using all 8 bits. */
  if (((tiop->c_cflag & CSIZE) == CS8) || meta_flag)
    tiop->c_iflag &= ~(ISTRIP | INPCK);

  /* Make sure we differentiate between CR and NL on input. */
  tiop->c_iflag &= ~(ICRNL | INLCR);

#if !defined (HANDLE_SIGNALS)
  tiop->c_lflag &= ~ISIG;
#else
  tiop->c_lflag |= ISIG;
#endif

  tiop->c_cc[VMIN] = 1;
  tiop->c_cc[VTIME] = 0;

  /* Turn off characters that we need on Posix systems with job control,
     just to be sure.  This includes ^Y and ^V.  This should not really
     be necessary.  */
#if defined (TERMIOS_TTY_DRIVER) && defined (_POSIX_VDISABLE)

#if defined (VLNEXT)
  tiop->c_cc[VLNEXT] = _POSIX_VDISABLE;
#endif

#if defined (VDSUSP)
  tiop->c_cc[VDSUSP] = _POSIX_VDISABLE;
#endif

#endif /* TERMIOS_TTY_DRIVER && _POSIX_VDISABLE */
}
#endif  /* NEW_TTY_DRIVER */

/* Put the terminal in CBREAK mode so that we can detect key presses. */
void
rl_prep_terminal (meta_flag)
     int meta_flag;
{
#if !defined (__GO32__)
  int tty = fileno (rl_instream);
  TIOTYPE tio;

  if (terminal_prepped)
    return;

  /* Try to keep this function from being INTerrupted. */
  block_sigint ();

  if (get_tty_settings (tty, &tio) < 0)
    {
      release_sigint ();
      return;
    }

  otio = tio;

  prepare_terminal_settings (meta_flag, otio, &tio);

  if (set_tty_settings (tty, &tio) < 0)
    {
      release_sigint ();
      return;
    }

  control_meta_key (1);
  control_keypad (1);
  terminal_prepped = 1;

  release_sigint ();
#endif /* !__GO32__ */
}

/* Restore the terminal's normal settings and modes. */
void
rl_deprep_terminal ()
{
#if !defined (__GO32__)
  int tty = fileno (rl_instream);

  if (!terminal_prepped)
    return;

  /* Try to keep this function from being INTerrupted. */
  block_sigint ();

  if (set_tty_settings (tty, &otio) < 0)
    {
      release_sigint ();
      return;
    }

  control_meta_key (0);
  control_keypad (0);
  terminal_prepped = 0;

  release_sigint ();
#endif /* !__GO32__ */
}

/* **************************************************************** */
/*								    */
/*			Bogus Flow Control      		    */
/*								    */
/* **************************************************************** */

rl_restart_output (count, key)
     int count, key;
{
  int fildes = fileno (rl_outstream);
#if defined (TIOCSTART)
#if defined (apollo)
  ioctl (&fildes, TIOCSTART, 0);
#else
  ioctl (fildes, TIOCSTART, 0);
#endif /* apollo */

#else /* !TIOCSTART */
#  if defined (TERMIOS_TTY_DRIVER)
#    if defined (__ksr1__)
  if (ksrflow)
    {
      ksrflow = 0;
      tcflow (fildes, TCOON);
    }
#    else /* !ksr1 */
  tcflow (fildes, TCOON);		/* Simulate a ^Q. */
#    endif /* !ksr1 */
#  else /* !TERMIOS_TTY_DRIVER */
#    if defined (TCXONC)
  ioctl (fildes, TCXONC, TCOON);
#    endif /* TCXONC */
#  endif /* !TERMIOS_TTY_DRIVER */
#endif /* !TIOCSTART */

  return 0;
}

rl_stop_output (count, key)
     int count, key;
{
  int fildes = fileno (rl_instream);

#if defined (TIOCSTOP)
# if defined (apollo)
  ioctl (&fildes, TIOCSTOP, 0);
# else
  ioctl (fildes, TIOCSTOP, 0);
# endif /* apollo */
#else /* !TIOCSTOP */
# if defined (TERMIOS_TTY_DRIVER)
#  if defined (__ksr1__)
  ksrflow = 1;
#  endif /* ksr1 */
  tcflow (fildes, TCOOFF);
# else
#   if defined (TCXONC)
  ioctl (fildes, TCXONC, TCOON);
#   endif /* TCXONC */
# endif /* !TERMIOS_TTY_DRIVER */
#endif /* !TIOCSTOP */

  return 0;
}

/* **************************************************************** */
/*								    */
/*			Default Key Bindings			    */
/*								    */
/* **************************************************************** */
void
rltty_set_default_bindings (kmap)
     Keymap kmap;
{
  TIOTYPE ttybuff;
  int tty = fileno (rl_instream);

#if defined (NEW_TTY_DRIVER)

  if (get_tty_settings (tty, &ttybuff) == 0)
    {
      if (ttybuff.flags & SGTTY_SET)
	{
	  int erase, kill;

	  erase = ttybuff.sgttyb.sg_erase;
	  kill  = ttybuff.sgttyb.sg_kill;

	  if (erase != -1 && kmap[erase].type == ISFUNC)
	    kmap[erase].function = rl_rubout;

	  if (kill != -1 && kmap[kill].type == ISFUNC)
	    kmap[kill].function = rl_unix_line_discard;
	}

#  if defined (TIOCGLTC)

      if (ttybuff.flags & LTCHARS_SET)
	{
	  int werase, nextc;

	  werase = ttybuff.ltchars.t_werasc;
	  nextc = ttybuff.ltchars.t_lnextc;

	  if (werase != -1 && kmap[werase].type == ISFUNC)
	    kmap[werase].function = rl_unix_word_rubout;

	  if (nextc != -1 && kmap[nextc].type == ISFUNC)
	    kmap[nextc].function = rl_quoted_insert;
	}
    }
#  endif /* TIOCGLTC */

#else /* !NEW_TTY_DRIVER */

  if (get_tty_settings (tty, &ttybuff) == 0)
    {
      unsigned char erase, kill;

      erase = ttybuff.c_cc[VERASE];
      kill = ttybuff.c_cc[VKILL];

      if (erase != (unsigned char)_POSIX_VDISABLE &&
	  kmap[erase].type == ISFUNC)
	kmap[erase].function = rl_rubout;

      if (kill != (unsigned char)_POSIX_VDISABLE &&
	  kmap[kill].type == ISFUNC)
	kmap[kill].function = rl_unix_line_discard;

#  if defined (VLNEXT) && defined (TERMIOS_TTY_DRIVER)
      {
	unsigned char nextc;

	nextc = ttybuff.c_cc[VLNEXT];

	if (nextc != (unsigned char)_POSIX_VDISABLE &&
	    kmap[nextc].type == ISFUNC)
	  kmap[nextc].function = rl_quoted_insert;
      }
#  endif /* VLNEXT && TERMIOS_TTY_DRIVER */

#  if defined (VWERASE) && defined (TERMIOS_TTY_DRIVER)
      {
	unsigned char werase;

	werase = ttybuff.c_cc[VWERASE];

	if (werase != (unsigned char)_POSIX_VDISABLE &&
	    kmap[werase].type == ISFUNC)
	  kmap[werase].function = rl_unix_word_rubout;
      }
#  endif /* VWERASE && TERMIOS_TTY_DRIVER */
    }
#endif /* !NEW_TTY_DRIVER */
}
