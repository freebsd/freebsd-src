/* Support routines for decoding "stabs" debugging information format.
   Copyright 1986, 1987, 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996
             Free Software Foundation, Inc.

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

/* Support routines for reading and decoding debugging information in
   the "stabs" format.  This format is used with many systems that use
   the a.out object file format, as well as some systems that use
   COFF or ELF where the stabs data is placed in a special section.
   Avoid placing any object file format specific code in this file. */

#include "defs.h"
#include "gdb_string.h"
#include "bfd.h"
#include "obstack.h"
#include "symtab.h"
#include "gdbtypes.h"
#include "expression.h"
#include "symfile.h"
#include "objfiles.h"
#include "aout/stab_gnu.h"	/* We always use GNU stabs, not native */
#include "libaout.h"
#include "aout/aout64.h"
#include "gdb-stabs.h"
#include "buildsym.h"
#include "complaints.h"
#include "demangle.h"
#include "language.h"

#include <ctype.h>

/* Ask stabsread.h to define the vars it normally declares `extern'.  */
#define	EXTERN	/**/
#include "stabsread.h"		/* Our own declarations */
#undef	EXTERN

/* The routines that read and process a complete stabs for a C struct or 
   C++ class pass lists of data member fields and lists of member function
   fields in an instance of a field_info structure, as defined below.
   This is part of some reorganization of low level C++ support and is
   expected to eventually go away... (FIXME) */

struct field_info
{
  struct nextfield
    {
      struct nextfield *next;

      /* This is the raw visibility from the stab.  It is not checked
	 for being one of the visibilities we recognize, so code which
	 examines this field better be able to deal.  */
      int visibility;

      struct field field;
    } *list;
  struct next_fnfieldlist
    {
      struct next_fnfieldlist *next;
      struct fn_fieldlist fn_fieldlist;
    } *fnlist;
};

static struct type *
dbx_alloc_type PARAMS ((int [2], struct objfile *));

static long read_huge_number PARAMS ((char **, int, int *));

static struct type *error_type PARAMS ((char **, struct objfile *));

static void
patch_block_stabs PARAMS ((struct pending *, struct pending_stabs *,
			   struct objfile *));

static void
fix_common_block PARAMS ((struct symbol *, int));

static int
read_type_number PARAMS ((char **, int *));

static struct type *
read_range_type PARAMS ((char **, int [2], struct objfile *));

static struct type *
read_sun_builtin_type PARAMS ((char **, int [2], struct objfile *));

static struct type *
read_sun_floating_type PARAMS ((char **, int [2], struct objfile *));

static struct type *
read_enum_type PARAMS ((char **, struct type *, struct objfile *));

static struct type *
rs6000_builtin_type PARAMS ((int));

static int
read_member_functions PARAMS ((struct field_info *, char **, struct type *,
			       struct objfile *));

static int
read_struct_fields PARAMS ((struct field_info *, char **, struct type *,
			    struct objfile *));

static int
read_baseclasses PARAMS ((struct field_info *, char **, struct type *,
			  struct objfile *));

static int
read_tilde_fields PARAMS ((struct field_info *, char **, struct type *,
			   struct objfile *));

static int
attach_fn_fields_to_type PARAMS ((struct field_info *, struct type *));

static int
attach_fields_to_type PARAMS ((struct field_info *, struct type *,
			       struct objfile *));

static struct type *
read_struct_type PARAMS ((char **, struct type *, struct objfile *));

static struct type *
read_array_type PARAMS ((char **, struct type *, struct objfile *));

static struct type **
read_args PARAMS ((char **, int, struct objfile *));

static int
read_cpp_abbrev PARAMS ((struct field_info *, char **, struct type *,
			 struct objfile *));

static const char vptr_name[] = { '_','v','p','t','r',CPLUS_MARKER,'\0' };
static const char vb_name[] =   { '_','v','b',CPLUS_MARKER,'\0' };

/* Define this as 1 if a pcc declaration of a char or short argument
   gives the correct address.  Otherwise assume pcc gives the
   address of the corresponding int, which is not the same on a
   big-endian machine.  */

#ifndef BELIEVE_PCC_PROMOTION
#define BELIEVE_PCC_PROMOTION 0
#endif

struct complaint invalid_cpp_abbrev_complaint =
  {"invalid C++ abbreviation `%s'", 0, 0};

struct complaint invalid_cpp_type_complaint =
  {"C++ abbreviated type name unknown at symtab pos %d", 0, 0};

struct complaint member_fn_complaint =
  {"member function type missing, got '%c'", 0, 0};

struct complaint const_vol_complaint =
  {"const/volatile indicator missing, got '%c'", 0, 0};

struct complaint error_type_complaint =
  {"debug info mismatch between compiler and debugger", 0, 0};

struct complaint invalid_member_complaint =
  {"invalid (minimal) member type data format at symtab pos %d.", 0, 0};

struct complaint range_type_base_complaint =
  {"base type %d of range type is not defined", 0, 0};

struct complaint reg_value_complaint =
  {"register number %d too large (max %d) in symbol %s", 0, 0};

struct complaint vtbl_notfound_complaint =
  {"virtual function table pointer not found when defining class `%s'", 0, 0};

struct complaint unrecognized_cplus_name_complaint =
  {"Unknown C++ symbol name `%s'", 0, 0};

struct complaint rs6000_builtin_complaint =
  {"Unknown builtin type %d", 0, 0};

struct complaint unresolved_sym_chain_complaint =
  {"%s: common block `%s' from global_sym_chain unresolved", 0, 0};

struct complaint stabs_general_complaint =
  {"%s", 0, 0};

/* Make a list of forward references which haven't been defined.  */

static struct type **undef_types;
static int undef_types_allocated;
static int undef_types_length;
static struct symbol *current_symbol = NULL;

/* Check for and handle cretinous stabs symbol name continuation!  */
#define STABS_CONTINUE(pp,objfile)				\
  do {							\
    if (**(pp) == '\\' || (**(pp) == '?' && (*(pp))[1] == '\0')) \
      *(pp) = next_symbol_text (objfile);	\
  } while (0)

/* FIXME: These probably should be our own types (like rs6000_builtin_type
   has its own types) rather than builtin_type_*.  */
static struct type **os9k_type_vector[] = {
	0,
	&builtin_type_int,
	&builtin_type_char,
	&builtin_type_long,
	&builtin_type_short,
	&builtin_type_unsigned_char,
	&builtin_type_unsigned_short,
	&builtin_type_unsigned_long,
	&builtin_type_unsigned_int,
	&builtin_type_float,
	&builtin_type_double,
	&builtin_type_void,
	&builtin_type_long_double
};

static void os9k_init_type_vector PARAMS ((struct type **));

static void
os9k_init_type_vector(tv)
    struct type **tv;
{
  int i;
  for (i=0; i<sizeof(os9k_type_vector)/sizeof(struct type **); i++)
    tv[i] = (os9k_type_vector[i] == 0 ? 0 : *(os9k_type_vector[i]));
}

/* Look up a dbx type-number pair.  Return the address of the slot
   where the type for that number-pair is stored.
   The number-pair is in TYPENUMS.

   This can be used for finding the type associated with that pair
   or for associating a new type with the pair.  */

struct type **
dbx_lookup_type (typenums)
     int typenums[2];
{
  register int filenum = typenums[0];
  register int index = typenums[1];
  unsigned old_len;
  register int real_filenum;
  register struct header_file *f;
  int f_orig_length;

  if (filenum == -1)		/* -1,-1 is for temporary types.  */
    return 0;

  if (filenum < 0 || filenum >= n_this_object_header_files)
    {
      static struct complaint msg = {"\
Invalid symbol data: type number (%d,%d) out of range at symtab pos %d.",
				0, 0};
      complain (&msg, filenum, index, symnum);
      goto error_return;
    }

  if (filenum == 0)
    {
      if (index < 0)
	{
	  /* Caller wants address of address of type.  We think
	     that negative (rs6k builtin) types will never appear as
	     "lvalues", (nor should they), so we stuff the real type
	     pointer into a temp, and return its address.  If referenced,
	     this will do the right thing.  */
	  static struct type *temp_type;

	  temp_type = rs6000_builtin_type(index);
	  return &temp_type;
	}

      /* Type is defined outside of header files.
	 Find it in this object file's type vector.  */
      if (index >= type_vector_length)
	{
	  old_len = type_vector_length;
	  if (old_len == 0)
	    {
	      type_vector_length = INITIAL_TYPE_VECTOR_LENGTH;
	      type_vector = (struct type **)
		xmalloc (type_vector_length * sizeof (struct type *));
	    }
	  while (index >= type_vector_length)
	    {
	      type_vector_length *= 2;
	    }
	  type_vector = (struct type **)
	    xrealloc ((char *) type_vector,
		      (type_vector_length * sizeof (struct type *)));
	  memset (&type_vector[old_len], 0,
		  (type_vector_length - old_len) * sizeof (struct type *));

	  if (os9k_stabs)
	    /* Deal with OS9000 fundamental types.  */
	    os9k_init_type_vector (type_vector);
	}
      return (&type_vector[index]);
    }
  else
    {
      real_filenum = this_object_header_files[filenum];

      if (real_filenum >= n_header_files)
	{
	  struct type *temp_type;
	  struct type **temp_type_p;

	  warning ("GDB internal error: bad real_filenum");

	error_return:
	  temp_type = init_type (TYPE_CODE_ERROR, 0, 0, NULL, NULL);
	  temp_type_p = (struct type **) xmalloc (sizeof (struct type *));
	  *temp_type_p = temp_type;
	  return temp_type_p;
	}

      f = &header_files[real_filenum];

      f_orig_length = f->length;
      if (index >= f_orig_length)
	{
	  while (index >= f->length)
	    {
	      f->length *= 2;
	    }
	  f->vector = (struct type **)
	    xrealloc ((char *) f->vector, f->length * sizeof (struct type *));
	  memset (&f->vector[f_orig_length], 0,
		  (f->length - f_orig_length) * sizeof (struct type *));
	}
      return (&f->vector[index]);
    }
}

/* Make sure there is a type allocated for type numbers TYPENUMS
   and return the type object.
   This can create an empty (zeroed) type object.
   TYPENUMS may be (-1, -1) to return a new type object that is not
   put into the type vector, and so may not be referred to by number. */

static struct type *
dbx_alloc_type (typenums, objfile)
     int typenums[2];
     struct objfile *objfile;
{
  register struct type **type_addr;

  if (typenums[0] == -1)
    {
      return (alloc_type (objfile));
    }

  type_addr = dbx_lookup_type (typenums);

  /* If we are referring to a type not known at all yet,
     allocate an empty type for it.
     We will fill it in later if we find out how.  */
  if (*type_addr == 0)
    {
      *type_addr = alloc_type (objfile);
    }

  return (*type_addr);
}

/* for all the stabs in a given stab vector, build appropriate types 
   and fix their symbols in given symbol vector. */

static void
patch_block_stabs (symbols, stabs, objfile)
     struct pending *symbols;
     struct pending_stabs *stabs;
     struct objfile *objfile;
{
  int ii;
  char *name;
  char *pp;
  struct symbol *sym;

  if (stabs)
    {
      
      /* for all the stab entries, find their corresponding symbols and 
	 patch their types! */
      
      for (ii = 0; ii < stabs->count; ++ii)
	{
	  name = stabs->stab[ii];
	  pp = (char*) strchr (name, ':');
	  while (pp[1] == ':')
	    {
	       pp += 2;
	       pp = (char *)strchr(pp, ':');
	    }
	  sym = find_symbol_in_list (symbols, name, pp-name);
	  if (!sym)
	    {
	      /* FIXME-maybe: it would be nice if we noticed whether
		 the variable was defined *anywhere*, not just whether
		 it is defined in this compilation unit.  But neither
		 xlc or GCC seem to need such a definition, and until
		 we do psymtabs (so that the minimal symbols from all
		 compilation units are available now), I'm not sure
		 how to get the information.  */

	      /* On xcoff, if a global is defined and never referenced,
		 ld will remove it from the executable.  There is then
		 a N_GSYM stab for it, but no regular (C_EXT) symbol.  */
	      sym = (struct symbol *)
		obstack_alloc (&objfile->symbol_obstack,
			       sizeof (struct symbol));

	      memset (sym, 0, sizeof (struct symbol));
	      SYMBOL_NAMESPACE (sym) = VAR_NAMESPACE;
	      SYMBOL_CLASS (sym) = LOC_OPTIMIZED_OUT;
	      SYMBOL_NAME (sym) =
		obstack_copy0 (&objfile->symbol_obstack, name, pp - name);
	      pp += 2;
	      if (*(pp-1) == 'F' || *(pp-1) == 'f')
		{
		  /* I don't think the linker does this with functions,
		     so as far as I know this is never executed.
		     But it doesn't hurt to check.  */
		  SYMBOL_TYPE (sym) =
		    lookup_function_type (read_type (&pp, objfile));
		}
	      else
		{
		  SYMBOL_TYPE (sym) = read_type (&pp, objfile);
		}
	      add_symbol_to_list (sym, &global_symbols);
	    }
	  else
	    {
	      pp += 2;
	      if (*(pp-1) == 'F' || *(pp-1) == 'f')
		{
		  SYMBOL_TYPE (sym) =
		    lookup_function_type (read_type (&pp, objfile));
		}
	      else
		{
		  SYMBOL_TYPE (sym) = read_type (&pp, objfile);
		}
	    }
	}
    }
}


/* Read a number by which a type is referred to in dbx data,
   or perhaps read a pair (FILENUM, TYPENUM) in parentheses.
   Just a single number N is equivalent to (0,N).
   Return the two numbers by storing them in the vector TYPENUMS.
   TYPENUMS will then be used as an argument to dbx_lookup_type.

   Returns 0 for success, -1 for error.  */

static int
read_type_number (pp, typenums)
     register char **pp;
     register int *typenums;
{
  int nbits;
  if (**pp == '(')
    {
      (*pp)++;
      typenums[0] = read_huge_number (pp, ',', &nbits);
      if (nbits != 0) return -1;
      typenums[1] = read_huge_number (pp, ')', &nbits);
      if (nbits != 0) return -1;
    }
  else
    {
      typenums[0] = 0;
      typenums[1] = read_huge_number (pp, 0, &nbits);
      if (nbits != 0) return -1;
    }
  return 0;
}


#if !defined (REG_STRUCT_HAS_ADDR)
#define REG_STRUCT_HAS_ADDR(gcc_p,type) 0
#endif

/* ARGSUSED */
struct symbol *
define_symbol (valu, string, desc, type, objfile)
     CORE_ADDR valu;
     char *string;
     int desc;
     int type;
     struct objfile *objfile;
{
  register struct symbol *sym;
  char *p = (char *) strchr (string, ':');
  int deftype;
  int synonym = 0;
  register int i;

  /* We would like to eliminate nameless symbols, but keep their types.
     E.g. stab entry ":t10=*2" should produce a type 10, which is a pointer
     to type 2, but, should not create a symbol to address that type. Since
     the symbol will be nameless, there is no way any user can refer to it. */

  int nameless;

  /* Ignore syms with empty names.  */
  if (string[0] == 0)
    return 0;

  /* Ignore old-style symbols from cc -go  */
  if (p == 0)
    return 0;

  while (p[1] == ':')
    {
       p += 2;
       p = strchr(p, ':');
    }

  /* If a nameless stab entry, all we need is the type, not the symbol.
     e.g. ":t10=*2" or a nameless enum like " :T16=ered:0,green:1,blue:2,;" */
  nameless = (p == string || ((string[0] == ' ') && (string[1] == ':')));

  current_symbol = sym = (struct symbol *) 
    obstack_alloc (&objfile -> symbol_obstack, sizeof (struct symbol));
  memset (sym, 0, sizeof (struct symbol));

  switch (type & N_TYPE)
    {
    case N_TEXT:
      SYMBOL_SECTION(sym) = SECT_OFF_TEXT;
      break;
    case N_DATA:
      SYMBOL_SECTION(sym) = SECT_OFF_DATA;
      break;
    case N_BSS:
      SYMBOL_SECTION(sym) = SECT_OFF_BSS;
      break;
    }

  if (processing_gcc_compilation)
    {
      /* GCC 2.x puts the line number in desc.  SunOS apparently puts in the
	 number of bytes occupied by a type or object, which we ignore.  */
      SYMBOL_LINE(sym) = desc;
    }
  else
    {
      SYMBOL_LINE(sym) = 0;			/* unknown */
    }

