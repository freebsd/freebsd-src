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
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

/* 
  BitString class implementation
 */
 
#ifdef __GNUG__
#pragma implementation
#endif
#include <BitString.h>
#include <std.h>
#include <limits.h>
#include <Obstack.h>
#include <AllocRing.h>
#include <new.h>
#include <builtin.h>
#include <strstream.h>

#undef OK

void BitString::error(const char* msg) const
{
  (*lib_error_handler)("BitString", msg);
}

//  globals

BitStrRep    _nilBitStrRep = {  0, 1, {0} };

BitString _nil_BitString;

#define MINBitStrRep_SIZE  8
#define MAXBitStrRep_SIZE  ((1 << (sizeof(short)*CHAR_BIT - 1)) - 1)

#ifndef MALLOC_MIN_OVERHEAD
#define MALLOC_MIN_OVERHEAD    4
#endif

#define ONES  ((_BS_word)(~0L))
#define MAXBIT  (((_BS_word)1) << (BITSTRBITS - 1))

/*
 *  bit manipulation utilities
*/

// break things up into .s indices and positions

inline static int BitStr_len(int l)
{
  return (unsigned)(l) / BITSTRBITS + 1;
}


// mask out low bits

static inline _BS_word lmask(int p)
{
  return ONES _BS_RIGHT p;
}

// mask out high bits

static inline _BS_word rmask(int p)
{
  int s = BITSTRBITS - 1 - p;
  if (s <= 0)
    return ONES;
  else
    return ONES _BS_LEFT s;
}


// mask out unused bits in last word of rep

inline static void check_last(BitStrRep* r)
{
  int bit_len_mod = r->len & (BITSTRBITS - 1);
  if (bit_len_mod)
    r->s[r->len / BITSTRBITS] &= ONES _BS_LEFT (BITSTRBITS - bit_len_mod);
}

// merge bits from next word

static inline _BS_word borrow_hi(const _BS_word a[], int ind, 
                                      int maxind, int p)
{
  if (p == 0)
    return a[ind];
  else if (ind < maxind)
    return (a[ind] _BS_LEFT p) | (a[ind+1] _BS_RIGHT (BITSTRBITS - p));
  else
    return (a[ind] _BS_LEFT p);
}

// merge bits from prev word

static inline _BS_word borrow_lo(const _BS_word a[], int ind, 
                                      int minind, int p)
{
  _BS_word word = a[ind] _BS_RIGHT (BITSTRBITS - 1 - p);
  if (ind > minind)
    word |= (a[ind-1] _BS_LEFT (p + 1));
  return word;
}

// same with bounds check (for masks shorter than patterns)

static inline _BS_word safe_borrow_hi(const _BS_word a[], int ind, 
                                           int maxind, int p)
{
  if (ind > maxind)
    return 0;
  else if (p == 0)
    return a[ind];
  else if (ind == maxind)
    return a[ind] _BS_LEFT p;
  else
    return (a[ind] _BS_LEFT p) | (a[ind+1] _BS_RIGHT (BITSTRBITS - p));
}


// allocate a new rep; pad to near a power of two

inline static BitStrRep* BSnew(int newlen)
{
  unsigned int siz = sizeof(BitStrRep) + BitStr_len(newlen) * sizeof(_BS_word) 
    + MALLOC_MIN_OVERHEAD;
  unsigned int allocsiz = MINBitStrRep_SIZE;;
  while (allocsiz < siz) allocsiz <<= 1;
  allocsiz -= MALLOC_MIN_OVERHEAD;
  if (allocsiz >= MAXBitStrRep_SIZE * sizeof(_BS_word))
    (*lib_error_handler)("BitString", "Requested length out of range");
    
  BitStrRep* rep = new (operator new (allocsiz)) BitStrRep;
  memset(rep, 0, allocsiz);
  rep->sz =
    (allocsiz - sizeof(BitStrRep) + sizeof(_BS_word)) / sizeof(_BS_word);
  return rep;
}

inline void
copy_bits (_BS_word* pdst, _BS_size_t dstbit,
	   const _BS_word* psrc, _BS_size_t srcbit,
	   _BS_size_t length)
{
  _BS_NORMALIZE (pdst, dstbit);
  _BS_NORMALIZE (psrc, srcbit);
  _BS_copy (pdst, dstbit, psrc, srcbit, length);
}

BitStrRep* BStr_alloc(BitStrRep* old, const _BS_word* src,
                      int startpos, int endp, int newlen)
{
  if (old == &_nilBitStrRep) old = 0; 
  if (newlen < 0) newlen = 0;
  int news = BitStr_len(newlen);
  BitStrRep* rep;
  if (old == 0 || news > old->sz)
    rep = BSnew(newlen);
  else
    rep = old;
  rep->len = newlen;

  if (src != 0 && endp > 0 && (src != rep->s || startpos > 0))
    copy_bits (rep->s, 0, src, startpos, endp - startpos);

  check_last(rep);

  if (old != rep && old != 0) delete old;

  return rep;
}

BitStrRep* BStr_resize(BitStrRep* old, int newlen)
{
  BitStrRep* rep;
  if (newlen < 0) newlen = 0;
  int news = BitStr_len(newlen);
  if (old == 0 || old == &_nilBitStrRep)
  {
    rep = BSnew(newlen);
  }
  else if (news > old->sz)
  {
    rep = BSnew(newlen);
    memcpy(rep->s, old->s, BitStr_len(old->len) * sizeof(_BS_word));
    delete old;
  }
  else
    rep = old;

  rep->len = newlen;
  check_last(rep);
  return rep;
}

