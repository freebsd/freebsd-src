/* objcopy.c -- copy object file from input to output, optionally massaging it.
   Copyright (C) 1991, 92, 93, 94, 95, 96, 97, 1998
   Free Software Foundation, Inc.

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

#include "bfd.h"
#include "progress.h"
#include "bucomm.h"
#include "getopt.h"
#include "libiberty.h"
#include "budbg.h"
#include <sys/stat.h>

#ifdef HAVE_GOOD_UTIME_H
#include <utime.h>
#else /* ! HAVE_GOOD_UTIME_H */
#ifdef HAVE_UTIMES
#include <sys/time.h>
#endif /* HAVE_UTIMES */
#endif /* ! HAVE_GOOD_UTIME_H */

/* A list of symbols to explicitly strip out, or to keep.  A linked
   list is good enough for a small number from the command line, but
   this will slow things down a lot if many symbols are being
   deleted. */

struct symlist
{
  const char *name;
  struct symlist *next;
};

static void copy_usage PARAMS ((FILE *, int));
static void strip_usage PARAMS ((FILE *, int));
static flagword parse_flags PARAMS ((const char *));
static struct section_list *find_section_list PARAMS ((const char *, boolean));
static void setup_section PARAMS ((bfd *, asection *, PTR));
static void copy_section PARAMS ((bfd *, asection *, PTR));
static void get_sections PARAMS ((bfd *, asection *, PTR));
static int compare_section_lma PARAMS ((const PTR, const PTR));
static void add_specific_symbol PARAMS ((const char *, struct symlist **));
static boolean is_specified_symbol PARAMS ((const char *, struct symlist *));
static boolean is_strip_section PARAMS ((bfd *, asection *));
static unsigned int filter_symbols
  PARAMS ((bfd *, bfd *, asymbol **, asymbol **, long));
static void mark_symbols_used_in_relocations PARAMS ((bfd *, asection *, PTR));
static void filter_bytes PARAMS ((char *, bfd_size_type *));
static boolean write_debugging_info PARAMS ((bfd *, PTR, long *, asymbol ***));
static void copy_object PARAMS ((bfd *, bfd *));
static void copy_archive PARAMS ((bfd *, bfd *, const char *));
static void copy_file
  PARAMS ((const char *, const char *, const char *, const char *));
static int simple_copy PARAMS ((const char *, const char *));
static int smart_rename PARAMS ((const char *, const char *));
static void set_times PARAMS ((const char *, const struct stat *));
static int strip_main PARAMS ((int, char **));
static int copy_main PARAMS ((int, char **));

#define nonfatal(s) {bfd_nonfatal(s); status = 1; return;}

static asymbol **isympp = NULL;	/* Input symbols */
static asymbol **osympp = NULL;	/* Output symbols that survive stripping */

/* If `copy_byte' >= 0, copy only that byte of every `interleave' bytes.  */
static int copy_byte = -1;
static int interleave = 4;

static boolean verbose;		/* Print file and target names. */
static int status = 0;		/* Exit status.  */

enum strip_action
  {
    strip_undef,
    strip_none,			/* don't strip */
    strip_debug,		/* strip all debugger symbols */
    strip_unneeded,		/* strip unnecessary symbols */
    strip_all			/* strip all symbols */
  };

/* Which symbols to remove. */
static enum strip_action strip_symbols;

enum locals_action
  {
    locals_undef,
    locals_start_L,		/* discard locals starting with L */
    locals_all			/* discard all locals */
  };

/* Which local symbols to remove.  Overrides strip_all.  */
static enum locals_action discard_locals;

/* Structure used to hold lists of sections and actions to take.  */

struct section_list
{
  /* Next section to adjust.  */
  struct section_list *next;
  /* Section name.  */
  const char *name;
  /* Whether this entry was used.  */
  boolean used;
  /* Whether to remove this section.  */
  boolean remove;
  /* Whether to adjust or set VMA.  */
  enum { ignore_vma, adjust_vma, set_vma } adjust;
  /* Amount to adjust by or set to.  */
  bfd_vma val;
  /* Whether to set the section flags.  */
  boolean set_flags;
  /* What to set the section flags to.  */
  flagword flags;
};

static struct section_list *adjust_sections;
static boolean sections_removed;

/* Adjustments to the start address.  */
static bfd_vma adjust_start = 0;
static boolean set_start_set = false;
static bfd_vma set_start;

/* Adjustments to section VMA's.  */
static bfd_vma adjust_section_vma = 0;

/* Filling gaps between sections.  */
static boolean gap_fill_set = false;
static bfd_byte gap_fill = 0;

/* Pad to a given address.  */
static boolean pad_to_set = false;
static bfd_vma pad_to;

/* List of sections to add.  */

struct section_add
{
  /* Next section to add.  */
  struct section_add *next;
  /* Name of section to add.  */
  const char *name;
  /* Name of file holding section contents.  */
  const char *filename;
  /* Size of file.  */
  size_t size;
  /* Contents of file.  */
  bfd_byte *contents;
  /* BFD section, after it has been added.  */
  asection *section;
};

static struct section_add *add_sections;

/* Whether to convert debugging information.  */

static boolean convert_debugging = false;

/* Whether to change the leading character in symbol names.  */

static boolean change_leading_char = false;

/* Whether to remove the leading character from global symbol names.  */

static boolean remove_leading_char = false;

/* List of symbols to strip, keep, localize, and weaken.  */

static struct symlist *strip_specific_list = NULL;
static struct symlist *keep_specific_list = NULL;
static struct symlist *localize_specific_list = NULL;
static struct symlist *weaken_specific_list = NULL;

/* If this is true, we weaken global symbols (set BSF_WEAK).  */

static boolean weaken = false;

/* 150 isn't special; it's just an arbitrary non-ASCII char value.  */

#define OPTION_ADD_SECTION 150
#define OPTION_ADJUST_START (OPTION_ADD_SECTION + 1)
#define OPTION_ADJUST_VMA (OPTION_ADJUST_START + 1)
#define OPTION_ADJUST_SECTION_VMA (OPTION_ADJUST_VMA + 1)
#define OPTION_ADJUST_WARNINGS (OPTION_ADJUST_SECTION_VMA + 1)
#define OPTION_CHANGE_LEADING_CHAR (OPTION_ADJUST_WARNINGS + 1)
#define OPTION_DEBUGGING (OPTION_CHANGE_LEADING_CHAR + 1)
#define OPTION_GAP_FILL (OPTION_DEBUGGING + 1)
#define OPTION_NO_ADJUST_WARNINGS (OPTION_GAP_FILL + 1)
#define OPTION_PAD_TO (OPTION_NO_ADJUST_WARNINGS + 1)
#define OPTION_REMOVE_LEADING_CHAR (OPTION_PAD_TO + 1)
#define OPTION_SET_SECTION_FLAGS (OPTION_REMOVE_LEADING_CHAR + 1)
#define OPTION_SET_START (OPTION_SET_SECTION_FLAGS + 1)
#define OPTION_STRIP_UNNEEDED (OPTION_SET_START + 1)
#define OPTION_WEAKEN (OPTION_STRIP_UNNEEDED + 1)

/* Options to handle if running as "strip".  */

static struct option strip_options[] =
{
  {"discard-all", no_argument, 0, 'x'},
  {"discard-locals", no_argument, 0, 'X'},
  {"format", required_argument, 0, 'F'}, /* Obsolete */
  {"help", no_argument, 0, 'h'},
  {"input-format", required_argument, 0, 'I'}, /* Obsolete */
  {"input-target", required_argument, 0, 'I'},
  {"keep-symbol", required_argument, 0, 'K'},
  {"output-format", required_argument, 0, 'O'},	/* Obsolete */
  {"output-target", required_argument, 0, 'O'},
  {"preserve-dates", no_argument, 0, 'p'},
  {"remove-section", required_argument, 0, 'R'},
  {"strip-all", no_argument, 0, 's'},
  {"strip-debug", no_argument, 0, 'S'},
  {"strip-unneeded", no_argument, 0, OPTION_STRIP_UNNEEDED},
  {"strip-symbol", required_argument, 0, 'N'},
  {"target", required_argument, 0, 'F'},
  {"verbose", no_argument, 0, 'v'},
  {"version", no_argument, 0, 'V'},
  {0, no_argument, 0, 0}
};

