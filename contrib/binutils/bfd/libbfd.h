/* libbfd.h -- Declarations used by bfd library *implementation*.
   (This include file is not for users of the library.)
   Copyright 1990, 91, 92, 93, 94, 95, 96, 97, 1998 Free Software Foundation, Inc.
   Written by Cygnus Support.

** NOTE: libbfd.h is a GENERATED file.  Don't change it; instead,
** change libbfd-in.h or the other BFD source files processed to
** generate this file.

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

/* Align an address upward to a boundary, expressed as a number of bytes.
   E.g. align to an 8-byte boundary with argument of 8.  Take care never
   to wrap around if the address is within boundary-1 of the end of the
   address space.  */
#define BFD_ALIGN(this, boundary)					\
  ((((bfd_vma) (this) + (boundary) - 1) >= (bfd_vma) (this))		\
   ? (((bfd_vma) (this) + ((boundary) - 1)) & (~((boundary)-1)))	\
   : ~ (bfd_vma) 0)

/* If you want to read and write large blocks, you might want to do it
   in quanta of this amount */
#define DEFAULT_BUFFERSIZE 8192

/* Set a tdata field.  Can't use the other macros for this, since they
   do casts, and casting to the left of assignment isn't portable.  */
#define set_tdata(bfd, v) ((bfd)->tdata.any = (PTR) (v))

/* If BFD_IN_MEMORY is set for a BFD, then the iostream fields points
   to an instance of this structure.  */

struct bfd_in_memory
{
  /* Size of buffer.  */
  bfd_size_type size;
  /* Buffer holding contents of BFD.  */
  bfd_byte *buffer;
};

/* tdata for an archive.  For an input archive, cache
   needs to be free()'d.  For an output archive, symdefs do.  */

struct artdata {
  file_ptr first_file_filepos;
  /* Speed up searching the armap */
  struct ar_cache *cache;
  bfd *archive_head;            /* Only interesting in output routines */
  carsym *symdefs;		/* the symdef entries */
  symindex symdef_count;             /* how many there are */
  char *extended_names;		/* clever intel extension */
  /* when more compilers are standard C, this can be a time_t */
  long  armap_timestamp;	/* Timestamp value written into armap.
				   This is used for BSD archives to check
				   that the timestamp is recent enough
				   for the BSD linker to not complain,
				   just before we finish writing an
				   archive.  */
  file_ptr armap_datepos;	/* Position within archive to seek to
				   rewrite the date field.  */
  PTR tdata;			/* Backend specific information.  */
};

#define bfd_ardata(bfd) ((bfd)->tdata.aout_ar_data)

/* Goes in bfd's arelt_data slot */
struct areltdata {
  char * arch_header;			     /* it's actually a string */
  unsigned int parsed_size;     /* octets of filesize not including ar_hdr */
  char *filename;			     /* null-terminated */
};

#define arelt_size(bfd) (((struct areltdata *)((bfd)->arelt_data))->parsed_size)

extern PTR bfd_malloc PARAMS ((size_t));
extern PTR bfd_realloc PARAMS ((PTR, size_t));
extern PTR bfd_zmalloc PARAMS ((size_t));

extern bfd_error_handler_type _bfd_error_handler;

/* These routines allocate and free things on the BFD's objalloc.  */

extern PTR bfd_alloc PARAMS ((bfd *, size_t));
extern PTR bfd_zalloc PARAMS ((bfd *, size_t));
extern void bfd_release PARAMS ((bfd *, PTR));

bfd *	_bfd_create_empty_archive_element_shell PARAMS ((bfd *obfd));
bfd *	_bfd_look_for_bfd_in_cache PARAMS ((bfd *arch_bfd, file_ptr index));
boolean _bfd_add_bfd_to_archive_cache PARAMS ((bfd *, file_ptr, bfd *));
boolean	_bfd_generic_mkarchive PARAMS ((bfd *abfd));
const bfd_target *bfd_generic_archive_p PARAMS ((bfd *abfd));
boolean	bfd_slurp_armap PARAMS ((bfd *abfd));
boolean bfd_slurp_bsd_armap_f2 PARAMS ((bfd *abfd));
#define bfd_slurp_bsd_armap bfd_slurp_armap
#define bfd_slurp_coff_armap bfd_slurp_armap
boolean	_bfd_slurp_extended_name_table PARAMS ((bfd *abfd));
extern boolean _bfd_construct_extended_name_table
  PARAMS ((bfd *, boolean, char **, bfd_size_type *));
boolean	_bfd_write_archive_contents PARAMS ((bfd *abfd));
boolean _bfd_compute_and_write_armap PARAMS ((bfd *, unsigned int elength));
bfd *_bfd_get_elt_at_filepos PARAMS ((bfd *archive, file_ptr filepos));
extern bfd *_bfd_generic_get_elt_at_index PARAMS ((bfd *, symindex));
bfd * _bfd_new_bfd PARAMS ((void));

boolean	bfd_false PARAMS ((bfd *ignore));
boolean	bfd_true PARAMS ((bfd *ignore));
PTR	bfd_nullvoidptr PARAMS ((bfd *ignore));
int	bfd_0 PARAMS ((bfd *ignore));
unsigned int	bfd_0u PARAMS ((bfd *ignore));
long	bfd_0l PARAMS ((bfd *ignore));
long	_bfd_n1 PARAMS ((bfd *ignore));
void	bfd_void PARAMS ((bfd *ignore));

