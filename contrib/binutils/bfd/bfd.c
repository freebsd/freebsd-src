/* Generic BFD library interface and support routines.
   Copyright 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999,
   2000, 2001, 2002
   Free Software Foundation, Inc.
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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/*
SECTION
	<<typedef bfd>>

	A BFD has type <<bfd>>; objects of this type are the
	cornerstone of any application using BFD. Using BFD
	consists of making references though the BFD and to data in the BFD.

	Here is the structure that defines the type <<bfd>>.  It
	contains the major data about the file and pointers
	to the rest of the data.

CODE_FRAGMENT
.
.struct _bfd
.{
.  {* The filename the application opened the BFD with.  *}
.  const char *filename;
.
.  {* A pointer to the target jump table.  *}
.  const struct bfd_target *xvec;
.
.  {* To avoid dragging too many header files into every file that
.     includes `<<bfd.h>>', IOSTREAM has been declared as a "char *",
.     and MTIME as a "long".  Their correct types, to which they
.     are cast when used, are "FILE *" and "time_t".    The iostream
.     is the result of an fopen on the filename.  However, if the
.     BFD_IN_MEMORY flag is set, then iostream is actually a pointer
.     to a bfd_in_memory struct.  *}
.  PTR iostream;
.
.  {* Is the file descriptor being cached?  That is, can it be closed as
.     needed, and re-opened when accessed later?  *}
.  boolean cacheable;
.
.  {* Marks whether there was a default target specified when the
.     BFD was opened. This is used to select which matching algorithm
.     to use to choose the back end.  *}
.  boolean target_defaulted;
.
.  {* The caching routines use these to maintain a
.     least-recently-used list of BFDs.  *}
.  struct _bfd *lru_prev, *lru_next;
.
.  {* When a file is closed by the caching routines, BFD retains
.     state information on the file here...  *}
.  ufile_ptr where;
.
.  {* ... and here: (``once'' means at least once).  *}
.  boolean opened_once;
.
.  {* Set if we have a locally maintained mtime value, rather than
.     getting it from the file each time.  *}
.  boolean mtime_set;
.
.  {* File modified time, if mtime_set is true.  *}
.  long mtime;
.
.  {* Reserved for an unimplemented file locking extension.  *}
.  int ifd;
.
.  {* The format which belongs to the BFD. (object, core, etc.)  *}
.  bfd_format format;
.
.  {* The direction with which the BFD was opened.  *}
.  enum bfd_direction
.    {
.      no_direction = 0,
.      read_direction = 1,
.      write_direction = 2,
.      both_direction = 3
.    }
.  direction;
.
.  {* Format_specific flags.  *}
.  flagword flags;
.
.  {* Currently my_archive is tested before adding origin to
.     anything. I believe that this can become always an add of
.     origin, with origin set to 0 for non archive files.  *}
.  ufile_ptr origin;
.
.  {* Remember when output has begun, to stop strange things
.     from happening.  *}
.  boolean output_has_begun;
.
.  {* A hash table for section names.  *}
.  struct bfd_hash_table section_htab;
.
.  {* Pointer to linked list of sections.  *}
.  struct sec *sections;
.
.  {* The place where we add to the section list.  *}
.  struct sec **section_tail;
.
.  {* The number of sections.  *}
.  unsigned int section_count;
.
.  {* Stuff only useful for object files:
.     The start address.  *}
.  bfd_vma start_address;
.
.  {* Used for input and output.  *}
.  unsigned int symcount;
.
.  {* Symbol table for output BFD (with symcount entries).  *}
.  struct symbol_cache_entry  **outsymbols;
.
.  {* Used for slurped dynamic symbol tables.  *}
.  unsigned int dynsymcount;
.
.  {* Pointer to structure which contains architecture information.  *}
.  const struct bfd_arch_info *arch_info;
.
.  {* Stuff only useful for archives.  *}
.  PTR arelt_data;
.  struct _bfd *my_archive;     {* The containing archive BFD.  *}
.  struct _bfd *next;           {* The next BFD in the archive.  *}
.  struct _bfd *archive_head;   {* The first BFD in the archive.  *}
.  boolean has_armap;
.
.  {* A chain of BFD structures involved in a link.  *}
.  struct _bfd *link_next;
.
.  {* A field used by _bfd_generic_link_add_archive_symbols.  This will
.     be used only for archive elements.  *}
.  int archive_pass;
.
.  {* Used by the back end to hold private data.  *}
.  union
.    {
.      struct aout_data_struct *aout_data;
.      struct artdata *aout_ar_data;
.      struct _oasys_data *oasys_obj_data;
.      struct _oasys_ar_data *oasys_ar_data;
.      struct coff_tdata *coff_obj_data;
.      struct pe_tdata *pe_obj_data;
.      struct xcoff_tdata *xcoff_obj_data;
.      struct ecoff_tdata *ecoff_obj_data;
.      struct ieee_data_struct *ieee_data;
.      struct ieee_ar_data_struct *ieee_ar_data;
.      struct srec_data_struct *srec_data;
.      struct ihex_data_struct *ihex_data;
.      struct tekhex_data_struct *tekhex_data;
.      struct elf_obj_tdata *elf_obj_data;
.      struct nlm_obj_tdata *nlm_obj_data;
.      struct bout_data_struct *bout_data;
.      struct mmo_data_struct *mmo_data;
.      struct sun_core_struct *sun_core_data;
.      struct sco5_core_struct *sco5_core_data;
.      struct trad_core_struct *trad_core_data;
.      struct som_data_struct *som_data;
.      struct hpux_core_struct *hpux_core_data;
.      struct hppabsd_core_struct *hppabsd_core_data;
.      struct sgi_core_struct *sgi_core_data;
.      struct lynx_core_struct *lynx_core_data;
.      struct osf_core_struct *osf_core_data;
.      struct cisco_core_struct *cisco_core_data;
.      struct versados_data_struct *versados_data;
.      struct netbsd_core_struct *netbsd_core_data;
.      PTR any;
.    }
.  tdata;
.
.  {* Used by the application to hold private data.  *}
.  PTR usrdata;
.
.  {* Where all the allocated stuff under this BFD goes.  This is a
.     struct objalloc *, but we use PTR to avoid requiring the inclusion of
.     objalloc.h.  *}
.  PTR memory;
.};
.
*/

