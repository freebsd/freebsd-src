/*
Copyright (C) 1992 Free Software Foundation
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
Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef _minmax_h
#ifdef _GNUG_
#pragma interface
#endif
#define _minmax_h 1

#include <_G_config.h>

inline char min(char a, char b) { return (a < b)?a:b;}
#ifndef  _G_BROKEN_SIGNED_CHAR
inline signed char min(signed char a, signed char b) { return (a < b)?a:b;}
#endif
inline unsigned char min(unsigned char a, unsigned char b) {return (a<b)?a:b;}

inline short min(short a, short b) {return (a < b) ?a:b;}
inline unsigned short min(unsigned short a, unsigned short b) {return (a < b)?a:b;}

inline int min(int a, int b) {return (a < b)?a:b;}
inline unsigned int min(unsigned int a, unsigned int b) {return (a < b)?a:b;}

inline long min(long a, long b) {return (a < b)?a:b;}
inline unsigned long min(unsigned long a, unsigned long b) {return (a<b)?a:b;}

inline float min(float a, float b) {return (a < b)?a:b;}

inline double min(double a, double b) {return (a < b)?a:b;}

inline char max(char a, char b) { return (a > b)?a:b;}
#ifndef  _G_BROKEN_SIGNED_CHAR
inline signed char max(signed char a, signed char b) {return (a > b)?a:b;}
#endif
inline unsigned char max(unsigned char a, unsigned char b) {return (a>b)?a:b;}

inline short max(short a, short b) {return (a > b) ?a:b;}
inline unsigned short max(unsigned short a, unsigned short b) {return (a > b)?a:b;}

inline int max(int a, int b) {return (a > b)?a:b;}
inline unsigned int max(unsigned int a, unsigned int b) {return (a > b)?a:b;}

inline long max(long a, long b) {return (a > b)?a:b;}
inline unsigned long max(unsigned long a, unsigned long b) {return (a>b)?a:b;}

inline float max(float a, float b) {return (a > b)?a:b;}

inline double max(double a, double b) {return (a > b)?a:b;}

#endif

