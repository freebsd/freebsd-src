/* Generic code for supporting multiple C++ ABI's
   Copyright 2001 Free Software Foundation, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include "defs.h"
#include "value.h"
#include "cp-abi.h"

struct cp_abi_ops current_cp_abi;

struct cp_abi_ops *cp_abis;

int num_cp_abis = 0;

enum ctor_kinds
is_constructor_name (const char *name)
{
  if ((current_cp_abi.is_constructor_name) == NULL)
    error ("ABI doesn't define required function is_constructor_name");
  return (*current_cp_abi.is_constructor_name) (name);
}

enum dtor_kinds
is_destructor_name (const char *name)
{
  if ((current_cp_abi.is_destructor_name) == NULL)
    error ("ABI doesn't define required function is_destructor_name");
  return (*current_cp_abi.is_destructor_name) (name);
}

int
is_vtable_name (const char *name)
{
  if ((current_cp_abi.is_vtable_name) == NULL)
    error ("ABI doesn't define required function is_vtable_name");
  return (*current_cp_abi.is_vtable_name) (name);
}

int
is_operator_name (const char *name)
{
  if ((current_cp_abi.is_operator_name) == NULL)
    error ("ABI doesn't define required function is_operator_name");
  return (*current_cp_abi.is_operator_name) (name);
}

int
baseclass_offset (struct type *type, int index, char *valaddr,
		  CORE_ADDR address)
{
  if (current_cp_abi.baseclass_offset == NULL)
    error ("ABI doesn't define required function baseclass_offset");
  return (*current_cp_abi.baseclass_offset) (type, index, valaddr, address);
}

struct value *
value_virtual_fn_field (struct value **arg1p, struct fn_field * f, int j,
			struct type * type, int offset)
{
  if ((current_cp_abi.virtual_fn_field) == NULL)
    return NULL;
  return (*current_cp_abi.virtual_fn_field) (arg1p, f, j, type, offset);
}

struct type *
value_rtti_type (struct value *v, int *full, int *top, int *using_enc)
{
  if ((current_cp_abi.rtti_type) == NULL)
    return NULL;
  return (*current_cp_abi.rtti_type) (v, full, top, using_enc);
}

int
register_cp_abi (struct cp_abi_ops abi)
{
  cp_abis =
    xrealloc (cp_abis, (num_cp_abis + 1) * sizeof (struct cp_abi_ops));
  cp_abis[num_cp_abis++] = abi;

  return 1;

}

int
switch_to_cp_abi (const char *short_name)
{
  int i;
  for (i = 0; i < num_cp_abis; i++)
    if (strcmp (cp_abis[i].shortname, short_name) == 0)
      current_cp_abi = cp_abis[i];
  return 1;
}