#include "bfd.h"
#include "sysdep.h"

#ifdef ANSI_PROTOTYPES
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#include "libiberty.h"
#include "safe-ctype.h"
#include "bfdlink.h"
#include "libbfd.h"
#include "coff/internal.h"
#include "coff/sym.h"
#include "libcoff.h"
#include "libecoff.h"
#undef obj_symbols
#include "elf-bfd.h"

/* provide storage for subsystem, stack and heap data which may have been
   passed in on the command line.  Ld puts this data into a bfd_link_info
   struct which ultimately gets passed in to the bfd.  When it arrives, copy
   it to the following struct so that the data will be available in coffcode.h
   where it is needed.  The typedef's used are defined in bfd.h */

/*
SECTION
	Error reporting

	Most BFD functions return nonzero on success (check their
	individual documentation for precise semantics).  On an error,
	they call <<bfd_set_error>> to set an error condition that callers
	can check by calling <<bfd_get_error>>.
        If that returns <<bfd_error_system_call>>, then check
	<<errno>>.

	The easiest way to report a BFD error to the user is to
	use <<bfd_perror>>.

SUBSECTION
	Type <<bfd_error_type>>

	The values returned by <<bfd_get_error>> are defined by the
	enumerated type <<bfd_error_type>>.

CODE_FRAGMENT
.
.typedef enum bfd_error
.{
.  bfd_error_no_error = 0,
.  bfd_error_system_call,
.  bfd_error_invalid_target,
.  bfd_error_wrong_format,
.  bfd_error_wrong_object_format,
.  bfd_error_invalid_operation,
.  bfd_error_no_memory,
.  bfd_error_no_symbols,
.  bfd_error_no_armap,
.  bfd_error_no_more_archived_files,
.  bfd_error_malformed_archive,
.  bfd_error_file_not_recognized,
.  bfd_error_file_ambiguously_recognized,
.  bfd_error_no_contents,
.  bfd_error_nonrepresentable_section,
.  bfd_error_no_debug_section,
.  bfd_error_bad_value,
.  bfd_error_file_truncated,
.  bfd_error_file_too_big,
.  bfd_error_invalid_error_code
.}
.bfd_error_type;
.
*/

static bfd_error_type bfd_error = bfd_error_no_error;

const char *const bfd_errmsgs[] =
{
  N_("No error"),
  N_("System call error"),
  N_("Invalid bfd target"),
  N_("File in wrong format"),
  N_("Archive object file in wrong format"),
  N_("Invalid operation"),
  N_("Memory exhausted"),
  N_("No symbols"),
  N_("Archive has no index; run ranlib to add one"),
  N_("No more archived files"),
  N_("Malformed archive"),
  N_("File format not recognized"),
  N_("File format is ambiguous"),
  N_("Section has no contents"),
  N_("Nonrepresentable section on output"),
  N_("Symbol needs debug section which does not exist"),
  N_("Bad value"),
  N_("File truncated"),
  N_("File too big"),
  N_("#<Invalid error code>")
};

/*
FUNCTION
	bfd_get_error

SYNOPSIS
	bfd_error_type bfd_get_error (void);

DESCRIPTION
	Return the current BFD error condition.
*/

bfd_error_type
bfd_get_error ()
{
  return bfd_error;
}

/*
FUNCTION
	bfd_set_error

SYNOPSIS
	void bfd_set_error (bfd_error_type error_tag);

DESCRIPTION
	Set the BFD error condition to be @var{error_tag}.
*/

void
bfd_set_error (error_tag)
     bfd_error_type error_tag;
{
  bfd_error = error_tag;
}

