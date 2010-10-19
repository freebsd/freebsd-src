/* nlmconv.c -- NLM conversion program
   Copyright 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002,
   2003, 2004, 2005 Free Software Foundation, Inc.

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
   Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.  */

/* Written by Ian Lance Taylor <ian@cygnus.com>.

   This program can be used to convert any appropriate object file
   into a NetWare Loadable Module (an NLM).  It will accept a linker
   specification file which is identical to that accepted by the
   NetWare linker, NLMLINK.  */

/* AIX requires this to be the first thing in the file.  */
#ifndef __GNUC__
# ifdef _AIX
 #pragma alloca
#endif
#endif

#include "bfd.h"
#include "libiberty.h"
#include "bucomm.h"
#include "safe-ctype.h"

#include "ansidecl.h"
#include <time.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <assert.h>
#include "getopt.h"

/* Internal BFD NLM header.  */
#include "libnlm.h"
#include "nlmconv.h"

#ifdef NLMCONV_ALPHA
#include "coff/sym.h"
#include "coff/ecoff.h"
#endif

/* If strerror is just a macro, we want to use the one from libiberty
   since it will handle undefined values.  */
#undef strerror
extern char *strerror (int);

#ifndef SEEK_SET
#define SEEK_SET 0
#endif

#ifndef R_OK
#define R_OK 4
#define W_OK 2
#define X_OK 1
#endif

/* Global variables.  */

/* The name used to invoke the program.  */
char *program_name;

/* Local variables.  */

/* Whether to print out debugging information (currently just controls
   whether it prints the linker command if there is one).  */
static int debug;

/* The symbol table.  */
static asymbol **symbols;

/* A section we create in the output file to hold pointers to where
   the sections of the input file end up.  We will put a pointer to
   this section in the NLM header.  These is an entry for each input
   section.  The format is
       null terminated section name
       zeroes to adjust to 4 byte boundary
       4 byte section data file pointer
       4 byte section size
   We don't need a version number.  The way we find this information
   is by finding a stamp in the NLM header information.  If we need to
   change the format of this information, we can simply change the
   stamp.  */
static asection *secsec;

/* A temporary file name to be unlinked on exit.  Actually, for most
   errors, we leave it around.  It's not clear whether that is helpful
   or not.  */
static char *unlink_on_exit;

/* The list of long options.  */
static struct option long_options[] =
{
  { "debug", no_argument, 0, 'd' },
  { "header-file", required_argument, 0, 'T' },
  { "help", no_argument, 0, 'h' },
  { "input-target", required_argument, 0, 'I' },
  { "input-format", required_argument, 0, 'I' }, /* Obsolete */
  { "linker", required_argument, 0, 'l' },
  { "output-target", required_argument, 0, 'O' },
  { "output-format", required_argument, 0, 'O' }, /* Obsolete */
  { "version", no_argument, 0, 'V' },
  { NULL, no_argument, 0, 0 }
};

/* Local routines.  */

int main (int, char **);

static void show_usage (FILE *, int);
static const char *select_output_format
  (enum bfd_architecture, unsigned long, bfd_boolean);
static void setup_sections (bfd *, asection *, void *);
static void copy_sections (bfd *, asection *, void *);
static void mangle_relocs
  (bfd *, asection *, arelent ***, long *, char *, bfd_size_type);
static void default_mangle_relocs
  (bfd *, asection *, arelent ***, long *, char *, bfd_size_type);
static char *link_inputs (struct string_list *, char *, char *);

#ifdef NLMCONV_I386
static void i386_mangle_relocs (bfd *, asection *, arelent ***, long *, char *, bfd_size_type);
#endif

#ifdef NLMCONV_ALPHA
static void alpha_mangle_relocs (bfd *, asection *, arelent ***, long *, char *, bfd_size_type);
#endif

#ifdef NLMCONV_POWERPC
static void powerpc_build_stubs (bfd *, bfd *, asymbol ***, long *);
static void powerpc_resolve_stubs (bfd *, bfd *);
static void powerpc_mangle_relocs (bfd *, asection *, arelent ***, long *, char *, bfd_size_type);
#endif

/* The main routine.  */

