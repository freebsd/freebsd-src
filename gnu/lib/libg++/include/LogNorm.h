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
#ifndef _LogNormal_h
#ifdef __GNUG__
#pragma interface
#endif
#define _LogNormal_h 

#include <Normal.h>

class LogNormal: public Normal {
protected:
    double logMean;
    double logVariance;
    void setState();
public:
    LogNormal(double mean, double variance, RNG *gen);
    double mean();
    double mean(double x);
    double variance();
    double variance(double x);
    virtual double operator()();
};


inline void LogNormal::setState()
{
    double m2 = logMean * logMean;
    pMean = log(m2 / sqrt(logVariance + m2) );
// from ch@heike.informatik.uni-dortmund.de:
// (was   pVariance = log((sqrt(logVariance + m2)/m2 )); )
    pStdDev = sqrt(log((logVariance + m2)/m2 )); 
}

inline LogNormal::LogNormal(double mean, double variance, RNG *gen)
    : Normal(mean, variance, gen)
{
    logMean = mean;
    logVariance = variance;
    setState();
}

inline double LogNormal::mean() {
    return logMean;
}

inline double LogNormal::mean(double x)
{
    double t=logMean; logMean = x; setState();
    return t;
}

inline double LogNormal::variance() {
    return logVariance;
}

inline double LogNormal::variance(double x)
{
    double t=logVariance; logVariance = x; setState();
    return t;
}

#endif
