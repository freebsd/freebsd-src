/* Three way file comparison program (diff3) for Project GNU.
   Copyright (C) 1988, 1989, 1992, 1993, 1994, 1997 Free Software Foundation, Inc.

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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* Written by Randy Smith */
/* Librarification by Tim Pierce */

#include "system.h"
#include <stdio.h>
#include <setjmp.h>
#include "getopt.h"

/* diff3.c has a real initialize_main function. */
#ifdef initialize_main
#undef initialize_main
#endif

extern char const diff_version_string[];

/*
 * Internal data structures and macros for the diff3 program; includes
 * data structures for both diff3 diffs and normal diffs.
 */

/* Different files within a three way diff.  */
#define	FILE0	0
#define	FILE1	1
#define	FILE2	2

/*
 * A three way diff is built from two two-way diffs; the file which
 * the two two-way diffs share is:
 */
#define	FILEC	FILE2

/*
 * Different files within a two way diff.
 * FC is the common file, FO the other file.
 */
#define FO 0
#define FC 1

/* The ranges are indexed by */
#define	START	0
#define	END	1

enum diff_type {
  ERROR,			/* Should not be used */
  ADD,				/* Two way diff add */
  CHANGE,			/* Two way diff change */
  DELETE,			/* Two way diff delete */
  DIFF_ALL,			/* All three are different */
  DIFF_1ST,			/* Only the first is different */
  DIFF_2ND,			/* Only the second */
  DIFF_3RD			/* Only the third */
};

/* Two way diff */
struct diff_block {
  int ranges[2][2];		/* Ranges are inclusive */
  char **lines[2];		/* The actual lines (may contain nulls) */
  size_t *lengths[2];		/* Line lengths (including newlines, if any) */
  struct diff_block *next;
};

/* Three way diff */

struct diff3_block {
  enum diff_type correspond;	/* Type of diff */
  int ranges[3][2];		/* Ranges are inclusive */
  char **lines[3];		/* The actual lines (may contain nulls) */
  size_t *lengths[3];		/* Line lengths (including newlines, if any) */
  struct diff3_block *next;
};

/*
 * Access the ranges on a diff block.
 */
#define	D_LOWLINE(diff, filenum)	\
  ((diff)->ranges[filenum][START])
#define	D_HIGHLINE(diff, filenum)	\
  ((diff)->ranges[filenum][END])
#define	D_NUMLINES(diff, filenum)	\
  (D_HIGHLINE (diff, filenum) - D_LOWLINE (diff, filenum) + 1)

/*
 * Access the line numbers in a file in a diff by relative line
 * numbers (i.e. line number within the diff itself).  Note that these
 * are lvalues and can be used for assignment.
 */
#define	D_RELNUM(diff, filenum, linenum)	\
  ((diff)->lines[filenum][linenum])
#define	D_RELLEN(diff, filenum, linenum)	\
  ((diff)->lengths[filenum][linenum])

/*
 * And get at them directly, when that should be necessary.
 */
#define	D_LINEARRAY(diff, filenum)	\
  ((diff)->lines[filenum])
#define	D_LENARRAY(diff, filenum)	\
  ((diff)->lengths[filenum])

/*
 * Next block.
 */
#define	D_NEXT(diff)	((diff)->next)

/*
 * Access the type of a diff3 block.
 */
#define	D3_TYPE(diff)	((diff)->correspond)

/*
 * Line mappings based on diffs.  The first maps off the top of the
 * diff, the second off of the bottom.
 */
#define	D_HIGH_MAPLINE(diff, fromfile, tofile, lineno)	\
  ((lineno)						\
   - D_HIGHLINE ((diff), (fromfile))			\
   + D_HIGHLINE ((diff), (tofile)))

#define	D_LOW_MAPLINE(diff, fromfile, tofile, lineno)	\
  ((lineno)						\
   - D_LOWLINE ((diff), (fromfile))			\
   + D_LOWLINE ((diff), (tofile)))

/*
 * General memory allocation function.
 */
#define	ALLOCATE(number, type)	\
  (type *) xmalloc ((number) * sizeof (type))

/* Options variables for flags set on command line.  */

/* If nonzero, treat all files as text files, never as binary.  */
static int always_text;

/* If nonzero, write out an ed script instead of the standard diff3 format.  */
static int edscript;

/* If nonzero, in the case of overlapping diffs (type DIFF_ALL),
   preserve the lines which would normally be deleted from
   file 1 with a special flagging mechanism.  */
static int flagging;

/* Number of lines to keep in identical prefix and suffix.  */
static int horizon_lines = 10;

/* Use a tab to align output lines (-T).  */
static int tab_align_flag;

/* If nonzero, do not output information for overlapping diffs.  */
static int simple_only;

/* If nonzero, do not output information for non-overlapping diffs.  */
static int overlap_only;

/* If nonzero, show information for DIFF_2ND diffs.  */
static int show_2nd;

/* If nonzero, include `:wq' at the end of the script
   to write out the file being edited.   */
static int finalwrite;

/* If nonzero, output a merged file.  */
static int merge;

extern char *diff_program_name;

static char *read_diff PARAMS((char const *, char const *, char **));
static char *scan_diff_line PARAMS((char *, char **, size_t *, char *, int));
static enum diff_type process_diff_control PARAMS((char **, struct diff_block *));
static int compare_line_list PARAMS((char * const[], size_t const[], char * const[], size_t const[], int));
static int copy_stringlist PARAMS((char * const[], size_t const[], char *[], size_t[], int));
static int dotlines PARAMS((FILE *, struct diff3_block *, int));
static int output_diff3_edscript PARAMS((FILE *, struct diff3_block *, int const[3], int const[3], char const *, char const *, char const *));
static int output_diff3_merge PARAMS((FILE *, FILE *, struct diff3_block *, int const[3], int const[3], char const *, char const *, char const *));
static size_t myread PARAMS((int, char *, size_t));
static struct diff3_block *create_diff3_block PARAMS((int, int, int, int, int, int));
static struct diff3_block *make_3way_diff PARAMS((struct diff_block *, struct diff_block *));
static struct diff3_block *reverse_diff3_blocklist PARAMS((struct diff3_block *));
static struct diff3_block *using_to_diff3_block PARAMS((struct diff_block *[2], struct diff_block *[2], int, int, struct diff3_block const *));
static struct diff_block *process_diff PARAMS((char const *, char const *, struct diff_block **, char **));
static void check_output PARAMS((FILE *));
static void diff3_fatal PARAMS((char const *));
static void output_diff3 PARAMS((FILE *, struct diff3_block *, int const[3], int const[3]));
static void diff3_perror_with_exit PARAMS((char const *));
static int try_help PARAMS((char const *));
static void undotlines PARAMS((FILE *, int, int, int));
static void usage PARAMS((void));
static void initialize_main PARAMS((int *, char ***));
static void free_diff_blocks PARAMS((struct diff_block *));
static void free_diff3_blocks PARAMS((struct diff3_block *));

/* Functions provided in libdiff.a or other external sources. */
int diff_run PARAMS((int, char **, char *));
VOID *xmalloc PARAMS((size_t));
VOID *xrealloc PARAMS((VOID *, size_t));
void perror_with_name PARAMS((char const *));
void diff_error PARAMS((char const *, char const *, char const *));

