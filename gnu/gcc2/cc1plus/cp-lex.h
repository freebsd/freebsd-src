/* Define constants and variables for communication with cp-parse.y.
   Copyright (C) 1987, 1992, 1993 Free Software Foundation, Inc.
   Hacked by Michael Tiemann (tiemann@cygnus.com)
   and by Brendan Kehoe (brendan@cygnus.com).

This file is part of GNU CC.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY.  No author or distributor
accepts responsibility to anyone for the consequences of using it
or for whether it serves any particular purpose or works at all,
unless he says so in writing.  Refer to the GNU CC General Public
License for full details.

Everyone is granted permission to copy, modify and redistribute
GNU CC, but only under the conditions described in the
GNU CC General Public License.   A copy of this license is
supposed to have been given to you along with GNU CC so you
can know your rights and responsibilities.  It should be in a
file named COPYING.  Among other things, the copyright notice
and this notice must be preserved on all copies.  */



enum rid
{
  RID_UNUSED,
  RID_INT,
  RID_CHAR,
  RID_WCHAR,
  RID_FLOAT,
  RID_DOUBLE,
  RID_VOID,

  /* C++ extension */
  RID_CLASS,
  RID_RECORD,
  RID_UNION,
  RID_ENUM,
  RID_LONGLONG,

  /* This is where grokdeclarator starts its search when setting the specbits.
     The first seven are in the order of most frequently used, as found
     building libg++.  */

  RID_EXTERN,
  RID_CONST,
  RID_LONG,
  RID_TYPEDEF,
  RID_UNSIGNED,
  RID_SHORT,
  RID_INLINE,

  RID_STATIC,

  RID_REGISTER,
  RID_VOLATILE,
  RID_FRIEND,
  RID_VIRTUAL,
  RID_PUBLIC,
  RID_PRIVATE,
  RID_PROTECTED,
  RID_SIGNED,
  RID_EXCEPTION,
  RID_RAISES,
  RID_AUTO,

  /* Note this is 31, and is unusable in shifts where ints are 32 bits.
     As soon as a new rid has to be added to this enum, you have to
     stop and come up with a better way to do all of this than by
     doing `specbits & (1 << (int) RID_FOO)', since you'll end up
     with an integer overflow.  */
  RID_UNUSED1,

  RID_MAX
};

#define NORID RID_UNUSED

#define RID_FIRST_MODIFIER RID_EXTERN

/* The integral type that can represent all values of RIDBIT.  */
typedef unsigned long RID_BIT_TYPE;

/* A bit that represents the given RID_... value.  */
#define RIDBIT(N)  ((RID_BIT_TYPE) 1 << (int) (N))

/* The elements of `ridpointers' are identifier nodes
   for the reserved type names and storage classes.
   It is indexed by a RID_... value.  */
extern tree ridpointers[(int) RID_MAX];

/* the declaration found for the last IDENTIFIER token read in.
   yylex must look this up to detect typedefs, which get token type TYPENAME,
   so it is left around in case the identifier is not a typedef but is
   used in a context which makes it a reference to a variable.  */
extern tree lastiddecl;

extern char *token_buffer;	/* Pointer to token buffer.  */

/* Back-door communication channel to the lexer.  */
extern int looking_for_typename;

extern tree make_pointer_declarator (), make_reference_declarator ();
extern void reinit_parse_for_function ();
extern void reinit_parse_for_method ();
extern int yylex ();
