/* BFD back-end data structures for a.out (and similar) files.
   Copyright 1990, 1991, 1992 Free Software Foundation, Inc.
   Written by Cygnus Support.

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
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* We try to encapsulate the differences in the various a.out file
   variants in a few routines, and otherwise share large masses of code.
   This means we only have to fix bugs in one place, most of the time.  */

/* Parameterize the a.out code based on whether it is being built
   for a 32-bit architecture or a 64-bit architecture.  */
#if ARCH_SIZE==64
#define GET_WORD bfd_h_get_64
#define GET_SWORD bfd_h_get_signed_64
#define PUT_WORD bfd_h_put_64
#ifndef NAME
#define NAME(x,y) CAT3(x,_64_,y)
#endif
#define JNAME(x) CAT(x,_64)
#define BYTES_IN_WORD 8
#else /* ARCH_SIZE == 32 */
#define GET_WORD bfd_h_get_32
#define GET_SWORD bfd_h_get_signed_32
#define PUT_WORD bfd_h_put_32
#ifndef NAME
#define NAME(x,y) CAT3(x,_32_,y)
#endif
#define JNAME(x) CAT(x,_32)
#define BYTES_IN_WORD 4
#endif /* ARCH_SIZE==32 */

/* Declare these types at file level, since they are used in parameter
   lists, which have wierd scope.  */
struct external_exec;
struct internal_exec;

/* Back-end information for various a.out targets.  */
struct aout_backend_data
{
  /* Are ZMAGIC files mapped contiguously?  If so, the text section may
     need more padding, if the segment size (granularity for memory access
     control) is larger than the page size.  */
  unsigned char zmagic_mapped_contiguous;
  /* If this flag is set, ZMAGIC/NMAGIC file headers get mapped in with the
     text section, which starts immediately after the file header.
     If not, the text section starts on the next page.  */
  unsigned char text_includes_header;

  /* If the text section VMA isn't specified, and we need an absolute
     address, use this as the default.  If we're producing a relocatable
     file, zero is always used.  */
  /* ?? Perhaps a callback would be a better choice?  Will this do anything
     reasonable for a format that handles multiple CPUs with different
     load addresses for each?  */
  bfd_vma default_text_vma;

  /* Callback for setting the page and segment sizes, if they can't be
     trivially determined from the architecture.  */
  boolean (*set_sizes) PARAMS ((bfd *));

  /* zmagic files only. For go32, the length of the exec header contributes
     to the size of the text section in the file for alignment purposes but
     does *not* get counted in the length of the text section. */
  unsigned char exec_header_not_counted;
};
#define aout_backend_info(abfd) \
	((CONST struct aout_backend_data *)((abfd)->xvec->backend_data))

/* This is the layout in memory of a "struct exec" while we process it.
   All 'lengths' are given as a number of bytes.
   All 'alignments' are for relinkable files only;  an alignment of
	'n' indicates the corresponding segment must begin at an
	address that is a multiple of (2**n).  */

struct internal_exec
{
    long a_info;		/* Magic number and flags, packed */
    bfd_vma a_text;		/* length of text, in bytes  */
    bfd_vma a_data;		/* length of data, in bytes  */
    bfd_vma a_bss;		/* length of uninitialized data area in mem */
    bfd_vma a_syms;		/* length of symbol table data in file */
    bfd_vma a_entry;		/* start address */
    bfd_vma a_trsize;		/* length of text's relocation info, in bytes */
    bfd_vma a_drsize;		/* length of data's relocation info, in bytes */
    /* Added for i960 */
    bfd_vma a_tload;		/* Text runtime load address */
    bfd_vma a_dload;		/* Data runtime load address */
    unsigned char a_talign;	/* Alignment of text segment */
    unsigned char a_dalign;	/* Alignment of data segment */
    unsigned char a_balign;	/* Alignment of bss segment */
    char a_relaxable;           /* Enough info for linker relax */
};

/* Magic number is written 
< MSB        >
3130292827262524232221201918171615141312111009080706050403020100
< FLAGS      >< MACHINE TYPE ><  MAGIC NUMBER		       >
*/
/* Magic number for NetBSD is
<MSB         >
3130292827262524232221201918171615141312111009080706050403020100
< FLAGS    ><                  ><  MAGIC NUMBER                >
*/

enum machine_type {
  M_UNKNOWN = 0,
  M_68010 = 1,
  M_68020 = 2,
  M_SPARC = 3,
  /* skip a bunch so we don't run into any of suns numbers */
  M_386 = 100,
  M_29K = 101,          /* AMD 29000 */
  M_386_DYNIX = 102,	/* Sequent running dynix */
  M_386_NETBSD = 134,		/* NetBSD/386 binary */
  M_MIPS1 = 151,        /* MIPS R2000/R3000 binary */
  M_MIPS2 = 152,        /* MIPS R4000/R6000 binary */
  M_HP200 = 200,	/* HP 200 (68010) BSD binary */
  M_HP300 = (300 % 256), /* HP 300 (68020+68881) BSD binary */
  M_HPUX = (0x20c % 256)/* HP 200/300 HPUX binary */
};