/* Options to handle if running as "objcopy".  */

static struct option copy_options[] =
{
  {"add-section", required_argument, 0, OPTION_ADD_SECTION},
  {"adjust-start", required_argument, 0, OPTION_ADJUST_START},
  {"adjust-vma", required_argument, 0, OPTION_ADJUST_VMA},
  {"adjust-section-vma", required_argument, 0, OPTION_ADJUST_SECTION_VMA},
  {"adjust-warnings", no_argument, 0, OPTION_ADJUST_WARNINGS},
  {"byte", required_argument, 0, 'b'},
  {"change-leading-char", no_argument, 0, OPTION_CHANGE_LEADING_CHAR},
  {"debugging", no_argument, 0, OPTION_DEBUGGING},
  {"discard-all", no_argument, 0, 'x'},
  {"discard-locals", no_argument, 0, 'X'},
  {"format", required_argument, 0, 'F'}, /* Obsolete */
  {"gap-fill", required_argument, 0, OPTION_GAP_FILL},
  {"help", no_argument, 0, 'h'},
  {"input-format", required_argument, 0, 'I'}, /* Obsolete */
  {"input-target", required_argument, 0, 'I'},
  {"interleave", required_argument, 0, 'i'},
  {"keep-symbol", required_argument, 0, 'K'},
  {"no-adjust-warnings", no_argument, 0, OPTION_NO_ADJUST_WARNINGS},
  {"output-format", required_argument, 0, 'O'},	/* Obsolete */
  {"output-target", required_argument, 0, 'O'},
  {"pad-to", required_argument, 0, OPTION_PAD_TO},
  {"preserve-dates", no_argument, 0, 'p'},
  {"localize-symbol", required_argument, 0, 'L'},
  {"remove-leading-char", no_argument, 0, OPTION_REMOVE_LEADING_CHAR},
  {"remove-section", required_argument, 0, 'R'},
  {"set-section-flags", required_argument, 0, OPTION_SET_SECTION_FLAGS},
  {"set-start", required_argument, 0, OPTION_SET_START},
  {"strip-all", no_argument, 0, 'S'},
  {"strip-debug", no_argument, 0, 'g'},
  {"strip-unneeded", no_argument, 0, OPTION_STRIP_UNNEEDED},
  {"strip-symbol", required_argument, 0, 'N'},
  {"target", required_argument, 0, 'F'},
  {"verbose", no_argument, 0, 'v'},
  {"version", no_argument, 0, 'V'},
  {"weaken", no_argument, 0, OPTION_WEAKEN},
  {"weaken-symbol", required_argument, 0, 'W'},
  {0, no_argument, 0, 0}
};

/* IMPORTS */
extern char *program_name;

/* This flag distinguishes between strip and objcopy:
   1 means this is 'strip'; 0 means this is 'objcopy'.
   -1 means if we should use argv[0] to decide. */
extern int is_strip;


static void
copy_usage (stream, exit_status)
     FILE *stream;
     int exit_status;
{
  fprintf (stream, "\
Usage: %s [-vVSpgxX] [-I bfdname] [-O bfdname] [-F bfdname] [-b byte]\n\
       [-R section] [-i interleave] [--interleave=interleave] [--byte=byte]\n\
       [--input-target=bfdname] [--output-target=bfdname] [--target=bfdname]\n\
       [--strip-all] [--strip-debug] [--strip-unneeded] [--discard-all]\n\
       [--discard-locals] [--debugging] [--remove-section=section]\n",
	   program_name);
  fprintf (stream, "\
       [--gap-fill=val] [--pad-to=address] [--preserve-dates]\n\
       [--set-start=val] [--adjust-start=incr]\n\
       [--adjust-vma=incr] [--adjust-section-vma=section{=,+,-}val]\n\
       [--adjust-warnings] [--no-adjust-warnings]\n\
       [--set-section-flags=section=flags] [--add-section=sectionname=filename]\n\
       [--keep-symbol symbol] [-K symbol] [--strip-symbol symbol] [-N symbol]\n\
       [--localize-symbol symbol] [-L symbol] [--weaken-symbol symbol]\n\
       [-W symbol] [--change-leading-char] [--remove-leading-char] [--weaken]\n\
       [--verbose] [--version] [--help] in-file [out-file]\n");
  list_supported_targets (program_name, stream);
  if (exit_status == 0)
    fprintf (stream, "Report bugs to bug-gnu-utils@gnu.org\n");
  exit (exit_status);
}

static void
strip_usage (stream, exit_status)
     FILE *stream;
     int exit_status;
{
  fprintf (stream, "\
Usage: %s [-vVsSpgxX] [-I bfdname] [-O bfdname] [-F bfdname] [-R section]\n\
       [--input-target=bfdname] [--output-target=bfdname] [--target=bfdname]\n\
       [--strip-all] [--strip-debug] [--strip-unneeded] [--discard-all]\n\
       [--discard-locals] [--keep-symbol symbol] [-K symbol]\n\
       [--strip-symbol symbol] [-N symbol] [--remove-section=section]\n\
       [-o file] [--preserve-dates] [--verbose] [--version] [--help] file...\n",
	   program_name);
  list_supported_targets (program_name, stream);
  if (exit_status == 0)
    fprintf (stream, "Report bugs to bug-gnu-utils@gnu.org\n");
  exit (exit_status);
}

/* Parse section flags into a flagword, with a fatal error if the
   string can't be parsed.  */

static flagword
parse_flags (s)
     const char *s;
{
  flagword ret;
  const char *snext;
  int len;

  ret = SEC_NO_FLAGS;

  do
    {
      snext = strchr (s, ',');
      if (snext == NULL)
	len = strlen (s);
      else
	{
	  len = snext - s;
	  ++snext;
	}

      if (0) ;
#define PARSE_FLAG(fname,fval) \
  else if (strncasecmp (fname, s, len) == 0) ret |= fval
      PARSE_FLAG ("alloc", SEC_ALLOC);
      PARSE_FLAG ("load", SEC_LOAD);
      PARSE_FLAG ("readonly", SEC_READONLY);
      PARSE_FLAG ("code", SEC_CODE);
      PARSE_FLAG ("data", SEC_DATA);
      PARSE_FLAG ("rom", SEC_ROM);
      PARSE_FLAG ("contents", SEC_HAS_CONTENTS);
#undef PARSE_FLAG
      else
	{
	  char *copy;

	  copy = xmalloc (len + 1);
	  strncpy (copy, s, len);
	  copy[len] = '\0';
	  fprintf (stderr, "%s: unrecognized section flag `%s'\n",
		   program_name, copy);
	  fprintf (stderr,
		   "%s: supported flags: alloc, load, readonly, code, data, rom, contents\n",
		   program_name);
	  exit (1);
	}

      s = snext;
    }
  while (s != NULL);

  return ret;
}

/* Find and optionally add an entry in the adjust_sections list.  */

static struct section_list *
find_section_list (name, add)
     const char *name;
     boolean add;
{
  register struct section_list *p;

  for (p = adjust_sections; p != NULL; p = p->next)
    if (strcmp (p->name, name) == 0)
      return p;

  if (! add)
    return NULL;

  p = (struct section_list *) xmalloc (sizeof (struct section_list));
  p->name = name;
  p->used = false;
  p->remove = false;
  p->adjust = ignore_vma;
  p->val = 0;
  p->set_flags = false;
  p->flags = 0;

  p->next = adjust_sections;
  adjust_sections = p;

  return p;
}

/* Add a symbol to strip_specific_list.  */

static void 
add_specific_symbol (name, list)
     const char *name;
     struct symlist **list;
{
  struct symlist *tmp_list;

  tmp_list = (struct symlist *) xmalloc (sizeof (struct symlist));
  tmp_list->name = name;
  tmp_list->next = *list;
  *list = tmp_list;
}

/* See whether a symbol should be stripped or kept based on
   strip_specific_list and keep_symbols.  */

static boolean
is_specified_symbol (name, list)
     const char *name;
     struct symlist *list;
{
  struct symlist *tmp_list;

  for (tmp_list = list; tmp_list; tmp_list = tmp_list->next)
    {
      if (strcmp (name, tmp_list->name) == 0)
	return true;
    }
  return false;
}

