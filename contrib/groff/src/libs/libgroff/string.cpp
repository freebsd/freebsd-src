// -*- C++ -*-
/* Copyright (C) 1989, 1990, 1991, 1992, 2001, 2002
   Free Software Foundation, Inc.
     Written by James Clark (jjc@jclark.com)

This file is part of groff.

groff is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

groff is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License along
with groff; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */

#include "lib.h"

#include "stringclass.h"

static char *salloc(int len, int *sizep);
static void sfree(char *ptr, int size);
static char *sfree_alloc(char *ptr, int size, int len, int *sizep);
static char *srealloc(char *ptr, int size, int oldlen, int newlen, int *sizep);

static char *salloc(int len, int *sizep)
{
  if (len == 0) {
    *sizep = 0;
    return 0;
  }
  else
    return new char[*sizep = len*2];
}

static void sfree(char *ptr, int)
{
  a_delete ptr;
}

static char *sfree_alloc(char *ptr, int oldsz, int len, int *sizep)
{
  if (oldsz >= len) {
    *sizep = oldsz;
    return ptr;
  }
  a_delete ptr;
  if (len == 0) {
    *sizep = 0;
    return 0;
  }
  else
    return new char[*sizep = len*2];
}

static char *srealloc(char *ptr, int oldsz, int oldlen, int newlen, int *sizep)
{
  if (oldsz >= newlen) {
    *sizep = oldsz;
    return ptr;
  }
  if (newlen == 0) {
    a_delete ptr;
    *sizep = 0;
    return 0;
  }
  else {
    char *p = new char[*sizep = newlen*2];
    if (oldlen < newlen && oldlen != 0)
      memcpy(p, ptr, oldlen);
    a_delete ptr;
    return p;
  }
}

string::string() : ptr(0), len(0), sz(0)
{
}

string::string(const char *p, int n) : len(n)
{
  assert(n >= 0);
  ptr = salloc(n, &sz);
  if (n != 0)
    memcpy(ptr, p, n);
}

string::string(const char *p)
{
  if (p == 0) {
    len = 0;
    ptr = 0;
    sz = 0;
  }
  else {
    len = strlen(p);
    ptr = salloc(len, &sz);
    memcpy(ptr, p, len);
  }
}

string::string(char c) : len(1)
{
  ptr = salloc(1, &sz);
  *ptr = c;
}

string::string(const string &s) : len(s.len)
{
  ptr = salloc(len, &sz);
  if (len != 0)
    memcpy(ptr, s.ptr, len);
}
  
string::~string()
{
  sfree(ptr, sz);
}

string &string::operator=(const string &s)
{
  ptr = sfree_alloc(ptr, sz, s.len, &sz);
  len = s.len;
  if (len != 0)
    memcpy(ptr, s.ptr, len);
  return *this;
}

string &string::operator=(const char *p)
{
  if (p == 0) {
    sfree(ptr, len);
    len = 0;
    ptr = 0;
    sz = 0;
  }
  else {
    int slen = strlen(p);
    ptr = sfree_alloc(ptr, sz, slen, &sz);
    len = slen;
    memcpy(ptr, p, len);
  }
  return *this;
}

string &string::operator=(char c)
{
  ptr = sfree_alloc(ptr, sz, 1, &sz);
  len = 1;
  *ptr = c;
  return *this;
}

void string::move(string &s)
{
  sfree(ptr, sz);
  ptr = s.ptr;
  len = s.len;
  sz = s.sz;
  s.ptr = 0;
  s.len = 0;
  s.sz = 0;
}

void string::grow1()
{
  ptr = srealloc(ptr, sz, len, len + 1, &sz);
}

string &string::operator+=(const char *p)
{
  if (p != 0) {
    int n = strlen(p);
    int newlen = len + n;
    if (newlen > sz)
      ptr = srealloc(ptr, sz, len, newlen, &sz);
    memcpy(ptr + len, p, n);
    len = newlen;
  }
  return *this;
}

