/* ld.h -- general linker header file
   Copyright (C) 1991, 93, 94, 95, 96, 97, 98, 99, 2000
   Free Software Foundation, Inc.

   This file is part of GLD, the Gnu Linker.

   GLD is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GLD is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GLD; see the file COPYING.  If not, write to the Free
   Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

#ifndef LD_H
#define LD_H

#ifdef HAVE_LOCALE_H
# include <locale.h>
#endif

#ifdef ENABLE_NLS
# include <libintl.h>
# define _(String) gettext (String)
# ifdef gettext_noop
#  define N_(String) gettext_noop (String)
# else
#  define N_(String) (String)
# endif
#else
# define gettext(Msgid) (Msgid)
# define dgettext(Domainname, Msgid) (Msgid)
# define dcgettext(Domainname, Msgid, Category) (Msgid)
# define textdomain(Domainname) while (0) /* nothing */
# define bindtextdomain(Domainname, Dirname) while (0) /* nothing */
# define _(String) (String)
# define N_(String) (String)
#endif

#include "bin-bugs.h"

/* Look in this environment name for the linker to pretend to be */
#define EMULATION_ENVIRON "LDEMULATION"
/* If in there look for the strings: */

/* Look in this variable for a target format */
#define TARGET_ENVIRON "GNUTARGET"

/* Input sections which are put in a section of this name are actually
   discarded.  */
#define DISCARD_SECTION_NAME "/DISCARD/"

/* A file name list */
typedef struct name_list {
  const char *name;
  struct name_list *next;
}
name_list;

/* A wildcard specification.  This is only used in ldgram.y, but it
   winds up in ldgram.h, so we need to define it outside.  */

struct wildcard_spec {
  const char *name;
  struct name_list *exclude_name_list;
  boolean sorted;
};

/* Extra information we hold on sections */
typedef struct user_section_struct {
  /* Pointer to the section where this data will go */
  struct lang_input_statement_struct *file;
} section_userdata_type;

#define get_userdata(x) ((x)->userdata)

#define BYTE_SIZE	(1)
#define SHORT_SIZE	(2)
#define LONG_SIZE	(4)
#define QUAD_SIZE	(8)

/* ALIGN macro changed to ALIGN_N to avoid	*/
/* conflict in /usr/include/machine/machparam.h */
/* WARNING: If THIS is a 64 bit address and BOUNDARY is a 32 bit int,
   you must coerce boundary to the same type as THIS.
   ??? Is there a portable way to avoid this.  */
#define ALIGN_N(this, boundary) \
  ((( (this) + ((boundary) -1)) & (~((boundary)-1))))

typedef struct {
  /* 1 => assign space to common symbols even if `relocatable_output'.  */
  boolean force_common_definition;
  boolean relax;

  /* Name of runtime interpreter to invoke.  */
  char *interpreter;

  /* Name to give runtime libary from the -soname argument.  */
  char *soname;

  /* Runtime library search path from the -rpath argument.  */
  char *rpath;

  /* Link time runtime library search path from the -rpath-link
     argument.  */
  char *rpath_link;

  /* Big or little endian as set on command line.  */
  enum { ENDIAN_UNSET = 0, ENDIAN_BIG, ENDIAN_LITTLE } endian;

  /* If true, export all symbols in the dynamic symbol table of an ELF
     executable.  */
  boolean export_dynamic;

  /* If true, build MIPS embedded PIC relocation tables in the output
     file.  */
  boolean embedded_relocs;

  /* If true, force generation of a file with a .exe file.  */
  boolean force_exe_suffix;

  /* If true, generate a cross reference report.  */
  boolean cref;

  /* If true (which is the default), warn about mismatched input
     files.  */
  boolean warn_mismatch;

  /* Remove unreferenced sections?  */
  boolean gc_sections;

  /* Name of shared object whose symbol table should be filtered with
     this shared object.  From the --filter option.  */
  char *filter_shlib;

  /* Name of shared object for whose symbol table this shared object
     is an auxiliary filter.  From the --auxiliary option.  */
  char **auxiliary_filters;

  /* A version symbol to be applied to the symbol names found in the
     .exports sections.  */
  char *version_exports_section;

  /* If true (the default) check section addresses, once compute,
     fpor overlaps.  */
  boolean check_section_addresses;

} args_type;

extern args_type command_line;

typedef int token_code_type;

typedef struct {
  bfd_size_type specified_data_size;
  boolean magic_demand_paged;
  boolean make_executable;

  /* If true, doing a dynamic link.  */
  boolean dynamic_link;

  /* If true, -shared is supported.  */
  /* ??? A better way to do this is perhaps to define this in the
     ld_emulation_xfer_struct since this is really a target dependent
     parameter.  */
  boolean has_shared;

  /* If true, build constructors.  */
  boolean build_constructors;

  /* If true, warn about any constructors.  */
  boolean warn_constructors;

  /* If true, warn about merging common symbols with others.  */
  boolean warn_common;

  /* If true, only warn once about a particular undefined symbol.  */
  boolean warn_once;

  /* If true, warn if multiple global-pointers are needed (Alpha
     only).  */
  boolean warn_multiple_gp;

  /* If true, warn if the starting address of an output section
     changes due to the alignment of an input section.  */
  boolean warn_section_align;

  boolean sort_common;

  boolean text_read_only;

  char *map_filename;
  FILE *map_file;

  boolean stats;

  /* If set, orphan input sections will be mapped to separate output
     sections.  */
  boolean unique_orphan_sections;

  unsigned int split_by_reloc;
  bfd_size_type split_by_file;
} ld_config_type;

extern ld_config_type config;

typedef enum {
  lang_first_phase_enum,
  lang_allocating_phase_enum,
  lang_final_phase_enum
} lang_phase_type;

extern boolean had_script;
extern boolean force_make_executable;

/* Non-zero if we are processing a --defsym from the command line.  */
extern int parsing_defsym;

extern int yyparse PARAMS ((void));

extern void add_cref PARAMS ((const char *, bfd *, asection *, bfd_vma));
extern void output_cref PARAMS ((FILE *));
extern void check_nocrossrefs PARAMS ((void));

extern void ld_abort PARAMS ((const char *, int, const char *))
     ATTRIBUTE_NORETURN;

/* If gcc >= 2.6, we can give a function name, too.  */
#if __GNUC__ < 2 || (__GNUC__ == 2 && __GNUC_MINOR__ < 6)
#define __PRETTY_FUNCTION__  ((char*) NULL)
#endif

#undef abort
#define abort() ld_abort (__FILE__, __LINE__, __PRETTY_FUNCTION__)

#endif
