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
Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
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

// default parameters

uint16 Fix_default_length = 16;
int    Fix_default_print_width = 8;

Fix_peh Fix_overflow_handler = Fix_overflow_saturate;

_Frep _Frep_0	= { 16, 1, 1, { 0 } };
_Frep _Frep_m1	= { 16, 1, 1, { 0x8000 } };
_Frep _Frep_quotient_bump = { 16, 1, 1, { 0x4000 } };

// error handling

void default_Fix_error_handler(const char* msg)
{
  cerr << "Fix: " << msg << "\n";
  abort();
}

void default_Fix_range_error_handler(const char* msg)
{
  cerr << "Fix: range error in " << msg << "\n";
  //abort();
}

one_arg_error_handler_t 
  Fix_error_handler = default_Fix_error_handler,
  Fix_range_error_handler = default_Fix_range_error_handler;

one_arg_error_handler_t set_Fix_error_handler(one_arg_error_handler_t f)
{
  one_arg_error_handler_t old = Fix_error_handler;
  Fix_error_handler = f;
  return old;
}

one_arg_error_handler_t set_Fix_range_error_handler(one_arg_error_handler_t f)
{
  one_arg_error_handler_t old = Fix_range_error_handler;
  Fix_range_error_handler = f;
  return old;
}

void Fix::error(const char* msg)
{
  (*Fix_error_handler)(msg);
}

void Fix::range_error(const char* msg)
{
  (*Fix_range_error_handler)(msg);
}

// _Frep allocation and initialization functions

static inline _Fix _new_Fix(uint16 len)
{
  int siz = (((uint32 )len + 15) >> 4);
  if (siz <= 0) siz = 1;
  unsigned int allocsiz = (sizeof(_Frep) + (siz - 1) * sizeof(uint16));
  _Fix z = (_Fix)(new char[allocsiz]);
  memset(z, 0, allocsiz);
  z->len = len;
  z->siz = siz;
  z->ref = 1;
  return z;
}

_Fix new_Fix(uint16 len)
{
  return _new_Fix(len);
}

_Fix new_Fix(uint16 len, const _Fix x)
{
  _Fix z = _new_Fix(len);
  return copy(x,z);
}

