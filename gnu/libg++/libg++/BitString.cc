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

#define ONES  ((unsigned short)(~0L))
#define MAXBIT  (1 << (BITSTRBITS - 1))

/*
 *  bit manipulation utilities
*/

// break things up into .s indices and positions

inline static int BitStr_len(int l)
{
  return (unsigned)(l) / BITSTRBITS + 1;
}


// mask out low bits

static inline unsigned short lmask(int p)
{
  if (p <= 0)
    return ONES;
  else
    return ONES << p;
}

// mask out high bits

static inline unsigned short rmask(int p)
{
  int s = BITSTRBITS - 1 - p;
  if (s <= 0)
    return ONES;
  else
    return ONES >> s;
}


// mask out unused bits in last word of rep

inline static void check_last(BitStrRep* r)
{
  r->s[r->len / BITSTRBITS] &= ONES >> (BITSTRBITS - (r->len & (BITSTRBITS - 1)));
}

// merge bits from next word

static inline unsigned short borrow_hi(const unsigned short a[], int ind, 
                                      int maxind, int p)
{
  if (ind < maxind)
    return (a[ind] >> p) | (a[ind+1] << (BITSTRBITS - p));
  else
    return (a[ind] >> p);
}

// merge bits from prev word

static inline unsigned short borrow_lo(const unsigned short a[], int ind, 
                                      int minind, int p)
{
  if (ind > minind)
    return (a[ind] << (BITSTRBITS - 1 - p)) | (a[ind-1] >> (p + 1));
  else
    return (a[ind] << (BITSTRBITS - 1 - p));
}

// same with bounds check (for masks shorter than patterns)

static inline unsigned short safe_borrow_hi(const unsigned short a[], int ind, 
                                           int maxind, int p)
{
  if (ind > maxind)
    return 0;
  else if (ind == maxind)
    return(a[ind] >> p);
  else
    return (a[ind] >> p) | (a[ind+1] << (BITSTRBITS - p));
}


static inline unsigned short safe_borrow_lo(const unsigned short a[], int ind, 
                                            int minind, int p)
{
  if (ind < minind)
    return 0;
  else if (ind == minind)
    return (a[ind] << (BITSTRBITS - 1 - p));
  else
    return (a[ind] << (BITSTRBITS - 1 - p)) | (a[ind-1] >> (p + 1));
}

// copy bits from a word boundary

static inline void bit_copy(const unsigned short* ss, unsigned short* ds, 
                            int nbits)
{
  if (ss != ds)
  {
    int n = (unsigned)(nbits) / BITSTRBITS;
    if (n > 0) memmove((void*)ds, (const void*)ss, n * sizeof(short));
    unsigned short m = ONES << (nbits & (BITSTRBITS - 1));
    ds[n] = (ss[n] & ~m) | (ds[n] & m);
  }
}

// clear bits from a word boundary

static inline void bit_clear(unsigned short* ds, int nbits)
{
  int n = (unsigned)(nbits) / BITSTRBITS;
  if (n > 0) memset((void*)ds, 0, n * sizeof(short));
  ds[n] &= ONES << (nbits & (BITSTRBITS - 1));
}

  

// Copy ss from starts to fences-1 into ds starting at startd.
// This will work even if ss & ds overlap.
// The g++ optimizer does very good things with the messy shift expressions!