/* See if a section is being removed.  */

static boolean
is_strip_section (abfd, sec)
     bfd *abfd;
     asection *sec;
{
  struct section_list *p;

  if ((bfd_get_section_flags (abfd, sec) & SEC_DEBUGGING) != 0
      && (strip_symbols == strip_debug
	  || strip_symbols == strip_unneeded
	  || strip_symbols == strip_all
	  || discard_locals == locals_all
	  || convert_debugging))
    return true;

  if (! sections_removed)
    return false;
  p = find_section_list (bfd_get_section_name (abfd, sec), false);
  return p != NULL && p->remove ? true : false;
}

/* Choose which symbol entries to copy; put the result in OSYMS.
   We don't copy in place, because that confuses the relocs.
   Return the number of symbols to print.  */

static unsigned int
filter_symbols (abfd, obfd, osyms, isyms, symcount)
     bfd *abfd;
     bfd *obfd;
     asymbol **osyms, **isyms;
     long symcount;
{
  register asymbol **from = isyms, **to = osyms;
  long src_count = 0, dst_count = 0;

  for (; src_count < symcount; src_count++)
    {
      asymbol *sym = from[src_count];
      flagword flags = sym->flags;
      const char *name = bfd_asymbol_name (sym);
      int keep;

      if (change_leading_char
	  && (bfd_get_symbol_leading_char (abfd)
	      != bfd_get_symbol_leading_char (obfd))
	  && (bfd_get_symbol_leading_char (abfd) == '\0'
	      || (name[0] == bfd_get_symbol_leading_char (abfd))))
	{
	  if (bfd_get_symbol_leading_char (obfd) == '\0')
	    name = bfd_asymbol_name (sym) = name + 1;
	  else
	    {
	      char *n;

	      n = xmalloc (strlen (name) + 2);
	      n[0] = bfd_get_symbol_leading_char (obfd);
	      if (bfd_get_symbol_leading_char (abfd) == '\0')
		strcpy (n + 1, name);
	      else
		strcpy (n + 1, name + 1);
	      name = bfd_asymbol_name (sym) = n;
	    }
	}

      if (remove_leading_char
	  && ((flags & BSF_GLOBAL) != 0
	      || (flags & BSF_WEAK) != 0
	      || bfd_is_und_section (bfd_get_section (sym))
	      || bfd_is_com_section (bfd_get_section (sym)))
	  && name[0] == bfd_get_symbol_leading_char (abfd))
	name = bfd_asymbol_name (sym) = name + 1;

      if ((flags & BSF_KEEP) != 0)		/* Used in relocation.  */
	keep = 1;
      else if ((flags & BSF_GLOBAL) != 0	/* Global symbol.  */
	       || (flags & BSF_WEAK) != 0
	       || bfd_is_und_section (bfd_get_section (sym))
	       || bfd_is_com_section (bfd_get_section (sym)))
	keep = strip_symbols != strip_unneeded;
      else if ((flags & BSF_DEBUGGING) != 0)	/* Debugging symbol.  */
	keep = (strip_symbols != strip_debug
		&& strip_symbols != strip_unneeded
		&& ! convert_debugging);
      else			/* Local symbol.  */
	keep = (strip_symbols != strip_unneeded
		&& (discard_locals != locals_all
		    && (discard_locals != locals_start_L
			|| ! bfd_is_local_label (abfd, sym))));

      if (keep && is_specified_symbol (name, strip_specific_list))
	keep = 0;
      if (!keep && is_specified_symbol (name, keep_specific_list))
	keep = 1;
      if (keep && is_strip_section (abfd, bfd_get_section (sym)))
	keep = 0;

      if (keep && (flags & BSF_GLOBAL) != 0
	  && (weaken || is_specified_symbol (name, weaken_specific_list)))
	{
	  sym->flags &=~ BSF_GLOBAL;
	  sym->flags |= BSF_WEAK;
	}
      if (keep && (flags & (BSF_GLOBAL | BSF_WEAK))
	  && is_specified_symbol (name, localize_specific_list))
	{
	  sym->flags &= ~(BSF_GLOBAL | BSF_WEAK);
	  sym->flags |= BSF_LOCAL;
	}

      if (keep)
	to[dst_count++] = sym;
    }

  to[dst_count] = NULL;

  return dst_count;
}

/* Keep only every `copy_byte'th byte in MEMHUNK, which is *SIZE bytes long.
   Adjust *SIZE.  */

static void
filter_bytes (memhunk, size)
     char *memhunk;
     bfd_size_type *size;
{
  char *from = memhunk + copy_byte, *to = memhunk, *end = memhunk + *size;

  for (; from < end; from += interleave)
    *to++ = *from;
  *size /= interleave;
}

/* Copy object file IBFD onto OBFD.  */

