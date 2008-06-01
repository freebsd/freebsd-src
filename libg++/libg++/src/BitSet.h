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
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#ifndef _BitSet_h
#ifdef __GNUG__
#pragma interface
#endif

#define _BitSet_h 1

#include <iostream.h>
#include <limits.h>
#include <bitprims.h>

#undef OK

#define BITSETBITS  (sizeof(_BS_word) * CHAR_BIT)

struct BitSetRep
{
  unsigned short len;       // number of _BS_word in s
  unsigned short sz;        // allocated slots
  unsigned short virt;      // virtual 0 or 1
  _BS_word  s[1];         // bits start here
};

extern BitSetRep*   BitSetalloc(BitSetRep*, const _BS_word*, 
                                int, int, int);
extern BitSetRep*   BitSetcopy(BitSetRep*, const BitSetRep*);
extern BitSetRep*   BitSetresize(BitSetRep*, int);
extern BitSetRep*   BitSetop(const BitSetRep*, const BitSetRep*, 
                             BitSetRep*, char);
extern BitSetRep*   BitSetcmpl(const BitSetRep*, BitSetRep*);
extern BitSetRep    _nilBitSetRep;

class BitSet;

class BitSetBit
{
protected:
  BitSet*            src;
  unsigned long      pos;

 public:
                     BitSetBit(BitSet* v, int p);
                     BitSetBit(const BitSetBit& b);
                    ~BitSetBit();
                     operator int() const;
  int                operator = (int b);
  int                operator = (const BitSetBit& b);
};

class BitSet
{
protected:
  BitSetRep*          rep;

  enum BS_op {
    BS_and = (int) '&',
    BS_or = (int) '|',
    BS_xor = (int) '^',
    BS_diff = (int) '-',
    BS_inv = (int) '~'
  };
  BitSet(const BitSet& x, const BitSet& y, enum BS_op op)
    { rep = BitSetop (x.rep, y.rep, NULL, (char) op);  }
  BitSet(const BitSet& x, enum BS_op /* op */)
    { rep = BitSetcmpl (x.rep, NULL); }

public:

// constructors
                     BitSet();
                     BitSet(const BitSet&);

                    ~BitSet();

  BitSet&            operator =  (const BitSet& y);

// equality & subset tests

  friend int         operator == (const BitSet& x, const BitSet& y);
  friend int         operator != (const BitSet& x, const BitSet& y);
  friend int         operator <  (const BitSet& x, const BitSet& y);
  friend int         operator <= (const BitSet& x, const BitSet& y);
  friend int         operator >  (const BitSet& x, const BitSet& y);
  friend int         operator >= (const BitSet& x, const BitSet& y);
  friend int	       lcompare(const BitSet& x, const BitSet& y);

// operations on self

  BitSet&            operator |= (const BitSet& y);
  BitSet&            operator &= (const BitSet& y);
  BitSet&            operator -= (const BitSet& y);
  BitSet&            operator ^= (const BitSet& y);

  void               complement();

// functional operators

  friend BitSet operator & (const BitSet& x, const BitSet& y);
  friend BitSet operator | (const BitSet& x, const BitSet& y);
  friend BitSet operator ^ (const BitSet& x, const BitSet& y);
  friend BitSet operator - (const BitSet& x, const BitSet& y);
  friend BitSet operator ~ (const BitSet& x);

// individual bit manipulation

  void               set(int pos);
  void               set(int from, int to);
  void               set(); // set all

  void               clear(int pos);
  void               clear(int from, int to);
  void               clear(); // clear all

  void               invert(int pos);
  void               invert(int from, int to);

  int                test(int pos) const;
  int                test(int from, int to) const;

  BitSetBit          operator [] (int i);
  
// iterators

  int                first(int b = 1) const;
  int                last(int b = 1) const;

  int                next(int pos, int b = 1) const;
  int                prev(int pos, int b = 1) const;
  int                previous(int pos, int b = 1) const /* Obsolete synonym */
    { return prev(pos, b); }

// status

  int                empty() const;
  int                virtual_bit() const;
  int                count(int b = 1) const;
  
// convertors & IO

  friend BitSet      atoBitSet(const char* s, 
                               char f='0', char t='1', char star='*');
  // BitSettoa is deprecated; do not use in new programs.
  friend const char* BitSettoa(const BitSet& x, 
                               char f='0', char t='1', char star='*');

  friend BitSet      shorttoBitSet(unsigned short w);
  friend BitSet      longtoBitSet(unsigned long w);

  friend ostream&    operator << (ostream& s, const BitSet& x);
  void		     printon(ostream& s,
			     char f='0', char t='1', char star='*') const;

#ifndef __STRICT_ANSI__
  // procedural versions of operators

