/* GNU DIFF main routine.
   Copyright (C) 1988, 1989, 1992 Free Software Foundation, Inc.

This file is part of GNU DIFF.

GNU DIFF is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU DIFF is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU DIFF; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* GNU DIFF was written by Mike Haertel, David Hayes,
   Richard Stallman, Len Tower, and Paul Eggert.  */

#define GDIFF_MAIN
#include "diff.h"
#include "getopt.h"
#include "fnmatch.h"

#ifndef DEFAULT_WIDTH
#define DEFAULT_WIDTH 130
#endif

#ifndef GUTTER_WIDTH_MINIMUM
#define GUTTER_WIDTH_MINIMUM 3
#endif

int diff_dirs ();
int diff_2_files ();

static int compare_files ();
static int specify_format ();
static void add_regexp();
static void specify_style ();
static void usage ();

/* Nonzero for -r: if comparing two directories,
   compare their common subdirectories recursively.  */

static int recursive;

/* For debugging: don't do discard_confusing_lines.  */

int no_discards;

/* Return a string containing the command options with which diff was invoked.
   Spaces appear between what were separate ARGV-elements.
   There is a space at the beginning but none at the end.
   If there were no options, the result is an empty string.

   Arguments: OPTIONVEC, a vector containing separate ARGV-elements, and COUNT,
   the length of that vector.  */

static char *
option_list (optionvec, count)
     char **optionvec;  /* Was `vector', but that collides on Alliant.  */
     int count;
{
  int i;
  int length = 0;
  char *result;

  for (i = 0; i < count; i++)
    length += strlen (optionvec[i]) + 1;

  result = (char *) xmalloc (length + 1);
  result[0] = 0;

  for (i = 0; i < count; i++)
    {
      strcat (result, " ");
      strcat (result, optionvec[i]);
    }

  return result;
}

/* Convert STR to a positive integer, storing the result in *OUT. 
   If STR is not a valid integer, return -1 (otherwise 0). */
static int
ck_atoi (str, out)
     char *str;
     int *out;
{
  char *p;
  for (p = str; *p; p++)
    if (*p < '0' || *p > '9')
      return -1;

  *out = atoi (optarg);
  return 0;
}

/* Keep track of excluded file name patterns.  */

static const char **exclude;
static int exclude_alloc, exclude_count;

int
excluded_filename (f)
     const char *f;
{
  int i;
  for (i = 0;  i < exclude_count;  i++)
    if (fnmatch (exclude[i], f, 0) == 0)
      return 1;
  return 0;
}

static void
add_exclude (pattern)
     const char *pattern;
{
  if (exclude_alloc <= exclude_count)
    exclude = (const char **)
	      (exclude_alloc == 0
	       ? xmalloc ((exclude_alloc = 64) * sizeof (*exclude))
	       : xrealloc (exclude, (exclude_alloc *= 2) * sizeof (*exclude)));

  exclude[exclude_count++] = pattern;
}

static int
add_exclude_file (name)
     const char *name;
{
  struct file_data f;
  char *p, *q, *lim;

  f.name = optarg;
  f.desc = strcmp (optarg, "-") == 0 ? 0 : open (optarg, O_RDONLY, 0);
  if (f.desc < 0 || fstat (f.desc, &f.stat) != 0)
    return -1;

  sip (&f, 1);
  slurp (&f);

  for (p = f.buffer, lim = p + f.buffered_chars;  p < lim;  p = q)
    {
      q = memchr (p, '\n', lim - p);
      if (!q)
	q = lim;
      *q++ = 0;
      add_exclude (p);
    }

  return close (f.desc);
}

/* The numbers 129- that appear in the fourth element of some entries
   tell the big switch in `main' how to process those options.  */