static void bit_transfer(const unsigned short* ss, int starts, int fences,
                         unsigned short* ds, int startd)
{
  if (starts >= fences || ss == 0 || (ss == ds && starts == startd))
    return;

  int sind = BitStr_index(starts);
  int spos = BitStr_pos(starts);
  int dind = BitStr_index(startd);
  int dpos = BitStr_pos(startd);

  if (spos == 0 && dpos == 0)
  {
    bit_copy(&ss[sind], &ds[dind], fences - starts);
    return;
  }

  int ends = fences - 1;
  int endsind = BitStr_index(ends);
  int endspos = BitStr_pos(ends);
  int endd = startd + (ends - starts);
  int enddind = BitStr_index(endd);
  int enddpos = BitStr_pos(endd);

  if (dind == enddind)
  {
    if (sind == endsind)
      ds[dind] = (ds[dind] & ((ONES >> (BITSTRBITS - dpos)) | 
                              (ONES << (enddpos + 1)))) | 
                                (((ss[sind] >> spos) << dpos) & 
                                 ~((ONES >> (BITSTRBITS - dpos)) | 
                                   (ONES << (enddpos + 1))));
    else
      ds[dind] = (ds[dind] & ((ONES >> (BITSTRBITS - dpos)) | 
                              (ONES << (enddpos + 1)))) | 
                                ((((ss[sind] >> spos) | 
                                   (ss[sind+1] << (BITSTRBITS - spos))) 
                                  << dpos) & 
                                 ~((ONES >> (BITSTRBITS - dpos)) | 
                                   (ONES << (enddpos + 1))));
    return;
  }
  else if (sind == endsind)
  {
    unsigned short saveend = (ds[enddind] & (ONES << (enddpos + 1))) | 
        (((ss[sind] << (BITSTRBITS - 1 - endspos)) >> 
          (BITSTRBITS - 1 - enddpos)) & ~(ONES << (enddpos + 1)));
    ds[dind] = (ds[dind] & (ONES >> (BITSTRBITS - dpos))) |
        (((ss[sind] >> spos) << dpos) & ~(ONES >> (BITSTRBITS - dpos)));
    ds[enddind] = saveend;
    return;
  }

  unsigned short saveend = (ds[enddind] & (ONES << (enddpos + 1))) | 
    ((((ss[endsind] << (BITSTRBITS - 1 - endspos)) |
       (ss[endsind-1] >> (endspos + 1))) >> 
      (BITSTRBITS - 1 - enddpos)) & ~(ONES << (enddpos + 1)));
  unsigned short savestart = (ds[dind] & (ONES >> (BITSTRBITS - dpos))) |
    ((((ss[sind] >> spos) | (ss[sind+1] << (BITSTRBITS - spos))) << dpos) 
     & ~(ONES >> (BITSTRBITS - dpos)));


  if (ds != ss || startd < starts)
  {
    int pos = spos - dpos;
    if (pos < 0)
      pos += BITSTRBITS;
    else
      ++sind;
    
    for (;;)                    // lag by one in case of overlaps
    {
      if (dind == enddind - 1)
      {
        ds[dind] = savestart;
        ds[enddind] = saveend;
        return;
      }
      else
      {
        unsigned short tmp = ss[sind] >> pos;
        if (++sind <= endsind) tmp |= ss[sind] << (BITSTRBITS - pos);
        ds[dind++] = savestart;
        savestart = tmp;
      }
    }
  }
  else
  {
    int pos = endspos - enddpos;
    if (pos <= 0)
    {
      pos += BITSTRBITS;
      --endsind;
    }
    for (;;)
    {
      if (enddind == dind + 1)
      {
        ds[enddind] = saveend;
        ds[dind] = savestart;
        return;
      }
      else
      {
        unsigned short tmp = ss[endsind] << (BITSTRBITS - pos);
        if (--endsind >= sind) tmp |= ss[endsind] >> pos;
        ds[enddind--] = saveend;
        saveend = tmp;
      }
    }
  }
}
  
// allocate a new rep; pad to near a power of two

inline static BitStrRep* BSnew(int newlen)
{
  unsigned int siz = sizeof(BitStrRep) + BitStr_len(newlen) * sizeof(short) 
    + MALLOC_MIN_OVERHEAD;
  unsigned int allocsiz = MINBitStrRep_SIZE;;
  while (allocsiz < siz) allocsiz <<= 1;
  allocsiz -= MALLOC_MIN_OVERHEAD;
  if (allocsiz >= MAXBitStrRep_SIZE * sizeof(short))
    (*lib_error_handler)("BitString", "Requested length out of range");
    
  BitStrRep* rep = (BitStrRep *) new char[allocsiz];
  memset(rep, 0, allocsiz);
  rep->sz = (allocsiz - sizeof(BitStrRep) + sizeof(short)) / sizeof(short);
  return rep;
}

BitStrRep* BStr_alloc(BitStrRep* old, const unsigned short* src,
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
    bit_transfer(src, startpos, endp, rep->s, 0);

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
    memcpy(rep->s, old->s, BitStr_len(old->len) * sizeof(short));
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
    
    memcpy(rep->s, src->s, news * sizeof(short));
    rep->len = newlen;
  }
  check_last(rep);
  return rep;
}


