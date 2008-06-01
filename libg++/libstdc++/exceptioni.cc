// Functions for Exception Support for -*- C++ -*-
// Copyright (C) 1994, 1995 Free Software Foundation

// This file is part of the GNU ANSI C++ Library.  This library is free
// software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the
// Free Software Foundation; either version 2, or (at your option)
// any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this library; see the file COPYING.  If not, write to the Free
// Software Foundation, 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

// As a special exception, if you link this library with files
// compiled with a GNU compiler to produce an executable, this does not cause
// the resulting executable to be covered by the GNU General Public License.
// This exception does not however invalidate any other reasons why
// the executable file might be covered by the GNU General Public License.

#include <exception>
#include <stdlib.h>

/* terminate (), unexpected (), set_terminate (), set_unexpected () as
   well as the default terminate func and default unexpected func */

#if 0
extern "C" int printf(const char *, ...);
#endif

void
__default_terminate ()
{
  abort ();
}

void
__default_unexpected ()
{
  __default_terminate ();
}

static terminate_handler __terminate_func = __default_terminate;
static unexpected_handler __unexpected_func = __default_unexpected;

terminate_handler
set_terminate (terminate_handler func)
{
  terminate_handler old = __terminate_func;

  __terminate_func = func;
  return old;
}

unexpected_handler
set_unexpected (unexpected_handler func)
{
  unexpected_handler old = __unexpected_func;

  __unexpected_func = func;
  return old;
}

void
terminate ()
{
  __terminate_func ();
}

void
unexpected ()
{
  __unexpected_func ();
}
