/* Do various things to symbol tables (other than lookup), for GDB.
   Copyright 1986, 1987, 1989, 1991, 1992, 1993 Free Software Foundation, Inc.

This file is part of GDB.

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
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include "defs.h"
#include "symtab.h"
#include "gdbtypes.h"
#include "bfd.h"
#include "symfile.h"
#include "objfiles.h"
#include "breakpoint.h"
#include "command.h"
#include "obstack.h"
#include "language.h"

#include <string.h>

#ifndef DEV_TTY
#define DEV_TTY "/dev/tty"
#endif

/* Unfortunately for debugging, stderr is usually a macro.  This is painful
   when calling functions that take FILE *'s from the debugger.
   So we make a variable which has the same value and which is accessible when
   debugging GDB with itself.  Because stdin et al need not be constants,
   we initialize them in the _initialize_symmisc function at the bottom
   of the file.  */
FILE *std_in;
FILE *std_out;
FILE *std_err;

/* Prototypes for local functions */

static void 
dump_symtab PARAMS ((struct objfile *, struct symtab *, FILE *));

static void 
dump_psymtab PARAMS ((struct objfile *, struct partial_symtab *, FILE *));

static void 
dump_msymbols PARAMS ((struct objfile *, FILE *));

static void 
dump_objfile PARAMS ((struct objfile *));

static int
block_depth PARAMS ((struct block *));

static void
print_partial_symbol PARAMS ((struct partial_symbol *, int, char *, FILE *));

struct print_symbol_args {
  struct symbol *symbol;
  int depth;
  FILE *outfile;
};

static int print_symbol PARAMS ((char *));

static void
free_symtab_block PARAMS ((struct objfile *, struct block *));


/* Free a struct block <- B and all the symbols defined in that block.  */

static void
free_symtab_block (objfile, b)
     struct objfile *objfile;
     struct block *b;
{
  register int i, n;
  n = BLOCK_NSYMS (b);
  for (i = 0; i < n; i++)
    {
      mfree (objfile -> md, SYMBOL_NAME (BLOCK_SYM (b, i)));
      mfree (objfile -> md, (PTR) BLOCK_SYM (b, i));
    }
  mfree (objfile -> md, (PTR) b);
}

/* Free all the storage associated with the struct symtab <- S.
   Note that some symtabs have contents malloc'ed structure by structure,
   while some have contents that all live inside one big block of memory,
   and some share the contents of another symbol table and so you should
   not free the contents on their behalf (except sometimes the linetable,
   which maybe per symtab even when the rest is not).
   It is s->free_code that says which alternative to use.  */

void
free_symtab (s)
     register struct symtab *s;
{
  register int i, n;
  register struct blockvector *bv;

  switch (s->free_code)
    {
    case free_nothing:
      /* All the contents are part of a big block of memory (an obstack),
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
	free_symtab_block (s -> objfile, BLOCKVECTOR_BLOCK (bv, i));
      /* Free the blockvector itself.  */
      mfree (s -> objfile -> md, (PTR) bv);
      /* Also free the linetable.  */
      
    case free_linetable:
      /* Everything will be freed either by our `free_ptr'
	 or by some other symtab, except for our linetable.
	 Free that now.  */
      if (LINETABLE (s))
	mfree (s -> objfile -> md, (PTR) LINETABLE (s));
      break;
    }

  /* If there is a single block of memory to free, free it.  */
  if (s -> free_ptr != NULL)
    mfree (s -> objfile -> md, s -> free_ptr);

  /* Free source-related stuff */
  if (s -> line_charpos != NULL)
    mfree (s -> objfile -> md, (PTR) s -> line_charpos);
  if (s -> fullname != NULL)
    mfree (s -> objfile -> md, s -> fullname);
  mfree (s -> objfile -> md, (PTR) s);
}

#if MAINTENANCE_CMDS

static void 
dump_objfile (objfile)
     struct objfile *objfile;
{
  struct symtab *symtab;
  struct partial_symtab *psymtab;

  printf_filtered ("\nObject file %s:  ", objfile -> name);
  printf_filtered ("Objfile at %lx, bfd at %lx, %d minsyms\n\n",
		   (unsigned long) objfile,
		   (unsigned long) objfile -> obfd,
		   objfile->minimal_symbol_count);

