/* readelf.c -- display contents of an ELF format file
   Copyright (C) 1998, 99, 2000 Free Software Foundation, Inc.

   Originally developed by Eric Youngdale <eric@andante.jic.com>
   Modifications by Nick Clifton <nickc@cygnus.com>

   This file is part of GNU Binutils.

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */


#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <time.h>

#if __GNUC__ >= 2
/* Define BFD64 here, even if our default architecture is 32 bit ELF
   as this will allow us to read in and parse 64bit and 32bit ELF files.
   Only do this if we belive that the compiler can support a 64 bit
   data type.  For now we only rely on GCC being able to do this.  */
#define BFD64
#endif

#include "bfd.h"

#include "elf/common.h"
#include "elf/external.h"
#include "elf/internal.h"
#include "elf/dwarf2.h"

/* The following headers use the elf/reloc-macros.h file to
   automatically generate relocation recognition functions
   such as elf_mips_reloc_type()  */

#define RELOC_MACROS_GEN_FUNC

#include "elf/i386.h"
#include "elf/v850.h"
#include "elf/ppc.h"
#include "elf/mips.h"
#include "elf/alpha.h"
#include "elf/arm.h"
#include "elf/m68k.h"
#include "elf/sparc.h"
#include "elf/m32r.h"
#include "elf/d10v.h"
#include "elf/d30v.h"
#include "elf/sh.h"
#include "elf/mn10200.h"
#include "elf/mn10300.h"
#include "elf/hppa.h"
#include "elf/arc.h"
#include "elf/fr30.h"
#include "elf/mcore.h"
#include "elf/i960.h"
#include "elf/pj.h"
#include "elf/avr.h"

#include "bucomm.h"
#include "getopt.h"

char *          	program_name = "readelf";
unsigned int    	dynamic_addr;
bfd_size_type  		dynamic_size;
unsigned int    	rela_addr;
unsigned int    	rela_size;
char *          	dynamic_strings;
char *			string_table;
unsigned long           num_dynamic_syms;
Elf_Internal_Sym * 	dynamic_symbols;
Elf_Internal_Syminfo *	dynamic_syminfo;
unsigned long   	dynamic_syminfo_offset;
unsigned int		dynamic_syminfo_nent;
char            	program_interpreter [64];
int             	dynamic_info[DT_JMPREL + 1];
int             	version_info[16];
int             	loadaddr = 0;
Elf_Internal_Ehdr       elf_header;
Elf_Internal_Shdr *     section_headers;
Elf_Internal_Dyn *      dynamic_segment;
int 			show_name;
int 			do_dynamic;
int 			do_syms;
int 			do_reloc;
int 			do_sections;
int 			do_segments;
int 			do_using_dynamic;
int 			do_header;
int 			do_dump;
int 			do_version;
int			do_histogram;
int			do_debugging;
int                     do_debug_info;
int                     do_debug_abbrevs;
int                     do_debug_lines;
int                     do_debug_pubnames;
int                     do_debug_aranges;
int                     do_arch;
int                     do_notes;
int			is_32bit_elf;

/* A dynamic array of flags indicating which sections require dumping.  */
char *			dump_sects = NULL;
unsigned int		num_dump_sects = 0;

#define HEX_DUMP	(1 << 0)
#define DISASS_DUMP	(1 << 1)
#define DEBUG_DUMP	(1 << 2)

/* How to rpint a vma value.  */
typedef enum print_mode
{
  HEX,
  DEC,
  DEC_5,
  UNSIGNED,
  PREFIX_HEX,
  FULL_HEX,
  LONG_HEX
}
print_mode;

/* Forward declarations for dumb compilers.  */
static void		  print_vma		      PARAMS ((bfd_vma, print_mode));
static bfd_vma (*         byte_get)                   PARAMS ((unsigned char *, int));
static bfd_vma            byte_get_little_endian      PARAMS ((unsigned char *, int));
static bfd_vma            byte_get_big_endian         PARAMS ((unsigned char *, int));
static const char *       get_mips_dynamic_type       PARAMS ((unsigned long));
static const char *       get_sparc64_dynamic_type    PARAMS ((unsigned long));
static const char *       get_parisc_dynamic_type     PARAMS ((unsigned long));
static const char *       get_dynamic_type            PARAMS ((unsigned long));
static int                dump_relocations            PARAMS ((FILE *, unsigned long, unsigned long, Elf_Internal_Sym *, unsigned long, char *, int));
static char *             get_file_type               PARAMS ((unsigned));
static char *             get_machine_name            PARAMS ((unsigned));
static char *             get_machine_flags           PARAMS ((unsigned, unsigned));
static const char *       get_mips_segment_type       PARAMS ((unsigned long));
static const char *       get_parisc_segment_type     PARAMS ((unsigned long));
static const char *       get_segment_type            PARAMS ((unsigned long));
static const char *       get_mips_section_type_name  PARAMS ((unsigned int));
static const char *       get_parisc_section_type_name PARAMS ((unsigned int));
static const char *       get_section_type_name       PARAMS ((unsigned int));
static const char *       get_symbol_binding          PARAMS ((unsigned int));
static const char *       get_symbol_type             PARAMS ((unsigned int));
static const char *       get_symbol_visibility       PARAMS ((unsigned int));
static const char *       get_symbol_index_type       PARAMS ((unsigned int));
static const char *       get_dynamic_flags 	      PARAMS ((bfd_vma));
static void               usage                       PARAMS ((void));
static void               parse_args                  PARAMS ((int, char **));
static int                process_file_header         PARAMS ((void));
static int                process_program_headers     PARAMS ((FILE *));
static int                process_section_headers     PARAMS ((FILE *));
static void               dynamic_segment_mips_val    PARAMS ((Elf_Internal_Dyn *));
static void               dynamic_segment_parisc_val  PARAMS ((Elf_Internal_Dyn *));
static int                process_dynamic_segment     PARAMS ((FILE *));
static int                process_symbol_table        PARAMS ((FILE *));
static int                process_section_contents    PARAMS ((FILE *));
static void               process_file                PARAMS ((char *));
static int                process_relocs              PARAMS ((FILE *));
static int                process_version_sections    PARAMS ((FILE *));
static char *             get_ver_flags               PARAMS ((unsigned int));
static int                get_32bit_section_headers   PARAMS ((FILE *));
static int                get_64bit_section_headers   PARAMS ((FILE *));
static int		  get_32bit_program_headers   PARAMS ((FILE *, Elf_Internal_Phdr *));
static int		  get_64bit_program_headers   PARAMS ((FILE *, Elf_Internal_Phdr *));
static int                get_file_header             PARAMS ((FILE *));
static Elf_Internal_Sym * get_32bit_elf_symbols       PARAMS ((FILE *, unsigned long, unsigned long));
static Elf_Internal_Sym * get_64bit_elf_symbols       PARAMS ((FILE *, unsigned long, unsigned long));
static int *              get_dynamic_data            PARAMS ((FILE *, unsigned int));
static int                get_32bit_dynamic_segment   PARAMS ((FILE *));
static int                get_64bit_dynamic_segment   PARAMS ((FILE *));
#ifdef SUPPORT_DISASSEMBLY
static int 	          disassemble_section         PARAMS ((Elf32_Internal_Shdr *, FILE *));
#endif
static int 	          dump_section                PARAMS ((Elf32_Internal_Shdr *, FILE *));
static int 	          display_debug_section       PARAMS ((Elf32_Internal_Shdr *, FILE *));
static int                display_debug_info          PARAMS ((Elf32_Internal_Shdr *, unsigned char *, FILE *));
static int                display_debug_not_supported PARAMS ((Elf32_Internal_Shdr *, unsigned char *, FILE *));
static int                display_debug_lines         PARAMS ((Elf32_Internal_Shdr *, unsigned char *, FILE *));
static int                display_debug_abbrev        PARAMS ((Elf32_Internal_Shdr *, unsigned char *, FILE *));
static int                display_debug_aranges       PARAMS ((Elf32_Internal_Shdr *, unsigned char *, FILE *));
static unsigned char *    process_abbrev_section      PARAMS ((unsigned char *, unsigned char *));
static unsigned long      read_leb128                 PARAMS ((unsigned char *, int *, int));
static int                process_extended_line_op    PARAMS ((unsigned char *, int, int));
static void               reset_state_machine         PARAMS ((int));
static char *             get_TAG_name                PARAMS ((unsigned long));
static char *             get_AT_name                 PARAMS ((unsigned long));
static char *             get_FORM_name               PARAMS ((unsigned long));
static void               free_abbrevs                PARAMS ((void));
static void               add_abbrev                  PARAMS ((unsigned long, unsigned long, int));
static void               add_abbrev_attr             PARAMS ((unsigned long, unsigned long));
static unsigned char *    read_and_display_attr       PARAMS ((unsigned long, unsigned long, unsigned char *, unsigned long));
static unsigned char *    display_block               PARAMS ((unsigned char *, unsigned long));
static void               decode_location_expression  PARAMS ((unsigned char *, unsigned int));
static void		  request_dump                PARAMS ((unsigned int, char));
static const char *       get_elf_class               PARAMS ((unsigned char));
static const char *       get_data_encoding           PARAMS ((unsigned char));
static const char *       get_osabi_name              PARAMS ((unsigned char));
static int		  guess_is_rela               PARAMS ((unsigned long));
static char * 		  get_note_type		         PARAMS ((unsigned int));
static int		  process_note		         PARAMS ((Elf32_Internal_Note *));
static int		  process_corefile_note_segment  PARAMS ((FILE *, bfd_vma, bfd_vma));
static int		  process_corefile_note_segments PARAMS ((FILE *));
static int 		  process_corefile_contents	 PARAMS ((FILE *));

typedef int Elf32_Word;

#ifndef TRUE
#define TRUE     1
#define FALSE    0
#endif
#define UNKNOWN -1

#define SECTION_NAME(X) 	(string_table + (X)->sh_name)

#define DT_VERSIONTAGIDX(tag)	(DT_VERNEEDNUM - (tag))	/* Reverse order! */

#define BYTE_GET(field) 	byte_get (field, sizeof (field))

/* If we can support a 64 bit data type then BFD64 should be defined
   and sizeof (bfd_vma) == 8.  In this case when translating from an
   external 8 byte field to an internal field, we can assume that the
   internal field is also 8 bytes wide and so we can extact all the data.
   If, however, BFD64 is not defined, then we must assume that the
   internal data structure only has 4 byte wide fields that are the
   equivalent of the 8 byte wide external counterparts, and so we must
   truncate the data.  */
#ifdef  BFD64
#define BYTE_GET8(field) 	byte_get (field, -8)
#else
#define BYTE_GET8(field) 	byte_get (field, 8)
#endif

#define NUM_ELEM(array) 	(sizeof (array) / sizeof ((array)[0]))

#define GET_DATA_ALLOC(offset, size, var, type, reason)			\
  if (fseek (file, offset, SEEK_SET))					\
    {									\
      error (_("Unable to seek to start of %s at %x\n"), reason, offset); \
      return 0;								\
    }									\
									\
  var = (type) malloc (size);						\
									\
  if (var == NULL)							\
    {									\
      error (_("Out of memory allocating %d bytes for %s\n"), size, reason); \
      return 0;								\
    } 									\
 									\
  if (fread (var, size, 1, file) != 1) 					\
    { 									\
      error (_("Unable to read in %d bytes of %s\n"), size, reason); 	\
      free (var); 							\
      var = NULL;							\
      return 0; 							\
    }


#define GET_DATA(offset, var, reason) 					\
  if (fseek (file, offset, SEEK_SET))					\
    { 									\
      error (_("Unable to seek to %x for %s\n"), offset, reason);	\
      return 0;								\
    }									\
  else if (fread (& var, sizeof (var), 1, file) != 1)			\
    {									\
      error (_("Unable to read data at %x for %s\n"), offset, reason);	\
      return 0;								\
    }

#define GET_ELF_SYMBOLS(file, offset, size)			\
  (is_32bit_elf ? get_32bit_elf_symbols (file, offset, size)	\
   : get_64bit_elf_symbols (file, offset, size))


#ifdef ANSI_PROTOTYPES
static void
error (const char * message, ...)
{
  va_list args;

  fprintf (stderr, _("%s: Error: "), program_name);
  va_start (args, message);
  vfprintf (stderr, message, args);
  va_end (args);
  return;
}

static void
warn (const char * message, ...)
{
  va_list args;

  fprintf (stderr, _("%s: Warning: "), program_name);
  va_start (args, message);
  vfprintf (stderr, message, args);
  va_end (args);
  return;
}
#else
static void
error (va_alist)
     va_dcl
{
  char * message;
  va_list args;

  fprintf (stderr, _("%s: Error: "), program_name);
  va_start (args);
  message = va_arg (args, char *);
  vfprintf (stderr, message, args);
  va_end (args);
  return;
}

static void
warn (va_alist)
     va_dcl
{
  char * message;
  va_list args;

  fprintf (stderr, _("%s: Warning: "), program_name);
  va_start (args);
  message = va_arg (args, char *);
  vfprintf (stderr, message, args);
  va_end (args);
  return;
}
#endif

static bfd_vma
byte_get_little_endian (field, size)
     unsigned char * field;
     int             size;
{
  switch (size)
    {
    case 1:
      return * field;

    case 2:
      return  ((unsigned int) (field [0]))
	|    (((unsigned int) (field [1])) << 8);

    case 8:
      /* We want to extract data from an 8 byte wide field and
	 place it into a 4 byte wide field.  Since this is a little
	 endian source we can juts use the 4 byte extraction code.  */
      /* Fall through.  */
    case 4:
      return  ((unsigned long) (field [0]))
	|    (((unsigned long) (field [1])) << 8)
	|    (((unsigned long) (field [2])) << 16)
	|    (((unsigned long) (field [3])) << 24);

#ifdef BFD64
    case -8:
      /* This is a special case, generated by the BYTE_GET8 macro.
	 It means that we are loading an 8 byte value from a field
	 in an external structure into an 8 byte value in a field
	 in an internal strcuture.  */
      return  ((bfd_vma) (field [0]))
	|    (((bfd_vma) (field [1])) << 8)
	|    (((bfd_vma) (field [2])) << 16)
	|    (((bfd_vma) (field [3])) << 24)
	|    (((bfd_vma) (field [4])) << 32)
	|    (((bfd_vma) (field [5])) << 40)
	|    (((bfd_vma) (field [6])) << 48)
	|    (((bfd_vma) (field [7])) << 56);
#endif
    default:
      error (_("Unhandled data length: %d\n"), size);
      abort ();
    }
}

/* Print a VMA value.  */
static void
print_vma (vma, mode)
     bfd_vma vma;
     print_mode mode;
{
#ifdef BFD64
  if (is_32bit_elf)
#endif
    {
      switch (mode)
	{
	case FULL_HEX: printf ("0x"); /* drop through */
	case LONG_HEX: printf ("%8.8lx", (unsigned long) vma); break;
	case PREFIX_HEX: printf ("0x"); /* drop through */
	case HEX: printf ("%lx", (unsigned long) vma); break;
	case DEC: printf ("%ld", (unsigned long) vma); break;
	case DEC_5: printf ("%5ld", (long) vma); break;
	case UNSIGNED: printf ("%lu", (unsigned long) vma); break;
	}
    }
#ifdef BFD64
  else
    {
      switch (mode)
	{
	case FULL_HEX:
	  printf ("0x");
	  /* drop through */
	  
	case LONG_HEX:
	  printf_vma (vma);
	  break;
	  
	case PREFIX_HEX:
	  printf ("0x");
	  /* drop through */
	  
	case HEX:
#if BFD_HOST_64BIT_LONG
	  printf ("%lx", vma);
#else
	  if (_bfd_int64_high (vma))
	    printf ("%lx%lx", _bfd_int64_high (vma), _bfd_int64_low (vma));
	  else
	    printf ("%lx", _bfd_int64_low (vma));
#endif
	  break;

	case DEC:
#if BFD_HOST_64BIT_LONG
	  printf ("%ld", vma);
#else
	  if (_bfd_int64_high (vma))
	    /* ugg */
	    printf ("++%ld", _bfd_int64_low (vma));
	  else
	    printf ("%ld", _bfd_int64_low (vma));
#endif	  
	  break;

	case DEC_5:
#if BFD_HOST_64BIT_LONG
	  printf ("%5ld", vma);
#else
	  if (_bfd_int64_high (vma))
	    /* ugg */
	    printf ("++%ld", _bfd_int64_low (vma));
	  else
	    printf ("%5ld", _bfd_int64_low (vma));
#endif	  
	  break;
	  
	case UNSIGNED:
#if BFD_HOST_64BIT_LONG
	  printf ("%lu", vma);
#else	  
	  if (_bfd_int64_high (vma))
	    /* ugg */
	    printf ("++%lu", _bfd_int64_low (vma));
	  else
	    printf ("%lu", _bfd_int64_low (vma));
#endif
	  break;
	}
    }
#endif
}

static bfd_vma
byte_get_big_endian (field, size)
     unsigned char * field;
     int             size;
{
  switch (size)
    {
    case 1:
      return * field;

    case 2:
      return ((unsigned int) (field [1])) | (((int) (field [0])) << 8);

    case 4:
      return ((unsigned long) (field [3]))
	|   (((unsigned long) (field [2])) << 8)
	|   (((unsigned long) (field [1])) << 16)
	|   (((unsigned long) (field [0])) << 24);

    case 8:
      /* Although we are extracing data from an 8 byte wide field, we
	 are returning only 4 bytes of data.  */
      return ((unsigned long) (field [7]))
	|   (((unsigned long) (field [6])) << 8)
	|   (((unsigned long) (field [5])) << 16)
	|   (((unsigned long) (field [4])) << 24);

#ifdef BFD64
    case -8:
      /* This is a special case, generated by the BYTE_GET8 macro.
	 It means that we are loading an 8 byte value from a field
	 in an external structure into an 8 byte value in a field
	 in an internal strcuture.  */
      return ((bfd_vma) (field [7]))
	|   (((bfd_vma) (field [6])) << 8)
	|   (((bfd_vma) (field [5])) << 16)
	|   (((bfd_vma) (field [4])) << 24)
	|   (((bfd_vma) (field [3])) << 32)
	|   (((bfd_vma) (field [2])) << 40)
	|   (((bfd_vma) (field [1])) << 48)
	|   (((bfd_vma) (field [0])) << 56);
#endif

    default:
      error (_("Unhandled data length: %d\n"), size);
      abort ();
    }
}


/* Guess the relocation sized based on the sized commonly used by the specific machine.  */
static int
guess_is_rela (e_machine)
     unsigned long e_machine;
{
  switch (e_machine)
    {
      /* Targets that use REL relocations.  */
    case EM_ARM:
    case EM_386:
    case EM_486:
    case EM_960:
    case EM_CYGNUS_M32R:
    case EM_CYGNUS_D10V:
    case EM_MIPS:
    case EM_MIPS_RS4_BE:
      return FALSE;

      /* Targets that use RELA relocations.  */
    case EM_68K:
    case EM_SPARC32PLUS:
    case EM_SPARCV9:
    case EM_SPARC:
    case EM_PPC:
    case EM_CYGNUS_V850:
    case EM_CYGNUS_D30V:
    case EM_CYGNUS_MN10200:
    case EM_CYGNUS_MN10300:
    case EM_CYGNUS_FR30:
    case EM_SH:
    case EM_ALPHA:
    case EM_MCORE:
      return TRUE;

    case EM_MMA:
    case EM_PCP:
    case EM_NCPU:
    case EM_NDR1:
    case EM_STARCORE:
    case EM_ME16:
    case EM_ST100:
    case EM_TINYJ:
    case EM_FX66:
    case EM_ST9PLUS:
    case EM_ST7:
    case EM_68HC16:
    case EM_68HC11:
    case EM_68HC08:
    case EM_68HC05:
    case EM_SVX:
    case EM_ST19:
    case EM_VAX:
    default:
      warn (_("Don't know about relocations on this machine architecture\n"));
      return FALSE;
    }
}

/* Display the contents of the relocation data found at the specified offset.  */
static int
dump_relocations (file, rel_offset, rel_size, symtab, nsyms, strtab, is_rela)
     FILE *             file;
     unsigned long      rel_offset;
     unsigned long      rel_size;
     Elf_Internal_Sym * symtab;
     unsigned long      nsyms;
     char *             strtab;
     int                is_rela;
{
  unsigned int        i;
  Elf_Internal_Rel *  rels;
  Elf_Internal_Rela * relas;


  if (is_rela == UNKNOWN)
    is_rela = guess_is_rela (elf_header.e_machine);

  if (is_rela)
    {
      if (is_32bit_elf)
	{
	  Elf32_External_Rela * erelas;

	  GET_DATA_ALLOC (rel_offset, rel_size, erelas,
			  Elf32_External_Rela *, "relocs");

	  rel_size = rel_size / sizeof (Elf32_External_Rela);

	  relas = (Elf_Internal_Rela *)
	    malloc (rel_size * sizeof (Elf_Internal_Rela));

	  if (relas == NULL)
	    {
	      error(_("out of memory parsing relocs"));
	      return 0;
	    }

	  for (i = 0; i < rel_size; i++)
	    {
	      relas[i].r_offset = BYTE_GET (erelas[i].r_offset);
	      relas[i].r_info   = BYTE_GET (erelas[i].r_info);
	      relas[i].r_addend = BYTE_GET (erelas[i].r_addend);
	    }

	  free (erelas);

	  rels = (Elf_Internal_Rel *) relas;
	}
      else
	{
	  Elf64_External_Rela * erelas;

	  GET_DATA_ALLOC (rel_offset, rel_size, erelas,
			  Elf64_External_Rela *, "relocs");

	  rel_size = rel_size / sizeof (Elf64_External_Rela);

	  relas = (Elf_Internal_Rela *)
	    malloc (rel_size * sizeof (Elf_Internal_Rela));

	  if (relas == NULL)
	    {
	      error(_("out of memory parsing relocs"));
	      return 0;
	    }

	  for (i = 0; i < rel_size; i++)
	    {
	      relas[i].r_offset = BYTE_GET8 (erelas[i].r_offset);
	      relas[i].r_info   = BYTE_GET8 (erelas[i].r_info);
	      relas[i].r_addend = BYTE_GET8 (erelas[i].r_addend);
	    }

	  free (erelas);

	  rels = (Elf_Internal_Rel *) relas;
	}
    }
  else
    {
      if (is_32bit_elf)
	{
	  Elf32_External_Rel * erels;

	  GET_DATA_ALLOC (rel_offset, rel_size, erels,
			  Elf32_External_Rel *, "relocs");

	  rel_size = rel_size / sizeof (Elf32_External_Rel);

	  rels = (Elf_Internal_Rel *)
	    malloc (rel_size * sizeof (Elf_Internal_Rel));

	  if (rels == NULL)
	    {
	      error(_("out of memory parsing relocs"));
	      return 0;
	    }

	  for (i = 0; i < rel_size; i++)
	    {
	      rels[i].r_offset = BYTE_GET (erels[i].r_offset);
	      rels[i].r_info   = BYTE_GET (erels[i].r_info);
	    }

	  free (erels);

	  relas = (Elf_Internal_Rela *) rels;
	}
      else
	{
	  Elf64_External_Rel * erels;

	  GET_DATA_ALLOC (rel_offset, rel_size, erels,
			  Elf64_External_Rel *, "relocs");

	  rel_size = rel_size / sizeof (Elf64_External_Rel);

	  rels = (Elf_Internal_Rel *)
	    malloc (rel_size * sizeof (Elf_Internal_Rel));

	  if (rels == NULL)
	    {
	      error(_("out of memory parsing relocs"));
	      return 0;
	    }

	  for (i = 0; i < rel_size; i++)
	    {
	      rels[i].r_offset = BYTE_GET8 (erels[i].r_offset);
	      rels[i].r_info   = BYTE_GET8 (erels[i].r_info);
	    }

	  free (erels);

	  relas = (Elf_Internal_Rela *) rels;
	}
    }

  if (is_rela)
    printf
      (_("  Offset    Info  Type            Symbol's Value  Symbol's Name          Addend\n"));
  else
    printf
      (_("  Offset    Info  Type            Symbol's Value  Symbol's Name\n"));

  for (i = 0; i < rel_size; i++)
    {
      const char * rtype;
      bfd_vma      offset;
      bfd_vma      info;
      bfd_vma      symtab_index;
      bfd_vma      type;

      if (is_rela)
	{
	  offset = relas [i].r_offset;
	  info   = relas [i].r_info;
	}
      else
	{
	  offset = rels [i].r_offset;
	  info   = rels [i].r_info;
	}

      if (is_32bit_elf)
	{
	  type         = ELF32_R_TYPE (info);
	  symtab_index = ELF32_R_SYM  (info);
	}
      else
	{
	  if (elf_header.e_machine == EM_SPARCV9)
	    type       = ELF64_R_TYPE_ID (info);
	  else
	    type       = ELF64_R_TYPE (info);
	  /* The #ifdef BFD64 below is to prevent a compile time warning.
	     We know that if we do not have a 64 bit data type that we
	     will never execute this code anyway.  */
#ifdef BFD64
	  symtab_index = ELF64_R_SYM  (info);
#endif
	}

#ifdef _bfd_int64_low
      printf ("  %8.8lx  %5.5lx ", _bfd_int64_low (offset), _bfd_int64_low (info));
#else
      printf ("  %8.8lx  %5.5lx ", offset, info);
#endif

      switch (elf_header.e_machine)
	{
	default:
	  rtype = NULL;
	  break;

	case EM_CYGNUS_M32R:
	  rtype = elf_m32r_reloc_type (type);
	  break;

	case EM_386:
	case EM_486:
	  rtype = elf_i386_reloc_type (type);
	  break;

	case EM_68K:
	  rtype = elf_m68k_reloc_type (type);
	  break;

	case EM_960:
	  rtype = elf_i960_reloc_type (type);
	  break;

	case EM_AVR:
	  rtype = elf_avr_reloc_type (type);
	  break;

	case EM_OLD_SPARCV9:
	case EM_SPARC32PLUS:
	case EM_SPARCV9:
	case EM_SPARC:
	  rtype = elf_sparc_reloc_type (type);
	  break;

	case EM_CYGNUS_V850:
	  rtype = v850_reloc_type (type);
	  break;

	case EM_CYGNUS_D10V:
	  rtype = elf_d10v_reloc_type (type);
	  break;

	case EM_CYGNUS_D30V:
	  rtype = elf_d30v_reloc_type (type);
	  break;

	case EM_SH:
	  rtype = elf_sh_reloc_type (type);
	  break;

	case EM_CYGNUS_MN10300:
	  rtype = elf_mn10300_reloc_type (type);
	  break;

	case EM_CYGNUS_MN10200:
	  rtype = elf_mn10200_reloc_type (type);
	  break;

	case EM_CYGNUS_FR30:
	  rtype = elf_fr30_reloc_type (type);
	  break;

	case EM_MCORE:
	  rtype = elf_mcore_reloc_type (type);
	  break;

	case EM_PPC:
	  rtype = elf_ppc_reloc_type (type);
	  break;

	case EM_MIPS:
	case EM_MIPS_RS4_BE:
	  rtype = elf_mips_reloc_type (type);
	  break;

	case EM_ALPHA:
	  rtype = elf_alpha_reloc_type (type);
	  break;

	case EM_ARM:
	  rtype = elf_arm_reloc_type (type);
	  break;

	case EM_CYGNUS_ARC:
	  rtype = elf_arc_reloc_type (type);
	  break;

	case EM_PARISC:
	  rtype = elf_hppa_reloc_type (type);
	  break;

	case EM_PJ:
	  rtype = elf_pj_reloc_type (type);
	  break;
	}

      if (rtype == NULL)
#ifdef _bfd_int64_low
	printf (_("unrecognised: %-7lx"), _bfd_int64_low (type));
#else
	printf (_("unrecognised: %-7lx"), type);
#endif
      else
	printf ("%-21.21s", rtype);

      if (symtab_index)
	{
	  if (symtab != NULL)
	    {
	      if (symtab_index >= nsyms)
		printf (" bad symbol index: %08lx", (unsigned long) symtab_index);
	      else
		{
		  Elf_Internal_Sym * psym;

		  psym = symtab + symtab_index;

		  printf (" ");
		  print_vma (psym->st_value, LONG_HEX);
		  printf ("  ");

		  if (psym->st_name == 0)
		    printf ("%-25.25s",
			    SECTION_NAME (section_headers + psym->st_shndx));
		  else if (strtab == NULL)
		    printf (_("<string table index %3ld>"), psym->st_name);
		  else
		    printf ("%-25.25s", strtab + psym->st_name);

		  if (is_rela)
		    printf (" + %lx", (unsigned long) relas [i].r_addend);
		}
	    }
	}
      else if (is_rela)
	{
	  printf ("%*c", is_32bit_elf ? 34 : 26, ' ');
	  print_vma (relas[i].r_addend, LONG_HEX);
	}

      if (elf_header.e_machine == EM_SPARCV9
	  && !strcmp (rtype, "R_SPARC_OLO10"))
	printf (" + %lx", (unsigned long) ELF64_R_TYPE_DATA (info));

      putchar ('\n');
    }

  free (relas);

  return 1;
}

