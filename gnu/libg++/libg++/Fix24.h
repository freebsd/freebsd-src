// This may look like C code, but it is really -*- C++ -*-
/* 
Copyright (C) 1988 Free Software Foundation
    written by Kurt Baudendistel (gt-eedsp!baud@gatech.edu)
    adapted for libg++ by Doug Lea (dl@rocky.oswego.edu)

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

#ifndef _Fix24_h
#ifdef __GNUG__
#pragma interface
#endif
#define _Fix24_h 1

#include <stream.h>
#include <std.h>

// extra type definitions 

typedef struct {
  long                 u;
  unsigned long	       l;
} twolongs;

// constant definitions

static const int
  Fix24_shift = 31;
		  
static const double
  Fix24_fs = 2147483648.,		// 2^Fix24_shift
  Fix24_mult = Fix24_fs,
  Fix24_div = 1./Fix24_fs,
  Fix24_max = 1. - .5/Fix24_fs,
  Fix24_min = -1.;
	  
static const unsigned long
  Fix24_msb = 0x80000000L,
  Fix24_lsb = 0x00000100L,
  Fix24_m_max = 0x7fffff00L,
  Fix24_m_min = 0x80000000L;

static const double
  Fix48_fs = 36028797018963968.,	// 2^(24+Fix24_shift)
  Fix48_max = 1. - .5/Fix48_fs,
  Fix48_min = -1.,
  Fix48_div_u = 1./Fix24_fs,
  Fix48_div_l = 1./Fix48_fs;
   
static const twolongs
  Fix48_msb = { 0x80000000L, 0L },
  Fix48_lsb = { 0L, 0x00000100L },
  Fix48_m_max = { 0x7fffff00L, 0xffffff00L },
  Fix48_m_min = { 0x80000000L, 0L };
		  
//
// Fix24    class: 24-bit Fixed point data type
//
//	consists of a 24-bit mantissa (sign bit & 23 data bits).
//

class Fix24 
{ 
  friend class          Fix48;

  long                  m;

  long                  assign(double d);
         operator       double() const;
                        Fix24(long i);
                        Fix24(int i);


public:
                        Fix24();
                        Fix24(const Fix24&  f);
                        Fix24(double d);
                        Fix24(const Fix48& f);

                        ~Fix24();

  Fix24&                operator=(const Fix24&  f);
  Fix24&                operator=(double d);
  Fix24&                operator=(const Fix48& f);

  friend long&          mantissa(Fix24&  f);
  friend const long&    mantissa(const Fix24&  f);
  friend double         value(const Fix24&  f);

  Fix24                 operator +  () const;
  Fix24                 operator -  () const;

  friend Fix24          operator +  (const Fix24&  f, const Fix24&  g);
  friend Fix24          operator -  (const Fix24&  f, const Fix24&  g);
  friend Fix48          operator *  (const Fix24&  f, const Fix24&  g);
  friend Fix24          operator *  (const Fix24&  f, int     g);
  friend Fix24          operator *  (int     g, const Fix24&  f);
  friend Fix24          operator /  (const Fix24&  f, const Fix24&  g);
  friend Fix24          operator << (const Fix24&  f, int b);
  friend Fix24          operator >> (const Fix24&  f, int b);

  Fix24&                operator += (const Fix24&  f);
  Fix24&                operator -= (const Fix24&  f);
  Fix24&                operator *= (const Fix24&  f);
  Fix24&                operator *= (int     b);
  Fix24&                operator /= (const Fix24&  f);

  Fix24&                operator <<=(int b);
  Fix24&                operator >>=(int b);

  friend int            operator == (const Fix24&  f, const Fix24&  g);
  friend int            operator != (const Fix24&  f, const Fix24&  g);
  friend int            operator >= (const Fix24&  f, const Fix24&  g);
  friend int            operator <= (const Fix24&  f, const Fix24&  g);
  friend int            operator >  (const Fix24&  f, const Fix24&  g);
  friend int            operator <  (const Fix24&  f, const Fix24&  g);

  friend istream&       operator >> (istream& s, Fix24&  f);
  friend ostream&       operator << (ostream& s, const Fix24&  f);

  void                  overflow(long&) const;
  void                  range_error(long&) const;
};

 
//
// Fix48 class: 48-bit Fixed point data type
//
//	consists of a 48-bit mantissa (sign bit & 47 data bits).
//

class Fix48 
{ 
  friend class         Fix24;

  twolongs             m;

  twolongs             assign(double d);
         operator      double() const;
                       Fix48(twolongs i);

public:
                       Fix48();
                       Fix48(const Fix48& f);
                       Fix48(const Fix24&  f);
                       Fix48(double d);
                       ~Fix48();

  Fix48&               operator =  (const Fix48& f);
  Fix48&               operator =  (const Fix24&  f);
  Fix48&               operator =  (double d);

  friend twolongs&     mantissa(Fix48& f);
  friend const twolongs&  mantissa(const Fix48& f);
  friend double        value(const Fix48& f);

  Fix48                operator +  () const;
  Fix48                operator -  () const;

  friend Fix48         operator +  (const Fix48& f, const Fix48& g);
  friend Fix48         operator -  (const Fix48& f, const Fix48& g);
  friend Fix48         operator *  (const Fix48& f, int    g);
  friend Fix48         operator *  (int    g, const Fix48& f);
  friend Fix48         operator << (const Fix48& f, int b);
  friend Fix48         operator >> (const Fix48& f, int b);

  friend Fix48         operator *  (const Fix24&  f, const Fix24&  g);

  Fix48&               operator += (const Fix48& f);
  Fix48&               operator -= (const Fix48& f);
  Fix48&               operator *= (int    b);
  Fix48&               operator <<=(int b);
  Fix48&               operator >>=(int b);

  friend int           operator == (const Fix48& f, const Fix48& g);
  friend int           operator != (const Fix48& f, const Fix48& g);
  friend int           operator >= (const Fix48& f, const Fix48& g);
  friend int           operator <= (const Fix48& f, const Fix48& g);
  friend int           operator >  (const Fix48& f, const Fix48& g);
  friend int           operator <  (const Fix48& f, const Fix48& g);

  friend istream&      operator >> (istream& s, Fix48& f);
  friend ostream&      operator << (ostream& s, const Fix48& f);

  void                 overflow(twolongs& i) const;
  void                 range_error(twolongs& i) const;
};


// active error handler declarations

typedef void (*Fix24_peh)(long&);
typedef void (*Fix48_peh)(twolongs&);

extern Fix24_peh Fix24_overflow_handler;
extern Fix48_peh Fix48_overflow_handler;

extern Fix24_peh Fix24_range_error_handler;
extern Fix48_peh Fix48_range_error_handler;


// error handler declarations

#if defined(SHORT_NAMES) || defined(VMS)
#define	set_overflow_handler	sohndl
#define set_range_error_handler	srnghdl
#endif

extern Fix24_peh set_Fix24_overflow_handler(Fix24_peh);
extern Fix48_peh set_Fix48_overflow_handler(Fix48_peh);
extern void set_overflow_handler(Fix24_peh, Fix48_peh);

extern Fix24_peh set_Fix24_range_error_handler(Fix24_peh);
extern Fix48_peh set_Fix48_range_error_handler(Fix48_peh);
extern void set_range_error_handler(Fix24_peh, Fix48_peh);

extern void
  Fix24_ignore(long&),
  Fix24_overflow_saturate(long&),
  Fix24_overflow_warning_saturate(long&),
  Fix24_warning(long&),
  Fix24_abort(long&);

extern void
  Fix48_ignore(twolongs&),
  Fix48_overflow_saturate(twolongs&),
  Fix48_overflow_warning_saturate(twolongs&),
  Fix48_warning(twolongs&),
  Fix48_abort(twolongs&);


inline Fix24::~Fix24() {}

inline Fix24::Fix24(long i)		
{ 
  m = i; 
}

inline Fix24::Fix24(int i)	    
{ 
  m = i; 
}

inline Fix24::operator double() const
{ 
  return  Fix24_div * m; 
}

inline Fix24::Fix24()     			
{ 
  m = 0; 
}

inline Fix24::Fix24(const Fix24&  f)		
{ 
  m = f.m; 
}

inline Fix24::Fix24(double d)		
{
  m = assign(d);
}

inline Fix24::Fix24(const Fix48& f)		
{ 
  m = f.m.u;
}

inline Fix24&  Fix24::operator=(const Fix24&  f)	
{ 
  m = f.m; 
  return *this; 
}

inline Fix24&  Fix24::operator=(double d) 
{ 
  m = assign(d); 
  return *this; 
}

inline Fix24&  Fix24::operator=(const Fix48& f)
{ 
  m = f.m.u;
  return *this; 
}

inline long& mantissa(Fix24&  f)
{ 
  return f.m; 
}

inline const long& mantissa(const Fix24&  f)
{ 
  return f.m; 
}

inline double value(const Fix24&  f)
{ 
  return double(f); 
}

inline Fix24 Fix24::operator+() const		
{ 
  return m; 
}

inline Fix24 Fix24::operator-() const
{ 
  return -m; 
}

inline Fix24 operator+(const Fix24&  f, const Fix24&  g) 
{
  long sum = f.m + g.m;
  if ( (f.m ^ sum) & (g.m ^ sum) & Fix24_msb )
    f.overflow(sum);
  return sum;
}

inline Fix24 operator-(const Fix24&  f, const Fix24&  g) 
{
  long sum = f.m - g.m;
  if ( (f.m ^ sum) & (-g.m ^ sum) & Fix24_msb )
    f.overflow(sum);
  return sum;
}

inline Fix24 operator*(const Fix24& a, int b) 	
{ 
  return a.m * b; 
}

inline Fix24 operator*(int b, const Fix24& a) 	
{ 
  return a * b; 
}

inline Fix24 operator<<(const Fix24& a, int b) 	
{ 
  return a.m << b; 
}

inline Fix24 operator>>(const Fix24& a, int b) 	
{ 
  return (a.m >> b) & 0xffffff00L; 
}

inline  Fix24&  Fix24:: operator+=(const Fix24&  f)
{
  return *this = *this + f;
}

inline Fix24&  Fix24:: operator-=(const Fix24&  f) 	
{ 
  return *this = *this - f; 
}

inline Fix24& Fix24::operator*=(const Fix24& f) 	
{ 
  return *this = *this * f; 
}

inline Fix24&  Fix24:: operator/=(const Fix24&  f) 	
{ 
  return *this = *this / f; 
}

inline Fix24&  Fix24:: operator<<=(int b)	
{ 
  return *this = *this << b;
}

inline Fix24&  Fix24:: operator>>=(int b)	
{ 
  return *this = *this >> b;
}

inline Fix24& Fix24::operator*=(int b)
{ 
  return *this = *this * b; 
}

inline int operator==(const Fix24&  f, const Fix24&  g)	
{ 
  return f.m == g.m;
}

inline int operator!=(const Fix24&  f, const Fix24&  g)	
{ 
  return f.m != g.m;
}

inline int operator>=(const Fix24&  f, const Fix24&  g)	
{ 
  return f.m >= g.m;
}

inline int operator<=(const Fix24&  f, const Fix24&  g)	
{ 
  return f.m <= g.m;
}

inline int operator>(const Fix24&  f, const Fix24&  g)	
{ 
  return f.m > g.m;
}

inline int operator<(const Fix24&  f, const Fix24&  g)	
{ 
  return f.m < g.m;
}

inline istream&  operator>>(istream& s, Fix24&  f)
{ 
  double d;
  s >> d; 
  f = d; 
  return s; 
}

inline ostream&  operator<<(ostream& s, const Fix24&  f)
{ 
  return s << double(f);
}

inline Fix48::~Fix48() {}

inline Fix48::Fix48(twolongs i)		
{ 
  m = i;
}

inline Fix48:: operator double() const
{ 
/*
 * Note: can't simply do Fix48_div_u * m.u + Fix48_div_l * m.l, because
 * m.u is signed and m.l is unsigned.
 */
  return (m.u >= 0)? Fix48_div_u * m.u + Fix48_div_l * m.l :
      (Fix48_div_u * ((unsigned long)(m.u & 0xffffff00)) 
	  + Fix48_div_l * m.l) - 2;
}

