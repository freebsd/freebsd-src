/* Do various things to symbol tables (other than lookup)), for GDB.
   Copyright (C) 1986, 1987, 1989 Free Software Foundation, Inc.

This file is part of GDB.

GDB is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

GDB is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GDB; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */


#include "defs.h"
#include "symtab.h"

#include <stdio.h>
#include <obstack.h>

static void free_symtab ();


/* Free all the symtabs that are currently installed,
   and all storage associated with them.
   Leaves us in a consistent state with no symtabs installed.  */

void
free_all_symtabs ()
{
  register struct symtab *s, *snext;

  /* All values will be invalid because their types will be!  */

  clear_value_history ();
  clear_displays ();
  clear_internalvars ();
  clear_breakpoints ();
  set_default_breakpoint (0, 0, 0, 0);

  current_source_symtab = 0;

  for (s = symtab_list; s; s = snext)
    {
      snext = s->next;
      free_symtab (s);
    }
  symtab_list = 0;
  obstack_free (symbol_obstack, 0);
  obstack_init (symbol_obstack);

  if (misc_function_vector)
    free (misc_function_vector);
  misc_function_count = 0;
  misc_function_vector = 0;
}

/* Free a struct block <- B and all the symbols defined in that block.  */

static void
free_symtab_block (b)
     struct block *b;
{
  register int i, n;
  n = BLOCK_NSYMS (b);
  for (i = 0; i < n; i++)
    {
      free (SYMBOL_NAME (BLOCK_SYM (b, i)));
      free (BLOCK_SYM (b, i));
    }
  free (b);
}

/* Free all the storage associated with the struct symtab <- S.
   Note that some symtabs have contents malloc'ed structure by structure,
   while some have contents that all live inside one big block of memory,
   and some share the contents of another symbol table and so you should
   not free the contents on their behalf (except sometimes the linetable,
   which maybe per symtab even when the rest is not).
   It is s->free_code that says which alternative to use.  */

static void
free_symtab (s)
     register struct symtab *s;
{
  register int i, n;
  register struct blockvector *bv;
  register struct type *type;
  register struct typevector *tv;

  switch (s->free_code)
    {
    case free_nothing:
      /* All the contents are part of a big block of memory
	 and some other symtab is in charge of freeing that block.
	 Therefore, do nothing.  */
      break;

    case free_contents:
      /* Here all the contents were malloc'ed structure by structure
	 and must be freed that way.  */
      /* First free the blocks (and their symbols.  */
      bv = BLOCKVECTOR (s);
      n = BLOCKVECTOR_NBLOCKS (bv);
      for (i = 0; i < n; i++)
	free_symtab_block (BLOCKVECTOR_BLOCK (bv, i));
      /* Free the blockvector itself.  */
      free (bv);
      /* Free the type vector.  */
      tv = TYPEVECTOR (s);
      free (tv);
      /* Also free the linetable.  */
      
    case free_linetable:
      /* Everything will be freed either by our `free_ptr'
	 or by some other symbatb, except for our linetable.
	 Free that now.  */
      free (LINETABLE (s));
      break;
    }

  /* If there is a single block of memory to free, free it.  */
  if (s->free_ptr)
    free (s->free_ptr);

  if (s->line_charpos)
    free (s->line_charpos);
  free (s->filename);
  free (s);
}

/* Convert a raw symbol-segment to a struct symtab,
   and relocate its internal pointers so that it is valid.  */

/* This is how to relocate one pointer, given a name for it.
   Works independent of the type of object pointed to.  */
#define RELOCATE(slot) (slot ? (* (char **) &slot += relocation) : 0)

/* This is the inverse of RELOCATE.  We use it when storing
   a core address into a slot that has yet to be relocated.  */
#define UNRELOCATE(slot) (slot ? (* (char **) &slot -= relocation) : 0)

/* During the process of relocation, this holds the amount to relocate by
   (the address of the file's symtab data, in core in the debugger).  */
static int relocation;

#define CORE_RELOCATE(slot) \
  ((slot) += (((slot) < data_start) ? text_relocation		\
	      : ((slot) < bss_start) ? data_relocation : bss_relocation))

#define TEXT_RELOCATE(slot)  ((slot) += text_relocation)

/* Relocation amounts for addresses in the program's core image.  */
static int text_relocation, data_relocation, bss_relocation;

/* Boundaries that divide program core addresses into text, data and bss;
   used to determine which relocation amount to use.  */
static int data_start, bss_start;

static void relocate_typevector ();
static void relocate_blockvector ();
static void relocate_type ();
static void relocate_block ();
static void relocate_symbol ();
static void relocate_source ();

