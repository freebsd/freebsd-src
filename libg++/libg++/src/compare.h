// This may look like C code, but it is really -*- C++ -*-

/* 
Copyright (C) 1988 Free Software Foundation
    written by Doug Lea (dl@rocky.oswego.edu)

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

#ifndef _compare_h
#ifdef __GNUG__
#pragma interface
#endif
#define _compare_h 1

#include <builtin.h>

int compare(int a, int b);
int compare(short a, short b);
int compare(unsigned long a, unsigned long b);
int compare(unsigned int a, unsigned int b);
int compare(unsigned short a, unsigned short b);
int compare(unsigned char a, unsigned char b);
int compare(signed char a, signed char b);
int compare(float a, float b);
int compare(double a, double b);
int compare(const char* a, const char* b);


inline int compare(int a, int b)
{
  return a - b;
}

inline int compare(short a, short b)
{
  return a - b;
}


inline int compare(signed char a, signed char b)
{
  return a - b;
}

inline int compare(unsigned long a, unsigned long b)
{
  return (a < b)? -1 : (a > b)? 1 : 0;
}

inline int compare(unsigned int a, unsigned int b)
{
  return (a < b)? -1 : (a > b)? 1 : 0;
}

inline int compare(unsigned short a, unsigned short b)
{
  return (a < b)? -1 : (a > b)? 1 : 0;
}

inline int compare(unsigned char a, unsigned char b)
{
  return (a < b)? -1 : (a > b)? 1 : 0;
}

inline int compare(float a, float b)
{
  return (a < b)? -1 : (a > b)? 1 : 0;
}

inline int compare(double a, double b)
{
  return (a < b)? -1 : (a > b)? 1 : 0;
}

inline int compare(const char* a, const char* b)
{
  return strcmp(a,b);
}

#endif
