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
  BitSet class implementation
 */

#ifdef __GNUG__
#pragma implementation
#endif
#include <BitSet.h>
#include <std.h>
#include <limits.h>
#include <Obstack.h>
#include <AllocRing.h>
#include <new.h>
#include <builtin.h>
#include <string.h>
#include <strstream.h>

#undef OK

void BitSet::error(const char* msg) const
{
  (*lib_error_handler)("BitSet", msg);
}

//  globals & constants

BitSetRep  _nilBitSetRep = { 0, 1, 0, {0} }; // nil BitSets point here

#define ONES               ((_BS_word)(~0L))
#define MASK1(BITNO)  ((_BS_word)1 << (BITNO))
#define MAXBitSetRep_SIZE  (((_BS_word)1 << (sizeof(unsigned short)*CHAR_BIT - 1)) - 1)
#define MINBitSetRep_SIZE  (sizeof(_BS_word)*CHAR_BIT)

#ifndef MALLOC_MIN_OVERHEAD
#define MALLOC_MIN_OVERHEAD     4
#endif

// break things up into .s indices and positions


// mask out bits from left

static inline _BS_word lmask(int p)
{
  return ONES << p;
}

// mask out high bits

static inline _BS_word rmask(int p)
{
  return ONES >> (BITSETBITS - 1 - p);
}


inline static BitSetRep* BSnew(int newlen)
{
  unsigned int siz = sizeof(BitSetRep) + newlen * sizeof(_BS_word) 
    + MALLOC_MIN_OVERHEAD;
  unsigned int allocsiz = MINBitSetRep_SIZE;;
  while (allocsiz < siz) allocsiz <<= 1;
  allocsiz -= MALLOC_MIN_OVERHEAD;
  if (allocsiz >= MAXBitSetRep_SIZE * sizeof(_BS_word))
    (*lib_error_handler)("BitSet", "Requested length out of range");
    
  BitSetRep* rep = new (operator new (allocsiz)) BitSetRep;
  memset(rep, 0, allocsiz);
  rep->sz = (allocsiz - sizeof(BitSetRep) + sizeof(_BS_word)) / sizeof(_BS_word);
  return rep;
}

BitSetRep* BitSetalloc(BitSetRep* old, const _BS_word* src, int srclen,
                int newvirt, int newlen)
{
  if (old == &_nilBitSetRep) old = 0;
  BitSetRep* rep;
  if (old == 0 || newlen >= old->sz)
    rep = BSnew(newlen);
  else
    rep = old;

  rep->len = newlen;
  rep->virt = newvirt;

  if (srclen != 0 && src != rep->s)
    memcpy(rep->s, src, srclen * sizeof(_BS_word));
  // BUG fix: extend virtual bit! 20 Oct 1992 Kevin Karplus
  if (rep->virt)
      memset(&rep->s[srclen], (char)ONES,
	     (newlen - srclen) * sizeof(_BS_word));
  if (old != rep && old != 0) delete old;
  return rep;
}

BitSetRep* BitSetresize(BitSetRep* old, int newlen)
{
  BitSetRep* rep;
  if (old == 0 || old == &_nilBitSetRep)
  {
    rep = BSnew(newlen);
    rep->virt = 0;
  }
  else if (newlen >= old->sz)
  {
    rep = BSnew(newlen);
    memcpy(rep->s, old->s, old->len * sizeof(_BS_word));
    rep->virt = old->virt;
    // BUG fix: extend virtual bit!  20 Oct 1992 Kevin Karplus
    if (rep->virt)
	memset(&rep->s[old->len], (char)ONES,
	       (newlen - old->len) * sizeof(_BS_word));
    delete old;
  }
  else
    {
      rep = old;
      if (rep->len < newlen)
	memset(&rep->s[rep->len],
	       rep->virt ? (char)ONES : (char)0,
	       (newlen - rep->len) * sizeof(_BS_word));
    }

  rep->len = newlen;

  return rep;
}

// same, for straight copy

BitSetRep* BitSetcopy(BitSetRep* old, const BitSetRep* src)
{
  BitSetRep* rep;
  if (old == &_nilBitSetRep) old = 0;
  if (src == 0 || src == &_nilBitSetRep)
  {
    if (old == 0)
      rep = BSnew(0);
    else
      rep = old;
    rep->len = 0;
    rep->virt = 0;
  }
  else if (old == src) 
    return old; 
  else 
  {
    int newlen = src->len;
    if (old == 0 || newlen > old->sz)
    {
      rep = BSnew(newlen);
      if (old != 0) delete old;
    }
    else
      rep = old;

    memcpy(rep->s, src->s, newlen * sizeof(_BS_word));
    rep->len = newlen;
    rep->virt = src->virt;
  }
  return rep;
}


