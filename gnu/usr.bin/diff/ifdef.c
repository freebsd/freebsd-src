/* #ifdef-format output routines for GNU DIFF.
   Copyright (C) 1989, 91, 92 Free Software Foundation, Inc.

This file is part of GNU DIFF.

GNU DIFF is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY.  No author or distributor
accepts responsibility to anyone for the consequences of using it
or for whether it serves any particular purpose or works at all,
unless he says so in writing.  Refer to the GNU DIFF General Public
License for full details.

Everyone is granted permission to copy, modify and redistribute
GNU DIFF, but only under the conditions described in the
GNU DIFF General Public License.   A copy of this license is
supposed to have been given to you along with GNU DIFF so you
can know your rights and responsibilities.  It should be in a
file named COPYING.  Among other things, the copyright notice
and this notice must be preserved on all copies.  */


#include "diff.h"

static void format_ifdef ();
static void print_ifdef_hunk ();
static void print_ifdef_lines ();
struct change *find_change ();

static int next_line;

/* Print the edit-script SCRIPT as a merged #ifdef file.  */

void
print_ifdef_script (script)
     struct change *script;
{
  next_line = - files[0].prefix_lines;
  print_script (script, find_change, print_ifdef_hunk);
  if (next_line < files[0].valid_lines)
    {
      begin_output ();
      format_ifdef (group_format[UNCHANGED], next_line, files[0].valid_lines,
		    0, -1);
    }
}

/* Print a hunk of an ifdef diff.
   This is a contiguous portion of a complete edit script,
   describing changes in consecutive lines.  */

static void
print_ifdef_hunk (hunk)
     struct change *hunk;
{
  int first0, last0, first1, last1, deletes, inserts;
  const char *format;

  /* Determine range of line numbers involved in each file.  */
  analyze_hunk (hunk, &first0, &last0, &first1, &last1, &deletes, &inserts);
  if (inserts)
    format = deletes ? group_format[CHANGED] : group_format[NEW];
  else if (deletes)
    format = group_format[OLD];
  else
    return;

  begin_output ();

  /* Print lines up to this change.  */
  if (next_line < first0)
    format_ifdef (group_format[UNCHANGED], next_line, first0, 0, -1);

  /* Print this change.  */
  next_line = last0 + 1;
  format_ifdef (format, first0, next_line, first1, last1 + 1);
}

/* Print a set of lines according to FORMAT.
   Lines BEG0 up to END0 are from the first file.
   If END1 is -1, then the second file's lines are identical to the first;
   otherwise, lines BEG1 up to END1 are from the second file.  */

static void
format_ifdef (format, beg0, end0, beg1, end1)
     const char *format;
     int beg0, end0, beg1, end1;
{
  register FILE *out = outfile;
  register char c;
  register const char *f = format;

  while ((c = *f++) != 0)
    {
      if (c == '%')
	switch ((c = *f++))
	  {
	  case 0:
	    return;

	  case '<':
	    /* Print lines deleted from first file.  */
	    print_ifdef_lines (line_format[OLD], &files[0], beg0, end0);
	    continue;

	  case '=':
	    /* Print common lines.  */
	    print_ifdef_lines (line_format[UNCHANGED], &files[0], beg0, end0);
	    continue;

	  case '>':
	    /* Print lines inserted from second file.  */
	    if (end1 == -1)
	      print_ifdef_lines (line_format[NEW], &files[0], beg0, end0);
	    else
	      print_ifdef_lines (line_format[NEW], &files[1], beg1, end1);
	    continue;

	  case '0':
	    c = 0;
	    break;

	  default:
	    break;
	  }
      putc (c, out);
  }
}

/* Use FORMAT to print each line of CURRENT starting with FROM
   and continuing up to UPTO.  */
static void
print_ifdef_lines (format, current, from, upto)
     const char *format;
     const struct file_data *current;
     int from, upto;
{
  const char * const *linbuf = current->linbuf;

  /* If possible, use a single fwrite; it's faster.  */
  if (!tab_expand_flag && strcmp (format, "%l\n") == 0)
    fwrite (linbuf[from], sizeof (char),
	    linbuf[upto] + (linbuf[upto][-1] != '\n') -  linbuf[from],
	    outfile);
  else if (!tab_expand_flag && strcmp (format, "%L") == 0)
    fwrite (linbuf[from], sizeof (char), linbuf[upto] -  linbuf[from], outfile);
  else
    for (;  from < upto;  from++)
      {
	register FILE *out = outfile;
	register char c;
	register const char *f = format;

	while ((c = *f++) != 0)
	  {
	    if (c == '%')
	      switch ((c = *f++))
		{
		case 0:
		  goto format_done;

		case 'l':
		  output_1_line (linbuf[from],
				 linbuf[from + 1]
				   - (linbuf[from + 1][-1] == '\n'), 0, 0);
		  continue;

		case 'L':
		  output_1_line (linbuf[from], linbuf[from + 1], 0, 0);
		  continue;

		case '0':
		  c = 0;
		  break;

		default:
		  break;
		}
	    putc (c, out);
	  }

      format_done:;
      }
}
