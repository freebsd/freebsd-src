/* Main program of GNU linker.
   Copyright 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001,
   2002
   Free Software Foundation, Inc.
   Written by Steve Chamberlain steve@cygnus.com

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

#include "bfd.h"
#include "sysdep.h"
#include <stdio.h>
#include "safe-ctype.h"
#include "libiberty.h"
#include "progress.h"
#include "bfdlink.h"
#include "filenames.h"

#include "ld.h"
#include "ldmain.h"
#include "ldmisc.h"
#include "ldwrite.h"
#include "ldexp.h"
#include "ldlang.h"
#include <ldgram.h>
#include "ldlex.h"
#include "ldfile.h"
#include "ldemul.h"
#include "ldctor.h"

/* Somewhere above, sys/stat.h got included . . . .  */
#if !defined(S_ISDIR) && defined(S_IFDIR)
#define	S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif

#include <string.h>

#ifdef HAVE_SBRK
#ifdef NEED_DECLARATION_SBRK
extern PTR sbrk ();
#endif
#endif

int main PARAMS ((int, char **));

static char *get_emulation PARAMS ((int, char **));
static void set_scripts_dir PARAMS ((void));

/* EXPORTS */

char *default_target;
const char *output_filename = "a.out";

/* Name this program was invoked by.  */
char *program_name;

/* The file that we're creating.  */
bfd *output_bfd = 0;

/* Set by -G argument, for MIPS ECOFF target.  */
int g_switch_value = 8;

/* Nonzero means print names of input files as processed.  */
boolean trace_files;

/* Nonzero means same, but note open failures, too.  */
boolean trace_file_tries;

/* Nonzero means version number was printed, so exit successfully
   instead of complaining if no input files are given.  */
boolean version_printed;

/* Nonzero means link in every member of an archive.  */
boolean whole_archive;

/* True if we should demangle symbol names.  */
boolean demangling;

args_type command_line;

ld_config_type config;

static void remove_output PARAMS ((void));
static boolean check_for_scripts_dir PARAMS ((char *dir));
static boolean add_archive_element PARAMS ((struct bfd_link_info *, bfd *,
					    const char *));
static boolean multiple_definition PARAMS ((struct bfd_link_info *,
					    const char *,
					    bfd *, asection *, bfd_vma,
					    bfd *, asection *, bfd_vma));
static boolean multiple_common PARAMS ((struct bfd_link_info *,
					const char *, bfd *,
					enum bfd_link_hash_type, bfd_vma,
					bfd *, enum bfd_link_hash_type,
					bfd_vma));
static boolean add_to_set PARAMS ((struct bfd_link_info *,
				   struct bfd_link_hash_entry *,
				   bfd_reloc_code_real_type,
				   bfd *, asection *, bfd_vma));
static boolean constructor_callback PARAMS ((struct bfd_link_info *,
					     boolean constructor,
					     const char *name,
					     bfd *, asection *, bfd_vma));
static boolean warning_callback PARAMS ((struct bfd_link_info *,
					 const char *, const char *, bfd *,
					 asection *, bfd_vma));
static void warning_find_reloc PARAMS ((bfd *, asection *, PTR));
static boolean undefined_symbol PARAMS ((struct bfd_link_info *,
					 const char *, bfd *,
					 asection *, bfd_vma, boolean));
static boolean reloc_overflow PARAMS ((struct bfd_link_info *, const char *,
				       const char *, bfd_vma,
				       bfd *, asection *, bfd_vma));
static boolean reloc_dangerous PARAMS ((struct bfd_link_info *, const char *,
					bfd *, asection *, bfd_vma));
static boolean unattached_reloc PARAMS ((struct bfd_link_info *,
					 const char *, bfd *, asection *,
					 bfd_vma));
static boolean notice PARAMS ((struct bfd_link_info *, const char *,
			       bfd *, asection *, bfd_vma));

static struct bfd_link_callbacks link_callbacks = {
  add_archive_element,
  multiple_definition,
  multiple_common,
  add_to_set,
  constructor_callback,
  warning_callback,
  undefined_symbol,
  reloc_overflow,
  reloc_dangerous,
  unattached_reloc,
  notice
};

struct bfd_link_info link_info;

static void
remove_output ()
{
  if (output_filename)
    {
      if (output_bfd && output_bfd->iostream)
	fclose ((FILE *) (output_bfd->iostream));
      if (delete_output_file_on_failure)
	unlink (output_filename);
    }
}