int
main (int argc, char **argv)
{
  int opt;
  char *input_file = NULL;
  const char *input_format = NULL;
  const char *output_format = NULL;
  const char *header_file = NULL;
  char *ld_arg = NULL;
  Nlm_Internal_Fixed_Header fixed_hdr_struct;
  Nlm_Internal_Variable_Header var_hdr_struct;
  Nlm_Internal_Version_Header version_hdr_struct;
  Nlm_Internal_Copyright_Header copyright_hdr_struct;
  Nlm_Internal_Extended_Header extended_hdr_struct;
  bfd *inbfd;
  bfd *outbfd;
  asymbol **newsyms, **outsyms;
  long symcount, newsymalloc, newsymcount;
  long symsize;
  asection *text_sec, *bss_sec, *data_sec;
  bfd_vma vma;
  bfd_size_type align;
  asymbol *endsym;
  long i;
  char inlead, outlead;
  bfd_boolean gotstart, gotexit, gotcheck;
  struct stat st;
  FILE *custom_data = NULL;
  FILE *help_data = NULL;
  FILE *message_data = NULL;
  FILE *rpc_data = NULL;
  FILE *shared_data = NULL;
  size_t custom_size = 0;
  size_t help_size = 0;
  size_t message_size = 0;
  size_t module_size = 0;
  size_t rpc_size = 0;
  asection *custom_section = NULL;
  asection *help_section = NULL;
  asection *message_section = NULL;
  asection *module_section = NULL;
  asection *rpc_section = NULL;
  asection *shared_section = NULL;
  bfd *sharedbfd;
  size_t shared_offset = 0;
  size_t shared_size = 0;
  static Nlm_Internal_Fixed_Header sharedhdr;
  int len;
  char *modname;
  char **matching;

#if defined (HAVE_SETLOCALE) && defined (HAVE_LC_MESSAGES)
  setlocale (LC_MESSAGES, "");
#endif
#if defined (HAVE_SETLOCALE)
  setlocale (LC_CTYPE, "");
#endif
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  program_name = argv[0];
  xmalloc_set_program_name (program_name);

  expandargv (&argc, &argv);

  bfd_init ();
  set_default_bfd_target ();

  while ((opt = getopt_long (argc, argv, "dHhI:l:O:T:Vv", long_options,
			     (int *) NULL))
	 != EOF)
    {
      switch (opt)
	{
	case 'd':
	  debug = 1;
	  break;
	case 'H':
	case 'h':
	  show_usage (stdout, 0);
	  break;
	case 'I':
	  input_format = optarg;
	  break;
	case 'l':
	  ld_arg = optarg;
	  break;
	case 'O':
	  output_format = optarg;
	  break;
	case 'T':
	  header_file = optarg;
	  break;
	case 'v':
	case 'V':
	  print_version ("nlmconv");
	  break;
	case 0:
	  break;
	default:
	  show_usage (stderr, 1);
	  break;
	}
    }

  /* The input and output files may be named on the command line.  */
  output_file = NULL;
  if (optind < argc)
    {
      input_file = argv[optind];
      ++optind;
      if (optind < argc)
	{
	  output_file = argv[optind];
	  ++optind;
	  if (optind < argc)
	    show_usage (stderr, 1);
	  if (strcmp (input_file, output_file) == 0)
	    {
	      fatal (_("input and output files must be different"));
	    }
	}
    }

  /* Initialize the header information to default values.  */
  fixed_hdr = &fixed_hdr_struct;
  memset ((void *) &fixed_hdr_struct, 0, sizeof fixed_hdr_struct);
  var_hdr = &var_hdr_struct;
  memset ((void *) &var_hdr_struct, 0, sizeof var_hdr_struct);
  version_hdr = &version_hdr_struct;
  memset ((void *) &version_hdr_struct, 0, sizeof version_hdr_struct);
  copyright_hdr = &copyright_hdr_struct;
  memset ((void *) &copyright_hdr_struct, 0, sizeof copyright_hdr_struct);
  extended_hdr = &extended_hdr_struct;
  memset ((void *) &extended_hdr_struct, 0, sizeof extended_hdr_struct);
  check_procedure = NULL;
  custom_file = NULL;
  debug_info = FALSE;
  exit_procedure = "_Stop";
  export_symbols = NULL;
  map_file = NULL;
  full_map = FALSE;
  help_file = NULL;
  import_symbols = NULL;
  message_file = NULL;
  modules = NULL;
  sharelib_file = NULL;
  start_procedure = "_Prelude";
  verbose = FALSE;
  rpc_file = NULL;

  parse_errors = 0;

  /* Parse the header file (if there is one).  */
  if (header_file != NULL)
    {
      if (! nlmlex_file (header_file)
	  || yyparse () != 0
	  || parse_errors != 0)
	exit (1);
    }

  if (input_files != NULL)
    {
      if (input_file != NULL)
	{
	  fatal (_("input file named both on command line and with INPUT"));
	}
      if (input_files->next == NULL)
	input_file = input_files->string;
      else
	input_file = link_inputs (input_files, ld_arg, map_file);
    }
  else if (input_file == NULL)
    {
      non_fatal (_("no input file"));
      show_usage (stderr, 1);
    }

  inbfd = bfd_openr (input_file, input_format);
  if (inbfd == NULL)
    bfd_fatal (input_file);

  if (! bfd_check_format_matches (inbfd, bfd_object, &matching))
    {
      bfd_nonfatal (input_file);
      if (bfd_get_error () == bfd_error_file_ambiguously_recognized)
	{
	  list_matching_formats (matching);
	  free (matching);
	}
      exit (1);
    }

  if (output_format == NULL)
    output_format = select_output_format (bfd_get_arch (inbfd),
					  bfd_get_mach (inbfd),
					  bfd_big_endian (inbfd));

  assert (output_format != NULL);

  /* Use the output file named on the command line if it exists.
     Otherwise use the file named in the OUTPUT statement.  */
  if (output_file == NULL)
    {
      non_fatal (_("no name for output file"));
      show_usage (stderr, 1);
    }

  outbfd = bfd_openw (output_file, output_format);
  if (outbfd == NULL)
    bfd_fatal (output_file);
  if (! bfd_set_format (outbfd, bfd_object))
    bfd_fatal (output_file);

  assert (bfd_get_flavour (outbfd) == bfd_target_nlm_flavour);

  /* XXX: Should we accept the unknown bfd format here ?  */
  if (bfd_arch_get_compatible (inbfd, outbfd, TRUE) == NULL)
    non_fatal (_("warning: input and output formats are not compatible"));

  /* Move the values read from the command file into outbfd.  */
  *nlm_fixed_header (outbfd) = fixed_hdr_struct;
  *nlm_variable_header (outbfd) = var_hdr_struct;
  *nlm_version_header (outbfd) = version_hdr_struct;
  *nlm_copyright_header (outbfd) = copyright_hdr_struct;
  *nlm_extended_header (outbfd) = extended_hdr_struct;

  /* Start copying the input BFD to the output BFD.  */
  if (! bfd_set_file_flags (outbfd, bfd_get_file_flags (inbfd)))
    bfd_fatal (bfd_get_filename (outbfd));

  symsize = bfd_get_symtab_upper_bound (inbfd);
  if (symsize < 0)
    bfd_fatal (input_file);
  symbols = (asymbol **) xmalloc (symsize);
  symcount = bfd_canonicalize_symtab (inbfd, symbols);
  if (symcount < 0)
    bfd_fatal (input_file);

  /* Make sure we have a .bss section.  */
  bss_sec = bfd_get_section_by_name (outbfd, NLM_UNINITIALIZED_DATA_NAME);
  if (bss_sec == NULL)
    {
      bss_sec = bfd_make_section (outbfd, NLM_UNINITIALIZED_DATA_NAME);
      if (bss_sec == NULL
	  || ! bfd_set_section_flags (outbfd, bss_sec, SEC_ALLOC)
	  || ! bfd_set_section_alignment (outbfd, bss_sec, 1))
	bfd_fatal (_("make .bss section"));
    }

  /* We store the original section names in the .nlmsections section,
     so that programs which understand it can resurrect the original
     sections from the NLM.  We will put a pointer to .nlmsections in
     the NLM header area.  */
  secsec = bfd_make_section (outbfd, ".nlmsections");
  if (secsec == NULL)
    bfd_fatal (_("make .nlmsections section"));
  if (! bfd_set_section_flags (outbfd, secsec, SEC_HAS_CONTENTS))
    bfd_fatal (_("set .nlmsections flags"));

#ifdef NLMCONV_POWERPC
  /* For PowerPC NetWare we need to build stubs for calls to undefined
     symbols.  Because each stub requires an entry in the TOC section
     which must be at the same location as other entries in the TOC
     section, we must do this before determining where the TOC section
     goes in setup_sections.  */
  if (bfd_get_arch (inbfd) == bfd_arch_powerpc)
    powerpc_build_stubs (inbfd, outbfd, &symbols, &symcount);
#endif

  /* Set up the sections.  */
  bfd_map_over_sections (inbfd, setup_sections, (void *) outbfd);

  text_sec = bfd_get_section_by_name (outbfd, NLM_CODE_NAME);

  /* The .bss section immediately follows the .data section.  */
  data_sec = bfd_get_section_by_name (outbfd, NLM_INITIALIZED_DATA_NAME);
  if (data_sec != NULL)
    {
      bfd_size_type add;

      vma = bfd_get_section_size (data_sec);
      align = 1 << bss_sec->alignment_power;
      add = ((vma + align - 1) &~ (align - 1)) - vma;
      vma += add;
      if (! bfd_set_section_vma (outbfd, bss_sec, vma))
	bfd_fatal (_("set .bss vma"));
      if (add != 0)
	{
	  bfd_size_type data_size;

	  data_size = bfd_get_section_size (data_sec);
	  if (! bfd_set_section_size (outbfd, data_sec, data_size + add))
	    bfd_fatal (_("set .data size"));
	}
    }

  /* Adjust symbol information.  */
  inlead = bfd_get_symbol_leading_char (inbfd);
  outlead = bfd_get_symbol_leading_char (outbfd);
  gotstart = FALSE;
  gotexit = FALSE;
  gotcheck = FALSE;
  newsymalloc = 10;
  newsyms = (asymbol **) xmalloc (newsymalloc * sizeof (asymbol *));
  newsymcount = 0;
  endsym = NULL;
  for (i = 0; i < symcount; i++)
    {
      asymbol *sym;

      sym = symbols[i];

      /* Add or remove a leading underscore.  */
      if (inlead != outlead)
	{
	  if (inlead != '\0')
	    {
	      if (bfd_asymbol_name (sym)[0] == inlead)
		{
		  if (outlead == '\0')
		    ++sym->name;
		  else
		    {
		      char *new;

		      new = xmalloc (strlen (bfd_asymbol_name (sym)) + 1);
		      new[0] = outlead;
		      strcpy (new + 1, bfd_asymbol_name (sym) + 1);
		      sym->name = new;
		    }
		}
	    }
	  else
	    {
	      char *new;

	      new = xmalloc (strlen (bfd_asymbol_name (sym)) + 2);
	      new[0] = outlead;
	      strcpy (new + 1, bfd_asymbol_name (sym));
	      sym->name = new;
	    }
	}

      /* NLM's have an uninitialized data section, but they do not
	 have a common section in the Unix sense.  Move all common
	 symbols into the .bss section, and mark them as exported.  */
      if (bfd_is_com_section (bfd_get_section (sym)))
	{
	  bfd_vma size = sym->value;

	  sym->section = bss_sec;
	  sym->value = bfd_get_section_size (bss_sec);
	  size += sym->value;
	  align = 1 << bss_sec->alignment_power;
	  size = (size + align - 1) & ~(align - 1);
	  bfd_set_section_size (outbfd, bss_sec, size);
	  sym->flags |= BSF_EXPORT | BSF_GLOBAL;
	}
      else if (bfd_get_section (sym)->output_section != NULL)
	{
	  /* Move the symbol into the output section.  */
	  sym->value += bfd_get_section (sym)->output_offset;
	  sym->section = bfd_get_section (sym)->output_section;
	  /* This is no longer a section symbol.  */
	  sym->flags &=~ BSF_SECTION_SYM;
	}

      /* Force _edata and _end to be defined.  This would normally be
	 done by the linker, but the manipulation of the common
	 symbols will confuse it.  */
      if ((sym->flags & BSF_DEBUGGING) == 0
	  && bfd_asymbol_name (sym)[0] == '_'
	  && bfd_is_und_section (bfd_get_section (sym)))
	{
	  if (strcmp (bfd_asymbol_name (sym), "_edata") == 0)
	    {
	      sym->section = bss_sec;
	      sym->value = 0;
	    }
	  if (strcmp (bfd_asymbol_name (sym), "_end") == 0)
	    {
	      sym->section = bss_sec;
	      endsym = sym;
	    }

#ifdef NLMCONV_POWERPC
	  /* For PowerPC NetWare, we define __GOT0.  This is the start
	     of the .got section.  */
	  if (bfd_get_arch (inbfd) == bfd_arch_powerpc
	      && strcmp (bfd_asymbol_name (sym), "__GOT0") == 0)
	    {
	      asection *got_sec;

	      got_sec = bfd_get_section_by_name (inbfd, ".got");
	      assert (got_sec != (asection *) NULL);
	      sym->value = got_sec->output_offset;
	      sym->section = got_sec->output_section;
	    }
#endif
	}

      /* If this is a global symbol, check the export list.  */
      if ((sym->flags & (BSF_EXPORT | BSF_GLOBAL)) != 0)
	{
	  struct string_list *l;
	  int found_simple;

	  /* Unfortunately, a symbol can appear multiple times on the
	     export list, with and without prefixes.  */
	  found_simple = 0;
	  for (l = export_symbols; l != NULL; l = l->next)
	    {
	      if (strcmp (l->string, bfd_asymbol_name (sym)) == 0)
		found_simple = 1;
	      else
		{
		  char *zbase;

		  zbase = strchr (l->string, '@');
		  if (zbase != NULL
		      && strcmp (zbase + 1, bfd_asymbol_name (sym)) == 0)
		    {
		      /* We must add a symbol with this prefix.  */
		      if (newsymcount >= newsymalloc)
			{
			  newsymalloc += 10;
			  newsyms = ((asymbol **)
				     xrealloc ((void *) newsyms,
					       (newsymalloc
						* sizeof (asymbol *))));
			}
		      newsyms[newsymcount] =
			(asymbol *) xmalloc (sizeof (asymbol));
		      *newsyms[newsymcount] = *sym;
		      newsyms[newsymcount]->name = l->string;
		      ++newsymcount;
		    }
		}
	    }
	  if (! found_simple)
	    {
	      /* The unmodified symbol is actually not exported at
		 all.  */
	      sym->flags &=~ (BSF_GLOBAL | BSF_EXPORT);
	      sym->flags |= BSF_LOCAL;
	    }
	}

      /* If it's an undefined symbol, see if it's on the import list.
	 Change the prefix if necessary.  */
      if (bfd_is_und_section (bfd_get_section (sym)))
	{
	  struct string_list *l;

	  for (l = import_symbols; l != NULL; l = l->next)
	    {
	      if (strcmp (l->string, bfd_asymbol_name (sym)) == 0)
		break;
	      else
		{
		  char *zbase;

		  zbase = strchr (l->string, '@');
		  if (zbase != NULL
		      && strcmp (zbase + 1, bfd_asymbol_name (sym)) == 0)
		    {
		      sym->name = l->string;
		      break;
		    }
		}
	    }
	  if (l == NULL)
	    non_fatal (_("warning: symbol %s imported but not in import list"),
		       bfd_asymbol_name (sym));
	}

      /* See if it's one of the special named symbols.  */
      if ((sym->flags & BSF_DEBUGGING) == 0)
	{
	  bfd_vma val;

	  /* FIXME: If these symbols are not in the .text section, we
	     add the .text section size to the value.  This may not be
	     correct for all targets.  I'm not sure how this should
	     really be handled.  */
	  if (strcmp (bfd_asymbol_name (sym), start_procedure) == 0)
	    {
	      val = bfd_asymbol_value (sym);
	      if (bfd_get_section (sym) == data_sec
		  && text_sec != (asection *) NULL)
		val += bfd_section_size (outbfd, text_sec);
	      if (! bfd_set_start_address (outbfd, val))
		bfd_fatal (_("set start address"));
	      gotstart = TRUE;
	    }
	  if (strcmp (bfd_asymbol_name (sym), exit_procedure) == 0)
	    {
	      val = bfd_asymbol_value (sym);
	      if (bfd_get_section (sym) == data_sec
		  && text_sec != (asection *) NULL)
		val += bfd_section_size (outbfd, text_sec);
	      nlm_fixed_header (outbfd)->exitProcedureOffset = val;
	      gotexit = TRUE;
	    }
	  if (check_procedure != NULL
	      && strcmp (bfd_asymbol_name (sym), check_procedure) == 0)
	    {
	      val = bfd_asymbol_value (sym);
	      if (bfd_get_section (sym) == data_sec
		  && text_sec != (asection *) NULL)
		val += bfd_section_size (outbfd, text_sec);
	      nlm_fixed_header (outbfd)->checkUnloadProcedureOffset = val;
	      gotcheck = TRUE;
	    }
	}
    }

  if (endsym != NULL)
    {
      endsym->value = bfd_get_section_size (bss_sec);

      /* FIXME: If any relocs referring to _end use inplace addends,
	 then I think they need to be updated.  This is handled by
	 i386_mangle_relocs.  Is it needed for any other object
	 formats?  */
    }

  if (newsymcount == 0)
    outsyms = symbols;
  else
    {
      outsyms = (asymbol **) xmalloc ((symcount + newsymcount + 1)
				      * sizeof (asymbol *));
      memcpy (outsyms, symbols, symcount * sizeof (asymbol *));
      memcpy (outsyms + symcount, newsyms, newsymcount * sizeof (asymbol *));
      outsyms[symcount + newsymcount] = NULL;
    }

  bfd_set_symtab (outbfd, outsyms, symcount + newsymcount);

  if (! gotstart)
    non_fatal (_("warning: START procedure %s not defined"), start_procedure);
  if (! gotexit)
    non_fatal (_("warning: EXIT procedure %s not defined"), exit_procedure);
  if (check_procedure != NULL && ! gotcheck)
    non_fatal (_("warning: CHECK procedure %s not defined"), check_procedure);

  /* Add additional sections required for the header information.  */
  if (custom_file != NULL)
    {
      custom_data = fopen (custom_file, "r");
      if (custom_data == NULL
	  || fstat (fileno (custom_data), &st) < 0)
	{
	  fprintf (stderr, "%s:%s: %s\n", program_name, custom_file,
		   strerror (errno));
	  custom_file = NULL;
	}
      else
	{
	  custom_size = st.st_size;
	  custom_section = bfd_make_section (outbfd, ".nlmcustom");
	  if (custom_section == NULL
	      || ! bfd_set_section_size (outbfd, custom_section, custom_size)
	      || ! bfd_set_section_flags (outbfd, custom_section,
					  SEC_HAS_CONTENTS))
	    bfd_fatal (_("custom section"));
	}
    }
  if (help_file != NULL)
    {
      help_data = fopen (help_file, "r");
      if (help_data == NULL
	  || fstat (fileno (help_data), &st) < 0)
	{
	  fprintf (stderr, "%s:%s: %s\n", program_name, help_file,
		   strerror (errno));
	  help_file = NULL;
	}
      else
	{
	  help_size = st.st_size;
	  help_section = bfd_make_section (outbfd, ".nlmhelp");
	  if (help_section == NULL
	      || ! bfd_set_section_size (outbfd, help_section, help_size)
	      || ! bfd_set_section_flags (outbfd, help_section,
					  SEC_HAS_CONTENTS))
	    bfd_fatal (_("help section"));
	  strncpy (nlm_extended_header (outbfd)->stamp, "MeSsAgEs", 8);
	}
    }
  if (message_file != NULL)
    {
      message_data = fopen (message_file, "r");
      if (message_data == NULL
	  || fstat (fileno (message_data), &st) < 0)
	{
	  fprintf (stderr, "%s:%s: %s\n", program_name, message_file,
		   strerror (errno));
	  message_file = NULL;
	}
      else
	{
	  message_size = st.st_size;
	  message_section = bfd_make_section (outbfd, ".nlmmessages");
	  if (message_section == NULL
	      || ! bfd_set_section_size (outbfd, message_section, message_size)
	      || ! bfd_set_section_flags (outbfd, message_section,
					  SEC_HAS_CONTENTS))
	    bfd_fatal (_("message section"));
	  strncpy (nlm_extended_header (outbfd)->stamp, "MeSsAgEs", 8);
	}
    }
  if (modules != NULL)
    {
      struct string_list *l;

      module_size = 0;
      for (l = modules; l != NULL; l = l->next)
	module_size += strlen (l->string) + 1;
      module_section = bfd_make_section (outbfd, ".nlmmodules");
      if (module_section == NULL
	  || ! bfd_set_section_size (outbfd, module_section, module_size)
	  || ! bfd_set_section_flags (outbfd, module_section,
				      SEC_HAS_CONTENTS))
	bfd_fatal (_("module section"));
    }
  if (rpc_file != NULL)
    {
      rpc_data = fopen (rpc_file, "r");
      if (rpc_data == NULL
	  || fstat (fileno (rpc_data), &st) < 0)
	{
	  fprintf (stderr, "%s:%s: %s\n", program_name, rpc_file,
		   strerror (errno));
	  rpc_file = NULL;
	}
      else
	{
	  rpc_size = st.st_size;
	  rpc_section = bfd_make_section (outbfd, ".nlmrpc");
	  if (rpc_section == NULL
	      || ! bfd_set_section_size (outbfd, rpc_section, rpc_size)
	      || ! bfd_set_section_flags (outbfd, rpc_section,
					  SEC_HAS_CONTENTS))
	    bfd_fatal (_("rpc section"));
	  strncpy (nlm_extended_header (outbfd)->stamp, "MeSsAgEs", 8);
	}
    }
  if (sharelib_file != NULL)
    {
      sharedbfd = bfd_openr (sharelib_file, output_format);
      if (sharedbfd == NULL
	  || ! bfd_check_format (sharedbfd, bfd_object))
	{
	  fprintf (stderr, "%s:%s: %s\n", program_name, sharelib_file,
		   bfd_errmsg (bfd_get_error ()));
	  sharelib_file = NULL;
	}
      else
	{
	  sharedhdr = *nlm_fixed_header (sharedbfd);
	  bfd_close (sharedbfd);
	  shared_data = fopen (sharelib_file, "r");
	  if (shared_data == NULL
	      || (fstat (fileno (shared_data), &st) < 0))
	    {
	      fprintf (stderr, "%s:%s: %s\n", program_name, sharelib_file,
		       strerror (errno));
	      sharelib_file = NULL;
	    }
	  else
	    {
	      /* If we were clever, we could just copy out the
		 sections of the shared library which we actually
		 need.  However, we would have to figure out the sizes
		 of the external and public information, and that can
		 not be done without reading through them.  */
	      if (sharedhdr.uninitializedDataSize > 0)
		{
		  /* There is no place to record this information.  */
		  non_fatal (_("%s: warning: shared libraries can not have uninitialized data"),
			     sharelib_file);
		}
	      shared_offset = st.st_size;
	      if (shared_offset > (size_t) sharedhdr.codeImageOffset)
		shared_offset = sharedhdr.codeImageOffset;
	      if (shared_offset > (size_t) sharedhdr.dataImageOffset)
		shared_offset = sharedhdr.dataImageOffset;
	      if (shared_offset > (size_t) sharedhdr.relocationFixupOffset)
		shared_offset = sharedhdr.relocationFixupOffset;
	      if (shared_offset > (size_t) sharedhdr.externalReferencesOffset)
		shared_offset = sharedhdr.externalReferencesOffset;
	      if (shared_offset > (size_t) sharedhdr.publicsOffset)
		shared_offset = sharedhdr.publicsOffset;
	      shared_size = st.st_size - shared_offset;
	      shared_section = bfd_make_section (outbfd, ".nlmshared");
	      if (shared_section == NULL
		  || ! bfd_set_section_size (outbfd, shared_section,
					     shared_size)
		  || ! bfd_set_section_flags (outbfd, shared_section,
					      SEC_HAS_CONTENTS))
		bfd_fatal (_("shared section"));
	      strncpy (nlm_extended_header (outbfd)->stamp, "MeSsAgEs", 8);
	    }
	}
    }

  /* Check whether a version was given.  */
  if (strncmp (version_hdr->stamp, "VeRsIoN#", 8) != 0)
    non_fatal (_("warning: No version number given"));

  /* At least for now, always create an extended header, because that
     is what NLMLINK does.  */
  strncpy (nlm_extended_header (outbfd)->stamp, "MeSsAgEs", 8);

  strncpy (nlm_cygnus_ext_header (outbfd)->stamp, "CyGnUsEx", 8);

  /* If the date was not given, force it in.  */
  if (nlm_version_header (outbfd)->month == 0
      && nlm_version_header (outbfd)->day == 0
      && nlm_version_header (outbfd)->year == 0)
    {
      time_t now;
      struct tm *ptm;

      time (&now);
      ptm = localtime (&now);
      nlm_version_header (outbfd)->month = ptm->tm_mon + 1;
      nlm_version_header (outbfd)->day = ptm->tm_mday;
      nlm_version_header (outbfd)->year = ptm->tm_year + 1900;
      strncpy (version_hdr->stamp, "VeRsIoN#", 8);
    }

#ifdef NLMCONV_POWERPC
  /* Resolve the stubs we build for PowerPC NetWare.  */
  if (bfd_get_arch (inbfd) == bfd_arch_powerpc)
    powerpc_resolve_stubs (inbfd, outbfd);
#endif

  /* Copy over the sections.  */
  bfd_map_over_sections (inbfd, copy_sections, (void *) outbfd);

  /* Finish up the header information.  */
  if (custom_file != NULL)
    {
      void *data;

      data = xmalloc (custom_size);
      if (fread (data, 1, custom_size, custom_data) != custom_size)
	non_fatal (_("%s: read: %s"), custom_file, strerror (errno));
      else
	{
	  if (! bfd_set_section_contents (outbfd, custom_section, data,
					  (file_ptr) 0, custom_size))
	    bfd_fatal (_("custom section"));
	  nlm_fixed_header (outbfd)->customDataOffset =
	    custom_section->filepos;
	  nlm_fixed_header (outbfd)->customDataSize = custom_size;
	}
      free (data);
    }
  if (! debug_info)
    {
      /* As a special hack, the backend recognizes a debugInfoOffset
	 of -1 to mean that it should not output any debugging
	 information.  This can not be handling by fiddling with the
	 symbol table because exported symbols appear in both the
	 export information and the debugging information.  */
      nlm_fixed_header (outbfd)->debugInfoOffset = (file_ptr) -1;
    }
  if (full_map)
    non_fatal (_("warning: FULLMAP is not supported; try ld -M"));
  if (help_file != NULL)
    {
      void *data;

      data = xmalloc (help_size);
      if (fread (data, 1, help_size, help_data) != help_size)
	non_fatal (_("%s: read: %s"), help_file, strerror (errno));
      else
	{
	  if (! bfd_set_section_contents (outbfd, help_section, data,
					  (file_ptr) 0, help_size))
	    bfd_fatal (_("help section"));
	  nlm_extended_header (outbfd)->helpFileOffset =
	    help_section->filepos;
	  nlm_extended_header (outbfd)->helpFileLength = help_size;
	}
      free (data);
    }
  if (message_file != NULL)
    {
      void *data;

      data = xmalloc (message_size);
      if (fread (data, 1, message_size, message_data) != message_size)
	non_fatal (_("%s: read: %s"), message_file, strerror (errno));
      else
	{
	  if (! bfd_set_section_contents (outbfd, message_section, data,
					  (file_ptr) 0, message_size))
	    bfd_fatal (_("message section"));
	  nlm_extended_header (outbfd)->messageFileOffset =
	    message_section->filepos;
	  nlm_extended_header (outbfd)->messageFileLength = message_size;

	  /* FIXME: Are these offsets correct on all platforms?  Are
	     they 32 bits on all platforms?  What endianness?  */
	  nlm_extended_header (outbfd)->languageID =
	    bfd_h_get_32 (outbfd, (bfd_byte *) data + 106);
	  nlm_extended_header (outbfd)->messageCount =
	    bfd_h_get_32 (outbfd, (bfd_byte *) data + 110);
	}
      free (data);
    }
  if (modules != NULL)
    {
      void *data;
      unsigned char *set;
      struct string_list *l;
      bfd_size_type c;

      data = xmalloc (module_size);
      c = 0;
      set = (unsigned char *) data;
      for (l = modules; l != NULL; l = l->next)
	{
	  *set = strlen (l->string);
	  strncpy ((char *) set + 1, l->string, *set);
	  set += *set + 1;
	  ++c;
	}
      if (! bfd_set_section_contents (outbfd, module_section, data,
				      (file_ptr) 0, module_size))
	bfd_fatal (_("module section"));
      nlm_fixed_header (outbfd)->moduleDependencyOffset =
	module_section->filepos;
      nlm_fixed_header (outbfd)->numberOfModuleDependencies = c;
    }
  if (rpc_file != NULL)
    {
      void *data;

      data = xmalloc (rpc_size);
      if (fread (data, 1, rpc_size, rpc_data) != rpc_size)
	non_fatal (_("%s: read: %s"), rpc_file, strerror (errno));
      else
	{
	  if (! bfd_set_section_contents (outbfd, rpc_section, data,
					  (file_ptr) 0, rpc_size))
	    bfd_fatal (_("rpc section"));
	  nlm_extended_header (outbfd)->RPCDataOffset =
	    rpc_section->filepos;
	  nlm_extended_header (outbfd)->RPCDataLength = rpc_size;
	}
      free (data);
    }
  if (sharelib_file != NULL)
    {
      void *data;

      data = xmalloc (shared_size);
      if (fseek (shared_data, shared_offset, SEEK_SET) != 0
	  || fread (data, 1, shared_size, shared_data) != shared_size)
	non_fatal (_("%s: read: %s"), sharelib_file, strerror (errno));
      else
	{
	  if (! bfd_set_section_contents (outbfd, shared_section, data,
					  (file_ptr) 0, shared_size))
	    bfd_fatal (_("shared section"));
	}
      nlm_extended_header (outbfd)->sharedCodeOffset =
	sharedhdr.codeImageOffset - shared_offset + shared_section->filepos;
      nlm_extended_header (outbfd)->sharedCodeLength =
	sharedhdr.codeImageSize;
      nlm_extended_header (outbfd)->sharedDataOffset =
	sharedhdr.dataImageOffset - shared_offset + shared_section->filepos;
      nlm_extended_header (outbfd)->sharedDataLength =
	sharedhdr.dataImageSize;
      nlm_extended_header (outbfd)->sharedRelocationFixupOffset =
	(sharedhdr.relocationFixupOffset
	 - shared_offset
	 + shared_section->filepos);
      nlm_extended_header (outbfd)->sharedRelocationFixupCount =
	sharedhdr.numberOfRelocationFixups;
      nlm_extended_header (outbfd)->sharedExternalReferenceOffset =
	(sharedhdr.externalReferencesOffset
	 - shared_offset
	 + shared_section->filepos);
      nlm_extended_header (outbfd)->sharedExternalReferenceCount =
	sharedhdr.numberOfExternalReferences;
      nlm_extended_header (outbfd)->sharedPublicsOffset =
	sharedhdr.publicsOffset - shared_offset + shared_section->filepos;
      nlm_extended_header (outbfd)->sharedPublicsCount =
	sharedhdr.numberOfPublics;
      nlm_extended_header (outbfd)->sharedDebugRecordOffset =
	sharedhdr.debugInfoOffset - shared_offset + shared_section->filepos;
      nlm_extended_header (outbfd)->sharedDebugRecordCount =
	sharedhdr.numberOfDebugRecords;
      nlm_extended_header (outbfd)->SharedInitializationOffset =
	sharedhdr.codeStartOffset;
      nlm_extended_header (outbfd)->SharedExitProcedureOffset =
	sharedhdr.exitProcedureOffset;
      free (data);
    }

  {
    const int    max_len  = NLM_MODULE_NAME_SIZE - 2;
    const char * filename = lbasename (output_file);
    
    len = strlen (filename);
    if (len > max_len)
      len = max_len;
    nlm_fixed_header (outbfd)->moduleName[0] = len;

    strncpy (nlm_fixed_header (outbfd)->moduleName + 1, filename, max_len);
    nlm_fixed_header (outbfd)->moduleName[max_len + 1] = '\0';

    for (modname = nlm_fixed_header (outbfd)->moduleName;
	 *modname != '\0';
	 modname++)
      *modname = TOUPPER (*modname);
  }

  strncpy (nlm_variable_header (outbfd)->oldThreadName, " LONG",
	   NLM_OLD_THREAD_NAME_LENGTH);

  nlm_cygnus_ext_header (outbfd)->offset = secsec->filepos;
  nlm_cygnus_ext_header (outbfd)->length = bfd_section_size (outbfd, secsec);

  if (! bfd_close (outbfd))
    bfd_fatal (output_file);
  if (! bfd_close (inbfd))
    bfd_fatal (input_file);

  if (unlink_on_exit != NULL)
    unlink (unlink_on_exit);

  return 0;
}


