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

#ifndef _Fix16_h
#ifdef __GNUG__
#pragma interface
#endif
#define _Fix16_h 1

#include <stream.h>
#include <std.h>

// constant definitions

#define Fix16_fs 	((double)((unsigned)(1 << 15)))

#define Fix16_msb	(1 << 15)
#define Fix16_m_max	((1 << 15) - 1)
#define Fix16_m_min	((short)(1 << 15))

#define Fix16_mult	Fix16_fs
#define Fix16_div	(1./Fix16_fs)
#define Fix16_max	(1. - .5/Fix16_fs)
#define Fix16_min	(-1.)


#define Fix32_fs 	((double)((unsigned long)(1 << 31)))

#define Fix32_msb	((unsigned long)(1 << 31))
#define Fix32_m_max	((long)((1 << 31) - 1))
#define Fix32_m_min	((long)(1 << 31))

#define Fix32_mult	Fix32_fs
#define Fix32_div	(1./Fix32_fs)
#define Fix32_max	(1. - .5/Fix32_fs)
#define Fix32_min	(-1.)


//
// Fix16    class: 16-bit Fixed point data type
//
//	consists of a 16-bit mantissa (sign bit & 15 data bits).
//

class Fix16 
{ 
  friend class          Fix32;

  short                 m;

  short                 round(double d);
  short                 assign(double d);
                        Fix16(short i);
                        Fix16(int i);

         operator       double() const;


public:
                        Fix16();
                        Fix16(const Fix16&  f);
                        Fix16(double d);
                        Fix16(const Fix32& f);

                        ~Fix16();

  Fix16&                operator=(const Fix16&  f);
  Fix16&                operator=(double d);
  Fix16&                operator=(const Fix32& f);

  friend short&         mantissa(Fix16&  f);
  friend const short&   mantissa(const Fix16&  f);
  friend double         value(const Fix16&  f);

  Fix16                 operator +  () const;
  Fix16                 operator -  () const;

  friend Fix16          operator +  (const Fix16&  f, const Fix16&  g);
  friend Fix16          operator -  (const Fix16&  f, const Fix16&  g);
  friend Fix32          operator *  (const Fix16&  f, const Fix16&  g);
  friend Fix16          operator /  (const Fix16&  f, const Fix16&  g);
  friend Fix16          operator << (const Fix16&  f, int b);
  friend Fix16          operator >> (const Fix16&  f, int b);

  Fix16&                operator += (const Fix16&  f);
  Fix16&                operator -= (const Fix16&  f);
  Fix16&                operator *= (const Fix16& );
  Fix16&                operator /= (const Fix16&  f);
  
  Fix16&                operator <<=(int b);
  Fix16&                operator >>=(int b);

  friend int            operator == (const Fix16&  f, const Fix16&  g);
  friend int            operator != (const Fix16&  f, const Fix16&  g);
  friend int            operator >= (const Fix16&  f, const Fix16&  g);
  friend int            operator <= (const Fix16&  f, const Fix16&  g);
  friend int            operator >  (const Fix16&  f, const Fix16&  g);
  friend int            operator <  (const Fix16&  f, const Fix16&  g);

  friend istream&       operator >> (istream& s, Fix16&  f);
  friend ostream&       operator << (ostream& s, const Fix16&  f);

  void                  overflow(short&) const;
  void                  range_error(short&) const;

  friend Fix16          operator *  (const Fix16&  f, int g);
  friend Fix16          operator *  (int g, const Fix16&  f);
  Fix16&                operator *= (int g);
};

 
//
// Fix32 class: 32-bit Fixed point data type
//
//	consists of a 32-bit mantissa (sign bit & 31 data bits).
//

class Fix32 
{ 
  friend class         Fix16;

  long                 m;

  long                 round(double d);
  long                 assign(double d);

                       Fix32(long i);
                       operator double() const;


public:
                       Fix32();
                       Fix32(const Fix32& f);
                       Fix32(const Fix16&  f);
                       Fix32(double d);
                       ~Fix32();

