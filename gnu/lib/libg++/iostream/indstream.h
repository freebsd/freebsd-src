//    This is part of the iostream library, providing -*- C++ -*- input/output.
//    Copyright (C) 1992 Per Bothner.
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

#ifndef _INDSTREAM_H
#define _INDSTREAM_H

#ifdef __GNUG__
#pragma interface
#endif

#include <iostream.h>

// An indirectbuf is one that forwards all of its I/O requests
// to another streambuf.
// All get-related requests are sent to get_stream().
// All put-related requests are sent to put_stream().

// An indirectbuf can be used to implement Common Lisp
// synonym-streams and two-way-streams.
//
// class synonymbuf : public indirectbuf {
//    Symbol *sym;
//    synonymbuf(Symbol *s) { sym = s; }
//    virtual streambuf *lookup_stream(int mode) {
//        return coerce_to_streambuf(lookup_value(sym)); }
// };

class indirectbuf : public streambuf {
  protected:
    streambuf *_get_stream;  // Optional cache for get_stream().
    streambuf *_put_stream;  // Optional cache for put_stream().
    int _delete_flags;
  public:
    streambuf *get_stream()
	{ return _get_stream ? _get_stream : lookup_stream(ios::in); }
    streambuf *put_stream()
	{ return _put_stream ? _put_stream : lookup_stream(ios::out); }
    virtual streambuf *lookup_stream(int/*mode*/) { return NULL; } // ERROR!
    indirectbuf(streambuf *get=NULL, streambuf *put=NULL, int delete_mode=0);
    virtual ~indirectbuf();
    virtual int xsputn(const char* s, int n);
    virtual int xsgetn(char* s, int n);
    virtual int underflow();
    virtual int overflow(int c = EOF);
    virtual streampos seekoff(streamoff, _seek_dir, int mode=ios::in|ios::out);
    virtual streampos seekpos(streampos pos, int mode = ios::in|ios::out);
    virtual int sync();
    virtual int pbackfail(int c);
};

#endif /* !_INDSTREAM_H */