static const char *
get_mips_dynamic_type (type)
     unsigned long type;
{
  switch (type)
    {
    case DT_MIPS_RLD_VERSION: return "MIPS_RLD_VERSION";
    case DT_MIPS_TIME_STAMP: return "MIPS_TIME_STAMP";
    case DT_MIPS_ICHECKSUM: return "MIPS_ICHECKSUM";
    case DT_MIPS_IVERSION: return "MIPS_IVERSION";
    case DT_MIPS_FLAGS: return "MIPS_FLAGS";
    case DT_MIPS_BASE_ADDRESS: return "MIPS_BASE_ADDRESS";
    case DT_MIPS_MSYM: return "MIPS_MSYM";
    case DT_MIPS_CONFLICT: return "MIPS_CONFLICT";
    case DT_MIPS_LIBLIST: return "MIPS_LIBLIST";
    case DT_MIPS_LOCAL_GOTNO: return "MIPS_LOCAL_GOTNO";
    case DT_MIPS_CONFLICTNO: return "MIPS_CONFLICTNO";
    case DT_MIPS_LIBLISTNO: return "MIPS_LIBLISTNO";
    case DT_MIPS_SYMTABNO: return "MIPS_SYMTABNO";
    case DT_MIPS_UNREFEXTNO: return "MIPS_UNREFEXTNO";
    case DT_MIPS_GOTSYM: return "MIPS_GOTSYM";
    case DT_MIPS_HIPAGENO: return "MIPS_HIPAGENO";
    case DT_MIPS_RLD_MAP: return "MIPS_RLD_MAP";
    case DT_MIPS_DELTA_CLASS: return "MIPS_DELTA_CLASS";
    case DT_MIPS_DELTA_CLASS_NO: return "MIPS_DELTA_CLASS_NO";
    case DT_MIPS_DELTA_INSTANCE: return "MIPS_DELTA_INSTANCE";
    case DT_MIPS_DELTA_INSTANCE_NO: return "MIPS_DELTA_INSTANCE_NO";
    case DT_MIPS_DELTA_RELOC: return "MIPS_DELTA_RELOC";
    case DT_MIPS_DELTA_RELOC_NO: return "MIPS_DELTA_RELOC_NO";
    case DT_MIPS_DELTA_SYM: return "MIPS_DELTA_SYM";
    case DT_MIPS_DELTA_SYM_NO: return "MIPS_DELTA_SYM_NO";
    case DT_MIPS_DELTA_CLASSSYM: return "MIPS_DELTA_CLASSSYM";
    case DT_MIPS_DELTA_CLASSSYM_NO: return "MIPS_DELTA_CLASSSYM_NO";
    case DT_MIPS_CXX_FLAGS: return "MIPS_CXX_FLAGS";
    case DT_MIPS_PIXIE_INIT: return "MIPS_PIXIE_INIT";
    case DT_MIPS_SYMBOL_LIB: return "MIPS_SYMBOL_LIB";
    case DT_MIPS_LOCALPAGE_GOTIDX: return "MIPS_LOCALPAGE_GOTIDX";
    case DT_MIPS_LOCAL_GOTIDX: return "MIPS_LOCAL_GOTIDX";
    case DT_MIPS_HIDDEN_GOTIDX: return "MIPS_HIDDEN_GOTIDX";
    case DT_MIPS_PROTECTED_GOTIDX: return "MIPS_PROTECTED_GOTIDX";
    case DT_MIPS_OPTIONS: return "MIPS_OPTIONS";
    case DT_MIPS_INTERFACE: return "MIPS_INTERFACE";
    case DT_MIPS_DYNSTR_ALIGN: return "MIPS_DYNSTR_ALIGN";
    case DT_MIPS_INTERFACE_SIZE: return "MIPS_INTERFACE_SIZE";
    case DT_MIPS_RLD_TEXT_RESOLVE_ADDR: return "MIPS_RLD_TEXT_RESOLVE_ADDR";
    case DT_MIPS_PERF_SUFFIX: return "MIPS_PERF_SUFFIX";
    case DT_MIPS_COMPACT_SIZE: return "MIPS_COMPACT_SIZE";
    case DT_MIPS_GP_VALUE: return "MIPS_GP_VALUE";
    case DT_MIPS_AUX_DYNAMIC: return "MIPS_AUX_DYNAMIC";
    default:
      return NULL;
    }
}

static const char *
get_sparc64_dynamic_type (type)
     unsigned long type;
{
  switch (type)
    {
    case DT_SPARC_REGISTER: return "SPARC_REGISTER";
    default:
      return NULL;
    }
}

static const char *
get_parisc_dynamic_type (type)
     unsigned long type;
{
  switch (type)
    {
    case DT_HP_LOAD_MAP:	return "HP_LOAD_MAP";
    case DT_HP_DLD_FLAGS:	return "HP_DLD_FLAGS";
    case DT_HP_DLD_HOOK:	return "HP_DLD_HOOK";
    case DT_HP_UX10_INIT:	return "HP_UX10_INIT";
    case DT_HP_UX10_INITSZ:	return "HP_UX10_INITSZ";
    case DT_HP_PREINIT:		return "HP_PREINIT";
    case DT_HP_PREINITSZ:	return "HP_PREINITSZ";
    case DT_HP_NEEDED:		return "HP_NEEDED";
    case DT_HP_TIME_STAMP:	return "HP_TIME_STAMP";
    case DT_HP_CHECKSUM:	return "HP_CHECKSUM";
    case DT_HP_GST_SIZE:	return "HP_GST_SIZE";
    case DT_HP_GST_VERSION:	return "HP_GST_VERSION";
    case DT_HP_GST_HASHVAL:	return "HP_GST_HASHVAL";
    default:
      return NULL;
    }
}

static const char *
get_dynamic_type (type)
     unsigned long type;
{
  static char buff [32];

  switch (type)
    {
    case DT_NULL:	return "NULL";
    case DT_NEEDED:	return "NEEDED";
    case DT_PLTRELSZ:	return "PLTRELSZ";
    case DT_PLTGOT:	return "PLTGOT";
    case DT_HASH:	return "HASH";
    case DT_STRTAB:	return "STRTAB";
    case DT_SYMTAB:	return "SYMTAB";
    case DT_RELA:	return "RELA";
    case DT_RELASZ:	return "RELASZ";
    case DT_RELAENT:	return "RELAENT";
    case DT_STRSZ:	return "STRSZ";
    case DT_SYMENT:	return "SYMENT";
    case DT_INIT:	return "INIT";
    case DT_FINI:	return "FINI";
    case DT_SONAME:	return "SONAME";
    case DT_RPATH:	return "RPATH";
    case DT_SYMBOLIC:	return "SYMBOLIC";
    case DT_REL:	return "REL";
    case DT_RELSZ:	return "RELSZ";
    case DT_RELENT:	return "RELENT";
    case DT_PLTREL:	return "PLTREL";
    case DT_DEBUG:	return "DEBUG";
    case DT_TEXTREL:	return "TEXTREL";
    case DT_JMPREL:	return "JMPREL";
    case DT_BIND_NOW:   return "BIND_NOW";
    case DT_INIT_ARRAY: return "INIT_ARRAY";
    case DT_FINI_ARRAY: return "FINI_ARRAY";
    case DT_INIT_ARRAYSZ: return "INIT_ARRAYSZ";
    case DT_FINI_ARRAYSZ: return "FINI_ARRAYSZ";
    case DT_RUNPATH:    return "RUNPATH";
    case DT_FLAGS:      return "FLAGS";

    case DT_PREINIT_ARRAY: return "PREINIT_ARRAY";
    case DT_PREINIT_ARRAYSZ: return "PREINIT_ARRAYSZ";

    case DT_PLTPADSZ:	return "PLTPADSZ";
    case DT_MOVEENT:	return "MOVEENT";
    case DT_MOVESZ:	return "MOVESZ";
    case DT_FEATURE_1:	return "FEATURE_1";
    case DT_POSFLAG_1:	return "POSFLAG_1";
    case DT_SYMINSZ:	return "SYMINSZ";
    case DT_SYMINENT:	return "SYMINENT"; /* aka VALRNGHI */

    case DT_ADDRRNGLO:  return "ADDRRNGLO";
    case DT_SYMINFO:	return "SYMINFO"; /* aka ADDRRNGHI */

    case DT_VERSYM:	return "VERSYM";

    case DT_RELACOUNT:	return "RELACOUNT";
    case DT_RELCOUNT:	return "RELCOUNT";
    case DT_FLAGS_1:	return "FLAGS_1";
    case DT_VERDEF:	return "VERDEF";
    case DT_VERDEFNUM:	return "VERDEFNUM";
    case DT_VERNEED:	return "VERNEED";
    case DT_VERNEEDNUM:	return "VERNEEDNUM";

    case DT_AUXILIARY:	return "AUXILARY";
    case DT_USED:	return "USED";
    case DT_FILTER:	return "FILTER";

    default:
      if ((type >= DT_LOPROC) && (type <= DT_HIPROC))
	{
	  const char * result;

	  switch (elf_header.e_machine)
	    {
	    case EM_MIPS:
	    case EM_MIPS_RS4_BE:
	      result = get_mips_dynamic_type (type);
	      break;
	    case EM_SPARCV9:
	      result = get_sparc64_dynamic_type (type);
	      break;
	    default:
	      result = NULL;
	      break;
	    }

	  if (result != NULL)
	    return result;

	  sprintf (buff, _("Processor Specific: %lx"), type);
	}
      else if ((type >= DT_LOOS) && (type <= DT_HIOS))
	{
	  const char * result;

	  switch (elf_header.e_machine)
	    {
	    case EM_PARISC:
	      result = get_parisc_dynamic_type (type);
	      break;
	    default:
	      result = NULL;
	      break;
	    }

	  if (result != NULL)
	    return result;

	  sprintf (buff, _("Operating System specific: %lx"), type);
	}
      else
	sprintf (buff, _("<unknown>: %lx"), type);

      return buff;
    }
}

static char *
get_file_type (e_type)
     unsigned e_type;
{
  static char buff [32];

  switch (e_type)
    {
    case ET_NONE:	return _("NONE (None)");
    case ET_REL:	return _("REL (Relocatable file)");
    case ET_EXEC:       return _("EXEC (Executable file)");
    case ET_DYN:        return _("DYN (Shared object file)");
    case ET_CORE:       return _("CORE (Core file)");

    default:
      if ((e_type >= ET_LOPROC) && (e_type <= ET_HIPROC))
	sprintf (buff, _("Processor Specific: (%x)"), e_type);
      else if ((e_type >= ET_LOOS) && (e_type <= ET_HIOS))
	sprintf (buff, _("OS Specific: (%x)"), e_type);
      else
	sprintf (buff, _("<unknown>: %x"), e_type);
      return buff;
    }
}

static char *
get_machine_name (e_machine)
     unsigned e_machine;
{
  static char buff [64]; /* XXX */

  switch (e_machine)
    {
    case EM_NONE:        	return _("None");
    case EM_M32:         	return "WE32100";
    case EM_SPARC:       	return "Sparc";
    case EM_386:         	return "Intel 80386";
    case EM_68K:         	return "MC68000";
    case EM_88K:         	return "MC88000";
    case EM_486:         	return "Intel 80486";
    case EM_860:         	return "Intel 80860";
    case EM_MIPS:        	return "MIPS R3000";
    case EM_S370:        	return "IBM System/370";
    case EM_MIPS_RS4_BE: 	return "MIPS R4000 big-endian";
    case EM_OLD_SPARCV9:	return "Sparc v9 (old)";
    case EM_PARISC:      	return "HPPA";
    case EM_PPC_OLD:		return "Power PC (old)";
    case EM_SPARC32PLUS: 	return "Sparc v8+" ;
    case EM_960:         	return "Intel 90860";
    case EM_PPC:         	return "PowerPC";
    case EM_V800:         	return "NEC V800";
    case EM_FR20:         	return "Fujitsu FR20";
    case EM_RH32:         	return "TRW RH32";
    case EM_MCORE:	        return "MCORE";
    case EM_ARM:	 	return "ARM";
    case EM_OLD_ALPHA:	 	return "Digital Alpha (old)";
    case EM_SH:		 	return "Hitachi SH";
    case EM_SPARCV9:     	return "Sparc v9";
    case EM_TRICORE:    	return "Siemens Tricore";
    case EM_ARC:		return "Argonaut RISC Core";
    case EM_H8_300:		return "Hitachi H8/300";
    case EM_H8_300H:		return "Hitachi H8/300H";
    case EM_H8S:		return "Hitachi H8S";
    case EM_H8_500:		return "Hitachi H8/500";
    case EM_IA_64:		return "Intel IA-64";
    case EM_MIPS_X:		return "Stanford MIPS-X";
    case EM_COLDFIRE:		return "Motorola Coldfire";
    case EM_68HC12:		return "Motorola M68HC12";
    case EM_ALPHA:       	return "Alpha";
    case EM_CYGNUS_D10V:        return "d10v";
    case EM_CYGNUS_D30V:        return "d30v";
    case EM_CYGNUS_ARC:		return "Arc";
    case EM_CYGNUS_M32R:	return "Mitsubishi M32r";
    case EM_CYGNUS_V850:	return "NEC v850";
    case EM_CYGNUS_MN10300:	return "mn10300";
    case EM_CYGNUS_MN10200:	return "mn10200";
    case EM_CYGNUS_FR30:	return "Fujitsu FR30";
    case EM_PJ:                 return "picoJava";
    case EM_MMA:      		return "Fujitsu Multimedia Accelerator";
    case EM_PCP:      		return "Siemens PCP";
    case EM_NCPU:     		return "Sony nCPU embedded RISC processor";
    case EM_NDR1:     		return "Denso NDR1 microprocesspr";
    case EM_STARCORE: 		return "Motorola Star*Core processor";
    case EM_ME16:     		return "Toyota ME16 processor";
    case EM_ST100:    		return "STMicroelectronics ST100 processor";
    case EM_TINYJ:    		return "Advanced Logic Corp. TinyJ embedded processor";
    case EM_FX66:     		return "Siemens FX66 microcontroller";
    case EM_ST9PLUS:  		return "STMicroelectronics ST9+ 8/16 bit microcontroller";
    case EM_ST7:      		return "STMicroelectronics ST7 8-bit microcontroller";
    case EM_68HC16:   		return "Motorola MC68HC16 Microcontroller";
    case EM_68HC11:   		return "Motorola MC68HC11 Microcontroller";
    case EM_68HC08:   		return "Motorola MC68HC08 Microcontroller";
    case EM_68HC05:   		return "Motorola MC68HC05 Microcontroller";
    case EM_SVX:      		return "Silicon Graphics SVx";
    case EM_ST19:     		return "STMicroelectronics ST19 8-bit microcontroller";
    case EM_VAX:      		return "Digital VAX";
    default:
      sprintf (buff, _("<unknown>: %x"), e_machine);
      return buff;
    }
}

static char *
get_machine_flags (e_flags, e_machine)
     unsigned e_flags;
     unsigned e_machine;
{
  static char buf [1024];

  buf[0] = '\0';
  
  if (e_flags)
    {
      switch (e_machine)
	{
	default:
	  break;

        case EM_68K:
          if (e_flags & EF_CPU32)
            strcat (buf, ", cpu32");
          break;

	case EM_PPC:
	  if (e_flags & EF_PPC_EMB)
	    strcat (buf, ", emb");

	  if (e_flags & EF_PPC_RELOCATABLE)
	    strcat (buf, ", relocatable");

	  if (e_flags & EF_PPC_RELOCATABLE_LIB)
	    strcat (buf, ", relocatable-lib");
	  break;

	case EM_CYGNUS_V850:
	  switch (e_flags & EF_V850_ARCH)
	    {
	    case E_V850E_ARCH:
	      strcat (buf, ", v850e");
	      break;
	    case E_V850EA_ARCH:
	      strcat (buf, ", v850ea");
	      break;
	    case E_V850_ARCH:
	      strcat (buf, ", v850");
	      break;
	    default:
	      strcat (buf, ", unknown v850 architecture variant");
	      break;
	    }
	  break;

	case EM_CYGNUS_M32R:
	  if ((e_flags & EF_M32R_ARCH) == E_M32R_ARCH)
	    strcat (buf, ", m32r");

	  break;

	case EM_MIPS:
	case EM_MIPS_RS4_BE:
	  if (e_flags & EF_MIPS_NOREORDER)
	    strcat (buf, ", noreorder");

	  if (e_flags & EF_MIPS_PIC)
	    strcat (buf, ", pic");

	  if (e_flags & EF_MIPS_CPIC)
	    strcat (buf, ", cpic");

	  if (e_flags & EF_MIPS_ABI2)
	    strcat (buf, ", abi2");

	  if ((e_flags & EF_MIPS_ARCH) == E_MIPS_ARCH_1)
	    strcat (buf, ", mips1");

	  if ((e_flags & EF_MIPS_ARCH) == E_MIPS_ARCH_2)
	    strcat (buf, ", mips2");

	  if ((e_flags & EF_MIPS_ARCH) == E_MIPS_ARCH_3)
	    strcat (buf, ", mips3");

	  if ((e_flags & EF_MIPS_ARCH) == E_MIPS_ARCH_4)
	    strcat (buf, ", mips4");
	  break;

	case EM_SPARCV9:
	  if (e_flags & EF_SPARC_32PLUS)
	    strcat (buf, ", v8+");

	  if (e_flags & EF_SPARC_SUN_US1)
	    strcat (buf, ", ultrasparcI");

	  if (e_flags & EF_SPARC_SUN_US3)
	    strcat (buf, ", ultrasparcIII");

	  if (e_flags & EF_SPARC_HAL_R1)
	    strcat (buf, ", halr1");

	  if (e_flags & EF_SPARC_LEDATA)
	    strcat (buf, ", ledata");

	  if ((e_flags & EF_SPARCV9_MM) == EF_SPARCV9_TSO)
	    strcat (buf, ", tso");

	  if ((e_flags & EF_SPARCV9_MM) == EF_SPARCV9_PSO)
	    strcat (buf, ", pso");

	  if ((e_flags & EF_SPARCV9_MM) == EF_SPARCV9_RMO)
	    strcat (buf, ", rmo");
	  break;

	case EM_PARISC:
	  switch (e_flags & EF_PARISC_ARCH)
	    {
	    case EFA_PARISC_1_0:
	      strcpy (buf, ", PA-RISC 1.0");
	      break;
	    case EFA_PARISC_1_1:
	      strcpy (buf, ", PA-RISC 1.1");
	      break;
	    case EFA_PARISC_2_0:
	      strcpy (buf, ", PA-RISC 2.0");
	      break;
	    default:
	      break;
	    }
	  if (e_flags & EF_PARISC_TRAPNIL)
	    strcat (buf, ", trapnil");
	  if (e_flags & EF_PARISC_EXT)
	    strcat (buf, ", ext");
	  if (e_flags & EF_PARISC_LSB)
	    strcat (buf, ", lsb");
	  if (e_flags & EF_PARISC_WIDE)
	    strcat (buf, ", wide");
	  if (e_flags & EF_PARISC_NO_KABP)
	    strcat (buf, ", no kabp");
	  if (e_flags & EF_PARISC_LAZYSWAP)
	    strcat (buf, ", lazyswap");
	  break;
	  
	case EM_PJ:
	  if ((e_flags & EF_PICOJAVA_NEWCALLS) == EF_PICOJAVA_NEWCALLS)
	    strcat (buf, ", new calling convention");

	  if ((e_flags & EF_PICOJAVA_GNUCALLS) == EF_PICOJAVA_GNUCALLS)
	    strcat (buf, ", gnu calling convention");
	  break;
	}
    }

  return buf;
}

static const char *
get_mips_segment_type (type)
     unsigned long type;
{
  switch (type)
    {
    case PT_MIPS_REGINFO:
      return "REGINFO";
    case PT_MIPS_RTPROC:
      return "RTPROC";
    case PT_MIPS_OPTIONS:
      return "OPTIONS";
    default:
      break;
    }

  return NULL;
}

static const char *
get_parisc_segment_type (type)
     unsigned long type;
{
  switch (type)
    {
    case PT_HP_TLS:		return "HP_TLS";
    case PT_HP_CORE_NONE:	return "HP_CORE_NONE";
    case PT_HP_CORE_VERSION:	return "HP_CORE_VERSION";
    case PT_HP_CORE_KERNEL:	return "HP_CORE_KERNEL";
    case PT_HP_CORE_COMM:	return "HP_CORE_COMM";
    case PT_HP_CORE_PROC:	return "HP_CORE_PROC";
    case PT_HP_CORE_LOADABLE:	return "HP_CORE_LOADABLE";
    case PT_HP_CORE_STACK:	return "HP_CORE_STACK";
    case PT_HP_CORE_SHM:	return "HP_CORE_SHM";
    case PT_HP_CORE_MMF:	return "HP_CORE_MMF";
    case PT_HP_PARALLEL:	return "HP_PARALLEL";
    case PT_HP_FASTBIND:	return "HP_FASTBIND";
    case PT_PARISC_ARCHEXT:	return "PARISC_ARCHEXT";
    case PT_PARISC_UNWIND:	return "PARISC_UNWIND";
    default:
      break;
    }

  return NULL;
}

static const char *
get_segment_type (p_type)
     unsigned long p_type;
{
  static char buff [32];

  switch (p_type)
    {
    case PT_NULL:       return "NULL";
    case PT_LOAD:       return "LOAD";
    case PT_DYNAMIC:	return "DYNAMIC";
    case PT_INTERP:     return "INTERP";
    case PT_NOTE:       return "NOTE";
    case PT_SHLIB:      return "SHLIB";
    case PT_PHDR:       return "PHDR";

    default:
      if ((p_type >= PT_LOPROC) && (p_type <= PT_HIPROC))
	{
	  const char * result;

	  switch (elf_header.e_machine)
	    {
	    case EM_MIPS:
	    case EM_MIPS_RS4_BE:
	      result = get_mips_segment_type (p_type);
	      break;
	    case EM_PARISC:
	      result = get_parisc_segment_type (p_type);
	      break;
	    default:
	      result = NULL;
	      break;
	    }

	  if (result != NULL)
	    return result;

	  sprintf (buff, "LOPROC+%lx", p_type - PT_LOPROC);
	}
      else if ((p_type >= PT_LOOS) && (p_type <= PT_HIOS))
	{
	  const char * result;

	  switch (elf_header.e_machine)
	    {
	    case EM_PARISC:
	      result = get_parisc_segment_type (p_type);
	      break;
	    default:
	      result = NULL;
	      break;
	    }

	  if (result != NULL)
	    return result;

	  sprintf (buff, "LOOS+%lx", p_type - PT_LOOS);
	}
      else
	sprintf (buff, _("<unknown>: %lx"), p_type);

      return buff;
    }
}

static const char *
get_mips_section_type_name (sh_type)
     unsigned int sh_type;
{
  switch (sh_type)
    {
    case SHT_MIPS_LIBLIST:       return "MIPS_LIBLIST";
    case SHT_MIPS_MSYM:          return "MIPS_MSYM";
    case SHT_MIPS_CONFLICT:      return "MIPS_CONFLICT";
    case SHT_MIPS_GPTAB:         return "MIPS_GPTAB";
    case SHT_MIPS_UCODE:         return "MIPS_UCODE";
    case SHT_MIPS_DEBUG:         return "MIPS_DEBUG";
    case SHT_MIPS_REGINFO:       return "MIPS_REGINFO";
    case SHT_MIPS_PACKAGE:       return "MIPS_PACKAGE";
    case SHT_MIPS_PACKSYM:       return "MIPS_PACKSYM";
    case SHT_MIPS_RELD:          return "MIPS_RELD";
    case SHT_MIPS_IFACE:         return "MIPS_IFACE";
    case SHT_MIPS_CONTENT:       return "MIPS_CONTENT";
    case SHT_MIPS_OPTIONS:       return "MIPS_OPTIONS";
    case SHT_MIPS_SHDR:          return "MIPS_SHDR";
    case SHT_MIPS_FDESC:         return "MIPS_FDESC";
    case SHT_MIPS_EXTSYM:        return "MIPS_EXTSYM";
    case SHT_MIPS_DENSE:         return "MIPS_DENSE";
    case SHT_MIPS_PDESC:         return "MIPS_PDESC";
    case SHT_MIPS_LOCSYM:        return "MIPS_LOCSYM";
    case SHT_MIPS_AUXSYM:        return "MIPS_AUXSYM";
    case SHT_MIPS_OPTSYM:        return "MIPS_OPTSYM";
    case SHT_MIPS_LOCSTR:        return "MIPS_LOCSTR";
    case SHT_MIPS_LINE:          return "MIPS_LINE";
    case SHT_MIPS_RFDESC:        return "MIPS_RFDESC";
    case SHT_MIPS_DELTASYM:      return "MIPS_DELTASYM";
    case SHT_MIPS_DELTAINST:     return "MIPS_DELTAINST";
    case SHT_MIPS_DELTACLASS:    return "MIPS_DELTACLASS";
    case SHT_MIPS_DWARF:         return "MIPS_DWARF";
    case SHT_MIPS_DELTADECL:     return "MIPS_DELTADECL";
    case SHT_MIPS_SYMBOL_LIB:    return "MIPS_SYMBOL_LIB";
    case SHT_MIPS_EVENTS:        return "MIPS_EVENTS";
    case SHT_MIPS_TRANSLATE:     return "MIPS_TRANSLATE";
    case SHT_MIPS_PIXIE:         return "MIPS_PIXIE";
    case SHT_MIPS_XLATE:         return "MIPS_XLATE";
    case SHT_MIPS_XLATE_DEBUG:   return "MIPS_XLATE_DEBUG";
    case SHT_MIPS_WHIRL:         return "MIPS_WHIRL";
    case SHT_MIPS_EH_REGION:     return "MIPS_EH_REGION";
    case SHT_MIPS_XLATE_OLD:     return "MIPS_XLATE_OLD";
    case SHT_MIPS_PDR_EXCEPTION: return "MIPS_PDR_EXCEPTION";
    default:
      break;
    }
  return NULL;
}

static const char *
get_parisc_section_type_name (sh_type)
     unsigned int sh_type;
{
  switch (sh_type)
    {
    case SHT_PARISC_EXT:	return "PARISC_EXT";
    case SHT_PARISC_UNWIND:	return "PARISC_UNWIND";
    case SHT_PARISC_DOC:	return "PARISC_DOC";
    default:
      break;
    }
  return NULL;
}

static const char *
get_section_type_name (sh_type)
     unsigned int sh_type;
{
  static char buff [32];

  switch (sh_type)
    {
    case SHT_NULL:		return "NULL";
    case SHT_PROGBITS:		return "PROGBITS";
    case SHT_SYMTAB:		return "SYMTAB";
    case SHT_STRTAB:		return "STRTAB";
    case SHT_RELA:		return "RELA";
    case SHT_HASH:		return "HASH";
    case SHT_DYNAMIC:		return "DYNAMIC";
    case SHT_NOTE:		return "NOTE";
    case SHT_NOBITS:		return "NOBITS";
    case SHT_REL:		return "REL";
    case SHT_SHLIB:		return "SHLIB";
    case SHT_DYNSYM:		return "DYNSYM";
    case SHT_INIT_ARRAY:	return "INIT_ARRAY";
    case SHT_FINI_ARRAY:	return "FINI_ARRAY";
    case SHT_PREINIT_ARRAY:	return "PREINIT_ARRAY";
    case SHT_GNU_verdef:	return "VERDEF";
    case SHT_GNU_verneed:	return "VERNEED";
    case SHT_GNU_versym:	return "VERSYM";
    case 0x6ffffff0:	        return "VERSYM";
    case 0x6ffffffc:	        return "VERDEF";
    case 0x7ffffffd:		return "AUXILIARY";
    case 0x7fffffff:		return "FILTER";

    default:
      if ((sh_type >= SHT_LOPROC) && (sh_type <= SHT_HIPROC))
	{
	  const char * result;

	  switch (elf_header.e_machine)
	    {
	    case EM_MIPS:
	    case EM_MIPS_RS4_BE:
	      result = get_mips_section_type_name (sh_type);
	      break;
	    case EM_PARISC:
	      result = get_parisc_section_type_name (sh_type);
	      break;
	    default:
	      result = NULL;
	      break;
	    }

	  if (result != NULL)
	    return result;

	  sprintf (buff, "SHT_LOPROC+%x", sh_type - SHT_LOPROC);
	}
      else if ((sh_type >= SHT_LOOS) && (sh_type <= SHT_HIOS))
	sprintf (buff, "SHT_LOOS+%x", sh_type - SHT_LOOS);
      else if ((sh_type >= SHT_LOUSER) && (sh_type <= SHT_HIUSER))
	sprintf (buff, "SHT_LOUSER+%x", sh_type - SHT_LOUSER);
      else
	sprintf (buff, _("<unknown>: %x"), sh_type);

      return buff;
    }
}

struct option options [] =
{
  {"all",              no_argument, 0, 'a'},
  {"file-header",      no_argument, 0, 'h'},
  {"program-headers",  no_argument, 0, 'l'},
  {"headers",          no_argument, 0, 'e'},
  {"histogram",        no_argument, 0, 'I'},
  {"segments",         no_argument, 0, 'l'},
  {"sections",         no_argument, 0, 'S'},
  {"section-headers",  no_argument, 0, 'S'},
  {"symbols",          no_argument, 0, 's'},
  {"syms",             no_argument, 0, 's'},
  {"relocs",           no_argument, 0, 'r'},
  {"notes",            no_argument, 0, 'n'},
  {"dynamic",          no_argument, 0, 'd'},
  {"arch-specific",    no_argument, 0, 'A'},
  {"version-info",     no_argument, 0, 'V'},
  {"use-dynamic",      no_argument, 0, 'D'},
  {"hex-dump",         required_argument, 0, 'x'},
  {"debug-dump",       optional_argument, 0, 'w'},
#ifdef SUPPORT_DISASSEMBLY
  {"instruction-dump", required_argument, 0, 'i'},
#endif

