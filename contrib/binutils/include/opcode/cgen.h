/* Header file for targets using CGEN: Cpu tools GENerator.

Copyright (C) 1996, 1997 Free Software Foundation, Inc.

This file is part of GDB, the GNU debugger, and the GNU Binutils.

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

#ifndef CGEN_H
#define CGEN_H

#ifndef CGEN_CAT3
#if defined(__STDC__) || defined(ALMOST_STDC)
#define CGEN_XCAT3(a,b,c) a ## b ## c
#define CGEN_CAT3(a,b,c) CGEN_XCAT3 (a, b, c)
#else
#define CGEN_CAT3(a,b,c) a/**/b/**/c
#endif
#endif

/* Prepend the cpu name, defined in cpu-opc.h, and _cgen_ to symbol S.
   The lack of spaces in the arg list is important for non-stdc systems.
   This file is included by <cpu>-opc.h.
   It can be included independently of cpu-opc.h, in which case the cpu
   dependent portions will be declared as "unknown_cgen_foo".  */

#ifndef CGEN_SYM
#define CGEN_SYM(s) CGEN_CAT3 (unknown,_cgen_,s)
#endif

/* This file contains the static (unchanging) pieces and as much other stuff
   as we can reasonably put here.  It's generally cleaner to put stuff here
   rather than having it machine generated if possible.  */

/* The assembler syntax is made up of expressions (duh...).
   At the lowest level the values are mnemonics, register names, numbers, etc.
   Above that are subexpressions, if any (an example might be the
   "effective address" in m68k cpus).  At the second highest level are the
   insns themselves.  Above that are pseudo-insns, synthetic insns, and macros,
   if any.
*/

/* Lots of cpu's have a fixed insn size, or one which rarely changes,
   and it's generally easier to handle these by treating the insn as an
   integer type, rather than an array of characters.  So we allow targets
   to control this.  */

#ifdef CGEN_INT_INSN
typedef unsigned int cgen_insn_t;
#else
typedef char *cgen_insn_t;
#endif

#ifdef __GNUC__
#define CGEN_INLINE inline
#else
#define CGEN_INLINE
#endif

/* Perhaps we should just use bfd.h, but it's not clear
   one would want to require that yet.  */
enum cgen_endian {
  CGEN_ENDIAN_UNKNOWN,
  CGEN_ENDIAN_LITTLE,
  CGEN_ENDIAN_BIG
};

/* Attributes.
   Attributes are used to describe various random things.  */

/* Struct to record attribute information.  */
typedef struct {
  unsigned int num_nonbools;
  unsigned int bool;
  unsigned int nonbool[1];
} CGEN_ATTR;

/* Define a structure member for attributes with N non-boolean entries.
   The attributes are sorted so that the non-boolean ones come first.
   num_nonbools: count of nonboolean attributes
   bool: values of boolean attributes
   nonbool: values of non-boolean attributes
   There is a maximum of 32 attributes total.  */
#define CGEN_ATTR_TYPE(n) \
const struct { unsigned int num_nonbools; \
	       unsigned int bool; \
	       unsigned int nonbool[(n) ? (n) : 1]; }

/* Given an attribute number, return its mask.  */
#define CGEN_ATTR_MASK(attr) (1 << (attr))

/* Return value of attribute ATTR in ATTR_TABLE for OBJ.
   OBJ is a pointer to the entity that has the attributes.
   It's not used at present but is reserved for future purposes.  */
#define CGEN_ATTR_VALUE(obj, attr_table, attr) \
((unsigned int) (attr) < (attr_table)->num_nonbools \
 ? ((attr_table)->nonbool[attr]) \
 : (((attr_table)->bool & (1 << (attr))) != 0))

/* Parse result (also extraction result).

   The result of parsing an insn is stored here.
   To generate the actual insn, this is passed to the insert handler.
   When printing an insn, the result of extraction is stored here.
   To print the insn, this is passed to the print handler.

   It is machine generated so we don't define it here,
   but we do need a forward decl for the handler fns.

   There is one member for each possible field in the insn.
   The type depends on the field.
   Also recorded here is the computed length of the insn for architectures
   where it varies.
*/

