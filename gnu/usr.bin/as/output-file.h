/* This file is output-file.h

   Copyright (C) 1987-1992 Free Software Foundation, Inc.

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
   the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */
/*
 * $FreeBSD: src/gnu/usr.bin/as/output-file.h,v 1.5 1999/08/27 23:34:20 peter Exp $
 */


#ifdef __STDC__

void output_file_append(char *where, long length, char *filename);
void output_file_close(char *filename);
void output_file_create(char *name);

#else /* __STDC__ */

void output_file_append();
void output_file_close();
void output_file_create();

#endif /* __STDC__ */


/* end of output-file.h */
