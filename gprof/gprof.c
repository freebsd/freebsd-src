/*
 * Copyright (c) 1983, 1993, 1998, 2001, 2002
 *      The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "libiberty.h"
#include "gprof.h"
#include "search_list.h"
#include "source.h"
#include "symtab.h"
#include "basic_blocks.h"
#include "call_graph.h"
#include "cg_arcs.h"
#include "cg_print.h"
#include "corefile.h"
#include "gmon_io.h"
#include "hertz.h"
#include "hist.h"
#include "sym_ids.h"
#include "demangle.h"
#include "getopt.h"

static void usage (FILE *, int) ATTRIBUTE_NORETURN;

const char *whoami;
const char *function_mapping_file;
const char *a_out_name = A_OUTNAME;
long hz = HZ_WRONG;

/*
 * Default options values:
 */
int debug_level = 0;
int output_style = 0;
int output_width = 80;
bfd_boolean bsd_style_output = FALSE;
bfd_boolean demangle = TRUE;
bfd_boolean discard_underscores = TRUE;
bfd_boolean ignore_direct_calls = FALSE;
bfd_boolean ignore_static_funcs = FALSE;
bfd_boolean ignore_zeros = TRUE;
bfd_boolean line_granularity = FALSE;
bfd_boolean print_descriptions = TRUE;
bfd_boolean print_path = FALSE;
bfd_boolean ignore_non_functions = FALSE;
File_Format file_format = FF_AUTO;

bfd_boolean first_output = TRUE;

char copyright[] =
 "@(#) Copyright (c) 1983 Regents of the University of California.\n\
 All rights reserved.\n";

static char *gmon_name = GMONNAME;	/* profile filename */

/*
 * Functions that get excluded by default:
 */
static char *default_excluded_list[] =
{
  "_gprof_mcount", "mcount", "_mcount", "__mcount", "__mcount_internal",
  "__mcleanup",
  "<locore>", "<hicore>",
  0
};

/* Codes used for the long options with no short synonyms.  150 isn't
   special; it's just an arbitrary non-ASCII char value.  */

#define OPTION_DEMANGLE		(150)
#define OPTION_NO_DEMANGLE	(OPTION_DEMANGLE + 1)

static struct option long_options[] =
{
  {"line", no_argument, 0, 'l'},
  {"no-static", no_argument, 0, 'a'},
  {"ignore-non-functions", no_argument, 0, 'D'},

    /* output styles: */

  {"annotated-source", optional_argument, 0, 'A'},
  {"no-annotated-source", optional_argument, 0, 'J'},
  {"flat-profile", optional_argument, 0, 'p'},
  {"no-flat-profile", optional_argument, 0, 'P'},
  {"graph", optional_argument, 0, 'q'},
  {"no-graph", optional_argument, 0, 'Q'},
  {"exec-counts", optional_argument, 0, 'C'},
  {"no-exec-counts", optional_argument, 0, 'Z'},
  {"function-ordering", no_argument, 0, 'r'},
  {"file-ordering", required_argument, 0, 'R'},
  {"file-info", no_argument, 0, 'i'},
  {"sum", no_argument, 0, 's'},

    /* various options to affect output: */

  {"all-lines", no_argument, 0, 'x'},
  {"demangle", optional_argument, 0, OPTION_DEMANGLE},
  {"no-demangle", no_argument, 0, OPTION_NO_DEMANGLE},
  {"directory-path", required_argument, 0, 'I'},
  {"display-unused-functions", no_argument, 0, 'z'},
  {"min-count", required_argument, 0, 'm'},
  {"print-path", no_argument, 0, 'L'},
  {"separate-files", no_argument, 0, 'y'},
  {"static-call-graph", no_argument, 0, 'c'},
  {"table-length", required_argument, 0, 't'},
  {"time", required_argument, 0, 'n'},
  {"no-time", required_argument, 0, 'N'},
  {"width", required_argument, 0, 'w'},
    /*
     * These are for backwards-compatibility only.  Their functionality
     * is provided by the output style options already:
     */
  {"", required_argument, 0, 'e'},
  {"", required_argument, 0, 'E'},
  {"", required_argument, 0, 'f'},
  {"", required_argument, 0, 'F'},
  {"", required_argument, 0, 'k'},

    /* miscellaneous: */

  {"brief", no_argument, 0, 'b'},
  {"debug", optional_argument, 0, 'd'},
  {"help", no_argument, 0, 'h'},
  {"file-format", required_argument, 0, 'O'},
  {"traditional", no_argument, 0, 'T'},
  {"version", no_argument, 0, 'v'},
  {0, no_argument, 0, 0}
};