int
main (argc, argv)
     int argc;
     char **argv;
{
  char *emulation;
  long start_time = get_run_time ();

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

  START_PROGRESS (program_name, 0);

  bfd_init ();

  bfd_set_error_program_name (program_name);

  xatexit (remove_output);

  /* Set the default BFD target based on the configured target.  Doing
     this permits the linker to be configured for a particular target,
     and linked against a shared BFD library which was configured for
     a different target.  The macro TARGET is defined by Makefile.  */
  if (! bfd_set_default_target (TARGET))
    {
      einfo (_("%X%P: can't set BFD default target to `%s': %E\n"), TARGET);
      xexit (1);
    }

#if YYDEBUG
  {
    extern int yydebug;
    yydebug = 1;
  }
#endif

  /* Initialize the data about options.  */
  trace_files = trace_file_tries = version_printed = false;
  whole_archive = false;
  config.build_constructors = true;
  config.dynamic_link = false;
  config.has_shared = false;
  config.split_by_reloc = (unsigned) -1;
  config.split_by_file = (bfd_size_type) -1;
  command_line.force_common_definition = false;
  command_line.inhibit_common_definition = false;
  command_line.interpreter = NULL;
  command_line.rpath = NULL;
  command_line.warn_mismatch = true;
  command_line.check_section_addresses = true;

  /* We initialize DEMANGLING based on the environment variable
     COLLECT_NO_DEMANGLE.  The gcc collect2 program will demangle the
     output of the linker, unless COLLECT_NO_DEMANGLE is set in the
     environment.  Acting the same way here lets us provide the same
     interface by default.  */
  demangling = getenv ("COLLECT_NO_DEMANGLE") == NULL;

  link_info.callbacks = &link_callbacks;
  link_info.relocateable = false;
  link_info.emitrelocations = false;
  link_info.shared = false;
  link_info.symbolic = false;
  link_info.export_dynamic = false;
  link_info.static_link = false;
  link_info.traditional_format = false;
  link_info.optimize = false;
  link_info.no_undefined = false;
  link_info.allow_shlib_undefined = false;
  link_info.allow_multiple_definition = false;
  link_info.allow_undefined_version = true;
  link_info.strip = strip_none;
  link_info.discard = discard_sec_merge;
  link_info.keep_memory = true;
  link_info.input_bfds = NULL;
  link_info.create_object_symbols_section = NULL;
  link_info.gc_sym_list = NULL;
  link_info.hash = NULL;
  link_info.keep_hash = NULL;
  link_info.notice_all = false;
  link_info.notice_hash = NULL;
  link_info.wrap_hash = NULL;
  link_info.mpc860c0 = 0;
  /* SVR4 linkers seem to set DT_INIT and DT_FINI based on magic _init
     and _fini symbols.  We are compatible.  */
  link_info.init_function = "_init";
  link_info.fini_function = "_fini";
  link_info.new_dtags = false;
  link_info.eh_frame_hdr = false;
  link_info.flags = (bfd_vma) 0;
  link_info.flags_1 = (bfd_vma) 0;
  link_info.pei386_auto_import = false;
  link_info.pei386_auto_import = -1;
  link_info.combreloc = true;
  link_info.spare_dynamic_tags = 5;

  ldfile_add_arch ("");

  config.make_executable = true;
  force_make_executable = false;
  config.magic_demand_paged = true;
  config.text_read_only = true;

  emulation = get_emulation (argc, argv);
  ldemul_choose_mode (emulation);
  default_target = ldemul_choose_target (argc, argv);
  lang_init ();
  ldemul_before_parse ();
  lang_has_input_file = false;
  parse_args (argc, argv);

  ldemul_set_symbols ();

  if (link_info.relocateable)
    {
      if (command_line.gc_sections)
	einfo ("%P%F: --gc-sections and -r may not be used together\n");
      if (link_info.mpc860c0)
	einfo (_("%P%F: -r and --mpc860c0 may not be used together\n"));
      else if (command_line.relax)
	einfo (_("%P%F: --relax and -r may not be used together\n"));
      if (link_info.shared)
	einfo (_("%P%F: -r and -shared may not be used together\n"));
    }

  if (! link_info.shared)
    {
      if (command_line.filter_shlib)
	einfo (_("%P%F: -F may not be used without -shared\n"));
      if (command_line.auxiliary_filters)
	einfo (_("%P%F: -f may not be used without -shared\n"));
    }

  /* Treat ld -r -s as ld -r -S -x (i.e., strip all local symbols).  I
     don't see how else this can be handled, since in this case we
     must preserve all externally visible symbols.  */
  if (link_info.relocateable && link_info.strip == strip_all)
    {
      link_info.strip = strip_debugger;
      if (link_info.discard == discard_sec_merge)
	link_info.discard = discard_all;
    }

  /* This essentially adds another -L directory so this must be done after
     the -L's in argv have been processed.  */
  set_scripts_dir ();

  /* If we have not already opened and parsed a linker script
     read the emulation's appropriate default script.  */
  if (saved_script_handle == NULL)
    {
      int isfile;
      char *s = ldemul_get_script (&isfile);

      if (isfile)
	ldfile_open_command_file (s);
      else
	{
	  lex_string = s;
	  lex_redirect (s);
	}
      parser_input = input_script;
      yyparse ();
      lex_string = NULL;
    }

  if (trace_file_tries)
    {
      if (saved_script_handle)
	info_msg (_("using external linker script:"));
      else
	info_msg (_("using internal linker script:"));
      info_msg ("\n==================================================\n");

      if (saved_script_handle)
	{
	  static const int ld_bufsz = 8193;
	  size_t n;
	  char *buf = xmalloc (ld_bufsz);

	  rewind (saved_script_handle);
	  while ((n = fread (buf, 1, ld_bufsz - 1, saved_script_handle)) > 0)
	    {
	      buf[n] = 0;
	      info_msg (buf);
	    }
	  rewind (saved_script_handle);
	  free (buf);
	}
      else
	{
	  int isfile;

	  info_msg (ldemul_get_script (&isfile));
	}

      info_msg ("\n==================================================\n");
    }

  lang_final ();

  if (lang_has_input_file == false)
    {
      if (version_printed)
	xexit (0);
      einfo (_("%P%F: no input files\n"));
    }

  if (trace_files)
    {
      info_msg (_("%P: mode %s\n"), emulation);
    }

  ldemul_after_parse ();

  if (config.map_filename)
    {
      if (strcmp (config.map_filename, "-") == 0)
	{
	  config.map_file = stdout;
	}
      else
	{
	  config.map_file = fopen (config.map_filename, FOPEN_WT);
	  if (config.map_file == (FILE *) NULL)
	    {
	      bfd_set_error (bfd_error_system_call);
	      einfo (_("%P%F: cannot open map file %s: %E\n"),
		     config.map_filename);
	    }
	}
    }

  lang_process ();

  /* Print error messages for any missing symbols, for any warning
     symbols, and possibly multiple definitions.  */

  if (link_info.relocateable)
    output_bfd->flags &= ~EXEC_P;
  else
    output_bfd->flags |= EXEC_P;

  ldwrite ();

  if (config.map_file != NULL)
    lang_map ();
  if (command_line.cref)
    output_cref (config.map_file != NULL ? config.map_file : stdout);
  if (nocrossref_list != NULL)
    check_nocrossrefs ();

  /* Even if we're producing relocateable output, some non-fatal errors should
     be reported in the exit status.  (What non-fatal errors, if any, do we
     want to ignore for relocateable output?)  */

  if (config.make_executable == false && force_make_executable == false)
    {
      if (trace_files == true)
	{
	  einfo (_("%P: link errors found, deleting executable `%s'\n"),
		 output_filename);
	}

      /* The file will be removed by remove_output.  */

      xexit (1);
    }
  else
    {
      if (! bfd_close (output_bfd))
	einfo (_("%F%B: final close failed: %E\n"), output_bfd);

      /* If the --force-exe-suffix is enabled, and we're making an
	 executable file and it doesn't end in .exe, copy it to one
	 which does.  */
      if (! link_info.relocateable && command_line.force_exe_suffix)
	{
	  int len = strlen (output_filename);
	  if (len < 4
	      || (strcasecmp (output_filename + len - 4, ".exe") != 0
		  && strcasecmp (output_filename + len - 4, ".dll") != 0))
	    {
	      FILE *src;
	      FILE *dst;
	      const int bsize = 4096;
	      char *buf = xmalloc (bsize);
	      int l;
	      char *dst_name = xmalloc (len + 5);
	      strcpy (dst_name, output_filename);
	      strcat (dst_name, ".exe");
	      src = fopen (output_filename, FOPEN_RB);
	      dst = fopen (dst_name, FOPEN_WB);

	      if (!src)
		einfo (_("%X%P: unable to open for source of copy `%s'\n"), output_filename);
	      if (!dst)
		einfo (_("%X%P: unable to open for destination of copy `%s'\n"), dst_name);
	      while ((l = fread (buf, 1, bsize, src)) > 0)
		{
		  int done = fwrite (buf, 1, l, dst);
		  if (done != l)
		    {
		      einfo (_("%P: Error writing file `%s'\n"), dst_name);
		    }
		}
	      fclose (src);
	      if (fclose (dst) == EOF)
		{
		  einfo (_("%P: Error closing file `%s'\n"), dst_name);
		}
	      free (dst_name);
	      free (buf);
	    }
	}
    }

  END_PROGRESS (program_name);

  if (config.stats)
    {
#ifdef HAVE_SBRK
      char *lim = (char *) sbrk (0);
#endif
      long run_time = get_run_time () - start_time;

      fprintf (stderr, _("%s: total time in link: %ld.%06ld\n"),
	       program_name, run_time / 1000000, run_time % 1000000);
#ifdef HAVE_SBRK
      fprintf (stderr, _("%s: data size %ld\n"), program_name,
	       (long) (lim - (char *) &environ));
#endif
    }

  /* Prevent remove_output from doing anything, after a successful link.  */
  output_filename = NULL;

  xexit (0);
  return 0;
}

