// This may look like C code, but it is really -*- C++ -*-
/* 
Copyright (C) 1990 Free Software Foundation
    adapted from a submission from John Reidl <riedl@cs.purdue.edu>


GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY.  No author or distributor
accepts responsibility to anyone for the consequences of using it
or for whether it serves any particular purpose or works at all,
unless he says so in writing.  Refer to the GNU CC General Public
License for full details.

Everyone is granted permission to copy, modify and redistribute
GNU CC, but only under the conditions described in the
GNU CC General Public License.   A copy of this license is
supposed to have been given to you along with GNU CC so you
can know your rights and responsibilities.  It should be in a
file named COPYING.  Among other things, the copyright notice
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

#ifndef _RandomInteger_h
#ifdef __GNUG__
#pragma interface
#endif
#define _RandomInteger_h 1

// RandomInteger uses a random number generator to generate an integer
// in a specified range.  By default the range is 0..1.  Since in my
// experience random numbers are often needed for a wide variety of
// ranges in the same program, this generator accepts a new low or high value
// as an argument to the asLong and operator() methods to temporarily 
// override stored values

#include <math.h>
#include <RNG.h>

class RandomInteger 
{
 protected:
  RNG *pGenerator;
  long pLow;
  long pHigh;

  long _asLong(long, long);

 public:

       RandomInteger(long low, long high, RNG *gen);
       RandomInteger(long high, RNG *gen);
       RandomInteger(RNG *gen);

// read params

  long low() const;
  long high() const;
  RNG* generator() const;

// change params

  long low(long x);
  long high(long x);
  RNG* generator(RNG *gen);

// get a random number

  long asLong();
  long operator()(); // synonym for asLong
  int  asInt();      // (possibly) truncate as int

// override params for one shot

  long asLong(long high);
  long asLong(long low, long high);

  long operator () (long high);  // synonyms
  long operator () (long low, long high);

};


inline RandomInteger::RandomInteger(long low, long high, RNG *gen) 
     : pGenerator(gen),
       pLow((low < high) ? low : high),
       pHigh((low < high) ? high : low)
       
{}

inline RandomInteger::RandomInteger(long high, RNG *gen) 
     : pGenerator(gen),
       pLow((0 < high) ? 0 : high),
       pHigh((0 < high) ? high : 0)
{}
  

inline RandomInteger::RandomInteger(RNG *gen) 
     : pGenerator(gen),
       pLow(0),
       pHigh(1)
{}

inline RNG* RandomInteger::generator() const { return pGenerator;}
inline long RandomInteger::low() const       { return pLow; }
inline long RandomInteger::high() const      { return pHigh; }

inline RNG* RandomInteger::generator(RNG *gen) 
{
  RNG *tmp = pGenerator; pGenerator = gen;  return tmp;
}

inline long RandomInteger::low(long x)  
{
  long tmp = pLow;  pLow = x;  return tmp;
}

inline long RandomInteger:: high(long x) 
{
  long tmp = pHigh; pHigh = x; return tmp;
}

inline long RandomInteger:: _asLong(long low, long high)
{	
  return (pGenerator->asLong() % (high-low+1)) + low;
}


inline long RandomInteger:: asLong() 
{
  return _asLong(pLow, pHigh);
}

inline long RandomInteger:: asLong(long high)
{
  return _asLong(pLow, high);
}

inline long RandomInteger:: asLong(long low, long high)
{
  return _asLong(low, high);
}

inline long RandomInteger:: operator () () 
{
  return _asLong(pLow, pHigh);
}

inline long RandomInteger:: operator () (long high)
{
  return _asLong(pLow, high);
}

inline long RandomInteger:: operator () (long low, long high)
{
  return _asLong(low, high);
}




inline int RandomInteger:: asInt() 
{
  return int(asLong());
}

#endif