static void
usage (FILE *stream, int status)
{
  fprintf (stream, _("\
Usage: %s [-[abcDhilLsTvwxyz]] [-[ACeEfFJnNOpPqQZ][name]] [-I dirs]\n\
	[-d[num]] [-k from/to] [-m min-count] [-t table-length]\n\
	[--[no-]annotated-source[=name]] [--[no-]exec-counts[=name]]\n\
	[--[no-]flat-profile[=name]] [--[no-]graph[=name]]\n\
	[--[no-]time=name] [--all-lines] [--brief] [--debug[=level]]\n\
	[--function-ordering] [--file-ordering]\n\
	[--directory-path=dirs] [--display-unused-functions]\n\
	[--file-format=name] [--file-info] [--help] [--line] [--min-count=n]\n\
	[--no-static] [--print-path] [--separate-files]\n\
	[--static-call-graph] [--sum] [--table-length=len] [--traditional]\n\
	[--version] [--width=n] [--ignore-non-functions]\n\
	[--demangle[=STYLE]] [--no-demangle] [@FILE]\n\
	[image-file] [profile-file...]\n"),
	   whoami);
  if (status == 0)
    fprintf (stream, _("Report bugs to %s\n"), REPORT_BUGS_TO);
  done (status);
}


int
main (int argc, char **argv)
{
  char **sp, *str;
  Sym **cg = 0;
  int ch, user_specified = 0;

#if defined (HAVE_SETLOCALE) && defined (HAVE_LC_MESSAGES)
  setlocale (LC_MESSAGES, "");
#endif
#if defined (HAVE_SETLOCALE)
  setlocale (LC_CTYPE, "");
#endif
#ifdef ENABLE_NLS
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);
#endif

  whoami = argv[0];
  xmalloc_set_program_name (whoami);

  expandargv (&argc, &argv);

  while ((ch = getopt_long (argc, argv,
	"aA::bBcCd::De:E:f:F:hiI:J::k:lLm:n::N::O:p::P::q::Q::st:Tvw:xyzZ::",
			    long_options, 0))
	 != EOF)
    {
      switch (ch)
	{
	case 'a':
	  ignore_static_funcs = TRUE;
	  break;
	case 'A':
	  if (optarg)
	    {
	      sym_id_add (optarg, INCL_ANNO);
	    }
	  output_style |= STYLE_ANNOTATED_SOURCE;
	  user_specified |= STYLE_ANNOTATED_SOURCE;
	  break;
	case 'b':
	  print_descriptions = FALSE;
	  break;
	case 'B':
	  output_style |= STYLE_CALL_GRAPH;
	  user_specified |= STYLE_CALL_GRAPH;
	  break;
	case 'c':
	  ignore_direct_calls = TRUE;
	  break;
	case 'C':
	  if (optarg)
	    {
	      sym_id_add (optarg, INCL_EXEC);
	    }
	  output_style |= STYLE_EXEC_COUNTS;
	  user_specified |= STYLE_EXEC_COUNTS;
	  break;
	case 'd':
	  if (optarg)
	    {
	      debug_level |= atoi (optarg);
	      debug_level |= ANYDEBUG;
	    }
	  else
	    {
	      debug_level = ~0;
	    }
	  DBG (ANYDEBUG, printf ("[main] debug-level=0x%x\n", debug_level));
#ifndef DEBUG
	  printf (_("%s: debugging not supported; -d ignored\n"), whoami);
#endif	/* DEBUG */
	  break;
	case 'D':
	  ignore_non_functions = TRUE;
	  break;
	case 'E':
	  sym_id_add (optarg, EXCL_TIME);
	case 'e':
	  sym_id_add (optarg, EXCL_GRAPH);
	  break;
	case 'F':
	  sym_id_add (optarg, INCL_TIME);
	case 'f':
	  sym_id_add (optarg, INCL_GRAPH);
	  break;
	case 'g':
	  sym_id_add (optarg, EXCL_FLAT);
	  break;
	case 'G':
	  sym_id_add (optarg, INCL_FLAT);
	  break;
	case 'h':
	  usage (stdout, 0);
	case 'i':
	  output_style |= STYLE_GMON_INFO;
	  user_specified |= STYLE_GMON_INFO;
	  break;
	case 'I':
	  search_list_append (&src_search_list, optarg);
	  break;
	case 'J':
	  if (optarg)
	    {
	      sym_id_add (optarg, EXCL_ANNO);
	      output_style |= STYLE_ANNOTATED_SOURCE;
	    }
	  else
	    {
	      output_style &= ~STYLE_ANNOTATED_SOURCE;
	    }
	  user_specified |= STYLE_ANNOTATED_SOURCE;
	  break;
	case 'k':
	  sym_id_add (optarg, EXCL_ARCS);
	  break;
	case 'l':
	  line_granularity = TRUE;
	  break;
	case 'L':
	  print_path = TRUE;
	  break;
	case 'm':
	  bb_min_calls = (unsigned long) strtoul (optarg, (char **) NULL, 10);
	  break;
	case 'n':
	  sym_id_add (optarg, INCL_TIME);
	  break;
	case 'N':
	  sym_id_add (optarg, EXCL_TIME);
	  break;
	case 'O':
	  switch (optarg[0])
	    {
	    case 'a':
	      file_format = FF_AUTO;
	      break;
	    case 'm':
	      file_format = FF_MAGIC;
	      break;
	    case 'b':
	      file_format = FF_BSD;
	      break;
	    case '4':
	      file_format = FF_BSD44;
	      break;
	    case 'p':
	      file_format = FF_PROF;
	      break;
	    default:
	      fprintf (stderr, _("%s: unknown file format %s\n"),
		       optarg, whoami);
	      done (1);
	    }
	  break;
	case 'p':
	  if (optarg)
	    {
	      sym_id_add (optarg, INCL_FLAT);
	    }
	  output_style |= STYLE_FLAT_PROFILE;
	  user_specified |= STYLE_FLAT_PROFILE;
	  break;
	case 'P':
	  if (optarg)
	    {
	      sym_id_add (optarg, EXCL_FLAT);
	      output_style |= STYLE_FLAT_PROFILE;
	    }
	  else
	    {
	      output_style &= ~STYLE_FLAT_PROFILE;
	    }
	  user_specified |= STYLE_FLAT_PROFILE;
	  break;
	case 'q':
	  if (optarg)
	    {
	      if (strchr (optarg, '/'))
		{
		  sym_id_add (optarg, INCL_ARCS);
		}
	      else
		{
		  sym_id_add (optarg, INCL_GRAPH);
		}
	    }
	  output_style |= STYLE_CALL_GRAPH;
	  user_specified |= STYLE_CALL_GRAPH;
	  break;
	case 'r':
	  output_style |= STYLE_FUNCTION_ORDER;
	  user_specified |= STYLE_FUNCTION_ORDER;
	  break;
	case 'R':
	  output_style |= STYLE_FILE_ORDER;
	  user_specified |= STYLE_FILE_ORDER;
	  function_mapping_file = optarg;
	  break;
	case 'Q':
	  if (optarg)
	    {
	      if (strchr (optarg, '/'))
		{
		  sym_id_add (optarg, EXCL_ARCS);
		}
	      else
		{
		  sym_id_add (optarg, EXCL_GRAPH);
		}
	      output_style |= STYLE_CALL_GRAPH;
	    }
	  else
	    {
	      output_style &= ~STYLE_CALL_GRAPH;
	    }
	  user_specified |= STYLE_CALL_GRAPH;
	  break;
	case 's':
	  output_style |= STYLE_SUMMARY_FILE;
	  user_specified |= STYLE_SUMMARY_FILE;
	  break;
	case 't':
	  bb_table_length = atoi (optarg);
	  if (bb_table_length < 0)
	    {
	      bb_table_length = 0;
	    }
	  break;
	case 'T':
	  bsd_style_output = TRUE;
	  break;
	case 'v':
	  /* This output is intended to follow the GNU standards document.  */
	  printf (_("GNU gprof %s\n"), VERSION);
	  printf (_("Based on BSD gprof, copyright 1983 Regents of the University of California.\n"));
	  printf (_("\
This program is free software.  This program has absolutely no warranty.\n"));
	  done (0);
	case 'w':
	  output_width = atoi (optarg);
	  if (output_width < 1)
	    {
	      output_width = 1;
	    }
	  break;
	case 'x':
	  bb_annotate_all_lines = TRUE;
	  break;
	case 'y':
	  create_annotation_files = TRUE;
	  break;
	case 'z':
	  ignore_zeros = FALSE;
	  break;
	case 'Z':
	  if (optarg)
	    {
	      sym_id_add (optarg, EXCL_EXEC);
	      output_style |= STYLE_EXEC_COUNTS;
	    }
	  else
	    {
	      output_style &= ~STYLE_EXEC_COUNTS;
	    }
	  user_specified |= STYLE_ANNOTATED_SOURCE;
	  break;
	case OPTION_DEMANGLE:
	  demangle = TRUE;
	  if (optarg != NULL)
	    {
	      enum demangling_styles style;

	      style = cplus_demangle_name_to_style (optarg);
	      if (style == unknown_demangling)
		{
		  fprintf (stderr,
			   _("%s: unknown demangling style `%s'\n"),
			   whoami, optarg);
		  xexit (1);
		}

	      cplus_demangle_set_style (style);
	   }
	  break;
	case OPTION_NO_DEMANGLE:
	  demangle = FALSE;
	  break;
	default:
	  usage (stderr, 1);
	}
    }

  /* Don't allow both ordering options, they modify the arc data in-place.  */
  if ((user_specified & STYLE_FUNCTION_ORDER)
      && (user_specified & STYLE_FILE_ORDER))
    {
      fprintf (stderr,_("\
%s: Only one of --function-ordering and --file-ordering may be specified.\n"),
	       whoami);
      done (1);
    }

  /* --sum implies --line, otherwise we'd lose basic block counts in
       gmon.sum */
  if (output_style & STYLE_SUMMARY_FILE)
    line_granularity = 1;

  /* append value of GPROF_PATH to source search list if set: */
  str = (char *) getenv ("GPROF_PATH");
  if (str)
    search_list_append (&src_search_list, str);

  if (optind < argc)
    a_out_name = argv[optind++];

  if (optind < argc)
    gmon_name = argv[optind++];

  /* Turn off default functions.  */
  for (sp = &default_excluded_list[0]; *sp; sp++)
    {
      sym_id_add (*sp, EXCL_TIME);
      sym_id_add (*sp, EXCL_GRAPH);
      sym_id_add (*sp, EXCL_FLAT);
    }

  /* Read symbol table from core file.  */
  core_init (a_out_name);

  /* If we should ignore direct function calls, we need to load to
     core's text-space.  */
  if (ignore_direct_calls)
    core_get_text_space (core_bfd);

  /* Create symbols from core image.  */
  if (line_granularity)
    core_create_line_syms ();
  else
    core_create_function_syms ();

  /* Translate sym specs into syms.  */
  sym_id_parse ();

  if (file_format == FF_PROF)
    {
      fprintf (stderr,
	       _("%s: sorry, file format `prof' is not yet supported\n"),
	       whoami);
      done (1);
    }
  else
    {
      /* Get information about gmon.out file(s).  */
      do
	{
	  gmon_out_read (gmon_name);
	  if (optind < argc)
	    gmon_name = argv[optind];
	}
      while (optind++ < argc);
    }

  /* If user did not specify output style, try to guess something
     reasonable.  */
  if (output_style == 0)
    {
      if (gmon_input & (INPUT_HISTOGRAM | INPUT_CALL_GRAPH))
	output_style = STYLE_FLAT_PROFILE | STYLE_CALL_GRAPH;
      else
	output_style = STYLE_EXEC_COUNTS;

      output_style &= ~user_specified;
    }

  /* Dump a gmon.sum file if requested (before any other
     processing!)  */
  if (output_style & STYLE_SUMMARY_FILE)
    {
      gmon_out_write (GMONSUM);
    }

  if (gmon_input & INPUT_HISTOGRAM)
    {
      hist_assign_samples ();
    }

  if (gmon_input & INPUT_CALL_GRAPH)
    {
      cg = cg_assemble ();
    }

  /* Do some simple sanity checks.  */
  if ((output_style & STYLE_FLAT_PROFILE)
      && !(gmon_input & INPUT_HISTOGRAM))
    {
      fprintf (stderr, _("%s: gmon.out file is missing histogram\n"), whoami);
      done (1);
    }

  if ((output_style & STYLE_CALL_GRAPH) && !(gmon_input & INPUT_CALL_GRAPH))
    {
      fprintf (stderr,
	       _("%s: gmon.out file is missing call-graph data\n"), whoami);
      done (1);
    }

  /* Output whatever user whishes to see.  */
  if (cg && (output_style & STYLE_CALL_GRAPH) && bsd_style_output)
    {
      /* Print the dynamic profile.  */
      cg_print (cg);
    }

  if (output_style & STYLE_FLAT_PROFILE)
    {
      /* Print the flat profile.  */
      hist_print ();		
    }

  if (cg && (output_style & STYLE_CALL_GRAPH))
    {
      if (!bsd_style_output)
	{
	  /* Print the dynamic profile.  */
	  cg_print (cg);	
	}
      cg_print_index ();
    }

  if (output_style & STYLE_EXEC_COUNTS)
    print_exec_counts ();
  
  if (output_style & STYLE_ANNOTATED_SOURCE)
    print_annotated_source ();
  
  if (output_style & STYLE_FUNCTION_ORDER)
    cg_print_function_ordering ();
  
  if (output_style & STYLE_FILE_ORDER)
    cg_print_file_ordering ();

  return 0;
}

void
done (int status)
{
  exit (status);
}