int operator == (const BitString& x, const BitString& y)
{
  return x.rep->len == y.rep->len && 
    memcmp((void*)x.rep->s, (void*)y.rep->s, 
         BitStr_len(x.rep->len) * sizeof(short)) == 0;
}

int operator <= (const BitString& x, const BitString& y)
{
  unsigned int  xl = x.rep->len;
  unsigned int  yl = y.rep->len;
  if (xl > yl)
    return 0;

  const unsigned short* xs = x.rep->s;
  const unsigned short* topx = &(xs[BitStr_len(xl)]);
  const unsigned short* ys = y.rep->s;

  while (xs < topx)
  {
    unsigned short a = *xs++;
    unsigned short b = *ys++;
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

  const unsigned short* xs = x.rep->s;
  const unsigned short* ys = y.rep->s;
  const unsigned short* topx = &(xs[BitStr_len(xl)]);
  const unsigned short* topy = &(ys[BitStr_len(yl)]);
  int one_diff = 0;
  while (xs < topx)
  {
    unsigned short a = *xs++;
    unsigned short b = *ys++;
    unsigned short c = a | b;
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
  unsigned int  xl = x.rep->len;
  unsigned int  yl = y.rep->len;

  const unsigned short* xs = x.rep->s;
  const unsigned short* topx = &(xs[BitStr_len(xl)]);
  const unsigned short* ys = y.rep->s;
  const unsigned short* topy = &(ys[BitStr_len(yl)]);

  while (xs < topx && ys < topy)
  {
    unsigned short a = *xs++;
    unsigned short b = *ys++;
    if (a != b)
    {
      unsigned short mask = 1;
      for (;;)
      {
        unsigned short abit = (a & mask) != 0;
        unsigned short bbit = (b & mask) != 0;
        int diff = abit - bbit;
        if (diff != 0)
          return diff;
        else
          mask <<= 1;
      }
    }
  }
  return xl - yl;
}

int BitString::count(unsigned int b) const
{
  check_last(rep);
  int xwds = BitStr_len(rep->len);
  int xlast = BitStr_pos(rep->len);
  int l = 0;
  const unsigned short* s = rep->s;
  const unsigned short* tops = &(s[xwds - 1]);
  unsigned short a;
  int i;
  if (b != 0)
  {
    while (s < tops)
    {
      a = *s++;
      for (i = 0; i < BITSTRBITS && a != 0; ++i)
      {
        if (a & 1)
          ++l;
        a >>= 1;
      }
    }
    a = *s;
    for (i = 0; i < xlast && a != 0; ++i)
    {
      if (a & 1)
        ++l;
      a >>= 1;
    }
  }
  else
  {
    unsigned short maxbit = 1 << (BITSTRBITS - 1);
    while (s < tops)
    {
      a = *s++;
      for (i = 0; i < BITSTRBITS; ++i)
      {
        if ((a & maxbit) == 0)
          ++l;
        a <<= 1;
      }
    }
    maxbit = 1 << (xlast - 1);
    a = *s;
    for (i = 0; i < xlast; ++i)
    {
      if ((a & maxbit) == 0)
        ++l;
      a <<= 1;
    }
  }
  return l;
}


BitStrRep* cmpl(const BitStrRep* src, BitStrRep* r)
{
  r = BStr_copy(r, src);
  unsigned short* rs = r->s;
  unsigned short* topr = &(rs[BitStr_len(r->len)]);
  while (rs < topr)
  {
    unsigned short cmp = ~(*rs);
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

  unsigned short* rs = r->s;
  unsigned short* topr = &(rs[BitStr_len(rl)]);
  const unsigned short* xs = (xrsame)? rs : x->s;
  const unsigned short* ys = (yrsame)? rs : y->s;

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

  unsigned short* rs = r->s;
  const unsigned short* xs = (xrsame)? rs : x->s;
  const unsigned short* topx = &(xs[BitStr_len(xl)]);
  const unsigned short* ys = (yrsame)? rs : y->s;
  const unsigned short* topy = &(ys[BitStr_len(yl)]);

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

  unsigned short* rs = r->s;
  const unsigned short* xs = (xrsame)? rs : x->s;
  const unsigned short* topx = &(xs[BitStr_len(xl)]);
  const unsigned short* ys = (yrsame)? rs : y->s;
  const unsigned short* topy = &(ys[BitStr_len(yl)]);

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

  unsigned short* rs = r->s;
  const unsigned short* xs = (xrsame)? rs : x->s;
  const unsigned short* topx = &(xs[BitStr_len(xl)]);
  const unsigned short* ys = (yrsame)? rs : y->s;
  const unsigned short* topy = &(ys[BitStr_len(yl)]);

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
      bit_transfer(r->s, 0, yl, r->s, xl);
    }
    else
    {
      BitStrRep* tmp = BStr_copy(0, y);
      r = BStr_resize(r, rl);
      bit_copy(x->s, r->s, xl);
      bit_transfer(tmp->s, 0, yl, r->s, xl);
      delete tmp;
    }
  }
  else
  {
    r = BStr_resize(r, rl);
    if (!xrsame) bit_copy(x->s, r->s, xl);
    bit_transfer(y->s, 0, yl, r->s, xl);
  }
  check_last(r);
  return r;
}

BitStrRep* cat(const BitStrRep* x, unsigned int bit, BitStrRep* r)
{
  unsigned int  xl = x->len;
  int xrsame = x == r;
  r = BStr_resize(r, xl+1);
  if (!xrsame) bit_copy(x->s, r->s, xl);
  if (bit)
    r->s[BitStr_index(xl)] |= (1 << (BitStr_pos(xl)));
  else
    r->s[BitStr_index(xl)] &= ~(1 << (BitStr_pos(xl)));
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
    const unsigned short* xs = (xrsame)? r->s : x->s;
    bit_transfer(xs, 0, xl, r->s, s);
    bit_clear(r->s, s);
  }
  else if (xrsame)
  {
    r = BStr_resize(r, xl);
    r->len = rl;
    bit_transfer(r->s, -s, xl, r->s, 0);
  }
  else
  {
    r = BStr_resize(r, rl);
    bit_transfer(x->s, -s, xl, r->s, 0);
  }
  check_last(r);
  return r;
}


