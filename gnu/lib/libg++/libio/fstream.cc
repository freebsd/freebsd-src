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
the executable file might be covered by the GNU General Public License.

Written by Per Bothner (bothner@cygnus.com). */

#ifdef __GNUG__
#pragma implementation
#endif
#define _STREAM_COMPAT
extern "C" {
#include "libioP.h"
}
#include <fstream.h>

fstreambase::fstreambase()
{
    init(filebuf::__new());
}

fstreambase::fstreambase(int fd)
{
  init(filebuf::__new());
  _IO_file_attach(rdbuf(), fd);
}

fstreambase::fstreambase(const char *name, int mode, int prot)
{
    init(filebuf::__new());
    if (!rdbuf()->open(name, mode, prot))
	set(ios::badbit);
}

fstreambase::fstreambase(int fd, char *p, int l)
{
  init(filebuf::__new());
  _IO_file_attach(rdbuf(), fd);
  _IO_file_setbuf(rdbuf(), p, l);
}

void fstreambase::open(const char *name, int mode, int prot)
{
    clear();
    if (!rdbuf()->open(name, mode, prot))
	set(ios::badbit);
}

void fstreambase::close()
{
    if (!rdbuf()->close())
	set(ios::failbit);
}

#if 0
static int mode_to_sys(enum open_mode mode)
{
    return O_WRONLY;
}

static char* fopen_cmd_arg(io_mode i)
{
    return "w";
}
#endif