/* Relocate a file's symseg so that all the pointers are valid C pointers.
   Value is a `struct symtab'; but it is not suitable for direct
   insertion into the `symtab_list' because it describes several files.  */

static struct symtab *
relocate_symtab (root)
     struct symbol_root *root;
{
  struct symtab *sp = (struct symtab *) xmalloc (sizeof (struct symtab));
  bzero (sp, sizeof (struct symtab));

  relocation = (int) root;
  text_relocation = root->textrel;
  data_relocation = root->datarel;
  bss_relocation = root->bssrel;
  data_start = root->databeg;
  bss_start = root->bssbeg;

  sp->filename = root->filename;
  sp->ldsymoff = root->ldsymoff;
  sp->language = root->language;
  sp->compilation = root->compilation;
  sp->version = root->version;
  sp->blockvector = root->blockvector;
  sp->typevector = root->typevector;

  RELOCATE (TYPEVECTOR (sp));
  RELOCATE (BLOCKVECTOR (sp));
  RELOCATE (sp->version);
  RELOCATE (sp->compilation);
  RELOCATE (sp->filename);

  relocate_typevector (TYPEVECTOR (sp));
  relocate_blockvector (BLOCKVECTOR (sp));

  return sp;
}

static void
relocate_blockvector (blp)
     register struct blockvector *blp;
{
  register int nblocks = BLOCKVECTOR_NBLOCKS (blp);
  register int i;
  for (i = 0; i < nblocks; i++)
    RELOCATE (BLOCKVECTOR_BLOCK (blp, i));
  for (i = 0; i < nblocks; i++)
    relocate_block (BLOCKVECTOR_BLOCK (blp, i));
}

static void
relocate_block (bp)
     register struct block *bp;
{
  register int nsyms = BLOCK_NSYMS (bp);
  register int i;

  TEXT_RELOCATE (BLOCK_START (bp));
  TEXT_RELOCATE (BLOCK_END (bp));

  /* These two should not be recursively processed.
     The superblock need not be because all blocks are
     processed from relocate_blockvector.
     The function need not be because it will be processed
     under the block which is its scope.  */
  RELOCATE (BLOCK_SUPERBLOCK (bp));
  RELOCATE (BLOCK_FUNCTION (bp));

  for (i = 0; i < nsyms; i++)
    RELOCATE (BLOCK_SYM (bp, i));

  for (i = 0; i < nsyms; i++)
    relocate_symbol (BLOCK_SYM (bp, i));
}

static void
relocate_symbol (sp)
     register struct symbol *sp;
{
  RELOCATE (SYMBOL_NAME (sp));
  if (SYMBOL_CLASS (sp) == LOC_BLOCK)
    {
      RELOCATE (SYMBOL_BLOCK_VALUE (sp));
      /* We can assume the block that belongs to this symbol
	 is not relocated yet, since it comes after
	 the block that contains this symbol.  */
      BLOCK_FUNCTION (SYMBOL_BLOCK_VALUE (sp)) = sp;
      UNRELOCATE (BLOCK_FUNCTION (SYMBOL_BLOCK_VALUE (sp)));
    }
  else if (SYMBOL_CLASS (sp) == LOC_STATIC)
    CORE_RELOCATE (SYMBOL_VALUE (sp));
  else if (SYMBOL_CLASS (sp) == LOC_LABEL)
    TEXT_RELOCATE (SYMBOL_VALUE (sp));
  RELOCATE (SYMBOL_TYPE (sp));
}

static void
relocate_typevector (tv)
     struct typevector *tv;
{
  register int ntypes = TYPEVECTOR_NTYPES (tv);
  register int i;

  for (i = 0; i < ntypes; i++)
    RELOCATE (TYPEVECTOR_TYPE (tv, i));
  for (i = 0; i < ntypes; i++)
    relocate_type (TYPEVECTOR_TYPE (tv, i));
}

/* We cannot come up with an a priori spanning tree
   for the network of types, since types can be used
   for many symbols and also as components of other types.
   Therefore, we need to be able to mark types that we
   already have relocated (or are already in the middle of relocating)
   as in a garbage collector.  */

static void
relocate_type (tp)
     register struct type *tp;
{
  register int nfields = TYPE_NFIELDS (tp);
  register int i;

  RELOCATE (TYPE_NAME (tp));
  RELOCATE (TYPE_TARGET_TYPE (tp));
  RELOCATE (TYPE_FIELDS (tp));
  RELOCATE (TYPE_POINTER_TYPE (tp));

  for (i = 0; i < nfields; i++)
    {
      RELOCATE (TYPE_FIELD_TYPE (tp, i));
      RELOCATE (TYPE_FIELD_NAME (tp, i));
    }
}

