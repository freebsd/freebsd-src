/* Definitions and structures for reading debug symbols from the
   native HP C compiler.

   Written by the Center for Software Science at the University of Utah
   and by Cygnus Support.

   Copyright 1994 Free Software Foundation, Inc.

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

#ifndef HP_SYMTAB_INCLUDED
#define HP_SYMTAB_INCLUDED

/* General information:

   This header file defines and describes only the basic data structures
   necessary to read debug symbols produced by the HP C compiler using the
   SOM object file format.  Definitions and structures used by other compilers
   for other languages or object file formats may be missing.
   (For a full description of the debug format, ftp hpux-symtab.h from
   jaguar.cs.utah.edu:/dist).


   Debug symbols are contained entirely within an unloadable space called
   $DEBUG$.  $DEBUG$ contains several subspaces which group related
   debug symbols.

   $GNTT$ contains information for global variables, types and contants.

   $LNTT$ contains information for procedures (including nesting), scoping
   information, local variables, types, and constants.

   $SLT$ contains source line information so that code addresses may be
   mapped to source lines.

   $VT$ contains various strings and constants for named objects (variables,
   typedefs, functions, etc).  Strings are stored as null-terminated character
   lists.  Constants always begin on word boundaries.  The first byte of
   the VT must be zero (a null string).

   $XT$ is not currently used by GDB.

   Many structures within the subspaces point to other structures within
   the same subspace, or to structures within a different subspace.  These
   pointers are represented as a structure index from the beginning of
   the appropriate subspace.  */

/* Used to describe where a constant is stored.  */
enum location_type
{
  LOCATION_IMMEDIATE,
  LOCATION_PTR,
  LOCATION_VT,
};

/* Languages supported by this debug format.  Within the data structures
   this type is limited to 4 bits for a maximum of 16 languages.  */
enum hp_language
{
  HP_LANGUAGE_UNKNOWN,
  HP_LANGUAGE_C,
  HP_LANGUAGE_F77,
  HP_LANGUAGE_PASCAL,
  HP_LANGUAGE_COBOL,
  HP_LANGUAGE_BASIC,
  HP_LANGUAGE_ADA,
  HP_LANGUAGE_CPLUSPLUS,
};


/* Basic data types available in this debug format.  Within the data
   structures this type is limited to 5 bits for a maximum of 32 basic
   data types.  */
enum hp_type
{
  HP_TYPE_UNDEFINED,
  HP_TYPE_BOOLEAN,
  HP_TYPE_CHAR,
  HP_TYPE_INT,
  HP_TYPE_UNSIGNED_INT,
  HP_TYPE_REAL,
  HP_TYPE_COMPLEX,
  HP_TYPE_STRING200,
  HP_TYPE_LONGSTRING200,
  HP_TYPE_TEXT,
  HP_TYPE_FLABEL,
  HP_TYPE_FTN_STRING_SPEC,
  HP_TYPE_MOD_STRING_SPEC,
  HP_TYPE_PACKED_DECIMAL,
  HP_TYPE_REAL_3000,
  HP_TYPE_MOD_STRING_3000,
  HP_TYPE_ANYPOINTER,
  HP_TYPE_GLOBAL_ANYPOINTER,
  HP_TYPE_LOCAL_ANYPOINTER,
  HP_TYPE_COMPLEXS3000,
  HP_TYPE_FTN_STRING_S300_COMPAT,
  HP_TYPE_FTN_STRING_VAX_COMPAT,
  HP_TYPE_BOOLEAN_S300_COMPAT,
  HP_TYPE_BOOLEAN_VAX_COMPAT,
  HP_TYPE_WIDE_CHAR,
  HP_TYPE_LONG,
  HP_TYPE_UNSIGNED_LONG,
  HP_TYPE_DOUBLE,
  HP_TYPE_TEMPLATE_ARG,
};

/* An immediate name and type table entry.

   extension and immediate will always be one.
   global will always be zero.
   hp_type is the basic type this entry describes.
   bitlength is the length in bits for the basic type.  */
struct dnttp_immediate
{
  unsigned int extension:	1;
  unsigned int immediate:	1;
  unsigned int global:		1;
  unsigned int type: 		5;
  unsigned int bitlength:	24;
};

/* A nonimmediate name and type table entry.

   extension will always be one.
   immediate will always be zero.
   if global is zero, this entry points into the LNTT
   if global is one, this entry points into the GNTT
   index is the index within the GNTT or LNTT for this entry.  */