// remove unneeded top bits

inline static void trim(BitSetRep* rep)
{
  int l = rep->len;
  _BS_word* s = &(rep->s[l - 1]);

  if (rep->virt == 0)
    while (l > 0 && *s-- == 0) --l;
  else
    while (l > 0 && *s-- == ONES) --l;
  rep->len = l;
}

int operator == (const BitSet& x, const BitSet& y)
{
  if (x.rep->virt != y.rep->virt)
    return 0;
  int xl = x.rep->len;
  int yl = y.rep->len;

  const _BS_word* xs = x.rep->s;
  const _BS_word* ys = y.rep->s;
  if (xl < yl) {
    if (memcmp((void*)xs, (void*)ys, xl * sizeof(_BS_word)))
      return 0;
    ys+=xl;
    const _BS_word* topy = &(ys[yl]);
    while (ys<topy)
      if (x.rep->virt?((*ys++)!=ONES):((*ys++)!=0))
	return 0;
  }
  else {
    if (memcmp((void*)xs, (void*)ys, yl * sizeof(_BS_word)))
      return 0;
    if (xl > yl) {
      xs+=yl;
      const _BS_word* topx = &(xs[xl]);
      while (xs<topx)
	if (y.rep->virt?((*xs++)!=ONES):((*xs++)!=0)) 
	  return 0;
    }
  }
  return 1;
}

int operator <= (const BitSet& x, const BitSet& y)
{
  if (x.rep->virt > y.rep->virt)
    return 0;

  int xl = x.rep->len;
  int yl = y.rep->len; 

  const _BS_word* xs = x.rep->s;
  const _BS_word* ys = y.rep->s;
  const _BS_word* topx = &(xs[xl]);
  const _BS_word* topy = &(ys[yl]);

  while (xs < topx && ys < topy)
  {
    _BS_word a = *xs++;
    _BS_word b = *ys++;
    if ((a | b) != b)
      return 0;
  }
  if (xl < yl) {
    if (x.rep->virt) {
      while (ys<topy)
	if ((*ys++)!=ONES)
	  return 0;
    }
  }
  else if (xl > yl) {
    if (!y.rep->virt) {
      while (xs<topx)
	if ((*xs++)!=0) 
	  return 0;
    }
  }
  return 1;
}


int operator < (const BitSet& x, const BitSet& y)
{
  if (x.rep->virt > y.rep->virt)
    return 0;

  int xl = x.rep->len;
  int yl = y.rep->len;

  _BS_word* xs = x.rep->s;
  _BS_word* ys = y.rep->s;
  _BS_word* topx = &(xs[xl]);
  _BS_word* topy = &(ys[yl]);
  int one_diff = 0;
  while (xs < topx && ys < topy)
  {
    _BS_word a = *xs++;
    _BS_word b = *ys++;
    _BS_word c = a | b;
    if (c != b)
      return 0;
    else if (c != a)
      one_diff = 1;
  }
  if (xl < yl) {
    if (x.rep->virt) {
      if (!one_diff)
	return 0;
      while (ys<topy)
	if ((*ys++)!=ONES)
	  return 0;
      return 1;
    }	
    else {
      if (one_diff || y.rep->virt)
	return 1;
      while (ys<topy)
	if ((*ys++)!=0)
	  return 1;
      return 0;
    }
  }
  else if (xl > yl) {
    if (y.rep->virt) {
      if (one_diff || !x.rep->virt)
	return 1;
      while (xs<topx)
	if ((*xs++)!=ONES)
	  return 1;
      return 0;
    }
    else {
      if (!one_diff)
	return 0;
      while (xs<topx)
	if ((*xs++)!=0)
	  return 0;
      return 1;
    }
  }
  else
    return one_diff || x.rep->virt < y.rep->virt;
}

