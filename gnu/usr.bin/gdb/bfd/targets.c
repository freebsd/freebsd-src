/* Generic target-file-type support for the BFD library.
   Copyright 1990, 1991, 1992, 1993 Free Software Foundation, Inc.
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

	o First a BFD is created by calling the internal routine
	<<new_bfd>>, then <<bfd_find_target>> is called with the
	target string supplied to <<bfd_openr>> and the new BFD pointer. 

	o If a null target string was provided to <<bfd_find_target>>,
	it looks up the environment variable <<GNUTARGET>> and uses
	that as the target string. 

	o If the target string is still NULL, or the target string is
	<<default>>, then the first item in the target vector is used
	as the target type, and <<target_defaulted>> is set to
	cause <<bfd_check_format>> to loop through all the targets.
	@xref{bfd_target}.  @xref{Formats}.

	o Otherwise, the elements in the target vector are inspected
	one by one, until a match on target name is found. When found,
	that is used. 

	o Otherwise the error <<invalid_target>> is returned to
	<<bfd_openr>>.

	o <<bfd_openr>> attempts to open the file using
	<<bfd_open_file>>, and returns the BFD.

	Once the BFD has been opened and the target selected, the file
	format may be determined. This is done by calling
	<<bfd_check_format>> on the BFD with a suggested format. 
	If <<target_defaulted>> has been set, each possible target
	type is tried to see if it recognizes the specified format.  The
	routine returns <<true>> when the application guesses right.
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
	target. It includes things like its byte order, name, what
	routines to call to do various operations, etc.   

	Every BFD points to a target structure with its <<xvec>>
	member. 

	These macros are used to dispatch to functions through the
	bfd_target vector. They are used in a number of macros further
	down in @file{bfd.h}, and are also used when calling various
	routines by hand inside the BFD implementation.  The "arglist"
	argument must be parenthesized; it contains all the arguments
	to the called function. 

	They make the documentation (more) unpleasant to read, so if
	someone wants to fix this and not break the above, please do.

.#define BFD_SEND(bfd, message, arglist) \
.               ((*((bfd)->xvec->message)) arglist)

	For operations which index on the BFD format 

.#define BFD_SEND_FMT(bfd, message, arglist) \
.            (((bfd)->xvec->message[(int)((bfd)->format)]) arglist)

	This is the struct which defines the type of BFD this is.  The
	<<xvec>> member of the struct <<bfd>> itself points here.  Each
	module that implements access to a different target under BFD,
	defines one of these.


	FIXME, these names should be rationalised with the names of
	the entry points which call them. Too bad we can't have one
	macro to define them both! 

.typedef struct bfd_target
.{

Identifies the kind of target, eg SunOS4, Ultrix, etc.

.  char *name;

The "flavour" of a back end is a general indication about the contents
of a file.

.  enum target_flavour {
.    bfd_target_unknown_flavour,
.    bfd_target_aout_flavour,
.    bfd_target_coff_flavour,
.    bfd_target_ecoff_flavour,
.    bfd_target_elf_flavour,
.    bfd_target_ieee_flavour,
.    bfd_target_nlm_flavour,
.    bfd_target_oasys_flavour,
.    bfd_target_tekhex_flavour,
.    bfd_target_srec_flavour,
.    bfd_target_hppa_flavour} flavour;

The order of bytes within the data area of a file.

.  boolean byteorder_big_p;

The order of bytes within the header parts of a file.

.  boolean header_byteorder_big_p;

This is a mask of all the flags which an executable may have set -
from the set <<NO_FLAGS>>, <<HAS_RELOC>>, ...<<D_PAGED>>.

.  flagword object_flags;       

This is a mask of all the flags which a section may have set - from
the set <<SEC_NO_FLAGS>>, <<SEC_ALLOC>>, ...<<SET_NEVER_LOAD>>.

.  flagword section_flags;

The character normally found at the front of a symbol 
(if any), perhaps _.

.  char symbol_leading_char;

The pad character for filenames within an archive header.

.  char ar_pad_char;            

The maximum number of characters in an archive header.

.  unsigned short ar_max_namelen;

The minimum alignment restriction for any section.

.  unsigned int align_power_min;

Entries for byte swapping for data. These are different to the other
entry points, since they don't take BFD as first arg.  Certain other handlers
could do the same.

.  bfd_vma      (*bfd_getx64) PARAMS ((bfd_byte *));
.  bfd_signed_vma (*bfd_getx_signed_64) PARAMS ((bfd_byte *));
.  void         (*bfd_putx64) PARAMS ((bfd_vma, bfd_byte *));
.  bfd_vma      (*bfd_getx32) PARAMS ((bfd_byte *));
.  bfd_signed_vma (*bfd_getx_signed_32) PARAMS ((bfd_byte *));
.  void         (*bfd_putx32) PARAMS ((bfd_vma, bfd_byte *));
.  bfd_vma      (*bfd_getx16) PARAMS ((bfd_byte *));
.  bfd_signed_vma (*bfd_getx_signed_16) PARAMS ((bfd_byte *));
.  void         (*bfd_putx16) PARAMS ((bfd_vma, bfd_byte *));

Byte swapping for the headers

.  bfd_vma      (*bfd_h_getx64) PARAMS ((bfd_byte *));
.  bfd_signed_vma (*bfd_h_getx_signed_64) PARAMS ((bfd_byte *));
.  void         (*bfd_h_putx64) PARAMS ((bfd_vma, bfd_byte *));
.  bfd_vma      (*bfd_h_getx32) PARAMS ((bfd_byte *));
.  bfd_signed_vma (*bfd_h_getx_signed_32) PARAMS ((bfd_byte *));
.  void         (*bfd_h_putx32) PARAMS ((bfd_vma, bfd_byte *));
.  bfd_vma      (*bfd_h_getx16) PARAMS ((bfd_byte *));
.  bfd_signed_vma (*bfd_h_getx_signed_16) PARAMS ((bfd_byte *));
.  void         (*bfd_h_putx16) PARAMS ((bfd_vma, bfd_byte *));

Format dependent routines: these are vectors of entry points
within the target vector structure, one for each format to check.

Check the format of a file being read.  Return bfd_target * or zero. 

.  struct bfd_target * (*_bfd_check_format[bfd_type_end]) PARAMS ((bfd *));

Set the format of a file being written.  

.  boolean             (*_bfd_set_format[bfd_type_end]) PARAMS ((bfd *));

Write cached information into a file being written, at bfd_close. 

.  boolean             (*_bfd_write_contents[bfd_type_end]) PARAMS ((bfd *));

The following functions are defined in <<JUMP_TABLE>>. The idea is
that the back end writer of <<foo>> names all the routines
<<foo_>>@var{entry_point}, <<JUMP_TABLE>> will built the entries
in this structure in the right order.

Core file entry points

.  char *   (*_core_file_failing_command) PARAMS ((bfd *));
.  int      (*_core_file_failing_signal) PARAMS ((bfd *));
.  boolean  (*_core_file_matches_executable_p) PARAMS ((bfd *, bfd *));

Archive entry points

.  boolean  (*_bfd_slurp_armap) PARAMS ((bfd *));
.  boolean  (*_bfd_slurp_extended_name_table) PARAMS ((bfd *));
.  void     (*_bfd_truncate_arname) PARAMS ((bfd *, CONST char *, char *));
.  boolean  (*write_armap) PARAMS ((bfd *arch, 
.                              unsigned int elength,
.                              struct orl *map,
.                              unsigned int orl_count, 
.                              int stridx));

Standard stuff.

.  boolean       (*_close_and_cleanup) PARAMS ((bfd *));
.  boolean       (*_bfd_set_section_contents) PARAMS ((bfd *, sec_ptr, PTR,
.                                            file_ptr, bfd_size_type));
.  boolean       (*_bfd_get_section_contents) PARAMS ((bfd *, sec_ptr, PTR, 
.                                            file_ptr, bfd_size_type));
.  boolean       (*_new_section_hook) PARAMS ((bfd *, sec_ptr));

Symbols and relocations

.  unsigned int  (*_get_symtab_upper_bound) PARAMS ((bfd *));
.  unsigned int  (*_bfd_canonicalize_symtab) PARAMS ((bfd *,
.                                              struct symbol_cache_entry **));
.  unsigned int  (*_get_reloc_upper_bound) PARAMS ((bfd *, sec_ptr));
.  unsigned int  (*_bfd_canonicalize_reloc) PARAMS ((bfd *, sec_ptr, arelent **,
.                                              struct symbol_cache_entry **));
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

.  alent *    (*_get_lineno) PARAMS ((bfd *, struct symbol_cache_entry *));
.
.  boolean    (*_bfd_set_arch_mach) PARAMS ((bfd *, enum bfd_architecture,
.                    unsigned long));
.
.  bfd *      (*openr_next_archived_file) PARAMS ((bfd *arch, bfd *prev));
. 
.  boolean    (*_bfd_find_nearest_line) PARAMS ((bfd *abfd,
.                    struct sec *section, struct symbol_cache_entry **symbols,
.                    bfd_vma offset, CONST char **file, CONST char **func,
.                    unsigned int *line));
. 
.  int        (*_bfd_stat_arch_elt) PARAMS ((bfd *, struct stat *));
.
.  int        (*_bfd_sizeof_headers) PARAMS ((bfd *, boolean));
.
.  void       (*_bfd_debug_info_start) PARAMS ((bfd *));
.  void       (*_bfd_debug_info_end) PARAMS ((bfd *));
.  void       (*_bfd_debug_info_accumulate) PARAMS ((bfd *, struct sec *));
.
.  bfd_byte * (*_bfd_get_relocated_section_contents) PARAMS ((bfd *,
.                    struct bfd_seclet *, bfd_byte *data,
.                    boolean relocateable));
.
.  boolean    (*_bfd_relax_section) PARAMS ((bfd *, struct sec *,
.                    struct symbol_cache_entry **));
.
.  boolean    (*_bfd_seclet_link) PARAMS ((bfd *, PTR data,
.                     boolean relocateable));

. {* See documentation on reloc types.  *}
. CONST struct reloc_howto_struct *
.       (*reloc_type_lookup) PARAMS ((bfd *abfd,
.                                     bfd_reloc_code_real_type code));
.
. {* Back-door to allow format-aware applications to create debug symbols
.    while using BFD for everything else.  Currently used by the assembler
.    when creating COFF files.  *}
. asymbol *  (*_bfd_make_debug_symbol) PARAMS ((
.       bfd *abfd,
.       void *ptr,
.       unsigned long size));

Data for use by back-end routines, which isn't generic enough to belong
in this structure.

. PTR backend_data;
.} bfd_target;

*/

