// This may look like C code, but it is really -*- C++ -*-
/*
Copyright (C) 1994 Free Software Foundation
    written by Jason Merrill (jason@cygnus.com)

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

#ifndef _defines_h
#define _defines_h

#include <_G_config.h>
#include <stddef.h>

const size_t NPOS = (size_t)(-1);
typedef void fvoid_t();

#ifndef _WINT_T
#define _WINT_T
typedef _G_wint_t wint_t;
#endif

enum capacity { default_size, reserve };

#endif
