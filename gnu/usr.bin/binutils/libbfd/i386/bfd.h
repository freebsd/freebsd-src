/* $FreeBSD$ */

/* DO NOT EDIT!  -*- buffer-read-only: t -*-  This file is automatically 
   generated from "bfd-in.h", "init.c", "opncls.c", "libbfd.c", 
   "section.c", "archures.c", "reloc.c", "syms.c", "bfd.c", "archive.c", 
   "corefile.c", "targets.c" and "format.c".
   Run "make headers" in your build bfd/ to regenerate.  */

/* Main header file for the bfd library -- portable access to object files.
   Copyright 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999,
   2000, 2001
   Free Software Foundation, Inc.
   Contributed by Cygnus Support.

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

#ifndef __BFD_H_SEEN__
#define __BFD_H_SEEN__

#ifdef __cplusplus
extern "C" {
#endif

/* FreeBSD does not adhere to the Intel386 System V ABI.  */
#define ELF_DYNAMIC_INTERPRETER "/usr/libexec/ld-elf.so.1"

#include "ansidecl.h"
#include "symcat.h"
#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#ifndef SABER
/* This hack is to avoid a problem with some strict ANSI C preprocessors.
   The problem is, "32_" is not a valid preprocessing token, and we don't
   want extra underscores (e.g., "nlm_32_").  The XCONCAT2 macro will
   cause the inner CONCAT2 macros to be evaluated first, producing
   still-valid pp-tokens.  Then the final concatenation can be done.  */
#undef CONCAT4
#define CONCAT4(a,b,c,d) XCONCAT2(CONCAT2(a,b),CONCAT2(c,d))
#endif
#endif

/* #define BFD_VERSION 211930000 */
#define BFD_VERSION_DATE 20020126
#define BFD_VERSION_STRING "2.11.93 20020126"

/* The word size used by BFD on the host.  This may be 64 with a 32
   bit target if the host is 64 bit, or if other 64 bit targets have
   been selected with --enable-targets, or if --enable-64-bit-bfd.  */
#define BFD_ARCH_SIZE 32

/* The word size of the default bfd target.  */
#define BFD_DEFAULT_TARGET_SIZE 32

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
   force me to change it.  */
/* typedef enum boolean {false, true} boolean; */
/* Yup, SVR4 has a "typedef enum boolean" in <sys/types.h>  -fnf */
/* It gets worse if the host also defines a true/false enum... -sts */
/* And even worse if your compiler has built-in boolean types... -law */
#if defined (__GNUG__) && (__GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 6))
#define TRUE_FALSE_ALREADY_DEFINED
#endif
#ifdef MPW
/* Pre-emptive strike - get the file with the enum.  */
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
#define fprintf_vma(s,x) fprintf (s, "%08lx", x)
#define sprintf_vma(s,x) sprintf (s, "%08lx", x)

#endif /* not BFD64  */

/* A pointer to a position in a file.  */
/* FIXME:  This should be using off_t from <sys/types.h>.
   For now, try to avoid breaking stuff by not including <sys/types.h> here.
   This will break on systems with 64-bit file offsets (e.g. 4.4BSD).
   Probably the best long-term answer is to avoid using file_ptr AND off_t
   in this header file, and to handle this in the BFD implementation
   rather than in its interface.  */
/* typedef off_t	file_ptr; */
typedef bfd_signed_vma file_ptr;
typedef bfd_vma ufile_ptr;

extern void bfd_sprintf_vma PARAMS ((bfd *, char *, bfd_vma));
extern void bfd_fprintf_vma PARAMS ((bfd *, PTR, bfd_vma));

#define printf_vma(x) fprintf_vma(stdout,x)
#define bfd_printf_vma(abfd,x) bfd_fprintf_vma (abfd,stdout,x)

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
  union {
    file_ptr pos;
    bfd *abfd;
  } u;				/* bfd* or file position */
  int namidx;			/* index into string table */
};

/* Linenumber stuff */
typedef struct lineno_cache_entry {
  unsigned int line_number;	/* Linenumber from start of function*/
  union {
    struct symbol_cache_entry *sym; /* Function name */
    bfd_vma offset;	    /* Offset into section */
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
  const char *name;            /* Symbol name.  */
  unsigned char stab_type;     /* Stab type.  */
  char stab_other;             /* Stab other.  */
  short stab_desc;             /* Stab desc.  */
  const char *stab_name;       /* String for stab type.  */
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

#define COFF_SWAP_TABLE (PTR) &bfd_coff_std_swap_table

/* User program access to BFD facilities */

/* Direct I/O routines, for programs which know more about the object
   file than BFD does.  Use higher level routines if possible.  */

extern bfd_size_type bfd_bread PARAMS ((PTR, bfd_size_type, bfd *));
extern bfd_size_type bfd_bwrite PARAMS ((const PTR, bfd_size_type, bfd *));
extern int bfd_seek PARAMS ((bfd *, file_ptr, int));
extern ufile_ptr bfd_tell PARAMS ((bfd *));
extern int bfd_flush PARAMS ((bfd *));
extern int bfd_stat PARAMS ((bfd *, struct stat *));

/* Deprecated old routines.  */
#if __GNUC__
#define bfd_read(BUF, ELTSIZE, NITEMS, ABFD)				\
  (warn_deprecated ("bfd_read", __FILE__, __LINE__, __FUNCTION__),	\
   bfd_bread ((BUF), (ELTSIZE) * (NITEMS), (ABFD)))
#define bfd_write(BUF, ELTSIZE, NITEMS, ABFD)				\
  (warn_deprecated ("bfd_write", __FILE__, __LINE__, __FUNCTION__),	\
   bfd_bwrite ((BUF), (ELTSIZE) * (NITEMS), (ABFD)))
#else
#define bfd_read(BUF, ELTSIZE, NITEMS, ABFD)				\
  (warn_deprecated ("bfd_read", (const char *) 0, 0, (const char *) 0), \
   bfd_bread ((BUF), (ELTSIZE) * (NITEMS), (ABFD)))
#define bfd_write(BUF, ELTSIZE, NITEMS, ABFD)				\
  (warn_deprecated ("bfd_write", (const char *) 0, 0, (const char *) 0),\
   bfd_bwrite ((BUF), (ELTSIZE) * (NITEMS), (ABFD)))
#endif
extern void warn_deprecated
  PARAMS ((const char *, const char *, int, const char *));

/* Cast from const char * to char * so that caller can assign to
   a char * without a warning.  */
#define bfd_get_filename(abfd) ((char *) (abfd)->filename)
#define bfd_get_cacheable(abfd) ((abfd)->cacheable)
#define bfd_get_format(abfd) ((abfd)->format)
#define bfd_get_target(abfd) ((abfd)->xvec->name)
#define bfd_get_flavour(abfd) ((abfd)->xvec->flavour)
#define bfd_family_coff(abfd) \
  (bfd_get_flavour (abfd) == bfd_target_coff_flavour || \
   bfd_get_flavour (abfd) == bfd_target_xcoff_flavour)
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

#define bfd_set_cacheable(abfd,bool) (((abfd)->cacheable = (boolean) (bool)), true)

extern boolean bfd_cache_close PARAMS ((bfd *abfd));
/* NB: This declaration should match the autogenerated one in libbfd.h.  */

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

/* Byte swapping routines which take size and endiannes as arguments.  */

bfd_vma         bfd_get_bits       PARAMS ((bfd_byte *, int, boolean));
void            bfd_put_bits       PARAMS ((bfd_vma, bfd_byte *, int, boolean));

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
  PARAMS ((bfd *, const char *, const char *, const char *,
	   const char * const *, struct bfd_link_info *, struct sec **,
	   struct bfd_elf_version_tree *));
extern boolean bfd_elf64_size_dynamic_sections
  PARAMS ((bfd *, const char *, const char *, const char *,
	   const char * const *, struct bfd_link_info *, struct sec **,
	   struct bfd_elf_version_tree *));
extern void bfd_elf_set_dt_needed_name PARAMS ((bfd *, const char *));
extern void bfd_elf_set_dt_needed_soname PARAMS ((bfd *, const char *));
extern const char *bfd_elf_get_dt_soname PARAMS ((bfd *));
extern struct bfd_link_needed_list *bfd_elf_get_runpath_list
  PARAMS ((bfd *, struct bfd_link_info *));
extern boolean bfd_elf32_discard_info
  PARAMS ((bfd *, struct bfd_link_info *));
extern boolean bfd_elf64_discard_info
  PARAMS ((bfd *, struct bfd_link_info *));

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

/* Return the arch_size field of an elf bfd, or -1 if not elf.  */
extern int bfd_get_arch_size PARAMS ((bfd *));

/* Return true if address "naturally" sign extends, or -1 if not elf.  */
extern int bfd_get_sign_extend_vma PARAMS ((bfd *));

extern boolean bfd_m68k_elf32_create_embedded_relocs
  PARAMS ((bfd *, struct bfd_link_info *, struct sec *, struct sec *,
	   char **));

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
	   bfd_vma, const char *, const char *, const char *, unsigned int));
extern boolean bfd_xcoff_export_symbol
  PARAMS ((bfd *, struct bfd_link_info *, struct bfd_link_hash_entry *));
extern boolean bfd_xcoff_link_count_reloc
  PARAMS ((bfd *, struct bfd_link_info *, const char *));
extern boolean bfd_xcoff_record_link_assignment
  PARAMS ((bfd *, struct bfd_link_info *, const char *));
extern boolean bfd_xcoff_size_dynamic_sections
  PARAMS ((bfd *, struct bfd_link_info *, const char *, const char *,
	   unsigned long, unsigned long, unsigned long, boolean,
	   int, boolean, boolean, struct sec **));
extern boolean bfd_xcoff_link_generate_rtinit
  PARAMS ((bfd *, const char *, const char *));

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

extern boolean bfd_m68k_coff_create_embedded_relocs
  PARAMS ((bfd *, struct bfd_link_info *, struct sec *, struct sec *,
	   char **));

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

/* TI COFF load page support.  */
extern void bfd_ticoff_set_section_load_page
  PARAMS ((struct sec *, int));

extern int bfd_ticoff_get_section_load_page
  PARAMS ((struct sec *));

/* And more from the source.  */
void
bfd_init PARAMS ((void));

bfd *
bfd_openr PARAMS ((const char *filename, const char *target));

bfd *
bfd_fdopenr PARAMS ((const char *filename, const char *target, int fd));

bfd *
bfd_openstreamr PARAMS ((const char *, const char *, PTR));

bfd *
bfd_openw PARAMS ((const char *filename, const char *target));

boolean
bfd_close PARAMS ((bfd *abfd));

boolean
bfd_close_all_done PARAMS ((bfd *));

bfd *
bfd_create PARAMS ((const char *filename, bfd *templ));

boolean
bfd_make_writable PARAMS ((bfd *abfd));

boolean
bfd_make_readable PARAMS ((bfd *abfd));


/* Byte swapping macros for user section data.  */

#define bfd_put_8(abfd, val, ptr) \
                ((void) (*((unsigned char *) (ptr)) = (unsigned char) (val)))
#define bfd_put_signed_8 \
               bfd_put_8
#define bfd_get_8(abfd, ptr) \
                (*(unsigned char *) (ptr) & 0xff)
#define bfd_get_signed_8(abfd, ptr) \
               (((*(unsigned char *) (ptr) & 0xff) ^ 0x80) - 0x80)

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

