/* objdump.c -- dump information about an object file.
   Copyright 1990, 91, 92, 93, 94, 95, 96, 97, 98, 99, 2000
   Free Software Foundation, Inc.

This file is part of GNU Binutils.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "bfd.h"
#include "getopt.h"
#include "progress.h"
#include "bucomm.h"
#include <ctype.h>
#include "dis-asm.h"
#include "libiberty.h"
#include "demangle.h"
#include "debug.h"
#include "budbg.h"

/* Internal headers for the ELF .stab-dump code - sorry.  */
#define	BYTES_IN_WORD	32
#include "aout/aout64.h"

#ifdef NEED_DECLARATION_FPRINTF
/* This is needed by INIT_DISASSEMBLE_INFO.  */
extern int fprintf PARAMS ((FILE *, const char *, ...));
#endif

/* Exit status.  */
static int exit_status = 0;

static char *default_target = NULL;	/* default at runtime */

static int show_version = 0;		/* show the version number */
static int dump_section_contents;	/* -s */
static int dump_section_headers;	/* -h */
static boolean dump_file_header;	/* -f */
static int dump_symtab;			/* -t */
static int dump_dynamic_symtab;		/* -T */
static int dump_reloc_info;		/* -r */
static int dump_dynamic_reloc_info;	/* -R */
static int dump_ar_hdrs;		/* -a */
static int dump_private_headers;	/* -p */
static int prefix_addresses;		/* --prefix-addresses */
static int with_line_numbers;		/* -l */
static boolean with_source_code;	/* -S */
static int show_raw_insn;		/* --show-raw-insn */
static int dump_stab_section_info;	/* --stabs */
static int do_demangle;			/* -C, --demangle */
static boolean disassemble;		/* -d */
static boolean disassemble_all;		/* -D */
static int disassemble_zeroes;		/* --disassemble-zeroes */
static boolean formats_info;		/* -i */
static char *only;			/* -j secname */
static int wide_output;			/* -w */
static bfd_vma start_address = (bfd_vma) -1; /* --start-address */
static bfd_vma stop_address = (bfd_vma) -1;  /* --stop-address */
static int dump_debugging;		/* --debugging */
static bfd_vma adjust_section_vma = 0;	/* --adjust-vma */
static int file_start_context = 0;      /* --file-start-context */

/* Extra info to pass to the disassembler address printing function.  */
struct objdump_disasm_info {
  bfd *abfd;
  asection *sec;
  boolean require_sec;
};

/* Architecture to disassemble for, or default if NULL.  */
static char *machine = (char *) NULL;

/* Target specific options to the disassembler.  */
static char *disassembler_options = (char *) NULL;

/* Endianness to disassemble for, or default if BFD_ENDIAN_UNKNOWN.  */
static enum bfd_endian endian = BFD_ENDIAN_UNKNOWN;

/* The symbol table.  */
static asymbol **syms;

/* Number of symbols in `syms'.  */
static long symcount = 0;

/* The sorted symbol table.  */
static asymbol **sorted_syms;

/* Number of symbols in `sorted_syms'.  */
static long sorted_symcount = 0;

/* The dynamic symbol table.  */
static asymbol **dynsyms;

/* Number of symbols in `dynsyms'.  */
static long dynsymcount = 0;

/* Static declarations.  */

static void
usage PARAMS ((FILE *, int));

static void
nonfatal PARAMS ((const char *));

static void
display_file PARAMS ((char *filename, char *target));

static void
dump_section_header PARAMS ((bfd *, asection *, PTR));

static void
dump_headers PARAMS ((bfd *));

static void
dump_data PARAMS ((bfd *abfd));

static void
dump_relocs PARAMS ((bfd *abfd));

static void
dump_dynamic_relocs PARAMS ((bfd * abfd));

static void
dump_reloc_set PARAMS ((bfd *, asection *, arelent **, long));

static void
dump_symbols PARAMS ((bfd *abfd, boolean dynamic));

static void
dump_bfd_header PARAMS ((bfd *));

static void
dump_bfd_private_header PARAMS ((bfd *));

static void
display_bfd PARAMS ((bfd *abfd));

static void
display_target_list PARAMS ((void));

static void
display_info_table PARAMS ((int, int));

static void
display_target_tables PARAMS ((void));

static void
display_info PARAMS ((void));

static void
objdump_print_value PARAMS ((bfd_vma, struct disassemble_info *, boolean));

static void
objdump_print_symname PARAMS ((bfd *, struct disassemble_info *, asymbol *));

static asymbol *
find_symbol_for_address PARAMS ((bfd *, asection *, bfd_vma, boolean, long *));

static void
objdump_print_addr_with_sym PARAMS ((bfd *, asection *, asymbol *, bfd_vma,
				     struct disassemble_info *, boolean));

static void
objdump_print_addr PARAMS ((bfd_vma, struct disassemble_info *, boolean));

static void
objdump_print_address PARAMS ((bfd_vma, struct disassemble_info *));

static void
show_line PARAMS ((bfd *, asection *, bfd_vma));

static void
disassemble_bytes PARAMS ((struct disassemble_info *, disassembler_ftype,
			   boolean, bfd_byte *, bfd_vma, bfd_vma,
			   arelent ***, arelent **));

static void
disassemble_data PARAMS ((bfd *));

static const char *
endian_string PARAMS ((enum bfd_endian));

static asymbol **
slurp_symtab PARAMS ((bfd *));

static asymbol **
slurp_dynamic_symtab PARAMS ((bfd *));

static long
remove_useless_symbols PARAMS ((asymbol **, long));

static int
compare_symbols PARAMS ((const PTR, const PTR));

static int
compare_relocs PARAMS ((const PTR, const PTR));

static void
dump_stabs PARAMS ((bfd *));

static boolean
read_section_stabs PARAMS ((bfd *, const char *, const char *));

static void
print_section_stabs PARAMS ((bfd *, const char *, const char *));

static void
usage (stream, status)
     FILE *stream;
     int status;
{
  fprintf (stream, _("Usage: %s OPTION... FILE...\n"), program_name);
  fprintf (stream, _("Display information from object FILE.\n"));
  fprintf (stream, _("\n At least one of the following switches must be given:\n"));
  fprintf (stream, _("\
  -a, --archive-headers    Display archive header information\n\
  -f, --file-headers       Display the contents of the overall file header\n\
  -p, --private-headers    Display object format specific file header contents\n\
  -h, --[section-]headers  Display the contents of the section headers\n\
  -x, --all-headers        Display the contents of all headers\n\
  -d, --disassemble        Display assembler contents of executable sections\n\
  -D, --disassemble-all    Display assembler contents of all sections\n\
  -S, --source             Intermix source code with disassembly\n\
  -s, --full-contents      Display the full contents of all sections requested\n\
  -g, --debugging          Display debug information in object file\n\
  -G, --stabs              Display (in raw form) any STABS info in the file\n\
  -t, --syms               Display the contents of the symbol table(s)\n\
  -T, --dynamic-syms       Display the contents of the dynamic symbol table\n\
  -r, --reloc              Display the relocation entries in the file\n\
  -R, --dynamic-reloc      Display the dynamic relocation entries in the file\n\
  -V, --version            Display this program's version number\n\
  -i, --info               List object formats and architectures supported\n\
  -H, --help               Display this information\n\
"));
  if (status != 2)
    {
      fprintf (stream, _("\n The following switches are optional:\n"));
      fprintf (stream, _("\
  -b, --target=BFDNAME           Specify the target object format as BFDNAME\n\
  -m, --architecture=MACHINE     Specify the target architecture as MACHINE\n\
  -j, --section=NAME             Only display information for section NAME\n\
  -M, --disassembler-options=OPT Pass text OPT on to the disassembler\n\
  -EB --endian=big               Assume big endian format when disassembling\n\
  -EL --endian=little            Assume little endian format when disassembling\n\
      --file-start-context       Include context from start of file (with -S)\n\
  -l, --line-numbers             Include line numbers and filenames in output\n\
  -C, --demangle[=STYLE]         Decode mangled/processed symbol names\n\
                                  The STYLE, if specified, can be `auto', 'gnu',\n\
                                  'lucid', 'arm', 'hp', 'edg', or 'gnu-new-abi'\n\
  -w, --wide                     Format output for more than 80 columns\n\
  -z, --disassemble-zeroes       Do not skip blocks of zeroes when disassembling\n\
      --start-address=ADDR       Only process data whoes address is >= ADDR\n\
      --stop-address=ADDR        Only process data whoes address is <= ADDR\n\
      --prefix-addresses         Print complete address alongside disassembly\n\
      --[no-]show-raw-insn       Display hex alongside symbolic disassembly\n\
      --adjust-vma=OFFSET        Add OFFSET to all displayed section addresses\n\
\n"));
      list_supported_targets (program_name, stream);

      disassembler_usage (stream);
    }
  if (status == 0)
    fprintf (stream, _("Report bugs to %s.\n"), REPORT_BUGS_TO);
  exit (status);
}

/* 150 isn't special; it's just an arbitrary non-ASCII char value.  */

#define OPTION_ENDIAN (150)
#define OPTION_START_ADDRESS (OPTION_ENDIAN + 1)
#define OPTION_STOP_ADDRESS (OPTION_START_ADDRESS + 1)
#define OPTION_ADJUST_VMA (OPTION_STOP_ADDRESS + 1)

static struct option long_options[]=
{
  {"adjust-vma", required_argument, NULL, OPTION_ADJUST_VMA},
  {"all-headers", no_argument, NULL, 'x'},
  {"private-headers", no_argument, NULL, 'p'},
  {"architecture", required_argument, NULL, 'm'},
  {"archive-headers", no_argument, NULL, 'a'},
  {"debugging", no_argument, NULL, 'g'},
  {"demangle", optional_argument, NULL, 'C'},
  {"disassemble", no_argument, NULL, 'd'},
  {"disassemble-all", no_argument, NULL, 'D'},
  {"disassembler-options", required_argument, NULL, 'M'},
  {"disassemble-zeroes", no_argument, NULL, 'z'},
  {"dynamic-reloc", no_argument, NULL, 'R'},
  {"dynamic-syms", no_argument, NULL, 'T'},
  {"endian", required_argument, NULL, OPTION_ENDIAN},
  {"file-headers", no_argument, NULL, 'f'},
  {"file-start-context", no_argument, &file_start_context, 1},
  {"full-contents", no_argument, NULL, 's'},
  {"headers", no_argument, NULL, 'h'},
  {"help", no_argument, NULL, 'H'},
  {"info", no_argument, NULL, 'i'},
  {"line-numbers", no_argument, NULL, 'l'},
  {"no-show-raw-insn", no_argument, &show_raw_insn, -1},
  {"prefix-addresses", no_argument, &prefix_addresses, 1},
  {"reloc", no_argument, NULL, 'r'},
  {"section", required_argument, NULL, 'j'},
  {"section-headers", no_argument, NULL, 'h'},
  {"show-raw-insn", no_argument, &show_raw_insn, 1},
  {"source", no_argument, NULL, 'S'},
  {"stabs", no_argument, NULL, 'G'},
  {"start-address", required_argument, NULL, OPTION_START_ADDRESS},
  {"stop-address", required_argument, NULL, OPTION_STOP_ADDRESS},
  {"syms", no_argument, NULL, 't'},
  {"target", required_argument, NULL, 'b'},
  {"version", no_argument, NULL, 'V'},
  {"wide", no_argument, NULL, 'w'},
  {0, no_argument, 0, 0}
};