struct cgen_fields;

/* Total length of the insn, as recorded in the `fields' struct.  */
/* ??? The field insert handler has lots of opportunities for optimization
   if it ever gets inlined.  On architectures where insns all have the same
   size, may wish to detect that and make this macro a constant - to allow
   further optimizations.  */
#define CGEN_FIELDS_BITSIZE(fields) ((fields)->length)

/* Associated with each insn or expression is a set of "handlers" for
   performing operations like parsing, printing, etc.  */

/* Forward decl.  */
typedef struct cgen_insn CGEN_INSN;

/* Parse handler.
   The first argument is a pointer to a struct describing the insn being
   parsed.
   The second argument is a pointer to a pointer to the text being parsed.
   The third argument is a pointer to a cgen_fields struct
   in which the results are placed.
   If the expression is successfully parsed, the pointer to the text is
   updated.  If not it is left alone.
   The result is NULL if success or an error message.  */
typedef const char * (cgen_parse_fn) PARAMS ((const struct cgen_insn *,
					      const char **,
					      struct cgen_fields *));

/* Print handler.
   The first argument is a pointer to the disassembly info.
   Eg: disassemble_info.  It's defined as `PTR' so this file can be included
   without dis-asm.h.
   The second argument is a pointer to a struct describing the insn being
   printed.
   The third argument is a pointer to a cgen_fields struct.
   The fourth argument is the pc value of the insn.
   The fifth argument is the length of the insn, in bytes.  */
/* Don't require bfd.h unnecessarily.  */
#ifdef BFD_VERSION
typedef void (cgen_print_fn) PARAMS ((PTR, const struct cgen_insn *,
				      struct cgen_fields *, bfd_vma, int));
#else
typedef void (cgen_print_fn) ();
#endif

/* Insert handler.
   The first argument is a pointer to a struct describing the insn being
   parsed.
   The second argument is a pointer to a cgen_fields struct
   from which the values are fetched.
   The third argument is a pointer to a buffer in which to place the insn.  */
typedef void (cgen_insert_fn) PARAMS ((const struct cgen_insn *,
				       struct cgen_fields *, cgen_insn_t *));

/* Extract handler.
   The first argument is a pointer to a struct describing the insn being
   parsed.
   The second argument is a pointer to a struct controlling extraction
   (only used for variable length insns).
   The third argument is the first CGEN_BASE_INSN_SIZE bytes.
   The fourth argument is a pointer to a cgen_fields struct
   in which the results are placed.
   The result is the length of the insn or zero if not recognized.  */
typedef int (cgen_extract_fn) PARAMS ((const struct cgen_insn *,
				       void *, cgen_insn_t,
				       struct cgen_fields *));

/* The `parse' and `insert' fields are indices into these tables.
   The elements are pointer to specialized handler functions.
   Element 0 is special, it means use the default handler.  */
extern cgen_parse_fn * CGEN_SYM (parse_handlers) [];
#define CGEN_PARSE_FN(x) (CGEN_SYM (parse_handlers)[(x)->base.parse])
extern cgen_insert_fn * CGEN_SYM (insert_handlers) [];
#define CGEN_INSERT_FN(x) (CGEN_SYM (insert_handlers)[(x)->base.insert])

/* Likewise for the `extract' and `print' fields.  */
extern cgen_extract_fn * CGEN_SYM (extract_handlers) [];
#define CGEN_EXTRACT_FN(x) (CGEN_SYM (extract_handlers)[(x)->base.extract])
extern cgen_print_fn * CGEN_SYM (print_handlers) [];
#define CGEN_PRINT_FN(x) (CGEN_SYM (print_handlers)[(x)->base.print])

