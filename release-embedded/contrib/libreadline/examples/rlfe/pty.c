/* Copyright (c) 1993-2002
 *      Juergen Weigert (jnweiger@immd4.informatik.uni-erlangen.de)
 *      Michael Schroeder (mlschroe@immd4.informatik.uni-erlangen.de)
 * Copyright (c) 1987 Oliver Laumann
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (see the file COPYING); if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 ****************************************************************
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

#include "config.h"
#include "screen.h"

#ifndef sun
# include <sys/ioctl.h>
#endif

/* for solaris 2.1, Unixware (SVR4.2) and possibly others */
#ifdef HAVE_SVR4_PTYS
# include <sys/stropts.h>
#endif

#if defined(sun) && defined(LOCKPTY) && !defined(TIOCEXCL)
# include <sys/ttold.h>
#endif

#ifdef ISC
# include <sys/tty.h>
# include <sys/sioctl.h>
# include <sys/pty.h>
#endif

#ifdef sgi
# include <sys/sysmacros.h>
#endif /* sgi */

#include "extern.h"

/*
 * if no PTYRANGE[01] is in the config file, we pick a default
 */
#ifndef PTYRANGE0
# define PTYRANGE0 "qpr"
#endif
#ifndef PTYRANGE1
# define PTYRANGE1 "0123456789abcdef"
#endif

/* SVR4 pseudo ttys don't seem to work with SCO-5 */
#ifdef M_UNIX
# undef HAVE_SVR4_PTYS
#endif

extern int eff_uid;

/* used for opening a new pty-pair: */
static char PtyName[32], TtyName[32];

#if !(defined(sequent) || defined(_SEQUENT_) || defined(HAVE_SVR4_PTYS))
# ifdef hpux
static char PtyProto[] = "/dev/ptym/ptyXY";
static char TtyProto[] = "/dev/pty/ttyXY";
# else
#  ifdef M_UNIX
static char PtyProto[] = "/dev/ptypXY";
static char TtyProto[] = "/dev/ttypXY";
#  else
static char PtyProto[] = "/dev/ptyXY";
static char TtyProto[] = "/dev/ttyXY";
#  endif
# endif /* hpux */
#endif

static void initmaster __P((int));

#if defined(sun)
/* sun's utmp_update program opens the salve side, thus corrupting
 */
int pty_preopen = 1;
#else
int pty_preopen = 0;
#endif

/*
 *  Open all ptys with O_NOCTTY, just to be on the safe side
 *  (RISCos mips breaks otherwise)
 */
#ifndef O_NOCTTY
# define O_NOCTTY 0
#endif

/***************************************************************/

static void
initmaster(f)
int f;
{
#ifdef POSIX
  tcflush(f, TCIOFLUSH);
#else
# ifdef TIOCFLUSH
  (void) ioctl(f, TIOCFLUSH, (char *) 0);
# endif
#endif
#ifdef LOCKPTY
  (void) ioctl(f, TIOCEXCL, (char *) 0);
#endif
}

void
InitPTY(f)
int f;
{
  if (f < 0)
    return;
#if defined(I_PUSH) && defined(HAVE_SVR4_PTYS) && !defined(sgi) && !defined(linux) && !defined(__osf__) && !defined(M_UNIX)
  if (ioctl(f, I_PUSH, "ptem"))
    Panic(errno, "InitPTY: cannot I_PUSH ptem");
  if (ioctl(f, I_PUSH, "ldterm"))
    Panic(errno, "InitPTY: cannot I_PUSH ldterm");
# ifdef sun
  if (ioctl(f, I_PUSH, "ttcompat"))
    Panic(errno, "InitPTY: cannot I_PUSH ttcompat");
# endif
#endif
}

/***************************************************************/

#if defined(OSX) && !defined(PTY_DONE)
#define PTY_DONE
int
OpenPTY(ttyn)
char **ttyn;
{
  register int f;
  if ((f = open_controlling_pty(TtyName)) < 0)
    return -1;
  initmaster(f);
  *ttyn = TtyName;
  return f;
}
#endif

/***************************************************************/

#if (defined(sequent) || defined(_SEQUENT_)) && !defined(PTY_DONE)
#define PTY_DONE
int
OpenPTY(ttyn)
char **ttyn;
{
  char *m, *s;
  register int f;

  if ((f = getpseudotty(&s, &m)) < 0)
    return -1;
#ifdef _SEQUENT_
  fvhangup(s);
#endif
  strncpy(PtyName, m, sizeof(PtyName));
  strncpy(TtyName, s, sizeof(TtyName));
  initmaster(f);
  *ttyn = TtyName;
  return f;
}
#endif

/***************************************************************/

#if defined(__sgi) && !defined(PTY_DONE)
#define PTY_DONE
int
OpenPTY(ttyn)
char **ttyn;
{
  int f;
  char *name, *_getpty(); 
  sigret_t (*sigcld)__P(SIGPROTOARG);

  /*
   * SIGCHLD set to SIG_DFL for _getpty() because it may fork() and
   * exec() /usr/adm/mkpts
   */
  sigcld = signal(SIGCHLD, SIG_DFL);
  name = _getpty(&f, O_RDWR | O_NONBLOCK, 0600, 0);
  signal(SIGCHLD, sigcld);

  if (name == 0)
    return -1;
  initmaster(f);
  *ttyn = name;
  return f;
}
#endif

