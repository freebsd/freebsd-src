/* $FreeBSD: src/gnu/usr.bin/binutils/libbfd/rs6000/bfd.h,v 1.1.2.1 2000/07/06 22:16:06 obrien Exp $ */
/* Main header file for the bfd library -- portable access to object files.
   Copyright 1990, 91, 92, 93, 94, 95, 96, 97, 1998
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
#define BFD_VERSION  "2.9.1"
#define BFD_ARCH_SIZE 32
#define BFD_HOST_64BIT_LONG 0
#if 0
#define BFD_HOST_64_BIT 
#define BFD_HOST_U_64_BIT 
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
#if defined (__GNUG__) && (__GNUC_MINOR__ > 5)
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

#ifdef BFD64

#ifndef BFD_HOST_64_BIT
#if BFD_HOST_64BIT_LONG
#define BFD_HOST_64_BIT long
#define BFD_HOST_U_64_BIT unsigned long
#else
#ifdef __GNUC__
#define BFD_HOST_64_BIT long long
#define BFD_HOST_U_64_BIT unsigned long long
#else /* ! defined (__GNUC__) */
 #error No 64 bit integer type available
#endif /* ! defined (__GNUC__) */
#endif /* ! BFD_HOST_64BIT_LONG */
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

/* And more from the source.  */
void 
bfd_init PARAMS ((void));

bfd *
bfd_openr PARAMS ((CONST char *filename, CONST char *target));

bfd *
bfd_fdopenr PARAMS ((CONST char *filename, CONST char *target, int fd));

bfd *
bfd_openstreamr PARAMS ((const char *, const char *, PTR));

bfd *
bfd_openw PARAMS ((CONST char *filename, CONST char *target));

boolean 
bfd_close PARAMS ((bfd *abfd));

boolean 
bfd_close_all_done PARAMS ((bfd *));

bfd *
bfd_create PARAMS ((CONST char *filename, bfd *templ));


 /* Byte swapping macros for user section data.  */

#define bfd_put_8(abfd, val, ptr) \
                (*((unsigned char *)(ptr)) = (unsigned char)(val))
#define bfd_put_signed_8 \
               bfd_put_8
#define bfd_get_8(abfd, ptr) \
                (*(unsigned char *)(ptr))
#define bfd_get_signed_8(abfd, ptr) \
               ((*(unsigned char *)(ptr) ^ 0x80) - 0x80)

#define bfd_put_16(abfd, val, ptr) \
                BFD_SEND(abfd, bfd_putx16, ((val),(ptr)))
#define bfd_put_signed_16 \
                bfd_put_16
#define bfd_get_16(abfd, ptr) \
                BFD_SEND(abfd, bfd_getx16, (ptr))
#define bfd_get_signed_16(abfd, ptr) \
                BFD_SEND (abfd, bfd_getx_signed_16, (ptr))

#define bfd_put_32(abfd, val, ptr) \
                BFD_SEND(abfd, bfd_putx32, ((val),(ptr)))
#define bfd_put_signed_32 \
                bfd_put_32
#define bfd_get_32(abfd, ptr) \
                BFD_SEND(abfd, bfd_getx32, (ptr))
#define bfd_get_signed_32(abfd, ptr) \
                BFD_SEND(abfd, bfd_getx_signed_32, (ptr))

#define bfd_put_64(abfd, val, ptr) \
                BFD_SEND(abfd, bfd_putx64, ((val), (ptr)))
#define bfd_put_signed_64 \
                bfd_put_64
#define bfd_get_64(abfd, ptr) \
                BFD_SEND(abfd, bfd_getx64, (ptr))
#define bfd_get_signed_64(abfd, ptr) \
                BFD_SEND(abfd, bfd_getx_signed_64, (ptr))


 /* Byte swapping macros for file header data.  */

#define bfd_h_put_8(abfd, val, ptr) \
               bfd_put_8 (abfd, val, ptr)
#define bfd_h_put_signed_8(abfd, val, ptr) \
               bfd_put_8 (abfd, val, ptr)
#define bfd_h_get_8(abfd, ptr) \
               bfd_get_8 (abfd, ptr)
#define bfd_h_get_signed_8(abfd, ptr) \
               bfd_get_signed_8 (abfd, ptr)

#define bfd_h_put_16(abfd, val, ptr) \
                BFD_SEND(abfd, bfd_h_putx16,(val,ptr))
#define bfd_h_put_signed_16 \
                bfd_h_put_16
#define bfd_h_get_16(abfd, ptr) \
                BFD_SEND(abfd, bfd_h_getx16,(ptr))
#define bfd_h_get_signed_16(abfd, ptr) \
                BFD_SEND(abfd, bfd_h_getx_signed_16, (ptr))

#define bfd_h_put_32(abfd, val, ptr) \
                BFD_SEND(abfd, bfd_h_putx32,(val,ptr))
#define bfd_h_put_signed_32 \
                bfd_h_put_32
#define bfd_h_get_32(abfd, ptr) \
                BFD_SEND(abfd, bfd_h_getx32,(ptr))
#define bfd_h_get_signed_32(abfd, ptr) \
                BFD_SEND(abfd, bfd_h_getx_signed_32, (ptr))

#define bfd_h_put_64(abfd, val, ptr) \
                BFD_SEND(abfd, bfd_h_putx64,(val, ptr))
#define bfd_h_put_signed_64 \
                bfd_h_put_64
#define bfd_h_get_64(abfd, ptr) \
                BFD_SEND(abfd, bfd_h_getx64,(ptr))
#define bfd_h_get_signed_64(abfd, ptr) \
                BFD_SEND(abfd, bfd_h_getx_signed_64, (ptr))

typedef struct sec
{
         /* The name of the section; the name isn't a copy, the pointer is
        the same as that passed to bfd_make_section. */

    CONST char *name;

         /* Which section is it; 0..nth.      */

   int index;

         /* The next section in the list belonging to the BFD, or NULL. */

    struct sec *next;

         /* The field flags contains attributes of the section. Some
           flags are read in from the object file, and some are
           synthesized from other information.  */

    flagword flags;

#define SEC_NO_FLAGS   0x000

         /* Tells the OS to allocate space for this section when loading.
           This is clear for a section containing debug information
           only. */
#define SEC_ALLOC      0x001

         /* Tells the OS to load the section from the file when loading.
           This is clear for a .bss section. */
#define SEC_LOAD       0x002

         /* The section contains data still to be relocated, so there is
           some relocation information too. */
#define SEC_RELOC      0x004

#if 0    /* Obsolete ? */
#define SEC_BALIGN     0x008
#endif

         /* A signal to the OS that the section contains read only
          data. */
#define SEC_READONLY   0x010

         /* The section contains code only. */
#define SEC_CODE       0x020

         /* The section contains data only. */
#define SEC_DATA       0x040

         /* The section will reside in ROM. */
#define SEC_ROM        0x080

         /* The section contains constructor information. This section
           type is used by the linker to create lists of constructors and
           destructors used by <<g++>>. When a back end sees a symbol
           which should be used in a constructor list, it creates a new
           section for the type of name (e.g., <<__CTOR_LIST__>>), attaches
           the symbol to it, and builds a relocation. To build the lists
           of constructors, all the linker has to do is catenate all the
           sections called <<__CTOR_LIST__>> and relocate the data
           contained within - exactly the operations it would peform on
           standard data. */
#define SEC_CONSTRUCTOR 0x100

         /* The section is a constuctor, and should be placed at the
          end of the text, data, or bss section(?). */
#define SEC_CONSTRUCTOR_TEXT 0x1100
#define SEC_CONSTRUCTOR_DATA 0x2100
#define SEC_CONSTRUCTOR_BSS  0x3100

         /* The section has contents - a data section could be
           <<SEC_ALLOC>> | <<SEC_HAS_CONTENTS>>; a debug section could be
           <<SEC_HAS_CONTENTS>> */
#define SEC_HAS_CONTENTS 0x200

         /* An instruction to the linker to not output the section
           even if it has information which would normally be written. */
#define SEC_NEVER_LOAD 0x400

         /* The section is a COFF shared library section.  This flag is
           only for the linker.  If this type of section appears in
           the input file, the linker must copy it to the output file
           without changing the vma or size.  FIXME: Although this
           was originally intended to be general, it really is COFF
           specific (and the flag was renamed to indicate this).  It
           might be cleaner to have some more general mechanism to
           allow the back end to control what the linker does with
           sections. */
#define SEC_COFF_SHARED_LIBRARY 0x800

         /* The section contains common symbols (symbols may be defined
           multiple times, the value of a symbol is the amount of
           space it requires, and the largest symbol value is the one
           used).  Most targets have exactly one of these (which we
           translate to bfd_com_section_ptr), but ECOFF has two. */
#define SEC_IS_COMMON 0x8000

         /* The section contains only debugging information.  For
           example, this is set for ELF .debug and .stab sections.
           strip tests this flag to see if a section can be
           discarded. */
#define SEC_DEBUGGING 0x10000

         /* The contents of this section are held in memory pointed to
           by the contents field.  This is checked by
           bfd_get_section_contents, and the data is retrieved from
           memory if appropriate.  */
#define SEC_IN_MEMORY 0x20000

         /* The contents of this section are to be excluded by the
           linker for executable and shared objects unless those
           objects are to be further relocated.  */
#define SEC_EXCLUDE 0x40000

        /* The contents of this section are to be sorted by the
          based on the address specified in the associated symbol
          table.  */
#define SEC_SORT_ENTRIES 0x80000

        /* When linking, duplicate sections of the same name should be
          discarded, rather than being combined into a single section as
          is usually done.  This is similar to how common symbols are
          handled.  See SEC_LINK_DUPLICATES below.  */
#define SEC_LINK_ONCE 0x100000

        /* If SEC_LINK_ONCE is set, this bitfield describes how the linker
          should handle duplicate sections.  */
#define SEC_LINK_DUPLICATES 0x600000

        /* This value for SEC_LINK_DUPLICATES means that duplicate
          sections with the same name should simply be discarded. */
#define SEC_LINK_DUPLICATES_DISCARD 0x0

        /* This value for SEC_LINK_DUPLICATES means that the linker
          should warn if there are any duplicate sections, although
          it should still only link one copy.  */
#define SEC_LINK_DUPLICATES_ONE_ONLY 0x200000

        /* This value for SEC_LINK_DUPLICATES means that the linker
          should warn if any duplicate sections are a different size.  */
#define SEC_LINK_DUPLICATES_SAME_SIZE 0x400000

        /* This value for SEC_LINK_DUPLICATES means that the linker
          should warn if any duplicate sections contain different
          contents.  */
#define SEC_LINK_DUPLICATES_SAME_CONTENTS 0x600000

        /* This section was created by the linker as part of dynamic
          relocation or other arcane processing.  It is skipped when
          going through the first-pass output, trusting that someone
          else up the line will take care of it later.  */
#define SEC_LINKER_CREATED 0x800000

        /*  End of section flags.  */

        /* Some internal packed boolean fields.  */

        /* See the vma field.  */
       unsigned int user_set_vma : 1;

        /* Whether relocations have been processed.  */
       unsigned int reloc_done : 1;

        /* A mark flag used by some of the linker backends.  */
       unsigned int linker_mark : 1;

        /* End of internal packed boolean fields.  */

        /*  The virtual memory address of the section - where it will be
           at run time.  The symbols are relocated against this.  The
           user_set_vma flag is maintained by bfd; if it's not set, the
           backend can assign addresses (for example, in <<a.out>>, where
           the default address for <<.data>> is dependent on the specific
           target and various flags).  */

   bfd_vma vma;

        /*  The load address of the section - where it would be in a
           rom image; really only used for writing section header
           information. */

   bfd_vma lma;

         /* The size of the section in bytes, as it will be output.
           contains a value even if the section has no contents (e.g., the
           size of <<.bss>>). This will be filled in after relocation */

   bfd_size_type _cooked_size;

         /* The original size on disk of the section, in bytes.  Normally this
           value is the same as the size, but if some relaxing has
           been done, then this value will be bigger.  */

   bfd_size_type _raw_size;

         /* If this section is going to be output, then this value is the
           offset into the output section of the first byte in the input
           section. E.g., if this was going to start at the 100th byte in
           the output section, this value would be 100. */

   bfd_vma output_offset;

         /* The output section through which to map on output. */

   struct sec *output_section;

         /* The alignment requirement of the section, as an exponent of 2 -
           e.g., 3 aligns to 2^3 (or 8). */

   unsigned int alignment_power;

         /* If an input section, a pointer to a vector of relocation
           records for the data in this section. */

   struct reloc_cache_entry *relocation;

         /* If an output section, a pointer to a vector of pointers to
           relocation records for the data in this section. */

   struct reloc_cache_entry **orelocation;

         /* The number of relocation records in one of the above  */

   unsigned reloc_count;

         /* Information below is back end specific - and not always used
           or updated.  */

         /* File position of section data    */

   file_ptr filepos;

         /* File position of relocation info */

   file_ptr rel_filepos;

         /* File position of line data       */

   file_ptr line_filepos;

         /* Pointer to data for applications */

   PTR userdata;

         /* If the SEC_IN_MEMORY flag is set, this points to the actual
           contents.  */
   unsigned char *contents;

         /* Attached line number information */

   alent *lineno;

         /* Number of line number records   */

   unsigned int lineno_count;

         /* When a section is being output, this value changes as more
           linenumbers are written out */

   file_ptr moving_line_filepos;

         /* What the section number is in the target world  */

   int target_index;

   PTR used_by_bfd;

         /* If this is a constructor section then here is a list of the
           relocations created to relocate items within it. */

   struct relent_chain *constructor_chain;

         /* The BFD which owns the section. */

   bfd *owner;

         /* A symbol which points at this section only */
   struct symbol_cache_entry *symbol;
   struct symbol_cache_entry **symbol_ptr_ptr;

   struct bfd_link_order *link_order_head;
   struct bfd_link_order *link_order_tail;
} asection ;

     /* These sections are global, and are managed by BFD.  The application
       and target back end are not permitted to change the values in
       these sections.  New code should use the section_ptr macros rather
       than referring directly to the const sections.  The const sections
       may eventually vanish.  */
