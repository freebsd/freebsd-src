/*-
 * This code is derived from software copyrighted by the Free Software
 * Foundation.
 *
 * Modified 1991 by Donn Seeley at UUNET Technologies, Inc.
 * Modified 1990 by Van Jacobson at Lawrence Berkeley Laboratory.
 *
 *	@(#)symseg.h	6.3 (Berkeley) 5/8/91
 */

/* GDB symbol table format definitions.
   Copyright (C) 1986, 1989 Free Software Foundation, Inc.
   Hacked by Michael Tiemann (tiemann@mcc.com)

This file is part of GDB.

GDB is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

GDB is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GDB; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* Format of GDB symbol table data.
   There is one symbol segment for each source file or
   independant compilation.  These segments are simply concatenated
   to form the GDB symbol table.  A zero word where the beginning
   of a segment is expected indicates there are no more segments.

Format of a symbol segment:

   The symbol segment begins with a word containing 1
   if it is in the format described here.  Other formats may
   be designed, with other code numbers.

   The segment contains many objects which point at each other.
   The pointers are offsets in bytes from the beginning of the segment.
   Thus, each segment can be loaded into core and its pointers relocated
   to make valid in-core pointers.

   All the data objects in the segment can be found indirectly from
   one of them, the root object, of type `struct symbol_root'.
   It appears at the beginning of the segment.

   The total size of the segment, in bytes, appears as the `length'
   field of this object.  This size includes the size of the
   root object.

   All the object data types are defined here to contain pointer types
   appropriate for in-core use on a relocated symbol segment.
   Casts to and from type int are required for working with
   unrelocated symbol segments such as are found in the file.

   The ldsymaddr word is filled in by the loader to contain
   the offset (in bytes) within the ld symbol table
   of the first nonglobal symbol from this compilation.
   This makes it possible to match those symbols
   (which contain line number information) reliably with
   the segment they go with.

   Core addresses within the program that appear in the symbol segment
   are not relocated by the loader.  They are inserted by the assembler
   and apply to addresses as output by the assembler, so GDB must
   relocate them when it loads the symbol segment.  It gets the information
   on how to relocate from the textrel, datarel, bssrel, databeg and bssbeg
   words of the root object.

   The words textrel, datarel and bssrel
   are filled in by ld with the amounts to relocate within-the-file
   text, data and bss addresses by; databeg and bssbeg can be
   used to tell which kind of relocation an address needs.  */

enum language {language_c};

struct symbol_root
{
  int format;			/* Data format version */
  int length;			/* # bytes in this symbol segment */
  int ldsymoff;			/* Offset in ld symtab of this file's syms */
  int textrel;			/* Relocation for text addresses */
  int datarel;			/* Relocation for data addresses */
  int bssrel;			/* Relocation for bss addresses */
  char *filename;		/* Name of main source file compiled */
  char *filedir;		/* Name of directory it was reached from */
  struct blockvector *blockvector; /* Vector of all symbol-naming blocks */
  struct typevector *typevector; /* Vector of all data types */
  enum language language;	/* Code identifying the language used */
  char *version;		/* Version info.  Not fully specified */
  char *compilation;		/* Compilation info.  Not fully specified */
  int databeg;			/* Address within the file of data start */
  int bssbeg;			/* Address within the file of bss start */
  struct sourcevector *sourcevector; /* Vector of line-number info */
};

/* All data types of symbols in the compiled program
   are represented by `struct type' objects.
   All of these objects are pointed to by the typevector.
   The type vector may have empty slots that contain zero.  */

struct typevector
{
  int length;			/* Number of types described */
  struct type *type[1];
};

/* Different kinds of data types are distinguished by the `code' field.  */

enum type_code
{
  TYPE_CODE_UNDEF,		/* Not used; catches errors */
  TYPE_CODE_PTR,		/* Pointer type */
  TYPE_CODE_ARRAY,		/* Array type, lower bound zero */
  TYPE_CODE_STRUCT,		/* C struct or Pascal record */
  TYPE_CODE_UNION,		/* C union or Pascal variant part */
  TYPE_CODE_ENUM,		/* Enumeration type */
  TYPE_CODE_FUNC,		/* Function type */
  TYPE_CODE_INT,		/* Integer type */
  TYPE_CODE_FLT,		/* Floating type */
  TYPE_CODE_VOID,		/* Void type (values zero length) */
  TYPE_CODE_SET,		/* Pascal sets */
  TYPE_CODE_RANGE,		/* Range (integers within spec'd bounds) */
  TYPE_CODE_PASCAL_ARRAY,	/* Array with explicit type of index */

