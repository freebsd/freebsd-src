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

#ifndef _FSTREAM_H
#define _FSTREAM_H
#ifdef __GNUG__
#pragma interface
#endif
#include <iostream.h>

class fstreambase : virtual public ios {
  public:
    fstreambase();
    fstreambase(int fd);
    fstreambase(const char *name, int mode, int prot=0664);
    void close();
    filebuf* rdbuf() const { return (filebuf*)_strbuf; }
    void open(const char *name, int mode, int prot=0664);
    int is_open() const { return rdbuf()->is_open(); }
    void setbuf(char *ptr, int len) { rdbuf()->setbuf(ptr, len); }
#ifdef _STREAM_COMPAT
    int filedesc() { return rdbuf()->fd(); }
    fstreambase& raw() { rdbuf()->setbuf(NULL, 0); return *this; }
#endif
};

class ifstream : public fstreambase, public istream {
  public:
    ifstream() : fstreambase() { }
    ifstream(int fd) : fstreambase(fd) { }
    ifstream(const char *name, int mode=ios::in, int prot=0664)
	: fstreambase(name, mode, prot) { }
    void open(const char *name, int mode=ios::in, int prot=0664)
	{ fstreambase::open(name, mode, prot); }
};

class ofstream : public fstreambase, public ostream {
  public:
    ofstream() : fstreambase() { }
    ofstream(int fd) : fstreambase(fd) { }
    ofstream(const char *name, int mode=ios::out, int prot=0664)
	: fstreambase(name, mode, prot) { }
    void open(const char *name, int mode=ios::out, int prot=0664)
	{ fstreambase::open(name, mode, prot); }
};

class fstream : public fstreambase, public iostream {
  public:
    fstream() : fstreambase() { }
    fstream(int fd) : fstreambase(fd) { }
    fstream(const char *name, int mode, int prot=0664)
	: fstreambase(name, mode, prot) { }
    void open(const char *name, int mode, int prot=0664)
	{ fstreambase::open(name, mode, prot); }
};
#endif /*!_FSTREAM_H*/