static void
copy_object (ibfd, obfd)
     bfd *ibfd;
     bfd *obfd;
{
  bfd_vma start;
  long symcount;
  asection **osections = NULL;
  bfd_size_type *gaps = NULL;
  bfd_size_type max_gap = 0;

  if (!bfd_set_format (obfd, bfd_get_format (ibfd)))
    {
      nonfatal (bfd_get_filename (obfd));
    }

  if (verbose)
    printf ("copy from %s(%s) to %s(%s)\n",
	    bfd_get_filename(ibfd), bfd_get_target(ibfd),
	    bfd_get_filename(obfd), bfd_get_target(obfd));

  if (set_start_set)
    start = set_start;
  else
    start = bfd_get_start_address (ibfd);
  start += adjust_start;

  if (!bfd_set_start_address (obfd, start)
      || !bfd_set_file_flags (obfd,
			      (bfd_get_file_flags (ibfd)
			       & bfd_applicable_file_flags (obfd))))
    {
      nonfatal (bfd_get_filename (ibfd));
    }

  /* Copy architecture of input file to output file */
  if (!bfd_set_arch_mach (obfd, bfd_get_arch (ibfd),
			  bfd_get_mach (ibfd)))
    {
      fprintf (stderr,
	       "Warning: Output file cannot represent architecture %s\n",
	       bfd_printable_arch_mach (bfd_get_arch (ibfd),
					bfd_get_mach (ibfd)));
    }
  if (!bfd_set_format (obfd, bfd_get_format (ibfd)))
    {
      nonfatal (bfd_get_filename(ibfd));
    }

  if (isympp)
    free (isympp);
  if (osympp != isympp)
    free (osympp);

  /* bfd mandates that all output sections be created and sizes set before
     any output is done.  Thus, we traverse all sections multiple times.  */
  bfd_map_over_sections (ibfd, setup_section, (void *) obfd);

  if (add_sections != NULL)
    {
      struct section_add *padd;
      struct section_list *pset;

      for (padd = add_sections; padd != NULL; padd = padd->next)
	{
	  padd->section = bfd_make_section (obfd, padd->name);
	  if (padd->section == NULL)
	    {
	      fprintf (stderr, "%s: can't create section `%s': %s\n",
		       program_name, padd->name,
		       bfd_errmsg (bfd_get_error ()));
	      status = 1;
	      return;
	    }
	  else
	    {
	      flagword flags;
	      
	      if (! bfd_set_section_size (obfd, padd->section, padd->size))
		nonfatal (bfd_get_filename (obfd));

	      pset = find_section_list (padd->name, false);
	      if (pset != NULL)
		pset->used = true;

	      if (pset != NULL && pset->set_flags)
		flags = pset->flags | SEC_HAS_CONTENTS;
	      else
		flags = SEC_HAS_CONTENTS | SEC_READONLY | SEC_DATA;
	      if (! bfd_set_section_flags (obfd, padd->section, flags))
		nonfatal (bfd_get_filename (obfd));

	      if (pset != NULL
		  && (pset->adjust == adjust_vma
		      || pset->adjust == set_vma))
		{
		  if (! bfd_set_section_vma (obfd, padd->section, pset->val))
		    nonfatal (bfd_get_filename (obfd));
		}
	    }
	}
    }

  if (gap_fill_set || pad_to_set)
    {
      asection **set;
      unsigned int c, i;

      /* We must fill in gaps between the sections and/or we must pad
	 the last section to a specified address.  We do this by
	 grabbing a list of the sections, sorting them by VMA, and
	 increasing the section sizes as required to fill the gaps.
	 We write out the gap contents below.  */

      c = bfd_count_sections (obfd);
      osections = (asection **) xmalloc (c * sizeof (asection *));
      set = osections;
      bfd_map_over_sections (obfd, get_sections, (void *) &set);

      qsort (osections, c, sizeof (asection *), compare_section_lma);

      gaps = (bfd_size_type *) xmalloc (c * sizeof (bfd_size_type));
      memset (gaps, 0, c * sizeof (bfd_size_type));

      if (gap_fill_set)
	{
	  for (i = 0; i < c - 1; i++)
	    {
	      flagword flags;
	      bfd_size_type size;
	      bfd_vma gap_start, gap_stop;

	      flags = bfd_get_section_flags (obfd, osections[i]);
	      if ((flags & SEC_HAS_CONTENTS) == 0
		  || (flags & SEC_LOAD) == 0)
		continue;

	      size = bfd_section_size (obfd, osections[i]);
	      gap_start = bfd_section_lma (obfd, osections[i]) + size;
	      gap_stop = bfd_section_lma (obfd, osections[i + 1]);
	      if (gap_start < gap_stop)
		{
		  if (! bfd_set_section_size (obfd, osections[i],
					      size + (gap_stop - gap_start)))
		    {
		      fprintf (stderr, "%s: Can't fill gap after %s: %s\n",
			       program_name,
			       bfd_get_section_name (obfd, osections[i]),
			       bfd_errmsg (bfd_get_error()));
		      status = 1;
		      break;
		    }
		  gaps[i] = gap_stop - gap_start;
		  if (max_gap < gap_stop - gap_start)
		    max_gap = gap_stop - gap_start;
		}
	    }
	}

      if (pad_to_set)
	{
	  bfd_vma lma;
	  bfd_size_type size;

	  lma = bfd_section_lma (obfd, osections[c - 1]);
	  size = bfd_section_size (obfd, osections[c - 1]);
	  if (lma + size < pad_to)
	    {
	      if (! bfd_set_section_size (obfd, osections[c - 1],
					  pad_to - lma))
		{
		  fprintf (stderr, "%s: Can't add padding to %s: %s\n",
			   program_name,
			   bfd_get_section_name (obfd, osections[c - 1]),
			   bfd_errmsg (bfd_get_error ()));
		  status = 1;
		}
	      else
		{
		  gaps[c - 1] = pad_to - (lma + size);
		  if (max_gap < pad_to - (lma + size))
		    max_gap = pad_to - (lma + size);
		}
	    }
	}
    }

  /* Symbol filtering must happen after the output sections have
     been created, but before their contents are set.  */
  if (strip_symbols == strip_all)
    {
      osympp = isympp = NULL;
      symcount = 0;
    }
  else
    {
      long symsize;
      PTR dhandle = NULL;

      symsize = bfd_get_symtab_upper_bound (ibfd);
      if (symsize < 0)
	{
	  nonfatal (bfd_get_filename (ibfd));
	}

      osympp = isympp = (asymbol **) xmalloc (symsize);
      symcount = bfd_canonicalize_symtab (ibfd, isympp);
      if (symcount < 0)
	{
	  nonfatal (bfd_get_filename (ibfd));
	}

      if (convert_debugging)
	dhandle = read_debugging_info (ibfd, isympp, symcount);

      if (strip_symbols == strip_debug 
	  || strip_symbols == strip_unneeded
	  || discard_locals != locals_undef
	  || strip_specific_list != NULL
	  || keep_specific_list != NULL
	  || localize_specific_list != NULL
	  || weaken_specific_list != NULL
	  || sections_removed
	  || convert_debugging
	  || change_leading_char
	  || remove_leading_char
	  || weaken)
	{
	  /* Mark symbols used in output relocations so that they
	     are kept, even if they are local labels or static symbols.

	     Note we iterate over the input sections examining their
	     relocations since the relocations for the output sections
	     haven't been set yet.  mark_symbols_used_in_relocations will
	     ignore input sections which have no corresponding output
	     section.  */
	  bfd_map_over_sections (ibfd,
				 mark_symbols_used_in_relocations,
				 (PTR)isympp);
	  osympp = (asymbol **) xmalloc ((symcount + 1) * sizeof (asymbol *));
	  symcount = filter_symbols (ibfd, obfd, osympp, isympp, symcount);
	}

      if (convert_debugging && dhandle != NULL)
	{
	  if (! write_debugging_info (obfd, dhandle, &symcount, &osympp))
	    {
	      status = 1;
	      return;
	    }
	}
    }

  bfd_set_symtab (obfd, osympp, symcount);

  /* This has to happen after the symbol table has been set.  */
  bfd_map_over_sections (ibfd, copy_section, (void *) obfd);

  if (add_sections != NULL)
    {
      struct section_add *padd;

      for (padd = add_sections; padd != NULL; padd = padd->next)
	{
	  if (! bfd_set_section_contents (obfd, padd->section,
					  (PTR) padd->contents,
					  (file_ptr) 0,
					  (bfd_size_type) padd->size))
	    nonfatal (bfd_get_filename (obfd));
	}
    }

  if (gap_fill_set || pad_to_set)
    {
      bfd_byte *buf;
      int c, i;

      /* Fill in the gaps.  */

      if (max_gap > 8192)
	max_gap = 8192;
      buf = (bfd_byte *) xmalloc (max_gap);
      memset (buf, gap_fill, (size_t) max_gap);

      c = bfd_count_sections (obfd);
      for (i = 0; i < c; i++)
	{
	  if (gaps[i] != 0)
	    {
	      bfd_size_type left;
	      file_ptr off;

	      left = gaps[i];
	      off = bfd_section_size (obfd, osections[i]) - left;
	      while (left > 0)
		{
		  bfd_size_type now;

		  if (left > 8192)
		    now = 8192;
		  else
		    now = left;
		  if (! bfd_set_section_contents (obfd, osections[i], buf,
						  off, now))
		    {
		      nonfatal (bfd_get_filename (obfd));
		    }
		  left -= now;
		  off += now;
		}
	    }
	}
    }

  /* Allow the BFD backend to copy any private data it understands
     from the input BFD to the output BFD.  This is done last to
     permit the routine to look at the filtered symbol table, which is
     important for the ECOFF code at least.  */
  if (!bfd_copy_private_bfd_data (ibfd, obfd))
    {
      fprintf (stderr, "%s: %s: error copying private BFD data: %s\n",
	       program_name, bfd_get_filename (obfd),
	       bfd_errmsg (bfd_get_error ()));
      status = 1;
      return;
    }
}

/* Read each archive element in turn from IBFD, copy the
   contents to temp file, and keep the temp file handle.  */

static void
copy_archive (ibfd, obfd, output_target)
     bfd *ibfd;
     bfd *obfd;
     const char *output_target;
{
  struct name_list
    {
      struct name_list *next;
      char *name;
      bfd *obfd;
    } *list, *l;
  bfd **ptr = &obfd->archive_head;
  bfd *this_element;
  char *dir = make_tempname (bfd_get_filename (obfd));

  /* Make a temp directory to hold the contents.  */
#if defined (_WIN32) && !defined (__CYGWIN32__)
  if (mkdir (dir) != 0)
#else
  if (mkdir (dir, 0700) != 0)
#endif
    {
      fatal ("cannot mkdir %s for archive copying (error: %s)",
	     dir, strerror (errno));
    }
  obfd->has_armap = ibfd->has_armap;

  list = NULL;

  this_element = bfd_openr_next_archived_file (ibfd, NULL);
  while (this_element != (bfd *) NULL)
    {
      /* Create an output file for this member.  */
      char *output_name = concat (dir, "/", bfd_get_filename(this_element),
				  (char *) NULL);
      bfd *output_bfd = bfd_openw (output_name, output_target);
      bfd *last_element;

      l = (struct name_list *) xmalloc (sizeof (struct name_list));
      l->name = output_name;
      l->next = list;
      list = l;

      if (output_bfd == (bfd *) NULL)
	{
	  nonfatal (output_name);
	}
      if (!bfd_set_format (obfd, bfd_get_format (ibfd)))
	{
	  nonfatal (bfd_get_filename (obfd));
	}

      if (bfd_check_format (this_element, bfd_object) == true)
	{
	  copy_object (this_element, output_bfd);
	}

      bfd_close (output_bfd);

      /* Open the newly output file and attach to our list.  */
      output_bfd = bfd_openr (output_name, output_target);

      l->obfd = output_bfd;

      *ptr = output_bfd;
      ptr = &output_bfd->next;

      last_element = this_element;

      this_element = bfd_openr_next_archived_file (ibfd, last_element);

      bfd_close (last_element);
    }
  *ptr = (bfd *) NULL;

  if (!bfd_close (obfd))
    {
      nonfatal (bfd_get_filename (obfd));
    }

  if (!bfd_close (ibfd))
    {
      nonfatal (bfd_get_filename (ibfd));
    }

  /* Delete all the files that we opened.  */
  for (l = list; l != NULL; l = l->next)
    {
      bfd_close (l->obfd);
      unlink (l->name);
    }
  rmdir (dir);
}