/* We need to find any explicitly given emulation in order to initialize the
   state that's needed by the lex&yacc argument parser (parse_args).  */

static char *
get_emulation (argc, argv)
     int argc;
     char **argv;
{
  char *emulation;
  int i;

  emulation = getenv (EMULATION_ENVIRON);
  if (emulation == NULL)
    emulation = DEFAULT_EMULATION;

  for (i = 1; i < argc; i++)
    {
      if (!strncmp (argv[i], "-m", 2))
	{
	  if (argv[i][2] == '\0')
	    {
	      /* -m EMUL */
	      if (i < argc - 1)
		{
		  emulation = argv[i + 1];
		  i++;
		}
	      else
		{
		  einfo (_("%P%F: missing argument to -m\n"));
		}
	    }
	  else if (strcmp (argv[i], "-mips1") == 0
		   || strcmp (argv[i], "-mips2") == 0
		   || strcmp (argv[i], "-mips3") == 0
		   || strcmp (argv[i], "-mips32") == 0
		   || strcmp (argv[i], "-mips64") == 0
		   || strcmp (argv[i], "-mips4") == 0
		   || strcmp (argv[i], "-mips5") == 0)
	    {
	      /* FIXME: The arguments -mips1, -mips2, -mips3, etc. are
		 passed to the linker by some MIPS compilers.  They
		 generally tell the linker to use a slightly different
		 library path.  Perhaps someday these should be
		 implemented as emulations; until then, we just ignore
		 the arguments and hope that nobody ever creates
		 emulations named ips1, ips2 or ips3.  */
	    }
	  else if (strcmp (argv[i], "-m486") == 0)
	    {
	      /* FIXME: The argument -m486 is passed to the linker on
		 some Linux systems.  Hope that nobody creates an
		 emulation named 486.  */
	    }
	  else
	    {
	      /* -mEMUL */
	      emulation = &argv[i][2];
	    }
	}
    }

  return emulation;
}