/*
FUNCTION
	bfd_errmsg

SYNOPSIS
	const char *bfd_errmsg (bfd_error_type error_tag);

DESCRIPTION
	Return a string describing the error @var{error_tag}, or
	the system error if @var{error_tag} is <<bfd_error_system_call>>.
*/

const char *
bfd_errmsg (error_tag)
     bfd_error_type error_tag;
{
#ifndef errno
  extern int errno;
#endif
  if (error_tag == bfd_error_system_call)
    return xstrerror (errno);

  if ((((int) error_tag < (int) bfd_error_no_error) ||
       ((int) error_tag > (int) bfd_error_invalid_error_code)))
    error_tag = bfd_error_invalid_error_code;/* sanity check */

  return _(bfd_errmsgs [(int)error_tag]);
}

/*
FUNCTION
	bfd_perror

SYNOPSIS
	void bfd_perror (const char *message);

DESCRIPTION
	Print to the standard error stream a string describing the
	last BFD error that occurred, or the last system error if
	the last BFD error was a system call failure.  If @var{message}
	is non-NULL and non-empty, the error string printed is preceded
	by @var{message}, a colon, and a space.  It is followed by a newline.
*/

void
bfd_perror (message)
     const char *message;
{
  if (bfd_get_error () == bfd_error_system_call)
    /* Must be a system error then.  */
    perror ((char *)message);
  else
    {
      if (message == NULL || *message == '\0')
	fprintf (stderr, "%s\n", bfd_errmsg (bfd_get_error ()));
      else
	fprintf (stderr, "%s: %s\n", message, bfd_errmsg (bfd_get_error ()));
    }
}

/*
SUBSECTION
	BFD error handler

	Some BFD functions want to print messages describing the
	problem.  They call a BFD error handler function.  This
	function may be overriden by the program.

	The BFD error handler acts like printf.

CODE_FRAGMENT
.
.typedef void (*bfd_error_handler_type) PARAMS ((const char *, ...));
.
*/

/* The program name used when printing BFD error messages.  */

static const char *_bfd_error_program_name;

/* This is the default routine to handle BFD error messages.  */

static void _bfd_default_error_handler PARAMS ((const char *s, ...));

static void
_bfd_default_error_handler VPARAMS ((const char *s, ...))
{
  if (_bfd_error_program_name != NULL)
    fprintf (stderr, "%s: ", _bfd_error_program_name);
  else
    fprintf (stderr, "BFD: ");

  VA_OPEN (p, s);
  VA_FIXEDARG (p, const char *, s);
  vfprintf (stderr, s, p);
  VA_CLOSE (p);

  fprintf (stderr, "\n");
}

/* This is a function pointer to the routine which should handle BFD
   error messages.  It is called when a BFD routine encounters an
   error for which it wants to print a message.  Going through a
   function pointer permits a program linked against BFD to intercept
   the messages and deal with them itself.  */

bfd_error_handler_type _bfd_error_handler = _bfd_default_error_handler;

/*
FUNCTION
	bfd_set_error_handler

SYNOPSIS
	bfd_error_handler_type bfd_set_error_handler (bfd_error_handler_type);

DESCRIPTION
	Set the BFD error handler function.  Returns the previous
	function.
*/

bfd_error_handler_type
bfd_set_error_handler (pnew)
     bfd_error_handler_type pnew;
{
  bfd_error_handler_type pold;

  pold = _bfd_error_handler;
  _bfd_error_handler = pnew;
  return pold;
}

/*
FUNCTION
	bfd_set_error_program_name

SYNOPSIS
	void bfd_set_error_program_name (const char *);

DESCRIPTION
	Set the program name to use when printing a BFD error.  This
	is printed before the error message followed by a colon and
	space.  The string must not be changed after it is passed to
	this function.
*/

void
bfd_set_error_program_name (name)
     const char *name;
{
  _bfd_error_program_name = name;
}

/*
FUNCTION
	bfd_get_error_handler

SYNOPSIS
	bfd_error_handler_type bfd_get_error_handler (void);

DESCRIPTION
	Return the BFD error handler function.
*/

bfd_error_handler_type
bfd_get_error_handler ()
{
  return _bfd_error_handler;
}

/*
FUNCTION
	bfd_archive_filename

SYNOPSIS
	const char *bfd_archive_filename (bfd *);

DESCRIPTION
	For a BFD that is a component of an archive, returns a string
	with both the archive name and file name.  For other BFDs, just
	returns the file name.
*/

const char *
bfd_archive_filename (abfd)
     bfd *abfd;
{
  if (abfd->my_archive)
    {
      static size_t curr = 0;
      static char *buf;
      size_t needed;

      needed = (strlen (bfd_get_filename (abfd->my_archive))
		+ strlen (bfd_get_filename (abfd)) + 3);
      if (needed > curr)
	{
	  if (curr)
	    free (buf);
	  curr = needed + (needed >> 1);
	  buf = bfd_malloc ((bfd_size_type) curr);
	  /* If we can't malloc, fail safe by returning just the file
	     name. This function is only used when building error
	     messages.  */
	  if (!buf)
	    {
	      curr = 0;
	      return bfd_get_filename (abfd);
	    }
	}
      sprintf (buf, "%s(%s)", bfd_get_filename (abfd->my_archive),
	       bfd_get_filename (abfd));
      return buf;
    }
  else
    return bfd_get_filename (abfd);
}

