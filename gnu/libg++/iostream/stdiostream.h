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

#ifndef _STDIOSTREAM_H
#define _STDIOSTREAM_H

#ifdef __GNUG__
#pragma interface
#endif

#include <streambuf.h>
#include <stdio.h>

class stdiobuf : public filebuf {
  protected:
    FILE *_file;
  public:
    FILE* stdiofile() const { return _file; }
    stdiobuf(FILE *f);
    virtual _G_ssize_t sys_read(char* buf, _G_size_t size);
    virtual _G_fpos_t sys_seek(_G_fpos_t, _seek_dir);
    virtual _G_ssize_t sys_write(const void*, long);
    virtual int sys_close();
    virtual int sync();
    virtual int overflow(int c = EOF);
    virtual int xsputn(const char* s, int n);
};

#endif /* !_STDIOSTREAM_H */