struct dnttp_nonimmediate
{
  unsigned int extension:	1;
  unsigned int immediate:	1;
  unsigned int global:		1;
  unsigned int index:		29;
};

/* A pointer to an entry in the GNTT and LNTT tables.  It has two
   forms depending on the type being described.

   The immediate form is used for simple entries and is one
   word.

   The nonimmediate form is used for complex entries and contains
   an index into the LNTT or GNTT which describes the entire type.

   If a dnttpointer is -1, then it is a NIL entry.  */

#define DNTTNIL (-1)
typedef union dnttpointer
{
  struct dnttp_immediate dntti;
  struct dnttp_nonimmediate dnttp;
  int word;
} dnttpointer;

/* An index into the source line table.  As with dnttpointers, a sltpointer
   of -1 indicates a NIL entry.  */
#define SLTNIL (-1)
typedef int sltpointer;

/* Unsigned byte offset into the VT.  */
typedef unsigned int vtpointer;

/* A DNTT entry (used within the GNTT and LNTT).

   DNTT entries are variable sized objects, but are always a multiple
   of 3 words (we call each group of 3 words a "block").

   The first bit in each block is an extension bit.  This bit is zero
   for the first block of a DNTT entry.  If the entry requires more
   than one block, then this bit is set to one in all blocks after
   the first one.  */

/* Each DNTT entry describes a particular debug symbol (beginning of
   a source file, a function, variables, structures, etc.

   The type of the DNTT entry is stored in the "kind" field within the
   DNTT entry itself.  */

enum dntt_entry_type
{
  DNTT_TYPE_NIL = -1,
  DNTT_TYPE_SRCFILE,
  DNTT_TYPE_MODULE,
  DNTT_TYPE_FUNCTION,
  DNTT_TYPE_ENTRY,
  DNTT_TYPE_BEGIN,
  DNTT_TYPE_END,
  DNTT_TYPE_IMPORT,
  DNTT_TYPE_LABEL,
  DNTT_TYPE_FPARAM,
  DNTT_TYPE_SVAR,
  DNTT_TYPE_DVAR,
  DNTT_TYPE_HOLE1,
  DNTT_TYPE_CONST,
  DNTT_TYPE_TYPEDEF,
  DNTT_TYPE_TAGDEF,
  DNTT_TYPE_POINTER,
  DNTT_TYPE_ENUM,
  DNTT_TYPE_MEMENUM,
  DNTT_TYPE_SET,
  DNTT_TYPE_SUBRANGE,
  DNTT_TYPE_ARRAY,
  DNTT_TYPE_STRUCT,
  DNTT_TYPE_UNION,
  DNTT_TYPE_FIELD,
  DNTT_TYPE_VARIANT,
  DNTT_TYPE_FILE,
  DNTT_TYPE_FUNCTYPE,
  DNTT_TYPE_WITH,
  DNTT_TYPE_COMMON,
  DNTT_TYPE_COBSTRUCT,
  DNTT_TYPE_XREF,
  DNTT_TYPE_SA,
  DNTT_TYPE_MACRO,
  DNTT_TYPE_BLOCKDATA,
  DNTT_TYPE_CLASS_SCOPE,
  DNTT_TYPE_REFERENCE,
  DNTT_TYPE_PTRMEM,
  DNTT_TYPE_PTRMEMFUNC,
  DNTT_TYPE_CLASS,
  DNTT_TYPE_GENFIELD,
  DNTT_TYPE_VFUNC,
  DNTT_TYPE_MEMACCESS,
  DNTT_TYPE_INHERITANCE,
  DNTT_TYPE_FRIEND_CLASS,
  DNTT_TYPE_FRIEND_FUNC,
  DNTT_TYPE_MODIFIER,
  DNTT_TYPE_OBJECT_ID,
  DNTT_TYPE_MEMFUNC,
  DNTT_TYPE_TEMPLATE,
  DNTT_TYPE_TEMPLATE_ARG,
  DNTT_TYPE_FUNC_TEMPLATE,
  DNTT_TYPE_LINK,
  DNTT_TYPE_MAX,
};