#define N_DYNAMIC(exec) ((exec).a_info & 0x8000000)

#ifndef N_MAGIC
# define N_MAGIC(exec) ((exec).a_info & 0xffff)
#endif

#ifndef N_MACHTYPE
# define N_MACHTYPE(exec) ((enum machine_type)(((exec).a_info >> 16) & 0xff))
#endif

#ifndef N_FLAGS
# define N_FLAGS(exec) (((exec).a_info >> 24) & 0xff)
#endif

#ifndef N_SET_INFO
# define N_SET_INFO(exec, magic, type, flags) \
((exec).a_info = ((magic) & 0xffff) \
 | (((int)(type) & 0xff) << 16) \
 | (((flags) & 0xff) << 24))
#endif

#ifndef N_SET_MAGIC
# define N_SET_MAGIC(exec, magic) \
((exec).a_info = (((exec).a_info & 0xffff0000) | ((magic) & 0xffff)))
#endif

#ifndef N_SET_MACHTYPE
# define N_SET_MACHTYPE(exec, machtype) \
((exec).a_info = \
 ((exec).a_info&0xff00ffff) | ((((int)(machtype))&0xff) << 16))
#endif

#ifndef N_SET_FLAGS
# define N_SET_FLAGS(exec, flags) \
((exec).a_info = \
 ((exec).a_info&0x00ffffff) | (((flags) & 0xff) << 24))
#endif

typedef struct aout_symbol {
  asymbol symbol;
  short desc;
  char other;
  unsigned char type;
} aout_symbol_type;

/* The `tdata' struct for all a.out-like object file formats.
   Various things depend on this struct being around any time an a.out
   file is being handled.  An example is dbxread.c in GDB.  */

struct aoutdata {
  struct internal_exec *hdr;		/* exec file header */
  aout_symbol_type *symbols;		/* symtab for input bfd */
  
  /* For ease, we do this */
  asection *textsec;
  asection *datasec;
  asection *bsssec;

  /* We remember these offsets so that after check_file_format, we have
     no dependencies on the particular format of the exec_hdr.  */
  file_ptr sym_filepos;
  file_ptr str_filepos;

  /* Size of a relocation entry in external form */
  unsigned reloc_entry_size;

  /* Size of a symbol table entry in external form */
  unsigned symbol_entry_size;

  /* Page size - needed for alignment of demand paged files. */
  unsigned long page_size;

  /* Segment size - needed for alignment of demand paged files. */
  unsigned long segment_size;

  unsigned exec_bytes_size;
  unsigned vma_adjusted : 1;

  /* used when a bfd supports several highly similar formats */
  enum {
    default_format = 0,
    gnu_encap_format } subformat;

  enum {
    undecided_magic = 0,
    z_magic,
    o_magic,
    n_magic } magic;
};

struct  aout_data_struct {
    struct aoutdata a;
    struct internal_exec e;
};

#define	adata(bfd)		((bfd)->tdata.aout_data->a)
#define	exec_hdr(bfd)		(adata(bfd).hdr)
#define	obj_aout_symbols(bfd)	(adata(bfd).symbols)
#define	obj_textsec(bfd)	(adata(bfd).textsec)
#define	obj_datasec(bfd)	(adata(bfd).datasec)
#define	obj_bsssec(bfd)		(adata(bfd).bsssec)
#define	obj_sym_filepos(bfd)	(adata(bfd).sym_filepos)
#define	obj_str_filepos(bfd)	(adata(bfd).str_filepos)
#define	obj_reloc_entry_size(bfd) (adata(bfd).reloc_entry_size)
#define	obj_symbol_entry_size(bfd) (adata(bfd).symbol_entry_size)
#define obj_aout_subformat(bfd)	(adata(bfd).subformat)

/* We take the address of the first element of an asymbol to ensure that the
   macro is only ever applied to an asymbol */
#define aout_symbol(asymbol) ((aout_symbol_type *)(&(asymbol)->the_bfd))

/* Prototype declarations for functions defined in aoutx.h  */

boolean
NAME(aout,squirt_out_relocs) PARAMS ((bfd *abfd, asection *section));

bfd_target *
NAME(aout,some_aout_object_p) PARAMS ((bfd *abfd,
			struct internal_exec *execp,
			bfd_target * (*callback)(bfd *)));

boolean
NAME(aout,mkobject) PARAMS ((bfd *abfd));

enum machine_type
NAME(aout,machine_type) PARAMS ((enum bfd_architecture arch,
				 unsigned long machine));

boolean
NAME(aout,set_arch_mach) PARAMS ((bfd *abfd, enum bfd_architecture arch,
		 		  unsigned long machine));

boolean
NAME(aout,new_section_hook) PARAMS ((bfd *abfd, asection *newsect));

boolean
NAME(aout,set_section_contents) PARAMS ((bfd *abfd, sec_ptr section,
			 PTR location, file_ptr offset, bfd_size_type count));

asymbol *
NAME(aout,make_empty_symbol) PARAMS ((bfd *abfd));