/* If directory DIR contains an "ldscripts" subdirectory,
   add DIR to the library search path and return true,
   else return false.  */

static boolean
check_for_scripts_dir (dir)
     char *dir;
{
  size_t dirlen;
  char *buf;
  struct stat s;
  boolean res;

  dirlen = strlen (dir);
  /* sizeof counts the terminating NUL.  */
  buf = (char *) xmalloc (dirlen + sizeof ("/ldscripts"));
  sprintf (buf, "%s/ldscripts", dir);

  res = stat (buf, &s) == 0 && S_ISDIR (s.st_mode);
  free (buf);
  if (res)
    ldfile_add_library_path (dir, false);
  return res;
}

/* Set the default directory for finding script files.
   Libraries will be searched for here too, but that's ok.
   We look for the "ldscripts" directory in:

   SCRIPTDIR (passed from Makefile)
   the dir where this program is (for using it from the build tree)
   the dir where this program is/../lib (for installing the tool suite elsewhere) */

static void
set_scripts_dir ()
{
  char *end, *dir;
  size_t dirlen;

  if (check_for_scripts_dir (SCRIPTDIR))
    /* We've been installed normally.  */
    return;

  /* Look for "ldscripts" in the dir where our binary is.  */
  end = strrchr (program_name, '/');
#ifdef HAVE_DOS_BASED_FILE_SYSTEM
  {
    /* We could have \foo\bar, or /foo\bar.  */
    char *bslash = strrchr (program_name, '\\');
    if (end == NULL || (bslash != NULL && bslash > end))
      end = bslash;
  }
#endif

  if (end == NULL)
    {
      /* Don't look for ldscripts in the current directory.  There is
         too much potential for confusion.  */
      return;
    }

  dirlen = end - program_name;
  /* Make a copy of program_name in dir.
     Leave room for later "/../lib".  */
  dir = (char *) xmalloc (dirlen + 8);
  strncpy (dir, program_name, dirlen);
  dir[dirlen] = '\0';

  if (check_for_scripts_dir (dir))
    /* Don't free dir.  */
    return;

  /* Look for "ldscripts" in <the dir where our binary is>/../lib.  */
  strcpy (dir + dirlen, "/../lib");
  if (check_for_scripts_dir (dir))
    return;

  /* Well, we tried.  */
  free (dir);
}

void
add_ysym (name)
     const char *name;
{
  if (link_info.notice_hash == (struct bfd_hash_table *) NULL)
    {
      link_info.notice_hash = ((struct bfd_hash_table *)
			       xmalloc (sizeof (struct bfd_hash_table)));
      if (! bfd_hash_table_init_n (link_info.notice_hash,
				   bfd_hash_newfunc,
				   61))
	einfo (_("%P%F: bfd_hash_table_init failed: %E\n"));
    }

  if (bfd_hash_lookup (link_info.notice_hash, name, true, true)
      == (struct bfd_hash_entry *) NULL)
    einfo (_("%P%F: bfd_hash_lookup failed: %E\n"));
}

/* Record a symbol to be wrapped, from the --wrap option.  */