static struct option longopts[] =
{
  {"ignore-blank-lines", 0, 0, 'B'},
  {"context", 2, 0, 'C'},
  {"ifdef", 1, 0, 'D'},
  {"show-function-line", 1, 0, 'F'},
  {"speed-large-files", 0, 0, 'H'},
  {"ignore-matching-lines", 1, 0, 'I'},
  {"label", 1, 0, 'L'},
  {"file-label", 1, 0, 'L'},	/* An alias, no longer recommended */
  {"new-file", 0, 0, 'N'},
  {"entire-new-file", 0, 0, 'N'},	/* An alias, no longer recommended */
  {"unidirectional-new-file", 0, 0, 'P'},
  {"starting-file", 1, 0, 'S'},
  {"initial-tab", 0, 0, 'T'},
  {"width", 1, 0, 'W'},
  {"text", 0, 0, 'a'},
  {"ascii", 0, 0, 'a'},		/* An alias, no longer recommended */
  {"ignore-space-change", 0, 0, 'b'},
  {"minimal", 0, 0, 'd'},
  {"ed", 0, 0, 'e'},
  {"forward-ed", 0, 0, 'f'},
  {"ignore-case", 0, 0, 'i'},
  {"paginate", 0, 0, 'l'},
  {"print", 0, 0, 'l'},		/* An alias, no longer recommended */
  {"rcs", 0, 0, 'n'},
  {"show-c-function", 0, 0, 'p'},
  {"binary", 0, 0, 'q'},	/* An alias, no longer recommended */
  {"brief", 0, 0, 'q'},
  {"recursive", 0, 0, 'r'},
  {"report-identical-files", 0, 0, 's'},
  {"expand-tabs", 0, 0, 't'},
  {"version", 0, 0, 'v'},
  {"ignore-all-space", 0, 0, 'w'},
  {"exclude", 1, 0, 'x'},
  {"exclude-from", 1, 0, 'X'},
  {"side-by-side", 0, 0, 'y'},
  {"unified", 2, 0, 'U'},
  {"left-column", 0, 0, 129},
  {"suppress-common-lines", 0, 0, 130},
  {"sdiff-merge-assist", 0, 0, 131},
  {"old-line-format", 1, 0, 132},
  {"new-line-format", 1, 0, 133},
  {"unchanged-line-format", 1, 0, 134},
  {"old-group-format", 1, 0, 135},
  {"new-group-format", 1, 0, 136},
  {"unchanged-group-format", 1, 0, 137},
  {"changed-group-format", 1, 0, 138},
  {"horizon-lines", 1, 0, 139},
  {0, 0, 0, 0}
};