BitStrRep* BStr_copy(BitStrRep* old, const BitStrRep* src)
{
  BitStrRep* rep;
  if (old == src && old != &_nilBitStrRep) return old; 
  if (old == &_nilBitStrRep) old = 0;
  if (src == &_nilBitStrRep) src = 0;
  if (src == 0)
  {
    if (old == 0)
      rep = BSnew(0);
    else
      rep = old;
    rep->len = 0;
  }
  else 
  {
    int newlen = src->len;
    int news = BitStr_len(newlen);
    if (old == 0 || news  > old->sz)
    {
      rep = BSnew(newlen);
      if (old != 0) delete old;
    }
    else
      rep = old;
    
    memcpy(rep->s, src->s, news * sizeof(_BS_word));
    rep->len = newlen;
  }
  check_last(rep);
  return rep;
}


int operator == (const BitString& x, const BitString& y)
{
  return x.rep->len == y.rep->len && 
    memcmp((void*)x.rep->s, (void*)y.rep->s, 
         BitStr_len(x.rep->len) * sizeof(_BS_word)) == 0;
}

int operator <= (const BitString& x, const BitString& y)
{
  unsigned int  xl = x.rep->len;
  unsigned int  yl = y.rep->len;
  if (xl > yl)
    return 0;

  const _BS_word* xs = x.rep->s;
  const _BS_word* topx = &(xs[BitStr_len(xl)]);
  const _BS_word* ys = y.rep->s;

  while (xs < topx)
  {
    _BS_word a = *xs++;
    _BS_word b = *ys++;
    if ((a | b) != b)
      return 0;
  }
  return 1;
}

int operator < (const BitString& x, const BitString& y)
{
  unsigned short xl = x.rep->len;
  unsigned short yl = y.rep->len;
  if (xl > yl)
    return 0;

  const _BS_word* xs = x.rep->s;
  const _BS_word* ys = y.rep->s;
  const _BS_word* topx = &(xs[BitStr_len(xl)]);
  const _BS_word* topy = &(ys[BitStr_len(yl)]);
  int one_diff = 0;
  while (xs < topx)
  {
    _BS_word a = *xs++;
    _BS_word b = *ys++;
    _BS_word c = a | b;
    if (c != b)
      return 0;
    else if (c != a)
      one_diff = 1;
  }
  if (one_diff)
    return 1;
  else
  {
    while (ys < topy)
      if (*ys++ != 0)
        return 1;
    return 0;
  }
}

int lcompare(const BitString& x, const BitString& y)
{
  return _BS_lcompare_0 (x.rep->s, x.rep->len, y.rep->s, y.rep->len);
}

int BitString::count(unsigned int b) const
{
  int count = _BS_count (rep->s, 0, rep->len);
  if (!b)
    count = rep->len - count;
  return count;
}


BitStrRep* cmpl(const BitStrRep* src, BitStrRep* r)
{
  r = BStr_copy(r, src);
  _BS_word* rs = r->s;
  _BS_word* topr = &(rs[BitStr_len(r->len)]);
  while (rs < topr)
  {
    _BS_word cmp = ~(*rs);
    *rs++ = cmp;
  }
  check_last(r);
  return r;
}


BitStrRep* and(const BitStrRep* x, const BitStrRep* y, BitStrRep* r)
{
  int xrsame = x == r;
  int yrsame = y == r;

  unsigned int  xl = x->len;
  unsigned int  yl = y->len;
  unsigned int  rl = (xl <= yl)? xl : yl;

  r = BStr_resize(r, rl);

  _BS_word* rs = r->s;
  _BS_word* topr = &(rs[BitStr_len(rl)]);
  const _BS_word* xs = (xrsame)? rs : x->s;
  const _BS_word* ys = (yrsame)? rs : y->s;

  while (rs < topr) *rs++ = *xs++ & *ys++;
  check_last(r);
  return r;
}

BitStrRep* or(const BitStrRep* x, const BitStrRep* y, BitStrRep* r)
{
  unsigned int  xl = x->len;
  unsigned int  yl = y->len;
  unsigned int  rl = (xl >= yl)? xl : yl;
  int xrsame = x == r;
  int yrsame = y == r;

  r = BStr_resize(r, rl);

  _BS_word* rs = r->s;
  const _BS_word* xs = (xrsame)? rs : x->s;
  const _BS_word* topx = &(xs[BitStr_len(xl)]);
  const _BS_word* ys = (yrsame)? rs : y->s;
  const _BS_word* topy = &(ys[BitStr_len(yl)]);

  if (xl <= yl)
  {
    while (xs < topx) *rs++ = *xs++ | *ys++;
    if (rs != ys) while (ys < topy) *rs++ = *ys++;
  }
  else
  {
    while (ys < topy) *rs++ = *xs++ | *ys++;
    if (rs != xs) while (xs < topx) *rs++ = *xs++;
  }
  check_last(r);
  return r;
}


BitStrRep* xor(const BitStrRep* x, const BitStrRep* y, BitStrRep* r)
{
  unsigned int  xl = x->len;
  unsigned int  yl = y->len;
  unsigned int  rl = (xl >= yl)? xl : yl;
  int xrsame = x == r;
  int yrsame = y == r;

  r = BStr_resize(r, rl);

  _BS_word* rs = r->s;
  const _BS_word* xs = (xrsame)? rs : x->s;
  const _BS_word* topx = &(xs[BitStr_len(xl)]);
  const _BS_word* ys = (yrsame)? rs : y->s;
  const _BS_word* topy = &(ys[BitStr_len(yl)]);

  if (xl <= yl)
  {
    while (xs < topx) *rs++ = *xs++ ^ *ys++;
    if (rs != ys) while (ys < topy) *rs++ = *ys++;
  }
  else
  {
    while (ys < topy) *rs++ = *xs++ ^ *ys++;
    if (rs != xs) while (xs < topx) *rs++ = *xs++;
  }
  check_last(r);
  return r;
}