  if (is_cplus_marker (string[0]))
    {
      /* Special GNU C++ names.  */
      switch (string[1])
	{
	  case 't':
	    SYMBOL_NAME (sym) = obsavestring ("this", strlen ("this"),
					      &objfile -> symbol_obstack);
	    break;

	  case 'v': /* $vtbl_ptr_type */
	    /* Was: SYMBOL_NAME (sym) = "vptr"; */
	    goto normal;

	  case 'e':
	    SYMBOL_NAME (sym) = obsavestring ("eh_throw", strlen ("eh_throw"),
					      &objfile -> symbol_obstack);
	    break;

	  case '_':
	    /* This was an anonymous type that was never fixed up.  */
	    goto normal;

#ifdef STATIC_TRANSFORM_NAME
	  case 'X':
	    /* SunPRO (3.0 at least) static variable encoding.  */
	    goto normal;
#endif

	  default:
	    complain (&unrecognized_cplus_name_complaint, string);
	    goto normal;		/* Do *something* with it */
	}
    }
  else
    {
    normal:
      SYMBOL_LANGUAGE (sym) = current_subfile -> language;
      SYMBOL_NAME (sym)	= (char *)
	obstack_alloc (&objfile -> symbol_obstack, ((p - string) + 1));
      /* Open-coded memcpy--saves function call time.  */
      /* FIXME:  Does it really?  Try replacing with simple strcpy and
	 try it on an executable with a large symbol table. */
      /* FIXME: considering that gcc can open code memcpy anyway, I
	 doubt it.  xoxorich. */
      {
	register char *p1 = string;
	register char *p2 = SYMBOL_NAME (sym);
	while (p1 != p)
	  {
	    *p2++ = *p1++;
	  }
	*p2++ = '\0';
      }

      /* If this symbol is from a C++ compilation, then attempt to cache the
	 demangled form for future reference.  This is a typical time versus
	 space tradeoff, that was decided in favor of time because it sped up
	 C++ symbol lookups by a factor of about 20. */

      SYMBOL_INIT_DEMANGLED_NAME (sym, &objfile->symbol_obstack);
    }
  p++;

  /* Determine the type of name being defined.  */
#if 0
  /* Getting GDB to correctly skip the symbol on an undefined symbol
     descriptor and not ever dump core is a very dodgy proposition if
     we do things this way.  I say the acorn RISC machine can just
     fix their compiler.  */
  /* The Acorn RISC machine's compiler can put out locals that don't
     start with "234=" or "(3,4)=", so assume anything other than the
     deftypes we know how to handle is a local.  */
  if (!strchr ("cfFGpPrStTvVXCR", *p))
#else
  if (isdigit (*p) || *p == '(' || *p == '-')
#endif
    deftype = 'l';
  else
    deftype = *p++;

  switch (deftype)
    {
    case 'c':
      /* c is a special case, not followed by a type-number.
	 SYMBOL:c=iVALUE for an integer constant symbol.
	 SYMBOL:c=rVALUE for a floating constant symbol.
	 SYMBOL:c=eTYPE,INTVALUE for an enum constant symbol.
	 e.g. "b:c=e6,0" for "const b = blob1"
	 (where type 6 is defined by "blobs:t6=eblob1:0,blob2:1,;").  */
      if (*p != '=')
	{
	  SYMBOL_CLASS (sym) = LOC_CONST;
	  SYMBOL_TYPE (sym) = error_type (&p, objfile);
	  SYMBOL_NAMESPACE (sym) = VAR_NAMESPACE;
	  add_symbol_to_list (sym, &file_symbols);
	  return sym;
	}
      ++p;
      switch (*p++)
	{
	case 'r':
	  {
	    double d = atof (p);
	    char *dbl_valu;

	    /* FIXME-if-picky-about-floating-accuracy: Should be using
	       target arithmetic to get the value.  real.c in GCC
	       probably has the necessary code.  */

	    /* FIXME: lookup_fundamental_type is a hack.  We should be
	       creating a type especially for the type of float constants.
	       Problem is, what type should it be?

	       Also, what should the name of this type be?  Should we
	       be using 'S' constants (see stabs.texinfo) instead?  */

	    SYMBOL_TYPE (sym) = lookup_fundamental_type (objfile,
							 FT_DBL_PREC_FLOAT);
	    dbl_valu = (char *)
	      obstack_alloc (&objfile -> symbol_obstack,
			     TYPE_LENGTH (SYMBOL_TYPE (sym)));
	    store_floating (dbl_valu, TYPE_LENGTH (SYMBOL_TYPE (sym)), d);
	    SYMBOL_VALUE_BYTES (sym) = dbl_valu;
	    SYMBOL_CLASS (sym) = LOC_CONST_BYTES;
	  }
	  break;
	case 'i':
	  {
	    /* Defining integer constants this way is kind of silly,
	       since 'e' constants allows the compiler to give not
	       only the value, but the type as well.  C has at least
	       int, long, unsigned int, and long long as constant
	       types; other languages probably should have at least
	       unsigned as well as signed constants.  */

	    /* We just need one int constant type for all objfiles.
	       It doesn't depend on languages or anything (arguably its
	       name should be a language-specific name for a type of
	       that size, but I'm inclined to say that if the compiler
	       wants a nice name for the type, it can use 'e').  */
	    static struct type *int_const_type;

	    /* Yes, this is as long as a *host* int.  That is because we
	       use atoi.  */
	    if (int_const_type == NULL)
	      int_const_type =
		init_type (TYPE_CODE_INT,
			   sizeof (int) * HOST_CHAR_BIT / TARGET_CHAR_BIT, 0,
			   "integer constant",
			   (struct objfile *)NULL);
	    SYMBOL_TYPE (sym) = int_const_type;
	    SYMBOL_VALUE (sym) = atoi (p);
	    SYMBOL_CLASS (sym) = LOC_CONST;
	  }
	  break;
	case 'e':
	  /* SYMBOL:c=eTYPE,INTVALUE for a constant symbol whose value
	     can be represented as integral.
	     e.g. "b:c=e6,0" for "const b = blob1"
	     (where type 6 is defined by "blobs:t6=eblob1:0,blob2:1,;").  */
	  {
	    SYMBOL_CLASS (sym) = LOC_CONST;
	    SYMBOL_TYPE (sym) = read_type (&p, objfile);

	    if (*p != ',')
	      {
		SYMBOL_TYPE (sym) = error_type (&p, objfile);
		break;
	      }
	    ++p;

	    /* If the value is too big to fit in an int (perhaps because
	       it is unsigned), or something like that, we silently get
	       a bogus value.  The type and everything else about it is
	       correct.  Ideally, we should be using whatever we have
	       available for parsing unsigned and long long values,
	       however.  */
	    SYMBOL_VALUE (sym) = atoi (p);
	  }
	  break;
	default:
	  {
	    SYMBOL_CLASS (sym) = LOC_CONST;
	    SYMBOL_TYPE (sym) = error_type (&p, objfile);
	  }
	}
      SYMBOL_NAMESPACE (sym) = VAR_NAMESPACE;
      add_symbol_to_list (sym, &file_symbols);
      return sym;

    case 'C':
      /* The name of a caught exception.  */
      SYMBOL_TYPE (sym) = read_type (&p, objfile);
      SYMBOL_CLASS (sym) = LOC_LABEL;
      SYMBOL_NAMESPACE (sym) = VAR_NAMESPACE;
      SYMBOL_VALUE_ADDRESS (sym) = valu;
      add_symbol_to_list (sym, &local_symbols);
      break;

    case 'f':
      /* A static function definition.  */
      SYMBOL_TYPE (sym) = read_type (&p, objfile);
      SYMBOL_CLASS (sym) = LOC_BLOCK;
      SYMBOL_NAMESPACE (sym) = VAR_NAMESPACE;
      add_symbol_to_list (sym, &file_symbols);
      /* fall into process_function_types.  */

    process_function_types:
      /* Function result types are described as the result type in stabs.
	 We need to convert this to the function-returning-type-X type
	 in GDB.  E.g. "int" is converted to "function returning int".  */
      if (TYPE_CODE (SYMBOL_TYPE (sym)) != TYPE_CODE_FUNC)
	SYMBOL_TYPE (sym) = lookup_function_type (SYMBOL_TYPE (sym));
      /* fall into process_prototype_types */

    process_prototype_types:
      /* Sun acc puts declared types of arguments here.  We don't care
	 about their actual types (FIXME -- we should remember the whole
	 function prototype), but the list may define some new types
	 that we have to remember, so we must scan it now.  */
      while (*p == ';') {
	p++;
	read_type (&p, objfile);
      }
      break;

    case 'F':
      /* A global function definition.  */
      SYMBOL_TYPE (sym) = read_type (&p, objfile);
      SYMBOL_CLASS (sym) = LOC_BLOCK;
      SYMBOL_NAMESPACE (sym) = VAR_NAMESPACE;
      add_symbol_to_list (sym, &global_symbols);
      goto process_function_types;

    case 'G':
      /* For a class G (global) symbol, it appears that the
	 value is not correct.  It is necessary to search for the
	 corresponding linker definition to find the value.
	 These definitions appear at the end of the namelist.  */
      SYMBOL_TYPE (sym) = read_type (&p, objfile);
      i = hashname (SYMBOL_NAME (sym));
      SYMBOL_VALUE_CHAIN (sym) = global_sym_chain[i];
      global_sym_chain[i] = sym;
      SYMBOL_CLASS (sym) = LOC_STATIC;
      SYMBOL_NAMESPACE (sym) = VAR_NAMESPACE;
      add_symbol_to_list (sym, &global_symbols);
      break;

      /* This case is faked by a conditional above,
	 when there is no code letter in the dbx data.
	 Dbx data never actually contains 'l'.  */
    case 's':
    case 'l':
      SYMBOL_TYPE (sym) = read_type (&p, objfile);
      SYMBOL_CLASS (sym) = LOC_LOCAL;
      SYMBOL_VALUE (sym) = valu;
      SYMBOL_NAMESPACE (sym) = VAR_NAMESPACE;
      add_symbol_to_list (sym, &local_symbols);
      break;

    case 'p':
      if (*p == 'F')
	/* pF is a two-letter code that means a function parameter in Fortran.
	   The type-number specifies the type of the return value.
	   Translate it into a pointer-to-function type.  */
	{
	  p++;
	  SYMBOL_TYPE (sym)
	    = lookup_pointer_type
	      (lookup_function_type (read_type (&p, objfile)));
	}
      else
	SYMBOL_TYPE (sym) = read_type (&p, objfile);

      /* Normally this is a parameter, a LOC_ARG.  On the i960, it
	 can also be a LOC_LOCAL_ARG depending on symbol type.  */
#ifndef DBX_PARM_SYMBOL_CLASS
#define	DBX_PARM_SYMBOL_CLASS(type)	LOC_ARG
#endif

      SYMBOL_CLASS (sym) = DBX_PARM_SYMBOL_CLASS (type);
      SYMBOL_VALUE (sym) = valu;
      SYMBOL_NAMESPACE (sym) = VAR_NAMESPACE;
      add_symbol_to_list (sym, &local_symbols);

      if (TARGET_BYTE_ORDER != BIG_ENDIAN)
	{
	  /* On little-endian machines, this crud is never necessary,
	     and, if the extra bytes contain garbage, is harmful.  */
	  break;
	}

      /* If it's gcc-compiled, if it says `short', believe it.  */
      if (processing_gcc_compilation || BELIEVE_PCC_PROMOTION)
	break;

#if !BELIEVE_PCC_PROMOTION
      {
	/* This is the signed type which arguments get promoted to.  */
	static struct type *pcc_promotion_type;
	/* This is the unsigned type which arguments get promoted to.  */
	static struct type *pcc_unsigned_promotion_type;

	/* Call it "int" because this is mainly C lossage.  */
	if (pcc_promotion_type == NULL)
	  pcc_promotion_type =
	    init_type (TYPE_CODE_INT, TARGET_INT_BIT / TARGET_CHAR_BIT,
		       0, "int", NULL);

	if (pcc_unsigned_promotion_type == NULL)
	  pcc_unsigned_promotion_type =
	    init_type (TYPE_CODE_INT, TARGET_INT_BIT / TARGET_CHAR_BIT,
		       TYPE_FLAG_UNSIGNED, "unsigned int", NULL);

#if defined(BELIEVE_PCC_PROMOTION_TYPE)
	/* This macro is defined on machines (e.g. sparc) where
	   we should believe the type of a PCC 'short' argument,
	   but shouldn't believe the address (the address is
	   the address of the corresponding int).
	   
	   My guess is that this correction, as opposed to changing
	   the parameter to an 'int' (as done below, for PCC
	   on most machines), is the right thing to do
	   on all machines, but I don't want to risk breaking
	   something that already works.  On most PCC machines,
	   the sparc problem doesn't come up because the calling
	   function has to zero the top bytes (not knowing whether
	   the called function wants an int or a short), so there
	   is little practical difference between an int and a short
	   (except perhaps what happens when the GDB user types
	   "print short_arg = 0x10000;"). 
	   
	   Hacked for SunOS 4.1 by gnu@cygnus.com.  In 4.1, the compiler
	   actually produces the correct address (we don't need to fix it
	   up).  I made this code adapt so that it will offset the symbol
	   if it was pointing at an int-aligned location and not
	   otherwise.  This way you can use the same gdb for 4.0.x and
	   4.1 systems.
	   
	   If the parameter is shorter than an int, and is integral
	   (e.g. char, short, or unsigned equivalent), and is claimed to
	   be passed on an integer boundary, don't believe it!  Offset the
	   parameter's address to the tail-end of that integer.  */
	
	if (TYPE_LENGTH (SYMBOL_TYPE (sym)) < TYPE_LENGTH (pcc_promotion_type)
	    && TYPE_CODE (SYMBOL_TYPE (sym)) == TYPE_CODE_INT
	    && 0 == SYMBOL_VALUE (sym) % TYPE_LENGTH (pcc_promotion_type))
	  {
	    SYMBOL_VALUE (sym) += TYPE_LENGTH (pcc_promotion_type)
	      - TYPE_LENGTH (SYMBOL_TYPE (sym));
	  }
	break;
	
#else /* no BELIEVE_PCC_PROMOTION_TYPE.  */

	/* If PCC says a parameter is a short or a char,
	   it is really an int.  */
	if (TYPE_LENGTH (SYMBOL_TYPE (sym)) < TYPE_LENGTH (pcc_promotion_type)
	    && TYPE_CODE (SYMBOL_TYPE (sym)) == TYPE_CODE_INT)
	  {
	    SYMBOL_TYPE (sym) =
	      TYPE_UNSIGNED (SYMBOL_TYPE (sym))
		? pcc_unsigned_promotion_type
		: pcc_promotion_type;
	  }
	break;

#endif /* no BELIEVE_PCC_PROMOTION_TYPE.  */
      }
#endif /* !BELIEVE_PCC_PROMOTION.  */

    case 'P':
      /* acc seems to use P to declare the prototypes of functions that
         are referenced by this file.  gdb is not prepared to deal
         with this extra information.  FIXME, it ought to.  */
      if (type == N_FUN)
	{
	  SYMBOL_TYPE (sym) = read_type (&p, objfile);
	  goto process_prototype_types;
	}
      /*FALLTHROUGH*/

    case 'R':
      /* Parameter which is in a register.  */
      SYMBOL_TYPE (sym) = read_type (&p, objfile);
      SYMBOL_CLASS (sym) = LOC_REGPARM;
      SYMBOL_VALUE (sym) = STAB_REG_TO_REGNUM (valu);
      if (SYMBOL_VALUE (sym) >= NUM_REGS)
	{
	  complain (&reg_value_complaint, SYMBOL_VALUE (sym), NUM_REGS,
		    SYMBOL_SOURCE_NAME (sym));
	  SYMBOL_VALUE (sym) = SP_REGNUM;  /* Known safe, though useless */
	}
      SYMBOL_NAMESPACE (sym) = VAR_NAMESPACE;
      add_symbol_to_list (sym, &local_symbols);
      break;

    case 'r':
      /* Register variable (either global or local).  */
      SYMBOL_TYPE (sym) = read_type (&p, objfile);
      SYMBOL_CLASS (sym) = LOC_REGISTER;
      SYMBOL_VALUE (sym) = STAB_REG_TO_REGNUM (valu);
      if (SYMBOL_VALUE (sym) >= NUM_REGS)
	{
	  complain (&reg_value_complaint, SYMBOL_VALUE (sym), NUM_REGS,
		    SYMBOL_SOURCE_NAME (sym));
	  SYMBOL_VALUE (sym) = SP_REGNUM;  /* Known safe, though useless */
	}
      SYMBOL_NAMESPACE (sym) = VAR_NAMESPACE;
      if (within_function)
	{
	  /* Sun cc uses a pair of symbols, one 'p' and one 'r' with the same
	     name to represent an argument passed in a register.
	     GCC uses 'P' for the same case.  So if we find such a symbol pair
	     we combine it into one 'P' symbol.  For Sun cc we need to do this
	     regardless of REG_STRUCT_HAS_ADDR, because the compiler puts out
	     the 'p' symbol even if it never saves the argument onto the stack.

	     On most machines, we want to preserve both symbols, so that
	     we can still get information about what is going on with the
	     stack (VAX for computing args_printed, using stack slots instead
	     of saved registers in backtraces, etc.).

	     Note that this code illegally combines
	       main(argc) struct foo argc; { register struct foo argc; }
	     but this case is considered pathological and causes a warning
	     from a decent compiler.  */

	  if (local_symbols
	      && local_symbols->nsyms > 0
#ifndef USE_REGISTER_NOT_ARG
	      && REG_STRUCT_HAS_ADDR (processing_gcc_compilation,
				      SYMBOL_TYPE (sym))
	      && (TYPE_CODE (SYMBOL_TYPE (sym)) == TYPE_CODE_STRUCT
		  || TYPE_CODE (SYMBOL_TYPE (sym)) == TYPE_CODE_UNION
		  || TYPE_CODE (SYMBOL_TYPE (sym)) == TYPE_CODE_SET
		  || TYPE_CODE (SYMBOL_TYPE (sym)) == TYPE_CODE_BITSTRING)
#endif
	      )
	    {
	      struct symbol *prev_sym;
	      prev_sym = local_symbols->symbol[local_symbols->nsyms - 1];
	      if ((SYMBOL_CLASS (prev_sym) == LOC_REF_ARG
		   || SYMBOL_CLASS (prev_sym) == LOC_ARG)
		  && STREQ (SYMBOL_NAME (prev_sym), SYMBOL_NAME(sym)))
		{
		  SYMBOL_CLASS (prev_sym) = LOC_REGPARM;
		  /* Use the type from the LOC_REGISTER; that is the type
		     that is actually in that register.  */
		  SYMBOL_TYPE (prev_sym) = SYMBOL_TYPE (sym);
		  SYMBOL_VALUE (prev_sym) = SYMBOL_VALUE (sym);
		  sym = prev_sym;
		  break;
		}
	    }
          add_symbol_to_list (sym, &local_symbols);
	}
      else
        add_symbol_to_list (sym, &file_symbols);
      break;

    case 'S':
      /* Static symbol at top level of file */
      SYMBOL_TYPE (sym) = read_type (&p, objfile);
      SYMBOL_CLASS (sym) = LOC_STATIC;
      SYMBOL_VALUE_ADDRESS (sym) = valu;
#ifdef STATIC_TRANSFORM_NAME
      if (SYMBOL_NAME (sym)[0] == '$')
      {
	struct minimal_symbol *msym;
	msym = lookup_minimal_symbol (SYMBOL_NAME (sym), NULL, objfile);
	if (msym != NULL)
	  {
	    SYMBOL_NAME (sym) = STATIC_TRANSFORM_NAME (SYMBOL_NAME (sym));
	    SYMBOL_VALUE_ADDRESS (sym) = SYMBOL_VALUE_ADDRESS (msym);
	  }
      }
#endif
      SYMBOL_NAMESPACE (sym) = VAR_NAMESPACE;
      add_symbol_to_list (sym, &file_symbols);
      break;

    case 't':
      SYMBOL_TYPE (sym) = read_type (&p, objfile);

      /* For a nameless type, we don't want a create a symbol, thus we
	 did not use `sym'. Return without further processing. */
      if (nameless) return NULL;

      SYMBOL_CLASS (sym) = LOC_TYPEDEF;
      SYMBOL_VALUE (sym) = valu;
      SYMBOL_NAMESPACE (sym) = VAR_NAMESPACE;
      /* C++ vagaries: we may have a type which is derived from
	 a base type which did not have its name defined when the
	 derived class was output.  We fill in the derived class's
	 base part member's name here in that case.  */
      if (TYPE_NAME (SYMBOL_TYPE (sym)) != NULL)
	if ((TYPE_CODE (SYMBOL_TYPE (sym)) == TYPE_CODE_STRUCT
	     || TYPE_CODE (SYMBOL_TYPE (sym)) == TYPE_CODE_UNION)
	    && TYPE_N_BASECLASSES (SYMBOL_TYPE (sym)))
	  {
	    int j;
	    for (j = TYPE_N_BASECLASSES (SYMBOL_TYPE (sym)) - 1; j >= 0; j--)
	      if (TYPE_BASECLASS_NAME (SYMBOL_TYPE (sym), j) == 0)
		TYPE_BASECLASS_NAME (SYMBOL_TYPE (sym), j) =
		  type_name_no_tag (TYPE_BASECLASS (SYMBOL_TYPE (sym), j));
	  }

      if (TYPE_NAME (SYMBOL_TYPE (sym)) == NULL)
	{
	  /* gcc-2.6 or later (when using -fvtable-thunks)
	     emits a unique named type for a vtable entry.
	     Some gdb code depends on that specific name. */
	  extern const char vtbl_ptr_name[];

	  if ((TYPE_CODE (SYMBOL_TYPE (sym)) == TYPE_CODE_PTR
	       && strcmp (SYMBOL_NAME (sym), vtbl_ptr_name))
	      || TYPE_CODE (SYMBOL_TYPE (sym)) == TYPE_CODE_FUNC)
	    {
	      /* If we are giving a name to a type such as "pointer to
		 foo" or "function returning foo", we better not set
		 the TYPE_NAME.  If the program contains "typedef char
		 *caddr_t;", we don't want all variables of type char
		 * to print as caddr_t.  This is not just a
		 consequence of GDB's type management; PCC and GCC (at
		 least through version 2.4) both output variables of
		 either type char * or caddr_t with the type number
		 defined in the 't' symbol for caddr_t.  If a future
		 compiler cleans this up it GDB is not ready for it
		 yet, but if it becomes ready we somehow need to
		 disable this check (without breaking the PCC/GCC2.4
		 case).

		 Sigh.

		 Fortunately, this check seems not to be necessary
		 for anything except pointers or functions.  */
	    }
	  else
	    TYPE_NAME (SYMBOL_TYPE (sym)) = SYMBOL_NAME (sym);
	}

      add_symbol_to_list (sym, &file_symbols);
      break;

    case 'T':
      /* Struct, union, or enum tag.  For GNU C++, this can be be followed
	 by 't' which means we are typedef'ing it as well.  */
      synonym = *p == 't';

      if (synonym)
	p++;
      /* The semantics of C++ state that "struct foo { ... }" also defines 
	 a typedef for "foo".  Unfortunately, cfront never makes the typedef
	 when translating C++ into C.  We make the typedef here so that
	 "ptype foo" works as expected for cfront translated code.  */
      else if (current_subfile->language == language_cplus)
	synonym = 1;

      SYMBOL_TYPE (sym) = read_type (&p, objfile);

      /* For a nameless type, we don't want a create a symbol, thus we
	 did not use `sym'. Return without further processing. */
      if (nameless) return NULL;

      SYMBOL_CLASS (sym) = LOC_TYPEDEF;
      SYMBOL_VALUE (sym) = valu;
      SYMBOL_NAMESPACE (sym) = STRUCT_NAMESPACE;
      if (TYPE_TAG_NAME (SYMBOL_TYPE (sym)) == 0)
	TYPE_TAG_NAME (SYMBOL_TYPE (sym))
	  = obconcat (&objfile -> type_obstack, "", "", SYMBOL_NAME (sym));
      add_symbol_to_list (sym, &file_symbols);

      if (synonym)
	{
	  /* Clone the sym and then modify it. */
	  register struct symbol *typedef_sym = (struct symbol *)
	    obstack_alloc (&objfile -> symbol_obstack, sizeof (struct symbol));
	  *typedef_sym = *sym;
	  SYMBOL_CLASS (typedef_sym) = LOC_TYPEDEF;
	  SYMBOL_VALUE (typedef_sym) = valu;
	  SYMBOL_NAMESPACE (typedef_sym) = VAR_NAMESPACE;
	  if (TYPE_NAME (SYMBOL_TYPE (sym)) == 0)
	    TYPE_NAME (SYMBOL_TYPE (sym))
	      = obconcat (&objfile -> type_obstack, "", "", SYMBOL_NAME (sym));
	  add_symbol_to_list (typedef_sym, &file_symbols);
	}
      break;

    case 'V':
      /* Static symbol of local scope */
      SYMBOL_TYPE (sym) = read_type (&p, objfile);
      SYMBOL_CLASS (sym) = LOC_STATIC;
      SYMBOL_VALUE_ADDRESS (sym) = valu;
#ifdef STATIC_TRANSFORM_NAME
      if (SYMBOL_NAME (sym)[0] == '$')
      {
	struct minimal_symbol *msym;
	msym = lookup_minimal_symbol (SYMBOL_NAME (sym), NULL, objfile);
	if (msym != NULL)
	  {
	    SYMBOL_NAME (sym) = STATIC_TRANSFORM_NAME (SYMBOL_NAME (sym));
	    SYMBOL_VALUE_ADDRESS (sym) = SYMBOL_VALUE_ADDRESS (msym);
	  }
      }
#endif
      SYMBOL_NAMESPACE (sym) = VAR_NAMESPACE;
      if (os9k_stabs)
	add_symbol_to_list (sym, &global_symbols);
      else
	add_symbol_to_list (sym, &local_symbols);
      break;

    case 'v':
      /* Reference parameter */
      SYMBOL_TYPE (sym) = read_type (&p, objfile);
      SYMBOL_CLASS (sym) = LOC_REF_ARG;
      SYMBOL_VALUE (sym) = valu;
      SYMBOL_NAMESPACE (sym) = VAR_NAMESPACE;
      add_symbol_to_list (sym, &local_symbols);
      break;

    case 'a':
      /* Reference parameter which is in a register.  */
      SYMBOL_TYPE (sym) = read_type (&p, objfile);
      SYMBOL_CLASS (sym) = LOC_REGPARM_ADDR;
      SYMBOL_VALUE (sym) = STAB_REG_TO_REGNUM (valu);
      if (SYMBOL_VALUE (sym) >= NUM_REGS)
	{
	  complain (&reg_value_complaint, SYMBOL_VALUE (sym), NUM_REGS,
		    SYMBOL_SOURCE_NAME (sym));
	  SYMBOL_VALUE (sym) = SP_REGNUM;  /* Known safe, though useless */
	}
      SYMBOL_NAMESPACE (sym) = VAR_NAMESPACE;
      add_symbol_to_list (sym, &local_symbols);
      break;

    case 'X':
      /* This is used by Sun FORTRAN for "function result value".
	 Sun claims ("dbx and dbxtool interfaces", 2nd ed)
	 that Pascal uses it too, but when I tried it Pascal used
	 "x:3" (local symbol) instead.  */
      SYMBOL_TYPE (sym) = read_type (&p, objfile);
      SYMBOL_CLASS (sym) = LOC_LOCAL;
      SYMBOL_VALUE (sym) = valu;
      SYMBOL_NAMESPACE (sym) = VAR_NAMESPACE;
      add_symbol_to_list (sym, &local_symbols);
      break;

    default:
      SYMBOL_TYPE (sym) = error_type (&p, objfile);
      SYMBOL_CLASS (sym) = LOC_CONST;
      SYMBOL_VALUE (sym) = 0;
      SYMBOL_NAMESPACE (sym) = VAR_NAMESPACE;
      add_symbol_to_list (sym, &file_symbols);
      break;
    }