static void
nonfatal (msg)
     const char *msg;
{
  bfd_nonfatal (msg);
  exit_status = 1;
}

static void
dump_section_header (abfd, section, ignored)
     bfd *abfd ATTRIBUTE_UNUSED;
     asection *section;
     PTR ignored ATTRIBUTE_UNUSED;
{
  char *comma = "";
  unsigned int opb = bfd_octets_per_byte (abfd);

  printf ("%3d %-13s %08lx  ", section->index,
	  bfd_get_section_name (abfd, section),
	  (unsigned long) bfd_section_size (abfd, section) / opb);
  printf_vma (bfd_get_section_vma (abfd, section));
  printf ("  ");
  printf_vma (section->lma);
  printf ("  %08lx  2**%u", section->filepos,
	  bfd_get_section_alignment (abfd, section));
  if (! wide_output)
    printf ("\n                ");
  printf ("  ");

#define PF(x, y) \
  if (section->flags & x) { printf ("%s%s", comma, y); comma = ", "; }

  PF (SEC_HAS_CONTENTS, "CONTENTS");
  PF (SEC_ALLOC, "ALLOC");
  PF (SEC_CONSTRUCTOR, "CONSTRUCTOR");
  PF (SEC_CONSTRUCTOR_TEXT, "CONSTRUCTOR TEXT");
  PF (SEC_CONSTRUCTOR_DATA, "CONSTRUCTOR DATA");
  PF (SEC_CONSTRUCTOR_BSS, "CONSTRUCTOR BSS");
  PF (SEC_LOAD, "LOAD");
  PF (SEC_RELOC, "RELOC");
#ifdef SEC_BALIGN
  PF (SEC_BALIGN, "BALIGN");
#endif
  PF (SEC_READONLY, "READONLY");
  PF (SEC_CODE, "CODE");
  PF (SEC_DATA, "DATA");
  PF (SEC_ROM, "ROM");
  PF (SEC_DEBUGGING, "DEBUGGING");
  PF (SEC_NEVER_LOAD, "NEVER_LOAD");
  PF (SEC_EXCLUDE, "EXCLUDE");
  PF (SEC_SORT_ENTRIES, "SORT_ENTRIES");
  PF (SEC_BLOCK, "BLOCK");
  PF (SEC_CLINK, "CLINK");
  PF (SEC_SMALL_DATA, "SMALL_DATA");
  PF (SEC_SHARED, "SHARED");

  if ((section->flags & SEC_LINK_ONCE) != 0)
    {
      const char *ls;

      switch (section->flags & SEC_LINK_DUPLICATES)
	{
	default:
	  abort ();
	case SEC_LINK_DUPLICATES_DISCARD:
	  ls = "LINK_ONCE_DISCARD";
	  break;
	case SEC_LINK_DUPLICATES_ONE_ONLY:
	  ls = "LINK_ONCE_ONE_ONLY";
	  break;
	case SEC_LINK_DUPLICATES_SAME_SIZE:
	  ls = "LINK_ONCE_SAME_SIZE";
	  break;
	case SEC_LINK_DUPLICATES_SAME_CONTENTS:
	  ls = "LINK_ONCE_SAME_CONTENTS";
	  break;
	}
      printf ("%s%s", comma, ls);

      if (section->comdat != NULL)
	printf (" (COMDAT %s %ld)", section->comdat->name,
		section->comdat->symbol);

      comma = ", ";
    }

  printf ("\n");
#undef PF
}

static void
dump_headers (abfd)
     bfd *abfd;
{
  printf (_("Sections:\n"));

#ifndef BFD64
  printf (_("Idx Name          Size      VMA       LMA       File off  Algn"));
#else
  printf (_("Idx Name          Size      VMA               LMA               File off  Algn"));
#endif

  if (wide_output)
    printf (_("  Flags"));
  printf ("\n");

  bfd_map_over_sections (abfd, dump_section_header, (PTR) NULL);
}

static asymbol **
slurp_symtab (abfd)
     bfd *abfd;
{
  asymbol **sy = (asymbol **) NULL;
  long storage;

  if (!(bfd_get_file_flags (abfd) & HAS_SYMS))
    {
      non_fatal (_("%s: no symbols"), bfd_get_filename (abfd));
      symcount = 0;
      return NULL;
    }

  storage = bfd_get_symtab_upper_bound (abfd);
  if (storage < 0)
    bfd_fatal (bfd_get_filename (abfd));

  if (storage)
    {
      sy = (asymbol **) xmalloc (storage);
    }
  symcount = bfd_canonicalize_symtab (abfd, sy);
  if (symcount < 0)
    bfd_fatal (bfd_get_filename (abfd));
  if (symcount == 0)
    non_fatal (_("%s: no symbols"), bfd_get_filename (abfd));
  return sy;
}

/* Read in the dynamic symbols.  */

static asymbol **
slurp_dynamic_symtab (abfd)
     bfd *abfd;
{
  asymbol **sy = (asymbol **) NULL;
  long storage;

  storage = bfd_get_dynamic_symtab_upper_bound (abfd);
  if (storage < 0)
    {
      if (!(bfd_get_file_flags (abfd) & DYNAMIC))
	{
	  non_fatal (_("%s: not a dynamic object"), bfd_get_filename (abfd));
	  dynsymcount = 0;
	  return NULL;
	}

      bfd_fatal (bfd_get_filename (abfd));
    }

  if (storage)
    {
      sy = (asymbol **) xmalloc (storage);
    }
  dynsymcount = bfd_canonicalize_dynamic_symtab (abfd, sy);
  if (dynsymcount < 0)
    bfd_fatal (bfd_get_filename (abfd));
  if (dynsymcount == 0)
    non_fatal (_("%s: No dynamic symbols"), bfd_get_filename (abfd));
  return sy;
}

/* Filter out (in place) symbols that are useless for disassembly.
   COUNT is the number of elements in SYMBOLS.
   Return the number of useful symbols. */

static long
remove_useless_symbols (symbols, count)
     asymbol **symbols;
     long count;
{
  register asymbol **in_ptr = symbols, **out_ptr = symbols;

  while (--count >= 0)
    {
      asymbol *sym = *in_ptr++;

      if (sym->name == NULL || sym->name[0] == '\0')
	continue;
      if (sym->flags & (BSF_DEBUGGING))
	continue;
      if (bfd_is_und_section (sym->section)
	  || bfd_is_com_section (sym->section))
	continue;

      *out_ptr++ = sym;
    }
  return out_ptr - symbols;
}

/* Sort symbols into value order.  */

static int 
compare_symbols (ap, bp)
     const PTR ap;
     const PTR bp;
{
  const asymbol *a = *(const asymbol **)ap;
  const asymbol *b = *(const asymbol **)bp;
  const char *an, *bn;
  size_t anl, bnl;
  boolean af, bf;
  flagword aflags, bflags;

  if (bfd_asymbol_value (a) > bfd_asymbol_value (b))
    return 1;
  else if (bfd_asymbol_value (a) < bfd_asymbol_value (b))
    return -1;

  if (a->section > b->section)
    return 1;
  else if (a->section < b->section)
    return -1;

  an = bfd_asymbol_name (a);
  bn = bfd_asymbol_name (b);
  anl = strlen (an);
  bnl = strlen (bn);

  /* The symbols gnu_compiled and gcc2_compiled convey no real
     information, so put them after other symbols with the same value.  */

  af = (strstr (an, "gnu_compiled") != NULL
	|| strstr (an, "gcc2_compiled") != NULL);
  bf = (strstr (bn, "gnu_compiled") != NULL
	|| strstr (bn, "gcc2_compiled") != NULL);

  if (af && ! bf)
    return 1;
  if (! af && bf)
    return -1;

  /* We use a heuristic for the file name, to try to sort it after
     more useful symbols.  It may not work on non Unix systems, but it
     doesn't really matter; the only difference is precisely which
     symbol names get printed.  */

#define file_symbol(s, sn, snl)			\
  (((s)->flags & BSF_FILE) != 0			\
   || ((sn)[(snl) - 2] == '.'			\
       && ((sn)[(snl) - 1] == 'o'		\
	   || (sn)[(snl) - 1] == 'a')))

  af = file_symbol (a, an, anl);
  bf = file_symbol (b, bn, bnl);

  if (af && ! bf)
    return 1;
  if (! af && bf)
    return -1;

  /* Try to sort global symbols before local symbols before function
     symbols before debugging symbols.  */

  aflags = a->flags;
  bflags = b->flags;

  if ((aflags & BSF_DEBUGGING) != (bflags & BSF_DEBUGGING))
    {
      if ((aflags & BSF_DEBUGGING) != 0)
	return 1;
      else
	return -1;
    }
  if ((aflags & BSF_FUNCTION) != (bflags & BSF_FUNCTION))
    {
      if ((aflags & BSF_FUNCTION) != 0)
	return -1;
      else
	return 1;
    }
  if ((aflags & BSF_LOCAL) != (bflags & BSF_LOCAL))
    {
      if ((aflags & BSF_LOCAL) != 0)
	return 1;
      else
	return -1;
    }
  if ((aflags & BSF_GLOBAL) != (bflags & BSF_GLOBAL))
    {
      if ((aflags & BSF_GLOBAL) != 0)
	return -1;
      else
	return 1;
    }

  /* Symbols that start with '.' might be section names, so sort them
     after symbols that don't start with '.'.  */
  if (an[0] == '.' && bn[0] != '.')
    return 1;
  if (an[0] != '.' && bn[0] == '.')
    return -1;

  /* Finally, if we can't distinguish them in any other way, try to
     get consistent results by sorting the symbols by name.  */
  return strcmp (an, bn);
}

/* Sort relocs into address order.  */

static int
compare_relocs (ap, bp)
     const PTR ap;
     const PTR bp;
{
  const arelent *a = *(const arelent **)ap;
  const arelent *b = *(const arelent **)bp;

  if (a->address > b->address)
    return 1;
  else if (a->address < b->address)
    return -1;

  /* So that associated relocations tied to the same address show up
     in the correct order, we don't do any further sorting.  */
  if (a > b)
    return 1;
  else if (a < b)
    return -1;
  else
    return 0;
}

