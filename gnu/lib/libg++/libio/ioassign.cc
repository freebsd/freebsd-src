/* This is part of libio/iostream, providing -*- C++ -*- input/output.
Copyright (C) 1994 Free Software Foundation

This file is part of the GNU IO Library.  This library is free
software; you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the
Free Software Foundation; either version 2, or (at your option)
any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU CC; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

As a special exception, if you link this library with files
compiled with a GNU compiler to produce an executable, this does not cause
the resulting executable to be covered by the GNU General Public License.
This exception does not however invalidate any other reasons why
the executable file might be covered by the GNU General Public License. */

/* Written by Per Bothner (bothner@cygnus.com). */

#include <iostream.h>
#include "libioP.h"

// These method are provided for backward compatibility reasons.
// It's generally poor style to use them.
// They are not supported by the ANSI/ISO working paper.

_IO_istream_withassign&  _IO_istream_withassign::operator=(istream& rhs)
{
  if (&rhs != (istream*)this)
    {
      if (!(_flags & (unsigned int)ios::dont_close)) delete rdbuf();
      init (rhs.rdbuf ());
      _flags |= ios::dont_close;
      _gcount = 0;
    }
  return *this;
}

_IO_ostream_withassign&  _IO_ostream_withassign::operator=(ostream& rhs)
{
  if (&rhs != (ostream*)this)
    {
      if (!(_flags & (unsigned int)ios::dont_close)) delete rdbuf();
      init (rhs.rdbuf ());
      _flags |= ios::dont_close;
    }
  return *this;
}