BitStrRep* diff(const BitStrRep* x, const BitStrRep* y, BitStrRep* r)
{
  unsigned int  xl = x->len;
  unsigned int  yl = y->len;
  int xrsame = x == y;
  int yrsame = y == r;

  r = BStr_resize(r, xl);

  _BS_word* rs = r->s;
  const _BS_word* xs = (xrsame)? rs : x->s;
  const _BS_word* topx = &(xs[BitStr_len(xl)]);
  const _BS_word* ys = (yrsame)? rs : y->s;
  const _BS_word* topy = &(ys[BitStr_len(yl)]);

  if (xl <= yl)
  {
    while (xs < topx) *rs++ = *xs++ & ~(*ys++);
  }
  else
  {
    while (ys < topy) *rs++ = *xs++ & ~(*ys++);
    if (rs != xs) while (xs < topx) *rs++ = *xs++;
  }
  check_last(r);
  return r;
}


BitStrRep* cat(const BitStrRep* x, const BitStrRep* y, BitStrRep* r)
{
  unsigned int  xl = x->len;
  unsigned int  yl = y->len;
  unsigned int  rl = xl + yl;
  int xrsame = x == r;
  int yrsame = y == r;

  if (yrsame)
  {
    if (xrsame)
    {
      r = BStr_resize(r, rl);
      copy_bits (r->s, xl, r->s, 0, yl);
    }
    else
    {
      BitStrRep* tmp = BStr_copy(0, y);
      r = BStr_resize(r, rl);
      _BS_copy_0(r->s, x->s, xl);
      copy_bits (r->s, xl, tmp->s, 0, yl);
      delete tmp;
    }
  }
  else
  {
    r = BStr_resize(r, rl);
    if (!xrsame) _BS_copy_0(r->s, x->s, xl);
    copy_bits (r->s, xl, y->s, 0, yl);
  }
  check_last(r);
  return r;
}

BitStrRep* cat(const BitStrRep* x, unsigned int bit, BitStrRep* r)
{
  unsigned int  xl = x->len;
  int xrsame = x == r;
  r = BStr_resize(r, xl+1);
  if (!xrsame)
    _BS_copy_0(r->s, x->s, xl);
  if (bit)
    r->s[BitStr_index(xl)] |= _BS_BITMASK(BitStr_pos(xl));
  else
    r->s[BitStr_index(xl)] &= ~(_BS_BITMASK(BitStr_pos(xl)));
  check_last(r);
  return r;
}

BitStrRep* lshift(const BitStrRep* x, int s, BitStrRep* r)
{
  int xrsame = x == r;
  int  xl = x->len;
  int  rl = xl + s;
  if (s == 0)
    r = BStr_copy(r, x);
  else if (rl <= 0)
  {
    r = BStr_resize(r, 0);
    r->len = 0;
    r->s[0] = 0;
  }
  else if (s > 0)
  {
    r = BStr_resize(r, rl);
    const _BS_word* xs = (xrsame)? r->s : x->s;
    copy_bits (r->s, s, xs, 0, xl);
    _BS_clear (r->s, 0, s);
  }
  else if (xrsame)
  {
    r = BStr_resize(r, xl);
    r->len = rl;
    copy_bits (r->s, 0, r->s, -s, xl + s);
  }
  else
  {
    r = BStr_resize(r, rl);
    copy_bits (r->s, 0, x->s, -s, xl + s);
  }
  check_last(r);
  return r;
}


void BitString::set(int p)
{
  if (p < 0) error("Illegal bit index");
  if ((unsigned)(p) >= rep->len) rep = BStr_resize(rep, p + 1);
  rep->s[BitStr_index(p)] |= _BS_BITMASK(BitStr_pos(p));
}

void BitString::assign(int p, unsigned int bit)
{
  if (p < 0) error("Illegal bit index");
  if ((unsigned)(p) >= rep->len) rep = BStr_resize(rep, p + 1);
  if (bit)
    rep->s[BitStr_index(p)] |= _BS_BITMASK(BitStr_pos(p));
  else
      rep->s[BitStr_index(p)] &= ~(_BS_BITMASK(BitStr_pos(p)));
}

void BitString::clear(int p)
{
  if (p < 0) error("Illegal bit index");
  if ((unsigned)(p) >= rep->len) rep = BStr_resize(rep, p + 1);
  rep->s[BitStr_index(p)] &= ~(_BS_BITMASK(BitStr_pos(p)));
}

void BitString::clear()
{
  if (rep == &_nilBitStrRep) return;
  _BS_clear (rep->s, 0, rep->len);
}

void BitString::set()
{
  if (rep == &_nilBitStrRep) return;
  _BS_word* s = rep->s;
  _BS_word* tops = &(s[BitStr_len(rep->len)]);
  while (s < tops) *s++ = ONES;
  check_last(rep);
}

void BitString::invert(int p)
{
  if (p < 0) error("Illegal bit index");
  if ((unsigned)(p) >= rep->len) rep = BStr_resize(rep, p + 1);
  rep->s[BitStr_index(p)] ^= _BS_BITMASK(BitStr_pos(p));
}

void BitString::set(int from, int to)
{
  if (from < 0 || from > to) error("Illegal bit index");
  if ((unsigned)(to) >= rep->len) rep = BStr_resize(rep, to+1);

  _BS_size_t len = to - from + 1;
  _BS_word* xs = rep->s;
  _BS_NORMALIZE (xs, from);
  _BS_invert (xs, from, len);
}