  /* When passing structures to a function, some systems sometimes pass
     the address in a register, not the structure itself. */

  if (REG_STRUCT_HAS_ADDR (processing_gcc_compilation,
			      SYMBOL_TYPE (sym))
      && ((TYPE_CODE (SYMBOL_TYPE (sym)) == TYPE_CODE_STRUCT)
	  || (TYPE_CODE (SYMBOL_TYPE (sym)) == TYPE_CODE_UNION)
	  || (TYPE_CODE (SYMBOL_TYPE (sym)) == TYPE_CODE_BITSTRING)
	  || (TYPE_CODE (SYMBOL_TYPE (sym)) == TYPE_CODE_SET)))
    {
      /* If REG_STRUCT_HAS_ADDR yields non-zero we have to
	convert LOC_REGPARM to LOC_REGPARM_ADDR for structures and unions. */
      if (SYMBOL_CLASS (sym) == LOC_REGPARM)
	SYMBOL_CLASS (sym) = LOC_REGPARM_ADDR;
      /* Likewise for converting LOC_ARG to LOC_REF_ARG (for the 7th and
	 subsequent arguments on the sparc, for example).  */
      else if (SYMBOL_CLASS (sym) == LOC_ARG)
	SYMBOL_CLASS (sym) = LOC_REF_ARG;
    }

  return sym;
}


/* Skip rest of this symbol and return an error type.

   General notes on error recovery:  error_type always skips to the
   end of the symbol (modulo cretinous dbx symbol name continuation).
   Thus code like this:

   if (*(*pp)++ != ';')
     return error_type (pp, objfile);

   is wrong because if *pp starts out pointing at '\0' (typically as the
   result of an earlier error), it will be incremented to point to the
   start of the next symbol, which might produce strange results, at least
   if you run off the end of the string table.  Instead use

   if (**pp != ';')
     return error_type (pp, objfile);
   ++*pp;

   or

   if (**pp != ';')
     foo = error_type (pp, objfile);
   else
     ++*pp;

   And in case it isn't obvious, the point of all this hair is so the compiler
   can define new types and new syntaxes, and old versions of the
   debugger will be able to read the new symbol tables.  */

static struct type *
error_type (pp, objfile)
     char **pp;
     struct objfile *objfile;
{
  complain (&error_type_complaint);
  while (1)
    {
      /* Skip to end of symbol.  */
      while (**pp != '\0')
	{
	  (*pp)++;
	}

      /* Check for and handle cretinous dbx symbol name continuation!  */
      if ((*pp)[-1] == '\\' || (*pp)[-1] == '?')
	{
	  *pp = next_symbol_text (objfile);
	}
      else
	{
	  break;
	}
    }
  return (builtin_type_error);
}


/* Read type information or a type definition; return the type.  Even
   though this routine accepts either type information or a type
   definition, the distinction is relevant--some parts of stabsread.c
   assume that type information starts with a digit, '-', or '(' in
   deciding whether to call read_type.  */

struct type *
read_type (pp, objfile)
     register char **pp;
     struct objfile *objfile;
{
  register struct type *type = 0;
  struct type *type1;
  int typenums[2];
  int xtypenums[2];
  char type_descriptor;

  /* Size in bits of type if specified by a type attribute, or -1 if
     there is no size attribute.  */
  int type_size = -1;

  /* Used to distinguish string and bitstring from char-array and set. */
  int is_string = 0;