inline Fix48::Fix48()			    
{ 
  m.u = 0;
  m.l = 0;
}

inline Fix48::Fix48(const Fix48& f)		
{ 
  m = f.m;
}

inline Fix48::Fix48(const Fix24&  f)    
{ 
  m.u = f.m;
  m.l = 0;
}

inline Fix48::Fix48(double d)		
{ 
  m = assign(d);
}

inline Fix48& Fix48::operator=(const Fix48& f)	
{ 
  m = f.m;
  return *this; 
}

inline Fix48& Fix48::operator=(const Fix24&  f)	
{ 
  m.u = f.m;
  m.l = 0;
  return *this;
}

inline Fix48& Fix48::operator=(double d)	
{ 
  m = assign(d);
  return *this; 
}

inline twolongs& mantissa(Fix48& f)	
{ 
  return f.m;
}

inline const twolongs& mantissa(const Fix48& f)	
{ 
  return f.m;
}

inline double value(const Fix48& f)
{ 
  return double(f);
}

inline Fix48 Fix48::operator+() const		
{ 
  return m;
}

inline Fix48 Fix48::operator-() const
{ 
  twolongs n;
  n.l = -m.l;
  n.u = ~m.u + ((n.l ^ m.l) & Fix24_msb ? 0 : Fix24_lsb);
  return Fix48(n);
}

