/* read.h - of read.c

   Copyright (C) 1986, 1990, 1992 Free Software Foundation, Inc.

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
 * $FreeBSD: src/gnu/usr.bin/as/read.h,v 1.9 1999/08/27 23:34:21 peter Exp $
 */


extern char *input_line_pointer; /* -> char we are parsing now. */

#define PERMIT_WHITESPACE	/* Define to make whitespace be allowed in */
/* many syntactically unnecessary places. */
/* Normally undefined. For compatibility */
/* with ancient GNU cc. */
/* #undef PERMIT_WHITESPACE */

#ifdef PERMIT_WHITESPACE
#define SKIP_WHITESPACE() {if (* input_line_pointer == ' ') ++ input_line_pointer;}
#else
#define SKIP_WHITESPACE() know(*input_line_pointer != ' ' )
#endif


#define	LEX_NAME	(1)	/* may continue a name */
#define LEX_BEGIN_NAME	(2)	/* may begin a name */

#define is_name_beginner(c)     ( lex_type[c] & LEX_BEGIN_NAME )
#define is_part_of_name(c)      ( lex_type[c] & LEX_NAME       )

#ifndef is_a_char
#define CHAR_MASK	(0xff)
#define NOT_A_CHAR	(CHAR_MASK+1)
#define is_a_char(c)	(((unsigned)(c)) <= CHAR_MASK)
#endif /* is_a_char() */

#ifdef PIC
/* We change some of the entries in lex_type on some archs */
extern char lex_type[];
#else
extern const char lex_type[];
#endif
extern char is_end_of_line[];

/* These are initialized by the CPU specific target files (tc-*.c).  */
extern const char comment_chars[];
extern const char line_comment_chars[];
extern const char line_separator_chars[];

#if __STDC__ == 1

char *demand_copy_C_string(int *len_pointer);
char get_absolute_expression_and_terminator(long *val_pointer);
long get_absolute_expression(void);
void add_include_dir(char *path);
void big_cons(int nbytes);
void cons(unsigned int nbytes);
void demand_empty_rest_of_line(void);
void equals(char *sym_name);
void float_cons(int float_type);
void ignore_rest_of_line(void);
void pseudo_set(symbolS *symbolP);
void read_a_source_file(char *name);
void read_begin(void);
void s_abort(void);
void s_align_bytes(int arg);
void s_align_ptwo(void);
void s_app_file(int);
void s_app_line(int);
void s_comm(void);
void s_data(void);
void s_else(int arg);
void s_end(int arg);
void s_endif(int arg);
void s_fill(void);
void s_globl(void);
void s_if(int arg);
void s_ifdef(int arg);
void s_ifeqs(int arg);
void s_ignore(int arg);
void s_include(int arg);
void s_lcomm(int needs_align);
void s_lsym(void);
void s_org(void);
void s_set(void);
void s_size(void);
void s_space(void);
void s_text(void);
void s_type(void);
void s_weak(void);

#else /* not __STDC__ */

char *demand_copy_C_string();
char get_absolute_expression_and_terminator();
long get_absolute_expression();
void add_include_dir();
void big_cons();
void cons();
void demand_empty_rest_of_line();
void equals();
void float_cons();
void ignore_rest_of_line();
void pseudo_set();
void read_a_source_file();
void read_begin();
void s_abort();
void s_align_bytes();
void s_align_ptwo();
void s_app_file();
void s_app_line();
void s_comm();
void s_data();
void s_else();
void s_end();
void s_endif();
void s_fill();
void s_globl();
void s_if();
void s_ifdef();
void s_ifeqs();
void s_ignore();
void s_include();
void s_lcomm();
void s_lsym();
void s_org();
void s_set();
void s_size();
void s_space();
void s_text();
void s_type();
void s_weak();

#endif /* not __STDC__ */

/*
 * Local Variables:
 * comment-column: 0
 * fill-column: 131
 * End:
 */

/* end of read.h */