/* All known xvecs (even those that don't compile on all systems).
   Alphabetized for easy reference.
   They are listed a second time below, since
   we can't intermix extern's and initializers.  */
extern bfd_target a29kcoff_big_vec;
extern bfd_target a_out_adobe_vec;
extern bfd_target aout_mips_big_vec;
extern bfd_target aout_mips_little_vec;
extern bfd_target apollocoff_vec;
extern bfd_target b_out_vec_big_host;
extern bfd_target b_out_vec_little_host;
extern bfd_target bfd_elf32_big_generic_vec;
extern bfd_target bfd_elf32_bigmips_vec;
extern bfd_target bfd_elf32_hppa_vec;
extern bfd_target bfd_elf32_i386_vec;
extern bfd_target bfd_elf32_i860_vec;
extern bfd_target bfd_elf32_little_generic_vec;
extern bfd_target bfd_elf32_littlemips_vec;
extern bfd_target bfd_elf32_m68k_vec;
extern bfd_target bfd_elf32_m88k_vec;
extern bfd_target bfd_elf32_sparc_vec;
extern bfd_target bfd_elf64_big_generic_vec;
extern bfd_target bfd_elf64_little_generic_vec;
extern bfd_target demo_64_vec;
extern bfd_target ecoff_big_vec;
extern bfd_target ecoff_little_vec;
extern bfd_target ecoffalpha_little_vec;
extern bfd_target h8300coff_vec;
extern bfd_target h8500coff_vec;
extern bfd_target host_aout_vec;
extern bfd_target hp300bsd_vec;
extern bfd_target hp300hpux_vec;
extern bfd_target hppa_vec;
extern bfd_target i386aout_vec;
extern bfd_target i386bsd_vec;
extern bfd_target netbsd386_vec;
extern bfd_target freebsd386_vec;
extern bfd_target i386coff_vec;
extern bfd_target i386linux_vec;
extern bfd_target i386lynx_aout_vec;
extern bfd_target i386lynx_coff_vec;
extern bfd_target icoff_big_vec;
extern bfd_target icoff_little_vec;
extern bfd_target ieee_vec;
extern bfd_target m68kcoff_vec;
extern bfd_target m68kcoffun_vec;
extern bfd_target m68klynx_aout_vec;
extern bfd_target m68klynx_coff_vec;
extern bfd_target m88kbcs_vec;
extern bfd_target newsos3_vec;
extern bfd_target nlm32_big_generic_vec;
extern bfd_target nlm32_i386_vec;
extern bfd_target nlm32_little_generic_vec;
extern bfd_target nlm64_big_generic_vec;
extern bfd_target nlm64_little_generic_vec;
extern bfd_target oasys_vec;
extern bfd_target rs6000coff_vec;
extern bfd_target shcoff_vec;
extern bfd_target sunos_big_vec;
extern bfd_target tekhex_vec;
extern bfd_target we32kcoff_vec;
extern bfd_target z8kcoff_vec;