#define BFD_ABS_SECTION_NAME "*ABS*"
#define BFD_UND_SECTION_NAME "*UND*"
#define BFD_COM_SECTION_NAME "*COM*"
#define BFD_IND_SECTION_NAME "*IND*"

     /* the absolute section */
extern const asection bfd_abs_section;
#define bfd_abs_section_ptr ((asection *) &bfd_abs_section)
#define bfd_is_abs_section(sec) ((sec) == bfd_abs_section_ptr)
     /* Pointer to the undefined section */
extern const asection bfd_und_section;
#define bfd_und_section_ptr ((asection *) &bfd_und_section)
#define bfd_is_und_section(sec) ((sec) == bfd_und_section_ptr)
     /* Pointer to the common section */
extern const asection bfd_com_section;
#define bfd_com_section_ptr ((asection *) &bfd_com_section)
     /* Pointer to the indirect section */
extern const asection bfd_ind_section;
#define bfd_ind_section_ptr ((asection *) &bfd_ind_section)
#define bfd_is_ind_section(sec) ((sec) == bfd_ind_section_ptr)

extern const struct symbol_cache_entry * const bfd_abs_symbol;
extern const struct symbol_cache_entry * const bfd_com_symbol;
extern const struct symbol_cache_entry * const bfd_und_symbol;
extern const struct symbol_cache_entry * const bfd_ind_symbol;
#define bfd_get_section_size_before_reloc(section) \
     (section->reloc_done ? (abort(),1): (section)->_raw_size)
#define bfd_get_section_size_after_reloc(section) \
     ((section->reloc_done) ? (section)->_cooked_size: (abort(),1))
asection *
bfd_get_section_by_name PARAMS ((bfd *abfd, CONST char *name));

asection *
bfd_make_section_old_way PARAMS ((bfd *abfd, CONST char *name));

asection *
bfd_make_section_anyway PARAMS ((bfd *abfd, CONST char *name));

asection *
bfd_make_section PARAMS ((bfd *, CONST char *name));

boolean 
bfd_set_section_flags PARAMS ((bfd *abfd, asection *sec, flagword flags));

void 
bfd_map_over_sections PARAMS ((bfd *abfd,
    void (*func)(bfd *abfd,
    asection *sect,
    PTR obj),
    PTR obj));

boolean 
bfd_set_section_size PARAMS ((bfd *abfd, asection *sec, bfd_size_type val));

boolean 
bfd_set_section_contents
 PARAMS ((bfd *abfd,
    asection *section,
    PTR data,
    file_ptr offset,
    bfd_size_type count));

boolean 
bfd_get_section_contents
 PARAMS ((bfd *abfd, asection *section, PTR location,
    file_ptr offset, bfd_size_type count));

boolean 
bfd_copy_private_section_data PARAMS ((bfd *ibfd, asection *isec, bfd *obfd, asection *osec));

#define bfd_copy_private_section_data(ibfd, isection, obfd, osection) \
     BFD_SEND (obfd, _bfd_copy_private_section_data, \
               (ibfd, isection, obfd, osection))
enum bfd_architecture 
{
  bfd_arch_unknown,    /* File arch not known */
  bfd_arch_obscure,    /* Arch known, not one of these */
  bfd_arch_m68k,       /* Motorola 68xxx */
#define bfd_mach_m68000 1
#define bfd_mach_m68008 2
#define bfd_mach_m68010 3
#define bfd_mach_m68020 4
#define bfd_mach_m68030 5
#define bfd_mach_m68040 6
#define bfd_mach_m68060 7
  bfd_arch_vax,        /* DEC Vax */   
  bfd_arch_i960,       /* Intel 960 */
     /* The order of the following is important.
       lower number indicates a machine type that 
       only accepts a subset of the instructions
       available to machines with higher numbers.
       The exception is the "ca", which is
       incompatible with all other machines except 
       "core". */

#define bfd_mach_i960_core      1
#define bfd_mach_i960_ka_sa     2
#define bfd_mach_i960_kb_sb     3
#define bfd_mach_i960_mc        4
#define bfd_mach_i960_xa        5
#define bfd_mach_i960_ca        6
#define bfd_mach_i960_jx        7
#define bfd_mach_i960_hx        8

  bfd_arch_a29k,       /* AMD 29000 */
  bfd_arch_sparc,      /* SPARC */
#define bfd_mach_sparc                 1
 /* The difference between v8plus and v9 is that v9 is a true 64 bit env.  */
#define bfd_mach_sparc_sparclet        2
#define bfd_mach_sparc_sparclite       3
#define bfd_mach_sparc_v8plus          4
#define bfd_mach_sparc_v8plusa         5  /* with ultrasparc add'ns */
#define bfd_mach_sparc_v9              6
#define bfd_mach_sparc_v9a             7  /* with ultrasparc add'ns */
 /* Nonzero if MACH has the v9 instruction set.  */
#define bfd_mach_sparc_v9_p(mach) \
  ((mach) >= bfd_mach_sparc_v8plus && (mach) <= bfd_mach_sparc_v9a)
  bfd_arch_mips,       /* MIPS Rxxxx */
#define bfd_mach_mips3000              3000
#define bfd_mach_mips3900              3900
#define bfd_mach_mips4000              4000
#define bfd_mach_mips4010              4010
#define bfd_mach_mips4100              4100
#define bfd_mach_mips4300              4300
#define bfd_mach_mips4400              4400
#define bfd_mach_mips4600              4600
#define bfd_mach_mips4650              4650
#define bfd_mach_mips5000              5000
#define bfd_mach_mips6000              6000
#define bfd_mach_mips8000              8000
#define bfd_mach_mips10000             10000
#define bfd_mach_mips16                16
  bfd_arch_i386,       /* Intel 386 */
#define bfd_mach_i386_i386 0
#define bfd_mach_i386_i8086 1
  bfd_arch_we32k,      /* AT&T WE32xxx */
  bfd_arch_tahoe,      /* CCI/Harris Tahoe */
  bfd_arch_i860,       /* Intel 860 */
  bfd_arch_romp,       /* IBM ROMP PC/RT */
  bfd_arch_alliant,    /* Alliant */
  bfd_arch_convex,     /* Convex */
  bfd_arch_m88k,       /* Motorola 88xxx */
  bfd_arch_pyramid,    /* Pyramid Technology */
  bfd_arch_h8300,      /* Hitachi H8/300 */
#define bfd_mach_h8300   1
#define bfd_mach_h8300h  2
#define bfd_mach_h8300s  3
  bfd_arch_powerpc,    /* PowerPC */
  bfd_arch_rs6000,     /* IBM RS/6000 */
  bfd_arch_hppa,       /* HP PA RISC */
  bfd_arch_d10v,       /* Mitsubishi D10V */
  bfd_arch_z8k,        /* Zilog Z8000 */
#define bfd_mach_z8001         1
#define bfd_mach_z8002         2
  bfd_arch_h8500,      /* Hitachi H8/500 */
  bfd_arch_sh,         /* Hitachi SH */
#define bfd_mach_sh            0
#define bfd_mach_sh3        0x30
#define bfd_mach_sh3e       0x3e
#define bfd_mach_sh4        0x40
  bfd_arch_alpha,      /* Dec Alpha */
  bfd_arch_arm,        /* Advanced Risc Machines ARM */
#define bfd_mach_arm_2         1
#define bfd_mach_arm_2a                2
#define bfd_mach_arm_3         3
#define bfd_mach_arm_3M        4
#define bfd_mach_arm_4                 5
#define bfd_mach_arm_4T        6
  bfd_arch_ns32k,      /* National Semiconductors ns32000 */
  bfd_arch_w65,        /* WDC 65816 */
  bfd_arch_tic30,      /* Texas Instruments TMS320C30 */
  bfd_arch_v850,       /* NEC V850 */
#define bfd_mach_v850          0
  bfd_arch_arc,        /* Argonaut RISC Core */
#define bfd_mach_arc_base 0
  bfd_arch_m32r,       /* Mitsubishi M32R/D */
#define bfd_mach_m32r          0  /* backwards compatibility */
  bfd_arch_mn10200,    /* Matsushita MN10200 */
  bfd_arch_mn10300,    /* Matsushita MN10300 */
  bfd_arch_last
  };