/* Print VMA to STREAM.  If SKIP_ZEROES is true, omit leading zeroes.  */

static void
objdump_print_value (vma, info, skip_zeroes)
     bfd_vma vma;
     struct disassemble_info *info;
     boolean skip_zeroes;
{
  char buf[30];
  char *p;

  sprintf_vma (buf, vma);
  if (! skip_zeroes)
    p = buf;
  else
    {
      for (p = buf; *p == '0'; ++p)
	;
      if (*p == '\0')
	--p;
    }
  (*info->fprintf_func) (info->stream, "%s", p);
}

/* Print the name of a symbol.  */

static void
objdump_print_symname (abfd, info, sym)
     bfd *abfd;
     struct disassemble_info *info;
     asymbol *sym;
{
  char *alloc;
  const char *name;
  const char *print;

  alloc = NULL;
  name = bfd_asymbol_name (sym);
  if (! do_demangle || name[0] == '\0')
    print = name;
  else
    {
      /* Demangle the name.  */
      if (bfd_get_symbol_leading_char (abfd) == name[0])
	++name;

      alloc = cplus_demangle (name, DMGL_ANSI | DMGL_PARAMS);
      if (alloc == NULL)
	print = name;
      else
	print = alloc;
    }

  if (info != NULL)
    (*info->fprintf_func) (info->stream, "%s", print);
  else
    printf ("%s", print);

  if (alloc != NULL)
    free (alloc);
}

/* Locate a symbol given a bfd, a section, and a VMA.  If REQUIRE_SEC
   is true, then always require the symbol to be in the section.  This
   returns NULL if there is no suitable symbol.  If PLACE is not NULL,
   then *PLACE is set to the index of the symbol in sorted_syms.  */

static asymbol *
find_symbol_for_address (abfd, sec, vma, require_sec, place)
     bfd *abfd;
     asection *sec;
     bfd_vma vma;
     boolean require_sec;
     long *place;
{
  /* @@ Would it speed things up to cache the last two symbols returned,
     and maybe their address ranges?  For many processors, only one memory
     operand can be present at a time, so the 2-entry cache wouldn't be
     constantly churned by code doing heavy memory accesses.  */

  /* Indices in `sorted_syms'.  */
  long min = 0;
  long max = sorted_symcount;
  long thisplace;
  unsigned int opb = bfd_octets_per_byte (abfd); 

  if (sorted_symcount < 1)
    return NULL;

  /* Perform a binary search looking for the closest symbol to the
     required value.  We are searching the range (min, max].  */
  while (min + 1 < max)
    {
      asymbol *sym;

      thisplace = (max + min) / 2;
      sym = sorted_syms[thisplace];

      if (bfd_asymbol_value (sym) > vma)
	max = thisplace;
      else if (bfd_asymbol_value (sym) < vma)
	min = thisplace;
      else
	{
	  min = thisplace;
	  break;
	}
    }

  /* The symbol we want is now in min, the low end of the range we
     were searching.  If there are several symbols with the same
     value, we want the first one.  */
  thisplace = min;
  while (thisplace > 0
	 && (bfd_asymbol_value (sorted_syms[thisplace])
	     == bfd_asymbol_value (sorted_syms[thisplace - 1])))
    --thisplace;

  /* If the file is relocateable, and the symbol could be from this
     section, prefer a symbol from this section over symbols from
     others, even if the other symbol's value might be closer.
       
     Note that this may be wrong for some symbol references if the
     sections have overlapping memory ranges, but in that case there's
     no way to tell what's desired without looking at the relocation
     table.  */

  if (sorted_syms[thisplace]->section != sec
      && (require_sec
	  || ((abfd->flags & HAS_RELOC) != 0
	      && vma >= bfd_get_section_vma (abfd, sec)
	      && vma < (bfd_get_section_vma (abfd, sec)
			+ bfd_section_size (abfd, sec) / opb))))
    {
      long i;

      for (i = thisplace + 1; i < sorted_symcount; i++)
	{
	  if (bfd_asymbol_value (sorted_syms[i])
	      != bfd_asymbol_value (sorted_syms[thisplace]))
	    break;
	}
      --i;
      for (; i >= 0; i--)
	{
	  if (sorted_syms[i]->section == sec
	      && (i == 0
		  || sorted_syms[i - 1]->section != sec
		  || (bfd_asymbol_value (sorted_syms[i])
		      != bfd_asymbol_value (sorted_syms[i - 1]))))
	    {
	      thisplace = i;
	      break;
	    }
	}

      if (sorted_syms[thisplace]->section != sec)
	{
	  /* We didn't find a good symbol with a smaller value.
	     Look for one with a larger value.  */
	  for (i = thisplace + 1; i < sorted_symcount; i++)
	    {
	      if (sorted_syms[i]->section == sec)
		{
		  thisplace = i;
		  break;
		}
	    }
	}

      if (sorted_syms[thisplace]->section != sec
	  && (require_sec
	      || ((abfd->flags & HAS_RELOC) != 0
		  && vma >= bfd_get_section_vma (abfd, sec)
		  && vma < (bfd_get_section_vma (abfd, sec)
			    + bfd_section_size (abfd, sec)))))
	{
	  /* There is no suitable symbol.  */
	  return NULL;
	}
    }

  if (place != NULL)
    *place = thisplace;

  return sorted_syms[thisplace];
}

/* Print an address to INFO symbolically.  */

static void
objdump_print_addr_with_sym (abfd, sec, sym, vma, info, skip_zeroes)
     bfd *abfd;
     asection *sec;
     asymbol *sym;
     bfd_vma vma;
     struct disassemble_info *info;
     boolean skip_zeroes;
{
  objdump_print_value (vma, info, skip_zeroes);

  if (sym == NULL)
    {
      bfd_vma secaddr;

      (*info->fprintf_func) (info->stream, " <%s",
			     bfd_get_section_name (abfd, sec));
      secaddr = bfd_get_section_vma (abfd, sec);
      if (vma < secaddr)
	{
	  (*info->fprintf_func) (info->stream, "-0x");
	  objdump_print_value (secaddr - vma, info, true);
	}
      else if (vma > secaddr)
	{
	  (*info->fprintf_func) (info->stream, "+0x");
	  objdump_print_value (vma - secaddr, info, true);
	}
      (*info->fprintf_func) (info->stream, ">");
    }
  else
    {
      (*info->fprintf_func) (info->stream, " <");
      objdump_print_symname (abfd, info, sym);
      if (bfd_asymbol_value (sym) > vma)
	{
	  (*info->fprintf_func) (info->stream, "-0x");
	  objdump_print_value (bfd_asymbol_value (sym) - vma, info, true);
	}
      else if (vma > bfd_asymbol_value (sym))
	{
	  (*info->fprintf_func) (info->stream, "+0x");
	  objdump_print_value (vma - bfd_asymbol_value (sym), info, true);
	}
      (*info->fprintf_func) (info->stream, ">");
    }
}

/* Print VMA to INFO, symbolically if possible.  If SKIP_ZEROES is
   true, don't output leading zeroes.  */

static void
objdump_print_addr (vma, info, skip_zeroes)
     bfd_vma vma;
     struct disassemble_info *info;
     boolean skip_zeroes;
{
  struct objdump_disasm_info *aux;
  asymbol *sym;

  if (sorted_symcount < 1)
    {
      (*info->fprintf_func) (info->stream, "0x");
      objdump_print_value (vma, info, skip_zeroes);
      return;
    }

  aux = (struct objdump_disasm_info *) info->application_data;
  sym = find_symbol_for_address (aux->abfd, aux->sec, vma, aux->require_sec,
				 (long *) NULL);
  objdump_print_addr_with_sym (aux->abfd, aux->sec, sym, vma, info,
			       skip_zeroes);
}

/* Print VMA to INFO.  This function is passed to the disassembler
   routine.  */

static void
objdump_print_address (vma, info)
     bfd_vma vma;
     struct disassemble_info *info;
{
  objdump_print_addr (vma, info, ! prefix_addresses);
}

/* Determine of the given address has a symbol associated with it.  */

static int
objdump_symbol_at_address (vma, info)
     bfd_vma vma;
     struct disassemble_info * info;
{
  struct objdump_disasm_info * aux;
  asymbol * sym;

  /* No symbols - do not bother checking.  */
  if (sorted_symcount < 1)
    return 0;

  aux = (struct objdump_disasm_info *) info->application_data;
  sym = find_symbol_for_address (aux->abfd, aux->sec, vma, aux->require_sec,
				 (long *) NULL);

  return (sym != NULL && (bfd_asymbol_value (sym) == vma));
}

/* Hold the last function name and the last line number we displayed
   in a disassembly.  */

static char *prev_functionname;
static unsigned int prev_line;

/* We keep a list of all files that we have seen when doing a
   dissassembly with source, so that we know how much of the file to
   display.  This can be important for inlined functions.  */

struct print_file_list
{
  struct print_file_list *next;
  char *filename;
  unsigned int line;
  FILE *f;
};

static struct print_file_list *print_files;

/* The number of preceding context lines to show when we start
   displaying a file for the first time.  */

#define SHOW_PRECEDING_CONTEXT_LINES (5)

/* Skip ahead to a given line in a file, optionally printing each
   line.  */

static void
skip_to_line PARAMS ((struct print_file_list *, unsigned int, boolean));

static void
skip_to_line (p, line, show)
     struct print_file_list *p;
     unsigned int line;
     boolean show;
{
  while (p->line < line)
    {
      char buf[100];

      if (fgets (buf, sizeof buf, p->f) == NULL)
	{
	  fclose (p->f);
	  p->f = NULL;
	  break;
	}

      if (show)
	printf ("%s", buf);

      if (strchr (buf, '\n') != NULL)
	++p->line;
    }
}  

/* Show the line number, or the source line, in a dissassembly
   listing.  */