void BitString::clear(int from, int to)
{
  if (from < 0 || from > to) error("Illegal bit index");
  if ((unsigned)(to) >= rep->len) rep = BStr_resize(rep, to+1);

  _BS_size_t len = to - from + 1;
  _BS_word* xs = rep->s;
  _BS_NORMALIZE (xs, from);
  _BS_clear (xs, from, len);
}

void BitString::invert(int from, int to)
{
  if (from < 0 || from > to) error("Illegal bit index");
  if ((unsigned)(to) >= rep->len) rep = BStr_resize(rep, to+1);
  _BS_size_t len = to - from + 1;
  _BS_word* xs = rep->s;
  _BS_NORMALIZE (xs, from);
  _BS_invert (xs, from, len);
}


int BitString::test(int from, int to) const
{
  if (from < 0 || from > to || (unsigned)(from) >= rep->len) return 0;
  
  _BS_size_t len = to - from + 1;
  _BS_word* xs = rep->s;
  _BS_NORMALIZE (xs, from);
  return _BS_any (xs, from, len);
}

int BitString::next(int p, unsigned int b) const
{
  if ((unsigned)(++p) >= rep->len)
    return -1;

  int ind = BitStr_index(p);
  int pos = BitStr_pos(p);
  int l = BitStr_len(rep->len);

  int j = ind;
  const _BS_word* s = rep->s;
  _BS_word a = s[j] >> pos;
  int i = pos;

  if (b != 0)
  {
    for (; i < BITSTRBITS && a != 0; ++i)
    {
      if (a & 1)
        return j * BITSTRBITS + i;
      a >>= 1;
    }
    for (++j; j < l; ++j)
    {
      a = s[j];
      for (i = 0; i < BITSTRBITS && a != 0; ++i)
      {
        if (a & 1)
          return j * BITSTRBITS + i;
        a >>= 1;
      }
    }
    return -1;
  }
  else
  {
    int last = BitStr_pos(rep->len);
    if (j == l - 1)
    {
      for (; i < last; ++i)
      {
        if ((a & 1) == 0)
          return j * BITSTRBITS + i;
        a >>= 1;
      }
      return -1;
    }

    for (; i < BITSTRBITS; ++i)
    {
      if ((a & 1) == 0)
        return j * BITSTRBITS + i;
      a >>= 1;
    }
    for (++j; j < l - 1; ++j)
    {
      a = s[j];
      if (a != ONES)
      {
        for (i = 0; i < BITSTRBITS; ++i)
        {
          if ((a & 1) == 0)
            return j * BITSTRBITS + i;
          a >>= 1;
        }
      }
    }
    a = s[j];
    for (i = 0; i < last; ++i)
    {
      if ((a & 1) == 0)
        return j * BITSTRBITS + i;
      a >>= 1;
    }
    return -1;
  }
}

int BitString::prev(int p, unsigned int b) const
{
  if (--p < 0)
    return -1;

  int ind = BitStr_index(p);
  int pos = BitStr_pos(p);

  const _BS_word* s = rep->s;

  if ((unsigned)(p) >= rep->len)
  {
    ind = BitStr_index(rep->len - 1);
    pos = BitStr_pos(rep->len - 1);
  }

  int j = ind;
  _BS_word a = s[j];

  int i = pos;
  _BS_word maxbit = ((_BS_word)1) << pos;

  if (b != 0)
  {
    for (; i >= 0 && a != 0; --i)
    {
      if (a & maxbit)
        return j * BITSTRBITS + i;
      a <<= 1;
    }
    maxbit = ((_BS_word)1) << (BITSTRBITS - 1);
    for (--j; j >= 0; --j)
    {
      a = s[j];
      for (i = BITSTRBITS - 1; i >= 0 && a != 0; --i)
      {
        if (a & maxbit)
          return j * BITSTRBITS + i;
        a <<= 1;
      }
    }
    return -1;
  }
  else
  {
    if (a != ONES)
    {
      for (; i >= 0; --i)
      {
        if ((a & maxbit) == 0)
          return j * BITSTRBITS + i;
        a <<= 1;
      }
    }
    maxbit = ((_BS_word)1) << (BITSTRBITS - 1);
    for (--j; j >= 0; --j)
    {
      a = s[j];
      if (a != ONES)
      {
        for (i = BITSTRBITS - 1; i >= 0; --i)
        {
          if ((a & maxbit) == 0)
            return j * BITSTRBITS + i;
          a <<= 1;
        }
      }
    }
    return -1;
  }
}