/* DNTT_TYPE_SRCFILE:

   One DNTT_TYPE_SRCFILE symbol is output for the start of each source
   file and at the begin and end of an included file.  A DNTT_TYPE_SRCFILE
   entry is also output before each DNTT_TYPE_FUNC symbol so that debuggers
   can determine what file a function was defined in.

   LANGUAGE describes the source file's language.

   NAME points to an VT entry providing the source file's name.

   Note the name used for DNTT_TYPE_SRCFILE entries are exactly as seen
   by the compiler (ie they may be relative or absolute).  C include files
   via <> inclusion must use absolute paths.

   ADDRESS points to an SLT entry from which line number and code locations
   may be determined.  */

struct dntt_type_srcfile
{
  unsigned int extension:	1;
  unsigned int kind:	10;
  unsigned int language:	4;
  unsigned int unused:		17;
  vtpointer name;
  sltpointer address;
};

/* DNTT_TYPE_MODULE:

   A DNTT_TYPE_MODULE symbol is emitted for the start of a pascal
   module or C source file.

   Each DNTT_TYPE_MODULE must have an associated DNTT_TYPE_END symbol.

   NAME points to a VT entry providing the module's name.  Note C
   source files are considered nameless modules.

   ALIAS point to a VT entry providing a secondary name.

   ADDRESS points to an SLT entry from which line number and code locations
   may be determined.  */

struct dntt_type_module
{
  unsigned int extension:	1;
  unsigned int kind:	10;
  unsigned int unused:		21;
  vtpointer name;
  vtpointer alias;
  dnttpointer unused2;
  sltpointer address;
};

/* DNTT_TYPE_FUNCTION:

   A DNTT_TYPE_FUNCTION symbol is emitted for each function definition;
   a DNTT_TYPE_ENTRY symbols is used for secondary entry points.  Both
   symbols used the dntt_type_function structure.

   Each DNTT_TYPE_FUNCTION must have a matching DNTT_TYPE_END.

   GLOBAL is nonzero if the function has global scope.

   LANGUAGE describes the function's source language.

   OPT_LEVEL describes the optimization level the function was compiled
   with.

   VARARGS is nonzero if the function uses varargs.

   NAME points to a VT entry providing the function's name.

   ALIAS points to a VT entry providing a secondary name for the function.

   FIRSTPARAM points to a LNTT entry which describes the parameter list.

   ADDRESS points to an SLT entry from which line number and code locations
   may be determined.

   ENTRYADDR is the memory address corresponding the the function's entry point

   RETVAL points to a LNTT entry describing the function's return value.

   LOWADDR is the lowest memory address associated with this function.

   HIADDR is the highest memory address associated with this function.  */

struct dntt_type_function
{
  unsigned int extension:	1;
  unsigned int kind:	10;
  unsigned int global:		1;
  unsigned int language:	4;
  unsigned int nest_level:	5;
  unsigned int opt_level:	2;
  unsigned int varargs:		1;
  unsigned int lang_info:	4;
  unsigned int inlined:		1;
  unsigned int localalloc:	1;
  unsigned int expansion:	1;
  unsigned int unused:		1;
  vtpointer name;
  vtpointer alias;
  dnttpointer firstparam;
  sltpointer address;
  CORE_ADDR entryaddr;
  dnttpointer retval;
  CORE_ADDR lowaddr;
  CORE_ADDR hiaddr;
};

/* DNTT_TYPE_BEGIN:

   A DNTT_TYPE_BEGIN symbol is emitted to begin a new nested scope.
   Every DNTT_TYPE_BEGIN symbol must have a matching DNTT_TYPE_END symbol.

   CLASSFLAG is nonzero if this is the beginning of a c++ class definition.

   ADDRESS points to an SLT entry from which line number and code locations
   may be determined.  */

struct dntt_type_begin
{
  unsigned int extension:	1;
  unsigned int kind:	10;
  unsigned int classflag:	1;
  unsigned int unused:		20;
  sltpointer address;
};

/* DNTT_TYPE_END:

   A DNTT_TYPE_END symbol is emitted when closing a scope started by
   a DNTT_TYPE_MODULE, DNTT_TYPE_FUNCTION, and DNTT_TYPE_BEGIN symbols.

   ENDKIND describes what type of scope the DNTT_TYPE_END is closing
   (DNTT_TYPE_MODULE, DNTT_TYPE_BEGIN, etc).

   CLASSFLAG is nonzero if this is the end of a c++ class definition.

   ADDRESS points to an SLT entry from which line number and code locations
   may be determined.

   BEGINSCOPE points to the LNTT entry which opened the scope.  */

