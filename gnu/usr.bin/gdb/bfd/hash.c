/* hash.c -- hash table routines for BFD
   Copyright (C) 1993, 94 Free Software Foundation, Inc.
   Written by Steve Chamberlain <sac@cygnus.com>

This file is part of BFD, the Binary File Descriptor library.

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
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "obstack.h"

/*
SECTION
	Hash Tables

@cindex Hash tables
	BFD provides a simple set of hash table functions.  Routines
	are provided to initialize a hash table, to free a hash table,
	to look up a string in a hash table and optionally create an
	entry for it, and to traverse a hash table.  There is
	currently no routine to delete an string from a hash table.

	The basic hash table does not permit any data to be stored
	with a string.  However, a hash table is designed to present a
	base class from which other types of hash tables may be
	derived.  These derived types may store additional information
	with the string.  Hash tables were implemented in this way,
	rather than simply providing a data pointer in a hash table
	entry, because they were designed for use by the linker back
	ends.  The linker may create thousands of hash table entries,
	and the overhead of allocating private data and storing and
	following pointers becomes noticeable.

	The basic hash table code is in <<hash.c>>.

@menu
@* Creating and Freeing a Hash Table::
@* Looking Up or Entering a String::
@* Traversing a Hash Table::
@* Deriving a New Hash Table Type::
@end menu

INODE
Creating and Freeing a Hash Table, Looking Up or Entering a String, Hash Tables, Hash Tables
SUBSECTION
	Creating and freeing a hash table

@findex bfd_hash_table_init
@findex bfd_hash_table_init_n
	To create a hash table, create an instance of a <<struct
	bfd_hash_table>> (defined in <<bfd.h>>) and call
	<<bfd_hash_table_init>> (if you know approximately how many
	entries you will need, the function <<bfd_hash_table_init_n>>,
	which takes a @var{size} argument, may be used).
	<<bfd_hash_table_init>> returns <<false>> if some sort of
	error occurs.

@findex bfd_hash_newfunc
	The function <<bfd_hash_table_init>> take as an argument a
	function to use to create new entries.  For a basic hash
	table, use the function <<bfd_hash_newfunc>>.  @xref{Deriving
	a New Hash Table Type} for why you would want to use a
	different value for this argument.

@findex bfd_hash_allocate
	<<bfd_hash_table_init>> will create an obstack which will be
	used to allocate new entries.  You may allocate memory on this
	obstack using <<bfd_hash_allocate>>.

@findex bfd_hash_table_free
	Use <<bfd_hash_table_free>> to free up all the memory that has
	been allocated for a hash table.  This will not free up the
	<<struct bfd_hash_table>> itself, which you must provide.

INODE
Looking Up or Entering a String, Traversing a Hash Table, Creating and Freeing a Hash Table, Hash Tables
SUBSECTION
	Looking up or entering a string

@findex bfd_hash_lookup
	The function <<bfd_hash_lookup>> is used both to look up a
	string in the hash table and to create a new entry.

	If the @var{create} argument is <<false>>, <<bfd_hash_lookup>>
	will look up a string.  If the string is found, it will
	returns a pointer to a <<struct bfd_hash_entry>>.  If the
	string is not found in the table <<bfd_hash_lookup>> will
	return <<NULL>>.  You should not modify any of the fields in
	the returns <<struct bfd_hash_entry>>.

	If the @var{create} argument is <<true>>, the string will be
	entered into the hash table if it is not already there.
	Either way a pointer to a <<struct bfd_hash_entry>> will be
	returned, either to the existing structure or to a newly
	created one.  In this case, a <<NULL>> return means that an
	error occurred.

	If the @var{create} argument is <<true>>, and a new entry is
	created, the @var{copy} argument is used to decide whether to
	copy the string onto the hash table obstack or not.  If
	@var{copy} is passed as <<false>>, you must be careful not to
	deallocate or modify the string as long as the hash table
	exists.

INODE
Traversing a Hash Table, Deriving a New Hash Table Type, Looking Up or Entering a String, Hash Tables
SUBSECTION
	Traversing a hash table

@findex bfd_hash_traverse
	The function <<bfd_hash_traverse>> may be used to traverse a
	hash table, calling a function on each element.  The traversal
	is done in a random order.

	<<bfd_hash_traverse>> takes as arguments a function and a
	generic <<void *>> pointer.  The function is called with a
	hash table entry (a <<struct bfd_hash_entry *>>) and the
	generic pointer passed to <<bfd_hash_traverse>>.  The function
	must return a <<boolean>> value, which indicates whether to
	continue traversing the hash table.  If the function returns
	<<false>>, <<bfd_hash_traverse>> will stop the traversal and
	return immediately.

INODE
Deriving a New Hash Table Type, , Traversing a Hash Table, Hash Tables
SUBSECTION
	Deriving a new hash table type

	Many uses of hash tables want to store additional information
	which each entry in the hash table.  Some also find it
	convenient to store additional information with the hash table
	itself.  This may be done using a derived hash table.

	Since C is not an object oriented language, creating a derived
	hash table requires sticking together some boilerplate
	routines with a few differences specific to the type of hash
	table you want to create.

	An example of a derived hash table is the linker hash table.
	The structures for this are defined in <<bfdlink.h>>.  The
	functions are in <<linker.c>>.

	You may also derive a hash table from an already derived hash
	table.  For example, the a.out linker backend code uses a hash
	table derived from the linker hash table.

@menu
@* Define the Derived Structures::
@* Write the Derived Creation Routine::
@* Write Other Derived Routines::
@end menu

INODE
Define the Derived Structures, Write the Derived Creation Routine, Deriving a New Hash Table Type, Deriving a New Hash Table Type
SUBSUBSECTION
	Define the derived structures

	You must define a structure for an entry in the hash table,
	and a structure for the hash table itself.

	The first field in the structure for an entry in the hash
	table must be of the type used for an entry in the hash table
	you are deriving from.  If you are deriving from a basic hash
	table this is <<struct bfd_hash_entry>>, which is defined in
	<<bfd.h>>.  The first field in the structure for the hash
	table itself must be of the type of the hash table you are
	deriving from itself.  If you are deriving from a basic hash
	table, this is <<struct bfd_hash_table>>.

	For example, the linker hash table defines <<struct
	bfd_link_hash_entry>> (in <<bfdlink.h>>).  The first field,
	<<root>>, is of type <<struct bfd_hash_entry>>.  Similarly,
	the first field in <<struct bfd_link_hash_table>>, <<table>>,
	is of type <<struct bfd_hash_table>>.

INODE
Write the Derived Creation Routine, Write Other Derived Routines, Define the Derived Structures, Deriving a New Hash Table Type
SUBSUBSECTION
	Write the derived creation routine

	You must write a routine which will create and initialize an
	entry in the hash table.  This routine is passed as the
	function argument to <<bfd_hash_table_init>>.

	In order to permit other hash tables to be derived from the
	hash table you are creating, this routine must be written in a
	standard way.

	The first argument to the creation routine is a pointer to a
	hash table entry.  This may be <<NULL>>, in which case the
	routine should allocate the right amount of space.  Otherwise
	the space has already been allocated by a hash table type
	derived from this one.

	After allocating space, the creation routine must call the
	creation routine of the hash table type it is derived from,
	passing in a pointer to the space it just allocated.  This
	will initialize any fields used by the base hash table.

	Finally the creation routine must initialize any local fields
	for the new hash table type.

	Here is a boilerplate example of a creation routine.
	@var{function_name} is the name of the routine.
	@var{entry_type} is the type of an entry in the hash table you
	are creating.  @var{base_newfunc} is the name of the creation
	routine of the hash table type your hash table is derived
	from.

EXAMPLE

.struct bfd_hash_entry *
.@var{function_name} (entry, table, string)
.     struct bfd_hash_entry *entry;
.     struct bfd_hash_table *table;
.     const char *string;
.{
.  struct @var{entry_type} *ret = (@var{entry_type} *) entry;
.
. {* Allocate the structure if it has not already been allocated by a
.    derived class.  *}
.  if (ret == (@var{entry_type} *) NULL)
.    {
.      ret = ((@var{entry_type} *)
.	      bfd_hash_allocate (table, sizeof (@var{entry_type})));
.      if (ret == (@var{entry_type} *) NULL)
.        return NULL;
.    }
.
. {* Call the allocation method of the base class.  *}
.  ret = ((@var{entry_type} *)
.	 @var{base_newfunc} ((struct bfd_hash_entry *) ret, table, string));
.
. {* Initialize the local fields here.  *}
.
.  return (struct bfd_hash_entry *) ret;
.}

DESCRIPTION
	The creation routine for the linker hash table, which is in
	<<linker.c>>, looks just like this example.
	@var{function_name} is <<_bfd_link_hash_newfunc>>.
	@var{entry_type} is <<struct bfd_link_hash_entry>>.
	@var{base_newfunc} is <<bfd_hash_newfunc>>, the creation
	routine for a basic hash table.

	<<_bfd_link_hash_newfunc>> also initializes the local fields
	in a linker hash table entry: <<type>>, <<written>> and
	<<next>>.

INODE
Write Other Derived Routines, , Write the Derived Creation Routine, Deriving a New Hash Table Type
SUBSUBSECTION
	Write other derived routines

	You will want to write other routines for your new hash table,
	as well.  

	You will want an initialization routine which calls the
	initialization routine of the hash table you are deriving from
	and initializes any other local fields.  For the linker hash
	table, this is <<_bfd_link_hash_table_init>> in <<linker.c>>.

	You will want a lookup routine which calls the lookup routine
	of the hash table you are deriving from and casts the result.
	The linker hash table uses <<bfd_link_hash_lookup>> in
	<<linker.c>> (this actually takes an additional argument which
	it uses to decide how to return the looked up value).

	You may want a traversal routine.  This should just call the
	traversal routine of the hash table you are deriving from with
	appropriate casts.  The linker hash table uses
	<<bfd_link_hash_traverse>> in <<linker.c>>.

	These routines may simply be defined as macros.  For example,
	the a.out backend linker hash table, which is derived from the
	linker hash table, uses macros for the lookup and traversal
	routines.  These are <<aout_link_hash_lookup>> and
	<<aout_link_hash_traverse>> in aoutx.h.
*/

