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

#include <stdiostream.h>

// A stdiobuf is "tied" to a FILE object (as used by the stdio package).
// Thus a stdiobuf is always synchronized with the corresponding FILE,
// though at the cost of some overhead.  (If you use the implementation
// of stdio supplied with this library, you don't need stdiobufs.)
// This implementation inherits from filebuf, but implement the virtual
// functions sys_read/..., using the stdio functions fread/... instead
// of the low-level read/... system calls.  This has the advantage that
// we get all of the nice filebuf semantics automatically, though
// with some overhead.


#ifndef SEEK_SET
#define SEEK_SET 0
#endif
#ifndef SEEK_CUR
#define SEEK_CUR 1
#endif
#ifndef SEEK_END
#define SEEK_END 2
#endif

stdiobuf::stdiobuf(FILE *f) : filebuf(fileno(f))
{
    _file = f;
    // Turn off buffer in stdiobuf.  Instead, rely on buffering in (FILE).
    // Thus the stdiobuf will be synchronized with the FILE.
    setbuf(NULL, 0);
}

_G_ssize_t stdiobuf::sys_read(char* buf, size_t size)
{
    return fread(buf, 1, size, _file);
}

_G_ssize_t stdiobuf::sys_write(const void *buf, long n)
{
    _G_ssize_t count = fwrite(buf, 1, n, _file);
    if (_fb._offset >= 0)
	_fb._offset += n;
    return count;
}

_G_fpos_t stdiobuf::sys_seek(_G_fpos_t offset, _seek_dir dir)
{
    // Normally, equivalent to: fdir=dir
    int fdir =
	(dir == ios::beg) ? SEEK_SET :
        (dir == ios::cur) ? SEEK_CUR :
        (dir == ios::end) ? SEEK_END :
        dir;
    return fseek(_file, offset, fdir);
}

int stdiobuf::sys_close()
{
    int status = fclose(_file);
    _file = NULL;
    return status;
}

int stdiobuf::sync()
{
    if (filebuf::sync() == EOF)
	return EOF;
    if (!(xflags() & _S_NO_WRITES))
	if (fflush(_file))
	    return EOF;
#if 0
	// This loses when writing to a pipe.
    if (fseek(_file, 0, SEEK_CUR) == EOF)
	return EOF;
#endif
    return 0;
}

int stdiobuf::overflow(int c /* = EOF*/)
{
    if (filebuf::overflow(c) == EOF)
	return EOF;
    if (c != EOF)
	return c;
    return fflush(_file);
}

int stdiobuf::xsputn(const char* s, int n)
{
    // The filebuf implementation of sputn loses.
    return streambuf::xsputn(s, n);
}
