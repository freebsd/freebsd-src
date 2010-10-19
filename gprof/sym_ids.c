/* sym_ids.c

   Copyright 1999, 2000, 2001, 2002, 2004 Free Software Foundation, Inc.

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
#include "safe-ctype.h"
#include "gprof.h"
#include "search_list.h"
#include "source.h"
#include "symtab.h"
#include "cg_arcs.h"
#include "sym_ids.h"

static struct sym_id
  {
    struct sym_id *next;
    char *spec;			/* Parsing modifies this.  */
    Table_Id which_table;
    bfd_boolean has_right;

    struct match
      {
	int prev_index;		/* Index of prev match.  */
	Sym *prev_match;	/* Previous match.  */
	Sym *first_match;	/* Chain of all matches.  */
	Sym sym;
      }
    left, right;
  }
 *id_list;

static void parse_spec
  (char *, Sym *);
static void parse_id
  (struct sym_id *);
static bfd_boolean match
  (Sym *, Sym *);
static void extend_match
  (struct match *, Sym *, Sym_Table *, bfd_boolean);


Sym_Table syms[NUM_TABLES];

#ifdef DEBUG
static const char *table_name[] =
{
  "INCL_GRAPH", "EXCL_GRAPH",
  "INCL_ARCS", "EXCL_ARCS",
  "INCL_FLAT", "EXCL_FLAT",
  "INCL_TIME", "EXCL_TIME",
  "INCL_ANNO", "EXCL_ANNO",
  "INCL_EXEC", "EXCL_EXEC"
};
#endif /* DEBUG */

/* This is the table in which we keep all the syms that match
   the right half of an arc id.  It is NOT sorted according
   to the addresses, because it is accessed only through
   the left half's CHILDREN pointers (so it's crucial not
   to reorder this table once pointers into it exist).  */
static Sym_Table right_ids;

static Source_File non_existent_file =
{
  0, "<non-existent-file>", 0, 0, 0, NULL
};


void
sym_id_add (const char *spec, Table_Id which_table)
{
  struct sym_id *id;
  int len = strlen (spec);

  id = (struct sym_id *) xmalloc (sizeof (*id) + len + 1);
  memset (id, 0, sizeof (*id));

  id->spec = (char *) id + sizeof (*id);
  strcpy (id->spec, spec);
  id->which_table = which_table;

  id->next = id_list;
  id_list = id;
}


/* A spec has the syntax FILENAME:(FUNCNAME|LINENUM).  As a convenience
   to the user, a spec without a colon is interpreted as:

	(i)   a FILENAME if it contains a dot
	(ii)  a FUNCNAME if it starts with a non-digit character
	(iii) a LINENUM if it starts with a digit

   A FUNCNAME containing a dot can be specified by :FUNCNAME, a
   FILENAME not containing a dot can be specified by FILENAME.  */

static void
parse_spec (char *spec, Sym *sym)
{
  char *colon;

  sym_init (sym);
  colon = strrchr (spec, ':');

  if (colon)
    {
      *colon = '\0';

      if (colon > spec)
	{
	  sym->file = source_file_lookup_name (spec);

	  if (!sym->file)
	    sym->file = &non_existent_file;
	}

      spec = colon + 1;

      if (strlen (spec))
	{
	  if (ISDIGIT (spec[0]))
	    sym->line_num = atoi (spec);
	  else
	    sym->name = spec;
	}
    }
  else if (strlen (spec))
    {
      /* No colon: spec is a filename if it contains a dot.  */
      if (strchr (spec, '.'))
	{
	  sym->file = source_file_lookup_name (spec);

	  if (!sym->file)
	    sym->file = &non_existent_file;
	}
      else if (ISDIGIT (*spec))
	{
	  sym->line_num = atoi (spec);
	}
      else if (strlen (spec))
	{
	  sym->name = spec;
	}
    }
}


/* A symbol id has the syntax SPEC[/SPEC], where SPEC is is defined
   by parse_spec().  */

static void
parse_id (struct sym_id *id)
{
  char *slash;

  DBG (IDDEBUG, printf ("[parse_id] %s -> ", id->spec));

  slash = strchr (id->spec, '/');
  if (slash)
    {
      parse_spec (slash + 1, &id->right.sym);
      *slash = '\0';
      id->has_right = TRUE;
    }
  parse_spec (id->spec, &id->left.sym);

#ifdef DEBUG
  if (debug_level & IDDEBUG)
    {
      printf ("%s:", id->left.sym.file ? id->left.sym.file->name : "*");

      if (id->left.sym.name)
	printf ("%s", id->left.sym.name);
      else if (id->left.sym.line_num)
	printf ("%d", id->left.sym.line_num);
      else
	printf ("*");

      if (id->has_right)
	{
	  printf ("/%s:",
		  id->right.sym.file ? id->right.sym.file->name : "*");

	  if (id->right.sym.name)
	    printf ("%s", id->right.sym.name);
	  else if (id->right.sym.line_num)
	    printf ("%d", id->right.sym.line_num);
	  else
	    printf ("*");
	}

      printf ("\n");
    }
#endif
}