/* Permit non-local exits from diff3. */
static jmp_buf diff3_abort_buf;
#define DIFF3_ABORT(retval) longjmp(diff3_abort_buf, retval)

static struct option const longopts[] =
{
  {"text", 0, 0, 'a'},
  {"show-all", 0, 0, 'A'},
  {"ed", 0, 0, 'e'},
  {"show-overlap", 0, 0, 'E'},
  {"label", 1, 0, 'L'},
  {"merge", 0, 0, 'm'},
  {"initial-tab", 0, 0, 'T'},
  {"overlap-only", 0, 0, 'x'},
  {"easy-only", 0, 0, '3'},
  {"version", 0, 0, 'v'},
  {"help", 0, 0, 129},
  {0, 0, 0, 0}
};

/*
 * Main program.  Calls diff twice on two pairs of input files,
 * combines the two diffs, and outputs them.
 */
int
diff3_run (argc, argv, outfile)
     int argc;
     char **argv;
     char *outfile;
{
  int c, i;
  int mapping[3];
  int rev_mapping[3];
  int incompat = 0;
  int conflicts_found;
  int status;
  struct diff_block *thread0, *thread1, *last_block;
  char *content0, *content1;
  struct diff3_block *diff3;
  int tag_count = 0;
  char *tag_strings[3];
  char *commonname;
  char **file;
  struct stat statb;
  int optind_old;
  FILE *outstream;

  initialize_main (&argc, &argv);

  optind_old = optind;
  optind = 0;
  while ((c = getopt_long (argc, argv, "aeimvx3AEL:TX", longopts, 0)) != EOF)
    {
      switch (c)
	{
	case 'a':
	  always_text = 1;
	  break;
	case 'A':
	  show_2nd = 1;
	  flagging = 1;
	  incompat++;
	  break;
	case 'x':
	  overlap_only = 1;
	  incompat++;
	  break;
	case '3':
	  simple_only = 1;
	  incompat++;
	  break;
	case 'i':
	  finalwrite = 1;
	  break;
	case 'm':
	  merge = 1;
	  break;
	case 'X':
	  overlap_only = 1;
	  /* Falls through */
	case 'E':
	  flagging = 1;
	  /* Falls through */
	case 'e':
	  incompat++;
	  break;
	case 'T':
	  tab_align_flag = 1;
	  break;
	case 'v':
	  printf ("diff3 - GNU diffutils version %s\n", diff_version_string);
	  return 0;
	case 129:
	  usage ();
	  check_output (stdout);
	  return 0;
	case 'L':
	  /* Handle up to three -L options.  */
	  if (tag_count < 3)
	    {
	      tag_strings[tag_count++] = optarg;
	      break;
	    }
	  return try_help ("Too many labels were given.  The limit is 3.");
	default:
	  return try_help (0);
	}
    }

  edscript = incompat & ~merge;  /* -AeExX3 without -m implies ed script.  */
  show_2nd |= ~incompat & merge;  /* -m without -AeExX3 implies -A.  */
  flagging |= ~incompat & merge;

  if (incompat > 1  /* Ensure at most one of -AeExX3.  */
      || finalwrite & merge /* -i -m would rewrite input file.  */
      || (tag_count && ! flagging)) /* -L requires one of -AEX.  */
    return try_help ("incompatible options");

  if (argc - optind != 3)
    return try_help (argc - optind < 3 ? "missing operand" : "extra operand");

  file = &argv[optind];

  optind = optind_old;

  for (i = tag_count; i < 3; i++)
    tag_strings[i] = file[i];

  /* Always compare file1 to file2, even if file2 is "-".
     This is needed for -mAeExX3.  Using the file0 as
     the common file would produce wrong results, because if the
     file0-file1 diffs didn't line up with the file0-file2 diffs
     (which is entirely possible since we don't use diff's -n option),
     diff3 might report phantom changes from file1 to file2.  */

  if (strcmp (file[2], "-") == 0)
    {
      /* Sigh.  We've got standard input as the last arg.  We can't
	 call diff twice on stdin.  Use the middle arg as the common
	 file instead.  */
      if (strcmp (file[0], "-") == 0 || strcmp (file[1], "-") == 0)
        {
	  diff_error ("%s", "`-' specified for more than one input file", 0);
	  return 2;
        }
      mapping[0] = 0;
      mapping[1] = 2;
      mapping[2] = 1;
    }
  else
    {
      /* Normal, what you'd expect */
      mapping[0] = 0;
      mapping[1] = 1;
      mapping[2] = 2;
    }

  for (i = 0; i < 3; i++)
    rev_mapping[mapping[i]] = i;

  for (i = 0; i < 3; i++)
    if (strcmp (file[i], "-") != 0)
      {
	if (stat (file[i], &statb) < 0)
	  {
	    perror_with_name (file[i]);
	    return 2;
          }
	else if (S_ISDIR(statb.st_mode))
	  {
	    fprintf (stderr, "%s: %s: Is a directory\n",
		     diff_program_name, file[i]);
	    return 2;
	  }
      }

  if (outfile == NULL)
    outstream = stdout;
  else
    {
      outstream = fopen (outfile, "w");
      if (outstream == NULL)
        {
	  perror_with_name ("could not open output file");
	  return 2;
        }
    }

  /* Set the jump buffer, so that diff may abort execution without
     terminating the process. */
  status = setjmp (diff3_abort_buf);
  if (status != 0)
      return status;

  commonname = file[rev_mapping[FILEC]];
  thread1 = process_diff (file[rev_mapping[FILE1]], commonname, &last_block,
			  &content1);
  if (thread1)
    for (i = 0; i < 2; i++)
      {
	horizon_lines = max (horizon_lines, D_NUMLINES (thread1, i));
	horizon_lines = max (horizon_lines, D_NUMLINES (last_block, i));
      }
  thread0 = process_diff (file[rev_mapping[FILE0]], commonname, &last_block,
			  &content0);
  diff3 = make_3way_diff (thread0, thread1);
  if (edscript)
    conflicts_found
      = output_diff3_edscript (outstream, diff3, mapping, rev_mapping,
			       tag_strings[0], tag_strings[1], tag_strings[2]);
  else if (merge)
    {
      if (! freopen (file[rev_mapping[FILE0]], "r", stdin))
	diff3_perror_with_exit (file[rev_mapping[FILE0]]);
      conflicts_found
	= output_diff3_merge (stdin, outstream, diff3, mapping, rev_mapping,
			      tag_strings[0], tag_strings[1], tag_strings[2]);
      if (ferror (stdin))
	diff3_fatal ("read error");
    }
  else
    {
      output_diff3 (outstream, diff3, mapping, rev_mapping);
      conflicts_found = 0;
    }

  free(content0);
  free(content1);
  free_diff_blocks(thread0);
  free_diff_blocks(thread1);
  free_diff3_blocks(diff3);

  check_output (outstream);
  if (outstream != stdout)
    if (fclose (outstream) != 0)
	perror ("close error on output file");
  return conflicts_found;
}

static int
try_help (reason)
     char const *reason;
{
  if (reason)
    fprintf (stderr, "%s: %s\n", diff_program_name, reason);
  fprintf (stderr, "%s: Try `%s --help' for more information.\n",
	   diff_program_name, diff_program_name);
  return 2;
}