/* Base class of parser/printer.
   (Don't read too much into the use of the phrase "base class").

   Instructions and expressions all share this data in common.
   It's a collection of the common elements needed to parse and print
   each of them.  */

#ifndef CGEN_MAX_INSN_ATTRS
#define CGEN_MAX_INSN_ATTRS 1
#endif

struct cgen_base {
  /* Indices into the handler tables.
     We could use pointers here instead, but in the case of the insn table,
     90% of them would be identical and that's a lot of redundant data.
     0 means use the default (what the default is is up to the code).  */
  unsigned char parse, insert, extract, print;

  /* Attributes.  */
  CGEN_ATTR_TYPE (CGEN_MAX_INSN_ATTRS) attrs;
};

/* Syntax table.

   Each insn and subexpression has one of these.

   The syntax "string" consists of characters (n > 0 && n < 128), and operand
   values (n >= 128), and is terminated by 0.  Operand values are 128 + index
   into the operand table.  The operand table doesn't exist in C, per se, as
   the data is recorded in the parse/insert/extract/print switch statements.

   ??? Whether we want to use yacc instead is unclear, but we do make an
   effort to not make doing that difficult.  At least that's the intent.
*/

struct cgen_syntax {
  /* Original syntax string, for debugging purposes.  */
  char *orig;

  /* Name of entry (that distinguishes it from all other entries).
     This is used, for example, in simulator profiling results.  */
  char *name;

#if 0 /* not needed yet */
  /* Format of this insn.
     This doesn't closely follow the notion of instruction formats for more
     complex instruction sets.  This is the value computed at runtime.  */
  enum cgen_fmt_type fmt;
#endif

  /* Mnemonic (or name if expression).  */
  char *mnemonic;

  /* Syntax string.  */
  /* FIXME: If each insn's mnemonic is constant, do we want to record just
     the arguments here?  */
#ifndef CGEN_MAX_SYNTAX_BYTES
#define CGEN_MAX_SYNTAX_BYTES 16
#endif
  unsigned char syntax[CGEN_MAX_SYNTAX_BYTES];

#define CGEN_SYNTAX_CHAR_P(c) ((c) < 128)
#define CGEN_SYNTAX_CHAR(c) (c)
#define CGEN_SYNTAX_FIELD(c) ((c) - 128)

  /* recognize insn if (op & mask) == value
     For architectures with variable length insns, this is just a preliminary
     test.  */
  /* FIXME: Might want a selectable type (rather than always
     unsigned long).  */
  unsigned long mask, value;

  /* length, in bits
     This is the size that `mask' and `value' have been calculated to.
     Normally it is CGEN_BASE_INSN_BITSIZE.  On vliw architectures where
     the base insn size may be larger than the size of an insn, this field is
     less than CGEN_BASE_INSN_BITSIZE.
     On architectures like the 386 and m68k the real size of the insn may
     be computed while parsing.  */
  /* FIXME: wip, of course */
  int length;
};

/* Operand values (keywords, integers, symbols, etc.)  */

/* Types of assembler elements.  */

enum cgen_asm_type {
  CGEN_ASM_KEYWORD, CGEN_ASM_MAX
};

/* List of hardware elements.  */

typedef struct cgen_hw_entry {
  struct cgen_hw_entry *next;
  char *name;
  enum cgen_asm_type asm_type;
  PTR asm_data;
} CGEN_HW_ENTRY;

extern CGEN_HW_ENTRY *CGEN_SYM (hw_list);

CGEN_HW_ENTRY *cgen_hw_lookup PARAMS ((const char *));

#ifndef CGEN_MAX_KEYWORD_ATTRS
#define CGEN_MAX_KEYWORD_ATTRS 1
#endif

/* This struct is used to describe things like register names, etc.  */