/* Show a usage message and exit.  */

static void
show_usage (FILE *file, int status)
{
  fprintf (file, _("Usage: %s [option(s)] [in-file [out-file]]\n"), program_name);
  fprintf (file, _(" Convert an object file into a NetWare Loadable Module\n"));
  fprintf (file, _(" The options are:\n\
  -I --input-target=<bfdname>   Set the input binary file format\n\
  -O --output-target=<bfdname>  Set the output binary file format\n\
  -T --header-file=<file>       Read <file> for NLM header information\n\
  -l --linker=<linker>          Use <linker> for any linking\n\
  -d --debug                    Display on stderr the linker command line\n\
  @<file>                       Read options from <file>.\n\
  -h --help                     Display this information\n\
  -v --version                  Display the program's version\n\
"));
  if (status == 0)
    fprintf (file, _("Report bugs to %s\n"), REPORT_BUGS_TO);
  exit (status);
}

/* Select the output format based on the input architecture, machine,
   and endianness.  This chooses the appropriate NLM target.  */

static const char *
select_output_format (enum bfd_architecture arch, unsigned long mach,
		      bfd_boolean bigendian ATTRIBUTE_UNUSED)
{
  switch (arch)
    {
#ifdef NLMCONV_I386
    case bfd_arch_i386:
      return "nlm32-i386";
#endif
#ifdef NLMCONV_SPARC
    case bfd_arch_sparc:
      return "nlm32-sparc";
#endif
#ifdef NLMCONV_ALPHA
    case bfd_arch_alpha:
      return "nlm32-alpha";
#endif
#ifdef NLMCONV_POWERPC
    case bfd_arch_powerpc:
      return "nlm32-powerpc";
#endif
    default:
      fatal (_("support not compiled in for %s"),
	     bfd_printable_arch_mach (arch, mach));
    }
  /*NOTREACHED*/
}