int
main (argc, argv)
     int argc;
     char *argv[];
{
  int val;
  int c;
  int prev = -1;
  extern char *version_string;
  int width = DEFAULT_WIDTH;

  program = argv[0];

  /* Do our initializations. */
  output_style = OUTPUT_NORMAL;
  always_text_flag = FALSE;
  ignore_space_change_flag = FALSE;
  ignore_all_space_flag = FALSE;
  length_varies = FALSE;
  ignore_case_flag = FALSE;
  ignore_blank_lines_flag = FALSE;
  ignore_regexp_list = NULL;
  function_regexp_list = NULL;
  print_file_same_flag = FALSE;
  entire_new_file_flag = FALSE;
  unidirectional_new_file_flag = FALSE;
  no_details_flag = FALSE;
  context = -1;
  line_end_char = '\n';
  tab_align_flag = FALSE;
  tab_expand_flag = FALSE;
  recursive = FALSE;
  paginate_flag = FALSE;
  heuristic = FALSE;
  dir_start_file = NULL;
  msg_chain = NULL;
  msg_chain_end = NULL;
  no_discards = 0;

  /* Decode the options.  */

  while ((c = getopt_long (argc, argv,
			   "0123456789abBcC:dD:efF:hHiI:lL:nNpPqrsS:tTuU:vwW:x:X:y",
			   longopts, (int *)0)) != EOF)
    {
      switch (c)
	{
	  /* All digits combine in decimal to specify the context-size.  */
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
	case '0':
	  if (context == -1)
	    context = 0;
	  /* If a context length has already been specified,
	     more digits allowed only if they follow right after the others.
	     Reject two separate runs of digits, or digits after -C.  */
	  else if (prev < '0' || prev > '9')
	    fatal ("context length specified twice");

	  context = context * 10 + c - '0';
	  break;

	case 'a':
	  /* Treat all files as text files; never treat as binary.  */
	  always_text_flag = 1;
	  break;

	case 'b':
	  /* Ignore changes in amount of whitespace.  */
	  ignore_space_change_flag = 1;
	  length_varies = 1;
	  break;

	case 'B':
	  /* Ignore changes affecting only blank lines.  */
	  ignore_blank_lines_flag = 1;
	  break;

	case 'C':		/* +context[=lines] */
	case 'U':		/* +unified[=lines] */
	  if (optarg)
	    {
	      if (context >= 0)
		fatal ("context length specified twice");

	      if (ck_atoi (optarg, &context))
		fatal ("invalid context length argument");
	    }

	  /* Falls through.  */
	case 'c':
	  /* Make context-style output.  */
	  specify_style (c == 'U' ? OUTPUT_UNIFIED : OUTPUT_CONTEXT);
	  break;

	case 'd':
	  /* Don't discard lines.  This makes things slower (sometimes much
	     slower) but will find a guaranteed minimal set of changes.  */
	  no_discards = 1;
	  break;

	case 'D':
	  /* Make merged #ifdef output.  */
	  specify_style (OUTPUT_IFDEF);
	  {
	    int i, err = 0;
	    static const char C_ifdef_group_formats[] =
	      "#ifndef %s\n%%<#endif /* not %s */\n%c#ifdef %s\n%%>#endif /* %s */\n%c%%=%c#ifndef %s\n%%<#else /* %s */\n%%>#endif /* %s */\n";
	    char *b = xmalloc (sizeof (C_ifdef_group_formats)
			       + 7 * strlen(optarg) - 14 /* 7*"%s" */
			       - 8 /* 5*"%%" + 3*"%c" */);
	    sprintf (b, C_ifdef_group_formats,
		     optarg, optarg, 0,
		     optarg, optarg, 0, 0,
		     optarg, optarg, optarg);
	    for (i = 0; i < 4; i++)
	      {
		err |= specify_format (&group_format[i], b);
		b += strlen (b) + 1;
	      }
	    if (err)
	      error ("conflicting #ifdef formats", 0, 0);
	  }
	  break;

	case 'e':
	  /* Make output that is a valid `ed' script.  */
	  specify_style (OUTPUT_ED);
	  break;

	case 'f':
	  /* Make output that looks vaguely like an `ed' script
	     but has changes in the order they appear in the file.  */
	  specify_style (OUTPUT_FORWARD_ED);
	  break;

	case 'F':
	  /* Show, for each set of changes, the previous line that
	     matches the specified regexp.  Currently affects only
	     context-style output.  */
	  add_regexp (&function_regexp_list, optarg);
	  break;

	case 'h':
	  /* Split the files into chunks of around 1500 lines
	     for faster processing.  Usually does not change the result.

	     This currently has no effect.  */
	  break;

	case 'H':
	  /* Turn on heuristics that speed processing of large files
	     with a small density of changes.  */
	  heuristic = 1;
	  break;

	case 'i':
	  /* Ignore changes in case.  */
	  ignore_case_flag = 1;
	  break;

	case 'I':
	  /* Ignore changes affecting only lines that match the
	     specified regexp.  */
	  add_regexp (&ignore_regexp_list, optarg);
	  break;

	case 'l':
	  /* Pass the output through `pr' to paginate it.  */
	  paginate_flag = 1;
	  break;

	case 'L':
	  /* Specify file labels for `-c' output headers.  */
	  if (!file_label[0])
	    file_label[0] = optarg;
	  else if (!file_label[1])
	    file_label[1] = optarg;
	  else
	    fatal ("too many file label options");
	  break;
	  
	case 'n':
	  /* Output RCS-style diffs, like `-f' except that each command
	     specifies the number of lines affected.  */
	  specify_style (OUTPUT_RCS);
	  break;

	case 'N':
	  /* When comparing directories, if a file appears only in one
	     directory, treat it as present but empty in the other.  */
	  entire_new_file_flag = 1;
	  break;

	case 'p':
	  /* Make context-style output and show name of last C function.  */
	  specify_style (OUTPUT_CONTEXT);
	  add_regexp (&function_regexp_list, "^[_a-zA-Z$]");
	  break;

	case 'P':
	  /* When comparing directories, if a file appears only in
	     the second directory of the two,
	     treat it as present but empty in the other.  */
	  unidirectional_new_file_flag = 1;
	  break;

	case 'q':
	  no_details_flag = 1;
	  break;

	case 'r':
	  /* When comparing directories, 
	     recursively compare any subdirectories found.  */
	  recursive = 1;
	  break;

	case 's':
	  /* Print a message if the files are the same.  */
	  print_file_same_flag = 1;
	  break;

	case 'S':
	  /* When comparing directories, start with the specified
	     file name.  This is used for resuming an aborted comparison.  */
	  dir_start_file = optarg;
	  break;

	case 't':
	  /* Expand tabs to spaces in the output so that it preserves
	     the alignment of the input files.  */
	  tab_expand_flag = 1;
	  break;

	case 'T':
	  /* Use a tab in the output, rather than a space, before the
	     text of an input line, so as to keep the proper alignment
	     in the input line without changing the characters in it.  */
	  tab_align_flag = 1;
	  break;

	case 'u':
	  /* Output the context diff in unidiff format.  */
	  specify_style (OUTPUT_UNIFIED);
	  break;

	case 'v':
	  fprintf (stderr, "GNU diff version %s\n", version_string);
	  break;

	case 'w':
	  /* Ignore horizontal whitespace when comparing lines.  */
	  ignore_all_space_flag = 1;
	  length_varies = 1;
	  break;

	case 'x':
	  add_exclude (optarg);
	  break;

	case 'X':
	  if (add_exclude_file (optarg) != 0)
	    pfatal_with_name (optarg);
	  break;

	case 'y':
	  /* Use side-by-side (sdiff-style) columnar output. */
	  specify_style (OUTPUT_SDIFF);
	  break;

	case 'W':
	  /* Set the line width for OUTPUT_SDIFF.  */
	  if (ck_atoi (optarg, &width) || width <= 0)
	    fatal ("column width must be a positive integer");
	  break;
	  
	case 129:
	  sdiff_left_only = 1;
	  break;
	  
	case 130:
	  sdiff_skip_common_lines = 1;
	  break;
	  
	case 131:
	  /* sdiff-style columns output. */
	  specify_style (OUTPUT_SDIFF);
	  sdiff_help_sdiff = 1;
	  break;

	case 132:
	case 133:
	case 134:
	  specify_style (OUTPUT_IFDEF);
	  {
	    const char **form = &line_format[c - 132];
	    if (*form && strcmp (*form, optarg) != 0)
	      error ("conflicting line format", 0, 0);
	    *form = optarg;
	  }
	  break;

	case 135:
	case 136:
	case 137:
	case 138:
	  specify_style (OUTPUT_IFDEF);
	  {
	    const char **form = &group_format[c - 135];
	    if (*form && strcmp (*form, optarg) != 0)
	      error ("conflicting group format", 0, 0);
	    *form = optarg;
	  }
	  break;

	case 139:
	  if (ck_atoi (optarg, &horizon_lines) || horizon_lines < 0)
	    fatal ("horizon must be a nonnegative integer");
	  break;

	default:
	  usage ();
	}
      prev = c;
    }

  if (optind != argc - 2)
    usage ();


  {
    /*
     *	We maximize first the half line width, and then the gutter width,
     *	according to the following constraints:
     *	1.  Two half lines plus a gutter must fit in a line.
     *	2.  If the half line width is nonzero:
     *	    a.  The gutter width is at least GUTTER_WIDTH_MINIMUM.
     *	    b.  If tabs are not expanded to spaces,
     *		a half line plus a gutter is an integral number of tabs,
     *		so that tabs in the right column line up.
     */
    int t = tab_expand_flag ? 1 : TAB_WIDTH;
    int off = (width + t + GUTTER_WIDTH_MINIMUM) / (2*t)  *  t;
    sdiff_half_width = max (0, min (off - GUTTER_WIDTH_MINIMUM, width - off)),
    sdiff_column2_offset = sdiff_half_width ? off : width;
  }

  if (output_style != OUTPUT_CONTEXT && output_style != OUTPUT_UNIFIED)
    context = 0;
  else if (context == -1)
    /* Default amount of context for -c.  */
    context = 3;
 
  if (output_style == OUTPUT_IFDEF)
    {
      int i;
      for (i = 0; i < sizeof (line_format) / sizeof (*line_format); i++)
	if (!line_format[i])
	  line_format[i] = "%l\n";
      if (!group_format[OLD])
	group_format[OLD]
	  = group_format[UNCHANGED] ? group_format[UNCHANGED] : "%<";
      if (!group_format[NEW])
	group_format[NEW]
	  = group_format[UNCHANGED] ? group_format[UNCHANGED] : "%>";
      if (!group_format[UNCHANGED])
	group_format[UNCHANGED] = "%=";
      if (!group_format[CHANGED])
	group_format[CHANGED] = concat (group_format[OLD],
					group_format[NEW], "");
    }

  no_diff_means_no_output =
    (output_style == OUTPUT_IFDEF ?
      (!*group_format[UNCHANGED]
       || (strcmp (group_format[UNCHANGED], "%=") == 0
	   && !*line_format[UNCHANGED]))
     : output_style == OUTPUT_SDIFF ? sdiff_skip_common_lines : 1);

  switch_string = option_list (argv + 1, optind - 1);

  val = compare_files (NULL, argv[optind], NULL, argv[optind + 1], 0);

  /* Print any messages that were saved up for last.  */
  print_message_queue ();

  if (ferror (stdout) || fclose (stdout) != 0)
    fatal ("write error");
  exit (val);
  return val;
}