  /* C++ */
  TYPE_CODE_MEMBER,		/* Member type */
  TYPE_CODE_METHOD,		/* Method type */
  TYPE_CODE_REF,		/* C++ Reference types */
};

/* This appears in a type's flags word for an unsigned integer type.  */
#define TYPE_FLAG_UNSIGNED 1
/* This appears in a type's flags word
   if it is a (pointer to a|function returning a)* built in scalar type.
   These types are never freed.  */
#define TYPE_FLAG_PERM 4
/* This appears in a type's flags word if it is a stub type (eg. if
   someone referenced a type that wasn't definined in a source file
   via (struct sir_not_appearing_in_this_film *)).  */
#define TYPE_FLAG_STUB 8
/* Set when a class has a constructor defined */
#define	TYPE_FLAG_HAS_CONSTRUCTOR	256
/* Set when a class has a destructor defined */
#define	TYPE_FLAG_HAS_DESTRUCTOR	512
/* Indicates that this type is a public baseclass of another class,
   i.e. that all its public methods are available in the derived
   class. */
#define	TYPE_FLAG_VIA_PUBLIC		1024
/* Indicates that this type is a virtual baseclass of another class,
   i.e. that if this class is inherited more than once by another
   class, only one set of member variables will be included. */ 
#define	TYPE_FLAG_VIA_VIRTUAL		2048

struct type
{
  /* Code for kind of type */
  enum type_code code;
  /* Name of this type, or zero if none.
     This is used for printing only.
     Type names specified as input are defined by symbols.  */
  char *name;
  /* Length in bytes of storage for a value of this type */
  int length;
  /* For a pointer type, describes the type of object pointed to.
     For an array type, describes the type of the elements.
     For a function or method type, describes the type of the value.
     For a range type, describes the type of the full range.
     Unused otherwise.  */
  struct type *target_type;
  /* Type that is a pointer to this type.
     Zero if no such pointer-to type is known yet.
     The debugger may add the address of such a type
     if it has to construct one later.  */ 
  struct type *pointer_type;
  /* C++: also need a reference type.  */
  struct type *reference_type;
  struct type **arg_types;
  
  /* Type that is a function returning this type.
     Zero if no such function type is known here.
     The debugger may add the address of such a type
     if it has to construct one later.  */
  struct type *function_type;

/* Handling of pointers to members:
   TYPE_MAIN_VARIANT is used for pointer and pointer
   to member types.  Normally it the value of the address of its
   containing type.  However, for pointers to members, we must be
   able to allocate pointer to member types and look them up
   from some place of reference.
   NEXT_VARIANT is the next element in the chain. */
  struct type *main_variant, *next_variant;

  /* Flags about this type.  */
  short flags;
  /* Number of fields described for this type */
  short nfields;
  /* For structure and union types, a description of each field.
     For set and pascal array types, there is one "field",
     whose type is the domain type of the set or array.
     For range types, there are two "fields",
     the minimum and maximum values (both inclusive).
     For enum types, each possible value is described by one "field".

     Using a pointer to a separate array of fields
     allows all types to have the same size, which is useful
     because we can allocate the space for a type before
     we know what to put in it.  */
  struct field
    {
      /* Position of this field, counting in bits from start of
	 containing structure.  For a function type, this is the
	 position in the argument list of this argument.
	 For a range bound or enum value, this is the value itself.  */
      int bitpos;
      /* Size of this field, in bits, or zero if not packed.
	 For an unpacked field, the field's type's length
	 says how many bytes the field occupies.  */
      int bitsize;
      /* In a struct or enum type, type of this field.
	 In a function type, type of this argument.
	 In an array type, the domain-type of the array.  */
      struct type *type;
      /* Name of field, value or argument.
	 Zero for range bounds and array domains.  */
      char *name;
    } *fields;

  /* C++ */
  int *private_field_bits;
  int *protected_field_bits;

  /* Number of methods described for this type */
  short nfn_fields;
  /* Number of base classes this type derives from. */
  short n_baseclasses;

