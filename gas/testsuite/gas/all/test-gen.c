#ifndef TEST_GEN_C
#define TEST_GEN_C 1

/* Copyright (C) 2000, 2003 Free Software Foundation
   Contributed by Alexandre Oliva <aoliva@cygnus.com>

   This file is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* This is a source file with infra-structure to test generators for
   assemblers and disassemblers.

   The strategy to generate testcases is as follows.  We'll output to
   two streams: one will get the assembly source, and the other will
   get regexps that match the expected binary patterns.

   To generate each instruction, the functions of a func[] are called,
   each with the corresponding func_arg.  Each function should set
   members of insn_data, to decide what it's going to output to the
   assembly source, the corresponding output for the disassembler
   tester, and the bits to be set in the instruction word.  The
   strings to be output must have been allocated with strdup() or
   malloc(), so that they can be freed.  A function may also modify
   insn_size.  More details in test-gen.c

   Because this would have generated too many tests, we have chosen to
   define ``random'' sequences of numbers/registers, and simply
   generate each instruction a couple of times, which should get us
   enough coverage.

   In general, test generators should be compiled/run as follows:
  
   % gcc test.c -o test
   % ./test > test.s 2 > test.d

   Please note that this file contains a couple of GCC-isms, such as
   macro varargs (also available in C99, but with a difference syntax)
   and labeled elements in initializers (so that insn definitions are
   simpler and safer).

   It is assumed that the test generator #includes this file after
   defining any of the preprocessor macros documented below.  The test
   generator is supposed to define instructions, at least one group of
   instructions, optionally, a sequence of groups.

   It should also define a main() function that outputs the initial
   lines of the assembler input and of the test control file, that
   also contains the disassembler output.  The main() funcion may
   optionally set skip_list too, before calling output_groups() or
   output_insns().  */

/* Define to 1 to avoid repeating instructions and to use a simpler
   register/constant generation mechanism.  This makes it much easier
   to verify that the generated bit patterns are correct.  */
#ifndef SIMPLIFY_OUTPUT
#define SIMPLIFY_OUTPUT 0
#endif

/* Define to 0 to avoid generating disassembler tests.  */
#ifndef DISASSEMBLER_TEST
#define DISASSEMBLER_TEST 1
#endif

/* Define to the number of times to repeat the generation of each
   insn.  It's best to use prime numbers, to improve randomization.  */
#ifndef INSN_REPEAT
#define INSN_REPEAT 5
#endif

/* Define in order to get randomization_counter printed, as a comment,
   in the disassembler output, after each insn is emitted.  */
#ifndef OUTPUT_RANDOMIZATION_COUNTER
#define OUTPUT_RANDOMIZATION_COUNTER 0
#endif

/* Other configuration macros are DEFINED_WORD and DEFINED_FUNC_ARG,
   see below.  */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* It is expected that the main program defines the type `word' before
   includeing this.  */
#ifndef DEFINED_WORD
typedef unsigned long long word;
#endif

/* This struct is used as the output area for each function.  It
   should store in as_in a pointer to the string to be output to the
   assembler; in dis_out, the string to be expected in return from the
   disassembler, and in bits the bits of the instruction word that are
   enabled by the assembly fragment.  */
typedef struct
{
  char * as_in;
  char * dis_out;
  word   bits;
} insn_data;

#ifndef DEFINED_FUNC_ARG
/* This is the struct that feeds information to each function.  You're
   free to extend it, by `typedef'ing it before including this file,
   and defining DEFINED_FUNC_ARG.  You may even reorder the fields,
   but do not remove any of the existing fields.  */
typedef struct
{
  int    i1;
  int    i2;
  int    i3;
  void * p1;
  void * p2;
  word   w;
} func_arg;
#endif

/* This is the struct whose arrays define insns.  Each func in the
   array will be called, in sequence, being given a pointer to the
   associated arg and a pointer to a zero-initialized output area,
   that it may fill in.  */
typedef struct
{
  int (*    func) (func_arg *, insn_data *);
  func_arg  arg;
} func;

/* Use this to group insns under a name.  */
typedef struct
{
  const char * name;
  func **      insns;
} group_t;