/* The BFD sections are copied in two passes.  This function selects
   the output section for each input section, and sets up the section
   name, size, etc.  */

static void
setup_sections (bfd *inbfd ATTRIBUTE_UNUSED, asection *insec, void *data_ptr)
{
  bfd *outbfd = (bfd *) data_ptr;
  flagword f;
  const char *outname;
  asection *outsec;
  bfd_vma offset;
  bfd_size_type align;
  bfd_size_type add;
  bfd_size_type secsecsize;

  f = bfd_get_section_flags (inbfd, insec);
  if (f & SEC_CODE)
    outname = NLM_CODE_NAME;
  else if ((f & SEC_LOAD) && (f & SEC_HAS_CONTENTS))
    outname = NLM_INITIALIZED_DATA_NAME;
  else if (f & SEC_ALLOC)
    outname = NLM_UNINITIALIZED_DATA_NAME;
  else
    outname = bfd_section_name (inbfd, insec);

  outsec = bfd_get_section_by_name (outbfd, outname);
  if (outsec == NULL)
    {
      outsec = bfd_make_section (outbfd, outname);
      if (outsec == NULL)
	bfd_fatal (_("make section"));
    }

  insec->output_section = outsec;

  offset = bfd_section_size (outbfd, outsec);
  align = 1 << bfd_section_alignment (inbfd, insec);
  add = ((offset + align - 1) &~ (align - 1)) - offset;
  insec->output_offset = offset + add;

  if (! bfd_set_section_size (outbfd, outsec,
			      (bfd_section_size (outbfd, outsec)
			       + bfd_section_size (inbfd, insec)
			       + add)))
    bfd_fatal (_("set section size"));

  if ((bfd_section_alignment (inbfd, insec)
       > bfd_section_alignment (outbfd, outsec))
      && ! bfd_set_section_alignment (outbfd, outsec,
				      bfd_section_alignment (inbfd, insec)))
    bfd_fatal (_("set section alignment"));

  if (! bfd_set_section_flags (outbfd, outsec,
			       f | bfd_get_section_flags (outbfd, outsec)))
    bfd_fatal (_("set section flags"));

  bfd_set_reloc (outbfd, outsec, (arelent **) NULL, 0);

  /* For each input section we allocate space for an entry in
     .nlmsections.  */
  secsecsize = bfd_section_size (outbfd, secsec);
  secsecsize += strlen (bfd_section_name (inbfd, insec)) + 1;
  secsecsize = (secsecsize + 3) &~ 3;
  secsecsize += 8;
  if (! bfd_set_section_size (outbfd, secsec, secsecsize))
    bfd_fatal (_("set .nlmsections size"));
}

