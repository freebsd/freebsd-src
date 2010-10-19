/* basic_blocks.c  -  Basic-block level related code: reading/writing
   of basic-block info to/from gmon.out; computing and formatting of
   basic-block related statistics.

   Copyright 1999, 2000, 2001, 2002, 2004, 2005
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
   Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA
   02110-1301, USA.  */

#include "libiberty.h"
#include "gprof.h"
#include "basic_blocks.h"
#include "corefile.h"
#include "gmon_io.h"
#include "gmon_out.h"
#include "search_list.h"
#include "source.h"
#include "symtab.h"
#include "sym_ids.h"

static int cmp_bb (const PTR, const PTR);
static int cmp_ncalls (const PTR, const PTR);
static void fskip_string (FILE *);
static void annotate_with_count (char *, unsigned int, int, PTR);

/* Default option values:  */
bfd_boolean bb_annotate_all_lines = FALSE;
unsigned long bb_min_calls = 1;
int bb_table_length = 10;

/* Variables used to compute annotated source listing stats:  */
static long num_executable_lines;
static long num_lines_executed;


/* Helper for sorting.  Compares two symbols and returns result
   such that sorting will be increasing according to filename, line
   number, and address (in that order).  */

static int
cmp_bb (const PTR lp, const PTR rp)
{
  int r;
  const Sym *left = *(const Sym **) lp;
  const Sym *right = *(const Sym **) rp;

  if (left->file && right->file)
    {
      r = strcmp (left->file->name, right->file->name);

      if (r)
	return r;

      if (left->line_num != right->line_num)
	return left->line_num - right->line_num;
    }

  if (left->addr < right->addr)
    return -1;
  else if (left->addr > right->addr)
    return 1;
  else
    return 0;
}


/* Helper for sorting.  Order basic blocks in decreasing number of
   calls, ties are broken in increasing order of line numbers.  */
static int
cmp_ncalls (const PTR lp, const PTR rp)
{
  const Sym *left = *(const Sym **) lp;
  const Sym *right = *(const Sym **) rp;

  if (!left)
    return 1;
  else if (!right)
    return -1;

  if (left->ncalls < right->ncalls)
    return 1;
  else if (left->ncalls > right->ncalls)
    return -1;

  return left->line_num - right->line_num;
}

/* Skip over variable length string.  */
static void
fskip_string (FILE *fp)
{
  int ch;

  while ((ch = fgetc (fp)) != EOF)
    {
      if (ch == '\0')
	break;
    }
}

/* Read a basic-block record from file IFP.  FILENAME is the name
   of file IFP and is provided for formatting error-messages only.  */

void
bb_read_rec (FILE *ifp, const char *filename)
{
  unsigned int nblocks, b;
  bfd_vma addr, ncalls;
  Sym *sym;

  if (gmon_io_read_32 (ifp, &nblocks))
    {
      fprintf (stderr, _("%s: %s: unexpected end of file\n"),
	       whoami, filename);
      done (1);
    }

  nblocks = bfd_get_32 (core_bfd, (bfd_byte *) & nblocks);
  if (gmon_file_version == 0)
    fskip_string (ifp);

  for (b = 0; b < nblocks; ++b)
    {
      if (gmon_file_version == 0)
	{
	  int line_num;

	  /* Version 0 had lots of extra stuff that we don't
	     care about anymore.  */
	  if ((fread (&ncalls, sizeof (ncalls), 1, ifp) != 1)
	      || (fread (&addr, sizeof (addr), 1, ifp) != 1)
	      || (fskip_string (ifp), FALSE)
	      || (fskip_string (ifp), FALSE)
	      || (fread (&line_num, sizeof (line_num), 1, ifp) != 1))
	    {
	      perror (filename);
	      done (1);
	    }
	}
      else if (gmon_io_read_vma (ifp, &addr)
	       || gmon_io_read_vma (ifp, &ncalls))
	{
	  perror (filename);
	  done (1);
	}

      /* Basic-block execution counts are meaningful only if we're
	 profiling at the line-by-line level:  */
      if (line_granularity)
	{
	  sym = sym_lookup (&symtab, addr);

	  if (sym)
	    {
	      int i;

	      DBG (BBDEBUG,
		   printf ("[bb_read_rec] 0x%lx->0x%lx (%s:%d) cnt=%lu\n",
			   (unsigned long) addr, (unsigned long) sym->addr,
			   sym->name, sym->line_num, (unsigned long) ncalls));

	      for (i = 0; i < NBBS; i++)
		{
		  if (! sym->bb_addr[i] || sym->bb_addr[i] == addr)
		    {
		      sym->bb_addr[i] = addr;
		      sym->bb_calls[i] += ncalls;
		      break;
		    }
		}
	    }
	}
      else
	{
	  static bfd_boolean user_warned = FALSE;

	  if (!user_warned)
	    {
	      user_warned = TRUE;
	      fprintf (stderr,
  _("%s: warning: ignoring basic-block exec counts (use -l or --line)\n"),
		       whoami);
	    }
	}
    }
  return;
}

