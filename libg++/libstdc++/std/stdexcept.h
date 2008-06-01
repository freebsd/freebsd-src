// Methods for Exception Support for -*- C++ -*-
// Copyright (C) 1994, 1995 Free Software Foundation

// This file is part of the GNU ANSI C++ Library.  This library is free
// software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the
// Free Software Foundation; either version 2, or (at your option)
// any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this library; see the file COPYING.  If not, write to the Free
// Software Foundation, 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

// As a special exception, if you link this library with files
// compiled with a GNU compiler to produce an executable, this does not cause
// the resulting executable to be covered by the GNU General Public License.
// This exception does not however invalidate any other reasons why
// the executable file might be covered by the GNU General Public License.

// Written by Mike Stump based upon the specification in the 20 September 1994
// C++ working paper, ANSI document X3J16/94-0158.

#ifndef __STDEXCEPT__
#define __STDEXCEPT__

#ifdef __GNUG__
#pragma interface "std/stdexcept.h"
#endif

#include <typeinfo>

extern "C++" {
#if 0
#include <string>
typedef string __string;
#else
typedef const char * __string;
#endif

class exception {
public:
  typedef void (*raise_handler)(exception&);
  static raise_handler set_raise_handler(raise_handler handler_arg);
  exception (const __string& what_arg): desc (what_arg) { }
  virtual ~exception() { }
  void raise();
  virtual __string what() const { return desc; }
protected:
  exception() { }
  virtual void do_raise() { }
private:
  __string desc;
};

class logic_error : public exception {
public:
  logic_error(const __string& what_arg): exception (what_arg) { }
  virtual ~logic_error() { }
};

class domain_error : public logic_error {
public:
  domain_error (const __string& what_arg): logic_error (what_arg) { }
  virtual ~domain_error () { }
};

class invalid_argument : public logic_error {
public:
  invalid_argument (const __string& what_arg): logic_error (what_arg) { }
  virtual ~invalid_argument () { }
};

class length_error : public logic_error {
public:
  length_error (const __string& what_arg): logic_error (what_arg) { }
  virtual ~length_error () { }
};

class out_of_range : public logic_error {
public:
  out_of_range (const __string& what_arg): logic_error (what_arg) { }
  virtual ~out_of_range () { }
};

class runtime_error : public exception {
public:
  runtime_error(const __string& what_arg): exception (what_arg) { }
  virtual ~runtime_error() { }
protected:
  runtime_error(): exception () { }
};

class range_error : public runtime_error {
public:
  range_error (const __string& what_arg): runtime_error (what_arg) { }
  virtual ~range_error () { }
};

class overflow_error : public runtime_error {
public:
  overflow_error (const __string& what_arg): runtime_error (what_arg) { }
  virtual ~overflow_error () { }
};

// These are moved here from typeinfo so that we can compile with -frtti
class bad_cast : public logic_error {
public:
  bad_cast(const __string& what_arg): logic_error (what_arg) { }
  virtual ~bad_cast() { }
};

extern bad_cast __bad_cast_object;

class bad_typeid : public logic_error {
 public:
  bad_typeid (): logic_error ("bad_typeid") { }
  virtual ~bad_typeid () { }
};
} // extern "C++"

#endif