  // The first three of these are incompatible with ANSI C++ digraphs.
  // In any case, it's not a great interface.
  friend void        and(const BitSet& x, const BitSet& y, BitSet& r);
  friend void        or(const BitSet& x, const BitSet& y, BitSet& r);
  friend void        xor(const BitSet& x, const BitSet& y, BitSet& r);
  friend void        diff(const BitSet& x, const BitSet& y, BitSet& r);
  friend void        complement(const BitSet& x, BitSet& r);
#endif

// misc

  void      error(const char* msg) const;
  int                OK() const;
};


typedef BitSet BitSetTmp;

// These are inlined regardless of optimization

inline int BitSet_index(int l)
{
  return (unsigned)(l) / BITSETBITS;
}

inline int BitSet_pos(int l)
{
  return l & (BITSETBITS - 1);
}

inline BitSet::BitSet() : rep(&_nilBitSetRep) {}

inline BitSet::BitSet(const BitSet& x) :rep(BitSetcopy(0, x.rep)) {}

inline BitSet::~BitSet() { if (rep != &_nilBitSetRep) delete rep; }

inline BitSet& BitSet::operator =  (const BitSet& y)
{ 
  rep = BitSetcopy(rep, y.rep);
  return *this;
}

inline int operator != (const BitSet& x, const BitSet& y) { return !(x == y); }

inline int operator >  (const BitSet& x, const BitSet& y) { return y < x; }

inline int operator >= (const BitSet& x, const BitSet& y) { return y <= x; }

#ifndef __STRICT_ANSI__
inline void and(const BitSet& x, const BitSet& y, BitSet& r)
{
  r.rep =  BitSetop(x.rep, y.rep, r.rep, '&');
}

inline void or(const BitSet& x, const BitSet& y, BitSet& r)
{
  r.rep =  BitSetop(x.rep, y.rep, r.rep, '|');
}

inline void xor(const BitSet& x, const BitSet& y, BitSet& r)
{
  r.rep =  BitSetop(x.rep, y.rep, r.rep, '^');
}

inline void diff(const BitSet& x, const BitSet& y, BitSet& r)
{
  r.rep =  BitSetop(x.rep, y.rep, r.rep, '-');
}

inline void complement(const BitSet& x, BitSet& r)
{
  r.rep = BitSetcmpl(x.rep, r.rep);
}
#endif

inline BitSet operator & (const BitSet& x, const BitSet& y) 
{
  return BitSet::BitSet(x, y, BitSet::BS_and);
}

inline BitSet operator | (const BitSet& x, const BitSet& y) 
{
  return BitSet::BitSet(x, y, BitSet::BS_or);
}

inline BitSet operator ^ (const BitSet& x, const BitSet& y) 
{
  return BitSet::BitSet(x, y, BitSet::BS_xor);
}

inline BitSet operator - (const BitSet& x, const BitSet& y) 
{
  return BitSet::BitSet(x, y, BitSet::BS_diff);
}

inline BitSet operator ~ (const BitSet& x) 
{
  return BitSet::BitSet(x, BitSet::BS_inv);
}

inline BitSet& BitSet::operator &= (const BitSet& y)
{
  rep =  BitSetop(rep, y.rep, rep, '&');
  return *this;
}

inline BitSet& BitSet::operator |= (const BitSet& y)
{
  rep =  BitSetop(rep, y.rep, rep, '|');
  return *this;
}

inline BitSet& BitSet::operator ^= (const BitSet& y)
{
  rep =  BitSetop(rep, y.rep, rep, '^');
  return *this;
}

inline BitSet& BitSet::operator -= (const BitSet& y)
{
  rep =  BitSetop(rep, y.rep, rep, '-');
  return *this;
}


inline void BitSet::complement()
{
  rep = BitSetcmpl(rep, rep);
}

inline int BitSet::virtual_bit() const
{
  return rep->virt;
}

inline int BitSet::first(int b) const
{
  return next(-1, b);
}

inline int BitSet::test(int p) const
{
  if (p < 0) error("Illegal bit index");
  int index = BitSet_index(p);
  return (index >= rep->len)? rep->virt : 
         ((rep->s[index] & ((_BS_word)1 << BitSet_pos(p))) != 0);
}


inline void BitSet::set()
{
  rep = BitSetalloc(rep, 0, 0, 1, 0);
}

inline BitSetBit::BitSetBit(const BitSetBit& b) :src(b.src), pos(b.pos) {}

inline BitSetBit::BitSetBit(BitSet* v, int p)
{
  src = v;  pos = p;
}

inline BitSetBit::~BitSetBit() {}

inline BitSetBit::operator int() const
{
  return src->test(pos);
}

inline int BitSetBit::operator = (int b)
{
  if (b) src->set(pos); else src->clear(pos); return b;
}

inline int BitSetBit::operator = (const BitSetBit& b)
{
  int i = (int)b;
  *this = i;
  return i;
}

inline BitSetBit BitSet::operator [] (int i)
{
  if (i < 0) error("illegal bit index");
  return BitSetBit(this, i);
}

#endif