void
add_wrap (name)
     const char *name;
{
  if (link_info.wrap_hash == NULL)
    {
      link_info.wrap_hash = ((struct bfd_hash_table *)
			     xmalloc (sizeof (struct bfd_hash_table)));
      if (! bfd_hash_table_init_n (link_info.wrap_hash,
				   bfd_hash_newfunc,
				   61))
	einfo (_("%P%F: bfd_hash_table_init failed: %E\n"));
    }
  if (bfd_hash_lookup (link_info.wrap_hash, name, true, true) == NULL)
    einfo (_("%P%F: bfd_hash_lookup failed: %E\n"));
}

/* Handle the -retain-symbols-file option.  */

void
add_keepsyms_file (filename)
     const char *filename;
{
  FILE *file;
  char *buf;
  size_t bufsize;
  int c;

  if (link_info.strip == strip_some)
    einfo (_("%X%P: error: duplicate retain-symbols-file\n"));

  file = fopen (filename, "r");
  if (file == (FILE *) NULL)
    {
      bfd_set_error (bfd_error_system_call);
      einfo ("%X%P: %s: %E\n", filename);
      return;
    }

  link_info.keep_hash = ((struct bfd_hash_table *)
			 xmalloc (sizeof (struct bfd_hash_table)));
  if (! bfd_hash_table_init (link_info.keep_hash, bfd_hash_newfunc))
    einfo (_("%P%F: bfd_hash_table_init failed: %E\n"));

  bufsize = 100;
  buf = (char *) xmalloc (bufsize);

  c = getc (file);
  while (c != EOF)
    {
      while (ISSPACE (c))
	c = getc (file);

      if (c != EOF)
	{
	  size_t len = 0;

	  while (! ISSPACE (c) && c != EOF)
	    {
	      buf[len] = c;
	      ++len;
	      if (len >= bufsize)
		{
		  bufsize *= 2;
		  buf = xrealloc (buf, bufsize);
		}
	      c = getc (file);
	    }

	  buf[len] = '\0';

	  if (bfd_hash_lookup (link_info.keep_hash, buf, true, true)
	      == (struct bfd_hash_entry *) NULL)
	    einfo (_("%P%F: bfd_hash_lookup for insertion failed: %E\n"));
	}
    }

  if (link_info.strip != strip_none)
    einfo (_("%P: `-retain-symbols-file' overrides `-s' and `-S'\n"));

  link_info.strip = strip_some;
}

/* Callbacks from the BFD linker routines.  */

/* This is called when BFD has decided to include an archive member in
   a link.  */

static boolean
add_archive_element (info, abfd, name)
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
     bfd *abfd;
     const char *name;
{
  lang_input_statement_type *input;

  input = ((lang_input_statement_type *)
	   xmalloc (sizeof (lang_input_statement_type)));
  input->filename = abfd->filename;
  input->local_sym_name = abfd->filename;
  input->the_bfd = abfd;
  input->asymbols = NULL;
  input->next = NULL;
  input->just_syms_flag = false;
  input->loaded = false;
  input->search_dirs_flag = false;

  /* FIXME: The following fields are not set: header.next,
     header.type, closed, passive_position, symbol_count,
     next_real_file, is_archive, target, real.  This bit of code is
     from the old decode_library_subfile function.  I don't know
     whether any of those fields matters.  */

  ldlang_add_file (input);

  if (config.map_file != (FILE *) NULL)
    {
      static boolean header_printed;
      struct bfd_link_hash_entry *h;
      bfd *from;
      int len;

      h = bfd_link_hash_lookup (link_info.hash, name, false, false, true);

      if (h == NULL)
	from = NULL;
      else
	{
	  switch (h->type)
	    {
	    default:
	      from = NULL;
	      break;

	    case bfd_link_hash_defined:
	    case bfd_link_hash_defweak:
	      from = h->u.def.section->owner;
	      break;

	    case bfd_link_hash_undefined:
	    case bfd_link_hash_undefweak:
	      from = h->u.undef.abfd;
	      break;

	    case bfd_link_hash_common:
	      from = h->u.c.p->section->owner;
	      break;
	    }
	}

      if (! header_printed)
	{
	  char buf[100];

	  sprintf (buf, _("Archive member included because of file (symbol)\n\n"));
	  minfo ("%s", buf);
	  header_printed = true;
	}

      if (bfd_my_archive (abfd) == NULL)
	{
	  minfo ("%s", bfd_get_filename (abfd));
	  len = strlen (bfd_get_filename (abfd));
	}
      else
	{
	  minfo ("%s(%s)", bfd_get_filename (bfd_my_archive (abfd)),
		 bfd_get_filename (abfd));
	  len = (strlen (bfd_get_filename (bfd_my_archive (abfd)))
		 + strlen (bfd_get_filename (abfd))
		 + 2);
	}

      if (len >= 29)
	{
	  print_nl ();
	  len = 0;
	}
      while (len < 30)
	{
	  print_space ();
	  ++len;
	}

      if (from != NULL)
	minfo ("%B ", from);
      if (h != NULL)
	minfo ("(%T)\n", h->root.string);
      else
	minfo ("(%s)\n", name);
    }

  if (trace_files || trace_file_tries)
    info_msg ("%I\n", input);

  return true;
}