typedef struct cgen_keyword_entry {
  /* Name (as in register name).  */
  char *name;

  /* Value (as in register number).
     The value cannot be -1 as that is used to indicate "not found".
     IDEA: Have "FUNCTION" attribute? [function is called to fetch value].  */
  int value;

  /* Attributes.  */
  /* FIXME: Not used yet.  */
  CGEN_ATTR_TYPE (CGEN_MAX_KEYWORD_ATTRS) attrs;

  /* Next name hash table entry.  */
  struct cgen_keyword_entry *next_name;
  /* Next value hash table entry.  */
  struct cgen_keyword_entry *next_value;
} CGEN_KEYWORD_ENTRY;

/* Top level struct for describing a set of related keywords
   (e.g. register names).

   This struct supports runtime entry of new values, and hashed lookups.  */

typedef struct cgen_keyword {
  /* Pointer to initial [compiled in] values.  */
  struct cgen_keyword_entry *init_entries;
  /* Number of entries in `init_entries'.  */
  unsigned int num_init_entries;
  /* Hash table used for name lookup.  */
  struct cgen_keyword_entry **name_hash_table;
  /* Hash table used for value lookup.  */
  struct cgen_keyword_entry **value_hash_table;
  /* Number of entries in the hash_tables.  */
  unsigned int hash_table_size;
} CGEN_KEYWORD;

/* Structure used for searching.  */

typedef struct cgen_keyword_search {
  /* Table being searched.  */
  const struct cgen_keyword *table;
  /* Specification of what is being searched for.  */
  const char *spec;
  /* Current index in hash table.  */
  unsigned int current_hash;
  /* Current element in current hash chain.  */
  struct cgen_keyword_entry *current_entry;
} CGEN_KEYWORD_SEARCH;

/* Lookup a keyword from its name.  */
const struct cgen_keyword_entry * cgen_keyword_lookup_name
  PARAMS ((struct cgen_keyword *, const char *));
/* Lookup a keyword from its value.  */
const struct cgen_keyword_entry * cgen_keyword_lookup_value
  PARAMS ((struct cgen_keyword *, int));
/* Add a keyword.  */
void cgen_keyword_add PARAMS ((struct cgen_keyword *,
			       struct cgen_keyword_entry *));
/* Keyword searching.
   This can be used to retrieve every keyword, or a subset.  */
struct cgen_keyword_search cgen_keyword_search_init
  PARAMS ((struct cgen_keyword *, const char *));
const struct cgen_keyword_entry *cgen_keyword_search_next
  PARAMS ((struct cgen_keyword_search *));

/* Operand value support routines.  */
/* FIXME: some of the long's here will need to be bfd_vma or some such.  */

const char * cgen_parse_keyword PARAMS ((const char **,
					 struct cgen_keyword *,
					 long *));
const char * cgen_parse_signed_integer PARAMS ((const char **, int,
						long, long, long *));
const char * cgen_parse_unsigned_integer PARAMS ((const char **, int,
						  unsigned long, unsigned long,
						  unsigned long *));
const char * cgen_parse_address PARAMS ((const char **, int,
					 int, long *));
const char * cgen_validate_signed_integer PARAMS ((long, long, long));
const char * cgen_validate_unsigned_integer PARAMS ((unsigned long,
						     unsigned long,
						     unsigned long));

/* This struct defines each entry in the operand table.  */

#ifndef CGEN_MAX_OPERAND_ATTRS
#define CGEN_MAX_OPERAND_ATTRS 1
#endif

typedef struct cgen_operand {
  /* For debugging.  */
  char *name;

  /* Bit position (msb of first byte = bit 0).
     May be unused for a modifier.  */
  unsigned char start;

  /* The number of bits in the operand.
     May be unused for a modifier.  */
  unsigned char length;

  /* Attributes.  */
  CGEN_ATTR_TYPE (CGEN_MAX_OPERAND_ATTRS) attrs;
#define CGEN_OPERAND_ATTRS(operand) (&(operand)->attrs)

#if 0 /* ??? Interesting idea but relocs tend to get too complicated for
	 simple table lookups to work.  */
  /* Ideally this would be the internal (external?) reloc type.  */
  int reloc_type;
#endif
} CGEN_OPERAND;

