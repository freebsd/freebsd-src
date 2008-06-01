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
#ifndef _Binomial_h
#ifdef __GNUG__
#pragma interface
#endif
#define _Binomial_h 1

#include <Random.h>

class Binomial: public Random {
protected:
    int pN;
    double pU;
public:
    Binomial(int n, double u, RNG *gen);

    int n();
    int n(int xn);

    double u();
    double u(double xu);

    virtual double operator()();

};


inline Binomial::Binomial(int n, double u, RNG *gen)
: Random(gen){
  pN = n; pU = u;
}

inline int Binomial::n() { return pN; }
inline int Binomial::n(int xn) { int tmp = pN; pN = xn; return tmp; }

inline double Binomial::u() { return pU; }
inline double Binomial::u(double xu) { double tmp = pU; pU = xu; return tmp; }

#endif