  if (objfile -> psymtabs)
    {
      printf_filtered ("Psymtabs:\n");
      for (psymtab = objfile -> psymtabs;
	   psymtab != NULL;
	   psymtab = psymtab -> next)
	{
	  printf_filtered ("%s at %lx, ",
			   psymtab -> filename, (unsigned long) psymtab);
	  if (psymtab -> objfile != objfile)
	    {
	      printf_filtered ("NOT ON CHAIN!  ");
	    }
	  wrap_here ("  ");
	}
      printf_filtered ("\n\n");
    }

  if (objfile -> symtabs)
    {
      printf_filtered ("Symtabs:\n");
      for (symtab = objfile -> symtabs;
	   symtab != NULL;
	   symtab = symtab->next)
	{
	  printf_filtered ("%s at %lx, ",
			   symtab -> filename, (unsigned long) symtab);
	  if (symtab -> objfile != objfile)
	    {
	      printf_filtered ("NOT ON CHAIN!  ");
	    }
	  wrap_here ("  ");
	}
      printf_filtered ("\n\n");
    }
}

/* Print minimal symbols from this objfile.  */
 
static void 
dump_msymbols (objfile, outfile)
     struct objfile *objfile;
     FILE *outfile;
{
  struct minimal_symbol *msymbol;
  int index;
  char ms_type;
  
  fprintf_filtered (outfile, "\nObject file %s:\n\n", objfile -> name);
  if (objfile -> minimal_symbol_count == 0)
    {
      fprintf_filtered (outfile, "No minimal symbols found.\n");
      return;
    }
  for (index = 0, msymbol = objfile -> msymbols;
       SYMBOL_NAME (msymbol) != NULL; msymbol++, index++)
    {
      switch (msymbol -> type)
	{
	  case mst_unknown:
	    ms_type = 'u';
	    break;
	  case mst_text:
	    ms_type = 'T';
	    break;
	  case mst_data:
	    ms_type = 'D';
	    break;
	  case mst_bss:
	    ms_type = 'B';
	    break;
	  case mst_abs:
	    ms_type = 'A';
	    break;
	  case mst_file_text:
	    ms_type = 't';
	    break;
	  case mst_file_data:
	    ms_type = 'd';
	    break;
	  case mst_file_bss:
	    ms_type = 'b';
	    break;
	  default:
	    ms_type = '?';
	    break;
	}
      fprintf_filtered (outfile, "[%2d] %c %#10lx %s", index, ms_type,
			SYMBOL_VALUE_ADDRESS (msymbol), SYMBOL_NAME (msymbol));
      if (SYMBOL_DEMANGLED_NAME (msymbol) != NULL)
	{
	  fprintf_filtered (outfile, "  %s", SYMBOL_DEMANGLED_NAME (msymbol));
	}
      fputs_filtered ("\n", outfile);
    }
  if (objfile -> minimal_symbol_count != index)
    {
      warning ("internal error:  minimal symbol count %d != %d",
	       objfile -> minimal_symbol_count, index);
    }
  fprintf_filtered (outfile, "\n");
}

static void
dump_psymtab (objfile, psymtab, outfile)
     struct objfile *objfile;
     struct partial_symtab *psymtab;
     FILE *outfile;
{
  int i;

  fprintf_filtered (outfile, "\nPartial symtab for source file %s ",
		    psymtab -> filename);
  fprintf_filtered (outfile, "(object 0x%lx)\n\n", (unsigned long) psymtab);
  fprintf (outfile, "  Read from object file %s (0x%lx)\n",
	   objfile -> name, (unsigned long) objfile);
  
  if (psymtab -> readin)
    {
      fprintf_filtered (outfile,
		"  Full symtab was read (at 0x%lx by function at 0x%lx)\n",
			(unsigned long) psymtab -> symtab,
			(unsigned long) psymtab -> read_symtab);
    }

  /* FIXME, we need to be able to print the relocation stuff. */
  /* This prints some garbage for anything but stabs right now.  FIXME.  */
  if (psymtab->section_offsets)
    fprintf_filtered (outfile,
		      "  Relocate symbols by 0x%lx, 0x%lx, 0x%lx, 0x%lx.\n",
		      (unsigned long) ANOFFSET (psymtab->section_offsets, 0),
		      (unsigned long) ANOFFSET (psymtab->section_offsets, 1),
		      (unsigned long) ANOFFSET (psymtab->section_offsets, 2),
		      (unsigned long) ANOFFSET (psymtab->section_offsets, 3));

  fprintf_filtered (outfile, "  Symbols cover text addresses 0x%lx-0x%lx\n",
		    (unsigned long) psymtab -> textlow,
		    (unsigned long) psymtab -> texthigh);
  fprintf_filtered (outfile, "  Depends on %d other partial symtabs.\n",
		    psymtab -> number_of_dependencies);
  for (i = 0; i < psymtab -> number_of_dependencies; i++)
    {
      fprintf_filtered (outfile, "    %d 0x%lx %s\n", i,
			(unsigned long) psymtab -> dependencies[i],
			psymtab -> dependencies[i] -> filename);
    }
  if (psymtab -> n_global_syms > 0)
    {
      print_partial_symbol (objfile -> global_psymbols.list
			    + psymtab -> globals_offset,
			    psymtab -> n_global_syms, "Global", outfile);
    }
  if (psymtab -> n_static_syms > 0)
    {
      print_partial_symbol (objfile -> static_psymbols.list
			    + psymtab -> statics_offset,
			    psymtab -> n_static_syms, "Static", outfile);
    }
  fprintf_filtered (outfile, "\n");
}