bfd *_bfd_new_bfd_contained_in PARAMS ((bfd *));
const bfd_target *_bfd_dummy_target PARAMS ((bfd *abfd));

void	bfd_dont_truncate_arname PARAMS ((bfd *abfd, CONST char *filename,
					char *hdr));
void	bfd_bsd_truncate_arname PARAMS ((bfd *abfd, CONST char *filename,
					char *hdr));
void	bfd_gnu_truncate_arname PARAMS ((bfd *abfd, CONST char *filename,
					char *hdr));

boolean	bsd_write_armap PARAMS ((bfd *arch, unsigned int elength,
				  struct orl *map, unsigned int orl_count, int stridx));

boolean	coff_write_armap PARAMS ((bfd *arch, unsigned int elength,
				   struct orl *map, unsigned int orl_count, int stridx));

extern PTR _bfd_generic_read_ar_hdr PARAMS ((bfd *));

extern PTR _bfd_generic_read_ar_hdr_mag PARAMS ((bfd *, const char *));

bfd *	bfd_generic_openr_next_archived_file PARAMS ((bfd *archive,
						     bfd *last_file));

int	bfd_generic_stat_arch_elt PARAMS ((bfd *, struct stat *));

#define _bfd_read_ar_hdr(abfd) \
  BFD_SEND (abfd, _bfd_read_ar_hdr_fn, (abfd))

/* Generic routines to use for BFD_JUMP_TABLE_GENERIC.  Use
   BFD_JUMP_TABLE_GENERIC (_bfd_generic).  */

#define _bfd_generic_close_and_cleanup bfd_true
#define _bfd_generic_bfd_free_cached_info bfd_true
#define _bfd_generic_new_section_hook \
  ((boolean (*) PARAMS ((bfd *, asection *))) bfd_true)
extern boolean _bfd_generic_get_section_contents
  PARAMS ((bfd *, asection *, PTR location, file_ptr offset,
	   bfd_size_type count));
extern boolean _bfd_generic_get_section_contents_in_window
  PARAMS ((bfd *, asection *, bfd_window *, file_ptr, bfd_size_type));

/* Generic routines to use for BFD_JUMP_TABLE_COPY.  Use
   BFD_JUMP_TABLE_COPY (_bfd_generic).  */

#define _bfd_generic_bfd_copy_private_bfd_data \
  ((boolean (*) PARAMS ((bfd *, bfd *))) bfd_true)
#define _bfd_generic_bfd_merge_private_bfd_data \
  ((boolean (*) PARAMS ((bfd *, bfd *))) bfd_true)
#define _bfd_generic_bfd_set_private_flags \
  ((boolean (*) PARAMS ((bfd *, flagword))) bfd_true)
#define _bfd_generic_bfd_copy_private_section_data \
  ((boolean (*) PARAMS ((bfd *, asection *, bfd *, asection *))) bfd_true)
#define _bfd_generic_bfd_copy_private_symbol_data \
  ((boolean (*) PARAMS ((bfd *, asymbol *, bfd *, asymbol *))) bfd_true)
#define _bfd_generic_bfd_print_private_bfd_data \
  ((boolean (*) PARAMS ((bfd *, PTR))) bfd_true)

/* Routines to use for BFD_JUMP_TABLE_CORE when there is no core file
   support.  Use BFD_JUMP_TABLE_CORE (_bfd_nocore).  */

extern char *_bfd_nocore_core_file_failing_command PARAMS ((bfd *));
extern int _bfd_nocore_core_file_failing_signal PARAMS ((bfd *));
extern boolean _bfd_nocore_core_file_matches_executable_p
  PARAMS ((bfd *, bfd *));

/* Routines to use for BFD_JUMP_TABLE_ARCHIVE when there is no archive
   file support.  Use BFD_JUMP_TABLE_ARCHIVE (_bfd_noarchive).  */

#define _bfd_noarchive_slurp_armap bfd_false
#define _bfd_noarchive_slurp_extended_name_table bfd_false
#define _bfd_noarchive_construct_extended_name_table \
  ((boolean (*) PARAMS ((bfd *, char **, bfd_size_type *, const char **))) \
   bfd_false)
#define _bfd_noarchive_truncate_arname \
  ((void (*) PARAMS ((bfd *, const char *, char *))) bfd_void)
#define _bfd_noarchive_write_armap \
  ((boolean (*) \
    PARAMS ((bfd *, unsigned int, struct orl *, unsigned int, int))) \
   bfd_false)
#define _bfd_noarchive_read_ar_hdr bfd_nullvoidptr
#define _bfd_noarchive_openr_next_archived_file \
  ((bfd *(*) PARAMS ((bfd *, bfd *))) bfd_nullvoidptr)
#define _bfd_noarchive_get_elt_at_index \
  ((bfd *(*) PARAMS ((bfd *, symindex))) bfd_nullvoidptr)
#define _bfd_noarchive_generic_stat_arch_elt bfd_generic_stat_arch_elt
#define _bfd_noarchive_update_armap_timestamp bfd_false