static void
show_line (abfd, section, addr_offset)
     bfd *abfd;
     asection *section;
     bfd_vma addr_offset;
{
  CONST char *filename;
  CONST char *functionname;
  unsigned int line;

  if (! with_line_numbers && ! with_source_code)
    return;

  if (! bfd_find_nearest_line (abfd, section, syms, addr_offset, &filename,
			       &functionname, &line))
    return;

  if (filename != NULL && *filename == '\0')
    filename = NULL;
  if (functionname != NULL && *functionname == '\0')
    functionname = NULL;

  if (with_line_numbers)
    {
      if (functionname != NULL
	  && (prev_functionname == NULL
	      || strcmp (functionname, prev_functionname) != 0))
	printf ("%s():\n", functionname);
      if (line > 0 && line != prev_line)
	printf ("%s:%u\n", filename == NULL ? "???" : filename, line);
    }

  if (with_source_code
      && filename != NULL
      && line > 0)
    {
      struct print_file_list **pp, *p;

      for (pp = &print_files; *pp != NULL; pp = &(*pp)->next)
	if (strcmp ((*pp)->filename, filename) == 0)
	  break;
      p = *pp;

      if (p != NULL)
	{
	  if (p != print_files)
	    {
	      int l;

	      /* We have reencountered a file name which we saw
		 earlier.  This implies that either we are dumping out
		 code from an included file, or the same file was
		 linked in more than once.  There are two common cases
		 of an included file: inline functions in a header
		 file, and a bison or flex skeleton file.  In the
		 former case we want to just start printing (but we
		 back up a few lines to give context); in the latter
		 case we want to continue from where we left off.  I
		 can't think of a good way to distinguish the cases,
		 so I used a heuristic based on the file name.  */
	      if (strcmp (p->filename + strlen (p->filename) - 2, ".h") != 0)
		l = p->line;
	      else
		{
		  l = line - SHOW_PRECEDING_CONTEXT_LINES;
		  if (l < 0)
		    l = 0;
		}

	      if (p->f == NULL)
		{
		  p->f = fopen (p->filename, "r");
		  p->line = 0;
		}
	      if (p->f != NULL)
		skip_to_line (p, l, false);

	      if (print_files->f != NULL)
		{
		  fclose (print_files->f);
		  print_files->f = NULL;
		}
	    }

	  if (p->f != NULL)
	    {
	      skip_to_line (p, line, true);
	      *pp = p->next;
	      p->next = print_files;
	      print_files = p;
	    }
	}
      else
	{
	  FILE *f;

	  f = fopen (filename, "r");
	  if (f != NULL)
	    {
	      int l;

	      p = ((struct print_file_list *)
		   xmalloc (sizeof (struct print_file_list)));
	      p->filename = xmalloc (strlen (filename) + 1);
	      strcpy (p->filename, filename);
	      p->line = 0;
	      p->f = f;

	      if (print_files != NULL && print_files->f != NULL)
		{
		  fclose (print_files->f);
		  print_files->f = NULL;
		}
	      p->next = print_files;
	      print_files = p;

              if (file_start_context)
                l = 0;
              else
                l = line - SHOW_PRECEDING_CONTEXT_LINES;
	      if (l < 0)
		l = 0;
	      skip_to_line (p, l, false);
	      if (p->f != NULL)
		skip_to_line (p, line, true);
	    }
	}
    }

  if (functionname != NULL
      && (prev_functionname == NULL
	  || strcmp (functionname, prev_functionname) != 0))
    {
      if (prev_functionname != NULL)
	free (prev_functionname);
      prev_functionname = xmalloc (strlen (functionname) + 1);
      strcpy (prev_functionname, functionname);
    }

  if (line > 0 && line != prev_line)
    prev_line = line;
}

/* Pseudo FILE object for strings.  */
typedef struct
{
  char *buffer;
  size_t size;
  char *current;
} SFILE;

/* sprintf to a "stream" */

static int
#ifdef ANSI_PROTOTYPES
objdump_sprintf (SFILE *f, const char *format, ...)
#else
objdump_sprintf (va_alist)
     va_dcl
#endif
{
#ifndef ANSI_PROTOTYPES
  SFILE *f;
  const char *format;
#endif
  char *buf;
  va_list args;
  size_t n;

#ifdef ANSI_PROTOTYPES
  va_start (args, format);
#else
  va_start (args);
  f = va_arg (args, SFILE *);
  format = va_arg (args, const char *);
#endif

  vasprintf (&buf, format, args);

  va_end (args);

  if (buf == NULL)
    {
      fatal (_("Out of virtual memory"));
    }

  n = strlen (buf);

  while ((size_t) ((f->buffer + f->size) - f->current) < n + 1)
    {
      size_t curroff;

      curroff = f->current - f->buffer;
      f->size *= 2;
      f->buffer = xrealloc (f->buffer, f->size);
      f->current = f->buffer + curroff;
    }

  memcpy (f->current, buf, n);
  f->current += n;
  f->current[0] = '\0';

  free (buf);

  return n;
}

/* The number of zeroes we want to see before we start skipping them.
   The number is arbitrarily chosen.  */

#define SKIP_ZEROES (8)

/* The number of zeroes to skip at the end of a section.  If the
   number of zeroes at the end is between SKIP_ZEROES_AT_END and
   SKIP_ZEROES, they will be disassembled.  If there are fewer than
   SKIP_ZEROES_AT_END, they will be skipped.  This is a heuristic
   attempt to avoid disassembling zeroes inserted by section
   alignment.  */

#define SKIP_ZEROES_AT_END (3)

/* Disassemble some data in memory between given values.  */

static void
disassemble_bytes (info, disassemble_fn, insns, data, 
                   start_offset, stop_offset, relppp,
		   relppend)
     struct disassemble_info *info;
     disassembler_ftype disassemble_fn;
     boolean insns;
     bfd_byte *data;
     bfd_vma start_offset;
     bfd_vma stop_offset;
     arelent ***relppp;
     arelent **relppend;
{
  struct objdump_disasm_info *aux;
  asection *section;
  int octets_per_line;
  boolean done_dot;
  int skip_addr_chars;
  bfd_vma addr_offset;
  int opb = info->octets_per_byte;

  aux = (struct objdump_disasm_info *) info->application_data;
  section = aux->sec;

  if (insns)
    octets_per_line = 4;
  else
    octets_per_line = 16;

  /* Figure out how many characters to skip at the start of an
     address, to make the disassembly look nicer.  We discard leading
     zeroes in chunks of 4, ensuring that there is always a leading
     zero remaining.  */
  skip_addr_chars = 0;
  if (! prefix_addresses)
    {
      char buf[30];
      char *s;

      sprintf_vma (buf, section->vma + 
                   bfd_section_size (section->owner, section) / opb);
      s = buf;
      while (s[0] == '0' && s[1] == '0' && s[2] == '0' && s[3] == '0'
	     && s[4] == '0')
	{
	  skip_addr_chars += 4;
	  s += 4;
	}
    }

  info->insn_info_valid = 0;

  done_dot = false;
  addr_offset = start_offset;
  while (addr_offset < stop_offset)
    {
      bfd_vma z;
      int octets = 0;
      boolean need_nl = false;

      /* If we see more than SKIP_ZEROES octets of zeroes, we just
         print `...'.  */
      for (z = addr_offset * opb; z < stop_offset * opb; z++)
	if (data[z] != 0)
	  break;
      if (! disassemble_zeroes
	  && (info->insn_info_valid == 0
	      || info->branch_delay_insns == 0)
	  && (z - addr_offset * opb >= SKIP_ZEROES
	      || (z == stop_offset * opb && 
                  z - addr_offset * opb < SKIP_ZEROES_AT_END)))
	{
	  printf ("\t...\n");

	  /* If there are more nonzero octets to follow, we only skip
             zeroes in multiples of 4, to try to avoid running over
             the start of an instruction which happens to start with
             zero.  */
	  if (z != stop_offset * opb)
	    z = addr_offset * opb + ((z - addr_offset * opb) &~ 3);

	  octets = z - addr_offset * opb;
	}
      else
	{
	  char buf[50];
	  SFILE sfile;
	  int bpc = 0;
	  int pb = 0;

	  done_dot = false;

	  if (with_line_numbers || with_source_code)
	    show_line (aux->abfd, section, addr_offset);

	  if (! prefix_addresses)
	    {
	      char *s;

	      sprintf_vma (buf, section->vma + addr_offset);
	      for (s = buf + skip_addr_chars; *s == '0'; s++)
		*s = ' ';
	      if (*s == '\0')
		*--s = '0';
	      printf ("%s:\t", buf + skip_addr_chars);
	    }
	  else
	    {
	      aux->require_sec = true;
	      objdump_print_address (section->vma + addr_offset, info);
	      aux->require_sec = false;
	      putchar (' ');
	    }

	  if (insns)
	    {
	      sfile.size = 120;
	      sfile.buffer = xmalloc (sfile.size);
	      sfile.current = sfile.buffer;
	      info->fprintf_func = (fprintf_ftype) objdump_sprintf;
	      info->stream = (FILE *) &sfile;
	      info->bytes_per_line = 0;
	      info->bytes_per_chunk = 0;

#ifdef DISASSEMBLER_NEEDS_RELOCS
	      /* FIXME: This is wrong.  It tests the number of octets
                 in the last instruction, not the current one.  */
	      if (*relppp < relppend
		  && (**relppp)->address >= addr_offset
		  && (**relppp)->address <= addr_offset + octets / opb)
		info->flags = INSN_HAS_RELOC;
	      else
#endif
		info->flags = 0;

	      octets = (*disassemble_fn) (section->vma + addr_offset, info);
	      info->fprintf_func = (fprintf_ftype) fprintf;
	      info->stream = stdout;
	      if (info->bytes_per_line != 0)
		octets_per_line = info->bytes_per_line;
	      if (octets < 0)
		{
		  if (sfile.current != sfile.buffer)
		    printf ("%s\n", sfile.buffer);
		  free (sfile.buffer);
		  break;
		}
	    }
	  else
	    {
	      bfd_vma j;

	      octets = octets_per_line;
	      if (addr_offset + octets / opb > stop_offset)
		octets = (stop_offset - addr_offset) * opb;

	      for (j = addr_offset * opb; j < addr_offset * opb + octets; ++j)
		{
		  if (isprint (data[j]))
		    buf[j - addr_offset * opb] = data[j];
		  else
		    buf[j - addr_offset * opb] = '.';
		}
	      buf[j - addr_offset * opb] = '\0';
	    }

	  if (prefix_addresses
	      ? show_raw_insn > 0
	      : show_raw_insn >= 0)
	    {
	      bfd_vma j;

	      /* If ! prefix_addresses and ! wide_output, we print
                 octets_per_line octets per line.  */
	      pb = octets;
	      if (pb > octets_per_line && ! prefix_addresses && ! wide_output)
		pb = octets_per_line;

	      if (info->bytes_per_chunk)
		bpc = info->bytes_per_chunk;
	      else
		bpc = 1;

	      for (j = addr_offset * opb; j < addr_offset * opb + pb; j += bpc)
		{
		  int k;
		  if (bpc > 1 && info->display_endian == BFD_ENDIAN_LITTLE)
		    {
		      for (k = bpc - 1; k >= 0; k--)
			printf ("%02x", (unsigned) data[j + k]);
		      putchar (' ');
		    }
		  else
		    {
		      for (k = 0; k < bpc; k++)
			printf ("%02x", (unsigned) data[j + k]);
		      putchar (' ');
		    }
		}

	      for (; pb < octets_per_line; pb += bpc)
		{
		  int k;

		  for (k = 0; k < bpc; k++)
		    printf ("  ");
		  putchar (' ');
		}

	      /* Separate raw data from instruction by extra space.  */
	      if (insns)
		putchar ('\t');
	      else
		printf ("    ");
	    }

	  if (! insns)
	    printf ("%s", buf);
	  else
	    {
	      printf ("%s", sfile.buffer);
	      free (sfile.buffer);
	    }

	  if (prefix_addresses
	      ? show_raw_insn > 0
	      : show_raw_insn >= 0)
	    {
	      while (pb < octets)
		{
		  bfd_vma j;
		  char *s;

		  putchar ('\n');
		  j = addr_offset * opb + pb;

		  sprintf_vma (buf, section->vma + j / opb);
		  for (s = buf + skip_addr_chars; *s == '0'; s++)
		    *s = ' ';
		  if (*s == '\0')
		    *--s = '0';
		  printf ("%s:\t", buf + skip_addr_chars);

		  pb += octets_per_line;
		  if (pb > octets)
		    pb = octets;
		  for (; j < addr_offset * opb + pb; j += bpc)
		    {
		      int k;

		      if (bpc > 1 && info->display_endian == BFD_ENDIAN_LITTLE)
			{
			  for (k = bpc - 1; k >= 0; k--)
			    printf ("%02x", (unsigned) data[j + k]);
			  putchar (' ');
			}
		      else
			{
			  for (k = 0; k < bpc; k++)
			    printf ("%02x", (unsigned) data[j + k]);
			  putchar (' ');
			}
		    }
		}
	    }

	  if (!wide_output)
	    putchar ('\n');
	  else
	    need_nl = true;
	}

      if ((section->flags & SEC_RELOC) != 0
#ifndef DISASSEMBLER_NEEDS_RELOCS	  
  	  && dump_reloc_info
#endif
	  )
	{
	  while ((*relppp) < relppend
		 && ((**relppp)->address >= (bfd_vma) addr_offset
		     && (**relppp)->address < (bfd_vma) addr_offset + octets / opb))
#ifdef DISASSEMBLER_NEEDS_RELOCS
	    if (! dump_reloc_info)
	      ++(*relppp);
	    else
#endif
	    {
	      arelent *q;

	      q = **relppp;

	      if (wide_output)
		putchar ('\t');
	      else
		printf ("\t\t\t");

	      objdump_print_value (section->vma + q->address, info, true);

	      printf (": %s\t", q->howto->name);

	      if (q->sym_ptr_ptr == NULL || *q->sym_ptr_ptr == NULL)
		printf ("*unknown*");
	      else
		{
		  const char *sym_name;

		  sym_name = bfd_asymbol_name (*q->sym_ptr_ptr);
		  if (sym_name != NULL && *sym_name != '\0')
		    objdump_print_symname (aux->abfd, info, *q->sym_ptr_ptr);
		  else
		    {
		      asection *sym_sec;

		      sym_sec = bfd_get_section (*q->sym_ptr_ptr);
		      sym_name = bfd_get_section_name (aux->abfd, sym_sec);
		      if (sym_name == NULL || *sym_name == '\0')
			sym_name = "*unknown*";
		      printf ("%s", sym_name);
		    }
		}

	      if (q->addend)
		{
		  printf ("+0x");
		  objdump_print_value (q->addend, info, true);
		}

	      printf ("\n");
	      need_nl = false;
	      ++(*relppp);
	    }
	}

      if (need_nl)
	printf ("\n");

      addr_offset += octets / opb;
    }
}

