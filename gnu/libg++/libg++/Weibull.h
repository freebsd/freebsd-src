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
#ifndef _Weibull_h
#ifdef __GNUG__
#pragma interface
#endif
#define _Weibull_h 

#include <Random.h>

class Weibull: public Random {
protected:
    double pAlpha;
    double pInvAlpha;
    double pBeta;

    void setState();
    
public:
    Weibull(double alpha, double beta, RNG *gen);

    double alpha();
    double alpha(double x);

    double beta();
    double beta(double x);

    virtual double operator()();
};


inline void Weibull::setState() {
  pInvAlpha = 1.0 / pAlpha;
}
    
inline Weibull::Weibull(double alpha, double beta,  RNG *gen) : Random(gen)
{
  pAlpha = alpha;
  pBeta = beta;
  setState();
}

inline double Weibull::alpha() { return pAlpha; }

inline double Weibull::alpha(double x) {
  double tmp = pAlpha;
  pAlpha = x;
  setState();
  return tmp;
}

inline double Weibull::beta() { return pBeta; };
inline double Weibull::beta(double x) {
  double tmp = pBeta;
  pBeta = x;
  return tmp;
};

#endif