typedef struct bfd_arch_info 
{
  int bits_per_word;
  int bits_per_address;
  int bits_per_byte;
  enum bfd_architecture arch;
  unsigned long mach;
  const char *arch_name;
  const char *printable_name;
  unsigned int section_align_power;
  /* true if this is the default machine for the architecture */
  boolean the_default; 
  const struct bfd_arch_info * (*compatible)
       PARAMS ((const struct bfd_arch_info *a,
                const struct bfd_arch_info *b));

  boolean (*scan) PARAMS ((const struct bfd_arch_info *, const char *));

  const struct bfd_arch_info *next;
} bfd_arch_info_type;
const char *
bfd_printable_name PARAMS ((bfd *abfd));

const bfd_arch_info_type *
bfd_scan_arch PARAMS ((const char *string));

const char **
bfd_arch_list PARAMS ((void));

const bfd_arch_info_type *
bfd_arch_get_compatible PARAMS ((
    const bfd *abfd,
    const bfd *bbfd));

void 
bfd_set_arch_info PARAMS ((bfd *abfd, const bfd_arch_info_type *arg));

enum bfd_architecture 
bfd_get_arch PARAMS ((bfd *abfd));

unsigned long 
bfd_get_mach PARAMS ((bfd *abfd));

unsigned int 
bfd_arch_bits_per_byte PARAMS ((bfd *abfd));

unsigned int 
bfd_arch_bits_per_address PARAMS ((bfd *abfd));

const bfd_arch_info_type * 
bfd_get_arch_info PARAMS ((bfd *abfd));

const bfd_arch_info_type *
bfd_lookup_arch
 PARAMS ((enum bfd_architecture
    arch,
    unsigned long machine));

const char *
bfd_printable_arch_mach
 PARAMS ((enum bfd_architecture arch, unsigned long machine));

typedef enum bfd_reloc_status
{
        /* No errors detected */
  bfd_reloc_ok,

        /* The relocation was performed, but there was an overflow. */
  bfd_reloc_overflow,

        /* The address to relocate was not within the section supplied. */
  bfd_reloc_outofrange,

        /* Used by special functions */
  bfd_reloc_continue,

        /* Unsupported relocation size requested. */
  bfd_reloc_notsupported,

        /* Unused */
  bfd_reloc_other,

        /* The symbol to relocate against was undefined. */
  bfd_reloc_undefined,

        /* The relocation was performed, but may not be ok - presently
          generated only when linking i960 coff files with i960 b.out
          symbols.  If this type is returned, the error_message argument
          to bfd_perform_relocation will be set.  */
  bfd_reloc_dangerous
 }
 bfd_reloc_status_type;


typedef struct reloc_cache_entry
{
        /* A pointer into the canonical table of pointers  */
  struct symbol_cache_entry **sym_ptr_ptr;

        /* offset in section */
  bfd_size_type address;

        /* addend for relocation value */
  bfd_vma addend;

        /* Pointer to how to perform the required relocation */
  reloc_howto_type *howto;

} arelent;
enum complain_overflow
{
        /* Do not complain on overflow. */
  complain_overflow_dont,

        /* Complain if the bitfield overflows, whether it is considered
          as signed or unsigned. */
  complain_overflow_bitfield,

        /* Complain if the value overflows when considered as signed
          number. */
  complain_overflow_signed,

        /* Complain if the value overflows when considered as an
          unsigned number. */
  complain_overflow_unsigned
};

struct reloc_howto_struct
{
        /*  The type field has mainly a documentary use - the back end can
           do what it wants with it, though normally the back end's
           external idea of what a reloc number is stored
           in this field. For example, a PC relative word relocation
           in a coff environment has the type 023 - because that's
           what the outside world calls a R_PCRWORD reloc. */
  unsigned int type;

        /*  The value the final relocation is shifted right by. This drops
           unwanted data from the relocation.  */
  unsigned int rightshift;

        /*  The size of the item to be relocated.  This is *not* a
           power-of-two measure.  To get the number of bytes operated
           on by a type of relocation, use bfd_get_reloc_size.  */
  int size;

        /*  The number of bits in the item to be relocated.  This is used
           when doing overflow checking.  */
  unsigned int bitsize;

        /*  Notes that the relocation is relative to the location in the
           data section of the addend. The relocation function will
           subtract from the relocation value the address of the location
           being relocated. */
  boolean pc_relative;

        /*  The bit position of the reloc value in the destination.
           The relocated value is left shifted by this amount. */
  unsigned int bitpos;

        /* What type of overflow error should be checked for when
          relocating. */
  enum complain_overflow complain_on_overflow;

        /* If this field is non null, then the supplied function is
          called rather than the normal function. This allows really
          strange relocation methods to be accomodated (e.g., i960 callj
          instructions). */
  bfd_reloc_status_type (*special_function)
                                   PARAMS ((bfd *abfd,
                                            arelent *reloc_entry,
                                            struct symbol_cache_entry *symbol,
                                            PTR data,
                                            asection *input_section,
                                            bfd *output_bfd,
                                            char **error_message));

        /* The textual name of the relocation type. */
  char *name;

        /* When performing a partial link, some formats must modify the
          relocations rather than the data - this flag signals this.*/
  boolean partial_inplace;

        /* The src_mask selects which parts of the read in data
          are to be used in the relocation sum.  E.g., if this was an 8 bit
          bit of data which we read and relocated, this would be
          0x000000ff. When we have relocs which have an addend, such as
          sun4 extended relocs, the value in the offset part of a
          relocating field is garbage so we never use it. In this case
          the mask would be 0x00000000. */
  bfd_vma src_mask;

        /* The dst_mask selects which parts of the instruction are replaced
          into the instruction. In most cases src_mask == dst_mask,
          except in the above special case, where dst_mask would be
          0x000000ff, and src_mask would be 0x00000000.   */
  bfd_vma dst_mask;

        /* When some formats create PC relative instructions, they leave
          the value of the pc of the place being relocated in the offset
          slot of the instruction, so that a PC relative relocation can
          be made just by adding in an ordinary offset (e.g., sun3 a.out).
          Some formats leave the displacement part of an instruction
          empty (e.g., m88k bcs); this flag signals the fact.*/
  boolean pcrel_offset;

};
#define HOWTO(C, R,S,B, P, BI, O, SF, NAME, INPLACE, MASKSRC, MASKDST, PC) \
  {(unsigned)C,R,S,B, P, BI, O,SF,NAME,INPLACE,MASKSRC,MASKDST,PC}
#define NEWHOWTO( FUNCTION, NAME,SIZE,REL,IN) HOWTO(0,0,SIZE,0,REL,0,complain_overflow_dont,FUNCTION, NAME,false,0,0,IN)

#define HOWTO_PREPARE(relocation, symbol)      \
  {                                            \
  if (symbol != (asymbol *)NULL) {             \
    if (bfd_is_com_section (symbol->section)) { \
      relocation = 0;                          \
    }                                          \
    else {                                     \
      relocation = symbol->value;              \
    }                                          \
  }                                            \
}
unsigned int 
bfd_get_reloc_size  PARAMS ((reloc_howto_type *));

typedef struct relent_chain {
  arelent relent;
  struct   relent_chain *next;
} arelent_chain;
bfd_reloc_status_type

bfd_check_overflow
 PARAMS ((enum complain_overflow how,
    unsigned int bitsize,
    unsigned int rightshift,
    bfd_vma relocation));

bfd_reloc_status_type

bfd_perform_relocation
 PARAMS ((bfd *abfd,
    arelent *reloc_entry,
    PTR data,
    asection *input_section,
    bfd *output_bfd,
    char **error_message));

bfd_reloc_status_type

bfd_install_relocation
 PARAMS ((bfd *abfd,
    arelent *reloc_entry,
    PTR data, bfd_vma data_start,
    asection *input_section,
    char **error_message));

