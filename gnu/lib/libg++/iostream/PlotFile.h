// This may look like C code, but it is really -*- C++ -*-
/* 
Copyright (C) 1988, 1992 Free Software Foundation
    written by Doug Lea (dl@rocky.oswego.edu)
    converted to use iostream library by Per Bothner (bothner@cygnus.com)

This file is part of GNU CC.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY.  No author or distributor
accepts responsibility to anyone for the consequences of using it
or for whether it serves any particular purpose or works at all,
unless he says so in writing.  Refer to the GNU CC General Public
License for full details.

Everyone is granted permission to copy, modify and redistribute
GNU CC, but only under the conditions described in the
GNU CC General Public License.   A copy of this license is
supposed to have been given to you along with GNU CC so you
can know your rights and responsibilities.  It should be in a
file named COPYING.  Among other things, the copyright notice
and this notice must be preserved on all copies.  
*/

/* 
   a very simple implementation of a class to output unix "plot"
   format plotter files. See corresponding unix man pages for
   more details. 
*/

#ifndef _PlotFile_h
#ifdef __GNUG__
#pragma interface
#endif
#define _PlotFile_h

#include <fstream.h>

/*   
   Some plot libraries have the `box' command to draw boxes. Some don't.
   `box' is included here via moves & lines to allow both possiblilties.
*/


class PlotFile : public ofstream
{
protected:
  PlotFile& cmd(char c);
  PlotFile& operator << (const int x);
  PlotFile& operator << (const char *s);
  
public:
  
  PlotFile() : ofstream() { }
  PlotFile(int fd) : ofstream(fd) { }
  PlotFile(const char *name, int mode=ios::out, int prot=0664)
      : ofstream(name, mode, prot) { }
  
//  PlotFile& remove() { ofstream::remove(); return *this; }
  
//  int           filedesc() { return ofstream::filedesc(); }
//  const char*   name() { return File::name(); }
//  void          setname(const char* newname) { File::setname(newname); }
//  int           iocount() { return File::iocount(); }
  
  PlotFile& arc(const int xi, const int yi,
                const int x0, const int y0,
                const int x1, const int y1);
  PlotFile& box(const int x0, const int y0,
                const int x1, const int y1);
  PlotFile& circle(const int x, const int y, const int r);
  PlotFile& cont(const int xi, const int yi);
  PlotFile& dot(const int xi, const int yi, const int dx,
                int n, const int* pat);
  PlotFile& erase(); 
  PlotFile& label(const char* s);
  PlotFile& line(const int x0, const int y0,
                 const int x1, const int y1);
  PlotFile& linemod(const char* s);
  PlotFile& move(const int xi, const int yi);
  PlotFile& point(const int xi, const int yi);
  PlotFile& space(const int x0, const int y0,
                  const int x1, const int y1);
};
#endif

