  Fix32&               operator =  (const Fix32& f);
  Fix32&               operator =  (const Fix16&  f);
  Fix32&               operator =  (double d);

  friend long&         mantissa(Fix32& f);
  friend const long&   mantissa(const Fix32& f);
  friend double        value(const Fix32& f);

  Fix32                operator +  () const;
  Fix32                operator -  () const;

  friend Fix32         operator +  (const Fix32& f, const Fix32& g);
  friend Fix32         operator -  (const Fix32& f, const Fix32& g);
  friend Fix32         operator *  (const Fix32& f, const Fix32& g);
  friend Fix32         operator /  (const Fix32& f, const Fix32& g);
  friend Fix32         operator << (const Fix32& f, int b);
  friend Fix32         operator >> (const Fix32& f, int b);

  friend Fix32         operator *  (const Fix16&  f, const Fix16&  g);

  Fix32&               operator += (const Fix32& f);
  Fix32&               operator -= (const Fix32& f);
  Fix32&               operator *= (const Fix32& f);
  Fix32&               operator /= (const Fix32& f);
  Fix32&               operator <<=(int b);
  Fix32&               operator >>=(int b);

  friend int           operator == (const Fix32& f, const Fix32& g);
  friend int           operator != (const Fix32& f, const Fix32& g);
  friend int           operator >= (const Fix32& f, const Fix32& g);
  friend int           operator <= (const Fix32& f, const Fix32& g);
  friend int           operator >  (const Fix32& f, const Fix32& g);
  friend int           operator <  (const Fix32& f, const Fix32& g);

  friend istream&      operator >> (istream& s, Fix32& f);
  friend ostream&      operator << (ostream& s, const Fix32& f);

  void                 overflow(long& i) const;
  void                 range_error(long& i) const;

  friend Fix32          operator *  (const Fix32&  f, int g);
  friend Fix32          operator *  (int g, const Fix32&  f);
  Fix32&                operator *= (int g);
};

// active error handler declarations

typedef void (*Fix16_peh)(short&);
typedef void (*Fix32_peh)(long&);

extern Fix16_peh Fix16_overflow_handler;
extern Fix32_peh Fix32_overflow_handler;

extern Fix16_peh Fix16_range_error_handler;
extern Fix32_peh Fix32_range_error_handler;

#if defined(SHORT_NAMES) || defined(VMS)
#define	set_overflow_handler	sohndl
#define set_range_error_handler	srnghdl
#endif


// error handler declarations

extern Fix16_peh set_Fix16_overflow_handler(Fix16_peh);
extern Fix32_peh set_Fix32_overflow_handler(Fix32_peh);
extern void set_overflow_handler(Fix16_peh, Fix32_peh);

extern Fix16_peh set_Fix16_range_error_handler(Fix16_peh);
extern Fix32_peh set_Fix32_range_error_handler(Fix32_peh);
extern void set_range_error_handler(Fix16_peh, Fix32_peh);

extern void
  Fix16_ignore(short&),
  Fix16_overflow_saturate(short&),
  Fix16_overflow_warning_saturate(short&),
  Fix16_warning(short&),
  Fix16_abort(short&);

extern void
  Fix32_ignore(long&),
  Fix32_overflow_saturate(long&),
  Fix32_overflow_warning_saturate(long&),
  Fix32_warning(long&),
  Fix32_abort(long&);


inline Fix16::~Fix16() {}

inline short Fix16::round(double d)
{ 
  return short( (d >= 0)? d + 0.5 : d - 0.5); 
}

inline Fix16::Fix16(short i)		
{ 
  m = i; 
}

inline Fix16::Fix16(int i)	    
{ 
  m = i; 
}

inline Fix16::operator double() const 
{ 
  return  Fix16_div * m; 
}

inline Fix16::Fix16()     			
{ 
  m = 0; 
}