enum bfd_reloc_code_real {
  _dummy_first_bfd_reloc_code_real,


/* Basic absolute relocations of N bits. */
  BFD_RELOC_64,
  BFD_RELOC_32,
  BFD_RELOC_26,
  BFD_RELOC_24,
  BFD_RELOC_16,
  BFD_RELOC_14,
  BFD_RELOC_8,

/* PC-relative relocations.  Sometimes these are relative to the address
of the relocation itself; sometimes they are relative to the start of
the section containing the relocation.  It depends on the specific target.

The 24-bit relocation is used in some Intel 960 configurations. */
  BFD_RELOC_64_PCREL,
  BFD_RELOC_32_PCREL,
  BFD_RELOC_24_PCREL,
  BFD_RELOC_16_PCREL,
  BFD_RELOC_12_PCREL,
  BFD_RELOC_8_PCREL,

/* For ELF. */
  BFD_RELOC_32_GOT_PCREL,
  BFD_RELOC_16_GOT_PCREL,
  BFD_RELOC_8_GOT_PCREL,
  BFD_RELOC_32_GOTOFF,
  BFD_RELOC_16_GOTOFF,
  BFD_RELOC_LO16_GOTOFF,
  BFD_RELOC_HI16_GOTOFF,
  BFD_RELOC_HI16_S_GOTOFF,
  BFD_RELOC_8_GOTOFF,
  BFD_RELOC_32_PLT_PCREL,
  BFD_RELOC_24_PLT_PCREL,
  BFD_RELOC_16_PLT_PCREL,
  BFD_RELOC_8_PLT_PCREL,
  BFD_RELOC_32_PLTOFF,
  BFD_RELOC_16_PLTOFF,
  BFD_RELOC_LO16_PLTOFF,
  BFD_RELOC_HI16_PLTOFF,
  BFD_RELOC_HI16_S_PLTOFF,
  BFD_RELOC_8_PLTOFF,

/* Relocations used by 68K ELF. */
  BFD_RELOC_68K_GLOB_DAT,
  BFD_RELOC_68K_JMP_SLOT,
  BFD_RELOC_68K_RELATIVE,

/* Linkage-table relative. */
  BFD_RELOC_32_BASEREL,
  BFD_RELOC_16_BASEREL,
  BFD_RELOC_LO16_BASEREL,
  BFD_RELOC_HI16_BASEREL,
  BFD_RELOC_HI16_S_BASEREL,
  BFD_RELOC_8_BASEREL,
  BFD_RELOC_RVA,

/* Absolute 8-bit relocation, but used to form an address like 0xFFnn. */
  BFD_RELOC_8_FFnn,

/* These PC-relative relocations are stored as word displacements --
i.e., byte displacements shifted right two bits.  The 30-bit word
displacement (<<32_PCREL_S2>> -- 32 bits, shifted 2) is used on the
SPARC.  (SPARC tools generally refer to this as <<WDISP30>>.)  The
signed 16-bit displacement is used on the MIPS, and the 23-bit
displacement is used on the Alpha. */
  BFD_RELOC_32_PCREL_S2,
  BFD_RELOC_16_PCREL_S2,
  BFD_RELOC_23_PCREL_S2,

/* High 22 bits and low 10 bits of 32-bit value, placed into lower bits of
the target word.  These are used on the SPARC. */
  BFD_RELOC_HI22,
  BFD_RELOC_LO10,

/* For systems that allocate a Global Pointer register, these are
displacements off that register.  These relocation types are
handled specially, because the value the register will have is
decided relatively late. */
  BFD_RELOC_GPREL16,
  BFD_RELOC_GPREL32,

/* Reloc types used for i960/b.out. */
  BFD_RELOC_I960_CALLJ,

/* SPARC ELF relocations.  There is probably some overlap with other
relocation types already defined. */
  BFD_RELOC_NONE,
  BFD_RELOC_SPARC_WDISP22,
  BFD_RELOC_SPARC22,
  BFD_RELOC_SPARC13,
  BFD_RELOC_SPARC_GOT10,
  BFD_RELOC_SPARC_GOT13,
  BFD_RELOC_SPARC_GOT22,
  BFD_RELOC_SPARC_PC10,
  BFD_RELOC_SPARC_PC22,
  BFD_RELOC_SPARC_WPLT30,
  BFD_RELOC_SPARC_COPY,
  BFD_RELOC_SPARC_GLOB_DAT,
  BFD_RELOC_SPARC_JMP_SLOT,
  BFD_RELOC_SPARC_RELATIVE,
  BFD_RELOC_SPARC_UA32,

/* I think these are specific to SPARC a.out (e.g., Sun 4). */
  BFD_RELOC_SPARC_BASE13,
  BFD_RELOC_SPARC_BASE22,

/* SPARC64 relocations */
#define BFD_RELOC_SPARC_64 BFD_RELOC_64
  BFD_RELOC_SPARC_10,
  BFD_RELOC_SPARC_11,
  BFD_RELOC_SPARC_OLO10,
  BFD_RELOC_SPARC_HH22,
  BFD_RELOC_SPARC_HM10,
  BFD_RELOC_SPARC_LM22,
  BFD_RELOC_SPARC_PC_HH22,
  BFD_RELOC_SPARC_PC_HM10,
  BFD_RELOC_SPARC_PC_LM22,
  BFD_RELOC_SPARC_WDISP16,
  BFD_RELOC_SPARC_WDISP19,
  BFD_RELOC_SPARC_7,
  BFD_RELOC_SPARC_6,
  BFD_RELOC_SPARC_5,
#define BFD_RELOC_SPARC_DISP64 BFD_RELOC_64_PCREL
  BFD_RELOC_SPARC_PLT64,
  BFD_RELOC_SPARC_HIX22,
  BFD_RELOC_SPARC_LOX10,
  BFD_RELOC_SPARC_H44,
  BFD_RELOC_SPARC_M44,
  BFD_RELOC_SPARC_L44,
  BFD_RELOC_SPARC_REGISTER,

/* Alpha ECOFF and ELF relocations.  Some of these treat the symbol or
"addend" in some special way.
For GPDISP_HI16 ("gpdisp") relocations, the symbol is ignored when
writing; when reading, it will be the absolute section symbol.  The
addend is the displacement in bytes of the "lda" instruction from
the "ldah" instruction (which is at the address of this reloc). */
  BFD_RELOC_ALPHA_GPDISP_HI16,

/* For GPDISP_LO16 ("ignore") relocations, the symbol is handled as
with GPDISP_HI16 relocs.  The addend is ignored when writing the
relocations out, and is filled in with the file's GP value on
reading, for convenience. */
  BFD_RELOC_ALPHA_GPDISP_LO16,

/* The ELF GPDISP relocation is exactly the same as the GPDISP_HI16
relocation except that there is no accompanying GPDISP_LO16
relocation. */
  BFD_RELOC_ALPHA_GPDISP,

/* The Alpha LITERAL/LITUSE relocs are produced by a symbol reference;
the assembler turns it into a LDQ instruction to load the address of
the symbol, and then fills in a register in the real instruction.

The LITERAL reloc, at the LDQ instruction, refers to the .lita
section symbol.  The addend is ignored when writing, but is filled
in with the file's GP value on reading, for convenience, as with the
GPDISP_LO16 reloc.

The ELF_LITERAL reloc is somewhere between 16_GOTOFF and GPDISP_LO16.
It should refer to the symbol to be referenced, as with 16_GOTOFF,
but it generates output not based on the position within the .got
section, but relative to the GP value chosen for the file during the
final link stage.

The LITUSE reloc, on the instruction using the loaded address, gives
information to the linker that it might be able to use to optimize
away some literal section references.  The symbol is ignored (read
as the absolute section symbol), and the "addend" indicates the type
of instruction using the register:
1 - "memory" fmt insn
2 - byte-manipulation (byte offset reg)
3 - jsr (target of branch)

The GNU linker currently doesn't do any of this optimizing. */
  BFD_RELOC_ALPHA_LITERAL,
  BFD_RELOC_ALPHA_ELF_LITERAL,
  BFD_RELOC_ALPHA_LITUSE,

/* The HINT relocation indicates a value that should be filled into the
"hint" field of a jmp/jsr/ret instruction, for possible branch-
prediction logic which may be provided on some processors. */
  BFD_RELOC_ALPHA_HINT,

/* The LINKAGE relocation outputs a linkage pair in the object file,
which is filled by the linker. */
  BFD_RELOC_ALPHA_LINKAGE,

/* The CODEADDR relocation outputs a STO_CA in the object file,
which is filled by the linker. */
  BFD_RELOC_ALPHA_CODEADDR,

/* Bits 27..2 of the relocation address shifted right 2 bits;
simple reloc otherwise. */
  BFD_RELOC_MIPS_JMP,

/* The MIPS16 jump instruction. */
  BFD_RELOC_MIPS16_JMP,

/* MIPS16 GP relative reloc. */
  BFD_RELOC_MIPS16_GPREL,

/* High 16 bits of 32-bit value; simple reloc. */
  BFD_RELOC_HI16,

/* High 16 bits of 32-bit value but the low 16 bits will be sign
extended and added to form the final result.  If the low 16
bits form a negative number, we need to add one to the high value
to compensate for the borrow when the low bits are added. */
  BFD_RELOC_HI16_S,

/* Low 16 bits. */
  BFD_RELOC_LO16,

/* Like BFD_RELOC_HI16_S, but PC relative. */
  BFD_RELOC_PCREL_HI16_S,

/* Like BFD_RELOC_LO16, but PC relative. */
  BFD_RELOC_PCREL_LO16,

/* Relocation relative to the global pointer. */
#define BFD_RELOC_MIPS_GPREL BFD_RELOC_GPREL16

/* Relocation against a MIPS literal section. */
  BFD_RELOC_MIPS_LITERAL,

/* MIPS ELF relocations. */
  BFD_RELOC_MIPS_GOT16,
  BFD_RELOC_MIPS_CALL16,
#define BFD_RELOC_MIPS_GPREL32 BFD_RELOC_GPREL32
  BFD_RELOC_MIPS_GOT_HI16,
  BFD_RELOC_MIPS_GOT_LO16,
  BFD_RELOC_MIPS_CALL_HI16,
  BFD_RELOC_MIPS_CALL_LO16,


/* i386/elf relocations */
  BFD_RELOC_386_GOT32,
  BFD_RELOC_386_PLT32,
  BFD_RELOC_386_COPY,
  BFD_RELOC_386_GLOB_DAT,
  BFD_RELOC_386_JUMP_SLOT,
  BFD_RELOC_386_RELATIVE,
  BFD_RELOC_386_GOTOFF,
  BFD_RELOC_386_GOTPC,

/* ns32k relocations */
  BFD_RELOC_NS32K_IMM_8,
  BFD_RELOC_NS32K_IMM_16,
  BFD_RELOC_NS32K_IMM_32,
  BFD_RELOC_NS32K_IMM_8_PCREL,
  BFD_RELOC_NS32K_IMM_16_PCREL,
  BFD_RELOC_NS32K_IMM_32_PCREL,
  BFD_RELOC_NS32K_DISP_8,
  BFD_RELOC_NS32K_DISP_16,
  BFD_RELOC_NS32K_DISP_32,
  BFD_RELOC_NS32K_DISP_8_PCREL,
  BFD_RELOC_NS32K_DISP_16_PCREL,
  BFD_RELOC_NS32K_DISP_32_PCREL,

/* Power(rs6000) and PowerPC relocations. */
  BFD_RELOC_PPC_B26,
  BFD_RELOC_PPC_BA26,
  BFD_RELOC_PPC_TOC16,
  BFD_RELOC_PPC_B16,
  BFD_RELOC_PPC_B16_BRTAKEN,
  BFD_RELOC_PPC_B16_BRNTAKEN,
  BFD_RELOC_PPC_BA16,
  BFD_RELOC_PPC_BA16_BRTAKEN,
  BFD_RELOC_PPC_BA16_BRNTAKEN,
  BFD_RELOC_PPC_COPY,
  BFD_RELOC_PPC_GLOB_DAT,
  BFD_RELOC_PPC_JMP_SLOT,
  BFD_RELOC_PPC_RELATIVE,
  BFD_RELOC_PPC_LOCAL24PC,
  BFD_RELOC_PPC_EMB_NADDR32,
  BFD_RELOC_PPC_EMB_NADDR16,
  BFD_RELOC_PPC_EMB_NADDR16_LO,
  BFD_RELOC_PPC_EMB_NADDR16_HI,
  BFD_RELOC_PPC_EMB_NADDR16_HA,
  BFD_RELOC_PPC_EMB_SDAI16,
  BFD_RELOC_PPC_EMB_SDA2I16,
  BFD_RELOC_PPC_EMB_SDA2REL,
  BFD_RELOC_PPC_EMB_SDA21,
  BFD_RELOC_PPC_EMB_MRKREF,
  BFD_RELOC_PPC_EMB_RELSEC16,
  BFD_RELOC_PPC_EMB_RELST_LO,
  BFD_RELOC_PPC_EMB_RELST_HI,
  BFD_RELOC_PPC_EMB_RELST_HA,
  BFD_RELOC_PPC_EMB_BIT_FLD,
  BFD_RELOC_PPC_EMB_RELSDA,

/* The type of reloc used to build a contructor table - at the moment
probably a 32 bit wide absolute relocation, but the target can choose.
It generally does map to one of the other relocation types. */
  BFD_RELOC_CTOR,

/* ARM 26 bit pc-relative branch.  The lowest two bits must be zero and are
not stored in the instruction. */
  BFD_RELOC_ARM_PCREL_BRANCH,

/* These relocs are only used within the ARM assembler.  They are not
(at present) written to any object files. */
  BFD_RELOC_ARM_IMMEDIATE,
  BFD_RELOC_ARM_OFFSET_IMM,
  BFD_RELOC_ARM_SHIFT_IMM,
  BFD_RELOC_ARM_SWI,
  BFD_RELOC_ARM_MULTI,
  BFD_RELOC_ARM_CP_OFF_IMM,
  BFD_RELOC_ARM_ADR_IMM,
  BFD_RELOC_ARM_LDR_IMM,
  BFD_RELOC_ARM_LITERAL,
  BFD_RELOC_ARM_IN_POOL,
  BFD_RELOC_ARM_OFFSET_IMM8,
  BFD_RELOC_ARM_HWLITERAL,
  BFD_RELOC_ARM_THUMB_ADD,
  BFD_RELOC_ARM_THUMB_IMM,
  BFD_RELOC_ARM_THUMB_SHIFT,
  BFD_RELOC_ARM_THUMB_OFFSET,

/* Hitachi SH relocs.  Not all of these appear in object files. */
  BFD_RELOC_SH_PCDISP8BY2,
  BFD_RELOC_SH_PCDISP12BY2,
  BFD_RELOC_SH_IMM4,
  BFD_RELOC_SH_IMM4BY2,
  BFD_RELOC_SH_IMM4BY4,
  BFD_RELOC_SH_IMM8,
  BFD_RELOC_SH_IMM8BY2,
  BFD_RELOC_SH_IMM8BY4,
  BFD_RELOC_SH_PCRELIMM8BY2,
  BFD_RELOC_SH_PCRELIMM8BY4,
  BFD_RELOC_SH_SWITCH16,
  BFD_RELOC_SH_SWITCH32,
  BFD_RELOC_SH_USES,
  BFD_RELOC_SH_COUNT,
  BFD_RELOC_SH_ALIGN,
  BFD_RELOC_SH_CODE,
  BFD_RELOC_SH_DATA,
  BFD_RELOC_SH_LABEL,

/* Thumb 23-, 12- and 9-bit pc-relative branches.  The lowest bit must
be zero and is not stored in the instruction. */
  BFD_RELOC_THUMB_PCREL_BRANCH9,
  BFD_RELOC_THUMB_PCREL_BRANCH12,
  BFD_RELOC_THUMB_PCREL_BRANCH23,

/* Argonaut RISC Core (ARC) relocs.
ARC 22 bit pc-relative branch.  The lowest two bits must be zero and are
not stored in the instruction.  The high 20 bits are installed in bits 26
through 7 of the instruction. */
  BFD_RELOC_ARC_B22_PCREL,

/* ARC 26 bit absolute branch.  The lowest two bits must be zero and are not
stored in the instruction.  The high 24 bits are installed in bits 23
through 0. */
  BFD_RELOC_ARC_B26,

/* Mitsubishi D10V relocs.
This is a 10-bit reloc with the right 2 bits
assumed to be 0. */
  BFD_RELOC_D10V_10_PCREL_R,

/* Mitsubishi D10V relocs.
This is a 10-bit reloc with the right 2 bits
assumed to be 0.  This is the same as the previous reloc
except it is in the left container, i.e.,
shifted left 15 bits. */
  BFD_RELOC_D10V_10_PCREL_L,

/* This is an 18-bit reloc with the right 2 bits
assumed to be 0. */
  BFD_RELOC_D10V_18,

/* This is an 18-bit reloc with the right 2 bits
assumed to be 0. */
  BFD_RELOC_D10V_18_PCREL,



/* Mitsubishi M32R relocs.
This is a 24 bit absolute address. */
  BFD_RELOC_M32R_24,

/* This is a 10-bit pc-relative reloc with the right 2 bits assumed to be 0. */
  BFD_RELOC_M32R_10_PCREL,

/* This is an 18-bit reloc with the right 2 bits assumed to be 0. */
  BFD_RELOC_M32R_18_PCREL,

/* This is a 26-bit reloc with the right 2 bits assumed to be 0. */
  BFD_RELOC_M32R_26_PCREL,

/* This is a 16-bit reloc containing the high 16 bits of an address
used when the lower 16 bits are treated as unsigned. */
  BFD_RELOC_M32R_HI16_ULO,

/* This is a 16-bit reloc containing the high 16 bits of an address
used when the lower 16 bits are treated as signed. */
  BFD_RELOC_M32R_HI16_SLO,

/* This is a 16-bit reloc containing the lower 16 bits of an address. */
  BFD_RELOC_M32R_LO16,

/* This is a 16-bit reloc containing the small data area offset for use in
add3, load, and store instructions. */
  BFD_RELOC_M32R_SDA16,

/* This is a 9-bit reloc */
  BFD_RELOC_V850_9_PCREL,

/* This is a 22-bit reloc */
  BFD_RELOC_V850_22_PCREL,

/* This is a 16 bit offset from the short data area pointer. */
  BFD_RELOC_V850_SDA_16_16_OFFSET,

/* This is a 16 bit offset (of which only 15 bits are used) from the
short data area pointer. */
  BFD_RELOC_V850_SDA_15_16_OFFSET,

/* This is a 16 bit offset from the zero data area pointer. */
  BFD_RELOC_V850_ZDA_16_16_OFFSET,

/* This is a 16 bit offset (of which only 15 bits are used) from the
zero data area pointer. */
  BFD_RELOC_V850_ZDA_15_16_OFFSET,

/* This is an 8 bit offset (of which only 6 bits are used) from the
tiny data area pointer. */
  BFD_RELOC_V850_TDA_6_8_OFFSET,

/* This is an 8bit offset (of which only 7 bits are used) from the tiny
data area pointer. */
  BFD_RELOC_V850_TDA_7_8_OFFSET,

/* This is a 7 bit offset from the tiny data area pointer. */
  BFD_RELOC_V850_TDA_7_7_OFFSET,

/* This is a 16 bit offset from the tiny data area pointer. */
  BFD_RELOC_V850_TDA_16_16_OFFSET,


/* This is a 32bit pcrel reloc for the mn10300, offset by two bytes in the
instruction. */
  BFD_RELOC_MN10300_32_PCREL,

/* This is a 16bit pcrel reloc for the mn10300, offset by two bytes in the
instruction. */
  BFD_RELOC_MN10300_16_PCREL,

/* This is a 8bit DP reloc for the tms320c30, where the most
significant 8 bits of a 24 bit word are placed into the least
significant 8 bits of the opcode. */
  BFD_RELOC_TIC30_LDP,
  BFD_RELOC_UNUSED };