int lcompare(const BitSet& x, const BitSet& y)
{
  int xl = x.rep->len;
  int yl = y.rep->len; 

  const _BS_word* xs = x.rep->s;
  const _BS_word* ys = y.rep->s;
  const _BS_word* topx = &(xs[xl]);
  const _BS_word* topy = &(ys[yl]);

  while (xs < topx && ys < topy)
  {
    _BS_word a = *xs++;
    _BS_word b = *ys++;
    if (a!=b) {
#if 0
      // Faster but opinable
      return (a<b) ? -1 : 1;
#else
      _BS_word diff=(a^b); 		// one's where different
      _BS_word mask=diff&~(diff-1);	// first bit different
      return (a&mask)?1:-1;
#endif
    }
    if (a<b)
      return -1;
    if (a>b)
      return 1;
  }
  if (xl < yl) {
    if (x.rep->virt) {
      while (ys<topy)
	if ((*ys++)!=ONES)
	  return 1;
      return 0;
    }
    else {
      while (ys<topy)
	if ((*ys++)!=0)
	  return -1;
      return 0;
    }
  }
  else if (xl > yl) {
    if (y.rep->virt) {
      while (xs<topx)
	if ((*xs++)!=ONES)
	  return -1;
      return 0;
    }
    else {
      while (xs<topx)
	if ((*xs++)!=0) 
	  return 1;
      return 0;
    }
  }
  return 0;
}

int BitSet::empty() const
{
  if (rep->virt == 1)
    return 0;

  _BS_word* bots = rep->s;
  _BS_word* s = &(bots[rep->len - 1]);
  while (s >= bots) if (*s-- != 0) return 0;
  return 1;
}


int BitSet::count(int b) const
{
  if (b == rep->virt)
    return -1;
  int l = 0;
  _BS_word* s = rep->s;
  _BS_word* tops = &(s[rep->len]);
  if (b == 1)
  {
    while (s < tops)
    {
      _BS_word a = *s++;
      for (int i = 0; i < BITSETBITS && a != 0; ++i)
      {
        if (a & 1)
          ++l;
        a >>= 1;
      }
    }
  }
  else
  {
    _BS_word maxbit = MASK1 (BITSETBITS - 1);
    while (s < tops)
    {
      _BS_word a = *s++;
      for (int i = 0; i < BITSETBITS; ++i)
      {
        if ((a & maxbit) == 0)
          ++l;
        a <<= 1;
      }
    }
  }
  return l;
}

BitSetRep* BitSetcmpl(const BitSetRep* src, BitSetRep* r)
{
  r = BitSetcopy(r, src);
  r->virt = !src->virt;
  _BS_word* rs = r->s;
  _BS_word* topr = &(rs[r->len]);
  if (r->len == 0)
    *rs = ONES;
  else
  {
    while (rs < topr)
    {
      _BS_word cmp = ~(*rs);
      *rs++ = cmp;
    }
  }
  trim(r);
  return r;
}


BitSetRep* BitSetop(const BitSetRep* x, const BitSetRep* y, 
                    BitSetRep* r, char op)
{
  int xrsame = x == r;
  int yrsame = y == r;
  int xv = x->virt;
  int yv = y->virt;
  int xl = x->len;
  int yl = y->len;
  int rl = (xl >= yl)? xl : yl;

  r = BitSetresize(r, rl);
  _BS_word* rs = r->s;
  _BS_word* topr = &(rs[rl]);

  int av, bv;
  const _BS_word* as;
  const _BS_word* topa;
  const _BS_word* bs;
  const _BS_word* topb;
  
  if (xl <= yl)
  {
    as = (xrsame)? r->s : x->s;
    av = xv;
    topa = &(as[xl]);
    bs = (yrsame)? r->s : y->s;
    bv = yv;
    topb = &(bs[yl]);
  }
  else
  {
    as = (yrsame)? r->s : y->s;
    av = yv;
    topa = &(as[yl]);
    bs = (xrsame)? r->s : x->s;
    bv = xv;
    topb = &(bs[xl]);
    if (op == '-')              // reverse sense of difference
      op = 'D';
  }

  switch (op)
  {
  case '&':
    r->virt = av & bv;
    while (as < topa) *rs++ = *as++ & *bs++;
    if (av)
      while (rs < topr) *rs++ = *bs++;
    else
      while (rs < topr) *rs++ = 0;
    break;
  case '|':
    r->virt = av | bv;
    while (as < topa) *rs++ = *as++ | *bs++;
    if (av)
      while (rs < topr) *rs++ = ONES;
    else
      while (rs < topr) *rs++ = *bs++;
    break;
  case '^':
    r->virt = av ^ bv;
    while (as < topa) *rs++ = *as++ ^ *bs++;
    if (av)
      while (rs < topr) *rs++ = ~(*bs++);
    else
      while (rs < topr) *rs++ = *bs++;
    break;
  case '-':
    r->virt = av & ~(bv);
    while (as < topa) *rs++ = *as++ & ~(*bs++);
    if (av)
      while (rs < topr) *rs++ = ~(*bs++);
    else
      while (rs < topr) *rs++ = 0;
    break;
  case 'D':
    r->virt = ~(av) & (bv);
    while (as < topa) *rs++ = ~(*as++) & (*bs++);
    if (av)
      while (rs < topr) *rs++ = 0;
    else
      while (rs < topr) *rs++ = *bs++;
    break;
  }
  trim(r);
  return r;
}


