// Methods for type_info for the -*- C++ -*- Run Time Type Identification.
// Copyright (C) 1994 Free Software Foundation

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

#ifdef __GNUG__
#pragma implementation "std/typeinfo.h"
#endif

#include <std/cstdlib.h>
#include <std/typeinfo.h>

// Offset functions for the class type.

// 0 is returned if the cast is invalid, otherwise the converted
// object pointer that points to the sub-object that is matched is
// returned.

void*
__class_type_info::__rtti_match (const type_info& desired, int is_public,
				 void *objptr) const
{
  if (__rtti_compare (desired) == 0)
    return objptr;

  void *match_found = 0;
  for (int i = 0; i < n_bases; i++) {
    if (is_public && access_list[i] != _RTTI_ACCESS_PUBLIC)
      continue;
    void *p = (char *)objptr + offset_list[i];
    if (is_virtual_list[i])
      p = *(void **)p;

    if ((p=base_list[i]->__rtti_match (desired, is_public, p))
	!= 0)
      if (match_found == 0)
	match_found = p;
      else if (match_found != p) {
	// base found at two different pointers,
	// conversion is not unique
	return 0;
      }
  }

  return match_found;
}

void*
__pointer_type_info::__rtti_match (const type_info& catch_type, int,
				   void *objptr) const
{
  if (catch_type.__rtti_get_node_type () == __rtti_get_node_type())
    {
      type_info &sub_catch_type = ((__pointer_type_info&)catch_type).type;
      type_info &sub_throw_type = ((__pointer_type_info*)this)->type;
      if (sub_catch_type.__rtti_get_node_type () == _RTTI_BUILTIN_TYPE
	  && ((__builtin_type_info&)sub_catch_type).b_type == __builtin_type_info::_RTTI_BI_VOID)
	{
	  return objptr;
	}
      if (sub_catch_type.__rtti_get_node_type () == _RTTI_ATTR_TYPE
	  && ((__attr_type_info&)sub_catch_type).attr == __attr_type_info::_RTTI_ATTR_CONST)
	{
	  /* We have to allow a catch of const int* on a int * throw. */
	  type_info &sub_sub_catch_type = ((__attr_type_info&)sub_catch_type).type;
	  return __throw_type_match_rtti (&sub_sub_catch_type, &sub_throw_type, objptr);
	}
      return __throw_type_match_rtti (&sub_catch_type, &sub_throw_type, objptr);
    }
  return 0;
}

/* Low level match routine used by compiler to match types of catch variables and thrown
   objects.  */
extern "C"
void*
__throw_type_match_rtti (void *catch_type_r, void *throw_type_r, void *objptr)
{
  type_info &catch_type = *(type_info*)catch_type_r;
  type_info &throw_type = *(type_info*)throw_type_r;
  void *new_objptr;

  if (catch_type == throw_type)
    return objptr;

  /* Ensure we can call __rtti_match.  */
  if ((throw_type.__rtti_get_node_type () != type_info::_RTTI_CLASS_TYPE
       && throw_type.__rtti_get_node_type () != type_info::_RTTI_USER_TYPE
       && throw_type.__rtti_get_node_type () != type_info::_RTTI_POINTER_TYPE)
      || ((catch_type.__rtti_get_node_type () != type_info::_RTTI_CLASS_TYPE
	   && catch_type.__rtti_get_node_type () != type_info::_RTTI_USER_TYPE
	   && catch_type.__rtti_get_node_type () != type_info::_RTTI_POINTER_TYPE)))
    return 0;

#if 0
  printf ("We want to match a %s against a %s!\n",
	  throw_type.name (), catch_type.name ());
#endif

  /* The 1 skips conversions to private bases. */
  new_objptr = throw_type.__rtti_match (catch_type, 1, objptr);
#if 0
  if (new_objptr)
    printf ("It converts, delta is %d\n", new_objptr-objptr);
#endif
  return new_objptr;
}

bad_cast __bad_cast_object ("bad_cast");
