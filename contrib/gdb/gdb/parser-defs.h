/* Parser definitions for GDB.
   Copyright (C) 1986, 1989, 1990, 1991 Free Software Foundation, Inc.
   Modified from expread.y by the Department of Computer Science at the
   State University of New York at Buffalo.

This file is part of GDB.

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

#if !defined (PARSER_DEFS_H)
#define PARSER_DEFS_H 1

struct std_regs {
	char *name;
	int regnum;
};

extern struct std_regs std_regs[];
extern unsigned num_std_regs;

extern struct expression *expout;
extern int expout_size;
extern int expout_ptr;

/* If this is nonzero, this block is used as the lexical context
   for symbol names.  */

extern struct block *expression_context_block;

/* The innermost context required by the stack and register variables
   we've encountered so far. */
extern struct block *innermost_block;

/* The block in which the most recently discovered symbol was found.
   FIXME: Should be declared along with lookup_symbol in symtab.h; is not
   related specifically to parsing.  */
extern struct block *block_found;

/* Number of arguments seen so far in innermost function call.  */
extern int arglist_len;

/* A string token, either a char-string or bit-string.  Char-strings are
   used, for example, for the names of symbols. */

struct stoken
  {
    /* Pointer to first byte of char-string or first bit of bit-string */
    char *ptr;
    /* Length of string in bytes for char-string or bits for bit-string */
    int length;
  };

struct ttype
  {
    struct stoken stoken;
    struct type *type;
  };

struct symtoken
  {
    struct stoken stoken;
    struct symbol *sym;
    int is_a_field_of_this;
  };

/* For parsing of complicated types.
   An array should be preceded in the list by the size of the array.  */
enum type_pieces
  {tp_end = -1, tp_pointer, tp_reference, tp_array, tp_function};
/* The stack can contain either an enum type_pieces or an int.  */
union type_stack_elt {
  enum type_pieces piece;
  int int_val;
};
extern union type_stack_elt *type_stack;
extern int type_stack_depth, type_stack_size;

extern void write_exp_elt PARAMS ((union exp_element));

extern void write_exp_elt_opcode PARAMS ((enum exp_opcode));

extern void write_exp_elt_sym PARAMS ((struct symbol *));

extern void write_exp_elt_longcst PARAMS ((LONGEST));

extern void write_exp_elt_dblcst PARAMS ((DOUBLEST));

extern void write_exp_elt_type PARAMS ((struct type *));

extern void write_exp_elt_intern PARAMS ((struct internalvar *));

extern void write_exp_string PARAMS ((struct stoken));

extern void write_exp_bitstring PARAMS ((struct stoken));

extern void write_exp_elt_block PARAMS ((struct block *));

extern void write_exp_msymbol PARAMS ((struct minimal_symbol *,
				       struct type *, struct type *));

extern void write_dollar_variable PARAMS ((struct stoken str));

extern void
start_arglist PARAMS ((void));

extern int
end_arglist PARAMS ((void));

extern char *
copy_name PARAMS ((struct stoken));

extern void 
push_type PARAMS ((enum type_pieces));

extern void
push_type_int PARAMS ((int));

extern enum type_pieces 
pop_type PARAMS ((void));

extern int
pop_type_int PARAMS ((void));

extern struct type *follow_types PARAMS ((struct type *));

/* During parsing of a C expression, the pointer to the next character
   is in this variable.  */

extern char *lexptr;

/* Tokens that refer to names do so with explicit pointer and length,
   so they can share the storage that lexptr is parsing.

   When it is necessary to pass a name to a function that expects
   a null-terminated string, the substring is copied out
   into a block of storage that namecopy points to.

   namecopy is allocated once, guaranteed big enough, for each parsing.  */

extern char *namecopy;

/* Current depth in parentheses within the expression.  */

extern int paren_depth;

/* Nonzero means stop parsing on first comma (if not within parentheses).  */

extern int comma_terminates;

/* These codes indicate operator precedences for expression printing,
   least tightly binding first.  */
/* Adding 1 to a precedence value is done for binary operators,
   on the operand which is more tightly bound, so that operators
   of equal precedence within that operand will get parentheses.  */
/* PREC_HYPER and PREC_ABOVE_COMMA are not the precedence of any operator;
   they are used as the "surrounding precedence" to force
   various kinds of things to be parenthesized.  */
enum precedence
{ PREC_NULL, PREC_COMMA, PREC_ABOVE_COMMA, PREC_ASSIGN, PREC_LOGICAL_OR,
  PREC_LOGICAL_AND, PREC_BITWISE_IOR, PREC_BITWISE_AND, PREC_BITWISE_XOR,
  PREC_EQUAL, PREC_ORDER, PREC_SHIFT, PREC_ADD, PREC_MUL, PREC_REPEAT,
  PREC_HYPER, PREC_PREFIX, PREC_SUFFIX, PREC_BUILTIN_FUNCTION };

/* Table mapping opcodes into strings for printing operators
   and precedences of the operators.  */

struct op_print
{
  char *string;
  enum exp_opcode opcode;
  /* Precedence of operator.  These values are used only by comparisons.  */
  enum precedence precedence;

  /* For a binary operator:  1 iff right associate.
     For a unary operator:  1 iff postfix. */
  int right_assoc;
};

#endif	/* PARSER_DEFS_H */