/*
SECTION
	Symbols
*/

/*
FUNCTION
	bfd_get_reloc_upper_bound

SYNOPSIS
	long bfd_get_reloc_upper_bound(bfd *abfd, asection *sect);

DESCRIPTION
	Return the number of bytes required to store the
	relocation information associated with section @var{sect}
	attached to bfd @var{abfd}.  If an error occurs, return -1.

*/

long
bfd_get_reloc_upper_bound (abfd, asect)
     bfd *abfd;
     sec_ptr asect;
{
  if (abfd->format != bfd_object)
    {
      bfd_set_error (bfd_error_invalid_operation);
      return -1;
    }

  return BFD_SEND (abfd, _get_reloc_upper_bound, (abfd, asect));
}

/*
FUNCTION
	bfd_canonicalize_reloc

SYNOPSIS
	long bfd_canonicalize_reloc
        	(bfd *abfd,
		asection *sec,
		arelent **loc,
		asymbol	**syms);

DESCRIPTION
	Call the back end associated with the open BFD
	@var{abfd} and translate the external form of the relocation
	information attached to @var{sec} into the internal canonical
	form.  Place the table into memory at @var{loc}, which has
	been preallocated, usually by a call to
	<<bfd_get_reloc_upper_bound>>.  Returns the number of relocs, or
	-1 on error.

	The @var{syms} table is also needed for horrible internal magic
	reasons.

*/
long
bfd_canonicalize_reloc (abfd, asect, location, symbols)
     bfd *abfd;
     sec_ptr asect;
     arelent **location;
     asymbol **symbols;
{
  if (abfd->format != bfd_object)
    {
      bfd_set_error (bfd_error_invalid_operation);
      return -1;
    }

  return BFD_SEND (abfd, _bfd_canonicalize_reloc,
		   (abfd, asect, location, symbols));
}

/*
FUNCTION
	bfd_set_reloc

SYNOPSIS
	void bfd_set_reloc
	  (bfd *abfd, asection *sec, arelent **rel, unsigned int count);

DESCRIPTION
	Set the relocation pointer and count within
	section @var{sec} to the values @var{rel} and @var{count}.
	The argument @var{abfd} is ignored.

*/

void
bfd_set_reloc (ignore_abfd, asect, location, count)
     bfd *ignore_abfd ATTRIBUTE_UNUSED;
     sec_ptr asect;
     arelent **location;
     unsigned int count;
{
  asect->orelocation = location;
  asect->reloc_count = count;
}

/*
FUNCTION
	bfd_set_file_flags

SYNOPSIS
	boolean bfd_set_file_flags(bfd *abfd, flagword flags);

DESCRIPTION
	Set the flag word in the BFD @var{abfd} to the value @var{flags}.

	Possible errors are:
	o <<bfd_error_wrong_format>> - The target bfd was not of object format.
	o <<bfd_error_invalid_operation>> - The target bfd was open for reading.
	o <<bfd_error_invalid_operation>> -
	The flag word contained a bit which was not applicable to the
	type of file.  E.g., an attempt was made to set the <<D_PAGED>> bit
	on a BFD format which does not support demand paging.

*/

boolean
bfd_set_file_flags (abfd, flags)
     bfd *abfd;
     flagword flags;
{
  if (abfd->format != bfd_object)
    {
      bfd_set_error (bfd_error_wrong_format);
      return false;
    }

  if (bfd_read_p (abfd))
    {
      bfd_set_error (bfd_error_invalid_operation);
      return false;
    }

  bfd_get_file_flags (abfd) = flags;
  if ((flags & bfd_applicable_file_flags (abfd)) != flags)
    {
      bfd_set_error (bfd_error_invalid_operation);
      return false;
    }

  return true;
}

void
bfd_assert (file, line)
     const char *file;
     int line;
{
  (*_bfd_error_handler) (_("BFD %s assertion fail %s:%d"),
			 BFD_VERSION_STRING, file, line);
}

/* A more or less friendly abort message.  In libbfd.h abort is
   defined to call this function.  */

#ifndef EXIT_FAILURE
#define EXIT_FAILURE 1
#endif

void
_bfd_abort (file, line, fn)
     const char *file;
     int line;
     const char *fn;
{
  if (fn != NULL)
    (*_bfd_error_handler)
      (_("BFD %s internal error, aborting at %s line %d in %s\n"),
       BFD_VERSION_STRING, file, line, fn);
  else
    (*_bfd_error_handler)
      (_("BFD %s internal error, aborting at %s line %d\n"),
       BFD_VERSION_STRING, file, line);
  (*_bfd_error_handler) (_("Please report this bug.\n"));
  xexit (EXIT_FAILURE);
}