/* This is called when BFD has discovered a symbol which is defined
   multiple times.  */

static boolean
multiple_definition (info, name, obfd, osec, oval, nbfd, nsec, nval)
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
     const char *name;
     bfd *obfd;
     asection *osec;
     bfd_vma oval;
     bfd *nbfd;
     asection *nsec;
     bfd_vma nval;
{
  /* If either section has the output_section field set to
     bfd_abs_section_ptr, it means that the section is being
     discarded, and this is not really a multiple definition at all.
     FIXME: It would be cleaner to somehow ignore symbols defined in
     sections which are being discarded.  */
  if ((osec->output_section != NULL
       && ! bfd_is_abs_section (osec)
       && bfd_is_abs_section (osec->output_section))
      || (nsec->output_section != NULL
	  && ! bfd_is_abs_section (nsec)
	  && bfd_is_abs_section (nsec->output_section)))
    return true;

  einfo (_("%X%C: multiple definition of `%T'\n"),
	 nbfd, nsec, nval, name);
  if (obfd != (bfd *) NULL)
    einfo (_("%D: first defined here\n"), obfd, osec, oval);

  if (command_line.relax)
    {
      einfo (_("%P: Disabling relaxation: it will not work with multiple definitions\n"));
      command_line.relax = 0;
    }

  return true;
}

/* This is called when there is a definition of a common symbol, or
   when a common symbol is found for a symbol that is already defined,
   or when two common symbols are found.  We only do something if
   -warn-common was used.  */

static boolean
multiple_common (info, name, obfd, otype, osize, nbfd, ntype, nsize)
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
     const char *name;
     bfd *obfd;
     enum bfd_link_hash_type otype;
     bfd_vma osize;
     bfd *nbfd;
     enum bfd_link_hash_type ntype;
     bfd_vma nsize;
{
  if (! config.warn_common)
    return true;

  if (ntype == bfd_link_hash_defined
      || ntype == bfd_link_hash_defweak
      || ntype == bfd_link_hash_indirect)
    {
      ASSERT (otype == bfd_link_hash_common);
      einfo (_("%B: warning: definition of `%T' overriding common\n"),
	     nbfd, name);
      if (obfd != NULL)
	einfo (_("%B: warning: common is here\n"), obfd);
    }
  else if (otype == bfd_link_hash_defined
	   || otype == bfd_link_hash_defweak
	   || otype == bfd_link_hash_indirect)
    {
      ASSERT (ntype == bfd_link_hash_common);
      einfo (_("%B: warning: common of `%T' overridden by definition\n"),
	     nbfd, name);
      if (obfd != NULL)
	einfo (_("%B: warning: defined here\n"), obfd);
    }
  else
    {
      ASSERT (otype == bfd_link_hash_common && ntype == bfd_link_hash_common);
      if (osize > nsize)
	{
	  einfo (_("%B: warning: common of `%T' overridden by larger common\n"),
		 nbfd, name);
	  if (obfd != NULL)
	    einfo (_("%B: warning: larger common is here\n"), obfd);
	}
      else if (nsize > osize)
	{
	  einfo (_("%B: warning: common of `%T' overriding smaller common\n"),
		 nbfd, name);
	  if (obfd != NULL)
	    einfo (_("%B: warning: smaller common is here\n"), obfd);
	}
      else
	{
	  einfo (_("%B: warning: multiple common of `%T'\n"), nbfd, name);
	  if (obfd != NULL)
	    einfo (_("%B: warning: previous common is here\n"), obfd);
	}
    }

  return true;
}

/* This is called when BFD has discovered a set element.  H is the
   entry in the linker hash table for the set.  SECTION and VALUE
   represent a value which should be added to the set.  */

static boolean
add_to_set (info, h, reloc, abfd, section, value)
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
     struct bfd_link_hash_entry *h;
     bfd_reloc_code_real_type reloc;
     bfd *abfd;
     asection *section;
     bfd_vma value;
{
  if (config.warn_constructors)
    einfo (_("%P: warning: global constructor %s used\n"),
	   h->root.string);

  if (! config.build_constructors)
    return true;

  ldctor_add_set_entry (h, reloc, (const char *) NULL, section, value);

  if (h->type == bfd_link_hash_new)
    {
      h->type = bfd_link_hash_undefined;
      h->u.undef.abfd = abfd;
      /* We don't call bfd_link_add_undef to add this to the list of
	 undefined symbols because we are going to define it
	 ourselves.  */
    }

  return true;
}

/* This is called when BFD has discovered a constructor.  This is only
   called for some object file formats--those which do not handle
   constructors in some more clever fashion.  This is similar to
   adding an element to a set, but less general.  */