static void 
dump_symtab (objfile, symtab, outfile)
     struct objfile *objfile;
     struct symtab *symtab;
     FILE *outfile;
{
  register int i, j;
  int len, blen;
  register struct linetable *l;
  struct blockvector *bv;
  register struct block *b;
  int depth;

  fprintf (outfile, "\nSymtab for file %s\n", symtab->filename);
  fprintf (outfile, "Read from object file %s (%lx)\n", objfile->name,
	   (unsigned long) objfile);
  fprintf (outfile, "Language: %s\n", language_str (symtab -> language));
  
  /* First print the line table.  */
  l = LINETABLE (symtab);
  if (l) {
    fprintf (outfile, "\nLine table:\n\n");
    len = l->nitems;
    for (i = 0; i < len; i++)
      fprintf (outfile, " line %ld at %lx\n", l->item[i].line,
	       (unsigned long) l->item[i].pc);
  }
  /* Now print the block info.  */
  fprintf (outfile, "\nBlockvector:\n\n");
  bv = BLOCKVECTOR (symtab);
  len = BLOCKVECTOR_NBLOCKS (bv);
  for (i = 0; i < len; i++)
    {
      b = BLOCKVECTOR_BLOCK (bv, i);
      depth = block_depth (b) * 2;
      print_spaces (depth, outfile);
      fprintf (outfile, "block #%03d (object 0x%lx) ", i, (unsigned long) b);
      fprintf (outfile, "[0x%lx..0x%lx]",
	       (unsigned long) BLOCK_START (b),
	       (unsigned long) BLOCK_END (b));
      if (BLOCK_SUPERBLOCK (b))
	fprintf (outfile, " (under 0x%lx)",
		 (unsigned long) BLOCK_SUPERBLOCK (b));
      if (BLOCK_FUNCTION (b))
	{
	  fprintf (outfile, " %s", SYMBOL_NAME (BLOCK_FUNCTION (b)));
	  if (SYMBOL_DEMANGLED_NAME (BLOCK_FUNCTION (b)) != NULL)
	    {
	      fprintf (outfile, " %s",
		       SYMBOL_DEMANGLED_NAME (BLOCK_FUNCTION (b)));
	    }
	}
      if (BLOCK_GCC_COMPILED(b))
	fprintf (outfile, " gcc%d compiled", BLOCK_GCC_COMPILED(b));
      fputc ('\n', outfile);
      blen = BLOCK_NSYMS (b);
      for (j = 0; j < blen; j++)
	{
	  struct print_symbol_args s;
	  s.symbol = BLOCK_SYM (b, j);
	  s.depth = depth + 1;
	  s.outfile = outfile;
	  catch_errors (print_symbol, &s, "Error printing symbol:\n",
			RETURN_MASK_ERROR);
	}
    }
  fprintf (outfile, "\n");
}

void
maintenance_print_symbols (args, from_tty)
     char *args;
     int from_tty;
{
  char **argv;
  FILE *outfile;
  struct cleanup *cleanups;
  char *symname = NULL;
  char *filename = DEV_TTY;
  struct objfile *objfile;
  struct symtab *s;

  dont_repeat ();

  if (args == NULL)
    {
      error ("print-symbols takes an output file name and optional symbol file name");
    }
  else if ((argv = buildargv (args)) == NULL)
    {
      nomem (0);
    }
  cleanups = make_cleanup (freeargv, (char *) argv);

  if (argv[0] != NULL)
    {
      filename = argv[0];
      /* If a second arg is supplied, it is a source file name to match on */
      if (argv[1] != NULL)
	{
	  symname = argv[1];
	}
    }

  filename = tilde_expand (filename);
  make_cleanup (free, filename);
  
  outfile = fopen (filename, FOPEN_WT);
  if (outfile == 0)
    perror_with_name (filename);
  make_cleanup (fclose, (char *) outfile);

  immediate_quit++;
  ALL_SYMTABS (objfile, s)
    if (symname == NULL || (STREQ (symname, s -> filename)))
      dump_symtab (objfile, s, outfile);
  immediate_quit--;
  do_cleanups (cleanups);
}