inline Fix16::Fix16(const Fix16&  f)		
{ 
  m = f.m; 
}

inline Fix16::Fix16(double d)		
{
  m = assign(d);
}


inline Fix16&  Fix16::operator=(const Fix16&  f)	
{ 
  m = f.m; 
  return *this; 
}

inline Fix16&  Fix16::operator=(double d) 
{ 
  m = assign(d); 
  return *this; 
}


inline Fix32::Fix32()			    
{ 
  m = 0;
}

inline Fix32::Fix32(long i)		
{ 
  m = i;
}

inline Fix32:: operator double() const		
{ 
  return Fix32_div * m;
}


inline Fix32::Fix32(const Fix32& f)		
{ 
  m = f.m;
}

inline Fix32::Fix32(const Fix16&  f)    
{ 
  m = long(f.m) << 16;
}

inline Fix32::Fix32(double d)		
{ 
  m = assign(d);
}

inline Fix16::Fix16(const Fix32& f)		
{ 
  m = f.m >> 16; 
}


inline Fix16&  Fix16::operator=(const Fix32& f)
{ 
  m = f.m >> 16; 
  return *this; 
}

inline Fix32& Fix32::operator=(const Fix32& f)	
{ 
  m = f.m;
  return *this; 
}

inline Fix32& Fix32::operator=(const Fix16&  f)	
{ 
  m = long(f.m) << 16;
  return *this;
}

inline Fix32& Fix32::operator=(double d)	
{ 
  m = assign(d);
  return *this; 
}

inline short& mantissa(Fix16&  f)	
{ 
  return f.m; 
}

inline const short& mantissa(const Fix16&  f)	
{ 
  return f.m; 
}

inline double value(const Fix16&  f)		
{ 
  return double(f); 
}

inline Fix16 Fix16::operator+() const		
{ 
  return m; 
}

inline Fix16 Fix16::operator-() const	
{ 
  return -m; 
}

inline Fix16 operator+(const Fix16&  f, const Fix16&  g) 
{
  short sum = f.m + g.m;
  if ( (f.m ^ sum) & (g.m ^ sum) & Fix16_msb )
    f.overflow(sum);
  return sum;
}

inline Fix16 operator-(const Fix16&  f, const Fix16&  g) 
{
  short sum = f.m - g.m;
  if ( (f.m ^ sum) & (-g.m ^ sum) & Fix16_msb )
    f.overflow(sum);
  return sum;
}

inline Fix32 operator*(const Fix16&  f, const Fix16&  g)
{ 
  return Fix32( long( long(f.m) * long(g.m) << 1)); 
}

inline Fix16 operator<<(const Fix16& a, int b) 	
{ 
  return a.m << b; 
}

inline Fix16 operator>>(const Fix16& a, int b) 	
{ 
  return a.m >> b; 
}

inline  Fix16&  Fix16:: operator+=(const Fix16&  f)
{ 
  return *this = *this + f; 
}

inline Fix16&  Fix16:: operator-=(const Fix16&  f) 	
{ 
  return *this = *this - f; 
}

inline Fix16& Fix16::operator*=(const Fix16& f) 	
{ 
  return *this = *this * f; 
}

inline Fix16&  Fix16:: operator/=(const Fix16&  f) 	
{ 
  return *this = *this / f; 
}

inline Fix16&  Fix16:: operator<<=(int b)	
{ 
  return *this = *this << b;
}

inline Fix16&  Fix16:: operator>>=(int b)	
{ 
  return *this = *this >> b;
}

inline int operator==(const Fix16&  f, const Fix16&  g)	
{ 
  return f.m == g.m;
}

inline int operator!=(const Fix16&  f, const Fix16&  g)	
{ 
  return f.m != g.m;
}

inline int operator>=(const Fix16&  f, const Fix16&  g)	
{ 
  return f.m >= g.m;
}

inline int operator<=(const Fix16&  f, const Fix16&  g)	
{ 
  return f.m <= g.m;
}

