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
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

//
// Fix24.cc : fixed precision class support functions
//

#ifdef __GNUG__
#pragma implementation
#endif
#include <Fix24.h>

// basic operators too large to be inline

_G_int32_t Fix24::assign(double d) 
{ 
  if (d == 1.0)
    return Fix24_m_max;
  else if (d > Fix24_max)
  {
    _G_int32_t i = Fix24_m_max;
    range_error(i);
    return i;
  }
  else if (d < Fix24_min)
  {
    _G_int32_t i = Fix24_m_min;
    range_error(i);
    return i;
  }
  else {
    // Round to 24 bits
    d = (_G_int32_t) (d * (1 << 24) + ((d >= 0)? 0.5 : -0.5));
    /* Convert to integer format */
    return ((_G_int32_t) d) << (Fix24_shift - 24);
  }
}

twolongs Fix48::assign(double d) 
{ 
  if (d == 1.0)
    return Fix48_m_max;
  else if (d > Fix48_max)
  {
    twolongs i = Fix48_m_max;
    range_error(i);
    return i;
  }
  else if (d < Fix48_min)
  {
    twolongs i = Fix48_m_min;
    range_error(i);
    return i;
  }
  else {
    twolongs i;
    int sign = (d < 0);

/* First, convert the absolute value of d to a 48-bit integer format */
    if (d < 0) d = -d;
    i.u = ((_G_int32_t)(d *= Fix24_mult)) & 0xffffff00;
    i.l = ((_G_uint32_t)((d - i.u)* (Fix24_mult / (1 << 7)))) & 0xffffff00;

/* Calculate the two's complement if d was negative */
    if (sign) {
	_G_uint32_t oldlower = i.l;
	i.l = (~i.l + 1) & 0xffffff00;
	i.u = (~i.u + (((oldlower ^ i.l) & Fix24_msb)? 0 : 1)) & ~0xffL;
    }
    return i;
  }
}


Fix48 operator * (const Fix24& a, const Fix24& b)
{
// break a and b into lo and hi parts, and do a multiple-precision
// multiply, with rounding

  int apos = (a.m >= 0);
  _G_uint32_t ua = (apos)? a.m : - a.m;
  ua <<= 1; // ua is biased so result will be 47 bit mantissa, not 46:
  _G_uint32_t hi_a = (ua >> 16) & ((1 << 16) - 1);
  _G_uint32_t lo_a = ua & ((1 << 16) - 1);

  int bpos = (b.m >= 0);
  _G_uint32_t ub = (bpos)? b.m : -b.m;
  _G_uint32_t hi_b = (ub >> 16) & ((1 << 16) - 1);
  _G_uint32_t lo_b = ub & ((1 << 16) - 1);

  _G_uint32_t 
    hi_r = hi_a * hi_b,
    mi_r = hi_a * lo_b + lo_a * hi_b,
    lo_r = lo_a * lo_b,
    rl = ((hi_r << 16) & 0x00ffffffL) + (mi_r & 0x00ffffffL) + (lo_r >> 16);
  twolongs r;
  r.u = (hi_r & 0xffffff00L) + ((mi_r >> 16) & 0x0000ff00L)
    + ((rl >> 16) & 0x0000ff00L);
  r.l = rl << 8;

  if ( apos != bpos ) {
    _G_uint32_t l = r.l;
    r.l = -r.l;
    r.u = (~r.u + ((l ^ r.l) & Fix24_msb ? 0 : Fix24_lsb)) & 0xffffff00;
  }
  return r;
}

Fix24 operator / (const Fix24& a, const Fix24& b)
{
  _G_int32_t q;
  int apos = (a.m >= 0);
  _G_uint32_t la = (apos)? a.m : -a.m;
  int bpos = (b.m >= 0);
  _G_uint32_t lb = (bpos)? b.m: -b.m;
  if (la >= lb)
  {
    q = (apos == bpos)? Fix24_m_max: Fix24_m_min;
    a.range_error(q);
  }
  else                        // standard shift-based division alg
  {
    q = 0;
    _G_int32_t r = la;

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

    q += 0x80;			// Round result to 24 bits
    if (apos != bpos) q = -q;	// Fix sign
  }
  return (q & ~0xff);
}


Fix48 operator + (const Fix48&  f, const Fix48&  g)
{
  _G_int32_t lo_r = (f.m.l >> 8) + (g.m.l >> 8);
  twolongs r;
  r.u = f.m.u + g.m.u + (lo_r & 0x01000000L ? 0x00000100L : 0);
  r.l =  lo_r << 8;

  if ( (f.m.u ^ r.u) & (g.m.u ^ r.u) & Fix24_msb )
    f.overflow(r);
  return r;
}

Fix48 operator - (const Fix48&  f, const Fix48&  g)
{
  unsigned lo_r = (f.m.l >> 8) - (g.m.l >> 8);
  twolongs r;
  r.u = f.m.u - g.m.u - (lo_r & 0x01000000L ? 0x00000100L: 0);
  r.l = lo_r << 8;

  if ( ((f.m.u ^ r.u) & (-g.m.u ^ r.u) & Fix24_msb) && g.m.u )
    f.overflow(r);
  return r;
}