/* Print symbol ARGS->SYMBOL on ARGS->OUTFILE.  ARGS->DEPTH says how
   far to indent.  ARGS is really a struct print_symbol_args *, but is
   declared as char * to get it past catch_errors.  Returns 0 for error,
   1 for success.  */

static int
print_symbol (args)
     char *args;
{
  struct symbol *symbol = ((struct print_symbol_args *)args)->symbol;
  int depth = ((struct print_symbol_args *)args)->depth;
  FILE *outfile = ((struct print_symbol_args *)args)->outfile;

  print_spaces (depth, outfile);
  if (SYMBOL_NAMESPACE (symbol) == LABEL_NAMESPACE)
    {
      fprintf (outfile, "label %s at 0x%lx\n", SYMBOL_SOURCE_NAME (symbol),
	       (unsigned long) SYMBOL_VALUE_ADDRESS (symbol));
      return 1;
    }
  if (SYMBOL_NAMESPACE (symbol) == STRUCT_NAMESPACE)
    {
      if (TYPE_TAG_NAME (SYMBOL_TYPE (symbol)))
	{
	  LA_PRINT_TYPE (SYMBOL_TYPE (symbol), "", outfile, 1, depth);
	}
      else
	{
	  fprintf (outfile, "%s %s = ",
	       (TYPE_CODE (SYMBOL_TYPE (symbol)) == TYPE_CODE_ENUM
		? "enum"
		: (TYPE_CODE (SYMBOL_TYPE (symbol)) == TYPE_CODE_STRUCT
		   ? "struct" : "union")),
	       SYMBOL_NAME (symbol));
	  LA_PRINT_TYPE (SYMBOL_TYPE (symbol), "", outfile, 1, depth);
	}
      fprintf (outfile, ";\n");
    }
  else
    {
      if (SYMBOL_CLASS (symbol) == LOC_TYPEDEF)
	fprintf (outfile, "typedef ");
      if (SYMBOL_TYPE (symbol))
	{
	  /* Print details of types, except for enums where it's clutter.  */
	  LA_PRINT_TYPE (SYMBOL_TYPE (symbol), SYMBOL_SOURCE_NAME (symbol),
			 outfile,
			 TYPE_CODE (SYMBOL_TYPE (symbol)) != TYPE_CODE_ENUM,
			 depth);
	  fprintf (outfile, "; ");
	}
      else
	fprintf (outfile, "%s ", SYMBOL_SOURCE_NAME (symbol));

      switch (SYMBOL_CLASS (symbol))
	{
	case LOC_CONST:
	  fprintf (outfile, "const %ld (0x%lx),",
		   SYMBOL_VALUE (symbol),
		   (unsigned long) SYMBOL_VALUE (symbol));
	  break;

	case LOC_CONST_BYTES:
	  fprintf (outfile, "const %u hex bytes:",
		   TYPE_LENGTH (SYMBOL_TYPE (symbol)));
	  {
	    unsigned i;
	    for (i = 0; i < TYPE_LENGTH (SYMBOL_TYPE (symbol)); i++)
	      fprintf (outfile, " %02x",
			 (unsigned)SYMBOL_VALUE_BYTES (symbol) [i]);
	    fprintf (outfile, ",");
	  }
	  break;

	case LOC_STATIC:
	  fprintf (outfile, "static at 0x%lx,",
		   (unsigned long) SYMBOL_VALUE_ADDRESS (symbol));
	  break;

	case LOC_REGISTER:
	  fprintf (outfile, "register %ld,", SYMBOL_VALUE (symbol));
	  break;

	case LOC_ARG:
	  fprintf (outfile, "arg at 0x%lx,", SYMBOL_VALUE (symbol));
	  break;

	case LOC_LOCAL_ARG:
	  fprintf (outfile, "arg at offset 0x%lx from fp,",
		   SYMBOL_VALUE (symbol));
	  break;

	case LOC_REF_ARG:
	  fprintf (outfile, "reference arg at 0x%lx,", SYMBOL_VALUE (symbol));
	  break;

	case LOC_REGPARM:
	  fprintf (outfile, "parameter register %ld,", SYMBOL_VALUE (symbol));
	  break;

	case LOC_REGPARM_ADDR:
	  fprintf (outfile, "address parameter register %ld,", SYMBOL_VALUE (symbol));
	  break;

	case LOC_LOCAL:
	  fprintf (outfile, "local at 0x%lx,", SYMBOL_VALUE (symbol));
	  break;

	case LOC_BASEREG:
	  fprintf (outfile, "local at 0x%lx from register %d",
		   SYMBOL_VALUE (symbol), SYMBOL_BASEREG (symbol));
	  break;

	case LOC_BASEREG_ARG:
	  fprintf (outfile, "arg at 0x%lx from register %d,",
		   SYMBOL_VALUE (symbol), SYMBOL_BASEREG (symbol));
	  break;

	case LOC_TYPEDEF:
	  break;

	case LOC_LABEL:
	  fprintf (outfile, "label at 0x%lx",
		   (unsigned long) SYMBOL_VALUE_ADDRESS (symbol));
	  break;

	case LOC_BLOCK:
	  fprintf (outfile, "block (object 0x%lx) starting at 0x%lx,",
		   (unsigned long) SYMBOL_BLOCK_VALUE (symbol),
		   (unsigned long) BLOCK_START (SYMBOL_BLOCK_VALUE (symbol)));
	  break;

	case LOC_OPTIMIZED_OUT:
	  fprintf (outfile, "optimized out");
	  break;

        default:
	  fprintf (outfile, "botched symbol class %x", SYMBOL_CLASS (symbol));
	  break;
	}
    }
  fprintf (outfile, "\n");
  return 1;
}