/* The top-level control.  */

static void
copy_file (input_filename, output_filename, input_target, output_target)
     const char *input_filename;
     const char *output_filename;
     const char *input_target;
     const char *output_target;
{
  bfd *ibfd;
  char **matching;

  /* To allow us to do "strip *" without dying on the first
     non-object file, failures are nonfatal.  */

  ibfd = bfd_openr (input_filename, input_target);
  if (ibfd == NULL)
    {
      nonfatal (input_filename);
    }

  if (bfd_check_format (ibfd, bfd_archive))
    {
      bfd *obfd;

      /* bfd_get_target does not return the correct value until
         bfd_check_format succeeds.  */
      if (output_target == NULL)
	output_target = bfd_get_target (ibfd);

      obfd = bfd_openw (output_filename, output_target);
      if (obfd == NULL)
	{
	  nonfatal (output_filename);
	}
      copy_archive (ibfd, obfd, output_target);
    }
  else if (bfd_check_format_matches (ibfd, bfd_object, &matching))
    {
      bfd *obfd;

      /* bfd_get_target does not return the correct value until
         bfd_check_format succeeds.  */
      if (output_target == NULL)
	output_target = bfd_get_target (ibfd);

      obfd = bfd_openw (output_filename, output_target);
      if (obfd == NULL)
	{
	  nonfatal (output_filename);
	}

      copy_object (ibfd, obfd);

      if (!bfd_close (obfd))
	{
	  nonfatal (output_filename);
	}

      if (!bfd_close (ibfd))
	{
	  nonfatal (input_filename);
	}
    }
  else
    {
      bfd_nonfatal (input_filename);
      if (bfd_get_error () == bfd_error_file_ambiguously_recognized)
	{
	  list_matching_formats (matching);
	  free (matching);
	}
      status = 1;
    }
}

/* Create a section in OBFD with the same name and attributes
   as ISECTION in IBFD.  */

static void
setup_section (ibfd, isection, obfdarg)
     bfd *ibfd;
     sec_ptr isection;
     PTR obfdarg;
{
  bfd *obfd = (bfd *) obfdarg;
  struct section_list *p;
  sec_ptr osection;
  bfd_vma vma;
  bfd_vma lma;
  flagword flags;
  char *err;

  if ((bfd_get_section_flags (ibfd, isection) & SEC_DEBUGGING) != 0
      && (strip_symbols == strip_debug
	  || strip_symbols == strip_unneeded
	  || strip_symbols == strip_all
	  || discard_locals == locals_all
	  || convert_debugging))
    return;

  p = find_section_list (bfd_section_name (ibfd, isection), false);
  if (p != NULL)
    p->used = true;

  if (p != NULL && p->remove)
    return;

  osection = bfd_make_section_anyway (obfd, bfd_section_name (ibfd, isection));
  if (osection == NULL)
    {
      err = "making";
      goto loser;
    }

  if (!bfd_set_section_size (obfd,
			     osection,
			     bfd_section_size (ibfd, isection)))
    {
      err = "size";
      goto loser;
    }

  vma = bfd_section_vma (ibfd, isection);
  if (p != NULL && p->adjust == adjust_vma)
    vma += p->val;
  else if (p != NULL && p->adjust == set_vma)
    vma = p->val;
  else
    vma += adjust_section_vma;
  if (! bfd_set_section_vma (obfd, osection, vma))
    {
      err = "vma";
      goto loser;
    }

  lma = isection->lma;
  if (p != NULL && p->adjust == adjust_vma)
    lma += p->val;
  else if (p != NULL && p->adjust == set_vma)
    lma = p->val;
  else
    lma += adjust_section_vma;
  osection->lma = lma;

  if (bfd_set_section_alignment (obfd,
				 osection,
				 bfd_section_alignment (ibfd, isection))
      == false)
    {
      err = "alignment";
      goto loser;
    }

  flags = bfd_get_section_flags (ibfd, isection);
  if (p != NULL && p->set_flags)
    flags = p->flags | (flags & SEC_HAS_CONTENTS);
  if (!bfd_set_section_flags (obfd, osection, flags))
    {
      err = "flags";
      goto loser;
    }

  /* This used to be mangle_section; we do here to avoid using
     bfd_get_section_by_name since some formats allow multiple
     sections with the same name.  */
  isection->output_section = osection;
  isection->output_offset = 0;

  /* Allow the BFD backend to copy any private data it understands
     from the input section to the output section.  */
  if (!bfd_copy_private_section_data (ibfd, isection, obfd, osection))
    {
      err = "private data";
      goto loser;
    }

  /* All went well */
  return;

loser:
  fprintf (stderr, "%s: %s: section `%s': error in %s: %s\n",
	   program_name,
	   bfd_get_filename (ibfd), bfd_section_name (ibfd, isection),
	   err, bfd_errmsg (bfd_get_error ()));
  status = 1;
}

/* Copy the data of input section ISECTION of IBFD
   to an output section with the same name in OBFD.
   If stripping then don't copy any relocation info.  */

static void
copy_section (ibfd, isection, obfdarg)
     bfd *ibfd;
     sec_ptr isection;
     PTR obfdarg;
{
  bfd *obfd = (bfd *) obfdarg;
  struct section_list *p;
  arelent **relpp;
  long relcount;
  sec_ptr osection;
  bfd_size_type size;

  if ((bfd_get_section_flags (ibfd, isection) & SEC_DEBUGGING) != 0
      && (strip_symbols == strip_debug
	  || strip_symbols == strip_unneeded
	  || strip_symbols == strip_all
	  || discard_locals == locals_all
	  || convert_debugging))
    {
      return;
    }

  p = find_section_list (bfd_section_name (ibfd, isection), false);

  if (p != NULL && p->remove)
    return;

  osection = isection->output_section;
  size = bfd_get_section_size_before_reloc (isection);

  if (size == 0 || osection == 0)
    return;

  if (strip_symbols == strip_all)
    bfd_set_reloc (obfd, osection, (arelent **) NULL, 0);
  else
    {
      long relsize;

      relsize = bfd_get_reloc_upper_bound (ibfd, isection);
      if (relsize < 0)
	{
	  nonfatal (bfd_get_filename (ibfd));
	}
      if (relsize == 0)
	bfd_set_reloc (obfd, osection, (arelent **) NULL, 0);
      else
	{
	  relpp = (arelent **) xmalloc (relsize);
	  relcount = bfd_canonicalize_reloc (ibfd, isection, relpp, isympp);
	  if (relcount < 0)
	    {
	      nonfatal (bfd_get_filename (ibfd));
	    }
	  bfd_set_reloc (obfd, osection, relpp, relcount);
	}
    }

  isection->_cooked_size = isection->_raw_size;
  isection->reloc_done = true;

  if (bfd_get_section_flags (ibfd, isection) & SEC_HAS_CONTENTS)
    {
      PTR memhunk = (PTR) xmalloc ((unsigned) size);

      if (!bfd_get_section_contents (ibfd, isection, memhunk, (file_ptr) 0,
				     size))
	{
	  nonfatal (bfd_get_filename (ibfd));
	}

      if (copy_byte >= 0) 
        {
	  filter_bytes (memhunk, &size);
              /* The section has gotten smaller. */
          if (!bfd_set_section_size (obfd, osection, size))
            nonfatal (bfd_get_filename (obfd));
        }

      if (!bfd_set_section_contents (obfd, osection, memhunk, (file_ptr) 0,
				     size))
	{
	  nonfatal (bfd_get_filename (obfd));
	}
      free (memhunk);
    }
  else if (p != NULL && p->set_flags && (p->flags & SEC_HAS_CONTENTS) != 0)
    {
      PTR memhunk = (PTR) xmalloc ((unsigned) size);

      /* We don't permit the user to turn off the SEC_HAS_CONTENTS
	 flag--they can just remove the section entirely and add it
	 back again.  However, we do permit them to turn on the
	 SEC_HAS_CONTENTS flag, and take it to mean that the section
	 contents should be zeroed out.  */

      memset (memhunk, 0, size);
      if (! bfd_set_section_contents (obfd, osection, memhunk, (file_ptr) 0,
				      size))
	nonfatal (bfd_get_filename (obfd));
      free (memhunk);
    }
}