/* srec is always included.  */
extern bfd_target srec_vec;
extern bfd_target symbolsrec_vec;

/* All of the xvecs for core files.  */
extern bfd_target aix386_core_vec;
extern bfd_target hpux_core_vec;
extern bfd_target osf_core_vec;
extern bfd_target sco_core_vec;
extern bfd_target trad_core_vec;

bfd_target *target_vector[] = {

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
#if 0				/* No one seems to use this.  */
	&bfd_elf32_big_generic_vec,
	&bfd_elf32_bigmips_vec,
#endif
#if 0
	&bfd_elf32_hppa_vec,
#endif
	&bfd_elf32_i386_vec,
	&bfd_elf32_i860_vec,
#if 0				/* No one seems to use this.  */
	&bfd_elf32_little_generic_vec,
	&bfd_elf32_littlemips_vec,
#endif
	&bfd_elf32_m68k_vec,
	&bfd_elf32_m88k_vec,
	&bfd_elf32_sparc_vec,
#ifdef BFD64			/* No one seems to use this.  */
	&bfd_elf64_big_generic_vec,
	&bfd_elf64_little_generic_vec,
#endif
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
	&host_aout_vec,
#endif
#if 0				/* Clashes with sunos_big_vec magic no.  */
	&hp300bsd_vec,
#endif
	&hp300hpux_vec,
#if defined (HOST_HPPAHPUX) || defined (HOST_HPPABSD)
        &hppa_vec,
#endif
	&i386aout_vec,
	&i386bsd_vec,
	&netbsd386_vec,
	&freebsd386_vec,
	&i386coff_vec,
#if 0
	&i386linux_vec,
#endif
	&i386lynx_aout_vec,
	&i386lynx_coff_vec,
	&icoff_big_vec,
	&icoff_little_vec,
	&ieee_vec,
	&m68kcoff_vec,
	&m68kcoffun_vec,
	&m68klynx_aout_vec,
	&m68klynx_coff_vec,
	&m88kbcs_vec,
	&newsos3_vec,
#if 0				/* No one seems to use this.  */
	&nlm32_big_generic_vec,
#endif
	&nlm32_i386_vec,
#if 0				/* No one seems to use this.  */
	&nlm32_little_generic_vec,
#endif
#ifdef BFD64
	&nlm64_big_generic_vec,
	&nlm64_little_generic_vec,
#endif
#if 0
	/* We have no oasys tools anymore, so we can't test any of this
	   anymore. If you want to test the stuff yourself, go ahead...
	   steve@cygnus.com
	   Worse, since there is no magic number for archives, there
	   can be annoying target mis-matches.  */
	&oasys_vec,
#endif
	&rs6000coff_vec,
	&shcoff_vec,
	&sunos_big_vec,
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
#ifdef OSF_CORE
	&osf_core_vec,
#endif
#ifdef	SCO_CORE
	&sco_core_vec,
#endif
#ifdef	TRAD_CORE
	&trad_core_vec,
#endif

	NULL /* end of list marker */
};