inline Fix48 operator*(int b, const Fix48& a) 	
{ 
  return a * b; 
}

inline Fix48& Fix48::operator+=(const Fix48& f) 	
{ 
  return *this = *this + f;
}

inline Fix48& Fix48::operator-=(const Fix48& f) 	
{ 
  return *this = *this - f;
}

inline Fix48& Fix48::operator*=(int b)	
{ 
  return *this = *this * b;
}

inline Fix48& Fix48::operator<<=(int b)	
{ 
  return *this = *this << b;
}

inline Fix48& Fix48::operator>>=(int b)	
{ 
  return *this = *this >> b;
}

inline int operator==(const Fix48& f, const Fix48& g)	
{ 
  return f.m.u == g.m.u && f.m.l == g.m.l;
}

inline int operator!=(const Fix48& f, const Fix48& g)	
{ 
  return f.m.u != g.m.u || f.m.l != g.m.l;
}

inline int operator>=(const Fix48& f, const Fix48& g)	
{ 
  return f.m.u >= g.m.u || (f.m.u == g.m.u && f.m.l >= g.m.l);
}

inline int operator<=(const Fix48& f, const Fix48& g)	
{ 
  return f.m.u <= g.m.u || (f.m.u == g.m.u && f.m.l <= g.m.l);
}

inline int operator>(const Fix48& f, const Fix48& g)	
{ 
  return f.m.u > g.m.u || (f.m.u == g.m.u && f.m.l > g.m.l);
}

inline int operator<(const Fix48& f, const Fix48& g)	
{ 
  return f.m.u < g.m.u || (f.m.u == g.m.u && f.m.l < g.m.l);
}

inline istream& operator>>(istream& s, Fix48& f)
{ 
  double d;
  s >> d; 
  f = d; 
  return s; 
}

inline ostream& operator<<(ostream& s, const Fix48& f)
{ 
  return s << double(f);
}

#endif
