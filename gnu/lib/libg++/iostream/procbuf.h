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

#include <streambuf.h>

class procbuf : public filebuf {
    _G_pid_t _pid;
  public:
    procbuf() : filebuf() { }
    procbuf(const char *command, int mode);
    procbuf* open(const char *command, int mode);
    procbuf *close() { return (procbuf*)filebuf::close(); }
    virtual int sys_close();
    ~procbuf();
};
