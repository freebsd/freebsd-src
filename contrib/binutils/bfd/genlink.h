/* genlink.h -- interface to the BFD generic linker
   Copyright 1993, 1994, 1996 Free Software Foundation, Inc.
   Written by Ian Lance Taylor, Cygnus Support.

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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef GENLINK_H
#define GENLINK_H

/* This header file is internal to BFD.  It describes the internal
   structures and functions used by the BFD generic linker, in case
   any of the more specific linkers want to use or call them.  Note
   that some functions, such as _bfd_generic_link_hash_table_create,
   are declared in libbfd.h, because they are expected to be widely
   used.  The functions and structures in this file will probably only
   be used by a few files besides linker.c itself.  In fact, this file
   is not particularly complete; I have only put in the interfaces I
   actually needed.  */

/* The generic linker uses a hash table which is a derived class of
   the standard linker hash table, just as the other backend specific
   linkers do.  Do not confuse the generic linker hash table with the
   standard BFD linker hash table it is built upon.  */

/* Generic linker hash table entries.  */

struct generic_link_hash_entry
{
  struct bfd_link_hash_entry root;
  /* Whether this symbol has been written out.  */
  boolean written;
  /* Symbol from input BFD.  */
  asymbol *sym;
};

/* Generic linker hash table.  */

struct generic_link_hash_table
{
  struct bfd_link_hash_table root;
};

/* Look up an entry in an generic link hash table.  */

#define _bfd_generic_link_hash_lookup(table, string, create, copy, follow) \
  ((struct generic_link_hash_entry *) \
   bfd_link_hash_lookup (&(table)->root, (string), (create), (copy), (follow)))

/* Traverse an generic link hash table.  */

#define _bfd_generic_link_hash_traverse(table, func, info)		\
  (bfd_link_hash_traverse						\
   (&(table)->root,							\
    (boolean (*) PARAMS ((struct bfd_link_hash_entry *, PTR))) (func),	\
    (info)))

/* Get the generic link hash table from the info structure.  This is
   just a cast.  */

#define _bfd_generic_hash_table(p) \
  ((struct generic_link_hash_table *) ((p)->hash))

/* The generic linker reads in the asymbol structures for an input BFD
   and keeps them in the outsymbol and symcount fields.  */

#define _bfd_generic_link_get_symbols(abfd) ((abfd)->outsymbols)
#define _bfd_generic_link_get_symcount(abfd) ((abfd)->symcount)

/* Add the symbols of input_bfd to the symbols being built for
   output_bfd.  */
extern boolean _bfd_generic_link_output_symbols
  PARAMS ((bfd *output_bfd, bfd *input_bfd, struct bfd_link_info *,
	   size_t *psymalloc));

/* This structure is used to pass information to
   _bfd_generic_link_write_global_symbol, which may be called via
   _bfd_generic_link_hash_traverse.  */

struct generic_write_global_symbol_info
{
  struct bfd_link_info *info;
  bfd *output_bfd;
  size_t *psymalloc;
};

/* Write out a single global symbol.  This is expected to be called
   via _bfd_generic_link_hash_traverse.  The second argument must
   actually be a struct generic_write_global_symbol_info *.  */
extern boolean _bfd_generic_link_write_global_symbol
  PARAMS ((struct generic_link_hash_entry *, PTR));

/* Generic link hash table entry creation routine.  */
struct bfd_hash_entry *_bfd_generic_link_hash_newfunc
  PARAMS ((struct bfd_hash_entry *, struct bfd_hash_table *,
           const char *));

#endif