/* Copy the section contents.  */

static void
copy_sections (bfd *inbfd, asection *insec, void *data_ptr)
{
  static bfd_size_type secsecoff = 0;
  bfd *outbfd = (bfd *) data_ptr;
  const char *inname;
  asection *outsec;
  bfd_size_type size;
  void *contents;
  long reloc_size;
  bfd_byte buf[4];
  bfd_size_type add;

  inname = bfd_section_name (inbfd, insec);

  outsec = insec->output_section;
  assert (outsec != NULL);

  size = bfd_get_section_size (insec);

  if ((bfd_get_section_flags (inbfd, insec) & SEC_HAS_CONTENTS) == 0)
    contents = NULL;
  else
    {
      contents = xmalloc (size);
      if (! bfd_get_section_contents (inbfd, insec, contents,
				      (file_ptr) 0, size))
	bfd_fatal (bfd_get_filename (inbfd));
    }

  reloc_size = bfd_get_reloc_upper_bound (inbfd, insec);
  if (reloc_size < 0)
    bfd_fatal (bfd_get_filename (inbfd));
  if (reloc_size != 0)
    {
      arelent **relocs;
      long reloc_count;

      relocs = (arelent **) xmalloc (reloc_size);
      reloc_count = bfd_canonicalize_reloc (inbfd, insec, relocs, symbols);
      if (reloc_count < 0)
	bfd_fatal (bfd_get_filename (inbfd));
      mangle_relocs (outbfd, insec, &relocs, &reloc_count, (char *) contents,
		     size);

      /* FIXME: refers to internal BFD fields.  */
      if (outsec->orelocation != (arelent **) NULL)
	{
	  bfd_size_type total_count;
	  arelent **combined;

	  total_count = reloc_count + outsec->reloc_count;
	  combined = (arelent **) xmalloc (total_count * sizeof (arelent *));
	  memcpy (combined, outsec->orelocation,
		  outsec->reloc_count * sizeof (arelent *));
	  memcpy (combined + outsec->reloc_count, relocs,
		  (size_t) (reloc_count * sizeof (arelent *)));
	  free (outsec->orelocation);
	  reloc_count = total_count;
	  relocs = combined;
	}

      bfd_set_reloc (outbfd, outsec, relocs, reloc_count);
    }

  if (contents != NULL)
    {
      if (! bfd_set_section_contents (outbfd, outsec, contents,
				      insec->output_offset, size))
	bfd_fatal (bfd_get_filename (outbfd));
      free (contents);
    }

  /* Add this section to .nlmsections.  */
  if (! bfd_set_section_contents (outbfd, secsec, (void *) inname, secsecoff,
				  strlen (inname) + 1))
    bfd_fatal (_("set .nlmsection contents"));
  secsecoff += strlen (inname) + 1;

  add = ((secsecoff + 3) &~ 3) - secsecoff;
  if (add != 0)
    {
      bfd_h_put_32 (outbfd, (bfd_vma) 0, buf);
      if (! bfd_set_section_contents (outbfd, secsec, buf, secsecoff, add))
	bfd_fatal (_("set .nlmsection contents"));
      secsecoff += add;
    }

  if (contents != NULL)
    bfd_h_put_32 (outbfd, (bfd_vma) outsec->filepos, buf);
  else
    bfd_h_put_32 (outbfd, (bfd_vma) 0, buf);
  if (! bfd_set_section_contents (outbfd, secsec, buf, secsecoff, 4))
    bfd_fatal (_("set .nlmsection contents"));
  secsecoff += 4;

  bfd_h_put_32 (outbfd, (bfd_vma) size, buf);
  if (! bfd_set_section_contents (outbfd, secsec, buf, secsecoff, 4))
    bfd_fatal (_("set .nlmsection contents"));
  secsecoff += 4;
}

