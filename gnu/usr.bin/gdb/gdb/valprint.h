/* Declarations for value printing routines for GDB, the GNU debugger.
   Copyright 1986, 1988, 1989, 1991, 1992, 1993, 1994
             Free Software Foundation, Inc.

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
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */


extern int prettyprint_arrays;	/* Controls pretty printing of arrays.  */
extern int prettyprint_structs;	/* Controls pretty printing of structures */
extern int prettyprint_arrays;	/* Controls pretty printing of arrays.  */

extern int vtblprint;		/* Controls printing of vtbl's */
extern int unionprint;		/* Controls printing of nested unions.  */
extern int addressprint;	/* Controls pretty printing of addresses.  */
extern int objectprint;		/* Controls looking up an object's derived type
				   using what we find in its vtables.  */

extern unsigned int print_max;	/* Max # of chars for strings/vectors */

extern int output_format;

extern int stop_print_at_null;  /* Stop printing at null char? */

extern void
val_print_array_elements PARAMS ((struct type *, char *, CORE_ADDR, GDB_FILE *,
				  int, int, int, enum val_prettyprint, int));

extern void
val_print_type_code_int PARAMS ((struct type *, char *, GDB_FILE *));