/* Write all basic-blocks with non-zero counts to file OFP.  FILENAME
   is the name of OFP and is provided for producing error-messages
   only.  */
void
bb_write_blocks (FILE *ofp, const char *filename)
{
  unsigned int nblocks = 0;
  Sym *sym;
  int i;

  /* Count how many non-zero blocks with have:  */
  for (sym = symtab.base; sym < symtab.limit; ++sym)
    {
      for (i = 0; i < NBBS && sym->bb_addr[i]; i++)
	;
      nblocks += i;
    }

  /* Write header:  */
  if (gmon_io_write_8 (ofp, GMON_TAG_BB_COUNT)
      || gmon_io_write_32 (ofp, nblocks))
    {
      perror (filename);
      done (1);
    }

  /* Write counts:  */
  for (sym = symtab.base; sym < symtab.limit; ++sym)
    {
      for (i = 0; i < NBBS && sym->bb_addr[i]; i++)
	{
	  if (gmon_io_write_vma (ofp, sym->bb_addr[i])
	      || gmon_io_write_vma (ofp, (bfd_vma) sym->bb_calls[i]))
	    {
	      perror (filename);
	      done (1);
	    }
	}
    }
}

/* Output basic-block statistics in a format that is easily parseable.
   Current the format is:

	<filename>:<line-number>: (<function-name>:<bb-addr): <ncalls>  */

void
print_exec_counts ()
{
  Sym **sorted_bbs, *sym;
  unsigned int i, j, len;

  if (first_output)
    first_output = FALSE;
  else
    printf ("\f\n");

  /* Sort basic-blocks according to function name and line number:  */
  sorted_bbs = (Sym **) xmalloc (symtab.len * sizeof (sorted_bbs[0]));
  len = 0;

  for (sym = symtab.base; sym < symtab.limit; ++sym)
    {
      /* Accept symbol if it's in the INCL_EXEC table
	 or there is no INCL_EXEC table
	 and it does not appear in the EXCL_EXEC table.  */
      if (sym_lookup (&syms[INCL_EXEC], sym->addr)
	  || (syms[INCL_EXEC].len == 0
	      && !sym_lookup (&syms[EXCL_EXEC], sym->addr)))
	{
	  sorted_bbs[len++] = sym;
	}
    }

  qsort (sorted_bbs, len, sizeof (sorted_bbs[0]), cmp_bb);

  /* Output basic-blocks:  */

  for (i = 0; i < len; ++i)
    {
      sym = sorted_bbs [i];
      
      if (sym->ncalls > 0 || ! ignore_zeros)
	{
	  /* FIXME: This only works if bfd_vma is unsigned long.  */
	  printf (_("%s:%d: (%s:0x%lx) %lu executions\n"),
		  sym->file ? sym->file->name : _("<unknown>"), sym->line_num,
		  sym->name, (unsigned long) sym->addr, sym->ncalls);
	}

      for (j = 0; j < NBBS && sym->bb_addr[j]; j ++)
	{
	  if (sym->bb_calls[j] > 0 || ! ignore_zeros)
	    {
	      /* FIXME: This only works if bfd_vma is unsigned long.  */
	      printf (_("%s:%d: (%s:0x%lx) %lu executions\n"),
		      sym->file ? sym->file->name : _("<unknown>"), sym->line_num,
		      sym->name, (unsigned long) sym->bb_addr[j],
		      sym->bb_calls[j]);
	    }
	}
    }
  free (sorted_bbs);
}