/* Some, perhaps all, NetWare targets require changing the relocs used
   by the input formats.  */

static void
mangle_relocs (bfd *outbfd, asection *insec, arelent ***relocs_ptr,
	       long *reloc_count_ptr, char *contents,
	       bfd_size_type contents_size)
{
  switch (bfd_get_arch (outbfd))
    {
#ifdef NLMCONV_I386
    case bfd_arch_i386:
      i386_mangle_relocs (outbfd, insec, relocs_ptr, reloc_count_ptr,
			  contents, contents_size);
      break;
#endif
#ifdef NLMCONV_ALPHA
    case bfd_arch_alpha:
      alpha_mangle_relocs (outbfd, insec, relocs_ptr, reloc_count_ptr,
			   contents, contents_size);
      break;
#endif
#ifdef NLMCONV_POWERPC
    case bfd_arch_powerpc:
      powerpc_mangle_relocs (outbfd, insec, relocs_ptr, reloc_count_ptr,
			     contents, contents_size);
      break;
#endif
    default:
      default_mangle_relocs (outbfd, insec, relocs_ptr, reloc_count_ptr,
			     contents, contents_size);
      break;
    }
}

/* By default all we need to do for relocs is change the address by
   the output_offset.  */

static void
default_mangle_relocs (bfd *outbfd ATTRIBUTE_UNUSED, asection *insec,
		       arelent ***relocs_ptr, long *reloc_count_ptr,
		       char *contents ATTRIBUTE_UNUSED,
		       bfd_size_type contents_size ATTRIBUTE_UNUSED)
{
  if (insec->output_offset != 0)
    {
      long reloc_count;
      arelent **relocs;
      long i;

      reloc_count = *reloc_count_ptr;
      relocs = *relocs_ptr;
      for (i = 0; i < reloc_count; i++, relocs++)
	(*relocs)->address += insec->output_offset;
    }
}

#ifdef NLMCONV_I386

/* NetWare on the i386 supports a restricted set of relocs, which are
   different from those used on other i386 targets.  This routine
   converts the relocs.  It is, obviously, very target dependent.  At
   the moment, the nlm32-i386 backend performs similar translations;
   however, it is more reliable and efficient to do them here.  */

static reloc_howto_type nlm_i386_pcrel_howto =
  HOWTO (1,			/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 0,			/* special_function */
	 "DISP32",		/* name */
	 TRUE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 TRUE);			/* pcrel_offset */

static void
i386_mangle_relocs (bfd *outbfd, asection *insec, arelent ***relocs_ptr,
		    long *reloc_count_ptr, char *contents,
		    bfd_size_type contents_size)
{
  long reloc_count, i;
  arelent **relocs;

  reloc_count = *reloc_count_ptr;
  relocs = *relocs_ptr;
  for (i = 0; i < reloc_count; i++)
    {
      arelent *rel;
      asymbol *sym;
      bfd_size_type address;
      bfd_vma addend;

      rel = *relocs++;
      sym = *rel->sym_ptr_ptr;

      /* We're moving the relocs from the input section to the output
	 section, so we must adjust the address accordingly.  */
      address = rel->address;
      rel->address += insec->output_offset;

      /* Note that no serious harm will ensue if we fail to change a
	 reloc.  The backend will fail when writing out the reloc.  */

      /* Make sure this reloc is within the data we have.  We use only
	 4 byte relocs here, so we insist on having 4 bytes.  */
      if (address + 4 > contents_size)
	continue;

      /* A PC relative reloc entirely within a single section is
	 completely unnecessary.  This can be generated by ld -r.  */
      if (sym == insec->symbol
	  && rel->howto != NULL
	  && rel->howto->pc_relative
	  && ! rel->howto->pcrel_offset)
	{
	  --*reloc_count_ptr;
	  --relocs;
	  memmove (relocs, relocs + 1,
		   (size_t) ((reloc_count - i) * sizeof (arelent *)));
	  continue;
	}

      /* Get the amount the relocation will add in.  */
      addend = rel->addend + sym->value;

      /* NetWare doesn't support PC relative relocs against defined
	 symbols, so we have to eliminate them by doing the relocation
	 now.  We can only do this if the reloc is within a single
	 section.  */
      if (rel->howto != NULL
	  && rel->howto->pc_relative
	  && bfd_get_section (sym) == insec->output_section)
	{
	  bfd_vma val;

	  if (rel->howto->pcrel_offset)
	    addend -= address;

	  val = bfd_get_32 (outbfd, (bfd_byte *) contents + address);
	  val += addend;
	  bfd_put_32 (outbfd, val, (bfd_byte *) contents + address);

	  --*reloc_count_ptr;
	  --relocs;
	  memmove (relocs, relocs + 1,
		   (size_t) ((reloc_count - i) * sizeof (arelent *)));
	  continue;
	}

      /* NetWare doesn't support reloc addends, so we get rid of them
	 here by simply adding them into the object data.  We handle
	 the symbol value, if any, the same way.  */
      if (addend != 0
	  && rel->howto != NULL
	  && rel->howto->rightshift == 0
	  && rel->howto->size == 2
	  && rel->howto->bitsize == 32
	  && rel->howto->bitpos == 0
	  && rel->howto->src_mask == 0xffffffff
	  && rel->howto->dst_mask == 0xffffffff)
	{
	  bfd_vma val;

	  val = bfd_get_32 (outbfd, (bfd_byte *) contents + address);
	  val += addend;
	  bfd_put_32 (outbfd, val, (bfd_byte *) contents + address);

	  /* Adjust the reloc for the changes we just made.  */
	  rel->addend = 0;
	  if (! bfd_is_und_section (bfd_get_section (sym)))
	    rel->sym_ptr_ptr = bfd_get_section (sym)->symbol_ptr_ptr;
	}

      /* NetWare uses a reloc with pcrel_offset set.  We adjust
	 pc_relative relocs accordingly.  We are going to change the
	 howto field, so we can only do this if the current one is
	 compatible.  We should check that special_function is NULL
	 here, but at the moment coff-i386 uses a special_function
	 which does not affect what we are doing here.  */
      if (rel->howto != NULL
	  && rel->howto->pc_relative
	  && ! rel->howto->pcrel_offset
	  && rel->howto->rightshift == 0
	  && rel->howto->size == 2
	  && rel->howto->bitsize == 32
	  && rel->howto->bitpos == 0
	  && rel->howto->src_mask == 0xffffffff
	  && rel->howto->dst_mask == 0xffffffff)
	{
	  bfd_vma val;

	  /* When pcrel_offset is not set, it means that the negative
	     of the address of the memory location is stored in the
	     memory location.  We must add it back in.  */
	  val = bfd_get_32 (outbfd, (bfd_byte *) contents + address);
	  val += address;
	  bfd_put_32 (outbfd, val, (bfd_byte *) contents + address);

	  /* We must change to a new howto.  */
	  rel->howto = &nlm_i386_pcrel_howto;
	}
    }
}

#endif /* NLMCONV_I386 */

#ifdef NLMCONV_ALPHA

/* On the Alpha the first reloc for every section must be a special
   relocs which hold the GP address.  Also, the first reloc in the
   file must be a special reloc which holds the address of the .lita
   section.  */

static reloc_howto_type nlm32_alpha_nw_howto =
  HOWTO (ALPHA_R_NW_RELOC,	/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 0,			/* special_function */
	 "NW_RELOC",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE);		/* pcrel_offset */

static void
alpha_mangle_relocs (bfd *outbfd, asection *insec,
		     arelent ***relocs_ptr, long *reloc_count_ptr,
		     char *contents ATTRIBUTE_UNUSED,
		     bfd_size_type contents_size ATTRIBUTE_UNUSED)
{
  long old_reloc_count;
  arelent **old_relocs;
  arelent **relocs;

  old_reloc_count = *reloc_count_ptr;
  old_relocs = *relocs_ptr;
  relocs = (arelent **) xmalloc ((old_reloc_count + 3) * sizeof (arelent *));
  *relocs_ptr = relocs;

  if (nlm_alpha_backend_data (outbfd)->lita_address == 0)
    {
      bfd *inbfd;
      asection *lita_section;

      inbfd = insec->owner;
      lita_section = bfd_get_section_by_name (inbfd, _LITA);
      if (lita_section != (asection *) NULL)
	{
	  nlm_alpha_backend_data (outbfd)->lita_address =
	    bfd_get_section_vma (inbfd, lita_section);
	  nlm_alpha_backend_data (outbfd)->lita_size =
	    bfd_section_size (inbfd, lita_section);
	}
      else
	{
	  /* Avoid outputting this reloc again.  */
	  nlm_alpha_backend_data (outbfd)->lita_address = 4;
	}

      *relocs = (arelent *) xmalloc (sizeof (arelent));
      (*relocs)->sym_ptr_ptr = bfd_abs_section_ptr->symbol_ptr_ptr;
      (*relocs)->address = nlm_alpha_backend_data (outbfd)->lita_address;
      (*relocs)->addend = nlm_alpha_backend_data (outbfd)->lita_size + 1;
      (*relocs)->howto = &nlm32_alpha_nw_howto;
      ++relocs;
      ++(*reloc_count_ptr);
    }

  /* Get the GP value from bfd.  */
  if (nlm_alpha_backend_data (outbfd)->gp == 0)
    nlm_alpha_backend_data (outbfd)->gp =
      bfd_ecoff_get_gp_value (insec->owner);

  *relocs = (arelent *) xmalloc (sizeof (arelent));
  (*relocs)->sym_ptr_ptr = bfd_abs_section_ptr->symbol_ptr_ptr;
  (*relocs)->address = nlm_alpha_backend_data (outbfd)->gp;
  (*relocs)->addend = 0;
  (*relocs)->howto = &nlm32_alpha_nw_howto;
  ++relocs;
  ++(*reloc_count_ptr);

  memcpy (relocs, old_relocs, (size_t) old_reloc_count * sizeof (arelent *));
  relocs[old_reloc_count] = (arelent *) NULL;

  free (old_relocs);

  if (insec->output_offset != 0)
    {
      bfd_size_type i;

      for (i = 0; i < (bfd_size_type) old_reloc_count; i++, relocs++)
	(*relocs)->address += insec->output_offset;
    }
}