static void
check_output (stream)
     FILE *stream;
{
  if (ferror (stream) || fflush (stream) != 0)
    diff3_fatal ("write error");
}

/*
 * Explain, patiently and kindly, how to use this program.
 */
static void
usage ()
{
  printf ("Usage: %s [OPTION]... MYFILE OLDFILE YOURFILE\n\n", diff_program_name);

  printf ("%s", "\
  -e  --ed  Output unmerged changes from OLDFILE to YOURFILE into MYFILE.\n\
  -E  --show-overlap  Output unmerged changes, bracketing conflicts.\n\
  -A  --show-all  Output all changes, bracketing conflicts.\n\
  -x  --overlap-only  Output overlapping changes.\n\
  -X  Output overlapping changes, bracketing them.\n\
  -3  --easy-only  Output unmerged nonoverlapping changes.\n\n");
  printf ("%s", "\
  -m  --merge  Output merged file instead of ed script (default -A).\n\
  -L LABEL  --label=LABEL  Use LABEL instead of file name.\n\
  -i  Append `w' and `q' commands to ed scripts.\n\
  -a  --text  Treat all files as text.\n\
  -T  --initial-tab  Make tabs line up by prepending a tab.\n\n");
  printf ("%s", "\
  -v  --version  Output version info.\n\
  --help  Output this help.\n\n");
  printf ("If a FILE is `-', read standard input.\n");
}

/*
 * Routines that combine the two diffs together into one.  The
 * algorithm used follows:
 *
 *   File2 is shared in common between the two diffs.
 *   Diff02 is the diff between 0 and 2.
 *   Diff12 is the diff between 1 and 2.
 *
 *	1) Find the range for the first block in File2.
 *	    a) Take the lowest of the two ranges (in File2) in the two
 *	       current blocks (one from each diff) as being the low
 *	       water mark.  Assign the upper end of this block as
 *	       being the high water mark and move the current block up
 *	       one.  Mark the block just moved over as to be used.
 *	    b) Check the next block in the diff that the high water
 *	       mark is *not* from.
 *
 *	       *If* the high water mark is above
 *	       the low end of the range in that block,
 *
 *		   mark that block as to be used and move the current
 *		   block up.  Set the high water mark to the max of
 *		   the high end of this block and the current.  Repeat b.
 *
 *	 2) Find the corresponding ranges in File0 (from the blocks
 *	    in diff02; line per line outside of diffs) and in File1.
 *	    Create a diff3_block, reserving space as indicated by the ranges.
 *
 *	 3) Copy all of the pointers for file2 in.  At least for now,
 *	    do memcmp's between corresponding strings in the two diffs.
 *
 *	 4) Copy all of the pointers for file0 and 1 in.  Get what you
 *	    need from file2 (when there isn't a diff block, it's
 *	    identical to file2 within the range between diff blocks).
 *
 *	 5) If the diff blocks you used came from only one of the two
 *	    strings of diffs, then that file (i.e. the one other than
 *	    the common file in that diff) is the odd person out.  If you used
 *	    diff blocks from both sets, check to see if files 0 and 1 match:
 *
 *		Same number of lines?  If so, do a set of memcmp's (if a
 *	    memcmp matches; copy the pointer over; it'll be easier later
 *	    if you have to do any compares).  If they match, 0 & 1 are
 *	    the same.  If not, all three different.
 *
 *   Then you do it again, until you run out of blocks.
 *
 */

/*
 * This routine makes a three way diff (chain of diff3_block's) from two
 * two way diffs (chains of diff_block's).  It is assumed that each of
 * the two diffs passed are onto the same file (i.e. that each of the
 * diffs were made "to" the same file).  The three way diff pointer
 * returned will have numbering FILE0--the other file in diff02,
 * FILE1--the other file in diff12, and FILEC--the common file.
 */
static struct diff3_block *
make_3way_diff (thread0, thread1)
     struct diff_block *thread0, *thread1;
{
/*
 * This routine works on the two diffs passed to it as threads.
 * Thread number 0 is diff02, thread number 1 is diff12.  The USING
 * array is set to the base of the list of blocks to be used to
 * construct each block of the three way diff; if no blocks from a
 * particular thread are to be used, that element of the using array
 * is set to 0.  The elements LAST_USING array are set to the last
 * elements on each of the using lists.
 *
 * The HIGH_WATER_MARK is set to the highest line number in the common file
 * described in any of the diffs in either of the USING lists.  The
 * HIGH_WATER_THREAD names the thread.  Similarly the BASE_WATER_MARK
 * and BASE_WATER_THREAD describe the lowest line number in the common file
 * described in any of the diffs in either of the USING lists.  The
 * HIGH_WATER_DIFF is the diff from which the HIGH_WATER_MARK was
 * taken.
 *
 * The HIGH_WATER_DIFF should always be equal to LAST_USING
 * [HIGH_WATER_THREAD].  The OTHER_DIFF is the next diff to check for
 * higher water, and should always be equal to
 * CURRENT[HIGH_WATER_THREAD ^ 0x1].  The OTHER_THREAD is the thread
 * in which the OTHER_DIFF is, and hence should always be equal to
 * HIGH_WATER_THREAD ^ 0x1.
 *
 * The variable LAST_DIFF is kept set to the last diff block produced
 * by this routine, for line correspondence purposes between that diff
 * and the one currently being worked on.  It is initialized to
 * ZERO_DIFF before any blocks have been created.
 */

  struct diff_block
    *using[2],
    *last_using[2],
    *current[2];

  int
    high_water_mark;

  int
    high_water_thread,
    base_water_thread,
    other_thread;

  struct diff_block
    *high_water_diff,
    *other_diff;

  struct diff3_block
    *result,
    *tmpblock,
    **result_end;

  struct diff3_block const *last_diff3;

  static struct diff3_block const zero_diff3;

  /* Initialization */
  result = 0;
  result_end = &result;
  current[0] = thread0; current[1] = thread1;
  last_diff3 = &zero_diff3;

  /* Sniff up the threads until we reach the end */