/* Disassemble the contents of an object file.  */

static void
disassemble_data (abfd)
     bfd *abfd;
{
  unsigned long addr_offset;
  disassembler_ftype disassemble_fn;
  struct disassemble_info disasm_info;
  struct objdump_disasm_info aux;
  asection *section;
  unsigned int opb;

  print_files = NULL;
  prev_functionname = NULL;
  prev_line = -1;

  /* We make a copy of syms to sort.  We don't want to sort syms
     because that will screw up the relocs.  */
  sorted_syms = (asymbol **) xmalloc (symcount * sizeof (asymbol *));
  memcpy (sorted_syms, syms, symcount * sizeof (asymbol *));

  sorted_symcount = remove_useless_symbols (sorted_syms, symcount);

  /* Sort the symbols into section and symbol order */
  qsort (sorted_syms, sorted_symcount, sizeof (asymbol *), compare_symbols);

  INIT_DISASSEMBLE_INFO(disasm_info, stdout, fprintf);
  disasm_info.application_data = (PTR) &aux;
  aux.abfd = abfd;
  aux.require_sec = false;
  disasm_info.print_address_func = objdump_print_address;
  disasm_info.symbol_at_address_func = objdump_symbol_at_address;

  if (machine != (char *) NULL)
    {
      const bfd_arch_info_type *info = bfd_scan_arch (machine);
      if (info == NULL)
	{
	  fatal (_("Can't use supplied machine %s"), machine);
	}
      abfd->arch_info = info;
    }

  if (endian != BFD_ENDIAN_UNKNOWN)
    {
      struct bfd_target *xvec;

      xvec = (struct bfd_target *) xmalloc (sizeof (struct bfd_target));
      memcpy (xvec, abfd->xvec, sizeof (struct bfd_target));
      xvec->byteorder = endian;
      abfd->xvec = xvec;
    }

  disassemble_fn = disassembler (abfd);
  if (!disassemble_fn)
    {
      non_fatal (_("Can't disassemble for architecture %s\n"),
		 bfd_printable_arch_mach (bfd_get_arch (abfd), 0));
      exit_status = 1;
      return;
    }

  opb = bfd_octets_per_byte (abfd);

  disasm_info.flavour = bfd_get_flavour (abfd);
  disasm_info.arch = bfd_get_arch (abfd);
  disasm_info.mach = bfd_get_mach (abfd);
  disasm_info.disassembler_options = disassembler_options;
  disasm_info.octets_per_byte = opb;
  
  if (bfd_big_endian (abfd))
    disasm_info.display_endian = disasm_info.endian = BFD_ENDIAN_BIG;
  else if (bfd_little_endian (abfd))
    disasm_info.display_endian = disasm_info.endian = BFD_ENDIAN_LITTLE;
  else
    /* ??? Aborting here seems too drastic.  We could default to big or little
       instead.  */
    disasm_info.endian = BFD_ENDIAN_UNKNOWN;

  for (section = abfd->sections;
       section != (asection *) NULL;
       section = section->next)
    {
      bfd_byte *data = NULL;
      bfd_size_type datasize = 0;
      arelent **relbuf = NULL;
      arelent **relpp = NULL;
      arelent **relppend = NULL;
      unsigned long stop_offset;
      asymbol *sym = NULL;
      long place = 0;

      if ((section->flags & SEC_LOAD) == 0
	  || (! disassemble_all
	      && only == NULL
	      && (section->flags & SEC_CODE) == 0))
	continue;
      if (only != (char *) NULL && strcmp (only, section->name) != 0)
	continue;

      if ((section->flags & SEC_RELOC) != 0
#ifndef DISASSEMBLER_NEEDS_RELOCS	  
	  && dump_reloc_info
#endif
	  ) 
	{
	  long relsize;

	  relsize = bfd_get_reloc_upper_bound (abfd, section);
	  if (relsize < 0)
	    bfd_fatal (bfd_get_filename (abfd));

	  if (relsize > 0)
	    {
	      long relcount;

	      relbuf = (arelent **) xmalloc (relsize);
	      relcount = bfd_canonicalize_reloc (abfd, section, relbuf, syms);
	      if (relcount < 0)
		bfd_fatal (bfd_get_filename (abfd));

	      /* Sort the relocs by address.  */
	      qsort (relbuf, relcount, sizeof (arelent *), compare_relocs);

	      relpp = relbuf;
	      relppend = relpp + relcount;

	      /* Skip over the relocs belonging to addresses below the
		 start address.  */
	      if (start_address != (bfd_vma) -1)
		{
		  while (relpp < relppend
			 && (*relpp)->address < start_address)
		    ++relpp;
		}
	    }
	}

      printf (_("Disassembly of section %s:\n"), section->name);

      datasize = bfd_get_section_size_before_reloc (section);
      if (datasize == 0)
	continue;

      data = (bfd_byte *) xmalloc ((size_t) datasize);

      bfd_get_section_contents (abfd, section, data, 0, datasize);

      aux.sec = section;
      disasm_info.buffer = data;
      disasm_info.buffer_vma = section->vma;
      disasm_info.buffer_length = datasize;
      if (start_address == (bfd_vma) -1
	  || start_address < disasm_info.buffer_vma)
	addr_offset = 0;
      else
	addr_offset = start_address - disasm_info.buffer_vma;
      if (stop_address == (bfd_vma) -1)
	stop_offset = datasize / opb;
      else
	{
	  if (stop_address < disasm_info.buffer_vma)
	    stop_offset = 0;
	  else
	    stop_offset = stop_address - disasm_info.buffer_vma;
	  if (stop_offset > disasm_info.buffer_length / opb)
	    stop_offset = disasm_info.buffer_length / opb;
	}

      sym = find_symbol_for_address (abfd, section, section->vma + addr_offset,
				     true, &place);

      while (addr_offset < stop_offset)
	{
	  asymbol *nextsym;
	  unsigned long nextstop_offset;
	  boolean insns;
	  
	  if (sym != NULL && bfd_asymbol_value (sym) <= section->vma + addr_offset)
	    {
	      int x;

	      for (x = place;
		   (x < sorted_symcount
		    && bfd_asymbol_value (sorted_syms[x]) <= section->vma + addr_offset);
		   ++x)
		continue;
	      disasm_info.symbols = & sorted_syms[place];
	      disasm_info.num_symbols = x - place;
	    }
	  else
	    disasm_info.symbols = NULL;

	  if (! prefix_addresses)
	    {
	      printf ("\n");
	      objdump_print_addr_with_sym (abfd, section, sym,
					   section->vma + addr_offset,
					   &disasm_info,
					   false);
	      printf (":\n");
	    }
	  
	  if (sym != NULL && bfd_asymbol_value (sym) > section->vma + addr_offset)
	    nextsym = sym;
	  else if (sym == NULL)
	    nextsym = NULL;
	  else
	    {
	      /* Search forward for the next appropriate symbol in
                 SECTION.  Note that all the symbols are sorted
                 together into one big array, and that some sections
                 may have overlapping addresses.  */
	      while (place < sorted_symcount
		     && (sorted_syms[place]->section != section
			 || (bfd_asymbol_value (sorted_syms[place])
			     <= bfd_asymbol_value (sym))))
		++place;
	      if (place >= sorted_symcount)
		nextsym = NULL;
	      else
		nextsym = sorted_syms[place];
	    }
	  
	  if (sym != NULL && bfd_asymbol_value (sym) > section->vma + addr_offset)
	    {
	      nextstop_offset = bfd_asymbol_value (sym) - section->vma;
	      if (nextstop_offset > stop_offset)
		nextstop_offset = stop_offset;
	    }
	  else if (nextsym == NULL)
	    nextstop_offset = stop_offset;
	  else
	    {
	      nextstop_offset = bfd_asymbol_value (nextsym) - section->vma;
	      if (nextstop_offset > stop_offset)
		nextstop_offset = stop_offset;
	    }
	  
	  /* If a symbol is explicitly marked as being an object
	     rather than a function, just dump the bytes without
	     disassembling them.  */
	  if (disassemble_all
	      || sym == NULL
	      || bfd_asymbol_value (sym) > section->vma + addr_offset
	      || ((sym->flags & BSF_OBJECT) == 0
		  && (strstr (bfd_asymbol_name (sym), "gnu_compiled")
		      == NULL)
		  && (strstr (bfd_asymbol_name (sym), "gcc2_compiled")
		      == NULL))
	      || (sym->flags & BSF_FUNCTION) != 0)
	    insns = true;
	  else
	    insns = false;
	  
	  disassemble_bytes (&disasm_info, disassemble_fn, insns, data, 
                             addr_offset, nextstop_offset, &relpp, relppend);
	  
	  addr_offset = nextstop_offset;
	  sym = nextsym;
	}
      
      free (data);
      if (relbuf != NULL)
	free (relbuf);
    }
  free (sorted_syms);
}