  /* Read type number if present.  The type number may be omitted.
     for instance in a two-dimensional array declared with type
     "ar1;1;10;ar1;1;10;4".  */
  if ((**pp >= '0' && **pp <= '9')
      || **pp == '('
      || **pp == '-')
    {
      if (read_type_number (pp, typenums) != 0)
	return error_type (pp, objfile);
      
      /* Type is not being defined here.  Either it already exists,
	 or this is a forward reference to it.  dbx_alloc_type handles
	 both cases.  */
      if (**pp != '=')
	return dbx_alloc_type (typenums, objfile);

      /* Type is being defined here.  */
      /* Skip the '='.  */
      ++(*pp);

      while (**pp == '@')
	{
	  char *p = *pp + 1;
	  /* It might be a type attribute or a member type.  */
	  if (isdigit (*p) || *p ==  '(' || *p == '-')
	    /* Member type.  */
	    break;
	  else
	    {
	      /* Type attributes.  */
	      char *attr = p;

	      /* Skip to the semicolon.  */
	      while (*p != ';' && *p != '\0')
		++p;
	      *pp = p;
	      if (*p == '\0')
		return error_type (pp, objfile);
	      else
		/* Skip the semicolon.  */
		++*pp;

	      switch (*attr)
		{
		case 's':
		  type_size = atoi (attr + 1);
		  if (type_size <= 0)
		    type_size = -1;
		  break;

		case 'S':
		  is_string = 1;
		  break;

		default:
		  /* Ignore unrecognized type attributes, so future compilers
		     can invent new ones.  */
		  break;
		}
	    }
	}
      /* Skip the type descriptor, we get it below with (*pp)[-1].  */
      ++(*pp);
    }
  else
    {
      /* 'typenums=' not present, type is anonymous.  Read and return
	 the definition, but don't put it in the type vector.  */
      typenums[0] = typenums[1] = -1;
      (*pp)++;
    }

  type_descriptor = (*pp)[-1];
  switch (type_descriptor)
    {
    case 'x':
      {
	enum type_code code;

	/* Used to index through file_symbols.  */
	struct pending *ppt;
	int i;
	
	/* Name including "struct", etc.  */
	char *type_name;
	
	{
	  char *from, *to, *p, *q1, *q2;
	  
	  /* Set the type code according to the following letter.  */
	  switch ((*pp)[0])
	    {
	    case 's':
	      code = TYPE_CODE_STRUCT;
	      break;
	    case 'u':
	      code = TYPE_CODE_UNION;
	      break;
	    case 'e':
	      code = TYPE_CODE_ENUM;
	      break;
	    default:
	      {
		/* Complain and keep going, so compilers can invent new
		   cross-reference types.  */
		static struct complaint msg =
		  {"Unrecognized cross-reference type `%c'", 0, 0};
		complain (&msg, (*pp)[0]);
		code = TYPE_CODE_STRUCT;
		break;
	      }
	    }
	   
	  q1 = strchr(*pp, '<');
	  p = strchr(*pp, ':');
	  if (p == NULL)
	    return error_type (pp, objfile);
	  while (q1 && p > q1 && p[1] == ':')
	    {
	       q2 = strchr(q1, '>');
	       if (!q2 || q2 < p)
		 break;
	       p += 2;
	       p = strchr(p, ':');
	       if (p == NULL)
		 return error_type (pp, objfile);
	    }
	  to = type_name = 
		(char *)obstack_alloc (&objfile->type_obstack, p - *pp + 1);
	
	  /* Copy the name.  */
	  from = *pp + 1;
	  while (from < p) 
	    *to++ = *from++;
	  *to = '\0';
	  
	  /* Set the pointer ahead of the name which we just read, and
	     the colon.  */
	  *pp = from + 1;
	}

	/* Now check to see whether the type has already been
	   declared.  This was written for arrays of cross-referenced
	   types before we had TYPE_CODE_TARGET_STUBBED, so I'm pretty
	   sure it is not necessary anymore.  But it might be a good
	   idea, to save a little memory.  */

	for (ppt = file_symbols; ppt; ppt = ppt->next)
	  for (i = 0; i < ppt->nsyms; i++)
	    {
	      struct symbol *sym = ppt->symbol[i];

	      if (SYMBOL_CLASS (sym) == LOC_TYPEDEF
		  && SYMBOL_NAMESPACE (sym) == STRUCT_NAMESPACE
		  && (TYPE_CODE (SYMBOL_TYPE (sym)) == code)
		  && STREQ (SYMBOL_NAME (sym), type_name))
		{
		  obstack_free (&objfile -> type_obstack, type_name);
		  type = SYMBOL_TYPE (sym);
		  return type;
		}
	    }

	/* Didn't find the type to which this refers, so we must
	   be dealing with a forward reference.  Allocate a type
	   structure for it, and keep track of it so we can
	   fill in the rest of the fields when we get the full
	   type.  */
	type = dbx_alloc_type (typenums, objfile);
	TYPE_CODE (type) = code;
	TYPE_TAG_NAME (type) = type_name;
	INIT_CPLUS_SPECIFIC(type);
	TYPE_FLAGS (type) |= TYPE_FLAG_STUB;

	add_undefined_type (type);
	return type;
      }

    case '-':				/* RS/6000 built-in type */
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
    case '(':

      {
	char *pp_saved;

	(*pp)--;
	pp_saved = *pp;

	/* Peek ahead at the number to detect void.  */
	if (read_type_number (pp, xtypenums) != 0)
	  return error_type (pp, objfile);

	if (typenums[0] == xtypenums[0] && typenums[1] == xtypenums[1])
	  /* It's being defined as itself.  That means it is "void".  */
	  type = init_type (TYPE_CODE_VOID, 1, 0, NULL, objfile);
	else
	  {
	    struct type *xtype;

	    /* Go back to the number and have read_type get it.  This means
	       that we can deal with something like t(1,2)=(3,4)=... which
	       the Lucid compiler uses.  */
	    *pp = pp_saved;
	    xtype = read_type (pp, objfile);

	    /* The type is being defined to another type.  So we copy the type.
	       This loses if we copy a C++ class and so we lose track of how
	       the names are mangled (but g++ doesn't output stabs like this
	       now anyway).  */

	    type = alloc_type (objfile);
	    if (SYMBOL_LINE (current_symbol) == 0)
	      {
		*type = *xtype;
		/* The idea behind clearing the names is that the only purpose
		   for defining a type to another type is so that the name of
		   one can be different.  So we probably don't need to worry
		   much about the case where the compiler doesn't give a name
		   to the new type.  */
		TYPE_NAME (type) = NULL;
		TYPE_TAG_NAME (type) = NULL;
	      }
	    else
	      {
		TYPE_CODE (type) = TYPE_CODE_TYPEDEF;
		TYPE_FLAGS (type) |= TYPE_FLAG_TARGET_STUB;
		TYPE_TARGET_TYPE (type) = xtype;
	      }
	  }
	if (typenums[0] != -1)
	  *dbx_lookup_type (typenums) = type;
	break;
      }

    /* In the following types, we must be sure to overwrite any existing
       type that the typenums refer to, rather than allocating a new one
       and making the typenums point to the new one.  This is because there
       may already be pointers to the existing type (if it had been
       forward-referenced), and we must change it to a pointer, function,
       reference, or whatever, *in-place*.  */

    case '*':
      type1 = read_type (pp, objfile);
      type = make_pointer_type (type1, dbx_lookup_type (typenums));
      break;

    case '&':				/* Reference to another type */
      type1 = read_type (pp, objfile);
      type = make_reference_type (type1, dbx_lookup_type (typenums));
      break;

    case 'f':				/* Function returning another type */
      if (os9k_stabs && **pp == '(')
	{
	  /* Function prototype; parse it.
	     We must conditionalize this on os9k_stabs because otherwise
	     it could be confused with a Sun-style (1,3) typenumber
	     (I think).  */
	  struct type *t;
	  ++*pp;
	  while (**pp != ')')
            {
              t = read_type(pp, objfile);
              if (**pp == ',') ++*pp;
            }
	}
      type1 = read_type (pp, objfile);
      type = make_function_type (type1, dbx_lookup_type (typenums));
      break;

    case 'k':			   /* Const qualifier on some type (Sun) */
    case 'c':			   /* Const qualifier on some type (OS9000) */
      /* Because 'c' means other things to AIX and 'k' is perfectly good,
	 only accept 'c' in the os9k_stabs case.  */
      if (type_descriptor == 'c' && !os9k_stabs)
	return error_type (pp, objfile);
      type = read_type (pp, objfile);
      /* FIXME! For now, we ignore const and volatile qualifiers.  */
      break;

    case 'B':			     /* Volatile qual on some type (Sun) */
    case 'i':			     /* Volatile qual on some type (OS9000) */
      /* Because 'i' means other things to AIX and 'B' is perfectly good,
	 only accept 'i' in the os9k_stabs case.  */
      if (type_descriptor == 'i' && !os9k_stabs)
	return error_type (pp, objfile);
      type = read_type (pp, objfile);
      /* FIXME! For now, we ignore const and volatile qualifiers.  */
      break;

/* FIXME -- we should be doing smash_to_XXX types here.  */
    case '@':				/* Member (class & variable) type */
      {
	struct type *domain = read_type (pp, objfile);
	struct type *memtype;

	if (**pp != ',')
	  /* Invalid member type data format.  */
	  return error_type (pp, objfile);
	++*pp;

	memtype = read_type (pp, objfile);
	type = dbx_alloc_type (typenums, objfile);
	smash_to_member_type (type, domain, memtype);
      }
      break;

    case '#':				/* Method (class & fn) type */
      if ((*pp)[0] == '#')
	{
	  /* We'll get the parameter types from the name.  */
	  struct type *return_type;

	  (*pp)++;
	  return_type = read_type (pp, objfile);
	  if (*(*pp)++ != ';')
	    complain (&invalid_member_complaint, symnum);
	  type = allocate_stub_method (return_type);
	  if (typenums[0] != -1)
	    *dbx_lookup_type (typenums) = type;
	}
      else
	{
	  struct type *domain = read_type (pp, objfile);
	  struct type *return_type;
	  struct type **args;

	  if (**pp != ',')
	    /* Invalid member type data format.  */
	    return error_type (pp, objfile);
	  else
	    ++(*pp);

	  return_type = read_type (pp, objfile);
	  args = read_args (pp, ';', objfile);
	  type = dbx_alloc_type (typenums, objfile);
	  smash_to_method_type (type, domain, return_type, args);
	}
      break;

    case 'r':				/* Range type */
      type = read_range_type (pp, typenums, objfile);
      if (typenums[0] != -1)
	*dbx_lookup_type (typenums) = type;
      break;

    case 'b':
      if (os9k_stabs)
	/* Const and volatile qualified type.  */
	type = read_type (pp, objfile);
      else
	{
	  /* Sun ACC builtin int type */
	  type = read_sun_builtin_type (pp, typenums, objfile);
	  if (typenums[0] != -1)
	    *dbx_lookup_type (typenums) = type;
	}
      break;

    case 'R':				/* Sun ACC builtin float type */
      type = read_sun_floating_type (pp, typenums, objfile);
      if (typenums[0] != -1)
	*dbx_lookup_type (typenums) = type;
      break;
    
    case 'e':				/* Enumeration type */
      type = dbx_alloc_type (typenums, objfile);
      type = read_enum_type (pp, type, objfile);
      if (typenums[0] != -1)
	*dbx_lookup_type (typenums) = type;
      break;

    case 's':				/* Struct type */
    case 'u':				/* Union type */
      type = dbx_alloc_type (typenums, objfile);
      switch (type_descriptor)
	{
	  case 's':
	    TYPE_CODE (type) = TYPE_CODE_STRUCT;
	    break;
	  case 'u':
	    TYPE_CODE (type) = TYPE_CODE_UNION;
	    break;
	}
      type = read_struct_type (pp, type, objfile);
      break;

    case 'a':				/* Array type */
      if (**pp != 'r')
	return error_type (pp, objfile);
      ++*pp;
      
      type = dbx_alloc_type (typenums, objfile);
      type = read_array_type (pp, type, objfile);
      if (is_string)
	TYPE_CODE (type) = TYPE_CODE_STRING;
      break;

    case 'S':
      type1 = read_type (pp, objfile);
      type = create_set_type ((struct type*) NULL, type1);
      if (is_string)
	TYPE_CODE (type) = TYPE_CODE_BITSTRING;
      if (typenums[0] != -1)
	*dbx_lookup_type (typenums) = type;
      break;

    default:
      --*pp;			/* Go back to the symbol in error */
				/* Particularly important if it was \0! */
      return error_type (pp, objfile);
    }

  if (type == 0)
    {
      warning ("GDB internal error, type is NULL in stabsread.c\n");
      return error_type (pp, objfile);
    }

  /* Size specified in a type attribute overrides any other size.  */
  if (type_size != -1)
    TYPE_LENGTH (type) = (type_size + TARGET_CHAR_BIT - 1) / TARGET_CHAR_BIT;

  return type;
}

/* RS/6000 xlc/dbx combination uses a set of builtin types, starting from -1.
   Return the proper type node for a given builtin type number. */

static struct type *
rs6000_builtin_type (typenum)
     int typenum;
{
  /* We recognize types numbered from -NUMBER_RECOGNIZED to -1.  */
#define NUMBER_RECOGNIZED 34
  /* This includes an empty slot for type number -0.  */
  static struct type *negative_types[NUMBER_RECOGNIZED + 1];
  struct type *rettype = NULL;

  if (typenum >= 0 || typenum < -NUMBER_RECOGNIZED)
    {
      complain (&rs6000_builtin_complaint, typenum);
      return builtin_type_error;
    }
  if (negative_types[-typenum] != NULL)
    return negative_types[-typenum];

#if TARGET_CHAR_BIT != 8
  #error This code wrong for TARGET_CHAR_BIT not 8
  /* These definitions all assume that TARGET_CHAR_BIT is 8.  I think
     that if that ever becomes not true, the correct fix will be to
     make the size in the struct type to be in bits, not in units of
     TARGET_CHAR_BIT.  */
#endif

  switch (-typenum)
    {
    case 1:
      /* The size of this and all the other types are fixed, defined
	 by the debugging format.  If there is a type called "int" which
	 is other than 32 bits, then it should use a new negative type
	 number (or avoid negative type numbers for that case).
	 See stabs.texinfo.  */
      rettype = init_type (TYPE_CODE_INT, 4, 0, "int", NULL);
      break;
    case 2:
      rettype = init_type (TYPE_CODE_INT, 1, 0, "char", NULL);
      break;
    case 3:
      rettype = init_type (TYPE_CODE_INT, 2, 0, "short", NULL);
      break;
    case 4:
      rettype = init_type (TYPE_CODE_INT, 4, 0, "long", NULL);
      break;
    case 5:
      rettype = init_type (TYPE_CODE_INT, 1, TYPE_FLAG_UNSIGNED,
			   "unsigned char", NULL);
      break;
    case 6:
      rettype = init_type (TYPE_CODE_INT, 1, 0, "signed char", NULL);
      break;
    case 7:
      rettype = init_type (TYPE_CODE_INT, 2, TYPE_FLAG_UNSIGNED,
			   "unsigned short", NULL);
      break;
    case 8:
      rettype = init_type (TYPE_CODE_INT, 4, TYPE_FLAG_UNSIGNED,
			   "unsigned int", NULL);
      break;
    case 9:
      rettype = init_type (TYPE_CODE_INT, 4, TYPE_FLAG_UNSIGNED,
			   "unsigned", NULL);
    case 10:
      rettype = init_type (TYPE_CODE_INT, 4, TYPE_FLAG_UNSIGNED,
			   "unsigned long", NULL);
      break;
    case 11:
      rettype = init_type (TYPE_CODE_VOID, 1, 0, "void", NULL);
      break;
    case 12:
      /* IEEE single precision (32 bit).  */
      rettype = init_type (TYPE_CODE_FLT, 4, 0, "float", NULL);
      break;
    case 13:
      /* IEEE double precision (64 bit).  */
      rettype = init_type (TYPE_CODE_FLT, 8, 0, "double", NULL);
      break;
    case 14:
      /* This is an IEEE double on the RS/6000, and different machines with
	 different sizes for "long double" should use different negative
	 type numbers.  See stabs.texinfo.  */
      rettype = init_type (TYPE_CODE_FLT, 8, 0, "long double", NULL);
      break;
    case 15:
      rettype = init_type (TYPE_CODE_INT, 4, 0, "integer", NULL);
      break;
    case 16:
      rettype = init_type (TYPE_CODE_BOOL, 4, TYPE_FLAG_UNSIGNED,
			   "boolean", NULL);
      break;
    case 17:
      rettype = init_type (TYPE_CODE_FLT, 4, 0, "short real", NULL);
      break;
    case 18:
      rettype = init_type (TYPE_CODE_FLT, 8, 0, "real", NULL);
      break;
    case 19:
      rettype = init_type (TYPE_CODE_ERROR, 0, 0, "stringptr", NULL);
      break;
    case 20:
      rettype = init_type (TYPE_CODE_CHAR, 1, TYPE_FLAG_UNSIGNED,
			   "character", NULL);
      break;
    case 21:
      rettype = init_type (TYPE_CODE_BOOL, 1, TYPE_FLAG_UNSIGNED,
			   "logical*1", NULL);
      break;
    case 22:
      rettype = init_type (TYPE_CODE_BOOL, 2, TYPE_FLAG_UNSIGNED,
			   "logical*2", NULL);
      break;
    case 23:
      rettype = init_type (TYPE_CODE_BOOL, 4, TYPE_FLAG_UNSIGNED,
			   "logical*4", NULL);
      break;
    case 24:
      rettype = init_type (TYPE_CODE_BOOL, 4, TYPE_FLAG_UNSIGNED,
			   "logical", NULL);
      break;
    case 25:
      /* Complex type consisting of two IEEE single precision values.  */
      rettype = init_type (TYPE_CODE_ERROR, 8, 0, "complex", NULL);
      break;
    case 26:
      /* Complex type consisting of two IEEE double precision values.  */
      rettype = init_type (TYPE_CODE_ERROR, 16, 0, "double complex", NULL);
      break;
    case 27:
      rettype = init_type (TYPE_CODE_INT, 1, 0, "integer*1", NULL);
      break;
    case 28:
      rettype = init_type (TYPE_CODE_INT, 2, 0, "integer*2", NULL);
      break;
    case 29:
      rettype = init_type (TYPE_CODE_INT, 4, 0, "integer*4", NULL);
      break;
    case 30:
      rettype = init_type (TYPE_CODE_CHAR, 2, 0, "wchar", NULL);
      break;
    case 31:
      rettype = init_type (TYPE_CODE_INT, 8, 0, "long long", NULL);
      break;
    case 32:
      rettype = init_type (TYPE_CODE_INT, 8, TYPE_FLAG_UNSIGNED,
			   "unsigned long long", NULL);
      break;
    case 33:
      rettype = init_type (TYPE_CODE_INT, 8, TYPE_FLAG_UNSIGNED,
			   "logical*8", NULL);
      break;
    case 34:
      rettype = init_type (TYPE_CODE_INT, 8, 0, "integer*8", NULL);
      break;
    }
  negative_types[-typenum] = rettype;
  return rettype;
}