/* default_vector[0] contains either the address of the default vector,
   if there is one, or zero if there isn't.  */

bfd_target *default_vector[] = {
#ifdef DEFAULT_VECTOR
	&DEFAULT_VECTOR,
#endif
	NULL
};




/*
FUNCTION
	bfd_find_target

DESCRIPTION
	Returns a pointer to the transfer vector for the object target
	named target_name.  If target_name is NULL, chooses the one in
	the environment variable GNUTARGET; if that is null or not
	defined thenthe first entry in the target list is chosen.
	Passing in the string "default" or setting the environment
	variable to "default" will cause the first entry in the target
	list to be returned, and "target_defaulted" will be set in the
	BFD.  This causes <<bfd_check_format>> to loop over all the
	targets to find the one that matches the file being read.   

SYNOPSIS
	bfd_target *bfd_find_target(CONST char *, bfd *);
*/

bfd_target *
DEFUN(bfd_find_target,(target_name, abfd),
      CONST char *target_name AND
      bfd *abfd)
{
  bfd_target **target;
  extern char *getenv ();
  CONST char *targname = (target_name ? target_name : 
			  (CONST char *) getenv ("GNUTARGET"));

  /* This is safe; the vector cannot be null */
  if (targname == NULL || !strcmp (targname, "default")) {
    abfd->target_defaulted = true;
    return abfd->xvec = target_vector[0];
  }

  abfd->target_defaulted = false;

  for (target = &target_vector[0]; *target != NULL; target++) {
    if (!strcmp (targname, (*target)->name))
      return abfd->xvec = *target;
  }

  bfd_error = invalid_target;
  return NULL;
}


/*
FUNCTION
	bfd_target_list

DESCRIPTION
	This function returns a freshly malloced NULL-terminated
	vector of the names of all the valid BFD targets. Do not
	modify the names 

SYNOPSIS
	CONST char **bfd_target_list(void);

*/

CONST char **
DEFUN_VOID(bfd_target_list)
{
  int vec_length= 0;
#ifdef NATIVE_HPPAHPUX_COMPILER
  /* The native compiler on the HP9000/700 has a bug which causes it
     to loop endlessly when compiling this file.  This avoids it.  */
  volatile
#endif
    bfd_target **target;
  CONST  char **name_list, **name_ptr;

  for (target = &target_vector[0]; *target != NULL; target++)
    vec_length++;

  name_ptr = 
    name_list = (CONST char **) zalloc ((vec_length + 1) * sizeof (char **));

  if (name_list == NULL) {
    bfd_error = no_memory;
    return NULL;
  }

  for (target = &target_vector[0]; *target != NULL; target++)
    *(name_ptr++) = (*target)->name;

  return name_list;
}
