// Forward declarations of -*- C++ -*- string instantiations.
// Copyright (C) 1994 Free Software Foundation

// This file is part of the GNU ANSI C++ Library.  This library is free
// software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the
// Free Software Foundation; either version 2, or (at your option)
// any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this library; see the file COPYING.  If not, write to the Free
// Software Foundation, 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

// As a special exception, if you link this library with files
// compiled with a GNU compiler to produce an executable, this does not cause
// the resulting executable to be covered by the GNU General Public License.
// This exception does not however invalidate any other reasons why
// the executable file might be covered by the GNU General Public License.

// Written by Jason Merrill based upon the specification by Takanori Adachi
// in ANSI X3J16/94-0013R2.

#ifndef __SINST__
#define __SINST__

extern "C++" {
#define __S basic_string<char,string_char_traits<char> >
//#define __W basic_string<wchar_t,string_char_traits<wchar_t> >

extern template class __bsrep<char, string_char_traits<char> >;
extern template class __S;
// extern template class __W;
// extern template class __bsrep<wchar_t, string_char_traits<wchar_t> >;

#define __DOPR(op, ret, c, s) \
  extern template ret operator op (const s&, const s&); \
  extern template ret operator op (const c*, const s&); \
  extern template ret operator op (const s&, const c*); \

#define __DO(op, ret, c, s) \
  extern template ret operator op (const s&, const s&); \
  extern template ret operator op (const c*, const s&); \
  extern template ret operator op (const s&, const c*); \
  extern template ret operator op (c, const s&); \
  extern template ret operator op (const s&, c);

__DO (+, __S, char, __S)
// __DO (+, __W, wchar_t, __W) */

#define __DOB(op) \
  __DOPR (op, bool, char, __S)
//  __DOPR (op, bool, wchar_t, __W)

__DOB (==)
__DOB (!=)
__DOB (<)
__DOB (>)
__DOB (<=)
__DOB (>=)

#undef __S
//#undef __W
#undef __DO
#undef __DOB
#undef __DOPR
} // extern "C++"

#endif
