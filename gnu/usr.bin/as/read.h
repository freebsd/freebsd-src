/* read.h - of read.c
   Copyright (C) 1986 Free Software Foundation, Inc.

This file is part of GAS, the GNU Assembler.

GAS is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

GAS is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GAS; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

extern	char	* input_line_pointer; /* -> char we are parsing now. */

#define PERMIT_WHITESPACE	/* Define to make whitespace be allowed in */
				/* many syntactically unnecessary places. */
				/* Normally undefined. For compatibility */
				/* with ancient GNU cc. */
#undef PERMIT_WHITESPACE

#ifdef PERMIT_WHITESPACE
#define SKIP_WHITESPACE() {if (* input_line_pointer == ' ') ++ input_line_pointer;}
#else
#define SKIP_WHITESPACE() ASSERT( * input_line_pointer != ' ' )
#endif


#define	LEX_NAME	(1)	/* may continue a name */		      
#define LEX_BEGIN_NAME	(2)	/* may begin a name */			      
						        		      
#define is_name_beginner(c)     ( lex_type[c] & LEX_BEGIN_NAME )
#define is_part_of_name(c)      ( lex_type[c] & LEX_NAME       )

extern const char lex_type[];

void		read_begin();
void		read_end();
void		read_a_source_file();

/* end: read.h */