  while (current[0] || current[1])
    {
      using[0] = using[1] = last_using[0] = last_using[1] = 0;

      /* Setup low and high water threads, diffs, and marks.  */
      if (!current[0])
	base_water_thread = 1;
      else if (!current[1])
	base_water_thread = 0;
      else
	base_water_thread =
	  (D_LOWLINE (current[0], FC) > D_LOWLINE (current[1], FC));

      high_water_thread = base_water_thread;

      high_water_diff = current[high_water_thread];

#if 0
      /* low and high waters start off same diff */
      base_water_mark = D_LOWLINE (high_water_diff, FC);
#endif

      high_water_mark = D_HIGHLINE (high_water_diff, FC);

      /* Make the diff you just got info from into the using class */
      using[high_water_thread]
	= last_using[high_water_thread]
	= high_water_diff;
      current[high_water_thread] = high_water_diff->next;
      last_using[high_water_thread]->next = 0;

      /* And mark the other diff */
      other_thread = high_water_thread ^ 0x1;
      other_diff = current[other_thread];

      /* Shuffle up the ladder, checking the other diff to see if it
	 needs to be incorporated.  */
      while (other_diff
	     && D_LOWLINE (other_diff, FC) <= high_water_mark + 1)
	{

	  /* Incorporate this diff into the using list.  Note that
	     this doesn't take it off the current list */
	  if (using[other_thread])
	    last_using[other_thread]->next = other_diff;
	  else
	    using[other_thread] = other_diff;
	  last_using[other_thread] = other_diff;

	  /* Take it off the current list.  Note that this following
	     code assumes that other_diff enters it equal to
	     current[high_water_thread ^ 0x1] */
	  current[other_thread] = current[other_thread]->next;
	  other_diff->next = 0;

	  /* Set the high_water stuff
	     If this comparison is equal, then this is the last pass
	     through this loop; since diff blocks within a given
	     thread cannot overlap, the high_water_mark will be
	     *below* the range_start of either of the next diffs.  */

	  if (high_water_mark < D_HIGHLINE (other_diff, FC))
	    {
	      high_water_thread ^= 1;
	      high_water_diff = other_diff;
	      high_water_mark = D_HIGHLINE (other_diff, FC);
	    }

	  /* Set the other diff */
	  other_thread = high_water_thread ^ 0x1;
	  other_diff = current[other_thread];
	}

      /* The using lists contain a list of all of the blocks to be
	 included in this diff3_block.  Create it.  */

      tmpblock = using_to_diff3_block (using, last_using,
				       base_water_thread, high_water_thread,
				       last_diff3);

      if (!tmpblock)
	diff3_fatal ("internal error: screwup in format of diff blocks");

      /* Put it on the list.  */
      *result_end = tmpblock;
      result_end = &tmpblock->next;

      /* Set up corresponding lines correctly.  */
      last_diff3 = tmpblock;
    }
  return result;
}

/*
 * using_to_diff3_block:
 *   This routine takes two lists of blocks (from two separate diff
 * threads) and puts them together into one diff3 block.
 * It then returns a pointer to this diff3 block or 0 for failure.
 *
 * All arguments besides using are for the convenience of the routine;
 * they could be derived from the using array.
 * LAST_USING is a pair of pointers to the last blocks in the using
 * structure.
 * LOW_THREAD and HIGH_THREAD tell which threads contain the lowest
 * and highest line numbers for File0.
 * last_diff3 contains the last diff produced in the calling routine.
 * This is used for lines mappings which would still be identical to
 * the state that diff ended in.
 *
 * A distinction should be made in this routine between the two diffs
 * that are part of a normal two diff block, and the three diffs that
 * are part of a diff3_block.
 */
static struct diff3_block *
using_to_diff3_block (using, last_using, low_thread, high_thread, last_diff3)
     struct diff_block
       *using[2],
       *last_using[2];
     int low_thread, high_thread;
     struct diff3_block const *last_diff3;
{
  int low[2], high[2];
  struct diff3_block *result;
  struct diff_block *ptr;
  int d, i;

  /* Find the range in the common file.  */
  int lowc = D_LOWLINE (using[low_thread], FC);
  int highc = D_HIGHLINE (last_using[high_thread], FC);

  /* Find the ranges in the other files.
     If using[d] is null, that means that the file to which that diff
     refers is equivalent to the common file over this range.  */

  for (d = 0; d < 2; d++)
    if (using[d])
      {
	low[d] = D_LOW_MAPLINE (using[d], FC, FO, lowc);
	high[d] = D_HIGH_MAPLINE (last_using[d], FC, FO, highc);
      }
    else
      {
	low[d] = D_HIGH_MAPLINE (last_diff3, FILEC, FILE0 + d, lowc);
	high[d] = D_HIGH_MAPLINE (last_diff3, FILEC, FILE0 + d, highc);
      }

  /* Create a block with the appropriate sizes */
  result = create_diff3_block (low[0], high[0], low[1], high[1], lowc, highc);

  /* Copy information for the common file.
     Return with a zero if any of the compares failed.  */

  for (d = 0; d < 2; d++)
    for (ptr = using[d]; ptr; ptr = D_NEXT (ptr))
      {
	int result_offset = D_LOWLINE (ptr, FC) - lowc;

	if (!copy_stringlist (D_LINEARRAY (ptr, FC),
			      D_LENARRAY (ptr, FC),
			      D_LINEARRAY (result, FILEC) + result_offset,
			      D_LENARRAY (result, FILEC) + result_offset,
			      D_NUMLINES (ptr, FC)))
	  return 0;
      }

  /* Copy information for file d.  First deal with anything that might be
     before the first diff.  */

  for (d = 0; d < 2; d++)
    {
      struct diff_block *u = using[d];
      int lo = low[d], hi = high[d];

      for (i = 0;
	   i + lo < (u ? D_LOWLINE (u, FO) : hi + 1);
	   i++)
	{
	  D_RELNUM (result, FILE0 + d, i) = D_RELNUM (result, FILEC, i);
	  D_RELLEN (result, FILE0 + d, i) = D_RELLEN (result, FILEC, i);
	}

      for (ptr = u; ptr; ptr = D_NEXT (ptr))
	{
	  int result_offset = D_LOWLINE (ptr, FO) - lo;
	  int linec;

	  if (!copy_stringlist (D_LINEARRAY (ptr, FO),
				D_LENARRAY (ptr, FO),
				D_LINEARRAY (result, FILE0 + d) + result_offset,
				D_LENARRAY (result, FILE0 + d) + result_offset,
				D_NUMLINES (ptr, FO)))
	    return 0;

	  /* Catch the lines between here and the next diff */
	  linec = D_HIGHLINE (ptr, FC) + 1 - lowc;
	  for (i = D_HIGHLINE (ptr, FO) + 1 - lo;
	       i < (D_NEXT (ptr) ? D_LOWLINE (D_NEXT (ptr), FO) : hi + 1) - lo;
	       i++)
	    {
	      D_RELNUM (result, FILE0 + d, i) = D_RELNUM (result, FILEC, linec);
	      D_RELLEN (result, FILE0 + d, i) = D_RELLEN (result, FILEC, linec);
	      linec++;
	    }
	}
    }

  /* Set correspond */
  if (!using[0])
    D3_TYPE (result) = DIFF_2ND;
  else if (!using[1])
    D3_TYPE (result) = DIFF_1ST;
  else
    {
      int nl0 = D_NUMLINES (result, FILE0);
      int nl1 = D_NUMLINES (result, FILE1);

      if (nl0 != nl1
	  || !compare_line_list (D_LINEARRAY (result, FILE0),
				 D_LENARRAY (result, FILE0),
				 D_LINEARRAY (result, FILE1),
				 D_LENARRAY (result, FILE1),
				 nl0))
	D3_TYPE (result) = DIFF_ALL;
      else
	D3_TYPE (result) = DIFF_3RD;
    }

  return result;
}

/*
 * This routine copies pointers from a list of strings to a different list
 * of strings.  If a spot in the second list is already filled, it
 * makes sure that it is filled with the same string; if not it
 * returns 0, the copy incomplete.
 * Upon successful completion of the copy, it returns 1.
 */
