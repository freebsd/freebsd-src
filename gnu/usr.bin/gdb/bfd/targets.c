/* Generic target-file-type support for the BFD library.
   Copyright 1990, 91, 92, 93, 1994 Free Software Foundation, Inc.
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

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"

/*
SECTION 
	Targets

DESCRIPTION
	Each port of BFD to a different machine requries the creation
	of a target back end. All the back end provides to the root
	part of BFD is a structure containing pointers to functions
	which perform certain low level operations on files. BFD
	translates the applications's requests through a pointer into
	calls to the back end routines. 

	When a file is opened with <<bfd_openr>>, its format and
	target are unknown. BFD uses various mechanisms to determine
	how to interpret the file. The operations performed are:

	o Create a BFD by calling the internal routine
	<<_bfd_new_bfd>>, then call <<bfd_find_target>> with the
	target string supplied to <<bfd_openr>> and the new BFD pointer. 

	o If a null target string was provided to <<bfd_find_target>>,
	look up the environment variable <<GNUTARGET>> and use
	that as the target string. 

	o If the target string is still <<NULL>>, or the target string is
	<<default>>, then use the first item in the target vector
	as the target type, and set <<target_defaulted>> in the BFD to
	cause <<bfd_check_format>> to loop through all the targets.
	@xref{bfd_target}.  @xref{Formats}.

	o Otherwise, inspect the elements in the target vector
	one by one, until a match on target name is found. When found,
	use it. 

	o Otherwise return the error <<bfd_error_invalid_target>> to
	<<bfd_openr>>.

	o <<bfd_openr>> attempts to open the file using
	<<bfd_open_file>>, and returns the BFD.

	Once the BFD has been opened and the target selected, the file
	format may be determined. This is done by calling
	<<bfd_check_format>> on the BFD with a suggested format. 
	If <<target_defaulted>> has been set, each possible target
	type is tried to see if it recognizes the specified format.
	<<bfd_check_format>> returns <<true>> when the caller guesses right.
@menu
@* bfd_target::
@end menu
*/