/*
FUNCTION
	bfd_get_arch_size

SYNOPSIS
 	int bfd_get_arch_size (bfd *abfd);

DESCRIPTION
	Returns the architecture address size, in bits, as determined
	by the object file's format.  For ELF, this information is
	included in the header.

RETURNS
	Returns the arch size in bits if known, <<-1>> otherwise.
*/

int
bfd_get_arch_size (abfd)
     bfd *abfd;
{
  if (abfd->xvec->flavour == bfd_target_elf_flavour)
    return (get_elf_backend_data (abfd))->s->arch_size;

  return -1;
}

/*
FUNCTION
	bfd_get_sign_extend_vma

SYNOPSIS
 	int bfd_get_sign_extend_vma (bfd *abfd);

DESCRIPTION
	Indicates if the target architecture "naturally" sign extends
	an address.  Some architectures implicitly sign extend address
	values when they are converted to types larger than the size
	of an address.  For instance, bfd_get_start_address() will
	return an address sign extended to fill a bfd_vma when this is
	the case.

RETURNS
	Returns <<1>> if the target architecture is known to sign
	extend addresses, <<0>> if the target architecture is known to
	not sign extend addresses, and <<-1>> otherwise.
*/

int
bfd_get_sign_extend_vma (abfd)
     bfd *abfd;
{
  char *name;

  if (bfd_get_flavour (abfd) == bfd_target_elf_flavour)
    return (get_elf_backend_data (abfd)->sign_extend_vma);

  name = bfd_get_target (abfd);

  /* Return a proper value for DJGPP COFF (an x86 COFF variant).
     This function is required for DWARF2 support, but there is
     no place to store this information in the COFF back end.
     Should enough other COFF targets add support for DWARF2,
     a place will have to be found.  Until then, this hack will do.  */
  if (strncmp (name, "coff-go32", sizeof ("coff-go32") - 1) == 0)
    return 1;

  bfd_set_error (bfd_error_wrong_format);
  return -1;
}

/*
FUNCTION
	bfd_set_start_address

SYNOPSIS
 	boolean bfd_set_start_address(bfd *abfd, bfd_vma vma);

DESCRIPTION
	Make @var{vma} the entry point of output BFD @var{abfd}.

RETURNS
	Returns <<true>> on success, <<false>> otherwise.
*/

boolean
bfd_set_start_address (abfd, vma)
     bfd *abfd;
     bfd_vma vma;
{
  abfd->start_address = vma;
  return true;
}

/*
FUNCTION
	bfd_get_mtime

SYNOPSIS
	long bfd_get_mtime(bfd *abfd);

DESCRIPTION
	Return the file modification time (as read from the file system, or
	from the archive header for archive members).

*/

long
bfd_get_mtime (abfd)
     bfd *abfd;
{
  FILE *fp;
  struct stat buf;

  if (abfd->mtime_set)
    return abfd->mtime;

  fp = bfd_cache_lookup (abfd);
  if (0 != fstat (fileno (fp), &buf))
    return 0;

  abfd->mtime = buf.st_mtime;		/* Save value in case anyone wants it */
  return buf.st_mtime;
}

/*
FUNCTION
	bfd_get_size

SYNOPSIS
	long bfd_get_size(bfd *abfd);

DESCRIPTION
	Return the file size (as read from file system) for the file
	associated with BFD @var{abfd}.

	The initial motivation for, and use of, this routine is not
	so we can get the exact size of the object the BFD applies to, since
	that might not be generally possible (archive members for example).
	It would be ideal if someone could eventually modify
	it so that such results were guaranteed.

	Instead, we want to ask questions like "is this NNN byte sized
	object I'm about to try read from file offset YYY reasonable?"
	As as example of where we might do this, some object formats
	use string tables for which the first <<sizeof (long)>> bytes of the
	table contain the size of the table itself, including the size bytes.
	If an application tries to read what it thinks is one of these
	string tables, without some way to validate the size, and for
	some reason the size is wrong (byte swapping error, wrong location
	for the string table, etc.), the only clue is likely to be a read
	error when it tries to read the table, or a "virtual memory
	exhausted" error when it tries to allocate 15 bazillon bytes
	of space for the 15 bazillon byte table it is about to read.
	This function at least allows us to answer the quesion, "is the
	size reasonable?".
*/

long
bfd_get_size (abfd)
     bfd *abfd;
{
  FILE *fp;
  struct stat buf;

  if ((abfd->flags & BFD_IN_MEMORY) != 0)
    return ((struct bfd_in_memory *) abfd->iostream)->size;

  fp = bfd_cache_lookup (abfd);
  if (0 != fstat (fileno (fp), & buf))
    return 0;

  return buf.st_size;
}

/*
FUNCTION
	bfd_get_gp_size

SYNOPSIS
	unsigned int bfd_get_gp_size(bfd *abfd);

DESCRIPTION
	Return the maximum size of objects to be optimized using the GP
	register under MIPS ECOFF.  This is typically set by the <<-G>>
	argument to the compiler, assembler or linker.
*/