  {"version",          no_argument, 0, 'v'},
  {"help",             no_argument, 0, 'H'},
  {0,                  no_argument, 0, 0}
};

static void
usage ()
{
  fprintf (stdout, _("Usage: readelf {options} elf-file(s)\n"));
  fprintf (stdout, _("  Options are:\n"));
  fprintf (stdout, _("  -a or --all               Equivalent to: -h -l -S -s -r -d -V -A -I\n"));
  fprintf (stdout, _("  -h or --file-header       Display the ELF file header\n"));
  fprintf (stdout, _("  -l or --program-headers or --segments\n"));
  fprintf (stdout, _("                            Display the program headers\n"));
  fprintf (stdout, _("  -S or --section-headers or --sections\n"));
  fprintf (stdout, _("                            Display the sections' header\n"));
  fprintf (stdout, _("  -e or --headers           Equivalent to: -h -l -S\n"));
  fprintf (stdout, _("  -s or --syms or --symbols Display the symbol table\n"));
  fprintf (stdout, _("  -n or --notes             Display the core notes (if present)\n"));
  fprintf (stdout, _("  -r or --relocs            Display the relocations (if present)\n"));
  fprintf (stdout, _("  -d or --dynamic           Display the dynamic segment (if present)\n"));
  fprintf (stdout, _("  -V or --version-info      Display the version sections (if present)\n"));
  fprintf (stdout, _("  -A or --arch-specific     Display architecture specific information (if any).\n"));
  fprintf (stdout, _("  -D or --use-dynamic       Use the dynamic section info when displaying symbols\n"));
  fprintf (stdout, _("  -x <number> or --hex-dump=<number>\n"));
  fprintf (stdout, _("                            Dump the contents of section <number>\n"));
  fprintf (stdout, _("  -w[liapr] or --debug-dump[=line,=info,=abbrev,=pubnames,=ranges]\n"));
  fprintf (stdout, _("                            Display the contents of DWARF2 debug sections\n"));
#ifdef SUPPORT_DISASSEMBLY
  fprintf (stdout, _("  -i <number> or --instruction-dump=<number>\n"));
  fprintf (stdout, _("                            Disassemble the contents of section <number>\n"));
#endif
  fprintf (stdout, _("  -I or --histogram         Display histogram of bucket list lengths\n"));
  fprintf (stdout, _("  -v or --version           Display the version number of readelf\n"));
  fprintf (stdout, _("  -H or --help              Display this information\n"));
  fprintf (stdout, _("Report bugs to %s\n"), REPORT_BUGS_TO);

  exit (0);
}

static void
request_dump (section, type)
     unsigned int section;
     char         type;
{
  if (section >= num_dump_sects)
    {
      char * new_dump_sects;

      new_dump_sects = (char *) calloc (section + 1, 1);

      if (new_dump_sects == NULL)
	error (_("Out of memory allocating dump request table."));
      else
	{
	  /* Copy current flag settings.  */
	  memcpy (new_dump_sects, dump_sects, num_dump_sects);

	  free (dump_sects);

	  dump_sects = new_dump_sects;
	  num_dump_sects = section + 1;
	}
    }

  if (dump_sects)
    dump_sects [section] |= type;

  return;
}

static void
parse_args (argc, argv)
     int argc;
     char ** argv;
{
  int c;

  if (argc < 2)
    usage ();

  while ((c = getopt_long
	  (argc, argv, "ersahnldSDAIw::x:i:vV", options, NULL)) != EOF)
    {
      char *    cp;
      int	section;

      switch (c)
	{
	case 0:
	  /* Long options.  */
	  break;
	case 'H':
	  usage ();
	  break;

	case 'a':
	  do_syms ++;
	  do_reloc ++;
	  do_dynamic ++;
	  do_header ++;
	  do_sections ++;
	  do_segments ++;
	  do_version ++;
	  do_histogram ++;
	  do_arch ++;
	  do_notes ++;
	  break;
	case 'e':
	  do_header ++;
	  do_sections ++;
	  do_segments ++;
	  break;
	case 'A':
	  do_arch ++;
	  break;
	case 'D':
	  do_using_dynamic ++;
	  break;
	case 'r':
	  do_reloc ++;
	  break;
	case 'h':
	  do_header ++;
	  break;
	case 'l':
	  do_segments ++;
	  break;
	case 's':
	  do_syms ++;
	  break;
	case 'S':
	  do_sections ++;
	  break;
	case 'd':
	  do_dynamic ++;
	  break;
	case 'I':
	  do_histogram ++;
	  break;
	case 'n':
	  do_notes ++;
	  break;
	case 'x':
	  do_dump ++;
	  section = strtoul (optarg, & cp, 0);
	  if (! * cp && section >= 0)
	    {
	      request_dump (section, HEX_DUMP);
	      break;
	    }
	  goto oops;
	case 'w':
	  do_dump ++;
	  if (optarg == 0)
	    do_debugging = 1;
	  else
	    {
	      do_debugging = 0;
	      switch (optarg[0])
		{
		case 'i':
		case 'I':
		  do_debug_info = 1;
		  break;

		case 'a':
		case 'A':
		  do_debug_abbrevs = 1;
		  break;

		case 'l':
		case 'L':
		  do_debug_lines = 1;
		  break;

		case 'p':
		case 'P':
		  do_debug_pubnames = 1;
		  break;

		case 'r':
		case 'R':
		  do_debug_aranges = 1;
		  break;

		default:
		  warn (_("Unrecognised debug option '%s'\n"), optarg);
		  break;
		}
	    }
	  break;
#ifdef SUPPORT_DISASSEMBLY
	case 'i':
	  do_dump ++;
	  section = strtoul (optarg, & cp, 0);
	  if (! * cp && section >= 0)
	    {
	      request_dump (section, DISASS_DUMP);
	      break;
	    }
	  goto oops;
#endif
	case 'v':
	  print_version (program_name);
	  break;
	case 'V':
	  do_version ++;
	  break;
	default:
	oops:
	  /* xgettext:c-format */
	  error (_("Invalid option '-%c'\n"), c);
	  /* Drop through.  */
	case '?':
	  usage ();
	}
    }

  if (!do_dynamic && !do_syms && !do_reloc && !do_sections
      && !do_segments && !do_header && !do_dump && !do_version
      && !do_histogram && !do_debugging && !do_arch && !do_notes)
    usage ();
  else if (argc < 3)
    {
      warn (_("Nothing to do.\n"));
      usage();
    }
}

static const char *
get_elf_class (elf_class)
     unsigned char elf_class;
{
  static char buff [32];

  switch (elf_class)
    {
    case ELFCLASSNONE: return _("none");
    case ELFCLASS32:   return _("ELF32");
    case ELFCLASS64:   return _("ELF64");
    default:
      sprintf (buff, _("<unknown: %x>"), elf_class);
      return buff;
    }
}

static const char *
get_data_encoding (encoding)
     unsigned char encoding;
{
  static char buff [32];

  switch (encoding)
    {
    case ELFDATANONE: return _("none");
    case ELFDATA2LSB: return _("2's complement, little endian");
    case ELFDATA2MSB: return _("2's complement, big endian");
    default:
      sprintf (buff, _("<unknown: %x>"), encoding);
      return buff;
    }
}

static const char *
get_osabi_name (osabi)
     unsigned char osabi;
{
  static char buff [32];

  switch (osabi)
    {
    case ELFOSABI_NONE:       return _("UNIX - System V");
    case ELFOSABI_HPUX:       return _("UNIX - HP-UX");
    case ELFOSABI_NETBSD:     return _("UNIX - NetBSD");
    case ELFOSABI_LINUX:      return _("UNIX - Linux");
    case ELFOSABI_HURD:       return _("GNU/Hurd");
    case ELFOSABI_SOLARIS:    return _("UNIX - Solaris");
    case ELFOSABI_MONTEREY:   return _("UNIX - Monterey");
    case ELFOSABI_IRIX:       return _("UNIX - IRIX");
    case ELFOSABI_FREEBSD:    return _("UNIX - FreeBSD");
    case ELFOSABI_TRU64:      return _("UNIX - TRU64");
    case ELFOSABI_MODESTO:    return _("Novell - Modesto");
    case ELFOSABI_OPENBSD:    return _("UNIX - OpenBSD");
    case ELFOSABI_STANDALONE: return _("Standalone App");
    case ELFOSABI_ARM:        return _("ARM");
    default:
      sprintf (buff, _("<unknown: %x>"), osabi);
      return buff;
    }
}

/* Decode the data held in 'elf_header'.  */
static int
process_file_header ()
{
  if (   elf_header.e_ident [EI_MAG0] != ELFMAG0
      || elf_header.e_ident [EI_MAG1] != ELFMAG1
      || elf_header.e_ident [EI_MAG2] != ELFMAG2
      || elf_header.e_ident [EI_MAG3] != ELFMAG3)
    {
      error
	(_("Not an ELF file - it has the wrong magic bytes at the start\n"));
      return 0;
    }

  if (do_header)
    {
      int i;

      printf (_("ELF Header:\n"));
      printf (_("  Magic:   "));
      for (i = 0; i < EI_NIDENT; i ++)
	printf ("%2.2x ", elf_header.e_ident [i]);
      printf ("\n");
      printf (_("  Class:                             %s\n"),
	      get_elf_class (elf_header.e_ident [EI_CLASS]));
      printf (_("  Data:                              %s\n"),
	      get_data_encoding (elf_header.e_ident [EI_DATA]));
      printf (_("  Version:                           %d %s\n"),
	      elf_header.e_ident [EI_VERSION],
	      (elf_header.e_ident [EI_VERSION] == EV_CURRENT
	       ? "(current)"
	       : (elf_header.e_ident [EI_VERSION] != EV_NONE
		  ? "<unknown: %lx>"
		  : "")));
      printf (_("  OS/ABI:                            %s\n"),
	      get_osabi_name (elf_header.e_ident [EI_OSABI]));
      printf (_("  ABI Version:                       %d\n"),
	      elf_header.e_ident [EI_ABIVERSION]);
      printf (_("  Type:                              %s\n"),
	      get_file_type (elf_header.e_type));
      printf (_("  Machine:                           %s\n"),
	      get_machine_name (elf_header.e_machine));
      printf (_("  Version:                           0x%lx\n"),
	      (unsigned long) elf_header.e_version);
      
      printf (_("  Entry point address:               "));
      print_vma ((bfd_vma) elf_header.e_entry, PREFIX_HEX);
      printf (_("\n  Start of program headers:          "));
      print_vma ((bfd_vma) elf_header.e_phoff, DEC);
      printf (_(" (bytes into file)\n  Start of section headers:          "));
      print_vma ((bfd_vma) elf_header.e_shoff, DEC);
      printf (_(" (bytes into file)\n"));
	
      printf (_("  Flags:                             0x%lx%s\n"),
	      (unsigned long) elf_header.e_flags,
	      get_machine_flags (elf_header.e_flags, elf_header.e_machine));
      printf (_("  Size of this header:               %ld (bytes)\n"),
	      (long) elf_header.e_ehsize);
      printf (_("  Size of program headers:           %ld (bytes)\n"),
	      (long) elf_header.e_phentsize);
      printf (_("  Number of program headers:         %ld\n"),
	      (long) elf_header.e_phnum);
      printf (_("  Size of section headers:           %ld (bytes)\n"),
	      (long) elf_header.e_shentsize);
      printf (_("  Number of section headers:         %ld\n"),
	      (long) elf_header.e_shnum);
      printf (_("  Section header string table index: %ld\n"),
	      (long) elf_header.e_shstrndx);
    }

  return 1;
}


static int
get_32bit_program_headers (file, program_headers)
     FILE * file;
     Elf_Internal_Phdr * program_headers;
{
  Elf32_External_Phdr * phdrs;
  Elf32_External_Phdr * external;
  Elf32_Internal_Phdr * internal;
  unsigned int          i;

  GET_DATA_ALLOC (elf_header.e_phoff,
		  elf_header.e_phentsize * elf_header.e_phnum,
		  phdrs, Elf32_External_Phdr *, "program headers");

  for (i = 0, internal = program_headers, external = phdrs;
       i < elf_header.e_phnum;
       i ++, internal ++, external ++)
    {
      internal->p_type   = BYTE_GET (external->p_type);
      internal->p_offset = BYTE_GET (external->p_offset);
      internal->p_vaddr  = BYTE_GET (external->p_vaddr);
      internal->p_paddr  = BYTE_GET (external->p_paddr);
      internal->p_filesz = BYTE_GET (external->p_filesz);
      internal->p_memsz  = BYTE_GET (external->p_memsz);
      internal->p_flags  = BYTE_GET (external->p_flags);
      internal->p_align  = BYTE_GET (external->p_align);
    }

  free (phdrs);

  return 1;
}

static int
get_64bit_program_headers (file, program_headers)
     FILE * file;
     Elf_Internal_Phdr * program_headers;
{
  Elf64_External_Phdr * phdrs;
  Elf64_External_Phdr * external;
  Elf64_Internal_Phdr * internal;
  unsigned int          i;

  GET_DATA_ALLOC (elf_header.e_phoff,
		  elf_header.e_phentsize * elf_header.e_phnum,
		  phdrs, Elf64_External_Phdr *, "program headers");

  for (i = 0, internal = program_headers, external = phdrs;
       i < elf_header.e_phnum;
       i ++, internal ++, external ++)
    {
      internal->p_type   = BYTE_GET (external->p_type);
      internal->p_flags  = BYTE_GET (external->p_flags);
      internal->p_offset = BYTE_GET8 (external->p_offset);
      internal->p_vaddr  = BYTE_GET8 (external->p_vaddr);
      internal->p_paddr  = BYTE_GET8 (external->p_paddr);
      internal->p_filesz = BYTE_GET8 (external->p_filesz);
      internal->p_memsz  = BYTE_GET8 (external->p_memsz);
      internal->p_align  = BYTE_GET8 (external->p_align);
    }

  free (phdrs);

  return 1;
}

static int
process_program_headers (file)
     FILE * file;
{
  Elf_Internal_Phdr * program_headers;
  Elf_Internal_Phdr * segment;
  unsigned int	      i;

  if (elf_header.e_phnum == 0)
    {
      if (do_segments)
	printf (_("\nThere are no program headers in this file.\n"));
      return 1;
    }

  if (do_segments && !do_header)
    {
      printf (_("\nElf file type is %s\n"), get_file_type (elf_header.e_type));
      printf (_("Entry point "));
      print_vma ((bfd_vma) elf_header.e_entry, PREFIX_HEX);
      printf (_("\nThere are %d program headers, starting at offset "),
	      elf_header.e_phnum);
      print_vma ((bfd_vma) elf_header.e_phoff, DEC);
      printf ("\n");
    }

  program_headers = (Elf_Internal_Phdr *) malloc
    (elf_header.e_phnum * sizeof (Elf_Internal_Phdr));

  if (program_headers == NULL)
    {
      error (_("Out of memory\n"));
      return 0;
    }

  if (is_32bit_elf)
    i = get_32bit_program_headers (file, program_headers);
  else
    i = get_64bit_program_headers (file, program_headers);

  if (i == 0)
    {
      free (program_headers);
      return 0;
    }

  if (do_segments)
    {
      printf
	(_("\nProgram Header%s:\n"), elf_header.e_phnum > 1 ? "s" : "");
      
      if (is_32bit_elf)
	printf
	  (_("  Type           Offset   VirtAddr   PhysAddr   FileSiz MemSiz  Flg Align\n"));
      else
	{
	  printf
	    (_("  Type           Offset             VirtAddr           PhysAddr\n"));
	  printf
	    (_("                 FileSiz            MemSiz              Flags  Align\n"));
	}
    }

  loadaddr = -1;
  dynamic_addr = 0;
  dynamic_size = 0;

  for (i = 0, segment = program_headers;
       i < elf_header.e_phnum;
       i ++, segment ++)
    {
      if (do_segments)
	{
	  printf ("  %-14.14s ", get_segment_type (segment->p_type));

	  if (is_32bit_elf)
	    {
	      printf ("0x%6.6lx ", (unsigned long) segment->p_offset);
	      printf ("0x%8.8lx ", (unsigned long) segment->p_vaddr);
	      printf ("0x%8.8lx ", (unsigned long) segment->p_paddr);
	      printf ("0x%5.5lx ", (unsigned long) segment->p_filesz);
	      printf ("0x%5.5lx ", (unsigned long) segment->p_memsz);
	      printf ("%c%c%c ",
		      (segment->p_flags & PF_R ? 'R' : ' '),
		      (segment->p_flags & PF_W ? 'W' : ' '),
		      (segment->p_flags & PF_X ? 'E' : ' '));
	      printf ("%#lx", (unsigned long) segment->p_align);
	    }
	  else
	    {
	      print_vma (segment->p_offset, FULL_HEX);
	      putchar (' ');
	      print_vma (segment->p_vaddr, FULL_HEX);
	      putchar (' ');
	      print_vma (segment->p_paddr, FULL_HEX);
	      printf ("\n                 ");
	      print_vma (segment->p_filesz, FULL_HEX);
	      putchar (' ');
	      print_vma (segment->p_memsz, FULL_HEX);
	      printf ("  %c%c%c    ",
		      (segment->p_flags & PF_R ? 'R' : ' '),
		      (segment->p_flags & PF_W ? 'W' : ' '),
		      (segment->p_flags & PF_X ? 'E' : ' '));
	      print_vma (segment->p_align, HEX);
	    }
	}

      switch (segment->p_type)
	{
	case PT_LOAD:
	  if (loadaddr == -1)
	    loadaddr = (segment->p_vaddr & 0xfffff000)
	      - (segment->p_offset & 0xfffff000);
	  break;

	case PT_DYNAMIC:
	  if (dynamic_addr)
	    error (_("more than one dynamic segment\n"));

	  dynamic_addr = segment->p_offset;
	  dynamic_size = segment->p_filesz;
	  break;

	case PT_INTERP:
	  if (fseek (file, (long) segment->p_offset, SEEK_SET))
	    error (_("Unable to find program interpreter name\n"));
	  else
	    {
	      program_interpreter[0] = 0;
	      fscanf (file, "%63s", program_interpreter);

	      if (do_segments)
		printf (_("\n      [Requesting program interpreter: %s]"),
		    program_interpreter);
	    }
	  break;
	}

      if (do_segments)
	putc ('\n', stdout);
    }

  if (loadaddr == -1)
    {
      /* Very strange. */
      loadaddr = 0;
    }

  if (do_segments && section_headers != NULL)
    {
      printf (_("\n Section to Segment mapping:\n"));
      printf (_("  Segment Sections...\n"));

      assert (string_table != NULL);

      for (i = 0; i < elf_header.e_phnum; i++)
	{
	  int                 j;
	  Elf_Internal_Shdr * section;

	  segment = program_headers + i;
	  section = section_headers;

	  printf ("   %2.2d     ", i);

	  for (j = 0; j < elf_header.e_shnum; j++, section ++)
	    {
	      if (section->sh_size > 0
		  /* Compare allocated sections by VMA, unallocated
		     sections by file offset.  */
		  && (section->sh_flags & SHF_ALLOC
		      ? (section->sh_addr >= segment->p_vaddr
			 && section->sh_addr + section->sh_size
			 <= segment->p_vaddr + segment->p_memsz)
		      : ((bfd_vma) section->sh_offset >= segment->p_offset
			 && (section->sh_offset + section->sh_size
			     <= segment->p_offset + segment->p_filesz))))
		printf ("%s ", SECTION_NAME (section));
	    }

	  putc ('\n',stdout);
	}
    }

  free (program_headers);

  return 1;
}


static int
get_32bit_section_headers (file)
     FILE * file;
{
  Elf32_External_Shdr * shdrs;
  Elf32_Internal_Shdr * internal;
  unsigned int          i;

  GET_DATA_ALLOC (elf_header.e_shoff,
		  elf_header.e_shentsize * elf_header.e_shnum,
		  shdrs, Elf32_External_Shdr *, "section headers");

  section_headers = (Elf_Internal_Shdr *) malloc
    (elf_header.e_shnum * sizeof (Elf_Internal_Shdr));

  if (section_headers == NULL)
    {
      error (_("Out of memory\n"));
      return 0;
    }

  for (i = 0, internal = section_headers;
       i < elf_header.e_shnum;
       i ++, internal ++)
    {
      internal->sh_name      = BYTE_GET (shdrs[i].sh_name);
      internal->sh_type      = BYTE_GET (shdrs[i].sh_type);
      internal->sh_flags     = BYTE_GET (shdrs[i].sh_flags);
      internal->sh_addr      = BYTE_GET (shdrs[i].sh_addr);
      internal->sh_offset    = BYTE_GET (shdrs[i].sh_offset);
      internal->sh_size      = BYTE_GET (shdrs[i].sh_size);
      internal->sh_link      = BYTE_GET (shdrs[i].sh_link);
      internal->sh_info      = BYTE_GET (shdrs[i].sh_info);
      internal->sh_addralign = BYTE_GET (shdrs[i].sh_addralign);
      internal->sh_entsize   = BYTE_GET (shdrs[i].sh_entsize);
    }

  free (shdrs);

  return 1;
}

static int
get_64bit_section_headers (file)
     FILE * file;
{
  Elf64_External_Shdr * shdrs;
  Elf64_Internal_Shdr * internal;
  unsigned int          i;

  GET_DATA_ALLOC (elf_header.e_shoff,
		  elf_header.e_shentsize * elf_header.e_shnum,
		  shdrs, Elf64_External_Shdr *, "section headers");

  section_headers = (Elf_Internal_Shdr *) malloc
    (elf_header.e_shnum * sizeof (Elf_Internal_Shdr));

  if (section_headers == NULL)
    {
      error (_("Out of memory\n"));
      return 0;
    }

  for (i = 0, internal = section_headers;
       i < elf_header.e_shnum;
       i ++, internal ++)
    {
      internal->sh_name      = BYTE_GET (shdrs[i].sh_name);
      internal->sh_type      = BYTE_GET (shdrs[i].sh_type);
      internal->sh_flags     = BYTE_GET8 (shdrs[i].sh_flags);
      internal->sh_addr      = BYTE_GET8 (shdrs[i].sh_addr);
      internal->sh_size      = BYTE_GET8 (shdrs[i].sh_size);
      internal->sh_entsize   = BYTE_GET8 (shdrs[i].sh_entsize);
      internal->sh_link      = BYTE_GET (shdrs[i].sh_link);
      internal->sh_info      = BYTE_GET (shdrs[i].sh_info);
      internal->sh_offset    = BYTE_GET (shdrs[i].sh_offset);
      internal->sh_addralign = BYTE_GET (shdrs[i].sh_addralign);
    }

  free (shdrs);

  return 1;
}

static Elf_Internal_Sym *
get_32bit_elf_symbols (file, offset, number)
     FILE * file;
     unsigned long offset;
     unsigned long number;
{
  Elf32_External_Sym * esyms;
  Elf_Internal_Sym *   isyms;
  Elf_Internal_Sym *   psym;
  unsigned int         j;

  GET_DATA_ALLOC (offset, number * sizeof (Elf32_External_Sym),
		  esyms, Elf32_External_Sym *, "symbols");

  isyms = (Elf_Internal_Sym *) malloc (number * sizeof (Elf_Internal_Sym));

  if (isyms == NULL)
    {
      error (_("Out of memory\n"));
      free (esyms);

      return NULL;
    }

  for (j = 0, psym = isyms;
       j < number;
       j ++, psym ++)
    {
      psym->st_name  = BYTE_GET (esyms[j].st_name);
      psym->st_value = BYTE_GET (esyms[j].st_value);
      psym->st_size  = BYTE_GET (esyms[j].st_size);
      psym->st_shndx = BYTE_GET (esyms[j].st_shndx);
      psym->st_info  = BYTE_GET (esyms[j].st_info);
      psym->st_other = BYTE_GET (esyms[j].st_other);
    }

  free (esyms);

  return isyms;
}

static Elf_Internal_Sym *
get_64bit_elf_symbols (file, offset, number)
     FILE * file;
     unsigned long offset;
     unsigned long number;
{
  Elf64_External_Sym * esyms;
  Elf_Internal_Sym *   isyms;
  Elf_Internal_Sym *   psym;
  unsigned int         j;

  GET_DATA_ALLOC (offset, number * sizeof (Elf64_External_Sym),
		  esyms, Elf64_External_Sym *, "symbols");

  isyms = (Elf_Internal_Sym *) malloc (number * sizeof (Elf_Internal_Sym));

  if (isyms == NULL)
    {
      error (_("Out of memory\n"));
      free (esyms);

      return NULL;
    }

  for (j = 0, psym = isyms;
       j < number;
       j ++, psym ++)
    {
      psym->st_name  = BYTE_GET (esyms[j].st_name);
      psym->st_info  = BYTE_GET (esyms[j].st_info);
      psym->st_other = BYTE_GET (esyms[j].st_other);
      psym->st_shndx = BYTE_GET (esyms[j].st_shndx);
      psym->st_value = BYTE_GET8 (esyms[j].st_value);
      psym->st_size  = BYTE_GET8 (esyms[j].st_size);
    }

  free (esyms);

  return isyms;
}

static const char *
get_elf_section_flags (sh_flags)
     bfd_vma sh_flags;
{
  static char buff [32];

  * buff = 0;
  
  while (sh_flags)
    {
      bfd_vma flag;

      flag = sh_flags & - sh_flags;
      sh_flags &= ~ flag;
      
      switch (flag)
	{
	case SHF_WRITE:            strcat (buff, "W"); break;
	case SHF_ALLOC:            strcat (buff, "A"); break;
	case SHF_EXECINSTR:        strcat (buff, "X"); break;
	case SHF_MERGE:            strcat (buff, "M"); break;
	case SHF_STRINGS:          strcat (buff, "S"); break;
	case SHF_INFO_LINK:        strcat (buff, "I"); break;
	case SHF_LINK_ORDER:       strcat (buff, "L"); break;
	case SHF_OS_NONCONFORMING: strcat (buff, "O"); break;
	  
	default:
	  if (flag & SHF_MASKOS)
	    {
	      strcat (buff, "o");
	      sh_flags &= ~ SHF_MASKOS;
	    }
	  else if (flag & SHF_MASKPROC)
	    {
	      strcat (buff, "p");
	      sh_flags &= ~ SHF_MASKPROC;
	    }
	  else
	    strcat (buff, "x");
	  break;
	}
    }
  
  return buff;
}

static int
process_section_headers (file)
     FILE * file;
{
  Elf_Internal_Shdr * section;
  int                 i;

  section_headers = NULL;

  if (elf_header.e_shnum == 0)
    {
      if (do_sections)
	printf (_("\nThere are no sections in this file.\n"));

      return 1;
    }

  if (do_sections && !do_header)
    printf (_("There are %d section headers, starting at offset 0x%lx:\n"),
	    elf_header.e_shnum, (unsigned long) elf_header.e_shoff);

  if (is_32bit_elf)
    {
      if (! get_32bit_section_headers (file))
	return 0;
    }
  else if (! get_64bit_section_headers (file))
    return 0;

  /* Read in the string table, so that we have names to display.  */
  section = section_headers + elf_header.e_shstrndx;

  if (section->sh_size != 0)
    {
      unsigned long string_table_offset;

      string_table_offset = section->sh_offset;

      GET_DATA_ALLOC (section->sh_offset, section->sh_size,
		      string_table, char *, "string table");
    }

  /* Scan the sections for the dynamic symbol table
     and dynamic string table and debug sections. */
  dynamic_symbols = NULL;
  dynamic_strings = NULL;
  dynamic_syminfo = NULL;

  for (i = 0, section = section_headers;
       i < elf_header.e_shnum;
       i ++, section ++)
    {
      char * name = SECTION_NAME (section);

      if (section->sh_type == SHT_DYNSYM)
	{
	  if (dynamic_symbols != NULL)
	    {
	      error (_("File contains multiple dynamic symbol tables\n"));
	      continue;
	    }

	  num_dynamic_syms = section->sh_size / section->sh_entsize;
	  dynamic_symbols =
	    GET_ELF_SYMBOLS (file, section->sh_offset, num_dynamic_syms);
	}
      else if (section->sh_type == SHT_STRTAB
	       && strcmp (name, ".dynstr") == 0)
	{
	  if (dynamic_strings != NULL)
	    {
	      error (_("File contains multiple dynamic string tables\n"));
	      continue;
	    }

	  GET_DATA_ALLOC (section->sh_offset, section->sh_size,
			  dynamic_strings, char *, "dynamic strings");
	}
      else if ((do_debugging || do_debug_info || do_debug_abbrevs
		|| do_debug_lines || do_debug_pubnames || do_debug_aranges)
	       && strncmp (name, ".debug_", 7) == 0)
	{
	  name += 7;

	  if (do_debugging
	      || (do_debug_info     && (strcmp (name, "info") == 0))
	      || (do_debug_abbrevs  && (strcmp (name, "abbrev") == 0))
	      || (do_debug_lines    && (strcmp (name, "line") == 0))
	      || (do_debug_pubnames && (strcmp (name, "pubnames") == 0))
	      || (do_debug_aranges  && (strcmp (name, "aranges") == 0))
	      )
	    request_dump (i, DEBUG_DUMP);
	}
    }

  if (! do_sections)
    return 1;

  printf (_("\nSection Header%s:\n"), elf_header.e_shnum > 1 ? "s" : "");
  
  if (is_32bit_elf)
    printf
      (_("  [Nr] Name              Type            Addr     Off    Size   ES Flg Lk Inf Al\n"));
  else
    {
      printf (_("  [Nr] Name              Type             Address           Offset\n"));
      printf (_("       Size              EntSize          Flags  Link  Info  Align\n"));
    }

  for (i = 0, section = section_headers;
       i < elf_header.e_shnum;
       i ++, section ++)
    {
      printf ("  [%2d] %-17.17s %-15.15s ",
	      i,
	      SECTION_NAME (section),
	      get_section_type_name (section->sh_type));

      if (is_32bit_elf)
	{
	  print_vma (section->sh_addr, LONG_HEX);
      
	  printf ( " %6.6lx %6.6lx %2.2lx",
		   (unsigned long) section->sh_offset,
		   (unsigned long) section->sh_size,
		   (unsigned long) section->sh_entsize);

	  printf (" %3s ", get_elf_section_flags (section->sh_flags));
		  
	  printf (" %2ld %3lx %ld\n",
		  (unsigned long) section->sh_link,
		  (unsigned long) section->sh_info,
		  (unsigned long) section->sh_addralign);
	}
      else
	{
	  putchar (' ');
	  print_vma (section->sh_addr, LONG_HEX);
	  printf ("  %8.8lx", section->sh_offset);
	  printf ("\n       ");
	  print_vma (section->sh_size, LONG_HEX);
	  printf ("  ");
	  print_vma (section->sh_entsize, LONG_HEX);
	  
	  printf (" %3s ", get_elf_section_flags (section->sh_flags));
		  
	  printf ("     %2ld   %3lx     %ld\n",
		  (unsigned long) section->sh_link,
		  (unsigned long) section->sh_info,
		  (unsigned long) section->sh_addralign);
	}
    }

  printf (_("Key to Flags: W (write), A (alloc), X (execute), M (merge), S (strings)\n"));
  printf (_("              I (info), L (link order), O (extra OS processing required)\n"));
  printf (_("              o (os specific), p (processor specific) x (unknown)\n"));

  return 1;
}