static void
relocate_sourcevector (svp)
     register struct sourcevector *svp;
{
  register int nfiles = svp->length;
  register int i;
  for (i = 0; i < nfiles; i++)
    RELOCATE (svp->source[i]);
  for (i = 0; i < nfiles; i++)
    relocate_source (svp->source[i]);
}

static void
relocate_source (sp)
     register struct source *sp;
{
  register int nitems = sp->contents.nitems;
  register int i;

  RELOCATE (sp->name);
  for (i = 0; i < nitems; i++)
    TEXT_RELOCATE (sp->contents.item[i].pc);
}

/* Read symsegs from file named NAME open on DESC,
   make symtabs from them, and return a chain of them.
   These symtabs are not suitable for direct use in `symtab_list'
   because each one describes a single object file, perhaps many source files.
   `symbol_file_command' takes each of these, makes many real symtabs
   from it, and then frees it.

   We assume DESC is prepositioned at the end of the string table,
   just before the symsegs if there are any.  */

struct symtab *
read_symsegs (desc, name)
     int desc;
     char *name;
{
  struct symbol_root root;
  register char *data;
  register struct symtab *sp, *sp1, *chain = 0;
  register int len;

  while (1)
    {
      len = myread (desc, &root, sizeof root);
      if (len == 0 || root.format == 0)
	break;
      /* format 1 was ok for the original gdb, but since the size of the
	 type structure changed when C++ support was added, it can no
	 longer be used.  Accept only format 2. */
      if (root.format != 2 ||
	  root.length < sizeof root)
	error ("\nInvalid symbol segment format code");
      data = (char *) xmalloc (root.length);
      bcopy (&root, data, sizeof root);
      len = myread (desc, data + sizeof root,
		    root.length - sizeof root);
      sp = relocate_symtab (data);
      RELOCATE (((struct symbol_root *)data)->sourcevector);
      relocate_sourcevector (((struct symbol_root *)data)->sourcevector);
      sp->next = chain;
      chain = sp;
      sp->linetable = (struct linetable *) ((struct symbol_root *)data)->sourcevector;
    }

  return chain;
}

static int block_depth ();
void print_spaces ();
static void print_symbol ();

void
print_symtabs (filename)
     char *filename;
{
  FILE *outfile;
  register struct symtab *s;
  register int i, j;
  int len, line, blen;
  register struct linetable *l;
  struct blockvector *bv;
  register struct block *b;
  int depth;
  struct cleanup *cleanups;
  extern int fclose();

  if (filename == 0)
    error_no_arg ("file to write symbol data in");

  filename = tilde_expand (filename);
  make_cleanup (free, filename);
  
  outfile = fopen (filename, "w");
  if (outfile == 0)
    perror_with_name (filename);

  cleanups = make_cleanup (fclose, outfile);
  immediate_quit++;

  for (s = symtab_list; s; s = s->next)
    {
      /* First print the line table.  */
      fprintf (outfile, "Symtab for file %s\n\n", s->filename);
      fprintf (outfile, "Line table:\n\n");
      l = LINETABLE (s);
      len = l->nitems;
      for (i = 0; i < len; i++)
	fprintf (outfile, " line %d at %x\n", l->item[i].line,
		 l->item[i].pc);
      /* Now print the block info.  */
      fprintf (outfile, "\nBlockvector:\n\n");
      bv = BLOCKVECTOR (s);
      len = BLOCKVECTOR_NBLOCKS (bv);
      for (i = 0; i < len; i++)
	{
	  b = BLOCKVECTOR_BLOCK (bv, i);
	  depth = block_depth (b) * 2;
	  print_spaces (depth, outfile);
	  fprintf (outfile, "block #%03d (object 0x%x) ", i, b);
	  fprintf (outfile, "[0x%x..0x%x]", BLOCK_START (b), BLOCK_END (b));
	  if (BLOCK_SUPERBLOCK (b))
	    fprintf (outfile, " (under 0x%x)", BLOCK_SUPERBLOCK (b));
	  if (BLOCK_FUNCTION (b))
	    fprintf (outfile, " %s", SYMBOL_NAME (BLOCK_FUNCTION (b)));
	  fputc ('\n', outfile);
	  blen = BLOCK_NSYMS (b);
	  for (j = 0; j < blen; j++)
	    {
	      print_symbol (BLOCK_SYM (b, j), depth + 1, outfile);
	    }
	}

      fprintf (outfile, "\n\n");
    }

  immediate_quit--;
  do_cleanups (cleanups);
}