/* This is the size of each instruction.  Use `insn_size_bits' instead
   of `insn_bits' in an insn defition to modify it.  */
int insn_size = 4;

/* The offset of the next insn, as expected in the disassembler
   output.  */
int current_offset = 0;

/* The offset and name of the last label to be emitted.  */
int last_label_offset = 0;
const char * last_label_name = 0;

/* This variable may be initialized in main() to `argv+1', if
   `argc>1', so that tests are emitted only for instructions that
   match exactly one of the given command-line arguments.  If it is
   NULL, tests for all instructions are emitted.  It must be a
   NULL-terminated array of pointers to strings (just like
   `argv+1').  */
char ** skip_list = 0;

/* This is a counter used to walk the various arrays of ``random''
   operand generation.  In simplified output mode, it is zeroed after
   each insn, otherwise it just keeps growing.  */
unsigned randomization_counter = 0;

/* Use `define_insn' to create an array of funcs to define an insn,
   then `insn' to refer to that insn when defining an insn group.  */
#define define_insn(insname, funcs...) \
  func i_ ## insname[] = { funcs, { 0 } }
#define insn(insname) (i_ ## insname)

/* Use these to output a comma followed by an optional space, a single
   space, a plus sign, left and right square brackets and parentheses,
   all of them properly quoted.  */
#define comma  literal_q (", ", ", ?")
#define space  literal (" ")
#define tab    literal ("\t")
#define plus   literal_q ("+", "\\+")
#define lsqbkt literal_q ("[", "\\[")
#define rsqbkt literal_q ("]", "\\]")
#define lparen literal_q ("(", "\\(")
#define rparen literal_q (")", "\\)")

/* Use this as a placeholder when you define a macro that expects an
   argument, but you don't have anything to output there.  */
int
nothing (func_arg *arg, insn_data *data)
#define nothing { nothing }
{
  return 0;
}

/* This is to be used in the argument list of define_insn, causing a
   string to be copied into both the assembly and the expected
   disassembler output.  It is assumed not to modify the binary
   encoding of the insn.  */
int
literal (func_arg *arg, insn_data *data)
#define literal(s) { literal, { p1: (s) } }
{
  data->as_in = data->dis_out = strdup ((char *) arg->p1);
  return 0;
}

/* The characters `[', `]', `\\' and `^' must be quoted in the
   disassembler-output matcher.  If a literal string contains any of
   these characters, use literal_q instead of literal, and specify the
   unquoted version (for as input) as the first argument, and the
   quoted version (for expected disassembler output) as the second
   one.  */
int
literal_q (func_arg *arg, insn_data *data)
#define literal_q(s,q) { literal_q, { p1: (s), p2: (q) } }
{
  data->as_in = strdup ((char *) arg->p1);
  data->dis_out = strdup ((char *) arg->p2);
  return 0;
}

/* Given an insn name, check whether it should be skipped or not,
   depending on skip_list.  Return non-zero if the insn is to be
   skipped.  */
int
skip_insn (char *name)
{
  char **test;

  if (! skip_list)
    return 0;

  for (test = skip_list; * test; ++ test)
    if (strcmp (name, * test) == 0)
      return 0;

  return 1;
}

/* Use this to emit the actual insn name, with its opcode, in
   architectures with fixed-length instructions.  */