void BitSet::set(int p)
{
  if (p < 0) error("Illegal bit index");

  int index = BitSet_index(p);
  int pos   = BitSet_pos(p);

  if (index >= rep->len)
  {
    if (rep->virt)
      return;
    else
      rep = BitSetresize(rep, index+1);
  }

  rep->s[index] |= MASK1 (pos);
}

void BitSet::clear()
{
  if (rep->len > 0) memset(rep->s, 0, rep->sz * sizeof(_BS_word));
  rep->len = rep->virt = 0;
}

void BitSet::clear(int p)
{
  if (p < 0) error("Illegal bit index");
  int index = BitSet_index(p);
  if (index >= rep->len)
  {
    if (rep->virt == 0)
      return;
    else
      rep = BitSetresize(rep, index+1);
  }
  rep->s[index] &= ~MASK1(BitSet_pos(p));
}

void BitSet::invert(int p)
{
  if (p < 0) error("Illegal bit index");
  int index = BitSet_index(p);
  if (index >= rep->len) rep = BitSetresize(rep, index+1);
  rep->s[index] ^= MASK1(BitSet_pos(p));
}

void BitSet::set(int from, int to)
{
  if (from < 0 || from > to) error("Illegal bit index");

  int index1 = BitSet_index(from);
  int pos1   = BitSet_pos(from);
  
  if (rep->virt && index1 >= rep->len)
    return;

  int index2 = BitSet_index(to);
  int pos2   = BitSet_pos(to);

  if (index2 >= rep->len)
    rep = BitSetresize(rep, index2+1);

  _BS_word* s = &(rep->s[index1]);
  _BS_word m1 = lmask(pos1);
  _BS_word m2 = rmask(pos2);
  if (index2 == index1)
    *s |= m1 & m2;
  else
  {
    *s++ |= m1;
    _BS_word* top = &(rep->s[index2]);
    *top |= m2;
    while (s < top)
      *s++ = ONES;
  }
}

void BitSet::clear(int from, int to)
{
  if (from < 0 || from > to) error("Illegal bit index");

  int index1 = BitSet_index(from);
  int pos1   = BitSet_pos(from);
  
  if (!rep->virt && index1 >= rep->len)
    return;

  int index2 = BitSet_index(to);
  int pos2   = BitSet_pos(to);

  if (index2 >= rep->len)
    rep = BitSetresize(rep, index2+1);

  _BS_word* s = &(rep->s[index1]);
  _BS_word m1 = lmask(pos1);
  _BS_word m2 = rmask(pos2);
  if (index2 == index1)
    *s &= ~(m1 & m2);
  else
  {
    *s++ &= ~m1;
    _BS_word* top = &(rep->s[index2]);
    *top &= ~m2;
    while (s < top)
      *s++ = 0;
  }
}

void BitSet::invert(int from, int to)
{
  if (from < 0 || from > to) error("Illegal bit index");

  int index1 = BitSet_index(from);
  int pos1   = BitSet_pos(from);
  int index2 = BitSet_index(to);
  int pos2   = BitSet_pos(to);

  if (index2 >= rep->len)
    rep = BitSetresize(rep, index2+1);

  _BS_word* s = &(rep->s[index1]);
  _BS_word m1 = lmask(pos1);
  _BS_word m2 = rmask(pos2);
  if (index2 == index1)
    *s ^= m1 & m2;
  else
  {
    *s++ ^= m1;
    _BS_word* top = &(rep->s[index2]);
    *top ^= m2;
    while (s < top)
    {
      _BS_word cmp = ~(*s);
      *s++ = cmp;
    }
  }
}


