/* This is part of libio/iostream, providing -*- C++ -*- input/output.
Copyright (C) 1993 Free Software Foundation

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

#ifdef __GNUG__
#pragma implementation
#endif
#include "iostreamP.h"
#include "strstream.h"
#include <string.h>

static void* default_alloc(_IO_size_t size)
{
    return (void*)new char[size];
}

static void default_free(void* ptr)
{
    delete [] (char*)ptr;
}

/* Set to use the _IO_str_jump jumptable, for efficiency */

#define SET_STR_JUMPS(STRBUF) \
  (STRBUF)->_jumps = &_IO_str_jumps,\
  (STRBUF)->_vtable() = builtinbuf_vtable;

istrstream::istrstream(const char *cp, int n)
{
  init(new strstreambuf(cp, n));
  SET_STR_JUMPS(_strbuf);
}

ostrstream::ostrstream()
{
  init(new strstreambuf());
  SET_STR_JUMPS(_strbuf);
}

strstreambase::strstreambase(char *cp, int n, int mode)
{
  char *pstart;
  if (mode == ios::app || mode == ios::ate)
    pstart = cp + strlen(cp);
  else
    pstart = cp;
  init(new strstreambuf(cp, n, pstart));
  SET_STR_JUMPS(_strbuf);
}

char *strstreambuf::str()
{
    freeze(1);
    return base();
}

_IO_ssize_t strstreambuf::pcount() { return _IO_str_count (this); }

int strstreambuf::overflow(int c /* = EOF */)
{
  return _IO_str_overflow (this, c);
}

int strstreambuf::underflow()
{
  return _IO_str_underflow(this);
}


void strstreambuf::init_dynamic(_IO_alloc_type alloc, _IO_free_type free,
				int initial_size)
				
{
    _s._len = 0;
    if (initial_size < 16)
	initial_size = 16;
    _s._allocate_buffer = alloc ? alloc : default_alloc;
    _s._free_buffer = free ? free : default_free;
    char * buf = (char*)(*_s._allocate_buffer)(initial_size);
    setb(buf, buf + initial_size, 1);
    setp(buf, buf + initial_size);
    setg(buf, buf, buf);
}

void strstreambuf::init_static(char *ptr, int size, char *pstart)
{
  _IO_str_init_static (this, ptr, size, pstart);
}

void strstreambuf::init_readonly (const char *ptr, int size)
{
  _IO_str_init_readonly (this, ptr, size);
}

strstreambuf::~strstreambuf()
{
    if (_IO_buf_base && !(_flags & _IO_USER_BUF))
        (_s._free_buffer)(_IO_buf_base);
    _IO_buf_base = NULL;
}

streampos strstreambuf::seekoff(streamoff off, _seek_dir dir,
					int mode /*=ios::in|ios::out*/)
{
  return _IO_str_seekoff (this, off, convert_to_seekflags(dir, mode));
}

int strstreambuf::pbackfail(int c)
{
  return _IO_str_pbackfail (this, c);
}
