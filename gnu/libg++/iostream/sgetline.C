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

#include "ioprivate.h"

// Algorithm based on that used by Berkeley pre-4.4 fgets implementation.

// Read chars into buf (of size n), until delim is seen.
// Return number of chars read (at most n-1).
// If extract_delim < 0, leave delimiter unread.
// If extract_delim > 0, insert delim in output.

long streambuf::sgetline(char* buf, size_t n, char delim, int extract_delim)
{
    register char *ptr = buf;
    if (n <= 0)
	return EOF;
    n--; // Leave space for final '\0'.
    do {
	int len = egptr() - gptr();
	if (len <= 0)
	    if (underflow() == EOF)
		break;
	    else
		len = egptr() - gptr();
	if (len >= (int)n)
	    len = n;
	char *t = (char*)memchr((void*)_gptr, delim, len);
	if (t != NULL) {
	    size_t old_len = ptr-buf;
	    len = t - _gptr;
	    if (extract_delim >= 0) {
		t++;
		old_len++;
		if (extract_delim > 0)
		    len++;
	    }
	    memcpy((void*)ptr, (void*)_gptr, len);
	    ptr[len] = 0;
	    _gptr = t;
	    return old_len + len;
	}
	memcpy((void*)ptr, (void*)_gptr, len);
	_gptr += len;
	ptr += len;
	n -= len;
    } while (n != 0);
    *ptr = 0;
    return ptr - buf;
}