  /* Number of methods described for this type plus all the
     methods that it derives from.  */
  int nfn_fields_total;

  /* For classes, structures, and unions, a description of each field,
     which consists of an overloaded name, followed by the types of
     arguments that the method expects, and then the name after it
     has been renamed to make it distinct.  */
  struct fn_fieldlist
    {
      /* The overloaded name.  */
      char *name;
      /* The number of methods with this name.  */
      int length;
      /* The list of methods.  */
      struct fn_field
	{
#if 0
	  /* The overloaded name */
	  char *name;
#endif
	  /* The return value of the method */
	  struct type *type;
	  /* The argument list */
	  struct type **args;
	  /* The name after it has been processed */
	  char *physname;
	  /* If this is a virtual function, the offset into the vtbl-1,
	     else 0.  */
	  int voffset;
	} *fn_fields;

      int *private_fn_field_bits;
      int *protected_fn_field_bits;

    } *fn_fieldlists;

  unsigned char via_protected;
  unsigned char via_public;

  /* For types with virtual functions, VPTR_BASETYPE is the base class which
     defined the virtual function table pointer.  VPTR_FIELDNO is
     the field number of that pointer in the structure.

     For types that are pointer to member types, VPTR_BASETYPE
     ifs the type that this pointer is a member of.

     Unused otherwise.  */
  struct type *vptr_basetype;

  int vptr_fieldno;

  /* If this type has a base class, put it here.
     If this type is a pointer type, the chain of member pointer
     types goes here.
     Unused otherwise.
     
     Contrary to all maxims of C style and common sense, the baseclasses
     are indexed from 1 to N_BASECLASSES rather than 0 to N_BASECLASSES-1
     (i.e. BASECLASSES points to one *before* the first element of
     the array).  */
  struct type **baseclasses;
};

/* All of the name-scope contours of the program
   are represented by `struct block' objects.
   All of these objects are pointed to by the blockvector.

   Each block represents one name scope.
   Each lexical context has its own block.

   The first two blocks in the blockvector are special.
   The first one contains all the symbols defined in this compilation
   whose scope is the entire program linked together.
   The second one contains all the symbols whose scope is the
   entire compilation excluding other separate compilations.
   In C, these correspond to global symbols and static symbols.

   Each block records a range of core addresses for the code that
   is in the scope of the block.  The first two special blocks
   give, for the range of code, the entire range of code produced
   by the compilation that the symbol segment belongs to.

   The blocks appear in the blockvector
   in order of increasing starting-address,
   and, within that, in order of decreasing ending-address.

   This implies that within the body of one function
   the blocks appear in the order of a depth-first tree walk.  */

struct blockvector
{
  /* Number of blocks in the list.  */
  int nblocks;
  /* The blocks themselves.  */
  struct block *block[1];
};

struct block
{
  /* Addresses in the executable code that are in this block.
     Note: in an unrelocated symbol segment in a file,
     these are always zero.  They can be filled in from the
     N_LBRAC and N_RBRAC symbols in the loader symbol table.  */
  int startaddr, endaddr;
  /* The symbol that names this block,
     if the block is the body of a function;
     otherwise, zero.
     Note: In an unrelocated symbol segment in an object file,
     this field may be zero even when the block has a name.
     That is because the block is output before the name
     (since the name resides in a higher block).
     Since the symbol does point to the block (as its value),
     it is possible to find the block and set its name properly.  */
  struct symbol *function;
  /* The `struct block' for the containing block, or 0 if none.  */
  /* Note that in an unrelocated symbol segment in an object file
     this pointer may be zero when the correct value should be
     the second special block (for symbols whose scope is one compilation).
     This is because the compiler ouptuts the special blocks at the
     very end, after the other blocks.   */
  struct block *superblock;
  /* A flag indicating whether or not the fucntion corresponding
     to this block was compiled with gcc or not.  If there is no
     function corresponding to this block, this meaning of this flag
     is undefined.  (In practice it will be 1 if the block was created
     while processing a file compiled with gcc and 0 when not). */
  unsigned char gcc_compile_flag;
  /* Number of local symbols.  */
  int nsyms;
  /* The symbols.  */
  struct symbol *sym[1];
};

/* Represent one symbol name; a variable, constant, function or typedef.  */