int BitSet::test(int from, int to) const
{
  if (from < 0 || from > to) return 0;

  int index1 = BitSet_index(from);
  int pos1   = BitSet_pos(from);
  
  if (index1 >= rep->len)
    return rep->virt;

  int index2 = BitSet_index(to);
  int pos2   = BitSet_pos(to);

  if (index2 >= rep->len)
  {
    if (rep->virt)
      return 1;
    else 
    {
      index2 = rep->len - 1;
      pos2 = BITSETBITS - 1;
    }
  }

  _BS_word* s = &(rep->s[index1]);
  _BS_word m1 = lmask(pos1);
  _BS_word m2 = rmask(pos2);

  if (index2 == index1)
    return (*s & m1 & m2) != 0;
  else
  {
    if (*s++ & m1)
      return 1;
    _BS_word* top = &(rep->s[index2]);
    if (*top & m2)
      return 1;
    while (s < top)
      if (*s++ != 0) 
        return 1;
    return 0;
  }
}

int BitSet::next(int p, int b) const
{
  ++p;
  int index = BitSet_index(p);
  int pos   = BitSet_pos(p);

  int l = rep->len;
  
  if (index >= l)
  {
    if (rep->virt == b)
      return p;
    else
      return -1;
  }
  int j = index;
  _BS_word* s = rep->s;
  _BS_word a = s[j] >> pos;
  int i = pos;

  if (b == 1)
  {
    for (; i < BITSETBITS && a != 0; ++i)
    {
      if (a & 1)
        return j * BITSETBITS + i;
      a >>= 1;
    }
    for (++j; j < l; ++j)
    {
      a = s[j];
      for (i = 0; i < BITSETBITS && a != 0; ++i)
      {
        if (a & 1)
          return j * BITSETBITS + i;
        a >>= 1;
      }
    }
    if (rep->virt)
      return j * BITSETBITS;
    else
      return -1;
  }
  else
  {
    for (; i < BITSETBITS; ++i)
    {
      if ((a & 1) == 0)
        return j * BITSETBITS + i;
      a >>= 1;
    }
    for (++j; j < l; ++j)
    {
      a = s[j];
      if (a != ONES)
      {
        for (i = 0; i < BITSETBITS; ++i)
        {
          if ((a & 1) == 0)
            return j * BITSETBITS + i;
          a >>= 1;
        }
      }
    }
    if (!rep->virt)
      return j * BITSETBITS;
    else
      return -1;
  }
}

int BitSet::prev(int p, int b) const
{
  if (--p < 0)
    return -1;

  int index = BitSet_index(p);
  int pos   = BitSet_pos(p);

  _BS_word* s = rep->s;
  int l = rep->len;

  if (index >= l)
  {
    if (rep->virt == b)
      return p;
    else
    {
      index = l - 1;
      pos = BITSETBITS - 1;
    }
  }

  int j = index;
  _BS_word a = s[j];

  int i = pos;
  _BS_word maxbit = MASK1(pos);

  if (b == 1)
  {
    for (; i >= 0 && a != 0; --i)
    {
      if (a & maxbit)
        return j * BITSETBITS + i;
      a <<= 1;
    }
    maxbit = MASK1(BITSETBITS - 1);
    for (--j; j >= 0; --j)
    {
      a = s[j];
      for (i = BITSETBITS - 1; i >= 0 && a != 0; --i)
      {
        if (a & maxbit)
          return j * BITSETBITS + i;
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
          return j * BITSETBITS + i;
        a <<= 1;
      }
    }
    maxbit = MASK1(BITSETBITS - 1);
    for (--j; j >= 0; --j)
    {
      a = s[j];
      if (a != ONES)
      {
        for (i = BITSETBITS - 1; i >= 0; --i)
        {
          if ((a & maxbit) == 0)
            return j * BITSETBITS + i;
          a <<= 1;
        }
      }
    }
    return -1;
  }
}

int BitSet::last(int b) const
{
  if (b == rep->virt)
    return -1;
  else
    return prev((rep->len) * BITSETBITS, b);
}


extern AllocRing _libgxx_fmtq;

const char* BitSettoa(const BitSet& x, char f, char t, char star)
{
  trim(x.rep);
  int wrksiz = (x.rep->len + 1) * BITSETBITS + 2;
  char* fmtbase = (char *) _libgxx_fmtq.alloc(wrksiz);
  ostrstream stream(fmtbase, wrksiz);
  
  x.printon(stream, f, t, star);
  stream << ends;
  return fmtbase;
}

BitSet shorttoBitSet(unsigned short i) 
{
  BitSet r;
  _BS_word w = i;
  r.rep = BitSetalloc(0, &w, 1, 0, 2);  trim(r.rep);
  return r;
}

