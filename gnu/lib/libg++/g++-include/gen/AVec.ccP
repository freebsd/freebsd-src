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
Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifdef __GNUG__
#pragma implementation
#endif
#include <stream.h>
#include <builtin.h>
#include "<T>.AVec.h"

/*
 The following brought to you by the department of redundancy department
*/

<T>AVec& <T>AVec::operator = (<T>AVec& v)
{
  if (len != 0 && len != v.capacity())
    error("nonconformant vectors.");
  if (len == 0)
    s = new <T> [len = v.capacity()];
  if (s != v.vec())
  {
    for (int i = 0; i < len; ++i)
      s[i] = v.vec()[i];
  }
  return *this;
}

<T>AVec& <T>AVec::operator = (<T&> f)
{
  for (int i = 0; i < len; ++i) s[i] = f;
  return *this;
}


<T>AVec concat(<T>AVec & a, <T>AVec & b)
{
  int newl = a.capacity() + b.capacity();
  <T>* news = new <T> [newl];
  <T>* p = news;
  <T>* top = &(a.vec()[a.capacity()]);
  <T>* t = a.vec();
  while (t < top) *p++ = *t++;
  top = &(b.vec()[b.capacity()]);
  t = b.vec();
  while (t < top) *p++ = *t++;
  return <T>AVec(newl, news);
}


<T>AVec combine(<T>Combiner f, <T>AVec& a, <T>AVec& b)
{
  int newl = (a.capacity() < b.capacity())? a.capacity() : b.capacity();
  <T>* news = new <T> [newl];
  <T>* p = news;
  <T>* top = &(a.vec()[newl]);
  <T>* t = a.vec();
  <T>* u = b.vec();
  while (t < top) *p++ = (*f)(*t++, *u++);
  return <T>AVec(newl, news);
}

<T>AVec reverse(<T>AVec& a)
{
  <T>* news = new <T> [a.capacity()];
  if (a.capacity() != 0)
  {
    <T>* lo = news;
    <T>* hi = &(news[a.capacity() - 1]);
    while (lo < hi)
    {
      <T> tmp = *lo;
      *lo++ = *hi;
      *hi-- = tmp;
    }
  }
  return <T>AVec(a.capacity(), news);
}

<T>AVec map(<T>Mapper f, <T>AVec& a)
{
  <T>* news = new <T> [a.capacity()];
  <T>* p = news;
  <T>* top = &(a.vec()[a.capacity()]);
  <T>* t = a.vec();
  while(t < top) *p++ = (*f)(*t++);
  return <T>AVec(a.capacity(), news);
}

<T>AVec <T>AVec::at(int from, int n)
{
  int to;
  if (n < 0)
  {
    n = len - from;
    to = len - 1;
  }
  else
    to = from + n - 1;
  if ((unsigned)from > (unsigned)to)
    range_error();
  <T>* news = new <T> [n];
  <T>* p = news;
  <T>* t = &(s[from]);
  <T>* top = &(s[to]);
  while (t <= top) *p++ = *t++;
  return <T>AVec(n, news);
}

<T>AVec merge(<T>AVec & a, <T>AVec & b, <T>Comparator f)
{
  int newl = a.capacity() + b.capacity();
  <T>* news = new <T> [newl];
  <T>* p = news;
  <T>* topa = &(a.vec()[a.capacity()]);
  <T>* as = a.vec();
  <T>* topb = &(b.vec()[b.capacity()]);
  <T>* bs = b.vec();

  for (;;)
  {
    if (as >= topa)
    {
      while (bs < topb) *p++ = *bs++;
      break;
    }
    else if (bs >= topb)
    {
      while (as < topa) *p++ = *as++;
      break;
    }
    else if ((*f)(*as, *bs) <= 0)
      *p++ = *as++;
    else
      *p++ = *bs++;
  }
  return <T>AVec(newl, news);
}

<T>AVec operator + (<T>AVec& a, <T>AVec& b)
{
  a.check_len(b.capacity());
  <T>* news = new <T> [a.capacity()];
  <T>* p = news;
  <T>* top = &(a.vec()[a.capacity()]);
  <T>* t = a.vec();
  <T>* u = b.vec();
  while (t < top) *p++ = *t++ + *u++;
  return <T>AVec(a.capacity(), news);
}

<T>AVec operator - (<T>AVec& a, <T>AVec& b)
{
  a.check_len(b.capacity());
  <T>* news = new <T> [a.capacity()];
  <T>* p = news;
  <T>* top = &(a.vec()[a.capacity()]);
  <T>* t = a.vec();
  <T>* u = b.vec();
  while (t < top) *p++ = *t++ - *u++;
  return <T>AVec(a.capacity(), news);
}

<T>AVec  product (<T>AVec& a, <T>AVec& b)
{
  a.check_len(b.capacity());
  <T>* news = new <T> [a.capacity()];
  <T>* p = news;
  <T>* top = &(a.vec()[a.capacity()]);
  <T>* t = a.vec();
  <T>* u = b.vec();
  while (t < top) *p++ = *t++ * *u++;
  return <T>AVec(a.capacity(), news);
}

