// Forward declarations of -*- C++ -*- complex number instantiations.
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

// Written by Jason Merrill based upon the specification in the 27 May 1994
// C++ working paper, ANSI document X3J16/94-0098.

#ifndef __CINST__
#define __CINST__

#ifndef _G_NO_EXTERN_TEMPLATES
extern "C++" {
extern template class complex<float>;
extern template class complex<double>;
extern template class complex<long double>;

#define __B(type) bool
#define __C(type) complex<type>
#define __F(type) type
#define __I(type) int
#define __IS(type) istream&
#define __OS(type) ostream&
#define __CR(type) complex<type>&
#define __CCR(type) const complex<type>&

#define __D2(name,type,ret,arg1,arg2) ret(type) name (arg1(type), arg2(type));

#define __S1(name,type,ret,arg1) \
  extern template ret(type) name (arg1(type));
#define __S2(name,type,ret,arg1,arg2) \
  extern template __D2 (name,type,ret,arg1,arg2)

#define __DO1(name,ret,arg1) \
  __S1(name,float,ret,arg1) \
  __S1(name,double,ret,arg1) \
  __S1(name,long double,ret,arg1)
#define __DO2(name,ret,arg1,arg2) \
  __S2(name,float,ret,arg1,arg2) \
  __S2(name,double,ret,arg1,arg2) \
  __S2(name,long double,ret,arg1,arg2)

#define __DOCCC(name) __DO2(name,__C,__CCR,__CCR)
#define __DOCCF(name) __DO2(name,__C,__CCR,__F)
#define __DOCFC(name) __DO2(name,__C,__F,__CCR)
#define __DOCFF(name) __DO2(name,__C,__F,__F)
#define __DOBCC(name) __DO2(name,__B,__CCR,__CCR)
#define __DOBCF(name) __DO2(name,__B,__CCR,__F)
#define __DOBFC(name) __DO2(name,__B,__F,__CCR)
#define __DOFC(name) __DO1(name,__F,__CCR)
#define __DOCC(name) __DO1(name,__C,__CCR)

__DO2(operator+,__C,__CCR,__CCR)
__DO2(operator+,__C,__CCR,__F)
__DO2(operator+,__C,__F,__CCR)
__DO2(operator-,__C,__CCR,__CCR)
__DO2(operator-,__C,__CCR,__F)
__DO2(operator-,__C,__F,__CCR)
__DO2(operator*,__C,__CCR,__CCR)
__DO2(operator*,__C,__CCR,__F)
__DO2(operator*,__C,__F,__CCR)
__DO2(operator/,__C,__CCR,__F)
__DO1(operator+,__C,__CCR)
__DO1(operator-,__C,__CCR)
__DO2(operator==,__B,__CCR,__CCR)
__DO2(operator==,__B,__CCR,__F)
__DO2(operator==,__B,__F,__CCR)
__DO2(operator!=,__B,__CCR,__CCR)
__DO2(operator!=,__B,__CCR,__F)
__DO2(operator!=,__B,__F,__CCR)
__DO1(abs,__F,__CCR)
__DO1(arg,__F,__CCR)
__DO2(polar,__C,__F,__F)
__DO1(conj,__C,__CCR)
__DO1(norm,__F,__CCR)

#undef __DO1
#undef __DO2
#undef __S1
#undef __S2
#undef __D2
#undef __B
#undef __C
#undef __F
#undef __I
#undef __IS
#undef __OS
#undef __CR
#undef __CCR
} // extern "C++"
#endif

#endif
