/* Chill language support definitions for GDB, the GNU debugger.
   Copyright 1992, 1994, 1996, 1998, 1999, 2000
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
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

/* Forward decls for prototypes */
struct value;

extern int chill_parse (void);	/* Defined in ch-exp.y */

extern void chill_error (char *);	/* Defined in ch-exp.y */

/* Defined in ch-typeprint.c */
extern void chill_print_type (struct type *, char *, struct ui_file *, int,
			      int);

extern int chill_val_print (struct type *, char *, int, CORE_ADDR,
			    struct ui_file *, int, int, int,
			    enum val_prettyprint);

extern int chill_value_print (struct value *, struct ui_file *,
			      int, enum val_prettyprint);

extern LONGEST
type_lower_upper (enum exp_opcode, struct type *, struct type **);