void BitString::set(int p)
{
  if (p < 0) error("Illegal bit index");
  if ((unsigned)(p) >= rep->len) rep = BStr_resize(rep, p + 1);
  rep->s[BitStr_index(p)] |= (1 << (BitStr_pos(p)));
}

void BitString::assign(int p, unsigned int bit)
{
  if (p < 0) error("Illegal bit index");
  if ((unsigned)(p) >= rep->len) rep = BStr_resize(rep, p + 1);
  if (bit)
    rep->s[BitStr_index(p)] |= (1 << (BitStr_pos(p)));
  else
    rep->s[BitStr_index(p)] &= ~(1 << (BitStr_pos(p)));
}

void BitString::clear(int p)
{
  if (p < 0) error("Illegal bit index");
  if ((unsigned)(p) >= rep->len) rep = BStr_resize(rep, p + 1);
  rep->s[BitStr_index(p)] &= ~(1 << (BitStr_pos(p)));
}

void BitString::clear()
{
  if (rep == &_nilBitStrRep) return;
  bit_clear(rep->s, rep->len);
}

void BitString::set()
{
  if (rep == &_nilBitStrRep) return;
  unsigned short* s = rep->s;
  unsigned short* tops = &(s[BitStr_len(rep->len)]);
  while (s < tops) *s++ = ONES;
  check_last(rep);
}

void BitString::invert(int p)
{
  if (p < 0) error("Illegal bit index");
  if ((unsigned)(p) >= rep->len) rep = BStr_resize(rep, p + 1);
  rep->s[BitStr_index(p)] ^= (1 << (BitStr_pos(p)));
}



void BitString::set(int from, int to)
{
  if (from < 0 || from > to) error("Illegal bit index");
  if ((unsigned)(to) >= rep->len) rep = BStr_resize(rep, to+1);

  int ind1 = BitStr_index(from);
  int pos1 = BitStr_pos(from);
  int ind2 = BitStr_index(to);
  int pos2 = BitStr_pos(to);
  unsigned short* s = &(rep->s[ind1]);
  unsigned short m1 = lmask(pos1);
  unsigned short m2 = rmask(pos2);
  if (ind2 == ind1)
    *s |= m1 & m2;
  else
  {
    *s++ |= m1;
    unsigned short* top = &(rep->s[ind2]);
    *top |= m2;
    while (s < top)
      *s++ = ONES;
  }
}