/*

INODE
	bfd_target,  , Targets, Targets
DOCDD
SUBSECTION
	bfd_target

DESCRIPTION
	This structure contains everything that BFD knows about a
	target. It includes things like its byte order, name, and which
	routines to call to do various operations.   

	Every BFD points to a target structure with its <<xvec>>
	member. 

	The macros below are used to dispatch to functions through the
	<<bfd_target>> vector. They are used in a number of macros further
	down in @file{bfd.h}, and are also used when calling various
	routines by hand inside the BFD implementation.  The @var{arglist}
	argument must be parenthesized; it contains all the arguments
	to the called function. 

	They make the documentation (more) unpleasant to read, so if
	someone wants to fix this and not break the above, please do.

.#define BFD_SEND(bfd, message, arglist) \
.               ((*((bfd)->xvec->message)) arglist)
.
.#ifdef DEBUG_BFD_SEND
.#undef BFD_SEND
.#define BFD_SEND(bfd, message, arglist) \
.  (((bfd) && (bfd)->xvec && (bfd)->xvec->message) ? \
.    ((*((bfd)->xvec->message)) arglist) : \
.    (bfd_assert (__FILE__,__LINE__), NULL))
.#endif

	For operations which index on the BFD format:

.#define BFD_SEND_FMT(bfd, message, arglist) \
.            (((bfd)->xvec->message[(int)((bfd)->format)]) arglist)
.
.#ifdef DEBUG_BFD_SEND
.#undef BFD_SEND_FMT
.#define BFD_SEND_FMT(bfd, message, arglist) \
.  (((bfd) && (bfd)->xvec && (bfd)->xvec->message) ? \
.   (((bfd)->xvec->message[(int)((bfd)->format)]) arglist) : \
.   (bfd_assert (__FILE__,__LINE__), NULL))
.#endif

	This is the structure which defines the type of BFD this is.  The
	<<xvec>> member of the struct <<bfd>> itself points here.  Each
	module that implements access to a different target under BFD,
	defines one of these.


	FIXME, these names should be rationalised with the names of
	the entry points which call them. Too bad we can't have one
	macro to define them both! 

.enum bfd_flavour {
.  bfd_target_unknown_flavour,
.  bfd_target_aout_flavour,
.  bfd_target_coff_flavour,
.  bfd_target_ecoff_flavour,
.  bfd_target_elf_flavour,
.  bfd_target_ieee_flavour,
.  bfd_target_nlm_flavour,
.  bfd_target_oasys_flavour,
.  bfd_target_tekhex_flavour,
.  bfd_target_srec_flavour,
.  bfd_target_som_flavour,
.  bfd_target_os9k_flavour};
.
.{* Forward declaration.  *}
.typedef struct bfd_link_info _bfd_link_info;
.
.typedef struct bfd_target
.{

Identifies the kind of target, e.g., SunOS4, Ultrix, etc.

.  char *name;

The "flavour" of a back end is a general indication about the contents
of a file.

.  enum bfd_flavour flavour;

The order of bytes within the data area of a file.

.  boolean byteorder_big_p;

The order of bytes within the header parts of a file.

.  boolean header_byteorder_big_p;

A mask of all the flags which an executable may have set -
from the set <<NO_FLAGS>>, <<HAS_RELOC>>, ...<<D_PAGED>>.

.  flagword object_flags;       

A mask of all the flags which a section may have set - from
the set <<SEC_NO_FLAGS>>, <<SEC_ALLOC>>, ...<<SET_NEVER_LOAD>>.

.  flagword section_flags;

The character normally found at the front of a symbol 
(if any), perhaps `_'.

.  char symbol_leading_char;

The pad character for file names within an archive header.

.  char ar_pad_char;            

The maximum number of characters in an archive header.

.  unsigned short ar_max_namelen;

The minimum alignment restriction for any section.

.  unsigned int align_power_min;

Entries for byte swapping for data. These are different from the other
entry points, since they don't take a BFD asthe first argument.
Certain other handlers could do the same.

.  bfd_vma      (*bfd_getx64) PARAMS ((const bfd_byte *));
.  bfd_signed_vma (*bfd_getx_signed_64) PARAMS ((const bfd_byte *));
.  void         (*bfd_putx64) PARAMS ((bfd_vma, bfd_byte *));
.  bfd_vma      (*bfd_getx32) PARAMS ((const bfd_byte *));
.  bfd_signed_vma (*bfd_getx_signed_32) PARAMS ((const bfd_byte *));
.  void         (*bfd_putx32) PARAMS ((bfd_vma, bfd_byte *));
.  bfd_vma      (*bfd_getx16) PARAMS ((const bfd_byte *));
.  bfd_signed_vma (*bfd_getx_signed_16) PARAMS ((const bfd_byte *));
.  void         (*bfd_putx16) PARAMS ((bfd_vma, bfd_byte *));

Byte swapping for the headers

.  bfd_vma      (*bfd_h_getx64) PARAMS ((const bfd_byte *));
.  bfd_signed_vma (*bfd_h_getx_signed_64) PARAMS ((const bfd_byte *));
.  void         (*bfd_h_putx64) PARAMS ((bfd_vma, bfd_byte *));
.  bfd_vma      (*bfd_h_getx32) PARAMS ((const bfd_byte *));
.  bfd_signed_vma (*bfd_h_getx_signed_32) PARAMS ((const bfd_byte *));
.  void         (*bfd_h_putx32) PARAMS ((bfd_vma, bfd_byte *));
.  bfd_vma      (*bfd_h_getx16) PARAMS ((const bfd_byte *));
.  bfd_signed_vma (*bfd_h_getx_signed_16) PARAMS ((const bfd_byte *));
.  void         (*bfd_h_putx16) PARAMS ((bfd_vma, bfd_byte *));

Format dependent routines: these are vectors of entry points
within the target vector structure, one for each format to check.

Check the format of a file being read.  Return a <<bfd_target *>> or zero. 

.  const struct bfd_target *(*_bfd_check_format[bfd_type_end]) PARAMS ((bfd *));

Set the format of a file being written.  

.  boolean             (*_bfd_set_format[bfd_type_end]) PARAMS ((bfd *));

Write cached information into a file being written, at <<bfd_close>>. 

.  boolean             (*_bfd_write_contents[bfd_type_end]) PARAMS ((bfd *));

The general target vector.

.
.  {* Generic entry points.  *}
.#define BFD_JUMP_TABLE_GENERIC(NAME)\
.CAT(NAME,_close_and_cleanup),\
.CAT(NAME,_bfd_free_cached_info),\
.CAT(NAME,_new_section_hook),\
.CAT(NAME,_get_section_contents)
.  {* Called when the BFD is being closed to do any necessary cleanup.  *}
.  boolean       (*_close_and_cleanup) PARAMS ((bfd *));
.  {* Ask the BFD to free all cached information.  *}
.  boolean (*_bfd_free_cached_info) PARAMS ((bfd *));
.  {* Called when a new section is created.  *}
.  boolean       (*_new_section_hook) PARAMS ((bfd *, sec_ptr));
.  {* Read the contents of a section.  *}
.  boolean       (*_bfd_get_section_contents) PARAMS ((bfd *, sec_ptr, PTR, 
.                                            file_ptr, bfd_size_type));
.
.  {* Entry points to copy private data.  *}
.#define BFD_JUMP_TABLE_COPY(NAME)\
.CAT(NAME,_bfd_copy_private_bfd_data),\
.CAT(NAME,_bfd_copy_private_section_data)
.  {* Called to copy BFD general private data from one object file
.     to another.  *}
.  boolean	 (*_bfd_copy_private_bfd_data) PARAMS ((bfd *, bfd *));
.  {* Called to copy BFD private section data from one object file
.     to another.  *}
.  boolean       (*_bfd_copy_private_section_data) PARAMS ((bfd *, sec_ptr,
.                                                       bfd *, sec_ptr));
.
.  {* Core file entry points.  *}
.#define BFD_JUMP_TABLE_CORE(NAME)\
.CAT(NAME,_core_file_failing_command),\
.CAT(NAME,_core_file_failing_signal),\
.CAT(NAME,_core_file_matches_executable_p)
.  char *   (*_core_file_failing_command) PARAMS ((bfd *));
.  int      (*_core_file_failing_signal) PARAMS ((bfd *));
.  boolean  (*_core_file_matches_executable_p) PARAMS ((bfd *, bfd *));
.
.  {* Archive entry points.  *}
.#define BFD_JUMP_TABLE_ARCHIVE(NAME)\
.CAT(NAME,_slurp_armap),\
.CAT(NAME,_slurp_extended_name_table),\
.CAT(NAME,_truncate_arname),\
.CAT(NAME,_write_armap),\
.CAT(NAME,_openr_next_archived_file),\
.CAT(NAME,_generic_stat_arch_elt)
.  boolean  (*_bfd_slurp_armap) PARAMS ((bfd *));
.  boolean  (*_bfd_slurp_extended_name_table) PARAMS ((bfd *));
.  void     (*_bfd_truncate_arname) PARAMS ((bfd *, CONST char *, char *));
.  boolean  (*write_armap) PARAMS ((bfd *arch, 
.                              unsigned int elength,
.                              struct orl *map,
.                              unsigned int orl_count, 
.                              int stridx));
.  bfd *      (*openr_next_archived_file) PARAMS ((bfd *arch, bfd *prev));
.  int        (*_bfd_stat_arch_elt) PARAMS ((bfd *, struct stat *));
. 
.  {* Entry points used for symbols.  *}
.#define BFD_JUMP_TABLE_SYMBOLS(NAME)\
.CAT(NAME,_get_symtab_upper_bound),\
.CAT(NAME,_get_symtab),\
.CAT(NAME,_make_empty_symbol),\
.CAT(NAME,_print_symbol),\
.CAT(NAME,_get_symbol_info),\
.CAT(NAME,_bfd_is_local_label),\
.CAT(NAME,_get_lineno),\
.CAT(NAME,_find_nearest_line),\
.CAT(NAME,_bfd_make_debug_symbol)
.  long  (*_bfd_get_symtab_upper_bound) PARAMS ((bfd *));
.  long  (*_bfd_canonicalize_symtab) PARAMS ((bfd *,
.                                             struct symbol_cache_entry **));
.  struct symbol_cache_entry  *
.                (*_bfd_make_empty_symbol) PARAMS ((bfd *));
.  void          (*_bfd_print_symbol) PARAMS ((bfd *, PTR,
.                                      struct symbol_cache_entry *,
.                                      bfd_print_symbol_type));
.#define bfd_print_symbol(b,p,s,e) BFD_SEND(b, _bfd_print_symbol, (b,p,s,e))
.  void          (*_bfd_get_symbol_info) PARAMS ((bfd *,
.                                      struct symbol_cache_entry *,
.                                      symbol_info *));
.#define bfd_get_symbol_info(b,p,e) BFD_SEND(b, _bfd_get_symbol_info, (b,p,e))
.  boolean	 (*_bfd_is_local_label) PARAMS ((bfd *, asymbol *));
.
.  alent *    (*_get_lineno) PARAMS ((bfd *, struct symbol_cache_entry *));
.  boolean    (*_bfd_find_nearest_line) PARAMS ((bfd *abfd,
.                    struct sec *section, struct symbol_cache_entry **symbols,
.                    bfd_vma offset, CONST char **file, CONST char **func,
.                    unsigned int *line));
. {* Back-door to allow format-aware applications to create debug symbols
.    while using BFD for everything else.  Currently used by the assembler
.    when creating COFF files.  *}
. asymbol *  (*_bfd_make_debug_symbol) PARAMS ((
.       bfd *abfd,
.       void *ptr,
.       unsigned long size));
.
.  {* Routines for relocs.  *}
.#define BFD_JUMP_TABLE_RELOCS(NAME)\
.CAT(NAME,_get_reloc_upper_bound),\
.CAT(NAME,_canonicalize_reloc),\
.CAT(NAME,_bfd_reloc_type_lookup)
.  long  (*_get_reloc_upper_bound) PARAMS ((bfd *, sec_ptr));
.  long  (*_bfd_canonicalize_reloc) PARAMS ((bfd *, sec_ptr, arelent **,
.                                            struct symbol_cache_entry **));
.  {* See documentation on reloc types.  *}
.  CONST struct reloc_howto_struct *
.       (*reloc_type_lookup) PARAMS ((bfd *abfd,
.                                     bfd_reloc_code_real_type code));
.
.  {* Routines used when writing an object file.  *}
.#define BFD_JUMP_TABLE_WRITE(NAME)\
.CAT(NAME,_set_arch_mach),\
.CAT(NAME,_set_section_contents)
.  boolean    (*_bfd_set_arch_mach) PARAMS ((bfd *, enum bfd_architecture,
.                    unsigned long));
.  boolean       (*_bfd_set_section_contents) PARAMS ((bfd *, sec_ptr, PTR,
.                                            file_ptr, bfd_size_type));
.
.  {* Routines used by the linker.  *}
.#define BFD_JUMP_TABLE_LINK(NAME)\
.CAT(NAME,_sizeof_headers),\
.CAT(NAME,_bfd_get_relocated_section_contents),\
.CAT(NAME,_bfd_relax_section),\
.CAT(NAME,_bfd_link_hash_table_create),\
.CAT(NAME,_bfd_link_add_symbols),\
.CAT(NAME,_bfd_final_link)
.  int        (*_bfd_sizeof_headers) PARAMS ((bfd *, boolean));
.  bfd_byte * (*_bfd_get_relocated_section_contents) PARAMS ((bfd *,
.                    struct bfd_link_info *, struct bfd_link_order *,
.                    bfd_byte *data, boolean relocateable,
.                    struct symbol_cache_entry **));
.
.  boolean    (*_bfd_relax_section) PARAMS ((bfd *, struct sec *,
.                    struct bfd_link_info *, boolean *again));
.
.  {* Create a hash table for the linker.  Different backends store
.     different information in this table.  *}
.  struct bfd_link_hash_table *(*_bfd_link_hash_table_create) PARAMS ((bfd *));
.
.  {* Add symbols from this object file into the hash table.  *}
.  boolean (*_bfd_link_add_symbols) PARAMS ((bfd *, struct bfd_link_info *));
.
.  {* Do a link based on the link_order structures attached to each
.     section of the BFD.  *}
.  boolean (*_bfd_final_link) PARAMS ((bfd *, struct bfd_link_info *));
.
. {* Routines to handle dynamic symbols and relocs.  *}
.#define BFD_JUMP_TABLE_DYNAMIC(NAME)\
.CAT(NAME,_get_dynamic_symtab_upper_bound),\
.CAT(NAME,_canonicalize_dynamic_symtab),\
.CAT(NAME,_get_dynamic_reloc_upper_bound),\
.CAT(NAME,_canonicalize_dynamic_reloc)
.  {* Get the amount of memory required to hold the dynamic symbols. *}
.  long  (*_bfd_get_dynamic_symtab_upper_bound) PARAMS ((bfd *));
.  {* Read in the dynamic symbols.  *}
.  long  (*_bfd_canonicalize_dynamic_symtab)
.    PARAMS ((bfd *, struct symbol_cache_entry **));
.  {* Get the amount of memory required to hold the dynamic relocs.  *}
.  long  (*_bfd_get_dynamic_reloc_upper_bound) PARAMS ((bfd *));
.  {* Read in the dynamic relocs.  *}
.  long  (*_bfd_canonicalize_dynamic_reloc)
.    PARAMS ((bfd *, arelent **, struct symbol_cache_entry **));
.

Data for use by back-end routines, which isn't generic enough to belong
in this structure.

. PTR backend_data;
.} bfd_target;

*/