/* Return value of attribute ATTR in OPERAND.  */
#define CGEN_OPERAND_ATTR(operand, attr) \
CGEN_ATTR_VALUE (operand, CGEN_OPERAND_ATTRS (operand), attr)

/* The operand table is currently a very static entity.  */
extern const CGEN_OPERAND CGEN_SYM (operand_table)[];

enum cgen_operand_type;

#define CGEN_OPERAND_INDEX(operand) ((int) ((operand) - CGEN_SYM (operand_table)))
/* FIXME: Rename, cpu-opc.h defines this as the typedef of the enum.  */
#define CGEN_OPERAND_TYPE(operand) ((enum cgen_operand_type) CGEN_OPERAND_INDEX (operand))
#define CGEN_OPERAND_ENTRY(n) (& CGEN_SYM (operand_table) [n])

/* This struct defines each entry in the instruction table.  */

struct cgen_insn {
  struct cgen_base base;
/* Given a pointer to a cgen_insn struct, return a pointer to `base'.  */
#define CGEN_INSN_BASE(insn) (&(insn)->base)
#define CGEN_INSN_ATTRS(insn) (&(insn)->base.attrs)

  struct cgen_syntax syntax;
#define CGEN_INSN_SYNTAX(insn) (&(insn)->syntax)
#define CGEN_INSN_FMT(insn) ((insn)->syntax.fmt)
#define CGEN_INSN_BITSIZE(insn) ((insn)->syntax.length)
};

/* Return value of attribute ATTR in INSN.  */
#define CGEN_INSN_ATTR(insn, attr) \
CGEN_ATTR_VALUE (insn, CGEN_INSN_ATTRS (insn), attr)

/* Instruction lists.
   This is used for adding new entries and for creating the hash lists.  */

typedef struct cgen_insn_list {
  struct cgen_insn_list *next;
  const struct cgen_insn *insn;
} CGEN_INSN_LIST;

/* The table of instructions.  */

typedef struct cgen_insn_table {
  /* Pointer to initial [compiled in] entries.  */
  const struct cgen_insn *init_entries;
  /* Number of entries in `init_entries', including trailing NULL entry.  */
  unsigned int num_init_entries;
  /* Values added at runtime.  */
  struct cgen_insn_list *new_entries;
  /* Assembler hash function.  */
  unsigned int (*asm_hash) PARAMS ((const char *));
  /* Number of entries in assembler hash table.  */
  unsigned int asm_hash_table_size;
  /* Disassembler hash function.  */
  unsigned int (*dis_hash) PARAMS ((const char *, unsigned long));
  /* Number of entries in disassembler hash table.  */
  unsigned int dis_hash_table_size;
} CGEN_INSN_TABLE;

/* ??? This is currently used by the simulator.
   We want this to be fast and the simulator currently doesn't handle
   runtime added instructions so this is ok.  An alternative would be to
   store the index in the table.  */
extern const CGEN_INSN CGEN_SYM (insn_table_entries)[];
#define CGEN_INSN_INDEX(insn) ((int) ((insn) - CGEN_SYM (insn_table_entries)))
#define CGEN_INSN_ENTRY(n) (& CGEN_SYM (insn_table_entries) [n])

/* Return number of instructions.  This includes any added at runtime.  */

int cgen_insn_count PARAMS (());

/* The assembler insn table is hashed based on some function of the mnemonic
   (the actually hashing done is up to the target, but we provide a few
   examples like the first letter or a function of the entire mnemonic).
   The index of each entry is the index of the corresponding table entry.
   The value of each entry is the index of the next entry, with a 0
   terminating (thus the first entry is reserved).  */