string &string::operator+=(const string &s)
{
  if (s.len != 0) {
    int newlen = len + s.len;
    if (newlen > sz)
      ptr = srealloc(ptr, sz, len, newlen, &sz);
    memcpy(ptr + len, s.ptr, s.len);
    len = newlen;
  }
  return *this;
}

void string::append(const char *p, int n)
{
  if (n > 0) {
    int newlen = len + n;
    if (newlen > sz)
      ptr = srealloc(ptr, sz, len, newlen, &sz);
    memcpy(ptr + len, p, n);
    len = newlen;
  }
}

string::string(const char *s1, int n1, const char *s2, int n2)
{
  assert(n1 >= 0 && n2 >= 0);
  len = n1 + n2;
  if (len == 0) {
    sz = 0;
    ptr = 0;
  }
  else {
    ptr = salloc(len, &sz);
    if (n1 == 0)
      memcpy(ptr, s2, n2);
    else {
      memcpy(ptr, s1, n1);
      if (n2 != 0)
	memcpy(ptr + n1, s2, n2);
    }
  }
}

int operator<=(const string &s1, const string &s2)
{
  return (s1.len <= s2.len
	  ? s1.len == 0 || memcmp(s1.ptr, s2.ptr, s1.len) <= 0
	  : s2.len != 0 && memcmp(s1.ptr, s2.ptr, s2.len) < 0);
}

int operator<(const string &s1, const string &s2)
{
  return (s1.len < s2.len
	  ? s1.len == 0 || memcmp(s1.ptr, s2.ptr, s1.len) <= 0
	  : s2.len != 0 && memcmp(s1.ptr, s2.ptr, s2.len) < 0);
}

int operator>=(const string &s1, const string &s2)
{
  return (s1.len >= s2.len
	  ? s2.len == 0 || memcmp(s1.ptr, s2.ptr, s2.len) >= 0
	  : s1.len != 0 && memcmp(s1.ptr, s2.ptr, s1.len) > 0);
}

int operator>(const string &s1, const string &s2)
{
  return (s1.len > s2.len
	  ? s2.len == 0 || memcmp(s1.ptr, s2.ptr, s2.len) >= 0
	  : s1.len != 0 && memcmp(s1.ptr, s2.ptr, s1.len) > 0);
}

void string::set_length(int i)
{
  assert(i >= 0);
  if (i > sz)
    ptr = srealloc(ptr, sz, len, i, &sz);
  len = i;
}

void string::clear()
{
  len = 0;
}

int string::search(char c) const
{
  char *p = ptr ? (char *)memchr(ptr, c, len) : NULL;
  return p ? p - ptr : -1;
}

// we silently strip nuls

char *string::extract() const
{
  char *p = ptr;
  int n = len;
  int nnuls = 0;
  int i;
  for (i = 0; i < n; i++)
    if (p[i] == '\0')
      nnuls++;
  char *q = new char[n + 1 - nnuls];
  char *r = q;
  for (i = 0; i < n; i++)
    if (p[i] != '\0')
      *r++ = p[i];
  *r = '\0';
  return q;
}

void string::remove_spaces()
{
  int l = len - 1;
  while (l >= 0 && ptr[l] == ' ')
    l--;
  char *p = ptr;
  if (l > 0)
    while (*p == ' ') {
      p++;
      l--;
    }
  if (len - 1 != l) {
    if (l >= 0) {
      len = l + 1;
      char *tmp = new char[len];
      memcpy(tmp, p, len);
      a_delete ptr;
      ptr = tmp;
    }
    else {
      len = 0;
      if (ptr) {
	a_delete ptr;
	ptr = 0;
      }
    }
  }
}

void put_string(const string &s, FILE *fp)
{
  int len = s.length();
  const char *ptr = s.contents();
  for (int i = 0; i < len; i++)
    putc(ptr[i], fp);
}

string as_string(int i)
{
  static char buf[INT_DIGITS + 2];
  sprintf(buf, "%d", i);
  return string(buf);
}

