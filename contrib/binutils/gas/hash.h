/* hash.h - for hash.c
   Copyright (C) 1987, 1992 Free Software Foundation, Inc.

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
   along with GAS; see the file COPYING.  If not, write to
   the Free Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef hashH
#define hashH

struct hash_control;

/* returns control block */
struct hash_control *hash_new PARAMS ((void));
void hash_die PARAMS ((struct hash_control *));
/* returns previous value */
PTR hash_delete PARAMS ((struct hash_control *, const char *str));
/* returns previous value */
PTR hash_replace PARAMS ((struct hash_control *, const char *str, PTR val));
/* returns error string or null */
const char *hash_insert PARAMS ((struct hash_control *, const char *str,
				 PTR val));
/* returns value */
PTR hash_find PARAMS ((struct hash_control *, const char *str));
/* returns error text or null (internal) */
const char *hash_jam PARAMS ((struct hash_control *, const char *str,
			      PTR val));

void hash_print_statistics PARAMS ((FILE *, const char *,
				    struct hash_control *));
#endif /* #ifdef hashH */

/* end of hash.c */