/* Get all the sections.  This is used when --gap-fill or --pad-to is
   used.  */

static void
get_sections (obfd, osection, secppparg)
     bfd *obfd;
     asection *osection;
     PTR secppparg;
{
  asection ***secppp = (asection ***) secppparg;

  **secppp = osection;
  ++(*secppp);
}

/* Sort sections by VMA.  This is called via qsort, and is used when
   --gap-fill or --pad-to is used.  We force non loadable or empty
   sections to the front, where they are easier to ignore.  */

static int
compare_section_lma (arg1, arg2)
     const PTR arg1;
     const PTR arg2;
{
  const asection **sec1 = (const asection **) arg1;
  const asection **sec2 = (const asection **) arg2;
  flagword flags1, flags2;

  /* Sort non loadable sections to the front.  */
  flags1 = (*sec1)->flags;
  flags2 = (*sec2)->flags;
  if ((flags1 & SEC_HAS_CONTENTS) == 0
      || (flags1 & SEC_LOAD) == 0)
    {
      if ((flags2 & SEC_HAS_CONTENTS) != 0
	  && (flags2 & SEC_LOAD) != 0)
	return -1;
    }
  else
    {
      if ((flags2 & SEC_HAS_CONTENTS) == 0
	  || (flags2 & SEC_LOAD) == 0)
	return 1;
    }

  /* Sort sections by LMA.  */
  if ((*sec1)->lma > (*sec2)->lma)
    return 1;
  else if ((*sec1)->lma < (*sec2)->lma)
    return -1;

  /* Sort sections with the same LMA by size.  */
  if ((*sec1)->_raw_size > (*sec2)->_raw_size)
    return 1;
  else if ((*sec1)->_raw_size < (*sec2)->_raw_size)
    return -1;

  return 0;
}

/* Mark all the symbols which will be used in output relocations with
   the BSF_KEEP flag so that those symbols will not be stripped.

   Ignore relocations which will not appear in the output file.  */

static void
mark_symbols_used_in_relocations (ibfd, isection, symbolsarg)
     bfd *ibfd;
     sec_ptr isection;
     PTR symbolsarg;
{
  asymbol **symbols = (asymbol **) symbolsarg;
  long relsize;
  arelent **relpp;
  long relcount, i;

  /* Ignore an input section with no corresponding output section.  */
  if (isection->output_section == NULL)
    return;

  relsize = bfd_get_reloc_upper_bound (ibfd, isection);
  if (relsize < 0)
    bfd_fatal (bfd_get_filename (ibfd));

  if (relsize == 0)
    return;

  relpp = (arelent **) xmalloc (relsize);
  relcount = bfd_canonicalize_reloc (ibfd, isection, relpp, symbols);
  if (relcount < 0)
    bfd_fatal (bfd_get_filename (ibfd));

  /* Examine each symbol used in a relocation.  If it's not one of the
     special bfd section symbols, then mark it with BSF_KEEP.  */
  for (i = 0; i < relcount; i++)
    {
      if (*relpp[i]->sym_ptr_ptr != bfd_com_section_ptr->symbol
	  && *relpp[i]->sym_ptr_ptr != bfd_abs_section_ptr->symbol
	  && *relpp[i]->sym_ptr_ptr != bfd_und_section_ptr->symbol)
	(*relpp[i]->sym_ptr_ptr)->flags |= BSF_KEEP;
    }

  if (relpp != NULL)
    free (relpp);
}

/* Write out debugging information.  */

static boolean
write_debugging_info (obfd, dhandle, symcountp, symppp)
     bfd *obfd;
     PTR dhandle;
     long *symcountp;
     asymbol ***symppp;
{
  if (bfd_get_flavour (obfd) == bfd_target_ieee_flavour)
    return write_ieee_debugging_info (obfd, dhandle);

  if (bfd_get_flavour (obfd) == bfd_target_coff_flavour
      || bfd_get_flavour (obfd) == bfd_target_elf_flavour)
    {
      bfd_byte *syms, *strings;
      bfd_size_type symsize, stringsize;
      asection *stabsec, *stabstrsec;

      if (! write_stabs_in_sections_debugging_info (obfd, dhandle, &syms,
						    &symsize, &strings,
						    &stringsize))
	return false;

      stabsec = bfd_make_section (obfd, ".stab");
      stabstrsec = bfd_make_section (obfd, ".stabstr");
      if (stabsec == NULL
	  || stabstrsec == NULL
	  || ! bfd_set_section_size (obfd, stabsec, symsize)
	  || ! bfd_set_section_size (obfd, stabstrsec, stringsize)
	  || ! bfd_set_section_alignment (obfd, stabsec, 2)
	  || ! bfd_set_section_alignment (obfd, stabstrsec, 0)
	  || ! bfd_set_section_flags (obfd, stabsec,
				   (SEC_HAS_CONTENTS
				    | SEC_READONLY
				    | SEC_DEBUGGING))
	  || ! bfd_set_section_flags (obfd, stabstrsec,
				      (SEC_HAS_CONTENTS
				       | SEC_READONLY
				       | SEC_DEBUGGING)))
	{
	  fprintf (stderr, "%s: can't create debugging section: %s\n",
		   bfd_get_filename (obfd), bfd_errmsg (bfd_get_error ()));
	  return false;
	}

      /* We can get away with setting the section contents now because
         the next thing the caller is going to do is copy over the
         real sections.  We may someday have to split the contents
         setting out of this function.  */
      if (! bfd_set_section_contents (obfd, stabsec, syms, (file_ptr) 0,
				      symsize)
	  || ! bfd_set_section_contents (obfd, stabstrsec, strings,
					 (file_ptr) 0, stringsize))
	{
	  fprintf (stderr, "%s: can't set debugging section contents: %s\n",
		   bfd_get_filename (obfd), bfd_errmsg (bfd_get_error ()));
	  return false;
	}

      return true;
    }

  fprintf (stderr,
	   "%s: don't know how to write debugging information for %s\n",
	   bfd_get_filename (obfd), bfd_get_target (obfd));
  return false;
}

/* The number of bytes to copy at once.  */
#define COPY_BUF 8192

/* Copy file FROM to file TO, performing no translations.
   Return 0 if ok, -1 if error.  */

static int
simple_copy (from, to)
     const char *from;
     const char *to;
{
  int fromfd, tofd, nread;
  int saved;
  char buf[COPY_BUF];

  fromfd = open (from, O_RDONLY);
  if (fromfd < 0)
    return -1;
  tofd = creat (to, 0777);
  if (tofd < 0)
    {
      saved = errno;
      close (fromfd);
      errno = saved;
      return -1;
    }
  while ((nread = read (fromfd, buf, sizeof buf)) > 0)
    {
      if (write (tofd, buf, nread) != nread)
	{
	  saved = errno;
	  close (fromfd);
	  close (tofd);
	  errno = saved;
	  return -1;
	}
    }
  saved = errno;
  close (fromfd);
  close (tofd);
  if (nread < 0)
    {
      errno = saved;
      return -1;
    }
  return 0;
}

#ifndef S_ISLNK
#ifdef S_IFLNK
#define S_ISLNK(m) (((m) & S_IFMT) == S_IFLNK)
#else
#define S_ISLNK(m) 0
#define lstat stat
#endif
#endif

