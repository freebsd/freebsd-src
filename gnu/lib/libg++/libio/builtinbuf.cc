/* 
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

#ifdef __GNUC__
#pragma implementation
#endif
#define _STREAM_COMPAT
#include "builtinbuf.h"
#include "iostreamP.h"

int builtinbuf::overflow(int ch) { return (*_jumps->__overflow)(this, ch); }

int builtinbuf::underflow() { return (*_jumps->__underflow)(this); }

streamsize builtinbuf::xsgetn(char* buf, streamsize n)
{ return (*_jumps->__xsgetn)(this, buf, n); }

streamsize builtinbuf::xsputn(const char* buf, streamsize n)
{ return _jumps->__xsputn (this, buf, n); }

int builtinbuf::doallocate() { return _jumps->__doallocate(this); }

builtinbuf::~builtinbuf() { _jumps->__finish(this); }

int builtinbuf::sync() { return _jumps->__sync(this); }

streambuf* builtinbuf::setbuf(char *buf, int n)
{ return _jumps->__setbuf (this, buf, n) == 0 ? this : NULL; }

streampos builtinbuf::seekoff(streamoff off, _seek_dir dir, int mode)
{
  return _jumps->__seekoff (this, off, convert_to_seekflags(dir, mode));
}

streampos builtinbuf::seekpos(streampos pos, int mode)
{
  int flags = 0;
  if (!(mode & ios::in))
    flags |= _IO_seek_not_in;
  if (!(mode & ios::out))
    flags |= _IO_seek_not_out;
  return _jumps->__seekpos(this, pos, (_IO_seekflags)flags);
}

int builtinbuf::pbackfail(int c)
{ return _jumps->__pbackfail(this, c); }

streamsize builtinbuf::sys_read(char* buf, streamsize size)
{ return _jumps->__read(this, buf, size); }

streampos builtinbuf::sys_seek(streamoff off, _seek_dir dir)
{ return _jumps->__seek(this, off, dir); }

streamsize builtinbuf::sys_write(const char* buf, streamsize size)
{ return _jumps->__write(this, buf, size); }

int builtinbuf::sys_stat(void* buf) // Actually, a (struct stat*)
{ return _jumps->__stat(this, buf); }

int builtinbuf::sys_close()
{ return _jumps->__close(this); }

#ifdef _STREAM_COMPAT
/* These methods are TEMPORARY, for binary compatibility! */
#include <stdlib.h>
void ios::_IO_fix_vtable() const
{
  abort ();
}

void ios::_IO_fix_vtable()
{
  ((const ios*) this)->_IO_fix_vtable();
}
#endif