static int
copy_stringlist (fromptrs, fromlengths, toptrs, tolengths, copynum)
     char * const fromptrs[];
     char *toptrs[];
     size_t const fromlengths[];
     size_t tolengths[];
     int copynum;
{
  register char * const *f = fromptrs;
  register char **t = toptrs;
  register size_t const *fl = fromlengths;
  register size_t *tl = tolengths;

  while (copynum--)
    {
      if (*t)
	{ if (*fl != *tl || memcmp (*f, *t, *fl)) return 0; }
      else
	{ *t = *f ; *tl = *fl; }

      t++; f++; tl++; fl++;
    }
  return 1;
}

/*
 * Create a diff3_block, with ranges as specified in the arguments.
 * Allocate the arrays for the various pointers (and zero them) based
 * on the arguments passed.  Return the block as a result.
 */
static struct diff3_block *
create_diff3_block (low0, high0, low1, high1, low2, high2)
     register int low0, high0, low1, high1, low2, high2;
{
  struct diff3_block *result = ALLOCATE (1, struct diff3_block);
  int numlines;

  D3_TYPE (result) = ERROR;
  D_NEXT (result) = 0;

  /* Assign ranges */
  D_LOWLINE (result, FILE0) = low0;
  D_HIGHLINE (result, FILE0) = high0;
  D_LOWLINE (result, FILE1) = low1;
  D_HIGHLINE (result, FILE1) = high1;
  D_LOWLINE (result, FILE2) = low2;
  D_HIGHLINE (result, FILE2) = high2;

  /* Allocate and zero space */
  numlines = D_NUMLINES (result, FILE0);
  if (numlines)
    {
      D_LINEARRAY (result, FILE0) = ALLOCATE (numlines, char *);
      D_LENARRAY (result, FILE0) = ALLOCATE (numlines, size_t);
      bzero (D_LINEARRAY (result, FILE0), (numlines * sizeof (char *)));
      bzero (D_LENARRAY (result, FILE0), (numlines * sizeof (size_t)));
    }
  else
    {
      D_LINEARRAY (result, FILE0) = 0;
      D_LENARRAY (result, FILE0) = 0;
    }

  numlines = D_NUMLINES (result, FILE1);
  if (numlines)
    {
      D_LINEARRAY (result, FILE1) = ALLOCATE (numlines, char *);
      D_LENARRAY (result, FILE1) = ALLOCATE (numlines, size_t);
      bzero (D_LINEARRAY (result, FILE1), (numlines * sizeof (char *)));
      bzero (D_LENARRAY (result, FILE1), (numlines * sizeof (size_t)));
    }
  else
    {
      D_LINEARRAY (result, FILE1) = 0;
      D_LENARRAY (result, FILE1) = 0;
    }

  numlines = D_NUMLINES (result, FILE2);
  if (numlines)
    {
      D_LINEARRAY (result, FILE2) = ALLOCATE (numlines, char *);
      D_LENARRAY (result, FILE2) = ALLOCATE (numlines, size_t);
      bzero (D_LINEARRAY (result, FILE2), (numlines * sizeof (char *)));
      bzero (D_LENARRAY (result, FILE2), (numlines * sizeof (size_t)));
    }
  else
    {
      D_LINEARRAY (result, FILE2) = 0;
      D_LENARRAY (result, FILE2) = 0;
    }

  /* Return */
  return result;
}

/*
 * Compare two lists of lines of text.
 * Return 1 if they are equivalent, 0 if not.
 */
static int
compare_line_list (list1, lengths1, list2, lengths2, nl)
     char * const list1[], * const list2[];
     size_t const lengths1[], lengths2[];
     int nl;
{
  char
    * const *l1 = list1,
    * const *l2 = list2;
  size_t const
    *lgths1 = lengths1,
    *lgths2 = lengths2;

  while (nl--)
    if (!*l1 || !*l2 || *lgths1 != *lgths2++
	|| memcmp (*l1++, *l2++, *lgths1++))
      return 0;
  return 1;
}

/*
 * Routines to input and parse two way diffs.
 */

extern char **environ;

static struct diff_block *
process_diff (filea, fileb, last_block, diff_contents)
     char const *filea, *fileb;
     struct diff_block **last_block;
     char **diff_contents;
{
  char *diff_limit;
  char *scan_diff;
  enum diff_type dt;
  int i;
  struct diff_block *block_list, **block_list_end, *bptr;

  diff_limit = read_diff (filea, fileb, diff_contents);
  scan_diff = *diff_contents;
  block_list_end = &block_list;
  bptr = 0; /* Pacify `gcc -W'.  */

  while (scan_diff < diff_limit)
    {
      bptr = ALLOCATE (1, struct diff_block);
      bptr->lines[0] = bptr->lines[1] = 0;
      bptr->lengths[0] = bptr->lengths[1] = 0;

      dt = process_diff_control (&scan_diff, bptr);
      if (dt == ERROR || *scan_diff != '\n')
	{
	  fprintf (stderr, "%s: diff error: ", diff_program_name);
	  do
	    {
	      putc (*scan_diff, stderr);
	    }
	  while (*scan_diff++ != '\n');
	  DIFF3_ABORT (2);
	}
      scan_diff++;

      /* Force appropriate ranges to be null, if necessary */
      switch (dt)
	{
	case ADD:
	  bptr->ranges[0][0]++;
	  break;
	case DELETE:
	  bptr->ranges[1][0]++;
	  break;
	case CHANGE:
	  break;
	default:
	  diff3_fatal ("internal error: invalid diff type in process_diff");
	  break;
	}

      /* Allocate space for the pointers for the lines from filea, and
	 parcel them out among these pointers */
      if (dt != ADD)
	{
	  int numlines = D_NUMLINES (bptr, 0);
	  bptr->lines[0] = ALLOCATE (numlines, char *);
	  bptr->lengths[0] = ALLOCATE (numlines, size_t);
	  for (i = 0; i < numlines; i++)
	    scan_diff = scan_diff_line (scan_diff,
					&(bptr->lines[0][i]),
					&(bptr->lengths[0][i]),
					diff_limit,
					'<');
	}

      /* Get past the separator for changes */
      if (dt == CHANGE)
	{
	  if (strncmp (scan_diff, "---\n", 4))
	    diff3_fatal ("invalid diff format; invalid change separator");
	  scan_diff += 4;
	}

      /* Allocate space for the pointers for the lines from fileb, and
	 parcel them out among these pointers */
      if (dt != DELETE)
	{
	  int numlines = D_NUMLINES (bptr, 1);
	  bptr->lines[1] = ALLOCATE (numlines, char *);
	  bptr->lengths[1] = ALLOCATE (numlines, size_t);
	  for (i = 0; i < numlines; i++)
	    scan_diff = scan_diff_line (scan_diff,
					&(bptr->lines[1][i]),
					&(bptr->lengths[1][i]),
					diff_limit,
					'>');
	}

      /* Place this block on the blocklist.  */
      *block_list_end = bptr;
      block_list_end = &bptr->next;
    }

  *block_list_end = 0;
  *last_block = bptr;
  return block_list;
}

/*
 * This routine will parse a normal format diff control string.  It
 * returns the type of the diff (ERROR if the format is bad).  All of
 * the other important information is filled into to the structure
 * pointed to by db, and the string pointer (whose location is passed
 * to this routine) is updated to point beyond the end of the string
 * parsed.  Note that only the ranges in the diff_block will be set by
 * this routine.
 *
 * If some specific pair of numbers has been reduced to a single
 * number, then both corresponding numbers in the diff block are set
 * to that number.  In general these numbers are interpetted as ranges
 * inclusive, unless being used by the ADD or DELETE commands.  It is
 * assumed that these will be special cased in a superior routine.
 */