int BitString::search(int startx, int lengthx, 
                      const _BS_word* ys, int starty, int lengthy) const
{
  const _BS_word* xs = rep->s;
  int ylen = lengthy - starty;
  int righty = lengthy - 1;
  int rev = startx < 0;
  if (rev)
  {
    int leftx = 0;
    int rightx = lengthx + startx;
    startx = rightx - ylen + 1;
    if (ylen == 0) return startx;
    if (starty < 0 || righty < 0 || startx < 0 || startx >= lengthx) return -1;
    
    int xind = BitStr_index(startx);
    int xpos = BitStr_pos(startx);
    int yind = BitStr_index(starty);
    int ypos = BitStr_pos(starty);
    
    int rightxind = BitStr_index(rightx);

    _BS_word x = borrow_hi(xs, xind, rightxind, xpos);
  
    int rightyind = BitStr_index(righty);
    int rightypos = BitStr_pos(righty);
    _BS_word y = borrow_hi(ys, yind, rightyind, ypos);
    _BS_word ymask;
    if (yind == rightyind)
      ymask = rmask(rightypos);
    else if (yind+1 == rightyind)
      ymask = rmask(BITSTRBITS - ypos + rightypos + 1);
    else
      ymask = ONES;
    
    int p = startx;
    for (;;)
    {
      if ((x & ymask) == y)
      {
        int xi = xind;
        int yi = yind;
        for (;;)
        {
          if (++yi > rightyind || ++xi > rightxind)
            return p;
          _BS_word tx = borrow_hi(xs, xi, rightxind, xpos);
          _BS_word ty = borrow_hi(ys, yi, rightyind, ypos);
          if (yi == rightyind)
            tx &= rmask(rightypos);
          else if (yi+1 == rightyind)
            tx &= rmask(BITSTRBITS - ypos + rightypos + 1);
          if (tx != ty)
            break;
        }
      }
      if (--p < leftx)
        return -1;
      if (--xpos < 0)
      {
        xpos = BITSTRBITS - 1;
        --xind;
      }
      x = borrow_hi(xs, xind, rightxind, xpos);
    }
  }
  else
  {

    int rightx = lengthx - 1;
    if (ylen == 0) return startx;
    if (starty < 0 || righty < 0 || startx < 0 || startx >= lengthx) return -1;
    
    int xind = BitStr_index(startx);
    int xpos = BitStr_pos(startx);
    int yind = BitStr_index(starty);
    int ypos = BitStr_pos(starty);

    int rightxind = BitStr_index(rightx);

    _BS_word x = borrow_hi(xs, xind, rightxind, xpos);
    _BS_word nextx = (xind >= rightxind) ? 0 : (xs[xind+1] >> xpos);
  
    int rightyind = BitStr_index(righty);
    int rightypos = BitStr_pos(righty);
    _BS_word y = borrow_hi(ys, yind, rightyind, ypos);
    _BS_word ymask;
    if (yind == rightyind)
      ymask = rmask(rightypos);
    else if (yind+1 == rightyind)
      ymask = rmask(BITSTRBITS - ypos + rightypos + 1);
    else
      ymask = ONES;
  
    int p = startx;
    for (;;)
    {
      if ((x & ymask) == y)
      {
        int xi = xind;
        int yi = yind;
        for (;;)
        {
          if (++yi > rightyind || ++xi > rightxind)
            return p;
          _BS_word tx = borrow_hi(xs, xi, rightxind, xpos);
          _BS_word ty = borrow_hi(ys, yi, rightyind, ypos);
          if (yi == rightyind)
            tx &= rmask(rightypos);
          else if (yi+1 == rightyind)
            tx &= rmask(BITSTRBITS - ypos + rightypos + 1);
          if (tx != ty)
            break;
        }
      }
      if (++p > rightx)
        return -1;
      if (++xpos == BITSTRBITS)
      {
        xpos = 0;
        x = xs[++xind];
        nextx = (xind >= rightxind) ? 0 : xs[xind+1];
      }
      else
      {
        x >>= 1;
        if (nextx & 1)
          x |= MAXBIT;
        nextx >>= 1;
      }
    }
  }
}


int BitPattern::search(const _BS_word* xs, int startx, int lengthx) const
{
  const _BS_word* ys = pattern.rep->s;
  const _BS_word* ms = mask.rep->s;
  int righty = pattern.rep->len - 1;
  int rightm = mask.rep->len - 1;

  int rev = startx < 0;
  if (rev)
  {
    int leftx = 0;
    int rightx = lengthx + startx;
    startx = rightx - righty;

    if (righty < 0) return startx;
    if (startx < 0 || startx >= lengthx) return -1;
  
    int xind = BitStr_index(startx);
    int xpos = BitStr_pos(startx);
    
    int rightxind = BitStr_index(rightx);

    int rightmind = BitStr_index(rightm);
    int rightyind = BitStr_index(righty);
    
    _BS_word x = safe_borrow_hi(xs, xind, rightxind, xpos);
    _BS_word m = safe_borrow_hi(ms, 0, rightmind, 0);
    _BS_word y = safe_borrow_hi(ys, 0, rightyind, 0) & m;
    
    int p = startx;
    for (;;)
    {
      if ((x & m) == y)
      {
        int xi = xind;
        int yi = 0;
        for (;;)
        {
          if (++yi > rightyind || ++xi > rightxind)
            return p;
          _BS_word tm = safe_borrow_hi(ms, yi, rightmind, 0);
          _BS_word ty = safe_borrow_hi(ys, yi, rightyind, 0);
          _BS_word tx = safe_borrow_hi(xs, xi, rightxind, xpos);
          if ((tx & tm) != (ty & tm))
            break;
        }
      }
      if (--p < leftx)
        return -1;
      if (--xpos < 0)
      {
        xpos = BITSTRBITS - 1;
        --xind;
      }
      x = safe_borrow_hi(xs, xind, rightxind, xpos);
    }
  }
  else
  {

    int rightx = lengthx - 1;

    if (righty < 0) return startx;
    if (startx < 0 || startx >= lengthx) return -1;
    
    int xind = BitStr_index(startx);
    int xpos = BitStr_pos(startx);
    
    int rightxind = BitStr_index(rightx);

    int rightmind = BitStr_index(rightm);
    int rightyind = BitStr_index(righty);
    
    _BS_word x = safe_borrow_hi(xs, xind, rightxind, xpos);
    _BS_word m = safe_borrow_hi(ms, 0, rightmind, 0);
    _BS_word y = safe_borrow_hi(ys, 0, rightyind, 0) & m;

    _BS_word nextx = (xind >= rightxind) ? 0 : (xs[xind+1] >> xpos);
    
    int p = startx;
    for (;;)
    {
      if ((x & m) == y)
      {
        int xi = xind;
        int yi = 0;
        for (;;)
        {
          if (++yi > rightyind || ++xi > rightxind)
            return p;
          _BS_word tm = safe_borrow_hi(ms, yi, rightmind, 0);
          _BS_word ty = safe_borrow_hi(ys, yi, rightyind, 0);
          _BS_word tx = safe_borrow_hi(xs, xi, rightxind, xpos);
          if ((tx & tm) != (ty & tm))
            break;
        }
      }
      if (++p > rightx)
        return -1;
      if (++xpos == BITSTRBITS)
      {
        xpos = 0;
        x = xs[++xind];
        nextx = (xind >= rightxind) ? 0 : xs[xind+1];
      }
      else
      {
        x >>= 1;
        if (nextx & 1)
          x |= MAXBIT;
        nextx >>= 1;
      }
    }
  }
}

