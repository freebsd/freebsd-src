/* 
Copyright (C) 1988 Free Software Foundation
    written by Doug Lea (dl@rocky.oswego.edu)

This file is part of the GNU C++ Library.  This library is free
software; you can redistribute it and/or modify it under the terms of
the GNU Library General Public License as published by the Free
Software Foundation; either version 2 of the License, or (at your
option) any later version.  This library is distributed in the hope
that it will be useful, but WITHOUT ANY WARRANTY; without even the
implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.  See the GNU Library General Public License for more details.
You should have received a copy of the GNU Library General Public
License along with this library; if not, write to the Free Software
Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifdef __GNUG__
#pragma implementation
#endif
#include <SFile.h>

SFile::SFile(const char *filename, int size, int mode, int prot)
: fstream(filename, mode, prot)
{ 
  sz = size; 
}

SFile::SFile(int fd, int size)
: fstream(fd)
{ 
  sz = size; 
}

void SFile::open(const char *name, int size, int mode, int prot)
{
    fstream::open(name, mode, prot);
    sz = size;
}

SFile& SFile::get(void* x)
{
    read(x, sz);
    return *this;
}

SFile& SFile::put(void* x)
{
    write(x, sz);
    return *this;
}

SFile& SFile::operator[](long i)
{
    if (rdbuf()->seekoff(i * sz, ios::beg) == EOF)
	set(ios::badbit);
    return *this;
}