void BitString::clear(int from, int to)
{
  if (from < 0 || from > to) error("Illegal bit index");
  if ((unsigned)(to) >= rep->len) rep = BStr_resize(rep, to+1);

  int ind1 = BitStr_index(from);
  int pos1 = BitStr_pos(from);
  int ind2 = BitStr_index(to);
  int pos2 = BitStr_pos(to);
  unsigned short* s = &(rep->s[ind1]);
  unsigned short m1 = lmask(pos1);
  unsigned short m2 = rmask(pos2);
  if (ind2 == ind1)
    *s &= ~(m1 & m2);
  else
  {
    *s++ &= ~m1;
    unsigned short* top = &(rep->s[ind2]);
    *top &= ~m2;
    while (s < top)
      *s++ = 0;
  }
}

void BitString::invert(int from, int to)
{
  if (from < 0 || from > to) error("Illegal bit index");
  if ((unsigned)(to) >= rep->len) rep = BStr_resize(rep, to+1);

  int ind1 = BitStr_index(from);
  int pos1 = BitStr_pos(from);
  int ind2 = BitStr_index(to);
  int pos2 = BitStr_pos(to);
  unsigned short* s = &(rep->s[ind1]);
  unsigned short m1 = lmask(pos1);
  unsigned short m2 = rmask(pos2);
  if (ind2 == ind1)
    *s ^= m1 & m2;
  else
  {
    *s++ ^= m1;
    unsigned short* top = &(rep->s[ind2]);
    *top ^= m2;
    while (s < top)
    {
      unsigned short cmp = ~(*s);
      *s++ = cmp;
    }
  }
}


int BitString::test(int from, int to) const
{
  if (from < 0 || from > to || (unsigned)(from) >= rep->len) return 0;
  
  int ind1 = BitStr_index(from);
  int pos1 = BitStr_pos(from);
  int ind2 = BitStr_index(to);
  int pos2 = BitStr_pos(to);
  
  if ((unsigned)(to) >= rep->len)
  {
    ind2 = BitStr_index(rep->len - 1);
    pos2 = BitStr_pos(rep->len - 1);
  }
  
  const unsigned short* s = &(rep->s[ind1]);
  unsigned short m1 = lmask(pos1);
  unsigned short m2 = rmask(pos2);
  
  if (ind2 == ind1)
    return (*s & m1 & m2) != 0;
  else
  {
    if (*s++ & m1)
      return 1;
    unsigned short* top = &(rep->s[ind2]);
    if (*top & m2)
      return 1;
    while (s < top)
      if (*s++ != 0) 
        return 1;
    return 0;
  }
}