typedef enum bfd_reloc_code_real bfd_reloc_code_real_type;
reloc_howto_type *

bfd_reloc_type_lookup  PARAMS ((bfd *abfd, bfd_reloc_code_real_type code));

const char *
bfd_get_reloc_code_name  PARAMS ((bfd_reloc_code_real_type code));


typedef struct symbol_cache_entry
{
        /* A pointer to the BFD which owns the symbol. This information
          is necessary so that a back end can work out what additional
          information (invisible to the application writer) is carried
          with the symbol.

          This field is *almost* redundant, since you can use section->owner
          instead, except that some symbols point to the global sections
          bfd_{abs,com,und}_section.  This could be fixed by making
          these globals be per-bfd (or per-target-flavor).  FIXME. */

  struct _bfd *the_bfd;  /* Use bfd_asymbol_bfd(sym) to access this field. */

        /* The text of the symbol. The name is left alone, and not copied; the
          application may not alter it. */
  CONST char *name;

        /* The value of the symbol.  This really should be a union of a
          numeric value with a pointer, since some flags indicate that
          a pointer to another symbol is stored here.  */
  symvalue value;

        /* Attributes of a symbol: */

#define BSF_NO_FLAGS    0x00

        /* The symbol has local scope; <<static>> in <<C>>. The value
          is the offset into the section of the data. */
#define BSF_LOCAL      0x01

        /* The symbol has global scope; initialized data in <<C>>. The
          value is the offset into the section of the data. */
#define BSF_GLOBAL     0x02

        /* The symbol has global scope and is exported. The value is
          the offset into the section of the data. */
#define BSF_EXPORT     BSF_GLOBAL  /* no real difference */

        /* A normal C symbol would be one of:
          <<BSF_LOCAL>>, <<BSF_FORT_COMM>>,  <<BSF_UNDEFINED>> or
          <<BSF_GLOBAL>> */

        /* The symbol is a debugging record. The value has an arbitary
          meaning. */
#define BSF_DEBUGGING  0x08

        /* The symbol denotes a function entry point.  Used in ELF,
          perhaps others someday.  */
#define BSF_FUNCTION    0x10

        /* Used by the linker. */
#define BSF_KEEP        0x20
#define BSF_KEEP_G      0x40

        /* A weak global symbol, overridable without warnings by
          a regular global symbol of the same name.  */
#define BSF_WEAK        0x80

        /* This symbol was created to point to a section, e.g. ELF's
          STT_SECTION symbols.  */
#define BSF_SECTION_SYM 0x100

        /* The symbol used to be a common symbol, but now it is
          allocated. */
#define BSF_OLD_COMMON  0x200

        /* The default value for common data. */
#define BFD_FORT_COMM_DEFAULT_VALUE 0

        /* In some files the type of a symbol sometimes alters its
          location in an output file - ie in coff a <<ISFCN>> symbol
          which is also <<C_EXT>> symbol appears where it was
          declared and not at the end of a section.  This bit is set
          by the target BFD part to convey this information. */

#define BSF_NOT_AT_END    0x400

        /* Signal that the symbol is the label of constructor section. */
#define BSF_CONSTRUCTOR   0x800

        /* Signal that the symbol is a warning symbol.  The name is a
          warning.  The name of the next symbol is the one to warn about;
          if a reference is made to a symbol with the same name as the next
          symbol, a warning is issued by the linker. */
#define BSF_WARNING       0x1000

        /* Signal that the symbol is indirect.  This symbol is an indirect
          pointer to the symbol with the same name as the next symbol. */
#define BSF_INDIRECT      0x2000

        /* BSF_FILE marks symbols that contain a file name.  This is used
          for ELF STT_FILE symbols.  */
#define BSF_FILE          0x4000

        /* Symbol is from dynamic linking information.  */
#define BSF_DYNAMIC       0x8000

        /* The symbol denotes a data object.  Used in ELF, and perhaps
          others someday.  */
#define BSF_OBJECT        0x10000

  flagword flags;

        /* A pointer to the section to which this symbol is
          relative.  This will always be non NULL, there are special
          sections for undefined and absolute symbols.  */
  struct sec *section;

        /* Back end special data.  */
  union
    {
      PTR p;
      bfd_vma i;
    } udata;

} asymbol;
#define bfd_get_symtab_upper_bound(abfd) \
     BFD_SEND (abfd, _bfd_get_symtab_upper_bound, (abfd))