/* Process the reloc section.  */
static int
process_relocs (file)
     FILE * file;
{
  unsigned long    rel_size;
  unsigned long	   rel_offset;


  if (!do_reloc)
    return 1;

  if (do_using_dynamic)
    {
      int is_rela = FALSE;

      rel_size   = 0;
      rel_offset = 0;

      if (dynamic_info[DT_REL])
	{
	  rel_offset = dynamic_info[DT_REL];
	  rel_size   = dynamic_info[DT_RELSZ];
	  is_rela    = FALSE;
	}
      else if (dynamic_info [DT_RELA])
	{
	  rel_offset = dynamic_info[DT_RELA];
	  rel_size   = dynamic_info[DT_RELASZ];
	  is_rela    = TRUE;
	}
      else if (dynamic_info[DT_JMPREL])
	{
	  rel_offset = dynamic_info[DT_JMPREL];
	  rel_size   = dynamic_info[DT_PLTRELSZ];

	  switch (dynamic_info[DT_PLTREL])
	    {
	    case DT_REL:
	      is_rela = FALSE;
	      break;
	    case DT_RELA:
	      is_rela = TRUE;
	      break;
	    default:
	      is_rela = UNKNOWN;
	      break;
	    }
	}

      if (rel_size)
	{
	  printf
	    (_("\nRelocation section at offset 0x%lx contains %ld bytes:\n"),
	     rel_offset, rel_size);

	  dump_relocations (file, rel_offset - loadaddr, rel_size,
			    dynamic_symbols, num_dynamic_syms, dynamic_strings, is_rela);
	}
      else
	printf (_("\nThere are no dynamic relocations in this file.\n"));
    }
  else
    {
      Elf32_Internal_Shdr *     section;
      unsigned long 		i;
      int           		found = 0;

      for (i = 0, section = section_headers;
	   i < elf_header.e_shnum;
	   i++, section ++)
	{
	  if (   section->sh_type != SHT_RELA
	      && section->sh_type != SHT_REL)
	    continue;

	  rel_offset = section->sh_offset;
	  rel_size   = section->sh_size;

	  if (rel_size)
	    {
	      Elf32_Internal_Shdr * strsec;
	      Elf32_Internal_Shdr * symsec;
	      Elf_Internal_Sym *    symtab;
	      char *                strtab;
	      int                   is_rela;
	      unsigned long         nsyms;

	      printf (_("\nRelocation section "));

	      if (string_table == NULL)
		printf ("%d", section->sh_name);
	      else
		printf ("'%s'", SECTION_NAME (section));

	      printf (_(" at offset 0x%lx contains %lu entries:\n"),
		 rel_offset, (unsigned long) (rel_size / section->sh_entsize));

	      symsec = section_headers + section->sh_link;

	      nsyms = symsec->sh_size / symsec->sh_entsize;
	      symtab = GET_ELF_SYMBOLS (file, symsec->sh_offset, nsyms);

	      if (symtab == NULL)
		continue;

	      strsec = section_headers + symsec->sh_link;

	      GET_DATA_ALLOC (strsec->sh_offset, strsec->sh_size, strtab,
			      char *, "string table");

	      is_rela = section->sh_type == SHT_RELA;

	      dump_relocations (file, rel_offset, rel_size, symtab, nsyms, strtab, is_rela);

	      free (strtab);
	      free (symtab);

	      found = 1;
	    }
	}

      if (! found)
	printf (_("\nThere are no relocations in this file.\n"));
    }

  return 1;
}


static void
dynamic_segment_mips_val (entry)
     Elf_Internal_Dyn * entry;
{
  switch (entry->d_tag)
    {
    case DT_MIPS_FLAGS:
      if (entry->d_un.d_val == 0)
	printf ("NONE\n");
      else
	{
	  static const char * opts[] =
	  {
	    "QUICKSTART", "NOTPOT", "NO_LIBRARY_REPLACEMENT",
	    "NO_MOVE", "SGI_ONLY", "GUARANTEE_INIT", "DELTA_C_PLUS_PLUS",
	    "GUARANTEE_START_INIT", "PIXIE", "DEFAULT_DELAY_LOAD",
	    "REQUICKSTART", "REQUICKSTARTED", "CORD", "NO_UNRES_UNDEF",
	    "RLD_ORDER_SAFE"
	  };
	  unsigned int cnt;
	  int first = 1;
	  for (cnt = 0; cnt < NUM_ELEM (opts); ++ cnt)
	    if (entry->d_un.d_val & (1 << cnt))
	      {
		printf ("%s%s", first ? "" : " ", opts[cnt]);
		first = 0;
	      }
	  puts ("");
	}
      break;

    case DT_MIPS_IVERSION:
      if (dynamic_strings != NULL)
	printf ("Interface Version: %s\n",
		dynamic_strings + entry->d_un.d_val);
      else
	printf ("%ld\n", (long) entry->d_un.d_ptr);
      break;

    case DT_MIPS_TIME_STAMP:
      {
	char timebuf[20];
	struct tm * tmp;

	time_t time = entry->d_un.d_val;
	tmp = gmtime (&time);
	sprintf (timebuf, "%04u-%02u-%02uT%02u:%02u:%02u",
		 tmp->tm_year + 1900, tmp->tm_mon + 1, tmp->tm_mday,
		 tmp->tm_hour, tmp->tm_min, tmp->tm_sec);
	printf ("Time Stamp: %s\n", timebuf);
      }
      break;

    case DT_MIPS_RLD_VERSION:
    case DT_MIPS_LOCAL_GOTNO:
    case DT_MIPS_CONFLICTNO:
    case DT_MIPS_LIBLISTNO:
    case DT_MIPS_SYMTABNO:
    case DT_MIPS_UNREFEXTNO:
    case DT_MIPS_HIPAGENO:
    case DT_MIPS_DELTA_CLASS_NO:
    case DT_MIPS_DELTA_INSTANCE_NO:
    case DT_MIPS_DELTA_RELOC_NO:
    case DT_MIPS_DELTA_SYM_NO:
    case DT_MIPS_DELTA_CLASSSYM_NO:
    case DT_MIPS_COMPACT_SIZE:
      printf ("%ld\n", (long) entry->d_un.d_ptr);
      break;

    default:
      printf ("%#lx\n", (long) entry->d_un.d_ptr);
    }
}


static void
dynamic_segment_parisc_val (entry)
     Elf_Internal_Dyn * entry;
{
  switch (entry->d_tag)
    {
    case DT_HP_DLD_FLAGS:
      {
	static struct
	{
	  long int bit;
	  const char * str;
	}
	flags[] =
	{
	  { DT_HP_DEBUG_PRIVATE, "HP_DEBUG_PRIVATE" },
	  { DT_HP_DEBUG_CALLBACK, "HP_DEBUG_CALLBACK" },
	  { DT_HP_DEBUG_CALLBACK_BOR, "HP_DEBUG_CALLBACK_BOR" },
	  { DT_HP_NO_ENVVAR, "HP_NO_ENVVAR" },
	  { DT_HP_BIND_NOW, "HP_BIND_NOW" },
	  { DT_HP_BIND_NONFATAL, "HP_BIND_NONFATAL" },
	  { DT_HP_BIND_VERBOSE, "HP_BIND_VERBOSE" },
	  { DT_HP_BIND_RESTRICTED, "HP_BIND_RESTRICTED" },
	  { DT_HP_BIND_SYMBOLIC, "HP_BIND_SYMBOLIC" },
	  { DT_HP_RPATH_FIRST, "HP_RPATH_FIRST" },
	  { DT_HP_BIND_DEPTH_FIRST, "HP_BIND_DEPTH_FIRST" }
	};
	int first = 1;
	size_t cnt;
	bfd_vma val = entry->d_un.d_val;

	for (cnt = 0; cnt < sizeof (flags) / sizeof (flags[0]); ++cnt)
	  if (val & flags[cnt].bit)
	    {
	      if (! first)
		putchar (' ');
	      fputs (flags[cnt].str, stdout);
	      first = 0;
	      val ^= flags[cnt].bit;
	    }
	
	if (val != 0 || first)
	  {
	    if (! first)
	      putchar (' ');
	    print_vma (val, HEX);
	  }
      }
      break;
      
    default:
      print_vma (entry->d_un.d_ptr, PREFIX_HEX);
      break;
    }
}

static int
get_32bit_dynamic_segment (file)
     FILE * file;
{
  Elf32_External_Dyn * edyn;
  Elf_Internal_Dyn *   entry;
  bfd_size_type        i;

  GET_DATA_ALLOC (dynamic_addr, dynamic_size,
		  edyn, Elf32_External_Dyn *, "dynamic segment");

  /* SGI's ELF has more than one section in the DYNAMIC segment.  Determine
     how large this .dynamic is now.  We can do this even before the byte
     swapping since the DT_NULL tag is recognizable.  */
  dynamic_size = 0;
  while (*(Elf32_Word *) edyn [dynamic_size++].d_tag != DT_NULL)
    ;

  dynamic_segment = (Elf_Internal_Dyn *)
    malloc (dynamic_size * sizeof (Elf_Internal_Dyn));

  if (dynamic_segment == NULL)
    {
      error (_("Out of memory\n"));
      free (edyn);
      return 0;
    }

  for (i = 0, entry = dynamic_segment;
       i < dynamic_size;
       i ++, entry ++)
    {
      entry->d_tag      = BYTE_GET (edyn [i].d_tag);
      entry->d_un.d_val = BYTE_GET (edyn [i].d_un.d_val);
    }

  free (edyn);

  return 1;
}

static int
get_64bit_dynamic_segment (file)
     FILE * file;
{
  Elf64_External_Dyn * edyn;
  Elf_Internal_Dyn *   entry;
  bfd_size_type        i;

  GET_DATA_ALLOC (dynamic_addr, dynamic_size,
		  edyn, Elf64_External_Dyn *, "dynamic segment");

  /* SGI's ELF has more than one section in the DYNAMIC segment.  Determine
     how large this .dynamic is now.  We can do this even before the byte
     swapping since the DT_NULL tag is recognizable.  */
  dynamic_size = 0;
  while (*(bfd_vma *) edyn [dynamic_size ++].d_tag != DT_NULL)
    ;

  dynamic_segment = (Elf_Internal_Dyn *)
    malloc (dynamic_size * sizeof (Elf_Internal_Dyn));

  if (dynamic_segment == NULL)
    {
      error (_("Out of memory\n"));
      free (edyn);
      return 0;
    }

  for (i = 0, entry = dynamic_segment;
       i < dynamic_size;
       i ++, entry ++)
    {
      entry->d_tag      = BYTE_GET8 (edyn [i].d_tag);
      entry->d_un.d_val = BYTE_GET8 (edyn [i].d_un.d_val);
    }

  free (edyn);

  return 1;
}

static const char *
get_dynamic_flags (flags)
     bfd_vma flags;
{
  static char buff [64];
  while (flags)
    {
      bfd_vma flag;

      flag = flags & - flags;
      flags &= ~ flag;

      switch (flag)
	{
	case DF_ORIGIN:   strcat (buff, "ORIGIN "); break;
	case DF_SYMBOLIC: strcat (buff, "SYMBOLIC "); break;
	case DF_TEXTREL:  strcat (buff, "TEXTREL "); break;
	case DF_BIND_NOW: strcat (buff, "BIND_NOW "); break;
	default:          strcat (buff, "unknown "); break;
	}
    }
  return buff;
}

/* Parse and display the contents of the dynamic segment.  */
static int
process_dynamic_segment (file)
     FILE * file;
{
  Elf_Internal_Dyn * entry;
  bfd_size_type      i;

  if (dynamic_size == 0)
    {
      if (do_dynamic)
	printf (_("\nThere is no dynamic segment in this file.\n"));

      return 1;
    }

  if (is_32bit_elf)
    {
      if (! get_32bit_dynamic_segment (file))
	return 0;
    }
  else if (! get_64bit_dynamic_segment (file))
    return 0;

  /* Find the appropriate symbol table.  */
  if (dynamic_symbols == NULL)
    {
      for (i = 0, entry = dynamic_segment;
	   i < dynamic_size;
	   ++i, ++ entry)
	{
	  unsigned long        offset;

	  if (entry->d_tag != DT_SYMTAB)
	    continue;

	  dynamic_info[DT_SYMTAB] = entry->d_un.d_val;

	  /* Since we do not know how big the symbol table is,
	     we default to reading in the entire file (!) and
	     processing that.  This is overkill, I know, but it
	     should work. */
	  offset = entry->d_un.d_val - loadaddr;

	  if (fseek (file, 0, SEEK_END))
	    error (_("Unable to seek to end of file!"));

	  if (is_32bit_elf)
	    num_dynamic_syms = (ftell (file) - offset) / sizeof (Elf32_External_Sym);
	  else
	    num_dynamic_syms = (ftell (file) - offset) / sizeof (Elf64_External_Sym);

	  if (num_dynamic_syms < 1)
	    {
	      error (_("Unable to determine the number of symbols to load\n"));
	      continue;
	    }

	  dynamic_symbols = GET_ELF_SYMBOLS (file, offset, num_dynamic_syms);
	}
    }

  /* Similarly find a string table.  */
  if (dynamic_strings == NULL)
    {
      for (i = 0, entry = dynamic_segment;
	   i < dynamic_size;
	   ++i, ++ entry)
	{
	  unsigned long offset;
	  long          str_tab_len;

	  if (entry->d_tag != DT_STRTAB)
	    continue;

	  dynamic_info[DT_STRTAB] = entry->d_un.d_val;

	  /* Since we do not know how big the string table is,
	     we default to reading in the entire file (!) and
	     processing that.  This is overkill, I know, but it
	     should work. */

	  offset = entry->d_un.d_val - loadaddr;
	  if (fseek (file, 0, SEEK_END))
	    error (_("Unable to seek to end of file\n"));
	  str_tab_len = ftell (file) - offset;

	  if (str_tab_len < 1)
	    {
	      error
		(_("Unable to determine the length of the dynamic string table\n"));
	      continue;
	    }

	  GET_DATA_ALLOC (offset, str_tab_len, dynamic_strings, char *,
			  "dynamic string table");

	  break;
	}
    }

  /* And find the syminfo section if available.  */
  if (dynamic_syminfo == NULL)
    {
      unsigned int syminsz = 0;

      for (i = 0, entry = dynamic_segment;
	   i < dynamic_size;
	   ++i, ++ entry)
	{
	  if (entry->d_tag == DT_SYMINENT)
	    {
	      /* Note: these braces are necessary to avoid a syntax
		 error from the SunOS4 C compiler.  */
	      assert (sizeof (Elf_External_Syminfo) == entry->d_un.d_val);
	    }
	  else if (entry->d_tag == DT_SYMINSZ)
	    syminsz = entry->d_un.d_val;
	  else if (entry->d_tag == DT_SYMINFO)
	    dynamic_syminfo_offset = entry->d_un.d_val - loadaddr;
	}

      if (dynamic_syminfo_offset != 0 && syminsz != 0)
	{
	  Elf_External_Syminfo * extsyminfo;
	  Elf_Internal_Syminfo * syminfo;

	  /* There is a syminfo section.  Read the data.  */
	  GET_DATA_ALLOC (dynamic_syminfo_offset, syminsz, extsyminfo,
			  Elf_External_Syminfo *, "symbol information");

	  dynamic_syminfo = (Elf_Internal_Syminfo *) malloc (syminsz);
	  if (dynamic_syminfo == NULL)
	    {
	      error (_("Out of memory\n"));
	      return 0;
	    }

	  dynamic_syminfo_nent = syminsz / sizeof (Elf_External_Syminfo);
	  for (i = 0, syminfo = dynamic_syminfo; i < dynamic_syminfo_nent;
	       ++i, ++syminfo)
	    {
	      syminfo->si_boundto = BYTE_GET (extsyminfo[i].si_boundto);
	      syminfo->si_flags = BYTE_GET (extsyminfo[i].si_flags);
	    }

	  free (extsyminfo);
	}
    }

  if (do_dynamic && dynamic_addr)
    printf (_("\nDynamic segment at offset 0x%x contains %ld entries:\n"),
	    dynamic_addr, (long) dynamic_size);
  if (do_dynamic)
    printf (_("  Tag        Type                         Name/Value\n"));

  for (i = 0, entry = dynamic_segment;
       i < dynamic_size;
       i++, entry ++)
    {
      if (do_dynamic)
	{
	  const char *dtype;

	  putchar (' ');
	  print_vma (entry->d_tag, FULL_HEX);
	  dtype = get_dynamic_type (entry->d_tag);
	  printf (" (%s)%*s", dtype,
		  ((is_32bit_elf ? 27 : 19)
		   - (int) strlen (dtype)),
		  " ");
	}

      switch (entry->d_tag)
	{
	case DT_FLAGS:
	  if (do_dynamic)
	    printf ("%s", get_dynamic_flags (entry->d_un.d_val));
	  break;
	  
	case DT_AUXILIARY:
	case DT_FILTER:
	  if (do_dynamic)
	    {
	      if (entry->d_tag == DT_AUXILIARY)
		printf (_("Auxiliary library"));
	      else
		printf (_("Filter library"));

	      if (dynamic_strings)
		printf (": [%s]\n", dynamic_strings + entry->d_un.d_val);
	      else
		{
		  printf (": ");
		  print_vma (entry->d_un.d_val, PREFIX_HEX);
		  putchar ('\n');
		}
	    }
	  break;

	case DT_FEATURE_1:
	  if (do_dynamic)
	    {
	      printf (_("Flags:"));
	      if (entry->d_un.d_val == 0)
		printf (_(" None\n"));
	      else
		{
		  unsigned long int val = entry->d_un.d_val;
		  if (val & DTF_1_PARINIT)
		    {
		      printf (" PARINIT");
		      val ^= DTF_1_PARINIT;
		    }
		  if (val != 0)
		    printf (" %lx", val);
		  puts ("");
		}
	    }
	  break;

	case DT_POSFLAG_1:
	  if (do_dynamic)
	    {
	      printf (_("Flags:"));
	      if (entry->d_un.d_val == 0)
		printf (_(" None\n"));
	      else
		{
		  unsigned long int val = entry->d_un.d_val;
		  if (val & DF_P1_LAZYLOAD)
		    {
		      printf (" LAZYLOAD");
		      val ^= DF_P1_LAZYLOAD;
		    }
		  if (val & DF_P1_GROUPPERM)
		    {
		      printf (" GROUPPERM");
		      val ^= DF_P1_GROUPPERM;
		    }
		  if (val != 0)
		    printf (" %lx", val);
		  puts ("");
		}
	    }
	  break;

	case DT_FLAGS_1:
	  if (do_dynamic)
	    {
	      printf (_("Flags:"));
	      if (entry->d_un.d_val == 0)
		printf (_(" None\n"));
	      else
		{
		  unsigned long int val = entry->d_un.d_val;
		  if (val & DF_1_NOW)
		    {
		      printf (" NOW");
		      val ^= DF_1_NOW;
		    }
		  if (val & DF_1_GLOBAL)
		    {
		      printf (" GLOBAL");
		      val ^= DF_1_GLOBAL;
		    }
		  if (val & DF_1_GROUP)
		    {
		      printf (" GROUP");
		      val ^= DF_1_GROUP;
		    }
		  if (val & DF_1_NODELETE)
		    {
		      printf (" NODELETE");
		      val ^= DF_1_NODELETE;
		    }
		  if (val & DF_1_LOADFLTR)
		    {
		      printf (" LOADFLTR");
		      val ^= DF_1_LOADFLTR;
		    }
		  if (val & DF_1_INITFIRST)
		    {
		      printf (" INITFIRST");
		      val ^= DF_1_INITFIRST;
		    }
		  if (val & DF_1_NOOPEN)
		    {
		      printf (" NOOPEN");
		      val ^= DF_1_NOOPEN;
		    }
		  if (val & DF_1_ORIGIN)
		    {
		      printf (" ORIGIN");
		      val ^= DF_1_ORIGIN;
		    }
		  if (val & DF_1_DIRECT)
		    {
		      printf (" DIRECT");
		      val ^= DF_1_DIRECT;
		    }
		  if (val & DF_1_TRANS)
		    {
		      printf (" TRANS");
		      val ^= DF_1_TRANS;
		    }
		  if (val & DF_1_INTERPOSE)
		    {
		      printf (" INTERPOSE");
		      val ^= DF_1_INTERPOSE;
		    }
		  if (val != 0)
		    printf (" %lx", val);
		  puts ("");
		}
	    }
	  break;

	case DT_PLTREL:
	  if (do_dynamic)
	    puts (get_dynamic_type (entry->d_un.d_val));
	  break;

	case DT_NULL	:
	case DT_NEEDED	:
	case DT_PLTGOT	:
	case DT_HASH	:
	case DT_STRTAB	:
	case DT_SYMTAB	:
	case DT_RELA	:
	case DT_INIT	:
	case DT_FINI	:
	case DT_SONAME	:
	case DT_RPATH	:
	case DT_SYMBOLIC:
	case DT_REL	:
	case DT_DEBUG	:
	case DT_TEXTREL	:
	case DT_JMPREL	:
	  dynamic_info[entry->d_tag] = entry->d_un.d_val;

	  if (do_dynamic)
	    {
	      char * name;

	      if (dynamic_strings == NULL)
		name = NULL;
	      else
		name = dynamic_strings + entry->d_un.d_val;

	      if (name)
		{
		  switch (entry->d_tag)
		    {
		    case DT_NEEDED:
		      printf (_("Shared library: [%s]"), name);

		      if (strcmp (name, program_interpreter) == 0)
			printf (_(" program interpreter"));
		      break;

		    case DT_SONAME:
		      printf (_("Library soname: [%s]"), name);
		      break;

		    case DT_RPATH:
		      printf (_("Library rpath: [%s]"), name);
		      break;

		    default:
		      print_vma (entry->d_un.d_val, PREFIX_HEX);
		      break;
		    }
		}
	      else
		print_vma (entry->d_un.d_val, PREFIX_HEX);

	      putchar ('\n');
	    }
	  break;

	case DT_PLTRELSZ:
	case DT_RELASZ	:
	case DT_STRSZ	:
	case DT_RELSZ	:
	case DT_RELAENT	:
	case DT_SYMENT	:
	case DT_RELENT	:
	case DT_PLTPADSZ:
	case DT_MOVEENT	:
	case DT_MOVESZ	:
	case DT_INIT_ARRAYSZ:
	case DT_FINI_ARRAYSZ:
	  if (do_dynamic)
	    {
	      print_vma (entry->d_un.d_val, UNSIGNED);
	      printf (" (bytes)\n");
	    }
	  break;

	case DT_VERDEFNUM:
	case DT_VERNEEDNUM:
	case DT_RELACOUNT:
	case DT_RELCOUNT:
	  if (do_dynamic)
	    {
	      print_vma (entry->d_un.d_val, UNSIGNED);
	      putchar ('\n');
	    }
	  break;

	case DT_SYMINSZ:
	case DT_SYMINENT:
	case DT_SYMINFO:
	case DT_USED:
	case DT_INIT_ARRAY:
	case DT_FINI_ARRAY:
	  if (do_dynamic)
	    {
	      if (dynamic_strings != NULL && entry->d_tag == DT_USED)
		{
		  char * name;

		  name = dynamic_strings + entry->d_un.d_val;

		  if (* name)
		    {
		      printf (_("Not needed object: [%s]\n"), name);
		      break;
		    }
		}

	      print_vma (entry->d_un.d_val, PREFIX_HEX);
	      putchar ('\n');
	    }
	  break;

	case DT_BIND_NOW:
	  /* The value of this entry is ignored.  */
	  break;

	default:
	  if ((entry->d_tag >= DT_VERSYM) && (entry->d_tag <= DT_VERNEEDNUM))
	    version_info [DT_VERSIONTAGIDX (entry->d_tag)] =
	      entry->d_un.d_val;

	  if (do_dynamic)
	    {
	      switch (elf_header.e_machine)
		{
		case EM_MIPS:
		case EM_MIPS_RS4_BE:
		  dynamic_segment_mips_val (entry);
		  break;
		case EM_PARISC:
		  dynamic_segment_parisc_val (entry);
		  break;
		default:
		  print_vma (entry->d_un.d_val, PREFIX_HEX);
		  putchar ('\n');
		}
	    }
	  break;
	}
    }

  return 1;
}

static char *
get_ver_flags (flags)
     unsigned int flags;
{
  static char buff [32];

  buff[0] = 0;

  if (flags == 0)
    return _("none");

  if (flags & VER_FLG_BASE)
    strcat (buff, "BASE ");

  if (flags & VER_FLG_WEAK)
    {
      if (flags & VER_FLG_BASE)
	strcat (buff, "| ");

      strcat (buff, "WEAK ");
    }

  if (flags & ~(VER_FLG_BASE | VER_FLG_WEAK))
    strcat (buff, "| <unknown>");

  return buff;
}