/* This page contains subroutines of read_type.  */

#define VISIBILITY_PRIVATE	'0'	/* Stabs character for private field */
#define VISIBILITY_PROTECTED	'1'	/* Stabs character for protected fld */
#define VISIBILITY_PUBLIC	'2'	/* Stabs character for public field */
#define VISIBILITY_IGNORE	'9'	/* Optimized out or zero length */

/* Read member function stabs info for C++ classes.  The form of each member
   function data is:

	NAME :: TYPENUM[=type definition] ARGS : PHYSNAME ;

   An example with two member functions is:

	afunc1::20=##15;:i;2A.;afunc2::20:i;2A.;

   For the case of overloaded operators, the format is op$::*.funcs, where
   $ is the CPLUS_MARKER (usually '$'), `*' holds the place for an operator
   name (such as `+=') and `.' marks the end of the operator name.

   Returns 1 for success, 0 for failure.  */

static int
read_member_functions (fip, pp, type, objfile)
     struct field_info *fip;
     char **pp;
     struct type *type;
     struct objfile *objfile;
{
  int nfn_fields = 0;
  int length = 0;
  /* Total number of member functions defined in this class.  If the class
     defines two `f' functions, and one `g' function, then this will have
     the value 3.  */
  int total_length = 0;
  int i;
  struct next_fnfield
    {
      struct next_fnfield *next;
      struct fn_field fn_field;
    } *sublist;
  struct type *look_ahead_type;
  struct next_fnfieldlist *new_fnlist;
  struct next_fnfield *new_sublist;
  char *main_fn_name;
  register char *p;
      
  /* Process each list until we find something that is not a member function
     or find the end of the functions. */

  while (**pp != ';')
    {
      /* We should be positioned at the start of the function name.
	 Scan forward to find the first ':' and if it is not the
	 first of a "::" delimiter, then this is not a member function. */
      p = *pp;
      while (*p != ':')
	{
	  p++;
	}
      if (p[1] != ':')
	{
	  break;
	}

      sublist = NULL;
      look_ahead_type = NULL;
      length = 0;
      
      new_fnlist = (struct next_fnfieldlist *)
	xmalloc (sizeof (struct next_fnfieldlist));
      make_cleanup (free, new_fnlist);
      memset (new_fnlist, 0, sizeof (struct next_fnfieldlist));
      
      if ((*pp)[0] == 'o' && (*pp)[1] == 'p' && is_cplus_marker ((*pp)[2]))
	{
	  /* This is a completely wierd case.  In order to stuff in the
	     names that might contain colons (the usual name delimiter),
	     Mike Tiemann defined a different name format which is
	     signalled if the identifier is "op$".  In that case, the
	     format is "op$::XXXX." where XXXX is the name.  This is
	     used for names like "+" or "=".  YUUUUUUUK!  FIXME!  */
	  /* This lets the user type "break operator+".
	     We could just put in "+" as the name, but that wouldn't
	     work for "*".  */
	  static char opname[32] = {'o', 'p', CPLUS_MARKER};
	  char *o = opname + 3;
	  
	  /* Skip past '::'.  */
	  *pp = p + 2;

	  STABS_CONTINUE (pp, objfile);
	  p = *pp;
	  while (*p != '.')
	    {
	      *o++ = *p++;
	    }
	  main_fn_name = savestring (opname, o - opname);
	  /* Skip past '.'  */
	  *pp = p + 1;
	}
      else
	{
	  main_fn_name = savestring (*pp, p - *pp);
	  /* Skip past '::'.  */
	  *pp = p + 2;
	}
      new_fnlist -> fn_fieldlist.name = main_fn_name;
      
      do
	{
	  new_sublist =
	    (struct next_fnfield *) xmalloc (sizeof (struct next_fnfield));
	  make_cleanup (free, new_sublist);
	  memset (new_sublist, 0, sizeof (struct next_fnfield));
	  
	  /* Check for and handle cretinous dbx symbol name continuation!  */
	  if (look_ahead_type == NULL)
	    {
	      /* Normal case. */
	      STABS_CONTINUE (pp, objfile);
	      
	      new_sublist -> fn_field.type = read_type (pp, objfile);
	      if (**pp != ':')
		{
		  /* Invalid symtab info for member function.  */
		  return 0;
		}
	    }
	  else
	    {
	      /* g++ version 1 kludge */
	      new_sublist -> fn_field.type = look_ahead_type;
	      look_ahead_type = NULL;
	    }
	  
	  (*pp)++;
	  p = *pp;
	  while (*p != ';')
	    {
	      p++;
	    }
	  
	  /* If this is just a stub, then we don't have the real name here. */

	  if (TYPE_FLAGS (new_sublist -> fn_field.type) & TYPE_FLAG_STUB)
	    {
	      if (!TYPE_DOMAIN_TYPE (new_sublist -> fn_field.type))
		TYPE_DOMAIN_TYPE (new_sublist -> fn_field.type) = type;
	      new_sublist -> fn_field.is_stub = 1;
	    }
	  new_sublist -> fn_field.physname = savestring (*pp, p - *pp);
	  *pp = p + 1;
	  
	  /* Set this member function's visibility fields.  */
	  switch (*(*pp)++)
	    {
	      case VISIBILITY_PRIVATE:
	        new_sublist -> fn_field.is_private = 1;
		break;
	      case VISIBILITY_PROTECTED:
		new_sublist -> fn_field.is_protected = 1;
		break;
	    }
	  
	  STABS_CONTINUE (pp, objfile);
	  switch (**pp)
	    {
	      case 'A': /* Normal functions. */
	        new_sublist -> fn_field.is_const = 0;
		new_sublist -> fn_field.is_volatile = 0;
		(*pp)++;
		break;
	      case 'B': /* `const' member functions. */
		new_sublist -> fn_field.is_const = 1;
		new_sublist -> fn_field.is_volatile = 0;
		(*pp)++;
		break;
	      case 'C': /* `volatile' member function. */
		new_sublist -> fn_field.is_const = 0;
		new_sublist -> fn_field.is_volatile = 1;
		(*pp)++;
		break;
	      case 'D': /* `const volatile' member function. */
		new_sublist -> fn_field.is_const = 1;
		new_sublist -> fn_field.is_volatile = 1;
		(*pp)++;
		break;
	      case '*': /* File compiled with g++ version 1 -- no info */
	      case '?':
	      case '.':
		break;
	      default:
		complain (&const_vol_complaint, **pp);
		break;
	    }
	  
	  switch (*(*pp)++)
	    {
	      case '*':
	      {
		int nbits;
	        /* virtual member function, followed by index.
		   The sign bit is set to distinguish pointers-to-methods
		   from virtual function indicies.  Since the array is
		   in words, the quantity must be shifted left by 1
		   on 16 bit machine, and by 2 on 32 bit machine, forcing
		   the sign bit out, and usable as a valid index into
		   the array.  Remove the sign bit here.  */
	        new_sublist -> fn_field.voffset =
		  (0x7fffffff & read_huge_number (pp, ';', &nbits)) + 2;
		if (nbits != 0)
		  return 0;
	      
		STABS_CONTINUE (pp, objfile);
		if (**pp == ';' || **pp == '\0')
		  {
		    /* Must be g++ version 1.  */
		    new_sublist -> fn_field.fcontext = 0;
		  }
		else
		  {
		    /* Figure out from whence this virtual function came.
		       It may belong to virtual function table of
		       one of its baseclasses.  */
		    look_ahead_type = read_type (pp, objfile);
		    if (**pp == ':')
		      {
			/* g++ version 1 overloaded methods. */
		      }
		    else
		      {
			new_sublist -> fn_field.fcontext = look_ahead_type;
			if (**pp != ';')
			  {
			    return 0;
			  }
			else
			  {
			    ++*pp;
			  }
			look_ahead_type = NULL;
		      }
		  }
		break;
	      }
	      case '?':
		/* static member function.  */
		new_sublist -> fn_field.voffset = VOFFSET_STATIC;
		if (strncmp (new_sublist -> fn_field.physname,
			     main_fn_name, strlen (main_fn_name)))
		  {
		    new_sublist -> fn_field.is_stub = 1;
		  }
		break;
	      
	      default:
		/* error */
		complain (&member_fn_complaint, (*pp)[-1]);
		/* Fall through into normal member function.  */
	      
	      case '.':
		/* normal member function.  */
		new_sublist -> fn_field.voffset = 0;
		new_sublist -> fn_field.fcontext = 0;
		break;
	    }
	  
	  new_sublist -> next = sublist;
	  sublist = new_sublist;
	  length++;
	  STABS_CONTINUE (pp, objfile);
	}
      while (**pp != ';' && **pp != '\0');
      
      (*pp)++;
      
      new_fnlist -> fn_fieldlist.fn_fields = (struct fn_field *)
	obstack_alloc (&objfile -> type_obstack, 
		       sizeof (struct fn_field) * length);
      memset (new_fnlist -> fn_fieldlist.fn_fields, 0,
	      sizeof (struct fn_field) * length);
      for (i = length; (i--, sublist); sublist = sublist -> next)
	{
	  new_fnlist -> fn_fieldlist.fn_fields[i] = sublist -> fn_field;
	}
      
      new_fnlist -> fn_fieldlist.length = length;
      new_fnlist -> next = fip -> fnlist;
      fip -> fnlist = new_fnlist;
      nfn_fields++;
      total_length += length;
      STABS_CONTINUE (pp, objfile);
    }

  if (nfn_fields)
    {
      ALLOCATE_CPLUS_STRUCT_TYPE (type);
      TYPE_FN_FIELDLISTS (type) = (struct fn_fieldlist *)
	TYPE_ALLOC (type, sizeof (struct fn_fieldlist) * nfn_fields);
      memset (TYPE_FN_FIELDLISTS (type), 0,
	      sizeof (struct fn_fieldlist) * nfn_fields);
      TYPE_NFN_FIELDS (type) = nfn_fields;
      TYPE_NFN_FIELDS_TOTAL (type) = total_length;
    }

  return 1;
}

/* Special GNU C++ name.

   Returns 1 for success, 0 for failure.  "failure" means that we can't
   keep parsing and it's time for error_type().  */

static int
read_cpp_abbrev (fip, pp, type, objfile)
     struct field_info *fip;
     char **pp;
     struct type *type;
     struct objfile *objfile;
{
  register char *p;
  char *name;
  char cpp_abbrev;
  struct type *context;

  p = *pp;
  if (*++p == 'v')
    {
      name = NULL;
      cpp_abbrev = *++p;

      *pp = p + 1;

      /* At this point, *pp points to something like "22:23=*22...",
	 where the type number before the ':' is the "context" and
	 everything after is a regular type definition.  Lookup the
	 type, find it's name, and construct the field name. */

      context = read_type (pp, objfile);

      switch (cpp_abbrev)
	{
	  case 'f':		/* $vf -- a virtual function table pointer */
	    fip->list->field.name =
	      obconcat (&objfile->type_obstack, vptr_name, "", "");
	    break;

	  case 'b':		/* $vb -- a virtual bsomethingorother */
	    name = type_name_no_tag (context);
	    if (name == NULL)
	      {
		complain (&invalid_cpp_type_complaint, symnum);
		name = "FOO";
	      }
	    fip->list->field.name =
	      obconcat (&objfile->type_obstack, vb_name, name, "");
	    break;

	  default:
	    complain (&invalid_cpp_abbrev_complaint, *pp);
	    fip->list->field.name =
	      obconcat (&objfile->type_obstack,
			"INVALID_CPLUSPLUS_ABBREV", "", "");
	    break;
	}

      /* At this point, *pp points to the ':'.  Skip it and read the
	 field type. */

      p = ++(*pp);
      if (p[-1] != ':')
	{
	  complain (&invalid_cpp_abbrev_complaint, *pp);
	  return 0;
	}
      fip->list->field.type = read_type (pp, objfile);
      if (**pp == ',')
	(*pp)++;			/* Skip the comma.  */
      else
	return 0;

      {
	int nbits;
	fip->list->field.bitpos = read_huge_number (pp, ';', &nbits);
	if (nbits != 0)
	  return 0;
      }
      /* This field is unpacked.  */
      fip->list->field.bitsize = 0;
      fip->list->visibility = VISIBILITY_PRIVATE;
    }
  else
    {
      complain (&invalid_cpp_abbrev_complaint, *pp);
      /* We have no idea what syntax an unrecognized abbrev would have, so
	 better return 0.  If we returned 1, we would need to at least advance
	 *pp to avoid an infinite loop.  */
      return 0;
    }
  return 1;
}

static void
read_one_struct_field (fip, pp, p, type, objfile)
     struct field_info *fip;
     char **pp;
     char *p;
     struct type *type;
     struct objfile *objfile;
{
  /* The following is code to work around cfront generated stabs.
     The stabs contains full mangled name for each field.
     We try to demangle the name and extract the field name out of it.
  */
  if (ARM_DEMANGLING && current_subfile->language == language_cplus)
    {
      char save_p;
      char *dem, *dem_p;
      save_p = *p;
      *p = '\0';
      dem = cplus_demangle (*pp, DMGL_ANSI | DMGL_PARAMS);
      if (dem != NULL)
        {
          dem_p = strrchr (dem, ':');
          if (dem_p != 0 && *(dem_p-1)==':')
            dem_p++;
          fip->list->field.name =
            obsavestring (dem_p, strlen(dem_p), &objfile -> type_obstack);
        }
      else
        {
          fip->list->field.name =
            obsavestring (*pp, p - *pp, &objfile -> type_obstack);
        }
      *p = save_p;
    }
  /* end of code for cfront work around */

  else
    fip -> list -> field.name =
    obsavestring (*pp, p - *pp, &objfile -> type_obstack);
  *pp = p + 1;

  /* This means we have a visibility for a field coming. */
  if (**pp == '/')
    {
      (*pp)++;
      fip -> list -> visibility = *(*pp)++;
    }
  else
    {
      /* normal dbx-style format, no explicit visibility */
      fip -> list -> visibility = VISIBILITY_PUBLIC;
    }

