/* arsup.h - archive support header file
   Copyright 1992 Free Software Foundation, Inc.

This file is part of GNU Binutils.

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

struct list {
	char *name;	
	struct list *next;
};

void
maybequit PARAMS ((void));

void
prompt PARAMS ((void));

void
ar_clear PARAMS ((void));

void
ar_replace PARAMS ((struct list *));

void
ar_delete PARAMS ((struct list *));

void
ar_save PARAMS ((void));

void
ar_list PARAMS ((void));

void
ar_open PARAMS ((char *, int));

void
ar_directory PARAMS ((char *, struct list *, char *));

void
ar_addmod PARAMS ((struct list *));

void
ar_addlib PARAMS ((char *, struct list *));

void 
ar_end PARAMS ((void));

void
ar_extract PARAMS ((struct list *));

bfd *
open_inarch PARAMS ((const char *archive_filename, const char *));

int
yyparse PARAMS ((void));

/* Functions from ar.c */

void
extract_file PARAMS ((bfd * abfd));

extern int interactive;