void
maintenance_print_psymbols (args, from_tty)
     char *args;
     int from_tty;
{
  char **argv;
  FILE *outfile;
  struct cleanup *cleanups;
  char *symname = NULL;
  char *filename = DEV_TTY;
  struct objfile *objfile;
  struct partial_symtab *ps;

  dont_repeat ();

  if (args == NULL)
    {
      error ("print-psymbols takes an output file name and optional symbol file name");
    }
  else if ((argv = buildargv (args)) == NULL)
    {
      nomem (0);
    }
  cleanups = make_cleanup (freeargv, (char *) argv);

  if (argv[0] != NULL)
    {
      filename = argv[0];
      /* If a second arg is supplied, it is a source file name to match on */
      if (argv[1] != NULL)
	{
	  symname = argv[1];
	}
    }

  filename = tilde_expand (filename);
  make_cleanup (free, filename);
  
  outfile = fopen (filename, FOPEN_WT);
  if (outfile == 0)
    perror_with_name (filename);
  make_cleanup (fclose, outfile);

  immediate_quit++;
  ALL_PSYMTABS (objfile, ps)
    if (symname == NULL || (STREQ (symname, ps -> filename)))
      dump_psymtab (objfile, ps, outfile);
  immediate_quit--;
  do_cleanups (cleanups);
}