/* Obstack allocation and deallocation routines.  */
#define obstack_chunk_alloc malloc
#define obstack_chunk_free free

/* The default number of entries to use when creating a hash table.  */
#define DEFAULT_SIZE (4051)

/* Create a new hash table, given a number of entries.  */

boolean
bfd_hash_table_init_n (table, newfunc, size)
     struct bfd_hash_table *table;
     struct bfd_hash_entry *(*newfunc) PARAMS ((struct bfd_hash_entry *,
						struct bfd_hash_table *,
						const char *));
     unsigned int size;
{
  unsigned int alloc;

  alloc = size * sizeof (struct bfd_hash_entry *);
  if (!obstack_begin (&table->memory, alloc))
    {
      bfd_set_error (bfd_error_no_memory);
      return false;
    }
  table->table = ((struct bfd_hash_entry **)
		  obstack_alloc (&table->memory, alloc));
  if (!table->table)
    {
      bfd_set_error (bfd_error_no_memory);
      return false;
    }
  memset ((PTR) table->table, 0, alloc);
  table->size = size;
  table->newfunc = newfunc;
  return true;
}

/* Create a new hash table with the default number of entries.  */

boolean
bfd_hash_table_init (table, newfunc)
     struct bfd_hash_table *table;
     struct bfd_hash_entry *(*newfunc) PARAMS ((struct bfd_hash_entry *,
						struct bfd_hash_table *,
						const char *));
{
  return bfd_hash_table_init_n (table, newfunc, DEFAULT_SIZE);
}

