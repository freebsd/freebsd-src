//    This is part of the iostream library, providing input/output for C++.
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

#include "ioprivate.h"
#include "iostream.h"

istream& istream::get(streambuf& sb, char delim /* = '\n' */)
{
    _gcount = 0;
    if (ipfx1()) {
	register streambuf* isb = rdbuf();
	for (;;) {
	    int len = isb->egptr() - isb->gptr();
	    if (len <= 0)
		if (isb->underflow() == EOF)
		    break;
		else
		    len = isb->egptr() - isb->gptr();
	    char *delimp = (char*)memchr((void*)isb->gptr(), delim, len);
	    if (delimp != NULL)
		len = delimp - isb->gptr();
	    int written = sb.sputn(isb->gptr(), len);
	    isb->gbump(written);
	    _gcount += written;
	    if (written != len) {
		set(ios::failbit);
		break;
	    }
	    if (delimp != NULL)
		break;
	}
    }
    return *this;
}