/* Define a table of stab values and print-strings.  We wish the initializer
   could be a direct-mapped table, but instead we build one the first
   time we need it.  */

static void dump_section_stabs PARAMS ((bfd *abfd, char *stabsect_name,
					char *strsect_name));

/* Dump the stabs sections from an object file that has a section that
   uses Sun stabs encoding.  */

static void
dump_stabs (abfd)
     bfd *abfd;
{
  dump_section_stabs (abfd, ".stab", ".stabstr");
  dump_section_stabs (abfd, ".stab.excl", ".stab.exclstr");
  dump_section_stabs (abfd, ".stab.index", ".stab.indexstr");
  dump_section_stabs (abfd, "$GDB_SYMBOLS$", "$GDB_STRINGS$");
}

static bfd_byte *stabs;
static bfd_size_type stab_size;

static char *strtab;
static bfd_size_type stabstr_size;

/* Read ABFD's stabs section STABSECT_NAME into `stabs'
   and string table section STRSECT_NAME into `strtab'.
   If the section exists and was read, allocate the space and return true.
   Otherwise return false.  */

static boolean
read_section_stabs (abfd, stabsect_name, strsect_name)
     bfd *abfd;
     const char *stabsect_name;
     const char *strsect_name;
{
  asection *stabsect, *stabstrsect;

  stabsect = bfd_get_section_by_name (abfd, stabsect_name);
  if (0 == stabsect)
    {
      printf (_("No %s section present\n\n"), stabsect_name);
      return false;
    }

  stabstrsect = bfd_get_section_by_name (abfd, strsect_name);
  if (0 == stabstrsect)
    {
      non_fatal (_("%s has no %s section"),
		 bfd_get_filename (abfd), strsect_name);
      exit_status = 1;
      return false;
    }
 
  stab_size    = bfd_section_size (abfd, stabsect);
  stabstr_size = bfd_section_size (abfd, stabstrsect);

  stabs  = (bfd_byte *) xmalloc (stab_size);
  strtab = (char *) xmalloc (stabstr_size);
  
  if (! bfd_get_section_contents (abfd, stabsect, (PTR) stabs, 0, stab_size))
    {
      non_fatal (_("Reading %s section of %s failed: %s"),
		 stabsect_name, bfd_get_filename (abfd),
		 bfd_errmsg (bfd_get_error ()));
      free (stabs);
      free (strtab);
      exit_status = 1;
      return false;
    }

  if (! bfd_get_section_contents (abfd, stabstrsect, (PTR) strtab, 0,
				  stabstr_size))
    {
      non_fatal (_("Reading %s section of %s failed: %s\n"),
		 strsect_name, bfd_get_filename (abfd),
		 bfd_errmsg (bfd_get_error ()));
      free (stabs);
      free (strtab);
      exit_status = 1;
      return false;
    }

  return true;
}

/* Stabs entries use a 12 byte format:
     4 byte string table index
     1 byte stab type
     1 byte stab other field
     2 byte stab desc field
     4 byte stab value
   FIXME: This will have to change for a 64 bit object format.  */

#define STRDXOFF (0)
#define TYPEOFF (4)
#define OTHEROFF (5)
#define DESCOFF (6)
#define VALOFF (8)
#define STABSIZE (12)

/* Print ABFD's stabs section STABSECT_NAME (in `stabs'),
   using string table section STRSECT_NAME (in `strtab').  */

static void
print_section_stabs (abfd, stabsect_name, strsect_name)
     bfd *abfd;
     const char *stabsect_name;
     const char *strsect_name ATTRIBUTE_UNUSED;
{
  int i;
  unsigned file_string_table_offset = 0, next_file_string_table_offset = 0;
  bfd_byte *stabp, *stabs_end;

  stabp = stabs;
  stabs_end = stabp + stab_size;

  printf (_("Contents of %s section:\n\n"), stabsect_name);
  printf ("Symnum n_type n_othr n_desc n_value  n_strx String\n");

  /* Loop through all symbols and print them.

     We start the index at -1 because there is a dummy symbol on
     the front of stabs-in-{coff,elf} sections that supplies sizes.  */

  for (i = -1; stabp < stabs_end; stabp += STABSIZE, i++)
    {
      const char *name;
      unsigned long strx;
      unsigned char type, other;
      unsigned short desc;
      bfd_vma value;

      strx = bfd_h_get_32 (abfd, stabp + STRDXOFF);
      type = bfd_h_get_8 (abfd, stabp + TYPEOFF);
      other = bfd_h_get_8 (abfd, stabp + OTHEROFF);
      desc = bfd_h_get_16 (abfd, stabp + DESCOFF);
      value = bfd_h_get_32 (abfd, stabp + VALOFF);

      printf ("\n%-6d ", i);
      /* Either print the stab name, or, if unnamed, print its number
	 again (makes consistent formatting for tools like awk). */
      name = bfd_get_stab_name (type);
      if (name != NULL)
	printf ("%-6s", name);
      else if (type == N_UNDF)
	printf ("HdrSym");
      else
	printf ("%-6d", type);
      printf (" %-6d %-6d ", other, desc);
      printf_vma (value);
      printf (" %-6lu", strx);

      /* Symbols with type == 0 (N_UNDF) specify the length of the
	 string table associated with this file.  We use that info
	 to know how to relocate the *next* file's string table indices.  */

      if (type == N_UNDF)
	{
	  file_string_table_offset = next_file_string_table_offset;
	  next_file_string_table_offset += value;
	}
      else
	{
	  /* Using the (possibly updated) string table offset, print the
	     string (if any) associated with this symbol.  */

	  if ((strx + file_string_table_offset) < stabstr_size)
	    printf (" %s", &strtab[strx + file_string_table_offset]);
	  else
	    printf (" *");
	}
    }
  printf ("\n\n");
}

static void
dump_section_stabs (abfd, stabsect_name, strsect_name)
     bfd *abfd;
     char *stabsect_name;
     char *strsect_name;
{
  asection *s;

  /* Check for section names for which stabsect_name is a prefix, to
     handle .stab0, etc.  */
  for (s = abfd->sections;
       s != NULL;
       s = s->next)
    {
      int len;

      len = strlen (stabsect_name);

      /* If the prefix matches, and the files section name ends with a
	 nul or a digit, then we match.  I.e., we want either an exact
	 match or a section followed by a number.  */
      if (strncmp (stabsect_name, s->name, len) == 0
	  && (s->name[len] == '\000'
	      || isdigit ((unsigned char) s->name[len])))
	{
	  if (read_section_stabs (abfd, s->name, strsect_name))
	    {
	      print_section_stabs (abfd, s->name, strsect_name);
	      free (stabs);
	      free (strtab);
	    }
	}
    }
}

static void
dump_bfd_header (abfd)
     bfd *abfd;
{
  char *comma = "";

  printf (_("architecture: %s, "),
	  bfd_printable_arch_mach (bfd_get_arch (abfd),
				   bfd_get_mach (abfd)));
  printf (_("flags 0x%08x:\n"), abfd->flags);

#define PF(x, y)    if (abfd->flags & x) {printf("%s%s", comma, y); comma=", ";}
  PF (HAS_RELOC, "HAS_RELOC");
  PF (EXEC_P, "EXEC_P");
  PF (HAS_LINENO, "HAS_LINENO");
  PF (HAS_DEBUG, "HAS_DEBUG");
  PF (HAS_SYMS, "HAS_SYMS");
  PF (HAS_LOCALS, "HAS_LOCALS");
  PF (DYNAMIC, "DYNAMIC");
  PF (WP_TEXT, "WP_TEXT");
  PF (D_PAGED, "D_PAGED");
  PF (BFD_IS_RELAXABLE, "BFD_IS_RELAXABLE");
  printf (_("\nstart address 0x"));
  printf_vma (abfd->start_address);
  printf ("\n");
}

static void
dump_bfd_private_header (abfd)
bfd *abfd;
{
  bfd_print_private_bfd_data (abfd, stdout);
}

/* Dump selected contents of ABFD */

static void
dump_bfd (abfd)
     bfd *abfd;
{
  /* If we are adjusting section VMA's, change them all now.  Changing
     the BFD information is a hack.  However, we must do it, or
     bfd_find_nearest_line will not do the right thing.  */
  if (adjust_section_vma != 0)
    {
      asection *s;

      for (s = abfd->sections; s != NULL; s = s->next)
	{
	  s->vma += adjust_section_vma;
	  s->lma += adjust_section_vma;
	}
    }

  printf (_("\n%s:     file format %s\n"), bfd_get_filename (abfd),
	  abfd->xvec->name);
  if (dump_ar_hdrs)
    print_arelt_descr (stdout, abfd, true);
  if (dump_file_header)
    dump_bfd_header (abfd);
  if (dump_private_headers)
    dump_bfd_private_header (abfd);
  putchar ('\n');
  if (dump_section_headers)
    dump_headers (abfd);
  if (dump_symtab || dump_reloc_info || disassemble || dump_debugging)
    {
      syms = slurp_symtab (abfd);
    }
  if (dump_dynamic_symtab || dump_dynamic_reloc_info)
    {
      dynsyms = slurp_dynamic_symtab (abfd);
    }
  if (dump_symtab)
    dump_symbols (abfd, false);
  if (dump_dynamic_symtab)
    dump_symbols (abfd, true);
  if (dump_stab_section_info)
    dump_stabs (abfd);
  if (dump_reloc_info && ! disassemble)
    dump_relocs (abfd);
  if (dump_dynamic_reloc_info)
    dump_dynamic_relocs (abfd);
  if (dump_section_contents)
    dump_data (abfd);
  if (disassemble)
    disassemble_data (abfd);
  if (dump_debugging)
    {
      PTR dhandle;

      dhandle = read_debugging_info (abfd, syms, symcount);
      if (dhandle != NULL)
	{
	  if (! print_debugging_info (stdout, dhandle))
	    {
	      non_fatal (_("%s: printing debugging information failed"),
			 bfd_get_filename (abfd));
	      exit_status = 1;
	    }
	}
    }
  if (syms)
    {
      free (syms);
      syms = NULL;
    }
  if (dynsyms)
    {
      free (dynsyms);
      dynsyms = NULL;
    }
}