<T>AVec quotient(<T>AVec& a, <T>AVec& b)
{
  a.check_len(b.capacity());
  <T>* news = new <T> [a.capacity()];
  <T>* p = news;
  <T>* top = &(a.vec()[a.capacity()]);
  <T>* t = a.vec();
  <T>* u = b.vec();
  while (t < top) *p++ = *t++ / *u++;
  return <T>AVec(a.capacity(), news);
}

<T>AVec operator + (<T>AVec& a, <T&> b)
{
  <T>* news = new <T> [a.capacity()];
  <T>* p = news;
  <T>* top = &(a.vec()[a.capacity()]);
  <T>* t = a.vec();
  while (t < top) *p++ = *t++ + b;
  return <T>AVec(a.capacity(), news);
}

<T>AVec operator - (<T>AVec& a, <T&> b)
{
  <T>* news = new <T> [a.capacity()];
  <T>* p = news;
  <T>* top = &(a.vec()[a.capacity()]);
  <T>* t = a.vec();
  while (t < top) *p++ = *t++ - b;
  return <T>AVec(a.capacity(), news);
}

<T>AVec operator * (<T>AVec& a, <T&> b)
{
  <T>* news = new <T> [a.capacity()];
  <T>* p = news;
  <T>* top = &(a.vec()[a.capacity()]);
  <T>* t = a.vec();
  while (t < top) *p++ = *t++ * b;
  return <T>AVec(a.capacity(), news);
}

<T>AVec operator / (<T>AVec& a, <T&> b)
{
  <T>* news = new <T> [a.capacity()];
  <T>* p = news;
  <T>* top = &(a.vec()[a.capacity()]);
  <T>* t = a.vec();
  while (t < top) *p++ = *t++ / b;
  return <T>AVec(a.capacity(), news);
}

<T>AVec <T>AVec::operator - ()
{
  <T>* news = new <T> [len];
  <T>* p = news;
  <T>* top = &(s[len]);
  <T>* t = s;
  while (t < top) *p++ = -(*t++);
  return <T>AVec(len, news);
}

<T>AVec& <T>AVec::operator += (<T>AVec& b)
{
  check_len(b.capacity());
  <T>* u = b.vec();
  <T>* top = &(s[len]);
  <T>* t = s;
  while (t < top) *t++ += *u++;
  return *this;
}

<T>AVec& <T>AVec::operator -= (<T>AVec& b)
{
  check_len(b.capacity());
  <T>* u = b.vec();
  <T>* top = &(s[len]);
  <T>* t = s;
  while (t < top) *t++ -= *u++;
  return *this;
}

<T>AVec& <T>AVec::product(<T>AVec& b)
{
  check_len(b.capacity());
  <T>* u = b.vec();
  <T>* top = &(s[len]);
  <T>* t = s;
  while (t < top) *t++ *= *u++;
  return *this;
}

<T>AVec& <T>AVec::quotient(<T>AVec& b)
{
  check_len(b.capacity());
  <T>* u = b.vec();
  <T>* top = &(s[len]);
  <T>* t = s;
  while (t < top) *t++ /= *u++;
  return *this;
}

<T>AVec& <T>AVec::operator += (<T&> b)
{
  <T>* top = &(s[len]);
  <T>* t = s;
  while (t < top) *t++ += b;
  return *this;
}

<T>AVec& <T>AVec::operator -= (<T&> b)
{
  <T>* top = &(s[len]);
  <T>* t = s;
  while (t < top) *t++ -= b;
  return *this;
}

<T>AVec& <T>AVec::operator *= (<T&> b)
{
  <T>* top = &(s[len]);
  <T>* t = s;
  while (t < top) *t++ *= b;
  return *this;
}

<T>AVec& <T>AVec::operator /= (<T&> b)
{
  <T>* top = &(s[len]);
  <T>* t = s;
  while (t < top) *t++ /= b;
  return *this;
}

<T> <T>AVec::max()
{
  if (len == 0)
    return 0;
  <T>* top = &(s[len]);
  <T>* t = s;
  <T> res = *t++;
  for (; t < top; ++t) if (*t > res) res = *t;
  return res;
}

int <T>AVec::max_index()
{
  if (len == 0)
    return -1;
  int ind = 0;
  for (int i = 1; i < len; ++i)
    if (s[i] > s[ind])
      ind = i;
  return ind;
}

<T> <T>AVec::min()
{
  if (len == 0)
    return 0;
  <T>* top = &(s[len]);
  <T>* t = s;
  <T> res = *t++;
  for (; t < top; ++t) if (*t < res) res = *t;
  return res;
}

int <T>AVec::min_index()
{
  if (len == 0)
    return -1;
  int ind = 0;
  for (int i = 1; i < len; ++i)
    if (s[i] < s[ind])
      ind = i;
  return ind;
}

<T> <T>AVec::sum()
{
  <T> res = 0;
  <T>* top = &(s[len]);
  <T>* t = s;
  while (t < top) res += *t++;
  return res;
}


<T> <T>AVec::sumsq()
{
  <T> res = 0;
  <T>* top = &(s[len]);
  <T>* t = s;
  for (; t < top; ++t) res += *t * *t;
  return res;
}

<T> operator * (<T>AVec& a, <T>AVec& b)
{
  a.check_len(b.capacity());
  <T>* top = &(a.vec()[a.capacity()]);
  <T>* t = a.vec();
  <T>* u = b.vec();
  <T> res = 0;
  while (t < top) res += *t++ * *u++;
  return res;
}
