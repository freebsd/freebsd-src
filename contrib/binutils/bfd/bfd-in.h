/* Main header file for the bfd library -- portable access to object files.
   Copyright 1990, 91, 92, 93, 94, 95, 96, 97, 98, 99, 2000
   Free Software Foundation, Inc.
   Contributed by Cygnus Support.

** NOTE: bfd.h and bfd-in2.h are GENERATED files.  Don't change them;
** instead, change bfd-in.h or the other BFD source files processed to
** generate these files.

This file is part of BFD, the Binary File Descriptor library.

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

/* bfd.h -- The only header file required by users of the bfd library 

The bfd.h file is generated from bfd-in.h and various .c files; if you
change it, your changes will probably be lost.

All the prototypes and definitions following the comment "THE FOLLOWING
IS EXTRACTED FROM THE SOURCE" are extracted from the source files for
BFD.  If you change it, someone oneday will extract it from the source
again, and your changes will be lost.  To save yourself from this bind,
change the definitions in the source in the bfd directory.  Type "make
docs" and then "make headers" in that directory, and magically this file
will change to reflect your changes.

If you don't have the tools to perform the extraction, then you are
safe from someone on your system trampling over your header files.
You should still maintain the equivalence between the source and this
file though; every change you make to the .c file should be reflected
here.  */

#ifndef __BFD_H_SEEN__
#define __BFD_H_SEEN__

