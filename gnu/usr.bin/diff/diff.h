/* Shared definitions for GNU DIFF
   Copyright (C) 1988, 89, 91, 92 Free Software Foundation, Inc.

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

#include <ctype.h>
#include <stdio.h>
#include "system.h"
#include "regex.h"

#ifndef PR_FILE_NAME
#define PR_FILE_NAME "/bin/pr"
#endif

/* Character classes.  */
extern const char textchar[];

/* Is_space is a little broader than ctype.h's isspace,
   because it also includes backspace and no-break space.  */
#define Is_space(c) (textchar[c] & 2)

#define TAB_WIDTH 8

/* Variables for command line options */

#ifndef GDIFF_MAIN
#define EXTERN extern
#else
#define EXTERN
#endif

enum output_style {
  /* Default output style.  */
  OUTPUT_NORMAL,
  /* Output the differences with lines of context before and after (-c).  */
  OUTPUT_CONTEXT,
  /* Output the differences in a unified context diff format (-u). */
  OUTPUT_UNIFIED,
  /* Output the differences as commands suitable for `ed' (-e).  */
  OUTPUT_ED,
  /* Output the diff as a forward ed script (-f).  */
  OUTPUT_FORWARD_ED,
  /* Like -f, but output a count of changed lines in each "command" (-n). */
  OUTPUT_RCS,
  /* Output merged #ifdef'd file (-D).  */
  OUTPUT_IFDEF,
  /* Output sdiff style (-y).  */
  OUTPUT_SDIFF
};

/* True for output styles that are robust,
   i.e. can handle a file that ends in a non-newline.  */
#define ROBUST_OUTPUT_STYLE(S) ((S) != OUTPUT_ED && (S) != OUTPUT_FORWARD_ED)

EXTERN enum output_style output_style;

/* Nonzero if output cannot be generated for identical files.  */
EXTERN int no_diff_means_no_output;

/* Number of lines of context to show in each set of diffs.
   This is zero when context is not to be shown.  */
EXTERN int      context;

/* Consider all files as text files (-a).
   Don't interpret codes over 0177 as implying a "binary file".  */
EXTERN int	always_text_flag;

/* Number of lines to keep in identical prefix and suffix.  */
EXTERN int      horizon_lines;

/* Ignore changes in horizontal whitespace (-b).  */
EXTERN int      ignore_space_change_flag;

/* Ignore all horizontal whitespace (-w).  */
EXTERN int      ignore_all_space_flag;

/* Ignore changes that affect only blank lines (-B).  */
EXTERN int      ignore_blank_lines_flag;

/* 1 if lines may match even if their lengths are different.
   This depends on various options.  */
EXTERN int      length_varies;

/* Ignore differences in case of letters (-i).  */
EXTERN int      ignore_case_flag;

/* File labels for `-c' output headers (-L).  */
EXTERN char *file_label[2];

struct regexp_list
{
  struct re_pattern_buffer buf;
  struct regexp_list *next;
};

/* Regexp to identify function-header lines (-F).  */
EXTERN struct regexp_list *function_regexp_list;

/* Ignore changes that affect only lines matching this regexp (-I).  */
EXTERN struct regexp_list *ignore_regexp_list;

/* Say only whether files differ, not how (-q).  */
EXTERN int 	no_details_flag;

/* Report files compared that match (-s).
   Normally nothing is output when that happens.  */
EXTERN int      print_file_same_flag;

/* character that ends a line.  Currently this is always `\n'.  */
EXTERN char     line_end_char;

/* Output the differences with exactly 8 columns added to each line
   so that any tabs in the text line up properly (-T).  */
EXTERN int	tab_align_flag;

/* Expand tabs in the output so the text lines up properly
   despite the characters added to the front of each line (-t).  */
EXTERN int	tab_expand_flag;

/* In directory comparison, specify file to start with (-S).
   All file names less than this name are ignored.  */
EXTERN char	*dir_start_file;

/* If a file is new (appears in only one dir)
   include its entire contents (-N).
   Then `patch' would create the file with appropriate contents.  */
EXTERN int	entire_new_file_flag;

/* If a file is new (appears in only the second dir)
   include its entire contents (-P).
   Then `patch' would create the file with appropriate contents.  */
EXTERN int	unidirectional_new_file_flag;

/* Pipe each file's output through pr (-l).  */
EXTERN int	paginate_flag;

enum line_class {
  /* Lines taken from just the first file.  */
  OLD,
  /* Lines taken from just the second file.  */
  NEW,
  /* Lines common to both files.  */
  UNCHANGED,
  /* A hunk containing both old and new lines (line groups only).  */
  CHANGED
};

/* Line group formats for old, new, unchanged, and changed groups.  */
EXTERN const char *group_format[CHANGED + 1];

/* Line formats for old, new, and unchanged lines.  */
EXTERN const char *line_format[UNCHANGED + 1];