/* Add the compiled form of regexp PATTERN to REGLIST.  */

static void
add_regexp (reglist, pattern)
     struct regexp_list **reglist;
     char *pattern;
{
  struct regexp_list *r;
  const char *m;

  r = (struct regexp_list *) xmalloc (sizeof (*r));
  bzero (r, sizeof (*r));
  r->buf.fastmap = (char *) xmalloc (256);
  m = re_compile_pattern (pattern, strlen (pattern), &r->buf);
  if (m != 0)
    error ("%s: %s", pattern, m);

  /* Add to the start of the list, since it's easier than the end.  */
  r->next = *reglist;
  *reglist = r;
}

static void
usage ()
{
  fprintf (stderr, "Usage: %s [options] from-file to-file\n", program);
  fprintf (stderr, "Options:\n\
       [-abBcdefhHilnNpPqrstTuvwy] [-C lines] [-D name] [-F regexp]\n\
       [-I regexp] [-L from-label [-L to-label]] [-S starting-file] [-U lines]\n\
       [-W columns] [-x pattern] [-X pattern-file] [--exclude=pattern]\n\
       [--exclude-from=pattern-file] [--ignore-blank-lines] [--context[=lines]]\n\
       [--ifdef=name] [--show-function-line=regexp] [--speed-large-files]\n\
       [--label=from-label [--label=to-label]] [--new-file]\n");
  fprintf (stderr, "\
       [--ignore-matching-lines=regexp] [--unidirectional-new-file]\n\
       [--starting-file=starting-file] [--initial-tab] [--width=columns]\n\
       [--text] [--ignore-space-change] [--minimal] [--ed] [--forward-ed]\n\
       [--ignore-case] [--paginate] [--rcs] [--show-c-function] [--brief]\n\
       [--recursive] [--report-identical-files] [--expand-tabs] [--version]\n");
  fprintf (stderr, "\
       [--ignore-all-space] [--side-by-side] [--unified[=lines]]\n\
       [--left-column] [--suppress-common-lines] [--sdiff-merge-assist]\n\
       [--old-line-format=format] [--new-line-format=format]\n\
       [--unchanged-line-format=format]\n\
       [--old-group-format=format] [--new-group-format=format]\n\
       [--unchanged-group-format=format] [--changed-group-format=format]\n\
       [--horizon-lines=lines]\n");
  exit (2);
} 