unsigned int
bfd_get_gp_size (abfd)
     bfd *abfd;
{
  if (abfd->format == bfd_object)
    {
      if (abfd->xvec->flavour == bfd_target_ecoff_flavour)
	return ecoff_data (abfd)->gp_size;
      else if (abfd->xvec->flavour == bfd_target_elf_flavour)
	return elf_gp_size (abfd);
    }
  return 0;
}

/*
FUNCTION
	bfd_set_gp_size

SYNOPSIS
	void bfd_set_gp_size(bfd *abfd, unsigned int i);

DESCRIPTION
	Set the maximum size of objects to be optimized using the GP
	register under ECOFF or MIPS ELF.  This is typically set by
	the <<-G>> argument to the compiler, assembler or linker.
*/

void
bfd_set_gp_size (abfd, i)
     bfd *abfd;
     unsigned int i;
{
  /* Don't try to set GP size on an archive or core file!  */
  if (abfd->format != bfd_object)
    return;

  if (abfd->xvec->flavour == bfd_target_ecoff_flavour)
    ecoff_data (abfd)->gp_size = i;
  else if (abfd->xvec->flavour == bfd_target_elf_flavour)
    elf_gp_size (abfd) = i;
}

/* Get the GP value.  This is an internal function used by some of the
   relocation special_function routines on targets which support a GP
   register.  */

bfd_vma
_bfd_get_gp_value (abfd)
     bfd *abfd;
{
  if (abfd->format != bfd_object)
    return 0;

  if (abfd->xvec->flavour == bfd_target_ecoff_flavour)
    return ecoff_data (abfd)->gp;
  else if (abfd->xvec->flavour == bfd_target_elf_flavour)
    return elf_gp (abfd);

  return 0;
}

/* Set the GP value.  */

void
_bfd_set_gp_value (abfd, v)
     bfd *abfd;
     bfd_vma v;
{
  if (abfd->format != bfd_object)
    return;

  if (abfd->xvec->flavour == bfd_target_ecoff_flavour)
    ecoff_data (abfd)->gp = v;
  else if (abfd->xvec->flavour == bfd_target_elf_flavour)
    elf_gp (abfd) = v;
}

/*
FUNCTION
	bfd_scan_vma

SYNOPSIS
	bfd_vma bfd_scan_vma(const char *string, const char **end, int base);

DESCRIPTION
	Convert, like <<strtoul>>, a numerical expression
	@var{string} into a <<bfd_vma>> integer, and return that integer.
	(Though without as many bells and whistles as <<strtoul>>.)
	The expression is assumed to be unsigned (i.e., positive).
	If given a @var{base}, it is used as the base for conversion.
	A base of 0 causes the function to interpret the string
	in hex if a leading "0x" or "0X" is found, otherwise
	in octal if a leading zero is found, otherwise in decimal.

	If the value would overflow, the maximum <<bfd_vma>> value is
	returned.
*/

bfd_vma
bfd_scan_vma (string, end, base)
     const char *string;
     const char **end;
     int base;
{
  bfd_vma value;
  bfd_vma cutoff;
  unsigned int cutlim;
  int overflow;

  /* Let the host do it if possible.  */
  if (sizeof (bfd_vma) <= sizeof (unsigned long))
    return (bfd_vma) strtoul (string, (char **) end, base);

  if (base == 0)
    {
      if (string[0] == '0')
	{
	  if ((string[1] == 'x') || (string[1] == 'X'))
	    base = 16;
	  else
	    base = 8;
	}
    }

  if ((base < 2) || (base > 36))
    base = 10;

  if (base == 16
      && string[0] == '0'
      && (string[1] == 'x' || string[1] == 'X')
      && ISXDIGIT (string[2]))
    {
      string += 2;
    }

  cutoff = (~ (bfd_vma) 0) / (bfd_vma) base;
  cutlim = (~ (bfd_vma) 0) % (bfd_vma) base;
  value = 0;
  overflow = 0;
  while (1)
    {
      unsigned int digit;

      digit = *string;
      if (ISDIGIT (digit))
	digit = digit - '0';
      else if (ISALPHA (digit))
	digit = TOUPPER (digit) - 'A' + 10;
      else
	break;
      if (digit >= (unsigned int) base)
	break;
      if (value > cutoff || (value == cutoff && digit > cutlim))
	overflow = 1;
      value = value * base + digit;
      ++string;
    }

  if (overflow)
    value = ~ (bfd_vma) 0;

  if (end != NULL)
    *end = string;

  return value;
}

