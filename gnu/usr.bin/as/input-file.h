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
   along with GAS; see the file COPYING.  If not, write to
   the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

/*"input_file.c":Operating-system dependant functions to read source files.*/

/*
 * $FreeBSD$
 */


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

#if __STDC__ == 1

char *input_file_give_next_buffer(char *where);
char *input_file_push(void);
int input_file_buffer_size(void);
int input_file_is_open(void);
void input_file_begin(void);
void input_file_close(void);
void input_file_end(void);
void input_file_open(char *filename, int pre);
void input_file_pop(char *arg);

#else /* not __STDC__ */

char *input_file_give_next_buffer();
char *input_file_push();
int input_file_buffer_size();
int input_file_is_open();
void input_file_begin();
void input_file_close();
void input_file_end();
void input_file_open();
void input_file_pop();

#endif /* not __STDC__ */

/* end of input_file.h */
