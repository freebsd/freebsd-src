/* input_file.h header for input-file.c
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
   along with GAS; see the file COPYING.  If not, write to the Free
   Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

/*"input_file.c":Operating-system dependant functions to read source files.*/

/*
 * No matter what the operating system, this module must provide the
 * following services to its callers.
 *
 * input_file_begin()			Call once before anything else.
 *
 * input_file_end()			Call once after everything else.
 *
 * input_file_buffer_size()		Call anytime. Returns largest possible
 *					delivery from
 *					input_file_give_next_buffer().
 *
 * input_file_open(name)		Call once for each input file.
 *
 * input_file_give_next_buffer(where)	Call once to get each new buffer.
 *					Return 0: no more chars left in file,
 *					   the file has already been closed.
 *					Otherwise: return a pointer to just
 *					   after the last character we read
 *					   into the buffer.
 *					If we can only read 0 characters, then
 *					end-of-file is faked.
 *
 * input_file_push()			Push state, which can be restored
 *					later.  Does implicit input_file_begin.
 *					Returns char * to saved state.
 *
 * input_file_pop (arg)			Pops previously saved state.
 *
 * input_file_close ()			Closes opened file.
 *
 * All errors are reported (using as_perror) so caller doesn't have to think
 * about I/O errors. No I/O errors are fatal: an end-of-file may be faked.
 */

char *input_file_give_next_buffer PARAMS ((char *where));
char *input_file_push PARAMS ((void));
unsigned int input_file_buffer_size PARAMS ((void));
int input_file_is_open PARAMS ((void));
void input_file_begin PARAMS ((void));
void input_file_close PARAMS ((void));
void input_file_end PARAMS ((void));
void input_file_open PARAMS ((char *filename, int pre));
void input_file_pop PARAMS ((char *arg));
