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

#ifndef SampleHistogram_h
#ifdef __GNUG__
#pragma interface
#endif
#define SampleHistogram_h 1

#include <iostream.h>
#include <SmplStat.h>

extern const int SampleHistogramMinimum;
extern const int SampleHistogramMaximum;

class SampleHistogram : public SampleStatistic 
{
protected:
    short howManyBuckets;
    int *bucketCount;
    double *bucketLimit;

public:
    
    SampleHistogram(double low, double hi, double bucketWidth = -1.0);

    ~SampleHistogram();

    virtual void reset();
    virtual void operator+=(double);

    int similarSamples(double);

    int buckets();

    double bucketThreshold(int i);
    int inBucket(int i);
    void printBuckets(ostream&);

};


inline int SampleHistogram:: buckets() { return(howManyBuckets); };

inline double SampleHistogram:: bucketThreshold(int i) {
    if (i < 0 || i >= howManyBuckets)
        error("invalid bucket access");
    return(bucketLimit[i]);
}

inline int SampleHistogram:: inBucket(int i) {
    if (i < 0 || i >= howManyBuckets)
        error("invalid bucket access");
    return(bucketCount[i]);
}

#endif