/* Display the contents of the version sections.  */
static int
process_version_sections (file)
     FILE * file;
{
  Elf32_Internal_Shdr * section;
  unsigned   i;
  int        found = 0;

  if (! do_version)
    return 1;

  for (i = 0, section = section_headers;
       i < elf_header.e_shnum;
       i++, section ++)
    {
      switch (section->sh_type)
	{
	case SHT_GNU_verdef:
	  {
	    Elf_External_Verdef * edefs;
	    unsigned int          idx;
	    unsigned int          cnt;

	    found = 1;

	    printf
	      (_("\nVersion definition section '%s' contains %ld entries:\n"),
	       SECTION_NAME (section), section->sh_info);

	    printf (_("  Addr: 0x"));
	    printf_vma (section->sh_addr);
	    printf (_("  Offset: %#08lx  Link: %lx (%s)\n"),
		    (unsigned long) section->sh_offset, section->sh_link,
		    SECTION_NAME (section_headers + section->sh_link));

	    GET_DATA_ALLOC (section->sh_offset, section->sh_size,
			    edefs, Elf_External_Verdef *,
			    "version definition section");

	    for (idx = cnt = 0; cnt < section->sh_info; ++ cnt)
	      {
		char *                 vstart;
		Elf_External_Verdef *  edef;
		Elf_Internal_Verdef    ent;
		Elf_External_Verdaux * eaux;
		Elf_Internal_Verdaux   aux;
		int                    j;
		int                    isum;

		vstart = ((char *) edefs) + idx;

		edef = (Elf_External_Verdef *) vstart;

		ent.vd_version = BYTE_GET (edef->vd_version);
		ent.vd_flags   = BYTE_GET (edef->vd_flags);
		ent.vd_ndx     = BYTE_GET (edef->vd_ndx);
		ent.vd_cnt     = BYTE_GET (edef->vd_cnt);
		ent.vd_hash    = BYTE_GET (edef->vd_hash);
		ent.vd_aux     = BYTE_GET (edef->vd_aux);
		ent.vd_next    = BYTE_GET (edef->vd_next);

		printf (_("  %#06x: Rev: %d  Flags: %s"),
			idx, ent.vd_version, get_ver_flags (ent.vd_flags));

		printf (_("  Index: %d  Cnt: %d  "),
			ent.vd_ndx, ent.vd_cnt);

		vstart += ent.vd_aux;

		eaux = (Elf_External_Verdaux *) vstart;

		aux.vda_name = BYTE_GET (eaux->vda_name);
		aux.vda_next = BYTE_GET (eaux->vda_next);

		if (dynamic_strings)
		  printf (_("Name: %s\n"), dynamic_strings + aux.vda_name);
		else
		  printf (_("Name index: %ld\n"), aux.vda_name);

		isum = idx + ent.vd_aux;

		for (j = 1; j < ent.vd_cnt; j ++)
		  {
		    isum   += aux.vda_next;
		    vstart += aux.vda_next;

		    eaux = (Elf_External_Verdaux *) vstart;

		    aux.vda_name = BYTE_GET (eaux->vda_name);
		    aux.vda_next = BYTE_GET (eaux->vda_next);

		    if (dynamic_strings)
		      printf (_("  %#06x: Parent %d: %s\n"),
			      isum, j, dynamic_strings + aux.vda_name);
		    else
		      printf (_("  %#06x: Parent %d, name index: %ld\n"),
			      isum, j, aux.vda_name);
		  }

		idx += ent.vd_next;
	      }

	    free (edefs);
	  }
	  break;

	case SHT_GNU_verneed:
	  {
	    Elf_External_Verneed *  eneed;
	    unsigned int            idx;
	    unsigned int            cnt;

	    found = 1;

	    printf (_("\nVersion needs section '%s' contains %ld entries:\n"),
		    SECTION_NAME (section), section->sh_info);

	    printf (_(" Addr: 0x"));
	    printf_vma (section->sh_addr);
	    printf (_("  Offset: %#08lx  Link to section: %ld (%s)\n"),
		    (unsigned long) section->sh_offset, section->sh_link,
		    SECTION_NAME (section_headers + section->sh_link));

	    GET_DATA_ALLOC (section->sh_offset, section->sh_size,
			    eneed, Elf_External_Verneed *,
			    "version need section");

	    for (idx = cnt = 0; cnt < section->sh_info; ++cnt)
	      {
		Elf_External_Verneed * entry;
		Elf_Internal_Verneed     ent;
		int                      j;
		int                      isum;
		char *                   vstart;

		vstart = ((char *) eneed) + idx;

		entry = (Elf_External_Verneed *) vstart;

		ent.vn_version = BYTE_GET (entry->vn_version);
		ent.vn_cnt     = BYTE_GET (entry->vn_cnt);
		ent.vn_file    = BYTE_GET (entry->vn_file);
		ent.vn_aux     = BYTE_GET (entry->vn_aux);
		ent.vn_next    = BYTE_GET (entry->vn_next);

		printf (_("  %#06x: Version: %d"), idx, ent.vn_version);

		if (dynamic_strings)
		  printf (_("  File: %s"), dynamic_strings + ent.vn_file);
		else
		  printf (_("  File: %lx"), ent.vn_file);

		printf (_("  Cnt: %d\n"), ent.vn_cnt);

		vstart += ent.vn_aux;

		for (j = 0, isum = idx + ent.vn_aux; j < ent.vn_cnt; ++j)
		  {
		    Elf_External_Vernaux * eaux;
		    Elf_Internal_Vernaux   aux;

		    eaux = (Elf_External_Vernaux *) vstart;

		    aux.vna_hash  = BYTE_GET (eaux->vna_hash);
		    aux.vna_flags = BYTE_GET (eaux->vna_flags);
		    aux.vna_other = BYTE_GET (eaux->vna_other);
		    aux.vna_name  = BYTE_GET (eaux->vna_name);
		    aux.vna_next  = BYTE_GET (eaux->vna_next);

		    if (dynamic_strings)
		      printf (_("  %#06x: Name: %s"),
			      isum, dynamic_strings + aux.vna_name);
		    else
		      printf (_("  %#06x: Name index: %lx"),
			      isum, aux.vna_name);

		    printf (_("  Flags: %s  Version: %d\n"),
			    get_ver_flags (aux.vna_flags), aux.vna_other);

		    isum   += aux.vna_next;
		    vstart += aux.vna_next;
		  }

		idx += ent.vn_next;
	      }

	    free (eneed);
	  }
	  break;

	case SHT_GNU_versym:
	  {
	    Elf32_Internal_Shdr *       link_section;
	    int              		total;
	    int              		cnt;
	    unsigned char * 		edata;
	    unsigned short * 		data;
	    char *           		strtab;
	    Elf_Internal_Sym * 		symbols;
	    Elf32_Internal_Shdr *       string_sec;

	    link_section = section_headers + section->sh_link;
	    total = section->sh_size / section->sh_entsize;

	    found = 1;

	    symbols = GET_ELF_SYMBOLS (file, link_section->sh_offset,
				       link_section->sh_size / link_section->sh_entsize);

	    string_sec = section_headers + link_section->sh_link;

	    GET_DATA_ALLOC (string_sec->sh_offset, string_sec->sh_size,
			    strtab, char *, "version string table");

	    printf (_("\nVersion symbols section '%s' contains %d entries:\n"),
		    SECTION_NAME (section), total);

	    printf (_(" Addr: "));
	    printf_vma (section->sh_addr);
	    printf (_("  Offset: %#08lx  Link: %lx (%s)\n"),
		    (unsigned long) section->sh_offset, section->sh_link,
		    SECTION_NAME (link_section));

	    GET_DATA_ALLOC (version_info [DT_VERSIONTAGIDX (DT_VERSYM)]
			    - loadaddr,
			    total * sizeof (short), edata,
			    unsigned char *, "version symbol data");

	    data = (unsigned short *) malloc (total * sizeof (short));

	    for (cnt = total; cnt --;)
	      data [cnt] = byte_get (edata + cnt * sizeof (short),
				     sizeof (short));

	    free (edata);

	    for (cnt = 0; cnt < total; cnt += 4)
	      {
		int j, nn;
		char *name;

		printf ("  %03x:", cnt);

		for (j = 0; (j < 4) && (cnt + j) < total; ++j)
		  switch (data [cnt + j])
		    {
		    case 0:
		      fputs (_("   0 (*local*)    "), stdout);
		      break;

		    case 1:
		      fputs (_("   1 (*global*)   "), stdout);
		      break;

		    default:
		      nn = printf ("%4x%c", data [cnt + j] & 0x7fff,
				   data [cnt + j] & 0x8000 ? 'h' : ' ');

		      if (symbols [cnt + j].st_shndx < SHN_LORESERVE
			  && section_headers[symbols [cnt + j].st_shndx].sh_type
			  == SHT_NOBITS)
			{
			  /* We must test both.  */
			  Elf_Internal_Verneed     ivn;
			  unsigned long            offset;

			  offset = version_info [DT_VERSIONTAGIDX (DT_VERNEED)]
			    - loadaddr;

			  do
			    {
			      Elf_External_Verneed   evn;
			      Elf_External_Vernaux   evna;
			      Elf_Internal_Vernaux   ivna;
			      unsigned long          vna_off;

			      GET_DATA (offset, evn, "version need");

			      ivn.vn_aux  = BYTE_GET (evn.vn_aux);
			      ivn.vn_next = BYTE_GET (evn.vn_next);

			      vna_off = offset + ivn.vn_aux;

			      do
				{
				  GET_DATA (vna_off, evna,
					    "version need aux (1)");

				  ivna.vna_next  = BYTE_GET (evna.vna_next);
				  ivna.vna_other = BYTE_GET (evna.vna_other);

				  vna_off += ivna.vna_next;
				}
			      while (ivna.vna_other != data [cnt + j]
				     && ivna.vna_next != 0);

			      if (ivna.vna_other == data [cnt + j])
				{
				  ivna.vna_name = BYTE_GET (evna.vna_name);

				  name = strtab + ivna.vna_name;
				  nn += printf ("(%s%-*s",
						name,
						12 - (int) strlen (name),
						")");
				  break;
				}
			      else if (ivn.vn_next == 0)
				{
				  if (data [cnt + j] != 0x8001)
				    {
				      Elf_Internal_Verdef  ivd;
				      Elf_External_Verdef  evd;

				      offset = version_info
					[DT_VERSIONTAGIDX (DT_VERDEF)]
					- loadaddr;

				      do
					{
					  GET_DATA (offset, evd,
						    "version definition");

					  ivd.vd_next = BYTE_GET (evd.vd_next);
					  ivd.vd_ndx  = BYTE_GET (evd.vd_ndx);

					  offset += ivd.vd_next;
					}
				      while (ivd.vd_ndx
					     != (data [cnt + j] & 0x7fff)
					     && ivd.vd_next != 0);

				      if (ivd.vd_ndx
					  == (data [cnt + j] & 0x7fff))
					{
					  Elf_External_Verdaux  evda;
					  Elf_Internal_Verdaux  ivda;

					  ivd.vd_aux = BYTE_GET (evd.vd_aux);

					  GET_DATA (offset + ivd.vd_aux, evda,
						    "version definition aux");

					  ivda.vda_name =
					    BYTE_GET (evda.vda_name);

					  name = strtab + ivda.vda_name;
					  nn +=
					    printf ("(%s%-*s",
						    name,
						    12 - (int) strlen (name),
						    ")");
					}
				    }

				  break;
				}
			      else
				offset += ivn.vn_next;
			    }
			  while (ivn.vn_next);
			}
		      else if (symbols [cnt + j].st_shndx == SHN_UNDEF)
			{
			  Elf_Internal_Verneed     ivn;
			  unsigned long            offset;

			  offset = version_info [DT_VERSIONTAGIDX (DT_VERNEED)]
			    - loadaddr;

		          do
			    {
			      Elf_Internal_Vernaux   ivna;
			      Elf_External_Verneed   evn;
			      Elf_External_Vernaux   evna;
			      unsigned long          a_off;

			      GET_DATA (offset, evn, "version need");

			      ivn.vn_aux  = BYTE_GET (evn.vn_aux);
			      ivn.vn_next = BYTE_GET (evn.vn_next);

			      a_off = offset + ivn.vn_aux;

			      do
				{
				  GET_DATA (a_off, evna,
					    "version need aux (2)");

				  ivna.vna_next  = BYTE_GET (evna.vna_next);
				  ivna.vna_other = BYTE_GET (evna.vna_other);

				  a_off += ivna.vna_next;
				}
			      while (ivna.vna_other != data [cnt + j]
				     && ivna.vna_next != 0);

			      if (ivna.vna_other == data [cnt + j])
				{
				  ivna.vna_name = BYTE_GET (evna.vna_name);

				  name = strtab + ivna.vna_name;
				  nn += printf ("(%s%-*s",
						name,
						12 - (int) strlen (name),
						")");
				  break;
				}

			      offset += ivn.vn_next;
			    }
			  while (ivn.vn_next);
			}
		      else if (data [cnt + j] != 0x8001)
			{
			  Elf_Internal_Verdef  ivd;
			  Elf_External_Verdef  evd;
			  unsigned long        offset;

			  offset = version_info
			    [DT_VERSIONTAGIDX (DT_VERDEF)] - loadaddr;

			  do
			    {
			      GET_DATA (offset, evd, "version def");

			      ivd.vd_next = BYTE_GET (evd.vd_next);
			      ivd.vd_ndx  = BYTE_GET (evd.vd_ndx);

			      offset += ivd.vd_next;
			    }
			  while (ivd.vd_ndx != (data [cnt + j] & 0x7fff)
				 && ivd.vd_next != 0);

			  if (ivd.vd_ndx == (data [cnt + j] & 0x7fff))
			    {
			      Elf_External_Verdaux  evda;
			      Elf_Internal_Verdaux  ivda;

			      ivd.vd_aux = BYTE_GET (evd.vd_aux);

			      GET_DATA (offset - ivd.vd_next + ivd.vd_aux,
					evda, "version def aux");

			      ivda.vda_name = BYTE_GET (evda.vda_name);

			      name = strtab + ivda.vda_name;
			      nn += printf ("(%s%-*s",
					    name,
					    12 - (int) strlen (name),
					    ")");
			    }
			}

		      if (nn < 18)
			printf ("%*c", 18 - nn, ' ');
		    }

		putchar ('\n');
	      }

	    free (data);
	    free (strtab);
	    free (symbols);
	  }
	  break;

	default:
	  break;
	}
    }

  if (! found)
    printf (_("\nNo version information found in this file.\n"));

  return 1;
}

static const char *
get_symbol_binding (binding)
     unsigned int binding;
{
  static char buff [32];

  switch (binding)
    {
    case STB_LOCAL:  return "LOCAL";
    case STB_GLOBAL: return "GLOBAL";
    case STB_WEAK:   return "WEAK";
    default:
      if (binding >= STB_LOPROC && binding <= STB_HIPROC)
	sprintf (buff, _("<processor specific>: %d"), binding);
      else if (binding >= STB_LOOS && binding <= STB_HIOS)
	sprintf (buff, _("<OS specific>: %d"), binding);
      else
	sprintf (buff, _("<unknown>: %d"), binding);
      return buff;
    }
}

static const char *
get_symbol_type (type)
     unsigned int type;
{
  static char buff [32];

  switch (type)
    {
    case STT_NOTYPE:   return "NOTYPE";
    case STT_OBJECT:   return "OBJECT";
    case STT_FUNC:     return "FUNC";
    case STT_SECTION:  return "SECTION";
    case STT_FILE:     return "FILE";
    case STT_COMMON:   return "COMMON";
    default:
      if (type >= STT_LOPROC && type <= STT_HIPROC)
	{
	  if (elf_header.e_machine == EM_ARM && type == STT_ARM_TFUNC)
	    return "THUMB_FUNC";

	  if (elf_header.e_machine == EM_SPARCV9 && type == STT_REGISTER)
	    return "REGISTER";

	  if (elf_header.e_machine == EM_PARISC && type == STT_PARISC_MILLI)
	    return "PARISC_MILLI";

	  sprintf (buff, _("<processor specific>: %d"), type);
	}
      else if (type >= STT_LOOS && type <= STT_HIOS)
	{
	  if (elf_header.e_machine == EM_PARISC)
	    {
	      if (type == STT_HP_OPAQUE)
		return "HP_OPAQUE";
	      if (type == STT_HP_STUB)
		return "HP_STUB";
	    }

	  sprintf (buff, _("<OS specific>: %d"), type);
	}
      else
	sprintf (buff, _("<unknown>: %d"), type);
      return buff;
    }
}

static const char *
get_symbol_visibility (visibility)
     unsigned int visibility;
{
  switch (visibility)
    {
    case STV_DEFAULT:   return "DEFAULT";
    case STV_INTERNAL:  return "INTERNAL";
    case STV_HIDDEN:    return "HIDDEN";
    case STV_PROTECTED: return "PROTECTED";
    default: abort ();
    }
}

static const char *
get_symbol_index_type (type)
     unsigned int type;
{
  switch (type)
    {
    case SHN_UNDEF:  return "UND";
    case SHN_ABS:    return "ABS";
    case SHN_COMMON: return "COM";
    default:
      if (type >= SHN_LOPROC && type <= SHN_HIPROC)
	return "PRC";
      else if (type >= SHN_LORESERVE && type <= SHN_HIRESERVE)
	return "RSV";
      else if (type >= SHN_LOOS && type <= SHN_HIOS)
	return "OS ";
      else
	{
	  static char buff [32];

	  sprintf (buff, "%3d", type);
	  return buff;
	}
    }
}

static int *
get_dynamic_data (file, number)
     FILE *       file;
     unsigned int number;
{
  char * e_data;
  int *  i_data;

  e_data = (char *) malloc (number * 4);

  if (e_data == NULL)
    {
      error (_("Out of memory\n"));
      return NULL;
    }

  if (fread (e_data, 4, number, file) != number)
    {
      error (_("Unable to read in dynamic data\n"));
      return NULL;
    }

  i_data = (int *) malloc (number * sizeof (* i_data));

  if (i_data == NULL)
    {
      error (_("Out of memory\n"));
      free (e_data);
      return NULL;
    }

  while (number--)
    i_data [number] = byte_get (e_data + number * 4, 4);

  free (e_data);

  return i_data;
}

/* Dump the symbol table */
static int
process_symbol_table (file)
     FILE * file;
{
  Elf32_Internal_Shdr *   section;
  char   nb [4];
  char   nc [4];
  int    nbuckets = 0;
  int    nchains = 0;
  int *  buckets = NULL;
  int *  chains = NULL;

  if (! do_syms && !do_histogram)
    return 1;

  if (dynamic_info[DT_HASH] && ((do_using_dynamic && dynamic_strings != NULL)
				|| do_histogram))
    {
      if (fseek (file, dynamic_info[DT_HASH] - loadaddr, SEEK_SET))
	{
	  error (_("Unable to seek to start of dynamic information"));
	  return 0;
	}

      if (fread (nb, sizeof (nb), 1, file) != 1)
	{
	  error (_("Failed to read in number of buckets\n"));
	  return 0;
	}

      if (fread (nc, sizeof (nc), 1, file) != 1)
	{
	  error (_("Failed to read in number of chains\n"));
	  return 0;
	}

      nbuckets = byte_get (nb, 4);
      nchains  = byte_get (nc, 4);

      buckets = get_dynamic_data (file, nbuckets);
      chains  = get_dynamic_data (file, nchains);

      if (buckets == NULL || chains == NULL)
	return 0;
    }

  if (do_syms
      && dynamic_info[DT_HASH] && do_using_dynamic && dynamic_strings != NULL)
    {
      int    hn;
      int    si;

      printf (_("\nSymbol table for image:\n"));
      if (is_32bit_elf)
	printf (_("  Num Buc:    Value  Size   Type   Bind Vis      Ndx Name\n"));
      else
	printf (_("  Num Buc:    Value          Size   Type   Bind Vis      Ndx Name\n"));

      for (hn = 0; hn < nbuckets; hn++)
	{
	  if (! buckets [hn])
	    continue;

	  for (si = buckets [hn]; si < nchains && si > 0; si = chains [si])
	    {
	      Elf_Internal_Sym * psym;

	      psym = dynamic_symbols + si;

	      printf ("  %3d %3d: ", si, hn);
	      print_vma (psym->st_value, LONG_HEX);
	      putchar (' ' );
	      print_vma (psym->st_size, DEC_5);
		      
	      printf ("  %6s", get_symbol_type (ELF_ST_TYPE (psym->st_info)));
	      printf (" %6s",  get_symbol_binding (ELF_ST_BIND (psym->st_info)));
	      printf (" %3s",  get_symbol_visibility (ELF_ST_VISIBILITY (psym->st_other)));
	      printf (" %3.3s", get_symbol_index_type (psym->st_shndx));
	      printf (" %s\n", dynamic_strings + psym->st_name);
	    }
	}
    }
  else if (do_syms && !do_using_dynamic)
    {
      unsigned int     i;

      for (i = 0, section = section_headers;
	   i < elf_header.e_shnum;
	   i++, section++)
	{
	  unsigned int          si;
	  char *                strtab;
	  Elf_Internal_Sym *    symtab;
	  Elf_Internal_Sym *    psym;


	  if (   section->sh_type != SHT_SYMTAB
	      && section->sh_type != SHT_DYNSYM)
	    continue;

	  printf (_("\nSymbol table '%s' contains %lu entries:\n"),
		  SECTION_NAME (section),
		  (unsigned long) (section->sh_size / section->sh_entsize));
	  if (is_32bit_elf)
	    printf (_("   Num:    Value  Size Type    Bind   Vis      Ndx Name\n"));
	  else
	    printf (_("   Num:    Value          Size Type    Bind   Vis      Ndx Name\n"));

	  symtab = GET_ELF_SYMBOLS (file, section->sh_offset,
				    section->sh_size / section->sh_entsize);
	  if (symtab == NULL)
	    continue;

	  if (section->sh_link == elf_header.e_shstrndx)
	    strtab = string_table;
	  else
	    {
	      Elf32_Internal_Shdr * string_sec;

	      string_sec = section_headers + section->sh_link;

	      GET_DATA_ALLOC (string_sec->sh_offset, string_sec->sh_size,
			      strtab, char *, "string table");
	    }

	  for (si = 0, psym = symtab;
	       si < section->sh_size / section->sh_entsize;
	       si ++, psym ++)
	    {
	      printf ("%6d: ", si);
	      print_vma (psym->st_value, LONG_HEX);
	      putchar (' ');
	      print_vma (psym->st_size, DEC_5);
	      printf (" %-7s", get_symbol_type (ELF_ST_TYPE (psym->st_info)));
	      printf (" %-6s", get_symbol_binding (ELF_ST_BIND (psym->st_info)));
	      printf (" %-3s", get_symbol_visibility (ELF_ST_VISIBILITY (psym->st_other)));
	      printf (" %4s", get_symbol_index_type (psym->st_shndx));
	      printf (" %s", strtab + psym->st_name);

	      if (section->sh_type == SHT_DYNSYM &&
		  version_info [DT_VERSIONTAGIDX (DT_VERSYM)] != 0)
		{
		  unsigned char   data[2];
		  unsigned short  vers_data;
		  unsigned long   offset;
		  int             is_nobits;
		  int             check_def;

		  offset = version_info [DT_VERSIONTAGIDX (DT_VERSYM)]
		    - loadaddr;

		  GET_DATA (offset + si * sizeof (vers_data), data,
			    "version data");

		  vers_data = byte_get (data, 2);

		  is_nobits = psym->st_shndx < SHN_LORESERVE ?
		    (section_headers [psym->st_shndx].sh_type == SHT_NOBITS)
		    : 0;

		  check_def = (psym->st_shndx != SHN_UNDEF);

		  if ((vers_data & 0x8000) || vers_data > 1)
		    {
		      if (is_nobits || ! check_def)
			{
			  Elf_External_Verneed  evn;
			  Elf_Internal_Verneed  ivn;
			  Elf_Internal_Vernaux  ivna;

			  /* We must test both.  */
			  offset = version_info
			    [DT_VERSIONTAGIDX (DT_VERNEED)] - loadaddr;

			  do
			    {
			      unsigned long  vna_off;

			      GET_DATA (offset, evn, "version need");

			      ivn.vn_aux  = BYTE_GET (evn.vn_aux);
			      ivn.vn_next = BYTE_GET (evn.vn_next);

			      vna_off = offset + ivn.vn_aux;

			      do
				{
				  Elf_External_Vernaux  evna;

				  GET_DATA (vna_off, evna,
					    "version need aux (3)");

				  ivna.vna_other = BYTE_GET (evna.vna_other);
				  ivna.vna_next  = BYTE_GET (evna.vna_next);
				  ivna.vna_name  = BYTE_GET (evna.vna_name);

				  vna_off += ivna.vna_next;
				}
			      while (ivna.vna_other != vers_data
				     && ivna.vna_next != 0);

			      if (ivna.vna_other == vers_data)
				break;

			      offset += ivn.vn_next;
			    }
			  while (ivn.vn_next != 0);

			  if (ivna.vna_other == vers_data)
			    {
			      printf ("@%s (%d)",
				      strtab + ivna.vna_name, ivna.vna_other);
			      check_def = 0;
			    }
			  else if (! is_nobits)
			    error (_("bad dynamic symbol"));
			  else
			    check_def = 1;
			}

		      if (check_def)
			{
			  if (vers_data != 0x8001)
			    {
			      Elf_Internal_Verdef     ivd;
			      Elf_Internal_Verdaux    ivda;
			      Elf_External_Verdaux  evda;
			      unsigned long           offset;

			      offset =
				version_info [DT_VERSIONTAGIDX (DT_VERDEF)]
				- loadaddr;

			      do
				{
				  Elf_External_Verdef   evd;

				  GET_DATA (offset, evd, "version def");

				  ivd.vd_ndx  = BYTE_GET (evd.vd_ndx);
				  ivd.vd_aux  = BYTE_GET (evd.vd_aux);
				  ivd.vd_next = BYTE_GET (evd.vd_next);

				  offset += ivd.vd_next;
				}
			      while (ivd.vd_ndx != (vers_data & 0x7fff)
				     && ivd.vd_next != 0);

			      offset -= ivd.vd_next;
			      offset += ivd.vd_aux;

			      GET_DATA (offset, evda, "version def aux");

			      ivda.vda_name = BYTE_GET (evda.vda_name);

			      if (psym->st_name != ivda.vda_name)
				printf ((vers_data & 0x8000)
					? "@%s" : "@@%s",
					strtab + ivda.vda_name);
			    }
			}
		    }
		}

	      putchar ('\n');
	    }

	  free (symtab);
	  if (strtab != string_table)
	    free (strtab);
	}
    }
  else if (do_syms)
    printf
      (_("\nDynamic symbol information is not available for displaying symbols.\n"));

  if (do_histogram && buckets != NULL)
    {
      int *lengths;
      int *counts;
      int hn;
      int si;
      int maxlength = 0;
      int nzero_counts = 0;
      int nsyms = 0;

      printf (_("\nHistogram for bucket list length (total of %d buckets):\n"),
	      nbuckets);
      printf (_(" Length  Number     %% of total  Coverage\n"));

      lengths = (int *) calloc (nbuckets, sizeof (int));
      if (lengths == NULL)
	{
	  error (_("Out of memory"));
	  return 0;
	}
      for (hn = 0; hn < nbuckets; ++hn)
	{
	  if (! buckets [hn])
	    continue;

	  for (si = buckets[hn]; si > 0 && si < nchains; si = chains[si])
	    {
	      ++ nsyms;
	      if (maxlength < ++lengths[hn])
		++ maxlength;
	    }
	}

      counts = (int *) calloc (maxlength + 1, sizeof (int));
      if (counts == NULL)
	{
	  error (_("Out of memory"));
	  return 0;
	}

      for (hn = 0; hn < nbuckets; ++hn)
	++ counts [lengths [hn]];

      if (nbuckets > 0)
	{
	  printf ("      0  %-10d (%5.1f%%)\n",
		  counts[0], (counts[0] * 100.0) / nbuckets);
	  for (si = 1; si <= maxlength; ++si)
	    {
	      nzero_counts += counts[si] * si;
	      printf ("%7d  %-10d (%5.1f%%)    %5.1f%%\n",
		      si, counts[si], (counts[si] * 100.0) / nbuckets,
		      (nzero_counts * 100.0) / nsyms);
	    }
	}

      free (counts);
      free (lengths);
    }

  if (buckets != NULL)
    {
      free (buckets);
      free (chains);
    }

  return 1;
}

static int
process_syminfo (file)
     FILE * file ATTRIBUTE_UNUSED;
{
  unsigned int i;

  if (dynamic_syminfo == NULL
      || !do_dynamic)
    /* No syminfo, this is ok.  */
    return 1;

  /* There better should be a dynamic symbol section.  */
  if (dynamic_symbols == NULL || dynamic_strings == NULL)
    return 0;

  if (dynamic_addr)
    printf (_("\nDynamic info segment at offset 0x%lx contains %d entries:\n"),
	    dynamic_syminfo_offset, dynamic_syminfo_nent);

  printf (_(" Num: Name                           BoundTo     Flags\n"));
  for (i = 0; i < dynamic_syminfo_nent; ++i)
    {
      unsigned short int flags = dynamic_syminfo[i].si_flags;

      printf ("%4d: %-30s ", i,
	      dynamic_strings + dynamic_symbols[i].st_name);

      switch (dynamic_syminfo[i].si_boundto)
	{
	case SYMINFO_BT_SELF:
	  fputs ("SELF       ", stdout);
	  break;
	case SYMINFO_BT_PARENT:
	  fputs ("PARENT     ", stdout);
	  break;
	default:
	  if (dynamic_syminfo[i].si_boundto > 0
	      && dynamic_syminfo[i].si_boundto < dynamic_size)
	    printf ("%-10s ",
		    dynamic_strings
		    + dynamic_segment[dynamic_syminfo[i].si_boundto].d_un.d_val);
	  else
	    printf ("%-10d ", dynamic_syminfo[i].si_boundto);
	  break;
	}

      if (flags & SYMINFO_FLG_DIRECT)
	printf (" DIRECT");
      if (flags & SYMINFO_FLG_PASSTHRU)
	printf (" PASSTHRU");
      if (flags & SYMINFO_FLG_COPY)
	printf (" COPY");
      if (flags & SYMINFO_FLG_LAZYLOAD)
	printf (" LAZYLOAD");

      puts ("");
    }

  return 1;
}

#ifdef SUPPORT_DISASSEMBLY
static void
disassemble_section (section, file)
     Elf32_Internal_Shdr * section;
     FILE * file;
{
  printf (_("\nAssembly dump of section %s\n"),
	  SECTION_NAME (section));

  /* XXX -- to be done --- XXX */

  return 1;
}
#endif

