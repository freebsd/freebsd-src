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
#ifndef SampleStatistic_h
#ifdef __GNUG__
#pragma interface
#endif
#define SampleStatistic_h 1

#include <builtin.h>

class SampleStatistic {
protected:
    int	n;
    double x;
    double x2;
    double minValue, maxValue;

    public :

    SampleStatistic();
    virtual ~SampleStatistic();
    virtual void reset(); 

    virtual void operator+=(double);
    int  samples();
    double mean();
    double stdDev();
    double var();
    double min();
    double max();
    double confidence(int p_percentage);
    double confidence(double p_value);

    void            error(const char* msg);
};

// error handlers

extern  void default_SampleStatistic_error_handler(const char*);
extern  one_arg_error_handler_t SampleStatistic_error_handler;

extern  one_arg_error_handler_t 
        set_SampleStatistic_error_handler(one_arg_error_handler_t f);

inline SampleStatistic:: SampleStatistic(){ reset();}
inline int SampleStatistic::  samples() {return(n);}
inline double SampleStatistic:: min() {return(minValue);}
inline double SampleStatistic:: max() {return(maxValue);}
inline SampleStatistic::~SampleStatistic() {}

#endif