#define bfd_get(bits, abfd, ptr)                               \
                ( (bits) ==  8 ? (bfd_vma) bfd_get_8 (abfd, ptr)       \
                : (bits) == 16 ? bfd_get_16 (abfd, ptr)        \
                : (bits) == 32 ? bfd_get_32 (abfd, ptr)        \
                : (bits) == 64 ? bfd_get_64 (abfd, ptr)        \
                : (abort (), (bfd_vma) - 1))

#define bfd_put(bits, abfd, val, ptr)                          \
                ( (bits) ==  8 ? bfd_put_8  (abfd, val, ptr)   \
                : (bits) == 16 ? bfd_put_16 (abfd, val, ptr)   \
                : (bits) == 32 ? bfd_put_32 (abfd, val, ptr)   \
                : (bits) == 64 ? bfd_put_64 (abfd, val, ptr)   \
                : (abort (), (void) 0))


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
  BFD_SEND (abfd, bfd_h_putx16, (val, ptr))
#define bfd_h_put_signed_16 \
  bfd_h_put_16
#define bfd_h_get_16(abfd, ptr) \
  BFD_SEND (abfd, bfd_h_getx16, (ptr))
#define bfd_h_get_signed_16(abfd, ptr) \
  BFD_SEND (abfd, bfd_h_getx_signed_16, (ptr))

#define bfd_h_put_32(abfd, val, ptr) \
  BFD_SEND (abfd, bfd_h_putx32, (val, ptr))
#define bfd_h_put_signed_32 \
  bfd_h_put_32
#define bfd_h_get_32(abfd, ptr) \
  BFD_SEND (abfd, bfd_h_getx32, (ptr))
#define bfd_h_get_signed_32(abfd, ptr) \
  BFD_SEND (abfd, bfd_h_getx_signed_32, (ptr))

#define bfd_h_put_64(abfd, val, ptr) \
  BFD_SEND (abfd, bfd_h_putx64, (val, ptr))
#define bfd_h_put_signed_64 \
  bfd_h_put_64
#define bfd_h_get_64(abfd, ptr) \
  BFD_SEND (abfd, bfd_h_getx64, (ptr))
#define bfd_h_get_signed_64(abfd, ptr) \
  BFD_SEND (abfd, bfd_h_getx_signed_64, (ptr))

/* Refinements on the above, which should eventually go away.  Save
   cluttering the source with (bfd_vma) and (bfd_byte *) casts.  */

#define H_PUT_64(abfd, val, where) \
  bfd_h_put_64 ((abfd), (bfd_vma) (val), (bfd_byte *) (where))

#define H_PUT_32(abfd, val, where) \
  bfd_h_put_32 ((abfd), (bfd_vma) (val), (bfd_byte *) (where))

#define H_PUT_16(abfd, val, where) \
  bfd_h_put_16 ((abfd), (bfd_vma) (val), (bfd_byte *) (where))

#define H_PUT_8 bfd_h_put_8

#define H_PUT_S64(abfd, val, where) \
  bfd_h_put_signed_64 ((abfd), (bfd_vma) (val), (bfd_byte *) (where))

#define H_PUT_S32(abfd, val, where) \
  bfd_h_put_signed_32 ((abfd), (bfd_vma) (val), (bfd_byte *) (where))

#define H_PUT_S16(abfd, val, where) \
  bfd_h_put_signed_16 ((abfd), (bfd_vma) (val), (bfd_byte *) (where))

#define H_PUT_S8 bfd_h_put_signed_8

#define H_GET_64(abfd, where) \
  bfd_h_get_64 ((abfd), (bfd_byte *) (where))

#define H_GET_32(abfd, where) \
  bfd_h_get_32 ((abfd), (bfd_byte *) (where))

#define H_GET_16(abfd, where) \
  bfd_h_get_16 ((abfd), (bfd_byte *) (where))

#define H_GET_8 bfd_h_get_8

#define H_GET_S64(abfd, where) \
  bfd_h_get_signed_64 ((abfd), (bfd_byte *) (where))

#define H_GET_S32(abfd, where) \
  bfd_h_get_signed_32 ((abfd), (bfd_byte *) (where))

#define H_GET_S16(abfd, where) \
  bfd_h_get_signed_16 ((abfd), (bfd_byte *) (where))

#define H_GET_S8 bfd_h_get_signed_8


/* This structure is used for a comdat section, as in PE.  A comdat
   section is associated with a particular symbol.  When the linker
   sees a comdat section, it keeps only one of the sections with a
   given name and associated with a given symbol.  */

struct bfd_comdat_info
{
  /* The name of the symbol associated with a comdat section.  */
  const char *name;

  /* The local symbol table index of the symbol associated with a
     comdat section.  This is only meaningful to the object file format
     specific code; it is not an index into the list returned by
     bfd_canonicalize_symtab.  */
  long symbol;
};