/* Different name spaces for symbols.  Looking up a symbol specifies
   a namespace and ignores symbol definitions in other name spaces.

   VAR_NAMESPACE is the usual namespace.
   In C, this contains variables, function names, typedef names
   and enum type values.

   STRUCT_NAMESPACE is used in C to hold struct, union and enum type names.
   Thus, if `struct foo' is used in a C program,
   it produces a symbol named `foo' in the STRUCT_NAMESPACE.

   LABEL_NAMESPACE may be used for names of labels (for gotos);
   currently it is not used and labels are not recorded at all.  */

/* For a non-global symbol allocated statically,
   the correct core address cannot be determined by the compiler.
   The compiler puts an index number into the symbol's value field.
   This index number can be matched with the "desc" field of
   an entry in the loader symbol table.  */

enum namespace
{
  UNDEF_NAMESPACE, VAR_NAMESPACE, STRUCT_NAMESPACE, LABEL_NAMESPACE,
};

/* An address-class says where to find the value of the symbol in core.  */

enum address_class
{
  LOC_UNDEF,		/* Not used; catches errors */
  LOC_CONST,		/* Value is constant int */
  LOC_STATIC,		/* Value is at fixed address */
  LOC_REGISTER,		/* Value is in register */
  LOC_ARG,		/* Value is at spec'd position in arglist */
  LOC_REF_ARG,		/* Value address is at spec'd position in */
			/* arglist.  */
  LOC_REGPARM,		/* Value is at spec'd position in  register window */
  LOC_LOCAL,		/* Value is at spec'd pos in stack frame */
  LOC_TYPEDEF,		/* Value not used; definition in SYMBOL_TYPE
			   Symbols in the namespace STRUCT_NAMESPACE
			   all have this class.  */
  LOC_LABEL,		/* Value is address in the code */
  LOC_BLOCK,		/* Value is address of a `struct block'.
			   Function names have this class.  */
  LOC_EXTERNAL,		/* Value is at address not in this compilation.
			   This is used for .comm symbols
			   and for extern symbols within functions.
			   Inside GDB, this is changed to LOC_STATIC once the
			   real address is obtained from a loader symbol.  */
  LOC_CONST_BYTES	/* Value is a constant byte-sequence.   */
};

struct symbol
{
  /* Symbol name */
  char *name;
  /* Name space code.  */
  enum namespace namespace;
  /* Address class */
  enum address_class class;
  /* Data type of value */
  struct type *type;
  /* constant value, or address if static, or register number,
     or offset in arguments, or offset in stack frame.  */
  union
    {
      long value;
      struct block *block;      /* for LOC_BLOCK */
      char *bytes;		/* for LOC_CONST_BYTES */
    }
  value;
};

struct partial_symbol
{
  /* Symbol name */
  char *name;
  /* Name space code.  */
  enum namespace namespace;
  /* Address class (for info_symbols) */
  enum address_class class;
  /* Associated partial symbol table */
  struct partial_symtab *pst;
  /* Value (only used for static functions currently).  Done this
     way so that we can use the struct symbol macros.
     Note that the address of a function is SYMBOL_VALUE (pst)
     in a partial symbol table, but BLOCK_START (SYMBOL_BLOCK_VALUE (st))
     in a symbol table.  */
  union
    {
      long value;
    }
  value;
};

/* 
 * Vectors of all partial symbols read in from file; actually declared
 * and used in dbxread.c.
 */
extern struct psymbol_allocation_list {
  struct partial_symbol *list, *next;
  int size;
} global_psymbols, static_psymbols;


/* Source-file information.
   This describes the relation between source files and line numbers
   and addresses in the program text.  */

struct sourcevector
{
  int length;			/* Number of source files described */
  struct source *source[1];	/* Descriptions of the files */
};

/* Each item represents a line-->pc (or the reverse) mapping.  This is
   somewhat more wasteful of space than one might wish, but since only
   the files which are actually debugged are read in to core, we don't
   waste much space.

   Each item used to be an int; either minus a line number, or a
   program counter.  If it represents a line number, that is the line
   described by the next program counter value.  If it is positive, it
   is the program counter at which the code for the next line starts.  */

struct linetable_entry
{
  int line;
  CORE_ADDR pc;
};

struct linetable
{
  int nitems;
  struct linetable_entry item[1];
};

/* All the information on one source file.  */

struct source
{
  char *name;			/* Name of file */
  struct linetable contents;
};