/* All known xvecs (even those that don't compile on all systems).
   Alphabetized for easy reference.
   They are listed a second time below, since
   we can't intermix extern's and initializers.  */
extern const bfd_target a29kcoff_big_vec;
extern const bfd_target a_out_adobe_vec;
extern const bfd_target aout_mips_big_vec;
extern const bfd_target aout_mips_little_vec;
extern const bfd_target aout0_big_vec;
extern const bfd_target apollocoff_vec;
extern const bfd_target b_out_vec_big_host;
extern const bfd_target b_out_vec_little_host;
extern const bfd_target bfd_elf32_big_generic_vec;
extern const bfd_target bfd_elf32_bigmips_vec;
extern const bfd_target bfd_elf32_hppa_vec;
extern const bfd_target bfd_elf32_i386_vec;
extern const bfd_target bfd_elf32_i860_vec;
extern const bfd_target bfd_elf32_little_generic_vec;
extern const bfd_target bfd_elf32_littlemips_vec;
extern const bfd_target bfd_elf32_m68k_vec;
extern const bfd_target bfd_elf32_m88k_vec;
extern const bfd_target bfd_elf32_powerpc_vec;
extern const bfd_target bfd_elf32_sparc_vec;
extern const bfd_target bfd_elf64_big_generic_vec;
extern const bfd_target bfd_elf64_little_generic_vec;
extern const bfd_target bfd_elf64_sparc_vec;
extern const bfd_target demo_64_vec;
extern const bfd_target ecoff_big_vec;
extern const bfd_target ecoff_little_vec;
extern const bfd_target ecoffalpha_little_vec;
extern const bfd_target h8300coff_vec;
extern const bfd_target h8500coff_vec;
extern const bfd_target host_aout_vec;
extern const bfd_target hp300bsd_vec;
extern const bfd_target hp300hpux_vec;
extern const bfd_target som_vec;
extern const bfd_target i386aout_vec;
extern const bfd_target i386bsd_vec;
extern const bfd_target i386dynix_vec;
extern const bfd_target i386os9k_vec;
extern const bfd_target netbsd386_vec;
extern const bfd_target freebsd386_vec;
extern const bfd_target i386coff_vec;
extern const bfd_target go32coff_vec;
extern const bfd_target i386linux_vec;
extern const bfd_target i386lynx_aout_vec;
extern const bfd_target i386lynx_coff_vec;
extern const bfd_target i386mach3_vec;
extern const bfd_target icoff_big_vec;
extern const bfd_target icoff_little_vec;
extern const bfd_target ieee_vec;
extern const bfd_target m68kcoff_vec;
extern const bfd_target m68kcoffun_vec;
extern const bfd_target m68klynx_aout_vec;
extern const bfd_target m68klynx_coff_vec;
extern const bfd_target m88kbcs_vec;
extern const bfd_target m88kmach3_vec;
extern const bfd_target netbsd532_vec;
extern const bfd_target newsos3_vec;
extern const bfd_target nlm32_i386_vec;
extern const bfd_target nlm32_sparc_vec;
extern const bfd_target nlm32_alpha_vec;
extern const bfd_target nlm32_powerpc_vec;
extern const bfd_target oasys_vec;
extern const bfd_target pc532mach_vec;
extern const bfd_target rs6000coff_vec;
extern const bfd_target shcoff_vec;
extern const bfd_target sparclynx_aout_vec;
extern const bfd_target sparclynx_coff_vec;
extern const bfd_target sparccoff_vec;
extern const bfd_target sunos_big_vec;
extern const bfd_target tekhex_vec;
extern const bfd_target we32kcoff_vec;
extern const bfd_target z8kcoff_vec;