#ifndef CGEN_ASM_HASH
#ifdef CGEN_MNEMONIC_OPERANDS
#define CGEN_ASM_HASH_SIZE 127
#define CGEN_ASM_HASH(string) (*(unsigned char *) (string) % CGEN_ASM_HASH_SIZE)
#else
#define CGEN_ASM_HASH_SIZE 128
#define CGEN_ASM_HASH(string) (*(unsigned char *) (string) % CGEN_ASM_HASH_SIZE) /*FIXME*/
#endif
#endif

unsigned int CGEN_SYM (asm_hash_insn) PARAMS ((const char *));
CGEN_INSN_LIST * cgen_asm_lookup_insn PARAMS ((const char *));
#define CGEN_ASM_LOOKUP_INSN(insn) cgen_asm_lookup_insn (insn)
#define CGEN_ASM_NEXT_INSN(insn) ((insn)->next)

/* The disassembler insn table is hashed based on some function of machine
   instruction (the actually hashing done is up to the target).  */

/* It doesn't make much sense to provide a default here,
   but while this is under development we do.
   BUFFER is a pointer to the bytes of the insn.
   INSN is the first CGEN_BASE_INSN_SIZE bytes as an int in host order.  */
#ifndef CGEN_DIS_HASH
#define CGEN_DIS_HASH_SIZE 256
#define CGEN_DIS_HASH(buffer, insn) (*(unsigned char *) (buffer))
#endif

unsigned int CGEN_SYM (dis_hash_insn) PARAMS ((const char *, unsigned long));
CGEN_INSN_LIST * cgen_dis_lookup_insn PARAMS ((const char *, unsigned long));
#define CGEN_DIS_LOOKUP_INSN(buf, insn) cgen_dis_lookup_insn (buf, insn)
#define CGEN_DIS_NEXT_INSN(insn) ((insn)->next)

/* Top level structures and functions.  */

typedef struct cgen_opcode_data {
  CGEN_HW_ENTRY *hw_list;
  /*CGEN_OPERAND_TABLE *operand_table; - FIXME:wip */
  CGEN_INSN_TABLE *insn_table;
} CGEN_OPCODE_DATA;

/* Each CPU has one of these.  */
extern CGEN_OPCODE_DATA CGEN_SYM (opcode_data);

/* Global state access macros.
   Some of these are tucked away and accessed with cover fns.
   Simpler things like the current machine and endian are not.  */

extern int cgen_current_machine;
#define CGEN_CURRENT_MACHINE cgen_current_machine

extern enum cgen_endian cgen_current_endian;
#define CGEN_CURRENT_ENDIAN cgen_current_endian

/* Prototypes of major functions.  */

/* Set the current cpu (+ mach number, endian, etc.).  *?
void cgen_set_cpu PARAMS ((CGEN_OPCODE_DATA *, int, enum cgen_endian));

/* Initialize the assembler, disassembler.  */
void cgen_asm_init PARAMS ((void));
void cgen_dis_init PARAMS ((void));

/* `init_tables' must be called before `xxx_supported'.  */
void CGEN_SYM (init_tables) PARAMS ((int));
void CGEN_SYM (init_asm) PARAMS ((int, enum cgen_endian));
void CGEN_SYM (init_dis) PARAMS ((int, enum cgen_endian));
void CGEN_SYM (init_parse) PARAMS ((void));
void CGEN_SYM (init_print) PARAMS ((void));
void CGEN_SYM (init_insert) PARAMS ((void));
void CGEN_SYM (init_extract) PARAMS ((void));
const struct cgen_insn *
CGEN_SYM (assemble_insn) PARAMS ((const char *, struct cgen_fields *,
				  cgen_insn_t *, char **));
int CGEN_SYM (insn_supported) PARAMS ((const struct cgen_syntax *));
#if 0 /* old */
int CGEN_SYM (opval_supported) PARAMS ((const struct cgen_opval *));
#endif

extern const struct cgen_keyword  CGEN_SYM (operand_mach);
int CGEN_SYM (get_mach) PARAMS ((const char *));

CGEN_INLINE void
CGEN_SYM (put_operand) PARAMS ((int, const long *,
				struct cgen_fields *));
