// This may look like C code, but it is really -*- C++ -*-
/*
Copyright (C) 1989 Free Software Foundation
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
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/
//
// Fix.cc : variable length fixed point data type class functions
//

#ifdef __GNUG__
#pragma implementation
#endif
#include <Fix.h>
#include <std.h>
#include <Obstack.h>
#include <AllocRing.h>
#include <strstream.h>
#include <new.h>

// member constants

const _G_uint16_t Fix::min_length;
const _G_uint16_t Fix::max_length;
const double Fix::min_value;
const double Fix::max_value;

// default parameters

_G_uint16_t Fix::default_length = 16;
int    Fix::default_print_width = 8;

Fix::PEH Fix::overflow_handler = Fix::overflow_saturate;

Fix::Rep Fix::Rep_0	= { 16, 1, 1, { 0 } };
Fix::Rep Fix::Rep_m1	= { 16, 1, 1, { 0x8000 } };
Fix::Rep Fix::Rep_quotient_bump = { 16, 1, 1, { 0x4000 } };

// error handling

void
Fix::default_error_handler(const char* msg)
{
  cerr << "Fix: " << msg << "\n";
  abort();
}

void
Fix::default_range_error_handler(const char* msg)
{
  cerr << "Fix: range error in " << msg << "\n";
  //abort();
}

one_arg_error_handler_t 
  Fix::error_handler = Fix::default_error_handler,
  Fix::range_error_handler = Fix::default_range_error_handler;

one_arg_error_handler_t
Fix::set_error_handler(one_arg_error_handler_t f)
{
  one_arg_error_handler_t old = error_handler;
  error_handler = f;
  return old;
}

one_arg_error_handler_t
Fix::set_range_error_handler(one_arg_error_handler_t f)
{
  one_arg_error_handler_t old = range_error_handler;
  range_error_handler = f;
  return old;
}

void
Fix::error(const char* msg)
{
  error_handler(msg);
}

void
Fix::range_error(const char* msg)
{
  range_error_handler(msg);
}

// Fix::Rep allocation and initialization functions

static inline Fix::Rep*
_new_Fix(_G_uint16_t len)
{
  int siz = (((_G_uint32_t) len + 15) >> 4);
  if (siz <= 0) siz = 1;
  unsigned int allocsiz = (sizeof(Fix::Rep) + (siz - 1) * sizeof(_G_uint16_t));
  Fix::Rep* z = new (operator new (allocsiz)) Fix::Rep;
  memset(z, 0, allocsiz);
  z->len = len;
  z->siz = siz;
  z->ref = 1;
  return z;
}

Fix::Rep*
Fix::new_Fix(_G_uint16_t len)
{
  return _new_Fix(len);
}

Fix::Rep*
Fix::new_Fix(_G_uint16_t len, const Rep* x)
{
  Rep* z = _new_Fix(len);
  return copy(x,z);
}

Fix::Rep*
Fix::new_Fix(_G_uint16_t len, double d)
{
  Rep* z = _new_Fix(len);

  if ( d == max_value )
  {
    z->s[0] = 0x7fff;
    for ( int i=1; i < z->siz; i++ )
      z->s[i] = 0xffff;
  }
  else if ( d < min_value || d > max_value )
    range_error("declaration");
  else
  {
    if (d < 0)
      d += 2.0;
    d *= 32768;
    for ( int i=0; i < z->siz; i++ )
    {
      z->s[i] = (_G_uint16_t )d;
      d -= z->s[i];
      d *= 65536;
    }
    if ( d >= 32768 )
      z->s[z->siz-1]++;
  }
  mask(z);
  return z;
}

// convert to a double 

double
value(const Fix& x)
{ 
  double d = 0.0;
  for ( int i=x.rep->siz-1; i >= 0; i-- )
  {
    d += x.rep->s[i];
    d *= 1./65536.;
  }
  d *= 2.;
  return d < 1. ? d : d - 2.;
}

// extract mantissa to Integer

Integer
mantissa(const Fix& x)
{
  Integer a = 1, b=1;
  for ( int i=0; i < x.rep->siz; i++ )
  {
    a <<= 16;
    a += x.rep->s[i];
    b <<= 16;
  }
  return a-b;
}

// comparison functions
  
inline static int
docmp(const _G_uint16_t* x, const _G_uint16_t* y, int siz)
{
  int diff = (_G_int16_t )*x - (_G_int16_t )*y;
  while ( --siz && !diff )
    diff = (_G_int32_t )(_G_uint32_t )*++x - (_G_int32_t )(_G_uint32_t )*++y;
  return diff;
}

inline static int
docmpz(const _G_uint16_t* x, int siz)
{
  while ( siz-- )
    if ( *x++ ) return 1;
  return 0;
}

int
Fix::compare(const Rep* x, const Rep* y)
{
  if ( x->siz == y->siz )
    return docmp(x->s, y->s, x->siz);
  else
  {
    int r;
    const Rep* longer, *shorter;
    if ( x->siz > y->siz )
    {
      longer = x;
      shorter = y;
      r = 1;
    }
    else
    {
      longer = y;
      shorter = x;
      r = -1;
    }
    int diff = docmp(x->s, y->s, shorter->siz);
    if ( diff )
      return diff;
    else if ( docmpz(&longer->s[shorter->siz], longer->siz-shorter->siz) )
      return r;
    else
      return 0;
  }
}

// arithmetic functions

Fix::Rep*
Fix::add(const Rep* x, const Rep* y, Rep* r)
{
  _G_uint16_t xsign = x->s[0], ysign = y->s[0];
  const Rep* longer, *shorter;
  if ( x->len >= y->len )
    longer = x, shorter = y;
  else
    longer = y, shorter = x;
  if ( r == NULL )
    r = new_Fix(longer->len);
  int i;
  for ( i=r->siz-1; i >= longer->siz; i-- )
    r->s[i] = 0;
  for ( ; i >= shorter->siz; i-- )
    r->s[i] = longer->s[i];
  _G_uint32_t sum = 0, carry = 0;
  for ( ; i >= 0; i-- )
  {
    sum = carry + (_G_uint32_t )x->s[i] + (_G_uint32_t )y->s[i];
    carry = sum >> 16;
    r->s[i] = sum;
  }
  if ( (xsign ^ sum) & (ysign ^ sum) & 0x8000 )
    overflow_handler(r);
  return r;
}

Fix::Rep*
Fix::subtract(const Rep* x, const Rep* y, Rep* r)
{
  _G_uint16_t xsign = x->s[0], ysign = y->s[0];
  const Rep* longer, *shorter;
  if ( x->len >= y->len )
    longer = x, shorter = y;
  else
    longer = y, shorter = x;
  if ( r == NULL )
    r = new_Fix(longer->len);
  int i;
  for ( i=r->siz-1; i >= longer->siz; i-- )
    r->s[i] = 0;
  for ( ; i >= shorter->siz; i-- )
    r->s[i] = (longer == x ? x->s[i] : -y->s[i]);
  _G_int16_t carry = 0;
  _G_uint32_t sum = 0;
  for ( ; i >= 0; i-- )
  {
    sum = (_G_int32_t )carry + (_G_uint32_t )x->s[i] - (_G_uint32_t )y->s[i];
    carry = sum >> 16;
    r->s[i] = sum;
  }
  if ( (xsign ^ sum) & (~ysign ^ sum) & 0x8000 )
    overflow_handler(r);
  return r;
}

Fix::Rep*
Fix::multiply(const Rep* x, const Rep* y, Rep* r)
{
  if ( r == NULL )
    r = new_Fix(x->len + y->len);
  int xsign = x->s[0] & 0x8000,
    ysign = y->s[0] & 0x8000;
  Fix X(x->len), Y(y->len);
  if ( xsign )
    x = negate(x,X.rep);
  if ( ysign )
    y = negate(y,Y.rep);
  int i;
  for ( i=0; i < r->siz; i++ )
    r->s[i] = 0;
  for ( i=x->siz-1; i >= 0; i-- )
  {
    _G_uint32_t carry = 0;
    for ( int j=y->siz-1; j >= 0; j-- ) 
    {
      int k = i + j + 1;
      _G_uint32_t a = (_G_uint32_t )x->s[i] * (_G_uint32_t )y->s[j];
      _G_uint32_t b = ((a << 1) & 0xffff) + carry;
      if ( k < r->siz )
      {
	b += r->s[k];
        r->s[k] = b;
      }
      if ( k < (int)r->siz + 1 )
        carry = (a >> 15) + (b >> 16);
    }
    r->s[i] = carry;
  }
  if ( xsign != ysign )
    negate(r,r);
  return r;
}

Fix::Rep*
Fix::multiply(const Rep* x, int y, Rep* r)
{
  if ( y != (_G_int16_t )y )
    range_error("multiply by int -- int too large");
  if ( r == NULL )
    r = new_Fix(x->len);
  int i;
  for ( i=r->siz-1; i >= x->siz; i-- )
    r->s[i] = 0;
  _G_int32_t a, carry = 0;
  for ( ; i > 0; i-- )
  {
    a = (_G_int32_t) (_G_uint32_t )x->s[i] * y + carry;
    r->s[i] = a;
    carry = a >> 16;		// assumes arithmetic right shift
  }
  a = (_G_int32_t) (_G_int16_t )x->s[0] * y + carry;
  r->s[0] = a;
  a &= 0xffff8000L;
  if ( a != (_G_int32_t)0xffff8000L && a != (_G_int32_t)0L ) {
    r->s[0] = 0x8000 ^ x->s[0] ^ y;
    overflow_handler(r);
  }
  return r;
}

Fix::Rep*
Fix::divide(const Rep* x, const Rep* y, Rep* q, Rep* r)
{
  int xsign = x->s[0] & 0x8000, 
    ysign = y->s[0] & 0x8000;
  if ( q == NULL )
    q = new_Fix(x->len);
  copy(&Rep_0,q);
  if ( r == NULL )
    r = new_Fix(x->len + y->len - 1);
  if ( xsign )
    negate(x,r);
  else
    copy(x,r);
  Fix Y(y->len);
  Rep* y2 = ( ysign ? negate(y,Y.rep) : copy(y,Y.rep) );
  if ( !compare(y2) )
    range_error("division -- division by zero");
  else if ( compare(x,y2) >= 0 )
    if ( compare(x,y2) == 0 && (xsign ^ ysign) != 0 )
    {
      copy(&Rep_m1,q);
      copy(&Rep_0,r);
    }
    else
      range_error("division");
  else
  {
    Rep* t;
    Fix S(r->len),
      W(q->len,&Rep_quotient_bump);
    for ( int i=1; i < q->len; i++ )
    {
      shift(y2,-1,y2);
      subtract(r,y2,S.rep);
      int s_status = compare(S.rep);
      if ( s_status == 0 ) 
      {
	t = r, r = S.rep, S.rep = t;
	break;
      }
      else if ( s_status > 0 )
      {
	t = r, r = S.rep, S.rep = t;
	add(q,W.rep,q);
      }
      shift(W.rep,-1,W.rep);
    }
    if ( xsign ^ ysign )
      negate(q,q);
  }
  return q;
}

Fix::Rep*
Fix::shift(const Rep* x, int y, Rep* r)
{
  if ( r == NULL )
    r = new_Fix(x->len);
  if ( y == 0 )
    {
      copy (x, r);
      return r;
    }

  int ay = abs((_G_int32_t) y),
    ayh = ay >> 4,
    ayl = ay & 0x0f;
  int xl, u, ilow, ihigh;
  _G_uint16_t *rs;
  const _G_uint16_t *xsl, *xsr;

  if ( y > 0 )
  {
    rs = r->s;
    xsl = x->s + ayh;
    xsr = xsl + 1;
    xl = ayl;
    u = 1;
    ihigh = x->siz - ayh - 1;
    ilow = 0;
  }
  else
  {
    rs = &r->s[r->siz - 1];
    xsr = &x->s[r->siz - 1] - ayh;
    xsl = xsr - 1;
    xl = 16 - ayl;
    u = -1;
    ihigh = r->siz - ayh - 1;
    ilow = ihigh - x->siz;
  }

  int xr = 16 - xl;
  _G_uint16_t xrmask = 0xffffL >> xr;
  int i;
  for ( i=0; i < ilow; i++, rs+=u, xsl+=u, xsr+=u )
    *rs = 0;
  for ( ; i < ihigh; i++, rs+=u, xsl+=u, xsr+=u )
    *rs = (*xsl << xl) + ((*xsr >> xr) & xrmask);
  *rs = (y > 0 ? (*xsl << xl) : ((*xsr >> xr) & xrmask));
  rs += u;
  for ( ; ++i < r->siz; rs+=u )
    *rs = 0;
  return r;
}

Fix::Rep*
Fix::negate(const Rep* x, Rep* r)
{
  if ( r == NULL )
    r = new_Fix(x->len);
  _G_uint32_t carry = 1;
  int i;
  for ( i=r->siz-1; i >= x->siz; i-- )
    r->s[i] = 0;
  for ( ; i >= 0; i-- )
  {
    _G_uint32_t a = (_G_uint16_t )~x->s[i] + carry;	// bug work-around
    r->s[i] = a;
    carry = a >> 16;
  }
  return r;
}

// io functions

Fix
atoF(const char* a, int len)
{
  return Fix(len,atof(a));
}

extern AllocRing _libgxx_fmtq;

void
Fix::printon(ostream& s, int width) const
{
  double val = value(*this);
  int old_precision = s.precision(width-3);
  _G_int32_t old_flags = s.setf(ios::fixed, ios::fixed|ios::scientific);
  if (val >= 0)
      s << ' ';
  s.width(width-2);
  s << val;
  s.precision(old_precision);
  s.flags(old_flags);
}

char*
Ftoa(Fix& x, int width)
{
  int wrksiz = width + 2;
  char *fmtbase = (char *) _libgxx_fmtq.alloc(wrksiz);
  ostrstream stream(fmtbase, wrksiz);
  
  x.printon(stream, width);
  stream << ends;
  return fmtbase;
}

extern Obstack _libgxx_io_ob;

Fix
Fix::operator %= (int y)
{
  Fix r((int )rep->len + y, *this); return *this = r;
}

istream&
operator >> (istream& s, Fix& y)
{
  int got_one = 0;
  if (!s.ipfx(0))
  {
    s.clear(ios::failbit|s.rdstate()); // Redundant if using GNU iostreams.
    return s;
  }

  char sign = 0, point = 0;
  char ch;
  s >> ws;
  if (!s.good())
  {
    s.clear(ios::failbit|s.rdstate());
    return s;
  }
  while (s.get(ch))
  {
    if (ch == '-')
    {
      if (sign == 0)
      {
        sign = 1;
        _libgxx_io_ob.grow(ch);
      }
      else
        break;
    }
    if (ch == '.')
    {
      if (point == 0)
      {
        point = 1;
        _libgxx_io_ob.grow(ch);
      }
      else
        break;
    }
    else if (ch >= '0' && ch <= '9')
    {
      got_one = 1;
      _libgxx_io_ob.grow(ch);
    }
    else
      break;
  }
  char * p = (char*)(_libgxx_io_ob.finish(0));
  if (s.good())
    s.putback(ch);
  if (!got_one)
    s.clear(ios::failbit|s.rdstate());
  else
    y = atoF(p);
  _libgxx_io_ob.free(p);
  return s;
}

void
show(const Fix& x)
{
  cout << "len = " << x.rep->len << "\n";
  cout << "siz = " << x.rep->siz << "\n";
  cout << "ref = " << x.rep->ref << "\n";
  cout << "man = ";
#ifdef _OLD_STREAMS
  cout << Itoa(mantissa(x),16,4*x.rep->siz);
#else
  int old_flags = cout.setf(ios::hex, ios::hex|ios::dec|ios::oct);
  cout.width(4*x.rep->siz);
  cout << mantissa(x);
  cout.setf(old_flags, ios::hex|ios::dec|ios::oct);
#endif
  cout << "\n";
  cout << "val = " << value(x) << "\n";
}

// parameter setting operations

Fix::PEH Fix::set_overflow_handler(PEH new_handler)
{
  PEH old_handler = overflow_handler;
  overflow_handler = new_handler;
  return old_handler;
}

int
Fix::set_default_length(int newlen)
{
  _G_uint16_t oldlen = default_length;
  if ( newlen < min_length || newlen > max_length )
    error("illegal length in Fix::set_default_length");
  default_length = newlen;
  return oldlen;
}

// overflow handlers

void
Fix::overflow_saturate(Rep* r)
{
  if ( (_G_int16_t) r->s[0] > 0 ) 
  {
    r->s[0] = 0x8000;
    for ( int i=1; i < r->siz; i++ )
      r->s[i] = 0;
  }
  else
  {
    r->s[0] = 0x7fff;
    for ( int i = 1; i < (int)r->siz; i++ )
      r->s[i] = 0xffff;
    mask(r);
  }
}

void
Fix::overflow_wrap(Rep*)
{}

void
Fix::overflow_warning_saturate(Rep* r)
{
  overflow_warning(r);
  overflow_saturate(r);
}

void
Fix::overflow_warning(Rep*)
{
  cerr << "Fix: overflow warning\n"; 
}

void
Fix::overflow_error(Rep*)
{
  cerr << "Fix: overflow error\n"; 
  abort();
}