/* Routines to use for BFD_JUMP_TABLE_ARCHIVE to get BSD style
   archives.  Use BFD_JUMP_TABLE_ARCHIVE (_bfd_archive_bsd).  */

#define _bfd_archive_bsd_slurp_armap bfd_slurp_bsd_armap
#define _bfd_archive_bsd_slurp_extended_name_table \
  _bfd_slurp_extended_name_table
extern boolean _bfd_archive_bsd_construct_extended_name_table
  PARAMS ((bfd *, char **, bfd_size_type *, const char **));
#define _bfd_archive_bsd_truncate_arname bfd_bsd_truncate_arname
#define _bfd_archive_bsd_write_armap bsd_write_armap
#define _bfd_archive_bsd_read_ar_hdr _bfd_generic_read_ar_hdr
#define _bfd_archive_bsd_openr_next_archived_file \
  bfd_generic_openr_next_archived_file
#define _bfd_archive_bsd_get_elt_at_index _bfd_generic_get_elt_at_index
#define _bfd_archive_bsd_generic_stat_arch_elt \
  bfd_generic_stat_arch_elt
extern boolean _bfd_archive_bsd_update_armap_timestamp PARAMS ((bfd *));

/* Routines to use for BFD_JUMP_TABLE_ARCHIVE to get COFF style
   archives.  Use BFD_JUMP_TABLE_ARCHIVE (_bfd_archive_coff).  */

#define _bfd_archive_coff_slurp_armap bfd_slurp_coff_armap
#define _bfd_archive_coff_slurp_extended_name_table \
  _bfd_slurp_extended_name_table
extern boolean _bfd_archive_coff_construct_extended_name_table
  PARAMS ((bfd *, char **, bfd_size_type *, const char **));
#define _bfd_archive_coff_truncate_arname bfd_dont_truncate_arname
#define _bfd_archive_coff_write_armap coff_write_armap
#define _bfd_archive_coff_read_ar_hdr _bfd_generic_read_ar_hdr
#define _bfd_archive_coff_openr_next_archived_file \
  bfd_generic_openr_next_archived_file
#define _bfd_archive_coff_get_elt_at_index _bfd_generic_get_elt_at_index
#define _bfd_archive_coff_generic_stat_arch_elt \
  bfd_generic_stat_arch_elt
#define _bfd_archive_coff_update_armap_timestamp bfd_true

/* Routines to use for BFD_JUMP_TABLE_SYMBOLS where there is no symbol
   support.  Use BFD_JUMP_TABLE_SYMBOLS (_bfd_nosymbols).  */

#define _bfd_nosymbols_get_symtab_upper_bound _bfd_n1
#define _bfd_nosymbols_get_symtab \
  ((long (*) PARAMS ((bfd *, asymbol **))) _bfd_n1)
#define _bfd_nosymbols_make_empty_symbol \
  ((asymbol *(*) PARAMS ((bfd *))) bfd_nullvoidptr)
#define _bfd_nosymbols_print_symbol \
  ((void (*) PARAMS ((bfd *, PTR, asymbol *, bfd_print_symbol_type))) bfd_void)
#define _bfd_nosymbols_get_symbol_info \
  ((void (*) PARAMS ((bfd *, asymbol *, symbol_info *))) bfd_void)
#define _bfd_nosymbols_bfd_is_local_label_name \
  ((boolean (*) PARAMS ((bfd *, const char *))) bfd_false)
#define _bfd_nosymbols_get_lineno \
  ((alent *(*) PARAMS ((bfd *, asymbol *))) bfd_nullvoidptr)
#define _bfd_nosymbols_find_nearest_line \
  ((boolean (*) \
    PARAMS ((bfd *, asection *, asymbol **, bfd_vma, const char **, \
	     const char **, unsigned int *))) \
   bfd_false)
#define _bfd_nosymbols_bfd_make_debug_symbol \
  ((asymbol *(*) PARAMS ((bfd *, PTR, unsigned long))) bfd_nullvoidptr)
#define _bfd_nosymbols_read_minisymbols \
  ((long (*) PARAMS ((bfd *, boolean, PTR *, unsigned int *))) _bfd_n1)
#define _bfd_nosymbols_minisymbol_to_symbol \
  ((asymbol *(*) PARAMS ((bfd *, boolean, const PTR, asymbol *))) \
   bfd_nullvoidptr)

/* Routines to use for BFD_JUMP_TABLE_RELOCS when there is no reloc
   support.  Use BFD_JUMP_TABLE_RELOCS (_bfd_norelocs).  */

#define _bfd_norelocs_get_reloc_upper_bound \
  ((long (*) PARAMS ((bfd *, asection *))) _bfd_n1)
#define _bfd_norelocs_canonicalize_reloc \
  ((long (*) PARAMS ((bfd *, asection *, arelent **, asymbol **))) _bfd_n1)
#define _bfd_norelocs_bfd_reloc_type_lookup \
  ((reloc_howto_type *(*) PARAMS ((bfd *, bfd_reloc_code_real_type))) \
   bfd_nullvoidptr)

/* Routines to use for BFD_JUMP_TABLE_WRITE for targets which may not
   be written.  Use BFD_JUMP_TABLE_WRITE (_bfd_nowrite).  */

