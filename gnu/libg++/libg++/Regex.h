// This may look like C code, but it is really -*- C++ -*-
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


#ifndef _Regex_h
#ifdef __GNUG__
#pragma interface
#endif
#define _Regex_h 1

#if defined(SHORT_NAMES) || defined(VMS)
#define re_compile_pattern	recmppat
#define re_pattern_buffer	repatbuf
#define re_registers		reregs
#endif

struct re_pattern_buffer;       // defined elsewhere
struct re_registers;

class Regex
{
private:

                     Regex(const Regex&) {}  // no X(X&)
  void               operator = (const Regex&) {} // no assignment

protected:
  re_pattern_buffer* buf;
  re_registers*      reg;

public:
                     Regex(const char* t, 
                           int fast = 0, 
                           int bufsize = 40, 
                           const char* transtable = 0);

                    ~Regex();

  int                match(const char* s, int len, int pos = 0) const;
  int                search(const char* s, int len, 
                            int& matchlen, int startpos = 0) const;
  int                match_info(int& start, int& length, int nth = 0) const;

  int                OK() const;  // representation invariant
};

// some built in regular expressions

extern const Regex RXwhite;          // = "[ \n\t\r\v\f]+"
extern const Regex RXint;            // = "-?[0-9]+"
extern const Regex RXdouble;         // = "-?\\(\\([0-9]+\\.[0-9]*\\)\\|
                                     //    \\([0-9]+\\)\\|\\(\\.[0-9]+\\)\\)
                                     //    \\([eE][---+]?[0-9]+\\)?"
extern const Regex RXalpha;          // = "[A-Za-z]+"
extern const Regex RXlowercase;      // = "[a-z]+"
extern const Regex RXuppercase;      // = "[A-Z]+"
extern const Regex RXalphanum;       // = "[0-9A-Za-z]+"
extern const Regex RXidentifier;     // = "[A-Za-z_][A-Za-z0-9_]*"


#endif
