// This may look like C code, but it is really -*- C++ -*-
/* 
Copyright (C) 1989 Free Software Foundation
    written by Doug Lea (dl@rocky.oswego.edu)

This file is part of the GNU C++ Library.  This library is free
software; you can redistribute it and/or modify it under the terms of
the GNU Library General Public License as published by the Free
Software Foundation; either version 2 of the License, or (at your
option) any later version.  This library is distributed in the hope
that it will be useful, but WITHOUT ANY WARRANTY; without even the
implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.  See the GNU Library General Public License for more details.
You should have received a copy of the GNU Library General Public
License along with this library; if not, write to the Free Software
Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef _signal_h

#include <_G_config.h>

extern "C" {

#ifdef __signal_h_recursive
#include_next <signal.h>
#else

#define __signal_h_recursive

#define signal __hide_signal
#define psignal __hide_psignal
#include_next <signal.h>
#undef signal
#undef psignal

#define _signal_h 1

// The Interviews folks call this SignalHandler. Might as well conform.
typedef _G_signal_return_type (*SignalHandler) (...);
typedef int (*SSignalHandler) (...);

extern SignalHandler signal _G_ARGS((int sig, SignalHandler action));
//extern SignalHandler sigset _G_ARGS((int sig, SignalHandler action));
extern SSignalHandler ssignal _G_ARGS((int sig, SSignalHandler action));
extern int           gsignal  _G_ARGS((int sig));
extern int           kill  _G_ARGS((_G_pid_t pid, int sig));
#ifndef __386BSD__
extern int           killpg _G_ARGS((short int, int));
#else
extern int           killpg _G_ARGS((_G_pid_t, int));
#endif
extern int           siginterrupt _G_ARGS((int, int));
extern void	     psignal _G_ARGS((unsigned, const char*));

#ifndef hpux // Interviews folks claim that hpux doesn't like these
extern int           sigsetmask _G_ARGS((int mask));
extern int           sigblock _G_ARGS((int mask));
extern int           sigpause _G_ARGS((int mask));
extern int           sigvec _G_ARGS((int, struct sigvec*, struct sigvec*));
#endif

// The Interviews version also has these ...

#define SignalBad ((SignalHandler)-1)
#define SignalDefault ((SignalHandler)0)
#define SignalIgnore ((SignalHandler)1)

#undef BADSIG
#undef SIG_DFL
#undef SIG_IGN
#define BAD_SIG SignalBad
#define SIG_DFL SignalDefault
#define SIG_IGN SignalIgnore

#endif
}

#endif