#define _bfd_nowrite_set_arch_mach \
  ((boolean (*) PARAMS ((bfd *, enum bfd_architecture, unsigned long))) \
   bfd_false)
#define _bfd_nowrite_set_section_contents \
  ((boolean (*) PARAMS ((bfd *, asection *, PTR, file_ptr, bfd_size_type))) \
   bfd_false)

/* Generic routines to use for BFD_JUMP_TABLE_WRITE.  Use
   BFD_JUMP_TABLE_WRITE (_bfd_generic).  */

#define _bfd_generic_set_arch_mach bfd_default_set_arch_mach
extern boolean _bfd_generic_set_section_contents
  PARAMS ((bfd *, asection *, PTR, file_ptr, bfd_size_type));

/* Routines to use for BFD_JUMP_TABLE_LINK for targets which do not
   support linking.  Use BFD_JUMP_TABLE_LINK (_bfd_nolink).  */

#define _bfd_nolink_sizeof_headers ((int (*) PARAMS ((bfd *, boolean))) bfd_0)
#define _bfd_nolink_bfd_get_relocated_section_contents \
  ((bfd_byte *(*) \
    PARAMS ((bfd *, struct bfd_link_info *, struct bfd_link_order *, \
	     bfd_byte *, boolean, asymbol **))) \
   bfd_nullvoidptr)
#define _bfd_nolink_bfd_relax_section \
  ((boolean (*) \
    PARAMS ((bfd *, asection *, struct bfd_link_info *, boolean *))) \
   bfd_false)
#define _bfd_nolink_bfd_link_hash_table_create \
  ((struct bfd_link_hash_table *(*) PARAMS ((bfd *))) bfd_nullvoidptr)
#define _bfd_nolink_bfd_link_add_symbols \
  ((boolean (*) PARAMS ((bfd *, struct bfd_link_info *))) bfd_false)
#define _bfd_nolink_bfd_final_link \
  ((boolean (*) PARAMS ((bfd *, struct bfd_link_info *))) bfd_false)
#define _bfd_nolink_bfd_link_split_section \
  ((boolean (*) PARAMS ((bfd *, struct sec *))) bfd_false)

/* Routines to use for BFD_JUMP_TABLE_DYNAMIC for targets which do not
   have dynamic symbols or relocs.  Use BFD_JUMP_TABLE_DYNAMIC
   (_bfd_nodynamic).  */

#define _bfd_nodynamic_get_dynamic_symtab_upper_bound _bfd_n1
#define _bfd_nodynamic_canonicalize_dynamic_symtab \
  ((long (*) PARAMS ((bfd *, asymbol **))) _bfd_n1)
#define _bfd_nodynamic_get_dynamic_reloc_upper_bound _bfd_n1
#define _bfd_nodynamic_canonicalize_dynamic_reloc \
  ((long (*) PARAMS ((bfd *, arelent **, asymbol **))) _bfd_n1)

/* Generic routine to determine of the given symbol is a local
   label.  */
extern boolean bfd_generic_is_local_label_name PARAMS ((bfd *, const char *));

/* Generic minisymbol routines.  */
extern long _bfd_generic_read_minisymbols
  PARAMS ((bfd *, boolean, PTR *, unsigned int *));
extern asymbol *_bfd_generic_minisymbol_to_symbol
  PARAMS ((bfd *, boolean, const PTR, asymbol *));

/* Find the nearest line using .stab/.stabstr sections.  */
extern boolean _bfd_stab_section_find_nearest_line
  PARAMS ((bfd *, asymbol **, asection *, bfd_vma, boolean *, const char **,
	   const char **, unsigned int *, PTR *));

/* Find the nearest line using DWARF 2 debugging information.  */
extern boolean _bfd_dwarf2_find_nearest_line
  PARAMS ((bfd *, asection *, asymbol **, bfd_vma, const char **,
	   const char **, unsigned int *));

/* A routine to create entries for a bfd_link_hash_table.  */
extern struct bfd_hash_entry *_bfd_link_hash_newfunc
  PARAMS ((struct bfd_hash_entry *entry,
	   struct bfd_hash_table *table,
	   const char *string));

/* Initialize a bfd_link_hash_table.  */
extern boolean _bfd_link_hash_table_init
  PARAMS ((struct bfd_link_hash_table *, bfd *,
	   struct bfd_hash_entry *(*) (struct bfd_hash_entry *,
				       struct bfd_hash_table *,
				       const char *)));

/* Generic link hash table creation routine.  */
extern struct bfd_link_hash_table *_bfd_generic_link_hash_table_create
  PARAMS ((bfd *));

/* Generic add symbol routine.  */
extern boolean _bfd_generic_link_add_symbols
  PARAMS ((bfd *, struct bfd_link_info *));

/* Generic add symbol routine.  This version is used by targets for
   which the linker must collect constructors and destructors by name,
   as the collect2 program does.  */
extern boolean _bfd_generic_link_add_symbols_collect
  PARAMS ((bfd *, struct bfd_link_info *));

/* Generic archive add symbol routine.  */
extern boolean _bfd_generic_link_add_archive_symbols
  PARAMS ((bfd *, struct bfd_link_info *,
	   boolean (*checkfn) (bfd *, struct bfd_link_info *, boolean *)));



