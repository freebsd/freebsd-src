/* hash.h -- header file for gas hash table routines
   Copyright (C) 1987, 92, 93, 95, 1999 Free Software Foundation, Inc.

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to the Free
   Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

#ifndef HASH_H
#define HASH_H

struct hash_control;

/* Create a hash table.  This return a control block.  */

extern struct hash_control *hash_new PARAMS ((void));

/* Delete a hash table, freeing all allocated memory.  */

extern void hash_die PARAMS ((struct hash_control *));

/* Insert an entry into a hash table.  This returns NULL on success.
   On error, it returns a printable string indicating the error.  It
   is considered to be an error if the entry already exists in the
   hash table.  */

extern const char *hash_insert PARAMS ((struct hash_control *,
					const char *key, PTR value));

/* Insert or replace an entry in a hash table.  This returns NULL on
   success.  On error, it returns a printable string indicating the
   error.  If an entry already exists, its value is replaced.  */

extern const char *hash_jam PARAMS ((struct hash_control *,
				     const char *key, PTR value));

/* Replace an existing entry in a hash table.  This returns the old
   value stored for the entry.  If the entry is not found in the hash
   table, this does nothing and returns NULL.  */

extern PTR hash_replace PARAMS ((struct hash_control *, const char *key,
				 PTR value));

/* Find an entry in a hash table, returning its value.  Returns NULL
   if the entry is not found.  */

extern PTR hash_find PARAMS ((struct hash_control *, const char *key));

/* Delete an entry from a hash table.  This returns the value stored
   for that entry, or NULL if there is no such entry.  */

extern PTR hash_delete PARAMS ((struct hash_control *, const char *key));

/* Traverse a hash table.  Call the function on every entry in the
   hash table.  */

extern void hash_traverse PARAMS ((struct hash_control *,
				   void (*pfn) (const char *key, PTR value)));

/* Print hash table statistics on the specified file.  NAME is the
   name of the hash table, used for printing a header.  */

extern void hash_print_statistics PARAMS ((FILE *, const char *name,
					   struct hash_control *));

#endif /* HASH_H */
