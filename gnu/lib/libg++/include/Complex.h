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

#ifndef _Complex_h
#ifdef __GNUG__
#pragma interface
#endif
#define _Complex_h 1


#include <iostream.h>
#include <math.h>

class Complex
{
#ifdef __ATT_complex__
public:
#else
protected:
#endif

  double           re;
  double           im;

public:

  double           real() const;
  double           imag() const;

                   Complex();
                   Complex(const Complex& y);
                   Complex(double r, double i=0);

                  ~Complex();

  Complex&         operator =  (const Complex& y);

  Complex&         operator += (const Complex& y);
  Complex&         operator += (double y);
  Complex&         operator -= (const Complex& y);
  Complex&         operator -= (double y);
  Complex&         operator *= (const Complex& y);
  Complex&         operator *= (double y);

  Complex&         operator /= (const Complex& y); 
  Complex&         operator /= (double y); 

  void             error(const char* msg) const;
};


// non-inline functions

Complex   operator /  (const Complex& x, const Complex& y);
Complex   operator /  (const Complex& x, double y);
Complex   operator /  (double   x, const Complex& y);

Complex   cos(const Complex& x);
Complex   sin(const Complex& x);

Complex   cosh(const Complex& x);
Complex   sinh(const Complex& x);

Complex   exp(const Complex& x);
Complex   log(const Complex& x);

Complex   pow(const Complex& x, int p);
Complex   pow(const Complex& x, const Complex& p);
Complex   pow(const Complex& x, double y);
Complex   sqrt(const Complex& x);
   
istream&  operator >> (istream& s, Complex& x);
ostream&  operator << (ostream& s, const Complex& x);

// other functions defined as inlines

int  operator == (const Complex& x, const Complex& y);
int  operator == (const Complex& x, double y);
int  operator != (const Complex& x, const Complex& y);
int  operator != (const Complex& x, double y);

Complex  operator - (const Complex& x);
Complex  conj(const Complex& x);
Complex  operator + (const Complex& x, const Complex& y);
Complex  operator + (const Complex& x, double y);
Complex  operator + (double x, const Complex& y);
Complex  operator - (const Complex& x, const Complex& y);
Complex  operator - (const Complex& x, double y);
Complex  operator - (double x, const Complex& y);
Complex  operator * (const Complex& x, const Complex& y);
Complex  operator * (const Complex& x, double y);
Complex  operator * (double x, const Complex& y);

double  real(const Complex& x);
double  imag(const Complex& x);
double  abs(const Complex& x);
double  norm(const Complex& x);
double  arg(const Complex& x);

Complex  polar(double r, double t = 0.0);


// inline members

inline double  Complex::real() const { return re; }
inline double  Complex::imag() const { return im; }

inline Complex::Complex() {}
inline Complex::Complex(const Complex& y) :re(y.real()), im(y.imag()) {}
inline Complex::Complex(double r, double i) :re(r), im(i) {}

inline Complex::~Complex() {}

inline Complex&  Complex::operator =  (const Complex& y) 
{ 
  re = y.real(); im = y.imag(); return *this; 
} 

inline Complex&  Complex::operator += (const Complex& y)
{ 
  re += y.real();  im += y.imag(); return *this; 
}

inline Complex&  Complex::operator += (double y)
{ 
  re += y; return *this; 
}

inline Complex&  Complex::operator -= (const Complex& y)
{ 
  re -= y.real();  im -= y.imag(); return *this; 
}

inline Complex&  Complex::operator -= (double y)
{ 
  re -= y; return *this; 
}

inline Complex&  Complex::operator *= (const Complex& y)
{  
  double r = re * y.real() - im * y.imag();
  im = re * y.imag() + im * y.real(); 
  re = r; 
  return *this; 
}

inline Complex&  Complex::operator *= (double y)
{  
  re *=  y; im *=  y; return *this; 
}


//  functions

inline int  operator == (const Complex& x, const Complex& y)
{
  return x.real() == y.real() && x.imag() == y.imag();
}

inline int  operator == (const Complex& x, double y)
{
  return x.imag() == 0.0 && x.real() == y;
}

inline int  operator != (const Complex& x, const Complex& y)
{
  return x.real() != y.real() || x.imag() != y.imag();
}

inline int  operator != (const Complex& x, double y)
{
  return x.imag() != 0.0 || x.real() != y;
}

inline Complex  operator - (const Complex& x)
{
  return Complex(-x.real(), -x.imag());
}

inline Complex  conj(const Complex& x)
{
  return Complex(x.real(), -x.imag());
}

inline Complex  operator + (const Complex& x, const Complex& y)
{
  return Complex(x.real() + y.real(), x.imag() + y.imag());
}

inline Complex  operator + (const Complex& x, double y)
{
  return Complex(x.real() + y, x.imag());
}

inline Complex  operator + (double x, const Complex& y)
{
  return Complex(x + y.real(), y.imag());
}

inline Complex  operator - (const Complex& x, const Complex& y)
{
  return Complex(x.real() - y.real(), x.imag() - y.imag());
}

inline Complex  operator - (const Complex& x, double y)
{
  return Complex(x.real() - y, x.imag());
}

inline Complex  operator - (double x, const Complex& y)
{
  return Complex(x - y.real(), -y.imag());
}

inline Complex  operator * (const Complex& x, const Complex& y)
{
  return Complex(x.real() * y.real() - x.imag() * y.imag(), 
                 x.real() * y.imag() + x.imag() * y.real());
}

inline Complex  operator * (const Complex& x, double y)
{
  return Complex(x.real() * y, x.imag() * y);
}

inline Complex  operator * (double x, const Complex& y)
{
  return Complex(x * y.real(), x * y.imag());
}

inline double  real(const Complex& x)
{
  return x.real();
}

inline double  imag(const Complex& x)
{
  return x.imag();
}

inline double  abs(const Complex& x)
{
  return hypot(x.real(), x.imag());
}

inline double  norm(const Complex& x)
{
  return (x.real() * x.real() + x.imag() * x.imag());
}

inline double  arg(const Complex& x)
{
  return atan2(x.imag(), x.real());
}

inline Complex  polar(double r, double t)
{
  return Complex(r * cos(t), r * sin(t));
}

#endif