/* Forward declaration to avoid prototype errors.  */
typedef struct bfd_link_hash_entry _bfd_link_hash_entry;

/* Generic routine to add a single symbol.  */
extern boolean _bfd_generic_link_add_one_symbol
  PARAMS ((struct bfd_link_info *, bfd *, const char *name, flagword,
	   asection *, bfd_vma, const char *, boolean copy,
	   boolean constructor, struct bfd_link_hash_entry **));

/* Generic link routine.  */
extern boolean _bfd_generic_final_link
  PARAMS ((bfd *, struct bfd_link_info *));

extern boolean _bfd_generic_link_split_section
  PARAMS ((bfd *, struct sec *));

/* Generic reloc_link_order processing routine.  */
extern boolean _bfd_generic_reloc_link_order
  PARAMS ((bfd *, struct bfd_link_info *, asection *,
	   struct bfd_link_order *));

/* Default link order processing routine.  */
extern boolean _bfd_default_link_order
  PARAMS ((bfd *, struct bfd_link_info *, asection *,
	   struct bfd_link_order *));

/* Count the number of reloc entries in a link order list.  */
extern unsigned int _bfd_count_link_order_relocs
  PARAMS ((struct bfd_link_order *));

/* Final link relocation routine.  */
extern bfd_reloc_status_type _bfd_final_link_relocate
  PARAMS ((reloc_howto_type *, bfd *, asection *, bfd_byte *,
	   bfd_vma address, bfd_vma value, bfd_vma addend));

/* Relocate a particular location by a howto and a value.  */
extern bfd_reloc_status_type _bfd_relocate_contents
  PARAMS ((reloc_howto_type *, bfd *, bfd_vma, bfd_byte *));

/* Link stabs in sections in the first pass.  */

extern boolean _bfd_link_section_stabs
  PARAMS ((bfd *, PTR *, asection *, asection *, PTR *));

/* Write out the .stab section when linking stabs in sections.  */

extern boolean _bfd_write_section_stabs
  PARAMS ((bfd *, PTR *, asection *, PTR *, bfd_byte *));

/* Write out the .stabstr string table when linking stabs in sections.  */

extern boolean _bfd_write_stab_strings PARAMS ((bfd *, PTR *));

/* Find an offset within a .stab section when linking stabs in
   sections.  */

extern bfd_vma _bfd_stab_section_offset
  PARAMS ((bfd *, PTR *, asection *, PTR *, bfd_vma));

/* Create a string table.  */
extern struct bfd_strtab_hash *_bfd_stringtab_init PARAMS ((void));

/* Create an XCOFF .debug section style string table.  */
extern struct bfd_strtab_hash *_bfd_xcoff_stringtab_init PARAMS ((void));

/* Free a string table.  */
extern void _bfd_stringtab_free PARAMS ((struct bfd_strtab_hash *));

/* Get the size of a string table.  */
extern bfd_size_type _bfd_stringtab_size PARAMS ((struct bfd_strtab_hash *));

/* Add a string to a string table.  */
extern bfd_size_type _bfd_stringtab_add
  PARAMS ((struct bfd_strtab_hash *, const char *, boolean hash,
	   boolean copy));

/* Write out a string table.  */
extern boolean _bfd_stringtab_emit PARAMS ((bfd *, struct bfd_strtab_hash *));

/* Macros to tell if bfds are read or write enabled.

   Note that bfds open for read may be scribbled into if the fd passed
   to bfd_fdopenr is actually open both for read and write
   simultaneously.  However an output bfd will never be open for
   read.  Therefore sometimes you want to check bfd_read_p or
   !bfd_read_p, and only sometimes bfd_write_p.
*/

#define	bfd_read_p(abfd) ((abfd)->direction == read_direction || (abfd)->direction == both_direction)
#define	bfd_write_p(abfd) ((abfd)->direction == write_direction || (abfd)->direction == both_direction)

void	bfd_assert PARAMS ((const char*,int));

#define BFD_ASSERT(x) \
{ if (!(x)) bfd_assert(__FILE__,__LINE__); }

#define BFD_FAIL() \
{ bfd_assert(__FILE__,__LINE__); }

FILE *	bfd_cache_lookup_worker PARAMS ((bfd *));

extern bfd *bfd_last_cache;

/* List of supported target vectors, and the default vector (if
   bfd_default_vector[0] is NULL, there is no default).  */
extern const bfd_target * const bfd_target_vector[];
extern const bfd_target *bfd_default_vector[];

/* Functions shared by the ECOFF and MIPS ELF backends, which have no
   other common header files.  */

#if defined(__STDC__) || defined(ALMOST_STDC)
struct ecoff_find_line;
#endif

extern boolean _bfd_ecoff_locate_line
  PARAMS ((bfd *, asection *, bfd_vma, struct ecoff_debug_info * const,
	   const struct ecoff_debug_swap * const, struct ecoff_find_line *,
	   const char **, const char **, unsigned int *));
extern boolean _bfd_ecoff_get_accumulated_pdr PARAMS ((PTR, bfd_byte *));
extern boolean _bfd_ecoff_get_accumulated_sym PARAMS ((PTR, bfd_byte *));
extern boolean _bfd_ecoff_get_accumulated_ss PARAMS ((PTR, bfd_byte *));

