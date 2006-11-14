/* Unicode character output to streams with locale dependent encoding.

   Copyright (C) 2000, 2001 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef UNICODEIO_H
# define UNICODEIO_H

# include <stdio.h>

# ifndef PARAMS
#  if defined PROTOTYPES || (defined __STDC__ && __STDC__)
#   define PARAMS(Args) Args
#  else
#   define PARAMS(Args) ()
#  endif
# endif

/* Converts the Unicode character CODE to its multibyte representation
   in the current locale and calls the CALLBACK on the resulting byte
   sequence.  If an error occurs, invokes ERROR_CALLBACK instead,
   passing it CODE with errno set appropriately.  Returns whatever the
   callback returns.  */
extern int unicode_to_mb
	    PARAMS ((unsigned int code,
		     int (*callback) PARAMS ((const char *buf, size_t buflen,
					      void *callback_arg)),
		     int (*error_callback) PARAMS ((unsigned int code,
						    void * callback_arg)),
		     void *callback_arg));

/* Success callback that outputs the conversion of the character.  */
extern int print_unicode_success PARAMS((const char *buf, size_t buflen,
					 void *callback_arg));

/* Failure callback that outputs an ASCII representation.  */
extern int print_unicode_failure PARAMS((unsigned int code,
					 void *callback_arg));

/* Outputs the Unicode character CODE to the output stream STREAM.
   Returns -1 (setting errno) if unsuccessful.  */
extern int print_unicode_char PARAMS((FILE *stream, unsigned int code));

#endif