/* Rename FROM to TO, copying if TO is a link.
   Assumes that TO already exists, because FROM is a temp file.
   Return 0 if ok, -1 if error.  */

static int
smart_rename (from, to)
     const char *from;
     const char *to;
{
  struct stat s;
  int ret = 0;

  if (lstat (to, &s))
    return -1;

#if defined (_WIN32) && !defined (__CYGWIN32__)
  /* Win32, unlike unix, will not erase `to' in `rename(from, to)' but
     fail instead.  Also, chown is not present.  */

  if (stat (to, &s) == 0)
    remove (to);

  ret = rename (from, to);
  if (ret != 0)
    {
      /* We have to clean up here. */
      int saved = errno;
      fprintf (stderr, "%s: %s: ", program_name, to);
      errno = saved;
      perror ("rename");
      unlink (from);
    }
#else
  /* Use rename only if TO is not a symbolic link and has
     only one hard link.  */
  if (!S_ISLNK (s.st_mode) && s.st_nlink == 1)
    {
      ret = rename (from, to);
      if (ret == 0)
	{
	  /* Try to preserve the permission bits and ownership of TO.
             First get the mode right except for the setuid bit.  Then
             change the ownership.  Then fix the setuid bit.  We do
             the chmod before the chown because if the chown succeeds,
             and we are a normal user, we won't be able to do the
             chmod afterward.  We don't bother to fix the setuid bit
             first because that might introduce a fleeting security
             problem, and because the chown will clear the setuid bit
             anyhow.  We only fix the setuid bit if the chown
             succeeds, because we don't want to introduce an
             unexpected setuid file owned by the user running objcopy.  */
	  chmod (to, s.st_mode & 0777);
	  if (chown (to, s.st_uid, s.st_gid) >= 0)
	    chmod (to, s.st_mode & 07777);
	}
      else
	{
	  /* We have to clean up here. */
	  int saved = errno;
	  fprintf (stderr, "%s: %s: ", program_name, to);
	  errno = saved;
	  perror ("rename");
	  unlink (from);
	}
    }
  else
    {
      ret = simple_copy (from, to);
      if (ret != 0)
	{
	  int saved = errno;
	  fprintf (stderr, "%s: %s: ", program_name, to);
	  errno = saved;
	  perror ("simple_copy");
	}
      unlink (from);
    }
#endif /* _WIN32 && !__CYGWIN32__ */

  return ret;
}

/* Set the times of the file DESTINATION to be the same as those in
   STATBUF.  */

static void
set_times (destination, statbuf)
     const char *destination;
     const struct stat *statbuf;
{
  int result;

  {
#ifdef HAVE_GOOD_UTIME_H
    struct utimbuf tb;

    tb.actime = statbuf->st_atime;
    tb.modtime = statbuf->st_mtime;
    result = utime (destination, &tb);
#else /* ! HAVE_GOOD_UTIME_H */
#ifndef HAVE_UTIMES
    long tb[2];

    tb[0] = statbuf->st_atime;
    tb[1] = statbuf->st_mtime;
    result = utime (destination, tb);
#else /* HAVE_UTIMES */
    struct timeval tv[2];

    tv[0].tv_sec = statbuf->st_atime;
    tv[0].tv_usec = 0;
    tv[1].tv_sec = statbuf->st_mtime;
    tv[1].tv_usec = 0;
    result = utimes (destination, tv);
#endif /* HAVE_UTIMES */
#endif /* ! HAVE_GOOD_UTIME_H */
  }

  if (result != 0)
    {
      fprintf (stderr, "%s: ", destination);
      perror ("can not set time");
    }
}

static int
strip_main (argc, argv)
     int argc;
     char *argv[];
{
  char *input_target = NULL, *output_target = NULL;
  boolean show_version = false;
  boolean preserve_dates = false;
  int c, i;
  struct section_list *p;
  char *output_file = NULL;

  while ((c = getopt_long (argc, argv, "I:O:F:K:N:R:o:sSpgxXVv",
			   strip_options, (int *) 0)) != EOF)
    {
      switch (c)
	{
	case 'I':
	  input_target = optarg;
	  break;
	case 'O':
	  output_target = optarg;
	  break;
	case 'F':
	  input_target = output_target = optarg;
	  break;
	case 'R':
	  p = find_section_list (optarg, true);
	  p->remove = true;
	  sections_removed = true;
	  break;
	case 's':
	  strip_symbols = strip_all;
	  break;
	case 'S':
	case 'g':
	  strip_symbols = strip_debug;
	  break;
	case OPTION_STRIP_UNNEEDED:
	  strip_symbols = strip_unneeded;
	  break;
	case 'K':
	  add_specific_symbol (optarg, &keep_specific_list);
	  break;
	case 'N':
	  add_specific_symbol (optarg, &strip_specific_list);
	  break;
	case 'o':
	  output_file = optarg;
	  break;
	case 'p':
	  preserve_dates = true;
	  break;
	case 'x':
	  discard_locals = locals_all;
	  break;
	case 'X':
	  discard_locals = locals_start_L;
	  break;
	case 'v':
	  verbose = true;
	  break;
	case 'V':
	  show_version = true;
	  break;
	case 0:
	  break;		/* we've been given a long option */
	case 'h':
	  strip_usage (stdout, 0);
	default:
	  strip_usage (stderr, 1);
	}
    }

  if (show_version)
    print_version ("strip");

  /* Default is to strip all symbols.  */
  if (strip_symbols == strip_undef
      && discard_locals == locals_undef
      && strip_specific_list == NULL)
    strip_symbols = strip_all;

  if (output_target == (char *) NULL)
    output_target = input_target;

  i = optind;
  if (i == argc
      || (output_file != NULL && (i + 1) < argc))
    strip_usage (stderr, 1);

  for (; i < argc; i++)
    {
      int hold_status = status;
      struct stat statbuf;
      char *tmpname;

      if (preserve_dates)
	{
	  if (stat (argv[i], &statbuf) < 0)
	    {
	      fprintf (stderr, "%s: ", argv[i]);
	      perror ("cannot stat");
	      continue;
	    }
	}

      if (output_file != NULL)
	tmpname = output_file;
      else
	tmpname = make_tempname (argv[i]);
      status = 0;

      copy_file (argv[i], tmpname, input_target, output_target);
      if (status == 0)
	{
	  if (preserve_dates)
	    set_times (tmpname, &statbuf);
	  if (output_file == NULL)
	    smart_rename (tmpname, argv[i]);
	  status = hold_status;
	}
      else
	unlink (tmpname);
      if (output_file == NULL)
	free (tmpname);
    }

  return 0;
}

