/* signals.h -- Header to include system dependent signal definitions. */

/* This file is part of GNU Info, a program for reading online documentation
   stored in Info format.

   Copyright (C) 1993 Free Software Foundation, Inc.

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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   Written by Brian Fox (bfox@ai.mit.edu). */

#ifndef _SIGNALS_H_
#define _SIGNALS_H_

#include <signal.h>

#define HAVE_SIGSETMASK

#if !defined (_POSIX_VERSION) && !defined (sigmask)
#  define sigmask(x) (1 << ((x)-1))
#endif /* !POSIX && !sigmask */

#if !defined (_POSIX_VERSION)
#  if !defined (SIG_BLOCK)
#    define SIG_UNBLOCK 1
#    define SIG_BLOCK   2
#    define SIG_SETMASK 3
#  endif /* SIG_BLOCK */

/* Type of a signal set. */
#  define sigset_t int

/* Make SET have no signals in it. */
#  define sigemptyset(set) (*(set) = (sigset_t)0x0)

/* Make SET have the full range of signal specifications possible. */
#  define sigfillset(set) (*(set) = (sigset_t)0xffffffffff)

/* Add SIG to the contents of SET. */
#  define sigaddset(set, sig) *(set) |= sigmask (sig)

/* Delete SIG from the contents of SET. */
#  define sigdelset(set, sig) *(set) &= ~(sigmask (sig))

/* Tell if SET contains SIG. */
#  define sigismember(set, sig) (*(set) & (sigmask (sig)))

/* Suspend the process until the reception of one of the signals
   not present in SET. */
#  define sigsuspend(set) sigpause (*(set))
#endif /* !_POSIX_VERSION */

/* These definitions are used both in POSIX and non-POSIX implementations. */

#define BLOCK_SIGNAL(sig) \
  do { \
    sigset_t nvar, ovar; \
    sigemptyset (&nvar); \
    sigemptyset (&ovar); \
    sigaddset (&nvar, sig); \
    sigprocmask (SIG_BLOCK, &nvar, &ovar); \
  } while (0)

#define UNBLOCK_SIGNAL(sig) \
  do { \
    sigset_t nvar, ovar; \
    sigemptyset (&ovar); \
    sigemptyset (&nvar); \
    sigaddset (&nvar, sig); \
    sigprocmask (SIG_UNBLOCK, &nvar, &ovar); \
  } while (0)

#endif /* !_SIGNALS_H_ */
