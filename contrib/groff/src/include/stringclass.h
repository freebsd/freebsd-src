// -*- C++ -*-
/* Copyright (C) 1989, 1990, 1991, 1992 Free Software Foundation, Inc.
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

#include <string.h>
#include <stdio.h>
#include <assert.h>

// Ensure that the first declaration of functions that are later
// declared as inline declares them as inline.

class string;

inline string operator+(const string &, const string &);
inline string operator+(const string &, const char *);
inline string operator+(const char *, const string &);
inline string operator+(const string &, char);
inline string operator+(char, const string &);
inline int operator==(const string &, const string &);
inline int operator!=(const string &, const string &);

class string {
public:
  string();
  string(const string &);
  string(const char *);
  string(const char *, int);
  string(char);

  ~string();
  
  string &operator=(const string &);
  string &operator=(const char *);
  string &operator=(char);

  string &operator+=(const string &);
  string &operator+=(const char *);
  string &operator+=(char);
  void append(const char *, int);
  
  int length() const;
  int empty() const;
  int operator*() const;

  string substring(int i, int n) const;

  char &operator[](int);
  char operator[](int) const;

  void set_length(int i);
  const char *contents() const;
  int search(char) const;
  char *extract() const;
  void clear();
  void move(string &);

  friend string operator+(const string &, const string &);
  friend string operator+(const string &, const char *);
  friend string operator+(const char *, const string &);
  friend string operator+(const string &, char);
  friend string operator+(char, const string &);
	 
  friend int operator==(const string &, const string &);
  friend int operator!=(const string &, const string &);
  friend int operator<=(const string &, const string &);
  friend int operator<(const string &, const string &);
  friend int operator>=(const string &, const string &);
  friend int operator>(const string &, const string &);

private:
  char *ptr;
  int len;
  int sz;

  string(const char *, int, const char *, int);	// for use by operator+
  void grow1();
};


inline char &string::operator[](int i)
{
  assert(i >= 0 && i < len);
  return ptr[i];
}

inline char string::operator[](int i) const
{
  assert(i >= 0 && i < len);
  return ptr[i];
}

inline int string::length() const
{
  return len;
}

inline int string::empty() const
{
  return len == 0;
}

inline int string::operator*() const
{
  return len;
}

inline const char *string::contents() const
{
  return  ptr;
}

inline string operator+(const string &s1, const string &s2)
{
  return string(s1.ptr, s1.len, s2.ptr, s2.len);
}

inline string operator+(const string &s1, const char *s2)
{
#ifdef __GNUG__
  if (s2 == 0)
    return s1;
  else
    return string(s1.ptr, s1.len, s2, strlen(s2));
#else
  return s2 == 0 ? s1 : string(s1.ptr, s1.len, s2, strlen(s2));
#endif
}

inline string operator+(const char *s1, const string &s2)
{
#ifdef __GNUG__
  if (s1 == 0)
    return s2;
  else
    return string(s1, strlen(s1), s2.ptr, s2.len);
#else
  return s1 == 0 ? s2 : string(s1, strlen(s1), s2.ptr, s2.len);
#endif
}

inline string operator+(const string &s, char c)
{
  return string(s.ptr, s.len, &c, 1);
}

inline string operator+(char c, const string &s)
{
  return string(&c, 1, s.ptr, s.len);
}

inline int operator==(const string &s1, const string &s2)
{
  return (s1.len == s2.len 
	  && (s1.len == 0 || memcmp(s1.ptr, s2.ptr, s1.len) == 0));
}

inline int operator!=(const string &s1, const string &s2)
{
  return (s1.len != s2.len 
	  || (s1.len != 0 && memcmp(s1.ptr, s2.ptr, s1.len) != 0));
}

inline string string::substring(int i, int n) const
{
  assert(i >= 0 && i + n <= len);
  return string(ptr + i, n);
}

inline string &string::operator+=(char c)
{
  if (len >= sz)
    grow1();
  ptr[len++] = c;
  return *this;
}

void put_string(const string &, FILE *);

string as_string(int);