Fix48 operator * (const Fix48& a, int b)
{
  twolongs r;
  int bpos = (b >= 0);
  unsigned ub = (bpos)? b : -b;
  if ( ub >= 65536L ) {
    r = (bpos)? Fix48_m_max : Fix48_m_min;
    a.range_error(r);
  }
  else {
    _G_uint32_t 
      lo_r = (a.m.l & 0xffff) * ub,
      mi_r = ((a.m.l >> 16) & 0xffff) * ub,
      hi_r = a.m.u * ub;
    r.l = lo_r + (mi_r << 16);
    r.u = hi_r + ((mi_r >> 8) & 0x00ffff00L);
    if ( !bpos ) {
      _G_uint32_t l = r.l;
      r.l = -r.l & 0xffffffff;
      r.u = ~r.u + ((l ^ r.l) & Fix24_msb ? 0 : Fix24_lsb);
    }
  }
  return r;
}

Fix48 operator << (const Fix48& a, int b)
{
  twolongs r; r.u = 0; r.l = 0;
  if ( b >= 0 )
    if ( b < 24 ) {
      r.u = (a.m.u << b) + ((a.m.l >> (24 - b)) & 0xffffff00L);
      r.l = a.m.l << b;
    }
    else if ( b < 48 ) {
      r.u = a.m.l << (b - 24);
    }
  return r;
}

Fix48 operator >> (const Fix48& a, int b)
{
  twolongs r; r.u = 0; r.l = 0;
  if ( b >= 0 )
    if ( b < 24 ) {
      r.l = ((a.m.u << (24 - b)) & 0xffffffffL) + ((a.m.l >> b) & 0xffffff00L);
      r.u = (a.m.u >> b) & ~0xffL;
    }
    else if ( b < 48 ) {
      r.l = (a.m.u >> (b - 24)) & 0xffffff00L;
      r.u = (a.m.u >> 24) & ~0xffL;
    }
    else {
      r.l = (a.m.u >> 24) & ~0xffL;
      r.u = r.l;
    }
  return r;
}

// error handling

void Fix24::overflow(_G_int32_t& i) const
{
  (*Fix24_overflow_handler)(i);
}

void Fix48::overflow(twolongs& i) const
{
  (*Fix48_overflow_handler)(i);
}

void Fix24::range_error(_G_int32_t& i) const
{
  (*Fix24_range_error_handler)(i);
}

void Fix48::range_error(twolongs& i) const
{
  (*Fix48_range_error_handler)(i);
}

// data definitions

Fix24_peh Fix24_overflow_handler = Fix24_overflow_saturate;
Fix48_peh Fix48_overflow_handler = Fix48_overflow_saturate;

Fix24_peh Fix24_range_error_handler = Fix24_warning;
Fix48_peh Fix48_range_error_handler = Fix48_warning;

//function definitions

Fix24_peh set_Fix24_overflow_handler(Fix24_peh new_handler) {
  Fix24_peh old_handler = Fix24_overflow_handler;
  Fix24_overflow_handler = new_handler;
  return old_handler;
}

Fix48_peh set_Fix48_overflow_handler(Fix48_peh new_handler) {
  Fix48_peh old_handler = Fix48_overflow_handler;
  Fix48_overflow_handler = new_handler;
  return old_handler;
}

void set_overflow_handler(Fix24_peh handler24, Fix48_peh handler48) {
  set_Fix24_overflow_handler(handler24);
  set_Fix48_overflow_handler(handler48);
}

Fix24_peh set_Fix24_range_error_handler(Fix24_peh new_handler) {
  Fix24_peh old_handler = Fix24_range_error_handler;
  Fix24_range_error_handler = new_handler;
  return old_handler;
}

Fix48_peh set_Fix48_range_error_handler(Fix48_peh new_handler) {
  Fix48_peh old_handler = Fix48_range_error_handler;
  Fix48_range_error_handler = new_handler;
  return old_handler;
}

void set_range_error_handler(Fix24_peh handler24, Fix48_peh handler48) {
  set_Fix24_range_error_handler(handler24);
  set_Fix48_range_error_handler(handler48);
}

void Fix24_overflow_saturate(_G_int32_t& i)
  { i = (i > 0 ? Fix24_m_min : Fix24_m_max); }
void Fix24_ignore(_G_int32_t&) {}
void Fix24_warning(_G_int32_t&)
  { cerr << "warning: Fix24 result out of range\n"; }
void Fix24_overflow_warning_saturate(_G_int32_t& i)
  { cerr << "warning: Fix24 result out of range\n"; 
   Fix24_overflow_saturate(i); }
void Fix24_abort(_G_int32_t&)
  { cerr << "error: Fix24 result out of range\n"; abort(); }

void Fix48_ignore(twolongs&) {}
void Fix48_overflow_saturate(twolongs& i)
  { i = (i.u > 0 ? Fix48_m_min : Fix48_m_max); }
void Fix48_warning(twolongs&)
  { cerr << "warning: Fix48 result out of range\n"; }
void Fix48_overflow_warning_saturate(twolongs& i)
  { cerr << "warning: Fix48 result out of range\n"; 
   Fix48_overflow_saturate(i); }
void Fix48_abort(twolongs&)
  { cerr << "error: Fix48 result out of range\n"; abort(); }