int BitString::match(int startx, int lengthx, int exact, 
                     const _BS_word* ys, int starty, int yl) const
{
  const _BS_word* xs = rep->s;
  int ylen = yl - starty;
  int righty = yl - 1;

  int rightx;
  int rev = startx < 0;
  if (rev)
  {
    rightx = lengthx + startx;
    startx = rightx - ylen + 1;
    if (exact && startx != 0)
      return 0;
  }
  else
  {
    rightx = lengthx - 1;
    if (exact && rightx - startx != righty)
      return 0;
  }

  if (ylen == 0) return 1;
  if (righty < 0 || startx < 0 || startx >= lengthx) return 0;
  
  int xi   = BitStr_index(startx);
  int xpos = BitStr_pos(startx);
  int yi   = BitStr_index(starty);
  int ypos = BitStr_pos(starty);

  int rightxind = BitStr_index(rightx);
  int rightyind = BitStr_index(righty);
  int rightypos = BitStr_pos(righty);

  for (;;)
  {
    _BS_word x = borrow_hi(xs, xi, rightxind, xpos);
    _BS_word y = borrow_hi(ys, yi, rightyind, ypos);
    if (yi == rightyind)
      x &= rmask(rightypos);
    else if (yi+1 == rightyind)
      x &= rmask(BITSTRBITS - ypos + rightypos + 1);
    if (x != y)
      return 0;
    else if (++yi > rightyind || ++xi > rightxind)
      return 1;
  }
}

int BitPattern::match(const _BS_word* xs, int startx, 
                      int lengthx, int exact) const
{
  const _BS_word* ys = pattern.rep->s;
  int righty = pattern.rep->len - 1;
  _BS_word* ms = mask.rep->s;
  int rightm = mask.rep->len - 1;

  int rightx;
  int rev = startx < 0;
  if (rev)
  {
    rightx = lengthx + startx;
    startx = rightx - righty;
    if (exact && startx != 0)
      return 0;
  }
  else
  {
    rightx = lengthx - 1;
    if (exact && rightx - startx != righty)
      return 0;
  }

  if (righty < 0) return 1;
  if (startx < 0 || startx >= lengthx) return 0;
  
  int xind = BitStr_index(startx);
  int xpos = BitStr_pos(startx);
  int yind = 0;

  int rightxind = BitStr_index(rightx);
  int rightyind = BitStr_index(righty);
  int rightmind = BitStr_index(rightm);

  for(;;)
  {
    _BS_word m = safe_borrow_hi(ms, yind, rightmind, 0);
    _BS_word x = safe_borrow_hi(xs, xind, rightxind, xpos) & m;
    _BS_word y = safe_borrow_hi(ys, yind, rightyind, 0) & m;
    if (x != y)
      return 0;
    else if (++yind > rightyind || ++xind > rightxind)
      return 1;
  }
}

BitSubString& BitSubString::operator = (const BitString& y)
{
  if (&S == &_nil_BitString)
    return *this;
  BitStrRep* targ = S.rep;

  unsigned int ylen = y.rep->len;
  int sl = targ->len - len + ylen;

  if (y.rep == targ || ylen > len)
  {
    BitStrRep* oldtarg = targ;
    targ = BStr_alloc(0, 0, 0, 0, sl);
    _BS_copy (targ->s, 0, oldtarg->s, 0, pos);
    copy_bits (targ->s, pos, y.rep->s, 0, ylen);
    copy_bits (targ->s, pos + ylen, oldtarg->s, pos+len, oldtarg->len-pos-len);
    delete oldtarg;
  }
  else if (len == ylen)
    copy_bits (targ->s, pos, y.rep->s, 0, len);
  else if (ylen < len)
  {
    copy_bits (targ->s, pos, y.rep->s, 0, ylen);
    copy_bits (targ->s, pos + ylen, targ->s, pos + len, targ->len - pos - len);
    targ->len = sl;
  }
  check_last(targ);
  S.rep = targ;
  return *this;
}

BitSubString& BitSubString::operator = (const BitSubString& y)
{
  if (&S == &_nil_BitString)
    return *this;
  BitStrRep* targ = S.rep;
  
  if (len == 0 || pos >= targ->len)
    return *this;
  
  int sl = targ->len - len + y.len;
  
  if (y.S.rep == targ || y.len > len)
  {
    BitStrRep* oldtarg = targ;
    targ = BStr_alloc(0, 0, 0, 0, sl);
    _BS_copy_0(targ->s, oldtarg->s, pos);
    copy_bits (targ->s, pos, y.S.rep->s, y.pos, y.len);
    copy_bits (targ->s, pos + y.len, oldtarg->s, pos+len,
	       oldtarg->len-pos-len);
    delete oldtarg;
  }
  else if (len == y.len)
    copy_bits (targ->s, pos, y.S.rep->s, y.pos, y.len);
  else if (y.len < len)
  {
    copy_bits (targ->s, pos, y.S.rep->s, y.pos, y.len);
    copy_bits (targ->s, pos + y.len, targ->s, pos + len,
	       targ->len - pos - len);
    targ->len = sl;
  }
  check_last(targ);
  S.rep = targ;
  return *this;
}

BitSubString BitString::at(int first, int len)
{
  return _substr(first, len);
}