static void
display_bfd (abfd)
     bfd *abfd;
{
  char **matching;

  if (bfd_check_format_matches (abfd, bfd_object, &matching))
    {
      dump_bfd (abfd);
      return;
    }

  if (bfd_get_error () == bfd_error_file_ambiguously_recognized)
    {
      nonfatal (bfd_get_filename (abfd));
      list_matching_formats (matching);
      free (matching);
      return;
    }

  if (bfd_get_error () != bfd_error_file_not_recognized)
    {
      nonfatal (bfd_get_filename (abfd));
      return;
    }

  if (bfd_check_format_matches (abfd, bfd_core, &matching))
    {
      dump_bfd (abfd);
      return;
    }

  nonfatal (bfd_get_filename (abfd));

  if (bfd_get_error () == bfd_error_file_ambiguously_recognized)
    {
      list_matching_formats (matching);
      free (matching);
    }
}

static void
display_file (filename, target)
     char *filename;
     char *target;
{
  bfd *file, *arfile = (bfd *) NULL;

  file = bfd_openr (filename, target);
  if (file == NULL)
    {
      nonfatal (filename);
      return;
    }

  if (bfd_check_format (file, bfd_archive) == true)
    {
      bfd *last_arfile = NULL;

      printf (_("In archive %s:\n"), bfd_get_filename (file));
      for (;;)
	{
	  bfd_set_error (bfd_error_no_error);

	  arfile = bfd_openr_next_archived_file (file, arfile);
	  if (arfile == NULL)
	    {
	      if (bfd_get_error () != bfd_error_no_more_archived_files)
		nonfatal (bfd_get_filename (file));
	      break;
	    }

	  display_bfd (arfile);

	  if (last_arfile != NULL)
	    bfd_close (last_arfile);
	  last_arfile = arfile;
	}

      if (last_arfile != NULL)
	bfd_close (last_arfile);
    }
  else
    display_bfd (file);

  bfd_close (file);
}

/* Actually display the various requested regions */

static void
dump_data (abfd)
     bfd *abfd;
{
  asection *section;
  bfd_byte *data = 0;
  bfd_size_type datasize = 0;
  bfd_size_type addr_offset;
  bfd_size_type start_offset, stop_offset;
  unsigned int opb = bfd_octets_per_byte (abfd);

  for (section = abfd->sections; section != NULL; section =
       section->next)
    {
      int onaline = 16;

      if (only == (char *) NULL ||
	  strcmp (only, section->name) == 0)
	{
	  if (section->flags & SEC_HAS_CONTENTS)
	    {
	      printf (_("Contents of section %s:\n"), section->name);

	      if (bfd_section_size (abfd, section) == 0)
		continue;
	      data = (bfd_byte *) xmalloc ((size_t) bfd_section_size (abfd, section));
	      datasize = bfd_section_size (abfd, section);


	      bfd_get_section_contents (abfd, section, (PTR) data, 0, bfd_section_size (abfd, section));

	      if (start_address == (bfd_vma) -1
		  || start_address < section->vma)
		start_offset = 0;
	      else
		start_offset = start_address - section->vma;
	      if (stop_address == (bfd_vma) -1)
		stop_offset = bfd_section_size (abfd, section) / opb;
	      else
		{
		  if (stop_address < section->vma)
		    stop_offset = 0;
		  else
		    stop_offset = stop_address - section->vma;
		  if (stop_offset > bfd_section_size (abfd, section) / opb)
		    stop_offset = bfd_section_size (abfd, section) / opb;
		}
	      for (addr_offset = start_offset; 
                   addr_offset < stop_offset; addr_offset += onaline)
		{
		  bfd_size_type j;

		  printf (" %04lx ", (unsigned long int) 
                          (addr_offset + section->vma));
		  for (j = addr_offset * opb; 
                       j < addr_offset * opb + onaline; j++)
		    {
		      if (j < stop_offset * opb)
			printf ("%02x", (unsigned) (data[j]));
		      else
			printf ("  ");
		      if ((j & 3) == 3)
			printf (" ");
		    }

		  printf (" ");
		  for (j = addr_offset; j < addr_offset * opb + onaline; j++)
		    {
		      if (j >= stop_offset * opb)
			printf (" ");
		      else
			printf ("%c", isprint (data[j]) ? data[j] : '.');
		    }
		  putchar ('\n');
		}
	      free (data);
	    }
	}
    }
}

/* Should perhaps share code and display with nm? */
static void
dump_symbols (abfd, dynamic)
     bfd *abfd ATTRIBUTE_UNUSED;
     boolean dynamic;
{
  asymbol **current;
  long max;
  long count;

  if (dynamic)
    {
      current = dynsyms;
      max = dynsymcount;
      if (max == 0)
	return;
      printf ("DYNAMIC SYMBOL TABLE:\n");
    }
  else
    {
      current = syms;
      max = symcount;
      if (max == 0)
	return;
      printf ("SYMBOL TABLE:\n");
    }

  for (count = 0; count < max; count++)
    {
      if (*current)
	{
	  bfd *cur_bfd = bfd_asymbol_bfd (*current);

	  if (cur_bfd != NULL)
	    {
	      const char *name;
	      char *alloc;

	      name = bfd_asymbol_name (*current);
	      alloc = NULL;
	      if (do_demangle && name != NULL && *name != '\0')
		{
		  const char *n;

		  /* If we want to demangle the name, we demangle it
                     here, and temporarily clobber it while calling
                     bfd_print_symbol.  FIXME: This is a gross hack.  */

		  n = name;
		  if (bfd_get_symbol_leading_char (cur_bfd) == *n)
		    ++n;
		  alloc = cplus_demangle (n, DMGL_ANSI | DMGL_PARAMS);
		  if (alloc != NULL)
		    (*current)->name = alloc;
		  else
		    (*current)->name = n;
		}

	      bfd_print_symbol (cur_bfd, stdout, *current,
				bfd_print_symbol_all);

	      (*current)->name = name;
	      if (alloc != NULL)
		free (alloc);

	      printf ("\n");
	    }
	}
      current++;
    }
  printf ("\n");
  printf ("\n");
}

static void
dump_relocs (abfd)
     bfd *abfd;
{
  arelent **relpp;
  long relcount;
  asection *a;

  for (a = abfd->sections; a != (asection *) NULL; a = a->next)
    {
      long relsize;

      if (bfd_is_abs_section (a))
	continue;
      if (bfd_is_und_section (a))
	continue;
      if (bfd_is_com_section (a))
	continue;

      if (only)
	{
	  if (strcmp (only, a->name))
	    continue;
	}
      else if ((a->flags & SEC_RELOC) == 0)
	continue;

      relsize = bfd_get_reloc_upper_bound (abfd, a);
      if (relsize < 0)
	bfd_fatal (bfd_get_filename (abfd));

      printf ("RELOCATION RECORDS FOR [%s]:", a->name);

      if (relsize == 0)
	{
	  printf (" (none)\n\n");
	}
      else
	{
	  relpp = (arelent **) xmalloc (relsize);
	  relcount = bfd_canonicalize_reloc (abfd, a, relpp, syms);
	  if (relcount < 0)
	    bfd_fatal (bfd_get_filename (abfd));
	  else if (relcount == 0)
	    {
	      printf (" (none)\n\n");
	    }
	  else
	    {
	      printf ("\n");
	      dump_reloc_set (abfd, a, relpp, relcount);
	      printf ("\n\n");
	    }
	  free (relpp);
	}
    }
}

static void
dump_dynamic_relocs (abfd)
     bfd *abfd;
{
  long relsize;
  arelent **relpp;
  long relcount;

  relsize = bfd_get_dynamic_reloc_upper_bound (abfd);
  if (relsize < 0)
    bfd_fatal (bfd_get_filename (abfd));

  printf ("DYNAMIC RELOCATION RECORDS");

  if (relsize == 0)
    {
      printf (" (none)\n\n");
    }
  else
    {
      relpp = (arelent **) xmalloc (relsize);
      relcount = bfd_canonicalize_dynamic_reloc (abfd, relpp, dynsyms);
      if (relcount < 0)
	bfd_fatal (bfd_get_filename (abfd));
      else if (relcount == 0)
	{
	  printf (" (none)\n\n");
	}
      else
	{
	  printf ("\n");
	  dump_reloc_set (abfd, (asection *) NULL, relpp, relcount);
	  printf ("\n\n");
	}
      free (relpp);
    }
}

static void
dump_reloc_set (abfd, sec, relpp, relcount)
     bfd *abfd;
     asection *sec;
     arelent **relpp;
     long relcount;
{
  arelent **p;
  char *last_filename, *last_functionname;
  unsigned int last_line;

  /* Get column headers lined up reasonably.  */
  {
    static int width;
    if (width == 0)
      {
	char buf[30];
	sprintf_vma (buf, (bfd_vma) -1);
	width = strlen (buf) - 7;
      }
    printf ("OFFSET %*s TYPE %*s VALUE \n", width, "", 12, "");
  }

  last_filename = NULL;
  last_functionname = NULL;
  last_line = 0;

  for (p = relpp; relcount && *p != (arelent *) NULL; p++, relcount--)
    {
      arelent *q = *p;
      const char *filename, *functionname;
      unsigned int line;
      const char *sym_name;
      const char *section_name;

      if (start_address != (bfd_vma) -1
	  && q->address < start_address)
	continue;
      if (stop_address != (bfd_vma) -1
	  && q->address > stop_address)
	continue;

      if (with_line_numbers
	  && sec != NULL
	  && bfd_find_nearest_line (abfd, sec, syms, q->address,
				    &filename, &functionname, &line))
	{
	  if (functionname != NULL
	      && (last_functionname == NULL
		  || strcmp (functionname, last_functionname) != 0))
	    {
	      printf ("%s():\n", functionname);
	      if (last_functionname != NULL)
		free (last_functionname);
	      last_functionname = xstrdup (functionname);
	    }
	  if (line > 0
	      && (line != last_line
		  || (filename != NULL
		      && last_filename != NULL
		      && strcmp (filename, last_filename) != 0)))
	    {
	      printf ("%s:%u\n", filename == NULL ? "???" : filename, line);
	      last_line = line;
	      if (last_filename != NULL)
		free (last_filename);
	      if (filename == NULL)
		last_filename = NULL;
	      else
		last_filename = xstrdup (filename);
	    }
	}

      if (q->sym_ptr_ptr && *q->sym_ptr_ptr)
	{
	  sym_name = (*(q->sym_ptr_ptr))->name;
	  section_name = (*(q->sym_ptr_ptr))->section->name;
	}
      else
	{
	  sym_name = NULL;
	  section_name = NULL;
	}
      if (sym_name)
	{
	  printf_vma (q->address);
	  if (q->howto->name)
	    printf (" %-16s  ", q->howto->name);
	  else
	    printf (" %-16d  ", q->howto->type);
	  objdump_print_symname (abfd, (struct disassemble_info *) NULL,
				 *q->sym_ptr_ptr);
	}
      else
	{
	  if (section_name == (CONST char *) NULL)
	    section_name = "*unknown*";
	  printf_vma (q->address);
	  printf (" %-16s  [%s]",
		  q->howto->name,
		  section_name);
	}
      if (q->addend)
	{
	  printf ("+0x");
	  printf_vma (q->addend);
	}
      printf ("\n");
    }
}

