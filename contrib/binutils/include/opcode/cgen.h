/* Header file for targets using CGEN: Cpu tools GENerator.

Copyright (C) 1996, 1997, 1998 Free Software Foundation, Inc.

This file is part of GDB, the GNU debugger, and the GNU Binutils.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef CGEN_H
#define CGEN_H

/* Prepend the cpu name, defined in cpu-opc.h, and _cgen_ to symbol S.
   The lack of spaces in the arg list is important for non-stdc systems.
   This file is included by <cpu>-opc.h.
   It can be included independently of cpu-opc.h, in which case the cpu
   dependent portions will be declared as "unknown_cgen_foo".  */

#ifndef CGEN_SYM
#define CGEN_SYM(s) CONCAT3 (unknown,_cgen_,s)
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
typedef char * cgen_insn_t;
#endif

#ifdef __GNUC__
#define CGEN_INLINE inline
#else
#define CGEN_INLINE
#endif

/* Perhaps we should just use bfd.h, but it's not clear
   one would want to require that yet.  */
enum cgen_endian
{
  CGEN_ENDIAN_UNKNOWN,
  CGEN_ENDIAN_LITTLE,
  CGEN_ENDIAN_BIG
};

/* Attributes.
   Attributes are used to describe various random things.  */

/* Struct to record attribute information.  */
typedef struct
{
  unsigned char num_nonbools;
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
const struct { unsigned char num_nonbools; \
	       unsigned int bool; \
	       unsigned int nonbool[(n) ? (n) : 1]; }

/* Given an attribute number, return its mask.  */
#define CGEN_ATTR_MASK(attr) (1 << (attr))

/* Return the value of boolean attribute ATTR in ATTRS.  */
#define CGEN_BOOL_ATTR(attrs, attr) \
((CGEN_ATTR_MASK (attr) & (attrs)) != 0)

/* Return value of attribute ATTR in ATTR_TABLE for OBJ.
   OBJ is a pointer to the entity that has the attributes.
   It's not used at present but is reserved for future purposes.  */
#define CGEN_ATTR_VALUE(obj, attr_table, attr) \
((unsigned int) (attr) < (attr_table)->num_nonbools \
 ? ((attr_table)->nonbool[attr]) \
 : (((attr_table)->bool & (1 << (attr))) != 0))

/* Attribute name/value tables.
   These are used to assist parsing of descriptions at runtime.  */

typedef struct
{
  const char * name;
  int          value;
} CGEN_ATTR_ENTRY;

/* For each domain (fld,operand,insn), list of attributes.  */

typedef struct
{
  const char *            name;
  /* NULL for boolean attributes.  */
  const CGEN_ATTR_ENTRY * vals;
} CGEN_ATTR_TABLE;

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

typedef struct cgen_fields CGEN_FIELDS;

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
					      CGEN_FIELDS *));

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
				      CGEN_FIELDS *, bfd_vma, int));
#else
typedef void (cgen_print_fn) ();
#endif

/* Insert handler.
   The first argument is a pointer to a struct describing the insn being
   parsed.
   The second argument is a pointer to a cgen_fields struct
   from which the values are fetched.
   The third argument is a pointer to a buffer in which to place the insn.
   The result is an error message or NULL if success.  */
typedef const char * (cgen_insert_fn) PARAMS ((const struct cgen_insn *,
					       CGEN_FIELDS *, cgen_insn_t *));

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
				       CGEN_FIELDS *));

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
   (Don't read too much into the use of the phrase "base class".
   It's a name I'm using to organize my thoughts.)

   Instructions and expressions all share this data in common.
   It's a collection of the common elements needed to parse, insert, extract,
   and print each of them.  */

struct cgen_base
{
  /* Indices into the handler tables.
     We could use pointers here instead, but in the case of the insn table,
     90% of them would be identical and that's a lot of redundant data.
     0 means use the default (what the default is is up to the code).  */
  unsigned char parse, insert, extract, print;
};

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
enum cgen_parse_operand_type
{
  CGEN_PARSE_OPERAND_INIT,
  CGEN_PARSE_OPERAND_INTEGER,
  CGEN_PARSE_OPERAND_ADDRESS
};