static void
print_symbol (symbol, depth, outfile)
     struct symbol *symbol;
     int depth;
     FILE *outfile;
{
  print_spaces (depth, outfile);
  if (SYMBOL_NAMESPACE (symbol) == LABEL_NAMESPACE)
    {
      fprintf (outfile, "label %s at 0x%x\n", SYMBOL_NAME (symbol),
	       SYMBOL_VALUE (symbol));
      return;
    }
  if (SYMBOL_NAMESPACE (symbol) == STRUCT_NAMESPACE)
    {
      if (TYPE_NAME (SYMBOL_TYPE (symbol)))
	{
	  type_print_1 (SYMBOL_TYPE (symbol), "", outfile, 1, depth);
	}
      else
	{
	  fprintf (outfile, "%s %s = ",
	       (TYPE_CODE (SYMBOL_TYPE (symbol)) == TYPE_CODE_ENUM
		? "enum"
		: (TYPE_CODE (SYMBOL_TYPE (symbol)) == TYPE_CODE_STRUCT
		   ? "struct" : "union")),
	       SYMBOL_NAME (symbol));
	  type_print_1 (SYMBOL_TYPE (symbol), "", outfile, 1, depth);
	}
      fprintf (outfile, ";\n");
    }
  else
    {
      if (SYMBOL_CLASS (symbol) == LOC_TYPEDEF)
	fprintf (outfile, "typedef ");
      if (SYMBOL_TYPE (symbol))
	{
	  type_print_1 (SYMBOL_TYPE (symbol), SYMBOL_NAME (symbol),
			outfile, 1, depth);
	  fprintf (outfile, "; ");
	}
      else
	fprintf (outfile, "%s ", SYMBOL_NAME (symbol));

      switch (SYMBOL_CLASS (symbol))
	{
	case LOC_CONST:
	  fprintf (outfile, "const %d (0x%x),",
		   SYMBOL_VALUE (symbol), SYMBOL_VALUE (symbol));
	  break;

	case LOC_CONST_BYTES:
	  fprintf (outfile, "const %d hex bytes:",
		   TYPE_LENGTH (SYMBOL_TYPE (symbol)));
	  {
	    int i;
	    for (i = 0; i < TYPE_LENGTH (SYMBOL_TYPE (symbol)); i++)
	      fprintf (outfile, " %2x", SYMBOL_VALUE_BYTES (symbol) [i]);
	    fprintf (outfile, ",");
	  }
	  break;

	case LOC_STATIC:
	  fprintf (outfile, "static at 0x%x,", SYMBOL_VALUE (symbol));
	  break;

	case LOC_REGISTER:
	  fprintf (outfile, "register %d,", SYMBOL_VALUE (symbol));
	  break;

	case LOC_ARG:
	  fprintf (outfile, "arg at 0x%x,", SYMBOL_VALUE (symbol));
	  break;

	case LOC_REF_ARG:
	  fprintf (outfile, "reference arg at 0x%x,", SYMBOL_VALUE (symbol));
	  break;

	case LOC_REGPARM:
	  fprintf (outfile, "parameter register %d,", SYMBOL_VALUE (symbol));
	  break;

	case LOC_LOCAL:
	  fprintf (outfile, "local at 0x%x,", SYMBOL_VALUE (symbol));
	  break;

	case LOC_TYPEDEF:
	  break;

	case LOC_LABEL:
	  fprintf (outfile, "label at 0x%x", SYMBOL_VALUE (symbol));
	  break;

	case LOC_BLOCK:
	  fprintf (outfile, "block (object 0x%x) starting at 0x%x,",
		   SYMBOL_VALUE (symbol),
		   BLOCK_START (SYMBOL_BLOCK_VALUE (symbol)));
	  break;
	}
    }
  fprintf (outfile, "\n");
}

/* Return the nexting depth of a block within other blocks in its symtab.  */

static int
block_depth (block)
     struct block *block;
{
  register int i = 0;
  while (block = BLOCK_SUPERBLOCK (block)) i++;
  return i;
}

/*
 * Free all partial_symtab storage.
 */
void
free_all_psymtabs()
{
  obstack_free (psymbol_obstack, 0);
  obstack_init (psymbol_obstack);
  partial_symtab_list = (struct partial_symtab *) 0;
}

void
_initialize_symmisc ()
{
  symtab_list = (struct symtab *) 0;
  partial_symtab_list = (struct partial_symtab *) 0;
  
  add_com ("printsyms", class_obscure, print_symtabs,
	   "Print dump of current symbol definitions to file OUTFILE.");
}