/* The length of the longest architecture name + 1.  */
#define LONGEST_ARCH sizeof("powerpc:common")

static const char *
endian_string (endian)
     enum bfd_endian endian;
{
  if (endian == BFD_ENDIAN_BIG)
    return "big endian";
  else if (endian == BFD_ENDIAN_LITTLE)
    return "little endian";
  else
    return "endianness unknown";
}

/* List the targets that BFD is configured to support, each followed
   by its endianness and the architectures it supports.  */

static void
display_target_list ()
{
  extern bfd_target *bfd_target_vector[];
  char *dummy_name;
  int t;

  dummy_name = make_temp_file (NULL);
  for (t = 0; bfd_target_vector[t]; t++)
    {
      bfd_target *p = bfd_target_vector[t];
      bfd *abfd = bfd_openw (dummy_name, p->name);
      int a;

      printf ("%s\n (header %s, data %s)\n", p->name,
	      endian_string (p->header_byteorder),
	      endian_string (p->byteorder));

      if (abfd == NULL)
	{
	  nonfatal (dummy_name);
	  continue;
	}

      if (! bfd_set_format (abfd, bfd_object))
	{
	  if (bfd_get_error () != bfd_error_invalid_operation)
	    nonfatal (p->name);
	  bfd_close_all_done (abfd);
	  continue;
	}

      for (a = (int) bfd_arch_obscure + 1; a < (int) bfd_arch_last; a++)
	if (bfd_set_arch_mach (abfd, (enum bfd_architecture) a, 0))
	  printf ("  %s\n",
		  bfd_printable_arch_mach ((enum bfd_architecture) a, 0));
      bfd_close_all_done (abfd);
    }
  unlink (dummy_name);
  free (dummy_name);
}

/* Print a table showing which architectures are supported for entries
   FIRST through LAST-1 of bfd_target_vector (targets across,
   architectures down).  */

static void
display_info_table (first, last)
     int first;
     int last;
{
  extern bfd_target *bfd_target_vector[];
  int t, a;
  char *dummy_name;

  /* Print heading of target names.  */
  printf ("\n%*s", (int) LONGEST_ARCH, " ");
  for (t = first; t < last && bfd_target_vector[t]; t++)
    printf ("%s ", bfd_target_vector[t]->name);
  putchar ('\n');

  dummy_name = make_temp_file (NULL);
  for (a = (int) bfd_arch_obscure + 1; a < (int) bfd_arch_last; a++)
    if (strcmp (bfd_printable_arch_mach (a, 0), "UNKNOWN!") != 0)
      {
	printf ("%*s ", (int) LONGEST_ARCH - 1,
		bfd_printable_arch_mach (a, 0));
	for (t = first; t < last && bfd_target_vector[t]; t++)
	  {
	    bfd_target *p = bfd_target_vector[t];
	    boolean ok = true;
	    bfd *abfd = bfd_openw (dummy_name, p->name);

	    if (abfd == NULL)
	      {
		nonfatal (p->name);
		ok = false;
	      }

	    if (ok)
	      {
		if (! bfd_set_format (abfd, bfd_object))
		  {
		    if (bfd_get_error () != bfd_error_invalid_operation)
		      nonfatal (p->name);
		    ok = false;
		  }
	      }

	    if (ok)
	      {
		if (! bfd_set_arch_mach (abfd, a, 0))
		  ok = false;
	      }

	    if (ok)
	      printf ("%s ", p->name);
	    else
	      {
		int l = strlen (p->name);
		while (l--)
		  putchar ('-');
		putchar (' ');
	      }
	    if (abfd != NULL)
	      bfd_close_all_done (abfd);
	  }
	putchar ('\n');
      }
  unlink (dummy_name);
  free (dummy_name);
}

/* Print tables of all the target-architecture combinations that
   BFD has been configured to support.  */

static void
display_target_tables ()
{
  int t, columns;
  extern bfd_target *bfd_target_vector[];
  char *colum;

  columns = 0;
  colum = getenv ("COLUMNS");
  if (colum != NULL)
    columns = atoi (colum);
  if (columns == 0)
    columns = 80;

  t = 0;
  while (bfd_target_vector[t] != NULL)
    {
      int oldt = t, wid;

      wid = LONGEST_ARCH + strlen (bfd_target_vector[t]->name) + 1;
      ++t;
      while (wid < columns && bfd_target_vector[t] != NULL)
	{
	  int newwid;

	  newwid = wid + strlen (bfd_target_vector[t]->name) + 1;
	  if (newwid >= columns)
	    break;
	  wid = newwid;
	  ++t;
	}
      display_info_table (oldt, t);
    }
}

static void
display_info ()
{
  printf (_("BFD header file version %s\n"), BFD_VERSION);
  display_target_list ();
  display_target_tables ();
}

int
main (argc, argv)
     int argc;
     char **argv;
{
  int c;
  char *target = default_target;
  boolean seenflag = false;

#if defined (HAVE_SETLOCALE) && defined (HAVE_LC_MESSAGES)
  setlocale (LC_MESSAGES, "");
#endif
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  program_name = *argv;
  xmalloc_set_program_name (program_name);

  START_PROGRESS (program_name, 0);

  bfd_init ();
  set_default_bfd_target ();

  while ((c = getopt_long (argc, argv, "pib:m:M:VCdDlfahHrRtTxsSj:wE:zgG",
			   long_options, (int *) 0))
	 != EOF)
    {
      switch (c)
	{
	case 0:
	  break;		/* we've been given a long option */
	case 'm':
	  machine = optarg;
	  break;
	case 'M':
	  disassembler_options = optarg;
	  break;
	case 'j':
	  only = optarg;
	  break;
	case 'l':
	  with_line_numbers = true;
	  break;
	case 'b':
	  target = optarg;
	  break;
	case 'C':
	  do_demangle = true;
	  if (optarg != NULL)
	    {
	      enum demangling_styles style;
	      
	      style = cplus_demangle_name_to_style (optarg);
	      if (style == unknown_demangling) 
		fatal (_("unknown demangling style `%s'"),
		       optarg);
	      
	      cplus_demangle_set_style (style);
           }
	  break;
	case 'w':
	  wide_output = true;
	  break;
	case OPTION_ADJUST_VMA:
	  adjust_section_vma = parse_vma (optarg, "--adjust-vma");
	  break;
	case OPTION_START_ADDRESS:
	  start_address = parse_vma (optarg, "--start-address");
	  break;
	case OPTION_STOP_ADDRESS:
	  stop_address = parse_vma (optarg, "--stop-address");
	  break;
	case 'E':
	  if (strcmp (optarg, "B") == 0)
	    endian = BFD_ENDIAN_BIG;
	  else if (strcmp (optarg, "L") == 0)
	    endian = BFD_ENDIAN_LITTLE;
	  else
	    {
	      non_fatal (_("unrecognized -E option"));
	      usage (stderr, 1);
	    }
	  break;
	case OPTION_ENDIAN:
	  if (strncmp (optarg, "big", strlen (optarg)) == 0)
	    endian = BFD_ENDIAN_BIG;
	  else if (strncmp (optarg, "little", strlen (optarg)) == 0)
	    endian = BFD_ENDIAN_LITTLE;
	  else
	    {
	      non_fatal (_("unrecognized --endian type `%s'"), optarg);
	      usage (stderr, 1);
	    }
	  break;
	  
	case 'f':
	  dump_file_header = true;
	  seenflag = true;
	  break;
	case 'i':
	  formats_info = true;
	  seenflag = true;
	  break;
	case 'p':
	  dump_private_headers = true;
	  seenflag = true;
	  break;
	case 'x':
	  dump_private_headers = true;
	  dump_symtab = true;
	  dump_reloc_info = true;
	  dump_file_header = true;
	  dump_ar_hdrs = true;
	  dump_section_headers = true;
	  seenflag = true;
	  break;
	case 't':
	  dump_symtab = true;
	  seenflag = true;
	  break;
	case 'T':
	  dump_dynamic_symtab = true;
	  seenflag = true;
	  break;
	case 'd':
	  disassemble = true;
	  seenflag = true;
	  break;
	case 'z':
	  disassemble_zeroes = true;
	  break;
	case 'D':
	  disassemble = true;
	  disassemble_all = true;
	  seenflag = true;
	  break;
	case 'S':
	  disassemble = true;
	  with_source_code = true;
	  seenflag = true;
	  break;
	case 'g':
	  dump_debugging = 1;
	  seenflag = true;
	  break;
	case 'G':
	  dump_stab_section_info = true;
	  seenflag = true;
	  break;
	case 's':
	  dump_section_contents = true;
	  seenflag = true;
	  break;
	case 'r':
	  dump_reloc_info = true;
	  seenflag = true;
	  break;
	case 'R':
	  dump_dynamic_reloc_info = true;
	  seenflag = true;
	  break;
	case 'a':
	  dump_ar_hdrs = true;
	  seenflag = true;
	  break;
	case 'h':
	  dump_section_headers = true;
	  seenflag = true;
	  break;
	case 'H':
	  usage (stdout, 0);
	  seenflag = true;
	case 'V':
	  show_version = true;
	  seenflag = true;
	  break;
	  
	default:
	  usage (stderr, 1);
	}
    }

  if (show_version)
    print_version ("objdump");

  if (seenflag == false)
    usage (stderr, 2);

  if (formats_info)
    display_info ();
  else
    {
      if (optind == argc)
	display_file ("a.out", target);
      else
	for (; optind < argc;)
	  display_file (argv[optind++], target);
    }

  END_PROGRESS (program_name);

  return exit_status;
}
