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
#ifndef _HyperGeometric_h
#ifdef __GNUG__
#pragma interface
#endif
#define _HyperGeometric_h 

#include <Random.h>

class HyperGeometric: public Random {
protected:
    double pMean;
    double pVariance;
    double pP;
    void setState();

public:
    HyperGeometric(double mean, double variance, RNG *gen);

    double mean();
    double mean(double x);
    double variance();
    double variance(double x);

    virtual double operator()();
};


inline void HyperGeometric::setState() {
  double z = pVariance / (pMean * pMean);
  pP = 0.5 * (1.0 - sqrt((z - 1.0) / ( z + 1.0 )));
}

inline HyperGeometric::HyperGeometric(double mean, double variance, RNG *gen)
: Random(gen) {
  pMean = mean; pVariance = variance;
  setState();
}

inline double HyperGeometric::mean() { return pMean; };

inline double HyperGeometric::mean(double x) {
  double t = pMean; pMean = x;
  setState(); return t;
}

inline double HyperGeometric::variance() { return pVariance; }

inline double HyperGeometric::variance(double x) {
  double t = pVariance; pVariance = x;
  setState(); return t;
}

#endif