BitSubString BitString::before(int pos)
{
  return _substr(0, pos);
}

BitSubString BitString::after(int pos)
{
  return _substr(pos + 1, rep->len - (pos + 1));
}

BitSubString BitString::at(const BitString& y, int startpos)
{
  int first = search(startpos, rep->len, y.rep->s, 0, y.rep->len);
  return _substr(first,  y.rep->len);
}

BitSubString BitString::before(const BitString& y, int startpos)
{
  int last = search(startpos, rep->len, y.rep->s, 0, y.rep->len);
  return _substr(0, last);
}

BitSubString BitString::after(const BitString& y, int startpos)
{
  int first = search(startpos, rep->len, y.rep->s, 0, y.rep->len);
  if (first >= 0) first += y.rep->len;
  return _substr(first, rep->len - first);
}


BitSubString BitString::at(const BitSubString& y, int startpos)
{
  int first = search(startpos, rep->len, y.S.rep->s, y.pos, y.len);
  return _substr(first, y.len);
}

BitSubString BitString::before(const BitSubString& y, int startpos)
{
  int last = search(startpos, rep->len, y.S.rep->s, y.pos, y.len);
  return _substr(0, last);
}

BitSubString BitString::after(const BitSubString& y, int startpos)
{
  int first = search(startpos, rep->len, y.S.rep->s, y.pos, y.len);
  if (first >= 0) first += y.len;
  return _substr(first, rep->len - first);
}

BitSubString BitString::at(const BitPattern& r, int startpos)
{
  int first = r.search(rep->s, startpos, rep->len);
  return _substr(first, r.pattern.rep->len);
}


BitSubString BitString::before(const BitPattern& r, int startpos)
{
  int first = r.search(rep->s, startpos, rep->len);
  return _substr(0, first);
}

BitSubString BitString::after(const BitPattern& r, int startpos)
{
  int first = r.search(rep->s, startpos, rep->len);
  if (first >= 0) first += r.pattern.rep->len;
  return _substr(first, rep->len - first);
}

#if defined(__GNUG__) && !defined(_G_NO_NRV)
#define RETURN(r) return
#define RETURNS(r) return r;
#define RETURN_OBJECT(TYPE, NAME) /* nothing */
#define USE_UNSIGNED 1 /* probably correct */
#else /* _G_NO_NRV */
#define RETURN(r) return r
#define RETURNS(r) /* nothing */
#define RETURN_OBJECT(TYPE, NAME) TYPE NAME;
#define USE_UNSIGNED 0 /* probably old bug */
#endif

BitString
common_prefix (const BitString& x, const BitString& y, int startpos)
     RETURNS(r)
{
  RETURN_OBJECT(BitString, r);
  unsigned int  xl = x.rep->len;
  unsigned int  yl = y.rep->len;

  unsigned int startx, starty;
  if (startpos < 0)
  {
    startx = xl + startpos;
    starty = yl + startpos;
  }
  else
    startx = starty = startpos;

  if (startx >= xl || starty >= yl)
    RETURN(r);

  const _BS_word* xs = &(x.rep->s[BitStr_index(startx)]);
  _BS_word a = *xs++;
  unsigned int xp = startx;

  const _BS_word* ys = &(y.rep->s[BitStr_index(starty)]);
  _BS_word b = *ys++;
  unsigned int yp = starty;

  for(; xp < xl && yp < yl; ++xp, ++yp)
  {
    _BS_word xbit = ((_BS_word)1) << (BitStr_pos(xp));
    _BS_word ybit = ((_BS_word)1) << (BitStr_pos(yp));
    if (((a & xbit) == 0) != ((b & ybit) == 0))
      break;
    if (xbit == MAXBIT)
      a = *xs++;
    if (ybit == MAXBIT)
      b = *ys++;
  }
  r.rep = BStr_alloc(0, x.rep->s, startx, xp, xp - startx);
  RETURN(r);
}


BitString
common_suffix (const BitString& x, const BitString& y, int startpos)
     RETURNS(r)
{
  RETURN_OBJECT(BitString, r);
  unsigned int  xl = x.rep->len;
  unsigned int  yl = y.rep->len;

  unsigned int startx, starty;
  if (startpos < 0)
  {
    startx = xl + startpos;
    starty = yl + startpos;
  }
  else
    startx = starty = startpos;

  if (startx >= xl || starty >= yl)
    RETURN(r);

  const _BS_word* xs = &(x.rep->s[BitStr_index(startx)]);
  _BS_word a = *xs--;
  int xp = startx;

  const _BS_word* ys = &(y.rep->s[BitStr_index(starty)]);
  _BS_word b = *ys--;
  int yp = starty;

  for(; xp >= 0 && yp >= 0; --xp, --yp)
  {
    _BS_word xbit = ((_BS_word)1) << (BitStr_pos(xp));
    _BS_word ybit = ((_BS_word)1) << (BitStr_pos(yp));
    if (((a & xbit) == 0) != ((b & ybit) == 0))
      break;
    if (xbit == 1)
      a = *xs--;
    if (ybit == 1)
      b = *ys--;
  }
  r.rep = BStr_alloc(0, x.rep->s, xp+1, startx+1, startx - xp);
  RETURN(r);
}

BitString reverse (const BitString& x)
     RETURNS(r)
{
  RETURN_OBJECT(BitString, r);
  unsigned int  yl = x.rep->len;
  BitStrRep* y = BStr_resize(0, yl);
  if (yl > 0)
  {
    const _BS_word* ls = x.rep->s;
    _BS_word lm = 1;
    _BS_word* rs = &(y->s[BitStr_index(yl - 1)]);
    _BS_word rm = ((_BS_word)1) << (BitStr_pos(yl - 1));
    for (unsigned int  l = 0; l < yl; ++l)
    {
      if (*ls & lm)
        *rs |= rm;
      if (lm == MAXBIT)
      {
        ++ls;
        lm = 1;
      }
      else
        lm <<= 1;
      if (rm == 1)
      {
        --rs;
        rm = MAXBIT;
      }
      else
        rm >>= 1;
    }
  }
  r.rep = y;
  RETURN(r);
}