static int
dump_section (section, file)
     Elf32_Internal_Shdr * section;
     FILE * file;
{
  bfd_size_type   bytes;
  bfd_vma         addr;
  unsigned char * data;
  unsigned char * start;

  bytes = section->sh_size;

  if (bytes == 0)
    {
      printf (_("\nSection '%s' has no data to dump.\n"),
	      SECTION_NAME (section));
      return 0;
    }
  else
    printf (_("\nHex dump of section '%s':\n"), SECTION_NAME (section));

  addr = section->sh_addr;

  GET_DATA_ALLOC (section->sh_offset, bytes, start, unsigned char *,
		  "section data");

  data = start;

  while (bytes)
    {
      int j;
      int k;
      int lbytes;

      lbytes = (bytes > 16 ? 16 : bytes);

      printf ("  0x%8.8lx ", (unsigned long) addr);

      switch (elf_header.e_ident [EI_DATA])
	{
	default:
	case ELFDATA2LSB:
	  for (j = 15; j >= 0; j --)
	    {
	      if (j < lbytes)
		printf ("%2.2x", data [j]);
	      else
		printf ("  ");

	      if (!(j & 0x3))
		printf (" ");
	    }
	  break;

	case ELFDATA2MSB:
	  for (j = 0; j < 16; j++)
	    {
	      if (j < lbytes)
		printf ("%2.2x", data [j]);
	      else
		printf ("  ");

	      if ((j & 3) == 3)
		printf (" ");
	    }
	  break;
	}

      for (j = 0; j < lbytes; j++)
	{
	  k = data [j];
	  if (k >= ' ' && k < 0x80)
	    printf ("%c", k);
	  else
	    printf (".");
	}

      putchar ('\n');

      data  += lbytes;
      addr  += lbytes;
      bytes -= lbytes;
    }

  free (start);

  return 1;
}


static unsigned long int
read_leb128 (data, length_return, sign)
     unsigned char * data;
     int *           length_return;
     int             sign;
{
  unsigned long int result = 0;
  unsigned int      num_read = 0;
  int               shift = 0;
  unsigned char     byte;

  do
    {
      byte = * data ++;
      num_read ++;

      result |= (byte & 0x7f) << shift;

      shift += 7;

    }
  while (byte & 0x80);

  if (length_return != NULL)
    * length_return = num_read;

  if (sign && (shift < 32) && (byte & 0x40))
    result |= -1 << shift;

  return result;
}

typedef struct State_Machine_Registers
{
  unsigned long	address;
  unsigned int  file;
  unsigned int  line;
  unsigned int  column;
  int           is_stmt;
  int           basic_block;
  int	        end_sequence;
/* This variable hold the number of the last entry seen
   in the File Table.  */
  unsigned int  last_file_entry;
} SMR;

static SMR state_machine_regs;

static void
reset_state_machine (is_stmt)
     int is_stmt;
{
  state_machine_regs.address = 0;
  state_machine_regs.file = 1;
  state_machine_regs.line = 1;
  state_machine_regs.column = 0;
  state_machine_regs.is_stmt = is_stmt;
  state_machine_regs.basic_block = 0;
  state_machine_regs.end_sequence = 0;
  state_machine_regs.last_file_entry = 0;
}

/* Handled an extend line op.  Returns true if this is the end
   of sequence.  */
static int
process_extended_line_op (data, is_stmt, pointer_size)
     unsigned char * data;
     int is_stmt;
     int pointer_size;
{
  unsigned char   op_code;
  int             bytes_read;
  unsigned int    len;
  unsigned char * name;
  unsigned long   adr;

  len = read_leb128 (data, & bytes_read, 0);
  data += bytes_read;

  if (len == 0)
    {
      warn (_("badly formed extended line op encountered!"));
      return bytes_read;
    }

  len += bytes_read;
  op_code = * data ++;

  printf (_("  Extended opcode %d: "), op_code);

  switch (op_code)
    {
    case DW_LNE_end_sequence:
      printf (_("End of Sequence\n\n"));
      reset_state_machine (is_stmt);
      break;

    case DW_LNE_set_address:
      adr = byte_get (data, pointer_size);
      printf (_("set Address to 0x%lx\n"), adr);
      state_machine_regs.address = adr;
      break;

    case DW_LNE_define_file:
      printf (_("  define new File Table entry\n"));
      printf (_("  Entry\tDir\tTime\tSize\tName\n"));

      printf (_("   %d\t"), ++ state_machine_regs.last_file_entry);
      name = data;
      data += strlen (data) + 1;
      printf (_("%lu\t"), read_leb128 (data, & bytes_read, 0));
      data += bytes_read;
      printf (_("%lu\t"), read_leb128 (data, & bytes_read, 0));
      data += bytes_read;
      printf (_("%lu\t"), read_leb128 (data, & bytes_read, 0));
      printf (_("%s\n\n"), name);
      break;

    default:
      printf (_("UNKNOWN: length %d\n"), len - bytes_read);
      break;
    }

  return len;
}

/* Size of pointers in the .debug_line section.  This information is not
   really present in that section.  It's obtained before dumping the debug
   sections by doing some pre-scan of the .debug_info section.  */
static int debug_line_pointer_size = 4;

static int
display_debug_lines (section, start, file)
     Elf32_Internal_Shdr * section;
     unsigned char *       start;
     FILE *                file ATTRIBUTE_UNUSED;
{
  DWARF2_External_LineInfo * external;
  DWARF2_Internal_LineInfo   info;
  unsigned char *            standard_opcodes;
  unsigned char *            data = start;
  unsigned char *            end  = start + section->sh_size;
  unsigned char *            end_of_sequence;
  int                        i;

  printf (_("\nDump of debug contents of section %s:\n\n"),
	  SECTION_NAME (section));

  while (data < end)
    {
      external = (DWARF2_External_LineInfo *) data;

      /* Check the length of the block.  */
      info.li_length = BYTE_GET (external->li_length);
      if (info.li_length > section->sh_size)
	{
	  warn
	    (_("The line info appears to be corrupt - the section is too small\n"));
	  return 0;
	}

      /* Check its version number.  */
      info.li_version = BYTE_GET (external->li_version);
      if (info.li_version != 2)
	{
	  warn (_("Only DWARF version 2 line info is currently supported.\n"));
	  return 0;
	}

      info.li_prologue_length = BYTE_GET (external->li_prologue_length);
      info.li_min_insn_length = BYTE_GET (external->li_min_insn_length);
      info.li_default_is_stmt = BYTE_GET (external->li_default_is_stmt);
      info.li_line_base       = BYTE_GET (external->li_line_base);
      info.li_line_range      = BYTE_GET (external->li_line_range);
      info.li_opcode_base     = BYTE_GET (external->li_opcode_base);

      /* Sign extend the line base field.  */
      info.li_line_base <<= 24;
      info.li_line_base >>= 24;

      printf (_("  Length:                      %ld\n"), info.li_length);
      printf (_("  DWARF Version:               %d\n"), info.li_version);
      printf (_("  Prolgue Length:              %d\n"), info.li_prologue_length);
      printf (_("  Minimum Instruction Length:  %d\n"), info.li_min_insn_length);
      printf (_("  Initial value of 'is_stmt':  %d\n"), info.li_default_is_stmt);
      printf (_("  Line Base:                   %d\n"), info.li_line_base);
      printf (_("  Line Range:                  %d\n"), info.li_line_range);
      printf (_("  Opcode Base:                 %d\n"), info.li_opcode_base);

      end_of_sequence = data + info.li_length + sizeof (info.li_length);

      reset_state_machine (info.li_default_is_stmt);

      /* Display the contents of the Opcodes table.  */
      standard_opcodes = data + sizeof (* external);

      printf (_("\n Opcodes:\n"));

      for (i = 1; i < info.li_opcode_base; i++)
	printf (_("  Opcode %d has %d args\n"), i, standard_opcodes[i - 1]);

      /* Display the contents of the Directory table.  */
      data = standard_opcodes + info.li_opcode_base - 1;

      if (* data == 0)
	printf (_("\n The Directory Table is empty.\n"));
      else
	{
	  printf (_("\n The Directory Table:\n"));

	  while (* data != 0)
	    {
	      printf (_("  %s\n"), data);

	      data += strlen (data) + 1;
	    }
	}

      /* Skip the NUL at the end of the table.  */
      data ++;

      /* Display the contents of the File Name table.  */
      if (* data == 0)
	printf (_("\n The File Name Table is empty.\n"));
      else
	{
	  printf (_("\n The File Name Table:\n"));
	  printf (_("  Entry\tDir\tTime\tSize\tName\n"));

	  while (* data != 0)
	    {
	      char * name;
	      int bytes_read;

	      printf (_("  %d\t"), ++ state_machine_regs.last_file_entry);
	      name = data;

	      data += strlen (data) + 1;

	      printf (_("%lu\t"), read_leb128 (data, & bytes_read, 0));
	      data += bytes_read;
	      printf (_("%lu\t"), read_leb128 (data, & bytes_read, 0));
	      data += bytes_read;
	      printf (_("%lu\t"), read_leb128 (data, & bytes_read, 0));
	      data += bytes_read;
	      printf (_("%s\n"), name);
	    }
	}

      /* Skip the NUL at the end of the table.  */
      data ++;

      /* Now display the statements.  */
      printf (_("\n Line Number Statements:\n"));


      while (data < end_of_sequence)
	{
	  unsigned char op_code;
	  int           adv;
	  int           bytes_read;

	  op_code = * data ++;

	  switch (op_code)
	    {
	    case DW_LNS_extended_op:
	      data += process_extended_line_op (data, info.li_default_is_stmt,
                                                debug_line_pointer_size);
	      break;

	    case DW_LNS_copy:
	      printf (_("  Copy\n"));
	      break;

	    case DW_LNS_advance_pc:
	      adv = info.li_min_insn_length * read_leb128 (data, & bytes_read, 0);
	      data += bytes_read;
	      state_machine_regs.address += adv;
	      printf (_("  Advance PC by %d to %lx\n"), adv,
		      state_machine_regs.address);
	      break;

	    case DW_LNS_advance_line:
	      adv = read_leb128 (data, & bytes_read, 1);
	      data += bytes_read;
	      state_machine_regs.line += adv;
	      printf (_("  Advance Line by %d to %d\n"), adv,
		      state_machine_regs.line);
	      break;

	    case DW_LNS_set_file:
	      adv = read_leb128 (data, & bytes_read, 0);
	      data += bytes_read;
	      printf (_("  Set File Name to entry %d in the File Name Table\n"),
		      adv);
	      state_machine_regs.file = adv;
	      break;

	    case DW_LNS_set_column:
	      adv = read_leb128 (data, & bytes_read, 0);
	      data += bytes_read;
	      printf (_("  Set column to %d\n"), adv);
	      state_machine_regs.column = adv;
	      break;

	    case DW_LNS_negate_stmt:
	      adv = state_machine_regs.is_stmt;
	      adv = ! adv;
	      printf (_("  Set is_stmt to %d\n"), adv);
	      state_machine_regs.is_stmt = adv;
	      break;

	    case DW_LNS_set_basic_block:
	      printf (_("  Set basic block\n"));
	      state_machine_regs.basic_block = 1;
	      break;

	    case DW_LNS_const_add_pc:
	      adv = (((255 - info.li_opcode_base) / info.li_line_range)
		     * info.li_min_insn_length);
	      state_machine_regs.address += adv;
	      printf (_("  Advance PC by constant %d to 0x%lx\n"), adv,
		      state_machine_regs.address);
	      break;

	    case DW_LNS_fixed_advance_pc:
	      adv = byte_get (data, 2);
	      data += 2;
	      state_machine_regs.address += adv;
	      printf (_("  Advance PC by fixed size amount %d to 0x%lx\n"),
		      adv, state_machine_regs.address);
	      break;

	    default:
	      op_code -= info.li_opcode_base;
	      adv      = (op_code / info.li_line_range) * info.li_min_insn_length;
	      state_machine_regs.address += adv;
	      printf (_("  Special opcode %d: advance Address by %d to 0x%lx"),
		      op_code, adv, state_machine_regs.address);
	      adv = (op_code % info.li_line_range) + info.li_line_base;
	      state_machine_regs.line += adv;
	      printf (_(" and Line by %d to %d\n"),
		      adv, state_machine_regs.line);
	      break;
	    }
	}
      printf ("\n");
    }

  return 1;
}

static int
display_debug_pubnames (section, start, file)
     Elf32_Internal_Shdr * section;
     unsigned char *       start;
     FILE *                file ATTRIBUTE_UNUSED;
{
  DWARF2_External_PubNames * external;
  DWARF2_Internal_PubNames   pubnames;
  unsigned char *            end;

  end = start + section->sh_size;

  printf (_("Contents of the %s section:\n\n"), SECTION_NAME (section));

  while (start < end)
    {
      unsigned char * data;
      unsigned long   offset;

      external = (DWARF2_External_PubNames *) start;

      pubnames.pn_length  = BYTE_GET (external->pn_length);
      pubnames.pn_version = BYTE_GET (external->pn_version);
      pubnames.pn_offset  = BYTE_GET (external->pn_offset);
      pubnames.pn_size    = BYTE_GET (external->pn_size);

      data   = start + sizeof (* external);
      start += pubnames.pn_length + sizeof (external->pn_length);

      if (pubnames.pn_version != 2)
	{
	  warn (_("Only DWARF 2 pubnames are currently supported"));
	  continue;
	}

      printf (_("  Length:                              %ld\n"),
	      pubnames.pn_length);
      printf (_("  Version:                             %d\n"),
	      pubnames.pn_version);
      printf (_("  Offset into .debug_info section:     %ld\n"),
	      pubnames.pn_offset);
      printf (_("  Size of area in .debug_info section: %ld\n"),
	      pubnames.pn_size);

      printf (_("\n    Offset\tName\n"));

      do
	{
	  offset = byte_get (data, 4);

	  if (offset != 0)
	    {
	      data += 4;
	      printf ("    %ld\t\t%s\n", offset, data);
	      data += strlen (data) + 1;
	    }
	}
      while (offset != 0);
    }

  printf ("\n");
  return 1;
}

static char *
get_TAG_name (tag)
     unsigned long tag;
{
  switch (tag)
    {
    case DW_TAG_padding: return "DW_TAG_padding";
    case DW_TAG_array_type: return "DW_TAG_array_type";
    case DW_TAG_class_type: return "DW_TAG_class_type";
    case DW_TAG_entry_point: return "DW_TAG_entry_point";
    case DW_TAG_enumeration_type: return "DW_TAG_enumeration_type";
    case DW_TAG_formal_parameter: return "DW_TAG_formal_parameter";
    case DW_TAG_imported_declaration: return "DW_TAG_imported_declaration";
    case DW_TAG_label: return "DW_TAG_label";
    case DW_TAG_lexical_block: return "DW_TAG_lexical_block";
    case DW_TAG_member: return "DW_TAG_member";
    case DW_TAG_pointer_type: return "DW_TAG_pointer_type";
    case DW_TAG_reference_type: return "DW_TAG_reference_type";
    case DW_TAG_compile_unit: return "DW_TAG_compile_unit";
    case DW_TAG_string_type: return "DW_TAG_string_type";
    case DW_TAG_structure_type: return "DW_TAG_structure_type";
    case DW_TAG_subroutine_type: return "DW_TAG_subroutine_type";
    case DW_TAG_typedef: return "DW_TAG_typedef";
    case DW_TAG_union_type: return "DW_TAG_union_type";
    case DW_TAG_unspecified_parameters: return "DW_TAG_unspecified_parameters";
    case DW_TAG_variant: return "DW_TAG_variant";
    case DW_TAG_common_block: return "DW_TAG_common_block";
    case DW_TAG_common_inclusion: return "DW_TAG_common_inclusion";
    case DW_TAG_inheritance: return "DW_TAG_inheritance";
    case DW_TAG_inlined_subroutine: return "DW_TAG_inlined_subroutine";
    case DW_TAG_module: return "DW_TAG_module";
    case DW_TAG_ptr_to_member_type: return "DW_TAG_ptr_to_member_type";
    case DW_TAG_set_type: return "DW_TAG_set_type";
    case DW_TAG_subrange_type: return "DW_TAG_subrange_type";
    case DW_TAG_with_stmt: return "DW_TAG_with_stmt";
    case DW_TAG_access_declaration: return "DW_TAG_access_declaration";
    case DW_TAG_base_type: return "DW_TAG_base_type";
    case DW_TAG_catch_block: return "DW_TAG_catch_block";
    case DW_TAG_const_type: return "DW_TAG_const_type";
    case DW_TAG_constant: return "DW_TAG_constant";
    case DW_TAG_enumerator: return "DW_TAG_enumerator";
    case DW_TAG_file_type: return "DW_TAG_file_type";
    case DW_TAG_friend: return "DW_TAG_friend";
    case DW_TAG_namelist: return "DW_TAG_namelist";
    case DW_TAG_namelist_item: return "DW_TAG_namelist_item";
    case DW_TAG_packed_type: return "DW_TAG_packed_type";
    case DW_TAG_subprogram: return "DW_TAG_subprogram";
    case DW_TAG_template_type_param: return "DW_TAG_template_type_param";
    case DW_TAG_template_value_param: return "DW_TAG_template_value_param";
    case DW_TAG_thrown_type: return "DW_TAG_thrown_type";
    case DW_TAG_try_block: return "DW_TAG_try_block";
    case DW_TAG_variant_part: return "DW_TAG_variant_part";
    case DW_TAG_variable: return "DW_TAG_variable";
    case DW_TAG_volatile_type: return "DW_TAG_volatile_type";
    case DW_TAG_MIPS_loop: return "DW_TAG_MIPS_loop";
    case DW_TAG_format_label: return "DW_TAG_format_label";
    case DW_TAG_function_template: return "DW_TAG_function_template";
    case DW_TAG_class_template: return "DW_TAG_class_template";
    default:
      {
	static char buffer [100];

	sprintf (buffer, _("Unknown TAG value: %lx"), tag);
	return buffer;
      }
    }
}

static char *
get_AT_name (attribute)
     unsigned long attribute;
{
  switch (attribute)
    {
    case DW_AT_sibling: return "DW_AT_sibling";
    case DW_AT_location: return "DW_AT_location";
    case DW_AT_name: return "DW_AT_name";
    case DW_AT_ordering: return "DW_AT_ordering";
    case DW_AT_subscr_data: return "DW_AT_subscr_data";
    case DW_AT_byte_size: return "DW_AT_byte_size";
    case DW_AT_bit_offset: return "DW_AT_bit_offset";
    case DW_AT_bit_size: return "DW_AT_bit_size";
    case DW_AT_element_list: return "DW_AT_element_list";
    case DW_AT_stmt_list: return "DW_AT_stmt_list";
    case DW_AT_low_pc: return "DW_AT_low_pc";
    case DW_AT_high_pc: return "DW_AT_high_pc";
    case DW_AT_language: return "DW_AT_language";
    case DW_AT_member: return "DW_AT_member";
    case DW_AT_discr: return "DW_AT_discr";
    case DW_AT_discr_value: return "DW_AT_discr_value";
    case DW_AT_visibility: return "DW_AT_visibility";
    case DW_AT_import: return "DW_AT_import";
    case DW_AT_string_length: return "DW_AT_string_length";
    case DW_AT_common_reference: return "DW_AT_common_reference";
    case DW_AT_comp_dir: return "DW_AT_comp_dir";
    case DW_AT_const_value: return "DW_AT_const_value";
    case DW_AT_containing_type: return "DW_AT_containing_type";
    case DW_AT_default_value: return "DW_AT_default_value";
    case DW_AT_inline: return "DW_AT_inline";
    case DW_AT_is_optional: return "DW_AT_is_optional";
    case DW_AT_lower_bound: return "DW_AT_lower_bound";
    case DW_AT_producer: return "DW_AT_producer";
    case DW_AT_prototyped: return "DW_AT_prototyped";
    case DW_AT_return_addr: return "DW_AT_return_addr";
    case DW_AT_start_scope: return "DW_AT_start_scope";
    case DW_AT_stride_size: return "DW_AT_stride_size";
    case DW_AT_upper_bound: return "DW_AT_upper_bound";
    case DW_AT_abstract_origin: return "DW_AT_abstract_origin";
    case DW_AT_accessibility: return "DW_AT_accessibility";
    case DW_AT_address_class: return "DW_AT_address_class";
    case DW_AT_artificial: return "DW_AT_artificial";
    case DW_AT_base_types: return "DW_AT_base_types";
    case DW_AT_calling_convention: return "DW_AT_calling_convention";
    case DW_AT_count: return "DW_AT_count";
    case DW_AT_data_member_location: return "DW_AT_data_member_location";
    case DW_AT_decl_column: return "DW_AT_decl_column";
    case DW_AT_decl_file: return "DW_AT_decl_file";
    case DW_AT_decl_line: return "DW_AT_decl_line";
    case DW_AT_declaration: return "DW_AT_declaration";
    case DW_AT_discr_list: return "DW_AT_discr_list";
    case DW_AT_encoding: return "DW_AT_encoding";
    case DW_AT_external: return "DW_AT_external";
    case DW_AT_frame_base: return "DW_AT_frame_base";
    case DW_AT_friend: return "DW_AT_friend";
    case DW_AT_identifier_case: return "DW_AT_identifier_case";
    case DW_AT_macro_info: return "DW_AT_macro_info";
    case DW_AT_namelist_items: return "DW_AT_namelist_items";
    case DW_AT_priority: return "DW_AT_priority";
    case DW_AT_segment: return "DW_AT_segment";
    case DW_AT_specification: return "DW_AT_specification";
    case DW_AT_static_link: return "DW_AT_static_link";
    case DW_AT_type: return "DW_AT_type";
    case DW_AT_use_location: return "DW_AT_use_location";
    case DW_AT_variable_parameter: return "DW_AT_variable_parameter";
    case DW_AT_virtuality: return "DW_AT_virtuality";
    case DW_AT_vtable_elem_location: return "DW_AT_vtable_elem_location";
    case DW_AT_MIPS_fde: return "DW_AT_MIPS_fde";
    case DW_AT_MIPS_loop_begin: return "DW_AT_MIPS_loop_begin";
    case DW_AT_MIPS_tail_loop_begin: return "DW_AT_MIPS_tail_loop_begin";
    case DW_AT_MIPS_epilog_begin: return "DW_AT_MIPS_epilog_begin";
    case DW_AT_MIPS_loop_unroll_factor: return "DW_AT_MIPS_loop_unroll_factor";
    case DW_AT_MIPS_software_pipeline_depth: return "DW_AT_MIPS_software_pipeline_depth";
    case DW_AT_MIPS_linkage_name: return "DW_AT_MIPS_linkage_name";
    case DW_AT_MIPS_stride: return "DW_AT_MIPS_stride";
    case DW_AT_MIPS_abstract_name: return "DW_AT_MIPS_abstract_name";
    case DW_AT_MIPS_clone_origin: return "DW_AT_MIPS_clone_origin";
    case DW_AT_MIPS_has_inlines: return "DW_AT_MIPS_has_inlines";
    case DW_AT_sf_names: return "DW_AT_sf_names";
    case DW_AT_src_info: return "DW_AT_src_info";
    case DW_AT_mac_info: return "DW_AT_mac_info";
    case DW_AT_src_coords: return "DW_AT_src_coords";
    case DW_AT_body_begin: return "DW_AT_body_begin";
    case DW_AT_body_end: return "DW_AT_body_end";
    default:
      {
	static char buffer [100];

	sprintf (buffer, _("Unknown AT value: %lx"), attribute);
	return buffer;
      }
    }
}

static char *
get_FORM_name (form)
     unsigned long form;
{
  switch (form)
    {
    case DW_FORM_addr: return "DW_FORM_addr";
    case DW_FORM_block2: return "DW_FORM_block2";
    case DW_FORM_block4: return "DW_FORM_block4";
    case DW_FORM_data2: return "DW_FORM_data2";
    case DW_FORM_data4: return "DW_FORM_data4";
    case DW_FORM_data8: return "DW_FORM_data8";
    case DW_FORM_string: return "DW_FORM_string";
    case DW_FORM_block: return "DW_FORM_block";
    case DW_FORM_block1: return "DW_FORM_block1";
    case DW_FORM_data1: return "DW_FORM_data1";
    case DW_FORM_flag: return "DW_FORM_flag";
    case DW_FORM_sdata: return "DW_FORM_sdata";
    case DW_FORM_strp: return "DW_FORM_strp";
    case DW_FORM_udata: return "DW_FORM_udata";
    case DW_FORM_ref_addr: return "DW_FORM_ref_addr";
    case DW_FORM_ref1: return "DW_FORM_ref1";
    case DW_FORM_ref2: return "DW_FORM_ref2";
    case DW_FORM_ref4: return "DW_FORM_ref4";
    case DW_FORM_ref8: return "DW_FORM_ref8";
    case DW_FORM_ref_udata: return "DW_FORM_ref_udata";
    case DW_FORM_indirect: return "DW_FORM_indirect";
    default:
      {
	static char buffer [100];

	sprintf (buffer, _("Unknown FORM value: %lx"), form);
	return buffer;
      }
    }
}

/* FIXME:  There are better and more effiecint ways to handle
   these structures.  For now though, I just want something that
   is simple to implement.  */
typedef struct abbrev_attr
{
  unsigned long        attribute;
  unsigned long        form;
  struct abbrev_attr * next;
}
abbrev_attr;

typedef struct abbrev_entry
{
  unsigned long          entry;
  unsigned long          tag;
  int                    children;
  struct abbrev_attr *   first_attr;
  struct abbrev_attr *   last_attr;
  struct abbrev_entry *  next;
}
abbrev_entry;

static abbrev_entry * first_abbrev = NULL;
static abbrev_entry * last_abbrev = NULL;

static void
free_abbrevs PARAMS ((void))
{
  abbrev_entry * abbrev;

  for (abbrev = first_abbrev; abbrev;)
    {
      abbrev_entry * next = abbrev->next;
      abbrev_attr  * attr;

      for (attr = abbrev->first_attr; attr;)
	{
	  abbrev_attr * next = attr->next;

	  free (attr);
	  attr = next;
	}

      free (abbrev);
      abbrev = next;
    }

  last_abbrev = first_abbrev = NULL;
}

static void
add_abbrev (number, tag, children)
     unsigned long number;
     unsigned long tag;
     int           children;
{
  abbrev_entry * entry;

  entry = (abbrev_entry *) malloc (sizeof (* entry));

  if (entry == NULL)
    /* ugg */
    return;

  entry->entry      = number;
  entry->tag        = tag;
  entry->children   = children;
  entry->first_attr = NULL;
  entry->last_attr  = NULL;
  entry->next       = NULL;

  if (first_abbrev == NULL)
    first_abbrev = entry;
  else
    last_abbrev->next = entry;

  last_abbrev = entry;
}

static void
add_abbrev_attr (attribute, form)
     unsigned long attribute;
     unsigned long form;
{
  abbrev_attr * attr;

  attr = (abbrev_attr *) malloc (sizeof (* attr));

  if (attr == NULL)
    /* ugg */
    return;

  attr->attribute = attribute;
  attr->form      = form;
  attr->next      = NULL;

  if (last_abbrev->first_attr == NULL)
    last_abbrev->first_attr = attr;
  else
    last_abbrev->last_attr->next = attr;

  last_abbrev->last_attr = attr;
}

/* Processes the (partial) contents of a .debug_abbrev section.
   Returns NULL if the end of the section was encountered.
   Returns the address after the last byte read if the end of
   an abbreviation set was found.  */

static unsigned char *
process_abbrev_section (start, end)
     unsigned char * start;
     unsigned char * end;
{
  if (first_abbrev != NULL)
    return NULL;

  while (start < end)
    {
      int           bytes_read;
      unsigned long entry;
      unsigned long tag;
      unsigned long attribute;
      int           children;

      entry = read_leb128 (start, & bytes_read, 0);
      start += bytes_read;

      /* A single zero is supposed to end the section according
	 to the standard.  If there's more, then signal that to
	 the caller.  */
      if (entry == 0)
	return start == end ? NULL : start;

      tag = read_leb128 (start, & bytes_read, 0);
      start += bytes_read;

      children = * start ++;

      add_abbrev (entry, tag, children);

      do
	{
	  unsigned long form;

	  attribute = read_leb128 (start, & bytes_read, 0);
	  start += bytes_read;

	  form = read_leb128 (start, & bytes_read, 0);
	  start += bytes_read;

	  if (attribute != 0)
	    add_abbrev_attr (attribute, form);
	}
      while (attribute != 0);
    }

  return NULL;
}


static int
display_debug_abbrev (section, start, file)
     Elf32_Internal_Shdr * section;
     unsigned char *       start;
     FILE *                file ATTRIBUTE_UNUSED;
{
  abbrev_entry * entry;
  unsigned char * end = start + section->sh_size;

  printf (_("Contents of the %s section:\n\n"), SECTION_NAME (section));

  do
    {
      start = process_abbrev_section (start, end);

      printf (_("  Number TAG\n"));

      for (entry = first_abbrev; entry; entry = entry->next)
	{
	  abbrev_attr * attr;

	  printf (_("   %ld      %s    [%s]\n"),
		  entry->entry,
		  get_TAG_name (entry->tag),
		  entry->children ? _("has children") : _("no children"));

	  for (attr = entry->first_attr; attr; attr = attr->next)
	    {
	      printf (_("    %-18s %s\n"),
		      get_AT_name (attr->attribute),
		      get_FORM_name (attr->form));
	    }
	}
    }
  while (start);

  printf ("\n");

  return 1;
}


static unsigned char *
display_block (data, length)
     unsigned char * data;
     unsigned long   length;
{
  printf (_(" %lu byte block: "), length);

  while (length --)
    printf ("%lx ", (unsigned long) byte_get (data ++, 1));

  return data;
}

