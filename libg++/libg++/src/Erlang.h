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
#ifndef _Erlang_h
#ifdef __GNUG__
#pragma interface
#endif
#define _Erlang_h 1 

#include <Random.h>

class Erlang: public Random {
protected:
    double pMean;
    double pVariance;
    int k;
    double a;
    void setState();
public:
    Erlang(double mean, double variance, RNG *gen);

    double mean();
    double mean(double x);
    double variance();
    double variance(double x);

    virtual double operator()();

};


inline void Erlang::setState() {
  k = int( (pMean * pMean ) / pVariance + 0.5 );
  k = (k > 0) ? k : 1;
  a = k / pMean;
}

inline Erlang::Erlang(double mean, double variance, RNG *gen) : Random(gen)
{
  pMean = mean; pVariance = variance;
  setState();
}

inline double Erlang::mean() { return pMean; }
inline double Erlang::mean(double x) {
  double tmp = pMean; pMean = x; setState(); return tmp;
};

inline double Erlang::variance() { return pVariance; }
inline double Erlang::variance(double x) {
  double tmp = pVariance; pVariance = x; setState(); return tmp;
}

#endif