/* Return TRUE iff PATTERN matches SYM.  */

static bfd_boolean
match (Sym *pattern, Sym *sym)
{
  return (pattern->file ? pattern->file == sym->file : TRUE)
    && (pattern->line_num ? pattern->line_num == sym->line_num : TRUE)
    && (pattern->name
	? strcmp (pattern->name,
		  sym->name+(discard_underscores && sym->name[0] == '_')) == 0
	: TRUE);
}


static void
extend_match (struct match *m, Sym *sym, Sym_Table *tab, bfd_boolean second_pass)
{
  if (m->prev_match != sym - 1)
    {
      /* Discontinuity: add new match to table.  */
      if (second_pass)
	{
	  tab->base[tab->len] = *sym;
	  m->prev_index = tab->len;

	  /* Link match into match's chain.  */
	  tab->base[tab->len].next = m->first_match;
	  m->first_match = &tab->base[tab->len];
	}

      ++tab->len;
    }

  /* Extend match to include this symbol.  */
  if (second_pass)
    tab->base[m->prev_index].end_addr = sym->end_addr;

  m->prev_match = sym;
}


/* Go through sym_id list produced by option processing and fill
   in the various symbol tables indicating what symbols should
   be displayed or suppressed for the various kinds of outputs.

   This can potentially produce huge tables and in particulars
   tons of arcs, but this happens only if the user makes silly
   requests---you get what you ask for!  */

void
sym_id_parse ()
{
  Sym *sym, *left, *right;
  struct sym_id *id;
  Sym_Table *tab;

  /* Convert symbol ids into Syms, so we can deal with them more easily.  */
  for (id = id_list; id; id = id->next)
    parse_id (id);

  /* First determine size of each table.  */
  for (sym = symtab.base; sym < symtab.limit; ++sym)
    {
      for (id = id_list; id; id = id->next)
	{
	  if (match (&id->left.sym, sym))
	    extend_match (&id->left, sym, &syms[id->which_table], FALSE);

	  if (id->has_right && match (&id->right.sym, sym))
	    extend_match (&id->right, sym, &right_ids, FALSE);
	}
    }

  /* Create tables of appropriate size and reset lengths.  */
  for (tab = syms; tab < &syms[NUM_TABLES]; ++tab)
    {
      if (tab->len)
	{
	  tab->base = (Sym *) xmalloc (tab->len * sizeof (Sym));
	  tab->limit = tab->base + tab->len;
	  tab->len = 0;
	}
    }

  if (right_ids.len)
    {
      right_ids.base = (Sym *) xmalloc (right_ids.len * sizeof (Sym));
      right_ids.limit = right_ids.base + right_ids.len;
      right_ids.len = 0;
    }

  /* Make a second pass through symtab, creating syms as necessary.  */
  for (sym = symtab.base; sym < symtab.limit; ++sym)
    {
      for (id = id_list; id; id = id->next)
	{
	  if (match (&id->left.sym, sym))
	    extend_match (&id->left, sym, &syms[id->which_table], TRUE);

	  if (id->has_right && match (&id->right.sym, sym))
	    extend_match (&id->right, sym, &right_ids, TRUE);
	}
    }

  /* Go through ids creating arcs as needed.  */
  for (id = id_list; id; id = id->next)
    {
      if (id->has_right)
	{
	  for (left = id->left.first_match; left; left = left->next)
	    {
	      for (right = id->right.first_match; right; right = right->next)
		{
		  DBG (IDDEBUG,
		       printf (
				"[sym_id_parse]: arc %s:%s(%lx-%lx) -> %s:%s(%lx-%lx) to %s\n",
				left->file ? left->file->name : "*",
				left->name ? left->name : "*",
				(unsigned long) left->addr,
				(unsigned long) left->end_addr,
				right->file ? right->file->name : "*",
				right->name ? right->name : "*",
				(unsigned long) right->addr,
				(unsigned long) right->end_addr,
				table_name[id->which_table]));

		  arc_add (left, right, (unsigned long) 0);
		}
	    }
	}
    }

  /* Finally, we can sort the tables and we're done.  */
  for (tab = &syms[0]; tab < &syms[NUM_TABLES]; ++tab)
    {
      DBG (IDDEBUG, printf ("[sym_id_parse] syms[%s]:\n",
			    table_name[tab - &syms[0]]));
      symtab_finalize (tab);
    }
}


/* Symbol tables storing the FROM symbols of arcs do not necessarily
   have distinct address ranges.  For example, somebody might request
   -k /_mcount to suppress any arcs into _mcount, while at the same
   time requesting -k a/b.  Fortunately, those symbol tables don't get
   very big (the user has to type them!), so a linear search is probably
   tolerable.  */
bfd_boolean
sym_id_arc_is_present (Sym_Table *sym_tab, Sym *from, Sym *to)
{
  Sym *sym;

  for (sym = sym_tab->base; sym < sym_tab->limit; ++sym)
    {
      if (from->addr >= sym->addr && from->addr <= sym->end_addr
	  && arc_lookup (sym, to))
	return TRUE;
    }

  return FALSE;
}