  fip -> list -> field.type = read_type (pp, objfile);
  if (**pp == ':')
    {
      p = ++(*pp);
#if 0
      /* Possible future hook for nested types. */
      if (**pp == '!')
	{
	  fip -> list -> field.bitpos = (long)-2; /* nested type */
	  p = ++(*pp);
	}
      else
#endif
	{
	  /* Static class member.  */
	  fip -> list -> field.bitpos = (long) -1;
	}
      while (*p != ';') 
	{
	  p++;
	}
      fip -> list -> field.bitsize = (long) savestring (*pp, p - *pp);
      *pp = p + 1;
      return;
    }
  else if (**pp != ',')
    {
      /* Bad structure-type format.  */
      complain (&stabs_general_complaint, "bad structure-type format");
      return;
    }

  (*pp)++;			/* Skip the comma.  */

  {
    int nbits;
    fip -> list -> field.bitpos = read_huge_number (pp, ',', &nbits);
    if (nbits != 0)
      {
	complain (&stabs_general_complaint, "bad structure-type format");
	return;
      }
    fip -> list -> field.bitsize = read_huge_number (pp, ';', &nbits);
    if (nbits != 0)
      {
	complain (&stabs_general_complaint, "bad structure-type format");
	return;
      }
  }

  if (fip -> list -> field.bitpos == 0 && fip -> list -> field.bitsize == 0)
    {
      /* This can happen in two cases: (1) at least for gcc 2.4.5 or so,
	 it is a field which has been optimized out.  The correct stab for
	 this case is to use VISIBILITY_IGNORE, but that is a recent
	 invention.  (2) It is a 0-size array.  For example
	 union { int num; char str[0]; } foo.  Printing "<no value>" for
	 str in "p foo" is OK, since foo.str (and thus foo.str[3])
	 will continue to work, and a 0-size array as a whole doesn't
	 have any contents to print.

	 I suspect this probably could also happen with gcc -gstabs (not
	 -gstabs+) for static fields, and perhaps other C++ extensions.
	 Hopefully few people use -gstabs with gdb, since it is intended
	 for dbx compatibility.  */

      /* Ignore this field.  */
      fip -> list-> visibility = VISIBILITY_IGNORE;
    }
  else
    {
      /* Detect an unpacked field and mark it as such.
	 dbx gives a bit size for all fields.
	 Note that forward refs cannot be packed,
	 and treat enums as if they had the width of ints.  */

      if (TYPE_CODE (fip -> list -> field.type) != TYPE_CODE_INT
	  && TYPE_CODE (fip -> list -> field.type) != TYPE_CODE_BOOL
	  && TYPE_CODE (fip -> list -> field.type) != TYPE_CODE_ENUM)
	{
	  fip -> list -> field.bitsize = 0;
	}
      if ((fip -> list -> field.bitsize 
	   == TARGET_CHAR_BIT * TYPE_LENGTH (fip -> list -> field.type)
	   || (TYPE_CODE (fip -> list -> field.type) == TYPE_CODE_ENUM
	       && (fip -> list -> field.bitsize
		   == TARGET_INT_BIT)
	       )
	   )
	  &&
	  fip -> list -> field.bitpos % 8 == 0)
	{
	  fip -> list -> field.bitsize = 0;
	}
    }
}


/* Read struct or class data fields.  They have the form:

   	NAME : [VISIBILITY] TYPENUM , BITPOS , BITSIZE ;

   At the end, we see a semicolon instead of a field.

   In C++, this may wind up being NAME:?TYPENUM:PHYSNAME; for
   a static field.

   The optional VISIBILITY is one of:

   	'/0'	(VISIBILITY_PRIVATE)
	'/1'	(VISIBILITY_PROTECTED)
	'/2'	(VISIBILITY_PUBLIC)
	'/9'	(VISIBILITY_IGNORE)

   or nothing, for C style fields with public visibility.

   Returns 1 for success, 0 for failure.  */

static int
read_struct_fields (fip, pp, type, objfile)
     struct field_info *fip;
     char **pp;
     struct type *type;
     struct objfile *objfile;
{
  register char *p;
  struct nextfield *new;

  /* We better set p right now, in case there are no fields at all...    */

  p = *pp;

  /* Read each data member type until we find the terminating ';' at the end of
     the data member list, or break for some other reason such as finding the
     start of the member function list. */

  while (**pp != ';')
    {
      if (os9k_stabs && **pp == ',') break;
      STABS_CONTINUE (pp, objfile);
      /* Get space to record the next field's data.  */
      new = (struct nextfield *) xmalloc (sizeof (struct nextfield));
      make_cleanup (free, new);
      memset (new, 0, sizeof (struct nextfield));
      new -> next = fip -> list;
      fip -> list = new;

      /* Get the field name.  */
      p = *pp;

      /* If is starts with CPLUS_MARKER it is a special abbreviation,
	 unless the CPLUS_MARKER is followed by an underscore, in
	 which case it is just the name of an anonymous type, which we
	 should handle like any other type name.  */

      if (is_cplus_marker (p[0]) && p[1] != '_')
	{
	  if (!read_cpp_abbrev (fip, pp, type, objfile))
	    return 0;
	  continue;
	}

      /* Look for the ':' that separates the field name from the field
	 values.  Data members are delimited by a single ':', while member
	 functions are delimited by a pair of ':'s.  When we hit the member
	 functions (if any), terminate scan loop and return. */

      while (*p != ':' && *p != '\0') 
	{
	  p++;
	}
      if (*p == '\0')
	return 0;

      /* Check to see if we have hit the member functions yet.  */
      if (p[1] == ':')
	{
	  break;
	}
      read_one_struct_field (fip, pp, p, type, objfile);
    }
  if (p[0] == ':' && p[1] == ':')
    {
      /* chill the list of fields: the last entry (at the head) is a
	 partially constructed entry which we now scrub. */
      fip -> list = fip -> list -> next;
    }
  return 1;
}

/* The stabs for C++ derived classes contain baseclass information which
   is marked by a '!' character after the total size.  This function is
   called when we encounter the baseclass marker, and slurps up all the
   baseclass information.

   Immediately following the '!' marker is the number of base classes that
   the class is derived from, followed by information for each base class.
   For each base class, there are two visibility specifiers, a bit offset
   to the base class information within the derived class, a reference to
   the type for the base class, and a terminating semicolon.

   A typical example, with two base classes, would be "!2,020,19;0264,21;".
   						       ^^ ^ ^ ^  ^ ^  ^
	Baseclass information marker __________________|| | | |  | |  |
	Number of baseclasses __________________________| | | |  | |  |
	Visibility specifiers (2) ________________________| | |  | |  |
	Offset in bits from start of class _________________| |  | |  |
	Type number for base class ___________________________|  | |  |
	Visibility specifiers (2) _______________________________| |  |
	Offset in bits from start of class ________________________|  |
	Type number of base class ____________________________________|

  Return 1 for success, 0 for (error-type-inducing) failure.  */

static int
read_baseclasses (fip, pp, type, objfile)
     struct field_info *fip;
     char **pp;
     struct type *type;
     struct objfile *objfile;
{
  int i;
  struct nextfield *new;

  if (**pp != '!')
    {
      return 1;
    }
  else
    {
      /* Skip the '!' baseclass information marker. */
      (*pp)++;
    }

  ALLOCATE_CPLUS_STRUCT_TYPE (type);
  {
    int nbits;
    TYPE_N_BASECLASSES (type) = read_huge_number (pp, ',', &nbits);
    if (nbits != 0)
      return 0;
  }

#if 0
  /* Some stupid compilers have trouble with the following, so break
     it up into simpler expressions.  */
  TYPE_FIELD_VIRTUAL_BITS (type) = (B_TYPE *)
    TYPE_ALLOC (type, B_BYTES (TYPE_N_BASECLASSES (type)));
#else
  {
    int num_bytes = B_BYTES (TYPE_N_BASECLASSES (type));
    char *pointer;

    pointer = (char *) TYPE_ALLOC (type, num_bytes);
    TYPE_FIELD_VIRTUAL_BITS (type) = (B_TYPE *) pointer;
  }
#endif /* 0 */

  B_CLRALL (TYPE_FIELD_VIRTUAL_BITS (type), TYPE_N_BASECLASSES (type));

  for (i = 0; i < TYPE_N_BASECLASSES (type); i++)
    {
      new = (struct nextfield *) xmalloc (sizeof (struct nextfield));
      make_cleanup (free, new);
      memset (new, 0, sizeof (struct nextfield));
      new -> next = fip -> list;
      fip -> list = new;
      new -> field.bitsize = 0;	/* this should be an unpacked field! */

      STABS_CONTINUE (pp, objfile);
      switch (**pp)
	{
	  case '0':
	    /* Nothing to do. */
	    break;
	  case '1':
	    SET_TYPE_FIELD_VIRTUAL (type, i);
	    break;
	  default:
	    /* Unknown character.  Complain and treat it as non-virtual.  */
	    {
	      static struct complaint msg = {
		"Unknown virtual character `%c' for baseclass", 0, 0};
	      complain (&msg, **pp);
	    }
	}
      ++(*pp);

      new -> visibility = *(*pp)++;
      switch (new -> visibility)
	{
	  case VISIBILITY_PRIVATE:
	  case VISIBILITY_PROTECTED:
	  case VISIBILITY_PUBLIC:
	    break;
	  default:
	    /* Bad visibility format.  Complain and treat it as
	       public.  */
	    {
	      static struct complaint msg = {
		"Unknown visibility `%c' for baseclass", 0, 0};
	      complain (&msg, new -> visibility);
	      new -> visibility = VISIBILITY_PUBLIC;
	    }
	}

      {
	int nbits;
	
	/* The remaining value is the bit offset of the portion of the object
	   corresponding to this baseclass.  Always zero in the absence of
	   multiple inheritance.  */

	new -> field.bitpos = read_huge_number (pp, ',', &nbits);
	if (nbits != 0)
	  return 0;
      }

      /* The last piece of baseclass information is the type of the
	 base class.  Read it, and remember it's type name as this
	 field's name. */

      new -> field.type = read_type (pp, objfile);
      new -> field.name = type_name_no_tag (new -> field.type);

      /* skip trailing ';' and bump count of number of fields seen */
      if (**pp == ';')
	(*pp)++;
      else
	return 0;
    }
  return 1;
}

/* The tail end of stabs for C++ classes that contain a virtual function
   pointer contains a tilde, a %, and a type number.
   The type number refers to the base class (possibly this class itself) which
   contains the vtable pointer for the current class.

   This function is called when we have parsed all the method declarations,
   so we can look for the vptr base class info.  */

static int
read_tilde_fields (fip, pp, type, objfile)
     struct field_info *fip;
     char **pp;
     struct type *type;
     struct objfile *objfile;
{
  register char *p;

  STABS_CONTINUE (pp, objfile);

  /* If we are positioned at a ';', then skip it. */
  if (**pp == ';')
    {
      (*pp)++;
    }

  if (**pp == '~')
    {
      (*pp)++;

      if (**pp == '=' || **pp == '+' || **pp == '-')
	{
	  /* Obsolete flags that used to indicate the presence
	     of constructors and/or destructors. */
	  (*pp)++;
	}

      /* Read either a '%' or the final ';'.  */
      if (*(*pp)++ == '%')
	{
	  /* The next number is the type number of the base class
	     (possibly our own class) which supplies the vtable for
	     this class.  Parse it out, and search that class to find
	     its vtable pointer, and install those into TYPE_VPTR_BASETYPE
	     and TYPE_VPTR_FIELDNO.  */

	  struct type *t;
	  int i;

	  t = read_type (pp, objfile);
	  p = (*pp)++;
	  while (*p != '\0' && *p != ';')
	    {
	      p++;
	    }
	  if (*p == '\0')
	    {
	      /* Premature end of symbol.  */
	      return 0;
	    }
	  
	  TYPE_VPTR_BASETYPE (type) = t;
	  if (type == t)		/* Our own class provides vtbl ptr */
	    {
	      for (i = TYPE_NFIELDS (t) - 1;
		   i >= TYPE_N_BASECLASSES (t);
		   --i)
		{
		  if (! strncmp (TYPE_FIELD_NAME (t, i), vptr_name, 
				 sizeof (vptr_name) - 1))
		    {
		      TYPE_VPTR_FIELDNO (type) = i;
		      goto gotit;
		    }
		}
	      /* Virtual function table field not found.  */
	      complain (&vtbl_notfound_complaint, TYPE_NAME (type));
	      return 0;
	    }
	  else
	    {
	      TYPE_VPTR_FIELDNO (type) = TYPE_VPTR_FIELDNO (t);
	    }

    gotit:
	  *pp = p + 1;
	}
    }
  return 1;
}

static int
attach_fn_fields_to_type (fip, type)
     struct field_info *fip;
     register struct type *type;
{
  register int n;

  for (n = TYPE_NFN_FIELDS (type);
       fip -> fnlist != NULL;
       fip -> fnlist = fip -> fnlist -> next)
    {
      --n;                      /* Circumvent Sun3 compiler bug */
      TYPE_FN_FIELDLISTS (type)[n] = fip -> fnlist -> fn_fieldlist;
    }
  return 1;
}

/* Create the vector of fields, and record how big it is.
   We need this info to record proper virtual function table information
   for this class's virtual functions.  */

static int
attach_fields_to_type (fip, type, objfile)
     struct field_info *fip;
     register struct type *type;
     struct objfile *objfile;
{
  register int nfields = 0;
  register int non_public_fields = 0;
  register struct nextfield *scan;

  /* Count up the number of fields that we have, as well as taking note of
     whether or not there are any non-public fields, which requires us to
     allocate and build the private_field_bits and protected_field_bits
     bitfields. */

  for (scan = fip -> list; scan != NULL; scan = scan -> next)
    {
      nfields++;
      if (scan -> visibility != VISIBILITY_PUBLIC)
	{
	  non_public_fields++;
	}
    }

  /* Now we know how many fields there are, and whether or not there are any
     non-public fields.  Record the field count, allocate space for the
     array of fields, and create blank visibility bitfields if necessary. */

  TYPE_NFIELDS (type) = nfields;
  TYPE_FIELDS (type) = (struct field *)
    TYPE_ALLOC (type, sizeof (struct field) * nfields);
  memset (TYPE_FIELDS (type), 0, sizeof (struct field) * nfields);

  if (non_public_fields)
    {
      ALLOCATE_CPLUS_STRUCT_TYPE (type);

      TYPE_FIELD_PRIVATE_BITS (type) =
	(B_TYPE *) TYPE_ALLOC (type, B_BYTES (nfields));
      B_CLRALL (TYPE_FIELD_PRIVATE_BITS (type), nfields);

      TYPE_FIELD_PROTECTED_BITS (type) =
	(B_TYPE *) TYPE_ALLOC (type, B_BYTES (nfields));
      B_CLRALL (TYPE_FIELD_PROTECTED_BITS (type), nfields);

      TYPE_FIELD_IGNORE_BITS (type) =
	(B_TYPE *) TYPE_ALLOC (type, B_BYTES (nfields));
      B_CLRALL (TYPE_FIELD_IGNORE_BITS (type), nfields);
    }

  /* Copy the saved-up fields into the field vector.  Start from the head
     of the list, adding to the tail of the field array, so that they end
     up in the same order in the array in which they were added to the list. */

  while (nfields-- > 0)
    {
      TYPE_FIELD (type, nfields) = fip -> list -> field;
      switch (fip -> list -> visibility)
	{
	  case VISIBILITY_PRIVATE:
	    SET_TYPE_FIELD_PRIVATE (type, nfields);
	    break;

	  case VISIBILITY_PROTECTED:
	    SET_TYPE_FIELD_PROTECTED (type, nfields);
	    break;

	  case VISIBILITY_IGNORE:
	    SET_TYPE_FIELD_IGNORE (type, nfields);
	    break;

	  case VISIBILITY_PUBLIC:
	    break;

	  default:
	    /* Unknown visibility.  Complain and treat it as public.  */
	    {
	      static struct complaint msg = {
		"Unknown visibility `%c' for field", 0, 0};
	      complain (&msg, fip -> list -> visibility);
	    }
	    break;
	}
      fip -> list = fip -> list -> next;
    }
  return 1;
}

/* Read the description of a structure (or union type) and return an object
   describing the type.

   PP points to a character pointer that points to the next unconsumed token
   in the the stabs string.  For example, given stabs "A:T4=s4a:1,0,32;;",
   *PP will point to "4a:1,0,32;;".

   TYPE points to an incomplete type that needs to be filled in.

   OBJFILE points to the current objfile from which the stabs information is
   being read.  (Note that it is redundant in that TYPE also contains a pointer
   to this same objfile, so it might be a good idea to eliminate it.  FIXME). 
   */