/***************************************************************/

#if defined(MIPS) && defined(HAVE_DEV_PTC) && !defined(PTY_DONE)
#define PTY_DONE
int
OpenPTY(ttyn)
char **ttyn;
{
  register int f;
  struct stat buf;
   
  strcpy(PtyName, "/dev/ptc");
  if ((f = open(PtyName, O_RDWR | O_NOCTTY | O_NONBLOCK)) < 0)
    return -1;
  if (fstat(f, &buf) < 0)
    {
      close(f);
      return -1;
    }
  sprintf(TtyName, "/dev/ttyq%d", minor(buf.st_rdev));
  initmaster(f);
  *ttyn = TtyName;
  return f;
}
#endif

/***************************************************************/

#if defined(HAVE_SVR4_PTYS) && !defined(PTY_DONE)
#define PTY_DONE
int
OpenPTY(ttyn)
char **ttyn;
{
  register int f;
  char *m, *ptsname();
  int unlockpt __P((int)), grantpt __P((int));
#if defined(HAVE_GETPT) && defined(linux)
  int getpt __P((void));
#endif
  sigret_t (*sigcld)__P(SIGPROTOARG);

  strcpy(PtyName, "/dev/ptmx");
#if defined(HAVE_GETPT) && defined(linux)
  if ((f = getpt()) == -1)
#else
  if ((f = open(PtyName, O_RDWR | O_NOCTTY)) == -1)
#endif
    return -1;

  /*
   * SIGCHLD set to SIG_DFL for grantpt() because it fork()s and
   * exec()s pt_chmod
   */
  sigcld = signal(SIGCHLD, SIG_DFL);
  if ((m = ptsname(f)) == NULL || grantpt(f) || unlockpt(f))
    {
      signal(SIGCHLD, sigcld);
      close(f);
      return -1;
    } 
  signal(SIGCHLD, sigcld);
  strncpy(TtyName, m, sizeof(TtyName));
  initmaster(f);
  *ttyn = TtyName;
  return f;
}
#endif

/***************************************************************/

#if defined(_AIX) && defined(HAVE_DEV_PTC) && !defined(PTY_DONE)
#define PTY_DONE

int
OpenPTY(ttyn)
char **ttyn;
{
  register int f;

  /* a dumb looking loop replaced by mycrofts code: */
  strcpy (PtyName, "/dev/ptc");
  if ((f = open (PtyName, O_RDWR | O_NOCTTY)) < 0)
    return -1;
  strncpy(TtyName, ttyname(f), sizeof(TtyName));
  if (eff_uid && access(TtyName, R_OK | W_OK))
    {
      close(f);
      return -1;
    }
  initmaster(f);
# ifdef _IBMR2
  pty_preopen = 1;
# endif
  *ttyn = TtyName;
  return f;
}
#endif

/***************************************************************/

#if defined(HAVE_OPENPTY) && !defined(PTY_DONE)
#define PTY_DONE
int
OpenPTY(ttyn)
char **ttyn;
{
  int f, s;
  if (openpty(&f, &s, TtyName, NULL, NULL) != 0)
    return -1;
  close(s);
  initmaster(f);
  pty_preopen = 1;
  *ttyn = TtyName;
  return f;    
}
#endif

/***************************************************************/

#ifndef PTY_DONE
int
OpenPTY(ttyn)
char **ttyn;
{
  register char *p, *q, *l, *d;
  register int f;

  debug("OpenPTY: Using BSD style ptys.\n");
  strcpy(PtyName, PtyProto);
  strcpy(TtyName, TtyProto);
  for (p = PtyName; *p != 'X'; p++)
    ;
  for (q = TtyName; *q != 'X'; q++)
    ;
  for (l = PTYRANGE0; (*p = *l) != '\0'; l++)
    {
      for (d = PTYRANGE1; (p[1] = *d) != '\0'; d++)
	{
	  debug1("OpenPTY tries '%s'\n", PtyName);
	  if ((f = open(PtyName, O_RDWR | O_NOCTTY)) == -1)
	    continue;
	  q[0] = *l;
	  q[1] = *d;
	  if (eff_uid && access(TtyName, R_OK | W_OK))
	    {
	      close(f);
	      continue;
	    }
#if defined(sun) && defined(TIOCGPGRP) && !defined(SUNOS3)
	  /* Hack to ensure that the slave side of the pty is
	   * unused. May not work in anything other than SunOS4.1
	   */
	    {
	      int pgrp;

	      /* tcgetpgrp does not work (uses TIOCGETPGRP)! */
	      if (ioctl(f, TIOCGPGRP, (char *)&pgrp) != -1 || errno != EIO)
		{
		  close(f);
		  continue;
		}
	    }
#endif
	  initmaster(f);
	  *ttyn = TtyName;
	  return f;
	}
    }
  return -1;
}
#endif