#endif /* NLMCONV_ALPHA */

#ifdef NLMCONV_POWERPC

/* We keep a linked list of stubs which we must build.  Because BFD
   requires us to know the sizes of all sections before we can set the
   contents of any, we must figure out which stubs we want to build
   before we can actually build any of them.  */

struct powerpc_stub
{
  /* Next stub in linked list.  */
  struct powerpc_stub *next;

  /* Symbol whose value is the start of the stub.  This is a symbol
     whose name begins with `.'.  */
  asymbol *start;

  /* Symbol we are going to create a reloc against.  This is a symbol
     with the same name as START but without the leading `.'.  */
  asymbol *reloc;

  /* The TOC index for this stub.  This is the index into the TOC
     section at which the reloc is created.  */
  unsigned int toc_index;
};

/* The linked list of stubs.  */

static struct powerpc_stub *powerpc_stubs;

/* This is what a stub looks like.  The first instruction will get
   adjusted with the correct TOC index.  */

static unsigned long powerpc_stub_insns[] =
{
  0x81820000,		/* lwz	 r12,0(r2) */
  0x90410014,		/* stw	 r2,20(r1) */
  0x800c0000,		/* lwz	 r0,0(r12) */
  0x804c0004,		/* lwz	 r2,r(r12) */
  0x7c0903a6,		/* mtctr r0 */
  0x4e800420,		/* bctr */
  0,			/* Traceback table.  */
  0xc8000,
  0
};

#define POWERPC_STUB_INSN_COUNT \
  (sizeof powerpc_stub_insns / sizeof powerpc_stub_insns[0])

#define POWERPC_STUB_SIZE (4 * POWERPC_STUB_INSN_COUNT)

/* Each stub uses a four byte TOC entry.  */
#define POWERPC_STUB_TOC_ENTRY_SIZE (4)

/* The original size of the .got section.  */
static bfd_size_type powerpc_initial_got_size;

/* Look for all undefined symbols beginning with `.', and prepare to
   build a stub for each one.  */

static void
powerpc_build_stubs (bfd *inbfd, bfd *outbfd ATTRIBUTE_UNUSED,
		     asymbol ***symbols_ptr, long *symcount_ptr)
{
  asection *stub_sec;
  asection *got_sec;
  unsigned int got_base;
  long i;
  long symcount;
  long stubcount;

  /* Make a section to hold stubs.  We don't set SEC_HAS_CONTENTS for
     the section to prevent copy_sections from reading from it.  */
  stub_sec = bfd_make_section (inbfd, ".stubs");
  if (stub_sec == (asection *) NULL
      || ! bfd_set_section_flags (inbfd, stub_sec,
				  (SEC_CODE
				   | SEC_RELOC
				   | SEC_ALLOC
				   | SEC_LOAD))
      || ! bfd_set_section_alignment (inbfd, stub_sec, 2))
    bfd_fatal (".stubs");

  /* Get the TOC section, which is named .got.  */
  got_sec = bfd_get_section_by_name (inbfd, ".got");
  if (got_sec == (asection *) NULL)
    {
      got_sec = bfd_make_section (inbfd, ".got");
      if (got_sec == (asection *) NULL
	  || ! bfd_set_section_flags (inbfd, got_sec,
				      (SEC_DATA
				       | SEC_RELOC
				       | SEC_ALLOC
				       | SEC_LOAD
				       | SEC_HAS_CONTENTS))
	  || ! bfd_set_section_alignment (inbfd, got_sec, 2))
	bfd_fatal (".got");
    }

  powerpc_initial_got_size = bfd_section_size (inbfd, got_sec);
  got_base = powerpc_initial_got_size;
  got_base = (got_base + 3) &~ 3;

  stubcount = 0;

  symcount = *symcount_ptr;
  for (i = 0; i < symcount; i++)
    {
      asymbol *sym;
      asymbol *newsym;
      char *newname;
      struct powerpc_stub *item;

      sym = (*symbols_ptr)[i];

      /* We must make a stub for every undefined symbol whose name
	 starts with '.'.  */
      if (bfd_asymbol_name (sym)[0] != '.'
	  || ! bfd_is_und_section (bfd_get_section (sym)))
	continue;

      /* Make a new undefined symbol with the same name but without
	 the leading `.'.  */
      newsym = (asymbol *) xmalloc (sizeof (asymbol));
      *newsym = *sym;
      newname = (char *) xmalloc (strlen (bfd_asymbol_name (sym)));
      strcpy (newname, bfd_asymbol_name (sym) + 1);
      newsym->name = newname;

      /* Define the `.' symbol to be in the stub section.  */
      sym->section = stub_sec;
      sym->value = stubcount * POWERPC_STUB_SIZE;
      /* We set the BSF_DYNAMIC flag here so that we can check it when
	 we are mangling relocs.  FIXME: This is a hack.  */
      sym->flags = BSF_LOCAL | BSF_DYNAMIC;

      /* Add this stub to the linked list.  */
      item = (struct powerpc_stub *) xmalloc (sizeof (struct powerpc_stub));
      item->start = sym;
      item->reloc = newsym;
      item->toc_index = got_base + stubcount * POWERPC_STUB_TOC_ENTRY_SIZE;

      item->next = powerpc_stubs;
      powerpc_stubs = item;

      ++stubcount;
    }

  if (stubcount > 0)
    {
      asymbol **s;
      struct powerpc_stub *l;

      /* Add the new symbols we just created to the symbol table.  */
      *symbols_ptr = (asymbol **) xrealloc ((char *) *symbols_ptr,
					    ((symcount + stubcount)
					     * sizeof (asymbol)));
      *symcount_ptr += stubcount;
      s = &(*symbols_ptr)[symcount];
      for (l = powerpc_stubs; l != (struct powerpc_stub *) NULL; l = l->next)
	*s++ = l->reloc;

      /* Set the size of the .stubs section and increase the size of
	 the .got section.  */
      if (! bfd_set_section_size (inbfd, stub_sec,
				  stubcount * POWERPC_STUB_SIZE)
	  || ! bfd_set_section_size (inbfd, got_sec,
				     (got_base
				      + (stubcount
					 * POWERPC_STUB_TOC_ENTRY_SIZE))))
	bfd_fatal (_("stub section sizes"));
    }
}

/* Resolve all the stubs for PowerPC NetWare.  We fill in the contents
   of the output section, and create new relocs in the TOC.  */

static void
powerpc_resolve_stubs (bfd *inbfd, bfd *outbfd)
{
  bfd_byte buf[POWERPC_STUB_SIZE];
  unsigned int i;
  unsigned int stubcount;
  arelent **relocs;
  asection *got_sec;
  arelent **r;
  struct powerpc_stub *l;

  if (powerpc_stubs == (struct powerpc_stub *) NULL)
    return;

  for (i = 0; i < POWERPC_STUB_INSN_COUNT; i++)
    bfd_put_32 (outbfd, (bfd_vma) powerpc_stub_insns[i], buf + i * 4);

  got_sec = bfd_get_section_by_name (inbfd, ".got");
  assert (got_sec != (asection *) NULL);
  assert (got_sec->output_section->orelocation == (arelent **) NULL);

  stubcount = 0;
  for (l = powerpc_stubs; l != (struct powerpc_stub *) NULL; l = l->next)
    ++stubcount;
  relocs = (arelent **) xmalloc (stubcount * sizeof (arelent *));

  r = relocs;
  for (l = powerpc_stubs; l != (struct powerpc_stub *) NULL; l = l->next)
    {
      arelent *reloc;

      /* Adjust the first instruction to use the right TOC index.  */
      bfd_put_32 (outbfd, (bfd_vma) powerpc_stub_insns[0] + l->toc_index, buf);

      /* Write this stub out.  */
      if (! bfd_set_section_contents (outbfd,
				      bfd_get_section (l->start),
				      buf,
				      l->start->value,
				      POWERPC_STUB_SIZE))
	bfd_fatal (_("writing stub"));

      /* Create a new reloc for the TOC entry.  */
      reloc = (arelent *) xmalloc (sizeof (arelent));
      reloc->sym_ptr_ptr = &l->reloc;
      reloc->address = l->toc_index + got_sec->output_offset;
      reloc->addend = 0;
      reloc->howto = bfd_reloc_type_lookup (inbfd, BFD_RELOC_32);

      *r++ = reloc;
    }

  bfd_set_reloc (outbfd, got_sec->output_section, relocs, stubcount);
}