static void
print_partial_symbol (p, count, what, outfile)
     struct partial_symbol *p;
     int count;
     char *what;
     FILE *outfile;
{

  fprintf_filtered (outfile, "  %s partial symbols:\n", what);
  while (count-- > 0)
    {
      fprintf_filtered (outfile, "    `%s'", SYMBOL_NAME(p));
      if (SYMBOL_DEMANGLED_NAME (p) != NULL)
	{
	  fprintf_filtered (outfile, "  `%s'", SYMBOL_DEMANGLED_NAME (p));
	}
      fputs_filtered (", ", outfile);
      switch (SYMBOL_NAMESPACE (p))
	{
	case UNDEF_NAMESPACE:
	  fputs_filtered ("undefined namespace, ", outfile);
	  break;
	case VAR_NAMESPACE:
	  /* This is the usual thing -- don't print it */
	  break;
	case STRUCT_NAMESPACE:
	  fputs_filtered ("struct namespace, ", outfile);
	  break;
	case LABEL_NAMESPACE:
	  fputs_filtered ("label namespace, ", outfile);
	  break;
	default:
	  fputs_filtered ("<invalid namespace>, ", outfile);
	  break;
	}
      switch (SYMBOL_CLASS (p))
	{
	case LOC_UNDEF:
	  fputs_filtered ("undefined", outfile);
	  break;
	case LOC_CONST:
	  fputs_filtered ("constant int", outfile);
	  break;
	case LOC_STATIC:
	  fputs_filtered ("static", outfile);
	  break;
	case LOC_REGISTER:
	  fputs_filtered ("register", outfile);
	  break;
	case LOC_ARG:
	  fputs_filtered ("pass by value", outfile);
	  break;
	case LOC_REF_ARG:
	  fputs_filtered ("pass by reference", outfile);
	  break;
	case LOC_REGPARM:
	  fputs_filtered ("register parameter", outfile);
	  break;
	case LOC_REGPARM_ADDR:
	  fputs_filtered ("register address parameter", outfile);
	  break;
	case LOC_LOCAL:
	  fputs_filtered ("stack parameter", outfile);
	  break;
	case LOC_TYPEDEF:
	  fputs_filtered ("type", outfile);
	  break;
	case LOC_LABEL:
	  fputs_filtered ("label", outfile);
	  break;
	case LOC_BLOCK:
	  fputs_filtered ("function", outfile);
	  break;
	case LOC_CONST_BYTES:
	  fputs_filtered ("constant bytes", outfile);
	  break;
	case LOC_LOCAL_ARG:
	  fputs_filtered ("shuffled arg", outfile);
	  break;
	case LOC_OPTIMIZED_OUT:
	  fputs_filtered ("optimized out", outfile);
	  break;
	default:
	  fputs_filtered ("<invalid location>", outfile);
	  break;
	}
      fputs_filtered (", ", outfile);
      fprintf_filtered (outfile, "0x%lx\n", SYMBOL_VALUE (p));
      p++;
    }
}

void
maintenance_print_msymbols (args, from_tty)
     char *args;
     int from_tty;
{
  char **argv;
  FILE *outfile;
  struct cleanup *cleanups;
  char *filename = DEV_TTY;
  char *symname = NULL;
  struct objfile *objfile;

  dont_repeat ();

  if (args == NULL)
    {
      error ("print-msymbols takes an output file name and optional symbol file name");
    }
  else if ((argv = buildargv (args)) == NULL)
    {
      nomem (0);
    }
  cleanups = make_cleanup (freeargv, argv);

  if (argv[0] != NULL)
    {
      filename = argv[0];
      /* If a second arg is supplied, it is a source file name to match on */
      if (argv[1] != NULL)
	{
	  symname = argv[1];
	}
    }

  filename = tilde_expand (filename);
  make_cleanup (free, filename);
  
  outfile = fopen (filename, FOPEN_WT);
  if (outfile == 0)
    perror_with_name (filename);
  make_cleanup (fclose, outfile);

  immediate_quit++;
  ALL_OBJFILES (objfile)
    if (symname == NULL || (STREQ (symname, objfile -> name)))
      dump_msymbols (objfile, outfile);
  immediate_quit--;
  fprintf_filtered (outfile, "\n\n");
  do_cleanups (cleanups);
}

void
maintenance_print_objfiles (ignore, from_tty)
     char *ignore;
     int from_tty;
{
  struct objfile *objfile;

  dont_repeat ();

  immediate_quit++;
  ALL_OBJFILES (objfile)
    dump_objfile (objfile);
  immediate_quit--;
}


/* Return the nexting depth of a block within other blocks in its symtab.  */

static int
block_depth (block)
     struct block *block;
{
  register int i = 0;
  while ((block = BLOCK_SUPERBLOCK (block)) != NULL) 
    {
      i++;
    }
  return i;
}

#endif	/* MAINTENANCE_CMDS */


/* Increase the space allocated for LISTP, which is probably
   global_psymbol_list or static_psymbol_list. This space will eventually
   be freed in free_objfile().  */

void
extend_psymbol_list (listp, objfile)
     register struct psymbol_allocation_list *listp;
     struct objfile *objfile;
{
  int new_size;
  if (listp->size == 0)
    {
      new_size = 255;
      listp->list = (struct partial_symbol *)
	xmmalloc (objfile -> md, new_size * sizeof (struct partial_symbol));
    }
  else
    {
      new_size = listp->size * 2;
      listp->list = (struct partial_symbol *)
	xmrealloc (objfile -> md, (char *) listp->list,
		   new_size * sizeof (struct partial_symbol));
    }
  /* Next assumes we only went one over.  Should be good if
     program works correctly */
  listp->next = listp->list + listp->size;
  listp->size = new_size;
}


/* Do early runtime initializations. */
void
_initialize_symmisc ()
{
  std_in  = stdin;
  std_out = stdout;
  std_err = stderr;
}