/*
FUNCTION
	bfd_copy_private_bfd_data

SYNOPSIS
	boolean bfd_copy_private_bfd_data(bfd *ibfd, bfd *obfd);

DESCRIPTION
	Copy private BFD information from the BFD @var{ibfd} to the
	the BFD @var{obfd}.  Return <<true>> on success, <<false>> on error.
	Possible error returns are:

	o <<bfd_error_no_memory>> -
	Not enough memory exists to create private data for @var{obfd}.

.#define bfd_copy_private_bfd_data(ibfd, obfd) \
.     BFD_SEND (obfd, _bfd_copy_private_bfd_data, \
.		(ibfd, obfd))

*/

/*
FUNCTION
	bfd_merge_private_bfd_data

SYNOPSIS
	boolean bfd_merge_private_bfd_data(bfd *ibfd, bfd *obfd);

DESCRIPTION
	Merge private BFD information from the BFD @var{ibfd} to the
	the output file BFD @var{obfd} when linking.  Return <<true>>
	on success, <<false>> on error.  Possible error returns are:

	o <<bfd_error_no_memory>> -
	Not enough memory exists to create private data for @var{obfd}.

.#define bfd_merge_private_bfd_data(ibfd, obfd) \
.     BFD_SEND (obfd, _bfd_merge_private_bfd_data, \
.		(ibfd, obfd))

*/

/*
FUNCTION
	bfd_set_private_flags

SYNOPSIS
	boolean bfd_set_private_flags(bfd *abfd, flagword flags);

DESCRIPTION
	Set private BFD flag information in the BFD @var{abfd}.
	Return <<true>> on success, <<false>> on error.  Possible error
	returns are:

	o <<bfd_error_no_memory>> -
	Not enough memory exists to create private data for @var{obfd}.

.#define bfd_set_private_flags(abfd, flags) \
.     BFD_SEND (abfd, _bfd_set_private_flags, \
.		(abfd, flags))

*/

/*
FUNCTION
	stuff

DESCRIPTION
	Stuff which should be documented:

.#define bfd_sizeof_headers(abfd, reloc) \
.     BFD_SEND (abfd, _bfd_sizeof_headers, (abfd, reloc))
.
.#define bfd_find_nearest_line(abfd, sec, syms, off, file, func, line) \
.     BFD_SEND (abfd, _bfd_find_nearest_line,  (abfd, sec, syms, off, file, func, line))
.
.       {* Do these three do anything useful at all, for any back end?  *}
.#define bfd_debug_info_start(abfd) \
.        BFD_SEND (abfd, _bfd_debug_info_start, (abfd))
.
.#define bfd_debug_info_end(abfd) \
.        BFD_SEND (abfd, _bfd_debug_info_end, (abfd))
.
.#define bfd_debug_info_accumulate(abfd, section) \
.        BFD_SEND (abfd, _bfd_debug_info_accumulate, (abfd, section))
.
.
.#define bfd_stat_arch_elt(abfd, stat) \
.        BFD_SEND (abfd, _bfd_stat_arch_elt,(abfd, stat))
.
.#define bfd_update_armap_timestamp(abfd) \
.        BFD_SEND (abfd, _bfd_update_armap_timestamp, (abfd))
.
.#define bfd_set_arch_mach(abfd, arch, mach)\
.        BFD_SEND ( abfd, _bfd_set_arch_mach, (abfd, arch, mach))
.
.#define bfd_relax_section(abfd, section, link_info, again) \
.       BFD_SEND (abfd, _bfd_relax_section, (abfd, section, link_info, again))
.
.#define bfd_gc_sections(abfd, link_info) \
.	BFD_SEND (abfd, _bfd_gc_sections, (abfd, link_info))
.
.#define bfd_merge_sections(abfd, link_info) \
.	BFD_SEND (abfd, _bfd_merge_sections, (abfd, link_info))
.
.#define bfd_discard_group(abfd, sec) \
.	BFD_SEND (abfd, _bfd_discard_group, (abfd, sec))
.
.#define bfd_link_hash_table_create(abfd) \
.	BFD_SEND (abfd, _bfd_link_hash_table_create, (abfd))
.
.#define bfd_link_hash_table_free(abfd, hash) \
.	BFD_SEND (abfd, _bfd_link_hash_table_free, (hash))
.
.#define bfd_link_add_symbols(abfd, info) \
.	BFD_SEND (abfd, _bfd_link_add_symbols, (abfd, info))
.
.#define bfd_link_just_syms(sec, info) \
.	BFD_SEND (abfd, _bfd_link_just_syms, (sec, info))
.
.#define bfd_final_link(abfd, info) \
.	BFD_SEND (abfd, _bfd_final_link, (abfd, info))
.
.#define bfd_free_cached_info(abfd) \
.       BFD_SEND (abfd, _bfd_free_cached_info, (abfd))
.
.#define bfd_get_dynamic_symtab_upper_bound(abfd) \
.	BFD_SEND (abfd, _bfd_get_dynamic_symtab_upper_bound, (abfd))
.
.#define bfd_print_private_bfd_data(abfd, file)\
.	BFD_SEND (abfd, _bfd_print_private_bfd_data, (abfd, file))
.
.#define bfd_canonicalize_dynamic_symtab(abfd, asymbols) \
.	BFD_SEND (abfd, _bfd_canonicalize_dynamic_symtab, (abfd, asymbols))
.
.#define bfd_get_dynamic_reloc_upper_bound(abfd) \
.	BFD_SEND (abfd, _bfd_get_dynamic_reloc_upper_bound, (abfd))
.
.#define bfd_canonicalize_dynamic_reloc(abfd, arels, asyms) \
.	BFD_SEND (abfd, _bfd_canonicalize_dynamic_reloc, (abfd, arels, asyms))
.
.extern bfd_byte *bfd_get_relocated_section_contents
.	PARAMS ((bfd *, struct bfd_link_info *,
.		  struct bfd_link_order *, bfd_byte *,
.		  boolean, asymbol **));
.

*/