static struct type *
read_struct_type (pp, type, objfile)
     char **pp;
     struct type *type;
     struct objfile *objfile;
{
  struct cleanup *back_to;
  struct field_info fi;

  fi.list = NULL;
  fi.fnlist = NULL;

  back_to = make_cleanup (null_cleanup, 0);

  INIT_CPLUS_SPECIFIC (type);
  TYPE_FLAGS (type) &= ~TYPE_FLAG_STUB;

  /* First comes the total size in bytes.  */

  {
    int nbits;
    TYPE_LENGTH (type) = read_huge_number (pp, 0, &nbits);
    if (nbits != 0)
      return error_type (pp, objfile);
  }

  /* Now read the baseclasses, if any, read the regular C struct or C++
     class member fields, attach the fields to the type, read the C++
     member functions, attach them to the type, and then read any tilde
     field (baseclass specifier for the class holding the main vtable). */

  if (!read_baseclasses (&fi, pp, type, objfile)
      || !read_struct_fields (&fi, pp, type, objfile)
      || !attach_fields_to_type (&fi, type, objfile)
      || !read_member_functions (&fi, pp, type, objfile)
      || !attach_fn_fields_to_type (&fi, type)
      || !read_tilde_fields (&fi, pp, type, objfile))
    {
      type = error_type (pp, objfile);
    }

  do_cleanups (back_to);
  return (type);
}

/* Read a definition of an array type,
   and create and return a suitable type object.
   Also creates a range type which represents the bounds of that
   array.  */

static struct type *
read_array_type (pp, type, objfile)
     register char **pp;
     register struct type *type;
     struct objfile *objfile;
{
  struct type *index_type, *element_type, *range_type;
  int lower, upper;
  int adjustable = 0;
  int nbits;

  /* Format of an array type:
     "ar<index type>;lower;upper;<array_contents_type>".
     OS9000: "arlower,upper;<array_contents_type>".

     Fortran adjustable arrays use Adigits or Tdigits for lower or upper;
     for these, produce a type like float[][].  */

  if (os9k_stabs)
    index_type = builtin_type_int;
  else
    {
      index_type = read_type (pp, objfile);
      if (**pp != ';')
	/* Improper format of array type decl.  */
	return error_type (pp, objfile);
      ++*pp;
    }

  if (!(**pp >= '0' && **pp <= '9') && **pp != '-')
    {
      (*pp)++;
      adjustable = 1;
    }
  lower = read_huge_number (pp, os9k_stabs ? ',' : ';', &nbits);
  if (nbits != 0)
    return error_type (pp, objfile);

  if (!(**pp >= '0' && **pp <= '9') && **pp != '-')
    {
      (*pp)++;
      adjustable = 1;
    }
  upper = read_huge_number (pp, ';', &nbits);
  if (nbits != 0)
    return error_type (pp, objfile);
  
  element_type = read_type (pp, objfile);

  if (adjustable)
    {
      lower = 0;
      upper = -1;
    }

  range_type =
    create_range_type ((struct type *) NULL, index_type, lower, upper);
  type = create_array_type (type, element_type, range_type);

  return type;
}


/* Read a definition of an enumeration type,
   and create and return a suitable type object.
   Also defines the symbols that represent the values of the type.  */

static struct type *
read_enum_type (pp, type, objfile)
     register char **pp;
     register struct type *type;
     struct objfile *objfile;
{
  register char *p;
  char *name;
  register long n;
  register struct symbol *sym;
  int nsyms = 0;
  struct pending **symlist;
  struct pending *osyms, *syms;
  int o_nsyms;
  int nbits;
  int unsigned_enum = 1;

#if 0
  /* FIXME!  The stabs produced by Sun CC merrily define things that ought
     to be file-scope, between N_FN entries, using N_LSYM.  What's a mother
     to do?  For now, force all enum values to file scope.  */
  if (within_function)
    symlist = &local_symbols;
  else
#endif
    symlist = &file_symbols;
  osyms = *symlist;
  o_nsyms = osyms ? osyms->nsyms : 0;

  if (os9k_stabs)
    {
      /* Size.  Perhaps this does not have to be conditionalized on
	 os9k_stabs (assuming the name of an enum constant can't start
	 with a digit).  */
      read_huge_number (pp, 0, &nbits);
      if (nbits != 0)
	return error_type (pp, objfile);
    }

  /* The aix4 compiler emits an extra field before the enum members;
     my guess is it's a type of some sort.  Just ignore it.  */
  if (**pp == '-')
    {
      /* Skip over the type.  */
      while (**pp != ':')
        (*pp)++;

      /* Skip over the colon.  */
      (*pp)++;
    }

  /* Read the value-names and their values.
     The input syntax is NAME:VALUE,NAME:VALUE, and so on.
     A semicolon or comma instead of a NAME means the end.  */
  while (**pp && **pp != ';' && **pp != ',')
    {
      STABS_CONTINUE (pp, objfile);
      p = *pp;
      while (*p != ':') p++;
      name = obsavestring (*pp, p - *pp, &objfile -> symbol_obstack);
      *pp = p + 1;
      n = read_huge_number (pp, ',', &nbits);
      if (nbits != 0)
	return error_type (pp, objfile);

      sym = (struct symbol *)
	obstack_alloc (&objfile -> symbol_obstack, sizeof (struct symbol));
      memset (sym, 0, sizeof (struct symbol));
      SYMBOL_NAME (sym) = name;
      SYMBOL_LANGUAGE (sym) = current_subfile -> language;
      SYMBOL_CLASS (sym) = LOC_CONST;
      SYMBOL_NAMESPACE (sym) = VAR_NAMESPACE;
      SYMBOL_VALUE (sym) = n;
      if (n < 0)
	unsigned_enum = 0;
      add_symbol_to_list (sym, symlist);
      nsyms++;
    }

  if (**pp == ';')
    (*pp)++;			/* Skip the semicolon.  */

  /* Now fill in the fields of the type-structure.  */

  TYPE_LENGTH (type) = TARGET_INT_BIT / HOST_CHAR_BIT;
  TYPE_CODE (type) = TYPE_CODE_ENUM;
  TYPE_FLAGS (type) &= ~TYPE_FLAG_STUB;
  if (unsigned_enum)
    TYPE_FLAGS (type) |= TYPE_FLAG_UNSIGNED;
  TYPE_NFIELDS (type) = nsyms;
  TYPE_FIELDS (type) = (struct field *)
    TYPE_ALLOC (type, sizeof (struct field) * nsyms);
  memset (TYPE_FIELDS (type), 0, sizeof (struct field) * nsyms);

  /* Find the symbols for the values and put them into the type.
     The symbols can be found in the symlist that we put them on
     to cause them to be defined.  osyms contains the old value
     of that symlist; everything up to there was defined by us.  */
  /* Note that we preserve the order of the enum constants, so
     that in something like "enum {FOO, LAST_THING=FOO}" we print
     FOO, not LAST_THING.  */

  for (syms = *symlist, n = nsyms - 1; syms; syms = syms->next)
    {
      int last = syms == osyms ? o_nsyms : 0;
      int j = syms->nsyms;
      for (; --j >= last; --n)
	{
	  struct symbol *xsym = syms->symbol[j];
	  SYMBOL_TYPE (xsym) = type;
	  TYPE_FIELD_NAME (type, n) = SYMBOL_NAME (xsym);
	  TYPE_FIELD_VALUE (type, n) = 0;
	  TYPE_FIELD_BITPOS (type, n) = SYMBOL_VALUE (xsym);
	  TYPE_FIELD_BITSIZE (type, n) = 0;
	}
      if (syms == osyms)
	break;
    }

  return type;
}

/* Sun's ACC uses a somewhat saner method for specifying the builtin
   typedefs in every file (for int, long, etc):

	type = b <signed> <width>; <offset>; <nbits>
	signed = u or s.  Possible c in addition to u or s (for char?).
	offset = offset from high order bit to start bit of type.
	width is # bytes in object of this type, nbits is # bits in type.

   The width/offset stuff appears to be for small objects stored in
   larger ones (e.g. `shorts' in `int' registers).  We ignore it for now,
   FIXME.  */

static struct type *
read_sun_builtin_type (pp, typenums, objfile)
     char **pp;
     int typenums[2];
     struct objfile *objfile;
{
  int type_bits;
  int nbits;
  int signed_type;

  switch (**pp)
    {
      case 's':
        signed_type = 1;
	break;
      case 'u':
	signed_type = 0;
	break;
      default:
	return error_type (pp, objfile);
    }
  (*pp)++;

  /* For some odd reason, all forms of char put a c here.  This is strange
     because no other type has this honor.  We can safely ignore this because
     we actually determine 'char'acterness by the number of bits specified in
     the descriptor.  */

  if (**pp == 'c')
    (*pp)++;

  /* The first number appears to be the number of bytes occupied
     by this type, except that unsigned short is 4 instead of 2.
     Since this information is redundant with the third number,
     we will ignore it.  */
  read_huge_number (pp, ';', &nbits);
  if (nbits != 0)
    return error_type (pp, objfile);

  /* The second number is always 0, so ignore it too. */
  read_huge_number (pp, ';', &nbits);
  if (nbits != 0)
    return error_type (pp, objfile);

  /* The third number is the number of bits for this type. */
  type_bits = read_huge_number (pp, 0, &nbits);
  if (nbits != 0)
    return error_type (pp, objfile);
  /* The type *should* end with a semicolon.  If it are embedded
     in a larger type the semicolon may be the only way to know where
     the type ends.  If this type is at the end of the stabstring we
     can deal with the omitted semicolon (but we don't have to like
     it).  Don't bother to complain(), Sun's compiler omits the semicolon
     for "void".  */
  if (**pp == ';')
    ++(*pp);

  if (type_bits == 0)
    return init_type (TYPE_CODE_VOID, 1,
		      signed_type ? 0 : TYPE_FLAG_UNSIGNED, (char *)NULL,
		      objfile);
  else
    return init_type (TYPE_CODE_INT,
		      type_bits / TARGET_CHAR_BIT,
		      signed_type ? 0 : TYPE_FLAG_UNSIGNED, (char *)NULL,
		      objfile);
}

static struct type *
read_sun_floating_type (pp, typenums, objfile)
     char **pp;
     int typenums[2];
     struct objfile *objfile;
{
  int nbits;
  int details;
  int nbytes;

  /* The first number has more details about the type, for example
     FN_COMPLEX.  */
  details = read_huge_number (pp, ';', &nbits);
  if (nbits != 0)
    return error_type (pp, objfile);

  /* The second number is the number of bytes occupied by this type */
  nbytes = read_huge_number (pp, ';', &nbits);
  if (nbits != 0)
    return error_type (pp, objfile);

  if (details == NF_COMPLEX || details == NF_COMPLEX16
      || details == NF_COMPLEX32)
    /* This is a type we can't handle, but we do know the size.
       We also will be able to give it a name.  */
    return init_type (TYPE_CODE_ERROR, nbytes, 0, NULL, objfile);

  return init_type (TYPE_CODE_FLT, nbytes, 0, NULL, objfile);
}

/* Read a number from the string pointed to by *PP.
   The value of *PP is advanced over the number.
   If END is nonzero, the character that ends the
   number must match END, or an error happens;
   and that character is skipped if it does match.
   If END is zero, *PP is left pointing to that character.

   If the number fits in a long, set *BITS to 0 and return the value.
   If not, set *BITS to be the number of bits in the number and return 0.

   If encounter garbage, set *BITS to -1 and return 0.  */

static long
read_huge_number (pp, end, bits)
     char **pp;
     int end;
     int *bits;
{
  char *p = *pp;
  int sign = 1;
  long n = 0;
  int radix = 10;
  char overflow = 0;
  int nbits = 0;
  int c;
  long upper_limit;
  
  if (*p == '-')
    {
      sign = -1;
      p++;
    }

  /* Leading zero means octal.  GCC uses this to output values larger
     than an int (because that would be hard in decimal).  */
  if (*p == '0')
    {
      radix = 8;
      p++;
    }

  if (os9k_stabs)
    upper_limit = ULONG_MAX / radix;
  else
    upper_limit = LONG_MAX / radix;

  while ((c = *p++) >= '0' && c < ('0' + radix))
    {
      if (n <= upper_limit)
	{
	  n *= radix;
	  n += c - '0';		/* FIXME this overflows anyway */
	}
      else
	overflow = 1;
      
      /* This depends on large values being output in octal, which is
	 what GCC does. */
      if (radix == 8)
	{
	  if (nbits == 0)
	    {
	      if (c == '0')
		/* Ignore leading zeroes.  */
		;
	      else if (c == '1')
		nbits = 1;
	      else if (c == '2' || c == '3')
		nbits = 2;
	      else
		nbits = 3;
	    }
	  else
	    nbits += 3;
	}
    }
  if (end)
    {
      if (c && c != end)
	{
	  if (bits != NULL)
	    *bits = -1;
	  return 0;
	}
    }
  else
    --p;

  *pp = p;
  if (overflow)
    {
      if (nbits == 0)
	{
	  /* Large decimal constants are an error (because it is hard to
	     count how many bits are in them).  */
	  if (bits != NULL)
	    *bits = -1;
	  return 0;
	}
      
      /* -0x7f is the same as 0x80.  So deal with it by adding one to
	 the number of bits.  */
      if (sign == -1)
	++nbits;
      if (bits)
	*bits = nbits;
    }
  else
    {
      if (bits)
	*bits = 0;
      return n * sign;
    }
  /* It's *BITS which has the interesting information.  */
  return 0;
}

static struct type *
read_range_type (pp, typenums, objfile)
     char **pp;
     int typenums[2];
     struct objfile *objfile;
{
  char *orig_pp = *pp;
  int rangenums[2];
  long n2, n3;
  int n2bits, n3bits;
  int self_subrange;
  struct type *result_type;
  struct type *index_type = NULL;

  /* First comes a type we are a subrange of.
     In C it is usually 0, 1 or the type being defined.  */
  if (read_type_number (pp, rangenums) != 0)
    return error_type (pp, objfile);
  self_subrange = (rangenums[0] == typenums[0] &&
		   rangenums[1] == typenums[1]);

  if (**pp == '=')
    {
      *pp = orig_pp;
      index_type = read_type (pp, objfile);
    }

  /* A semicolon should now follow; skip it.  */
  if (**pp == ';')
    (*pp)++;

  /* The remaining two operands are usually lower and upper bounds
     of the range.  But in some special cases they mean something else.  */
  n2 = read_huge_number (pp, ';', &n2bits);
  n3 = read_huge_number (pp, ';', &n3bits);

  if (n2bits == -1 || n3bits == -1)
    return error_type (pp, objfile);

  if (index_type)
    goto handle_true_range;

  /* If limits are huge, must be large integral type.  */
  if (n2bits != 0 || n3bits != 0)
    {
      char got_signed = 0;
      char got_unsigned = 0;
      /* Number of bits in the type.  */
      int nbits = 0;

      /* Range from 0 to <large number> is an unsigned large integral type.  */
      if ((n2bits == 0 && n2 == 0) && n3bits != 0)
	{
	  got_unsigned = 1;
	  nbits = n3bits;
	}
      /* Range from <large number> to <large number>-1 is a large signed
	 integral type.  Take care of the case where <large number> doesn't
	 fit in a long but <large number>-1 does.  */
      else if ((n2bits != 0 && n3bits != 0 && n2bits == n3bits + 1)
	       || (n2bits != 0 && n3bits == 0
		   && (n2bits == sizeof (long) * HOST_CHAR_BIT)
		   && n3 == LONG_MAX))
	{
	  got_signed = 1;
	  nbits = n2bits;
	}

      if (got_signed || got_unsigned)
	{
	  return init_type (TYPE_CODE_INT, nbits / TARGET_CHAR_BIT,
			    got_unsigned ? TYPE_FLAG_UNSIGNED : 0, NULL,
			    objfile);
	}
      else
	return error_type (pp, objfile);
    }

  /* A type defined as a subrange of itself, with bounds both 0, is void.  */
  if (self_subrange && n2 == 0 && n3 == 0)
    return init_type (TYPE_CODE_VOID, 1, 0, NULL, objfile);

