/* C language support definitions for GDB, the GNU debugger.
   Copyright 1992 Free Software Foundation, Inc.

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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifdef __STDC__		/* Forward decls for prototypes */
struct value;
#endif

extern int
c_parse PARAMS ((void));	/* Defined in c-exp.y */

extern void
c_error PARAMS ((char *));	/* Defined in c-exp.y */

extern void			/* Defined in c-typeprint.c */
c_print_type PARAMS ((struct type *, char *, GDB_FILE *, int, int));

extern int
c_val_print PARAMS ((struct type *, char *, CORE_ADDR, GDB_FILE *, int, int,
		     int, enum val_prettyprint));

extern int
c_value_print PARAMS ((struct value *, GDB_FILE *, int, enum val_prettyprint));

/* These are in c-lang.c: */

extern void c_printchar PARAMS ((int, GDB_FILE*));

extern void c_printstr PARAMS ((GDB_FILE *, char *, unsigned int, int));

extern struct type * c_create_fundamental_type PARAMS ((struct objfile*, int));

extern struct type ** const (c_builtin_types[]);

/* These are in c-typeprint.c: */

extern void
c_type_print_base PARAMS ((struct type *, GDB_FILE *, int, int));

extern void
c_type_print_varspec_prefix PARAMS ((struct type *, GDB_FILE *, int, int));

extern void
cp_type_print_method_args PARAMS ((struct type **, char *, char *, int,
				   GDB_FILE *));
/* These are in cp-valprint.c */

extern void
cp_type_print_method_args PARAMS ((struct type **, char *, char *, int,
				   GDB_FILE *));

extern int vtblprint;		/* Controls printing of vtbl's */

extern void
cp_print_class_member PARAMS ((char *, struct type *, GDB_FILE *, char *));

extern void
cp_print_class_method PARAMS ((char *, struct type *, GDB_FILE *));

extern void
cp_print_value_fields PARAMS ((struct type *, char *, CORE_ADDR,
			       GDB_FILE *, int, int, enum val_prettyprint,
			       struct type**, int));

extern int
cp_is_vtbl_ptr_type PARAMS ((struct type *));

extern int
cp_is_vtbl_member PARAMS ((struct type *));