inline int operator>(const Fix16&  f, const Fix16&  g)	
{ 
  return f.m > g.m;
}

inline int operator<(const Fix16&  f, const Fix16&  g)	
{ 
  return f.m < g.m;
}

inline istream&  operator>>(istream& s, Fix16&  f)
{ 
  double d;
  s >> d; 
  f = d; 
  return s; 
}

inline ostream&  operator<<(ostream& s, const Fix16&  f)
{ 
  return s << double(f);
}


inline Fix16 operator*(const Fix16&  f, int g)
{
  return Fix16(short(f.m * g));
}

inline Fix16 operator*(int g, const Fix16&  f)
{
  return f * g;
}


inline Fix16& Fix16::operator*=(int g)
{
  return *this = *this * g;
}

inline Fix32::~Fix32() {}

inline long Fix32::round(double d)
{ 
  return long( (d >= 0)? d + 0.5 : d - 0.5);
}

inline long& mantissa(Fix32& f)	
{ 
  return f.m;
}

inline const long& mantissa(const Fix32& f)	
{ 
  return f.m;
}

inline double value(const Fix32& f)		
{ 
  return double(f);
}

inline Fix32 Fix32::operator+() const
{ 
  return m;
}

inline Fix32 Fix32::operator-() const		
{ 
  return -m;
}

inline Fix32 operator+(const Fix32& f, const Fix32& g) 
{
  long sum = f.m + g.m;
  if ( (f.m ^ sum) & (g.m ^ sum) & Fix32_msb )
    f.overflow(sum);
  return sum;
}

inline Fix32 operator-(const Fix32& f, const Fix32& g) 
{
  long sum = f.m - g.m;
  if ( (f.m ^ sum) & (-g.m ^ sum) & Fix32_msb )
    f.overflow(sum);
  return sum;
}

inline Fix32 operator<<(const Fix32& a, int b) 	
{ 
  return a.m << b;
}

inline Fix32 operator>>(const Fix32& a, int b) 	
{ 
  return a.m >> b;
}

inline Fix32& Fix32::operator+=(const Fix32& f) 	
{ 
  return *this = *this + f;
}

inline Fix32& Fix32::operator-=(const Fix32& f) 	
{ 
  return *this = *this - f;
}

inline Fix32& Fix32::operator*=(const Fix32& f) 	
{ 
  return *this = *this * f;
}

inline Fix32& Fix32::operator/=(const Fix32& f) 	
{ 
  return *this = *this / f;
}


inline Fix32& Fix32::operator<<=(int b)	
{ 
  return *this = *this << b;
}

inline Fix32& Fix32::operator>>=(int b)	
{ 
  return *this = *this >> b;
}

inline int operator==(const Fix32& f, const Fix32& g)	
{ 
  return f.m == g.m;
}

inline int operator!=(const Fix32& f, const Fix32& g)	
{ 
  return f.m != g.m;
}

inline int operator>=(const Fix32& f, const Fix32& g)	
{ 
  return f.m >= g.m;
}

inline int operator<=(const Fix32& f, const Fix32& g)	
{ 
  return f.m <= g.m;
}

inline int operator>(const Fix32& f, const Fix32& g)	
{ 
  return f.m > g.m;
}

inline int operator<(const Fix32& f, const Fix32& g)	
{ 
  return f.m < g.m;
}

inline istream& operator>>(istream& s, Fix32& f)
{ 
  double d;
  s >> d; 
  f = d; 
  return s; 
}

inline ostream& operator<<(ostream& s, const Fix32& f)
{ 
  return s << double(f);
}

inline Fix32 operator*(const Fix32&  f, int g)
{
  return Fix32(long(f.m * g));
}

inline Fix32 operator*(int g, const Fix32&  f)
{
  return f * g;
}



inline Fix32& Fix32::operator*=(int g)
{
  return *this = *this * g;
}

#endif