static enum diff_type
process_diff_control (string, db)
     char **string;
     struct diff_block *db;
{
  char *s = *string;
  int holdnum;
  enum diff_type type;

/* These macros are defined here because they can use variables
   defined in this function.  Don't try this at home kids, we're
   trained professionals!

   Also note that SKIPWHITE only recognizes tabs and spaces, and
   that READNUM can only read positive, integral numbers */

#define	SKIPWHITE(s)	{ while (*s == ' ' || *s == '\t') s++; }
#define	READNUM(s, num)	\
	{ unsigned char c = *s; if (!ISDIGIT (c)) return ERROR; holdnum = 0; \
	  do { holdnum = (c - '0' + holdnum * 10); }	\
	  while (ISDIGIT (c = *++s)); (num) = holdnum; }

  /* Read first set of digits */
  SKIPWHITE (s);
  READNUM (s, db->ranges[0][START]);

  /* Was that the only digit? */
  SKIPWHITE (s);
  if (*s == ',')
    {
      /* Get the next digit */
      s++;
      READNUM (s, db->ranges[0][END]);
    }
  else
    db->ranges[0][END] = db->ranges[0][START];

  /* Get the letter */
  SKIPWHITE (s);
  switch (*s)
    {
    case 'a':
      type = ADD;
      break;
    case 'c':
      type = CHANGE;
      break;
    case 'd':
      type = DELETE;
      break;
    default:
      return ERROR;			/* Bad format */
    }
  s++;				/* Past letter */

  /* Read second set of digits */
  SKIPWHITE (s);
  READNUM (s, db->ranges[1][START]);

  /* Was that the only digit? */
  SKIPWHITE (s);
  if (*s == ',')
    {
      /* Get the next digit */
      s++;
      READNUM (s, db->ranges[1][END]);
      SKIPWHITE (s);		/* To move to end */
    }
  else
    db->ranges[1][END] = db->ranges[1][START];

  *string = s;
  return type;
}

static char *
read_diff (filea, fileb, output_placement)
     char const *filea, *fileb;
     char **output_placement;
{
  char *diff_result;
  size_t bytes, current_chunk_size, total;
  int fd, wstatus;
  struct stat pipestat;

  /* 302 / 1000 is log10(2.0) rounded up.  Subtract 1 for the sign bit;
     add 1 for integer division truncation; add 1 more for a minus sign.  */
#define INT_STRLEN_BOUND(type) ((sizeof(type)*CHAR_BIT - 1) * 302 / 1000 + 2)

  char const *argv[7];
  char horizon_arg[17 + INT_STRLEN_BOUND (int)];
  char const **ap;
  char *diffout;

  ap = argv;
  *ap++ = "diff";
  if (always_text)
    *ap++ = "-a";
  sprintf (horizon_arg, "--horizon-lines=%d", horizon_lines);
  *ap++ = horizon_arg;
  *ap++ = "--";
  *ap++ = filea;
  *ap++ = fileb;
  *ap = 0;

  diffout = tmpnam(NULL);
  wstatus = diff_run (ap - argv, (char **) argv, diffout);
  if (wstatus == 2)
    diff3_fatal ("subsidiary diff failed");

  if (-1 == (fd = open (diffout, O_RDONLY)))
    diff3_fatal ("could not open temporary diff file");

  current_chunk_size = 8 * 1024;
  if (fstat (fd, &pipestat) == 0)
    current_chunk_size = max (current_chunk_size, STAT_BLOCKSIZE (pipestat));

  diff_result = xmalloc (current_chunk_size);
  total = 0;
  do {
    bytes = myread (fd,
		    diff_result + total,
		    current_chunk_size - total);
    total += bytes;
    if (total == current_chunk_size)
      {
	if (current_chunk_size < 2 * current_chunk_size)
	  current_chunk_size = 2 * current_chunk_size;
	else if (current_chunk_size < (size_t) -1)
	  current_chunk_size = (size_t) -1;
	else
	  diff3_fatal ("files are too large to fit into memory");
	diff_result = xrealloc (diff_result, (current_chunk_size *= 2));
      }
  } while (bytes);

  if (total != 0 && diff_result[total-1] != '\n')
    diff3_fatal ("invalid diff format; incomplete last line");

  *output_placement = diff_result;

  if (close (fd) != 0)
    diff3_perror_with_exit ("pipe close");
  unlink (diffout);

  return diff_result + total;
}


/*
 * Scan a regular diff line (consisting of > or <, followed by a
 * space, followed by text (including nulls) up to a newline.
 *
 * This next routine began life as a macro and many parameters in it
 * are used as call-by-reference values.
 */
static char *
scan_diff_line (scan_ptr, set_start, set_length, limit, leadingchar)
     char *scan_ptr, **set_start;
     size_t *set_length;
     char *limit;
     int leadingchar;
{
  char *line_ptr;

  if (!(scan_ptr[0] == leadingchar
	&& scan_ptr[1] == ' '))
    diff3_fatal ("invalid diff format; incorrect leading line chars");

  *set_start = line_ptr = scan_ptr + 2;
  while (*line_ptr++ != '\n')
    ;

  /* Include newline if the original line ended in a newline,
     or if an edit script is being generated.
     Copy any missing newline message to stderr if an edit script is being
     generated, because edit scripts cannot handle missing newlines.
     Return the beginning of the next line.  */
  *set_length = line_ptr - *set_start;
  if (line_ptr < limit && *line_ptr == '\\')
    {
      if (edscript)
	fprintf (stderr, "%s:", diff_program_name);
      else
	--*set_length;
      line_ptr++;
      do
	{
	  if (edscript)
	    putc (*line_ptr, stderr);
	}
      while (*line_ptr++ != '\n');
    }

  return line_ptr;
}

/*
 * This routine outputs a three way diff passed as a list of
 * diff3_block's.
 * The argument MAPPING is indexed by external file number (in the
 * argument list) and contains the internal file number (from the
 * diff passed).  This is important because the user expects his
 * outputs in terms of the argument list number, and the diff passed
 * may have been done slightly differently (if the last argument
 * was "-", for example).
 * REV_MAPPING is the inverse of MAPPING.
 */
