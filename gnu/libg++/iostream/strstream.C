//    This is part of the iostream library, providing input/output for C++.
//    Copyright (C) 1991 Per Bothner.
//
//    This library is free software; you can redistribute it and/or
//    modify it under the terms of the GNU Library General Public
//    License as published by the Free Software Foundation; either
//    version 2 of the License, or (at your option) any later version.
//
//    This library is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//    Library General Public License for more details.
//
//    You should have received a copy of the GNU Library General Public
//    License along with this library; if not, write to the Free
//    Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#ifdef __GNUG__
#pragma implementation
#endif
#include "ioprivate.h"
#include <strstream.h>

static void* default_alloc(_G_size_t size)
{
    return (void*)new char[size];
}

static void default_free(void* ptr)
{
    delete [] (char*)ptr;
}

istrstream::istrstream(const char *cp, int n)
{
    init(new strstreambuf(cp, n));
}

ostrstream::ostrstream()
{
    init(new strstreambuf());
}

strstreambase::strstreambase(char *cp, int n, int mode)
{
    char *pstart;
    if (mode == ios::app || mode == ios::ate)
	pstart = cp + strlen(cp);
    else
	pstart = cp;
    init(new strstreambuf(cp, n, pstart));
}

char *strstreambuf::str()
{
    freeze(1);
    return base();
}

_G_size_t strstreambuf::pcount()
{
    _G_size_t put_len = pptr() - pbase();
    if (put_len < _len) put_len = _len;
    return put_len;
}

int strstreambuf::overflow(int c /* = EOF */)
{
  const int flush_only = c == EOF;
  if (_flags & _S_NO_WRITES)
      return flush_only ? 0 : EOF;
  size_t pos = pptr() - pbase();
  size_t get_pos = gptr() - pbase();
  if (pos > _len) _len = pos;
  if (pos >= blen() + flush_only) {
      char *new_buf;
      size_t new_size = 2 * blen();
      if (frozen()) /* not allowed to enlarge */
	  return EOF;
      new_buf = (char*)(*_allocate_buffer)(new_size);
      memcpy(new_buf, base(), blen());
      if (new_buf == NULL) {
//	  __ferror(fp) = 1;
	  return EOF;
      }
#if 0
      if (lenp == &_len) /* use '\0'-filling */
	  memset(new_buf + pos, 0, blen() - pos);
#endif
      if (_base) {
	  (*_free_buffer)(_base);
	  _base = NULL; // So setb() won't try to delete _base.
      }
      setb(new_buf, new_buf + new_size, 1);
    }

  setp(base(), ebuf());
  pbump(pos);
  setg(base(), base() + get_pos, base() + _len);
  if (!flush_only) {
      *pptr() = (unsigned char) c;
      pbump(1);
  }
  return c;
}

int strstreambuf::underflow()
{
    size_t ppos = pptr() - pbase();
    if (ppos > _len) _len = ppos;
    setg(base(), gptr(), base() + _len);
    if (gptr() < egptr())
	return *gptr();
    else
	return EOF;
}


void strstreambuf::init_dynamic(_alloc_type alloc, _free_type free,
				int initial_size)
				
{
    _len = 0;
    if (initial_size < 16)
	initial_size = 16;
    _allocate_buffer = alloc ? alloc : default_alloc;
    _free_buffer = free ? free : default_free;
    char * buf = (char*)(*_allocate_buffer)(initial_size);
    setb(buf, buf + initial_size, 1);
    setp(buf, buf + initial_size);
    setg(buf, buf, buf);
}

void strstreambuf::init_static(char *ptr, int size, char *pstart)
{
    if (size == 0)
	size = strlen(ptr);
    else if (size < 0) {
	// If size is negative 'the characters are assumed to
	// continue indefinitely.'  This is kind of messy ...
#if 1
	size = 512;
	// Try increasing powers of 2, as long as we don't wrap around.
	// This can lose in pathological cases (ptr near the end
	// of the address space).  A better solution might be to
	// adjust the size on underflow/overflow.  FIXME.
	for (int s; s = 2*size, s > 0 && ptr + s > ptr && s < 0x4000000L; )
	    size = s;
	size = s;
#else
	// The following semi-portable kludge assumes that
	// sizeof(unsigned long) == sizeof(char*). Hence,
	// (unsigned long)(-1) should be the largest possible address.
	unsigned long highest = (unsigned long)(-1);
	// Pointers are signed on some brain-damaged systems, in
	// which case we divide by two to get the maximum signed address.
	if  ((char*)highest < ptr)
	    highest >>= 1;
	size = (char*)highest - ptr;
#endif
    }
    setb(ptr, ptr+size);
    if (pstart) {
	setp(ptr, ebuf());
	pbump(pstart-ptr);
	setg(ptr, ptr, pstart);
    }
    else {
	setp(ptr, ptr); 
	setg(ptr, ptr, ebuf());
    }
    _len = egptr() - ptr;
}

void strstreambuf::init_static (const char *ptr, int size)
{
  init_static((char*)ptr, size, NULL);
  xsetflags(_S_NO_WRITES);
}

strstreambuf::~strstreambuf()
{
    if (_base && !(_flags & _S_USER_BUF))
        (_free_buffer)(_base);
    _base = NULL;
}

streampos strstreambuf::seekoff(streamoff off, _seek_dir dir,
					int mode /*=ios::in|ios::out*/)
{
    size_t cur_size = pcount();
    streampos new_pos = EOF;

    // Move the get pointer, if requested.
    if (mode & ios::in) {
	switch (dir) {
	  case ios::end:
	    off += cur_size;
	    break;
	  case ios::cur:
	    off += gptr() - pbase();
	    break;
	  default: /*case ios::beg: */
	    break;
	}
	if (off < 0 || (size_t)off > cur_size)
	    return EOF;
	setg(base(), base() + off, base() + cur_size);
	new_pos = off;
    }

    // Move the put pointer, if requested.
    if (mode & ios::out) {
	switch (dir) {
	  case ios::end:
	    off += cur_size;
	    break;
	  case ios::cur:
	    off += pptr() - pbase();
	    break;
	  default: /*case ios::beg: */
	    break;
	}
	if (off < 0 || (size_t)off > cur_size)
	    return EOF;
	pbump(base() + off - pptr());
	new_pos = off;
    }
    return new_pos;
}

int strstreambuf::pbackfail(int c)
{
    if ((_flags & _S_NO_WRITES) && c != EOF)
	return EOF;
    return backupbuf::pbackfail(c);
}
