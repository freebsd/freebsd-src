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

//
// Fix.cc : fixed precision class support functions
//

#ifdef __GNUG__
#pragma implementation
#endif
#include <Fix16.h>

// basic operators too large to be inline

short Fix16::assign(double d) 
{ 
  if (d == 1.0)
    return Fix16_m_max;
  else if (d > Fix16_max)
  {
    short i = Fix16_m_max;
    range_error(i);
    return i;
  }
  else if (d < Fix16_min)
  {
    short i = Fix16_m_min;
    range_error(i);
    return i;
  }
  else 
    return round(Fix16_mult * d);
}

long Fix32::assign(double d) 
{ 
  if (d == 1.0)
    return Fix32_m_max;
  else if (d > Fix32_max)
  {
    long i = Fix32_m_max;
    range_error(i);
    return i;
  }
  else if (d < Fix32_min)
  {
    long i = Fix32_m_min;
    range_error(i);
    return i;
  }
  else 
    return round(Fix32_mult * d);
}


Fix32 operator * (const Fix32& a, const Fix32& b)
{
// break a and b into lo and hi parts, and do a multiple-precision
// multiply, with rounding

  int apos = (a.m >= 0);
  unsigned long ua = (apos)? a.m : - a.m;
  ua <<= 1; // ua is biased so result will be 31 bit mantissa, not 30:
  unsigned long hi_a = (ua >> 16) & ((1 << 16) - 1);
  unsigned long lo_a = ua & ((1 << 16) - 1);

  int bpos = (b.m >= 0);
  unsigned long ub = (bpos)? b.m : -b.m;
  unsigned long hi_b = (ub >> 16) & ((1 << 16) - 1);
  unsigned long lo_b = ub & ((1 << 16) - 1);

  unsigned long r = lo_a * lo_b + (1 << 15);
  r = (r >> 16) + hi_a * lo_b + lo_a * hi_b + (1 << 15);
  r = (r >> 16) + hi_a * hi_b;
  long p = (apos != bpos)? -r : r;
  return Fix32(p);
}

Fix16 operator / (const Fix16& a, const Fix16& b)
{
  short q;
  int apos = (a.m >= 0);
  long la = (apos)? a.m : -a.m;
  long scaled_a = la << 15;
  int bpos = (b.m >= 0);
  short sb = (bpos)? b.m: -b.m;
  if (la >= sb)
  {
    q = (apos == bpos)? Fix16_m_max: Fix16_m_min;
    a.range_error(q);
  }
  else
  {
    q = scaled_a / sb;
    if ((scaled_a % sb) >= (sb / 2)) ++q;
    if (apos != bpos) q = -q;
  }
  return Fix16(q);
}

Fix32 operator / (const Fix32& a, const Fix32& b)
{
  long q;
  int apos = (a.m >= 0);
  unsigned long la = (apos)? a.m : -a.m;
  int bpos = (b.m >= 0);
  unsigned long lb = (bpos)? b.m: -b.m;
  if (la >= lb)
  {
    q = (apos == bpos)? Fix32_m_max: Fix32_m_min;
    a.range_error(q);
  }
  else                        // standard shift-based division alg
  {
    q = 0;
    long r = la;

    for (int i = 32; i > 0; i--)
    {
	if ((unsigned)(r) > lb) {
	    q = (q << 1) | 1;
	    r -= lb;
	}
	else
	    q = (q << 1);
	r <<= 1;
    }

    if (apos != bpos) q = -q;	// Fix sign
  }	
  return Fix32(q);
}


// error handling

void Fix16::overflow(short& i) const
{
  (*Fix16_overflow_handler)(i);
}

void Fix32::overflow(long& i) const
{
  (*Fix32_overflow_handler)(i);
}

void Fix16::range_error(short& i) const
{
  (*Fix16_range_error_handler)(i);
}

void Fix32::range_error(long& i) const
{
  (*Fix32_range_error_handler)(i);
}

// data definitions

Fix16_peh Fix16_overflow_handler = Fix16_overflow_saturate;
Fix32_peh Fix32_overflow_handler = Fix32_overflow_saturate;

Fix16_peh Fix16_range_error_handler = Fix16_warning;
Fix32_peh Fix32_range_error_handler = Fix32_warning;

//function definitions

Fix16_peh set_Fix16_overflow_handler(Fix16_peh new_handler) {
  Fix16_peh old_handler = Fix16_overflow_handler;
  Fix16_overflow_handler = new_handler;
  return old_handler;
}

Fix32_peh set_Fix32_overflow_handler(Fix32_peh new_handler) {
  Fix32_peh old_handler = Fix32_overflow_handler;
  Fix32_overflow_handler = new_handler;
  return old_handler;
}

void set_overflow_handler(Fix16_peh handler16, Fix32_peh handler32) {
  set_Fix16_overflow_handler(handler16);
  set_Fix32_overflow_handler(handler32);
}

Fix16_peh set_Fix16_range_error_handler(Fix16_peh new_handler) {
  Fix16_peh old_handler = Fix16_range_error_handler;
  Fix16_range_error_handler = new_handler;
  return old_handler;
}

Fix32_peh set_Fix32_range_error_handler(Fix32_peh new_handler) {
  Fix32_peh old_handler = Fix32_range_error_handler;
  Fix32_range_error_handler = new_handler;
  return old_handler;
}

void set_range_error_handler(Fix16_peh handler16, Fix32_peh handler32) {
  set_Fix16_range_error_handler(handler16);
  set_Fix32_range_error_handler(handler32);
}

void Fix16_overflow_saturate(short& i)
  { i = (i > 0 ? Fix16_m_min : Fix16_m_max); }
void Fix16_ignore(short&) {}
void Fix16_warning(short&)
  { cerr << "warning: Fix16 result out of range\n"; }
void Fix16_overflow_warning_saturate(short& i)
  { cerr << "warning: Fix16 result out of range\n"; 
   Fix16_overflow_saturate(i); }
void Fix16_abort(short&)
  { cerr << "error: Fix16 result out of range\n"; abort(); }

void Fix32_ignore(long&) {}
void Fix32_overflow_saturate(long& i)
  { i = (i > 0 ? Fix32_m_min : Fix32_m_max); }
void Fix32_warning(long&)
  { cerr << "warning: Fix32 result out of range\n"; }
void Fix32_overflow_warning_saturate(long& i)
  { cerr << "warning: Fix32 result out of range\n"; 
   Fix32_overflow_saturate(i); }
void Fix32_abort(long&)
  { cerr << "error: Fix32 result out of range\n"; abort(); }