extern bfd_vma _bfd_get_gp_value PARAMS ((bfd *));
extern void _bfd_set_gp_value PARAMS ((bfd *, bfd_vma));

/* Function shared by the COFF and ELF SH backends, which have no
   other common header files.  */

extern boolean _bfd_sh_align_load_span
  PARAMS ((bfd *, asection *, bfd_byte *,
	   boolean (*) (bfd *, asection *, PTR, bfd_byte *, bfd_vma),
	   PTR, bfd_vma **, bfd_vma *, bfd_vma, bfd_vma, boolean *));

/* And more follows */

void 
bfd_write_bigendian_4byte_int PARAMS ((bfd *abfd,  int i));

unsigned int 
bfd_log2 PARAMS ((bfd_vma x));

#define BFD_CACHE_MAX_OPEN 10
extern bfd *bfd_last_cache;

#define bfd_cache_lookup(x) \
    ((x)==bfd_last_cache? \
      (FILE*)(bfd_last_cache->iostream): \
       bfd_cache_lookup_worker(x))
boolean 
bfd_cache_init  PARAMS ((bfd *abfd));

boolean 
bfd_cache_close  PARAMS ((bfd *abfd));

FILE* 
bfd_open_file PARAMS ((bfd *abfd));

FILE *
bfd_cache_lookup_worker PARAMS ((bfd *abfd));

#ifdef _BFD_MAKE_TABLE_bfd_reloc_code_real