typedef struct sec
{
  /* The name of the section; the name isn't a copy, the pointer is
     the same as that passed to bfd_make_section.  */

  const char *name;

  /* A unique sequence number.  */

  int id;

  /* Which section in the bfd; 0..n-1 as sections are created in a bfd.  */

  int index;

  /* The next section in the list belonging to the BFD, or NULL.  */

  struct sec *next;

  /* The field flags contains attributes of the section. Some
     flags are read in from the object file, and some are
     synthesized from other information.  */

  flagword flags;

#define SEC_NO_FLAGS   0x000

  /* Tells the OS to allocate space for this section when loading.
     This is clear for a section containing debug information only.  */
#define SEC_ALLOC      0x001

  /* Tells the OS to load the section from the file when loading.
     This is clear for a .bss section.  */
#define SEC_LOAD       0x002

  /* The section contains data still to be relocated, so there is
     some relocation information too.  */
#define SEC_RELOC      0x004

  /* ELF reserves 4 processor specific bits and 8 operating system
     specific bits in sh_flags; at present we can get away with just
     one in communicating between the assembler and BFD, but this
     isn't a good long-term solution.  */
#define SEC_ARCH_BIT_0 0x008

  /* A signal to the OS that the section contains read only data.  */
#define SEC_READONLY   0x010

  /* The section contains code only.  */
#define SEC_CODE       0x020

  /* The section contains data only.  */
#define SEC_DATA       0x040

  /* The section will reside in ROM.  */
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
     standard data.  */
#define SEC_CONSTRUCTOR 0x100

  /* The section is a constructor, and should be placed at the
     end of the text, data, or bss section(?).  */
#define SEC_CONSTRUCTOR_TEXT 0x1100
#define SEC_CONSTRUCTOR_DATA 0x2100
#define SEC_CONSTRUCTOR_BSS  0x3100

  /* The section has contents - a data section could be
     <<SEC_ALLOC>> | <<SEC_HAS_CONTENTS>>; a debug section could be
     <<SEC_HAS_CONTENTS>>  */
#define SEC_HAS_CONTENTS 0x200

  /* An instruction to the linker to not output the section
     even if it has information which would normally be written.  */
#define SEC_NEVER_LOAD 0x400

  /* The section is a COFF shared library section.  This flag is
     only for the linker.  If this type of section appears in
     the input file, the linker must copy it to the output file
     without changing the vma or size.  FIXME: Although this
     was originally intended to be general, it really is COFF
     specific (and the flag was renamed to indicate this).  It
     might be cleaner to have some more general mechanism to
     allow the back end to control what the linker does with
     sections.  */
#define SEC_COFF_SHARED_LIBRARY 0x800

  /* The section has GOT references.  This flag is only for the
     linker, and is currently only used by the elf32-hppa back end.
     It will be set if global offset table references were detected
     in this section, which indicate to the linker that the section
     contains PIC code, and must be handled specially when doing a
     static link.  */
#define SEC_HAS_GOT_REF 0x4000

  /* The section contains common symbols (symbols may be defined
     multiple times, the value of a symbol is the amount of
     space it requires, and the largest symbol value is the one
     used).  Most targets have exactly one of these (which we
     translate to bfd_com_section_ptr), but ECOFF has two.  */
#define SEC_IS_COMMON 0x8000

  /* The section contains only debugging information.  For
     example, this is set for ELF .debug and .stab sections.
     strip tests this flag to see if a section can be
     discarded.  */
#define SEC_DEBUGGING 0x10000

  /* The contents of this section are held in memory pointed to
     by the contents field.  This is checked by bfd_get_section_contents,
     and the data is retrieved from memory if appropriate.  */
#define SEC_IN_MEMORY 0x20000

  /* The contents of this section are to be excluded by the
     linker for executable and shared objects unless those
     objects are to be further relocated.  */
#define SEC_EXCLUDE 0x40000

  /* The contents of this section are to be sorted based on the sum of
     the symbol and addend values specified by the associated relocation
     entries.  Entries without associated relocation entries will be
     appended to the end of the section in an unspecified order.  */
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
     sections with the same name should simply be discarded.  */
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

  /* This section should not be subject to garbage collection.  */
#define SEC_KEEP 0x1000000

  /* This section contains "short" data, and should be placed
     "near" the GP.  */
#define SEC_SMALL_DATA 0x2000000

  /* This section contains data which may be shared with other
     executables or shared objects.  */
#define SEC_SHARED 0x4000000

  /* When a section with this flag is being linked, then if the size of
     the input section is less than a page, it should not cross a page
     boundary.  If the size of the input section is one page or more, it
     should be aligned on a page boundary.  */
#define SEC_BLOCK 0x8000000

  /* Conditionally link this section; do not link if there are no
     references found to any symbol in the section.  */
#define SEC_CLINK 0x10000000

  /* Attempt to merge identical entities in the section.
     Entity size is given in the entsize field.  */
#define SEC_MERGE 0x20000000

  /* If given with SEC_MERGE, entities to merge are zero terminated
     strings where entsize specifies character size instead of fixed
     size entries.  */
#define SEC_STRINGS 0x40000000

  /* This section contains data about section groups.  */
#define SEC_GROUP 0x80000000

  /*  End of section flags.  */

  /* Some internal packed boolean fields.  */

  /* See the vma field.  */
  unsigned int user_set_vma : 1;

  /* Whether relocations have been processed.  */
  unsigned int reloc_done : 1;

  /* A mark flag used by some of the linker backends.  */
  unsigned int linker_mark : 1;

  /* Another mark flag used by some of the linker backends.  Set for
     output sections that have an input section.  */
  unsigned int linker_has_input : 1;

  /* A mark flag used by some linker backends for garbage collection.  */
  unsigned int gc_mark : 1;

  /* Used by the ELF code to mark sections which have been allocated
     to segments.  */
  unsigned int segment_mark : 1;

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

  /* The size of the section in octets, as it will be output.
     Contains a value even if the section has no contents (e.g., the
     size of <<.bss>>).  This will be filled in after relocation.  */

  bfd_size_type _cooked_size;

  /* The original size on disk of the section, in octets.  Normally this
     value is the same as the size, but if some relaxing has
     been done, then this value will be bigger.  */

  bfd_size_type _raw_size;

  /* If this section is going to be output, then this value is the
     offset in *bytes* into the output section of the first byte in the
     input section (byte ==> smallest addressable unit on the
     target).  In most cases, if this was going to start at the
     100th octet (8-bit quantity) in the output section, this value
     would be 100.  However, if the target byte size is 16 bits
     (bfd_octets_per_byte is "2"), this value would be 50.  */

  bfd_vma output_offset;

  /* The output section through which to map on output.  */

  struct sec *output_section;

  /* The alignment requirement of the section, as an exponent of 2 -
     e.g., 3 aligns to 2^3 (or 8).  */

  unsigned int alignment_power;

  /* If an input section, a pointer to a vector of relocation
     records for the data in this section.  */

  struct reloc_cache_entry *relocation;

  /* If an output section, a pointer to a vector of pointers to
     relocation records for the data in this section.  */

  struct reloc_cache_entry **orelocation;

  /* The number of relocation records in one of the above  */

  unsigned reloc_count;

  /* Information below is back end specific - and not always used
     or updated.  */

  /* File position of section data.  */

  file_ptr filepos;

  /* File position of relocation info.  */

  file_ptr rel_filepos;

  /* File position of line data.  */

  file_ptr line_filepos;

  /* Pointer to data for applications.  */

  PTR userdata;

  /* If the SEC_IN_MEMORY flag is set, this points to the actual
     contents.  */
  unsigned char *contents;

  /* Attached line number information.  */

  alent *lineno;

  /* Number of line number records.  */

  unsigned int lineno_count;

  /* Entity size for merging purposes.  */

  unsigned int entsize;

  /* Optional information about a COMDAT entry; NULL if not COMDAT.  */

  struct bfd_comdat_info *comdat;

  /* When a section is being output, this value changes as more
     linenumbers are written out.  */

  file_ptr moving_line_filepos;

  /* What the section number is in the target world.  */

  int target_index;

  PTR used_by_bfd;

  /* If this is a constructor section then here is a list of the
     relocations created to relocate items within it.  */

  struct relent_chain *constructor_chain;

  /* The BFD which owns the section.  */

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

#define bfd_is_const_section(SEC)              \
 (   ((SEC) == bfd_abs_section_ptr)            \
  || ((SEC) == bfd_und_section_ptr)            \
  || ((SEC) == bfd_com_section_ptr)            \
  || ((SEC) == bfd_ind_section_ptr))

extern const struct symbol_cache_entry * const bfd_abs_symbol;
extern const struct symbol_cache_entry * const bfd_com_symbol;
extern const struct symbol_cache_entry * const bfd_und_symbol;
extern const struct symbol_cache_entry * const bfd_ind_symbol;
#define bfd_get_section_size_before_reloc(section) \
     ((section)->reloc_done ? (abort (), (bfd_size_type) 1) \
                            : (section)->_raw_size)
#define bfd_get_section_size_after_reloc(section) \
     ((section)->reloc_done ? (section)->_cooked_size \
                            : (abort (), (bfd_size_type) 1))

/* Macros to handle insertion and deletion of a bfd's sections.  These
   only handle the list pointers, ie. do not adjust section_count,
   target_index etc.  */
#define bfd_section_list_remove(ABFD, PS) \
  do                                                   \
    {                                                  \
      asection **_ps = PS;                             \
      asection *_s = *_ps;                             \
      *_ps = _s->next;                                 \
      if (_s->next == NULL)                            \
        (ABFD)->section_tail = _ps;                    \
    }                                                  \
  while (0)
#define bfd_section_list_insert(ABFD, PS, S) \
  do                                                   \
    {                                                  \
      asection **_ps = PS;                             \
      asection *_s = S;                                \
      _s->next = *_ps;                                 \
      *_ps = _s;                                       \
      if (_s->next == NULL)                            \
        (ABFD)->section_tail = &_s->next;              \
    }                                                  \
  while (0)

void
bfd_section_list_clear PARAMS ((bfd *));

asection *
bfd_get_section_by_name PARAMS ((bfd *abfd, const char *name));

char *
bfd_get_unique_section_name PARAMS ((bfd *abfd,
    const char *templat,
    int *count));

asection *
bfd_make_section_old_way PARAMS ((bfd *abfd, const char *name));

asection *
bfd_make_section_anyway PARAMS ((bfd *abfd, const char *name));

asection *
bfd_make_section PARAMS ((bfd *, const char *name));

boolean
bfd_set_section_flags PARAMS ((bfd *abfd, asection *sec, flagword flags));

void
bfd_map_over_sections PARAMS ((bfd *abfd,
    void (*func) (bfd *abfd,
    asection *sect,
    PTR obj),
    PTR obj));

boolean
bfd_set_section_size PARAMS ((bfd *abfd, asection *sec, bfd_size_type val));

boolean
bfd_set_section_contents PARAMS ((bfd *abfd, asection *section,
    PTR data, file_ptr offset,
    bfd_size_type count));

boolean
bfd_get_section_contents PARAMS ((bfd *abfd, asection *section,
    PTR location, file_ptr offset,
    bfd_size_type count));

boolean
bfd_copy_private_section_data PARAMS ((bfd *ibfd, asection *isec,
    bfd *obfd, asection *osec));

#define bfd_copy_private_section_data(ibfd, isection, obfd, osection) \
     BFD_SEND (obfd, _bfd_copy_private_section_data, \
               (ibfd, isection, obfd, osection))
void
_bfd_strip_section_from_output PARAMS ((struct bfd_link_info *info, asection *section));

enum bfd_architecture
{
  bfd_arch_unknown,   /* File arch not known */
  bfd_arch_obscure,   /* Arch known, not one of these */
  bfd_arch_m68k,      /* Motorola 68xxx */
#define bfd_mach_m68000 1
#define bfd_mach_m68008 2
#define bfd_mach_m68010 3
#define bfd_mach_m68020 4
#define bfd_mach_m68030 5
#define bfd_mach_m68040 6
#define bfd_mach_m68060 7
#define bfd_mach_cpu32  8
#define bfd_mach_mcf5200  9
#define bfd_mach_mcf5206e 10
#define bfd_mach_mcf5307  11
#define bfd_mach_mcf5407  12
  bfd_arch_vax,       /* DEC Vax */
  bfd_arch_i960,      /* Intel 960 */
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

  bfd_arch_a29k,      /* AMD 29000 */
  bfd_arch_sparc,     /* SPARC */
#define bfd_mach_sparc                 1
/* The difference between v8plus and v9 is that v9 is a true 64 bit env.  */
#define bfd_mach_sparc_sparclet        2
#define bfd_mach_sparc_sparclite       3
#define bfd_mach_sparc_v8plus          4
#define bfd_mach_sparc_v8plusa         5 /* with ultrasparc add'ns */
#define bfd_mach_sparc_sparclite_le    6
#define bfd_mach_sparc_v9              7
#define bfd_mach_sparc_v9a             8 /* with ultrasparc add'ns */
#define bfd_mach_sparc_v8plusb         9 /* with cheetah add'ns */
#define bfd_mach_sparc_v9b             10 /* with cheetah add'ns */
/* Nonzero if MACH has the v9 instruction set.  */
#define bfd_mach_sparc_v9_p(mach) \
  ((mach) >= bfd_mach_sparc_v8plus && (mach) <= bfd_mach_sparc_v9b \
   && (mach) != bfd_mach_sparc_sparclite_le)
  bfd_arch_mips,      /* MIPS Rxxxx */
#define bfd_mach_mips3000              3000
#define bfd_mach_mips3900              3900
#define bfd_mach_mips4000              4000
#define bfd_mach_mips4010              4010
#define bfd_mach_mips4100              4100
#define bfd_mach_mips4111              4111
#define bfd_mach_mips4300              4300
#define bfd_mach_mips4400              4400
#define bfd_mach_mips4600              4600
#define bfd_mach_mips4650              4650
#define bfd_mach_mips5000              5000
#define bfd_mach_mips6000              6000
#define bfd_mach_mips8000              8000
#define bfd_mach_mips10000             10000
#define bfd_mach_mips12000             12000
#define bfd_mach_mips16                16
#define bfd_mach_mips5                 5
#define bfd_mach_mips_sb1              12310201 /* octal 'SB', 01 */
#define bfd_mach_mipsisa32             32
#define bfd_mach_mipsisa64             64
  bfd_arch_i386,      /* Intel 386 */
#define bfd_mach_i386_i386 0
#define bfd_mach_i386_i8086 1
#define bfd_mach_i386_i386_intel_syntax 2
#define bfd_mach_x86_64 3
#define bfd_mach_x86_64_intel_syntax 4
  bfd_arch_we32k,     /* AT&T WE32xxx */
  bfd_arch_tahoe,     /* CCI/Harris Tahoe */
  bfd_arch_i860,      /* Intel 860 */
  bfd_arch_i370,      /* IBM 360/370 Mainframes */
  bfd_arch_romp,      /* IBM ROMP PC/RT */
  bfd_arch_alliant,   /* Alliant */
  bfd_arch_convex,    /* Convex */
  bfd_arch_m88k,      /* Motorola 88xxx */
  bfd_arch_pyramid,   /* Pyramid Technology */
  bfd_arch_h8300,     /* Hitachi H8/300 */
#define bfd_mach_h8300   1
#define bfd_mach_h8300h  2
#define bfd_mach_h8300s  3
  bfd_arch_pdp11,     /* DEC PDP-11 */
  bfd_arch_powerpc,   /* PowerPC */
#define bfd_mach_ppc           0
#define bfd_mach_ppc_403       403
#define bfd_mach_ppc_403gc     4030
#define bfd_mach_ppc_505       505
#define bfd_mach_ppc_601       601
#define bfd_mach_ppc_602       602
#define bfd_mach_ppc_603       603
#define bfd_mach_ppc_ec603e    6031
#define bfd_mach_ppc_604       604
#define bfd_mach_ppc_620       620
#define bfd_mach_ppc_630       630
#define bfd_mach_ppc_750       750
#define bfd_mach_ppc_860       860
#define bfd_mach_ppc_a35       35
#define bfd_mach_ppc_rs64ii    642
#define bfd_mach_ppc_rs64iii   643
#define bfd_mach_ppc_7400      7400
  bfd_arch_rs6000,    /* IBM RS/6000 */
#define bfd_mach_rs6k          0
#define bfd_mach_rs6k_rs1      6001
#define bfd_mach_rs6k_rsc      6003
#define bfd_mach_rs6k_rs2      6002
  bfd_arch_hppa,      /* HP PA RISC */
  bfd_arch_d10v,      /* Mitsubishi D10V */
#define bfd_mach_d10v          0
#define bfd_mach_d10v_ts2      2
#define bfd_mach_d10v_ts3      3
  bfd_arch_d30v,      /* Mitsubishi D30V */
  bfd_arch_m68hc11,   /* Motorola 68HC11 */
  bfd_arch_m68hc12,   /* Motorola 68HC12 */
  bfd_arch_z8k,       /* Zilog Z8000 */
#define bfd_mach_z8001         1
#define bfd_mach_z8002         2
  bfd_arch_h8500,     /* Hitachi H8/500 */
  bfd_arch_sh,        /* Hitachi SH */
#define bfd_mach_sh            0
#define bfd_mach_sh2        0x20
#define bfd_mach_sh_dsp     0x2d
#define bfd_mach_sh3        0x30
#define bfd_mach_sh3_dsp    0x3d
#define bfd_mach_sh3e       0x3e
#define bfd_mach_sh4        0x40
  bfd_arch_alpha,     /* Dec Alpha */
#define bfd_mach_alpha_ev4  0x10
#define bfd_mach_alpha_ev5  0x20
#define bfd_mach_alpha_ev6  0x30
  bfd_arch_arm,       /* Advanced Risc Machines ARM */
#define bfd_mach_arm_2         1
#define bfd_mach_arm_2a        2
#define bfd_mach_arm_3         3
#define bfd_mach_arm_3M        4
#define bfd_mach_arm_4         5
#define bfd_mach_arm_4T        6
#define bfd_mach_arm_5         7
#define bfd_mach_arm_5T        8
#define bfd_mach_arm_5TE       9
#define bfd_mach_arm_XScale    10
  bfd_arch_ns32k,     /* National Semiconductors ns32000 */
  bfd_arch_w65,       /* WDC 65816 */
  bfd_arch_tic30,     /* Texas Instruments TMS320C30 */
  bfd_arch_tic54x,    /* Texas Instruments TMS320C54X */
  bfd_arch_tic80,     /* TI TMS320c80 (MVP) */
  bfd_arch_v850,      /* NEC V850 */
#define bfd_mach_v850          0
#define bfd_mach_v850e         'E'
#define bfd_mach_v850ea        'A'
  bfd_arch_arc,       /* ARC Cores */
#define bfd_mach_arc_5         0
#define bfd_mach_arc_6         1
#define bfd_mach_arc_7         2
#define bfd_mach_arc_8         3
  bfd_arch_m32r,      /* Mitsubishi M32R/D */
#define bfd_mach_m32r          0 /* backwards compatibility */
#define bfd_mach_m32rx         'x'
  bfd_arch_mn10200,   /* Matsushita MN10200 */
  bfd_arch_mn10300,   /* Matsushita MN10300 */
#define bfd_mach_mn10300               300
#define bfd_mach_am33          330
  bfd_arch_fr30,
#define bfd_mach_fr30          0x46523330
  bfd_arch_mcore,
  bfd_arch_ia64,      /* HP/Intel ia64 */
#define bfd_mach_ia64_elf64    0
#define bfd_mach_ia64_elf32    1
  bfd_arch_pj,
  bfd_arch_avr,       /* Atmel AVR microcontrollers */
#define bfd_mach_avr1          1
#define bfd_mach_avr2          2
#define bfd_mach_avr3          3
#define bfd_mach_avr4          4
#define bfd_mach_avr5          5
  bfd_arch_cris,      /* Axis CRIS */
  bfd_arch_s390,      /* IBM s390 */
#define bfd_mach_s390_esa      0
#define bfd_mach_s390_esame    1
  bfd_arch_openrisc,  /* OpenRISC */
  bfd_arch_mmix,      /* Donald Knuth's educational processor */
  bfd_arch_xstormy16,
#define bfd_mach_xstormy16     0
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
  /* True if this is the default machine for the architecture.  */
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
bfd_lookup_arch PARAMS ((enum bfd_architecture
    arch,
    unsigned long machine));

const char *
bfd_printable_arch_mach PARAMS ((enum bfd_architecture arch, unsigned long machine));

unsigned int
bfd_octets_per_byte PARAMS ((bfd *abfd));

unsigned int
bfd_arch_mach_octets_per_byte PARAMS ((enum bfd_architecture arch,
    unsigned long machine));

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
      in this field.  For example, a PC relative word relocation
      in a coff environment has the type 023 - because that's
      what the outside world calls a R_PCRWORD reloc.  */
  unsigned int type;

  /*  The value the final relocation is shifted right by.  This drops
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
      data section of the addend.  The relocation function will
      subtract from the relocation value the address of the location
      being relocated.  */
  boolean pc_relative;

  /*  The bit position of the reloc value in the destination.
      The relocated value is left shifted by this amount.  */
  unsigned int bitpos;

  /* What type of overflow error should be checked for when
     relocating.  */
  enum complain_overflow complain_on_overflow;

  /* If this field is non null, then the supplied function is
     called rather than the normal function.  This allows really
     strange relocation methods to be accomodated (e.g., i960 callj
     instructions).  */
  bfd_reloc_status_type (*special_function)
    PARAMS ((bfd *, arelent *, struct symbol_cache_entry *, PTR, asection *,
             bfd *, char **));

  /* The textual name of the relocation type.  */
  char *name;

  /* Some formats record a relocation addend in the section contents
     rather than with the relocation.  For ELF formats this is the
     distinction between USE_REL and USE_RELA (though the code checks
     for USE_REL == 1/0).  The value of this field is TRUE if the
     addend is recorded with the section contents; when performing a
     partial link (ld -r) the section contents (the data) will be
     modified.  The value of this field is FALSE if addends are
     recorded with the relocation (in arelent.addend); when performing
     a partial link the relocation will be modified.
     All relocations for all ELF USE_RELA targets should set this field
     to FALSE (values of TRUE should be looked on with suspicion).
     However, the converse is not true: not all relocations of all ELF
     USE_REL targets set this field to TRUE.  Why this is so is peculiar
     to each particular target.  For relocs that aren't used in partial
     links (e.g. GOT stuff) it doesn't matter what this is set to.  */
  boolean partial_inplace;

  /* The src_mask selects which parts of the read in data
     are to be used in the relocation sum.  E.g., if this was an 8 bit
     byte of data which we read and relocated, this would be
     0x000000ff.  When we have relocs which have an addend, such as
     sun4 extended relocs, the value in the offset part of a
     relocating field is garbage so we never use it.  In this case
     the mask would be 0x00000000.  */
  bfd_vma src_mask;

  /* The dst_mask selects which parts of the instruction are replaced
     into the instruction.  In most cases src_mask == dst_mask,
     except in the above special case, where dst_mask would be
     0x000000ff, and src_mask would be 0x00000000.  */
  bfd_vma dst_mask;

  /* When some formats create PC relative instructions, they leave
     the value of the pc of the place being relocated in the offset
     slot of the instruction, so that a PC relative relocation can
     be made just by adding in an ordinary offset (e.g., sun3 a.out).
     Some formats leave the displacement part of an instruction
     empty (e.g., m88k bcs); this flag signals the fact.  */
  boolean pcrel_offset;
};
#define HOWTO(C, R, S, B, P, BI, O, SF, NAME, INPLACE, MASKSRC, MASKDST, PC) \
  { (unsigned) C, R, S, B, P, BI, O, SF, NAME, INPLACE, MASKSRC, MASKDST, PC }
#define NEWHOWTO(FUNCTION, NAME, SIZE, REL, IN) \
  HOWTO (0, 0, SIZE, 0, REL, 0, complain_overflow_dont, FUNCTION, \
         NAME, false, 0, 0, IN)

#define EMPTY_HOWTO(C) \
  HOWTO ((C), 0, 0, 0, false, 0, complain_overflow_dont, NULL, \
         NULL, false, 0, 0, false)

#define HOWTO_PREPARE(relocation, symbol)               \
  {                                                     \
    if (symbol != (asymbol *) NULL)                     \
      {                                                 \
        if (bfd_is_com_section (symbol->section))       \
          {                                             \
            relocation = 0;                             \
          }                                             \
        else                                            \
          {                                             \
            relocation = symbol->value;                 \
          }                                             \
      }                                                 \
  }
unsigned int
bfd_get_reloc_size PARAMS ((reloc_howto_type *));

typedef struct relent_chain
{
  arelent relent;
  struct relent_chain *next;
} arelent_chain;
bfd_reloc_status_type
bfd_check_overflow PARAMS ((enum complain_overflow how,
    unsigned int bitsize,
    unsigned int rightshift,
    unsigned int addrsize,
    bfd_vma relocation));

bfd_reloc_status_type
bfd_perform_relocation PARAMS ((bfd *abfd,
    arelent *reloc_entry,
    PTR data,
    asection *input_section,
    bfd *output_bfd,
    char **error_message));

bfd_reloc_status_type
bfd_install_relocation PARAMS ((bfd *abfd,
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
  BFD_RELOC_64_PLT_PCREL,
  BFD_RELOC_32_PLT_PCREL,
  BFD_RELOC_24_PLT_PCREL,
  BFD_RELOC_16_PLT_PCREL,
  BFD_RELOC_8_PLT_PCREL,
  BFD_RELOC_64_PLTOFF,
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
  BFD_RELOC_SPARC_UA16,
  BFD_RELOC_SPARC_UA32,
  BFD_RELOC_SPARC_UA64,

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
  BFD_RELOC_SPARC_PLT32,
  BFD_RELOC_SPARC_PLT64,
  BFD_RELOC_SPARC_HIX22,
  BFD_RELOC_SPARC_LOX10,
  BFD_RELOC_SPARC_H44,
  BFD_RELOC_SPARC_M44,
  BFD_RELOC_SPARC_L44,
  BFD_RELOC_SPARC_REGISTER,

/* SPARC little endian relocation */
  BFD_RELOC_SPARC_REV32,

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
3 - jsr (target of branch) */
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

/* The GPREL_HI/LO relocations together form a 32-bit offset from the
GP register. */
  BFD_RELOC_ALPHA_GPREL_HI16,
  BFD_RELOC_ALPHA_GPREL_LO16,

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

/* Relocation against a MIPS literal section. */
  BFD_RELOC_MIPS_LITERAL,

/* MIPS ELF relocations. */
  BFD_RELOC_MIPS_GOT16,
  BFD_RELOC_MIPS_CALL16,
  BFD_RELOC_MIPS_GOT_HI16,
  BFD_RELOC_MIPS_GOT_LO16,
  BFD_RELOC_MIPS_CALL_HI16,
  BFD_RELOC_MIPS_CALL_LO16,
  BFD_RELOC_MIPS_SUB,
  BFD_RELOC_MIPS_GOT_PAGE,
  BFD_RELOC_MIPS_GOT_OFST,
  BFD_RELOC_MIPS_GOT_DISP,
  BFD_RELOC_MIPS_SHIFT5,
  BFD_RELOC_MIPS_SHIFT6,
  BFD_RELOC_MIPS_INSERT_A,
  BFD_RELOC_MIPS_INSERT_B,
  BFD_RELOC_MIPS_DELETE,
  BFD_RELOC_MIPS_HIGHEST,
  BFD_RELOC_MIPS_HIGHER,
  BFD_RELOC_MIPS_SCN_DISP,
  BFD_RELOC_MIPS_REL16,
  BFD_RELOC_MIPS_RELGOT,
  BFD_RELOC_MIPS_JALR,


/* i386/elf relocations */
  BFD_RELOC_386_GOT32,
  BFD_RELOC_386_PLT32,
  BFD_RELOC_386_COPY,
  BFD_RELOC_386_GLOB_DAT,
  BFD_RELOC_386_JUMP_SLOT,
  BFD_RELOC_386_RELATIVE,
  BFD_RELOC_386_GOTOFF,
  BFD_RELOC_386_GOTPC,

/* x86-64/elf relocations */
  BFD_RELOC_X86_64_GOT32,
  BFD_RELOC_X86_64_PLT32,
  BFD_RELOC_X86_64_COPY,
  BFD_RELOC_X86_64_GLOB_DAT,
  BFD_RELOC_X86_64_JUMP_SLOT,
  BFD_RELOC_X86_64_RELATIVE,
  BFD_RELOC_X86_64_GOTPCREL,
  BFD_RELOC_X86_64_32S,

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

/* PDP11 relocations */
  BFD_RELOC_PDP11_DISP_8_PCREL,
  BFD_RELOC_PDP11_DISP_6_PCREL,

/* Picojava relocs.  Not all of these appear in object files. */
  BFD_RELOC_PJ_CODE_HI16,
  BFD_RELOC_PJ_CODE_LO16,
  BFD_RELOC_PJ_CODE_DIR16,
  BFD_RELOC_PJ_CODE_DIR32,
  BFD_RELOC_PJ_CODE_REL16,
  BFD_RELOC_PJ_CODE_REL32,

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
  BFD_RELOC_PPC64_HIGHER,
  BFD_RELOC_PPC64_HIGHER_S,
  BFD_RELOC_PPC64_HIGHEST,
  BFD_RELOC_PPC64_HIGHEST_S,
  BFD_RELOC_PPC64_TOC16_LO,
  BFD_RELOC_PPC64_TOC16_HI,
  BFD_RELOC_PPC64_TOC16_HA,
  BFD_RELOC_PPC64_TOC,
  BFD_RELOC_PPC64_PLTGOT16,
  BFD_RELOC_PPC64_PLTGOT16_LO,
  BFD_RELOC_PPC64_PLTGOT16_HI,
  BFD_RELOC_PPC64_PLTGOT16_HA,
  BFD_RELOC_PPC64_ADDR16_DS,
  BFD_RELOC_PPC64_ADDR16_LO_DS,
  BFD_RELOC_PPC64_GOT16_DS,
  BFD_RELOC_PPC64_GOT16_LO_DS,
  BFD_RELOC_PPC64_PLT16_LO_DS,
  BFD_RELOC_PPC64_SECTOFF_DS,
  BFD_RELOC_PPC64_SECTOFF_LO_DS,
  BFD_RELOC_PPC64_TOC16_DS,
  BFD_RELOC_PPC64_TOC16_LO_DS,
  BFD_RELOC_PPC64_PLTGOT16_DS,
  BFD_RELOC_PPC64_PLTGOT16_LO_DS,

/* IBM 370/390 relocations */
  BFD_RELOC_I370_D12,

/* The type of reloc used to build a contructor table - at the moment
probably a 32 bit wide absolute relocation, but the target can choose.
It generally does map to one of the other relocation types. */
  BFD_RELOC_CTOR,

/* ARM 26 bit pc-relative branch.  The lowest two bits must be zero and are
not stored in the instruction. */
  BFD_RELOC_ARM_PCREL_BRANCH,

/* ARM 26 bit pc-relative branch.  The lowest bit must be zero and is
not stored in the instruction.  The 2nd lowest bit comes from a 1 bit
field in the instruction. */
  BFD_RELOC_ARM_PCREL_BLX,

/* Thumb 22 bit pc-relative branch.  The lowest bit must be zero and is
not stored in the instruction.  The 2nd lowest bit comes from a 1 bit
field in the instruction. */
  BFD_RELOC_THUMB_PCREL_BLX,

/* These relocs are only used within the ARM assembler.  They are not
(at present) written to any object files. */
  BFD_RELOC_ARM_IMMEDIATE,
  BFD_RELOC_ARM_ADRL_IMMEDIATE,
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
  BFD_RELOC_ARM_GOT12,
  BFD_RELOC_ARM_GOT32,
  BFD_RELOC_ARM_JUMP_SLOT,
  BFD_RELOC_ARM_COPY,
  BFD_RELOC_ARM_GLOB_DAT,
  BFD_RELOC_ARM_PLT32,
  BFD_RELOC_ARM_RELATIVE,
  BFD_RELOC_ARM_GOTOFF,
  BFD_RELOC_ARM_GOTPC,

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
  BFD_RELOC_SH_LOOP_START,
  BFD_RELOC_SH_LOOP_END,
  BFD_RELOC_SH_COPY,
  BFD_RELOC_SH_GLOB_DAT,
  BFD_RELOC_SH_JMP_SLOT,
  BFD_RELOC_SH_RELATIVE,
  BFD_RELOC_SH_GOTPC,

/* Thumb 23-, 12- and 9-bit pc-relative branches.  The lowest bit must
be zero and is not stored in the instruction. */
  BFD_RELOC_THUMB_PCREL_BRANCH9,
  BFD_RELOC_THUMB_PCREL_BRANCH12,
  BFD_RELOC_THUMB_PCREL_BRANCH23,

/* ARC Cores relocs.
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

/* Mitsubishi D30V relocs.
This is a 6-bit absolute reloc. */
  BFD_RELOC_D30V_6,

/* This is a 6-bit pc-relative reloc with
the right 3 bits assumed to be 0. */
  BFD_RELOC_D30V_9_PCREL,

/* This is a 6-bit pc-relative reloc with
the right 3 bits assumed to be 0. Same
as the previous reloc but on the right side
of the container. */
  BFD_RELOC_D30V_9_PCREL_R,

/* This is a 12-bit absolute reloc with the
right 3 bitsassumed to be 0. */
  BFD_RELOC_D30V_15,

/* This is a 12-bit pc-relative reloc with
the right 3 bits assumed to be 0. */
  BFD_RELOC_D30V_15_PCREL,

/* This is a 12-bit pc-relative reloc with
the right 3 bits assumed to be 0. Same
as the previous reloc but on the right side
of the container. */
  BFD_RELOC_D30V_15_PCREL_R,

/* This is an 18-bit absolute reloc with
the right 3 bits assumed to be 0. */
  BFD_RELOC_D30V_21,

/* This is an 18-bit pc-relative reloc with
the right 3 bits assumed to be 0. */
  BFD_RELOC_D30V_21_PCREL,

/* This is an 18-bit pc-relative reloc with
the right 3 bits assumed to be 0. Same
as the previous reloc but on the right side
of the container. */
  BFD_RELOC_D30V_21_PCREL_R,

/* This is a 32-bit absolute reloc. */
  BFD_RELOC_D30V_32,

/* This is a 32-bit pc-relative reloc. */
  BFD_RELOC_D30V_32_PCREL,

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

/* This is a 5 bit offset (of which only 4 bits are used) from the tiny
data area pointer. */
  BFD_RELOC_V850_TDA_4_5_OFFSET,

/* This is a 4 bit offset from the tiny data area pointer. */
  BFD_RELOC_V850_TDA_4_4_OFFSET,

/* This is a 16 bit offset from the short data area pointer, with the
bits placed non-contigously in the instruction. */
  BFD_RELOC_V850_SDA_16_16_SPLIT_OFFSET,

/* This is a 16 bit offset from the zero data area pointer, with the
bits placed non-contigously in the instruction. */
  BFD_RELOC_V850_ZDA_16_16_SPLIT_OFFSET,

/* This is a 6 bit offset from the call table base pointer. */
  BFD_RELOC_V850_CALLT_6_7_OFFSET,

/* This is a 16 bit offset from the call table base pointer. */
  BFD_RELOC_V850_CALLT_16_16_OFFSET,


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

/* This is a 7bit reloc for the tms320c54x, where the least
significant 7 bits of a 16 bit word are placed into the least
significant 7 bits of the opcode. */
  BFD_RELOC_TIC54X_PARTLS7,

/* This is a 9bit DP reloc for the tms320c54x, where the most
significant 9 bits of a 16 bit word are placed into the least
significant 9 bits of the opcode. */
  BFD_RELOC_TIC54X_PARTMS9,

/* This is an extended address 23-bit reloc for the tms320c54x. */
  BFD_RELOC_TIC54X_23,

/* This is a 16-bit reloc for the tms320c54x, where the least
significant 16 bits of a 23-bit extended address are placed into
the opcode. */
  BFD_RELOC_TIC54X_16_OF_23,

/* This is a reloc for the tms320c54x, where the most
significant 7 bits of a 23-bit extended address are placed into
the opcode. */
  BFD_RELOC_TIC54X_MS7_OF_23,

/* This is a 48 bit reloc for the FR30 that stores 32 bits. */
  BFD_RELOC_FR30_48,

/* This is a 32 bit reloc for the FR30 that stores 20 bits split up into
two sections. */
  BFD_RELOC_FR30_20,

/* This is a 16 bit reloc for the FR30 that stores a 6 bit word offset in
4 bits. */
  BFD_RELOC_FR30_6_IN_4,

/* This is a 16 bit reloc for the FR30 that stores an 8 bit byte offset
into 8 bits. */
  BFD_RELOC_FR30_8_IN_8,

/* This is a 16 bit reloc for the FR30 that stores a 9 bit short offset
into 8 bits. */
  BFD_RELOC_FR30_9_IN_8,

/* This is a 16 bit reloc for the FR30 that stores a 10 bit word offset
into 8 bits. */
  BFD_RELOC_FR30_10_IN_8,

/* This is a 16 bit reloc for the FR30 that stores a 9 bit pc relative
short offset into 8 bits. */
  BFD_RELOC_FR30_9_PCREL,

/* This is a 16 bit reloc for the FR30 that stores a 12 bit pc relative
short offset into 11 bits. */
  BFD_RELOC_FR30_12_PCREL,

/* Motorola Mcore relocations. */
  BFD_RELOC_MCORE_PCREL_IMM8BY4,
  BFD_RELOC_MCORE_PCREL_IMM11BY2,
  BFD_RELOC_MCORE_PCREL_IMM4BY2,
  BFD_RELOC_MCORE_PCREL_32,
  BFD_RELOC_MCORE_PCREL_JSR_IMM11BY2,
  BFD_RELOC_MCORE_RVA,

/* These are relocations for the GETA instruction. */
  BFD_RELOC_MMIX_GETA,
  BFD_RELOC_MMIX_GETA_1,
  BFD_RELOC_MMIX_GETA_2,
  BFD_RELOC_MMIX_GETA_3,

/* These are relocations for a conditional branch instruction. */
  BFD_RELOC_MMIX_CBRANCH,
  BFD_RELOC_MMIX_CBRANCH_J,
  BFD_RELOC_MMIX_CBRANCH_1,
  BFD_RELOC_MMIX_CBRANCH_2,
  BFD_RELOC_MMIX_CBRANCH_3,

/* These are relocations for the PUSHJ instruction. */
  BFD_RELOC_MMIX_PUSHJ,
  BFD_RELOC_MMIX_PUSHJ_1,
  BFD_RELOC_MMIX_PUSHJ_2,
  BFD_RELOC_MMIX_PUSHJ_3,

/* These are relocations for the JMP instruction. */
  BFD_RELOC_MMIX_JMP,
  BFD_RELOC_MMIX_JMP_1,
  BFD_RELOC_MMIX_JMP_2,
  BFD_RELOC_MMIX_JMP_3,

/* This is a relocation for a relative address as in a GETA instruction or
a branch. */
  BFD_RELOC_MMIX_ADDR19,

/* This is a relocation for a relative address as in a JMP instruction. */
  BFD_RELOC_MMIX_ADDR27,

/* This is a relocation for an instruction field that may be a general
register or a value 0..255. */
  BFD_RELOC_MMIX_REG_OR_BYTE,

/* This is a relocation for an instruction field that may be a general
register. */
  BFD_RELOC_MMIX_REG,

/* This is a relocation for two instruction fields holding a register and
an offset, the equivalent of the relocation. */
  BFD_RELOC_MMIX_BASE_PLUS_OFFSET,

/* This relocation is an assertion that the expression is not allocated as
a global register.  It does not modify contents. */
  BFD_RELOC_MMIX_LOCAL,

/* This is a 16 bit reloc for the AVR that stores 8 bit pc relative
short offset into 7 bits. */
  BFD_RELOC_AVR_7_PCREL,

/* This is a 16 bit reloc for the AVR that stores 13 bit pc relative
short offset into 12 bits. */
  BFD_RELOC_AVR_13_PCREL,

/* This is a 16 bit reloc for the AVR that stores 17 bit value (usually
program memory address) into 16 bits. */
  BFD_RELOC_AVR_16_PM,

/* This is a 16 bit reloc for the AVR that stores 8 bit value (usually
data memory address) into 8 bit immediate value of LDI insn. */
  BFD_RELOC_AVR_LO8_LDI,

/* This is a 16 bit reloc for the AVR that stores 8 bit value (high 8 bit
of data memory address) into 8 bit immediate value of LDI insn. */
  BFD_RELOC_AVR_HI8_LDI,

/* This is a 16 bit reloc for the AVR that stores 8 bit value (most high 8 bit
of program memory address) into 8 bit immediate value of LDI insn. */
  BFD_RELOC_AVR_HH8_LDI,

/* This is a 16 bit reloc for the AVR that stores negated 8 bit value
(usually data memory address) into 8 bit immediate value of SUBI insn. */
  BFD_RELOC_AVR_LO8_LDI_NEG,

/* This is a 16 bit reloc for the AVR that stores negated 8 bit value
(high 8 bit of data memory address) into 8 bit immediate value of
SUBI insn. */
  BFD_RELOC_AVR_HI8_LDI_NEG,

/* This is a 16 bit reloc for the AVR that stores negated 8 bit value
(most high 8 bit of program memory address) into 8 bit immediate value
of LDI or SUBI insn. */
  BFD_RELOC_AVR_HH8_LDI_NEG,

/* This is a 16 bit reloc for the AVR that stores 8 bit value (usually
command address) into 8 bit immediate value of LDI insn. */
  BFD_RELOC_AVR_LO8_LDI_PM,

/* This is a 16 bit reloc for the AVR that stores 8 bit value (high 8 bit
of command address) into 8 bit immediate value of LDI insn. */
  BFD_RELOC_AVR_HI8_LDI_PM,

/* This is a 16 bit reloc for the AVR that stores 8 bit value (most high 8 bit
of command address) into 8 bit immediate value of LDI insn. */
  BFD_RELOC_AVR_HH8_LDI_PM,

/* This is a 16 bit reloc for the AVR that stores negated 8 bit value
(usually command address) into 8 bit immediate value of SUBI insn. */
  BFD_RELOC_AVR_LO8_LDI_PM_NEG,

/* This is a 16 bit reloc for the AVR that stores negated 8 bit value
(high 8 bit of 16 bit command address) into 8 bit immediate value
of SUBI insn. */
  BFD_RELOC_AVR_HI8_LDI_PM_NEG,

/* This is a 16 bit reloc for the AVR that stores negated 8 bit value
(high 6 bit of 22 bit command address) into 8 bit immediate
value of SUBI insn. */
  BFD_RELOC_AVR_HH8_LDI_PM_NEG,

/* This is a 32 bit reloc for the AVR that stores 23 bit value
into 22 bits. */
  BFD_RELOC_AVR_CALL,

/* Direct 12 bit. */
  BFD_RELOC_390_12,

/* 12 bit GOT offset. */
  BFD_RELOC_390_GOT12,

/* 32 bit PC relative PLT address. */
  BFD_RELOC_390_PLT32,

/* Copy symbol at runtime. */
  BFD_RELOC_390_COPY,

/* Create GOT entry. */
  BFD_RELOC_390_GLOB_DAT,

/* Create PLT entry. */
  BFD_RELOC_390_JMP_SLOT,

/* Adjust by program base. */
  BFD_RELOC_390_RELATIVE,

/* 32 bit PC relative offset to GOT. */
  BFD_RELOC_390_GOTPC,

/* 16 bit GOT offset. */
  BFD_RELOC_390_GOT16,

/* PC relative 16 bit shifted by 1. */
  BFD_RELOC_390_PC16DBL,

/* 16 bit PC rel. PLT shifted by 1. */
  BFD_RELOC_390_PLT16DBL,

/* PC relative 32 bit shifted by 1. */
  BFD_RELOC_390_PC32DBL,

/* 32 bit PC rel. PLT shifted by 1. */
  BFD_RELOC_390_PLT32DBL,

/* 32 bit PC rel. GOT shifted by 1. */
  BFD_RELOC_390_GOTPCDBL,

/* 64 bit GOT offset. */
  BFD_RELOC_390_GOT64,

/* 64 bit PC relative PLT address. */
  BFD_RELOC_390_PLT64,

/* 32 bit rel. offset to GOT entry. */
  BFD_RELOC_390_GOTENT,

/* These two relocations are used by the linker to determine which of
the entries in a C++ virtual function table are actually used.  When
the --gc-sections option is given, the linker will zero out the entries
that are not used, so that the code for those functions need not be
included in the output.

VTABLE_INHERIT is a zero-space relocation used to describe to the
linker the inheritence tree of a C++ virtual function table.  The
relocation's symbol should be the parent class' vtable, and the
relocation should be located at the child vtable.

VTABLE_ENTRY is a zero-space relocation that describes the use of a
virtual function table entry.  The reloc's symbol should refer to the
table of the class mentioned in the code.  Off of that base, an offset
describes the entry that is being used.  For Rela hosts, this offset
is stored in the reloc's addend.  For Rel hosts, we are forced to put
this offset in the reloc's section offset. */
  BFD_RELOC_VTABLE_INHERIT,
  BFD_RELOC_VTABLE_ENTRY,

/* Intel IA64 Relocations. */
  BFD_RELOC_IA64_IMM14,
  BFD_RELOC_IA64_IMM22,
  BFD_RELOC_IA64_IMM64,
  BFD_RELOC_IA64_DIR32MSB,
  BFD_RELOC_IA64_DIR32LSB,
  BFD_RELOC_IA64_DIR64MSB,
  BFD_RELOC_IA64_DIR64LSB,
  BFD_RELOC_IA64_GPREL22,
  BFD_RELOC_IA64_GPREL64I,
  BFD_RELOC_IA64_GPREL32MSB,
  BFD_RELOC_IA64_GPREL32LSB,
  BFD_RELOC_IA64_GPREL64MSB,
  BFD_RELOC_IA64_GPREL64LSB,
  BFD_RELOC_IA64_LTOFF22,
  BFD_RELOC_IA64_LTOFF64I,
  BFD_RELOC_IA64_PLTOFF22,
  BFD_RELOC_IA64_PLTOFF64I,
  BFD_RELOC_IA64_PLTOFF64MSB,
  BFD_RELOC_IA64_PLTOFF64LSB,
  BFD_RELOC_IA64_FPTR64I,
  BFD_RELOC_IA64_FPTR32MSB,
  BFD_RELOC_IA64_FPTR32LSB,
  BFD_RELOC_IA64_FPTR64MSB,
  BFD_RELOC_IA64_FPTR64LSB,
  BFD_RELOC_IA64_PCREL21B,
  BFD_RELOC_IA64_PCREL21BI,
  BFD_RELOC_IA64_PCREL21M,
  BFD_RELOC_IA64_PCREL21F,
  BFD_RELOC_IA64_PCREL22,
  BFD_RELOC_IA64_PCREL60B,
  BFD_RELOC_IA64_PCREL64I,
  BFD_RELOC_IA64_PCREL32MSB,
  BFD_RELOC_IA64_PCREL32LSB,
  BFD_RELOC_IA64_PCREL64MSB,
  BFD_RELOC_IA64_PCREL64LSB,
  BFD_RELOC_IA64_LTOFF_FPTR22,
  BFD_RELOC_IA64_LTOFF_FPTR64I,
  BFD_RELOC_IA64_LTOFF_FPTR32MSB,
  BFD_RELOC_IA64_LTOFF_FPTR32LSB,
  BFD_RELOC_IA64_LTOFF_FPTR64MSB,
  BFD_RELOC_IA64_LTOFF_FPTR64LSB,
  BFD_RELOC_IA64_SEGREL32MSB,
  BFD_RELOC_IA64_SEGREL32LSB,
  BFD_RELOC_IA64_SEGREL64MSB,
  BFD_RELOC_IA64_SEGREL64LSB,
  BFD_RELOC_IA64_SECREL32MSB,
  BFD_RELOC_IA64_SECREL32LSB,
  BFD_RELOC_IA64_SECREL64MSB,
  BFD_RELOC_IA64_SECREL64LSB,
  BFD_RELOC_IA64_REL32MSB,
  BFD_RELOC_IA64_REL32LSB,
  BFD_RELOC_IA64_REL64MSB,
  BFD_RELOC_IA64_REL64LSB,
  BFD_RELOC_IA64_LTV32MSB,
  BFD_RELOC_IA64_LTV32LSB,
  BFD_RELOC_IA64_LTV64MSB,
  BFD_RELOC_IA64_LTV64LSB,
  BFD_RELOC_IA64_IPLTMSB,
  BFD_RELOC_IA64_IPLTLSB,
  BFD_RELOC_IA64_COPY,
  BFD_RELOC_IA64_TPREL22,
  BFD_RELOC_IA64_TPREL64MSB,
  BFD_RELOC_IA64_TPREL64LSB,
  BFD_RELOC_IA64_LTOFF_TP22,
  BFD_RELOC_IA64_LTOFF22X,
  BFD_RELOC_IA64_LDXMOV,

/* Motorola 68HC11 reloc.
This is the 8 bits high part of an absolute address. */
  BFD_RELOC_M68HC11_HI8,

/* Motorola 68HC11 reloc.
This is the 8 bits low part of an absolute address. */
  BFD_RELOC_M68HC11_LO8,

/* Motorola 68HC11 reloc.
This is the 3 bits of a value. */
  BFD_RELOC_M68HC11_3B,

/* These relocs are only used within the CRIS assembler.  They are not
(at present) written to any object files. */
  BFD_RELOC_CRIS_BDISP8,
  BFD_RELOC_CRIS_UNSIGNED_5,
  BFD_RELOC_CRIS_SIGNED_6,
  BFD_RELOC_CRIS_UNSIGNED_6,
  BFD_RELOC_CRIS_UNSIGNED_4,

/* Relocs used in ELF shared libraries for CRIS. */
  BFD_RELOC_CRIS_COPY,
  BFD_RELOC_CRIS_GLOB_DAT,
  BFD_RELOC_CRIS_JUMP_SLOT,
  BFD_RELOC_CRIS_RELATIVE,

/* 32-bit offset to symbol-entry within GOT. */
  BFD_RELOC_CRIS_32_GOT,

/* 16-bit offset to symbol-entry within GOT. */
  BFD_RELOC_CRIS_16_GOT,

/* 32-bit offset to symbol-entry within GOT, with PLT handling. */
  BFD_RELOC_CRIS_32_GOTPLT,

/* 16-bit offset to symbol-entry within GOT, with PLT handling. */
  BFD_RELOC_CRIS_16_GOTPLT,

/* 32-bit offset to symbol, relative to GOT. */
  BFD_RELOC_CRIS_32_GOTREL,

/* 32-bit offset to symbol with PLT entry, relative to GOT. */
  BFD_RELOC_CRIS_32_PLT_GOTREL,

/* 32-bit offset to symbol with PLT entry, relative to this relocation. */
  BFD_RELOC_CRIS_32_PLT_PCREL,

/* Intel i860 Relocations. */
  BFD_RELOC_860_COPY,
  BFD_RELOC_860_GLOB_DAT,
  BFD_RELOC_860_JUMP_SLOT,
  BFD_RELOC_860_RELATIVE,
  BFD_RELOC_860_PC26,
  BFD_RELOC_860_PLT26,
  BFD_RELOC_860_PC16,
  BFD_RELOC_860_LOW0,
  BFD_RELOC_860_SPLIT0,
  BFD_RELOC_860_LOW1,
  BFD_RELOC_860_SPLIT1,
  BFD_RELOC_860_LOW2,
  BFD_RELOC_860_SPLIT2,
  BFD_RELOC_860_LOW3,
  BFD_RELOC_860_LOGOT0,
  BFD_RELOC_860_SPGOT0,
  BFD_RELOC_860_LOGOT1,
  BFD_RELOC_860_SPGOT1,
  BFD_RELOC_860_LOGOTOFF0,
  BFD_RELOC_860_SPGOTOFF0,
  BFD_RELOC_860_LOGOTOFF1,
  BFD_RELOC_860_SPGOTOFF1,
  BFD_RELOC_860_LOGOTOFF2,
  BFD_RELOC_860_LOGOTOFF3,
  BFD_RELOC_860_LOPC,
  BFD_RELOC_860_HIGHADJ,
  BFD_RELOC_860_HAGOT,
  BFD_RELOC_860_HAGOTOFF,
  BFD_RELOC_860_HAPC,
  BFD_RELOC_860_HIGH,
  BFD_RELOC_860_HIGOT,
  BFD_RELOC_860_HIGOTOFF,

/* OpenRISC Relocations. */
  BFD_RELOC_OPENRISC_ABS_26,
  BFD_RELOC_OPENRISC_REL_26,

/* H8 elf Relocations. */
  BFD_RELOC_H8_DIR16A8,
  BFD_RELOC_H8_DIR16R8,
  BFD_RELOC_H8_DIR24A8,
  BFD_RELOC_H8_DIR24R8,
  BFD_RELOC_H8_DIR32A16,

/* Sony Xstormy16 Relocations. */
  BFD_RELOC_XSTORMY16_REL_12,
  BFD_RELOC_XSTORMY16_24,
  BFD_RELOC_XSTORMY16_FPTR16,
  BFD_RELOC_UNUSED };
typedef enum bfd_reloc_code_real bfd_reloc_code_real_type;
reloc_howto_type *
bfd_reloc_type_lookup PARAMS ((bfd *abfd, bfd_reloc_code_real_type code));

const char *
bfd_get_reloc_code_name PARAMS ((bfd_reloc_code_real_type code));


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

  struct _bfd *the_bfd; /* Use bfd_asymbol_bfd(sym) to access this field. */

       /* The text of the symbol. The name is left alone, and not copied; the
          application may not alter it. */
  const char *name;

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
#define BSF_EXPORT     BSF_GLOBAL /* no real difference */

       /* A normal C symbol would be one of:
          <<BSF_LOCAL>>, <<BSF_FORT_COMM>>,  <<BSF_UNDEFINED>> or
          <<BSF_GLOBAL>> */

       /* The symbol is a debugging record. The value has an arbitary
          meaning, unless BSF_DEBUGGING_RELOC is also set.  */
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

       /* This symbol is a debugging symbol.  The value is the offset
          into the section of the data.  BSF_DEBUGGING should be set
          as well.  */
#define BSF_DEBUGGING_RELOC 0x20000

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
bfd_set_symtab PARAMS ((bfd *abfd, asymbol **location, unsigned int count));

void
bfd_print_symbol_vandf PARAMS ((bfd *abfd, PTR file, asymbol *symbol));

#define bfd_make_empty_symbol(abfd) \
     BFD_SEND (abfd, _bfd_make_empty_symbol, (abfd))
asymbol *
_bfd_generic_make_empty_symbol PARAMS ((bfd *));

#define bfd_make_debug_symbol(abfd,ptr,size) \
        BFD_SEND (abfd, _bfd_make_debug_symbol, (abfd, ptr, size))
int
bfd_decode_symclass PARAMS ((asymbol *symbol));

boolean
bfd_is_undefined_symclass PARAMS ((int symclass));

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
    const char *filename;

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

    ufile_ptr where;

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

    ufile_ptr origin;

    /* Remember when output has begun, to stop strange things
       from happening. */
    boolean output_has_begun;

    /* A hash table for section names. */
    struct bfd_hash_table section_htab;

    /* Pointer to linked list of sections. */
    struct sec *sections;

    /* The place where we add to the section list. */
    struct sec **section_tail;

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
    struct _bfd *my_archive;     /* The containing archive BFD.  */
    struct _bfd *next;           /* The next BFD in the archive.  */
    struct _bfd *archive_head;   /* The first BFD in the archive.  */
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
      struct mmo_data_struct *mmo_data;
      struct sun_core_struct *sun_core_data;
      struct sco5_core_struct *sco5_core_data;
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
  bfd_error_wrong_object_format,
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
bfd_get_error PARAMS ((void));

void
bfd_set_error PARAMS ((bfd_error_type error_tag));

const char *
bfd_errmsg PARAMS ((bfd_error_type error_tag));

void
bfd_perror PARAMS ((const char *message));

typedef void (*bfd_error_handler_type) PARAMS ((const char *, ...));

bfd_error_handler_type
bfd_set_error_handler PARAMS ((bfd_error_handler_type));

void
bfd_set_error_program_name PARAMS ((const char *));

bfd_error_handler_type
bfd_get_error_handler PARAMS ((void));

const char *
bfd_archive_filename PARAMS ((bfd *));

long
bfd_get_reloc_upper_bound PARAMS ((bfd *abfd, asection *sect));

long
bfd_canonicalize_reloc PARAMS ((bfd *abfd,
    asection *sec,
    arelent **loc,
    asymbol **syms));

void
bfd_set_reloc PARAMS ((bfd *abfd, asection *sec, arelent **rel, unsigned int count)
    
    );

boolean
bfd_set_file_flags PARAMS ((bfd *abfd, flagword flags));

int
bfd_get_arch_size PARAMS ((bfd *abfd));

int
bfd_get_sign_extend_vma PARAMS ((bfd *abfd));

boolean
bfd_set_start_address PARAMS ((bfd *abfd, bfd_vma vma));

long
bfd_get_mtime PARAMS ((bfd *abfd));

long
bfd_get_size PARAMS ((bfd *abfd));

unsigned int
bfd_get_gp_size PARAMS ((bfd *abfd));

void
bfd_set_gp_size PARAMS ((bfd *abfd, unsigned int i));

bfd_vma
bfd_scan_vma PARAMS ((const char *string, const char **end, int base));

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

#define bfd_gc_sections(abfd, link_info) \
       BFD_SEND (abfd, _bfd_gc_sections, (abfd, link_info))

#define bfd_merge_sections(abfd, link_info) \
       BFD_SEND (abfd, _bfd_merge_sections, (abfd, link_info))

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

boolean
bfd_alt_mach_code PARAMS ((bfd *abfd, int index));

symindex
bfd_get_next_mapent PARAMS ((bfd *abfd, symindex previous, carsym **sym));

boolean
bfd_set_archive_head PARAMS ((bfd *output, bfd *new_head));

bfd *
bfd_openr_next_archived_file PARAMS ((bfd *archive, bfd *previous));

const char *
bfd_core_file_failing_command PARAMS ((bfd *abfd));

int
bfd_core_file_failing_signal PARAMS ((bfd *abfd));

boolean
core_file_matches_executable_p PARAMS ((bfd *core_bfd, bfd *exec_bfd));

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
            (((bfd)->xvec->message[(int) ((bfd)->format)]) arglist)

#ifdef DEBUG_BFD_SEND
#undef BFD_SEND_FMT
#define BFD_SEND_FMT(bfd, message, arglist) \
  (((bfd) && (bfd)->xvec && (bfd)->xvec->message) ? \
   (((bfd)->xvec->message[(int) ((bfd)->format)]) arglist) : \
   (bfd_assert (__FILE__,__LINE__), NULL))
#endif
enum bfd_flavour {
  bfd_target_unknown_flavour,
  bfd_target_aout_flavour,
  bfd_target_coff_flavour,
  bfd_target_ecoff_flavour,
  bfd_target_xcoff_flavour,
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
  bfd_target_ovax_flavour,
  bfd_target_evax_flavour,
  bfd_target_mmo_flavour
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
  bfd_vma        (*bfd_getx64) PARAMS ((const bfd_byte *));
  bfd_signed_vma (*bfd_getx_signed_64) PARAMS ((const bfd_byte *));
  void           (*bfd_putx64) PARAMS ((bfd_vma, bfd_byte *));
  bfd_vma        (*bfd_getx32) PARAMS ((const bfd_byte *));
  bfd_signed_vma (*bfd_getx_signed_32) PARAMS ((const bfd_byte *));
  void           (*bfd_putx32) PARAMS ((bfd_vma, bfd_byte *));
  bfd_vma        (*bfd_getx16) PARAMS ((const bfd_byte *));
  bfd_signed_vma (*bfd_getx_signed_16) PARAMS ((const bfd_byte *));
  void           (*bfd_putx16) PARAMS ((bfd_vma, bfd_byte *));
  bfd_vma        (*bfd_h_getx64) PARAMS ((const bfd_byte *));
  bfd_signed_vma (*bfd_h_getx_signed_64) PARAMS ((const bfd_byte *));
  void           (*bfd_h_putx64) PARAMS ((bfd_vma, bfd_byte *));
  bfd_vma        (*bfd_h_getx32) PARAMS ((const bfd_byte *));
  bfd_signed_vma (*bfd_h_getx_signed_32) PARAMS ((const bfd_byte *));
  void           (*bfd_h_putx32) PARAMS ((bfd_vma, bfd_byte *));
  bfd_vma        (*bfd_h_getx16) PARAMS ((const bfd_byte *));
  bfd_signed_vma (*bfd_h_getx_signed_16) PARAMS ((const bfd_byte *));
  void           (*bfd_h_putx16) PARAMS ((bfd_vma, bfd_byte *));
  const struct bfd_target *(*_bfd_check_format[bfd_type_end]) PARAMS ((bfd *));
  boolean  (*_bfd_set_format[bfd_type_end]) PARAMS ((bfd *));
  boolean  (*_bfd_write_contents[bfd_type_end]) PARAMS ((bfd *));

  /* Generic entry points.  */
#define BFD_JUMP_TABLE_GENERIC(NAME) \
CONCAT2 (NAME,_close_and_cleanup), \
CONCAT2 (NAME,_bfd_free_cached_info), \
CONCAT2 (NAME,_new_section_hook), \
CONCAT2 (NAME,_get_section_contents), \
CONCAT2 (NAME,_get_section_contents_in_window)

  /* Called when the BFD is being closed to do any necessary cleanup.  */
  boolean  (*_close_and_cleanup) PARAMS ((bfd *));
  /* Ask the BFD to free all cached information.  */
  boolean  (*_bfd_free_cached_info) PARAMS ((bfd *));
  /* Called when a new section is created.  */
  boolean  (*_new_section_hook) PARAMS ((bfd *, sec_ptr));
  /* Read the contents of a section.  */
  boolean  (*_bfd_get_section_contents) PARAMS ((bfd *, sec_ptr, PTR,
                                                 file_ptr, bfd_size_type));
  boolean  (*_bfd_get_section_contents_in_window)
    PARAMS ((bfd *, sec_ptr, bfd_window *, file_ptr, bfd_size_type));

  /* Entry points to copy private data.  */
#define BFD_JUMP_TABLE_COPY(NAME) \
CONCAT2 (NAME,_bfd_copy_private_bfd_data), \
CONCAT2 (NAME,_bfd_merge_private_bfd_data), \
CONCAT2 (NAME,_bfd_copy_private_section_data), \
CONCAT2 (NAME,_bfd_copy_private_symbol_data), \
CONCAT2 (NAME,_bfd_set_private_flags), \
CONCAT2 (NAME,_bfd_print_private_bfd_data) \
  /* Called to copy BFD general private data from one object file
     to another.  */
  boolean  (*_bfd_copy_private_bfd_data) PARAMS ((bfd *, bfd *));
  /* Called to merge BFD general private data from one object file
     to a common output file when linking.  */
  boolean  (*_bfd_merge_private_bfd_data) PARAMS ((bfd *, bfd *));
  /* Called to copy BFD private section data from one object file
     to another.  */
  boolean  (*_bfd_copy_private_section_data) PARAMS ((bfd *, sec_ptr,
                                                      bfd *, sec_ptr));
  /* Called to copy BFD private symbol data from one symbol
     to another.  */
  boolean  (*_bfd_copy_private_symbol_data) PARAMS ((bfd *, asymbol *,
                                                     bfd *, asymbol *));
  /* Called to set private backend flags */
  boolean  (*_bfd_set_private_flags) PARAMS ((bfd *, flagword));

  /* Called to print private BFD data */
  boolean  (*_bfd_print_private_bfd_data) PARAMS ((bfd *, PTR));

  /* Core file entry points.  */
#define BFD_JUMP_TABLE_CORE(NAME) \
CONCAT2 (NAME,_core_file_failing_command), \
CONCAT2 (NAME,_core_file_failing_signal), \
CONCAT2 (NAME,_core_file_matches_executable_p)
  char *   (*_core_file_failing_command) PARAMS ((bfd *));
  int      (*_core_file_failing_signal) PARAMS ((bfd *));
  boolean  (*_core_file_matches_executable_p) PARAMS ((bfd *, bfd *));

  /* Archive entry points.  */
#define BFD_JUMP_TABLE_ARCHIVE(NAME) \
CONCAT2 (NAME,_slurp_armap), \
CONCAT2 (NAME,_slurp_extended_name_table), \
CONCAT2 (NAME,_construct_extended_name_table), \
CONCAT2 (NAME,_truncate_arname), \
CONCAT2 (NAME,_write_armap), \
CONCAT2 (NAME,_read_ar_hdr), \
CONCAT2 (NAME,_openr_next_archived_file), \
CONCAT2 (NAME,_get_elt_at_index), \
CONCAT2 (NAME,_generic_stat_arch_elt), \
CONCAT2 (NAME,_update_armap_timestamp)
  boolean  (*_bfd_slurp_armap) PARAMS ((bfd *));
  boolean  (*_bfd_slurp_extended_name_table) PARAMS ((bfd *));
  boolean  (*_bfd_construct_extended_name_table)
    PARAMS ((bfd *, char **, bfd_size_type *, const char **));
  void     (*_bfd_truncate_arname) PARAMS ((bfd *, const char *, char *));
  boolean  (*write_armap)
    PARAMS ((bfd *, unsigned int, struct orl *, unsigned int, int));
  PTR      (*_bfd_read_ar_hdr_fn) PARAMS ((bfd *));
  bfd *    (*openr_next_archived_file) PARAMS ((bfd *, bfd *));
#define bfd_get_elt_at_index(b,i) BFD_SEND(b, _bfd_get_elt_at_index, (b,i))
  bfd *    (*_bfd_get_elt_at_index) PARAMS ((bfd *, symindex));
  int      (*_bfd_stat_arch_elt) PARAMS ((bfd *, struct stat *));
  boolean  (*_bfd_update_armap_timestamp) PARAMS ((bfd *));

  /* Entry points used for symbols.  */
#define BFD_JUMP_TABLE_SYMBOLS(NAME) \
CONCAT2 (NAME,_get_symtab_upper_bound), \
CONCAT2 (NAME,_get_symtab), \
CONCAT2 (NAME,_make_empty_symbol), \
CONCAT2 (NAME,_print_symbol), \
CONCAT2 (NAME,_get_symbol_info), \
CONCAT2 (NAME,_bfd_is_local_label_name), \
CONCAT2 (NAME,_get_lineno), \
CONCAT2 (NAME,_find_nearest_line), \
CONCAT2 (NAME,_bfd_make_debug_symbol), \
CONCAT2 (NAME,_read_minisymbols), \
CONCAT2 (NAME,_minisymbol_to_symbol)
  long     (*_bfd_get_symtab_upper_bound) PARAMS ((bfd *));
  long     (*_bfd_canonicalize_symtab) PARAMS ((bfd *,
                                                struct symbol_cache_entry **));
  struct symbol_cache_entry *
           (*_bfd_make_empty_symbol) PARAMS ((bfd *));
  void     (*_bfd_print_symbol) PARAMS ((bfd *, PTR,
                                         struct symbol_cache_entry *,
                                         bfd_print_symbol_type));
#define bfd_print_symbol(b,p,s,e) BFD_SEND(b, _bfd_print_symbol, (b,p,s,e))
  void     (*_bfd_get_symbol_info) PARAMS ((bfd *,
                                            struct symbol_cache_entry *,
                                            symbol_info *));
#define bfd_get_symbol_info(b,p,e) BFD_SEND(b, _bfd_get_symbol_info, (b,p,e))
  boolean  (*_bfd_is_local_label_name) PARAMS ((bfd *, const char *));

  alent *  (*_get_lineno) PARAMS ((bfd *, struct symbol_cache_entry *));
  boolean  (*_bfd_find_nearest_line)
    PARAMS ((bfd *, struct sec *, struct symbol_cache_entry **, bfd_vma,
             const char **, const char **, unsigned int *));
 /* Back-door to allow format-aware applications to create debug symbols
    while using BFD for everything else.  Currently used by the assembler
    when creating COFF files.  */
  asymbol *(*_bfd_make_debug_symbol) PARAMS ((bfd *, void *,
                                              unsigned long size));
#define bfd_read_minisymbols(b, d, m, s) \
  BFD_SEND (b, _read_minisymbols, (b, d, m, s))
  long     (*_read_minisymbols) PARAMS ((bfd *, boolean, PTR *,
                                         unsigned int *));
#define bfd_minisymbol_to_symbol(b, d, m, f) \
  BFD_SEND (b, _minisymbol_to_symbol, (b, d, m, f))
  asymbol *(*_minisymbol_to_symbol) PARAMS ((bfd *, boolean, const PTR,
                                             asymbol *));

  /* Routines for relocs.  */
#define BFD_JUMP_TABLE_RELOCS(NAME) \
CONCAT2 (NAME,_get_reloc_upper_bound), \
CONCAT2 (NAME,_canonicalize_reloc), \
CONCAT2 (NAME,_bfd_reloc_type_lookup)
  long     (*_get_reloc_upper_bound) PARAMS ((bfd *, sec_ptr));
  long     (*_bfd_canonicalize_reloc) PARAMS ((bfd *, sec_ptr, arelent **,
                                               struct symbol_cache_entry **));
  /* See documentation on reloc types.  */
  reloc_howto_type *
           (*reloc_type_lookup) PARAMS ((bfd *, bfd_reloc_code_real_type));

  /* Routines used when writing an object file.  */
#define BFD_JUMP_TABLE_WRITE(NAME) \
CONCAT2 (NAME,_set_arch_mach), \
CONCAT2 (NAME,_set_section_contents)
  boolean  (*_bfd_set_arch_mach) PARAMS ((bfd *, enum bfd_architecture,
                                          unsigned long));
  boolean  (*_bfd_set_section_contents) PARAMS ((bfd *, sec_ptr, PTR,
                                                 file_ptr, bfd_size_type));

  /* Routines used by the linker.  */
#define BFD_JUMP_TABLE_LINK(NAME) \
CONCAT2 (NAME,_sizeof_headers), \
CONCAT2 (NAME,_bfd_get_relocated_section_contents), \
CONCAT2 (NAME,_bfd_relax_section), \
CONCAT2 (NAME,_bfd_link_hash_table_create), \
CONCAT2 (NAME,_bfd_link_add_symbols), \
CONCAT2 (NAME,_bfd_final_link), \
CONCAT2 (NAME,_bfd_link_split_section), \
CONCAT2 (NAME,_bfd_gc_sections), \
CONCAT2 (NAME,_bfd_merge_sections)
  int      (*_bfd_sizeof_headers) PARAMS ((bfd *, boolean));
  bfd_byte *(*_bfd_get_relocated_section_contents)
    PARAMS ((bfd *, struct bfd_link_info *, struct bfd_link_order *,
             bfd_byte *, boolean, struct symbol_cache_entry **));

  boolean  (*_bfd_relax_section)
    PARAMS ((bfd *, struct sec *, struct bfd_link_info *, boolean *));

  /* Create a hash table for the linker.  Different backends store
     different information in this table.  */
  struct bfd_link_hash_table *(*_bfd_link_hash_table_create) PARAMS ((bfd *));

  /* Add symbols from this object file into the hash table.  */
  boolean  (*_bfd_link_add_symbols) PARAMS ((bfd *, struct bfd_link_info *));

  /* Do a link based on the link_order structures attached to each
     section of the BFD.  */
  boolean  (*_bfd_final_link) PARAMS ((bfd *, struct bfd_link_info *));

  /* Should this section be split up into smaller pieces during linking.  */
  boolean  (*_bfd_link_split_section) PARAMS ((bfd *, struct sec *));

  /* Remove sections that are not referenced from the output.  */
  boolean  (*_bfd_gc_sections) PARAMS ((bfd *, struct bfd_link_info *));

  /* Attempt to merge SEC_MERGE sections.  */
  boolean  (*_bfd_merge_sections) PARAMS ((bfd *, struct bfd_link_info *));

  /* Routines to handle dynamic symbols and relocs.  */
#define BFD_JUMP_TABLE_DYNAMIC(NAME) \
CONCAT2 (NAME,_get_dynamic_symtab_upper_bound), \
CONCAT2 (NAME,_canonicalize_dynamic_symtab), \
CONCAT2 (NAME,_get_dynamic_reloc_upper_bound), \
CONCAT2 (NAME,_canonicalize_dynamic_reloc)
  /* Get the amount of memory required to hold the dynamic symbols. */
  long     (*_bfd_get_dynamic_symtab_upper_bound) PARAMS ((bfd *));
  /* Read in the dynamic symbols.  */
  long     (*_bfd_canonicalize_dynamic_symtab)
    PARAMS ((bfd *, struct symbol_cache_entry **));
  /* Get the amount of memory required to hold the dynamic relocs.  */
  long     (*_bfd_get_dynamic_reloc_upper_bound) PARAMS ((bfd *));
  /* Read in the dynamic relocs.  */
  long     (*_bfd_canonicalize_dynamic_reloc)
    PARAMS ((bfd *, arelent **, struct symbol_cache_entry **));

 /* Opposite endian version of this target.  */
 const struct bfd_target * alternative_target;

 PTR backend_data;

} bfd_target;
boolean
bfd_set_default_target PARAMS ((const char *name));

const bfd_target *
bfd_find_target PARAMS ((const char *target_name, bfd *abfd));

const char **
bfd_target_list PARAMS ((void));

const bfd_target *
bfd_search_for_target PARAMS ((int (* search_func) (const bfd_target *, void *), void *));

boolean
bfd_check_format PARAMS ((bfd *abfd, bfd_format format));

boolean
bfd_check_format_matches PARAMS ((bfd *abfd, bfd_format format, char ***matching));

boolean
bfd_set_format PARAMS ((bfd *abfd, bfd_format format));

const char *
bfd_format_string PARAMS ((bfd_format format));

#ifdef __cplusplus
}
#endif
#endif
