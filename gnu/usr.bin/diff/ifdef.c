/* #ifdef-format output routines for GNU DIFF.
   Copyright (C) 1989, 91, 92, 93 Free Software Foundation, Inc.

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

struct group
{
  struct file_data const *file;
  int from, upto; /* start and limit lines for this group of lines */
};

static char *format_group PARAMS((FILE *, char *, int, struct group const[]));
static char *scan_char_literal PARAMS((char *, int *));
static char *scan_printf_spec PARAMS((char *));
static int groups_letter_value PARAMS((struct group const[], int));
static void format_ifdef PARAMS((char *, int, int, int, int));
static void print_ifdef_hunk PARAMS((struct change *));
static void print_ifdef_lines PARAMS((FILE *, char *, struct group const *));

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
		    next_line - files[0].valid_lines + files[1].valid_lines,
		    files[1].valid_lines);
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
  char *format;

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
    format_ifdef (group_format[UNCHANGED], next_line, first0,
		  next_line - first0 + first1, first1);

  /* Print this change.  */
  next_line = last0 + 1;
  format_ifdef (format, first0, next_line, first1, last1 + 1);
}

/* Print a set of lines according to FORMAT.
   Lines BEG0 up to END0 are from the first file;
   lines BEG1 up to END1 are from the second file.  */

static void
format_ifdef (format, beg0, end0, beg1, end1)
     char *format;
     int beg0, end0, beg1, end1;
{
  struct group groups[2];

  groups[0].file = &files[0];
  groups[0].from = beg0;
  groups[0].upto = end0;
  groups[1].file = &files[1];
  groups[1].from = beg1;
  groups[1].upto = end1;
  format_group (outfile, format, '\0', groups);
}

/* Print to file OUT a set of lines according to FORMAT.
   The format ends at the first free instance of ENDCHAR.
   Yield the address of the terminating character.
   GROUPS specifies which lines to print.
   If OUT is zero, do not actually print anything; just scan the format.  */

static char *
format_group (out, format, endchar, groups)
     register FILE *out;
     char *format;
     int endchar;
     struct group const groups[];
{
  register char c;
  register char *f = format;

  while ((c = *f) != endchar && c != 0)
    {
      f++;
      if (c == '%')
	{
	  char *spec = f;
	  switch ((c = *f++))
	    {
	    case '%':
	      break;

	    case '(':
	      /* Print if-then-else format e.g. `%(n=1?thenpart:elsepart)'.  */
	      {
		int i, value[2];
		FILE *thenout, *elseout;

		for (i = 0; i < 2; i++)
		  {
		    unsigned char f0 = f[0];
		    if (isdigit (f0))
		      {
			value[i] = atoi (f);
			while (isdigit ((unsigned char) *++f))
			  continue;
		      }
		    else
		      {
			value[i] = groups_letter_value (groups, f0);
			if (value[i] < 0)
			  goto bad_format;
			f++;
		      }
		    if (*f++ != "=?"[i])
		      goto bad_format;
		  }
		if (value[0] == value[1])
		  thenout = out, elseout = 0;
		else
		  thenout = 0, elseout = out;
		f = format_group (thenout, f, ':', groups);
		if (*f)
		  {
		    f = format_group (elseout, f + 1, ')', groups);
		    if (*f)
		      f++;
		  }
	      }
	      continue;

	    case '<':
	      /* Print lines deleted from first file.  */
	      print_ifdef_lines (out, line_format[OLD], &groups[0]);
	      continue;

	    case '=':
	      /* Print common lines.  */
	      print_ifdef_lines (out, line_format[UNCHANGED], &groups[0]);
	      continue;

	    case '>':
	      /* Print lines inserted from second file.  */
	      print_ifdef_lines (out, line_format[NEW], &groups[1]);
	      continue;

	    default:
	      {
		int value;
		char *speclim;

		f = scan_printf_spec (spec);
		if (!f)
		  goto bad_format;
		speclim = f;
		c = *f++;
		switch (c)
		  {
		    case '\'':
		      f = scan_char_literal (f, &value);
		      if (!f)
			goto bad_format;
		      break;

		    default:
		      value = groups_letter_value (groups, c);
		      if (value < 0)
			goto bad_format;
		      break;
		  }
		if (out)
		  {
		    /* Temporarily replace e.g. "%3dnx" with "%3d\0x".  */
		    *speclim = 0;
		    fprintf (out, spec - 1, value);
		    /* Undo the temporary replacement.  */
		    *speclim = c;
		  }
	      }
	      continue;

	    bad_format:
	      c = '%';
	      f = spec;
	      break;
	    }
	}
      if (out)
	putc (c, out);
    }
  return f;
}