struct dntt_type_end
{
  unsigned int extension:	1;
  unsigned int kind:	10;
  unsigned int endkind:	10;
  unsigned int classflag:	1;
  unsigned int unused:		10;
  sltpointer address;
  dnttpointer beginscope;
};

/* DNTT_TYPE_IMPORT is unused by GDB.  */
/* DNTT_TYPE_LABEL is unused by GDB.  */

/* DNTT_TYPE_FPARAM:

   A DNTT_TYPE_FPARAM symbol is emitted for a function argument.  When
   chained together the symbols represent an argument list for a function.

   REGPARAM is nonzero if this parameter was passed in a register.

   INDIRECT is nonzero if this parameter is a pointer to the parameter
   (pass by reference or pass by value for large items).

   LONGADDR is nonzero if the parameter is a 64bit pointer.

   NAME is a pointer into the VT for the parameter's name.

   LOCATION describes where the parameter is stored.  Depending on the
   parameter type LOCATION could be a register number, or an offset
   from the stack pointer.

   TYPE points to a NTT entry describing the type of this parameter.

   NEXTPARAM points to the LNTT entry describing the next parameter.  */

struct dntt_type_fparam
{
  unsigned int extension:	1;
  unsigned int kind:	10;
  unsigned int regparam:	1;
  unsigned int indirect:	1;
  unsigned int longaddr:	1;
  unsigned int copyparam:	1;
  unsigned int dflt:		1;
  unsigned int unused:		16;
  vtpointer name;
  int location;
  dnttpointer type;
  dnttpointer nextparam;
  int misc;
};

/* DNTT_TYPE_SVAR:

   A DNTT_TYPE_SVAR is emitted to describe a variable in static storage.

   GLOBAL is nonzero if the variable has global scope.

   INDIRECT is nonzero if the variable is a pointer to an object.

   LONGADDR is nonzero if the variable is in long pointer space.

   STATICMEM is nonzero if the variable is a member of a class.

   A_UNION is nonzero if the variable is an anonymous union member.

   NAME is a pointer into the VT for the variable's name.

   LOCATION provides the memory address for the variable.

   TYPE is a pointer into either the GNTT or LNTT which describes
   the type of this variable.  */

struct dntt_type_svar
{
  unsigned int extension:	1;
  unsigned int kind:	10;
  unsigned int global:		1;
  unsigned int indirect:	1;
  unsigned int longaddr:	1;
  unsigned int staticmem:	1;
  unsigned int a_union:		1;
  unsigned int unused:		16;
  vtpointer name;
  CORE_ADDR location;
  dnttpointer type;
  unsigned int offset;
  unsigned int displacement;
};

/* DNTT_TYPE_DVAR:

   A DNTT_TYPE_DVAR is emitted to describe automatic variables and variables
   held in registers.

   GLOBAL is nonzero if the variable has global scope.

   INDIRECT is nonzero if the variable is a pointer to an object.

   REGVAR is nonzero if the variable is in a register.

   A_UNION is nonzero if the variable is an anonymous union member.

   NAME is a pointer into the VT for the variable's name.

   LOCATION provides the memory address or register number for the variable.

   TYPE is a pointer into either the GNTT or LNTT which describes
   the type of this variable.  */

struct dntt_type_dvar
{
  unsigned int extension:	1;
  unsigned int kind:	10;
  unsigned int global:		1;
  unsigned int indirect:	1;
  unsigned int regvar:		1;
  unsigned int a_union:		1;
  unsigned int unused:		17;
  vtpointer name;
  int location;
  dnttpointer type;
  unsigned int offset;
};

/* DNTT_TYPE_CONST:

   A DNTT_TYPE_CONST symbol is emitted for program constants.

   GLOBAL is nonzero if the constant has global scope.

   INDIRECT is nonzero if the constant is a pointer to an object.

   LOCATION_TYPE describes where to find the constant's value
   (in the VT, memory, or embedded in an instruction).

   CLASSMEM is nonzero if the constant is a member of a class.

   NAME is a pointer into the VT for the constant's name.

   LOCATION provides the memory address, register number or pointer
   into the VT for the constant's value.

   TYPE is a pointer into either the GNTT or LNTT which describes
   the type of this variable.  */

struct dntt_type_const
{
  unsigned int extension:	1;
  unsigned int kind:	10;
  unsigned int global:		1;
  unsigned int indirect:	1;
  unsigned int:		3;
  unsigned int classmem:	1;
  unsigned int unused:		15;
  vtpointer name;
  CORE_ADDR location;
  dnttpointer type;
  unsigned int offset;
  unsigned int displacement;
};

/* DNTT_TYPE_TYPEDEF and DNTT_TYPE_TAGDEF:

   The same structure is used to describe typedefs and tagdefs.

   DNTT_TYPE_TYPEDEFS are associated with C "typedefs".

   DNTT_TYPE_TAGDEFs are associated with C "struct", "union", and "enum"
   tags, which may have the same name as a typedef in the same scope.

   GLOBAL is nonzero if the typedef/tagdef has global scope.

   TYPEINFO is used to determine if full type information is available
   for a tag.  (usually 1, but can be zero for opaque types in C).

   NAME is a pointer into the VT for the constant's name.

   TYPE points to the underlying type for the typedef/tagdef in the
   GNTT or LNTT.  */

struct dntt_type_type
{
  unsigned int extension:	1;
  unsigned int kind:	10;
  unsigned int global:		1;
  unsigned int typeinfo:	1;
  unsigned int unused:		19;
  vtpointer name;
  dnttpointer type;
};

/* DNTT_TYPE_POINTER:

   Used to describe a pointer to an underlying type.

   POINTSTO is a pointer into the GNTT or LNTT for the type which this
   pointer points to.

   BITLENGTH is the length of the pointer (not the underlying type). */

struct dntt_type_pointer
{
  unsigned int extension:	1;
  unsigned int kind:	10;
  unsigned int unused:		21;
  dnttpointer pointsto;
  unsigned int bitlength;
};


/* DNTT_TYPE_ENUM:

   Used to describe enumerated types.

   FIRSTMEM is a pointer to a DNTT_TYPE_MEMENUM in the GNTT/LNTT which
   describes the first member (and contains a pointer to the chain of
   members).

   BITLENGTH is the number of bits used to hold the values of the enum's
   members.  */

struct dntt_type_enum
{
  unsigned int extension:	1;
  unsigned int kind:	10;
  unsigned int unused:		21;
  dnttpointer firstmem;
  unsigned int bitlength;
};

/* DNTT_TYPE_MEMENUM

   Used to describe members of an enumerated type.

   CLASSMEM is nonzero if this member is part of a class.

   NAME points into the VT for the name of this member.

   VALUE is the value of this enumeration member.

   NEXTMEM points to the next DNTT_TYPE_MEMENUM in the chain.  */

struct dntt_type_memenum
{
  unsigned int extension:	1;
  unsigned int kind:	10;
  unsigned int classmem:	1;
  unsigned int unused:		20;
  vtpointer name;
  unsigned int value;
  dnttpointer nextmem;
};

/* DNTT_TYPE_SET

   DECLARATION describes the bitpacking of the set.

   SUBTYPE points to a DNTT entry describing the type of the members.

   BITLENGTH is the size of the set.  */ 

struct dntt_type_set
{
  unsigned int extension:	1;
  unsigned int kind:	10;
  unsigned int declaration:	2;
  unsigned int unused:		19;
  dnttpointer subtype;
  unsigned int bitlength;
};

/* DNTT_TYPE_SUBRANGE

   DYN_LOW describes the lower bound of the subrange:

     00 for a constant lower bound (found in LOWBOUND).

     01 for a dynamic lower bound with the lower bound found in the the
     memory address pointed to by LOWBOUND.

     10 for a dynamic lower bound described by an variable found in the
     DNTT/LNTT (LOWBOUND would be a pointer into the DNTT/LNTT).

   DYN_HIGH is similar to DYN_LOW, except it describes the upper bound.

   SUBTYPE points to the type of the subrange.

   BITLENGTH is the length in bits needed to describe the subrange's
   values.  */

struct dntt_type_subrange
{
  unsigned int extension:	1;
  unsigned int kind:	10;
  unsigned int dyn_low:		2;
  unsigned int dyn_high:	2;
  unsigned int unused:		17;
  int lowbound;
  int highbound;
  dnttpointer subtype;
  unsigned int bitlength;
};

/* DNTT_TYPE_ARRAY

   DECLARATION describes the bit packing used in the array.

   ARRAYISBYTES is nonzero if the field in arraylength describes the
   length in bytes rather than in bits.  A value of zero is used to
   describe an array with size 2**32.

   ELEMISBYTES is nonzero if the length if each element in the array
   is describes in bytes rather than bits.  A value of zero is used
   to an element with size 2**32.

   ELEMORDER is nonzero if the elements are indexed in increasing order.

   JUSTIFIED if the elements are left justified to index zero.

   ARRAYLENGTH is the length of the array.

   INDEXTYPE is a DNTT pointer to the type used to index the array.

   ELEMTYPE is a DNTT pointer to the type for the array elements.

   ELEMLENGTH is the length of each element in the array (including
   any padding).

   Multi-dimensional arrays are represented by ELEMTYPE pointing to
   another DNTT_TYPE_ARRAY.  */

struct dntt_type_array
{
  unsigned int extension:	1;
  unsigned int kind:	10;
  unsigned int declaration:	2;
  unsigned int dyn_low:		2;
  unsigned int dyn_high:	2;
  unsigned int arrayisbytes:	1;
  unsigned int elemisbytes:	1;
  unsigned int elemorder:	1;
  unsigned int justified:	1;
  unsigned int unused:		11;
  unsigned int arraylength;
  dnttpointer indextype;
  dnttpointer elemtype;
  unsigned int elemlength;
};

/* DNTT_TYPE_STRUCT

   DNTT_TYPE_STRUCT is used to describe a C structure.

   DECLARATION describes the bitpacking used.

   FIRSTFIELD is a DNTT pointer to the first field of the structure
   (each field contains a pointer to the next field, walk the list
   to access all fields of the structure).

   VARTAGFIELD and VARLIST are used for Pascal variant records.

   BITLENGTH is the size of the structure in bits.  */

struct dntt_type_struct
{
  unsigned int extension:	1;
  unsigned int kind:	10;
  unsigned int declaration:	2;
  unsigned int unused:		19;
  dnttpointer firstfield;
  dnttpointer vartagfield;
  dnttpointer varlist;
  unsigned int bitlength;
};

/* DNTT_TYPE_UNION

   DNTT_TYPE_UNION is used to describe a C union.

   FIRSTFIELD is a DNTT pointer to the beginning of the field chain.

   BITLENGTH is the size of the union in bits.  */

struct dntt_type_union
{
  unsigned int extension:	1;
  unsigned int kind:	10;
  unsigned int unused:		21;
  dnttpointer firstfield;
  unsigned int bitlength;
};

/* DNTT_TYPE_FIELD

   DNTT_TYPE_FIELD describes one field in a structure or union.

   VISIBILITY is used to describe the visibility of the field
   (for c++.  public = 0, protected = 1, private = 2).

   A_UNION is nonzero if this field is a member of an anonymous union.

   STATICMEM is nonzero if this field is a static member of a template.

   NAME is a pointer into the VT for the name of the field.

   BITOFFSET gives the offset of this field in bits from the beginning
   of the structure or union this field is a member of.

   TYPE is a DNTT pointer to the type describing this field.

   BITLENGTH is the size of the entry in bits.

   NEXTFIELD is a DNTT pointer to the next field in the chain.  */

struct dntt_type_field
{
  unsigned int extension:	1;
  unsigned int kind:	10;
  unsigned int visibility:	2;
  unsigned int a_union:		1;
  unsigned int staticmem:	1;
  unsigned int unused:		17;
  vtpointer name;
  unsigned int bitoffset;
  dnttpointer type;
  unsigned int bitlength;
  dnttpointer nextfield;
};

/* DNTT_TYPE_VARIANT is unused by GDB.  */
/* DNTT_TYPE_FILE is unused by GDB.  */

/* DNTT_TYPE_COMMON is unused by GDB.  */
/* DNTT_TYPE_LINK is unused by GDB.  */
/* DNTT_TYPE_FFUNC_LINK is unused by GDB.  */
/* DNTT_TYPE_TEMPLATE is unused by GDB.  */

/* DNTT_TYPE_FUNCTYPE

   VARARGS is nonzero if this function uses varargs.

   FIRSTPARAM is a DNTT pointer to the first entry in the parameter
   chain.

   RETVAL is a DNTT pointer to the type of the return value.  */

struct dntt_type_functype
{
  unsigned int extension:	1;
  unsigned int kind:	10;
  unsigned int varargs:		1;
  unsigned int info:		4;
  unsigned int unused:		16;
  unsigned int bitlength;
  dnttpointer firstparam;
  dnttpointer retval;
};

/* DNTT_TYPE_WITH is unued by GDB.  */
/* DNTT_TYPE_COBSTRUCT is unused by GDB.  */
/* DNTT_TYPE_MODIFIER is unused by GDB.  */
/* DNTT_TYPE_GENFIELD is unused by GDB.  */
/* DNTT_TYPE_MEMACCESS is unused by GDB.  */
/* DNTT_TYPE_VFUNC is unused by GDB.  */
/* DNTT_TYPE_CLASS_SCOPE is unused by GDB.  */
/* DNTT_TYPE_FRIEND_CLASS is unused by GDB.  */
/* DNTT_TYPE_FRIEND_FUNC is unused by GDB.  */
/* DNTT_TYPE_CLASS unused by GDB.  */
/* DNTT_TYPE_TEMPLATE unused by GDB.  */
/* DNTT_TYPE_TEMPL_ARG is unused by GDB.  */
/* DNTT_TYPE_PTRMEM not used by GDB */
/* DNTT_TYPE_INHERITANCE is unused by GDB.  */
/* DNTT_TYPE_OBJECT_ID is unused by GDB. */
/* DNTT_TYPE_XREF is unused by GDB.  */
/* DNTT_TYPE_SA is unused by GDB.  */

/* DNTT_TYPE_GENERIC and DNTT_TYPE_BLOCK are convience structures
   so we can examine a DNTT entry in a generic fashion.  */
struct dntt_type_generic
{
  unsigned int word[9];
};

struct dntt_type_block
{
  unsigned int extension:	1;
  unsigned int kind:	10;
  unsigned int unused:		21;
  unsigned int word[2];
};

/* One entry in a DNTT (either the LNTT or GNTT).  */
union dnttentry
{
  struct dntt_type_srcfile dsfile;
  struct dntt_type_module dmodule;
  struct dntt_type_function dfunc;
  struct dntt_type_function dentry;
  struct dntt_type_begin dbegin;
  struct dntt_type_end dend;
  struct dntt_type_fparam dfparam;
  struct dntt_type_svar dsvar;
  struct dntt_type_dvar ddvar;
  struct dntt_type_const dconst;
  struct dntt_type_type dtype;
  struct dntt_type_type dtag;
  struct dntt_type_pointer dptr;
  struct dntt_type_enum denum;
  struct dntt_type_memenum dmember;
  struct dntt_type_set dset;
  struct dntt_type_subrange dsubr;
  struct dntt_type_array darray;
  struct dntt_type_struct dstruct;
  struct dntt_type_union dunion;
  struct dntt_type_field dfield;
  struct dntt_type_functype dfunctype;
  struct dntt_type_generic dgeneric;
  struct dntt_type_block dblock;
};

/* Source line entry types.  */
enum slttype
{
  SLT_NORMAL,
  SLT_SRCFILE,
  SLT_MODULE,
  SLT_FUNCTION,
  SLT_ENTRY,
  SLT_BEGIN,
  SLT_END,
  SLT_WITH,
  SLT_EXIT,
  SLT_ASSIST,
  SLT_MARKER,
};

/* A normal source line entry.  Simply provides a mapping of a source
   line number to a code address.

   SLTDESC will always be SLT_NORMAL or SLT_EXIT.  */

struct slt_normal
{
  unsigned int sltdesc:	4;
  unsigned int line:	28;
  CORE_ADDR address;
};

/* A special source line entry.  Provides a mapping of a declaration
   to a line number.  These entries point back into the DNTT which
   references them.  */

struct slt_special
{
  unsigned int sltdesc:	4;
  unsigned int line:	28;
  dnttpointer backptr;
};

/* Used to describe nesting.

   For nested languages, an slt_assist entry must follow each SLT_FUNC
   entry in the SLT.  The address field will point forward to the
   first slt_normal entry within the function's scope.  */

struct slt_assist
{
  unsigned int sltdesc:	4;
  unsigned int unused:	28;
  sltpointer address;
};

struct slt_generic
{
  unsigned int word[2];
};

union sltentry
{
  struct slt_normal snorm;
  struct slt_special sspec;
  struct slt_assist sasst;
  struct slt_generic sgeneric;
};

#endif /* HP_SYMTAB_INCLUDED */