/* srec is always included.  */
extern const bfd_target srec_vec;
extern const bfd_target symbolsrec_vec;

/* All of the xvecs for core files.  */
extern const bfd_target aix386_core_vec;
extern const bfd_target cisco_core_vec;
extern const bfd_target hpux_core_vec;
extern const bfd_target hppabsd_core_vec;
extern const bfd_target irix_core_vec;
extern const bfd_target osf_core_vec;
extern const bfd_target sco_core_vec;
extern const bfd_target trad_core_vec;
extern const bfd_target ptrace_core_vec;

const bfd_target * const bfd_target_vector[] = {

#ifdef SELECT_VECS

	SELECT_VECS,

#else /* not SELECT_VECS */

#ifdef DEFAULT_VECTOR
	&DEFAULT_VECTOR,
#endif
	/* This list is alphabetized to make it easy to compare
	   with other vector lists -- the decls above and
	   the case statement in configure.in.
	   Vectors that don't compile on all systems, or aren't finished,
	   should have an entry here with #if 0 around it, to show that
	   it wasn't omitted by mistake.  */
	&a29kcoff_big_vec,
	&a_out_adobe_vec,
#if 0				/* No one seems to use this.  */
	&aout_mips_big_vec,
#endif
	&aout_mips_little_vec,
	&b_out_vec_big_host,
	&b_out_vec_little_host,

	/* This, and other vectors, may not be used in any *.mt configuration.
	   But that does not mean they are unnecessary.  If configured with
	   --enable-targets=all, objdump or gdb should be able to examine
	   the file even if we don't recognize the machine type.  */
	&bfd_elf32_big_generic_vec,
	&bfd_elf32_bigmips_vec,
	&bfd_elf32_hppa_vec,
	&bfd_elf32_i386_vec,
	&bfd_elf32_i860_vec,
	&bfd_elf32_little_generic_vec,
	&bfd_elf32_littlemips_vec,
	&bfd_elf32_m68k_vec,
	&bfd_elf32_m88k_vec,
	&bfd_elf32_sparc_vec,
	&bfd_elf32_powerpc_vec,
#ifdef BFD64			/* No one seems to use this.  */
	&bfd_elf64_big_generic_vec,
	&bfd_elf64_little_generic_vec,
#endif
#if 0
	&bfd_elf64_sparc_vec,
#endif
	/* We don't include cisco_core_vec.  Although it has a magic number,
	   the magic number isn't at the beginning of the file, and thus
	   might spuriously match other kinds of files.  */
#ifdef BFD64
	&demo_64_vec,	/* Only compiled if host has long-long support */
#endif
	&ecoff_big_vec,
	&ecoff_little_vec,
#if 0
	&ecoffalpha_little_vec,
#endif
	&h8300coff_vec,
	&h8500coff_vec,
#if 0
	/* Since a.out files lack decent magic numbers, no way to recognize
	   which kind of a.out file it is.  */
	&host_aout_vec,
#endif
#if 0				/* Clashes with sunos_big_vec magic no.  */
	&hp300bsd_vec,
#endif
	&hp300hpux_vec,
#if defined (HOST_HPPAHPUX) || defined (HOST_HPPABSD) || defined (HOST_HPPAOSF)
        &som_vec,
#endif
	&i386aout_vec,
	&i386bsd_vec,
	&i386coff_vec,
	&go32coff_vec,
#if 0
	/* Since a.out files lack decent magic numbers, no way to recognize
	   which kind of a.out file it is.  */
	&i386linux_vec,
#endif
	&i386lynx_aout_vec,
	&i386lynx_coff_vec,
#if 0
	/* No distinguishing features for Mach 3 executables.  */
	&i386mach3_vec,
#endif
	&i386os9k_vec,
	&icoff_big_vec,
	&icoff_little_vec,
	&ieee_vec,
	&m68kcoff_vec,
	&m68kcoffun_vec,
	&m68klynx_aout_vec,
	&m68klynx_coff_vec,
	&m88kbcs_vec,
	&m88kmach3_vec,
	&newsos3_vec,
	&netbsd386_vec,
	&netbsd532_vec,
	&nlm32_i386_vec,
	&nlm32_sparc_vec,
#ifdef BFD64
	&nlm32_alpha_vec,
#endif
#if 0
	/* We have no oasys tools anymore, so we can't test any of this
	   anymore. If you want to test the stuff yourself, go ahead...
	   steve@cygnus.com
	   Worse, since there is no magic number for archives, there
	   can be annoying target mis-matches.  */
	&oasys_vec,
#endif
	&pc532machaout_vec,
	&rs6000coff_vec,
	&shcoff_vec,
	&sparclynx_aout_vec,
	&sparclynx_coff_vec,
	&sunos_big_vec,
	&aout0_big_vec,
#if 0
	&tekhex_vec,
#endif
	&we32kcoff_vec,
	&z8kcoff_vec,

#endif /* not SELECT_VECS */

/* Always support S-records, for convenience.  */
	&srec_vec,
	&symbolsrec_vec,

/* Add any required traditional-core-file-handler.  */

#ifdef AIX386_CORE
	&aix386_core_vec,
#endif
#ifdef HPUX_CORE
	&hpux_core_vec,
#endif
#ifdef HPPABSD_CORE
	&hppabsd_core_vec,
#endif
#ifdef IRIX_CORE
	&irix_core_vec,
#endif
#ifdef OSF_CORE
	&osf_core_vec,
#endif
#ifdef	TRAD_CORE
	&trad_core_vec,
#endif

#ifdef	PTRACE_CORE
	&ptrace_core_vec,
#endif

	NULL /* end of list marker */
};

