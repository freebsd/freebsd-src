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
#ifdef __GNUG__
#pragma implementation
#endif
#include <stream.h>
#include <SmplStat.h>
#include <math.h>

#ifndef HUGE_VAL
#ifdef HUGE
#define HUGE_VAL HUGE
#else
#include <float.h>
#define HUGE_VAL DBL_MAX
#endif
#endif

// error handling

void default_SampleStatistic_error_handler(const char* msg)
{
  cerr << "Fatal SampleStatistic error. " << msg << "\n";
  exit(1);
}

one_arg_error_handler_t SampleStatistic_error_handler = default_SampleStatistic_error_handler;

one_arg_error_handler_t set_SampleStatistic_error_handler(one_arg_error_handler_t f)
{
  one_arg_error_handler_t old = SampleStatistic_error_handler;
  SampleStatistic_error_handler = f;
  return old;
}

void SampleStatistic::error(const char* msg)
{
  (*SampleStatistic_error_handler)(msg);
}

// t-distribution: given p-value and degrees of freedom, return t-value
// adapted from Peizer & Pratt JASA, vol63, p1416

double tval(double p, int df) 
{
  double t;
  int positive = p >= 0.5;
  p = (positive)? 1.0 - p : p;
  if (p <= 0.0 || df <= 0)
    t = HUGE_VAL;
  else if (p == 0.5)
    t = 0.0;
  else if (df == 1)
    t = 1.0 / tan((p + p) * 1.57079633);
  else if (df == 2)
    t = sqrt(1.0 / ((p + p) * (1.0 - p)) - 2.0);
  else
  {	
    double ddf = df;
    double a = sqrt(log(1.0 / (p * p)));
    double aa = a * a;
    a = a - ((2.515517 + (0.802853 * a) + (0.010328 * aa)) /
             (1.0 + (1.432788 * a) + (0.189269 * aa) +
              (0.001308 * aa * a)));
    t = ddf - 0.666666667 + 1.0 / (10.0 * ddf);
    t = sqrt(ddf * (exp(a * a * (ddf - 0.833333333) / (t * t)) - 1.0));
  }
  return (positive)? t : -t;
}

void
SampleStatistic::reset()
{
    n = 0; x = x2 = 0.0;
    maxValue = -HUGE_VAL;
    minValue = HUGE_VAL;
}

void
SampleStatistic::operator+=(double value)
{
    n += 1;
    x += value;
    x2 += (value * value);
    if ( minValue > value) minValue = value;
    if ( maxValue < value) maxValue = value;
}

double
SampleStatistic::mean()
{
    if ( n > 0) {
	return (x / n);
    }
    else {
	return ( 0.0 );
    }
}

double
SampleStatistic::var()
{
    if ( n > 1) {
	return(( x2 - ((x * x) /  n)) / ( n - 1));
    }
    else {
	return ( 0.0 );
    }
}

double
SampleStatistic::stdDev()
{
    if ( n <= 0 || this -> var() <= 0) {
	return(0);
    } else {
	return( (double) sqrt( var() ) );
    }
}

double
SampleStatistic::confidence(int interval)
{
  int df = n - 1;
  if (df <= 0) return HUGE_VAL;
  double t = tval(double(100 + interval) * 0.005, df);
  if (t == HUGE_VAL)
    return t;
  else
    return (t * stdDev()) / sqrt(double(n));
}

double
SampleStatistic::confidence(double p_value)
{
  int df = n - 1;
  if (df <= 0) return HUGE_VAL;
  double t = tval((1.0 + p_value) * 0.5, df);
  if (t == HUGE_VAL)
    return t;
  else
    return (t * stdDev()) / sqrt(double(n));
}