static int
specify_format (var, value)
     const char **var;
     const char *value;
{
  int err = *var ? strcmp (*var, value) : 0;
  *var = value;
  return err;
}

static void
specify_style (style)
     enum output_style style;
{
  if (output_style != OUTPUT_NORMAL
      && output_style != style)
    error ("conflicting specifications of output style", 0, 0);
  output_style = style;
}

/* Compare two files (or dirs) with specified names
   DIR0/NAME0 and DIR1/NAME1, at level DEPTH in directory recursion.
   (if DIR0 is 0, then the name is just NAME0, etc.)
   This is self-contained; it opens the files and closes them.

   Value is 0 if files are the same, 1 if different,
   2 if there is a problem opening them.  */

static int
compare_files (dir0, name0, dir1, name1, depth)
     char *dir0, *dir1;
     char *name0, *name1;
     int depth;
{
  struct file_data inf[2];
  register int i;
  int val;
  int same_files;
  int errorcount = 0;

  /* If this is directory comparison, perhaps we have a file
     that exists only in one of the directories.
     If so, just print a message to that effect.  */

  if (! ((name0 != 0 && name1 != 0)
	 || (unidirectional_new_file_flag && name1 != 0)
	 || entire_new_file_flag))
    {
      char *name = name0 == 0 ? name1 : name0;
      char *dir = name0 == 0 ? dir1 : dir0;
      message ("Only in %s: %s\n", dir, name);
      /* Return 1 so that diff_dirs will return 1 ("some files differ").  */
      return 1;
    }

  /* Mark any nonexistent file with -1 in the desc field.  */
  /* Mark unopened files (i.e. directories) with -2. */

  inf[0].desc = name0 == 0 ? -1 : -2;
  inf[1].desc = name1 == 0 ? -1 : -2;

  /* Now record the full name of each file, including nonexistent ones.  */

  if (name0 == 0)
    name0 = name1;
  if (name1 == 0)
    name1 = name0;

  inf[0].name = dir0 == 0 ? name0 : concat (dir0, "/", name0);
  inf[1].name = dir1 == 0 ? name1 : concat (dir1, "/", name1);

  /* Stat the files.  Record whether they are directories.  */

  for (i = 0; i <= 1; i++)
    {
      bzero (&inf[i].stat, sizeof (struct stat));
      inf[i].dir_p = 0;

      if (inf[i].desc != -1)
	{
	  int stat_result;

	  if (strcmp (inf[i].name, "-") == 0)
	    {
	      inf[i].desc = 0;
	      inf[i].name = "Standard Input";
	      stat_result = fstat (0, &inf[i].stat);
	    }
	  else
	    stat_result = stat (inf[i].name, &inf[i].stat);

	  if (stat_result != 0)
	    {
	      perror_with_name (inf[i].name);
	      errorcount = 1;
	    }
	  else
	    inf[i].dir_p = S_ISDIR (inf[i].stat.st_mode) && inf[i].desc != 0;
	}
    }

  if (name0 == 0)
    inf[0].dir_p = inf[1].dir_p;
  if (name1 == 0)
    inf[1].dir_p = inf[0].dir_p;

  if (errorcount == 0 && depth == 0 && inf[0].dir_p != inf[1].dir_p)
    {
      /* If one is a directory, and it was specified in the command line,
	 use the file in that dir with the other file's basename.  */

      int fnm_arg = inf[0].dir_p;
      int dir_arg = 1 - fnm_arg;
      char *p = rindex (inf[fnm_arg].name, '/');
      char *filename = inf[dir_arg].name
	= concat (inf[dir_arg].name,  "/", (p ? p+1 : inf[fnm_arg].name));

      if (inf[fnm_arg].desc == 0)
	fatal ("can't compare - to a directory");

      if (stat (filename, &inf[dir_arg].stat) != 0)
	{
	  perror_with_name (filename);
	  errorcount = 1;
	}
      else
	inf[dir_arg].dir_p = S_ISDIR (inf[dir_arg].stat.st_mode);
    }

  if (errorcount)
    {

      /* If either file should exist but does not, return 2.  */

      val = 2;

    }
  else if ((same_files =    inf[0].stat.st_ino == inf[1].stat.st_ino
			 && inf[0].stat.st_dev == inf[1].stat.st_dev
			 && inf[0].desc != -1
			 && inf[1].desc != -1)
	   && no_diff_means_no_output)
    {
      /* The two named files are actually the same physical file.
	 We know they are identical without actually reading them.  */

      val = 0;
    }
  else if (inf[0].dir_p & inf[1].dir_p)
    {
      if (output_style == OUTPUT_IFDEF)
	fatal ("-D option not supported with directories");

      /* If both are directories, compare the files in them.  */

      if (depth > 0 && !recursive)
	{
	  /* But don't compare dir contents one level down
	     unless -r was specified.  */
	  message ("Common subdirectories: %s and %s\n",
		   inf[0].name, inf[1].name);
	  val = 0;
	}
      else
	{
	  val = diff_dirs (inf, compare_files, depth);
	}

    }
  else if (inf[0].dir_p | inf[1].dir_p)
    {
      /* Perhaps we have a subdirectory that exists only in one directory.
	 If so, just print a message to that effect.  */

      if (inf[0].desc == -1 || inf[1].desc == -1)
	{
	  if (recursive
	      && (entire_new_file_flag
		  || (unidirectional_new_file_flag && inf[0].desc == -1)))
	    val = diff_dirs (inf, compare_files, depth);
	  else
	    {
	      char *dir = (inf[0].desc == -1) ? dir1 : dir0;
	      message ("Only in %s: %s\n", dir, name0);
	      val = 1;
	    }
	}
      else
	{
	  /* We have a subdirectory in one directory
	     and a file in the other.  */

	  message ("%s is a directory but %s is not\n",
		   inf[1 - inf[0].dir_p].name, inf[inf[0].dir_p].name);

	  /* This is a difference.  */
	  val = 1;
	}
    }
  else if (no_details_flag
	   && inf[0].stat.st_size != inf[1].stat.st_size
	   && (inf[0].desc == -1 || S_ISREG (inf[0].stat.st_mode))
	   && (inf[1].desc == -1 || S_ISREG (inf[1].stat.st_mode)))
    {
      message ("Files %s and %s differ\n", inf[0].name, inf[1].name);
      val = 1;
    }
  else
    {
      /* Both exist and neither is a directory.  */

      /* Open the files and record their descriptors.  */

      if (inf[0].desc == -2)
	if ((inf[0].desc = open (inf[0].name, O_RDONLY, 0)) < 0)
	  {
	    perror_with_name (inf[0].name);
	    errorcount = 1;
	  }
      if (inf[1].desc == -2)
	if (same_files)
	  inf[1].desc = inf[0].desc;
	else if ((inf[1].desc = open (inf[1].name, O_RDONLY, 0)) < 0)
	  {
	    perror_with_name (inf[1].name);
	    errorcount = 1;
	  }
    
      /* Compare the files, if no error was found.  */

      val = errorcount ? 2 : diff_2_files (inf, depth);

      /* Close the file descriptors.  */

      if (inf[0].desc >= 0 && close (inf[0].desc) != 0)
	{
	  perror_with_name (inf[0].name);
	  val = 2;
	}
      if (inf[1].desc >= 0 && inf[0].desc != inf[1].desc
	  && close (inf[1].desc) != 0)
	{
	  perror_with_name (inf[1].name);
	  val = 2;
	}
    }

  /* Now the comparison has been done, if no error prevented it,
     and VAL is the value this function will return.  */

  if (val == 0 && !inf[0].dir_p)
    {
      if (print_file_same_flag)
	message ("Files %s and %s are identical\n",
		 inf[0].name, inf[1].name);
    }
  else
    fflush (stdout);

  if (dir0 != 0)
    free (inf[0].name);
  if (dir1 != 0)
    free (inf[1].name);

  return val;
}