boolean
NAME(aout,slurp_symbol_table) PARAMS ((bfd *abfd));

void
NAME(aout,write_syms) PARAMS ((bfd *abfd));

void
NAME(aout,reclaim_symbol_table) PARAMS ((bfd *abfd));

unsigned int
NAME(aout,get_symtab_upper_bound) PARAMS ((bfd *abfd));

unsigned int
NAME(aout,get_symtab) PARAMS ((bfd *abfd, asymbol **location));

boolean
NAME(aout,slurp_reloc_table) PARAMS ((bfd *abfd, sec_ptr asect,
				      asymbol **symbols));

unsigned int
NAME(aout,canonicalize_reloc) PARAMS ((bfd *abfd, sec_ptr section,
				       arelent **relptr, asymbol **symbols));

unsigned int
NAME(aout,get_reloc_upper_bound) PARAMS ((bfd *abfd, sec_ptr asect));

void
NAME(aout,reclaim_reloc) PARAMS ((bfd *ignore_abfd, sec_ptr ignore));

alent *
NAME(aout,get_lineno) PARAMS ((bfd *ignore_abfd, asymbol *ignore_symbol));

void
NAME(aout,print_symbol) PARAMS ((bfd *ignore_abfd, PTR file,
			    asymbol *symbol, bfd_print_symbol_type how));

void
NAME(aout,get_symbol_info) PARAMS ((bfd *ignore_abfd,
                           asymbol *symbol, symbol_info *ret));

boolean
NAME(aout,close_and_cleanup) PARAMS ((bfd *abfd));

boolean
NAME(aout,find_nearest_line) PARAMS ((bfd *abfd, asection *section,
      asymbol **symbols, bfd_vma offset, CONST char **filename_ptr,
      CONST char **functionname_ptr, unsigned int *line_ptr));

int
NAME(aout,sizeof_headers) PARAMS ((bfd *abfd, boolean exec));

boolean
NAME(aout,adjust_sizes_and_vmas) PARAMS ((bfd *abfd,
       bfd_size_type *text_size, file_ptr *text_end));

void
NAME(aout,swap_exec_header_in) PARAMS ((bfd *abfd,
       struct external_exec *raw_bytes, struct internal_exec *execp));

void
NAME(aout,swap_exec_header_out) PARAMS ((bfd *abfd,
       struct internal_exec *execp, struct external_exec *raw_bytes));

/* Prototypes for functions in stab-syms.c. */

CONST char *
aout_stab_name PARAMS ((int code));

/* A.out uses the generic versions of these routines... */

#define	aout_32_get_section_contents	bfd_generic_get_section_contents
#define	aout_32_close_and_cleanup	bfd_generic_close_and_cleanup

#define	aout_64_get_section_contents	bfd_generic_get_section_contents
#define	aout_64_close_and_cleanup	bfd_generic_close_and_cleanup
#ifndef NO_WRITE_HEADER_KLUDGE
#define NO_WRITE_HEADER_KLUDGE 0
#endif

#ifndef WRITE_HEADERS
#define WRITE_HEADERS(abfd, execp)					      \
      {									      \
	bfd_size_type text_size; /* dummy vars */			      \
	file_ptr text_end;						      \
	if (adata(abfd).magic == undecided_magic)			      \
	  NAME(aout,adjust_sizes_and_vmas) (abfd, &text_size, &text_end);     \
    									      \
	execp->a_syms = bfd_get_symcount (abfd) * EXTERNAL_NLIST_SIZE;	      \
	execp->a_entry = bfd_get_start_address (abfd);			      \
    									      \
	execp->a_trsize = ((obj_textsec (abfd)->reloc_count) *		      \
			   obj_reloc_entry_size (abfd));		      \
	execp->a_drsize = ((obj_datasec (abfd)->reloc_count) *		      \
			   obj_reloc_entry_size (abfd));		      \
	NAME(aout,swap_exec_header_out) (abfd, execp, &exec_bytes);	      \
									      \
	bfd_seek (abfd, (file_ptr) 0, SEEK_SET);			      \
	bfd_write ((PTR) &exec_bytes, 1, EXEC_BYTES_SIZE, abfd);	      \
	/* Now write out reloc info, followed by syms and strings */	      \
  									      \
	if (bfd_get_symcount (abfd) != 0) 				      \
	    {								      \
	      bfd_seek (abfd, (file_ptr)(N_SYMOFF(*execp)), SEEK_SET);	      \
									      \
	      NAME(aout,write_syms)(abfd);				      \
									      \
	      bfd_seek (abfd, (file_ptr)(N_TRELOFF(*execp)), SEEK_SET);	      \
									      \
	      if (!NAME(aout,squirt_out_relocs) (abfd, obj_textsec (abfd))) return false; \
	      bfd_seek (abfd, (file_ptr)(N_DRELOFF(*execp)), SEEK_SET);	      \
									      \
	      if (!NAME(aout,squirt_out_relocs)(abfd, obj_datasec (abfd))) return false; \
	    }								      \
      }									      
#endif