/* For the line group pair G, return the number corresponding to LETTER.
   Return -1 if LETTER is not a group format letter.  */
static int
groups_letter_value (g, letter)
     struct group const g[];
     int letter;
{
  if (isupper (letter))
    {
      g++;
      letter = tolower (letter);
    }
  switch (letter)
    {
      case 'e': return translate_line_number (g->file, g->from) - 1;
      case 'f': return translate_line_number (g->file, g->from);
      case 'l': return translate_line_number (g->file, g->upto) - 1;
      case 'm': return translate_line_number (g->file, g->upto);
      case 'n': return g->upto - g->from;
      default: return -1;
    }
}

/* Print to file OUT, using FORMAT to print the line group GROUP.
   But do nothing if OUT is zero.  */
static void
print_ifdef_lines (out, format, group)
     register FILE *out;
     char *format;
     struct group const *group;
{
  struct file_data const *file = group->file;
  char const * const *linbuf = file->linbuf;
  int from = group->from, upto = group->upto;

  if (!out)
    return;

  /* If possible, use a single fwrite; it's faster.  */
  if (!tab_expand_flag && format[0] == '%')
    {
      if (format[1] == 'l' && format[2] == '\n' && !format[3])
	{
	  fwrite (linbuf[from], sizeof (char),
		  linbuf[upto] + (linbuf[upto][-1] != '\n') -  linbuf[from],
		  out);
	  return;
	}
      if (format[1] == 'L' && !format[2])
	{
	  fwrite (linbuf[from], sizeof (char),
		  linbuf[upto] -  linbuf[from], out);
	  return;
	}
    }

  for (;  from < upto;  from++)
    {
      register char c;
      register char *f = format;

      while ((c = *f++) != 0)
	{
	  if (c == '%')
	    {
	      char *spec = f;
	      switch ((c = *f++))
		{
		case '%':
		  break;

		case 'l':
		  output_1_line (linbuf[from],
				 linbuf[from + 1]
				   - (linbuf[from + 1][-1] == '\n'), 0, 0);
		  continue;

		case 'L':
		  output_1_line (linbuf[from], linbuf[from + 1], 0, 0);
		  continue;

		default:
		  {
		    int value;
		    char *speclim;

		    f = scan_printf_spec (spec);
		    if (!f)
		      goto bad_format;
		    speclim = f;
		    c = *f++;
		    switch (c)
		      {
			case '\'':
			  f = scan_char_literal (f, &value);
			  if (!f)
			    goto bad_format;
			  break;

		        case 'n':
			  value = translate_line_number (file, from);
			  break;
			
			default:
			  goto bad_format;
		      }
		    /* Temporarily replace e.g. "%3dnx" with "%3d\0x".  */
		    *speclim = 0;
		    fprintf (out, spec - 1, value);
		    /* Undo the temporary replacement.  */
		    *speclim = c;
		  }
		  continue;

		bad_format:
		  c = '%';
		  f = spec;
		  break;
		}
	    }
	  putc (c, out);
	}
    }
}

/* Scan the character literal represented in the string LIT; LIT points just
   after the initial apostrophe.  Put the literal's value into *INTPTR.
   Yield the address of the first character after the closing apostrophe,
   or zero if the literal is ill-formed.  */
static char *
scan_char_literal (lit, intptr)
     char *lit;
     int *intptr;
{
  register char *p = lit;
  int value, digits;
  char c = *p++;

  switch (c)
    {
      case 0:
      case '\'':
	return 0;

      case '\\':
	value = 0;
	while ((c = *p++) != '\'')
	  {
	    unsigned digit = c - '0';
	    if (8 <= digit)
	      return 0;
	    value = 8 * value + digit;
	  }
	digits = p - lit - 2;
	if (! (1 <= digits && digits <= 3))
	  return 0;
	break;

      default:
	value = c;
	if (*p++ != '\'')
	  return 0;
	break;
    }
  *intptr = value;
  return p;
}

/* Scan optional printf-style SPEC of the form `-*[0-9]*(.[0-9]*)?[cdoxX]'.
   Return the address of the character following SPEC, or zero if failure.  */
static char *
scan_printf_spec (spec)
     register char *spec;
{
  register unsigned char c;

  while ((c = *spec++) == '-')
    continue;
  while (isdigit (c))
    c = *spec++;
  if (c == '.')
    while (isdigit (c = *spec++))
      continue;
  switch (c)
    {
      case 'c': case 'd': case 'o': case 'x': case 'X':
	return spec;

      default:
	return 0;
    }
}