static const char *const bfd_reloc_code_real_names[] = { "@@uninitialized@@",

  "BFD_RELOC_64",
  "BFD_RELOC_32",
  "BFD_RELOC_26",
  "BFD_RELOC_24",
  "BFD_RELOC_16",
  "BFD_RELOC_14",
  "BFD_RELOC_8",
  "BFD_RELOC_64_PCREL",
  "BFD_RELOC_32_PCREL",
  "BFD_RELOC_24_PCREL",
  "BFD_RELOC_16_PCREL",
  "BFD_RELOC_12_PCREL",
  "BFD_RELOC_8_PCREL",
  "BFD_RELOC_32_GOT_PCREL",
  "BFD_RELOC_16_GOT_PCREL",
  "BFD_RELOC_8_GOT_PCREL",
  "BFD_RELOC_32_GOTOFF",
  "BFD_RELOC_16_GOTOFF",
  "BFD_RELOC_LO16_GOTOFF",
  "BFD_RELOC_HI16_GOTOFF",
  "BFD_RELOC_HI16_S_GOTOFF",
  "BFD_RELOC_8_GOTOFF",
  "BFD_RELOC_32_PLT_PCREL",
  "BFD_RELOC_24_PLT_PCREL",
  "BFD_RELOC_16_PLT_PCREL",
  "BFD_RELOC_8_PLT_PCREL",
  "BFD_RELOC_32_PLTOFF",
  "BFD_RELOC_16_PLTOFF",
  "BFD_RELOC_LO16_PLTOFF",
  "BFD_RELOC_HI16_PLTOFF",
  "BFD_RELOC_HI16_S_PLTOFF",
  "BFD_RELOC_8_PLTOFF",
  "BFD_RELOC_68K_GLOB_DAT",
  "BFD_RELOC_68K_JMP_SLOT",
  "BFD_RELOC_68K_RELATIVE",
  "BFD_RELOC_32_BASEREL",
  "BFD_RELOC_16_BASEREL",
  "BFD_RELOC_LO16_BASEREL",
  "BFD_RELOC_HI16_BASEREL",
  "BFD_RELOC_HI16_S_BASEREL",
  "BFD_RELOC_8_BASEREL",
  "BFD_RELOC_RVA",
  "BFD_RELOC_8_FFnn",
  "BFD_RELOC_32_PCREL_S2",
  "BFD_RELOC_16_PCREL_S2",
  "BFD_RELOC_23_PCREL_S2",
  "BFD_RELOC_HI22",
  "BFD_RELOC_LO10",
  "BFD_RELOC_GPREL16",
  "BFD_RELOC_GPREL32",
  "BFD_RELOC_I960_CALLJ",
  "BFD_RELOC_NONE",
  "BFD_RELOC_SPARC_WDISP22",
  "BFD_RELOC_SPARC22",
  "BFD_RELOC_SPARC13",
  "BFD_RELOC_SPARC_GOT10",
  "BFD_RELOC_SPARC_GOT13",
  "BFD_RELOC_SPARC_GOT22",
  "BFD_RELOC_SPARC_PC10",
  "BFD_RELOC_SPARC_PC22",
  "BFD_RELOC_SPARC_WPLT30",
  "BFD_RELOC_SPARC_COPY",
  "BFD_RELOC_SPARC_GLOB_DAT",
  "BFD_RELOC_SPARC_JMP_SLOT",
  "BFD_RELOC_SPARC_RELATIVE",
  "BFD_RELOC_SPARC_UA32",
  "BFD_RELOC_SPARC_BASE13",
  "BFD_RELOC_SPARC_BASE22",
  "BFD_RELOC_SPARC_10",
  "BFD_RELOC_SPARC_11",
  "BFD_RELOC_SPARC_OLO10",
  "BFD_RELOC_SPARC_HH22",
  "BFD_RELOC_SPARC_HM10",
  "BFD_RELOC_SPARC_LM22",
  "BFD_RELOC_SPARC_PC_HH22",
  "BFD_RELOC_SPARC_PC_HM10",
  "BFD_RELOC_SPARC_PC_LM22",
  "BFD_RELOC_SPARC_WDISP16",
  "BFD_RELOC_SPARC_WDISP19",
  "BFD_RELOC_SPARC_7",
  "BFD_RELOC_SPARC_6",
  "BFD_RELOC_SPARC_5",
  "BFD_RELOC_SPARC_PLT64",
  "BFD_RELOC_SPARC_HIX22",
  "BFD_RELOC_SPARC_LOX10",
  "BFD_RELOC_SPARC_H44",
  "BFD_RELOC_SPARC_M44",
  "BFD_RELOC_SPARC_L44",
  "BFD_RELOC_SPARC_REGISTER",
  "BFD_RELOC_ALPHA_GPDISP_HI16",
  "BFD_RELOC_ALPHA_GPDISP_LO16",
  "BFD_RELOC_ALPHA_GPDISP",
  "BFD_RELOC_ALPHA_LITERAL",
  "BFD_RELOC_ALPHA_ELF_LITERAL",
  "BFD_RELOC_ALPHA_LITUSE",
  "BFD_RELOC_ALPHA_HINT",
  "BFD_RELOC_ALPHA_LINKAGE",
  "BFD_RELOC_ALPHA_CODEADDR",
  "BFD_RELOC_MIPS_JMP",
  "BFD_RELOC_MIPS16_JMP",
  "BFD_RELOC_MIPS16_GPREL",
  "BFD_RELOC_HI16",
  "BFD_RELOC_HI16_S",
  "BFD_RELOC_LO16",
  "BFD_RELOC_PCREL_HI16_S",
  "BFD_RELOC_PCREL_LO16",
  "BFD_RELOC_MIPS_LITERAL",
  "BFD_RELOC_MIPS_GOT16",
  "BFD_RELOC_MIPS_CALL16",
  "BFD_RELOC_MIPS_GOT_HI16",
  "BFD_RELOC_MIPS_GOT_LO16",
  "BFD_RELOC_MIPS_CALL_HI16",
  "BFD_RELOC_MIPS_CALL_LO16",

  "BFD_RELOC_386_GOT32",
  "BFD_RELOC_386_PLT32",
  "BFD_RELOC_386_COPY",
  "BFD_RELOC_386_GLOB_DAT",
  "BFD_RELOC_386_JUMP_SLOT",
  "BFD_RELOC_386_RELATIVE",
  "BFD_RELOC_386_GOTOFF",
  "BFD_RELOC_386_GOTPC",
  "BFD_RELOC_NS32K_IMM_8",
  "BFD_RELOC_NS32K_IMM_16",
  "BFD_RELOC_NS32K_IMM_32",
  "BFD_RELOC_NS32K_IMM_8_PCREL",
  "BFD_RELOC_NS32K_IMM_16_PCREL",
  "BFD_RELOC_NS32K_IMM_32_PCREL",
  "BFD_RELOC_NS32K_DISP_8",
  "BFD_RELOC_NS32K_DISP_16",
  "BFD_RELOC_NS32K_DISP_32",
  "BFD_RELOC_NS32K_DISP_8_PCREL",
  "BFD_RELOC_NS32K_DISP_16_PCREL",
  "BFD_RELOC_NS32K_DISP_32_PCREL",
  "BFD_RELOC_PPC_B26",
  "BFD_RELOC_PPC_BA26",
  "BFD_RELOC_PPC_TOC16",
  "BFD_RELOC_PPC_B16",
  "BFD_RELOC_PPC_B16_BRTAKEN",
  "BFD_RELOC_PPC_B16_BRNTAKEN",
  "BFD_RELOC_PPC_BA16",
  "BFD_RELOC_PPC_BA16_BRTAKEN",
  "BFD_RELOC_PPC_BA16_BRNTAKEN",
  "BFD_RELOC_PPC_COPY",
  "BFD_RELOC_PPC_GLOB_DAT",
  "BFD_RELOC_PPC_JMP_SLOT",
  "BFD_RELOC_PPC_RELATIVE",
  "BFD_RELOC_PPC_LOCAL24PC",
  "BFD_RELOC_PPC_EMB_NADDR32",
  "BFD_RELOC_PPC_EMB_NADDR16",
  "BFD_RELOC_PPC_EMB_NADDR16_LO",
  "BFD_RELOC_PPC_EMB_NADDR16_HI",
  "BFD_RELOC_PPC_EMB_NADDR16_HA",
  "BFD_RELOC_PPC_EMB_SDAI16",
  "BFD_RELOC_PPC_EMB_SDA2I16",
  "BFD_RELOC_PPC_EMB_SDA2REL",
  "BFD_RELOC_PPC_EMB_SDA21",
  "BFD_RELOC_PPC_EMB_MRKREF",
  "BFD_RELOC_PPC_EMB_RELSEC16",
  "BFD_RELOC_PPC_EMB_RELST_LO",
  "BFD_RELOC_PPC_EMB_RELST_HI",
  "BFD_RELOC_PPC_EMB_RELST_HA",
  "BFD_RELOC_PPC_EMB_BIT_FLD",
  "BFD_RELOC_PPC_EMB_RELSDA",
  "BFD_RELOC_CTOR",
  "BFD_RELOC_ARM_PCREL_BRANCH",
  "BFD_RELOC_ARM_IMMEDIATE",
  "BFD_RELOC_ARM_OFFSET_IMM",
  "BFD_RELOC_ARM_SHIFT_IMM",
  "BFD_RELOC_ARM_SWI",
  "BFD_RELOC_ARM_MULTI",
  "BFD_RELOC_ARM_CP_OFF_IMM",
  "BFD_RELOC_ARM_ADR_IMM",
  "BFD_RELOC_ARM_LDR_IMM",
  "BFD_RELOC_ARM_LITERAL",
  "BFD_RELOC_ARM_IN_POOL",
  "BFD_RELOC_ARM_OFFSET_IMM8",
  "BFD_RELOC_ARM_HWLITERAL",
  "BFD_RELOC_ARM_THUMB_ADD",
  "BFD_RELOC_ARM_THUMB_IMM",
  "BFD_RELOC_ARM_THUMB_SHIFT",
  "BFD_RELOC_ARM_THUMB_OFFSET",
  "BFD_RELOC_SH_PCDISP8BY2",
  "BFD_RELOC_SH_PCDISP12BY2",
  "BFD_RELOC_SH_IMM4",
  "BFD_RELOC_SH_IMM4BY2",
  "BFD_RELOC_SH_IMM4BY4",
  "BFD_RELOC_SH_IMM8",
  "BFD_RELOC_SH_IMM8BY2",
  "BFD_RELOC_SH_IMM8BY4",
  "BFD_RELOC_SH_PCRELIMM8BY2",
  "BFD_RELOC_SH_PCRELIMM8BY4",
  "BFD_RELOC_SH_SWITCH16",
  "BFD_RELOC_SH_SWITCH32",
  "BFD_RELOC_SH_USES",
  "BFD_RELOC_SH_COUNT",
  "BFD_RELOC_SH_ALIGN",
  "BFD_RELOC_SH_CODE",
  "BFD_RELOC_SH_DATA",
  "BFD_RELOC_SH_LABEL",
  "BFD_RELOC_THUMB_PCREL_BRANCH9",
  "BFD_RELOC_THUMB_PCREL_BRANCH12",
  "BFD_RELOC_THUMB_PCREL_BRANCH23",
  "BFD_RELOC_ARC_B22_PCREL",
  "BFD_RELOC_ARC_B26",
  "BFD_RELOC_D10V_10_PCREL_R",
  "BFD_RELOC_D10V_10_PCREL_L",
  "BFD_RELOC_D10V_18",
  "BFD_RELOC_D10V_18_PCREL",


  "BFD_RELOC_M32R_24",
  "BFD_RELOC_M32R_10_PCREL",
  "BFD_RELOC_M32R_18_PCREL",
  "BFD_RELOC_M32R_26_PCREL",
  "BFD_RELOC_M32R_HI16_ULO",
  "BFD_RELOC_M32R_HI16_SLO",
  "BFD_RELOC_M32R_LO16",
  "BFD_RELOC_M32R_SDA16",
  "BFD_RELOC_V850_9_PCREL",
  "BFD_RELOC_V850_22_PCREL",
  "BFD_RELOC_V850_SDA_16_16_OFFSET",
  "BFD_RELOC_V850_SDA_15_16_OFFSET",
  "BFD_RELOC_V850_ZDA_16_16_OFFSET",
  "BFD_RELOC_V850_ZDA_15_16_OFFSET",
  "BFD_RELOC_V850_TDA_6_8_OFFSET",
  "BFD_RELOC_V850_TDA_7_8_OFFSET",
  "BFD_RELOC_V850_TDA_7_7_OFFSET",
  "BFD_RELOC_V850_TDA_16_16_OFFSET",

  "BFD_RELOC_MN10300_32_PCREL",
  "BFD_RELOC_MN10300_16_PCREL",
  "BFD_RELOC_TIC30_LDP",
 "@@overflow: BFD_RELOC_UNUSED@@",
};
#endif

reloc_howto_type *
bfd_default_reloc_type_lookup
 PARAMS ((bfd *abfd, bfd_reloc_code_real_type  code));

boolean 
bfd_generic_relax_section
 PARAMS ((bfd *abfd,
    asection *section,
    struct bfd_link_info *,
    boolean *));

bfd_byte *

bfd_generic_get_relocated_section_contents  PARAMS ((bfd *abfd,
    struct bfd_link_info *link_info,
    struct bfd_link_order *link_order,
    bfd_byte *data,
    boolean relocateable,
    asymbol **symbols));

extern const bfd_arch_info_type bfd_default_arch_struct;
boolean 
bfd_default_set_arch_mach PARAMS ((bfd *abfd,
    enum bfd_architecture arch,
    unsigned long mach));

const bfd_arch_info_type *
bfd_default_compatible
 PARAMS ((const bfd_arch_info_type *a,
    const bfd_arch_info_type *b));

boolean 
bfd_default_scan PARAMS ((const struct bfd_arch_info *info, const char *string));

struct elf_internal_shdr *
bfd_elf_find_section  PARAMS ((bfd *abfd, char *name));