static boolean
constructor_callback (info, constructor, name, abfd, section, value)
     struct bfd_link_info *info;
     boolean constructor;
     const char *name;
     bfd *abfd;
     asection *section;
     bfd_vma value;
{
  char *s;
  struct bfd_link_hash_entry *h;
  char set_name[1 + sizeof "__CTOR_LIST__"];

  if (config.warn_constructors)
    einfo (_("%P: warning: global constructor %s used\n"), name);

  if (! config.build_constructors)
    return true;

  /* Ensure that BFD_RELOC_CTOR exists now, so that we can give a
     useful error message.  */
  if (bfd_reloc_type_lookup (output_bfd, BFD_RELOC_CTOR) == NULL
      && (link_info.relocateable
	  || bfd_reloc_type_lookup (abfd, BFD_RELOC_CTOR) == NULL))
    einfo (_("%P%F: BFD backend error: BFD_RELOC_CTOR unsupported\n"));

  s = set_name;
  if (bfd_get_symbol_leading_char (abfd) != '\0')
    *s++ = bfd_get_symbol_leading_char (abfd);
  if (constructor)
    strcpy (s, "__CTOR_LIST__");
  else
    strcpy (s, "__DTOR_LIST__");

  h = bfd_link_hash_lookup (info->hash, set_name, true, true, true);
  if (h == (struct bfd_link_hash_entry *) NULL)
    einfo (_("%P%F: bfd_link_hash_lookup failed: %E\n"));
  if (h->type == bfd_link_hash_new)
    {
      h->type = bfd_link_hash_undefined;
      h->u.undef.abfd = abfd;
      /* We don't call bfd_link_add_undef to add this to the list of
	 undefined symbols because we are going to define it
	 ourselves.  */
    }

  ldctor_add_set_entry (h, BFD_RELOC_CTOR, name, section, value);
  return true;
}

/* A structure used by warning_callback to pass information through
   bfd_map_over_sections.  */

struct warning_callback_info {
  boolean found;
  const char *warning;
  const char *symbol;
  asymbol **asymbols;
};

/* This is called when there is a reference to a warning symbol.  */

static boolean
warning_callback (info, warning, symbol, abfd, section, address)
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
     const char *warning;
     const char *symbol;
     bfd *abfd;
     asection *section;
     bfd_vma address;
{
  /* This is a hack to support warn_multiple_gp.  FIXME: This should
     have a cleaner interface, but what?  */
  if (! config.warn_multiple_gp
      && strcmp (warning, "using multiple gp values") == 0)
    return true;

  if (section != NULL)
    einfo ("%C: %s\n", abfd, section, address, warning);
  else if (abfd == NULL)
    einfo ("%P: %s\n", warning);
  else if (symbol == NULL)
    einfo ("%B: %s\n", abfd, warning);
  else
    {
      lang_input_statement_type *entry;
      asymbol **asymbols;
      struct warning_callback_info info;

      /* Look through the relocs to see if we can find a plausible
	 address.  */

      entry = (lang_input_statement_type *) abfd->usrdata;
      if (entry != NULL && entry->asymbols != NULL)
	asymbols = entry->asymbols;
      else
	{
	  long symsize;
	  long symbol_count;

	  symsize = bfd_get_symtab_upper_bound (abfd);
	  if (symsize < 0)
	    einfo (_("%B%F: could not read symbols: %E\n"), abfd);
	  asymbols = (asymbol **) xmalloc (symsize);
	  symbol_count = bfd_canonicalize_symtab (abfd, asymbols);
	  if (symbol_count < 0)
	    einfo (_("%B%F: could not read symbols: %E\n"), abfd);
	  if (entry != NULL)
	    {
	      entry->asymbols = asymbols;
	      entry->symbol_count = symbol_count;
	    }
	}

      info.found = false;
      info.warning = warning;
      info.symbol = symbol;
      info.asymbols = asymbols;
      bfd_map_over_sections (abfd, warning_find_reloc, (PTR) &info);

      if (! info.found)
	einfo ("%B: %s\n", abfd, warning);

      if (entry == NULL)
	free (asymbols);
    }

  return true;
}

/* This is called by warning_callback for each section.  It checks the
   relocs of the section to see if it can find a reference to the
   symbol which triggered the warning.  If it can, it uses the reloc
   to give an error message with a file and line number.  */

static void
warning_find_reloc (abfd, sec, iarg)
     bfd *abfd;
     asection *sec;
     PTR iarg;
{
  struct warning_callback_info *info = (struct warning_callback_info *) iarg;
  long relsize;
  arelent **relpp;
  long relcount;
  arelent **p, **pend;

  if (info->found)
    return;

  relsize = bfd_get_reloc_upper_bound (abfd, sec);
  if (relsize < 0)
    einfo (_("%B%F: could not read relocs: %E\n"), abfd);
  if (relsize == 0)
    return;

  relpp = (arelent **) xmalloc (relsize);
  relcount = bfd_canonicalize_reloc (abfd, sec, relpp, info->asymbols);
  if (relcount < 0)
    einfo (_("%B%F: could not read relocs: %E\n"), abfd);

  p = relpp;
  pend = p + relcount;
  for (; p < pend && *p != NULL; p++)
    {
      arelent *q = *p;

      if (q->sym_ptr_ptr != NULL
	  && *q->sym_ptr_ptr != NULL
	  && strcmp (bfd_asymbol_name (*q->sym_ptr_ptr), info->symbol) == 0)
	{
	  /* We found a reloc for the symbol we are looking for.  */
	  einfo ("%C: %s\n", abfd, sec, q->address, info->warning);
	  info->found = true;
	  break;
	}
    }

  free (relpp);
}

