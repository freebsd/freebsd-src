/* Random host-dependent support code.
   Copyright (C) 1995 Free Software Foundation, Inc.
   Written by Ken Raeburn.

This file is part of libopcodes, the opcodes library.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* Do system-dependent stuff, mainly driven by autoconf-detected info.

   Well, some generic common stuff is done here too, like including
   ansidecl.h.  That's because the .h files in bfd/hosts files I'm
   trying to replace often did that.  If it can be dropped from this
   file (check in a non-ANSI environment!), it should be.  */

#include "config.h"

#include <ansidecl.h>

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif
