/* Internal type definitions for GDB.
   Copyright (C) 1992 Free Software Foundation, Inc.
   Contributed by Cygnus Support, using pieces from other GDB modules.

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
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#if !defined (GDBTYPES_H)
#define GDBTYPES_H 1

/* When gdb creates fundamental types, it uses one of the following
   type identifiers.  The identifiers are used to index a vector of
   pointers to any types that are created. */

#define FT_VOID			0
#define FT_BOOLEAN		1
#define FT_CHAR			2
#define FT_SIGNED_CHAR		3
#define FT_UNSIGNED_CHAR	4
#define FT_SHORT		5
#define FT_SIGNED_SHORT		6
#define FT_UNSIGNED_SHORT	7
#define FT_INTEGER		8
#define FT_SIGNED_INTEGER	9
#define FT_UNSIGNED_INTEGER	10
#define FT_LONG			11
#define FT_SIGNED_LONG		12
#define FT_UNSIGNED_LONG	13
#define FT_LONG_LONG		14
#define FT_SIGNED_LONG_LONG	15
#define FT_UNSIGNED_LONG_LONG	16
#define FT_FLOAT		17
#define FT_DBL_PREC_FLOAT	18
#define FT_EXT_PREC_FLOAT	19
#define FT_COMPLEX		20
#define FT_DBL_PREC_COMPLEX	21
#define FT_EXT_PREC_COMPLEX	22
#define FT_STRING		23
#define FT_FIXED_DECIMAL	24
#define FT_FLOAT_DECIMAL	25
#define FT_BYTE			26
#define FT_UNSIGNED_BYTE	27

#define FT_NUM_MEMBERS		28	/* Highest FT_* above, plus one. */

/* Some macros for char-based bitfields.  */

#define B_SET(a,x)	((a)[(x)>>3] |= (1 << ((x)&7)))
#define B_CLR(a,x)	((a)[(x)>>3] &= ~(1 << ((x)&7)))
#define B_TST(a,x)	((a)[(x)>>3] & (1 << ((x)&7)))
#define B_TYPE		unsigned char
#define	B_BYTES(x)	( 1 + ((x)>>3) )
#define	B_CLRALL(a,x)	memset ((a), 0, B_BYTES(x))

/* Different kinds of data types are distinguished by the `code' field.  */

enum type_code
{
  TYPE_CODE_UNDEF,		/* Not used; catches errors */
  TYPE_CODE_PTR,		/* Pointer type */
  TYPE_CODE_ARRAY,		/* Array type with lower & upper bounds. */
  TYPE_CODE_STRUCT,		/* C struct or Pascal record */
  TYPE_CODE_UNION,		/* C union or Pascal variant part */
  TYPE_CODE_ENUM,		/* Enumeration type */
  TYPE_CODE_FUNC,		/* Function type */
  TYPE_CODE_INT,		/* Integer type */

  /* Floating type.  This is *NOT* a complex type.  Complex types, when
     we have them, will have their own type code (or TYPE_CODE_ERROR if
     we can parse a complex type but not manipulate it).  There are parts
     of GDB which bogusly assume that TYPE_CODE_FLT can mean complex.  */
  TYPE_CODE_FLT,

  /* Void type (values zero length; the length field is ignored).  */
  TYPE_CODE_VOID,

  TYPE_CODE_SET,		/* Pascal sets */
  TYPE_CODE_RANGE,		/* Range (integers within spec'd bounds) */
  TYPE_CODE_STRING,		/* String types, distinct from array of char */
  TYPE_CODE_BITSTRING,		/* String of bits, distinct from bool array */

  /* Unknown type.  The length field is valid if we were able to
     deduce that much about the type, or 0 if we don't even know that.  */
  TYPE_CODE_ERROR,

  /* C++ */
  TYPE_CODE_MEMBER,		/* Member type */
  TYPE_CODE_METHOD,		/* Method type */
  TYPE_CODE_REF,		/* C++ Reference types */

  /* Modula-2 */
  TYPE_CODE_CHAR,		/* *real* character type */
  TYPE_CODE_BOOL		/* BOOLEAN type */
};

/* For now allow source to use TYPE_CODE_CLASS for C++ classes, as an
   alias for TYPE_CODE_STRUCT.  Eventually these should probably be
   officially distinct types within gdb. */

#define TYPE_CODE_CLASS TYPE_CODE_STRUCT

/* Some bits for the type's flags word. */

/* Explicitly unsigned integer type */

#define TYPE_FLAG_UNSIGNED	(1 << 0)

/* Explicitly signed integer type */

#define TYPE_FLAG_SIGNED	(1 << 1)

/* This appears in a type's flags word if it is a stub type (e.g., if
   someone referenced a type that wasn't defined in a source file
   via (struct sir_not_appearing_in_this_film *)).  */

#define TYPE_FLAG_STUB		(1 << 2)


struct type
{

  /* Code for kind of type */

  enum type_code code;

  /* Name of this type, or NULL if none.

     This is used for printing only, except by poorly designed C++ code.
     For looking up a name, look for a symbol in the VAR_NAMESPACE.  */

  char *name;

  /* Tag name for this type, or NULL if none.  This means that the
     name of the type consists of a keyword followed by the tag name.
     Which keyword is determined by the type code ("struct" for
     TYPE_CODE_STRUCT, etc.).  As far as I know C/C++ are the only languages
     with this feature.

     This is used for printing only, except by poorly designed C++ code.
     For looking up a name, look for a symbol in the STRUCT_NAMESPACE.
     One more legitimate use is that if TYPE_FLAG_STUB is set, this is
     the name to use to look for definitions in other files.  */

  char *tag_name;

  /* Length, in units of TARGET_CHAR_BIT bits,
     of storage for a value of this type */

  unsigned length;

  /* Every type is now associated with a particular objfile, and the
     type is allocated on the type_obstack for that objfile.  One problem
     however, is that there are times when gdb allocates new types while
     it is not in the process of reading symbols from a particular objfile.
     Fortunately, these happen when the type being created is a derived
     type of an existing type, such as in lookup_pointer_type().  So
     we can just allocate the new type using the same objfile as the
     existing type, but to do this we need a backpointer to the objfile
     from the existing type.  Yes this is somewhat ugly, but without
     major overhaul of the internal type system, it can't be avoided
     for now. */

  struct objfile *objfile;

  /* For a pointer type, describes the type of object pointed to.
     For an array type, describes the type of the elements.
     For a function or method type, describes the type of the return value.
     For a range type, describes the type of the full range.
     Unused otherwise.  */

  struct type *target_type;

  /* Type that is a pointer to this type.
     NULL if no such pointer-to type is known yet.
     The debugger may add the address of such a type
     if it has to construct one later.  */ 

  struct type *pointer_type;

  /* C++: also need a reference type.  */

  struct type *reference_type;

  /* Type that is a function returning this type.
     NULL if no such function type is known here.
     The debugger may add the address of such a type
     if it has to construct one later.  */

  struct type *function_type;

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
     For C++ classes, there is one field for each base class (if it is
     a derived class) plus one field for each class data member.  Member
     functions are recorded elsewhere.

     Using a pointer to a separate array of fields
     allows all types to have the same size, which is useful
     because we can allocate the space for a type before
     we know what to put in it.  */

  struct field
    {

      /* Position of this field, counting in bits from start of
	 containing structure.  For a function type, this is the
	 position in the argument list of this argument.
	 For a range bound or enum value, this is the value itself.
	 (FIXME:  What about ranges larger than host int size?)
	 For BITS_BIG_ENDIAN=1 targets, it is the bit offset to the MSB.
	 For BITS_BIG_ENDIAN=0 targets, it is the bit offset to the LSB. */

      int bitpos;

      /* Size of this field, in bits, or zero if not packed.
	 For an unpacked field, the field's type's length
	 says how many bytes the field occupies.  */
      /* FIXME: This is abused by TYPE_FIELD_STATIC_PHYSNAME to contain 
	 a pointer, so it has to be long.  */

      long bitsize;

      /* In a struct or enum type, type of this field.
	 In a function type, type of this argument.
	 In an array type, the domain-type of the array.  */

      struct type *type;

      /* Name of field, value or argument.
	 NULL for range bounds and array domains.  */

      char *name;

    } *fields;

  /* For types with virtual functions, VPTR_BASETYPE is the base class which
     defined the virtual function table pointer.  

     For types that are pointer to member types, VPTR_BASETYPE
     is the type that this pointer is a member of.

     Unused otherwise.  */

  struct type *vptr_basetype;

  /* Field number of the virtual function table pointer in
     VPTR_BASETYPE.  If -1, we were unable to find the virtual
     function table pointer in initial symbol reading, and
     fill_in_vptr_fieldno should be called to find it if possible.

     Unused if this type does not have virtual functions.  */

  int vptr_fieldno;

  /* Slot to point to additional language-specific fields of this type.  */

  union type_specific
    {

      /* ARG_TYPES is for TYPE_CODE_METHOD and TYPE_CODE_FUNC.  */

      struct type **arg_types;

      /* CPLUS_STUFF is for TYPE_CODE_STRUCT.  It is initialized to point to
	 cplus_struct_default, a default static instance of a struct
	 cplus_struct_type. */

      struct cplus_struct_type *cplus_stuff;

    } type_specific;
};

#define	NULL_TYPE ((struct type *) 0)

/* C++ language-specific information for TYPE_CODE_STRUCT and TYPE_CODE_UNION
   nodes.  */

struct cplus_struct_type
{
  /* Number of base classes this type derives from.  The baseclasses are
     stored in the first N_BASECLASSES fields (i.e. the `fields' field of
     the struct type).  I think only the `type' field of such a field has
     any meaning.  */

  short n_baseclasses;

  /* Number of methods with unique names.  All overloaded methods with
     the same name count only once. */

  short nfn_fields;

  /* Number of methods described for this type plus all the
     methods that it derives from.  */

  int nfn_fields_total;

  /* For derived classes, the number of base classes is given by n_baseclasses
     and virtual_field_bits is a bit vector containing one bit per base class.
     If the base class is virtual, the corresponding bit will be set.
     I.E, given:

	class A{};
	class B{};
	class C : public B, public virtual A {};

     B is a baseclass of C; A is a virtual baseclass for C.
     This is a C++ 2.0 language feature. */

  B_TYPE *virtual_field_bits;

  /* For classes with private fields, the number of fields is given by
     nfields and private_field_bits is a bit vector containing one bit
     per field.
     If the field is private, the corresponding bit will be set. */

  B_TYPE *private_field_bits;

  /* For classes with protected fields, the number of fields is given by
     nfields and protected_field_bits is a bit vector containing one bit
     per field.
     If the field is private, the corresponding bit will be set. */

  B_TYPE *protected_field_bits;

  /* For classes, structures, and unions, a description of each field,
     which consists of an overloaded name, followed by the types of
     arguments that the method expects, and then the name after it
     has been renamed to make it distinct.

     fn_fieldlists points to an array of nfn_fields of these. */

  struct fn_fieldlist
    {

      /* The overloaded name.  */

      char *name;

      /* The number of methods with this name.  */

      int length;

      /* The list of methods.  */

      struct fn_field
	{

	  /* If is_stub is clear, this is the mangled name which we can
	     look up to find the address of the method (FIXME: it would
	     be cleaner to have a pointer to the struct symbol here
	     instead).  */

	  /* If is_stub is set, this is the portion of the mangled
	     name which specifies the arguments.  For example, "ii",
	     if there are two int arguments, or "" if there are no
	     arguments.  See gdb_mangle_name for the conversion from this
	     format to the one used if is_stub is clear.  */

	  char *physname;

	  /* The return value of the method */

	  struct type *type;

	  /* The argument list.  Only valid if is_stub is clear.  Contains
	     the type of each argument, including `this', and ending with
	     a NULL pointer after the last argument.  */

	  struct type **args;

	  /* For virtual functions.
	     First baseclass that defines this virtual function.   */

	  struct type *fcontext;

	  /* Attributes. */

	  unsigned int is_const : 1;
	  unsigned int is_volatile : 1;
	  unsigned int is_private : 1;
	  unsigned int is_protected : 1;

	  /* A stub method only has some fields valid (but they are enough
	     to reconstruct the rest of the fields).  */
	  unsigned int is_stub : 1;

	  /* Unused.  */
	  unsigned int dummy : 3;

	  /* Index into that baseclass's virtual function table,
	     minus 2; else if static: VOFFSET_STATIC; else: 0.  */

	  unsigned int voffset : 24;

#	  define VOFFSET_STATIC 1

	} *fn_fields;

    } *fn_fieldlists;

};

/* The default value of TYPE_CPLUS_SPECIFIC(T) points to the
   this shared static structure. */

extern const struct cplus_struct_type cplus_struct_default;

extern void
allocate_cplus_struct_type PARAMS ((struct type *));

#define INIT_CPLUS_SPECIFIC(type) \
  (TYPE_CPLUS_SPECIFIC(type)=(struct cplus_struct_type*)&cplus_struct_default)
#define ALLOCATE_CPLUS_STRUCT_TYPE(type) allocate_cplus_struct_type (type)
#define HAVE_CPLUS_STRUCT(type) \
  (TYPE_CPLUS_SPECIFIC(type) != &cplus_struct_default)

#define TYPE_NAME(thistype) (thistype)->name
#define TYPE_TAG_NAME(type) ((type)->tag_name)
#define TYPE_TARGET_TYPE(thistype) (thistype)->target_type
#define TYPE_POINTER_TYPE(thistype) (thistype)->pointer_type
#define TYPE_REFERENCE_TYPE(thistype) (thistype)->reference_type
#define TYPE_FUNCTION_TYPE(thistype) (thistype)->function_type
#define TYPE_LENGTH(thistype) (thistype)->length
#define TYPE_OBJFILE(thistype) (thistype)->objfile
#define TYPE_FLAGS(thistype) (thistype)->flags
#define TYPE_UNSIGNED(thistype) ((thistype)->flags & TYPE_FLAG_UNSIGNED)
#define TYPE_CODE(thistype) (thistype)->code
#define TYPE_NFIELDS(thistype) (thistype)->nfields
#define TYPE_FIELDS(thistype) (thistype)->fields

/* C++ */

#define TYPE_VPTR_BASETYPE(thistype) (thistype)->vptr_basetype
#define TYPE_DOMAIN_TYPE(thistype) (thistype)->vptr_basetype
#define TYPE_VPTR_FIELDNO(thistype) (thistype)->vptr_fieldno
#define TYPE_FN_FIELDS(thistype) TYPE_CPLUS_SPECIFIC(thistype)->fn_fields
#define TYPE_NFN_FIELDS(thistype) TYPE_CPLUS_SPECIFIC(thistype)->nfn_fields
#define TYPE_NFN_FIELDS_TOTAL(thistype) TYPE_CPLUS_SPECIFIC(thistype)->nfn_fields_total
#define	TYPE_TYPE_SPECIFIC(thistype) (thistype)->type_specific
#define TYPE_ARG_TYPES(thistype) (thistype)->type_specific.arg_types
#define TYPE_CPLUS_SPECIFIC(thistype) (thistype)->type_specific.cplus_stuff
#define TYPE_BASECLASS(thistype,index) (thistype)->fields[index].type
#define TYPE_N_BASECLASSES(thistype) TYPE_CPLUS_SPECIFIC(thistype)->n_baseclasses
#define TYPE_BASECLASS_NAME(thistype,index) (thistype)->fields[index].name
#define TYPE_BASECLASS_BITPOS(thistype,index) (thistype)->fields[index].bitpos
#define BASETYPE_VIA_PUBLIC(thistype, index) (!TYPE_FIELD_PRIVATE(thistype, index))
#define BASETYPE_VIA_VIRTUAL(thistype, index) \
  B_TST(TYPE_CPLUS_SPECIFIC(thistype)->virtual_field_bits, (index))

#define TYPE_FIELD(thistype, n) (thistype)->fields[n]
#define TYPE_FIELD_TYPE(thistype, n) (thistype)->fields[n].type
#define TYPE_FIELD_NAME(thistype, n) (thistype)->fields[n].name
#define TYPE_FIELD_VALUE(thistype, n) (* (int*) &(thistype)->fields[n].type)
#define TYPE_FIELD_BITPOS(thistype, n) (thistype)->fields[n].bitpos
#define TYPE_FIELD_BITSIZE(thistype, n) (thistype)->fields[n].bitsize
#define TYPE_FIELD_PACKED(thistype, n) (thistype)->fields[n].bitsize

#define TYPE_FIELD_PRIVATE_BITS(thistype) \
  TYPE_CPLUS_SPECIFIC(thistype)->private_field_bits
#define TYPE_FIELD_PROTECTED_BITS(thistype) \
  TYPE_CPLUS_SPECIFIC(thistype)->protected_field_bits
#define TYPE_FIELD_VIRTUAL_BITS(thistype) \
  TYPE_CPLUS_SPECIFIC(thistype)->virtual_field_bits
#define SET_TYPE_FIELD_PRIVATE(thistype, n) \
  B_SET (TYPE_CPLUS_SPECIFIC(thistype)->private_field_bits, (n))
#define SET_TYPE_FIELD_PROTECTED(thistype, n) \
  B_SET (TYPE_CPLUS_SPECIFIC(thistype)->protected_field_bits, (n))
#define SET_TYPE_FIELD_VIRTUAL(thistype, n) \
  B_SET (TYPE_CPLUS_SPECIFIC(thistype)->virtual_field_bits, (n))
#define TYPE_FIELD_PRIVATE(thistype, n) \
  (TYPE_CPLUS_SPECIFIC(thistype)->private_field_bits == NULL ? 0 \
    : B_TST(TYPE_CPLUS_SPECIFIC(thistype)->private_field_bits, (n)))
#define TYPE_FIELD_PROTECTED(thistype, n) \
  (TYPE_CPLUS_SPECIFIC(thistype)->protected_field_bits == NULL ? 0 \
    : B_TST(TYPE_CPLUS_SPECIFIC(thistype)->protected_field_bits, (n)))
#define TYPE_FIELD_VIRTUAL(thistype, n) \
       B_TST(TYPE_CPLUS_SPECIFIC(thistype)->virtual_field_bits, (n))

#define TYPE_FIELD_STATIC(thistype, n) ((thistype)->fields[n].bitpos == -1)
#define TYPE_FIELD_STATIC_PHYSNAME(thistype, n) ((char *)(thistype)->fields[n].bitsize)

#define TYPE_FN_FIELDLISTS(thistype) TYPE_CPLUS_SPECIFIC(thistype)->fn_fieldlists
#define TYPE_FN_FIELDLIST(thistype, n) TYPE_CPLUS_SPECIFIC(thistype)->fn_fieldlists[n]
#define TYPE_FN_FIELDLIST1(thistype, n) TYPE_CPLUS_SPECIFIC(thistype)->fn_fieldlists[n].fn_fields
#define TYPE_FN_FIELDLIST_NAME(thistype, n) TYPE_CPLUS_SPECIFIC(thistype)->fn_fieldlists[n].name
#define TYPE_FN_FIELDLIST_LENGTH(thistype, n) TYPE_CPLUS_SPECIFIC(thistype)->fn_fieldlists[n].length

#define TYPE_FN_FIELD(thisfn, n) (thisfn)[n]
#define TYPE_FN_FIELD_PHYSNAME(thisfn, n) (thisfn)[n].physname
#define TYPE_FN_FIELD_TYPE(thisfn, n) (thisfn)[n].type
#define TYPE_FN_FIELD_ARGS(thisfn, n) TYPE_ARG_TYPES ((thisfn)[n].type)
#define TYPE_FN_FIELD_CONST(thisfn, n) ((thisfn)[n].is_const)
#define TYPE_FN_FIELD_VOLATILE(thisfn, n) ((thisfn)[n].is_volatile)
#define TYPE_FN_FIELD_PRIVATE(thisfn, n) ((thisfn)[n].is_private)
#define TYPE_FN_FIELD_PROTECTED(thisfn, n) ((thisfn)[n].is_protected)
#define TYPE_FN_FIELD_STUB(thisfn, n) ((thisfn)[n].is_stub)
#define TYPE_FN_FIELD_FCONTEXT(thisfn, n) ((thisfn)[n].fcontext)
#define TYPE_FN_FIELD_VOFFSET(thisfn, n) ((thisfn)[n].voffset-2)
#define TYPE_FN_FIELD_VIRTUAL_P(thisfn, n) ((thisfn)[n].voffset > 1)
#define TYPE_FN_FIELD_STATIC_P(thisfn, n) ((thisfn)[n].voffset == VOFFSET_STATIC)

extern struct type *builtin_type_void;
extern struct type *builtin_type_char;
extern struct type *builtin_type_short;
extern struct type *builtin_type_int;
extern struct type *builtin_type_long;
extern struct type *builtin_type_signed_char;
extern struct type *builtin_type_unsigned_char;
extern struct type *builtin_type_unsigned_short;
extern struct type *builtin_type_unsigned_int;
extern struct type *builtin_type_unsigned_long;
extern struct type *builtin_type_float;
extern struct type *builtin_type_double;
extern struct type *builtin_type_long_double;
extern struct type *builtin_type_complex;
extern struct type *builtin_type_double_complex;
extern struct type *builtin_type_string;

/* This type represents a type that was unrecognized in symbol
   read-in.  */

extern struct type *builtin_type_error;

extern struct type *builtin_type_long_long;
extern struct type *builtin_type_unsigned_long_long;

/* Modula-2 types */

extern struct type *builtin_type_m2_char;
extern struct type *builtin_type_m2_int;
extern struct type *builtin_type_m2_card;
extern struct type *builtin_type_m2_real;
extern struct type *builtin_type_m2_bool;

/* Chill types */

extern struct type *builtin_type_chill_bool;
extern struct type *builtin_type_chill_char;
extern struct type *builtin_type_chill_long;
extern struct type *builtin_type_chill_ulong;
extern struct type *builtin_type_chill_real;

/* CC_HAS_LONG_LONG is defined if the host has "long long".  */

#ifdef CC_HAS_LONG_LONG

#define BUILTIN_TYPE_LONGEST builtin_type_long_long
#define BUILTIN_TYPE_UNSIGNED_LONGEST builtin_type_unsigned_long_long

#else /* not CC_HAS_LONG_LONG.  */

#define BUILTIN_TYPE_LONGEST builtin_type_long
#define BUILTIN_TYPE_UNSIGNED_LONGEST builtin_type_unsigned_long

#endif /* not CC_HAS_LONG_LONG.  */

/* Maximum and minimum values of built-in types */

#define	MAX_OF_TYPE(t)	\
   TYPE_UNSIGNED(t) ? UMAX_OF_SIZE(TYPE_LENGTH(t)) \
    : MAX_OF_SIZE(TYPE_LENGTH(t))

#define MIN_OF_TYPE(t)	\
   TYPE_UNSIGNED(t) ? UMIN_OF_SIZE(TYPE_LENGTH(t)) \
    : MIN_OF_SIZE(TYPE_LENGTH(t))

/* Allocate space for storing data associated with a particular type.
   We ensure that the space is allocated using the same mechanism that
   was used to allocate the space for the type structure itself.  I.E.
   if the type is on an objfile's type_obstack, then the space for data
   associated with that type will also be allocated on the type_obstack.
   If the type is not associated with any particular objfile (such as
   builtin types), then the data space will be allocated with xmalloc,
   the same as for the type structure. */

#define TYPE_ALLOC(t,size)  \
   (TYPE_OBJFILE (t) != NULL  \
    ? obstack_alloc (&TYPE_OBJFILE (t) -> type_obstack, size) \
    : xmalloc (size))

extern struct type *
alloc_type PARAMS ((struct objfile *));

extern struct type *
init_type PARAMS ((enum type_code, int, int, char *, struct objfile *));

extern struct type *
lookup_reference_type PARAMS ((struct type *));

extern struct type *
make_reference_type PARAMS ((struct type *, struct type **));

extern struct type *
lookup_member_type PARAMS ((struct type *, struct type *));

extern void
smash_to_method_type PARAMS ((struct type *, struct type *, struct type *,
			      struct type **));

extern void
smash_to_member_type PARAMS ((struct type *, struct type *, struct type *));

extern struct type *
allocate_stub_method PARAMS ((struct type *));

extern char *
type_name_no_tag PARAMS ((const struct type *));

extern struct type *
lookup_struct_elt_type PARAMS ((struct type *, char *, int));

extern struct type *
make_pointer_type PARAMS ((struct type *, struct type **));

extern struct type *
lookup_pointer_type PARAMS ((struct type *));

extern struct type *
make_function_type PARAMS ((struct type *, struct type **));

extern struct type *
lookup_function_type PARAMS ((struct type *));

extern struct type *
create_range_type PARAMS ((struct type *, struct type *, int, int));

extern struct type *
create_array_type PARAMS ((struct type *, struct type *, struct type *));

extern struct type *
create_string_type PARAMS ((struct type *, struct type *));

extern struct type *
lookup_unsigned_typename PARAMS ((char *));

extern struct type *
lookup_signed_typename PARAMS ((char *));

extern void
check_stub_type PARAMS ((struct type *));

extern void
check_stub_method PARAMS ((struct type *, int, int));

extern struct type *
lookup_primitive_typename PARAMS ((char *));

extern char *
gdb_mangle_name PARAMS ((struct type *, int, int));

extern struct type *
builtin_type PARAMS ((char **));

extern struct type *
lookup_typename PARAMS ((char *, struct block *, int));

extern struct type *
lookup_template_type PARAMS ((char *, struct type *, struct block *));

extern struct type *
lookup_fundamental_type PARAMS ((struct objfile *, int));

extern void
fill_in_vptr_fieldno PARAMS ((struct type *));

#if MAINTENANCE_CMDS
extern void recursive_dump_type PARAMS ((struct type *, int));
#endif

/* printcmd.c */

extern void
print_scalar_formatted PARAMS ((char *, struct type *, int, int, FILE *));

#if MAINTENANCE_CMDS
extern void maintenance_print_type PARAMS ((char *, int));
#endif

#endif	/* GDBTYPES_H */