/* Values for indicating what was parsed.
   ??? Not too useful at present but in time.  */
enum cgen_parse_operand_result
{
  CGEN_PARSE_OPERAND_RESULT_NUMBER,
  CGEN_PARSE_OPERAND_RESULT_REGISTER,
  CGEN_PARSE_OPERAND_RESULT_QUEUED,
  CGEN_PARSE_OPERAND_RESULT_ERROR
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
const char * cgen_parse_operand PARAMS ((enum cgen_parse_operand_type,
					 const char **, int, int,
					 enum cgen_parse_operand_result *,
					 bfd_vma *));
#endif

void cgen_save_fixups PARAMS ((void));
void cgen_restore_fixups PARAMS ((void));
void cgen_swap_fixups PARAMS ((void));
     
/* Add a register to the assembler's hash table.
   This makes lets GAS parse registers for us.
   ??? This isn't currently used, but it could be in the future.  */
void cgen_asm_record_register PARAMS ((char *, int));

/* After CGEN_SYM (assemble_insn) is done, this is called to
   output the insn and record any fixups.  The address of the
   assembled instruction is returned in case it is needed by
   the caller.  */
char * cgen_asm_finish_insn PARAMS ((const struct cgen_insn *, cgen_insn_t *,
				   unsigned int));

/* Operand values (keywords, integers, symbols, etc.)  */

/* Types of assembler elements.  */

enum cgen_asm_type
{
  CGEN_ASM_KEYWORD, CGEN_ASM_MAX
};

/* List of hardware elements.  */

typedef struct cgen_hw_entry
{
  /* The type of this entry, one of `enum hw_type'.
     This is an int and not the enum as the latter may not be declared yet.  */
  int                          type;
  const struct cgen_hw_entry * next;
  char *                       name;
  enum cgen_asm_type           asm_type;
  PTR                          asm_data;
} CGEN_HW_ENTRY;

const CGEN_HW_ENTRY * cgen_hw_lookup PARAMS ((const char *));

/* This struct is used to describe things like register names, etc.  */

typedef struct cgen_keyword_entry
{
  /* Name (as in register name).  */
  char * name;

  /* Value (as in register number).
     The value cannot be -1 as that is used to indicate "not found".
     IDEA: Have "FUNCTION" attribute? [function is called to fetch value].  */
  int value;

  /* Attributes.
     This should, but technically needn't, appear last.  It is a variable sized
     array in that one architecture may have 1 nonbool attribute and another
     may have more.  Having this last means the non-architecture specific code
     needn't care.  */
  /* ??? Moving this last should be done by treating keywords like insn lists
     and moving the `next' fields into a CGEN_KEYWORD_LIST struct.  */
  /* FIXME: Not used yet.  */
#ifndef CGEN_KEYWORD_NBOOL_ATTRS
#define CGEN_KEYWORD_NBOOL_ATTRS 1
#endif
  CGEN_ATTR_TYPE (CGEN_KEYWORD_NBOOL_ATTRS) attrs;

  /* Next name hash table entry.  */
  struct cgen_keyword_entry *next_name;
  /* Next value hash table entry.  */
  struct cgen_keyword_entry *next_value;
} CGEN_KEYWORD_ENTRY;

/* Top level struct for describing a set of related keywords
   (e.g. register names).

   This struct supports runtime entry of new values, and hashed lookups.  */

typedef struct cgen_keyword
{
  /* Pointer to initial [compiled in] values.  */
  CGEN_KEYWORD_ENTRY * init_entries;
  
  /* Number of entries in `init_entries'.  */
  unsigned int num_init_entries;
  
  /* Hash table used for name lookup.  */
  CGEN_KEYWORD_ENTRY ** name_hash_table;
  
  /* Hash table used for value lookup.  */
  CGEN_KEYWORD_ENTRY ** value_hash_table;
  
  /* Number of entries in the hash_tables.  */
  unsigned int hash_table_size;
  
  /* Pointer to null keyword "" entry if present.  */
  const CGEN_KEYWORD_ENTRY * null_entry;
} CGEN_KEYWORD;

/* Structure used for searching.  */

typedef struct
{
  /* Table being searched.  */
  const CGEN_KEYWORD * table;
  
  /* Specification of what is being searched for.  */
  const char * spec;
  
  /* Current index in hash table.  */
  unsigned int current_hash;
  
  /* Current element in current hash chain.  */
  CGEN_KEYWORD_ENTRY * current_entry;
} CGEN_KEYWORD_SEARCH;

/* Lookup a keyword from its name.  */
const CGEN_KEYWORD_ENTRY * cgen_keyword_lookup_name
  PARAMS ((CGEN_KEYWORD *, const char *));
/* Lookup a keyword from its value.  */
const CGEN_KEYWORD_ENTRY * cgen_keyword_lookup_value
  PARAMS ((CGEN_KEYWORD *, int));
/* Add a keyword.  */
void cgen_keyword_add PARAMS ((CGEN_KEYWORD *, CGEN_KEYWORD_ENTRY *));
/* Keyword searching.
   This can be used to retrieve every keyword, or a subset.  */
CGEN_KEYWORD_SEARCH cgen_keyword_search_init
  PARAMS ((CGEN_KEYWORD *, const char *));
const CGEN_KEYWORD_ENTRY *cgen_keyword_search_next
  PARAMS ((CGEN_KEYWORD_SEARCH *));

/* Operand value support routines.  */
/* FIXME: some of the long's here will need to be bfd_vma or some such.  */

const char * cgen_parse_keyword PARAMS ((const char **,
					 CGEN_KEYWORD *,
					 long *));
const char * cgen_parse_signed_integer PARAMS ((const char **, int, long *));
const char * cgen_parse_unsigned_integer PARAMS ((const char **, int,
						  unsigned long *));
const char * cgen_parse_address PARAMS ((const char **, int, int,
					 enum cgen_parse_operand_result *,
					 long *));
const char * cgen_validate_signed_integer PARAMS ((long, long, long));
const char * cgen_validate_unsigned_integer PARAMS ((unsigned long,
						     unsigned long,
						     unsigned long));

/* Operand modes.  */

/* ??? This duplicates the values in arch.h.  Revisit.
   These however need the CGEN_ prefix [as does everything in this file].  */
/* ??? Targets may need to add their own modes so we may wish to move this
   to <arch>-opc.h, or add a hook.  */

enum cgen_mode {
  CGEN_MODE_VOID, /* FIXME: rename simulator's VM to VOID */
  CGEN_MODE_BI, CGEN_MODE_QI, CGEN_MODE_HI, CGEN_MODE_SI, CGEN_MODE_DI,
  CGEN_MODE_UBI, CGEN_MODE_UQI, CGEN_MODE_UHI, CGEN_MODE_USI, CGEN_MODE_UDI,
  CGEN_MODE_SF, CGEN_MODE_DF, CGEN_MODE_XF, CGEN_MODE_TF,
  CGEN_MODE_MAX
};

/* FIXME: Until simulator is updated.  */
#define CGEN_MODE_VM CGEN_MODE_VOID

/* This struct defines each entry in the operand table.  */

typedef struct cgen_operand
{
  /* Name as it appears in the syntax string.  */
  char * name;

  /* The hardware element associated with this operand.  */
  const CGEN_HW_ENTRY *hw;

  /* FIXME: We don't yet record ifield definitions, which we should.
     When we do it might make sense to delete start/length (since they will
     be duplicated in the ifield's definition) and replace them with a
     pointer to the ifield entry.  Note that as more complicated situations
     need to be handled, going more and more with an OOP paradigm will help
     keep the complication under control.  Of course, this was the goal from
     the start, but getting there in one step was too much too soon.  */

  /* Bit position (msb of first byte = bit 0).
     This is just a hint, and may be unused in more complex operands.
     May be unused for a modifier.  */
  unsigned char start;

  /* The number of bits in the operand.
     This is just a hint, and may be unused in more complex operands.
     May be unused for a modifier.  */
  unsigned char length;

#if 0 /* ??? Interesting idea but relocs tend to get too complicated,
	 and ABI dependent, for simple table lookups to work.  */
  /* Ideally this would be the internal (external?) reloc type.  */
  int reloc_type;
#endif

  /* Attributes.
     This should, but technically needn't, appear last.  It is a variable sized
     array in that one architecture may have 1 nonbool attribute and another
     may have more.  Having this last means the non-architecture specific code
     needn't care, now or tomorrow.  */
#ifndef CGEN_OPERAND_NBOOL_ATTRS
#define CGEN_OPERAND_NBOOL_ATTRS 1
#endif
  CGEN_ATTR_TYPE (CGEN_OPERAND_NBOOL_ATTRS) attrs;
#define CGEN_OPERAND_ATTRS(operand) (&(operand)->attrs)
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

/* Types of parse/insert/extract/print cover-fn handlers.  */
/* FIXME: move opindex first to match caller.  */
/* FIXME: also need types of insert/extract/print fns.  */
/* FIXME: not currently used as type of 3rd arg varies.  */
typedef const char * (CGEN_PARSE_OPERAND_FN) PARAMS ((const char **, int,
						      long *));

/* Instruction operand instances.

   For each instruction, a list of the hardware elements that are read and
   written are recorded.  */

/* The type of the instance.  */
enum cgen_operand_instance_type {
  /* End of table marker.  */
  CGEN_OPERAND_INSTANCE_END = 0,
  CGEN_OPERAND_INSTANCE_INPUT, CGEN_OPERAND_INSTANCE_OUTPUT
};

typedef struct
{
  /* The type of this operand.  */
  enum cgen_operand_instance_type type;
#define CGEN_OPERAND_INSTANCE_TYPE(opinst) ((opinst)->type)

  /* The hardware element referenced.  */
  const CGEN_HW_ENTRY *hw;
#define CGEN_OPERAND_INSTANCE_HW(opinst) ((opinst)->hw)

  /* The mode in which the operand is being used.  */
  enum cgen_mode mode;
#define CGEN_OPERAND_INSTANCE_MODE(opinst) ((opinst)->mode)

  /* The operand table entry or NULL if there is none (i.e. an explicit
     hardware reference).  */
  const CGEN_OPERAND *operand;
#define CGEN_OPERAND_INSTANCE_OPERAND(opinst) ((opinst)->operand)

  /* If `operand' is NULL, the index (e.g. into array of registers).  */
  int index;
#define CGEN_OPERAND_INSTANCE_INDEX(opinst) ((opinst)->index)
} CGEN_OPERAND_INSTANCE;

/* Syntax string.

   Each insn format and subexpression has one of these.

   The syntax "string" consists of characters (n > 0 && n < 128), and operand
   values (n >= 128), and is terminated by 0.  Operand values are 128 + index
   into the operand table.  The operand table doesn't exist in C, per se, as
   the data is recorded in the parse/insert/extract/print switch statements. */

#ifndef CGEN_MAX_SYNTAX_BYTES
#define CGEN_MAX_SYNTAX_BYTES 16
#endif

typedef struct
{
  unsigned char syntax[CGEN_MAX_SYNTAX_BYTES];
} CGEN_SYNTAX;

#define CGEN_SYNTAX_STRING(syn) (syn->syntax)
#define CGEN_SYNTAX_CHAR_P(c) ((c) < 128)
#define CGEN_SYNTAX_CHAR(c) (c)
#define CGEN_SYNTAX_FIELD(c) ((c) - 128)
#define CGEN_SYNTAX_MAKE_FIELD(c) ((c) + 128)

/* ??? I can't currently think of any case where the mnemonic doesn't come
   first [and if one ever doesn't building the hash tables will be tricky].
   However, we treat mnemonics as just another operand of the instruction.
   A value of 1 means "this is where the mnemonic appears".  1 isn't
   special other than it's a non-printable ASCII char.  */
#define CGEN_SYNTAX_MNEMONIC       1
#define CGEN_SYNTAX_MNEMONIC_P(ch) ((ch) == CGEN_SYNTAX_MNEMONIC)

/* Instruction formats.

   Instructions are grouped by format.  Associated with an instruction is its
   format.  Each opcode table entry contains a format table entry.
   ??? There is usually very few formats compared with the number of insns,
   so one can reduce the size of the opcode table by recording the format table
   as a separate entity.  Given that we currently don't, format table entries
   are also distinguished by their operands.  This increases the size of the
   table, but reduces the number of tables.  It's all minutiae anyway so it
   doesn't really matter [at this point in time].

   ??? Support for variable length ISA's is wip.  */

typedef struct
{
  /* Length that MASK and VALUE have been calculated to
     [VALUE is recorded elsewhere].
     Normally it is CGEN_BASE_INSN_BITSIZE.  On [V]LIW architectures where
     the base insn size may be larger than the size of an insn, this field is
     less than CGEN_BASE_INSN_BITSIZE.  */
  unsigned char mask_length;

  /* Total length of instruction, in bits.  */
  unsigned char length;

  /* Mask to apply to the first MASK_LENGTH bits.
     Each insn's value is stored with the insn.
     The first step in recognizing an insn for disassembly is
     (opcode & mask) == value.  */
  unsigned int mask;
} CGEN_FORMAT;

/* This struct defines each entry in the instruction table.  */

struct cgen_insn
{
  /* ??? Further table size reductions can be had by moving this element
     either to the format table or to a separate table of its own.  Not
     sure this is desirable yet.  */
  struct cgen_base base;
  
/* Given a pointer to a cgen_insn struct, return a pointer to `base'.  */
#define CGEN_INSN_BASE(insn) (&(insn)->base)

  /* Name of entry (that distinguishes it from all other entries).
     This is used, for example, in simulator profiling results.  */
  /* ??? If mnemonics have operands, try to print full mnemonic.  */
  const char * name;
#define CGEN_INSN_NAME(insn) ((insn)->name)

  /* Mnemonic.  This is used when parsing and printing the insn.
     In the case of insns that have operands on the mnemonics, this is
     only the constant part.  E.g. for conditional execution of an `add' insn,
     where the full mnemonic is addeq, addne, etc., this is only "add".  */
  const char * mnemonic;
#define CGEN_INSN_MNEMONIC(insn) ((insn)->mnemonic)

  /* Syntax string.  */
  const CGEN_SYNTAX syntax;
#define CGEN_INSN_SYNTAX(insn) (& (insn)->syntax)

  /* Format entry.  */
  const CGEN_FORMAT format;
#define CGEN_INSN_MASK_BITSIZE(insn) ((insn)->format.mask_length)
#define CGEN_INSN_BITSIZE(insn) ((insn)->format.length)

  /* Instruction opcode value.  */
  unsigned int value;
#define CGEN_INSN_VALUE(insn) ((insn)->value)
#define CGEN_INSN_MASK(insn) ((insn)->format.mask)

  /* Pointer to NULL entry terminated table of operands used,
     or NULL if none.  */
  const CGEN_OPERAND_INSTANCE *operands;
#define CGEN_INSN_OPERANDS(insn) ((insn)->operands)

  /* Attributes.
     This must appear last.  It is a variable sized array in that one
     architecture may have 1 nonbool attribute and another may have more.
     Having this last means the non-architecture specific code needn't
     care.  */
#ifndef CGEN_INSN_NBOOL_ATTRS
#define CGEN_INSN_NBOOL_ATTRS 1
#endif
  CGEN_ATTR_TYPE (CGEN_INSN_NBOOL_ATTRS) attrs;
#define CGEN_INSN_ATTRS(insn) (&(insn)->attrs)
/* Return value of attribute ATTR in INSN.  */
#define CGEN_INSN_ATTR(insn, attr) \
CGEN_ATTR_VALUE (insn, CGEN_INSN_ATTRS (insn), attr)
};

/* Instruction lists.
   This is used for adding new entries and for creating the hash lists.  */

typedef struct cgen_insn_list
{
  struct cgen_insn_list * next;
  const CGEN_INSN * insn;
} CGEN_INSN_LIST;

/* The table of instructions.  */

typedef struct
{
  /* Pointer to initial [compiled in] entries.  */
  const CGEN_INSN * init_entries;
  
  /* Size of an entry (since the attribute member is variable sized).  */
  unsigned int entry_size;
  
  /* Number of entries in `init_entries', including trailing NULL entry.  */
  unsigned int num_init_entries;
  
  /* Values added at runtime.  */
  CGEN_INSN_LIST * new_entries;
  
  /* Assembler hash function.  */
  unsigned int (* asm_hash) PARAMS ((const char *));
  
  /* Number of entries in assembler hash table.  */
  unsigned int asm_hash_table_size;
  
  /* Disassembler hash function.  */
  unsigned int (* dis_hash) PARAMS ((const char *, unsigned long));
  
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

int cgen_insn_count PARAMS ((void));

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

typedef struct
{
  const CGEN_HW_ENTRY *  hw_list;
  /*CGEN_OPERAND_TABLE * operand_table; - FIXME:wip */
  CGEN_INSN_TABLE *      insn_table;
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

/* Set the current cpu (+ mach number, endian, etc.).  */
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

/* FIXME: This prototype is wrong ifndef CGEN_INT_INSN.
   Furthermore, ifdef CGEN_INT_INSN, the insn is created in
   target byte order (in which case why use int's at all).
   Perhaps replace cgen_insn_t * with char *?  */
const struct cgen_insn *
CGEN_SYM (assemble_insn) PARAMS ((const char *, CGEN_FIELDS *,
				  cgen_insn_t *, char **));
#if 0 /* old */
int CGEN_SYM (insn_supported) PARAMS ((const struct cgen_insn *));
int CGEN_SYM (opval_supported) PARAMS ((const struct cgen_opval *));
#endif

extern const CGEN_KEYWORD  CGEN_SYM (operand_mach);
int CGEN_SYM (get_mach) PARAMS ((const char *));

const CGEN_INSN *
CGEN_SYM (get_insn_operands) PARAMS ((const CGEN_INSN *, cgen_insn_t,
				      int, int *));
const CGEN_INSN *
CGEN_SYM (lookup_insn) PARAMS ((const CGEN_INSN *, cgen_insn_t,
				int, CGEN_FIELDS *, int));

CGEN_INLINE void
CGEN_SYM (put_operand) PARAMS ((int, const long *,
				CGEN_FIELDS *));
CGEN_INLINE long
CGEN_SYM (get_operand) PARAMS ((int, const CGEN_FIELDS *));

const char *
CGEN_SYM (parse_operand) PARAMS ((int, const char **, CGEN_FIELDS *));

const char *
CGEN_SYM (insert_operand) PARAMS ((int, CGEN_FIELDS *, char *));

/* Default insn parser, printer.  */
extern cgen_parse_fn CGEN_SYM (parse_insn);
extern cgen_insert_fn CGEN_SYM (insert_insn);
extern cgen_extract_fn CGEN_SYM (extract_insn);
extern cgen_print_fn CGEN_SYM (print_insn);

/* Read in a cpu description file.  */
const char * cgen_read_cpu_file PARAMS ((const char *));

#endif /* CGEN_H */