  /* If n3 is zero and n2 is positive, we want a floating type,
     and n2 is the width in bytes.

     Fortran programs appear to use this for complex types also,
     and they give no way to distinguish between double and single-complex!

     GDB does not have complex types.

     Just return the complex as a float of that size.  It won't work right
     for the complex values, but at least it makes the file loadable.  */

  if (n3 == 0 && n2 > 0)
    {
      return init_type (TYPE_CODE_FLT, n2, 0, NULL, objfile);
    }

  /* If the upper bound is -1, it must really be an unsigned int.  */

  else if (n2 == 0 && n3 == -1)
    {
      /* It is unsigned int or unsigned long.  */
      /* GCC 2.3.3 uses this for long long too, but that is just a GDB 3.5
	 compatibility hack.  */
      return init_type (TYPE_CODE_INT, TARGET_INT_BIT / TARGET_CHAR_BIT,
			TYPE_FLAG_UNSIGNED, NULL, objfile);
    }

  /* Special case: char is defined (Who knows why) as a subrange of
     itself with range 0-127.  */
  else if (self_subrange && n2 == 0 && n3 == 127)
    return init_type (TYPE_CODE_INT, 1, 0, NULL, objfile);

  else if (current_symbol && SYMBOL_LANGUAGE (current_symbol) == language_chill
	   && !self_subrange)
    goto handle_true_range;

  /* We used to do this only for subrange of self or subrange of int.  */
  else if (n2 == 0)
    {
      if (n3 < 0)
	/* n3 actually gives the size.  */
	return init_type (TYPE_CODE_INT, - n3, TYPE_FLAG_UNSIGNED,
			  NULL, objfile);
      if (n3 == 0xff)
	return init_type (TYPE_CODE_INT, 1, TYPE_FLAG_UNSIGNED, NULL, objfile);
      if (n3 == 0xffff)
	return init_type (TYPE_CODE_INT, 2, TYPE_FLAG_UNSIGNED, NULL, objfile);

      /* -1 is used for the upper bound of (4 byte) "unsigned int" and
	 "unsigned long", and we already checked for that,
	 so don't need to test for it here.  */
    }
  /* I think this is for Convex "long long".  Since I don't know whether
     Convex sets self_subrange, I also accept that particular size regardless
     of self_subrange.  */
  else if (n3 == 0 && n2 < 0
	   && (self_subrange
	       || n2 == - TARGET_LONG_LONG_BIT / TARGET_CHAR_BIT))
    return init_type (TYPE_CODE_INT, - n2, 0, NULL, objfile);
  else if (n2 == -n3 -1)
    {
      if (n3 == 0x7f)
	return init_type (TYPE_CODE_INT, 1, 0, NULL, objfile);
      if (n3 == 0x7fff)
	return init_type (TYPE_CODE_INT, 2, 0, NULL, objfile);
      if (n3 == 0x7fffffff)
	return init_type (TYPE_CODE_INT, 4, 0, NULL, objfile);
    }

  /* We have a real range type on our hands.  Allocate space and
     return a real pointer.  */
 handle_true_range:

  if (self_subrange)
    index_type = builtin_type_int;
  else
    index_type = *dbx_lookup_type (rangenums);
  if (index_type == NULL)
    {
      /* Does this actually ever happen?  Is that why we are worrying
         about dealing with it rather than just calling error_type?  */

      static struct type *range_type_index;

      complain (&range_type_base_complaint, rangenums[1]);
      if (range_type_index == NULL)
	range_type_index =
	  init_type (TYPE_CODE_INT, TARGET_INT_BIT / TARGET_CHAR_BIT,
		     0, "range type index type", NULL);
      index_type = range_type_index;
    }

  result_type = create_range_type ((struct type *) NULL, index_type, n2, n3);
  return (result_type);
}

/* Read in an argument list.  This is a list of types, separated by commas
   and terminated with END.  Return the list of types read in, or (struct type
   **)-1 if there is an error.  */

static struct type **
read_args (pp, end, objfile)
     char **pp;
     int end;
     struct objfile *objfile;
{
  /* FIXME!  Remove this arbitrary limit!  */
  struct type *types[1024], **rval; /* allow for fns of 1023 parameters */
  int n = 0;

  while (**pp != end)
    {
      if (**pp != ',')
	/* Invalid argument list: no ','.  */
	return (struct type **)-1;
      (*pp)++;
      STABS_CONTINUE (pp, objfile);
      types[n++] = read_type (pp, objfile);
    }
  (*pp)++;			/* get past `end' (the ':' character) */

  if (n == 1)
    {
      rval = (struct type **) xmalloc (2 * sizeof (struct type *));
    }
  else if (TYPE_CODE (types[n-1]) != TYPE_CODE_VOID)
    {
      rval = (struct type **) xmalloc ((n + 1) * sizeof (struct type *));
      memset (rval + n, 0, sizeof (struct type *));
    }
  else
    {
      rval = (struct type **) xmalloc (n * sizeof (struct type *));
    }
  memcpy (rval, types, n * sizeof (struct type *));
  return rval;
}

/* Common block handling.  */

/* List of symbols declared since the last BCOMM.  This list is a tail
   of local_symbols.  When ECOMM is seen, the symbols on the list
   are noted so their proper addresses can be filled in later,
   using the common block base address gotten from the assembler
   stabs.  */

static struct pending *common_block;
static int common_block_i;

/* Name of the current common block.  We get it from the BCOMM instead of the
   ECOMM to match IBM documentation (even though IBM puts the name both places
   like everyone else).  */
static char *common_block_name;

/* Process a N_BCOMM symbol.  The storage for NAME is not guaranteed
   to remain after this function returns.  */

void
common_block_start (name, objfile)
     char *name;
     struct objfile *objfile;
{
  if (common_block_name != NULL)
    {
      static struct complaint msg = {
	"Invalid symbol data: common block within common block",
	0, 0};
      complain (&msg);
    }
  common_block = local_symbols;
  common_block_i = local_symbols ? local_symbols->nsyms : 0;
  common_block_name = obsavestring (name, strlen (name),
				    &objfile -> symbol_obstack);
}

/* Process a N_ECOMM symbol.  */

void
common_block_end (objfile)
     struct objfile *objfile;
{
  /* Symbols declared since the BCOMM are to have the common block
     start address added in when we know it.  common_block and
     common_block_i point to the first symbol after the BCOMM in
     the local_symbols list; copy the list and hang it off the
     symbol for the common block name for later fixup.  */
  int i;
  struct symbol *sym;
  struct pending *new = 0;
  struct pending *next;
  int j;

  if (common_block_name == NULL)
    {
      static struct complaint msg = {"ECOMM symbol unmatched by BCOMM", 0, 0};
      complain (&msg);
      return;
    }

  sym = (struct symbol *) 
    obstack_alloc (&objfile -> symbol_obstack, sizeof (struct symbol));
  memset (sym, 0, sizeof (struct symbol));
  SYMBOL_NAME (sym) = common_block_name;
  SYMBOL_CLASS (sym) = LOC_BLOCK;

  /* Now we copy all the symbols which have been defined since the BCOMM.  */

  /* Copy all the struct pendings before common_block.  */
  for (next = local_symbols;
       next != NULL && next != common_block;
       next = next->next)
    {
      for (j = 0; j < next->nsyms; j++)
	add_symbol_to_list (next->symbol[j], &new);
    }

  /* Copy however much of COMMON_BLOCK we need.  If COMMON_BLOCK is
     NULL, it means copy all the local symbols (which we already did
     above).  */

  if (common_block != NULL)
    for (j = common_block_i; j < common_block->nsyms; j++)
      add_symbol_to_list (common_block->symbol[j], &new);

  SYMBOL_TYPE (sym) = (struct type *) new;

  /* Should we be putting local_symbols back to what it was?
     Does it matter?  */

  i = hashname (SYMBOL_NAME (sym));
  SYMBOL_VALUE_CHAIN (sym) = global_sym_chain[i];
  global_sym_chain[i] = sym;
  common_block_name = NULL;
}

/* Add a common block's start address to the offset of each symbol
   declared to be in it (by being between a BCOMM/ECOMM pair that uses
   the common block name).  */

static void
fix_common_block (sym, valu)
    struct symbol *sym;
    int valu;
{
  struct pending *next = (struct pending *) SYMBOL_TYPE (sym);
  for ( ; next; next = next->next)
    {
      register int j;
      for (j = next->nsyms - 1; j >= 0; j--)
	SYMBOL_VALUE_ADDRESS (next->symbol[j]) += valu;
    }
}



/* What about types defined as forward references inside of a small lexical
   scope?  */
/* Add a type to the list of undefined types to be checked through
   once this file has been read in.  */

void
add_undefined_type (type)
     struct type *type;
{
  if (undef_types_length == undef_types_allocated)
    {
      undef_types_allocated *= 2;
      undef_types = (struct type **)
	xrealloc ((char *) undef_types,
		  undef_types_allocated * sizeof (struct type *));
    }
  undef_types[undef_types_length++] = type;
}

/* Go through each undefined type, see if it's still undefined, and fix it
   up if possible.  We have two kinds of undefined types:

   TYPE_CODE_ARRAY:  Array whose target type wasn't defined yet.
			Fix:  update array length using the element bounds
			and the target type's length.
   TYPE_CODE_STRUCT, TYPE_CODE_UNION:  Structure whose fields were not
			yet defined at the time a pointer to it was made.
   			Fix:  Do a full lookup on the struct/union tag.  */
void
cleanup_undefined_types ()
{
  struct type **type;

  for (type = undef_types; type < undef_types + undef_types_length; type++)
    {
      switch (TYPE_CODE (*type))
	{

	  case TYPE_CODE_STRUCT:
	  case TYPE_CODE_UNION:
	  case TYPE_CODE_ENUM:
	  {
	    /* Check if it has been defined since.  Need to do this here
	       as well as in check_typedef to deal with the (legitimate in
	       C though not C++) case of several types with the same name
	       in different source files.  */
	    if (TYPE_FLAGS (*type) & TYPE_FLAG_STUB)
	      {
		struct pending *ppt;
		int i;
		/* Name of the type, without "struct" or "union" */
		char *typename = TYPE_TAG_NAME (*type);

		if (typename == NULL)
		  {
		    static struct complaint msg = {"need a type name", 0, 0};
		    complain (&msg);
		    break;
		  }
		for (ppt = file_symbols; ppt; ppt = ppt->next)
		  {
		    for (i = 0; i < ppt->nsyms; i++)
		      {
			struct symbol *sym = ppt->symbol[i];
			
			if (SYMBOL_CLASS (sym) == LOC_TYPEDEF
			    && SYMBOL_NAMESPACE (sym) == STRUCT_NAMESPACE
			    && (TYPE_CODE (SYMBOL_TYPE (sym)) ==
				TYPE_CODE (*type))
			    && STREQ (SYMBOL_NAME (sym), typename))
			  {
			    memcpy (*type, SYMBOL_TYPE (sym),
				    sizeof (struct type));
			  }
		      }
		  }
	      }
	  }
	  break;

	default:
	  {
	    static struct complaint msg = {"\
GDB internal error.  cleanup_undefined_types with bad type %d.", 0, 0};
	    complain (&msg, TYPE_CODE (*type));
	  }
	  break;
	}
    }

  undef_types_length = 0;
}

/* Scan through all of the global symbols defined in the object file,
   assigning values to the debugging symbols that need to be assigned
   to.  Get these symbols from the minimal symbol table.  */

void
scan_file_globals (objfile)
     struct objfile *objfile;
{
  int hash;
  struct minimal_symbol *msymbol;
  struct symbol *sym, *prev;

  /* Avoid expensive loop through all minimal symbols if there are
     no unresolved symbols.  */
  for (hash = 0; hash < HASHSIZE; hash++)
    {
      if (global_sym_chain[hash])
	break;
    }
  if (hash >= HASHSIZE)
    return;

  for (msymbol = objfile -> msymbols;
       msymbol && SYMBOL_NAME (msymbol) != NULL;
       msymbol++)
    {
      QUIT;

      /* Skip static symbols.  */
      switch (MSYMBOL_TYPE (msymbol))
	{
	case mst_file_text:
	case mst_file_data:
	case mst_file_bss:
	  continue;
	default:
	  break;
	}

      prev = NULL;

      /* Get the hash index and check all the symbols
	 under that hash index. */

      hash = hashname (SYMBOL_NAME (msymbol));

      for (sym = global_sym_chain[hash]; sym;)
	{
	  if (SYMBOL_NAME (msymbol)[0] == SYMBOL_NAME (sym)[0] &&
	      STREQ(SYMBOL_NAME (msymbol) + 1, SYMBOL_NAME (sym) + 1))
	    {
	      /* Splice this symbol out of the hash chain and
		 assign the value we have to it. */
	      if (prev)
		{
		  SYMBOL_VALUE_CHAIN (prev) = SYMBOL_VALUE_CHAIN (sym);
		}
	      else
		{
		  global_sym_chain[hash] = SYMBOL_VALUE_CHAIN (sym);
		}
	      
	      /* Check to see whether we need to fix up a common block.  */
	      /* Note: this code might be executed several times for
		 the same symbol if there are multiple references.  */

	      if (SYMBOL_CLASS (sym) == LOC_BLOCK)
		{
		  fix_common_block (sym, SYMBOL_VALUE_ADDRESS (msymbol));
		}
	      else
		{
		  SYMBOL_VALUE_ADDRESS (sym) = SYMBOL_VALUE_ADDRESS (msymbol);
		}

	      SYMBOL_SECTION (sym) = SYMBOL_SECTION (msymbol);
	      
	      if (prev)
		{
		  sym = SYMBOL_VALUE_CHAIN (prev);
		}
	      else
		{
		  sym = global_sym_chain[hash];
		}
	    }
	  else
	    {
	      prev = sym;
	      sym = SYMBOL_VALUE_CHAIN (sym);
	    }
	}
    }

  /* Change the storage class of any remaining unresolved globals to
     LOC_UNRESOLVED and remove them from the chain.  */
  for (hash = 0; hash < HASHSIZE; hash++)
    {
      sym = global_sym_chain[hash];
      while (sym)
	{
	  prev = sym;
	  sym = SYMBOL_VALUE_CHAIN (sym);

	  /* Change the symbol address from the misleading chain value
	     to address zero.  */
	  SYMBOL_VALUE_ADDRESS (prev) = 0;

	  /* Complain about unresolved common block symbols.  */
	  if (SYMBOL_CLASS (prev) == LOC_STATIC)
	    SYMBOL_CLASS (prev) = LOC_UNRESOLVED;
	  else
	    complain (&unresolved_sym_chain_complaint,
		      objfile->name, SYMBOL_NAME (prev));
	}
    }
  memset (global_sym_chain, 0, sizeof (global_sym_chain));
}

/* Initialize anything that needs initializing when starting to read
   a fresh piece of a symbol file, e.g. reading in the stuff corresponding
   to a psymtab.  */

void
stabsread_init ()
{
}

/* Initialize anything that needs initializing when a completely new
   symbol file is specified (not just adding some symbols from another
   file, e.g. a shared library).  */

void
stabsread_new_init ()
{
  /* Empty the hash table of global syms looking for values.  */
  memset (global_sym_chain, 0, sizeof (global_sym_chain));
}

/* Initialize anything that needs initializing at the same time as
   start_symtab() is called. */

void start_stabs ()
{
  global_stabs = NULL;		/* AIX COFF */
  /* Leave FILENUM of 0 free for builtin types and this file's types.  */
  n_this_object_header_files = 1;
  type_vector_length = 0;
  type_vector = (struct type **) 0;

  /* FIXME: If common_block_name is not already NULL, we should complain().  */
  common_block_name = NULL;

  os9k_stabs = 0;
}

/* Call after end_symtab() */

void end_stabs ()
{
  if (type_vector)
    {
      free ((char *) type_vector);
    }
  type_vector = 0;
  type_vector_length = 0;
  previous_stab_code = 0;
}

void
finish_global_stabs (objfile)
     struct objfile *objfile;
{
  if (global_stabs)
    {
      patch_block_stabs (global_symbols, global_stabs, objfile);
      free ((PTR) global_stabs);
      global_stabs = NULL;
    }
}

/* Initializer for this module */

void
_initialize_stabsread ()
{
  undef_types_allocated = 20;
  undef_types_length = 0;
  undef_types = (struct type **)
    xmalloc (undef_types_allocated * sizeof (struct type *));
}
