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
#ifndef _Geometric_h
#ifdef __GNUG__
#pragma interface
#endif
#define _Geometric_h 

#include <Random.h>

class Geometric: public Random {
protected:
    double pMean;
public:
    Geometric(double mean, RNG *gen);

    double mean();
    double mean(double x);

    virtual double operator()();

};


inline Geometric::Geometric(double mean, RNG *gen) : Random(gen)
{
  pMean = mean;
}


inline double Geometric::mean() { return pMean; }
inline double Geometric::mean(double x) {
  double tmp = pMean; pMean = x; return tmp;
}


#endif
