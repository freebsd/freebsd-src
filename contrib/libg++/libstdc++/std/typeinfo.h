// RTTI support for -*- C++ -*-
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

// Written by Kung Hsu based upon the specification in the 20 September 1994
// C++ working paper, ANSI document X3J16/94-0158.

#ifndef __TYPEINFO__
#define __TYPEINFO__

#ifdef __GNUG__
#pragma interface "std/typeinfo.h"
#endif

extern "C" void* __throw_type_match_rtti (void *, void *, void *);

extern "C++" {
class type_info {
private:
  // assigning type_info is not supported.  made private.
  type_info& operator=(const type_info&);
  type_info(const type_info&);

public:
  enum node_type {
    _RTTI_BUILTIN_TYPE,               // builtin type
    _RTTI_USER_TYPE,                  // user defined type
    _RTTI_CLASS_TYPE,                 // class type
    _RTTI_POINTER_TYPE,               // pointer type
    _RTTI_ATTR_TYPE,                  // attribute type for const and volatile
    _RTTI_FUNC_TYPE,                  // function type
    _RTTI_PTMF_TYPE,                  // pointer to member function type
    _RTTI_PTMD_TYPE                   // pointer to member data type
    };

  // return node type of the object
  virtual node_type __rtti_get_node_type() const { return _RTTI_BUILTIN_TYPE; }

  // get_name will return the name of the type, NULL if no name (like builtin)
  virtual const char * __rtti_get_name() const { return 0; }

  // compare if type represented by the type_info are the same type
  virtual int __rtti_compare(const type_info&) const { return 0; }

  // argument passed is the desired type, 
  // for class type, if the type can be converted to the desired type, 
  // it will be, and returned, else 0 is returned.  If the match
  // succeeds, the return value will be adjusted to point to the sub-object.
  virtual void* __rtti_match(const type_info&, int, void *) const {
    // This should never be called.
    return 0;
  };

  // destructor
  virtual ~type_info() {}
  type_info() {}
    
  bool before(const type_info& arg);
  const char* name() const
    { return __rtti_get_name(); }
  bool operator==(const type_info& arg) const 
    { return __rtti_compare(arg) == 0; }
  bool operator!=(const type_info& arg) const 
    { return __rtti_compare(arg) != 0; }
};

// type_info for builtin type

class __builtin_type_info : public type_info {
public:
  enum builtin_type_val {
    _RTTI_BI_BOOL = 1, _RTTI_BI_CHAR, _RTTI_BI_SHORT, _RTTI_BI_INT, 
    _RTTI_BI_LONG, _RTTI_BI_LONGLONG, _RTTI_BI_FLOAT,
    _RTTI_BI_DOUBLE, _RTTI_BI_LDOUBLE, _RTTI_BI_UCHAR, 
    _RTTI_BI_USHORT, _RTTI_BI_UINT, _RTTI_BI_ULONG,
    _RTTI_BI_ULONGLONG, _RTTI_BI_SCHAR, _RTTI_BI_WCHAR, _RTTI_BI_VOID
  };

  builtin_type_val b_type;

  __builtin_type_info (builtin_type_val bt) : b_type (bt) {}
  node_type __rtti_get_node_type () const
    { return _RTTI_BUILTIN_TYPE; }
  const char *__rtti_get_name () const
    { return (const char *)0; }
  int __rtti_compare (const type_info& arg) const
    { return (arg.__rtti_get_node_type () == _RTTI_BUILTIN_TYPE && 
	      ((__builtin_type_info&)arg).b_type == b_type) ? 0 : -1; }
};

// serice function for comparing types by name.

inline int __fast_compare (const char *n1, const char *n2) {
  int c;
  if (n1 == n2) return 0;
  if (n1 == 0) return *n2;
  else if (n2 == 0) return *n1;

  c = (int)*n1++ - (int)*n2++;
  return c == 0 ? strcmp (n1, n2) : c;
};

// type_info for user type.

class __user_type_info : public type_info {
 private:
  const char *_name;

public:
  __user_type_info (const char *nm) : _name (nm) {}
  node_type __rtti_get_node_type () const
    { return _RTTI_USER_TYPE; }
  const char *__rtti_get_name () const
    { return _name; }
  int __rtti_compare (const type_info& arg) const
    { return (arg.__rtti_get_node_type () == __rtti_get_node_type() &&
	__fast_compare (_name, arg.__rtti_get_name ()) == 0) ? 0 : -1; }
};

// type_info for a class.

class __class_type_info : public __user_type_info {
private:
  enum access_mode {
    _RTTI_ACCESS_PUBLIC, _RTTI_ACCESS_PROTECTED, _RTTI_ACCESS_PRIVATE
    };
  type_info **base_list;
  int *offset_list;
  int *is_virtual_list;
  access_mode *access_list;
  int n_bases;

public:
  __class_type_info (const char *name, type_info **bl, int *off, 
		     int *is_vir, access_mode *acc, int bn)
    : __user_type_info (name), base_list (bl), offset_list(off), 
      is_virtual_list(is_vir), access_list(acc), n_bases (bn) {}
  node_type __rtti_get_node_type () const
    { return _RTTI_CLASS_TYPE; }

  // inherit __rtti_compare from __user_type_info

  // This is a little complex defined in typeinfo.cc
  void* __rtti_match(const type_info&, int, void *) const;
};

// type info for pointer type.

class __pointer_type_info : public type_info {
private:
  type_info& type;

public:
  __pointer_type_info (type_info& ti) : type (ti) {}
  node_type __rtti_get_node_type () const
    { return _RTTI_POINTER_TYPE; }
  const char *__rtti_get_name () const
    { return (const char *)0; }
  int __rtti_compare (const type_info& arg) const
    { return (arg.__rtti_get_node_type () == __rtti_get_node_type() &&
	type.__rtti_compare ( ((__pointer_type_info&)arg).type) == 0) ? 0 : -1; }
  void* __rtti_match(const type_info& catch_type, int, void *objptr) const;
};

// type info for attributes

class __attr_type_info : public type_info {
public:
  enum attr_val {
    _RTTI_ATTR_CONST = 1, _RTTI_ATTR_VOLATILE, _RTTI_ATTR_CONSTVOL
    };

  attr_val attr;
  type_info& type;

  __attr_type_info (attr_val a, type_info& t) : attr (a), type(t) {}
  node_type __rtti_get_node_type () const
    { return _RTTI_ATTR_TYPE; }
  const char *__rtti_get_name () const
    { return (const char *)0; }
  int __rtti_compare (const type_info& arg)  const
    { return (arg.__rtti_get_node_type () == _RTTI_ATTR_TYPE && 
	      attr == ((__attr_type_info&)arg).attr &&
	      type.__rtti_compare ( ((__attr_type_info&)arg).type ) == 0) 
	      ? 0 : -1; }
};

// type info for function.

class __func_type_info : public __user_type_info {
public:
  __func_type_info (const char *name) : __user_type_info (name) {}
  node_type __rtti_get_node_type () const
    { return _RTTI_FUNC_TYPE; }
};

// type info for pointer to member function.

class __ptmf_type_info : public __user_type_info {
public:
  __ptmf_type_info (const char *name) : __user_type_info (name) {}
  node_type __rtti_get_node_type () const
    { return _RTTI_PTMF_TYPE; }
};

// type info for pointer to data member.

class __ptmd_type_info : public type_info {
  type_info& classtype;
  type_info& type;
public:
  __ptmd_type_info (type_info& tc, type_info& t) : classtype (tc), type (t) {}
  node_type __rtti_get_node_type () const
    { return _RTTI_PTMD_TYPE; }
  int __rtti_compare (const type_info& arg)  const
    { return (arg.__rtti_get_node_type () == _RTTI_PTMD_TYPE && 
	classtype.__rtti_compare ( ((__ptmd_type_info&)arg).classtype ) == 0 &&
	type.__rtti_compare ( ((__ptmd_type_info&)arg).type ) == 0) 
	      ? 0 : -1; }
};
} // extern "C++"

#include <stdexcept>

#endif