boolean 
bfd_is_local_label PARAMS ((bfd *abfd, asymbol *sym));

boolean 
bfd_is_local_label_name PARAMS ((bfd *abfd, const char *name));

#define bfd_is_local_label_name(abfd, name) \
     BFD_SEND (abfd, _bfd_is_local_label_name, (abfd, name))
#define bfd_canonicalize_symtab(abfd, location) \
     BFD_SEND (abfd, _bfd_canonicalize_symtab,\
                  (abfd, location))
boolean 
bfd_set_symtab  PARAMS ((bfd *abfd, asymbol **location, unsigned int count));

void 
bfd_print_symbol_vandf PARAMS ((PTR file, asymbol *symbol));

#define bfd_make_empty_symbol(abfd) \
     BFD_SEND (abfd, _bfd_make_empty_symbol, (abfd))
#define bfd_make_debug_symbol(abfd,ptr,size) \
        BFD_SEND (abfd, _bfd_make_debug_symbol, (abfd, ptr, size))
int 
bfd_decode_symclass PARAMS ((asymbol *symbol));

void 
bfd_symbol_info PARAMS ((asymbol *symbol, symbol_info *ret));

boolean 
bfd_copy_private_symbol_data PARAMS ((bfd *ibfd, asymbol *isym, bfd *obfd, asymbol *osym));

#define bfd_copy_private_symbol_data(ibfd, isymbol, obfd, osymbol) \
     BFD_SEND (obfd, _bfd_copy_private_symbol_data, \
               (ibfd, isymbol, obfd, osymbol))
struct _bfd 
{
     /* The filename the application opened the BFD with.  */
    CONST char *filename;                

     /* A pointer to the target jump table.             */
    const struct bfd_target *xvec;

     /* To avoid dragging too many header files into every file that
       includes `<<bfd.h>>', IOSTREAM has been declared as a "char
       *", and MTIME as a "long".  Their correct types, to which they
       are cast when used, are "FILE *" and "time_t".    The iostream
       is the result of an fopen on the filename.  However, if the
       BFD_IN_MEMORY flag is set, then iostream is actually a pointer
       to a bfd_in_memory struct.  */
    PTR iostream;

     /* Is the file descriptor being cached?  That is, can it be closed as
       needed, and re-opened when accessed later?  */

    boolean cacheable;

     /* Marks whether there was a default target specified when the
       BFD was opened. This is used to select which matching algorithm
       to use to choose the back end. */

    boolean target_defaulted;

     /* The caching routines use these to maintain a
       least-recently-used list of BFDs */

    struct _bfd *lru_prev, *lru_next;

     /* When a file is closed by the caching routines, BFD retains
       state information on the file here: */

    file_ptr where;              

     /* and here: (``once'' means at least once) */

    boolean opened_once;

     /* Set if we have a locally maintained mtime value, rather than
       getting it from the file each time: */

    boolean mtime_set;

     /* File modified time, if mtime_set is true: */

    long mtime;          

     /* Reserved for an unimplemented file locking extension.*/

    int ifd;

     /* The format which belongs to the BFD. (object, core, etc.) */

    bfd_format format;

     /* The direction the BFD was opened with*/

    enum bfd_direction {no_direction = 0,
                        read_direction = 1,
                        write_direction = 2,
                        both_direction = 3} direction;

     /* Format_specific flags*/

    flagword flags;              

     /* Currently my_archive is tested before adding origin to
       anything. I believe that this can become always an add of
       origin, with origin set to 0 for non archive files.   */

    file_ptr origin;             

     /* Remember when output has begun, to stop strange things
       from happening. */
    boolean output_has_begun;

     /* Pointer to linked list of sections*/
    struct sec  *sections;

     /* The number of sections */
    unsigned int section_count;

     /* Stuff only useful for object files: 
       The start address. */
    bfd_vma start_address;

     /* Used for input and output*/
    unsigned int symcount;

     /* Symbol table for output BFD (with symcount entries) */
    struct symbol_cache_entry  **outsymbols;             

     /* Pointer to structure which contains architecture information*/
    const struct bfd_arch_info *arch_info;

     /* Stuff only useful for archives:*/
    PTR arelt_data;              
    struct _bfd *my_archive;      /* The containing archive BFD.  */
    struct _bfd *next;            /* The next BFD in the archive.  */
    struct _bfd *archive_head;    /* The first BFD in the archive.  */
    boolean has_armap;           

     /* A chain of BFD structures involved in a link.  */
    struct _bfd *link_next;

     /* A field used by _bfd_generic_link_add_archive_symbols.  This will
       be used only for archive elements.  */
    int archive_pass;

     /* Used by the back end to hold private data. */

    union 
      {
      struct aout_data_struct *aout_data;
      struct artdata *aout_ar_data;
      struct _oasys_data *oasys_obj_data;
      struct _oasys_ar_data *oasys_ar_data;
      struct coff_tdata *coff_obj_data;
      struct pe_tdata *pe_obj_data;
      struct xcoff_tdata *xcoff_obj_data;
      struct ecoff_tdata *ecoff_obj_data;
      struct ieee_data_struct *ieee_data;
      struct ieee_ar_data_struct *ieee_ar_data;
      struct srec_data_struct *srec_data;
      struct ihex_data_struct *ihex_data;
      struct tekhex_data_struct *tekhex_data;
      struct elf_obj_tdata *elf_obj_data;
      struct nlm_obj_tdata *nlm_obj_data;
      struct bout_data_struct *bout_data;
      struct sun_core_struct *sun_core_data;
      struct trad_core_struct *trad_core_data;
      struct som_data_struct *som_data;
      struct hpux_core_struct *hpux_core_data;
      struct hppabsd_core_struct *hppabsd_core_data;
      struct sgi_core_struct *sgi_core_data;
      struct lynx_core_struct *lynx_core_data;
      struct osf_core_struct *osf_core_data;
      struct cisco_core_struct *cisco_core_data;
      struct versados_data_struct *versados_data;
      struct netbsd_core_struct *netbsd_core_data;
      PTR any;
      } tdata;
  
     /* Used by the application to hold private data*/
    PTR usrdata;

   /* Where all the allocated stuff under this BFD goes.  This is a
     struct objalloc *, but we use PTR to avoid requiring the inclusion of
     objalloc.h.  */
    PTR memory;
};

typedef enum bfd_error
{
  bfd_error_no_error = 0,
  bfd_error_system_call,
  bfd_error_invalid_target,
  bfd_error_wrong_format,
  bfd_error_invalid_operation,
  bfd_error_no_memory,
  bfd_error_no_symbols,
  bfd_error_no_armap,
  bfd_error_no_more_archived_files,
  bfd_error_malformed_archive,
  bfd_error_file_not_recognized,
  bfd_error_file_ambiguously_recognized,
  bfd_error_no_contents,
  bfd_error_nonrepresentable_section,
  bfd_error_no_debug_section,
  bfd_error_bad_value,
  bfd_error_file_truncated,
  bfd_error_file_too_big,
  bfd_error_invalid_error_code
} bfd_error_type;

bfd_error_type 
bfd_get_error  PARAMS ((void));

void 
bfd_set_error  PARAMS ((bfd_error_type error_tag));

CONST char *
bfd_errmsg  PARAMS ((bfd_error_type error_tag));

void 
bfd_perror  PARAMS ((CONST char *message));

typedef void (*bfd_error_handler_type) PARAMS ((const char *, ...));

bfd_error_handler_type 
bfd_set_error_handler  PARAMS ((bfd_error_handler_type));

void 
bfd_set_error_program_name  PARAMS ((const char *));

bfd_error_handler_type 
bfd_get_error_handler  PARAMS ((void));

long 
bfd_get_reloc_upper_bound PARAMS ((bfd *abfd, asection *sect));

long 
bfd_canonicalize_reloc
 PARAMS ((bfd *abfd,
    asection *sec,
    arelent **loc,
    asymbol **syms));

void 
bfd_set_reloc
 PARAMS ((bfd *abfd, asection *sec, arelent **rel, unsigned int count)
    
    );

boolean 
bfd_set_file_flags PARAMS ((bfd *abfd, flagword flags));

boolean 
bfd_set_start_address PARAMS ((bfd *abfd, bfd_vma vma));

long 
bfd_get_mtime PARAMS ((bfd *abfd));

long 
bfd_get_size PARAMS ((bfd *abfd));