CGEN_INLINE long
CGEN_SYM (get_operand) PARAMS ((int, const struct cgen_fields *));

CGEN_INLINE const char *
CGEN_SYM (parse_operand) PARAMS ((int, const char **, struct cgen_fields *));

CGEN_INLINE const char *
CGEN_SYM (validate_operand) PARAMS ((int, const struct cgen_fields *));

/* Default insn parser, printer.  */
extern cgen_parse_fn CGEN_SYM (parse_insn);
extern cgen_insert_fn CGEN_SYM (insert_insn);
extern cgen_extract_fn CGEN_SYM (extract_insn);
extern cgen_print_fn CGEN_SYM (print_insn);

/* Read in a cpu description file.  */
const char * cgen_read_cpu_file PARAMS ((const char *));

/* Assembler interface.

   The interface to the assembler is intended to be clean in the sense that
   libopcodes.a is a standalone entity and could be used with any assembler.
   Not that one would necessarily want to do that but rather that it helps
   keep a clean interface.  The interface will obviously be slanted towards
   GAS, but at least it's a start.

   Parsing is controlled by the assembler which calls
   CGEN_SYM (assemble_insn).  If it can parse and build the entire insn
   it doesn't call back to the assembler.  If it needs/wants to call back
   to the assembler, (*cgen_parse_operand_fn) is called which can either

   - return a number to be inserted in the insn
   - return a "register" value to be inserted
     (the register might not be a register per pe)
   - queue the argument and return a marker saying the expression has been
     queued (eg: a fix-up)
   - return an error message indicating the expression wasn't recognizable

   The result is an error message or NULL for success.
   The parsed value is stored in the bfd_vma *.  */

/* Values for indicating what the caller wants.  */
enum cgen_parse_operand_type {
  CGEN_PARSE_OPERAND_INIT, CGEN_PARSE_OPERAND_INTEGER,
  CGEN_PARSE_OPERAND_ADDRESS
};

/* Values for indicating what was parsed.
   ??? Not too useful at present but in time.  */
enum cgen_parse_operand_result {
  CGEN_PARSE_OPERAND_RESULT_NUMBER, CGEN_PARSE_OPERAND_RESULT_REGISTER,
  CGEN_PARSE_OPERAND_RESULT_QUEUED, CGEN_PARSE_OPERAND_RESULT_ERROR
};

/* Don't require bfd.h unnecessarily.  */
#ifdef BFD_VERSION
extern const char * (*cgen_parse_operand_fn)
     PARAMS ((enum cgen_parse_operand_type, const char **, int, int,
	      enum cgen_parse_operand_result *, bfd_vma *));
#endif

/* Called before trying to match a table entry with the insn.  */
void cgen_init_parse_operand PARAMS ((void));

/* Called from <cpu>-asm.c to initialize operand parsing.  */

/* These are GAS specific.  They're not here as part of the interface,
   but rather that we need to put them somewhere.  */

/* Call this from md_assemble to initialize the assembler callback.  */
void cgen_asm_init_parse PARAMS ((void));

/* Don't require bfd.h unnecessarily.  */
#ifdef BFD_VERSION
/* The result is an error message or NULL for success.
   The parsed value is stored in the bfd_vma *.  */
const char *cgen_parse_operand PARAMS ((enum cgen_parse_operand_type,
					const char **, int, int,
					enum cgen_parse_operand_result *,
					bfd_vma *));
#endif

/* Add a register to the assembler's hash table.
   This makes lets GAS parse registers for us.
   ??? This isn't currently used, but it could be in the future.  */
void cgen_asm_record_register PARAMS ((char *, int));

/* After CGEN_SYM (assemble_insn) is done, this is called to
   output the insn and record any fixups.  */
void cgen_asm_finish_insn PARAMS ((const struct cgen_insn *, cgen_insn_t *,
				   unsigned int));

#endif /* CGEN_H */