static void
decode_location_expression (data, pointer_size)
     unsigned char * data;
     unsigned int    pointer_size;
{
  unsigned char op;
  int           bytes_read;
  unsigned long uvalue;

  op = * data ++;

  switch (op)
    {
    case DW_OP_addr:
      printf ("DW_OP_addr: %lx", (unsigned long) byte_get (data, pointer_size));
      break;
    case DW_OP_deref:
      printf ("DW_OP_deref");
      break;
    case DW_OP_const1u:
      printf ("DW_OP_const1u: %lu", (unsigned long) byte_get (data, 1));
      break;
    case DW_OP_const1s:
      printf ("DW_OP_const1s: %ld", (long) byte_get (data, 1));
      break;
    case DW_OP_const2u:
      printf ("DW_OP_const2u: %lu", (unsigned long) byte_get (data, 2));
      break;
    case DW_OP_const2s:
      printf ("DW_OP_const2s: %ld", (long) byte_get (data, 2));
      break;
    case DW_OP_const4u:
      printf ("DW_OP_const4u: %lu", (unsigned long) byte_get (data, 4));
      break;
    case DW_OP_const4s:
      printf ("DW_OP_const4s: %ld", (long) byte_get (data, 4));
      break;
    case DW_OP_const8u:
      printf ("DW_OP_const8u: %lu %lu", (unsigned long) byte_get (data, 4),
	      (unsigned long) byte_get (data + 4, 4));
      break;
    case DW_OP_const8s:
      printf ("DW_OP_const8s: %ld %ld", (long) byte_get (data, 4),
	      (long) byte_get (data + 4, 4));
      break;
    case DW_OP_constu:
      printf ("DW_OP_constu: %lu", read_leb128 (data, NULL, 0));
      break;
    case DW_OP_consts:
      printf ("DW_OP_consts: %ld", read_leb128 (data, NULL, 1));
      break;
    case DW_OP_dup:
      printf ("DW_OP_dup");
      break;
    case DW_OP_drop:
      printf ("DW_OP_drop");
      break;
    case DW_OP_over:
      printf ("DW_OP_over");
      break;
    case DW_OP_pick:
      printf ("DW_OP_pick: %ld", (unsigned long) byte_get (data, 1));
      break;
    case DW_OP_swap:
      printf ("DW_OP_swap");
      break;
    case DW_OP_rot:
      printf ("DW_OP_rot");
      break;
    case DW_OP_xderef:
      printf ("DW_OP_xderef");
      break;
    case DW_OP_abs:
      printf ("DW_OP_abs");
      break;
    case DW_OP_and:
      printf ("DW_OP_and");
      break;
    case DW_OP_div:
      printf ("DW_OP_div");
      break;
    case DW_OP_minus:
      printf ("DW_OP_minus");
      break;
    case DW_OP_mod:
      printf ("DW_OP_mod");
      break;
    case DW_OP_mul:
      printf ("DW_OP_mul");
      break;
    case DW_OP_neg:
      printf ("DW_OP_neg");
      break;
    case DW_OP_not:
      printf ("DW_OP_not");
      break;
    case DW_OP_or:
      printf ("DW_OP_or");
      break;
    case DW_OP_plus:
      printf ("DW_OP_plus");
      break;
    case DW_OP_plus_uconst:
      printf ("DW_OP_plus_uconst: %lu", read_leb128 (data, NULL, 0));
      break;
    case DW_OP_shl:
      printf ("DW_OP_shl");
      break;
    case DW_OP_shr:
      printf ("DW_OP_shr");
      break;
    case DW_OP_shra:
      printf ("DW_OP_shra");
      break;
    case DW_OP_xor:
      printf ("DW_OP_xor");
      break;
    case DW_OP_bra:
      printf ("DW_OP_bra: %ld", (long) byte_get (data, 2));
      break;
    case DW_OP_eq:
      printf ("DW_OP_eq");
      break;
    case DW_OP_ge:
      printf ("DW_OP_ge");
      break;
    case DW_OP_gt:
      printf ("DW_OP_gt");
      break;
    case DW_OP_le:
      printf ("DW_OP_le");
      break;
    case DW_OP_lt:
      printf ("DW_OP_lt");
      break;
    case DW_OP_ne:
      printf ("DW_OP_ne");
      break;
    case DW_OP_skip:
      printf ("DW_OP_skip: %ld", (long) byte_get (data, 2));
      break;
    case DW_OP_lit0:
      printf ("DW_OP_lit0");
      break;
    case DW_OP_lit1:
      printf ("DW_OP_lit1");
      break;
    case DW_OP_lit2:
      printf ("DW_OP_lit2");
      break;
    case DW_OP_lit3:
      printf ("DW_OP_lit3");
      break;
    case DW_OP_lit4:
      printf ("DW_OP_lit4");
      break;
    case DW_OP_lit5:
      printf ("DW_OP_lit5");
      break;
    case DW_OP_lit6:
      printf ("DW_OP_lit6");
      break;
    case DW_OP_lit7:
      printf ("DW_OP_lit7");
      break;
    case DW_OP_lit8:
      printf ("DW_OP_lit8");
      break;
    case DW_OP_lit9:
      printf ("DW_OP_lit9");
      break;
    case DW_OP_lit10:
      printf ("DW_OP_lit10");
      break;
    case DW_OP_lit11:
      printf ("DW_OP_lit11");
      break;
    case DW_OP_lit12:
      printf ("DW_OP_lit12");
      break;
    case DW_OP_lit13:
      printf ("DW_OP_lit13");
      break;
    case DW_OP_lit14:
      printf ("DW_OP_lit14");
      break;
    case DW_OP_lit15:
      printf ("DW_OP_lit15");
      break;
    case DW_OP_lit16:
      printf ("DW_OP_lit16");
      break;
    case DW_OP_lit17:
      printf ("DW_OP_lit17");
      break;
    case DW_OP_lit18:
      printf ("DW_OP_lit18");
      break;
    case DW_OP_lit19:
      printf ("DW_OP_lit19");
      break;
    case DW_OP_lit20:
      printf ("DW_OP_lit20");
      break;
    case DW_OP_lit21:
      printf ("DW_OP_lit21");
      break;
    case DW_OP_lit22:
      printf ("DW_OP_lit22");
      break;
    case DW_OP_lit23:
      printf ("DW_OP_lit23");
      break;
    case DW_OP_lit24:
      printf ("DW_OP_lit24");
      break;
    case DW_OP_lit25:
      printf ("DW_OP_lit25");
      break;
    case DW_OP_lit26:
      printf ("DW_OP_lit26");
      break;
    case DW_OP_lit27:
      printf ("DW_OP_lit27");
      break;
    case DW_OP_lit28:
      printf ("DW_OP_lit28");
      break;
    case DW_OP_lit29:
      printf ("DW_OP_lit29");
      break;
    case DW_OP_lit30:
      printf ("DW_OP_lit30");
      break;
    case DW_OP_lit31:
      printf ("DW_OP_lit31");
      break;
    case DW_OP_reg0:
      printf ("DW_OP_reg0");
      break;
    case DW_OP_reg1:
      printf ("DW_OP_reg1");
      break;
    case DW_OP_reg2:
      printf ("DW_OP_reg2");
      break;
    case DW_OP_reg3:
      printf ("DW_OP_reg3");
      break;
    case DW_OP_reg4:
      printf ("DW_OP_reg4");
      break;
    case DW_OP_reg5:
      printf ("DW_OP_reg5");
      break;
    case DW_OP_reg6:
      printf ("DW_OP_reg6");
      break;
    case DW_OP_reg7:
      printf ("DW_OP_reg7");
      break;
    case DW_OP_reg8:
      printf ("DW_OP_reg8");
      break;
    case DW_OP_reg9:
      printf ("DW_OP_reg9");
      break;
    case DW_OP_reg10:
      printf ("DW_OP_reg10");
      break;
    case DW_OP_reg11:
      printf ("DW_OP_reg11");
      break;
    case DW_OP_reg12:
      printf ("DW_OP_reg12");
      break;
    case DW_OP_reg13:
      printf ("DW_OP_reg13");
      break;
    case DW_OP_reg14:
      printf ("DW_OP_reg14");
      break;
    case DW_OP_reg15:
      printf ("DW_OP_reg15");
      break;
    case DW_OP_reg16:
      printf ("DW_OP_reg16");
      break;
    case DW_OP_reg17:
      printf ("DW_OP_reg17");
      break;
    case DW_OP_reg18:
      printf ("DW_OP_reg18");
      break;
    case DW_OP_reg19:
      printf ("DW_OP_reg19");
      break;
    case DW_OP_reg20:
      printf ("DW_OP_reg20");
      break;
    case DW_OP_reg21:
      printf ("DW_OP_reg21");
      break;
    case DW_OP_reg22:
      printf ("DW_OP_reg22");
      break;
    case DW_OP_reg23:
      printf ("DW_OP_reg23");
      break;
    case DW_OP_reg24:
      printf ("DW_OP_reg24");
      break;
    case DW_OP_reg25:
      printf ("DW_OP_reg25");
      break;
    case DW_OP_reg26:
      printf ("DW_OP_reg26");
      break;
    case DW_OP_reg27:
      printf ("DW_OP_reg27");
      break;
    case DW_OP_reg28:
      printf ("DW_OP_reg28");
      break;
    case DW_OP_reg29:
      printf ("DW_OP_reg29");
      break;
    case DW_OP_reg30:
      printf ("DW_OP_reg30");
      break;
    case DW_OP_reg31:
      printf ("DW_OP_reg31");
      break;
    case DW_OP_breg0:
      printf ("DW_OP_breg0: %ld", read_leb128 (data, NULL, 1));
      break;
    case DW_OP_breg1:
      printf ("DW_OP_breg1: %ld", read_leb128 (data, NULL, 1));
      break;
    case DW_OP_breg2:
      printf ("DW_OP_breg2: %ld", read_leb128 (data, NULL, 1));
      break;
    case DW_OP_breg3:
      printf ("DW_OP_breg3: %ld", read_leb128 (data, NULL, 1));
      break;
    case DW_OP_breg4:
      printf ("DW_OP_breg4: %ld", read_leb128 (data, NULL, 1));
      break;
    case DW_OP_breg5:
      printf ("DW_OP_breg5: %ld", read_leb128 (data, NULL, 1));
      break;
    case DW_OP_breg6:
      printf ("DW_OP_breg6: %ld", read_leb128 (data, NULL, 1));
      break;
    case DW_OP_breg7:
      printf ("DW_OP_breg7: %ld", read_leb128 (data, NULL, 1));
      break;
    case DW_OP_breg8:
      printf ("DW_OP_breg8: %ld", read_leb128 (data, NULL, 1));
      break;
    case DW_OP_breg9:
      printf ("DW_OP_breg9: %ld", read_leb128 (data, NULL, 1));
      break;
    case DW_OP_breg10:
      printf ("DW_OP_breg10: %ld", read_leb128 (data, NULL, 1));
      break;
    case DW_OP_breg11:
      printf ("DW_OP_breg11: %ld", read_leb128 (data, NULL, 1));
      break;
    case DW_OP_breg12:
      printf ("DW_OP_breg12: %ld", read_leb128 (data, NULL, 1));
      break;
    case DW_OP_breg13:
      printf ("DW_OP_breg13: %ld", read_leb128 (data, NULL, 1));
      break;
    case DW_OP_breg14:
      printf ("DW_OP_breg14: %ld", read_leb128 (data, NULL, 1));
      break;
    case DW_OP_breg15:
      printf ("DW_OP_breg15: %ld", read_leb128 (data, NULL, 1));
      break;
    case DW_OP_breg16:
      printf ("DW_OP_breg16: %ld", read_leb128 (data, NULL, 1));
      break;
    case DW_OP_breg17:
      printf ("DW_OP_breg17: %ld", read_leb128 (data, NULL, 1));
      break;
    case DW_OP_breg18:
      printf ("DW_OP_breg18: %ld", read_leb128 (data, NULL, 1));
      break;
    case DW_OP_breg19:
      printf ("DW_OP_breg19: %ld", read_leb128 (data, NULL, 1));
      break;
    case DW_OP_breg20:
      printf ("DW_OP_breg20: %ld", read_leb128 (data, NULL, 1));
      break;
    case DW_OP_breg21:
      printf ("DW_OP_breg21: %ld", read_leb128 (data, NULL, 1));
      break;
    case DW_OP_breg22:
      printf ("DW_OP_breg22: %ld", read_leb128 (data, NULL, 1));
      break;
    case DW_OP_breg23:
      printf ("DW_OP_breg23: %ld", read_leb128 (data, NULL, 1));
      break;
    case DW_OP_breg24:
      printf ("DW_OP_breg24: %ld", read_leb128 (data, NULL, 1));
      break;
    case DW_OP_breg25:
      printf ("DW_OP_breg25: %ld", read_leb128 (data, NULL, 1));
      break;
    case DW_OP_breg26:
      printf ("DW_OP_breg26: %ld", read_leb128 (data, NULL, 1));
      break;
    case DW_OP_breg27:
      printf ("DW_OP_breg27: %ld", read_leb128 (data, NULL, 1));
      break;
    case DW_OP_breg28:
      printf ("DW_OP_breg28: %ld", read_leb128 (data, NULL, 1));
      break;
    case DW_OP_breg29:
      printf ("DW_OP_breg29: %ld", read_leb128 (data, NULL, 1));
      break;
    case DW_OP_breg30:
      printf ("DW_OP_breg30: %ld", read_leb128 (data, NULL, 1));
      break;
    case DW_OP_breg31:
      printf ("DW_OP_breg31: %ld", read_leb128 (data, NULL, 1));
      break;
    case DW_OP_regx:
      printf ("DW_OP_regx: %lu", read_leb128 (data, NULL, 0));
      break;
    case DW_OP_fbreg:
      printf ("DW_OP_fbreg: %ld", read_leb128 (data, NULL, 1));
      break;
    case DW_OP_bregx:
      uvalue = read_leb128 (data, &bytes_read, 0);
      printf ("DW_OP_bregx: %lu %ld", uvalue,
	      read_leb128 (data + bytes_read, NULL, 1));
      break;
    case DW_OP_piece:
      printf ("DW_OP_piece: %lu", read_leb128 (data, NULL, 0));
      break;
    case DW_OP_deref_size:
      printf ("DW_OP_deref_size: %ld", (long) byte_get (data, 1));
      break;
    case DW_OP_xderef_size:
      printf ("DW_OP_xderef_size: %ld", (long) byte_get (data, 1));
      break;
    case DW_OP_nop:
      printf ("DW_OP_nop");
      break;

    default:
      if (op >= DW_OP_lo_user
	  && op <= DW_OP_hi_user)
	printf (_("(User defined location op)"));
      else
	printf (_("(Unknown location op)"));
      break;
    }
}


static unsigned char *
read_and_display_attr (attribute, form, data, pointer_size)
     unsigned long   attribute;
     unsigned long   form;
     unsigned char * data;
     unsigned long   pointer_size;
{
  unsigned long   uvalue = 0;
  unsigned char * block_start = NULL;
  int             bytes_read;
  int		  is_ref = 0;

  printf ("     %-18s:", get_AT_name (attribute));

  switch (form)
    {
    case DW_FORM_ref_addr:
    case DW_FORM_ref1:
    case DW_FORM_ref2:
    case DW_FORM_ref4:
    case DW_FORM_ref8:
    case DW_FORM_ref_udata:
      is_ref = 1;
    }

  switch (form)
    {
    case DW_FORM_ref_addr:
    case DW_FORM_addr:
      uvalue = byte_get (data, pointer_size);
      printf (is_ref ? " <%lx>" : " %#lx", uvalue);
      data += pointer_size;
      break;

    case DW_FORM_ref1:
    case DW_FORM_flag:
    case DW_FORM_data1:
      uvalue = byte_get (data ++, 1);
      printf (is_ref ? " <%lx>" : " %ld", uvalue);
      break;

    case DW_FORM_ref2:
    case DW_FORM_data2:
      uvalue = byte_get (data, 2);
      data += 2;
      printf (is_ref ? " <%lx>" : " %ld", uvalue);
      break;

    case DW_FORM_ref4:
    case DW_FORM_data4:
      uvalue = byte_get (data, 4);
      data += 4;
      printf (is_ref ? " <%lx>" : " %ld", uvalue);
      break;

    case DW_FORM_ref8:
    case DW_FORM_data8:
      uvalue = byte_get (data, 4);
      printf (" %lx", uvalue);
      printf (" %lx", (unsigned long) byte_get (data + 4, 4));
      data += 8;
      break;

    case DW_FORM_string:
      printf (" %s", data);
      data += strlen (data) + 1;
      break;

    case DW_FORM_sdata:
      uvalue = read_leb128 (data, & bytes_read, 1);
      data += bytes_read;
      printf (" %ld", (long) uvalue);
      break;

    case DW_FORM_ref_udata:
    case DW_FORM_udata:
      uvalue = read_leb128 (data, & bytes_read, 0);
      data += bytes_read;
      printf (is_ref ? " <%lx>" : " %ld", uvalue);
      break;

    case DW_FORM_block:
      uvalue = read_leb128 (data, & bytes_read, 0);
      block_start = data + bytes_read;
      data = display_block (block_start, uvalue);
      uvalue = * block_start;
      break;

    case DW_FORM_block1:
      uvalue = byte_get (data, 1);
      block_start = data + 1;
      data = display_block (block_start, uvalue);
      uvalue = * block_start;
      break;

    case DW_FORM_block2:
      uvalue = byte_get (data, 2);
      block_start = data + 2;
      data = display_block (block_start, uvalue);
      uvalue = * block_start;
      break;

    case DW_FORM_block4:
      uvalue = byte_get (data, 4);
      block_start = data + 4;
      data = display_block (block_start, uvalue);
      uvalue = * block_start;
      break;

    case DW_FORM_strp:
    case DW_FORM_indirect:
      warn (_("Unable to handle FORM: %d"), form);
      break;

    default:
      warn (_("Unrecognised form: %d"), form);
      break;
    }

  /* For some attributes we can display futher information.  */

  printf ("\t");

  switch (attribute)
    {
    case DW_AT_inline:
      switch (uvalue)
	{
	case DW_INL_not_inlined:          printf (_("(not inlined)")); break;
	case DW_INL_inlined:              printf (_("(inlined)")); break;
	case DW_INL_declared_not_inlined: printf (_("(declared as inline but ignored)")); break;
	case DW_INL_declared_inlined:     printf (_("(declared as inline and inlined)")); break;
	default: printf (_("  (Unknown inline attribute value: %lx)"), uvalue); break;
	}
      break;

    case DW_AT_frame_base:
      if (uvalue >= DW_OP_reg0 && uvalue <= DW_OP_reg31)
	printf ("(reg %ld)", uvalue - DW_OP_reg0);
      break;

    case DW_AT_language:
      switch (uvalue)
	{
	case DW_LANG_C:              printf ("(non-ANSI C)"); break;
	case DW_LANG_C89:            printf ("(ANSI C)"); break;
	case DW_LANG_C_plus_plus:    printf ("(C++)"); break;
	case DW_LANG_Fortran77:      printf ("(FORTRAN 77)"); break;
	case DW_LANG_Fortran90:      printf ("(Fortran 90)"); break;
	case DW_LANG_Modula2:        printf ("(Modula 2)"); break;
	case DW_LANG_Pascal83:       printf ("(ANSI Pascal)"); break;
	case DW_LANG_Ada83:          printf ("(Ada)"); break;
	case DW_LANG_Cobol74:        printf ("(Cobol 74)"); break;
	case DW_LANG_Cobol85:        printf ("(Cobol 85)"); break;
	case DW_LANG_Mips_Assembler: printf ("(MIPS assembler)"); break;
	default:                     printf ("(Unknown: %lx)", uvalue); break;
	}
      break;

    case DW_AT_encoding:
      switch (uvalue)
	{
	case DW_ATE_void:            printf ("(void)"); break;
	case DW_ATE_address:         printf ("(machine address)"); break;
	case DW_ATE_boolean:         printf ("(boolean)"); break;
	case DW_ATE_complex_float:   printf ("(complex float)"); break;
	case DW_ATE_float:           printf ("(float)"); break;
	case DW_ATE_signed:          printf ("(signed)"); break;
	case DW_ATE_signed_char:     printf ("(signed char)"); break;
	case DW_ATE_unsigned:        printf ("(unsigned)"); break;
	case DW_ATE_unsigned_char:   printf ("(unsigned char)"); break;
	default:
	  if (uvalue >= DW_ATE_lo_user
	      && uvalue <= DW_ATE_hi_user)
	    printf ("(user defined type)");
	  else
	    printf ("(unknown type)");
	  break;
	}
      break;

    case DW_AT_accessibility:
      switch (uvalue)
	{
	case DW_ACCESS_public:		printf ("(public)"); break;
	case DW_ACCESS_protected:	printf ("(protected)"); break;
	case DW_ACCESS_private:		printf ("(private)"); break;
	default:		        printf ("(unknown accessibility)"); break;
	}
      break;

    case DW_AT_visibility:
      switch (uvalue)
	{
	case DW_VIS_local:	printf ("(local)"); break;
	case DW_VIS_exported:	printf ("(exported)"); break;
	case DW_VIS_qualified:	printf ("(qualified)"); break;
	default:		printf ("(unknown visibility)"); break;
	}
      break;

    case DW_AT_virtuality:
      switch (uvalue)
	{
	case DW_VIRTUALITY_none:	printf ("(none)"); break;
	case DW_VIRTUALITY_virtual:	printf ("(virtual)"); break;
	case DW_VIRTUALITY_pure_virtual:printf ("(pure_virtual)"); break;
	default:		        printf ("(unknown virtuality)"); break;
	}
      break;

    case DW_AT_identifier_case:
      switch (uvalue)
	{
	case DW_ID_case_sensitive:	printf ("(case_sensitive)"); break;
	case DW_ID_up_case:		printf ("(up_case)"); break;
	case DW_ID_down_case:		printf ("(down_case)"); break;
	case DW_ID_case_insensitive:	printf ("(case_insensitive)"); break;
	default:		        printf ("(unknown case)"); break;
	}
      break;

    case DW_AT_calling_convention:
      switch (uvalue)
	{
	case DW_CC_normal:	printf ("(normal)"); break;
	case DW_CC_program:	printf ("(program)"); break;
	case DW_CC_nocall:	printf ("(nocall)"); break;
	default:
	  if (uvalue >= DW_CC_lo_user
	      && uvalue <= DW_CC_hi_user)
	    printf ("(user defined)");
	  else
	    printf ("(unknown convention)");
	}
      break;

    case DW_AT_location:
    case DW_AT_data_member_location:
    case DW_AT_vtable_elem_location:
      printf ("(");
      decode_location_expression (block_start, pointer_size);
      printf (")");
      break;

    default:
      break;
    }

  printf ("\n");
  return data;
}

static int
display_debug_info (section, start, file)
     Elf32_Internal_Shdr * section;
     unsigned char *       start;
     FILE *                file;
{
  unsigned char * end = start + section->sh_size;
  unsigned char * section_begin = start;

  printf (_("The section %s contains:\n\n"), SECTION_NAME (section));

  while (start < end)
    {
      DWARF2_External_CompUnit * external;
      DWARF2_Internal_CompUnit   compunit;
      unsigned char *            tags;
      int                        i;
      int			 level;

      external = (DWARF2_External_CompUnit *) start;

      compunit.cu_length        = BYTE_GET (external->cu_length);
      compunit.cu_version       = BYTE_GET (external->cu_version);
      compunit.cu_abbrev_offset = BYTE_GET (external->cu_abbrev_offset);
      compunit.cu_pointer_size  = BYTE_GET (external->cu_pointer_size);

      tags = start + sizeof (* external);
      start += compunit.cu_length + sizeof (external->cu_length);

      if (compunit.cu_version != 2)
	{
	  warn (_("Only version 2 DWARF debug information is currently supported.\n"));
	  continue;
	}

      printf (_("  Compilation Unit:\n"));
      printf (_("   Length:        %ld\n"), compunit.cu_length);
      printf (_("   Version:       %d\n"), compunit.cu_version);
      printf (_("   Abbrev Offset: %ld\n"), compunit.cu_abbrev_offset);
      printf (_("   Pointer Size:  %d\n"), compunit.cu_pointer_size);

      if (first_abbrev != NULL)
	free_abbrevs ();

      /* Read in the abbrevs used by this compilation unit.  */

      {
	Elf32_Internal_Shdr * sec;
	unsigned char *       begin;

	/* Locate the .debug_abbrev section and process it.  */
	for (i = 0, sec = section_headers;
	     i < elf_header.e_shnum;
	     i ++, sec ++)
	  if (strcmp (SECTION_NAME (sec), ".debug_abbrev") == 0)
	    break;

	if (i == -1 || sec->sh_size == 0)
	  {
	    warn (_("Unable to locate .debug_abbrev section!\n"));
	    return 0;
	  }

	GET_DATA_ALLOC (sec->sh_offset, sec->sh_size, begin, unsigned char *,
			"debug_abbrev section data");

	process_abbrev_section (begin + compunit.cu_abbrev_offset,
				begin + sec->sh_size);

	free (begin);
      }

      level = 0;
      while (tags < start)
	{
	  int            bytes_read;
	  unsigned long  abbrev_number;
	  abbrev_entry * entry;
	  abbrev_attr  * attr;

	  abbrev_number = read_leb128 (tags, & bytes_read, 0);
	  tags += bytes_read;

	  /* A null DIE marks the end of a list of children.  */
	  if (abbrev_number == 0)
	    {
	      --level;
	      continue;
	    }

	  /* Scan through the abbreviation list until we reach the
	     correct entry.  */
	  for (entry = first_abbrev;
	       entry && entry->entry != abbrev_number;
	       entry = entry->next)
	    continue;

	  if (entry == NULL)
	    {
	      warn (_("Unable to locate entry %lu in the abbreviation table\n"),
		    abbrev_number);
	      return 0;
	    }

	  printf (_(" <%d><%x>: Abbrev Number: %lu (%s)\n"),
		  level, tags - section_begin - bytes_read,
		  abbrev_number,
		  get_TAG_name (entry->tag));

	  for (attr = entry->first_attr; attr; attr = attr->next)
	    tags = read_and_display_attr (attr->attribute,
					  attr->form,
					  tags,
					  compunit.cu_pointer_size);

	  if (entry->children)
	    ++level;
	}
    }

  printf ("\n");

  return 1;
}

static int
display_debug_aranges (section, start, file)
     Elf32_Internal_Shdr * section;
     unsigned char *       start;
     FILE *                file ATTRIBUTE_UNUSED;
{
  unsigned char * end = start + section->sh_size;

  printf (_("The section %s contains:\n\n"), SECTION_NAME (section));

  while (start < end)
    {
      DWARF2_External_ARange * external;
      DWARF2_Internal_ARange   arange;
      unsigned char *          ranges;
      unsigned long            length;
      unsigned long            address;
      int		       excess;

      external = (DWARF2_External_ARange *) start;

      arange.ar_length       = BYTE_GET (external->ar_length);
      arange.ar_version      = BYTE_GET (external->ar_version);
      arange.ar_info_offset  = BYTE_GET (external->ar_info_offset);
      arange.ar_pointer_size = BYTE_GET (external->ar_pointer_size);
      arange.ar_segment_size = BYTE_GET (external->ar_segment_size);

      printf (_("  Length:                   %ld\n"), arange.ar_length);
      printf (_("  Version:                  %d\n"), arange.ar_version);
      printf (_("  Offset into .debug_info:  %lx\n"), arange.ar_info_offset);
      printf (_("  Pointer Size:             %d\n"), arange.ar_pointer_size);
      printf (_("  Segment Size:             %d\n"), arange.ar_segment_size);

      printf (_("\n    Address  Length\n"));

      ranges = start + sizeof (* external);

      /* Must pad to an alignment boundary that is twice the pointer size.  */
      excess = sizeof (*external) % (2 * arange.ar_pointer_size);
      if (excess)
	ranges += (2 * arange.ar_pointer_size) - excess;

      for (;;)
	{
	  address = byte_get (ranges, arange.ar_pointer_size);

	  ranges += arange.ar_pointer_size;

	  length  = byte_get (ranges, arange.ar_pointer_size);

	  ranges += arange.ar_pointer_size;

	  /* A pair of zeros marks the end of the list.  */
	  if (address == 0 && length == 0)
	    break;

	  printf ("    %8.8lx %lu\n", address, length);
	}

      start += arange.ar_length + sizeof (external->ar_length);
    }

  printf ("\n");

  return 1;
}


static int
display_debug_not_supported (section, start, file)
     Elf32_Internal_Shdr * section;
     unsigned char *       start ATTRIBUTE_UNUSED;
     FILE *                file ATTRIBUTE_UNUSED;
{
  printf (_("Displaying the debug contents of section %s is not yet supported.\n"),
	    SECTION_NAME (section));

  return 1;
}

/* Pre-scan the .debug_info section to record the size of address.
   When dumping the .debug_line, we use that size information, assuming
   that all compilation units have the same address size.  */
static int
prescan_debug_info (section, start, file)
     Elf32_Internal_Shdr * section ATTRIBUTE_UNUSED;
     unsigned char *       start;
     FILE *                file ATTRIBUTE_UNUSED;
{
  DWARF2_External_CompUnit * external;

  external = (DWARF2_External_CompUnit *) start;

  debug_line_pointer_size = BYTE_GET (external->cu_pointer_size);
  return 0;
}

  /* A structure containing the name of a debug section and a pointer
     to a function that can decode it.  The third field is a prescan
     function to be run over the section before displaying any of the
     sections.  */