static void
output_diff3 (outputfile, diff, mapping, rev_mapping)
     FILE *outputfile;
     struct diff3_block *diff;
     int const mapping[3], rev_mapping[3];
{
  int i;
  int oddoneout;
  char *cp;
  struct diff3_block *ptr;
  int line;
  size_t length;
  int dontprint;
  static int skew_increment[3] = { 2, 3, 1 }; /* 0==>2==>1==>3 */
  char const *line_prefix = tab_align_flag ? "\t" : "  ";

  for (ptr = diff; ptr; ptr = D_NEXT (ptr))
    {
      char x[2];

      switch (ptr->correspond)
	{
	case DIFF_ALL:
	  x[0] = '\0';
	  dontprint = 3;	/* Print them all */
	  oddoneout = 3;	/* Nobody's odder than anyone else */
	  break;
	case DIFF_1ST:
	case DIFF_2ND:
	case DIFF_3RD:
	  oddoneout = rev_mapping[(int) ptr->correspond - (int) DIFF_1ST];

	  x[0] = oddoneout + '1';
	  x[1] = '\0';
	  dontprint = oddoneout==0;
	  break;
	default:
	  diff3_fatal ("internal error: invalid diff type passed to output");
	}
      fprintf (outputfile, "====%s\n", x);

      /* Go 0, 2, 1 if the first and third outputs are equivalent.  */
      for (i = 0; i < 3;
	   i = (oddoneout == 1 ? skew_increment[i] : i + 1))
	{
	  int realfile = mapping[i];
	  int
	    lowt = D_LOWLINE (ptr, realfile),
	    hight = D_HIGHLINE (ptr, realfile);

	  fprintf (outputfile, "%d:", i + 1);
	  switch (lowt - hight)
	    {
	    case 1:
	      fprintf (outputfile, "%da\n", lowt - 1);
	      break;
	    case 0:
	      fprintf (outputfile, "%dc\n", lowt);
	      break;
	    default:
	      fprintf (outputfile, "%d,%dc\n", lowt, hight);
	      break;
	    }

	  if (i == dontprint) continue;

	  if (lowt <= hight)
	    {
	      line = 0;
	      do
		{
		  fprintf (outputfile, line_prefix);
		  cp = D_RELNUM (ptr, realfile, line);
		  length = D_RELLEN (ptr, realfile, line);
		  fwrite (cp, sizeof (char), length, outputfile);
		}
	      while (++line < hight - lowt + 1);
	      if (cp[length - 1] != '\n')
		fprintf (outputfile, "\n\\ No newline at end of file\n");
	    }
	}
    }
}


/*
 * Output to OUTPUTFILE the lines of B taken from FILENUM.
 * Double any initial '.'s; yield nonzero if any initial '.'s were doubled.
 */
static int
dotlines (outputfile, b, filenum)
     FILE *outputfile;
     struct diff3_block *b;
     int filenum;
{
  int i;
  int leading_dot = 0;

  for (i = 0;
       i < D_NUMLINES (b, filenum);
       i++)
    {
      char *line = D_RELNUM (b, filenum, i);
      if (line[0] == '.')
	{
	  leading_dot = 1;
	  fprintf (outputfile, ".");
	}
      fwrite (line, sizeof (char),
	      D_RELLEN (b, filenum, i), outputfile);
    }

  return leading_dot;
}

/*
 * Output to OUTPUTFILE a '.' line.  If LEADING_DOT is nonzero,
 * also output a command that removes initial '.'s
 * starting with line START and continuing for NUM lines.
 */
static void
undotlines (outputfile, leading_dot, start, num)
     FILE *outputfile;
     int leading_dot, start, num;
{
  fprintf (outputfile, ".\n");
  if (leading_dot)
    if (num == 1)
      fprintf (outputfile, "%ds/^\\.//\n", start);
    else
      fprintf (outputfile, "%d,%ds/^\\.//\n", start, start + num - 1);
}

/*
 * This routine outputs a diff3 set of blocks as an ed script.  This
 * script applies the changes between file's 2 & 3 to file 1.  It
 * takes the precise format of the ed script to be output from global
 * variables set during options processing.  Note that it does
 * destructive things to the set of diff3 blocks it is passed; it
 * reverses their order (this gets around the problems involved with
 * changing line numbers in an ed script).
 *
 * Note that this routine has the same problem of mapping as the last
 * one did; the variable MAPPING maps from file number according to
 * the argument list to file number according to the diff passed.  All
 * files listed below are in terms of the argument list.
 * REV_MAPPING is the inverse of MAPPING.
 *
 * The arguments FILE0, FILE1 and FILE2 are the strings to print
 * as the names of the three files.  These may be the actual names,
 * or may be the arguments specified with -L.
 *
 * Returns 1 if conflicts were found.
 */

static int
output_diff3_edscript (outputfile, diff, mapping, rev_mapping,
		       file0, file1, file2)
     FILE *outputfile;
     struct diff3_block *diff;
     int const mapping[3], rev_mapping[3];
     char const *file0, *file1, *file2;
{
  int leading_dot;
  int conflicts_found = 0, conflict;
  struct diff3_block *b;

  for (b = reverse_diff3_blocklist (diff); b; b = b->next)
    {
      /* Must do mapping correctly.  */
      enum diff_type type
	= ((b->correspond == DIFF_ALL) ?
	   DIFF_ALL :
	   ((enum diff_type)
	    (((int) DIFF_1ST)
	     + rev_mapping[(int) b->correspond - (int) DIFF_1ST])));

      /* If we aren't supposed to do this output block, skip it.  */
      switch (type)
	{
	default: continue;
	case DIFF_2ND: if (!show_2nd) continue; conflict = 1; break;
	case DIFF_3RD: if (overlap_only) continue; conflict = 0; break;
	case DIFF_ALL: if (simple_only) continue; conflict = flagging; break;
	}

      if (conflict)
	{
	  conflicts_found = 1;


	  /* Mark end of conflict.  */

	  fprintf (outputfile, "%da\n", D_HIGHLINE (b, mapping[FILE0]));
	  leading_dot = 0;
	  if (type == DIFF_ALL)
	    {
	      if (show_2nd)
		{
		  /* Append lines from FILE1.  */
		  fprintf (outputfile, "||||||| %s\n", file1);
		  leading_dot = dotlines (outputfile, b, mapping[FILE1]);
		}
	      /* Append lines from FILE2.  */
	      fprintf (outputfile, "=======\n");
	      leading_dot |= dotlines (outputfile, b, mapping[FILE2]);
	    }
	  fprintf (outputfile, ">>>>>>> %s\n", file2);
	  undotlines (outputfile, leading_dot,
		      D_HIGHLINE (b, mapping[FILE0]) + 2,
		      (D_NUMLINES (b, mapping[FILE1])
		       + D_NUMLINES (b, mapping[FILE2]) + 1));


	  /* Mark start of conflict.  */

	  fprintf (outputfile, "%da\n<<<<<<< %s\n",
		   D_LOWLINE (b, mapping[FILE0]) - 1,
		   type == DIFF_ALL ? file0 : file1);
	  leading_dot = 0;
	  if (type == DIFF_2ND)
	    {
	      /* Prepend lines from FILE1.  */
	      leading_dot = dotlines (outputfile, b, mapping[FILE1]);
	      fprintf (outputfile, "=======\n");
	    }
	  undotlines (outputfile, leading_dot,
		      D_LOWLINE (b, mapping[FILE0]) + 1,
		      D_NUMLINES (b, mapping[FILE1]));
	}
      else if (D_NUMLINES (b, mapping[FILE2]) == 0)
	/* Write out a delete */
	{
	  if (D_NUMLINES (b, mapping[FILE0]) == 1)
	    fprintf (outputfile, "%dd\n",
		     D_LOWLINE (b, mapping[FILE0]));
	  else
	    fprintf (outputfile, "%d,%dd\n",
		     D_LOWLINE (b, mapping[FILE0]),
		     D_HIGHLINE (b, mapping[FILE0]));
	}
      else
	/* Write out an add or change */
	{
	  switch (D_NUMLINES (b, mapping[FILE0]))
	    {
	    case 0:
	      fprintf (outputfile, "%da\n",
		       D_HIGHLINE (b, mapping[FILE0]));
	      break;
	    case 1:
	      fprintf (outputfile, "%dc\n",
		       D_HIGHLINE (b, mapping[FILE0]));
	      break;
	    default:
	      fprintf (outputfile, "%d,%dc\n",
		       D_LOWLINE (b, mapping[FILE0]),
		       D_HIGHLINE (b, mapping[FILE0]));
	      break;
	    }

	  undotlines (outputfile, dotlines (outputfile, b, mapping[FILE2]),
		      D_LOWLINE (b, mapping[FILE0]),
		      D_NUMLINES (b, mapping[FILE2]));
	}
    }
  if (finalwrite) fprintf (outputfile, "w\nq\n");
  return conflicts_found;
}

