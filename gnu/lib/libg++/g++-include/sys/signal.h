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

/* Partly for systems that think signal.h is is sys/ */
/* But note that some systems that use sys/signal.h to define signal.h. */

#ifndef __libgxx_sys_signal_h
#if defined(__sys_signal_h_recursive) || defined(__signal_h_recursive)
#include_next <sys/signal.h>
#else
#define __sys_signal_h_recursive

extern "C" {
#define signal __hide_signal
#include_next <sys/signal.h>
#undef signal
}

#define __libgxx_sys_signal_h 1
#endif
#endif