BitSet longtoBitSet(unsigned long i)
{
  BitSet r;
#if 1
  _BS_word w = i;
  r.rep = BitSetalloc(0, &w, 1, 0, 2);
#else
  _BS_word u[2];
  u[0] = i & ((_BS_word)(~(0)));
  u[1] = sizeof(long) <= sizeof(_BS_word) ? 0 : i >> BITSETBITS;
  r.rep = BitSetalloc(0, &u[0], 2, 0, 3);
#endif
  trim(r.rep);
  return r;
}

#if defined(__GNUG__) && !defined(_G_NO_NRV)

BitSet atoBitSet(const char* s, char f, char t, char star) return r
{
  int sl = strlen(s);
  if (sl != 0)
  {
    r.rep = BitSetresize(r.rep, sl / BITSETBITS + 1);
    _BS_word* rs = r.rep->s;
    _BS_word a = 0;
    _BS_word m = 1;
    char lastch = 0;
    unsigned int i = 0;
    unsigned int l = 1;
    for(;;)
    {
      char ch = s[i];
      if (ch == t)
        a |= m;
      else if (ch == star)
      {
        if ((r.rep->virt = (lastch == t)))
          *rs = a | ~(m - 1);
        else
          *rs = a;
        break;
      }
      else if (ch != f)
      {
        *rs = a;
        break;
      }
      lastch = ch;
      if (++i == sl)
      {
        *rs = a;
        break;
      }
      else if (i % BITSETBITS == 0)
      {
        *rs++ = a;
        a = 0;
        m = 1;
        ++l;
      }
      else
        m <<= 1;
    }
    r.rep->len = l;
    trim(r.rep);
  }
  return;
}

#else

BitSet atoBitSet(const char* s, char f, char t, char star) 
{
  BitSet r;
  int sl = strlen(s);
  if (sl != 0)
  {
    r.rep = BitSetresize(r.rep, sl / BITSETBITS + 1);
    _BS_word* rs = r.rep->s;
    _BS_word a = 0;
    _BS_word m = 1;
    char lastch = 0;
    unsigned int i = 0;
    unsigned int l = 1;
    for(;;)
    {
      char ch = s[i];
      if (ch == t)
        a |= m;
      else if (ch == star)
      {
        if (r.rep->virt = lastch == t)
          *rs = a | ~(m - 1);
        else
          *rs = a;
        break;
      }
      else if (ch != f)
      {
        *rs = a;
        break;
      }
      lastch = ch;
      if (++i == sl)
      {
        *rs = a;
        break;
      }
      else if (i % BITSETBITS == 0)
      {
        *rs++ = a;
        a = 0;
        m = 1;
        ++l;
      }
      else
        m <<= 1;
    }
    r.rep->len = l;
    trim(r.rep);
  }
  return r;
}

#endif

ostream& operator << (ostream& s, const BitSet& x)
{
  if (s.opfx())
    x.printon(s);
  return s;
}

void BitSet::printon(ostream& os, char f, char t, char star) const
// FIXME:  Does not respect s.width()!
{
  trim(rep);
  register streambuf* sb = os.rdbuf();
  const _BS_word* s = rep->s;
  const _BS_word* top = &(s[rep->len - 1]);

  while (s < top)
  {
    _BS_word a = *s++;
    for (int j = 0; j < BITSETBITS; ++j)
    {
      sb->sputc((a & 1)? t : f);
      a >>= 1;
    }
  }

  if (!rep->virt)
  {
    _BS_word a = *s;
    if (rep->len != 0)
    {
      for (int j = 0; j < BITSETBITS && a != 0; ++j)
      {
        sb->sputc((a & 1)? t : f);
        a >>= 1;
      }
    }
    sb->sputc(f);
  }
  else
  {
    _BS_word a = *s;
    _BS_word mask = ONES;
    _BS_word himask = MASK1(BITSETBITS - 1) - 1;
    if (rep->len != 0)
    {
      for (int j = 0; j < BITSETBITS && a != mask; ++j)
      {
        sb->sputc((a & 1)? t : f);
        a = (a >> 1) & himask;
        mask = (mask >> 1) & himask;
      }
    }
    sb->sputc(t);
  }

  sb->sputc(star);
}

int BitSet::OK() const
{
  int v = rep != 0;             // have a rep
  v &= rep->len <= rep->sz;     // within bounds
  v &= rep->virt == 0 || rep->virt == 1; // valid virtual bit
  if (!v) error("invariant failure");
  return v;
}