/* Free a hash table.  */

void
bfd_hash_table_free (table)
     struct bfd_hash_table *table;
{
  obstack_free (&table->memory, (PTR) NULL);
}

/* Look up a string in a hash table.  */

struct bfd_hash_entry *
bfd_hash_lookup (table, string, create, copy)
     struct bfd_hash_table *table;
     const char *string;
     boolean create;
     boolean copy;
{
  register const unsigned char *s;
  register unsigned long hash;
  register unsigned int c;
  struct bfd_hash_entry *hashp;
  unsigned int len;
  unsigned int index;
  
  hash = 0;
  len = 0;
  s = (const unsigned char *) string;
  while ((c = *s++) != '\0')
    {
      hash += c + (c << 17);
      hash ^= hash >> 2;
      ++len;
    }
  hash += len + (len << 17);
  hash ^= hash >> 2;

  index = hash % table->size;
  for (hashp = table->table[index];
       hashp != (struct bfd_hash_entry *) NULL;
       hashp = hashp->next)
    {
      if (hashp->hash == hash
	  && strcmp (hashp->string, string) == 0)
	return hashp;
    }

  if (! create)
    return (struct bfd_hash_entry *) NULL;

  hashp = (*table->newfunc) ((struct bfd_hash_entry *) NULL, table, string);
  if (hashp == (struct bfd_hash_entry *) NULL)
    return (struct bfd_hash_entry *) NULL;
  if (copy)
    {
      char *new;

      new = (char *) obstack_alloc (&table->memory, len + 1);
      if (!new)
	{
	  bfd_set_error (bfd_error_no_memory);
	  return (struct bfd_hash_entry *) NULL;
	}
      strcpy (new, string);
      string = new;
    }
  hashp->string = string;
  hashp->hash = hash;
  hashp->next = table->table[index];
  table->table[index] = hashp;

  return hashp;
}

/* Base method for creating a new hash table entry.  */

/*ARGSUSED*/
struct bfd_hash_entry *
bfd_hash_newfunc (entry, table, string)
     struct bfd_hash_entry *entry;
     struct bfd_hash_table *table;
     const char *string;
{
  if (entry == (struct bfd_hash_entry *) NULL)
    entry = ((struct bfd_hash_entry *)
	     bfd_hash_allocate (table, sizeof (struct bfd_hash_entry)));
  return entry;
}

/* Allocate space in a hash table.  */

PTR
bfd_hash_allocate (table, size)
     struct bfd_hash_table *table;
     unsigned int size;
{
  PTR ret;

  ret = obstack_alloc (&table->memory, size);
  if (ret == NULL && size != 0)
    bfd_set_error (bfd_error_no_memory);
  return ret;
}

/* Traverse a hash table.  */

void
bfd_hash_traverse (table, func, info)
     struct bfd_hash_table *table;
     boolean (*func) PARAMS ((struct bfd_hash_entry *, PTR));
     PTR info;
{
  unsigned int i;

  for (i = 0; i < table->size; i++)
    {
      struct bfd_hash_entry *p;

      for (p = table->table[i]; p != NULL; p = p->next)
	{
	  if (! (*func) (p, info))
	    return;
	}
    }
}