/* If using OUTPUT_SDIFF print extra information to help the sdiff filter. */
EXTERN int sdiff_help_sdiff;  

/* Tell OUTPUT_SDIFF to show only the left version of common lines. */
EXTERN int sdiff_left_only;

/* Tell OUTPUT_SDIFF to not show common lines. */
EXTERN int sdiff_skip_common_lines;

/* The half line width and column 2 offset for OUTPUT_SDIFF.  */
EXTERN unsigned sdiff_half_width;
EXTERN unsigned sdiff_column2_offset;

/* String containing all the command options diff received,
   with spaces between and at the beginning but none at the end.
   If there were no options given, this string is empty.  */
EXTERN char *	switch_string;

/* Nonzero means use heuristics for better speed.  */
EXTERN int	heuristic;

/* Name of program the user invoked (for error messages).  */
EXTERN char *	program;

/* The result of comparison is an "edit script": a chain of `struct change'.
   Each `struct change' represents one place where some lines are deleted
   and some are inserted.
   
   LINE0 and LINE1 are the first affected lines in the two files (origin 0).
   DELETED is the number of lines deleted here from file 0.
   INSERTED is the number of lines inserted here in file 1.

   If DELETED is 0 then LINE0 is the number of the line before
   which the insertion was done; vice versa for INSERTED and LINE1.  */

struct change
{
  struct change *link;		/* Previous or next edit command  */
  int inserted;			/* # lines of file 1 changed here.  */
  int deleted;			/* # lines of file 0 changed here.  */
  int line0;			/* Line number of 1st deleted line.  */
  int line1;			/* Line number of 1st inserted line.  */
  char ignore;			/* Flag used in context.c */
};

/* Structures that describe the input files.  */

/* Data on one input file being compared.  */

struct file_data {
    int             desc;	/* File descriptor  */
    char           *name;	/* File name  */
    struct stat     stat;	/* File status from fstat()  */
    int             dir_p;	/* nonzero if file is a directory  */

    /* Buffer in which text of file is read.  */
    char *	    buffer;
    /* Allocated size of buffer.  */
    int		    bufsize;
    /* Number of valid characters now in the buffer. */
    int		    buffered_chars;

    /* Array of pointers to lines in the file.  */
    const char **linbuf;

    /* linbuf_base <= buffered_lines <= valid_lines <= alloc_lines.
       linebuf[linbuf_base ... buffered_lines - 1] are possibly differing.
       linebuf[linbuf_base ... valid_lines - 1] contain valid data.
       linebuf[linbuf_base ... alloc_lines - 1] are allocated.  */
    int linbuf_base, buffered_lines, valid_lines, alloc_lines;

    /* Pointer to end of prefix of this file to ignore when hashing. */
    const char *prefix_end;

    /* Count of lines in the prefix.
       There are this many lines in the file before linbuf[0].  */
    int prefix_lines;

    /* Pointer to start of suffix of this file to ignore when hashing. */
    const char *suffix_begin;

    /* Vector, indexed by line number, containing an equivalence code for
       each line.  It is this vector that is actually compared with that
       of another file to generate differences. */
    int		   *equivs;

    /* Vector, like the previous one except that
       the elements for discarded lines have been squeezed out.  */
    int		   *undiscarded;

    /* Vector mapping virtual line numbers (not counting discarded lines)
       to real ones (counting those lines).  Both are origin-0.  */
    int		   *realindexes;

    /* Total number of nondiscarded lines. */
    int		    nondiscarded_lines;

    /* Vector, indexed by real origin-0 line number,
       containing 1 for a line that is an insertion or a deletion.
       The results of comparison are stored here.  */
    char	   *changed_flag;

    /* 1 if file ends in a line with no final newline. */
    int		    missing_newline;

    /* 1 more than the maximum equivalence value used for this or its
       sibling file. */
    int equiv_max;
};

/* Describe the two files currently being compared.  */

EXTERN struct file_data files[2];

/* Queue up one-line messages to be printed at the end,
   when -l is specified.  Each message is recorded with a `struct msg'.  */

struct msg
{
  struct msg *next;
  char *format;
  char *arg1;
  char *arg2;
};

/* Head of the chain of queues messages.  */

EXTERN struct msg *msg_chain;

/* Tail of the chain of queues messages.  */

EXTERN struct msg *msg_chain_end;

/* Stdio stream to output diffs to.  */

EXTERN FILE *outfile;

/* Declare various functions.  */

#if __STDC__
#define VOID void
#else
#define VOID char
#endif
VOID *xmalloc ();
VOID *xrealloc ();
char *concat ();

int excluded_filename ();
int sip ();

struct change *find_change ();

void analyze_hunk ();
void begin_output ();
void error ();
void fatal ();
void message ();
void output_1_line ();
void perror_with_name ();
void pfatal_with_name ();
void print_1_line ();
void print_context_header ();
void print_message_queue ();
void print_number_range ();
void print_script ();
void slurp ();
void translate_range ();