int 
bfd_get_gp_size PARAMS ((bfd *abfd));

void 
bfd_set_gp_size PARAMS ((bfd *abfd, int i));

bfd_vma 
bfd_scan_vma PARAMS ((CONST char *string, CONST char **end, int base));

boolean 
bfd_copy_private_bfd_data PARAMS ((bfd *ibfd, bfd *obfd));

#define bfd_copy_private_bfd_data(ibfd, obfd) \
     BFD_SEND (obfd, _bfd_copy_private_bfd_data, \
               (ibfd, obfd))
boolean 
bfd_merge_private_bfd_data PARAMS ((bfd *ibfd, bfd *obfd));

#define bfd_merge_private_bfd_data(ibfd, obfd) \
     BFD_SEND (obfd, _bfd_merge_private_bfd_data, \
               (ibfd, obfd))
boolean 
bfd_set_private_flags PARAMS ((bfd *abfd, flagword flags));

#define bfd_set_private_flags(abfd, flags) \
     BFD_SEND (abfd, _bfd_set_private_flags, \
               (abfd, flags))
#define bfd_sizeof_headers(abfd, reloc) \
     BFD_SEND (abfd, _bfd_sizeof_headers, (abfd, reloc))

#define bfd_find_nearest_line(abfd, sec, syms, off, file, func, line) \
     BFD_SEND (abfd, _bfd_find_nearest_line,  (abfd, sec, syms, off, file, func, line))

        /* Do these three do anything useful at all, for any back end?  */
#define bfd_debug_info_start(abfd) \
        BFD_SEND (abfd, _bfd_debug_info_start, (abfd))

#define bfd_debug_info_end(abfd) \
        BFD_SEND (abfd, _bfd_debug_info_end, (abfd))

#define bfd_debug_info_accumulate(abfd, section) \
        BFD_SEND (abfd, _bfd_debug_info_accumulate, (abfd, section))


#define bfd_stat_arch_elt(abfd, stat) \
        BFD_SEND (abfd, _bfd_stat_arch_elt,(abfd, stat))

#define bfd_update_armap_timestamp(abfd) \
        BFD_SEND (abfd, _bfd_update_armap_timestamp, (abfd))

#define bfd_set_arch_mach(abfd, arch, mach)\
        BFD_SEND ( abfd, _bfd_set_arch_mach, (abfd, arch, mach))

#define bfd_relax_section(abfd, section, link_info, again) \
       BFD_SEND (abfd, _bfd_relax_section, (abfd, section, link_info, again))

#define bfd_link_hash_table_create(abfd) \
       BFD_SEND (abfd, _bfd_link_hash_table_create, (abfd))

#define bfd_link_add_symbols(abfd, info) \
       BFD_SEND (abfd, _bfd_link_add_symbols, (abfd, info))

#define bfd_final_link(abfd, info) \
       BFD_SEND (abfd, _bfd_final_link, (abfd, info))

#define bfd_free_cached_info(abfd) \
       BFD_SEND (abfd, _bfd_free_cached_info, (abfd))

#define bfd_get_dynamic_symtab_upper_bound(abfd) \
       BFD_SEND (abfd, _bfd_get_dynamic_symtab_upper_bound, (abfd))

#define bfd_print_private_bfd_data(abfd, file)\
       BFD_SEND (abfd, _bfd_print_private_bfd_data, (abfd, file))

#define bfd_canonicalize_dynamic_symtab(abfd, asymbols) \
       BFD_SEND (abfd, _bfd_canonicalize_dynamic_symtab, (abfd, asymbols))

#define bfd_get_dynamic_reloc_upper_bound(abfd) \
       BFD_SEND (abfd, _bfd_get_dynamic_reloc_upper_bound, (abfd))

#define bfd_canonicalize_dynamic_reloc(abfd, arels, asyms) \
       BFD_SEND (abfd, _bfd_canonicalize_dynamic_reloc, (abfd, arels, asyms))

extern bfd_byte *bfd_get_relocated_section_contents
       PARAMS ((bfd *, struct bfd_link_info *,
                 struct bfd_link_order *, bfd_byte *,
                 boolean, asymbol **));

symindex 
bfd_get_next_mapent PARAMS ((bfd *abfd, symindex previous, carsym **sym));

boolean 
bfd_set_archive_head PARAMS ((bfd *output, bfd *new_head));

bfd *
bfd_openr_next_archived_file PARAMS ((bfd *archive, bfd *previous));

CONST char *
bfd_core_file_failing_command PARAMS ((bfd *abfd));

int 
bfd_core_file_failing_signal PARAMS ((bfd *abfd));

boolean 
core_file_matches_executable_p
 PARAMS ((bfd *core_bfd, bfd *exec_bfd));

#define BFD_SEND(bfd, message, arglist) \
               ((*((bfd)->xvec->message)) arglist)

#ifdef DEBUG_BFD_SEND
#undef BFD_SEND
#define BFD_SEND(bfd, message, arglist) \
  (((bfd) && (bfd)->xvec && (bfd)->xvec->message) ? \
    ((*((bfd)->xvec->message)) arglist) : \
    (bfd_assert (__FILE__,__LINE__), NULL))
#endif
#define BFD_SEND_FMT(bfd, message, arglist) \
            (((bfd)->xvec->message[(int)((bfd)->format)]) arglist)

#ifdef DEBUG_BFD_SEND
#undef BFD_SEND_FMT
#define BFD_SEND_FMT(bfd, message, arglist) \
  (((bfd) && (bfd)->xvec && (bfd)->xvec->message) ? \
   (((bfd)->xvec->message[(int)((bfd)->format)]) arglist) : \
   (bfd_assert (__FILE__,__LINE__), NULL))
#endif
enum bfd_flavour {
  bfd_target_unknown_flavour,
  bfd_target_aout_flavour,
  bfd_target_coff_flavour,
  bfd_target_ecoff_flavour,
  bfd_target_elf_flavour,
  bfd_target_ieee_flavour,
  bfd_target_nlm_flavour,
  bfd_target_oasys_flavour,
  bfd_target_tekhex_flavour,
  bfd_target_srec_flavour,
  bfd_target_ihex_flavour,
  bfd_target_som_flavour,
  bfd_target_os9k_flavour,
  bfd_target_versados_flavour,
  bfd_target_msdos_flavour,
  bfd_target_evax_flavour
};

enum bfd_endian { BFD_ENDIAN_BIG, BFD_ENDIAN_LITTLE, BFD_ENDIAN_UNKNOWN };

 /* Forward declaration.  */
typedef struct bfd_link_info _bfd_link_info;