_Fix new_Fix(uint16 len, double d)
{
  _Fix z = _new_Fix(len);

  if ( d == _Fix_max_value )
  {
    z->s[0] = 0x7fff;
    for ( int i=1; i < z->siz; i++ )
      z->s[i] = 0xffff;
  }
  else if ( d < _Fix_min_value || d > _Fix_max_value )
    (*Fix_range_error_handler)("declaration");
  else
  {
    if (d < 0)
      d += 2.0;
    d *= 32768;
    for ( int i=0; i < z->siz; i++ )
    {
      z->s[i] = (uint16 )d;
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

double value(const Fix& x)
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

Integer mantissa(Fix& x)
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
  
inline static int docmp(uint16* x, uint16* y, int siz)
{
  int diff = (int16 )*x - (int16 )*y;
  while ( --siz && !diff )
    diff = (int32 )(uint32 )*++x - (int32 )(uint32 )*++y;
  return diff;
}

inline static int docmpz(uint16* x, int siz)
{
  while ( siz-- )
    if ( *x++ ) return 1;
  return 0;
}

int compare(const _Fix x, const _Fix y)
{
  if ( x->siz == y->siz )
    return docmp(x->s, y->s, x->siz);
  else
  {
    int r;
    _Fix longer, shorter;
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

_Fix add(_Fix x, _Fix y, _Fix r)
{
  uint16 xsign = x->s[0], ysign = y->s[0];
  _Fix longer, shorter;
  if ( x->len >= y->len )
    longer = x, shorter = y;
  else
    longer = y, shorter = x;
  if ( r == NULL )
    r = new_Fix(longer->len);
  for ( int i=r->siz-1; i >= longer->siz; i-- )
    r->s[i] = 0;
  for ( ; i >= shorter->siz; i-- )
    r->s[i] = longer->s[i];
  uint32 sum = 0, carry = 0;
  for ( ; i >= 0; i-- )
  {
    sum = carry + (uint32 )x->s[i] + (uint32 )y->s[i];
    carry = sum >> 16;
    r->s[i] = sum;
  }
  if ( (xsign ^ sum) & (ysign ^ sum) & 0x8000 )
    (*Fix_overflow_handler)(r);
  return r;
}

_Fix subtract(_Fix x, _Fix y, _Fix r)
{
  uint16 xsign = x->s[0], ysign = y->s[0];
  _Fix longer, shorter;
  if ( x->len >= y->len )
    longer = x, shorter = y;
  else
    longer = y, shorter = x;
  if ( r == NULL )
    r = new_Fix(longer->len);
  for ( int i=r->siz-1; i >= longer->siz; i-- )
    r->s[i] = 0;
  for ( ; i >= shorter->siz; i-- )
    r->s[i] = (longer == x ? x->s[i] : -y->s[i]);
  int16 carry = 0;
  uint32 sum = 0;
  for ( ; i >= 0; i-- )
  {
    sum = (int32 )carry + (uint32 )x->s[i] - (uint32 )y->s[i];
    carry = sum >> 16;
    r->s[i] = sum;
  }
  if ( (xsign ^ sum) & (~ysign ^ sum) & 0x8000 )
    (*Fix_overflow_handler)(r);
  return r;
}

_Fix multiply(_Fix x, _Fix y, _Fix r)
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
  for ( int i=0; i < r->siz; i++ )
    r->s[i] = 0;
  for ( i=x->siz-1; i >= 0; i-- )
  {
    uint32 carry = 0;
    for ( int j=y->siz-1; j >= 0; j-- ) 
    {
      int k = i + j + 1;
      uint32 a = (uint32 )x->s[i] * (uint32 )y->s[j];
      uint32 b = ((a << 1) & 0xffff) + carry;
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

_Fix multiply(_Fix x, int y, _Fix r)
{
  if ( y != (int16 )y )
    (*Fix_range_error_handler)("multiply by int -- int too large");
  if ( r == NULL )
    r = new_Fix(x->len);
  for ( int i=r->siz-1; i >= x->siz; i-- )
    r->s[i] = 0;
  int32 a, carry = 0;
  for ( ; i > 0; i-- )
  {
    a = (int32 )(uint32 )x->s[i] * y + carry;
    r->s[i] = a;
    carry = a >> 16;		// assumes arithmetic right shift
  }
  a = (int32 )(int16 )x->s[0] * y + carry;
  r->s[0] = a;
  a &= 0xffff8000L;
  if ( a != 0xffff8000L && a != 0L ) {
    r->s[0] = 0x8000 ^ x->s[0] ^ y;
    (*Fix_overflow_handler)(r);
  }
  return r;
}

_Fix divide(_Fix x, _Fix y, _Fix q, _Fix r)
{
  int xsign = x->s[0] & 0x8000, 
    ysign = y->s[0] & 0x8000;
  if ( q == NULL )
    q = new_Fix(x->len);
  copy(&_Frep_0,q);
  if ( r == NULL )
    r = new_Fix(x->len + y->len - 1);
  if ( xsign )
    negate(x,r);
  else
    copy(x,r);
  Fix Y(y->len);
  y = ( ysign ? negate(y,Y.rep) : copy(y,Y.rep) );
  if ( !compare(y) )
    (*Fix_range_error_handler)("division -- division by zero");
  else if ( compare(x,y) >= 0 )
    if ( compare(x,y) == 0 && (xsign ^ ysign) != 0 )
    {
      copy(&_Frep_m1,q);
      copy(&_Frep_0,r);
    }
    else
      (*Fix_range_error_handler)("division");
  else
  {
    _Fix t;
    Fix S(r->len),
      W(q->len,&_Frep_quotient_bump);
    for ( int i=1; i < q->len; i++ )
    {
      shift(y,-1,y);
      subtract(r,y,S.rep);
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

_Fix shift(_Fix x, int y, _Fix r)
{
  if ( y == 0 )
    return x;
  else if ( r == NULL )
    r = new_Fix(x->len);

  int ay = abs((long) y),
    ayh = ay >> 4,
    ayl = ay & 0x0f;
  int xl, u, ilow, ihigh;
  uint16 *rs, *xsl, *xsr;

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
  uint16 xrmask = 0xffffL >> xr;
  for ( int i=0; i < ilow; i++, rs+=u, xsl+=u, xsr+=u )
    *rs = 0;
  for ( ; i < ihigh; i++, rs+=u, xsl+=u, xsr+=u )
    *rs = (*xsl << xl) + ((*xsr >> xr) & xrmask);
  *rs = (y > 0 ? (*xsl << xl) : ((*xsr >> xr) & xrmask));
  rs += u;
  for ( ; ++i < r->siz; rs+=u )
    *rs = 0;
  return r;
}

_Fix negate(_Fix x, _Fix r)
{
  if ( r == NULL )
    r = new_Fix(x->len);
  uint32 carry = 1;
  for ( int i=r->siz-1; i >= x->siz; i-- )
    r->s[i] = 0;
  for ( ; i >= 0; i-- )
  {
    uint32 a = (uint16 )~x->s[i] + carry;	// bug work-around
    r->s[i] = a;
    carry = a >> 16;
  }
  return r;
}

// io functions

Fix atoF(const char* a, int len)
{
  return Fix(len,atof(a));
}

extern AllocRing _libgxx_fmtq;

void Fix::printon(ostream& s, int width) const
{
  char format[20];
  double val = value(*this);
  int old_precision = s.precision(width-3);
  long old_flags = s.setf(ios::fixed, ios::fixed|ios::scientific);
  if (val >= 0)
      s << ' ';
  s.width(width-2);
  s << val;
  s.precision(old_precision);
  s.flags(old_flags);
}

char* Ftoa(Fix& x, int width)
{
  int wrksiz = width + 2;
  char *fmtbase = (char *) _libgxx_fmtq.alloc(wrksiz);
  ostrstream stream(fmtbase, wrksiz);
  
  x.printon(stream, width);
  stream << ends;
  return fmtbase;
}

extern Obstack _libgxx_io_ob;

Fix Fix::operator %= (int y)
{
  Fix r((int )rep->len + y, *this); return *this = r;
}

istream& operator >> (istream& s, Fix& y)
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

void show(Fix& x)
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

Fix_peh set_overflow_handler(Fix_peh new_handler) {
  Fix_peh old_handler = Fix_overflow_handler;
  Fix_overflow_handler = new_handler;
  return old_handler;
}

int Fix_set_default_length(int newlen)
{
  uint16 oldlen = Fix_default_length;
  if ( newlen < _Fix_min_length || newlen > _Fix_max_length )
    (*Fix_error_handler)("illegal length in Fix_set_default_length");
  Fix_default_length = newlen;
  return oldlen;
}

// overflow handlers

void Fix_overflow_saturate(_Fix& r) {
  if ( (int16 )r->s[0] > 0 ) 
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

void Fix_overflow_wrap(_Fix&) {}

void Fix_overflow_warning_saturate(_Fix& r) {
  Fix_overflow_warning(r);
  Fix_overflow_saturate(r);
}

void Fix_overflow_warning(_Fix&) {
  cerr << "Fix: overflow warning\n"; 
}

void Fix_overflow_error(_Fix&) {
  cerr << "Fix: overflow error\n"; 
  abort();
}