#ifdef __cplusplus
extern "C" {
#endif

#include "ansidecl.h"

/* These two lines get substitutions done by commands in Makefile.in.  */
#define BFD_VERSION  "@VERSION@"
#define BFD_ARCH_SIZE @wordsize@
#define BFD_HOST_64BIT_LONG @BFD_HOST_64BIT_LONG@
#if @BFD_HOST_64_BIT_DEFINED@
#define BFD_HOST_64_BIT @BFD_HOST_64_BIT@
#define BFD_HOST_U_64_BIT @BFD_HOST_U_64_BIT@
#endif

#if BFD_ARCH_SIZE >= 64
#define BFD64
#endif

#ifndef INLINE
#if __GNUC__ >= 2
#define INLINE __inline__
#else
#define INLINE
#endif
#endif

/* forward declaration */
typedef struct _bfd bfd;

/* To squelch erroneous compiler warnings ("illegal pointer
   combination") from the SVR3 compiler, we would like to typedef
   boolean to int (it doesn't like functions which return boolean.
   Making sure they are never implicitly declared to return int
   doesn't seem to help).  But this file is not configured based on
   the host.  */
/* General rules: functions which are boolean return true on success
   and false on failure (unless they're a predicate).   -- bfd.doc */
/* I'm sure this is going to break something and someone is going to
   force me to change it. */
/* typedef enum boolean {false, true} boolean; */
/* Yup, SVR4 has a "typedef enum boolean" in <sys/types.h>  -fnf */
/* It gets worse if the host also defines a true/false enum... -sts */
/* And even worse if your compiler has built-in boolean types... -law */
#if defined (__GNUG__) && (__GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 6))
#define TRUE_FALSE_ALREADY_DEFINED
#endif
#ifdef MPW
/* Pre-emptive strike - get the file with the enum. */
#include <Types.h>
#define TRUE_FALSE_ALREADY_DEFINED
#endif /* MPW */
#ifndef TRUE_FALSE_ALREADY_DEFINED
typedef enum bfd_boolean {false, true} boolean;
#define BFD_TRUE_FALSE
#else
/* Use enum names that will appear nowhere else.  */
typedef enum bfd_boolean {bfd_fffalse, bfd_tttrue} boolean;
#endif

/* A pointer to a position in a file.  */
/* FIXME:  This should be using off_t from <sys/types.h>.
   For now, try to avoid breaking stuff by not including <sys/types.h> here.
   This will break on systems with 64-bit file offsets (e.g. 4.4BSD).
   Probably the best long-term answer is to avoid using file_ptr AND off_t 
   in this header file, and to handle this in the BFD implementation
   rather than in its interface.  */
/* typedef off_t	file_ptr; */
typedef long int file_ptr;

/* Support for different sizes of target format ints and addresses.
   If the type `long' is at least 64 bits, BFD_HOST_64BIT_LONG will be
   set to 1 above.  Otherwise, if gcc is being used, this code will
   use gcc's "long long" type.  Otherwise, BFD_HOST_64_BIT must be
   defined above.  */

#ifndef BFD_HOST_64_BIT
# if BFD_HOST_64BIT_LONG
#  define BFD_HOST_64_BIT long
#  define BFD_HOST_U_64_BIT unsigned long
# else
#  ifdef __GNUC__
#   if __GNUC__ >= 2
#    define BFD_HOST_64_BIT long long
#    define BFD_HOST_U_64_BIT unsigned long long
#   endif /* __GNUC__ >= 2 */
#  endif /* ! defined (__GNUC__) */
# endif /* ! BFD_HOST_64BIT_LONG */
#endif /* ! defined (BFD_HOST_64_BIT) */

#ifdef BFD64

#ifndef BFD_HOST_64_BIT
 #error No 64 bit integer type available
#endif /* ! defined (BFD_HOST_64_BIT) */

typedef BFD_HOST_U_64_BIT bfd_vma;
typedef BFD_HOST_64_BIT bfd_signed_vma;
typedef BFD_HOST_U_64_BIT bfd_size_type;
typedef BFD_HOST_U_64_BIT symvalue;

#ifndef fprintf_vma
#if BFD_HOST_64BIT_LONG
#define sprintf_vma(s,x) sprintf (s, "%016lx", x)
#define fprintf_vma(f,x) fprintf (f, "%016lx", x)
#else
#define _bfd_int64_low(x) ((unsigned long) (((x) & 0xffffffff)))
#define _bfd_int64_high(x) ((unsigned long) (((x) >> 32) & 0xffffffff))
#define fprintf_vma(s,x) \
  fprintf ((s), "%08lx%08lx", _bfd_int64_high (x), _bfd_int64_low (x))
#define sprintf_vma(s,x) \
  sprintf ((s), "%08lx%08lx", _bfd_int64_high (x), _bfd_int64_low (x))
#endif
#endif

#else /* not BFD64  */

/* Represent a target address.  Also used as a generic unsigned type
   which is guaranteed to be big enough to hold any arithmetic types
   we need to deal with.  */
typedef unsigned long bfd_vma;

/* A generic signed type which is guaranteed to be big enough to hold any
   arithmetic types we need to deal with.  Can be assumed to be compatible
   with bfd_vma in the same way that signed and unsigned ints are compatible
   (as parameters, in assignment, etc).  */
typedef long bfd_signed_vma;

typedef unsigned long symvalue;
typedef unsigned long bfd_size_type;

/* Print a bfd_vma x on stream s.  */
#define fprintf_vma(s,x) fprintf(s, "%08lx", x)
#define sprintf_vma(s,x) sprintf(s, "%08lx", x)

#endif /* not BFD64  */

#define printf_vma(x) fprintf_vma(stdout,x)

typedef unsigned int flagword;	/* 32 bits of flags */
typedef unsigned char bfd_byte;

/** File formats */

typedef enum bfd_format {
	      bfd_unknown = 0,	/* file format is unknown */
	      bfd_object,	/* linker/assember/compiler output */
	      bfd_archive,	/* object archive file */
	      bfd_core,		/* core dump */
	      bfd_type_end}	/* marks the end; don't use it! */
         bfd_format;

/* Values that may appear in the flags field of a BFD.  These also
   appear in the object_flags field of the bfd_target structure, where
   they indicate the set of flags used by that backend (not all flags
   are meaningful for all object file formats) (FIXME: at the moment,
   the object_flags values have mostly just been copied from backend
   to another, and are not necessarily correct).  */

/* No flags.  */
#define BFD_NO_FLAGS   	0x00

/* BFD contains relocation entries.  */
#define HAS_RELOC   	0x01

/* BFD is directly executable.  */
#define EXEC_P      	0x02

/* BFD has line number information (basically used for F_LNNO in a
   COFF header).  */
#define HAS_LINENO  	0x04

/* BFD has debugging information.  */
#define HAS_DEBUG   	0x08

/* BFD has symbols.  */
#define HAS_SYMS    	0x10

/* BFD has local symbols (basically used for F_LSYMS in a COFF
   header).  */
#define HAS_LOCALS  	0x20

/* BFD is a dynamic object.  */
#define DYNAMIC     	0x40

/* Text section is write protected (if D_PAGED is not set, this is
   like an a.out NMAGIC file) (the linker sets this by default, but
   clears it for -r or -N).  */
#define WP_TEXT     	0x80

/* BFD is dynamically paged (this is like an a.out ZMAGIC file) (the
   linker sets this by default, but clears it for -r or -n or -N).  */
#define D_PAGED     	0x100

/* BFD is relaxable (this means that bfd_relax_section may be able to
   do something) (sometimes bfd_relax_section can do something even if
   this is not set).  */
#define BFD_IS_RELAXABLE 0x200

/* This may be set before writing out a BFD to request using a
   traditional format.  For example, this is used to request that when
   writing out an a.out object the symbols not be hashed to eliminate
   duplicates.  */
#define BFD_TRADITIONAL_FORMAT 0x400

/* This flag indicates that the BFD contents are actually cached in
   memory.  If this is set, iostream points to a bfd_in_memory struct.  */
#define BFD_IN_MEMORY 0x800

/* symbols and relocation */

/* A count of carsyms (canonical archive symbols).  */
typedef unsigned long symindex;

/* How to perform a relocation.  */
typedef const struct reloc_howto_struct reloc_howto_type;

#define BFD_NO_MORE_SYMBOLS ((symindex) ~0)

/* General purpose part of a symbol X;
   target specific parts are in libcoff.h, libaout.h, etc.  */

#define bfd_get_section(x) ((x)->section)
#define bfd_get_output_section(x) ((x)->section->output_section)
#define bfd_set_section(x,y) ((x)->section) = (y)
#define bfd_asymbol_base(x) ((x)->section->vma)
#define bfd_asymbol_value(x) (bfd_asymbol_base(x) + (x)->value)
#define bfd_asymbol_name(x) ((x)->name)
/*Perhaps future: #define bfd_asymbol_bfd(x) ((x)->section->owner)*/
#define bfd_asymbol_bfd(x) ((x)->the_bfd)
#define bfd_asymbol_flavour(x) (bfd_asymbol_bfd(x)->xvec->flavour)

/* A canonical archive symbol.  */
/* This is a type pun with struct ranlib on purpose! */
typedef struct carsym {
  char *name;
  file_ptr file_offset;		/* look here to find the file */
} carsym;			/* to make these you call a carsymogen */

  
/* Used in generating armaps (archive tables of contents).
   Perhaps just a forward definition would do? */
struct orl {			/* output ranlib */
  char **name;			/* symbol name */ 
  file_ptr pos;			/* bfd* or file position */
  int namidx;			/* index into string table */
};


/* Linenumber stuff */
typedef struct lineno_cache_entry {
  unsigned int line_number;	/* Linenumber from start of function*/  
  union {
    struct symbol_cache_entry *sym; /* Function name */
    unsigned long offset;	/* Offset into section */
  } u;
} alent;

/* object and core file sections */

#define	align_power(addr, align)	\
	( ((addr) + ((1<<(align))-1)) & (-1 << (align)))

typedef struct sec *sec_ptr;

#define bfd_get_section_name(bfd, ptr) ((ptr)->name + 0)
#define bfd_get_section_vma(bfd, ptr) ((ptr)->vma + 0)
#define bfd_get_section_alignment(bfd, ptr) ((ptr)->alignment_power + 0)
#define bfd_section_name(bfd, ptr) ((ptr)->name)
#define bfd_section_size(bfd, ptr) (bfd_get_section_size_before_reloc(ptr))
#define bfd_section_vma(bfd, ptr) ((ptr)->vma)
#define bfd_section_lma(bfd, ptr) ((ptr)->lma)
#define bfd_section_alignment(bfd, ptr) ((ptr)->alignment_power)
#define bfd_get_section_flags(bfd, ptr) ((ptr)->flags + 0)
#define bfd_get_section_userdata(bfd, ptr) ((ptr)->userdata)

#define bfd_is_com_section(ptr) (((ptr)->flags & SEC_IS_COMMON) != 0)

#define bfd_set_section_vma(bfd, ptr, val) (((ptr)->vma = (ptr)->lma= (val)), ((ptr)->user_set_vma = (boolean)true), true)
#define bfd_set_section_alignment(bfd, ptr, val) (((ptr)->alignment_power = (val)),true)
#define bfd_set_section_userdata(bfd, ptr, val) (((ptr)->userdata = (val)),true)

typedef struct stat stat_type; 

typedef enum bfd_print_symbol
{ 
  bfd_print_symbol_name,
  bfd_print_symbol_more,
  bfd_print_symbol_all
} bfd_print_symbol_type;
    
/* Information about a symbol that nm needs.  */

typedef struct _symbol_info
{
  symvalue value;
  char type;
  CONST char *name;            /* Symbol name.  */
  unsigned char stab_type;     /* Stab type.  */
  char stab_other;             /* Stab other. */
  short stab_desc;             /* Stab desc.  */
  CONST char *stab_name;       /* String for stab type.  */
} symbol_info;

/* Get the name of a stabs type code.  */

extern const char *bfd_get_stab_name PARAMS ((int));

/* Hash table routines.  There is no way to free up a hash table.  */

/* An element in the hash table.  Most uses will actually use a larger
   structure, and an instance of this will be the first field.  */

struct bfd_hash_entry
{
  /* Next entry for this hash code.  */
  struct bfd_hash_entry *next;
  /* String being hashed.  */
  const char *string;
  /* Hash code.  This is the full hash code, not the index into the
     table.  */
  unsigned long hash;
};

/* A hash table.  */

struct bfd_hash_table
{
  /* The hash array.  */
  struct bfd_hash_entry **table;
  /* The number of slots in the hash table.  */
  unsigned int size;
  /* A function used to create new elements in the hash table.  The
     first entry is itself a pointer to an element.  When this
     function is first invoked, this pointer will be NULL.  However,
     having the pointer permits a hierarchy of method functions to be
     built each of which calls the function in the superclass.  Thus
     each function should be written to allocate a new block of memory
     only if the argument is NULL.  */
  struct bfd_hash_entry *(*newfunc) PARAMS ((struct bfd_hash_entry *,
					     struct bfd_hash_table *,
					     const char *));
   /* An objalloc for this hash table.  This is a struct objalloc *,
     but we use PTR to avoid requiring the inclusion of objalloc.h.  */
  PTR memory;
};

/* Initialize a hash table.  */
extern boolean bfd_hash_table_init
  PARAMS ((struct bfd_hash_table *,
	   struct bfd_hash_entry *(*) (struct bfd_hash_entry *,
				       struct bfd_hash_table *,
				       const char *)));

/* Initialize a hash table specifying a size.  */
extern boolean bfd_hash_table_init_n
  PARAMS ((struct bfd_hash_table *,
	   struct bfd_hash_entry *(*) (struct bfd_hash_entry *,
				       struct bfd_hash_table *,
				       const char *),
	   unsigned int size));

/* Free up a hash table.  */
extern void bfd_hash_table_free PARAMS ((struct bfd_hash_table *));

/* Look up a string in a hash table.  If CREATE is true, a new entry
   will be created for this string if one does not already exist.  The
   COPY argument must be true if this routine should copy the string
   into newly allocated memory when adding an entry.  */
extern struct bfd_hash_entry *bfd_hash_lookup
  PARAMS ((struct bfd_hash_table *, const char *, boolean create,
	   boolean copy));

/* Replace an entry in a hash table.  */
extern void bfd_hash_replace
  PARAMS ((struct bfd_hash_table *, struct bfd_hash_entry *old,
	   struct bfd_hash_entry *nw));

/* Base method for creating a hash table entry.  */
extern struct bfd_hash_entry *bfd_hash_newfunc
  PARAMS ((struct bfd_hash_entry *, struct bfd_hash_table *,
	   const char *));

/* Grab some space for a hash table entry.  */
extern PTR bfd_hash_allocate PARAMS ((struct bfd_hash_table *,
				      unsigned int));

/* Traverse a hash table in a random order, calling a function on each
   element.  If the function returns false, the traversal stops.  The
   INFO argument is passed to the function.  */
extern void bfd_hash_traverse PARAMS ((struct bfd_hash_table *,
				       boolean (*) (struct bfd_hash_entry *,
						    PTR),
				       PTR info));

/* Semi-portable string concatenation in cpp.
   The CAT4 hack is to avoid a problem with some strict ANSI C preprocessors.
   The problem is, "32_" is not a valid preprocessing token, and we don't
   want extra underscores (e.g., "nlm_32_").  The XCAT2 macro will cause the
   inner CAT macros to be evaluated first, producing still-valid pp-tokens.
   Then the final concatenation can be done.  (Sigh.)  */
#ifndef CAT
#ifdef SABER
#define CAT(a,b)	a##b
#define CAT3(a,b,c)	a##b##c
#define CAT4(a,b,c,d)	a##b##c##d
#else
#if defined(__STDC__) || defined(ALMOST_STDC)
#define CAT(a,b) a##b
#define CAT3(a,b,c) a##b##c
#define XCAT2(a,b)	CAT(a,b)
#define CAT4(a,b,c,d)	XCAT2(CAT(a,b),CAT(c,d))
#else
#define CAT(a,b) a/**/b
#define CAT3(a,b,c) a/**/b/**/c
#define CAT4(a,b,c,d)	a/**/b/**/c/**/d
#endif
#endif
#endif

#define COFF_SWAP_TABLE (PTR) &bfd_coff_std_swap_table

/* User program access to BFD facilities */

/* Direct I/O routines, for programs which know more about the object
   file than BFD does.  Use higher level routines if possible.  */

extern bfd_size_type bfd_read
  PARAMS ((PTR, bfd_size_type size, bfd_size_type nitems, bfd *abfd));
extern bfd_size_type bfd_write
  PARAMS ((const PTR, bfd_size_type size, bfd_size_type nitems, bfd *abfd));
extern int bfd_seek PARAMS ((bfd *abfd, file_ptr fp, int direction));
extern long bfd_tell PARAMS ((bfd *abfd));
extern int bfd_flush PARAMS ((bfd *abfd));
extern int bfd_stat PARAMS ((bfd *abfd, struct stat *));


/* Cast from const char * to char * so that caller can assign to
   a char * without a warning.  */
#define bfd_get_filename(abfd) ((char *) (abfd)->filename)
#define bfd_get_cacheable(abfd) ((abfd)->cacheable)
#define bfd_get_format(abfd) ((abfd)->format)
#define bfd_get_target(abfd) ((abfd)->xvec->name)
#define bfd_get_flavour(abfd) ((abfd)->xvec->flavour)
#define bfd_big_endian(abfd) ((abfd)->xvec->byteorder == BFD_ENDIAN_BIG)
#define bfd_little_endian(abfd) ((abfd)->xvec->byteorder == BFD_ENDIAN_LITTLE)
#define bfd_header_big_endian(abfd) \
  ((abfd)->xvec->header_byteorder == BFD_ENDIAN_BIG)
#define bfd_header_little_endian(abfd) \
  ((abfd)->xvec->header_byteorder == BFD_ENDIAN_LITTLE)
#define bfd_get_file_flags(abfd) ((abfd)->flags)
#define bfd_applicable_file_flags(abfd) ((abfd)->xvec->object_flags)
#define bfd_applicable_section_flags(abfd) ((abfd)->xvec->section_flags)
#define bfd_my_archive(abfd) ((abfd)->my_archive)
#define bfd_has_map(abfd) ((abfd)->has_armap)

#define bfd_valid_reloc_types(abfd) ((abfd)->xvec->valid_reloc_types)
#define bfd_usrdata(abfd) ((abfd)->usrdata)

#define bfd_get_start_address(abfd) ((abfd)->start_address)
#define bfd_get_symcount(abfd) ((abfd)->symcount)
#define bfd_get_outsymbols(abfd) ((abfd)->outsymbols)
#define bfd_count_sections(abfd) ((abfd)->section_count)

#define bfd_get_symbol_leading_char(abfd) ((abfd)->xvec->symbol_leading_char)

#define bfd_set_cacheable(abfd,bool) (((abfd)->cacheable = (boolean)(bool)), true)

extern boolean bfd_record_phdr
  PARAMS ((bfd *, unsigned long, boolean, flagword, boolean, bfd_vma,
	   boolean, boolean, unsigned int, struct sec **));

/* Byte swapping routines.  */

bfd_vma		bfd_getb64	   PARAMS ((const unsigned char *));
bfd_vma 	bfd_getl64	   PARAMS ((const unsigned char *));
bfd_signed_vma	bfd_getb_signed_64 PARAMS ((const unsigned char *));
bfd_signed_vma	bfd_getl_signed_64 PARAMS ((const unsigned char *));
bfd_vma		bfd_getb32	   PARAMS ((const unsigned char *));
bfd_vma		bfd_getl32	   PARAMS ((const unsigned char *));
bfd_signed_vma	bfd_getb_signed_32 PARAMS ((const unsigned char *));
bfd_signed_vma	bfd_getl_signed_32 PARAMS ((const unsigned char *));
bfd_vma		bfd_getb16	   PARAMS ((const unsigned char *));
bfd_vma		bfd_getl16	   PARAMS ((const unsigned char *));
bfd_signed_vma	bfd_getb_signed_16 PARAMS ((const unsigned char *));
bfd_signed_vma	bfd_getl_signed_16 PARAMS ((const unsigned char *));
void		bfd_putb64	   PARAMS ((bfd_vma, unsigned char *));
void		bfd_putl64	   PARAMS ((bfd_vma, unsigned char *));
void		bfd_putb32	   PARAMS ((bfd_vma, unsigned char *));
void		bfd_putl32	   PARAMS ((bfd_vma, unsigned char *));
void		bfd_putb16	   PARAMS ((bfd_vma, unsigned char *));
void		bfd_putl16	   PARAMS ((bfd_vma, unsigned char *));

/* Externally visible ECOFF routines.  */

#if defined(__STDC__) || defined(ALMOST_STDC)
struct ecoff_debug_info;
struct ecoff_debug_swap;
struct ecoff_extr;
struct symbol_cache_entry;
struct bfd_link_info;
struct bfd_link_hash_entry;
struct bfd_elf_version_tree;
#endif
extern bfd_vma bfd_ecoff_get_gp_value PARAMS ((bfd * abfd));
extern boolean bfd_ecoff_set_gp_value PARAMS ((bfd *abfd, bfd_vma gp_value));
extern boolean bfd_ecoff_set_regmasks
  PARAMS ((bfd *abfd, unsigned long gprmask, unsigned long fprmask,
	   unsigned long *cprmask));
extern PTR bfd_ecoff_debug_init
  PARAMS ((bfd *output_bfd, struct ecoff_debug_info *output_debug,
	   const struct ecoff_debug_swap *output_swap,
	   struct bfd_link_info *));
extern void bfd_ecoff_debug_free
  PARAMS ((PTR handle, bfd *output_bfd, struct ecoff_debug_info *output_debug,
	   const struct ecoff_debug_swap *output_swap,
	   struct bfd_link_info *));
extern boolean bfd_ecoff_debug_accumulate
  PARAMS ((PTR handle, bfd *output_bfd, struct ecoff_debug_info *output_debug,
	   const struct ecoff_debug_swap *output_swap,
	   bfd *input_bfd, struct ecoff_debug_info *input_debug,
	   const struct ecoff_debug_swap *input_swap,
	   struct bfd_link_info *));
extern boolean bfd_ecoff_debug_accumulate_other
  PARAMS ((PTR handle, bfd *output_bfd, struct ecoff_debug_info *output_debug,
	   const struct ecoff_debug_swap *output_swap, bfd *input_bfd,
	   struct bfd_link_info *));
extern boolean bfd_ecoff_debug_externals
  PARAMS ((bfd *abfd, struct ecoff_debug_info *debug,
	   const struct ecoff_debug_swap *swap,
	   boolean relocateable,
	   boolean (*get_extr) (struct symbol_cache_entry *,
				struct ecoff_extr *),
	   void (*set_index) (struct symbol_cache_entry *,
			      bfd_size_type)));
extern boolean bfd_ecoff_debug_one_external
  PARAMS ((bfd *abfd, struct ecoff_debug_info *debug,
	   const struct ecoff_debug_swap *swap,
	   const char *name, struct ecoff_extr *esym));
extern bfd_size_type bfd_ecoff_debug_size
  PARAMS ((bfd *abfd, struct ecoff_debug_info *debug,
	   const struct ecoff_debug_swap *swap));
extern boolean bfd_ecoff_write_debug
  PARAMS ((bfd *abfd, struct ecoff_debug_info *debug,
	   const struct ecoff_debug_swap *swap, file_ptr where));
extern boolean bfd_ecoff_write_accumulated_debug
  PARAMS ((PTR handle, bfd *abfd, struct ecoff_debug_info *debug,
	   const struct ecoff_debug_swap *swap,
	   struct bfd_link_info *info, file_ptr where));
extern boolean bfd_mips_ecoff_create_embedded_relocs
  PARAMS ((bfd *, struct bfd_link_info *, struct sec *, struct sec *,
	   char **));

/* Externally visible ELF routines.  */

struct bfd_link_needed_list
{
  struct bfd_link_needed_list *next;
  bfd *by;
  const char *name;
};

extern boolean bfd_elf32_record_link_assignment
  PARAMS ((bfd *, struct bfd_link_info *, const char *, boolean));
extern boolean bfd_elf64_record_link_assignment
  PARAMS ((bfd *, struct bfd_link_info *, const char *, boolean));
extern struct bfd_link_needed_list *bfd_elf_get_needed_list
  PARAMS ((bfd *, struct bfd_link_info *));
extern boolean bfd_elf_get_bfd_needed_list
  PARAMS ((bfd *, struct bfd_link_needed_list **));
extern boolean bfd_elf32_size_dynamic_sections
  PARAMS ((bfd *, const char *, const char *, boolean, const char *,
	   const char * const *, struct bfd_link_info *, struct sec **,
	   struct bfd_elf_version_tree *));
extern boolean bfd_elf64_size_dynamic_sections
  PARAMS ((bfd *, const char *, const char *, boolean, const char *,
	   const char * const *, struct bfd_link_info *, struct sec **,
	   struct bfd_elf_version_tree *));
extern void bfd_elf_set_dt_needed_name PARAMS ((bfd *, const char *));
extern const char *bfd_elf_get_dt_soname PARAMS ((bfd *));

/* Return an upper bound on the number of bytes required to store a
   copy of ABFD's program header table entries.  Return -1 if an error
   occurs; bfd_get_error will return an appropriate code.  */
extern long bfd_get_elf_phdr_upper_bound PARAMS ((bfd *abfd));

/* Copy ABFD's program header table entries to *PHDRS.  The entries
   will be stored as an array of Elf_Internal_Phdr structures, as
   defined in include/elf/internal.h.  To find out how large the
   buffer needs to be, call bfd_get_elf_phdr_upper_bound.

   Return the number of program header table entries read, or -1 if an
   error occurs; bfd_get_error will return an appropriate code.  */
extern int bfd_get_elf_phdrs PARAMS ((bfd *abfd, void *phdrs));

/* SunOS shared library support routines for the linker.  */

extern struct bfd_link_needed_list *bfd_sunos_get_needed_list
  PARAMS ((bfd *, struct bfd_link_info *));
extern boolean bfd_sunos_record_link_assignment
  PARAMS ((bfd *, struct bfd_link_info *, const char *));
extern boolean bfd_sunos_size_dynamic_sections
  PARAMS ((bfd *, struct bfd_link_info *, struct sec **, struct sec **,
	   struct sec **));

/* Linux shared library support routines for the linker.  */

extern boolean bfd_i386linux_size_dynamic_sections
  PARAMS ((bfd *, struct bfd_link_info *));
extern boolean bfd_m68klinux_size_dynamic_sections
  PARAMS ((bfd *, struct bfd_link_info *));
extern boolean bfd_sparclinux_size_dynamic_sections
  PARAMS ((bfd *, struct bfd_link_info *));

/* mmap hacks */

struct _bfd_window_internal;
typedef struct _bfd_window_internal bfd_window_internal;

typedef struct _bfd_window {
  /* What the user asked for.  */
  PTR data;
  bfd_size_type size;
  /* The actual window used by BFD.  Small user-requested read-only
     regions sharing a page may share a single window into the object
     file.  Read-write versions shouldn't until I've fixed things to
     keep track of which portions have been claimed by the
     application; don't want to give the same region back when the
     application wants two writable copies!  */
  struct _bfd_window_internal *i;
} bfd_window;

extern void bfd_init_window PARAMS ((bfd_window *));
extern void bfd_free_window PARAMS ((bfd_window *));
extern boolean bfd_get_file_window
  PARAMS ((bfd *, file_ptr, bfd_size_type, bfd_window *, boolean));

/* XCOFF support routines for the linker.  */

extern boolean bfd_xcoff_link_record_set
  PARAMS ((bfd *, struct bfd_link_info *, struct bfd_link_hash_entry *,
	   bfd_size_type));
extern boolean bfd_xcoff_import_symbol
  PARAMS ((bfd *, struct bfd_link_info *, struct bfd_link_hash_entry *,
	   bfd_vma, const char *, const char *, const char *));
extern boolean bfd_xcoff_export_symbol
  PARAMS ((bfd *, struct bfd_link_info *, struct bfd_link_hash_entry *,
	   boolean));
extern boolean bfd_xcoff_link_count_reloc
  PARAMS ((bfd *, struct bfd_link_info *, const char *));
extern boolean bfd_xcoff_record_link_assignment
  PARAMS ((bfd *, struct bfd_link_info *, const char *));
extern boolean bfd_xcoff_size_dynamic_sections
  PARAMS ((bfd *, struct bfd_link_info *, const char *, const char *,
	   unsigned long, unsigned long, unsigned long, boolean,
	   int, boolean, boolean, struct sec **));

/* Externally visible COFF routines.  */

#if defined(__STDC__) || defined(ALMOST_STDC)
struct internal_syment;
union internal_auxent;
#endif

extern boolean bfd_coff_get_syment
  PARAMS ((bfd *, struct symbol_cache_entry *, struct internal_syment *));

extern boolean bfd_coff_get_auxent
  PARAMS ((bfd *, struct symbol_cache_entry *, int, union internal_auxent *));

extern boolean bfd_coff_set_symbol_class
  PARAMS ((bfd *, struct symbol_cache_entry *, unsigned int));

/* ARM Interworking support.  Called from linker.  */
extern boolean bfd_arm_allocate_interworking_sections
  PARAMS ((struct bfd_link_info *));

extern boolean bfd_arm_process_before_allocation
  PARAMS ((bfd *, struct bfd_link_info *, int));

extern boolean bfd_arm_get_bfd_for_interworking
  PARAMS ((bfd *, struct bfd_link_info *));

/* PE ARM Interworking support.  Called from linker.  */
extern boolean bfd_arm_pe_allocate_interworking_sections
  PARAMS ((struct bfd_link_info *));

extern boolean bfd_arm_pe_process_before_allocation
  PARAMS ((bfd *, struct bfd_link_info *, int));

extern boolean bfd_arm_pe_get_bfd_for_interworking
  PARAMS ((bfd *, struct bfd_link_info *));

/* ELF ARM Interworking support.  Called from linker.  */
extern boolean bfd_elf32_arm_allocate_interworking_sections
  PARAMS ((struct bfd_link_info *));

extern boolean bfd_elf32_arm_process_before_allocation
  PARAMS ((bfd *, struct bfd_link_info *, int));

extern boolean bfd_elf32_arm_get_bfd_for_interworking
  PARAMS ((bfd *, struct bfd_link_info *));

/* And more from the source.  */
