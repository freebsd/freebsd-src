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

#ifndef __STRSTREAM_H
#define __STRSTREAM_H
#ifdef __GNUG__
#pragma interface
#endif
#include <iostream.h>
#include <strfile.h>

class strstreambuf : public streambuf
{
  struct _IO_str_fields _s;

    void init_dynamic(_IO_alloc_type alloc, _IO_free_type free,
		      int initial_size = 128);
    void init_static(char *ptr, int size, char *pstart);
    void init_readonly(const char *ptr, int size);
  protected:
    int is_static() const { return _s._allocate_buffer == (_IO_alloc_type)0; }
    virtual int overflow(int = EOF);
    virtual int underflow();
    virtual int pbackfail(int c);
  public:
    virtual ~strstreambuf();
    strstreambuf() { init_dynamic(0, 0); }
    strstreambuf(int initial_size) { init_dynamic(0, 0, initial_size); }
    strstreambuf(void *(*alloc)(_IO_size_t), void (*free)(void*))
	{ init_dynamic(alloc, free); }
    strstreambuf(char *ptr, int size, char *pstart = NULL)
	{ init_static(ptr, size, pstart); }
    strstreambuf(unsigned char *ptr, int size, unsigned char *pstart = NULL)
	{ init_static((char*)ptr, size, (char*)pstart); }
    strstreambuf(const char *ptr, int size)
	{ init_readonly(ptr, size); }
    strstreambuf(const unsigned char *ptr, int size)
	{ init_readonly((const char*)ptr, size); }
    strstreambuf(signed char *ptr, int size, signed char *pstart = NULL)
	{ init_static((char*)ptr, size, (char*)pstart); }
    strstreambuf(const signed char *ptr, int size)
	{ init_readonly((const char*)ptr, size); }
    // Note: frozen() is always true if is_static().
    int frozen() { return _flags & _IO_USER_BUF ? 1 : 0; }
    void freeze(int n=1)
	{ if (!is_static())
	    { if (n) _flags |= _IO_USER_BUF; else _flags &= ~_IO_USER_BUF; } }
    _IO_ssize_t pcount();
    char *str();
    virtual streampos seekoff(streamoff, _seek_dir, int mode=ios::in|ios::out);
};

class strstreambase : virtual public ios {
  public:
    strstreambuf* rdbuf() { return (strstreambuf*)ios::rdbuf(); }
  protected:
    strstreambase() { }
    strstreambase(char *cp, int n, int mode=ios::out);
};

class istrstream : public strstreambase, public istream {
  public:
    istrstream(const char*, int=0);
};

class ostrstream : public strstreambase, public ostream {
  public:
    ostrstream();
    ostrstream(char *cp, int n, int mode=ios::out) :strstreambase(cp,n,mode){}
    _IO_ssize_t pcount() { return ((strstreambuf*)_strbuf)->pcount(); }
    char *str() { return ((strstreambuf*)_strbuf)->str(); }
    void freeze(int n = 1) { ((strstreambuf*)_strbuf)->freeze(n); }
    int frozen() { return ((strstreambuf*)_strbuf)->frozen(); }
};

class strstream : public strstreambase, public iostream {
  public:
    strstream() : strstreambase() { init(new strstreambuf()); }
    strstream(char *cp, int n, int mode=ios::out) :strstreambase(cp,n,mode){}
    _IO_ssize_t pcount() { return ((strstreambuf*)_strbuf)->pcount(); }
    char *str() { return ((strstreambuf*)_strbuf)->str(); }
    void freeze(int n = 1) { ((strstreambuf*)_strbuf)->freeze(n); }
    int frozen() { return ((strstreambuf*)_strbuf)->frozen(); }
};

#endif /*!__STRSTREAM_H*/