static int
copy_main (argc, argv)
     int argc;
     char *argv[];
{
  char *input_filename = NULL, *output_filename = NULL;
  char *input_target = NULL, *output_target = NULL;
  boolean show_version = false;
  boolean adjust_warn = true;
  boolean preserve_dates = false;
  int c;
  struct section_list *p;
  struct stat statbuf;

  while ((c = getopt_long (argc, argv, "b:i:I:K:N:s:O:d:F:L:R:SpgxXVvW:",
			   copy_options, (int *) 0)) != EOF)
    {
      switch (c)
	{
	case 'b':
	  copy_byte = atoi(optarg);
	  if (copy_byte < 0)
	    {
	      fprintf (stderr, "%s: byte number must be non-negative\n",
		       program_name);
	      exit (1);
	    }
	  break;
	case 'i':
	  interleave = atoi(optarg);
	  if (interleave < 1)
	    {
	      fprintf(stderr, "%s: interleave must be positive\n",
		      program_name);
	      exit (1);
	    }
	  break;
	case 'I':
	case 's':		/* "source" - 'I' is preferred */
	  input_target = optarg;
	  break;
	case 'O':
	case 'd':		/* "destination" - 'O' is preferred */
	  output_target = optarg;
	  break;
	case 'F':
	  input_target = output_target = optarg;
	  break;
	case 'R':
	  p = find_section_list (optarg, true);
	  p->remove = true;
	  sections_removed = true;
	  break;
	case 'S':
	  strip_symbols = strip_all;
	  break;
	case 'g':
	  strip_symbols = strip_debug;
	  break;
	case OPTION_STRIP_UNNEEDED:
	  strip_symbols = strip_unneeded;
	  break;
	case 'K':
	  add_specific_symbol (optarg, &keep_specific_list);
	  break;
	case 'N':
	  add_specific_symbol (optarg, &strip_specific_list);
	  break;
	case 'L':
	  add_specific_symbol (optarg, &localize_specific_list);
	  break;
	case 'W':
	  add_specific_symbol (optarg, &weaken_specific_list);
	  break;
	case 'p':
	  preserve_dates = true;
	  break;
	case 'x':
	  discard_locals = locals_all;
	  break;
	case 'X':
	  discard_locals = locals_start_L;
	  break;
	case 'v':
	  verbose = true;
	  break;
	case 'V':
	  show_version = true;
	  break;
	case OPTION_WEAKEN:
	  weaken = true;
	  break;
	case OPTION_ADD_SECTION:
	  {
	    const char *s;
	    struct stat st;
	    struct section_add *pa;
	    int len;
	    char *name;
	    FILE *f;

	    s = strchr (optarg, '=');
	    if (s == NULL)
	      {
		fprintf (stderr,
			 "%s: bad format for --add-section NAME=FILENAME\n",
			 program_name);
		exit (1);
	      }

	    if (stat (s + 1, &st) < 0)
	      {
		fprintf (stderr, "%s: ", program_name);
		perror (s + 1);
		exit (1);
	      }

	    pa = (struct section_add *) xmalloc (sizeof (struct section_add));

	    len = s - optarg;
	    name = (char *) xmalloc (len + 1);
	    strncpy (name, optarg, len);
	    name[len] = '\0';
	    pa->name = name;

	    pa->filename = s + 1;

	    pa->size = st.st_size;

	    pa->contents = (bfd_byte *) xmalloc (pa->size);
	    f = fopen (pa->filename, FOPEN_RB);
	    if (f == NULL)
	      {
		fprintf (stderr, "%s: ", program_name);
		perror (pa->filename);
		exit (1);
	      }
	    if (fread (pa->contents, 1, pa->size, f) == 0
		|| ferror (f))
	      {
		fprintf (stderr, "%s: %s: fread failed\n",
			 program_name, pa->filename);
		exit (1);
	      }
	    fclose (f);

	    pa->next = add_sections;
	    add_sections = pa;
	  }
	  break;
	case OPTION_ADJUST_START:
	  adjust_start = parse_vma (optarg, "--adjust-start");
	  break;
	case OPTION_ADJUST_SECTION_VMA:
	  {
	    const char *s;
	    int len;
	    char *name;

	    s = strchr (optarg, '=');
	    if (s == NULL)
	      {
		s = strchr (optarg, '+');
		if (s == NULL)
		  {
		    s = strchr (optarg, '-');
		    if (s == NULL)
		      {
			fprintf (stderr,
				 "%s: bad format for --adjust-section-vma\n",
				 program_name);
			exit (1);
		      }
		  }
	      }

	    len = s - optarg;
	    name = (char *) xmalloc (len + 1);
	    strncpy (name, optarg, len);
	    name[len] = '\0';

	    p = find_section_list (name, true);

	    p->val = parse_vma (s + 1, "--adjust-section-vma");

	    if (*s == '=')
	      p->adjust = set_vma;
	    else
	      {
		p->adjust = adjust_vma;
		if (*s == '-')
		  p->val = - p->val;
	      }
	  }
	  break;
	case OPTION_ADJUST_VMA:
	  adjust_section_vma = parse_vma (optarg, "--adjust-vma");
	  adjust_start = adjust_section_vma;
	  break;
	case OPTION_ADJUST_WARNINGS:
	  adjust_warn = true;
	  break;
	case OPTION_CHANGE_LEADING_CHAR:
	  change_leading_char = true;
	  break;
	case OPTION_DEBUGGING:
	  convert_debugging = true;
	  break;
	case OPTION_GAP_FILL:
	  {
	    bfd_vma gap_fill_vma;

	    gap_fill_vma = parse_vma (optarg, "--gap-fill");
	    gap_fill = (bfd_byte) gap_fill_vma;
	    if ((bfd_vma) gap_fill != gap_fill_vma)
	      {
		fprintf (stderr, "%s: warning: truncating gap-fill from 0x",
			 program_name);
		fprintf_vma (stderr, gap_fill_vma);
		fprintf (stderr, "to 0x%x\n", (unsigned int) gap_fill);
	      }
	    gap_fill_set = true;
	  }
	  break;
	case OPTION_NO_ADJUST_WARNINGS:
	  adjust_warn = false;
	  break;
	case OPTION_PAD_TO:
	  pad_to = parse_vma (optarg, "--pad-to");
	  pad_to_set = true;
	  break;
	case OPTION_REMOVE_LEADING_CHAR:
	  remove_leading_char = true;
	  break;
	case OPTION_SET_SECTION_FLAGS:
	  {
	    const char *s;
	    int len;
	    char *name;

	    s = strchr (optarg, '=');
	    if (s == NULL)
	      {
		fprintf (stderr, "%s: bad format for --set-section-flags\n",
			 program_name);
		exit (1);
	      }

	    len = s - optarg;
	    name = (char *) xmalloc (len + 1);
	    strncpy (name, optarg, len);
	    name[len] = '\0';

	    p = find_section_list (name, true);

	    p->set_flags = true;
	    p->flags = parse_flags (s + 1);
	  }
	  break;
	case OPTION_SET_START:
	  set_start = parse_vma (optarg, "--set-start");
	  set_start_set = true;
	  break;
	case 0:
	  break;		/* we've been given a long option */
	case 'h':
	  copy_usage (stdout, 0);
	default:
	  copy_usage (stderr, 1);
	}
    }

  if (show_version)
    print_version ("objcopy");

  if (copy_byte >= interleave)
    {
      fprintf (stderr, "%s: byte number must be less than interleave\n",
	       program_name);
      exit (1);
    }

  if (optind == argc || optind + 2 < argc)
    copy_usage (stderr, 1);

  input_filename = argv[optind];
  if (optind + 1 < argc)
    output_filename = argv[optind + 1];

  /* Default is to strip no symbols.  */
  if (strip_symbols == strip_undef && discard_locals == locals_undef)
    strip_symbols = strip_none;

  if (output_target == (char *) NULL)
    output_target = input_target;

  if (preserve_dates)
    {
      if (stat (input_filename, &statbuf) < 0)
	{
	  fprintf (stderr, "%s: ", input_filename);
	  perror ("cannot stat");
	  exit (1);
	}
    }

  /* If there is no destination file then create a temp and rename
     the result into the input.  */

  if (output_filename == (char *) NULL)
    {
      char *tmpname = make_tempname (input_filename);

      copy_file (input_filename, tmpname, input_target, output_target);
      if (status == 0)
	{	
	  if (preserve_dates)
	    set_times (tmpname, &statbuf);
	  smart_rename (tmpname, input_filename);
	}
      else
	unlink (tmpname);
    }
  else
    {
      copy_file (input_filename, output_filename, input_target, output_target);
      if (status == 0 && preserve_dates)
	set_times (output_filename, &statbuf);
    }

  if (adjust_warn)
    {
      for (p = adjust_sections; p != NULL; p = p->next)
	{
	  if (! p->used && p->adjust != ignore_vma)
	    {
	      fprintf (stderr, "%s: warning: --adjust-section-vma %s%c0x",
		       program_name, p->name,
		       p->adjust == set_vma ? '=' : '+');
	      fprintf_vma (stderr, p->val);
	      fprintf (stderr, " never used\n");
	    }
	}
    }

  return 0;
}

int
main (argc, argv)
     int argc;
     char *argv[];
{
  program_name = argv[0];
  xmalloc_set_program_name (program_name);

  START_PROGRESS (program_name, 0);

  strip_symbols = strip_undef;
  discard_locals = locals_undef;

  bfd_init ();
  set_default_bfd_target ();

  if (is_strip < 0)
    {
      int i = strlen (program_name);
      is_strip = (i >= 5 && strcmp (program_name + i - 5, "strip") == 0);
    }

  if (is_strip)
    strip_main (argc, argv);
  else
    copy_main (argc, argv);

  END_PROGRESS (program_name);

  return status;
}