/* bfd_default_vector[0] contains either the address of the default vector,
   if there is one, or zero if there isn't.  */

const bfd_target * const bfd_default_vector[] = {
#ifdef DEFAULT_VECTOR
	&DEFAULT_VECTOR,
#endif
	NULL
};

/* When there is an ambiguous match, bfd_check_format_matches puts the
   names of the matching targets in an array.  This variable is the maximum
   number of entries that the array could possibly need.  */
const size_t _bfd_target_vector_entries = sizeof(bfd_target_vector)/sizeof(*bfd_target_vector);

/*
FUNCTION
	bfd_find_target

SYNOPSIS
	const bfd_target *bfd_find_target(CONST char *target_name, bfd *abfd);

DESCRIPTION
	Return a pointer to the transfer vector for the object target
	named @var{target_name}.  If @var{target_name} is <<NULL>>, choose the
	one in the environment variable <<GNUTARGET>>; if that is null or not
	defined, then choose the first entry in the target list.
	Passing in the string "default" or setting the environment
	variable to "default" will cause the first entry in the target
	list to be returned, and "target_defaulted" will be set in the
	BFD.  This causes <<bfd_check_format>> to loop over all the
	targets to find the one that matches the file being read.   
*/

const bfd_target *
bfd_find_target (target_name, abfd)
     CONST char *target_name;
     bfd *abfd;
{
  const bfd_target * const *target;
  extern char *getenv ();
  CONST char *targname = (target_name ? target_name : 
			  (CONST char *) getenv ("GNUTARGET"));

  /* This is safe; the vector cannot be null */
  if (targname == NULL || !strcmp (targname, "default")) {
    abfd->target_defaulted = true;
    return abfd->xvec = bfd_target_vector[0];
  }

  abfd->target_defaulted = false;

  for (target = &bfd_target_vector[0]; *target != NULL; target++) {
    if (!strcmp (targname, (*target)->name))
      return abfd->xvec = *target;
  }

  bfd_set_error (bfd_error_invalid_target);
  return NULL;
}


/*
FUNCTION
	bfd_target_list

SYNOPSIS
	const char **bfd_target_list(void);

DESCRIPTION
	Return a freshly malloced NULL-terminated
	vector of the names of all the valid BFD targets. Do not
	modify the names.

*/

const char **
bfd_target_list ()
{
  int vec_length= 0;
#ifdef NATIVE_HPPAHPUX_COMPILER
  /* The native compiler on the HP9000/700 has a bug which causes it
     to loop endlessly when compiling this file.  This avoids it.  */
  volatile
#endif
    const bfd_target * const *target;
  CONST  char **name_list, **name_ptr;

  for (target = &bfd_target_vector[0]; *target != NULL; target++)
    vec_length++;

  name_ptr = name_list = (CONST char **)
    bfd_zmalloc ((vec_length + 1) * sizeof (char **));

  if (name_list == NULL) {
    bfd_set_error (bfd_error_no_memory);
    return NULL;
  }

  for (target = &bfd_target_vector[0]; *target != NULL; target++)
    *(name_ptr++) = (*target)->name;

  return name_list;
}