BitString
atoBitString (const char* s, char f, char t)
     RETURNS(res)
{
  RETURN_OBJECT(BitString, res);
  int sl = strlen(s);
  BitStrRep* r = BStr_resize(0, sl);
  if (sl != 0)
  {
    unsigned int  rl = 0;
    _BS_word* rs = r->s;
    _BS_word a = 0;
    _BS_word m = 1;
    unsigned int  i = 0;
    for(;;)
    {
      char ch = s[i];
      if (ch != t && ch != f)
      {
        *rs = a;
        break;
      }
      ++rl;
      if (ch == t)
        a |= m;
      if (++i == sl)
      {
        *rs = a;
        break;
      }
      else if (i % BITSTRBITS == 0)
      {
        *rs++ = a;
        a = 0;
        m = 1;
      }
      else
        m <<= 1;
    }
    r = BStr_resize(r, rl);
  }
  res.rep = r;
  RETURN(res);
}

BitPattern
atoBitPattern (const char* s, char f,char t,char x)
     RETURNS(r)
{
  RETURN_OBJECT(BitPattern, r);
  int sl = strlen(s);
  if (sl != 0)
  {
    unsigned int  rl = 0;
    r.pattern.rep = BStr_resize(r.pattern.rep, sl);
    r.mask.rep = BStr_resize(r.mask.rep, sl);
    _BS_word* rs = r.pattern.rep->s;
    _BS_word* ms = r.mask.rep->s;
    _BS_word a = 0;
    _BS_word b = 0;
    _BS_word m = 1;
    unsigned int  i = 0;
    for(;;)
    {
      char ch = s[i];
      if (ch != t && ch != f && ch != x)
      {
        *rs = a;
        *ms = b;
        break;
      }
      ++rl;
      if (ch == t)
      {
        a |= m;
        b |= m;
      }
      else if (ch == f)
      {
        b |= m;
      }
      if (++i == sl)
      {
        *rs = a;
        *ms = b;
        break;
      }
      else if (i % BITSTRBITS == 0)
      {
        *rs++ = a;
        *ms++ = b;
        a = 0;
        b = 0;
        m = 1;
      }
      else
        m <<= 1;
    }
    r.pattern.rep = BStr_resize(r.pattern.rep, rl);
    r.mask.rep = BStr_resize(r.mask.rep, rl);
  }
  RETURN(r);
}

extern AllocRing _libgxx_fmtq;

void BitString::printon (ostream& os, char f, char t) const
{
  unsigned int  xl = rep->len;
  const _BS_word* ptr = rep->s;
  register streambuf *sb = os.rdbuf();
  _BS_word a = 0;

  for (unsigned int  i = 0; i < xl; ++i)
  {
    if (i % BITSTRBITS == 0)
      a = *ptr++;
    sb->sputc((a & 1)? t : f);
    a >>= 1;
  }
}
const char* BitStringtoa(const BitString& x, char f, char t)
{
  int wrksiz = x.length() + 2;
  char* fmtbase = (char *) _libgxx_fmtq.alloc(wrksiz);
  ostrstream stream(fmtbase, wrksiz);
  
  x.printon(stream, f, t);
  stream << ends;
  return fmtbase;
}

ostream& operator << (ostream& s, const BitString& x)
{
  if (s.opfx())
    x.printon(s);
  return s;
}

const char* BitPatterntoa(const BitPattern& p, char f,char t,char x)
{
  unsigned int  pl = p.pattern.rep->len;
  unsigned int  ml = p.mask.rep->len;
  unsigned int  l = (pl <= ml)? pl : ml;

  int wrksiz = l + 2;
  char* fmtbase = (char *) _libgxx_fmtq.alloc(wrksiz);
  ostrstream stream(fmtbase, wrksiz);
  
  p.printon(stream, f, t, x);
  stream << ends;
  return fmtbase;
}

void BitPattern::printon(ostream& s, char f,char t,char x) const
{
  unsigned int  pl = pattern.rep->len;
  unsigned int  ml = mask.rep->len;
  unsigned int  l = (pl <= ml)? pl : ml;
  register streambuf *sb = s.rdbuf();

  const _BS_word* ps = pattern.rep->s;
  const _BS_word* ms = mask.rep->s;
  _BS_word a = 0;
  _BS_word m = 0;

  for (unsigned int  i = 0; i < l; ++i)
  {
    if (i % BITSTRBITS == 0)
    {
      a = *ps++;
      m = *ms++;
    }
    if (m & 1)
      sb->sputc((a & 1)? t : f);
    else
      sb->sputc(x);
    a >>= 1;
    m >>= 1;
  }
}

ostream& operator << (ostream& s, const BitPattern& x)
{
  if (s.opfx())
    x.printon(s);
  return s;
}


int BitString::OK() const
{
  int v = rep != 0;             // have a rep;
  v &= BitStr_len(rep->len) <= rep->sz; // within allocated size
  if (!v) error("invariant failure");
  return v;
}

int BitSubString::OK() const
{
  int v = S.OK();               // valid BitString
  v &= pos + len <= S.rep->len; // within bounds of targ
  if (!v) S.error("BitSubString invariant failure");
  return v;
}

int BitPattern::OK() const
{
  int v = pattern.OK() && mask.OK();
  if (!v) pattern.error("BitPattern invariant failure");
  return v;
}