/* Adjust relocation entries for PowerPC NetWare.  We do not output
   TOC relocations.  The object code already contains the offset from
   the TOC pointer.  When the function is called, the TOC register,
   r2, will be set to the correct TOC value, so there is no need for
   any further reloc.  */

static void
powerpc_mangle_relocs (bfd *outbfd, asection *insec,
		       arelent ***relocs_ptr,
		       long *reloc_count_ptr, char *contents,
		       bfd_size_type contents_size ATTRIBUTE_UNUSED)
{
  reloc_howto_type *toc_howto;
  long reloc_count;
  arelent **relocs;
  long i;

  toc_howto = bfd_reloc_type_lookup (insec->owner, BFD_RELOC_PPC_TOC16);
  if (toc_howto == (reloc_howto_type *) NULL)
    abort ();

  /* If this is the .got section, clear out all the contents beyond
     the initial size.  We must do this here because copy_sections is
     going to write out whatever we return in the contents field.  */
  if (strcmp (bfd_get_section_name (insec->owner, insec), ".got") == 0)
    memset (contents + powerpc_initial_got_size, 0,
	    (size_t) (bfd_get_section_size (insec) - powerpc_initial_got_size));

  reloc_count = *reloc_count_ptr;
  relocs = *relocs_ptr;
  for (i = 0; i < reloc_count; i++)
    {
      arelent *rel;
      asymbol *sym;
      bfd_vma sym_value;

      rel = *relocs++;
      sym = *rel->sym_ptr_ptr;

      /* Convert any relocs against the .bss section into relocs
         against the .data section.  */
      if (strcmp (bfd_get_section_name (outbfd, bfd_get_section (sym)),
		  NLM_UNINITIALIZED_DATA_NAME) == 0)
	{
	  asection *datasec;

	  datasec = bfd_get_section_by_name (outbfd,
					     NLM_INITIALIZED_DATA_NAME);
	  if (datasec != NULL)
	    {
	      rel->addend += (bfd_get_section_vma (outbfd,
						   bfd_get_section (sym))
			      + sym->value);
	      rel->sym_ptr_ptr = datasec->symbol_ptr_ptr;
	      sym = *rel->sym_ptr_ptr;
	    }
	}

      /* We must be able to resolve all PC relative relocs at this
	 point.  If we get a branch to an undefined symbol we build a
	 stub, since NetWare will resolve undefined symbols into a
	 pointer to a function descriptor.  */
      if (rel->howto->pc_relative)
	{
	  /* This check for whether a symbol is in the same section as
	     the reloc will be wrong if there is a PC relative reloc
	     between two sections both of which were placed in the
	     same output section.  This should not happen.  */
	  if (bfd_get_section (sym) != insec->output_section)
	    non_fatal (_("unresolved PC relative reloc against %s"),
		       bfd_asymbol_name (sym));
	  else
	    {
	      bfd_vma val;

	      assert (rel->howto->size == 2 && rel->howto->pcrel_offset);
	      val = bfd_get_32 (outbfd, (bfd_byte *) contents + rel->address);
	      val = ((val &~ rel->howto->dst_mask)
		     | (((val & rel->howto->src_mask)
			 + (sym->value - rel->address)
			 + rel->addend)
			& rel->howto->dst_mask));
	      bfd_put_32 (outbfd, val, (bfd_byte *) contents + rel->address);

	      /* If this reloc is against an stubbed symbol and the
		 next instruction is
		     cror 31,31,31
		 then we replace the next instruction with
		     lwz  r2,20(r1)
		 This reloads the TOC pointer after a stub call.  */
	      if (bfd_asymbol_name (sym)[0] == '.'
		  && (sym->flags & BSF_DYNAMIC) != 0
		  && (bfd_get_32 (outbfd,
				  (bfd_byte *) contents + rel->address + 4)
		      == 0x4ffffb82)) /* cror 31,31,31 */
		bfd_put_32 (outbfd, (bfd_vma) 0x80410014, /* lwz r2,20(r1) */
			    (bfd_byte *) contents + rel->address + 4);

	      --*reloc_count_ptr;
	      --relocs;
	      memmove (relocs, relocs + 1,
		       (size_t) ((reloc_count - 1) * sizeof (arelent *)));
	      continue;
	    }
	}

      /* When considering a TOC reloc, we do not want to include the
	 symbol value.  The symbol will be start of the TOC section
	 (which is named .got).  We do want to include the addend.  */
      if (rel->howto == toc_howto)
	sym_value = 0;
      else
	sym_value = sym->value;

      /* If this is a relocation against a symbol with a value, or
	 there is a reloc addend, we need to update the addend in the
	 object file.  */
      if (sym_value + rel->addend != 0)
	{
	  bfd_vma val;

	  switch (rel->howto->size)
	    {
	    case 1:
	      val = bfd_get_16 (outbfd,
				(bfd_byte *) contents + rel->address);
	      val = ((val &~ rel->howto->dst_mask)
		     | (((val & rel->howto->src_mask)
			 + sym_value
			 + rel->addend)
			& rel->howto->dst_mask));
	      if ((bfd_signed_vma) val < - 0x8000
		  || (bfd_signed_vma) val >= 0x8000)
		non_fatal (_("overflow when adjusting relocation against %s"),
			   bfd_asymbol_name (sym));
	      bfd_put_16 (outbfd, val, (bfd_byte *) contents + rel->address);
	      break;

	    case 2:
	      val = bfd_get_32 (outbfd,
				(bfd_byte *) contents + rel->address);
	      val = ((val &~ rel->howto->dst_mask)
		     | (((val & rel->howto->src_mask)
			 + sym_value
			 + rel->addend)
			& rel->howto->dst_mask));
	      bfd_put_32 (outbfd, val, (bfd_byte *) contents + rel->address);
	      break;

	    default:
	      abort ();
	    }

	  if (! bfd_is_und_section (bfd_get_section (sym)))
	    rel->sym_ptr_ptr = bfd_get_section (sym)->symbol_ptr_ptr;
	  rel->addend = 0;
	}

      /* Now that we have incorporated the addend, remove any TOC
	 relocs.  */
      if (rel->howto == toc_howto)
	{
	  --*reloc_count_ptr;
	  --relocs;
	  memmove (relocs, relocs + 1,
		   (size_t) ((reloc_count - i) * sizeof (arelent *)));
	  continue;
	}

      rel->address += insec->output_offset;
    }
}

#endif /* NLMCONV_POWERPC */

/* Name of linker.  */
#ifndef LD_NAME
#define LD_NAME "ld"
#endif

/* The user has specified several input files.  Invoke the linker to
   link them all together, and convert and delete the resulting output
   file.  */

static char *
link_inputs (struct string_list *inputs, char *ld, char * map_file)
{
  size_t c;
  struct string_list *q;
  char **argv;
  size_t i;
  int pid;
  int status;
  char *errfmt;
  char *errarg;

  c = 0;
  for (q = inputs; q != NULL; q = q->next)
    ++c;

  argv = (char **) alloca ((c + 7) * sizeof (char *));

#ifndef __MSDOS__
  if (ld == NULL)
    {
      char *p;

      /* Find the linker to invoke based on how nlmconv was run.  */
      p = program_name + strlen (program_name);
      while (p != program_name)
	{
	  if (p[-1] == '/')
	    {
	      ld = (char *) xmalloc (p - program_name + strlen (LD_NAME) + 1);
	      memcpy (ld, program_name, p - program_name);
	      strcpy (ld + (p - program_name), LD_NAME);
	      break;
	    }
	  --p;
	}
    }
#endif

  if (ld == NULL)
    ld = (char *) LD_NAME;

  unlink_on_exit = make_temp_file (".O");

  argv[0] = ld;
  argv[1] = (char *) "-Ur";
  argv[2] = (char *) "-o";
  argv[3] = unlink_on_exit;
  /* If we have been given the name of a mapfile and that
     name is not 'stderr' then pass it on to the linker.  */
  if (map_file
      && * map_file
      && strcmp (map_file, "stderr") == 0)
    {
      argv[4] = (char *) "-Map";
      argv[5] = map_file;
      i = 6;
    }
  else
    i = 4;

  for (q = inputs; q != NULL; q = q->next, i++)
    argv[i] = q->string;
  argv[i] = NULL;

  if (debug)
    {
      for (i = 0; argv[i] != NULL; i++)
	fprintf (stderr, " %s", argv[i]);
      fprintf (stderr, "\n");
    }

  pid = pexecute (ld, argv, program_name, (char *) NULL, &errfmt, &errarg,
		  PEXECUTE_SEARCH | PEXECUTE_ONE);
  if (pid == -1)
    {
      fprintf (stderr, _("%s: execution of %s failed: "), program_name, ld);
      fprintf (stderr, errfmt, errarg);
      unlink (unlink_on_exit);
      exit (1);
    }

  if (pwait (pid, &status, 0) < 0)
    {
      perror ("pwait");
      unlink (unlink_on_exit);
      exit (1);
    }

  if (status != 0)
    {
      non_fatal (_("Execution of %s failed"), ld);
      unlink (unlink_on_exit);
      exit (1);
    }

  return unlink_on_exit;
}