/* Helper for bb_annotated_source: format annotation containing
   number of line executions.  Depends on being called on each
   line of a file in sequential order.

   Global variable bb_annotate_all_lines enables execution count
   compression (counts are supressed if identical to the last one)
   and prints counts on all executed lines.  Otherwise, print
   all basic-block execution counts exactly once on the line
   that starts the basic-block.  */

static void
annotate_with_count (char *buf, unsigned int width, int line_num, PTR arg)
{
  Source_File *sf = arg;
  Sym *b;
  unsigned int i;
  static unsigned long last_count;
  unsigned long last_print = (unsigned long) -1;

  b = NULL;

  if (line_num <= sf->num_lines)
    b = sf->line[line_num - 1];

  if (!b)
    {
      for (i = 0; i < width; i++)
	buf[i] = ' ';
      buf[width] = '\0';
    }
  else
    {
      char tmpbuf[NBBS * 30];
      char *p;
      unsigned long ncalls;
      int ncalls_set;
      unsigned int len;

      ++num_executable_lines;

      p = tmpbuf;
      *p = '\0';

      ncalls = 0;
      ncalls_set = 0;

      /* If this is a function entry point, label the line no matter what.
	 Otherwise, we're in the middle of a function, so check to see
	 if the first basic-block address is larger than the starting
	 address of the line.  If so, then this line begins with a
	 a portion of the previous basic-block, so print that prior
	 execution count (if bb_annotate_all_lines is set).  */
      if (b->is_func)
	{
	  sprintf (p, "%lu", b->ncalls);
	  p += strlen (p);
	  last_count = b->ncalls;
	  last_print = last_count;
	  ncalls = b->ncalls;
	  ncalls_set = 1;
	}
      else if (bb_annotate_all_lines
	       && b->bb_addr[0] && b->bb_addr[0] > b->addr)
	{
	  sprintf (p, "%lu", last_count);
	  p += strlen (p);
	  last_print = last_count;
	  ncalls = last_count;
	  ncalls_set = 1;
	}

      /* Loop through all of this line's basic-blocks.  For each one,
	 update last_count, then compress sequential identical counts
	 (if bb_annotate_all_lines) and print the execution count.  */

      for (i = 0; i < NBBS && b->bb_addr[i]; i++)
	{
	  last_count = b->bb_calls[i];
	  if (! ncalls_set)
	    {
	      ncalls = 0;
	      ncalls_set = 1;
	    }
	  ncalls += last_count;

	  if (bb_annotate_all_lines && last_count == last_print)
	    continue;

	  if (p > tmpbuf)
	    *p++ = ',';
	  sprintf (p, "%lu", last_count);
	  p += strlen (p);

	  last_print = last_count;
	}

      /* We're done.  If nothing has been printed on this line,
	 print the last execution count (bb_annotate_all_lines),
	 which could be from either a previous line (if there were
	 no BBs on this line), or from this line (if all our BB
	 counts were compressed out because they were identical).  */

      if (bb_annotate_all_lines && p == tmpbuf)
	{
	  sprintf (p, "%lu", last_count);
	  p += strlen (p);
	  ncalls = last_count;
	  ncalls_set = 1;
	}

      if (! ncalls_set)
	{
	  unsigned int c;

	  for (c = 0; c < width; c++)
	    buf[c] = ' ';
	  buf[width] = '\0';
	  return;
	}

      ++num_lines_executed;

      if (ncalls < bb_min_calls)
	{
	  strcpy (tmpbuf, "#####");
	  p = tmpbuf + 5;
	}

      strcpy (p, " -> ");
      p += 4;

      len = p - tmpbuf;
      if (len >= width)
	{
	  strncpy (buf, tmpbuf, width);
	  buf[width] = '\0';
	}
      else
	{
	  unsigned int c;

	  strcpy (buf + width - len, tmpbuf);
	  for (c = 0; c < width - len; ++c)
	    buf[c] = ' ';
	}
    }
}

/* Annotate the files named in SOURCE_FILES with basic-block statistics
   (execution counts).  After each source files, a few statistics
   regarding that source file are printed.  */