bfd_byte *
bfd_get_relocated_section_contents (abfd, link_info, link_order, data,
				    relocateable, symbols)
     bfd *abfd;
     struct bfd_link_info *link_info;
     struct bfd_link_order *link_order;
     bfd_byte *data;
     boolean relocateable;
     asymbol **symbols;
{
  bfd *abfd2;
  bfd_byte *(*fn) PARAMS ((bfd *, struct bfd_link_info *,
			   struct bfd_link_order *, bfd_byte *, boolean,
			   asymbol **));

  if (link_order->type == bfd_indirect_link_order)
    {
      abfd2 = link_order->u.indirect.section->owner;
      if (abfd2 == NULL)
	abfd2 = abfd;
    }
  else
    abfd2 = abfd;

  fn = abfd2->xvec->_bfd_get_relocated_section_contents;

  return (*fn) (abfd, link_info, link_order, data, relocateable, symbols);
}

/* Record information about an ELF program header.  */

boolean
bfd_record_phdr (abfd, type, flags_valid, flags, at_valid, at,
		 includes_filehdr, includes_phdrs, count, secs)
     bfd *abfd;
     unsigned long type;
     boolean flags_valid;
     flagword flags;
     boolean at_valid;
     bfd_vma at;
     boolean includes_filehdr;
     boolean includes_phdrs;
     unsigned int count;
     asection **secs;
{
  struct elf_segment_map *m, **pm;
  bfd_size_type amt;

  if (bfd_get_flavour (abfd) != bfd_target_elf_flavour)
    return true;

  amt = sizeof (struct elf_segment_map);
  amt += ((bfd_size_type) count - 1) * sizeof (asection *);
  m = (struct elf_segment_map *) bfd_alloc (abfd, amt);
  if (m == NULL)
    return false;

  m->next = NULL;
  m->p_type = type;
  m->p_flags = flags;
  m->p_paddr = at;
  m->p_flags_valid = flags_valid;
  m->p_paddr_valid = at_valid;
  m->includes_filehdr = includes_filehdr;
  m->includes_phdrs = includes_phdrs;
  m->count = count;
  if (count > 0)
    memcpy (m->sections, secs, count * sizeof (asection *));

  for (pm = &elf_tdata (abfd)->segment_map; *pm != NULL; pm = &(*pm)->next)
    ;
  *pm = m;

  return true;
}

void
bfd_sprintf_vma (abfd, buf, value)
     bfd *abfd;
     char *buf;
     bfd_vma value;
{
  if (bfd_get_flavour (abfd) == bfd_target_elf_flavour)
    get_elf_backend_data (abfd)->elf_backend_sprintf_vma (abfd, buf, value);
  else
    sprintf_vma (buf, value);
}

void
bfd_fprintf_vma (abfd, stream, value)
     bfd *abfd;
     PTR stream;
     bfd_vma value;
{
  if (bfd_get_flavour (abfd) == bfd_target_elf_flavour)
    get_elf_backend_data (abfd)->elf_backend_fprintf_vma (abfd, stream, value);
  else
    fprintf_vma ((FILE *) stream, value);
}

/*
FUNCTION
	bfd_alt_mach_code

SYNOPSIS
	boolean bfd_alt_mach_code(bfd *abfd, int alternative);

DESCRIPTION

	When more than one machine code number is available for the
	same machine type, this function can be used to switch between
	the preferred one (alternative == 0) and any others.  Currently,
	only ELF supports this feature, with up to two alternate
	machine codes.
*/

boolean
bfd_alt_mach_code (abfd, alternative)
     bfd *abfd;
     int alternative;
{
  if (bfd_get_flavour (abfd) == bfd_target_elf_flavour)
    {
      int code;

      switch (alternative)
	{
	case 0:
	  code = get_elf_backend_data (abfd)->elf_machine_code;
	  break;

	case 1:
	  code = get_elf_backend_data (abfd)->elf_machine_alt1;
	  if (code == 0)
	    return false;
	  break;

	case 2:
	  code = get_elf_backend_data (abfd)->elf_machine_alt2;
	  if (code == 0)
	    return false;
	  break;

	default:
	  return false;
	}

      elf_elfheader (abfd)->e_machine = code;

      return true;
    }

  return false;
}