/* This is called when an undefined symbol is found.  */

static boolean
undefined_symbol (info, name, abfd, section, address, fatal)
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
     const char *name;
     bfd *abfd;
     asection *section;
     bfd_vma address;
     boolean fatal ATTRIBUTE_UNUSED;
{
  static char *error_name;
  static unsigned int error_count;

#define MAX_ERRORS_IN_A_ROW 5

  if (config.warn_once)
    {
      static struct bfd_hash_table *hash;

      /* Only warn once about a particular undefined symbol.  */

      if (hash == NULL)
	{
	  hash = ((struct bfd_hash_table *)
		  xmalloc (sizeof (struct bfd_hash_table)));
	  if (! bfd_hash_table_init (hash, bfd_hash_newfunc))
	    einfo (_("%F%P: bfd_hash_table_init failed: %E\n"));
	}

      if (bfd_hash_lookup (hash, name, false, false) != NULL)
	return true;

      if (bfd_hash_lookup (hash, name, true, true) == NULL)
	einfo (_("%F%P: bfd_hash_lookup failed: %E\n"));
    }

  /* We never print more than a reasonable number of errors in a row
     for a single symbol.  */
  if (error_name != (char *) NULL
      && strcmp (name, error_name) == 0)
    ++error_count;
  else
    {
      error_count = 0;
      if (error_name != (char *) NULL)
	free (error_name);
      error_name = xstrdup (name);
    }

  if (section != NULL)
    {
      if (error_count < MAX_ERRORS_IN_A_ROW)
	{
	  einfo (_("%C: undefined reference to `%T'\n"),
		 abfd, section, address, name);
	  if (fatal)
	    einfo ("%X");
	}
      else if (error_count == MAX_ERRORS_IN_A_ROW)
	einfo (_("%D: more undefined references to `%T' follow\n"),
	       abfd, section, address, name);
    }
  else
    {
      if (error_count < MAX_ERRORS_IN_A_ROW)
	{
	  einfo (_("%B: undefined reference to `%T'\n"),
		 abfd, name);
	  if (fatal)
	    einfo ("%X");
	}
      else if (error_count == MAX_ERRORS_IN_A_ROW)
	einfo (_("%B: more undefined references to `%T' follow\n"),
	       abfd, name);
    }

  return true;
}

/* This is called when a reloc overflows.  */

static boolean
reloc_overflow (info, name, reloc_name, addend, abfd, section, address)
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
     const char *name;
     const char *reloc_name;
     bfd_vma addend;
     bfd *abfd;
     asection *section;
     bfd_vma address;
{
  if (abfd == (bfd *) NULL)
    einfo (_("%P%X: generated"));
  else
    einfo ("%X%C:", abfd, section, address);
  einfo (_(" relocation truncated to fit: %s %T"), reloc_name, name);
  if (addend != 0)
    einfo ("+%v", addend);
  einfo ("\n");
  return true;
}

/* This is called when a dangerous relocation is made.  */

static boolean
reloc_dangerous (info, message, abfd, section, address)
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
     const char *message;
     bfd *abfd;
     asection *section;
     bfd_vma address;
{
  if (abfd == (bfd *) NULL)
    einfo (_("%P%X: generated"));
  else
    einfo ("%X%C:", abfd, section, address);
  einfo (_("dangerous relocation: %s\n"), message);
  return true;
}

/* This is called when a reloc is being generated attached to a symbol
   that is not being output.  */

static boolean
unattached_reloc (info, name, abfd, section, address)
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
     const char *name;
     bfd *abfd;
     asection *section;
     bfd_vma address;
{
  if (abfd == (bfd *) NULL)
    einfo (_("%P%X: generated"));
  else
    einfo ("%X%C:", abfd, section, address);
  einfo (_(" reloc refers to symbol `%T' which is not being output\n"), name);
  return true;
}

/* This is called if link_info.notice_all is set, or when a symbol in
   link_info.notice_hash is found.  Symbols are put in notice_hash
   using the -y option.  */

static boolean
notice (info, name, abfd, section, value)
     struct bfd_link_info *info;
     const char *name;
     bfd *abfd;
     asection *section;
     bfd_vma value;
{
  if (! info->notice_all
      || (info->notice_hash != NULL
	  && bfd_hash_lookup (info->notice_hash, name, false, false) != NULL))
    {
      if (bfd_is_und_section (section))
	einfo ("%B: reference to %s\n", abfd, name);
      else
	einfo ("%B: definition of %s\n", abfd, name);
    }

  if (command_line.cref || nocrossref_list != NULL)
    add_cref (name, abfd, section, value);

  return true;
}
