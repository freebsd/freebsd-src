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

#ifdef __GNUG__
#pragma implementation
#endif

#include <indstream.h>

indirectbuf::indirectbuf(streambuf *get, streambuf *put, int delete_mode)
: streambuf()
{
    _get_stream = get;
    _put_stream = put == NULL ? get : put;
    _delete_flags = delete_mode;
}

indirectbuf::~indirectbuf()
{
    if (_delete_flags & ios::in)  delete get_stream();
    if (_delete_flags & ios::out)  delete put_stream();
}

int indirectbuf::xsputn(const char* s, int n)
{
    return put_stream()->sputn(s, n);
}

int indirectbuf::xsgetn(char* s, int n)
{
    return get_stream()->sgetn(s, n);
}

int indirectbuf::overflow(int c /* = EOF */)
{
    if (c == EOF)
	return put_stream()->overflow(c);
    else
	return put_stream()->sputc(c);
}

int indirectbuf::underflow()
{
    return get_stream()->sbumpc();
}

streampos indirectbuf::seekoff(streamoff off, _seek_dir dir, int mode)
{
    int ret_val = 0;
    int select = mode == 0 ? (ios::in|ios::out) : mode;
    streambuf *gbuf = (select & ios::in) ? get_stream() : NULL;
    streambuf *pbuf = (select & ios::out) ? put_stream() : NULL;
    if (gbuf == pbuf)
	ret_val = gbuf->seekoff(off, dir, mode);
    else {
	if (gbuf)
	    ret_val = gbuf->seekoff(off, dir, ios::in);
	if (pbuf && ret_val != EOF)
	    ret_val = pbuf->seekoff(off, dir, ios::out);
    }
    return ret_val;
}

streampos indirectbuf::seekpos(streampos pos, int mode)
{
    int ret_val = EOF;
    int select = mode == 0 ? (ios::in|ios::out) : mode;
    streambuf *gbuf = (select & ios::in) ? get_stream() : NULL;
    streambuf *pbuf = (select & ios::out) ? put_stream() : NULL;
    if (gbuf == pbuf)
	ret_val = gbuf->seekpos(pos, mode);
    else {
	if (gbuf)
	    ret_val = gbuf->seekpos(pos, ios::in);
	if (pbuf && ret_val != EOF)
	    ret_val = pbuf->seekpos(pos, ios::out);
    }
    return ret_val;
}

int indirectbuf::sync()
{
    streambuf *gbuf = get_stream();
    int ret_val = gbuf->sync();
    if (ret_val == EOF) return ret_val;
    streambuf *pbuf = put_stream();
    if (pbuf != gbuf) return pbuf->sync();
    else return ret_val;
}

int indirectbuf::pbackfail(int c)
{
    return get_stream()->sputbackc(c);
}
