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

#ifndef _Rational_h
#ifdef __GNUG__
#pragma interface
#endif
#define _Rational_h 1

#include <Integer.h>
#include <math.h>

class Rational
{
protected:
  Integer          num;
  Integer          den;

  void             normalize();

public:
                   Rational();
                   Rational(double);
                   Rational(int n);
                   Rational(long n);
                   Rational(int n, int d);
                   Rational(long n, long d);
                   Rational(long n, unsigned long d);
                   Rational(unsigned long n, long d);
                   Rational(unsigned long n, unsigned long d);
                   Rational(const Integer& n);
                   Rational(const Integer& n, const Integer& d);
                   Rational(const Rational&);

                  ~Rational();

  void             operator =  (const Rational& y);

  friend int       operator == (const Rational& x, const Rational& y);
  friend int       operator != (const Rational& x, const Rational& y);
  friend int       operator <  (const Rational& x, const Rational& y);
  friend int       operator <= (const Rational& x, const Rational& y);
  friend int       operator >  (const Rational& x, const Rational& y);
  friend int       operator >= (const Rational& x, const Rational& y);

  friend Rational  operator +  (const Rational& x, const Rational& y);
  friend Rational  operator -  (const Rational& x, const Rational& y);
  friend Rational  operator *  (const Rational& x, const Rational& y);
  friend Rational  operator /  (const Rational& x, const Rational& y);

  void             operator += (const Rational& y);
  void             operator -= (const Rational& y);
  void             operator *= (const Rational& y);
  void             operator /= (const Rational& y);

#ifdef __GNUG__
  friend Rational  operator <? (const Rational& x, const Rational& y); // min
  friend Rational  operator >? (const Rational& x, const Rational& y); // max
#endif

  friend Rational  operator - (const Rational& x);


// builtin Rational functions


  void             negate();                      // x = -x
  void             invert();                      // x = 1/x

  friend int       sign(const Rational& x);             // -1, 0, or +1
  friend Rational  abs(const Rational& x);              // absolute value
  friend Rational  sqr(const Rational& x);              // square
  friend Rational  pow(const Rational& x, long y);
  friend Rational  pow(const Rational& x, const Integer& y);
  const Integer&   numerator() const;
  const Integer&   denominator() const;

// coercion & conversion

                   operator double() const;
  friend Integer   floor(const Rational& x);
  friend Integer   ceil(const Rational& x);
  friend Integer   trunc(const Rational& x);
  friend Integer   round(const Rational& x);

  friend istream&  operator >> (istream& s, Rational& y);
  friend ostream&  operator << (ostream& s, const Rational& y);

  int		   fits_in_float() const;
  int		   fits_in_double() const;

// procedural versions of operators

  friend int       compare(const Rational& x, const Rational& y);
  friend void      add(const Rational& x, const Rational& y, Rational& dest);
  friend void      sub(const Rational& x, const Rational& y, Rational& dest);
  friend void      mul(const Rational& x, const Rational& y, Rational& dest);
  friend void      div(const Rational& x, const Rational& y, Rational& dest);

// error detection

  void    error(const char* msg) const;
  int              OK() const;

};

typedef Rational RatTmp; // backwards compatibility

inline Rational::Rational() : num(&_ZeroRep), den(&_OneRep) {}
inline Rational::~Rational() {}

inline Rational::Rational(const Rational& y) :num(y.num), den(y.den) {}

inline Rational::Rational(const Integer& n) :num(n), den(&_OneRep) {}

inline Rational::Rational(const Integer& n, const Integer& d) :num(n),den(d)
{
  normalize();
}

inline Rational::Rational(long n) :num(n), den(&_OneRep) { }

inline Rational::Rational(int n) :num(n), den(&_OneRep) { }

inline Rational::Rational(long n, long d) :num(n), den(d) { normalize(); }
inline Rational::Rational(int n, int d) :num(n), den(d) { normalize(); }
inline Rational::Rational(long n, unsigned long d) :num(n), den(d)
{
  normalize();
}
inline Rational::Rational(unsigned long n, long d) :num(n), den(d)
{
  normalize();
}
inline Rational::Rational(unsigned long n, unsigned long d) :num(n), den(d)
{
  normalize();
}

inline  void Rational::operator =  (const Rational& y)
{
  num = y.num;  den = y.den;
}

inline int operator == (const Rational& x, const Rational& y)
{
  return compare(x.num, y.num) == 0 && compare(x.den, y.den) == 0;
}

inline int operator != (const Rational& x, const Rational& y)
{
  return compare(x.num, y.num) != 0 || compare(x.den, y.den) != 0;
}

inline int operator <  (const Rational& x, const Rational& y)
{
  return compare(x, y) <  0; 
}

inline int operator <= (const Rational& x, const Rational& y)
{
  return compare(x, y) <= 0; 
}

inline int operator >  (const Rational& x, const Rational& y)
{
  return compare(x, y) >  0; 
}

inline int operator >= (const Rational& x, const Rational& y)
{
  return compare(x, y) >= 0; 
}

inline int sign(const Rational& x)
{
  return sign(x.num);
}

inline void Rational::negate()
{
  num.negate();
}


inline void Rational::operator += (const Rational& y) 
{
  add(*this, y, *this);
}

inline void Rational::operator -= (const Rational& y) 
{
  sub(*this, y, *this);
}

inline void Rational::operator *= (const Rational& y) 
{
  mul(*this, y, *this);
}

inline void Rational::operator /= (const Rational& y) 
{
  div(*this, y, *this);
}

inline const Integer& Rational::numerator() const { return num; }
inline const Integer& Rational::denominator() const { return den; }
inline Rational::operator double() const { return ratio(num, den); }

#ifdef __GNUG__
inline Rational operator <? (const Rational& x, const Rational& y)
{
  if (compare(x, y) <= 0) return x; else return y;
}

inline Rational operator >? (const Rational& x, const Rational& y)
{
  if (compare(x, y) >= 0) return x; else return y;
}
#endif

#if defined(__GNUG__) && !defined(NO_NRV)

inline Rational operator + (const Rational& x, const Rational& y) return r
{
  add(x, y, r);
}

inline Rational operator - (const Rational& x, const Rational& y) return r
{
  sub(x, y, r);
}

inline Rational operator * (const Rational& x, const Rational& y) return r
{
  mul(x, y, r);
}

inline Rational operator / (const Rational& x, const Rational& y) return r
{
  div(x, y, r);
}

#else /* NO_NRV */

inline Rational operator + (const Rational& x, const Rational& y) 
{
  Rational r; add(x, y, r); return r;
}

inline Rational operator - (const Rational& x, const Rational& y)
{
  Rational r; sub(x, y, r); return r;
}

inline Rational operator * (const Rational& x, const Rational& y)
{
  Rational r; mul(x, y, r); return r;
}

inline Rational operator / (const Rational& x, const Rational& y)
{
  Rational r; div(x, y, r); return r;
}
#endif

#endif