int BitString::next(int p, unsigned int b) const
{
  if ((unsigned)(++p) >= rep->len)
    return -1;

  int ind = BitStr_index(p);
  int pos = BitStr_pos(p);
  int l = BitStr_len(rep->len);

  int j = ind;
  const unsigned short* s = rep->s;
  unsigned short a = s[j] >> pos;
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

  const unsigned short* s = rep->s;

  if ((unsigned)(p) >= rep->len)
  {
    ind = BitStr_index(rep->len - 1);
    pos = BitStr_pos(rep->len - 1);
  }

  int j = ind;
  unsigned short a = s[j];

  int i = pos;
  unsigned short maxbit = 1 << pos;

  if (b != 0)
  {
    for (; i >= 0 && a != 0; --i)
    {
      if (a & maxbit)
        return j * BITSTRBITS + i;
      a <<= 1;
    }
    maxbit = 1 << (BITSTRBITS - 1);
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
    maxbit = 1 << (BITSTRBITS - 1);
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
                      const unsigned short* ys, int starty, int lengthy) const
{
  const unsigned short* xs = rep->s;
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

    unsigned short x = borrow_hi(xs, xind, rightxind, xpos);
  
    int rightyind = BitStr_index(righty);
    int rightypos = BitStr_pos(righty);
    unsigned short y = borrow_hi(ys, yind, rightyind, ypos);
    unsigned short ymask;
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
          unsigned short tx = borrow_hi(xs, xi, rightxind, xpos);
          unsigned short ty = borrow_hi(ys, yi, rightyind, ypos);
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

    unsigned short x = borrow_hi(xs, xind, rightxind, xpos);
    unsigned short nextx = (xind >= rightxind) ? 0 : (xs[xind+1] >> xpos);
  
    int rightyind = BitStr_index(righty);
    int rightypos = BitStr_pos(righty);
    unsigned short y = borrow_hi(ys, yind, rightyind, ypos);
    unsigned short ymask;
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
          unsigned short tx = borrow_hi(xs, xi, rightxind, xpos);
          unsigned short ty = borrow_hi(ys, yi, rightyind, ypos);
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


int BitPattern::search(const unsigned short* xs, int startx, int lengthx) const
{
  const unsigned short* ys = pattern.rep->s;
  const unsigned short* ms = mask.rep->s;
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
    
    unsigned short x = safe_borrow_hi(xs, xind, rightxind, xpos);
    unsigned short m = safe_borrow_hi(ms, 0, rightmind, 0);
    unsigned short y = safe_borrow_hi(ys, 0, rightyind, 0) & m;
    
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
          unsigned short tm = safe_borrow_hi(ms, yi, rightmind, 0);
          unsigned short ty = safe_borrow_hi(ys, yi, rightyind, 0);
          unsigned short tx = safe_borrow_hi(xs, xi, rightxind, xpos);
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
    
    unsigned short x = safe_borrow_hi(xs, xind, rightxind, xpos);
    unsigned short m = safe_borrow_hi(ms, 0, rightmind, 0);
    unsigned short y = safe_borrow_hi(ys, 0, rightyind, 0) & m;

    unsigned short nextx = (xind >= rightxind) ? 0 : (xs[xind+1] >> xpos);
    
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
          unsigned short tm = safe_borrow_hi(ms, yi, rightmind, 0);
          unsigned short ty = safe_borrow_hi(ys, yi, rightyind, 0);
          unsigned short tx = safe_borrow_hi(xs, xi, rightxind, xpos);
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
                     const unsigned short* ys, int starty, int yl) const
{
  const unsigned short* xs = rep->s;
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
    unsigned short x = borrow_hi(xs, xi, rightxind, xpos);
    unsigned short y = borrow_hi(ys, yi, rightyind, ypos);
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

int BitPattern::match(const unsigned short* xs, int startx, 
                      int lengthx, int exact) const
{
  const unsigned short* ys = pattern.rep->s;
  int righty = pattern.rep->len - 1;
  unsigned short* ms = mask.rep->s;
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
    unsigned short m = safe_borrow_hi(ms, yind, rightmind, 0);
    unsigned short x = safe_borrow_hi(xs, xind, rightxind, xpos) & m;
    unsigned short y = safe_borrow_hi(ys, yind, rightyind, 0) & m;
    if (x != y)
      return 0;
    else if (++yind > rightyind || ++xind > rightxind)
      return 1;
  }
}

void BitSubString::operator = (const BitString& y)
{
  if (&S == &_nil_BitString) return;
  BitStrRep* targ = S.rep;

  unsigned int ylen = y.rep->len;
  int sl = targ->len - len + ylen;

  if (y.rep == targ || ylen > len)
  {
    BitStrRep* oldtarg = targ;
    targ = BStr_alloc(0, 0, 0, 0, sl);
    bit_transfer(oldtarg->s, 0, pos, targ->s, 0);
    bit_transfer(y.rep->s, 0, ylen, targ->s, pos);
    bit_transfer(oldtarg->s, pos+len, oldtarg->len, targ->s, pos + ylen);
    delete oldtarg;
  }
  else if (len == ylen)
    bit_transfer(y.rep->s, 0, len, targ->s, pos);
  else if (ylen < len)
  {
    bit_transfer(y.rep->s, 0, ylen, targ->s, pos);
    bit_transfer(targ->s, pos+len, targ->len, targ->s, pos + ylen);
    targ->len = sl;
  }
  check_last(targ);
  S.rep = targ;
}

void BitSubString::operator = (const BitSubString& y)
{
  if (&S == &_nil_BitString) return;
  BitStrRep* targ = S.rep;
  
  if (len == 0 || pos >= targ->len)
    return;
  
  int sl = targ->len - len + y.len;
  
  if (y.S.rep == targ || y.len > len)
  {
    BitStrRep* oldtarg = targ;
    targ = BStr_alloc(0, 0, 0, 0, sl);
    bit_copy(oldtarg->s, targ->s, pos);
    bit_transfer(y.S.rep->s, y.pos, y.pos+y.len, targ->s, pos);
    bit_transfer(oldtarg->s, pos+len, oldtarg->len, targ->s, pos + y.len);
    delete oldtarg;
  }
  else if (len == y.len)
    bit_transfer(y.S.rep->s, y.pos, y.pos+y.len, targ->s, pos);
  else if (y.len < len)
  {
    bit_transfer(y.S.rep->s, y.pos, y.pos+y.len, targ->s, pos);
    bit_transfer(targ->s, pos+len, targ->len, targ->s, pos + y.len);
    targ->len = sl;
  }
  check_last(targ);
  S.rep = targ;
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

#if defined(__GNUG__) && !defined(NO_NRV)

BitString common_prefix(const BitString& x, const BitString& y, int startpos)
     return r
{
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
    return;

  const unsigned short* xs = &(x.rep->s[BitStr_index(startx)]);
  unsigned short a = *xs++;
  unsigned int xp = startx;

  const unsigned short* ys = &(y.rep->s[BitStr_index(starty)]);
  unsigned short b = *ys++;
  unsigned int yp = starty;

  for(; xp < xl && yp < yl; ++xp, ++yp)
  {
    unsigned short xbit = 1 << (BitStr_pos(xp));
    unsigned short ybit = 1 << (BitStr_pos(yp));
    if (((a & xbit) == 0) != ((b & ybit) == 0))
      break;
    if (xbit == MAXBIT)
      a = *xs++;
    if (ybit == MAXBIT)
      b = *ys++;
  }
  r.rep = BStr_alloc(0, x.rep->s, startx, xp, xp - startx);
}


BitString common_suffix(const BitString& x, const BitString& y, int startpos)
     return r;
{
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
    return;

  const unsigned short* xs = &(x.rep->s[BitStr_index(startx)]);
  unsigned short a = *xs--;
  int xp = startx;

  const unsigned short* ys = &(y.rep->s[BitStr_index(starty)]);
  unsigned short b = *ys--;
  int yp = starty;

  for(; xp >= 0 && yp >= 0; --xp, --yp)
  {
    unsigned short xbit = 1 << (BitStr_pos(xp));
    unsigned short ybit = 1 << (BitStr_pos(yp));
    if (((a & xbit) == 0) != ((b & ybit) == 0))
      break;
    if (xbit == 1)
      a = *xs--;
    if (ybit == 1)
      b = *ys--;
  }
  r.rep = BStr_alloc(0, x.rep->s, xp+1, startx+1, startx - xp);
}

BitString reverse(const BitString& x) return r
{
  unsigned int  yl = x.rep->len;
  BitStrRep* y = BStr_resize(0, yl);
  if (yl > 0)
  {
    const unsigned short* ls = x.rep->s;
    unsigned short lm = 1;
    unsigned short* rs = &(y->s[BitStr_index(yl - 1)]);
    unsigned short rm = 1 << (BitStr_pos(yl - 1));
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
}

BitString atoBitString(const char* s, char f, char t) return res
{
  int sl = strlen(s);
  BitStrRep* r = BStr_resize(0, sl);
  if (sl != 0)
  {
    unsigned int  rl = 0;
    unsigned short* rs = r->s;
    unsigned short a = 0;
    unsigned short m = 1;
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
}

BitPattern atoBitPattern(const char* s, char f,char t,char x) return r
{
  int sl = strlen(s);
  if (sl != 0)
  {
    unsigned int  rl = 0;
    r.pattern.rep = BStr_resize(r.pattern.rep, sl);
    r.mask.rep = BStr_resize(r.mask.rep, sl);
    unsigned short* rs = r.pattern.rep->s;
    unsigned short* ms = r.mask.rep->s;
    unsigned short a = 0;
    unsigned short b = 0;
    unsigned short m = 1;
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
  return;
}

#else /* NO_NRV */

BitString common_prefix(const BitString& x, const BitString& y, int startpos)
{
  BitString r;

  unsigned int  xl = x.rep->len;
  unsigned int  yl = y.rep->len;

  int startx, starty;
  if (startpos < 0)
  {
    startx = xl + startpos;
    starty = yl + startpos;
  }
  else
    startx = starty = startpos;

  if (startx < 0 || startx >= xl || starty < 0 || starty >= yl)
    return r;

  const unsigned short* xs = &(x.rep->s[BitStr_index(startx)]);
  unsigned short a = *xs++;
  int xp = startx;

  const unsigned short* ys = &(y.rep->s[BitStr_index(starty)]);
  unsigned short b = *ys++;
  int yp = starty;

  for(; xp < xl && yp < yl; ++xp, ++yp)
  {
    unsigned short xbit = 1 << (BitStr_pos(xp));
    unsigned short ybit = 1 << (BitStr_pos(yp));
    if (((a & xbit) == 0) != ((b & ybit) == 0))
      break;
    if (xbit == MAXBIT)
      a = *xs++;
    if (ybit == MAXBIT)
      b = *ys++;
  }
  r.rep = BStr_alloc(0, x.rep->s, startx, xp, xp - startx);
  return r;
}


BitString common_suffix(const BitString& x, const BitString& y, int startpos)
{
  BitString r;
  unsigned int  xl = x.rep->len;
  unsigned int  yl = y.rep->len;

  int startx, starty;
  if (startpos < 0)
  {
    startx = xl + startpos;
    starty = yl + startpos;
  }
  else
    startx = starty = startpos;

  if (startx < 0 || startx >= xl || starty < 0 || starty >= yl)
    return r;

  const unsigned short* xs = &(x.rep->s[BitStr_index(startx)]);
  unsigned short a = *xs--;
  int xp = startx;

  const unsigned short* ys = &(y.rep->s[BitStr_index(starty)]);
  unsigned short b = *ys--;
  int yp = starty;

  for(; xp >= 0 && yp >= 0; --xp, --yp)
  {
    unsigned short xbit = 1 << (BitStr_pos(xp));
    unsigned short ybit = 1 << (BitStr_pos(yp));
    if (((a & xbit) == 0) != ((b & ybit) == 0))
      break;
    if (xbit == 1)
      a = *xs--;
    if (ybit == 1)
      b = *ys--;
  }
  r.rep = BStr_alloc(0, x.rep->s, xp+1, startx+1, startx - xp);
  return r;
}

BitString reverse(const BitString& x)
{
  BitString r;
  unsigned int  yl = x.rep->len;
  BitStrRep* y = BStr_resize(0, yl);
  if (yl > 0)
  {
    const unsigned short* ls = x.rep->s;
    unsigned short lm = 1;
    unsigned short* rs = &(y->s[BitStr_index(yl - 1)]);
    unsigned short rm = 1 << (BitStr_pos(yl - 1));
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
  return r;
}

BitString atoBitString(const char* s, char f, char t)
{
  BitString res;
  int sl = strlen(s);
  BitStrRep* r = BStr_resize(0, sl);
  if (sl != 0)
  {
    unsigned int  rl = 0;
    unsigned short* rs = r->s;
    unsigned short a = 0;
    unsigned short m = 1;
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
  return res;
}

BitPattern atoBitPattern(const char* s, char f,char t,char x)
{
  BitPattern r;
  int sl = strlen(s);
  if (sl != 0)
  {
    unsigned int  rl = 0;
    r.pattern.rep = BStr_resize(r.pattern.rep, sl);
    r.mask.rep = BStr_resize(r.mask.rep, sl);
    unsigned short* rs = r.pattern.rep->s;
    unsigned short* ms = r.mask.rep->s;
    unsigned short a = 0;
    unsigned short b = 0;
    unsigned short m = 1;
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
  return r;
}

#endif

extern AllocRing _libgxx_fmtq;

void BitString::printon(ostream& os, char f, char t) const
{
  unsigned int  xl = rep->len;
  const unsigned short* ptr = rep->s;
  register streambuf *sb = os.rdbuf();
  unsigned short a = 0;

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

  const unsigned short* ps = pattern.rep->s;
  const unsigned short* ms = mask.rep->s;
  unsigned short a = 0;
  unsigned short m = 0;

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