int
insn_bits (func_arg *arg, insn_data *data)
#define insn_bits(name,bits) \
  { insn_bits, { p1: # name, w: bits } }
{
  if (skip_insn ((char *) arg->p1))
    return 1;
  data->as_in = data->dis_out = strdup ((char *) arg->p1);
  data->bits = arg->w;
  return 0;
}

/* Use this to emit the insn name and its opcode in architectures
   without a variable instruction length.  */ 
int
insn_size_bits (func_arg *arg, insn_data *data)
#define insn_size_bits(name,size,bits) \
  { insn_size_bits, { p1: # name, i1: size, w: bits } }
{
  if (skip_insn ((char *) arg->p1))
    return 1;
  data->as_in = data->dis_out = strdup ((char *) arg->p1);
  data->bits = arg->w;
  insn_size = arg->i1;
  return 0;
}

/* Use this to advance the random generator by one, in case it is
   generating repetitive patterns.  It is usually good to arrange that
   each insn consumes a prime number of ``random'' numbers, or, at
   least, that it does not consume an exact power of two ``random''
   numbers.  */
int
tick_random (func_arg *arg, insn_data *data)
#define tick_random { tick_random }
{
  ++ randomization_counter;
  return 0;
}

/* Select the next ``random'' number from the array V of size S, and
   advance the counter.  */
#define get_bits_from_size(V,S) \
  ((V)[randomization_counter ++ % (S)])

/* Utility macros.  `_get_bits_var', used in some macros below, assume
   the names of the arrays used to define the ``random'' orders start
   with `random_order_'.  */
#define _get_bits_var(N) (random_order_ ## N)
#define _get_bits_size(V) (sizeof (V) / sizeof * (V))

/* Use this within a `func_arg' to select one of the arrays below (or
   any other array that starts with random_order_N.  */
#define mk_get_bits(N) \
  p2: _get_bits_var (N), i3: _get_bits_size (_get_bits_var (N))

/* Simplified versions of get_bits_from_size for when you have access
   to the array, so that its size can be implicitly calculated.  */
#define get_bits_from(V) get_bits_from_size ((V),_get_bits_size ((V)))
#define get_bits(N)      get_bits_from (_get_bits_var (N))


/* Use `2u' to generate 2-bit unsigned values.  Good for selecting
   registers randomly from a set of 4 registers.  */
unsigned random_order_2u[] =
  {
    /* This sequence was generated by hand so that no digit appers more
       than once in any horizontal or vertical line.  */
    0, 1, 3, 2,
    2, 0, 1, 3,
    1, 3, 2, 0,
    3, 2, 0, 1
  };

/* Use `3u' to generate 3-bit unsigned values.  Good for selecting
   registers randomly from a set of 8 registers.  */
unsigned random_order_3u[] =
  {
    /* This sequence was generated by:
       f(k) = 3k mod 8
       except that the middle pairs were swapped.  */
    0, 6, 3, 1, 4, 2, 7, 5,
    /* This sequence was generated by:
       f(k) = 5k mod 8
       except that the middle pairs were swapped.  */
    0, 2, 5, 7, 4, 6, 1, 3,
  };

/* Use `4u' to generate 4-bit unsigned values.  Good for selecting
   registers randomly from a set of 16 registers.  */
unsigned random_order_4u[] =
  {
    /* This sequence was generated by:
       f(k) = 5k mod 16
       except that the middle pairs were swapped.  */
    0,  5, 15, 10, 9,  4, 14,  3,
    8, 13,  7,  2, 1, 12,  6, 11,
    /* This sequence was generated by:
       f(k) = 7k mod 16
       except that the middle pairs were swapped.  */
    0,  7,  5, 14,  3, 12, 10, 1,
    8, 15, 13,  6, 11,  4,  2, 9,
  };

/* Use `5u' to generate 5-bit unsigned values.  Good for selecting
   registers randomly from a set of 32 registers.  */
unsigned random_order_5u[] =
  {
    /* This sequence was generated by:
       f(k) = (13k) mod 32
       except that the middle pairs were swapped.  */
    0, 26, 13,  7, 20, 14,  1, 27,
    8, 2,  21, 15, 28, 22,  9,  3,
    16, 10, 29, 23,  4, 30, 17, 11,
    24,  18, 5, 31, 12, 6,  25, 19
  };

/* Use `7s' to generate 7-bit signed values.  Good for selecting
   ``interesting'' constants from -64 to +63.  */
int random_order_7s[] =
  {
    /* Sequence generated by hand, to explore limit values and a few
       intermediate values selected by chance.  Keep the number of
       intermediate values low, to ensure that the limit values are
       generated often enough.  */
    0, -1, -64, 63, -32, 32, 24, -20,
    9, -27, -31, 33, 40, -2, -5, 1
  };

/* Use `8s' to generate 8-bit signed values.  Good for selecting
   ``interesting'' constants from -128 to +127.  */
int random_order_8s[] =
  {
    /* Sequence generated by hand, to explore limit values and a few
       intermediate values selected by chance.  Keep the number of
       intermediate values low, to ensure that the limit values are
       generated often enough.  */
    0, -1, -128, 127, -32, 32, 24, -20,
    73, -27, -95, 33, 104, -2, -69, 1
  };

/* Use `9s' to generate 9-bit signed values.  Good for selecting
   ``interesting'' constants from -256 to +255.  */
int random_order_9s[] =
  {
    /* Sequence generated by hand, to explore limit values and a few
       intermediate values selected by chance.  Keep the number of
       intermediate values low, to ensure that the limit values are
       generated often enough.  */
    0, -1, -256, 255, -64, 64, 72, -40,
    73, -137, -158, 37, 104, -240, -69, 1
  };

/* Use `16s' to generate 16-bit signed values.  Good for selecting
   ``interesting'' constants from -32768 to +32767.  */
int random_order_16s[] =
  {
    /* Sequence generated by hand, to explore limit values and a few
       intermediate values selected by chance.  Keep the number of
       intermediate values low, to ensure that the limit values are
       generated often enough.  */
    -32768,
    32767,
    (-1 << 15) | (64 << 8) | 32,
    (64 << 8) | 32,
    0x1234,
    (-1 << 15) | 0x8765,
    0x0180,
    (-1 << 15) | 0x8001
};

/* Use `24s' to generate 24-bit signed values.  Good for selecting
   ``interesting'' constants from -2^23 to 2^23-1.  */
int random_order_24s[] =
  {
    /* Sequence generated by hand, to explore limit values and a few
       intermediate values selected by chance.  Keep the number of
       intermediate values low, to ensure that the limit values are
       generated often enough.  */
    -1 << 23,
    1 << 23 -1,
    (-1 << 23) | (((64 << 8) | 32) << 8) | 16,
    (((64 << 8) | 32) << 8) | 16,
    0x123456,
    (-1 << 23) | 0x876543,
    0x01ff80,
    (-1 << 23) | 0x80ff01
};

/* Use `32s' to generate 32-bit signed values.  Good for selecting
   ``interesting'' constants from -2^31 to 2^31-1.  */
int random_order_32s[] =
  {
    /* Sequence generated by hand, to explore limit values and a few
       intermediate values selected by chance.  Keep the number of
       intermediate values low, to ensure that the limit values are
       generated often enough.  */
    -1 << 31,
    1 << 31 - 1,
    (-1 << 31) | (((((64 << 8) | 32) << 8) | 16) << 8) | 8,
    (((((64 << 8) | 32) << 8) | 16) << 8) | 8,
    0x12345678,
    (-1 << 31) | 0x87654321,
    0x01ffff80,
    (-1 << 31) | 0x80ffff01
  };

/* This function computes the number of digits needed to represent a
   given number.  */
unsigned long
ulen (unsigned long i, unsigned base)
{
  int count = 0;

  if (i == 0)
    return 1;
  for (; i > 0; ++ count)
    i /= base;
  return count;
}

/* Use this to generate a signed constant of the given size, shifted
   by the given amount, with the specified endianness.  */
int
signed_constant (func_arg * arg, insn_data * data)
#define signed_constant(bits, shift, revert) \
  { signed_constant, { i1: shift, i2: bits * (revert ? -1 : 1), \
		       mk_get_bits (bits ## s) } }
{
  long val = get_bits_from_size ((unsigned *) arg->p2, arg->i3);
  int len = (val >= 0 ? ulen (val, 10) : (1 + ulen (-val, 10)));
  int nbits = (arg->i2 >= 0 ? arg->i2 : -arg->i2);
  word bits = ((word) val) & (((((word) 1) << (nbits - 1)) << 1) - 1);

  data->as_in = data->dis_out = malloc (len + 1);
  sprintf (data->as_in, "%ld", val);
  if (arg->i2 < 0)
    {
      word rbits = 0;

      do
	{
	  rbits <<= 8;
	  rbits |= bits & 0xff;
	  bits >>= 8;
	  nbits -= 8;
	}
      while (nbits > 0);

      bits = rbits;
    }
  data->bits = bits << arg->i1;

  return 0;
}

/* Use this to generate a unsigned constant of the given size, shifted
   by the given amount, with the specified endianness.  */
int
unsigned_constant (func_arg * arg, insn_data * data)
#define unsigned_constant(bits, shift, revert) \
  { unsigned_constant, { i1: shift, i2: bits * (revert ? -1 : 1), \
			 mk_get_bits (bits ## s) } }
{
  int nbits = (arg->i2 >= 0 ? arg->i2 : -arg->i2);
  unsigned long val =
    get_bits_from_size ((unsigned *) arg->p2, arg->i3)
    & (((((word) 1) << (nbits - 1)) << 1) - 1);
  int len = ulen (val, 10);
  word bits = val;

  data->as_in = data->dis_out = malloc (len + 1);
  sprintf (data->as_in, "%lu", val);
  if (arg->i2 < 0)
    {
      word rbits = 0;

      do
	{
	  rbits <<= 8;
	  rbits |= bits & 0xff;
	  bits >>= 8;
	  nbits -= 8;
	}
      while (nbits > 0);

      bits = rbits;
    }
  data->bits = bits << arg->i1;

  return 0;
}

/* Use this to generate an absolute address of the given size, shifted
   by the given amount, with the specified endianness.  */
int
absolute_address (func_arg *arg, insn_data *data)
#define absolute_address (bits, shift, revert) \
  { absolute_address, { i1: shift, i2: bits * (revert ? -1 : 1), \
			mk_get_bits (bits ## s) } }
{
  int nbits = (arg->i2 >= 0 ? arg->i2 : -arg->i2);
  unsigned long val =
    get_bits_from_size ((unsigned *) arg->p2, arg->i3)
    & (((((word) 1) << (nbits - 1)) << 1) - 1);
  word bits = val;

  data->as_in = malloc (ulen (val, 10) + 1);
  sprintf (data->as_in, "%lu", val);
  data->dis_out = malloc (nbits / 4 + 11);
  sprintf (data->dis_out, "0*%0*lx <[^>]*>", nbits / 4, val);
  if (arg->i2 < 0)
    {
      word rbits = 0;

      do
	{
	  rbits <<= 8;
	  rbits |= bits & 0xff;
	  bits >>= 8;
	  nbits -= 8;
	}
      while (nbits > 0);

      bits = rbits;
    }
  data->bits = bits << arg->i1;

  return 0;
}

/* Use this to generate a register name that starts with a given
   prefix, and is followed by a number generated by `gen' (see
   mk_get_bits below).  The register number is shifted `shift' bits
   left before being stored in the binary insn.  */
int
reg_p (func_arg *arg, insn_data *data)
#define reg_p(prefix,shift,gen) \
  { reg_p, { i1: (shift), p1: (prefix), gen } }
{
  unsigned reg = get_bits_from_size ((unsigned *) arg->p2, arg->i3);
  char *regname = (char *) arg->p1;

  data->as_in = data->dis_out = malloc (strlen (regname) + ulen (reg, 10) + 1);
  sprintf (data->as_in, "%s%u", regname, reg);
  data->bits = reg;
  data->bits <<= arg->i1;
  return 0;
}

/* Use this to generate a register name taken from an array.  The
   index into the array `names' is to be produced by `gen', but `mask'
   may be used to filter out some of the bits before choosing the
   disassembler output and the bits for the binary insn, shifted left
   by `shift'.  For example, if registers have canonical names, but
   can also be referred to by aliases, the array can be n times larger
   than the actual number of registers, and the mask is then used to
   pick the canonical name for the disassembler output, and to
   eliminate the extra bits from the binary output.  */
int
reg_r (func_arg *arg, insn_data *data)
#define reg_r(names,shift,mask,gen) \
  { reg_r, { i1: (shift), i2: (mask), p1: (names), gen } }
{
  unsigned reg = get_bits_from_size ((unsigned *) arg->p2, arg->i3);
  
  data->as_in = strdup (((const char **) arg->p1)[reg]);
  reg &= arg->i2;
  data->dis_out = strdup (((const char **) arg->p1)[reg]);
  data->bits = reg;
  data->bits <<= arg->i1;
  return 0;
}

/* Given a NULL-terminated array of insns-definitions (pointers to
   arrays of funcs), output test code for the insns to as_in (assembly
   input) and dis_out (expected disassembler output).  */
void
output_insns (func **insn, FILE *as_in, FILE *dis_out)
{
  for (; *insn; ++insn)
    {
      insn_data *data;
      func *parts = *insn;
      int part_count = 0, r;

      /* Figure out how many funcs have to be called.  */
      while (parts[part_count].func)
	++part_count;

      /* Allocate storage for the output area of each func.  */
      data = (insn_data*) malloc (part_count * sizeof (insn_data));

#if SIMPLIFY_OUTPUT
      randomization_counter = 0;
#else
      /* Repeat each insn several times.  */
      for (r = 0; r < INSN_REPEAT; ++r)
#endif
	{
	  unsigned saved_rc = randomization_counter;
	  int part;
	  word bits = 0;

	  for (part = 0; part < part_count; ++part)
	    {
	      /* Zero-initialize the storage.  */
	      data[part].as_in = data[part].dis_out = 0;
	      data[part].bits = 0;
	      /* If a func returns non-zero, skip this line.  */
	      if (parts[part].func (&parts[part].arg, &data[part]))
		goto skip;
	      /* Otherwise, get its output bit pattern into the total
	         bit pattern.  */
	      bits |= data[part].bits;
	    }
	  
	  if (as_in)
	    {
	      /* Output the whole assembly line.  */
	      fputc ('\t', as_in);
	      for (part = 0; part < part_count; ++part)
		if (data[part].as_in)
		  fputs (data[part].as_in, as_in);
	      fputc ('\n', as_in);
	    }

	  if (dis_out)
	    {
	      /* Output the disassembler expected output line,
	         starting with the offset and the insn binary pattern,
	         just like objdump outputs.  Because objdump sometimes
	         inserts spaces between each byte in the insn binary
	         pattern, make the space optional.  */
	      fprintf (dis_out, "0*%x <", current_offset);
	      if (last_label_name)
		if (current_offset == last_label_offset)
		  fputs (last_label_name, dis_out);
		else
		  fprintf (dis_out, "%s\\+0x%x", last_label_name,
			   current_offset - last_label_offset);
	      else
		fputs ("[^>]*", dis_out);
	      fputs ("> ", dis_out);
	      for (part = insn_size; part-- > 0; )
		fprintf (dis_out, "%02x ?", (int)(bits >> (part * 8)) & 0xff);
	      fputs (" *\t", dis_out);
	      
#if DISASSEMBLER_TEST
	      for (part = 0; part < part_count; ++part)
		if (data[part].dis_out)
		  fputs (data[part].dis_out, dis_out);
#else
	      /* If we're not testing the DISASSEMBLER, just match
	         anything.  */
	      fputs (".*", dis_out);
#endif
	      fputc ('\n', dis_out);
#if OUTPUT_RANDOMIZATION_COUNTER
	      fprintf (dis_out, "# %i\n", randomization_counter);
#endif
	    }

	  /* Account for the insn_size bytes we've just output.  */
	  current_offset += insn_size;

	  /* Release the memory that each func may have allocated.  */
	  for (; part-- > 0;)
	    {
	    skip:
	      if (data[part].as_in)
		free (data[part].as_in);
	      if (data[part].dis_out
		  && data[part].dis_out != data[part].as_in)
		free (data[part].dis_out);
	    }

	  /* There's nothing random here, don't repeat this insn.  */
	  if (randomization_counter == saved_rc)
	    break;
	}

      free (data);
    }
}

/* For each group, output an asm label and the insns of the group.  */
void
output_groups (group_t group[], FILE *as_in, FILE *dis_out)
{
  for (; group->name; ++group)
    {
      fprintf (as_in, "%s:\n", group->name);
      fprintf (dis_out, "# %s:\n", group->name);
      last_label_offset = current_offset;
      last_label_name = group->name;
      output_insns (group->insns, as_in, dis_out);
    }
}

#endif