void
print_annotated_source ()
{
  Sym *sym, *line_stats, *new_line;
  Source_File *sf;
  int i, table_len;
  FILE *ofp;

  /* Find maximum line number for each source file that user is
     interested in:  */
  for (sym = symtab.base; sym < symtab.limit; ++sym)
    {
      /* Accept symbol if it's file is known, its line number is
	 bigger than anything we have seen for that file so far and
	 if it's in the INCL_ANNO table or there is no INCL_ANNO
	 table and it does not appear in the EXCL_ANNO table.  */
      if (sym->file && sym->line_num > sym->file->num_lines
	  && (sym_lookup (&syms[INCL_ANNO], sym->addr)
	      || (syms[INCL_ANNO].len == 0
		  && !sym_lookup (&syms[EXCL_ANNO], sym->addr))))
	{
	  sym->file->num_lines = sym->line_num;
	}
    }

  /* Allocate line descriptors:  */
  for (sf = first_src_file; sf; sf = sf->next)
    {
      if (sf->num_lines > 0)
	{
	  sf->line = (void *) xmalloc (sf->num_lines * sizeof (sf->line[0]));
	  memset (sf->line, 0, sf->num_lines * sizeof (sf->line[0]));
	}
    }

  /* Count executions per line:  */
  for (sym = symtab.base; sym < symtab.limit; ++sym)
    {
      if (sym->file && sym->file->num_lines
	  && (sym_lookup (&syms[INCL_ANNO], sym->addr)
	      || (syms[INCL_ANNO].len == 0
		  && !sym_lookup (&syms[EXCL_ANNO], sym->addr))))
	{
	  sym->file->ncalls += sym->ncalls;
	  line_stats = sym->file->line[sym->line_num - 1];

	  if (!line_stats)
	    {
	      /* Common case has at most one basic-block per source line:  */
	      sym->file->line[sym->line_num - 1] = sym;
	    }
	  else if (!line_stats->addr)
	    {
	      /* sym is the 3rd .. nth basic block for this line:  */
	      line_stats->ncalls += sym->ncalls;
	    }
	  else
	    {
	      /* sym is the second basic block for this line.  */
	      new_line = (Sym *) xmalloc (sizeof (*new_line));
	      *new_line = *line_stats;
	      new_line->addr = 0;
	      new_line->ncalls += sym->ncalls;
	      sym->file->line[sym->line_num - 1] = new_line;
	    }
	}
    }

  /* Plod over source files, annotating them:  */
  for (sf = first_src_file; sf; sf = sf->next)
    {
      if (!sf->num_lines || (ignore_zeros && sf->ncalls == 0))
	continue;

      num_executable_lines = num_lines_executed = 0;

      ofp = annotate_source (sf, 16, annotate_with_count, sf);
      if (!ofp)
	continue;

      if (bb_table_length > 0)
	{
	  fprintf (ofp, _("\n\nTop %d Lines:\n\n     Line      Count\n\n"),
		   bb_table_length);

	  /* Abuse line arrays---it's not needed anymore:  */
	  qsort (sf->line, sf->num_lines, sizeof (sf->line[0]), cmp_ncalls);
	  table_len = bb_table_length;

	  if (table_len > sf->num_lines)
	    table_len = sf->num_lines;

	  for (i = 0; i < table_len; ++i)
	    {
	      sym = sf->line[i];

	      if (!sym || sym->ncalls == 0)
		  break;

	      fprintf (ofp, "%9d %10lu\n", sym->line_num, sym->ncalls);
	    }
	}

      free (sf->line);
      sf->line = 0;

      fprintf (ofp, _("\nExecution Summary:\n\n"));
      fprintf (ofp, _("%9ld   Executable lines in this file\n"),
	       num_executable_lines);
      fprintf (ofp, _("%9ld   Lines executed\n"), num_lines_executed);
      fprintf (ofp, _("%9.2f   Percent of the file executed\n"),
	       num_executable_lines
	       ? 100.0 * num_lines_executed / (double) num_executable_lines
	       : 100.0);
      fprintf (ofp, _("\n%9lu   Total number of line executions\n"),
	       sf->ncalls);
      fprintf (ofp, _("%9.2f   Average executions per line\n"),
	       num_executable_lines
	       ? (double) sf->ncalls / (double) num_executable_lines
	       : 0.0);

      if (ofp != stdout)
	fclose (ofp);
    }
}
