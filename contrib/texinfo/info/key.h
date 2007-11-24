/* key.h -- Structure associating function names with numeric codes. */

/* This file is part of GNU Info, a program for reading online documentation
   stored in Info format.

   Copyright (C) 1993 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

   Written by Andrew Bettison <andrewb@zip.com.au> */

#if !defined (KEY_H)
#define KEY_H

typedef struct {
	char *name;
	unsigned char	code;
}
	FUNCTION_KEY;

extern FUNCTION_KEY function_key_array[];

#endif /* !KEY_H */
