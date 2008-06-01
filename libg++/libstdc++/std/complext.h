// The template and inlines for the -*- C++ -*- complex number classes.
// Copyright (C) 1994 Free Software Foundation

// This file is part of the GNU ANSI C++ Library.  This library is free
// software; you can redistribute it and/or modify it under the terms of
// the GNU General Public License as published by the Free Software
// Foundation; either version 2, or (at your option) any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this library; see the file COPYING.  If not, write to the Free
// Software Foundation, 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

// As a special exception, if you link this library with files compiled
// with a GNU compiler to produce an executable, this does not cause the
// resulting executable to be covered by the GNU General Public License.
// This exception does not however invalidate any other reasons why the
// executable file might be covered by the GNU General Public License.

// Written by Jason Merrill based upon the specification in the 27 May 1994
// C++ working paper, ANSI document X3J16/94-0098.

#ifndef __COMPLEXT__
#define __COMPLEXT__

#ifdef __GNUG__
#pragma interface
#endif

#include <std/cmath.h>

#if ! defined (__GNUG__) && ! defined (__attribute__)
#define __attribute__ (foo) /* Ignore.  */
#endif

extern "C++" {
template <class FLOAT>
class complex
{
public:
  complex (FLOAT r = 0, FLOAT i = 0): re (r), im (i) { }
  complex& operator += (const complex&);
  complex& operator -= (const complex&);
  complex& operator *= (const complex&);
  complex& operator /= (const complex&);
  FLOAT real () const { return re; }
  FLOAT imag () const { return im; }
private:
  FLOAT re, im;

  // These functions are specified as friends for purposes of name injection;
  // they do not actually reference private members.
  friend FLOAT real (const complex&) __attribute__ ((const));
  friend FLOAT imag (const complex&) __attribute__ ((const));
  friend complex operator + (const complex&, const complex&) __attribute__ ((const));
  friend complex operator + (const complex&, FLOAT) __attribute__ ((const));
  friend complex operator + (FLOAT, const complex&) __attribute__ ((const));
  friend complex operator - (const complex&, const complex&) __attribute__ ((const));
  friend complex operator - (const complex&, FLOAT) __attribute__ ((const));
  friend complex operator - (FLOAT, const complex&) __attribute__ ((const));
  friend complex operator * (const complex&, const complex&) __attribute__ ((const));
  friend complex operator * (const complex&, FLOAT) __attribute__ ((const));
  friend complex operator * (FLOAT, const complex&) __attribute__ ((const));
  friend complex operator / (const complex&, const complex&) __attribute__ ((const));
  friend complex operator / (const complex&, FLOAT) __attribute__ ((const));
  friend complex operator / (FLOAT, const complex&) __attribute__ ((const));
  friend bool operator == (const complex&, const complex&) __attribute__ ((const));
  friend bool operator == (const complex&, FLOAT) __attribute__ ((const));
  friend bool operator == (FLOAT, const complex&) __attribute__ ((const));
  friend bool operator != (const complex&, const complex&) __attribute__ ((const));
  friend bool operator != (const complex&, FLOAT) __attribute__ ((const));
  friend bool operator != (FLOAT, const complex&) __attribute__ ((const));
  friend complex polar (FLOAT, FLOAT) __attribute__ ((const));
  friend complex pow (const complex&, const complex&) __attribute__ ((const));
  friend complex pow (const complex&, FLOAT) __attribute__ ((const));
  friend complex pow (const complex&, int) __attribute__ ((const));
  friend complex pow (FLOAT, const complex&) __attribute__ ((const));
  friend istream& operator>> (istream&, complex&);
  friend ostream& operator<< (ostream&, const complex&);
};

// Declare specializations.
class complex<float>;
class complex<double>;
class complex<long double>;

template <class FLOAT>
inline complex<FLOAT>&
complex<FLOAT>::operator += (const complex<FLOAT>& r)
{
  re += r.re;
  im += r.im;
  return *this;
}

template <class FLOAT>
inline complex<FLOAT>&
complex<FLOAT>::operator -= (const complex<FLOAT>& r)
{
  re -= r.re;
  im -= r.im;
  return *this;
}

template <class FLOAT>
inline complex<FLOAT>&
complex<FLOAT>::operator *= (const complex<FLOAT>& r)
{
  FLOAT f = re * r.re - im * r.im;
  im = re * r.im + im * r.re;
  re = f;
  return *this;
}

template <class FLOAT> inline FLOAT
imag (const complex<FLOAT>& x) __attribute__ ((const))
{
  return x.imag ();
}

template <class FLOAT> inline FLOAT
real (const complex<FLOAT>& x) __attribute__ ((const))
{
  return x.real ();
}

template <class FLOAT> inline complex<FLOAT>
operator + (const complex<FLOAT>& x, const complex<FLOAT>& y) __attribute__ ((const))
{
  return complex<FLOAT> (real (x) + real (y), imag (x) + imag (y));
}

template <class FLOAT> inline complex<FLOAT>
operator + (const complex<FLOAT>& x, FLOAT y) __attribute__ ((const))
{
  return complex<FLOAT> (real (x) + y, imag (x));
}

template <class FLOAT> inline complex<FLOAT>
operator + (FLOAT x, const complex<FLOAT>& y) __attribute__ ((const))
{
  return complex<FLOAT> (x + real (y), imag (y));
}

template <class FLOAT> inline complex<FLOAT>
operator - (const complex<FLOAT>& x, const complex<FLOAT>& y) __attribute__ ((const))
{
  return complex<FLOAT> (real (x) - real (y), imag (x) - imag (y));
}

template <class FLOAT> inline complex<FLOAT>
operator - (const complex<FLOAT>& x, FLOAT y) __attribute__ ((const))
{
  return complex<FLOAT> (real (x) - y, imag (x));
}

template <class FLOAT> inline complex<FLOAT>
operator - (FLOAT x, const complex<FLOAT>& y) __attribute__ ((const))
{
  return complex<FLOAT> (x - real (y), - imag (y));
}

template <class FLOAT> inline complex<FLOAT>
operator * (const complex<FLOAT>& x, const complex<FLOAT>& y) __attribute__ ((const))
{
  return complex<FLOAT> (real (x) * real (y) - imag (x) * imag (y),
			   real (x) * imag (y) + imag (x) * real (y));
}

template <class FLOAT> inline complex<FLOAT>
operator * (const complex<FLOAT>& x, FLOAT y) __attribute__ ((const))
{
  return complex<FLOAT> (real (x) * y, imag (x) * y);
}

template <class FLOAT> inline complex<FLOAT>
operator * (FLOAT x, const complex<FLOAT>& y) __attribute__ ((const))
{
  return complex<FLOAT> (x * real (y), x * imag (y));
}

template <class FLOAT> complex<FLOAT>
operator / (const complex<FLOAT>& x, FLOAT y) __attribute__ ((const))
{
  return complex<FLOAT> (real (x) / y, imag (x) / y);
}

template <class FLOAT> inline complex<FLOAT>
operator + (const complex<FLOAT>& x) __attribute__ ((const))
{
  return x;
}

template <class FLOAT> inline complex<FLOAT>
operator - (const complex<FLOAT>& x) __attribute__ ((const))
{
  return complex<FLOAT> (-real (x), -imag (x));
}

template <class FLOAT> inline bool
operator == (const complex<FLOAT>& x, const complex<FLOAT>& y) __attribute__ ((const))
{
  return real (x) == real (y) && imag (x) == imag (y);
}

template <class FLOAT> inline bool
operator == (const complex<FLOAT>& x, FLOAT y) __attribute__ ((const))
{
  return real (x) == y && imag (x) == 0;
}

template <class FLOAT> inline bool
operator == (FLOAT x, const complex<FLOAT>& y) __attribute__ ((const))
{
  return x == real (y) && imag (y) == 0;
}

template <class FLOAT> inline bool
operator != (const complex<FLOAT>& x, const complex<FLOAT>& y) __attribute__ ((const))
{
  return real (x) != real (y) || imag (x) != imag (y);
}

template <class FLOAT> inline bool
operator != (const complex<FLOAT>& x, FLOAT y) __attribute__ ((const))
{
  return real (x) != y || imag (x) != 0;
}

template <class FLOAT> inline bool
operator != (FLOAT x, const complex<FLOAT>& y) __attribute__ ((const))
{
  return x != real (y) || imag (y) != 0;
}

// Some targets don't provide a prototype for hypot when -ansi.
extern "C" double hypot (double, double) __attribute__ ((const));

template <class FLOAT> inline FLOAT
abs (const complex<FLOAT>& x) __attribute__ ((const))
{
  return hypot (real (x), imag (x));
}

template <class FLOAT> inline FLOAT
arg (const complex<FLOAT>& x) __attribute__ ((const))
{
  return atan2 (imag (x), real (x));
}

template <class FLOAT> inline complex<FLOAT>
polar (FLOAT r, FLOAT t) __attribute__ ((const))
{
  return complex<FLOAT> (r * cos (t), r * sin (t));
}

template <class FLOAT> inline complex<FLOAT>
conj (const complex<FLOAT>& x)  __attribute__ ((const))
{
  return complex<FLOAT> (real (x), -imag (x));
}

template <class FLOAT> inline FLOAT
norm (const complex<FLOAT>& x) __attribute__ ((const))
{
  return real (x) * real (x) + imag (x) * imag (x);
}

// Declarations of templates in complext.ccI

template <class FLOAT> complex<FLOAT>
  operator / (const complex<FLOAT>&, const complex<FLOAT>&) __attribute__ ((const));
template <class FLOAT> complex<FLOAT>
  operator / (FLOAT, const complex<FLOAT>&) __attribute__ ((const));
template <class FLOAT> complex<FLOAT>
  cos (const complex<FLOAT>&) __attribute__ ((const));
template <class FLOAT> complex<FLOAT>
  cosh (const complex<FLOAT>&) __attribute__ ((const));
template <class FLOAT> complex<FLOAT>
  exp (const complex<FLOAT>&) __attribute__ ((const));
template <class FLOAT> complex<FLOAT>
  log (const complex<FLOAT>&) __attribute__ ((const));
template <class FLOAT> complex<FLOAT>
  pow (const complex<FLOAT>&, const complex<FLOAT>&) __attribute__ ((const));
template <class FLOAT> complex<FLOAT>
  pow (const complex<FLOAT>&, FLOAT) __attribute__ ((const));
template <class FLOAT> complex<FLOAT>
  pow (const complex<FLOAT>&, int) __attribute__ ((const));
template <class FLOAT> complex<FLOAT>
  pow (FLOAT, const complex<FLOAT>&) __attribute__ ((const));
template <class FLOAT> complex<FLOAT>
  sin (const complex<FLOAT>&) __attribute__ ((const));
template <class FLOAT> complex<FLOAT>
  sinh (const complex<FLOAT>&) __attribute__ ((const));
template <class FLOAT> complex<FLOAT>
  sqrt (const complex<FLOAT>&) __attribute__ ((const));

class istream;
class ostream;
template <class FLOAT> istream& operator >> (istream&, complex<FLOAT>&);
template <class FLOAT> ostream& operator << (ostream&, const complex<FLOAT>&);
} // extern "C++"

// Specializations and such

#include <std/fcomplex.h>
#include <std/dcomplex.h>
#include <std/ldcomplex.h>

// Declare the instantiations.
#include <std/cinst.h>

#endif
