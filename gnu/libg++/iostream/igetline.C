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

#include "iostream.h"
#include <string.h>

istream& istream::getline(char* buf, int len, char delim)
{
    _gcount = 0;
    if (ipfx1()) {
	streambuf *sb = rdbuf();
	long count = sb->sgetline(buf, len, delim, -1);
	if (count == len-1)
	    set(ios::failbit);
	else {
	    int ch = sb->sbumpc();
	    if (ch == EOF)
		set(ios::failbit|ios::eofbit);
	    else if (ch == (unsigned char)delim)
		count++;
	    else
		sb->sungetc(); // Leave delimiter unread.
	}
	_gcount = count;
    }
    return *this;
}

istream& istream::get(char* buf, int len, char delim)
{
    _gcount = 0;
    if (ipfx1()) {
	streambuf *sbuf = rdbuf();
	long count = sbuf->sgetline(buf, len, delim, -1);
	if (count < 0 || (count == 0 && sbuf->sgetc() == EOF))
	    set(ios::failbit|ios::eofbit);
	else
	    _gcount = count;
    }
    return *this;
}

//    This is part of the iostream library, providing input/output for C++.
//    Copyright (C) 1992 Free Software Foundation.
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

// from Doug Schmidt

#define CHUNK_SIZE 512

/* Reads an arbitrarily long input line terminated by a user-specified
   TERMINATOR.  Super-nifty trick using recursion avoids unnecessary calls
   to NEW! */

char *_sb_readline (streambuf *sb, long& total, char terminator) 
{
    char buf[CHUNK_SIZE+1];
    char *ptr;
    int ch;
    
    long count = sb->sgetline(buf, CHUNK_SIZE+1, terminator, -1);
    if (count == EOF)
	return NULL;
    ch = sb->sbumpc();
    long old_total = total;
    total += count;
    if (ch != EOF && ch != terminator) {
	total++; // Include ch in total.
	ptr = _sb_readline(sb, total, terminator);
	if (ptr) {
	    memcpy(ptr + old_total, buf, count);
	    ptr[old_total+count] = ch;
	}
	return ptr;
    }
    
    if (ptr = new char[total+1]) {
	ptr[total] = '\0';
	memcpy(ptr + total - count, buf, count);
	return ptr;
    } 
    else 
	return NULL;
}

/* Reads an arbitrarily long input line terminated by TERMINATOR.
   This routine allocates its own memory, so the user should
   only supply the address of a (char *). */

istream& istream::gets(char **s, char delim /* = '\n' */)
{
    if (ipfx1()) {
	long size = 0;
	streambuf *sb = rdbuf();
	*s = _sb_readline (sb, size, delim);
	_gcount = *s ? size : 0;
	if (sb->_flags & _S_EOF_SEEN) {
	    set(ios::eofbit);
	    if (_gcount == 0)
		set(ios::failbit);
	}
    }
    else {
	_gcount = 0;
	*s = NULL;
    }
    return *this;
}