struct
{
  char * name;
  int (* display) PARAMS ((Elf32_Internal_Shdr *, unsigned char *, FILE *));
  int (* prescan) PARAMS ((Elf32_Internal_Shdr *, unsigned char *, FILE *));
}
debug_displays[] =
{
  { ".debug_info",        display_debug_info, prescan_debug_info },
  { ".debug_abbrev",      display_debug_abbrev, NULL },
  { ".debug_line",        display_debug_lines, NULL },
  { ".debug_aranges",     display_debug_aranges, NULL },
  { ".debug_pubnames",    display_debug_pubnames, NULL },
  { ".debug_macinfo",     display_debug_not_supported, NULL },
  { ".debug_frame",       display_debug_not_supported, NULL },
  { ".debug_str",         display_debug_not_supported, NULL },
  { ".debug_static_func", display_debug_not_supported, NULL },
  { ".debug_static_vars", display_debug_not_supported, NULL },
  { ".debug_types",       display_debug_not_supported, NULL },
  { ".debug_weaknames",   display_debug_not_supported, NULL }
};

static int
display_debug_section (section, file)
     Elf32_Internal_Shdr * section;
     FILE * file;
{
  char *          name = SECTION_NAME (section);
  bfd_size_type   length;
  unsigned char * start;
  int             i;

  length = section->sh_size;
  if (length == 0)
    {
      printf (_("\nSection '%s' has no debugging data.\n"), name);
      return 0;
    }

  GET_DATA_ALLOC (section->sh_offset, length, start, unsigned char *,
		  "debug section data");

  /* See if we know how to display the contents of this section.  */
  for (i = NUM_ELEM (debug_displays); i--;)
    if (strcmp (debug_displays[i].name, name) == 0)
      {
	debug_displays[i].display (section, start, file);
	break;
      }

  if (i == -1)
    printf (_("Unrecognised debug section: %s\n"), name);

  free (start);

  /* If we loaded in the abbrev section at some point,
     we must release it here.  */
  if (first_abbrev != NULL)
    free_abbrevs ();

  return 1;
}

static int
process_section_contents (file)
     FILE * file;
{
  Elf32_Internal_Shdr * section;
  unsigned int  	i;

  if (! do_dump)
    return 1;

  /* Pre-scan the debug sections to find some debug information not
     present in some of them.  For the .debug_line, we must find out the
     size of address (specified in .debug_info and .debug_aranges).  */
  for (i = 0, section = section_headers;
       i < elf_header.e_shnum && i < num_dump_sects;
       i ++, section ++)
    {
      char *	name = SECTION_NAME (section);
      int       j;

      if (section->sh_size == 0)
        continue;

      /* See if there is some pre-scan operation for this section.  */
      for (j = NUM_ELEM (debug_displays); j--;)
        if (strcmp (debug_displays[j].name, name) == 0)
	  {
	    if (debug_displays[j].prescan != NULL)
	      {
		bfd_size_type   length;
		unsigned char * start;

		length = section->sh_size;
		GET_DATA_ALLOC (section->sh_offset, length, start, unsigned char *,
				"debug section data");

		debug_displays[j].prescan (section, start, file);
		free (start);
	      }

            break;
          }
    }

  for (i = 0, section = section_headers;
       i < elf_header.e_shnum && i < num_dump_sects;
       i ++, section ++)
    {
#ifdef SUPPORT_DISASSEMBLY
      if (dump_sects[i] & DISASS_DUMP)
	disassemble_section (section, file);
#endif
      if (dump_sects[i] & HEX_DUMP)
	dump_section (section, file);

      if (dump_sects[i] & DEBUG_DUMP)
	display_debug_section (section, file);
    }

  if (i < num_dump_sects)
    warn (_("Some sections were not dumped because they do not exist!\n"));

  return 1;
}

static void
process_mips_fpe_exception (mask)
     int mask;
{
  if (mask)
    {
      int first = 1;
      if (mask & OEX_FPU_INEX)
	fputs ("INEX", stdout), first = 0;
      if (mask & OEX_FPU_UFLO)
	printf ("%sUFLO", first ? "" : "|"), first = 0;
      if (mask & OEX_FPU_OFLO)
	printf ("%sOFLO", first ? "" : "|"), first = 0;
      if (mask & OEX_FPU_DIV0)
	printf ("%sDIV0", first ? "" : "|"), first = 0;
      if (mask & OEX_FPU_INVAL)
	printf ("%sINVAL", first ? "" : "|");
    }
  else
    fputs ("0", stdout);
}

static int
process_mips_specific (file)
     FILE * file;
{
  Elf_Internal_Dyn * entry;
  size_t liblist_offset = 0;
  size_t liblistno = 0;
  size_t conflictsno = 0;
  size_t options_offset = 0;
  size_t conflicts_offset = 0;

  /* We have a lot of special sections.  Thanks SGI!  */
  if (dynamic_segment == NULL)
    /* No information available.  */
    return 0;

  for (entry = dynamic_segment; entry->d_tag != DT_NULL; ++entry)
    switch (entry->d_tag)
      {
      case DT_MIPS_LIBLIST:
	liblist_offset = entry->d_un.d_val - loadaddr;
	break;
      case DT_MIPS_LIBLISTNO:
	liblistno = entry->d_un.d_val;
	break;
      case DT_MIPS_OPTIONS:
	options_offset = entry->d_un.d_val - loadaddr;
	break;
      case DT_MIPS_CONFLICT:
	conflicts_offset = entry->d_un.d_val - loadaddr;
	break;
      case DT_MIPS_CONFLICTNO:
	conflictsno = entry->d_un.d_val;
	break;
      default:
	break;
      }

  if (liblist_offset != 0 && liblistno != 0 && do_dynamic)
    {
      Elf32_External_Lib * elib;
      size_t cnt;

      GET_DATA_ALLOC (liblist_offset, liblistno * sizeof (Elf32_External_Lib),
		      elib, Elf32_External_Lib *, "liblist");

      printf ("\nSection '.liblist' contains %lu entries:\n",
	      (unsigned long) liblistno);
      fputs ("     Library              Time Stamp          Checksum   Version Flags\n",
	     stdout);

      for (cnt = 0; cnt < liblistno; ++cnt)
	{
	  Elf32_Lib liblist;
	  time_t time;
	  char timebuf[20];
	  struct tm * tmp;

	  liblist.l_name = BYTE_GET (elib[cnt].l_name);
	  time = BYTE_GET (elib[cnt].l_time_stamp);
	  liblist.l_checksum = BYTE_GET (elib[cnt].l_checksum);
	  liblist.l_version = BYTE_GET (elib[cnt].l_version);
	  liblist.l_flags = BYTE_GET (elib[cnt].l_flags);

	  tmp = gmtime (&time);
	  sprintf (timebuf, "%04u-%02u-%02uT%02u:%02u:%02u",
		   tmp->tm_year + 1900, tmp->tm_mon + 1, tmp->tm_mday,
		   tmp->tm_hour, tmp->tm_min, tmp->tm_sec);

	  printf ("%3lu: %-20s %s %#10lx %-7ld", (unsigned long) cnt,
		  dynamic_strings + liblist.l_name, timebuf,
		  liblist.l_checksum, liblist.l_version);

	  if (liblist.l_flags == 0)
	    puts (" NONE");
	  else
	    {
	      static const struct
	      {
		const char * name;
		int bit;
	      }
	      l_flags_vals[] =
	      {
		{ " EXACT_MATCH", LL_EXACT_MATCH },
		{ " IGNORE_INT_VER", LL_IGNORE_INT_VER },
		{ " REQUIRE_MINOR", LL_REQUIRE_MINOR },
		{ " EXPORTS", LL_EXPORTS },
		{ " DELAY_LOAD", LL_DELAY_LOAD },
		{ " DELTA", LL_DELTA }
	      };
	      int flags = liblist.l_flags;
	      size_t fcnt;

	      for (fcnt = 0;
		   fcnt < sizeof (l_flags_vals) / sizeof (l_flags_vals[0]);
		   ++fcnt)
		if ((flags & l_flags_vals[fcnt].bit) != 0)
		  {
		    fputs (l_flags_vals[fcnt].name, stdout);
		    flags ^= l_flags_vals[fcnt].bit;
		  }
	      if (flags != 0)
		printf (" %#x", (unsigned int) flags);

	      puts ("");
	    }
	}

      free (elib);
    }

  if (options_offset != 0)
    {
      Elf_External_Options * eopt;
      Elf_Internal_Shdr *    sect = section_headers;
      Elf_Internal_Options * iopt;
      Elf_Internal_Options * option;
      size_t offset;
      int cnt;

      /* Find the section header so that we get the size.  */
      while (sect->sh_type != SHT_MIPS_OPTIONS)
	++ sect;

      GET_DATA_ALLOC (options_offset, sect->sh_size, eopt,
		      Elf_External_Options *, "options");

      iopt = (Elf_Internal_Options *) malloc ((sect->sh_size / sizeof (eopt))
					      * sizeof (*iopt));
      if (iopt == NULL)
	{
	  error (_("Out of memory"));
	  return 0;
	}

      offset = cnt = 0;
      option = iopt;
      
      while (offset < sect->sh_size)
	{
	  Elf_External_Options * eoption;

	  eoption = (Elf_External_Options *) ((char *) eopt + offset);

	  option->kind = BYTE_GET (eoption->kind);
	  option->size = BYTE_GET (eoption->size);
	  option->section = BYTE_GET (eoption->section);
	  option->info = BYTE_GET (eoption->info);

	  offset += option->size;
	  
	  ++option;
	  ++cnt;
	}

      printf (_("\nSection '%s' contains %d entries:\n"),
	      string_table + sect->sh_name, cnt);

      option = iopt;
      
      while (cnt-- > 0)
	{
	  size_t len;

	  switch (option->kind)
	    {
	    case ODK_NULL:
	      /* This shouldn't happen.  */
	      printf (" NULL       %d %lx", option->section, option->info);
	      break;
	    case ODK_REGINFO:
	      printf (" REGINFO    ");
	      if (elf_header.e_machine == EM_MIPS)
		{
		  /* 32bit form.  */
		  Elf32_External_RegInfo *ereg;
		  Elf32_RegInfo reginfo;

		  ereg = (Elf32_External_RegInfo *) (option + 1);
		  reginfo.ri_gprmask = BYTE_GET (ereg->ri_gprmask);
		  reginfo.ri_cprmask[0] = BYTE_GET (ereg->ri_cprmask[0]);
		  reginfo.ri_cprmask[1] = BYTE_GET (ereg->ri_cprmask[1]);
		  reginfo.ri_cprmask[2] = BYTE_GET (ereg->ri_cprmask[2]);
		  reginfo.ri_cprmask[3] = BYTE_GET (ereg->ri_cprmask[3]);
		  reginfo.ri_gp_value = BYTE_GET (ereg->ri_gp_value);

		  printf ("GPR %08lx  GP 0x%lx\n",
			  reginfo.ri_gprmask,
			  (unsigned long) reginfo.ri_gp_value);
		  printf ("            CPR0 %08lx  CPR1 %08lx  CPR2 %08lx  CPR3 %08lx\n",
			  reginfo.ri_cprmask[0], reginfo.ri_cprmask[1],
			  reginfo.ri_cprmask[2], reginfo.ri_cprmask[3]);
		}
	      else
		{
		  /* 64 bit form.  */
		  Elf64_External_RegInfo * ereg;
		  Elf64_Internal_RegInfo reginfo;

		  ereg = (Elf64_External_RegInfo *) (option + 1);
		  reginfo.ri_gprmask    = BYTE_GET (ereg->ri_gprmask);
		  reginfo.ri_cprmask[0] = BYTE_GET (ereg->ri_cprmask[0]);
		  reginfo.ri_cprmask[1] = BYTE_GET (ereg->ri_cprmask[1]);
		  reginfo.ri_cprmask[2] = BYTE_GET (ereg->ri_cprmask[2]);
		  reginfo.ri_cprmask[3] = BYTE_GET (ereg->ri_cprmask[3]);
		  reginfo.ri_gp_value   = BYTE_GET8 (ereg->ri_gp_value);

		  printf ("GPR %08lx  GP 0x",
			  reginfo.ri_gprmask);
		  printf_vma (reginfo.ri_gp_value);
		  printf ("\n");

		  printf ("            CPR0 %08lx  CPR1 %08lx  CPR2 %08lx  CPR3 %08lx\n",
			  reginfo.ri_cprmask[0], reginfo.ri_cprmask[1],
			  reginfo.ri_cprmask[2], reginfo.ri_cprmask[3]);
		}
	      ++option;
	      continue;
	    case ODK_EXCEPTIONS:
	      fputs (" EXCEPTIONS fpe_min(", stdout);
	      process_mips_fpe_exception (option->info & OEX_FPU_MIN);
	      fputs (") fpe_max(", stdout);
	      process_mips_fpe_exception ((option->info & OEX_FPU_MAX) >> 8);
	      fputs (")", stdout);

	      if (option->info & OEX_PAGE0)
		fputs (" PAGE0", stdout);
	      if (option->info & OEX_SMM)
		fputs (" SMM", stdout);
	      if (option->info & OEX_FPDBUG)
		fputs (" FPDBUG", stdout);
	      if (option->info & OEX_DISMISS)
		fputs (" DISMISS", stdout);
	      break;
	    case ODK_PAD:
	      fputs (" PAD       ", stdout);
	      if (option->info & OPAD_PREFIX)
		fputs (" PREFIX", stdout);
	      if (option->info & OPAD_POSTFIX)
		fputs (" POSTFIX", stdout);
	      if (option->info & OPAD_SYMBOL)
		fputs (" SYMBOL", stdout);
	      break;
	    case ODK_HWPATCH:
	      fputs (" HWPATCH   ", stdout);
	      if (option->info & OHW_R4KEOP)
		fputs (" R4KEOP", stdout);
	      if (option->info & OHW_R8KPFETCH)
		fputs (" R8KPFETCH", stdout);
	      if (option->info & OHW_R5KEOP)
		fputs (" R5KEOP", stdout);
	      if (option->info & OHW_R5KCVTL)
		fputs (" R5KCVTL", stdout);
	      break;
	    case ODK_FILL:
	      fputs (" FILL       ", stdout);
	      /* XXX Print content of info word?  */
	      break;
	    case ODK_TAGS:
	      fputs (" TAGS       ", stdout);
	      /* XXX Print content of info word?  */
	      break;
	    case ODK_HWAND:
	      fputs (" HWAND     ", stdout);
	      if (option->info & OHWA0_R4KEOP_CHECKED)
		fputs (" R4KEOP_CHECKED", stdout);
	      if (option->info & OHWA0_R4KEOP_CLEAN)
		fputs (" R4KEOP_CLEAN", stdout);
	      break;
	    case ODK_HWOR:
	      fputs (" HWOR      ", stdout);
	      if (option->info & OHWA0_R4KEOP_CHECKED)
		fputs (" R4KEOP_CHECKED", stdout);
	      if (option->info & OHWA0_R4KEOP_CLEAN)
		fputs (" R4KEOP_CLEAN", stdout);
	      break;
	    case ODK_GP_GROUP:
	      printf (" GP_GROUP  %#06lx  self-contained %#06lx",
		      option->info & OGP_GROUP,
		      (option->info & OGP_SELF) >> 16);
	      break;
	    case ODK_IDENT:
	      printf (" IDENT     %#06lx  self-contained %#06lx",
		      option->info & OGP_GROUP,
		      (option->info & OGP_SELF) >> 16);
	      break;
	    default:
	      /* This shouldn't happen.  */
	      printf (" %3d ???     %d %lx",
		      option->kind, option->section, option->info);
	      break;
	    }

	  len = sizeof (*eopt);
	  while (len < option->size)
	    if (((char *) option)[len] >= ' '
		&& ((char *) option)[len] < 0x7f)
	      printf ("%c", ((char *) option)[len++]);
	    else
	      printf ("\\%03o", ((char *) option)[len++]);

	  fputs ("\n", stdout);
	  ++option;
	}

      free (eopt);
    }

  if (conflicts_offset != 0 && conflictsno != 0)
    {
      Elf32_External_Conflict * econf32;
      Elf64_External_Conflict * econf64;
      Elf32_Conflict * iconf;
      size_t cnt;

      if (dynamic_symbols == NULL)
	{
	  error (_("conflict list with without table"));
	  return 0;
	}

      iconf = (Elf32_Conflict *) malloc (conflictsno * sizeof (*iconf));
      if (iconf == NULL)
	{
	  error (_("Out of memory"));
	  return 0;
	}

      if (is_32bit_elf)
	{
	  GET_DATA_ALLOC (conflicts_offset, conflictsno * sizeof (*econf32),
			  econf32, Elf32_External_Conflict *, "conflict");

	  for (cnt = 0; cnt < conflictsno; ++cnt)
	    iconf[cnt] = BYTE_GET (econf32[cnt]);
	}
      else
	{
	  GET_DATA_ALLOC (conflicts_offset, conflictsno * sizeof (*econf64),
			  econf64, Elf64_External_Conflict *, "conflict");

	  for (cnt = 0; cnt < conflictsno; ++cnt)
	    iconf[cnt] = BYTE_GET (econf64[cnt]);
	}

      printf (_("\nSection '.conflict' contains %d entries:\n"), conflictsno);
      puts (_("  Num:    Index       Value  Name"));

      for (cnt = 0; cnt < conflictsno; ++cnt)
	{
	  Elf_Internal_Sym * psym = &dynamic_symbols[iconf[cnt]];

	  printf ("%5lu: %8lu  ", (unsigned long) cnt, iconf[cnt]);
	  print_vma (psym->st_value, FULL_HEX);
	  printf ("  %s\n", dynamic_strings + psym->st_name);
	}

      free (iconf);
    }

  return 1;
}

static char *
get_note_type (e_type)
     unsigned e_type;
{
  static char buff[64];

  switch (e_type)
    {
    case NT_PRSTATUS:	return _("NT_PRSTATUS (prstatus structure)");
    case NT_FPREGSET:	return _("NT_FPREGSET (floating point registers)");
    case NT_PRPSINFO:   return _("NT_PRPSINFO (prpsinfo structure)");
    case NT_TASKSTRUCT: return _("NT_TASKSTRUCT (task structure)");
    case NT_PRXFPREG:   return _("NT_PRXFPREG (user_xfpregs structure)");
    case NT_PSTATUS:	return _("NT_PSTATUS (pstatus structure)");
    case NT_FPREGS:	return _("NT_FPREGS (floating point registers)");
    case NT_PSINFO:	return _("NT_PSINFO (psinfo structure)");
    case NT_LWPSTATUS:	return _("NT_LWPSTATUS (lwpstatus_t structure)");
    case NT_LWPSINFO:	return _("NT_LWPSINFO (lwpsinfo_t structure)");
    case NT_WIN32PSTATUS: return _("NT_WIN32PSTATUS (win32_pstatus strcuture)");
    default:
      sprintf (buff, _("Unknown note type: (0x%08x)"), e_type);
      return buff;
    }
}

/* Note that by the ELF standard, the name field is already null byte
   terminated, and namesz includes the terminating null byte.
   I.E. the value of namesz for the name "FSF" is 4.

   If the value of namesz is zero, there is no name present. */
static int
process_note (pnote)
  Elf32_Internal_Note * pnote;
{
  printf ("  %s\t\t0x%08lx\t%s\n",
	  pnote->namesz ? pnote->namedata : "(NONE)",
  	  pnote->descsz, get_note_type (pnote->type));
  return 1;
}


static int
process_corefile_note_segment (file, offset, length)
     FILE * file;
     bfd_vma offset;
     bfd_vma length;
{
  Elf_External_Note *  pnotes;
  Elf_External_Note *  external;
  int                  res = 1;

  if (length <= 0)
    return 0;

  GET_DATA_ALLOC (offset, length, pnotes, Elf_External_Note *, "notes");

  external = pnotes;

  printf (_("\nNotes at offset 0x%08lx with length 0x%08lx:\n"),
	  (unsigned long) offset,
	  (unsigned long) length);
  printf (_("  Owner\t\tData size\tDescription\n"));

  while (external < (Elf_External_Note *)((char *) pnotes + length))
    {
      Elf32_Internal_Note inote;
      char * temp = NULL;

      inote.type     = BYTE_GET (external->type);
      inote.namesz   = BYTE_GET (external->namesz);
      inote.namedata = external->name;
      inote.descsz   = BYTE_GET (external->descsz);
      inote.descdata = inote.namedata + align_power (inote.namesz, 2);
      inote.descpos  = offset + (inote.descdata - (char *) pnotes);
      
      external = (Elf_External_Note *)(inote.descdata + align_power (inote.descsz, 2));

      /* Verify that name is null terminated.  It appears that at least
	 one version of Linux (RedHat 6.0) generates corefiles that don't
	 comply with the ELF spec by failing to include the null byte in
	 namesz.  */
      if (inote.namedata[inote.namesz] != '\0')
	{
	  temp = malloc (inote.namesz + 1);
	  
	  if (temp == NULL)
	    {
	      error (_("Out of memory\n"));
	      res = 0;
	      break;
	    }
	  
	  strncpy (temp, inote.namedata, inote.namesz);
	  temp[inote.namesz] = 0;
	  
	  /* warn (_("'%s' NOTE name not properly null terminated\n"), temp);  */
	  inote.namedata = temp;
	}

      res &= process_note (& inote);

      if (temp != NULL)
	{
	  free (temp);
	  temp = NULL;
	}
    }

  free (pnotes);

  return res;
}

static int
process_corefile_note_segments (file)
     FILE * file;
{
  Elf_Internal_Phdr * program_headers;
  Elf_Internal_Phdr * segment;
  unsigned int	      i;
  int                 res = 1;

  program_headers = (Elf_Internal_Phdr *) malloc
    (elf_header.e_phnum * sizeof (Elf_Internal_Phdr));

  if (program_headers == NULL)
    {
      error (_("Out of memory\n"));
      return 0;
    }

  if (is_32bit_elf)
    i = get_32bit_program_headers (file, program_headers);
  else
    i = get_64bit_program_headers (file, program_headers);

  if (i == 0)
    {
      free (program_headers);
      return 0;
    }

  for (i = 0, segment = program_headers;
       i < elf_header.e_phnum;
       i ++, segment ++)
    {
      if (segment->p_type == PT_NOTE)
	res &= process_corefile_note_segment (file,
					      (bfd_vma) segment->p_offset,
					      (bfd_vma) segment->p_filesz);
    }

  free (program_headers);

  return res;
}

static int
process_corefile_contents (file)
     FILE * file;
{
  /* If we have not been asked to display the notes then do nothing.  */
  if (! do_notes)
    return 1;

  /* If file is not a core file then exit.  */
  if (elf_header.e_type != ET_CORE)
    return 1;

  /* No program headers means no NOTE segment.  */
  if (elf_header.e_phnum == 0)
    {
      printf (_("No note segments present in the core file.\n"));
      return 1;
   }

  return process_corefile_note_segments (file);
}

static int
process_arch_specific (file)
     FILE * file;
{
  if (! do_arch)
    return 1;

  switch (elf_header.e_machine)
    {
    case EM_MIPS:
    case EM_MIPS_RS4_BE:
      return process_mips_specific (file);
      break;
    default:
      break;
    }
  return 1;
}

static int
get_file_header (file)
     FILE * file;
{
  /* Read in the identity array.  */
  if (fread (elf_header.e_ident, EI_NIDENT, 1, file) != 1)
    return 0;

  /* Determine how to read the rest of the header.  */
  switch (elf_header.e_ident [EI_DATA])
    {
    default: /* fall through */
    case ELFDATANONE: /* fall through */
    case ELFDATA2LSB: byte_get = byte_get_little_endian; break;
    case ELFDATA2MSB: byte_get = byte_get_big_endian; break;
    }

  /* For now we only support 32 bit and 64 bit ELF files.  */
  is_32bit_elf = (elf_header.e_ident [EI_CLASS] != ELFCLASS64);

  /* Read in the rest of the header.  */
  if (is_32bit_elf)
    {
      Elf32_External_Ehdr ehdr32;

      if (fread (ehdr32.e_type, sizeof (ehdr32) - EI_NIDENT, 1, file) != 1)
	return 0;

      elf_header.e_type      = BYTE_GET (ehdr32.e_type);
      elf_header.e_machine   = BYTE_GET (ehdr32.e_machine);
      elf_header.e_version   = BYTE_GET (ehdr32.e_version);
      elf_header.e_entry     = BYTE_GET (ehdr32.e_entry);
      elf_header.e_phoff     = BYTE_GET (ehdr32.e_phoff);
      elf_header.e_shoff     = BYTE_GET (ehdr32.e_shoff);
      elf_header.e_flags     = BYTE_GET (ehdr32.e_flags);
      elf_header.e_ehsize    = BYTE_GET (ehdr32.e_ehsize);
      elf_header.e_phentsize = BYTE_GET (ehdr32.e_phentsize);
      elf_header.e_phnum     = BYTE_GET (ehdr32.e_phnum);
      elf_header.e_shentsize = BYTE_GET (ehdr32.e_shentsize);
      elf_header.e_shnum     = BYTE_GET (ehdr32.e_shnum);
      elf_header.e_shstrndx  = BYTE_GET (ehdr32.e_shstrndx);
    }
  else
    {
      Elf64_External_Ehdr ehdr64;

      /* If we have been compiled with sizeof (bfd_vma) == 4, then
	 we will not be able to cope with the 64bit data found in
	 64 ELF files.  Detect this now and abort before we start
	 overwritting things.  */
      if (sizeof (bfd_vma) < 8)
	{
	  error (_("This instance of readelf has been built without support for a\n"));
	  error (_("64 bit data type and so it cannot read 64 bit ELF files.\n"));
	  return 0;
	}

      if (fread (ehdr64.e_type, sizeof (ehdr64) - EI_NIDENT, 1, file) != 1)
	return 0;

      elf_header.e_type      = BYTE_GET (ehdr64.e_type);
      elf_header.e_machine   = BYTE_GET (ehdr64.e_machine);
      elf_header.e_version   = BYTE_GET (ehdr64.e_version);
      elf_header.e_entry     = BYTE_GET8 (ehdr64.e_entry);
      elf_header.e_phoff     = BYTE_GET8 (ehdr64.e_phoff);
      elf_header.e_shoff     = BYTE_GET8 (ehdr64.e_shoff);
      elf_header.e_flags     = BYTE_GET (ehdr64.e_flags);
      elf_header.e_ehsize    = BYTE_GET (ehdr64.e_ehsize);
      elf_header.e_phentsize = BYTE_GET (ehdr64.e_phentsize);
      elf_header.e_phnum     = BYTE_GET (ehdr64.e_phnum);
      elf_header.e_shentsize = BYTE_GET (ehdr64.e_shentsize);
      elf_header.e_shnum     = BYTE_GET (ehdr64.e_shnum);
      elf_header.e_shstrndx  = BYTE_GET (ehdr64.e_shstrndx);
    }

  return 1;
}

static void
process_file (file_name)
     char * file_name;
{
  FILE *       file;
  struct stat  statbuf;
  unsigned int i;

  if (stat (file_name, & statbuf) < 0)
    {
      error (_("Cannot stat input file %s.\n"), file_name);
      return;
    }

  file = fopen (file_name, "rb");
  if (file == NULL)
    {
      error (_("Input file %s not found.\n"), file_name);
      return;
    }

  if (! get_file_header (file))
    {
      error (_("%s: Failed to read file header\n"), file_name);
      fclose (file);
      return;
    }

  /* Initialise per file variables.  */
  for (i = NUM_ELEM (version_info); i--;)
    version_info[i] = 0;

  for (i = NUM_ELEM (dynamic_info); i--;)
    dynamic_info[i] = 0;

  /* Process the file.  */
  if (show_name)
    printf (_("\nFile: %s\n"), file_name);

  if (! process_file_header ())
    {
      fclose (file);
      return;
    }

  process_section_headers (file);

  process_program_headers (file);

  process_dynamic_segment (file);

  process_relocs (file);

  process_symbol_table (file);

  process_syminfo (file);

  process_version_sections (file);

  process_section_contents (file);

  process_corefile_contents (file);

  process_arch_specific (file);

  fclose (file);

  if (section_headers)
    {
      free (section_headers);
      section_headers = NULL;
    }

  if (string_table)
    {
      free (string_table);
      string_table = NULL;
    }

  if (dynamic_strings)
    {
      free (dynamic_strings);
      dynamic_strings = NULL;
    }

  if (dynamic_symbols)
    {
      free (dynamic_symbols);
      dynamic_symbols = NULL;
      num_dynamic_syms = 0;
    }

  if (dynamic_syminfo)
    {
      free (dynamic_syminfo);
      dynamic_syminfo = NULL;
    }
}

#ifdef SUPPORT_DISASSEMBLY
/* Needed by the i386 disassembler.  For extra credit, someone could
   fix this so that we insert symbolic addresses here, esp for GOT/PLT
   symbols */

void
print_address (unsigned int addr, FILE * outfile)
{
  fprintf (outfile,"0x%8.8x", addr);
}

/* Needed by the i386 disassembler. */
void
db_task_printsym (unsigned int addr)
{
  print_address (addr, stderr);
}
#endif

int
main (argc, argv)
     int     argc;
     char ** argv;
{
#if defined (HAVE_SETLOCALE) && defined (HAVE_LC_MESSAGES)
  setlocale (LC_MESSAGES, "");
#endif
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  parse_args (argc, argv);

  if (optind < (argc - 1))
    show_name = 1;

  while (optind < argc)
    process_file (argv [optind ++]);

  if (dump_sects != NULL)
    free (dump_sects);

  return 0;
}