/*
 * Read from INFILE and output to OUTPUTFILE a set of diff3_ blocks DIFF
 * as a merged file.  This acts like 'ed file0 <[output_diff3_edscript]',
 * except that it works even for binary data or incomplete lines.
 *
 * As before, MAPPING maps from arg list file number to diff file number,
 * REV_MAPPING is its inverse,
 * and FILE0, FILE1, and FILE2 are the names of the files.
 *
 * Returns 1 if conflicts were found.
 */

static int
output_diff3_merge (infile, outputfile, diff, mapping, rev_mapping,
		    file0, file1, file2)
     FILE *infile, *outputfile;
     struct diff3_block *diff;
     int const mapping[3], rev_mapping[3];
     char const *file0, *file1, *file2;
{
  int c, i;
  int conflicts_found = 0, conflict;
  struct diff3_block *b;
  int linesread = 0;

  for (b = diff; b; b = b->next)
    {
      /* Must do mapping correctly.  */
      enum diff_type type
	= ((b->correspond == DIFF_ALL) ?
	   DIFF_ALL :
	   ((enum diff_type)
	    (((int) DIFF_1ST)
	     + rev_mapping[(int) b->correspond - (int) DIFF_1ST])));
      char const *format_2nd = "<<<<<<< %s\n";

      /* If we aren't supposed to do this output block, skip it.  */
      switch (type)
	{
	default: continue;
	case DIFF_2ND: if (!show_2nd) continue; conflict = 1; break;
	case DIFF_3RD: if (overlap_only) continue; conflict = 0; break;
	case DIFF_ALL: if (simple_only) continue; conflict = flagging;
	  format_2nd = "||||||| %s\n";
	  break;
	}

      /* Copy I lines from file 0.  */
      i = D_LOWLINE (b, FILE0) - linesread - 1;
      linesread += i;
      while (0 <= --i)
	do
	  {
	    c = getc (infile);
	    if (c == EOF)
	      if (ferror (infile))
		diff3_perror_with_exit ("input file");
	      else if (feof (infile))
		diff3_fatal ("input file shrank");
	    putc (c, outputfile);
	  }
	while (c != '\n');

      if (conflict)
	{
	  conflicts_found = 1;

	  if (type == DIFF_ALL)
	    {
	      /* Put in lines from FILE0 with bracket.  */
	      fprintf (outputfile, "<<<<<<< %s\n", file0);
	      for (i = 0;
		   i < D_NUMLINES (b, mapping[FILE0]);
		   i++)
		fwrite (D_RELNUM (b, mapping[FILE0], i), sizeof (char),
			D_RELLEN (b, mapping[FILE0], i), outputfile);
	    }

	  if (show_2nd)
	    {
	      /* Put in lines from FILE1 with bracket.  */
	      fprintf (outputfile, format_2nd, file1);
	      for (i = 0;
		   i < D_NUMLINES (b, mapping[FILE1]);
		   i++)
		fwrite (D_RELNUM (b, mapping[FILE1], i), sizeof (char),
			D_RELLEN (b, mapping[FILE1], i), outputfile);
	    }

	  fprintf (outputfile, "=======\n");
	}

      /* Put in lines from FILE2.  */
      for (i = 0;
	   i < D_NUMLINES (b, mapping[FILE2]);
	   i++)
	fwrite (D_RELNUM (b, mapping[FILE2], i), sizeof (char),
		D_RELLEN (b, mapping[FILE2], i), outputfile);

      if (conflict)
	fprintf (outputfile, ">>>>>>> %s\n", file2);

      /* Skip I lines in file 0.  */
      i = D_NUMLINES (b, FILE0);
      linesread += i;
      while (0 <= --i)
	while ((c = getc (infile)) != '\n')
	  if (c == EOF)
	    if (ferror (infile))
	      diff3_perror_with_exit ("input file");
	    else if (feof (infile))
	      {
		if (i || b->next)
		  diff3_fatal ("input file shrank");
		return conflicts_found;
	      }
    }
  /* Copy rest of common file.  */
  while ((c = getc (infile)) != EOF || !(ferror (infile) | feof (infile)))
    putc (c, outputfile);
  return conflicts_found;
}

/*
 * Reverse the order of the list of diff3 blocks.
 */
static struct diff3_block *
reverse_diff3_blocklist (diff)
     struct diff3_block *diff;
{
  register struct diff3_block *tmp, *next, *prev;

  for (tmp = diff, prev = 0;  tmp;  tmp = next)
    {
      next = tmp->next;
      tmp->next = prev;
      prev = tmp;
    }

  return prev;
}

static size_t
myread (fd, ptr, size)
     int fd;
     char *ptr;
     size_t size;
{
  size_t result = read (fd, ptr, size);
  if (result == -1)
    diff3_perror_with_exit ("read failed");
  return result;
}

static void
diff3_fatal (string)
     char const *string;
{
  fprintf (stderr, "%s: %s\n", diff_program_name, string);
  DIFF3_ABORT (2);
}

static void
diff3_perror_with_exit (string)
     char const *string;
{
  int e = errno;
  fprintf (stderr, "%s: ", diff_program_name);
  errno = e;
  perror (string);
  DIFF3_ABORT (2);
}

static void
initialize_main (argcp, argvp)
    int *argcp;
    char ***argvp;
{
  always_text = 0;
  edscript = 0;
  flagging = 0;
  horizon_lines = 10;
  tab_align_flag = 0;
  simple_only = 0;
  overlap_only = 0;
  show_2nd = 0;
  finalwrite = 0;
  merge = 0;
  diff_program_name = (*argvp)[0];
}

static void
free_diff_blocks(p)
    struct diff_block *p;
{
  register struct diff_block *next;

  while (p)
    {
      next = p->next;
      if (p->lines[0]) free(p->lines[0]);
      if (p->lines[1]) free(p->lines[1]);
      if (p->lengths[0]) free(p->lengths[0]);
      if (p->lengths[1]) free(p->lengths[1]);
      free(p);
      p = next;
    }
}

static void
free_diff3_blocks(p)
    struct diff3_block *p;
{
  register struct diff3_block *next;

  while (p)
    {
      next = p->next;
      if (p->lines[0]) free(p->lines[0]);
      if (p->lines[1]) free(p->lines[1]);
      if (p->lines[2]) free(p->lines[2]);
      if (p->lengths[0]) free(p->lengths[0]);
      if (p->lengths[1]) free(p->lengths[1]);
      if (p->lengths[2]) free(p->lengths[2]);
      free(p);
      p = next;
    }
}
