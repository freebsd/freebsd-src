// This may look like C code, but it is really -*- C++ -*-
/* 
Copyright (C) 1988 Free Software Foundation
    written by Dirk Grunwald (grunwald@cs.uiuc.edu)

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
#ifndef _DiscreteUniform_h
#ifdef __GNUG__
#pragma interface
#endif
#define _DiscreteUniform_h 1

#include <Random.h>

//
//	The interval [lo..hi)
// 

class DiscreteUniform: public Random {
    long pLow;
    long pHigh;
    double delta;
public:
    DiscreteUniform(long low, long high, RNG *gen);

    long low();
    long low(long x);
    long high();
    long high(long x);

    virtual double operator()();
};


inline DiscreteUniform::DiscreteUniform(long low, long high, RNG *gen)
: Random(gen)
{
    pLow = (low < high) ? low : high;
    pHigh = (low < high) ? high : low;
    delta = (pHigh - pLow) + 1;
}

inline long DiscreteUniform::low() { return pLow; }

inline long DiscreteUniform::low(long x) {
  long tmp = pLow;
  pLow = x;
  delta = (pHigh - pLow) + 1;
  return tmp;
}

inline long DiscreteUniform::high() { return pHigh; }

inline long DiscreteUniform::high(long x) {
  long tmp = pHigh;
  pHigh = x;
  delta = (pHigh - pLow) + 1;
  return tmp;
}

#endif