typedef struct bfd_target
{
  char *name;
  enum bfd_flavour flavour;
  enum bfd_endian byteorder;
  enum bfd_endian header_byteorder;
  flagword object_flags;       
  flagword section_flags;
  char symbol_leading_char;
  char ar_pad_char;            
  unsigned short ar_max_namelen;
  bfd_vma      (*bfd_getx64) PARAMS ((const bfd_byte *));
  bfd_signed_vma (*bfd_getx_signed_64) PARAMS ((const bfd_byte *));
  void         (*bfd_putx64) PARAMS ((bfd_vma, bfd_byte *));
  bfd_vma      (*bfd_getx32) PARAMS ((const bfd_byte *));
  bfd_signed_vma (*bfd_getx_signed_32) PARAMS ((const bfd_byte *));
  void         (*bfd_putx32) PARAMS ((bfd_vma, bfd_byte *));
  bfd_vma      (*bfd_getx16) PARAMS ((const bfd_byte *));
  bfd_signed_vma (*bfd_getx_signed_16) PARAMS ((const bfd_byte *));
  void         (*bfd_putx16) PARAMS ((bfd_vma, bfd_byte *));
  bfd_vma      (*bfd_h_getx64) PARAMS ((const bfd_byte *));
  bfd_signed_vma (*bfd_h_getx_signed_64) PARAMS ((const bfd_byte *));
  void         (*bfd_h_putx64) PARAMS ((bfd_vma, bfd_byte *));
  bfd_vma      (*bfd_h_getx32) PARAMS ((const bfd_byte *));
  bfd_signed_vma (*bfd_h_getx_signed_32) PARAMS ((const bfd_byte *));
  void         (*bfd_h_putx32) PARAMS ((bfd_vma, bfd_byte *));
  bfd_vma      (*bfd_h_getx16) PARAMS ((const bfd_byte *));
  bfd_signed_vma (*bfd_h_getx_signed_16) PARAMS ((const bfd_byte *));
  void         (*bfd_h_putx16) PARAMS ((bfd_vma, bfd_byte *));
  const struct bfd_target *(*_bfd_check_format[bfd_type_end]) PARAMS ((bfd *));
  boolean             (*_bfd_set_format[bfd_type_end]) PARAMS ((bfd *));
  boolean             (*_bfd_write_contents[bfd_type_end]) PARAMS ((bfd *));

   /* Generic entry points.  */
#define BFD_JUMP_TABLE_GENERIC(NAME)\
CAT(NAME,_close_and_cleanup),\
CAT(NAME,_bfd_free_cached_info),\
CAT(NAME,_new_section_hook),\
CAT(NAME,_get_section_contents),\
CAT(NAME,_get_section_contents_in_window)

   /* Called when the BFD is being closed to do any necessary cleanup.  */
  boolean       (*_close_and_cleanup) PARAMS ((bfd *));
   /* Ask the BFD to free all cached information.  */
  boolean (*_bfd_free_cached_info) PARAMS ((bfd *));
   /* Called when a new section is created.  */
  boolean       (*_new_section_hook) PARAMS ((bfd *, sec_ptr));
   /* Read the contents of a section.  */
  boolean       (*_bfd_get_section_contents) PARAMS ((bfd *, sec_ptr, PTR, 
                                            file_ptr, bfd_size_type));
  boolean       (*_bfd_get_section_contents_in_window)
                          PARAMS ((bfd *, sec_ptr, bfd_window *,
                                   file_ptr, bfd_size_type));

   /* Entry points to copy private data.  */
#define BFD_JUMP_TABLE_COPY(NAME)\
CAT(NAME,_bfd_copy_private_bfd_data),\
CAT(NAME,_bfd_merge_private_bfd_data),\
CAT(NAME,_bfd_copy_private_section_data),\
CAT(NAME,_bfd_copy_private_symbol_data),\
CAT(NAME,_bfd_set_private_flags),\
CAT(NAME,_bfd_print_private_bfd_data)\
   /* Called to copy BFD general private data from one object file
     to another.  */
  boolean       (*_bfd_copy_private_bfd_data) PARAMS ((bfd *, bfd *));
   /* Called to merge BFD general private data from one object file
     to a common output file when linking.  */
  boolean       (*_bfd_merge_private_bfd_data) PARAMS ((bfd *, bfd *));
   /* Called to copy BFD private section data from one object file
     to another.  */
  boolean       (*_bfd_copy_private_section_data) PARAMS ((bfd *, sec_ptr,
                                                       bfd *, sec_ptr));
   /* Called to copy BFD private symbol data from one symbol 
     to another.  */
  boolean       (*_bfd_copy_private_symbol_data) PARAMS ((bfd *, asymbol *,
                                                          bfd *, asymbol *));
   /* Called to set private backend flags */
  boolean       (*_bfd_set_private_flags) PARAMS ((bfd *, flagword));

   /* Called to print private BFD data */
  boolean       (*_bfd_print_private_bfd_data) PARAMS ((bfd *, PTR));

   /* Core file entry points.  */
#define BFD_JUMP_TABLE_CORE(NAME)\
CAT(NAME,_core_file_failing_command),\
CAT(NAME,_core_file_failing_signal),\
CAT(NAME,_core_file_matches_executable_p)
  char *   (*_core_file_failing_command) PARAMS ((bfd *));
  int      (*_core_file_failing_signal) PARAMS ((bfd *));
  boolean  (*_core_file_matches_executable_p) PARAMS ((bfd *, bfd *));

   /* Archive entry points.  */
#define BFD_JUMP_TABLE_ARCHIVE(NAME)\
CAT(NAME,_slurp_armap),\
CAT(NAME,_slurp_extended_name_table),\
CAT(NAME,_construct_extended_name_table),\
CAT(NAME,_truncate_arname),\
CAT(NAME,_write_armap),\
CAT(NAME,_read_ar_hdr),\
CAT(NAME,_openr_next_archived_file),\
CAT(NAME,_get_elt_at_index),\
CAT(NAME,_generic_stat_arch_elt),\
CAT(NAME,_update_armap_timestamp)
  boolean  (*_bfd_slurp_armap) PARAMS ((bfd *));
  boolean  (*_bfd_slurp_extended_name_table) PARAMS ((bfd *));
  boolean  (*_bfd_construct_extended_name_table)
             PARAMS ((bfd *, char **, bfd_size_type *, const char **));
  void     (*_bfd_truncate_arname) PARAMS ((bfd *, CONST char *, char *));
  boolean  (*write_armap) PARAMS ((bfd *arch, 
                              unsigned int elength,
                              struct orl *map,
                              unsigned int orl_count, 
                              int stridx));
  PTR (*_bfd_read_ar_hdr_fn) PARAMS ((bfd *));
  bfd *    (*openr_next_archived_file) PARAMS ((bfd *arch, bfd *prev));
#define bfd_get_elt_at_index(b,i) BFD_SEND(b, _bfd_get_elt_at_index, (b,i))
  bfd *    (*_bfd_get_elt_at_index) PARAMS ((bfd *, symindex));
  int      (*_bfd_stat_arch_elt) PARAMS ((bfd *, struct stat *));
  boolean  (*_bfd_update_armap_timestamp) PARAMS ((bfd *));

   /* Entry points used for symbols.  */
#define BFD_JUMP_TABLE_SYMBOLS(NAME)\
CAT(NAME,_get_symtab_upper_bound),\
CAT(NAME,_get_symtab),\
CAT(NAME,_make_empty_symbol),\
CAT(NAME,_print_symbol),\
CAT(NAME,_get_symbol_info),\
CAT(NAME,_bfd_is_local_label_name),\
CAT(NAME,_get_lineno),\
CAT(NAME,_find_nearest_line),\
CAT(NAME,_bfd_make_debug_symbol),\
CAT(NAME,_read_minisymbols),\
CAT(NAME,_minisymbol_to_symbol)
  long  (*_bfd_get_symtab_upper_bound) PARAMS ((bfd *));
  long  (*_bfd_canonicalize_symtab) PARAMS ((bfd *,
                                             struct symbol_cache_entry **));
  struct symbol_cache_entry  *
                (*_bfd_make_empty_symbol) PARAMS ((bfd *));
  void          (*_bfd_print_symbol) PARAMS ((bfd *, PTR,
                                      struct symbol_cache_entry *,
                                      bfd_print_symbol_type));
#define bfd_print_symbol(b,p,s,e) BFD_SEND(b, _bfd_print_symbol, (b,p,s,e))
  void          (*_bfd_get_symbol_info) PARAMS ((bfd *,
                                      struct symbol_cache_entry *,
                                      symbol_info *));
#define bfd_get_symbol_info(b,p,e) BFD_SEND(b, _bfd_get_symbol_info, (b,p,e))
  boolean       (*_bfd_is_local_label_name) PARAMS ((bfd *, const char *));

  alent *    (*_get_lineno) PARAMS ((bfd *, struct symbol_cache_entry *));
  boolean    (*_bfd_find_nearest_line) PARAMS ((bfd *abfd,
                    struct sec *section, struct symbol_cache_entry **symbols,
                    bfd_vma offset, CONST char **file, CONST char **func,
                    unsigned int *line));
  /* Back-door to allow format-aware applications to create debug symbols
    while using BFD for everything else.  Currently used by the assembler
    when creating COFF files.  */
  asymbol *  (*_bfd_make_debug_symbol) PARAMS ((
       bfd *abfd,
       void *ptr,
       unsigned long size));
#define bfd_read_minisymbols(b, d, m, s) \
  BFD_SEND (b, _read_minisymbols, (b, d, m, s))
  long  (*_read_minisymbols) PARAMS ((bfd *, boolean, PTR *,
                                      unsigned int *));
#define bfd_minisymbol_to_symbol(b, d, m, f) \
  BFD_SEND (b, _minisymbol_to_symbol, (b, d, m, f))
  asymbol *(*_minisymbol_to_symbol) PARAMS ((bfd *, boolean, const PTR,
                                             asymbol *));

   /* Routines for relocs.  */
#define BFD_JUMP_TABLE_RELOCS(NAME)\
CAT(NAME,_get_reloc_upper_bound),\
CAT(NAME,_canonicalize_reloc),\
CAT(NAME,_bfd_reloc_type_lookup)
  long  (*_get_reloc_upper_bound) PARAMS ((bfd *, sec_ptr));
  long  (*_bfd_canonicalize_reloc) PARAMS ((bfd *, sec_ptr, arelent **,
                                            struct symbol_cache_entry **));
   /* See documentation on reloc types.  */
  reloc_howto_type *
       (*reloc_type_lookup) PARAMS ((bfd *abfd,
                                     bfd_reloc_code_real_type code));

   /* Routines used when writing an object file.  */
#define BFD_JUMP_TABLE_WRITE(NAME)\
CAT(NAME,_set_arch_mach),\
CAT(NAME,_set_section_contents)
  boolean    (*_bfd_set_arch_mach) PARAMS ((bfd *, enum bfd_architecture,
                    unsigned long));
  boolean       (*_bfd_set_section_contents) PARAMS ((bfd *, sec_ptr, PTR,
                                            file_ptr, bfd_size_type));

   /* Routines used by the linker.  */
#define BFD_JUMP_TABLE_LINK(NAME)\
CAT(NAME,_sizeof_headers),\
CAT(NAME,_bfd_get_relocated_section_contents),\
CAT(NAME,_bfd_relax_section),\
CAT(NAME,_bfd_link_hash_table_create),\
CAT(NAME,_bfd_link_add_symbols),\
CAT(NAME,_bfd_final_link),\
CAT(NAME,_bfd_link_split_section)
  int        (*_bfd_sizeof_headers) PARAMS ((bfd *, boolean));
  bfd_byte * (*_bfd_get_relocated_section_contents) PARAMS ((bfd *,
                    struct bfd_link_info *, struct bfd_link_order *,
                    bfd_byte *data, boolean relocateable,
                    struct symbol_cache_entry **));

  boolean    (*_bfd_relax_section) PARAMS ((bfd *, struct sec *,
                    struct bfd_link_info *, boolean *again));

   /* Create a hash table for the linker.  Different backends store
     different information in this table.  */
  struct bfd_link_hash_table *(*_bfd_link_hash_table_create) PARAMS ((bfd *));

   /* Add symbols from this object file into the hash table.  */
  boolean (*_bfd_link_add_symbols) PARAMS ((bfd *, struct bfd_link_info *));

   /* Do a link based on the link_order structures attached to each
     section of the BFD.  */
  boolean (*_bfd_final_link) PARAMS ((bfd *, struct bfd_link_info *));

   /* Should this section be split up into smaller pieces during linking.  */
  boolean (*_bfd_link_split_section) PARAMS ((bfd *, struct sec *));

  /* Routines to handle dynamic symbols and relocs.  */
#define BFD_JUMP_TABLE_DYNAMIC(NAME)\
CAT(NAME,_get_dynamic_symtab_upper_bound),\
CAT(NAME,_canonicalize_dynamic_symtab),\
CAT(NAME,_get_dynamic_reloc_upper_bound),\
CAT(NAME,_canonicalize_dynamic_reloc)
   /* Get the amount of memory required to hold the dynamic symbols. */
  long  (*_bfd_get_dynamic_symtab_upper_bound) PARAMS ((bfd *));
   /* Read in the dynamic symbols.  */
  long  (*_bfd_canonicalize_dynamic_symtab)
    PARAMS ((bfd *, struct symbol_cache_entry **));
   /* Get the amount of memory required to hold the dynamic relocs.  */
  long  (*_bfd_get_dynamic_reloc_upper_bound) PARAMS ((bfd *));
   /* Read in the dynamic relocs.  */
  long  (*_bfd_canonicalize_dynamic_reloc)
    PARAMS ((bfd *, arelent **, struct symbol_cache_entry **));

 PTR backend_data;
} bfd_target;
boolean 
bfd_set_default_target  PARAMS ((const char *name));

const bfd_target *
bfd_find_target PARAMS ((CONST char *target_name, bfd *abfd));

const char **
bfd_target_list PARAMS ((void));

boolean 
bfd_check_format PARAMS ((bfd *abfd, bfd_format format));

boolean 
bfd_check_format_matches PARAMS ((bfd *abfd, bfd_format format, char ***matching));

boolean 
bfd_set_format PARAMS ((bfd *abfd, bfd_format format));

CONST char *
bfd_format_string PARAMS ((bfd_format format));

#ifdef __cplusplus
}
#endif
#endif
