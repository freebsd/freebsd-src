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
Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
*/
#ifndef _Uniform_h
#ifdef __GNUG__
#pragma interface
#endif
#define _Uniform_h 1

#include <Random.h>

//
//	The interval [lo..hi]
// 

class Uniform: public Random {
    double pLow;
    double pHigh;
    double delta;
public:
    Uniform(double low, double high, RNG *gen);

    double low();
    double low(double x);
    double high();
    double high(double x);

    virtual double operator()();
};


inline Uniform::Uniform(double low, double high, RNG *gen) : Random(gen)
{
    pLow = (low < high) ? low : high;
    pHigh = (low < high) ? high : low;
    delta = pHigh - pLow;
}

inline double Uniform::low() { return pLow; }

inline double Uniform::low(double x) {
  double tmp = pLow;
  pLow = x;
  delta = pHigh - pLow;
  return tmp;
}

inline double Uniform::high() { return pHigh; }

inline double Uniform::high(double x) {
  double tmp = pHigh;
  pHigh = x;
  delta = pHigh - pLow;
  return tmp;
}

#endif
